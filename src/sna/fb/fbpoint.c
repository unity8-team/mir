/*
 * Copyright © 1998 Keith Packard
 * Copyright © 2012 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "fb.h"
#include <micoord.h>

#define DOTS	    fbDots8
#define BITS	    BYTE
#define BITS2	    CARD16
#define BITS4	    CARD32
#include "fbpointbits.h"
#undef BITS
#undef BITS2
#undef BITS4
#undef DOTS

#define DOTS	    fbDots16
#define BITS	    CARD16
#define BITS2	    CARD32
#include "fbpointbits.h"
#undef BITS
#undef BITS2
#undef DOTS

#define DOTS	    fbDots32
#define BITS	    CARD32
#include "fbpointbits.h"
#undef ARC
#undef BITS
#undef DOTS

static void
fbDots(FbBits *dstOrig, FbStride dstStride, int dstBpp,
       RegionPtr clip,
       xPoint *pts, int n,
       int xorg, int yorg,
       int xoff, int yoff,
       FbBits andOrig, FbBits xorOrig)
{
	FbStip *dst = (FbStip *) dstOrig;
	FbStip and = andOrig;
	FbStip xor = xorOrig;

	while (n--) {
		int x = pts->x + xorg;
		int y = pts->y + yorg;
		pts++;
		if (RegionContainsPoint(clip, x, y, NULL)) {
			FbStip mask;
			FbStip *d;

			x = (x + xoff) * dstBpp;
			d = dst + ((y + yoff) * dstStride) + (x >> FB_STIP_SHIFT);
			x &= FB_STIP_MASK;

			mask = FbStipMask(x, dstBpp);
			WRITE(d, FbDoMaskRRop(READ(d), and, xor, mask));
		}
	}
}

void
fbPolyPoint(DrawablePtr drawable, GCPtr gc,
	    int mode, int n, xPoint *pt)
{
	FbBits *dst;
	FbStride dstStride;
	int dstBpp;
	int dstXoff, dstYoff;
	void (*dots)(FbBits *dst, FbStride dstStride, int dstBpp,
		     RegionPtr clip,
		     xPoint *pts, int n,
		     int xorg, int yorg,
		     int xoff, int yoff,
		     FbBits and, FbBits xor);
	FbBits and, xor;

	DBG(("%s x %d\n", __FUNCTION__, n));

	if (mode == CoordModePrevious)
		fbFixCoordModePrevious(n, pt);

	fbGetDrawable(drawable, dst, dstStride, dstBpp, dstXoff, dstYoff);
	and = fb_gc(gc)->and;
	xor = fb_gc(gc)->xor;
	dots = fbDots;
	switch (dstBpp) {
	case 8:
		dots = fbDots8;
		break;
	case 16:
		dots = fbDots16;
		break;
	case 32:
		dots = fbDots32;
		break;
	}
	dots(dst, dstStride, dstBpp, gc->pCompositeClip, pt, n,
	     drawable->x, drawable->y, dstXoff, dstYoff, and, xor);
}
