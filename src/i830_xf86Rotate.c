/*
 * Copyright © 2006 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "xf86.h"
#include "xf86DDC.h"
/*#include "i830.h" */
#include "i830_xf86Crtc.h"
#include "i830_xf86Modes.h"
#include "i830_randr.h"
#include "X11/extensions/render.h"
#define DPMS_SERVER
#include "X11/extensions/dpms.h"
#include "X11/Xatom.h"

static int
mode_height (DisplayModePtr mode, Rotation rotation)
{
    switch (rotation & 0xf) {
    case RR_Rotate_0:
    case RR_Rotate_180:
	return mode->VDisplay;
    case RR_Rotate_90:
    case RR_Rotate_270:
	return mode->HDisplay;
    default:
	return 0;
    }
}

static int
mode_width (DisplayModePtr mode, Rotation rotation)
{
    switch (rotation & 0xf) {
    case RR_Rotate_0:
    case RR_Rotate_180:
	return mode->HDisplay;
    case RR_Rotate_90:
    case RR_Rotate_270:
	return mode->VDisplay;
    default:
	return 0;
    }
}

/* borrowed from composite extension, move to Render and publish? */

static VisualPtr
compGetWindowVisual (WindowPtr pWin)
{
    ScreenPtr	    pScreen = pWin->drawable.pScreen;
    VisualID	    vid = wVisual (pWin);
    int		    i;

    for (i = 0; i < pScreen->numVisuals; i++)
	if (pScreen->visuals[i].vid == vid)
	    return &pScreen->visuals[i];
    return 0;
}

static PictFormatPtr
compWindowFormat (WindowPtr pWin)
{
    ScreenPtr	pScreen = pWin->drawable.pScreen;
    
    return PictureMatchVisual (pScreen, pWin->drawable.depth,
			       compGetWindowVisual (pWin));
}

static void
xf86RotateCrtcRedisplay (xf86CrtcPtr crtc, RegionPtr region)
{
    ScrnInfoPtr		scrn = crtc->scrn;
    ScreenPtr		screen = scrn->pScreen;
    PixmapPtr		src_pixmap = (*screen->GetScreenPixmap) (screen);
    PixmapPtr		dst_pixmap = crtc->rotatedPixmap;
    PictFormatPtr	format = compWindowFormat (WindowTable[screen->myNum]);
    int			error;
    PicturePtr		src, dst;
    PictTransform	transform;
    
    src = CreatePicture (None,
			 &src_pixmap->drawable,
			 format,
			 0L,
			 NULL,
			 serverClient,
			 &error);
    if (!src)
	return;
    dst = CreatePicture (None,
			 &dst_pixmap->drawable,
			 format,
			 0L,
			 NULL,
			 serverClient,
			 &error);
    if (!dst)
	return;
    SetPictureClipRegion (src, 0, 0, region);
    memset (&transform, '\0', sizeof (transform));
    transform.matrix[2][2] = 1;
    transform.matrix[0][2] = crtc->x;
    transform.matrix[1][2] = crtc->y;
    switch (crtc->curRotation & 0xf) {
    case RR_Rotate_0:
	transform.matrix[0][0] = 1;
	transform.matrix[1][1] = 1;
	break;
    case RR_Rotate_90:
	/* XXX probably wrong */
	transform.matrix[0][1] = 1;
	transform.matrix[1][0] = -1;
	transform.matrix[1][2] += crtc->curMode.HDisplay;
	break;
    case RR_Rotate_180:
	/* XXX probably wrong */
	transform.matrix[0][0] = -1;
	transform.matrix[1][1] = -1;
	transform.matrix[0][2] += crtc->curMode.HDisplay;
	transform.matrix[1][2] += crtc->curMode.VDisplay;
	break;
    case RR_Rotate_270:
	/* XXX probably wrong */
	transform.matrix[0][1] = -1;
	transform.matrix[1][0] = 1;
	transform.matrix[0][2] += crtc->curMode.VDisplay;
	break;
    }
    /* handle reflection */
    if (crtc->curRotation & RR_Reflect_X)
    {
	/* XXX figure this out */
    }
    if (crtc->curRotation & RR_Reflect_Y)
    {
	/* XXX figure this out too */
    }
    SetPictureTransform (src, &transform);
    CompositePicture (PictOpSrc,
		      src, NULL, dst,
		      0, 0, 0, 0, 0, 0,
		      dst_pixmap->drawable.width,
		      dst_pixmap->drawable.height);
    FreePicture (src, None);
    FreePicture (dst, None);
}

static void
xf86RotateRedisplay(ScreenPtr pScreen)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    DamagePtr		damage = xf86_config->rotationDamage;
    RegionPtr		region;

    if (!damage)
	return;
    region = DamageRegion(damage);
    if (REGION_NOTEMPTY(pScreen, region)) 
    {
	int		    c;
	
	for (c = 0; c < xf86_config->num_crtc; c++)
	{
	    xf86CrtcPtr	    crtc = xf86_config->crtc[c];

	    if (crtc->curRotation != RR_Rotate_0)
	    {
		BoxRec	    box;
		RegionRec   crtc_damage;

		/* compute portion of damage that overlaps crtc */
		box.x1 = crtc->x;
		box.x2 = crtc->x + mode_width (&crtc->curMode, crtc->curRotation);
		box.y1 = crtc->y;
		box.y2 = crtc->y + mode_height (&crtc->curMode, crtc->curRotation);
		REGION_INIT(pScreen, &crtc_damage, &box, 1);
		REGION_INTERSECT (pScreen, &crtc_damage, &crtc_damage, region);
		
		/* update damaged region */
		if (REGION_NOTEMPTY(pScreen, &crtc_damage))
		    xf86RotateCrtcRedisplay (crtc, &crtc_damage);
		
		REGION_UNINIT (pScreen, &crtc_damage);
	    }
	}
	DamageEmpty(damage);
    }
}

static void
xf86RotateBlockHandler(pointer data, OSTimePtr pTimeout, pointer pRead)
{
    ScreenPtr pScreen = (ScreenPtr) data;

    xf86RotateRedisplay(pScreen);
}

static void
xf86RotateWakeupHandler(pointer data, int i, pointer LastSelectMask)
{
}

Bool
xf86CrtcRotate (xf86CrtcPtr crtc, DisplayModePtr mode, Rotation rotation)
{
    ScrnInfoPtr		pScrn = crtc->scrn;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    ScreenPtr		pScreen = pScrn->pScreen;
    
    if (rotation == RR_Rotate_0)
    {
	/* Free memory from rotation */
	if (crtc->rotatedPixmap)
	{
	    crtc->funcs->shadow_destroy (crtc, crtc->rotatedPixmap);
	    crtc->rotatedPixmap = NULL;
	}

	if (xf86_config->rotationDamage)
	{
	    /* Free damage structure */
	    DamageDestroy (xf86_config->rotationDamage);
	    xf86_config->rotationDamage = NULL;
	    /* Free block/wakeup handler */
	    RemoveBlockAndWakeupHandlers (xf86RotateBlockHandler,
					  xf86RotateWakeupHandler,
					  (pointer) pScreen);
	}
    }
    else
    {
	int	    width = mode_width (mode, rotation);
	int	    height = mode_height (mode, rotation);
	PixmapPtr   shadow = crtc->rotatedPixmap;
	int	    old_width = shadow ? shadow->drawable.width : 0;
	int	    old_height = shadow ? shadow->drawable.height : 0;
	
	/* Allocate memory for rotation */
	if (old_width != width || old_height != height)
	{
	    if (shadow)
	    {
		crtc->funcs->shadow_destroy (crtc, shadow);
		crtc->rotatedPixmap = NULL;
	    }
	    shadow = crtc->funcs->shadow_create (crtc, width, height);
	    if (!shadow)
		goto bail1;
	}
	
	if (!xf86_config->rotationDamage)
	{
	    /* Create damage structure */
	    xf86_config->rotationDamage = DamageCreate (NULL, NULL,
						DamageReportNone,
						TRUE, pScreen, pScreen);
	    if (!xf86_config->rotationDamage)
		goto bail2;
	    
	    /* Hook damage to screen pixmap */
	    DamageRegister (&(*pScreen->GetScreenPixmap)(pScreen)->drawable,
			    xf86_config->rotationDamage);
	    
	    /* Assign block/wakeup handler */
	    if (!RegisterBlockAndWakeupHandlers (xf86RotateBlockHandler,
						 xf86RotateWakeupHandler,
						 (pointer) pScreen))
	    {
		goto bail3;
	    }
	}
	if (0)
	{
bail3:
	    DamageDestroy (xf86_config->rotationDamage);
	    xf86_config->rotationDamage = NULL;
	    
bail2:
	    if (shadow)
	    {
		crtc->funcs->shadow_destroy (crtc, shadow);
		crtc->rotatedPixmap = NULL;
	    }
bail1:
	    if (old_width && old_height)
		crtc->rotatedPixmap = crtc->funcs->shadow_create (crtc,
								  old_width,
								  old_height);
	    return FALSE;
	}
    }
    
    /* All done */
    crtc->curRotation = rotation;
    return TRUE;
}
