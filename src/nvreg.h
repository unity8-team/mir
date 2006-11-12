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
#define NV4_SURFACE                   0x42
#define NV_ROP5_SOLID                 0x43
#define NV4_IMAGE_PATTERN             0x44
#define NV4_GDI_RECTANGLE_TEXT        0x4a
#define NV4_RENDER_SOLID_LIN          0x5c
#define NV5_SCALED_IMAGE_FROM_MEMORY  0x63
#define NV_SCALED_IMAGE_FROM_MEMORY   0x77
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

#define NV_RAMDAC_0404              0x404

#define NV_RAMDAC_NVPLL             0x500
#define NV_RAMDAC_MPLL              0x504
#	define NV_RAMDAC_PLL_COEFF_MDIV     0x000000FF
#	define NV_RAMDAC_PLL_COEFF_NDIV     0x0000FF00
#	define NV_RAMDAC_PLL_COEFF_PDIV     0x00070000

#define NV_RAMDAC_VPLL              0x508
#define NV_RAMDAC_PLL_SELECT        0x50c
#define NV_RAMDAC_VPLL2             0x520
#define NV_RAMDAC_DITHER_NV11       0x528
#define NV_RAMDAC_052C              0x52c

#define NV_RAMDAC_NVPLL_B           0x570
#define NV_RAMDAC_MPLL_B            0x574
#define NV_RAMDAC_VPLL_B            0x578
#define NV_RAMDAC_VPLL2_B           0x57c

#define NV_RAMDAC_GENERAL_CONTROL   0x600
#define NV_RAMDAC_TEST_CONTROL      0x608
#define NV_RAMDAC_TEST_DATA         0x610

#define NV_RAMDAC_FP_VDISP_END      0x800
#define NV_RAMDAC_FP_HDISP_END      0x820
#define NV_RAMDAC_FP_HCRTC          0x828
#define NV_RAMDAC_FP_DITHER         0x83c
#define NV_RAMDAC_FP_CONTROL        0x848

#define NV_RAMDAC_FP_TMDS_DATA      0x8b0
#define NV_RAMDAC_FP_TMDS_LVDS      0x8b4

#define NV_CRTC_INTR_0              0x100
#	define NV_CRTC_INTR_VBLANK           1
#define NV_CRTC_INTR_EN_0           0x140
#define NV_CRTC_START               0x800
#define NV_CRTC_CURSOR_CONFIG       0x810
#define NV_CRTC_081C                0x81c
#define NV_CRTC_0830                0x830
#define NV_CRTC_0834                0x834
#define NV_CRTC_HEAD_CONFIG         0x860

#define NV_PFB_CFG0                 0x200
#define NV_PFB_CFG1                 0x204
#define NV_PFB_020C                 0x20C
#define NV_PFB_TILE_NV10            0x240
#define NV_PFB_TILE_SIZE_NV10       0x244
#define NV_PFB_CLOSE_PAGE2          0x33C
#define NV_PFB_TILE_NV40            0x600
#define NV_PFB_TILE_SIZE_NV40       0x604

#define NV_PGRAPH_DEBUG_0           0x080
#define NV_PGRAPH_DEBUG_1           0x084
#define NV_PGRAPH_DEBUG_2           0x620
#define NV_PGRAPH_DEBUG_3           0x08c
#define NV_PGRAPH_DEBUG_4           0x090

#define NV_PGRAPH_INTR              0x100
#define NV_PGRAPH_INTR_EN           0x140
#define NV_PGRAPH_CTX_CONTROL       0x144
#define NV_PGRAPH_BETA_AND          0x608
#define NV_PGRAPH_LIMIT_VIOL_PIX    0x610

#define NV_PGRAPH_BOFFSET0          0x640
#define NV_PGRAPH_BOFFSET1          0x644
#define NV_PGRAPH_BOFFSET2          0x648

#define NV_PGRAPH_BLIMIT0           0x684
#define NV_PGRAPH_BLIMIT1           0x688
#define NV_PGRAPH_BLIMIT2           0x68c

#define NV_PGRAPH_SURFACE           0x710
#define NV_PGRAPH_STATE             0x714
#define NV_PGRAPH_FIFO              0x720

#define NV_PGRAPH_PATTERN_SHAPE     0x810

#endif


