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

#define CURSOR_Y_SHIFT 16
#define CURSOR_POS_MASK 0xffff

#define TO_ARGB1555(c) (0x8000			|	/* Mask bit */	\
			((c & 0xf80000) >> 9 )	|	/* Red      */	\
			((c & 0xf800) >> 6 )	|	/* Green    */	\
			((c & 0xf8) >> 3 ))		/* Blue     */
#define TO_ARGB8888(c) (0xff000000 | c)

/* nv04 cursor max dimensions of 32x32 (A1R5G5B5) */
#define NV04_CURSOR_SIZE 32
#define NV04_CURSOR_PIXELS (NV04_CURSOR_SIZE * NV04_CURSOR_SIZE)

/* limit nv10/11 <-FIXME cursors to 64x64 (ARGB8) (we could go to 64x255) */
#define NV1x_CURSOR_SIZE 64
#define NV1x_CURSOR_PIXELS (NV1x_CURSOR_SIZE * NV1x_CURSOR_SIZE)

#define SOURCE_MASK_INTERLEAVE 32
#define TRANSPARENT_PIXEL   0

static void
nv_cursor_convert_cursor(int px, uint32_t *src, uint16_t *dst, int bpp, uint32_t fg, uint32_t bg)
{
	uint32_t b, m, pxval;
	int i, j;

	for (i = 0; i < px / SOURCE_MASK_INTERLEAVE; i++) {
		b = *src++;
		m = *src++;
		for (j = 0; j < SOURCE_MASK_INTERLEAVE; j++) {
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
			if (bpp == 32) {
				*(uint32_t *)dst = pxval;
				dst += 2;
			} else
				*dst++ = pxval;
		}
	}
}

static void nv_cursor_transform_cursor(NVPtr pNv, int head)
{
	uint32_t *tmp;
	struct nouveau_bo *cursor = NULL;
	int px = pNv->NVArch >= 0x10 ? NV1x_CURSOR_PIXELS : NV04_CURSOR_PIXELS;

	if (!(tmp = xcalloc(px, 4)))
		return;

	/* convert to colour cursor */
	if (pNv->alphaCursor)
		nv_cursor_convert_cursor(px, pNv->curImage, (uint16_t *)tmp, 32, pNv->curFg, pNv->curBg);
	else
		nv_cursor_convert_cursor(px, pNv->curImage, (uint16_t *)tmp, 16, pNv->curFg, pNv->curBg);

	if (pNv->Architecture >= NV_ARCH_10) {
		nouveau_bo_ref(head ? pNv->Cursor2 : pNv->Cursor, &cursor);
		nouveau_bo_map(cursor, NOUVEAU_BO_WR);

		memcpy(cursor->map, tmp, px * 4);

		nouveau_bo_unmap(cursor);
		nouveau_bo_ref(NULL, &cursor);
	} else
		for (i = 0; i < (px / 2); i++)
			pNv->CURSOR[i] = tmp[i];

	xfree(tmp);
}

void nv_crtc_set_cursor_colors(xf86CrtcPtr crtc, int bg, int fg)
{
	NVPtr pNv = NVPTR(crtc->scrn);
	uint32_t fore, back;
	int head = to_nouveau_crtc(crtc)->head;

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

	if (pNv->curFg != fore || pNv->curBg != back) {
		pNv->curFg = fore;
		pNv->curBg = back;
		nv_cursor_transform_cursor(pNv, head);
	}
}

void nv_crtc_load_cursor_image(xf86CrtcPtr crtc, CARD8 *image)
{
	NVPtr pNv = NVPTR(crtc->scrn);
	int sz = (pNv->NVArch >= 0x10 ? NV1x_CURSOR_PIXELS : NV04_CURSOR_PIXELS) / 4;

	/* save copy of image for colour changes */
	memcpy(pNv->curImage, image, sz);

	nv_cursor_transform_cursor(pNv, to_nouveau_crtc(crtc)->head);
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

	if (pNv->NVArch != 0x11)
		/* the blob uses non-premultiplied alpha mode for cursors on
		 * most hardware, so here the multiplication is undone...
		 */
		for (i = 0; i < NV1x_CURSOR_PIXELS; i++) {
			alpha = *src >> 24;
			if (alpha == 0x0 || alpha == 0xff)
				*dst++ = *src;
			else
				*dst++ = (alpha << 24)					      |
					 ((((*src & 0xff0000) * 0xff) / alpha)	& 0x00ff0000) |
					 ((((*src & 0xff00) * 0xff) / alpha) 	& 0x0000ff00) |
					 ((((*src & 0xff) * 0xff) / alpha) 	& 0x000000ff);
			src++;
		}
	else
		/* use premultiplied alpha directly for NV11 (on-GPU blending
		 * apparently has issues in combination with fp dithering)
		 */
		for (i = 0; i < NV1x_CURSOR_PIXELS; i++) {
			alpha = (*src >> 24);
			if (alpha == 0xff)
				tmp = *src;
			else
				/* hw gets unhappy if alpha <= rgb values.  objecting to "less
				 * than" is reasonable (as cursor images are premultiplied),
				 * but fix "equal to" case by adding one to alpha channel
				 */
				tmp = ((alpha + 1) << 24) | (*src & 0xffffff);
#if X_BYTE_ORDER == X_BIG_ENDIAN
			*dst++ = lswapl(tmp);
#else
			*dst++ = tmp;
#endif
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

	NVWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_CURSOR_POS,
		      (y << CURSOR_Y_SHIFT) | (x & CURSOR_POS_MASK));
}

Bool NVCursorInitRandr12(ScreenPtr pScreen)
{
	NVPtr pNv = NVPTR(xf86Screens[pScreen->myNum]);
	int size = pNv->alphaCursor ? NV1x_CURSOR_SIZE : NV04_CURSOR_SIZE;
	int flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
		    HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32 |
		    (pNv->alphaCursor ? HARDWARE_CURSOR_ARGB : 0);

	return xf86_cursors_init(pScreen, size, size, flags);
}
