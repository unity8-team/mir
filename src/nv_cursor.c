/*
 * Copyright 2003 NVIDIA, Corporation
 * Copyright 2007 Maarten Maathuis
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

#include "cursorstr.h"

/****************************************************************************\
*                                                                            *
*                          HW Cursor Entrypoints                             *
*                                                                            *
\****************************************************************************/

#define CURSOR_X_SHIFT 0
#define CURSOR_Y_SHIFT 16
#define CURSOR_POS_MASK 0xffff

#define TRANSPARENT_PIXEL   0

#define ConvertToRGB555(c)  (((c & 0xf80000) >> 9 ) | /* Blue  */           \
                            ((c & 0xf800) >> 6 )    | /* Green */           \
                            ((c & 0xf8) >> 3 )      | /* Red   */           \
                            0x8000)                   /* Set upper bit, else we get complete transparency. */

#define ConvertToRGB888(c) (c | 0xff000000)

#define BYTE_SWAP_32(c)  ((c & 0xff000000) >> 24) |  \
                         ((c & 0xff0000) >> 8) |     \
                         ((c & 0xff00) << 8) |       \
                         ((c & 0xff) << 24)

/* Limit non-alpha cursors to 32x32 (x2 bytes) */
#define MAX_CURSOR_SIZE 32

/* Limit alpha cursors to 64x64 (x4 bytes) */
#define MAX_CURSOR_SIZE_ALPHA (MAX_CURSOR_SIZE * 2)

static void 
ConvertCursor1555(NVPtr pNv, CARD32 *src, CARD16 *dst)
{
	CARD32 b, m;
	int i, j;
	int sz=pNv->NVArch==0x10?MAX_CURSOR_SIZE_ALPHA:MAX_CURSOR_SIZE;

	for ( i = 0; i < sz; i++ ) {
		b = *src++;
		m = *src++;
		for ( j = 0; j < sz; j++ ) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
			if ( m & 0x80000000)
				*dst = ( b & 0x80000000) ? pNv->curFg : pNv->curBg;
			else
				*dst = TRANSPARENT_PIXEL;
			b <<= 1;
			m <<= 1;
#else
			if ( m & 1 )
				*dst = ( b & 1) ? pNv->curFg : pNv->curBg;
			else
				*dst = TRANSPARENT_PIXEL;
			b >>= 1;
			m >>= 1;
#endif
			dst++;
		}
	}
}


static void
ConvertCursor8888(NVPtr pNv, CARD32 *src, CARD32 *dst)
{
	CARD32 b, m;
	int i, j;

	/* Iterate over each byte in the cursor. */
	for ( i = 0; i < MAX_CURSOR_SIZE * 4; i++ ) {
		b = *src++;
		m = *src++;
		for ( j = 0; j < MAX_CURSOR_SIZE; j++ ) {
#if X_BYTE_ORDER == X_BIG_ENDIAN
			if ( m & 0x80000000)
				*dst = ( b & 0x80000000) ? pNv->curFg : pNv->curBg;
			else
				*dst = TRANSPARENT_PIXEL;
			b <<= 1;
			m <<= 1;
#else
			if ( m & 1 )
				*dst = ( b & 1) ? pNv->curFg : pNv->curBg;
			else
				*dst = TRANSPARENT_PIXEL;
			b >>= 1;
			m >>= 1;
#endif
			dst++;
		}
	}
}

static void
TransformCursor (NVPtr pNv)
{
	CARD32 *tmp;
	int i, dwords;

	/* convert to color cursor */
	if(pNv->NVArch==0x10) {
		dwords = (MAX_CURSOR_SIZE_ALPHA * MAX_CURSOR_SIZE_ALPHA) >> 1;
		if(!(tmp = xalloc(dwords * 4))) return;
		ConvertCursor1555(pNv, pNv->curImage, (CARD16*)tmp);
	} else if(pNv->alphaCursor) {
		dwords = MAX_CURSOR_SIZE_ALPHA * MAX_CURSOR_SIZE_ALPHA;
		if(!(tmp = xalloc(dwords * 4))) return;
		ConvertCursor8888(pNv, pNv->curImage, tmp);
	} else {
		dwords = (MAX_CURSOR_SIZE * MAX_CURSOR_SIZE) >> 1;
		if(!(tmp = xalloc(dwords * 4))) return;
		ConvertCursor1555(pNv, pNv->curImage, (CARD16*)tmp);
	}

	for(i = 0; i < dwords; i++)
		pNv->CURSOR[i] = tmp[i];

	xfree(tmp);
}

static void
NVLoadCursorImage( ScrnInfoPtr pScrn, unsigned char *src )
{
    NVPtr pNv = NVPTR(pScrn);

    /* save copy of image for color changes */
    memcpy(pNv->curImage, src, (pNv->alphaCursor) ? 1024 : 256);

    TransformCursor(pNv);
}

static void
NVSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
	NVPtr pNv = NVPTR(pScrn);
	nvWriteCurRAMDAC(pNv, NV_RAMDAC_CURSOR_POS, (x & 0xFFFF) | (y << 16));
}

static void
NVSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    NVPtr pNv = NVPTR(pScrn);
    CARD32 fore, back;

    if(pNv->alphaCursor) {
        fore = ConvertToRGB888(fg);
        back = ConvertToRGB888(bg);
#if X_BYTE_ORDER == X_BIG_ENDIAN
        if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
           fore = BYTE_SWAP_32(fore);
           back = BYTE_SWAP_32(back);
        }
#endif
    } else {
        fore = ConvertToRGB555(fg);
        back = ConvertToRGB555(bg);
#if X_BYTE_ORDER == X_BIG_ENDIAN
        if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
           fore = ((fore & 0xff) << 8) | (fore >> 8);
           back = ((back & 0xff) << 8) | (back >> 8);
        }
#endif
   }

    if ((pNv->curFg != fore) || (pNv->curBg != back)) {
        pNv->curFg = fore;
        pNv->curBg = back;
            
        TransformCursor(pNv);
    }
}


static void 
NVShowCursor(ScrnInfoPtr pScrn)
{
	/* Enable cursor - X-Windows mode */
	NVShowHideCursor(pScrn, 1);
}

static void
NVHideCursor(ScrnInfoPtr pScrn)
{
	/* Disable cursor */
	NVShowHideCursor(pScrn, 0);
}

static Bool 
NVUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
	return TRUE;
}

#ifdef ARGB_CURSOR
static Bool 
NVUseHWCursorARGB(ScreenPtr pScreen, CursorPtr pCurs)
{
    if((pCurs->bits->width <= MAX_CURSOR_SIZE_ALPHA) && (pCurs->bits->height <= MAX_CURSOR_SIZE_ALPHA))
        return TRUE;

    return FALSE;
}

static void
NVLoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    NVPtr pNv = NVPTR(pScrn);
    CARD32 *image = pCurs->bits->argb;
    CARD32 *dst = (CARD32*)pNv->CURSOR;
    CARD32 alpha, tmp;
    int x, y, w, h;

    w = pCurs->bits->width;
    h = pCurs->bits->height;

    if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {  /* premultiply */
       for(y = 0; y < h; y++) {
          for(x = 0; x < w; x++) {
             alpha = *image >> 24;
             if(alpha == 0xff)
                tmp = *image;
             else {
                tmp = (alpha << 24) |
                         (((*image & 0xff) * alpha) / 255) |
                        ((((*image & 0xff00) * alpha) / 255) & 0xff00) |
                       ((((*image & 0xff0000) * alpha) / 255) & 0xff0000); 
             }
             image++;
#if X_BYTE_ORDER == X_BIG_ENDIAN
             *dst++ = BYTE_SWAP_32(tmp);
#else
             *dst++ = tmp;
#endif
         }
         for(; x < MAX_CURSOR_SIZE_ALPHA; x++)
             *dst++ = 0;
      }
    } else {
       for(y = 0; y < h; y++) {
          for(x = 0; x < w; x++)
              *dst++ = *image++;
          for(; x < MAX_CURSOR_SIZE_ALPHA; x++)
              *dst++ = 0;
       }
    }

    if(y < MAX_CURSOR_SIZE_ALPHA)
      memset(dst, 0, MAX_CURSOR_SIZE_ALPHA * (MAX_CURSOR_SIZE_ALPHA - y) * 4);
}
#endif

Bool 
NVCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);
    xf86CursorInfoPtr infoPtr;

    infoPtr = xf86CreateCursorInfoRec();
    if(!infoPtr) return FALSE;
    
    pNv->CursorInfoRec = infoPtr;

    if(pNv->alphaCursor)
       infoPtr->MaxWidth = infoPtr->MaxHeight = MAX_CURSOR_SIZE_ALPHA;
    else
       infoPtr->MaxWidth = infoPtr->MaxHeight = MAX_CURSOR_SIZE;

    infoPtr->Flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
                     HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32; 
    infoPtr->SetCursorColors = NVSetCursorColors;
    infoPtr->SetCursorPosition = NVSetCursorPosition;
    infoPtr->LoadCursorImage = NVLoadCursorImage;
    infoPtr->HideCursor = NVHideCursor;
    infoPtr->ShowCursor = NVShowCursor;
    infoPtr->UseHWCursor = NVUseHWCursor;

#ifdef ARGB_CURSOR
    if(pNv->alphaCursor) {
       infoPtr->UseHWCursorARGB = NVUseHWCursorARGB;
       infoPtr->LoadCursorARGB = NVLoadCursorARGB;
    }
#endif

    return(xf86InitCursor(pScreen, infoPtr));
}

#define CURSOR_PTR ((CARD32*)pNv->Cursor->map)

Bool NVCursorInitRandr12(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	int flags = HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
			HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32;
	int cursor_size = 0;
	if (pNv->alphaCursor) { /* >= NV11 */
		cursor_size = MAX_CURSOR_SIZE_ALPHA;
		flags |= HARDWARE_CURSOR_ARGB;
	} else {
		cursor_size = MAX_CURSOR_SIZE;
	}
	return xf86_cursors_init(pScreen, cursor_size, cursor_size, flags);
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
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);

	NVWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_CURSOR_POS, ((x & CURSOR_POS_MASK) << CURSOR_X_SHIFT) | (y << CURSOR_Y_SHIFT));
}

void nv_crtc_set_cursor_colors(xf86CrtcPtr crtc, int bg, int fg)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	CARD32 fore, back;

	fore = ConvertToRGB555(fg);
	back = ConvertToRGB555(bg);
#if X_BYTE_ORDER == X_BIG_ENDIAN
	if(pNv->NVArch == 0x11) {
		fore = ((fore & 0xff) << 8) | (fore >> 8);
		back = ((back & 0xff) << 8) | (back >> 8);
	}
#endif

	/* Eventually this must be replaced as well */
	if ((pNv->curFg != fore) || (pNv->curBg != back)) {
		pNv->curFg = fore;
		pNv->curBg = back;
		TransformCursor(pNv);
	}
}


void nv_crtc_load_cursor_image(xf86CrtcPtr crtc, CARD8 *image)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	struct nouveau_bo *cursor = NULL;
	NVPtr pNv = NVPTR(pScrn);

	/* save copy of image for color changes */
	memcpy(pNv->curImage, image, 256);

	if (pNv->Architecture >= NV_ARCH_10) {
		/* Due to legacy code */
		nouveau_bo_ref(nv_crtc->head ? pNv->Cursor2 : pNv->Cursor, &cursor);
		nouveau_bo_map(cursor, NOUVEAU_BO_WR);
		pNv->CURSOR = cursor->map;
	}

	/* Eventually this has to be replaced as well */
	TransformCursor(pNv);

	if (cursor) {
		nouveau_bo_unmap(cursor);
		nouveau_bo_ref(NULL, &cursor);
	}
}

void nv_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *image)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	struct nouveau_bo *cursor = NULL;
	uint32_t *dst = NULL;
	uint32_t *src = (uint32_t *)image;

	nouveau_bo_ref(nv_crtc->head ? pNv->Cursor2 : pNv->Cursor, &cursor);
	nouveau_bo_map(cursor, NOUVEAU_BO_WR);
	dst = cursor->map;

	/* It seems we get premultiplied alpha and the hardware takes non-premultiplied? */
	/* This is needed, because without bit28 of cursorControl, we use what ever ROP is set currently */
	/* This causes artifacts (on nv4x at least) */
	int x, y;
	uint32_t alpha, value;

	if (pNv->NVArch == 0x11) { /* NV11 takes premultiplied cursors i think. */
		for (x = 0; x < MAX_CURSOR_SIZE_ALPHA; x++) {
			for (y = 0; y < MAX_CURSOR_SIZE_ALPHA; y++) {
				/* I suspect NV11 is the only card needing cursor byteswapping. */
				#if X_BYTE_ORDER == X_BIG_ENDIAN
					*dst++ = BYTE_SWAP_32(*src);
					src++;
				#else
					*dst++ = *src++;
				#endif
			}
		}
	} else {
		for (x = 0; x < MAX_CURSOR_SIZE_ALPHA; x++) {
			for (y = 0; y < MAX_CURSOR_SIZE_ALPHA; y++) {
				alpha = *src >> 24;
				if (alpha == 0x0 || alpha == 0xff) {
					value = *src;
				} else {
					value = 	((((*src & 0xff) * 0xff) / alpha) 		& 0x000000ff)	|
							((((*src & 0xff00) * 0xff) / alpha) 	& 0x0000ff00)	|
							((((*src & 0xff0000) * 0xff) / alpha)	& 0x00ff0000)	|
							((alpha << 24)				& 0xff000000);
				}
				src++;
				*dst++ = value;
			}
		}
	}

	nouveau_bo_unmap(cursor);
}
