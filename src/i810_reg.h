/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i810_reg.h,v 1.13 2003/02/06 04:18:04 dawes Exp $ */
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
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 *   based on the i740 driver by
 *        Kevin E. Martin <kevin@precisioninsight.com> 
 *   
 *
 */

#ifndef _I810_REG_H
#define _I810_REG_H

/* I/O register offsets
 */
#define SRX 0x3C4		/* p208 */
#define GRX 0x3CE		/* p213 */
#define ARX 0x3C0		/* p224 */

/* VGA Color Palette Registers */
#define DACMASK  0x3C6		/* p232 */
#define DACSTATE 0x3C7		/* p232 */
#define DACRX    0x3C7		/* p233 */
#define DACWX    0x3C8		/* p233 */
#define DACDATA  0x3C9		/* p233 */

/* CRT Controller Registers (CRX) */
#define START_ADDR_HI        0x0C /* p246 */
#define START_ADDR_LO        0x0D /* p247 */
#define VERT_SYNC_END        0x11 /* p249 */
#define EXT_VERT_TOTAL       0x30 /* p257 */
#define EXT_VERT_DISPLAY     0x31 /* p258 */
#define EXT_VERT_SYNC_START  0x32 /* p259 */
#define EXT_VERT_BLANK_START 0x33 /* p260 */
#define EXT_HORIZ_TOTAL      0x35 /* p261 */
#define EXT_HORIZ_BLANK      0x39 /* p261 */
#define EXT_START_ADDR       0x40 /* p262 */
#define EXT_START_ADDR_ENABLE    0x80 
#define EXT_OFFSET           0x41 /* p263 */
#define EXT_START_ADDR_HI    0x42 /* p263 */
#define INTERLACE_CNTL       0x70 /* p264 */
#define INTERLACE_ENABLE         0x80 
#define INTERLACE_DISABLE        0x00 

/* Miscellaneous Output Register 
 */
#define MSR_R          0x3CC	/* p207 */
#define MSR_W          0x3C2	/* p207 */
#define IO_ADDR_SELECT     0x01

#define MDA_BASE       0x3B0	/* p207 */
#define CGA_BASE       0x3D0	/* p207 */

/* CR80 - IO Control, p264
 */
#define IO_CTNL            0x80
#define EXTENDED_ATTR_CNTL     0x02
#define EXTENDED_CRTC_CNTL     0x01

/* GR10 - Address mapping, p221
 */
#define ADDRESS_MAPPING    0x10
#define PAGE_TO_LOCAL_MEM_ENABLE 0x10
#define GTT_MEM_MAP_ENABLE     0x08
#define PACKED_MODE_ENABLE     0x04
#define LINEAR_MODE_ENABLE     0x02
#define PAGE_MAPPING_ENABLE    0x01

/* Blitter control, p378
 */
#define BITBLT_CNTL        0x7000c
#define COLEXP_MODE            0x30
#define COLEXP_8BPP            0x00
#define COLEXP_16BPP           0x10
#define COLEXP_24BPP           0x20
#define COLEXP_RESERVED        0x30
#define BITBLT_STATUS          0x01

/* p375. 
 */
#define DISPLAY_CNTL       0x70008
#define VGA_WRAP_MODE          0x02
#define VGA_WRAP_AT_256KB      0x00
#define VGA_NO_WRAP            0x02
#define GUI_MODE               0x01
#define STANDARD_VGA_MODE      0x00
#define HIRES_MODE             0x01

/* p375
 */
#define PIXPIPE_CONFIG_0   0x70009
#define DAC_8_BIT              0x80
#define DAC_6_BIT              0x00
#define HW_CURSOR_ENABLE       0x10
#define EXTENDED_PALETTE       0x01

/* p375
 */
#define PIXPIPE_CONFIG_1   0x7000a
#define DISPLAY_COLOR_MODE     0x0F
#define DISPLAY_VGA_MODE       0x00
#define DISPLAY_8BPP_MODE      0x02
#define DISPLAY_15BPP_MODE     0x04
#define DISPLAY_16BPP_MODE     0x05
#define DISPLAY_24BPP_MODE     0x06
#define DISPLAY_32BPP_MODE     0x07

/* p375
 */
#define PIXPIPE_CONFIG_2   0x7000b
#define DISPLAY_GAMMA_ENABLE   0x08
#define DISPLAY_GAMMA_DISABLE  0x00
#define OVERLAY_GAMMA_ENABLE   0x04
#define OVERLAY_GAMMA_DISABLE  0x00


/* p380
 */
#define DISPLAY_BASE       0x70020
#define DISPLAY_BASE_MASK  0x03fffffc


/* Cursor control registers, pp383-384
 */
/* Desktop (845G, 865G) */
#define CURSOR_CONTROL     0x70080
#define CURSOR_ENABLE          0x80000000
#define CURSOR_GAMMA_ENABLE    0x40000000
#define CURSOR_STRIDE_MASK     0x30000000
#define CURSOR_FORMAT_SHIFT    24
#define CURSOR_FORMAT_MASK     (0x07 << CURSOR_FORMAT_SHIFT)
#define CURSOR_FORMAT_2C       (0x00 << CURSOR_FORMAT_SHIFT)
#define CURSOR_FORMAT_3C       (0x01 << CURSOR_FORMAT_SHIFT)
#define CURSOR_FORMAT_4C       (0x02 << CURSOR_FORMAT_SHIFT)
#define CURSOR_FORMAT_ARGB     (0x04 << CURSOR_FORMAT_SHIFT)
#define CURSOR_FORMAT_XRGB     (0x05 << CURSOR_FORMAT_SHIFT)

/* Mobile and i810 */
#define CURSOR_A_CONTROL   CURSOR_CONTROL
#define CURSOR_ORIGIN_SCREEN   0x00	/* i810 only */
#define CURSOR_ORIGIN_DISPLAY  0x1	/* i810 only */
#define CURSOR_MODE            0x27
#define CURSOR_MODE_DISABLE    0x00
#define CURSOR_MODE_32_4C_AX   0x01	/* i810 only */
#define CURSOR_MODE_64_3C      0x04
#define CURSOR_MODE_64_4C_AX   0x05
#define CURSOR_MODE_64_4C      0x06
#define CURSOR_MODE_64_32B_AX  0x07
#define CURSOR_MODE_64_ARGB_AX (0x20 | CURSOR_MODE_64_32B_AX)
#define MCURSOR_PIPE_SELECT    (1 << 28)
#define MCURSOR_PIPE_A         0x00
#define MCURSOR_PIPE_B         (1 << 28)
#define MCURSOR_GAMMA_ENABLE   (1 << 26)
#define MCURSOR_MEM_TYPE_LOCAL (1 << 25)


#define CURSOR_BASEADDR    0x70084
#define CURSOR_A_BASE      CURSOR_BASEADDR
#define CURSOR_BASEADDR_MASK 0x1FFFFF00
#define CURSOR_A_POSITION  0x70088
#define CURSOR_POS_SIGN        0x8000
#define CURSOR_POS_MASK        0x007FF
#define CURSOR_X_SHIFT	       0
#define CURSOR_Y_SHIFT         16
#define CURSOR_X_LO        0x70088
#define CURSOR_X_HI        0x70089
#define CURSOR_X_POS           0x00
#define CURSOR_X_NEG           0x80
#define CURSOR_Y_LO        0x7008A
#define CURSOR_Y_HI        0x7008B
#define CURSOR_Y_POS           0x00
#define CURSOR_Y_NEG           0x80

#define CURSOR_A_PALETTE0  0x70090
#define CURSOR_A_PALETTE1  0x70094
#define CURSOR_A_PALETTE2  0x70098
#define CURSOR_A_PALETTE3  0x7009C

#define CURSOR_SIZE	   0x700A0
#define CURSOR_SIZE_MASK       0x3FF
#define CURSOR_SIZE_HSHIFT     0
#define CURSOR_SIZE_VSHIFT     12

#define CURSOR_B_CONTROL   0x700C0
#define CURSOR_B_BASE      0x700C4
#define CURSOR_B_POSITION  0x700C8
#define CURSOR_B_PALETTE0  0x700D0
#define CURSOR_B_PALETTE1  0x700D4
#define CURSOR_B_PALETTE2  0x700D8
#define CURSOR_B_PALETTE3  0x700DC


/* Similar registers exist in Device 0 on the i810 (pp55-65), but I'm
 * not sure they refer to local (graphics) memory.
 *
 * These details are for the local memory control registers,
 * (pp301-310).  The test machines are not equiped with local memory,
 * so nothing is tested.  Only a single row seems to be supported.
 */
#define DRAM_ROW_TYPE      0x3000
#define DRAM_ROW_0             0x01
#define DRAM_ROW_0_SDRAM       0x01
#define DRAM_ROW_0_EMPTY       0x00
#define DRAM_ROW_CNTL_LO   0x3001
#define DRAM_PAGE_MODE_CTRL    0x10
#define DRAM_RAS_TO_CAS_OVRIDE 0x08
#define DRAM_CAS_LATENCY       0x04
#define DRAM_RAS_TIMING        0x02
#define DRAM_RAS_PRECHARGE     0x01
#define DRAM_ROW_CNTL_HI   0x3002
#define DRAM_REFRESH_RATE      0x18
#define DRAM_REFRESH_DISABLE   0x00
#define DRAM_REFRESH_60HZ      0x08
#define DRAM_REFRESH_FAST_TEST 0x10
#define DRAM_REFRESH_RESERVED  0x18
#define DRAM_SMS               0x07
#define DRAM_SMS_NORMAL        0x00
#define DRAM_SMS_NOP_ENABLE    0x01
#define DRAM_SMS_ABPCE         0x02
#define DRAM_SMS_MRCE          0x03
#define DRAM_SMS_CBRCE         0x04

/* p307
 */
#define DPMS_SYNC_SELECT   0x5002
#define VSYNC_CNTL             0x08
#define VSYNC_ON               0x00
#define VSYNC_OFF              0x08
#define HSYNC_CNTL             0x02
#define HSYNC_ON               0x00
#define HSYNC_OFF              0x02



/* p317, 319
 */
#define VCLK2_VCO_M        0x6008 /* treat as 16 bit? (includes msbs) */
#define VCLK2_VCO_N        0x600a
#define VCLK2_VCO_DIV_SEL  0x6012

#define VCLK_DIVISOR_VGA0   0x6000
#define VCLK_DIVISOR_VGA1   0x6004
#define VCLK_POST_DIV	    0x6010

#define POST_DIV_SELECT        0x70
#define POST_DIV_1             0x00
#define POST_DIV_2             0x10
#define POST_DIV_4             0x20
#define POST_DIV_8             0x30
#define POST_DIV_16            0x40
#define POST_DIV_32            0x50
#define VCO_LOOP_DIV_BY_4M     0x00
#define VCO_LOOP_DIV_BY_16M    0x04


/* Instruction Parser Mode Register 
 *    - p281
 *    - 2 new bits.
 */
#define INST_PM                  0x20c0	
#define AGP_SYNC_PACKET_FLUSH_ENABLE 0x20 /* reserved */
#define SYNC_PACKET_FLUSH_ENABLE     0x10
#define TWO_D_INST_DISABLE           0x08
#define THREE_D_INST_DISABLE         0x04
#define STATE_VAR_UPDATE_DISABLE     0x02
#define PAL_STIP_DISABLE             0x01


#define MEMMODE                  0x20dc


/* Instruction parser error register.  p279
 */
#define IPEIR                  0x2088
#define IPEHR                  0x208C
#define INST_DONE                0x2090
#define INST_PS                  0x20c4
#define IPEIR_I965                  0x2064 /* i965 */
#define IPEHR_I965                  0x2068 /* i965 */
#define INST_DONE_I965              0x206c
#define INST_PS_I965                0x2070
#define ACTHD                 0x2074
#define DMA_FADD_P             0x2078
#define INST_DONE_1              0x207c

#define CACHE_MODE_0           0x2120
#define CACHE_MODE_1           0x2124
#define MI_ARB_STATE           0x20e4

#define WIZ_CTL                0x7c00
#define WIZ_CTL_SINGLE_SUBSPAN  (1<<6)
#define WIZ_CTL_IGNORE_STALLS  (1<<5)

#define SVG_WORK_CTL           0x7408

#define TS_CTL                 0x7e00
#define TS_MUX_ERR_CODE        (0<<8)
#define TS_MUX_URB_0           (1<<8)
#define TS_MUX_DISPATCH_ID_0   (10<<8)
#define TS_MUX_ERR_CODE_VALID  (15<<8)
#define TS_MUX_TID_0           (16<<8)
#define TS_MUX_EUID_0          (18<<8)
#define TS_MUX_FFID_0          (22<<8)
#define TS_MUX_EOT             (26<<8)
#define TS_MUX_SIDEBAND_0      (27<<8)
#define TS_SNAP_ALL_CHILD      (1<<2)
#define TS_SNAP_ALL_ROOT       (1<<1)
#define TS_SNAP_ENABLE         (1<<0)

#define TS_DEBUG_DATA          0x7e0c

#define TD_CTL                 0x8000
#define TD_CTL2                0x8004


#define ECOSKPD 0x21d0
#define EXCC    0x2028

/* I965 debug regs:
 */
#define IA_VERTICES_COUNT_QW   0x2310
#define IA_PRIMITIVES_COUNT_QW 0x2318
#define VS_INVOCATION_COUNT_QW 0x2320
#define GS_INVOCATION_COUNT_QW 0x2328
#define GS_PRIMITIVES_COUNT_QW 0x2330
#define CL_INVOCATION_COUNT_QW 0x2338
#define CL_PRIMITIVES_COUNT_QW 0x2340
#define PS_INVOCATION_COUNT_QW 0x2348
#define PS_DEPTH_COUNT_QW      0x2350
#define TIMESTAMP_QW           0x2358
#define CLKCMP_QW              0x2360






/* General error reporting regs, p296
 */
#define EIR               0x20B0
#define EMR               0x20B4
#define ESR               0x20B8
#define IP_ERR                    0x0001
#define ERROR_RESERVED            0xffc6


/* Interrupt Control Registers 
 *   - new bits for i810
 *   - new register hwstam (mask)
 */
#define HWSTAM               0x2098 /* p290 */
#define IER                  0x20a0 /* p291 */
#define IIR                  0x20a4 /* p292 */
#define IMR                  0x20a8 /* p293 */
#define ISR                  0x20ac /* p294 */
#define HW_ERROR                 0x8000
#define SYNC_STATUS_TOGGLE       0x1000
#define DPY_0_FLIP_PENDING       0x0800
#define DPY_1_FLIP_PENDING       0x0400	/* not implemented on i810 */
#define OVL_0_FLIP_PENDING       0x0200
#define OVL_1_FLIP_PENDING       0x0100	/* not implemented on i810 */
#define DPY_0_VBLANK             0x0080
#define DPY_0_EVENT              0x0040
#define DPY_1_VBLANK             0x0020	/* not implemented on i810 */
#define DPY_1_EVENT              0x0010	/* not implemented on i810 */
#define HOST_PORT_EVENT          0x0008	/*  */
#define CAPTURE_EVENT            0x0004	/*  */
#define USER_DEFINED             0x0002
#define BREAKPOINT               0x0001


#define INTR_RESERVED            (0x6000 | 		\
				  DPY_1_FLIP_PENDING |	\
				  OVL_1_FLIP_PENDING |	\
				  DPY_1_VBLANK |	\
				  DPY_1_EVENT |		\
				  HOST_PORT_EVENT |	\
				  CAPTURE_EVENT )

/* FIFO Watermark and Burst Length Control Register 
 *
 * - different offset and contents on i810 (p299) (fewer bits per field)
 * - some overlay fields added
 * - what does it all mean?
 */
#define FWATER_BLC       0x20d8
#define FWATER_BLC2	 0x20dc
#define MM_BURST_LENGTH     0x00700000
#define MM_FIFO_WATERMARK   0x0001F000
#define LM_BURST_LENGTH     0x00000700
#define LM_FIFO_WATERMARK   0x0000001F


/* Fence/Tiling ranges [0..7]
 */
#define FENCE            0x2000
#define FENCE_NR         8

#define FENCE_NEW        0x3000
#define FENCE_NEW_NR     16

#define FENCE_LINEAR     0
#define FENCE_XMAJOR	 1
#define FENCE_YMAJOR  	 2

#define I915G_FENCE_START_MASK	0x0ff00000

#define I830_FENCE_START_MASK	0x07f80000

#define FENCE_START_MASK    0x03F80000
#define FENCE_X_MAJOR       0x00000000
#define FENCE_Y_MAJOR       0x00001000
#define FENCE_SIZE_MASK     0x00000700
#define FENCE_SIZE_512K     0x00000000
#define FENCE_SIZE_1M       0x00000100
#define FENCE_SIZE_2M       0x00000200
#define FENCE_SIZE_4M       0x00000300
#define FENCE_SIZE_8M       0x00000400
#define FENCE_SIZE_16M      0x00000500
#define FENCE_SIZE_32M      0x00000600
#define FENCE_SIZE_64M	    0x00000700
#define I915G_FENCE_SIZE_1M       0x00000000
#define I915G_FENCE_SIZE_2M       0x00000100
#define I915G_FENCE_SIZE_4M       0x00000200
#define I915G_FENCE_SIZE_8M       0x00000300
#define I915G_FENCE_SIZE_16M      0x00000400
#define I915G_FENCE_SIZE_32M      0x00000500
#define I915G_FENCE_SIZE_64M	0x00000600
#define I915G_FENCE_SIZE_128M	0x00000700
#define FENCE_PITCH_1       0x00000000
#define FENCE_PITCH_2       0x00000010
#define FENCE_PITCH_4       0x00000020
#define FENCE_PITCH_8       0x00000030
#define FENCE_PITCH_16      0x00000040
#define FENCE_PITCH_32      0x00000050
#define FENCE_PITCH_64	    0x00000060
#define FENCE_VALID         0x00000001


/* Registers to control page table, p274
 */
#define PGETBL_CTL       0x2020
#define PGETBL_ADDR_MASK    0xFFFFF000
#define PGETBL_ENABLE_MASK  0x00000001
#define PGETBL_ENABLED      0x00000001

/* Register containing pge table error results, p276
 */
#define PGE_ERR          0x2024
#define PGE_ERR_ADDR_MASK   0xFFFFF000
#define PGE_ERR_ID_MASK     0x00000038
#define PGE_ERR_CAPTURE     0x00000000
#define PGE_ERR_OVERLAY     0x00000008
#define PGE_ERR_DISPLAY     0x00000010
#define PGE_ERR_HOST        0x00000018
#define PGE_ERR_RENDER      0x00000020
#define PGE_ERR_BLITTER     0x00000028
#define PGE_ERR_MAPPING     0x00000030
#define PGE_ERR_CMD_PARSER  0x00000038
#define PGE_ERR_TYPE_MASK   0x00000007
#define PGE_ERR_INV_TABLE   0x00000000
#define PGE_ERR_INV_PTE     0x00000001
#define PGE_ERR_MIXED_TYPES 0x00000002
#define PGE_ERR_PAGE_MISS   0x00000003
#define PGE_ERR_ILLEGAL_TRX 0x00000004
#define PGE_ERR_LOCAL_MEM   0x00000005
#define PGE_ERR_TILED       0x00000006



/* Page table entries loaded via mmio region, p323
 */
#define PTE_BASE         0x10000
#define PTE_ADDR_MASK       0x3FFFF000
#define PTE_TYPE_MASK       0x00000006
#define PTE_LOCAL           0x00000002
#define PTE_MAIN_UNCACHED   0x00000000
#define PTE_MAIN_CACHED     0x00000006
#define PTE_VALID_MASK      0x00000001
#define PTE_VALID           0x00000001


/* Ring buffer registers, p277, overview p19
 */
#define LP_RING     0x2030
#define HP_RING     0x2040

#define RING_TAIL      0x00
#define TAIL_ADDR           0x000FFFF8
#define I830_TAIL_MASK	    0x001FFFF8

#define RING_HEAD      0x04
#define HEAD_WRAP_COUNT     0xFFE00000
#define HEAD_WRAP_ONE       0x00200000
#define HEAD_ADDR           0x001FFFFC
#define I830_HEAD_MASK      0x001FFFFC

#define RING_START     0x08
#define START_ADDR          0x03FFFFF8
#define I830_RING_START_MASK	0xFFFFF000

#define RING_LEN       0x0C
#define RING_NR_PAGES       0x001FF000 
#define I830_RING_NR_PAGES	0x001FF000
#define RING_REPORT_MASK    0x00000006
#define RING_REPORT_64K     0x00000002
#define RING_REPORT_128K    0x00000004
#define RING_NO_REPORT      0x00000000
#define RING_VALID_MASK     0x00000001
#define RING_VALID          0x00000001
#define RING_INVALID        0x00000000



/* BitBlt Instructions
 *
 * There are many more masks & ranges yet to add.
 */
#define BR00_BITBLT_CLIENT   0x40000000
#define BR00_OP_COLOR_BLT    0x10000000
#define BR00_OP_SRC_COPY_BLT 0x10C00000
#define BR00_OP_FULL_BLT     0x11400000
#define BR00_OP_MONO_SRC_BLT 0x11800000
#define BR00_OP_MONO_SRC_COPY_BLT 0x11000000
#define BR00_OP_MONO_PAT_BLT 0x11C00000
#define BR00_OP_MONO_SRC_COPY_IMMEDIATE_BLT (0x61 << 22)
#define BR00_OP_TEXT_IMMEDIATE_BLT 0xc000000


#define BR00_TPCY_DISABLE    0x00000000
#define BR00_TPCY_ENABLE     0x00000010

#define BR00_TPCY_ROP        0x00000000
#define BR00_TPCY_NO_ROP     0x00000020
#define BR00_TPCY_EQ         0x00000000
#define BR00_TPCY_NOT_EQ     0x00000040

#define BR00_PAT_MSB_FIRST   0x00000000	/* ? */

#define BR00_PAT_VERT_ALIGN  0x000000e0

#define BR00_LENGTH          0x0000000F

#define BR09_DEST_ADDR       0x03FFFFFF

#define BR11_SOURCE_PITCH    0x00003FFF

#define BR12_SOURCE_ADDR     0x03FFFFFF

#define BR13_SOLID_PATTERN   0x80000000
#define BR13_RIGHT_TO_LEFT   0x40000000
#define BR13_LEFT_TO_RIGHT   0x00000000
#define BR13_MONO_TRANSPCY   0x20000000
#define BR13_MONO_PATN_TRANS 0x10000000
#define BR13_USE_DYN_DEPTH   0x04000000
#define BR13_DYN_8BPP        0x00000000
#define BR13_DYN_16BPP       0x01000000
#define BR13_DYN_24BPP       0x02000000
#define BR13_ROP_MASK        0x00FF0000
#define BR13_DEST_PITCH      0x0000FFFF
#define BR13_PITCH_SIGN_BIT  0x00008000

#define BR14_DEST_HEIGHT     0xFFFF0000
#define BR14_DEST_WIDTH      0x0000FFFF

#define BR15_PATTERN_ADDR    0x03FFFFFF

#define BR16_SOLID_PAT_COLOR 0x00FFFFFF
#define BR16_BACKGND_PAT_CLR 0x00FFFFFF

#define BR17_FGND_PAT_CLR    0x00FFFFFF

#define BR18_SRC_BGND_CLR    0x00FFFFFF
#define BR19_SRC_FGND_CLR    0x00FFFFFF


/* Instruction parser instructions
 */

#define INST_PARSER_CLIENT   0x00000000
#define INST_OP_FLUSH        0x02000000
#define INST_FLUSH_MAP_CACHE 0x00000001


#define GFX_OP_USER_INTERRUPT ((0<<29)|(2<<23))


/* Registers in the i810 host-pci bridge pci config space which affect
 * the i810 graphics operations.  
 */
#define SMRAM_MISCC         0x70
#define GMS                    0x000000c0
#define GMS_DISABLE            0x00000000
#define GMS_ENABLE_BARE        0x00000040
#define GMS_ENABLE_512K        0x00000080
#define GMS_ENABLE_1M          0x000000c0
#define USMM                   0x00000030 
#define USMM_DISABLE           0x00000000
#define USMM_TSEG_ZERO         0x00000010
#define USMM_TSEG_512K         0x00000020
#define USMM_TSEG_1M           0x00000030  
#define GFX_MEM_WIN_SIZE       0x00010000
#define GFX_MEM_WIN_32M        0x00010000
#define GFX_MEM_WIN_64M        0x00000000

/* Overkill?  I don't know.  Need to figure out top of mem to make the
 * SMRAM calculations come out.  Linux seems to have problems
 * detecting it all on its own, so this seems a reasonable double
 * check to any user supplied 'mem=...' boot param.
 *
 * ... unfortunately this reg doesn't work according to spec on the
 * test hardware.
 */
#define WHTCFG_PAMR_DRP      0x50
#define SYS_DRAM_ROW_0_SHIFT    16
#define SYS_DRAM_ROW_1_SHIFT    20
#define DRAM_MASK           0x0f
#define DRAM_VALUE_0        0
#define DRAM_VALUE_1        8
/* No 2 value defined */
#define DRAM_VALUE_3        16
#define DRAM_VALUE_4        16
#define DRAM_VALUE_5        24
#define DRAM_VALUE_6        32
#define DRAM_VALUE_7        32
#define DRAM_VALUE_8        48
#define DRAM_VALUE_9        64
#define DRAM_VALUE_A        64
#define DRAM_VALUE_B        96
#define DRAM_VALUE_C        128
#define DRAM_VALUE_D        128
#define DRAM_VALUE_E        192
#define DRAM_VALUE_F        256	/* nice one, geezer */
#define LM_FREQ_MASK        0x10
#define LM_FREQ_133         0x10
#define LM_FREQ_100         0x00




/* These are 3d state registers, but the state is invarient, so we let
 * the X server handle it:
 */



/* GFXRENDERSTATE_COLOR_CHROMA_KEY, p135
 */
#define GFX_OP_COLOR_CHROMA_KEY  ((0x3<<29)|(0x1d<<24)|(0x2<<16)|0x1)
#define CC1_UPDATE_KILL_WRITE    (1<<28)
#define CC1_ENABLE_KILL_WRITE    (1<<27)
#define CC1_DISABLE_KILL_WRITE    0
#define CC1_UPDATE_COLOR_IDX     (1<<26)
#define CC1_UPDATE_CHROMA_LOW    (1<<25)
#define CC1_UPDATE_CHROMA_HI     (1<<24)
#define CC1_CHROMA_LOW_MASK      ((1<<24)-1)
#define CC2_COLOR_IDX_SHIFT      24
#define CC2_COLOR_IDX_MASK       (0xff<<24)
#define CC2_CHROMA_HI_MASK       ((1<<24)-1)


#define GFX_CMD_CONTEXT_SEL      ((0<<29)|(0x5<<23))
#define CS_UPDATE_LOAD           (1<<17)
#define CS_UPDATE_USE            (1<<16)
#define CS_UPDATE_LOAD           (1<<17)
#define CS_LOAD_CTX0             0
#define CS_LOAD_CTX1             (1<<8)
#define CS_USE_CTX0              0
#define CS_USE_CTX1              (1<<0)

/* I810 LCD/TV registers */
#define LCD_TV_HTOTAL	0x60000
#define LCD_TV_C	0x60018
#define LCD_TV_OVRACT   0x6001C

#define LCD_TV_ENABLE (1 << 31)
#define LCD_TV_VGAMOD (1 << 28)

/* I830 CRTC registers */
#define HTOTAL_A	0x60000
#define HBLANK_A	0x60004
#define HSYNC_A 	0x60008
#define VTOTAL_A	0x6000c
#define VBLANK_A	0x60010
#define VSYNC_A 	0x60014
#define PIPEASRC	0x6001c
#define BCLRPAT_A	0x60020

#define HTOTAL_B	0x61000
#define HBLANK_B	0x61004
#define HSYNC_B 	0x61008
#define VTOTAL_B	0x6100c
#define VBLANK_B	0x61010
#define VSYNC_B 	0x61014
#define PIPEBSRC	0x6101c
#define BCLRPAT_B	0x61020

#define DPLL_A		0x06014
#define DPLL_B		0x06018
#define FPA0		0x06040
#define FPA1		0x06044

#define I830_HTOTAL_MASK 	0xfff0000
#define I830_HACTIVE_MASK	0x7ff

#define I830_HBLANKEND_MASK	0xfff0000
#define I830_HBLANKSTART_MASK    0xfff

#define I830_HSYNCEND_MASK	0xfff0000
#define I830_HSYNCSTART_MASK    0xfff

#define I830_VTOTAL_MASK 	0xfff0000
#define I830_VACTIVE_MASK	0x7ff

#define I830_VBLANKEND_MASK	0xfff0000
#define I830_VBLANKSTART_MASK    0xfff

#define I830_VSYNCEND_MASK	0xfff0000
#define I830_VSYNCSTART_MASK    0xfff

#define I830_PIPEA_HORZ_MASK	0x7ff0000
#define I830_PIPEA_VERT_MASK	0x7ff

#define ADPA			0x61100
#define ADPA_DAC_ENABLE 	(1<<31)
#define ADPA_DAC_DISABLE	0
#define ADPA_PIPE_SELECT_MASK	(1<<30)
#define ADPA_PIPE_A_SELECT	0
#define ADPA_PIPE_B_SELECT	(1<<30)
#define ADPA_USE_VGA_HVPOLARITY (1<<15)
#define ADPA_SETS_HVPOLARITY	0
#define ADPA_VSYNC_CNTL_DISABLE (1<<11)
#define ADPA_VSYNC_CNTL_ENABLE	0
#define ADPA_HSYNC_CNTL_DISABLE (1<<10)
#define ADPA_HSYNC_CNTL_ENABLE	0
#define ADPA_VSYNC_ACTIVE_HIGH	(1<<4)
#define ADPA_VSYNC_ACTIVE_LOW	0
#define ADPA_HSYNC_ACTIVE_HIGH	(1<<3)
#define ADPA_HSYNC_ACTIVE_LOW	0


#define DVOA			0x61120
#define DVOB			0x61140
#define DVOC			0x61160
#define DVO_ENABLE		(1<<31)

#define DVOA_SRCDIM		0x61124
#define DVOB_SRCDIM		0x61144
#define DVOC_SRCDIM		0x61164

#define LVDS			0x61180

#define PIPEACONF 0x70008
#define PIPEACONF_ENABLE	(1<<31)
#define PIPEACONF_DISABLE	0
#define PIPEACONF_DOUBLE_WIDE	(1<<30)
#define PIPEACONF_SINGLE_WIDE	0
#define PIPEACONF_PIPE_UNLOCKED 0
#define PIPEACONF_PIPE_LOCKED	(1<<25)
#define PIPEACONF_PALETTE	0
#define PIPEACONF_GAMMA 	(1<<24)

#define PIPEBCONF 0x71008
#define PIPEBCONF_ENABLE	(1<<31)
#define PIPEBCONF_DISABLE	0
#define PIPEBCONF_DOUBLE_WIDE	(1<<30)
#define PIPEBCONF_DISABLE	0
#define PIPEBCONF_GAMMA 	(1<<24)
#define PIPEBCONF_PALETTE	0

#define DSPACNTR		0x70180
#define DSPBCNTR		0x71180
#define DISPLAY_PLANE_ENABLE 			(1<<31)
#define DISPLAY_PLANE_DISABLE			0
#define DISPPLANE_GAMMA_ENABLE			(1<<30)
#define DISPPLANE_GAMMA_DISABLE			0
#define DISPPLANE_PIXFORMAT_MASK		(0xf<<26)
#define DISPPLANE_8BPP				(0x2<<26)
#define DISPPLANE_15_16BPP			(0x4<<26)
#define DISPPLANE_16BPP				(0x5<<26)
#define DISPPLANE_32BPP_NO_ALPHA 		(0x6<<26)
#define DISPPLANE_32BPP				(0x7<<26)
#define DISPPLANE_STEREO_ENABLE			(1<<25)
#define DISPPLANE_STEREO_DISABLE		0
#define DISPPLANE_SEL_PIPE_MASK			(1<<24)
#define DISPPLANE_SEL_PIPE_A			0
#define DISPPLANE_SEL_PIPE_B			(1<<24)
#define DISPPLANE_SRC_KEY_ENABLE		(1<<22)
#define DISPPLANE_SRC_KEY_DISABLE		0
#define DISPPLANE_LINE_DOUBLE			(1<<20)
#define DISPPLANE_NO_LINE_DOUBLE		0
#define DISPPLANE_STEREO_POLARITY_FIRST		0
#define DISPPLANE_STEREO_POLARITY_SECOND	(1<<18)
/* plane B only */
#define DISPPLANE_ALPHA_TRANS_ENABLE		(1<<15)
#define DISPPLANE_ALPHA_TRANS_DISABLE		0
#define DISPPLANE_SPRITE_ABOVE_DISPLAYA		0
#define DISPPLANE_SPRITE_ABOVE_OVERLAY		(1)

#define DSPABASE		0x70184
#define DSPASTRIDE		0x70188

#define DSPBBASE		0x71184
#define DSPBADDR		DSPBBASE
#define DSPBSTRIDE		0x71188

#define DSPAPOS			0x7018C /* reserved */
#define DSPASIZE		0x70190
#define DSPBPOS			0x7118C
#define DSPBSIZE		0x71190

#define DSPASURF		0x7019C
#define DSPATILEOFF		0x701A4

#define DSPBSURF		0x7119C
#define DSPBTILEOFF		0x711A4

/* Various masks for reserved bits, etc. */
#define I830_FWATER1_MASK        (~((1<<11)|(1<<10)|(1<<9)|      \
        (1<<8)|(1<<26)|(1<<25)|(1<<24)|(1<<5)|(1<<4)|(1<<3)|    \
        (1<<2)|(1<<1)|1|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16)))
#define I830_FWATER2_MASK ~(0)

#define DV0A_RESERVED ((1<<26)|(1<<25)|(1<<24)|(1<<23)|(1<<22)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<16)|(1<<5)|(1<<1)|1)
#define DV0B_RESERVED ((1<<27)|(1<<26)|(1<<25)|(1<<24)|(1<<23)|(1<<22)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<16)|(1<<5)|(1<<1)|1)
#define VGA0_N_DIVISOR_MASK     ((1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16))
#define VGA0_M1_DIVISOR_MASK    ((1<<13)|(1<<12)|(1<<11)|(1<<10)|(1<<9)|(1<<8))
#define VGA0_M2_DIVISOR_MASK    ((1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<1)|1)
#define VGA0_M1M2N_RESERVED	~(VGA0_N_DIVISOR_MASK|VGA0_M1_DIVISOR_MASK|VGA0_M2_DIVISOR_MASK)
#define VGA0_POSTDIV_MASK       ((1<<7)|(1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<1)|1)
#define VGA1_POSTDIV_MASK       ((1<<15)|(1<<13)|(1<<12)|(1<<11)|(1<<10)|(1<<9)|(1<<8))
#define VGA_POSTDIV_RESERVED	~(VGA0_POSTDIV_MASK|VGA1_POSTDIV_MASK|(1<<7)|(1<<15))
#define DPLLA_POSTDIV_MASK ((1<<23)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16))
#define DPLLA_RESERVED     ((1<<27)|(1<<26)|(1<<25)|(1<<24)|(1<<22)|(1<<15)|(1<<12)|(1<<11)|(1<<10)|(1<<9)|(1<<8)|(1<<7)|(1<<6)|(1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<1)|1)
#define ADPA_RESERVED	((1<<2)|(1<<1)|1|(1<<9)|(1<<8)|(1<<7)|(1<<6)|(1<<5)|(1<<30)|(1<<29)|(1<<28)|(1<<27)|(1<<26)|(1<<25)|(1<<24)|(1<<23)|(1<<22)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16))
#define SUPER_WORD              32
#define BURST_A_MASK    ((1<<11)|(1<<10)|(1<<9)|(1<<8))
#define BURST_B_MASK    ((1<<26)|(1<<25)|(1<<24))
#define WATER_A_MASK    ((1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<1)|1)
#define WATER_B_MASK    ((1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16))
#define WATER_RESERVED	((1<<31)|(1<<30)|(1<<29)|(1<<28)|(1<<27)|(1<<23)|(1<<22)|(1<<21)|(1<<15)|(1<<14)|(1<<13)|(1<<12)|(1<<7)|(1<<6))
#define PIPEACONF_RESERVED ((1<<29)|(1<<28)|(1<<27)|(1<<23)|(1<<22)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16)|0xffff)
#define PIPEBCONF_RESERVED ((1<<30)|(1<<29)|(1<<28)|(1<<27)|(1<<26)|(1<<25)|(1<<23)|(1<<22)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16)|0xffff)
#define DSPACNTR_RESERVED ((1<<23)|(1<<19)|(1<<17)|(1<<16)|0xffff)
#define DSPBCNTR_RESERVED ((1<<23)|(1<<19)|(1<<17)|(1<<16)|0x7ffe)

#define I830_GMCH_CTRL		0x52

#define I830_GMCH_ENABLED	0x4
#define I830_GMCH_MEM_MASK	0x1
#define I830_GMCH_MEM_64M	0x1
#define I830_GMCH_MEM_128M	0

#define I830_GMCH_GMS_MASK			0x70
#define I830_GMCH_GMS_DISABLED		0x00
#define I830_GMCH_GMS_LOCAL			0x10
#define I830_GMCH_GMS_STOLEN_512	0x20
#define I830_GMCH_GMS_STOLEN_1024	0x30
#define I830_GMCH_GMS_STOLEN_8192	0x40

#define I830_RDRAM_CHANNEL_TYPE		0x03010
#define I830_RDRAM_ND(x)			(((x) & 0x20) >> 5)
#define I830_RDRAM_DDT(x)			(((x) & 0x18) >> 3)

#define I855_GMCH_GMS_MASK			(0x7 << 4)
#define I855_GMCH_GMS_DISABLED			0x00
#define I855_GMCH_GMS_STOLEN_1M			(0x1 << 4)
#define I855_GMCH_GMS_STOLEN_4M			(0x2 << 4)
#define I855_GMCH_GMS_STOLEN_8M			(0x3 << 4)
#define I855_GMCH_GMS_STOLEN_16M		(0x4 << 4)
#define I855_GMCH_GMS_STOLEN_32M		(0x5 << 4)
#define I915G_GMCH_GMS_STOLEN_48M		(0x6 << 4)
#define I915G_GMCH_GMS_STOLEN_64M		(0x7 << 4)

#define I85X_CAPID			0x44
#define I85X_VARIANT_MASK			0x7
#define I85X_VARIANT_SHIFT			5
#define I855_GME				0x0
#define I855_GM					0x4
#define I852_GME				0x2
#define I852_GM					0x5

#define CMD_MI				(0 << 29)
#define CMD_2D				(2 << 29)
#define CMD_3D				(3 << 29)
/* BLT commands */
#define COLOR_BLT_CMD		((2<<29)|(0x40<<22)|(0x3))
#define COLOR_BLT_WRITE_ALPHA	(1<<21)
#define COLOR_BLT_WRITE_RGB	(1<<20)

#define XY_COLOR_BLT_CMD		((2<<29)|(0x50<<22)|(0x4))
#define XY_COLOR_BLT_WRITE_ALPHA	(1<<21)
#define XY_COLOR_BLT_WRITE_RGB		(1<<20)

#define XY_SETUP_CLIP_BLT_CMD		((2<<29)|(3<<22)|1)

#define XY_SRC_COPY_BLT_CMD		((2<<29)|(0x53<<22)|6)
#define XY_SRC_COPY_BLT_WRITE_ALPHA	(1<<21)
#define XY_SRC_COPY_BLT_WRITE_RGB	(1<<20)

#define SRC_COPY_BLT_CMD		((2<<29)|(0x43<<22)|0x4)
#define SRC_COPY_BLT_WRITE_ALPHA	(1<<21)
#define SRC_COPY_BLT_WRITE_RGB		(1<<20)

#define XY_MONO_PAT_BLT_CMD		((0x2<<29)|(0x52<<22)|0x7)
#define XY_MONO_PAT_VERT_SEED		((1<<10)|(1<<9)|(1<<8))
#define XY_MONO_PAT_HORT_SEED		((1<<14)|(1<<13)|(1<<12))
#define XY_MONO_PAT_BLT_WRITE_ALPHA	(1<<21)
#define XY_MONO_PAT_BLT_WRITE_RGB	(1<<20)

#define XY_MONO_SRC_BLT_CMD		((0x2<<29)|(0x54<<22)|(0x6))
#define XY_MONO_SRC_BLT_WRITE_ALPHA	(1<<21)
#define XY_MONO_SRC_BLT_WRITE_RGB	(1<<20)

/* 3d state */
#define STATE3D_ANTI_ALIASING		(CMD_3D | (0x06<<24))
#define LINE_CAP_WIDTH_MODIFY		(1 << 16)
#define LINE_CAP_WIDTH_1_0		(0x1 << 14)
#define LINE_WIDTH_MODIFY		(1 << 8)
#define LINE_WIDTH_1_0			(0x1 << 6)

#define STATE3D_RASTERIZATION_RULES	(CMD_3D | (0x07<<24))
#define ENABLE_POINT_RASTER_RULE	(1<<15)
#define OGL_POINT_RASTER_RULE		(1<<13)
#define ENABLE_TEXKILL_3D_4D            (1<<10)
#define TEXKILL_3D                      (0<<9)
#define TEXKILL_4D                      (1<<9)
#define ENABLE_LINE_STRIP_PROVOKE_VRTX	(1<<8)
#define ENABLE_TRI_FAN_PROVOKE_VRTX	(1<<5)
#define LINE_STRIP_PROVOKE_VRTX(x)	((x)<<6)
#define TRI_FAN_PROVOKE_VRTX(x) 	((x)<<3)

#define STATE3D_INDEPENDENT_ALPHA_BLEND	(CMD_3D | (0x0b<<24))
#define IAB_MODIFY_ENABLE	        (1<<23)
#define IAB_ENABLE       	        (1<<22)
#define IAB_MODIFY_FUNC         	(1<<21)
#define IAB_FUNC_SHIFT          	16
#define IAB_MODIFY_SRC_FACTOR   	(1<<11)
#define IAB_SRC_FACTOR_SHIFT		6
#define IAB_SRC_FACTOR_MASK		(BLENDFACT_MASK<<6)
#define IAB_MODIFY_DST_FACTOR	        (1<<5)
#define IAB_DST_FACTOR_SHIFT		0
#define IAB_DST_FACTOR_MASK		(BLENDFACT_MASK<<0)

#define BLENDFUNC_ADD			0x0
#define BLENDFUNC_SUBTRACT		0x1
#define BLENDFUNC_REVERSE_SUBTRACT	0x2
#define BLENDFUNC_MIN			0x3
#define BLENDFUNC_MAX			0x4
#define BLENDFUNC_MASK			0x7

#define BLENDFACT_ZERO			0x01
#define BLENDFACT_ONE			0x02
#define BLENDFACT_SRC_COLR		0x03
#define BLENDFACT_INV_SRC_COLR 		0x04
#define BLENDFACT_SRC_ALPHA		0x05
#define BLENDFACT_INV_SRC_ALPHA 	0x06
#define BLENDFACT_DST_ALPHA		0x07
#define BLENDFACT_INV_DST_ALPHA 	0x08
#define BLENDFACT_DST_COLR		0x09
#define BLENDFACT_INV_DST_COLR		0x0a
#define BLENDFACT_SRC_ALPHA_SATURATE	0x0b
#define BLENDFACT_CONST_COLOR		0x0c
#define BLENDFACT_INV_CONST_COLOR	0x0d
#define BLENDFACT_CONST_ALPHA		0x0e
#define BLENDFACT_INV_CONST_ALPHA	0x0f
#define BLENDFACT_MASK          	0x0f

#define STATE3D_MODES_4			(CMD_3D | (0x0d<<24))
#define ENABLE_LOGIC_OP_FUNC		(1<<23)
#define LOGIC_OP_FUNC(x)		((x)<<18)
#define LOGICOP_MASK			(0xf<<18)
#define MODE4_ENABLE_STENCIL_TEST_MASK	((1<<17)|(0xff00))
#define ENABLE_STENCIL_TEST_MASK	(1<<17)
#define STENCIL_TEST_MASK(x)		((x)<<8)
#define MODE4_ENABLE_STENCIL_WRITE_MASK	((1<<16)|(0x00ff))
#define ENABLE_STENCIL_WRITE_MASK	(1<<16)
#define STENCIL_WRITE_MASK(x)		((x)&0xff)

#define LOGICOP_CLEAR			0
#define LOGICOP_NOR			0x1
#define LOGICOP_AND_INV 		0x2
#define LOGICOP_COPY_INV		0x3
#define LOGICOP_AND_RVRSE		0x4
#define LOGICOP_INV			0x5
#define LOGICOP_XOR			0x6
#define LOGICOP_NAND			0x7
#define LOGICOP_AND			0x8
#define LOGICOP_EQUIV			0x9
#define LOGICOP_NOOP			0xa
#define LOGICOP_OR_INV			0xb
#define LOGICOP_COPY			0xc
#define LOGICOP_OR_RVRSE		0xd
#define LOGICOP_OR			0xe
#define LOGICOP_SET			0xf

#define STATE3D_COORD_SET_BINDINGS	(CMD_3D | (0x16<<24))
#define CSB_TCB(iunit,eunit)		((eunit) << ((iunit) * 3))

#define STATE3D_SCISSOR_ENABLE		(CMD_3D | (0x1c<<24)|(0x10<<19))
#define ENABLE_SCISSOR_RECT		((1<<1) | 1)
#define DISABLE_SCISSOR_RECT		((1<<1) | 0)

#define STATE3D_MAP_STATE		(CMD_3D | (0x1d<<24)|(0x00<<16))

#define MS1_MAPMASK_SHIFT               0
#define MS1_MAPMASK_MASK                (0x8fff<<0)

#define MS2_UNTRUSTED_SURFACE           (1<<31)
#define MS2_ADDRESS_MASK                0xfffffffc
#define MS2_VERTICAL_LINE_STRIDE        (1<<1)
#define MS2_VERTICAL_OFFSET             (1<<1)

#define MS3_HEIGHT_SHIFT              21
#define MS3_WIDTH_SHIFT               10
#define MS3_PALETTE_SELECT            (1<<9)
#define MS3_MAPSURF_FORMAT_SHIFT      7
#define MS3_MAPSURF_FORMAT_MASK       (0x7<<7)
#define    MAPSURF_8BIT		 	   (1<<7)
#define    MAPSURF_16BIT		   (2<<7)
#define    MAPSURF_32BIT		   (3<<7)
#define    MAPSURF_422			   (5<<7)
#define    MAPSURF_COMPRESSED		   (6<<7)
#define    MAPSURF_4BIT_INDEXED		   (7<<7)
#define MS3_MT_FORMAT_MASK         (0x7 << 3)
#define MS3_MT_FORMAT_SHIFT        3
#define    MT_4BIT_IDX_ARGB8888	           (7<<3) /* SURFACE_4BIT_INDEXED */
#define    MT_8BIT_I8		           (0<<3) /* SURFACE_8BIT */
#define    MT_8BIT_L8		           (1<<3)
#define    MT_8BIT_A8		           (4<<3)
#define    MT_8BIT_MONO8	           (5<<3)
#define    MT_16BIT_RGB565 		   (0<<3) /* SURFACE_16BIT */
#define    MT_16BIT_ARGB1555		   (1<<3)
#define    MT_16BIT_ARGB4444		   (2<<3)
#define    MT_16BIT_AY88		   (3<<3)
#define    MT_16BIT_88DVDU	           (5<<3)
#define    MT_16BIT_BUMP_655LDVDU	   (6<<3)
#define    MT_16BIT_I16	                   (7<<3)
#define    MT_16BIT_L16	                   (8<<3)
#define    MT_16BIT_A16	                   (9<<3)
#define    MT_32BIT_ARGB8888		   (0<<3) /* SURFACE_32BIT */
#define    MT_32BIT_ABGR8888		   (1<<3)
#define    MT_32BIT_XRGB8888		   (2<<3)
#define    MT_32BIT_XBGR8888		   (3<<3)
#define    MT_32BIT_QWVU8888		   (4<<3)
#define    MT_32BIT_AXVU8888		   (5<<3)
#define    MT_32BIT_LXVU8888	           (6<<3)
#define    MT_32BIT_XLVU8888	           (7<<3)
#define    MT_32BIT_ARGB2101010	           (8<<3)
#define    MT_32BIT_ABGR2101010	           (9<<3)
#define    MT_32BIT_AWVU2101010	           (0xA<<3)
#define    MT_32BIT_GR1616	           (0xB<<3)
#define    MT_32BIT_VU1616	           (0xC<<3)
#define    MT_32BIT_xI824	           (0xD<<3)
#define    MT_32BIT_xA824	           (0xE<<3)
#define    MT_32BIT_xL824	           (0xF<<3)
#define    MT_422_YCRCB_SWAPY	           (0<<3) /* SURFACE_422 */
#define    MT_422_YCRCB_NORMAL	           (1<<3)
#define    MT_422_YCRCB_SWAPUV	           (2<<3)
#define    MT_422_YCRCB_SWAPUVY	           (3<<3)
#define    MT_COMPRESS_DXT1		   (0<<3) /* SURFACE_COMPRESSED */
#define    MT_COMPRESS_DXT2_3	           (1<<3)
#define    MT_COMPRESS_DXT4_5	           (2<<3)
#define    MT_COMPRESS_FXT1		   (3<<3)
#define    MT_COMPRESS_DXT1_RGB		   (4<<3)
#define MS3_USE_FENCE_REGS              (1<<2)
#define MS3_TILED_SURFACE             (1<<1)
#define MS3_TILE_WALK                 (1<<0)

#define MS4_PITCH_SHIFT                 21
#define MS4_CUBE_FACE_ENA_NEGX          (1<<20)
#define MS4_CUBE_FACE_ENA_POSX          (1<<19)
#define MS4_CUBE_FACE_ENA_NEGY          (1<<18)
#define MS4_CUBE_FACE_ENA_POSY          (1<<17)
#define MS4_CUBE_FACE_ENA_NEGZ          (1<<16)
#define MS4_CUBE_FACE_ENA_POSZ          (1<<15)
#define MS4_CUBE_FACE_ENA_MASK          (0x3f<<15)
#define MS4_MAX_LOD_SHIFT		9
#define MS4_MAX_LOD_MASK		(0x3f<<9)
#define MS4_MIP_LAYOUT_LEGACY           (0<<8)
#define MS4_MIP_LAYOUT_BELOW_LPT        (0<<8)
#define MS4_MIP_LAYOUT_RIGHT_LPT        (1<<8)
#define MS4_VOLUME_DEPTH_SHIFT          0    
#define MS4_VOLUME_DEPTH_MASK           (0xff<<0)

#define STATE3D_SAMPLER_STATE		(CMD_3D | (0x1d<<24)|(0x01<<16))

#define SS1_MAPMASK_SHIFT               0
#define SS1_MAPMASK_MASK                (0x8fff<<0)

#define SS2_REVERSE_GAMMA_ENABLE        (1<<31)
#define SS2_PLANAR_TO_PACKED_ENABLE     (1<<30)
#define SS2_COLORSPACE_CONVERSION       (1<<29)
#define SS2_CHROMAKEY_SHIFT             27
#define SS2_BASE_MIP_LEVEL_SHIFT        22
#define SS2_BASE_MIP_LEVEL_MASK         (0x1f<<22)
#define SS2_MIP_FILTER_SHIFT            20
#define SS2_MIP_FILTER_MASK             (0x3<<20)
#define   MIPFILTER_NONE       	0
#define   MIPFILTER_NEAREST	1
#define   MIPFILTER_LINEAR	3
#define SS2_MAG_FILTER_SHIFT          17
#define SS2_MAG_FILTER_MASK           (0x7<<17)
#define   FILTER_NEAREST	0
#define   FILTER_LINEAR		1
#define   FILTER_ANISOTROPIC	2
#define   FILTER_4X4_1    	3
#define   FILTER_4X4_2    	4
#define   FILTER_4X4_FLAT 	5
#define   FILTER_6X5_MONO   	6 /* XXX - check */
#define SS2_MIN_FILTER_SHIFT          14
#define SS2_MIN_FILTER_MASK           (0x7<<14)
#define SS2_LOD_BIAS_SHIFT            5
#define SS2_LOD_BIAS_ONE              (0x10<<5)
#define SS2_LOD_BIAS_MASK             (0x1ff<<5)
/* Shadow requires:
 *  MT_X8{I,L,A}24 or MT_{I,L,A}16 texture format
 *  FILTER_4X4_x  MIN and MAG filters
 */
#define SS2_SHADOW_ENABLE             (1<<4)
#define SS2_MAX_ANISO_MASK            (1<<3)
#define SS2_MAX_ANISO_2               (0<<3)
#define SS2_MAX_ANISO_4               (1<<3)
#define SS2_SHADOW_FUNC_SHIFT         0
#define SS2_SHADOW_FUNC_MASK          (0x7<<0)
/* SS2_SHADOW_FUNC values: see COMPAREFUNC_* */

#define SS3_MIN_LOD_SHIFT            24
#define SS3_MIN_LOD_ONE              (0x10<<24)
#define SS3_MIN_LOD_MASK             (0xff<<24)
#define SS3_KILL_PIXEL_ENABLE        (1<<17)
#define SS3_TCX_ADDR_MODE_SHIFT      12
#define SS3_TCX_ADDR_MODE_MASK       (0x7<<12)
#define   TEXCOORDMODE_WRAP		0
#define   TEXCOORDMODE_MIRROR		1
#define   TEXCOORDMODE_CLAMP_EDGE	2
#define   TEXCOORDMODE_CUBE       	3
#define   TEXCOORDMODE_CLAMP_BORDER	4
#define   TEXCOORDMODE_MIRROR_ONCE      5
#define SS3_TCY_ADDR_MODE_SHIFT      9
#define SS3_TCY_ADDR_MODE_MASK       (0x7<<9)
#define SS3_TCZ_ADDR_MODE_SHIFT      6
#define SS3_TCZ_ADDR_MODE_MASK       (0x7<<6)
#define SS3_NORMALIZED_COORDS        (1<<5)
#define SS3_TEXTUREMAP_INDEX_SHIFT   1
#define SS3_TEXTUREMAP_INDEX_MASK    (0xf<<1)
#define SS3_DEINTERLACER_ENABLE      (1<<0)

#define SS4_BORDER_COLOR_MASK        (~0)

#define STATE3D_LOAD_STATE_IMMEDIATE_1	(CMD_3D | (0x1d<<24)|(0x04<<16))
#define I1_LOAD_S(n)				(1 << (4 + n))

#define S0_VB_OFFSET_MASK              0xffffffc
#define S0_AUTO_CACHE_INV_DISABLE      (1<<0)

#define S1_VERTEX_WIDTH_SHIFT          24
#define S1_VERTEX_WIDTH_MASK           (0x3f<<24)
#define S1_VERTEX_PITCH_SHIFT          16
#define S1_VERTEX_PITCH_MASK           (0x3f<<16)

#define TEXCOORDFMT_2D                 0x0
#define TEXCOORDFMT_3D                 0x1
#define TEXCOORDFMT_4D                 0x2
#define TEXCOORDFMT_1D                 0x3
#define TEXCOORDFMT_2D_16              0x4
#define TEXCOORDFMT_4D_16              0x5
#define TEXCOORDFMT_NOT_PRESENT        0xf
#define S2_TEXCOORD_FMT0_MASK            0xf
#define S2_TEXCOORD_FMT1_SHIFT           4
#define S2_TEXCOORD_FMT(unit, type)    ((type)<<(unit*4))
#define S2_TEXCOORD_NONE               (~0)

/* S3 not interesting */

#define S4_POINT_WIDTH_SHIFT           23
#define S4_POINT_WIDTH_MASK            (0x1ff<<23)
#define S4_LINE_WIDTH_SHIFT            19
#define S4_LINE_WIDTH_ONE              (0x2<<19)
#define S4_LINE_WIDTH_MASK             (0xf<<19)
#define S4_FLATSHADE_ALPHA             (1<<18)
#define S4_FLATSHADE_FOG               (1<<17)
#define S4_FLATSHADE_SPECULAR          (1<<16)
#define S4_FLATSHADE_COLOR             (1<<15)
#define S4_CULLMODE_BOTH	       (0<<13)
#define S4_CULLMODE_NONE	       (1<<13)
#define S4_CULLMODE_CW		       (2<<13)
#define S4_CULLMODE_CCW		       (3<<13)
#define S4_CULLMODE_MASK	       (3<<13)
#define S4_VFMT_POINT_WIDTH            (1<<12)
#define S4_VFMT_SPEC_FOG               (1<<11)
#define S4_VFMT_COLOR                  (1<<10)
#define S4_VFMT_DEPTH_OFFSET           (1<<9)
#define S4_VFMT_XYZ     	       (1<<6)
#define S4_VFMT_XYZW     	       (2<<6)
#define S4_VFMT_XY     		       (3<<6)
#define S4_VFMT_XYW     	       (4<<6)
#define S4_VFMT_XYZW_MASK              (7<<6)
#define S4_FORCE_DEFAULT_DIFFUSE       (1<<5)
#define S4_FORCE_DEFAULT_SPECULAR      (1<<4)
#define S4_LOCAL_DEPTH_OFFSET_ENABLE   (1<<3)
#define S4_VFMT_FOG_PARAM              (1<<2)
#define S4_SPRITE_POINT_ENABLE         (1<<1)
#define S4_LINE_ANTIALIAS_ENABLE       (1<<0)

#define S4_VFMT_MASK (S4_VFMT_POINT_WIDTH   | 	\
		      S4_VFMT_SPEC_FOG      |	\
		      S4_VFMT_COLOR         |	\
		      S4_VFMT_DEPTH_OFFSET  |	\
		      S4_VFMT_XYZW_MASK     |	\
		      S4_VFMT_FOG_PARAM)


#define S5_WRITEDISABLE_ALPHA          (1<<31)
#define S5_WRITEDISABLE_RED            (1<<30)
#define S5_WRITEDISABLE_GREEN          (1<<29)
#define S5_WRITEDISABLE_BLUE           (1<<28)
#define S5_WRITEDISABLE_MASK           (0xf<<28)
#define S5_FORCE_DEFAULT_POINT_SIZE    (1<<27)
#define S5_LAST_PIXEL_ENABLE           (1<<26)
#define S5_GLOBAL_DEPTH_OFFSET_ENABLE  (1<<25)
#define S5_FOG_ENABLE                  (1<<24)
#define S5_STENCIL_REF_SHIFT           16
#define S5_STENCIL_REF_MASK            (0xff<<16)
#define S5_STENCIL_TEST_FUNC_SHIFT     13
#define S5_STENCIL_TEST_FUNC_MASK      (0x7<<13)
#define S5_STENCIL_FAIL_SHIFT          10
#define S5_STENCIL_FAIL_MASK           (0x7<<10)
#define S5_STENCIL_PASS_Z_FAIL_SHIFT   7
#define S5_STENCIL_PASS_Z_FAIL_MASK    (0x7<<7)
#define S5_STENCIL_PASS_Z_PASS_SHIFT   4
#define S5_STENCIL_PASS_Z_PASS_MASK    (0x7<<4)
#define S5_STENCIL_WRITE_ENABLE        (1<<3)
#define S5_STENCIL_TEST_ENABLE         (1<<2)
#define S5_COLOR_DITHER_ENABLE         (1<<1)
#define S5_LOGICOP_ENABLE              (1<<0)


#define S6_ALPHA_TEST_ENABLE           (1<<31)
#define S6_ALPHA_TEST_FUNC_SHIFT       28
#define S6_ALPHA_TEST_FUNC_MASK        (0x7<<28)
#define S6_ALPHA_REF_SHIFT             20
#define S6_ALPHA_REF_MASK              (0xff<<20)
#define S6_DEPTH_TEST_ENABLE           (1<<19)
#define S6_DEPTH_TEST_FUNC_SHIFT       16
#define S6_DEPTH_TEST_FUNC_MASK        (0x7<<16)
#define S6_CBUF_BLEND_ENABLE           (1<<15)
#define S6_CBUF_BLEND_FUNC_SHIFT       12
#define S6_CBUF_BLEND_FUNC_MASK        (0x7<<12)
#define S6_CBUF_SRC_BLEND_FACT_SHIFT   8
#define S6_CBUF_SRC_BLEND_FACT_MASK    (0xf<<8)
#define S6_CBUF_DST_BLEND_FACT_SHIFT   4
#define S6_CBUF_DST_BLEND_FACT_MASK    (0xf<<4)
#define S6_DEPTH_WRITE_ENABLE          (1<<3)
#define S6_COLOR_WRITE_ENABLE          (1<<2)
#define S6_TRISTRIP_PV_SHIFT           0
#define S6_TRISTRIP_PV_MASK            (0x3<<0)

#define S7_DEPTH_OFFSET_CONST_MASK     ~0

#define STATE3D_PIXEL_SHADER_PROGRAM	(CMD_3D | (0x1d<<24)|(0x05<<16))

#define REG_TYPE_R                 0 /* temporary regs, no need to
				      * dcl, must be written before
				      * read -- Preserved between
				      * phases. 
				      */
#define REG_TYPE_T                 1 /* Interpolated values, must be
				      * dcl'ed before use.
				      *
				      * 0..7: texture coord,
				      * 8: diffuse spec,
				      * 9: specular color,
				      * 10: fog parameter in w.
				      */
#define REG_TYPE_CONST             2 /* Restriction: only one const
				      * can be referenced per
				      * instruction, though it may be
				      * selected for multiple inputs.
				      * Constants not initialized
				      * default to zero.
				      */
#define REG_TYPE_S                 3 /* sampler */
#define REG_TYPE_OC                4 /* output color (rgba) */
#define REG_TYPE_OD                5 /* output depth (w), xyz are
				      * temporaries.  If not written,
				      * interpolated depth is used?
				      */
#define REG_TYPE_U                 6 /* unpreserved temporaries */
#define REG_TYPE_MASK              0x7
#define REG_NR_MASK                0xf


/* REG_TYPE_T:
 */
#define T_TEX0     0
#define T_TEX1     1
#define T_TEX2     2
#define T_TEX3     3
#define T_TEX4     4
#define T_TEX5     5
#define T_TEX6     6
#define T_TEX7     7
#define T_DIFFUSE  8
#define T_SPECULAR 9
#define T_FOG_W    10		/* interpolated fog is in W coord */

/* Arithmetic instructions */

/* .replicate_swizzle == selection and replication of a particular
 * scalar channel, ie., .xxxx, .yyyy, .zzzz or .wwww 
 */
#define A0_NOP    (0x0<<24)		/* no operation */
#define A0_ADD    (0x1<<24)		/* dst = src0 + src1 */
#define A0_MOV    (0x2<<24)		/* dst = src0 */
#define A0_MUL    (0x3<<24)		/* dst = src0 * src1 */
#define A0_MAD    (0x4<<24)		/* dst = src0 * src1 + src2 */
#define A0_DP2ADD (0x5<<24)		/* dst.xyzw = src0.xy dot src1.xy + src2.replicate_swizzle */
#define A0_DP3    (0x6<<24)		/* dst.xyzw = src0.xyz dot src1.xyz */
#define A0_DP4    (0x7<<24)		/* dst.xyzw = src0.xyzw dot src1.xyzw */
#define A0_FRC    (0x8<<24)		/* dst = src0 - floor(src0) */
#define A0_RCP    (0x9<<24)		/* dst.xyzw = 1/(src0.replicate_swizzle) */
#define A0_RSQ    (0xa<<24)		/* dst.xyzw = 1/(sqrt(abs(src0.replicate_swizzle))) */
#define A0_EXP    (0xb<<24)		/* dst.xyzw = exp2(src0.replicate_swizzle) */
#define A0_LOG    (0xc<<24)		/* dst.xyzw = log2(abs(src0.replicate_swizzle)) */
#define A0_CMP    (0xd<<24)		/* dst = (src0 >= 0.0) ? src1 : src2 */
#define A0_MIN    (0xe<<24)		/* dst = (src0 < src1) ? src0 : src1 */
#define A0_MAX    (0xf<<24)		/* dst = (src0 >= src1) ? src0 : src1 */
#define A0_FLR    (0x10<<24)		/* dst = floor(src0) */
#define A0_MOD    (0x11<<24)		/* dst = src0 fmod 1.0 */
#define A0_TRC    (0x12<<24)		/* dst = int(src0) */
#define A0_SGE    (0x13<<24)		/* dst = src0 >= src1 ? 1.0 : 0.0 */
#define A0_SLT    (0x14<<24)		/* dst = src0 < src1 ? 1.0 : 0.0 */
#define A0_DEST_SATURATE                 (1<<22)
#define A0_DEST_TYPE_SHIFT                19
/* Allow: R, OC, OD, U */
#define A0_DEST_NR_SHIFT                 14
/* Allow R: 0..15, OC,OD: 0..0, U: 0..2 */
#define A0_DEST_CHANNEL_X                (1<<10)
#define A0_DEST_CHANNEL_Y                (2<<10)
#define A0_DEST_CHANNEL_Z                (4<<10)
#define A0_DEST_CHANNEL_W                (8<<10)
#define A0_DEST_CHANNEL_ALL              (0xf<<10)
#define A0_DEST_CHANNEL_SHIFT            10
#define A0_SRC0_TYPE_SHIFT               7
#define A0_SRC0_NR_SHIFT                 2

#define A0_DEST_CHANNEL_XY              (A0_DEST_CHANNEL_X|A0_DEST_CHANNEL_Y)
#define A0_DEST_CHANNEL_XYZ             (A0_DEST_CHANNEL_XY|A0_DEST_CHANNEL_Z)


#define SRC_X        0
#define SRC_Y        1
#define SRC_Z        2
#define SRC_W        3
#define SRC_ZERO     4
#define SRC_ONE      5

#define A1_SRC0_CHANNEL_X_NEGATE         (1<<31)
#define A1_SRC0_CHANNEL_X_SHIFT          28
#define A1_SRC0_CHANNEL_Y_NEGATE         (1<<27)
#define A1_SRC0_CHANNEL_Y_SHIFT          24
#define A1_SRC0_CHANNEL_Z_NEGATE         (1<<23)
#define A1_SRC0_CHANNEL_Z_SHIFT          20
#define A1_SRC0_CHANNEL_W_NEGATE         (1<<19)
#define A1_SRC0_CHANNEL_W_SHIFT          16
#define A1_SRC1_TYPE_SHIFT               13
#define A1_SRC1_NR_SHIFT                 8
#define A1_SRC1_CHANNEL_X_NEGATE         (1<<7)
#define A1_SRC1_CHANNEL_X_SHIFT          4
#define A1_SRC1_CHANNEL_Y_NEGATE         (1<<3)
#define A1_SRC1_CHANNEL_Y_SHIFT          0

#define A2_SRC1_CHANNEL_Z_NEGATE         (1<<31)
#define A2_SRC1_CHANNEL_Z_SHIFT          28
#define A2_SRC1_CHANNEL_W_NEGATE         (1<<27)
#define A2_SRC1_CHANNEL_W_SHIFT          24
#define A2_SRC2_TYPE_SHIFT               21
#define A2_SRC2_NR_SHIFT                 16
#define A2_SRC2_CHANNEL_X_NEGATE         (1<<15)
#define A2_SRC2_CHANNEL_X_SHIFT          12
#define A2_SRC2_CHANNEL_Y_NEGATE         (1<<11)
#define A2_SRC2_CHANNEL_Y_SHIFT          8
#define A2_SRC2_CHANNEL_Z_NEGATE         (1<<7)
#define A2_SRC2_CHANNEL_Z_SHIFT          4
#define A2_SRC2_CHANNEL_W_NEGATE         (1<<3)
#define A2_SRC2_CHANNEL_W_SHIFT          0



/* Texture instructions */
#define T0_TEXLD     (0x15<<24)	/* Sample texture using predeclared
				 * sampler and address, and output
				 * filtered texel data to destination
				 * register */
#define T0_TEXLDP    (0x16<<24)	/* Same as texld but performs a
				 * perspective divide of the texture
				 * coordinate .xyz values by .w before
				 * sampling. */
#define T0_TEXLDB    (0x17<<24)	/* Same as texld but biases the
				 * computed LOD by w.  Only S4.6 two's
				 * comp is used.  This implies that a
				 * float to fixed conversion is
				 * done. */
#define T0_TEXKILL   (0x18<<24)	/* Does not perform a sampling
				 * operation.  Simply kills the pixel
				 * if any channel of the address
				 * register is < 0.0. */
#define T0_DEST_TYPE_SHIFT                19
/* Allow: R, OC, OD, U */
/* Note: U (unpreserved) regs do not retain their values between
 * phases (cannot be used for feedback) 
 *
 * Note: oC and OD registers can only be used as the destination of a
 * texture instruction once per phase (this is an implementation
 * restriction). 
 */
#define T0_DEST_NR_SHIFT                 14
/* Allow R: 0..15, OC,OD: 0..0, U: 0..2 */
#define T0_SAMPLER_NR_SHIFT              0 /* This field ignored for TEXKILL */
#define T0_SAMPLER_NR_MASK               (0xf<<0)

#define T1_ADDRESS_REG_TYPE_SHIFT        24 /* Reg to use as texture coord */
/* Allow R, T, OC, OD -- R, OC, OD are 'dependent' reads, new program phase */
#define T1_ADDRESS_REG_NR_SHIFT          17
#define T2_MBZ                           0

/* Declaration instructions */
#define D0_DCL       (0x19<<24)	/* Declare a t (interpolated attrib)
				 * register or an s (sampler)
				 * register. */
#define D0_SAMPLE_TYPE_SHIFT              22
#define D0_SAMPLE_TYPE_2D                 (0x0<<22)
#define D0_SAMPLE_TYPE_CUBE               (0x1<<22)
#define D0_SAMPLE_TYPE_VOLUME             (0x2<<22)
#define D0_SAMPLE_TYPE_MASK               (0x3<<22)

#define D0_TYPE_SHIFT                19
/* Allow: T, S */
#define D0_NR_SHIFT                  14
/* Allow T: 0..10, S: 0..15 */
#define D0_CHANNEL_X                (1<<10)
#define D0_CHANNEL_Y                (2<<10)
#define D0_CHANNEL_Z                (4<<10)
#define D0_CHANNEL_W                (8<<10)
#define D0_CHANNEL_ALL              (0xf<<10)
#define D0_CHANNEL_NONE             (0<<10)

#define D0_CHANNEL_XY               (D0_CHANNEL_X|D0_CHANNEL_Y)
#define D0_CHANNEL_XYZ              (D0_CHANNEL_XY|D0_CHANNEL_Z)
/* End description of STATE3D_PIXEL_SHADER_PROGRAM */

#define STATE3D_PIXEL_SHADER_CONSTANTS	(CMD_3D | (0x1d<<24)|(0x06<<16))

#define STATE3D_DRAWING_RECTANGLE	(CMD_3D | (0x1d<<24)|(0x80<<16)|3)

#define STATE3D_SCISSOR_RECTANGLE	(CMD_3D | (0x1d<<24)|(0x81<<16)|1)

#define STATE3D_STIPPLE			(CMD_3D | (0x1d<<24)|(0x83<<16))
#define ST1_ENABLE               (1<<16)
#define ST1_MASK                 (0xffff)

#define STATE3D_DEST_BUFFER_VARIABLES	(CMD_3D | (0x1d<<24)|(0x85<<16))
#define TEX_DEFAULT_COLOR_OGL           (0<<30)
#define TEX_DEFAULT_COLOR_D3D           (1<<30)
#define ZR_EARLY_DEPTH                  (1<<29)
#define LOD_PRECLAMP_OGL                (1<<28)
#define LOD_PRECLAMP_D3D                (0<<28)
#define DITHER_FULL_ALWAYS              (0<<26)
#define DITHER_FULL_ON_FB_BLEND         (1<<26)
#define DITHER_CLAMPED_ALWAYS           (2<<26)
#define LINEAR_GAMMA_BLEND_32BPP        (1<<25)
#define DEBUG_DISABLE_ENH_DITHER        (1<<24)
#define DSTORG_HORIZ_BIAS(x)		((x)<<20)
#define DSTORG_VERT_BIAS(x)		((x)<<16)
#define COLOR_4_2_2_CHNL_WRT_ALL	0
#define COLOR_4_2_2_CHNL_WRT_Y		(1<<12)
#define COLOR_4_2_2_CHNL_WRT_CR		(2<<12)
#define COLOR_4_2_2_CHNL_WRT_CB		(3<<12)
#define COLOR_4_2_2_CHNL_WRT_CRCB	(4<<12)
#define COLR_BUF_8BIT			0
#define COLR_BUF_RGB555 		(1<<8)
#define COLR_BUF_RGB565 		(2<<8)
#define COLR_BUF_ARGB8888		(3<<8)
#define DEPTH_FRMT_16_FIXED		0
#define DEPTH_FRMT_16_FLOAT		(1<<2)
#define DEPTH_FRMT_24_FIXED_8_OTHER	(2<<2)
#define VERT_LINE_STRIDE_1		(1<<1)
#define VERT_LINE_STRIDE_0		(0<<1)
#define VERT_LINE_STRIDE_OFS_1		1
#define VERT_LINE_STRIDE_OFS_0		0

#define STATE3D_CONST_BLEND_COLOR	(CMD_3D | (0x1d<<24)|(0x88<<16))

#define STATE3D_FOG_MODE		((3<<29)|(0x1d<<24)|(0x89<<16)|2)
#define FOG_MODE_VERTEX 		(1<<31)
#define STATE3D_MAP_COORD_TRANSFORM	((3<<29)|(0x1d<<24)|(0x8c<<16))

#define STATE3D_BUFFER_INFO		(CMD_3D | (0x1d<<24)|(0x8e<<16)|1)
#define BUFFERID_COLOR_BACK		(3 << 24)
#define BUFFERID_COLOR_AUX		(4 << 24)
#define BUFFERID_MC_INTRA_CORR		(5 << 24)
#define BUFFERID_DEPTH			(7 << 24)
#define BUFFER_USE_FENCES		(1 << 23)

#define STATE3D_DFLT_Z_CMD		(CMD_3D | (0x1d<<24)|(0x98<<16))

#define STATE3D_DFLT_DIFFUSE_CMD	(CMD_3D | (0x1d<<24)|(0x99<<16))

#define STATE3D_DFLT_SPEC_CMD		(CMD_3D | (0x1d<<24)|(0x9a<<16))

#define PRIMITIVE3D			(CMD_3D | (0x1f<<24))
#define PRIM3D_INLINE		(0<<23)
#define PRIM3D_INDIRECT		(1<<23)
#define PRIM3D_TRILIST		(0x0<<18)
#define PRIM3D_TRISTRIP 	(0x1<<18)
#define PRIM3D_TRISTRIP_RVRSE	(0x2<<18)
#define PRIM3D_TRIFAN		(0x3<<18)
#define PRIM3D_POLY		(0x4<<18)
#define PRIM3D_LINELIST 	(0x5<<18)
#define PRIM3D_LINESTRIP	(0x6<<18)
#define PRIM3D_RECTLIST 	(0x7<<18)
#define PRIM3D_POINTLIST	(0x8<<18)
#define PRIM3D_DIB		(0x9<<18)
#define PRIM3D_CLEAR_RECT	(0xa<<18)
#define PRIM3D_ZONE_INIT	(0xd<<18)
#define PRIM3D_MASK		(0x1f<<18)


#define DISABLE_TEX_TRANSFORM		(1<<28)
#define TEXTURE_SET(x)			(x<<29)

#define STATE3D_VERTEX_TRANSFORM	((3<<29)|(0x1d<<24)|(0x8b<<16))
#define DISABLE_VIEWPORT_TRANSFORM	(1<<31)
#define DISABLE_PERSPECTIVE_DIVIDE	(1<<29)

#define MI_SET_CONTEXT			(0x18<<23)
#define CTXT_NO_RESTORE 		(1)
#define CTXT_PALETTE_SAVE_DISABLE	(1<<3)
#define CTXT_PALETTE_RESTORE_DISABLE	(1<<2)

/* Dword 0 */
#define MI_VERTEX_BUFFER		(0x17<<23)
#define MI_VERTEX_BUFFER_IDX(x) 	(x<<20)
#define MI_VERTEX_BUFFER_PITCH(x)	(x<<13)
#define MI_VERTEX_BUFFER_WIDTH(x)	(x<<6)
/* Dword 1 */
#define MI_VERTEX_BUFFER_DISABLE	(1)

/* Overlay Flip */
#define MI_OVERLAY_FLIP			(0x11<<23)
#define MI_OVERLAY_FLIP_CONTINUE	(0<<21)
#define MI_OVERLAY_FLIP_ON		(1<<21)
#define MI_OVERLAY_FLIP_OFF		(2<<21)

/* Wait for Events */
#define MI_WAIT_FOR_EVENT		(0x03<<23)
#define MI_WAIT_FOR_OVERLAY_FLIP	(1<<16)

/* Flush */
#define MI_FLUSH			(0x04<<23)
#define MI_WRITE_DIRTY_STATE		(1<<4)
#define MI_END_SCENE			(1<<3)
#define MI_INHIBIT_RENDER_CACHE_FLUSH	(1<<2)
#define MI_STATE_INSTRUCTION_CACHE_FLUSH (1<<1)
#define MI_INVALIDATE_MAP_CACHE		(1<<0)
/* broadwater flush bits */
#define BRW_MI_GLOBAL_SNAPSHOT_RESET   (1 << 3)

/* Noop */
#define MI_NOOP				0x00
#define MI_NOOP_WRITE_ID		(1<<22)
#define MI_NOOP_ID_MASK			(1<<22 - 1)

#define STATE3D_COLOR_FACTOR	((0x3<<29)|(0x1d<<24)|(0x01<<16))

/* STATE3D_FOG_MODE stuff */
#define ENABLE_FOG_SOURCE	(1<<27)
#define ENABLE_FOG_CONST	(1<<24)
#define ENABLE_FOG_DENSITY	(1<<23)

/*
 * New regs for broadwater -- we need to split this file up sensibly somehow.
 */
#define BRW_3D(Pipeline,Opcode,Subopcode) (CMD_3D | \
					   ((Pipeline) << 27) | \
					   ((Opcode) << 24) | \
					   ((Subopcode) << 16))

#define BRW_URB_FENCE				BRW_3D(0, 0, 0)
#define BRW_CS_URB_STATE			BRW_3D(0, 0, 1)
#define BRW_CONSTANT_BUFFER			BRW_3D(0, 0, 2)
#define BRW_STATE_PREFETCH			BRW_3D(0, 0, 3)

#define BRW_STATE_BASE_ADDRESS			BRW_3D(0, 1, 1)
#define BRW_STATE_SIP				BRW_3D(0, 1, 2)
#define BRW_PIPELINE_SELECT			BRW_3D(0, 1, 4)

#define BRW_MEDIA_STATE_POINTERS		BRW_3D(2, 0, 0)
#define BRW_MEDIA_OBJECT			BRW_3D(2, 1, 0)

#define BRW_3DSTATE_PIPELINED_POINTERS		BRW_3D(3, 0, 0)
#define BRW_3DSTATE_BINDING_TABLE_POINTERS	BRW_3D(3, 0, 1)
#define BRW_3DSTATE_VERTEX_BUFFERS		BRW_3D(3, 0, 8)
#define BRW_3DSTATE_VERTEX_ELEMENTS		BRW_3D(3, 0, 9)
#define BRW_3DSTATE_INDEX_BUFFER		BRW_3D(3, 0, 0xa)
#define BRW_3DSTATE_VF_STATISTICS		BRW_3D(3, 0, 0xb)

#define BRW_3DSTATE_DRAWING_RECTANGLE		BRW_3D(3, 1, 0)
#define BRW_3DSTATE_CONSTANT_COLOR		BRW_3D(3, 1, 1)
#define BRW_3DSTATE_SAMPLER_PALETTE_LOAD	BRW_3D(3, 1, 2)
#define BRW_3DSTATE_CHROMA_KEY			BRW_3D(3, 1, 4)
#define BRW_3DSTATE_DEPTH_BUFFER		BRW_3D(3, 1, 5)
#define BRW_3DSTATE_POLY_STIPPLE_OFFSET		BRW_3D(3, 1, 6)
#define BRW_3DSTATE_POLY_STIPPLE_PATTERN	BRW_3D(3, 1, 7)
#define BRW_3DSTATE_LINE_STIPPLE		BRW_3D(3, 1, 8)
#define BRW_3DSTATE_GLOBAL_DEPTH_OFFSET_CLAMP	BRW_3D(3, 1, 9)
/* These two are BLC and CTG only, not BW or CL */
#define BRW_3DSTATE_AA_LINE_PARAMS		BRW_3D(3, 1, 0xa)
#define BRW_3DSTATE_GS_SVB_INDEX		BRW_3D(3, 1, 0xb)

#define BRW_PIPE_CONTROL			BRW_3D(3, 2, 0)

#define BRW_3DPRIMITIVE				BRW_3D(3, 3, 0)

#define PIPELINE_SELECT_3D		0
#define PIPELINE_SELECT_MEDIA		1

#define UF0_CS_REALLOC			(1 << 13)
#define UF0_VFE_REALLOC			(1 << 12)
#define UF0_SF_REALLOC			(1 << 11)
#define UF0_CLIP_REALLOC		(1 << 10)
#define UF0_GS_REALLOC			(1 << 9)
#define UF0_VS_REALLOC			(1 << 8)
#define UF1_CLIP_FENCE_SHIFT		20
#define UF1_GS_FENCE_SHIFT		10
#define UF1_VS_FENCE_SHIFT		0
#define UF2_CS_FENCE_SHIFT		20
#define UF2_VFE_FENCE_SHIFT		10
#define UF2_SF_FENCE_SHIFT		0

/* for BRW_STATE_BASE_ADDRESS */
#define BASE_ADDRESS_MODIFY		(1 << 0)

/* for BRW_3DSTATE_PIPELINED_POINTERS */
#define BRW_GS_DISABLE		       0
#define BRW_GS_ENABLE		       1
#define BRW_CLIP_DISABLE	       0
#define BRW_CLIP_ENABLE		       1

/* for BRW_PIPE_CONTROL */
#define BRW_PIPE_CONTROL_NOWRITE       (0 << 14)
#define BRW_PIPE_CONTROL_WRITE_QWORD   (1 << 14)
#define BRW_PIPE_CONTROL_WRITE_DEPTH   (2 << 14)
#define BRW_PIPE_CONTROL_WRITE_TIME    (3 << 14)
#define BRW_PIPE_CONTROL_DEPTH_STALL   (1 << 13)
#define BRW_PIPE_CONTROL_WC_FLUSH      (1 << 12)
#define BRW_PIPE_CONTROL_IS_FLUSH      (1 << 11)
#define BRW_PIPE_CONTROL_NOTIFY_ENABLE (1 << 8)
#define BRW_PIPE_CONTROL_GLOBAL_GTT    (1 << 2)
#define BRW_PIPE_CONTROL_LOCAL_PGTT    (0 << 2)

/* VERTEX_BUFFER_STATE Structure */
#define VB0_BUFFER_INDEX_SHIFT		27
#define VB0_VERTEXDATA			(0 << 26)
#define VB0_INSTANCEDATA		(1 << 26)
#define VB0_BUFFER_PITCH_SHIFT		0

/* VERTEX_ELEMENT_STATE Structure */
#define VE0_VERTEX_BUFFER_INDEX_SHIFT	27
#define VE0_VALID			(1 << 26)
#define VE0_FORMAT_SHIFT		16
#define VE0_OFFSET_SHIFT		0
#define VE1_VFCOMPONENT_0_SHIFT		28
#define VE1_VFCOMPONENT_1_SHIFT		24
#define VE1_VFCOMPONENT_2_SHIFT		20
#define VE1_VFCOMPONENT_3_SHIFT		16
#define VE1_DESTINATION_ELEMENT_OFFSET_SHIFT	0

/* 3DPRIMITIVE bits */
#define BRW_3DPRIMITIVE_VERTEX_SEQUENTIAL (0 << 15)
#define BRW_3DPRIMITIVE_VERTEX_RANDOM	  (1 << 15)
/* Primitive types are in brw_defines.h */
#define BRW_3DPRIMITIVE_TOPOLOGY_SHIFT	  10

#define BRW_SVG_CTL		       0x7400

#define BRW_SVG_CTL_GS_BA	       (0 << 8)
#define BRW_SVG_CTL_SS_BA	       (1 << 8)
#define BRW_SVG_CTL_IO_BA	       (2 << 8)
#define BRW_SVG_CTL_GS_AUB	       (3 << 8)
#define BRW_SVG_CTL_IO_AUB	       (4 << 8)
#define BRW_SVG_CTL_SIP		       (5 << 8)

#define BRW_SVG_RDATA		       0x7404
#define BRW_SVG_WORK_CTL	       0x7408

#define BRW_VF_CTL		       0x7500

#define BRW_VF_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_VF_CTL_SNAPSHOT_MUX_SELECT_THREADID	   (0 << 8)
#define BRW_VF_CTL_SNAPSHOT_MUX_SELECT_VF_DEBUG	   (1 << 8)
#define BRW_VF_CTL_SNAPSHOT_TYPE_VERTEX_SEQUENCE   (0 << 4)
#define BRW_VF_CTL_SNAPSHOT_TYPE_VERTEX_INDEX	   (1 << 4)
#define BRW_VF_CTL_SKIP_INITIAL_PRIMITIVES	   (1 << 3)
#define BRW_VF_CTL_MAX_PRIMITIVES_LIMIT_ENABLE	   (1 << 2)
#define BRW_VF_CTL_VERTEX_RANGE_LIMIT_ENABLE	   (1 << 1)
#define BRW_VF_CTL_SNAPSHOT_ENABLE	     	   (1 << 0)

#define BRW_VF_STRG_VAL		       0x7504
#define BRW_VF_STR_VL_OVR	       0x7508
#define BRW_VF_VC_OVR		       0x750c
#define BRW_VF_STR_PSKIP	       0x7510
#define BRW_VF_MAX_PRIM		       0x7514
#define BRW_VF_RDATA		       0x7518

#define BRW_VS_CTL		       0x7600
#define BRW_VS_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_VS_CTL_SNAPSHOT_MUX_VERTEX_0	   (0 << 8)
#define BRW_VS_CTL_SNAPSHOT_MUX_VERTEX_1	   (1 << 8)
#define BRW_VS_CTL_SNAPSHOT_MUX_VALID_COUNT	   (2 << 8)
#define BRW_VS_CTL_SNAPSHOT_MUX_VS_KERNEL_POINTER  (3 << 8)
#define BRW_VS_CTL_SNAPSHOT_ALL_THREADS		   (1 << 2)
#define BRW_VS_CTL_THREAD_SNAPSHOT_ENABLE	   (1 << 1)
#define BRW_VS_CTL_SNAPSHOT_ENABLE		   (1 << 0)

#define BRW_VS_STRG_VAL		       0x7604
#define BRW_VS_RDATA		       0x7608

#define BRW_SF_CTL		       0x7b00
#define BRW_SF_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_0_FF_ID	   (0 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_0_REL_COUNT (1 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_1_FF_ID	   (2 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_1_REL_COUNT (3 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_2_FF_ID	   (4 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_2_REL_COUNT (5 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_VERTEX_COUNT	   (6 << 8)
#define BRW_SF_CTL_SNAPSHOT_MUX_SF_KERNEL_POINTER  (7 << 8)
#define BRW_SF_CTL_MIN_MAX_PRIMITIVE_RANGE_ENABLE  (1 << 4)
#define BRW_SF_CTL_DEBUG_CLIP_RECTANGLE_ENABLE	   (1 << 3)
#define BRW_SF_CTL_SNAPSHOT_ALL_THREADS		   (1 << 2)
#define BRW_SF_CTL_THREAD_SNAPSHOT_ENABLE	   (1 << 1)
#define BRW_SF_CTL_SNAPSHOT_ENABLE		   (1 << 0)

#define BRW_SF_STRG_VAL		       0x7b04
#define BRW_SF_RDATA		       0x7b18

#define BRW_WIZ_CTL		       0x7c00
#define BRW_WIZ_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_WIZ_CTL_SUBSPAN_INSTANCE_SHIFT	   16
#define BRW_WIZ_CTL_SNAPSHOT_MUX_WIZ_KERNEL_POINTER   (0 << 8)
#define BRW_WIZ_CTL_SNAPSHOT_MUX_SUBSPAN_INSTANCE     (1 << 8)
#define BRW_WIZ_CTL_SNAPSHOT_MUX_PRIMITIVE_SEQUENCE   (2 << 8)
#define BRW_WIZ_CTL_SINGLE_SUBSPAN_DISPATCH	      (1 << 6)
#define BRW_WIZ_CTL_IGNORE_COLOR_SCOREBOARD_STALLS    (1 << 5)
#define BRW_WIZ_CTL_ENABLE_SUBSPAN_INSTANCE_COMPARE   (1 << 4)
#define BRW_WIZ_CTL_USE_UPSTREAM_SNAPSHOT_FLAG	      (1 << 3)
#define BRW_WIZ_CTL_SNAPSHOT_ALL_THREADS	      (1 << 2)
#define BRW_WIZ_CTL_THREAD_SNAPSHOT_ENABLE	      (1 << 1)
#define BRW_WIZ_CTL_SNAPSHOT_ENABLE		      (1 << 0)

#define BRW_WIZ_STRG_VAL			      0x7c04
#define BRW_WIZ_RDATA				      0x7c18

#define BRW_TS_CTL		       0x7e00
#define BRW_TS_CTL_SNAPSHOT_COMPLETE		   (1 << 31)
#define BRW_TS_CTL_SNAPSHOT_MESSAGE_ERROR	   (0 << 8)
#define BRW_TS_CTL_SNAPSHOT_INTERFACE_DESCRIPTOR   (3 << 8)
#define BRW_TS_CTL_SNAPSHOT_ALL_CHILD_THREADS	   (1 << 2)
#define BRW_TS_CTL_SNAPSHOT_ALL_ROOT_THREADS  	   (1 << 1)
#define BRW_TS_CTL_SNAPSHOT_ENABLE		   (1 << 0)

#define BRW_TS_STRG_VAL		       0x7e04
#define BRW_TS_RDATA		       0x7e08

#define BRW_TD_CTL		       0x8000
#define BRW_TD_CTL_MUX_SHIFT	       8
#define BRW_TD_CTL_EXTERNAL_HALT_R0_DEBUG_MATCH	   (1 << 7)
#define BRW_TD_CTL_FORCE_EXTERNAL_HALT		   (1 << 6)
#define BRW_TD_CTL_EXCEPTION_MASK_OVERRIDE	   (1 << 5)
#define BRW_TD_CTL_FORCE_THREAD_BREAKPOINT_ENABLE  (1 << 4)
#define BRW_TD_CTL_BREAKPOINT_ENABLE		   (1 << 2)
#define BRW_TD_CTL2		       0x8004
#define BRW_TD_CTL2_ILLEGAL_OPCODE_EXCEPTION_OVERRIDE (1 << 28)
#define BRW_TD_CTL2_MASKSTACK_EXCEPTION_OVERRIDE      (1 << 26)
#define BRW_TD_CTL2_SOFTWARE_EXCEPTION_OVERRIDE	      (1 << 25)
#define BRW_TD_CTL2_ACTIVE_THREAD_LIMIT_SHIFT	      16
#define BRW_TD_CTL2_ACTIVE_THREAD_LIMIT_ENABLE	      (1 << 8)
#define BRW_TD_CTL2_THREAD_SPAWNER_EXECUTION_MASK_ENABLE (1 << 7)
#define BRW_TD_CTL2_WIZ_EXECUTION_MASK_ENABLE	      (1 << 6)
#define BRW_TD_CTL2_SF_EXECUTION_MASK_ENABLE	      (1 << 5)
#define BRW_TD_CTL2_CLIPPER_EXECUTION_MASK_ENABLE     (1 << 4)
#define BRW_TD_CTL2_GS_EXECUTION_MASK_ENABLE	      (1 << 3)
#define BRW_TD_CTL2_VS_EXECUTION_MASK_ENABLE	      (1 << 0)
#define BRW_TD_VF_VS_EMSK	       0x8008
#define BRW_TD_GS_EMSK		       0x800c
#define BRW_TD_CLIP_EMSK	       0x8010
#define BRW_TD_SF_EMSK		       0x8014
#define BRW_TD_WIZ_EMSK		       0x8018
#define BRW_TD_0_6_EHTRG_VAL	       0x801c
#define BRW_TD_0_7_EHTRG_VAL	       0x8020
#define BRW_TD_0_6_EHTRG_MSK           0x8024
#define BRW_TD_0_7_EHTRG_MSK	       0x8028
#define BRW_TD_RDATA		       0x802c
#define BRW_TD_TS_EMSK		       0x8030

#define BRW_EU_CTL		       0x8800
#define BRW_EU_CTL_SELECT_SHIFT	       16
#define BRW_EU_CTL_DATA_MUX_SHIFT      8
#define BRW_EU_ATT_0		       0x8810
#define BRW_EU_ATT_1		       0x8814
#define BRW_EU_ATT_DATA_0	       0x8820
#define BRW_EU_ATT_DATA_1	       0x8824
#define BRW_EU_ATT_CLR_0	       0x8830
#define BRW_EU_ATT_CLR_1	       0x8834
#define BRW_EU_RDATA		       0x8840

/* End regs for broadwater */

#define MAX_DISPLAY_PIPES	2

typedef enum {
   CrtIndex = 0,
   TvIndex,
   DfpIndex,
   LfpIndex,
   Tv2Index,
   Dfp2Index,
   UnknownIndex,
   Unknown2Index,
   NumDisplayTypes,
   NumKnownDisplayTypes = UnknownIndex
} DisplayType;

/* What's connected to the pipes (as reported by the BIOS) */
#define PIPE_ACTIVE_MASK		0xff
#define PIPE_CRT_ACTIVE			(1 << CrtIndex)
#define PIPE_TV_ACTIVE			(1 << TvIndex)
#define PIPE_DFP_ACTIVE			(1 << DfpIndex)
#define PIPE_LCD_ACTIVE			(1 << LfpIndex)
#define PIPE_TV2_ACTIVE			(1 << Tv2Index)
#define PIPE_DFP2_ACTIVE		(1 << Dfp2Index)
#define PIPE_UNKNOWN_ACTIVE		((1 << UnknownIndex) |	\
					 (1 << Unknown2Index))

#define PIPE_SIZED_DISP_MASK		(PIPE_DFP_ACTIVE |	\
					 PIPE_LCD_ACTIVE |	\
					 PIPE_DFP2_ACTIVE)

#define PIPE_A_SHIFT			0
#define PIPE_B_SHIFT			8
#define PIPE_SHIFT(n)			((n) == 0 ? \
					 PIPE_A_SHIFT : PIPE_B_SHIFT)

/*
 * Some BIOS scratch area registers.  The 845 (and 830?) store the amount
 * of video memory available to the BIOS in SWF1.
 */

#define SWF0			0x71410
#define SWF1			0x71414
#define SWF2			0x71418
#define SWF3			0x7141c
#define SWF4			0x71420
#define SWF5			0x71424
#define SWF6			0x71428

/*
 * 855 scratch registers.
 */
#define SWF00			0x70410
#define SWF01			0x70414
#define SWF02			0x70418
#define SWF03			0x7041c
#define SWF04			0x70420
#define SWF05			0x70424
#define SWF06			0x70428

#define SWF10			SWF0
#define SWF11			SWF1
#define SWF12			SWF2
#define SWF13			SWF3
#define SWF14			SWF4
#define SWF15			SWF5
#define SWF16			SWF6

#define SWF30			0x72414
#define SWF31			0x72418
#define SWF32			0x7241c

/*
 * Overlay registers.  These are overlay registers accessed via MMIO.
 * Those loaded via the overlay register page are defined in i830_video.c.
 */
#define OVADD			0x30000

#define DOVSTA			0x30008
#define OC_BUF			(0x3<<20)

#define OGAMC5			0x30010
#define OGAMC4			0x30014
#define OGAMC3			0x30018
#define OGAMC2			0x3001c
#define OGAMC1			0x30020
#define OGAMC0			0x30024


/*
 * Palette registers
 */
#define PALETTE_A		0x0a000
#define PALETTE_B		0x0a800

#endif /* _I810_REG_H */
