/*
 * Based on code from intel_uxa.c and i830_xaa.c
 * Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright (c) 2005 Jesse Barnes <jbarnes@virtuousgeek.org>
 * Copyright (c) 2009-2011 Intel Corporation
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
#include "sna_render.h"
#include "sna_render_inline.h"
#include "sna_reg.h"

#include <mipict.h>
#include <fbpict.h>
#include <xaarop.h>

#if DEBUG_BLT
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

#define NO_BLT_COMPOSITE 0
#define NO_BLT_COPY 0
#define NO_BLT_COPY_BOXES 0
#define NO_BLT_FILL 0
#define NO_BLT_FILL_BOXES 0

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

static void nop_done(struct sna *sna, const struct sna_composite_op *op)
{
}

static void blt_done(struct sna *sna, const struct sna_composite_op *op)
{
	struct kgem *kgem = &sna->kgem;

	DBG(("%s: nbatch=%d\n", __FUNCTION__, kgem->nbatch));
	_kgem_set_mode(kgem, KGEM_BLT);
}

static bool sna_blt_fill_init(struct sna *sna,
			      struct sna_blt_state *blt,
			      struct kgem_bo *bo,
			      int bpp,
			      uint8_t alu,
			      uint32_t pixel)
{
	struct kgem *kgem = &sna->kgem;
	int pitch;


	blt->bo[0] = bo;

	blt->cmd = XY_COLOR_BLT_CMD;
	if (bpp == 32)
		blt->cmd |= BLT_WRITE_ALPHA | BLT_WRITE_RGB;

	pitch = bo->pitch;
	if (kgem->gen >= 40 && bo->tiling) {
		blt->cmd |= BLT_DST_TILED;
		pitch >>= 2;
	}
	if (pitch > MAXSHORT)
		return FALSE;

	blt->overwrites = alu == GXcopy || alu == GXclear;
	blt->br13 = (fill_ROP[alu] << 16) | pitch;
	switch (bpp) {
	default: assert(0);
	case 32: blt->br13 |= 1 << 25; /* RGB8888 */
	case 16: blt->br13 |= 1 << 24; /* RGB565 */
	case 8: break;
	}

	blt->pixel = pixel;

	kgem_set_mode(kgem, KGEM_BLT);
	if (!kgem_check_bo_fenced(kgem, bo, NULL))
		_kgem_submit(kgem);

	return TRUE;
}

static void sna_blt_fill_one(struct sna *sna,
			     const struct sna_blt_state *blt,
			     int x, int y,
			     int width, int height)
{
	struct kgem *kgem = &sna->kgem;
	uint32_t *b;

	DBG(("%s: (%d, %d) x (%d, %d): %08x\n",
	     __FUNCTION__, x, y, width, height, blt->pixel));

	assert(x >= 0);
	assert(y >= 0);
	assert((y+height) * blt->bo[0]->pitch <= blt->bo[0]->size);

	/* All too frequently one blt completely overwrites the previous */
	if (kgem->nbatch >= 6 &&
	    blt->overwrites &&
	    kgem->batch[kgem->nbatch-6] == blt->cmd &&
	    kgem->batch[kgem->nbatch-4] == (y << 16 | x) &&
	    kgem->batch[kgem->nbatch-3] == ((y+height) << 16 | (x+width)) &&
	    kgem->reloc[kgem->nreloc-1].target_handle == blt->bo[0]->handle) {
		DBG(("%s: replacing last fill\n", __FUNCTION__));
		kgem->batch[kgem->nbatch-5] = blt->br13;
		kgem->batch[kgem->nbatch-1] = blt->pixel;
		return;
	}

	if (!kgem_check_batch(kgem,  6) ||
	    kgem->nreloc + 1 > KGEM_RELOC_SIZE(kgem))
		_kgem_submit(kgem);

	b = kgem->batch + kgem->nbatch;
	b[0] = blt->cmd;
	b[1] = blt->br13;
	b[2] = (y << 16) | x;
	b[3] = ((y + height) << 16) | (x + width);
	b[4] = kgem_add_reloc(kgem, kgem->nbatch + 4,
			      blt->bo[0],
			      I915_GEM_DOMAIN_RENDER << 16 | I915_GEM_DOMAIN_RENDER | KGEM_RELOC_FENCED,
			      0);
	b[5] = blt->pixel;
	kgem->nbatch += 6;
}

static Bool sna_blt_copy_init(struct sna *sna,
			      struct sna_blt_state *blt,
			      struct kgem_bo *src,
			      struct kgem_bo *dst,
			      int bpp,
			      uint8_t alu)
{
	struct kgem *kgem = &sna->kgem;

	blt->bo[0] = src;
	blt->bo[1] = dst;

	blt->cmd = XY_SRC_COPY_BLT_CMD;
	if (bpp == 32)
		blt->cmd |= BLT_WRITE_ALPHA | BLT_WRITE_RGB;

	blt->pitch[0] = src->pitch;
	if (kgem->gen >= 40 && src->tiling) {
		blt->cmd |= BLT_SRC_TILED;
		blt->pitch[0] >>= 2;
	}
	if (blt->pitch[0] > MAXSHORT)
		return FALSE;

	blt->pitch[1] = dst->pitch;
	if (kgem->gen >= 40 && dst->tiling) {
		blt->cmd |= BLT_DST_TILED;
		blt->pitch[1] >>= 2;
	}
	if (blt->pitch[1] > MAXSHORT)
		return FALSE;

	blt->overwrites = alu == GXcopy || alu == GXclear;
	blt->br13 = (copy_ROP[alu] << 16) | blt->pitch[1];
	switch (bpp) {
	default: assert(0);
	case 32: blt->br13 |= 1 << 25; /* RGB8888 */
	case 16: blt->br13 |= 1 << 24; /* RGB565 */
	case 8: break;
	}

	kgem_set_mode(kgem, KGEM_BLT);
	if (!kgem_check_bo_fenced(kgem, src, dst, NULL))
		_kgem_submit(kgem);

	return TRUE;
}

static void sna_blt_copy_one(struct sna *sna,
			     const struct sna_blt_state *blt,
			     int src_x, int src_y,
			     int width, int height,
			     int dst_x, int dst_y)
{
	struct kgem *kgem = &sna->kgem;
	uint32_t *b;

	DBG(("%s: (%d, %d) -> (%d, %d) x (%d, %d)\n",
	     __FUNCTION__, src_x, src_y, dst_x, dst_y, width, height));

	assert(src_x >= 0);
	assert(src_y >= 0);
	assert((src_y + height) * blt->bo[0]->pitch <= blt->bo[0]->size);
	assert(dst_x >= 0);
	assert(dst_y >= 0);
	assert((dst_y + height) * blt->bo[1]->pitch <= blt->bo[1]->size);
	assert(width > 0);
	assert(height > 0);

	/* Compare against a previous fill */
	if (kgem->nbatch >= 6 &&
	    blt->overwrites &&
	    kgem->batch[kgem->nbatch-6] == ((blt->cmd & ~XY_SRC_COPY_BLT_CMD) | XY_COLOR_BLT_CMD) &&
	    kgem->batch[kgem->nbatch-4] == (dst_y << 16 | dst_x) &&
	    kgem->batch[kgem->nbatch-3] == ((dst_y+height) << 16 | (dst_x+width)) &&
	    kgem->reloc[kgem->nreloc-1].target_handle == blt->bo[1]->handle) {
		DBG(("%s: replacing last fill\n", __FUNCTION__));
		b = kgem->batch + kgem->nbatch - 6;
		b[0] = blt->cmd;
		b[1] = blt->br13;
		b[5] = (src_y << 16) | src_x;
		b[6] = blt->pitch[0];
		b[7] = kgem_add_reloc(kgem, kgem->nbatch + 7 - 6,
				      blt->bo[0],
				      I915_GEM_DOMAIN_RENDER << 16 | KGEM_RELOC_FENCED,
				      0);
		kgem->nbatch += 8 - 6;
		return;
	}

	if (kgem->nbatch + 8 + KGEM_BATCH_RESERVED > kgem->surface ||
	    kgem->nreloc + 2 > KGEM_RELOC_SIZE(kgem))
		_kgem_submit(kgem);

	b = kgem->batch + kgem->nbatch;
	b[0] = blt->cmd;
	b[1] = blt->br13;
	b[2] = (dst_y << 16) | dst_x;
	b[3] = ((dst_y + height) << 16) | (dst_x + width);
	b[4] = kgem_add_reloc(kgem, kgem->nbatch + 4,
			      blt->bo[1],
			      I915_GEM_DOMAIN_RENDER << 16 | I915_GEM_DOMAIN_RENDER | KGEM_RELOC_FENCED,
			      0);
	b[5] = (src_y << 16) | src_x;
	b[6] = blt->pitch[0];
	b[7] = kgem_add_reloc(kgem, kgem->nbatch + 7,
			      blt->bo[0],
			      I915_GEM_DOMAIN_RENDER << 16 | KGEM_RELOC_FENCED,
			      0);
	kgem->nbatch += 8;
}

static Bool
get_rgba_from_pixel(uint32_t pixel,
		    uint16_t *red,
		    uint16_t *green,
		    uint16_t *blue,
		    uint16_t *alpha,
		    uint32_t format)
{
	int rbits, bbits, gbits, abits;
	int rshift, bshift, gshift, ashift;

	rbits = PICT_FORMAT_R(format);
	gbits = PICT_FORMAT_G(format);
	bbits = PICT_FORMAT_B(format);
	abits = PICT_FORMAT_A(format);

	if (PICT_FORMAT_TYPE(format) == PICT_TYPE_A) {
		rshift = gshift = bshift = ashift = 0;
	} else if (PICT_FORMAT_TYPE(format) == PICT_TYPE_ARGB) {
		bshift = 0;
		gshift = bbits;
		rshift = gshift + gbits;
		ashift = rshift + rbits;
	} else if (PICT_FORMAT_TYPE(format) == PICT_TYPE_ABGR) {
		rshift = 0;
		gshift = rbits;
		bshift = gshift + gbits;
		ashift = bshift + bbits;
	} else if (PICT_FORMAT_TYPE(format) == PICT_TYPE_BGRA) {
		ashift = 0;
		rshift = abits;
		if (abits == 0)
			rshift = PICT_FORMAT_BPP(format) - (rbits+gbits+bbits);
		gshift = rshift + rbits;
		bshift = gshift + gbits;
	} else {
		return FALSE;
	}

	if (rbits) {
		*red = ((pixel >> rshift) & ((1 << rbits) - 1)) << (16 - rbits);
		while (rbits < 16) {
			*red |= *red >> rbits;
			rbits <<= 1;
		}
	} else
		*red = 0;

	if (gbits) {
		*green = ((pixel >> gshift) & ((1 << gbits) - 1)) << (16 - gbits);
		while (gbits < 16) {
			*green |= *green >> gbits;
			gbits <<= 1;
		}
	} else
		*green = 0;

	if (bbits) {
		*blue = ((pixel >> bshift) & ((1 << bbits) - 1)) << (16 - bbits);
		while (bbits < 16) {
			*blue |= *blue >> bbits;
			bbits <<= 1;
		}
	} else
		*blue = 0;

	if (abits) {
		*alpha = ((pixel >> ashift) & ((1 << abits) - 1)) << (16 - abits);
		while (abits < 16) {
			*alpha |= *alpha >> abits;
			abits <<= 1;
		}
	} else
		*alpha = 0xffff;

	return TRUE;
}

Bool
sna_get_pixel_from_rgba(uint32_t * pixel,
			uint16_t red,
			uint16_t green,
			uint16_t blue,
			uint16_t alpha,
			uint32_t format)
{
	int rbits, bbits, gbits, abits;
	int rshift, bshift, gshift, ashift;

	rbits = PICT_FORMAT_R(format);
	gbits = PICT_FORMAT_G(format);
	bbits = PICT_FORMAT_B(format);
	abits = PICT_FORMAT_A(format);
	if (abits == 0)
	    abits = PICT_FORMAT_BPP(format) - (rbits+gbits+bbits);

	if (PICT_FORMAT_TYPE(format) == PICT_TYPE_A) {
		*pixel = alpha >> (16 - abits);
		return TRUE;
	}

	if (!PICT_FORMAT_COLOR(format))
		return FALSE;

	if (PICT_FORMAT_TYPE(format) == PICT_TYPE_ARGB) {
		bshift = 0;
		gshift = bbits;
		rshift = gshift + gbits;
		ashift = rshift + rbits;
	} else if (PICT_FORMAT_TYPE(format) == PICT_TYPE_ABGR) {
		rshift = 0;
		gshift = rbits;
		bshift = gshift + gbits;
		ashift = bshift + bbits;
	} else if (PICT_FORMAT_TYPE(format) == PICT_TYPE_BGRA) {
		ashift = 0;
		rshift = abits;
		gshift = rshift + rbits;
		bshift = gshift + gbits;
	} else
		return FALSE;

	*pixel = 0;
	*pixel |= (blue  >> (16 - bbits)) << bshift;
	*pixel |= (green >> (16 - gbits)) << gshift;
	*pixel |= (red   >> (16 - rbits)) << rshift;
	*pixel |= (alpha >> (16 - abits)) << ashift;

	return TRUE;
}

static uint32_t
color_convert(uint32_t pixel,
	      uint32_t src_format,
	      uint32_t dst_format)
{
	DBG(("%s: src=%08x [%08x]\n", __FUNCTION__, pixel, src_format));

	if (src_format != dst_format) {
		uint16_t red, green, blue, alpha;

		if (!get_rgba_from_pixel(pixel,
					 &red, &green, &blue, &alpha,
					 src_format))
			return 0;

		if (!sna_get_pixel_from_rgba(&pixel,
					     red, green, blue, alpha,
					     dst_format))
			return 0;
	}

	DBG(("%s: dst=%08x [%08x]\n", __FUNCTION__, pixel, dst_format));
	return pixel;
}

uint32_t
sna_rgba_for_color(uint32_t color, int depth)
{
	return color_convert(color, sna_format_for_depth(depth), PICT_a8r8g8b8);
}

static uint32_t
get_pixel(PicturePtr picture)
{
	PixmapPtr pixmap = get_drawable_pixmap(picture->pDrawable);

	DBG(("%s: %p\n", __FUNCTION__, pixmap));

	sna_pixmap_move_to_cpu(pixmap, false);
	switch (pixmap->drawable.bitsPerPixel) {
	case 32: return *(uint32_t *)pixmap->devPrivate.ptr;
	case 16: return *(uint16_t *)pixmap->devPrivate.ptr;
	default: return *(uint8_t *)pixmap->devPrivate.ptr;
	}
}

static uint32_t
get_solid_color(PicturePtr picture, uint32_t format)
{
	if (picture->pSourcePict) {
		PictSolidFill *fill = (PictSolidFill *)picture->pSourcePict;
		return color_convert(fill->color, PICT_a8r8g8b8, format);
	} else
		return color_convert(get_pixel(picture), picture->format, format);
}

static Bool
is_solid(PicturePtr picture)
{
	if (picture->pSourcePict) {
		if (picture->pSourcePict->type == SourcePictTypeSolidFill)
			return TRUE;
	}

	if (picture->pDrawable) {
		if (picture->pDrawable->width == 1 &&
		    picture->pDrawable->height == 1 &&
		    picture->repeat)
			return TRUE;
	}

	return FALSE;
}

Bool
sna_picture_is_solid(PicturePtr picture, uint32_t *color)
{
	if (!is_solid(picture))
		return FALSE;

	*color = get_solid_color(picture, PICT_a8r8g8b8);
	return TRUE;
}

static Bool
pixel_is_opaque(uint32_t pixel, uint32_t format)
{
	int abits;

	abits = PICT_FORMAT_A(format);
	if (!abits)
		return TRUE;

	if (PICT_FORMAT_TYPE(format) == PICT_TYPE_A ||
	    PICT_FORMAT_TYPE(format) == PICT_TYPE_BGRA) {
		return (pixel & ((1 << abits) - 1)) == ((1 << abits) - 1);
	} else if (PICT_FORMAT_TYPE(format) == PICT_TYPE_ARGB ||
		   PICT_FORMAT_TYPE(format) == PICT_TYPE_ABGR) {
		int ashift = PICT_FORMAT_BPP(format) - abits;
		return (pixel >> ashift) == ((1 << abits) - 1);
	} else
		return FALSE;
}

static Bool
is_opaque_solid(PicturePtr picture)
{
	if (picture->pSourcePict) {
		PictSolidFill *fill = (PictSolidFill *) picture->pSourcePict;
		return (fill->color >> 24) == 0xff;
	} else
		return pixel_is_opaque(get_pixel(picture), picture->format);
}

fastcall
static void blt_fill_composite(struct sna *sna,
			       const struct sna_composite_op *op,
			       const struct sna_composite_rectangles *r)
{
	int x1, x2, y1, y2;

	x1 = r->dst.x + op->dst.x;
	y1 = r->dst.y + op->dst.y;
	x2 = x1 + r->width;
	y2 = y1 + r->height;

	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;

	if (x2 > op->dst.width)
		x2 = op->dst.width;
	if (y2 > op->dst.height)
		y2 = op->dst.height;

	if (x2 <= x1 || y2 <= y1)
		return;

	sna_blt_fill_one(sna, &op->u.blt, x1, y1, x2-x1, y2-y1);
}

static void blt_fill_composite_boxes(struct sna *sna,
				     const struct sna_composite_op *op,
				     const BoxRec *box, int n)
{
	do {
		sna_blt_fill_one(sna, &op->u.blt,
				 box->x1 + op->dst.x, box->y1 + op->dst.y,
				 box->x2 - box->x1, box->y2 - box->y1);
		box++;
	} while (--n);
}

static Bool
prepare_blt_clear(struct sna *sna,
		  struct sna_composite_op *op)
{
	DBG(("%s\n", __FUNCTION__));

	op->blt   = blt_fill_composite;
	op->boxes = blt_fill_composite_boxes;
	op->done  = blt_done;

	return sna_blt_fill_init(sna, &op->u.blt,
				 op->dst.bo,
				 op->dst.pixmap->drawable.bitsPerPixel,
				 GXclear, 0);
}

static bool
prepare_blt_fill(struct sna *sna,
		 struct sna_composite_op *op,
		 PicturePtr source)
{
	DBG(("%s\n", __FUNCTION__));

	op->blt = blt_fill_composite;
	op->boxes = blt_fill_composite_boxes;
	op->done = blt_done;

	return sna_blt_fill_init(sna, &op->u.blt, op->dst.bo,
				 op->dst.pixmap->drawable.bitsPerPixel,
				 GXcopy,
				 get_solid_color(source, op->dst.format));
}

fastcall static void
blt_copy_composite(struct sna *sna,
		   const struct sna_composite_op *op,
		   const struct sna_composite_rectangles *r)
{
	int x1, x2, y1, y2;
	int src_x, src_y;

	DBG(("%s: src=(%d, %d), dst=(%d, %d), size=(%d, %d)\n",
	     __FUNCTION__,
	     r->src.x, r->src.y,
	     r->dst.x, r->dst.y,
	     r->width, r->height));

	/* XXX higher layer should have clipped? */

	x1 = r->dst.x + op->dst.x;
	y1 = r->dst.y + op->dst.y;
	x2 = x1 + r->width;
	y2 = y1 + r->height;

	src_x = r->src.x - x1;
	src_y = r->src.y - y1;

	/* clip against dst */
	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;

	if (x2 > op->dst.width)
		x2 = op->dst.width;

	if (y2 > op->dst.height)
		y2 = op->dst.height;

	DBG(("%s: box=(%d, %d), (%d, %d)\n", __FUNCTION__, x1, y1, x2, y2));

	if (x2 <= x1 || y2 <= y1)
		return;

	sna_blt_copy_one(sna, &op->u.blt,
			 x1 + src_x, y1 + src_y,
			 x2 - x1, y2 - y1,
			 x1, y1);
}

static void blt_copy_composite_boxes(struct sna *sna,
				     const struct sna_composite_op *op,
				     const BoxRec *box, int nbox)
{
	DBG(("%s: nbox=%d\n", __FUNCTION__, nbox));
	do {
		DBG(("%s: box (%d, %d), (%d, %d)\n",
		     __FUNCTION__, box->x1, box->y1, box->x2, box->y2));
		sna_blt_copy_one(sna, &op->u.blt,
				 box->x1 + op->u.blt.sx, box->y1 + op->u.blt.sy,
				 box->x2 - box->x1, box->y2 - box->y1,
				 box->x1 + op->dst.x, box->y1 + op->dst.y);
		box++;
	} while(--nbox);
}

static Bool
prepare_blt_copy(struct sna *sna,
		 struct sna_composite_op *op)
{
	PixmapPtr src = op->u.blt.src_pixmap;
	struct sna_pixmap *priv = sna_pixmap(src);

	if (priv->gpu_bo->tiling == I915_TILING_Y)
		return FALSE;

	if (!kgem_check_bo_fenced(&sna->kgem, priv->gpu_bo, NULL))
		_kgem_submit(&sna->kgem);

	DBG(("%s\n", __FUNCTION__));

	op->blt   = blt_copy_composite;
	op->boxes = blt_copy_composite_boxes;
	op->done  = blt_done;

	return sna_blt_copy_init(sna, &op->u.blt,
				 priv->gpu_bo,
				 op->dst.bo,
				 src->drawable.bitsPerPixel,
				 GXcopy);
}

static void blt_vmap_done(struct sna *sna, const struct sna_composite_op *op)
{
	struct kgem_bo *bo = (struct kgem_bo *)op->u.blt.src_pixmap;

	blt_done(sna, op);
	if (bo) {
		struct kgem *kgem = &sna->kgem;
		kgem_bo_sync(kgem, bo, true);
		kgem_bo_destroy(kgem, bo);
	}
}

fastcall static void
blt_put_composite(struct sna *sna,
		  const struct sna_composite_op *op,
		  const struct sna_composite_rectangles *r)
{
	PixmapPtr dst = op->dst.pixmap;
	PixmapPtr src = op->u.blt.src_pixmap;
	struct sna_pixmap *dst_priv = sna_pixmap(dst);
	int pitch = src->devKind;
	char *data = src->devPrivate.ptr;
	int bpp = src->drawable.bitsPerPixel;

	int16_t dst_x = r->dst.x + op->dst.x;
	int16_t dst_y = r->dst.y + op->dst.y;
	int16_t src_x = r->src.x + op->u.blt.sx;
	int16_t src_y = r->src.y + op->u.blt.sy;

	if (!dst_priv->pinned &&
	    dst_x <= 0 && dst_y <= 0 &&
	    dst_x + r->width >= op->dst.width &&
	    dst_y + r->height >= op->dst.height) {
		data += (src_x - dst_x) * bpp / 8;
		data += (src_y - dst_y) * pitch;

		dst_priv->gpu_bo =
			sna_replace(sna, dst_priv->gpu_bo,
				    r->width, r->height, bpp,
				    data, pitch);
	} else {
		BoxRec box;

		box.x1 = dst_x;
		box.y1 = dst_y;
		box.x2 = dst_x + r->width;
		box.y2 = dst_y + r->height;

		sna_write_boxes(sna,
				dst_priv->gpu_bo, 0, 0,
				data, pitch, bpp, src_x, src_y,
				&box, 1);
	}
}

static void blt_put_composite_boxes(struct sna *sna,
				    const struct sna_composite_op *op,
				    const BoxRec *box, int n)
{
	PixmapPtr src = op->u.blt.src_pixmap;
	struct sna_pixmap *dst_priv = sna_pixmap(op->dst.pixmap);

	DBG(("%s: src=(%d, %d), dst=(%d, %d) x %d\n", __FUNCTION__,
	     op->u.blt.sx, op->u.blt.sy,
	     op->dst.x, op->dst.y, n));

	if (n == 1 && !dst_priv->pinned &&
	    box->x2 - box->x1 == op->dst.width &&
	    box->y2 - box->y1 == op->dst.height) {
		int pitch = src->devKind;
		int bpp = src->drawable.bitsPerPixel / 8;
		char *data = src->devPrivate.ptr;

		data += (box->y1 + op->u.blt.sy) * pitch;
		data += (box->x1 + op->u.blt.sx) * bpp;

		dst_priv->gpu_bo =
			sna_replace(sna,
				    op->dst.bo,
				    op->dst.width,
				    op->dst.height,
				    src->drawable.bitsPerPixel,
				    data, pitch);
	} else {
		sna_write_boxes(sna,
				op->dst.bo, op->dst.x, op->dst.y,
				src->devPrivate.ptr,
				src->devKind,
				src->drawable.bitsPerPixel,
				op->u.blt.sx, op->u.blt.sy,
				box, n);
	}
}

static Bool
prepare_blt_put(struct sna *sna,
		struct sna_composite_op *op)
{
	PixmapPtr src = op->u.blt.src_pixmap;
	struct sna_pixmap *priv = sna_pixmap(src);
	struct kgem_bo *src_bo = NULL;
	struct kgem_bo *free_bo = NULL;

	DBG(("%s\n", __FUNCTION__));

	if (priv) {
		if (!priv->gpu_only) {
			src_bo = priv->cpu_bo;
			if (!src_bo) {
				src_bo = kgem_create_map(&sna->kgem,
							 src->devPrivate.ptr,
							 pixmap_size(src),
							 1);
				priv->cpu_bo = src_bo;
			}
		}
	} else {
		src_bo = kgem_create_map(&sna->kgem,
					 src->devPrivate.ptr,
					 pixmap_size(src),
					 0);
		free_bo = src_bo;
	}
	if (src_bo) {
		op->blt = blt_copy_composite;
		op->boxes = blt_copy_composite_boxes;

		op->u.blt.src_pixmap = (void *)free_bo;
		op->done = blt_vmap_done;

		src_bo->pitch = src->devKind;
		if (!sna_blt_copy_init(sna, &op->u.blt,
				       src_bo, op->dst.bo,
				       op->dst.pixmap->drawable.bitsPerPixel,
				       GXcopy))
			return FALSE;
	} else {
		op->blt   = blt_put_composite;
		op->boxes = blt_put_composite_boxes;
		op->done  = nop_done;
	}

	return TRUE;
}

static Bool
has_gpu_area(PixmapPtr pixmap, int x, int y, int w, int h)
{
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	BoxRec area;

	if (!priv)
		return FALSE;
	if (!priv->gpu_bo)
		return FALSE;

	if (priv->cpu_damage == NULL)
		return TRUE;

	area.x1 = x;
	area.y1 = y;
	area.x2 = x + w;
	area.y2 = y + h;
	return sna_damage_contains_box(priv->cpu_damage,
				       &area) == PIXMAN_REGION_OUT;
}

static Bool
has_cpu_area(PixmapPtr pixmap, int x, int y, int w, int h)
{
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	BoxRec area;

	if (!priv)
		return TRUE;
	if (!priv->gpu_bo)
		return TRUE;
	if (priv->gpu_only)
		return FALSE;

	if (priv->gpu_damage == NULL)
		return TRUE;

	area.x1 = x;
	area.y1 = y;
	area.x2 = x + w;
	area.y2 = y + h;
	return sna_damage_contains_box(priv->gpu_damage,
				       &area) == PIXMAN_REGION_OUT;
}

Bool
sna_blt_composite(struct sna *sna,
		  uint32_t op,
		  PicturePtr src,
		  PicturePtr dst,
		  int16_t x, int16_t y,
		  int16_t dst_x, int16_t dst_y,
		  int16_t width, int16_t height,
		  struct sna_composite_op *tmp)
{
	struct sna_blt_state *blt = &tmp->u.blt;
	PictFormat src_format = src->format;
	struct sna_pixmap *priv;
	int16_t tx, ty;
	Bool ret;

#if DEBUG_NO_BLT || NO_BLT_COMPOSITE
	return FALSE;
#endif

	DBG(("%s (%d, %d), (%d, %d), %dx%d\n",
	     __FUNCTION__, x, y, dst_x, dst_y, width, height));

	switch (dst->pDrawable->bitsPerPixel) {
	case 8:
	case 16:
	case 32:
		break;
	default:
		DBG(("%s: unhandled bpp: %d\n", __FUNCTION__,
		     dst->pDrawable->bitsPerPixel));
		return FALSE;
	}

	tmp->dst.pixmap = get_drawable_pixmap(dst->pDrawable);
	priv = sna_pixmap_move_to_gpu(tmp->dst.pixmap);
	if (priv == NULL || priv->gpu_bo->tiling == I915_TILING_Y) {
		DBG(("%s: dst not on the gpu or using Y-tiling\n",
		     __FUNCTION__));
		return FALSE;
	}

	tmp->dst.format = dst->format;
	tmp->dst.width = tmp->dst.pixmap->drawable.width;
	tmp->dst.height = tmp->dst.pixmap->drawable.height;
	get_drawable_deltas(dst->pDrawable, tmp->dst.pixmap,
			    &tmp->dst.x, &tmp->dst.y);
	tmp->dst.bo = priv->gpu_bo;
	if (!priv->gpu_only)
		tmp->damage = &priv->gpu_damage;

	if (!kgem_check_bo_fenced(&sna->kgem, priv->gpu_bo, NULL))
		_kgem_submit(&sna->kgem);

	if (op == PictOpClear)
		return prepare_blt_clear(sna, tmp);

	if (is_solid(src)) {
		if (op == PictOpOver && is_opaque_solid(src))
			op = PictOpSrc;

		if (op != PictOpSrc) {
			DBG(("%s: unsuported op [%d] for blitting\n",
			     __FUNCTION__, op));
			return FALSE;
		}

		return prepare_blt_fill(sna, tmp, src);
	}

	if (!src->pDrawable) {
		DBG(("%s: unsuported procedural source\n",
		     __FUNCTION__));
		return FALSE;
	}

	if (!sna_blt_compare_depth(src->pDrawable, dst->pDrawable)) {
		DBG(("%s: mismatching depth src=%d/%d, dst=%d/%d\n",
		     __FUNCTION__,
		     src->pDrawable->depth, src->pDrawable->bitsPerPixel,
		     dst->pDrawable->depth, dst->pDrawable->bitsPerPixel));
		return FALSE;
	}

	if (op == PictOpOver && PICT_FORMAT_A(src_format) == 0)
		op = PictOpSrc;

	if (op != PictOpSrc) {
		DBG(("%s: unsuported op [%d] for blitting\n",
		     __FUNCTION__, op));
		return FALSE;
	}

	if (src->filter == PictFilterConvolution) {
		DBG(("%s: convolutions filters not handled\n",
		     __FUNCTION__));
		return FALSE;
	}

	if (!(dst->format == src_format ||
	      dst->format == PICT_FORMAT(PICT_FORMAT_BPP(src_format),
					 PICT_FORMAT_TYPE(src_format),
					 0,
					 PICT_FORMAT_R(src_format),
					 PICT_FORMAT_G(src_format),
					 PICT_FORMAT_B(src_format)))) {
		DBG(("%s: incompatible src/dst formats src=%08x, dst=%08x\n",
		     __FUNCTION__, (unsigned)src_format, dst->format));
		return FALSE;
	}

	if (!sna_transform_is_integer_translation(src->transform, &tx, &ty)) {
		DBG(("%s: source transform is not an integer translation\n",
		     __FUNCTION__));
		return FALSE;
	}
	x += tx;
	y += ty;

	/* XXX tiling? */
	if (x < 0 || y < 0 ||
	    x + width > src->pDrawable->width ||
	    y + height > src->pDrawable->height) {
		DBG(("%s: source extends outside of valid area\n",
		     __FUNCTION__));
		return FALSE;
	}

	blt->src_pixmap = get_drawable_pixmap(src->pDrawable);
	get_drawable_deltas(src->pDrawable, blt->src_pixmap, &tx, &ty);
	x += tx + src->pDrawable->x;
	y += ty + src->pDrawable->y;
	assert(x >= 0);
	assert(y >= 0);
	assert(x + width <= blt->src_pixmap->drawable.width);
	assert(y + height <= blt->src_pixmap->drawable.height);

	tmp->u.blt.sx = x - dst_x;
	tmp->u.blt.sy = y - dst_y;
	DBG(("%s: blt dst offset (%d, %d), source offset (%d, %d)\n",
	     __FUNCTION__,
	     tmp->dst.x, tmp->dst.y, tmp->u.blt.sx, tmp->u.blt.sy));

	if (has_gpu_area(blt->src_pixmap, x, y, width, height))
		ret = prepare_blt_copy(sna, tmp);
	else if (has_cpu_area(blt->src_pixmap, x, y, width, height))
		ret = prepare_blt_put(sna, tmp);
	else if (sna_pixmap_move_to_gpu(blt->src_pixmap))
		ret = prepare_blt_copy(sna, tmp);
	else
		ret = prepare_blt_put(sna, tmp);

	return ret;
}

static void sna_blt_fill_op_blt(struct sna *sna,
				const struct sna_fill_op *op,
				int16_t x, int16_t y,
				int16_t width, int16_t height)
{
	sna_blt_fill_one(sna, &op->base.u.blt, x, y, width, height);
}

static void sna_blt_fill_op_done(struct sna *sna,
				 const struct sna_fill_op *fill)
{
	blt_done(sna, &fill->base);
}

bool sna_blt_fill(struct sna *sna, uint8_t alu,
		  struct kgem_bo *bo, int bpp,
		  uint32_t pixel,
		  struct sna_fill_op *fill)
{
#if DEBUG_NO_BLT || NO_BLT_FILL
	return FALSE;
#endif

	DBG(("%s(alu=%d, pixel=%x, bpp=%d)\n", __FUNCTION__, alu, pixel, bpp));

	if (bo->tiling == I915_TILING_Y) {
		DBG(("%s: rejected due to incompatible Y-tiling\n",
		     __FUNCTION__));
		return FALSE;
	}

	if (!sna_blt_fill_init(sna, &fill->base.u.blt,
			       bo, bpp, alu, pixel))
		return FALSE;

	fill->blt  = sna_blt_fill_op_blt;
	fill->done = sna_blt_fill_op_done;
	return TRUE;
}

static void sna_blt_copy_op_blt(struct sna *sna,
				const struct sna_copy_op *op,
				int16_t src_x, int16_t src_y,
				int16_t width, int16_t height,
				int16_t dst_x, int16_t dst_y)
{
	sna_blt_copy_one(sna, &op->base.u.blt,
			 src_x, src_y,
			 width, height,
			 dst_x, dst_y);
}

static void sna_blt_copy_op_done(struct sna *sna,
				 const struct sna_copy_op *op)
{
	blt_done(sna, &op->base);
}

bool sna_blt_copy(struct sna *sna, uint8_t alu,
		  struct kgem_bo *src,
		  struct kgem_bo *dst,
		  int bpp,
		  struct sna_copy_op *op)
{
#if DEBUG_NO_BLT || NO_BLT_COPY
	return FALSE;
#endif

	if (src->tiling == I915_TILING_Y)
		return FALSE;

	if (dst->tiling == I915_TILING_Y)
		return FALSE;

	if (!sna_blt_copy_init(sna, &op->base.u.blt,
			       src, dst,
			       bpp, alu))
		return FALSE;

	op->blt  = sna_blt_copy_op_blt;
	op->done = sna_blt_copy_op_done;
	return TRUE;
}

Bool sna_blt_fill_boxes(struct sna *sna, uint8_t alu,
			struct kgem_bo *bo, int bpp,
			uint32_t color,
			const BoxRec *box, int nbox)
{
	struct kgem *kgem = &sna->kgem;
	int br13, cmd;

#if DEBUG_NO_BLT || NO_BLT_FILL_BOXES
	return FALSE;
#endif

	DBG(("%s (%d, %08x, %d) x %d\n",
	     __FUNCTION__, bpp, color, alu, nbox));

	if (bo->tiling == I915_TILING_Y) {
		DBG(("%s: fallback -- dst uses Y-tiling\n",
		     __FUNCTION__));
		return FALSE;
	}

	cmd = XY_COLOR_BLT_CMD;
	if (bpp == 32)
		cmd |= BLT_WRITE_ALPHA | BLT_WRITE_RGB;

	br13 = bo->pitch;
	if (kgem->gen >= 40 && bo->tiling) {
		cmd |= BLT_DST_TILED;
		br13 >>= 2;
	}
	if (br13 > MAXSHORT) {
		DBG(("%s: fallback -- pitch is too large %d [%d]\n",
		     __FUNCTION__, bo->pitch, br13));
		return FALSE;
	}

	br13 |= fill_ROP[alu] << 16;
	switch (bpp) {
	default: assert(0);
	case 32: br13 |= 1 << 25; /* RGB8888 */
	case 16: br13 |= 1 << 24; /* RGB565 */
	case 8: break;
	}

	/* All too frequently one blt completely overwrites the previous */
	if (kgem->nbatch >= 6 &&
	    (alu == GXcopy || alu == GXclear) &&
	    kgem->batch[kgem->nbatch-6] == cmd &&
	    kgem->batch[kgem->nbatch-4] == (box[0].y1 << 16 | box[0].x1) &&
	    kgem->batch[kgem->nbatch-3] == (box[0].y2 << 16 | box[0].x2) &&
	    kgem->reloc[kgem->nreloc-1].target_handle == bo->handle) {
		DBG(("%s: replacing last fill\n", __FUNCTION__));
		kgem->batch[kgem->nbatch-5] = br13;
		kgem->batch[kgem->nbatch-1] = color;
		if (--nbox == 0)
			return TRUE;
		box++;
	}

	kgem_set_mode(kgem, KGEM_BLT);
	if (!kgem_check_batch(kgem, 6) ||
	    !kgem_check_bo_fenced(kgem, bo, NULL) ||
	    kgem->nreloc + 1 > KGEM_RELOC_SIZE(kgem))
		_kgem_submit(kgem);

	do {
		int nbox_this_time;

		nbox_this_time = nbox;
		if (6*nbox_this_time > kgem->surface - kgem->nbatch - KGEM_BATCH_RESERVED)
			nbox_this_time = (kgem->surface - kgem->nbatch - KGEM_BATCH_RESERVED) / 6;
		if (nbox_this_time > KGEM_RELOC_SIZE(kgem) - kgem->nreloc)
			nbox_this_time = KGEM_RELOC_SIZE(kgem) - kgem->nreloc;
		assert(nbox_this_time);
		nbox -= nbox_this_time;

		do {
			uint32_t *b = kgem->batch + kgem->nbatch;

			DBG(("%s: box=((%d, %d), (%d, %d))\n", __FUNCTION__,
			     box->x1, box->y1, box->x2, box->y2));

			assert(box->x1 >= 0);
			assert(box->y1 >= 0);

			b[0] = cmd;
			b[1] = br13;
			b[2] = box->y1 << 16 | box->x1;
			b[3] = box->y2 << 16 | box->x2;
			b[4] = kgem_add_reloc(kgem, kgem->nbatch + 4,
					      bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      0);
			b[5] = color;
			kgem->nbatch += 6;
			box++;
		} while (--nbox_this_time);

		if (nbox)
			_kgem_submit(kgem);
	} while (nbox);

	_kgem_set_mode(kgem, KGEM_BLT);
	return TRUE;
}

Bool sna_blt_copy_boxes(struct sna *sna, uint8_t alu,
			struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
			struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
			int bpp, const BoxRec *box, int nbox)
{
	struct kgem *kgem = &sna->kgem;
	int src_pitch, br13, cmd;

#if DEBUG_NO_BLT || NO_BLT_COPY_BOXES
	return FALSE;
#endif

	DBG(("%s src=(%d, %d) -> (%d, %d) x %d, tiling=(%d, %d), pitch=(%d, %d)\n",
	     __FUNCTION__, src_dx, src_dy, dst_dx, dst_dy, nbox,
	    src_bo->tiling, dst_bo->tiling,
	    src_bo->pitch, dst_bo->pitch));

	if (src_bo->tiling == I915_TILING_Y)
		return FALSE;

	if (dst_bo->tiling == I915_TILING_Y)
		return FALSE;

	cmd = XY_SRC_COPY_BLT_CMD;
	if (bpp == 32)
		cmd |= BLT_WRITE_ALPHA | BLT_WRITE_RGB;

	src_pitch = src_bo->pitch;
	if (kgem->gen >= 40 && src_bo->tiling) {
		cmd |= BLT_SRC_TILED;
		src_pitch >>= 2;
	}
	if (src_pitch > MAXSHORT)
		return FALSE;

	br13 = dst_bo->pitch;
	if (kgem->gen >= 40 && dst_bo->tiling) {
		cmd |= BLT_DST_TILED;
		br13 >>= 2;
	}
	if (br13 > MAXSHORT)
		return FALSE;

	br13 |= copy_ROP[alu] << 16;
	switch (bpp) {
	default: assert(0);
	case 32: br13 |= 1 << 25; /* RGB8888 */
	case 16: br13 |= 1 << 24; /* RGB565 */
	case 8: break;
	}

	kgem_set_mode(kgem, KGEM_BLT);
	if (!kgem_check_batch(kgem, 8) ||
	    !kgem_check_bo_fenced(kgem, dst_bo, src_bo, NULL) ||
	    kgem->nreloc + 2 > KGEM_RELOC_SIZE(kgem))
		_kgem_submit(kgem);

	do {
		int nbox_this_time;

		nbox_this_time = nbox;
		if (8*nbox_this_time > kgem->surface - kgem->nbatch - KGEM_BATCH_RESERVED)
			nbox_this_time = (kgem->surface - kgem->nbatch - KGEM_BATCH_RESERVED) / 8;
		if (2*nbox_this_time > KGEM_RELOC_SIZE(kgem) - kgem->nreloc)
			nbox_this_time = (KGEM_RELOC_SIZE(kgem) - kgem->nreloc)/2;
		assert(nbox_this_time);
		nbox -= nbox_this_time;

		do {
			uint32_t *b = kgem->batch + kgem->nbatch;

			DBG(("  %s: box=(%d, %d)x(%d, %d)\n",
			     __FUNCTION__,
			     box->x1, box->y1,
			     box->x2 - box->x1, box->y2 - box->y1));

			assert(box->x1 + src_dx >= 0);
			assert(box->y1 + src_dy >= 0);

			assert(box->x1 + dst_dx >= 0);
			assert(box->y1 + dst_dy >= 0);

			b[0] = cmd;
			b[1] = br13;
			b[2] = ((box->y1 + dst_dy) << 16) | (box->x1 + dst_dx);
			b[3] = ((box->y2 + dst_dy) << 16) | (box->x2 + dst_dx);
			b[4] = kgem_add_reloc(kgem, kgem->nbatch + 4,
					      dst_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      0);
			b[5] = ((box->y1 + src_dy) << 16) | (box->x1 + src_dx);
			b[6] = src_pitch;
			b[7] = kgem_add_reloc(kgem, kgem->nbatch + 7,
					      src_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      KGEM_RELOC_FENCED,
					      0);
			kgem->nbatch += 8;
			box++;
		} while (--nbox_this_time);

		if (nbox)
			_kgem_submit(kgem);
	} while (nbox);

	_kgem_set_mode(kgem, KGEM_BLT);
	return TRUE;
}
