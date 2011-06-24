/*
 * Copyright © 2011 Intel Corporation
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

#include "sna.h"
#include "sna_render.h"

#include <fb.h>

#if DEBUG_RENDER
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

#define NO_REDIRECT 0
#define NO_CONVERT 0
#define NO_FIXUP 0
#define NO_EXTRACT 0

CARD32
sna_format_for_depth(int depth)
{
	switch (depth) {
	case 1: return PICT_a1;
	case 4: return PICT_a4;
	case 8: return PICT_a8;
	case 15: return PICT_x1r5g5b5;
	case 16: return PICT_r5g6b5;
	default:
	case 24: return PICT_x8r8g8b8;
	case 30: return PICT_x2r10g10b10;
	case 32: return PICT_a8r8g8b8;
	}
}

static Bool
no_render_composite(struct sna *sna,
		    uint8_t op,
		    PicturePtr src,
		    PicturePtr mask,
		    PicturePtr dst,
		    int16_t src_x, int16_t src_y,
		    int16_t mask_x, int16_t mask_y,
		    int16_t dst_x, int16_t dst_y,
		    int16_t width, int16_t height,
		    struct sna_composite_op *tmp)
{
	DBG(("%s ()\n", __FUNCTION__));

	if (mask == NULL &&
	    sna_blt_composite(sna,
			      op, src, dst,
			      src_x, src_y,
			      dst_x, dst_y,
			      width, height,
			      tmp))
		return TRUE;

	return FALSE;
}

static Bool
no_render_copy_boxes(struct sna *sna, uint8_t alu,
		     PixmapPtr src, struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
		     PixmapPtr dst, struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
		     const BoxRec *box, int n)
{
	DBG(("%s (n=%d)\n", __FUNCTION__, n));

	if (src->drawable.depth != dst->drawable.depth)
		return FALSE;

	return sna_blt_copy_boxes(sna, alu,
				  src_bo, src_dx, src_dy,
				  dst_bo, dst_dx, dst_dy,
				  dst->drawable.bitsPerPixel,
				  box, n);
}

static Bool
no_render_copy(struct sna *sna, uint8_t alu,
		 PixmapPtr src, struct kgem_bo *src_bo,
		 PixmapPtr dst, struct kgem_bo *dst_bo,
		 struct sna_copy_op *tmp)
{
	DBG(("%s ()\n", __FUNCTION__));

	if (src->drawable.depth == dst->drawable.depth &&
	    sna_blt_copy(sna, alu,
			 src_bo, dst_bo, dst->drawable.bitsPerPixel,
			 tmp))
		return TRUE;

	return FALSE;
}

static Bool
no_render_fill_boxes(struct sna *sna,
		     CARD8 op,
		     PictFormat format,
		     const xRenderColor *color,
		     PixmapPtr dst, struct kgem_bo *dst_bo,
		     const BoxRec *box, int n)
{
	uint8_t alu = GXcopy;
	uint32_t pixel;

	DBG(("%s (op=%d, color=(%04x,%04x,%04x, %04x))\n",
	     __FUNCTION__, op,
	     color->red, color->green, color->blue, color->alpha));

	if (color == 0)
		op = PictOpClear;

	if (op == PictOpClear) {
		alu = GXclear;
		op = PictOpSrc;
	}

	if (op == PictOpOver) {
		if ((color->alpha >= 0xff00))
			op = PictOpSrc;
	}

	if (op != PictOpSrc)
		return FALSE;

	if (!sna_get_pixel_from_rgba(&pixel,
				     color->red,
				     color->green,
				     color->blue,
				     color->alpha,
				     format))
		return FALSE;

	return sna_blt_fill_boxes(sna, alu,
				  dst_bo, dst->drawable.bitsPerPixel,
				  pixel, box, n);
}

static Bool
no_render_fill(struct sna *sna, uint8_t alu,
	       PixmapPtr dst, struct kgem_bo *dst_bo,
	       uint32_t color,
	       struct sna_fill_op *tmp)
{
	DBG(("%s (alu=%d, color=%08x)\n", __FUNCTION__, alu, color));
	return sna_blt_fill(sna, alu,
			    dst_bo, dst->drawable.bitsPerPixel,
			    color,
			    tmp);
}

static void no_render_reset(struct sna *sna)
{
}

static void no_render_flush(struct sna *sna)
{
}

static void
no_render_context_switch(struct kgem *kgem,
			 int new_mode)
{
}

static void
no_render_fini(struct sna *sna)
{
}

void no_render_init(struct sna *sna)
{
	struct sna_render *render = &sna->render;

	render->composite = no_render_composite;

	render->copy_boxes = no_render_copy_boxes;
	render->copy = no_render_copy;

	render->fill_boxes = no_render_fill_boxes;
	render->fill = no_render_fill;

	render->reset = no_render_reset;
	render->flush = no_render_flush;
	render->fini = no_render_fini;

	sna->kgem.context_switch = no_render_context_switch;
	if (sna->kgem.gen >= 60)
		sna->kgem.ring = KGEM_BLT;
}

static Bool
move_to_gpu(PixmapPtr pixmap, const BoxRec *box)
{
	struct sna_pixmap *priv;
	int count, w, h;

	if (pixmap->usage_hint) {
		DBG(("%s: not migrating pixmap due to usage_hint=%d\n",
		     __FUNCTION__, pixmap->usage_hint));
		return FALSE;
	}

	w = box->x2 - box->x1;
	h = box->y2 - box->y1;
	if (w == pixmap->drawable.width || h == pixmap->drawable.height) {
		DBG(("%s: migrating whole pixmap (%dx%d) for source\n",
		     __FUNCTION__,
		     pixmap->drawable.width,
		     pixmap->drawable.height));
		return TRUE;
	}

	count = SOURCE_BIAS;
	priv = sna_pixmap(pixmap);
	if (priv)
		count = ++priv->source_count;

	DBG(("%s: migrate box (%d, %d), (%d, %d)? source count=%d, fraction=%d/%d [%d]\n",
	     __FUNCTION__,
	     box->x1, box->y1, box->x2, box->y2,
	     count, w*h,
	     pixmap->drawable.width * pixmap->drawable.height,
	     pixmap->drawable.width * pixmap->drawable.height / (w*h)));

	return count*w*h >= pixmap->drawable.width * pixmap->drawable.height;
}

static Bool
_texture_is_cpu(PixmapPtr pixmap, const BoxRec *box)
{
	struct sna_pixmap *priv = sna_pixmap(pixmap);

	if (priv == NULL)
		return TRUE;

	if (priv->gpu_only)
		return FALSE;

	if (priv->gpu_bo == NULL)
		return TRUE;

	if (!priv->cpu_damage)
		return FALSE;

	if (sna_damage_contains_box(priv->gpu_damage, box) != PIXMAN_REGION_OUT)
		return FALSE;

	return sna_damage_contains_box(priv->cpu_damage, box) != PIXMAN_REGION_OUT;
}

#if DEBUG_RENDER
static Bool
texture_is_cpu(PixmapPtr pixmap, const BoxRec *box)
{
	Bool ret = _texture_is_cpu(pixmap, box);
	ErrorF("%s(pixmap=%p, box=((%d, %d), (%d, %d)) = %d\n",
	       __FUNCTION__, pixmap, box->x1, box->y1, box->x2, box->y2, ret);
	return ret;
}
#else
static Bool
texture_is_cpu(PixmapPtr pixmap, const BoxRec *box)
{
	return _texture_is_cpu(pixmap, box);
}
#endif

static struct kgem_bo *upload(struct sna *sna,
			      struct sna_composite_channel *channel,
			      PixmapPtr pixmap,
			      int16_t x, int16_t y, int16_t w, int16_t h,
			      BoxPtr box)
{
	struct kgem_bo *bo;

	DBG(("%s: origin=(%d, %d), box=(%d, %d), (%d, %d), pixmap=%dx%d\n",
	     __FUNCTION__, x, y, box->x1, box->y1, box->x2, box->y2, pixmap->drawable.width, pixmap->drawable.height));
	assert(box->x1 >= 0);
	assert(box->y1 >= 0);
	assert(box->x2 <= pixmap->drawable.width);
	assert(box->y2 <= pixmap->drawable.height);

	bo = kgem_upload_source_image(&sna->kgem,
				      pixmap->devPrivate.ptr,
				      box->x1, box->y1, w, h,
				      pixmap->devKind,
				      pixmap->drawable.bitsPerPixel);
	if (bo) {
		channel->offset[0] -= box->x1;
		channel->offset[1] -= box->y1;
		channel->scale[0] = 1./w;
		channel->scale[1] = 1./h;
		channel->width  = w;
		channel->height = h;
	}

	return bo;
}

int
sna_render_pixmap_bo(struct sna *sna,
		     struct sna_composite_channel *channel,
		     PixmapPtr pixmap,
		     int16_t x, int16_t y,
		     int16_t w, int16_t h,
		     int16_t dst_x, int16_t dst_y)
{
	struct kgem_bo *bo = NULL;
	struct sna_pixmap *priv;
	BoxRec box;

	DBG(("%s (%d, %d)x(%d, %d)\n", __FUNCTION__, x, y, w,h));

	/* XXX handle transformed repeat */
	if (w == 0 || h == 0 || channel->transform) {
		box.x1 = box.y1 = 0;
		box.x2 = pixmap->drawable.width;
		box.y2 = pixmap->drawable.height;
	} else {
		box.x1 = x;
		box.y1 = y;
		box.x2 = x + w;
		box.y2 = y + h;

		if (channel->repeat != RepeatNone) {
			if (box.x1 < 0 ||
			    box.y1 < 0 ||
			    box.x2 > pixmap->drawable.width ||
			    box.y2 > pixmap->drawable.height) {
				box.x1 = box.y1 = 0;
				box.x2 = pixmap->drawable.width;
				box.y2 = pixmap->drawable.height;
			}
		} else {
			if (box.x1 < 0)
				box.x1 = 0;
			if (box.y1 < 0)
				box.y1 = 0;
			if (box.x2 > pixmap->drawable.width)
				box.x2 = pixmap->drawable.width;
			if (box.y2 > pixmap->drawable.height)
				box.y2 = pixmap->drawable.height;
		}
	}

	w = box.x2 - box.x1;
	h = box.y2 - box.y1;
	DBG(("%s box=(%d, %d), (%d, %d): (%d, %d)/(%d, %d)\n", __FUNCTION__,
	     box.x1, box.y1, box.x2, box.y2, w, h,
	     pixmap->drawable.width, pixmap->drawable.height));
	if (w <= 0 || h <= 0) {
		DBG(("%s: sample extents outside of texture -> clear\n",
		     __FUNCTION__));
		return 0;
	}

	channel->height = pixmap->drawable.height;
	channel->width  = pixmap->drawable.width;
	channel->scale[0] = 1. / pixmap->drawable.width;
	channel->scale[1] = 1. / pixmap->drawable.height;
	channel->offset[0] = x - dst_x;
	channel->offset[1] = y - dst_y;

	DBG(("%s: offset=(%d, %d), size=(%d, %d)\n",
	     __FUNCTION__,
	     channel->offset[0], channel->offset[1],
	     pixmap->drawable.width, pixmap->drawable.height));

	if (texture_is_cpu(pixmap, &box) && !move_to_gpu(pixmap, &box)) {
		/* If we are using transient data, it is better to copy
		 * to an amalgamated upload buffer so that we don't
		 * stall on releasing the cpu bo immediately upon
		 * completion of the operation.
		 */
		if (pixmap->usage_hint != CREATE_PIXMAP_USAGE_SCRATCH_HEADER &&
		    w * pixmap->drawable.bitsPerPixel * h > 8*4096) {
			priv = sna_pixmap_attach(pixmap);
			bo = pixmap_vmap(&sna->kgem, pixmap);
			if (bo)
				bo = kgem_bo_reference(bo);
		}

		if (bo == NULL) {
			DBG(("%s: uploading CPU box\n", __FUNCTION__));
			bo = upload(sna, channel, pixmap, x,y, w,h, &box);
		}
	}

	if (bo == NULL) {
		priv = sna_pixmap_force_to_gpu(pixmap);
		if (priv)
			bo = kgem_bo_reference(priv->gpu_bo);
		else
			bo = upload(sna, channel, pixmap, x,y, w,h, &box);
	}

	channel->bo = bo;
	return bo != NULL;
}

int
sna_render_picture_extract(struct sna *sna,
			   PicturePtr picture,
			   struct sna_composite_channel *channel,
			   int16_t x, int16_t y,
			   int16_t w, int16_t h,
			   int16_t dst_x, int16_t dst_y)
{
	struct kgem_bo *bo = NULL;
	PixmapPtr pixmap = get_drawable_pixmap(picture->pDrawable);
	int16_t ox, oy;
	BoxRec box;

#if NO_EXTRACT
	return -1;
#endif

	DBG(("%s (%d, %d)x(%d, %d) [dst=(%d, %d)]\n",
	     __FUNCTION__, x, y, w, h, dst_x, dst_y));

	if (w == 0 || h == 0) {
		DBG(("%s: fallback -- unknown bounds\n", __FUNCTION__));
		return -1;
	}

	ox = box.x1 = x;
	oy = box.y1 = y;
	box.x2 = x + w;
	box.y2 = y + h;
	if (channel->transform) {
		pixman_vector_t v;

		pixman_transform_bounds(channel->transform, &box);

		v.vector[0] = ox << 16;
		v.vector[1] = oy << 16;
		v.vector[2] =  1 << 16;
		pixman_transform_point(channel->transform, &v);
		ox = v.vector[0] / v.vector[2];
		oy = v.vector[1] / v.vector[2];
	}

	if (channel->repeat != RepeatNone) {
		if (box.x1 < 0 ||
		    box.y1 < 0 ||
		    box.x2 > pixmap->drawable.width ||
		    box.y2 > pixmap->drawable.height) {
			/* XXX tiled repeats? */
			box.x1 = box.y1 = 0;
			box.x2 = pixmap->drawable.width;
			box.y2 = pixmap->drawable.height;

			if (!channel->is_affine) {
				DBG(("%s: fallback -- repeating project transform too large for texture\n",
				     __FUNCTION__));
				return -1;
			}
		}
	} else {
		if (box.x1 < 0)
			box.x1 = 0;
		if (box.y1 < 0)
			box.y1 = 0;
		if (box.x2 > pixmap->drawable.width)
			box.x2 = pixmap->drawable.width;
		if (box.y2 > pixmap->drawable.height)
			box.y2 = pixmap->drawable.height;
	}

	w = box.x2 - box.x1;
	h = box.y2 - box.y1;
	DBG(("%s box=(%d, %d), (%d, %d): (%d, %d)/(%d, %d)\n", __FUNCTION__,
	     box.x1, box.y1, box.x2, box.y2, w, h,
	     pixmap->drawable.width, pixmap->drawable.height));
	if (w <= 0 || h <= 0) {
		DBG(("%s: sample extents outside of texture -> clear\n",
		     __FUNCTION__));
		return 0;
	}

	if (w > sna->render.max_3d_size || h > sna->render.max_3d_size) {
		DBG(("%s: fallback -- sample too large for texture (%d, %d)x(%d, %d)\n",
		     __FUNCTION__, box.x1, box.y1, w, h));
		return -1;
	}

	if (texture_is_cpu(pixmap, &box) && !move_to_gpu(pixmap, &box)) {
		bo = kgem_upload_source_image(&sna->kgem,
					      pixmap->devPrivate.ptr,
					      box.x1, box.y1, w, h,
					      pixmap->devKind,
					      pixmap->drawable.bitsPerPixel);
		if (!bo) {
			DBG(("%s: failed to upload source image, using clear\n",
			     __FUNCTION__));
			return 0;
		}
	} else {
		if (!sna_pixmap_move_to_gpu(pixmap)) {
			DBG(("%s: falback -- pixmap is not on the GPU\n",
			     __FUNCTION__));
			return -1;
		}

		bo = kgem_create_2d(&sna->kgem, w, h,
				    pixmap->drawable.bitsPerPixel,
				    kgem_choose_tiling(&sna->kgem,
						       I915_TILING_X, w, h,
						       pixmap->drawable.bitsPerPixel),
				    0);
		if (!bo) {
			DBG(("%s: failed to create bo, using clear\n",
			     __FUNCTION__));
			return 0;
		}

		if (!sna_blt_copy_boxes(sna, GXcopy,
					sna_pixmap_get_bo(pixmap), 0, 0,
					bo, -box.x1, -box.y1,
					pixmap->drawable.bitsPerPixel,
					&box, 1)) {
			DBG(("%s: fallback -- unable to copy boxes\n",
			     __FUNCTION__));
			return -1;
		}
	}

	if (ox == x && oy == y) {
		x = y = 0;
	} else if (channel->transform) {
		pixman_vector_t v;
		pixman_transform_t m;

		v.vector[0] = (ox - box.x1) << 16;
		v.vector[1] = (oy - box.y1) << 16;
		v.vector[2] = 1 << 16;
		pixman_transform_invert(&m, channel->transform);
		pixman_transform_point(&m, &v);
		x = v.vector[0] / v.vector[2];
		y = v.vector[1] / v.vector[2];
	} else {
		x = ox - box.x1;
		y = oy - box.y1;
	}

	channel->offset[0] = x - dst_x;
	channel->offset[1] = y - dst_y;
	channel->scale[0] = 1./w;
	channel->scale[1] = 1./h;
	channel->width  = w;
	channel->height = h;
	channel->bo = bo;
	return 1;
}

int
sna_render_picture_fixup(struct sna *sna,
			 PicturePtr picture,
			 struct sna_composite_channel *channel,
			 int16_t x, int16_t y,
			 int16_t w, int16_t h,
			 int16_t dst_x, int16_t dst_y)
{
	pixman_image_t *dst, *src;
	uint32_t pitch;
	int dx, dy;
	void *ptr;

#if NO_FIXUP
	return -1;
#endif

	DBG(("%s: (%d, %d)x(%d, %d)\n", __FUNCTION__, x, y, w, h));

	if (w == 0 || h == 0) {
		DBG(("%s: fallback - unknown bounds\n", __FUNCTION__));
		return -1;
	}
	if (w > sna->render.max_3d_size || h > sna->render.max_3d_size) {
		DBG(("%s: fallback - too large (%dx%d)\n", __FUNCTION__, w, h));
		return -1;
	}

	if (PICT_FORMAT_RGB(picture->format) == 0) {
		pitch = ALIGN(w, 4);
		channel->pict_format = PIXMAN_a8;
	} else {
		pitch = sizeof(uint32_t)*w;
		channel->pict_format = PIXMAN_a8r8g8b8;
	}
	if (channel->pict_format != picture->format) {
		DBG(("%s: converting to %08x (pitch=%d) from %08x\n",
		     __FUNCTION__, channel->pict_format, pitch, picture->format));
	}

	channel->bo = kgem_create_buffer(&sna->kgem,
					 pitch*h, KGEM_BUFFER_WRITE,
					 &ptr);
	if (!channel->bo) {
		DBG(("%s: failed to create upload buffer, using clear\n",
		     __FUNCTION__));
		return 0;
	}

	/* XXX Convolution filter? */
	memset(ptr, 0, pitch*h);
	channel->bo->pitch = pitch;

	/* Composite in the original format to preserve idiosyncracies */
	if (picture->format == channel->pict_format)
		dst = pixman_image_create_bits(picture->format, w, h, ptr, pitch);
	else
		dst = pixman_image_create_bits(picture->format, w, h, NULL, 0);
	if (!dst) {
		kgem_bo_destroy(&sna->kgem, channel->bo);
		return 0;
	}

	if (picture->pDrawable)
		sna_drawable_move_to_cpu(picture->pDrawable, false);

	src = image_from_pict(picture, FALSE, &dx, &dy);
	if (src == NULL) {
		pixman_image_unref(dst);
		kgem_bo_destroy(&sna->kgem, channel->bo);
		return 0;
	}

	DBG(("%s: compositing tmp=(%d+%d, %d+%d)x(%d, %d)\n",
	     __FUNCTION__, x, dx, y, dy, w, h));
	pixman_image_composite(PictOpSrc, src, NULL, dst,
			       x + dx, y + dy,
			       0, 0,
			       0, 0,
			       w, h);
	free_pixman_pict(picture, src);

	/* Then convert to card format */
	if (picture->format != channel->pict_format) {
		DBG(("%s: performing post-conversion %08x->%08x (%d, %d)\n",
		     __FUNCTION__,
		     picture->format, channel->pict_format,
		     w, h));

		src = dst;
		dst = pixman_image_create_bits(channel->pict_format,
					       w, h, ptr, pitch);

		pixman_image_composite(PictOpSrc, src, NULL, dst,
				       0, 0,
				       0, 0,
				       0, 0,
				       w, h);
		pixman_image_unref(src);
	}
	pixman_image_unref(dst);

	channel->width  = w;
	channel->height = h;

	channel->filter = PictFilterNearest;
	channel->repeat = RepeatNone;
	channel->is_affine = TRUE;

	channel->scale[0] = 1./w;
	channel->scale[1] = 1./h;
	channel->offset[0] = -dst_x;
	channel->offset[1] = -dst_y;
	channel->transform = NULL;

	return 1;
}

int
sna_render_picture_convert(struct sna *sna,
			   PicturePtr picture,
			   struct sna_composite_channel *channel,
			   PixmapPtr pixmap,
			   int16_t x, int16_t y,
			   int16_t w, int16_t h,
			   int16_t dst_x, int16_t dst_y)
{
	uint32_t pitch;
	pixman_image_t *src, *dst;
	BoxRec box;
	void *ptr;

#if NO_CONVERT
	return -1;
#endif

	if (w != 0 && h != 0) {
		box.x1 = x;
		box.y1 = y;
		box.x2 = x + w;
		box.y2 = y + h;

		if (channel->transform) {
			DBG(("%s: has transform, converting whole surface\n",
			     __FUNCTION__));
			box.x1 = box.y1 = 0;
			box.x2 = pixmap->drawable.width;
			box.y2 = pixmap->drawable.height;
		}

		if (box.x1 < 0)
			box.x1 = 0;
		if (box.y1 < 0)
			box.y1 = 0;
		if (box.x2 > pixmap->drawable.width)
			box.x2 = pixmap->drawable.width;
		if (box.y2 > pixmap->drawable.height)
			box.y2 = pixmap->drawable.height;
	} else {
		DBG(("%s: op no bounds, converting whole surface\n",
		     __FUNCTION__));
		box.x1 = box.y1 = 0;
		box.x2 = pixmap->drawable.width;
		box.y2 = pixmap->drawable.height;
	}

	w = box.x2 - box.x1;
	h = box.y2 - box.y1;

	DBG(("%s: convert (%d, %d)x(%d, %d), source size %dx%d\n",
	     __FUNCTION__, box.x1, box.y1, w, h,
	     pixmap->drawable.width,
	     pixmap->drawable.height));

	if (w == 0 || h == 0) {
		DBG(("%s: sample extents lie outside of source, using clear\n",
		     __FUNCTION__));
		return 0;
	}

	sna_pixmap_move_to_cpu(pixmap, false);

	src = pixman_image_create_bits(picture->format,
				       pixmap->drawable.width,
				       pixmap->drawable.height,
				       pixmap->devPrivate.ptr,
				       pixmap->devKind);
	if (!src)
		return 0;

	if (PICT_FORMAT_RGB(picture->format) == 0) {
		pitch = ALIGN(w, 4);
		channel->pict_format = PIXMAN_a8;
		DBG(("%s: converting to a8 (pitch=%d) from %08x\n",
		     __FUNCTION__, pitch, picture->format));
	} else {
		pitch = sizeof(uint32_t)*w;
		channel->pict_format = PIXMAN_a8r8g8b8;
		DBG(("%s: converting to a8r8g8b8 (pitch=%d) from %08x\n",
		     __FUNCTION__, pitch, picture->format));
	}

	channel->bo = kgem_create_buffer(&sna->kgem,
					 pitch*h, KGEM_BUFFER_WRITE,
					 &ptr);
	if (!channel->bo) {
		pixman_image_unref(src);
		return 0;
	}

	channel->bo->pitch = pitch;
	dst = pixman_image_create_bits(channel->pict_format, w, h, ptr, pitch);
	if (!dst) {
		kgem_bo_destroy(&sna->kgem, channel->bo);
		pixman_image_unref(src);
		return 0;
	}

	pixman_image_composite(PictOpSrc, src, NULL, dst,
			       box.x1, box.y1,
			       0, 0,
			       0, 0,
			       w, h);
	pixman_image_unref(dst);
	pixman_image_unref(src);

	channel->width  = w;
	channel->height = h;

	channel->scale[0] = 1. / w;
	channel->scale[1] = 1. / h;
	channel->offset[0] = x - dst_x - box.x1;
	channel->offset[1] = y - dst_y - box.y1;

	DBG(("%s: offset=(%d, %d), size=(%d, %d) ptr[0]=%08x\n",
	     __FUNCTION__,
	     channel->offset[0], channel->offset[1],
	     channel->width, channel->height,
	     *(uint32_t*)ptr));
	return 1;
}

Bool
sna_render_composite_redirect(struct sna *sna,
			      struct sna_composite_op *op,
			      int x, int y, int width, int height)
{
	struct sna_composite_redirect *t = &op->redirect;
	int bpp = op->dst.pixmap->drawable.bitsPerPixel;
	struct sna_pixmap *priv;
	struct kgem_bo *bo;

#if NO_REDIRECT
	return FALSE;
#endif

	DBG(("%s: target too large (%dx%d), copying to temporary %dx%d\n",
	     __FUNCTION__, op->dst.width, op->dst.height, width,height));

	if (!width || !height)
		return FALSE;

	priv = sna_pixmap(op->dst.pixmap);
	if (priv->gpu_bo == NULL) {
		DBG(("%s: fallback -- no GPU bo attached\n", __FUNCTION__));
		return FALSE;
	}

	if (!sna_pixmap_move_to_gpu(op->dst.pixmap))
		return FALSE;

	/* We can process the operation in a single pass,
	 * but the target is too large for the 3D pipeline.
	 * Copy into a smaller surface and replace afterwards.
	 */
	bo = kgem_create_2d(&sna->kgem,
			    width, height, bpp,
			    kgem_choose_tiling(&sna->kgem, I915_TILING_X,
					       width, height, bpp),
			    0);
	if (!bo)
		return FALSE;

	t->box.x1 = x + op->dst.x;
	t->box.y1 = y + op->dst.y;
	t->box.x2 = t->box.x1 + width;
	t->box.y2 = t->box.y1 + height;

	DBG(("%s: original box (%d, %d), (%d, %d)\n",
	     __FUNCTION__, t->box.x1, t->box.y1, t->box.x2, t->box.y2));

	if (!sna_blt_copy_boxes(sna, GXcopy,
				op->dst.bo, 0, 0,
				bo, -t->box.x1, -t->box.y1,
				bpp, &t->box, 1)) {
		kgem_bo_destroy(&sna->kgem, bo);
		return FALSE;
	}

	t->real_bo = priv->gpu_bo;
	op->dst.bo = bo;
	op->dst.x = -x;
	op->dst.y = -y;
	op->dst.width  = width;
	op->dst.height = height;
	op->damage = &priv->gpu_damage;
	return TRUE;
}

void
sna_render_composite_redirect_done(struct sna *sna,
				   const struct sna_composite_op *op)
{
	const struct sna_composite_redirect *t = &op->redirect;

	if (t->real_bo) {
		DBG(("%s: copying temporary to dst\n", __FUNCTION__));

		sna_blt_copy_boxes(sna, GXcopy,
				   op->dst.bo, -t->box.x1, -t->box.y1,
				   t->real_bo, 0, 0,
				   op->dst.pixmap->drawable.bitsPerPixel,
				   &t->box, 1);

		kgem_bo_destroy(&sna->kgem, op->dst.bo);
	}
}
