/*
 * Copyright © 2007 Red Hat, Inc.
 * Copyright 2007  Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Dave Airlie <airlied@redhat.com>
 *    Alex Deucher <alexdeucher@gmail.com>
 *
 */

/*
 * avivo output handling functions.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/* DPMS */
#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#include <unistd.h>

#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_atombios.h"

#include "ati_pciids_gen.h"

static int
atombios_output_dac1_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DAC_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.ucAction = 1;

    if (radeon_output->MonType == MT_CRT)
	disp_data.ucDacStandard = ATOM_DAC1_PS2;
    else if (radeon_output->MonType == MT_CV)
	disp_data.ucDacStandard = ATOM_DAC1_CV;
    else if (OUTPUT_IS_TV) {
	switch (radeon_output->tvStd) {
	case TV_STD_PAL:
	case TV_STD_PAL_M:
	case TV_STD_SCART_PAL:
	case TV_STD_SECAM:
	case TV_STD_PAL_CN:
	    disp_data.ucDacStandard = ATOM_DAC1_PAL;
	    break;
	case TV_STD_NTSC:
	case TV_STD_NTSC_J:
	case TV_STD_PAL_60:
	default:
	    disp_data.ucDacStandard = ATOM_DAC1_NTSC;
	    break;
	}
    }

    disp_data.usPixelClock = cpu_to_le16(mode->Clock / 10);
    data.exec.index = GetIndexIntoMasterTable(COMMAND, DAC1EncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output DAC1 setup success\n");
	return ATOM_SUCCESS;
    }

    ErrorF("Output DAC1 setup failed\n");
    return ATOM_NOT_IMPLEMENTED;

}

static int
atombios_output_dac2_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DAC_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.ucAction = 1;

    if (radeon_output->MonType == MT_CRT)
	disp_data.ucDacStandard = ATOM_DAC2_PS2;
    else if (radeon_output->MonType == MT_CV)
	disp_data.ucDacStandard = ATOM_DAC2_CV;
    else if (OUTPUT_IS_TV) {
	switch (radeon_output->tvStd) {
	case TV_STD_NTSC:
	case TV_STD_NTSC_J:
	case TV_STD_PAL_60:
	    disp_data.ucDacStandard = ATOM_DAC2_NTSC;
	    break;
	case TV_STD_PAL:
	case TV_STD_PAL_M:
	case TV_STD_SCART_PAL:
	case TV_STD_SECAM:
	case TV_STD_PAL_CN:
	    disp_data.ucDacStandard = ATOM_DAC2_PAL;
	    break;
	default:
	    disp_data.ucDacStandard = ATOM_DAC2_NTSC;
	    break;
	}
    }

    disp_data.usPixelClock = cpu_to_le16(mode->Clock / 10);
    data.exec.index = GetIndexIntoMasterTable(COMMAND, DAC2EncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output DAC2 setup success\n");
	return ATOM_SUCCESS;
    }

    ErrorF("Output DAC2 setup failed\n");
    return ATOM_NOT_IMPLEMENTED;

}

static int
atombios_output_tv1_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    TV_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.sTVEncoder.ucAction = 1;

    if (radeon_output->MonType == MT_CV)
	disp_data.sTVEncoder.ucTvStandard = ATOM_TV_CV;
    else {
	switch (radeon_output->tvStd) {
	case TV_STD_NTSC:
	    disp_data.sTVEncoder.ucTvStandard = ATOM_TV_NTSC;
	    break;
	case TV_STD_PAL:
	    disp_data.sTVEncoder.ucTvStandard = ATOM_TV_PAL;
	    break;
	case TV_STD_PAL_M:
	    disp_data.sTVEncoder.ucTvStandard = ATOM_TV_PALM;
	    break;
	case TV_STD_PAL_60:
	    disp_data.sTVEncoder.ucTvStandard = ATOM_TV_PAL60;
	    break;
	case TV_STD_NTSC_J:
	    disp_data.sTVEncoder.ucTvStandard = ATOM_TV_NTSCJ;
	    break;
	case TV_STD_SCART_PAL:
	    disp_data.sTVEncoder.ucTvStandard = ATOM_TV_PAL; /* ??? */
	    break;
	case TV_STD_SECAM:
	    disp_data.sTVEncoder.ucTvStandard = ATOM_TV_SECAM;
	    break;
	case TV_STD_PAL_CN:
	    disp_data.sTVEncoder.ucTvStandard = ATOM_TV_PALCN;
	    break;
	default:
	    disp_data.sTVEncoder.ucTvStandard = ATOM_TV_NTSC;
	    break;
	}
    }

    disp_data.sTVEncoder.usPixelClock = cpu_to_le16(mode->Clock / 10);
    data.exec.index = GetIndexIntoMasterTable(COMMAND, TVEncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output TV1 setup success\n");
	return ATOM_SUCCESS;
    }

    ErrorF("Output TV1 setup failed\n");
    return ATOM_NOT_IMPLEMENTED;

}

int
atombios_external_tmds_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.sXTmdsEncoder.ucEnable = 1;

    if (mode->Clock > 165000)
	disp_data.sXTmdsEncoder.ucMisc = 1;
    else
	disp_data.sXTmdsEncoder.ucMisc = 0;

    if (pScrn->rgbBits == 8)
	disp_data.sXTmdsEncoder.ucMisc |= (1 << 1);

    data.exec.index = GetIndexIntoMasterTable(COMMAND, DVOEncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("External TMDS setup success\n");
	return ATOM_SUCCESS;
    }

    ErrorF("External TMDS setup failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

static int
atombios_output_ddia_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DVO_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.sDVOEncoder.ucAction = ATOM_ENABLE;
    disp_data.sDVOEncoder.usPixelClock = cpu_to_le16(mode->Clock / 10);

    if (mode->Clock > 165000)
	disp_data.sDVOEncoder.usDevAttr.sDigAttrib.ucAttribute = PANEL_ENCODER_MISC_DUAL;
    else
	disp_data.sDVOEncoder.usDevAttr.sDigAttrib.ucAttribute = 0;

    data.exec.index = GetIndexIntoMasterTable(COMMAND, DVOEncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("DDIA setup success\n");
	return ATOM_SUCCESS;
    }

    ErrorF("DDIA setup failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

static int
atombios_output_digital_setup(xf86OutputPtr output, int device, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    LVDS_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    LVDS_ENCODER_CONTROL_PS_ALLOCATION_V2 disp_data2;
    AtomBiosArgRec data;
    unsigned char *space;
    int index;
    int major, minor;

    switch (device) {
    case ATOM_DEVICE_DFP1_INDEX:
	index = GetIndexIntoMasterTable(COMMAND, TMDS1EncoderControl);
	break;
    case ATOM_DEVICE_LCD1_INDEX:
	index = GetIndexIntoMasterTable(COMMAND, LVDSEncoderControl);
	break;
    case ATOM_DEVICE_DFP3_INDEX:
	index = GetIndexIntoMasterTable(COMMAND, TMDS2EncoderControl);
	break;
    default:
	return ATOM_NOT_IMPLEMENTED;
	break;
    }

    atombios_get_command_table_version(info->atomBIOS, index, &major, &minor);

    /*ErrorF("table is %d %d\n", major, minor);*/
    switch (major) {
    case 0:
    case 1:
    case 2:
	switch (minor) {
	case 1:
	    disp_data.ucMisc = 0;
	    disp_data.ucAction = PANEL_ENCODER_ACTION_ENABLE;
	    if (radeon_output->type == OUTPUT_HDMI)
		disp_data.ucMisc |= PANEL_ENCODER_MISC_HDMI_TYPE;
	    disp_data.usPixelClock = cpu_to_le16(mode->Clock / 10);
	    if (device == ATOM_DEVICE_LCD1_INDEX) {
		if (radeon_output->lvds_misc & (1 << 0))
		    disp_data.ucMisc |= PANEL_ENCODER_MISC_DUAL;
		if (radeon_output->lvds_misc & (1 << 1))
		    disp_data.ucMisc |= (1 << 1);
	    } else {
		if (radeon_output->linkb)
		    disp_data.ucMisc |= PANEL_ENCODER_MISC_TMDS_LINKB;
		if (mode->Clock > 165000)
		    disp_data.ucMisc |= PANEL_ENCODER_MISC_DUAL;
		if (pScrn->rgbBits == 8)
		    disp_data.ucMisc |= (1 << 1);
	    }
	    data.exec.pspace = &disp_data;
	    break;
	case 2:
	case 3:
	    disp_data2.ucMisc = 0;
	    disp_data2.ucAction = PANEL_ENCODER_ACTION_ENABLE;
	    if (minor == 3) {
		if (radeon_output->coherent_mode) {
		    disp_data2.ucMisc |= PANEL_ENCODER_MISC_COHERENT;
		    xf86DrvMsg(output->scrn->scrnIndex, X_INFO, "Coherent Mode enabled\n");
		}
	    }
	    if (radeon_output->type == OUTPUT_HDMI)
		disp_data2.ucMisc |= PANEL_ENCODER_MISC_HDMI_TYPE;
	    disp_data2.usPixelClock = cpu_to_le16(mode->Clock / 10);
	    disp_data2.ucTruncate = 0;
	    disp_data2.ucSpatial = 0;
	    disp_data2.ucTemporal = 0;
	    disp_data2.ucFRC = 0;
	    if (device == ATOM_DEVICE_LCD1_INDEX) {
		if (radeon_output->lvds_misc & (1 << 0))
		    disp_data2.ucMisc |= PANEL_ENCODER_MISC_DUAL;
		if (radeon_output->lvds_misc & (1 << 5)) {
		    disp_data2.ucSpatial = PANEL_ENCODER_SPATIAL_DITHER_EN;
		    if (radeon_output->lvds_misc & (1 << 1))
			disp_data2.ucSpatial |= PANEL_ENCODER_SPATIAL_DITHER_DEPTH;
		}
		if (radeon_output->lvds_misc & (1 << 6)) {
		    disp_data2.ucTemporal = PANEL_ENCODER_TEMPORAL_DITHER_EN;
		    if (radeon_output->lvds_misc & (1 << 1))
			disp_data2.ucTemporal |= PANEL_ENCODER_TEMPORAL_DITHER_DEPTH;
		    if (((radeon_output->lvds_misc >> 2) & 0x3) == 2)
			disp_data2.ucTemporal |= PANEL_ENCODER_TEMPORAL_LEVEL_4;
		}
	    } else {
		if (radeon_output->linkb)
		    disp_data2.ucMisc |= PANEL_ENCODER_MISC_TMDS_LINKB;
		if (mode->Clock > 165000)
		    disp_data2.ucMisc |= PANEL_ENCODER_MISC_DUAL;
	    }
	    data.exec.pspace = &disp_data2;
	    break;
	default:
	    ErrorF("Unknown table version\n");
	    exit(-1);
	}
	break;
    default:
	ErrorF("Unknown table version\n");
	exit(-1);
    }

    data.exec.index = index;
    data.exec.dataSpace = (void *)&space;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output digital setup success\n");
	return ATOM_SUCCESS;
    }

    ErrorF("Output digital setup failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

static int
atombios_maybe_hdmi_mode(xf86OutputPtr output)
{
#ifndef EDID_COMPLETE_RAWDATA
    /* there's no getting this right unless we have complete EDID */
    return ATOM_ENCODER_MODE_HDMI;
#else
    if (output && xf86MonitorIsHDMI(output->MonInfo))
	return ATOM_ENCODER_MODE_HDMI;

    return ATOM_ENCODER_MODE_DVI;
#endif
}

static int
atombios_output_dig_encoder_setup(xf86OutputPtr output, int device, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONCrtcPrivatePtr radeon_crtc = output->crtc->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DIG_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;
    int index;
    int major, minor;

    if (radeon_crtc->crtc_id)
	index = GetIndexIntoMasterTable(COMMAND, DIG2EncoderControl);
    else
	index = GetIndexIntoMasterTable(COMMAND, DIG1EncoderControl);

    atombios_get_command_table_version(info->atomBIOS, index, &major, &minor);

    disp_data.ucAction = ATOM_ENABLE;
    disp_data.usPixelClock = cpu_to_le16(mode->Clock / 10);

    if (IS_DCE32_VARIANT) {
	if (radeon_output->TMDSType == TMDS_UNIPHY)
	    disp_data.ucConfig = ATOM_ENCODER_CONFIG_V2_TRANSMITTER1;
	if (radeon_output->TMDSType == TMDS_UNIPHY1)
	    disp_data.ucConfig = ATOM_ENCODER_CONFIG_V2_TRANSMITTER2;
	if (radeon_output->TMDSType == TMDS_UNIPHY2)
	    disp_data.ucConfig = ATOM_ENCODER_CONFIG_V2_TRANSMITTER3;
    } else {
	switch (device) {
	case ATOM_DEVICE_DFP1_INDEX:
	    disp_data.ucConfig = ATOM_ENCODER_CONFIG_TRANSMITTER1;
	    break;
	case ATOM_DEVICE_LCD1_INDEX:
	case ATOM_DEVICE_DFP3_INDEX:
	    disp_data.ucConfig = ATOM_ENCODER_CONFIG_TRANSMITTER2;
	    break;
	default:
	    return ATOM_NOT_IMPLEMENTED;
	    break;
	}
    }

    if (mode->Clock > 165000) {
	disp_data.ucConfig |= ATOM_ENCODER_CONFIG_LINKA_B;
	disp_data.ucLaneNum = 8;
    } else {
	if (radeon_output->linkb)
	    disp_data.ucConfig |= ATOM_ENCODER_CONFIG_LINKB;
	else
	    disp_data.ucConfig |= ATOM_ENCODER_CONFIG_LINKA;
	disp_data.ucLaneNum = 4;
    }

    if (OUTPUT_IS_DVI)
	disp_data.ucEncoderMode = ATOM_ENCODER_MODE_DVI;
    else if (radeon_output->type == OUTPUT_HDMI)
	disp_data.ucEncoderMode = atombios_maybe_hdmi_mode(output);
    else if (radeon_output->type == OUTPUT_DP)
	disp_data.ucEncoderMode = ATOM_ENCODER_MODE_DP;
    else if (radeon_output->type == OUTPUT_LVDS)
	disp_data.ucEncoderMode = ATOM_ENCODER_MODE_LVDS;

    data.exec.index = index;
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output DIG%d encoder setup success\n", radeon_crtc->crtc_id + 1);
	return ATOM_SUCCESS;
    }

    ErrorF("Output DIG%d setup failed\n", radeon_crtc->crtc_id + 1);
    return ATOM_NOT_IMPLEMENTED;

}

union dig_transmitter_control {
    DIG_TRANSMITTER_CONTROL_PS_ALLOCATION v1;
    DIG_TRANSMITTER_CONTROL_PARAMETERS_V2 v2;
};

static int
atombios_output_dig_transmitter_setup(xf86OutputPtr output, int device, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONCrtcPrivatePtr radeon_crtc = output->crtc->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    union dig_transmitter_control disp_data;
    AtomBiosArgRec data;
    unsigned char *space;
    int index, num = 0;
    int major, minor;

    memset(&disp_data,0, sizeof(disp_data));
    if (IS_DCE32_VARIANT)
	index = GetIndexIntoMasterTable(COMMAND, UNIPHYTransmitterControl);
    else {
	switch (device) {
	case ATOM_DEVICE_DFP1_INDEX:
	    index = GetIndexIntoMasterTable(COMMAND, DIG1TransmitterControl);
	    num = 1;
	    break;
	case ATOM_DEVICE_LCD1_INDEX:
	case ATOM_DEVICE_DFP3_INDEX:
	    index = GetIndexIntoMasterTable(COMMAND, DIG2TransmitterControl);
	    num = 2;
	    break;
	default:
	    return ATOM_NOT_IMPLEMENTED;
	    break;
	}
    }

    atombios_get_command_table_version(info->atomBIOS, index, &major, &minor);

    disp_data.v1.ucAction = ATOM_TRANSMITTER_ACTION_ENABLE;

    if (IS_DCE32_VARIANT) {
	if (mode->Clock > 165000) {
	    disp_data.v2.usPixelClock = cpu_to_le16((mode->Clock * 10 * 2) / 100);
	    disp_data.v2.acConfig.fDualLinkConnector = 1;
	} else {
	    disp_data.v2.usPixelClock = cpu_to_le16((mode->Clock * 10 * 4) / 100);
	}
	if (radeon_crtc->crtc_id)
	    disp_data.v2.acConfig.ucEncoderSel = 1;
	
	switch (radeon_output->TMDSType) {
	case TMDS_UNIPHY:
	    disp_data.v2.acConfig.ucTransmitterSel = 0;
	    num = 0;
	    break;
	case TMDS_UNIPHY1:
	    disp_data.v2.acConfig.ucTransmitterSel = 1;
	    num = 1;
	    break;
	case TMDS_UNIPHY2:
	    disp_data.v2.acConfig.ucTransmitterSel = 2;
	    num = 2;
	    break;
	default:
	    return ATOM_NOT_IMPLEMENTED;
	    break;
	}
	
	if (OUTPUT_IS_DVI || (radeon_output->type == OUTPUT_HDMI)) {
	    if (radeon_output->coherent_mode) {
		disp_data.v2.acConfig.fCoherentMode = 1;
		xf86DrvMsg(output->scrn->scrnIndex, X_INFO, "UNIPHY%d transmitter: Coherent Mode enabled\n",disp_data.v2.acConfig.ucTransmitterSel);
	    } else
		xf86DrvMsg(output->scrn->scrnIndex, X_INFO, "UNIPHY%d transmitter: Coherent Mode disabled\n",disp_data.v2.acConfig.ucTransmitterSel);
	}
    } else {
	disp_data.v1.ucConfig = ATOM_TRANSMITTER_CONFIG_CLKSRC_PPLL;
	disp_data.v1.usPixelClock = cpu_to_le16((mode->Clock) / 10);
	
	if (radeon_crtc->crtc_id)
	    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_DIG2_ENCODER;
	else
	    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_DIG1_ENCODER;
	
	if (OUTPUT_IS_DVI || (radeon_output->type == OUTPUT_HDMI)) {
	    if (radeon_output->coherent_mode) {
		disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_COHERENT;
		xf86DrvMsg(output->scrn->scrnIndex, X_INFO, "DIG%d transmitter: Coherent Mode enabled\n", num);
	    } else
		xf86DrvMsg(output->scrn->scrnIndex, X_INFO, "DIG%d transmitter: Coherent Mode disabled\n", num);
	}
	
	if (info->IsIGP && (radeon_output->TMDSType == TMDS_UNIPHY)) {
	    if (mode->Clock > 165000) {
		disp_data.v1.ucConfig |= (ATOM_TRANSMITTER_CONFIG_8LANE_LINK |
				       ATOM_TRANSMITTER_CONFIG_LINKA_B);
		/* guess */
		if (radeon_output->igp_lane_info & 0x3)
		    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LANE_0_7;
		else if (radeon_output->igp_lane_info & 0xc)
		    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LANE_8_15;
	    } else {
		disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LINKA;
		if (radeon_output->igp_lane_info & 0x1)
		    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LANE_0_3;
		else if (radeon_output->igp_lane_info & 0x2)
		    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LANE_4_7;
		else if (radeon_output->igp_lane_info & 0x4)
		    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LANE_8_11;
		else if (radeon_output->igp_lane_info & 0x8)
		    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LANE_12_15;
	    }
	} else {
	    if (mode->Clock > 165000)
		disp_data.v1.ucConfig |= (ATOM_TRANSMITTER_CONFIG_8LANE_LINK |
				       ATOM_TRANSMITTER_CONFIG_LINKA_B |
				       ATOM_TRANSMITTER_CONFIG_LANE_0_7);
	    else
		disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LINKA | ATOM_TRANSMITTER_CONFIG_LANE_0_3;
	}
    }

    radeon_output->transmitter_config = disp_data.v1.ucConfig;
	
    data.exec.index = index;
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	if (IS_DCE32_VARIANT)
	    ErrorF("Output UNIPHY%d transmitter setup success\n", num);
	else
	   ErrorF("Output DIG%d transmitter setup success\n", num);
	return ATOM_SUCCESS;
    }

    ErrorF("Output DIG%d transmitter setup failed\n", num);
    return ATOM_NOT_IMPLEMENTED;

}

static int
atombios_output_scaler_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONCrtcPrivatePtr radeon_crtc = output->crtc->driver_private;
    ENABLE_SCALER_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    memset(&disp_data, 0, sizeof(disp_data));

    disp_data.ucScaler = radeon_crtc->crtc_id;

    if (OUTPUT_IS_TV) {
	switch (radeon_output->tvStd) {
	case TV_STD_NTSC:
	    disp_data.ucTVStandard = ATOM_TV_NTSC;
	    break;
	case TV_STD_PAL:
	    disp_data.ucTVStandard = ATOM_TV_PAL;
	    break;
	case TV_STD_PAL_M:
	    disp_data.ucTVStandard = ATOM_TV_PALM;
	    break;
	case TV_STD_PAL_60:
	    disp_data.ucTVStandard = ATOM_TV_PAL60;
	    break;
	case TV_STD_NTSC_J:
	    disp_data.ucTVStandard = ATOM_TV_NTSCJ;
	    break;
	case TV_STD_SCART_PAL:
	    disp_data.ucTVStandard = ATOM_TV_PAL; /* ??? */
	    break;
	case TV_STD_SECAM:
	    disp_data.ucTVStandard = ATOM_TV_SECAM;
	    break;
	case TV_STD_PAL_CN:
	    disp_data.ucTVStandard = ATOM_TV_PALCN;
	    break;
	default:
	    disp_data.ucTVStandard = ATOM_TV_NTSC;
	    break;
	}
	disp_data.ucEnable = SCALER_ENABLE_MULTITAP_MODE;
        ErrorF("Using TV scaler %x %x\n", disp_data.ucTVStandard, disp_data.ucEnable);

    } else if (radeon_output->Flags & RADEON_USE_RMX) {
	ErrorF("Using RMX\n");
	if (radeon_output->rmx_type == RMX_FULL)
	    disp_data.ucEnable = ATOM_SCALER_EXPANSION;
	else if (radeon_output->rmx_type == RMX_CENTER)
	    disp_data.ucEnable = ATOM_SCALER_CENTER;
    } else {
	ErrorF("Not using RMX\n");
	disp_data.ucEnable = ATOM_SCALER_DISABLE;
    }

    data.exec.index = GetIndexIntoMasterTable(COMMAND, EnableScaler);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("scaler %d setup success\n", radeon_crtc->crtc_id);
	return ATOM_SUCCESS;
    }

    ErrorF("scaler %d setup failed\n", radeon_crtc->crtc_id);
    return ATOM_NOT_IMPLEMENTED;

}

static AtomBiosResult
atombios_display_device_control(atomBiosHandlePtr atomBIOS, int index, Bool state)
{
    DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.ucAction = state;
    data.exec.index = index;
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output %d %s success\n", index, state? "enable":"disable");
	return ATOM_SUCCESS;
    }

    ErrorF("Output %d %s failed\n", index, state? "enable":"disable");
    return ATOM_NOT_IMPLEMENTED;
}

static int
atombios_dig_dpms(xf86OutputPtr output, int mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DIG_TRANSMITTER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;
    int block;

    memset(&disp_data, 0, sizeof(disp_data));

    if ((radeon_output->MonType == MT_LCD) ||
	(radeon_output->TMDSType == TMDS_LVTMA))
	block = 2;
    else
	block = 1;

    switch (mode) {
    case DPMSModeOn:
	disp_data.ucAction = ATOM_TRANSMITTER_ACTION_ENABLE_OUTPUT;
	break;
    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
	disp_data.ucAction = ATOM_TRANSMITTER_ACTION_DISABLE_OUTPUT;
	break;
    }

    disp_data.ucConfig = radeon_output->transmitter_config;

    if (IS_DCE32_VARIANT) {
	data.exec.index = GetIndexIntoMasterTable(COMMAND, UNIPHYTransmitterControl);
    } else {
	if (block == 1)
	    data.exec.index = GetIndexIntoMasterTable(COMMAND, DIG1TransmitterControl);
	else
	    data.exec.index = GetIndexIntoMasterTable(COMMAND, DIG2TransmitterControl);
    }
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output DIG%d dpms success\n", block);
	return ATOM_SUCCESS;
    }

    ErrorF("Output DIG%d dpms failed\n", block);
    return ATOM_NOT_IMPLEMENTED;

}

static void
atombios_device_dpms(xf86OutputPtr output, int mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    int index = 0;

    if (radeon_output->MonType == MT_CRT) {
	if (radeon_output->DACType == DAC_PRIMARY)
	    index = GetIndexIntoMasterTable(COMMAND, DAC1OutputControl);
	else if (radeon_output->DACType == DAC_TVDAC)
	    index = GetIndexIntoMasterTable(COMMAND, DAC2OutputControl);
	else
	    return;
    } else if (radeon_output->MonType == MT_DFP) {
	switch (radeon_output->TMDSType) {
	case TMDS_INT:
	    index = GetIndexIntoMasterTable(COMMAND, TMDSAOutputControl);
	    break;
	case TMDS_EXT:
	case TMDS_DDIA:
	    index = GetIndexIntoMasterTable(COMMAND, DVOOutputControl);
	    break;
	case TMDS_LVTMA:
	    index = GetIndexIntoMasterTable(COMMAND, LVTMAOutputControl);
	    break;
	default:
	    return;
	}
    } else if (radeon_output->MonType == MT_LCD) {
	index = GetIndexIntoMasterTable(COMMAND, LCD1OutputControl);
    } else if (radeon_output->MonType == MT_STV ||
               radeon_output->MonType == MT_CTV) {
	index = GetIndexIntoMasterTable(COMMAND, TV1OutputControl);
    } else if (radeon_output->MonType == MT_CV) {
	index = GetIndexIntoMasterTable(COMMAND, CV1OutputControl);
    } else
	return;

    switch (mode) {
    case DPMSModeOn:
	atombios_display_device_control(info->atomBIOS, index, ATOM_ENABLE);
	break;
    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
	atombios_display_device_control(info->atomBIOS, index, ATOM_DISABLE);
	break;
    }
}

void
atombios_output_dpms(xf86OutputPtr output, int mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);

    /*ErrorF("output dpms %d\n", mode);*/
    if (IS_DCE3_VARIANT) {
	if (radeon_output->MonType == MT_LCD)
	    atombios_dig_dpms(output, mode);
	else if (radeon_output->MonType == MT_DFP) {
	    if (radeon_output->TMDSType >= TMDS_LVTMA)
		atombios_dig_dpms(output, mode);
	    else
		atombios_device_dpms(output, mode);
	} else
	    atombios_device_dpms(output, mode);
    } else
	atombios_device_dpms(output, mode);

}

static void
atombios_set_output_crtc_source(xf86OutputPtr output)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONCrtcPrivatePtr radeon_crtc = output->crtc->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    AtomBiosArgRec data;
    unsigned char *space;
    SELECT_CRTC_SOURCE_PS_ALLOCATION crtc_src_param;
    SELECT_CRTC_SOURCE_PARAMETERS_V2 crtc_src_param2;
    int index = GetIndexIntoMasterTable(COMMAND, SelectCRTC_Source);
    int major, minor;

    memset(&crtc_src_param, 0, sizeof(crtc_src_param));
    memset(&crtc_src_param2, 0, sizeof(crtc_src_param2));
    atombios_get_command_table_version(info->atomBIOS, index, &major, &minor);

    /*ErrorF("select crtc source table is %d %d\n", major, minor);*/

    switch(major) {
    case 1:
	switch(minor) {
	case 0:
	case 1:
	default:
	    crtc_src_param.ucCRTC = radeon_crtc->crtc_id;
	    crtc_src_param.ucDevice = 0;
	    if (radeon_output->MonType == MT_CRT) {
		if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_CRT1_INDEX;
		else if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_CRT2_INDEX;
	    } else if (radeon_output->MonType == MT_DFP) {
		if (radeon_output->devices & ATOM_DEVICE_DFP1_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_DFP1_INDEX;
		else if (radeon_output->devices & ATOM_DEVICE_DFP2_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_DFP2_INDEX;
		else if (radeon_output->devices & ATOM_DEVICE_DFP3_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_DFP3_INDEX;
		else if (radeon_output->devices & ATOM_DEVICE_DFP4_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_DFP4_INDEX;
		else if (radeon_output->devices & ATOM_DEVICE_DFP5_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_DFP5_INDEX;
	    } else if (radeon_output->MonType == MT_LCD) {
		if (radeon_output->devices & ATOM_DEVICE_LCD1_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_LCD1_INDEX;
	    } else if (OUTPUT_IS_TV) {
		if (radeon_output->devices & ATOM_DEVICE_TV1_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_TV1_INDEX;
	    } else if (radeon_output->MonType == MT_CV) {
		if (radeon_output->devices & ATOM_DEVICE_CV_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_CV_INDEX;
	    }
	    data.exec.pspace = &crtc_src_param;
	    /*ErrorF("device sourced: 0x%x\n", crtc_src_param.ucDevice);*/
	    break;
	case 2:
	    crtc_src_param2.ucCRTC = radeon_crtc->crtc_id;
	    if (radeon_output->MonType == MT_CRT) {
		crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_CRT;
		if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT)
		    crtc_src_param2.ucEncoderID = ASIC_INT_DAC1_ENCODER_ID;
		else if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT)
		    crtc_src_param2.ucEncoderID = ASIC_INT_DAC2_ENCODER_ID;
	    } else if (radeon_output->MonType == MT_DFP) {
		if (IS_DCE3_VARIANT) {
		    /* we route digital encoders using the CRTC ids */
		    if (radeon_crtc->crtc_id)
			crtc_src_param2.ucEncoderID = ASIC_INT_DIG2_ENCODER_ID;
		    else
			crtc_src_param2.ucEncoderID = ASIC_INT_DIG1_ENCODER_ID;
		} else {
		    if (radeon_output->devices & ATOM_DEVICE_DFP1_SUPPORT)
			crtc_src_param2.ucEncoderID = ATOM_DEVICE_DFP1_INDEX;
		    else if (radeon_output->devices & ATOM_DEVICE_DFP2_SUPPORT)
			crtc_src_param2.ucEncoderID = ATOM_DEVICE_DFP2_INDEX;
		    else if (radeon_output->devices & ATOM_DEVICE_DFP3_SUPPORT)
			crtc_src_param2.ucEncoderID = ATOM_DEVICE_DFP3_INDEX;
		}
		if (OUTPUT_IS_DVI)
		    crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_DVI;
		else if (radeon_output->type == OUTPUT_HDMI)
		    crtc_src_param2.ucEncodeMode =
			atombios_maybe_hdmi_mode(output);
		else if (radeon_output->type == OUTPUT_DP)
		    crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_DP;
	    } else if (radeon_output->MonType == MT_LCD) {
		if (radeon_output->devices & ATOM_DEVICE_LCD1_SUPPORT)
		    crtc_src_param2.ucEncoderID = ATOM_DEVICE_LCD1_INDEX;
		crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_LVDS;
	    } else if (OUTPUT_IS_TV) {
		if (radeon_output->devices & ATOM_DEVICE_TV1_SUPPORT)
		    crtc_src_param2.ucEncoderID = ASIC_INT_TV_ENCODER_ID;
		crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_TV;
	    } else if (radeon_output->MonType == MT_CV) {
		if (radeon_output->devices & ATOM_DEVICE_CV_SUPPORT)
		    crtc_src_param2.ucEncoderID = ASIC_INT_TV_ENCODER_ID;
		crtc_src_param2.ucEncodeMode = ATOM_ENCODER_MODE_CV;
	    }

	    data.exec.pspace = &crtc_src_param2;
	    /*ErrorF("device sourced: 0x%x\n", crtc_src_param2.ucEncoderID);*/
	    break;
	}
	break;
    default:
	ErrorF("Unknown table version\n");
	exit(-1);
    }

    data.exec.index = index;
    data.exec.dataSpace = (void *)&space;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Set CRTC %d Source success\n", radeon_crtc->crtc_id);
	return;
    }

    ErrorF("Set CRTC Source failed\n");
    return;
}

static void
atombios_apply_output_quirks(xf86OutputPtr output)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    unsigned char *RADEONMMIO = info->MMIO;

    /* Funky macbooks */
    if ((info->Chipset == PCI_CHIP_RV530_71C5) &&
	(PCI_SUB_VENDOR_ID(info->PciInfo) == 0x106b) &&
	(PCI_SUB_DEVICE_ID(info->PciInfo) == 0x0080)) {
	if (radeon_output->MonType == MT_LCD) {
	    if (radeon_output->devices & ATOM_DEVICE_LCD1_SUPPORT) {
		uint32_t lvtma_bit_depth_control = INREG(AVIVO_LVTMA_BIT_DEPTH_CONTROL);

		lvtma_bit_depth_control &= ~AVIVO_LVTMA_BIT_DEPTH_CONTROL_TRUNCATE_EN;
		lvtma_bit_depth_control &= ~AVIVO_LVTMA_BIT_DEPTH_CONTROL_SPATIAL_DITHER_EN;

		OUTREG(AVIVO_LVTMA_BIT_DEPTH_CONTROL, lvtma_bit_depth_control);
	    }
	}
    }
}

void
atombios_output_mode_set(xf86OutputPtr output,
			 DisplayModePtr mode,
			 DisplayModePtr adjusted_mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);

    atombios_output_scaler_setup(output, mode);
    atombios_set_output_crtc_source(output);

    if (radeon_output->MonType == MT_CRT) {
       if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT ||
	   radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT) {
	   if (radeon_output->DACType == DAC_PRIMARY)
	       atombios_output_dac1_setup(output, adjusted_mode);
	   else if (radeon_output->DACType == DAC_TVDAC)
	       atombios_output_dac2_setup(output, adjusted_mode);
       }
    } else if (radeon_output->MonType == MT_DFP) {
	if (radeon_output->devices & ATOM_DEVICE_DFP1_SUPPORT) {
	    if (IS_DCE3_VARIANT) {
		atombios_output_dig_encoder_setup(output, ATOM_DEVICE_DFP1_INDEX, adjusted_mode);
		atombios_output_dig_transmitter_setup(output, ATOM_DEVICE_DFP1_INDEX, adjusted_mode);
	    } else
		atombios_output_digital_setup(output, ATOM_DEVICE_DFP1_INDEX, adjusted_mode);
	} else if (radeon_output->devices & ATOM_DEVICE_DFP2_SUPPORT) {
	    if (IS_DCE32_VARIANT) {
		atombios_output_dig_encoder_setup(output, ATOM_DEVICE_DFP2_INDEX, adjusted_mode);
		atombios_output_dig_transmitter_setup(output, ATOM_DEVICE_DFP2_INDEX, adjusted_mode);
	    } else {
		if ((info->ChipFamily == CHIP_FAMILY_RS600) ||
		    (info->ChipFamily == CHIP_FAMILY_RS690) ||
		    (info->ChipFamily == CHIP_FAMILY_RS740))
		    atombios_output_ddia_setup(output, adjusted_mode);
		else
		    atombios_external_tmds_setup(output, adjusted_mode);
	    }
	} else if (radeon_output->devices & ATOM_DEVICE_DFP3_SUPPORT) {
	    if (IS_DCE3_VARIANT) {
		atombios_output_dig_encoder_setup(output, ATOM_DEVICE_DFP3_INDEX, adjusted_mode);
		atombios_output_dig_transmitter_setup(output, ATOM_DEVICE_DFP3_INDEX, adjusted_mode);
	    } else
		atombios_output_digital_setup(output, ATOM_DEVICE_DFP3_INDEX, adjusted_mode);
	} else if (radeon_output->devices & ATOM_DEVICE_DFP4_SUPPORT) {
	    atombios_output_dig_encoder_setup(output, ATOM_DEVICE_DFP4_INDEX, adjusted_mode);
	    atombios_output_dig_transmitter_setup(output, ATOM_DEVICE_DFP4_INDEX, adjusted_mode);
	} else if (radeon_output->devices & ATOM_DEVICE_DFP5_SUPPORT) {
	    atombios_output_dig_encoder_setup(output, ATOM_DEVICE_DFP5_INDEX, adjusted_mode);
	    atombios_output_dig_transmitter_setup(output, ATOM_DEVICE_DFP5_INDEX, adjusted_mode);
	}
    } else if (radeon_output->MonType == MT_LCD) {
	if (radeon_output->devices & ATOM_DEVICE_LCD1_SUPPORT) {
	    if (IS_DCE3_VARIANT) {
		atombios_output_dig_encoder_setup(output, ATOM_DEVICE_LCD1_INDEX, adjusted_mode);
		atombios_output_dig_transmitter_setup(output, ATOM_DEVICE_LCD1_INDEX, adjusted_mode);
	    } else
		atombios_output_digital_setup(output, ATOM_DEVICE_LCD1_INDEX, adjusted_mode);
	}
    } else if ((radeon_output->MonType == MT_CTV) ||
	       (radeon_output->MonType == MT_STV) ||
	       (radeon_output->MonType == MT_CV)) {
	if (radeon_output->DACType == DAC_PRIMARY)
	    atombios_output_dac1_setup(output, adjusted_mode);
	else if (radeon_output->DACType == DAC_TVDAC)
	    atombios_output_dac2_setup(output, adjusted_mode);
	atombios_output_tv1_setup(output, adjusted_mode);
    }
    atombios_apply_output_quirks(output);
}

static AtomBiosResult
atom_bios_dac_load_detect(atomBiosHandlePtr atomBIOS, xf86OutputPtr output)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DAC_LOAD_DETECTION_PS_ALLOCATION dac_data;
    AtomBiosArgRec data;
    unsigned char *space;

    dac_data.sDacload.ucMisc = 0;

    if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT) {
	dac_data.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_CRT1_SUPPORT);
	if (radeon_output->DACType == DAC_PRIMARY)
	    dac_data.sDacload.ucDacType = ATOM_DAC_A;
	else if (radeon_output->DACType == DAC_TVDAC)
	    dac_data.sDacload.ucDacType = ATOM_DAC_B;
    } else if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT) {
	dac_data.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_CRT2_SUPPORT);
	if (radeon_output->DACType == DAC_PRIMARY)
	    dac_data.sDacload.ucDacType = ATOM_DAC_A;
	else if (radeon_output->DACType == DAC_TVDAC)
	    dac_data.sDacload.ucDacType = ATOM_DAC_B;
    } else if (radeon_output->devices & ATOM_DEVICE_CV_SUPPORT) {
	dac_data.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_CV_SUPPORT);
	if (radeon_output->DACType == DAC_PRIMARY)
	    dac_data.sDacload.ucDacType = ATOM_DAC_A;
	else if (radeon_output->DACType == DAC_TVDAC)
	    dac_data.sDacload.ucDacType = ATOM_DAC_B;
	if (IS_DCE3_VARIANT)
	    dac_data.sDacload.ucMisc = 1;
    } else if (radeon_output->devices & ATOM_DEVICE_TV1_SUPPORT) {
	dac_data.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_TV1_SUPPORT);
	if (radeon_output->DACType == DAC_PRIMARY)
	    dac_data.sDacload.ucDacType = ATOM_DAC_A;
	else if (radeon_output->DACType == DAC_TVDAC)
	    dac_data.sDacload.ucDacType = ATOM_DAC_B;
	if (IS_DCE3_VARIANT)
	    dac_data.sDacload.ucMisc = 1;
    } else {
	ErrorF("invalid output device for dac detection\n");
	return ATOM_NOT_IMPLEMENTED;
    }


    data.exec.index = GetIndexIntoMasterTable(COMMAND, DAC_LoadDetection);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &dac_data;

    if (RHDAtomBiosFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {

	ErrorF("Dac detection success\n");
	return ATOM_SUCCESS ;
    }

    ErrorF("DAC detection failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

RADEONMonitorType
atombios_dac_detect(ScrnInfoPtr pScrn, xf86OutputPtr output)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONMonitorType MonType = MT_NONE;
    AtomBiosResult ret;
    uint32_t bios_0_scratch;

    if (OUTPUT_IS_TV) {
	if (xf86ReturnOptValBool(info->Options, OPTION_FORCE_TVOUT, FALSE)) {
	    if (radeon_output->type == OUTPUT_STV)
		return MT_STV;
	    else
		return MT_CTV;
	}
    }

    ret = atom_bios_dac_load_detect(info->atomBIOS, output);
    if (ret == ATOM_SUCCESS) {
	if (info->ChipFamily >= CHIP_FAMILY_R600)
	    bios_0_scratch = INREG(R600_BIOS_0_SCRATCH);
	else
	    bios_0_scratch = INREG(RADEON_BIOS_0_SCRATCH);
	/*ErrorF("DAC connect %08X\n", (unsigned int)bios_0_scratch);*/

	if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT) {
	    if (bios_0_scratch & ATOM_S0_CRT1_MASK)
		MonType = MT_CRT;
	} else if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT) {
	    if (bios_0_scratch & ATOM_S0_CRT2_MASK)
		MonType = MT_CRT;
	} else if (radeon_output->devices & ATOM_DEVICE_CV_SUPPORT) {
	    if (bios_0_scratch & (ATOM_S0_CV_MASK | ATOM_S0_CV_MASK_A))
		MonType = MT_CV;
	} else if (radeon_output->devices & ATOM_DEVICE_TV1_SUPPORT) {
	    if (bios_0_scratch & (ATOM_S0_TV1_COMPOSITE | ATOM_S0_TV1_COMPOSITE_A))
		MonType = MT_CTV;
	    else if (bios_0_scratch & (ATOM_S0_TV1_SVIDEO | ATOM_S0_TV1_SVIDEO_A))
		MonType = MT_STV;
	}
    }

    return MonType;
}

