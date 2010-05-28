/*
 * Copyright © 2008 Red Hat, Inc.
 * Partly based on code Copyright © 2000 SuSE, Inc.
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
 * Author: Owen Taylor <otaylor@fishsoup.net>
 * Based on code by: Keith Packard
 */

#ifdef HAVE_DIX_CONFIG_H
#include <dix-config.h>
#endif

#include <stdlib.h>

#include "uxa-priv.h"
#include "../src/common.h"

#include "mipict.h"

#if DEBUG_GLYPH_CACHE
#define DBG_GLYPH_CACHE(a) ErrorF a
#else
#define DBG_GLYPH_CACHE(a)
#endif

/* Width of the pixmaps we use for the caches; this should be less than
 * max texture size of the driver; this may need to actually come from
 * the driver.
 */
#define CACHE_PICTURE_WIDTH 1024

/* Maximum number of glyphs we buffer on the stack before flushing
 * rendering to the mask or destination surface.
 */
#define GLYPH_BUFFER_SIZE 256
#define GLYPH_CACHE_SIZE 256

typedef struct {
	PicturePtr source;
	uxa_composite_rect_t rects[GLYPH_BUFFER_SIZE];
	int count;
} uxa_glyph_buffer_t;

typedef enum {
	UXA_GLYPH_SUCCESS,	/* Glyph added to render buffer */
	UXA_GLYPH_FAIL,		/* out of memory, etc */
	UXA_GLYPH_NEED_FLUSH,	/* would evict a glyph already in the buffer */
} uxa_glyph_cache_result_t;

struct uxa_glyph {
	uxa_glyph_cache_t *cache;
	int pos;
};

static int uxa_glyph_index;

static inline struct uxa_glyph *uxa_glyph_get_private(GlyphPtr glyph)
{
	return dixLookupPrivate(&glyph->devPrivates, &uxa_glyph_index);
}

static inline void uxa_glyph_set_private(GlyphPtr glyph, struct uxa_glyph *priv)
{
	dixSetPrivate(&glyph->devPrivates, &uxa_glyph_index, priv);
}

void uxa_glyphs_init(ScreenPtr pScreen)
{
	uxa_screen_t *uxa_screen = uxa_get_screen(pScreen);
	int i = 0;

	dixRequestPrivate(&uxa_glyph_index, 0); /* XXX ignores status */

	memset(uxa_screen->glyphCaches, 0, sizeof(uxa_screen->glyphCaches));

	uxa_screen->glyphCaches[i].format = PICT_a8;
	uxa_screen->glyphCaches[i].glyphWidth =
	    uxa_screen->glyphCaches[i].glyphHeight = 16;
	i++;
	uxa_screen->glyphCaches[i].format = PICT_a8;
	uxa_screen->glyphCaches[i].glyphWidth =
	    uxa_screen->glyphCaches[i].glyphHeight = 32;
	i++;
	uxa_screen->glyphCaches[i].format = PICT_a8r8g8b8;
	uxa_screen->glyphCaches[i].glyphWidth =
	    uxa_screen->glyphCaches[i].glyphHeight = 16;
	i++;
	uxa_screen->glyphCaches[i].format = PICT_a8r8g8b8;
	uxa_screen->glyphCaches[i].glyphWidth =
	    uxa_screen->glyphCaches[i].glyphHeight = 32;
	i++;

	assert(i == UXA_NUM_GLYPH_CACHES);

	for (i = 0; i < UXA_NUM_GLYPH_CACHES; i++) {
		uxa_screen->glyphCaches[i].columns =
		    CACHE_PICTURE_WIDTH / uxa_screen->glyphCaches[i].glyphWidth;
	}
}

static void uxa_unrealize_glyph_caches(ScreenPtr pScreen, unsigned int format)
{
	uxa_screen_t *uxa_screen = uxa_get_screen(pScreen);
	int i;

	for (i = 0; i < UXA_NUM_GLYPH_CACHES; i++) {
		uxa_glyph_cache_t *cache = &uxa_screen->glyphCaches[i];

		if (cache->format != format)
			continue;

		if (cache->picture) {
			FreePicture((pointer) cache->picture, (XID) 0);
			cache->picture = NULL;
		}

		if (cache->glyphs) {
			xfree(cache->glyphs);
			cache->glyphs = NULL;
		}
		cache->glyphCount = 0;
	}
}

#define NeedsComponent(f) (PICT_FORMAT_A(f) != 0 && PICT_FORMAT_RGB(f) != 0)

/* All caches for a single format share a single pixmap for glyph storage,
 * allowing mixing glyphs of different sizes without paying a penalty
 * for switching between source pixmaps. (Note that for a size of font
 * right at the border between two sizes, we might be switching for almost
 * every glyph.)
 *
 * This function allocates the storage pixmap, and then fills in the
 * rest of the allocated structures for all caches with the given format.
 */
static Bool uxa_realize_glyph_caches(ScreenPtr pScreen, unsigned int format)
{
	uxa_screen_t *uxa_screen = uxa_get_screen(pScreen);
	int depth = PIXMAN_FORMAT_DEPTH(format);
	PictFormatPtr pPictFormat;
	PixmapPtr pPixmap;
	PicturePtr pPicture;
	CARD32 component_alpha;
	int height;
	int i;
	int error;

	pPictFormat = PictureMatchFormat(pScreen, depth, format);
	if (!pPictFormat)
		return FALSE;

	/* Compute the total vertical size needed for the format */

	height = 0;
	for (i = 0; i < UXA_NUM_GLYPH_CACHES; i++) {
		uxa_glyph_cache_t *cache = &uxa_screen->glyphCaches[i];
		int rows;

		if (cache->format != format)
			continue;

		cache->yOffset = height;

		rows = (GLYPH_CACHE_SIZE + cache->columns - 1) / cache->columns;
		height += rows * cache->glyphHeight;
	}

	/* Now allocate the pixmap and picture */

	pPixmap = (*pScreen->CreatePixmap) (pScreen,
					    CACHE_PICTURE_WIDTH,
					    height, depth,
					    INTEL_CREATE_PIXMAP_TILING_X);
	if (!pPixmap)
		return FALSE;

	component_alpha = NeedsComponent(pPictFormat->format);
	pPicture = CreatePicture(0, &pPixmap->drawable, pPictFormat,
				 CPComponentAlpha, &component_alpha,
				 serverClient, &error);

	(*pScreen->DestroyPixmap) (pPixmap);	/* picture holds a refcount */

	if (!pPicture)
		return FALSE;

	ValidatePicture(pPicture);

	/* And store the picture in all the caches for the format */

	for (i = 0; i < UXA_NUM_GLYPH_CACHES; i++) {
		uxa_glyph_cache_t *cache = &uxa_screen->glyphCaches[i];

		if (cache->format != format)
			continue;

		cache->picture = pPicture;
		cache->picture->refcnt++;
		cache->glyphs =
		    xcalloc(sizeof(GlyphPtr), GLYPH_CACHE_SIZE);
		cache->glyphCount = 0;

		if (!cache->glyphs)
			goto bail;

		cache->evictionPosition = rand() % GLYPH_CACHE_SIZE;
	}

	/* Each cache references the picture individually */
	FreePicture((pointer) pPicture, (XID) 0);
	return TRUE;

bail:
	uxa_unrealize_glyph_caches(pScreen, format);
	return FALSE;
}

void uxa_glyphs_fini(ScreenPtr pScreen)
{
	uxa_screen_t *uxa_screen = uxa_get_screen(pScreen);
	int i;

	for (i = 0; i < UXA_NUM_GLYPH_CACHES; i++) {
		uxa_glyph_cache_t *cache = &uxa_screen->glyphCaches[i];

		if (cache->picture)
			uxa_unrealize_glyph_caches(pScreen, cache->format);
	}
}

#define CACHE_X(pos) (((pos) % cache->columns) * cache->glyphWidth)
#define CACHE_Y(pos) (cache->yOffset + ((pos) / cache->columns) * cache->glyphHeight)

/* The most efficient thing to way to upload the glyph to the screen
 * is to use CopyArea; uxa pixmaps are always offscreen.
 */
static Bool
uxa_glyph_cache_upload_glyph(ScreenPtr pScreen,
			     uxa_glyph_cache_t * cache,
			     int pos, GlyphPtr pGlyph)
{
	PicturePtr pGlyphPicture = GlyphPicture(pGlyph)[pScreen->myNum];
	PixmapPtr pGlyphPixmap = (PixmapPtr) pGlyphPicture->pDrawable;
	PixmapPtr pCachePixmap = (PixmapPtr) cache->picture->pDrawable;
	PixmapPtr scratch;
	GCPtr pGC;

	/* UploadToScreen only works if bpp match */
	if (pGlyphPixmap->drawable.bitsPerPixel !=
	    pCachePixmap->drawable.bitsPerPixel)
		return FALSE;

	pGC = GetScratchGC(pCachePixmap->drawable.depth, pScreen);
	ValidateGC(&pCachePixmap->drawable, pGC);

	/* Create a temporary bo to stream the updates to the cache */
	scratch = (*pScreen->CreatePixmap)(pScreen,
					   pGlyph->info.width,
					   pGlyph->info.height,
					   pGlyphPixmap->drawable.depth,
					   UXA_CREATE_PIXMAP_FOR_MAP);
	if (scratch) {
		(void)uxa_copy_area(&pGlyphPixmap->drawable,
				    &scratch->drawable,
				    pGC,
				    0, 0,
				    pGlyph->info.width, pGlyph->info.height,
				    0, 0);
	} else {
		scratch = pGlyphPixmap;
	}

	(void)uxa_copy_area(&scratch->drawable,
			    &pCachePixmap->drawable,
			    pGC,
			    0, 0, pGlyph->info.width, pGlyph->info.height,
			    CACHE_X(pos), CACHE_Y(pos));

	if (scratch != pGlyphPixmap)
		(*pScreen->DestroyPixmap)(scratch);

	FreeScratchGC(pGC);

	return TRUE;
}

void
uxa_glyph_unrealize(ScreenPtr pScreen,
		    GlyphPtr pGlyph)
{
	struct uxa_glyph *priv;

	priv = uxa_glyph_get_private(pGlyph);
	if (priv == NULL)
		return;

	priv->cache->glyphs[priv->pos] = NULL;

	uxa_glyph_set_private(pGlyph, NULL);
	xfree(priv);
}

static uxa_glyph_cache_result_t
uxa_glyph_cache_buffer_glyph(ScreenPtr pScreen,
			     uxa_glyph_cache_t * cache,
			     uxa_glyph_buffer_t * buffer,
			     GlyphPtr pGlyph, int xGlyph, int yGlyph)
{
	uxa_composite_rect_t *rect;
	struct uxa_glyph *priv = NULL;
	int pos;

	if (buffer->source && buffer->source != cache->picture)
		return UXA_GLYPH_NEED_FLUSH;

	if (!cache->picture) {
		if (!uxa_realize_glyph_caches(pScreen, cache->format))
			return UXA_GLYPH_FAIL;
	}

	DBG_GLYPH_CACHE(("(%d,%d,%s): buffering glyph %lx\n",
			 cache->glyphWidth, cache->glyphHeight,
			 cache->format == PICT_a8 ? "A" : "ARGB",
			 (long)*(CARD32 *) pGlyph->sha1));

	if (cache->glyphCount < GLYPH_CACHE_SIZE) {
		/* Space remaining; we fill from the start */
		pos = cache->glyphCount++;
		DBG_GLYPH_CACHE(("  storing glyph in free space at %d\n", pos));
	} else {
		GlyphPtr evicted;

		/* Need to evict an entry. We have to see if any glyphs
		 * already in the output buffer were at this position in
		 * the cache
		 */

		pos = cache->evictionPosition;
		DBG_GLYPH_CACHE(("  evicting glyph at %d\n", pos));
		if (buffer->count) {
			int x, y;
			int i;

			x = CACHE_X(pos);
			y = CACHE_Y(pos);

			for (i = 0; i < buffer->count; i++) {
				if (buffer->rects[i].xSrc == x
				    && buffer->rects[i].ySrc == y) {
					DBG_GLYPH_CACHE(("  must flush buffer\n"));
					return UXA_GLYPH_NEED_FLUSH;
				}
			}
		}

		evicted = cache->glyphs[pos];
		if (evicted != NULL) {
			priv = uxa_glyph_get_private(evicted);
			uxa_glyph_set_private(evicted, NULL);
		}

		/* And pick a new eviction position */
		cache->evictionPosition = rand() % GLYPH_CACHE_SIZE;
	}

	/* Now actually upload the glyph into the cache picture; if
	 * we can't do it with UploadToScreen (because the glyph is
	 * offscreen, etc), we fall back to CompositePicture.
	 */
	if (!uxa_glyph_cache_upload_glyph(pScreen, cache, pos, pGlyph)) {
		CompositePicture(PictOpSrc,
				 GlyphPicture(pGlyph)[pScreen->myNum],
				 None,
				 cache->picture,
				 0, 0,
				 0, 0,
				 CACHE_X(pos),
				 CACHE_Y(pos),
				 pGlyph->info.width,
				 pGlyph->info.height);
	}

	if (priv == NULL) {
		priv = xalloc(sizeof(struct uxa_glyph));
		if (priv == NULL) {
			cache->glyphs[pos] = NULL;
			return UXA_GLYPH_FAIL;
		}
	}

	priv->cache = cache;
	priv->pos = pos;
	uxa_glyph_set_private(pGlyph, priv);
	cache->glyphs[pos] = pGlyph;

	buffer->source = cache->picture;

	rect = &buffer->rects[buffer->count++];
	rect->xSrc = CACHE_X(pos);
	rect->ySrc = CACHE_Y(pos);
	rect->xDst = xGlyph - pGlyph->info.x;
	rect->yDst = yGlyph - pGlyph->info.y;
	rect->width = pGlyph->info.width;
	rect->height = pGlyph->info.height;
	return UXA_GLYPH_SUCCESS;
}

static uxa_glyph_cache_result_t
uxa_buffer_glyph(ScreenPtr pScreen,
		 uxa_glyph_buffer_t * buffer,
		 GlyphPtr pGlyph, int xGlyph, int yGlyph)
{
	uxa_screen_t *uxa_screen = uxa_get_screen(pScreen);
	unsigned int format = (GlyphPicture(pGlyph)[pScreen->myNum])->format;
	int width = pGlyph->info.width;
	int height = pGlyph->info.height;
	uxa_composite_rect_t *rect;
	struct uxa_glyph *priv;
	PicturePtr source;
	int i;

	if (buffer->count == GLYPH_BUFFER_SIZE)
		return UXA_GLYPH_NEED_FLUSH;

	priv = uxa_glyph_get_private(pGlyph);
	if (priv != NULL) {
		uxa_glyph_cache_t *cache = priv->cache;
		int pos = priv->pos;

		if (buffer->source && buffer->source != cache->picture)
			return UXA_GLYPH_NEED_FLUSH;

		buffer->source = cache->picture;

		rect = &buffer->rects[buffer->count++];
		rect->xSrc = CACHE_X(pos);
		rect->ySrc = CACHE_Y(pos);
		rect->xDst = xGlyph - pGlyph->info.x;
		rect->yDst = yGlyph - pGlyph->info.y;
		rect->width = pGlyph->info.width;
		rect->height = pGlyph->info.height;
		return UXA_GLYPH_SUCCESS;
	}

	if (PICT_FORMAT_BPP(format) == 1)
		format = PICT_a8;

	for (i = 0; i < UXA_NUM_GLYPH_CACHES; i++) {
		uxa_glyph_cache_t *cache = &uxa_screen->glyphCaches[i];

		if (format == cache->format &&
		    width  <= cache->glyphWidth &&
		    height <= cache->glyphHeight) {
			uxa_glyph_cache_result_t result =
				uxa_glyph_cache_buffer_glyph(pScreen,
							     &uxa_screen->
							     glyphCaches[i],
							     buffer,
							     pGlyph, xGlyph,
							     yGlyph);
			switch (result) {
			case UXA_GLYPH_FAIL:
				break;
			case UXA_GLYPH_SUCCESS:
			case UXA_GLYPH_NEED_FLUSH:
				return result;
			}
		}
	}

	/* Couldn't find the glyph in the cache, use the glyph picture directly */

	source = GlyphPicture(pGlyph)[pScreen->myNum];
	if (buffer->source && buffer->source != source)
		return UXA_GLYPH_NEED_FLUSH;

	buffer->source = source;

	rect = &buffer->rects[buffer->count++];
	rect->xSrc = 0;
	rect->ySrc = 0;
	rect->xDst = xGlyph - pGlyph->info.x;
	rect->yDst = yGlyph - pGlyph->info.y;
	rect->width = pGlyph->info.width;
	rect->height = pGlyph->info.height;
	return UXA_GLYPH_SUCCESS;
}

#undef CACHE_X
#undef CACHE_Y

static PicturePtr
uxa_glyphs_acquire_source(ScreenPtr screen,
			  PicturePtr src,
			  INT16 x, INT16 y,
			  const uxa_glyph_buffer_t * buffer,
			  INT16 * out_x, INT16 * out_y)
{
	uxa_screen_t *uxa_screen = uxa_get_screen(screen);
	int x1, y1, x2, y2;
	int width, height;
	int i;

	if (uxa_screen->info->check_composite_texture &&
	    uxa_screen->info->check_composite_texture(screen, src)) {
		if (src->pDrawable) {
			*out_x = x + src->pDrawable->x;
			*out_y = y + src->pDrawable->y;
		} else {
			*out_x = x;
			*out_y = y;
		}
		return src;
	}

	for (i = 0; i < buffer->count; i++) {
	    const uxa_composite_rect_t *r = &buffer->rects[i];

	    if (r->xDst < x1)
		x1 = r->xDst;
	    if (r->xDst + r->width > x2)
		x2 = r->xDst + r->width;

	    if (r->yDst < y1)
		y1 = r->yDst;
	    if (r->yDst + r->height > y2)
		y2 = r->yDst + r->height;
	}

	width  = x2 - x1;
	height = y2 - y1;

	if (src->pDrawable) {
		PicturePtr dst;

		dst = uxa_acquire_drawable(screen, src,
					   x, y,
					   width, height,
					   out_x, out_y);
		if (uxa_screen->info->check_composite_texture &&
		    !uxa_screen->info->check_composite_texture(screen, dst)) {
			if (dst != src)
				FreePicture(dst, 0);
			return 0;
		}

		return dst;
	}

	*out_x = 0;
	*out_y = 0;
	return uxa_acquire_pattern(screen, src,
				   PICT_a8r8g8b8, x, y, width, height);
}

static int
uxa_glyphs_try_driver_composite(CARD8 op,
				PicturePtr pSrc,
				PicturePtr pDst,
				const uxa_glyph_buffer_t * buffer,
				INT16 xSrc, INT16 ySrc,
				INT16 xDst, INT16 yDst)
{
	ScreenPtr screen = pDst->pDrawable->pScreen;
	uxa_screen_t *uxa_screen = uxa_get_screen(screen);
	PicturePtr localSrc;
	int src_off_x = 0, src_off_y = 0, mask_off_x, mask_off_y, dst_off_x, dst_off_y;
	PixmapPtr pSrcPix = NULL, pMaskPix, pDstPix;
	const uxa_composite_rect_t *rects;
	int nrect;

	if (uxa_screen->info->check_composite &&
	    !(*uxa_screen->info->check_composite) (op, pSrc, buffer->source, pDst, 0, 0)) {
		return -1;
	}

	pDstPix =
	    uxa_get_offscreen_pixmap(pDst->pDrawable, &dst_off_x, &dst_off_y);

	pMaskPix =
	    uxa_get_offscreen_pixmap(buffer->source->pDrawable, &mask_off_x, &mask_off_y);
	if(!pMaskPix)
		return -1;

	localSrc = uxa_glyphs_acquire_source(screen, pSrc,
					     xSrc, ySrc,
					     buffer,
					     &xSrc, &ySrc);
	if (!localSrc)
		return 0;

	if (localSrc->pDrawable) {
		pSrcPix =
			uxa_get_offscreen_pixmap(localSrc->pDrawable, &src_off_x, &src_off_y);
		if (!pSrcPix) {
			if (localSrc != pSrc)
				FreePicture(localSrc, 0);
			return 0;
		}

		xSrc += localSrc->pDrawable->x;
		ySrc += localSrc->pDrawable->y;
	}

	if (!(*uxa_screen->info->prepare_composite)
	    (op, localSrc, buffer->source, pDst, pSrcPix, pMaskPix, pDstPix)) {
		if (localSrc != pSrc)
			FreePicture(localSrc, 0);
		return -1;
	}

	nrect = buffer->count;
	rects = buffer->rects;
	do {
		INT16 _xDst = rects->xDst + pDst->pDrawable->x;
		INT16 _yDst = rects->yDst + pDst->pDrawable->y;
		INT16 _xMask = rects->xSrc + buffer->source->pDrawable->x;
		INT16 _yMask = rects->ySrc + buffer->source->pDrawable->y;
		INT16 _xSrc = xSrc, _ySrc = ySrc;

		RegionRec region;
		BoxPtr pbox;
		int nbox;

		if (!miComputeCompositeRegion(&region,
					      localSrc, buffer->source, pDst,
					      _xSrc, _ySrc,
					      _xMask, _yMask,
					      _xDst, _yDst,
					      rects->width, rects->height))
			goto next_rect;

		_xSrc += src_off_x - _xDst;
		_ySrc += src_off_y - _yDst;
		_xMask += mask_off_x - _xDst;
		_yMask += mask_off_y - _yDst;

		nbox = REGION_NUM_RECTS(&region);
		pbox = REGION_RECTS(&region);
		while (nbox--) {
			(*uxa_screen->info->composite) (pDstPix,
							pbox->x1 + _xSrc,
							pbox->y1 + _ySrc,
							pbox->x1 + _xMask,
							pbox->y1 + _yMask,
							pbox->x1 + dst_off_x,
							pbox->y1 + dst_off_y,
							pbox->x2 - pbox->x1,
							pbox->y2 - pbox->y1);
			pbox++;
		}

next_rect:
		REGION_UNINIT(screen, &region);

		rects++;
	} while (--nrect);
	(*uxa_screen->info->done_composite) (pDstPix);

	if (localSrc != pSrc)
		FreePicture(localSrc, 0);

	return 1;
}

static void
uxa_glyphs_to_dst(CARD8 op,
		  PicturePtr pSrc,
		  PicturePtr pDst,
		  const uxa_glyph_buffer_t * buffer,
		  INT16 xSrc, INT16 ySrc,
		  INT16 xDst, INT16 yDst)
{
	if (uxa_glyphs_try_driver_composite(op, pSrc, pDst, buffer,
					    xSrc, ySrc,
					    xDst, yDst) != 1) {
		int i;

		for (i = 0; i < buffer->count; i++) {
			const uxa_composite_rect_t *rect = &buffer->rects[i];

			CompositePicture(op,
					 pSrc, buffer->source, pDst,
					 xSrc + rect->xDst - xDst,
					 ySrc + rect->yDst - yDst,
					 rect->xSrc, rect->ySrc,
					 rect->xDst, rect->yDst,
					 rect->width, rect->height);
		}
	}
}

static int
uxa_glyphs_try_driver_add_to_mask(PicturePtr pDst,
				  const uxa_glyph_buffer_t *buffer)
{
	uxa_screen_t *uxa_screen = uxa_get_screen(pDst->pDrawable->pScreen);
	int src_off_x, src_off_y, dst_off_x, dst_off_y;
	PixmapPtr pSrcPix, pDstPix;
	const uxa_composite_rect_t *rects;
	int nrect;

	if (uxa_screen->info->check_composite &&
	    !(*uxa_screen->info->check_composite) (PictOpAdd, buffer->source, NULL, pDst, 0, 0)) {
		return -1;
	}

	pDstPix =
	    uxa_get_offscreen_pixmap(pDst->pDrawable, &dst_off_x, &dst_off_y);

	pSrcPix =
	    uxa_get_offscreen_pixmap(buffer->source->pDrawable, &src_off_x, &src_off_y);
	if(!pSrcPix)
		return -1;

	if (!(*uxa_screen->info->prepare_composite)
	    (PictOpAdd, buffer->source, NULL, pDst, pSrcPix, NULL, pDstPix))
		return -1;

	rects = buffer->rects;
	nrect = buffer->count;
	do {
		INT16 xDst = rects->xDst + pDst->pDrawable->x;
		INT16 yDst = rects->yDst + pDst->pDrawable->y;
		INT16 xSrc = rects->xSrc + buffer->source->pDrawable->x;
		INT16 ySrc = rects->ySrc + buffer->source->pDrawable->y;

		RegionRec region;
		BoxPtr pbox;
		int nbox;

		if (!miComputeCompositeRegion(&region, buffer->source, NULL, pDst,
					      xSrc, ySrc, 0, 0, xDst, yDst,
					      rects->width, rects->height))
			goto next_rect;

		xSrc += src_off_x - xDst;
		ySrc += src_off_y - yDst;

		nbox = REGION_NUM_RECTS(&region);
		pbox = REGION_RECTS(&region);

		while (nbox--) {
			(*uxa_screen->info->composite) (pDstPix,
							pbox->x1 + xSrc,
							pbox->y1 + ySrc,
							0, 0,
							pbox->x1 + dst_off_x,
							pbox->y1 + dst_off_y,
							pbox->x2 - pbox->x1,
							pbox->y2 - pbox->y1);
			pbox++;
		}

next_rect:
		REGION_UNINIT(pDst->pDrawable->pScreen, &region);

		rects++;
	} while (--nrect);
	(*uxa_screen->info->done_composite) (pDstPix);

	return 1;
}

static void uxa_glyphs_to_mask(PicturePtr pDst, const uxa_glyph_buffer_t *buffer)
{
	if (uxa_glyphs_try_driver_add_to_mask(pDst, buffer) != 1) {
		int i;

		for (i = 0; i < buffer->count; i++) {
			const uxa_composite_rect_t *r = &buffer->rects[i];

			uxa_check_composite(PictOpAdd, buffer->source, NULL, pDst,
					    r->xSrc, r->ySrc,
					    0, 0,
					    r->xDst, r->yDst,
					    r->width, r->height);
		}
	}
}

/* Cut and paste from render/glyph.c - probably should export it instead */
static void
uxa_glyph_extents(int nlist,
		  GlyphListPtr list, GlyphPtr * glyphs, BoxPtr extents)
{
	int x1, x2, y1, y2;
	int n;
	GlyphPtr glyph;
	int x, y;

	x = 0;
	y = 0;
	extents->x1 = MAXSHORT;
	extents->x2 = MINSHORT;
	extents->y1 = MAXSHORT;
	extents->y2 = MINSHORT;
	while (nlist--) {
		x += list->xOff;
		y += list->yOff;
		n = list->len;
		list++;
		while (n--) {
			glyph = *glyphs++;
			x1 = x - glyph->info.x;
			if (x1 < MINSHORT)
				x1 = MINSHORT;
			y1 = y - glyph->info.y;
			if (y1 < MINSHORT)
				y1 = MINSHORT;
			x2 = x1 + glyph->info.width;
			if (x2 > MAXSHORT)
				x2 = MAXSHORT;
			y2 = y1 + glyph->info.height;
			if (y2 > MAXSHORT)
				y2 = MAXSHORT;
			if (x1 < extents->x1)
				extents->x1 = x1;
			if (x2 > extents->x2)
				extents->x2 = x2;
			if (y1 < extents->y1)
				extents->y1 = y1;
			if (y2 > extents->y2)
				extents->y2 = y2;
			x += glyph->info.xOff;
			y += glyph->info.yOff;
		}
	}
}

/**
 * Returns TRUE if the glyphs in the lists intersect.  Only checks based on
 * bounding box, which appears to be good enough to catch most cases at least.
 */
static Bool
uxa_glyphs_intersect(int nlist, GlyphListPtr list, GlyphPtr * glyphs)
{
	int x1, x2, y1, y2;
	int n;
	GlyphPtr glyph;
	int x, y;
	BoxRec extents;
	Bool first = TRUE;

	x = 0;
	y = 0;
	extents.x1 = 0;
	extents.y1 = 0;
	extents.x2 = 0;
	extents.y2 = 0;
	while (nlist--) {
		x += list->xOff;
		y += list->yOff;
		n = list->len;
		list++;
		while (n--) {
			glyph = *glyphs++;

			if (glyph->info.width == 0 || glyph->info.height == 0) {
				x += glyph->info.xOff;
				y += glyph->info.yOff;
				continue;
			}

			x1 = x - glyph->info.x;
			if (x1 < MINSHORT)
				x1 = MINSHORT;
			y1 = y - glyph->info.y;
			if (y1 < MINSHORT)
				y1 = MINSHORT;
			x2 = x1 + glyph->info.width;
			if (x2 > MAXSHORT)
				x2 = MAXSHORT;
			y2 = y1 + glyph->info.height;
			if (y2 > MAXSHORT)
				y2 = MAXSHORT;

			if (first) {
				extents.x1 = x1;
				extents.y1 = y1;
				extents.x2 = x2;
				extents.y2 = y2;
				first = FALSE;
			} else {
				if (x1 < extents.x2 && x2 > extents.x1 &&
				    y1 < extents.y2 && y2 > extents.y1) {
					return TRUE;
				}

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

	return FALSE;
}

static void
uxa_check_glyphs(CARD8 op,
		 PicturePtr src,
		 PicturePtr dst,
		 PictFormatPtr maskFormat,
		 INT16 xSrc,
		 INT16 ySrc, int nlist, GlyphListPtr list, GlyphPtr * glyphs)
{
	int screen = dst->pDrawable->pScreen->myNum;
	pixman_image_t *image;
	PixmapPtr scratch;
	PicturePtr mask;
	int width = 0, height = 0;
	int x, y, n;
	int xDst = list->xOff, yDst = list->yOff;
	BoxRec extents = { 0, 0, 0, 0 };

	if (maskFormat) {
		pixman_format_code_t format;
		CARD32 component_alpha;
		int error;

		uxa_glyph_extents(nlist, list, glyphs, &extents);
		if (extents.x2 <= extents.x1 || extents.y2 <= extents.y1)
			return;

		width = extents.x2 - extents.x1;
		height = extents.y2 - extents.y1;

		format = maskFormat->format |
			(BitsPerPixel(maskFormat->depth) << 24);
		image =
			pixman_image_create_bits(format, width, height, NULL, 0);
		if (!image)
			return;

		scratch = GetScratchPixmapHeader(dst->pDrawable->pScreen, width, height,
						 PIXMAN_FORMAT_DEPTH(format),
						 PIXMAN_FORMAT_BPP(format),
						 pixman_image_get_stride(image),
						 pixman_image_get_data(image));

		if (!scratch) {
			pixman_image_unref(image);
			return;
		}

		component_alpha = NeedsComponent(maskFormat->format);
		mask = CreatePicture(0, &scratch->drawable,
				     maskFormat, CPComponentAlpha,
				     &component_alpha, serverClient, &error);
		if (!mask) {
			FreeScratchPixmapHeader(scratch);
			pixman_image_unref(image);
			return;
		}

		x = -extents.x1;
		y = -extents.y1;
	} else {
		mask = dst;
		x = 0;
		y = 0;
	}

	while (nlist--) {
		x += list->xOff;
		y += list->yOff;
		n = list->len;
		while (n--) {
			GlyphPtr glyph = *glyphs++;
			PicturePtr g = GlyphPicture(glyph)[screen];
			if (g) {
				if (maskFormat) {
					CompositePicture(PictOpAdd, g, NULL, mask,
							 0, 0,
							 0, 0,
							 x - glyph->info.x,
							 y - glyph->info.y,
							 glyph->info.width,
							 glyph->info.height);
				} else {
					CompositePicture(op, src, g, dst,
							 xSrc + (x - glyph->info.x) - xDst,
							 ySrc + (y - glyph->info.y) - yDst,
							 0, 0,
							 x - glyph->info.x,
							 y - glyph->info.y,
							 glyph->info.width,
							 glyph->info.height);
				}
			}

			x += glyph->info.xOff;
			y += glyph->info.yOff;
		}
		list++;
	}

	if (maskFormat) {
		x = extents.x1;
		y = extents.y1;
		CompositePicture(op, src, mask, dst,
				 xSrc + x - xDst,
				 ySrc + y - yDst,
				 0, 0,
				 x, y,
				 width, height);
		FreePicture(mask, 0);
		FreeScratchPixmapHeader(scratch);
		pixman_image_unref(image);
	}
}

void
uxa_glyphs(CARD8 op,
	   PicturePtr pSrc,
	   PicturePtr pDst,
	   PictFormatPtr maskFormat,
	   INT16 xSrc, INT16 ySrc,
	   int nlist, GlyphListPtr list, GlyphPtr * glyphs)
{
	ScreenPtr screen = pDst->pDrawable->pScreen;
	uxa_screen_t *uxa_screen = uxa_get_screen(screen);
	PicturePtr pMask = NULL;
	int width = 0, height = 0;
	int x, y;
	int xDst = list->xOff, yDst = list->yOff;
	int n;
	GlyphPtr glyph;
	int error;
	BoxRec extents = { 0, 0, 0, 0 };
	Bool have_extents = FALSE;
	CARD32 component_alpha;
	uxa_glyph_buffer_t buffer;
	PicturePtr localDst = pDst;

	if (!uxa_screen->info->prepare_composite ||
	    uxa_screen->swappedOut ||
	    !uxa_drawable_is_offscreen(pDst->pDrawable) ||
	    pDst->alphaMap || pSrc->alphaMap) {
fallback:
	    uxa_check_glyphs(op, pSrc, pDst, maskFormat, xSrc, ySrc, nlist, list, glyphs);
	    return;
	}

	ValidatePicture(pSrc);
	ValidatePicture(pDst);

	if (!maskFormat) {
		/* If we don't have a mask format but all the glyphs have the same format,
		 * require ComponentAlpha and don't intersect, use the glyph format as mask
		 * format for the full benefits of the glyph cache.
		 */
		if (NeedsComponent(list[0].format->format)) {
			Bool sameFormat = TRUE;
			int i;

			maskFormat = list[0].format;

			for (i = 0; i < nlist; i++) {
				if (maskFormat->format != list[i].format->format) {
					sameFormat = FALSE;
					break;
				}
			}

			if (!sameFormat ||
			    uxa_glyphs_intersect(nlist, list, glyphs))
				maskFormat = NULL;
		}
	}

	x = y = 0;
	if (!maskFormat &&
	    uxa_screen->info->check_composite_target &&
	    !uxa_screen->info->check_composite_target(uxa_get_drawable_pixmap(pDst->pDrawable))) {
		int depth = pDst->pDrawable->depth;
		PixmapPtr pixmap;
		int error;
		GCPtr gc;

		pixmap = uxa_get_drawable_pixmap(pDst->pDrawable);
		if (uxa_screen->info->check_copy &&
		    !uxa_screen->info->check_copy(pixmap, pixmap, GXcopy, FB_ALLONES))
			goto fallback;

		uxa_glyph_extents(nlist, list, glyphs, &extents);

		/* clip against dst bounds */
		if (extents.x1 < 0)
			extents.x1 = 0;
		if (extents.y1 < 0)
			extents.y1 = 0;
		if (extents.x2 > pDst->pDrawable->width)
			extents.x2 = pDst->pDrawable->width;
		if (extents.y2 > pDst->pDrawable->height)
			extents.y2 = pDst->pDrawable->height;

		if (extents.x2 <= extents.x1 || extents.y2 <= extents.y1)
			return;
		width  = extents.x2 - extents.x1;
		height = extents.y2 - extents.y1;
		x = -extents.x1;
		y = -extents.y1;
		have_extents = TRUE;

		xDst += x;
		yDst += y;

		pixmap = screen->CreatePixmap(screen,
					      width, height, depth,
					      CREATE_PIXMAP_USAGE_SCRATCH);
		if (!pixmap)
			return;

		gc = GetScratchGC(depth, screen);
		if (!gc) {
			screen->DestroyPixmap(pixmap);
			return;
		}

		ValidateGC(&pixmap->drawable, gc);
		gc->ops->CopyArea(pDst->pDrawable, &pixmap->drawable, gc,
				  extents.x1, extents.y1,
				  width, height,
				  0, 0);
		FreeScratchGC(gc);

		localDst = CreatePicture(0, &pixmap->drawable,
					 PictureMatchFormat(screen, depth, pDst->format),
					 0, 0, serverClient, &error);
		screen->DestroyPixmap(pixmap);

		if (!localDst)
			return;

		ValidatePicture(localDst);
	}

	if (maskFormat) {
		PixmapPtr pixmap;
		GCPtr gc;
		xRectangle rect;

		if (!have_extents) {
			uxa_glyph_extents(nlist, list, glyphs, &extents);

			if (extents.x2 <= extents.x1 || extents.y2 <= extents.y1)
				return;
			width  = extents.x2 - extents.x1;
			height = extents.y2 - extents.y1;
			x = -extents.x1;
			y = -extents.y1;
			have_extents = TRUE;
		}

		if (maskFormat->depth == 1) {
			PictFormatPtr a8Format =
			    PictureMatchFormat(screen, 8, PICT_a8);

			if (a8Format)
				maskFormat = a8Format;
		}

		pixmap = screen->CreatePixmap(screen, width, height,
					      maskFormat->depth,
					      CREATE_PIXMAP_USAGE_SCRATCH);
		if (!pixmap) {
			if (localDst != pDst)
				FreePicture(localDst, 0);
			return;
		}

		gc = GetScratchGC(pixmap->drawable.depth, screen);
		ValidateGC(&pixmap->drawable, gc);
		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;
		gc->ops->PolyFillRect(&pixmap->drawable, gc, 1, &rect);
		FreeScratchGC(gc);

		component_alpha = NeedsComponent(maskFormat->format);
		pMask = CreatePicture(0, &pixmap->drawable,
				      maskFormat, CPComponentAlpha,
				      &component_alpha, serverClient, &error);
		screen->DestroyPixmap(pixmap);

		if (!pMask) {
			if (localDst != pDst)
				FreePicture(localDst, 0);
			return;
		}

		ValidatePicture(pMask);
	}

	buffer.count = 0;
	buffer.source = NULL;
	while (nlist--) {
		x += list->xOff;
		y += list->yOff;
		n = list->len;
		while (n--) {
			glyph = *glyphs++;

			if (glyph->info.width > 0 && glyph->info.height > 0 &&
			    uxa_buffer_glyph(screen, &buffer, glyph, x,
					     y) == UXA_GLYPH_NEED_FLUSH) {
				if (maskFormat)
					uxa_glyphs_to_mask(pMask, &buffer);
				else
					uxa_glyphs_to_dst(op, pSrc, localDst,
							  &buffer, xSrc, ySrc,
							  xDst, yDst);

				buffer.count = 0;
				buffer.source = NULL;

				uxa_buffer_glyph(screen, &buffer, glyph, x, y);
			}

			x += glyph->info.xOff;
			y += glyph->info.yOff;
		}
		list++;
	}

	if (buffer.count) {
		if (maskFormat)
			uxa_glyphs_to_mask(pMask, &buffer);
		else
			uxa_glyphs_to_dst(op, pSrc, localDst, &buffer,
					  xSrc, ySrc, xDst, yDst);
	}

	if (maskFormat) {
		if (localDst == pDst) {
			x = extents.x1;
			y = extents.y1;
		} else
			x = y = 0;
		CompositePicture(op,
				 pSrc,
				 pMask,
				 localDst,
				 xSrc + x - xDst,
				 ySrc + y - yDst,
				 0, 0,
				 x, y,
				 width, height);
		FreePicture(pMask, 0);
	}

	if (localDst != pDst) {
		GCPtr gc;

		gc = GetScratchGC(pDst->pDrawable->depth, screen);
		if (gc) {
			ValidateGC(pDst->pDrawable, gc);
			gc->ops->CopyArea(localDst->pDrawable, pDst->pDrawable, gc,
					  0, 0,
					  width, height,
					  extents.x1, extents.y1);
			FreeScratchGC(gc);
		}

		FreePicture(localDst, 0);
	}
}
