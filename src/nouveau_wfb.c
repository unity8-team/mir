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
	unsigned long base;
	unsigned long end;
	unsigned pitch;
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

#define TILE_FUNC_FOR_WRAP(name,wrap)                                          \
static FbBits                                                                  \
nouveau_wfb_rd_##name(const void *ptr, int size) {                             \
	unsigned long offset = (unsigned long)ptr;                             \
	struct wfb_pixmap *wfb = &wfb_pixmap[wrap];                            \
	FbBits bits = 0;                                                       \
	int x, y;                                                              \
                                                                               \
	offset -= wfb->base;                                                   \
                                                                               \
	y = (offset * wfb->multiply_factor) >> 32;                             \
	x = offset - y * wfb->pitch;                                           \
                                                                               \
	offset  = (x >> TP) + ((y >> TH) * wfb->horiz_tiles);                  \
	offset *= (1 << (TH + TP));                                            \
	offset += ((y & THM) * (1 << TP)) + (x & TPM);                         \
                                                                               \
	memcpy(&bits, (void *)wfb->base + offset, size);                       \
	return bits;                                                           \
}                                                                              \
                                                                               \
static void                                                                    \
nouveau_wfb_wr_##name(void *ptr, FbBits value, int size) {                     \
	unsigned long offset = (unsigned long)ptr;                             \
	struct wfb_pixmap *wfb = &wfb_pixmap[wrap];                            \
	int x, y;                                                              \
                                                                               \
	offset -= wfb->base;                                                   \
                                                                               \
	y = (offset * wfb->multiply_factor) >> 32;                             \
	x = offset - y * wfb->pitch;                                           \
                                                                               \
	offset  = (x >> TP) + ((y >> TH) * wfb->horiz_tiles);                  \
	offset *= (1 << (TH + TP));                                            \
	offset += ((y & THM) * (1 << TP)) + (x & TPM);                         \
                                                                               \
	memcpy((void *)wfb->base + offset, &value, size);                      \
}

#define TILE_FUNC(name)                                                        \
	TILE_FUNC_FOR_WRAP(w00_##name, 0);                                     \
	TILE_FUNC_FOR_WRAP(w01_##name, 1);                                     \
	TILE_FUNC_FOR_WRAP(w02_##name, 2);                                     \
	TILE_FUNC_FOR_WRAP(w03_##name, 3);                                     \
	TILE_FUNC_FOR_WRAP(w04_##name, 4);                                     \
	TILE_FUNC_FOR_WRAP(w05_##name, 5);                                     \
	static struct {                                                        \
		ReadMemoryProcPtr read;                                        \
		WriteMemoryProcPtr write;                                      \
	} nouveau_wfb_##name[6] = {                                            \
		{ nouveau_wfb_rd_w00_##name, nouveau_wfb_wr_w00_##name },      \
		{ nouveau_wfb_rd_w01_##name, nouveau_wfb_wr_w01_##name },      \
		{ nouveau_wfb_rd_w02_##name, nouveau_wfb_wr_w02_##name },      \
		{ nouveau_wfb_rd_w03_##name, nouveau_wfb_wr_w03_##name },      \
		{ nouveau_wfb_rd_w04_##name, nouveau_wfb_wr_w04_##name },      \
		{ nouveau_wfb_rd_w05_##name, nouveau_wfb_wr_w05_##name },      \
	}

#define HW_MODE 0
#define TH (HW_MODE + 2)
#define TP 5
#define THM ((1 << TH) - 1)
#define TPM ((1 << TP) - 1)
TILE_FUNC(mode0);
#undef TPM
#undef THM
#undef TP
#undef TH
#undef HW_MODE

#define HW_MODE 1
#define TH (HW_MODE + 2)
#define TP 6
#define THM ((1 << TH) - 1)
#define TPM ((1 << TP) - 1)
TILE_FUNC(mode1);
#undef TPM
#undef THM
#undef TP
#undef TH
#undef HW_MODE

#define HW_MODE 2
#define TH (HW_MODE + 2)
#define TP 6
#define THM ((1 << TH) - 1)
#define TPM ((1 << TP) - 1)
TILE_FUNC(mode2);
#undef TPM
#undef THM
#undef TP
#undef TH
#undef HW_MODE

#define HW_MODE 3
#define TH (HW_MODE + 2)
#define TP 6
#define THM ((1 << TH) - 1)
#define TPM ((1 << TP) - 1)
TILE_FUNC(mode3);
#undef TPM
#undef THM
#undef TP
#undef TH
#undef HW_MODE

#define HW_MODE 4
#define TH (HW_MODE + 2)
#define TP 6
#define THM ((1 << TH) - 1)
#define TPM ((1 << TP) - 1)
TILE_FUNC(mode4);
#undef TPM
#undef THM
#undef TP
#undef TH
#undef HW_MODE

void
nouveau_wfb_setup_wrap(ReadMemoryProcPtr *pRead, WriteMemoryProcPtr *pWrite,
		       DrawablePtr pDraw)
{
	struct nouveau_pixmap *nvpix;
	struct wfb_pixmap *wfb;
	PixmapPtr ppix;
	int wrap;

	if (!pRead || !pWrite)
		return;

	ppix = NVGetDrawablePixmap(pDraw);
	if (!ppix)
		return;
	nvpix = nouveau_pixmap(ppix);

	wrap = 0;
	while (wfb_pixmap[wrap].base)
		wrap++;
	wfb = &wfb_pixmap[wrap];

	wfb->base = (unsigned long)ppix->devPrivate.ptr;
	wfb->end = wfb->base + nvpix->bo->size;
	wfb->pitch = ppix->devKind;
	wfb->multiply_factor = (0xFFFFFFFF / wfb->pitch) + 1;
	wfb->horiz_tiles = wfb->pitch / (nvpix->bo->tile_mode ? 64 : 32);

	if (!nvpix->bo->tile_flags) {
		*pRead = nouveau_wfb_rd_linear;
		*pWrite = nouveau_wfb_wr_linear;
		return;
	}

	switch (nvpix->bo->tile_mode) {
	case 0:
		*pRead = nouveau_wfb_mode0[wrap].read;
		*pWrite = nouveau_wfb_mode0[wrap].write;
		break;
	case 1:
		*pRead = nouveau_wfb_mode1[wrap].read;
		*pWrite = nouveau_wfb_mode1[wrap].write;
		break;
	case 2:
		*pRead = nouveau_wfb_mode2[wrap].read;
		*pWrite = nouveau_wfb_mode2[wrap].write;
		break;
	case 3:
		*pRead = nouveau_wfb_mode3[wrap].read;
		*pWrite = nouveau_wfb_mode3[wrap].write;
		break;
	case 4:
		*pRead = nouveau_wfb_mode4[wrap].read;
		*pWrite = nouveau_wfb_mode4[wrap].write;
		break;
	default:
		FatalError("uh oh\n");
	}
}

void
nouveau_wfb_finish_wrap(DrawablePtr pDraw)
{
	struct wfb_pixmap *wfb = &wfb_pixmap[0];
	PixmapPtr ppix;

	ppix = NVGetDrawablePixmap(pDraw);
	if (!ppix)
		return;

	while (wfb->base != (unsigned long)ppix->devPrivate.ptr)
		wfb++;
	wfb->base = 0;
}

