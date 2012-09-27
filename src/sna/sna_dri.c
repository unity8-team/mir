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

#include <errno.h>
#include <string.h>

#include "sna.h"
#include "sna_reg.h"

#include <xf86drm.h>
#include <i915_drm.h>
#include <dri2.h>

#if DRI2INFOREC_VERSION <= 2
#error DRI2 version supported by the Xserver is too old
#endif

#if DRI2INFOREC_VERSION < 10
#define USE_ASYNC_SWAP 0
#endif

#define COLOR_PREFER_TILING_Y 0

enum frame_event_type {
	DRI2_SWAP,
	DRI2_SWAP_WAIT,
	DRI2_SWAP_THROTTLE,
	DRI2_XCHG_THROTTLE,
	DRI2_ASYNC_FLIP,
	DRI2_FLIP,
	DRI2_FLIP_THROTTLE,
	DRI2_WAITMSC,
};

struct sna_dri_frame_event {
	DrawablePtr draw;
	ClientPtr client;
	enum frame_event_type type;
	int pipe;
	int count;

	/* for swaps & flips only */
	DRI2SwapEventPtr event_complete;
	void *event_data;
	DRI2BufferPtr front;
	DRI2BufferPtr back;
	struct kgem_bo *bo;

	struct sna_dri_frame_event *chain;

	unsigned int fe_frame;
	unsigned int fe_tv_sec;
	unsigned int fe_tv_usec;

	struct dri_bo {
		struct kgem_bo *bo;
		uint32_t name;
	} old_front, next_front, cache;

	int off_delay;
};

struct sna_dri_private {
	int refcnt;
	PixmapPtr pixmap;
	struct kgem_bo *bo;
};

static inline struct sna_dri_frame_event *
to_frame_event(uintptr_t  data)
{
	 return (struct sna_dri_frame_event *)(data & ~1);
}

static inline struct sna_dri_private *
get_private(DRI2Buffer2Ptr buffer)
{
	return (struct sna_dri_private *)(buffer+1);
}

static inline struct kgem_bo *ref(struct kgem_bo *bo)
{
	assert(bo->refcnt);
	bo->refcnt++;
	return bo;
}

/* Prefer to enable TILING_Y if this buffer will never be a
 * candidate for pageflipping
 */
static uint32_t color_tiling(struct sna *sna, DrawablePtr draw)
{
	uint32_t tiling;

	if (COLOR_PREFER_TILING_Y &&
	    (draw->width  != sna->front->drawable.width ||
	     draw->height != sna->front->drawable.height))
		tiling = I915_TILING_Y;
	else
		tiling = I915_TILING_X;

	return kgem_choose_tiling(&sna->kgem, -tiling,
				  draw->width,
				  draw->height,
				  draw->bitsPerPixel);
}

static uint32_t other_tiling(struct sna *sna, DrawablePtr draw)
{
	/* XXX Can mix color X / depth Y? */
	return kgem_choose_tiling(&sna->kgem, -I915_TILING_Y,
				  draw->width,
				  draw->height,
				  draw->bitsPerPixel);
}

static struct kgem_bo *sna_pixmap_set_dri(struct sna *sna,
					  PixmapPtr pixmap)
{
	struct sna_pixmap *priv;
	int tiling;

	DBG(("%s: attaching DRI client to pixmap=%ld\n",
	     __FUNCTION__, pixmap->drawable.serialNumber));

	priv = sna_pixmap(pixmap);
	if (priv != NULL && priv->shm) {
		DBG(("%s: SHM Pixmap, BadAlloc\n", __FUNCTION__));
		return NULL;
	}

	priv = sna_pixmap_move_to_gpu(pixmap,
				      MOVE_READ | MOVE_WRITE | __MOVE_FORCE | __MOVE_DRI);
	if (priv == NULL) {
		DBG(("%s: failed to move to GPU, BadAlloc\n", __FUNCTION__));
		return NULL;
	}

	assert(priv->cpu_damage == NULL);
	if (priv->flush++)
		return priv->gpu_bo;

	tiling = color_tiling(sna, &pixmap->drawable);
	if (tiling < 0)
		tiling = -tiling;
	if (priv->gpu_bo->tiling != tiling)
		sna_pixmap_change_tiling(pixmap, tiling);

	/* We need to submit any modifications to and reads from this
	 * buffer before we send any reply to the Client.
	 *
	 * As we don't track which Client, we flush for all.
	 */
	sna_accel_watch_flush(sna, 1);

	/* Don't allow this named buffer to be replaced */
	priv->pinned |= PIN_DRI;

	return priv->gpu_bo;
}

constant static inline void *sna_pixmap_get_buffer(PixmapPtr pixmap)
{
	assert(pixmap->refcnt);
	return ((void **)dixGetPrivateAddr(&pixmap->devPrivates, &sna_pixmap_key))[2];
}

static inline void sna_pixmap_set_buffer(PixmapPtr pixmap, void *ptr)
{
	assert(pixmap->refcnt);
	((void **)dixGetPrivateAddr(&pixmap->devPrivates, &sna_pixmap_key))[2] = ptr;
}

static DRI2Buffer2Ptr
sna_dri_create_buffer(DrawablePtr draw,
		      unsigned int attachment,
		      unsigned int format)
{
	struct sna *sna = to_sna_from_drawable(draw);
	DRI2Buffer2Ptr buffer;
	struct sna_dri_private *private;
	PixmapPtr pixmap;
	struct kgem_bo *bo;
	int bpp;

	DBG(("%s(attachment=%d, format=%d, drawable=%dx%d)\n",
	     __FUNCTION__, attachment, format, draw->width, draw->height));

	pixmap = NULL;
	switch (attachment) {
	case DRI2BufferFrontLeft:
		pixmap = get_drawable_pixmap(draw);
		buffer = sna_pixmap_get_buffer(pixmap);
		if (buffer) {
			private = get_private(buffer);

			DBG(("%s: reusing front buffer attachment, pixmap=%ld, handle=%d, name=%d\n",
			     __FUNCTION__, pixmap->drawable.serialNumber,
			     private->bo->handle, buffer->name));

			assert(private->pixmap == pixmap);
			assert(sna_pixmap(pixmap)->gpu_bo == private->bo);
			assert(kgem_bo_flink(&sna->kgem, private->bo) == buffer->name);

			private->refcnt++;
			return buffer;
		}

		bo = sna_pixmap_set_dri(sna, pixmap);
		if (bo == NULL)
			return NULL;

		bo = ref(bo);
		bpp = pixmap->drawable.bitsPerPixel;
		DBG(("%s: attaching to front buffer %dx%d [%p:%d]\n",
		     __FUNCTION__,
		     pixmap->drawable.width, pixmap->drawable.height,
		     pixmap, pixmap->refcnt));
		break;

	case DRI2BufferBackLeft:
	case DRI2BufferBackRight:
	case DRI2BufferFrontRight:
	case DRI2BufferFakeFrontLeft:
	case DRI2BufferFakeFrontRight:
		bpp = draw->bitsPerPixel;
		bo = kgem_create_2d(&sna->kgem,
				    draw->width,
				    draw->height,
				    draw->bitsPerPixel,
				    color_tiling(sna, draw),
				    CREATE_SCANOUT | CREATE_EXACT);
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
		 *
		 * The alignment for W-tiling is quite different to the
		 * nominal no-tiling case, so we have to account for
		 * the tiled access pattern explicitly.
		 *
		 * The stencil buffer is W tiled. However, we request from
		 * the kernel a non-tiled buffer because the kernel does
		 * not understand W tiling and the GTT is incapable of
		 * W fencing.
		 */
		bpp = format ? format : draw->bitsPerPixel;
		bpp *= 2;
		bo = kgem_create_2d(&sna->kgem,
				    ALIGN(draw->width, 64),
				    ALIGN((draw->height + 1) / 2, 64),
				    bpp, I915_TILING_NONE, CREATE_EXACT);
		break;

	case DRI2BufferDepth:
	case DRI2BufferDepthStencil:
	case DRI2BufferHiz:
	case DRI2BufferAccum:
		bpp = format ? format : draw->bitsPerPixel,
		bo = kgem_create_2d(&sna->kgem,
				    draw->width, draw->height, bpp,
				    other_tiling(sna, draw),
				    CREATE_EXACT);
		break;

	default:
		return NULL;
	}
	if (bo == NULL)
		return NULL;

	buffer = calloc(1, sizeof *buffer + sizeof *private);
	if (buffer == NULL)
		goto err;

	private = get_private(buffer);
	buffer->attachment = attachment;
	buffer->pitch = bo->pitch;
	buffer->cpp = bpp / 8;
	buffer->driverPrivate = private;
	buffer->format = format;
	buffer->flags = 0;
	buffer->name = kgem_bo_flink(&sna->kgem, bo);
	private->refcnt = 1;
	private->bo = bo;
	private->pixmap = pixmap;

	if (buffer->name == 0)
		goto err;

	if (pixmap) {
		assert(attachment == DRI2BufferFrontLeft);
		sna_pixmap_set_buffer(pixmap, buffer);
		assert(sna_pixmap_get_buffer(pixmap) == buffer);
		pixmap->refcnt++;
	}

	assert(bo->flush == true);

	return buffer;

err:
	kgem_bo_destroy(&sna->kgem, bo);
	free(buffer);
	return NULL;
}

static void _sna_dri_destroy_buffer(struct sna *sna, DRI2Buffer2Ptr buffer)
{
	struct sna_dri_private *private = get_private(buffer);

	if (buffer == NULL)
		return;

	DBG(("%s: %p [handle=%d] -- refcnt=%d, pixmap=%ld\n",
	     __FUNCTION__, buffer, private->bo->handle, private->refcnt,
	     private->pixmap ? private->pixmap->drawable.serialNumber : 0));

	if (--private->refcnt == 0) {
		if (private->pixmap) {
			PixmapPtr pixmap = private->pixmap;
			struct sna_pixmap *priv = sna_pixmap(pixmap);

			/* Undo the DRI markings on this pixmap */
			if (priv->flush && --priv->flush == 0) {
				DBG(("%s: releasing last DRI pixmap=%ld, scanout?=%d\n",
				     __FUNCTION__,
				     pixmap->drawable.serialNumber,
				     pixmap == sna->front));
				list_del(&priv->list);
				sna_accel_watch_flush(sna, -1);
				priv->pinned &= ~PIN_DRI;
			}

			sna_pixmap_set_buffer(pixmap, NULL);
			pixmap->drawable.pScreen->DestroyPixmap(pixmap);
		}

		private->bo->flush = 0;
		kgem_bo_destroy(&sna->kgem, private->bo);

		free(buffer);
	}
}

static void sna_dri_destroy_buffer(DrawablePtr draw, DRI2Buffer2Ptr buffer)
{
	_sna_dri_destroy_buffer(to_sna_from_drawable(draw), buffer);
}

static void sna_dri_reference_buffer(DRI2Buffer2Ptr buffer)
{
	get_private(buffer)->refcnt++;
}

static void damage(PixmapPtr pixmap, RegionPtr region)
{
	struct sna_pixmap *priv;

	priv = sna_pixmap(pixmap);
	assert(priv != NULL);
	if (DAMAGE_IS_ALL(priv->gpu_damage))
		return;

	if (region == NULL) {
damage_all:
		priv->gpu_damage = _sna_damage_all(priv->gpu_damage,
						   pixmap->drawable.width,
						   pixmap->drawable.height);
		sna_damage_destroy(&priv->cpu_damage);
		priv->undamaged = false;
	} else {
		sna_damage_subtract(&priv->cpu_damage, region);
		if (priv->cpu_damage == NULL)
			goto damage_all;
		sna_damage_add(&priv->gpu_damage, region);
	}
}

static void set_bo(PixmapPtr pixmap, struct kgem_bo *bo)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	RegionRec region;

	sna_damage_all(&priv->gpu_damage,
		       pixmap->drawable.width,
		       pixmap->drawable.height);
	sna_damage_destroy(&priv->cpu_damage);
	list_del(&priv->list);
	priv->cpu = false;
	priv->undamaged = false;

	assert(bo->refcnt);
	if (priv->gpu_bo != bo) {
		kgem_bo_destroy(&sna->kgem, priv->gpu_bo);
		priv->gpu_bo = ref(bo);
	}
	if (bo->domain != DOMAIN_GPU)
		bo->domain = DOMAIN_NONE;

	/* Post damage on the new front buffer so that listeners, such
	 * as DisplayLink know take a copy and shove it over the USB.
	 */
	region.extents.x1 = region.extents.y1 = 0;
	region.extents.x2 = pixmap->drawable.width;
	region.extents.y2 = pixmap->drawable.height;
	region.data = NULL;
	DamageRegionAppend(&pixmap->drawable, &region);
	DamageRegionProcessPending(&pixmap->drawable);
}

static void sna_dri_select_mode(struct sna *sna, struct kgem_bo *src, bool sync)
{
	struct drm_i915_gem_busy busy;
	int mode;

	if (sna->kgem.gen < 60)
		return;

	if (sync) {
		DBG(("%s: sync, force RENDER ring\n", __FUNCTION__));
		kgem_set_mode(&sna->kgem, KGEM_RENDER);
		return;
	}

	if (sna->kgem.mode != KGEM_NONE) {
		DBG(("%s: busy, not switching\n", __FUNCTION__));
		return;
	}

	if (sna->kgem.has_semaphores) {
		DBG(("%s: have sempahores, prefering RENDER\n", __FUNCTION__));
		kgem_set_mode(&sna->kgem, KGEM_RENDER);
		return;
	}

	VG_CLEAR(busy);
	busy.handle = src->handle;
	if (drmIoctl(sna->kgem.fd, DRM_IOCTL_I915_GEM_BUSY, &busy))
		return;

	DBG(("%s: src busy?=%x\n", __FUNCTION__, busy.busy));
	if (busy.busy == 0) {
		DBG(("%s: src is idle, using defaults\n", __FUNCTION__));
		return;
	}

	/* Sandybridge introduced a separate ring which it uses to
	 * perform blits. Switching rendering between rings incurs
	 * a stall as we wait upon the old ring to finish and
	 * flush its render cache before we can proceed on with
	 * the operation on the new ring.
	 *
	 * As this buffer, we presume, has just been written to by
	 * the DRI client using the RENDER ring, we want to perform
	 * our operation on the same ring, and ideally on the same
	 * ring as we will flip from (which should be the RENDER ring
	 * as well).
	 *
	 * The ultimate question is whether preserving the ring outweighs
	 * the cost of the query.
	 */
	mode = KGEM_RENDER;
	if (busy.busy & (1 << 17))
		mode = KGEM_BLT;
	_kgem_set_mode(&sna->kgem, mode);
}

static void
sna_dri_copy_fallback(struct sna *sna, int bpp,
		      struct kgem_bo *src_bo, int sx, int sy,
		      struct kgem_bo *dst_bo, int dx, int dy,
		      const BoxRec *box, int n)
{
	void *dst = kgem_bo_map__gtt(&sna->kgem, dst_bo);
	void *src = kgem_bo_map__gtt(&sna->kgem, src_bo);

	DBG(("%s: src(%d, %d), dst(%d, %d) x %d\n",
	     __FUNCTION__, sx, sy, dx, dy, n));

	do {
		memcpy_blt(src, dst, bpp,
			   src_bo->pitch, dst_bo->pitch,
			   box->x1 + sx, box->y1 + sy,
			   box->x1 + dx, box->y1 + dy,
			   box->x2 - box->x1, box->y2 - box->y1);
		box++;
	} while (--n);
}

static struct kgem_bo *
sna_dri_copy_to_front(struct sna *sna, DrawablePtr draw, RegionPtr region,
		      struct kgem_bo *dst_bo, struct kgem_bo *src_bo,
		      bool sync)
{
	PixmapPtr pixmap = get_drawable_pixmap(draw);
	pixman_region16_t clip;
	struct kgem_bo *bo = NULL;
	bool flush = false;
	xf86CrtcPtr crtc;
	BoxRec *boxes;
	int16_t dx, dy;
	int n;

	clip.extents.x1 = draw->x;
	clip.extents.y1 = draw->y;
	clip.extents.x2 = draw->x + draw->width;
	clip.extents.y2 = draw->y + draw->height;
	clip.data = NULL;

	if (region) {
		pixman_region_translate(region, draw->x, draw->y);
		pixman_region_intersect(&clip, &clip, region);
		region = &clip;

		if (!pixman_region_not_empty(region)) {
			DBG(("%s: all clipped\n", __FUNCTION__));
			return NULL;
		}
	}

	if (!wedged(sna)) {
		if (sync)
			sync = sna_pixmap_is_scanout(sna, pixmap);

		sna_dri_select_mode(sna, src_bo, sync);
	} else
		sync = false;

	dx = dy = 0;
	if (draw->type != DRAWABLE_PIXMAP) {
		WindowPtr win = (WindowPtr)draw;

		if (is_clipped(&win->clipList, draw)) {
			DBG(("%s: draw=(%d, %d), delta=(%d, %d), clip.extents=(%d, %d), (%d, %d)\n",
			     __FUNCTION__, draw->x, draw->y,
			     get_drawable_dx(draw), get_drawable_dy(draw),
			     win->clipList.extents.x1, win->clipList.extents.y1,
			     win->clipList.extents.x2, win->clipList.extents.y2));

			if (region == NULL)
				region = &clip;

			pixman_region_intersect(&clip, &win->clipList, region);
			if (!pixman_region_not_empty(&clip)) {
				DBG(("%s: all clipped\n", __FUNCTION__));
				return NULL;
			}

			region = &clip;
		}

		if (sync) {
			crtc = sna_covering_crtc(sna->scrn, &clip.extents, NULL);
			if (crtc)
				flush = sna_wait_for_scanline(sna, pixmap, crtc,
							      &clip.extents);
		}

		get_drawable_deltas(draw, pixmap, &dx, &dy);
	}

	damage(pixmap, region);
	if (region) {
		boxes = REGION_RECTS(region);
		n = REGION_NUM_RECTS(region);
		assert(n);
	} else {
		region = &clip;
		boxes = &clip.extents;
		n = 1;
	}
	if (wedged(sna)) {
		sna_dri_copy_fallback(sna, draw->bitsPerPixel,
				      src_bo, -draw->x, -draw->y,
				      dst_bo, dx, dy,
				      boxes, n);
	} else {
		sna->render.copy_boxes(sna, GXcopy,
				       (PixmapPtr)draw, src_bo, -draw->x, -draw->y,
				       pixmap, dst_bo, dx, dy,
				       boxes, n, COPY_LAST);

		DBG(("%s: flushing? %d\n", __FUNCTION__, flush));
		if (flush) { /* STAT! */
			struct kgem_request *rq = sna->kgem.next_request;
			kgem_submit(&sna->kgem);
			bo = kgem_bo_reference(rq->bo);
		}
	}

	pixman_region_translate(region, dx, dy);
	DamageRegionAppend(&pixmap->drawable, region);
	DamageRegionProcessPending(&pixmap->drawable);

	if (clip.data)
		pixman_region_fini(&clip);

	return bo;
}

static void
sna_dri_copy_from_front(struct sna *sna, DrawablePtr draw, RegionPtr region,
			struct kgem_bo *dst_bo, struct kgem_bo *src_bo,
			bool sync)
{
	PixmapPtr pixmap = get_drawable_pixmap(draw);
	pixman_region16_t clip;
	BoxRec box, *boxes;
	int16_t dx, dy;
	int n;

	box.x1 = draw->x;
	box.y1 = draw->y;
	box.x2 = draw->x + draw->width;
	box.y2 = draw->y + draw->height;

	if (region) {
		pixman_region_translate(region, draw->x, draw->y);
		pixman_region_init_rects(&clip, &box, 1);
		pixman_region_intersect(&clip, &clip, region);
		region = &clip;

		if (!pixman_region_not_empty(region)) {
			DBG(("%s: all clipped\n", __FUNCTION__));
			return;
		}
	}

	dx = dy = 0;
	if (draw->type != DRAWABLE_PIXMAP) {
		WindowPtr win = (WindowPtr)draw;

		DBG(("%s: draw=(%d, %d), delta=(%d, %d), clip.extents=(%d, %d), (%d, %d)\n",
		     __FUNCTION__, draw->x, draw->y,
		     get_drawable_dx(draw), get_drawable_dy(draw),
		     win->clipList.extents.x1, win->clipList.extents.y1,
		     win->clipList.extents.x2, win->clipList.extents.y2));

		if (region == NULL) {
			pixman_region_init_rects(&clip, &box, 1);
			region = &clip;
		}

		pixman_region_intersect(region, &win->clipList, region);
		if (!pixman_region_not_empty(region)) {
			DBG(("%s: all clipped\n", __FUNCTION__));
			return;
		}

		get_drawable_deltas(draw, pixmap, &dx, &dy);
	}

	if (region) {
		boxes = REGION_RECTS(region);
		n = REGION_NUM_RECTS(region);
		assert(n);
	} else {
		pixman_region_init_rects(&clip, &box, 1);
		region = &clip;
		boxes = &box;
		n = 1;
	}
	if (wedged(sna)) {
		sna_dri_copy_fallback(sna, draw->bitsPerPixel,
				      src_bo, dx, dy,
				      dst_bo, -draw->x, -draw->y,
				      boxes, n);
	} else {
		sna_dri_select_mode(sna, src_bo, false);
		sna->render.copy_boxes(sna, GXcopy,
				       pixmap, src_bo, dx, dy,
				       (PixmapPtr)draw, dst_bo, -draw->x, -draw->y,
				       boxes, n, COPY_LAST);
	}

	if (region == &clip)
		pixman_region_fini(&clip);
}

static void
sna_dri_copy(struct sna *sna, DrawablePtr draw, RegionPtr region,
	     struct kgem_bo *dst_bo, struct kgem_bo *src_bo,
	     bool sync)
{
	pixman_region16_t clip;
	BoxRec box, *boxes;
	int n;

	box.x1 = 0;
	box.y1 = 0;
	box.x2 = draw->width;
	box.y2 = draw->height;

	if (region) {
		pixman_region_init_rects(&clip, &box, 1);
		pixman_region_intersect(&clip, &clip, region);
		region = &clip;

		if (!pixman_region_not_empty(region)) {
			DBG(("%s: all clipped\n", __FUNCTION__));
			return;
		}

		boxes = REGION_RECTS(region);
		n = REGION_NUM_RECTS(region);
		assert(n);
	} else {
		boxes = &box;
		n = 1;
	}

	if (wedged(sna)) {
		sna_dri_copy_fallback(sna, draw->bitsPerPixel,
				      src_bo, 0, 0,
				      dst_bo, 0, 0,
				      boxes, n);
	} else {
		sna_dri_select_mode(sna, src_bo, false);
		sna->render.copy_boxes(sna, GXcopy,
				       (PixmapPtr)draw, src_bo, 0, 0,
				       (PixmapPtr)draw, dst_bo, 0, 0,
				       boxes, n, COPY_LAST);
	}

	if (region == &clip)
		pixman_region_fini(&clip);
}

static void
sna_dri_copy_region(DrawablePtr draw,
		    RegionPtr region,
		    DRI2BufferPtr dst_buffer,
		    DRI2BufferPtr src_buffer)
{
	PixmapPtr pixmap = get_drawable_pixmap(draw);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct kgem_bo *src, *dst;
	void (*copy)(struct sna *, DrawablePtr, RegionPtr,
		     struct kgem_bo *, struct kgem_bo *, bool) = sna_dri_copy;

	if (dst_buffer->attachment == DRI2BufferFrontLeft) {
		dst = sna_pixmap_get_bo(pixmap);
		copy = (void *)sna_dri_copy_to_front;
	} else
		dst = get_private(dst_buffer)->bo;

	if (src_buffer->attachment == DRI2BufferFrontLeft) {
		src = sna_pixmap_get_bo(pixmap);
		assert(copy == sna_dri_copy);
		copy = sna_dri_copy_from_front;
	} else
		src = get_private(src_buffer)->bo;

	assert(dst != NULL);
	assert(src != NULL);

	DBG(("%s: dst -- attachment=%d, name=%d, handle=%d [screen=%d]\n",
	     __FUNCTION__,
	     dst_buffer->attachment, dst_buffer->name, dst->handle,
	     sna_pixmap_get_bo(sna->front)->handle));
	DBG(("%s: src -- attachment=%d, name=%d, handle=%d\n",
	     __FUNCTION__,
	     src_buffer->attachment, src_buffer->name, src->handle));
	DBG(("%s: region (%d, %d), (%d, %d) x %d\n",
	     __FUNCTION__,
	     region->extents.x1, region->extents.y1,
	     region->extents.x2, region->extents.y2,
	     REGION_NUM_RECTS(region)));

	copy(sna, draw, region, dst, src, false);
}

static inline int sna_wait_vblank(struct sna *sna, drmVBlank *vbl)
{
	return drmIoctl(sna->kgem.fd, DRM_IOCTL_WAIT_VBLANK, vbl);
}

#if DRI2INFOREC_VERSION >= 4

static int
sna_dri_get_pipe(DrawablePtr pDraw)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pDraw->pScreen);
	xf86CrtcPtr crtc;
	BoxRec box;
	int pipe;

	if (pDraw->type == DRAWABLE_PIXMAP)
		return -1;

	box.x1 = pDraw->x;
	box.y1 = pDraw->y;
	box.x2 = box.x1 + pDraw->width;
	box.y2 = box.y1 + pDraw->height;

	crtc = sna_covering_crtc(pScrn, &box, NULL);

	/* Make sure the CRTC is valid and this is the real front buffer */
	pipe = -1;
	if (crtc != NULL)
		pipe = sna_crtc_to_pipe(crtc);

	DBG(("%s(box=((%d, %d), (%d, %d)), pipe=%d)\n",
	     __FUNCTION__, box.x1, box.y1, box.x2, box.y2, pipe));

	return pipe;
}

static struct sna_dri_frame_event *
sna_dri_window_get_chain(WindowPtr win)
{
	return ((void **)dixGetPrivateAddr(&win->devPrivates, &sna_window_key))[1];
}

static void
sna_dri_window_set_chain(WindowPtr win,
			 struct sna_dri_frame_event *chain)
{
	DBG(("%s: head now %p\n", __FUNCTION__, chain));
	assert(win->drawable.type == DRAWABLE_WINDOW);
	((void **)dixGetPrivateAddr(&win->devPrivates, &sna_window_key))[1] = chain;
}

static void
sna_dri_remove_frame_event(WindowPtr win,
			    struct sna_dri_frame_event *info)
{
	struct sna_dri_frame_event *chain;

	DBG(("%s: remove[%p] from window %ld)\n",
	     __FUNCTION__, info, (long)win->drawable.id));

	chain = sna_dri_window_get_chain(win);
	if (chain == NULL)
		return;

	if (chain == info) {
		sna_dri_window_set_chain(win, info->chain);
		return;
	}

	while (chain->chain != info)
		chain = chain->chain;
	assert(chain != info);
	assert(info->chain != chain);
	chain->chain = info->chain;
}

void sna_dri_destroy_window(WindowPtr win)
{
	struct sna_dri_frame_event *chain;

	chain = sna_dri_window_get_chain(win);
	if (chain == NULL)
		return;

	DBG(("%s: window=%ld\n", __FUNCTION__, win->drawable.serialNumber));
	while (chain) {
		chain->draw = NULL;
		chain = chain->chain;
	}
}

static void
sna_dri_add_frame_event(DrawablePtr draw, struct sna_dri_frame_event *info)
{
	struct sna_dri_frame_event *chain;

	if (draw->type != DRAWABLE_WINDOW)
		return;

	DBG(("%s: add[%p] to window %ld)\n",
	     __FUNCTION__, info, (long)draw->id));

	chain = sna_dri_window_get_chain((WindowPtr)draw);
	if (chain == NULL) {
		sna_dri_window_set_chain((WindowPtr)draw, info);
		return;
	}

	assert(chain != info);
	while (chain->chain != NULL)
		chain = chain->chain;

	assert(chain != info);
	chain->chain = info;
}

static void
sna_dri_frame_event_info_free(struct sna *sna,
			      DrawablePtr draw,
			      struct sna_dri_frame_event *info)
{
	if (draw && draw->type == DRAWABLE_WINDOW)
		sna_dri_remove_frame_event((WindowPtr)draw, info);
	_sna_dri_destroy_buffer(sna, info->front);
	_sna_dri_destroy_buffer(sna, info->back);

	if (info->old_front.bo)
		kgem_bo_destroy(&sna->kgem, info->old_front.bo);

	if (info->cache.bo)
		kgem_bo_destroy(&sna->kgem, info->cache.bo);

	if (info->bo)
		kgem_bo_destroy(&sna->kgem, info->bo);

	free(info);
}

static void
sna_dri_page_flip(struct sna *sna, struct sna_dri_frame_event *info)
{
	struct kgem_bo *bo = get_private(info->back)->bo;

	DBG(("%s()\n", __FUNCTION__));

	assert(sna_pixmap_get_buffer(sna->front) == info->front);
	assert(get_drawable_pixmap(info->draw)->drawable.height * bo->pitch <= kgem_bo_size(bo));

	info->count = sna_page_flip(sna, bo, info, info->pipe);

	info->old_front.name = info->front->name;
	info->old_front.bo = get_private(info->front)->bo;

	set_bo(sna->front, bo);

	info->front->name = info->back->name;
	get_private(info->front)->bo = bo;
}

static bool
can_flip(struct sna * sna,
	 DrawablePtr draw,
	 DRI2BufferPtr front,
	 DRI2BufferPtr back)
{
	WindowPtr win = (WindowPtr)draw;
	PixmapPtr pixmap;

	if (draw->type == DRAWABLE_PIXMAP)
		return false;

	if (!sna->scrn->vtSema) {
		DBG(("%s: no, not attached to VT\n", __FUNCTION__));
		return false;
	}

	if (sna->flags & SNA_NO_FLIP) {
		DBG(("%s: no, pageflips disabled\n", __FUNCTION__));
		return false;
	}

	if (front->format != back->format) {
		DBG(("%s: no, format mismatch, front = %d, back = %d\n",
		     __FUNCTION__, front->format, back->format));
		return false;
	}

	if (front->attachment != DRI2BufferFrontLeft) {
		DBG(("%s: no, front attachment [%d] is not FrontLeft [%d]\n",
		     __FUNCTION__,
		     front->attachment,
		     DRI2BufferFrontLeft));
		return false;
	}

	if (sna->mode.shadow_active) {
		DBG(("%s: no, shadow enabled\n", __FUNCTION__));
		return false;
	}

	pixmap = get_drawable_pixmap(draw);
	if (pixmap != sna->front) {
		DBG(("%s: no, window is not on the front buffer\n",
		     __FUNCTION__));
		return false;
	}

	DBG(("%s: window size: %dx%d, clip=(%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     win->drawable.width, win->drawable.height,
	     win->clipList.extents.x1, win->clipList.extents.y1,
	     win->clipList.extents.x2, win->clipList.extents.y2));
	if (!RegionEqual(&win->clipList, &draw->pScreen->root->winSize)) {
		DBG(("%s: no, window is clipped: clip region=(%d, %d), (%d, %d), root size=(%d, %d), (%d, %d)\n",
		     __FUNCTION__,
		     win->clipList.extents.x1,
		     win->clipList.extents.y1,
		     win->clipList.extents.x2,
		     win->clipList.extents.y2,
		     draw->pScreen->root->winSize.extents.x1,
		     draw->pScreen->root->winSize.extents.y1,
		     draw->pScreen->root->winSize.extents.x2,
		     draw->pScreen->root->winSize.extents.y2));
		return false;
	}

	if (draw->x != 0 || draw->y != 0 ||
#ifdef COMPOSITE
	    draw->x != pixmap->screen_x ||
	    draw->y != pixmap->screen_y ||
#endif
	    draw->width != pixmap->drawable.width ||
	    draw->height != pixmap->drawable.height) {
		DBG(("%s: no, window is not full size (%dx%d)!=(%dx%d)\n",
		     __FUNCTION__,
		     draw->width, draw->height,
		     pixmap->drawable.width,
		     pixmap->drawable.height));
		return false;
	}

	/* prevent an implicit tiling mode change */
	if (get_private(front)->bo->tiling != get_private(back)->bo->tiling) {
		DBG(("%s -- no, tiling mismatch: front %d, back=%d\n",
		     __FUNCTION__,
		     get_private(front)->bo->tiling,
		     get_private(back)->bo->tiling));
		return false;
	}

	return true;
}

static bool
can_exchange(struct sna * sna,
	     DrawablePtr draw,
	     DRI2BufferPtr front,
	     DRI2BufferPtr back)
{
	WindowPtr win = (WindowPtr)draw;
	PixmapPtr pixmap;

	if (draw->type == DRAWABLE_PIXMAP)
		return true;

	if (front->format != back->format) {
		DBG(("%s: no, format mismatch, front = %d, back = %d\n",
		     __FUNCTION__, front->format, back->format));
		return false;
	}

	pixmap = get_window_pixmap(win);
	if (pixmap == sna->front) {
		DBG(("%s: no, window is attached to the front buffer\n",
		     __FUNCTION__));
		return false;
	}

	if (pixmap->drawable.width != win->drawable.width ||
	    pixmap->drawable.height != win->drawable.height) {
		DBG(("%s: no, window has been reparented, window size %dx%d, parent %dx%d\n",
		     __FUNCTION__,
		     win->drawable.width,
		     win->drawable.height,
		     pixmap->drawable.width,
		     pixmap->drawable.height));
		return false;
	}

	if (sna_pixmap_get_buffer(pixmap) != front) {
		DBG(("%s: no, DRI2 drawable is no longer attached\n",
		     __FUNCTION__));
		return false;
	}

	return true;
}

inline static uint32_t pipe_select(int pipe)
{
	/* The third pipe was introduced with IvyBridge long after
	 * multiple pipe support was added to the kernel, hence
	 * we can safely ignore the capability check - if we have more
	 * than two pipes, we can assume that they are fully supported.
	 */
	if (pipe > 1)
		return pipe << DRM_VBLANK_HIGH_CRTC_SHIFT;
	else if (pipe > 0)
		return DRM_VBLANK_SECONDARY;
	else
		return 0;
}

static void
sna_dri_exchange_buffers(DrawablePtr draw,
			 DRI2BufferPtr front,
			 DRI2BufferPtr back)
{
	struct kgem_bo *back_bo, *front_bo;
	PixmapPtr pixmap;
	int tmp;

	pixmap = get_drawable_pixmap(draw);

	back_bo = get_private(back)->bo;
	front_bo = get_private(front)->bo;

	DBG(("%s: exchange front=%d/%d and back=%d/%d, pixmap=%ld %dx%d\n",
	     __FUNCTION__,
	     front_bo->handle, front->name,
	     back_bo->handle, back->name,
	     pixmap->drawable.serialNumber,
	     pixmap->drawable.width,
	     pixmap->drawable.height));

	DBG(("%s: back_bo pitch=%d, size=%d\n",
	     __FUNCTION__, back_bo->pitch, kgem_bo_size(back_bo)));
	DBG(("%s: front_bo pitch=%d, size=%d\n",
	     __FUNCTION__, front_bo->pitch, kgem_bo_size(front_bo)));

	assert(sna_pixmap_get_buffer(pixmap) == front);
	assert(pixmap->drawable.height * back_bo->pitch <= kgem_bo_size(back_bo));
	assert(pixmap->drawable.height * front_bo->pitch <= kgem_bo_size(front_bo));

	set_bo(pixmap, back_bo);

	get_private(front)->bo = back_bo;
	get_private(back)->bo = front_bo;

	tmp = front->name;
	front->name = back->name;
	back->name = tmp;
}

static void chain_swap(struct sna *sna,
		       DrawablePtr draw,
		       struct drm_event_vblank *event,
		       struct sna_dri_frame_event *chain)
{
	drmVBlank vbl;
	int type;

	assert(chain == sna_dri_window_get_chain((WindowPtr)draw));
	DBG(("%s: chaining type=%d\n", __FUNCTION__, chain->type));
	switch (chain->type) {
	case DRI2_XCHG_THROTTLE:
	case DRI2_SWAP_THROTTLE:
		break;
	default:
		return;
	}

	if (chain->type == DRI2_XCHG_THROTTLE &&
	    can_exchange(sna, draw, chain->front, chain->back)) {
		DBG(("%s: performing chained exchange\n", __FUNCTION__));
		sna_dri_exchange_buffers(draw, chain->front, chain->back);
		type = DRI2_EXCHANGE_COMPLETE;
	} else {
		DBG(("%s: emitting chained vsync'ed blit\n", __FUNCTION__));

		chain->bo = sna_dri_copy_to_front(sna, draw, NULL,
						  get_private(chain->front)->bo,
						  get_private(chain->back)->bo,
						  true);

		type = DRI2_BLIT_COMPLETE;
	}

	DRI2SwapComplete(chain->client, draw,
			 event->sequence, event->tv_sec, event->tv_usec,
			 type, chain->client ? chain->event_complete : NULL, chain->event_data);

	VG_CLEAR(vbl);
	vbl.request.type =
		DRM_VBLANK_RELATIVE |
		DRM_VBLANK_NEXTONMISS |
		DRM_VBLANK_EVENT |
		pipe_select(chain->pipe);
	vbl.request.sequence = 0;
	vbl.request.signal = (unsigned long)chain;
	if (sna_wait_vblank(sna, &vbl))
		sna_dri_frame_event_info_free(sna, draw, chain);
}

static bool sna_dri_blit_complete(struct sna *sna,
				  struct sna_dri_frame_event *info)
{
	if (info->bo && __kgem_bo_is_busy(&sna->kgem, info->bo)) {
		drmVBlank vbl;

		DBG(("%s: vsync'ed blit is still busy, postponing\n",
		     __FUNCTION__));

		VG_CLEAR(vbl);
		vbl.request.type =
			DRM_VBLANK_RELATIVE |
			DRM_VBLANK_EVENT |
			pipe_select(info->pipe);
		vbl.request.sequence = 1;
		vbl.request.signal = (unsigned long)info;
		if (!sna_wait_vblank(sna, &vbl))
			return false;
	}

	return true;
}

void sna_dri_vblank_handler(struct sna *sna, struct drm_event_vblank *event)
{
	struct sna_dri_frame_event *info = (void *)(uintptr_t)event->user_data;
	DrawablePtr draw;

	DBG(("%s(type=%d)\n", __FUNCTION__, info->type));

	draw = info->draw;
	if (draw == NULL)
		goto done;

	switch (info->type) {
	case DRI2_FLIP:
		/* If we can still flip... */
		if (can_flip(sna, draw, info->front, info->back)) {
			sna_dri_page_flip(sna, info);
			info->back->name = info->old_front.name;
			get_private(info->back)->bo = info->old_front.bo;
			info->old_front.bo = NULL;
			return;
		}
		/* else fall through to blit */
	case DRI2_SWAP:
		info->bo = sna_dri_copy_to_front(sna, draw, NULL,
						 get_private(info->front)->bo,
						 get_private(info->back)->bo,
						 true);
		info->type = DRI2_SWAP_WAIT;
		/* fall through to SwapComplete */
	case DRI2_SWAP_WAIT:
		if (!sna_dri_blit_complete(sna, info))
			return;

		DRI2SwapComplete(info->client,
				 draw, event->sequence,
				 event->tv_sec, event->tv_usec,
				 DRI2_BLIT_COMPLETE,
				 info->client ? info->event_complete : NULL,
				 info->event_data);
		break;

	case DRI2_SWAP_THROTTLE:
		DBG(("%s: %d complete, frame=%d tv=%d.%06d\n",
		     __FUNCTION__, info->type,
		     event->sequence, event->tv_sec, event->tv_usec));
		break;

	case DRI2_XCHG_THROTTLE:
		DBG(("%s: xchg throttle\n", __FUNCTION__));
		break;

	case DRI2_WAITMSC:
		DRI2WaitMSCComplete(info->client, draw,
				    event->sequence,
				    event->tv_sec,
				    event->tv_usec);
		break;
	default:
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "%s: unknown vblank event received\n", __func__);
		/* Unknown type */
		break;
	}

	if (info->chain) {
		sna_dri_remove_frame_event((WindowPtr)draw, info);
		chain_swap(sna, draw, event, info->chain);
		draw = NULL;
	}

done:
	sna_dri_frame_event_info_free(sna, draw, info);
}

static void
sna_dri_flip_continue(struct sna *sna, struct sna_dri_frame_event *info)
{
	struct dri_bo tmp;

	DBG(("%s()\n", __FUNCTION__));

	assert(sna_pixmap_get_buffer(get_drawable_pixmap(info->draw)) == info->front);

	tmp = info->old_front;

	sna_dri_page_flip(sna, info);

	get_private(info->back)->bo = tmp.bo;
	info->back->name = tmp.name;

	info->next_front.name = 0;
}

static void sna_dri_flip_event(struct sna *sna,
			       struct sna_dri_frame_event *flip)
{
	DBG(("%s(frame=%d, tv=%d.%06d, type=%d)\n",
	     __FUNCTION__,
	     flip->fe_frame,
	     flip->fe_tv_sec,
	     flip->fe_tv_usec,
	     flip->type));

	if (sna->dri.flip_pending == flip)
		sna->dri.flip_pending = NULL;

	/* We assume our flips arrive in order, so we don't check the frame */
	switch (flip->type) {
	case DRI2_FLIP:
		DBG(("%s: flip complete (drawable gone? %d)\n",
		     __FUNCTION__, flip->draw == NULL));
		if (flip->draw)
			DRI2SwapComplete(flip->client, flip->draw,
					 flip->fe_frame,
					 flip->fe_tv_sec,
					 flip->fe_tv_usec,
					 DRI2_FLIP_COMPLETE,
					 flip->client ? flip->event_complete : NULL,
					 flip->event_data);

		sna_dri_frame_event_info_free(sna, flip->draw, flip);
		break;

	case DRI2_FLIP_THROTTLE:
		if (!flip->next_front.name) {
			DBG(("%s: flip chain complete\n", __FUNCTION__));
			sna_dri_frame_event_info_free(sna, flip->draw, flip);
		} else if (flip->draw &&
			   can_flip(sna, flip->draw, flip->front, flip->back)) {
			sna_dri_flip_continue(sna, flip);
			DRI2SwapComplete(flip->client, flip->draw,
					 0, 0, 0,
					 DRI2_FLIP_COMPLETE,
					 flip->client ? flip->event_complete : NULL,
					 flip->event_data);
			if (flip->count)
				sna->dri.flip_pending = flip;
			else
				sna_dri_frame_event_info_free(sna, flip->draw, flip);
		} else {
			DBG(("%s: no longer able to flip\n", __FUNCTION__));

			if (flip->draw) {
				flip->bo = sna_dri_copy_to_front(sna, flip->draw, NULL,
								 get_private(flip->front)->bo,
								 get_private(flip->back)->bo,
								 false);
				DRI2SwapComplete(flip->client, flip->draw,
						 0, 0, 0,
						 DRI2_BLIT_COMPLETE,
						 flip->client ? flip->event_complete : NULL,
						 flip->event_data);
			}

			sna_dri_frame_event_info_free(sna, flip->draw, flip);
		}
		break;

#if USE_ASYNC_SWAP
	case DRI2_ASYNC_FLIP:
		DBG(("%s: async swap flip completed on pipe %d, pending? %d, new? %d\n",
		     __FUNCTION__, flip->pipe,
		     sna->dri.flip_pending != NULL,
		     flip->front->name != flip->old_front.name));

		if (sna->dri.flip_pending)
			goto finish_async_flip;

		if (flip->front->name != flip->next_front.name) {
			DBG(("%s: async flip continuing\n", __FUNCTION__));

			flip->cache = flip->old_front;
			flip->old_front = flip->next_front;

			flip->count = sna_page_flip(sna,
						    get_private(flip->front)->bo,
						    flip, flip->pipe);
			if (flip->count == 0)
				goto finish_async_flip;

			flip->next_front.bo = get_private(flip->front)->bo;
			flip->next_front.name = flip->front->name;
			flip->off_delay = 5;

			sna->dri.flip_pending = flip;
		} else if (flip->draw && --flip->off_delay) {
			DBG(("%s: queuing no-flip [delay=%d]\n",
			     __FUNCTION__, flip->off_delay));
			/* Just queue a no-op flip to trigger another event */
			flip->count = sna_page_flip(sna,
						    get_private(flip->front)->bo,
						    flip, flip->pipe);
			if (flip->count == 0)
				goto finish_async_flip;

			sna->dri.flip_pending = flip;
		} else {
finish_async_flip:
			DBG(("%s: async flip completed\n", __FUNCTION__));
			sna_dri_frame_event_info_free(sna, flip->draw, flip);
		}
		break;
#endif

	default:
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "%s: unknown vblank event received\n", __func__);
		/* Unknown type */
		break;
	}
}

void
sna_dri_page_flip_handler(struct sna *sna,
			  struct drm_event_vblank *event)
{
	struct sna_dri_frame_event *info = to_frame_event(event->user_data);

	DBG(("%s: pending flip_count=%d\n", __FUNCTION__, info->count));

	/* Is this the event whose info shall be delivered to higher level? */
	if (event->user_data & 1) {
		info->fe_frame = event->sequence;
		info->fe_tv_sec = event->tv_sec;
		info->fe_tv_usec = event->tv_usec;
	}

	if (--info->count)
		return;

	sna_dri_flip_event(sna, info);
}

static bool
sna_dri_schedule_flip(ClientPtr client, DrawablePtr draw, DRI2BufferPtr front,
		      DRI2BufferPtr back, CARD64 *target_msc, CARD64 divisor,
		      CARD64 remainder, DRI2SwapEventPtr func, void *data)
{
	struct sna *sna = to_sna_from_drawable(draw);
	struct sna_dri_frame_event *info;
	drmVBlank vbl;
	int pipe;
	CARD64 current_msc;

	DBG(("%s(target_msc=%llu, divisor=%llu, remainder=%llu)\n",
	     __FUNCTION__,
	     (long long)*target_msc,
	     (long long)divisor,
	     (long long)remainder));

	VG_CLEAR(vbl);

	pipe = sna_dri_get_pipe(draw);
	if (pipe == -1) {
		/* XXX WARN_ON(sna->dri.flip_pending) ? */
		if (sna->dri.flip_pending == NULL) {
			sna_dri_exchange_buffers(draw, front, back);
			DRI2SwapComplete(client, draw, 0, 0, 0,
					DRI2_EXCHANGE_COMPLETE, func, data);
			return true;
		} else
			return false;
	}

	/* Truncate to match kernel interfaces; means occasional overflow
	 * misses, but that's generally not a big deal */
	divisor &= 0xffffffff;
	if (divisor == 0) {
		int type = DRI2_FLIP_THROTTLE;

		DBG(("%s: performing immediate swap on pipe %d, pending? %d\n",
		     __FUNCTION__, pipe, sna->dri.flip_pending != NULL));

		info = sna->dri.flip_pending;
		if (info) {
			if (info->draw == draw) {
				DBG(("%s: chaining flip\n", __FUNCTION__));
				info->next_front.name = 1;
				return true;
			} else {
				/* We need to first wait (one vblank) for the
				 * async flips to complete before this client
				 * can take over.
				 */
				DBG(("%s: queueing flip after pending completion\n",
				     __FUNCTION__));
				type = DRI2_FLIP;
			}
		}

		info = calloc(1, sizeof(struct sna_dri_frame_event));
		if (info == NULL)
			return false;

		info->type = type;

		info->draw = draw;
		info->client = client;
		info->event_complete = func;
		info->event_data = data;
		info->front = front;
		info->back = back;
		info->pipe = pipe;

		sna_dri_add_frame_event(draw, info);
		sna_dri_reference_buffer(front);
		sna_dri_reference_buffer(back);

		sna_dri_page_flip(sna, info);

		if (info->count == 0) {
			info->back->name = info->old_front.name;
			get_private(info->back)->bo = info->old_front.bo;
			info->old_front.bo = NULL;

			DRI2SwapComplete(info->client, draw, 0, 0, 0,
					 DRI2_EXCHANGE_COMPLETE,
					 info->event_complete,
					 info->event_data);
			sna_dri_frame_event_info_free(sna, draw, info);
		} else if (type != DRI2_FLIP) {
			get_private(info->back)->bo =
				kgem_create_2d(&sna->kgem,
					       draw->width,
					       draw->height,
					       draw->bitsPerPixel,
					       get_private(info->front)->bo->tiling,
					       CREATE_SCANOUT | CREATE_EXACT);
			info->back->name = kgem_bo_flink(&sna->kgem,
							 get_private(info->back)->bo);
			sna->dri.flip_pending = info;

			DRI2SwapComplete(info->client, draw, 0, 0, 0,
					 DRI2_EXCHANGE_COMPLETE,
					 info->event_complete,
					 info->event_data);
		} else {
			info->back->name = info->old_front.name;
			get_private(info->back)->bo = info->old_front.bo;
			info->old_front.bo = NULL;
		}
	} else {
		info = calloc(1, sizeof(struct sna_dri_frame_event));
		if (info == NULL)
			return false;

		info->draw = draw;
		info->client = client;
		info->event_complete = func;
		info->event_data = data;
		info->front = front;
		info->back = back;
		info->pipe = pipe;
		info->type = DRI2_FLIP;

		sna_dri_add_frame_event(draw, info);
		sna_dri_reference_buffer(front);
		sna_dri_reference_buffer(back);

		/* Get current count */
		vbl.request.type = DRM_VBLANK_RELATIVE | pipe_select(pipe);
		vbl.request.sequence = 0;
		if (sna_wait_vblank(sna, &vbl)) {
			sna_dri_frame_event_info_free(sna, draw, info);
			return false;
		}

		current_msc = vbl.reply.sequence;
		*target_msc &= 0xffffffff;
		remainder &= 0xffffffff;

		vbl.request.type =
			DRM_VBLANK_ABSOLUTE |
			DRM_VBLANK_EVENT |
			pipe_select(pipe);

		/*
		 * If divisor is zero, or current_msc is smaller than target_msc
		 * we just need to make sure target_msc passes before initiating
		 * the swap.
		 */
		if (current_msc < *target_msc) {
			DBG(("%s: waiting for swap: current=%d, target=%d, divisor=%d\n",
			     __FUNCTION__,
			     (int)current_msc,
			     (int)*target_msc,
			     (int)divisor));
			vbl.request.sequence = *target_msc;
		} else {
			DBG(("%s: missed target, queueing event for next: current=%d, target=%d, divisor=%d\n",
			     __FUNCTION__,
			     (int)current_msc,
			     (int)*target_msc,
			     (int)divisor));

			vbl.request.sequence = current_msc - current_msc % divisor + remainder;

			/*
			 * If the calculated deadline vbl.request.sequence is
			 * smaller than or equal to current_msc, it means
			 * we've passed the last point when effective onset
			 * frame seq could satisfy *seq % divisor == remainder,
			 * so we need to wait for the next time this will
			 * happen.
			 *
			 * This comparison takes the 1 frame swap delay
			 * in pageflipping mode into account.
			 */
			if (vbl.request.sequence <= current_msc)
				vbl.request.sequence += divisor;

			/* Adjust returned value for 1 frame pageflip offset */
			*target_msc = vbl.reply.sequence + 1;
		}

		/* Account for 1 frame extra pageflip delay */
		vbl.request.sequence -= 1;
		vbl.request.signal = (unsigned long)info;
		if (sna_wait_vblank(sna, &vbl)) {
			sna_dri_frame_event_info_free(sna, draw, info);
			return false;
		}
	}

	return true;
}

static void
sna_dri_immediate_xchg(struct sna *sna,
		       DrawablePtr draw,
		       struct sna_dri_frame_event *info)
{
	drmVBlank vbl;

	DBG(("%s: emitting immediate exchange, throttling client\n",
	     __FUNCTION__));
	VG_CLEAR(vbl);

	if ((sna->flags & SNA_NO_WAIT) == 0) {
		info->type = DRI2_XCHG_THROTTLE;
		if (sna_dri_window_get_chain((WindowPtr)draw) == info) {
			DBG(("%s: no pending xchg, starting chain\n",
			     __FUNCTION__));

			sna_dri_exchange_buffers(draw, info->front, info->back);
			DRI2SwapComplete(info->client, draw, 0, 0, 0,
					 DRI2_EXCHANGE_COMPLETE,
					 info->event_complete,
					 info->event_data);
			vbl.request.type =
				DRM_VBLANK_RELATIVE |
				DRM_VBLANK_NEXTONMISS |
				DRM_VBLANK_EVENT |
				pipe_select(info->pipe);
			vbl.request.sequence = 0;
			vbl.request.signal = (unsigned long)info;
			if (sna_wait_vblank(sna, &vbl))
				sna_dri_frame_event_info_free(sna, draw, info);
		}
	} else {
		sna_dri_exchange_buffers(draw, info->front, info->back);
		DRI2SwapComplete(info->client, draw, 0, 0, 0,
				 DRI2_EXCHANGE_COMPLETE,
				 info->event_complete,
				 info->event_data);
		sna_dri_frame_event_info_free(sna, draw, info);
	}
}

static void
sna_dri_immediate_blit(struct sna *sna,
		       DrawablePtr draw,
		       struct sna_dri_frame_event *info)
{
	drmVBlank vbl;

	DBG(("%s: emitting immediate blit, throttling client\n", __FUNCTION__));
	VG_CLEAR(vbl);

	if ((sna->flags & SNA_NO_WAIT) == 0) {
		info->type = DRI2_SWAP_THROTTLE;
		if (sna_dri_window_get_chain((WindowPtr)draw) == info) {
			DBG(("%s: no pending blit, starting chain\n",
			     __FUNCTION__));

			info->bo = sna_dri_copy_to_front(sna, draw, NULL,
							 get_private(info->front)->bo,
							 get_private(info->back)->bo,
							 true);
			DRI2SwapComplete(info->client, draw, 0, 0, 0,
					 DRI2_BLIT_COMPLETE,
					 info->event_complete,
					 info->event_data);

			vbl.request.type =
				DRM_VBLANK_RELATIVE |
				DRM_VBLANK_NEXTONMISS |
				DRM_VBLANK_EVENT |
				pipe_select(info->pipe);
			vbl.request.sequence = 0;
			vbl.request.signal = (unsigned long)info;
			if (sna_wait_vblank(sna, &vbl))
				sna_dri_frame_event_info_free(sna, draw, info);
		}
	} else {
		info->bo = sna_dri_copy_to_front(sna, draw, NULL,
						 get_private(info->front)->bo,
						 get_private(info->back)->bo,
						 false);
		DRI2SwapComplete(info->client, draw, 0, 0, 0,
				 DRI2_BLIT_COMPLETE,
				 info->event_complete,
				 info->event_data);
		sna_dri_frame_event_info_free(sna, draw, info);
	}
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
	ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
	struct sna *sna = to_sna(scrn);
	drmVBlank vbl;
	int pipe;
	struct sna_dri_frame_event *info = NULL;
	enum frame_event_type swap_type = DRI2_SWAP;
	CARD64 current_msc;

	DBG(("%s(target_msc=%llu, divisor=%llu, remainder=%llu)\n",
	     __FUNCTION__,
	     (long long)*target_msc,
	     (long long)divisor,
	     (long long)remainder));

	if (can_flip(sna, draw, front, back)) {
		DBG(("%s: try flip\n", __FUNCTION__));
		if (!sna_dri_schedule_flip(client, draw, front, back,
					   target_msc, divisor, remainder,
					   func, data))
			goto blit_fallback;

		return TRUE;
	}

	/* Drawable not displayed... just complete the swap */
	pipe = sna_dri_get_pipe(draw);
	if (pipe == -1) {
		if (can_exchange(sna, draw, front, back)) {
			DBG(("%s: unattached, exchange pixmaps\n", __FUNCTION__));
			sna_dri_exchange_buffers(draw, front, back);

			DRI2SwapComplete(client, draw, 0, 0, 0,
					 DRI2_EXCHANGE_COMPLETE, func, data);
			return TRUE;
		}

		DBG(("%s: off-screen, immediate update\n", __FUNCTION__));
		goto blit_fallback;
	}

	VG_CLEAR(vbl);

	/* Truncate to match kernel interfaces; means occasional overflow
	 * misses, but that's generally not a big deal */
	*target_msc &= 0xffffffff;
	divisor &= 0xffffffff;
	remainder &= 0xffffffff;

	info = calloc(1, sizeof(struct sna_dri_frame_event));
	if (!info)
		goto blit_fallback;

	info->draw = draw;
	info->client = client;
	info->event_complete = func;
	info->event_data = data;
	info->front = front;
	info->back = back;
	info->pipe = pipe;

	sna_dri_add_frame_event(draw, info);
	sna_dri_reference_buffer(front);
	sna_dri_reference_buffer(back);

	info->type = swap_type;
	if (divisor == 0) {
		if (can_exchange(sna, draw, front, back))
			sna_dri_immediate_xchg(sna, draw, info);
		else
			sna_dri_immediate_blit(sna, draw, info);
		return TRUE;
	}

	/* Get current count */
	vbl.request.type = DRM_VBLANK_RELATIVE | pipe_select(pipe);
	vbl.request.sequence = 0;
	if (sna_wait_vblank(sna, &vbl))
		goto blit_fallback;

	current_msc = vbl.reply.sequence;

	/*
	 * If divisor is zero, or current_msc is smaller than target_msc
	 * we just need to make sure target_msc passes before initiating
	 * the swap.
	 */
	if (current_msc < *target_msc) {
		DBG(("%s: waiting for swap: current=%d, target=%d, divisor=%d\n",
		     __FUNCTION__,
		     (int)current_msc,
		     (int)*target_msc,
		     (int)divisor));

		info->type = DRI2_SWAP;

		vbl.request.type =
			DRM_VBLANK_ABSOLUTE |
			DRM_VBLANK_EVENT |
			pipe_select(pipe);
		vbl.request.sequence = *target_msc;
		vbl.request.signal = (unsigned long)info;
		if (sna_wait_vblank(sna, &vbl))
			goto blit_fallback;

		return TRUE;
	}

	/*
	 * If we get here, target_msc has already passed or we don't have one,
	 * and we need to queue an event that will satisfy the divisor/remainder
	 * equation.
	 */
	DBG(("%s: missed target, queueing event for next: current=%d, target=%d,  divisor=%d\n",
	     __FUNCTION__,
	     (int)current_msc,
	     (int)*target_msc,
	     (int)divisor));

	vbl.request.type =
		DRM_VBLANK_ABSOLUTE |
		DRM_VBLANK_EVENT |
		DRM_VBLANK_NEXTONMISS |
		pipe_select(pipe);

	vbl.request.sequence = current_msc - current_msc % divisor + remainder;
	/*
	 * If the calculated deadline vbl.request.sequence is smaller than
	 * or equal to current_msc, it means we've passed the last point
	 * when effective onset frame seq could satisfy
	 * seq % divisor == remainder, so we need to wait for the next time
	 * this will happen.
	 */
	if (vbl.request.sequence < current_msc)
		vbl.request.sequence += divisor;
	vbl.request.sequence -= 1;

	vbl.request.signal = (unsigned long)info;
	if (sna_wait_vblank(sna, &vbl))
		goto blit_fallback;

	*target_msc = vbl.reply.sequence;
	return TRUE;

blit_fallback:
	if (can_exchange(sna, draw, front, back)) {
		DBG(("%s -- xchg\n", __FUNCTION__));
		sna_dri_exchange_buffers(draw, front, back);
		pipe = DRI2_EXCHANGE_COMPLETE;
	} else {
		DBG(("%s -- blit\n", __FUNCTION__));
		sna_dri_copy_to_front(sna, draw, NULL,
				      get_private(front)->bo,
				      get_private(back)->bo,
				      false);
		pipe = DRI2_BLIT_COMPLETE;
	}
	if (info)
		sna_dri_frame_event_info_free(sna, draw, info);
	DRI2SwapComplete(client, draw, 0, 0, 0, pipe, func, data);
	*target_msc = 0; /* offscreen, so zero out target vblank count */
	return TRUE;
}

#if USE_ASYNC_SWAP
static Bool
sna_dri_async_swap(ClientPtr client, DrawablePtr draw,
		   DRI2BufferPtr front, DRI2BufferPtr back,
		   DRI2SwapEventPtr func, void *data)
{
	struct sna *sna = to_sna_from_drawable(draw);
	struct sna_dri_frame_event *info;
	struct kgem_bo *bo;
	int name;

	DBG(("%s()\n", __FUNCTION__));

	if (!can_flip(sna, draw, front, back)) {
blit:
		if (can_exchange(sna, draw, front, back)) {
			DBG(("%s: unable to flip, so xchg\n", __FUNCTION__));
			sna_dri_exchange_buffers(draw, front, back);
			name = DRI2_EXCHANGE_COMPLETE;
		} else {
			DBG(("%s: unable to flip, so blit\n", __FUNCTION__));
			sna_dri_copy_to_front(sna, draw, NULL,
					      get_private(front)->bo,
					      get_private(back)->bo,
					      false);
			name = DRI2_BLIT_COMPLETE;
		}

		DRI2SwapComplete(client, draw, 0, 0, 0, name, func, data);
		return name == DRI2_EXCHANGE_COMPLETE;
	}

	bo = NULL;
	name = 0;

	info = sna->dri.flip_pending;
	if (info == NULL) {
		int pipe = sna_dri_get_pipe(draw);
		if (pipe == -1)
			goto blit;

		DBG(("%s: no pending flip, so updating scanout\n",
		     __FUNCTION__));

		info = calloc(1, sizeof(struct sna_dri_frame_event));
		if (!info)
			goto blit;

		info->client = client;
		info->draw = draw;
		info->type = DRI2_ASYNC_FLIP;
		info->pipe = pipe;
		info->front = front;
		info->back = back;

		sna_dri_add_frame_event(draw, info);
		sna_dri_reference_buffer(front);
		sna_dri_reference_buffer(back);

		sna_dri_page_flip(sna, info);

		info->next_front.name = info->front->name;
		info->next_front.bo = get_private(info->front)->bo;
		info->off_delay = 5;
	} else if (info->type != DRI2_ASYNC_FLIP) {
		/* A normal vsync'ed client is finishing, wait for it
		 * to unpin the old framebuffer before taking over.
		 */
		goto blit;
	} else {
		DBG(("%s: pending flip, chaining next\n", __FUNCTION__));
		if (info->next_front.name == info->front->name) {
			name = info->cache.name;
			bo = info->cache.bo;
		} else {
			name = info->front->name;
			bo = get_private(info->front)->bo;
		}
		info->front->name = info->back->name;
		get_private(info->front)->bo = get_private(info->back)->bo;
	}

	if (bo == NULL) {
		DBG(("%s: creating new back buffer\n", __FUNCTION__));
		bo = kgem_create_2d(&sna->kgem,
				    draw->width,
				    draw->height,
				    draw->bitsPerPixel,
				    get_private(info->front)->bo->tiling,
				    CREATE_SCANOUT | CREATE_EXACT);
		name = kgem_bo_flink(&sna->kgem, bo);
	}
	assert(bo->refcnt);
	get_private(info->back)->bo = bo;
	info->back->name = name;

	set_bo(sna->front, get_private(info->front)->bo);
	sna->dri.flip_pending = info;

	DRI2SwapComplete(client, draw, 0, 0, 0,
			 DRI2_EXCHANGE_COMPLETE, func, data);
	return TRUE;
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

	DBG(("%s(pipe=%d)\n", __FUNCTION__, pipe));

	/* Drawable not displayed, make up a value */
	if (pipe == -1) {
		*ust = 0;
		*msc = 0;
		return TRUE;
	}

	VG_CLEAR(vbl);
	vbl.request.type = DRM_VBLANK_RELATIVE | pipe_select(pipe);
	vbl.request.sequence = 0;
	if (sna_wait_vblank(sna, &vbl)) {
		DBG(("%s: query failed on pipe %d, ret=%d\n",
		     __FUNCTION__, pipe, errno));
		return FALSE;
	}

	*ust = ((CARD64)vbl.reply.tval_sec * 1000000) + vbl.reply.tval_usec;
	*msc = vbl.reply.sequence;
	DBG(("%s: msc=%llu, ust=%llu\n", __FUNCTION__,
	     (long long)*msc, (long long)*ust));
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
	struct sna *sna = to_sna_from_drawable(draw);
	struct sna_dri_frame_event *info = NULL;
	int pipe = sna_dri_get_pipe(draw);
	CARD64 current_msc;
	drmVBlank vbl;

	DBG(("%s(pipe=%d, target_msc=%llu, divisor=%llu, rem=%llu)\n",
	     __FUNCTION__, pipe,
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

	VG_CLEAR(vbl);

	/* Get current count */
	vbl.request.type = DRM_VBLANK_RELATIVE | pipe_select(pipe);
	vbl.request.sequence = 0;
	if (sna_wait_vblank(sna, &vbl))
		goto out_complete;

	current_msc = vbl.reply.sequence;

	/* If target_msc already reached or passed, set it to
	 * current_msc to ensure we return a reasonable value back
	 * to the caller. This keeps the client from continually
	 * sending us MSC targets from the past by forcibly updating
	 * their count on this call.
	 */
	if (divisor == 0 && current_msc >= target_msc) {
		target_msc = current_msc;
		goto out_complete;
	}

	info = calloc(1, sizeof(struct sna_dri_frame_event));
	if (!info)
		goto out_complete;

	info->draw = draw;
	info->client = client;
	info->type = DRI2_WAITMSC;
	sna_dri_add_frame_event(draw, info);

	/*
	 * If divisor is zero, or current_msc is smaller than target_msc,
	 * we just need to make sure target_msc passes before waking up the
	 * client.
	 */
	if (divisor == 0 || current_msc < target_msc) {
		vbl.request.type =
			DRM_VBLANK_ABSOLUTE |
			DRM_VBLANK_EVENT |
			pipe_select(pipe);
		vbl.request.sequence = target_msc;
		vbl.request.signal = (unsigned long)info;
		if (sna_wait_vblank(sna, &vbl))
			goto out_free_info;

		DRI2BlockClient(client, draw);
		return TRUE;
	}

	/*
	 * If we get here, target_msc has already passed or we don't have one,
	 * so we queue an event that will satisfy the divisor/remainder
	 * equation.
	 */
	vbl.request.type =
		DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT | pipe_select(pipe);

	vbl.request.sequence = current_msc - current_msc % divisor + remainder;

	/*
	 * If calculated remainder is larger than requested remainder,
	 * it means we've passed the last point where
	 * seq % divisor == remainder, so we need to wait for the next time
	 * that will happen.
	 */
	if ((current_msc % divisor) >= remainder)
		vbl.request.sequence += divisor;

	vbl.request.signal = (unsigned long)info;
	if (sna_wait_vblank(sna, &vbl))
		goto out_free_info;

	DRI2BlockClient(client, draw);
	return TRUE;

out_free_info:
	sna_dri_frame_event_info_free(sna, draw, info);
out_complete:
	DRI2WaitMSCComplete(client, draw, target_msc, 0, 0);
	return TRUE;
}
#endif

bool sna_dri_open(struct sna *sna, ScreenPtr screen)
{
	DRI2InfoRec info;
	int major = 1, minor = 0;
#if DRI2INFOREC_VERSION >= 4
	const char *driverNames[1];
#endif

	DBG(("%s()\n", __FUNCTION__));

	if (wedged(sna)) {
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "loading DRI2 whilst the GPU is wedged.\n");
	}

	if (xf86LoaderCheckSymbol("DRI2Version"))
		DRI2Version(&major, &minor);

	if (minor < 1) {
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "DRI2 requires DRI2 module version 1.1.0 or later\n");
		return false;
	}

	sna->deviceName = drmGetDeviceNameFromFd(sna->kgem.fd);
	memset(&info, '\0', sizeof(info));
	info.fd = sna->kgem.fd;
	info.driverName =
		(sna->kgem.gen && sna->kgem.gen < 40) ? "i915" : "i965";
	info.deviceName = sna->deviceName;

	DBG(("%s: loading dri driver '%s' [gen=%d] for device '%s'\n",
	     __FUNCTION__, info.driverName, sna->kgem.gen, info.deviceName));

	info.version = 3;
	info.CreateBuffer = sna_dri_create_buffer;
	info.DestroyBuffer = sna_dri_destroy_buffer;

	info.CopyRegion = sna_dri_copy_region;
#if DRI2INFOREC_VERSION >= 4
	info.version = 4;
	info.ScheduleSwap = sna_dri_schedule_swap;
	info.GetMSC = sna_dri_get_msc;
	info.ScheduleWaitMSC = sna_dri_schedule_wait_msc;
	info.numDrivers = 1;
	info.driverNames = driverNames;
	driverNames[0] = info.driverName;
#endif

#if DRI2INFOREC_VERSION >= 6
	info.version = 6;
	info.SwapLimitValidate = NULL;
	info.ReuseBufferNotify = NULL;
#endif

#if USE_ASYNC_SWAP
	info.version = 10;
	info.AsyncSwap = sna_dri_async_swap;
#endif

	return DRI2ScreenInit(screen, &info);
}

void sna_dri_close(struct sna *sna, ScreenPtr screen)
{
	DBG(("%s()\n", __FUNCTION__));
	DRI2CloseScreen(screen);
	drmFree(sna->deviceName);
}
