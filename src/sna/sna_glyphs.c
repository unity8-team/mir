/*
 * Copyright © 2010 Intel Corporation
 * Partly based on code Copyright © 2008 Red Hat, Inc.
 * Partly based on code Copyright © 2000 SuSE, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Intel not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Intel makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL INTEL
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * Red Hat DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL Red Hat
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Chris Wilson <chris@chris-wilson.co.uk>
 * Based on code by: Keith Packard <keithp@keithp.com> and Owen Taylor <otaylor@fishsoup.net>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sna.h"
#include "sna_render.h"
#include "sna_render_inline.h"

#include <mipict.h>
#include <fbpict.h>
#include <fb.h>

#if DEBUG_GLYPHS
#undef DBG
#define DBG(x) ErrorF x
#endif

#define FALLBACK 0
#define NO_GLYPH_CACHE 0
#define NO_GLYPHS_TO_DST 0
#define NO_GLYPHS_VIA_MASK 0
#define NO_SMALL_MASK 0
#define NO_GLYPHS_SLOW 0

#define CACHE_PICTURE_SIZE 1024
#define GLYPH_MIN_SIZE 8
#define GLYPH_MAX_SIZE 64
#define GLYPH_CACHE_SIZE (CACHE_PICTURE_SIZE * CACHE_PICTURE_SIZE / (GLYPH_MIN_SIZE * GLYPH_MIN_SIZE))

#if DEBUG_GLYPHS
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

static inline struct sna_glyph *sna_glyph(GlyphPtr glyph)
{
	return (struct sna_glyph *)glyph->devPrivates;
}

#define NeedsComponent(f) (PICT_FORMAT_A(f) != 0 && PICT_FORMAT_RGB(f) != 0)

static void unrealize_glyph_caches(struct sna *sna)
{
	struct sna_render *render = &sna->render;
	unsigned int i;

	DBG(("%s\n", __FUNCTION__));

	for (i = 0; i < ARRAY_SIZE(render->glyph); i++) {
		struct sna_glyph_cache *cache = &render->glyph[i];

		if (cache->picture)
			FreePicture(cache->picture, 0);

		free(cache->glyphs);
	}
	memset(render->glyph, 0, sizeof(render->glyph));
}

/* All caches for a single format share a single pixmap for glyph storage,
 * allowing mixing glyphs of different sizes without paying a penalty
 * for switching between source pixmaps. (Note that for a size of font
 * right at the border between two sizes, we might be switching for almost
 * every glyph.)
 *
 * This function allocates the storage pixmap, and then fills in the
 * rest of the allocated structures for all caches with the given format.
 */
static Bool realize_glyph_caches(struct sna *sna)
{
	ScreenPtr screen = sna->scrn->pScreen;
	unsigned int formats[] = {
		PIXMAN_a8,
		PIXMAN_a8r8g8b8,
	};
	unsigned int i;

	DBG(("%s\n", __FUNCTION__));

	if (sna->kgem.wedged || !sna->have_render)
		return TRUE;

	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		struct sna_glyph_cache *cache = &sna->render.glyph[i];
		struct sna_pixmap *priv;
		PixmapPtr pixmap;
		PicturePtr picture = NULL;
		PictFormatPtr pPictFormat;
		CARD32 component_alpha;
		int depth = PIXMAN_FORMAT_DEPTH(formats[i]);
		int error;

		pPictFormat = PictureMatchFormat(screen, depth, formats[i]);
		if (!pPictFormat)
			goto bail;

		/* Now allocate the pixmap and picture */
		pixmap = screen->CreatePixmap(screen,
					      CACHE_PICTURE_SIZE,
					      CACHE_PICTURE_SIZE,
					      depth,
					      SNA_CREATE_SCRATCH);
		if (!pixmap)
			goto bail;

		priv = sna_pixmap(pixmap);
		if (priv != NULL) {
			/* Prevent the cache from ever being paged out */
			priv->pinned = true;

			component_alpha = NeedsComponent(pPictFormat->format);
			picture = CreatePicture(0, &pixmap->drawable, pPictFormat,
						CPComponentAlpha, &component_alpha,
						serverClient, &error);
		}

		screen->DestroyPixmap(pixmap);
		if (!picture)
			goto bail;

		ValidatePicture(picture);

		cache->count = cache->evict = 0;
		cache->picture = picture;
		cache->glyphs = calloc(sizeof(struct sna_glyph *),
				       GLYPH_CACHE_SIZE);
		if (!cache->glyphs)
			goto bail;

		cache->evict = rand() % GLYPH_CACHE_SIZE;
	}

	return TRUE;

bail:
	unrealize_glyph_caches(sna);
	return FALSE;
}

static void
glyph_cache_upload(ScreenPtr screen,
		   struct sna_glyph_cache *cache,
		   GlyphPtr glyph,
		   int16_t x, int16_t y)
{
	DBG(("%s: upload glyph %p to cache (%d, %d)x(%d, %d)\n",
	     __FUNCTION__, glyph, x, y, glyph->info.width, glyph->info.height));
	sna_composite(PictOpSrc,
		      GlyphPicture(glyph)[screen->myNum], 0, cache->picture,
		      0, 0,
		      0, 0,
		      x, y,
		      glyph->info.width,
		      glyph->info.height);
}

static void
glyph_extents(int nlist,
	      GlyphListPtr list,
	      GlyphPtr *glyphs,
	      BoxPtr extents)
{
	int16_t x1, x2, y1, y2;
	int16_t x, y;

	x1 = y1 = MAXSHORT;
	x2 = y2 = MINSHORT;
	x = y = 0;
	while (nlist--) {
		int n = list->len;
		x += list->xOff;
		y += list->yOff;
		list++;
		while (n--) {
			GlyphPtr glyph = *glyphs++;

			if (glyph->info.width && glyph->info.height) {
				int v;

				v = x - glyph->info.x;
				if (v < x1)
					x1 = v;
				v += glyph->info.width;
				if (v > x2)
					x2 = v;

				v = y - glyph->info.y;
				if (v < y1)
					y1 = v;
				v += glyph->info.height;
				if (v > y2)
					y2 = v;
			}

			x += glyph->info.xOff;
			y += glyph->info.yOff;
		}
	}

	extents->x1 = x1;
	extents->x2 = x2;
	extents->y1 = y1;
	extents->y2 = y2;
}

static inline unsigned int
glyph_size_to_count(int size)
{
	size /= GLYPH_MIN_SIZE;
	return size * size;
}

static inline unsigned int
glyph_count_to_mask(int count)
{
	return ~(count - 1);
}

static inline unsigned int
glyph_size_to_mask(int size)
{
	return glyph_count_to_mask(glyph_size_to_count(size));
}

static int
glyph_cache(ScreenPtr screen,
	    struct sna_render *render,
	    GlyphPtr glyph)
{
	PicturePtr glyph_picture = GlyphPicture(glyph)[screen->myNum];
	struct sna_glyph_cache *cache = &render->glyph[PICT_FORMAT_RGB(glyph_picture->format) != 0];
	struct sna_glyph *priv;
	int size, mask, pos, s;

	if (NO_GLYPH_CACHE)
		return FALSE;

	if (glyph->info.width > GLYPH_MAX_SIZE ||
	    glyph->info.height > GLYPH_MAX_SIZE) {
		PixmapPtr pixmap = (PixmapPtr)glyph_picture->pDrawable;
		assert(glyph_picture->pDrawable->type == DRAWABLE_PIXMAP);
		if (pixmap->drawable.depth >= 8) {
			pixmap->usage_hint = 0;
			sna_pixmap_force_to_gpu(pixmap, MOVE_READ);
		}
		return FALSE;
	}

	for (size = GLYPH_MIN_SIZE; size <= GLYPH_MAX_SIZE; size *= 2)
		if (glyph->info.width <= size && glyph->info.height <= size)
			break;

	s = glyph_size_to_count(size);
	mask = glyph_count_to_mask(s);
	pos = (cache->count + s - 1) & mask;
	if (pos < GLYPH_CACHE_SIZE) {
		cache->count = pos + s;
	} else {
		priv = NULL;
		for (s = size; s <= GLYPH_MAX_SIZE; s *= 2) {
			int i = cache->evict & glyph_size_to_mask(s);
			priv = cache->glyphs[i];
			if (priv == NULL)
				continue;

			if (priv->size >= s) {
				cache->glyphs[i] = NULL;
				priv->atlas = NULL;
				pos = i;
			} else
				priv = NULL;
			break;
		}
		if (priv == NULL) {
			int count = glyph_size_to_count(size);
			pos = cache->evict & glyph_count_to_mask(count);
			for (s = 0; s < count; s++) {
				priv = cache->glyphs[pos + s];
				if (priv != NULL) {
					priv->atlas =NULL;
					cache->glyphs[pos + s] = NULL;
				}
			}
		}

		/* And pick a new eviction position */
		cache->evict = rand() % GLYPH_CACHE_SIZE;
	}
	assert(cache->glyphs[pos] == NULL);

	priv = sna_glyph(glyph);
	cache->glyphs[pos] = priv;
	priv->atlas = cache->picture;
	priv->size = size;
	priv->pos = pos << 1 | (PICT_FORMAT_RGB(glyph_picture->format) != 0);
	s = pos / ((GLYPH_MAX_SIZE / GLYPH_MIN_SIZE) * (GLYPH_MAX_SIZE / GLYPH_MIN_SIZE));
	priv->coordinate.x = s % (CACHE_PICTURE_SIZE / GLYPH_MAX_SIZE) * GLYPH_MAX_SIZE;
	priv->coordinate.y = (s / (CACHE_PICTURE_SIZE / GLYPH_MAX_SIZE)) * GLYPH_MAX_SIZE;
	for (s = GLYPH_MIN_SIZE; s < GLYPH_MAX_SIZE; s *= 2) {
		if (pos & 1)
			priv->coordinate.x += s;
		if (pos & 2)
			priv->coordinate.y += s;
		pos >>= 2;
	}

	glyph_cache_upload(screen, cache, glyph,
			   priv->coordinate.x, priv->coordinate.y);

	return TRUE;
}

static void apply_damage(struct sna_composite_op *op,
			 const struct sna_composite_rectangles *r)
{
	BoxRec box;

	if (op->damage == NULL)
		return;

	box.x1 = r->dst.x + op->dst.x;
	box.y1 = r->dst.y + op->dst.y;
	box.x2 = box.x1 + r->width;
	box.y2 = box.y1 + r->height;

	assert_pixmap_contains_box(op->dst.pixmap, &box);
	sna_damage_add_box(op->damage, &box);
}

static Bool
glyphs_to_dst(struct sna *sna,
	      CARD8 op,
	      PicturePtr src,
	      PicturePtr dst,
	      INT16 src_x, INT16 src_y,
	      int nlist, GlyphListPtr list, GlyphPtr *glyphs)
{
	struct sna_composite_op tmp;
	ScreenPtr screen = dst->pDrawable->pScreen;
	int index = screen->myNum;
	PicturePtr glyph_atlas;
	BoxPtr rects;
	int nrect;
	int16_t x, y;

	if (NO_GLYPHS_TO_DST)
		return FALSE;

	memset(&tmp, 0, sizeof(tmp));

	DBG(("%s(op=%d, src=(%d, %d), nlist=%d,  dst=(%d, %d)+(%d, %d))\n",
	     __FUNCTION__, op, src_x, src_y, nlist,
	     list->xOff, list->yOff, dst->pDrawable->x, dst->pDrawable->y));

	rects = REGION_RECTS(dst->pCompositeClip);
	nrect = REGION_NUM_RECTS(dst->pCompositeClip);

	x = dst->pDrawable->x;
	y = dst->pDrawable->y;
	src_x -= list->xOff + x;
	src_y -= list->yOff + y;

	glyph_atlas = NULL;
	while (nlist--) {
		int n = list->len;
		x += list->xOff;
		y += list->yOff;
		while (n--) {
			GlyphPtr glyph = *glyphs++;
			struct sna_glyph priv;
			int i;

			if (glyph->info.width == 0 || glyph->info.height == 0)
				goto next_glyph;

			priv = *sna_glyph(glyph);
			if (priv.atlas == NULL) {
				if (glyph_atlas) {
					tmp.done(sna, &tmp);
					glyph_atlas = NULL;
				}
				if (!glyph_cache(screen, &sna->render, glyph)) {
					/* no cache for this glyph */
					priv.atlas = GlyphPicture(glyph)[index];
					priv.coordinate.x = priv.coordinate.y = 0;
				} else
					priv = *sna_glyph(glyph);
			}

			if (priv.atlas != glyph_atlas) {
				if (glyph_atlas)
					tmp.done(sna, &tmp);

				if (!sna->render.composite(sna,
							   op, src, priv.atlas, dst,
							   0, 0, 0, 0, 0, 0,
							   0, 0,
							   &tmp))
					return FALSE;

				glyph_atlas = priv.atlas;
			}

			for (i = 0; i < nrect; i++) {
				struct sna_composite_rectangles r;
				int16_t dx, dy;
				int16_t x2, y2;

				r.dst.x = x - glyph->info.x;
				r.dst.y = y - glyph->info.y;
				x2 = r.dst.x + glyph->info.width;
				y2 = r.dst.y + glyph->info.height;
				dx = dy = 0;

				DBG(("%s: glyph=(%d, %d), (%d, %d), clip=(%d, %d), (%d, %d)\n",
				     __FUNCTION__,
				     r.dst.x, r.dst.y, x2, y2,
				     rects[i].x1, rects[i].y1,
				     rects[i].x2, rects[i].y2));
				if (rects[i].y1 >= y2)
					break;

				if (r.dst.x < rects[i].x1)
					dx = rects[i].x1 - r.dst.x, r.dst.x = rects[i].x1;
				if (x2 > rects[i].x2)
					x2 = rects[i].x2;
				if (r.dst.y < rects[i].y1)
					dy = rects[i].y1 - r.dst.y, r.dst.y = rects[i].y1;
				if (y2 > rects[i].y2)
					y2 = rects[i].y2;

				if (r.dst.x < x2 && r.dst.y < y2) {
					DBG(("%s: blt=(%d, %d), (%d, %d)\n",
					     __FUNCTION__, r.dst.x, r.dst.y, x2, y2));

					r.src.x = r.dst.x + src_x;
					r.src.y = r.dst.y + src_y;
					r.mask.x = dx + priv.coordinate.x;
					r.mask.y = dy + priv.coordinate.y;
					r.width  = x2 - r.dst.x;
					r.height = y2 - r.dst.y;
					tmp.blt(sna, &tmp, &r);
					apply_damage(&tmp, &r);
				}
			}

next_glyph:
			x += glyph->info.xOff;
			y += glyph->info.yOff;
		}
		list++;
	}
	if (glyph_atlas)
		tmp.done(sna, &tmp);

	return TRUE;
}

static Bool
glyphs_slow(struct sna *sna,
	    CARD8 op,
	    PicturePtr src,
	    PicturePtr dst,
	    INT16 src_x, INT16 src_y,
	    int nlist, GlyphListPtr list, GlyphPtr *glyphs)
{
	struct sna_composite_op tmp;
	ScreenPtr screen = dst->pDrawable->pScreen;
	int index = screen->myNum;
	int16_t x, y;

	if (NO_GLYPHS_SLOW)
		return FALSE;

	memset(&tmp, 0, sizeof(tmp));

	DBG(("%s(op=%d, src=(%d, %d), nlist=%d,  dst=(%d, %d)+(%d, %d))\n",
	     __FUNCTION__, op, src_x, src_y, nlist,
	     list->xOff, list->yOff, dst->pDrawable->x, dst->pDrawable->y));

	x = dst->pDrawable->x;
	y = dst->pDrawable->y;
	src_x -= list->xOff + x;
	src_y -= list->yOff + y;

	while (nlist--) {
		int n = list->len;
		x += list->xOff;
		y += list->yOff;
		while (n--) {
			GlyphPtr glyph = *glyphs++;
			struct sna_glyph priv;
			BoxPtr rects;
			int nrect;

			if (glyph->info.width == 0 || glyph->info.height == 0)
				goto next_glyph;

			priv = *sna_glyph(glyph);
			if (priv.atlas == NULL) {
				if (!glyph_cache(screen, &sna->render, glyph)) {
					/* no cache for this glyph */
					priv.atlas = GlyphPicture(glyph)[index];
					priv.coordinate.x = priv.coordinate.y = 0;
				} else
					priv = *sna_glyph(glyph);
			}

			DBG(("%s: glyph=(%d, %d)x(%d, %d), src=(%d, %d), mask=(%d, %d)\n",
			     __FUNCTION__,
			     x - glyph->info.x,
			     y - glyph->info.y,
			     glyph->info.width,
			     glyph->info.height,
			     src_x + x - glyph->info.x,
			     src_y + y - glyph->info.y,
			     priv.coordinate.x, priv.coordinate.y));

			if (!sna->render.composite(sna,
						   op, src, priv.atlas, dst,
						   src_x + x - glyph->info.x,
						   src_y + y - glyph->info.y,
						   priv.coordinate.x, priv.coordinate.y,
						   x - glyph->info.x,
						   y - glyph->info.y,
						   glyph->info.width,
						   glyph->info.height,
						   &tmp))
				return FALSE;

			rects = REGION_RECTS(dst->pCompositeClip);
			nrect = REGION_NUM_RECTS(dst->pCompositeClip);
			do {
				struct sna_composite_rectangles r;
				int16_t x2, y2;

				r.dst.x = x - glyph->info.x;
				r.dst.y = y - glyph->info.y;
				x2 = r.dst.x + glyph->info.width;
				y2 = r.dst.y + glyph->info.height;

				DBG(("%s: glyph=(%d, %d), (%d, %d), clip=(%d, %d), (%d, %d)\n",
				     __FUNCTION__,
				     r.dst.x, r.dst.y, x2, y2,
				     rects->x1, rects->y1,
				     rects->x2, rects->y2));
				if (rects->y1 >= y2)
					break;

				if (r.dst.x < rects->x1)
					r.dst.x = rects->x1;
				if (x2 > rects->x2)
					x2 = rects->x2;

				if (r.dst.y < rects->y1)
					r.dst.y = rects->y1;
				if (y2 > rects->y2)
					y2 = rects->y2;

				if (r.dst.x < x2 && r.dst.y < y2) {
					DBG(("%s: blt=(%d, %d), (%d, %d)\n",
					     __FUNCTION__, r.dst.x, r.dst.y, x2, y2));
					r.width  = x2 - r.dst.x;
					r.height = y2 - r.dst.y;
					r.src = r.mask = r .dst;
					tmp.blt(sna, &tmp, &r);
					apply_damage(&tmp, &r);
				}
				rects++;
			} while (--nrect);
			tmp.done(sna, &tmp);

next_glyph:
			x += glyph->info.xOff;
			y += glyph->info.yOff;
		}
		list++;
	}

	return TRUE;
}

static bool
clear_pixmap(struct sna *sna, PixmapPtr pixmap)
{
	struct sna_pixmap *priv = sna_pixmap(pixmap);
	return sna->render.clear(sna, pixmap, priv->gpu_bo);
}

static bool
too_large(struct sna *sna, int width, int height)
{
	return (width > sna->render.max_3d_size ||
		height > sna->render.max_3d_size);
}

static Bool
glyphs_via_mask(struct sna *sna,
		CARD8 op,
		PicturePtr src,
		PicturePtr dst,
		PictFormatPtr format,
		INT16 src_x, INT16 src_y,
		int nlist, GlyphListPtr list, GlyphPtr *glyphs)
{
	ScreenPtr screen = dst->pDrawable->pScreen;
	struct sna_composite_op tmp;
	int index = screen->myNum;
	CARD32 component_alpha;
	PixmapPtr pixmap;
	PicturePtr glyph_atlas, mask;
	int16_t x, y, width, height;
	int error;
	BoxRec box;

	if (NO_GLYPHS_VIA_MASK)
		return FALSE;

	DBG(("%s(op=%d, src=(%d, %d), nlist=%d,  dst=(%d, %d)+(%d, %d))\n",
	     __FUNCTION__, op, src_x, src_y, nlist,
	     list->xOff, list->yOff, dst->pDrawable->x, dst->pDrawable->y));

	glyph_extents(nlist, list, glyphs, &box);
	if (box.x2 <= box.x1 || box.y2 <= box.y1)
		return TRUE;

	DBG(("%s: bounds=((%d, %d), (%d, %d))\n", __FUNCTION__,
	     box.x1, box.y1, box.x2, box.y2));

	if (!sna_compute_composite_extents(&box,
					   src, NULL, dst,
					   src_x, src_y,
					   0, 0,
					   box.x1, box.y1,
					   box.x2 - box.x1,
					   box.y2 - box.y1))
		return TRUE;

	DBG(("%s: extents=((%d, %d), (%d, %d))\n", __FUNCTION__,
	     box.x1, box.y1, box.x2, box.y2));

	width  = box.x2 - box.x1;
	height = box.y2 - box.y1;
	box.x1 -= dst->pDrawable->x;
	box.y1 -= dst->pDrawable->y;
	x = -box.x1;
	y = -box.y1;
	src_x += box.x1 - list->xOff;
	src_y += box.y1 - list->yOff;

	if (format->depth < 8) {
		format = PictureMatchFormat(screen, 8, PICT_a8);
		if (!format)
			return FALSE;
	}

	component_alpha = NeedsComponent(format->format);
	if (!NO_SMALL_MASK &&
	    ((uint32_t)width * height * format->depth < 8 * 4096 ||
	     too_large(sna, width, height))) {
		pixman_image_t *mask_image;
		int s;

		DBG(("%s: small mask [format=%lx, depth=%d, size=%d], rendering glyphs to upload buffer\n",
		     __FUNCTION__, (unsigned long)format->format,
		     format->depth, (uint32_t)width*height*format->depth));

upload:
		pixmap = sna_pixmap_create_upload(screen,
						  width, height,
						  format->depth,
						  KGEM_BUFFER_WRITE);
		if (!pixmap)
			return FALSE;

		mask_image =
			pixman_image_create_bits(format->depth << 24 | format->format,
						 width, height,
						 pixmap->devPrivate.ptr,
						 pixmap->devKind);
		if (mask_image == NULL) {
			screen->DestroyPixmap(pixmap);
			return FALSE;
		}

		memset(pixmap->devPrivate.ptr, 0, pixmap->devKind*height);
		s = dst->pDrawable->pScreen->myNum;
		do {
			int n = list->len;
			x += list->xOff;
			y += list->yOff;
			while (n--) {
				GlyphPtr g = *glyphs++;
				PicturePtr picture;
				pixman_image_t *glyph_image;
				int16_t xi, yi;

				if (g->info.width == 0 || g->info.height == 0)
					goto next_image;

				/* If the mask has been cropped, it is likely
				 * that some of the glyphs fall outside.
				 */
				xi = x - g->info.x;
				yi = y - g->info.y;
				if (xi >= width || yi >= height)
					goto next_image;
				if (xi + g->info.width  <= 0 ||
				    yi + g->info.height <= 0)
					goto next_image;

				glyph_image = sna_glyph(g)->image;
				if (glyph_image == NULL) {
					int dx, dy;

					picture = GlyphPicture(g)[s];
					if (picture == NULL)
						goto next_image;

					glyph_image = image_from_pict(picture,
								      FALSE,
								      &dx, &dy);
					if (!glyph_image)
						goto next_image;

					assert(dx == 0 && dy == 0);
					sna_glyph(g)->image = glyph_image;
				}

				DBG(("%s: glyph to mask (%d, %d)x(%d, %d)\n",
				     __FUNCTION__,
				     xi, yi,
				     g->info.width,
				     g->info.height));

				pixman_image_composite(PictOpAdd,
						       glyph_image,
						       NULL,
						       mask_image,
						       0, 0,
						       0, 0,
						       xi, yi,
						       g->info.width,
						       g->info.height);

next_image:
				x += g->info.xOff;
				y += g->info.yOff;
			}
			list++;
		} while (--nlist);
		pixman_image_unref(mask_image);

		mask = CreatePicture(0, &pixmap->drawable,
				     format, CPComponentAlpha,
				     &component_alpha, serverClient, &error);
		screen->DestroyPixmap(pixmap);
		if (!mask)
			return FALSE;

		ValidatePicture(mask);
	} else {
		pixmap = screen->CreatePixmap(screen,
					      width, height, format->depth,
					      SNA_CREATE_SCRATCH);
		if (!pixmap)
			return FALSE;

		mask = CreatePicture(0, &pixmap->drawable,
				     format, CPComponentAlpha,
				     &component_alpha, serverClient, &error);
		screen->DestroyPixmap(pixmap);
		if (!mask)
			return FALSE;

		ValidatePicture(mask);
		if (!clear_pixmap(sna, pixmap)) {
			FreePicture(mask, 0);
			goto upload;
		}

		memset(&tmp, 0, sizeof(tmp));
		glyph_atlas = NULL;
		do {
			int n = list->len;
			x += list->xOff;
			y += list->yOff;
			while (n--) {
				GlyphPtr glyph = *glyphs++;
				struct sna_glyph *priv;
				PicturePtr this_atlas;
				struct sna_composite_rectangles r;

				if (glyph->info.width == 0 || glyph->info.height == 0)
					goto next_glyph;

				priv = sna_glyph(glyph);
				if (priv->atlas != NULL) {
					this_atlas = priv->atlas;
					r.src = priv->coordinate;
				} else {
					if (glyph_atlas) {
						tmp.done(sna, &tmp);
						glyph_atlas = NULL;
					}
					if (glyph_cache(screen, &sna->render, glyph)) {
						this_atlas = priv->atlas;
						r.src = priv->coordinate;
					} else {
						/* no cache for this glyph */
						this_atlas = GlyphPicture(glyph)[index];
						r.src.x = r.src.y = 0;
					}
				}

				if (this_atlas != glyph_atlas) {
					if (glyph_atlas)
						tmp.done(sna, &tmp);

					if (!sna->render.composite(sna, PictOpAdd,
								   this_atlas, NULL, mask,
								   0, 0, 0, 0, 0, 0,
								   0, 0,
								   &tmp)) {
						FreePicture(mask, 0);
						return FALSE;
					}

					glyph_atlas = this_atlas;
				}

				DBG(("%s: blt glyph origin (%d, %d), offset (%d, %d), src (%d, %d), size (%d, %d)\n",
				     __FUNCTION__,
				     x, y,
				     glyph->info.x, glyph->info.y,
				     r.src.x, r.src.y,
				     glyph->info.width, glyph->info.height));

				r.dst.x = x - glyph->info.x;
				r.dst.y = y - glyph->info.y;
				r.width  = glyph->info.width;
				r.height = glyph->info.height;
				tmp.blt(sna, &tmp, &r);

next_glyph:
				x += glyph->info.xOff;
				y += glyph->info.yOff;
			}
			list++;
		} while (--nlist);
		if (glyph_atlas)
			tmp.done(sna, &tmp);
	}

	sna_composite(op,
		      src, mask, dst,
		      src_x, src_y,
		      0, 0,
		      box.x1, box.y1,
		      width, height);

	FreePicture(mask, 0);
	return TRUE;
}

Bool sna_glyphs_create(struct sna *sna)
{
	return realize_glyph_caches(sna);
}

static PictFormatPtr
glyphs_format(int nlist, GlyphListPtr list, GlyphPtr * glyphs)
{
	PictFormatPtr format = list[0].format;
	int16_t x1, x2, y1, y2;
	int16_t x, y;
	BoxRec extents;
	Bool first = TRUE;

	x = 0;
	y = 0;
	extents.x1 = 0;
	extents.y1 = 0;
	extents.x2 = 0;
	extents.y2 = 0;
	while (nlist--) {
		int n = list->len;

		if (format->format != list->format->format)
			return NULL;

		x += list->xOff;
		y += list->yOff;
		list++;
		while (n--) {
			GlyphPtr glyph = *glyphs++;

			if (glyph->info.width == 0 || glyph->info.height == 0) {
				x += glyph->info.xOff;
				y += glyph->info.yOff;
				continue;
			}

			x1 = x - glyph->info.x;
			y1 = y - glyph->info.y;
			x2 = x1 + glyph->info.width;
			y2 = y1 + glyph->info.height;

			if (first) {
				extents.x1 = x1;
				extents.y1 = y1;
				extents.x2 = x2;
				extents.y2 = y2;
				first = FALSE;
			} else {
				/* Potential overlap */
				if (x1 < extents.x2 && x2 > extents.x1 &&
				    y1 < extents.y2 && y2 > extents.y1)
					return NULL;

				if (x1 < extents.x1)
					extents.x1 = x1;
				if (x2 > extents.x2)
					extents.x2 = x2;
				if (y1 < extents.y1)
					extents.y1 = y1;
				if (y2 > extents.y2)
					extents.y2 = y2;
			}
			x += glyph->info.xOff;
			y += glyph->info.yOff;
		}
	}

	return format;
}

static void
glyphs_fallback(CARD8 op,
		PicturePtr src,
		PicturePtr dst,
		PictFormatPtr mask_format,
		int src_x,
		int src_y,
		int nlist,
		GlyphListPtr list,
		GlyphPtr *glyphs)
{
	int screen = dst->pDrawable->pScreen->myNum;
	pixman_image_t *dst_image, *mask_image, *src_image;
	int dx, dy, x, y;
	BoxRec box;
	RegionRec region;

	glyph_extents(nlist, list, glyphs, &box);
	if (box.x2 <= box.x1 || box.y2 <= box.y1)
		return;

	DBG(("%s: (%d, %d), (%d, %d)\n",
	     __FUNCTION__, box.x1, box.y1, box.x2, box.y2));

	RegionInit(&region, &box, 0);
	RegionTranslate(&region, dst->pDrawable->x, dst->pDrawable->y);
	if (dst->pCompositeClip)
		RegionIntersect(&region, &region, dst->pCompositeClip);
	DBG(("%s: clipped extents (%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     RegionExtents(&region)->x1, RegionExtents(&region)->y1,
	     RegionExtents(&region)->x2, RegionExtents(&region)->y2));
	if (!RegionNotEmpty(&region))
		return;

	if (!sna_drawable_move_region_to_cpu(dst->pDrawable, &region,
					     MOVE_READ | MOVE_WRITE))
		return;
	if (dst->alphaMap &&
	    !sna_drawable_move_to_cpu(dst->alphaMap->pDrawable,
				      MOVE_READ | MOVE_WRITE))
		return;

	if (src->pDrawable) {
		if (!sna_drawable_move_to_cpu(src->pDrawable,
					      MOVE_READ))
			return;

		if (src->alphaMap &&
		    !sna_drawable_move_to_cpu(src->alphaMap->pDrawable,
					      MOVE_READ))
			return;
	}
	RegionTranslate(&region, -dst->pDrawable->x, -dst->pDrawable->y);

	dst_image = image_from_pict(dst, TRUE, &x, &y);
	if (dst_image == NULL)
		goto cleanup_region;
	DBG(("%s: dst offset (%d, %d)\n", __FUNCTION__, x, y));
	if (x | y) {
		region.extents.x1 += x;
		region.extents.x2 += x;
		region.extents.y1 += y;
		region.extents.y2 += y;
	}

	src_image = image_from_pict(src, FALSE, &dx, &dy);
	if (src_image == NULL)
		goto cleanup_dst;
	DBG(("%s: src offset (%d, %d)\n", __FUNCTION__, dx, dy));
	src_x += dx - list->xOff;
	src_y += dy - list->yOff;

	if (mask_format) {
		DBG(("%s: create mask (%d, %d)x(%d,%d) + (%d,%d) + (%d,%d), depth=%d, format=%lx [%lx], ca? %d\n",
		     __FUNCTION__,
		     region.extents.x1, region.extents.y1,
		     region.extents.x2 - region.extents.x1,
		     region.extents.y2 - region.extents.y1,
		     dst->pDrawable->x, dst->pDrawable->y,
		     x, y,
		     mask_format->depth,
		     (long)mask_format->format,
		     (long)(mask_format->depth << 24 | mask_format->format),
		     NeedsComponent(mask_format->format)));
		mask_image =
			pixman_image_create_bits(mask_format->depth << 24 | mask_format->format,
						 region.extents.x2 - region.extents.x1,
						 region.extents.y2 - region.extents.y1,
						 NULL, 0);
		if (mask_image == NULL)
			goto cleanup_src;
		if (NeedsComponent(mask_format->format))
			pixman_image_set_component_alpha(mask_image, TRUE);

		x -= region.extents.x1;
		y -= region.extents.y1;
	} else {
		mask_image = dst_image;
		src_x -= x;
		src_y -= y;
	}

	do {
		int n = list->len;
		x += list->xOff;
		y += list->yOff;
		while (n--) {
			GlyphPtr g = *glyphs++;
			pixman_image_t *glyph_image;

			if (g->info.width == 0 || g->info.height == 0)
				goto next_glyph;

			glyph_image = sna_glyph(g)->image;
			if (glyph_image == NULL) {
				PicturePtr picture;
				int dx, dy;

				picture = GlyphPicture(g)[screen];
				if (picture == NULL)
					goto next_glyph;

				glyph_image = image_from_pict(picture,
							      FALSE,
							      &dx, &dy);
				if (!glyph_image)
					goto next_glyph;

				assert(dx == 0 && dy == 0);
				sna_glyph(g)->image = glyph_image;
			}

			if (mask_format) {
				DBG(("%s: glyph+(%d,%d) to mask (%d, %d)x(%d, %d)\n",
				     __FUNCTION__,
				     dx, dy,
				     x - g->info.x,
				     y - g->info.y,
				     g->info.width,
				     g->info.height));

				pixman_image_composite(PictOpAdd,
						       glyph_image,
						       NULL,
						       mask_image,
						       dx, dy,
						       0, 0,
						       x - g->info.x,
						       y - g->info.y,
						       g->info.width,
						       g->info.height);
			} else {
				int xi = x - g->info.x;
				int yi = y - g->info.y;

				DBG(("%s: glyph+(%d, %d) to dst (%d, %d)x(%d, %d), src (%d, %d) [op=%d]\n",
				     __FUNCTION__,
				     dx, dy,
				     xi, yi,
				     g->info.width, g->info.height,
				     src_x + xi,
				     src_y + yi,
				     op));

				pixman_image_composite(op,
						       src_image,
						       glyph_image,
						       dst_image,
						       src_x + xi,
						       src_y + yi,
						       dx, dy,
						       xi, yi,
						       g->info.width,
						       g->info.height);
			}
next_glyph:
			x += g->info.xOff;
			y += g->info.yOff;
		}
		list++;
	} while (--nlist);

	if (mask_format) {
		DBG(("%s: glyph mask composite src=(%d+%d,%d+%d) dst=(%d, %d)x(%d, %d)\n",
		     __FUNCTION__,
		     src_x, region.extents.x1, src_y, region.extents.y1,
		     region.extents.x1, region.extents.y1,
		     region.extents.x2 - region.extents.x1,
		     region.extents.y2 - region.extents.y1));
		pixman_image_composite(op, src_image, mask_image, dst_image,
				       src_x, src_y,
				       0, 0,
				       region.extents.x1, region.extents.y1,
				       region.extents.x2 - region.extents.x1,
				       region.extents.y2 - region.extents.y1);
		pixman_image_unref(mask_image);
	}

cleanup_src:
	free_pixman_pict(src, src_image);
cleanup_dst:
	free_pixman_pict(dst, dst_image);
cleanup_region:
	RegionUninit(&region);
}

void
sna_glyphs(CARD8 op,
	   PicturePtr src,
	   PicturePtr dst,
	   PictFormatPtr mask,
	   INT16 src_x, INT16 src_y,
	   int nlist, GlyphListPtr list, GlyphPtr *glyphs)
{
	PixmapPtr pixmap = get_drawable_pixmap(dst->pDrawable);
	struct sna *sna = to_sna_from_pixmap(pixmap);
	struct sna_pixmap *priv;
	PictFormatPtr _mask;

	DBG(("%s(op=%d, nlist=%d, src=(%d, %d))\n",
	     __FUNCTION__, op, nlist, src_x, src_y));

	if (REGION_NUM_RECTS(dst->pCompositeClip) == 0)
		return;

	if (FALLBACK || DEBUG_NO_RENDER)
		goto fallback;

	if (wedged(sna)) {
		DBG(("%s: wedged\n", __FUNCTION__));
		goto fallback;
	}

	if (dst->alphaMap) {
		DBG(("%s: fallback -- dst alpha map\n", __FUNCTION__));
		goto fallback;
	}

	priv = sna_pixmap(pixmap);
	if (priv == NULL) {
		DBG(("%s: fallback -- destination unattached\n", __FUNCTION__));
		goto fallback;
	}

	if (too_small(priv) && !picture_is_gpu(src)) {
		DBG(("%s: fallback -- too small (%dx%d)\n",
		     __FUNCTION__, dst->pDrawable->width, dst->pDrawable->height));
		goto fallback;
	}

	_mask = mask;
	/* XXX discard the mask for non-overlapping glyphs? */

	if (!_mask ||
	    (((nlist == 1 && list->len == 1) || op == PictOpAdd) &&
	     dst->format == (_mask->depth << 24 | _mask->format))) {
		if (glyphs_to_dst(sna, op,
				  src, dst,
				  src_x, src_y,
				  nlist, list, glyphs))
			return;
	}

	if (!_mask)
		_mask = glyphs_format(nlist, list, glyphs);
	if (_mask) {
		if (glyphs_via_mask(sna, op,
				    src, dst, _mask,
				    src_x, src_y,
				    nlist, list, glyphs))
			return;
	} else {
		if (glyphs_slow(sna, op,
				src, dst,
				src_x, src_y,
				nlist, list, glyphs))
			return;
	}

fallback:
	glyphs_fallback(op, src, dst, mask, src_x, src_y, nlist, list, glyphs);
}

void
sna_glyph_unrealize(ScreenPtr screen, GlyphPtr glyph)
{
	struct sna_glyph *priv = sna_glyph(glyph);

	if (priv->image) {
		pixman_image_unref(priv->image);
		priv->image = NULL;
	}

	if (priv->atlas) {
		struct sna *sna = to_sna_from_screen(screen);
		struct sna_glyph_cache *cache = &sna->render.glyph[priv->pos&1];
		assert(cache->glyphs[priv->pos >> 1] == priv);
		cache->glyphs[priv->pos >> 1] = NULL;
		priv->atlas = NULL;
	}
}

void sna_glyphs_close(struct sna *sna)
{
	unrealize_glyph_caches(sna);
}
