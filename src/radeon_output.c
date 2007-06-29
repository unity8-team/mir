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

const char *TMDSTypeName[4] = {
  "Unknown",
  "Internal",
  "External",
  "None"
};

const char *DDCTypeName[6] = {
  "NONE",
  "MONID",
  "DVI_DDC",
  "VGA_DDC",
  "CRT2_DDC",
  "LCD_DDC"
};

const char *DACTypeName[4] = {
  "Unknown",
  "Primary",
  "TVDAC/ExtDAC",
  "None"
};

const char *ConnectorTypeName[8] = {
  "None",
  "Proprietary/LVDS",
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

static RADEONMonitorType RADEONPortCheckNonDDC(ScrnInfoPtr pScrn, xf86OutputPtr output);
static void RADEONUpdatePanelSize(xf86OutputPtr output);

void RADEONPrintPortMap(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONOutputPrivatePtr radeon_output;
    xf86OutputPtr output;
    int o;

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

}

static RADEONMonitorType
RADEONDisplayDDCConnected(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long DDCReg;
    RADEONMonitorType MonType = MT_NONE;
    xf86MonPtr* MonInfo = &output->MonInfo;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONDDCType DDCType = radeon_output->DDCType;
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
	if ((info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_LVDS_ATOM) ||
	    (!info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_PROPRIETARY)) {
	    MonType = MT_LCD;
	} else if ((info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_DVI_D_ATOM) ||
		 (!info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_DVI_D)) {
	    MonType = MT_DFP;
	} else if ((*MonInfo)->rawData[0x14] & 0x80) {	/* if it's digital */
	    MonType = MT_DFP;
	} else {
	    MonType = MT_CRT;
	}
    } else MonType = MT_NONE;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "DDC Type: %d, Detected Monitor Type: %d\n", DDCType, MonType);

    return MonType;
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

/* Primary Head (DVI or Laptop Int. panel)*/
/* A ddc capable display connected on DVI port */
/* Secondary Head (mostly VGA, can be DVI on some OEM boards)*/
void RADEONConnectorFindMonitor(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    
    if (radeon_output->MonType == MT_UNKNOWN) {
	if ((radeon_output->MonType = RADEONDisplayDDCConnected(pScrn, output)));
	else if((radeon_output->MonType = RADEONPortCheckNonDDC(pScrn, output)));
	else if (radeon_output->DACType == DAC_PRIMARY) 
	    radeon_output->MonType = RADEONCrtIsPhysicallyConnected(pScrn, !(radeon_output->DACType));
    }

    /* update panel info for RMX */
    if (radeon_output->MonType == MT_LCD || radeon_output->MonType == MT_DFP)
	RADEONUpdatePanelSize(output);

    if (output->MonInfo) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID data from the display on connector: %s ----------------------\n",
		   info->IsAtomBios ?
		   ConnectorTypeNameATOM[radeon_output->ConnectorType]:
		   ConnectorTypeName[radeon_output->ConnectorType]
		   );
	xf86PrintEDID( output->MonInfo );
    }
}

static RADEONMonitorType RADEONPortCheckNonDDC(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONMonitorType MonType = MT_NONE;


    if (radeon_output->type == OUTPUT_LVDS) {
	if (INREG(RADEON_BIOS_4_SCRATCH) & 4)
	    MonType =  MT_LCD;
    } else if (radeon_output->type == OUTPUT_DVI) {
	if (radeon_output->TMDSType == TMDS_INT) {
	    if (INREG(RADEON_FP_GEN_CNTL) & RADEON_FP_DETECT_SENSE)
		MonType = MT_DFP;
	} else if (radeon_output->TMDSType == TMDS_EXT) {
	    if (INREG(RADEON_FP2_GEN_CNTL) & RADEON_FP2_DETECT_SENSE)
		MonType = MT_DFP;
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Detected Monitor Type: %d\n", MonType);

    return MonType;

}

static void
radeon_dpms(xf86OutputPtr output, int mode)
{
    switch(mode) {
    case DPMSModeOn:
	RADEONEnableDisplay(output, TRUE);
	break;
    case DPMSModeOff:
    case DPMSModeSuspend:
    case DPMSModeStandby:
	RADEONEnableDisplay(output, FALSE);
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
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    if (radeon_output->type != OUTPUT_LVDS)
	return MODE_OK;

    if (pMode->HDisplay > radeon_output->PanelXRes ||
	pMode->VDisplay > radeon_output->PanelYRes)
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

    if (radeon_output->MonType == MT_LCD || radeon_output->MonType == MT_DFP) {
	xf86CrtcPtr crtc = output->crtc;
	RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;

	if ((mode->HDisplay < radeon_output->PanelXRes ||
	     mode->VDisplay < radeon_output->PanelYRes) &&
	    radeon_crtc->crtc_id == 0)
	    adjusted_mode->Flags |= RADEON_USE_RMX;

	if (adjusted_mode->Flags & RADEON_USE_RMX) {
	    adjusted_mode->CrtcHTotal     = mode->CrtcHDisplay + radeon_output->HBlank;
	    adjusted_mode->CrtcHSyncStart = mode->CrtcHDisplay + radeon_output->HOverPlus;
	    adjusted_mode->CrtcHSyncEnd   = mode->CrtcHSyncStart + radeon_output->HSyncWidth;
	    adjusted_mode->CrtcVTotal     = mode->CrtcVDisplay + radeon_output->VBlank;
	    adjusted_mode->CrtcVSyncStart = mode->CrtcVDisplay + radeon_output->VOverPlus;
	    adjusted_mode->CrtcVSyncEnd   = mode->CrtcVSyncStart + radeon_output->VSyncWidth;
	    adjusted_mode->Clock          = radeon_output->DotClock;
	    radeon_output->Flags |= RADEON_USE_RMX;
	    adjusted_mode->Flags          = radeon_output->Flags;
	} else
	    radeon_output->Flags &= ~RADEON_USE_RMX;

    }

    return TRUE;
}

static void
radeon_mode_prepare(xf86OutputPtr output)
{
}

static void RADEONInitFPRegisters(xf86OutputPtr output, RADEONSavePtr save,
				  DisplayModePtr mode, BOOL IsPrimary)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr  pRADEONEnt = RADEONEntPriv(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int i;
    CARD32 tmp = info->SavedReg.tmds_pll_cntl & 0xfffff;

    for (i=0; i<4; i++) {
	if (radeon_output->tmds_pll[i].freq == 0) break;
	if ((CARD32)(mode->Clock/10) < radeon_output->tmds_pll[i].freq) {
	    tmp = radeon_output->tmds_pll[i].value ;
	    break;
	}
    }

    if (IS_R300_VARIANT || (info->ChipFamily == CHIP_FAMILY_RV280)) {
	if (tmp & 0xfff00000)
	    save->tmds_pll_cntl = tmp;
	else {
	    save->tmds_pll_cntl = info->SavedReg.tmds_pll_cntl & 0xfff00000;
	    save->tmds_pll_cntl |= tmp;
	}
    } else save->tmds_pll_cntl = tmp;

    save->tmds_transmitter_cntl = info->SavedReg.tmds_transmitter_cntl &
					~(RADEON_TMDS_TRANSMITTER_PLLRST);

    if (IS_R300_VARIANT || (info->ChipFamily == CHIP_FAMILY_R200) || !pRADEONEnt->HasCRTC2)
	save->tmds_transmitter_cntl &= ~(RADEON_TMDS_TRANSMITTER_PLLEN);
    else /* weird, RV chips got this bit reversed? */
        save->tmds_transmitter_cntl |= (RADEON_TMDS_TRANSMITTER_PLLEN);

    save->fp_gen_cntl = info->SavedReg.fp_gen_cntl |
			 (RADEON_FP_CRTC_DONT_SHADOW_VPAR |
			  RADEON_FP_CRTC_DONT_SHADOW_HEND );

    save->fp_gen_cntl &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);

    if (pScrn->rgbBits == 8)
        save->fp_gen_cntl |= RADEON_FP_PANEL_FORMAT;  /* 24 bit format */
    else
        save->fp_gen_cntl &= ~RADEON_FP_PANEL_FORMAT;/* 18 bit format */


    if (IsPrimary) {
	if ((IS_R300_VARIANT) || (info->ChipFamily == CHIP_FAMILY_R200)) {
	    save->fp_gen_cntl &= ~R200_FP_SOURCE_SEL_MASK;
	    if (mode->Flags & RADEON_USE_RMX) 
		save->fp_gen_cntl |= R200_FP_SOURCE_SEL_RMX;
	    else
		save->fp_gen_cntl |= R200_FP_SOURCE_SEL_CRTC1;
	} else 
	    save->fp_gen_cntl |= RADEON_FP_SEL_CRTC1;
    } else {
	if ((IS_R300_VARIANT) || (info->ChipFamily == CHIP_FAMILY_R200)) {
	    save->fp_gen_cntl &= ~R200_FP_SOURCE_SEL_MASK;
	    save->fp_gen_cntl |= R200_FP_SOURCE_SEL_CRTC2;
	} else 
	    save->fp_gen_cntl |= RADEON_FP_SEL_CRTC2;
    }

}

static void RADEONInitFP2Registers(xf86OutputPtr output, RADEONSavePtr save,
				   DisplayModePtr mode, BOOL IsPrimary)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info       = RADEONPTR(pScrn);


    if (pScrn->rgbBits == 8) 
	save->fp2_gen_cntl = info->SavedReg.fp2_gen_cntl |
				RADEON_FP2_PANEL_FORMAT; /* 24 bit format, */
    else
	save->fp2_gen_cntl = info->SavedReg.fp2_gen_cntl &
				~RADEON_FP2_PANEL_FORMAT;/* 18 bit format, */

    save->fp2_gen_cntl &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);

    if (IsPrimary) {
        if ((info->ChipFamily == CHIP_FAMILY_R200) || IS_R300_VARIANT) {
            save->fp2_gen_cntl   &= ~(R200_FP2_SOURCE_SEL_MASK | 
                                      RADEON_FP2_DVO_EN |
                                      RADEON_FP2_DVO_RATE_SEL_SDR);
	if (mode->Flags & RADEON_USE_RMX) 
	    save->fp2_gen_cntl |= R200_FP2_SOURCE_SEL_RMX;
        } else {
            save->fp2_gen_cntl   &= ~(RADEON_FP2_SRC_SEL_CRTC2 | 
                                      RADEON_FP2_DVO_RATE_SEL_SDR);
            }
    } else {
        if ((info->ChipFamily == CHIP_FAMILY_R200) || IS_R300_VARIANT) {
            save->fp2_gen_cntl &= ~(R200_FP2_SOURCE_SEL_MASK | 
                                    RADEON_FP2_DVO_RATE_SEL_SDR);
            save->fp2_gen_cntl |= R200_FP2_SOURCE_SEL_CRTC2;
        } else {
            save->fp2_gen_cntl &= ~(RADEON_FP2_DVO_RATE_SEL_SDR);
            save->fp2_gen_cntl |= RADEON_FP2_SRC_SEL_CRTC2;
        }
    }

}

static void RADEONInitLVDSRegisters(xf86OutputPtr output, RADEONSavePtr save,
				    DisplayModePtr mode, BOOL IsPrimary)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

    save->lvds_pll_cntl = info->SavedReg.lvds_pll_cntl;

    save->lvds_gen_cntl = info->SavedReg.lvds_gen_cntl;
    save->lvds_gen_cntl |= RADEON_LVDS_DISPLAY_DIS;
    save->lvds_gen_cntl &= ~(RADEON_LVDS_ON | RADEON_LVDS_BLON);

    if (IsPrimary)
	save->lvds_gen_cntl &= ~RADEON_LVDS_SEL_CRTC2;
    else
	save->lvds_gen_cntl |= RADEON_LVDS_SEL_CRTC2;

}

static void RADEONInitRMXRegisters(xf86OutputPtr output, RADEONSavePtr save,
				   DisplayModePtr mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int    xres = mode->HDisplay;
    int    yres = mode->VDisplay;
    float  Hratio, Vratio;

    if (radeon_output->PanelXRes == 0 || radeon_output->PanelYRes == 0) {
	Hratio = 1.0;
	Vratio = 1.0;
    } else {
	if (xres > radeon_output->PanelXRes) xres = radeon_output->PanelXRes;
	if (yres > radeon_output->PanelYRes) yres = radeon_output->PanelYRes;
	    
	Hratio = (float)xres/(float)radeon_output->PanelXRes;
	Vratio = (float)yres/(float)radeon_output->PanelYRes;
    }

	save->fp_vert_stretch = info->SavedReg.fp_vert_stretch &
				  RADEON_VERT_STRETCH_RESERVED;
	save->fp_horz_stretch = info->SavedReg.fp_horz_stretch &
				  (RADEON_HORZ_FP_LOOP_STRETCH |
				  RADEON_HORZ_AUTO_RATIO_INC);

    if (Hratio == 1.0 || !(mode->Flags & RADEON_USE_RMX)) {
	save->fp_horz_stretch |= ((xres/8-1)<<16);
    } else {
	save->fp_horz_stretch |= ((((unsigned long)(Hratio * RADEON_HORZ_STRETCH_RATIO_MAX +
				     0.5)) & RADEON_HORZ_STRETCH_RATIO_MASK) |
				    RADEON_HORZ_STRETCH_BLEND |
				    RADEON_HORZ_STRETCH_ENABLE |
				    ((radeon_output->PanelXRes/8-1)<<16));
    }

    if (Vratio == 1.0 || !(mode->Flags & RADEON_USE_RMX)) {
	save->fp_vert_stretch |= ((yres-1)<<12);
    } else {
	save->fp_vert_stretch |= ((((unsigned long)(Vratio * RADEON_VERT_STRETCH_RATIO_MAX +
						0.5)) & RADEON_VERT_STRETCH_RATIO_MASK) |
				      RADEON_VERT_STRETCH_ENABLE |
				      RADEON_VERT_STRETCH_BLEND |
				      ((radeon_output->PanelYRes-1)<<12));
    }

}

static void RADEONInitDACRegisters(xf86OutputPtr output, RADEONSavePtr save,
				  DisplayModePtr mode, BOOL IsPrimary)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

    if (IsPrimary) {
	if ((info->ChipFamily == CHIP_FAMILY_R200) || IS_R300_VARIANT) {
            save->disp_output_cntl = info->SavedReg.disp_output_cntl &
					~RADEON_DISP_DAC_SOURCE_MASK;
        } else {
            save->dac2_cntl = info->SavedReg.dac2_cntl & ~(RADEON_DAC2_DAC_CLK_SEL);
        }
    } else {
        if ((info->ChipFamily == CHIP_FAMILY_R200) || IS_R300_VARIANT) {
            save->disp_output_cntl = info->SavedReg.disp_output_cntl &
					~RADEON_DISP_DAC_SOURCE_MASK;
            save->disp_output_cntl |= RADEON_DISP_DAC_SOURCE_CRTC2;
        } else {
            save->dac2_cntl = info->SavedReg.dac2_cntl | RADEON_DAC2_DAC_CLK_SEL;
        }
    }
    save->dac_cntl = (RADEON_DAC_MASK_ALL
		      | RADEON_DAC_VGA_ADR_EN
		      | (info->dac6bits ? 0 : RADEON_DAC_8BIT_EN));

    save->dac_macro_cntl = info->SavedReg.dac_macro_cntl;
}

/* XXX: fix me */
static void
RADEONInitTvDacCntl(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

    if (info->ChipFamily == CHIP_FAMILY_R420 ||
	info->ChipFamily == CHIP_FAMILY_RV410) {
	save->tv_dac_cntl = info->SavedReg.tv_dac_cntl &
			     ~(RADEON_TV_DAC_STD_MASK |
			       RADEON_TV_DAC_BGADJ_MASK |
			       R420_TV_DAC_DACADJ_MASK |
			       R420_TV_DAC_RDACPD |
			       R420_TV_DAC_GDACPD |
			       R420_TV_DAC_GDACPD |
			       R420_TV_DAC_TVENABLE);
    } else {
	save->tv_dac_cntl = info->SavedReg.tv_dac_cntl &
			     ~(RADEON_TV_DAC_STD_MASK |
			       RADEON_TV_DAC_BGADJ_MASK |
			       RADEON_TV_DAC_DACADJ_MASK |
			       RADEON_TV_DAC_RDACPD |
			       RADEON_TV_DAC_GDACPD |
			       RADEON_TV_DAC_GDACPD);
    }
    /* FIXME: doesn't make sense, this just replaces the previous value... */
    save->tv_dac_cntl |= (RADEON_TV_DAC_NBLANK |
			 RADEON_TV_DAC_NHOLD |
			  RADEON_TV_DAC_STD_PS2);
			  //			 info->tv_dac_adj);
}

static void RADEONInitDAC2Registers(xf86OutputPtr output, RADEONSavePtr save,
				  DisplayModePtr mode, BOOL IsPrimary)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

    /*0x0028023;*/
    RADEONInitTvDacCntl(pScrn, save);

    if (IsPrimary) {
        save->dac2_cntl = info->SavedReg.dac2_cntl | RADEON_DAC2_DAC2_CLK_SEL;
        if (IS_R300_VARIANT) {
            save->disp_output_cntl = info->SavedReg.disp_output_cntl &
					~RADEON_DISP_TVDAC_SOURCE_MASK;
            save->disp_output_cntl |= RADEON_DISP_TVDAC_SOURCE_CRTC;
        } else if (info->ChipFamily == CHIP_FAMILY_R200) {
	    save->fp2_gen_cntl = info->SavedReg.fp2_gen_cntl &
				  ~(R200_FP2_SOURCE_SEL_MASK |
				    RADEON_FP2_DVO_RATE_SEL_SDR);
	} else {
            save->disp_hw_debug = info->SavedReg.disp_hw_debug | RADEON_CRT2_DISP1_SEL;
        }
    } else {
            save->dac2_cntl = info->SavedReg.dac2_cntl | RADEON_DAC2_DAC2_CLK_SEL;
        if (IS_R300_VARIANT) {
            save->dac2_cntl = info->SavedReg.dac2_cntl | RADEON_DAC2_DAC2_CLK_SEL;
            save->disp_output_cntl = info->SavedReg.disp_output_cntl &
					~RADEON_DISP_TVDAC_SOURCE_MASK;
            save->disp_output_cntl |= RADEON_DISP_TVDAC_SOURCE_CRTC2;
	} else if (info->ChipFamily == CHIP_FAMILY_R200) {
	    save->fp2_gen_cntl = info->SavedReg.fp2_gen_cntl &
				  ~(R200_FP2_SOURCE_SEL_MASK |
				    RADEON_FP2_DVO_RATE_SEL_SDR);
            save->fp2_gen_cntl |= R200_FP2_SOURCE_SEL_CRTC2;
        } else {
            save->dac2_cntl = info->SavedReg.dac2_cntl | RADEON_DAC2_DAC2_CLK_SEL;
            save->disp_hw_debug = info->SavedReg.disp_hw_debug &
					~RADEON_CRT2_DISP1_SEL;
        }
    }
}

static void
RADEONInitOutputRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save,
			  DisplayModePtr mode, xf86OutputPtr output,
			  int crtc_num)
{
    Bool IsPrimary = crtc_num == 0 ? TRUE : FALSE;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    if (radeon_output->MonType == MT_CRT) {
	if (radeon_output->DACType == DAC_PRIMARY) {
	    RADEONInitDACRegisters(output, save, mode, IsPrimary);
	} else {
	    RADEONInitDAC2Registers(output, save, mode, IsPrimary);
	}
    } else if (radeon_output->MonType == MT_LCD) {
	if (crtc_num == 0)
	    RADEONInitRMXRegisters(output, save, mode);
	RADEONInitLVDSRegisters(output, save, mode, IsPrimary);
    } else if (radeon_output->MonType == MT_DFP) {
	if (crtc_num == 0)
	    RADEONInitRMXRegisters(output, save, mode);
	if (radeon_output->TMDSType == TMDS_INT) {
	    RADEONInitFPRegisters(output, save, mode, IsPrimary);
	} else {
	    RADEONInitFP2Registers(output, save, mode, IsPrimary);
	}
    }
}

static void
radeon_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		  DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    xf86CrtcPtr	crtc = output->crtc;
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;

    RADEONInitOutputRegisters(pScrn, &info->ModeReg, adjusted_mode, output, radeon_crtc->crtc_id);

    switch(radeon_output->MonType) {
    case MT_LCD:
	ErrorF("restore LVDS\n");
	if (radeon_crtc->crtc_id == 0)
	    RADEONRestoreRMXRegisters(pScrn, &info->ModeReg);
	RADEONRestoreLVDSRegisters(pScrn, &info->ModeReg);
    case MT_DFP:
	if (radeon_crtc->crtc_id == 0)
	    RADEONRestoreRMXRegisters(pScrn, &info->ModeReg);
	if (radeon_output->TMDSType == TMDS_INT) {
	    ErrorF("restore FP\n");
	    RADEONRestoreFPRegisters(pScrn, &info->ModeReg);
	} else {
	    ErrorF("restore FP2\n");
	    RADEONRestoreFP2Registers(pScrn, &info->ModeReg);
	}
	break;
    default:
	ErrorF("restore dac\n");
	RADEONRestoreDACRegisters(pScrn, &info->ModeReg);
    }

}

static void
radeon_mode_commit(xf86OutputPtr output)
{
    RADEONEnableDisplay(output, TRUE);
}

static xf86OutputStatus
radeon_detect(xf86OutputPtr output)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    
    radeon_output->MonType = MT_UNKNOWN;
    RADEONConnectorFindMonitor(pScrn, output);

    /* force montype based on output property */
    if (radeon_output->type == OUTPUT_DVI) {
	if ((info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_DVI_I_ATOM) ||
	    (!info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_DVI_I)) {
	    if (radeon_output->MonType > MT_NONE) {
		if (radeon_output->DVIType == DVI_ANALOG)
		    radeon_output->MonType = MT_CRT;
		else if (radeon_output->DVIType == DVI_DIGITAL)
		    radeon_output->MonType = MT_DFP;
	    }
	}
    }


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

static void
radeon_set_backlight_level(xf86OutputPtr output, int level)
{
#if 0
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char * RADEONMMIO = info->MMIO;
    CARD32 lvds_gen_cntl;

    lvds_gen_cntl = INREG(RADEON_LVDS_GEN_CNTL);
    lvds_gen_cntl |= RADEON_LVDS_BL_MOD_EN;
    lvds_gen_cntl &= ~RADEON_LVDS_BL_MOD_LEVEL_MASK;
    lvds_gen_cntl |= (level << RADEON_LVDS_BL_MOD_LEVEL_SHIFT) & RADEON_LVDS_BL_MOD_LEVEL_MASK;
    //usleep (radeon_output->PanelPwrDly * 1000);
    OUTREG(RADEON_LVDS_GEN_CNTL, lvds_gen_cntl);
    lvds_gen_cntl &= ~RADEON_LVDS_BL_MOD_EN;
    //usleep (radeon_output->PanelPwrDly * 1000);
    OUTREG(RADEON_LVDS_GEN_CNTL, lvds_gen_cntl);
#endif
}

static Atom backlight_atom;
static Atom rmx_atom;
static Atom monitor_type_atom;
#define RADEON_MAX_BACKLIGHT_LEVEL 255

static void
radeon_create_resources(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    INT32 range[2];
    int data, err;

    /* backlight control */
    if (radeon_output->type == OUTPUT_LVDS) {
	backlight_atom = MAKE_ATOM("BACKLIGHT");

	range[0] = 0;
	range[1] = RADEON_MAX_BACKLIGHT_LEVEL;
	err = RRConfigureOutputProperty(output->randr_output, backlight_atom,
					FALSE, TRUE, FALSE, 2, range);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRConfigureOutputProperty error, %d\n", err);
	}
	/* Set the current value of the backlight property */
	//data = (info->SavedReg.lvds_gen_cntl & RADEON_LVDS_BL_MOD_LEVEL_MASK) >> RADEON_LVDS_BL_MOD_LEVEL_SHIFT;
	data = RADEON_MAX_BACKLIGHT_LEVEL;
	err = RRChangeOutputProperty(output->randr_output, backlight_atom,
				     XA_INTEGER, 32, PropModeReplace, 1, &data,
				     FALSE, TRUE);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRChangeOutputProperty error, %d\n", err);
	}
    }

    /* RMX control - fullscreen, centered, keep ratio */
    if (radeon_output->type == OUTPUT_LVDS ||
	radeon_output->type == OUTPUT_DVI) {
	rmx_atom = MAKE_ATOM("PANELSCALER");

	range[0] = 0;
	range[1] = 2;
	err = RRConfigureOutputProperty(output->randr_output, rmx_atom,
					FALSE, TRUE, FALSE, 2, range);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRConfigureOutputProperty error, %d\n", err);
	}
	/* Set the current value of the property */
	data = 0;
	err = RRChangeOutputProperty(output->randr_output, rmx_atom,
				     XA_INTEGER, 32, PropModeReplace, 1, &data,
				     FALSE, TRUE);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRChangeOutputProperty error, %d\n", err);
	}
    }

    /* force auto/analog/digital for DVI-I ports */
    if (radeon_output->type == OUTPUT_DVI) {
	if ((info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_DVI_I_ATOM) ||
	    (!info->IsAtomBios && radeon_output->ConnectorType == CONNECTOR_DVI_I)) {
	    monitor_type_atom = MAKE_ATOM("MONITORTYPE");

	    range[0] = DVI_AUTO;
	    range[1] = DVI_ANALOG;
	    err = RRConfigureOutputProperty(output->randr_output, monitor_type_atom,
					    FALSE, TRUE, FALSE, 2, range);
	    if (err != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "RRConfigureOutputProperty error, %d\n", err);
	    }
	    /* Set the current value of the backlight property */
	    radeon_output->DVIType = DVI_AUTO;
	    data = DVI_AUTO;
	    err = RRChangeOutputProperty(output->randr_output, monitor_type_atom,
					 XA_INTEGER, 32, PropModeReplace, 1, &data,
					 FALSE, TRUE);
	    if (err != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "RRChangeOutputProperty error, %d\n", err);
	    }
	}
    }

}

static Bool
radeon_set_property(xf86OutputPtr output, Atom property,
		       RRPropertyValuePtr value)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    INT32 val;


    if (property == backlight_atom) {
	if (value->type != XA_INTEGER ||
	    value->format != 32 ||
	    value->size != 1) {
	    return FALSE;
	}

	val = *(INT32 *)value->data;
	if (val < 0 || val > RADEON_MAX_BACKLIGHT_LEVEL)
	    return FALSE;

#if defined(__powerpc__)
	val = RADEON_MAX_BACKLIGHT_LEVEL - val;
#endif

	radeon_set_backlight_level(output, val);

    } else if (property == rmx_atom) {
	return TRUE;
    } else if (property == monitor_type_atom) {
	if (value->type != XA_INTEGER ||
	    value->format != 32 ||
	    value->size != 1) {
	    return FALSE;
	}

	val = *(INT32 *)value->data;
	if (val < DVI_AUTO || val > DVI_ANALOG)
	    return FALSE;

	radeon_output->DVIType = val;
    }

    return TRUE;
}

static const xf86OutputFuncsRec radeon_output_funcs = {
    .create_resources = radeon_create_resources,
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
    .set_property = radeon_set_property,
    .destroy = radeon_destroy
};

void RADEONSetOutputType(ScrnInfoPtr pScrn, RADEONOutputPrivatePtr radeon_output)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    RADEONOutputType output;

    if (info->IsAtomBios) {
	switch(radeon_output->ConnectorType) {
	case CONNECTOR_VGA_ATOM:
	    output = OUTPUT_VGA; break;
	case CONNECTOR_DVI_I_ATOM:
	case CONNECTOR_DVI_D_ATOM:
	case CONNECTOR_DVI_A_ATOM:
	    output = OUTPUT_DVI; break;
	case CONNECTOR_STV_ATOM:
	    output = OUTPUT_STV; break;
	case CONNECTOR_CTV_ATOM:
	    output = OUTPUT_CTV; break;
	case CONNECTOR_LVDS_ATOM:
	case CONNECTOR_DIGITAL_ATOM:
	    output = OUTPUT_LVDS; break;
	case CONNECTOR_NONE_ATOM:
	case CONNECTOR_UNSUPPORTED_ATOM:
	default:
	    output = OUTPUT_NONE; break;
	}
    }
    else {
	switch(radeon_output->ConnectorType) {
	case CONNECTOR_PROPRIETARY:
	    output = OUTPUT_LVDS; break;
	case CONNECTOR_CRT:
	    output = OUTPUT_VGA; break;
	case CONNECTOR_DVI_I:
	case CONNECTOR_DVI_D:
	    output = OUTPUT_DVI; break;
	case CONNECTOR_CTV:
	    output = OUTPUT_STV; break;
	case CONNECTOR_STV:
	    output = OUTPUT_CTV; break;
	case CONNECTOR_NONE:
	case CONNECTOR_UNSUPPORTED:
	default:
	    output = OUTPUT_NONE; break;
	}
    }
    radeon_output->type = output;
}

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

static Bool
RADEONI2CInit(ScrnInfoPtr pScrn, I2CBusPtr *bus_ptr, int i2c_reg, char *name)
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

static void
RADEONGetPanelInfoFromReg (xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 fp_vert_stretch = INREG(RADEON_FP_VERT_STRETCH);
    CARD32 fp_horz_stretch = INREG(RADEON_FP_HORZ_STRETCH);

    radeon_output->PanelPwrDly = 200;
    if (fp_vert_stretch & RADEON_VERT_STRETCH_ENABLE) {
	radeon_output->PanelYRes = ((fp_vert_stretch & RADEON_VERT_PANEL_SIZE) >>
				    RADEON_VERT_PANEL_SHIFT) + 1;
    } else {
	radeon_output->PanelYRes = (INREG(RADEON_CRTC_V_TOTAL_DISP)>>16) + 1;
    }
    if (fp_horz_stretch & RADEON_HORZ_STRETCH_ENABLE) {
	radeon_output->PanelXRes = (((fp_horz_stretch & RADEON_HORZ_PANEL_SIZE) >>
				     RADEON_HORZ_PANEL_SHIFT) + 1) * 8;
    } else {
	radeon_output->PanelXRes = ((INREG(RADEON_CRTC_H_TOTAL_DISP)>>16) + 1) * 8;
    }
    
    if ((radeon_output->PanelXRes < 640) || (radeon_output->PanelYRes < 480)) {
	radeon_output->PanelXRes = 640;
	radeon_output->PanelYRes = 480;
    }

    // move this to crtc function
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
	       radeon_output->PanelXRes, radeon_output->PanelYRes);
}

/* BIOS may not have right panel size, we search through all supported
 * DDC modes looking for the maximum panel size.
 */
static void
RADEONUpdatePanelSize(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int             j;
    /* XXX: fixme */
    //xf86MonPtr      ddc  = pScrn->monitor->DDC;
    xf86MonPtr ddc = output->MonInfo;
    DisplayModePtr  p;

    // crtc should handle?
    if ((info->UseBiosDividers && radeon_output->DotClock != 0) || (ddc == NULL))
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
           if (radeon_output->DotClock == 0 &&
               radeon_output->PanelXRes == d_timings->h_active &&
               radeon_output->PanelYRes == d_timings->v_active)
               match = 1;

           /* If we don't have a BIOS provided panel data with fixed dividers,
            * check for a larger panel size
            */
	    if (radeon_output->PanelXRes < d_timings->h_active &&
               radeon_output->PanelYRes < d_timings->v_active &&
               !info->UseBiosDividers)
               match = 1;

             if (match) {
		radeon_output->PanelXRes  = d_timings->h_active;
		radeon_output->PanelYRes  = d_timings->v_active;
		radeon_output->DotClock   = d_timings->clock / 1000;
		radeon_output->HOverPlus  = d_timings->h_sync_off;
		radeon_output->HSyncWidth = d_timings->h_sync_width;
		radeon_output->HBlank     = d_timings->h_blanking;
		radeon_output->VOverPlus  = d_timings->v_sync_off;
		radeon_output->VSyncWidth = d_timings->v_sync_width;
		radeon_output->VBlank     = d_timings->v_blanking;
                radeon_output->Flags      = (d_timings->interlaced ? V_INTERLACE : 0);
                switch (d_timings->misc) {
                case 0: radeon_output->Flags |= V_NHSYNC | V_NVSYNC; break;
                case 1: radeon_output->Flags |= V_PHSYNC | V_NVSYNC; break;
                case 2: radeon_output->Flags |= V_NHSYNC | V_PVSYNC; break;
                case 3: radeon_output->Flags |= V_PHSYNC | V_PVSYNC; break;
                }
                xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel infos found from DDC detailed: %dx%d\n",
                           radeon_output->PanelXRes, radeon_output->PanelYRes);
	    }
	}
    }

    if (info->UseBiosDividers && radeon_output->DotClock != 0)
       return;

    /* Search thru standard VESA modes from EDID */
    for (j = 0; j < 8; j++) {
	if ((radeon_output->PanelXRes < ddc->timings2[j].hsize) &&
	    (radeon_output->PanelYRes < ddc->timings2[j].vsize)) {
	    for (p = pScrn->monitor->Modes; p; p = p->next) {
		if ((ddc->timings2[j].hsize == p->HDisplay) &&
		    (ddc->timings2[j].vsize == p->VDisplay)) {
		    float  refresh =
			(float)p->Clock * 1000.0 / p->HTotal / p->VTotal;

		    if (abs((float)ddc->timings2[j].refresh - refresh) < 1.0) {
			/* Is this good enough? */
			radeon_output->PanelXRes  = ddc->timings2[j].hsize;
			radeon_output->PanelYRes  = ddc->timings2[j].vsize;
			radeon_output->HBlank     = p->HTotal - p->HDisplay;
			radeon_output->HOverPlus  = p->HSyncStart - p->HDisplay;
			radeon_output->HSyncWidth = p->HSyncEnd - p->HSyncStart;
			radeon_output->VBlank     = p->VTotal - p->VDisplay;
			radeon_output->VOverPlus  = p->VSyncStart - p->VDisplay;
			radeon_output->VSyncWidth = p->VSyncEnd - p->VSyncStart;
			radeon_output->DotClock   = p->Clock;
                        radeon_output->Flags      = p->Flags;
                        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Panel infos found from DDC VESA/EDID: %dx%d\n",
                                   radeon_output->PanelXRes, radeon_output->PanelYRes);
		    }
		}
	    }
	}
    }
}

static Bool
RADEONGetLVDSInfo (xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    char* s;

    if (!RADEONGetLVDSInfoFromBIOS(output))
	RADEONGetPanelInfoFromReg(output);

    if ((s = xf86GetOptValString(info->Options, OPTION_PANEL_SIZE))) {
	radeon_output->PanelPwrDly = 200;
	if (sscanf (s, "%dx%d", &radeon_output->PanelXRes, &radeon_output->PanelYRes) != 2) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Invalid PanelSize option: %s\n", s);
	    RADEONGetPanelInfoFromReg(output);
	}
    }

    /* The panel size we collected from BIOS may not be the
     * maximum size supported by the panel.  If not, we update
     * it now.  These will be used if no matching mode can be
     * found from EDID data.
     */
    RADEONUpdatePanelSize(output);

    if (radeon_output->DotClock == 0) {
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
	    if ((tmp_mode->HDisplay == radeon_output->PanelXRes) &&
		(tmp_mode->VDisplay == radeon_output->PanelYRes)) {
		    
		float  refresh =
		    (float)tmp_mode->Clock * 1000.0 / tmp_mode->HTotal / tmp_mode->VTotal;
		if ((abs(60.0 - refresh) < 1.0) ||
		    (tmp_mode->type == 0)) {
		    radeon_output->HBlank     = tmp_mode->HTotal - tmp_mode->HDisplay;
		    radeon_output->HOverPlus  = tmp_mode->HSyncStart - tmp_mode->HDisplay;
		    radeon_output->HSyncWidth = tmp_mode->HSyncEnd - tmp_mode->HSyncStart;
		    radeon_output->VBlank     = tmp_mode->VTotal - tmp_mode->VDisplay;
		    radeon_output->VOverPlus  = tmp_mode->VSyncStart - tmp_mode->VDisplay;
		    radeon_output->VSyncWidth = tmp_mode->VSyncEnd - tmp_mode->VSyncStart;
		    radeon_output->DotClock   = tmp_mode->Clock;
		    radeon_output->Flags = 0;
		    break;
		}
	    }

	    tmp_mode = tmp_mode->next;

	    if (tmp_mode == pScrn->monitor->Modes)
		break;
	}
	if ((radeon_output->DotClock == 0) && !output->MonInfo) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Panel size is not correctly detected.\n"
		       "Please try to use PanelSize option for correct settings.\n");
	    return FALSE;
	}
    }

    return TRUE;
}

static void
RADEONGetTMDSInfo(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int i;

    for (i=0; i<4; i++) {
        radeon_output->tmds_pll[i].value = 0;
        radeon_output->tmds_pll[i].freq = 0;
    }

    if (RADEONGetTMDSInfoFromBIOS(output)) return;

    for (i=0; i<4; i++) {
        radeon_output->tmds_pll[i].value = default_tmds_pll[info->ChipFamily][i].value;
        radeon_output->tmds_pll[i].freq = default_tmds_pll[info->ChipFamily][i].freq;
    }
}

void RADEONInitConnector(xf86OutputPtr output)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int DDCReg = 0;
    char* name = (char*) DDCTypeName[radeon_output->DDCType];

    switch(radeon_output->DDCType) {
    case DDC_MONID: DDCReg = RADEON_GPIO_MONID; break;
    case DDC_DVI  : DDCReg = RADEON_GPIO_DVI_DDC; break;
    case DDC_VGA  : DDCReg = RADEON_GPIO_VGA_DDC; break;
    case DDC_CRT2 : DDCReg = RADEON_GPIO_CRT2_DDC; break;
    case DDC_LCD  : DDCReg = RADEON_LCD_GPIO_MASK; break;
    default: break;
    }
    
    if (DDCReg) {
	radeon_output->DDCReg = DDCReg;
	RADEONI2CInit(pScrn, &radeon_output->pI2CBus, DDCReg, name);
    }

    if (radeon_output->type == OUTPUT_LVDS) {
	RADEONGetLVDSInfo(output);
    }

    if (radeon_output->type == OUTPUT_DVI) {
	RADEONGetTMDSInfo(output);

	// FIXME -- this should be done in detect or getmodes
	/*if (i == 0)
	  RADEONGetHardCodedEDIDFromBIOS(output);*/

	/*RADEONUpdatePanelSize(output);*/
    }

    if (radeon_output->DACType == DAC_TVDAC) {
	RADEONGetTVDacAdjInfo(output);
    }

}

/*
 * initialise the static data sos we don't have to re-do at randr change */
Bool RADEONSetupConnectors(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    xf86OutputPtr output;
    char *optstr;
    int i = 0;


    /* We first get the information about all connectors from BIOS.
     * This is how the card is phyiscally wired up.
     * The information should be correct even on a OEM card.
     * If not, we may have problem -- need to use MonitorLayout option.
     */
    for (i = 0; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	info->BiosConnector[i].DDCType = DDC_NONE_DETECTED;
	info->BiosConnector[i].DACType = DAC_UNKNOWN;
	info->BiosConnector[i].TMDSType = TMDS_UNKNOWN;
	info->BiosConnector[i].ConnectorType = CONNECTOR_NONE;
    }

    if (!RADEONGetConnectorInfoFromBIOS(pScrn) ||
        ((info->BiosConnector[0].DDCType == 0) &&
        (info->BiosConnector[1].DDCType == 0))) {
	if (info->IsMobility) {
	    /* Below is the most common setting, but may not be true */
#if defined(__powerpc__)
	    info->BiosConnector[0].DDCType = DDC_DVI;
#else
	    info->BiosConnector[0].DDCType = DDC_LCD;
#endif
	    info->BiosConnector[0].DACType = DAC_UNKNOWN;
	    info->BiosConnector[0].TMDSType = TMDS_UNKNOWN;
	    info->BiosConnector[0].ConnectorType = CONNECTOR_PROPRIETARY;

	    info->BiosConnector[1].DDCType = DDC_VGA;
	    info->BiosConnector[1].DACType = DAC_PRIMARY;
	    info->BiosConnector[1].TMDSType = TMDS_EXT;
	    info->BiosConnector[1].ConnectorType = CONNECTOR_CRT;
	} else {
	    /* Below is the most common setting, but may not be true */
	    info->BiosConnector[0].DDCType = DDC_DVI;
	    info->BiosConnector[0].DACType = DAC_TVDAC;
	    info->BiosConnector[0].TMDSType = TMDS_INT;
	    info->BiosConnector[0].ConnectorType = CONNECTOR_DVI_I;

	    info->BiosConnector[1].DDCType = DDC_VGA;
	    info->BiosConnector[1].DACType = DAC_PRIMARY;
	    info->BiosConnector[1].TMDSType = TMDS_EXT;
	    info->BiosConnector[1].ConnectorType = CONNECTOR_CRT;
	}

       /* Some cards have the DDC lines swapped and we have no way to
        * detect it yet (Mac cards)
        */
       if (xf86ReturnOptValBool(info->Options, OPTION_REVERSE_DDC, FALSE)) {
           info->BiosConnector[0].DDCType = DDC_VGA;
           info->BiosConnector[1].DDCType = DDC_DVI;
        }
    }

    if (info->HasSingleDAC) {
        /* For RS300/RS350/RS400 chips, there is no primary DAC. Force VGA port to use TVDAC*/
        if (info->BiosConnector[0].ConnectorType == CONNECTOR_CRT) {
            info->BiosConnector[0].DACType = DAC_TVDAC;
            info->BiosConnector[1].DACType = DAC_NONE;
        } else {
            info->BiosConnector[1].DACType = DAC_TVDAC;
            info->BiosConnector[0].DACType = DAC_NONE;
        }
    } else if (!pRADEONEnt->HasCRTC2) {
        info->BiosConnector[0].DACType = DAC_PRIMARY;
    }

    /* parse connector table option */
    optstr = (char *)xf86GetOptValString(info->Options, OPTION_CONNECTORTABLE);

    if (optstr) {
	if (sscanf(optstr, "%u,%d,%d,%u,%u,%d,%d,%u",
		   &info->BiosConnector[0].DDCType,
		   &info->BiosConnector[0].DACType,
		   &info->BiosConnector[0].TMDSType,
		   &info->BiosConnector[0].ConnectorType,
		   &info->BiosConnector[1].DDCType,
		   &info->BiosConnector[1].DACType,
		   &info->BiosConnector[1].TMDSType,
		   &info->BiosConnector[1].ConnectorType) != 8) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid ConnectorTable option: %s\n", optstr);
	    return FALSE;
	}
    }

    for (i = 0 ; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	if (info->BiosConnector[i].ConnectorType != CONNECTOR_NONE) {
	    RADEONOutputPrivatePtr radeon_output = xnfcalloc(sizeof(RADEONOutputPrivateRec), 1);
	    if (!radeon_output) {
		return FALSE;
	    }
	    radeon_output->MonType = MT_UNKNOWN;
	    radeon_output->ConnectorType = info->BiosConnector[i].ConnectorType;
	    radeon_output->DDCType = info->BiosConnector[i].DDCType;
	    if (info->IsAtomBios) {
		if (radeon_output->ConnectorType == CONNECTOR_DVI_D_ATOM)
		    radeon_output->DACType = DAC_NONE;
		else
		    radeon_output->DACType = info->BiosConnector[i].DACType;

		if (radeon_output->ConnectorType == CONNECTOR_VGA_ATOM)
		    radeon_output->TMDSType = TMDS_NONE;
		else
		    radeon_output->TMDSType = info->BiosConnector[i].TMDSType;
	    } else {
		if (radeon_output->ConnectorType == CONNECTOR_DVI_D)
		    radeon_output->DACType = DAC_NONE;
		else
		    radeon_output->DACType = info->BiosConnector[i].DACType;

		if (radeon_output->ConnectorType == CONNECTOR_CRT)
		    radeon_output->TMDSType = TMDS_NONE;
		else
		    radeon_output->TMDSType = info->BiosConnector[i].TMDSType;
	    }
	    RADEONSetOutputType(pScrn, radeon_output);
	    if (info->IsAtomBios) {
		if (((info->BiosConnector[0].ConnectorType == CONNECTOR_DVI_D_ATOM) ||
		     (info->BiosConnector[0].ConnectorType == CONNECTOR_DVI_I_ATOM) ||
		     (info->BiosConnector[0].ConnectorType == CONNECTOR_DVI_A_ATOM)) &&
		    ((info->BiosConnector[1].ConnectorType == CONNECTOR_DVI_D_ATOM) ||
		     (info->BiosConnector[1].ConnectorType == CONNECTOR_DVI_I_ATOM) ||
		     (info->BiosConnector[1].ConnectorType == CONNECTOR_DVI_A_ATOM))) {
		    if (i > 0)
			output = xf86OutputCreate(pScrn, &radeon_output_funcs, "DVI-1");
		    else
			output = xf86OutputCreate(pScrn, &radeon_output_funcs, "DVI-0");
		} else if ((info->BiosConnector[0].ConnectorType == CONNECTOR_VGA_ATOM) &&
			   (info->BiosConnector[1].ConnectorType == CONNECTOR_VGA_ATOM)) {
		    if (i > 0)
			output = xf86OutputCreate(pScrn, &radeon_output_funcs, "VGA-1");
		    else
			output = xf86OutputCreate(pScrn, &radeon_output_funcs, "VGA-0");
		} else
		    output = xf86OutputCreate(pScrn, &radeon_output_funcs, OutputType[radeon_output->type]);
	    } else {
		if (((info->BiosConnector[0].ConnectorType == CONNECTOR_DVI_D) ||
		     (info->BiosConnector[0].ConnectorType == CONNECTOR_DVI_I)) &&
		    ((info->BiosConnector[1].ConnectorType == CONNECTOR_DVI_D) ||
		     (info->BiosConnector[1].ConnectorType == CONNECTOR_DVI_I))) {
		    if (i > 0)
			output = xf86OutputCreate(pScrn, &radeon_output_funcs, "DVI-1");
		    else
			output = xf86OutputCreate(pScrn, &radeon_output_funcs, "DVI-0");
		} else if ((info->BiosConnector[0].ConnectorType == CONNECTOR_CRT) &&
			   (info->BiosConnector[1].ConnectorType == CONNECTOR_CRT)) {
		    if (i > 0)
			output = xf86OutputCreate(pScrn, &radeon_output_funcs, "VGA-1");
		    else
			output = xf86OutputCreate(pScrn, &radeon_output_funcs, "VGA-0");
		} else
		    output = xf86OutputCreate(pScrn, &radeon_output_funcs, OutputType[radeon_output->type]);
	    }

	    if (!output) {
		return FALSE;
	    }
	    output->driver_private = radeon_output;
	    output->possible_crtcs = 1;
	    /* crtc2 can drive LVDS, it just doesn't have RMX */
	    if (radeon_output->type != OUTPUT_LVDS)
		output->possible_crtcs |= 2;

	    /* we can clone the DACs, and probably TV-out, 
	       but I'm not sure it's worth the trouble */
	    output->possible_clones = 0;

	    RADEONInitConnector(output);
	}
    }

    /* if it's a mobility make sure we have a LVDS port */
    if (info->IsMobility) {
	if (info->IsAtomBios) {
	    if (info->BiosConnector[0].ConnectorType != CONNECTOR_LVDS_ATOM &&
		info->BiosConnector[1].ConnectorType != CONNECTOR_LVDS_ATOM) {
		/* add LVDS port */
		RADEONOutputPrivatePtr radeon_output = xnfcalloc(sizeof(RADEONOutputPrivateRec), 1);
		if (!radeon_output) {
		    return FALSE;
		}
		radeon_output->MonType = MT_UNKNOWN;
		radeon_output->DDCType = DDC_LCD;
		radeon_output->DACType = DAC_NONE;
		radeon_output->TMDSType = TMDS_NONE;
		radeon_output->ConnectorType = CONNECTOR_LVDS_ATOM;
		RADEONSetOutputType(pScrn, radeon_output);
		output = xf86OutputCreate(pScrn, &radeon_output_funcs, OutputType[radeon_output->type]);
		if (!output) {
		    return FALSE;
		}
		output->driver_private = radeon_output;
		output->possible_crtcs = 1;
		output->possible_clones = 0;

		RADEONInitConnector(output);

	    }
	} else {
	    if (info->BiosConnector[0].ConnectorType != CONNECTOR_PROPRIETARY &&
		info->BiosConnector[1].ConnectorType != CONNECTOR_PROPRIETARY) {
		/* add LVDS port */
		RADEONOutputPrivatePtr radeon_output = xnfcalloc(sizeof(RADEONOutputPrivateRec), 1);
		if (!radeon_output) {
		    return FALSE;
		}
		radeon_output->MonType = MT_UNKNOWN;
		radeon_output->DDCType = DDC_LCD;
		radeon_output->DACType = DAC_NONE;
		radeon_output->TMDSType = TMDS_NONE;
		radeon_output->ConnectorType = CONNECTOR_PROPRIETARY;
		RADEONSetOutputType(pScrn, radeon_output);
		output = xf86OutputCreate(pScrn, &radeon_output_funcs, OutputType[radeon_output->type]);
		if (!output) {
		    return FALSE;
		}
		output->driver_private = radeon_output;
		output->possible_crtcs = 1;
		output->possible_clones = 0;

		RADEONInitConnector(output);
	    }
	}
    }
    return TRUE;
}

