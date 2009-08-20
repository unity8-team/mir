/*
 * Copyright 2003 NVIDIA, Corporation
 * Copyright 2007 Maarten Maathuis
 * Copyright 2009 Stuart Bennett
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nv_include.h"

#define TO_ARGB1555(c) (0x8000			|	/* Mask bit */	\
			((c & 0xf80000) >> 9 )	|	/* Red      */	\
			((c & 0xf800) >> 6 )	|	/* Green    */	\
			((c & 0xf8) >> 3 ))		/* Blue     */
#define TO_ARGB8888(c) (0xff000000 | c)

#define SOURCE_MASK_INTERLEAVE 32
#define TRANSPARENT_PIXEL   0

/*
 * Convert a source/mask bitmap cursor to an ARGB cursor, clipping or
 * padding as necessary. source/mask are assumed to be alternated each
 * SOURCE_MASK_INTERLEAVE bits.
 */
void
nv_cursor_convert_cursor(uint32_t *src, void *dst, int src_stride, int dst_stride,
			 int bpp, uint32_t fg, uint32_t bg)
{
	int width = min(src_stride, dst_stride);
	uint32_t b, m, pxval;
	int i, j, k;

	for (i = 0; i < width; i++) {
		for (j = 0; j < width / SOURCE_MASK_INTERLEAVE; j++) {
			int src_off = i*src_stride/SOURCE_MASK_INTERLEAVE + j;
			int dst_off = i*dst_stride + j*SOURCE_MASK_INTERLEAVE;

			b = src[2*src_off];
			m = src[2*src_off + 1];

			for (k = 0; k < SOURCE_MASK_INTERLEAVE; k++) {
				pxval = TRANSPARENT_PIXEL;
#if X_BYTE_ORDER == X_BIG_ENDIAN
				if (m & 0x80000000)
					pxval = (b & 0x80000000) ? fg : bg;
				b <<= 1;
				m <<= 1;
#else
				if (m & 1)
					pxval = (b & 1) ? fg : bg;
				b >>= 1;
				m >>= 1;
#endif
				if (bpp == 32)
					((uint32_t *)dst)[dst_off + k] = pxval;
				else
					((uint16_t *)dst)[dst_off + k] = pxval;
			}
		}
	}
}

static void nv_cursor_transform_cursor(NVPtr pNv, struct nouveau_crtc *nv_crtc)
{
	uint16_t *tmp;
	struct nouveau_bo *cursor = NULL;
	int px = nv_cursor_pixels(pNv);
	int width = nv_cursor_width(pNv);

	if (!(tmp = xcalloc(px, 4)))
		return;

	/* convert to colour cursor */
	nv_cursor_convert_cursor(pNv->curImage, tmp, width, width,
				 pNv->alphaCursor ? 32 : 16, nv_crtc->cursor_fg,
				 nv_crtc->cursor_bg);

	nouveau_bo_ref(nv_crtc->head ? pNv->Cursor2 : pNv->Cursor, &cursor);
	nouveau_bo_map(cursor, NOUVEAU_BO_WR);

	memcpy(cursor->map, tmp, px * 4);

	nouveau_bo_unmap(cursor);
	nouveau_bo_ref(NULL, &cursor);

	xfree(tmp);
}

void nv_crtc_set_cursor_colors(xf86CrtcPtr crtc, int bg, int fg)
{
	NVPtr pNv = NVPTR(crtc->scrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	uint32_t fore, back;

	if (pNv->alphaCursor) {
		fore = TO_ARGB8888(fg);
		back = TO_ARGB8888(bg);
#if X_BYTE_ORDER == X_BIG_ENDIAN
		if ((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
			fore = lswapl(fore);
			back = lswapl(back);
		}
#endif
	} else {
		fore = TO_ARGB1555(fg);
		back = TO_ARGB1555(bg);
	}

	if (nv_crtc->cursor_fg != fore || nv_crtc->cursor_bg != back) {
		nv_crtc->cursor_fg = fore;
		nv_crtc->cursor_bg = back;
		nv_cursor_transform_cursor(pNv, nv_crtc);
	}
}

void nv_crtc_load_cursor_image(xf86CrtcPtr crtc, CARD8 *image)
{
	NVPtr pNv = NVPTR(crtc->scrn);

	/* save copy of image for colour changes */
	memcpy(pNv->curImage, image, nv_cursor_pixels(pNv) / 4);

	nv_cursor_transform_cursor(pNv, to_nouveau_crtc(crtc));
}

void nv_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *image)
{
	NVPtr pNv = NVPTR(crtc->scrn);
	int head = to_nouveau_crtc(crtc)->head, i, alpha;
	struct nouveau_bo *cursor = NULL;
	uint32_t *dst, *src = (uint32_t *)image, tmp;

	nouveau_bo_ref(head ? pNv->Cursor2 : pNv->Cursor, &cursor);
	nouveau_bo_map(cursor, NOUVEAU_BO_WR);
	dst = cursor->map;

	/* nv11+ supports premultiplied (PM), or non-premultiplied (NPM) alpha
	 * cursors (though NPM in combination with fp dithering may not work on
	 * nv11, from "nv" driver history)
	 * NPM mode needs NV_PCRTC_CURSOR_CONFIG_ALPHA_BLEND set and is what the
	 * blob uses, however we get given PM cursors so we use PM mode
	 */
	for (i = 0; i < nv_cursor_pixels(pNv); i++) {
		/* hw gets unhappy if alpha <= rgb values.  for a PM image "less
		 * than" shouldn't happen; fix "equal to" case by adding one to
		 * alpha channel (slightly inaccurate, but so is attempting to
		 * get back to NPM images, due to limits of integer precision)
		 */
		alpha = (*src >> 24);
		if (!alpha || alpha == 0xff)
			/* alpha == max(r,g,b) works ok for 0x0 and 0xff */
			tmp = *src;
		else
			tmp = ((alpha + 1) << 24) | (*src & 0xffffff);
#if X_BYTE_ORDER == X_BIG_ENDIAN
		if (pNv->NVArch == 0x11)
			tmp = lswapl(tmp);
#endif
		*dst++ = tmp;
		src++;
	}

	nouveau_bo_unmap(cursor);
	nouveau_bo_ref(NULL, &cursor);
}

void nv_crtc_show_cursor(xf86CrtcPtr crtc)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);

	nv_show_cursor(NVPTR(crtc->scrn), nv_crtc->head, true);
}

void nv_crtc_hide_cursor(xf86CrtcPtr crtc)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);

	nv_show_cursor(NVPTR(crtc->scrn), nv_crtc->head, false);
}

void nv_crtc_set_cursor_position(xf86CrtcPtr crtc, int x, int y)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(crtc->scrn);

	NVWriteRAMDAC(pNv, nv_crtc->head, NV_PRAMDAC_CU_START_POS,
		      XLATE(y, 0, NV_PRAMDAC_CU_START_POS_Y) |
		      XLATE(x, 0, NV_PRAMDAC_CU_START_POS_X));
}

Bool NVCursorInitRandr12(ScreenPtr pScreen)
{
	NVPtr pNv = NVPTR(xf86Screens[pScreen->myNum]);
	int width = nv_cursor_width(pNv);
	int flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
		    HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32 |
		    (pNv->alphaCursor ? HARDWARE_CURSOR_ARGB : 0);

	return xf86_cursors_init(pScreen, width, width, flags);
}
