/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define RADEONCTRACE(x)
/*#define RADEONCTRACE(x) RADEONTRACE(x) */

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *
 * References:
 *
 * !!!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 */

				/* Driver data structures */
#include "radeon.h"
#include "radeon_version.h"
#include "radeon_reg.h"
#include "radeon_macros.h"

				/* X and server generic header files */
#include "xf86.h"

/* Mono ARGB cursor colours (premultiplied). */
static CARD32 mono_cursor_color[] = {
	0x00000000, /* White, fully transparent. */
	0x00000000, /* Black, fully transparent. */
	0xffffffff, /* White, fully opaque. */
	0xff000000, /* Black, fully opaque. */
};

#define CURSOR_WIDTH	64
#define CURSOR_HEIGHT	64

#define COMMON_CURSOR_SWAPPING_START()	 RADEON_SYNC(info, pScrn)

/*
 * The cursor bits are always 32bpp.  On MSBFirst buses,
 * configure byte swapping to swap 32 bit units when writing
 * the cursor image.  Byte swapping must always be returned
 * to its previous value before returning.
 */
#if X_BYTE_ORDER == X_BIG_ENDIAN

#define CURSOR_SWAPPING_DECL_MMIO   unsigned char *RADEONMMIO = info->MMIO;
#define CURSOR_SWAPPING_START() \
  do { \
    COMMON_CURSOR_SWAPPING_START(); \
    OUTREG(RADEON_SURFACE_CNTL, \
	   (info->ModeReg.surface_cntl | \
	     RADEON_NONSURF_AP0_SWP_32BPP | RADEON_NONSURF_AP1_SWP_32BPP) & \
	   ~(RADEON_NONSURF_AP0_SWP_16BPP | RADEON_NONSURF_AP1_SWP_16BPP)); \
  } while (0)
#define CURSOR_SWAPPING_END()	(OUTREG(RADEON_SURFACE_CNTL, \
					info->ModeReg.surface_cntl))

#else

#define CURSOR_SWAPPING_DECL_MMIO
#define CURSOR_SWAPPING_START() \
  do { \
    COMMON_CURSOR_SWAPPING_START(); \
  } while (0)
#define CURSOR_SWAPPING_END()

#endif

static void
RADEONCrtcCursor(xf86CrtcPtr crtc,  Bool force)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
    int crtc_id = radeon_crtc->crtc_id;
    RADEONInfoPtr      info       = RADEONPTR(pScrn);
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    Bool		show;
    unsigned char     *RADEONMMIO = info->MMIO;
    CARD32 save1 = 0, save2 = 0;

    if (!crtc->enabled && !crtc->cursorShown)
      return;

    
    show = crtc->cursorInRange && crtc->enabled;
    if (show && (force || !crtc->cursorShown))
    {
	if (crtc_id == 0) 
	    OUTREGP(RADEON_CRTC_GEN_CNTL, RADEON_CRTC_CUR_EN,
		    ~RADEON_CRTC_CUR_EN);
	else if (crtc_id == 1)
	    OUTREGP(RADEON_CRTC2_GEN_CNTL, RADEON_CRTC2_CUR_EN,
		    ~RADEON_CRTC2_CUR_EN);
	crtc->cursorShown = TRUE;
    } else if (!show && (force || crtc->cursorShown)) {

	if (crtc_id == 0)
	    OUTREGP(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_CUR_EN);
	else if (crtc_id == 1)
	    OUTREGP(RADEON_CRTC2_GEN_CNTL, 0, ~RADEON_CRTC2_CUR_EN);

	crtc->cursorShown = FALSE;
    }
    
}    

/* Set cursor foreground and background colors */
static void RADEONSetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    CARD32        *pixels     = (CARD32 *)(pointer)(info->FB + info->cursor_offset + pScrn->fbOffset);
    int            pixel, i;
    CURSOR_SWAPPING_DECL_MMIO

    RADEONCTRACE(("RADEONSetCursorColors\n"));

#ifdef ARGB_CURSOR
    /* Don't recolour cursors set with SetCursorARGB. */
    if (info->cursor_argb)
       return;
#endif

    fg |= 0xff000000;
    bg |= 0xff000000;

    /* Don't recolour the image if we don't have to. */
    if (fg == info->cursor_fg && bg == info->cursor_bg)
       return;

    CURSOR_SWAPPING_START();

    /* Note: We assume that the pixels are either fully opaque or fully
     * transparent, so we won't premultiply them, and we can just
     * check for non-zero pixel values; those are either fg or bg
     */
    for (i = 0; i < CURSOR_WIDTH * CURSOR_HEIGHT; i++, pixels++)
       if ((pixel = *pixels))
           *pixels = (pixel == info->cursor_fg) ? fg : bg;

    CURSOR_SWAPPING_END();
    info->cursor_fg = fg;
    info->cursor_bg = bg;
}

static void
RADEONRandrSetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONInfoPtr      info       = RADEONPTR(pScrn);
    unsigned char     *RADEONMMIO = info->MMIO;
    xf86CursorInfoPtr  cursor     = info->cursor;
    Bool inrange;
    int temp;
    int oldx = x, oldy = y;
    int hotspotx = 0, hotspoty = 0;
    int c;
    int xorigin, yorigin;
    int		       stride     = 256;

    oldx += pScrn->frameX0; /* undo what xf86HWCurs did */
    oldy += pScrn->frameY0;

    x = oldx;
    y = oldy;

    for (c = 0 ; c < xf86_config->num_crtc; c++) {
	xf86CrtcPtr crtc = xf86_config->crtc[c];
	DisplayModePtr mode = &crtc->curMode;
	int thisx = x - crtc->x;
	int thisy = y - crtc->y;
	
	if (!crtc->enabled && !crtc->cursorShown)
	    continue;

	/*
	 * There is a screen display problem when the cursor position is set
	 * wholely outside of the viewport.  We trap that here, turning the
	 * cursor off when that happens, and back on when it comes back into
	 * the viewport.
	 */
	inrange = TRUE;
	if (thisx >= mode->HDisplay ||
	    thisy >= mode->VDisplay ||
	    thisx <= -cursor->MaxWidth || thisy <= -cursor->MaxHeight) 
	{
	    inrange = FALSE;
	    thisx = 0;
	    thisy = 0;
	}

	temp = 0;
	xorigin = 0;
	yorigin = 0;
	if (thisx < 0) xorigin = -thisx+1;
	if (thisy < 0) yorigin = -thisy+1;
	if (xorigin >= cursor->MaxWidth) xorigin = cursor->MaxWidth - 1;
	if (yorigin >= cursor->MaxHeight) yorigin = cursor->MaxHeight - 1;

	temp |= (xorigin ? 0 : thisx) << 16;
	temp |= (yorigin ? 0 : thisy);

	if (c == 0) {
	    OUTREG(RADEON_CUR_HORZ_VERT_OFF,  (RADEON_CUR_LOCK
					       | (xorigin << 16)
					       | yorigin));
	    OUTREG(RADEON_CUR_HORZ_VERT_POSN, (RADEON_CUR_LOCK |
					       temp));
	    RADEONCTRACE(("cursor_offset: 0x%x, yorigin: %d, stride: %d, temp %08X\n",
			  info->cursor_offset + pScrn->fbOffset, yorigin, stride, temp));
	    OUTREG(RADEON_CUR_OFFSET,
		   info->cursor_offset + pScrn->fbOffset + yorigin * stride);
	}
	if (c == 1) {
	    OUTREG(RADEON_CUR2_HORZ_VERT_OFF,  (RADEON_CUR2_LOCK
						| (xorigin << 16)
						| yorigin));
	    OUTREG(RADEON_CUR2_HORZ_VERT_POSN, (RADEON_CUR2_LOCK |
						temp));
	    RADEONCTRACE(("cursor_offset2: 0x%x, yorigin: %d, stride: %d, temp %08X\n",
			  info->cursor_offset + pScrn->fbOffset, yorigin, stride, temp));
	    OUTREG(RADEON_CUR2_OFFSET,
		   info->cursor_offset + pScrn->fbOffset + yorigin * stride);
	}
	crtc->cursorInRange = inrange;

	RADEONCrtcCursor(crtc, FALSE);
    }


}

/* Copy cursor image from `image' to video memory.  RADEONSetCursorPosition
 * will be called after this, so we can ignore xorigin and yorigin.
 */
static void RADEONLoadCursorImage(ScrnInfoPtr pScrn, unsigned char *image)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    CARD8         *s          = (CARD8 *)(pointer)image;
    CARD32        *d          = (CARD32 *)(pointer)(info->FB + info->cursor_offset + pScrn->fbOffset);
    CARD32         save1      = 0;
    CARD32         save2      = 0;
    CARD8	   chunk;
    CARD32         i, j;

    RADEONCTRACE(("RADEONLoadCursorImage (at %x)\n", info->cursor_offset));


    if (!info->IsSecondary) {
	save1 = INREG(RADEON_CRTC_GEN_CNTL) & ~(CARD32) (3 << 20);
	save1 |= (CARD32) (2 << 20);
	OUTREG(RADEON_CRTC_GEN_CNTL, save1 & (CARD32)~RADEON_CRTC_CUR_EN);

	if (pRADEONEnt->HasCRTC2) {
	    save2 = INREG(RADEON_CRTC2_GEN_CNTL) & ~(CARD32) (3 << 20);
	    save2 |= (CARD32) (2 << 20);
	    OUTREG(RADEON_CRTC2_GEN_CNTL, save2 & (CARD32)~RADEON_CRTC2_CUR_EN);
	}
    }

    if (info->IsSecondary) {
	save2 = INREG(RADEON_CRTC2_GEN_CNTL) & ~(CARD32) (3 << 20);
	save2 |= (CARD32) (2 << 20);
	OUTREG(RADEON_CRTC2_GEN_CNTL, save2 & (CARD32)~RADEON_CRTC2_CUR_EN);
    }

#ifdef ARGB_CURSOR
    info->cursor_argb = FALSE;
#endif

    /*
     * Convert the bitmap to ARGB32.
     *
     * HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_1 always places
     * source in the low bit of the pair and mask in the high bit,
     * and MSBFirst machines set HARDWARE_CURSOR_BIT_ORDER_MSBFIRST
     * (which actually bit swaps the image) to make the bits LSBFirst
     */
    CURSOR_SWAPPING_START();
#define ARGB_PER_CHUNK	(8 * sizeof (chunk) / 2)
    for (i = 0; i < (CURSOR_WIDTH * CURSOR_HEIGHT / ARGB_PER_CHUNK); i++) {
        chunk = *s++;
	for (j = 0; j < ARGB_PER_CHUNK; j++, chunk >>= 2)
	    *d++ = mono_cursor_color[chunk & 3];
    }
    CURSOR_SWAPPING_END();

    info->cursor_bg = mono_cursor_color[2];
    info->cursor_fg = mono_cursor_color[3];

    if (!info->IsSecondary)
	OUTREG(RADEON_CRTC_GEN_CNTL, save1);

    OUTREG(RADEON_CRTC2_GEN_CNTL, save2);

}

/* Hide hardware cursor. */
static void RADEONHideCursor(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int c;
    RADEONCTRACE(("RADEONHideCursor\n"));

    for (c = 0; c < xf86_config->num_crtc; c++)
        RADEONCrtcCursor(xf86_config->crtc[c], TRUE);
}

/* Show hardware cursor. */
static void RADEONShowCursor(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int c;

    RADEONCTRACE(("RADEONShowCursor\n"));

    for (c = 0; c < xf86_config->num_crtc; c++)
        RADEONCrtcCursor(xf86_config->crtc[c], FALSE);
}

/* Determine if hardware cursor is in use. */
static Bool RADEONUseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);

    return info->cursor ? TRUE : FALSE;
}

#ifdef ARGB_CURSOR
#include "cursorstr.h"

static Bool RADEONUseHWCursorARGB (ScreenPtr pScreen, CursorPtr pCurs)
{
    if (RADEONUseHWCursor(pScreen, pCurs) &&
	pCurs->bits->height <= CURSOR_HEIGHT && pCurs->bits->width <= CURSOR_WIDTH)
	return TRUE;
    return FALSE;
}

static void RADEONLoadCursorARGB (ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32        *d          = (CARD32 *)(pointer)(info->FB + info->cursor_offset + pScrn->fbOffset);
    int            x, y, w, h;
    CARD32         save1      = 0;
    CARD32         save2      = 0;
    CARD32	  *image = pCurs->bits->argb;
    CARD32	  *i;

    RADEONCTRACE(("RADEONLoadCursorARGB\n"));

    if (!info->IsSecondary) {
	save1 = INREG(RADEON_CRTC_GEN_CNTL) & ~(CARD32) (3 << 20);
	save1 |= (CARD32) (2 << 20);
	OUTREG(RADEON_CRTC_GEN_CNTL, save1 & (CARD32)~RADEON_CRTC_CUR_EN);

	if (pRADEONEnt->HasCRTC2) {
	    save2 = INREG(RADEON_CRTC2_GEN_CNTL) & ~(CARD32) (3 << 20);
	    save2 |= (CARD32) (2 << 20);
	    OUTREG(RADEON_CRTC2_GEN_CNTL, save2 & (CARD32)~RADEON_CRTC2_CUR_EN);
	}
    }

    if (info->IsSecondary) {
	save2 = INREG(RADEON_CRTC2_GEN_CNTL) & ~(CARD32) (3 << 20);
	save2 |= (CARD32) (2 << 20);
	OUTREG(RADEON_CRTC2_GEN_CNTL, save2 & (CARD32)~RADEON_CRTC2_CUR_EN);
    }

#ifdef ARGB_CURSOR
    info->cursor_argb = TRUE;
#endif

    CURSOR_SWAPPING_START();

    w = pCurs->bits->width;
    if (w > CURSOR_WIDTH)
	w = CURSOR_WIDTH;
    h = pCurs->bits->height;
    if (h > CURSOR_HEIGHT)
	h = CURSOR_HEIGHT;
    for (y = 0; y < h; y++)
    {
	i = image;
	image += pCurs->bits->width;
	for (x = 0; x < w; x++)
	    *d++ = *i++;
	/* pad to the right with transparent */
	for (; x < CURSOR_WIDTH; x++)
	    *d++ = 0;
    }
    /* pad below with transparent */
    for (; y < CURSOR_HEIGHT; y++)
	for (x = 0; x < CURSOR_WIDTH; x++)
	    *d++ = 0;

    CURSOR_SWAPPING_END ();

    if (!info->IsSecondary)
	OUTREG(RADEON_CRTC_GEN_CNTL, save1);

    if (info->IsSecondary || info->MergedFB)
	OUTREG(RADEON_CRTC2_GEN_CNTL, save2);

}

#endif


/* Initialize hardware cursor support. */
Bool RADEONCursorInit(ScreenPtr pScreen)
{
    ScrnInfoPtr        pScrn   = xf86Screens[pScreen->myNum];
    RADEONInfoPtr      info    = RADEONPTR(pScrn);
    xf86CursorInfoPtr  cursor;
    int                width;
    int		       width_bytes;
    int                height;
    int                size_bytes;

    if (!(cursor = info->cursor = xf86CreateCursorInfoRec())) return FALSE;

    cursor->MaxWidth          = CURSOR_WIDTH;
    cursor->MaxHeight         = CURSOR_HEIGHT;
    cursor->Flags             = (HARDWARE_CURSOR_TRUECOLOR_AT_8BPP
				 | HARDWARE_CURSOR_AND_SOURCE_WITH_MASK
#if X_BYTE_ORDER == X_BIG_ENDIAN
				 /* this is a lie --
				  * HARDWARE_CURSOR_BIT_ORDER_MSBFIRST
				  * actually inverts the bit order, so
				  * this switches to LSBFIRST
				  */
				 | HARDWARE_CURSOR_BIT_ORDER_MSBFIRST
#endif
				 | HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_1);

    cursor->SetCursorColors   = RADEONSetCursorColors;
    cursor->SetCursorPosition = RADEONRandrSetCursorPosition;
    cursor->LoadCursorImage   = RADEONLoadCursorImage;
    cursor->HideCursor        = RADEONHideCursor;
    cursor->ShowCursor        = RADEONShowCursor;
    cursor->UseHWCursor       = RADEONUseHWCursor;

#ifdef ARGB_CURSOR
    cursor->UseHWCursorARGB   = RADEONUseHWCursorARGB;
    cursor->LoadCursorARGB    = RADEONLoadCursorARGB;
#endif
    size_bytes                = CURSOR_WIDTH * 4 * CURSOR_HEIGHT;
    width                     = pScrn->displayWidth;
    width_bytes		      = width * (pScrn->bitsPerPixel / 8);
    height                    = (size_bytes + width_bytes - 1) / width_bytes;

#ifdef USE_XAA
    if (!info->useEXA) {
	FBAreaPtr          fbarea;

	fbarea = xf86AllocateOffscreenArea(pScreen, width, height,
					   256, NULL, NULL, NULL);

	if (!fbarea) {
	    info->cursor_offset    = 0;
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Hardware cursor disabled"
		   " due to insufficient offscreen memory\n");
	} else {
	    info->cursor_offset  = RADEON_ALIGN((fbarea->box.x1 +
						fbarea->box.y1 * width) *
						info->CurrentLayout.pixel_bytes,
						256);
	    info->cursor_end = info->cursor_offset + size_bytes;
	}
	RADEONCTRACE(("RADEONCursorInit (0x%08x-0x%08x)\n",
		    info->cursor_offset, info->cursor_end));
    }
#endif

    return xf86InitCursor(pScreen, cursor);
}
