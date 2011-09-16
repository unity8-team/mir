/*
 * Copyright (c) 2011 Intel Corporation
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

#if DEBUG_BLT
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

void
memcpy_blt(const void *src, void *dst, int bpp,
	   uint16_t src_stride, uint16_t dst_stride,
	   int16_t src_x, int16_t src_y,
	   int16_t dst_x, int16_t dst_y,
	   uint16_t width, uint16_t height)
{
	uint8_t *src_bytes;
	uint8_t *dst_bytes;

	assert(width && height);
	assert(bpp >= 8);

	DBG(("%s: src=(%d, %d), dst=(%d, %d), size=%dx%d, pitch=%d/%d\n",
	     __FUNCTION__, src_x, src_y, dst_x, dst_y, width, height, src_stride, dst_stride));

	bpp /= 8;
	width *= bpp;

	src_bytes = (uint8_t *)src + src_stride * src_y + src_x * bpp;
	dst_bytes = (uint8_t *)dst + dst_stride * dst_y + dst_x * bpp;

	if (width == src_stride && width == dst_stride) {
		memcpy(dst_bytes, src_bytes, width * height);
		return;
	}

	do {
		memcpy(dst_bytes, src_bytes, width);
		src_bytes += src_stride;
		dst_bytes += dst_stride;
	} while (--height);
}
