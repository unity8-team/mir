/*
 * Copyright 1993-2003 NVIDIA, Corporation
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

#ifndef __NV_LOCAL_H__
#define __NV_LOCAL_H__

/*
 * This file includes any environment or machine specific values to access the
 * HW.  Put all affected includes, typdefs, etc. here so the riva_hw.* files
 * can stay generic in nature.
 */ 
#include "compiler.h"
#include "xf86_OSproc.h"

//#define NOUVEAU_MODESET_TRACE
#ifdef NOUVEAU_MODESET_TRACE
#define DDXMMIOH(f, h, r, v)	ErrorF(f, h, r, v)
#define DDXMMIOW(f, r, w)	(ErrorF(f, r, w), w)
#else
#define DDXMMIOH(f, h, r, v)
#define DDXMMIOW(f, r, w)	w
#endif

/*
 * HW access macros.  These assume memory-mapped I/O, and not normal I/O space.
 */
#define NV_WR08(p,i,d)  MMIO_OUT8((pointer)(p), (i), (d))
#define NV_RD08(p,i)    MMIO_IN8((pointer)(p), (i))
#define NV_WR16(p,i,d)  MMIO_OUT16((pointer)(p), (i), (d))
#define NV_RD16(p,i)    MMIO_IN16((pointer)(p), (i))
#define NV_WR32(p,i,d)  MMIO_OUT32((pointer)(p), (i), (d))
#define NV_RD32(p,i)    MMIO_IN32((pointer)(p), (i))

/* VGA I/O is now always done through MMIO */
#define VGA_WR08(p,i,d) NV_WR08(p, i, DDXMMIOW("VGA_WR08 port 0x%04x val 0x%02x\n", i, d))
#define VGA_RD08(p,i)   DDXMMIOW("VGA_RD08 port 0x%04x val 0x%02x\n", i, NV_RD08(p,i))

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

#endif /* __NV_LOCAL_H__ */
