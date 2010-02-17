/**************************************************************************

Copyright 2001 VA Linux Systems Inc., Fremont, California.
Copyright © 2002 by David Dawes

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
ATI, VA LINUX SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors: Jeff Hartmann <jhartmann@valinux.com>
 *          David Dawes <dawes@xfree86.org>
 *          Keith Whitwell <keith@tungstengraphics.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Priv.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "windowstr.h"
#include "shadow.h"

#include "GL/glxtokens.h"

#include "i830.h"
#include "i830_dri.h"

#include "i915_drm.h"

#include "dri2.h"

#ifdef DRI2
#if DRI2INFOREC_VERSION >= 1
#define USE_DRI2_1_1_0
#endif

extern XF86ModuleData dri2ModuleData;
#endif

#ifndef USE_DRI2_1_1_0
static DRI2BufferPtr
I830DRI2CreateBuffers(DrawablePtr drawable, unsigned int *attachments,
		      int count)
{
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	DRI2BufferPtr buffers;
	dri_bo *bo;
	int i;
	I830DRI2BufferPrivatePtr privates;
	PixmapPtr pixmap, pDepthPixmap;

	buffers = xcalloc(count, sizeof *buffers);
	if (buffers == NULL)
		return NULL;
	privates = xcalloc(count, sizeof *privates);
	if (privates == NULL) {
		xfree(buffers);
		return NULL;
	}

	pDepthPixmap = NULL;
	for (i = 0; i < count; i++) {
		if (attachments[i] == DRI2BufferFrontLeft) {
			pixmap = get_drawable_pixmap(drawable);
			pixmap->refcnt++;
		} else if (attachments[i] == DRI2BufferStencil && pDepthPixmap) {
			pixmap = pDepthPixmap;
			pixmap->refcnt++;
		} else {
			unsigned int hint = 0;

			switch (attachments[i]) {
			case DRI2BufferDepth:
				if (SUPPORTS_YTILING(intel))
					hint = INTEL_CREATE_PIXMAP_TILING_Y;
				else
					hint = INTEL_CREATE_PIXMAP_TILING_X;
				break;
			case DRI2BufferFakeFrontLeft:
			case DRI2BufferFakeFrontRight:
			case DRI2BufferBackLeft:
			case DRI2BufferBackRight:
				hint = INTEL_CREATE_PIXMAP_TILING_X;
				break;
			}

			if (!intel->tiling)
				hint = 0;

			pixmap = screen->CreatePixmap(screen,
						      drawable->width,
						      drawable->height,
						      drawable->depth,
						      hint);

		}

		if (attachments[i] == DRI2BufferDepth)
			pDepthPixmap = pixmap;

		buffers[i].attachment = attachments[i];
		buffers[i].pitch = pixmap->devKind;
		buffers[i].cpp = pixmap->drawable.bitsPerPixel / 8;
		buffers[i].driverPrivate = &privates[i];
		buffers[i].flags = 0;	/* not tiled */
		privates[i].pixmap = pixmap;
		privates[i].attachment = attachments[i];

		bo = i830_get_pixmap_bo(pixmap);
		if (dri_bo_flink(bo, &buffers[i].name) != 0) {
			/* failed to name buffer */
		}

	}

	return buffers;
}

#else

static DRI2Buffer2Ptr
I830DRI2CreateBuffer(DrawablePtr drawable, unsigned int attachment,
		     unsigned int format)
{
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	DRI2Buffer2Ptr buffer;
	dri_bo *bo;
	I830DRI2BufferPrivatePtr privates;
	PixmapPtr pixmap;

	buffer = xcalloc(1, sizeof *buffer);
	if (buffer == NULL)
		return NULL;
	privates = xcalloc(1, sizeof *privates);
	if (privates == NULL) {
		xfree(buffer);
		return NULL;
	}

	if (attachment == DRI2BufferFrontLeft) {
		pixmap = get_drawable_pixmap(drawable);
		pixmap->refcnt++;
	} else {
		unsigned int hint = 0;

		switch (attachment) {
		case DRI2BufferDepth:
		case DRI2BufferDepthStencil:
			if (SUPPORTS_YTILING(intel))
				hint = INTEL_CREATE_PIXMAP_TILING_Y;
			else
				hint = INTEL_CREATE_PIXMAP_TILING_X;
			break;
		case DRI2BufferFakeFrontLeft:
		case DRI2BufferFakeFrontRight:
		case DRI2BufferBackLeft:
		case DRI2BufferBackRight:
			hint = INTEL_CREATE_PIXMAP_TILING_X;
			break;
		}

		if (!intel->tiling)
			hint = 0;

		pixmap = screen->CreatePixmap(screen,
					      drawable->width,
					      drawable->height,
					      (format != 0) ? format :
							      drawable->depth,
					      hint);

	}

	buffer->attachment = attachment;
	buffer->pitch = pixmap->devKind;
	buffer->cpp = pixmap->drawable.bitsPerPixel / 8;
	buffer->driverPrivate = privates;
	buffer->format = format;
	buffer->flags = 0;	/* not tiled */
	privates->pixmap = pixmap;
	privates->attachment = attachment;

	bo = i830_get_pixmap_bo(pixmap);
	if (dri_bo_flink(bo, &buffer->name) != 0) {
		/* failed to name buffer */
	}

	return buffer;
}

#endif

#ifndef USE_DRI2_1_1_0

static void
I830DRI2DestroyBuffers(DrawablePtr drawable, DRI2BufferPtr buffers, int count)
{
	ScreenPtr screen = drawable->pScreen;
	I830DRI2BufferPrivatePtr private;
	int i;

	for (i = 0; i < count; i++) {
		private = buffers[i].driverPrivate;
		screen->DestroyPixmap(private->pixmap);
	}

	if (buffers) {
		xfree(buffers[0].driverPrivate);
		xfree(buffers);
	}
}

#else

static void I830DRI2DestroyBuffer(DrawablePtr drawable, DRI2Buffer2Ptr buffer)
{
	if (buffer) {
		I830DRI2BufferPrivatePtr private = buffer->driverPrivate;
		ScreenPtr screen = drawable->pScreen;

		screen->DestroyPixmap(private->pixmap);

		xfree(private);
		xfree(buffer);
	}
}

#endif

static void
I830DRI2CopyRegion(DrawablePtr drawable, RegionPtr pRegion,
		   DRI2BufferPtr destBuffer, DRI2BufferPtr sourceBuffer)
{
	I830DRI2BufferPrivatePtr srcPrivate = sourceBuffer->driverPrivate;
	I830DRI2BufferPrivatePtr dstPrivate = destBuffer->driverPrivate;
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	DrawablePtr src = (srcPrivate->attachment == DRI2BufferFrontLeft)
	    ? drawable : &srcPrivate->pixmap->drawable;
	DrawablePtr dst = (dstPrivate->attachment == DRI2BufferFrontLeft)
	    ? drawable : &dstPrivate->pixmap->drawable;
	RegionPtr pCopyClip;
	GCPtr gc;

	gc = GetScratchGC(drawable->depth, screen);
	pCopyClip = REGION_CREATE(screen, NULL, 0);
	REGION_COPY(screen, pCopyClip, pRegion);
	(*gc->funcs->ChangeClip) (gc, CT_REGION, pCopyClip, 0);
	ValidateGC(dst, gc);

	/* Wait for the scanline to be outside the region to be copied */
	if (pixmap_is_scanout(get_drawable_pixmap(dst))
	    && intel->swapbuffers_wait) {
		BoxPtr box;
		BoxRec crtcbox;
		int y1, y2;
		int pipe = -1, event, load_scan_lines_pipe;
		xf86CrtcPtr crtc;
		Bool full_height = FALSE;

		box = REGION_EXTENTS(unused, gc->pCompositeClip);
		crtc = i830_covering_crtc(scrn, box, NULL, &crtcbox);

		/*
		 * Make sure the CRTC is valid and this is the real front
		 * buffer
		 */
		if (crtc != NULL && !crtc->rotatedData) {
			pipe = i830_crtc_to_pipe(crtc);

			/*
			 * Make sure we don't wait for a scanline that will
			 * never occur
			 */
			y1 = (crtcbox.y1 <= box->y1) ? box->y1 - crtcbox.y1 : 0;
			y2 = (box->y2 <= crtcbox.y2) ?
			    box->y2 - crtcbox.y1 : crtcbox.y2 - crtcbox.y1;

			if (y1 == 0 && y2 == (crtcbox.y2 - crtcbox.y1))
			    full_height = TRUE;

			/*
			 * Pre-965 doesn't have SVBLANK, so we need a bit
			 * of extra time for the blitter to start up and
			 * do its job for a full height blit
			 */
			if (full_height && !IS_I965G(intel))
			    y2 -= 2;

			if (pipe == 0) {
				event = MI_WAIT_FOR_PIPEA_SCAN_LINE_WINDOW;
				load_scan_lines_pipe =
				    MI_LOAD_SCAN_LINES_DISPLAY_PIPEA;
				if (full_height && IS_I965G(intel))
				    event = MI_WAIT_FOR_PIPEA_SVBLANK;
			} else {
				event = MI_WAIT_FOR_PIPEB_SCAN_LINE_WINDOW;
				load_scan_lines_pipe =
				    MI_LOAD_SCAN_LINES_DISPLAY_PIPEB;
				if (full_height && IS_I965G(intel))
				    event = MI_WAIT_FOR_PIPEB_SVBLANK;
			}

			BEGIN_BATCH(5);
			/*
			 * The documentation says that the LOAD_SCAN_LINES
			 * command always comes in pairs. Don't ask me why.
			 */
			OUT_BATCH(MI_LOAD_SCAN_LINES_INCL |
				  load_scan_lines_pipe);
			OUT_BATCH((y1 << 16) | y2);
			OUT_BATCH(MI_LOAD_SCAN_LINES_INCL |
				  load_scan_lines_pipe);
			OUT_BATCH((y1 << 16) | y2);
			OUT_BATCH(MI_WAIT_FOR_EVENT | event);
			ADVANCE_BATCH();
		}
	}

	(*gc->ops->CopyArea) (src, dst,
			       gc,
			       0, 0,
			       drawable->width, drawable->height,
			       0, 0);
	FreeScratchGC(gc);

	/* Emit a flush of the rendering cache, or on the 965 and beyond
	 * rendering results may not hit the framebuffer until significantly
	 * later.
	 *
	 * We can't rely on getting into the block handler before the DRI
	 * client gets to run again so flush now. */
	intel_batch_submit(scrn);
#if ALWAYS_SYNC
	intel_sync(scrn);
#endif
	drmCommandNone(intel->drmSubFD, DRM_I915_GEM_THROTTLE);

}

#if DRI2INFOREC_VERSION >= 4

enum DRI2FrameEventType {
    DRI2_SWAP,
    DRI2_FLIP,
    DRI2_WAITMSC,
};

typedef struct _DRI2FrameEvent {
    DrawablePtr		pDraw;
    ClientPtr		client;
    enum DRI2FrameEventType type;
    int			frame;

    /* for swaps & flips only */
    DRI2SwapEventPtr	event_complete;
    void		*event_data;
    DRI2BufferPtr	front;
    DRI2BufferPtr	back;
} DRI2FrameEventRec, *DRI2FrameEventPtr;

static int
I830DRI2DrawablePipe(DrawablePtr pDraw)
{
    ScreenPtr pScreen = pDraw->pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    BoxRec box, crtcbox;
    xf86CrtcPtr crtc;
    int pipe = -1;

    box.x1 = pDraw->x;
    box.y1 = pDraw->y;
    box.x2 = box.x1 + pDraw->width;
    box.y2 = box.y1 + pDraw->height;

    crtc = i830_covering_crtc(pScrn, &box, NULL, &crtcbox);

    /* Make sure the CRTC is valid and this is the real front buffer */
    if (crtc != NULL && !crtc->rotatedData)
	pipe = i830_crtc_to_pipe(crtc);

    return pipe;
}

static void
I830DRI2ExchangeBuffers(DrawablePtr draw, DRI2BufferPtr front,
			DRI2BufferPtr back)
{
	I830DRI2BufferPrivatePtr front_priv, back_priv;
	dri_bo *tmp_bo;
	int tmp;

	front_priv = front->driverPrivate;
	back_priv = back->driverPrivate;

	/* Swap BO names so DRI works */
	tmp = front->name;
	front->name = back->name;
	back->name = tmp;

	/* Swap pixmap bos */
	dri_bo_reference(i830_get_pixmap_bo(front_priv->pixmap));

	tmp_bo = i830_get_pixmap_bo(front_priv->pixmap);
	i830_set_pixmap_bo(front_priv->pixmap,
			   i830_get_pixmap_bo(back_priv->pixmap));
	i830_set_pixmap_bo(back_priv->pixmap, tmp_bo); /* should be screen */
}

/*
 * Our internal swap routine takes care of actually exchanging, blitting, or
 * flipping buffers as necessary.
 */
static Bool
I830DRI2ScheduleFlip(ClientPtr client, DrawablePtr draw, DRI2BufferPtr front,
		     DRI2BufferPtr back, DRI2SwapEventPtr func, void *data)
{
	ScreenPtr screen = draw->pScreen;
	I830DRI2BufferPrivatePtr front_priv, back_priv;
	dri_bo *tmp_bo;
	DRI2FrameEventPtr flip_info;
	Bool ret;

	flip_info = xcalloc(1, sizeof(DRI2FrameEventRec));
	if (!flip_info)
	    return FALSE;

	flip_info->pDraw = draw;
	flip_info->client = client;
	flip_info->type = DRI2_SWAP;
	flip_info->event_complete = func;
	flip_info->event_data = data;

	front_priv = front->driverPrivate;
	back_priv = back->driverPrivate;
	tmp_bo = i830_get_pixmap_bo(front_priv->pixmap);

	I830DRI2ExchangeBuffers(draw, front, back);

	/* Page flip the full screen buffer */
	ret = drmmode_do_pageflip(screen,
				  i830_get_pixmap_bo(front_priv->pixmap),
				  i830_get_pixmap_bo(back_priv->pixmap),
				  flip_info);

	/* Unwind in case of failure */
	if (!ret) {
	    i830_set_pixmap_bo(back_priv->pixmap,
			       i830_get_pixmap_bo(front_priv->pixmap));
	    i830_set_pixmap_bo(front_priv->pixmap, tmp_bo);
	    return FALSE;
	}

	return ret;
}

void I830DRI2FrameEventHandler(unsigned int frame, unsigned int tv_sec,
			       unsigned int tv_usec, void *event_data)
{
    DRI2FrameEventPtr event = event_data;
    DrawablePtr pDraw = event->pDraw;
    ScreenPtr screen = pDraw->pScreen;
    ScrnInfoPtr scrn = xf86Screens[screen->myNum];
    intel_screen_private *intel = intel_get_screen_private(scrn);

    switch (event->type) {
    case DRI2_FLIP:
	/* If we can still flip... */
	if (DRI2CanFlip(pDraw) && !intel->shadow_present &&
	    intel->use_pageflipping &&
	    I830DRI2ScheduleFlip(event->client, pDraw, event->front,
				 event->back, event->event_complete,
				 event->event_data)) {
	    break;
	}
	/* else fall through to exchange/blit */
    case DRI2_SWAP: {
	int swap_type;

	if (DRI2CanExchange(pDraw)) {
	    I830DRI2ExchangeBuffers(pDraw, event->front, event->back);
	    swap_type = DRI2_EXCHANGE_COMPLETE;
	} else {
	    BoxRec	    box;
	    RegionRec	    region;

	    box.x1 = 0;
	    box.y1 = 0;
	    box.x2 = pDraw->width;
	    box.y2 = pDraw->height;
	    REGION_INIT(pScreen, &region, &box, 0);

	    I830DRI2CopyRegion(pDraw, &region, event->front, event->back);
	    swap_type = DRI2_BLIT_COMPLETE;
	}
	DRI2SwapComplete(event->client, pDraw, frame, tv_sec, tv_usec,
			 swap_type, event->event_complete, event->event_data);
	break;
    }
    case DRI2_WAITMSC:
	DRI2WaitMSCComplete(event->client, pDraw, frame, tv_sec, tv_usec);
	break;
    default:
	xf86DrvMsg(scrn->scrnIndex, X_WARNING,
		   "%s: unknown vblank event received\n", __func__);
	/* Unknown type */
	break;
    }

    xfree(event);
}

void I830DRI2FlipEventHandler(unsigned int frame, unsigned int tv_sec,
			      unsigned int tv_usec, void *event_data)
{
    DRI2FrameEventPtr flip = event_data;
    DrawablePtr pDraw = flip->pDraw;
    ScreenPtr screen = pDraw->pScreen;
    ScrnInfoPtr scrn = xf86Screens[screen->myNum];

    /* We assume our flips arrive in order, so we don't check the frame */
    switch (flip->type) {
    case DRI2_SWAP:
	DRI2SwapComplete(flip->client, pDraw, frame, tv_sec, tv_usec,
			 DRI2_FLIP_COMPLETE, flip->event_complete,
			 flip->event_data);
	break;
    default:
	xf86DrvMsg(scrn->scrnIndex, X_WARNING,
		   "%s: unknown vblank event received\n", __func__);
	/* Unknown type */
	break;
    }

    xfree(flip);
}

/*
 * ScheduleSwap is responsible for requesting a DRM vblank event for the
 * appropriate frame.
 *
 * In the case of a blit (e.g. for a windowed swap) or buffer exchange,
 * the vblank requested can simply be the last queued swap frame + the swap
 * interval for the drawable.
 *
 * In the case of a page flip, we request an event for the last queued swap
 * frame + swap interval - 1, since we'll need to queue the flip for the frame
 * immediately following the received event.
 *
 * The client will be blocked if it tries to perform further GL commands
 * after queueing a swap, though in the Intel case after queueing a flip, the
 * client is free to queue more commands; they'll block in the kernel if
 * they access buffers busy with the flip.
 *
 * When the swap is complete, the driver should call into the server so it
 * can send any swap complete events that have been requested.
 */
static int
I830DRI2ScheduleSwap(ClientPtr client, DrawablePtr draw, DRI2BufferPtr front,
		     DRI2BufferPtr back, CARD64 *target_msc, CARD64 divisor,
		     CARD64 remainder, DRI2SwapEventPtr func, void *data)
{
	ScreenPtr screen = draw->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	drmVBlank vbl;
	int ret, pipe = I830DRI2DrawablePipe(draw), flip = 0;
	DRI2FrameEventPtr swap_info;
	enum DRI2FrameEventType swap_type = DRI2_SWAP;

	swap_info = xcalloc(1, sizeof(DRI2FrameEventRec));

	/* Drawable not displayed... just complete the swap */
	if (pipe == -1 || !swap_info) {
	    BoxRec	    box;
	    RegionRec	    region;

	    box.x1 = 0;
	    box.y1 = 0;
	    box.x2 = draw->width;
	    box.y2 = draw->height;
	    REGION_INIT(pScreen, &region, &box, 0);

	    I830DRI2CopyRegion(draw, &region, front, back);

	    DRI2SwapComplete(client, draw, 0, 0, 0, DRI2_BLIT_COMPLETE, func,
			     data);
	    if (swap_info)
		xfree(swap_info);
	    return TRUE;
	}

	swap_info->pDraw = draw;
	swap_info->client = client;
	swap_info->event_complete = func;
	swap_info->event_data = data;
	swap_info->front = front;
	swap_info->back = back;

	/* Get current count */
	vbl.request.type = DRM_VBLANK_RELATIVE;
	if (pipe > 0)
		vbl.request.type |= DRM_VBLANK_SECONDARY;
	vbl.request.sequence = 0;
	ret = drmWaitVBlank(intel->drmSubFD, &vbl);
	if (ret) {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "first get vblank counter failed: %s\n",
			   strerror(errno));
		return FALSE;
	}

	/* Flips need to be submitted one frame before */
	if (DRI2CanFlip(draw) && !intel->shadow_present &&
	    intel->use_pageflipping) {
	    swap_type = DRI2_FLIP;
	    flip = 1;
	}

	swap_info->type = swap_type;

	/*
	 * If divisor is zero, we just need to make sure target_msc passes
	 * before waking up the client.
	 */
	if (divisor == 0) {
		vbl.request.type = DRM_VBLANK_NEXTONMISS |
		    DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
		if (pipe > 0)
			vbl.request.type |= DRM_VBLANK_SECONDARY;

		vbl.request.sequence = *target_msc;
		vbl.request.sequence -= flip;
		vbl.request.signal = (unsigned long)swap_info;
		ret = drmWaitVBlank(intel->drmSubFD, &vbl);
		if (ret) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "divisor 0 get vblank counter failed: %s\n",
				   strerror(errno));
			return FALSE;
		}

		*target_msc = vbl.reply.sequence;
		swap_info->frame = *target_msc;

		return TRUE;
	}

	/*
	 * If we get here, target_msc has already passed or we don't have one,
	 * so we queue an event that will satisfy the divisor/remainderequation.
	 */
	if ((vbl.reply.sequence % divisor) == remainder) {
	    BoxRec	    box;
	    RegionRec	    region;

	    box.x1 = 0;
	    box.y1 = 0;
	    box.x2 = draw->width;
	    box.y2 = draw->height;
	    REGION_INIT(pScreen, &region, &box, 0);

	    I830DRI2CopyRegion(draw, &region, front, back);

	    DRI2SwapComplete(client, draw, 0, 0, 0, DRI2_BLIT_COMPLETE, func,
			     data);
	    if (swap_info)
		xfree(swap_info);
	    return TRUE;
	}

	vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
	if (pipe > 0)
		vbl.request.type |= DRM_VBLANK_SECONDARY;

	/*
	 * If we have no remainder, and the test above failed, it means we've
	 * passed the last point where seq % divisor == remainder, so we need
	 * to wait for the next time that will happen.
	 */
	if (!remainder)
		vbl.request.sequence += divisor;

	vbl.request.sequence = vbl.reply.sequence -
	    (vbl.reply.sequence % divisor) + remainder;
	vbl.request.sequence -= flip;
	vbl.request.signal = (unsigned long)swap_info;
	ret = drmWaitVBlank(intel->drmSubFD, &vbl);
	if (ret) {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "final get vblank counter failed: %s\n",
			   strerror(errno));
		return FALSE;
	}

	*target_msc = vbl.reply.sequence;
	swap_info->frame = *target_msc;

	return TRUE;
}

/*
 * Get current frame count and frame count timestamp, based on drawable's
 * crtc.
 */
static int
I830DRI2GetMSC(DrawablePtr draw, CARD64 *ust, CARD64 *msc)
{
    ScreenPtr screen = draw->pScreen;
    ScrnInfoPtr scrn = xf86Screens[screen->myNum];
    intel_screen_private *intel = intel_get_screen_private(scrn);
    drmVBlank vbl;
    int ret, pipe = I830DRI2DrawablePipe(draw);

    /* Drawable not displayed, make up a value */
    if (pipe == -1) {
	*ust = 0;
	*msc = 0;
	return TRUE;
    }

    vbl.request.type = DRM_VBLANK_RELATIVE;
    if (pipe > 0)
	vbl.request.type |= DRM_VBLANK_SECONDARY;
    vbl.request.sequence = 0;

    ret = drmWaitVBlank(intel->drmSubFD, &vbl);
    if (ret) {
	xf86DrvMsg(scrn->scrnIndex, X_WARNING,
		   "get vblank counter failed: %s\n", strerror(errno));
	return FALSE;
    }

    *ust = ((CARD64)vbl.reply.tval_sec * 1000000) + vbl.reply.tval_usec;
    *msc = vbl.reply.sequence;

    return TRUE;
}

/*
 * Request a DRM event when the requested conditions will be satisfied.
 *
 * We need to handle the event and ask the server to wake up the client when
 * we receive it.
 */
static int
I830DRI2ScheduleWaitMSC(ClientPtr client, DrawablePtr draw, CARD64 target_msc,
			CARD64 divisor, CARD64 remainder)
{
    ScreenPtr screen = draw->pScreen;
    ScrnInfoPtr scrn = xf86Screens[screen->myNum];
    intel_screen_private *intel = intel_get_screen_private(scrn);
    DRI2FrameEventPtr wait_info;
    drmVBlank vbl;
    int ret, pipe = I830DRI2DrawablePipe(draw);

    /* Drawable not visible, return immediately */
    if (pipe == -1) {
	DRI2WaitMSCComplete(client, draw, target_msc, 0, 0);
	return TRUE;
    }

    wait_info = xcalloc(1, sizeof(DRI2FrameEventRec));
    if (!wait_info) {
	DRI2WaitMSCComplete(client, draw, 0, 0, 0);
	return TRUE;
    }

    wait_info->pDraw = draw;
    wait_info->client = client;
    wait_info->type = DRI2_WAITMSC;

    /* Get current count */
    vbl.request.type = DRM_VBLANK_RELATIVE;
    if (pipe > 0)
	vbl.request.type |= DRM_VBLANK_SECONDARY;
    vbl.request.sequence = 0;
    ret = drmWaitVBlank(intel->drmSubFD, &vbl);
    if (ret) {
	xf86DrvMsg(scrn->scrnIndex, X_WARNING,
		   "get vblank counter failed: %s\n", strerror(errno));
	return FALSE;
    }

    /*
     * If divisor is zero, we just need to make sure target_msc passes
     * before waking up the client.
     */
    if (divisor == 0) {
	vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
	if (pipe > 0)
	    vbl.request.type |= DRM_VBLANK_SECONDARY;
	vbl.request.sequence = target_msc;
	vbl.request.signal = (unsigned long)wait_info;
	ret = drmWaitVBlank(intel->drmSubFD, &vbl);
	if (ret) {
	    xf86DrvMsg(scrn->scrnIndex, X_WARNING,
		       "get vblank counter failed: %s\n", strerror(errno));
	    return FALSE;
	}

	wait_info->frame = vbl.reply.sequence;
	DRI2BlockClient(client, draw);
	return TRUE;
    }

    /*
     * If we get here, target_msc has already passed or we don't have one,
     * so we queue an event that will satisfy the divisor/remainder equation.
     */
    vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
    if (pipe > 0)
	vbl.request.type |= DRM_VBLANK_SECONDARY;

    /*
     * If we have no remainder and the condition isn't satisified, it means
     * we've passed the last point where seq % divisor == remainder, so we need
     * to wait for the next time that will happen.
     */
    if (((vbl.reply.sequence % divisor) != remainder) && !remainder)
	vbl.request.sequence += divisor;

    vbl.request.sequence = vbl.reply.sequence - (vbl.reply.sequence % divisor) +
	remainder;
    vbl.request.signal = (unsigned long)wait_info;
    ret = drmWaitVBlank(intel->drmSubFD, &vbl);
    if (ret) {
	xf86DrvMsg(scrn->scrnIndex, X_WARNING,
		   "get vblank counter failed: %s\n", strerror(errno));
	return FALSE;
    }

    wait_info->frame = vbl.reply.sequence;
    DRI2BlockClient(client, draw);

    return TRUE;
}
#endif

Bool I830DRI2ScreenInit(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	DRI2InfoRec info;
#ifdef USE_DRI2_1_1_0
	int dri2_major = 1;
	int dri2_minor = 0;
#endif
#if DRI2INFOREC_VERSION >= 4
	const char *driverNames[1];
#endif

#ifdef USE_DRI2_1_1_0
	if (xf86LoaderCheckSymbol("DRI2Version")) {
		DRI2Version(&dri2_major, &dri2_minor);
	}

	if (dri2_minor < 1) {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "DRI2 requires DRI2 module version 1.1.0 or later\n");
		return FALSE;
	}
#endif

	intel->deviceName = drmGetDeviceNameFromFd(intel->drmSubFD);
	memset(&info, '\0', sizeof(info));
	info.fd = intel->drmSubFD;
	info.driverName = IS_I965G(intel) ? "i965" : "i915";
	info.deviceName = intel->deviceName;

#if DRI2INFOREC_VERSION >= 3
	info.version = 3;
	info.CreateBuffer = I830DRI2CreateBuffer;
	info.DestroyBuffer = I830DRI2DestroyBuffer;
#else
# ifdef USE_DRI2_1_1_0
	info.version = 2;
	info.CreateBuffers = NULL;
	info.DestroyBuffers = NULL;
	info.CreateBuffer = I830DRI2CreateBuffer;
	info.DestroyBuffer = I830DRI2DestroyBuffer;
# else
	info.version = 1;
	info.CreateBuffers = I830DRI2CreateBuffers;
	info.DestroyBuffers = I830DRI2DestroyBuffers;
# endif
#endif

	info.CopyRegion = I830DRI2CopyRegion;
#if DRI2INFOREC_VERSION >= 4
	if (intel->use_pageflipping) {
	    info.version = 4;
	    info.ScheduleSwap = I830DRI2ScheduleSwap;
	    info.GetMSC = I830DRI2GetMSC;
	    info.ScheduleWaitMSC = I830DRI2ScheduleWaitMSC;
	    info.numDrivers = 1;
	    info.driverNames = driverNames;
	    driverNames[0] = info.driverName;
	}
#endif

	return DRI2ScreenInit(screen, &info);
}

void I830DRI2CloseScreen(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	DRI2CloseScreen(screen);
	intel->directRenderingType = DRI_NONE;
	drmFree(intel->deviceName);
}
