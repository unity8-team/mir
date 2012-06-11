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

#include <xf86.h>
#include <xf86_OSproc.h>
#include <errno.h>
#include <string.h>

#include <i915_drm.h>
#include <dri2.h>

#include "sna.h"
#include "sna_reg.h"

#if DRI2INFOREC_VERSION <= 2
#error DRI2 version supported by the Xserver is too old
#endif

#if DEBUG_DRI
#undef DBG
#define DBG(x) ErrorF x
#endif

#define COLOR_PREFER_TILING_Y 0

enum frame_event_type {
	DRI2_SWAP,
	DRI2_SWAP_THROTTLE,
	DRI2_ASYNC_FLIP,
	DRI2_FLIP,
	DRI2_FLIP_THROTTLE,
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
	ClientPtr client;
	enum frame_event_type type;
	unsigned frame;
	int pipe;
	int count;

	struct list drawable_resource;
	struct list client_resource;

	/* for swaps & flips only */
	DRI2SwapEventPtr event_complete;
	void *event_data;
	DRI2BufferPtr front;
	DRI2BufferPtr back;

	unsigned int fe_frame;
	unsigned int fe_tv_sec;
	unsigned int fe_tv_usec;

	struct {
		struct kgem_bo *bo;
		uint32_t name;
	} old_front, next_front, cache;
	uint32_t old_fb;

	int off_delay;
};

static DevPrivateKeyRec sna_client_key;

static inline struct sna_dri_frame_event *
to_frame_event(void *data)
{
	 return (struct sna_dri_frame_event *)((uintptr_t)data & ~1);
}

static inline struct sna_dri_private *
get_private(DRI2Buffer2Ptr buffer)
{
	return (struct sna_dri_private *)(buffer+1);
}

static inline struct kgem_bo *ref(struct kgem_bo *bo)
{
	bo->refcnt++;
	return bo;
}

/* Prefer to enable TILING_Y if this buffer will never be a
 * candidate for pageflipping
 */
static uint32_t color_tiling(struct sna *sna, DrawablePtr drawable)
{
	uint32_t tiling;

	if (COLOR_PREFER_TILING_Y &&
	    (drawable->width  != sna->front->drawable.width ||
	     drawable->height != sna->front->drawable.height))
		tiling = I915_TILING_Y;
	else
		tiling = I915_TILING_X;

	return kgem_choose_tiling(&sna->kgem, -tiling,
				  drawable->width,
				  drawable->height,
				  drawable->bitsPerPixel);
}

static uint32_t other_tiling(struct sna *sna, DrawablePtr drawable)
{
	/* XXX Can mix color X / depth Y? */
	return kgem_choose_tiling(&sna->kgem, -I915_TILING_Y,
				  drawable->width,
				  drawable->height,
				  drawable->bitsPerPixel);
}

static struct kgem_bo *sna_pixmap_set_dri(struct sna *sna,
					  PixmapPtr pixmap)
{
	struct sna_pixmap *priv;
	int tiling;

	priv = sna_pixmap_force_to_gpu(pixmap, MOVE_READ | MOVE_WRITE);
	if (priv == NULL)
		return NULL;

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
	priv->pinned = 1;

	return priv->gpu_bo;
}

static DRI2Buffer2Ptr
sna_dri_create_buffer(DrawablePtr drawable,
		      unsigned int attachment,
		      unsigned int format)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	DRI2Buffer2Ptr buffer;
	struct sna_dri_private *private;
	PixmapPtr pixmap;
	struct kgem_bo *bo;
	int bpp;

	DBG(("%s(attachment=%d, format=%d, drawable=%dx%d)\n",
	     __FUNCTION__, attachment, format,
	     drawable->width, drawable->height));

	pixmap = NULL;
	switch (attachment) {
	case DRI2BufferFrontLeft:
		pixmap = get_drawable_pixmap(drawable);
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
		bpp = drawable->bitsPerPixel;
		bo = kgem_create_2d(&sna->kgem,
				    drawable->width,
				    drawable->height,
				    drawable->bitsPerPixel,
				    color_tiling(sna, drawable),
				    CREATE_EXACT);
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
		bpp = format ? format : drawable->bitsPerPixel;
		bpp *= 2;
		bo = kgem_create_2d(&sna->kgem,
				    ALIGN(drawable->width, 64),
				    ALIGN((drawable->height + 1) / 2, 64),
				    bpp, I915_TILING_NONE, CREATE_EXACT);
		break;

	case DRI2BufferDepth:
	case DRI2BufferDepthStencil:
	case DRI2BufferHiz:
	case DRI2BufferAccum:
		bpp = format ? format : drawable->bitsPerPixel,
		bo = kgem_create_2d(&sna->kgem,
				    drawable->width, drawable->height, bpp,
				    other_tiling(sna, drawable),
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
	private->pixmap = pixmap;
	private->bo = bo;

	if (buffer->name == 0)
		goto err;

	if (pixmap)
		pixmap->refcnt++;

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

	if (--private->refcnt == 0) {
		if (private->pixmap) {
			ScreenPtr screen = private->pixmap->drawable.pScreen;
			struct sna_pixmap *priv = sna_pixmap(private->pixmap);

			/* Undo the DRI markings on this pixmap */
			assert(priv->flush > 0);
			if (--priv->flush == 0) {
				list_del(&priv->list);
				sna_accel_watch_flush(sna, -1);
				priv->pinned = private->pixmap == sna->front;
			}

			screen->DestroyPixmap(private->pixmap);
		}

		private->bo->flush = 0;
		kgem_bo_clear_scanout(&sna->kgem, private->bo); /* paranoia */
		kgem_bo_destroy(&sna->kgem, private->bo);

		free(buffer);
	}
}

static void sna_dri_destroy_buffer(DrawablePtr drawable, DRI2Buffer2Ptr buffer)
{
	_sna_dri_destroy_buffer(to_sna_from_drawable(drawable), buffer);
}

static void sna_dri_reference_buffer(DRI2Buffer2Ptr buffer)
{
	get_private(buffer)->refcnt++;
}

static void damage(PixmapPtr pixmap, RegionPtr region)
{
	struct sna_pixmap *priv;

	priv = sna_pixmap(pixmap);
	if (DAMAGE_IS_ALL(priv->gpu_damage))
		return;

	if (region == NULL) {
damage_all:
		sna_damage_all(&priv->gpu_damage,
			       pixmap->drawable.width,
			       pixmap->drawable.height);
		sna_damage_destroy(&priv->cpu_damage);
		priv->undamaged = false;
	} else {
		BoxPtr box = RegionExtents(region);
		if (region->data == NULL &&
		    box->x1 <= 0 && box->y1 <= 0 &&
		    box->x2 >= pixmap->drawable.width &&
		    box->y2 >= pixmap->drawable.height)
			goto damage_all;

		sna_damage_add(&priv->gpu_damage, region);
		sna_damage_subtract(&priv->cpu_damage, region);
	}
}

static void set_bo(PixmapPtr pixmap, struct kgem_bo *bo)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);

	sna_damage_all(&priv->gpu_damage,
		       pixmap->drawable.width,
		       pixmap->drawable.height);
	sna_damage_destroy(&priv->cpu_damage);
	priv->undamaged = false;

	kgem_bo_clear_scanout(&sna->kgem, priv->gpu_bo); /* paranoia */
	kgem_bo_destroy(&sna->kgem, priv->gpu_bo);
	priv->gpu_bo = ref(bo);
}

static void
sna_dri_copy_to_front(struct sna *sna, DrawablePtr draw, RegionPtr region,
		      struct kgem_bo *dst_bo, struct kgem_bo *src_bo,
		      bool sync)
{
	PixmapPtr pixmap = get_drawable_pixmap(draw);
	pixman_region16_t clip;
	bool flush = false;
	xf86CrtcPtr crtc;
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

		if (pixmap == sna->front && sync) {
			BoxRec crtc_box;

			crtc = sna_covering_crtc(sna->scrn, &region->extents,
						 NULL, &crtc_box);
			if (crtc)
				flush = sna_wait_for_scanline(sna, pixmap, crtc,
							      &region->extents);
		}

		get_drawable_deltas(draw, pixmap, &dx, &dy);
	}

	if (sna->kgem.gen >= 60) {
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
		 */
		kgem_set_mode(&sna->kgem, KGEM_RENDER);
	}

	damage(pixmap, region);
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
	sna->render.copy_boxes(sna, GXcopy,
			       (PixmapPtr)draw, src_bo, -draw->x, -draw->y,
			       pixmap, dst_bo, dx, dy,
			       boxes, n);

	DBG(("%s: flushing? %d\n", __FUNCTION__, flush));
	if (flush) { /* STAT! */
		if (!sna_crtc_is_bound(sna, crtc))
			sna->kgem.batch[sna->kgem.wait] = 0;
		kgem_submit(&sna->kgem);
	}

	pixman_region_translate(region, dx, dy);
	DamageRegionAppend(&pixmap->drawable, region);
	DamageRegionProcessPending(&pixmap->drawable);

	if (region == &clip)
		pixman_region_fini(&clip);
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

	if (sna->kgem.gen >= 60)
		kgem_set_mode(&sna->kgem, KGEM_RENDER);

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
	sna->render.copy_boxes(sna, GXcopy,
			       pixmap, src_bo, dx, dy,
			       (PixmapPtr)draw, dst_bo, -draw->x, -draw->y,
			       boxes, n);

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

	if (sna->kgem.gen >= 60) {
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
		 */
		kgem_set_mode(&sna->kgem, KGEM_RENDER);
	}

	sna->render.copy_boxes(sna, GXcopy,
			       (PixmapPtr)draw, src_bo, 0, 0,
			       (PixmapPtr)draw, dst_bo, 0, 0,
			       boxes, n);

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
		dst = sna_pixmap_set_dri(sna, pixmap);
		copy = sna_dri_copy_to_front;
	} else
		dst = get_private(dst_buffer)->bo;

	if (src_buffer->attachment == DRI2BufferFrontLeft) {
		src = sna_pixmap_set_dri(sna, pixmap);
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

static struct list *
get_resource(XID id, RESTYPE type)
{
	struct list *resource;
	void *ptr;

	ptr = NULL;
	dixLookupResourceByType(&ptr, id, type, NULL, DixWriteAccess);
	if (ptr)
		return ptr;

	resource = malloc(sizeof(*resource));
	if (resource == NULL)
		return NULL;

	if (!AddResource(id, type, resource)) {
		DBG(("%s: failed to add resource (%ld, %ld)\n",
		     __FUNCTION__, (long)id, (long)type));
		free(resource);
		return NULL;
	}

	DBG(("%s(%ld): new(%ld)=%p\n", __FUNCTION__,
	     (long)id, (long)type, resource));

	list_init(resource);
	return resource;
}

static int
sna_dri_frame_event_client_gone(void *data, XID id)
{
	struct list *resource = data;

	DBG(("%s(%ld): %p\n", __FUNCTION__, (long)id, data));

	while (!list_is_empty(resource)) {
		struct sna_dri_frame_event *info =
			list_first_entry(resource,
					 struct sna_dri_frame_event,
					 client_resource);

		DBG(("%s: marking client gone [%p]: %p\n",
		     __FUNCTION__, info, info->client));

		list_del(&info->client_resource);
		info->client = NULL;
	}
	free(resource);

	return Success;
}

static int
sna_dri_frame_event_drawable_gone(void *data, XID id)
{
	struct list *resource = data;

	DBG(("%s(%ld): resource=%p\n", __FUNCTION__, (long)id, resource));

	while (!list_is_empty(resource)) {
		struct sna_dri_frame_event *info =
			list_first_entry(resource,
					 struct sna_dri_frame_event,
					 drawable_resource);

		DBG(("%s: marking drawable gone [%p]: %ld\n",
		     __FUNCTION__, info, (long)info->drawable_id));

		list_del(&info->drawable_resource);
		info->drawable_id = None;
	}
	free(resource);

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

static XID
get_client_id(ClientPtr client)
{
	XID *ptr = dixGetPrivateAddr(&client->devPrivates, &sna_client_key);
	if (*ptr == 0)
		*ptr = FakeClientID(client->index);
	return *ptr;
}

/*
 * Hook this frame event into the server resource
 * database so we can clean it up if the drawable or
 * client exits while the swap is pending
 */
static Bool
sna_dri_add_frame_event(struct sna_dri_frame_event *info)
{
	struct list *resource;

	resource = get_resource(get_client_id(info->client),
				frame_event_client_type);
	if (resource == NULL) {
		DBG(("%s: failed to get client resource\n", __FUNCTION__));
		return FALSE;
	}

	list_add(&info->client_resource, resource);

	resource = get_resource(info->drawable_id, frame_event_drawable_type);
	if (resource == NULL) {
		DBG(("%s: failed to get drawable resource\n", __FUNCTION__));
		list_del(&info->client_resource);
		return FALSE;
	}

	list_add(&info->drawable_resource, resource);

	DBG(("%s: add[%p] (%p, %ld)\n", __FUNCTION__,
	     info, info->client, (long)info->drawable_id));

	return TRUE;
}

static void
sna_dri_frame_event_release_bo(struct kgem *kgem, struct kgem_bo *bo)
{
	kgem_bo_clear_scanout(kgem, bo);
	kgem_bo_destroy(kgem, bo);
}

static void
sna_dri_frame_event_finish(struct sna_dri_frame_event *info)
{
	sna_mode_delete_fb(info->sna, info->old_fb);
	kgem_bo_clear_scanout(&info->sna->kgem, info->old_front.bo);
}

static void
sna_dri_frame_event_info_free(struct sna_dri_frame_event *info)
{
	DBG(("%s: del[%p] (%p, %ld)\n", __FUNCTION__,
	     info, info->client, (long)info->drawable_id));

	list_del(&info->client_resource);
	list_del(&info->drawable_resource);

	_sna_dri_destroy_buffer(info->sna, info->front);
	_sna_dri_destroy_buffer(info->sna, info->back);

	if (info->old_front.bo)
		sna_dri_frame_event_release_bo(&info->sna->kgem,
					       info->old_front.bo);

	if (info->next_front.bo)
		sna_dri_frame_event_release_bo(&info->sna->kgem,
					       info->next_front.bo);

	if (info->cache.bo)
		sna_dri_frame_event_release_bo(&info->sna->kgem,
					       info->cache.bo);

	free(info);
}

static void
sna_dri_exchange_attachment(DRI2BufferPtr front, DRI2BufferPtr back)
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
sna_dri_page_flip(struct sna *sna, struct sna_dri_frame_event *info)
{
	struct kgem_bo *bo = get_private(info->back)->bo;

	DBG(("%s()\n", __FUNCTION__));

	info->count = sna_page_flip(sna, bo,
				    info, info->pipe,
				    &info->old_fb);
	if (info->count == 0)
		return FALSE;

	info->old_front.name = info->front->name;
	info->old_front.bo = get_private(info->front)->bo;

	set_bo(sna->front, bo);

	info->front->name = info->back->name;
	get_private(info->front)->bo = bo;
	return TRUE;
}

static Bool
can_flip(struct sna * sna,
	 DrawablePtr draw,
	 DRI2BufferPtr front,
	 DRI2BufferPtr back)
{
	WindowPtr win = (WindowPtr)draw;
	PixmapPtr pixmap;

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

	pixmap = get_drawable_pixmap(draw);
	if (pixmap != sna->front) {
		DBG(("%s: no, window is not on the front buffer\n",
		     __FUNCTION__));
		return FALSE;
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
		return FALSE;
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
		return FALSE;
	}

	/* prevent an implicit tiling mode change */
	if (get_private(front)->bo->tiling != get_private(back)->bo->tiling) {
		DBG(("%s -- no, tiling mismatch: front %d, back=%d\n",
		     __FUNCTION__,
		     get_private(front)->bo->tiling,
		     get_private(back)->bo->tiling));
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
		    sna_dri_page_flip(sna, info)) {
			info->back->name = info->old_front.name;
			get_private(info->back)->bo = info->old_front.bo;
			info->old_front.bo = NULL;
			return;
		}
		/* else fall through to blit */
	case DRI2_SWAP:
		sna_dri_copy_to_front(sna, draw, NULL,
				      get_private(info->front)->bo,
				      get_private(info->back)->bo,
				      true);
	case DRI2_SWAP_THROTTLE:
		DBG(("%s: %d complete, frame=%d tv=%d.%06d\n",
		     __FUNCTION__, info->type, frame, tv_sec, tv_usec));
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

static int
sna_dri_flip_continue(struct sna *sna,
		      DrawablePtr draw,
		      struct sna_dri_frame_event *info)
{
	struct kgem_bo *bo;
	int name;

	DBG(("%s()\n", __FUNCTION__));

	name = info->back->name;
	bo = get_private(info->back)->bo;

	info->count = sna_page_flip(sna, bo, info, info->pipe, &info->old_fb);
	if (info->count == 0)
		return FALSE;

	set_bo(sna->front, bo);

	get_private(info->back)->bo = info->old_front.bo;
	info->back->name = info->old_front.name;

	info->old_front.name = info->front->name;
	info->old_front.bo = get_private(info->front)->bo;

	info->front->name = name;
	get_private(info->front)->bo = bo;

	info->next_front.name = 0;

	sna->dri.flip_pending[info->pipe] = info;

	return TRUE;
}

static void sna_dri_flip_event(struct sna *sna,
			       struct sna_dri_frame_event *flip)
{
	DrawablePtr drawable;

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
		if (flip->drawable_id &&
		    dixLookupDrawable(&drawable,
				      flip->drawable_id,
				      serverClient,
				      M_ANY, DixWriteAccess) == Success) {
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
		}

		sna_dri_frame_event_finish(flip);
		sna_dri_frame_event_info_free(flip);
		break;

	case DRI2_FLIP_THROTTLE:
		sna_dri_frame_event_finish(flip);

		assert(sna->dri.flip_pending[flip->pipe] == flip);
		sna->dri.flip_pending[flip->pipe] = NULL;

		if (flip->next_front.name &&
		    flip->drawable_id &&
		    dixLookupDrawable(&drawable,
				      flip->drawable_id,
				      serverClient,
				      M_ANY, DixWriteAccess) == Success) {
			if (can_flip(sna, drawable, flip->front, flip->back) &&
			    !sna_dri_flip_continue(sna, drawable, flip)) {
				DRI2SwapComplete(flip->client, drawable,
						 0, 0, 0,
						 DRI2_BLIT_COMPLETE,
						 flip->client ? flip->event_complete : NULL,
						 flip->event_data);
				sna_dri_frame_event_info_free(flip);
			} else {
				DRI2SwapComplete(flip->client, drawable,
						 0, 0, 0,
						 DRI2_FLIP_COMPLETE,
						 flip->client ? flip->event_complete : NULL,
						 flip->event_data);
			}
		} else {
			sna_dri_frame_event_info_free(flip);
		}
		break;

#if USE_ASYNC_SWAP && DRI2INFOREC_VERSION >= 7
	case DRI2_ASYNC_FLIP:
		DBG(("%s: async swap flip completed on pipe %d, pending? %d, new? %d\n",
		     __FUNCTION__, flip->pipe,
		     sna->dri.flip_pending[flip->pipe] != NULL,
		     flip->front->name != flip->old_front.name));
		assert(sna->dri.flip_pending[flip->pipe] == flip);

		sna_dri_frame_event_finish(flip);

		if (flip->front->name != flip->next_front.name) {
			DBG(("%s: async flip continuing\n", __FUNCTION__));

			flip->cache = flip->old_front;
			flip->old_front = flip->next_front;
			flip->next_front.bo = NULL;

			flip->count = sna_page_flip(sna,
						    get_private(flip->front)->bo,
						    flip, flip->pipe,
						    &flip->old_fb);
			if (flip->count == 0)
				goto finish_async_flip;

			flip->next_front.bo = get_private(flip->front)->bo;
			flip->next_front.name = flip->front->name;
			flip->off_delay = 5;
		} else if (--flip->off_delay) {
			/* Just queue a no-op flip to trigger another event */
			flip->count = sna_page_flip(sna,
						    get_private(flip->front)->bo,
						    flip, flip->pipe,
						    &flip->old_fb);
			if (flip->count == 0)
				goto finish_async_flip;
		} else {
finish_async_flip:
			flip->next_front.bo = NULL;

			DBG(("%s: async flip completed\n", __FUNCTION__));
			sna->dri.flip_pending[flip->pipe] = NULL;
			sna_dri_frame_event_info_free(flip);
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

static int
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

	/* Drawable not displayed... just complete the swap */
	pipe = sna_dri_get_pipe(draw);
	if (pipe == -1) {
		RegionRec region;

		DBG(("%s: off-screen, immediate update\n", __FUNCTION__));

		sna_dri_exchange_attachment(front, back);
		get_private(back)->pixmap = get_private(front)->pixmap;
		get_private(front)->pixmap = NULL;
		set_bo(get_private(back)->pixmap, get_private(back)->bo);

		/* XXX can we query whether we need to process damage? */
		region.extents.x1 = draw->x;
		region.extents.y1 = draw->y;
		region.extents.x2 = draw->x + draw->width;
		region.extents.y2 = draw->y + draw->height;
		region.data = NULL;
		DamageRegionAppend(draw, &region);
		DamageRegionProcessPending(draw);

		DRI2SwapComplete(client, draw, 0, 0, 0,
				 DRI2_EXCHANGE_COMPLETE, func, data);
		return TRUE;
	}

	/* Truncate to match kernel interfaces; means occasional overflow
	 * misses, but that's generally not a big deal */
	divisor &= 0xffffffff;
	if (divisor == 0) {
		int type = DRI2_FLIP_THROTTLE;

		DBG(("%s: performing immediate swap on pipe %d, pending? %d\n",
		     __FUNCTION__, pipe, sna->dri.flip_pending[pipe] != NULL));

		info = sna->dri.flip_pending[pipe];
		if (info) {
			if (info->drawable_id == draw->id) {
				DBG(("%s: chaining flip\n", __FUNCTION__));
				info->next_front.name = 1;
				return TRUE;
			} else {
				/* We need to first wait (one vblank) for the
				 * async flips to complete before this client can
				 * take over.
				 */
				type = DRI2_FLIP;
			}
		}

		info = calloc(1, sizeof(struct sna_dri_frame_event));
		if (!info)
			return FALSE;

		info->type = type;

		info->sna = sna;
		info->drawable_id = draw->id;
		info->client = client;
		info->event_complete = func;
		info->event_data = data;
		info->front = front;
		info->back = back;
		info->pipe = pipe;

		if (!sna_dri_add_frame_event(info)) {
			DBG(("%s: failed to hook up frame event\n", __FUNCTION__));
			free(info);
			return FALSE;
		}

		sna_dri_reference_buffer(front);
		sna_dri_reference_buffer(back);

		if (!sna_dri_page_flip(sna, info)) {
			DBG(("%s: failed to queue page flip\n", __FUNCTION__));
			sna_dri_frame_event_info_free(info);
			return FALSE;
		}

		get_private(info->back)->bo =
			kgem_create_2d(&sna->kgem,
				       draw->width,
				       draw->height,
				       draw->bitsPerPixel,
				       get_private(info->front)->bo->tiling,
				       CREATE_EXACT);
		info->back->name = kgem_bo_flink(&sna->kgem,
						 get_private(info->back)->bo);
		sna->dri.flip_pending[info->pipe] = info;

		DRI2SwapComplete(info->client, draw, 0, 0, 0,
				 DRI2_EXCHANGE_COMPLETE,
				 info->event_complete,
				 info->event_data);
	} else {
		info = calloc(1, sizeof(struct sna_dri_frame_event));
		if (info)
			return FALSE;

		info->sna = sna;
		info->drawable_id = draw->id;
		info->client = client;
		info->event_complete = func;
		info->event_data = data;
		info->front = front;
		info->back = back;
		info->pipe = pipe;
		info->type = DRI2_FLIP;

		if (!sna_dri_add_frame_event(info)) {
			DBG(("%s: failed to hook up frame event\n", __FUNCTION__));
			free(info);
			return FALSE;
		}

		sna_dri_reference_buffer(front);
		sna_dri_reference_buffer(back);

		/* Get current count */
		vbl.request.type = DRM_VBLANK_RELATIVE;
		if (pipe > 0)
			vbl.request.type |= DRM_VBLANK_SECONDARY;
		vbl.request.sequence = 0;
		if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
			sna_dri_frame_event_info_free(info);
			return FALSE;
		}

		current_msc = vbl.reply.sequence;
		*target_msc &= 0xffffffff;
		remainder &= 0xffffffff;

		vbl.request.type =
			DRM_VBLANK_ABSOLUTE |
			DRM_VBLANK_EVENT;
		if (pipe > 0)
			vbl.request.type |= DRM_VBLANK_SECONDARY;

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
			 * If the calculated deadline vbl.request.sequence is smaller than
			 * or equal to current_msc, it means we've passed the last point
			 * when effective onset frame seq could satisfy
			 * seq % divisor == remainder, so we need to wait for the next time
			 * this will happen.
			 *
			 * This comparison takes the 1 frame swap delay in pageflipping mode
			 * into account.
			 */
			if (vbl.request.sequence <= current_msc)
				vbl.request.sequence += divisor;

			/* Adjust returned value for 1 frame pageflip offset */
			*target_msc = vbl.reply.sequence + 1;
		}

		/* Account for 1 frame extra pageflip delay */
		vbl.request.sequence -= 1;
		vbl.request.signal = (unsigned long)info;
		if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
			sna_dri_frame_event_info_free(info);
			return FALSE;
		}

		info->frame = *target_msc;
	}

	return TRUE;
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
		DBG(("%s: off-screen, immediate update\n", __FUNCTION__));
		goto blit_fallback;
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
		DBG(("%s: failed to hook up frame event\n", __FUNCTION__));
		free(info);
		info = NULL;
		goto blit_fallback;
	}

	sna_dri_reference_buffer(front);
	sna_dri_reference_buffer(back);

	info->type = swap_type;
	if (divisor == 0) {
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
		 if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
			 sna_dri_frame_event_info_free(info);
			 DRI2SwapComplete(client, draw, 0, 0, 0, DRI2_BLIT_COMPLETE, func, data);
		 }

		 sna_dri_copy_to_front(sna, draw, NULL,
				       get_private(front)->bo,
				       get_private(back)->bo,
				       true);
		 return TRUE;
	}

	/* Get current count */
	vbl.request.type = DRM_VBLANK_RELATIVE;
	if (pipe > 0)
		vbl.request.type |= DRM_VBLANK_SECONDARY;
	vbl.request.sequence = 0;
	if (drmWaitVBlank(sna->kgem.fd, &vbl))
		goto blit_fallback;

	current_msc = vbl.reply.sequence;

	/*
	 * If divisor is zero, or current_msc is smaller than target_msc
	 * we just need to make sure target_msc passes before initiating
	 * the swap.
	 */
	if (current_msc < *target_msc) {
		DBG(("%s: waiting for swap: current=%d, target=%d,  divisor=%d\n",
		     __FUNCTION__,
		     (int)current_msc,
		     (int)*target_msc,
		     (int)divisor));

		info->frame = *target_msc;
		info->type = DRI2_SWAP;

		 vbl.request.type =
			 DRM_VBLANK_ABSOLUTE |
			 DRM_VBLANK_EVENT;
		 if (pipe > 0)
			 vbl.request.type |= DRM_VBLANK_SECONDARY;
		 vbl.request.sequence = *target_msc;
		 vbl.request.signal = (unsigned long)info;
		 if (drmWaitVBlank(sna->kgem.fd, &vbl))
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

	vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
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
	 *
	 * This comparison takes the 1 frame swap delay in pageflipping mode
	 * into account, as well as a potential DRM_VBLANK_NEXTONMISS delay
	 * if we are blitting/exchanging instead of flipping.
	 */
	if (vbl.request.sequence <= current_msc)
		vbl.request.sequence += divisor;

	vbl.request.signal = (unsigned long)info;
	if (drmWaitVBlank(sna->kgem.fd, &vbl))
		goto blit_fallback;

	*target_msc = vbl.reply.sequence;
	info->frame = *target_msc;
	return TRUE;

blit_fallback:
	DBG(("%s -- blit\n", __FUNCTION__));
	sna_dri_copy_to_front(sna, draw, NULL,
			      get_private(front)->bo,
			      get_private(back)->bo,
			      false);
	if (info)
		sna_dri_frame_event_info_free(info);
	DRI2SwapComplete(client, draw, 0, 0, 0, DRI2_BLIT_COMPLETE, func, data);
	*target_msc = 0; /* offscreen, so zero out target vblank count */
	return TRUE;
}

#if USE_ASYNC_SWAP && DRI2INFOREC_VERSION >= 7
static Bool
sna_dri_async_swap(ClientPtr client, DrawablePtr draw,
		   DRI2BufferPtr front, DRI2BufferPtr back,
		   DRI2SwapEventPtr func, void *data)
{
	struct sna *sna = to_sna_from_drawable(draw);
	struct sna_dri_frame_event *info;
	struct kgem_bo *bo;
	int name, pipe;

	DBG(("%s()\n", __FUNCTION__));

	pipe = sna_dri_get_pipe(draw);
	if (pipe == -1) {
		PixmapPtr pixmap = get_drawable_pixmap(draw);

		DBG(("%s: unattached, exchange pixmaps\n", __FUNCTION__));

		set_bo(pixmap, get_private(back)->bo);
		sna_dri_exchange_attachment(front, back);
		get_private(back)->pixmap = pixmap;
		get_private(front)->pixmap = NULL;

		DRI2SwapComplete(client, draw, 0, 0, 0,
				 DRI2_EXCHANGE_COMPLETE, func, data);
		return TRUE;
	}

	if (!can_flip(sna, draw, front, back)) {
blit:
		DBG(("%s: unable to flip, so blit\n", __FUNCTION__));

		sna_dri_copy_to_front(sna, draw, NULL,
				      get_private(front)->bo,
				      get_private(back)->bo,
				      false);
		DRI2SwapComplete(client, draw, 0, 0, 0,
				 DRI2_BLIT_COMPLETE, func, data);
		return FALSE;
	}

	bo = NULL;
	name = 0;

	info = sna->dri.flip_pending[pipe];
	if (info == NULL) {
		DBG(("%s: no pending flip on pipe %d, so updating scanout\n",
		     __FUNCTION__, pipe));

		info = calloc(1, sizeof(struct sna_dri_frame_event));
		if (!info)
			goto blit;

		info->sna = sna;
		info->client = client;
		info->type = DRI2_ASYNC_FLIP;
		info->pipe = pipe;
		info->front = front;
		info->back = back;

		if (!sna_dri_add_frame_event(info)) {
			DBG(("%s: failed to hook up frame event\n", __FUNCTION__));
			free(info);
			goto blit;
		}

		if (!sna_dri_page_flip(sna, info)) {
			sna_dri_frame_event_info_free(info);
			goto blit;
		}

		info->next_front.name = info->front->name;
		info->next_front.bo = get_private(info->front)->bo;
		info->off_delay = 5;

		sna_dri_reference_buffer(front);
		sna_dri_reference_buffer(back);
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
		__kgem_flush(&sna->kgem, get_private(info->back)->bo);
	}

	if (bo == NULL) {
		DBG(("%s: creating new back buffer\n", __FUNCTION__));
		bo = kgem_create_2d(&sna->kgem,
				    draw->width,
				    draw->height,
				    draw->bitsPerPixel,
				    I915_TILING_X, CREATE_EXACT);
		name = kgem_bo_flink(&sna->kgem, bo);
	}
	get_private(info->back)->bo = bo;
	info->back->name = name;

	set_bo(sna->front, get_private(info->front)->bo);
	sna->dri.flip_pending[info->pipe] = info;

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

	/* Get current count */
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
		goto out_complete;
	}

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

	info->sna = sna;
	info->drawable_id = draw->id;
	info->client = client;
	info->type = DRI2_WAITMSC;
	if (!sna_dri_add_frame_event(info)) {
		DBG(("%s: failed to hook up frame event\n", __FUNCTION__));
		free(info);
		goto out_complete;
	}

	/*
	 * If divisor is zero, or current_msc is smaller than target_msc,
	 * we just need to make sure target_msc passes before waking up the
	 * client.
	 */
	if (divisor == 0 || current_msc < target_msc) {
		vbl.request.type = DRM_VBLANK_ABSOLUTE | DRM_VBLANK_EVENT;
		if (pipe > 0)
			vbl.request.type |= DRM_VBLANK_SECONDARY;
		vbl.request.sequence = target_msc;
		vbl.request.signal = (unsigned long)info;
		if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
			static int limit = 5;
			if (limit) {
				xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
					   "%s:%d get vblank counter failed: %s\n",
					   __FUNCTION__, __LINE__,
					   strerror(errno));
				limit--;
			}
			goto out_free_info;
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
	if (drmWaitVBlank(sna->kgem.fd, &vbl)) {
		static int limit = 5;
		if (limit) {
			xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
				   "%s:%d get vblank counter failed: %s\n",
				   __FUNCTION__, __LINE__,
				   strerror(errno));
			limit--;
		}
		goto out_free_info;
	}

	info->frame = vbl.reply.sequence;
	DRI2BlockClient(client, draw);
	return TRUE;

out_free_info:
	sna_dri_frame_event_info_free(info);
out_complete:
	DRI2WaitMSCComplete(client, draw, target_msc, 0, 0);
	return TRUE;
}
#endif

static unsigned int dri2_server_generation;

Bool sna_dri_open(struct sna *sna, ScreenPtr screen)
{
	DRI2InfoRec info;
	int major = 1, minor = 0;
#if DRI2INFOREC_VERSION >= 4
	const char *driverNames[1];
#endif

	DBG(("%s()\n", __FUNCTION__));

	if (sna->kgem.wedged) {
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "cannot enable DRI2 whilst the GPU is wedged\n");
		return FALSE;
	}

	if (xf86LoaderCheckSymbol("DRI2Version"))
		DRI2Version(&major, &minor);

	if (minor < 1) {
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

	if (!dixRegisterPrivateKey(&sna_client_key, PRIVATE_CLIENT, sizeof(XID)))
		return FALSE;

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

#if USE_ASYNC_SWAP && DRI2INFOREC_VERSION >= 7
	info.version = 7;
	info.AsyncSwap = sna_dri_async_swap;
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
