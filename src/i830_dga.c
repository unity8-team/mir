/*
 * Copyright 2000 by Alan Hourihane, Sychdyn, North Wales, UK.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Alan Hourihane not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Alan Hourihane makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * ALAN HOURIHANE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL ALAN HOURIHANE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, <alanh@fairlite.demon.co.uk>
 */
/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i830_dga.c,v 1.2 2002/11/05 02:01:18 dawes Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86DDC.h"
#include "xf86_OSproc.h"
#include "dgaproc.h"
#include "i830_xf86Crtc.h"
#include "i830_xf86Modes.h"
#include "gcstruct.h"

static DGAModePtr
xf86_dga_get_modes (ScreenPtr pScreen, int *nump)
{
    ScrnInfoPtr	    pScrn = xf86Screens[pScreen->myNum];
    DGAModePtr	    modes, mode;
    DisplayModePtr  display_mode;
    int		    bpp = pScrn->bitsPerPixel >> 3;
    int		    num;
    PixmapPtr	    pPixmap = pScreen->GetScreenPixmap (pScreen);

    if (!pPixmap)
	return NULL;

    num = 0;
    display_mode = pScrn->modes;
    while (display_mode) 
    {
	num++;
	display_mode = display_mode->next;
	if (display_mode == pScrn->modes)
	    break;
    }
    
    if (!num)
	return NULL;
    
    modes = xalloc(num * sizeof(DGAModeRec));
    if (!modes)
	return NULL;
    
    num = 0;
    display_mode = pScrn->modes;
    while (display_mode) 
    {
	mode = modes + num++;

	mode->mode = display_mode;
	mode->flags = DGA_CONCURRENT_ACCESS | DGA_PIXMAP_AVAILABLE;
        mode->flags |= DGA_FILL_RECT | DGA_BLIT_RECT;
	if (display_mode->Flags & V_DBLSCAN)
	    mode->flags |= DGA_DOUBLESCAN;
	if (display_mode->Flags & V_INTERLACE)
	    mode->flags |= DGA_INTERLACED;
	mode->byteOrder = pScrn->imageByteOrder;
	mode->depth = pScrn->depth;
	mode->bitsPerPixel = pScrn->bitsPerPixel;
	mode->red_mask = pScrn->mask.red;
	mode->green_mask = pScrn->mask.green;
	mode->blue_mask = pScrn->mask.blue;
	mode->visualClass = (bpp == 1) ? PseudoColor : TrueColor;
	mode->viewportWidth = display_mode->HDisplay;
	mode->viewportHeight = display_mode->VDisplay;
	mode->xViewportStep = (bpp == 3) ? 2 : 1;
	mode->yViewportStep = 1;
	mode->viewportFlags = DGA_FLIP_RETRACE;
	mode->offset = 0;
	mode->address = pPixmap->devPrivate.ptr;
	mode->bytesPerScanline = pPixmap->devKind;
	mode->imageWidth = pPixmap->drawable.width;
	mode->imageHeight = pPixmap->drawable.height;
	mode->pixmapWidth = mode->imageWidth;
	mode->pixmapHeight = mode->imageHeight;
	mode->maxViewportX = mode->imageWidth -	mode->viewportWidth;
	mode->maxViewportY = mode->imageHeight - mode->viewportHeight;

	display_mode = display_mode->next;
	if (display_mode == pScrn->modes)
	    break;
    }
    *nump = num;
    return modes;
}

static Bool
xf86_dga_set_mode(ScrnInfoPtr pScrn, DGAModePtr display_mode)
{
    ScreenPtr	pScreen = pScrn->pScreen;

    if (!display_mode) 
	xf86SwitchMode(pScreen, pScrn->currentMode);
    else
	xf86SwitchMode(pScreen, display_mode->mode);

    return TRUE;
}

static int
xf86_dga_get_viewport(ScrnInfoPtr pScrn)
{
    return 0;
}

static void
xf86_dga_set_viewport(ScrnInfoPtr pScrn, int x, int y, int flags)
{
   pScrn->AdjustFrame(pScrn->pScreen->myNum, x, y, flags);
}

static void
xf86_dga_fill_rect(ScrnInfoPtr pScrn, int x, int y, int w, int h, unsigned long color)
{
    ScreenPtr	pScreen = pScrn->pScreen;
    WindowPtr	pRoot = WindowTable [pScreen->myNum];
    GCPtr	pGC = GetScratchGC (pRoot->drawable.depth, pScreen);
    XID		vals[2];
    xRectangle	r;

    if (!pGC)
	return;
    vals[0] = color;
    vals[1] = IncludeInferiors;
    ChangeGC (pGC, GCForeground|GCSubwindowMode, vals);
    ValidateGC (&pRoot->drawable, pGC);
    r.x = x;
    r.y = y;
    r.width = w;
    r.height = h;
    pGC->ops->PolyFillRect (&pRoot->drawable, pGC, 1, &r);
    FreeScratchGC (pGC);
}

static void
xf86_dga_sync(ScrnInfoPtr pScrn)
{
    ScreenPtr	pScreen = pScrn->pScreen;
    WindowPtr	pRoot = WindowTable [pScreen->myNum];
    char	buffer[4];

    pScreen->GetImage (&pRoot->drawable, 0, 0, 1, 1, ZPixmap, ~0L, buffer);
}

static void
xf86_dga_blit_rect(ScrnInfoPtr pScrn, int srcx, int srcy, int w, int h, int dstx, int dsty)
{
    ScreenPtr	pScreen = pScrn->pScreen;
    WindowPtr	pRoot = WindowTable [pScreen->myNum];
    GCPtr	pGC = GetScratchGC (pRoot->drawable.depth, pScreen);
    XID		vals[1];

    if (!pGC)
	return;
    vals[0] = IncludeInferiors;
    ChangeGC (pGC, GCSubwindowMode, vals);
    ValidateGC (&pRoot->drawable, pGC);
    pGC->ops->CopyArea (&pRoot->drawable, &pRoot->drawable, pGC,
			srcx, srcy, w, h, dstx, dsty);
    FreeScratchGC (pGC);
}

static Bool
xf86_dga_open_framebuffer(ScrnInfoPtr pScrn,
		     char **name,
		     unsigned char **mem, int *size, int *offset, int *flags)
{
    ScreenPtr	pScreen = pScrn->pScreen;
    PixmapPtr	pPixmap = pScreen->GetScreenPixmap (pScreen);
    
    if (!pPixmap)
	return FALSE;
    
    *size = pPixmap->drawable.height * pPixmap->devKind;
    *mem = (unsigned char *) (pScrn->memPhysBase + pScrn->fbOffset);
    *offset = 0;
    *flags = DGA_NEED_ROOT;

    return TRUE;
}

static void
xf86_dga_close_framebuffer(ScrnInfoPtr pScrn)
{
}

static DGAFunctionRec xf86_dga_funcs = {
   xf86_dga_open_framebuffer,
   xf86_dga_close_framebuffer,
   xf86_dga_set_mode,
   xf86_dga_set_viewport,
   xf86_dga_get_viewport,
   xf86_dga_sync,
   xf86_dga_fill_rect,
   xf86_dga_blit_rect,
   NULL
};

static DGAModePtr   xf86_dga_modes[MAXSCREENS];

Bool
xf86_dga_reinit (ScreenPtr pScreen)
{
    int		num;
    DGAModePtr	modes;

    modes = xf86_dga_get_modes (pScreen, &num);
    if (!modes)
	return FALSE;

    if (xf86_dga_modes[pScreen->myNum])
	xfree (xf86_dga_modes[pScreen->myNum]);

    xf86_dga_modes[pScreen->myNum] = modes;
    return DGAReInitModes (pScreen, modes, num);
}

Bool
xf86_dga_init (ScreenPtr pScreen)
{
    int		num;
    DGAModePtr	modes;

    modes = xf86_dga_get_modes (pScreen, &num);
    if (!modes)
	return FALSE;

    if (xf86_dga_modes[pScreen->myNum])
	xfree (xf86_dga_modes[pScreen->myNum]);

    xf86_dga_modes[pScreen->myNum] = modes;
    return DGAInit(pScreen, &xf86_dga_funcs, modes, num);
}

