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
#include "sna_reg.h"

#include <X11/fonts/font.h>
#include <X11/fonts/fontstruct.h>

#include <xaarop.h>
#include <fb.h>
#include <dixfontstr.h>

#ifdef RENDER
#include <mipict.h>
#include <fbpict.h>
#endif
#include <miline.h>

#include <sys/time.h>
#include <sys/mman.h>
#include <unistd.h>

#if DEBUG_ACCEL
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

#define FORCE_GPU_ONLY 0
#define FORCE_FALLBACK 0
#define FORCE_FLUSH 0

#define USE_SPANS 0
#define USE_ZERO_SPANS 1

DevPrivateKeyRec sna_pixmap_index;
DevPrivateKey sna_window_key;

static inline void region_set(RegionRec *r, const BoxRec *b)
{
	r->extents = *b;
	r->data = NULL;
}

static inline void region_maybe_clip(RegionRec *r, RegionRec *clip)
{
	if (clip && clip->data)
		RegionIntersect(r, r, clip);
}

typedef struct box32 {
	int32_t x1, y1, x2, y2;
} Box32Rec;

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
		    kgem_bo_is_busy(priv->cpu_bo)) {
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

	priv = _sna_pixmap_attach(pixmap);
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

	priv->gpu_only = 1;

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

	if (usage == CREATE_PIXMAP_USAGE_SCRATCH)
		return fbCreatePixmap(screen, width, height, depth, usage);

	if (usage == SNA_CREATE_SCRATCH)
		return sna_pixmap_create_scratch(screen,
						 width, height, depth,
						 I915_TILING_Y);

	if (FORCE_GPU_ONLY && width && height)
		return sna_pixmap_create_scratch(screen,
						 width, height, depth,
						 I915_TILING_X);

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

	priv->gpu = false;
}

static Bool
region_subsumes_drawable(RegionPtr region, DrawablePtr drawable)
{
	const BoxRec *extents;

	if (region->data)
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
		DBG(("%s: region (%dx%d) intersects gpu damage\n",
		     __FUNCTION__,
		     region->extents.x2 - region->extents.x1,
		     region->extents.y2 - region->extents.y1));

		if (!write &&
		    region->extents.x2 - region->extents.x1 == 1 &&
		    region->extents.y2 - region->extents.y1 == 1) {
			/*  Often associated with synchronisation, KISS */
			sna_read_boxes(sna,
				       priv->gpu_bo, 0, 0,
				       pixmap, 0, 0,
				       &region->extents, 1);
		} else {
			RegionRec want, need, *r;

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

				sna_damage_subtract(&priv->gpu_damage, r);
				RegionUninit(&need);
			}
			if (r == &want)
				pixman_region_fini(&want);
		}
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

	priv->gpu = false;
}

static void
sna_pixmap_move_area_to_gpu(PixmapPtr pixmap, BoxPtr box)
{
	struct sna *sna = to_sna_from_drawable(&pixmap->drawable);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	RegionRec i, r;

	DBG(("%s()\n", __FUNCTION__));

	assert(priv->gpu);
	assert(priv->gpu_bo);

	sna_damage_reduce(&priv->cpu_damage);
	DBG(("%s: CPU damage? %d\n", __FUNCTION__, priv->cpu_damage != NULL));

	if (priv->cpu_damage == NULL)
		goto done;

	region_set(&r, box);
	if (sna_damage_intersect(priv->cpu_damage, &r, &i)) {
		BoxPtr box = REGION_RECTS(&i);
		int n = REGION_NUM_RECTS(&i);
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

		sna_damage_subtract(&priv->cpu_damage, &r);
		RegionUninit(&i);
	}

done:
	if (priv->cpu_damage == NULL)
		list_del(&priv->list);
}

static inline Bool
_sna_drawable_use_gpu_bo(DrawablePtr drawable, const BoxRec *box)
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

	if (sna_damage_contains_box(priv->cpu_damage,
				    &extents) == PIXMAN_REGION_OUT)
		return TRUE;

	if (!priv->gpu || priv->gpu_damage == NULL)
		return FALSE;

	if (sna_damage_contains_box(priv->gpu_damage,
				    &extents) == PIXMAN_REGION_OUT)
		return FALSE;

	sna_pixmap_move_area_to_gpu(pixmap, &extents);
	return TRUE;
}

static inline Bool
sna_drawable_use_gpu_bo(DrawablePtr drawable, const BoxRec *box)
{
	Bool ret = _sna_drawable_use_gpu_bo(drawable, box);
	DBG(("%s((%d, %d), (%d, %d)) = %d\n", __FUNCTION__,
	     box->x1, box->y1, box->x2, box->y2, ret));
	return ret;
}

static inline Bool
_sna_drawable_use_cpu_bo(DrawablePtr drawable, const BoxRec *box)
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
sna_drawable_use_cpu_bo(DrawablePtr drawable, const BoxRec *box)
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

	priv->gpu_bo = kgem_create_buffer(&sna->kgem,
					  pad*height, KGEM_BUFFER_WRITE,
					  &ptr);
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
		if (!wedged(sna))
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
	priv->gpu = true;
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

static inline bool trim_box(BoxPtr box, DrawablePtr d)
{
	bool clipped = false;

	if (box->x1 < 0)
		box->x1 = 0, clipped = true;
	if (box->x2 > d->width)
		box->x2 = d->width, clipped = true;

	if (box->y1 < 0)
		box->y1 = 0, clipped = true;
	if (box->y2 > d->height)
		box->y2 = d->height, clipped = true;

	return clipped;
}

static inline bool clip_box(BoxPtr box, GCPtr gc)
{
	const BoxRec *clip;
	bool clipped;

	if (!gc->pCompositeClip)
		return false;

	clip = &gc->pCompositeClip->extents;

	clipped = gc->pCompositeClip->data != NULL;
	if (box->x1 < clip->x1)
		box->x1 = clip->x1, clipped = true;
	if (box->x2 > clip->x2)
		box->x2 = clip->x2, clipped = true;

	if (box->y1 < clip->y1)
		box->y1 = clip->y1, clipped = true;
	if (box->y2 > clip->y2)
		box->y2 = clip->y2, clipped = true;

	return clipped;
}

static inline void translate_box(BoxPtr box, DrawablePtr d)
{
	box->x1 += d->x;
	box->x2 += d->x;

	box->y1 += d->y;
	box->y2 += d->y;
}

static inline bool trim_and_translate_box(BoxPtr box, DrawablePtr d, GCPtr gc)
{
	bool clipped = trim_box(box, d);
	translate_box(box, d);
	clipped |= clip_box(box, gc);
	return clipped;
}

static inline bool box32_trim(Box32Rec *box, DrawablePtr d)
{
	bool clipped = false;

	if (box->x1 < 0)
		box->x1 = 0, clipped = true;
	if (box->x2 > d->width)
		box->x2 = d->width, clipped = true;

	if (box->y1 < 0)
		box->y1 = 0, clipped = true;
	if (box->y2 > d->height)
		box->y2 = d->height, clipped = true;

	return clipped;
}

static inline bool box32_clip(Box32Rec *box, GCPtr gc)
{
	bool clipped = gc->pCompositeClip->data != NULL;
	const BoxRec *clip = &gc->pCompositeClip->extents;

	if (box->x1 < clip->x1)
		box->x1 = clip->x1, clipped = true;
	if (box->x2 > clip->x2)
		box->x2 = clip->x2, clipped = true;

	if (box->y1 < clip->y1)
		box->y1 = clip->y1, clipped = true;
	if (box->y2 > clip->y2)
		box->y2 = clip->y2, clipped = true;

	return clipped;
}

static inline void box32_translate(Box32Rec *box, DrawablePtr d)
{
	box->x1 += d->x;
	box->x2 += d->x;

	box->y1 += d->y;
	box->y2 += d->y;
}

static inline bool box32_trim_and_translate(Box32Rec *box, DrawablePtr d, GCPtr gc)
{
	bool clipped;

	if (likely (gc->pCompositeClip)) {
		box32_translate(box, d);
		clipped = box32_clip(box, gc);
	} else {
		clipped = box32_trim(box, d);
		box32_translate(box, d);
	}

	return clipped;
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

static inline bool box32_to_box16(const Box32Rec *b32, BoxRec *b16)
{
	b16->x1 = b32->x1;
	b16->y1 = b32->y1;
	b16->x2 = b32->x2;
	b16->y2 = b32->y2;

	return b16->x2 > b16->x1 && b16->y2 > b16->y1;
}

static inline void box32_add_rect(Box32Rec *box, const xRectangle *r)
{
	int32_t v;

	v = r->x;
	if (box->x1 > v)
		box->x1 = v;
	v += r->width;
	if (box->x2 < v)
		box->x2 = v;

	v = r->y;
	if (box->y1 > v)
		box->y1 = v;
	v += r->height;
	if (box->y2 < v)
		box->y2 = v;
}

static inline bool box_empty(const BoxRec *box)
{
	return box->x2 <= box->x1 || box->y2 <= box->y1;
}

static Bool
sna_put_image_upload_blt(DrawablePtr drawable, GCPtr gc, RegionPtr region,
			 int x, int y, int w, int h, char *bits, int stride)
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
	char *dst_bits;
	int dst_stride;
	BoxRec *box;
	int16_t dx, dy;
	int n;

	if (!priv->gpu_bo)
		return false;

	if (priv->gpu_only)
		return sna_put_image_upload_blt(drawable, gc, region,
						x, y, w, h, bits, stride);

	if (gc->alu != GXcopy)
		return false;

	/* XXX performing the upload inplace is currently about 20x slower
	 * for putimage10 on gen6 -- mostly due to slow page faulting in kernel.
	 */
#if 0
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
#endif

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
	x += dx + drawable->x;
	y += dy + drawable->y;

	DBG(("%s: upload(%d, %d, %d, %d)\n", __FUNCTION__, x, y, w, h));

	dst_stride = pixmap->devKind;
	dst_bits = pixmap->devPrivate.ptr;

	/* Region is pre-clipped and translated into pixmap space */
	box = REGION_RECTS(region);
	n = REGION_NUM_RECTS(region);
	do {
		assert(box->x1 >= 0);
		assert(box->y1 >= 0);
		assert(box->x2 <= pixmap->drawable.width);
		assert(box->y2 <= pixmap->drawable.height);

		assert(box->x1 - x >= 0);
		assert(box->y1 - y >= 0);
		assert(box->x2 - x <= w);
		assert(box->y2 - y <= h);

		memcpy_blt(bits, dst_bits,
			   pixmap->drawable.bitsPerPixel,
			   stride, dst_stride,
			   box->x1 - x, box->y1 - y,
			   box->x1, box->y1,
			   box->x2 - box->x1, box->y2 - box->y1);
		box++;
	} while (--n);

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
	int16_t dx, dy;

	DBG(("%s((%d, %d)x(%d, %d), depth=%d, format=%d)\n",
	     __FUNCTION__, x, y, w, h, depth, format));

	if (w == 0 || h == 0)
		return;

	if (priv == NULL) {
		DBG(("%s: fbPutImage, unattached(%d, %d, %d, %d)\n",
		     __FUNCTION__, x, y, w, h));
		fbPutImage(drawable, gc, depth, x, y, w, h, left, format, bits);
		return;
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	region.extents.x1 = x + drawable->x + dx;
	region.extents.y1 = y + drawable->y + dy;
	region.extents.x2 = region.extents.x1 + w;
	region.extents.y2 = region.extents.y1 + h;

	trim_box(&region.extents, &pixmap->drawable);
	if (box_empty(&region.extents))
		return;

	region.data = NULL;
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

static bool
move_to_gpu(PixmapPtr pixmap, struct sna_pixmap *priv,
	    const BoxRec *box, uint8_t alu)
{
	int w = box->x2 - box->x1;
	int h = box->y2 - box->y1;

	if (priv->gpu_bo)
		return TRUE;

	if (alu != GXcopy)
		return TRUE;

	return ++priv->source_count * w*h >= 2 * pixmap->drawable.width * pixmap->drawable.height;
}

static void
sna_self_copy_boxes(DrawablePtr src, DrawablePtr dst, GCPtr gc,
		    BoxPtr box, int n,
		    int dx, int dy,
		    Bool reverse, Bool upsidedown, Pixel bitplane,
		    void *closure)
{
	struct sna *sna = to_sna_from_drawable(src);
	PixmapPtr pixmap = get_drawable_pixmap(src);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	int alu = gc ? gc->alu : GXcopy;
	int16_t tx, ty;

	if (n == 0 || (dx | dy) == 0)
		return;

	DBG(("%s (boxes=%dx[(%d, %d), (%d, %d)...], src=+(%d, %d), alu=%d, pix.size=%dx%d)\n",
	     __FUNCTION__, n,
	     box[0].x1, box[0].y1, box[0].x2, box[0].y2,
	     dx, dy, alu,
	     pixmap->drawable.width, pixmap->drawable.height));

	get_drawable_deltas(src, pixmap, &tx, &ty);
	dx += tx;
	dy += ty;
	if (dst != src)
		get_drawable_deltas(dst, pixmap, &tx, &ty);

	if (priv && priv->gpu_bo) {
		if (!sna_pixmap_move_to_gpu(pixmap)) {
			DBG(("%s: fallback - not a pure copy and failed to move dst to GPU\n",
			     __FUNCTION__));
			goto fallback;
		}

		if (!sna->render.copy_boxes(sna, alu,
					    pixmap, priv->gpu_bo, dx, dy,
					    pixmap, priv->gpu_bo, tx, ty,
					    box, n)) {
			DBG(("%s: fallback - accelerated copy boxes failed\n",
			     __FUNCTION__));
			goto fallback;
		}

		if (!priv->gpu_only)
			sna_damage_add_boxes(&priv->gpu_damage, box, n, tx, ty);
	} else {
		FbBits *dst_bits, *src_bits;
		int stride, bpp;

fallback:
		DBG(("%s: fallback", __FUNCTION__));
		sna_pixmap_move_to_cpu(pixmap, true);

		stride = pixmap->devKind;
		bpp = pixmap->drawable.bitsPerPixel;
		if (alu == GXcopy && !reverse && !upsidedown && bpp >= 8) {
			dst_bits = (FbBits *)
				((char *)pixmap->devPrivate.ptr +
				 ty * stride + tx * bpp / 8);
			src_bits = (FbBits *)
				((char *)pixmap->devPrivate.ptr +
				 dy * stride + dx * bpp / 8);

			do {
				memcpy_blt(src_bits, dst_bits, bpp,
					   stride, stride,
					   box->x1, box->y1,
					   box->x1, box->y1,
					   box->x2 - box->x1,
					   box->y2 - box->y1);
				box++;
			} while (--n);
		} else {
			DBG(("%s: alu==GXcopy? %d, reverse? %d, upsidedown? %d, bpp? %d\n",
			     __FUNCTION__, alu == GXcopy, reverse, upsidedown, bpp));
			dst_bits = pixmap->devPrivate.ptr;
			stride /= sizeof(FbBits);
			do {
				fbBlt(dst_bits + (box->y1 + dy) * stride,
				      stride,
				      (box->x1 + dx) * bpp,

				      dst_bits + (box->y1 + ty) * stride,
				      stride,
				      (box->x1 + tx) * bpp,

				      (box->x2 - box->x1) * bpp,
				      (box->y2 - box->y1),

				      alu, -1, bpp,
				      reverse, upsidedown);
				box++;
			}while (--n);
		}
	}
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

	if (src_pixmap == dst_pixmap)
		return sna_self_copy_boxes(src, dst, gc,
					   box, n,
					   dx, dy,
					   reverse, upsidedown, bitplane,
					   closure);

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

		if (src_priv &&
		    move_to_gpu(src_pixmap, src_priv, &region.extents, alu) &&
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
		} else if (alu != GXcopy) {
			PixmapPtr tmp;
			int i;

			assert (src_pixmap->drawable.depth != 1);

			DBG(("%s: creating temporary source upload for non-copy alu [%d]\n",
			     __FUNCTION__, alu));

			tmp = sna_pixmap_create_upload(src->pScreen,
						       src->width,
						       src->height,
						       src->depth);
			if (tmp == NullPixmap)
				return;

			for (i = 0; i < n; i++) {
				assert(box->x1 + src_dx >= 0);
				assert(box->y1 + src_dy >= 0);
				assert(box->x2 + src_dx <= src_pixmap->drawable.width);
				assert(box->y2 + src_dy <= src_pixmap->drawable.height);

				assert(box->x1 + dx >= 0);
				assert(box->y1 + dy >= 0);
				assert(box->x2 + dx <= tmp->drawable.width);
				assert(box->y2 + dy <= tmp->drawable.height);

				memcpy_blt(src_pixmap->devPrivate.ptr,
					   tmp->devPrivate.ptr,
					   src_pixmap->drawable.bitsPerPixel,
					   src_pixmap->devKind,
					   tmp->devKind,
					   box[i].x1 + src_dx,
					   box[i].y1 + src_dy,
					   box[i].x1 + dx,
					   box[i].y1 + dy,
					   box[i].x2 - box[i].x1,
					   box[i].y2 - box[i].y1);
			}

			if (!sna->render.copy_boxes(sna, alu,
						    tmp, sna_pixmap_get_bo(tmp), dx, dy,
						    dst_pixmap, dst_priv->gpu_bo, dst_dx, dst_dy,
						    box, n)) {
				DBG(("%s: fallback - accelerated copy boxes failed\n",
				     __FUNCTION__));
				tmp->drawable.pScreen->DestroyPixmap(tmp);
				goto fallback;
			}
			tmp->drawable.pScreen->DestroyPixmap(tmp);

			RegionTranslate(&region, dst_dx, dst_dy);
			assert_pixmap_contains_box(dst_pixmap,
						   RegionExtents(&region));
			sna_damage_add(&dst_priv->gpu_damage, &region);
			RegionTranslate(&region, -dst_dx, -dst_dy);
		} else {
			if (src_priv) {
				RegionTranslate(&region, src_dx, src_dy);
				sna_drawable_move_region_to_cpu(&src_pixmap->drawable,
								&region, false);
				RegionTranslate(&region, -src_dx, -src_dy);
			}

			if (!dst_priv->pinned && replaces) {
				stride = src_pixmap->devKind;
				bits = src_pixmap->devPrivate.ptr;
				bits += (src_dy + box->y1) * stride + (src_dx + box->x1) * bpp / 8;
				assert(src_dy + box->y1 + dst_pixmap->drawable.height <= src_pixmap->drawable.height);
				assert(src_dx + box->x1 + dst_pixmap->drawable.width <= src_pixmap->drawable.width);

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
			DBG(("%s: alu==GXcopy? %d, reverse? %d, upsidedown? %d, bpp? %d\n",
			     __FUNCTION__, alu == GXcopy, reverse, upsidedown, bpp));
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

	if (wedged(sna) || !PM_IS_SOLID(dst, gc->planemask)) {
		RegionRec region;

		DBG(("%s: -- fallback, wedged=%d, solid=%d [%x]\n",
		     __FUNCTION__, sna->kgem.wedged,
		     PM_IS_SOLID(dst, gc->planemask),
		     (unsigned)gc->planemask));

		region.extents.x1 = dst_x + dst->x;
		region.extents.y1 = dst_y + dst->y;
		region.extents.x2 = region.extents.x1 + width;
		region.extents.y2 = region.extents.y1 + height;
		region.data = NULL;
		if (gc->pCompositeClip)
			RegionIntersect(&region, &region, gc->pCompositeClip);
		if (!RegionNotEmpty(&region))
			return NULL;

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
			src == dst ? sna_self_copy_boxes : sna_copy_boxes,
			0, NULL);
}

inline static Bool
box_intersect(BoxPtr a, const BoxRec *b)
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

inline static bool
sna_fill_init_blt(struct sna_fill_op *fill,
		  struct sna *sna,
		  PixmapPtr pixmap,
		  struct kgem_bo *bo,
		  uint8_t alu,
		  uint32_t pixel)
{
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

static const BoxRec *
find_clip_box_for_y(const BoxRec *begin, const BoxRec *end, int16_t y)
{
    const BoxRec *mid;

    if (end == begin)
	return end;

    if (end - begin == 1) {
	if (begin->y2 > y)
	    return begin;
	else
	    return end;
    }

    mid = begin + (end - begin) / 2;
    if (mid->y2 > y)
	/* If no box is found in [begin, mid], the function
	 * will return @mid, which is then known to be the
	 * correct answer.
	 */
	return find_clip_box_for_y(begin, mid, y);
    else
	return find_clip_box_for_y(mid, end, y);
}


static Bool
sna_fill_spans_blt(DrawablePtr drawable,
		   struct kgem_bo *bo, struct sna_damage **damage,
		   GCPtr gc, int n,
		   DDXPointPtr pt, int *width, int sorted,
		   const BoxRec *extents, unsigned clipped)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	int16_t dx, dy;
	struct sna_fill_op fill;
	BoxRec box[512], *b = box, *const last_box = box + ARRAY_SIZE(box);
	static void * const jump[] = {
		&&no_damage_translate,
		&&damage_translate,
		&&no_damage_clipped_translate,
		&&damage_clipped_translate,

		&&no_damage,
		&&damage,
		&&no_damage_clipped,
		&&damage_clipped,
	};
	unsigned v;

	DBG(("%s: alu=%d, fg=%08lx, damge=%p, clipped?=%d\n",
	     __FUNCTION__, gc->alu, gc->fgPixel, damage, clipped));

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return false;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	v = (damage != NULL) | clipped | gc->miTranslate << 2;
	goto *jump[v];

no_damage_translate:
	dx += drawable->x;
	dy += drawable->y;
no_damage:
	if (dx|dy) {
		do {
			int nbox = n;
			if (nbox > last_box - box)
				nbox = last_box - box;
			n -= nbox;
			do {
				*(DDXPointRec *)b = *pt++;
				b->x1 += dx;
				b->y1 += dy;
				b->x2 = b->x1 + (int)*width++;
				b->y2 = b->y1 + 1;
				b++;
			} while (--nbox);
			fill.boxes(sna, &fill, box, b - box);
			b = box;
		} while (n);
	} else {
		do {
			int nbox = n;
			if (nbox > last_box - box)
				nbox = last_box - box;
			n -= nbox;
			do {
				*(DDXPointRec *)b = *pt++;
				b->x2 = b->x1 + (int)*width++;
				b->y2 = b->y1 + 1;
				b++;
			} while (--nbox);
			fill.boxes(sna, &fill, box, b - box);
			b = box;
		} while (n);
	}
	goto done;

damage_translate:
	dx += drawable->x;
	dy += drawable->y;
damage:
	do {
		*(DDXPointRec *)b = *pt++;
		b->x1 += dx;
		b->y1 += dy;
		b->x2 = b->x1 + (int)*width++;
		b->y2 = b->y1 + 1;

		if (++b == last_box) {
			fill.boxes(sna, &fill, box, last_box - box);
			sna_damage_add_boxes(damage, box, last_box - box, 0, 0);
			b = box;
		}
	} while (--n);
	if (b != box) {
		fill.boxes(sna, &fill, box, b - box);
		sna_damage_add_boxes(damage, box, b - box, 0, 0);
	}
	goto done;

	{
		RegionRec clip;
		int i;

no_damage_clipped_translate:
		for (i = 0; i < n; i++) {
			/* XXX overflow? */
			pt->x += drawable->x;
			pt->y += drawable->y;
		}

no_damage_clipped:
		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			return TRUE;

		assert(clip.extents.x1 >= 0);
		assert(clip.extents.y1 >= 0);
		assert(clip.extents.x2 <= pixmap->drawable.width);
		assert(clip.extents.y2 <= pixmap->drawable.height);

		DBG(("%s: clip %d x [(%d, %d), (%d, %d)] x %d [(%d, %d)...]\n",
		     __FUNCTION__,
		     REGION_NUM_RECTS(&clip),
		     clip.extents.x1, clip.extents.y1, clip.extents.x2, clip.extents.y2,
		     n, pt->x, pt->y));

		if (clip.data == NULL) {
			do {
				*(DDXPointRec *)b = *pt++;
				b->x2 = b->x1 + (int)*width++;
				b->y2 = b->y1 + 1;

				if (box_intersect(b, &clip.extents)) {
					b->x1 += dx;
					b->x2 += dx;
					b->y1 += dy;
					b->y2 += dy;
					if (++b == last_box) {
						fill.boxes(sna, &fill, box, last_box - box);
						b = box;
					}
				}
			} while (--n);
		} else {
			const BoxRec * const clip_start = RegionBoxptr(&clip);
			const BoxRec * const clip_end = clip_start + clip.data->numRects;
			do {
				int16_t X1 = pt->x;
				int16_t y = pt->y;
				int16_t X2 = X1 + (int)*width;
				const BoxRec *c;

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

				c = find_clip_box_for_y(clip_start,
							clip_end,
							y);
				while (c != clip_end) {
					if (y + 1 <= c->y1)
						break;

					if (X1 >= c->x2)
						break;
					if (X2 <= c->x1) {
						c++;
						continue;
					}

					b->x1 = c->x1;
					b->x2 = c->x2;
					c++;

					if (b->x1 < X1)
						b->x1 = X1;
					if (b->x2 > X2)
						b->x2 = X2;
					if (b->x2 <= b->x1)
						continue;

					b->x1 += dx;
					b->x2 += dx;
					b->y1 = y + dy;
					b->y2 = b->y1 + 1;
					if (++b == last_box) {
						fill.boxes(sna, &fill, box, last_box - box);
						b = box;
					}
				}
			} while (--n);
			RegionUninit(&clip);
		}
		if (b != box)
			fill.boxes(sna, &fill, box, b - box);
		goto done;
	}

	{
		RegionRec clip;
		int i;

damage_clipped_translate:
		for (i = 0; i < n; i++) {
			/* XXX overflow? */
			pt->x += drawable->x;
			pt->y += drawable->y;
		}

damage_clipped:
		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			return TRUE;

		assert(clip.extents.x1 >= 0);
		assert(clip.extents.y1 >= 0);
		assert(clip.extents.x2 <= pixmap->drawable.width);
		assert(clip.extents.y2 <= pixmap->drawable.height);

		DBG(("%s: clip %d x [(%d, %d), (%d, %d)] x %d [(%d, %d)...]\n",
		     __FUNCTION__,
		     REGION_NUM_RECTS(&clip),
		     clip.extents.x1, clip.extents.y1, clip.extents.x2, clip.extents.y2,
		     n, pt->x, pt->y));

		if (clip.data == NULL) {
			do {
				*(DDXPointRec *)b = *pt++;
				b->x2 = b->x1 + (int)*width++;
				b->y2 = b->y1 + 1;

				if (box_intersect(b, &clip.extents)) {
					b->x1 += dx;
					b->x2 += dx;
					b->y1 += dy;
					b->y2 += dy;
					if (++b == last_box) {
						fill.boxes(sna, &fill, box, last_box - box);
						sna_damage_add_boxes(damage, box, b - box, 0, 0);
						b = box;
					}
				}
			} while (--n);
		} else {
			const BoxRec * const clip_start = RegionBoxptr(&clip);
			const BoxRec * const clip_end = clip_start + clip.data->numRects;
			do {
				int16_t X1 = pt->x;
				int16_t y = pt->y;
				int16_t X2 = X1 + (int)*width;
				const BoxRec *c;

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

				c = find_clip_box_for_y(clip_start,
							clip_end,
							y);
				while (c != clip_end) {
					if (y + 1 <= c->y1)
						break;

					if (X1 >= c->x2)
						break;
					if (X2 <= c->x1) {
						c++;
						continue;
					}

					b->x1 = c->x1;
					b->x2 = c->x2;
					c++;

					if (b->x1 < X1)
						b->x1 = X1;
					if (b->x2 > X2)
						b->x2 = X2;
					if (b->x2 <= b->x1)
						continue;

					b->x1 += dx;
					b->x2 += dx;
					b->y1 = y + dy;
					b->y2 = b->y1 + 1;
					if (++b == last_box) {
						fill.boxes(sna, &fill, box, last_box - box);
						sna_damage_add_boxes(damage, box, last_box - box, 0, 0);
						b = box;
					}
				}
			} while (--n);
			RegionUninit(&clip);
		}
		if (b != box) {
			fill.boxes(sna, &fill, box, b - box);
			sna_damage_add_boxes(damage, box, b - box, 0, 0);
		}
		goto done;
	}

done:
	fill.done(sna, &fill);
	return TRUE;
}

static unsigned
sna_spans_extents(DrawablePtr drawable, GCPtr gc,
		  int n, DDXPointPtr pt, int *width,
		  BoxPtr out)
{
	BoxRec box;
	bool clipped = false;

	if (n == 0)
		return 0;

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
		clipped = clip_box(&box, gc);
	}
	if (box_empty(&box))
		return 0;

	*out = box;
	return 1 | clipped << 1;
}

static struct sna_damage **
reduce_damage(DrawablePtr drawable,
	      struct sna_damage **damage,
	      const BoxRec *box)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	int16_t dx, dy;
	BoxRec r;

	if (*damage == NULL)
		return damage;

	if (sna_damage_is_all(damage,
			      pixmap->drawable.width,
			      pixmap->drawable.height))
		return NULL;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	r = *box;
	r.x1 += dx; r.x2 += dx;
	r.y1 += dy; r.y2 += dy;
	if (sna_damage_contains_box(*damage, &r) == PIXMAN_REGION_IN)
		return NULL;
	else
		return damage;
}

static Bool
sna_poly_fill_rect_tiled(DrawablePtr drawable,
			 struct kgem_bo *bo,
			 struct sna_damage **damage,
			 GCPtr gc, int n, xRectangle *rect,
			 const BoxRec *extents, unsigned clipped);

static bool
can_fill_spans(DrawablePtr drawable, GCPtr gc)
{
	DBG(("%s: is-solid-mask? %d\n", __FUNCTION__,
	     PM_IS_SOLID(drawable, gc->planemask)));
	if (!PM_IS_SOLID(drawable, gc->planemask))
		return false;

	DBG(("%s: non-stipple fill? %d\n", __FUNCTION__,
	     gc->fillStyle != FillStippled));
	return gc->fillStyle == FillSolid || gc->fillStyle == FillTiled;
}

static void
sna_fill_spans(DrawablePtr drawable, GCPtr gc, int n,
	       DDXPointPtr pt, int *width, int sorted)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	RegionRec region;
	unsigned flags;

	DBG(("%s(n=%d, pt[0]=(%d, %d)+%d, sorted=%d\n",
	     __FUNCTION__, n, pt[0].x, pt[0].y, width[0], sorted));

	flags = sna_spans_extents(drawable, gc, n, pt, width, &region.extents);
	if (flags == 0)
		return;

	DBG(("%s: extents (%d, %d), (%d, %d)\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	DBG(("%s: fillStyle=%x [%d], mask=%lx [%d]\n", __FUNCTION__,
	     gc->fillStyle, gc->fillStyle == FillSolid,
	     gc->planemask, PM_IS_SOLID(drawable, gc->planemask)));
	if (!PM_IS_SOLID(drawable, gc->planemask))
		goto fallback;

	if (gc->fillStyle == FillSolid) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);

		DBG(("%s: trying solid fill [alu=%d, pixel=%08lx] blt paths\n",
		     __FUNCTION__, gc->alu, gc->fgPixel));

		if (sna_drawable_use_gpu_bo(drawable, &region.extents) &&
		    sna_fill_spans_blt(drawable,
				       priv->gpu_bo,
				       priv->gpu_only ? NULL : reduce_damage(drawable, &priv->gpu_damage, &region.extents),
				       gc, n, pt, width, sorted,
				       &region.extents, flags & 2))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &region.extents) &&
		    sna_fill_spans_blt(drawable,
				       priv->cpu_bo,
				       reduce_damage(drawable, &priv->cpu_damage, &region.extents),
				       gc, n, pt, width, sorted,
				       &region.extents, flags & 2))
			return;
	} else if (gc->fillStyle == FillTiled) {
		/* Try converting these to a set of rectangles instead */

		if (sna_drawable_use_gpu_bo(drawable, &region.extents)) {
			struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);
			xRectangle *rect;
			int i;

			DBG(("%s: converting to rectagnles\n", __FUNCTION__));

			rect = malloc (n * sizeof (xRectangle));
			if (rect == NULL)
				return;

			for (i = 0; i < n; i++) {
				rect[i].x = pt[i].x;
				rect[i].width = width[i];
				rect[i].y = pt[i].y;
				rect[i].height = 1;
			}

			i = sna_poly_fill_rect_tiled(drawable,
						     priv->gpu_bo,
						     priv->gpu_only ? NULL : reduce_damage(drawable, &priv->gpu_damage, &region.extents),
						     gc, n, rect,
						     &region.extents, flags & 2);
			free (rect);

			if (i)
				return;
		}
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
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
	RegionRec region;

	if (sna_spans_extents(drawable, gc, n, pt, width, &region.extents) == 0)
		return;

	DBG(("%s: extents=(%d, %d), (%d, %d)\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
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
	RegionRec region;

	DBG(("%s: src=(%d, %d), dst=(%d, %d), size=%dx%d\n", __FUNCTION__,
	     src_x, src_y, dst_x, dst_y, w, h));

	region.extents.x1 = dst_x + dst->x;
	region.extents.y1 = dst_y + dst->y;
	region.extents.x2 = region.extents.x1 + w;
	region.extents.y2 = region.extents.y1 + h;
	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return NULL;

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
		   GCPtr gc, int mode, int n, DDXPointPtr pt,
		   bool clipped)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	RegionPtr clip = fbGetCompositeClip(gc);
	BoxRec box[512], *b = box, * const last_box = box + ARRAY_SIZE(box);
	struct sna_fill_op fill;
	DDXPointRec last;
	int16_t dx, dy;

	DBG(("%s: alu=%d, pixel=%08lx, clipped?=%d\n",
	     __FUNCTION__, gc->alu, gc->fgPixel, clipped));

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	last.x = drawable->x;
	last.y = drawable->y;

	if (!clipped) {
		last.x += dx;
		last.y += dy;

		sna_damage_add_points(damage, pt, n, last.x, last.y);
		do {
			int nbox = n;
			if (nbox > ARRAY_SIZE(box))
				nbox = ARRAY_SIZE(box);
			n -= nbox;
			do {
				*(DDXPointRec *)b = *pt++;

				b->x1 += last.x;
				b->y1 += last.y;
				if (mode == CoordModePrevious)
					last = *(DDXPointRec *)b;

				b->x2 = b->x1 + 1;
				b->y2 = b->y1 + 1;
				b++;
			} while (--nbox);
			fill.boxes(sna, &fill, box, b - box);
			b = box;
		} while (n);
	} else {
		while (n--) {
			int x, y;

			x = pt->x;
			y = pt->y;
			pt++;
			if (mode == CoordModePrevious) {
				x += last.x;
				y += last.y;
				last.x = x;
				last.y = y;
			} else {
				x += drawable->x;
				y += drawable->y;
			}

			if (RegionContainsPoint(clip, x, y, NULL)) {
				b->x1 = x + dx;
				b->y1 = y + dy;
				b->x2 = b->x1 + 1;
				b->y2 = b->y1 + 1;
				if (++b == last_box){
					fill.boxes(sna, &fill, box, last_box - box);
					if (damage)
						sna_damage_add_boxes(damage, box, last_box-box, 0, 0);
					b = box;
				}
			}
		}
		if (b != box){
			fill.boxes(sna, &fill, box, b - box);
			if (damage)
				sna_damage_add_boxes(damage, box, b-box, 0, 0);
		}
	}
	fill.done(sna, &fill);
	return TRUE;
}

static unsigned
sna_poly_point_extents(DrawablePtr drawable, GCPtr gc,
		       int mode, int n, DDXPointPtr pt, BoxPtr out)
{
	BoxRec box;
	bool clipped;

	if (n == 0)
		return 0;

	box.x2 = box.x1 = pt->x;
	box.y2 = box.y1 = pt->y;
	while (--n) {
		pt++;
		box_add_pt(&box, pt->x, pt->y);
	}
	box.x2++;
	box.y2++;

	clipped = trim_and_translate_box(&box, drawable, gc);
	if (box_empty(&box))
		return 0;

	*out = box;
	return 1 | clipped << 1;
}

static void
sna_poly_point(DrawablePtr drawable, GCPtr gc,
	       int mode, int n, DDXPointPtr pt)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	RegionRec region;
	unsigned flags;

	DBG(("%s(mode=%d, n=%d, pt[0]=(%d, %d)\n",
	     __FUNCTION__, mode, n, pt[0].x, pt[0].y));

	flags = sna_poly_point_extents(drawable, gc, mode, n, pt, &region.extents);
	if (flags == 0)
		return;

	DBG(("%s: extents (%d, %d), (%d, %d), flags=%x\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2,
	     flags));

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (gc->fillStyle == FillSolid &&
	    PM_IS_SOLID(drawable, gc->planemask)) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);

		DBG(("%s: trying solid fill [%08lx] blt paths\n",
		     __FUNCTION__, gc->fgPixel));

		if (sna_drawable_use_gpu_bo(drawable, &region.extents) &&
		    sna_poly_point_blt(drawable,
				       priv->gpu_bo,
				      priv->gpu_only ? NULL : reduce_damage(drawable, &priv->gpu_damage, &region.extents),
				       gc, mode, n, pt, flags & 2))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &region.extents) &&
		    sna_poly_point_blt(drawable,
				       priv->cpu_bo,
				       reduce_damage(drawable, &priv->cpu_damage, &region.extents),
				       gc, mode, n, pt, flags & 2))
			return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	fbPolyPoint(drawable, gc, mode, n, pt);
}

static bool
sna_poly_zero_line_blt(DrawablePtr drawable,
		       struct kgem_bo *bo,
		       struct sna_damage **damage,
		       GCPtr gc, int mode, const int _n, const DDXPointRec * const _pt,
		       const BoxRec *extents, unsigned clipped)
{
	static void * const _jump[] = {
		&&no_damage,
		&&damage,

		&&no_damage_offset,
		&&damage_offset,
	};

	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	int x2, y2, xstart, ystart;
	int oc2, pt2_clipped = 0;
	unsigned int bias = miGetZeroLineBias(drawable->pScreen);
	bool degenerate = true;
	struct sna_fill_op fill;
	RegionRec clip;
	BoxRec box[512], *b, * const last_box = box + ARRAY_SIZE(box);
	const BoxRec *last_extents;
	int16_t dx, dy;
	void *jump, *ret;

	DBG(("%s: alu=%d, pixel=%lx, n=%d, clipped=%d, damage=%p\n",
	     __FUNCTION__, gc->alu, gc->fgPixel, _n, clipped, damage));
	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	region_set(&clip, extents);
	if (clipped) {
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			return TRUE;
	}

	jump = _jump[(damage != NULL) | !!(dx|dy) << 1];
	DBG(("%s: [clipped] extents=(%d, %d), (%d, %d), delta=(%d, %d)\n",
	     __FUNCTION__,
	     clip.extents.x1, clip.extents.y1,
	     clip.extents.x2, clip.extents.y2,
	     dx, dy));

	extents = REGION_RECTS(&clip);
	last_extents = extents + REGION_NUM_RECTS(&clip);

	b = box;
	do {
		int n = _n;
		const DDXPointRec *pt = _pt;

		xstart = pt->x + drawable->x;
		ystart = pt->y + drawable->y;

		/* x2, y2, oc2 copied to x1, y1, oc1 at top of loop to simplify
		 * iteration logic
		 */
		x2 = xstart;
		y2 = ystart;
		oc2 = 0;
		MIOUTCODES(oc2, x2, y2,
			   extents->x1,
			   extents->y1,
			   extents->x2,
			   extents->y2);

		while (--n) {
			int16_t sdx, sdy;
			int16_t adx, ady;
			int16_t e, e1, e2, e3;
			int16_t length;
			int x1 = x2, x;
			int y1 = y2, y;
			int oc1 = oc2;
			int octant;

			++pt;

			x2 = pt->x;
			y2 = pt->y;
			if (mode == CoordModePrevious) {
				x2 += x1;
				y2 += y1;
			} else {
				x2 += drawable->x;
				y2 += drawable->y;
			}
			DBG(("%s: segment (%d, %d) to (%d, %d)\n",
			     __FUNCTION__, x1, y1, x2, y2));
			if (x2 == x1 && y2 == y1)
				continue;

			degenerate = false;

			oc2 = 0;
			MIOUTCODES(oc2, x2, y2,
				   extents->x1,
				   extents->y1,
				   extents->x2,
				   extents->y2);
			if (oc1 & oc2)
				continue;

			CalcLineDeltas(x1, y1, x2, y2,
				       adx, ady, sdx, sdy,
				       1, 1, octant);

			DBG(("%s: adx=(%d, %d), sdx=(%d, %d)\n",
			     __FUNCTION__, adx, ady, sdx, sdy));
			if (adx == 0 || ady == 0) {
				if (x1 <= x2) {
					b->x1 = x1;
					b->x2 = x2;
				} else {
					b->x1 = x2;
					b->x2 = x1;
				}
				if (y1 <= y2) {
					b->y1 = y1;
					b->y2 = y2;
				} else {
					b->y1 = y2;
					b->y2 = y1;
				}
				b->x2++;
				b->y2++;
				if (oc1 | oc2)
					box_intersect(b, extents);
				if (++b == last_box) {
					ret = &&rectangle_continue;
					goto *jump;
rectangle_continue:
					b = box;
				}
			} else if (adx >= ady) {
				/* X-major segment */
				e1 = ady << 1;
				e2 = e1 - (adx << 1);
				e  = e1 - adx;
				length = adx;	/* don't draw endpoint in main loop */

				FIXUP_ERROR(e, octant, bias);

				x = x1;
				y = y1;
				pt2_clipped = 0;

				if (oc1 | oc2) {
					int x2_clipped = x2, y2_clipped = y2;
					int pt1_clipped;

					if (miZeroClipLine(extents->x1, extents->y1,
							   extents->x2, extents->y2,
							   &x, &y, &x2_clipped, &y2_clipped,
							   adx, ady,
							   &pt1_clipped, &pt2_clipped,
							   octant, bias, oc1, oc2) == -1)
						continue;

					length = abs(x2_clipped - x);

					/* if we've clipped the endpoint, always draw the full length
					 * of the segment, because then the capstyle doesn't matter
					 */
					if (pt2_clipped)
						length++;

					if (pt1_clipped) {
						int clipdx = abs(x - x1);
						int clipdy = abs(y - y1);
						e += clipdy * e2 + (clipdx - clipdy) * e1;
					}
				}
				if (length == 0)
					continue;

				e3 = e2 - e1;
				e  = e - e1;

				b->x1 = x;
				b->y2 = b->y1 = y;
				while (--length) {
					e += e1;
					if (e >= 0) {
						b->x2 = x;
						if (b->x2 < b->x1) {
							int16_t t = b->x1;
							b->x1 = b->x2;
							b->x2 = t;
						}
						b->x2++;
						b->y2++;
						if (++b == last_box) {
							ret = &&X_continue;
							goto *jump;
X_continue:
							b = box;
						}
						y += sdy;
						e += e3;
						b->y2 = b->y1 = y;
						b->x1 = x + dx;
					}
					x += sdx;
				}

				b->x2 = x;
				if (b->x2 < b->x1) {
					int16_t t = b->x1;
					b->x1 = b->x2;
					b->x2 = t;
				}
				b->x2++;
				b->y2++;
				if (++b == last_box) {
					ret = &&X_continue2;
					goto *jump;
X_continue2:
					b = box;
				}
			} else {
				/* Y-major segment */
				e1 = adx << 1;
				e2 = e1 - (ady << 1);
				e  = e1 - ady;
				length  = ady;	/* don't draw endpoint in main loop */

				SetYMajorOctant(octant);
				FIXUP_ERROR(e, octant, bias);

				x = x1;
				y = y1;
				pt2_clipped = 0;

				if (oc1 | oc2) {
					int x2_clipped = x2, y2_clipped = y2;
					int pt1_clipped;

					if (miZeroClipLine(extents->x1, extents->y1,
							   extents->x2, extents->y2,
							   &x, &y, &x2_clipped, &y2_clipped,
							   adx, ady,
							   &pt1_clipped, &pt2_clipped,
							   octant, bias, oc1, oc2) == -1)
						continue;

					length = abs(y2 - y);

					/* if we've clipped the endpoint, always draw the full length
					 * of the segment, because then the capstyle doesn't matter
					 */
					if (pt2_clipped)
						length++;

					if (pt1_clipped) {
						int clipdx = abs(x - x1);
						int clipdy = abs(y - y1);
						e += clipdx * e2 + (clipdy - clipdx) * e1;
					}
				}
				if (length == 0)
					continue;

				e3 = e2 - e1;
				e  = e - e1;

				b->x2 = b->x1 = x;
				b->y1 = y;
				while (--length) {
					e += e1;
					if (e >= 0) {
						b->y2 = y;
						if (b->y2 < b->y1) {
							int16_t t = b->y1;
							b->y1 = b->y2;
							b->y2 = t;
						}
						b->x2++;
						b->y2++;
						if (++b == last_box) {
							ret = &&Y_continue;
							goto *jump;
Y_continue:
							b = box;
						}
						x += sdx;
						e += e3;
						b->x2 = b->x1 = x;
						b->y1 = y + sdy;
					}
					y += sdy;
				}

				b->y2 = y;
				if (b->y2 < b->y1) {
					int16_t t = b->y1;
					b->y1 = b->y2;
					b->y2 = t;
				}
				b->x2++;
				b->y2++;
				if (++b == last_box) {
					ret = &&Y_continue2;
					goto *jump;
Y_continue2:
					b = box;
				}
			}
		}

#if 0
		/* Only do the CapNotLast check on the last segment
		 * and only if the endpoint wasn't clipped.  And then, if the last
		 * point is the same as the first point, do not draw it, unless the
		 * line is degenerate
		 */
		if (!pt2_clipped &&
		    gc->capStyle != CapNotLast &&
		    !(xstart == x2 && ystart == y2 && !degenerate))
		{
			b->x2 = x2;
			b->y2 = y2;
			if (b->x2 < b->x1) {
				int16_t t = b->x1;
				b->x1 = b->x2;
				b->x2 = t;
			}
			if (b->y2 < b->y1) {
				int16_t t = b->y1;
				b->y1 = b->y2;
				b->y2 = t;
			}
			b->x2++;
			b->y2++;
			b++;
		}
#endif
	} while (++extents != last_extents);

	if (b != box) {
		ret = &&done;
		goto *jump;
	}

done:
	fill.done(sna, &fill);
	return true;

damage:
	sna_damage_add_boxes(damage, box, b-box, 0, 0);
no_damage:
	fill.boxes(sna, &fill, box, b-box);
	goto *ret;

no_damage_offset:
	{
		BoxRec *bb = box;
		do {
			bb->x1 += dx;
			bb->x2 += dx;
			bb->y1 += dy;
			bb->y2 += dy;
		} while (++bb != b);
		fill.boxes(sna, &fill, box, b - box);
	}
	goto *ret;

damage_offset:
	{
		BoxRec *bb = box;
		do {
			bb->x1 += dx;
			bb->x2 += dx;
			bb->y1 += dy;
			bb->y2 += dy;
		} while (++bb != b);
		fill.boxes(sna, &fill, box, b - box);
		sna_damage_add_boxes(damage, box, b - box, 0, 0);
	}
	goto *ret;
}

static Bool
sna_poly_line_blt(DrawablePtr drawable,
		  struct kgem_bo *bo,
		  struct sna_damage **damage,
		  GCPtr gc, int mode, int n, DDXPointPtr pt,
		  const BoxRec *extents, bool clipped)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	BoxRec boxes[512], *b = boxes, * const last_box = boxes + ARRAY_SIZE(boxes);
	struct sna_fill_op fill;
	DDXPointRec last;
	int16_t dx, dy;

	DBG(("%s: alu=%d, fg=%08lx\n", __FUNCTION__, gc->alu, gc->fgPixel));

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	if (!clipped) {
		dx += drawable->x;
		dy += drawable->y;

		last.x = pt->x + dx;
		last.y = pt->y + dy;
		pt++;

		while (--n) {
			DDXPointRec p;

			p = *pt++;
			if (mode == CoordModePrevious) {
				p.x += last.x;
				p.y += last.y;
			} else {
				p.x += dx;
				p.y += dy;
			}
			if (last.x == p.x) {
				b->x1 = last.x;
				b->x2 = last.x + 1;
			} else if (last.x < p.x) {
				b->x1 = last.x;
				b->x2 = p.x;
			} else {
				b->x1 = p.x;
				b->x2 = last.x;
			}
			if (last.y == p.y) {
				b->y1 = last.y;
				b->y2 = last.y + 1;
			} else if (last.y < p.y) {
				b->y1 = last.y;
				b->y2 = p.y;
			} else {
				b->y1 = p.y;
				b->y2 = last.y;
			}
			DBG(("%s: blt (%d, %d), (%d, %d)\n",
			     __FUNCTION__,
			     b->x1, b->y1, b->x2, b->y2));
			if (++b == last_box) {
				fill.boxes(sna, &fill, boxes, last_box - boxes);
				if (damage)
					sna_damage_add_boxes(damage, boxes, last_box - boxes, 0, 0);
				b = boxes;
			}

			last = p;
		}
	} else {
		RegionRec clip;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			return TRUE;

		last.x = pt->x + drawable->x;
		last.y = pt->y + drawable->y;
		pt++;

		if (clip.data == NULL) {
			while (--n) {
				DDXPointRec p;

				p = *pt++;
				if (mode == CoordModePrevious) {
					p.x += last.x;
					p.y += last.y;
				} else {
					p.x += drawable->x;
					p.y += drawable->y;
				}
				if (last.x == p.x) {
					b->x1 = last.x;
					b->x2 = last.x + 1;
				} else if (last.x < p.x) {
					b->x1 = last.x;
					b->x2 = p.x;
				} else {
					b->x1 = p.x;
					b->x2 = last.x;
				}
				if (last.y == p.y) {
					b->y1 = last.y;
					b->y2 = last.y + 1;
				} else if (last.y < p.y) {
					b->y1 = last.y;
					b->y2 = p.y;
				} else {
					b->y1 = p.y;
					b->y2 = last.y;
				}
				DBG(("%s: blt (%d, %d), (%d, %d)\n",
				     __FUNCTION__,
				     b->x1, b->y1, b->x2, b->y2));
				if (box_intersect(b, &clip.extents)) {
					b->x1 += dx;
					b->x2 += dx;
					b->y1 += dy;
					b->y2 += dy;
					if (++b == last_box) {
						fill.boxes(sna, &fill, boxes, last_box - boxes);
						if (damage)
							sna_damage_add_boxes(damage, boxes, last_box - boxes, 0, 0);
						b = boxes;
					}
				}

				last = p;
			}
		} else {
			const BoxRec * const clip_start = RegionBoxptr(&clip);
			const BoxRec * const clip_end = clip_start + clip.data->numRects;
			const BoxRec *c;

			while (--n) {
				DDXPointRec p;
				BoxRec box;

				p = *pt++;
				if (mode == CoordModePrevious) {
					p.x += last.x;
					p.y += last.y;
				} else {
					p.x += drawable->x;
					p.y += drawable->y;
				}
				if (last.x == p.x) {
					box.x1 = last.x;
					box.x2 = last.x + 1;
				} else if (last.x < p.x) {
					box.x1 = last.x;
					box.x2 = p.x;
				} else {
					box.x1 = p.x;
					box.x2 = last.x;
				}
				if (last.y == p.y) {
					box.y1 = last.y;
					box.y2 = last.y + 1;
				} else if (last.y < p.y) {
					box.y1 = last.y;
					box.y2 = p.y;
				} else {
					box.y1 = p.y;
					box.y2 = last.y;
				}
				DBG(("%s: blt (%d, %d), (%d, %d)\n",
				     __FUNCTION__,
				     box.x1, box.y1, box.x2, box.y2));

				c = find_clip_box_for_y(clip_start,
							clip_end,
							box.y1);
				while (c != clip_end) {
					if (box.y2 <= c->y1)
						break;

					*b = box;
					if (box_intersect(b, c++)) {
						b->x1 += dx;
						b->x2 += dx;
						b->y1 += dy;
						b->y2 += dy;
						if (++b == last_box) {
							fill.boxes(sna, &fill, boxes, last_box-boxes);
							if (damage)
								sna_damage_add_boxes(damage, boxes, last_box-boxes, 0, 0);
							b = boxes;
						}
					}
				}

				last = p;
			}
		}
		RegionUninit(&clip);
	}
	if (b != boxes) {
		fill.boxes(sna, &fill, boxes, b - boxes);
		if (damage)
			sna_damage_add_boxes(damage, boxes, b - boxes, 0, 0);
	}
	fill.done(sna, &fill);
	return TRUE;
}

static unsigned
sna_poly_line_extents(DrawablePtr drawable, GCPtr gc,
		      int mode, int n, DDXPointPtr pt,
		      BoxPtr out)
{
	BoxRec box;
	int extra = gc->lineWidth >> 1;
	bool clip, blt = true;

	if (n == 0)
		return 0;

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
			if (blt)
				blt &= pt->x == 0 || pt->y == 0;
			box_add_pt(&box, x, y);
		}
	} else {
		int x = box.x1;
		int y = box.y1;
		while (--n) {
			pt++;
			if (blt) {
				blt &= pt->x == x || pt->y == y;
				x = pt->x;
				y = pt->y;
			}
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

	clip = trim_and_translate_box(&box, drawable, gc);
	if (box_empty(&box))
		return 0;

	*out = box;
	return 1 | blt << 1 | clip << 2;
}

static void
sna_poly_line(DrawablePtr drawable, GCPtr gc,
	      int mode, int n, DDXPointPtr pt)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	RegionRec region;
	unsigned flags;

	DBG(("%s(mode=%d, n=%d, pt[0]=(%d, %d), lineWidth=%d\n",
	     __FUNCTION__, mode, n, pt[0].x, pt[0].y, gc->lineWidth));

	flags = sna_poly_line_extents(drawable, gc, mode, n, pt,
				      &region.extents);
	if (flags == 0)
		return;

	DBG(("%s: extents (%d, %d), (%d, %d)\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	DBG(("%s: fill=%d [%d], line=%d [%d], width=%d, mask=%lu [%d], rectlinear=%d\n",
	     __FUNCTION__,
	     gc->fillStyle, gc->fillStyle == FillSolid,
	     gc->lineStyle, gc->lineStyle == LineSolid,
	     gc->lineWidth,
	     gc->planemask, PM_IS_SOLID(drawable, gc->planemask),
	     flags & 2));
	if (gc->fillStyle == FillSolid &&
	    gc->lineStyle == LineSolid &&
	    gc->lineWidth <= 1 &&
	    PM_IS_SOLID(drawable, gc->planemask)) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);

		DBG(("%s: trying solid fill [%08lx]\n",
		     __FUNCTION__, gc->fgPixel));

	    if (flags & 2) {
		if (sna_drawable_use_gpu_bo(drawable, &region.extents) &&
		    sna_poly_line_blt(drawable,
				      priv->gpu_bo,
				      priv->gpu_only ? NULL : reduce_damage(drawable, &priv->gpu_damage, &region.extents),
				      gc, mode, n, pt,
				      &region.extents, flags & 4))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &region.extents) &&
		    sna_poly_line_blt(drawable,
				      priv->cpu_bo,
				      reduce_damage(drawable, &priv->cpu_damage, &region.extents),
				      gc, mode, n, pt,
				      &region.extents, flags & 4))
			return;
	    } else { /* !rectilinear */
		if (USE_ZERO_SPANS &&
		    sna_drawable_use_gpu_bo(drawable, &region.extents) &&
		    sna_poly_zero_line_blt(drawable,
					   priv->gpu_bo,
					   priv->gpu_only ? NULL : reduce_damage(drawable, &priv->gpu_damage, &region.extents),
					   gc, mode, n, pt,
					   &region.extents, flags & 4))
			return;

	    }
	}

	if (USE_SPANS && can_fill_spans(drawable, gc) &&
	    sna_drawable_use_gpu_bo(drawable, &region.extents)) {
		DBG(("%s: converting line into spans\n", __FUNCTION__));
		switch (gc->lineStyle) {
		case LineSolid:
			if (gc->lineWidth == 0)
				miZeroLine(drawable, gc, mode, n, pt);
			else
				miWideLine(drawable, gc, mode, n, pt);
			break;
		case LineOnOffDash:
		case LineDoubleDash:
			miWideDash(drawable, gc, mode, n, pt);
			break;
		}
		return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	if (gc->lineWidth) {
		if (gc->lineStyle != LineSolid)
			miWideDash(drawable, gc, mode, n, pt);
		else
			miWideLine(drawable, gc, mode, n, pt);
		return;
	}

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	DBG(("%s: fbPolyLine\n", __FUNCTION__));
	fbPolyLine(drawable, gc, mode, n, pt);
}

static Bool
sna_poly_segment_blt(DrawablePtr drawable,
		     struct kgem_bo *bo,
		     struct sna_damage **damage,
		     GCPtr gc, int n, xSegment *seg,
		     const BoxRec *extents, unsigned clipped)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	BoxRec boxes[512], *b = boxes, * const last_box = boxes + ARRAY_SIZE(boxes);
	struct sna_fill_op fill;
	int16_t dx, dy;

	DBG(("%s: alu=%d, fg=%08lx\n", __FUNCTION__, gc->alu, gc->fgPixel));

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	if (!clipped) {
		dx += drawable->x;
		dy += drawable->y;
		if (dx|dy) {
			do {
				int nbox = n;
				if (nbox > ARRAY_SIZE(boxes))
					nbox = ARRAY_SIZE(boxes);
				n -= nbox;
				do {
					if (seg->x1 < seg->x2) {
						b->x1 = seg->x1;
						b->x2 = seg->x2;
					} else {
						b->x1 = seg->x2;
						b->x2 = seg->x1;
					}
					b->x2++;

					if (seg->y1 < seg->y2) {
						b->y1 = seg->y1;
						b->y2 = seg->y2;
					} else {
						b->y1 = seg->y2;
						b->y2 = seg->y1;
					}
					b->y2++;

					/* don't paint last pixel */
					if (gc->capStyle == CapNotLast) {
						if (seg->x1 == seg->x2)
							b->y2--;
						else
							b->x2--;
					}

					b->x1 += dx;
					b->x2 += dx;
					b->y1 += dy;
					b->y2 += dy;
					b++;
					seg++;
				} while (--nbox);

				fill.boxes(sna, &fill, boxes, b-boxes);
				if (damage)
					sna_damage_add_boxes(damage, boxes, b-boxes, 0, 0);
				b = boxes;
			} while (n);
		} else {
			do {
				int nbox = n;
				if (nbox > ARRAY_SIZE(boxes))
					nbox = ARRAY_SIZE(boxes);
				n -= nbox;
				do {
					if (seg->x1 < seg->x2) {
						b->x1 = seg->x1;
						b->x2 = seg->x2;
					} else {
						b->x1 = seg->x2;
						b->x2 = seg->x1;
					}
					b->x2++;

					if (seg->y1 < seg->y2) {
						b->y1 = seg->y1;
						b->y2 = seg->y2;
					} else {
						b->y1 = seg->y2;
						b->y2 = seg->y1;
					}
					b->y2++;

					/* don't paint last pixel */
					if (gc->capStyle == CapNotLast) {
						if (seg->x1 == seg->x2)
							b->y2--;
						else
							b->x2--;
					}

					b++;
					seg++;
				} while (--nbox);

				fill.boxes(sna, &fill, boxes, b-boxes);
				if (damage)
					sna_damage_add_boxes(damage, boxes, b-boxes, 0, 0);
				b = boxes;
			} while (n);
		}
	} else {
		RegionRec clip;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			goto done;

		if (clip.data) {
			const BoxRec * const clip_start = RegionBoxptr(&clip);
			const BoxRec * const clip_end = clip_start + clip.data->numRects;
			const BoxRec *c;
			do {
				int x, y, width, height;
				BoxRec box;

				if (seg->x1 < seg->x2) {
					x = seg->x1;
					width = seg->x2;
				} else {
					x = seg->x2;
					width = seg->x1;
				}
				width -= x - 1;

				if (seg->y1 < seg->y2) {
					y = seg->y1;
					height = seg->y2;
				} else {
					y = seg->y2;
					height = seg->y1;
				}
				height -= y - 1;

				/* don't paint last pixel */
				if (gc->capStyle == CapNotLast) {
					if (width == 1)
						height--;
					else
						width--;
				}

				DBG(("%s: [%d] (%d, %d)x(%d, %d) + (%d, %d)\n", __FUNCTION__, n,
				     x, y, width, height, dx+drawable->x, dy+drawable->y));
				box.x1 = x + drawable->x;
				box.x2 = box.x1 + width;
				box.y1 = y + drawable->y;
				box.y2 = box.y1 + height;
				c = find_clip_box_for_y(clip_start,
							clip_end,
							box.y1);
				while (c != clip_end) {
					if (box.y2 <= c->y1)
						break;

					*b = box;
					if (box_intersect(b, c++)) {
						b->x1 += dx;
						b->x2 += dx;
						b->y1 += dy;
						b->y2 += dy;
						if (++b == last_box) {
							fill.boxes(sna, &fill, boxes, last_box-boxes);
							if (damage)
								sna_damage_add_boxes(damage, boxes, last_box-boxes, 0, 0);
							b = boxes;
						}
					}
				}

				seg++;
			} while (--n);
		} else {
			do {
				int x, y, width, height;

				if (seg->x1 < seg->x2) {
					x = seg->x1;
					width = seg->x2;
				} else {
					x = seg->x2;
					width = seg->x1;
				}
				width -= x - 1;

				if (seg->y1 < seg->y2) {
					y = seg->y1;
					height = seg->y2;
				} else {
					y = seg->y2;
					height = seg->y1;
				}
				height -= y - 1;

				/* don't paint last pixel */
				if (gc->capStyle == CapNotLast) {
					if (width == 1)
						height--;
					else
						width--;
				}

				DBG(("%s: [%d] (%d, %d)x(%d, %d) + (%d, %d)\n", __FUNCTION__, n,
				     x, y, width, height, dx+drawable->x, dy+drawable->y));

				b->x1 = x + drawable->x;
				b->x2 = b->x1 + width;
				b->y1 = y + drawable->y;
				b->y2 = b->y1 + height;

				if (box_intersect(b, &clip.extents)) {
					b->x1 += dx;
					b->x2 += dx;
					b->y1 += dy;
					b->y2 += dy;
					if (++b == last_box) {
						fill.boxes(sna, &fill, boxes, last_box-boxes);
						if (damage)
							sna_damage_add_boxes(damage, boxes, last_box-boxes, 0, 0);
						b = boxes;
					}
				}

				seg++;
			} while (--n);
		}
		RegionUninit(&clip);
	}
	if (b != boxes) {
		fill.boxes(sna, &fill, boxes, b - boxes);
		if (damage)
			sna_damage_add_boxes(damage, boxes, b - boxes, 0, 0);
	}
done:
	fill.done(sna, &fill);
	return TRUE;
}

static bool
sna_poly_zero_segment_blt(DrawablePtr drawable,
			  struct kgem_bo *bo,
			  struct sna_damage **damage,
			  GCPtr gc, const int _n, const xSegment *_s,
			  const BoxRec *extents, unsigned clipped)
{
	static void * const _jump[] = {
		&&no_damage,
		&&damage,

		&&no_damage_offset,
		&&damage_offset,
	};

	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	unsigned int bias = miGetZeroLineBias(drawable->pScreen);
	struct sna_fill_op fill;
	RegionRec clip;
	const BoxRec *last_extents;
	BoxRec box[512], *b;
	BoxRec *const last_box = box + ARRAY_SIZE(box);
	int16_t dx, dy;
	void *jump, *ret;

	DBG(("%s: alu=%d, pixel=%lx, n=%d, clipped=%d, damage=%p\n",
	     __FUNCTION__, gc->alu, gc->fgPixel, _n, clipped, damage));
	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	region_set(&clip, extents);
	if (clipped) {
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			return TRUE;
	}
	DBG(("%s: [clipped] extents=(%d, %d), (%d, %d), delta=(%d, %d)\n",
	     __FUNCTION__,
	     clip.extents.x1, clip.extents.y1,
	     clip.extents.x2, clip.extents.y2,
	     dx, dy));

	jump = _jump[(damage != NULL) | !!(dx|dy) << 1];

	b = box;
	extents = REGION_RECTS(&clip);
	last_extents = extents + REGION_NUM_RECTS(&clip);
	do {
		int n = _n;
		const xSegment *s = _s;
		do {
			int16_t sdx, sdy;
			int16_t adx, ady;
			int16_t e, e1, e2, e3;
			int16_t length;
			int x1, x2;
			int y1, y2;
			int oc1, oc2;
			int octant;

			x1 = s->x1 + drawable->x;
			y1 = s->y1 + drawable->y;
			x2 = s->x2 + drawable->x;
			y2 = s->y2 + drawable->y;
			s++;

			DBG(("%s: segment (%d, %d) to (%d, %d)\n",
			     __FUNCTION__, x1, y1, x2, y2));
			if (x2 == x1 && y2 == y1)
				continue;

			oc1 = 0;
			MIOUTCODES(oc1, x1, y1,
				   extents->x1,
				   extents->y1,
				   extents->x2,
				   extents->y2);
			oc2 = 0;
			MIOUTCODES(oc2, x2, y2,
				   extents->x1,
				   extents->y1,
				   extents->x2,
				   extents->y2);
			if (oc1 & oc2)
				continue;

			CalcLineDeltas(x1, y1, x2, y2,
				       adx, ady, sdx, sdy,
				       1, 1, octant);

			DBG(("%s: adx=(%d, %d), sdx=(%d, %d)\n",
			     __FUNCTION__, adx, ady, sdx, sdy));
			if (adx == 0 || ady == 0) {
				if (x1 <= x2) {
					b->x1 = x1;
					b->x2 = x2;
				} else {
					b->x1 = x2;
					b->x2 = x1;
				}
				if (y1 <= y2) {
					b->y1 = y1;
					b->y2 = y2;
				} else {
					b->y1 = y2;
					b->y2 = y1;
				}
				b->x2++;
				b->y2++;
				if (oc1 | oc2)
					box_intersect(b, extents);
				if (++b == last_box) {
					ret = &&rectangle_continue;
					goto *jump;
rectangle_continue:
					b = box;
				}
			} else if (adx >= ady) {
				/* X-major segment */
				e1 = ady << 1;
				e2 = e1 - (adx << 1);
				e  = e1 - adx;
				length = adx;	/* don't draw endpoint in main loop */

				FIXUP_ERROR(e, octant, bias);

				if (oc1 | oc2) {
					int pt1_clipped, pt2_clipped;
					int x = x1, y = y1;

					if (miZeroClipLine(extents->x1,
							   extents->y1,
							   extents->x2,
							   extents->y2,
							   &x1, &y1, &x2, &y2,
							   adx, ady,
							   &pt1_clipped, &pt2_clipped,
							   octant, bias, oc1, oc2) == -1)
						continue;

					length = abs(x2 - x1);

					/* if we've clipped the endpoint, always draw the full length
					 * of the segment, because then the capstyle doesn't matter
					 */
					if (pt2_clipped)
						length++;

					if (pt1_clipped) {
						int clipdx = abs(x1 - x);
						int clipdy = abs(y1 - y);
						e += clipdy * e2 + (clipdx - clipdy) * e1;
					}
				}
				if (length == 0)
					continue;

				e3 = e2 - e1;
				e  = e - e1;

				b->x1 = x1;
				b->y2 = b->y1 = y1;
				while (--length) {
					e += e1;
					if (e >= 0) {
						b->x2 = x1;
						if (b->x2 < b->x1) {
							int16_t t = b->x1;
							b->x1 = b->x2;
							b->x2 = t;
						}
						b->x2++;
						b->y2++;
						if (++b == last_box) {
							ret = &&X_continue;
							goto *jump;
X_continue:
							b = box;
						}
						y1 += sdy;
						e += e3;
						b->y2 = b->y1 = y1;
						b->x1 = x1 + sdx;
					}
					x1 += sdx;
				}

				b->x2 = x1;
				if (b->x2 < b->x1) {
					int16_t t = b->x1;
					b->x1 = b->x2;
					b->x2 = t;
				}
				if (gc->capStyle != CapNotLast)
					b->x2++;
				b->y2++;
				if (++b == last_box) {
					ret = &&X_continue2;
					goto *jump;
X_continue2:
					b = box;
				}
			} else {
				/* Y-major segment */
				e1 = adx << 1;
				e2 = e1 - (ady << 1);
				e  = e1 - ady;
				length  = ady;	/* don't draw endpoint in main loop */

				SetYMajorOctant(octant);
				FIXUP_ERROR(e, octant, bias);

				if (oc1 | oc2) {
					int pt1_clipped, pt2_clipped;
					int x = x1, y = y1;

					if (miZeroClipLine(extents->x1,
							   extents->y1,
							   extents->x2,
							   extents->y2,
							   &x1, &y1, &x2, &y2,
							   adx, ady,
							   &pt1_clipped, &pt2_clipped,
							   octant, bias, oc1, oc2) == -1)
						continue;

					length = abs(y2 - y1);

					/* if we've clipped the endpoint, always draw the full length
					 * of the segment, because then the capstyle doesn't matter
					 */
					if (pt2_clipped)
						length++;

					if (pt1_clipped) {
						int clipdx = abs(x1 - x);
						int clipdy = abs(y1 - y);
						e += clipdx * e2 + (clipdy - clipdx) * e1;
					}
				}
				if (length == 0)
					continue;

				e3 = e2 - e1;
				e  = e - e1;

				b->x2 = b->x1 = x1;
				b->y1 = y1;
				while (--length) {
					e += e1;
					if (e >= 0) {
						b->y2 = y1;
						if (b->y2 < b->y1) {
							int16_t t = b->y1;
							b->y1 = b->y2;
							b->y2 = t;
						}
						b->x2++;
						b->y2++;
						if (++b == last_box) {
							ret = &&Y_continue;
							goto *jump;
Y_continue:
							b = box;
						}
						x1 += sdx;
						e += e3;
						b->x2 = b->x1 = x1;
						b->y1 = y1 + sdy;
					}
					y1 += sdy;
				}

				b->y2 = y1;
				if (b->y2 < b->y1) {
					int16_t t = b->y1;
					b->y1 = b->y2;
					b->y2 = t;
				}
				b->x2++;
				if (gc->capStyle != CapNotLast)
					b->y2++;
				if (++b == last_box) {
					ret = &&Y_continue2;
					goto *jump;
Y_continue2:
					b = box;
				}
			}
		} while (--n);
	} while (++extents != last_extents);

	if (b != box) {
		ret = &&done;
		goto *jump;
	}

done:
	fill.done(sna, &fill);
	return true;

damage:
	sna_damage_add_boxes(damage, box, b-box, 0, 0);
no_damage:
	fill.boxes(sna, &fill, box, b-box);
	goto *ret;

no_damage_offset:
	{
		BoxRec *bb = box;
		do {
			bb->x1 += dx;
			bb->x2 += dx;
			bb->y1 += dy;
			bb->y2 += dy;
		} while (++bb != b);
		fill.boxes(sna, &fill, box, b - box);
	}
	goto *ret;

damage_offset:
	{
		BoxRec *bb = box;
		do {
			bb->x1 += dx;
			bb->x2 += dx;
			bb->y1 += dy;
			bb->y2 += dy;
		} while (++bb != b);
		fill.boxes(sna, &fill, box, b - box);
		sna_damage_add_boxes(damage, box, b - box, 0, 0);
	}
	goto *ret;
}

static unsigned
sna_poly_segment_extents(DrawablePtr drawable, GCPtr gc,
			 int n, xSegment *seg,
			 BoxPtr out)
{
	BoxRec box;
	int extra = gc->lineWidth;
	bool clipped, can_blit;

	if (n == 0)
		return 0;

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

	can_blit = seg->x1 == seg->x2 || seg->y1 == seg->y2;
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

		if (can_blit && !(seg->x1 == seg->x2 || seg->y1 == seg->y2))
			can_blit = false;
	}

	box.x2++;
	box.y2++;

	if (extra) {
		box.x1 -= extra;
		box.x2 += extra;
		box.y1 -= extra;
		box.y2 += extra;
	}

	clipped = trim_and_translate_box(&box, drawable, gc);
	if (box_empty(&box))
		return 0;
	*out = box;
	return 1 | clipped << 1 | can_blit << 2;
}

static void
sna_poly_segment(DrawablePtr drawable, GCPtr gc, int n, xSegment *seg)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	RegionRec region;
	unsigned flags;

	DBG(("%s(n=%d, first=((%d, %d), (%d, %d)), lineWidth=%d\n",
	     __FUNCTION__,
	     n, seg->x1, seg->y1, seg->x2, seg->y2,
	     gc->lineWidth));

	flags = sna_poly_segment_extents(drawable, gc, n, seg, &region.extents);
	if (flags == 0)
		return;

	DBG(("%s: extents=(%d, %d), (%d, %d)\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	DBG(("%s: fill=%d [%d], line=%d [%d], width=%d, mask=%lu [%d], rectlinear=%d\n",
	     __FUNCTION__,
	     gc->fillStyle, gc->fillStyle == FillSolid,
	     gc->lineStyle, gc->lineStyle == LineSolid,
	     gc->lineWidth,
	     gc->planemask, PM_IS_SOLID(drawable, gc->planemask),
	     flags & 4));
	if (gc->fillStyle == FillSolid &&
	    gc->lineStyle == LineSolid &&
	    gc->lineWidth <= 1 &&
	    PM_IS_SOLID(drawable, gc->planemask)) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);

		DBG(("%s: trying blt solid fill [%08lx] paths\n",
		     __FUNCTION__, gc->fgPixel));

	    if (flags & 4) {
		if (sna_drawable_use_gpu_bo(drawable, &region.extents) &&
		    sna_poly_segment_blt(drawable,
					 priv->gpu_bo,
					 priv->gpu_only ? NULL : reduce_damage(drawable, &priv->gpu_damage, &region.extents),
					 gc, n, seg,
					 &region.extents, flags & 2))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &region.extents) &&
		    sna_poly_segment_blt(drawable,
					 priv->cpu_bo,
					 reduce_damage(drawable, &priv->cpu_damage, &region.extents),
					 gc, n, seg,
					 &region.extents, flags & 2))
			return;
	    } else {
		    if (USE_ZERO_SPANS &&
			sna_drawable_use_gpu_bo(drawable, &region.extents) &&
			sna_poly_zero_segment_blt(drawable,
						  priv->gpu_bo,
						  priv->gpu_only ? NULL : reduce_damage(drawable, &priv->gpu_damage, &region.extents),
						  gc, n, seg, &region.extents, flags & 2))
			    return;
	    }
	}

	/* XXX Do we really want to base this decision on the amalgam ? */
	if (USE_SPANS && can_fill_spans(drawable, gc) &&
	    sna_drawable_use_gpu_bo(drawable, &region.extents)) {
		void (*line)(DrawablePtr, GCPtr, int, int, DDXPointPtr);
		int i;

		DBG(("%s: converting segments into spans\n", __FUNCTION__));

		switch (gc->lineStyle) {
		default:
		case LineSolid:
			if (gc->lineWidth == 0)
				line = miZeroLine;
			else
				line = miWideLine;
			break;
		case LineOnOffDash:
		case LineDoubleDash:
			line = miWideDash;
			break;
		}

		for (i = 0; i < n; i++)
			line(drawable, gc, CoordModeOrigin, 2,
			     (DDXPointPtr)&seg[i]);
		return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	if (gc->lineWidth) {
		miPolySegment(drawable, gc, n, seg);
		return;
	}

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	DBG(("%s: fbPolySegment\n", __FUNCTION__));
	fbPolySegment(drawable, gc, n, seg);
}

static unsigned
sna_poly_rectangle_extents(DrawablePtr drawable, GCPtr gc,
			   int n, xRectangle *r,
			   BoxPtr out)
{
	Box32Rec box;
	int extra = gc->lineWidth >> 1;
	bool clipped;

	if (n == 0)
		return 0;

	box.x1 = r->x;
	box.y1 = r->y;
	box.x2 = box.x1 + r->width;
	box.y2 = box.y1 + r->height;

	while (--n)
		box32_add_rect(&box, r++);

	box.x2++;
	box.y2++;

	if (extra) {
		box.x1 -= extra;
		box.x2 += extra;
		box.y1 -= extra;
		box.y2 += extra;
	}

	clipped = box32_trim_and_translate(&box, drawable, gc);
	if (!box32_to_box16(&box, out))
		return 0;

	return 1 | clipped << 1;
}

static Bool
sna_poly_rectangle_blt(DrawablePtr drawable,
		       struct kgem_bo *bo,
		       struct sna_damage **damage,
		       GCPtr gc, int n, xRectangle *r,
		       const BoxRec *extents, bool clipped)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_fill_op fill;
	BoxRec boxes[512], *b = boxes, *const last_box = boxes+ARRAY_SIZE(boxes);
	int16_t dx, dy;
	static void * const jump[] = {
		&&zero,
		&&zero_clipped,
		&&wide,
		&&wide_clipped,
	};
	unsigned v;

	DBG(("%s: alu=%d, width=%d, fg=%08lx, damge=%p, clipped?=%d\n",
	     __FUNCTION__, gc->alu, gc->lineWidth, gc->fgPixel, damage, clipped));

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	v = !!clipped;
	v |= (gc->lineWidth <= 1) << 1;
	goto *jump[v];

zero:
	dx += drawable->x;
	dy += drawable->y;

	do {
		xRectangle rr = *r++;
		rr.x += dx;
		rr.y += dy;

		if (b+4 > last_box) {
			fill.boxes(sna, &fill, boxes, b-boxes);
			if (damage)
				sna_damage_add_boxes(damage, boxes, b-boxes, 0, 0);
			b = boxes;
		}

		if (rr.width <= 2 || rr.height <= 2) {
			b->x1 = rr.x;
			b->y1 = rr.y;
			b->x2 = rr.x + rr.width + 1;
			b->y2 = rr.y + rr.height + 1;
			DBG(("%s: blt (%d, %d), (%d, %d)\n",
			     __FUNCTION__,
			     b->x1, b->y1, b->x2,b->y2));
			b++;
		} else {
			b[0].x1 = rr.x;
			b[0].y1 = rr.y;
			b[0].x2 = rr.x + rr.width + 1;
			b[0].y2 = rr.y + 1;

			b[1] = b[0];
			b[1].y1 += rr.height;
			b[1].y2 += rr.height;

			b[2].y1 = rr.y + 1;
			b[2].y2 = rr.y + rr.height - 1;
			b[2].x1 = rr.x;
			b[2].x2 = rr.x + 1;

			b[3] = b[2];
			b[3].x1 += rr.width;
			b[3].x2 += rr.width;

			b += 4;
		}
	} while (--n);
	goto done;

zero_clipped:
	{
		RegionRec clip;
		BoxRec box[4];
		int count;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			goto done;

		if (clip.data) {
			const BoxRec * const clip_start = RegionBoxptr(&clip);
			const BoxRec * const clip_end = clip_start + clip.data->numRects;
			const BoxRec *c;
			do {
				xRectangle rr = *r++;
				rr.x += drawable->x;
				rr.y += drawable->y;

				if (rr.width <= 2 || rr.height <= 2) {
					box[0].x1 = rr.x;
					box[0].y1 = rr.y;
					box[0].x2 = rr.x + rr.width + 1;
					box[0].y2 = rr.y + rr.height + 1;
					count = 1;
				} else {
					box[0].x1 = rr.x;
					box[0].y1 = rr.y;
					box[0].x2 = rr.x + rr.width + 1;
					box[0].y2 = rr.y + 1;

					box[1] = box[0];
					box[1].y1 += rr.height;
					box[1].y2 += rr.height;

					box[2].y1 = rr.y + 1;
					box[2].y2 = box[2].y1 + rr.height - 1;
					box[2].x1 = rr.x;
					box[2].x2 = rr.x + 1;

					box[3] = box[2];
					box[3].x1 += rr.width;
					box[3].x2 += rr.width;
					count = 4;
				}

				while (count--) {
					c = find_clip_box_for_y(clip_start,
								clip_end,
								box[count].y1);
					while (c != clip_end) {
						if (box[count].y2 <= c->y1)
							break;

						*b = box[count];
						if (box_intersect(b, c++)) {
							b->x1 += dx;
							b->x2 += dx;
							b->y1 += dy;
							b->y2 += dy;
							if (++b == last_box) {
								fill.boxes(sna, &fill, boxes, last_box-boxes);
								if (damage)
									sna_damage_add_boxes(damage, boxes, b-boxes, 0, 0);
								b = boxes;
							}
						}

					}
				}
			} while (--n);
		} else {
			do {
				xRectangle rr = *r++;
				rr.x += drawable->x;
				rr.y += drawable->y;

				if (rr.width <= 2 || rr.height <= 2) {
					box[0].x1 = rr.x;
					box[0].y1 = rr.y;
					box[0].x2 = rr.x + rr.width + 1;
					box[0].y2 = rr.y + rr.height + 1;
					count = 1;
				} else {
					box[0].x1 = rr.x;
					box[0].y1 = rr.y;
					box[0].x2 = rr.x + rr.width + 1;
					box[0].y2 = rr.y + 1;

					box[1] = box[0];
					box[1].y1 += rr.height;
					box[1].y2 += rr.height;

					box[2].y1 = rr.y + 1;
					box[2].y2 = box[2].y1 + rr.height - 1;
					box[2].x1 = rr.x;
					box[2].x2 = rr.x + 1;

					box[3] = box[2];
					box[3].x1 += rr.width;
					box[3].x2 += rr.width;
					count = 4;
				}

				while (count--) {
					*b = box[count];
					if (box_intersect(b, &clip.extents)) {
						b->x1 += dx;
						b->x2 += dx;
						b->y1 += dy;
						b->y2 += dy;
						if (++b == last_box) {
							fill.boxes(sna, &fill, boxes, last_box-boxes);
							if (damage)
								sna_damage_add_boxes(damage, boxes, b-boxes, 0, 0);
							b = boxes;
						}
					}

				}
			} while (--n);
		}
	}
	goto done;

wide_clipped:
	{
		RegionRec clip;
		BoxRec box[4];
		int16_t offset2 = gc->lineWidth;
		int16_t offset1 = offset2 >> 1;
		int16_t offset3 = offset2 - offset1;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			goto done;

		if (clip.data) {
			const BoxRec * const clip_start = RegionBoxptr(&clip);
			const BoxRec * const clip_end = clip_start + clip.data->numRects;
			const BoxRec *c;
			do {
				xRectangle rr = *r++;
				int count;
				rr.x += drawable->x;
				rr.y += drawable->y;

				if (rr.height <= offset2 || rr.width <= offset2) {
					if (rr.height == 0) {
						box[0].x1 = rr.x;
						box[0].x2 = rr.x + rr.width + 1;
					} else {
						box[0].x1 = rr.x - offset1;
						box[0].x2 = box[0].x1 + rr.width + offset2;
					}
					if (rr.width == 0) {
						box[0].y1 = rr.y;
						box[0].y2 = rr.y + rr.height + 1;
					} else {
						box[0].y1 = rr.y - offset1;
						box[0].y2 = box[0].y1 + rr.height + offset2;
					}
					count = 1;
				} else {
					box[0].x1 = rr.x - offset1;
					box[0].x2 = box[0].x1 + rr.width + offset2;
					box[0].y1 = rr.y - offset1;
					box[0].y2 = box[0].y1 + offset2;

					box[1].x1 = rr.x - offset1;
					box[1].x2 = box[1].x1 + offset2;
					box[1].y1 = rr.y + offset3;
					box[1].y2 = rr.y + rr.height - offset1;

					box[2].x1 = rr.x + rr.width - offset1;
					box[2].x2 = box[2].x1 + offset2;
					box[2].y1 = rr.y + offset3;
					box[2].y2 = rr.y + rr.height - offset1;

					box[3] = box[1];
					box[3].y1 += rr.height;
					box[3].y2 += rr.height;
					count = 4;
				}

				while (count--) {
					c = find_clip_box_for_y(clip_start,
								clip_end,
								box[count].y1);
					while (c != clip_end) {
						if (box[count].y2 <= c->y1)
							break;

						*b = box[count];
						if (box_intersect(b, c++)) {
							b->x1 += dx;
							b->x2 += dx;
							b->y1 += dy;
							b->y2 += dy;
							if (++b == last_box) {
								fill.boxes(sna, &fill, boxes, last_box-boxes);
								if (damage)
									sna_damage_add_boxes(damage, boxes, b-boxes, 0, 0);
								b = boxes;
							}
						}
					}
				}
			} while (--n);
		} else {
			do {
				xRectangle rr = *r++;
				int count;
				rr.x += drawable->x;
				rr.y += drawable->y;

				if (rr.height <= offset2 || rr.width <= offset2) {
					if (rr.height == 0) {
						box[0].x1 = rr.x;
						box[0].x2 = rr.x + rr.width + 1;
					} else {
						box[0].x1 = rr.x - offset1;
						box[0].x2 = box[0].x1 + rr.width + offset2;
					}
					if (rr.width == 0) {
						box[0].y1 = rr.y;
						box[0].y2 = rr.y + rr.height + 1;
					} else {
						box[0].y1 = rr.y - offset1;
						box[0].y2 = box[0].y1 + rr.height + offset2;
					}
					count = 1;
				} else {
					box[0].x1 = rr.x - offset1;
					box[0].x2 = box[0].x1 + rr.width + offset2;
					box[0].y1 = rr.y - offset1;
					box[0].y2 = box[0].y1 + offset2;

					box[1].x1 = rr.x - offset1;
					box[1].x2 = box[1].x1 + offset2;
					box[1].y1 = rr.y + offset3;
					box[1].y2 = rr.y + rr.height - offset1;

					box[2].x1 = rr.x + rr.width - offset1;
					box[2].x2 = box[2].x1 + offset2;
					box[2].y1 = rr.y + offset3;
					box[2].y2 = rr.y + rr.height - offset1;

					box[3] = box[1];
					box[3].y1 += rr.height;
					box[3].y2 += rr.height;
					count = 4;
				}

				while (count--) {
					*b = box[count];
					if (box_intersect(b, &clip.extents)) {
						b->x1 += dx;
						b->x2 += dx;
						b->y1 += dy;
						b->y2 += dy;
						if (++b == last_box) {
							fill.boxes(sna, &fill, boxes, last_box-boxes);
							if (damage)
								sna_damage_add_boxes(damage, boxes, b-boxes, 0, 0);
							b = boxes;
						}
					}
				}
			} while (--n);
		}
	}
	goto done;

wide:
	{
		int offset2 = gc->lineWidth;
		int offset1 = offset2 >> 1;
		int offset3 = offset2 - offset1;

		dx += drawable->x;
		dy += drawable->y;

		do {
			xRectangle rr = *r++;
			rr.x += dx;
			rr.y += dy;

			if (b+4 > last_box) {
				fill.boxes(sna, &fill, boxes, last_box-boxes);
				if (damage)
					sna_damage_add_boxes(damage, boxes, last_box-boxes, 0, 0);
				b = boxes;
			}

			if (rr.height <= offset2 || rr.width <= offset2) {
				if (rr.height == 0) {
					b->x1 = rr.x;
					b->x2 = rr.x + rr.width + 1;
				} else {
					b->x1 = rr.x - offset1;
					b->x2 = rr.x + rr.width + offset2;
				}
				if (rr.width == 0) {
					b->y1 = rr.y;
					b->y2 = rr.y + rr.height + 1;
				} else {
					b->y1 = rr.y - offset1;
					b->y2 = rr.y + rr.height + offset2;
				}
				b++;
			} else {
				b[0].x1 = rr.x - offset1;
				b[0].x2 = b[0].x1 + rr.width + offset2;
				b[0].y1 = rr.y - offset1;
				b[0].y2 = b[0].y1 + offset2;

				b[1] = b[0];
				b[1].y1 = rr.y + rr.height - offset1;
				b[1].y2 = b[1].y1 + offset2;

				b[2].x1 = rr.x - offset1;
				b[2].x2 = b[2].x1 + offset2;
				b[2].y1 = rr.y + offset3;
				b[2].y2 = rr.y + rr.height - offset1;

				b[3] = b[2];
				b[3].x1 = rr.x + rr.width - offset1;
				b[3].x2 = b[3].x1 + offset2;
				b += 4;
			}
		} while (--n);
	}
	goto done;

done:
	if (b != boxes) {
		fill.boxes(sna, &fill, boxes, b-boxes);
		if (damage)
			sna_damage_add_boxes(damage, boxes, b-boxes, 0, 0);
	}
	fill.done(sna, &fill);
	return TRUE;
}

static void
sna_poly_rectangle(DrawablePtr drawable, GCPtr gc, int n, xRectangle *r)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	RegionRec region;
	unsigned flags;

	DBG(("%s(n=%d, first=((%d, %d)x(%d, %d)), lineWidth=%d\n",
	     __FUNCTION__,
	     n, r->x, r->y, r->width, r->height,
	     gc->lineWidth));

	flags = sna_poly_rectangle_extents(drawable, gc, n, r, &region.extents);
	if (flags == 0)
		return;

	DBG(("%s: extents=(%d, %d), (%d, %d), flags=%x\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2,
	     flags));

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	DBG(("%s: line=%d [%d], join=%d [%d], mask=%lu [%d]\n",
	     __FUNCTION__,
	     gc->lineStyle, gc->lineStyle == LineSolid,
	     gc->joinStyle, gc->joinStyle == JoinMiter,
	     gc->planemask, PM_IS_SOLID(drawable, gc->planemask)));
	if (gc->lineStyle == LineSolid &&
	    gc->joinStyle == JoinMiter &&
	    PM_IS_SOLID(drawable, gc->planemask)) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(drawable);

		DBG(("%s: trying blt solid fill [%08lx] paths\n",
		     __FUNCTION__, gc->fgPixel));

		if (sna_drawable_use_gpu_bo(drawable, &region.extents) &&
		    sna_poly_rectangle_blt(drawable, priv->gpu_bo,
					   priv->gpu_only ? NULL : reduce_damage(drawable, &priv->gpu_damage, &region.extents),
					   gc, n, r, &region.extents, flags&2))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &region.extents) &&
		    sna_poly_rectangle_blt(drawable, priv->cpu_bo,
					   reduce_damage(drawable, &priv->cpu_damage, &region.extents),
					   gc, n, r, &region.extents, flags&2))
			return;
	}

	/* Not a trivial outline, but we still maybe able to break it
	 * down into simpler operations that we can accelerate.
	 */
	if (sna_drawable_use_gpu_bo(drawable, &region.extents)) {
		miPolyRectangle(drawable, gc, n, r);
		return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	DBG(("%s: fbPolyRectangle\n", __FUNCTION__));
	fbPolyRectangle(drawable, gc, n, r);
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

static bool
arc_to_spans(GCPtr gc, int n)
{
	if (gc->lineStyle != LineSolid)
		return false;

	if (gc->lineWidth == 0)
		return true;

	if (n == 1)
		return true;

	return false;
}

static void
sna_poly_arc(DrawablePtr drawable, GCPtr gc, int n, xArc *arc)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	RegionRec region;

	DBG(("%s(n=%d, lineWidth=%d\n", __FUNCTION__, n, gc->lineWidth));

	if (sna_poly_arc_extents(drawable, gc, n, arc, &region.extents))
		return;

	DBG(("%s: extents=(%d, %d), (%d, %d)\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	/* For "simple" cases use the miPolyArc to spans path */
	if (USE_SPANS && arc_to_spans(gc, n) && can_fill_spans(drawable, gc) &&
	    sna_drawable_use_gpu_bo(drawable, &region.extents)) {
		DBG(("%s: converting arcs into spans\n", __FUNCTION__));
		/* XXX still around 10x slower for x11perf -ellipse */
		if (gc->lineWidth == 0)
			miZeroPolyArc(drawable, gc, n, arc);
		else
			miPolyArc(drawable, gc, n, arc);
		return;
	}

fallback:
	DBG(("%s -- fallback\n", __FUNCTION__));
	if (gc->lineWidth) {
		DBG(("%s -- miPolyArc\n", __FUNCTION__));
		miPolyArc(drawable, gc, n, arc);
		return;
	}

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	/* XXX may still fallthrough to miZeroPolyArc */
	DBG(("%s -- fbPolyArc\n", __FUNCTION__));
	fbPolyArc(drawable, gc, n, arc);
}

static Bool
sna_poly_fill_rect_blt(DrawablePtr drawable,
		       struct kgem_bo *bo,
		       struct sna_damage **damage,
		       GCPtr gc, uint32_t pixel,
		       int n, xRectangle *rect,
		       const BoxRec *extents,
		       bool clipped)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_fill_op fill;
	BoxRec boxes[512], *b = boxes, *const last_box = boxes+ARRAY_SIZE(boxes);
	int16_t dx, dy;

	DBG(("%s x %d [(%d, %d)+(%d, %d)...], clipped?=%d\n",
	     __FUNCTION__,
	     n, rect->x, rect->y, rect->width, rect->height,
	     clipped));

	if (n == 1 && gc->pCompositeClip->data == NULL) {
		BoxRec r;
		bool success = true;

		r.x1 = rect->x + drawable->x;
		r.y1 = rect->y + drawable->y;
		r.x2 = bound(r.x1, rect->width);
		r.y2 = bound(r.y1, rect->height);
		if (box_intersect(&r, &gc->pCompositeClip->extents)) {
			get_drawable_deltas(drawable, pixmap, &dx, &dy);
			r.x1 += dx; r.y1 += dy;
			r.x2 += dx; r.y2 += dy;
			if (sna->render.fill_one(sna, pixmap, bo, pixel,
						 r.x1, r.y1, r.x2, r.y2,
						 gc->alu)) {
				if (damage) {
					assert_pixmap_contains_box(pixmap, &r);
					sna_damage_add_box(damage, &r);
				}
			} else
				success = false;
		}

		return success;
	}

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, pixel)) {
		DBG(("%s: unsupported blt\n", __FUNCTION__));
		return FALSE;
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	if (!clipped) {
		dx += drawable->x;
		dy += drawable->y;

		sna_damage_add_rectangles(damage, rect, n, dx, dy);
		if (dx|dy) {
			do {
				int nbox = n;
				if (nbox > ARRAY_SIZE(boxes))
					nbox = ARRAY_SIZE(boxes);
				n -= nbox;
				do {
					b->x1 = rect->x + dx;
					b->y1 = rect->y + dy;
					b->x2 = b->x1 + rect->width;
					b->y2 = b->y1 + rect->height;
					b++;
					rect++;
				} while (--nbox);
				fill.boxes(sna, &fill, boxes, b-boxes);
				b = boxes;
			} while (n);
		} else {
			do {
				int nbox = n;
				if (nbox > ARRAY_SIZE(boxes))
					nbox = ARRAY_SIZE(boxes);
				n -= nbox;
				do {
					b->x1 = rect->x;
					b->y1 = rect->y;
					b->x2 = b->x1 + rect->width;
					b->y2 = b->y1 + rect->height;
					b++;
					rect++;
				} while (--nbox);
				fill.boxes(sna, &fill, boxes, b-boxes);
				b = boxes;
			} while (n);
		}
	} else {
		RegionRec clip;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			goto done;

		if (clip.data == NULL) {
			do {
				b->x1 = rect->x + drawable->x;
				b->y1 = rect->y + drawable->y;
				b->x2 = bound(b->x1, rect->width);
				b->y2 = bound(b->y1, rect->height);
				rect++;

				if (box_intersect(b, &clip.extents)) {
					b->x1 += dx;
					b->x2 += dx;
					b->y1 += dy;
					b->y2 += dy;
					if (++b == last_box) {
						fill.boxes(sna, &fill, boxes, last_box-boxes);
						if (damage)
							sna_damage_add_boxes(damage, boxes, last_box-boxes, 0, 0);
						b = boxes;
					}
				}
			} while (--n);
		} else {
			const BoxRec * const clip_start = RegionBoxptr(&clip);
			const BoxRec * const clip_end = clip_start + clip.data->numRects;
			const BoxRec *c;

			do {
				BoxRec box;

				box.x1 = rect->x + drawable->x;
				box.y1 = rect->y + drawable->y;
				box.x2 = bound(box.x1, rect->width);
				box.y2 = bound(box.y1, rect->height);
				rect++;

				c = find_clip_box_for_y(clip_start,
							clip_end,
							box.y1);
				while (c != clip_end) {
					if (box.y2 <= c->y1)
						break;

					*b = box;
					if (box_intersect(b, c++)) {
						b->x1 += dx;
						b->x2 += dx;
						b->y1 += dy;
						b->y2 += dy;
						if (++b == last_box) {
							fill.boxes(sna, &fill, boxes, last_box-boxes);
							if (damage)
								sna_damage_add_boxes(damage, boxes, last_box-boxes, 0, 0);
							b = boxes;
						}
					}

				}
			} while (--n);
		}

		RegionUninit(&clip);
		if (b != boxes)
			fill.boxes(sna, &fill, boxes, b-boxes);
	}
done:
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
			 GCPtr gc, int n, xRectangle *rect,
			 const BoxRec *extents, unsigned clipped)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	PixmapPtr tile = gc->tile.pixmap;
	const DDXPointRec * const origin = &gc->patOrg;
	struct sna_copy_op copy;
	CARD32 alu = gc->alu;
	int tile_width, tile_height;
	int16_t dx, dy;

	DBG(("%s x %d [(%d, %d)+(%d, %d)...]\n",
	     __FUNCTION__, n, rect->x, rect->y, rect->width, rect->height));

	tile_width = tile->drawable.width;
	tile_height = tile->drawable.height;
	if (tile_width == 1 && tile_height == 1)
		return sna_poly_fill_rect_blt(drawable, bo, damage,
					      gc, get_pixel(tile),
					      n, rect,
					      extents, clipped);

	if (!sna_pixmap_move_to_gpu(tile))
		return FALSE;

	if (!sna_copy_init_blt(&copy, sna,
			       tile, sna_pixmap_get_bo(tile),
			       pixmap, bo,
			       alu)) {
		DBG(("%s: unsupported blt\n", __FUNCTION__));
		return FALSE;
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	if (!clipped) {
		dx += drawable->x;
		dy += drawable->y;

		sna_damage_add_rectangles(damage, rect, n, dx, dy);
		do {
			xRectangle r = *rect++;
			int16_t tile_y = (r.y - origin->y) % tile_height;

			r.y += dy;
			do {
				int16_t width = r.width;
				int16_t x = r.x + dx;
				int16_t tile_x = (r.x - origin->x) % tile_width;
				int16_t h = tile_height - tile_y;
				if (h > r.height)
					h = r.height;
				r.height -= h;

				do {
					int16_t w = tile_width - tile_x;
					if (w > width)
						w = width;
					width -= w;

					copy.blt(sna, &copy,
						 tile_x, tile_y,
						 w, h,
						 x, r.y);

					x += w;
					tile_x = 0;
				} while (width);
				r.y += h;
				tile_y = 0;
			} while (r.height);
		} while (--n);
	} else {
		RegionRec clip;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			goto done;

		if (clip.data == NULL) {
			const BoxRec *box = &clip.extents;
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
				BoxRec *box;
				int nbox;

				region.extents.x1 = rect->x + drawable->x;
				region.extents.y1 = rect->y + drawable->y;
				region.extents.x2 = bound(region.extents.x1, rect->width);
				region.extents.y2 = bound(region.extents.y1, rect->height);
				rect++;

				region.data = NULL;
				RegionIntersect(&region, &region, &clip);

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

		RegionUninit(&clip);
	}
done:
	copy.done(sna, &copy);
	return TRUE;
}

static unsigned
sna_poly_fill_rect_extents(DrawablePtr drawable, GCPtr gc,
			   int n, xRectangle *rect,
			   BoxPtr out)
{
	Box32Rec box;
	bool clipped;

	if (n == 0)
		return 0;

	DBG(("%s: [0] = (%d, %d)x(%d, %d)\n",
	     __FUNCTION__, rect->x, rect->y, rect->width, rect->height));
	box.x1 = rect->x;
	box.x2 = box.x1 + rect->width;
	box.y1 = rect->y;
	box.y2 = box.y1 + rect->height;
	while (--n)
		box32_add_rect(&box, ++rect);

	clipped = box32_trim_and_translate(&box, drawable, gc);
	if (!box32_to_box16(&box, out))
		return 0;

	return 1 | clipped << 1;
}

static void
sna_poly_fill_rect(DrawablePtr draw, GCPtr gc, int n, xRectangle *rect)
{
	struct sna *sna = to_sna_from_drawable(draw);
	RegionRec region;
	unsigned flags;

	DBG(("%s(n=%d, PlaneMask: %lx (solid %d), solid fill: %d [style=%d, tileIsPixel=%d], alu=%d)\n", __FUNCTION__,
	     n, gc->planemask, !!PM_IS_SOLID(draw, gc->planemask),
	     (gc->fillStyle == FillSolid ||
	      (gc->fillStyle == FillTiled && gc->tileIsPixel)),
	     gc->fillStyle, gc->tileIsPixel,
	     gc->alu));

	flags = sna_poly_fill_rect_extents(draw, gc, n, rect, &region.extents);
	if (flags == 0) {
		DBG(("%s, nothing to do\n", __FUNCTION__));
		return;
	}

	DBG(("%s: extents(%d, %d), (%d, %d), flags=%x\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2,
	     flags));

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (!PM_IS_SOLID(draw, gc->planemask))
		goto fallback;

	if (gc->fillStyle == FillSolid ||
	    (gc->fillStyle == FillTiled && gc->tileIsPixel)) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(draw);
		uint32_t color = gc->fillStyle == FillSolid ? gc->fgPixel : gc->tile.pixel;

		DBG(("%s: solid fill [%08lx], testing for blt\n",
		     __FUNCTION__,
		     gc->fillStyle == FillSolid ? gc->fgPixel : gc->tile.pixel));

		if (sna_drawable_use_gpu_bo(draw, &region.extents) &&
		    sna_poly_fill_rect_blt(draw,
					   priv->gpu_bo,
					   priv->gpu_only ? NULL : reduce_damage(draw, &priv->gpu_damage, &region.extents),
					   gc, color, n, rect,
					   &region.extents, flags & 2))
			return;

		if (sna_drawable_use_cpu_bo(draw, &region.extents) &&
		    sna_poly_fill_rect_blt(draw,
					   priv->cpu_bo,
					   reduce_damage(draw, &priv->cpu_damage, &region.extents),
					   gc, color, n, rect,
					   &region.extents, flags & 2))
			return;
	} else if (gc->fillStyle == FillTiled) {
		struct sna_pixmap *priv = sna_pixmap_from_drawable(draw);

		DBG(("%s: tiled fill, testing for blt\n", __FUNCTION__));

		if (sna_drawable_use_gpu_bo(draw, &region.extents) &&
		    sna_poly_fill_rect_tiled(draw,
					     priv->gpu_bo,
					     priv->gpu_only ? NULL : reduce_damage(draw, &priv->gpu_damage, &region.extents),
					     gc, n, rect,
					     &region.extents, flags & 2))
			return;

		if (sna_drawable_use_cpu_bo(draw, &region.extents) &&
		    sna_poly_fill_rect_tiled(draw,
					     priv->cpu_bo,
					     reduce_damage(draw, &priv->cpu_damage, &region.extents),
					     gc, n, rect,
					     &region.extents, flags & 2))
			return;
	}

fallback:
	DBG(("%s: fallback (%d, %d), (%d, %d)\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));
	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
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

static uint8_t byte_reverse(uint8_t b)
{
	return ((b * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32;
}

static uint8_t blt_depth(int depth)
{
	switch (depth) {
	case 8: return 0;
	case 15: return 0x2;
	case 16: return 0x1;
	default: return 0x3;
	}
}

static bool
sna_glyph_blt(DrawablePtr drawable, GCPtr gc,
	      int x, int y, unsigned int n,
	      CharInfoPtr *info, pointer base,
	      bool transparent,
	      const BoxRec *extents)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	struct sna_damage **damage;
	uint32_t *b;
	int16_t dx, dy;

	/* XXX sna_blt! */
	static const uint8_t ROP[] = {
		ROP_0,                  /* GXclear */
		ROP_DSa,                /* GXand */
		ROP_SDna,               /* GXandReverse */
		ROP_S,                  /* GXcopy */
		ROP_DSna,               /* GXandInverted */
		ROP_D,                  /* GXnoop */
		ROP_DSx,                /* GXxor */
		ROP_DSo,                /* GXor */
		ROP_DSon,               /* GXnor */
		ROP_DSxn,               /* GXequiv */
		ROP_Dn,                 /* GXinvert */
		ROP_SDno,               /* GXorReverse */
		ROP_Sn,                 /* GXcopyInverted */
		ROP_DSno,               /* GXorInverted */
		ROP_DSan,               /* GXnand */
		ROP_1                   /* GXset */
	};
	uint8_t rop = transparent ? ROP[gc->alu] : ROP_S;
	RegionRec clip;

	if (priv->gpu_bo->tiling == I915_TILING_Y) {
		DBG(("%s -- fallback, dst uses Y-tiling\n", __FUNCTION__));
		return false;
	}

	region_set(&clip, extents);
	region_maybe_clip(&clip, gc->pCompositeClip);
	if (!RegionNotEmpty(&clip))
		return true;

	/* XXX loop over clips using SETUP_CLIP? */
	if (clip.data != NULL) {
		DBG(("%s -- fallback too many clip rects [%d]\n",
		     __FUNCTION__, REGION_NUM_RECTS(&clip)));
		RegionUninit(&clip);
		return false;
	}

	damage = priv->gpu_only ? NULL :
		reduce_damage(drawable, &priv->gpu_damage, extents),

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	x += drawable->x + dx;
	y += drawable->y + dy;

	clip.extents.x1 += dx;
	clip.extents.x2 += dx;
	clip.extents.y1 += dy;
	clip.extents.y2 += dy;

	kgem_set_mode(&sna->kgem, KGEM_BLT);
	if (!kgem_check_batch(&sna->kgem, 16) ||
	    !kgem_check_bo_fenced(&sna->kgem, priv->gpu_bo, NULL) ||
	    !kgem_check_reloc(&sna->kgem, 1)) {
		_kgem_submit(&sna->kgem);
		_kgem_set_mode(&sna->kgem, KGEM_BLT);
	}
	b = sna->kgem.batch + sna->kgem.nbatch;
	b[0] = XY_SETUP_BLT | 1 << 20;
	b[1] = priv->gpu_bo->pitch;
	if (sna->kgem.gen >= 40) {
		if (priv->gpu_bo->tiling)
			b[0] |= 1 << 11;
		b[1] >>= 2;
	}
	b[1] |= 1 << 30 | transparent << 29 | blt_depth(drawable->depth) << 24 | rop << 16;
	b[2] = clip.extents.y1 << 16 | clip.extents.x1;
	b[3] = clip.extents.y2 << 16 | clip.extents.x2;
	b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4,
			      priv->gpu_bo,
			      I915_GEM_DOMAIN_RENDER << 16 |
			      I915_GEM_DOMAIN_RENDER |
			      KGEM_RELOC_FENCED,
			      0);
	b[5] = gc->bgPixel;
	b[6] = gc->fgPixel;
	b[7] = 0;
	sna->kgem.nbatch += 8;

	while (n--) {
		CharInfoPtr c = *info++;
		uint8_t *glyph = FONTGLYPHBITS(base, c);
		int w = GLYPHWIDTHPIXELS(c);
		int h = GLYPHHEIGHTPIXELS(c);
		int stride = GLYPHWIDTHBYTESPADDED(c);
		int w8 = (w + 7) >> 3;
		int x1, y1, len, i;
		uint8_t *byte;

		if (w == 0 || h == 0)
			goto skip;

		len = (w8 * h + 7) >> 3 << 1;
		DBG(("%s glyph: (%d, %d) x (%d[%d], %d), len=%d\n" ,__FUNCTION__,
		     x,y, w, w8, h, len));

		x1 = x + c->metrics.leftSideBearing;
		y1 = y - c->metrics.ascent;

		if (!kgem_check_batch(&sna->kgem, 3+len)) {
			_kgem_submit(&sna->kgem);
			_kgem_set_mode(&sna->kgem, KGEM_BLT);

			b = sna->kgem.batch + sna->kgem.nbatch;
			b[0] = XY_SETUP_BLT | 1 << 20;
			b[1] = priv->gpu_bo->pitch;
			if (sna->kgem.gen >= 40) {
				if (priv->gpu_bo->tiling)
					b[0] |= 1 << 11;
				b[1] >>= 2;
			}
			b[1] |= 1 << 30 | transparent << 29 | blt_depth(drawable->depth) << 24 | rop << 16;
			b[2] = clip.extents.y1 << 16 | clip.extents.x1;
			b[3] = clip.extents.y2 << 16 | clip.extents.x2;
			b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4,
					      priv->gpu_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      0);
			b[5] = gc->bgPixel;
			b[6] = gc->fgPixel;
			b[7] = 0;
			sna->kgem.nbatch += 8;
		}

		b = sna->kgem.batch + sna->kgem.nbatch;
		b[0] = XY_TEXT_IMMEDIATE_BLT | (1 + len);
		if (priv->gpu_bo->tiling && sna->kgem.gen >= 40)
			b[0] |= 1 << 11;
		b[1] = (uint16_t)y1 << 16 | (uint16_t)x1;
		b[2] = (uint16_t)(y1+h) << 16 | (uint16_t)(x1+w);

		byte = (uint8_t *)&b[3];
		stride -= w8;
		do {
			i = w8;
			do {
				*byte++ = byte_reverse(*glyph++);
			} while (--i);
			glyph += stride;
		} while (--h);
		while ((byte - (uint8_t *)&b[3]) & 7)
			*byte++ = 0;
		sna->kgem.nbatch += 3 + len;
		assert((uint32_t *)byte == sna->kgem.batch + sna->kgem.nbatch);

		if (damage) {
			BoxRec r;

			r.x1 = x1;
			r.y1 = y1;
			r.x2 = x1 + w;
			r.y2 = y1 + h;
			if (box_intersect(&r, &clip.extents))
				sna_damage_add_box(damage, &r);
		}
skip:
		x += c->metrics.characterWidth;
	}

	RegionUninit(&clip);
	sna->blt_state.fill_bo = 0;
	return true;
}

static void
sna_image_glyph(DrawablePtr drawable, GCPtr gc,
		int x, int y, unsigned int n,
		CharInfoPtr *info, pointer base)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	ExtentInfoRec extents;
	RegionRec region;

	if (n == 0)
		return;

	QueryGlyphExtents(gc->font, info, n, &extents);
	region.extents.x1 = x + extents.overallLeft;
	region.extents.y1 = y - extents.overallAscent;
	region.extents.x2 = x + extents.overallRight;
	region.extents.y2 = y + extents.overallDescent;

	trim_box(&region.extents, drawable);
	translate_box(&region.extents, drawable);
	clip_box(&region.extents, gc);
	if (box_empty(&region.extents))
		return;

	DBG(("%s: extents(%d, %d), (%d, %d)\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (sna_drawable_use_gpu_bo(drawable, &region.extents) &&
	    sna_glyph_blt(drawable, gc, x, y, n, info, base, false, &region.extents))
		return;

fallback:
	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	DBG(("%s: fallback -- fbImageGlyphBlt\n", __FUNCTION__));
	fbImageGlyphBlt(drawable, gc, x, y, n, info, base);
}

static void
sna_poly_glyph(DrawablePtr drawable, GCPtr gc,
	       int x, int y, unsigned int n,
	       CharInfoPtr *info, pointer base)
{
	struct sna *sna = to_sna_from_drawable(drawable);
	ExtentInfoRec extents;
	RegionRec region;

	if (n == 0)
		return;

	QueryGlyphExtents(gc->font, info, n, &extents);
	region.extents.x1 = x + extents.overallLeft;
	region.extents.y1 = y - extents.overallAscent;
	region.extents.x2 = x + extents.overallRight;
	region.extents.y2 = y + extents.overallDescent;

	trim_box(&region.extents, drawable);
	translate_box(&region.extents, drawable);
	clip_box(&region.extents, gc);
	if (box_empty(&region.extents))
		return;

	DBG(("%s: extents(%d, %d), (%d, %d)\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (sna_drawable_use_gpu_bo(drawable, &region.extents) &&
	    sna_glyph_blt(drawable, gc, x, y, n, info, base, true, &region.extents))
		return;

fallback:
	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	sna_gc_move_to_cpu(gc);
	sna_drawable_move_region_to_cpu(drawable, &region, true);
	RegionUninit(&region);

	DBG(("%s: fallback -- fbPolyGlyphBlt\n", __FUNCTION__));
	fbPolyGlyphBlt(drawable, gc, x, y, n, info, base);
}

static void
sna_push_pixels(GCPtr gc, PixmapPtr bitmap, DrawablePtr drawable,
		int w, int h,
		int x, int y)
{
	RegionRec region;

	if (w == 0 || h == 0)
		return;

	DBG(("%s (%d, %d)x(%d, %d)\n", __FUNCTION__, x, y, w, h));

	region.extents.x1 = x;
	region.extents.y1 = y;
	if (!gc->miTranslate) {
		region.extents.x1 += drawable->x;
		region.extents.y1 += drawable->y;
	}
	region.extents.x2 = region.extents.x1 + w;
	region.extents.y2 = region.extents.y1 + h;

	clip_box(&region.extents, gc);
	if (box_empty(&region.extents))
		return;

	DBG(("%s: extents(%d, %d), (%d, %d)\n", __FUNCTION__,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2));

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
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
	sna_poly_rectangle,
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
	RegionRec region;

	DBG(("%s (%d, %d, %d, %d)\n", __FUNCTION__, x, y, w, h));

	region.extents.x1 = x + drawable->x;
	region.extents.y1 = y + drawable->y;
	region.extents.x2 = region.extents.x1 + w;
	region.extents.y2 = region.extents.y1 + h;
	region.data = NULL;

	sna_drawable_move_region_to_cpu(drawable, &region, false);
	fbGetImage(drawable, x, y, w, h, format, mask, dst);
}

static void
sna_get_spans(DrawablePtr drawable, int wMax,
	      DDXPointPtr pt, int *width, int n, char *start)
{
	RegionRec region;

	if (sna_spans_extents(drawable, NULL, n, pt, width, &region.extents) == 0)
		return;

	region.data = NULL;
	sna_drawable_move_region_to_cpu(drawable, &region, false);

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

	if (wedged(sna)) {
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
		     NULL, &dst, dx, dy, sna_self_copy_boxes, 0, NULL);

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

static struct sna_pixmap *sna_accel_scanout(struct sna *sna)
{
	PixmapPtr front = sna->shadow ? sna->shadow : sna->front;
	struct sna_pixmap *priv = sna_pixmap(front);
	return priv && priv->gpu_bo ? priv : NULL;
}

#if HAVE_SYS_TIMERFD_H && !FORCE_FLUSH
#include <sys/timerfd.h>
#include <errno.h>

static uint64_t read_timer(int fd)
{
	uint64_t count = 0;
	int ret = read(fd, &count, sizeof(count));
	return count;
	(void)ret;
}

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
	if (priv == NULL) {
		DBG(("%s -- no scanout attached\n", __FUNCTION__));
		return FALSE;
	}

	if (priv->cpu_damage == NULL && priv->gpu_bo->rq == NULL) {
		DBG(("%s -- no pending write to scanout\n", __FUNCTION__));
		return FALSE;
	}

	if (sna->timer[FLUSH_TIMER] == -1)
		return TRUE;

	DBG(("%s, starting flush timer, at time=%ld\n",
	     __FUNCTION__, (long)GetTimeInMillis()));

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
static Bool sna_accel_do_expire(struct sna *sna) { return sna->kgem.need_expire; }
static void _sna_accel_disarm_timer(struct sna *sna, int id) { }
#endif

static bool sna_accel_flush(struct sna *sna)
{
	struct sna_pixmap *priv = sna_accel_scanout(sna);
	bool nothing_to_do =
		priv->cpu_damage == NULL && priv->gpu_bo->rq == NULL;

	DBG(("%s (time=%ld), nothing_to_do=%d, busy? %d\n",
	     __FUNCTION__, (long)GetTimeInMillis(),
	     nothing_to_do, sna->kgem.busy));

	if (nothing_to_do && !sna->kgem.busy)
		_sna_accel_disarm_timer(sna, FLUSH_TIMER);
	else
		sna_pixmap_move_to_gpu(priv->pixmap);
	sna->kgem.busy = 0;
	kgem_bo_flush(&sna->kgem, priv->gpu_bo);
	return !nothing_to_do;
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

	sna_window_key = fbGetWinPrivateKey();

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
			ps->Triangles = sna_composite_triangles;
#if PICTURE_SCREEN_VERSION >= 2
			ps->TriStrip = sna_composite_tristrip;
			ps->TriFan = sna_composite_trifan;
#endif
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

	kgem_cleanup_cache(&sna->kgem);
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
	if (sna_accel_do_flush(sna)) {
		if (sna_accel_flush(sna))
			sna_accel_throttle(sna);
	}

	if (sna_accel_do_expire(sna))
		sna_accel_expire(sna);
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
