/*
 * Copyright © 2010 Intel Corporation
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

#include <fbpict.h>

#if DEBUG_RENDER
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

struct sna_tile_state {
	int op;
	PicturePtr src, mask, dst;
	PixmapPtr dst_pixmap;
	uint32_t dst_format;
	int16_t src_x, src_y;
	int16_t mask_x, mask_y;
	int16_t dst_x, dst_y;
	int16_t width, height;

	int rect_count;
	int rect_size;
	struct sna_composite_rectangles rects_embedded[16], *rects;
};

static void
sna_tiling_composite_add_rect(struct sna_tile_state *tile,
			      const struct sna_composite_rectangles *r)
{
	if (tile->rect_count == tile->rect_size) {
		struct sna_composite_rectangles *a;
		int newsize = tile->rect_size * 2;

		if (tile->rects == tile->rects_embedded) {
			a = malloc (sizeof(struct sna_composite_rectangles) * newsize);
			if (a == NULL)
				return;

			memcpy(a,
			       tile->rects_embedded,
			       sizeof(struct sna_composite_rectangles) * tile->rect_count);
		} else {
			a = realloc(tile->rects,
				    sizeof(struct sna_composite_rectangles) * newsize);
			if (a == NULL)
				return;
		}

		tile->rects = a;
		tile->rect_size = newsize;
	}

	tile->rects[tile->rect_count++] = *r;
}

fastcall static void
sna_tiling_composite_blt(struct sna *sna,
			 const struct sna_composite_op *op,
			 const struct sna_composite_rectangles *r)
{
	sna_tiling_composite_add_rect(op->u.priv, r);
}

static void
sna_tiling_composite_boxes(struct sna *sna,
			   const struct sna_composite_op *op,
			   const BoxRec *box, int nbox)
{
	while (nbox--) {
		struct sna_composite_rectangles r;

		r.dst.x = box->x1;
		r.dst.y = box->y1;
		r.mask = r.src = r.dst;

		r.width  = box->x2 - box->x1;
		r.height = box->y2 - box->y1;

		sna_tiling_composite_add_rect(op->u.priv, &r);
		box++;
	}
}

static void
sna_tiling_composite_done(struct sna *sna,
			  const struct sna_composite_op *op)
{
	struct sna_tile_state *tile = op->u.priv;
	struct sna_composite_op tmp;
	int x, y, n, step = sna->render.max_3d_size;

	DBG(("%s -- %dx%d, count=%d\n", __FUNCTION__,
	     tile->width, tile->height, tile->rect_count));

	if (tile->rect_count == 0)
		goto done;

	for (y = 0; y < tile->height; y += step) {
		int height = step;
		if (y + height > tile->height)
			height = tile->height - y;
		for (x = 0; x < tile->width; x += step) {
			int width = step;
			if (x + width > tile->width)
				width = tile->width - x;
			memset(&tmp, 0, sizeof(tmp));
			if (sna->render.composite(sna, tile->op,
						  tile->src, tile->mask, tile->dst,
						  tile->src_x + x,  tile->src_y + y,
						  tile->mask_x + x, tile->mask_y + y,
						  tile->dst_x + x,  tile->dst_y + y,
						  width, height,
						  &tmp)) {
				for (n = 0; n < tile->rect_count; n++) {
					const struct sna_composite_rectangles *r = &tile->rects[n];
					int x1, x2, dx, y1, y2, dy;

					x1 = r->dst.x - tile->dst_x, dx = 0;
					if (x1 < x)
						dx = x - x1, x1 = x;
					y1 = r->dst.y - tile->dst_y, dy = 0;
					if (y1 < y)
						dy = y - y1, y1 = y;

					x2 = r->dst.x + r->width - tile->dst_x;
					if (x2 > x + width)
						x2 = x + width;
					y2 = r->dst.y + r->height - tile->dst_y;
					if (y2 > y + height)
						y2 = y + height;

					if (y2 > y1 && x2 > x1) {
						struct sna_composite_rectangles rr;
						rr.src.x = dx + r->src.x;
						rr.src.y = dy + r->src.y;

						rr.mask.x = dx + r->mask.x;
						rr.mask.y = dy + r->mask.y;

						rr.dst.x = dx + r->dst.x;
						rr.dst.y = dy + r->dst.y;

						rr.width  = x2 - x1;
						rr.height = y2 - y1;

						tmp.blt(sna, &tmp, &rr);
					}
				}
				tmp.done(sna, &tmp);
			} else {
				DBG(("%s -- falback\n", __FUNCTION__));

				sna_drawable_move_to_cpu(tile->dst->pDrawable, true);
				if (tile->src->pDrawable)
					sna_drawable_move_to_cpu(tile->src->pDrawable, false);
				if (tile->mask && tile->mask->pDrawable)
					sna_drawable_move_to_cpu(tile->mask->pDrawable, false);

				fbComposite(tile->op,
					    tile->src, tile->mask, tile->dst,
					    tile->src_x + x,  tile->src_y + y,
					    tile->mask_x + x, tile->mask_y + y,
					    tile->dst_x + x,  tile->dst_y + y,
					    width, height);
			}
		}
	}

done:
	if (tile->rects != tile->rects_embedded)
		free(tile->rects);
	free(tile);
}

static inline int split(int x, int y)
{
	int n = x / y + 1;
	return (x + n - 1) / n;
}

Bool
sna_tiling_composite(struct sna *sna,
		     uint32_t op,
		     PicturePtr src,
		     PicturePtr mask,
		     PicturePtr dst,
		     int16_t src_x,  int16_t src_y,
		     int16_t mask_x, int16_t mask_y,
		     int16_t dst_x,  int16_t dst_y,
		     int16_t width,  int16_t height,
		     struct sna_composite_op *tmp)
{
	struct sna_tile_state *tile;
	struct sna_pixmap *priv;

	DBG(("%s size=(%d, %d), tile=%d\n",
	     __FUNCTION__, width, height, sna->render.max_3d_size));

	priv = sna_pixmap(get_drawable_pixmap(dst->pDrawable));
	if (priv == NULL || priv->gpu_bo == NULL)
		return FALSE;

	tile = malloc(sizeof(*tile));
	if (!tile)
		return FALSE;

	tile->op = op;

	tile->src  = src;
	tile->mask = mask;
	tile->dst  = dst;

	tile->src_x = src_x;
	tile->src_y = src_y;
	tile->mask_x = mask_x;
	tile->mask_y = mask_y;
	tile->dst_x = dst_x;
	tile->dst_y = dst_y;
	tile->width = width;
	tile->height = height;
	tile->rects = tile->rects_embedded;
	tile->rect_count = 0;
	tile->rect_size = ARRAY_SIZE(tile->rects_embedded);

	tmp->blt   = sna_tiling_composite_blt;
	tmp->boxes = sna_tiling_composite_boxes;
	tmp->done  = sna_tiling_composite_done;

	tmp->u.priv = tile;
	return TRUE;
}
