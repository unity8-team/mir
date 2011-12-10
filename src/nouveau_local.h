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

#include "nouveau_pushbuf.h"

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

#define NOUVEAU_ALIGN(x,bytes) (((x) + ((bytes) - 1)) & ~((bytes) - 1))

#define NVC0_TILE_PITCH(m) (64 << ((m) & 0xf))
#define NVC0_TILE_HEIGHT(m) (8 << ((m) >> 4))

static inline int log2i(int i)
{
	int r = 0;

	if (i & 0xffff0000) {
		i >>= 16;
		r += 16;
	}
	if (i & 0x0000ff00) {
		i >>= 8;
		r += 8;
	}
	if (i & 0x000000f0) {
		i >>= 4;
		r += 4;
	}
	if (i & 0x0000000c) {
		i >>= 2;
		r += 2;
	}
	if (i & 0x00000002) {
		r += 1;
	}
	return r;
}

static inline int round_down_pow2(int x)
{
	return 1 << log2i(x);
}

static inline int round_up_pow2(int x)
{
   int r = round_down_pow2(x);
   if (r < x)
      r <<= 1;
   return r;
}

#define SWAP(x, y) do {			\
		typeof(x) __z = (x);	\
		(x) = (y);		\
		(y) = __z;		\
	} while (0)

static inline void
BEGIN_NV04(struct nouveau_channel *chan, int subc, int mthd, int size)
{
	WAIT_RING(chan, size + 1);
	OUT_RING (chan, 0x00000000 | (size << 18) | (subc << 13) | mthd);
}

static inline void
BEGIN_NI04(struct nouveau_channel *chan, int subc, int mthd, int size)
{
	WAIT_RING(chan, size + 1);
	OUT_RING (chan, 0x40000000 | (size << 18) | (subc << 13) | mthd);
}

static inline void
BEGIN_NVC0(struct nouveau_channel *chan, int subc, int mthd, int size)
{
	WAIT_RING(chan, size + 1);
	OUT_RING (chan, 0x20000000 | (size << 16) | (subc << 13) | (mthd >> 2));
}

static inline void
BEGIN_NIC0(struct nouveau_channel *chan, int subc, int mthd, int size)
{
	WAIT_RING(chan, size + 1);
	OUT_RING (chan, 0x60000000 | (size << 16) | (subc << 13) | (mthd >> 2));
}

static inline void
BEGIN_IMC0(struct nouveau_channel *chan, int subc, int mthd, int data)
{
	WAIT_RING(chan, 1);
	OUT_RING (chan, 0x80000000 | (data << 16) | (subc << 13) | (mthd >> 2));
}

static inline void
BEGIN_1IC0(struct nouveau_channel *chan, int subc, int mthd, int size)
{
	WAIT_RING(chan, size + 1);
	OUT_RING (chan, 0xa0000000 | (size << 16) | (subc << 13) | (mthd >> 2));
}

/* subchannel assignment */
#define SUBC_M2MF(mthd)  0, (mthd)				/* nv04:     */
#define NV03_M2MF(mthd)  SUBC_M2MF(NV03_M2MF_##mthd)
#define NV50_M2MF(mthd)  SUBC_M2MF(NV50_M2MF_##mthd)
#define NVC0_M2MF(mthd)  SUBC_M2MF(NVC0_M2MF_##mthd)
#define SUBC_NVSW(mthd)  1, (mthd)
#define SUBC_SF2D(mthd)  2, (mthd)				/* nv04:nv50 */
#define SUBC_2D(mthd)    2, (mthd)				/* nv50:     */
#define NV04_SF2D(mthd)  SUBC_SF2D(NV04_SURFACE_2D_##mthd)
#define NV10_SF2D(mthd)  SUBC_SF2D(NV10_SURFACE_2D_##mthd)
#define NV50_2D(mthd)    SUBC_2D(NV50_2D_##mthd)
#define NVC0_2D(mthd)    SUBC_2D(NVC0_2D_##mthd)
#define SUBC_RECT(mthd)  3, (mthd)				/* nv04:nv50 */
#define NV04_RECT(mthd)  SUBC_RECT(NV04_GDI_##mthd)
#define SUBC_BLIT(mthd)  4, (mthd)				/* nv04:nv50 */
#define NV01_BLIT(mthd)  SUBC_BLIT(NV01_BLIT_##mthd)
#define NV04_BLIT(mthd)  SUBC_BLIT(NV04_BLIT_##mthd)
#define NV15_BLIT(mthd)  SUBC_BLIT(NV15_BLIT_##mthd)
#define SUBC_IFC(mthd)   5, (mthd)				/* nv04:nv50 */
#define NV01_IFC(mthd)   SUBC_IFC(NV01_IFC_##mthd)
#define NV04_IFC(mthd)   SUBC_IFC(NV04_IFC_##mthd)
#define SUBC_MISC(mthd)  6, (mthd)				/* nv04:nv50 */
#define NV03_SIFM(mthd)  SUBC_MISC(NV03_SIFM_##mthd)
#define NV05_SIFM(mthd)  SUBC_MISC(NV05_SIFM_##mthd)
#define NV01_BETA(mthd)  SUBC_MISC(NV01_BETA_##mthd)
#define NV04_BETA4(mthd) SUBC_MISC(NV04_BETA4_##mthd)
#define NV01_PATT(mthd)  SUBC_MISC(NV01_PATTERN_##mthd)
#define NV04_PATT(mthd)  SUBC_MISC(NV04_PATTERN_##mthd)
#define NV01_ROP(mthd)   SUBC_MISC(NV01_ROP_##mthd)
#define NV01_CLIP(mthd)  SUBC_MISC(NV01_CLIP_##mthd)
#define SUBC_3D(mthd)    7, (mthd)				/* nv10:     */
#define NV10_3D(mthd)    SUBC_3D(NV10_3D_##mthd)
#define NV30_3D(mthd)    SUBC_3D(NV30_3D_##mthd)
#define NV40_3D(mthd)    SUBC_3D(NV40_3D_##mthd)
#define NV50_3D(mthd)    SUBC_3D(NV50_3D_##mthd)
#define NVC0_3D(mthd)    SUBC_3D(NVC0_3D_##mthd)

#define NV01_SUBC(subc, mthd) SUBC_##subc((NV01_SUBCHAN_##mthd))
#define NV11_SUBC(subc, mthd) SUBC_##subc((NV11_SUBCHAN_##mthd))

#define NV04_GRAPH(subc, mthd) SUBC_##subc((NV04_GRAPH_##mthd))
#define NV50_GRAPH(subc, mthd) SUBC_##subc((NV50_GRAPH_##mthd))
#define NVC0_GRAPH(subc, mthd) SUBC_##subc((NVC0_GRAPH_##mthd))

#endif
