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

#define NOUVEAU_PRIVATE _X_HIDDEN
#define NOUVEAU_PUBLIC _X_EXPORT

/* Debug output */
#define NOUVEAU_MSG(fmt,args...) ErrorF(fmt, ##args)
#define NOUVEAU_ERR(fmt,args...) \
	ErrorF("%s:%d - "fmt, __func__, __LINE__, ##args)
#if 0
#define NOUVEAU_FALLBACK(fmt,args...) do {    \
	NOUVEAU_ERR("FALLBACK: "fmt, ##args); \
	return FALSE;                         \
} while(0)
#else
#define NOUVEAU_FALLBACK(fmt,args...) do {    \
	return FALSE;                         \
} while(0)
#endif

#define NOUVEAU_TIME_MSEC() GetTimeInMillis()

#define NOUVEAU_ALIGN(x,bytes) (((x) + ((bytes) - 1)) & ~((bytes) - 1))

/* User FIFO control */
//#define NOUVEAU_DMA_TRACE
//#define NOUVEAU_DMA_DEBUG
#define NOUVEAU_DMA_SUBCHAN_LRU
#define NOUVEAU_DMA_BARRIER mem_barrier();
#define NOUVEAU_DMA_TIMEOUT 2000

/* Push buffer access macros */
#define BEGIN_RING(chan,obj,mthd,size) do {                                    \
	BEGIN_RING_CH((chan), (obj), (mthd), (size));                          \
} while(0)

#define OUT_RING(chan,data) do {                                               \
	OUT_RING_CH((chan), (data));                                           \
} while(0)

#define OUT_RINGp(chan,src,size) do {                                          \
	OUT_RINGp_CH((chan), (src), (size));                                   \
} while(0)

#define OUT_RINGf(chan,data) do {                                              \
	union { float v; uint32_t u; } c;                                      \
	c.v = (data);                                                          \
	OUT_RING((chan), c.u);                                                 \
} while(0)

#define WAIT_RING(chan,size) do {                                              \
	WAIT_RING_CH((chan), (size));                                          \
} while(0)

#define FIRE_RING(chan) do {                                                   \
	FIRE_RING_CH((chan));                                               \
} while(0)

#define OUT_RELOC(chan,bo,data,flags,vor,tor) do {                             \
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);           \
	nouveau_bo_emit_reloc((chan), &nvchan->pushbuf[nvchan->dma.cur],       \
			      (bo), (data), (flags), (vor), (tor));            \
	OUT_RING((chan), 0);                                                   \
} while(0)

/* Raw data + flags depending on FB/TT buffer */
#define OUT_RELOCd(chan,bo,data,flags,vor,tor) do {                            \
	OUT_RELOC((chan), (bo), (data), (flags) | NOUVEAU_BO_OR, (vor), (tor));\
} while(0)

/* FB/TT object handle */
#define OUT_RELOCo(chan,bo,flags) do {                                         \
	OUT_RELOC((chan), (bo), 0, (flags) | NOUVEAU_BO_OR,                    \
		  (chan)->vram->handle, (chan)->gart->handle);                 \
} while(0)

/* Low 32-bits of offset */
#define OUT_RELOCl(chan,bo,delta,flags) do {                                   \
	OUT_RELOC((chan), (bo), (delta), (flags) | NOUVEAU_BO_LOW, 0, 0);      \
} while(0)

/* High 32-bits of offset */
#define OUT_RELOCh(chan,bo,delta,flags) do {                                   \
	OUT_RELOC((chan), (bo), (delta), (flags) | NOUVEAU_BO_HIGH, 0, 0);     \
} while(0)


/* Alternate versions of OUT_RELOCx above, takes pixmaps instead of BOs */
#define OUT_PIXMAPd(chan,pm,data,flags,vor,tor) do {                           \
	OUT_RELOCd((chan), pNv->FB, (data), (flags), (vor), (tor));            \
} while(0)
#define OUT_PIXMAPo(chan,pm,flags) do {                                        \
	OUT_RELOCo((chan), pNv->FB, (flags));                                  \
} while(0)
#define OUT_PIXMAPl(chan,pm,delta,flags) do {                                  \
	OUT_RELOCl((chan), pNv->FB, exaGetPixmapOffset(pm) + (delta), (flags));\
} while(0)
#define OUT_PIXMAPh(chan,pm,delta,flags) do {                                  \
	OUT_RELOCh((chan), pNv->FB, exaGetPixmapOffset(pm) + (delta), (flags));\
} while(0)

#endif
