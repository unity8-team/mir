/*
 * Copyright © 2011 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including
 * the next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Zhigang Gong <zhigang.gong@linux.intel.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xf86.h>
#define GLAMOR_FOR_XORG  1
#include <glamor.h>

#include "intel.h"
#include "intel_glamor.h"
#include "uxa.h"

Bool
intel_glamor_create_screen_resources(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (!(intel->uxa_flags & UXA_USE_GLAMOR))
		return TRUE;

	if (!glamor_glyphs_init(screen))
		return FALSE;
	if (!glamor_egl_create_textured_screen(screen,
					       intel->front_buffer->handle,
					       intel->front_pitch))
		return FALSE;
	return TRUE;
}

Bool
intel_glamor_pre_init(ScrnInfoPtr scrn)
{
	intel_screen_private *intel;
	intel = intel_get_screen_private(scrn);

	/* Load glamor module */
	if (xf86LoadSubModule(scrn, "glamor_egl") &&
	    glamor_egl_init(scrn, intel->drmSubFD)) {
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "glamor detected, initialising\n");
		intel->uxa_flags |= UXA_USE_GLAMOR;
	} else {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "glamor not available\n");
		intel->uxa_flags &= ~UXA_USE_GLAMOR;
	}

	return TRUE;
}

PixmapPtr
intel_glamor_create_pixmap(ScreenPtr screen, int w, int h,
			   int depth, unsigned int usage)
{
	return glamor_create_pixmap(screen, w, h, depth, usage);
}

Bool
intel_glamor_create_textured_pixmap(PixmapPtr pixmap)
{
	ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct intel_pixmap *priv;

	if ((intel->uxa_flags & UXA_USE_GLAMOR) == 0)
		return TRUE;

	priv = intel_get_pixmap_private(pixmap);
	if (glamor_egl_create_textured_pixmap(pixmap, priv->bo->handle,
					      priv->stride)) {
		priv->pinned = 1;
		return TRUE;
	} else
		return FALSE;
}

void
intel_glamor_destroy_pixmap(PixmapPtr pixmap)
{
	ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
	intel_screen_private * intel;

	intel = intel_get_screen_private(scrn);
	if (intel->uxa_flags & UXA_USE_GLAMOR)
		glamor_egl_destroy_textured_pixmap(pixmap);
}

static void
intel_glamor_need_flush(DrawablePtr pDrawable)
{
	ScrnInfoPtr scrn = xf86Screens[pDrawable->pScreen->myNum];
	intel_screen_private * intel;

	intel = intel_get_screen_private(scrn);
	intel->needs_flush = TRUE;
}

static void
intel_glamor_finish_access(PixmapPtr pixmap, uxa_access_t access)
{
	switch(access) {
	case UXA_ACCESS_RO:
	case UXA_ACCESS_RW:
	case UXA_GLAMOR_ACCESS_RO:
		break;
	case UXA_GLAMOR_ACCESS_RW:
		intel_glamor_need_flush(&pixmap->drawable);
		glamor_block_handler(pixmap->drawable.pScreen);
		break;
	default:
		ErrorF("Invalid access mode %d\n", access);
	}

	return;
}


Bool
intel_glamor_init(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if ((intel->uxa_flags & UXA_USE_GLAMOR) == 0)
		return TRUE;

	if (!glamor_init(screen, GLAMOR_INVERTED_Y_AXIS)) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to initialize glamor\n");
		goto fail;
	}

	if (!glamor_egl_init_textured_pixmap(screen)) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to initialize textured pixmap.\n");
		goto fail;
	}

	intel->uxa_driver->flags |= UXA_USE_GLAMOR;
	intel->uxa_flags = intel->uxa_driver->flags;

	intel->uxa_driver->finish_access = intel_glamor_finish_access;

	xf86DrvMsg(scrn->scrnIndex, X_INFO,
		   "Use GLAMOR acceleration.\n");
	return TRUE;

  fail:
	xf86DrvMsg(scrn->scrnIndex, X_WARNING,
		   "Use standard UXA acceleration.");
	return FALSE;
}

void
intel_glamor_flush(intel_screen_private * intel)
{
	ScreenPtr screen;

	screen = screenInfo.screens[intel->scrn->scrnIndex];
	if (intel->uxa_flags & UXA_USE_GLAMOR)
		glamor_block_handler(screen);
}

Bool
intel_glamor_create_screen_image(ScreenPtr screen, int handle, int stride)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel;

	intel = intel_get_screen_private(scrn);
	if ((intel->uxa_flags & UXA_USE_GLAMOR) == 0)
		return TRUE;

	return glamor_egl_create_textured_screen(screen, handle, stride);
}

Bool
intel_glamor_close_screen(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private * intel;

	intel = intel_get_screen_private(scrn);
	if (intel && (intel->uxa_flags & UXA_USE_GLAMOR))
		return glamor_egl_close_screen(screen);
	return TRUE;
}

void
intel_glamor_free_screen(int scrnIndex, int flags)
{
	ScrnInfoPtr scrn = xf86Screens[scrnIndex];
	intel_screen_private * intel;

	intel = intel_get_screen_private(scrn);
	if (intel && (intel->uxa_flags & UXA_USE_GLAMOR))
		glamor_egl_free_screen(scrnIndex, GLAMOR_EGL_EXTERNAL_BUFFER);
}
