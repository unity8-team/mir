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
#include <sys/mman.h>
#include <time.h>
#include <errno.h>

#include "xf86.h"
#include "xf86_OSproc.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "windowstr.h"
#include "gcstruct.h"

#include "sna.h"
#include "sna_reg.h"

#include "i915_drm.h"

#include "dri2.h"

#if DRI2INFOREC_VERSION <= 2
#error DRI2 version supported by the Xserver is too old
#endif

#if DEBUG_DRI
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

enum frame_event_type {
	DRI2_SWAP,
	DRI2_SWAP_THROTTLE,
	DRI2_ASYNC_FLIP,
	DRI2_FLIP,
	DRI2_WAITMSC,
};

struct sna_dri_private {
	int refcnt;
	PixmapPtr pixmap;
	struct kgem_bo *bo;
};

struct sna_dri_frame_event {
	struct sna *sna;
	XID drawable_id;
	XID client_id;	/* fake client ID to track client destruction */
	ClientPtr client;
	enum frame_event_type type;
	int frame;
	int pipe;
	int count;

	/* for swaps & flips only */
	DRI2SwapEventPtr event_complete;
	void *event_data;
	DRI2BufferPtr front;
	DRI2BufferPtr back;

	unsigned int fe_frame;
	unsigned int fe_tv_sec;
	unsigned int fe_tv_usec;

	PixmapPtr old_front;
	PixmapPtr next_front;
	uint32_t old_fb;
};

static inline struct sna_dri_frame_event *
to_frame_event(void *data)
{
	 return (struct sna_dri_frame_event *)((uintptr_t)data & ~1);
}

static inline PixmapPtr
get_pixmap(DRI2Buffer2Ptr buffer)
{
	struct sna_dri_private *priv = buffer->driverPrivate;
	return priv->pixmap;
}

static struct kgem_bo *sna_pixmap_set_dri(struct sna *sna,
					  PixmapPtr pixmap)
{
	struct sna_pixmap *priv;

	priv = sna_pixmap_force_to_gpu(pixmap);
	if (priv == NULL)
		return NULL;

	if (priv->flush)
		return priv->gpu_bo;

	if (priv->cpu_damage)
		list_add(&priv->list, &sna->dirty_pixmaps);

	/* The bo is outside of our control, so presume it is written to */
	priv->gpu_bo->needs_flush = 1;

	/* We need to submit any modifications to and reads from this
	 * buffer before we send any reply to the Client.
	 *
	 * As we don't track which Client, we flush for all.
	 */
	priv->flush = 1;
	priv->gpu_bo->flush = 1;
	if (priv->gpu_bo->exec)
		sna->kgem.flush = 1;

	/* Don't allow this named buffer to be replaced */
	priv->pinned = 1;

	return priv->gpu_bo;
}

static DRI2Buffer2Ptr
sna_dri_create_buffer(DrawablePtr drawable,
		      unsigned int attachment,
		      unsigned int format)
{
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	DRI2Buffer2Ptr buffer;
	struct sna_dri_private *private;
	PixmapPtr pixmap;
	struct kgem_bo *bo;
	int bpp, usage;

	DBG(("%s(attachment=%d, format=%d, drawable=%dx%d)\n",
	     __FUNCTION__, attachment, format,drawable->width,drawable->height));

	buffer = calloc(1, sizeof *buffer + sizeof *private);
	if (buffer == NULL)
		return NULL;
	private = (struct sna_dri_private *)(buffer + 1);

	pixmap = NULL;
	bo = NULL;
	usage = CREATE_PIXMAP_USAGE_SCRATCH;
	switch (attachment) {
	case DRI2BufferFrontLeft:
		pixmap = get_drawable_pixmap(drawable);
		pixmap->refcnt++;
		bo = sna_pixmap_set_dri(sna, pixmap);
		bpp = pixmap->drawable.bitsPerPixel;
		DBG(("%s: attaching to front buffer %dx%d [%p:%d]\n",
		     __FUNCTION__,
		     pixmap->drawable.width, pixmap->drawable.height,
		     pixmap, pixmap->refcnt));
		break;

	case DRI2BufferBackLeft:
	case DRI2BufferBackRight:
	case DRI2BufferFrontRight:
		/* Allocate a normal window, perhaps flippable */
		usage = 0;
		if (drawable->width == sna->front->drawable.width &&
		    drawable->height == sna->front->drawable.height &&
		    drawable->depth == sna->front->drawable.depth)
			usage = SNA_CREATE_FB;

	case DRI2BufferFakeFrontLeft:
	case DRI2BufferFakeFrontRight:
		pixmap = screen->CreatePixmap(screen,
					      drawable->width,
					      drawable->height,
					      drawable->depth,
					      usage);
		if (!pixmap)
			goto err;

		bo = sna_pixmap_set_dri(sna, pixmap);
		bpp = pixmap->drawable.bitsPerPixel;
		break;

	case DRI2BufferStencil:
		/*
		 * The stencil buffer has quirky pitch requirements.  From Vol
		 * 2a, 11.5.6.2.1 3DSTATE_STENCIL_BUFFER, field "Surface
		 * Pitch":
		 *    The pitch must be set to 2x the value computed based on
		 *    width, as the stencil buffer is stored with two rows
		 *    interleaved.
		 * To accomplish this, we resort to the nasty hack of doubling
		 * the drm region's cpp and halving its height.
		 *
		 * If we neglect to double the pitch, then
		 * drm_intel_gem_bo_map_gtt() maps the memory incorrectly.
		 */
		bpp = format ? format : drawable->bitsPerPixel;
		bo = kgem_create_2d(&sna->kgem,
				    drawable->width,
				    drawable->height/2, 2*bpp,
				    I915_TILING_Y,
				    CREATE_EXACT);
		break;

	case DRI2BufferDepth:
	case DRI2BufferDepthStencil:
	case DRI2BufferHiz:
	case DRI2BufferAccum:
		bpp = format ? format : drawable->bitsPerPixel,
		bo = kgem_create_2d(&sna->kgem,
				    drawable->width, drawable->height, bpp,
				    //sna->kgem.gen >= 40 ? I915_TILING_Y : I915_TILING_X,
				    I915_TILING_Y,
				    CREATE_EXACT);
		break;

	default:
		break;
	}
	if (bo == NULL)
		goto err;

	buffer->attachment = attachment;
	buffer->pitch = bo->pitch;
	buffer->cpp = bpp / 8;
	buffer->driverPrivate = private;
	buffer->format = format;
	buffer->flags = 0;
	buffer->name = kgem_bo_flink(&sna->kgem, bo);
	private->refcnt = 1;
	private->pixmap = pixmap;
	private->bo = bo;

	if (buffer->name == 0) {
		/* failed to name buffer */
		if (pixmap)
			screen->DestroyPixmap(pixmap);
		else
			kgem_bo_destroy(&sna->kgem, bo);
		goto err;
	}

	return buffer;

err:
	free(buffer);
	return NULL;
}

static void _sna_dri_destroy_buffer(struct sna *sna, DRI2Buffer2Ptr buffer)
{
	struct sna_dri_private *private = buffer->driverPrivate;

	if (--private->refcnt == 0) {
		if (private->pixmap) {
			ScreenPtr screen = private->pixmap->drawable.pScreen;
			screen->DestroyPixmap(private->pixmap);
		} else
			kgem_bo_destroy(&sna->kgem, private->bo);

		free(buffer);
	}
}

static void sna_dri_destroy_buffer(DrawablePtr drawable, DRI2Buffer2Ptr buffer)
{
	if (buffer && buffer->driverPrivate)
		_sna_dri_destroy_buffer(to_sna_from_drawable(drawable), buffer);
	else
		free(buffer);
}

static void sna_dri_reference_buffer(DRI2Buffer2Ptr buffer)
{
	if (buffer) {
		struct sna_dri_private *private = buffer->driverPrivate;
		private->refcnt++;
	}
}

static void damage(DrawablePtr drawable, PixmapPtr pixmap, RegionPtr region)
{
	struct sna_pixmap *priv;
	int16_t dx, dy;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	priv = sna_pixmap(pixmap);
	if (priv->gpu_only)
		return;

	if (region) {
		BoxPtr box;

		RegionTranslate(region, dx, dy);
		box = RegionExtents(region);
		if (RegionNumRects(region) == 1 &&
		    box->x1 <= 0 && box->y1 <= 0 &&
		    box->x2 >= pixmap->drawable.width &&
		    box->y2 >= pixmap->drawable.height) {
			sna_damage_all(&priv->gpu_damage,
				       pixmap->drawable.width,
				       pixmap->drawable.height);
			sna_damage_destroy(&priv->cpu_damage);
		} else {
			sna_damage_add(&priv->gpu_damage, region);
			sna_damage_subtract(&priv->cpu_damage, region);
		}

		RegionTranslate(region, -dx, -dy);
	} else {
		BoxRec box;

		box.x1 = drawable->x + dx;
		box.x2 = box.x1 + drawable->width;

		box.y1 = drawable->y + dy;
		box.y2 = box.y1 + drawable->height;
		if (box.x1 == 0 && box.y1 == 0 &&
		    box.x2 == pixmap->drawable.width &&
		    box.y2 == pixmap->drawable.height) {
			sna_damage_all(&priv->gpu_damage,
				       pixmap->drawable.width,
				       pixmap->drawable.height);
			sna_damage_destroy(&priv->cpu_damage);
		} else {
			sna_damage_add_box(&priv->gpu_damage, &box);
			sna_damage_subtract_box(&priv->gpu_damage, &box);
		}
	}
}

static void
sna_dri_copy_region(DrawablePtr draw,
		    RegionPtr region,
		    DRI2BufferPtr dst_buffer,
		    DRI2BufferPtr src_buffer)
{
	struct sna *sna = to_sna_from_drawable(draw);
	struct sna_dri_private *src_priv = src_buffer->driverPrivate;
	struct sna_dri_private *dst_priv = dst_buffer->driverPrivate;
	PixmapPtr src = src_priv->pixmap;
	PixmapPtr dst = dst_priv->pixmap;
	pixman_region16_t clip;
	int16_t dx, dy;

	DBG(("%s(region=(%d, %d), (%d, %d)))\n", __FUNCTION__,
	     REGION_EXTENTS(NULL, region)->x1,
	     REGION_EXTENTS(NULL, region)->y1,
	     REGION_EXTENTS(NULL, region)->x2,
	     REGION_EXTENTS(NULL, region)->y2));

	DBG(("%s: draw (%d, %d)\n", __FUNCTION__, draw->x, draw->y));
	DBG(("%s: dst -- attachment=%d, name=%d, handle=%d [screen %d]\n",
	     __FUNCTION__,
	     dst_buffer->attachment,
	     dst_buffer->name,
	     dst_priv->bo->handle,
	     sna_pixmap(sna->front)->gpu_bo->handle));
	DBG(("%s: src -- attachment=%d, name=%d, handle=%d\n",
	     __FUNCTION__,
	     src_buffer->attachment,
	     src_buffer->name,
	     src_priv->bo->handle));

	if (draw->type == DRAWABLE_WINDOW) {
		WindowPtr win = (WindowPtr)draw;

		DBG(("%s: draw=(%d, %d), delta=(%d, %d)\n",
		     __FUNCTION__, draw->x, draw->y,
		     get_drawable_dx(draw), get_drawable_dy(draw)));
		pixman_region_translate(region, draw->x, draw->y);

		pixman_region_init(&clip);
		pixman_region_intersect(&clip, &win->clipList, region);
		if (!pixman_region_not_empty(&clip)) {
			DBG(("%s: all clipped\n", __FUNCTION__));
			return;
		}
		region = &clip;

		get_drawable_deltas(draw, dst, &dx, &dy);

		DBG(("%s clipped=(%d, %d), (%d, %d)x%d\n", __FUNCTION__,
		     REGION_EXTENTS(NULL, region)->x1,
		     REGION_EXTENTS(NULL, region)->y1,
		     REGION_EXTENTS(NULL, region)->x2,
		     REGION_EXTENTS(NULL, region)->y2,
		     REGION_NUM_RECTS(region)));

		if (dst_buffer->attachment == DRI2BufferFrontLeft) {
			assert(dst == sna->front);
			assert(dst_priv->bo == sna_pixmap(sna->front)->gpu_bo);
		}
	} else {
		assert(draw->x == 0);
		assert(draw->y == 0);
		dx = dy = 0;
	}

	assert(sna_pixmap(src)->cpu_damage == NULL);
	assert(sna_pixmap(dst)->cpu_damage == NULL);

	/* It's important that this copy gets submitted before the
	 * direct rendering client submits rendering for the next
	 * frame, but we don't actually need to submit right now.  The
	 * client will wait for the DRI2CopyRegion reply or the swap
	 * buffer event before rendering, and we'll hit the flush
	 * callback chain before those messages are sent.  We submit
	 * our batch buffers from the flush callback chain so we know
	 * that will happen before the client tries to render
	 * again.
	 */
	sna->render.copy_boxes(sna, GXcopy,
			       src, src_priv->bo, -draw->x, -draw->y,
			       dst, dst_priv->bo, dx, dy,
			       REGION_RECTS(region),
			       REGION_NUM_RECTS(region));

	damage(draw, dst, region);
	if (region == &clip)
		pixman_region_fini(&clip);
}

static void
sna_dri_swap_blit(struct sna *sna, DrawablePtr draw, DRI2BufferPtr front, DRI2BufferPtr back)
{
	struct sna_dri_private *src_priv = back->driverPrivate;
	struct sna_dri_private *dst_priv = front->driverPrivate;
	PixmapPtr src = src_priv->pixmap;
	PixmapPtr dst = dst_priv->pixmap;
	bool flush = false;
	BoxRec box, *boxes;
	int16_t dx, dy;
	int n;

	DBG(("%s: back -- attachment=%d, name=%d, handle=%d\n",
	     __FUNCTION__,
	     back->attachment,
	     back->name,
	     src_priv->bo->handle));

	if (draw->type == DRAWABLE_PIXMAP) {
		box.x1 = box.y1 = 0;
		box.x2 = draw->width;
		box.y2 = draw->height;

		boxes = &box;
		n = 1;
		dx = dy = 0;
	} else {
		WindowPtr win = (WindowPtr)draw;

		boxes = REGION_RECTS(&win->clipList);
		n = REGION_NUM_RECTS(&win->clipList);
		if (n == 0)
			return;

		flush = sna_wait_for_scanline(sna, sna->front,
					      NULL, &win->clipList.extents);

		get_drawable_deltas(draw, dst, &dx, &dy);
	}

	/* It's important that this copy gets submitted before the
	 * direct rendering client submits rendering for the next
	 * frame, but we don't actually need to submit right now.  The
	 * client will wait for the DRI2CopyRegion reply or the swap
	 * buffer event before rendering, and we'll hit the flush
	 * callback chain before those messages are sent.  We submit
	 * our batch buffers from the flush callback chain so we know
	 * that will happen before the client tries to render
	 * again.
	 */
	sna->render.copy_boxes(sna, GXcopy,
			       src, src_priv->bo, -draw->x, -draw->y,
			       dst, dst_priv->bo, dx, dy,
			       boxes, n);

	damage(draw, dst, NULL);

	DBG(("%s: flushing? %d\n", __FUNCTION__, flush));
	if (flush) /* STAT! */
		kgem_submit(&sna->kgem);
}

#if DRI2INFOREC_VERSION >= 4

static int
sna_dri_get_pipe(DrawablePtr pDraw)
{
	ScrnInfoPtr pScrn = xf86Screens[pDraw->pScreen->myNum];
	BoxRec box, crtcbox;
	xf86CrtcPtr crtc;
	int pipe;

	if (pDraw->type == DRAWABLE_PIXMAP)
		return -1;

	box.x1 = pDraw->x;
	box.y1 = pDraw->y;
	box.x2 = box.x1 + pDraw->width;
	box.y2 = box.y1 + pDraw->height;

	crtc = sna_covering_crtc(pScrn, &box, NULL, &crtcbox);

	/* Make sure the CRTC is valid and this is the real front buffer */
	pipe = -1;
	if (crtc != NULL && !crtc->rotatedData)
		pipe = sna_crtc_to_pipe(crtc);

	DBG(("%s(box=((%d, %d), (%d, %d)), crtcbox=((%d, %d), (%d, %d)), pipe=%d)\n",
	     __FUNCTION__,
	     box.x1, box.y1, box.x2, box.y2,
	     crtcbox.x1, crtcbox.y1, crtcbox.x2, crtcbox.y2,
	     pipe));

	return pipe;
}

static RESTYPE frame_event_client_type, frame_event_drawable_type;

static int
sna_dri_frame_event_client_gone(void *data, XID id)
{
	struct sna_dri_frame_event *frame_event = data;

	frame_event->client = NULL;
	frame_event->client_id = None;
	return Success;
}

static int
sna_dri_frame_event_drawable_gone(void *data, XID id)
{
	struct sna_dri_frame_event *frame_event = data;

	frame_event->drawable_id = None;
	return Success;
}

static Bool
sna_dri_register_frame_event_resource_types(void)
{
	frame_event_client_type =
		CreateNewResourceType(sna_dri_frame_event_client_gone,
				      "Frame Event Client");
	if (!frame_event_client_type)
		return FALSE;

	frame_event_drawable_type =
		CreateNewResourceType(sna_dri_frame_event_drawable_gone,
				      "Frame Event Drawable");
	if (!frame_event_drawable_type)
		return FALSE;

	return TRUE;
}

/*
 * Hook this frame event into the server resource
 * database so we can clean it up if the drawable or
 * client exits while the swap is pending
 */
static Bool
sna_dri_add_frame_event(struct sna_dri_frame_event *frame_event)
{
	frame_event->client_id = FakeClientID(frame_event->client->index);

	if (!AddResource(frame_event->client_id,
			 frame_event_client_type,
			 frame_event))
		return FALSE;

	if (!AddResource(frame_event->drawable_id,
			 frame_event_drawable_type,
			 frame_event)) {
		FreeResourceByType(frame_event->client_id,
				   frame_event_client_type,
				   TRUE);
		return FALSE;
	}

	return TRUE;
}

static void
sna_dri_frame_event_info_free(struct sna_dri_frame_event *info)
{
	if (info->client_id)
		FreeResourceByType(info->client_id,
				   frame_event_client_type,
				   TRUE);

	if (info->drawable_id)
		FreeResourceByType(info->drawable_id,
				   frame_event_drawable_type,
				   TRUE);

	if (info->front)
		_sna_dri_destroy_buffer(info->sna, info->front);
	if (info->back)
		_sna_dri_destroy_buffer(info->sna, info->back);
	free(info);
}

static void
sna_dri_exchange_buffers(DrawablePtr draw,
			 DRI2BufferPtr front,
			 DRI2BufferPtr back)
{
	int tmp;

	DBG(("%s(%d <--> %d)\n",
	     __FUNCTION__, front->attachment, back->attachment));

	tmp = front->attachment;
	front->attachment = back->attachment;
	back->attachment = tmp;
}

/*
 * Our internal swap routine takes care of actually exchanging, blitting, or
 * flipping buffers as necessary.
 */
static Bool
sna_dri_schedule_flip(struct sna *sna,
		      DrawablePtr draw,
		      struct sna_dri_frame_event *info)
{
	struct sna_dri_private *back_priv;

	DBG(("%s()\n", __FUNCTION__));

	/* Page flip the full screen buffer */
	back_priv = info->back->driverPrivate;
	info->count = sna_do_pageflip(sna,
				      back_priv->pixmap,
				      info, info->pipe,
				      &info->old_fb);
	return info->count != 0;
}

static Bool
can_flip(struct sna * sna,
	 DrawablePtr draw,
	 DRI2BufferPtr front,
	 DRI2BufferPtr back)
{
	struct sna_dri_private *back_priv = back->driverPrivate;
	struct sna_dri_private *front_priv = front->driverPrivate;
	struct sna_pixmap *front_sna, *back_sna;
	WindowPtr win = (WindowPtr)draw;
	PixmapPtr front_pixmap = front_priv->pixmap;
	PixmapPtr back_pixmap = back_priv->pixmap;

	ScreenPtr screen = draw->pScreen;

	if (draw->type == DRAWABLE_PIXMAP)
		return FALSE;

	if (front->format != back->format) {
		DBG(("%s: no, format mismatch, front = %d, back = %d\n",
		     __FUNCTION__, front->format, back->format));
		return FALSE;
	}

	if (front->attachment != DRI2BufferFrontLeft) {
		DBG(("%s: no, front attachment [%d] is not FrontLeft [%d]\n",
		     __FUNCTION__,
		     front->attachment,
		     DRI2BufferFrontLeft));
		return FALSE;
	}

	if (sna->shadow) {
		DBG(("%s: no, shadow enabled\n", __FUNCTION__));
		return FALSE;
	}

	if (front_pixmap != sna->front) {
		DBG(("%s: no, window is not on the front buffer\n",
		     __FUNCTION__));
		return FALSE;
	}

	DBG(("%s: window size: %dx%d, clip=(%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     win->drawable.width, win->drawable.height,
	     win->clipList.extents.x1, win->clipList.extents.y1,
	     win->clipList.extents.x2, win->clipList.extents.y2));
	if (!RegionEqual(&win->clipList, &screen->root->winSize)) {
		DBG(("%s: no, window is clipped: clip region=(%d, %d), (%d, %d), root size=(%d, %d), (%d, %d)\n",
		     __FUNCTION__,
		     win->clipList.extents.x1,
		     win->clipList.extents.y1,
		     win->clipList.extents.x2,
		     win->clipList.extents.y2,
		     screen->root->winSize.extents.x1,
		     screen->root->winSize.extents.y1,
		     screen->root->winSize.extents.x2,
		     screen->root->winSize.extents.y2));
		return FALSE;
	}

	if (draw->x != 0 || draw->y != 0 ||
#ifdef COMPOSITE
	    draw->x != front_pixmap->screen_x ||
	    draw->y != front_pixmap->screen_y ||
#endif
	    draw->width != front_pixmap->drawable.width ||
	    draw->height != front_pixmap->drawable.height) {
		DBG(("%s: no, window is not full size (%dx%d)!=(%dx%d)\n",
		     __FUNCTION__,
		     draw->width, draw->height,
		     front_pixmap->drawable.width,
		     front_pixmap->drawable.height));
		return FALSE;
	}

	if (front_pixmap->drawable.width != back_pixmap->drawable.width) {
		DBG(("%s -- no, size mismatch: front width=%d, back=%d\n",
		     __FUNCTION__,
		     front_pixmap->drawable.width,
		     back_pixmap->drawable.width));
		return FALSE;
	}

	if (front_pixmap->drawable.height != back_pixmap->drawable.height) {
		DBG(("%s -- no, size mismatch: front height=%d, back=%d\n",
		     __FUNCTION__,
		     front_pixmap->drawable.height,
		     back_pixmap->drawable.height));
		return FALSE;
	}

	if (front_pixmap->drawable.depth != back_pixmap->drawable.depth) {
		DBG(("%s -- no, depth mismatch: front bpp=%d/%d, back=%d/%d\n",
		     __FUNCTION__,
		     front_pixmap->drawable.depth,
		     front_pixmap->drawable.bitsPerPixel,
		     back_pixmap->drawable.depth,
		     back_pixmap->drawable.bitsPerPixel));
		return FALSE;
	}

	/* prevent an implicit tiling mode change */
	front_sna = sna_pixmap(front_pixmap);
	back_sna  = sna_pixmap(back_pixmap);
	if (front_sna->gpu_bo->tiling != back_sna->gpu_bo->tiling) {
		DBG(("%s -- no, tiling mismatch: front %d, back=%d\n",
		     __FUNCTION__,
		     front_sna->gpu_bo->tiling,
		     back_sna->gpu_bo->tiling));
		return FALSE;
	}

	if (front_sna->gpu_only != back_sna->gpu_only) {
		DBG(("%s -- no, mismatch in gpu_only: front %d, back=%d\n",
		     __FUNCTION__, front_sna->gpu_only, back_sna->gpu_only));
		return FALSE;
	}

	return TRUE;
}

static void sna_dri_vblank_handle(int fd,
				  unsigned int frame, unsigned int tv_sec,
				  unsigned int tv_usec,
				  void *data)
{
	struct sna_dri_frame_event *info = data;
	DrawablePtr draw;
	struct sna *sna;
	int status;

	DBG(("%s(id=%d, type=%d)\n", __FUNCTION__,
	     (int)info->drawable_id, info->type));

	status = BadDrawable;
	if (info->drawable_id)
		status = dixLookupDrawable(&draw,
					   info->drawable_id,
					   serverClient,
					   M_ANY, DixWriteAccess);
	if (status != Success)
		goto done;

	sna = to_sna_from_drawable(draw);

	switch (info->type) {
	case DRI2_FLIP:
		/* If we can still flip... */
		if (can_flip(sna, draw, info->front, info->back) &&
		    sna_dri_schedule_flip(sna, draw, info)) {
			sna_dri_exchange_buffers(draw, info->front, info->back);
			return;
		}
		/* else fall through to exchange/blit */
	case DRI2_SWAP:
		sna_dri_swap_blit(sna, draw, info->front, info->back);
	case DRI2_SWAP_THROTTLE:
		DRI2SwapComplete(info->client,
				 draw, frame,
				 tv_sec, tv_usec,
				 DRI2_BLIT_COMPLETE,
				 info->client ? info->event_complete : NULL,
				 info->event_data);
		break;

	case DRI2_WAITMSC:
		if (info->client)
			DRI2WaitMSCComplete(info->client, draw,
					    frame, tv_sec, tv_usec);
		break;
	default:
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "%s: unknown vblank event received\n", __func__);
		/* Unknown type */
		break;
	}

done:
	sna_dri_frame_event_info_free(info);
}

static void sna_dri_flip_event(struct sna *sna,
			       struct sna_dri_frame_event *flip)
{
	DrawablePtr drawable;
	int status;

	DBG(("%s(frame=%d, tv=%d.%06d, type=%d)\n",
	     __FUNCTION__,
	     flip->fe_frame,
	     flip->fe_tv_sec,
	     flip->fe_tv_usec,
	     flip->type));

	/* We assume our flips arrive in order, so we don't check the frame */
	switch (flip->type) {
	case DRI2_FLIP:
		/* Deliver cached msc, ust from reference crtc */
		/* Check for too small vblank count of pageflip completion, taking wraparound
		 * into account. This usually means some defective kms pageflip completion,
		 * causing wrong (msc, ust) return values and possible visual corruption.
		 */
		if (!flip->drawable_id)
			return;

		status = dixLookupDrawable(&drawable,
					   flip->drawable_id,
					   serverClient,
					   M_ANY, DixWriteAccess);
		if (status != Success)
			return;

		if ((flip->fe_frame < flip->frame) &&
		    (flip->frame - flip->fe_frame < 5)) {
			static int limit = 5;

			/* XXX we are currently hitting this path with older
			 * kernels, so make it quieter.
			 */
			if (limit) {
				xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
					   "%s: Pageflip completion has impossible msc %d < target_msc %d\n",
					   __func__, flip->fe_frame, flip->frame);
				limit--;
			}

			/* All-0 values signal timestamping failure. */
			flip->fe_frame = flip->fe_tv_sec = flip->fe_tv_usec = 0;
		}

		DBG(("%s: flip complete\n", __FUNCTION__));
		DRI2SwapComplete(flip->client, drawable,
				 flip->fe_frame,
				 flip->fe_tv_sec,
				 flip->fe_tv_usec,
				 DRI2_FLIP_COMPLETE,
				 flip->client ? flip->event_complete : NULL,
				 flip->event_data);

		sna_mode_delete_fb(flip->sna, flip->old_front, flip->old_fb);
		sna_dri_frame_event_info_free(flip);
		break;

	case DRI2_ASYNC_FLIP:
		DBG(("%s: async swap flip completed on pipe %d, pending? %d, new? %d\n",
		     __FUNCTION__, flip->pipe,
		     sna->dri.flip_pending[flip->pipe] != NULL,
		     flip->next_front != sna->front));
		assert(sna->dri.flip_pending[flip->pipe] == flip);

		sna_mode_delete_fb(flip->sna, flip->old_front, flip->old_fb);

		if (sna->front != flip->next_front) {
			PixmapPtr next = sna->front;

			DBG(("%s: async flip continuing\n", __FUNCTION__));
			flip->count = sna_do_pageflip(sna, next,
						      flip, flip->pipe,
						      &flip->old_fb);
			if (flip->count) {
				flip->old_front = flip->next_front;
				flip->next_front = next;
				flip->next_front->refcnt++;
			} else
				goto finish_async_flip;
		} else {
finish_async_flip:
			DBG(("%s: async flip completed\n", __FUNCTION__));
			flip->next_front->drawable.pScreen->DestroyPixmap(flip->next_front);
			sna->dri.flip_pending[flip->pipe] = NULL;
			sna_dri_frame_event_info_free(flip);
		}
		break;

	default:
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "%s: unknown vblank event received\n", __func__);
		/* Unknown type */
		break;
	}
}

static void
sna_dri_page_flip_handler(int fd, unsigned int frame, unsigned int tv_sec,
			  unsigned int tv_usec, void *data)
{
	struct sna_dri_frame_event *info = to_frame_event(data);

	DBG(("%s: pending flip_count=%d\n", __FUNCTION__, info->count));

	/* Is this the event whose info shall be delivered to higher level? */
	if ((uintptr_t)data & 1) {
		/* Yes: Cache msc, ust for later delivery. */
		info->fe_frame = frame;
		info->fe_tv_sec = tv_sec;
		info->fe_tv_usec = tv_usec;
	}

	if (--info->count)
		return;

	sna_dri_flip_event(info->sna, info);
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
sna_dri_schedule_swap(ClientPtr client, DrawablePtr draw, DRI2BufferPtr front,
		       DRI2BufferPtr back, CARD64 *target_msc, CARD64 divisor,
		       CARD64 remainder, DRI2SwapEventPtr func, void *data)
{
	ScreenPtr screen = draw->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	drmVBlank vbl;
	int pipe, flip;
	struct sna_dri_frame_event *info = NULL;
	enum frame_event_type swap_type = DRI2_SWAP;
	CARD64 current_msc;

	DBG(("%s(target_msc=%llu, divisor=%llu, remainder=%llu)\n",
	     __FUNCTION__,
	     (long long)*target_msc,
	     (long long)divisor,
	     (long long)remainder));

	flip = 0;
	if (can_flip(sna, draw, front, back)) {
		DBG(("%s: can flip\n", __FUNCTION__));
		swap_type = DRI2_FLIP;
		flip = 1;
	}

	/* Drawable not displayed... just complete the swap */
	pipe = sna_dri_get_pipe(draw);
	if (pipe == -1) {
		struct sna_dri_private *back_priv = back->driverPrivate;
		PixmapPtr pixmap;

		if (!flip)
			goto blit_fallback;

		pixmap = sna_set_screen_pixmap(sna, back_priv->pixmap);
		assert(pixmap->refcnt > 1);
		pixmap->refcnt--;
		sna_dri_exchange_buffers(draw, front, back);
		DRI2SwapComplete(client, draw, 0, 0, 0,
				 DRI2_EXCHANGE_COMPLETE, func, data);
		return TRUE;
	}

	/* Truncate to match kernel interfaces; means occasional overflow
	 * misses, but that's generally not a big deal */
	*target_msc &= 0xffffffff;
	divisor &= 0xffffffff;
	remainder &= 0xffffffff;

	info = calloc(1, sizeof(struct sna_dri_frame_event));
	if (!info)
		goto blit_fallback;

	info->sna = sna;
	info->drawable_id = draw->id;
	info->client = client;
	info->event_complete = func;
	info->event_data = data;
	info->front = front;
	info->back = back;
	info->pipe = pipe;

	if (!sna_dri_add_frame_event(info)) {
		free(info);
		info = NULL;
		goto blit_fallback;
	}

	sna_dri_reference_buffer(front);
	sna_dri_reference_buffer(back);

	info->type = swap_type;
	if (divisor == 0)
		goto immediate;

	/* Get current count */
	vbl.request.type = DRM_VBLANK_RELATIVE;
	if (pipe > 0)
		vbl.request.type |= DRM_VBLANK_SECONDARY;
	vbl.request.sequence = 0;
	if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "first get vblank counter failed: %s\n",
			   strerror(errno));
		goto blit_fallback;
	}

	current_msc = vbl.reply.sequence;

	/*
	 * If divisor is zero, or current_msc is smaller than target_msc
	 * we just need to make sure target_msc passes before initiating
	 * the swap.
	 */
	if (current_msc < *target_msc) {
		DBG(("%s: performing immediate swap: current=%d, target=%d,  divisor=%d\n",
		     __FUNCTION__,
		     (int)current_msc,
		     (int)*target_msc,
		     (int)divisor));

		/* If target_msc already reached or passed, set it to
		 * current_msc to ensure we return a reasonable value back
		 * to the caller. This makes swap_interval logic more robust.
		 */
		if (current_msc >= *target_msc)
			*target_msc = current_msc;

immediate:
		info->frame = *target_msc;
		if (flip && sna_dri_schedule_flip(sna, draw, info)) {
			info->old_front =
				sna_set_screen_pixmap(sna, get_pixmap(back));
			sna_dri_exchange_buffers(draw, front, back);
			return TRUE;
		}

		DBG(("%s: emitting immediate vsync'ed blit, throttling client\n",
		     __FUNCTION__));

		 info->type = DRI2_SWAP_THROTTLE;

		 vbl.request.type =
			 DRM_VBLANK_RELATIVE |
			 DRM_VBLANK_EVENT |
			 DRM_VBLANK_NEXTONMISS;
		 if (pipe > 0)
			 vbl.request.type |= DRM_VBLANK_SECONDARY;
		 vbl.request.sequence = 0;
		 vbl.request.signal = (unsigned long)info;
		 if (drmWaitVBlank(sna->kgem.fd, &vbl))
			 sna_dri_frame_event_info_free(info);

		 sna_dri_swap_blit(sna, draw, front, back);
		 return TRUE;
	}

	/* Correct target_msc by 'flip' if swap_type == DRI2_FLIP.
	 * Do it early, so handling of different timing constraints
	 * for divisor, remainder and msc vs. target_msc works.
	 */
	if (flip && *target_msc > 0)
		--*target_msc;

	DBG(("%s: missed target, queueing event for next: current=%d, target=%d,  divisor=%d\n",
		     __FUNCTION__,
		     (int)current_msc,
		     (int)*target_msc,
		     (int)divisor));

	/*
	 * If we get here, target_msc has already passed or we don't have one,
	 * and we need to queue an event that will satisfy the divisor/remainder
	 * equation.
	 */
	vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
	if (flip == 0)
		vbl.request.type |= DRM_VBLANK_NEXTONMISS;
	if (pipe > 0)
		vbl.request.type |= DRM_VBLANK_SECONDARY;

	vbl.request.sequence = current_msc - current_msc % divisor + remainder;

	/*
	 * If the calculated deadline vbl.request.sequence is smaller than
	 * or equal to current_msc, it means we've passed the last point
	 * when effective onset frame seq could satisfy
	 * seq % divisor == remainder, so we need to wait for the next time
	 * this will happen.

	 * This comparison takes the 1 frame swap delay in pageflipping mode
	 * into account, as well as a potential DRM_VBLANK_NEXTONMISS delay
	 * if we are blitting/exchanging instead of flipping.
	 */
	if (vbl.request.sequence <= current_msc)
		vbl.request.sequence += divisor;

	/* Account for 1 frame extra pageflip delay if flip > 0 */
	vbl.request.sequence -= flip;

	vbl.request.signal = (unsigned long)info;
	if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "final get vblank counter failed: %s\n",
			   strerror(errno));
		goto blit_fallback;
	}

	/* Adjust returned value for 1 fame pageflip offset of flip > 0 */
	*target_msc = vbl.reply.sequence + flip;
	info->frame = *target_msc;
	return TRUE;

blit_fallback:
	DBG(("%s -- blit\n", __FUNCTION__));
	sna_dri_swap_blit(sna, draw, front, back);
	if (info)
		sna_dri_frame_event_info_free(info);
	DRI2SwapComplete(client, draw, 0, 0, 0, DRI2_BLIT_COMPLETE, func, data);
	*target_msc = 0; /* offscreen, so zero out target vblank count */
	return TRUE;
}

#if DRI2INFOREC_VERSION >= 6
static void
sna_dri_async_swap(ClientPtr client, DrawablePtr draw,
		   DRI2BufferPtr front, DRI2BufferPtr back,
		   DRI2SwapEventPtr func, void *data)
{
	ScreenPtr screen = draw->pScreen;
	struct sna *sna = to_sna_from_screen(screen);
	int type = DRI2_EXCHANGE_COMPLETE;
	struct sna_dri_private *back_priv = back->driverPrivate;
	struct sna_dri_private *front_priv = front->driverPrivate;
	struct sna_dri_frame_event *info;
	PixmapPtr pixmap;
	int pipe;

	DBG(("%s()\n", __FUNCTION__));

	if (!can_flip(sna, draw, front, back)) {
		BoxRec box, *boxes;
		int16_t dx, dy;
		int n;

		if (draw->type == DRAWABLE_PIXMAP) {
			box.x1 = box.y1 = 0;
			box.x2 = draw->width;
			box.y2 = draw->height;

			boxes = &box;
			n = 1;
			dx = dy = 0;
		} else {
			WindowPtr win = (WindowPtr)draw;

			assert(front_priv->pixmap == sna->front);
			boxes = REGION_RECTS(&win->clipList);
			n = REGION_NUM_RECTS(&win->clipList);
			get_drawable_deltas(draw, front_priv->pixmap, &dx, &dy);
		}

		DBG(("%s: fallback blit: %dx%d\n",
		     __FUNCTION__, draw->width, draw->height));

		if (n) {
			sna->render.copy_boxes(sna, GXcopy,
					       back_priv->pixmap,
					       back_priv->bo,
					       -draw->x, -draw->y,
					       front_priv->pixmap,
					       front_priv->bo,
					       dx, dy,
					       boxes, n);
		}

		DRI2SwapComplete(client, draw, 0, 0, 0,
				 DRI2_BLIT_COMPLETE, func, data);
		return;
	}

	assert(front_priv->pixmap == sna->front);

	pipe = sna_dri_get_pipe(draw);
	if (pipe == -1) {
		/* Drawable not displayed... just complete the swap */
		goto exchange;
	}

	info = sna->dri.flip_pending[pipe];
	if (info == NULL) {
		DBG(("%s: no pending flip on pipe %d, so updating scanout\n",
		     __FUNCTION__, pipe));

		info = calloc(1, sizeof(struct sna_dri_frame_event));
		if (!info)
			goto exchange;

		info->sna = sna;
		info->type = DRI2_ASYNC_FLIP;
		info->pipe = pipe;

		info->count = sna_do_pageflip(sna, back_priv->pixmap,
					      info, pipe,
					      &info->old_fb);

		if (info->count == 0) {
			DBG(("%s: pageflip failed\n", __FUNCTION__));
			free(info);
			goto exchange;
		}

		info->old_front = sna->front;
		info->old_front->refcnt++;

		info->next_front = back_priv->pixmap;
		info->next_front->refcnt++;

		type = DRI2_FLIP_COMPLETE;
		sna->dri.flip_pending[pipe] = info;

		if ((pixmap = screen->CreatePixmap(screen,
						   draw->width,
						   draw->height,
						   draw->depth,
						   SNA_CREATE_FB))) {
			DBG(("%s: new back buffer\n", __FUNCTION__));
			assert(front_priv->pixmap->refcnt > 1);
			front_priv->pixmap->refcnt--;
			front_priv->pixmap = pixmap;
			front_priv->bo = sna_pixmap_set_dri(sna, pixmap);
			front->name = kgem_bo_flink(&sna->kgem, front_priv->bo);
			front->pitch = front_priv->bo->pitch;
		}
	}

	if (front_priv->pixmap == info->next_front &&
	    (pixmap = screen->CreatePixmap(screen,
					   draw->width,
					   draw->height,
					   draw->depth,
					   SNA_CREATE_FB))) {
		DBG(("%s: new back buffer\n", __FUNCTION__));
		assert(front_priv->pixmap->refcnt > 1);
		front_priv->pixmap->refcnt--;
		front_priv->pixmap = pixmap;
		front_priv->bo = sna_pixmap_set_dri(sna, pixmap);
		front->name = kgem_bo_flink(&sna->kgem, front_priv->bo);
		front->pitch = front_priv->bo->pitch;
	}

exchange:
	pixmap = sna_set_screen_pixmap(sna, back_priv->pixmap);
	screen->DestroyPixmap(pixmap);

	sna_dri_exchange_buffers(draw, front, back);
	DRI2SwapComplete(client, draw, 0, 0, 0, type, func, data);
}
#endif

/*
 * Get current frame count and frame count timestamp, based on drawable's
 * crtc.
 */
static int
sna_dri_get_msc(DrawablePtr draw, CARD64 *ust, CARD64 *msc)
{
	struct sna *sna = to_sna_from_drawable(draw);
	drmVBlank vbl;
	int pipe = sna_dri_get_pipe(draw);

	DBG(("%s()\n", __FUNCTION__));

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

	if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
		static int limit = 5;
		if (limit) {
			xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
				   "%s:%d get vblank counter failed: %s\n",
				   __FUNCTION__, __LINE__,
				   strerror(errno));
			limit--;
		}
		DBG(("%s: failed on pipe %d\n", __FUNCTION__, pipe));
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
sna_dri_schedule_wait_msc(ClientPtr client, DrawablePtr draw, CARD64 target_msc,
			   CARD64 divisor, CARD64 remainder)
{
	ScreenPtr screen = draw->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	struct sna_dri_frame_event *info;
	drmVBlank vbl;
	int pipe = sna_dri_get_pipe(draw);
	CARD64 current_msc;

	DBG(("%s(target_msc=%llu, divisor=%llu, rem=%llu)\n",
	     __FUNCTION__,
	     (long long)target_msc,
	     (long long)divisor,
	     (long long)remainder));

	/* Truncate to match kernel interfaces; means occasional overflow
	 * misses, but that's generally not a big deal */
	target_msc &= 0xffffffff;
	divisor &= 0xffffffff;
	remainder &= 0xffffffff;

	/* Drawable not visible, return immediately */
	if (pipe == -1)
		goto out_complete;

	info = calloc(1, sizeof(struct sna_dri_frame_event));
	if (!info)
		goto out_complete;

	info->sna = sna;
	info->drawable_id = draw->id;
	info->client = client;
	info->type = DRI2_WAITMSC;

	/* Get current count */
	vbl.request.type = DRM_VBLANK_RELATIVE;
	if (pipe > 0)
		vbl.request.type |= DRM_VBLANK_SECONDARY;
	vbl.request.sequence = 0;
	if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
		static int limit = 5;
		if (limit) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "%s:%d get vblank counter failed: %s\n",
				   __FUNCTION__, __LINE__,
				   strerror(errno));
			limit--;
		}
		goto out_complete;
	}

	current_msc = vbl.reply.sequence;

	/*
	 * If divisor is zero, or current_msc is smaller than target_msc,
	 * we just need to make sure target_msc passes  before waking up the
	 * client.
	 */
	if (divisor == 0 || current_msc < target_msc) {
		/* If target_msc already reached or passed, set it to
		 * current_msc to ensure we return a reasonable value back
		 * to the caller. This keeps the client from continually
		 * sending us MSC targets from the past by forcibly updating
		 * their count on this call.
		 */
		if (current_msc >= target_msc)
			target_msc = current_msc;
		vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
		if (pipe > 0)
			vbl.request.type |= DRM_VBLANK_SECONDARY;
		vbl.request.sequence = target_msc;
		vbl.request.signal = (unsigned long)info;
		if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
			static int limit = 5;
			if (limit) {
				xf86DrvMsg(scrn->scrnIndex, X_WARNING,
					   "%s:%d get vblank counter failed: %s\n",
					   __FUNCTION__, __LINE__,
					   strerror(errno));
				limit--;
			}
			goto out_complete;
		}

		info->frame = vbl.reply.sequence;
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

	vbl.request.sequence = current_msc - (current_msc % divisor) +
	    remainder;

	/*
	 * If calculated remainder is larger than requested remainder,
	 * it means we've passed the last point where
	 * seq % divisor == remainder, so we need to wait for the next time
	 * that will happen.
	 */
	if ((current_msc % divisor) >= remainder)
	    vbl.request.sequence += divisor;

	vbl.request.signal = (unsigned long)info;
	if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
		static int limit = 5;
		if (limit) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "%s:%d get vblank counter failed: %s\n",
				   __FUNCTION__, __LINE__,
				   strerror(errno));
			limit--;
		}
		goto out_complete;
	}

	info->frame = vbl.reply.sequence;
	DRI2BlockClient(client, draw);

	return TRUE;

out_complete:
	DRI2WaitMSCComplete(client, draw, target_msc, 0, 0);
	return TRUE;
}
#endif

static int dri2_server_generation;

Bool sna_dri_open(struct sna *sna, ScreenPtr screen)
{
	DRI2InfoRec info;
	int dri2_major = 1;
	int dri2_minor = 0;
#if DRI2INFOREC_VERSION >= 4
	const char *driverNames[1];
#endif

	DBG(("%s()\n", __FUNCTION__));

	if (sna->kgem.wedged) {
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "cannot enable DRI2 whilst forcing software fallbacks\n");
		return FALSE;
	}

	if (xf86LoaderCheckSymbol("DRI2Version"))
		DRI2Version(&dri2_major, &dri2_minor);

	if (dri2_minor < 1) {
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "DRI2 requires DRI2 module version 1.1.0 or later\n");
		return FALSE;
	}

	if (serverGeneration != dri2_server_generation) {
	    dri2_server_generation = serverGeneration;
	    if (!sna_dri_register_frame_event_resource_types()) {
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "Cannot register DRI2 frame event resources\n");
		return FALSE;
	    }
	}
	sna->deviceName = drmGetDeviceNameFromFd(sna->kgem.fd);
	memset(&info, '\0', sizeof(info));
	info.fd = sna->kgem.fd;
	info.driverName = sna->kgem.gen < 40 ? "i915" : "i965";
	info.deviceName = sna->deviceName;

	DBG(("%s: loading dri driver '%s' [gen=%d] for device '%s'\n",
	     __FUNCTION__, info.driverName, sna->kgem.gen, info.deviceName));

	info.version = 3;
	info.CreateBuffer = sna_dri_create_buffer;
	info.DestroyBuffer = sna_dri_destroy_buffer;

	info.CopyRegion = sna_dri_copy_region;
#if DRI2INFOREC_VERSION >= 4
	{
	    info.version = 4;
	    info.ScheduleSwap = sna_dri_schedule_swap;
	    info.GetMSC = sna_dri_get_msc;
	    info.ScheduleWaitMSC = sna_dri_schedule_wait_msc;
	    info.numDrivers = 1;
	    info.driverNames = driverNames;
	    driverNames[0] = info.driverName;
#if DRI2INFOREC_VERSION >= 6
	    info.version = 6;
	    info.AsyncSwap = sna_dri_async_swap;
#endif
	}
#endif

	return DRI2ScreenInit(screen, &info);
}

void
sna_dri_wakeup(struct sna *sna)
{
	drmEventContext ctx;

	DBG(("%s\n", __FUNCTION__));

	ctx.version = DRM_EVENT_CONTEXT_VERSION;
	ctx.vblank_handler = sna_dri_vblank_handle;
	ctx.page_flip_handler = sna_dri_page_flip_handler;

	drmHandleEvent(sna->kgem.fd, &ctx);
}

void sna_dri_close(struct sna *sna, ScreenPtr screen)
{
	DBG(("%s()\n", __FUNCTION__));
	DRI2CloseScreen(screen);
	drmFree(sna->deviceName);
}
