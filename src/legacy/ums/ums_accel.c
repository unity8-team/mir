/*
 * XXX So far, for GXxor this is about 40% of the speed of SW, but CPU
 * utilisation falls from 95% to < 5%.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i830_accel.c,v 1.8 2003/04/24 18:00:24 eich Exp $ */

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

#include <errno.h>

#include "xf86.h"
#include "xaarop.h"
#include "ums.h"
#include "ums_i810_reg.h"
#include "ums_debug.h"
#include "ums_ring.h"

#include <xf86drm.h> /* drmCommand*() */
#include "ums_i830_common.h" /* XXX really i830_drm.h! */
#include <i915_drm.h>

int
ums_I830WaitLpRing(ScrnInfoPtr pScrn, int n, int timeout_millis)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830RingBuffer *ring = &pI830->ring;
   int iters = 0;
   unsigned int start = 0;
   unsigned int now = 0;
   int last_head = 0;
   unsigned int first = 0;

   /* If your system hasn't moved the head pointer in 2 seconds, I'm going to
    * call it crashed.
    */
   if (timeout_millis == 0)
      timeout_millis = 2000;

   if (UMS_DEBUG & DEBUG_VERBOSE_ACCEL) {
      ErrorF("I830WaitLpRing %d\n", n);
      first = GetTimeInMillis();
   }

   while (ring->space < n) {
      ring->head = INREG(LP_RING + RING_HEAD) & I830_HEAD_MASK;
      ring->space = ring->head - (ring->tail + 8);

      if (ring->space < 0)
	 ring->space += ring->mem->size;

      iters++;
      now = GetTimeInMillis();
      if (start == 0 || now < start || ring->head != last_head) {
	 if (UMS_DEBUG & DEBUG_VERBOSE_ACCEL)
	    if (now > start)
	       ErrorF("space: %d wanted %d\n", ring->space, n);
	 start = now;
	 last_head = ring->head;
      } else if (now - start > timeout_millis) {
	 ErrorF("Error in I830WaitLpRing(), timeout for %d seconds\n",
		timeout_millis/1000);
	 if (IS_I965G(pI830))
	     ums_i965_dump_error_state(pScrn);
	 else
	     ums_i830_dump_error_state(pScrn);
	 ErrorF("space: %d wanted %d\n", ring->space, n);
#ifdef BUILD_DRI
	 if (pI830->directRenderingType == DRI_XF86DRI) {
	    DRIUnlock(screenInfo.screens[pScrn->scrnIndex]);
	    DRICloseScreen(screenInfo.screens[pScrn->scrnIndex]);
	 }
#endif
	 pI830->EXADriverPtr = NULL;
	 FatalError("lockup\n");
      }

      DELAY(10);
   }

   if (UMS_DEBUG & DEBUG_VERBOSE_ACCEL) {
      now = GetTimeInMillis();
      if (now - first) {
	 ErrorF("Elapsed %u ms\n", now - first);
	 ErrorF("space: %d wanted %d\n", ring->space, n);
      }
   }

   return iters;
}

void
ums_I830Sync(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (UMS_DEBUG & (DEBUG_VERBOSE_ACCEL | DEBUG_VERBOSE_SYNC))
      ErrorF("I830Sync\n");

   if (!pScrn->vtSema || !pI830->batch_bo)
       return;

#ifdef BUILD_DRI
   /* VT switching tries to do this.
    */
   if (!pI830->LockHeld && pI830->directRenderingType == DRI_XF86DRI) {
      return;
   }
#endif

   ums_I830EmitFlush(pScrn);

   ums_batch_flush(pScrn, TRUE);
   ums_wait_ring_idle(pScrn);

   pI830->nextColorExpandBuf = 0;
}

void
ums_I830EmitFlush(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   int flags = MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE;

   if (IS_I965G(pI830))
      flags = 0;

   {
       BEGIN_BATCH(1);
       OUT_BATCH(MI_FLUSH | flags);
       ADVANCE_BATCH();
   }
}

Bool
ums_I830SelectBuffer(ScrnInfoPtr pScrn, int buffer)
{
   I830Ptr pI830 = I830PTR(pScrn);

   switch (buffer) {
#ifdef BUILD_DRI
   case I830_SELECT_BACK:
      pI830->bufferOffset = pI830->back_buffer->offset;
      if (pI830->back_buffer->tiling == TILE_YMAJOR)
	 return FALSE;
      break;
   case I830_SELECT_DEPTH:
      pI830->bufferOffset = pI830->depth_buffer->offset;
      if (pI830->depth_buffer->tiling == TILE_YMAJOR)
	 return FALSE;
      break;
#endif
   default:
   case I830_SELECT_FRONT:
      pI830->bufferOffset = pScrn->fbOffset;
      break;
   }

   if (UMS_DEBUG & DEBUG_VERBOSE_ACCEL)
      ErrorF("I830SelectBuffer %d --> offset %x\n",
	     buffer, pI830->bufferOffset);
   return TRUE;
}

#if (ALWAYS_SYNC || ALWAYS_FLUSH)
void
ums__debug_sync(ScrnInfoPtr scrn)
{
    if (ALWAYS_SYNC)
	ums_I830Sync(scrn);
    else
	ums_batch_flush(scrn, FALSE);
}
#endif

/* The following function sets up the supported acceleration. Call it
 * from the FbInit() function in the SVGA driver, or before ScreenInit
 * in a monolithic server.
 */
Bool
ums_I830AccelInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    /* Limits are described in the BLT engine chapter under Graphics Data Size
     * Limitations, and the descriptions of SURFACE_STATE, 3DSTATE_BUFFER_INFO,
     * 3DSTATE_DRAWING_RECTANGLE, 3DSTATE_MAP_INFO, and 3DSTATE_MAP_INFO.
     *
     * i845 through i965 limits 2D rendering to 65536 lines and pitch of 32768.
     *
     * i965 limits 3D surface to (2*element size)-aligned offset if un-tiled.
     * i965 limits 3D surface to 4kB-aligned offset if tiled.
     * i965 limits 3D surfaces to w,h of ?,8192.
     * i965 limits 3D surface to pitch of 1B - 128kB.
     * i965 limits 3D surface pitch alignment to 1 or 2 times the element size.
     * i965 limits 3D surface pitch alignment to 512B if tiled.
     * i965 limits 3D destination drawing rect to w,h of 8192,8192.
     *
     * i915 limits 3D textures to 4B-aligned offset if un-tiled.
     * i915 limits 3D textures to ~4kB-aligned offset if tiled.
     * i915 limits 3D textures to width,height of 2048,2048.
     * i915 limits 3D textures to pitch of 16B - 8kB, in dwords.
     * i915 limits 3D destination to ~4kB-aligned offset if tiled.
     * i915 limits 3D destination to pitch of 16B - 8kB, in dwords, if un-tiled.
     * i915 limits 3D destination to pitch 64B-aligned if used with depth.
     * i915 limits 3D destination to pitch of 512B - 8kB, in tiles, if tiled.
     * i915 limits 3D destination to POT aligned pitch if tiled.
     * i915 limits 3D destination drawing rect to w,h of 2048,2048.
     *
     * i845 limits 3D textures to 4B-aligned offset if un-tiled.
     * i845 limits 3D textures to ~4kB-aligned offset if tiled.
     * i845 limits 3D textures to width,height of 2048,2048.
     * i845 limits 3D textures to pitch of 4B - 8kB, in dwords.
     * i845 limits 3D destination to 4B-aligned offset if un-tiled.
     * i845 limits 3D destination to ~4kB-aligned offset if tiled.
     * i845 limits 3D destination to pitch of 8B - 8kB, in dwords.
     * i845 limits 3D destination drawing rect to w,h of 2048,2048.
     *
     * For the tiled issues, the only tiled buffer we draw to should be
     * the front, which will have an appropriate pitch/offset already set up,
     * so EXA doesn't need to worry.
     */
    if (IS_I965G(pI830)) {
	pI830->accel_pixmap_offset_alignment = 4 * 2;
	pI830->accel_pixmap_pitch_alignment = 64;
	pI830->accel_max_x = 8192;
	pI830->accel_max_y = 8192;
    } else {
	pI830->accel_pixmap_offset_alignment = 4;
	pI830->accel_pixmap_pitch_alignment = 64;
	pI830->accel_max_x = 2048;
	pI830->accel_max_y = 2048;
    }

    return ums_I830EXAInit(pScreen);
}
