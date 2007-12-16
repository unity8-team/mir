/*
 * Copyright 2007 Nouveau Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __NOUVEAU_LOCAL_H__
#define __NOUVEAU_LOCAL_H__

#include "compiler.h"
#include "xf86_OSproc.h"

#if EXA_VERSION_MINOR >= 4
#define NOUVEAU_EXA_PIXMAPS 0
struct nouveau_pixmap {
	struct nouveau_bo *bo;
	int mapped;
};
#endif

/* Debug output */
#define NOUVEAU_MSG(fmt,args...) ErrorF(fmt, ##args)
#define NOUVEAU_ERR(fmt,args...) \
	ErrorF("%s:%d - "fmt, __func__, __LINE__, ##args)

#define NOUVEAU_TIME_MSEC() GetTimeInMillis()

#define NOUVEAU_ALIGN(x,bytes) (((x) + ((bytes) - 1)) & ~((bytes) - 1))

/* User FIFO control */
//#define NOUVEAU_DMA_TRACE
//#define NOUVEAU_DMA_DEBUG
#define NOUVEAU_DMA_SUBCHAN_LRU
#define NOUVEAU_DMA_BARRIER mem_barrier();
#define NOUVEAU_DMA_TIMEOUT 2000

/* Push buffer access macros */
#define BEGIN_RING(obj,mthd,size) do {                                         \
	BEGIN_RING_CH(pNv->chan, pNv->obj, (mthd), (size));                    \
} while(0)

#define OUT_RING(data) do {                                                    \
	OUT_RING_CH(pNv->chan, (data));                                        \
} while(0)

#define OUT_RINGp(src,size) do {                                               \
	OUT_RINGp_CH(pNv->chan, (src), (size));                                \
} while(0)

#define OUT_RINGf(data) do {                                                   \
	union { float v; uint32_t u; } c;                                      \
	c.v = (data);                                                          \
	OUT_RING(c.u);                                                         \
} while(0)

#define WAIT_RING(size) do {                                                   \
	WAIT_RING_CH(pNv->chan, (size));                                       \
} while(0)

#define FIRE_RING() do {                                                       \
	FIRE_RING_CH(pNv->chan);                                               \
} while(0)

#define OUT_RELOC(bo,data,flags,vor,tor) do {                                  \
	struct nouveau_channel_priv *chan = nouveau_channel(pNv->chan);        \
	nouveau_bo_emit_reloc(&chan->base, &chan->pushbuf[chan->dma.cur],      \
			      (bo), (data), (flags), (vor), (tor));            \
	OUT_RING(0);                                                           \
} while(0)

/* Raw data + flags depending on FB/TT buffer */
#define OUT_RELOCd(bo,data,flags,vor,tor) do {                                 \
	OUT_RELOC((bo), (data), (flags) | NOUVEAU_BO_OR, (vor), (tor));        \
} while(0)

/* FB/TT object handle */
#define OUT_RELOCo(bo,flags) do {                                              \
	OUT_RELOC((bo), 0, (flags) | NOUVEAU_BO_OR,                            \
		  pNv->chan->vram->handle, pNv->chan->gart->handle);           \
} while(0)

/* Low 32-bits of offset */
#define OUT_RELOCl(bo,delta,flags) do {                                        \
	OUT_RELOC((bo), (delta), (flags) | NOUVEAU_BO_LOW, 0, 0);              \
} while(0)

/* High 32-bits of offset */
#define OUT_RELOCh(bo,delta,flags) do {                                        \
	OUT_RELOC((bo), (delta), (flags) | NOUVEAU_BO_HIGH, 0, 0);             \
} while(0)


/* Alternate versions of OUT_RELOCx above, takes pixmaps instead of BOs */
#if NOUVEAU_EXA_PIXMAPS
#define OUT_PIXMAPd(pm,data,flags,vor,tor) do {                                \
	struct nouveau_pixmap *nvpix = exaGetPixmapDriverPrivate((pm));        \
	struct nouveau_bo *pmo = nvpix->bo;                                    \
	OUT_RELOCd(pmo, (data), (flags), (vor), (tor));                        \
} while(0)
#define OUT_PIXMAPo(pm,flags) do {                                             \
	struct nouveau_pixmap *nvpix = exaGetPixmapDriverPrivate((pm));        \
	struct nouveau_bo *pmo = nvpix->bo;                                    \
	OUT_RELOCo(pmo, (flags));                                              \
} while(0)
#define OUT_PIXMAPl(pm,delta,flags) do {                                       \
	struct nouveau_pixmap *nvpix = exaGetPixmapDriverPrivate((pm));        \
	struct nouveau_bo *pmo = nvpix->bo;                                    \
	OUT_RELOCl(pmo, (delta), (flags));                                     \
} while(0)
#define OUT_PIXMAPh(pm,delta,flags) do {                                       \
	struct nouveau_pixmap *nvpix = exaGetPixmapDriverPrivate((pm));        \
	struct nouveau_bo *pmo = nvpix->bo;                                    \
	OUT_RELOCh(pmo, (delta), (flags));                                     \
} while(0)
#else
#define OUT_PIXMAPd(pm,data,flags,vor,tor) do {                                \
	OUT_RELOCd(pNv->FB, (data), (flags), (vor), (tor));                    \
} while(0)
#define OUT_PIXMAPo(pm,flags) do {                                             \
	OUT_RELOCo(pNv->FB, (flags));                                          \
} while(0)
#define OUT_PIXMAPl(pm,delta,flags) do {                                       \
	OUT_RELOCl(pNv->FB, exaGetPixmapOffset(pm) + (delta), (flags));        \
} while(0)
#define OUT_PIXMAPh(pm,delta,flags) do {                                       \
	OUT_RELOCh(pNv->FB, exaGetPixmapOffset(pm) + (delta), (flags));        \
} while(0)
#endif

#endif
