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
#include "xf86Priv.h"

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
	enum DRI2FrameEventType type;
	int frame;
	int pipe;
	int count;

	/* for swaps & flips only */
	DRI2SwapEventPtr event_complete;
	void *event_data;
	DRI2BufferPtr front;
	DRI2BufferPtr back;

	PixmapPtr old_front;
	uint32_t old_fb;
};

static struct sna_dri_frame_event *
to_frame_event(void *data)
{
	 return (struct sna_dri_frame_event *)((uintptr_t)data & ~1);
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

	priv->flush = 1;
	priv->gpu_bo->flush = 1;
	if (priv->gpu_bo->exec)
		sna->kgem.flush = 1;

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
		DBG(("%s: attaching to front buffer %dx%d\n",
		     __FUNCTION__,
		     pixmap->drawable.width, pixmap->drawable.height));
		break;

	case DRI2BufferBackLeft:
	case DRI2BufferBackRight:
	case DRI2BufferFrontRight:
		/* Allocate a normal window, perhaps flippable */
		usage = 0;
		if (drawable->width == sna->front->drawable.width &&
		    drawable->height == sna->front->drawable.height &&
		    drawable->bitsPerPixel == sna->front->drawable.bitsPerPixel)
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

static void damage(DrawablePtr drawable, RegionPtr region)
{
	PixmapPtr pixmap;
	struct sna_pixmap *priv;
	int16_t dx, dy;

	pixmap = get_drawable_pixmap(drawable);
	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	priv = sna_pixmap(pixmap);
	if (priv->gpu_only)
		return;

	if (region) {
		BoxPtr box;

		RegionTranslate(region,
				drawable->x + dx,
				drawable->y + dy);
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

		RegionTranslate(region,
				-(drawable->x + dx),
				-(drawable->y + dy));
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
sna_dri_copy_region(DrawablePtr drawable, RegionPtr region,
		     DRI2BufferPtr dst_buffer, DRI2BufferPtr src_buffer)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	struct sna_dri_private *srcPrivate = src_buffer->driverPrivate;
	struct sna_dri_private *dstPrivate = dst_buffer->driverPrivate;
	ScreenPtr screen = drawable->pScreen;
	DrawablePtr src = (src_buffer->attachment == DRI2BufferFrontLeft)
		? drawable : &srcPrivate->pixmap->drawable;
	DrawablePtr dst = (dst_buffer->attachment == DRI2BufferFrontLeft)
		? drawable : &dstPrivate->pixmap->drawable;
	GCPtr gc;
	bool flush = false;

	DBG(("%s(region=(%d, %d), (%d, %d)))\n", __FUNCTION__,
	     region ? REGION_EXTENTS(NULL, region)->x1 : 0,
	     region ? REGION_EXTENTS(NULL, region)->y1 : 0,
	     region ? REGION_EXTENTS(NULL, region)->x2 : dst->width,
	     region ? REGION_EXTENTS(NULL, region)->y2 : dst->height));

	DBG(("%s: dst -- attachment=%d, name=%d, handle=%d\n",
	     __FUNCTION__,
	     dst_buffer->attachment,
	     dst_buffer->name,
	     dstPrivate->bo->handle));
	DBG(("%s: src -- attachment=%d, name=%d, handle=%d\n",
	     __FUNCTION__,
	     src_buffer->attachment,
	     src_buffer->name,
	     srcPrivate->bo->handle));

	gc = GetScratchGC(dst->depth, screen);
	if (!gc)
		return;

	if (region) {
		RegionPtr clip;

		clip = REGION_CREATE(screen, NULL, 0);
		pixman_region_intersect_rect(clip, region,
					     0, 0, dst->width, dst->height);
		(*gc->funcs->ChangeClip)(gc, CT_REGION, clip, 0);
		region = clip;
	}
	ValidateGC(dst, gc);

	/* Invalidate src to reflect unknown modifications made by the client
	 *
	 * XXX But what about any conflicting shadow writes made by others
	 * between the last flush and this request? Hopefully nobody will
	 * hit that race window to find out...
	 */
	damage(src, region);

	/* Wait for the scanline to be outside the region to be copied */
	if (sna->flags & SNA_SWAP_WAIT)
		flush = sna_wait_for_scanline(sna, get_drawable_pixmap(dst),
					      NULL, region);

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
	gc->ops->CopyArea(src, dst, gc,
			  0, 0,
			  drawable->width, drawable->height,
			  0, 0);
	FreeScratchGC(gc);

	DBG(("%s: flushing? %d\n", __FUNCTION__, flush));
	if (flush) /* STAT! */
		kgem_submit(&sna->kgem);
}

#if DRI2INFOREC_VERSION >= 4


static int
sna_dri_get_pipe(DrawablePtr pDraw)
{
	ScreenPtr pScreen = pDraw->pScreen;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
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
sna_dri_frame_event_info(struct sna_dri_frame_event *info)
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

	DBG(("%s()\n", __FUNCTION__));

	assert(front->format == back->format);

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
	struct sna_dri *dri = &sna->dri;
	struct sna_dri_private *back_priv;

	/* Main crtc for this drawable shall finally deliver pageflip event. */
	int ref_crtc_hw_id = sna_dri_get_pipe(draw);

	DBG(("%s()\n", __FUNCTION__));

	dri->fe_frame = 0;
	dri->fe_tv_sec = 0;
	dri->fe_tv_usec = 0;

	/* Page flip the full screen buffer */
	back_priv = info->back->driverPrivate;
	info->count = sna_do_pageflip(sna,
				      back_priv->pixmap,
				      info, ref_crtc_hw_id,
				      &info->old_front,
				      &info->old_fb);
	return info->count != 0;
}

static Bool
can_exchange(DRI2BufferPtr front,
	     DRI2BufferPtr back)
{
	struct sna_dri_private *front_priv = front->driverPrivate;
	struct sna_dri_private *back_priv = back->driverPrivate;
	PixmapPtr front_pixmap = front_priv->pixmap;
	PixmapPtr back_pixmap = back_priv->pixmap;
	struct sna_pixmap *front_sna, *back_sna;

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

	if (front_pixmap->drawable.bitsPerPixel != back_pixmap->drawable.bitsPerPixel) {
		DBG(("%s -- no, depth mismatch: front bpp=%d, back=%d\n",
		     __FUNCTION__,
		     front_pixmap->drawable.bitsPerPixel,
		     back_pixmap->drawable.bitsPerPixel));
		return FALSE;
	}

	/* prevent an implicit tiling mode change */
	front_sna = sna_pixmap(front_pixmap);
	back_sna = sna_pixmap(back_pixmap);
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

static Bool
can_flip(struct sna * sna,
	 DrawablePtr draw,
	 DRI2BufferPtr front,
	 DRI2BufferPtr back)
{
	struct sna_dri_private *back_priv = back->driverPrivate;
	struct sna_pixmap *front_sna, *back_sna;
	WindowPtr win = (WindowPtr)draw;
	PixmapPtr front_pixmap;
	PixmapPtr back_pixmap = back_priv->pixmap;

	ScreenPtr screen = draw->pScreen;

	assert(draw->type == DRAWABLE_WINDOW);

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

	front_pixmap = screen->GetWindowPixmap(win);
	if (front_pixmap != sna->front) {
		DBG(("%s: no, window is not on the front buffer\n",
		     __FUNCTION__));
		return FALSE;
	}

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

	if (front_pixmap->drawable.bitsPerPixel != back_pixmap->drawable.bitsPerPixel) {
		DBG(("%s -- no, depth mismatch: front bpp=%d, back=%d\n",
		     __FUNCTION__,
		     front_pixmap->drawable.bitsPerPixel,
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
	struct sna_dri_frame_event *swap_info = data;
	DrawablePtr draw;
	ScreenPtr screen;
	ScrnInfoPtr scrn;
	struct sna *sna;
	int status;

	DBG(("%s(id=%d, type=%d)\n", __FUNCTION__,
	     (int)swap_info->drawable_id, swap_info->type));

	status = BadDrawable;
	if (swap_info->drawable_id)
		status = dixLookupDrawable(&draw,
					   swap_info->drawable_id,
					   serverClient,
					   M_ANY, DixWriteAccess);
	if (status != Success)
		goto done;

	screen = draw->pScreen;
	scrn = xf86Screens[screen->myNum];
	sna = to_sna(scrn);

	switch (swap_info->type) {
	case DRI2_FLIP:
		/* If we can still flip... */
		if (can_flip(sna, draw,
			     swap_info->front, swap_info->back) &&
		    sna_dri_schedule_flip(sna, draw, swap_info)) {
			sna_dri_exchange_buffers(draw,
						 swap_info->front,
						 swap_info->back);
			return;
		}
		/* else fall through to exchange/blit */
	case DRI2_SWAP: {
		int swap_type;

		if (DRI2CanExchange(draw) &&
		    can_exchange(swap_info->front, swap_info->back)) {
			sna_dri_exchange_buffers(draw,
						 swap_info->front,
						 swap_info->back);
			swap_type = DRI2_EXCHANGE_COMPLETE;
		} else {
			sna_dri_copy_region(draw, NULL,
					    swap_info->front,
					    swap_info->back);
			swap_type = DRI2_BLIT_COMPLETE;
		}
		DRI2SwapComplete(swap_info->client,
				 draw, frame,
				 tv_sec, tv_usec,
				 swap_type,
				 swap_info->client ? swap_info->event_complete : NULL,
				 swap_info->event_data);
		break;
	}
	case DRI2_WAITMSC:
		if (swap_info->client)
			DRI2WaitMSCComplete(swap_info->client, draw,
					    frame, tv_sec, tv_usec);
		break;
	default:
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "%s: unknown vblank event received\n", __func__);
		/* Unknown type */
		break;
	}

done:
	sna_dri_frame_event_info(swap_info);
}

static void sna_dri_flip_event(struct sna *sna,
			       struct sna_dri_frame_event *flip)
{
	DrawablePtr drawable;
	int status;

	DBG(("%s(frame=%d, tv=%d.%06d, type=%d)\n",
	     __FUNCTION__,
	     sna->dri.fe_frame,
	     sna->dri.fe_tv_sec,
	     sna->dri.fe_tv_usec,
	     flip->type));

	if (!flip->drawable_id)
		return;

	status = dixLookupDrawable(&drawable,
				   flip->drawable_id,
				   serverClient,
				   M_ANY, DixWriteAccess);
	if (status != Success)
		return;

	/* We assume our flips arrive in order, so we don't check the frame */
	switch (flip->type) {
	case DRI2_FLIP:
		/* Deliver cached msc, ust from reference crtc */
		/* Check for too small vblank count of pageflip completion, taking wraparound
		 * into account. This usually means some defective kms pageflip completion,
		 * causing wrong (msc, ust) return values and possible visual corruption.
		 */
		if ((sna->dri.fe_frame < flip->frame) &&
		    (flip->frame - sna->dri.fe_frame < 5)) {
			static int limit = 5;

			/* XXX we are currently hitting this path with older
			 * kernels, so make it quieter.
			 */
			if (limit) {
				xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
					   "%s: Pageflip completion has impossible msc %d < target_msc %d\n",
					   __func__, sna->dri.fe_frame, flip->frame);
				limit--;
			}

			/* All-0 values signal timestamping failure. */
			sna->dri.fe_frame = sna->dri.fe_tv_sec = sna->dri.fe_tv_usec = 0;
		}

		DBG(("%s: flip complete\n", __FUNCTION__));
		DRI2SwapComplete(flip->client, drawable,
				 sna->dri.fe_frame,
				 sna->dri.fe_tv_sec,
				 sna->dri.fe_tv_usec,
				 DRI2_FLIP_COMPLETE,
				 flip->client ? flip->event_complete : NULL,
				 flip->event_data);
		break;

	case DRI2_ASYNC_SWAP:
		DBG(("%s: async swap flip completed on pipe %d, pending %d\n",
		     __FUNCTION__, flip->pipe, sna->dri.flip_pending[flip->pipe]));
		sna->dri.flip_pending[flip->pipe]--;
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
	struct sna_dri *dri = &info->sna->dri;

	DBG(("%s: pending flip_count=%d\n", __FUNCTION__, info->count));

	/* Is this the event whose info shall be delivered to higher level? */
	if ((uintptr_t)data & 1) {
		/* Yes: Cache msc, ust for later delivery. */
		dri->fe_frame = frame;
		dri->fe_tv_sec = tv_sec;
		dri->fe_tv_usec = tv_usec;
	}

	if (--info->count)
		return;

	sna_dri_flip_event(info->sna, info);

	sna_mode_delete_fb(info->sna, info->old_front, info->old_fb);
	sna_dri_frame_event_info(info);
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
	struct sna_dri_frame_event *info;
	enum DRI2FrameEventType swap_type = DRI2_SWAP;
	CARD64 current_msc;

	DBG(("%s(target_msc=%llu)\n", __FUNCTION__, (long long)*target_msc));

	/* Drawable not displayed... just complete the swap */
	pipe = sna_dri_get_pipe(draw);
	if (pipe == -1)
		goto xchg_fallback;

	/* Truncate to match kernel interfaces; means occasional overflow
	 * misses, but that's generally not a big deal */
	*target_msc &= 0xffffffff;
	divisor &= 0xffffffff;
	remainder &= 0xffffffff;

	info = calloc(1, sizeof(struct sna_dri_frame_event));
	if (!info)
		goto xchg_fallback;

	info->sna = sna;
	info->drawable_id = draw->id;
	info->client = client;
	info->event_complete = func;
	info->event_data = data;
	info->front = front;
	info->back = back;

	if (!sna_dri_add_frame_event(info)) {
		free(info);
		goto xchg_fallback;
	}

	sna_dri_reference_buffer(front);
	sna_dri_reference_buffer(back);

	flip = 0;
	if (can_flip(sna, draw, front, back)) {
		DBG(("%s: can flip\n", __FUNCTION__));
		swap_type = DRI2_FLIP;
		flip = 1;
	}

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
			sna_dri_exchange_buffers(draw, front, back);
			return TRUE;
		}

		/* Similarly, the CopyRegion blit is coupled to vsync. */
		goto blit_fallback;
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
	sna_dri_copy_region(draw, NULL, front, back);
	sna_dri_frame_event_info(info);
	swap_type = DRI2_BLIT_COMPLETE;
	goto fallback;
xchg_fallback:
	swap_type = DRI2_EXCHANGE_COMPLETE;
	sna_dri_exchange_buffers(draw, front, back);
fallback:
	DRI2SwapComplete(client, draw, 0, 0, 0, swap_type, func, data);
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
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	int pipe = sna_dri_get_pipe(draw);
	int type = DRI2_EXCHANGE_COMPLETE;

	DBG(("%s()\n", __FUNCTION__));

	/* Drawable not displayed... just complete the swap */
	if (pipe == -1)
		goto exchange;

	if (!can_flip(sna, draw, front, back)) {
		/* Do an synchronous copy instead */
		struct sna_dri_private *front_priv = front->driverPrivate;
		struct sna_dri_private *back_priv = back->driverPrivate;
		BoxRec box, *boxes;
		PixmapPtr dst;
		int n;

		DBG(("%s: fallback blit: %dx%d\n",
		     __FUNCTION__, draw->width, draw->height));

		/* XXX clipping */
		if (draw->type == DRAWABLE_PIXMAP) {
			box.x1 = box.y1 = 0;
			box.x2 = draw->width;
			box.y2 = draw->height;

			dst = front_priv->pixmap;
			boxes = &box;
			n = 1;
		} else {
			WindowPtr win = (WindowPtr)draw;

			dst = sna->front;
			boxes = REGION_RECTS(&win->clipList);
			n = REGION_NUM_RECTS(&win->clipList);
		}

		sna->render.copy_boxes(sna, GXcopy,
				       back_priv->pixmap, back_priv->bo, 0, 0,
				       dst, sna_pixmap_get_bo(dst), 0, 0,
				       boxes, n);

		DRI2SwapComplete(client, draw, 0, 0, 0,
				 DRI2_BLIT_COMPLETE, func, data);
		return;
	}

	if (!sna->dri.flip_pending[pipe]) {
		struct sna_dri_frame_event *info;
		struct sna_dri_private *back_priv = back->driverPrivate;
		struct sna_pixmap *priv;
		PixmapPtr src = back_priv->pixmap;
		PixmapPtr copy;
		BoxRec box;

		DBG(("%s: no pending flip on pipe %d, so updating scanout\n",
		     __FUNCTION__, pipe));

		copy = screen->CreatePixmap(screen,
					    src->drawable.width,
					    src->drawable.height,
					    src->drawable.depth,
					    SNA_CREATE_FB);
		if (copy == NullPixmap)
			goto exchange;

		priv = sna_pixmap_force_to_gpu(copy);
		if (priv == NULL) {
			screen->DestroyPixmap(copy);
			goto exchange;
		}

		box.x1 = box.y1 = 0;
		box.x2 = src->drawable.width;
		box.y2 = src->drawable.height;
		if (!sna->render.copy_boxes(sna, GXcopy,
					    src, back_priv->bo, 0, 0,
					    copy, priv->gpu_bo, 0, 0,
					    &box, 1)) {
			screen->DestroyPixmap(copy);
			goto exchange;
		}
		sna_damage_all(&priv->gpu_damage,
			       src->drawable.width, src->drawable.height);
		assert(priv->cpu_damage == NULL);

		info = calloc(1, sizeof(struct sna_dri_frame_event));
		if (!info) {
			screen->DestroyPixmap(copy);
			goto exchange;
		}

		info->sna = sna;
		info->drawable_id = draw->id;
		info->client = client;
		info->type = DRI2_ASYNC_SWAP;
		info->pipe = pipe;

		if (!sna_dri_add_frame_event(info)) {
			free(info);
			screen->DestroyPixmap(copy);
			goto exchange;
		}

		info->count = sna_do_pageflip(sna, copy, info, pipe,
					      &info->old_front, &info->old_fb);
		screen->DestroyPixmap(copy);

		if (info->count == 0) {
			free(info);
			goto exchange;
		}

		type = DRI2_FLIP_COMPLETE;
		sna->dri.flip_pending[pipe]++;
	}

exchange:
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
	ScreenPtr screen = draw->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	drmVBlank vbl;
	int ret, pipe = sna_dri_get_pipe(draw);

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

	ret = drmWaitVBlank(sna->kgem.fd, &vbl);
	if (ret) {
		static int limit = 5;
		if (limit) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "%s:%d get vblank counter failed: %s\n",
				   __FUNCTION__, __LINE__,
				   strerror(errno));
			limit--;
		}
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
	struct sna_dri_frame_event *wait_info;
	drmVBlank vbl;
	int ret, pipe = sna_dri_get_pipe(draw);
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

	wait_info = calloc(1, sizeof(struct sna_dri_frame_event));
	if (!wait_info)
		goto out_complete;

	wait_info->sna = sna;
	wait_info->drawable_id = draw->id;
	wait_info->client = client;
	wait_info->type = DRI2_WAITMSC;

	/* Get current count */
	vbl.request.type = DRM_VBLANK_RELATIVE;
	if (pipe > 0)
		vbl.request.type |= DRM_VBLANK_SECONDARY;
	vbl.request.sequence = 0;
	ret = drmWaitVBlank(sna->kgem.fd, &vbl);
	if (ret) {
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
		vbl.request.signal = (unsigned long)wait_info;
		ret = drmWaitVBlank(sna->kgem.fd, &vbl);
		if (ret) {
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

	vbl.request.signal = (unsigned long)wait_info;
	ret = drmWaitVBlank(sna->kgem.fd, &vbl);
	if (ret) {
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

	wait_info->frame = vbl.reply.sequence;
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
