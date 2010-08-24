/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include "xf86.h"
#include "xaarop.h"
#include "mipict.h"

#include "ums.h"
#include "ums_i810_reg.h"

#ifndef DO_SCANLINE_IMAGE_WRITE
#define DO_SCANLINE_IMAGE_WRITE 0
#endif

static unsigned int
I830CheckTiling(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (pI830->bufferOffset == pI830->front_buffer->offset &&
       pI830->front_buffer->tiling != TILE_NONE)
   {
       return TRUE;
   }
#ifdef BUILD_DRI
   if (pI830->back_buffer != NULL &&
       pI830->bufferOffset == pI830->back_buffer->offset &&
       pI830->back_buffer->tiling != TILE_NONE)
   {
       return TRUE;
   }
   if (pI830->depth_buffer != NULL &&
       pI830->bufferOffset == pI830->depth_buffer->offset &&
       pI830->depth_buffer->tiling != TILE_NONE)
   {
       return TRUE;
   }
#endif

   return FALSE;
}

void
ums_I830SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
			     unsigned int planemask)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (UMS_DEBUG & DEBUG_VERBOSE_ACCEL)
	ErrorF("I830SetupForFillRectSolid color: %x rop: %x mask: %x\n",
	       color, rop, planemask);

    if (IS_I965G(pI830) && I830CheckTiling(pScrn)) {
	pI830->BR[13] = (pScrn->displayWidth * pI830->cpp) >> 2;
    } else {
	pI830->BR[13] = (pScrn->displayWidth * pI830->cpp);
    }

    /* This function gets used by I830DRIInitBuffers(), and we might not have
     * XAAGetPatternROP() available.  So just use the ROPs from our EXA code
     * if available.
     */
    pI830->BR[13] |= (I830PatternROP[rop] << 16);

    pI830->BR[16] = color;

    switch (pScrn->bitsPerPixel) {
    case 8:
	break;
    case 16:
	pI830->BR[13] |= (1 << 24);
	break;
    case 32:
	pI830->BR[13] |= ((1 << 25) | (1 << 24));
	break;
    }
}

void
ums_I830SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y, int w, int h)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (UMS_DEBUG & DEBUG_VERBOSE_ACCEL)
	ErrorF("I830SubsequentFillRectSolid %d,%d %dx%d\n", x, y, w, h);

    {
	BEGIN_BATCH(6);

	if (pScrn->bitsPerPixel == 32) {
	    OUT_BATCH(COLOR_BLT_CMD | COLOR_BLT_WRITE_ALPHA |
		      COLOR_BLT_WRITE_RGB);
	} else {
	    OUT_BATCH(COLOR_BLT_CMD);
	}
	OUT_BATCH(pI830->BR[13]);
	OUT_BATCH((h << 16) | (w * pI830->cpp));
	OUT_BATCH(pI830->bufferOffset + (y * pScrn->displayWidth + x) *
		  pI830->cpp);
	OUT_BATCH(pI830->BR[16]);
	OUT_BATCH(0);

	ADVANCE_BATCH();
    }

    if (IS_I965G(pI830))
      ums_I830EmitFlush(pScrn);
}

void
ums_I830SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir, int ydir, int rop,
			       unsigned int planemask, int transparency_color)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (UMS_DEBUG & DEBUG_VERBOSE_ACCEL)
	ErrorF("I830SetupForScreenToScreenCopy %d %d %x %x %d\n",
	       xdir, ydir, rop, planemask, transparency_color);

    if (IS_I965G(pI830) && I830CheckTiling(pScrn)) {
	pI830->BR[13] = (pScrn->displayWidth * pI830->cpp) >> 2;
    } else {
	pI830->BR[13] = (pScrn->displayWidth * pI830->cpp);
    }

    /* This function gets used by I830DRIInitBuffers(), and we might not have
     * XAAGetCopyROP() available.  So just use the ROPs from our EXA code
     * if available.
     */
    pI830->BR[13] |= I830CopyROP[rop] << 16;

    switch (pScrn->bitsPerPixel) {
    case 8:
	break;
    case 16:
	pI830->BR[13] |= (1 << 24);
	break;
    case 32:
	pI830->BR[13] |= ((1 << 25) | (1 << 24));
	break;
    }

}

void
ums_I830SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int src_x1, int src_y1,
				 int dst_x1, int dst_y1, int w, int h)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int dst_x2, dst_y2;
    unsigned int tiled = I830CheckTiling(pScrn);

    if (UMS_DEBUG & DEBUG_VERBOSE_ACCEL)
	ErrorF("I830SubsequentScreenToScreenCopy %d,%d - %d,%d %dx%d\n",
	       src_x1, src_y1, dst_x1, dst_y1, w, h);

    dst_x2 = dst_x1 + w;
    dst_y2 = dst_y1 + h;

    {
	BEGIN_BATCH(8);

	if (pScrn->bitsPerPixel == 32) {
	    OUT_BATCH(XY_SRC_COPY_BLT_CMD | XY_SRC_COPY_BLT_WRITE_ALPHA |
		      XY_SRC_COPY_BLT_WRITE_RGB | tiled << 15 | tiled << 11);
	} else {
	    OUT_BATCH(XY_SRC_COPY_BLT_CMD | tiled << 15 | tiled << 11);
	}
	OUT_BATCH(pI830->BR[13]);
	OUT_BATCH((dst_y1 << 16) | (dst_x1 & 0xffff));
	OUT_BATCH((dst_y2 << 16) | (dst_x2 & 0xffff));
	OUT_BATCH(pI830->bufferOffset);
	OUT_BATCH((src_y1 << 16) | (src_x1 & 0xffff));
	OUT_BATCH(pI830->BR[13] & 0xFFFF);
	OUT_BATCH(pI830->bufferOffset);

	ADVANCE_BATCH();
    }

    if (IS_I965G(pI830))
      ums_I830EmitFlush(pScrn);
}
