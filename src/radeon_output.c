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

static RADEONMonitorType RADEONPortCheckNonDDC(ScrnInfoPtr pScrn, xf86OutputPtr output);

void RADEONPrintPortMap(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
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

/* Primary Head (DVI or Laptop Int. panel)*/
/* A ddc capable display connected on DVI port */
/* Secondary Head (mostly VGA, can be DVI on some OEM boards)*/
void RADEONConnectorFindMonitor(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    
    if (radeon_output->MonType == MT_UNKNOWN) {
      if ((radeon_output->MonType = RADEONDisplayDDCConnected(pScrn, output)));
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
    ScrnInfoPtr	pScrn = output->scrn;

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
    ScrnInfoPtr	pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    DisplayModePtr m;

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

    if (radeon_output->type != OUTPUT_LVDS)
	return TRUE;

    if (mode->HDisplay < radeon_output->PanelXRes ||
	mode->VDisplay < radeon_output->PanelYRes)
	adjusted_mode->Flags |= RADEON_USE_RMX;

    if (adjusted_mode->Flags & RADEON_USE_RMX) {
	adjusted_mode->CrtcHTotal     = mode->CrtcHDisplay + radeon_output->HBlank;
	adjusted_mode->CrtcHSyncStart = mode->CrtcHDisplay + radeon_output->HOverPlus;
	adjusted_mode->CrtcHSyncEnd   = mode->CrtcHSyncStart + radeon_output->HSyncWidth;
	adjusted_mode->CrtcVTotal     = mode->CrtcVDisplay + radeon_output->VBlank;
	adjusted_mode->CrtcVSyncStart = mode->CrtcVDisplay + radeon_output->VOverPlus;
	adjusted_mode->CrtcVSyncEnd   = mode->CrtcVSyncStart + radeon_output->VSyncWidth;
	adjusted_mode->Clock          = radeon_output->DotClock;
	adjusted_mode->Flags          = radeon_output->Flags | RADEON_USE_RMX;
	/* save these for Xv with RMX */
	info->PanelYRes = radeon_output->PanelYRes;
	info->PanelXRes = radeon_output->PanelXRes;
    }

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
    RADEONInfoPtr info = RADEONPTR(pScrn);
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int i;

    /* get the outputs connected to this CRTC */
    for (i = 0; i < xf86_config->num_crtc; i++) {
	xf86CrtcPtr	crtc = xf86_config->crtc[i];
	RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
	if (output->crtc == crtc) {
	    RADEONInitOutputRegisters(pScrn, &info->ModeReg, adjusted_mode, output, radeon_crtc->crtc_id);
	}
    }

    switch(radeon_output->MonType) {
    case MT_LCD:
    case MT_DFP:
	ErrorF("restore FP\n");
	RADEONRestoreFPRegisters(pScrn, &info->ModeReg);
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
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
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
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    unsigned char * RADEONMMIO = info->MMIO;
    CARD32 lvds_gen_cntl;

#if 0
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
	/* Set the current value of the backlight property */
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
    ScrnInfoPtr pScrn = output->scrn;
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
    int i = 0, second = 0, max_mt = 5;


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
    if (optstr = (char *)xf86GetOptValString(info->Options, OPTION_CONNECTORTABLE)) {
	if (sscanf(optstr, "%d,%d,%d,%d,%d,%d,%d,%d",
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
	    output = xf86OutputCreate(pScrn, &radeon_output_funcs, OutputType[radeon_output->type]);
	    if (!output) {
		return FALSE;
	    }
	    output->driver_private = radeon_output;
	    output->possible_crtcs = 1;
	    /* crtc2 can drive LVDS, it just doesn't have RMX */
	    if (radeon_output->type != OUTPUT_LVDS)
		output->possible_crtcs |= 2;

	    output->possible_clones = 0 /*1|2*/;

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
		output->possible_clones = 0 /*1|2*/;

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
		output->possible_clones = 0 /*1|2*/;

		RADEONInitConnector(output);
	    }
	}
    }
    return TRUE;
}

