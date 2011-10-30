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
#include "sna_render.h"
#include "sna_render_inline.h"

#include <mipict.h>
#include <fbpict.h>

#if DEBUG_COMPOSITE
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

static void dst_move_area_to_cpu(PicturePtr picture,
				 uint8_t op,
				 BoxPtr box)
{
	RegionRec area;

	DBG(("%s: (%d, %d), (%d %d)\n", __FUNCTION__,
	     box->x1, box->y1, box->x2, box->y2));

	RegionInit(&area, box, 1);
	if (picture->pCompositeClip)
		RegionIntersect(&area, &area, picture->pCompositeClip);
	sna_drawable_move_region_to_cpu(picture->pDrawable, &area, true);
	RegionUninit(&area);

	/* XXX use op to avoid a readback? */
	(void)op;
}

#define BOUND(v)	(INT16) ((v) < MINSHORT ? MINSHORT : (v) > MAXSHORT ? MAXSHORT : (v))

static inline bool
region_is_singular(pixman_region16_t *region)
{
	return region->data == NULL;
}

static inline pixman_bool_t
clip_to_dst(pixman_region16_t *region,
	    pixman_region16_t *clip,
	    int		dx,
	    int		dy)
{
	DBG(("%s: region: %dx[(%d, %d), (%d, %d)], clip: %dx[(%d, %d), (%d, %d)]\n",
	     __FUNCTION__,
	     pixman_region_n_rects(region),
	     region->extents.x1, region->extents.y1,
	     region->extents.x2, region->extents.y2,
	     pixman_region_n_rects(clip),
	     clip->extents.x1, clip->extents.y1,
	     clip->extents.x2, clip->extents.y2));

	if (region_is_singular(region) && region_is_singular(clip)) {
		pixman_box16_t *r = &region->extents;
		pixman_box16_t *c = &clip->extents;
		int v;

		if (r->x1 < (v = c->x1 + dx))
			r->x1 = BOUND(v);
		if (r->x2 > (v = c->x2 + dx))
			r->x2 = BOUND(v);
		if (r->y1 < (v = c->y1 + dy))
			r->y1 = BOUND(v);
		if (r->y2 > (v = c->y2 + dy))
			r->y2 = BOUND(v);

		if (r->x1 >= r->x2 || r->y1 >= r->y2) {
			pixman_region_init(region);
			return FALSE;
		}

		return TRUE;
	} else if (!pixman_region_not_empty(clip)) {
		return FALSE;
	} else {
		if (dx | dy)
			pixman_region_translate(region, -dx, -dy);
		if (!pixman_region_intersect(region, region, clip))
			return FALSE;
		if (dx | dy)
			pixman_region_translate(region, dx, dy);

		return pixman_region_not_empty(region);
	}
}

static inline Bool
clip_to_src(RegionPtr region, PicturePtr p, int dx, int	 dy)
{
	Bool result;

	if (p->clientClipType == CT_NONE)
		return TRUE;

	pixman_region_translate(p->clientClip,
				p->clipOrigin.x + dx,
				p->clipOrigin.y + dy);

	result = RegionIntersect(region, region, p->clientClip);

	pixman_region_translate(p->clientClip,
				-(p->clipOrigin.x + dx),
				-(p->clipOrigin.y + dy));

	return result && pixman_region_not_empty(region);
}

Bool
sna_compute_composite_region(RegionPtr region,
			     PicturePtr src, PicturePtr mask, PicturePtr dst,
			     INT16 src_x,  INT16 src_y,
			     INT16 mask_x, INT16 mask_y,
			     INT16 dst_x,  INT16 dst_y,
			     CARD16 width, CARD16 height)
{
	int v;

	DBG(("%s: dst=(%d, %d)x(%d, %d)\n",
	     __FUNCTION__,
	     dst_x, dst_y,
	     width, height));

	region->extents.x1 = dst_x < 0 ? 0 : dst_x;
	v = dst_x + width;
	if (v > dst->pDrawable->width)
		v = dst->pDrawable->width;
	region->extents.x2 = v;

	region->extents.y1 = dst_y < 0 ? 0 : dst_y;
	v = dst_y + height;
	if (v > dst->pDrawable->height)
		v = dst->pDrawable->height;
	region->extents.y2 = v;

	region->data = 0;

	DBG(("%s: initial clip against dst->pDrawable: (%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     region->extents.x1, region->extents.y1,
	     region->extents.x2, region->extents.y2));

	if (region->extents.x1 >= region->extents.x2 ||
	    region->extents.y1 >= region->extents.y2)
		return FALSE;

	region->extents.x1 += dst->pDrawable->x;
	region->extents.x2 += dst->pDrawable->x;
	region->extents.y1 += dst->pDrawable->y;
	region->extents.y2 += dst->pDrawable->y;

	dst_x += dst->pDrawable->x;
	dst_y += dst->pDrawable->y;

	/* clip against dst */
	if (!clip_to_dst(region, dst->pCompositeClip, 0, 0))
		return FALSE;

	DBG(("%s: clip against dst->pCompositeClip: (%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     region->extents.x1, region->extents.y1,
	     region->extents.x2, region->extents.y2));

	if (dst->alphaMap) {
		if (!clip_to_dst(region, dst->alphaMap->pCompositeClip,
				 -dst->alphaOrigin.x,
				 -dst->alphaOrigin.y)) {
			pixman_region_fini (region);
			return FALSE;
		}
	}

	/* clip against src */
	if (src->pDrawable) {
		src_x += src->pDrawable->x;
		src_y += src->pDrawable->y;
	}
	if (!clip_to_src(region, src, dst_x - src_x, dst_y - src_y)) {
		pixman_region_fini (region);
		return FALSE;
	}
	DBG(("%s: clip against src: (%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     region->extents.x1, region->extents.y1,
	     region->extents.x2, region->extents.y2));

	if (src->alphaMap) {
		if (!clip_to_src(region, src->alphaMap,
				 dst_x - (src_x - src->alphaOrigin.x),
				 dst_y - (src_y - src->alphaOrigin.y))) {
			pixman_region_fini(region);
			return FALSE;
		}
	}

	/* clip against mask */
	if (mask) {
		if (mask->pDrawable) {
			mask_x += mask->pDrawable->x;
			mask_y += mask->pDrawable->y;
		}
		if (!clip_to_src(region, mask, dst_x - mask_x, dst_y - mask_y)) {
			pixman_region_fini(region);
			return FALSE;
		}
		if (mask->alphaMap) {
			if (!clip_to_src(region, mask->alphaMap,
					 dst_x - (mask_x - mask->alphaOrigin.x),
					 dst_y - (mask_y - mask->alphaOrigin.y))) {
				pixman_region_fini(region);
				return FALSE;
			}
		}

		DBG(("%s: clip against mask: (%d, %d), (%d, %d)\n",
		     __FUNCTION__,
		     region->extents.x1, region->extents.y1,
		     region->extents.x2, region->extents.y2));
	}

	return pixman_region_not_empty(region);
}

static void
trim_extents(BoxPtr extents, const PicturePtr p, int dx, int dy)
{
	const BoxPtr box = REGION_EXTENTS(NULL, p->pCompositeClip);

	DBG(("%s: trim((%d, %d), (%d, %d)) against ((%d, %d), (%d, %d)) + (%d, %d)\n",
	     __FUNCTION__,
	     extents->x1, extents->y1, extents->x2, extents->y2,
	     box->x1, box->y1, box->x2, box->y2,
	     dx, dy));

	if (extents->x1 < box->x1 + dx)
		extents->x1 = box->x1 + dx;
	if (extents->x2 > box->x2 + dx)
		extents->x2 = box->x2 + dx;

	if (extents->y1 < box->y1 + dy)
		extents->y1 = box->y1 + dy;
	if (extents->y2 > box->y2 + dy)
		extents->y2 = box->y2 + dy;
}

static void
_trim_source_extents(BoxPtr extents, const PicturePtr p, int dx, int dy)
{
	if (p->clientClipType != CT_NONE)
		trim_extents(extents, p, dx, dy);
}

static void
trim_source_extents(BoxPtr extents, const PicturePtr p, int dx, int dy)
{
	if (p->pDrawable) {
		dx += p->pDrawable->x;
		dy += p->pDrawable->y;
	}
	_trim_source_extents(extents, p, dx, dy);
	if (p->alphaMap)
		_trim_source_extents(extents, p->alphaMap,
				     dx - p->alphaOrigin.x,
				     dy - p->alphaOrigin.y);

	DBG(("%s: -> (%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     extents->x1, extents->y1,
	     extents->x2, extents->y2));
}

Bool
sna_compute_composite_extents(BoxPtr extents,
			      PicturePtr src, PicturePtr mask, PicturePtr dst,
			      INT16 src_x,  INT16 src_y,
			      INT16 mask_x, INT16 mask_y,
			      INT16 dst_x,  INT16 dst_y,
			      CARD16 width, CARD16 height)
{
	int v;

	DBG(("%s: dst=(%d, %d)x(%d, %d)\n",
	     __FUNCTION__,
	     dst_x, dst_y,
	     width, height));

	extents->x1 = dst_x < 0 ? 0 : dst_x;
	v = dst_x + width;
	if (v > dst->pDrawable->width)
		v = dst->pDrawable->width;
	extents->x2 = v;

	extents->y1 = dst_y < 0 ? 0 : dst_y;
	v = dst_y + height;
	if (v > dst->pDrawable->height)
		v = dst->pDrawable->height;

	DBG(("%s: initial clip against dst->pDrawable: (%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     extents->x1, extents->y1,
	     extents->x2, extents->y2));

	if (extents->x1 >= extents->x2 ||
	    extents->y1 >= extents->y2)
		return FALSE;

	extents->x1 += dst->pDrawable->x;
	extents->x2 += dst->pDrawable->x;
	extents->y1 += dst->pDrawable->y;
	extents->y2 += dst->pDrawable->y;

	dst_x += dst->pDrawable->x;
	dst_y += dst->pDrawable->y;

	/* clip against dst */
	trim_extents(extents, dst, 0, 0);
	if (dst->alphaMap)
		trim_extents(extents, dst->alphaMap,
			     -dst->alphaOrigin.x,
			     -dst->alphaOrigin.y);

	DBG(("%s: clip against dst: (%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     extents->x1, extents->y1,
	     extents->x2, extents->y2));

	trim_source_extents(extents, src, dst_x - src_x, dst_y - src_y);
	if (mask)
		trim_source_extents(extents, mask,
				    dst_x - mask_x, dst_y - mask_y);

	return extents->x1 < extents->x2 && extents->y1 < extents->y2;
}

#if DEBUG_COMPOSITE
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

static void apply_damage(struct sna_composite_op *op, RegionPtr region)
{
	DBG(("%s: damage=%p, region=%d [(%d, %d), (%d, %d) + (%d, %d)]\n",
	     __FUNCTION__, op->damage, REGION_NUM_RECTS(region),
	     region->extents.x1, region->extents.y1,
	     region->extents.x2, region->extents.y2,
	     op->dst.x, op->dst.y));

	if (op->damage == NULL)
		return;

	if (op->dst.x | op->dst.y)
		RegionTranslate(region, op->dst.x, op->dst.y);

	assert_pixmap_contains_box(op->dst.pixmap, RegionExtents(region));
	sna_damage_add(op->damage, region);
}

void
sna_composite(CARD8 op,
	      PicturePtr src,
	      PicturePtr mask,
	      PicturePtr dst,
	      INT16 src_x,  INT16 src_y,
	      INT16 mask_x, INT16 mask_y,
	      INT16 dst_x,  INT16 dst_y,
	      CARD16 width, CARD16 height)
{
	struct sna *sna = to_sna_from_drawable(dst->pDrawable);
	struct sna_composite_op tmp;
	RegionRec region;
	int dx, dy;

	DBG(("%s(%d src=(%d, %d), mask=(%d, %d), dst=(%d, %d)+(%d, %d), size=(%d, %d)\n",
	     __FUNCTION__, op,
	     src_x, src_y,
	     mask_x, mask_y,
	     dst_x, dst_y, dst->pDrawable->x, dst->pDrawable->y,
	     width, height));

	if (mask && sna_composite_mask_is_opaque(mask))
		mask = NULL;

	if (!sna_compute_composite_region(&region,
					  src, mask, dst,
					  src_x,  src_y,
					  mask_x, mask_y,
					  dst_x, dst_y,
					  width,  height))
		return;

	if (wedged(sna)) {
		DBG(("%s: fallback -- wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (dst->alphaMap || src->alphaMap || (mask && mask->alphaMap)) {
		DBG(("%s: fallback due to unhandled alpha-map\n", __FUNCTION__));
		goto fallback;
	}

	if (too_small(dst->pDrawable) &&
	    !picture_is_gpu(src) && !picture_is_gpu(mask)) {
		DBG(("%s: fallback due to too small\n", __FUNCTION__));
		goto fallback;
	}

	dx = region.extents.x1 - (dst_x + dst->pDrawable->x);
	dy = region.extents.y1 - (dst_y + dst->pDrawable->y);

	DBG(("%s: composite region extents:+(%d, %d) -> (%d, %d), (%d, %d) + (%d, %d)\n",
	     __FUNCTION__,
	     dx, dy,
	     region.extents.x1, region.extents.y1,
	     region.extents.x2, region.extents.y2,
	     get_drawable_dx(dst->pDrawable),
	     get_drawable_dy(dst->pDrawable)));

	memset(&tmp, 0, sizeof(tmp));
	if (!sna->render.composite(sna,
				   op, src, mask, dst,
				   src_x + dx,  src_y + dy,
				   mask_x + dx, mask_y + dy,
				   region.extents.x1,
				   region.extents.y1,
				   region.extents.x2 - region.extents.x1,
				   region.extents.y2 - region.extents.y1,
				   &tmp)) {
		DBG(("%s: fallback due unhandled composite op\n", __FUNCTION__));
		goto fallback;
	}

	tmp.boxes(sna, &tmp,
		  REGION_RECTS(&region),
		  REGION_NUM_RECTS(&region));
	apply_damage(&tmp, &region);
	tmp.done(sna, &tmp);

	REGION_UNINIT(NULL, &region);
	return;

fallback:
	DBG(("%s -- fallback dst=(%d, %d)+(%d, %d), size=(%d, %d)\n",
	     __FUNCTION__,
	     dst_x, dst_y,
	     dst->pDrawable->x, dst->pDrawable->y,
	     width, height));

	dst_move_area_to_cpu(dst, op, &region.extents);
	if (src->pDrawable)
		sna_drawable_move_to_cpu(src->pDrawable, false);
	if (mask && mask->pDrawable)
		sna_drawable_move_to_cpu(mask->pDrawable, false);

	DBG(("%s: fallback -- fbCompposite\n", __FUNCTION__));
	fbComposite(op, src, mask, dst,
		    src_x, src_y,
		    mask_x, mask_y,
		    dst_x, dst_y,
		    width, height);
}

static int16_t bound(int16_t a, uint16_t b)
{
	int v = (int)a + (int)b;
	if (v > MAXSHORT)
		return MAXSHORT;
	return v;
}

static Bool
_pixman_region_init_clipped_rectangles(pixman_region16_t *region,
				       unsigned int num_rects,
				       xRectangle *rects,
				       int tx, int ty,
				       int maxx, int maxy)
{
	pixman_box16_t stack_boxes[64], *boxes = stack_boxes;
	pixman_bool_t ret;
	unsigned int i, j;

	if (num_rects > ARRAY_SIZE(stack_boxes)) {
		boxes = malloc(sizeof(pixman_box16_t) * num_rects);
		if (boxes == NULL)
			return FALSE;
	}

	for (i = j = 0; i < num_rects; i++) {
		boxes[j].x1 = rects[i].x + tx;
		if (boxes[j].x1 < 0)
			boxes[j].x1 = 0;

		boxes[j].y1 = rects[i].y + ty;
		if (boxes[j].y1 < 0)
			boxes[j].y1 = 0;

		boxes[j].x2 = bound(rects[i].x, rects[i].width);
		if (boxes[j].x2 > maxx)
			boxes[j].x2 = maxx;
		boxes[j].x2 += tx;

		boxes[j].y2 = bound(rects[i].y, rects[i].height);
		if (boxes[j].y2 > maxy)
			boxes[j].y2 = maxy;
		boxes[j].y2 += ty;

		if (boxes[j].x2 > boxes[j].x1 && boxes[j].y2 > boxes[j].y1)
			j++;
	}

	ret = TRUE;
	if (j)
	    ret = pixman_region_init_rects(region, boxes, j);
	else
	    pixman_region_init(region);

	if (boxes != stack_boxes)
		free(boxes);

	DBG(("%s: nrects=%d, region=(%d, %d), (%d, %d) x %d\n",
	     __FUNCTION__, num_rects,
	     region->extents.x1, region->extents.y1,
	     region->extents.x2, region->extents.y2,
	     pixman_region_n_rects(region)));
	return ret;
}

void
sna_composite_rectangles(CARD8		 op,
			 PicturePtr	 dst,
			 xRenderColor	*color,
			 int		 num_rects,
			 xRectangle	*rects)
{
	struct sna *sna = to_sna_from_drawable(dst->pDrawable);
	PixmapPtr pixmap;
	struct sna_pixmap *priv;
	pixman_region16_t region;
	pixman_box16_t *boxes;
	int16_t dst_x, dst_y;
	int num_boxes;
	int error;

	DBG(("%s(op=%d, %08x x %d [(%d, %d)x(%d, %d) ...])\n",
	     __FUNCTION__, op,
	     (color->alpha >> 8 << 24) |
	     (color->red   >> 8 << 16) |
	     (color->green >> 8 << 8) |
	     (color->blue  >> 8 << 0),
	     num_rects,
	     rects[0].x, rects[0].y, rects[0].width, rects[0].height));

	if (!num_rects)
		return;

	if (color->alpha <= 0x00ff) {
		switch (op) {
		case PictOpOver:
		case PictOpOutReverse:
		case PictOpAdd:
			return;
		case  PictOpInReverse:
		case  PictOpSrc:
			op = PictOpClear;
			break;
		case  PictOpAtopReverse:
			op = PictOpOut;
			break;
		case  PictOpXor:
			op = PictOpOverReverse;
			break;
		}
	} else if (color->alpha >= 0xff00) {
		switch (op) {
		case PictOpOver:
			op = PictOpSrc;
			break;
		case PictOpInReverse:
			return;
		case PictOpOutReverse:
			op = PictOpClear;
			break;
		case  PictOpAtopReverse:
			op = PictOpOverReverse;
			break;
		case  PictOpXor:
			op = PictOpOut;
			break;
		}
	}

	if (!pixman_region_not_empty(dst->pCompositeClip)) {
		DBG(("%s: empty clip, skipping\n", __FUNCTION__));
		return;
	}

	if (!_pixman_region_init_clipped_rectangles(&region,
						    num_rects, rects,
						    dst->pDrawable->x, dst->pDrawable->y,
						    dst->pDrawable->width, dst->pDrawable->height))
	{
		DBG(("%s: allocation failed for region\n", __FUNCTION__));
		return;
	}

	DBG(("%s: drawable extents (%d, %d),(%d, %d) x %d\n",
	     __FUNCTION__,
	     RegionExtents(&region)->x1, RegionExtents(&region)->y1,
	     RegionExtents(&region)->x2, RegionExtents(&region)->y2,
	     RegionNumRects(&region)));

	if (!pixman_region_intersect(&region, &region, dst->pCompositeClip) ||
	    !pixman_region_not_empty(&region)) {
		DBG(("%s: zero-intersection between rectangles and clip\n",
		     __FUNCTION__));
		pixman_region_fini(&region);
		return;
	}

	DBG(("%s: clipped extents (%d, %d),(%d, %d) x %d\n",
	     __FUNCTION__,
	     RegionExtents(&region)->x1, RegionExtents(&region)->y1,
	     RegionExtents(&region)->x2, RegionExtents(&region)->y2,
	     RegionNumRects(&region)));

	pixmap = get_drawable_pixmap(dst->pDrawable);
	get_drawable_deltas(dst->pDrawable, pixmap, &dst_x, &dst_y);
	pixman_region_translate(&region, dst_x, dst_y);

	DBG(("%s: pixmap +(%d, %d) extents (%d, %d),(%d, %d)\n",
	     __FUNCTION__, dst_x, dst_y,
	     RegionExtents(&region)->x1, RegionExtents(&region)->y1,
	     RegionExtents(&region)->x2, RegionExtents(&region)->y2));

	if (wedged(sna))
		goto fallback;

	if (dst->alphaMap) {
		DBG(("%s: fallback, dst has an alpha-map\n", __FUNCTION__));
		goto fallback;
	}

	boxes = pixman_region_rectangles(&region, &num_boxes);

	if (op == PictOpClear) {
		color->red = color->green = color->blue = color->alpha = 0;
	} else if (color->alpha >= 0xff00 && op == PictOpOver) {
		color->alpha = 0xffff;
		op = PictOpSrc;
	}

	if (too_small(dst->pDrawable)) {
		DBG(("%s: fallback, dst is too small\n", __FUNCTION__));
		goto fallback;
	}

	/* If we going to be overwriting any CPU damage with a subsequent
	 * operation, then we may as well delete it without moving it
	 * first to the GPU.
	 */
	if (op == PictOpSrc || op == PictOpClear) {
		priv = sna_pixmap_attach(pixmap);
		if (priv && !priv->gpu_only)
			sna_damage_subtract(&priv->cpu_damage, &region);
	}

	priv = sna_pixmap_move_to_gpu(pixmap);
	if (priv == NULL) {
		DBG(("%s: fallback due to no GPU bo\n", __FUNCTION__));
		goto fallback;
	}

	if (!sna->render.fill_boxes(sna, op, dst->format, color,
				    pixmap, priv->gpu_bo,
				    boxes, num_boxes)) {
		DBG(("%s: fallback - acceleration failed\n", __FUNCTION__));
		goto fallback;
	}

	if (!priv->gpu_only) {
		assert_pixmap_contains_box(pixmap, RegionExtents(&region));
		sna_damage_add(&priv->gpu_damage, &region);
	}

	goto done;

fallback:
	DBG(("%s: fallback\n", __FUNCTION__));
	sna_drawable_move_region_to_cpu(&pixmap->drawable, &region, true);

	if (op == PictOpSrc || op == PictOpClear) {
		PixmapPtr pixmap = get_drawable_pixmap(dst->pDrawable);
		int nbox = REGION_NUM_RECTS(&region);
		BoxPtr box = REGION_RECTS(&region);
		uint32_t pixel;

		if (sna_get_pixel_from_rgba(&pixel,
					     color->red,
					     color->green,
					     color->blue,
					     color->alpha,
					     dst->format)) {
			do {
				DBG(("%s: fallback fill: (%d, %d)x(%d, %d) %08x\n",
				     __FUNCTION__,
				     box->x1, box->y1,
				     box->x2 - box->x1,
				     box->y2 - box->y1,
				     pixel));

				pixman_fill(pixmap->devPrivate.ptr,
					    pixmap->devKind/sizeof(uint32_t),
					    pixmap->drawable.bitsPerPixel,
					    box->x1, box->y1,
					    box->x2 - box->x1,
					    box->y2 - box->y1,
					    pixel);
				box++;
			} while (--nbox);
		}
	} else {
		PicturePtr src;

		src = CreateSolidPicture(0, color, &error);
		if (src) {
			do {
				fbComposite(op, src, NULL, dst,
					    0, 0,
					    0, 0,
					    rects->x, rects->y,
					    rects->width, rects->height);
				rects++;
			} while (--num_rects);
			FreePicture(src, 0);
		}
	}

done:
	/* XXX xserver-1.8: CompositeRects is not tracked by Damage, so we must
	 * manually append the damaged regions ourselves.
	 */
	DamageRegionAppend(&pixmap->drawable, &region);
	DamageRegionProcessPending(&pixmap->drawable);

	pixman_region_fini(&region);
	return;
}
