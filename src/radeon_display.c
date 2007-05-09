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
#include "xf86Modes.h"

/* Driver data structures */
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_version.h"

void RADEONSetOutputType(ScrnInfoPtr pScrn, RADEONOutputPrivatePtr radeon_output);
void radeon_crtc_load_lut(xf86CrtcPtr crtc);
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

const char *OutputType[10] = {
    "None",
    "VGA",
    "DVI",
    "LVDS",
    "S-video",
    "Composite",
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

    if (b->DriverPrivate.uval == RADEON_LCD_GPIO_MASK) { 
        val = INREG(b->DriverPrivate.uval+4);
        *Clock = (val & (1<<13)) != 0;
        *data  = (val & (1<<12)) != 0;
    } else {
        val = INREG(b->DriverPrivate.uval);
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

    if (b->DriverPrivate.uval == RADEON_LCD_GPIO_MASK) {
        val = INREG(b->DriverPrivate.uval) & (CARD32)~((1<<12) | (1<<13));
        val |= (Clock ? 0:(1<<13));
        val |= (data ? 0:(1<<12));
        OUTREG(b->DriverPrivate.uval, val);
    } else {
        val = INREG(b->DriverPrivate.uval) & (CARD32)~(RADEON_GPIO_EN_0 | RADEON_GPIO_EN_1);
        val |= (Clock ? 0:RADEON_GPIO_EN_1);
        val |= (data ? 0:RADEON_GPIO_EN_0);
        OUTREG(b->DriverPrivate.uval, val);
   }
    /* read back to improve reliability on some cards. */
    val = INREG(b->DriverPrivate.uval);
}

Bool RADEONI2CInit(ScrnInfoPtr pScrn, I2CBusPtr *bus_ptr, int i2c_reg, char *name)
{
    I2CBusPtr pI2CBus;

    pI2CBus = xf86CreateI2CBusRec();
    if (!pI2CBus) return FALSE;

    pI2CBus->BusName    = name;
    pI2CBus->scrnIndex  = pScrn->scrnIndex;
    pI2CBus->I2CPutBits = RADEONI2CPutBits;
    pI2CBus->I2CGetBits = RADEONI2CGetBits;
    pI2CBus->AcknTimeout = 5;
    pI2CBus->DriverPrivate.uval = i2c_reg;

    if (!xf86I2CBusInit(pI2CBus)) return FALSE;

    *bus_ptr = pI2CBus;
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
	return MT_UNKNOWN;
    }

    return(bConnected ? MT_CRT : MT_NONE);
}


static RADEONMonitorType RADEONDisplayDDCConnected(ScrnInfoPtr pScrn, RADEONDDCType DDCType, xf86OutputPtr output)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long DDCReg;
    RADEONMonitorType MonType = MT_NONE;
    xf86MonPtr* MonInfo = &output->MonInfo;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int i, j;


    DDCReg = radeon_output->DDCReg;

    /* Read and output monitor info using DDC2 over I2C bus */
    if (radeon_output->pI2CBus && info->ddc2 && (DDCReg != RADEON_LCD_GPIO_MASK)) {
	OUTREG(DDCReg, INREG(DDCReg) &
	       (CARD32)~(RADEON_GPIO_A_0 | RADEON_GPIO_A_1));

	/* For some old monitors (like Compaq Presario FP500), we need
	 * following process to initialize/stop DDC
	 */
	OUTREG(DDCReg, INREG(DDCReg) & ~(RADEON_GPIO_EN_1));
	for (j = 0; j < 3; j++) {
	    OUTREG(DDCReg,
		   INREG(DDCReg) & ~(RADEON_GPIO_EN_0));
	    usleep(13000);

	    OUTREG(DDCReg,
		   INREG(DDCReg) & ~(RADEON_GPIO_EN_1));
	    for (i = 0; i < 10; i++) {
		usleep(15000);
		if (INREG(DDCReg) & RADEON_GPIO_Y_1)
		    break;
	    }
	    if (i == 10) continue;

	    usleep(15000);

	    OUTREG(DDCReg, INREG(DDCReg) | RADEON_GPIO_EN_0);
	    usleep(15000);

	    OUTREG(DDCReg, INREG(DDCReg) | RADEON_GPIO_EN_1);
	    usleep(15000);
	    OUTREG(DDCReg,
		   INREG(DDCReg) & ~(RADEON_GPIO_EN_0));
	    usleep(15000);
	    *MonInfo = xf86DoEDID_DDC2(pScrn->scrnIndex, radeon_output->pI2CBus);

	    OUTREG(DDCReg, INREG(DDCReg) | RADEON_GPIO_EN_1);
	    OUTREG(DDCReg, INREG(DDCReg) | RADEON_GPIO_EN_0);
	    usleep(15000);
	    OUTREG(DDCReg,
		   INREG(DDCReg) & ~(RADEON_GPIO_EN_1));
	    for (i = 0; i < 5; i++) {
		usleep(15000);
		if (INREG(DDCReg) & RADEON_GPIO_Y_1)
		    break;
	    }
	    usleep(15000);
	    OUTREG(DDCReg,
		   INREG(DDCReg) & ~(RADEON_GPIO_EN_0));
	    usleep(15000);

	    OUTREG(DDCReg, INREG(DDCReg) | RADEON_GPIO_EN_1);
	    OUTREG(DDCReg, INREG(DDCReg) | RADEON_GPIO_EN_0);
	    usleep(15000);
	    if(*MonInfo)  break;
	}
    } else if (radeon_output->pI2CBus && info->ddc2 && DDCReg == RADEON_LCD_GPIO_MASK) {
         *MonInfo = xf86DoEDID_DDC2(pScrn->scrnIndex, radeon_output->pI2CBus);
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "DDC2/I2C is not properly initialized\n");
	MonType = MT_NONE;
    }

    OUTREG(DDCReg, INREG(DDCReg) &
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
	    if (radeon_output->TMDSType == TMDS_EXT) MonType = MT_DFP;
	    else {
		if (INREG(RADEON_FP_GEN_CNTL) & RADEON_FP_EN_TMDS)
		    MonType = MT_DFP;
		else
		    MonType = MT_LCD;
	    }
	} else MonType = MT_CRT;
    } else MonType = MT_NONE;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "DDC Type: %d, Detected Monitor Type: %d\n", DDCType, MonType);

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
	info->PanelYRes = (fp_vert_stretch>>12) + 1;
    } else {
	info->PanelYRes = (INREG(RADEON_CRTC_V_TOTAL_DISP)>>16) + 1;
    }
    if (fp_horz_stretch & RADEON_HORZ_STRETCH_ENABLE) {
	info->PanelXRes = ((fp_horz_stretch>>16) + 1) * 8;
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
		tmp_mode = tmp_mode->next;
	    }
	}
	if ((info->DotClock == 0) && !pRADEONEnt->pOutput[0]->MonInfo) {
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

static void RADEONSwapOutputs(ScrnInfoPtr pScrn)
{
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    xf86OutputPtr connector;
    RADEONOutputPrivatePtr conn_priv;
    
    connector = pRADEONEnt->pOutput[0];
    pRADEONEnt->pOutput[0] = pRADEONEnt->pOutput[1];
    pRADEONEnt->pOutput[1] = connector;
    
    conn_priv = pRADEONEnt->PortInfo[0];
    pRADEONEnt->PortInfo[0] = pRADEONEnt->PortInfo[1];
    pRADEONEnt->PortInfo[1] = conn_priv;
}
/*
 * initialise the static data sos we don't have to re-do at randr change */
void RADEONSetupConnectors(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    const char *s;
    int i = 0, second = 0, max_mt = 5;

    /* We first get the information about all connectors from BIOS.
     * This is how the card is phyiscally wired up.
     * The information should be correct even on a OEM card.
     * If not, we may have problem -- need to use MonitorLayout option.
     */
    for (i = 0; i < info->max_connectors; i++) {
	pRADEONEnt->PortInfo[i]->MonType = MT_UNKNOWN;
	pRADEONEnt->PortInfo[i]->DDCType = DDC_NONE_DETECTED;
	pRADEONEnt->PortInfo[i]->DACType = DAC_UNKNOWN;
	pRADEONEnt->PortInfo[i]->TMDSType = TMDS_UNKNOWN;
	pRADEONEnt->PortInfo[i]->ConnectorType = CONNECTOR_NONE;
    }

    if (!RADEONGetConnectorInfoFromBIOS(pScrn) ||
        ((pRADEONEnt->PortInfo[0]->DDCType == 0) &&
        (pRADEONEnt->PortInfo[1]->DDCType == 0))) {
	/* Below is the most common setting, but may not be true */
	pRADEONEnt->PortInfo[0]->MonType = MT_UNKNOWN;
	pRADEONEnt->PortInfo[0]->DDCType = DDC_DVI;
	pRADEONEnt->PortInfo[0]->DACType = DAC_TVDAC;
	pRADEONEnt->PortInfo[0]->TMDSType = TMDS_INT;
	pRADEONEnt->PortInfo[0]->ConnectorType = CONNECTOR_DVI_I;

	pRADEONEnt->PortInfo[1]->MonType = MT_UNKNOWN;
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
    if (pRADEONEnt->PortInfo[1]->TMDSType == TMDS_INT) {
	RADEONSwapOutputs(pScrn);
    } else if ((pRADEONEnt->PortInfo[0]->TMDSType != TMDS_INT &&
                pRADEONEnt->PortInfo[1]->TMDSType != TMDS_INT)) {
        /* no TMDS_INT port, make primary DAC port first */
	/* On my Inspiron 8600 both internal and external ports are
	   marked DAC_PRIMARY in BIOS. So be extra careful - only
	   swap when the first port is not DAC_PRIMARY */
        if ((!(pRADEONEnt->PortInfo[0]->ConnectorType == CONNECTOR_PROPRIETARY)) &&  (pRADEONEnt->PortInfo[1]->DACType == DAC_PRIMARY) &&
	     (pRADEONEnt->PortInfo[0]->DACType != DAC_PRIMARY)) {
	    RADEONSwapOutputs(pScrn);
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

#if 1
    if (info->IsMobility) {
        pRADEONEnt->PortInfo[2]->DDCType = DDC_DVI;
        pRADEONEnt->PortInfo[2]->TMDSType = TMDS_INT;
        pRADEONEnt->PortInfo[2]->ConnectorType = CONNECTOR_DVI_D;
        pRADEONEnt->PortInfo[0]->TMDSType = TMDS_UNKNOWN;
	if (pRADEONEnt->PortInfo[0]->DDCType == DDC_DVI) {
	    pRADEONEnt->PortInfo[0]->DDCType = DDC_MONID;
	}
	if (pRADEONEnt->PortInfo[0]->TMDSType == TMDS_INT) {
	    pRADEONEnt->PortInfo[0]->TMDSType = TMDS_UNKNOWN;
	}
    }
#endif

    for (i = 0; i < info->max_connectors; i++) {
      RADEONOutputPrivatePtr radeon_output = pRADEONEnt->PortInfo[i];

      int DDCReg = 0;
      char *names[] = { "DDC1", "DDC2", "DDC3" };

      RADEONSetOutputType(pScrn, radeon_output);
      switch(radeon_output->DDCType) {
      case DDC_MONID: DDCReg = RADEON_GPIO_MONID; break;
      case DDC_DVI  : DDCReg = RADEON_GPIO_DVI_DDC; break;
      case DDC_VGA: DDCReg = RADEON_GPIO_VGA_DDC; break;
      case DDC_CRT2: DDCReg = RADEON_GPIO_CRT2_DDC; break;
      default: break;
      }
      
      if (DDCReg) {
	radeon_output->DDCReg = DDCReg;
	RADEONI2CInit(pScrn, &radeon_output->pI2CBus, DDCReg, names[i]);
      }

      if (radeon_output->type == OUTPUT_LVDS) {
	RADEONGetLVDSInfo(pScrn);
      }

      if (radeon_output->type == OUTPUT_DVI) {
	RADEONGetTMDSInfo(pScrn);

	if (i == 0)
		RADEONGetHardCodedEDIDFromBIOS(pScrn);

	/*RADEONUpdatePanelSize(pScrn);*/

      }


    }

    
}

static RADEONMonitorType RADEONPortCheckNonDDC(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONMonitorType MonType = MT_NONE;


    if (info->IsMobility) {
        if ((info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_LVDS_ATOM) ||
	     radeon_output->ConnectorType == CONNECTOR_PROPRIETARY) {
	     if (INREG(RADEON_BIOS_4_SCRATCH) & 4)
	         MonType =  MT_LCD;
        }
	/* non-DDC TMDS panel connected through DVO */
	if (INREG(RADEON_FP2_GEN_CNTL) & RADEON_FP2_ON)
	  MonType = MT_DFP;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Detected Monitor Type: %d\n", MonType);

    return MonType;

}

/* Primary Head (DVI or Laptop Int. panel)*/
/* A ddc capable display connected on DVI port */
/* Secondary Head (mostly VGA, can be DVI on some OEM boards)*/
void RADEONConnectorFindMonitor(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    
    if (radeon_output->MonType == MT_UNKNOWN) {
      if ((radeon_output->MonType = RADEONDisplayDDCConnected(pScrn,
						     radeon_output->DDCType,
						     output)));
      else if((radeon_output->MonType = RADEONPortCheckNonDDC(pScrn, output)));
      else if (radeon_output->DACType == DAC_PRIMARY) 
	  radeon_output->MonType = RADEONCrtIsPhysicallyConnected(pScrn, !(radeon_output->DACType));
    }

    if (output->MonInfo) {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID data from the display on connector: %s ----------------------\n",
		 info->IsAtomBios ?
		 ConnectorTypeNameATOM[radeon_output->ConnectorType]:
		 ConnectorTypeName[radeon_output->ConnectorType]
		 );
      xf86PrintEDID( output->MonInfo );
    }
}

void RADEONQueryConnectedDisplays(ScrnInfoPtr pScrn)
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
					  pRADEONEnt->pOutput[0]);
            if ((pRADEONEnt->PortInfo[1]->MonType > MT_NONE) &&
                (pRADEONEnt->PortInfo[1]->MonType < MT_STV))
		RADEONDisplayDDCConnected(pScrn, pRADEONEnt->PortInfo[1]->DDCType,
					  pRADEONEnt->pOutput[1]);
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
									     pRADEONEnt->pOutput[0])));
	    else if((pRADEONEnt->PortInfo[0]->MonType = RADEONDisplayDDCConnected(pScrn, DDC_VGA,
										  pRADEONEnt->pOutput[0])));
	    else if((pRADEONEnt->PortInfo[0]->MonType = RADEONDisplayDDCConnected(pScrn, DDC_CRT2,
										  pRADEONEnt->pOutput[0])));
	    else
		pRADEONEnt->PortInfo[0]->MonType = MT_CRT;
	    
	    if (!ignore_edid) {
		if (pRADEONEnt->pOutput[0]->MonInfo) {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Monitor1 EDID data ---------------------------\n");
		    xf86PrintEDID(pRADEONEnt->pOutput[0]->MonInfo );
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "End of Monitor1 EDID data --------------------\n");
		}
	    }
	    
	    pRADEONEnt->PortInfo[1]->MonType = MT_NONE;
	    pRADEONEnt->pOutput[1]->MonInfo = NULL;
	    pRADEONEnt->PortInfo[1]->DDCType = DDC_NONE_DETECTED;
	    pRADEONEnt->PortInfo[1]->DACType = DAC_UNKNOWN;
	    pRADEONEnt->PortInfo[1]->TMDSType = TMDS_UNKNOWN;
	    pRADEONEnt->PortInfo[1]->ConnectorType = CONNECTOR_NONE;
	    
	    pRADEONEnt->PortInfo[0]->crtc_num = 1;
	    pRADEONEnt->PortInfo[1]->crtc_num = 2;
	    
	    return;
	}
	
	RADEONConnectorFindMonitor(pScrn, pRADEONEnt->pOutput[0]);
	RADEONConnectorFindMonitor(pScrn, pRADEONEnt->pOutput[1]);
	
    }

    if(ignore_edid) {
        pRADEONEnt->pOutput[0]->MonInfo = NULL;
        pRADEONEnt->pOutput[1]->MonInfo = NULL;
    } else {
        if (pRADEONEnt->pOutput[0]->MonInfo) {
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID data from the display on 1st port ----------------------\n");
            xf86PrintEDID( pRADEONEnt->pOutput[0]->MonInfo );
        }

        if (pRADEONEnt->pOutput[1]->MonInfo) {
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID data from the display on 2nd port -----------------------\n");
            xf86PrintEDID( pRADEONEnt->pOutput[1]->MonInfo );
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
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONOutputPrivatePtr radeon_output;
    xf86OutputPtr output;
    int o;

    pRADEONEnt->Controller[0]->binding = 1;
    pRADEONEnt->Controller[1]->binding = 1;

    for (o = 0; o < xf86_config->num_output; o++) {
      output = xf86_config->output[o];
      radeon_output = output->driver_private;

      xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		 "Port%d:\n Monitor   -- %s\n Connector -- %s\n DAC Type  -- %s\n TMDS Type -- %s\n DDC Type  -- %s\n", 
	  o,
	  MonTypeName[radeon_output->MonType+1],
	  info->IsAtomBios ? 
	  ConnectorTypeNameATOM[radeon_output->ConnectorType]:
	  ConnectorTypeName[radeon_output->ConnectorType],
	  DACTypeName[radeon_output->DACType+1],
	  TMDSTypeName[radeon_output->TMDSType+1],
	  DDCTypeName[radeon_output->DDCType]);

    }

#if 0
    if (!info->IsSecondary) {
      pRADEONEnt->PortInfo[0]->crtc_num = 1;
      pRADEONEnt->PortInfo[1]->crtc_num = 2;


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
		pRADEONEnt->PortInfo[0]->crtc_num = 2;
		pRADEONEnt->PortInfo[1]->crtc_num = 1;
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
		pRADEONEnt->PortInfo[0]->crtc_num = 2;
		pRADEONEnt->PortInfo[1]->crtc_num = 1;
		pRADEONEnt->PortInfo[0]->MonType = MT_NONE;
		head_reversed = TRUE;
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
		    pRADEONEnt->PortInfo[0]->crtc_num = 2;
		    pRADEONEnt->PortInfo[1]->crtc_num = 1;
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
	    output = RADEONGetCrtcConnector(pScrn, 2);
	    radeon_output = output->driver_private;
  	    pRADEONEnt->Controller[1]->binding = 2;
	    if (output) {
		pScrn->monitor->DDC = output->MonInfo;
	    }
	} else {
	    output = RADEONGetCrtcConnector(pScrn, 1);
	    radeon_output = output->driver_private;
  	    pRADEONEnt->Controller[0]->binding = 1;
	    if (output) {
		pScrn->monitor->DDC = output->MonInfo;
	    }
	}
	
	if(!pRADEONEnt->HasSecondary) {
	  pRADEONEnt->Controller[1]->binding = 1;
	} 
    } else {
	output = RADEONGetCrtcConnector(pScrn, 1);
	radeon_output = output->driver_private;
	if (output) {
	    if (radeon_output->MonType == MT_NONE) 
		radeon_output->MonType = MT_CRT;
	    pScrn->monitor->DDC = output->MonInfo;
	}
	output = RADEONGetCrtcConnector(pScrn, 2);
	radeon_output = output->driver_private;
	if (output)
	    radeon_output->MonType = MT_NONE;
	pRADEONEnt->Controller[1]->binding = 1;
    }

    if (!info->IsSecondary) {
	output = RADEONGetCrtcConnector(pScrn, 2);
	radeon_output = output->driver_private;
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "---- Primary Head:   Port%d ---- \n", head_reversed?2:1);
	if (radeon_output->MonType != MT_NONE)
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "---- Secondary Head: Port%d ----\n", head_reversed?1:2);
 	else
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, "---- Secondary Head: Not used ----\n");
    }
#endif

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
    tmpPixclksCntl = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
    if (info->IsMobility || info->IsIGP) {
	/* Asic bug, when turning off LVDS_ON, we have to make sure
	   RADEON_PIXCLK_LVDS_ALWAYS_ON bit is off
	 */
	OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
    }
    tmp = INREG(RADEON_LVDS_GEN_CNTL);
    tmp |= (RADEON_LVDS_ON | RADEON_LVDS_DISPLAY_DIS);
    tmp &= ~(RADEON_LVDS_BLON);
    OUTREG(RADEON_LVDS_GEN_CNTL, tmp);
    if (info->IsMobility || info->IsIGP) {
	OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmpPixclksCntl);
    }

}

/* This is to be used enable/disable displays dynamically */
void RADEONEnableDisplay(ScrnInfoPtr pScrn, xf86OutputPtr output, BOOL bEnable)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONSavePtr save = &info->ModeReg;
    unsigned char * RADEONMMIO = info->MMIO;
    unsigned long tmp;
    RADEONOutputPrivatePtr radeon_output;
    radeon_output = output->driver_private;

    ErrorF("montype: %d\n", radeon_output->MonType);

    if (bEnable) {
        if (radeon_output->MonType == MT_CRT) {
            if (radeon_output->DACType == DAC_PRIMARY) {
                tmp = INREG(RADEON_CRTC_EXT_CNTL);
                tmp |= RADEON_CRTC_CRT_ON;                    
                OUTREG(RADEON_CRTC_EXT_CNTL, tmp);
                save->crtc_ext_cntl |= RADEON_CRTC_CRT_ON;
            } else if (radeon_output->DACType == DAC_TVDAC) {
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
	    RADEONDacPowerSet(pScrn, bEnable, (radeon_output->DACType == DAC_PRIMARY));
        } else if (radeon_output->MonType == MT_DFP) {
            if (radeon_output->TMDSType == TMDS_INT) {
                tmp = INREG(RADEON_FP_GEN_CNTL);
                tmp |= (RADEON_FP_FPON | RADEON_FP_TMDS_EN);
                OUTREG(RADEON_FP_GEN_CNTL, tmp);
                save->fp_gen_cntl |= (RADEON_FP_FPON | RADEON_FP_TMDS_EN);
            } else if (radeon_output->TMDSType == TMDS_EXT) {
                tmp = INREG(RADEON_FP2_GEN_CNTL);
                tmp |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                OUTREG(RADEON_FP2_GEN_CNTL, tmp);
                save->fp2_gen_cntl |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
            }
        } else if (radeon_output->MonType == MT_LCD) {
            tmp = INREG(RADEON_LVDS_GEN_CNTL);
	    ErrorF("read in LVDS reg\n");
            tmp |= (RADEON_LVDS_ON | RADEON_LVDS_BLON);
            tmp &= ~(RADEON_LVDS_DISPLAY_DIS);
	    usleep (info->PanelPwrDly * 1000);
            OUTREG(RADEON_LVDS_GEN_CNTL, tmp);
	    ErrorF("wrote out LVDS reg\n");
            save->lvds_gen_cntl |= (RADEON_LVDS_ON | RADEON_LVDS_BLON);
            save->lvds_gen_cntl &= ~(RADEON_LVDS_DISPLAY_DIS);
        } 
    } else {
        if (radeon_output->MonType == MT_CRT || radeon_output->MonType == NONE) {
            if (radeon_output->DACType == DAC_PRIMARY) {
                tmp = INREG(RADEON_CRTC_EXT_CNTL);
                tmp &= ~RADEON_CRTC_CRT_ON;                    
                OUTREG(RADEON_CRTC_EXT_CNTL, tmp);
                save->crtc_ext_cntl &= ~RADEON_CRTC_CRT_ON;
            } else if (radeon_output->DACType == DAC_TVDAC) {
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
	    RADEONDacPowerSet(pScrn, bEnable, (radeon_output->DACType == DAC_PRIMARY));
        }

        if (radeon_output->MonType == MT_DFP || radeon_output->MonType == NONE) {
            if (radeon_output->TMDSType == TMDS_INT) {
                tmp = INREG(RADEON_FP_GEN_CNTL);
                tmp &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
                OUTREG(RADEON_FP_GEN_CNTL, tmp);
                save->fp_gen_cntl &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
            } else if (radeon_output->TMDSType == TMDS_EXT) {
                tmp = INREG(RADEON_FP2_GEN_CNTL);
                tmp &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                OUTREG(RADEON_FP2_GEN_CNTL, tmp);
                save->fp2_gen_cntl &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
            }
        }

        if (radeon_output->MonType == MT_LCD || 
            (radeon_output->MonType == NONE && radeon_output->ConnectorType == CONNECTOR_PROPRIETARY)) {
	    unsigned long tmpPixclksCntl = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
	    if (info->IsMobility || info->IsIGP) {
	    /* Asic bug, when turning off LVDS_ON, we have to make sure
	       RADEON_PIXCLK_LVDS_ALWAYS_ON bit is off
	    */
		OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
	    }
            tmp = INREG(RADEON_LVDS_GEN_CNTL);
            tmp |= (RADEON_LVDS_ON | RADEON_LVDS_DISPLAY_DIS);
            tmp &= ~(RADEON_LVDS_BLON);
            OUTREG(RADEON_LVDS_GEN_CNTL, tmp);
            save->lvds_gen_cntl |= (RADEON_LVDS_ON | RADEON_LVDS_DISPLAY_DIS);
            save->lvds_gen_cntl &= ~(RADEON_LVDS_BLON);
	    if (info->IsMobility || info->IsIGP) {
		OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmpPixclksCntl);
	    }
        }
    }
    ErrorF("finished output enable\n");
}

/* Calculate display buffer watermark to prevent buffer underflow */
void RADEONInitDispBandwidth2(ScrnInfoPtr pScrn, RADEONInfoPtr info, int pixel_bytes2, DisplayModePtr mode1, DisplayModePtr mode2)
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
	if (pRADEONEnt->pCrtc[1]->enabled) {
	    mc_init_misc_lat_timer |= 0x1100; /* display 0 and 1 */
	} else {
	    mc_init_misc_lat_timer |= 0x0100; /* display 0 only */
	}
	OUTREG(R300_MC_INIT_MISC_LAT_TIMER, mc_init_misc_lat_timer);
    }


    /* R420 and RV410 family not supported yet */
    if (info->ChipFamily == CHIP_FAMILY_R420 || info->ChipFamily == CHIP_FAMILY_RV410) return; 

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
    if (pixel_bytes2)
      peak_disp_bw += (pix_clk2 * pixel_bytes2);

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
    if (pixel_bytes2)
	disp_drain_rate2 = pix_clk2 / (16.0/pixel_bytes2);
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

    RADEONTRACE(("GRPH_BUFFER_CNTL from %x to %x\n",
		 (unsigned int)info->SavedReg.grph_buffer_cntl, INREG(RADEON_GRPH_BUFFER_CNTL)));

    if (mode2) {
	stop_req = mode2->HDisplay * pixel_bytes2 / 16;

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

	RADEONTRACE(("GRPH2_BUFFER_CNTL from %x to %x\n",
		     (unsigned int)info->SavedReg.grph2_buffer_cntl, INREG(RADEON_GRPH2_BUFFER_CNTL)));
    }
}

void RADEONInitDispBandwidth(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    DisplayModePtr mode1, mode2;
    RADEONInfoPtr info2 = NULL;
    xf86CrtcPtr crtc;
    int pixel_bytes2 = 0;

    if (pRADEONEnt->pSecondaryScrn) {
	if (info->IsSecondary) return;
	info2 = RADEONPTR(pRADEONEnt->pSecondaryScrn);
    } else if (pRADEONEnt->Controller[1]->binding == 1) info2 = info;

    mode1 = info->CurrentLayout.mode;
    if ((pRADEONEnt->HasSecondary) && info2) {
	mode2 = info2->CurrentLayout.mode;
    } else {
	mode2 = NULL;
    }

    if (info2) 
      pixel_bytes2 = info2->CurrentLayout.pixel_bytes;
    
    
    if (xf86_config->num_crtc == 2) {
      pixel_bytes2 = 0;
      mode2 = NULL;

      if (xf86_config->crtc[1]->enabled && xf86_config->crtc[0]->enabled) {
	pixel_bytes2 = info->CurrentLayout.pixel_bytes;
	mode1 = &xf86_config->crtc[0]->mode;
	mode2 = &xf86_config->crtc[1]->mode;
      } else if (xf86_config->crtc[0]->enabled) {
	mode1 = &xf86_config->crtc[0]->mode;
      } else if (xf86_config->crtc[1]->enabled) {
	mode1 = &xf86_config->crtc[1]->mode;
      } else
	return;
    }

    RADEONInitDispBandwidth2(pScrn, info, pixel_bytes2, mode1, mode2);
}

void RADEONBlank(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    xf86OutputPtr output;
    xf86CrtcPtr crtc;
    int o, c;

    for (c = 0; c < xf86_config->num_crtc; c++) {
	crtc = xf86_config->crtc[c];
    	for (o = 0; o < xf86_config->num_output; o++) {
	    output = xf86_config->output[o];
	    if (output->crtc != crtc)
		continue;

	    output->funcs->dpms(output, DPMSModeOff);
	}
	crtc->funcs->dpms(crtc, DPMSModeOff);
    }
}

void RADEONUnblank(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    xf86OutputPtr output;
    xf86CrtcPtr crtc;
    int o, c;

    for (c = 0; c < xf86_config->num_crtc; c++) {
	crtc = xf86_config->crtc[c];
	if(!crtc->enabled)
		continue;
	crtc->funcs->dpms(crtc, DPMSModeOn);
    	for (o = 0; o < xf86_config->num_output; o++) {
	    output = xf86_config->output[o];
	    if (output->crtc != crtc)
		continue;

	    output->funcs->dpms(output, DPMSModeOn);
	}
    }
}

static void RADEONDPMSSetOn(xf86OutputPtr output)
{
  ScrnInfoPtr pScrn = output->scrn;
  RADEONInfoPtr  info       = RADEONPTR(pScrn);
  unsigned char *RADEONMMIO = info->MMIO;
  RADEONMonitorType MonType;
  RADEONTmdsType TmdsType;
  RADEONDacType DacType;
  RADEONOutputPrivatePtr radeon_output = output->driver_private;

  MonType = radeon_output->MonType;
  TmdsType = radeon_output->TMDSType;
  DacType = radeon_output->DACType;

  ErrorF("radeon_dpms_on %d %d %d\n", radeon_output->num, MonType, DacType);

  switch(MonType) {
  case MT_LCD:
    OUTREGP (RADEON_LVDS_GEN_CNTL, RADEON_LVDS_BLON, ~RADEON_LVDS_BLON);
    usleep (info->PanelPwrDly * 1000);
    OUTREGP (RADEON_LVDS_GEN_CNTL, RADEON_LVDS_ON, ~RADEON_LVDS_ON);
    break;
  case MT_DFP:
    if (TmdsType == TMDS_EXT) {
      OUTREGP (RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_BLANK_EN);
      OUTREGP (RADEON_FP2_GEN_CNTL, RADEON_FP2_ON, ~RADEON_FP2_ON);
      if (info->ChipFamily >= CHIP_FAMILY_R200) {
	OUTREGP (RADEON_FP2_GEN_CNTL, RADEON_FP2_DVO_EN, 
		 ~RADEON_FP2_DVO_EN);
      }
    } else
      OUTREGP (RADEON_FP_GEN_CNTL, (RADEON_FP_FPON | RADEON_FP_TMDS_EN),
	       ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN));
    break;
  case MT_CRT:
  default:
    RADEONDacPowerSet(pScrn, TRUE, (DacType == DAC_PRIMARY));
    break;
  }
}

static void RADEONDPMSSetOff(xf86OutputPtr output)
{
  ScrnInfoPtr pScrn = output->scrn;
  RADEONInfoPtr  info       = RADEONPTR(pScrn);
  unsigned char *RADEONMMIO = info->MMIO;
  RADEONMonitorType MonType;
  RADEONTmdsType TmdsType;
  RADEONDacType DacType;
  unsigned long tmpPixclksCntl;
  RADEONOutputPrivatePtr radeon_output = output->driver_private;

  MonType = radeon_output->MonType;
  TmdsType = radeon_output->TMDSType;
  DacType = radeon_output->DACType;

  switch(MonType) {
  case MT_LCD:
    tmpPixclksCntl = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
    if (info->IsMobility || info->IsIGP) {
      /* Asic bug, when turning off LVDS_ON, we have to make sure
	 RADEON_PIXCLK_LVDS_ALWAYS_ON bit is off
      */
      OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
    }
    OUTREGP (RADEON_LVDS_GEN_CNTL, 0,
	     ~(RADEON_LVDS_BLON | RADEON_LVDS_ON));
    if (info->IsMobility || info->IsIGP) {
      OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmpPixclksCntl);
    }
    break;
  case MT_DFP:
    if (TmdsType == TMDS_EXT) {
      OUTREGP (RADEON_FP2_GEN_CNTL, RADEON_FP2_BLANK_EN, ~RADEON_FP2_BLANK_EN);
      OUTREGP (RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_ON);
      if (info->ChipFamily >= CHIP_FAMILY_R200) {
	OUTREGP (RADEON_FP2_GEN_CNTL, 0, ~RADEON_FP2_DVO_EN);
      }
    } else
      OUTREGP (RADEON_FP_GEN_CNTL, 0, ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN));
    break;
  case MT_CRT:
  default:
    RADEONDacPowerSet(pScrn, FALSE, (DacType == DAC_PRIMARY));
    break;
  }
}


static void
radeon_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
  int mask;
  ScrnInfoPtr pScrn = crtc->scrn;
  RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
  RADEONInfoPtr info = RADEONPTR(pScrn);
  unsigned char *RADEONMMIO = info->MMIO;
    
  mask = radeon_crtc->crtc_id ? (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS) : (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS | RADEON_CRTC_VSYNC_DIS);

  switch(mode) {
  case DPMSModeOn:
    if (radeon_crtc->crtc_id) {
      OUTREGP(RADEON_CRTC2_GEN_CNTL, 0, ~mask);
    } else {
      OUTREGP(RADEON_CRTC_EXT_CNTL, 0, ~mask);
    }
    break;
  case DPMSModeStandby:
    if (radeon_crtc->crtc_id) {
      OUTREGP(RADEON_CRTC2_GEN_CNTL, (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_HSYNC_DIS), ~mask);
    } else {
      OUTREGP(RADEON_CRTC_EXT_CNTL, (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS), ~mask);
    }
    break;
  case DPMSModeSuspend:
    if (radeon_crtc->crtc_id) {
      OUTREGP(RADEON_CRTC2_GEN_CNTL, (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS), ~mask);
    } else {
      OUTREGP(RADEON_CRTC_EXT_CNTL, (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS), ~mask);
    }
    break;
  case DPMSModeOff:
    if (radeon_crtc->crtc_id) {
      OUTREGP(RADEON_CRTC2_GEN_CNTL, mask, ~mask);
    } else {
      OUTREGP(RADEON_CRTC_EXT_CNTL, mask, ~mask);
    }
    break;
  }
  
  if (mode != DPMSModeOff)
    radeon_crtc_load_lut(crtc);  
}

static Bool
radeon_crtc_mode_fixup(xf86CrtcPtr crtc, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode)
{
    return TRUE;
}

static void
radeon_crtc_mode_prepare(xf86CrtcPtr crtc)
{
}

static void
radeon_crtc_mode_set(xf86CrtcPtr crtc, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode, int x, int y)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONMonitorType montype;
    int i = 0;

    for (i = 0; i < xf86_config->num_output; i++) {
      xf86OutputPtr output = xf86_config->output[i];
      RADEONOutputPrivatePtr radeon_output = output->driver_private;

      if (output->crtc == crtc) {
	montype = radeon_output->MonType;
	radeon_output->crtc_num = radeon_crtc->crtc_id + 1;
	ErrorF("using crtc: %d on output %s montype: %d\n", radeon_output->crtc_num, OutputType[radeon_output->type], montype);
      }
    }
    
    switch (radeon_crtc->crtc_id) {
    case 0: 
      RADEONInit2(pScrn, adjusted_mode, NULL, 1, &info->ModeReg, montype);
      break;
    case 1: 
      RADEONInit2(pScrn, NULL, adjusted_mode, 2, &info->ModeReg, montype);
      break;
    }

    RADEONBlank(pScrn);
    if (radeon_crtc->crtc_id == 0)
	RADEONDoAdjustFrame(pScrn, x, y, FALSE);
    else if (radeon_crtc->crtc_id == 1)
	RADEONDoAdjustFrame(pScrn, x, y, TRUE);
    RADEONRestoreMode(pScrn, &info->ModeReg);

    ErrorF("mode restored\n");

    ErrorF("frame adjusted\n");

    if (info->DispPriority)
        RADEONInitDispBandwidth(pScrn);
    ErrorF("bandwidth set\n");
    RADEONUnblank(pScrn);
    ErrorF("unblank\n");
}

static void
radeon_crtc_mode_commit(xf86CrtcPtr crtc)
{
}

void radeon_crtc_load_lut(xf86CrtcPtr crtc)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int i;

    if (!crtc->enabled)
	return;

    PAL_SELECT(radeon_crtc->crtc_id);

    for (i = 0; i < 256; i++) {
	OUTPAL(i, radeon_crtc->lut_r[i], radeon_crtc->lut_g[i], radeon_crtc->lut_b[i]);
    }
}


static void
radeon_crtc_gamma_set(xf86CrtcPtr crtc, CARD16 *red, CARD16 *green, 
		      CARD16 *blue, int size)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    int i;

    for (i = 0; i < 256; i++) {
	radeon_crtc->lut_r[i] = red[i] >> 8;
	radeon_crtc->lut_g[i] = green[i] >> 8;
	radeon_crtc->lut_b[i] = blue[i] >> 8;
    }

    radeon_crtc_load_lut(crtc);
}

static Bool
radeon_crtc_lock(xf86CrtcPtr crtc)
{
    ScrnInfoPtr		pScrn = crtc->scrn;
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    Bool           CPStarted   = info->CPStarted;

    if (info->accelOn)
        RADEON_SYNC(info, pScrn);
    return FALSE;
}

static void
radeon_crtc_unlock(xf86CrtcPtr crtc)
{
    ScrnInfoPtr		pScrn = crtc->scrn;
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (info->accelOn)
        RADEON_SYNC(info, pScrn);
}

static const xf86CrtcFuncsRec radeon_crtc_funcs = {
    .dpms = radeon_crtc_dpms,
    .save = NULL, /* XXX */
    .restore = NULL, /* XXX */
    .mode_fixup = radeon_crtc_mode_fixup,
    .prepare = radeon_crtc_mode_prepare,
    .mode_set = radeon_crtc_mode_set,
    .commit = radeon_crtc_mode_commit,
    .gamma_set = radeon_crtc_gamma_set,
    .lock = radeon_crtc_lock,
    .unlock = radeon_crtc_unlock,
    .set_cursor_colors = radeon_crtc_set_cursor_colors,
    .set_cursor_position = radeon_crtc_set_cursor_position,
    .show_cursor = radeon_crtc_show_cursor,
    .hide_cursor = radeon_crtc_hide_cursor,
/*    .load_cursor_image = i830_crtc_load_cursor_image, */
    .load_cursor_argb = radeon_crtc_load_cursor_argb,
    .destroy = NULL, /* XXX */
};

static void
radeon_dpms(xf86OutputPtr output, int mode)
{
    switch(mode) {
    case DPMSModeOn:
      RADEONDPMSSetOn(output);
      break;
    case DPMSModeOff:
    case DPMSModeSuspend:
    case DPMSModeStandby:
      RADEONDPMSSetOff(output);
      break;
    }
}

static void
radeon_save(xf86OutputPtr output)
{

}

static void
radeon_restore(xf86OutputPtr restore)
{

}

static int
radeon_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
    ScrnInfoPtr	pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    DisplayModePtr m;

    if (radeon_output->type != OUTPUT_LVDS)
	return MODE_OK;

    if (pMode->HDisplay > info->PanelXRes ||
	pMode->VDisplay > info->PanelYRes)
	return MODE_PANEL;

    return MODE_OK;
}

static Bool
radeon_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		    DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr	pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    if (radeon_output->type != OUTPUT_LVDS)
	return TRUE;

    if (mode->HDisplay < info->PanelXRes ||
	mode->VDisplay < info->PanelYRes)
	adjusted_mode->Flags |= RADEON_USE_RMX;

    return TRUE;
}

static void
radeon_mode_prepare(xf86OutputPtr output)
{
}

static void
radeon_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		  DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    
    //    RADEONInitOutputRegisters(pScrn, save, mode, pRADEONEnt->pOutput[0], );
}

static void
radeon_mode_commit(xf86OutputPtr output)
{
}

static xf86OutputStatus
radeon_detect(xf86OutputPtr output)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    
    radeon_output->MonType = MT_UNKNOWN;
    RADEONConnectorFindMonitor(pScrn, output);
    if (radeon_output->MonType == MT_UNKNOWN) {
        output->subpixel_order = SubPixelUnknown;
	return XF86OutputStatusUnknown;
    }
    else if (radeon_output->MonType == MT_NONE) {
        output->subpixel_order = SubPixelUnknown;
	return XF86OutputStatusDisconnected;
    } else {

      switch(radeon_output->MonType) {
      case MT_LCD:
      case MT_DFP: output->subpixel_order = SubPixelHorizontalRGB; break;
      default: output->subpixel_order = SubPixelNone; break;
      }
      
      return XF86OutputStatusConnected;
    }

}

static DisplayModePtr
radeon_get_modes(xf86OutputPtr output)
{
  DisplayModePtr modes;
  modes = RADEONProbeOutputModes(output);
  return modes;
}

static void
radeon_destroy (xf86OutputPtr output)
{
    if(output->driver_private)
        xfree(output->driver_private);
}

static const xf86OutputFuncsRec radeon_output_funcs = {
    .dpms = radeon_dpms,
    .save = radeon_save,
    .restore = radeon_restore,
    .mode_valid = radeon_mode_valid,
    .mode_fixup = radeon_mode_fixup,
    .prepare = radeon_mode_prepare,
    .mode_set = radeon_mode_set,
    .commit = radeon_mode_commit,
    .detect = radeon_detect,
    .get_modes = radeon_get_modes,
    .destroy = radeon_destroy
};

Bool RADEONAllocateControllers(ScrnInfoPtr pScrn)
{
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);

    if (pRADEONEnt->Controller[0])
      return TRUE;

    pRADEONEnt->pCrtc[0] = xf86CrtcCreate(pScrn, &radeon_crtc_funcs);
    if (!pRADEONEnt->pCrtc[0])
      return FALSE;

    pRADEONEnt->Controller[0] = xnfcalloc(sizeof(RADEONCrtcPrivateRec), 1);
    if (!pRADEONEnt->Controller[0])
        return FALSE;

    pRADEONEnt->pCrtc[0]->driver_private = pRADEONEnt->Controller[0];
    pRADEONEnt->Controller[0]->crtc_id = 0;

    if (!pRADEONEnt->HasCRTC2)
	return TRUE;

    pRADEONEnt->pCrtc[1] = xf86CrtcCreate(pScrn, &radeon_crtc_funcs);
    if (!pRADEONEnt->pCrtc[1])
      return FALSE;

    pRADEONEnt->Controller[1] = xnfcalloc(sizeof(RADEONCrtcPrivateRec), 1);
    if (!pRADEONEnt->Controller[1])
    {
	xfree(pRADEONEnt->Controller[0]);
	return FALSE;
    }

    pRADEONEnt->pCrtc[1]->driver_private = pRADEONEnt->Controller[1];
    pRADEONEnt->Controller[1]->crtc_id = 1;
    return TRUE;
}

Bool RADEONAllocatePortInfo(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr      info = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    int num_connectors;
    int i;

    if (pRADEONEnt->PortInfo[0])
	return TRUE;

    /* when we support TV, this should be incremented */
    if (info->IsMobility) {
      /* DVI on docks */
      info->max_connectors = 3;
    } else {
      info->max_connectors = 2;
    }

    /* for now always allocate max connectors */
    for (i = 0 ; i < info->max_connectors; i++) {

	pRADEONEnt->PortInfo[i] = xnfcalloc(sizeof(RADEONOutputPrivateRec), 1);
	if (!pRADEONEnt->PortInfo[i])
	    return FALSE;
    }
    return TRUE;
}

void RADEONSetOutputType(ScrnInfoPtr pScrn, RADEONOutputPrivatePtr radeon_output)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    RADEONOutputType output;
    if (info->IsAtomBios) {
	switch(radeon_output->ConnectorType) {
	case 0: output = OUTPUT_NONE; break;
	case 1: output = OUTPUT_VGA; break;
	case 2:
	case 3:
	case 4: output = OUTPUT_DVI; break;
	case 5: output = OUTPUT_STV; break;
	case 6: output = OUTPUT_CTV; break;
	case 7:
	case 8: output = OUTPUT_LVDS; break;
	case 9:
	default:
	    output = OUTPUT_NONE; break;
	}
    }
    else {
	switch(radeon_output->ConnectorType) {
	case 0: output = OUTPUT_NONE; break;
	case 1: output = OUTPUT_LVDS; break;
	case 2: output = OUTPUT_VGA; break;
	case 3:
	case 4: output = OUTPUT_DVI; break;
	case 5: output = OUTPUT_STV; break;
	case 6: output = OUTPUT_CTV; break;
	default: output = OUTPUT_NONE; break;
	}
    }
    radeon_output->type = output;
}

Bool RADEONAllocateConnectors(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr      info = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    int i;

    if (pRADEONEnt->pOutput[0])
        return TRUE;
    
    /* for now always allocate max connectors */
    for (i = 0 ; i < info->max_connectors; i++) {

	pRADEONEnt->pOutput[i] = xf86OutputCreate(pScrn, &radeon_output_funcs, OutputType[pRADEONEnt->PortInfo[i]->type]);
	if (!pRADEONEnt->pOutput[i])
	    return FALSE;
	
	pRADEONEnt->pOutput[i]->driver_private = pRADEONEnt->PortInfo[i];
	pRADEONEnt->PortInfo[i]->num = i;

	pRADEONEnt->pOutput[i]->possible_crtcs = 1;
	if (pRADEONEnt->PortInfo[i]->type != OUTPUT_LVDS)
 	    pRADEONEnt->pOutput[i]->possible_crtcs |= 2;

	pRADEONEnt->pOutput[i]->possible_clones = 0 /*1|2*/;
    }

    return TRUE;
}


#if 0
xf86OutputPtr RADEONGetCrtcConnector(ScrnInfoPtr pScrn, int crtc_num)
{
    RADEONInfoPtr      info = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    int i;

    for (i = 0; i < info->max_connectors; i++) {
        if (pRADEONEnt->PortInfo[i]->crtc_num == crtc_num)
	    return pRADEONEnt->pOutput[i];
    }
    return NULL;
}
#endif

/**
 * In the current world order, there are lists of modes per output, which may
 * or may not include the mode that was asked to be set by XFree86's mode
 * selection.  Find the closest one, in the following preference order:
 *
 * - Equality
 * - Closer in size to the requested mode, but no larger
 * - Closer in refresh rate to the requested mode.
 */
DisplayModePtr
RADEONCrtcFindClosestMode(xf86CrtcPtr crtc, DisplayModePtr pMode)
{
    ScrnInfoPtr	pScrn = crtc->scrn;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    DisplayModePtr pBest = NULL, pScan = NULL;
    int i;

    /* Assume that there's only one output connected to the given CRTC. */
    for (i = 0; i < xf86_config->num_output; i++) 
    {
	xf86OutputPtr  output = xf86_config->output[i];
	if (output->crtc == crtc && output->probed_modes != NULL)
	{
	    pScan = output->probed_modes;
	    break;
	}
    }

    /* If the pipe doesn't have any detected modes, just let the system try to
     * spam the desired mode in.
     */
    if (pScan == NULL) {
	RADEONCrtcPrivatePtr  radeon_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "No crtc mode list for crtc %d,"
		   "continuing with desired mode\n", radeon_crtc->crtc_id);
	return pMode;
    }

    for (; pScan != NULL; pScan = pScan->next) {
	assert(pScan->VRefresh != 0.0);

	/* If there's an exact match, we're done. */
	if (xf86ModesEqual(pScan, pMode)) {
	    pBest = pMode;
	    break;
	}

	/* Reject if it's larger than the desired mode. */
	if (pScan->HDisplay > pMode->HDisplay ||
	    pScan->VDisplay > pMode->VDisplay)
	{
	    continue;
	}

	if (pBest == NULL) {
	    pBest = pScan;
	    continue;
	}

	/* Find if it's closer to the right size than the current best
	 * option.
	 */
	if ((pScan->HDisplay > pBest->HDisplay &&
	     pScan->VDisplay >= pBest->VDisplay) ||
	    (pScan->HDisplay >= pBest->HDisplay &&
	     pScan->VDisplay > pBest->VDisplay))
	{
	    pBest = pScan;
	    continue;
	}

	/* Find if it's still closer to the right refresh than the current
	 * best resolution.
	 */
	if (pScan->HDisplay == pBest->HDisplay &&
	    pScan->VDisplay == pBest->VDisplay &&
	    (fabs(pScan->VRefresh - pMode->VRefresh) <
	     fabs(pBest->VRefresh - pMode->VRefresh))) {
	    pBest = pScan;
	}
    }

    if (pBest == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "No suitable mode found to program for the pipe.\n"
		   "	continuing with desired mode %dx%d@%.1f\n",
		   pMode->HDisplay, pMode->VDisplay, pMode->VRefresh);
    } else if (!xf86ModesEqual(pBest, pMode)) {
      RADEONCrtcPrivatePtr  radeon_crtc = crtc->driver_private;
      int		    crtc = radeon_crtc->crtc_id;
      xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Choosing pipe %d's mode %dx%d@%.1f instead of xf86 "
		   "mode %dx%d@%.1f\n", crtc,
		   pBest->HDisplay, pBest->VDisplay, pBest->VRefresh,
		   pMode->HDisplay, pMode->VDisplay, pMode->VRefresh);
	pMode = pBest;
    }
    return pMode;
}

void
RADEONChooseOverlayCRTC(ScrnInfoPtr pScrn, BoxPtr dstBox)
{
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    int c;
    int highx = 0, highy = 0;
    int crtc_num;

    for (c = 0; c < xf86_config->num_crtc; c++)
    {
        xf86CrtcPtr crtc = xf86_config->crtc[c];

	if (!crtc->enabled)
	    continue;

	if ((dstBox->x1 >= crtc->x) && (dstBox->y1 >= crtc->y))
	    crtc_num = c;
    }

    if (crtc_num == 1)
        info->OverlayOnCRTC2 = TRUE;
    else
        info->OverlayOnCRTC2 = FALSE;
}
