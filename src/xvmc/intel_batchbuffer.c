/**************************************************************************
 *
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>

#include <sys/ioctl.h>
#include <X11/Xlibint.h>
#include <fourcc.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/XvMC.h>
#include <X11/extensions/XvMClib.h>

#include "I915XvMC.h"
#include "intel_batchbuffer.h"

#define MI_BATCH_BUFFER_END     (0xA << 23)

int intelEmitIrqLocked(i915XvMCContext *pI915XvMC)
{
   drmI830IrqEmit ie;
   int ret, seq;

   ie.irq_seq = &seq;
   ret = drmCommandWriteRead(xvmc_driver->fd, DRM_I830_IRQ_EMIT,
                             &ie, sizeof(ie));

   if ( ret ) {
      fprintf(stderr, "%s: drmI830IrqEmit: %d\n", __FUNCTION__, ret);
      exit(1);
   }

   return seq;
}

void intelWaitIrq(i915XvMCContext *pI915XvMC, int seq)
{
   int ret;
   drmI830IrqWait iw;

   iw.irq_seq = seq;

   do {
      ret = drmCommandWrite(xvmc_driver->fd, DRM_I830_IRQ_WAIT, &iw, sizeof(iw) );
   } while (ret == -EAGAIN || ret == -EINTR);

   if (ret) {
      fprintf(stderr, "%s: drmI830IrqWait: %d\n", __FUNCTION__, ret);
      exit(1);
   }
}

void intelDestroyBatchBuffer(i915XvMCContext *pI915XvMC)
{
   if (pI915XvMC->alloc.offset) {
       pI915XvMC->alloc.ptr = NULL;
       pI915XvMC->alloc.offset = 0;
   } else if (pI915XvMC->alloc.ptr) {
      free(pI915XvMC->alloc.ptr);
      pI915XvMC->alloc.ptr = NULL;
   }

   memset(&pI915XvMC->batch, 0, sizeof(pI915XvMC->batch));
}


void intelInitBatchBuffer(i915XvMCContext  *pI915XvMC)
{
    if (pI915XvMC->batchbuffer.map) {
        pI915XvMC->alloc.size = pI915XvMC->batchbuffer.size;
        pI915XvMC->alloc.offset = pI915XvMC->batchbuffer.offset;
        pI915XvMC->alloc.ptr = pI915XvMC->batchbuffer.map;
    } else {
        pI915XvMC->alloc.size = 8 * 1024;
        pI915XvMC->alloc.offset = 0;
        pI915XvMC->alloc.ptr = malloc(pI915XvMC->alloc.size);
    }

   pI915XvMC->alloc.active_buf = 0;
   assert(pI915XvMC->alloc.ptr);
}

void intelBatchbufferRequireSpace(i915XvMCContext *pI915XvMC, unsigned sz)
{
   if (pI915XvMC->batch.space < sz)
      intelFlushBatch(pI915XvMC, TRUE);
}

void intelBatchbufferData(i915XvMCContext *pI915XvMC, const void *data, 
                          unsigned bytes, unsigned flags)
{
   assert((bytes & 0x3) == 0);

   intelBatchbufferRequireSpace(pI915XvMC, bytes);
   memcpy(pI915XvMC->batch.ptr, data, bytes);
   pI915XvMC->batch.ptr += bytes;
   pI915XvMC->batch.space -= bytes;

   assert(pI915XvMC->batch.space >= 0);
}

#define MI_FLUSH                ((0 << 29) | (4 << 23))
#define FLUSH_MAP_CACHE         (1 << 0)
#define FLUSH_RENDER_CACHE      (0 << 2)
#define FLUSH_WRITE_DIRTY_STATE (1 << 4)
void intelRefillBatchLocked(i915XvMCContext *pI915XvMC, Bool allow_unlock )
{
   unsigned half = pI915XvMC->alloc.size >> 1;
   unsigned buf = (pI915XvMC->alloc.active_buf ^= 1);
   unsigned dword[2];

   dword[0] = MI_FLUSH | FLUSH_WRITE_DIRTY_STATE | FLUSH_RENDER_CACHE | FLUSH_MAP_CACHE;
   dword[1] = 0;
   intelCmdIoctl(pI915XvMC, (char *)&dword[0], sizeof(dword));
   pI915XvMC->alloc.irq_emitted = intelEmitIrqLocked(pI915XvMC);

   if (pI915XvMC->alloc.irq_emitted) {
       intelWaitIrq(pI915XvMC, pI915XvMC->alloc.irq_emitted);
   }

   pI915XvMC->batch.start_offset = pI915XvMC->alloc.offset + buf * half;
   pI915XvMC->batch.ptr = (unsigned char *)pI915XvMC->alloc.ptr + buf * half;
   pI915XvMC->batch.size = half - 8;
   pI915XvMC->batch.space = half - 8;
   assert(pI915XvMC->batch.space >= 0);
}


void intelFlushBatchLocked(i915XvMCContext *pI915XvMC,
                           Bool ignore_cliprects,
                           Bool refill,
                           Bool allow_unlock)
{
   drmI830BatchBuffer batch;

   if (pI915XvMC->batch.space != pI915XvMC->batch.size) {

      batch.start = pI915XvMC->batch.start_offset;
      batch.used = pI915XvMC->batch.size - pI915XvMC->batch.space;
      batch.cliprects = 0;
      batch.num_cliprects = 0;
      batch.DR1 = 0;
      batch.DR4 = 0;

      if (pI915XvMC->alloc.offset) {
          if ((batch.used & 0x4) == 0) {
              ((int *)pI915XvMC->batch.ptr)[0] = 0;
              ((int *)pI915XvMC->batch.ptr)[1] = MI_BATCH_BUFFER_END;
              batch.used += 0x8;
              pI915XvMC->batch.ptr += 0x8;
          } else {
              ((int *)pI915XvMC->batch.ptr)[0] = MI_BATCH_BUFFER_END;
              batch.used += 0x4;
              pI915XvMC->batch.ptr += 0x4;
          }
      }

      pI915XvMC->batch.start_offset += batch.used;
      pI915XvMC->batch.size -= batch.used;

      if (pI915XvMC->batch.size < 8) {
         refill = TRUE;
         pI915XvMC->batch.space = pI915XvMC->batch.size = 0;
      }
      else {
         pI915XvMC->batch.size -= 8;
         pI915XvMC->batch.space = pI915XvMC->batch.size;
      }

      assert(pI915XvMC->batch.space >= 0);
      assert(batch.start >= pI915XvMC->alloc.offset);
      assert(batch.start < pI915XvMC->alloc.offset + pI915XvMC->alloc.size);
      assert(batch.start + batch.used > pI915XvMC->alloc.offset);
      assert(batch.start + batch.used <= pI915XvMC->alloc.offset + pI915XvMC->alloc.size);

      if (pI915XvMC->alloc.offset) {
          if (drmCommandWrite(xvmc_driver->fd, DRM_I830_BATCHBUFFER, &batch, sizeof(batch))) {
              fprintf(stderr, "DRM_I830_BATCHBUFFER: %d\n",  -errno);
              exit(1);
          }
      } else {
         drmI830CmdBuffer cmd;
         cmd.buf = (char *)pI915XvMC->alloc.ptr + batch.start;
         cmd.sz = batch.used;
         cmd.DR1 = batch.DR1;
         cmd.DR4 = batch.DR4;
         cmd.num_cliprects = batch.num_cliprects;
         cmd.cliprects = batch.cliprects;

         if (drmCommandWrite(xvmc_driver->fd, DRM_I830_CMDBUFFER, 
                             &cmd, sizeof(cmd))) {
            fprintf(stderr, "DRM_I915_CMDBUFFER: %d\n",  -errno);
            exit(1);
         }
      }
   }

   if (refill)
      intelRefillBatchLocked(pI915XvMC, allow_unlock);
}

void intelFlushBatch(i915XvMCContext *pI915XvMC, Bool refill )
{
   intelFlushBatchLocked(pI915XvMC, FALSE, refill, TRUE);
}

void intelCmdIoctl(i915XvMCContext *pI915XvMC, char *buf, unsigned used)
{
   drmI830CmdBuffer cmd;

   cmd.buf = buf;
   cmd.sz = used;
   cmd.cliprects = 0;
   cmd.num_cliprects = 0;
   cmd.DR1 = 0;
   cmd.DR4 = 0;

   if (drmCommandWrite(xvmc_driver->fd, DRM_I830_CMDBUFFER, 
                       &cmd, sizeof(cmd))) {
      fprintf(stderr, "DRM_I830_CMDBUFFER: %d\n",  -errno);
      exit(1);
   }
}
