/* $XConsortium: nvreg.h /main/2 1996/10/28 05:13:41 kaleb $ */
/*
 * Copyright 1996-1997  David J. McKay
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
 * DAVID J. MCKAY BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nvreg.h,v 1.6 2002/01/25 21:56:06 tsi Exp $ */

#ifndef __NVREG_H_
#define __NVREG_H_

#define NV_IMAGE_PATTERN              0x18
#define NV_IMAGE_BLACK_RECTANGLE      0x19
#define NV_MEMORY_TO_MEMORY_FORMAT    0x39
#define NV4_SURFACE                   0x42
#define NV_ROP5_SOLID                 0x43
#define NV4_IMAGE_PATTERN             0x44
#define NV4_GDI_RECTANGLE_TEXT        0x4a
#define NV4_RENDER_SOLID_LIN          0x5c
#define NV_IMAGE_BLIT                 0x5f
#define NV10_CONTEXT_SURFACES_2D      0x62
#define NV5_SCALED_IMAGE_FROM_MEMORY  0x63
#define NV_SCALED_IMAGE_FROM_MEMORY   0x77
#define NV10_SCALED_IMAGE_FROM_MEMORY 0x89
#define NV12_IMAGE_BLIT               0x9f


#define NV_PRAMIN_OFFSET            0x00710000
#define NV_PRAMIN_SIZE              0x00100000

#define NV_PCRTC0_OFFSET            0x00600000
#define NV_PCRTC0_SIZE              0x00001000 /* empirical */

#define NV_PRAMDAC0_OFFSET          0x00680000
#define NV_PRAMDAC0_SIZE            0x00001000

#define NV_PFB_OFFSET               0x00100000
#define NV_PFB_SIZE                 0x00001000

#define NV_PFIFO_OFFSET             0x00002000
#define NV_PFIFO_SIZE               0x00010000

#define NV_PGRAPH_OFFSET            0x00400000
#define NV_PGRAPH_SIZE              0x00010000

#define NV_PEXTDEV_OFFSET           0x00101000
#define NV_PEXTDEV_SIZE             0x00001000

#define NV_PTIMER_OFFSET            0x00009000
#define NV_PTIMER_SIZE              0x00001000

#define NV_PMC_OFFSET               0x00000000
#define NV_PMC_SIZE                 0x0000f000

#define NV_FIFO_OFFSET              0x00800000
#define NV_FIFO_SIZE                0x00800000

#define NV_PCIO0_OFFSET             0x00601000
#define NV_PCIO0_SIZE               0x00002000

#define NV_PDIO0_OFFSET             0x00681000
#define NV_PDIO0_SIZE               0x00002000

#define NV_PVIO_OFFSET              0x000C0000
#define NV_PVIO_SIZE                0x00008000

#define NV_PROM_OFFSET              0x00300000
#define NV_PROM_SIZE                0x00010000



#define NV_PGRAPH_STATUS            (0x00000700)
#define NV_PFIFO_RAMHT              (0x00000210)
#define NV_PFB_BOOT                 (0x00000000)
#define NV_PEXTDEV_BOOT             (0x00000000)

#define NV_RAMDAC_CURSOR_POS  0x300
#define NV_RAMDAC_CURSOR_CTRL       0x320
#define NV_RAMDAC_CURSOR_DATA_LO    0x324
#define NV_RAMDAC_CURSOR_DATA_HI    0x328


#endif


