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
#include "vgaHW.h"
#include "xf86Modes.h"

/* Driver data structures */
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_version.h"
#include "radeon_tv.h"
#include "radeon_atombios.h"

const char *encoder_name[34] = {
    "NONE",
    "INTERNAL_LVDS",
    "INTERNAL_TMDS1",
    "INTERNAL_TMDS2",
    "INTERNAL_DAC1",
    "INTERNAL_DAC2",
    "INTERNAL_SDVOA",
    "INTERNAL_SDVOB",
    "SI170B",
    "CH7303",
    "CH7301",
    "INTERNAL_DVO1",
    "EXTERNAL_SDVOA",
    "EXTERNAL_SDVOB",
    "TITFP513",
    "INTERNAL_LVTM1",
    "VT1623",
    "HDMI_SI1930",
    "HDMI_INTERNAL",
    "INTERNAL_KLDSCP_TMDS1",
    "INTERNAL_KLDSCP_DVO1",
    "INTERNAL_KLDSCP_DAC1",
    "INTERNAL_KLDSCP_DAC2",
    "SI178",
    "MVPU_FPGA",
    "INTERNAL_DDI",
    "VT1625",
    "HDMI_SI1932",
    "DP_AN9801",
    "DP_DP501",
    "INTERNAL_UNIPHY",
    "INTERNAL_KLDSCP_LVTMA",
    "INTERNAL_UNIPHY1",
    "INTERNAL_UNIPHY2",
};

const char *ConnectorTypeName[17] = {
  "None",
  "VGA",
  "DVI-I",
  "DVI-D",
  "DVI-A",
  "S-video",
  "Composite",
  "LVDS",
  "Digital",
  "SCART",
  "HDMI-A",
  "HDMI-B",
  "Unsupported",
  "Unsupported",
  "DIN",
  "DisplayPort",
  "Unsupported"
};

static void RADEONUpdatePanelSize(xf86OutputPtr output);
extern void atombios_output_mode_set(xf86OutputPtr output,
				     DisplayModePtr mode,
				     DisplayModePtr adjusted_mode);
extern void atombios_output_dpms(xf86OutputPtr output, int mode);
extern RADEONMonitorType atombios_dac_detect(xf86OutputPtr output);
extern int atombios_external_tmds_setup(xf86OutputPtr output, DisplayModePtr mode);
extern AtomBiosResult
atombios_lock_crtc(atomBiosHandlePtr atomBIOS, int crtc, int lock);
extern void
RADEONGetExtTMDSInfo(xf86OutputPtr output);
extern void
RADEONGetTMDSInfoFromTable(xf86OutputPtr output);
extern void
RADEONGetTMDSInfo(xf86OutputPtr output);
extern void
RADEONGetTVDacAdjInfo(xf86OutputPtr output);
static void
radeon_bios_output_dpms(xf86OutputPtr output, int mode);
static void
radeon_bios_output_crtc(xf86OutputPtr output);
static void
radeon_bios_output_lock(xf86OutputPtr output, Bool lock);

void RADEONPrintPortMap(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONOutputPrivatePtr radeon_output;
    xf86OutputPtr output;
    int o;

    for (o = 0; o < xf86_config->num_output; o++) {
	output = xf86_config->output[o];
	radeon_output = output->driver_private;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Port%d:\n", o);
	ErrorF("  XRANDR name: %s\n", output->name);
	ErrorF("  Connector: %s\n", ConnectorTypeName[radeon_output->ConnectorType]);
	if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT)
	    ErrorF("  CRT1: %s\n", encoder_name[info->encoders[ATOM_DEVICE_CRT1_INDEX]->encoder_id]);
	if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT)
	    ErrorF("  CRT2: %s\n", encoder_name[info->encoders[ATOM_DEVICE_CRT2_INDEX]->encoder_id]);
	if (radeon_output->devices & ATOM_DEVICE_LCD1_SUPPORT)
	    ErrorF("  LCD1: %s\n", encoder_name[info->encoders[ATOM_DEVICE_LCD1_INDEX]->encoder_id]);
	if (radeon_output->devices & ATOM_DEVICE_DFP1_SUPPORT)
	    ErrorF("  DFP1: %s\n", encoder_name[info->encoders[ATOM_DEVICE_DFP1_INDEX]->encoder_id]);
	if (radeon_output->devices & ATOM_DEVICE_DFP2_SUPPORT)
	    ErrorF("  DFP2: %s\n", encoder_name[info->encoders[ATOM_DEVICE_DFP2_INDEX]->encoder_id]);
	if (radeon_output->devices & ATOM_DEVICE_DFP3_SUPPORT)
	    ErrorF("  DFP3: %s\n", encoder_name[info->encoders[ATOM_DEVICE_DFP3_INDEX]->encoder_id]);
	if (radeon_output->devices & ATOM_DEVICE_DFP4_SUPPORT)
	    ErrorF("  DFP4: %s\n", encoder_name[info->encoders[ATOM_DEVICE_DFP4_INDEX]->encoder_id]);
	if (radeon_output->devices & ATOM_DEVICE_DFP5_SUPPORT)
	    ErrorF("  DFP5: %s\n", encoder_name[info->encoders[ATOM_DEVICE_DFP5_INDEX]->encoder_id]);
	if (radeon_output->devices & ATOM_DEVICE_TV1_SUPPORT)
	    ErrorF("  TV1: %s\n", encoder_name[info->encoders[ATOM_DEVICE_TV1_INDEX]->encoder_id]);
	if (radeon_output->devices & ATOM_DEVICE_CV_SUPPORT)
	    ErrorF("  CV: %s\n", encoder_name[info->encoders[ATOM_DEVICE_CRT1_INDEX]->encoder_id]);
	ErrorF("  DDC reg: 0x%x\n",(unsigned int)radeon_output->ddc_i2c.mask_clk_reg);
    }

}

static void
radeon_set_active_device(xf86OutputPtr output)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    radeon_output->active_device = 0;

    switch (radeon_output->MonType) {
    case MT_DFP:
	if (radeon_output->devices & ATOM_DEVICE_DFP1_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_DFP1_SUPPORT;
	else if (radeon_output->devices & ATOM_DEVICE_DFP2_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_DFP2_SUPPORT;
	else if (radeon_output->devices & ATOM_DEVICE_DFP3_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_DFP3_SUPPORT;
	else if (radeon_output->devices & ATOM_DEVICE_DFP4_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_DFP4_SUPPORT;
	else if (radeon_output->devices & ATOM_DEVICE_DFP5_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_DFP5_SUPPORT;
	break;
    case MT_CRT:
	if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_CRT1_SUPPORT;
	else if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_CRT2_SUPPORT;
	break;
    case MT_LCD:
	if (radeon_output->devices & ATOM_DEVICE_LCD1_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_LCD1_SUPPORT;
	else if (radeon_output->devices & ATOM_DEVICE_LCD2_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_LCD2_SUPPORT;
	break;
    case MT_STV:
    case MT_CTV:
	if (radeon_output->devices & ATOM_DEVICE_TV1_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_TV1_SUPPORT;
	else if (radeon_output->devices & ATOM_DEVICE_TV2_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_TV2_SUPPORT;
	break;
    case MT_CV:
	if (radeon_output->devices & ATOM_DEVICE_CV_SUPPORT)
	    radeon_output->active_device = ATOM_DEVICE_CV_SUPPORT;
	break;
    default:
	radeon_output->active_device = 0;
    }
}

static RADEONMonitorType
radeon_ddc_connected(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn        = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONMonitorType MonType = MT_NONE;
    xf86MonPtr MonInfo = NULL;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    if (radeon_output->pI2CBus) {
	/* RV410 RADEON_GPIO_VGA_DDC seems to only work via hw i2c
	 * We may want to extend this to other cases if the need arises...
	 */
	if ((info->ChipFamily == CHIP_FAMILY_RV410) &&
	    (radeon_output->ddc_i2c.mask_clk_reg == RADEON_GPIO_VGA_DDC) &&
	    info->IsAtomBios)
	    MonInfo = radeon_atom_get_edid(output);
	else if (info->get_hardcoded_edid_from_bios) {
	    MonInfo = RADEONGetHardCodedEDIDFromBIOS(output);
	    if (MonInfo == NULL) {
		RADEONI2CDoLock(output, TRUE);
		MonInfo = xf86OutputGetEDID(output, radeon_output->pI2CBus);
		RADEONI2CDoLock(output, FALSE);
	    }
	} else {
	    RADEONI2CDoLock(output, TRUE);
	    MonInfo = xf86OutputGetEDID(output, radeon_output->pI2CBus);
	    RADEONI2CDoLock(output, FALSE);
	}
    }
    if (MonInfo) {
	switch (radeon_output->ConnectorType) {
	case CONNECTOR_LVDS:
	    MonType = MT_LCD;
	    break;
	case CONNECTOR_DVI_D:
	case CONNECTOR_HDMI_TYPE_A:
	case CONNECTOR_HDMI_TYPE_B:
	    if (radeon_output->shared_ddc) {
		if (MonInfo->rawData[0x14] & 0x80) /* if it's digital and DVI/HDMI/etc. */
		    MonType = MT_DFP;
		else
		    MonType = MT_NONE;
	    } else
		MonType = MT_DFP;
	    break;
	case CONNECTOR_DISPLAY_PORT:
	    MonType = MT_DP;
	case CONNECTOR_DVI_I:
	    if (MonInfo->rawData[0x14] & 0x80) /* if it's digital and DVI */
		MonType = MT_DFP;
	    else
		MonType = MT_CRT;
	    break;
	case CONNECTOR_VGA:
	case CONNECTOR_DVI_A:
	default:
	    if (radeon_output->shared_ddc) {
		if (MonInfo->rawData[0x14] & 0x80) /* if it's digital and VGA */
		    MonType = MT_NONE;
		else
		    MonType = MT_CRT;
	    } else
		MonType = MT_CRT;
	    break;
	}

	if (MonType != MT_NONE)
	    if (!xf86ReturnOptValBool(info->Options, OPTION_IGNORE_EDID, FALSE))
		xf86OutputSetEDID(output, MonInfo);
    } else
	MonType = MT_NONE;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Output: %s, Detected Monitor Type: %d\n", output->name, MonType);

    return MonType;
}

#ifndef __powerpc__

static RADEONMonitorType
RADEONDetectLidStatus(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONMonitorType MonType = MT_NONE;
#ifdef __linux__
    char lidline[50];  /* 50 should be sufficient for our purposes */
    FILE *f = fopen ("/proc/acpi/button/lid/LID/state", "r");

    if (f != NULL) {
	while (fgets(lidline, sizeof lidline, f)) {
	    if (!strncmp(lidline, "state:", strlen ("state:"))) {
		if (strstr(lidline, "open")) {
		    fclose(f);
		    ErrorF("proc lid open\n");
		    return MT_LCD;
		}
		else if (strstr(lidline, "closed")) {
		    fclose(f);
		    ErrorF("proc lid closed\n");
		    return MT_NONE;
		}
	    }
	}
	fclose(f);
    }
#endif

    if (!info->IsAtomBios) {
	unsigned char *RADEONMMIO = info->MMIO;

	/* see if the lid is closed -- only works at boot */
	if (INREG(RADEON_BIOS_6_SCRATCH) & 0x10)
	    MonType = MT_NONE;
	else
	    MonType = MT_LCD;
    } else
	MonType = MT_LCD;

    return MonType;
}

#endif /* __powerpc__ */

static void
radeon_dpms(xf86OutputPtr output, int mode)
{
    RADEONInfoPtr info = RADEONPTR(output->scrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    if ((mode == DPMSModeOn) && radeon_output->enabled)
	return;

    if (IS_AVIVO_VARIANT) {
	atombios_output_dpms(output, mode);
    } else {
	legacy_output_dpms(output, mode);
    }
    radeon_bios_output_dpms(output, mode);

    if (mode == DPMSModeOn)
	radeon_output->enabled = TRUE;
    else
	radeon_output->enabled = FALSE;

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
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);

    /*
     * RN50 has effective maximum mode bandwidth of about 300MiB/s.
     * XXX should really do this for all chips by properly computing
     * memory bandwidth and an overhead factor.
     */
    if (info->ChipFamily == CHIP_FAMILY_RV100 && !pRADEONEnt->HasCRTC2) {
	if (xf86ModeBandwidth(pMode, pScrn->bitsPerPixel) > 300)
	    return MODE_BANDWIDTH;
    }

    if (radeon_output->active_device & (ATOM_DEVICE_TV_SUPPORT)) {
	/* FIXME: Update when more modes are added */
	if (!info->IsAtomBios) {
	    if (pMode->HDisplay == 800 && pMode->VDisplay == 600)
		return MODE_OK;
	    else
		return MODE_CLOCK_RANGE;
	}
    }

    if (radeon_output->active_device & (ATOM_DEVICE_LCD_SUPPORT)) {
	if (radeon_output->rmx_type == RMX_OFF) {
	    if (pMode->HDisplay != radeon_output->PanelXRes ||
		pMode->VDisplay != radeon_output->PanelYRes)
		return MODE_PANEL;
	}
	if (pMode->HDisplay > radeon_output->PanelXRes ||
	    pMode->VDisplay > radeon_output->PanelYRes)
	    return MODE_PANEL;
    }

    return MODE_OK;
}

static Bool
radeon_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		    DisplayModePtr adjusted_mode)
{
    RADEONInfoPtr info = RADEONPTR(output->scrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    radeon_output->Flags &= ~RADEON_USE_RMX;

    /* 
     *  Refresh the Crtc values without INTERLACE_HALVE_V 
     *  Should we use output->scrn->adjustFlags like xf86RandRModeConvert() does? 
     */
    xf86SetModeCrtc(adjusted_mode, 0);

    /* decide if we are using RMX */
    if ((radeon_output->active_device & (ATOM_DEVICE_LCD_SUPPORT | ATOM_DEVICE_DFP_SUPPORT))
	&& radeon_output->rmx_type != RMX_OFF) {
	xf86CrtcPtr crtc = output->crtc;
	RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;

	if (IS_AVIVO_VARIANT || radeon_crtc->crtc_id == 0) {
	    if (mode->HDisplay < radeon_output->PanelXRes ||
		mode->VDisplay < radeon_output->PanelYRes) {
		radeon_output->Flags |= RADEON_USE_RMX;
		if (IS_AVIVO_VARIANT) {
		    /* set to the panel's native mode */
		    adjusted_mode->HDisplay = radeon_output->PanelXRes;
		    adjusted_mode->VDisplay = radeon_output->PanelYRes;
		    adjusted_mode->HTotal = radeon_output->PanelXRes + radeon_output->HBlank;
		    adjusted_mode->HSyncStart = radeon_output->PanelXRes + radeon_output->HOverPlus;
		    adjusted_mode->HSyncEnd = adjusted_mode->HSyncStart + radeon_output->HSyncWidth;
		    adjusted_mode->VTotal = radeon_output->PanelYRes + radeon_output->VBlank;
		    adjusted_mode->VSyncStart = radeon_output->PanelYRes + radeon_output->VOverPlus;
		    adjusted_mode->VSyncEnd = adjusted_mode->VSyncStart + radeon_output->VSyncWidth;
		    /* update crtc values */
		    xf86SetModeCrtc(adjusted_mode, INTERLACE_HALVE_V);
		    /* adjust crtc values */
		    adjusted_mode->CrtcHDisplay = radeon_output->PanelXRes;
		    adjusted_mode->CrtcVDisplay = radeon_output->PanelYRes;
		    adjusted_mode->CrtcHTotal = adjusted_mode->CrtcHDisplay + radeon_output->HBlank;
		    adjusted_mode->CrtcHSyncStart = adjusted_mode->CrtcHDisplay + radeon_output->HOverPlus;
		    adjusted_mode->CrtcHSyncEnd = adjusted_mode->CrtcHSyncStart + radeon_output->HSyncWidth;
		    adjusted_mode->CrtcVTotal = adjusted_mode->CrtcVDisplay + radeon_output->VBlank;
		    adjusted_mode->CrtcVSyncStart = adjusted_mode->CrtcVDisplay + radeon_output->VOverPlus;
		    adjusted_mode->CrtcVSyncEnd = adjusted_mode->CrtcVSyncStart + radeon_output->VSyncWidth;
		} else {
		    /* set to the panel's native mode */
		    adjusted_mode->HTotal = radeon_output->PanelXRes + radeon_output->HBlank;
		    adjusted_mode->HSyncStart = radeon_output->PanelXRes + radeon_output->HOverPlus;
		    adjusted_mode->HSyncEnd = adjusted_mode->HSyncStart + radeon_output->HSyncWidth;
		    adjusted_mode->VTotal = radeon_output->PanelYRes + radeon_output->VBlank;
		    adjusted_mode->VSyncStart = radeon_output->PanelYRes + radeon_output->VOverPlus;
		    adjusted_mode->VSyncEnd = adjusted_mode->VSyncStart + radeon_output->VSyncWidth;
		    adjusted_mode->Clock = radeon_output->DotClock;
		    /* update crtc values */
		    xf86SetModeCrtc(adjusted_mode, INTERLACE_HALVE_V);
		    /* adjust crtc values */
		    adjusted_mode->CrtcHTotal = adjusted_mode->CrtcHDisplay + radeon_output->HBlank;
		    adjusted_mode->CrtcHSyncStart = adjusted_mode->CrtcHDisplay + radeon_output->HOverPlus;
		    adjusted_mode->CrtcHSyncEnd = adjusted_mode->CrtcHSyncStart + radeon_output->HSyncWidth;
		    adjusted_mode->CrtcVTotal = adjusted_mode->CrtcVDisplay + radeon_output->VBlank;
		    adjusted_mode->CrtcVSyncStart = adjusted_mode->CrtcVDisplay + radeon_output->VOverPlus;
		    adjusted_mode->CrtcVSyncEnd = adjusted_mode->CrtcVSyncStart + radeon_output->VSyncWidth;
		}
		adjusted_mode->Clock = radeon_output->DotClock;
		adjusted_mode->Flags = radeon_output->Flags;
	    }
	}
    }

    if (IS_AVIVO_VARIANT) {
	/* hw bug */
	if ((mode->Flags & V_INTERLACE)
	    && (adjusted_mode->CrtcVSyncStart < (adjusted_mode->CrtcVDisplay + 2)))
	    adjusted_mode->CrtcVSyncStart = adjusted_mode->CrtcVDisplay + 2;
    }

    return TRUE;
}

static void
radeon_mode_prepare(xf86OutputPtr output)
{
    RADEONInfoPtr info = RADEONPTR(output->scrn);
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR (output->scrn);
    int o;

    for (o = 0; o < config->num_output; o++) {
	xf86OutputPtr loop_output = config->output[o];
	if (loop_output == output)
	    continue;
	else if (loop_output->crtc) {
	    xf86CrtcPtr other_crtc = loop_output->crtc;
	    RADEONCrtcPrivatePtr other_radeon_crtc = other_crtc->driver_private;
	    if (other_crtc->enabled) {
		radeon_crtc_dpms(other_crtc, DPMSModeOff);
		if (IS_AVIVO_VARIANT)
		    atombios_lock_crtc(info->atomBIOS, other_radeon_crtc->crtc_id, 1);
		radeon_dpms(loop_output, DPMSModeOff);
	    }
	}
    }

    radeon_bios_output_lock(output, TRUE);
    radeon_dpms(output, DPMSModeOff);
    radeon_crtc_dpms(output->crtc, DPMSModeOff);

}

static void
radeon_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		DisplayModePtr adjusted_mode)
{
    RADEONInfoPtr info = RADEONPTR(output->scrn);

    if (IS_AVIVO_VARIANT)
	atombios_output_mode_set(output, mode, adjusted_mode);
    else
	legacy_output_mode_set(output, mode, adjusted_mode);
    radeon_bios_output_crtc(output);

}

static void
radeon_mode_commit(xf86OutputPtr output)
{
    RADEONInfoPtr info = RADEONPTR(output->scrn);
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR (output->scrn);
    int o;

    for (o = 0; o < config->num_output; o++) {
	xf86OutputPtr loop_output = config->output[o];
	if (loop_output == output)
	    continue;
	else if (loop_output->crtc) {
	    xf86CrtcPtr other_crtc = loop_output->crtc;
	    RADEONCrtcPrivatePtr other_radeon_crtc = other_crtc->driver_private;
	    if (other_crtc->enabled) {
		radeon_crtc_dpms(other_crtc, DPMSModeOn);
		if (IS_AVIVO_VARIANT)
		    atombios_lock_crtc(info->atomBIOS, other_radeon_crtc->crtc_id, 0);
		radeon_dpms(loop_output, DPMSModeOn);
	    }
	}
    }

    radeon_dpms(output, DPMSModeOn);
    radeon_crtc_dpms(output->crtc, DPMSModeOn);
    radeon_bios_output_lock(output, FALSE);
}

static void
radeon_bios_output_lock(xf86OutputPtr output, Bool lock)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONSavePtr save = info->ModeReg;

    if (info->IsAtomBios) {
	if (lock) {
	    save->bios_6_scratch |= ATOM_S6_CRITICAL_STATE;
	} else {
	    save->bios_6_scratch &= ~ATOM_S6_CRITICAL_STATE;
	}
    } else {
	if (lock) {
	    save->bios_6_scratch |= RADEON_DRIVER_CRITICAL;
	} else {
	    save->bios_6_scratch &= ~RADEON_DRIVER_CRITICAL;
	}
    }
    if (info->ChipFamily >= CHIP_FAMILY_R600)
	OUTREG(R600_BIOS_6_SCRATCH, save->bios_6_scratch);
    else
	OUTREG(RADEON_BIOS_6_SCRATCH, save->bios_6_scratch);
}

static void
radeon_bios_output_dpms(xf86OutputPtr output, int mode)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONSavePtr save = info->ModeReg;

    if (info->IsAtomBios) {
	if (radeon_output->active_device & ATOM_DEVICE_TV1_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_2_scratch &= ~ATOM_S2_TV1_DPMS_STATE;
		save->bios_3_scratch |= ATOM_S3_TV1_ACTIVE;
	    } else {
		save->bios_2_scratch |= ATOM_S2_TV1_DPMS_STATE;
		save->bios_3_scratch &= ~ATOM_S3_TV1_ACTIVE;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_CV_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_2_scratch &= ~ATOM_S2_CV_DPMS_STATE;
		save->bios_3_scratch |= ATOM_S3_CV_ACTIVE;
	    } else {
		save->bios_2_scratch |= ATOM_S2_CV_DPMS_STATE;
		save->bios_3_scratch &= ~ATOM_S3_CV_ACTIVE;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_CRT1_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_2_scratch &= ~ATOM_S2_CRT1_DPMS_STATE;
		save->bios_3_scratch |= ATOM_S3_CRT1_ACTIVE;
	    } else {
		save->bios_2_scratch |= ATOM_S2_CRT1_DPMS_STATE;
		save->bios_3_scratch &= ~ATOM_S3_CRT1_ACTIVE;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_CRT2_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_2_scratch &= ~ATOM_S2_CRT2_DPMS_STATE;
		save->bios_3_scratch |= ATOM_S3_CRT2_ACTIVE;
	    } else {
		save->bios_2_scratch |= ATOM_S2_CRT2_DPMS_STATE;
		save->bios_3_scratch &= ~ATOM_S3_CRT2_ACTIVE;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_LCD1_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_2_scratch &= ~ATOM_S2_LCD1_DPMS_STATE;
		save->bios_3_scratch |= ATOM_S3_LCD1_ACTIVE;
	    } else {
		save->bios_2_scratch |= ATOM_S2_LCD1_DPMS_STATE;
		save->bios_3_scratch &= ~ATOM_S3_LCD1_ACTIVE;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP1_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_2_scratch &= ~ATOM_S2_DFP1_DPMS_STATE;
		save->bios_3_scratch |= ATOM_S3_DFP1_ACTIVE;
	    } else {
		save->bios_2_scratch |= ATOM_S2_DFP1_DPMS_STATE;
		save->bios_3_scratch &= ~ATOM_S3_DFP1_ACTIVE;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP2_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_2_scratch &= ~ATOM_S2_DFP2_DPMS_STATE;
		save->bios_3_scratch |= ATOM_S3_DFP2_ACTIVE;
	    } else {
		save->bios_2_scratch |= ATOM_S2_DFP2_DPMS_STATE;
		save->bios_3_scratch &= ~ATOM_S3_DFP2_ACTIVE;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP3_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_2_scratch &= ~ATOM_S2_DFP3_DPMS_STATE;
		save->bios_3_scratch |= ATOM_S3_DFP3_ACTIVE;
	    } else {
		save->bios_2_scratch |= ATOM_S2_DFP3_DPMS_STATE;
		save->bios_3_scratch &= ~ATOM_S3_DFP3_ACTIVE;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP4_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_2_scratch &= ~ATOM_S2_DFP4_DPMS_STATE;
		save->bios_3_scratch |= ATOM_S3_DFP4_ACTIVE;
	    } else {
		save->bios_2_scratch |= ATOM_S2_DFP4_DPMS_STATE;
		save->bios_3_scratch &= ~ATOM_S3_DFP4_ACTIVE;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP5_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_2_scratch &= ~ATOM_S2_DFP5_DPMS_STATE;
		save->bios_3_scratch |= ATOM_S3_DFP5_ACTIVE;
	    } else {
		save->bios_2_scratch |= ATOM_S2_DFP5_DPMS_STATE;
		save->bios_3_scratch &= ~ATOM_S3_DFP5_ACTIVE;
	    }
	}
	if (info->ChipFamily >= CHIP_FAMILY_R600) {
	    OUTREG(R600_BIOS_2_SCRATCH, save->bios_2_scratch);
	    OUTREG(R600_BIOS_3_SCRATCH, save->bios_3_scratch);
	} else {
	    OUTREG(RADEON_BIOS_2_SCRATCH, save->bios_2_scratch);
	    OUTREG(RADEON_BIOS_3_SCRATCH, save->bios_3_scratch);
	}
    } else {
	if (mode == DPMSModeOn) {
	    save->bios_6_scratch &= ~(RADEON_DPMS_MASK | RADEON_SCREEN_BLANKING);
	    save->bios_6_scratch |= RADEON_DPMS_ON;
	} else {
	    save->bios_6_scratch &= ~RADEON_DPMS_MASK;
	    save->bios_6_scratch |= (RADEON_DPMS_OFF | RADEON_SCREEN_BLANKING);
	}
	if (radeon_output->active_device & ATOM_DEVICE_TV1_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_5_scratch |= RADEON_TV1_ON;
		save->bios_6_scratch |= RADEON_TV_DPMS_ON;
	    } else {
		save->bios_5_scratch &= ~RADEON_TV1_ON;
		save->bios_6_scratch &= ~RADEON_TV_DPMS_ON;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_CRT1_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_5_scratch |= RADEON_CRT1_ON;
		save->bios_6_scratch |= RADEON_CRT_DPMS_ON;
	    } else {
		save->bios_5_scratch &= ~RADEON_CRT1_ON;
		save->bios_6_scratch &= ~RADEON_CRT_DPMS_ON;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_CRT2_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_5_scratch |= RADEON_CRT2_ON;
		save->bios_6_scratch |= RADEON_CRT_DPMS_ON;
	    } else {
		save->bios_5_scratch &= ~RADEON_CRT2_ON;
		save->bios_6_scratch &= ~RADEON_CRT_DPMS_ON;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_LCD1_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_5_scratch |= RADEON_LCD1_ON;
		save->bios_6_scratch |= RADEON_LCD_DPMS_ON;
	    } else {
		save->bios_5_scratch &= ~RADEON_LCD1_ON;
		save->bios_6_scratch &= ~RADEON_LCD_DPMS_ON;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP1_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_5_scratch |= RADEON_DFP1_ON;
		save->bios_6_scratch |= RADEON_DFP_DPMS_ON;
	    } else {
		save->bios_5_scratch &= ~RADEON_DFP1_ON;
		save->bios_6_scratch &= ~RADEON_DFP_DPMS_ON;
	    }
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP2_SUPPORT) {
	    if (mode == DPMSModeOn) {
		save->bios_5_scratch |= RADEON_DFP2_ON;
		save->bios_6_scratch |= RADEON_DFP_DPMS_ON;
	    } else {
		save->bios_5_scratch &= ~RADEON_DFP2_ON;
		save->bios_6_scratch &= ~RADEON_DFP_DPMS_ON;
	    }
	}
	OUTREG(RADEON_BIOS_5_SCRATCH, save->bios_5_scratch);
	OUTREG(RADEON_BIOS_6_SCRATCH, save->bios_6_scratch);
    }
}

static void
radeon_bios_output_crtc(xf86OutputPtr output)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONSavePtr save = info->ModeReg;
    xf86CrtcPtr crtc = output->crtc;
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;

    if (info->IsAtomBios) {
	if (radeon_output->active_device & ATOM_DEVICE_TV1_SUPPORT) {
	    save->bios_3_scratch &= ~ATOM_S3_TV1_CRTC_ACTIVE;
	    save->bios_3_scratch |= (radeon_crtc->crtc_id << 18);
	} else if (radeon_output->active_device & ATOM_DEVICE_CV_SUPPORT) {
	    save->bios_2_scratch &= ~ATOM_S3_CV_CRTC_ACTIVE;
	    save->bios_3_scratch |= (radeon_crtc->crtc_id << 24);
	} else if (radeon_output->active_device & ATOM_DEVICE_CRT1_SUPPORT) {
	    save->bios_2_scratch &= ~ATOM_S3_CRT1_CRTC_ACTIVE;
	    save->bios_3_scratch |= (radeon_crtc->crtc_id << 16);
	} else if (radeon_output->active_device & ATOM_DEVICE_CRT2_SUPPORT) {
	    save->bios_2_scratch &= ~ATOM_S3_CRT2_CRTC_ACTIVE;
	    save->bios_3_scratch |= (radeon_crtc->crtc_id << 20);
	} else if (radeon_output->active_device & ATOM_DEVICE_LCD1_SUPPORT) {
	    save->bios_2_scratch &= ~ATOM_S3_LCD1_CRTC_ACTIVE;
	    save->bios_3_scratch |= (radeon_crtc->crtc_id << 17);
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP1_SUPPORT) {
	    save->bios_2_scratch &= ~ATOM_S3_DFP1_CRTC_ACTIVE;
	    save->bios_3_scratch |= (radeon_crtc->crtc_id << 19);
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP2_SUPPORT) {
	    save->bios_2_scratch &= ~ATOM_S3_DFP2_CRTC_ACTIVE;
	    save->bios_3_scratch |= (radeon_crtc->crtc_id << 23);
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP3_SUPPORT) {
	    save->bios_2_scratch &= ~ATOM_S3_DFP3_CRTC_ACTIVE;
	    save->bios_3_scratch |= (radeon_crtc->crtc_id << 25);
	}
	if (info->ChipFamily >= CHIP_FAMILY_R600)
	    OUTREG(R600_BIOS_3_SCRATCH, save->bios_3_scratch);
	else
	    OUTREG(RADEON_BIOS_3_SCRATCH, save->bios_3_scratch);
    } else {
	if (radeon_output->active_device & ATOM_DEVICE_TV1_SUPPORT) {
	    save->bios_5_scratch &= ~RADEON_TV1_CRTC_MASK;
	    save->bios_5_scratch |= (radeon_crtc->crtc_id << RADEON_TV1_CRTC_SHIFT);
	} else if (radeon_output->active_device & ATOM_DEVICE_CRT1_SUPPORT) {
	    save->bios_5_scratch &= ~RADEON_CRT1_CRTC_MASK;
	    save->bios_5_scratch |= (radeon_crtc->crtc_id << RADEON_CRT1_CRTC_SHIFT);
	} else if (radeon_output->active_device & ATOM_DEVICE_CRT2_SUPPORT) {
	    save->bios_5_scratch &= ~RADEON_CRT2_CRTC_MASK;
	    save->bios_5_scratch |= (radeon_crtc->crtc_id << RADEON_CRT2_CRTC_SHIFT);
	} else if (radeon_output->active_device & ATOM_DEVICE_LCD1_SUPPORT) {
	    save->bios_5_scratch &= ~RADEON_LCD1_CRTC_MASK;
	    save->bios_5_scratch |= (radeon_crtc->crtc_id << RADEON_LCD1_CRTC_SHIFT);
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP1_SUPPORT) {
	    save->bios_5_scratch &= ~RADEON_DFP1_CRTC_MASK;
	    save->bios_5_scratch |= (radeon_crtc->crtc_id << RADEON_DFP1_CRTC_SHIFT);
	} else if (radeon_output->active_device & ATOM_DEVICE_DFP2_SUPPORT) {
	    save->bios_5_scratch &= ~RADEON_DFP2_CRTC_MASK;
	    save->bios_5_scratch |= (radeon_crtc->crtc_id << RADEON_DFP2_CRTC_SHIFT);
	}
	OUTREG(RADEON_BIOS_5_SCRATCH, save->bios_5_scratch);
    }
}

static void
radeon_bios_output_connected(xf86OutputPtr output, Bool connected)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONSavePtr save = info->ModeReg;

    if (info->IsAtomBios) {
	switch (radeon_output->active_device) {
	case ATOM_DEVICE_TV1_SUPPORT:
	    if (!connected)
		save->bios_0_scratch &= ~ATOM_S0_TV1_MASK;
	    break;
	case ATOM_DEVICE_CV_SUPPORT:
	    if (!connected)
		save->bios_0_scratch &= ~ATOM_S0_CV_MASK;
	    break;
	case ATOM_DEVICE_LCD1_SUPPORT:
	    if (connected)
		save->bios_0_scratch |= ATOM_S0_LCD1;
	    else
		save->bios_0_scratch &= ~ATOM_S0_LCD1;
	    break;
	case ATOM_DEVICE_CRT1_SUPPORT:
	    if (connected)
		save->bios_0_scratch |= ATOM_S0_CRT1_COLOR;
	    else
		save->bios_0_scratch &= ~ATOM_S0_CRT1_MASK;
	    break;
	case ATOM_DEVICE_CRT2_SUPPORT:
	    if (connected)
		save->bios_0_scratch |= ATOM_S0_CRT2_COLOR;
	    else
		save->bios_0_scratch &= ~ATOM_S0_CRT2_MASK;
	    break;
	case ATOM_DEVICE_DFP1_SUPPORT:
	    if (connected)
		save->bios_0_scratch |= ATOM_S0_DFP1;
	    else
		save->bios_0_scratch &= ~ATOM_S0_DFP1;
	    break;
	case ATOM_DEVICE_DFP2_SUPPORT:
	    if (connected)
		save->bios_0_scratch |= ATOM_S0_DFP2;
	    else
		save->bios_0_scratch &= ~ATOM_S0_DFP2;
	    break;
	case ATOM_DEVICE_DFP3_SUPPORT:
	    if (connected)
		save->bios_0_scratch |= ATOM_S0_DFP3;
	    else
		save->bios_0_scratch &= ~ATOM_S0_DFP3;
	    break;
	case ATOM_DEVICE_DFP4_SUPPORT:
	    if (connected)
		save->bios_0_scratch |= ATOM_S0_DFP4;
	    else
		save->bios_0_scratch &= ~ATOM_S0_DFP4;
	    break;
	case ATOM_DEVICE_DFP5_SUPPORT:
	    if (connected)
		save->bios_0_scratch |= ATOM_S0_DFP5;
	    else
		save->bios_0_scratch &= ~ATOM_S0_DFP5;
	    break;
	}
	if (info->ChipFamily >= CHIP_FAMILY_R600)
	    OUTREG(R600_BIOS_0_SCRATCH, save->bios_0_scratch);
	else
	    OUTREG(RADEON_BIOS_0_SCRATCH, save->bios_0_scratch);
    } else {
	switch (radeon_output->active_device) {
	case ATOM_DEVICE_TV1_SUPPORT:
	    if (connected) {
		if (radeon_output->MonType == MT_STV)
		    save->bios_4_scratch |= RADEON_TV1_ATTACHED_SVIDEO;
		else if (radeon_output->MonType == MT_CTV)
		    save->bios_4_scratch |= RADEON_TV1_ATTACHED_COMP;
	    } else
		save->bios_4_scratch &= ~RADEON_TV1_ATTACHED_MASK;
	    break;
	case ATOM_DEVICE_LCD1_SUPPORT:
	    if (connected)
		save->bios_4_scratch |= RADEON_LCD1_ATTACHED;
	    else
		save->bios_4_scratch &= ~RADEON_LCD1_ATTACHED;
	    break;
	case ATOM_DEVICE_CRT1_SUPPORT:
	    if (connected)
		save->bios_4_scratch |= RADEON_CRT1_ATTACHED_COLOR;
	    else
		save->bios_4_scratch &= ~RADEON_CRT1_ATTACHED_MASK;
	    break;
	case ATOM_DEVICE_CRT2_SUPPORT:
	    if (connected)
		save->bios_4_scratch |= RADEON_CRT2_ATTACHED_COLOR;
	    else
		save->bios_4_scratch &= ~RADEON_CRT2_ATTACHED_MASK;
	    break;
	case ATOM_DEVICE_DFP1_SUPPORT:
	    if (connected)
		save->bios_4_scratch |= RADEON_DFP1_ATTACHED;
	    else
		save->bios_4_scratch &= ~RADEON_DFP1_ATTACHED;
	    break;
	case ATOM_DEVICE_DFP2_SUPPORT:
	    if (connected)
		save->bios_4_scratch |= RADEON_DFP2_ATTACHED;
	    else
		save->bios_4_scratch &= ~RADEON_DFP2_ATTACHED;
	    break;
	}
	OUTREG(RADEON_BIOS_4_SCRATCH, save->bios_4_scratch);
    }

}

static xf86OutputStatus
radeon_detect(xf86OutputPtr output)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    Bool connected = TRUE;

    radeon_output->MonType = MT_UNKNOWN;
    radeon_bios_output_connected(output, FALSE);
    radeon_output->MonType = radeon_ddc_connected(output);
    if (!radeon_output->MonType) {
	if (radeon_output->devices & (ATOM_DEVICE_LCD_SUPPORT)) {
	    if (xf86ReturnOptValBool(info->Options, OPTION_IGNORE_LID_STATUS, TRUE))
		radeon_output->MonType = MT_LCD;
	    else
#if defined(__powerpc__)
		radeon_output->MonType = MT_LCD;
#else
	        radeon_output->MonType = RADEONDetectLidStatus(pScrn);
#endif
	} else {
	    if (info->IsAtomBios)
		radeon_output->MonType = atombios_dac_detect(output);
	    else
		radeon_output->MonType = legacy_dac_detect(output);
	}
    }

    /* update panel info for RMX */
    if (radeon_output->MonType == MT_LCD || radeon_output->MonType == MT_DFP)
	RADEONUpdatePanelSize(output);

    /* panel is probably busted or not connected */
    if ((radeon_output->MonType == MT_LCD) &&
	((radeon_output->PanelXRes == 0) || (radeon_output->PanelYRes == 0)))
	radeon_output->MonType = MT_NONE;

    if (output->MonInfo) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID data from the display on output: %s ----------------------\n",
		   output->name);
	xf86PrintEDID( output->MonInfo );
    }

    /* nothing connected, light up some defaults so the server comes up */
    if (radeon_output->MonType == MT_NONE &&
	info->first_load_no_devices) {
	if (info->IsMobility) {
	    if (radeon_output->devices & (ATOM_DEVICE_LCD_SUPPORT)) {
		radeon_output->MonType = MT_LCD;
		info->first_load_no_devices = FALSE;
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Using LCD default\n");
	    }
	} else {
	    if (radeon_output->devices & (ATOM_DEVICE_CRT_SUPPORT)) {
		radeon_output->MonType = MT_CRT;
		info->first_load_no_devices = FALSE;
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Using CRT default\n");
	    } else if (radeon_output->devices & (ATOM_DEVICE_DFP_SUPPORT)) {
		radeon_output->MonType = MT_DFP;
		info->first_load_no_devices = FALSE;
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Using DFP default\n");
	    }
	}
    }

    radeon_bios_output_connected(output, TRUE);

    /* set montype so users can force outputs on even if detection fails */
    if (radeon_output->MonType == MT_NONE) {
	connected = FALSE;
	switch (radeon_output->ConnectorType) {
	case CONNECTOR_LVDS:
	    radeon_output->MonType = MT_LCD;
	    break;
	case CONNECTOR_DVI_D:
	case CONNECTOR_HDMI_TYPE_A:
	case CONNECTOR_HDMI_TYPE_B:
	    radeon_output->MonType = MT_DFP;
	    break;
	case CONNECTOR_VGA:
	case CONNECTOR_DVI_A:
	default:
	    radeon_output->MonType = MT_CRT;
	    break;
	case CONNECTOR_DVI_I:
	    if (radeon_output->DVIType == DVI_ANALOG)
		radeon_output->MonType = MT_CRT;
	    else if (radeon_output->DVIType == DVI_DIGITAL)
		radeon_output->MonType = MT_DFP;
	    break;
	case CONNECTOR_STV:
            radeon_output->MonType = MT_STV;
	    break;
	case CONNECTOR_CTV:
            radeon_output->MonType = MT_CTV;
	    break;
	case CONNECTOR_DIN:
            radeon_output->MonType = MT_CV;
	    break;
	case CONNECTOR_DISPLAY_PORT:
	    break;
	}
    }

    radeon_set_active_device(output);

    if (radeon_output->active_device & (ATOM_DEVICE_LCD_SUPPORT | ATOM_DEVICE_DFP_SUPPORT))
	output->subpixel_order = SubPixelHorizontalRGB;
    else
	output->subpixel_order = SubPixelNone;

    if (connected)
	return XF86OutputStatusConnected;
    else
	return XF86OutputStatusDisconnected;
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
    if (output->driver_private)
        xfree(output->driver_private);
}

static void
radeon_set_backlight_level(xf86OutputPtr output, int level)
{
#if 0
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char * RADEONMMIO = info->MMIO;
    uint32_t lvds_gen_cntl;

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
static Atom tmds_pll_atom;
static Atom rmx_atom;
static Atom monitor_type_atom;
static Atom load_detection_atom;
static Atom coherent_mode_atom;
static Atom tv_hsize_atom;
static Atom tv_hpos_atom;
static Atom tv_vpos_atom;
static Atom tv_std_atom;
#define RADEON_MAX_BACKLIGHT_LEVEL 255

static void
radeon_create_resources(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    radeon_encoder_ptr radeon_encoder = radeon_get_encoder(output);
    INT32 range[2];
    int data, err;
    const char *s;

    if (radeon_encoder == NULL)
	return;

#if 0
    /* backlight control */
    if (radeon_output->type == OUTPUT_LVDS) {
	backlight_atom = MAKE_ATOM("backlight");

	range[0] = 0;
	range[1] = RADEON_MAX_BACKLIGHT_LEVEL;
	err = RRConfigureOutputProperty(output->randr_output, backlight_atom,
					FALSE, TRUE, FALSE, 2, range);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRConfigureOutputProperty error, %d\n", err);
	}
	/* Set the current value of the backlight property */
	//data = (info->SavedReg->lvds_gen_cntl & RADEON_LVDS_BL_MOD_LEVEL_MASK) >> RADEON_LVDS_BL_MOD_LEVEL_SHIFT;
	data = RADEON_MAX_BACKLIGHT_LEVEL;
	err = RRChangeOutputProperty(output->randr_output, backlight_atom,
				     XA_INTEGER, 32, PropModeReplace, 1, &data,
				     FALSE, TRUE);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRChangeOutputProperty error, %d\n", err);
	}
    }
#endif

    if (radeon_output->devices & (ATOM_DEVICE_CRT_SUPPORT)) {
	load_detection_atom = MAKE_ATOM("load_detection");

	range[0] = 0; /* off */
	range[1] = 1; /* on */
	err = RRConfigureOutputProperty(output->randr_output, load_detection_atom,
					FALSE, TRUE, FALSE, 2, range);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRConfigureOutputProperty error, %d\n", err);
	}

	if (radeon_output->load_detection)
	    data = 1;
	else
	    data = 0;

	err = RRChangeOutputProperty(output->randr_output, load_detection_atom,
				     XA_INTEGER, 32, PropModeReplace, 1, &data,
				     FALSE, TRUE);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRChangeOutputProperty error, %d\n", err);
	}
    }

    if (IS_AVIVO_VARIANT && (radeon_output->devices & (ATOM_DEVICE_DFP_SUPPORT))) {
	coherent_mode_atom = MAKE_ATOM("coherent_mode");

	range[0] = 0; /* off */
	range[1] = 1; /* on */
	err = RRConfigureOutputProperty(output->randr_output, coherent_mode_atom,
					FALSE, TRUE, FALSE, 2, range);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRConfigureOutputProperty error, %d\n", err);
	}

	data = 1; /* coherent mode on by default */

	err = RRChangeOutputProperty(output->randr_output, coherent_mode_atom,
				     XA_INTEGER, 32, PropModeReplace, 1, &data,
				     FALSE, TRUE);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRChangeOutputProperty error, %d\n", err);
	}
    }

    if ((!IS_AVIVO_VARIANT) && (radeon_encoder->encoder_id == ENCODER_OBJECT_ID_INTERNAL_TMDS1)) {
	tmds_pll_atom = MAKE_ATOM("tmds_pll");

	err = RRConfigureOutputProperty(output->randr_output, tmds_pll_atom,
					FALSE, FALSE, FALSE, 0, NULL);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRConfigureOutputProperty error, %d\n", err);
	}
	/* Set the current value of the property */
#if defined(__powerpc__)
	s = "driver";
#else
	s = "bios";
#endif
	if (xf86ReturnOptValBool(info->Options, OPTION_DEFAULT_TMDS_PLL, FALSE)) {
	    s = "driver";
	}

	err = RRChangeOutputProperty(output->randr_output, tmds_pll_atom,
				     XA_STRING, 8, PropModeReplace, strlen(s), (pointer)s,
				     FALSE, FALSE);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRChangeOutputProperty error, %d\n", err);
	}

    }

    /* RMX control - fullscreen, centered, keep ratio, off */
    /* actually more of a crtc property as only crtc1 has rmx */
    if (radeon_output->devices & (ATOM_DEVICE_DFP_SUPPORT | ATOM_DEVICE_LCD_SUPPORT)) {
	rmx_atom = MAKE_ATOM("scaler");

	err = RRConfigureOutputProperty(output->randr_output, rmx_atom,
					FALSE, FALSE, FALSE, 0, NULL);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRConfigureOutputProperty error, %d\n", err);
	}
	/* Set the current value of the property */
	if (radeon_output->devices & (ATOM_DEVICE_LCD_SUPPORT))
	    s = "full";
	else
	    s = "off";
	err = RRChangeOutputProperty(output->randr_output, rmx_atom,
				     XA_STRING, 8, PropModeReplace, strlen(s), (pointer)s,
				     FALSE, FALSE);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRChangeOutputProperty error, %d\n", err);
	}
    }

    /* force auto/analog/digital for DVI-I ports */
    if ((radeon_output->devices & (ATOM_DEVICE_CRT_SUPPORT)) &&
	(radeon_output->devices & (ATOM_DEVICE_DFP_SUPPORT))){
	monitor_type_atom = MAKE_ATOM("dvi_monitor_type");

	err = RRConfigureOutputProperty(output->randr_output, monitor_type_atom,
					FALSE, FALSE, FALSE, 0, NULL);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRConfigureOutputProperty error, %d\n", err);
	}
	/* Set the current value of the backlight property */
	s = "auto";
	err = RRChangeOutputProperty(output->randr_output, monitor_type_atom,
				     XA_STRING, 8, PropModeReplace, strlen(s), (pointer)s,
				     FALSE, FALSE);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRChangeOutputProperty error, %d\n", err);
	}
    }

    if (radeon_output->devices & (ATOM_DEVICE_TV_SUPPORT)) {
	if (!IS_AVIVO_VARIANT) {
	    tv_hsize_atom = MAKE_ATOM("tv_horizontal_size");

	    range[0] = -MAX_H_SIZE;
	    range[1] = MAX_H_SIZE;
	    err = RRConfigureOutputProperty(output->randr_output, tv_hsize_atom,
					    FALSE, TRUE, FALSE, 2, range);
	    if (err != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "RRConfigureOutputProperty error, %d\n", err);
	    }
	    data = 0;
	    err = RRChangeOutputProperty(output->randr_output, tv_hsize_atom,
					 XA_INTEGER, 32, PropModeReplace, 1, &data,
					 FALSE, TRUE);
	    if (err != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "RRChangeOutputProperty error, %d\n", err);
	    }

	    tv_hpos_atom = MAKE_ATOM("tv_horizontal_position");

	    range[0] = -MAX_H_POSITION;
	    range[1] = MAX_H_POSITION;
	    err = RRConfigureOutputProperty(output->randr_output, tv_hpos_atom,
					    FALSE, TRUE, FALSE, 2, range);
	    if (err != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "RRConfigureOutputProperty error, %d\n", err);
	    }
	    data = 0;
	    err = RRChangeOutputProperty(output->randr_output, tv_hpos_atom,
					 XA_INTEGER, 32, PropModeReplace, 1, &data,
					 FALSE, TRUE);
	    if (err != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "RRChangeOutputProperty error, %d\n", err);
	    }

	    tv_vpos_atom = MAKE_ATOM("tv_vertical_position");

	    range[0] = -MAX_V_POSITION;
	    range[1] = MAX_V_POSITION;
	    err = RRConfigureOutputProperty(output->randr_output, tv_vpos_atom,
					    FALSE, TRUE, FALSE, 2, range);
	    if (err != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "RRConfigureOutputProperty error, %d\n", err);
	    }
	    data = 0;
	    err = RRChangeOutputProperty(output->randr_output, tv_vpos_atom,
					 XA_INTEGER, 32, PropModeReplace, 1, &data,
					 FALSE, TRUE);
	    if (err != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "RRChangeOutputProperty error, %d\n", err);
	    }
	}

	tv_std_atom = MAKE_ATOM("tv_standard");

	err = RRConfigureOutputProperty(output->randr_output, tv_std_atom,
					FALSE, FALSE, FALSE, 0, NULL);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRConfigureOutputProperty error, %d\n", err);
	}

	/* Set the current value of the property */
	switch (radeon_output->tvStd) {
	case TV_STD_PAL:
	    s = "pal";
	    break;
	case TV_STD_PAL_M:
	    s = "pal-m";
	    break;
	case TV_STD_PAL_60:
	    s = "pal-60";
	    break;
	case TV_STD_NTSC_J:
	    s = "ntsc-j";
	    break;
	case TV_STD_SCART_PAL:
	    s = "scart-pal";
	    break;
	case TV_STD_NTSC:
	default:
	    s = "ntsc";
	    break;
	}

	err = RRChangeOutputProperty(output->randr_output, tv_std_atom,
				     XA_STRING, 8, PropModeReplace, strlen(s), (pointer)s,
				     FALSE, FALSE);
	if (err != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "RRChangeOutputProperty error, %d\n", err);
	}
    }
}

static Bool
radeon_set_mode_for_property(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;

    if (output->crtc) {
	xf86CrtcPtr crtc = output->crtc;

	if (crtc->enabled) {
	    if (!xf86CrtcSetMode(crtc, &crtc->desiredMode, crtc->desiredRotation,
				 crtc->desiredX, crtc->desiredY)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Failed to set mode after propery change!\n");
		return FALSE;
	    }
	}
    }
    return TRUE;
}

static Bool
radeon_set_property(xf86OutputPtr output, Atom property,
		       RRPropertyValuePtr value)
{
    RADEONInfoPtr info = RADEONPTR(output->scrn);
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

    } else if (property == load_detection_atom) {
	if (value->type != XA_INTEGER ||
	    value->format != 32 ||
	    value->size != 1) {
	    return FALSE;
	}

	val = *(INT32 *)value->data;
	if (val < 0 || val > 1)
	    return FALSE;

	radeon_output->load_detection = val;

    } else if (property == coherent_mode_atom) {
	Bool coherent_mode = radeon_output->coherent_mode;

	if (value->type != XA_INTEGER ||
	    value->format != 32 ||
	    value->size != 1) {
	    return FALSE;
	}

	val = *(INT32 *)value->data;
	if (val < 0 || val > 1)
	    return FALSE;

	radeon_output->coherent_mode = val;
	if (!radeon_set_mode_for_property(output)) {
	    radeon_output->coherent_mode = coherent_mode;
	    (void)radeon_set_mode_for_property(output);
	    return FALSE;
	}

    } else if (property == rmx_atom) {
	const char *s;
	RADEONRMXType rmx = radeon_output->rmx_type;

	if (value->type != XA_STRING || value->format != 8)
	    return FALSE;
	s = (char*)value->data;
	if (value->size == strlen("full") && !strncmp("full", s, strlen("full"))) {
	    radeon_output->rmx_type = RMX_FULL;
	} else if (value->size == strlen("center") && !strncmp("center", s, strlen("center"))) {
	    radeon_output->rmx_type = RMX_CENTER;
	} else if (value->size == strlen("off") && !strncmp("off", s, strlen("off"))) {
	    radeon_output->rmx_type = RMX_OFF;
	} else
	    return FALSE;

	if (!radeon_set_mode_for_property(output)) {
	    radeon_output->rmx_type = rmx;
	    (void)radeon_set_mode_for_property(output);
	    return FALSE;
	}
    } else if (property == tmds_pll_atom) {
	const char *s;
	if (value->type != XA_STRING || value->format != 8)
	    return FALSE;
	s = (char*)value->data;
	if (value->size == strlen("bios") && !strncmp("bios", s, strlen("bios"))) {
	    if (!RADEONGetTMDSInfoFromBIOS(output))
		RADEONGetTMDSInfoFromTable(output);
	} else if (value->size == strlen("driver") && !strncmp("driver", s, strlen("driver"))) {
	    RADEONGetTMDSInfoFromTable(output);
	} else
	    return FALSE;

	return radeon_set_mode_for_property(output);
    } else if (property == monitor_type_atom) {
	const char *s;
	if (value->type != XA_STRING || value->format != 8)
	    return FALSE;
	s = (char*)value->data;
	if (value->size == strlen("auto") && !strncmp("auto", s, strlen("auto"))) {
	    radeon_output->DVIType = DVI_AUTO;
	    return TRUE;
	} else if (value->size == strlen("analog") && !strncmp("analog", s, strlen("analog"))) {
	    radeon_output->DVIType = DVI_ANALOG;
	    return TRUE;
	} else if (value->size == strlen("digital") && !strncmp("digital", s, strlen("digital"))) {
	    radeon_output->DVIType = DVI_DIGITAL;
	    return TRUE;
	} else
	    return FALSE;
    } else if (property == tv_hsize_atom) {
	if (value->type != XA_INTEGER ||
	    value->format != 32 ||
	    value->size != 1) {
	    return FALSE;
	}

	val = *(INT32 *)value->data;
	if (val < -MAX_H_SIZE || val > MAX_H_SIZE)
	    return FALSE;

	radeon_output->hSize = val;
	if (radeon_output->tv_on && !IS_AVIVO_VARIANT)
	    RADEONUpdateHVPosition(output, &output->crtc->mode);

    } else if (property == tv_hpos_atom) {
	if (value->type != XA_INTEGER ||
	    value->format != 32 ||
	    value->size != 1) {
	    return FALSE;
	}

	val = *(INT32 *)value->data;
	if (val < -MAX_H_POSITION || val > MAX_H_POSITION)
	    return FALSE;

	radeon_output->hPos = val;
	if (radeon_output->tv_on && !IS_AVIVO_VARIANT)
	    RADEONUpdateHVPosition(output, &output->crtc->mode);

    } else if (property == tv_vpos_atom) {
	if (value->type != XA_INTEGER ||
	    value->format != 32 ||
	    value->size != 1) {
	    return FALSE;
	}

	val = *(INT32 *)value->data;
	if (val < -MAX_H_POSITION || val > MAX_H_POSITION)
	    return FALSE;

	radeon_output->vPos = val;
	if (radeon_output->tv_on && !IS_AVIVO_VARIANT)
	    RADEONUpdateHVPosition(output, &output->crtc->mode);

    } else if (property == tv_std_atom) {
	const char *s;
	TVStd std = radeon_output->tvStd;

	if (value->type != XA_STRING || value->format != 8)
	    return FALSE;
	s = (char*)value->data;
	if (value->size == strlen("ntsc") && !strncmp("ntsc", s, strlen("ntsc"))) {
	    radeon_output->tvStd = TV_STD_NTSC;
	} else if (value->size == strlen("pal") && !strncmp("pal", s, strlen("pal"))) {
	    radeon_output->tvStd = TV_STD_PAL;
	} else if (value->size == strlen("pal-m") && !strncmp("pal-m", s, strlen("pal-m"))) {
	    radeon_output->tvStd = TV_STD_PAL_M;
	} else if (value->size == strlen("pal-60") && !strncmp("pal-60", s, strlen("pal-60"))) {
	    radeon_output->tvStd = TV_STD_PAL_60;
	} else if (value->size == strlen("ntsc-j") && !strncmp("ntsc-j", s, strlen("ntsc-j"))) {
	    radeon_output->tvStd = TV_STD_NTSC_J;
	} else if (value->size == strlen("scart-pal") && !strncmp("scart-pal", s, strlen("scart-pal"))) {
	    radeon_output->tvStd = TV_STD_SCART_PAL;
	} else if (value->size == strlen("pal-cn") && !strncmp("pal-cn", s, strlen("pal-cn"))) {
	    radeon_output->tvStd = TV_STD_PAL_CN;
	} else if (value->size == strlen("secam") && !strncmp("secam", s, strlen("secam"))) {
	    radeon_output->tvStd = TV_STD_SECAM;
	} else
	    return FALSE;

	if (!radeon_set_mode_for_property(output)) {
	    radeon_output->tvStd = std;
	    (void)radeon_set_mode_for_property(output);
	    return FALSE;
	}
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

Bool
RADEONI2CDoLock(xf86OutputPtr output, int lock_state)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONI2CBusPtr pRADEONI2CBus = radeon_output->pI2CBus->DriverPrivate.ptr;
    unsigned char *RADEONMMIO = info->MMIO;
    uint32_t temp;

    if (lock_state) {
	temp = INREG(pRADEONI2CBus->a_clk_reg);
	temp &= ~(pRADEONI2CBus->a_clk_mask);
	OUTREG(pRADEONI2CBus->a_clk_reg, temp);

	temp = INREG(pRADEONI2CBus->a_data_reg);
	temp &= ~(pRADEONI2CBus->a_data_mask);
	OUTREG(pRADEONI2CBus->a_data_reg, temp);
    }

    temp = INREG(pRADEONI2CBus->mask_clk_reg);
    if (lock_state)
	temp |= (pRADEONI2CBus->mask_clk_mask);
    else
	temp &= ~(pRADEONI2CBus->mask_clk_mask);
    OUTREG(pRADEONI2CBus->mask_clk_reg, temp);
    temp = INREG(pRADEONI2CBus->mask_clk_reg);

    temp = INREG(pRADEONI2CBus->mask_data_reg);
    if (lock_state)
	temp |= (pRADEONI2CBus->mask_data_mask);
    else
	temp &= ~(pRADEONI2CBus->mask_data_mask);
    OUTREG(pRADEONI2CBus->mask_data_reg, temp);
    temp = INREG(pRADEONI2CBus->mask_data_reg);

    return TRUE;
}

static void RADEONI2CGetBits(I2CBusPtr b, int *Clock, int *data)
{
    ScrnInfoPtr    pScrn      = xf86Screens[b->scrnIndex];
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned long  val;
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONI2CBusPtr pRADEONI2CBus = b->DriverPrivate.ptr;

    /* Get the result */
    val = INREG(pRADEONI2CBus->get_clk_reg);
    *Clock = (val & pRADEONI2CBus->get_clk_mask) != 0;
    val = INREG(pRADEONI2CBus->get_data_reg);
    *data  = (val & pRADEONI2CBus->get_data_mask) != 0;

}

static void RADEONI2CPutBits(I2CBusPtr b, int Clock, int data)
{
    ScrnInfoPtr    pScrn      = xf86Screens[b->scrnIndex];
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned long  val;
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONI2CBusPtr pRADEONI2CBus = b->DriverPrivate.ptr;

    val = INREG(pRADEONI2CBus->put_clk_reg) & (uint32_t)~(pRADEONI2CBus->put_clk_mask);
    val |= (Clock ? 0:pRADEONI2CBus->put_clk_mask);
    OUTREG(pRADEONI2CBus->put_clk_reg, val);
    /* read back to improve reliability on some cards. */
    val = INREG(pRADEONI2CBus->put_clk_reg);

    val = INREG(pRADEONI2CBus->put_data_reg) & (uint32_t)~(pRADEONI2CBus->put_data_mask);
    val |= (data ? 0:pRADEONI2CBus->put_data_mask);
    OUTREG(pRADEONI2CBus->put_data_reg, val);
    /* read back to improve reliability on some cards. */
    val = INREG(pRADEONI2CBus->put_data_reg);

}

static Bool
RADEONI2CInit(xf86OutputPtr output, I2CBusPtr *bus_ptr, char *name, Bool dvo)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    I2CBusPtr pI2CBus;
    RADEONI2CBusPtr pRADEONI2CBus;

    pI2CBus = xf86CreateI2CBusRec();
    if (!pI2CBus) return FALSE;

    pI2CBus->BusName    = name;
    pI2CBus->scrnIndex  = pScrn->scrnIndex;
    pI2CBus->I2CPutBits = RADEONI2CPutBits;
    pI2CBus->I2CGetBits = RADEONI2CGetBits;
    pI2CBus->AcknTimeout = 5;

    if (dvo) {
	pRADEONI2CBus = &(radeon_output->dvo_i2c);
    } else {
	pRADEONI2CBus = &(radeon_output->ddc_i2c);
    }

    pI2CBus->DriverPrivate.ptr = (pointer)pRADEONI2CBus;

    if (!xf86I2CBusInit(pI2CBus))
	return FALSE;

    *bus_ptr = pI2CBus;
    return TRUE;
}

RADEONI2CBusRec
legacy_setup_i2c_bus(int ddc_line)
{
    RADEONI2CBusRec i2c;

    i2c.hw_line = 0;
    i2c.hw_capable = FALSE;
    i2c.mask_clk_mask = RADEON_GPIO_EN_1;
    i2c.mask_data_mask = RADEON_GPIO_EN_0;
    i2c.a_clk_mask = RADEON_GPIO_A_1;
    i2c.a_data_mask = RADEON_GPIO_A_0;
    i2c.put_clk_mask = RADEON_GPIO_EN_1;
    i2c.put_data_mask = RADEON_GPIO_EN_0;
    i2c.get_clk_mask = RADEON_GPIO_Y_1;
    i2c.get_data_mask = RADEON_GPIO_Y_0;
    if ((ddc_line == RADEON_LCD_GPIO_MASK) ||
	(ddc_line == RADEON_MDGPIO_EN_REG)) {
	i2c.mask_clk_reg = ddc_line;
	i2c.mask_data_reg = ddc_line;
	i2c.a_clk_reg = ddc_line;
	i2c.a_data_reg = ddc_line;
	i2c.put_clk_reg = ddc_line;
	i2c.put_data_reg = ddc_line;
	i2c.get_clk_reg = ddc_line + 4;
	i2c.get_data_reg = ddc_line + 4;
    } else {
	i2c.mask_clk_reg = ddc_line;
	i2c.mask_data_reg = ddc_line;
	i2c.a_clk_reg = ddc_line;
	i2c.a_data_reg = ddc_line;
	i2c.put_clk_reg = ddc_line;
	i2c.put_data_reg = ddc_line;
	i2c.get_clk_reg = ddc_line;
	i2c.get_data_reg = ddc_line;
    }

    if (ddc_line)
	i2c.valid = TRUE;
    else
	i2c.valid = FALSE;

    return i2c;
}

RADEONI2CBusRec
atom_setup_i2c_bus(int ddc_line)
{
    RADEONI2CBusRec i2c;

    i2c.hw_line = 0;
    i2c.hw_capable = FALSE;
    if (ddc_line == AVIVO_GPIO_0) {
	i2c.put_clk_mask = (1 << 19);
	i2c.put_data_mask = (1 << 18);
	i2c.get_clk_mask = (1 << 19);
	i2c.get_data_mask = (1 << 18);
	i2c.mask_clk_mask = (1 << 19);
	i2c.mask_data_mask = (1 << 18);
	i2c.a_clk_mask = (1 << 19);
	i2c.a_data_mask = (1 << 18);
    } else {
	i2c.put_clk_mask = (1 << 0);
	i2c.put_data_mask = (1 << 8);
	i2c.get_clk_mask = (1 << 0);
	i2c.get_data_mask = (1 << 8);
	i2c.mask_clk_mask = (1 << 0);
	i2c.mask_data_mask = (1 << 8);
	i2c.a_clk_mask = (1 << 0);
	i2c.a_data_mask = (1 << 8);
    }
    i2c.mask_clk_reg = ddc_line;
    i2c.mask_data_reg = ddc_line;
    i2c.a_clk_reg = ddc_line + 0x4;
    i2c.a_data_reg = ddc_line + 0x4;
    i2c.put_clk_reg = ddc_line + 0x8;
    i2c.put_data_reg = ddc_line + 0x8;
    i2c.get_clk_reg = ddc_line + 0xc;
    i2c.get_data_reg = ddc_line + 0xc;
    if (ddc_line)
	i2c.valid = TRUE;
    else
	i2c.valid = FALSE;

    return i2c;
}

static void
RADEONGetPanelInfoFromReg (xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    unsigned char *RADEONMMIO = info->MMIO;
    uint32_t fp_vert_stretch = INREG(RADEON_FP_VERT_STRETCH);
    uint32_t fp_horz_stretch = INREG(RADEON_FP_HORZ_STRETCH);

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
           uint32_t ppll_div_sel, ppll_val;

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
RADEONGetTVInfo(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    char *optstr;

    radeon_output->hPos = 0;
    radeon_output->vPos = 0;
    radeon_output->hSize = 0;

    if (!RADEONGetTVInfoFromBIOS(output)) {
	/* set some reasonable defaults */
	radeon_output->default_tvStd = TV_STD_NTSC;
	radeon_output->tvStd = TV_STD_NTSC;
	radeon_output->TVRefClk = 27.000000000;
	radeon_output->SupportedTVStds = TV_STD_NTSC | TV_STD_PAL;
    }

    optstr = (char *)xf86GetOptValString(info->Options, OPTION_TVSTD);
    if (optstr) {
	if (!strncmp("ntsc", optstr, strlen("ntsc")))
	    radeon_output->tvStd = TV_STD_NTSC;
	else if (!strncmp("pal", optstr, strlen("pal")))
	    radeon_output->tvStd = TV_STD_PAL;
	else if (!strncmp("pal-m", optstr, strlen("pal-m")))
	    radeon_output->tvStd = TV_STD_PAL_M;
	else if (!strncmp("pal-60", optstr, strlen("pal-60")))
	    radeon_output->tvStd = TV_STD_PAL_60;
	else if (!strncmp("ntsc-j", optstr, strlen("ntsc-j")))
	    radeon_output->tvStd = TV_STD_NTSC_J;
	else if (!strncmp("scart-pal", optstr, strlen("scart-pal")))
	    radeon_output->tvStd = TV_STD_SCART_PAL;
	else {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid TV Standard: %s\n", optstr);
	}
    }

}

void RADEONInitConnector(xf86OutputPtr output)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    radeon_encoder_ptr radeon_encoder = radeon_get_encoder(output);

    if (radeon_encoder == NULL)
	return;

    radeon_output->rmx_type = RMX_OFF;

    if (!IS_AVIVO_VARIANT) {
	if (radeon_encoder->encoder_id == ENCODER_OBJECT_ID_INTERNAL_DVO1)
	    RADEONGetExtTMDSInfo(output);

	if (radeon_encoder->encoder_id == ENCODER_OBJECT_ID_INTERNAL_TMDS1)
	    RADEONGetTMDSInfo(output);

	if (radeon_encoder->encoder_id == ENCODER_OBJECT_ID_INTERNAL_DAC2) {
	    if (xf86ReturnOptValBool(info->Options, OPTION_TVDAC_LOAD_DETECT, FALSE))
		radeon_output->load_detection = 1;
	    radeon_output->tv_on = FALSE;
	    RADEONGetTVDacAdjInfo(output);
	}
    }

    if (radeon_output->devices & (ATOM_DEVICE_LCD_SUPPORT)) {
	radeon_output->rmx_type = RMX_FULL;
	RADEONGetLVDSInfo(output);
    }

    if (radeon_output->devices & (ATOM_DEVICE_TV_SUPPORT))
	RADEONGetTVInfo(output);

    if (radeon_output->devices & (ATOM_DEVICE_DFP_SUPPORT))
	radeon_output->coherent_mode = TRUE;

    if (radeon_output->ddc_i2c.valid)
	RADEONI2CInit(output, &radeon_output->pI2CBus, output->name, FALSE);

}

#if defined(__powerpc__)
static Bool RADEONSetupAppleConnectors(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);


    switch (info->MacModel) {
    case RADEON_MAC_IBOOK:
	info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
	info->BiosConnector[0].ConnectorType = CONNECTOR_LVDS;
	info->BiosConnector[0].valid = TRUE;
	info->BiosConnector[0].devices = ATOM_DEVICE_LCD1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_LCD1_SUPPORT,
									    0),
				ATOM_DEVICE_LCD1_SUPPORT))
	    return FALSE;

	info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	info->BiosConnector[1].load_detection = FALSE;
	info->BiosConnector[1].ConnectorType = CONNECTOR_VGA;
	info->BiosConnector[1].valid = TRUE;
	info->BiosConnector[1].devices = ATOM_DEVICE_CRT2_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_CRT2_SUPPORT,
									    2),
				ATOM_DEVICE_CRT2_SUPPORT))
	    return FALSE;

	info->BiosConnector[2].ConnectorType = CONNECTOR_STV;
	info->BiosConnector[2].load_detection = FALSE;
	info->BiosConnector[2].ddc_i2c.valid = FALSE;
	info->BiosConnector[2].valid = TRUE;
	info->BiosConnector[2].devices = ATOM_DEVICE_TV1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_TV1_SUPPORT,
									    2),
				ATOM_DEVICE_TV1_SUPPORT))
	    return FALSE;
	return TRUE;
    case RADEON_MAC_POWERBOOK_EXTERNAL:
	info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
	info->BiosConnector[0].ConnectorType = CONNECTOR_LVDS;
	info->BiosConnector[0].valid = TRUE;
	info->BiosConnector[0].devices = ATOM_DEVICE_LCD1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_LCD1_SUPPORT,
									    0),
				ATOM_DEVICE_LCD1_SUPPORT))
	    return FALSE;

	info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	info->BiosConnector[1].ConnectorType = CONNECTOR_DVI_I;
	info->BiosConnector[1].valid = TRUE;
	info->BiosConnector[1].devices = ATOM_DEVICE_CRT1_SUPPORT | ATOM_DEVICE_DFP2_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_CRT1_SUPPORT,
									    1),
				ATOM_DEVICE_CRT1_SUPPORT))
	    return FALSE;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_DFP2_SUPPORT,
									    0),
				ATOM_DEVICE_DFP2_SUPPORT))
	    return FALSE;

	info->BiosConnector[2].ConnectorType = CONNECTOR_STV;
	info->BiosConnector[2].load_detection = FALSE;
	info->BiosConnector[2].ddc_i2c.valid = FALSE;
	info->BiosConnector[2].valid = TRUE;
	info->BiosConnector[2].devices = ATOM_DEVICE_TV1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_TV1_SUPPORT,
									    2),
				ATOM_DEVICE_TV1_SUPPORT))
	    return FALSE;
	return TRUE;
    case RADEON_MAC_POWERBOOK_INTERNAL:
	info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
	info->BiosConnector[0].ConnectorType = CONNECTOR_LVDS;
	info->BiosConnector[0].valid = TRUE;
	info->BiosConnector[0].devices = ATOM_DEVICE_LCD1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_LCD1_SUPPORT,
									    0),
				ATOM_DEVICE_LCD1_SUPPORT))
	    return FALSE;

	info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	info->BiosConnector[1].ConnectorType = CONNECTOR_DVI_I;
	info->BiosConnector[1].valid = TRUE;
	info->BiosConnector[1].devices = ATOM_DEVICE_CRT1_SUPPORT | ATOM_DEVICE_DFP1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_CRT1_SUPPORT,
									    1),
				ATOM_DEVICE_CRT1_SUPPORT))
	    return FALSE;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_DFP1_SUPPORT,
									    0),
				ATOM_DEVICE_DFP1_SUPPORT))
	    return FALSE;

	info->BiosConnector[2].ConnectorType = CONNECTOR_STV;
	info->BiosConnector[2].load_detection = FALSE;
	info->BiosConnector[2].ddc_i2c.valid = FALSE;
	info->BiosConnector[2].valid = TRUE;
	info->BiosConnector[2].devices = ATOM_DEVICE_TV1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_TV1_SUPPORT,
									    2),
				ATOM_DEVICE_TV1_SUPPORT))
	    return FALSE;
	return TRUE;
    case RADEON_MAC_POWERBOOK_VGA:
	info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
	info->BiosConnector[0].ConnectorType = CONNECTOR_LVDS;
	info->BiosConnector[0].valid = TRUE;
	info->BiosConnector[0].devices = ATOM_DEVICE_LCD1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_LCD1_SUPPORT,
									    0),
				ATOM_DEVICE_LCD1_SUPPORT))
	    return FALSE;

	info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	info->BiosConnector[1].ConnectorType = CONNECTOR_VGA;
	info->BiosConnector[1].valid = TRUE;
	info->BiosConnector[1].devices = ATOM_DEVICE_CRT1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_CRT1_SUPPORT,
									    1),
				ATOM_DEVICE_CRT1_SUPPORT))
	    return FALSE;

	info->BiosConnector[2].ConnectorType = CONNECTOR_STV;
	info->BiosConnector[2].load_detection = FALSE;
	info->BiosConnector[2].ddc_i2c.valid = FALSE;
	info->BiosConnector[2].valid = TRUE;
	info->BiosConnector[2].devices = ATOM_DEVICE_TV1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_TV1_SUPPORT,
									    2),
				ATOM_DEVICE_TV1_SUPPORT))
	    return FALSE;
	return TRUE;
    case RADEON_MAC_MINI_EXTERNAL:
	info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_CRT2_DDC);
	info->BiosConnector[0].load_detection = FALSE;
	info->BiosConnector[0].ConnectorType = CONNECTOR_DVI_I;
	info->BiosConnector[0].valid = TRUE;
	info->BiosConnector[0].devices = ATOM_DEVICE_CRT1_SUPPORT | ATOM_DEVICE_DFP2_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_CRT1_SUPPORT,
									    1),
				ATOM_DEVICE_CRT1_SUPPORT))
	    return FALSE;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_DFP2_SUPPORT,
									    0),
				ATOM_DEVICE_DFP2_SUPPORT))
	    return FALSE;

	info->BiosConnector[1].ConnectorType = CONNECTOR_STV;
	info->BiosConnector[1].load_detection = FALSE;
	info->BiosConnector[1].ddc_i2c.valid = FALSE;
	info->BiosConnector[1].valid = TRUE;
	info->BiosConnector[1].devices = ATOM_DEVICE_TV1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_TV1_SUPPORT,
									    2),
				ATOM_DEVICE_TV1_SUPPORT))
	    return FALSE;
	return TRUE;
    case RADEON_MAC_MINI_INTERNAL:
	info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_CRT2_DDC);
	info->BiosConnector[0].load_detection = FALSE;
	info->BiosConnector[0].ConnectorType = CONNECTOR_DVI_I;
	info->BiosConnector[0].valid = TRUE;
	info->BiosConnector[0].devices = ATOM_DEVICE_CRT1_SUPPORT | ATOM_DEVICE_DFP1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_CRT1_SUPPORT,
									    1),
				ATOM_DEVICE_CRT1_SUPPORT))
	    return FALSE;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_DFP1_SUPPORT,
									    0),
				ATOM_DEVICE_DFP1_SUPPORT))
	    return FALSE;

	info->BiosConnector[1].ConnectorType = CONNECTOR_STV;
	info->BiosConnector[1].load_detection = FALSE;
	info->BiosConnector[1].ddc_i2c.valid = FALSE;
	info->BiosConnector[1].valid = TRUE;
	info->BiosConnector[1].devices = ATOM_DEVICE_TV1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_TV1_SUPPORT,
									    2),
				ATOM_DEVICE_TV1_SUPPORT))
	    return FALSE;
	return TRUE;
    case RADEON_MAC_IMAC_G5_ISIGHT:
	info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_MONID);
	info->BiosConnector[0].ConnectorType = CONNECTOR_DVI_D;
	info->BiosConnector[0].valid = TRUE;
	info->BiosConnector[0].devices = ATOM_DEVICE_DFP1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_DFP1_SUPPORT,
									    0),
				ATOM_DEVICE_DFP1_SUPPORT))
	    return FALSE;

	info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
	info->BiosConnector[1].load_detection = FALSE;
	info->BiosConnector[1].ConnectorType = CONNECTOR_VGA;
	info->BiosConnector[1].valid = TRUE;
	info->BiosConnector[1].devices = ATOM_DEVICE_CRT2_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_CRT2_SUPPORT,
									    2),
				ATOM_DEVICE_CRT2_SUPPORT))
	    return FALSE;

	info->BiosConnector[2].ConnectorType = CONNECTOR_STV;
	info->BiosConnector[2].load_detection = FALSE;
	info->BiosConnector[2].ddc_i2c.valid = FALSE;
	info->BiosConnector[2].valid = TRUE;
	info->BiosConnector[2].devices = ATOM_DEVICE_TV1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_TV1_SUPPORT,
									    2),
				ATOM_DEVICE_TV1_SUPPORT))
	    return FALSE;
	return TRUE;
    case RADEON_MAC_EMAC:
	/* eMac G4 800/1.0 with radeon 7500, no EDID on internal monitor
	 * later eMac's (G4 1.25/1.42) with radeon 9200 and 9600 may have
	 * different ddc setups.  need to verify
	 */
	info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	info->BiosConnector[0].ConnectorType = CONNECTOR_VGA;
	info->BiosConnector[0].valid = TRUE;
	info->BiosConnector[0].devices = ATOM_DEVICE_CRT1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_CRT1_SUPPORT,
									    1),
				ATOM_DEVICE_CRT1_SUPPORT))
	    return FALSE;

	info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_CRT2_DDC);
	info->BiosConnector[1].load_detection = FALSE;
	info->BiosConnector[1].ConnectorType = CONNECTOR_VGA;
	info->BiosConnector[1].valid = TRUE;
	info->BiosConnector[1].devices = ATOM_DEVICE_CRT2_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_CRT2_SUPPORT,
									    2),
				ATOM_DEVICE_CRT2_SUPPORT))
	    return FALSE;

	info->BiosConnector[2].ConnectorType = CONNECTOR_STV;
	info->BiosConnector[2].load_detection = FALSE;
	info->BiosConnector[2].ddc_i2c.valid = FALSE;
	info->BiosConnector[2].valid = TRUE;
	info->BiosConnector[2].devices = ATOM_DEVICE_TV1_SUPPORT;
	if (!radeon_add_encoder(pScrn,
				radeon_get_encoder_id_from_supported_device(pScrn,
									    ATOM_DEVICE_TV1_SUPPORT,
									    2),
				ATOM_DEVICE_TV1_SUPPORT))
	    return FALSE;
	return TRUE;
    default:
	return FALSE;
    }

    return FALSE;
}
#endif

static void RADEONSetupGenericConnectors(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);

    if (IS_AVIVO_VARIANT)
	return;

    if (!pRADEONEnt->HasCRTC2) {
	info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	info->BiosConnector[0].ConnectorType = CONNECTOR_VGA;
	info->BiosConnector[0].valid = TRUE;
	info->BiosConnector[0].devices = ATOM_DEVICE_CRT1_SUPPORT;
	radeon_add_encoder(pScrn,
			   radeon_get_encoder_id_from_supported_device(pScrn,
								       ATOM_DEVICE_CRT1_SUPPORT,
								       1),
			   ATOM_DEVICE_CRT1_SUPPORT);
	return;
    }

    if (info->IsMobility) {
	/* Below is the most common setting, but may not be true */
	if (info->IsIGP) {
	    info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_LCD_GPIO_MASK);
	    info->BiosConnector[0].ConnectorType = CONNECTOR_LVDS;
	    info->BiosConnector[0].valid = TRUE;
	    info->BiosConnector[0].devices = ATOM_DEVICE_LCD1_SUPPORT;
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_LCD1_SUPPORT,
									   0),
			       ATOM_DEVICE_LCD1_SUPPORT);

	    /* IGP only has TVDAC */
	    if ((info->ChipFamily == CHIP_FAMILY_RS400) ||
		(info->ChipFamily == CHIP_FAMILY_RS480))
		info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_CRT2_DDC);
	    else
		info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	    info->BiosConnector[1].load_detection = FALSE;
	    info->BiosConnector[1].ConnectorType = CONNECTOR_VGA;
	    info->BiosConnector[1].valid = TRUE;
	    info->BiosConnector[1].devices = ATOM_DEVICE_CRT1_SUPPORT;
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_CRT1_SUPPORT,
									   2),
			       ATOM_DEVICE_CRT1_SUPPORT);
	} else {
#if defined(__powerpc__)
	    info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
#else
	    info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_LCD_GPIO_MASK);
#endif
	    info->BiosConnector[0].ConnectorType = CONNECTOR_LVDS;
	    info->BiosConnector[0].valid = TRUE;
	    info->BiosConnector[0].devices = ATOM_DEVICE_LCD1_SUPPORT;
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_LCD1_SUPPORT,
									   0),
			       ATOM_DEVICE_LCD1_SUPPORT);

	    info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	    info->BiosConnector[1].ConnectorType = CONNECTOR_VGA;
	    info->BiosConnector[1].valid = TRUE;
	    info->BiosConnector[1].devices = ATOM_DEVICE_CRT1_SUPPORT;
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_CRT1_SUPPORT,
									   1),
			       ATOM_DEVICE_CRT1_SUPPORT);
	}
    } else {
	/* Below is the most common setting, but may not be true */
	if (info->IsIGP) {
	    if ((info->ChipFamily == CHIP_FAMILY_RS400) ||
		(info->ChipFamily == CHIP_FAMILY_RS480))
		info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_CRT2_DDC);
	    else
		info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	    info->BiosConnector[0].load_detection = FALSE;
	    info->BiosConnector[0].ConnectorType = CONNECTOR_VGA;
	    info->BiosConnector[0].valid = TRUE;
	    info->BiosConnector[0].devices = ATOM_DEVICE_CRT1_SUPPORT;
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_CRT1_SUPPORT,
									   1),
			       ATOM_DEVICE_CRT1_SUPPORT);

	    /* not sure what a good default DDCType for DVI on
	     * IGP desktop chips is
	     */
	    info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_MONID); /* DDC_DVI? */
	    info->BiosConnector[1].ConnectorType = CONNECTOR_DVI_D;
	    info->BiosConnector[1].valid = TRUE;
	    info->BiosConnector[1].devices = ATOM_DEVICE_DFP1_SUPPORT;
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_DFP1_SUPPORT,
									   0),
			       ATOM_DEVICE_DFP1_SUPPORT);
	} else {
	    info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
	    info->BiosConnector[0].load_detection = FALSE;
	    info->BiosConnector[0].ConnectorType = CONNECTOR_DVI_I;
	    info->BiosConnector[0].valid = TRUE;
	    info->BiosConnector[0].devices = ATOM_DEVICE_CRT2_SUPPORT | ATOM_DEVICE_DFP1_SUPPORT;
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_CRT2_SUPPORT,
									   2),
			       ATOM_DEVICE_CRT2_SUPPORT);
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_DFP1_SUPPORT,
									   0),
			       ATOM_DEVICE_DFP1_SUPPORT);

#if defined(__powerpc__)
	    info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	    info->BiosConnector[1].ConnectorType = CONNECTOR_DVI_I;
	    info->BiosConnector[1].valid = TRUE;
	    info->BiosConnector[1].devices = ATOM_DEVICE_CRT1_SUPPORT | ATOM_DEVICE_DFP2_SUPPORT;
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_CRT1_SUPPORT,
									   1),
			       ATOM_DEVICE_CRT1_SUPPORT);
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_DFP2_SUPPORT,
									   0),
			       ATOM_DEVICE_DFP2_SUPPORT);
#else
	    info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	    info->BiosConnector[1].ConnectorType = CONNECTOR_VGA;
	    info->BiosConnector[1].valid = TRUE;
	    info->BiosConnector[1].devices = ATOM_DEVICE_CRT1_SUPPORT;
	    radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_CRT1_SUPPORT,
									   1),
			       ATOM_DEVICE_CRT1_SUPPORT);
#endif
	}
    }

    if (info->InternalTVOut) {
	info->BiosConnector[2].ConnectorType = CONNECTOR_STV;
	info->BiosConnector[2].load_detection = FALSE;
	info->BiosConnector[2].ddc_i2c.valid = FALSE;
	info->BiosConnector[2].valid = TRUE;
	info->BiosConnector[2].devices = ATOM_DEVICE_TV1_SUPPORT;
	radeon_add_encoder(pScrn,
			       radeon_get_encoder_id_from_supported_device(pScrn,
									   ATOM_DEVICE_TV1_SUPPORT,
									   2),
			       ATOM_DEVICE_TV1_SUPPORT);
    }

    /* Some cards have the DDC lines swapped and we have no way to
     * detect it yet (Mac cards)
     */
    if (xf86ReturnOptValBool(info->Options, OPTION_REVERSE_DDC, FALSE)) {
	info->BiosConnector[0].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_VGA_DDC);
	info->BiosConnector[1].ddc_i2c = legacy_setup_i2c_bus(RADEON_GPIO_DVI_DDC);
    }
}

#if defined(__powerpc__)

/*
 * Returns RADEONMacModel or 0 based on lines 'detected as' and 'machine'
 * in /proc/cpuinfo (on Linux) */
static RADEONMacModel RADEONDetectMacModel(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONMacModel ret = 0;
#ifdef __linux__
    char cpuline[50];  /* 50 should be sufficient for our purposes */
    FILE *f = fopen ("/proc/cpuinfo", "r");

    /* Some macs (minis and powerbooks) use internal tmds, others use external tmds
     * and not just for dual-link TMDS, it shows up with single-link as well.
     * Unforunately, there doesn't seem to be any good way to figure it out.
     */

    /*
     * PowerBook5,[1-5]: external tmds, single-link
     * PowerBook5,[789]: external tmds, dual-link
     * PowerBook5,6:     external tmds, single-link or dual-link
     * need to add another option to specify the external tmds chip
     * or find out what's used and add it.
     */


    if (f != NULL) {
	while (fgets(cpuline, sizeof cpuline, f)) {
	    if (!strncmp(cpuline, "machine", strlen ("machine"))) {
		if (strstr(cpuline, "PowerBook5,1") ||
		    strstr(cpuline, "PowerBook5,2") ||
		    strstr(cpuline, "PowerBook5,3") ||
		    strstr(cpuline, "PowerBook5,4") ||
		    strstr(cpuline, "PowerBook5,5")) {
		    ret = RADEON_MAC_POWERBOOK_EXTERNAL; /* single link */
		    info->ext_tmds_chip = RADEON_SIL_164; /* works on 5,2 */
		    break;
		}

		if (strstr(cpuline, "PowerBook5,6")) {
		    ret = RADEON_MAC_POWERBOOK_EXTERNAL; /* dual or single link */
		    break;
		}

		if (strstr(cpuline, "PowerBook5,7") ||
		    strstr(cpuline, "PowerBook5,8") ||
		    strstr(cpuline, "PowerBook5,9")) {
		    ret = RADEON_MAC_POWERBOOK_EXTERNAL; /* dual link */
		    info->ext_tmds_chip = RADEON_SIL_1178; /* guess */
		    break;
		}

		if (strstr(cpuline, "PowerBook3,3")) {
		    ret = RADEON_MAC_POWERBOOK_VGA; /* vga rather than dvi */
		    break;
		}

		if (strstr(cpuline, "PowerMac10,1")) {
		    ret = RADEON_MAC_MINI_INTERNAL; /* internal tmds */
		    break;
		}
		if (strstr(cpuline, "PowerMac10,2")) {
		    ret = RADEON_MAC_MINI_EXTERNAL; /* external tmds */
		    break;
		}
	    } else if (!strncmp(cpuline, "detected as", strlen("detected as"))) {
		if (strstr(cpuline, "iBook")) {
		    ret = RADEON_MAC_IBOOK;
		    break;
		} else if (strstr(cpuline, "PowerBook")) {
		    ret = RADEON_MAC_POWERBOOK_INTERNAL; /* internal tmds */
		    break;
		} else if (strstr(cpuline, "iMac G5 (iSight)")) {
		    ret = RADEON_MAC_IMAC_G5_ISIGHT;
		    break;
		} else if (strstr(cpuline, "eMac")) {
		    ret = RADEON_MAC_EMAC;
		    break;
		}

		/* No known PowerMac model detected */
		break;
	    }
	}

	fclose (f);
    } else
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Cannot detect PowerMac model because /proc/cpuinfo not "
		   "readable.\n");

#endif /* __linux */

    if (ret) {
	xf86DrvMsg(pScrn->scrnIndex, X_DEFAULT, "Detected %s.\n",
		   ret == RADEON_MAC_POWERBOOK_EXTERNAL ? "PowerBook with external DVI" :
		   ret == RADEON_MAC_POWERBOOK_INTERNAL ? "PowerBook with integrated DVI" :
		   ret == RADEON_MAC_POWERBOOK_VGA ? "PowerBook with VGA" :
		   ret == RADEON_MAC_IBOOK ? "iBook" :
		   ret == RADEON_MAC_MINI_EXTERNAL ? "Mac Mini with external DVI" :
		   ret == RADEON_MAC_MINI_INTERNAL ? "Mac Mini with integrated DVI" :
		   "iMac G5 iSight");
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "If this is not correct, try Option \"MacModel\" and "
		   "consider reporting to the\n");
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "xorg-driver-ati@lists.x.org mailing list"
#ifdef __linux__
		   " with the contents of /proc/cpuinfo"
#endif
		   ".\n");
    }

    return ret;
}

#endif /* __powerpc__ */

static int
radeon_output_clones (ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR (pScrn);
    int			o;
    int			index_mask = 0;

    if (IS_DCE3_VARIANT)
	return index_mask;

    /* LVDS is too wacky */
    if (radeon_output->devices & (ATOM_DEVICE_LCD_SUPPORT))
	return index_mask;

    if (radeon_output->devices & (ATOM_DEVICE_TV_SUPPORT))
	return index_mask;

    for (o = 0; o < config->num_output; o++) {
	xf86OutputPtr clone = config->output[o];
	RADEONOutputPrivatePtr radeon_clone = clone->driver_private;

	if (output == clone) /* don't clone yourself */
	    continue;
	else if (radeon_clone->devices & (ATOM_DEVICE_LCD_SUPPORT)) /* LVDS */
	    continue;
	else if (radeon_clone->devices & (ATOM_DEVICE_TV_SUPPORT)) /* TV */
	    continue;
	else
	    index_mask |= (1 << o);
    }

    return index_mask;
}

/*
 * initialise the static data sos we don't have to re-do at randr change */
Bool RADEONSetupConnectors(ScrnInfoPtr pScrn)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    xf86OutputPtr output;
    char *optstr;
    int i;
    int num_vga = 0;
    int num_dvi = 0;
    int num_hdmi = 0;

    /* We first get the information about all connectors from BIOS.
     * This is how the card is phyiscally wired up.
     * The information should be correct even on a OEM card.
     */
    for (i = 0; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	info->encoders[i] = NULL;
	info->BiosConnector[i].valid = FALSE;
	info->BiosConnector[i].load_detection = TRUE;
	info->BiosConnector[i].shared_ddc = FALSE;
	info->BiosConnector[i].ddc_i2c.valid = FALSE;
	info->BiosConnector[i].ConnectorType = CONNECTOR_NONE;
	info->BiosConnector[i].devices = 0;
    }

#if defined(__powerpc__)
    info->MacModel = 0;
    optstr = (char *)xf86GetOptValString(info->Options, OPTION_MAC_MODEL);
    if (optstr) {
	if (!strncmp("ibook", optstr, strlen("ibook")))
	    info->MacModel = RADEON_MAC_IBOOK;
	else if (!strncmp("powerbook-duallink", optstr, strlen("powerbook-duallink"))) /* alias */
	    info->MacModel = RADEON_MAC_POWERBOOK_EXTERNAL;
	else if (!strncmp("powerbook-external", optstr, strlen("powerbook-external")))
	    info->MacModel = RADEON_MAC_POWERBOOK_EXTERNAL;
	else if (!strncmp("powerbook-internal", optstr, strlen("powerbook-internal")))
	    info->MacModel = RADEON_MAC_POWERBOOK_INTERNAL;
	else if (!strncmp("powerbook-vga", optstr, strlen("powerbook-vga")))
	    info->MacModel = RADEON_MAC_POWERBOOK_VGA;
	else if (!strncmp("powerbook", optstr, strlen("powerbook"))) /* alias */
	    info->MacModel = RADEON_MAC_POWERBOOK_INTERNAL;
	else if (!strncmp("mini-internal", optstr, strlen("mini-internal")))
	    info->MacModel = RADEON_MAC_MINI_INTERNAL;
	else if (!strncmp("mini-external", optstr, strlen("mini-external")))
	    info->MacModel = RADEON_MAC_MINI_EXTERNAL;
	else if (!strncmp("mini", optstr, strlen("mini"))) /* alias */
	    info->MacModel = RADEON_MAC_MINI_EXTERNAL;
	else if (!strncmp("imac-g5-isight", optstr, strlen("imac-g5-isight")))
	    info->MacModel = RADEON_MAC_IMAC_G5_ISIGHT;
	else if (!strncmp("emac", optstr, strlen("emac")))
	    info->MacModel = RADEON_MAC_EMAC;
	else {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid Mac Model: %s\n", optstr);
	}
    }

    if (!info->MacModel) {
	info->MacModel = RADEONDetectMacModel(pScrn);
    }

    if (info->MacModel){
	if (!RADEONSetupAppleConnectors(pScrn))
	    RADEONSetupGenericConnectors(pScrn);
    } else
#endif
    if (xf86ReturnOptValBool(info->Options, OPTION_DEFAULT_CONNECTOR_TABLE, FALSE)) {
	RADEONSetupGenericConnectors(pScrn);
    } else {
	if (!RADEONGetConnectorInfoFromBIOS(pScrn))
	    RADEONSetupGenericConnectors(pScrn);
    }

    /* parse connector table option */
    optstr = (char *)xf86GetOptValString(info->Options, OPTION_CONNECTORTABLE);

    if (optstr) {
	unsigned int ddc_line[2];
	int DACType[2], TMDSType[2];

	for (i = 2; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	    info->BiosConnector[i].valid = FALSE;
	}

	if (sscanf(optstr, "%u,%u,%u,%u,%u,%u,%u,%u",
		   &ddc_line[0],
		   &DACType[0],
		   &TMDSType[0],
		   &info->BiosConnector[0].ConnectorType,
		   &ddc_line[1],
		   &DACType[1],
		   &TMDSType[1],
		   &info->BiosConnector[1].ConnectorType) != 8) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid ConnectorTable option: %s\n", optstr);
	    return FALSE;
	}

	for (i = 0; i < 2; i++) {
	    info->BiosConnector[i].valid = TRUE;
	    info->BiosConnector[i].ddc_i2c = legacy_setup_i2c_bus(ddc_line[i]);
	    switch (DACType[i]) {
	    case 1:
		info->BiosConnector[i].devices |= ATOM_DEVICE_CRT1_SUPPORT;
		if (!radeon_add_encoder(pScrn,
					radeon_get_encoder_id_from_supported_device(pScrn,
										    ATOM_DEVICE_CRT1_SUPPORT,
										    1),
					ATOM_DEVICE_CRT1_SUPPORT))
		    return FALSE;
		info->BiosConnector[i].load_detection = TRUE;
		break;
	    case 2:
		info->BiosConnector[i].devices |= ATOM_DEVICE_CRT2_SUPPORT;
		if (!radeon_add_encoder(pScrn,
					radeon_get_encoder_id_from_supported_device(pScrn,
										    ATOM_DEVICE_CRT1_SUPPORT,
										    2),
					ATOM_DEVICE_CRT1_SUPPORT))
		    return FALSE;
		info->BiosConnector[i].load_detection = FALSE;
		break;
	    }
	    switch (TMDSType[i]) {
	    case 1:
		info->BiosConnector[i].devices |= ATOM_DEVICE_DFP1_SUPPORT;
		if (!radeon_add_encoder(pScrn,
					radeon_get_encoder_id_from_supported_device(pScrn,
										    ATOM_DEVICE_DFP1_SUPPORT,
										    0),
					ATOM_DEVICE_DFP1_SUPPORT))
		    return FALSE;
		break;
	    case 2:
		info->BiosConnector[i].devices |= ATOM_DEVICE_DFP2_SUPPORT;
		if (!radeon_add_encoder(pScrn,
					radeon_get_encoder_id_from_supported_device(pScrn,
										    ATOM_DEVICE_DFP2_SUPPORT,
										    0),
					ATOM_DEVICE_DFP2_SUPPORT))
		    return FALSE;
		break;
	    }
	}
    }

    for (i = 0; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	if (info->BiosConnector[i].valid) {
	    if ((info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_D) ||
		(info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_I) ||
		(info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_A)) {
		num_dvi++;
	    } else if (info->BiosConnector[i].ConnectorType == CONNECTOR_VGA) {
		num_vga++;
	    } else if ((info->BiosConnector[i].ConnectorType == CONNECTOR_HDMI_TYPE_A) ||
		       (info->BiosConnector[i].ConnectorType == CONNECTOR_HDMI_TYPE_B)) {
		num_hdmi++;
	    }
	}
    }

    for (i = 0 ; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	if (info->BiosConnector[i].valid) {
	    RADEONOutputPrivatePtr radeon_output;

	    if (info->BiosConnector[i].ConnectorType == CONNECTOR_NONE)
		continue;

	    radeon_output = xnfcalloc(sizeof(RADEONOutputPrivateRec), 1);
	    if (!radeon_output) {
		return FALSE;
	    }
	    radeon_output->MonType = MT_UNKNOWN;
	    radeon_output->ConnectorType = info->BiosConnector[i].ConnectorType;
	    radeon_output->devices = info->BiosConnector[i].devices;
	    radeon_output->ddc_i2c = info->BiosConnector[i].ddc_i2c;
	    radeon_output->igp_lane_info = info->BiosConnector[i].igp_lane_info;
	    radeon_output->shared_ddc = info->BiosConnector[i].shared_ddc;
	    radeon_output->load_detection = info->BiosConnector[i].load_detection;
	    radeon_output->linkb = info->BiosConnector[i].linkb;
	    radeon_output->connector_id = info->BiosConnector[i].connector_object;

	    if ((info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_D) ||
		(info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_I) ||
		(info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_A)) {
		if (num_dvi > 1) {
		    output = xf86OutputCreate(pScrn, &radeon_output_funcs, "DVI-1");
		    num_dvi--;
		} else {
		    output = xf86OutputCreate(pScrn, &radeon_output_funcs, "DVI-0");
		}
	    } else if (info->BiosConnector[i].ConnectorType == CONNECTOR_VGA) {
		if (num_vga > 1) {
		    output = xf86OutputCreate(pScrn, &radeon_output_funcs, "VGA-1");
		    num_vga--;
		} else {
		    output = xf86OutputCreate(pScrn, &radeon_output_funcs, "VGA-0");
		}
	    } else if ((info->BiosConnector[i].ConnectorType == CONNECTOR_HDMI_TYPE_A) ||
		(info->BiosConnector[i].ConnectorType == CONNECTOR_HDMI_TYPE_B)) {
		if (num_hdmi > 1) {
		    output = xf86OutputCreate(pScrn, &radeon_output_funcs, "HDMI-1");
		    num_hdmi--;
		} else {
		    output = xf86OutputCreate(pScrn, &radeon_output_funcs, "HDMI-0");
		}
	    } else
		output = xf86OutputCreate(pScrn, &radeon_output_funcs,
					  ConnectorTypeName[radeon_output->ConnectorType]);

	    if (!output) {
		return FALSE;
	    }
	    output->driver_private = radeon_output;
	    output->possible_crtcs = 1;
	    /* crtc2 can drive LVDS, it just doesn't have RMX */
	    if (!(radeon_output->devices & (ATOM_DEVICE_LCD_SUPPORT)))
		output->possible_crtcs |= 2;

	    /* we can clone the DACs, and probably TV-out,
	       but I'm not sure it's worth the trouble */
	    output->possible_clones = 0;

	    RADEONInitConnector(output);
	}
    }

    for (i = 0; i < xf86_config->num_output; i++) {
	xf86OutputPtr output = xf86_config->output[i];

	output->possible_clones = radeon_output_clones(pScrn, output);
    }

    return TRUE;
}

