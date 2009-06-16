/*
 * Copyright © 2009 Red Hat, Inc.
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
 */

/* Authors:
 *    Ben Skeggs <bskeggs@redhat.com>
 */

#include "nv_include.h"

struct wfb_pixmap {
	PixmapPtr ppix;
	unsigned long base;
	unsigned long end;
	unsigned pitch;
	unsigned tile_height;
	unsigned horiz_tiles;
	uint64_t multiply_factor;
};

static struct wfb_pixmap wfb_pixmap[6];

static inline FbBits
nouveau_wfb_rd_linear(const void *src, int size)
{
	FbBits bits = 0;

	memcpy(&bits, src, size);
	return bits;
}

static inline void
nouveau_wfb_wr_linear(void *dst, FbBits value, int size)
{
	memcpy(dst, &value, size);
}

#define TP 6
#define TH wfb->tile_height
#define TPM ((1 << TP) - 1)
#define THM ((1 << TH) - 1)

static FbBits
nouveau_wfb_rd_tiled(const void *ptr, int size) {
	unsigned long offset = (unsigned long)ptr;
	struct wfb_pixmap *wfb = NULL;
	FbBits bits = 0;
	int x, y, i;

	for (i = 0; i < 6; i++) {
		if (offset >= wfb_pixmap[i].base &&
		    offset < wfb_pixmap[i].end) {
			wfb = &wfb_pixmap[i];
			break;
		}
	}

	if (!wfb || !wfb->pitch)
		return nouveau_wfb_rd_linear(ptr, size);

	offset -= wfb->base;

	y = (offset * wfb->multiply_factor) >> 32;
	x = offset - y * wfb->pitch;

	offset  = (x >> TP) + ((y >> TH) * wfb->horiz_tiles);
	offset *= (1 << (TH + TP));
	offset += ((y & THM) * (1 << TP)) + (x & TPM);

	memcpy(&bits, (void *)wfb->base + offset, size);
	return bits;
}

static void
nouveau_wfb_wr_tiled(void *ptr, FbBits value, int size) {
	unsigned long offset = (unsigned long)ptr;
	struct wfb_pixmap *wfb = NULL;
	int x, y, i;

	for (i = 0; i < 6; i++) {
		if (offset >= wfb_pixmap[i].base &&
		    offset < wfb_pixmap[i].end) {
			wfb = &wfb_pixmap[i];
			break;
		}
	}

	if (!wfb || !wfb->pitch) {
		nouveau_wfb_wr_linear(ptr, value, size);
		return;
	}

	offset -= wfb->base;

	y = (offset * wfb->multiply_factor) >> 32;
	x = offset - y * wfb->pitch;

	offset  = (x >> TP) + ((y >> TH) * wfb->horiz_tiles);
	offset *= (1 << (TH + TP));
	offset += ((y & THM) * (1 << TP)) + (x & TPM);

	memcpy((void *)wfb->base + offset, &value, size);
}

void
nouveau_wfb_setup_wrap(ReadMemoryProcPtr *pRead, WriteMemoryProcPtr *pWrite,
		       DrawablePtr pDraw)
{
	struct nouveau_pixmap *nvpix;
	struct wfb_pixmap *wfb;
	PixmapPtr ppix = NULL;
	int wrap;

	if (!pRead || !pWrite)
		return;

	ppix = NVGetDrawablePixmap(pDraw);
	if (ppix)
		nvpix = nouveau_pixmap(ppix);
	if (!nvpix || !nvpix->bo) {
		*pRead = nouveau_wfb_rd_linear;
		*pWrite = nouveau_wfb_wr_linear;
		return;
	}

	wrap = 0;
	while (wfb_pixmap[wrap].ppix)
		wrap++;
	wfb = &wfb_pixmap[wrap];

	wfb->ppix = ppix;
	wfb->base = (unsigned long)ppix->devPrivate.ptr;
	wfb->end = wfb->base + nvpix->bo->size;
	if (!nvpix->bo->tile_flags) {
		wfb->pitch = 0;
	} else {
		wfb->pitch = ppix->devKind;
		wfb->multiply_factor = (0xFFFFFFFF / wfb->pitch) + 1;
		wfb->tile_height = nvpix->bo->tile_mode + 2;
		wfb->horiz_tiles = wfb->pitch / 64;
	}

	*pRead = nouveau_wfb_rd_tiled;
	*pWrite = nouveau_wfb_wr_tiled;
}

void
nouveau_wfb_finish_wrap(DrawablePtr pDraw)
{
	struct wfb_pixmap *wfb = &wfb_pixmap[0];
	PixmapPtr ppix;
	int i;

	ppix = NVGetDrawablePixmap(pDraw);
	if (!ppix)
		return;

	for (i = 0; i < 6; i++) {
		if (wfb->ppix != ppix)
			continue;

		wfb->ppix = NULL;
		break;
	}
}

