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
#include "rop.h"

#include <X11/fonts/font.h>
#include <X11/fonts/fontstruct.h>

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
#endif

#define FORCE_GPU_ONLY 0
#define FORCE_INPLACE 0
#define FORCE_FALLBACK 0
#define FORCE_FLUSH 0

#define USE_SPANS 0
#define USE_ZERO_SPANS 1
#define USE_BO_FOR_SCRATCH_PIXMAP 1

static int sna_font_key;

static const uint8_t copy_ROP[] = {
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
static const uint8_t fill_ROP[] = {
	ROP_0,
	ROP_DPa,
	ROP_PDna,
	ROP_P,
	ROP_DPna,
	ROP_D,
	ROP_DPx,
	ROP_DPo,
	ROP_DPon,
	ROP_PDxn,
	ROP_Dn,
	ROP_PDno,
	ROP_Pn,
	ROP_DPno,
	ROP_DPan,
	ROP_1
};

static inline void region_set(RegionRec *r, const BoxRec *b)
{
	r->extents = *b;
	r->data = NULL;
}

static inline void region_maybe_clip(RegionRec *r, RegionRec *clip)
{
	if (clip->data)
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

static void sna_pixmap_destroy_gpu_bo(struct sna *sna, struct sna_pixmap *priv)
{
	sna_damage_destroy(&priv->gpu_damage);

	if (priv->gpu_bo && !priv->pinned) {
		kgem_bo_destroy(&sna->kgem, priv->gpu_bo);
		priv->gpu_bo = NULL;
	}

	if (priv->mapped) {
		priv->pixmap->devPrivate.ptr = NULL;
		priv->mapped = 0;
	}

	list_del(&priv->inactive);

	/* and reset the upload counter */
	priv->source_count = SOURCE_BIAS;
}

static bool must_check
sna_pixmap_alloc_cpu(struct sna *sna,
		     PixmapPtr pixmap,
		     struct sna_pixmap *priv,
		     bool from_gpu)
{
	/* Restore after a GTT mapping? */
	if (priv->ptr)
		goto done;

	assert(priv->stride);

	if (!DEBUG_NO_LLC && sna->kgem.gen >= 60) {
		DBG(("%s: allocating CPU buffer (%dx%d)\n", __FUNCTION__,
		     pixmap->drawable.width, pixmap->drawable.height));

		priv->cpu_bo = kgem_create_2d(&sna->kgem,
					      pixmap->drawable.width,
					      pixmap->drawable.height,
					      pixmap->drawable.bitsPerPixel,
					      I915_TILING_NONE,
					      from_gpu ? 0 : CREATE_CPU_MAP | CREATE_INACTIVE);
		DBG(("%s: allocated CPU handle=%d\n", __FUNCTION__,
		     priv->cpu_bo->handle));

		if (priv->cpu_bo) {
			priv->ptr = kgem_bo_map__cpu(&sna->kgem, priv->cpu_bo);
			if (priv->ptr == NULL) {
				kgem_bo_destroy(&sna->kgem, priv->cpu_bo);
				priv->cpu_bo = NULL;
			} else
				priv->stride = priv->cpu_bo->pitch;
		}
	}

	if (priv->ptr == NULL)
		priv->ptr = malloc(priv->stride * pixmap->drawable.height);

	assert(priv->ptr);
done:
	pixmap->devPrivate.ptr = priv->ptr;
	pixmap->devKind = priv->stride;
	assert(priv->stride);
	return priv->ptr != NULL;
}

static void sna_pixmap_free_cpu(struct sna *sna, struct sna_pixmap *priv)
{
	assert(priv->stride);

	if (priv->cpu_bo) {
		DBG(("%s: discarding CPU buffer, handle=%d, size=%d\n",
		     __FUNCTION__, priv->cpu_bo->handle, priv->cpu_bo->size));

		kgem_bo_unmap__cpu(&sna->kgem, priv->cpu_bo, priv->ptr);
		kgem_bo_destroy(&sna->kgem, priv->cpu_bo);

		priv->cpu_bo = NULL;
	} else
		free(priv->ptr);

	priv->pixmap->devPrivate.ptr = priv->ptr = NULL;
	list_del(&priv->list);
	priv->mapped = 0;
}

static Bool sna_destroy_private(PixmapPtr pixmap, struct sna_pixmap *priv)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);

	list_del(&priv->list);
	list_del(&priv->inactive);

	sna_damage_destroy(&priv->gpu_damage);
	sna_damage_destroy(&priv->cpu_damage);

	/* Always release the gpu bo back to the lower levels of caching */
	if (priv->gpu_bo)
		kgem_bo_destroy(&sna->kgem, priv->gpu_bo);

	if (priv->ptr)
		sna_pixmap_free_cpu(sna, priv);

	if (priv->cpu_bo) {
		if (priv->cpu_bo->vmap && kgem_bo_is_busy(priv->cpu_bo)) {
			list_add_tail(&priv->list, &sna->deferred_free);
			return false;
		}
		kgem_bo_destroy(&sna->kgem, priv->cpu_bo);
	}

	if (!sna->freed_pixmap && priv->header) {
		sna->freed_pixmap = pixmap;
		assert(priv->ptr == NULL);
		return false;
	}

	free(priv);
	return true;
}

static inline uint32_t default_tiling(PixmapPtr pixmap)
{
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	struct sna *sna = to_sna_from_pixmap(pixmap);

	/* Try to avoid hitting the Y-tiling GTT mapping bug on 855GM */
	if (sna->kgem.gen == 21)
		return I915_TILING_X;

	if (pixmap->usage_hint == CREATE_PIXMAP_USAGE_BACKING_PIXMAP) {
		/* Treat this like a window, and require accelerated
		 * scrolling i.e. overlapped blits.
		 */
		return I915_TILING_X;
	}

	return sna_damage_is_all(&priv->cpu_damage,
				 pixmap->drawable.width,
				 pixmap->drawable.height) ? I915_TILING_Y : sna->default_tiling;
}

constant static uint32_t sna_pixmap_choose_tiling(PixmapPtr pixmap)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);
	uint32_t tiling = default_tiling(pixmap);
	uint32_t bit;

	/* Use tiling by default, but disable per user request */
	if (pixmap->usage_hint == SNA_CREATE_FB) {
		tiling = I915_TILING_X;
		bit = SNA_TILING_FB;
	} else
		bit = SNA_TILING_2D;
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

struct kgem_bo *sna_pixmap_change_tiling(PixmapPtr pixmap, uint32_t tiling)
{
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct kgem_bo *bo;
	BoxRec box;

	DBG(("%s: changing tiling %d -> %d for %dx%d pixmap\n",
	     __FUNCTION__, priv->gpu_bo->tiling, tiling,
	     pixmap->drawable.width, pixmap->drawable.height));

	if (priv->pinned) {
		DBG(("%s: can't convert pinned bo\n", __FUNCTION__));
		return NULL;
	}

	bo = kgem_create_2d(&sna->kgem,
			    pixmap->drawable.width,
			    pixmap->drawable.height,
			    pixmap->drawable.bitsPerPixel,
			    tiling, 0);
	if (bo == NULL) {
		DBG(("%s: allocation failed\n", __FUNCTION__));
		return NULL;
	}

	box.x1 = box.y1 = 0;
	box.x2 = pixmap->drawable.width;
	box.y2 = pixmap->drawable.height;

	if (!sna->render.copy_boxes(sna, GXcopy,
				    pixmap, priv->gpu_bo, 0, 0,
				    pixmap, bo, 0, 0,
				    &box, 1)) {
		DBG(("%s: copy failed\n", __FUNCTION__));
		kgem_bo_destroy(&sna->kgem, bo);
		return false;
	}

	kgem_bo_destroy(&sna->kgem, priv->gpu_bo);
	return priv->gpu_bo = bo;
}

static inline void sna_set_pixmap(PixmapPtr pixmap, struct sna_pixmap *sna)
{
	dixSetPrivate(&pixmap->devPrivates, &sna_pixmap_index, sna);
	assert(sna_pixmap(pixmap) == sna);
}

static struct sna_pixmap *
_sna_pixmap_init(struct sna_pixmap *priv, PixmapPtr pixmap)
{
	list_init(&priv->list);
	list_init(&priv->inactive);
	priv->source_count = SOURCE_BIAS;
	priv->pixmap = pixmap;

	return priv;
}

static struct sna_pixmap *
_sna_pixmap_reset(PixmapPtr pixmap)
{
	struct sna_pixmap *priv;

	assert(pixmap->drawable.type == DRAWABLE_PIXMAP);
	assert(pixmap->drawable.class == 0);
	assert(pixmap->drawable.id == 0);
	assert(pixmap->drawable.x == 0);
	assert(pixmap->drawable.y == 0);

	priv = sna_pixmap(pixmap);
	assert(priv != NULL);

	memset(priv, 0, sizeof(*priv));
	return _sna_pixmap_init(priv, pixmap);
}

static struct sna_pixmap *_sna_pixmap_attach(struct sna *sna,
					     PixmapPtr pixmap)
{
	struct sna_pixmap *priv;

	priv = calloc(1, sizeof(*priv));
	if (!priv)
		return NULL;

	sna_set_pixmap(pixmap, priv);
	return _sna_pixmap_init(priv, pixmap);
}

struct sna_pixmap *sna_pixmap_attach(PixmapPtr pixmap)
{
	struct sna *sna;
	struct sna_pixmap *priv;

	priv = sna_pixmap(pixmap);
	if (priv)
		return priv;

	sna = to_sna_from_pixmap(pixmap);
	switch (pixmap->usage_hint) {
	case CREATE_PIXMAP_USAGE_GLYPH_PICTURE:
#if FAKE_CREATE_PIXMAP_USAGE_SCRATCH_HEADER
	case CREATE_PIXMAP_USAGE_SCRATCH_HEADER:
#endif
		return NULL;

	case SNA_CREATE_FB:
		/* We assume that the Screen pixmap will be pre-validated */
		break;

	default:
		if (!kgem_can_create_2d(&sna->kgem,
					pixmap->drawable.width,
					pixmap->drawable.height,
					pixmap->drawable.bitsPerPixel,
					I915_TILING_NONE))
			return NULL;
		break;
	}

	return _sna_pixmap_attach(sna, pixmap);
}

static inline PixmapPtr
create_pixmap(struct sna *sna, ScreenPtr screen,
	      int width, int height, int depth,
	      unsigned usage)
{
	PixmapPtr pixmap;

	pixmap = fbCreatePixmap(screen, width, height, depth, usage);
	if (pixmap == NullPixmap)
		return NullPixmap;

	assert(sna_private_index.offset == 0);
	dixSetPrivate(&pixmap->devPrivates, &sna_private_index, sna);
	return pixmap;
}

static PixmapPtr
sna_pixmap_create_scratch(ScreenPtr screen,
			  int width, int height, int depth,
			  uint32_t tiling)
{
	struct sna *sna = to_sna_from_screen(screen);
	struct sna_pixmap *priv;
	PixmapPtr pixmap;
	int bpp;

	DBG(("%s(%d, %d, %d, tiling=%d)\n", __FUNCTION__,
	     width, height, depth, tiling));

	if (depth < 8)
		return create_pixmap(sna, screen, width, height, depth,
				     CREATE_PIXMAP_USAGE_SCRATCH);

	if (tiling == I915_TILING_Y && !sna->have_render)
		tiling = I915_TILING_X;

	  if (tiling == I915_TILING_Y &&
	      (width > sna->render.max_3d_size ||
	       height > sna->render.max_3d_size))
		  tiling = I915_TILING_X;

	bpp = BitsPerPixel(depth);
	tiling = kgem_choose_tiling(&sna->kgem, tiling, width, height, bpp);
	if (!kgem_can_create_2d(&sna->kgem, width, height, bpp, tiling))
		return create_pixmap(sna, screen, width, height, depth,
				     CREATE_PIXMAP_USAGE_SCRATCH);

	/* you promise never to access this via the cpu... */
	if (sna->freed_pixmap) {
		pixmap = sna->freed_pixmap;
		sna->freed_pixmap = NULL;

		pixmap->usage_hint = CREATE_PIXMAP_USAGE_SCRATCH;
		pixmap->refcnt = 1;

		pixmap->drawable.width = width;
		pixmap->drawable.height = height;
		pixmap->drawable.depth = depth;
		pixmap->drawable.bitsPerPixel = bpp;
		pixmap->drawable.serialNumber = NEXT_SERIAL_NUMBER;

		priv = _sna_pixmap_reset(pixmap);
	} else {
		pixmap = create_pixmap(sna, screen, 0, 0, depth,
				       CREATE_PIXMAP_USAGE_SCRATCH);
		if (pixmap == NullPixmap)
			return NullPixmap;

		pixmap->drawable.width = width;
		pixmap->drawable.height = height;
		pixmap->drawable.depth = depth;
		pixmap->drawable.bitsPerPixel = bpp;

		priv = _sna_pixmap_attach(sna, pixmap);
		if (!priv) {
			fbDestroyPixmap(pixmap);
			return NullPixmap;
		}
	}

	priv->stride = PixmapBytePad(width, depth);
	pixmap->devPrivate.ptr = NULL;

	priv->gpu_bo = kgem_create_2d(&sna->kgem,
				      width, height, bpp, tiling,
				      0);
	if (priv->gpu_bo == NULL) {
		free(priv);
		fbDestroyPixmap(pixmap);
		return NullPixmap;
	}

	priv->header = true;
	sna_damage_all(&priv->gpu_damage, width, height);

	return pixmap;
}

static PixmapPtr sna_create_pixmap(ScreenPtr screen,
				   int width, int height, int depth,
				   unsigned int usage)
{
	struct sna *sna = to_sna_from_screen(screen);
	PixmapPtr pixmap;
	int pad, size;

	DBG(("%s(%d, %d, %d, usage=%x)\n", __FUNCTION__,
	     width, height, depth, usage));

	if (wedged(sna))
		return create_pixmap(sna, screen,
				     width, height, depth,
				     usage);

	if (usage == CREATE_PIXMAP_USAGE_SCRATCH)
#if USE_BO_FOR_SCRATCH_PIXMAP
		return sna_pixmap_create_scratch(screen,
						 width, height, depth,
						 I915_TILING_X);
#else
		return create_pixmap(sna, screen,
				     width, height, depth,
				     usage);
#endif

	if (usage == SNA_CREATE_SCRATCH)
		return sna_pixmap_create_scratch(screen,
						 width, height, depth,
						 I915_TILING_Y);

	if (FORCE_GPU_ONLY && width && height)
		return sna_pixmap_create_scratch(screen,
						 width, height, depth,
						 I915_TILING_X);

	if (usage == CREATE_PIXMAP_USAGE_GLYPH_PICTURE ||
	    !kgem_can_create_2d(&sna->kgem, width, height,
				BitsPerPixel(depth), I915_TILING_NONE))
		return create_pixmap(sna, screen, width, height, depth, usage);

#if FAKE_CREATE_PIXMAP_USAGE_SCRATCH_HEADER
	if (width == 0 || height == 0)
		return create_pixmap(sna, screen, width, height, depth, usage);
#endif

	pad = PixmapBytePad(width, depth);
	size = pad * height;
	if (size <= 4096) {
		pixmap = create_pixmap(sna, screen,
				       width, height, depth, usage);
		if (pixmap == NullPixmap)
			return NullPixmap;

		sna_pixmap_attach(pixmap);
	} else {
		struct sna_pixmap *priv;

		pixmap = create_pixmap(sna, screen, 0, 0, depth, usage);
		if (pixmap == NullPixmap)
			return NullPixmap;

		pixmap->drawable.width = width;
		pixmap->drawable.height = height;
		pixmap->devKind = pad;
		pixmap->devPrivate.ptr = NULL;

		priv = _sna_pixmap_attach(sna, pixmap);
		if (priv == NULL) {
			free(pixmap);
			return create_pixmap(sna, screen,
					     width, height, depth, usage);
		}

		priv->stride = pad;
	}

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

static inline void list_move(struct list *list, struct list *head)
{
	__list_del(list->prev, list->next);
	list_add(list, head);
}

static inline bool pixmap_inplace(struct sna *sna,
				  PixmapPtr pixmap,
				  struct sna_pixmap *priv)
{
	if (FORCE_INPLACE)
		return FORCE_INPLACE > 0;

	if (wedged(sna))
		return false;

	if (priv->mapped)
		return true;

	return (pixmap->devKind * pixmap->drawable.height >> 12) >
		sna->kgem.half_cpu_cache_pages;
}

bool
_sna_pixmap_move_to_cpu(PixmapPtr pixmap, unsigned int flags)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv;

	DBG(("%s(pixmap=%p, flags=%x)\n", __FUNCTION__, pixmap, flags));

	priv = sna_pixmap(pixmap);
	if (priv == NULL) {
		DBG(("%s: not attached to %p\n", __FUNCTION__, pixmap));
		return true;
	}

	DBG(("%s: gpu_bo=%p, gpu_damage=%p\n",
	     __FUNCTION__, priv->gpu_bo, priv->gpu_damage));

	if ((flags & MOVE_READ) == 0) {
		assert(flags == MOVE_WRITE);

		sna_damage_destroy(&priv->cpu_damage);
		sna_damage_destroy(&priv->gpu_damage);

		if (priv->stride && priv->gpu_bo &&
		    pixmap_inplace(sna, pixmap, priv)) {
			if (kgem_bo_is_busy(priv->gpu_bo) &&
			    priv->gpu_bo->exec == NULL)
				kgem_retire(&sna->kgem);

			if (kgem_bo_is_busy(priv->gpu_bo)) {
				sna_pixmap_destroy_gpu_bo(sna, priv);
				if (!sna_pixmap_move_to_gpu(pixmap, MOVE_WRITE))
					goto skip_inplace_map;
			}

			pixmap->devPrivate.ptr =
				kgem_bo_map(&sna->kgem, priv->gpu_bo,
					    PROT_WRITE);
			if (pixmap->devPrivate.ptr == NULL)
				goto skip_inplace_map;

			priv->mapped = 1;
			pixmap->devKind = priv->gpu_bo->pitch;

			sna_damage_all(&priv->gpu_damage,
				       pixmap->drawable.width,
				       pixmap->drawable.height);
			sna_damage_destroy(&priv->cpu_damage);
			if (priv->cpu_bo)
				sna_pixmap_free_cpu(sna, priv);

			return true;
		}

skip_inplace_map:
		if (priv->cpu_bo && kgem_bo_is_busy(priv->cpu_bo)) {
			if (priv->cpu_bo->exec == NULL)
				kgem_retire(&sna->kgem);

			if (kgem_bo_is_busy(priv->cpu_bo)) {
				DBG(("%s: discarding busy CPU bo\n", __FUNCTION__));
				sna_pixmap_free_cpu(sna, priv);
			}
		}
	}

	if (priv->mapped) {
		pixmap->devPrivate.ptr = NULL;
		priv->mapped = 0;
	}

	if (pixmap->devPrivate.ptr == NULL &&
	    !sna_pixmap_alloc_cpu(sna, pixmap, priv, priv->gpu_damage != NULL))
		return false;

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

	if (priv->cpu_bo) {
		DBG(("%s: syncing CPU bo\n", __FUNCTION__));
		kgem_bo_sync__cpu(&sna->kgem, priv->cpu_bo);
	}

	if (flags & MOVE_WRITE) {
		DBG(("%s: marking as damaged\n", __FUNCTION__));
		sna_damage_all(&priv->cpu_damage,
			       pixmap->drawable.width,
			       pixmap->drawable.height);
		sna_pixmap_destroy_gpu_bo(sna, priv);

		if (priv->flush)
			list_move(&priv->list, &sna->dirty_pixmaps);
	}

	priv->source_count = SOURCE_BIAS;
	return true;
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

static bool
region_subsumes_gpu_damage(const RegionRec *region, struct sna_pixmap *priv)
{
	if (region->data)
		return false;

	if (priv->gpu_damage) {
		const BoxRec *extents = &region->extents;
		const BoxRec *damage = &priv->gpu_damage->extents;
		if (extents->x2 < damage->x2 || extents->x1 > damage->x1 ||
		    extents->y2 < damage->y2 || extents->y1 > damage->y1)
			return false;
	}

	return true;
}

static bool sync_will_stall(struct kgem_bo *bo)
{
	return kgem_bo_is_busy(bo);
}

static inline bool region_inplace(struct sna *sna,
				  PixmapPtr pixmap,
				  RegionPtr region,
				  struct sna_pixmap *priv)
{
	if (FORCE_INPLACE)
		return FORCE_INPLACE > 0;

	if (wedged(sna))
		return false;

	if (priv->mapped)
		return true;

	return ((region->extents.x2 - region->extents.x1) *
		(region->extents.y2 - region->extents.y1) *
		pixmap->drawable.bitsPerPixel >> 12)
		>= sna->kgem.half_cpu_cache_pages;
}

static bool
sna_pixmap_create_mappable_gpu(PixmapPtr pixmap)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);;

	if (wedged(sna))
		return false;

	assert(priv->gpu_bo == NULL);
	priv->gpu_bo =
		kgem_create_2d(&sna->kgem,
			       pixmap->drawable.width,
			       pixmap->drawable.height,
			       pixmap->drawable.bitsPerPixel,
			       sna_pixmap_choose_tiling(pixmap),
			       CREATE_GTT_MAP | CREATE_INACTIVE);

	return priv->gpu_bo != NULL;
}

bool
sna_drawable_move_region_to_cpu(DrawablePtr drawable,
				RegionPtr region,
				unsigned flags)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv;
	int16_t dx, dy;

	DBG(("%s(pixmap=%p (%dx%d), [(%d, %d), (%d, %d)], flags=%d)\n",
	     __FUNCTION__, pixmap,
	     pixmap->drawable.width, pixmap->drawable.height,
	     RegionExtents(region)->x1, RegionExtents(region)->y1,
	     RegionExtents(region)->x2, RegionExtents(region)->y2,
	     flags));

	priv = sna_pixmap(pixmap);
	if (priv == NULL) {
		DBG(("%s: not attached to %p\n", __FUNCTION__, pixmap));
		return true;
	}

	if (priv->stride == 0 && priv->gpu_bo == NULL && flags & MOVE_WRITE)
		return _sna_pixmap_move_to_cpu(pixmap, flags);

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	DBG(("%s: delta=(%d, %d)\n", __FUNCTION__, dx, dy));
	if (dx | dy)
		RegionTranslate(region, dx, dy);

	if (region_subsumes_drawable(region, &pixmap->drawable)) {
		DBG(("%s: region subsumes drawable\n", __FUNCTION__));
		if (dx | dy)
			RegionTranslate(region, -dx, -dy);
		return _sna_pixmap_move_to_cpu(pixmap, flags);
	}

	if ((flags & MOVE_READ) == 0) {
		assert(flags == MOVE_WRITE);

		if (priv->stride && priv->gpu_bo &&
		    region_inplace(sna, pixmap, region, priv)) {
			if (sync_will_stall(priv->gpu_bo) &&
			    priv->gpu_bo->exec == NULL)
				kgem_retire(&sna->kgem);

			if (!kgem_bo_map_will_stall(&sna->kgem, priv->gpu_bo)) {
				pixmap->devPrivate.ptr =
					kgem_bo_map(&sna->kgem, priv->gpu_bo,
						    PROT_WRITE);
				if (pixmap->devPrivate.ptr == NULL)
					return false;

				priv->mapped = 1;
				pixmap->devKind = priv->gpu_bo->pitch;

				sna_damage_subtract(&priv->cpu_damage, region);
				if (priv->cpu_damage == NULL)
					sna_damage_all(&priv->gpu_damage,
						       pixmap->drawable.width,
						       pixmap->drawable.height);
				else
					sna_damage_add(&priv->gpu_damage,
						       region);

				return true;
			}
		}

		if (priv->cpu_bo && !priv->cpu_bo->vmap) {
			if (sync_will_stall(priv->cpu_bo) && priv->cpu_bo->exec == NULL)
				kgem_retire(&sna->kgem);
			if (sync_will_stall(priv->cpu_bo)) {
				sna_damage_subtract(&priv->cpu_damage, region);
				if (!sna_pixmap_move_to_gpu(pixmap, MOVE_WRITE))
					return false;

				sna_pixmap_free_cpu(sna, priv);
			}
		}

		if (priv->gpu_bo == NULL && priv->stride &&
		    sna_pixmap_choose_tiling(pixmap) != I915_TILING_NONE &&
		    region_inplace(sna, pixmap, region, priv) &&
		    sna_pixmap_create_mappable_gpu(pixmap)) {
			pixmap->devPrivate.ptr =
				kgem_bo_map(&sna->kgem, priv->gpu_bo,
					    PROT_WRITE);
			if (pixmap->devPrivate.ptr == NULL)
				return false;

			priv->mapped = 1;
			pixmap->devKind = priv->gpu_bo->pitch;

			sna_damage_subtract(&priv->cpu_damage, region);
			if (priv->cpu_damage == NULL)
				sna_damage_all(&priv->gpu_damage,
					       pixmap->drawable.width,
					       pixmap->drawable.height);
			else
				sna_damage_add(&priv->gpu_damage, region);

			return true;
		}
	}

	if (priv->mapped) {
		pixmap->devPrivate.ptr = NULL;
		priv->mapped = 0;
	}

	if (pixmap->devPrivate.ptr == NULL &&
	    !sna_pixmap_alloc_cpu(sna, pixmap, priv, priv->gpu_damage != NULL))
		return false;

	if (priv->gpu_bo == NULL)
		goto done;

	if ((flags & MOVE_READ) == 0) {
		assert(flags == MOVE_WRITE);
		sna_damage_subtract(&priv->gpu_damage, region);
		goto done;
	}

	if (sna_damage_contains_box(priv->gpu_damage,
				    REGION_EXTENTS(NULL, region)) != PIXMAN_REGION_OUT) {
		DBG(("%s: region (%dx%d) intersects gpu damage\n",
		     __FUNCTION__,
		     region->extents.x2 - region->extents.x1,
		     region->extents.y2 - region->extents.y1));

		if ((flags & MOVE_WRITE) == 0 &&
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
		kgem_bo_sync__cpu(&sna->kgem, priv->cpu_bo);
	}

	if (flags & MOVE_WRITE) {
		DBG(("%s: applying cpu damage\n", __FUNCTION__));
		assert_pixmap_contains_box(pixmap, RegionExtents(region));
		sna_damage_add(&priv->cpu_damage, region);
		if (priv->gpu_bo &&
		    sna_damage_is_all(&priv->cpu_damage,
				      pixmap->drawable.width,
				      pixmap->drawable.height)) {
			DBG(("%s: replaced entire pixmap\n", __FUNCTION__));
			sna_pixmap_destroy_gpu_bo(sna, priv);
		}
		if (priv->flush)
			list_move(&priv->list, &sna->dirty_pixmaps);
	}

	if (dx | dy)
		RegionTranslate(region, -dx, -dy);

	priv->source_count = SOURCE_BIAS;
	return true;
}

static bool
sna_pixmap_move_area_to_gpu(PixmapPtr pixmap, BoxPtr box)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	RegionRec i, r;

	DBG(("%s()\n", __FUNCTION__));

	if (priv->gpu_bo == NULL) {
		struct sna *sna = to_sna_from_pixmap(pixmap);
		unsigned flags;

		flags = 0;
		if (priv->cpu_damage)
			flags |= CREATE_INACTIVE;
		if (pixmap->usage_hint == SNA_CREATE_FB)
			flags |= CREATE_EXACT | CREATE_SCANOUT;

		priv->gpu_bo = kgem_create_2d(&sna->kgem,
					      pixmap->drawable.width,
					      pixmap->drawable.height,
					      pixmap->drawable.bitsPerPixel,
					      sna_pixmap_choose_tiling(pixmap),
					      flags);
		if (priv->gpu_bo == NULL)
			return false;

		DBG(("%s: created gpu bo\n", __FUNCTION__));
	}

	sna_damage_reduce(&priv->cpu_damage);
	DBG(("%s: CPU damage? %d\n", __FUNCTION__, priv->cpu_damage != NULL));

	if (priv->cpu_damage == NULL)
		goto done;

	region_set(&r, box);
	if (sna_damage_intersect(priv->cpu_damage, &r, &i)) {
		int n = REGION_NUM_RECTS(&i);
		struct kgem_bo *src_bo;
		Bool ok = FALSE;

		box = REGION_RECTS(&i);
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
						    pixmap,
						    priv->gpu_bo,
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
	if (!priv->pinned)
		list_move(&priv->inactive, &sna->active_pixmaps);
	return true;
}

static inline bool
box_inplace(PixmapPtr pixmap, const BoxRec *box)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);
	return ((box->x2 - box->x1) * (box->y2 - box->y1) * pixmap->drawable.bitsPerPixel >> 15) >= sna->kgem.half_cpu_cache_pages;
}

static inline Bool
_sna_drawable_use_gpu_bo(DrawablePtr drawable,
			 const BoxRec *box,
			 struct sna_damage ***damage)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	BoxRec extents;
	int16_t dx, dy;

	if (priv == NULL)
		return FALSE;

	if (priv->gpu_bo == NULL &&
	    (sna_pixmap_choose_tiling(pixmap) == I915_TILING_NONE ||
	     (priv->cpu_damage && !box_inplace(pixmap, box)) ||
	     !sna_pixmap_move_to_gpu(pixmap, MOVE_WRITE | MOVE_READ)))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	extents = *box;
	extents.x1 += dx;
	extents.x2 += dx;
	extents.y1 += dy;
	extents.y2 += dy;

	if (priv->gpu_damage) {
		int ret = sna_damage_contains_box(priv->gpu_damage, &extents);
		if (ret == PIXMAN_REGION_IN) {
			DBG(("%s: region wholly contained within GPU damage\n",
			     __FUNCTION__));
			*damage = NULL;
			return TRUE;
		}

		if (ret != PIXMAN_REGION_OUT && kgem_bo_is_busy(priv->gpu_bo)) {
			DBG(("%s: region partially contained within busy GPU damage\n",
			     __FUNCTION__));
			goto move_to_gpu;
		}
	}

	if (priv->cpu_damage) {
		int ret = sna_damage_contains_box(priv->cpu_damage, &extents);
		if (ret == PIXMAN_REGION_IN) {
			DBG(("%s: region wholly contained within CPU damage\n",
			     __FUNCTION__));
			return FALSE;
		}

		if (box_inplace(pixmap, box)) {
			DBG(("%s: forcing inplace\n", __FUNCTION__));
			goto move_to_gpu;
		}

		if (ret != PIXMAN_REGION_OUT && !kgem_bo_is_busy(priv->gpu_bo)) {
			DBG(("%s: region partially contained within idle CPU damage\n",
			     __FUNCTION__));
			return FALSE;
		}
	}

move_to_gpu:
	if (!sna_pixmap_move_area_to_gpu(pixmap, &extents))
		return FALSE;

	*damage = &priv->gpu_damage;
	return TRUE;
}

static inline Bool
sna_drawable_use_gpu_bo(DrawablePtr drawable,
			const BoxRec *box,
			struct sna_damage ***damage)
{
	Bool ret;

	DBG(("%s((%d, %d), (%d, %d))...\n", __FUNCTION__,
	     box->x1, box->y1, box->x2, box->y2));

	ret = _sna_drawable_use_gpu_bo(drawable, box, damage);

	DBG(("%s((%d, %d), (%d, %d)) = %d\n", __FUNCTION__,
	     box->x1, box->y1, box->x2, box->y2, ret));

	return ret;
}

static inline Bool
_sna_drawable_use_cpu_bo(DrawablePtr drawable,
			 const BoxRec *box,
			 struct sna_damage ***damage)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	BoxRec extents;
	int16_t dx, dy;

	if (priv == NULL)
		return FALSE;
	if (priv->cpu_bo == NULL)
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	extents = *box;
	extents.x1 += dx;
	extents.x2 += dx;
	extents.y1 += dy;
	extents.y2 += dy;

	if (priv->gpu_damage == NULL)
		goto done;

	if (sna_damage_contains_box(priv->gpu_damage,
				    &extents) != PIXMAN_REGION_OUT)
		return FALSE;

done:
	*damage = &priv->cpu_damage;
	if (priv->cpu_damage &&
	    (priv->cpu_damage->mode == DAMAGE_ALL ||
	     sna_damage_contains_box__no_reduce(priv->cpu_damage, &extents)))
		*damage = NULL;

	return TRUE;
}

static inline Bool
sna_drawable_use_cpu_bo(DrawablePtr drawable,
			const BoxRec *box,
			struct sna_damage ***damage)
{
	Bool ret = _sna_drawable_use_cpu_bo(drawable, box, damage);
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
		return create_pixmap(sna, screen, width, height, depth,
				     CREATE_PIXMAP_USAGE_SCRATCH);

	if (sna->freed_pixmap) {
		pixmap = sna->freed_pixmap;
		sna->freed_pixmap = NULL;

		pixmap->usage_hint = CREATE_PIXMAP_USAGE_SCRATCH;
		pixmap->drawable.serialNumber = NEXT_SERIAL_NUMBER;
		pixmap->refcnt = 1;
	} else {
		pixmap = create_pixmap(sna, screen, 0, 0, depth,
				       CREATE_PIXMAP_USAGE_SCRATCH);
		if (!pixmap)
			return NullPixmap;

		priv = malloc(sizeof(*priv));
		if (!priv) {
			fbDestroyPixmap(pixmap);
			return NullPixmap;
		}

		sna_set_pixmap(pixmap, priv);
	}

	priv = _sna_pixmap_reset(pixmap);
	priv->header = true;

	priv->gpu_bo = kgem_create_buffer(&sna->kgem,
					  pad*height, KGEM_BUFFER_WRITE,
					  &ptr);
	if (!priv->gpu_bo) {
		free(priv);
		fbDestroyPixmap(pixmap);
		return NullPixmap;
	}

	priv->gpu_bo->pitch = pad;

	pixmap->drawable.width = width;
	pixmap->drawable.height = height;
	pixmap->drawable.depth = depth;
	pixmap->drawable.bitsPerPixel = bpp;
	pixmap->drawable.serialNumber = NEXT_SERIAL_NUMBER;
	pixmap->devKind = pad;
	pixmap->devPrivate.ptr = ptr;

	return pixmap;
}

struct sna_pixmap *
sna_pixmap_force_to_gpu(PixmapPtr pixmap, unsigned flags)
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
		struct sna *sna = to_sna_from_pixmap(pixmap);
		unsigned flags;

		flags = 0;
		if (priv->cpu_damage)
			flags |= CREATE_INACTIVE;
		if (pixmap->usage_hint == SNA_CREATE_FB)
			flags |= CREATE_EXACT | CREATE_SCANOUT;

		priv->gpu_bo = kgem_create_2d(&sna->kgem,
					      pixmap->drawable.width,
					      pixmap->drawable.height,
					      pixmap->drawable.bitsPerPixel,
					      sna_pixmap_choose_tiling(pixmap),
					      flags);
		if (priv->gpu_bo == NULL)
			return NULL;

		DBG(("%s: created gpu bo\n", __FUNCTION__));
	}

	if (!sna_pixmap_move_to_gpu(pixmap, flags))
		return NULL;

	return priv;
}

struct sna_pixmap *
sna_pixmap_move_to_gpu(PixmapPtr pixmap, unsigned flags)
{
	struct sna *sna = to_sna_from_pixmap(pixmap);
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
					       priv->cpu_damage ? CREATE_GTT_MAP | CREATE_INACTIVE : 0);
		if (priv->gpu_bo == NULL) {
			assert(list_is_empty(&priv->list));
			return NULL;
		}

		if (flags & MOVE_WRITE && priv->cpu_damage == NULL) {
			/* Presume that we will only ever write to the GPU
			 * bo. Readbacks are expensive but fairly constant
			 * in cost for all sizes i.e. it is the act of
			 * synchronisation that takes the most time. This is
			 * mitigated by avoiding fallbacks in the first place.
			 */
			sna_damage_all(&priv->gpu_damage,
				       pixmap->drawable.width,
				       pixmap->drawable.height);
		}
	}

	if ((flags & MOVE_READ) == 0)
		sna_damage_destroy(&priv->cpu_damage);

	if (priv->cpu_damage == NULL)
		goto done;

	assert(pixmap->devPrivate.ptr != NULL);

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
						    pixmap,
						    priv->gpu_bo,
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

done:
	sna_damage_reduce_all(&priv->gpu_damage,
			      pixmap->drawable.width,
			      pixmap->drawable.height);
	if (priv->mapped) {
		pixmap->devPrivate.ptr = NULL;
		priv->mapped = 0;
	}
	list_del(&priv->list);
	if (!priv->pinned)
		list_move(&priv->inactive, &sna->active_pixmaps);
	return priv;
}

static bool must_check sna_validate_pixmap(DrawablePtr draw, PixmapPtr pixmap)
{
	bool ret = true;

	if (draw->bitsPerPixel == pixmap->drawable.bitsPerPixel &&
	    FbEvenTile(pixmap->drawable.width *
		       pixmap->drawable.bitsPerPixel)) {
		DBG(("%s: flushing pixmap\n", __FUNCTION__));
		ret = sna_pixmap_move_to_cpu(pixmap, MOVE_READ | MOVE_WRITE);
	}

	return ret;
}

static bool must_check sna_gc_move_to_cpu(GCPtr gc, DrawablePtr drawable)
{
	struct sna_gc *sgc = sna_gc(gc);
	long changes = sgc->changes;

	DBG(("%s, changes=%lx\n", __FUNCTION__, changes));

	if (gc->clientClipType == CT_PIXMAP) {
		PixmapPtr clip = gc->clientClip;
		gc->clientClip = BitmapToRegion(gc->pScreen, clip);
		gc->pScreen->DestroyPixmap(clip);
		gc->clientClipType = gc->clientClip ? CT_REGION : CT_NONE;
		changes |= GCClipMask;
	} else
		changes &= ~GCClipMask;

	if (changes || drawable->serialNumber != sgc->serial) {
		gc->serialNumber = sgc->serial;

		if (changes & GCTile && !gc->tileIsPixel) {
			DBG(("%s: flushing tile pixmap\n", __FUNCTION__));
			if (!sna_validate_pixmap(drawable, gc->tile.pixmap))
				return false;
		}

		if (changes & GCStipple && gc->stipple) {
			DBG(("%s: flushing stipple pixmap\n", __FUNCTION__));
			if (!sna_pixmap_move_to_cpu(gc->stipple, MOVE_READ))
				return false;
		}

		fbValidateGC(gc, changes, drawable);

		gc->serialNumber = drawable->serialNumber;
		sgc->serial = drawable->serialNumber;
	}
	sgc->changes = 0;

	switch (gc->fillStyle) {
	case FillTiled:
		return sna_drawable_move_to_cpu(&gc->tile.pixmap->drawable, MOVE_READ);
	case FillStippled:
	case FillOpaqueStippled:
		return sna_drawable_move_to_cpu(&gc->stipple->drawable, MOVE_READ);
	default:
		return true;
	}
}

static inline bool clip_box(BoxPtr box, GCPtr gc)
{
	const BoxRec *clip;
	bool clipped;

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
	translate_box(box, d);
	return clip_box(box, gc);
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
	box32_translate(box, d);
	return box32_clip(box, gc);
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
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
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

	if (priv->gpu_bo == NULL &&
	    !sna_pixmap_create_mappable_gpu(pixmap))
		return FALSE;

	assert(priv->gpu_bo);

	if (gc->alu == GXcopy &&
	    !priv->pinned && nbox == 1 &&
	    box->x1 <= 0 && box->y1 <= 0 &&
	    box->x2 >= pixmap->drawable.width &&
	    box->y2 >= pixmap->drawable.height) {
		priv->gpu_bo =
			sna_replace(sna, pixmap, priv->gpu_bo, bits, stride);
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
sna_put_zpixmap_blt(DrawablePtr drawable, GCPtr gc, RegionPtr region,
		    int x, int y, int w, int  h, char *bits, int stride)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	char *dst_bits;
	int dst_stride;
	BoxRec *box;
	int16_t dx, dy;
	int n;

	assert_pixmap_contains_box(pixmap, RegionExtents(region));

	if (gc->alu != GXcopy)
		return false;

	if (!priv) {
		if (drawable->depth < 8)
			return false;

		goto blt;
	}

	if (!priv->pinned && priv->gpu_bo &&
	    region_subsumes_gpu_damage(region, priv) &&
	    kgem_bo_map_will_stall(&sna->kgem, priv->gpu_bo)) {
		sna_damage_destroy(&priv->gpu_damage);
		sna_pixmap_destroy_gpu_bo(sna, priv);
	}

	/* XXX performing the upload inplace is currently about 20x slower
	 * for putimage10 on gen6 -- mostly due to slow page faulting in kernel.
	 * So we try again with vma caching and only for pixmaps who will be
	 * immediately flushed...
	 */
	if ((priv->flush ||
	     (region_inplace(sna, pixmap, region, priv) &&
	      ((priv->gpu_bo == NULL && priv->cpu_bo == NULL) ||
	       (priv->gpu_bo != NULL && !kgem_bo_map_will_stall(&sna->kgem, priv->gpu_bo))))) &&
	    sna_put_image_upload_blt(drawable, gc, region,
				     x, y, w, h, bits, stride)) {
		if (region_subsumes_drawable(region, &pixmap->drawable)) {
			sna_damage_destroy(&priv->cpu_damage);
			sna_damage_all(&priv->gpu_damage,
				       pixmap->drawable.width,
				       pixmap->drawable.height);
		} else {
			sna_damage_subtract(&priv->cpu_damage, region);
			sna_damage_add(&priv->gpu_damage, region);
		}

		return true;
	}

	if (priv->cpu_bo) {
		/* If the GPU is currently accessing the CPU pixmap, then
		 * we will need to wait for that to finish before we can
		 * modify the memory.
		 *
		 * However, we can queue some writes to the GPU bo to avoid
		 * the wait. Or we can try to replace the CPU bo.
		 */
		if (sync_will_stall(priv->cpu_bo) && priv->cpu_bo->exec == NULL)
			kgem_retire(&sna->kgem);
		if (sync_will_stall(priv->cpu_bo)) {
			if (priv->cpu_bo->vmap) {
				if (sna_put_image_upload_blt(drawable, gc, region,
							     x, y, w, h, bits, stride)) {
					if (region_subsumes_drawable(region, &pixmap->drawable)) {
						sna_damage_destroy(&priv->cpu_damage);
						sna_damage_all(&priv->gpu_damage,
							       pixmap->drawable.width,
							       pixmap->drawable.height);
					} else {
						sna_damage_subtract(&priv->cpu_damage, region);
						sna_damage_add(&priv->gpu_damage, region);
					}

					return true;
				}
			} else {
				if (!region_subsumes_drawable(region, &pixmap->drawable)) {
					sna_damage_subtract(&priv->cpu_damage, region);
					if (!sna_pixmap_move_to_gpu(pixmap,
								    MOVE_WRITE))
						return false;
				}

				sna_pixmap_free_cpu(sna, priv);
			}
		}

		if (priv->cpu_bo)
			kgem_bo_sync__cpu(&sna->kgem, priv->cpu_bo);
	}

	if (pixmap->devPrivate.ptr == NULL &&
	    !sna_pixmap_alloc_cpu(sna, pixmap, priv, false))
		return true;

	if (region_subsumes_drawable(region, &pixmap->drawable)) {
		DBG(("%s: replacing entire pixmap\n", __FUNCTION__));
		sna_damage_all(&priv->cpu_damage,
			       pixmap->drawable.width,
			       pixmap->drawable.height);
		sna_pixmap_destroy_gpu_bo(sna, priv);
	} else {
		sna_damage_subtract(&priv->gpu_damage, region);
		sna_damage_add(&priv->cpu_damage, region);
		if (priv->gpu_bo &&
		    sna_damage_is_all(&priv->cpu_damage,
				      pixmap->drawable.width,
				      pixmap->drawable.height)) {
			DBG(("%s: replaced entire pixmap\n", __FUNCTION__));
			sna_pixmap_destroy_gpu_bo(sna, priv);
		}
	}
	if (priv->flush)
		list_move(&priv->list, &sna->dirty_pixmaps);
	priv->source_count = SOURCE_BIAS;

blt:
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

static inline uint8_t byte_reverse(uint8_t b)
{
	return ((b * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32;
}

static inline uint8_t blt_depth(int depth)
{
	switch (depth) {
	case 8: return 0;
	case 15: return 0x2;
	case 16: return 0x1;
	default: return 0x3;
	}
}

static Bool
sna_put_xybitmap_blt(DrawablePtr drawable, GCPtr gc, RegionPtr region,
		    int x, int y, int w, int  h, char *bits)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	struct sna_damage **damage;
	BoxRec *box;
	int16_t dx, dy;
	int n;
	uint8_t rop = copy_ROP[gc->alu];

	if (!sna_drawable_use_gpu_bo(&pixmap->drawable,
				     &region->extents,
				     &damage))
		return false;

	if (priv->gpu_bo->tiling == I915_TILING_Y) {
		DBG(("%s: converting bo from Y-tiling\n", __FUNCTION__));
		if (!sna_pixmap_change_tiling(pixmap, I915_TILING_X))
			return false;
	}

	assert_pixmap_contains_box(pixmap, RegionExtents(region));
	if (damage)
		sna_damage_add(damage, region);

	DBG(("%s: upload(%d, %d, %d, %d)\n", __FUNCTION__, x, y, w, h));

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	x += dx + drawable->x;
	y += dy + drawable->y;

	kgem_set_mode(&sna->kgem, KGEM_BLT);

	/* Region is pre-clipped and translated into pixmap space */
	box = REGION_RECTS(region);
	n = REGION_NUM_RECTS(region);
	do {
		int bx1 = (box->x1 - x) & ~7;
		int bx2 = (box->x2 - x + 7) & ~7;
		int bw = (bx2 - bx1)/8;
		int bh = box->y2 - box->y1;
		int bstride = ALIGN(bw, 2);
		int src_stride;
		uint8_t *dst, *src;
		uint32_t *b;
		struct kgem_bo *upload;
		void *ptr;

		if (!kgem_check_batch(&sna->kgem, 8) ||
		    !kgem_check_bo_fenced(&sna->kgem, priv->gpu_bo, NULL) ||
		    !kgem_check_reloc(&sna->kgem, 2)) {
			_kgem_submit(&sna->kgem);
			_kgem_set_mode(&sna->kgem, KGEM_BLT);
		}

		upload = kgem_create_buffer(&sna->kgem,
					    bstride*bh,
					    KGEM_BUFFER_WRITE,
					    &ptr);
		if (!upload)
			break;

		dst = ptr;
		bstride -= bw;

		src_stride = BitmapBytePad(w);
		src = (uint8_t*)bits + (box->y1 - y) * src_stride + bx1/8;
		src_stride -= bw;
		do {
			int i = bw;
			do {
				*dst++ = byte_reverse(*src++);
			} while (--i);
			dst += bstride;
			src += src_stride;
		} while (--bh);

		b = sna->kgem.batch + sna->kgem.nbatch;
		b[0] = XY_MONO_SRC_COPY;
		if (drawable->bitsPerPixel == 32)
			b[0] |= 3 << 20;
		b[0] |= ((box->x1 - x) & 7) << 17;
		b[1] = priv->gpu_bo->pitch;
		if (sna->kgem.gen >= 40) {
			if (priv->gpu_bo->tiling)
				b[0] |= BLT_DST_TILED;
			b[1] >>= 2;
		}
		b[1] |= blt_depth(drawable->depth) << 24;
		b[1] |= rop << 16;
		b[2] = box->y1 << 16 | box->x1;
		b[3] = box->y2 << 16 | box->x2;
		b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4,
				      priv->gpu_bo,
				      I915_GEM_DOMAIN_RENDER << 16 |
				      I915_GEM_DOMAIN_RENDER |
				      KGEM_RELOC_FENCED,
				      0);
		b[5] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 5,
				      upload,
				      I915_GEM_DOMAIN_RENDER << 16 |
				      KGEM_RELOC_FENCED,
				      0);
		b[6] = gc->bgPixel;
		b[7] = gc->fgPixel;

		sna->kgem.nbatch += 8;
		kgem_bo_destroy(&sna->kgem, upload);

		box++;
	} while (--n);

	sna->blt_state.fill_bo = 0;
	return true;
}

static Bool
sna_put_xypixmap_blt(DrawablePtr drawable, GCPtr gc, RegionPtr region,
		    int x, int y, int w, int  h, int left,char *bits)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	struct sna_damage **damage;
	struct kgem_bo *bo = priv->gpu_bo;
	int16_t dx, dy;
	unsigned i, skip;

	if (gc->alu != GXcopy)
		return false;

	if (!sna_drawable_use_gpu_bo(&pixmap->drawable,
				     &region->extents,
				     &damage))
		return false;

	if (bo->tiling == I915_TILING_Y) {
		DBG(("%s: converting bo from Y-tiling\n", __FUNCTION__));
		if (!sna_pixmap_change_tiling(pixmap, I915_TILING_X))
			return false;

		bo = priv->gpu_bo;
	}

	assert_pixmap_contains_box(pixmap, RegionExtents(region));
	if (damage)
		sna_damage_add(damage, region);

	DBG(("%s: upload(%d, %d, %d, %d)\n", __FUNCTION__, x, y, w, h));

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	x += dx + drawable->x;
	y += dy + drawable->y;

	kgem_set_mode(&sna->kgem, KGEM_BLT);

	skip = h * BitmapBytePad(w + left);
	for (i = 1 << (gc->depth-1); i; i >>= 1, bits += skip) {
		const BoxRec *box = REGION_RECTS(region);
		int n = REGION_NUM_RECTS(region);

		if ((gc->planemask & i) == 0)
			continue;

		/* Region is pre-clipped and translated into pixmap space */
		do {
			int bx1 = (box->x1 - x) & ~7;
			int bx2 = (box->x2 - x + 7) & ~7;
			int bw = (bx2 - bx1)/8;
			int bh = box->y2 - box->y1;
			int bstride = ALIGN(bw, 2);
			int src_stride;
			uint8_t *dst, *src;
			uint32_t *b;
			struct kgem_bo *upload;
			void *ptr;

			if (!kgem_check_batch(&sna->kgem, 12) ||
			    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
			    !kgem_check_reloc(&sna->kgem, 2)) {
				_kgem_submit(&sna->kgem);
				_kgem_set_mode(&sna->kgem, KGEM_BLT);
			}

			upload = kgem_create_buffer(&sna->kgem,
						    bstride*bh,
						    KGEM_BUFFER_WRITE,
						    &ptr);
			if (!upload)
				break;

			dst = ptr;
			bstride -= bw;

			src_stride = BitmapBytePad(w);
			src = (uint8_t*)bits + (box->y1 - y) * src_stride + bx1/8;
			src_stride -= bw;
			do {
				int j = bw;
				do {
					*dst++ = byte_reverse(*src++);
				} while (--j);
				dst += bstride;
				src += src_stride;
			} while (--bh);

			b = sna->kgem.batch + sna->kgem.nbatch;
			b[0] = XY_FULL_MONO_PATTERN_MONO_SRC_BLT;
			if (drawable->bitsPerPixel == 32)
				b[0] |= 3 << 20;
			b[0] |= ((box->x1 - x) & 7) << 17;
			b[1] = bo->pitch;
			if (sna->kgem.gen >= 40) {
				if (bo->tiling)
					b[0] |= BLT_DST_TILED;
				b[1] >>= 2;
			}
			b[1] |= 1 << 31; /* solid pattern */
			b[1] |= blt_depth(drawable->depth) << 24;
			b[1] |= 0xce << 16; /* S or (D and !P) */
			b[2] = box->y1 << 16 | box->x1;
			b[3] = box->y2 << 16 | box->x2;
			b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4,
					      bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      0);
			b[5] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 5,
					      upload,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      KGEM_RELOC_FENCED,
					      0);
			b[6] = 0;
			b[7] = i;
			b[8] = i;
			b[9] = i;
			b[10] = -1;
			b[11] = -1;

			sna->kgem.nbatch += 12;
			kgem_bo_destroy(&sna->kgem, upload);

			box++;
		} while (--n);
	}

	sna->blt_state.fill_bo = 0;
	return true;
}

static void
sna_put_image(DrawablePtr drawable, GCPtr gc, int depth,
	      int x, int y, int w, int h, int left, int format,
	      char *bits)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	RegionRec region;
	int16_t dx, dy;

	DBG(("%s((%d, %d)x(%d, %d), depth=%d, format=%d)\n",
	     __FUNCTION__, x, y, w, h, depth, format));

	if (w == 0 || h == 0)
		return;

	if (priv == NULL) {
		DBG(("%s: fbPutImage, unattached(%d, %d, %d, %d)\n",
		     __FUNCTION__, x, y, w, h));
		if (!sna_gc_move_to_cpu(gc, drawable))
			goto out;

		fbPutImage(drawable, gc, depth, x, y, w, h, left, format, bits);
		return;
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	region.extents.x1 = x + drawable->x;
	region.extents.y1 = y + drawable->y;
	region.extents.x2 = region.extents.x1 + w;
	region.extents.y2 = region.extents.y1 + h;
	region.data = NULL;

	RegionIntersect(&region, &region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	RegionTranslate(&region, dx, dy);

	if (wedged(sna))
		goto fallback;

	switch (format) {
	case ZPixmap:
		if (!PM_IS_SOLID(drawable, gc->planemask))
			goto fallback;

		if (sna_put_zpixmap_blt(drawable, gc, &region,
					x, y, w, h,
					bits, PixmapBytePad(w, depth)))
			return;
		break;

	case XYBitmap:
		if (!PM_IS_SOLID(drawable, gc->planemask))
			goto fallback;

		if (sna_put_xybitmap_blt(drawable, gc, &region,
					 x, y, w, h,
					 bits))
			return;
		break;

	case XYPixmap:
		if (sna_put_xypixmap_blt(drawable, gc, &region,
					 x, y, w, h, left,
					 bits))
			return;
		break;

	default:
		break;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	RegionTranslate(&region, -dx, -dy);

	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;

	DBG(("%s: fbPutImage(%d, %d, %d, %d)\n",
	     __FUNCTION__, x, y, w, h));
	fbPutImage(drawable, gc, depth, x, y, w, h, left, format, bits);
out:
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

	return ++priv->source_count * w*h >= (SOURCE_BIAS+2) * (int)pixmap->drawable.width * pixmap->drawable.height;
}

static void
sna_self_copy_boxes(DrawablePtr src, DrawablePtr dst, GCPtr gc,
		    BoxPtr box, int n,
		    int dx, int dy,
		    Bool reverse, Bool upsidedown, Pixel bitplane,
		    void *closure)
{
	PixmapPtr pixmap = get_drawable_pixmap(src);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	int alu = gc ? gc->alu : GXcopy;
	int16_t tx, ty;

	if (n == 0 || ((dx | dy) == 0 && alu == GXcopy))
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
		if (!sna_pixmap_move_to_gpu(pixmap, MOVE_WRITE | MOVE_READ)) {
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

		sna_damage_add_boxes(&priv->gpu_damage, box, n, tx, ty);
	} else {
		FbBits *dst_bits, *src_bits;
		int stride, bpp;

fallback:
		DBG(("%s: fallback", __FUNCTION__));
		if (!sna_pixmap_move_to_cpu(pixmap, MOVE_READ | MOVE_WRITE))
			return;

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

static bool copy_use_gpu_bo(struct sna *sna,
			    struct sna_pixmap *priv,
			    RegionPtr region)
{
	if (priv->flush)
		return true;

	if (region_inplace(sna, priv->pixmap, region, priv))
		return true;

	if (!priv->cpu_bo)
	       return false;

	if (kgem_bo_is_busy(priv->cpu_bo)) {
		if (priv->cpu_bo->exec)
			return true;

		kgem_retire(&sna->kgem);
	}

	return kgem_bo_is_busy(priv->cpu_bo);
}

static void
sna_copy_boxes(DrawablePtr src, DrawablePtr dst, GCPtr gc,
	       BoxPtr box, int n,
	       int dx, int dy,
	       Bool reverse, Bool upsidedown, Pixel bitplane,
	       void *closure)
{
	PixmapPtr src_pixmap = get_drawable_pixmap(src);
	struct sna_pixmap *src_priv = sna_pixmap(src_pixmap);
	PixmapPtr dst_pixmap = get_drawable_pixmap(dst);
	struct sna_pixmap *dst_priv = sna_pixmap(dst_pixmap);
	struct sna *sna = to_sna_from_pixmap(src_pixmap);
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

	if (dst_priv == NULL)
		goto fallback;

	if (replaces)
		sna_damage_destroy(&dst_priv->cpu_damage);

	/* Try to maintain the data on the GPU */
	if (dst_priv->gpu_bo == NULL &&
	    ((dst_priv->cpu_damage == NULL && copy_use_gpu_bo(sna, dst_priv, &region)) ||
	     (src_priv && (src_priv->gpu_bo != NULL || (src_priv->cpu_bo && kgem_bo_is_busy(src_priv->cpu_bo)))))) {
		uint32_t tiling = sna_pixmap_choose_tiling(dst_pixmap);

		DBG(("%s: create dst GPU bo for upload\n", __FUNCTION__));

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

	if (dst_priv->gpu_bo) {
		if (!src_priv && !copy_use_gpu_bo(sna, dst_priv, &region)) {
			DBG(("%s: fallback - src_priv=%p and not use dst gpu bo\n",
			     __FUNCTION__, src_priv));
			goto fallback;
		}

		if (alu != GXcopy && !sna_pixmap_move_to_gpu(dst_pixmap, MOVE_READ | MOVE_WRITE)) {
			DBG(("%s: fallback - not a pure copy and failed to move dst to GPU\n",
			     __FUNCTION__));
			goto fallback;
		}

		if (src_priv &&
		    move_to_gpu(src_pixmap, src_priv, &region.extents, alu) &&
		    sna_pixmap_move_to_gpu(src_pixmap, MOVE_READ)) {
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
		} else if (src_priv && src_priv->cpu_bo) {
			if (!sna->render.copy_boxes(sna, alu,
						    src_pixmap, src_priv->cpu_bo, src_dx, src_dy,
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
				if (!sna_drawable_move_region_to_cpu(&src_pixmap->drawable,
								&region, MOVE_READ))
					goto out;
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
						    dst_pixmap,
						    dst_priv->gpu_bo,
						    bits, stride);

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
			if (!sna_drawable_move_region_to_cpu(&src_pixmap->drawable,
							     &region, MOVE_READ))
				goto out;
			RegionTranslate(&region, -src_dx, -src_dy);
		}

		RegionTranslate(&region, dst_dx, dst_dy);
		if (dst_priv) {
			unsigned mode;

			assert_pixmap_contains_box(dst_pixmap,
						   RegionExtents(&region));

			mode = MOVE_WRITE;
			if (alu != GXcopy)
				mode |= MOVE_READ;
			if (!sna_drawable_move_region_to_cpu(&dst_pixmap->drawable,
							     &region, mode))
				goto out;
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
				assert(box->x1 + src_dx >= 0);
				assert(box->y1 + src_dy >= 0);
				assert(box->x1 + dst_dx >= 0);
				assert(box->y1 + dst_dy >= 0);
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
out:
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
		RegionRec region, *ret;

		DBG(("%s: -- fallback, wedged=%d, solid=%d [%x]\n",
		     __FUNCTION__, sna->kgem.wedged,
		     PM_IS_SOLID(dst, gc->planemask),
		     (unsigned)gc->planemask));

		region.extents.x1 = dst_x + dst->x;
		region.extents.y1 = dst_y + dst->y;
		region.extents.x2 = region.extents.x1 + width;
		region.extents.y2 = region.extents.y1 + height;
		region.data = NULL;
		RegionIntersect(&region, &region, gc->pCompositeClip);
		if (!RegionNotEmpty(&region))
			return NULL;

		ret = NULL;
		if (!sna_gc_move_to_cpu(gc, dst))
			goto out;

		if (!sna_drawable_move_region_to_cpu(dst, &region, MOVE_READ | MOVE_WRITE))
			goto out;

		RegionTranslate(&region,
				src_x - dst_x - dst->x + src->x,
				src_y - dst_y - dst->y + src->y);
		if (!sna_drawable_move_region_to_cpu(src, &region, MOVE_READ))
			goto out;

		ret = fbCopyArea(src, dst, gc,
				  src_x, src_y,
				  width, height,
				  dst_x, dst_y);
out:
		RegionUninit(&region);
		return ret;
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
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	int16_t dx, dy;
	struct sna_fill_op fill;
	BoxRec box[512], *b = box, *const last_box = box + ARRAY_SIZE(box);
	static void * const jump[] = {
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

	v = (damage != NULL) | clipped;
	goto *jump[v];

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

no_damage_clipped:
	{
		RegionRec clip;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			return TRUE;

		assert(dx + clip.extents.x1 >= 0);
		assert(dy + clip.extents.y1 >= 0);
		assert(dx + clip.extents.x2 <= pixmap->drawable.width);
		assert(dy + clip.extents.y2 <= pixmap->drawable.height);

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

damage_clipped:
	{
		RegionRec clip;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			return TRUE;

		assert(dx + clip.extents.x1 >= 0);
		assert(dy + clip.extents.y1 >= 0);
		assert(dx + clip.extents.x2 <= pixmap->drawable.width);
		assert(dy + clip.extents.y2 <= pixmap->drawable.height);

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

	if (gc)
		clipped = clip_box(&box, gc);
	if (box_empty(&box))
		return 0;

	*out = box;
	return 1 | clipped << 1;
}

static Bool
sna_poly_fill_rect_tiled_blt(DrawablePtr drawable,
			     struct kgem_bo *bo,
			     struct sna_damage **damage,
			     GCPtr gc, int n, xRectangle *rect,
			     const BoxRec *extents, unsigned clipped);

static bool
sna_poly_fill_rect_stippled_blt(DrawablePtr drawable,
				struct kgem_bo *bo,
				struct sna_damage **damage,
				GCPtr gc, int n, xRectangle *rect,
				const BoxRec *extents, unsigned clipped);

static void
sna_fill_spans(DrawablePtr drawable, GCPtr gc, int n,
	       DDXPointPtr pt, int *width, int sorted)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
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
		struct sna_pixmap *priv = sna_pixmap(pixmap);
		struct sna_damage **damage;

		DBG(("%s: trying solid fill [alu=%d, pixel=%08lx] blt paths\n",
		     __FUNCTION__, gc->alu, gc->fgPixel));

		if (sna_drawable_use_gpu_bo(drawable, &region.extents, &damage) &&
		    sna_fill_spans_blt(drawable,
				       priv->gpu_bo, damage,
				       gc, n, pt, width, sorted,
				       &region.extents, flags & 2))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &region.extents, &damage) &&
		    sna_fill_spans_blt(drawable,
				       priv->cpu_bo, damage,
				       gc, n, pt, width, sorted,
				       &region.extents, flags & 2))
			return;
	} else {
		struct sna_pixmap *priv = sna_pixmap(pixmap);
		struct sna_damage **damage;

		/* Try converting these to a set of rectangles instead */
		if (sna_drawable_use_gpu_bo(drawable, &region.extents, &damage)) {
			xRectangle *rect;
			int i;

			DBG(("%s: converting to rectagnles\n", __FUNCTION__));

			rect = malloc (n * sizeof (xRectangle));
			if (rect == NULL)
				return;

			for (i = 0; i < n; i++) {
				rect[i].x = pt[i].x - drawable->x;
				rect[i].width = width[i];
				rect[i].y = pt[i].y - drawable->y;
				rect[i].height = 1;
			}

			if (gc->fillStyle == FillTiled) {
				i = sna_poly_fill_rect_tiled_blt(drawable,
								 priv->gpu_bo, damage,
								 gc, n, rect,
								 &region.extents, flags & 2);
			} else {
				i = sna_poly_fill_rect_stippled_blt(drawable,
								    priv->gpu_bo, damage,
								    gc, n, rect,
								    &region.extents, flags & 2);
			}
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

	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;

	fbFillSpans(drawable, gc, n, pt, width, sorted);
out:
	RegionUninit(&region);
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

	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;

	fbSetSpans(drawable, gc, src, pt, width, n, sorted);
out:
	RegionUninit(&region);
}

static void
sna_copy_bitmap_blt(DrawablePtr _bitmap, DrawablePtr drawable, GCPtr gc,
		    BoxPtr box, int n,
		    int sx, int sy,
		    Bool reverse, Bool upsidedown, Pixel bitplane,
		    void *closure)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	PixmapPtr bitmap = (PixmapPtr)_bitmap;
	uint32_t br00, br13;
	int16_t dx, dy;

	DBG(("%s: plane=%x x%d\n", __FUNCTION__, (unsigned)bitplane, n));

	if (n == 0)
		return;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	if (closure)
		sna_damage_add_boxes(closure, box, n, dx, dy);

	br00 = 3 << 20;
	br13 = priv->gpu_bo->pitch;
	if (sna->kgem.gen >= 40) {
		if (priv->gpu_bo->tiling)
			br00 |= BLT_DST_TILED;
		br13 >>= 2;
	}
	br13 |= blt_depth(drawable->depth) << 24;
	br13 |= copy_ROP[gc->alu] << 16;

	kgem_set_mode(&sna->kgem, KGEM_BLT);
	do {
		int bx1 = (box->x1 + sx) & ~7;
		int bx2 = (box->x2 + sx + 7) & ~7;
		int bw = (bx2 - bx1)/8;
		int bh = box->y2 - box->y1;
		int bstride = ALIGN(bw, 2);
		int src_stride;
		uint8_t *dst, *src;
		uint32_t *b;

		DBG(("%s: box(%d, %d), (%d, %d), sx=(%d,%d) bx=[%d, %d]\n",
		     __FUNCTION__,
		     box->x1, box->y1,
		     box->x2, box->y2,
		     sx, sy, bx1, bx2));

		src_stride = bstride*bh;
		if (src_stride <= 128) {
			src_stride = ALIGN(src_stride, 8) / 4;
			if (!kgem_check_batch(&sna->kgem, 7+src_stride) ||
			    !kgem_check_bo_fenced(&sna->kgem, priv->gpu_bo, NULL) ||
			    !kgem_check_reloc(&sna->kgem, 1)) {
				_kgem_submit(&sna->kgem);
				_kgem_set_mode(&sna->kgem, KGEM_BLT);
			}

			b = sna->kgem.batch + sna->kgem.nbatch;
			b[0] = XY_MONO_SRC_COPY_IMM | (5 + src_stride) | br00;
			b[0] |= ((box->x1 + sx) & 7) << 17;
			b[1] = br13;
			b[2] = (box->y1 + dy) << 16 | (box->x1 + dx);
			b[3] = (box->y2 + dy) << 16 | (box->x2 + dx);
			b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4,
					      priv->gpu_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      0);
			b[5] = gc->bgPixel;
			b[6] = gc->fgPixel;

			sna->kgem.nbatch += 7 + src_stride;

			dst = (uint8_t *)&b[7];
			src_stride = bitmap->devKind;
			src = bitmap->devPrivate.ptr;
			src += (box->y1 + sy) * src_stride + bx1/8;
			src_stride -= bstride;
			do {
				int i = bstride;
				do {
					*dst++ = byte_reverse(*src++);
					*dst++ = byte_reverse(*src++);
					i -= 2;
				} while (i);
				src += src_stride;
			} while (--bh);
		} else {
			struct kgem_bo *upload;
			void *ptr;

			if (!kgem_check_batch(&sna->kgem, 8) ||
			    !kgem_check_bo_fenced(&sna->kgem, priv->gpu_bo, NULL) ||
			    !kgem_check_reloc(&sna->kgem, 2)) {
				_kgem_submit(&sna->kgem);
				_kgem_set_mode(&sna->kgem, KGEM_BLT);
			}

			upload = kgem_create_buffer(&sna->kgem,
						    bstride*bh,
						    KGEM_BUFFER_WRITE,
						    &ptr);
			if (!upload)
				break;

			b = sna->kgem.batch + sna->kgem.nbatch;
			b[0] = XY_MONO_SRC_COPY | br00;
			b[0] |= ((box->x1 + sx) & 7) << 17;
			b[1] = br13;
			b[2] = (box->y1 + dy) << 16 | (box->x1 + dx);
			b[3] = (box->y2 + dy) << 16 | (box->x2 + dx);
			b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4,
					      priv->gpu_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      0);
			b[5] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 5,
					      upload,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      KGEM_RELOC_FENCED,
					      0);
			b[6] = gc->bgPixel;
			b[7] = gc->fgPixel;

			sna->kgem.nbatch += 8;

			dst = ptr;
			src_stride = bitmap->devKind;
			src = bitmap->devPrivate.ptr;
			src += (box->y1 + sy) * src_stride + bx1/8;
			src_stride -= bstride;
			do {
				int i = bstride;
				do {
					*dst++ = byte_reverse(*src++);
					*dst++ = byte_reverse(*src++);
					i -= 2;
				} while (i);
				src += src_stride;
			} while (--bh);

			kgem_bo_destroy(&sna->kgem, upload);
		}

		box++;
	} while (--n);

	sna->blt_state.fill_bo = 0;
}

static void
sna_copy_plane_blt(DrawablePtr source, DrawablePtr drawable, GCPtr gc,
		   BoxPtr box, int n,
		   int sx, int sy,
		   Bool reverse, Bool upsidedown, Pixel bitplane,
		   void *closure)
{
	PixmapPtr dst_pixmap = get_drawable_pixmap(drawable);
	PixmapPtr src_pixmap = get_drawable_pixmap(source);
	struct sna_pixmap *priv = sna_pixmap(dst_pixmap);
	struct sna *sna = to_sna_from_pixmap(dst_pixmap);
	int16_t dx, dy;
	int bit = ffs(bitplane) - 1;
	uint32_t br00, br13;

	DBG(("%s: plane=%x [%d] x%d\n", __FUNCTION__,
	     (unsigned)bitplane, bit, n));

	if (n == 0)
		return;

	if (!sna_pixmap_move_to_cpu(src_pixmap, MOVE_READ))
		return;
	get_drawable_deltas(source, src_pixmap, &dx, &dy);
	sx += dx;
	sy += dy;

	get_drawable_deltas(drawable, dst_pixmap, &dx, &dy);
	if (closure)
		sna_damage_add_boxes(closure, box, n, dx, dy);

	br00 = XY_MONO_SRC_COPY;
	if (drawable->bitsPerPixel == 32)
		br00 |= 3 << 20;
	br13 = priv->gpu_bo->pitch;
	if (sna->kgem.gen >= 40) {
		if (priv->gpu_bo->tiling)
			br00 |= BLT_DST_TILED;
		br13 >>= 2;
	}
	br13 |= blt_depth(drawable->depth) << 24;
	br13 |= copy_ROP[gc->alu] << 16;

	kgem_set_mode(&sna->kgem, KGEM_BLT);
	do {
		int bx1 = (box->x1 + sx) & ~7;
		int bx2 = (box->x2 + sx + 7) & ~7;
		int bw = (bx2 - bx1)/8;
		int bh = box->y2 - box->y1;
		int bstride = ALIGN(bw, 2);
		uint32_t *b;
		struct kgem_bo *upload;
		void *ptr;

		DBG(("%s: box(%d, %d), (%d, %d), sx=(%d,%d) bx=[%d, %d]\n",
		     __FUNCTION__,
		     box->x1, box->y1,
		     box->x2, box->y2,
		     sx, sy, bx1, bx2));

		if (!kgem_check_batch(&sna->kgem, 8) ||
		    !kgem_check_bo_fenced(&sna->kgem, priv->gpu_bo, NULL) ||
		    !kgem_check_reloc(&sna->kgem, 2)) {
			_kgem_submit(&sna->kgem);
			_kgem_set_mode(&sna->kgem, KGEM_BLT);
		}

		upload = kgem_create_buffer(&sna->kgem,
					    bstride*bh,
					    KGEM_BUFFER_WRITE,
					    &ptr);
		if (!upload)
			break;

		switch (source->bitsPerPixel) {
		case 32:
			{
				uint32_t *src = src_pixmap->devPrivate.ptr;
				uint32_t src_stride = src_pixmap->devKind/sizeof(uint32_t);
				uint8_t *dst = ptr;

				src += (box->y1 + sy) * src_stride;
				src += bx1;

				src_stride -= bw * 8;
				bstride -= bw;

				do {
					int i = bw;
					do {
						uint8_t v = 0;

						v |= ((*src++ >> bit) & 1) << 7;
						v |= ((*src++ >> bit) & 1) << 6;
						v |= ((*src++ >> bit) & 1) << 5;
						v |= ((*src++ >> bit) & 1) << 4;
						v |= ((*src++ >> bit) & 1) << 3;
						v |= ((*src++ >> bit) & 1) << 2;
						v |= ((*src++ >> bit) & 1) << 1;
						v |= ((*src++ >> bit) & 1) << 0;

						*dst++ = v;
					} while (--i);
					dst += bstride;
					src += src_stride;
				} while (--bh);
				break;
			}
		case 16:
			{
				uint16_t *src = src_pixmap->devPrivate.ptr;
				uint16_t src_stride = src_pixmap->devKind/sizeof(uint16_t);
				uint8_t *dst = ptr;

				src += (box->y1 + sy) * src_stride;
				src += bx1;

				src_stride -= bw * 8;
				bstride -= bw;

				do {
					int i = bw;
					do {
						uint8_t v = 0;

						v |= ((*src++ >> bit) & 1) << 7;
						v |= ((*src++ >> bit) & 1) << 6;
						v |= ((*src++ >> bit) & 1) << 5;
						v |= ((*src++ >> bit) & 1) << 4;
						v |= ((*src++ >> bit) & 1) << 3;
						v |= ((*src++ >> bit) & 1) << 2;
						v |= ((*src++ >> bit) & 1) << 1;
						v |= ((*src++ >> bit) & 1) << 0;

						*dst++ = v;
					} while (--i);
					dst += bstride;
					src += src_stride;
				} while (--bh);
				break;
			}
		case 8:
			{
				uint8_t *src = src_pixmap->devPrivate.ptr;
				uint8_t src_stride = src_pixmap->devKind/sizeof(uint8_t);
				uint8_t *dst = ptr;

				src += (box->y1 + sy) * src_stride;
				src += bx1;

				src_stride -= bw * 8;
				bstride -= bw;

				do {
					int i = bw;
					do {
						uint8_t v = 0;

						v |= ((*src++ >> bit) & 1) << 7;
						v |= ((*src++ >> bit) & 1) << 6;
						v |= ((*src++ >> bit) & 1) << 5;
						v |= ((*src++ >> bit) & 1) << 4;
						v |= ((*src++ >> bit) & 1) << 3;
						v |= ((*src++ >> bit) & 1) << 2;
						v |= ((*src++ >> bit) & 1) << 1;
						v |= ((*src++ >> bit) & 1) << 0;

						*dst++ = v;
					} while (--i);
					dst += bstride;
					src += src_stride;
				} while (--bh);
				break;
			}
		default:
			assert(0);
			return;
		}

		b = sna->kgem.batch + sna->kgem.nbatch;
		b[0] = br00 | ((box->x1 + sx) & 7) << 17;
		b[1] = br13;
		b[2] = (box->y1 + dy) << 16 | (box->x1 + dx);
		b[3] = (box->y2 + dy) << 16 | (box->x2 + dx);
		b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4,
				      priv->gpu_bo,
				      I915_GEM_DOMAIN_RENDER << 16 |
				      I915_GEM_DOMAIN_RENDER |
				      KGEM_RELOC_FENCED,
				      0);
		b[5] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 5,
				      upload,
				      I915_GEM_DOMAIN_RENDER << 16 |
				      KGEM_RELOC_FENCED,
				      0);
		b[6] = gc->bgPixel;
		b[7] = gc->fgPixel;

		sna->kgem.nbatch += 8;
		kgem_bo_destroy(&sna->kgem, upload);

		box++;
	} while (--n);

	sna->blt_state.fill_bo = 0;
}

static RegionPtr
sna_copy_plane(DrawablePtr src, DrawablePtr dst, GCPtr gc,
	       int src_x, int src_y,
	       int w, int h,
	       int dst_x, int dst_y,
	       unsigned long bit)
{
	PixmapPtr pixmap = get_drawable_pixmap(dst);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	RegionRec region, *ret;
	struct sna_damage **damage;

	DBG(("%s: src=(%d, %d), dst=(%d, %d), size=%dx%d\n", __FUNCTION__,
	     src_x, src_y, dst_x, dst_y, w, h));

	if (src->bitsPerPixel == 1 && (bit&1) == 0)
		return miHandleExposures(src, dst, gc,
					 src_x, src_y,
					 w, h,
					 dst_x, dst_y, bit);

	region.extents.x1 = dst_x + dst->x;
	region.extents.y1 = dst_y + dst->y;
	region.extents.x2 = region.extents.x1 + w;
	region.extents.y2 = region.extents.y1 + h;
	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return NULL;

	if (wedged(sna))
		goto fallback;

	if (sna_drawable_use_gpu_bo(dst, &region.extents, &damage)) {
		struct sna_pixmap *priv = sna_pixmap(pixmap);
		if (priv->gpu_bo->tiling != I915_TILING_Y ||
		    sna_pixmap_change_tiling(pixmap, I915_TILING_X)) {
			RegionUninit(&region);
			return miDoCopy(src, dst, gc,
					src_x, src_y,
					w, h,
					dst_x, dst_y,
					src->depth == 1 ? sna_copy_bitmap_blt :sna_copy_plane_blt,
					(Pixel)bit, damage);
		}
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	ret = NULL;
	if (!sna_gc_move_to_cpu(gc, dst))
		goto out;

	if (!sna_drawable_move_region_to_cpu(dst, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;

	RegionTranslate(&region,
			src_x - dst_x - dst->x + src->x,
			src_y - dst_y - dst->y + src->y);
	if (!sna_drawable_move_region_to_cpu(src, &region, MOVE_READ))
		goto out;

	DBG(("%s: fbCopyPlane(%d, %d, %d, %d, %d,%d) %x\n",
	     __FUNCTION__, src_x, src_y, w, h, dst_x, dst_y, (unsigned)bit));
	ret = fbCopyPlane(src, dst, gc, src_x, src_y, w, h, dst_x, dst_y, bit);
out:
	RegionUninit(&region);
	return ret;
}

static Bool
sna_poly_point_blt(DrawablePtr drawable,
		   struct kgem_bo *bo,
		   struct sna_damage **damage,
		   GCPtr gc, int mode, int n, DDXPointPtr pt,
		   bool clipped)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
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
			unsigned nbox = n;
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
		RegionPtr clip = fbGetCompositeClip(gc);

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
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
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
		struct sna_pixmap *priv = sna_pixmap(pixmap);
		struct sna_damage **damage;

		DBG(("%s: trying solid fill [%08lx] blt paths\n",
		     __FUNCTION__, gc->fgPixel));

		if (sna_drawable_use_gpu_bo(drawable, &region.extents, &damage) &&
		    sna_poly_point_blt(drawable,
				       priv->gpu_bo, damage,
				       gc, mode, n, pt, flags & 2))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &region.extents, &damage) &&
		    sna_poly_point_blt(drawable,
				       priv->cpu_bo, damage,
				       gc, mode, n, pt, flags & 2))
			return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;

	DBG(("%s: fbPolyPoint\n", __FUNCTION__));
	fbPolyPoint(drawable, gc, mode, n, pt);
out:
	RegionUninit(&region);
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

	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
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
			   extents->x1, extents->y1,
			   extents->x2, extents->y2);

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
				   extents->x1, extents->y1,
				   extents->x2, extents->y2);
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
					if (length == 0)
						continue;

					if (pt1_clipped) {
						int clipdx = abs(x - x1);
						int clipdy = abs(y - y1);
						e += clipdy * e2 + (clipdx - clipdy) * e1;
					}
				}

				e3 = e2 - e1;
				e  = e - e1;

				if (sdx < 0) {
					x = x2;
					y = y2;
					sdy = -sdy;
				}

				b->x1 = x;
				b->y2 = b->y1 = y;
				while (length--) {
					e += e1;
					x++;
					if (e >= 0) {
						b->x2 = x;
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
						b->x1 = x;
					}
				}

				b->x2 = ++x;
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
					if (length == 0)
						continue;

					if (pt1_clipped) {
						int clipdx = abs(x - x1);
						int clipdy = abs(y - y1);
						e += clipdx * e2 + (clipdy - clipdx) * e1;
					}
				}

				e3 = e2 - e1;
				e  = e - e1;

				if (sdx < 0) {
					x = x2;
					y = y2;
					sdy = -sdy;
				}

				b->x2 = b->x1 = x;
				b->y1 = y;
				while (length--) {
					e += e1;
					y += sdy;
					if (e >= 0) {
						b->y2 = y;
						if (b->y2 < b->y1) {
							int16_t t = b->y1;
							b->y1 = b->y2;
							b->y2 = t;
						}
						b->x2++;
						if (++b == last_box) {
							ret = &&Y_continue;
							goto *jump;
Y_continue:
							b = box;
						}
						e += e3;
						b->x2 = b->x1 = ++x;
						b->y1 = y;
					}
				}

				b->y2 = y + sdy;
				if (b->y2 < b->y1) {
					int16_t t = b->y1;
					b->y1 = b->y2;
					b->y2 = t;
				}
				b->x2++;
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
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
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
	return 1 | blt << 2 | clip << 1;
}

static void
sna_poly_line(DrawablePtr drawable, GCPtr gc,
	      int mode, int n, DDXPointPtr pt)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_damage **damage;
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
	     flags & 4));

	if (!PM_IS_SOLID(drawable, gc->planemask))
		goto fallback;

	if (gc->lineStyle != LineSolid)
		goto spans_fallback;
	if (!(gc->lineWidth == 0 ||
	      (gc->lineWidth == 1 && (n == 1 || gc->alu == GXcopy))))
		goto spans_fallback;

	if (gc->fillStyle == FillSolid) {
		struct sna_pixmap *priv = sna_pixmap(pixmap);

		DBG(("%s: trying solid fill [%08lx]\n",
		     __FUNCTION__, gc->fgPixel));

		if (flags & 4) {
			if (sna_drawable_use_gpu_bo(drawable,
						    &region.extents,
						    &damage) &&
			    sna_poly_line_blt(drawable,
					      priv->gpu_bo, damage,
					      gc, mode, n, pt,
					      &region.extents, flags & 2))
				return;

			if (sna_drawable_use_cpu_bo(drawable,
						    &region.extents,
						    &damage) &&
			    sna_poly_line_blt(drawable,
					      priv->cpu_bo, damage,
					      gc, mode, n, pt,
					      &region.extents, flags & 2))
				return;
		} else { /* !rectilinear */
			if (USE_ZERO_SPANS &&
			    sna_drawable_use_gpu_bo(drawable,
						    &region.extents,
						    &damage) &&
			    sna_poly_zero_line_blt(drawable,
						   priv->gpu_bo, damage,
						   gc, mode, n, pt,
						   &region.extents, flags & 2))
				return;

		}
	} else if (flags & 4) {
		struct sna_pixmap *priv = sna_pixmap(pixmap);

		/* Try converting these to a set of rectangles instead */
		if (sna_drawable_use_gpu_bo(drawable, &region.extents, &damage)) {
			DDXPointRec p1, p2;
			xRectangle *rect;
			int i;

			DBG(("%s: converting to rectagnles\n", __FUNCTION__));

			rect = malloc (n * sizeof (xRectangle));
			if (rect == NULL)
				return;

			p1 = pt[0];
			for (i = 1; i < n; i++) {
				if (mode == CoordModePrevious) {
					p2.x = p1.x + pt[i].x;
					p2.y = p1.y + pt[i].y;
				} else {
					p2 = pt[i];
				}
				if (p1.x < p2.x) {
					rect[i].x = p1.x;
					rect[i].width = p2.x - p1.x + 1;
				} else if (p1.x > p2.x) {
					rect[i].x = p2.x;
					rect[i].width = p1.x - p2.x + 1;
				} else {
					rect[i].x = p1.x;
					rect[i].width = 1;
				}
				if (p1.y < p2.y) {
					rect[i].y = p1.y;
					rect[i].height = p2.y - p1.y + 1;
				} else if (p1.y > p2.y) {
					rect[i].y = p2.y;
					rect[i].height = p1.y - p2.y + 1;
				} else {
					rect[i].y = p1.y;
					rect[i].height = 1;
				}

				/* don't paint last pixel */
				if (gc->capStyle == CapNotLast) {
					if (p1.x == p2.x)
						rect[i].height--;
					else
						rect[i].width--;
				}
				p1 = p2;
			}

			if (gc->fillStyle == FillTiled) {
				i = sna_poly_fill_rect_tiled_blt(drawable,
								 priv->gpu_bo, damage,
								 gc, n - 1, rect + 1,
								 &region.extents, flags & 2);
			} else {
				i = sna_poly_fill_rect_stippled_blt(drawable,
								    priv->gpu_bo, damage,
								    gc, n - 1, rect + 1,
								    &region.extents, flags & 2);
			}
			free (rect);

			if (i)
				return;
		}
	}

spans_fallback:
	if (USE_SPANS &&
	    sna_drawable_use_gpu_bo(drawable, &region.extents, &damage)) {
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

	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;

	DBG(("%s: fbPolyLine\n", __FUNCTION__));
	fbPolyLine(drawable, gc, mode, n, pt);
out:
	RegionUninit(&region);
}

static Bool
sna_poly_segment_blt(DrawablePtr drawable,
		     struct kgem_bo *bo,
		     struct sna_damage **damage,
		     GCPtr gc, int n, xSegment *seg,
		     const BoxRec *extents, unsigned clipped)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	BoxRec boxes[512], *b = boxes, * const last_box = boxes + ARRAY_SIZE(boxes);
	struct sna_fill_op fill;
	int16_t dx, dy;

	DBG(("%s: n=%d, alu=%d, fg=%08lx, clipped=%d\n",
	     __FUNCTION__, n, gc->alu, gc->fgPixel, clipped));

	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	if (!clipped) {
		dx += drawable->x;
		dy += drawable->y;
		if (dx|dy) {
			do {
				unsigned nbox = n;
				if (nbox > ARRAY_SIZE(boxes))
					nbox = ARRAY_SIZE(boxes);
				n -= nbox;
				do {
					if (seg->x1 <= seg->x2) {
						b->x1 = seg->x1;
						b->x2 = seg->x2;
					} else {
						b->x1 = seg->x2;
						b->x2 = seg->x1;
					}
					b->x2++;

					if (seg->y1 <= seg->y2) {
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
				unsigned nbox = n;
				if (nbox > ARRAY_SIZE(boxes))
					nbox = ARRAY_SIZE(boxes);
				n -= nbox;
				do {
					if (seg->x1 <= seg->x2) {
						b->x1 = seg->x1;
						b->x2 = seg->x2;
					} else {
						b->x1 = seg->x2;
						b->x2 = seg->x1;
					}
					b->x2++;

					if (seg->y1 <= seg->y2) {
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

	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
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
					if (length == 0)
						continue;

					if (pt1_clipped) {
						int clipdx = abs(x1 - x);
						int clipdy = abs(y1 - y);
						e += clipdy * e2 + (clipdx - clipdy) * e1;
					}
				}
				e3 = e2 - e1;
				e  = e - e1;

				if (sdx < 0) {
					x1 = x2;
					y1 = y2;
					sdy = -sdy;
				}

				b->x1 = x1;
				b->y2 = b->y1 = y1;
				while (length--) {
					e += e1;
					x1++;
					if (e >= 0) {
						b->x2 = x1;
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
						b->x1 = x1;
					}
				}

				b->x2 = ++x1;
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
					if (length == 0)
						continue;

					if (pt1_clipped) {
						int clipdx = abs(x1 - x);
						int clipdy = abs(y1 - y);
						e += clipdx * e2 + (clipdy - clipdx) * e1;
					}
				}

				e3 = e2 - e1;
				e  = e - e1;

				if (sdx < 0) {
					x1 = x2;
					y1 = y2;
					sdy = -sdy;
				}

				b->x2 = b->x1 = x1;
				b->y1 = y1;
				while (length--) {
					e += e1;
					y1 += sdy;
					if (e >= 0) {
						b->y2 = y1;
						if (b->y2 < b->y1) {
							int16_t t = b->y1;
							b->y1 = b->y2;
							b->y2 = t;
						}
						b->x2++;
						if (++b == last_box) {
							ret = &&Y_continue;
							goto *jump;
Y_continue:
							b = box;
						}
						e += e3;
						b->x2 = b->x1 = ++x1;
						b->y1 = y1;
					}
				}

				b->y2 = y1 + sdy;
				if (b->y2 < b->y1) {
					int16_t t = b->y1;
					b->y1 = b->y2;
					b->y2 = t;
				}
				b->x2++;
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

	if (seg->x2 >= seg->x1) {
		box.x1 = seg->x1;
		box.x2 = seg->x2;
	} else {
		box.x2 = seg->x1;
		box.x1 = seg->x2;
	}

	if (seg->y2 >= seg->y1) {
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

	DBG(("%s: unclipped, untranslated extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__, box.x1, box.y1, box.x2, box.y2));

	clipped = trim_and_translate_box(&box, drawable, gc);
	if (box_empty(&box))
		return 0;

	*out = box;
	return 1 | clipped << 1 | can_blit << 2;
}

static void
sna_poly_segment(DrawablePtr drawable, GCPtr gc, int n, xSegment *seg)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_damage **damage;
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
	if (!PM_IS_SOLID(drawable, gc->planemask))
		goto fallback;
	if (gc->lineStyle != LineSolid || gc->lineWidth > 1)
		goto spans_fallback;
	if (gc->fillStyle == FillSolid) {
		struct sna_pixmap *priv = sna_pixmap(pixmap);

		DBG(("%s: trying blt solid fill [%08lx, flags=%x] paths\n",
		     __FUNCTION__, gc->fgPixel,flags));

		if (flags & 4) {
			if (sna_drawable_use_gpu_bo(drawable,
						    &region.extents,
						    &damage) &&
			    sna_poly_segment_blt(drawable,
						 priv->gpu_bo, damage,
						 gc, n, seg,
						 &region.extents, flags & 2))
				return;

			if (sna_drawable_use_cpu_bo(drawable,
						    &region.extents,
						    &damage) &&
			    sna_poly_segment_blt(drawable,
						 priv->cpu_bo, damage,
						 gc, n, seg,
						 &region.extents, flags & 2))
				return;
		} else {
			if (USE_ZERO_SPANS &&
			    sna_drawable_use_gpu_bo(drawable,
						    &region.extents,
						    &damage) &&
			    sna_poly_zero_segment_blt(drawable,
						      priv->gpu_bo, damage,
						      gc, n, seg, &region.extents, flags & 2))
				return;
		}
	} else if (flags & 4) {
		struct sna_pixmap *priv = sna_pixmap(pixmap);

		/* Try converting these to a set of rectangles instead */
		if (sna_drawable_use_gpu_bo(drawable, &region.extents, &damage)) {
			xRectangle *rect;
			int i;

			DBG(("%s: converting to rectagnles\n", __FUNCTION__));

			rect = malloc (n * sizeof (xRectangle));
			if (rect == NULL)
				return;

			for (i = 0; i < n; i++) {
				if (seg[i].x1 < seg[i].x2) {
					rect[i].x = seg[i].x1;
					rect[i].width = seg[i].x2 - seg[i].x1 + 1;
				} else if (seg[i].x1 > seg[i].x2) {
					rect[i].x = seg[i].x2;
					rect[i].width = seg[i].x1 - seg[i].x2 + 1;
				} else {
					rect[i].x = seg[i].x1;
					rect[i].width = 1;
				}
				if (seg[i].y1 < seg[i].y2) {
					rect[i].y = seg[i].y1;
					rect[i].height = seg[i].y2 - seg[i].y1 + 1;
				} else if (seg[i].x1 > seg[i].y2) {
					rect[i].y = seg[i].y2;
					rect[i].height = seg[i].y1 - seg[i].y2 + 1;
				} else {
					rect[i].y = seg[i].y1;
					rect[i].height = 1;
				}

				/* don't paint last pixel */
				if (gc->capStyle == CapNotLast) {
					if (seg[i].x1 == seg[i].x2)
						rect[i].height--;
					else
						rect[i].width--;
				}
			}

			if (gc->fillStyle == FillTiled) {
				i = sna_poly_fill_rect_tiled_blt(drawable,
								 priv->gpu_bo, damage,
								 gc, n, rect,
								 &region.extents, flags & 2);
			} else {
				i = sna_poly_fill_rect_stippled_blt(drawable,
								    priv->gpu_bo, damage,
								    gc, n, rect,
								    &region.extents, flags & 2);
			}
			free (rect);

			if (i)
				return;
		}
	}

	/* XXX Do we really want to base this decision on the amalgam ? */
spans_fallback:
	if (USE_SPANS &&
	    sna_drawable_use_gpu_bo(drawable, &region.extents, &damage)) {
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

	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;

	DBG(("%s: fbPolySegment\n", __FUNCTION__));
	fbPolySegment(drawable, gc, n, seg);
out:
	RegionUninit(&region);
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
		box32_add_rect(&box, ++r);

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
		       const BoxRec *extents, unsigned clipped)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_fill_op fill;
	BoxRec boxes[512], *b = boxes, *const last_box = boxes+ARRAY_SIZE(boxes);
	int16_t dx, dy;
	static void * const jump[] = {
		&&wide,
		&&zero,
		&&wide_clipped,
		&&zero_clipped,
	};

	DBG(("%s: n=%d, alu=%d, width=%d, fg=%08lx, damge=%p, clipped?=%d\n",
	     __FUNCTION__, n, gc->alu, gc->lineWidth, gc->fgPixel, damage, clipped));
	if (!sna_fill_init_blt(&fill, sna, pixmap, bo, gc->alu, gc->fgPixel))
		return FALSE;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);

	goto *jump[(gc->lineWidth <= 1) | clipped];

zero:
	dx += drawable->x;
	dy += drawable->y;

	do {
		xRectangle rr = *r++;

		DBG(("%s - zero : r[%d] = (%d, %d) x (%d, %d)\n", __FUNCTION__,
		     n, rr.x, rr.y, rr.width, rr.height));
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
			b[2].y2 = rr.y + rr.height;
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

				DBG(("%s - zero, clipped complex: r[%d] = (%d, %d) x (%d, %d)\n", __FUNCTION__,
				     n, rr.x, rr.y, rr.width, rr.height));
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
					box[2].y2 = rr.y + rr.height;
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
									sna_damage_add_boxes(damage, boxes, last_box-boxes, 0, 0);
								b = boxes;
							}
						}

					}
				}
			} while (--n);
		} else {
			do {
				xRectangle rr = *r++;
				DBG(("%s - zero, clip: r[%d] = (%d, %d) x (%d, %d)\n", __FUNCTION__,
				     n, rr.x, rr.y, rr.width, rr.height));
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
					box[2].y2 = rr.y + rr.height;
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
								sna_damage_add_boxes(damage, boxes, last_box-boxes, 0, 0);
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

					box[2] = box[1];
					box[3].x1 += rr.width;
					box[3].x2 += rr.width;

					box[3] = box[0];
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
									sna_damage_add_boxes(damage, boxes, last_box-boxes, 0, 0);
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

					box[2] = box[1];
					box[3].x1 += rr.width;
					box[3].x2 += rr.width;

					box[3] = box[0];
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
								sna_damage_add_boxes(damage, boxes, last_box-boxes, 0, 0);
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
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_damage **damage;
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
		struct sna_pixmap *priv = sna_pixmap(pixmap);

		DBG(("%s: trying blt solid fill [%08lx] paths\n",
		     __FUNCTION__, gc->fgPixel));

		if (sna_drawable_use_gpu_bo(drawable, &region.extents, &damage) &&
		    sna_poly_rectangle_blt(drawable, priv->gpu_bo, damage,
					   gc, n, r, &region.extents, flags&2))
			return;

		if (sna_drawable_use_cpu_bo(drawable, &region.extents, &damage) &&
		    sna_poly_rectangle_blt(drawable, priv->cpu_bo, damage,
					   gc, n, r, &region.extents, flags&2))
			return;
	}

	/* Not a trivial outline, but we still maybe able to break it
	 * down into simpler operations that we can accelerate.
	 */
	if (sna_drawable_use_gpu_bo(drawable, &region.extents, &damage)) {
		miPolyRectangle(drawable, gc, n, r);
		return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;

	DBG(("%s: fbPolyRectangle\n", __FUNCTION__));
	fbPolyRectangle(drawable, gc, n, r);
out:
	RegionUninit(&region);
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
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_damage **damage;
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

	if (!PM_IS_SOLID(drawable, gc->planemask))
		goto fallback;

	/* For "simple" cases use the miPolyArc to spans path */
	if (USE_SPANS && arc_to_spans(gc, n) &&
	    sna_drawable_use_gpu_bo(drawable, &region.extents, &damage)) {
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

	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;

	/* XXX may still fallthrough to miZeroPolyArc */
	DBG(("%s -- fbPolyArc\n", __FUNCTION__));
	fbPolyArc(drawable, gc, n, arc);
out:
	RegionUninit(&region);
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
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_fill_op fill;
	BoxRec boxes[512], *b = boxes, *const last_box = boxes+ARRAY_SIZE(boxes);
	int16_t dx, dy;

	DBG(("%s x %d [(%d, %d)x(%d, %d)...]+(%d,%d), clipped?=%d\n",
	     __FUNCTION__, n,
	     rect->x, rect->y, rect->width, rect->height,
	     drawable->x, drawable->y,
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
					if (r.x2 - r.x1 == pixmap->drawable.width &&
					    r.y2 - r.y1 == pixmap->drawable.height)
						sna_damage_all(damage,
							       pixmap->drawable.width,
							       pixmap->drawable.height);
					else
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
				unsigned nbox = n;
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
				unsigned nbox = n;
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
		if (b != boxes) {
			fill.boxes(sna, &fill, boxes, b-boxes);
			if (damage)
				sna_damage_add_boxes(damage, boxes, b-boxes, 0, 0);
		}
	}
done:
	fill.done(sna, &fill);
	return TRUE;
}

static uint32_t
get_pixel(PixmapPtr pixmap)
{
	DBG(("%s\n", __FUNCTION__));
	if (!sna_pixmap_move_to_cpu(pixmap, MOVE_READ))
		return 0;

	switch (pixmap->drawable.bitsPerPixel) {
	case 32: return *(uint32_t *)pixmap->devPrivate.ptr;
	case 16: return *(uint16_t *)pixmap->devPrivate.ptr;
	default: return *(uint8_t *)pixmap->devPrivate.ptr;
	}
}

static Bool
sna_poly_fill_rect_tiled_blt(DrawablePtr drawable,
			     struct kgem_bo *bo,
			     struct sna_damage **damage,
			     GCPtr gc, int n, xRectangle *rect,
			     const BoxRec *extents, unsigned clipped)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
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

	if (!sna_pixmap_move_to_gpu(tile, MOVE_READ))
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
								BoxRec b;

								b.x1 = dst_x + dx;
								b.y1 = dst_y + dy;
								b.x2 = b.x1 + w;
								b.y2 = b.y1 + h;

								assert_pixmap_contains_box(pixmap, &b);
								sna_damage_add_box(damage, &b);
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
								BoxRec b;

								b.x1 = dst_x + dx;
								b.y1 = dst_y + dy;
								b.x2 = b.x1 + w;
								b.y2 = b.y1 + h;

								assert_pixmap_contains_box(pixmap, &b);
								sna_damage_add_box(damage, &b);
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

static bool
sna_poly_fill_rect_stippled_8x8_blt(DrawablePtr drawable,
				    struct kgem_bo *bo,
				    struct sna_damage **damage,
				    GCPtr gc, int n, xRectangle *r,
				    const BoxRec *extents, unsigned clipped)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	uint32_t pat[2] = {0, 0}, br00, br13;
	int16_t dx, dy;

	DBG(("%s: alu=%d, upload (%d, %d), (%d, %d), origin (%d, %d)\n",
	     __FUNCTION__, gc->alu,
	     extents->x1, extents->y1,
	     extents->x2, extents->y2,
	     gc->patOrg.x, gc->patOrg.y));

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	{
		unsigned px = (0 - gc->patOrg.x - dx) & 7;
		unsigned py = (0 - gc->patOrg.y - dy) & 7;
		DBG(("%s: pat offset (%d, %d)\n", __FUNCTION__ ,px, py));
		br00 = XY_MONO_PAT | px << 12 | py << 8;
		if (drawable->bitsPerPixel == 32)
			br00 |= 3 << 20;

		br13 = bo->pitch;
		if (sna->kgem.gen >= 40) {
			if (bo->tiling)
				br00 |= BLT_DST_TILED;
			br13 >>= 2;
		}
		br13 |= (gc->fillStyle == FillStippled) << 28;
		br13 |= blt_depth(drawable->depth) << 24;
		br13 |= fill_ROP[gc->alu] << 16;
	}

	{
		uint8_t *dst = (uint8_t *)pat;
		const uint8_t *src = gc->stipple->devPrivate.ptr;
		int stride = gc->stipple->devKind;
		int j = gc->stipple->drawable.height;
		do {
			*dst++ = byte_reverse(*src);
			src += stride;
		} while (--j);
	}

	kgem_set_mode(&sna->kgem, KGEM_BLT);
	if (!clipped) {
		dx += drawable->x;
		dy += drawable->y;

		sna_damage_add_rectangles(damage, r, n, dx, dy);
		do {
			uint32_t *b;

			DBG(("%s: rect (%d, %d)x(%d, %d)\n",
			     __FUNCTION__, r->x + dx, r->y + dy, r->width, r->height));

			if (!kgem_check_batch(&sna->kgem, 9) ||
			    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
			    !kgem_check_reloc(&sna->kgem, 1)) {
				_kgem_submit(&sna->kgem);
				_kgem_set_mode(&sna->kgem, KGEM_BLT);
			}

			b = sna->kgem.batch + sna->kgem.nbatch;
			b[0] = br00;
			b[1] = br13;
			b[2] = (r->y + dy) << 16 | (r->x + dx);
			b[3] = (r->y + r->height + dy) << 16 | (r->x + r->width + dx);
			b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4, bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      0);
			b[5] = gc->bgPixel;
			b[6] = gc->fgPixel;
			b[7] = pat[0];
			b[8] = pat[1];
			sna->kgem.nbatch += 9;

			r++;
		} while (--n);
	} else {
		RegionRec clip;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			return true;

		/* XXX XY_SETUP_BLT + XY_SCANLINE_BLT */

		if (clip.data == NULL) {
			do {
				BoxRec box;

				box.x1 = r->x + drawable->x;
				box.y1 = r->y + drawable->y;
				box.x2 = bound(box.x1, r->width);
				box.y2 = bound(box.y1, r->height);
				r++;

				if (box_intersect(&box, &clip.extents)) {
					uint32_t *b;

					if (!kgem_check_batch(&sna->kgem, 9) ||
					    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
					    !kgem_check_reloc(&sna->kgem, 1)) {
						_kgem_submit(&sna->kgem);
						_kgem_set_mode(&sna->kgem, KGEM_BLT);
					}

					b = sna->kgem.batch + sna->kgem.nbatch;
					b[0] = br00;
					b[1] = br13;
					b[2] = (box.y1 + dy) << 16 | (box.x1 + dx);
					b[3] = (box.y2 + dy) << 16 | (box.x2 + dx);
					b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4, bo,
							      I915_GEM_DOMAIN_RENDER << 16 |
							      I915_GEM_DOMAIN_RENDER |
							      KGEM_RELOC_FENCED,
							      0);
					b[5] = gc->bgPixel;
					b[6] = gc->fgPixel;
					b[7] = pat[0];
					b[8] = pat[1];
					sna->kgem.nbatch += 9;
				}
			} while (n--);
		} else {
			const BoxRec * const clip_start = RegionBoxptr(&clip);
			const BoxRec * const clip_end = clip_start + clip.data->numRects;
			const BoxRec *c;

			do {
				BoxRec box;

				box.x1 = r->x + drawable->x;
				box.y1 = r->y + drawable->y;
				box.x2 = bound(box.x1, r->width);
				box.y2 = bound(box.y1, r->height);
				r++;

				c = find_clip_box_for_y(clip_start,
							clip_end,
							box.y1);
				while (c != clip_end) {
					BoxRec bb;
					if (box.y2 <= c->y1)
						break;

					bb = box;
					if (box_intersect(&bb, c++)) {
						uint32_t *b;

						if (!kgem_check_batch(&sna->kgem, 9) ||
						    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
						    !kgem_check_reloc(&sna->kgem, 1)) {
							_kgem_submit(&sna->kgem);
							_kgem_set_mode(&sna->kgem, KGEM_BLT);
						}

						b = sna->kgem.batch + sna->kgem.nbatch;
						b[0] = br00;
						b[1] = br13;
						b[2] = (bb.y1 + dy) << 16 | (bb.x1 + dx);
						b[3] = (bb.y2 + dy) << 16 | (bb.x2 + dx);
						b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4, bo,
								      I915_GEM_DOMAIN_RENDER << 16 |
								      I915_GEM_DOMAIN_RENDER |
								      KGEM_RELOC_FENCED,
								      0);
						b[5] = gc->bgPixel;
						b[6] = gc->fgPixel;
						b[7] = pat[0];
						b[8] = pat[1];
						sna->kgem.nbatch += 9;
					}

				}
			} while (--n);
		}
	}

	sna->blt_state.fill_bo = 0;
	return true;
}

static bool
sna_poly_fill_rect_stippled_nxm_blt(DrawablePtr drawable,
				    struct kgem_bo *bo,
				    struct sna_damage **damage,
				    GCPtr gc, int n, xRectangle *r,
				    const BoxRec *extents, unsigned clipped)
{
	PixmapPtr scratch, stipple;
	uint8_t bytes[8], *dst = bytes;
	const uint8_t *src, *end;
	int j, stride;
	bool ret;

	DBG(("%s: expanding %dx%d stipple to 8x8\n",
	     __FUNCTION__,
	     gc->stipple->drawable.width,
	     gc->stipple->drawable.height));

	scratch = GetScratchPixmapHeader(drawable->pScreen,
					 8, 8, 1, 1, 1, bytes);
	if (scratch == NullPixmap)
		return false;

	stipple = gc->stipple;
	gc->stipple = scratch;

	stride = stipple->devKind;
	src = stipple->devPrivate.ptr;
	end = src + stride * stipple->drawable.height;
	for(j = 0; j < 8; j++) {
		switch (stipple->drawable.width) {
		case 1: *dst = (*src & 1) * 0xff; break;
		case 2: *dst = (*src & 3) * 0x55; break;
		case 4: *dst = (*src & 15) * 0x11; break;
		case 8: *dst = *src; break;
		default: assert(0); break;
		}
		dst++;
		src += stride;
		if (src == end)
			src = stipple->devPrivate.ptr;
	}

	ret = sna_poly_fill_rect_stippled_8x8_blt(drawable, bo, damage,
						  gc, n, r, extents, clipped);

	gc->stipple = stipple;
	FreeScratchPixmapHeader(scratch);

	return ret;
}

static bool
sna_poly_fill_rect_stippled_1_blt(DrawablePtr drawable,
				  struct kgem_bo *bo,
				  struct sna_damage **damage,
				  GCPtr gc, int n, xRectangle *r,
				  const BoxRec *extents, unsigned clipped)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	PixmapPtr stipple = gc->stipple;
	const DDXPointRec *origin = &gc->patOrg;
	int16_t dx, dy;
	uint32_t br00, br13;

	DBG(("%s: upload (%d, %d), (%d, %d), origin (%d, %d)\n", __FUNCTION__,
	     extents->x1, extents->y1,
	     extents->x2, extents->y2,
	     origin->x, origin->y));

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	kgem_set_mode(&sna->kgem, KGEM_BLT);

	br00 = 3 << 20;
	br13 = bo->pitch;
	if (sna->kgem.gen >= 40) {
		if (bo->tiling)
			br00 |= BLT_DST_TILED;
		br13 >>= 2;
	}
	br13 |= (gc->fillStyle == FillStippled) << 29;
	br13 |= blt_depth(drawable->depth) << 24;
	br13 |= copy_ROP[gc->alu] << 16;

	if (!clipped) {
		dx += drawable->x;
		dy += drawable->y;

		sna_damage_add_rectangles(damage, r, n, dx, dy);
		do {
			int bx1 = (r->x - origin->x) & ~7;
			int bx2 = (r->x + r->width - origin->x + 7) & ~7;
			int bw = (bx2 - bx1)/8;
			int bh = r->height;
			int bstride = ALIGN(bw, 2);
			int src_stride;
			uint8_t *dst, *src;
			uint32_t *b;

			DBG(("%s: rect (%d, %d)x(%d, %d) stipple [%d,%d]\n",
			     __FUNCTION__,
			     r->x, r->y, r->width, r->height,
			     bx1, bx2));

			src_stride = bstride*bh;
			if (src_stride <= 128) {
				src_stride = ALIGN(src_stride, 8) / 4;
				if (!kgem_check_batch(&sna->kgem, 7+src_stride) ||
				    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
				    !kgem_check_reloc(&sna->kgem, 1)) {
					_kgem_submit(&sna->kgem);
					_kgem_set_mode(&sna->kgem, KGEM_BLT);
				}

				b = sna->kgem.batch + sna->kgem.nbatch;
				b[0] = XY_MONO_SRC_COPY_IMM | (5 + src_stride) | br00;
				b[0] |= ((r->x - origin->x) & 7) << 17;
				b[1] = br13;
				b[2] = (r->y + dy) << 16 | (r->x + dx);
				b[3] = (r->y + r->height + dy) << 16 | (r->x + r->width + dx);
				b[4] = kgem_add_reloc(&sna->kgem,
						      sna->kgem.nbatch + 4, bo,
						      I915_GEM_DOMAIN_RENDER << 16 |
						      I915_GEM_DOMAIN_RENDER |
						      KGEM_RELOC_FENCED,
						      0);
				b[5] = gc->bgPixel;
				b[6] = gc->fgPixel;

				sna->kgem.nbatch += 7 + src_stride;

				dst = (uint8_t *)&b[7];
				src_stride = stipple->devKind;
				src = stipple->devPrivate.ptr;
				src += (r->y - origin->y) * src_stride + bx1/8;
				src_stride -= bstride;
				do {
					int i = bstride;
					do {
						*dst++ = byte_reverse(*src++);
						*dst++ = byte_reverse(*src++);
						i -= 2;
					} while (i);
					src += src_stride;
				} while (--bh);
			} else {
				struct kgem_bo *upload;
				void *ptr;

				if (!kgem_check_batch(&sna->kgem, 8) ||
				    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
				    !kgem_check_reloc(&sna->kgem, 2)) {
					_kgem_submit(&sna->kgem);
					_kgem_set_mode(&sna->kgem, KGEM_BLT);
				}

				upload = kgem_create_buffer(&sna->kgem,
							    bstride*bh,
							    KGEM_BUFFER_WRITE,
							    &ptr);
				if (!upload)
					break;

				dst = ptr;
				src_stride = stipple->devKind;
				src = stipple->devPrivate.ptr;
				src += (r->y - origin->y) * src_stride + bx1/8;
				src_stride -= bstride;
				do {
					int i = bstride;
					do {
						*dst++ = byte_reverse(*src++);
						*dst++ = byte_reverse(*src++);
						i -= 2;
					} while (i);
					src += src_stride;
				} while (--bh);
				b = sna->kgem.batch + sna->kgem.nbatch;
				b[0] = XY_MONO_SRC_COPY | br00;
				b[0] |= ((r->x - origin->x) & 7) << 17;
				b[1] = br13;
				b[2] = (r->y + dy) << 16 | (r->x + dx);
				b[3] = (r->y + r->height + dy) << 16 | (r->x + r->width + dx);
				b[4] = kgem_add_reloc(&sna->kgem,
						      sna->kgem.nbatch + 4, bo,
						      I915_GEM_DOMAIN_RENDER << 16 |
						      I915_GEM_DOMAIN_RENDER |
						      KGEM_RELOC_FENCED,
						      0);
				b[5] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 5,
						      upload,
						      I915_GEM_DOMAIN_RENDER << 16 |
						      KGEM_RELOC_FENCED,
						      0);
				b[6] = gc->bgPixel;
				b[7] = gc->fgPixel;

				sna->kgem.nbatch += 8;
				kgem_bo_destroy(&sna->kgem, upload);
			}

			r++;
		} while (--n);
	} else {
		RegionRec clip;
		DDXPointRec pat;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			return true;

		pat.x = origin->x + drawable->x;
		pat.y = origin->y + drawable->y;

		if (clip.data == NULL) {
			do {
				BoxRec box;
				int bx1, bx2, bw, bh, bstride;
				int src_stride;
				uint8_t *dst, *src;
				uint32_t *b;
				struct kgem_bo *upload;
				void *ptr;

				box.x1 = r->x + drawable->x;
				box.x2 = bound(r->x, r->width);
				box.y1 = r->y + drawable->y;
				box.y2 = bound(r->y, r->height);
				r++;

				if (!box_intersect(&box, &clip.extents))
					continue;

				bx1 = (box.x1 - pat.x) & ~7;
				bx2 = (box.x2 - pat.x + 7) & ~7;
				bw = (bx2 - bx1)/8;
				bh = box.y2 - box.y1;
				bstride = ALIGN(bw, 2);

				DBG(("%s: rect (%d, %d)x(%d, %d), box (%d,%d),(%d,%d) stipple [%d,%d], pitch=%d, stride=%d\n",
				     __FUNCTION__,
				     r->x, r->y, r->width, r->height,
				     box.x1, box.y1, box.x2, box.y2,
				     bx1, bx2, bw, bstride));

				src_stride = bstride*bh;
				if (src_stride <= 128) {
					src_stride = ALIGN(src_stride, 8) / 4;
					if (!kgem_check_batch(&sna->kgem, 7+src_stride) ||
					    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
					    !kgem_check_reloc(&sna->kgem, 1)) {
						_kgem_submit(&sna->kgem);
						_kgem_set_mode(&sna->kgem, KGEM_BLT);
					}

					b = sna->kgem.batch + sna->kgem.nbatch;
					b[0] = XY_MONO_SRC_COPY_IMM | (5 + src_stride) | br00;
					b[0] |= ((box.x1 - pat.x) & 7) << 17;
					b[1] = br13;
					b[2] = (box.y1 + dy) << 16 | (box.x1 + dx);
					b[3] = (box.y2 + dy) << 16 | (box.x2 + dx);
					b[4] = kgem_add_reloc(&sna->kgem,
							      sna->kgem.nbatch + 4, bo,
							      I915_GEM_DOMAIN_RENDER << 16 |
							      I915_GEM_DOMAIN_RENDER |
							      KGEM_RELOC_FENCED,
							      0);
					b[5] = gc->bgPixel;
					b[6] = gc->fgPixel;

					sna->kgem.nbatch += 7 + src_stride;

					dst = (uint8_t *)&b[7];
					src_stride = stipple->devKind;
					src = stipple->devPrivate.ptr;
					src += (box.y1 - pat.y) * src_stride + bx1/8;
					src_stride -= bstride;
					do {
						int i = bstride;
						do {
							*dst++ = byte_reverse(*src++);
							*dst++ = byte_reverse(*src++);
							i -= 2;
						} while (i);
						src += src_stride;
					} while (--bh);
				} else {
					if (!kgem_check_batch(&sna->kgem, 8) ||
					    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
					    !kgem_check_reloc(&sna->kgem, 2)) {
						_kgem_submit(&sna->kgem);
						_kgem_set_mode(&sna->kgem, KGEM_BLT);
					}

					upload = kgem_create_buffer(&sna->kgem,
								    bstride*bh,
								    KGEM_BUFFER_WRITE,
								    &ptr);
					if (!upload)
						break;

					dst = ptr;
					src_stride = stipple->devKind;
					src = stipple->devPrivate.ptr;
					src += (box.y1 - pat.y) * src_stride + bx1/8;
					src_stride -= bstride;
					do {
						int i = bstride;
						do {
							*dst++ = byte_reverse(*src++);
							*dst++ = byte_reverse(*src++);
							i -= 2;
						} while (i);
						src += src_stride;
					} while (--bh);

					b = sna->kgem.batch + sna->kgem.nbatch;
					b[0] = XY_MONO_SRC_COPY | br00;
					b[0] |= ((box.x1 - pat.x) & 7) << 17;
					b[1] = br13;
					b[2] = (box.y1 + dy) << 16 | (box.x1 + dx);
					b[3] = (box.y2 + dy) << 16 | (box.x2 + dx);
					b[4] = kgem_add_reloc(&sna->kgem,
							      sna->kgem.nbatch + 4, bo,
							      I915_GEM_DOMAIN_RENDER << 16 |
							      I915_GEM_DOMAIN_RENDER |
							      KGEM_RELOC_FENCED,
							      0);
					b[5] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 5,
							      upload,
							      I915_GEM_DOMAIN_RENDER << 16 |
							      KGEM_RELOC_FENCED,
							      0);
					b[6] = gc->bgPixel;
					b[7] = gc->fgPixel;

					sna->kgem.nbatch += 8;
					kgem_bo_destroy(&sna->kgem, upload);
				}
			} while (--n);
		} else {
			const BoxRec * const clip_start = RegionBoxptr(&clip);
			const BoxRec * const clip_end = clip_start + clip.data->numRects;
			const BoxRec *c;

			do {
				BoxRec unclipped;
				int bx1, bx2, bw, bh, bstride;
				int src_stride;
				uint8_t *dst, *src;
				uint32_t *b;
				struct kgem_bo *upload;
				void *ptr;

				unclipped.x1 = r->x + drawable->x;
				unclipped.x2 = bound(r->x, r->width);
				unclipped.y1 = r->y + drawable->y;
				unclipped.y2 = bound(r->y, r->height);
				r++;

				c = find_clip_box_for_y(clip_start,
							clip_end,
							unclipped.y1);
				while (c != clip_end) {
					BoxRec box;

					if (unclipped.y2 <= c->y1)
						break;

					box = unclipped;
					if (!box_intersect(&box, c++))
						continue;

					bx1 = (box.x1 - pat.x) & ~7;
					bx2 = (box.x2 - pat.x + 7) & ~7;
					bw = (bx2 - bx1)/8;
					bh = box.y2 - box.y1;
					bstride = ALIGN(bw, 2);

					DBG(("%s: rect (%d, %d)x(%d, %d), box (%d,%d),(%d,%d) stipple [%d,%d]\n",
					     __FUNCTION__,
					     r->x, r->y, r->width, r->height,
					     box.x1, box.y1, box.x2, box.y2,
					     bx1, bx2));

					src_stride = bstride*bh;
					if (src_stride <= 128) {
						src_stride = ALIGN(src_stride, 8) / 4;
						if (!kgem_check_batch(&sna->kgem, 7+src_stride) ||
						    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
						    !kgem_check_reloc(&sna->kgem, 1)) {
							_kgem_submit(&sna->kgem);
							_kgem_set_mode(&sna->kgem, KGEM_BLT);
						}

						b = sna->kgem.batch + sna->kgem.nbatch;
						b[0] = XY_MONO_SRC_COPY_IMM | (5 + src_stride) | br00;
						b[0] |= ((box.x1 - pat.x) & 7) << 17;
						b[1] = br13;
						b[2] = (box.y1 + dy) << 16 | (box.x1 + dx);
						b[3] = (box.y2 + dy) << 16 | (box.x2 + dx);
						b[4] = kgem_add_reloc(&sna->kgem,
								      sna->kgem.nbatch + 4, bo,
								      I915_GEM_DOMAIN_RENDER << 16 |
								      I915_GEM_DOMAIN_RENDER |
								      KGEM_RELOC_FENCED,
								      0);
						b[5] = gc->bgPixel;
						b[6] = gc->fgPixel;

						sna->kgem.nbatch += 7 + src_stride;

						dst = (uint8_t *)&b[7];
						src_stride = stipple->devKind;
						src = stipple->devPrivate.ptr;
						src += (box.y1 - pat.y) * src_stride + bx1/8;
						src_stride -= bstride;
						do {
							int i = bstride;
							do {
								*dst++ = byte_reverse(*src++);
								*dst++ = byte_reverse(*src++);
								i -= 2;
							} while (i);
							src += src_stride;
						} while (--bh);
					} else {
						if (!kgem_check_batch(&sna->kgem, 8) ||
						    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
						    !kgem_check_reloc(&sna->kgem, 2)) {
							_kgem_submit(&sna->kgem);
							_kgem_set_mode(&sna->kgem, KGEM_BLT);
						}

						upload = kgem_create_buffer(&sna->kgem,
									    bstride*bh,
									    KGEM_BUFFER_WRITE,
									    &ptr);
						if (!upload)
							break;

						dst = ptr;
						src_stride = stipple->devKind;
						src = stipple->devPrivate.ptr;
						src += (box.y1 - pat.y) * src_stride + bx1/8;
						src_stride -= bstride;
						do {
							int i = bstride;
							do {
								*dst++ = byte_reverse(*src++);
								*dst++ = byte_reverse(*src++);
								i -= 2;
							} while (i);
							src += src_stride;
						} while (--bh);

						b = sna->kgem.batch + sna->kgem.nbatch;
						b[0] = XY_MONO_SRC_COPY | br00;
						b[0] |= ((box.x1 - pat.x) & 7) << 17;
						b[1] = br13;
						b[2] = (box.y1 + dy) << 16 | (box.x1 + dx);
						b[3] = (box.y2 + dy) << 16 | (box.x2 + dx);
						b[4] = kgem_add_reloc(&sna->kgem,
								      sna->kgem.nbatch + 4, bo,
								      I915_GEM_DOMAIN_RENDER << 16 |
								      I915_GEM_DOMAIN_RENDER |
								      KGEM_RELOC_FENCED,
								      0);
						b[5] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 5,
								      upload,
								      I915_GEM_DOMAIN_RENDER << 16 |
								      KGEM_RELOC_FENCED,
								      0);
						b[6] = gc->bgPixel;
						b[7] = gc->fgPixel;

						sna->kgem.nbatch += 8;
						kgem_bo_destroy(&sna->kgem, upload);
					}
				}
			} while (--n);

		}
	}

	sna->blt_state.fill_bo = 0;
	return true;
}

static void
sna_poly_fill_rect_stippled_n_box(struct sna *sna,
				  struct kgem_bo *bo,
				  uint32_t br00, uint32_t br13,
				  GCPtr gc,
				  BoxRec *box,
				  DDXPointRec *origin)
{
	int x1, x2, y1, y2;
	uint32_t *b;

	for (y1 = box->y1; y1 < box->y2; y1 = y2) {
		int oy = (y1 - origin->y)  % gc->stipple->drawable.height;

		y2 = box->y2;
		if (y2 - y1 > gc->stipple->drawable.height - oy)
			y2 = y1 + gc->stipple->drawable.height - oy;

		for (x1 = box->x1; x1 < box->x2; x1 = x2) {
			int bx1, bx2, bw, bh, len, ox;
			uint8_t *dst, *src;

			x2 = box->x2;
			ox = (x1 - origin->x) % gc->stipple->drawable.width;
			bx1 = ox & ~7;
			bx2 = ox + (x2 - x1);
			if (bx2 - bx1 > gc->stipple->drawable.width) {
				bx2 = bx1 + gc->stipple->drawable.width;
				x2 = x1 + (bx1-ox) + gc->stipple->drawable.width;
			}
			bx2 = (bx2 + 7) & ~7;
			bw = (bx2 - bx1)/8;
			bw = ALIGN(bw, 2);
			bh = y2 - y1;

			DBG(("%s: box(%d, %d), (%d, %d) pat=(%d, %d), up=(%d, %d)\n",
			     __FUNCTION__, x1, y1, x2, y2, ox, oy, bx1, bx2));

			len = bw*bh;
			len = ALIGN(len, 8) / 4;
			if (!kgem_check_batch(&sna->kgem, 7+len) ||
			    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
			    !kgem_check_reloc(&sna->kgem, 1)) {
				_kgem_submit(&sna->kgem);
				_kgem_set_mode(&sna->kgem, KGEM_BLT);
			}

			b = sna->kgem.batch + sna->kgem.nbatch;
			b[0] = br00 | (5 + len) | (ox & 7) << 17;
			b[1] = br13;
			b[2] = y1 << 16 | x1;
			b[3] = y2 << 16 | x2;
			b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4,
					      bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      0);
			b[5] = gc->bgPixel;
			b[6] = gc->fgPixel;

			sna->kgem.nbatch += 7 + len;

			dst = (uint8_t *)&b[7];
			len = gc->stipple->devKind;
			src = gc->stipple->devPrivate.ptr;
			src += oy*len + ox/8;
			len -= bw;
			do {
				int i = bw;
				do {
					*dst++ = byte_reverse(*src++);
					*dst++ = byte_reverse(*src++);
					i -= 2;
				} while (i);
				src += len;
			} while (--bh);
		}
	}
}

static bool
sna_poly_fill_rect_stippled_n_blt(DrawablePtr drawable,
				  struct kgem_bo *bo,
				  struct sna_damage **damage,
				  GCPtr gc, int n, xRectangle *r,
				  const BoxRec *extents, unsigned clipped)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	DDXPointRec origin = gc->patOrg;
	int16_t dx, dy;
	uint32_t br00, br13;

	DBG(("%s: upload (%d, %d), (%d, %d), origin (%d, %d), clipped=%d\n", __FUNCTION__,
	     extents->x1, extents->y1,
	     extents->x2, extents->y2,
	     origin.x, origin.y,
	     clipped));

	if (gc->stipple->drawable.width > 32 ||
	    gc->stipple->drawable.height > 32)
		return false;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	kgem_set_mode(&sna->kgem, KGEM_BLT);

	br00 = XY_MONO_SRC_COPY_IMM | 3 << 20;
	br13 = bo->pitch;
	if (sna->kgem.gen >= 40) {
		if (bo->tiling)
			br00 |= BLT_DST_TILED;
		br13 >>= 2;
	}
	br13 |= (gc->fillStyle == FillStippled) << 29;
	br13 |= blt_depth(drawable->depth) << 24;
	br13 |= copy_ROP[gc->alu] << 16;

	origin.x += dx + drawable->x;
	origin.y += dy + drawable->y;

	if (!clipped) {
		dx += drawable->x;
		dy += drawable->y;

		sna_damage_add_rectangles(damage, r, n, dx, dy);
		do {
			BoxRec box;

			box.x1 = r->x + dx;
			box.y1 = r->y + dy;
			box.x2 = box.x1 + r->width;
			box.y2 = box.y1 + r->height;

			sna_poly_fill_rect_stippled_n_box(sna, bo,
							  br00, br13, gc,
							  &box, &origin);
			r++;
		} while (--n);
	} else {
		RegionRec clip;

		region_set(&clip, extents);
		region_maybe_clip(&clip, gc->pCompositeClip);
		if (!RegionNotEmpty(&clip))
			return true;

		if (clip.data == NULL) {
			do {
				BoxRec box;

				box.x1 = r->x + drawable->x;
				box.x2 = bound(r->x, r->width);
				box.y1 = r->y + drawable->y;
				box.y2 = bound(r->y, r->height);
				r++;

				if (!box_intersect(&box, &clip.extents))
					continue;

				box.x1 += dx; box.x2 += dx;
				box.y1 += dy; box.y2 += dy;

				sna_poly_fill_rect_stippled_n_box(sna, bo,
								  br00, br13, gc,
								  &box, &origin);
			} while (--n);
		} else {
			const BoxRec * const clip_start = RegionBoxptr(&clip);
			const BoxRec * const clip_end = clip_start + clip.data->numRects;
			const BoxRec *c;

			do {
				BoxRec unclipped;

				unclipped.x1 = r->x + drawable->x;
				unclipped.x2 = bound(r->x, r->width);
				unclipped.y1 = r->y + drawable->y;
				unclipped.y2 = bound(r->y, r->height);
				r++;

				c = find_clip_box_for_y(clip_start,
							clip_end,
							unclipped.y1);
				while (c != clip_end) {
					BoxRec box;

					if (unclipped.y2 <= c->y1)
						break;

					box = unclipped;
					if (!box_intersect(&box, c++))
						continue;

					box.x1 += dx; box.x2 += dx;
					box.y1 += dy; box.y2 += dy;

					sna_poly_fill_rect_stippled_n_box(sna, bo,
									  br00, br13, gc,
									  &box, &origin);
				}
			} while (--n);
		}
	}

	sna->blt_state.fill_bo = 0;
	return true;
}

static bool
sna_poly_fill_rect_stippled_blt(DrawablePtr drawable,
				struct kgem_bo *bo,
				struct sna_damage **damage,
				GCPtr gc, int n, xRectangle *rect,
				const BoxRec *extents, unsigned clipped)
{

	PixmapPtr stipple = gc->stipple;

	if (bo->tiling == I915_TILING_Y) {
		PixmapPtr pixmap = get_drawable_pixmap(drawable);

		DBG(("%s: converting bo from Y-tiling\n", __FUNCTION__));
		/* This is cheating, but only the gpu_bo can be tiled */
		assert(bo == sna_pixmap(pixmap)->gpu_bo);
		bo = sna_pixmap_change_tiling(pixmap, I915_TILING_X);
		if (bo == NULL)
			return false;
	}

	if (!sna_drawable_move_to_cpu(&stipple->drawable, MOVE_READ))
		return false;

	DBG(("%s: origin (%d, %d), extents (stipple): (%d, %d), stipple size %dx%d\n",
	     __FUNCTION__, gc->patOrg.x, gc->patOrg.y,
	     extents->x2 - gc->patOrg.x - drawable->x,
	     extents->y2 - gc->patOrg.y - drawable->y,
	     stipple->drawable.width, stipple->drawable.height));

	if ((stipple->drawable.width | stipple->drawable.height) == 8)
		return sna_poly_fill_rect_stippled_8x8_blt(drawable, bo, damage,
							   gc, n, rect,
							   extents, clipped);

	if ((stipple->drawable.width | stipple->drawable.height) <= 0xc &&
	    is_power_of_two(stipple->drawable.width) &&
	    is_power_of_two(stipple->drawable.height))
		return sna_poly_fill_rect_stippled_nxm_blt(drawable, bo, damage,
							   gc, n, rect,
							   extents, clipped);

	if (extents->x2 - gc->patOrg.x - drawable->x <= stipple->drawable.width &&
	    extents->y2 - gc->patOrg.y - drawable->y <= stipple->drawable.height) {
		if (stipple->drawable.width <= 8 && stipple->drawable.height <= 8)
			return sna_poly_fill_rect_stippled_8x8_blt(drawable, bo, damage,
								   gc, n, rect,
								   extents, clipped);
		else
			return sna_poly_fill_rect_stippled_1_blt(drawable, bo, damage,
								 gc, n, rect,
								 extents, clipped);
	} else {
		return sna_poly_fill_rect_stippled_n_blt(drawable, bo, damage,
							 gc, n, rect,
							 extents, clipped);
	}
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

static bool alu_overwrites(uint8_t alu)
{
	switch (alu) {
	case GXclear:
	case GXcopy:
	case GXcopyInverted:
	case GXset:
		return true;
	default:
		return false;
	}
}

static void
sna_poly_fill_rect(DrawablePtr draw, GCPtr gc, int n, xRectangle *rect)
{
	PixmapPtr pixmap = get_drawable_pixmap(draw);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	struct sna_damage **damage;
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

	if (priv == NULL) {
		DBG(("%s: fallback -- unattached\n", __FUNCTION__));
		goto fallback;
	}

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (!PM_IS_SOLID(draw, gc->planemask))
		goto fallback;

	if (n == 1 && (flags & 2) == 0 &&
	    gc->fillStyle != FillStippled && alu_overwrites(gc->alu)) {
		region.data = NULL;
		sna_damage_subtract(&priv->cpu_damage, &region);
	}

	if (gc->fillStyle == FillSolid ||
	    (gc->fillStyle == FillTiled && gc->tileIsPixel) ||
	    (gc->fillStyle == FillOpaqueStippled && gc->bgPixel == gc->fgPixel)) {
		uint32_t color = gc->fillStyle == FillTiled ? gc->tile.pixel : gc->fgPixel;

		DBG(("%s: solid fill [%08x], testing for blt\n",
		     __FUNCTION__, color));

		if (sna_drawable_use_gpu_bo(draw, &region.extents, &damage) &&
		    sna_poly_fill_rect_blt(draw,
					   priv->gpu_bo, damage,
					   gc, color, n, rect,
					   &region.extents, flags & 2))
			return;

		if (sna_drawable_use_cpu_bo(draw, &region.extents, &damage) &&
		    sna_poly_fill_rect_blt(draw,
					   priv->cpu_bo, damage,
					   gc, color, n, rect,
					   &region.extents, flags & 2))
			return;
	} else if (gc->fillStyle == FillTiled) {
		DBG(("%s: tiled fill, testing for blt\n", __FUNCTION__));

		if (sna_drawable_use_gpu_bo(draw, &region.extents, &damage) &&
		    sna_poly_fill_rect_tiled_blt(draw,
						 priv->gpu_bo, damage,
						 gc, n, rect,
						 &region.extents, flags & 2))
			return;

		if (sna_drawable_use_cpu_bo(draw, &region.extents, &damage) &&
		    sna_poly_fill_rect_tiled_blt(draw,
						 priv->cpu_bo, damage,
						 gc, n, rect,
						 &region.extents, flags & 2))
			return;
	} else {
		DBG(("%s: stippled fill, testing for blt\n", __FUNCTION__));

		if (sna_drawable_use_gpu_bo(draw, &region.extents, &damage) &&
		    sna_poly_fill_rect_stippled_blt(draw,
						    priv->gpu_bo, damage,
						    gc, n, rect,
						    &region.extents, flags & 2))
			return;

		if (sna_drawable_use_cpu_bo(draw, &region.extents, &damage) &&
		    sna_poly_fill_rect_stippled_blt(draw,
						    priv->cpu_bo, damage,
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

	if (!sna_gc_move_to_cpu(gc, draw))
		goto out;

	flags = MOVE_WRITE;
	if (gc->fillStyle == FillStippled  ||
	    !(gc->alu == GXcopy || gc->alu == GXclear || gc->alu == GXset))
		flags |= MOVE_READ;
	if (!sna_drawable_move_region_to_cpu(draw, &region, flags))
		goto out;

	DBG(("%s: fallback - fbPolyFillRect\n", __FUNCTION__));
	if (region.data == NULL) {
		do {
			BoxRec box;

			box.x1 = rect->x + draw->x;
			box.y1 = rect->y + draw->y;
			box.x2 = bound(box.x1, rect->width);
			box.y2 = bound(box.y1, rect->height);
			rect++;

			if (box_intersect(&box, &region.extents)) {
				DBG(("%s: fallback - fbFill((%d, %d), (%d, %d))\n",
				     __FUNCTION__,
				     box.x1, box.y1,
				     box.x2-box.x1, box.y2-box.y1));
				fbFill(draw, gc,
				       box.x1, box.y1,
				       box.x2-box.x1, box.y2-box.y1);
			}
		} while (--n);
	} else {
		const BoxRec * const clip_start = RegionBoxptr(&region);
		const BoxRec * const clip_end = clip_start + region.data->numRects;
		const BoxRec *c;

		do {
			BoxRec box;

			box.x1 = rect->x + draw->x;
			box.y1 = rect->y + draw->y;
			box.x2 = bound(box.x1, rect->width);
			box.y2 = bound(box.y1, rect->height);
			rect++;

			c = find_clip_box_for_y(clip_start,
						clip_end,
						box.y1);

			while (c != clip_end) {
				BoxRec b;

				if (box.y2 <= c->y1)
					break;

				b = box;
				if (box_intersect(&b, c++)) {
					DBG(("%s: fallback - fbFill((%d, %d), (%d, %d))\n",
					     __FUNCTION__,
					       b.x1, b.y1,
					       b.x2-b.x1, b.y2-b.y1));
					fbFill(draw, gc,
					       b.x1, b.y1,
					       b.x2-b.x1, b.y2-b.y1);
				}
			}
		} while (--n);
	}
out:
	RegionUninit(&region);
}

struct sna_font {
	CharInfoRec glyphs8[256];
	CharInfoRec *glyphs16[256];
};

static Bool
sna_realize_font(ScreenPtr screen, FontPtr font)
{
	struct sna_font *priv;

	priv = calloc(1, sizeof(struct sna_font));
	if (priv == NULL)
		return FALSE;

	if (!FontSetPrivate(font, sna_font_key, priv)) {
		free(priv);
		return FALSE;
	}

	return TRUE;
}

static Bool
sna_unrealize_font(ScreenPtr screen, FontPtr font)
{
	struct sna_font *priv = FontGetPrivate(font, sna_font_key);
	int n;

	if (priv) {
		for (n = 0; n < 256; n++)
			free(priv->glyphs16[n]);
		free(priv);
	}

	return TRUE;
}

static bool
sna_glyph_blt(DrawablePtr drawable, GCPtr gc,
	      int _x, int _y, unsigned int _n,
	      CharInfoPtr *_info,
	      RegionRec *clip,
	      bool transparent)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	struct kgem_bo *bo;
	struct sna_damage **damage;
	const BoxRec *extents, *last_extents;
	uint32_t *b;
	int16_t dx, dy;
	uint32_t br00;

	uint8_t rop = transparent ? copy_ROP[gc->alu] : ROP_S;

	DBG(("%s (%d, %d) x %d, transparent? %d, alu=%d\n",
	     __FUNCTION__, _x, _y, _n, transparent, rop));

	if (wedged(sna)) {
		DBG(("%s -- fallback, wedged\n", __FUNCTION__));
		return false;
	}

	if (!sna_drawable_use_gpu_bo(drawable, &clip->extents, &damage))
		return false;

	bo = priv->gpu_bo;
	if (bo->tiling == I915_TILING_Y) {
		DBG(("%s: converting bo from Y-tiling\n", __FUNCTION__));
		bo = sna_pixmap_change_tiling(pixmap, I915_TILING_X);
		if (bo == NULL) {
			DBG(("%s -- fallback, dst uses Y-tiling\n", __FUNCTION__));
			return false;
		}
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	_x += drawable->x + dx;
	_y += drawable->y + dy;

	RegionTranslate(clip, dx, dy);
	extents = REGION_RECTS(clip);
	last_extents = extents + REGION_NUM_RECTS(clip);

	kgem_set_mode(&sna->kgem, KGEM_BLT);
	if (!kgem_check_batch(&sna->kgem, 16) ||
	    !kgem_check_bo_fenced(&sna->kgem, bo, NULL) ||
	    !kgem_check_reloc(&sna->kgem, 1)) {
		_kgem_submit(&sna->kgem);
		_kgem_set_mode(&sna->kgem, KGEM_BLT);
	}
	b = sna->kgem.batch + sna->kgem.nbatch;
	b[0] = XY_SETUP_BLT | 3 << 20;
	b[1] = bo->pitch;
	if (sna->kgem.gen >= 40) {
		if (bo->tiling)
			b[0] |= BLT_DST_TILED;
		b[1] >>= 2;
	}
	b[1] |= 1 << 30 | transparent << 29 | blt_depth(drawable->depth) << 24 | rop << 16;
	b[2] = extents->y1 << 16 | extents->x1;
	b[3] = extents->y2 << 16 | extents->x2;
	b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4, bo,
			      I915_GEM_DOMAIN_RENDER << 16 |
			      I915_GEM_DOMAIN_RENDER |
			      KGEM_RELOC_FENCED,
			      0);
	b[5] = gc->bgPixel;
	b[6] = gc->fgPixel;
	b[7] = 0;
	sna->kgem.nbatch += 8;

	br00 = XY_TEXT_IMMEDIATE_BLT;
	if (bo->tiling && sna->kgem.gen >= 40)
		br00 |= BLT_DST_TILED;

	do {
		CharInfoPtr *info = _info;
		int x = _x, y = _y, n = _n;

		do {
			CharInfoPtr c = *info++;
			int w = GLYPHWIDTHPIXELS(c);
			int h = GLYPHHEIGHTPIXELS(c);
			int w8 = (w + 7) >> 3;
			int x1, y1, len;

			if (c->bits == (void *)1)
				goto skip;

			len = (w8 * h + 7) >> 3 << 1;
			DBG(("%s glyph: (%d, %d) x (%d[%d], %d), len=%d\n" ,__FUNCTION__,
			     x,y, w, w8, h, len));

			x1 = x + c->metrics.leftSideBearing;
			y1 = y - c->metrics.ascent;

			if (x1 >= extents->x2 || y1 >= extents->y2)
				goto skip;
			if (x1 + w <= extents->x1 || y1 + h <= extents->y1)
				goto skip;

			if (!kgem_check_batch(&sna->kgem, 3+len)) {
				_kgem_submit(&sna->kgem);
				_kgem_set_mode(&sna->kgem, KGEM_BLT);

				b = sna->kgem.batch + sna->kgem.nbatch;
				b[0] = XY_SETUP_BLT | 3 << 20;
				b[1] = bo->pitch;
				if (sna->kgem.gen >= 40) {
					if (bo->tiling)
						b[0] |= BLT_DST_TILED;
					b[1] >>= 2;
				}
				b[1] |= 1 << 30 | transparent << 29 | blt_depth(drawable->depth) << 24 | rop << 16;
				b[2] = extents->y1 << 16 | extents->x1;
				b[3] = extents->y2 << 16 | extents->x2;
				b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4, bo,
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
			sna->kgem.nbatch += 3 + len;

			b[0] = br00 | (1 + len);
			b[1] = (uint16_t)y1 << 16 | (uint16_t)x1;
			b[2] = (uint16_t)(y1+h) << 16 | (uint16_t)(x1+w);
			 {
				uint64_t *src = (uint64_t *)c->bits;
				uint64_t *dst = (uint64_t *)(b + 3);
				do  {
					*dst++ = *src++;
					len -= 2;
				} while (len);
			}

			if (damage) {
				BoxRec r;

				r.x1 = x1;
				r.y1 = y1;
				r.x2 = x1 + w;
				r.y2 = y1 + h;
				if (box_intersect(&r, extents))
					sna_damage_add_box(damage, &r);
			}
skip:
			x += c->metrics.characterWidth;
		} while (--n);

		if (++extents == last_extents)
			break;

		if (kgem_check_batch(&sna->kgem, 3)) {
			b = sna->kgem.batch + sna->kgem.nbatch;
			sna->kgem.nbatch += 3;

			b[0] = XY_SETUP_CLIP;
			b[1] = extents->y1 << 16 | extents->x1;
			b[2] = extents->y2 << 16 | extents->x2;
		}
	} while (1);

	sna->blt_state.fill_bo = 0;
	return true;
}

static void
sna_glyph_extents(FontPtr font,
		  CharInfoPtr *info,
		  unsigned long count,
		  ExtentInfoRec *extents)
{
	extents->drawDirection = font->info.drawDirection;
	extents->fontAscent = font->info.fontAscent;
	extents->fontDescent = font->info.fontDescent;

	extents->overallAscent = info[0]->metrics.ascent;
	extents->overallDescent = info[0]->metrics.descent;
	extents->overallLeft = info[0]->metrics.leftSideBearing;
	extents->overallRight = info[0]->metrics.rightSideBearing;
	extents->overallWidth = info[0]->metrics.characterWidth;

	while (--count) {
		CharInfoPtr p =*++info;
		int v;

		if (p->metrics.ascent > extents->overallAscent)
			extents->overallAscent = p->metrics.ascent;
		if (p->metrics.descent > extents->overallDescent)
			extents->overallDescent = p->metrics.descent;

		v = extents->overallWidth + p->metrics.leftSideBearing;
		if (v < extents->overallLeft)
			extents->overallLeft = v;

		v = extents->overallWidth + p->metrics.rightSideBearing;
		if (v > extents->overallRight)
			extents->overallRight = v;

		extents->overallWidth += p->metrics.characterWidth;
	}
}

static bool sna_set_glyph(CharInfoPtr in, CharInfoPtr out)
{
	int w = GLYPHWIDTHPIXELS(in);
	int h = GLYPHHEIGHTPIXELS(in);
	int stride = GLYPHWIDTHBYTESPADDED(in);
	uint8_t *dst, *src;

	/* Skip empty glyphs */
	if (w == 0 || h == 0 || ((w|h) == 1 && (in->bits[0] & 1) == 0)) {
		out->bits = (void *)1;
		return true;
	}

	w = (w + 7) >> 3;

	out->metrics = in->metrics;
	out->bits = malloc((w*h + 7) & ~7);
	if (out->bits == NULL)
		return false;

	src = (uint8_t *)in->bits;
	dst = (uint8_t *)out->bits;
	stride -= w;
	do {
		int i = w;
		do {
			*dst++ = byte_reverse(*src++);
		} while (--i);
		src += stride;
	} while (--h);

	return true;
}

inline static bool sna_get_glyph8(FontPtr font, struct sna_font *priv,
				  uint8_t g, CharInfoPtr *out)
{
	unsigned long n;
	CharInfoPtr p, ret;

	p = &priv->glyphs8[g];
	if (p->bits) {
		*out = p;
		return p->bits != (void*)-1;
	}

	font->get_glyphs(font, 1, &g, Linear8Bit, &n, &ret);
	if (n == 0) {
		p->bits = (void*)-1;
		return false;
	}

	return sna_set_glyph(ret, *out = p);
}

inline static bool sna_get_glyph16(FontPtr font, struct sna_font *priv,
				   uint16_t g, CharInfoPtr *out)
{
	unsigned long n;
	CharInfoPtr page, p, ret;

	page = priv->glyphs16[g>>8];
	if (page == NULL)
		page = priv->glyphs16[g>>8] = calloc(256, sizeof(CharInfoRec));

	p = &page[g&0xff];
	if (p->bits) {
		*out = p;
		return p->bits != (void*)-1;
	}

	font->get_glyphs(font, 1, (unsigned char *)&g,
			 FONTLASTROW(font) ? TwoD16Bit : Linear16Bit,
			 &n, &ret);
	if (n == 0) {
		p->bits = (void*)-1;
		return false;
	}

	return sna_set_glyph(ret, *out = p);
}

static int
sna_poly_text8(DrawablePtr drawable, GCPtr gc,
	       int x, int y,
	       int count, char *chars)
{
	struct sna_font *priv = gc->font->devPrivates[sna_font_key];
	CharInfoPtr info[255];
	ExtentInfoRec extents;
	RegionRec region;
	long unsigned i, n;

	if (drawable->depth < 8)
		goto fallback;

	for (i = n = 0; i < count; i++) {
		if (sna_get_glyph8(gc->font, priv, chars[i], &info[n]))
			n++;
	}
	if (n == 0)
		return x;

	sna_glyph_extents(gc->font, info, n, &extents);
	region.extents.x1 = x + extents.overallLeft;
	region.extents.y1 = y - extents.overallAscent;
	region.extents.x2 = x + extents.overallRight;
	region.extents.y2 = y + extents.overallDescent;

	translate_box(&region.extents, drawable);
	clip_box(&region.extents, gc);
	if (box_empty(&region.extents))
		return x + extents.overallRight;

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return x + extents.overallRight;

	if (!sna_glyph_blt(drawable, gc, x, y, n, info, &region, true)) {
		DBG(("%s: fallback\n", __FUNCTION__));
		gc->font->get_glyphs(gc->font, count, (unsigned char *)chars,
				     Linear8Bit, &n, info);

		if (!sna_gc_move_to_cpu(gc, drawable))
			goto out;

		if (!sna_drawable_move_region_to_cpu(drawable, &region,
						     MOVE_READ | MOVE_WRITE))
			goto out;

		DBG(("%s: fallback -- fbPolyGlyphBlt\n", __FUNCTION__));
		fbPolyGlyphBlt(drawable, gc, x, y, n,
			       info, FONTGLYPHS(gc->font));
	}
out:
	RegionUninit(&region);
	return x + extents.overallRight;

fallback:
	DBG(("%s: fallback -- depth=%d\n", __FUNCTION__, drawable->depth));
	gc->font->get_glyphs(gc->font, count, (unsigned char *)chars,
			     Linear8Bit, &n, info);
	if (n == 0)
		return x;

	extents.overallWidth = x;
	for (i = 0; i < n; i++)
		extents.overallWidth += info[i]->metrics.characterWidth;

	DBG(("%s: fallback -- fbPolyGlyphBlt\n", __FUNCTION__));
	fbPolyGlyphBlt(drawable, gc, x, y, n, info, FONTGLYPHS(gc->font));

	return extents.overallWidth;
}

static int
sna_poly_text16(DrawablePtr drawable, GCPtr gc,
		int x, int y,
		int count, unsigned short *chars)
{
	struct sna_font *priv = gc->font->devPrivates[sna_font_key];
	CharInfoPtr info[255];
	ExtentInfoRec extents;
	RegionRec region;
	long unsigned i, n;

	if (drawable->depth < 8)
		goto fallback;

	for (i = n =  0; i < count; i++) {
		if (sna_get_glyph16(gc->font, priv, chars[i], &info[n]))
			n++;
	}
	if (n == 0)
		return x;

	sna_glyph_extents(gc->font, info, n, &extents);
	region.extents.x1 = x + extents.overallLeft;
	region.extents.y1 = y - extents.overallAscent;
	region.extents.x2 = x + extents.overallRight;
	region.extents.y2 = y + extents.overallDescent;

	translate_box(&region.extents, drawable);
	clip_box(&region.extents, gc);
	if (box_empty(&region.extents))
		return x + extents.overallRight;

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return x + extents.overallRight;

	if (!sna_glyph_blt(drawable, gc, x, y, n, info, &region, true)) {
		DBG(("%s: fallback\n", __FUNCTION__));
		gc->font->get_glyphs(gc->font, count, (unsigned char *)chars,
				     FONTLASTROW(gc->font) ? TwoD16Bit : Linear16Bit,
				     &n, info);

		if (!sna_gc_move_to_cpu(gc, drawable))
			goto out;
		if (!sna_drawable_move_region_to_cpu(drawable, &region,
						     MOVE_READ | MOVE_WRITE))
			goto out;

		DBG(("%s: fallback -- fbPolyGlyphBlt\n", __FUNCTION__));
		fbPolyGlyphBlt(drawable, gc, x, y, n,
			       info, FONTGLYPHS(gc->font));
	}
out:
	RegionUninit(&region);
	return x + extents.overallRight;

fallback:
	DBG(("%s: fallback -- depth=%d\n", __FUNCTION__, drawable->depth));
	gc->font->get_glyphs(gc->font, count, (unsigned char *)chars,
			     FONTLASTROW(gc->font) ? TwoD16Bit : Linear16Bit,
			     &n, info);
	if (n == 0)
		return x;

	extents.overallWidth = x;
	for (i = 0; i < n; i++)
		extents.overallWidth += info[i]->metrics.characterWidth;

	DBG(("%s: fallback -- fbPolyGlyphBlt\n", __FUNCTION__));
	fbPolyGlyphBlt(drawable, gc, x, y, n, info, FONTGLYPHS(gc->font));

	return extents.overallWidth;
}

static void
sna_image_text8(DrawablePtr drawable, GCPtr gc,
	       int x, int y,
	       int count, char *chars)
{
	struct sna_font *priv = gc->font->devPrivates[sna_font_key];
	CharInfoPtr info[255];
	ExtentInfoRec extents;
	RegionRec region;
	long unsigned i, n;

	if (drawable->depth < 8)
		goto fallback;

	for (i = n = 0; i < count; i++) {
		if (sna_get_glyph8(gc->font, priv, chars[i], &info[n]))
			n++;
	}
	if (n == 0)
		return;

	sna_glyph_extents(gc->font, info, n, &extents);
	region.extents.x1 = x + extents.overallLeft;
	region.extents.y1 = y - extents.overallAscent;
	region.extents.x2 = x + extents.overallRight;
	region.extents.y2 = y + extents.overallDescent;

	translate_box(&region.extents, drawable);
	clip_box(&region.extents, gc);
	if (box_empty(&region.extents))
		return;

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	if (!sna_glyph_blt(drawable, gc, x, y, n, info, &region, false)) {
		DBG(("%s: fallback\n", __FUNCTION__));
		gc->font->get_glyphs(gc->font, count, (unsigned char *)chars,
				     Linear8Bit, &n, info);

		if (!sna_gc_move_to_cpu(gc, drawable))
			goto out;
		if (!sna_drawable_move_region_to_cpu(drawable, &region,
						     MOVE_READ | MOVE_WRITE))
			goto out;

		DBG(("%s: fallback -- fbImageGlyphBlt\n", __FUNCTION__));
		fbImageGlyphBlt(drawable, gc, x, y, n,
				info, FONTGLYPHS(gc->font));
	}
out:
	RegionUninit(&region);
	return;

fallback:
	DBG(("%s: fallback, depth=%d\n", __FUNCTION__, drawable->depth));
	gc->font->get_glyphs(gc->font, count, (unsigned char *)chars,
			     Linear8Bit, &n, info);
	if (n) {
		DBG(("%s: fallback -- fbImageGlyphBlt\n", __FUNCTION__));
		fbImageGlyphBlt(drawable, gc, x, y, n, info, FONTGLYPHS(gc->font));
	}
}

static void
sna_image_text16(DrawablePtr drawable, GCPtr gc,
	       int x, int y,
	       int count, unsigned short *chars)
{
	struct sna_font *priv = gc->font->devPrivates[sna_font_key];
	CharInfoPtr info[255];
	ExtentInfoRec extents;
	RegionRec region;
	long unsigned i, n;

	if (drawable->depth < 8)
		goto fallback;

	for (i = n = 0; i < count; i++) {
		if (sna_get_glyph16(gc->font, priv, chars[i], &info[n]))
			n++;
	}
	if (n == 0)
		return;

	sna_glyph_extents(gc->font, info, n, &extents);
	region.extents.x1 = x + extents.overallLeft;
	region.extents.y1 = y - extents.overallAscent;
	region.extents.x2 = x + extents.overallRight;
	region.extents.y2 = y + extents.overallDescent;

	translate_box(&region.extents, drawable);
	clip_box(&region.extents, gc);
	if (box_empty(&region.extents))
		return;

	region.data = NULL;
	region_maybe_clip(&region, gc->pCompositeClip);
	if (!RegionNotEmpty(&region))
		return;

	if (!sna_glyph_blt(drawable, gc, x, y, n, info, &region, false)) {
		DBG(("%s: fallback\n", __FUNCTION__));
		gc->font->get_glyphs(gc->font, count, (unsigned char *)chars,
				     FONTLASTROW(gc->font) ? TwoD16Bit : Linear16Bit,
				     &n, info);

		if (!sna_gc_move_to_cpu(gc, drawable))
			goto out;
		if(!sna_drawable_move_region_to_cpu(drawable, &region,
						    MOVE_READ | MOVE_WRITE))
			goto out;

		DBG(("%s: fallback -- fbImageGlyphBlt\n", __FUNCTION__));
		fbImageGlyphBlt(drawable, gc, x, y, n,
				info, FONTGLYPHS(gc->font));
	}
out:
	RegionUninit(&region);
	return;

fallback:
	DBG(("%s: fallback -- depth=%d\n", __FUNCTION__, drawable->depth));
	gc->font->get_glyphs(gc->font, count, (unsigned char *)chars,
			     FONTLASTROW(gc->font) ? TwoD16Bit : Linear16Bit,
			     &n, info);
	if (n) {
		DBG(("%s: fallback -- fbImageGlyphBlt\n", __FUNCTION__));
		fbImageGlyphBlt(drawable, gc, x, y, n, info, FONTGLYPHS(gc->font));
	}
}

/* XXX Damage bypasses the Text interface and so we lose our custom gluphs */
static bool
sna_reversed_glyph_blt(DrawablePtr drawable, GCPtr gc,
		       int _x, int _y, unsigned int _n,
		       CharInfoPtr *_info, pointer _base,
		       struct sna_damage **damage,
		       RegionPtr clip,
		       bool transparent)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	const BoxRec *extents, *last_extents;
	uint32_t *b;
	int16_t dx, dy;
	uint8_t rop = transparent ? copy_ROP[gc->alu] : ROP_S;

	if (priv->gpu_bo->tiling == I915_TILING_Y) {
		DBG(("%s: converting bo from Y-tiling\n", __FUNCTION__));
		if (!sna_pixmap_change_tiling(pixmap, I915_TILING_X)) {
			DBG(("%s -- fallback, dst uses Y-tiling\n", __FUNCTION__));
			return false;
		}
	}

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	_x += drawable->x + dx;
	_y += drawable->y + dy;

	RegionTranslate(clip, dx, dy);
	extents = REGION_RECTS(clip);
	last_extents = extents + REGION_NUM_RECTS(clip);

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
			b[0] |= BLT_DST_TILED;
		b[1] >>= 2;
	}
	b[1] |= 1 << 30 | transparent << 29 | blt_depth(drawable->depth) << 24 | rop << 16;
	b[2] = extents->y1 << 16 | extents->x1;
	b[3] = extents->y2 << 16 | extents->x2;
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

	do {
		CharInfoPtr *info = _info;
		int x = _x, y = _y, n = _n;

		do {
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

			if (x1 >= extents->x2 || y1 >= extents->y2)
				goto skip;
			if (x1 + w <= extents->x1 || y1 + h <= extents->y1)
				goto skip;

			if (!kgem_check_batch(&sna->kgem, 3+len)) {
				_kgem_submit(&sna->kgem);
				_kgem_set_mode(&sna->kgem, KGEM_BLT);

				b = sna->kgem.batch + sna->kgem.nbatch;
				b[0] = XY_SETUP_BLT | 1 << 20;
				b[1] = priv->gpu_bo->pitch;
				if (sna->kgem.gen >= 40) {
					if (priv->gpu_bo->tiling)
						b[0] |= BLT_DST_TILED;
					b[1] >>= 2;
				}
				b[1] |= 1 << 30 | transparent << 29 | blt_depth(drawable->depth) << 24 | rop << 16;
				b[2] = extents->y1 << 16 | extents->x1;
				b[3] = extents->y2 << 16 | extents->x2;
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
			sna->kgem.nbatch += 3 + len;

			b[0] = XY_TEXT_IMMEDIATE_BLT | (1 + len);
			if (priv->gpu_bo->tiling && sna->kgem.gen >= 40)
				b[0] |= BLT_DST_TILED;
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
			assert((uint32_t *)byte == sna->kgem.batch + sna->kgem.nbatch);

			if (damage) {
				BoxRec r;

				r.x1 = x1;
				r.y1 = y1;
				r.x2 = x1 + w;
				r.y2 = y1 + h;
				if (box_intersect(&r, extents))
					sna_damage_add_box(damage, &r);
			}
skip:
			x += c->metrics.characterWidth;
		} while (--n);

		if (++extents == last_extents)
			break;

		if (kgem_check_batch(&sna->kgem, 3)) {
			b = sna->kgem.batch + sna->kgem.nbatch;
			sna->kgem.nbatch += 3;

			b[0] = XY_SETUP_CLIP;
			b[1] = extents->y1 << 16 | extents->x1;
			b[2] = extents->y2 << 16 | extents->x2;
		}
	} while (1);

	sna->blt_state.fill_bo = 0;
	return true;
}

static void
sna_image_glyph(DrawablePtr drawable, GCPtr gc,
		int x, int y, unsigned int n,
		CharInfoPtr *info, pointer base)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	ExtentInfoRec extents;
	RegionRec region;
	struct sna_damage **damage;

	if (n == 0)
		return;

	QueryGlyphExtents(gc->font, info, n, &extents);
	region.extents.x1 = x + extents.overallLeft;
	region.extents.y1 = y - extents.overallAscent;
	region.extents.x2 = x + extents.overallRight;
	region.extents.y2 = y + extents.overallDescent;

	translate_box(&region.extents, drawable);
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

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (sna_drawable_use_gpu_bo(drawable, &region.extents, &damage) &&
	    sna_reversed_glyph_blt(drawable, gc, x, y, n, info, base,
				   damage, &region, false)) {
		RegionUninit(&region);
		return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region, true))
		goto out;

	DBG(("%s: fallback -- fbImageGlyphBlt\n", __FUNCTION__));
	fbImageGlyphBlt(drawable, gc, x, y, n, info, base);

out:
	RegionUninit(&region);
}

static void
sna_poly_glyph(DrawablePtr drawable, GCPtr gc,
	       int x, int y, unsigned int n,
	       CharInfoPtr *info, pointer base)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	ExtentInfoRec extents;
	RegionRec region;
	struct sna_damage **damage;

	if (n == 0)
		return;

	QueryGlyphExtents(gc->font, info, n, &extents);
	region.extents.x1 = x + extents.overallLeft;
	region.extents.y1 = y - extents.overallAscent;
	region.extents.x2 = x + extents.overallRight;
	region.extents.y2 = y + extents.overallDescent;

	translate_box(&region.extents, drawable);
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

	if (FORCE_FALLBACK)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (sna_drawable_use_gpu_bo(drawable, &region.extents, &damage) &&
	    sna_reversed_glyph_blt(drawable, gc, x, y, n, info, base,
				   damage, &region, true)) {
		RegionUninit(&region);
		return;
	}

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;
	DBG(("%s: fallback -- fbPolyGlyphBlt\n", __FUNCTION__));
	fbPolyGlyphBlt(drawable, gc, x, y, n, info, base);

out:
	RegionUninit(&region);
}

static bool
sna_push_pixels_solid_blt(GCPtr gc,
			  PixmapPtr bitmap,
			  DrawablePtr drawable,
			  RegionPtr region)
{
	PixmapPtr pixmap = get_drawable_pixmap(drawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	struct sna_damage **damage;
	BoxRec *box;
	int16_t dx, dy;
	int n;
	uint8_t rop = copy_ROP[gc->alu];

	if (priv->gpu_bo->tiling == I915_TILING_Y) {
		DBG(("%s: converting bo from Y-tiling\n", __FUNCTION__));
		if (!sna_pixmap_change_tiling(pixmap, I915_TILING_X))
			return false;
	}

	if (!sna_drawable_use_gpu_bo(drawable, &region->extents, &damage))
		return false;

	get_drawable_deltas(drawable, pixmap, &dx, &dy);
	RegionTranslate(region, dx, dy);

	assert_pixmap_contains_box(pixmap, RegionExtents(region));
	if (damage)
		sna_damage_add(damage, region);

	DBG(("%s: upload(%d, %d, %d, %d)\n", __FUNCTION__,
	     region->extents.x1, region->extents.y1,
	     region->extents.x2, region->extents.y2));

	kgem_set_mode(&sna->kgem, KGEM_BLT);

	/* Region is pre-clipped and translated into pixmap space */
	box = REGION_RECTS(region);
	n = REGION_NUM_RECTS(region);
	do {
		int bx1 = (box->x1 - region->extents.x1) & ~7;
		int bx2 = (box->x2 - region->extents.x1 + 7) & ~7;
		int bw = (bx2 - bx1)/8;
		int bh = box->y2 - box->y1;
		int bstride = ALIGN(bw, 2);
		int src_stride;
		uint8_t *dst, *src;
		uint32_t *b;
		struct kgem_bo *upload;
		void *ptr;

		if (!kgem_check_batch(&sna->kgem, 8) ||
		    !kgem_check_bo_fenced(&sna->kgem, priv->gpu_bo, NULL) ||
		    !kgem_check_reloc(&sna->kgem, 2)) {
			_kgem_submit(&sna->kgem);
			_kgem_set_mode(&sna->kgem, KGEM_BLT);
		}

		upload = kgem_create_buffer(&sna->kgem,
					    bstride*bh,
					    KGEM_BUFFER_WRITE,
					    &ptr);
		if (!upload)
			break;

		dst = ptr;

		src_stride = bitmap->devKind;
		src = (uint8_t*)bitmap->devPrivate.ptr;
		src += (box->y1 - region->extents.y1) * src_stride + bx1/8;
		src_stride -= bstride;
		do {
			int i = bstride;
			do {
				*dst++ = byte_reverse(*src++);
				*dst++ = byte_reverse(*src++);
				i -= 2;
			} while (i);
			src += src_stride;
		} while (--bh);

		b = sna->kgem.batch + sna->kgem.nbatch;
		b[0] = XY_MONO_SRC_COPY;
		if (drawable->bitsPerPixel == 32)
			b[0] |= 3 << 20;
		b[0] |= ((box->x1 - region->extents.x1) & 7) << 17;
		b[1] = priv->gpu_bo->pitch;
		if (sna->kgem.gen >= 40) {
			if (priv->gpu_bo->tiling)
				b[0] |= BLT_DST_TILED;
			b[1] >>= 2;
		}
		b[1] |= 1 << 29;
		b[1] |= blt_depth(drawable->depth) << 24;
		b[1] |= rop << 16;
		b[2] = box->y1 << 16 | box->x1;
		b[3] = box->y2 << 16 | box->x2;
		b[4] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 4,
				      priv->gpu_bo,
				      I915_GEM_DOMAIN_RENDER << 16 |
				      I915_GEM_DOMAIN_RENDER |
				      KGEM_RELOC_FENCED,
				      0);
		b[5] = kgem_add_reloc(&sna->kgem, sna->kgem.nbatch + 5,
				      upload,
				      I915_GEM_DOMAIN_RENDER << 16 |
				      KGEM_RELOC_FENCED,
				      0);
		b[6] = gc->bgPixel;
		b[7] = gc->fgPixel;

		sna->kgem.nbatch += 8;
		kgem_bo_destroy(&sna->kgem, upload);

		box++;
	} while (--n);

	sna->blt_state.fill_bo = 0;
	return true;
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

	switch (gc->fillStyle) {
	case FillSolid:
		if (sna_push_pixels_solid_blt(gc, bitmap, drawable, &region))
			return;
		break;
	default:
		break;
	}

	DBG(("%s: fallback\n", __FUNCTION__));
	if (!sna_gc_move_to_cpu(gc, drawable))
		goto out;
	if (!sna_pixmap_move_to_cpu(bitmap, MOVE_READ))
		goto out;
	if (!sna_drawable_move_region_to_cpu(drawable, &region,
					     MOVE_READ | MOVE_WRITE))
		goto out;

	DBG(("%s: fallback, fbPushPixels(%d, %d, %d %d)\n",
	     __FUNCTION__, w, h, x, y));
	fbPushPixels(gc, bitmap, drawable, w, h, x, y);
out:
	RegionUninit(&region);
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
	sna_poly_text8,
	sna_poly_text16,
	sna_image_text8,
	sna_image_text16,
	sna_image_glyph,
	sna_poly_glyph,
	sna_push_pixels,
};

static void
sna_validate_gc(GCPtr gc, unsigned long changes, DrawablePtr drawable)
{
	DBG(("%s\n", __FUNCTION__));

	if (changes & (GCClipMask|GCSubwindowMode) ||
	    drawable->serialNumber != (gc->serialNumber & DRAWABLE_SERIAL_BITS) ||
	    (gc->clientClipType != CT_NONE && (changes & (GCClipXOrigin | GCClipYOrigin))))
		miComputeCompositeClip(gc, drawable);

	sna_gc(gc)->changes |= changes;
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

	if (!sna_drawable_move_region_to_cpu(drawable, &region, MOVE_READ))
		return;

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
	if (!sna_drawable_move_region_to_cpu(drawable, &region, MOVE_READ))
		return;

	fbGetSpans(drawable, wMax, pt, width, n, start);
}

static void
sna_copy_window(WindowPtr win, DDXPointRec origin, RegionPtr src)
{
	PixmapPtr pixmap = get_window_pixmap(win);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	RegionRec dst;
	int dx, dy;

	DBG(("%s origin=(%d, %d)\n", __FUNCTION__, origin.x, origin.y));

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		if (sna_pixmap_move_to_cpu(pixmap, MOVE_READ | MOVE_WRITE))
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
	bool ret = true;

	DBG(("%s\n", __FUNCTION__));

	/* Check if the fb layer wishes to modify the attached pixmaps,
	 * to fix up mismatches between the window and pixmap depths.
	 */
	if (mask & CWBackPixmap && win->backgroundState == BackgroundPixmap) {
		DBG(("%s: flushing background pixmap\n", __FUNCTION__));
		ret &= sna_validate_pixmap(&win->drawable, win->background.pixmap);
	}

	if (mask & CWBorderPixmap && win->borderIsPixel == FALSE) {
		DBG(("%s: flushing border pixmap\n", __FUNCTION__));
		ret &= sna_validate_pixmap(&win->drawable, win->border.pixmap);
	}

	return ret && fbChangeWindowAttributes(win, mask);
}

static void
sna_accel_flush_callback(CallbackListPtr *list,
			 pointer user_data, pointer call_data)
{
	struct sna *sna = user_data;

	if ((sna->kgem.sync|sna->kgem.flush) == 0 &&
	    list_is_empty(&sna->dirty_pixmaps))
		return;

	DBG(("%s\n", __FUNCTION__));

	/* flush any pending damage from shadow copies to tfp clients */
	while (!list_is_empty(&sna->dirty_pixmaps)) {
		struct sna_pixmap *priv = list_first_entry(&sna->dirty_pixmaps,
							   struct sna_pixmap,
							   list);
		sna_pixmap_move_to_gpu(priv->pixmap, MOVE_READ);
	}

	kgem_submit(&sna->kgem);
	sna->kgem.flush_now = 0;

	if (sna->kgem.sync) {
		kgem_sync(&sna->kgem);

		while (!list_is_empty(&sna->deferred_free)) {
			struct sna_pixmap *priv =
				list_first_entry(&sna->deferred_free,
						 struct sna_pixmap,
						 list);
			list_del(&priv->list);
			kgem_bo_destroy(&sna->kgem, priv->cpu_bo);
			fbDestroyPixmap(priv->pixmap);
			free(priv);
		}
	}
}

static void sna_deferred_free(struct sna *sna)
{
	struct sna_pixmap *priv, *next;

	list_for_each_entry_safe(priv, next, &sna->deferred_free, list) {
		if (kgem_bo_is_busy(priv->cpu_bo))
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

static void sna_accel_drain_timer(struct sna *sna, int id)
{
	if (sna->timer_active & (1<<id))
		read_timer(sna->timer[id]);
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

	priv = sna_accel_scanout(sna);
	if (priv == NULL) {
		DBG(("%s -- no scanout attached\n", __FUNCTION__));
		return FALSE;
	}

	if (sna->kgem.flush_now) {
		sna->kgem.flush_now = 0;
		if (priv->gpu_bo->exec) {
			DBG(("%s -- forcing flush\n", __FUNCTION__));
			sna_accel_drain_timer(sna, FLUSH_TIMER);
			return TRUE;
		}
	}

	return_if_timer_active(FLUSH_TIMER);

	if (priv->cpu_damage == NULL && priv->gpu_bo->exec == NULL) {
		DBG(("%s -- no pending write to scanout\n", __FUNCTION__));
		return FALSE;
	}

	if (sna->flags & SNA_NO_DELAYED_FLUSH)
		return TRUE;

	if (sna->timer[FLUSH_TIMER] == -1)
		return TRUE;

	DBG(("%s, starting flush timer, at time=%ld\n",
	     __FUNCTION__, (long)GetTimeInMillis()));

	/* Initial redraw hopefully before this vblank */
	to.it_value.tv_sec = 0;
	to.it_value.tv_nsec = sna->vblank_interval / 2;

	/* Then periodic updates for every vblank */
	to.it_interval.tv_sec = 0;
	to.it_interval.tv_nsec = sna->vblank_interval;
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

	to.it_interval.tv_sec = MAX_INACTIVE_TIME;
	to.it_interval.tv_nsec = 0;
	to.it_value = to.it_interval;
	timerfd_settime(sna->timer[EXPIRE_TIMER], 0, &to, NULL);

	sna->timer_active |= 1 << EXPIRE_TIMER;
	return FALSE;
}

static Bool sna_accel_do_inactive(struct sna *sna)
{
	struct itimerspec to;

	return_if_timer_active(INACTIVE_TIMER);

	if (list_is_empty(&sna->active_pixmaps))
		return FALSE;

	if (sna->timer[INACTIVE_TIMER] == -1)
		return FALSE;

	/* Periodic expiration after every 2 minutes. */
	to.it_interval.tv_sec = 120;
	to.it_interval.tv_nsec = 0;
	to.it_value = to.it_interval;
	timerfd_settime(sna->timer[INACTIVE_TIMER], 0, &to, NULL);

	sna->timer_active |= 1 << INACTIVE_TIMER;
	return FALSE;
}

static void sna_accel_create_timers(struct sna *sna)
{
	int id;

	/* XXX Can we replace this with OSTimer provided by dix? */

#ifdef CLOCK_MONOTONIC_COARSE
	for (id = 0; id < NUM_FINE_TIMERS; id++)
		sna->timer[id] = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	for (; id < NUM_TIMERS; id++) {
		sna->timer[id] = timerfd_create(CLOCK_MONOTONIC_COARSE, TFD_NONBLOCK);
		if (sna->timer[id] == -1)
			sna->timer[id] = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	}
#else
	for (id = 0; id < NUM_TIMERS; id++)
		sna->timer[id] = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
#endif
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
static Bool sna_accel_do_inactive(struct sna *sna) { return FALSE; }
static void sna_accel_drain_timer(struct sna *sna, int id) { }
static void _sna_accel_disarm_timer(struct sna *sna, int id) { }
#endif

static bool sna_accel_flush(struct sna *sna)
{
	struct sna_pixmap *priv = sna_accel_scanout(sna);
	bool nothing_to_do = priv->cpu_damage == NULL && sna->kgem.nbatch == 0;
	bool need_throttle = priv->gpu_bo->rq;

	DBG(("%s (time=%ld), nothing_to_do=%d, busy? %d\n",
	     __FUNCTION__, (long)GetTimeInMillis(),
	     nothing_to_do, sna->kgem.busy));

	if (nothing_to_do && !sna->kgem.busy)
		_sna_accel_disarm_timer(sna, FLUSH_TIMER);
	if (priv->cpu_damage)
		sna_pixmap_move_to_gpu(priv->pixmap, MOVE_READ);
	sna->kgem.busy = !nothing_to_do;
	kgem_bo_flush(&sna->kgem, priv->gpu_bo);
	sna->kgem.flush_now = 0;
	return need_throttle && !sna->kgem.busy;
}

static void sna_accel_expire(struct sna *sna)
{
	DBG(("%s (time=%ld)\n", __FUNCTION__, (long)GetTimeInMillis()));

	if (!kgem_expire_cache(&sna->kgem))
		_sna_accel_disarm_timer(sna, EXPIRE_TIMER);
}

static bool
sna_pixmap_free_gpu(struct sna *sna, struct sna_pixmap *priv)
{
	PixmapPtr pixmap = priv->pixmap;

	assert (!priv->flush);

	if (pixmap->devPrivate.ptr == NULL &&
	    !sna_pixmap_alloc_cpu(sna, pixmap, priv, priv->gpu_damage != NULL))
		return false;

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

	sna_damage_all(&priv->cpu_damage,
		       pixmap->drawable.width,
		       pixmap->drawable.height);
	sna_pixmap_destroy_gpu_bo(sna, priv);

	priv->source_count = SOURCE_BIAS;
	return true;
}

static void sna_accel_inactive(struct sna *sna)
{
	struct sna_pixmap *priv;
	struct list preserve;

	DBG(("%s (time=%ld)\n", __FUNCTION__, (long)GetTimeInMillis()));

#if DEBUG_ACCEL
	{
		unsigned count, bytes;

		count = bytes = 0;
		list_for_each_entry(priv, &sna->inactive_clock[1], inactive)
			if (!priv->pinned)
				count++, bytes += priv->gpu_bo->size;

		DBG(("%s: trimming %d inactive GPU buffers, %d bytes\n",
		    __FUNCTION__, count, bytes));

		count = bytes = 0;
		list_for_each_entry(priv, &sna->active_pixmaps, inactive) {
			if (priv->ptr &&
			    sna_damage_is_all(&priv->gpu_damage,
					      priv->pixmap->drawable.width,
					      priv->pixmap->drawable.height)) {
				count++, bytes += priv->pixmap->devKind * priv->pixmap->drawable.height;
			}
		}

		DBG(("%s: trimming %d inactive CPU buffers, %d bytes\n",
		    __FUNCTION__, count, bytes));
	}
#endif

	/* clear out the oldest inactive pixmaps */
	list_init(&preserve);
	while (!list_is_empty(&sna->inactive_clock[1])) {
		priv = list_first_entry(&sna->inactive_clock[1],
					struct sna_pixmap,
					inactive);
		/* XXX Rather than discarding the GPU buffer here, we
		 * could mark it purgeable and allow the shrinker to
		 * reap its storage only under memory pressure.
		 */
		list_del(&priv->inactive);
		if (priv->pinned)
			continue;

		if (priv->ptr &&
		    sna_damage_is_all(&priv->gpu_damage,
				      priv->pixmap->drawable.width,
				      priv->pixmap->drawable.height)) {
			DBG(("%s: discarding inactive CPU shadow\n",
			     __FUNCTION__));
			sna_pixmap_free_cpu(sna, priv);
			list_add(&priv->inactive, &preserve);
		} else {
			DBG(("%s: discarding inactive GPU bo handle=%d\n",
			     __FUNCTION__, priv->gpu_bo->handle));
			if (!sna_pixmap_free_gpu(sna, priv))
				list_add(&priv->inactive, &preserve);
		}
	}

	/* Age the current inactive pixmaps */
	sna->inactive_clock[1].next = sna->inactive_clock[0].next;
	sna->inactive_clock[0].next->prev = &sna->inactive_clock[1];
	sna->inactive_clock[0].prev->next = &sna->inactive_clock[1];
	sna->inactive_clock[1].prev = sna->inactive_clock[0].prev;

	sna->inactive_clock[0].next = sna->active_pixmaps.next;
	sna->active_pixmaps.next->prev = &sna->inactive_clock[0];
	sna->active_pixmaps.prev->next = &sna->inactive_clock[0];
	sna->inactive_clock[0].prev = sna->active_pixmaps.prev;

	sna->active_pixmaps.next = preserve.next;
	preserve.next->prev = &sna->active_pixmaps;
	preserve.prev->next = &sna->active_pixmaps;
	sna->active_pixmaps.prev = preserve.prev;

	if (list_is_empty(&sna->inactive_clock[1]) &&
	    list_is_empty(&sna->inactive_clock[0]) &&
	    list_is_empty(&sna->active_pixmaps))
		_sna_accel_disarm_timer(sna, INACTIVE_TIMER);
}

static void sna_accel_install_timers(struct sna *sna)
{
	int n;

	for (n = 0; n < NUM_TIMERS; n++) {
		if (sna->timer[n] != -1)
			AddGeneralSocket(sna->timer[n]);
	}
}

Bool sna_accel_pre_init(struct sna *sna)
{
	sna_accel_create_timers(sna);
	return TRUE;
}

Bool sna_accel_init(ScreenPtr screen, struct sna *sna)
{
	const char *backend;

	if (!AddCallback(&FlushCallback, sna_accel_flush_callback, sna))
		return FALSE;

	sna_font_key = AllocateFontPrivateIndex();
	screen->RealizeFont = sna_realize_font;
	screen->UnrealizeFont = sna_unrealize_font;

	list_init(&sna->deferred_free);
	list_init(&sna->dirty_pixmaps);
	list_init(&sna->active_pixmaps);
	list_init(&sna->inactive_clock[0]);
	list_init(&sna->inactive_clock[1]);

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
	sna->default_tiling = I915_TILING_X;
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

	if (!sna_composite_create(sna))
		return FALSE;

	return TRUE;
}

void sna_accel_close(struct sna *sna)
{
	if (sna->freed_pixmap) {
		assert(sna->freed_pixmap->refcnt == 1);
		free(sna_pixmap(sna->freed_pixmap));
		fbDestroyPixmap(sna->freed_pixmap);
		sna->freed_pixmap = NULL;
	}

	sna_composite_close(sna);
	sna_gradients_close(sna);
	sna_glyphs_close(sna);

	DeleteCallback(&FlushCallback, sna_accel_flush_callback, sna);

	kgem_cleanup_cache(&sna->kgem);
}

static void sna_accel_throttle(struct sna *sna)
{
	if (sna->flags & SNA_NO_THROTTLE)
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

	if (sna_accel_do_inactive(sna))
		sna_accel_inactive(sna);
}

void sna_accel_wakeup_handler(struct sna *sna)
{
	if (sna->kgem.need_retire)
		kgem_retire(&sna->kgem);
	if (sna->kgem.need_purge)
		kgem_purge_cache(&sna->kgem);

	sna_deferred_free(sna);
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
