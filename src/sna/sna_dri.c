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

#if DEBUG_DRI
#undef DBG
#define DBG(x) ErrorF x
#endif

struct sna_dri2_private {
	int refcnt;
	PixmapPtr pixmap;
	struct kgem_bo *bo;
	unsigned int attachment;
};

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

#if DRI2INFOREC_VERSION < 2
static DRI2BufferPtr
sna_dri2_create_buffers(DrawablePtr drawable, unsigned int *attachments,
		      int count)
{
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	DRI2BufferPtr buffers;
	struct sna_dri2_private *privates;
	int depth = -1;
	int i;

	buffers = calloc(count, sizeof *buffers);
	if (buffers == NULL)
		return NULL;
	privates = calloc(count, sizeof *privates);
	if (privates == NULL) {
		free(buffers);
		return NULL;
	}

	for (i = 0; i < count; i++) {
		PixmapPtr pixmap = NULL;
		if (attachments[i] == DRI2BufferFrontLeft) {
			pixmap = get_drawable_pixmap(drawable);
			pixmap->refcnt++;
			bo = sna_pixmap_set_dri(sna, pixmap);
		} else if (attachments[i] == DRI2BufferBackLeft) {
			pixmap = screen->CreatePixmap(screen,
						      drawable->width, drawable->height, drawable->depth,
						      0);
			if (!pixmap)
				goto unwind;

			bo = sna_pixmap_set_dri(sna, pixmap);
		} else if (attachments[i] == DRI2BufferStencil && depth != -1) {
			buffers[i] = buffers[depth];
			buffers[i].attachment = attachments[i];
			privates[depth].refcnt++;
			continue;
		} else {
			unsigned int tiling = I915_TILING_X;
			if (SUPPORTS_YTILING(intel)) {
				switch (attachment) {
				case DRI2BufferDepth:
				case DRI2BufferDepthStencil:
					tiling = I915_TILING_Y;
					break;
				}
			}

			bo = kgem_create_2d(&intel->kgem,
					    drawable->width,
					    drawable->height,
					    32, tiling);
			if (!bo)
				goto unwind;
		}

		if (attachments[i] == DRI2BufferDepth)
			depth = i;

		buffers[i].attachment = attachments[i];
		buffers[i].pitch = pitch;
		buffers[i].cpp = bpp / 8;
		buffers[i].driverPrivate = &privates[i];
		buffers[i].flags = 0;	/* not tiled */
		buffers[i].name = kgem_bo_flink(&intel->kgem, bo);
		privates[i].refcnt = 1;
		privates[i].pixmap = pixmap;
		privates[i].bo = bo;
		privates[i].attachment = attachments[i];

		if (buffers[i].name == 0)
			goto unwind;
	}

	return buffers;

unwind:
	do {
		if (--privates[i].refcnt == 0) {
			if (privates[i].pixmap)
				screen->DestroyPixmap(privates[i].pixmap);
			else
				gem_close(privates[i].handle);
		}
	} while (i--);
	free(privates);
	free(buffers);
	return NULL;
}

static void
sna_dri2_destroy_buffers(DrawablePtr drawable, DRI2BufferPtr buffers, int count)
{
	ScreenPtr screen = drawable->pScreen;
	sna_dri2_private *private;
	int i;

	for (i = 0; i < count; i++) {
		private = buffers[i].driverPrivate;
		if (private->pixmap)
			screen->DestroyPixmap(private->pixmap);
		else
			kgem_delete(&intel->kgem, private->bo);
	}

	if (buffers) {
		free(buffers[0].driverPrivate);
		free(buffers);
	}
}

#else

static DRI2Buffer2Ptr
sna_dri2_create_buffer(DrawablePtr drawable, unsigned int attachment,
		       unsigned int format)
{
	ScreenPtr screen = drawable->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	DRI2Buffer2Ptr buffer;
	struct sna_dri2_private *private;
	PixmapPtr pixmap;
	struct kgem_bo *bo;
	int bpp, usage;

	DBG(("%s(attachment=%d, format=%d)\n",
	     __FUNCTION__, attachment, format));

	buffer = calloc(1, sizeof *buffer + sizeof *private);
	if (buffer == NULL)
		return NULL;
	private = (struct sna_dri2_private *)(buffer + 1);

	pixmap = NULL;
	usage = CREATE_PIXMAP_USAGE_SCRATCH;
	switch (attachment) {
	case DRI2BufferFrontLeft:
		pixmap = get_drawable_pixmap(drawable);
		pixmap->refcnt++;
		bo = sna_pixmap_set_dri(sna, pixmap);
		bpp = pixmap->drawable.bitsPerPixel;
		break;

	case DRI2BufferFakeFrontLeft:
	case DRI2BufferFakeFrontRight:
		usage = 0;
	case DRI2BufferFrontRight:
	case DRI2BufferBackLeft:
	case DRI2BufferBackRight:
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

	default:
		bpp = format ? format : drawable->bitsPerPixel,
		bo = kgem_create_2d(&sna->kgem,
				    drawable->width, drawable->height, bpp,
				    //sna->kgem.gen >= 40 ? I915_TILING_Y : I915_TILING_X,
				    I915_TILING_Y,
				    CREATE_EXACT);
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
	private->attachment = attachment;

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

static void sna_dri2_destroy_buffer(DrawablePtr drawable, DRI2Buffer2Ptr buffer)
{
	if (buffer && buffer->driverPrivate) {
		struct sna_dri2_private *private = buffer->driverPrivate;
		if (--private->refcnt == 0) {
			if (private->pixmap) {
				ScreenPtr screen = private->pixmap->drawable.pScreen;
				screen->DestroyPixmap(private->pixmap);
			} else {
				struct sna *sna = to_sna_from_drawable(drawable);
				kgem_bo_destroy(&sna->kgem, private->bo);
			}

			free(buffer);
		}
	} else
		free(buffer);
}

#endif

static void sna_dri2_reference_buffer(DRI2Buffer2Ptr buffer)
{
	if (buffer) {
		struct sna_dri2_private *private = buffer->driverPrivate;
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
sna_dri2_copy_region(DrawablePtr drawable, RegionPtr region,
		     DRI2BufferPtr destBuffer, DRI2BufferPtr sourceBuffer)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	struct sna_dri2_private *srcPrivate = sourceBuffer->driverPrivate;
	struct sna_dri2_private *dstPrivate = destBuffer->driverPrivate;
	ScreenPtr screen = drawable->pScreen;
	DrawablePtr src = (srcPrivate->attachment == DRI2BufferFrontLeft)
		? drawable : &srcPrivate->pixmap->drawable;
	DrawablePtr dst = (dstPrivate->attachment == DRI2BufferFrontLeft)
		? drawable : &dstPrivate->pixmap->drawable;
	GCPtr gc;
	bool flush = false;

	DBG(("%s(region=(%d, %d), (%d, %d)))\n", __FUNCTION__,
	     region ? REGION_EXTENTS(NULL, region)->x1 : 0,
	     region ? REGION_EXTENTS(NULL, region)->y1 : 0,
	     region ? REGION_EXTENTS(NULL, region)->x2 : dst->width,
	     region ? REGION_EXTENTS(NULL, region)->y2 : dst->height));

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

	/* Invalidate src to reflect unknown modifications made by the client */
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
sna_dri2_get_pipe(DrawablePtr pDraw)
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

	crtc = sna_covering_crtc(pScrn, &box, NULL, &crtcbox);

	/* Make sure the CRTC is valid and this is the real front buffer */
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
sna_dri2_frame_event_client_gone(void *data, XID id)
{
	DRI2FrameEventPtr frame_event = data;

	frame_event->client = NULL;
	frame_event->client_id = None;
	return Success;
}

static int
sna_dri2_frame_event_drawable_gone(void *data, XID id)
{
	DRI2FrameEventPtr frame_event = data;

	frame_event->drawable_id = None;
	return Success;
}

static Bool
sna_dri2_register_frame_event_resource_types(void)
{
	frame_event_client_type =
		CreateNewResourceType(sna_dri2_frame_event_client_gone,
				      "Frame Event Client");
	if (!frame_event_client_type)
		return FALSE;

	frame_event_drawable_type =
		CreateNewResourceType(sna_dri2_frame_event_drawable_gone,
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
sna_dri2_add_frame_event(DRI2FrameEventPtr frame_event)
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
sna_dri2_del_frame_event(DRI2FrameEventPtr frame_event)
{
	if (frame_event->client_id)
		FreeResourceByType(frame_event->client_id,
				   frame_event_client_type,
				   TRUE);

	if (frame_event->drawable_id)
		FreeResourceByType(frame_event->drawable_id,
				   frame_event_drawable_type,
				   TRUE);
}

static void
sna_dri2_exchange_buffers(DrawablePtr draw,
			  DRI2BufferPtr front, DRI2BufferPtr back)
{
	struct sna_dri2_private *front_priv, *back_priv;
	struct sna_pixmap *front_sna, *back_sna;
	struct kgem_bo *bo;
	int tmp;

	DBG(("%s()\n", __FUNCTION__));

	front_priv = front->driverPrivate;
	back_priv = back->driverPrivate;

	front_sna = sna_pixmap(front_priv->pixmap);
	back_sna = sna_pixmap(back_priv->pixmap);

	/* Force a copy/readback for the next CPU access */
	if (!front_sna->gpu_only) {
		sna_damage_all(&front_sna->gpu_damage,
			       front_priv->pixmap->drawable.width,
			       front_priv->pixmap->drawable.height);
		sna_damage_destroy(&front_sna->cpu_damage);
	}
	if (front_sna->mapped) {
		munmap(front_priv->pixmap->devPrivate.ptr,
		       front_sna->gpu_bo->size);
		front_sna->mapped = false;
	}
	if (!back_sna->gpu_only) {
		sna_damage_all(&back_sna->gpu_damage,
			       back_priv->pixmap->drawable.width,
			       back_priv->pixmap->drawable.height);
		sna_damage_destroy(&back_sna->cpu_damage);
	}
	if (back_sna->mapped) {
		munmap(back_priv->pixmap->devPrivate.ptr,
		       back_sna->gpu_bo->size);
		back_sna->mapped = false;
	}

	/* Swap BO names so DRI works */
	tmp = front->name;
	front->name = back->name;
	back->name = tmp;

	/* and swap bo so future flips work */
	bo = front_priv->bo;
	front_priv->bo = back_priv->bo;
	back_priv->bo = bo;

	bo = front_sna->gpu_bo;
	front_sna->gpu_bo = back_sna->gpu_bo;
	back_sna->gpu_bo = bo;
}

/*
 * Our internal swap routine takes care of actually exchanging, blitting, or
 * flipping buffers as necessary.
 */
static Bool
sna_dri2_schedule_flip(struct sna *sna,
		       ClientPtr client, DrawablePtr draw, DRI2BufferPtr front,
		       DRI2BufferPtr back, DRI2SwapEventPtr func, void *data,
		       unsigned int target_msc)
{
	struct sna_dri2_private *back_priv;
	DRI2FrameEventPtr flip_info;

	/* Main crtc for this drawable shall finally deliver pageflip event. */
	int ref_crtc_hw_id = sna_dri2_get_pipe(draw);

	DBG(("%s()\n", __FUNCTION__));

	flip_info = calloc(1, sizeof(DRI2FrameEventRec));
	if (!flip_info)
		return FALSE;

	flip_info->drawable_id = draw->id;
	flip_info->client = client;
	flip_info->type = DRI2_SWAP;
	flip_info->event_complete = func;
	flip_info->event_data = data;
	flip_info->frame = target_msc;

	if (!sna_dri2_add_frame_event(flip_info)) {
		free(flip_info);
		return FALSE;
	}

	/* Page flip the full screen buffer */
	back_priv = back->driverPrivate;
	if (sna_do_pageflip(sna,
			    back_priv->pixmap,
			    flip_info, ref_crtc_hw_id))
		return TRUE;

	sna_dri2_del_frame_event(flip_info);
	free(flip_info);
	return FALSE;
}

static Bool
can_exchange(DRI2BufferPtr front, DRI2BufferPtr back)
{
	struct sna_dri2_private *front_priv = front->driverPrivate;
	struct sna_dri2_private *back_priv = back->driverPrivate;
	PixmapPtr front_pixmap = front_priv->pixmap;
	PixmapPtr back_pixmap = back_priv->pixmap;
	struct sna_pixmap *front_sna = sna_pixmap(front_pixmap);
	struct sna_pixmap *back_sna = sna_pixmap(back_pixmap);

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

void sna_dri2_frame_event(unsigned int frame, unsigned int tv_sec,
			  unsigned int tv_usec, DRI2FrameEventPtr swap_info)
{
	DrawablePtr drawable;
	ScreenPtr screen;
	ScrnInfoPtr scrn;
	struct sna *sna;
	int status;

	DBG(("%s(id=%d, type=%d)\n", __FUNCTION__,
	     (int)swap_info->drawable_id, swap_info->type));

	status = BadDrawable;
	if (swap_info->drawable_id)
		status = dixLookupDrawable(&drawable,
					   swap_info->drawable_id,
					   serverClient,
					   M_ANY, DixWriteAccess);
	if (status != Success)
		goto done;

	screen = drawable->pScreen;
	scrn = xf86Screens[screen->myNum];
	sna = to_sna(scrn);

	switch (swap_info->type) {
	case DRI2_FLIP:
		/* If we can still flip... */
		if (DRI2CanFlip(drawable) &&
		    !sna->shadow &&
		    can_exchange(swap_info->front, swap_info->back) &&
		    sna_dri2_schedule_flip(sna,
					   swap_info->client,
					   drawable,
					   swap_info->front,
					   swap_info->back,
					   swap_info->event_complete,
					   swap_info->event_data,
					   swap_info->frame)) {
			sna_dri2_exchange_buffers(drawable,
						  swap_info->front,
						  swap_info->back);
			break;
		}
		/* else fall through to exchange/blit */
	case DRI2_SWAP: {
		int swap_type;

		if (DRI2CanExchange(drawable) &&
		    can_exchange(swap_info->front, swap_info->back)) {
			sna_dri2_exchange_buffers(drawable,
						  swap_info->front,
						  swap_info->back);
			swap_type = DRI2_EXCHANGE_COMPLETE;
		} else {
			sna_dri2_copy_region(drawable, NULL,
					     swap_info->front,
					     swap_info->back);
			swap_type = DRI2_BLIT_COMPLETE;
		}
		DRI2SwapComplete(swap_info->client,
				 drawable, frame,
				 tv_sec, tv_usec,
				 swap_type,
				 swap_info->client ? swap_info->event_complete : NULL,
				 swap_info->event_data);
		break;
	}
	case DRI2_WAITMSC:
		if (swap_info->client)
			DRI2WaitMSCComplete(swap_info->client, drawable,
					    frame, tv_sec, tv_usec);
		break;
	default:
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "%s: unknown vblank event received\n", __func__);
		/* Unknown type */
		break;
	}

done:
	sna_dri2_del_frame_event(swap_info);
	sna_dri2_destroy_buffer(drawable, swap_info->front);
	sna_dri2_destroy_buffer(drawable, swap_info->back);
	free(swap_info);
}

void sna_dri2_flip_event(unsigned int frame, unsigned int tv_sec,
			 unsigned int tv_usec, DRI2FrameEventPtr flip)
{
	DrawablePtr drawable;
	ScreenPtr screen;
	ScrnInfoPtr scrn;
	int status;

	DBG(("%s(frame=%d, tv=%d.%06d, type=%d)\n",
	     __FUNCTION__, frame, tv_sec, tv_usec, flip->type));

	if (!flip->drawable_id)
		status = BadDrawable;
	else
		status = dixLookupDrawable(&drawable,
					   flip->drawable_id,
					   serverClient,
					   M_ANY, DixWriteAccess);
	if (status != Success) {
		sna_dri2_del_frame_event(flip);
		free(flip);
		return;
	}

	screen = drawable->pScreen;
	scrn = xf86Screens[screen->myNum];

	/* We assume our flips arrive in order, so we don't check the frame */
	switch (flip->type) {
	case DRI2_SWAP:
		/* Check for too small vblank count of pageflip completion, taking wraparound
		 * into account. This usually means some defective kms pageflip completion,
		 * causing wrong (msc, ust) return values and possible visual corruption.
		 */
		if ((frame < flip->frame) && (flip->frame - frame < 5)) {
			static int limit = 5;

			/* XXX we are currently hitting this path with older
			 * kernels, so make it quieter.
			 */
			if (limit) {
				xf86DrvMsg(scrn->scrnIndex, X_WARNING,
					   "%s: Pageflip completion has impossible msc %d < target_msc %d\n",
					   __func__, frame, flip->frame);
				limit--;
			}

			/* All-0 values signal timestamping failure. */
			frame = tv_sec = tv_usec = 0;
		}

		DBG(("%s: swap complete\n", __FUNCTION__));
		DRI2SwapComplete(flip->client, drawable, frame, tv_sec, tv_usec,
				 DRI2_FLIP_COMPLETE, flip->client ? flip->event_complete : NULL,
				 flip->event_data);
		break;
	case DRI2_ASYNC_SWAP:
		DBG(("%s: asunc swap flip completed\n", __FUNCTION__));
		to_sna(scrn)->mode.flip_pending[flip->pipe]--;
		break;
	default:
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "%s: unknown vblank event received\n", __func__);
		/* Unknown type */
		break;
	}

	sna_dri2_del_frame_event(flip);
	free(flip);
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
sna_dri2_schedule_swap(ClientPtr client, DrawablePtr draw, DRI2BufferPtr front,
		       DRI2BufferPtr back, CARD64 *target_msc, CARD64 divisor,
		       CARD64 remainder, DRI2SwapEventPtr func, void *data)
{
	ScreenPtr screen = draw->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	drmVBlank vbl;
	int ret, pipe = sna_dri2_get_pipe(draw), flip = 0;
	DRI2FrameEventPtr swap_info = NULL;
	enum DRI2FrameEventType swap_type = DRI2_SWAP;
	CARD64 current_msc;

	DBG(("%s()\n", __FUNCTION__));

	/* Drawable not displayed... just complete the swap */
	if (pipe == -1)
		goto blit_fallback;

	/* Truncate to match kernel interfaces; means occasional overflow
	 * misses, but that's generally not a big deal */
	*target_msc &= 0xffffffff;
	divisor &= 0xffffffff;
	remainder &= 0xffffffff;

	swap_info = calloc(1, sizeof(DRI2FrameEventRec));
	if (!swap_info)
		goto blit_fallback;

	swap_info->drawable_id = draw->id;
	swap_info->client = client;
	swap_info->event_complete = func;
	swap_info->event_data = data;
	swap_info->front = front;
	swap_info->back = back;

	if (!sna_dri2_add_frame_event(swap_info)) {
		free(swap_info);
		swap_info = NULL;
		goto blit_fallback;
	}

	sna_dri2_reference_buffer(front);
	sna_dri2_reference_buffer(back);

	/* Get current count */
	vbl.request.type = DRM_VBLANK_RELATIVE;
	if (pipe > 0)
		vbl.request.type |= DRM_VBLANK_SECONDARY;
	vbl.request.sequence = 0;
	ret = drmWaitVBlank(sna->kgem.fd, &vbl);
	if (ret) {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "first get vblank counter failed: %s\n",
			   strerror(errno));
		goto blit_fallback;
	}

	current_msc = vbl.reply.sequence;

	/* Flips need to be submitted one frame before */
	if (!sna->shadow && DRI2CanFlip(draw) && can_exchange(front, back)) {
		DBG(("%s: can flip\n", __FUNCTION__));
		swap_type = DRI2_FLIP;
		flip = 1;
	}

	swap_info->type = swap_type;

	/* Correct target_msc by 'flip' if swap_type == DRI2_FLIP.
	 * Do it early, so handling of different timing constraints
	 * for divisor, remainder and msc vs. target_msc works.
	 */
	if (*target_msc > 0)
		*target_msc -= flip;

	/*
	 * If divisor is zero, or current_msc is smaller than target_msc
	 * we just need to make sure target_msc passes before initiating
	 * the swap.
	 */
	if (divisor == 0 || current_msc < *target_msc) {
		vbl.request.type =  DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
		if (pipe > 0)
			vbl.request.type |= DRM_VBLANK_SECONDARY;

		/* If non-pageflipping, but blitting/exchanging, we need to use
		 * DRM_VBLANK_NEXTONMISS to avoid unreliable timestamping later
		 * on.
		 */
		if (flip == 0)
			vbl.request.type |= DRM_VBLANK_NEXTONMISS;
		if (pipe > 0)
			vbl.request.type |= DRM_VBLANK_SECONDARY;

		/* If target_msc already reached or passed, set it to
		 * current_msc to ensure we return a reasonable value back
		 * to the caller. This makes swap_interval logic more robust.
		 */
		if (current_msc >= *target_msc)
			*target_msc = current_msc;

		vbl.request.sequence = *target_msc;
		vbl.request.signal = (unsigned long)swap_info;
		ret = drmWaitVBlank(sna->kgem.fd, &vbl);
		if (ret) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "divisor 0 get vblank counter failed: %s\n",
				   strerror(errno));
			goto blit_fallback;
		}

		*target_msc = vbl.reply.sequence + flip;
		swap_info->frame = *target_msc;
		return TRUE;
	}

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

	vbl.request.sequence = current_msc - (current_msc % divisor) +
		remainder;

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

	vbl.request.signal = (unsigned long)swap_info;
	ret = drmWaitVBlank(sna->kgem.fd, &vbl);
	if (ret) {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "final get vblank counter failed: %s\n",
			   strerror(errno));
		goto blit_fallback;
	}

	/* Adjust returned value for 1 fame pageflip offset of flip > 0 */
	*target_msc = vbl.reply.sequence + flip;
	swap_info->frame = *target_msc;
	return TRUE;

blit_fallback:
	DBG(("%s -- blit\n", __FUNCTION__));
	sna_dri2_copy_region(draw, NULL, front, back);

	DRI2SwapComplete(client, draw, 0, 0, 0, DRI2_BLIT_COMPLETE, func, data);
	if (swap_info) {
		sna_dri2_del_frame_event(swap_info);
		sna_dri2_destroy_buffer(draw, swap_info->front);
		sna_dri2_destroy_buffer(draw, swap_info->back);
		free(swap_info);
	}
	*target_msc = 0; /* offscreen, so zero out target vblank count */
	return TRUE;
}

#if DRI2INFOREC_VERSION >= 6
static void
sna_dri2_async_swap(ClientPtr client, DrawablePtr draw,
		  DRI2BufferPtr front, DRI2BufferPtr back,
		  DRI2SwapEventPtr func, void *data)
{
	ScreenPtr screen = draw->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	int pipe = sna_dri2_get_pipe(draw);
	int type = DRI2_EXCHANGE_COMPLETE;

	DBG(("%s()\n", __FUNCTION__));

	/* Drawable not displayed... just complete the swap */
	if (pipe == -1)
		goto exchange;

	if (sna->shadow ||
	    !DRI2CanFlip(draw) ||
	    !can_exchange(front, back)) {
		sna_dri2_copy_region(draw, NULL, front, back);
		DRI2SwapComplete(client, draw, 0, 0, 0,
				 DRI2_BLIT_COMPLETE, func, data);
		return;
	}

	if (!sna->mode.flip_pending[pipe]) {
		DRI2FrameEventPtr info;
		struct sna_dri2_private *backPrivate = back->driverPrivate;
		DrawablePtr src = &backPrivate->pixmap->drawable;
		PixmapPtr copy;
		GCPtr gc;

		copy = screen->CreatePixmap(screen,
					    src->width, src->height, src->depth,
					    0);
		if (!copy)
			goto exchange;

		if (!sna_pixmap_force_to_gpu(copy)) {
			screen->DestroyPixmap(copy);
			goto exchange;
		}

		/* copy back to new buffer, and schedule flip */
		gc = GetScratchGC(src->depth, screen);
		if (!gc) {
			screen->DestroyPixmap(copy);
			goto exchange;
		}
		ValidateGC(src, gc);

		gc->ops->CopyArea(src, &copy->drawable, gc,
				  0, 0,
				  draw->width, draw->height,
				  0, 0);
		FreeScratchGC(gc);

		info = calloc(1, sizeof(DRI2FrameEventRec));
		if (!info) {
			screen->DestroyPixmap(copy);
			goto exchange;
		}

		info->drawable_id = draw->id;
		info->client = client;
		info->type = DRI2_ASYNC_SWAP;
		info->pipe = pipe;

		sna->mode.flip_pending[pipe]++;
		sna_do_pageflip(sna, copy, info,
				sna_dri2_get_pipe(draw));
		screen->DestroyPixmap(copy);

		type = DRI2_FLIP_COMPLETE;
	}

exchange:
	sna_dri2_exchange_buffers(draw, front, back);
	DRI2SwapComplete(client, draw, 0, 0, 0, type, func, data);
}
#endif

/*
 * Get current frame count and frame count timestamp, based on drawable's
 * crtc.
 */
static int
sna_dri2_get_msc(DrawablePtr draw, CARD64 *ust, CARD64 *msc)
{
	ScreenPtr screen = draw->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	drmVBlank vbl;
	int ret, pipe = sna_dri2_get_pipe(draw);

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
sna_dri2_schedule_wait_msc(ClientPtr client, DrawablePtr draw, CARD64 target_msc,
			   CARD64 divisor, CARD64 remainder)
{
	ScreenPtr screen = draw->pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	struct sna *sna = to_sna(scrn);
	DRI2FrameEventPtr wait_info;
	drmVBlank vbl;
	int ret, pipe = sna_dri2_get_pipe(draw);
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

	wait_info = calloc(1, sizeof(DRI2FrameEventRec));
	if (!wait_info)
		goto out_complete;

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

Bool sna_dri2_open(struct sna *sna, ScreenPtr screen)
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
	    if (!sna_dri2_register_frame_event_resource_types()) {
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

#if DRI2INFOREC_VERSION == 1
	info.version = 1;
	info.CreateBuffers = sna_dri2_create_buffers;
	info.DestroyBuffers = sna_dri2_destroy_buffers;
#elif DRI2INFOREC_VERSION == 2
	/* The ABI between 2 and 3 was broken so we could get rid of
	 * the multi-buffer alloc functions.  Make sure we indicate the
	 * right version so DRI2 can reject us if it's version 3 or above. */
	info.version = 2;
	info.CreateBuffer = sna_dri2_create_buffer;
	info.DestroyBuffer = sna_dri2_destroy_buffer;
#else
	info.version = 3;
	info.CreateBuffer = sna_dri2_create_buffer;
	info.DestroyBuffer = sna_dri2_destroy_buffer;
#endif

	info.CopyRegion = sna_dri2_copy_region;
#if DRI2INFOREC_VERSION >= 4
	{
	    info.version = 4;
	    info.ScheduleSwap = sna_dri2_schedule_swap;
	    info.GetMSC = sna_dri2_get_msc;
	    info.ScheduleWaitMSC = sna_dri2_schedule_wait_msc;
	    info.numDrivers = 1;
	    info.driverNames = driverNames;
	    driverNames[0] = info.driverName;
#if DRI2INFOREC_VERSION >= 6
	    info.version = 6;
	    info.AsyncSwap = sna_dri2_async_swap;
#endif
	}
#endif

	return DRI2ScreenInit(screen, &info);
}

void sna_dri2_close(struct sna *sna, ScreenPtr screen)
{
	DBG(("%s()\n", __FUNCTION__));
	DRI2CloseScreen(screen);
	drmFree(sna->deviceName);
}
