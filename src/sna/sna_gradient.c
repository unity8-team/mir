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

#if DEBUG_GRADIENT
#undef DBG
#define DBG(x) ErrorF x
#endif

#define xFixedToDouble(f) pixman_fixed_to_double(f)

static int
sna_gradient_sample_width(PictGradient *gradient)
{
	int n, width;

	width = 2;
	for (n = 1; n < gradient->nstops; n++) {
		xFixed dx = gradient->stops[n].x - gradient->stops[n-1].x;
		uint16_t delta, max;
		int ramp;

		if (dx == 0)
			return 1024;

		max = gradient->stops[n].color.red -
			gradient->stops[n-1].color.red;

		delta = gradient->stops[n].color.green -
			gradient->stops[n-1].color.green;
		if (delta > max)
			max = delta;

		delta = gradient->stops[n].color.blue -
			gradient->stops[n-1].color.blue;
		if (delta > max)
			max = delta;

		delta = gradient->stops[n].color.alpha -
			gradient->stops[n-1].color.alpha;
		if (delta > max)
			max = delta;

		ramp = 128 * max / dx;
		if (ramp > width)
			width = ramp;
	}

	width *= gradient->nstops-1;
	width = (width + 7) & -8;
	return min(width, 1024);
}

static Bool
_gradient_color_stops_equal(PictGradient *pattern,
			    struct sna_gradient_cache *cache)
{
    if (cache->nstops != pattern->nstops)
	    return FALSE;

    return memcmp(cache->stops,
		  pattern->stops,
		  sizeof(PictGradientStop)*cache->nstops) == 0;
}

struct kgem_bo *
sna_render_get_gradient(struct sna *sna,
			PictGradient *pattern)
{
	struct sna_render *render = &sna->render;
	struct sna_gradient_cache *cache;
	pixman_image_t *gradient, *image;
	pixman_point_fixed_t p1, p2;
	int i, width;
	struct kgem_bo *bo;

	DBG(("%s: %dx[%f:%x ... %f:%x ... %f:%x]\n", __FUNCTION__,
	     pattern->nstops,
	     pattern->stops[0].x / 65536.,
	     pattern->stops[0].color.alpha >> 8 << 24 |
	     pattern->stops[0].color.red   >> 8 << 16 |
	     pattern->stops[0].color.green >> 8 << 8 |
	     pattern->stops[0].color.blue  >> 8 << 0,
	     pattern->stops[pattern->nstops/2].x / 65536.,
	     pattern->stops[pattern->nstops/2].color.alpha >> 8 << 24 |
	     pattern->stops[pattern->nstops/2].color.red   >> 8 << 16 |
	     pattern->stops[pattern->nstops/2].color.green >> 8 << 8 |
	     pattern->stops[pattern->nstops/2].color.blue  >> 8 << 0,
	     pattern->stops[pattern->nstops-1].x / 65536.,
	     pattern->stops[pattern->nstops-1].color.alpha >> 8 << 24 |
	     pattern->stops[pattern->nstops-1].color.red   >> 8 << 16 |
	     pattern->stops[pattern->nstops-1].color.green >> 8 << 8 |
	     pattern->stops[pattern->nstops-1].color.blue  >> 8 << 0));

	for (i = 0; i < render->gradient_cache.size; i++) {
		cache = &render->gradient_cache.cache[i];
		if (_gradient_color_stops_equal(pattern, cache)) {
			DBG(("%s: old --> %d\n", __FUNCTION__, i));
			return kgem_bo_reference(cache->bo);
		}
	}

	width = sna_gradient_sample_width(pattern);
	DBG(("%s: sample width = %d\n", __FUNCTION__, width));
	if (width == 0)
		return NULL;

	p1.x = 0;
	p1.y = 0;
	p2.x = width << 16;
	p2.y = 0;

	gradient = pixman_image_create_linear_gradient(&p1, &p2,
						       (pixman_gradient_stop_t *)pattern->stops,
						       pattern->nstops);
	if (gradient == NULL)
		return NULL;

	pixman_image_set_filter(gradient, PIXMAN_FILTER_BILINEAR, NULL, 0);
	pixman_image_set_repeat(gradient, PIXMAN_REPEAT_PAD);

	image = pixman_image_create_bits(PIXMAN_a8r8g8b8, width, 1, NULL, 0);
	if (image == NULL) {
		pixman_image_unref(gradient);
		return NULL;
	}

	pixman_image_composite(PIXMAN_OP_SRC,
			       gradient, NULL, image,
			       0, 0,
			       0, 0,
			       0, 0,
			       width, 1);
	pixman_image_unref(gradient);

	DBG(("%s: [0]=%x, [%d]=%x [%d]=%x\n", __FUNCTION__,
	     pixman_image_get_data(image)[0],
	     width/2, pixman_image_get_data(image)[width/2],
	     width-1, pixman_image_get_data(image)[width-1]));

	bo = kgem_create_linear(&sna->kgem, width*4, 0);
	if (!bo) {
		pixman_image_unref(image);
		return NULL;
	}

	bo->pitch = 4*width;
	kgem_bo_write(&sna->kgem, bo, pixman_image_get_data(image), 4*width);

	pixman_image_unref(image);

	if (render->gradient_cache.size < GRADIENT_CACHE_SIZE)
		i = render->gradient_cache.size++;
	else
		i = rand () % GRADIENT_CACHE_SIZE;

	cache = &render->gradient_cache.cache[i];
	if (cache->nstops < pattern->nstops) {
		PictGradientStop *newstops;

		newstops = malloc(sizeof(PictGradientStop) * pattern->nstops);
		if (newstops == NULL)
			return bo;

		free(cache->stops);
		cache->stops = newstops;
	}

	memcpy(cache->stops, pattern->stops,
	       sizeof(PictGradientStop) * pattern->nstops);
	cache->nstops = pattern->nstops;

	if (cache->bo)
		kgem_bo_destroy(&sna->kgem, cache->bo);
	cache->bo = kgem_bo_reference(bo);

	return bo;
}

void
sna_render_flush_solid(struct sna *sna)
{
	struct sna_solid_cache *cache = &sna->render.solid_cache;

	DBG(("sna_render_flush_solid(size=%d)\n", cache->size));
	assert(cache->dirty);
	assert(cache->size);

	kgem_bo_write(&sna->kgem, cache->cache_bo,
		      cache->color, cache->size*sizeof(uint32_t));
	cache->dirty = 0;
	cache->last = 0;
}

static void
sna_render_finish_solid(struct sna *sna, bool force)
{
	struct sna_solid_cache *cache = &sna->render.solid_cache;
	int i;

	DBG(("sna_render_finish_solid(force=%d, domain=%d, busy=%d, dirty=%d)\n",
	     force, cache->cache_bo->domain, cache->cache_bo->rq != NULL, cache->dirty));

	if (!force && cache->cache_bo->domain != DOMAIN_GPU)
		return;

	if (cache->dirty)
		sna_render_flush_solid(sna);

	for (i = 0; i < cache->size; i++) {
		if (cache->bo[i] == NULL)
			continue;

		kgem_bo_destroy(&sna->kgem, cache->bo[i]);
		cache->bo[i] = NULL;
	}
	kgem_bo_destroy(&sna->kgem, cache->cache_bo);

	DBG(("sna_render_finish_solid reset\n"));

	cache->cache_bo = kgem_create_linear(&sna->kgem, sizeof(cache->color), 0);
	cache->bo[0] = kgem_create_proxy(cache->cache_bo, 0, sizeof(uint32_t));
	cache->bo[0]->pitch = 4;
	if (force)
		cache->size = 1;
}

struct kgem_bo *
sna_render_get_solid(struct sna *sna, uint32_t color)
{
	struct sna_solid_cache *cache = &sna->render.solid_cache;
	int i;

	DBG(("%s: %08x\n", __FUNCTION__, color));

	if ((color & 0xffffff) == 0) /* alpha only */
		return kgem_bo_reference(sna->render.alpha_cache.bo[color>>24]);

	if (color == 0xffffffff) {
		DBG(("%s(white)\n", __FUNCTION__));
		return kgem_bo_reference(cache->bo[0]);
	}

	if (cache->color[cache->last] == color) {
		DBG(("sna_render_get_solid(%d) = %x (last)\n",
		     cache->last, color));
		return kgem_bo_reference(cache->bo[cache->last]);
	}

	for (i = 1; i < cache->size; i++) {
		if (cache->color[i] == color) {
			if (cache->bo[i] == NULL) {
				DBG(("sna_render_get_solid(%d) = %x (recreate)\n",
				     i, color));
				goto create;
			} else {
				DBG(("sna_render_get_solid(%d) = %x (old)\n",
				     i, color));
				goto done;
			}
		}
	}

	sna_render_finish_solid(sna, i == ARRAY_SIZE(cache->color));

	i = cache->size++;
	cache->color[i] = color;
	cache->dirty = 1;
	DBG(("sna_render_get_solid(%d) = %x (new)\n", i, color));

create:
	cache->bo[i] = kgem_create_proxy(cache->cache_bo,
					 i*sizeof(uint32_t), sizeof(uint32_t));
	cache->bo[i]->pitch = 4;

done:
	cache->last = i;
	return kgem_bo_reference(cache->bo[i]);
}

static Bool sna_alpha_cache_init(struct sna *sna)
{
	struct sna_alpha_cache *cache = &sna->render.alpha_cache;
	uint32_t color[256];
	int i;

	DBG(("%s\n", __FUNCTION__));

	cache->cache_bo = kgem_create_linear(&sna->kgem, sizeof(color), 0);
	if (!cache->cache_bo)
		return FALSE;

	for (i = 0; i < 256; i++) {
		color[i] = i << 24;
		cache->bo[i] = kgem_create_proxy(cache->cache_bo,
						 sizeof(uint32_t)*i,
						 sizeof(uint32_t));
		cache->bo[i]->pitch = 4;
	}
	kgem_bo_write(&sna->kgem, cache->cache_bo, color, sizeof(color));
	return TRUE;
}

static Bool sna_solid_cache_init(struct sna *sna)
{
	struct sna_solid_cache *cache = &sna->render.solid_cache;

	DBG(("%s\n", __FUNCTION__));

	cache->cache_bo =
		kgem_create_linear(&sna->kgem, sizeof(cache->color), 0);
	if (!cache->cache_bo)
		return FALSE;

	/*
	 * Initialise [0] with white since it is very common and filling the
	 * zeroth slot simplifies some of the checks.
	 */
	cache->color[0] = 0xffffffff;
	cache->bo[0] = kgem_create_proxy(cache->cache_bo, 0, sizeof(uint32_t));
	cache->bo[0]->pitch = 4;
	cache->dirty = 1;
	cache->size = 1;
	cache->last = 0;

	return TRUE;
}

Bool sna_gradients_create(struct sna *sna)
{
	DBG(("%s\n", __FUNCTION__));

	if (!sna_alpha_cache_init(sna))
		return FALSE;

	if (!sna_solid_cache_init(sna))
		return FALSE;

	return TRUE;
}

void sna_gradients_close(struct sna *sna)
{
	int i;

	DBG(("%s\n", __FUNCTION__));

	for (i = 0; i < 256; i++) {
		if (sna->render.alpha_cache.bo[i])
			kgem_bo_destroy(&sna->kgem, sna->render.alpha_cache.bo[i]);
	}
	if (sna->render.alpha_cache.cache_bo)
		kgem_bo_destroy(&sna->kgem, sna->render.alpha_cache.cache_bo);

	if (sna->render.solid_cache.cache_bo)
		kgem_bo_destroy(&sna->kgem, sna->render.solid_cache.cache_bo);
	for (i = 0; i < sna->render.solid_cache.size; i++) {
		if (sna->render.solid_cache.bo[i])
			kgem_bo_destroy(&sna->kgem, sna->render.solid_cache.bo[i]);
	}
	sna->render.solid_cache.cache_bo = 0;
	sna->render.solid_cache.size = 0;
	sna->render.solid_cache.dirty = 0;

	for (i = 0; i < sna->render.gradient_cache.size; i++) {
		struct sna_gradient_cache *cache =
			&sna->render.gradient_cache.cache[i];

		if (cache->bo)
			kgem_bo_destroy(&sna->kgem, cache->bo);

		free(cache->stops);
		cache->stops = NULL;
		cache->nstops = 0;
	}
	sna->render.gradient_cache.size = 0;
}
