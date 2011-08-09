/*
 * Copyright (c) 2011 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sna.h"

#include <X11/fonts/font.h>
#include <X11/fonts/fontstruct.h>

#include <xaarop.h>
#include <fb.h>
#include <dixfontstr.h>

#ifdef RENDER
#include <mipict.h>
#include <fbpict.h>
#endif

#include <sys/time.h>
#include <sys/mman.h>
#include <unistd.h>

#if DEBUG_ACCEL
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

DevPrivateKeyRec sna_pixmap_index;

#define PM_IS_SOLID(_draw, _pm) \
	(((_pm) & FbFullMask((_draw)->depth)) == FbFullMask((_draw)->depth))

#if DEBUG_ACCEL
static void _assert_pixmap_contains_box(PixmapPtr pixmap, BoxPtr box, const char *function)
{
	if (box->x1 < 0 || box->y1 < 0 ||
	    box->x2 > pixmap->drawable.width ||
	    box->y2 > pixmap->drawable.height)
	{
		ErrorF("%s: damage box is beyond the pixmap: box=(%d, %d), (%d, %d), pixmap=(%d, %d)\n",
		       __FUNCTION__,
		       box->x1, box->y1, box->x2, box->y2,
		       pixmap->drawable.width,
		       pixmap->drawable.height);
		assert(0);
	}
}
#define assert_pixmap_contains_box(p, b) _assert_pixmap_contains_box(p, b, __FUNCTION__)
#else
#define assert_pixmap_contains_box(p, b)
#endif

static void sna_pixmap_destroy_gpu_bo(struct sna *sna, struct sna_pixmap *priv)
{
	kgem_bo_destroy(&sna->kgem, priv->gpu_bo);
	priv->gpu_bo = NULL;
}

static Bool sna_destroy_private(PixmapPtr pixmap, struct sna_pixmap *priv)
{
	struct sna *sna = to_sna_from_drawable(&pixmap->drawable);

	list_del(&priv->list);

	sna_damage_destroy(&priv->gpu_damage);
	sna_damage_destroy(&priv->cpu_damage);

	if (priv->mapped)
		munmap(pixmap->devPrivate.ptr, priv->gpu_bo->size);

	/* Always release the gpu bo back to the lower levels of caching */
	if (priv->gpu_bo)
		kgem_bo_destroy(&sna->kgem, priv->gpu_bo);

	if (priv->cpu_bo) {
		if (pixmap->usage_hint != CREATE_PIXMAP_USAGE_SCRATCH_HEADER &&
		    kgem_bo_is_busy(&sna->kgem, priv->cpu_bo)) {
			list_add_tail(&priv->list, &sna->deferred_free);
			return false;
		}
		kgem_bo_sync(&sna->kgem, priv->cpu_bo, true);
		kgem_bo_destroy(&sna->kgem, priv->cpu_bo);
	}

	free(priv);
	return TRUE;
}

static uint32_t sna_pixmap_choose_tiling(PixmapPtr pixmap)
{
	struct sna *sna = to_sna_from_drawable(&pixmap->drawable);
	uint32_t tiling, bit;

	/* Use tiling by default, but disable per user request */
	tiling = I915_TILING_X;
	bit = pixmap->usage_hint == SNA_CREATE_FB ?
		SNA_TILING_FB : SNA_TILING_2D;
	if ((sna->tiling && (1 << bit)) == 0)
		tiling = I915_TILING_NONE;

	if (pixmap->usage_hint == SNA_CREATE_FB)
		tiling = -tiling;

	/* Also adjust tiling if it is not supported or likely to
	 * slow us down,
	 */
	return kgem_choose_tiling(&sna->kgem, tiling,
				  pixmap->drawable.width,
				  pixmap->drawable.height,
				  pixmap->drawable.bitsPerPixel);
}

static struct sna_pixmap *_sna_pixmap_attach(PixmapPtr pixmap)
{
	struct sna_pixmap *priv;

	priv = calloc(1, sizeof(*priv));
	if (!priv)
		return NULL;

	list_init(&priv->list);
	priv->pixmap = pixmap;

	sna_set_pixmap(pixmap, priv);
	return priv;
}

struct sna_pixmap *sna_pixmap_attach(PixmapPtr pixmap)
{
	struct sna *sna;
	struct sna_pixmap *priv;

	priv = sna_pixmap(pixmap);
	if (priv)
		return priv;

	switch (pixmap->usage_hint) {
	case CREATE_PIXMAP_USAGE_GLYPH_PICTURE:
#if FAKE_CREATE_PIXMAP_USAGE_SCRATCH_HEADER
	case CREATE_PIXMAP_USAGE_SCRATCH_HEADER:
#endif
		return NULL;
	}

	sna = to_sna_from_drawable(&pixmap->drawable);
	if (!kgem_can_create_2d(&sna->kgem,
				pixmap->drawable.width,
				pixmap->drawable.height,
				pixmap->drawable.bitsPerPixel,
				sna_pixmap_choose_tiling(pixmap)))
		return NULL;

	return _sna_pixmap_attach(pixmap);
}

static PixmapPtr
sna_pixmap_create_scratch(ScreenPtr screen,
			  int width, int height, int depth,
			  uint32_t tiling)
{
	struct sna *sna = to_sna_from_screen(screen);
	PixmapPtr pixmap;
	struct sna_pixmap *priv;
	int bpp = BitsPerPixel(depth);

	DBG(("%s(%d, %d, %d, tiling=%d)\n", __FUNCTION__,
	     width, height, depth, tiling));

	if (tiling == I915_TILING_Y && !sna->have_render)
		tiling = I915_TILING_X;

	tiling = kgem_choose_tiling(&sna->kgem, tiling, width, height, bpp);
	if (!kgem_can_create_2d(&sna->kgem, width, height, bpp, tiling))
		return fbCreatePixmap(screen, width, height, depth,
				      CREATE_PIXMAP_USAGE_SCRATCH);

	/* you promise never to access this via the cpu... */
	pixmap = fbCreatePixmap(screen, 0, 0, depth,
				CREATE_PIXMAP_USAGE_SCRATCH);
	if (!pixmap)
		return NullPixmap;

	priv = malloc(sizeof(*priv));
	if (!priv) {
		fbDestroyPixmap(pixmap);
		return NullPixmap;
	}

	priv->gpu_bo = kgem_create_2d(&sna->kgem,
				      width, height, bpp, tiling,
				      0);
	if (priv->gpu_bo == NULL) {
		free(priv);
		fbDestroyPixmap(pixmap);
		return NullPixmap;
	}

	priv->source_count = 0;
	priv->cpu_bo = NULL;
	priv->cpu_damage = priv->gpu_damage = NULL;
	priv->gpu_only = 1;
	priv->pinned = 0;
	priv->mapped = 0;
	list_init(&priv->list);

	priv->pixmap = pixmap;
	sna_set_pixmap(pixmap, priv);

	miModifyPixmapHeader(pixmap,
			     width, height, depth, bpp,
			     priv->gpu_bo->pitch, NULL);
	pixmap->devPrivate.ptr = NULL;

	return pixmap;
}


static PixmapPtr sna_create_pixmap(ScreenPtr screen,
				   int width, int height, int depth,
				   unsigned int usage)
{
	PixmapPtr pixmap;

	DBG(("%s(%d, %d, %d, usage=%x)\n", __FUNCTION__,
	     width, height, depth, usage));

	if (usage == CREATE_PIXMAP_USAGE_SCRATCH &&
	    to_sna_from_screen(screen)->have_render)
		return sna_pixmap_create_scratch(screen,
						 width, height, depth,
						 I915_TILING_Y);

#if FAKE_CREATE_PIXMAP_USAGE_SCRATCH_HEADER
	if (width == 0 || height == 0)
		usage = CREATE_PIXMAP_USAGE_SCRATCH_HEADER;
#endif

	/* XXX could use last deferred free? */

	pixmap = fbCreatePixmap(screen, width, height, depth, usage);
	if (pixmap == NullPixmap)
		return NullPixmap;

/* XXX if (pixmap->drawable.devKind * height > 128) */
	sna_pixmap_attach(pixmap);
	return pixmap;
}

static Bool sna_destroy_pixmap(PixmapPtr pixmap)
{
	if (pixmap->refcnt == 1) {
		struct sna_pixmap *priv = sna_pixmap(pixmap);
		if (priv) {
			if (!sna_destroy_private(pixmap, priv))
				return TRUE;
		}
	}

	return fbDestroyPixmap(pixmap);
}

static void sna_pixmap_map_to_cpu(struct sna *sna,
				 PixmapPtr pixmap,
				 struct sna_pixmap *priv)
{
	DBG(("%s: AWOOGA, AWOOGA!\n", __FUNCTION__));

	if (priv->mapped == 0) {
		ScreenPtr screen = pixmap->drawable.pScreen;
		void *ptr;

		ptr = kgem_bo_map(&sna->kgem,
				  priv->gpu_bo,
				  PROT_READ | PROT_WRITE);
		assert(ptr != NULL);

		screen->ModifyPixmapHeader(pixmap,
					   pixmap->drawable.width,
					   pixmap->drawable.height,
					   pixmap->drawable.depth,
					   pixmap->drawable.bitsPerPixel,
					   priv->gpu_bo->pitch,
					   ptr);
		priv->mapped = 1;
	}
	kgem_bo_submit(&sna->kgem, priv->gpu_bo);
}

static inline void list_move(struct list *list, struct list *head)
{
	__list_del(list->prev, list->next);
	list_add(list, head);
}

void
sna_pixmap_move_to_cpu(PixmapPtr pixmap, bool write)
{
	struct sna *sna = to_sna_from_drawable(&pixmap->drawable);
	struct sna_pixmap *priv;

	DBG(("%s(pixmap=%p, write=%d)\n", __FUNCTION__, pixmap, write));

	priv = sna_pixmap(pixmap);
	if (priv == NULL) {
		DBG(("%s: not attached to %p\n", __FUNCTION__, pixmap));
		return;
	}

	DBG(("%s: gpu_bo=%p, gpu_damage=%p, gpu_only=%d\n",
	     __FUNCTION__, priv->gpu_bo, priv->gpu_damage, priv->gpu_only));

	if (priv->gpu_bo == NULL) {
		DBG(("%s: no GPU bo\n", __FUNCTION__));
		goto done;
	}

	if (priv->gpu_only) {
		sna_pixmap_map_to_cpu(sna, pixmap, priv);
		goto done;
	}

	if (priv->gpu_damage) {
		BoxPtr box;
		int n;

		DBG(("%s: flushing GPU damage\n", __FUNCTION__));

		n = sna_damage_get_boxes(priv->gpu_damage, &box);
		if (n) {
			struct kgem_bo *dst_bo;
			Bool ok = FALSE;

			dst_bo = NULL;
			if (sna->kgem.gen >= 30)
				dst_bo = pixmap_vmap(&sna->kgem, pixmap);
			if (dst_bo)
				ok = sna->render.copy_boxes(sna, GXcopy,
							    pixmap, priv->gpu_bo, 0, 0,
							    pixmap, dst_bo, 0, 0,
							    box, n);
			if (!ok)
				sna_read_boxes(sna,
					       priv->gpu_bo, 0, 0,
					       pixmap, 0, 0,
					       box, n);
		}

		__sna_damage_destroy(priv->gpu_damage);
		priv->gpu_damage = NULL;
	}

done:
	if (priv->cpu_bo) {
		DBG(("%s: syncing CPU bo\n", __FUNCTION__));
		kgem_bo_sync(&sna->kgem, priv->cpu_bo, write);
	}

	if (write) {
		DBG(("%s: marking as damaged\n", __FUNCTION__));
		sna_damage_all(&priv->cpu_damage,
			       pixmap->drawable.width,
			       pixmap->drawable.height);

		if (priv->gpu_bo && !priv->pinned)
			sna_pixmap_destroy_gpu_bo(sna, priv);

		if (priv->flush)
			list_move(&priv->list, &sna->dirty_pixmaps);
	}
}

static Bool
region_subsumes_drawable(RegionPtr region, DrawablePtr drawable)
{
	const BoxRec *extents;

	if (REGION_NUM_RECTS(region) != 1)
		return false;

	extents = RegionExtents(region);
	return  extents->x1 <= 0 && extents->y1 <= 0 &&
		extents->x2 >= drawable->width &&
		extents->y2 >= drawable->height;
}

void
sna_drawable_move_region_to_cpu(DrawablePtr drawable,
				RegionPtr region,
				Bool write)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_pixmap *priv;
	int16_t dx, dy;

	DBG(("%s(pixmap=%p (%dx%d), [(%d, %d), (%d, %d)], write=%d)\n",
	     __FUNCTION__, pixmap,
	     pixmap->drawable.width, pixmap->drawable.height,
	     RegionExtents(region)->x1, RegionExtents(region)->y1,
	     RegionExtents(region)->x2, RegionExtents(region)->y2,
	     write));

	priv = sna_pixmap(pixmap);
	if (priv == NULL) {
		DBG(("%s: not attached to %p\n", __FUNCTION__, pixmap));
		return;
	}

	if (priv->gpu_only) {
		DBG(("%s: gpu only\n", __FUNCTION__));
		return sna_pixmap_map_to_cpu(sna, pixmap, priv);
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	DBG(("%s: delta=(%d, %d)\n", __FUNCTION__, dx, dy));
	if (dx | dy)
		RegionTranslate(region, dx, dy);

	if (region_subsumes_drawable(region, &pixmap->drawable)) {
		DBG(("%s: region subsumes drawable\n", __FUNCTION__));
		if (dx | dy)
			RegionTranslate(region, -dx, -dy);
		return sna_pixmap_move_to_cpu(pixmap, write);
	}

#if 0
	pixman_region_intersect_rect(region, region,
				     0, 0,
				     pixmap->drawable.width,
				     pixmap->drawable.height);
#endif

	if (priv->gpu_bo == NULL)
		goto done;

	if (sna_damage_contains_box(priv->gpu_damage,
				    REGION_EXTENTS(NULL, region))) {
		RegionRec want, need, *r;

		DBG(("%s: region intersects gpu damage\n", __FUNCTION__));

		r = region;
		/* expand the region to move 32x32 pixel blocks at a time */
		if (priv->cpu_damage == NULL) {
			int n = REGION_NUM_RECTS(region), i;
			BoxPtr boxes = REGION_RECTS(region);
			BoxPtr blocks = malloc(sizeof(BoxRec) * REGION_NUM_RECTS(region));
			if (blocks) {
				for (i = 0; i < n; i++) {
					blocks[i].x1 = boxes[i].x1 & ~31;
					if (blocks[i].x1 < 0)
						blocks[i].x1 = 0;

					blocks[i].x2 = (boxes[i].x2 + 31) & ~31;
					if (blocks[i].x2 > pixmap->drawable.width)
						blocks[i].x2 = pixmap->drawable.width;

					blocks[i].y1 = boxes[i].y1 & ~31;
					if (blocks[i].y1 < 0)
						blocks[i].y1 = 0;

					blocks[i].y2 = (boxes[i].y2 + 31) & ~31;
					if (blocks[i].y2 > pixmap->drawable.height)
						blocks[i].y2 = pixmap->drawable.height;
				}
				if (pixman_region_init_rects(&want, blocks, i))
					r = &want;
				free(blocks);
			}
		}

		pixman_region_init(&need);
		if (sna_damage_intersect(priv->gpu_damage, r, &need)) {
			BoxPtr box = REGION_RECTS(&need);
			int n = REGION_NUM_RECTS(&need);
			struct kgem_bo *dst_bo;
			Bool ok = FALSE;

			dst_bo = NULL;
			if (sna->kgem.gen >= 30)
				dst_bo = pixmap_vmap(&sna->kgem, pixmap);
			if (dst_bo)
				ok = sna->render.copy_boxes(sna, GXcopy,
							    pixmap, priv->gpu_bo, 0, 0,
							    pixmap, dst_bo, 0, 0,
							    box, n);
			if (!ok)
				sna_read_boxes(sna,
					       priv->gpu_bo, 0, 0,
					       pixmap, 0, 0,
					       box, n);

			sna_damage_subtract(&priv->gpu_damage,
					    n <= REGION_NUM_RECTS(r) ? &need : r);
			RegionUninit(&need);
		}
		if (r == &want)
			pixman_region_fini(&want);
	}

done:
	if (priv->cpu_bo) {
		DBG(("%s: syncing cpu bo\n", __FUNCTION__));
		kgem_bo_sync(&sna->kgem, priv->cpu_bo, write);
	}

	if (write) {
		DBG(("%s: applying cpu damage\n", __FUNCTION__));
		assert_pixmap_contains_box(pixmap, RegionExtents(region));
		sna_damage_add(&priv->cpu_damage, region);
		if (priv->flush)
			list_move(&priv->list, &sna->dirty_pixmaps);
	}

	if (dx | dy)
		RegionTranslate(region, -dx, -dy);
}

static inline Bool
_sna_drawable_use_gpu_bo(DrawablePtr drawable, const BoxPtr box)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	BoxRec extents;
	int16_t dx, dy;

	if (priv == NULL)
		return FALSE;
	if (priv->gpu_bo == NULL)
		return FALSE;

	if (priv->cpu_damage == NULL)
		return TRUE;

	assert(!priv->gpu_only);
	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	extents = *box;
	extents.x1 += dx;
	extents.x2 += dx;
	extents.y1 += dy;
	extents.y2 += dy;

	return sna_damage_contains_box(priv->cpu_damage,
				       &extents) == PIXMAN_REGION_OUT;
}

static inline Bool
sna_drawable_use_gpu_bo(DrawablePtr drawable, const BoxPtr box)
{
	Bool ret = _sna_drawable_use_gpu_bo(drawable, box);
	DBG(("%s((%d, %d), (%d, %d)) = %d\n", __FUNCTION__,
	     box->x1, box->y1, box->x2, box->y2, ret));
	return ret;
}

static inline Bool
_sna_drawable_use_cpu_bo(DrawablePtr drawable, const BoxPtr box)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	BoxRec extents;
	int16_t dx, dy;

	if (priv == NULL)
		return FALSE;
	if (priv->cpu_bo == NULL)
		return FALSE;

	if (priv->gpu_damage == NULL)
		return TRUE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	extents = *box;
	extents.x1 += dx;
	extents.x2 += dx;
	extents.y1 += dy;
	extents.y2 += dy;

	return sna_damage_contains_box(priv->gpu_damage,
				       &extents) == PIXMAN_REGION_OUT;
}

static inline Bool
sna_drawable_use_cpu_bo(DrawablePtr drawable, const BoxPtr box)
{
	Bool ret = _sna_drawable_use_cpu_bo(drawable, box);
	DBG(("%s((%d, %d), (%d, %d)) = %d\n", __FUNCTION__,
	     box->x1, box->y1, box->x2, box->y2, ret));
	return ret;
}

PixmapPtr
sna_pixmap_create_upload(ScreenPtr screen,
			 int width, int height, int depth)
{
	struct sna *sna = to_sna_from_screen(screen);
	PixmapPtr pixmap;
	struct sna_pixmap *priv;
	int bpp = BitsPerPixel(depth);
	int pad = ALIGN(width * bpp / 8, 4);
	void *ptr;

	DBG(("%s(%d, %d, %d)\n", __FUNCTION__, width, height, depth));
	assert(width);
	assert(height);
	if (!sna->have_render ||
	    !kgem_can_create_2d(&sna->kgem,
				width, height, bpp,
				I915_TILING_NONE))
		return fbCreatePixmap(screen, width, height, depth,
				      CREATE_PIXMAP_USAGE_SCRATCH);

	pixmap = fbCreatePixmap(screen, 0, 0, depth,
				CREATE_PIXMAP_USAGE_SCRATCH);
	if (!pixmap)
		return NullPixmap;

	priv = malloc(sizeof(*priv));
	if (!priv) {
		fbDestroyPixmap(pixmap);
		return NullPixmap;
	}

	priv->gpu_bo = kgem_create_buffer(&sna->kgem, pad*height, true, &ptr);
	if (!priv->gpu_bo) {
		free(priv);
		fbDestroyPixmap(pixmap);
		return NullPixmap;
	}

	priv->gpu_bo->pitch = pad;

	priv->source_count = SOURCE_BIAS;
	priv->cpu_bo = NULL;
	priv->cpu_damage = priv->gpu_damage = NULL;
	priv->gpu_only = 0;
	priv->pinned = 0;
	priv->mapped = 0;
	list_init(&priv->list);

	priv->pixmap = pixmap;
	sna_set_pixmap(pixmap, priv);

	miModifyPixmapHeader(pixmap, width, height, depth, bpp, pad, ptr);
	return pixmap;
}

struct sna_pixmap *
sna_pixmap_force_to_gpu(PixmapPtr pixmap)
{
	struct sna_pixmap *priv;

	DBG(("%s(pixmap=%p)\n", __FUNCTION__, pixmap));

	priv = sna_pixmap(pixmap);
	if (priv == NULL) {
		priv = sna_pixmap_attach(pixmap);
		if (priv == NULL)
			return NULL;

		DBG(("%s: created priv and marking all cpu damaged\n",
		     __FUNCTION__));
		sna_damage_all(&priv->cpu_damage,
				 pixmap->drawable.width,
				 pixmap->drawable.height);
	}

	if (priv->gpu_bo == NULL) {
		struct sna *sna = to_sna_from_drawable(&pixmap->drawable);

		priv->gpu_bo = kgem_create_2d(&sna->kgem,
					      pixmap->drawable.width,
					      pixmap->drawable.height,
					      pixmap->drawable.bitsPerPixel,
					      sna_pixmap_choose_tiling(pixmap),
					      priv->cpu_damage ? CREATE_INACTIVE : 0);
		if (priv->gpu_bo == NULL)
			return NULL;

		DBG(("%s: created gpu bo\n", __FUNCTION__));
	}

	if (!sna_pixmap_move_to_gpu(pixmap))
		return NULL;

	return priv;
}

struct sna_pixmap *
sna_pixmap_move_to_gpu(PixmapPtr pixmap)
{
	struct sna *sna = to_sna_from_drawable(&pixmap->drawable);
	struct sna_pixmap *priv;
	BoxPtr box;
	int n;

	DBG(("%s()\n", __FUNCTION__));

	priv = sna_pixmap(pixmap);
	if (priv == NULL)
		return NULL;

	sna_damage_reduce(&priv->cpu_damage);
	DBG(("%s: CPU damage? %d\n", __FUNCTION__, priv->cpu_damage != NULL));

	if (priv->gpu_bo == NULL) {
		if (!sna->kgem.wedged)
			priv->gpu_bo =
				kgem_create_2d(&sna->kgem,
					       pixmap->drawable.width,
					       pixmap->drawable.height,
					       pixmap->drawable.bitsPerPixel,
					       sna_pixmap_choose_tiling(pixmap),
					       priv->cpu_damage ? CREATE_INACTIVE : 0);
		if (priv->gpu_bo == NULL) {
			assert(list_is_empty(&priv->list));
			return NULL;
		}
	}

	if (priv->cpu_damage == NULL)
		goto done;

	n = sna_damage_get_boxes(priv->cpu_damage, &box);
	if (n) {
		struct kgem_bo *src_bo;
		Bool ok = FALSE;

		src_bo = pixmap_vmap(&sna->kgem, pixmap);
		if (src_bo)
			ok = sna->render.copy_boxes(sna, GXcopy,
						    pixmap, src_bo, 0, 0,
						    pixmap, priv->gpu_bo, 0, 0,
						    box, n);
		if (!ok) {
			if (n == 1 && !priv->pinned &&
			    box->x1 <= 0 && box->y1 <= 0 &&
			    box->x2 >= pixmap->drawable.width &&
			    box->y2 >= pixmap->drawable.height) {
				priv->gpu_bo =
					sna_replace(sna,
						    priv->gpu_bo,
						    pixmap->drawable.width,
						    pixmap->drawable.height,
						    pixmap->drawable.bitsPerPixel,
						    pixmap->devPrivate.ptr,
						    pixmap->devKind);
			} else {
				sna_write_boxes(sna,
						priv->gpu_bo, 0, 0,
						pixmap->devPrivate.ptr,
						pixmap->devKind,
						pixmap->drawable.bitsPerPixel,
						0, 0,
						box, n);
			}
		}
	}

	__sna_damage_destroy(priv->cpu_damage);
	priv->cpu_damage = NULL;

	sna_damage_reduce(&priv->gpu_damage);
done:
	list_del(&priv->list);
	return priv;
}

static void sna_gc_move_to_cpu(GCPtr gc)
{
	DBG(("%s\n", __FUNCTION__));

	if (gc->stipple)
		sna_drawable_move_to_cpu(&gc->stipple->drawable, false);

	if (gc->fillStyle == FillTiled)
		sna_drawable_move_to_cpu(&gc->tile.pixmap->drawable, false);
}

static inline void trim_box(BoxPtr box, DrawablePtr d)
{
	if (box->x1 < 0)
		box->x1 = 0;
	if (box->x2 > d->width)
		box->x2 = d->width;

	if (box->y1 < 0)
		box->y1 = 0;
	if (box->y2 > d->height)
		box->y2 = d->height;
}

static inline void clip_box(BoxPtr box, GCPtr gc)
{
	const BoxRec *clip;

	if (!gc->pCompositeClip)
		return;

	clip = &gc->pCompositeClip->extents;

	if (box->x1 < clip->x1)
		box->x1 = clip->x1;
	if (box->x2 > clip->x2)
		box->x2 = clip->x2;

	if (box->y1 < clip->y1)
		box->y1 = clip->y1;
	if (box->y2 > clip->y2)
		box->y2 = clip->y2;
}

static inline void translate_box(BoxPtr box, DrawablePtr d)
{
	box->x1 += d->x;
	box->x2 += d->x;

	box->y1 += d->y;
	box->y2 += d->y;
}

static inline void trim_and_translate_box(BoxPtr box, DrawablePtr d, GCPtr gc)
{
	trim_box(box, d);
	translate_box(box, d);
	clip_box(box, gc);
}

static inline void box_add_pt(BoxPtr box, int16_t x, int16_t y)
{
	if (box->x1 > x)
		box->x1 = x;
	else if (box->x2 < x)
		box->x2 = x;

	if (box->y1 > y)
		box->y1 = y;
	else if (box->y2 < y)
		box->y2 = y;
}

static int16_t bound(int16_t a, uint16_t b)
{
	int v = (int)a + (int)b;
	if (v > MAXSHORT)
		return MAXSHORT;
	return v;
}

static inline void box_add_rect(BoxPtr box, const xRectangle *r)
{
	int v;

	if (box->x1 > r->x)
		box->x1 = r->x;
	v = bound(r->x, r->width);
	if (box->x2 < v)
		box->x2 = v;

	if (box->y1 > r->y)
		box->y1 = r->y;
	v =bound(r->y, r->height);
	if (box->y2 < v)
		box->y2 = v;
}

static inline bool box_empty(const BoxRec *box)
{
	return box->x2 <= box->x1 || box->y2 <= box->y1;
}

static Bool
sna_put_image_upload_blt(DrawablePtr drawable, GCPtr gc, RegionPtr region,
			 int x, int y, int w, int  h, char *bits, int stride)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	struct kgem_bo *src_bo;
	Bool ok = FALSE;
	BoxPtr box;
	int nbox;
	int16_t dx, dy;

	box = REGION_RECTS(region);
	nbox = REGION_NUM_RECTS(region);

	DBG(("%s: %d x [(%d, %d), (%d, %d)...]\n",
	     __FUNCTION__, nbox,
	     box->x1, box->y1, box->x2, box->y2));

	if (gc->alu == GXcopy &&
	    !priv->pinned && nbox == 1 &&
	    box->x1 <= 0 && box->y1 <= 0 &&
	    box->x2 >= pixmap->drawable.width &&
	    box->y2 >= pixmap->drawable.height) {
		priv->gpu_bo =
			sna_replace(sna, priv->gpu_bo,
				    pixmap->drawable.width,
				    pixmap->drawable.height,
				    pixmap->drawable.bitsPerPixel,
				    bits, stride);
		return TRUE;
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	x += dx + drawable->x;
	y += dy + drawable->y;

	src_bo = kgem_create_map(&sna->kgem, bits, stride*h, 1);
	if (src_bo) {
		src_bo->pitch = stride;
		ok = sna->render.copy_boxes(sna, gc->alu,
					    pixmap, src_bo, -x, -y,
					    pixmap, priv->gpu_bo, 0, 0,
					    box, nbox);
		kgem_bo_sync(&sna->kgem, src_bo, true);
		kgem_bo_destroy(&sna->kgem, src_bo);
	}

	if (!ok && gc->alu == GXcopy) {
		sna_write_boxes(sna,
				priv->gpu_bo, 0, 0,
				bits,
				stride,
				pixmap->drawable.bitsPerPixel,
				-x, -y,
				box, nbox);
		ok = TRUE;
	}

	return ok;
}

static Bool
sna_put_image_blt(DrawablePtr drawable, GCPtr gc, RegionPtr region,
		  int x, int y, int w, int  h, char *bits, int stride)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	int16_t dx, dy;

	if (!priv->gpu_bo)
		return false;

	if (priv->gpu_only)
		return sna_put_image_upload_blt(drawable, gc, region,
						x, y, w, h, bits, stride);

	if (gc->alu != GXcopy)
		return false;

	if (priv->gpu_bo->rq == NULL &&
	    sna_put_image_upload_blt(drawable, gc, region,
				     x, y, w, h, bits, stride)) {
		if (region_subsumes_drawable(region, &pixmap->drawable)) {
			sna_damage_destroy(&priv->cpu_damage);
			sna_damage_all(&priv->gpu_damage,
				       pixmap->drawable.width,
				       pixmap->drawable.height);
		} else {
			assert_pixmap_contains_box(pixmap, RegionExtents(region));
			sna_damage_subtract(&priv->cpu_damage, region);
			sna_damage_add(&priv->gpu_damage, region);
		}

		return true;
	}

	if (priv->cpu_bo)
		kgem_bo_sync(&sna->kgem, priv->cpu_bo, true);

	if (region_subsumes_drawable(region, &pixmap->drawable)) {
		sna_damage_destroy(&priv->gpu_damage);
		sna_damage_all(&priv->cpu_damage,
			       pixmap->drawable.width,
			       pixmap->drawable.height);
		if (priv->gpu_bo && !priv->pinned)
			sna_pixmap_destroy_gpu_bo(sna, priv);
	} else {
		assert_pixmap_contains_box(pixmap, RegionExtents(region));
		sna_damage_subtract(&priv->gpu_damage, region);
		sna_damage_add(&priv->cpu_damage, region);
		if (priv->flush)
			list_move(&priv->list, &sna->dirty_pixmaps);
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	dx += drawable->x;
	dy += drawable->y;

	DBG(("%s: fbPutZImage(%d[+%d], %d[+%d], %d, %d)\n",
	     __FUNCTION__,
	     x+dx, pixmap->drawable.x,
	     y+dy, pixmap->drawable.y,
	     w, h));
	fbPutZImage(&pixmap->drawable, region,
		    GXcopy, ~0U,
		    x + dx, y + dy, w, h,
		    (FbStip*)bits, stride/sizeof(FbStip));
	return true;
}

static void
sna_put_image(DrawablePtr drawable, GCPtr gc, int depth,
	      int x, int y, int w, int h, int left, int format,
	      char *bits)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	RegionRec region, *clip;
	BoxRec box;
	int16_t dx, dy;

	DBG(("%s((%d, %d)x(%d, %d)\n", __FUNCTION__, x, y, w, h));

	if (w == 0 || h == 0)
		return;

	if (priv == NULL) {
		DBG(("%s: fbPutImage, unattached(%d, %d, %d, %d)\n",
		     __FUNCTION__, x, y, w, h));
		fbPutImage(drawable, gc, depth, x, y, w, h, left, format, bits);
		return;
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	box.x1 = x + drawable->x + dx;
	box.y1 = y + drawable->y + dy;
	box.x2 = box.x1 + w;
	box.y2 = box.y1 + h;

	if (box.x1 < 0)
		box.x1 = 0;
	if (box.y1 < 0)
		box.y1 = 0;
	if (box.x2 > pixmap->drawable.width)
		box.x2 = pixmap->drawable.width;
	if (box.y2 > pixmap->drawable.height)
		box.y2 = pixmap->drawable.height;
	if (box_empty(&box))
		return;

	RegionInit(&region, &box, 1);

	clip = fbGetCompositeClip(gc);
	if (clip) {
		RegionTranslate(clip, dx, dy);
		RegionIntersect(&region, &region, clip);
		RegionTranslate(clip, -dx, -dy);
	}

	if (RegionNotEmpty(&region) &&
	    (format != ZPixmap || !PM_IS_SOLID(drawable, gc->planemask) ||
	     !sna_put_image_blt(drawable, gc, &region,
				x, y, w, h,
				bits, PixmapBytePad(w, depth)))) {
		RegionTranslate(&region, -dx, -dy);

		sna_drawable_move_region_to_cpu(drawable, &region, true);
		DBG(("%s: fbPutImage(%d, %d, %d, %d)\n",
		     __FUNCTION__, x, y, w, h));
		fbPutImage(drawable, gc, depth, x, y, w, h, left, format, bits);
	}

	RegionUninit(&region);
}

static void
sna_copy_boxes(DrawablePtr src, DrawablePtr dst, GCPtr gc,
	       BoxPtr box, int n,
	       int dx, int dy,
	       Bool reverse, Bool upsidedown, Pixel bitplane,
	       void *closure)
{
	struct sna *sna = to_sna_from_drawable(dst);
	PixmapPtr src_pixmap = get_drawable_pixmap(src);
	PixmapPtr dst_pixmap = get_drawable_pixmap(dst);
	struct sna_pixmap *src_priv = sna_pixmap(src_pixmap);
	struct sna_pixmap *dst_priv = sna_pixmap(dst_pixmap);
	int alu = gc ? gc->alu : GXcopy;
	int16_t src_dx, src_dy;
	int16_t dst_dx, dst_dy;
	int stride, bpp;
	char *bits;
	RegionRec region;
	Bool replaces;

	if (n == 0)
		return;

	DBG(("%s (boxes=%dx[(%d, %d), (%d, %d)...], src=+(%d, %d), alu=%d, src.size=%dx%d, dst.size=%dx%d)\n",
	     __FUNCTION__, n,
	     box[0].x1, box[0].y1, box[0].x2, box[0].y2,
	     dx, dy, alu,
	     src_pixmap->drawable.width, src_pixmap->drawable.height,
	     dst_pixmap->drawable.width, dst_pixmap->drawable.height));

	pixman_region_init_rects(&region, box, n);

	bpp = dst_pixmap->drawable.bitsPerPixel;

	get_drawable_deltas(dst, dst_pixmap, &dst_dx, &dst_dy);
	get_drawable_deltas(src, src_pixmap, &src_dx, &src_dy);
	src_dx += dx;
	src_dy += dy;

	replaces = alu == GXcopy && n == 1 &&
		box->x1 + dst_dx <= 0 &&
		box->y1 + dst_dy <= 0 &&
		box->x2 + dst_dx >= dst_pixmap->drawable.width &&
		box->y2 + dst_dy >= dst_pixmap->drawable.height;

	DBG(("%s: dst=(priv=%p, gpu_bo=%p, cpu_bo=%p), src=(priv=%p, gpu_bo=%p, cpu_bo=%p), replaces=%d\n",
	     __FUNCTION__,
	     dst_priv,
	     dst_priv ? dst_priv->gpu_bo : NULL,
	     dst_priv ? dst_priv->cpu_bo : NULL,
	     src_priv,
	     src_priv ? src_priv->gpu_bo : NULL,
	     src_priv ? src_priv->cpu_bo : NULL,
	     replaces));

	/* Try to maintain the data on the GPU */
	if (dst_priv && dst_priv->gpu_bo == NULL &&
	    src_priv && src_priv->gpu_bo != NULL &&
	    alu == GXcopy) {
		uint32_t tiling =
			sna_pixmap_choose_tiling(dst_pixmap);

		DBG(("%s: create dst GPU bo for copy\n", __FUNCTION__));

		if (kgem_can_create_2d(&sna->kgem,
				       dst_pixmap->drawable.width,
				       dst_pixmap->drawable.height,
				       dst_pixmap->drawable.bitsPerPixel,
				       tiling))
			dst_priv->gpu_bo =
				kgem_create_2d(&sna->kgem,
					       dst_pixmap->drawable.width,
					       dst_pixmap->drawable.height,
					       dst_pixmap->drawable.bitsPerPixel,
					       tiling, 0);
	}

	if (dst_priv && dst_priv->gpu_bo) {
		if (!src_priv && !dst_priv->gpu_only) {
			DBG(("%s: fallback - src_priv=%p but dst gpu_only=%d\n",
			     __FUNCTION__,
			     src_priv, dst_priv->gpu_only));
			goto fallback;
		}

		if (alu != GXcopy && !sna_pixmap_move_to_gpu(dst_pixmap)) {
			DBG(("%s: fallback - not a pure copy and failed to move dst to GPU\n",
			     __FUNCTION__));
			goto fallback;
		}

		if (src_priv && src_priv->gpu_bo &&
		    sna_pixmap_move_to_gpu(src_pixmap)) {
			if (!sna->render.copy_boxes(sna, alu,
						    src_pixmap, src_priv->gpu_bo, src_dx, src_dy,
						    dst_pixmap, dst_priv->gpu_bo, dst_dx, dst_dy,
						    box, n)) {
				DBG(("%s: fallback - accelerated copy boxes failed\n",
				     __FUNCTION__));
				goto fallback;
			}

			if (replaces) {
				sna_damage_destroy(&dst_priv->cpu_damage);
				sna_damage_all(&dst_priv->gpu_damage,
					       dst_pixmap->drawable.width,
					       dst_pixmap->drawable.height);
			} else {
				RegionTranslate(&region, dst_dx, dst_dy);
				assert_pixmap_contains_box(dst_pixmap,
							   RegionExtents(&region));
				sna_damage_add(&dst_priv->gpu_damage, &region);
				if (alu == GXcopy)
					sna_damage_subtract(&dst_priv->cpu_damage,
							    &region);
				RegionTranslate(&region, -dst_dx, -dst_dy);
			}
		} else {
			if (alu != GXcopy) {
				DBG(("%s: fallback - not a copy and source is on the CPU\n",
				     __FUNCTION__));
				goto fallback;
			}

			if (src_priv) {
				RegionTranslate(&region, src_dx, src_dy);
				sna_drawable_move_region_to_cpu(&src_pixmap->drawable,
								&region, false);
				RegionTranslate(&region, -src_dx, -src_dy);
			}

			if (!dst_priv->pinned && replaces) {
				stride = src_pixmap->devKind;
				bits = src_pixmap->devPrivate.ptr;
				bits += src_dy * stride + src_dx * bpp / 8;

				dst_priv->gpu_bo =
					sna_replace(sna,
						    dst_priv->gpu_bo,
						    dst_pixmap->drawable.width,
						    dst_pixmap->drawable.height,
						    bpp, bits, stride);

				sna_damage_destroy(&dst_priv->cpu_damage);
				sna_damage_all(&dst_priv->gpu_damage,
					       dst_pixmap->drawable.width,
					       dst_pixmap->drawable.height);
			} else {
				DBG(("%s: dst is on the GPU, src is on the CPU, uploading\n",
				     __FUNCTION__));
				sna_write_boxes(sna,
						dst_priv->gpu_bo, dst_dx, dst_dy,
						src_pixmap->devPrivate.ptr,
						src_pixmap->devKind,
						src_pixmap->drawable.bitsPerPixel,
						src_dx, src_dy,
						box, n);

				RegionTranslate(&region, dst_dx, dst_dy);
				assert_pixmap_contains_box(dst_pixmap,
							   RegionExtents(&region));
				sna_damage_add(&dst_priv->gpu_damage,
					       &region);
				sna_damage_subtract(&dst_priv->cpu_damage,
						    &region);
				RegionTranslate(&region, -dst_dx, -dst_dy);
			}
		}
	} else {
		FbBits *dst_bits, *src_bits;
		int dst_stride, src_stride;

fallback:
		DBG(("%s: fallback -- src=(%d, %d), dst=(%d, %d)\n",
		     __FUNCTION__, src_dx, src_dy, dst_dx, dst_dy));
		if (src_priv) {
			RegionTranslate(&region, src_dx, src_dy);
			sna_drawable_move_region_to_cpu(&src_pixmap->drawable,
							&region, false);
			RegionTranslate(&region, -src_dx, -src_dy);
		}

		RegionTranslate(&region, dst_dx, dst_dy);
		if (dst_priv) {
			if (alu == GXcopy) {
				if (replaces) {
					sna_damage_destroy(&dst_priv->gpu_damage);
					sna_damage_all(&dst_priv->cpu_damage,
						       dst_pixmap->drawable.width,
						       dst_pixmap->drawable.height);
					if (dst_priv->gpu_bo && !dst_priv->pinned)
						sna_pixmap_destroy_gpu_bo(sna, dst_priv);
				} else {
					assert_pixmap_contains_box(dst_pixmap,
								   RegionExtents(&region));
					sna_damage_subtract(&dst_priv->gpu_damage,
							    &region);
					sna_damage_add(&dst_priv->cpu_damage,
						       &region);
					if (dst_priv->flush)
						list_move(&dst_priv->list,
							  &sna->dirty_pixmaps);
				}
			} else
				sna_drawable_move_region_to_cpu(&dst_pixmap->drawable,
								&region, true);
		}

		dst_stride = dst_pixmap->devKind;
		src_stride = src_pixmap->devKind;

		if (alu == GXcopy && !reverse && !upsidedown && bpp >= 8) {
			dst_bits = (FbBits *)
				((char *)dst_pixmap->devPrivate.ptr +
				 dst_dy * dst_stride + dst_dx * bpp / 8);
			src_bits = (FbBits *)
				((char *)src_pixmap->devPrivate.ptr +
				 src_dy * src_stride + src_dx * bpp / 8);

			do {
				DBG(("%s: memcpy_blt(box=(%d, %d), (%d, %d), src=(%d, %d), dst=(%d, %d), pitches=(%d, %d))\n",
				     __FUNCTION__,
				     box->x1, box->y1,
				     box->x2 - box->x1,
				     box->y2 - box->y1,
				     src_dx, src_dy,
				     dst_dx, dst_dy,
				     src_stride, dst_stride));
				memcpy_blt(src_bits, dst_bits, bpp,
					   src_stride, dst_stride,
					   box->x1, box->y1,
					   box->x1, box->y1,
					   box->x2 - box->x1,
					   box->y2 - box->y1);
				box++;
			} while (--n);
		} else {
			dst_bits = dst_pixmap->devPrivate.ptr;
			src_bits = src_pixmap->devPrivate.ptr;

			dst_stride /= sizeof(FbBits);
			src_stride /= sizeof(FbBits);
			do {
				DBG(("%s: fbBlt (%d, %d), (%d, %d)\n",
				     __FUNCTION__,
				     box->x1, box->y1,
				     box->x2, box->y2));
				fbBlt(src_bits + (box->y1 + src_dy) * src_stride,
				      src_stride,
				      (box->x1 + src_dx) * bpp,

				      dst_bits + (box->y1 + dst_dy) * dst_stride,
				      dst_stride,
				      (box->x1 + dst_dx) * bpp,

				      (box->x2 - box->x1) * bpp,
				      (box->y2 - box->y1),

				      alu, -1, bpp,
				      reverse, upsidedown);
				box++;
			}while (--n);
		}
	}
	RegionUninit(&region);
}

static RegionPtr
sna_copy_area(DrawablePtr src, DrawablePtr dst, GCPtr gc,
	      int src_x, int src_y,
	      int width, int height,
	      int dst_x, int dst_y)
{
	struct sna *sna = to_sna_from_drawable(dst);

	DBG(("%s: src=(%d, %d)x(%d, %d) -> dst=(%d, %d)\n",
	     __FUNCTION__, src_x, src_y, width, height, dst_x, dst_y));

	if (sna->kgem.wedged || !PM_IS_SOLID(dst, gc->planemask)) {
		BoxRec box;
		RegionRec region;

		DBG(("%s: -- fallback, wedged=%d, solid=%d [%x]\n",
		     __FUNCTION__, sna->kgem.wedged,
		     PM_IS_SOLID(dst, gc->planemask),
		     (unsigned)gc->planemask));

		box.x1 = dst_x + dst->x;
		box.y1 = dst_y + dst->y;
		box.x2 = box.x1 + width;
		box.y2 = box.y1 + height;
		RegionInit(&region, &box, 1);

		if (gc->pCompositeClip)
			RegionIntersect(&region, &region, gc->pCompositeClip);

		sna_drawable_move_region_to_cpu(dst, &region, true);
		RegionTranslate(&region,
				src_x - dst_x - dst->x + src->x,
				src_y - dst_y - dst->y + src->y);
		sna_drawable_move_region_to_cpu(src, &region, false);

		return fbCopyArea(src, dst, gc,
				  src_x, src_y,
				  width, height,
				  dst_x, dst_y);
	}

	return miDoCopy(src, dst, gc,
			src_x, src_y,
			width, height,
			dst_x, dst_y,
			sna_copy_boxes, 0, NULL);
}

static Bool
box_intersect(BoxPtr a, const BoxPtr b)
{
	if (a->x1 < b->x1)
		a->x1 = b->x1;
	if (a->x2 > b->x2)
		a->x2 = b->x2;
	if (a->y1 < b->y1)
		a->y1 = b->y1;
	if (a->y2 > b->y2)
		a->y2 = b->y2;

	return a->x1 < a->x2 && a->y1 < a->y2;
}

static Bool
sna_fill_init_blt(struct sna_fill_op *fill,
		  struct sna *sna,
		  PixmapPtr pixmap,
		  struct kgem_bo *bo,
		  uint8_t alu,
		  uint32_t pixel)
{
	memset(fill, 0, sizeof(*fill));
	return sna->render.fill(sna, alu, pixmap, bo, pixel, fill);
}

static Bool
sna_copy_init_blt(struct sna_copy_op *copy,
		  struct sna *sna,
		  PixmapPtr src, struct kgem_bo *src_bo,
		  PixmapPtr dst, struct kgem_bo *dst_bo,
		  uint8_t alu)
{
	memset(copy, 0, sizeof(*copy));
	return sna->render.copy(sna, alu, src, src_bo, dst, dst_bo, copy);
}

static Bool
sna_fill_spans_blt(DrawablePtr drawable,
		   struct kgem_bo *bo, struct sna_damage **damage,
		   GCPtr gc, int n,
		   DDXPointPtr pt, int *width, int sorted)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_fill_op fill;
	BoxPtr extents, clip;
	int nclip;
	int16_t dx, dy;

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return false;

	extents = REGION_EXTENTS(gc->screen, gc->pCompositeClip);
	DBG(("%s: clip %d x [(%d, %d), (%d, %d)] x %d [(%d, %d)...]\n",
	     __FUNCTION__,
	     REGION_NUM_RECTS(gc->pCompositeClip),
	     extents->x1, extents->y1, extents->x2, extents->y2,
	     n, pt->x, pt->y));

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	while (n--) {
		int X1 = pt->x;
		int y = pt->y;
		int X2 = X1 + (int)*width;

		if (!gc->miTranslate) {
			X1 += drawable->x;
			X2 += drawable->x;
			y += drawable->y;
		}

		pt++;
		width++;

		if (y < extents->y1 || extents->y2 <= y)
			continue;

		if (X1 < extents->x1)
			X1 = extents->x1;

		if (X2 > extents->x2)
			X2 = extents->x2;

		if (X1 >= X2)
			continue;

		nclip = REGION_NUM_RECTS(gc->pCompositeClip);
		if (nclip == 1) {
			X1 += dx;
			if (X1 < 0)
				X1 = 0;
			X2 += dx;
			if (X2 > pixmap->drawable.width)
				X2 = pixmap->drawable.width;
			if (X2 > X1) {
				fill.blt(sna, &fill, X1, y+dy, X2-X1, 1);
				if (damage) {
					BoxRec box;

					box.x1 = X1;
					box.x2 = X2;
					box.y1 = y + dy;
					box.y2 = box.y1 + 1;

					assert_pixmap_contains_box(pixmap, &box);
					sna_damage_add_box(damage, &box);
				}
			}
		} else {
			clip = REGION_RECTS(gc->pCompositeClip);
			while (nclip--) {
				if (clip->y1 <= y && y < clip->y2) {
					int x1 = clip->x1;
					int x2 = clip->x2;

					if (x1 < X1)
						x1 = X1;
					x1 += dx;
					if (x1 < 0)
						x1 = 0;
					if (x2 > X2)
						x2 = X2;
					x2 += dx;
					if (x2 > pixmap->drawable.width)
						x2 = pixmap->drawable.width;

					if (x2 > x1) {
						fill.blt(sna, &fill,
							 x1, y + dy,
							 x2-x1, 1);
						if (damage) {
							BoxRec box;

							box.x1 = x1;
							box.y1 = y + dy;
							box.x2 = x2;
							box.y2 = box.y1 + 1;

							assert_pixmap_contains_box(pixmap, &box);
							sna_damage_add_box(damage, &box);
						}
					}
				}
				clip++;
			}
		}
	}
	fill.done(sna, &fill);
	return TRUE;
}

static Bool
sna_spans_extents(DrawablePtr drawable, GCPtr gc,
		  int n, DDXPointPtr pt, int *width,
		  BoxPtr out)
{
	BoxRec box;

	if (n == 0)
		return true;

	box.x1 = pt->x;
	box.x2 = box.x1 + *width;
	box.y2 = box.y1 = pt->y;

	while (--n) {
		pt++;
		width++;
		if (box.x1 > pt->x)
			box.x1 = pt->x;
		if (box.x2 < pt->x + *width)
			box.x2 = pt->x + *width;

		if (box.y1 > pt->y)
			box.y1 = pt->y;
		else if (box.y2 < pt->y)
			box.y2 = pt->y;
	}
	box.y2++;

	if (gc) {
		if (!gc->miTranslate)
			translate_box(&box, drawable);
		clip_box(&box, gc);
	}
	*out = box;
	return box_empty(&box);
}

static void
sna_fill_spans(DrawablePtr drawable, GCPtr gc, int n,
	       DDXPointPtr pt, int *width, int sorted)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	BoxRec extents;
	RegionRec region;

	DBG(("%s(n=%d, pt[0]=(%d, %d)\n",
	     __FUNCTION__, n, pt[0].x, pt[0].y));

	if (sna_spans_extents(drawable, gc, n, pt, width, &extents))
		return;

	DBG(("%s: extents (%d, %d), (%d, %d)\n", __FUNCTION__,
	     extents.x1, extents.y1, extents.x2, extents.y2));

	if (sna->kgem.wedged) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (gc->fillStyle == FillSolid &&
	    PM_IS_SOLID(drawable, gc->planemask)) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);

		DBG(("%s: trying solid fill [alu=%d, pixel=%08lx] blt paths\n",
		     __FUNCTION__, gc->alu, gc->fgPixel));

		if (sna_drawable_use_gpu_bo(drawable, &extents) &&
		    sna_fill_spans_blt(drawable,
				       priv->gpu_bo,
				       priv->gpu_only ? NULL : &priv->gpu_damage,
				       gc, n, pt, width, sorted))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &extents) &&
		    sna_fill_spans_blt(drawable,
				       priv->cpu_bo, &priv->cpu_damage,
				       gc, n, pt, width, sorted))
			return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	RegionInit(&region, &extents, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	fbFillSpans(drawable, gc, n, pt, width, sorted);
}

static void
sna_set_spans(DrawablePtr drawable, GCPtr gc, char *src,
	      DDXPointPtr pt, int *width, int n, int sorted)
{
	BoxRec extents;
	RegionRec region;

	if (sna_spans_extents(drawable, gc, n, pt, width, &extents))
		return;

	DBG(("%s: extents=(%d, %d), (%d, %d)\n", __FUNCTION__,
	     extents.x1, extents.y1, extents.x2, extents.y2));

	RegionInit(&region, &extents, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	fbSetSpans(drawable, gc, src, pt, width, n, sorted);
}

static RegionPtr
sna_copy_plane(DrawablePtr src, DrawablePtr dst, GCPtr gc,
	       int src_x, int src_y,
	       int w, int h,
	       int dst_x, int dst_y,
	       unsigned long bit)
{
	BoxRec box;
	RegionRec region;

	DBG(("%s: src=(%d, %d), dst=(%d, %d), size=%dx%d\n", __FUNCTION__,
	     src_x, src_y, dst_x, dst_y, w, h));

	box.x1 = dst_x + dst->x;
	box.y1 = dst_y + dst->y;
	box.x2 = box.x1 + w;
	box.y2 = box.y1 + h;

	RegionInit(&region, &box, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);

	sna_drawable_move_region_to_cpu(dst, &region, true);
	RegionTranslate(&region,
			src_x - dst_x - dst->x + src->x,
			src_y - dst_y - dst->y + src->y);
	sna_drawable_move_region_to_cpu(src, &region, false);

	return fbCopyPlane(src, dst, gc, src_x, src_y, w, h, dst_x, dst_y, bit);
}

static Bool
sna_poly_point_blt(DrawablePtr drawable,
		   struct kgem_bo *bo,
		   struct sna_damage **damage,
		   GCPtr gc, int mode, int n, DDXPointPtr pt)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	RegionPtr clip = fbGetCompositeClip(gc);
	struct sna_fill_op fill;
	DDXPointRec last;
	int16_t dx, dy;

	DBG(("%s: alu=%d, pixel=%08lx\n", __FUNCTION__, gc->alu, gc->fgPixel));

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	last.x = drawable->x;
	last.y = drawable->y;

	while (n--) {
		int x, y;

		x = pt->x;
		y = pt->y;
		pt++;
		if (mode == CoordModePrevious) {
			x += last.x;
			y += last.x;
			last.x = x;
			last.y = y;
		} else {
			x += drawable->x;
			y += drawable->y;
		}

		if (RegionContainsPoint(clip, x, y, NULL)) {
			fill.blt(sna, &fill, x + dx, y + dy, 1, 1);
			if (damage) {
				BoxRec box;

				box.x1 = x + dx;
				box.y1 = y + dy;
				box.x2 = box.x1 + 1;
				box.y2 = box.y1 + 1;

				assert_pixmap_contains_box(pixmap, &box);
				sna_damage_add_box(damage, &box);
			}
		}
	}
	fill.done(sna, &fill);
	return TRUE;
}

static Bool
sna_poly_point_extents(DrawablePtr drawable, GCPtr gc,
		       int mode, int n, DDXPointPtr pt, BoxPtr out)
{
	BoxRec box;

	if (n == 0)
		return true;

	box.x2 = box.x1 = pt->x;
	box.y2 = box.y1 = pt->y;
	while (--n) {
		pt++;
		box_add_pt(&box, pt->x, pt->y);
	}
	box.x2++;
	box.y2++;

	trim_and_translate_box(&box, drawable, gc);
	*out = box;
	return box_empty(&box);
}

static void
sna_poly_point(DrawablePtr drawable, GCPtr gc,
	       int mode, int n, DDXPointPtr pt)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	BoxRec extents;
	RegionRec region;

	DBG(("%s(mode=%d, n=%d, pt[0]=(%d, %d)\n",
	     __FUNCTION__, mode, n, pt[0].x, pt[0].y));

	if (sna_poly_point_extents(drawable, gc, mode, n, pt, &extents))
		return;

	DBG(("%s: extents (%d, %d), (%d, %d)\n", __FUNCTION__,
	     extents.x1, extents.y1, extents.x2, extents.y2));

	if (sna->kgem.wedged) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (gc->fillStyle == FillSolid &&
	    PM_IS_SOLID(drawable, gc->planemask)) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);

		DBG(("%s: trying solid fill [%08lx] blt paths\n",
		     __FUNCTION__, gc->fgPixel));

		if (sna_drawable_use_gpu_bo(drawable, &extents) &&
		    sna_poly_point_blt(drawable,
				       priv->gpu_bo,
				       priv->gpu_only ? NULL : &priv->gpu_damage,
				       gc, mode, n, pt))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &extents) &&
		    sna_poly_point_blt(drawable,
				       priv->cpu_bo,
				       &priv->cpu_damage,
				       gc, mode, n, pt))
			return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	RegionInit(&region, &extents, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	fbPolyPoint(drawable, gc, mode, n, pt);
}

static Bool
sna_poly_line_can_blt(int mode, int n, DDXPointPtr pt)
{
	int i;

	if (mode == CoordModePrevious) {
		for (i = 1; i < n; i++) {
			if (pt[i].x != 0 && pt[i].y != 0)
				return FALSE;
		}
	} else {
		for (i = 1; i < n; i++) {
			if (pt[i].x != pt[i-1].x && pt[i].y != pt[i-1].y)
				return FALSE;
		}
	}

	return TRUE;
}

static Bool
sna_poly_line_blt(DrawablePtr drawable,
		  struct kgem_bo *bo,
		  struct sna_damage **damage,
		  GCPtr gc, int mode, int n, DDXPointPtr pt)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	RegionPtr clip = fbGetCompositeClip(gc);
	struct sna_fill_op fill;
	DDXPointRec last;
	int16_t dx, dy;
	int first;

	DBG(("%s: alu=%d, fg=%08lx\n", __FUNCTION__, gc->alu, gc->fgPixel));

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	last.x = drawable->x;
	last.y = drawable->y;
	first = 1;

	while (n--) {
		int nclip;
		BoxPtr box;
		int x, y;

		x = pt->x;
		y = pt->y;
		pt++;
		if (mode == CoordModePrevious) {
			x += last.x;
			y += last.x;
		} else {
			x += drawable->x;
			y += drawable->y;
		}

		if (!first) {
			for (nclip = REGION_NUM_RECTS(clip), box = REGION_RECTS(clip); nclip--; box++) {
				BoxRec r;

				if (last.x == x) {
					r.x1 = last.x;
					r.x2 = last.x + 1;
				} else {
					r.x1 = last.x < x ? last.x : x;
					r.x2 = last.x > x ? last.x : x;
				}
				if (last.y == y) {
					r.y1 = last.y;
					r.y2 = last.y + 1;
				} else {
					r.y1 = last.y < y ? last.y : y;
					r.y2 = last.y > y ? last.y : y;
				}
				DBG(("%s: (%d, %d) -> (%d, %d) clipping line (%d, %d), (%d, %d) against box (%d, %d), (%d, %d)\n",
				     __FUNCTION__,
				     last.x, last.y, x, y,
				     r.x1, r.y1, r.x2, r.y2,
				     box->x1, box->y1, box->x2, box->y2));
				if (box_intersect(&r, box)) {
					r.x1 += dx;
					r.x2 += dx;
					r.y1 += dy;
					r.y2 += dy;
					DBG(("%s: blt (%d, %d), (%d, %d)\n",
					     __FUNCTION__,
					     r.x1, r.y1, r.x2, r.y2));
					fill.blt(sna, &fill,
						 r.x1, r.y1,
						 r.x2-r.x1, r.y2-r.y1);
					if (damage) {
						assert_pixmap_contains_box(pixmap, &r);
						sna_damage_add_box(damage, &r);
					}
				}
			}
		}

		last.x = x;
		last.y = y;
		first = 0;
	}
	fill.done(sna, &fill);
	return TRUE;
}

static Bool
sna_poly_line_extents(DrawablePtr drawable, GCPtr gc,
		      int mode, int n, DDXPointPtr pt,
		      BoxPtr out)
{
	BoxRec box;
	int extra = gc->lineWidth >> 1;

	if (n == 0)
		return true;

	if (n > 1) {
		if (gc->joinStyle == JoinMiter)
			extra = 6 * gc->lineWidth;
		else if (gc->capStyle == CapProjecting)
			extra = gc->lineWidth;
	}

	box.x2 = box.x1 = pt->x;
	box.y2 = box.y1 = pt->y;
	if (mode == CoordModePrevious) {
		int x = box.x1;
		int y = box.y1;
		while (--n) {
			pt++;
			x += pt->x;
			y += pt->y;
			box_add_pt(&box, x, y);
		}
	} else {
		while (--n) {
			pt++;
			box_add_pt(&box, pt->x, pt->y);
		}
	}
	box.x2++;
	box.y2++;

	if (extra) {
		box.x1 -= extra;
		box.x2 += extra;
		box.y1 -= extra;
		box.y2 += extra;
	}

	trim_and_translate_box(&box, drawable, gc);
	*out = box;
	return box_empty(&box);
}

static void
sna_poly_line(DrawablePtr drawable, GCPtr gc,
	      int mode, int n, DDXPointPtr pt)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	BoxRec extents;
	RegionRec region;

	DBG(("%s(mode=%d, n=%d, pt[0]=(%d, %d)\n",
	     __FUNCTION__, mode, n, pt[0].x, pt[0].y));

	if (sna_poly_line_extents(drawable, gc, mode, n, pt, &extents))
		return;

	DBG(("%s: extents (%d, %d), (%d, %d)\n", __FUNCTION__,
	     extents.x1, extents.y1, extents.x2, extents.y2));

	if (sna->kgem.wedged) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (gc->fillStyle == FillSolid &&
	    gc->lineStyle == LineSolid &&
	    (gc->lineWidth == 0 || gc->lineWidth == 1) &&
	    PM_IS_SOLID(drawable, gc->planemask) &&
	    sna_poly_line_can_blt(mode, n, pt)) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);

		DBG(("%s: trying solid fill [%08lx]\n",
		     __FUNCTION__, gc->fgPixel));

		if (sna_drawable_use_gpu_bo(drawable, &extents) &&
		    sna_poly_line_blt(drawable,
				      priv->gpu_bo,
				      priv->gpu_only ? NULL : &priv->gpu_damage,
				      gc, mode, n, pt))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &extents) &&
		    sna_poly_line_blt(drawable,
				      priv->cpu_bo,
				      &priv->cpu_damage,
				      gc, mode, n, pt))
			return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	RegionInit(&region, &extents, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	fbPolyLine(drawable, gc, mode, n, pt);
}

static Bool
sna_poly_segment_can_blt(int n, xSegment *seg)
{
	while (n--) {
		if (seg->x1 != seg->x2 && seg->y1 != seg->y2)
			return FALSE;

		seg++;
	}

	return TRUE;
}

static Bool
sna_poly_segment_blt(DrawablePtr drawable,
		     struct kgem_bo *bo,
		     struct sna_damage **damage,
		     GCPtr gc, int n, xSegment *seg)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	RegionPtr clip = fbGetCompositeClip(gc);
	struct sna_fill_op fill;
	int16_t dx, dy;

	DBG(("%s: alu=%d, fg=%08lx\n", __FUNCTION__, gc->alu, gc->fgPixel));

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	while (n--) {
		int x, y, width, height, nclip;
		BoxPtr box;

		if (seg->x1 < seg->x2) {
			x = seg->x1;
			width = seg->x2;
		} else {
			x = seg->x2;
			width = seg->x1;
		}
		width -= x - 1;
		x += drawable->x;

		if (seg->y1 < seg->y2) {
			y = seg->y1;
			height = seg->y2;
		} else {
			y = seg->y2;
			height = seg->y1;
		}
		height -= y - 1;
		y += drawable->y;

		/* don't paint last pixel */
		if (gc->capStyle == CapNotLast) {
			if (width == 1)
				height--;
			else
				width--;
		}

		DBG(("%s: [%d] (%d, %d)x(%d, %d) + (%d, %d)\n", __FUNCTION__, n,
		     x, y, width, height, dx, dy));
		for (nclip = REGION_NUM_RECTS(clip), box = REGION_RECTS(clip); nclip--; box++) {
			BoxRec r = { x, y, x + width, y + height };
			if (box_intersect(&r, box)) {
				r.x1 += dx;
				r.x2 += dx;
				r.y1 += dy;
				r.y2 += dy;
				fill.blt(sna, &fill,
					 r.x1, r.y1,
					 r.x2-r.x1, r.y2-r.y1);
				if (damage) {
					assert_pixmap_contains_box(pixmap, &r);
					sna_damage_add_box(damage, &r);
				}
			}
		}

		seg++;
	}
	fill.done(sna, &fill);
	return TRUE;
}

static Bool
sna_poly_segment_extents(DrawablePtr drawable, GCPtr gc,
			 int n, xSegment *seg,
			 BoxPtr out)
{
	BoxRec box;
	int extra = gc->lineWidth;

	if (n == 0)
		return true;

	if (gc->capStyle != CapProjecting)
		extra >>= 1;

	if (seg->x2 > seg->x1) {
		box.x1 = seg->x1;
		box.x2 = seg->x2;
	} else {
		box.x2 = seg->x1;
		box.x1 = seg->x2;
	}

	if (seg->y2 > seg->y1) {
		box.y1 = seg->y1;
		box.y2 = seg->y2;
	} else {
		box.y2 = seg->y1;
		box.y1 = seg->y2;
	}

	while (--n) {
		seg++;
		if (seg->x2 > seg->x1) {
			if (seg->x1 < box.x1) box.x1 = seg->x1;
			if (seg->x2 > box.x2) box.x2 = seg->x2;
		} else {
			if (seg->x2 < box.x1) box.x1 = seg->x2;
			if (seg->x1 > box.x2) box.x2 = seg->x1;
		}

		if (seg->y2 > seg->y1) {
			if (seg->y1 < box.y1) box.y1 = seg->y1;
			if (seg->y2 > box.y2) box.y2 = seg->y2;
		} else {
			if (seg->y2 < box.y1) box.y1 = seg->y2;
			if (seg->y1 > box.y2) box.y2 = seg->y1;
		}
	}

	box.x2++;
	box.y2++;

	if (extra) {
		box.x1 -= extra;
		box.x2 += extra;
		box.y1 -= extra;
		box.y2 += extra;
	}

	trim_and_translate_box(&box, drawable, gc);
	*out = box;
	return box_empty(&box);
}

static void
sna_poly_segment(DrawablePtr drawable, GCPtr gc, int n, xSegment *seg)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	BoxRec extents;
	RegionRec region;

	DBG(("%s(n=%d, first=((%d, %d), (%d, %d))\n", __FUNCTION__,
	     n, seg->x1, seg->y1, seg->x2, seg->y2));

	if (sna_poly_segment_extents(drawable, gc, n, seg, &extents))
		return;

	DBG(("%s: extents=(%d, %d), (%d, %d)\n", __FUNCTION__,
	     extents.x1, extents.y1, extents.x2, extents.y2));

	if (sna->kgem.wedged) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (gc->fillStyle == FillSolid &&
	    gc->lineStyle == LineSolid &&
	    gc->lineWidth == 0 &&
	    PM_IS_SOLID(drawable, gc->planemask) &&
	    sna_poly_segment_can_blt(n, seg)) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);

		DBG(("%s: trying blt solid fill [%08lx] paths\n",
		     __FUNCTION__, gc->fgPixel));

		if (sna_drawable_use_gpu_bo(drawable, &extents) &&
		    sna_poly_segment_blt(drawable,
					 priv->gpu_bo,
					 priv->gpu_only ? NULL : &priv->gpu_damage,
					 gc, n, seg))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &extents) &&
		    sna_poly_segment_blt(drawable,
					 priv->cpu_bo,
					 &priv->cpu_damage,
					 gc, n, seg))
			return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	RegionInit(&region, &extents, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	fbPolySegment(drawable, gc, n, seg);
}

static Bool
sna_poly_arc_extents(DrawablePtr drawable, GCPtr gc,
		     int n, xArc *arc,
		     BoxPtr out)
{
	int extra = gc->lineWidth >> 1;
	BoxRec box;
	int v;

	if (n == 0)
		return true;

	box.x1 = arc->x;
	box.x2 = bound(box.x1, arc->width);
	box.y1 = arc->y;
	box.y2 = bound(box.y1, arc->height);

	while (--n) {
		arc++;
		if (box.x1 > arc->x)
			box.x1 = arc->x;
		v = bound(arc->x, arc->width);
		if (box.x2 < v)
			box.x2 = v;
		if (box.y1 > arc->y)
			box.y1 = arc->y;
		v = bound(arc->y, arc->height);
		if (box.y2 < v)
			box.y2 = v;
	}

	if (extra) {
		box.x1 -= extra;
		box.x2 += extra;
		box.y1 -= extra;
		box.y2 += extra;
	}

	box.x2++;
	box.y2++;

	trim_and_translate_box(&box, drawable, gc);
	*out = box;
	return box_empty(&box);
}

static void
sna_poly_arc(DrawablePtr drawable, GCPtr gc, int n, xArc *arc)
{
	BoxRec extents;
	RegionRec region;

	if (sna_poly_arc_extents(drawable, gc, n, arc, &extents))
		return;

	DBG(("%s: extents=(%d, %d), (%d, %d)\n", __FUNCTION__,
	     extents.x1, extents.y1, extents.x2, extents.y2));

	RegionInit(&region, &extents, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	fbPolyArc(drawable, gc, n, arc);
}

static Bool
sna_poly_fill_rect_blt(DrawablePtr drawable,
		       struct kgem_bo *bo,
		       struct sna_damage **damage,
		       GCPtr gc, int n,
		       xRectangle *rect)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	RegionPtr clip = fbGetCompositeClip(gc);
	struct sna_fill_op fill;
	uint32_t pixel = gc->fillStyle == FillSolid ? gc->fgPixel : gc->tile.pixel;
	int16_t dx, dy;

	DBG(("%s x %d [(%d, %d)+(%d, %d)...]\n",
	     __FUNCTION__, n, rect->x, rect->y, rect->width, rect->height));

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, pixel)) {
		DBG(("%s: unsupported blt\n", __FUNCTION__));
		return FALSE;
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	if (REGION_NUM_RECTS(clip) == 1) {
		BoxPtr box = REGION_RECTS(clip);
		while (n--) {
			BoxRec r;

			r.x1 = rect->x + drawable->x;
			r.y1 = rect->y + drawable->y;
			r.x2 = bound(r.x1, rect->width);
			r.y2 = bound(r.y1, rect->height);
			rect++;

			if (box_intersect(&r, box)) {
				r.x1 += dx;
				r.x2 += dx;
				r.y1 += dy;
				r.y2 += dy;
				fill.blt(sna, &fill,
					 r.x1, r.y1,
					 r.x2-r.x1, r.y2-r.y1);
				if (damage) {
					assert_pixmap_contains_box(pixmap, &r);
					sna_damage_add_box(damage, &r);
				}
			}
		}
	} else {
		while (n--) {
			RegionRec region;
			BoxRec r,*box;
			int nbox;

			r.x1 = rect->x + drawable->x;
			r.y1 = rect->y + drawable->y;
			r.x2 = bound(r.x1, rect->width);
			r.y2 = bound(r.y1, rect->height);
			rect++;

			RegionInit(&region, &r, 1);
			RegionIntersect(&region, &region, clip);

			nbox = REGION_NUM_RECTS(&region);
			box = REGION_RECTS(&region);
			while (nbox--) {
				box->x1 += dx;
				box->x2 += dx;
				box->y1 += dy;
				box->y2 += dy;
				fill.blt(sna, &fill,
					 box->x1, box->y1,
					 box->x2-box->x1, box->y2-box->y1);
				if (damage) {
					assert_pixmap_contains_box(pixmap, box);
					sna_damage_add_box(damage, box);
				}
				box++;
			}

			RegionUninit(&region);
		}
	}
	fill.done(sna, &fill);
	return TRUE;
}

static uint32_t
get_pixel(PixmapPtr pixmap)
{
	DBG(("%s\n", __FUNCTION__));
	sna_pixmap_move_to_cpu(pixmap, false);
	switch (pixmap->drawable.bitsPerPixel) {
	case 32: return *(uint32_t *)pixmap->devPrivate.ptr;
	case 16: return *(uint16_t *)pixmap->devPrivate.ptr;
	default: return *(uint8_t *)pixmap->devPrivate.ptr;
	}
}

static Bool
sna_poly_fill_rect_tiled(DrawablePtr drawable,
			 struct kgem_bo *bo,
			 struct sna_damage **damage,
			 GCPtr gc, int n,
			 xRectangle *rect)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	PixmapPtr tile = gc->tile.pixmap;
	RegionPtr clip = fbGetCompositeClip(gc);
	DDXPointPtr origin = &gc->patOrg;
	CARD32 alu = gc->alu;
	int tile_width, tile_height;
	int16_t dx, dy;

	DBG(("%s x %d [(%d, %d)+(%d, %d)...]\n",
	     __FUNCTION__, n, rect->x, rect->y, rect->width, rect->height));

	tile_width = tile->drawable.width;
	tile_height = tile->drawable.height;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	if (tile_width == 1 && tile_height == 1) {
		struct sna_fill_op fill;

		if (!sna_fill_init_blt(&fill, sna, pixmap, bo, alu, get_pixel(tile))) {
			DBG(("%s: unsupported blt\n", __FUNCTION__));
			return FALSE;
		}

		if (REGION_NUM_RECTS(clip) == 1) {
			BoxPtr box = REGION_RECTS(clip);
			while (n--) {
				BoxRec r;

				r.x1 = rect->x + drawable->x;
				r.y1 = rect->y + drawable->y;
				r.x2 = bound(r.x1, rect->width);
				r.y2 = bound(r.y1, rect->height);
				rect++;

				if (box_intersect(&r, box)) {
					r.x1 += dx;
					r.x2 += dx;
					r.y1 += dy;
					r.y2 += dy;
					fill.blt(sna, &fill,
						 r.x1, r.y1,
						 r.x2-r.x1, r.y2-r.y1);
					if (damage) {
						assert_pixmap_contains_box(pixmap, &r);
						sna_damage_add_box(damage, &r);
					}
				}
			}
		} else {
			while (n--) {
				RegionRec region;
				BoxRec r,*box;
				int nbox;

				r.x1 = rect->x + drawable->x;
				r.y1 = rect->y + drawable->y;
				r.x2 = bound(r.x1, rect->width);
				r.y2 = bound(r.y1, rect->height);
				rect++;

				RegionInit(&region, &r, 1);
				RegionIntersect(&region, &region, clip);

				nbox = REGION_NUM_RECTS(&region);
				box = REGION_RECTS(&region);
				while (nbox--) {
					box->x1 += dx;
					box->x2 += dx;
					box->y1 += dy;
					box->y2 += dy;
					fill.blt(sna, &fill,
						 box->x1, box->y1,
						 box->x2-box->x1,
						 box->y2-box->y1);
					if (damage) {
						assert_pixmap_contains_box(pixmap, box);
						sna_damage_add_box(damage, box);
					}
					box++;
				}

				RegionUninit(&region);
			}
		}
		fill.done(sna, &fill);
	} else {
		struct sna_copy_op copy;

		if (!sna_pixmap_move_to_gpu(tile))
			return FALSE;

		if (!sna_copy_init_blt(&copy, sna,
				       tile, sna_pixmap_get_bo(tile),
				       pixmap, bo,
				       alu)) {
			DBG(("%s: unsupported blt\n", __FUNCTION__));
			return FALSE;
		}

		if (REGION_NUM_RECTS(clip) == 1) {
			const BoxPtr box = REGION_RECTS(clip);
			while (n--) {
				BoxRec r;

				r.x1 = rect->x + drawable->x;
				r.y1 = rect->y + drawable->y;
				r.x2 = bound(r.x1, rect->width);
				r.y2 = bound(r.y1, rect->height);
				rect++;

				if (box_intersect(&r, box)) {
					int height = r.y2 - r.y1;
					int dst_y = r.y1;
					int tile_y = (r.y1 - drawable->y - origin->y) % tile_height;
					while (height) {
						int width = r.x2 - r.x1;
						int dst_x = r.x1;
						int tile_x = (r.x1 - drawable->x - origin->x) % tile_width;
						int h = tile_height - tile_y;
						if (h > height)
							h = height;
						height -= h;

						while (width > 0) {
							int w = tile_width - tile_x;
							if (w > width)
								w = width;
							width -= w;

							copy.blt(sna, &copy,
								 tile_x, tile_y,
								 w, h,
								 dst_x + dx, dst_y + dy);
							if (damage) {
								BoxRec box;
								box.x1 = dst_x + dx;
								box.y1 = dst_y + dy;
								box.x2 = box.x1 + w;
								box.y2 = box.y1 + h;
								assert_pixmap_contains_box(pixmap, &box);
								sna_damage_add_box(damage, &box);
							}

							dst_x += w;
							tile_x = 0;
						}
						dst_y += h;
						tile_y = 0;
					}
				}
			}
		} else {
			while (n--) {
				RegionRec region;
				BoxRec r,*box;
				int nbox;

				r.x1 = rect->x + drawable->x;
				r.y1 = rect->y + drawable->y;
				r.x2 = bound(r.x1, rect->width);
				r.y2 = bound(r.y1, rect->height);
				rect++;

				RegionInit(&region, &r, 1);
				RegionIntersect(&region, &region, clip);

				nbox = REGION_NUM_RECTS(&region);
				box = REGION_RECTS(&region);
				while (nbox--) {
					int height = box->y2 - box->y1;
					int dst_y = box->y1;
					int tile_y = (box->y1 - drawable->y - origin->y) % tile_height;
					while (height) {
						int width = box->x2 - box->x1;
						int dst_x = box->x1;
						int tile_x = (box->x1 - drawable->x - origin->x) % tile_width;
						int h = tile_height - tile_y;
						if (h > height)
							h = height;
						height -= h;

						while (width > 0) {
							int w = tile_width - tile_x;
							if (w > width)
								w = width;
							width -= w;

							copy.blt(sna, &copy,
								 tile_x, tile_y,
								 w, h,
								 dst_x + dx, dst_y + dy);
							if (damage) {
								BoxRec box;

								box.x1 = dst_x + dx;
								box.y1 = dst_y + dy;
								box.x2 = box.x1 + w;
								box.y2 = box.y1 + h;

								assert_pixmap_contains_box(pixmap, &box);
								sna_damage_add_box(damage, &box);
							}

							dst_x += w;
							tile_x = 0;
						}
						dst_y += h;
						tile_y = 0;
					}
					box++;
				}

				RegionUninit(&region);
			}
		}
		copy.done(sna, &copy);
	}
	return TRUE;
}

static Bool
sna_poly_fill_rect_extents(DrawablePtr drawable, GCPtr gc,
			   int n, xRectangle *rect,
			   BoxPtr out)
{
	BoxRec box;

	if (n == 0)
		return true;

	DBG(("%s: [0] = (%d, %d)x(%d, %d)\n",
	     __FUNCTION__, rect->x, rect->y, rect->width, rect->height));
	box.x1 = rect->x;
	box.x2 = bound(box.x1, rect->width);
	box.y1 = rect->y;
	box.y2 = bound(box.y1, rect->height);

	while (--n) {
		rect++;
		box_add_rect(&box, rect);
	}

	trim_and_translate_box(&box, drawable, gc);
	*out = box;
	return box_empty(&box);
}

static void
sna_poly_fill_rect(DrawablePtr draw, GCPtr gc, int n, xRectangle *rect)
{
	struct sna *sna = to_sna_from_drawable(draw);
	BoxRec extents;
	RegionRec region;

	DBG(("%s(n=%d, PlaneMask: %lx (solid %d), solid fill: %d [style=%d, tileIsPixel=%d], alu=%d)\n", __FUNCTION__,
	     n, gc->planemask, !!PM_IS_SOLID(draw, gc->planemask),
	     (gc->fillStyle == FillSolid ||
	      (gc->fillStyle == FillTiled && gc->tileIsPixel)),
	     gc->fillStyle, gc->tileIsPixel,
	     gc->alu));

	if (sna_poly_fill_rect_extents(draw, gc, n, rect, &extents)) {
		DBG(("%s, nothing to do\n", __FUNCTION__));
		return;
	}

	if (sna->kgem.wedged) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (!PM_IS_SOLID(draw, gc->planemask))
		goto fallback;

	if (gc->fillStyle == FillSolid ||
	    (gc->fillStyle == FillTiled && gc->tileIsPixel)) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(draw);

		DBG(("%s: solid fill [%08lx], testing for blt\n",
		     __FUNCTION__,
		     gc->fillStyle == FillSolid ? gc->fgPixel : gc->tile.pixel));

		if (sna_drawable_use_gpu_bo(draw, &extents) &&
		    sna_poly_fill_rect_blt(draw,
					   priv->gpu_bo,
					   priv->gpu_only ? NULL : &priv->gpu_damage,
					   gc, n, rect))
			return;

		if (sna_drawable_use_cpu_bo(draw, &extents) &&
		    sna_poly_fill_rect_blt(draw,
					   priv->cpu_bo,
					   &priv->cpu_damage,
					   gc, n, rect))
			return;
	} else if (gc->fillStyle == FillTiled) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(draw);

		DBG(("%s: tiled fill, testing for blt\n", __FUNCTION__));

		if (sna_drawable_use_gpu_bo(draw, &extents) &&
		    sna_poly_fill_rect_tiled(draw,
					     priv->gpu_bo,
					     priv->gpu_only ? NULL : &priv->gpu_damage,
					     gc, n, rect))
			return;

		if (sna_drawable_use_cpu_bo(draw, &extents) &&
		    sna_poly_fill_rect_tiled(draw,
					     priv->cpu_bo,
					     &priv->cpu_damage,
					     gc, n, rect))
			return;
	}

fallback:
	DBG(("%s: fallback (%d, %d), (%d, %d)\n", __FUNCTION__,
	     extents.x1, extents.y1, extents.x2, extents.y2));
	RegionInit(&region, &extents, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region)) {
		DBG(("%s: nothing to do, all clipped\n", __FUNCTION__));
		return;
	}

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(draw, &region, true);
	RegionUninit(&region);

	DBG(("%s: fallback - fbPolyFillRect\n", __FUNCTION__));
	fbPolyFillRect(draw, gc, n, rect);
}

static void
sna_image_glyph(DrawablePtr drawable, GCPtr gc,
		int x, int y, unsigned int n,
		CharInfoPtr *info, pointer base)
{
	ExtentInfoRec extents;
	BoxRec box;
	RegionRec region;

	if (n == 0)
		return;

	QueryGlyphExtents(gc->font, info, n, &extents);
	if (extents.overallWidth >= 0) {
		box.x1 = x;
		box.x2 = x + extents.overallWidth;
	} else {
		box.x2 = x;
		box.x1 = x + extents.overallWidth;
	}
	box.y1 = y - FONTASCENT(gc->font);
	box.y2 = y + FONTDESCENT(gc->font);
	trim_box(&box, drawable);
	if (box_empty(&box))
		return;
	translate_box(&box, drawable);

	DBG(("%s: extents(%d, %d), (%d, %d)\n",
	     __FUNCTION__, box.x1, box.y1, box.x2, box.y2));

	RegionInit(&region, &box, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	fbImageGlyphBlt(drawable, gc, x, y, n, info, base);
}

static void
sna_poly_glyph(DrawablePtr drawable, GCPtr gc,
	       int x, int y, unsigned int n,
	       CharInfoPtr *info, pointer base)
{
	ExtentInfoRec extents;
	BoxRec box;
	RegionRec region;

	if (n == 0)
		return;

	QueryGlyphExtents(gc->font, info, n, &extents);
	box.x1 = x + extents.overallLeft;
	box.y1 = y - extents.overallAscent;
	box.x2 = x + extents.overallRight;
	box.y2 = y + extents.overallDescent;

	trim_box(&box, drawable);
	if (box_empty(&box))
		return;
	translate_box(&box, drawable);

	DBG(("%s: extents(%d, %d), (%d, %d)\n",
	     __FUNCTION__, box.x1, box.y1, box.x2, box.y2));

	RegionInit(&region, &box, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	fbPolyGlyphBlt(drawable, gc, x, y, n, info, base);
}

static void
sna_push_pixels(GCPtr gc, PixmapPtr bitmap, DrawablePtr drawable,
		int w, int h,
		int x, int y)
{
	BoxRec box;
	RegionRec region;

	if (w == 0 || h == 0)
		return;

	DBG(("%s (%d, %d)x(%d, %d)\n", __FUNCTION__, x, y, w, h));

	box.x1 = x;
	box.y1 = y;
	if (!gc->miTranslate) {
		box.x1 += drawable->x;
		box.y1 += drawable->y;
	}
	box.x2 = box.x1 + w;
	box.y2 = box.y1 + h;

	clip_box(&box, gc);
	if (box_empty(&box))
		return;

	DBG(("%s: extents(%d, %d), (%d, %d)\n",
	     __FUNCTION__, box.x1, box.y1, box.x2, box.y2));

	RegionInit(&region, &box, 1);
	if (gc->pCompositeClip)
		RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_pixmap_move_to_cpu(bitmap, false);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	fbPushPixels(gc, bitmap, drawable, w, h, x, y);
}

static const GCOps sna_gc_ops = {
	sna_fill_spans,
	sna_set_spans,
	sna_put_image,
	sna_copy_area,
	sna_copy_plane,
	sna_poly_point,
	sna_poly_line,
	sna_poly_segment,
	miPolyRectangle,
	sna_poly_arc,
	miFillPolygon,
	sna_poly_fill_rect,
	miPolyFillArc,
	miPolyText8,
	miPolyText16,
	miImageText8,
	miImageText16,
	sna_image_glyph,
	sna_poly_glyph,
	sna_push_pixels,
};

static void sna_validate_pixmap(DrawablePtr draw, PixmapPtr pixmap)
{
	if (draw->bitsPerPixel == pixmap->drawable.bitsPerPixel &&
	    FbEvenTile(pixmap->drawable.width *
		       pixmap->drawable.bitsPerPixel)) {
		DBG(("%s: flushing pixmap\n", __FUNCTION__));
		sna_pixmap_move_to_cpu(pixmap, true);
	}
}

static void
sna_validate_gc(GCPtr gc, unsigned long changes, DrawablePtr drawable)
{
	DBG(("%s\n", __FUNCTION__));

	if (changes & GCTile && !gc->tileIsPixel) {
		DBG(("%s: flushing tile pixmap\n", __FUNCTION__));
		sna_validate_pixmap(drawable, gc->tile.pixmap);
	}

	if (changes & GCStipple && gc->stipple) {
		DBG(("%s: flushing stipple pixmap\n", __FUNCTION__));
		sna_pixmap_move_to_cpu(gc->stipple, true);
	}

	fbValidateGC(gc, changes, drawable);
}

static const GCFuncs sna_gc_funcs = {
	sna_validate_gc,
	miChangeGC,
	miCopyGC,
	miDestroyGC,
	miChangeClip,
	miDestroyClip,
	miCopyClip
};

static int sna_create_gc(GCPtr gc)
{
	if (!fbCreateGC(gc))
		return FALSE;

	gc->funcs = (GCFuncs *)&sna_gc_funcs;
	gc->ops = (GCOps *)&sna_gc_ops;
	return TRUE;
}

static void
sna_get_image(DrawablePtr drawable,
	      int x, int y, int w, int h,
	      unsigned int format, unsigned long mask,
	      char *dst)
{
	BoxRec extents;
	RegionRec region;

	DBG(("%s (%d, %d, %d, %d)\n", __FUNCTION__, x, y, w, h));

	extents.x1 = x + drawable->x;
	extents.y1 = y + drawable->y;
	extents.x2 = extents.x1 + w;
	extents.y2 = extents.y1 + h;
	RegionInit(&region, &extents, 1);

	sna_drawable_move_region_to_cpu(drawable, &region, false);
	fbGetImage(drawable, x, y, w, h, format, mask, dst);

	RegionUninit(&region);
}

static void
sna_get_spans(DrawablePtr drawable, int wMax,
	      DDXPointPtr pt, int *width, int n, char *start)
{
	BoxRec extents;
	RegionRec region;

	if (sna_spans_extents(drawable, NULL, n, pt, width, &extents))
		return;

	RegionInit(&region, &extents, 1);
	sna_drawable_move_region_to_cpu(drawable, &region, false);
	RegionUninit(&region);

	fbGetSpans(drawable, wMax, pt, width, n, start);
}

static void
sna_copy_window(WindowPtr win, DDXPointRec origin, RegionPtr src)
{
	struct sna *sna = to_sna_from_drawable(&win->drawable);
	PixmapPtr pixmap = fbGetWindowPixmap(win);
	RegionRec dst;
	int dx, dy;

	DBG(("%s origin=(%d, %d)\n", __FUNCTION__, origin.x, origin.y));

	if (sna->kgem.wedged) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		sna_pixmap_move_to_cpu(pixmap, true);
		fbCopyWindow(win, origin, src);
		return;
	}

	dx = origin.x - win->drawable.x;
	dy = origin.y - win->drawable.y;
	RegionTranslate(src, -dx, -dy);

	RegionNull(&dst);
	RegionIntersect(&dst, &win->borderClip, src);
#ifdef COMPOSITE
	if (pixmap->screen_x || pixmap->screen_y)
		RegionTranslate(&dst, -pixmap->screen_x, -pixmap->screen_y);
#endif

	miCopyRegion(&pixmap->drawable, &pixmap->drawable,
		     NULL, &dst, dx, dy, sna_copy_boxes, 0, NULL);

	RegionUninit(&dst);
}

static Bool sna_change_window_attributes(WindowPtr win, unsigned long mask)
{
	DBG(("%s\n", __FUNCTION__));

	/* Check if the fb layer wishes to modify the attached pixmaps,
	 * to fix up mismatches between the window and pixmap depths.
	 */
	if (mask & CWBackPixmap && win->backgroundState == BackgroundPixmap) {
		DBG(("%s: flushing background pixmap\n", __FUNCTION__));
		sna_validate_pixmap(&win->drawable, win->background.pixmap);
	}

	if (mask & CWBorderPixmap && win->borderIsPixel == FALSE) {
		DBG(("%s: flushing border pixmap\n", __FUNCTION__));
		sna_validate_pixmap(&win->drawable, win->border.pixmap);
	}

	return fbChangeWindowAttributes(win, mask);
}

static void
sna_add_traps(PicturePtr picture, INT16 x, INT16 y, int n, xTrap *t)
{
	DBG(("%s (%d, %d) x %d\n", __FUNCTION__, x, y, n));

	sna_drawable_move_to_cpu(picture->pDrawable, true);

	fbAddTraps(picture, x, y, n, t);
}

static void
sna_accel_flush_callback(CallbackListPtr *list,
			 pointer user_data, pointer call_data)
{
	struct sna *sna = user_data;

	if (sna->kgem.flush == 0 && list_is_empty(&sna->dirty_pixmaps))
		return;

	DBG(("%s\n", __FUNCTION__));

	/* flush any pending damage from shadow copies to tfp clients */
	while (!list_is_empty(&sna->dirty_pixmaps)) {
		struct sna_pixmap *priv = list_first_entry(&sna->dirty_pixmaps,
							   struct sna_pixmap,
							   list);
		sna_pixmap_move_to_gpu(priv->pixmap);
	}

	kgem_submit(&sna->kgem);
}

static void sna_deferred_free(struct sna *sna)
{
	struct sna_pixmap *priv, *next;

	list_for_each_entry_safe(priv, next, &sna->deferred_free, list) {
		if (priv->cpu_bo->gpu)
			continue;

		list_del(&priv->list);
		kgem_bo_destroy(&sna->kgem, priv->cpu_bo);
		fbDestroyPixmap(priv->pixmap);
		free(priv);
	}
}

static uint64_t read_timer(int fd)
{
	uint64_t count = 0;
	int ret = read(fd, &count, sizeof(count));
	return count;
	(void)ret;
}

static struct sna_pixmap *sna_accel_scanout(struct sna *sna)
{
	PixmapPtr front = sna->shadow ? sna->shadow : sna->front;
	struct sna_pixmap *priv = sna_pixmap(front);
	return priv && priv->gpu_bo ? priv : NULL;
}

#if HAVE_SYS_TIMERFD_H
#include <sys/timerfd.h>
#include <errno.h>

static void _sna_accel_disarm_timer(struct sna *sna, int id)
{
	struct itimerspec to;

	DBG(("%s[%d] (time=%ld)\n", __FUNCTION__, id, (long)GetTimeInMillis()));

	memset(&to, 0, sizeof(to));
	timerfd_settime(sna->timer[id], 0, &to, NULL);
	sna->timer_active &= ~(1<<id);
}

#define return_if_timer_active(id) do {					\
	if (sna->timer_active & (1<<(id)))				\
		return read_timer(sna->timer[id]) > 0;			\
} while (0)

static Bool sna_accel_do_flush(struct sna *sna)
{
	struct itimerspec to;
	struct sna_pixmap *priv;

	return_if_timer_active(FLUSH_TIMER);

	priv = sna_accel_scanout(sna);
	if (priv == NULL)
		return FALSE;

	if (priv->cpu_damage == NULL && priv->gpu_bo->rq == NULL)
		return FALSE;

	if (sna->timer[FLUSH_TIMER] == -1)
		return TRUE;

	DBG(("%s, time=%ld\n", __FUNCTION__, (long)GetTimeInMillis()));

	/* Initial redraw after 10ms. */
	to.it_value.tv_sec = 0;
	to.it_value.tv_nsec = 10 * 1000 * 1000;

	/* Then periodic updates at 25Hz.*/
	to.it_interval.tv_sec = 0;
	to.it_interval.tv_nsec = 40 * 1000 * 1000;
	timerfd_settime(sna->timer[FLUSH_TIMER], 0, &to, NULL);

	sna->timer_active |= 1 << FLUSH_TIMER;
	return FALSE;
}

static Bool sna_accel_do_expire(struct sna *sna)
{
	struct itimerspec to;

	return_if_timer_active(EXPIRE_TIMER);

	if (!sna->kgem.need_expire)
		return FALSE;

	if (sna->timer[EXPIRE_TIMER] == -1)
		return TRUE;

	/* Initial expiration after 5s. */
	to.it_value.tv_sec = 5;
	to.it_value.tv_nsec = 0;

	/* Then periodic update every 10s.*/
	to.it_interval.tv_sec = 10;
	to.it_interval.tv_nsec = 0;
	timerfd_settime(sna->timer[EXPIRE_TIMER], 0, &to, NULL);

	sna->timer_active |= 1 << EXPIRE_TIMER;
	return FALSE;
}

static void sna_accel_create_timers(struct sna *sna)
{
	int id;

	for (id = 0; id < NUM_TIMERS; id++)
		sna->timer[id] = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
}
#else
static void sna_accel_create_timers(struct sna *sna)
{
	int id;

	for (id = 0; id < NUM_TIMERS; id++)
		sna->timer[id] = -1;
}
static Bool sna_accel_do_flush(struct sna *sna) { return sna_accel_scanout(sna) != NULL; }
static Bool sna_accel_arm_expire(struct sna *sna) { return TRUE; }
static void _sna_accel_disarm_timer(struct sna *sna, int id) { }
#endif

static void sna_accel_flush(struct sna *sna)
{
	struct sna_pixmap *priv = sna_accel_scanout(sna);

	DBG(("%s (time=%ld)\n", __FUNCTION__, (long)GetTimeInMillis()));

	sna_pixmap_move_to_gpu(priv->pixmap);
	kgem_bo_flush(&sna->kgem, priv->gpu_bo);

	if (priv->gpu_bo->rq == NULL)
		_sna_accel_disarm_timer(sna, FLUSH_TIMER);
}

static void sna_accel_expire(struct sna *sna)
{
	DBG(("%s (time=%ld)\n", __FUNCTION__, (long)GetTimeInMillis()));

	if (!kgem_expire_cache(&sna->kgem))
		_sna_accel_disarm_timer(sna, EXPIRE_TIMER);
}

static void sna_accel_install_timers(struct sna *sna)
{
	if (sna->timer[FLUSH_TIMER] != -1)
		AddGeneralSocket(sna->timer[FLUSH_TIMER]);

	if (sna->timer[EXPIRE_TIMER] != -1)
		AddGeneralSocket(sna->timer[EXPIRE_TIMER]);
}

Bool sna_accel_pre_init(struct sna *sna)
{
	sna_accel_create_timers(sna);
	return TRUE;
}

Bool sna_accel_init(ScreenPtr screen, struct sna *sna)
{
	const char *backend;

	if (!dixRegisterPrivateKey(&sna_pixmap_index, PRIVATE_PIXMAP, 0))
		return FALSE;

	if (!AddCallback(&FlushCallback, sna_accel_flush_callback, sna))
		return FALSE;

	if (!sna_glyphs_init(screen))
		return FALSE;

	list_init(&sna->dirty_pixmaps);
	list_init(&sna->deferred_free);

	AddGeneralSocket(sna->kgem.fd);
	sna_accel_install_timers(sna);

	screen->CreateGC = sna_create_gc;
	screen->GetImage = sna_get_image;
	screen->GetSpans = sna_get_spans;
	screen->CopyWindow = sna_copy_window;
	screen->ChangeWindowAttributes = sna_change_window_attributes;
	screen->CreatePixmap = sna_create_pixmap;
	screen->DestroyPixmap = sna_destroy_pixmap;

#ifdef RENDER
	{
		PictureScreenPtr ps = GetPictureScreenIfSet(screen);
		if (ps) {
			ps->Composite = sna_composite;
			ps->CompositeRects = sna_composite_rectangles;
			ps->Glyphs = sna_glyphs;
			ps->UnrealizeGlyph = sna_glyph_unrealize;
			ps->AddTraps = sna_add_traps;
			ps->Trapezoids = sna_composite_trapezoids;
		}
	}
#endif

	backend = "no";
	sna->have_render = false;
	no_render_init(sna);

#if !DEBUG_NO_RENDER
	if (sna->chipset.info->gen >= 80) {
	} else if (sna->chipset.info->gen >= 70) {
		if ((sna->have_render = gen7_render_init(sna)))
			backend = "IvyBridge";
	} else if (sna->chipset.info->gen >= 60) {
		if ((sna->have_render = gen6_render_init(sna)))
			backend = "SandyBridge";
	} else if (sna->chipset.info->gen >= 50) {
		if ((sna->have_render = gen5_render_init(sna)))
			backend = "Ironlake";
	} else if (sna->chipset.info->gen >= 40) {
		if ((sna->have_render = gen4_render_init(sna)))
			backend = "Broadwater";
	} else if (sna->chipset.info->gen >= 30) {
		if ((sna->have_render = gen3_render_init(sna)))
			backend = "gen3";
	} else if (sna->chipset.info->gen >= 20) {
		if ((sna->have_render = gen2_render_init(sna)))
			backend = "gen2";
	}
#endif
	DBG(("%s(backend=%s, have_render=%d)\n",
	     __FUNCTION__, backend, sna->have_render));

	kgem_reset(&sna->kgem);

	xf86DrvMsg(sna->scrn->scrnIndex, X_INFO,
		   "SNA initialized with %s backend\n",
		   backend);

	return TRUE;
}

Bool sna_accel_create(struct sna *sna)
{
	if (!sna_glyphs_create(sna))
		return FALSE;

	if (!sna_gradients_create(sna))
		return FALSE;

	return TRUE;
}

void sna_accel_close(struct sna *sna)
{
	sna_glyphs_close(sna);
	sna_gradients_close(sna);

	DeleteCallback(&FlushCallback, sna_accel_flush_callback, sna);
}

static void sna_accel_throttle(struct sna *sna)
{
	if (sna->flags & SNA_NO_THROTTLE)
		return;

	if (list_is_empty(&sna->kgem.requests))
		return;

	DBG(("%s (time=%ld)\n", __FUNCTION__, (long)GetTimeInMillis()));

	kgem_throttle(&sna->kgem);
}

void sna_accel_block_handler(struct sna *sna)
{
	if (sna_accel_do_flush(sna))
		sna_accel_flush(sna);

	if (sna_accel_do_expire(sna))
		sna_accel_expire(sna);

	sna_accel_throttle(sna);
}

void sna_accel_wakeup_handler(struct sna *sna)
{
	kgem_retire(&sna->kgem);
	sna_deferred_free(sna);

	if (sna->kgem.need_purge)
		kgem_expire_cache(&sna->kgem);
}

void sna_accel_free(struct sna *sna)
{
	int id;

	for (id = 0; id < NUM_TIMERS; id++)
		if (sna->timer[id] != -1) {
			close(sna->timer[id]);
			sna->timer[id] = -1;
		}
}
