/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>

/* X and server generic header files */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "fbdevhw.h"
#include "vgaHW.h"

/* Driver data structures */
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_version.h"
#include "radeon_mergedfb.h"

extern int getRADEONEntityIndex(void);

const char *MonTypeName[7] = {
  "AUTO",
  "NONE",
  "CRT",
  "LVDS",
  "TMDS",
  "CTV",
  "STV"
};

const RADEONMonitorType MonTypeID[7] = {
  MT_UNKNOWN, /* this is just a dummy value for AUTO DETECTION */
  MT_NONE,    /* NONE -> NONE */
  MT_CRT,     /* CRT -> CRT */
  MT_LCD,     /* Laptop LCDs are driven via LVDS port */
  MT_DFP,     /* DFPs are driven via TMDS */
  MT_CTV,     /* CTV -> CTV */
  MT_STV,     /* STV -> STV */
};

const char *TMDSTypeName[3] = {
  "NONE",
  "Internal",
  "External"
};

const char *DDCTypeName[6] = {
  "NONE",
  "MONID",
  "DVI_DDC",
  "VGA_DDC",
  "CRT2_DDC",
  "LCD_DDC"
};

const char *DACTypeName[3] = {
  "Unknown",
  "Primary",
  "TVDAC/ExtDAC",
};

const char *ConnectorTypeName[8] = {
  "None",
  "Proprietary",
  "VGA",
  "DVI-I",
  "DVI-D",
  "CTV",
  "STV",
  "Unsupported"
};

const char *ConnectorTypeNameATOM[10] = {
  "None",
  "VGA",
  "DVI-I",
  "DVI-D",
  "DVI-A",
  "STV",
  "CTV",
  "LVDS",
  "Digital",
  "Unsupported"
};


static const RADEONTMDSPll default_tmds_pll[CHIP_FAMILY_LAST][4] =
{
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}},				/*CHIP_FAMILY_UNKNOW*/
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}},				/*CHIP_FAMILY_LEGACY*/
    {{12000, 0xa1b}, {0xffffffff, 0xa3f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RADEON*/
    {{12000, 0xa1b}, {0xffffffff, 0xa3f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RV100*/
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}},				/*CHIP_FAMILY_RS100*/
    {{15000, 0xa1b}, {0xffffffff, 0xa3f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RV200*/
    {{12000, 0xa1b}, {0xffffffff, 0xa3f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RS200*/
    {{15000, 0xa1b}, {0xffffffff, 0xa3f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_R200*/
    {{15500, 0x81b}, {0xffffffff, 0x83f}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RV250*/
    {{0, 0}, {0, 0}, {0, 0}, {0, 0}},				/*CHIP_FAMILY_RS300*/
    {{13000, 0x400f4}, {15000, 0x400f7}, {0xffffffff, 0x40111}, {0, 0}}, /*CHIP_FAMILY_RV280*/
    {{0xffffffff, 0xb01cb}, {0, 0}, {0, 0}, {0, 0}},		/*CHIP_FAMILY_R300*/
    {{0xffffffff, 0xb01cb}, {0, 0}, {0, 0}, {0, 0}},		/*CHIP_FAMILY_R350*/
    {{15000, 0xb0155}, {0xffffffff, 0xb01cb}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RV350*/
    {{15000, 0xb0155}, {0xffffffff, 0xb01cb}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RV380*/
    {{0xffffffff, 0xb01cb}, {0, 0}, {0, 0}, {0, 0}},		/*CHIP_FAMILY_R420*/
    {{0xffffffff, 0xb01cb}, {0, 0}, {0, 0}, {0, 0}},		/*CHIP_FAMILY_RV410*/ /* FIXME: just values from r420 used... */
    {{15000, 0xb0155}, {0xffffffff, 0xb01cb}, {0, 0}, {0, 0}},	/*CHIP_FAMILY_RS400*/ /* FIXME: just values from rv380 used... */
};

static const CARD32 default_tvdac_adj [CHIP_FAMILY_LAST] =
{
    0x00000000,   /* unknown */
    0x00000000,   /* legacy */
    0x00000000,   /* r100 */
    0x00280000,   /* rv100 */
    0x00000000,   /* rs100 */
    0x00880000,   /* rv200 */
    0x00000000,   /* rs200 */
    0x00000000,   /* r200 */
    0x00770000,   /* rv250 */
    0x00290000,   /* rs300 */
    0x00560000,   /* rv280 */
    0x00780000,   /* r300 */
    0x00770000,   /* r350 */
    0x00780000,   /* rv350 */
    0x00780000,   /* rv380 */
    0x01080000,   /* r420 */
    0x01080000,   /* rv410 */ /* FIXME: just values from r420 used... */
    0x00780000,   /* rs400 */ /* FIXME: just values from rv380 used... */
};

static void RADEONI2CGetBits(I2CBusPtr b, int *Clock, int *data)
{
    ScrnInfoPtr    pScrn      = xf86Screens[b->scrnIndex];
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned long  val;
    unsigned char *RADEONMMIO = info->MMIO;

    /* Get the result */

    if (info->DDCReg == RADEON_LCD_GPIO_MASK) { 
        val = INREG(info->DDCReg+4);
        *Clock = (val & (1<<13)) != 0;
        *data  = (val & (1<<12)) != 0;
    } else {
        val = INREG(info->DDCReg);
        *Clock = (val & RADEON_GPIO_Y_1) != 0;
        *data  = (val & RADEON_GPIO_Y_0) != 0;
    }
}

static void RADEONI2CPutBits(I2CBusPtr b, int Clock, int data)
{
    ScrnInfoPtr    pScrn      = xf86Screens[b->scrnIndex];
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned long  val;
    unsigned char *RADEONMMIO = info->MMIO;

    if (info->DDCReg == RADEON_LCD_GPIO_MASK) {
        val = INREG(info->DDCReg) & (CARD32)~((1<<12) | (1<<13));
        val |= (Clock ? 0:(1<<13));
        val |= (data ? 0:(1<<12));
        OUTREG(info->DDCReg, val);
    } else {
        val = INREG(info->DDCReg) & (CARD32)~(RADEON_GPIO_EN_0 | RADEON_GPIO_EN_1);
        val |= (Clock ? 0:RADEON_GPIO_EN_1);
        val |= (data ? 0:RADEON_GPIO_EN_0);
        OUTREG(info->DDCReg, val);
   }
    /* read back to improve reliability on some cards. */
    val = INREG(info->DDCReg);
}

Bool RADEONI2cInit(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    info->pI2CBus = xf86CreateI2CBusRec();
    if (!info->pI2CBus) return FALSE;

    info->pI2CBus->BusName    = "DDC";
    info->pI2CBus->scrnIndex  = pScrn->scrnIndex;
    info->pI2CBus->I2CPutBits = RADEONI2CPutBits;
    info->pI2CBus->I2CGetBits = RADEONI2CGetBits;
    info->pI2CBus->AcknTimeout = 5;

    if (!xf86I2CBusInit(info->pI2CBus)) return FALSE;
    return TRUE;
}

void RADEONSetSyncRangeFromEdid(ScrnInfoPtr pScrn, int flag)
{
    MonPtr      mon = pScrn->monitor;
    xf86MonPtr  ddc = mon->DDC;
    int         i;

    if (flag) { /* HSync */
	for (i = 0; i < 4; i++) {
	    if (ddc->det_mon[i].type == DS_RANGES) {
		mon->nHsync = 1;
		mon->hsync[0].lo = ddc->det_mon[i].section.ranges.min_h;
		mon->hsync[0].hi = ddc->det_mon[i].section.ranges.max_h;
		return;
	    }
	}
	/* If no sync ranges detected in detailed timing table, let's
	 * try to derive them from supported VESA modes.  Are we doing
	 * too much here!!!?  */
	i = 0;
	if (ddc->timings1.t1 & 0x02) { /* 800x600@56 */
	    mon->hsync[i].lo = mon->hsync[i].hi = 35.2;
	    i++;
	}
	if (ddc->timings1.t1 & 0x04) { /* 640x480@75 */
	    mon->hsync[i].lo = mon->hsync[i].hi = 37.5;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x08) || (ddc->timings1.t1 & 0x01)) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 37.9;
	    i++;
	}
	if (ddc->timings1.t2 & 0x40) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 46.9;
	    i++;
	}
	if ((ddc->timings1.t2 & 0x80) || (ddc->timings1.t2 & 0x08)) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 48.1;
	    i++;
	}
	if (ddc->timings1.t2 & 0x04) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 56.5;
	    i++;
	}
	if (ddc->timings1.t2 & 0x02) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 60.0;
	    i++;
	}
	if (ddc->timings1.t2 & 0x01) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 64.0;
	    i++;
	}
	mon->nHsync = i;
    } else {  /* Vrefresh */
	for (i = 0; i < 4; i++) {
	    if (ddc->det_mon[i].type == DS_RANGES) {
		mon->nVrefresh = 1;
		mon->vrefresh[0].lo = ddc->det_mon[i].section.ranges.min_v;
		mon->vrefresh[0].hi = ddc->det_mon[i].section.ranges.max_v;
		return;
	    }
	}

	i = 0;
	if (ddc->timings1.t1 & 0x02) { /* 800x600@56 */
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 56;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x01) || (ddc->timings1.t2 & 0x08)) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 60;
	    i++;
	}
	if (ddc->timings1.t2 & 0x04) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 70;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x08) || (ddc->timings1.t2 & 0x80)) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 72;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x04) || (ddc->timings1.t2 & 0x40) ||
	    (ddc->timings1.t2 & 0x02) || (ddc->timings1.t2 & 0x01)) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 75;
	    i++;
	}
	mon->nVrefresh = i;
    }
}

static RADEONMonitorType
RADEONCrtIsPhysicallyConnected(ScrnInfoPtr pScrn, int IsCrtDac)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int		  bConnected = 0;

    /* the monitor either wasn't connected or it is a non-DDC CRT.
     * try to probe it
     */
    if(IsCrtDac) {
	unsigned long ulOrigVCLK_ECP_CNTL;
	unsigned long ulOrigDAC_CNTL;
	unsigned long ulOrigDAC_MACRO_CNTL;
	unsigned long ulOrigDAC_EXT_CNTL;
	unsigned long ulOrigCRTC_EXT_CNTL;
	unsigned long ulData;
	unsigned long ulMask;

	ulOrigVCLK_ECP_CNTL = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);

	ulData              = ulOrigVCLK_ECP_CNTL;
	ulData             &= ~(RADEON_PIXCLK_ALWAYS_ONb
				| RADEON_PIXCLK_DAC_ALWAYS_ONb);
	ulMask              = ~(RADEON_PIXCLK_ALWAYS_ONb
				|RADEON_PIXCLK_DAC_ALWAYS_ONb);
	OUTPLLP(pScrn, RADEON_VCLK_ECP_CNTL, ulData, ulMask);

	ulOrigCRTC_EXT_CNTL = INREG(RADEON_CRTC_EXT_CNTL);
	ulData              = ulOrigCRTC_EXT_CNTL;
	ulData             |= RADEON_CRTC_CRT_ON;
	OUTREG(RADEON_CRTC_EXT_CNTL, ulData);

	ulOrigDAC_EXT_CNTL = INREG(RADEON_DAC_EXT_CNTL);
	ulData             = ulOrigDAC_EXT_CNTL;
	ulData            &= ~RADEON_DAC_FORCE_DATA_MASK;
	ulData            |=  (RADEON_DAC_FORCE_BLANK_OFF_EN
			       |RADEON_DAC_FORCE_DATA_EN
			       |RADEON_DAC_FORCE_DATA_SEL_MASK);
	if ((info->ChipFamily == CHIP_FAMILY_RV250) ||
	    (info->ChipFamily == CHIP_FAMILY_RV280))
	    ulData |= (0x01b6 << RADEON_DAC_FORCE_DATA_SHIFT);
	else
	    ulData |= (0x01ac << RADEON_DAC_FORCE_DATA_SHIFT);

	OUTREG(RADEON_DAC_EXT_CNTL, ulData);

	/* turn on power so testing can go through */
	ulOrigDAC_CNTL = INREG(RADEON_DAC_CNTL);
	ulOrigDAC_CNTL &= ~RADEON_DAC_PDWN;
	OUTREG(RADEON_DAC_CNTL, ulOrigDAC_CNTL);

	ulOrigDAC_MACRO_CNTL = INREG(RADEON_DAC_MACRO_CNTL);
	ulOrigDAC_MACRO_CNTL &= ~(RADEON_DAC_PDWN_R | RADEON_DAC_PDWN_G |
				  RADEON_DAC_PDWN_B);
	OUTREG(RADEON_DAC_MACRO_CNTL, ulOrigDAC_MACRO_CNTL);

	/* Enable comparators and set DAC range to PS2 (VGA) output level */
	ulData = ulOrigDAC_CNTL;
	ulData |= RADEON_DAC_CMP_EN;
	ulData &= ~RADEON_DAC_RANGE_CNTL_MASK;
	ulData |= 0x2;
	OUTREG(RADEON_DAC_CNTL, ulData);

	/* Settle down */
	usleep(10000);

	/* Read comparators */
	ulData     = INREG(RADEON_DAC_CNTL);
	bConnected =  (RADEON_DAC_CMP_OUTPUT & ulData)?1:0;

	/* Restore things */
	ulData    = ulOrigVCLK_ECP_CNTL;
	ulMask    = 0xFFFFFFFFL;
	OUTPLLP(pScrn, RADEON_VCLK_ECP_CNTL, ulData, ulMask);

	OUTREG(RADEON_DAC_CNTL,      ulOrigDAC_CNTL     );
	OUTREG(RADEON_DAC_EXT_CNTL,  ulOrigDAC_EXT_CNTL );
	OUTREG(RADEON_CRTC_EXT_CNTL, ulOrigCRTC_EXT_CNTL);

	if (!bConnected) {
	    /* Power DAC down if CRT is not connected */
            ulOrigDAC_MACRO_CNTL = INREG(RADEON_DAC_MACRO_CNTL);
            ulOrigDAC_MACRO_CNTL |= (RADEON_DAC_PDWN_R | RADEON_DAC_PDWN_G |
	    	RADEON_DAC_PDWN_B);
            OUTREG(RADEON_DAC_MACRO_CNTL, ulOrigDAC_MACRO_CNTL);

	    ulData = INREG(RADEON_DAC_CNTL);
	    ulData |= RADEON_DAC_PDWN;
	    OUTREG(RADEON_DAC_CNTL, ulData);
    	}
    } else { /* TV DAC */

        /* This doesn't seem to work reliably (maybe worse on some OEM cards),
           for now we always return false. If one wants to connected a
           non-DDC monitor on the DVI port when CRT port is also connected,
           he will need to explicitly tell the driver in the config file
           with Option MonitorLayout.
        */
        bConnected = FALSE;

#if 0
	if (info->ChipFamily == CHIP_FAMILY_R200) {
	    unsigned long ulOrigGPIO_MONID;
	    unsigned long ulOrigFP2_GEN_CNTL;
	    unsigned long ulOrigDISP_OUTPUT_CNTL;
	    unsigned long ulOrigCRTC2_GEN_CNTL;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_A;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_B;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_C;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_D;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_E;
	    unsigned long ulOrigDISP_LIN_TRANS_GRPH_F;
	    unsigned long ulOrigCRTC2_H_TOTAL_DISP;
	    unsigned long ulOrigCRTC2_V_TOTAL_DISP;
	    unsigned long ulOrigCRTC2_H_SYNC_STRT_WID;
	    unsigned long ulOrigCRTC2_V_SYNC_STRT_WID;
	    unsigned long ulData, i;

	    ulOrigGPIO_MONID = INREG(RADEON_GPIO_MONID);
	    ulOrigFP2_GEN_CNTL = INREG(RADEON_FP2_GEN_CNTL);
	    ulOrigDISP_OUTPUT_CNTL = INREG(RADEON_DISP_OUTPUT_CNTL);
	    ulOrigCRTC2_GEN_CNTL = INREG(RADEON_CRTC2_GEN_CNTL);
	    ulOrigDISP_LIN_TRANS_GRPH_A = INREG(RADEON_DISP_LIN_TRANS_GRPH_A);
	    ulOrigDISP_LIN_TRANS_GRPH_B = INREG(RADEON_DISP_LIN_TRANS_GRPH_B);
	    ulOrigDISP_LIN_TRANS_GRPH_C = INREG(RADEON_DISP_LIN_TRANS_GRPH_C);
	    ulOrigDISP_LIN_TRANS_GRPH_D = INREG(RADEON_DISP_LIN_TRANS_GRPH_D);
	    ulOrigDISP_LIN_TRANS_GRPH_E = INREG(RADEON_DISP_LIN_TRANS_GRPH_E);
	    ulOrigDISP_LIN_TRANS_GRPH_F = INREG(RADEON_DISP_LIN_TRANS_GRPH_F);

	    ulOrigCRTC2_H_TOTAL_DISP = INREG(RADEON_CRTC2_H_TOTAL_DISP);
	    ulOrigCRTC2_V_TOTAL_DISP = INREG(RADEON_CRTC2_V_TOTAL_DISP);
	    ulOrigCRTC2_H_SYNC_STRT_WID = INREG(RADEON_CRTC2_H_SYNC_STRT_WID);
	    ulOrigCRTC2_V_SYNC_STRT_WID = INREG(RADEON_CRTC2_V_SYNC_STRT_WID);

	    ulData     = INREG(RADEON_GPIO_MONID);
	    ulData    &= ~RADEON_GPIO_A_0;
	    OUTREG(RADEON_GPIO_MONID, ulData);

	    OUTREG(RADEON_FP2_GEN_CNTL, 0x0a000c0c);

	    OUTREG(RADEON_DISP_OUTPUT_CNTL, 0x00000012);

	    OUTREG(RADEON_CRTC2_GEN_CNTL, 0x06000000);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_A, 0x00000000);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_B, 0x000003f0);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_C, 0x00000000);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_D, 0x000003f0);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_E, 0x00000000);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_F, 0x000003f0);
	    OUTREG(RADEON_CRTC2_H_TOTAL_DISP, 0x01000008);
	    OUTREG(RADEON_CRTC2_H_SYNC_STRT_WID, 0x00000800);
	    OUTREG(RADEON_CRTC2_V_TOTAL_DISP, 0x00080001);
	    OUTREG(RADEON_CRTC2_V_SYNC_STRT_WID, 0x00000080);

	    for (i = 0; i < 200; i++) {
		ulData     = INREG(RADEON_GPIO_MONID);
		bConnected = (ulData & RADEON_GPIO_Y_0)?1:0;
		if (!bConnected) break;

		usleep(1000);
	    }

	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_A, ulOrigDISP_LIN_TRANS_GRPH_A);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_B, ulOrigDISP_LIN_TRANS_GRPH_B);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_C, ulOrigDISP_LIN_TRANS_GRPH_C);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_D, ulOrigDISP_LIN_TRANS_GRPH_D);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_E, ulOrigDISP_LIN_TRANS_GRPH_E);
	    OUTREG(RADEON_DISP_LIN_TRANS_GRPH_F, ulOrigDISP_LIN_TRANS_GRPH_F);
	    OUTREG(RADEON_CRTC2_H_TOTAL_DISP, ulOrigCRTC2_H_TOTAL_DISP);
	    OUTREG(RADEON_CRTC2_V_TOTAL_DISP, ulOrigCRTC2_V_TOTAL_DISP);
	    OUTREG(RADEON_CRTC2_H_SYNC_STRT_WID, ulOrigCRTC2_H_SYNC_STRT_WID);
	    OUTREG(RADEON_CRTC2_V_SYNC_STRT_WID, ulOrigCRTC2_V_SYNC_STRT_WID);
	    OUTREG(RADEON_CRTC2_GEN_CNTL, ulOrigCRTC2_GEN_CNTL);
	    OUTREG(RADEON_DISP_OUTPUT_CNTL, ulOrigDISP_OUTPUT_CNTL);
	    OUTREG(RADEON_FP2_GEN_CNTL, ulOrigFP2_GEN_CNTL);
	    OUTREG(RADEON_GPIO_MONID, ulOrigGPIO_MONID);
        } else {
	    unsigned long ulOrigPIXCLKSDATA;
	    unsigned long ulOrigTV_MASTER_CNTL;
	    unsigned long ulOrigTV_DAC_CNTL;
	    unsigned long ulOrigTV_PRE_DAC_MUX_CNTL;
	    unsigned long ulOrigDAC_CNTL2;
	    unsigned long ulData;
	    unsigned long ulMask;

	    ulOrigPIXCLKSDATA = INPLL(pScrn, RADEON_PIXCLKS_CNTL);

	    ulData            = ulOrigPIXCLKSDATA;
	    ulData           &= ~(RADEON_PIX2CLK_ALWAYS_ONb
				  | RADEON_PIX2CLK_DAC_ALWAYS_ONb);
	    ulMask            = ~(RADEON_PIX2CLK_ALWAYS_ONb
			  | RADEON_PIX2CLK_DAC_ALWAYS_ONb);
	    OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, ulData, ulMask);

	    ulOrigTV_MASTER_CNTL = INREG(RADEON_TV_MASTER_CNTL);
	    ulData               = ulOrigTV_MASTER_CNTL;
	    ulData              &= ~RADEON_TVCLK_ALWAYS_ONb;
	    OUTREG(RADEON_TV_MASTER_CNTL, ulData);

	    ulOrigDAC_CNTL2 = INREG(RADEON_DAC_CNTL2);
	    ulData          = ulOrigDAC_CNTL2;
	    ulData          &= ~RADEON_DAC2_DAC2_CLK_SEL;
	    OUTREG(RADEON_DAC_CNTL2, ulData);

	    ulOrigTV_DAC_CNTL = INREG(RADEON_TV_DAC_CNTL);

	    ulData  = 0x00880213;
	    OUTREG(RADEON_TV_DAC_CNTL, ulData);

	    ulOrigTV_PRE_DAC_MUX_CNTL = INREG(RADEON_TV_PRE_DAC_MUX_CNTL);

	    ulData  =  (RADEON_Y_RED_EN
			| RADEON_C_GRN_EN
			| RADEON_CMP_BLU_EN
			| RADEON_RED_MX_FORCE_DAC_DATA
			| RADEON_GRN_MX_FORCE_DAC_DATA
			| RADEON_BLU_MX_FORCE_DAC_DATA);
            if (IS_R300_VARIANT)
		ulData |= 0x180 << RADEON_TV_FORCE_DAC_DATA_SHIFT;
	    else
		ulData |= 0x1f5 << RADEON_TV_FORCE_DAC_DATA_SHIFT;
	    OUTREG(RADEON_TV_PRE_DAC_MUX_CNTL, ulData);

	    usleep(10000);

	    ulData     = INREG(RADEON_TV_DAC_CNTL);
	    bConnected = (ulData & RADEON_TV_DAC_CMPOUT)?1:0;

	    ulData    = ulOrigPIXCLKSDATA;
	    ulMask    = 0xFFFFFFFFL;
	    OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, ulData, ulMask);

	    OUTREG(RADEON_TV_MASTER_CNTL, ulOrigTV_MASTER_CNTL);
	    OUTREG(RADEON_DAC_CNTL2, ulOrigDAC_CNTL2);
	    OUTREG(RADEON_TV_DAC_CNTL, ulOrigTV_DAC_CNTL);
	    OUTREG(RADEON_TV_PRE_DAC_MUX_CNTL, ulOrigTV_PRE_DAC_MUX_CNTL);
	}
#endif
    }

    return(bConnected ? MT_CRT : MT_NONE);
}


static RADEONMonitorType RADEONDisplayDDCConnected(ScrnInfoPtr pScrn, RADEONDDCType DDCType, RADEONConnector* port)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long DDCReg;
    RADEONMonitorType MonType = MT_NONE;
    xf86MonPtr* MonInfo = &port->MonInfo;
    int i, j;

    DDCReg = info->DDCReg;
    switch(DDCType)
    {
    case DDC_MONID:
	info->DDCReg = RADEON_GPIO_MONID;
	break;
    case DDC_DVI:
	info->DDCReg = RADEON_GPIO_DVI_DDC;
	break;
    case DDC_VGA:
	info->DDCReg = RADEON_GPIO_VGA_DDC;
	break;
    case DDC_CRT2:
	info->DDCReg = RADEON_GPIO_CRT2_DDC;
	break;
    case DDC_LCD:
	info->DDCReg = RADEON_LCD_GPIO_MASK;
	break;
    default:
	info->DDCReg = DDCReg;
	return MT_NONE;
    }

    /* Read and output monitor info using DDC2 over I2C bus */
    if (info->pI2CBus && info->ddc2 && (info->DDCReg != RADEON_LCD_GPIO_MASK)) {
	OUTREG(info->DDCReg, INREG(info->DDCReg) &
	       (CARD32)~(RADEON_GPIO_A_0 | RADEON_GPIO_A_1));

	/* For some old monitors (like Compaq Presario FP500), we need
	 * following process to initialize/stop DDC
	 */
	OUTREG(info->DDCReg, INREG(info->DDCReg) & ~(RADEON_GPIO_EN_1));
	for (j = 0; j < 3; j++) {
	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_0));
	    usleep(13000);

	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_1));
	    for (i = 0; i < 10; i++) {
		usleep(15000);
		if (INREG(info->DDCReg) & RADEON_GPIO_Y_1)
		    break;
	    }
	    if (i == 10) continue;

	    usleep(15000);

	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_0);
	    usleep(15000);

	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_1);
	    usleep(15000);
	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_0));
	    usleep(15000);
	    *MonInfo = xf86DoEDID_DDC2(pScrn->scrnIndex, info->pI2CBus);

	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_1);
	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_0);
	    usleep(15000);
	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_1));
	    for (i = 0; i < 5; i++) {
		usleep(15000);
		if (INREG(info->DDCReg) & RADEON_GPIO_Y_1)
		    break;
	    }
	    usleep(15000);
	    OUTREG(info->DDCReg,
		   INREG(info->DDCReg) & ~(RADEON_GPIO_EN_0));
	    usleep(15000);

	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_1);
	    OUTREG(info->DDCReg, INREG(info->DDCReg) | RADEON_GPIO_EN_0);
	    usleep(15000);
	    if(*MonInfo)  break;
	}
    } else if (info->pI2CBus && info->ddc2 && info->DDCReg == RADEON_LCD_GPIO_MASK) {
         *MonInfo = xf86DoEDID_DDC2(pScrn->scrnIndex, info->pI2CBus);
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "DDC2/I2C is not properly initialized\n");
	MonType = MT_NONE;
    }

    OUTREG(info->DDCReg, INREG(info->DDCReg) &
        ~(RADEON_GPIO_EN_0 | RADEON_GPIO_EN_1));

    if (*MonInfo) {
	if ((*MonInfo)->rawData[0x14] & 0x80) {
	    /* Note some laptops have a DVI output that uses internal TMDS,
	     * when its DVI is enabled by hotkey, LVDS panel is not used.
	     * In this case, the laptop is configured as DVI+VGA as a normal 
	     * desktop card.
	     * Also for laptop, when X starts with lid closed (no DVI connection)
	     * both LDVS and TMDS are disable, we still need to treat it as a LVDS panel.
	     */
	    if (port->TMDSType == TMDS_EXT) MonType = MT_DFP;
	    else {
		if ((INREG(RADEON_FP_GEN_CNTL) & RADEON_FP_EN_TMDS) || !info->IsMobility)
		    MonType = MT_DFP;
		else 
		    MonType = MT_LCD;
	    }
	} else MonType = MT_CRT;
    } else MonType = MT_NONE;

    info->DDCReg = DDCReg;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "DDC Type: %d, Detected Type: %d\n", DDCType, MonType);

    return MonType;
}

static void RADEONGetPanelInfoFromReg (ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info     = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 fp_vert_stretch = INREG(RADEON_FP_VERT_STRETCH);
    CARD32 fp_horz_stretch = INREG(RADEON_FP_HORZ_STRETCH);

    info->PanelPwrDly = 200;
    if (fp_vert_stretch & RADEON_VERT_STRETCH_ENABLE) {
	info->PanelYRes = ((fp_vert_stretch & RADEON_VERT_PANEL_SIZE) >>
			   RADEON_VERT_PANEL_SHIFT) + 1;
    } else {
	info->PanelYRes = (INREG(RADEON_CRTC_V_TOTAL_DISP)>>16) + 1;
    }
    if (fp_horz_stretch & RADEON_HORZ_STRETCH_ENABLE) {
	info->PanelXRes = (((fp_horz_stretch & RADEON_HORZ_PANEL_SIZE) >>
			    RADEON_HORZ_PANEL_SHIFT) + 1) * 8;
    } else {
	info->PanelXRes = ((INREG(RADEON_CRTC_H_TOTAL_DISP)>>16) + 1) * 8;
    }
    
    if ((info->PanelXRes < 640) || (info->PanelYRes < 480)) {
	info->PanelXRes = 640;
	info->PanelYRes = 480;
    }

    if (xf86ReturnOptValBool(info->Options, OPTION_LVDS_PROBE_PLL, TRUE)) {
           CARD32 ppll_div_sel, ppll_val;

           ppll_div_sel = INREG8(RADEON_CLOCK_CNTL_INDEX + 1) & 0x3;
	   RADEONPllErrataAfterIndex(info);
	   ppll_val = INPLL(pScrn, RADEON_PPLL_DIV_0 + ppll_div_sel);
           if ((ppll_val & 0x000707ff) == 0x1bb)
		   goto noprobe;
	   info->FeedbackDivider = ppll_val & 0x7ff;
	   info->PostDivider = (ppll_val >> 16) & 0x7;
	   info->RefDivider = info->pll.reference_div;
	   info->UseBiosDividers = TRUE;

           xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                      "Existing panel PLL dividers will be used.\n");
    }
 noprobe:

    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
	       "Panel size %dx%d is derived, this may not be correct.\n"
		   "If not, use PanelSize option to overwrite this setting\n",
	       info->PanelXRes, info->PanelYRes);
}


/* BIOS may not have right panel size, we search through all supported
 * DDC modes looking for the maximum panel size.
 */
static void RADEONUpdatePanelSize(ScrnInfoPtr pScrn)
{
    int             j;
    RADEONInfoPtr   info = RADEONPTR (pScrn);
    xf86MonPtr      ddc  = pScrn->monitor->DDC;
    DisplayModePtr  p;

    if ((info->UseBiosDividers && info->DotClock != 0) || (ddc == NULL))
       return;

    /* Go thru detailed timing table first */
    for (j = 0; j < 4; j++) {
	if (ddc->det_mon[j].type == 0) {
	    struct detailed_timings *d_timings =
		&ddc->det_mon[j].section.d_timings;
           int match = 0;

           /* If we didn't get a panel clock or guessed one, try to match the
            * mode with the panel size. We do that because we _need_ a panel
            * clock, or ValidateFPModes will fail, even when UseBiosDividers
            * is set.
            */
           if (info->DotClock == 0 &&
               info->PanelXRes == d_timings->h_active &&
               info->PanelYRes == d_timings->v_active)
               match = 1;

           /* If we don't have a BIOS provided panel data with fixed dividers,
            * check for a larger panel size
            */
	    if (info->PanelXRes < d_timings->h_active &&
               info->PanelYRes < d_timings->v_active &&
               !info->UseBiosDividers)
               match = 1;

             if (match) {
		info->PanelXRes  = d_timings->h_active;
		info->PanelYRes  = d_timings->v_active;
		info->DotClock   = d_timings->clock / 1000;
		info->HOverPlus  = d_timings->h_sync_off;
		info->HSyncWidth = d_timings->h_sync_width;
		info->HBlank     = d_timings->h_blanking;
		info->VOverPlus  = d_timings->v_sync_off;
		info->VSyncWidth = d_timings->v_sync_width;
		info->VBlank     = d_timings->v_blanking;
                info->Flags      = (d_timings->interlaced ? V_INTERLACE : 0);
                switch (d_timings->misc) {
                case 0: info->Flags |= V_NHSYNC | V_NVSYNC; break;
                case 1: info->Flags |= V_PHSYNC | V_NVSYNC; break;
                case 2: info->Flags |= V_NHSYNC | V_PVSYNC; break;
                case 3: info->Flags |= V_PHSYNC | V_PVSYNC; break;
                }
                xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel infos found from DDC detailed: %dx%d\n",
                           info->PanelXRes, info->PanelYRes);
	    }
	}
    }

    if (info->UseBiosDividers && info->DotClock != 0)
       return;

    /* Search thru standard VESA modes from EDID */
    for (j = 0; j < 8; j++) {
	if ((info->PanelXRes < ddc->timings2[j].hsize) &&
	    (info->PanelYRes < ddc->timings2[j].vsize)) {
	    for (p = pScrn->monitor->Modes; p; p = p->next) {
		if ((ddc->timings2[j].hsize == p->HDisplay) &&
		    (ddc->timings2[j].vsize == p->VDisplay)) {
		    float  refresh =
			(float)p->Clock * 1000.0 / p->HTotal / p->VTotal;

		    if (abs((float)ddc->timings2[j].refresh - refresh) < 1.0) {
			/* Is this good enough? */
			info->PanelXRes  = ddc->timings2[j].hsize;
			info->PanelYRes  = ddc->timings2[j].vsize;
			info->HBlank     = p->HTotal - p->HDisplay;
			info->HOverPlus  = p->HSyncStart - p->HDisplay;
			info->HSyncWidth = p->HSyncEnd - p->HSyncStart;
			info->VBlank     = p->VTotal - p->VDisplay;
			info->VOverPlus  = p->VSyncStart - p->VDisplay;
			info->VSyncWidth = p->VSyncEnd - p->VSyncStart;
			info->DotClock   = p->Clock;
                        info->Flags      = p->Flags;
                        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel infos found from DDC VESA/EDID: %dx%d\n",
                                   info->PanelXRes, info->PanelYRes);
		    }
		}
	    }
	}
    }
}

static Bool RADEONGetLVDSInfo (ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info     = RADEONPTR(pScrn);

    if (!RADEONGetLVDSInfoFromBIOS(pScrn))
        RADEONGetPanelInfoFromReg(pScrn);

    /* The panel size we collected from BIOS may not be the
     * maximum size supported by the panel.  If not, we update
     * it now.  These will be used if no matching mode can be
     * found from EDID data.
     */
    RADEONUpdatePanelSize(pScrn);

    if (info->DotClock == 0) {
	RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
	DisplayModePtr  tmp_mode = NULL;
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "No valid timing info from BIOS.\n");
	/* No timing information for the native mode,
	   use whatever specified in the Modeline.
	   If no Modeline specified, we'll just pick
	   the VESA mode at 60Hz refresh rate which
	   is likely to be the best for a flat panel.
	*/
	tmp_mode = pScrn->monitor->Modes;
	while(tmp_mode) {
	    if ((tmp_mode->HDisplay == info->PanelXRes) &&
		(tmp_mode->VDisplay == info->PanelYRes)) {
		    
		float  refresh =
		    (float)tmp_mode->Clock * 1000.0 / tmp_mode->HTotal / tmp_mode->VTotal;
		if ((abs(60.0 - refresh) < 1.0) ||
		    (tmp_mode->type == 0)) {
		    info->HBlank     = tmp_mode->HTotal - tmp_mode->HDisplay;
		    info->HOverPlus  = tmp_mode->HSyncStart - tmp_mode->HDisplay;
		    info->HSyncWidth = tmp_mode->HSyncEnd - tmp_mode->HSyncStart;
		    info->VBlank     = tmp_mode->VTotal - tmp_mode->VDisplay;
		    info->VOverPlus  = tmp_mode->VSyncStart - tmp_mode->VDisplay;
		    info->VSyncWidth = tmp_mode->VSyncEnd - tmp_mode->VSyncStart;
		    info->DotClock   = tmp_mode->Clock;
		    info->Flags = 0;
		    break;
		}
	    }

	    tmp_mode = tmp_mode->next;

	    if (tmp_mode == pScrn->monitor->Modes)
		break;
	}
	if ((info->DotClock == 0) && !pRADEONEnt->PortInfo[0]->MonInfo) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Panel size is not correctly detected.\n"
		       "Please try to use PanelSize option for correct settings.\n");
	    return FALSE;
	}
    }

    return TRUE;
}

static void RADEONGetTMDSInfo(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    int i;

    for (i=0; i<4; i++) {
        info->tmds_pll[i].value = 0;
        info->tmds_pll[i].freq = 0;
    }

    if (RADEONGetTMDSInfoFromBIOS(pScrn)) return;

    for (i=0; i<4; i++) {
        info->tmds_pll[i].value = default_tmds_pll[info->ChipFamily][i].value;
        info->tmds_pll[i].freq = default_tmds_pll[info->ChipFamily][i].freq;
    }
}

void RADEONGetPanelInfo (ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info     = RADEONPTR(pScrn);
    char* s;

    if((s = xf86GetOptValString(info->Options, OPTION_PANEL_SIZE))) {
        info->PanelPwrDly = 200;
        if (sscanf (s, "%dx%d", &info->PanelXRes, &info->PanelYRes) != 2) {
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Invalid PanelSize option: %s\n", s);
            RADEONGetPanelInfoFromReg(pScrn);
        }
    } else {

        if(info->DisplayType == MT_LCD) {
            RADEONGetLVDSInfo(pScrn);
	    if (info->MergeType == MT_DFP) {
		RADEONGetTMDSInfo(pScrn);
	    }
        } else if ((info->DisplayType == MT_DFP) || (info->MergeType == MT_DFP)) {
            RADEONGetTMDSInfo(pScrn);
            if (!pScrn->monitor->DDC)
                RADEONGetHardCodedEDIDFromBIOS(pScrn);
            else if (!info->IsSecondary)
               RADEONUpdatePanelSize(pScrn);
        }
    }
}

void RADEONGetTVDacAdjInfo(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    
    /* Todo: get this setting from BIOS */
    info->tv_dac_adj = default_tvdac_adj[info->ChipFamily];
    if (info->IsMobility) { /* some mobility chips may different */
	if (info->ChipFamily == CHIP_FAMILY_RV250)
	    info->tv_dac_adj = 0x00880000;
    }
}

static void
RADEONConnectorReverse(RADEONEntPtr pRADEONEnt)
{
    RADEONConnector *connector;

    connector = pRADEONEnt->PortInfo[0];
    pRADEONEnt->PortInfo[0] = pRADEONEnt->PortInfo[1];
    pRADEONEnt->PortInfo[1] = connector;
}

/*
 * initialise the static data sos we don't have to re-do at randr change */
void RADEONSetupConnectors(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    const char *s;
    Bool ignore_edid = FALSE;
    int i = 0, second = 0, max_mt = 5;

    /* We first get the information about all connectors from BIOS.
     * This is how the card is phyiscally wired up.
     * The information should be correct even on a OEM card.
     * If not, we may have problem -- need to use MonitorLayout option.
     */
    for (i = 0; i < 2; i++) {
	pRADEONEnt->PortInfo[i]->MonType = MT_UNKNOWN;
	pRADEONEnt->PortInfo[i]->MonInfo = NULL;
	pRADEONEnt->PortInfo[i]->DDCType = DDC_NONE_DETECTED;
	pRADEONEnt->PortInfo[i]->DACType = DAC_UNKNOWN;
	pRADEONEnt->PortInfo[i]->TMDSType = TMDS_UNKNOWN;
	pRADEONEnt->PortInfo[i]->ConnectorType = CONNECTOR_NONE;
    }
    pRADEONEnt->Controller[0]->IsUsed = FALSE;
    pRADEONEnt->Controller[1]->IsUsed = FALSE;
    pRADEONEnt->Controller[0]->IsActive = FALSE;
    pRADEONEnt->Controller[1]->IsActive = FALSE;

    if (!RADEONGetConnectorInfoFromBIOS(pScrn) ||
        ((pRADEONEnt->PortInfo[0]->DDCType == 0) &&
        (pRADEONEnt->PortInfo[1]->DDCType == 0))) {
	/* Below is the most common setting, but may not be true */
	pRADEONEnt->PortInfo[0]->MonType = MT_UNKNOWN;
	pRADEONEnt->PortInfo[0]->MonInfo = NULL;
	pRADEONEnt->PortInfo[0]->DDCType = DDC_DVI;
	pRADEONEnt->PortInfo[0]->DACType = DAC_TVDAC;
	pRADEONEnt->PortInfo[0]->TMDSType = TMDS_INT;
	pRADEONEnt->PortInfo[0]->ConnectorType = CONNECTOR_DVI_I;

	pRADEONEnt->PortInfo[1]->MonType = MT_UNKNOWN;
	pRADEONEnt->PortInfo[1]->MonInfo = NULL;
	pRADEONEnt->PortInfo[1]->DDCType = DDC_VGA;
	pRADEONEnt->PortInfo[1]->DACType = DAC_PRIMARY;
	pRADEONEnt->PortInfo[1]->TMDSType = TMDS_EXT;
	pRADEONEnt->PortInfo[1]->ConnectorType = CONNECTOR_CRT;


       /* Some cards have the DDC lines swapped and we have no way to
        * detect it yet (Mac cards)
        */
       if (xf86ReturnOptValBool(info->Options, OPTION_REVERSE_DDC, FALSE)) {
           pRADEONEnt->PortInfo[0]->DDCType = DDC_VGA;
           pRADEONEnt->PortInfo[1]->DDCType = DDC_DVI;
        }
    }

    /* always make TMDS_INT port first*/
    if (pRADEONEnt->PortInfo[1]->TMDSType == TMDS_INT)
	RADEONConnectorReverse(pRADEONEnt);
    else if ((pRADEONEnt->PortInfo[0]->TMDSType != TMDS_INT &&
                pRADEONEnt->PortInfo[1]->TMDSType != TMDS_INT)) {
        /* no TMDS_INT port, make primary DAC port first */
	/* On my Inspiron 8600 both internal and external ports are
	   marked DAC_PRIMARY in BIOS. So be extra careful - only
	   swap when the first port is not DAC_PRIMARY */
        if ((!(pRADEONEnt->PortInfo[0]->ConnectorType == CONNECTOR_PROPRIETARY)) &&  (pRADEONEnt->PortInfo[1]->DACType == DAC_PRIMARY) &&
	     (pRADEONEnt->PortInfo[0]->DACType != DAC_PRIMARY)) {
	    RADEONConnectorReverse(pRADEONEnt);
        }
    }

    if (info->HasSingleDAC) {
        /* For RS300/RS350/RS400 chips, there is no primary DAC. Force VGA port to use TVDAC*/
        if (pRADEONEnt->PortInfo[0]->ConnectorType == CONNECTOR_CRT) {
            pRADEONEnt->PortInfo[0]->DACType = DAC_TVDAC;
            pRADEONEnt->PortInfo[1]->DACType = DAC_PRIMARY;
        } else {
            pRADEONEnt->PortInfo[1]->DACType = DAC_TVDAC;
            pRADEONEnt->PortInfo[0]->DACType = DAC_PRIMARY;
        }
    } else if (!pRADEONEnt->HasCRTC2) {
        pRADEONEnt->PortInfo[0]->DACType = DAC_PRIMARY;
    }

    /* IgnoreEDID option is different from the NoDDCxx options used by DDC module
     * When IgnoreEDID is used, monitor detection will still use DDC
     * detection, but all EDID data will not be used in mode validation.
     * You can use this option when you have a DDC monitor but want specify your own
     * monitor timing parameters by using HSync, VRefresh and Modeline,
     */
    if (xf86GetOptValBool(info->Options, OPTION_IGNORE_EDID, &ignore_edid)) {
        if (ignore_edid)
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "IgnoreEDID is specified, EDID data will be ignored\n");
    }

    /*
     * MonitorLayout option takes a string for two monitors connected in following format:
     * Option "MonitorLayout" "primary-port-display, secondary-port-display"
     * primary and secondary port displays can have one of following:
     *    NONE, CRT, LVDS, TMDS
     * With this option, driver will bring up monitors as specified,
     * not using auto-detection routines to probe monitors.
     *
     * This option can be used when the false monitor detection occurs.
     *
     * This option can also be used to disable one connected display.
     * For example, if you have a laptop connected to an external CRT
     * and you want to disable the internal LCD panel, you can specify
     * Option "MonitorLayout" "NONE, CRT"
     *
     * This option can also used to disable Clone mode. One there is only
     * one monitor is specified, clone mode will be turned off automatically
     * even you have two monitors connected.
     *
     * Another usage of this option is you want to config the server
     * to start up with a certain monitor arrangement even one monitor
     * is not plugged in when server starts.
     * For example, you can config your laptop with 
     * Option "MonitorLayout" "LVDS, CRT"
     * Option "CloneHSync" "40-150"
     * Option "CloneVRefresh" "60-120"
     * With these options, you can connect in your CRT monitor later
     * after the X server has started.
     */
    if ((s = xf86GetOptValString(info->Options, OPTION_MONITOR_LAYOUT))) {
        char s1[5], s2[5];
        i = 0;
        /* When using user specified monitor types, we will not do DDC detection
         *
         */
        do {
            switch(*s) {
            case ',':
                s1[i] = '\0';
                i = 0;
                second = 1;
                break;
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                break;
            default:
                if (second)
                    s2[i] = *s;
                else
                    s1[i] = *s;
                i++;
                break;
            }
            if (i > 4) i = 4;
        } while(*s++);
        s2[i] = '\0';

        for (i = 0; i < max_mt; i++)
        {
            if (strcmp(s1, MonTypeName[i]) == 0) 
            {
                pRADEONEnt->PortInfo[0]->MonType = MonTypeID[i];
                break;
            }
        }
        if (i ==  max_mt)
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
                       "Invalid Monitor type specified for 1st port \n"); 

        for (i = 0; i < max_mt; i++)
        {
            if (strcmp(s2, MonTypeName[i]) == 0) 
            {
                pRADEONEnt->PortInfo[1]->MonType = MonTypeID[i];
                break;
            }

        }
        if (i ==  max_mt)
            xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
                       "Invalid Monitor type specified for 2nd port \n"); 

	if (i ==  max_mt)
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Invalid Monitor type specified for 2nd port \n");

	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "MonitorLayout Option: \n\tMonitor1--Type %s, Monitor2--Type %s\n\n", s1, s2);
#if 0
	if (pRADEONEnt->PortInfo[1]->MonType == MT_CRT) {
	    pRADEONEnt->PortInfo[1]->DACType = DAC_PRIMARY;
	    pRADEONEnt->PortInfo[1]->TMDSType = TMDS_UNKNOWN;
	    pRADEONEnt->PortInfo[1]->DDCType = DDC_VGA;
	    pRADEONEnt->PortInfo[1]->ConnectorType = CONNECTOR_CRT;
	    pRADEONEnt->PortInfo[0]->DACType = DAC_TVDAC;
	    pRADEONEnt->PortInfo[0]->TMDSType = TMDS_UNKNOWN;
	    pRADEONEnt->PortInfo[0]->DDCType = DDC_NONE_DETECTED;
	    pRADEONEnt->PortInfo[0]->ConnectorType = pRADEONEnt->PortInfo[0]->MonType+1;
	    pRADEONEnt->PortInfo[0]->MonInfo = NULL;
        }
#endif

        /* some thinkpads and powerbooks use lvds and internal tmds 
	 * at the same time.  --AGD
	 */
	if ((pRADEONEnt->PortInfo[0]->MonType  == MT_LCD) &&
	    (pRADEONEnt->PortInfo[1]->MonType == MT_DFP)) {
	    pRADEONEnt->PortInfo[1]->DDCType = DDC_DVI;
	    pRADEONEnt->PortInfo[0]->DDCType = DDC_MONID;
            pRADEONEnt->PortInfo[1]->TMDSType = TMDS_INT;
            pRADEONEnt->PortInfo[1]->ConnectorType = CONNECTOR_DVI_I;
            pRADEONEnt->PortInfo[0]->TMDSType = TMDS_UNKNOWN;
	}

    }
}

static RADEONMonitorType RADEONPortCheckNonDDC(ScrnInfoPtr pScrn, int connector)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    if (info->IsMobility) {
      switch(connector) {
      case 0:
	/* non-DDC laptop panel connected on primary */
	if (INREG(RADEON_BIOS_4_SCRATCH) & 4)
	  return MT_LCD;
	break;
      case 1:
	/* non-DDC TMDS panel connected through DVO */
	if (INREG(RADEON_FP2_GEN_CNTL) & RADEON_FP2_ON)
	  return MT_DFP;
	break;
      default:
	break;
      }
    }
    return MT_NONE;
}

/* Primary Head (DVI or Laptop Int. panel)*/
/* A ddc capable display connected on DVI port */
/* Secondary Head (mostly VGA, can be DVI on some OEM boards)*/
void RADEONConnectorFindMonitor(ScrnInfoPtr pScrn, int connector)
{
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    RADEONConnector *pPort = pRADEONEnt->PortInfo[connector];
    
    if (pPort->MonType == MT_UNKNOWN) {
      if ((pPort->MonType = RADEONDisplayDDCConnected(pScrn,
						     pPort->DDCType,
						     pPort)));
      else if((pPort->MonType = RADEONPortCheckNonDDC(pScrn, connector)));
      else
	pPort->MonType = RADEONCrtIsPhysicallyConnected(pScrn, !(pPort->DACType));
    }
}

static void RADEONQueryConnectedDisplays(ScrnInfoPtr pScrn)
{

    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    const char *s;
    Bool ignore_edid = FALSE;

    /* IgnoreEDID option is different from the NoDDCxx options used by DDC module
     * When IgnoreEDID is used, monitor detection will still use DDC
     * detection, but all EDID data will not be used in mode validation.
     * You can use this option when you have a DDC monitor but want specify your own
     * monitor timing parameters by using HSync, VRefresh and Modeline,
     */
    if (xf86GetOptValBool(info->Options, OPTION_IGNORE_EDID, &ignore_edid)) {
        if (ignore_edid)
            xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
                       "IgnoreEDID is specified, EDID data will be ignored\n");
    }

    if ((s = xf86GetOptValString(info->Options, OPTION_MONITOR_LAYOUT))) {
        if (!ignore_edid) {
            if ((pRADEONEnt->PortInfo[0]->MonType > MT_NONE) &&
                (pRADEONEnt->PortInfo[0]->MonType < MT_STV))
		RADEONDisplayDDCConnected(pScrn, pRADEONEnt->PortInfo[0]->DDCType,
					  pRADEONEnt->PortInfo[0]);
            if ((pRADEONEnt->PortInfo[1]->MonType > MT_NONE) &&
                (pRADEONEnt->PortInfo[1]->MonType < MT_STV))
		RADEONDisplayDDCConnected(pScrn, pRADEONEnt->PortInfo[1]->DDCType,
					  pRADEONEnt->PortInfo[1]);
        }
    }
    else {
      /* force monitor redetection */
      pRADEONEnt->PortInfo[0]->MonType = MT_UNKNOWN;
      pRADEONEnt->PortInfo[1]->MonType = MT_UNKNOWN;
    }
      
    
    if (pRADEONEnt->PortInfo[0]->MonType == MT_UNKNOWN || pRADEONEnt->PortInfo[1]->MonType == MT_UNKNOWN) {
	
        if ((!pRADEONEnt->HasCRTC2) && (pRADEONEnt->PortInfo[0]->MonType == MT_UNKNOWN)) {
	    if((pRADEONEnt->PortInfo[0]->MonType = RADEONDisplayDDCConnected(pScrn, DDC_DVI,
									     pRADEONEnt->PortInfo[0])));
	    else if((pRADEONEnt->PortInfo[0]->MonType = RADEONDisplayDDCConnected(pScrn, DDC_VGA,
										  pRADEONEnt->PortInfo[0])));
	    else if((pRADEONEnt->PortInfo[0]->MonType = RADEONDisplayDDCConnected(pScrn, DDC_CRT2,
										  pRADEONEnt->PortInfo[0])));
	    else
		pRADEONEnt->PortInfo[0]->MonType = MT_CRT;
	    
	    if (!ignore_edid) {
		if (pRADEONEnt->PortInfo[0]->MonInfo) {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Monitor1 EDID data ---------------------------\n");
		    xf86PrintEDID(pRADEONEnt->PortInfo[0]->MonInfo );
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "End of Monitor1 EDID data --------------------\n");
		}
	    }
	    
	    pRADEONEnt->PortInfo[1]->MonType = MT_NONE;
	    pRADEONEnt->PortInfo[1]->MonInfo = NULL;
	    pRADEONEnt->PortInfo[1]->DDCType = DDC_NONE_DETECTED;
	    pRADEONEnt->PortInfo[1]->DACType = DAC_UNKNOWN;
	    pRADEONEnt->PortInfo[1]->TMDSType = TMDS_UNKNOWN;
	    pRADEONEnt->PortInfo[1]->ConnectorType = CONNECTOR_NONE;

	    return;
	}
	
	RADEONConnectorFindMonitor(pScrn, 0);
	RADEONConnectorFindMonitor(pScrn, 1);
	
    }

    if(ignore_edid) {
        pRADEONEnt->PortInfo[0]->MonInfo = NULL;
        pRADEONEnt->PortInfo[1]->MonInfo = NULL;
    } else {
        if (pRADEONEnt->PortInfo[0]->MonInfo) {
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID data from the display on 1st port ----------------------\n");
            xf86PrintEDID( pRADEONEnt->PortInfo[0]->MonInfo );
        }

        if (pRADEONEnt->PortInfo[1]->MonInfo) {
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID data from the display on 2nd port -----------------------\n");
            xf86PrintEDID( pRADEONEnt->PortInfo[1]->MonInfo );
        }
    }
    
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "\n");

    return;
}

Bool RADEONMapControllers(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    Bool head_reversed = FALSE;

    info->MergeType = MT_NONE;

    if (!info->IsSecondary) {
      RADEONQueryConnectedDisplays(pScrn);

      xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		 "Port1:\n Monitor   -- %s\n Connector -- %s\n DAC Type  -- %s\n TMDS Type -- %s\n DDC Type  -- %s\n", 
		 MonTypeName[pRADEONEnt->PortInfo[0]->MonType+1], 
		 info->IsAtomBios ? 
		 ConnectorTypeNameATOM[pRADEONEnt->PortInfo[0]->ConnectorType]:
		 ConnectorTypeName[pRADEONEnt->PortInfo[0]->ConnectorType],
		 DACTypeName[pRADEONEnt->PortInfo[0]->DACType+1],
		 TMDSTypeName[pRADEONEnt->PortInfo[0]->TMDSType+1],
		 DDCTypeName[pRADEONEnt->PortInfo[0]->DDCType]);

      xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		 "Port2:\n Monitor   -- %s\n Connector -- %s\n DAC Type  -- %s\n TMDS Type -- %s\n DDC Type  -- %s\n", 
		 MonTypeName[pRADEONEnt->PortInfo[1]->MonType+1], 
		 info->IsAtomBios ? 
		 ConnectorTypeNameATOM[pRADEONEnt->PortInfo[1]->ConnectorType]:
		 ConnectorTypeName[pRADEONEnt->PortInfo[1]->ConnectorType],
		 DACTypeName[pRADEONEnt->PortInfo[1]->DACType+1],
		 TMDSTypeName[pRADEONEnt->PortInfo[1]->TMDSType+1],
		 		 DDCTypeName[pRADEONEnt->PortInfo[1]->DDCType]);

	/* no display detected on primary port*/
	if (pRADEONEnt->PortInfo[0]->MonType == MT_NONE) {
	    if (pRADEONEnt->PortInfo[1]->MonType != MT_NONE) {
		/* Only one detected on secondary, let it to be primary */
		if (!head_reversed)
		    RADEONConnectorReverse(pRADEONEnt);
		head_reversed = TRUE;
	    } else {
		/* None detected, Default to a CRT connected */
		pRADEONEnt->PortInfo[0]->MonType = MT_CRT;
	    }
	}

	if ((pRADEONEnt->PortInfo[0]->MonType == MT_LCD) &&
	    (pRADEONEnt->PortInfo[1]->MonType == MT_CRT)) {
	    if (!(INREG(RADEON_LVDS_GEN_CNTL) & RADEON_LVDS_ON)) {
		/* LCD is switched off, don't turn it on, otherwise it may casue lockup due to SS issue. */
		if (!head_reversed)
		    RADEONConnectorReverse(pRADEONEnt);
		head_reversed = TRUE;
		pRADEONEnt->PortInfo[0]->MonType = MT_NONE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "LCD is switched off, only CRT will be used\n");
	    }
	}

	if ((pRADEONEnt->PortInfo[0]->MonType != MT_NONE) &&
	    (pRADEONEnt->PortInfo[1]->MonType != MT_NONE)) {
	  if (xf86ReturnOptValBool(info->Options, OPTION_REVERSE_DISPLAY, FALSE)) {
		if (info->IsMobility) {
		    /* Don't reverse display for mobility chips, as only CRTC1 path has RMX which
		       will be required by many LCD panels
		    */
		    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Reverse Display cannot be used for mobility chip\n");
		} else {
		    if (!head_reversed)
			RADEONConnectorReverse(pRADEONEnt);
		    head_reversed = TRUE;
		    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Primary and Secondary mapping is reversed\n");
		}
	    }
	}

	if (pRADEONEnt->HasSecondary && pRADEONEnt->PortInfo[1]->MonType == MT_NONE) {
	    pRADEONEnt->HasSecondary = FALSE;
	}
    }

    if(pRADEONEnt->HasCRTC2) {
	if(info->IsSecondary) {
  	    pRADEONEnt->Controller[1]->binding = 2;
	    info->DisplayType = pRADEONEnt->PortInfo[1]->MonType;
	    pScrn->monitor->DDC = pRADEONEnt->PortInfo[1]->MonInfo;
	} else {
  	    pRADEONEnt->Controller[0]->binding = 1;
	    info->DisplayType = pRADEONEnt->PortInfo[0]->MonType;
	    pScrn->monitor->DDC = pRADEONEnt->PortInfo[0]->MonInfo;
	}

	if(!pRADEONEnt->HasSecondary) {
	    info->MergeType = pRADEONEnt->PortInfo[1]->MonType;
	    if (info->MergeType)
  	    	pRADEONEnt->Controller[1]->binding = 1;
	}
    } else {
	if (pRADEONEnt->PortInfo[0]->MonType == MT_NONE)
	    pRADEONEnt->PortInfo[0]->MonType = MT_CRT;
	info->DisplayType = pRADEONEnt->PortInfo[0]->MonType;
	pScrn->monitor->DDC = pRADEONEnt->PortInfo[0]->MonInfo;

	pRADEONEnt->PortInfo[1]->MonType = MT_NONE;
	pRADEONEnt->Controller[1]->binding = 1;
    }

    if (!info->IsSecondary) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "---- Primary Head:   Port%d ---- \n", head_reversed?2:1);
	if (pRADEONEnt->PortInfo[1]->MonType != MT_NONE)
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "---- Secondary Head: Port%d ----\n", head_reversed?1:2);
 	else
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "---- Secondary Head: Not used ----\n");
    }

    info->HBlank     = 0;
    info->HOverPlus  = 0;
    info->HSyncWidth = 0;
    info->VBlank     = 0;
    info->VOverPlus  = 0;
    info->VSyncWidth = 0;
    info->DotClock   = 0;
    info->UseBiosDividers = FALSE;

    info->OverlayOnCRTC2 = FALSE;

    return TRUE;
}

/*
 * Powering done DAC, needed for DPMS problem with ViewSonic P817 (or its variant).
 *
 */
static void RADEONDacPowerSet(ScrnInfoPtr pScrn, Bool IsOn, Bool IsPrimaryDAC)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    if (IsPrimaryDAC) {
	CARD32 dac_cntl;
	CARD32 dac_macro_cntl = 0;
	dac_cntl = INREG(RADEON_DAC_CNTL);
	if ((!info->IsMobility) || (info->ChipFamily == CHIP_FAMILY_RV350)) 
	    dac_macro_cntl = INREG(RADEON_DAC_MACRO_CNTL);
	if (IsOn) {
	    dac_cntl &= ~RADEON_DAC_PDWN;
	    dac_macro_cntl &= ~(RADEON_DAC_PDWN_R |
				RADEON_DAC_PDWN_G |
				RADEON_DAC_PDWN_B);
	} else {
	    dac_cntl |= RADEON_DAC_PDWN;
	    dac_macro_cntl |= (RADEON_DAC_PDWN_R |
			       RADEON_DAC_PDWN_G |
			       RADEON_DAC_PDWN_B);
	}
	OUTREG(RADEON_DAC_CNTL, dac_cntl);
	if ((!info->IsMobility) || (info->ChipFamily == CHIP_FAMILY_RV350)) 
	    OUTREG(RADEON_DAC_MACRO_CNTL, dac_macro_cntl);
    } else {
	CARD32 tv_dac_cntl;
	CARD32 fp2_gen_cntl;
	
	switch(info->ChipFamily)
	{
	case CHIP_FAMILY_R420:
	case CHIP_FAMILY_RV410:
	    tv_dac_cntl = INREG(RADEON_TV_DAC_CNTL);
	    if (IsOn) {
		tv_dac_cntl &= ~(R420_TV_DAC_RDACPD |
				 R420_TV_DAC_GDACPD |
				 R420_TV_DAC_BDACPD |
				 RADEON_TV_DAC_BGSLEEP);
	    } else {
		tv_dac_cntl |= (R420_TV_DAC_RDACPD |
				R420_TV_DAC_GDACPD |
				R420_TV_DAC_BDACPD |
				RADEON_TV_DAC_BGSLEEP);
	    }
	    OUTREG(RADEON_TV_DAC_CNTL, tv_dac_cntl);
	    break;
	case CHIP_FAMILY_R200:
	    fp2_gen_cntl = INREG(RADEON_FP2_GEN_CNTL);
	    if (IsOn) {
		fp2_gen_cntl |= RADEON_FP2_DVO_EN;
	    } else {
		fp2_gen_cntl &= ~RADEON_FP2_DVO_EN;
	    }
	    OUTREG(RADEON_FP2_GEN_CNTL, fp2_gen_cntl);
	    break;

	default:
	    tv_dac_cntl = INREG(RADEON_TV_DAC_CNTL);
	    if (IsOn) {
		tv_dac_cntl &= ~(RADEON_TV_DAC_RDACPD |
				 RADEON_TV_DAC_GDACPD |
				 RADEON_TV_DAC_BDACPD |
				 RADEON_TV_DAC_BGSLEEP);
	    } else {
		tv_dac_cntl |= (RADEON_TV_DAC_RDACPD |
				RADEON_TV_DAC_GDACPD |
				RADEON_TV_DAC_BDACPD |
				RADEON_TV_DAC_BGSLEEP);
	    }
	    OUTREG(RADEON_TV_DAC_CNTL, tv_dac_cntl);
	    break;
	}
    }
}

/* disable all ouputs before enabling the ones we want */
void RADEONDisableDisplays(ScrnInfoPtr pScrn) {
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char * RADEONMMIO = info->MMIO;
    unsigned long tmp, tmpPixclksCntl;


    /* primary DAC */
    tmp = INREG(RADEON_CRTC_EXT_CNTL);
    tmp &= ~RADEON_CRTC_CRT_ON;                    
    OUTREG(RADEON_CRTC_EXT_CNTL, tmp);
    RADEONDacPowerSet(pScrn, FALSE, TRUE);

    /* Secondary DAC */
    if (info->ChipFamily == CHIP_FAMILY_R200) {
        tmp = INREG(RADEON_FP2_GEN_CNTL);
        tmp &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
        OUTREG(RADEON_FP2_GEN_CNTL, tmp);
    } else {
        tmp = INREG(RADEON_CRTC2_GEN_CNTL);
        tmp &= ~RADEON_CRTC2_CRT2_ON;  
        OUTREG(RADEON_CRTC2_GEN_CNTL, tmp);
    }
    RADEONDacPowerSet(pScrn, FALSE, FALSE);

    /* FP 1 */
    tmp = INREG(RADEON_FP_GEN_CNTL);
    tmp &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
    OUTREG(RADEON_FP_GEN_CNTL, tmp);

    /* FP 2 */
    tmp = INREG(RADEON_FP2_GEN_CNTL);
    tmp &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
    OUTREG(RADEON_FP2_GEN_CNTL, tmp);

    /* LVDS */
    if (info->IsMobility) {
	tmpPixclksCntl = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
	if (info->IsMobility || info->IsIGP) {
	    /* Asic bug, when turning off LVDS_ON, we have to make sure
	       RADEON_PIXCLK_LVDS_ALWAYS_ON bit is off
	    */
	    OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
	}
	tmp = INREG(RADEON_LVDS_GEN_CNTL);
	tmp |= RADEON_LVDS_DISPLAY_DIS;
	tmp &= ~(RADEON_LVDS_ON | RADEON_LVDS_BLON);
	OUTREG(RADEON_LVDS_GEN_CNTL, tmp);
	if (info->IsMobility || info->IsIGP) {
	    OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmpPixclksCntl);
	}
    }

}

/* This is to be used enable/disable displays dynamically */
void RADEONEnableDisplay(ScrnInfoPtr pScrn, RADEONConnector* pPort, BOOL bEnable)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONSavePtr save = &info->ModeReg;
    unsigned char * RADEONMMIO = info->MMIO;
    unsigned long tmp;

    if (bEnable) {
        if (pPort->MonType == MT_CRT) {
            if (pPort->DACType == DAC_PRIMARY) {
                tmp = INREG(RADEON_CRTC_EXT_CNTL);
                tmp |= RADEON_CRTC_CRT_ON;                    
                OUTREG(RADEON_CRTC_EXT_CNTL, tmp);
                save->crtc_ext_cntl |= RADEON_CRTC_CRT_ON;
            } else if (pPort->DACType == DAC_TVDAC) {
                if (info->ChipFamily == CHIP_FAMILY_R200) {
                    tmp = INREG(RADEON_FP2_GEN_CNTL);
                    tmp |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                    OUTREG(RADEON_FP2_GEN_CNTL, tmp);
                    save->fp2_gen_cntl |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                } else {
                    tmp = INREG(RADEON_CRTC2_GEN_CNTL);
                    tmp |= RADEON_CRTC2_CRT2_ON;  
                    OUTREG(RADEON_CRTC2_GEN_CNTL, tmp);
                    save->crtc2_gen_cntl |= RADEON_CRTC2_CRT2_ON;
                }
            }
	    RADEONDacPowerSet(pScrn, bEnable, (pPort->DACType == DAC_PRIMARY));
        } else if (pPort->MonType == MT_DFP) {
            if (pPort->TMDSType == TMDS_INT) {
                tmp = INREG(RADEON_FP_GEN_CNTL);
                tmp |= (RADEON_FP_FPON | RADEON_FP_TMDS_EN);
                OUTREG(RADEON_FP_GEN_CNTL, tmp);
                save->fp_gen_cntl |= (RADEON_FP_FPON | RADEON_FP_TMDS_EN);
            } else if (pPort->TMDSType == TMDS_EXT) {
                tmp = INREG(RADEON_FP2_GEN_CNTL);
                tmp |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                OUTREG(RADEON_FP2_GEN_CNTL, tmp);
                save->fp2_gen_cntl |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
            }
        } else if (pPort->MonType == MT_LCD) {
            tmp = INREG(RADEON_LVDS_GEN_CNTL);
            tmp |= (RADEON_LVDS_ON | RADEON_LVDS_BLON);
            tmp &= ~(RADEON_LVDS_DISPLAY_DIS);
	    usleep (info->PanelPwrDly * 1000);
            OUTREG(RADEON_LVDS_GEN_CNTL, tmp);
            save->lvds_gen_cntl |= (RADEON_LVDS_ON | RADEON_LVDS_BLON);
            save->lvds_gen_cntl &= ~(RADEON_LVDS_DISPLAY_DIS);
        } 
    } else {
        if (pPort->MonType == MT_CRT || pPort->MonType == NONE) {
            if (pPort->DACType == DAC_PRIMARY) {
                tmp = INREG(RADEON_CRTC_EXT_CNTL);
                tmp &= ~RADEON_CRTC_CRT_ON;                    
                OUTREG(RADEON_CRTC_EXT_CNTL, tmp);
                save->crtc_ext_cntl &= ~RADEON_CRTC_CRT_ON;
            } else if (pPort->DACType == DAC_TVDAC) {
                if (info->ChipFamily == CHIP_FAMILY_R200) {
                    tmp = INREG(RADEON_FP2_GEN_CNTL);
                    tmp &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                    OUTREG(RADEON_FP2_GEN_CNTL, tmp);
                    save->fp2_gen_cntl &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                } else {
                    tmp = INREG(RADEON_CRTC2_GEN_CNTL);
                    tmp &= ~RADEON_CRTC2_CRT2_ON;  
                    OUTREG(RADEON_CRTC2_GEN_CNTL, tmp);
                    save->crtc2_gen_cntl &= ~RADEON_CRTC2_CRT2_ON;
                }
            }
	    RADEONDacPowerSet(pScrn, bEnable, (pPort->DACType == DAC_PRIMARY));
        }

        if (pPort->MonType == MT_DFP || pPort->MonType == NONE) {
            if (pPort->TMDSType == TMDS_INT) {
                tmp = INREG(RADEON_FP_GEN_CNTL);
                tmp &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
                OUTREG(RADEON_FP_GEN_CNTL, tmp);
                save->fp_gen_cntl &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
            } else if (pPort->TMDSType == TMDS_EXT) {
                tmp = INREG(RADEON_FP2_GEN_CNTL);
                tmp &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                OUTREG(RADEON_FP2_GEN_CNTL, tmp);
                save->fp2_gen_cntl &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
            }
        }

        if (pPort->MonType == MT_LCD || 
            (pPort->MonType == NONE && pPort->ConnectorType == CONNECTOR_PROPRIETARY)) {
	    unsigned long tmpPixclksCntl = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
	    if (info->IsMobility || info->IsIGP) {
	    /* Asic bug, when turning off LVDS_ON, we have to make sure
	       RADEON_PIXCLK_LVDS_ALWAYS_ON bit is off
	    */
		OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
	    }
            tmp = INREG(RADEON_LVDS_GEN_CNTL);
            tmp |= RADEON_LVDS_DISPLAY_DIS;
            tmp &= ~(RADEON_LVDS_ON | RADEON_LVDS_BLON);
            OUTREG(RADEON_LVDS_GEN_CNTL, tmp);
            save->lvds_gen_cntl |= RADEON_LVDS_DISPLAY_DIS;
            save->lvds_gen_cntl &= ~(RADEON_LVDS_ON | RADEON_LVDS_BLON);
	    if (info->IsMobility || info->IsIGP) {
		OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmpPixclksCntl);
	    }
        }
    }
}

/* Calculate display buffer watermark to prevent buffer underflow */
void RADEONInitDispBandwidth2(ScrnInfoPtr pScrn, RADEONInfoPtr info, RADEONInfoPtr info2,
			      DisplayModePtr mode1, DisplayModePtr mode2)
{
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    CARD32 temp, data, mem_trcd, mem_trp, mem_tras, mem_trbs=0;
    float mem_tcas;
    int k1, c;
    CARD32 MemTrcdExtMemCntl[4]     = {1, 2, 3, 4};
    CARD32 MemTrpExtMemCntl[4]      = {1, 2, 3, 4};
    CARD32 MemTrasExtMemCntl[8]     = {1, 2, 3, 4, 5, 6, 7, 8};

    CARD32 MemTrcdMemTimingCntl[8]     = {1, 2, 3, 4, 5, 6, 7, 8};
    CARD32 MemTrpMemTimingCntl[8]      = {1, 2, 3, 4, 5, 6, 7, 8};
    CARD32 MemTrasMemTimingCntl[16]    = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

    float MemTcas[8]  = {0, 1, 2, 3, 0, 1.5, 2.5, 0};
    float MemTcas2[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    float MemTrbs[8]  = {1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5};

    float mem_bw, peak_disp_bw;
    float min_mem_eff = 0.8;
    float sclk_eff, sclk_delay;
    float mc_latency_mclk, mc_latency_sclk, cur_latency_mclk, cur_latency_sclk;
    float disp_latency, disp_latency_overhead, disp_drain_rate, disp_drain_rate2;
    float pix_clk, pix_clk2; /* in MHz */
    int cur_size = 16;       /* in octawords */
    int critical_point, critical_point2;
    int stop_req, max_stop_req;
    float read_return_rate, time_disp1_drop_priority;

    /* 
     * Set display0/1 priority up on r3/4xx in the memory controller for 
     * high res modes if the user specifies HIGH for displaypriority 
     * option.
     */
    if ((info->DispPriority == 2) && IS_R300_VARIANT) {
        CARD32 mc_init_misc_lat_timer = INREG(R300_MC_INIT_MISC_LAT_TIMER);
	if (pRADEONEnt->Controller[1]->IsActive) {
	    mc_init_misc_lat_timer |= 0x1100; /* display 0 and 1 */
	} else {
	    mc_init_misc_lat_timer |= 0x0100; /* display 0 only */
	}
	OUTREG(R300_MC_INIT_MISC_LAT_TIMER, mc_init_misc_lat_timer);
    }


    /* R420 and RV410 family not supported yet */
    if (info->ChipFamily == CHIP_FAMILY_R420 || info->ChipFamily == CHIP_FAMILY_RV410) return; 

    if (pRADEONEnt->pSecondaryScrn) {
	if (info->IsSecondary) return;
	info2 = RADEONPTR(pRADEONEnt->pSecondaryScrn);
    }  else if (pRADEONEnt->Controller[1]->binding == 1) info2 = info;

    /*
     * Determine if there is enough bandwidth for current display mode
     */
    mem_bw = info->mclk * (info->RamWidth / 8) * (info->IsDDR ? 2 : 1);

    pix_clk = mode1->Clock/1000.0;
    if (mode2)
	pix_clk2 = mode2->Clock/1000.0;
    else
	pix_clk2 = 0;

    peak_disp_bw = (pix_clk * info->CurrentLayout.pixel_bytes);
    if (info2) 
	peak_disp_bw +=	(pix_clk2 * info2->CurrentLayout.pixel_bytes);

    if (peak_disp_bw >= mem_bw * min_mem_eff) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
		   "You may not have enough display bandwidth for current mode\n"
		   "If you have flickering problem, try to lower resolution, refresh rate, or color depth\n");
    } 

    /*  CRTC1
        Set GRPH_BUFFER_CNTL register using h/w defined optimal values.
    	GRPH_STOP_REQ <= MIN[ 0x7C, (CRTC_H_DISP + 1) * (bit depth) / 0x10 ]
    */
    stop_req = mode1->HDisplay * info->CurrentLayout.pixel_bytes / 16;

    /* setup Max GRPH_STOP_REQ default value */
    if (IS_RV100_VARIANT)
	max_stop_req = 0x5c;
    else
	max_stop_req  = 0x7c;
    if (stop_req > max_stop_req)
	stop_req = max_stop_req;
      
    /*  Get values from the EXT_MEM_CNTL register...converting its contents. */
    temp = INREG(RADEON_MEM_TIMING_CNTL);
    if ((info->ChipFamily == CHIP_FAMILY_RV100) || info->IsIGP) { /* RV100, M6, IGPs */
	mem_trcd      = MemTrcdExtMemCntl[(temp & 0x0c) >> 2];
	mem_trp       = MemTrpExtMemCntl[ (temp & 0x03) >> 0];
	mem_tras      = MemTrasExtMemCntl[(temp & 0x70) >> 4];
    } else { /* RV200 and later */
	mem_trcd      = MemTrcdMemTimingCntl[(temp & 0x07) >> 0];
	mem_trp       = MemTrpMemTimingCntl[ (temp & 0x700) >> 8];
	mem_tras      = MemTrasMemTimingCntl[(temp & 0xf000) >> 12];
    }
    
    /* Get values from the MEM_SDRAM_MODE_REG register...converting its */ 
    temp = INREG(RADEON_MEM_SDRAM_MODE_REG);
    data = (temp & (7<<20)) >> 20;
    if ((info->ChipFamily == CHIP_FAMILY_RV100) || info->IsIGP) { /* RV100, M6, IGPs */
	mem_tcas = MemTcas [data];
    } else {
	mem_tcas = MemTcas2 [data];
    }

    if (IS_R300_VARIANT) {

	/* on the R300, Tcas is included in Trbs.
	*/
	temp = INREG(RADEON_MEM_CNTL);
	data = (R300_MEM_NUM_CHANNELS_MASK & temp);
	if (data == 1) {
	    if (R300_MEM_USE_CD_CH_ONLY & temp) {
		temp  = INREG(R300_MC_IND_INDEX);
		temp &= ~R300_MC_IND_ADDR_MASK;
		temp |= R300_MC_READ_CNTL_CD_mcind;
		OUTREG(R300_MC_IND_INDEX, temp);
		temp  = INREG(R300_MC_IND_DATA);
		data = (R300_MEM_RBS_POSITION_C_MASK & temp);
	    } else {
		temp = INREG(R300_MC_READ_CNTL_AB);
		data = (R300_MEM_RBS_POSITION_A_MASK & temp);
	    }
	} else {
	    temp = INREG(R300_MC_READ_CNTL_AB);
	    data = (R300_MEM_RBS_POSITION_A_MASK & temp);
	}

	mem_trbs = MemTrbs[data];
	mem_tcas += mem_trbs;
    }

    if ((info->ChipFamily == CHIP_FAMILY_RV100) || info->IsIGP) { /* RV100, M6, IGPs */
	/* DDR64 SCLK_EFF = SCLK for analysis */
	sclk_eff = info->sclk;
    } else {
#ifdef XF86DRI
	if (info->directRenderingEnabled)
	    sclk_eff = info->sclk - (info->agpMode * 50.0 / 3.0);
	else
#endif
	    sclk_eff = info->sclk;
    }

    /* Find the memory controller latency for the display client.
    */
    if (IS_R300_VARIANT) {
	/*not enough for R350 ???*/
	/*
	if (!mode2) sclk_delay = 150;
	else {
	    if (info->RamWidth == 256) sclk_delay = 87;
	    else sclk_delay = 97;
	}
	*/
	sclk_delay = 250;
    } else {
	if ((info->ChipFamily == CHIP_FAMILY_RV100) ||
	    info->IsIGP) {
	    if (info->IsDDR) sclk_delay = 41;
	    else sclk_delay = 33;
	} else {
	    if (info->RamWidth == 128) sclk_delay = 57;
	    else sclk_delay = 41;
	}
    }

    mc_latency_sclk = sclk_delay / sclk_eff;
	
    if (info->IsDDR) {
	if (info->RamWidth == 32) {
	    k1 = 40;
	    c  = 3;
	} else {
	    k1 = 20;
	    c  = 1;
	}
    } else {
	k1 = 40;
	c  = 3;
    }
    mc_latency_mclk = ((2.0*mem_trcd + mem_tcas*c + 4.0*mem_tras + 4.0*mem_trp + k1) /
		       info->mclk) + (4.0 / sclk_eff);

    /*
      HW cursor time assuming worst case of full size colour cursor.
    */
    cur_latency_mclk = (mem_trp + MAX(mem_tras, (mem_trcd + 2*(cur_size - (info->IsDDR+1))))) / info->mclk;
    cur_latency_sclk = cur_size / sclk_eff;

    /*
      Find the total latency for the display data.
    */
    disp_latency_overhead = 8.0 / info->sclk;
    mc_latency_mclk = mc_latency_mclk + disp_latency_overhead + cur_latency_mclk;
    mc_latency_sclk = mc_latency_sclk + disp_latency_overhead + cur_latency_sclk;
    disp_latency = MAX(mc_latency_mclk, mc_latency_sclk);

    /*
      Find the drain rate of the display buffer.
    */
    disp_drain_rate = pix_clk / (16.0/info->CurrentLayout.pixel_bytes);
    if (info2)
	disp_drain_rate2 = pix_clk2 / (16.0/info2->CurrentLayout.pixel_bytes);
    else
	disp_drain_rate2 = 0;

    /*
      Find the critical point of the display buffer.
    */
    critical_point= (CARD32)(disp_drain_rate * disp_latency + 0.5); 

    /* ???? */
    /*
    temp = (info->SavedReg.grph_buffer_cntl & RADEON_GRPH_CRITICAL_POINT_MASK) >> RADEON_GRPH_CRITICAL_POINT_SHIFT;
    if (critical_point < temp) critical_point = temp;
    */
    if (info->DispPriority == 2) {
	critical_point = 0;
    }

    /*
      The critical point should never be above max_stop_req-4.  Setting
      GRPH_CRITICAL_CNTL = 0 will thus force high priority all the time.
    */
    if (max_stop_req - critical_point < 4) critical_point = 0; 

    if (critical_point == 0 && mode2 && info->ChipFamily == CHIP_FAMILY_R300) {
	/* some R300 cards have problem with this set to 0, when CRTC2 is enabled.*/
	critical_point = 0x10;
    }

    temp = info->SavedReg.grph_buffer_cntl;
    temp &= ~(RADEON_GRPH_STOP_REQ_MASK);
    temp |= (stop_req << RADEON_GRPH_STOP_REQ_SHIFT);
    temp &= ~(RADEON_GRPH_START_REQ_MASK);
    if ((info->ChipFamily == CHIP_FAMILY_R350) &&
	(stop_req > 0x15)) {
	stop_req -= 0x10;
    }
    temp |= (stop_req << RADEON_GRPH_START_REQ_SHIFT);

    temp |= RADEON_GRPH_BUFFER_SIZE;
    temp &= ~(RADEON_GRPH_CRITICAL_CNTL   |
	      RADEON_GRPH_CRITICAL_AT_SOF |
	      RADEON_GRPH_STOP_CNTL);
    /*
      Write the result into the register.
    */
    OUTREG(RADEON_GRPH_BUFFER_CNTL, ((temp & ~RADEON_GRPH_CRITICAL_POINT_MASK) |
				     (critical_point << RADEON_GRPH_CRITICAL_POINT_SHIFT)));

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "GRPH_BUFFER_CNTL from %x to %x\n",
		   (unsigned int)info->SavedReg.grph_buffer_cntl,
		   INREG(RADEON_GRPH_BUFFER_CNTL));

    if (mode2) {
	stop_req = mode2->HDisplay * info2->CurrentLayout.pixel_bytes / 16;

	if (stop_req > max_stop_req) stop_req = max_stop_req;

	temp = info->SavedReg.grph2_buffer_cntl;
	temp &= ~(RADEON_GRPH_STOP_REQ_MASK);
	temp |= (stop_req << RADEON_GRPH_STOP_REQ_SHIFT);
	temp &= ~(RADEON_GRPH_START_REQ_MASK);
	if ((info->ChipFamily == CHIP_FAMILY_R350) &&
	    (stop_req > 0x15)) {
	    stop_req -= 0x10;
	}
	temp |= (stop_req << RADEON_GRPH_START_REQ_SHIFT);
	temp |= RADEON_GRPH_BUFFER_SIZE;
	temp &= ~(RADEON_GRPH_CRITICAL_CNTL   |
		  RADEON_GRPH_CRITICAL_AT_SOF |
		  RADEON_GRPH_STOP_CNTL);

	if ((info->ChipFamily == CHIP_FAMILY_RS100) || 
	    (info->ChipFamily == CHIP_FAMILY_RS200))
	    critical_point2 = 0;
	else {
	    read_return_rate = MIN(info->sclk, info->mclk*(info->RamWidth*(info->IsDDR+1)/128));
	    time_disp1_drop_priority = critical_point / (read_return_rate - disp_drain_rate);

	    critical_point2 = (CARD32)((disp_latency + time_disp1_drop_priority + 
					disp_latency) * disp_drain_rate2 + 0.5);

	    if (info->DispPriority == 2) {
		critical_point2 = 0;
	    }

	    if (max_stop_req - critical_point2 < 4) critical_point2 = 0;

	}

	if (critical_point2 == 0 && info->ChipFamily == CHIP_FAMILY_R300) {
	    /* some R300 cards have problem with this set to 0 */
	    critical_point2 = 0x10;
	}

	OUTREG(RADEON_GRPH2_BUFFER_CNTL, ((temp & ~RADEON_GRPH_CRITICAL_POINT_MASK) |
					  (critical_point2 << RADEON_GRPH_CRITICAL_POINT_SHIFT)));

	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		       "GRPH2_BUFFER_CNTL from %x to %x\n",
		       (unsigned int)info->SavedReg.grph2_buffer_cntl,
		       INREG(RADEON_GRPH2_BUFFER_CNTL));
    }
}

void RADEONInitDispBandwidth(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    DisplayModePtr mode1, mode2;
    RADEONInfoPtr info2 = NULL;

    if (pRADEONEnt->pSecondaryScrn) {
	if (info->IsSecondary) return;
	info2 = RADEONPTR(pRADEONEnt->pSecondaryScrn);
    } else if (pRADEONEnt->Controller[1]->binding == 1) info2 = info;

    mode1 = info->CurrentLayout.mode;
    if (info->MergedFB) {
	mode1 = ((RADEONMergedDisplayModePtr)info->CurrentLayout.mode->Private)->CRT1;
	mode2 = ((RADEONMergedDisplayModePtr)info->CurrentLayout.mode->Private)->CRT2;
    } else if ((pRADEONEnt->HasSecondary) && info2) {
	mode2 = info2->CurrentLayout.mode;
    } else {
	mode2 = NULL;
    }

    RADEONInitDispBandwidth2(pScrn, info, info2, mode1, mode2);
}

static void
RADEONOutputsBlank(ScrnInfoPtr pScrn, RADEONConnector *pPort, Bool Blank)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    switch(pPort->MonType) {
    case MT_LCD:
	if (Blank)
	    OUTREGP(RADEON_LVDS_GEN_CNTL, RADEON_LVDS_DISPLAY_DIS, ~RADEON_LVDS_DISPLAY_DIS);
	else
	    OUTREGP(RADEON_LVDS_GEN_CNTL, 0, ~RADEON_LVDS_DISPLAY_DIS);
        break;
    case MT_CRT:
	if ((info->ChipFamily == CHIP_FAMILY_R200) && (pPort->DACType == DAC_TVDAC)) {
	    if (Blank)
		OUTREGP(RADEON_FP2_GEN_CNTL, RADEON_FP2_BLANK_EN, ~RADEON_FP2_BLANK_EN);
	    else
		OUTREGP(RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_BLANK_EN);
	}
        break;
    case MT_DFP:
	if (pPort->TMDSType == TMDS_EXT) {
	    if (Blank)
		OUTREGP(RADEON_FP2_GEN_CNTL, RADEON_FP2_BLANK_EN, ~RADEON_FP2_BLANK_EN);
	    else
		OUTREGP(RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_BLANK_EN);
	} else {
	    if (Blank)
		OUTREGP(RADEON_FP_GEN_CNTL, RADEON_FP_BLANK_EN, ~RADEON_FP_BLANK_EN);
	    else
		OUTREGP(RADEON_FP_GEN_CNTL, 0, ~RADEON_FP_BLANK_EN);
	}
        break;
    case MT_NONE:
    default:
        break;
    }
}
 
static void
RADEONCRTC1Blank(RADEONInfoPtr info, Bool Blank)
{
    unsigned char *RADEONMMIO = info->MMIO;
    
    if (Blank)
	OUTREGP(RADEON_CRTC_EXT_CNTL,
		RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS | RADEON_CRTC_HSYNC_DIS,
		~(RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS | RADEON_CRTC_HSYNC_DIS));
    else
	OUTREGP(RADEON_CRTC_EXT_CNTL, 0,
		~(RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS | RADEON_CRTC_HSYNC_DIS));
}

static void
RADEONCRTC2Blank(RADEONInfoPtr info, Bool Blank)
{
    unsigned char *RADEONMMIO = info->MMIO;
    
    if (Blank)
	OUTREGP(RADEON_CRTC2_GEN_CNTL,
		RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS,
		~(RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS));
    else
	OUTREGP(RADEON_CRTC2_GEN_CNTL, 0,
		~(RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS));
}

/* Blank screen */
void RADEONBlank(ScrnInfoPtr pScrn, Bool Blank)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);

    if (!pRADEONEnt->HasSecondary ||
	(pRADEONEnt->HasSecondary && !info->IsSwitching) ||
	(info->IsSwitching && (!info->IsSecondary))) {
 
	RADEONOutputsBlank(pScrn, pRADEONEnt->PortInfo[0], Blank);
	RADEONCRTC1Blank(info, Blank);
 
	if (!pRADEONEnt->HasCRTC2)
	    return;

	if (pRADEONEnt->Controller[1]->binding == 1) {
	    RADEONOutputsBlank(pScrn, pRADEONEnt->PortInfo[1], Blank);
	    RADEONCRTC2Blank(info, Blank);
	}
    }

    if ((pRADEONEnt->HasSecondary && !info->IsSwitching) ||
	(info->IsSwitching && info->IsSecondary)) {
	RADEONOutputsBlank(pScrn, pRADEONEnt->PortInfo[1], Blank);
	RADEONCRTC2Blank(info, Blank);
    }
}


static void
RADEONOutputsDPMS(ScrnInfoPtr pScrn, RADEONConnector *pPort, int Mode)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    RADEONMonitorType MonType;
    RADEONTmdsType TmdsType;
    RADEONDacType DacType;
  
    MonType = pPort->MonType;
    TmdsType = pPort->TMDSType;
    DacType = pPort->DACType;
    
    switch (MonType) {
    case MT_LCD:
	if (Mode == DPMSModeOn) {
	    OUTREGP(RADEON_LVDS_GEN_CNTL, RADEON_LVDS_BLON, ~RADEON_LVDS_BLON);
	    usleep (info->PanelPwrDly * 1000);
	    OUTREGP(RADEON_LVDS_GEN_CNTL, RADEON_LVDS_ON, ~RADEON_LVDS_ON);
	} else {
	    unsigned int tmpPixclksCntl = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
	    
	    /* Asic bug, when turning off LVDS_ON, we have to make sure
	       RADEON_PIXCLK_LVDS_ALWAYS_ON bit is off */
	    if (info->IsMobility || info->IsIGP)
		OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
	    OUTREGP(RADEON_LVDS_GEN_CNTL, 0, ~(RADEON_LVDS_BLON | RADEON_LVDS_ON));
	    if (info->IsMobility || info->IsIGP)
		OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmpPixclksCntl);
	}
	break;
    case MT_DFP:
	if (Mode == DPMSModeOn) {
	    if (TmdsType == TMDS_EXT) {
		OUTREGP(RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_BLANK_EN);
		OUTREGP(RADEON_FP2_GEN_CNTL, RADEON_FP2_ON, ~RADEON_FP2_ON);
		if (info->ChipFamily >= CHIP_FAMILY_R200)
		    OUTREGP(RADEON_FP2_GEN_CNTL, RADEON_FP2_DVO_EN, ~RADEON_FP2_DVO_EN);
	    } else
		OUTREGP(RADEON_FP_GEN_CNTL, (RADEON_FP_FPON | RADEON_FP_TMDS_EN),
			~(RADEON_FP_FPON | RADEON_FP_TMDS_EN));
	} else {
	    if (TmdsType == TMDS_EXT) {
		OUTREGP(RADEON_FP2_GEN_CNTL, RADEON_FP2_BLANK_EN, ~RADEON_FP2_BLANK_EN);
		OUTREGP(RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_ON);
		if (info->ChipFamily >= CHIP_FAMILY_R200) {
		    OUTREGP(RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_DVO_EN);
		}
	    } else
		OUTREGP(RADEON_FP_GEN_CNTL, 0, ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN));
	}
	break;
    case MT_CRT:
    default:
	RADEONDacPowerSet(pScrn, (Mode == DPMSModeOn), (DacType == DAC_PRIMARY));
	break;
    }
}

void
RADEONCRTC1DPMS(RADEONInfoPtr info, int Mode)
{
    unsigned char *RADEONMMIO = info->MMIO;

    switch (Mode) {
	case DPMSModeOn:
	    /* Screen: On; HSync: On, VSync: On */
	    OUTREGP(RADEON_CRTC_EXT_CNTL, 0,
		    ~(RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS | RADEON_CRTC_VSYNC_DIS));
	    break;
	case DPMSModeStandby:
	    /* Screen: Off; HSync: Off, VSync: On */
	    OUTREGP(RADEON_CRTC_EXT_CNTL, (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS),
		    ~(RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS | RADEON_CRTC_VSYNC_DIS));
	    break;
	case DPMSModeSuspend:
	    /* Screen: Off; HSync: On, VSync: Off */
	    OUTREGP(RADEON_CRTC_EXT_CNTL, (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS),
		    ~(RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS | RADEON_CRTC_VSYNC_DIS));
	    break;
	case DPMSModeOff:
	default:
	    /* Screen: Off; HSync: Off, VSync: Off */
	    OUTREGP(RADEON_CRTC_EXT_CNTL,
		    (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS | RADEON_CRTC_VSYNC_DIS),
		    ~(RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS | RADEON_CRTC_VSYNC_DIS));
	    break;
    }
}

void
RADEONCRTC2DPMS(RADEONInfoPtr info, int Mode)
{
    unsigned char *RADEONMMIO = info->MMIO;
    
    switch (Mode) {
	case DPMSModeOn:
	    /* Screen: On; HSync: On, VSync: On */
	    OUTREGP(RADEON_CRTC2_GEN_CNTL, 0,
		    ~(RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS));
	    break;
	case DPMSModeStandby:
	    /* Screen: Off; HSync: Off, VSync: On */
	    OUTREGP(RADEON_CRTC2_GEN_CNTL, (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_HSYNC_DIS),
		    ~(RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS));
	    break;
	case DPMSModeSuspend:
	    /* Screen: Off; HSync: On, VSync: Off */
	    OUTREGP(RADEON_CRTC2_GEN_CNTL, (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS),
		    ~(RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS));
	    break;
	case DPMSModeOff:
	default:
	    /* Screen: Off; HSync: Off, VSync: Off */
	    OUTREGP(RADEON_CRTC2_GEN_CNTL,
		    (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS),
		    ~(RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS));
	    break;
	    
    }
}


/* Sets VESA Display Power Management Signaling (DPMS) Mode */
void
RADEONDisplayPowerManagementSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    RADEONConnector *pPort;

    if (!pScrn->vtSema)
	return;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONDisplayPowerManagementSet(%d,0x%x)\n",
		   PowerManagementMode, flags);

#ifdef XF86DRI
    if (info->CPStarted)
	DRILock(pScrn->pScreen, 0);
#endif

    if (info->accelOn)
        RADEON_SYNC(info, pScrn);

    if (info->FBDev) {
	fbdevHWDPMSSet(pScrn, PowerManagementMode, flags);
    } else {
	if (info->IsSecondary) {
	    RADEONCRTC2DPMS(info, PowerManagementMode);
	    RADEONOutputsDPMS(pScrn, pRADEONEnt->PortInfo[1], PowerManagementMode);
	} else {
	    RADEONCRTC1DPMS(info, PowerManagementMode);
	    RADEONOutputsDPMS(pScrn, pRADEONEnt->PortInfo[0], PowerManagementMode);

	    if (pRADEONEnt->Controller[1]->binding == 1) {
		RADEONCRTC2DPMS(info, PowerManagementMode);
		RADEONOutputsDPMS(pScrn, pRADEONEnt->PortInfo[1], PowerManagementMode);
	    }
	}
    }

#ifdef XF86DRI
    if (info->CPStarted)
	DRIUnlock(pScrn->pScreen);
#endif
}

Bool RADEONAllocateControllers(ScrnInfoPtr pScrn)
{
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);

    if (pRADEONEnt->Controller[0])
      return TRUE;

    pRADEONEnt->Controller[0] = xcalloc(sizeof(RADEONController), 1);
    if (!pRADEONEnt->Controller[0])
        return FALSE;
    
    pRADEONEnt->Controller[1] = xcalloc(sizeof(RADEONController), 1);
    if (!pRADEONEnt->Controller[1])
    {
	xfree(pRADEONEnt->Controller[0]);
	return FALSE;
    }

    return TRUE;
}

Bool RADEONAllocateConnectors(ScrnInfoPtr pScrn)
{
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);

    if (pRADEONEnt->PortInfo[0])
        return TRUE;

    /* for now always allocate both connectors */
    pRADEONEnt->PortInfo[0] = xcalloc(sizeof(RADEONConnector), 1);
    if (!pRADEONEnt->PortInfo[0])
	return FALSE;

    pRADEONEnt->PortInfo[1] = xcalloc(sizeof(RADEONConnector), 1);
    if (!pRADEONEnt->PortInfo[1]) {
	xfree(pRADEONEnt->PortInfo[0]);
	return FALSE;
    }

    return TRUE;
}

