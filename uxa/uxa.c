/*
 * Copyright � 2001 Keith Packard
 *
 * Partly based on code that is Copyright � The XFree86 Project Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/** @file
 * This file covers the initialization and teardown of UXA, and has various
 * functions not responsible for performing rendering, pixmap migration, or
 * memory management.
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <stdlib.h>

#include "uxa-priv.h"
#include <X11/fonts/fontstruct.h>
#include "dixfontstr.h"
#include "uxa.h"

int uxa_screen_index;

/**
 * uxa_get_drawable_pixmap() returns a backing pixmap for a given drawable.
 *
 * @param pDrawable the drawable being requested.
 *
 * This function returns the backing pixmap for a drawable, whether it is a
 * redirected window, unredirected window, or already a pixmap.  Note that
 * coordinate translation is needed when drawing to the backing pixmap of a
 * redirected window, and the translation coordinates are provided by calling
 * uxa_get_drawable_pixmap() on the drawable.
 */
PixmapPtr
uxa_get_drawable_pixmap(DrawablePtr pDrawable)
{
     if (pDrawable->type == DRAWABLE_WINDOW)
	return pDrawable->pScreen->GetWindowPixmap ((WindowPtr) pDrawable);
     else
	return (PixmapPtr) pDrawable;
}	

/**
 * Sets the offsets to add to coordinates to make them address the same bits in
 * the backing drawable. These coordinates are nonzero only for redirected
 * windows.
 */
void
uxa_get_drawable_deltas (DrawablePtr pDrawable, PixmapPtr pPixmap,
			 int *xp, int *yp)
{
#ifdef COMPOSITE
    if (pDrawable->type == DRAWABLE_WINDOW) {
	*xp = -pPixmap->screen_x;
	*yp = -pPixmap->screen_y;
	return;
    }
#endif

    *xp = 0;
    *yp = 0;
}

/**
 * uxa_pixmap_is_offscreen() is used to determine if a pixmap is in offscreen
 * memory, meaning that acceleration could probably be done to it, and that it
 * will need to be wrapped by PrepareAccess()/FinishAccess() when accessing it
 * with the CPU.
 *
 * Note that except for UploadToScreen()/DownloadFromScreen() (which explicitly
 * deal with moving pixmaps in and out of system memory), UXA will give drivers
 * pixmaps as arguments for which uxa_pixmap_is_offscreen() is TRUE.
 *
 * @return TRUE if the given drawable is in framebuffer memory.
 */
Bool
uxa_pixmap_is_offscreen(PixmapPtr p)
{
    ScreenPtr	    pScreen = p->drawable.pScreen;
    uxa_screen_t    *uxa_screen = uxa_get_screen(pScreen);

    if (uxa_screen->info->pixmap_is_offscreen)
	return uxa_screen->info->pixmap_is_offscreen(p);

    return FALSE;
}

/**
 * uxa_drawable_is_offscreen() is a convenience wrapper for uxa_pixmap_is_offscreen().
 */
Bool
uxa_drawable_is_offscreen (DrawablePtr pDrawable)
{
    return uxa_pixmap_is_offscreen (uxa_get_drawable_pixmap (pDrawable));
}

/**
  * Returns the pixmap which backs a drawable, and the offsets to add to
  * coordinates to make them address the same bits in the backing drawable.
  */
PixmapPtr
uxa_get_offscreen_pixmap (DrawablePtr drawable, int *xp, int *yp)
{
    PixmapPtr   pixmap = uxa_get_drawable_pixmap (drawable);

    uxa_get_drawable_deltas (drawable, pixmap, xp, yp);

    if (uxa_pixmap_is_offscreen (pixmap))
	return pixmap;
    else
	return NULL;
}



/**
 * uxa_prepare_access() is UXA's wrapper for the driver's PrepareAccess() handler.
 *
 * It deals with waiting for synchronization with the card, determining if
 * PrepareAccess() is necessary, and working around PrepareAccess() failure.
 */
void
uxa_prepare_access(DrawablePtr pDrawable, uxa_access_t access)
{
    ScreenPtr	    pScreen = pDrawable->pScreen;
    uxa_screen_t    *uxa_screen = uxa_get_screen(pScreen);
    PixmapPtr	    pPixmap = uxa_get_drawable_pixmap (pDrawable);
    Bool	    offscreen = uxa_pixmap_is_offscreen(pPixmap);

    if (!offscreen)
	return;

    if (uxa_screen->info->prepare_access)
	(*uxa_screen->info->prepare_access) (pPixmap, access);
}

/**
 * uxa_finish_access() is UXA's wrapper for the driver's finish_access() handler.
 *
 * It deals with calling the driver's finish_access() only if necessary.
 */
void
uxa_finish_access(DrawablePtr pDrawable)
{
    ScreenPtr	    pScreen = pDrawable->pScreen;
    uxa_screen_t    *uxa_screen = uxa_get_screen(pScreen);
    PixmapPtr	    pPixmap = uxa_get_drawable_pixmap (pDrawable);

    if (uxa_screen->info->finish_access == NULL)
	return;

    if (!uxa_pixmap_is_offscreen (pPixmap))
	return;

    (*uxa_screen->info->finish_access) (pPixmap);
}

/**
 * uxa_validate_gc() sets the ops to UXA's implementations, which may be
 * accelerated or may sync the card and fall back to fb.
 */
static void
uxa_validate_gc (GCPtr pGC, unsigned long changes, DrawablePtr pDrawable)
{
    /* fbValidateGC will do direct access to pixmaps if the tiling has changed.
     * Preempt fbValidateGC by doing its work and masking the change out, so
     * that we can do the Prepare/finish_access.
     */
#ifdef FB_24_32BIT
    if ((changes & GCTile) && fbGetRotatedPixmap(pGC)) {
	(*pGC->pScreen->DestroyPixmap) (fbGetRotatedPixmap(pGC));
	fbGetRotatedPixmap(pGC) = 0;
    }
	
    if (pGC->fillStyle == FillTiled) {
	PixmapPtr	pOldTile, pNewTile;

	pOldTile = pGC->tile.pixmap;
	if (pOldTile->drawable.bitsPerPixel != pDrawable->bitsPerPixel)
	{
	    pNewTile = fbGetRotatedPixmap(pGC);
	    if (!pNewTile ||
		pNewTile ->drawable.bitsPerPixel != pDrawable->bitsPerPixel)
	    {
		if (pNewTile)
		    (*pGC->pScreen->DestroyPixmap) (pNewTile);
		/* fb24_32ReformatTile will do direct access of a newly-
		 * allocated pixmap.  This isn't a problem yet, since we don't
		 * put pixmaps in FB until at least one accelerated UXA op.
		 */
		uxa_prepare_access(&pOldTile->drawable, UXA_ACCESS_RO);
		pNewTile = fb24_32ReformatTile (pOldTile,
						pDrawable->bitsPerPixel);
		uxa_finish_access(&pOldTile->drawable);
	    }
	    if (pNewTile)
	    {
		fbGetRotatedPixmap(pGC) = pOldTile;
		pGC->tile.pixmap = pNewTile;
		changes |= GCTile;
	    }
	}
    }
#endif
    if (changes & GCTile) {
	if (!pGC->tileIsPixel && FbEvenTile (pGC->tile.pixmap->drawable.width *
					     pDrawable->bitsPerPixel))
	{
	    uxa_prepare_access(&pGC->tile.pixmap->drawable, UXA_ACCESS_RW);
	    fbPadPixmap (pGC->tile.pixmap);
	    uxa_finish_access(&pGC->tile.pixmap->drawable);
	}
	/* Mask out the GCTile change notification, now that we've done FB's
	 * job for it.
	 */
	changes &= ~GCTile;
    }

    uxa_prepare_access_gc(pGC);
    fbValidateGC (pGC, changes, pDrawable);
    uxa_finish_access_gc(pGC);

    pGC->ops = (GCOps *) &uxa_ops;
}

static GCFuncs	uxaGCFuncs = {
    uxa_validate_gc,
    miChangeGC,
    miCopyGC,
    miDestroyGC,
    miChangeClip,
    miDestroyClip,
    miCopyClip
};

/**
 * uxa_create_gc makes a new GC and hooks up its funcs handler, so that
 * uxa_validate_gc() will get called.
 */
static int
uxa_create_gc (GCPtr pGC)
{
    if (!fbCreateGC (pGC))
	return FALSE;

    pGC->funcs = &uxaGCFuncs;

    return TRUE;
}

void
uxa_prepare_access_window(WindowPtr pWin)
{
    if (pWin->backgroundState == BackgroundPixmap) 
        uxa_prepare_access(&pWin->background.pixmap->drawable, UXA_ACCESS_RO);

    if (pWin->borderIsPixel == FALSE)
        uxa_prepare_access(&pWin->border.pixmap->drawable, UXA_ACCESS_RO);
}

void
uxa_finish_access_window(WindowPtr pWin)
{
    if (pWin->backgroundState == BackgroundPixmap) 
        uxa_finish_access(&pWin->background.pixmap->drawable);

    if (pWin->borderIsPixel == FALSE)
        uxa_finish_access(&pWin->border.pixmap->drawable);
}

static Bool
uxa_change_window_attributes(WindowPtr pWin, unsigned long mask)
{
    Bool ret;

    uxa_prepare_access_window(pWin);
    ret = fbChangeWindowAttributes(pWin, mask);
    uxa_finish_access_window(pWin);
    return ret;
}

static RegionPtr
uxa_bitmap_to_region(PixmapPtr pPix)
{
  RegionPtr ret;
  uxa_prepare_access(&pPix->drawable, UXA_ACCESS_RO);
  ret = fbPixmapToRegion(pPix);
  uxa_finish_access(&pPix->drawable);
  return ret;
}

static void
uxa_xorg_enable_disable_fb_access (int index, Bool enable)
{
    ScreenPtr screen = screenInfo.screens[index];
    uxa_screen_t *uxa_screen = uxa_get_screen(screen);

    if (!enable && uxa_screen->disableFbCount++ == 0)
	uxa_screen->swappedOut = TRUE;

    if (enable && --uxa_screen->disableFbCount == 0)
	uxa_screen->swappedOut = FALSE;

    if (uxa_screen->SavedEnableDisableFBAccess)
       uxa_screen->SavedEnableDisableFBAccess(index, enable);
}

/**
 * uxa_close_screen() unwraps its wrapped screen functions and tears down UXA's
 * screen private, before calling down to the next CloseSccreen.
 */
static Bool
uxa_close_screen(int i, ScreenPtr pScreen)
{
    uxa_screen_t	*uxa_screen = uxa_get_screen(pScreen);
    ScrnInfoPtr scrn = xf86Screens[pScreen->myNum];
#ifdef RENDER
    PictureScreenPtr	ps = GetPictureScreenIfSet(pScreen);
#endif

    uxa_glyphs_fini(pScreen);

    pScreen->CreateGC = uxa_screen->SavedCreateGC;
    pScreen->CloseScreen = uxa_screen->SavedCloseScreen;
    pScreen->GetImage = uxa_screen->SavedGetImage;
    pScreen->GetSpans = uxa_screen->SavedGetSpans;
    pScreen->CreatePixmap = uxa_screen->SavedCreatePixmap;
    pScreen->DestroyPixmap = uxa_screen->SavedDestroyPixmap;
    pScreen->CopyWindow = uxa_screen->SavedCopyWindow;
    pScreen->ChangeWindowAttributes = uxa_screen->SavedChangeWindowAttributes;
    pScreen->BitmapToRegion = uxa_screen->SavedBitmapToRegion;
    scrn->EnableDisableFBAccess = uxa_screen->SavedEnableDisableFBAccess;
#ifdef RENDER
    if (ps) {
	ps->Composite = uxa_screen->SavedComposite;
	ps->Glyphs = uxa_screen->SavedGlyphs;
	ps->Trapezoids = uxa_screen->SavedTrapezoids;
	ps->AddTraps = uxa_screen->SavedAddTraps;
	ps->Triangles = uxa_screen->SavedTriangles;
    }
#endif

    xfree (uxa_screen);

    return (*pScreen->CloseScreen) (i, pScreen);
}

/**
 * This function allocates a driver structure for UXA drivers to fill in.  By
 * having UXA allocate the structure, the driver structure can be extended
 * without breaking ABI between UXA and the drivers.  The driver's
 * responsibility is to check beforehand that the UXA module has a matching
 * major number and sufficient minor.  Drivers are responsible for freeing the
 * driver structure using xfree().
 *
 * @return a newly allocated, zero-filled driver structure
 */
uxa_driver_t *
uxa_driver_alloc(void)
{
    return xcalloc(1, sizeof(uxa_driver_t));
}

/**
 * @param pScreen screen being initialized
 * @param pScreenInfo UXA driver record
 *
 * uxa_driver_init sets up UXA given a driver record filled in by the driver.
 * pScreenInfo should have been allocated by uxa_driver_alloc().  See the
 * comments in _UxaDriver for what must be filled in and what is optional.
 *
 * @return TRUE if UXA was successfully initialized.
 */
Bool
uxa_driver_init(ScreenPtr screen, uxa_driver_t *uxa_driver)
{
    uxa_screen_t	*uxa_screen;
    ScrnInfoPtr scrn = xf86Screens[screen->myNum];
#ifdef RENDER
    PictureScreenPtr	ps;
#endif

    if (!uxa_driver)
	return FALSE;

    if (uxa_driver->uxa_major != UXA_VERSION_MAJOR ||
	uxa_driver->uxa_minor > UXA_VERSION_MINOR)
    {
	LogMessage(X_ERROR, "UXA(%d): driver's UXA version requirements "
		   "(%d.%d) are incompatible with UXA version (%d.%d)\n",
		   screen->myNum,
		   uxa_driver->uxa_major, uxa_driver->uxa_minor,
		   UXA_VERSION_MAJOR, UXA_VERSION_MINOR);
	return FALSE;
    }

    if (!uxa_driver->prepare_solid) {
	LogMessage(X_ERROR, "UXA(%d): uxa_driver_t::prepare_solid must be "
		   "non-NULL\n", screen->myNum);
	return FALSE;
    }

    if (!uxa_driver->prepare_copy) {
	LogMessage(X_ERROR, "UXA(%d): uxa_driver_t::prepare_copy must be "
		   "non-NULL\n", screen->myNum);
	return FALSE;
    }

#ifdef RENDER
    ps = GetPictureScreenIfSet(screen);
#endif

    uxa_screen = xcalloc (sizeof (uxa_screen_t), 1);

    if (!uxa_screen) {
        LogMessage(X_WARNING, "UXA(%d): Failed to allocate screen private\n",
		   screen->myNum);
	return FALSE;
    }

    uxa_screen->info = uxa_driver;

    dixSetPrivate(&screen->devPrivates, &uxa_screen_index, uxa_screen);

//    exaDDXDriverInit(screen);

    /*
     * Replace various fb screen functions
     */
    uxa_screen->SavedCloseScreen = screen->CloseScreen;
    screen->CloseScreen = uxa_close_screen;

    uxa_screen->SavedCreateGC = screen->CreateGC;
    screen->CreateGC = uxa_create_gc;

    uxa_screen->SavedGetImage = screen->GetImage;
    screen->GetImage = uxa_get_image;

    uxa_screen->SavedGetSpans = screen->GetSpans;
    screen->GetSpans = uxa_check_get_spans;

    uxa_screen->SavedCopyWindow = screen->CopyWindow;
    screen->CopyWindow = uxa_copy_window;

    uxa_screen->SavedChangeWindowAttributes = screen->ChangeWindowAttributes;
    screen->ChangeWindowAttributes = uxa_change_window_attributes;

    uxa_screen->SavedBitmapToRegion = screen->BitmapToRegion;
    screen->BitmapToRegion = uxa_bitmap_to_region;

    uxa_screen->SavedEnableDisableFBAccess = scrn->EnableDisableFBAccess;
    scrn->EnableDisableFBAccess = uxa_xorg_enable_disable_fb_access;

#ifdef RENDER
    if (ps) {
        uxa_screen->SavedComposite = ps->Composite;
	ps->Composite = uxa_composite;

	uxa_screen->SavedGlyphs = ps->Glyphs;
	ps->Glyphs = uxa_glyphs;
	
	uxa_screen->SavedTriangles = ps->Triangles;
	ps->Triangles = uxa_triangles;

	uxa_screen->SavedTrapezoids = ps->Trapezoids;
	ps->Trapezoids = uxa_trapezoids;

	uxa_screen->SavedAddTraps = ps->AddTraps;
	ps->AddTraps = uxa_check_add_traps;
    }
#endif

#ifdef MITSHM
    /* Re-register with the MI funcs, which don't allow shared pixmaps.
     * Shared pixmaps are almost always a performance loss for us, but this
     * still allows for SHM PutImage.
     */
    ShmRegisterFuncs(screen, &uxa_shm_funcs);
#endif

    uxa_glyphs_init(screen);

    LogMessage(X_INFO, "UXA(%d): Driver registered support for the following"
	       " operations:\n", screen->myNum);
    assert(uxa_driver->prepare_solid != NULL);
    LogMessage(X_INFO, "        solid\n");
    assert(uxa_driver->prepare_copy != NULL);
    LogMessage(X_INFO, "        copy\n");
    if (uxa_driver->prepare_composite != NULL) {
	LogMessage(X_INFO, "        composite (RENDER acceleration)\n");
    }
    if (uxa_driver->put_image != NULL) {
	LogMessage(X_INFO, "        put_image\n");
    }
    if (uxa_driver->get_image != NULL) {
	LogMessage(X_INFO, "        get_image\n");
    }

    return TRUE;
}

/**
 * uxa_driver_fini tears down UXA on a given screen.
 *
 * @param pScreen screen being torn down.
 */
void
uxa_driver_fini (ScreenPtr pScreen)
{
    /*right now does nothing*/
}
