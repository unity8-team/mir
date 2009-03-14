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

#define NV_PMC_OFFSET               0x00000000
#define NV_PMC_SIZE                 0x00001000

#define NV_PBUS_OFFSET              0x00001000
#define NV_PBUS_SIZE                0x00001000

#define NV_PFIFO_OFFSET             0x00002000
#define NV_PFIFO_SIZE               0x00002000

#define NV_HDIAG_OFFSET             0x00005000
#define NV_HDIAG_SIZE               0x00001000

#define NV_PRAM_OFFSET              0x00006000
#define NV_PRAM_SIZE                0x00001000

#define NV_PVIDEO_OFFSET            0x00008000
#define NV_PVIDEO_SIZE              0x00001000

#define NV_PTIMER_OFFSET            0x00009000
#define NV_PTIMER_SIZE              0x00001000

#define NV_PPM_OFFSET               0x0000A000
#define NV_PPM_SIZE                 0x00001000

#define NV_PRMVGA_OFFSET            0x000A0000
#define NV_PRMVGA_SIZE              0x00020000

#define NV_PRMVIO0_OFFSET           0x000C0000
#define NV_PRMVIO_SIZE              0x00002000
#define NV_PRMVIO1_OFFSET           0x000C2000

#define NV_PFB_OFFSET               0x00100000
#define NV_PFB_SIZE                 0x00001000

#define NV_PEXTDEV_OFFSET           0x00101000
#define NV_PEXTDEV_SIZE             0x00001000

#define NV_PME_OFFSET               0x00200000
#define NV_PME_SIZE                 0x00001000

#define NV_PROM_OFFSET              0x00300000
#define NV_PROM_SIZE                0x00010000

#define NV_PGRAPH_OFFSET            0x00400000
#define NV_PGRAPH_SIZE              0x00010000

#define NV_PCRTC0_OFFSET            0x00600000
#define NV_PCRTC0_SIZE              0x00002000 /* empirical */

#define NV_PRMCIO0_OFFSET           0x00601000
#define NV_PRMCIO_SIZE              0x00002000
#define NV_PRMCIO1_OFFSET           0x00603000

#define NV50_DISPLAY_OFFSET           0x00610000
#define NV50_DISPLAY_SIZE             0x0000FFFF

#define NV_PRAMDAC0_OFFSET          0x00680000
#define NV_PRAMDAC0_SIZE            0x00002000

#define NV_PRMDIO0_OFFSET           0x00681000
#define NV_PRMDIO_SIZE              0x00002000
#define NV_PRMDIO1_OFFSET           0x00683000

#define NV_PRAMIN_OFFSET            0x00700000
#define NV_PRAMIN_SIZE              0x00100000

#define NV_FIFO_OFFSET              0x00800000
#define NV_FIFO_SIZE                0x00800000

#define NV_PMC_BOOT_0			0x00000000
#define NV_PMC_ENABLE			0x00000200

#define NV_VIO_VSE2			0x000003c3
#define NV_VIO_SRX			0x000003c4

#define NV_CIO_CRX__COLOR		0x000003d4
#define NV_CIO_CR__COLOR		0x000003d5

#define NV_PBUS_DEBUG_1			0x00001084
#define NV_PBUS_DEBUG_4			0x00001098
#define NV_PBUS_DEBUG_DUALHEAD_CTL	0x000010f0
#define NV_PBUS_POWERCTRL_1		0x00001584
#define NV_PBUS_POWERCTRL_2		0x00001588
#define NV_PBUS_POWERCTRL_4		0x00001590
#define NV_PBUS_PCI_NV_19		0x0000184C
#define NV_PBUS_PCI_NV_20		0x00001850
#	define NV_PBUS_PCI_NV_20_ROM_SHADOW_DISABLED	(0 << 0)
#	define NV_PBUS_PCI_NV_20_ROM_SHADOW_ENABLED	(1 << 0)

#define NV_PFIFO_RAMHT			0x00002210

#define NV_PRMVIO_MISC__WRITE		0x000c03c2
#define NV_PRMVIO_SRX			0x000c03c4
#define NV_PRMVIO_SR			0x000c03c5
	#define NV_VIO_SR_RESET_INDEX		0x00
	#define NV_VIO_SR_CLOCK_INDEX		0x01
	#define NV_VIO_SR_PLANE_MASK_INDEX	0x02
	#define NV_VIO_SR_CHAR_MAP_INDEX	0x03
	#define NV_VIO_SR_MEM_MODE_INDEX	0x04
#define NV_PRMVIO_MISC__READ		0x000c03cc
#define NV_PRMVIO_GRX			0x000c03ce
#define NV_PRMVIO_GX			0x000c03cf
	#define NV_VIO_GX_SR_INDEX		0x00
	#define NV_VIO_GX_SREN_INDEX		0x01
	#define NV_VIO_GX_CCOMP_INDEX		0x02
	#define NV_VIO_GX_ROP_INDEX		0x03
	#define NV_VIO_GX_READ_MAP_INDEX	0x04
	#define NV_VIO_GX_MODE_INDEX		0x05
	#define NV_VIO_GX_MISC_INDEX		0x06
	#define NV_VIO_GX_DONT_CARE_INDEX	0x07
	#define NV_VIO_GX_BIT_MASK_INDEX	0x08

#define NV_PFB_BOOT_0			0x00100000
#define NV_PFB_CFG0			0x00100200
#define NV_PFB_CFG1			0x00100204
#define NV_PFB_CSTATUS			0x0010020C
#define NV_PFB_REFCTRL			0x00100210
#	define NV_PFB_REFCTRL_VALID_1			(1 << 31)
#define NV_PFB_PAD			0x0010021C
#	define NV_PFB_PAD_CKE_NORMAL			(1 << 0)
#define NV_PFB_TILE_NV10		0x00100240
#define NV_PFB_TILE_SIZE_NV10		0x00100244
#define NV_PFB_REF			0x001002D0
#	define NV_PFB_REF_CMD_REFRESH			(1 << 0)
#define NV_PFB_PRE			0x001002D4
#	define NV_PFB_PRE_CMD_PRECHARGE			(1 << 0)
#define NV_PFB_CLOSE_PAGE2		0x0010033C
#define NV_PFB_TILE_NV40		0x00100600
#define NV_PFB_TILE_SIZE_NV40		0x00100604

#define NV_PEXTDEV_BOOT_0		0x00101000
#	define NV_PEXTDEV_BOOT_0_STRAP_FP_IFACE_12BIT	(8 << 12)
#define NV_PEXTDEV_BOOT_3		0x0010100c

#define NV_CRTC_INTR_0			0x00600100
#	define NV_CRTC_INTR_VBLANK			(1<<0)
#define NV_CRTC_INTR_EN_0		0x00600140
#define NV_CRTC_START			0x00600800
#define NV_CRTC_CONFIG			0x00600804
	#define NV_PCRTC_CONFIG_START_ADDRESS_NON_VGA	1
	#define NV_PCRTC_CONFIG_START_ADDRESS_HSYNC	2
#define NV_CRTC_CURSOR_ADDRESS		0x0060080C
#define NV_CRTC_CURSOR_CONFIG		0x00600810
#	define NV_CRTC_CURSOR_CONFIG_ENABLE		(1 << 0)
#	define NV_CRTC_CURSOR_CONFIG_DOUBLE_SCAN	(1 << 4)
#	define NV_PCRTC_CURSOR_CONFIG_ADDRESS_SPACE_PNVM	(1 << 8)
#	define NV_CRTC_CURSOR_CONFIG_32BPP		(1 << 12)
#	define NV_CRTC_CURSOR_CONFIG_64PIXELS		(1 << 16)
#	define NV_CRTC_CURSOR_CONFIG_32LINES		(2 << 24)
#	define NV_CRTC_CURSOR_CONFIG_64LINES		(4 << 24)
#	define NV_CRTC_CURSOR_CONFIG_ALPHA_BLEND	(1 << 28)

#define NV_CRTC_GPIO			0x00600818
#define NV_CRTC_GPIO_EXT		0x0060081c
#define NV_CRTC_0830			0x00600830
#define NV_CRTC_0834			0x00600834
#define NV_CRTC_0850			0x00600850
#define NV_CRTC_FSEL			0x00600860
#	define NV_CRTC_FSEL_I2C				(1<<4)
#	define NV_CRTC_FSEL_TVOUT1			(1<<8)
#	define NV_CRTC_FSEL_TVOUT2			(2<<8)
#	define NV_CRTC_FSEL_OVERLAY			(1<<12)

#define NV_PRMCIO_ARX			0x006013c0
#define NV_PRMCIO_AR__WRITE		0x006013c0
#define NV_PRMCIO_AR__READ		0x006013c1
	#define	NV_CIO_AR_MODE_INDEX		0x10
	#define NV_CIO_AR_OSCAN_INDEX		0x11
	#define NV_CIO_AR_PLANE_INDEX		0x12
	#define NV_CIO_AR_HPP_INDEX		0x13
	#define NV_CIO_AR_CSEL_INDEX		0x14
#define NV_PRMCIO_CRX__COLOR		0x006013d4
#define NV_PRMCIO_CR__COLOR		0x006013d5
	/* Standard VGA CRTC registers */
	#define NV_CIO_CR_HDT_INDEX		0x00	/* horizontal display total */
	#define NV_CIO_CR_HDE_INDEX		0x01	/* horizontal display end */
	#define NV_CIO_CR_HBS_INDEX		0x02	/* horizontal blanking start */
	#define NV_CIO_CR_HBE_INDEX		0x03	/* horizontal blanking end */
	#define NV_CIO_CR_HRS_INDEX		0x04	/* horizontal retrace start */
	#define NV_CIO_CR_HRE_INDEX		0x05	/* horizontal retrace end */
	#define NV_CIO_CR_VDT_INDEX		0x06	/* vertical display total */
	#define NV_CIO_CR_OVL_INDEX		0x07	/* overflow bits */
	#define NV_CIO_CR_RSAL_INDEX		0x08	/* normally "preset row scan" */
	#define NV_CIO_CR_CELL_HT_INDEX		0x09	/* cell height?! normally "max scan line" */
		#define NV_CIO_CR_CELL_HT_SCANDBL	0x80
	#define NV_CIO_CR_CURS_ST_INDEX		0x0a	/* cursor start */
	#define NV_CIO_CR_CURS_END_INDEX	0x0b	/* cursor end */
	#define NV_CIO_CR_SA_HI_INDEX		0x0c	/* screen start address high */
	#define NV_CIO_CR_SA_LO_INDEX		0x0d	/* screen start address low */
	#define NV_CIO_CR_TCOFF_HI_INDEX	0x0e	/* cursor offset high */
	#define NV_CIO_CR_TCOFF_LO_INDEX	0x0f	/* cursor offset low */
	#define NV_CIO_CR_VRS_INDEX		0x10	/* vertical retrace start */
	#define NV_CIO_CR_VRE_INDEX		0x11	/* vertical retrace end */
	#define NV_CIO_CR_VDE_INDEX		0x12	/* vertical display end */
	#define NV_CIO_CR_OFFSET_INDEX		0x13	/* sets screen pitch */
	#define NV_CIO_CR_ULINE_INDEX		0x14	/* underline location */
	#define NV_CIO_CR_VBS_INDEX		0x15	/* vertical blank start */
	#define NV_CIO_CR_VBE_INDEX		0x16	/* vertical blank end */
	#define NV_CIO_CR_MODE_INDEX		0x17	/* crtc mode control */
	#define NV_CIO_CR_LCOMP_INDEX		0x18	/* line compare */
	/* Extended VGA CRTC registers */
	#define NV_CIO_CRE_RPC0_INDEX		0x19	/* repaint control 0 */
	#define NV_CIO_CRE_RPC1_INDEX		0x1a	/* repaint control 1 */
		#define NV_CIO_CRE_RPC1_LARGE		0x04
	#define NV_CIO_CRE_FF_INDEX		0x1b	/* fifo control */
	#define NV_CIO_CRE_ENH_INDEX		0x1c	/* enhanced? */
	#define NV_CIO_SR_LOCK_INDEX		0x1f	/* crtc lock */
		#define	NV_CIO_SR_UNLOCK_RW_VALUE	0x57
		#define	NV_CIO_SR_LOCK_VALUE		0x99
	#define NV_CIO_CRE_FFLWM__INDEX		0x20	/* fifo low water mark */
	#define NV_CIO_CRE_21			0x21	/* referred to by some .scp as `shadow lock' */
	#define NV_CIO_CRE_LSR_INDEX		0x25	/* ? */
	#define NV_CIO_CR_ARX_INDEX		0x26	/* attribute index -- ro copy of 0x60.3c0 */
	#define NV_CIO_CRE_CHIP_ID_INDEX	0x27	/* chip revision */
	#define NV_CIO_CRE_PIXEL_INDEX		0x28
	#define NV_CIO_CRE_HEB__INDEX		0x2d	/* horizontal extra bits? */
	#define NV_CIO_CRE_2E			0x2e	/* some scratch or dummy reg to force writes to sink in */
	#define NV_CIO_CRE_HCUR_ADDR2_INDEX	0x2f	/* cursor */
	#define NV_CIO_CRE_HCUR_ADDR0_INDEX	0x30		/* pixmap */
		#define NV_CIO_CRE_HCUR_ASI		0x80
	#define NV_CIO_CRE_HCUR_ADDR1_INDEX	0x31			/* address */
		#define NV_CIO_CRE_HCUR_ADDR1_CUR_DBL	0x02
		#define NV_CIO_CRE_HCUR_ADDR1_ENABLE	0x01
	#define NV_CIO_CRE_LCD__INDEX		0x33
		#define NV_CIO_CRE_LCD_LCD_SELECT	0x01
	#define NV_CIO_CRE_DDC0_STATUS__INDEX	0x36
	#define NV_CIO_CRE_DDC0_WR__INDEX	0x37
	#define NV_CIO_CRE_ILACE__INDEX		0x39	/* interlace */
	#define NV_CIO_CRE_SCRATCH3__INDEX	0x3b
	#define NV_CIO_CRE_SCRATCH4__INDEX	0x3c
	#define NV_CIO_CRE_DDC_STATUS__INDEX	0x3e
	#define NV_CIO_CRE_DDC_WR__INDEX	0x3f
	#define NV_CIO_CRE_EBR_INDEX		0x41	/* extra bits ? (vertical) */
	#define NV_CIO_CRE_44			0x44	/* head control */
	#define NV_CIO_CRE_CSB			0x45	/* colour saturation b...? */
	#define NV_CIO_CRE_RCR			0x46
		#define NV_CIO_CRE_RCR_ENDIAN_BIG	0x80
	#define NV_CIO_CRE_47			0x47	/* extended fifo lwm, used on nv30+ */
	#define NV_CIO_CRE_4B			0x4b	/* given patterns in 0x[2-3][a-c] regs, probably scratch 6 */
	#define NV_CIO_CRE_TVOUT_LATENCY	0x52
	#define NV_CIO_CRE_53			0x53	/* `fp_htiming' according to Haiku */
	#define NV_CIO_CRE_54			0x54	/* `fp_vtiming' according to Haiku */
	#define NV_CIO_CRE_57			0x57	/* index reg for cr58 */
	#define NV_CIO_CRE_58			0x58	/* data reg for cr57 */
	#define NV_CIO_CRE_59			0x59
	#define NV_CIO_CRE_5B			0x5B	/* newer colour saturation reg */
	#define NV_CIO_CRE_85			0x85
	#define NV_CIO_CRE_86			0x86
#define NV_PRMCIO_INP0__COLOR		0x006013da

#define NV_RAMDAC_CURSOR_POS		0x00680300
#define NV_RAMDAC_CURSOR_CTRL		0x00680320
#define NV_RAMDAC_CURSOR_DATA_LO	0x00680324
#define NV_RAMDAC_CURSOR_DATA_HI	0x00680328
#define NV_RAMDAC_NV10_CURSYNC		0x00680404

#define NV_RAMDAC_NVPLL			0x00680500
#define NV_RAMDAC_MPLL			0x00680504
#define NV_RAMDAC_VPLL			0x00680508
#	define NV_RAMDAC_PLL_COEFF_MDIV			0x000000FF
#	define NV_RAMDAC_PLL_COEFF_NDIV			0x0000FF00
#	define NV_RAMDAC_PLL_COEFF_PDIV			0x00070000
#	define NV30_RAMDAC_ENABLE_VCO2			(8 << 4)

#define NV_RAMDAC_PLL_SELECT		0x0068050c
/* Without this it will use vpll1 */
/* Maybe only for nv4x */
#	define NV_RAMDAC_PLL_SELECT_USE_VPLL2_FALSE	(0<<0)
#	define NV_RAMDAC_PLL_SELECT_USE_VPLL2_TRUE	(4<<0)
#	define NV_RAMDAC_PLL_SELECT_DLL_BYPASS_TRUE	(1<<4)
#	define NV_RAMDAC_PLL_SELECT_PLL_SOURCE_DEFAULT	(0<<8)
#	define NV_RAMDAC_PLL_SELECT_PLL_SOURCE_MPLL	(1<<8)
#	define NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL	(2<<8)
#	define NV_RAMDAC_PLL_SELECT_PLL_SOURCE_NVPLL	(4<<8)
#	define NV_RAMDAC_PLL_SELECT_PLL_SOURCE_ALL	(7<<8)
#	define NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL2	(8<<8)
#	define NV_RAMDAC_PLL_SELECT_MPLL_BYPASS_FALSE	(0<<12)
#	define NV_RAMDAC_PLL_SELECT_MPLL_BYPASS_TRUE	(1<<12)
#	define NV_RAMDAC_PLL_SELECT_VS_PCLK_TV_NONE	(0<<16)
#	define NV_RAMDAC_PLL_SELECT_VS_PCLK_TV_VSCLK	(1<<16)
#	define NV_RAMDAC_PLL_SELECT_VS_PCLK_TV_PCLK	(2<<16)
#	define NV_RAMDAC_PLL_SELECT_VS_PCLK_TV_BOTH	(3<<16)
#	define NV_RAMDAC_PLL_SELECT_TVCLK_SOURCE_EXT	(0<<20)
#	define NV_RAMDAC_PLL_SELECT_TVCLK_SOURCE_VIP	(1<<20)
#	define NV_RAMDAC_PLL_SELECT_TVCLK_RATIO_DB1	(0<<24)
#	define NV_RAMDAC_PLL_SELECT_TVCLK_RATIO_DB2	(1<<24)
#	define NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB1	(0<<28)
#	define NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2	(1<<28)
#	define NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB1	(0<<28)
#	define NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2	(2<<28)

#define NV_RAMDAC_PLL_SETUP_CONTROL	0x00680510
#define NV_RAMDAC_PLL_TEST_COUNTER	0x00680514
#define NV_RAMDAC_PALETTE_TEST		0x00680518
#define NV_RAMDAC_VPLL2			0x00680520
#define NV_RAMDAC_SEL_CLK		0x00680524
#define NV_RAMDAC_DITHER_NV11		0x00680528
#define NV_RAMDAC_OUTPUT		0x0068052c
#	define NV_RAMDAC_OUTPUT_DAC_ENABLE		(1<<0)
#	define NV_RAMDAC_OUTPUT_SELECT_CRTC1		(1<<8)

#define NV_RAMDAC_NVPLL_B		0x00680570
#define NV_RAMDAC_MPLL_B		0x00680574
#define NV_RAMDAC_VPLL_B		0x00680578
#define NV_RAMDAC_VPLL2_B		0x0068057c
/* Educated guess, should remain on for NV4x vpll's. */
#	define NV31_RAMDAC_ENABLE_VCO2			(8 << 28)

#define NV_RAMDAC_580			0x00680580
/* This is not always activated, but only when VCLK_RATIO_DB1 is used */
#	define NV_RAMDAC_580_VPLL1_ACTIVE		(1<<8)
#	define NV_RAMDAC_580_VPLL2_ACTIVE		(1<<28)

#define NV_RAMDAC_594			0x00680594
#define NV_RAMDAC_GENERAL_CONTROL	0x00680600
#define NV_RAMDAC_TEST_CONTROL		0x00680608
	#define NV_PRAMDAC_TEST_CONTROL_TP_INS_EN_ASSERTED	(1 << 12)
	#define NV_PRAMDAC_TEST_CONTROL_PWRDWN_DAC_OFF	(1 << 16)
	#define NV_PRAMDAC_TEST_CONTROL_SENSEB_ALLHI	(1 << 28)
#define NV_RAMDAC_TEST_DATA		0x00680610
	#define NV_PRAMDAC_TESTPOINT_DATA_NOTBLANK	(8 << 28)
#define NV_RAMDAC_630			0x00680630
#define NV_RAMDAC_634			0x00680634
/* This register is similar to TEST_CONTROL in the style of values */
#define NV_RAMDAC_670			0x00680670

#define NV_RAMDAC_TV_SETUP		0x00680700
#define NV_RAMDAC_TV_VBLANK_START	0x00680704
#define NV_RAMDAC_TV_VBLANK_END		0x00680708
#define NV_RAMDAC_TV_HBLANK_START	0x0068070c
#define NV_RAMDAC_TV_HBLANK_END		0x00680710
#define NV_RAMDAC_TV_BLANK_COLOR	0x00680714
#define NV_RAMDAC_TV_VTOTAL		0x00680720
#define NV_RAMDAC_TV_VSYNC_START	0x00680724
#define NV_RAMDAC_TV_VSYNC_END		0x00680728
#define NV_RAMDAC_TV_HTOTAL		0x0068072c
#define NV_RAMDAC_TV_HSYNC_START	0x00680730
#define NV_RAMDAC_TV_HSYNC_END		0x00680734
#define NV_RAMDAC_TV_SYNC_DELAY		0x00680738

#define REG_DISP_END 0
#define REG_DISP_TOTAL 1
#define REG_DISP_CRTC 2
#define REG_DISP_SYNC_START 3
#define REG_DISP_SYNC_END 4
#define REG_DISP_VALID_START 5
#define REG_DISP_VALID_END 6

#define NV_RAMDAC_FP_VDISP_END		0x00680800
#define NV_RAMDAC_FP_VTOTAL		0x00680804
#define NV_RAMDAC_FP_VCRTC		0x00680808
#define NV_RAMDAC_FP_VSYNC_START	0x0068080c
#define NV_RAMDAC_FP_VSYNC_END		0x00680810
#define NV_RAMDAC_FP_VVALID_START	0x00680814
#define NV_RAMDAC_FP_VVALID_END		0x00680818
#define NV_RAMDAC_FP_HDISP_END		0x00680820
#define NV_RAMDAC_FP_HTOTAL		0x00680824
#define NV_RAMDAC_FP_HCRTC		0x00680828
#define NV_RAMDAC_FP_HSYNC_START	0x0068082c
#define NV_RAMDAC_FP_HSYNC_END		0x00680830
#define NV_RAMDAC_FP_HVALID_START	0x00680834
#define NV_RAMDAC_FP_HVALID_END		0x00680838

#define NV_RAMDAC_FP_DITHER		0x0068083c
#define NV_RAMDAC_FP_CHECKSUM		0x00680840
#define NV_RAMDAC_FP_TEST_CONTROL	0x00680844
#define NV_RAMDAC_FP_CONTROL		0x00680848
#	define NV_RAMDAC_FP_CONTROL_VSYNC_NEG		(0 << 0)
#	define NV_RAMDAC_FP_CONTROL_VSYNC_POS		(1 << 0)
#	define NV_RAMDAC_FP_CONTROL_VSYNC_DISABLE	(2 << 0)
#	define NV_RAMDAC_FP_CONTROL_HSYNC_NEG		(0 << 4)
#	define NV_RAMDAC_FP_CONTROL_HSYNC_POS		(1 << 4)
#	define NV_RAMDAC_FP_CONTROL_HSYNC_DISABLE	(2 << 4)
#	define NV_RAMDAC_FP_CONTROL_MODE_SCALE		(0 << 8)
#	define NV_RAMDAC_FP_CONTROL_MODE_CENTER		(1 << 8)
#	define NV_RAMDAC_FP_CONTROL_MODE_NATIVE		(2 << 8)
#	define NV_RAMDAC_FP_CONTROL_WIDTH_12			(1 << 24)
#	define NV_RAMDAC_FP_CONTROL_DISPEN_POS			(1 << 28)
#	define NV_RAMDAC_FP_CONTROL_DISPEN_DISABLE		(2 << 28)
	#define NV_PRAMDAC_FP_TG_CONTROL_OFF		(NV_RAMDAC_FP_CONTROL_DISPEN_DISABLE |	\
							 NV_RAMDAC_FP_CONTROL_HSYNC_DISABLE |	\
							 NV_RAMDAC_FP_CONTROL_VSYNC_DISABLE)
#define NV_RAMDAC_FP_850		0x00680850
#define NV_RAMDAC_FP_85C		0x0068085c

#define NV_RAMDAC_FP_DEBUG_0		0x00680880
#	define NV_RAMDAC_FP_DEBUG_0_XSCALE_ENABLED	(1 << 0)
#	define NV_RAMDAC_FP_DEBUG_0_YSCALE_ENABLED	(1 << 4)
/* This doesn't seem to be essential for tmds, but still often set */
#	define NV_RAMDAC_FP_DEBUG_0_TMDS_ENABLED	(8 << 4)
#	define NV_RAMDAC_FP_DEBUG_0_PWRDOWN_FPCLK	(1 << 28)
#	define NV_RAMDAC_FP_DEBUG_0_PWRDOWN_TMDS_PLL	(2 << 28)
#	define NV_RAMDAC_FP_DEBUG_0_PWRDOWN_BOTH	(3 << 28)
#define NV_RAMDAC_FP_DEBUG_1		0x00680884
#define NV_RAMDAC_FP_DEBUG_2		0x00680888
#define NV_RAMDAC_FP_DEBUG_3		0x0068088C

/* Some unknown regs, purely for NV30 it seems. */
#define NV30_RAMDAC_890			0x00680890
#define NV30_RAMDAC_894			0x00680894
#define NV30_RAMDAC_89C			0x0068089C

/* see NV_PRAMDAC_INDIR_TMDS in rules.xml */
#define NV_RAMDAC_FP_TMDS_CONTROL	0x006808b0
#	define NV_RAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE	(1<<16)
#define NV_RAMDAC_FP_TMDS_DATA		0x006808b4
#define NV_RAMDAC_FP_TMDS_CONTROL_2	0x006808b8
#	define NV_RAMDAC_FP_TMDS_CONTROL_2_WRITE_DISABLE	(1<<16)
#define NV_RAMDAC_FP_TMDS_DATA_2	0x006808bc

/* Some kind of switch */
#define NV_RAMDAC_900			0x00680900
#define NV_RAMDAC_A20			0x00680A20
#define NV_RAMDAC_A24			0x00680A24
#define NV_RAMDAC_A34			0x00680A34

/* names fabricated from NV_USER_DAC info */
#define NV_PRMDIO_PIXEL_MASK		0x006813c6
	#define NV_PRMDIO_PIXEL_MASK_MASK	0xff
#define NV_PRMDIO_READ_MODE_ADDRESS	0x006813c7
#define NV_PRMDIO_WRITE_MODE_ADDRESS	0x006813c8
#define NV_PRMDIO_PALETTE_DATA		0x006813c9

#define NV_PGRAPH_DEBUG_0		0x00400080
#define NV_PGRAPH_DEBUG_1		0x00400084
#define NV_PGRAPH_DEBUG_2_NV04		0x00400088
#define NV_PGRAPH_DEBUG_2		0x00400620
#define NV_PGRAPH_DEBUG_3		0x0040008c
#define NV_PGRAPH_DEBUG_4		0x00400090
#define NV_PGRAPH_INTR			0x00400100
#define NV_PGRAPH_INTR_EN		0x00400140
#define NV_PGRAPH_CTX_CONTROL		0x00400144
#define NV_PGRAPH_CTX_CONTROL_NV04	0x00400170
#define NV_PGRAPH_ABS_UCLIP_XMIN	0x0040053C
#define NV_PGRAPH_ABS_UCLIP_YMIN	0x00400540
#define NV_PGRAPH_ABS_UCLIP_XMAX	0x00400544
#define NV_PGRAPH_ABS_UCLIP_YMAX	0x00400548
#define NV_PGRAPH_BETA_AND		0x00400608
#define NV_PGRAPH_LIMIT_VIOL_PIX	0x00400610
#define NV_PGRAPH_BOFFSET0		0x00400640
#define NV_PGRAPH_BOFFSET1		0x00400644
#define NV_PGRAPH_BOFFSET2		0x00400648
#define NV_PGRAPH_BLIMIT0		0x00400684
#define NV_PGRAPH_BLIMIT1		0x00400688
#define NV_PGRAPH_BLIMIT2		0x0040068c
#define NV_PGRAPH_STATUS		0x00400700
#define NV_PGRAPH_SURFACE		0x00400710
#define NV_PGRAPH_STATE			0x00400714
#define NV_PGRAPH_FIFO			0x00400720
#define NV_PGRAPH_PATTERN_SHAPE		0x00400810
#define NV_PGRAPH_TILE			0x00400b00

#define NV_PVIDEO_INTR_EN		0x00008140
#define NV_PVIDEO_BUFFER		0x00008700
#define NV_PVIDEO_STOP			0x00008704
#define NV_PVIDEO_UVPLANE_BASE(buff)	(0x00008800+(buff)*4)
#define NV_PVIDEO_UVPLANE_LIMIT(buff)	(0x00008808+(buff)*4)
#define NV_PVIDEO_UVPLANE_OFFSET_BUFF(buff)	(0x00008820+(buff)*4)
#define NV_PVIDEO_BASE(buff)		(0x00008900+(buff)*4)
#define NV_PVIDEO_LIMIT(buff)		(0x00008908+(buff)*4)
#define NV_PVIDEO_LUMINANCE(buff)	(0x00008910+(buff)*4)
#define NV_PVIDEO_CHROMINANCE(buff)	(0x00008918+(buff)*4)
#define NV_PVIDEO_OFFSET_BUFF(buff)	(0x00008920+(buff)*4)
#define NV_PVIDEO_SIZE_IN(buff)		(0x00008928+(buff)*4)
#define NV_PVIDEO_POINT_IN(buff)	(0x00008930+(buff)*4)
#define NV_PVIDEO_DS_DX(buff)		(0x00008938+(buff)*4)
#define NV_PVIDEO_DT_DY(buff)		(0x00008940+(buff)*4)
#define NV_PVIDEO_POINT_OUT(buff)	(0x00008948+(buff)*4)
#define NV_PVIDEO_SIZE_OUT(buff)	(0x00008950+(buff)*4)
#define NV_PVIDEO_FORMAT(buff)		(0x00008958+(buff)*4)
#	define NV_PVIDEO_FORMAT_PLANAR			(1 << 0)
#	define NV_PVIDEO_FORMAT_COLOR_LE_CR8YB8CB8YA8	(1 << 16)
#	define NV_PVIDEO_FORMAT_DISPLAY_COLOR_KEY	(1 << 20)
#	define NV_PVIDEO_FORMAT_MATRIX_ITURBT709	(1 << 24)
#define NV_PVIDEO_COLOR_KEY		0x00008B00

/* NV04 overlay defines from VIDIX & Haiku */
#define NV_PVIDEO_INTR_EN_0		0x00680140
#define NV_PVIDEO_STEP_SIZE		0x00680200
#define NV_PVIDEO_CONTROL_Y		0x00680204
#define NV_PVIDEO_CONTROL_X		0x00680208
#define NV_PVIDEO_BUFF0_START_ADDRESS	0x0068020c
#define NV_PVIDEO_BUFF0_PITCH_LENGTH	0x00680214
#define NV_PVIDEO_BUFF0_OFFSET		0x0068021c
#define NV_PVIDEO_BUFF1_START_ADDRESS	0x00680210
#define NV_PVIDEO_BUFF1_PITCH_LENGTH	0x00680218
#define NV_PVIDEO_BUFF1_OFFSET		0x00680220
#define NV_PVIDEO_OE_STATE		0x00680224
#define NV_PVIDEO_SU_STATE		0x00680228
#define NV_PVIDEO_RM_STATE		0x0068022c
#define NV_PVIDEO_WINDOW_START		0x00680230
#define NV_PVIDEO_WINDOW_SIZE		0x00680234
#define NV_PVIDEO_FIFO_THRES_SIZE	0x00680238
#define NV_PVIDEO_FIFO_BURST_LENGTH	0x0068023c
#define NV_PVIDEO_KEY			0x00680240
#define NV_PVIDEO_OVERLAY		0x00680244
#define NV_PVIDEO_RED_CSC_OFFSET	0x00680280
#define NV_PVIDEO_GREEN_CSC_OFFSET	0x00680284
#define NV_PVIDEO_BLUE_CSC_OFFSET	0x00680288
#define NV_PVIDEO_CSC_ADJUST		0x0068028c

/* These are the real registers, not the redirected ones */
#define NV40_VCLK1_A			0x4010
#define NV40_VCLK1_B			0x4014
#define NV40_VCLK2_A			0x4018
#define NV40_VCLK2_B			0x401c

#endif
