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
atombios_output_dac_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    radeon_encoder_ptr radeon_encoder = radeon_get_encoder(output);
    DAC_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;
    int index, num = 0;

    if (radeon_encoder == NULL)
	return ATOM_NOT_IMPLEMENTED;

    memset(&disp_data,0, sizeof(disp_data));

    switch (radeon_encoder->encoder_id) {
    case ENCODER_OBJECT_ID_INTERNAL_DAC1:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DAC1:
	index = GetIndexIntoMasterTable(COMMAND, DAC1EncoderControl);
	num = 1;
	break;
    case ENCODER_OBJECT_ID_INTERNAL_DAC2:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DAC2:
	index = GetIndexIntoMasterTable(COMMAND, DAC2EncoderControl);
	num = 2;
	break;
    }

    disp_data.ucAction = ATOM_ENABLE;

    if (radeon_output->active_device & (ATOM_DEVICE_CRT_SUPPORT))
	disp_data.ucDacStandard = ATOM_DAC1_PS2;
    else if (radeon_output->active_device & (ATOM_DEVICE_CV_SUPPORT))
	disp_data.ucDacStandard = ATOM_DAC1_CV;
    else {
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

    data.exec.index = index;
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output DAC%d setup success\n", num);
	return ATOM_SUCCESS;
    }

    ErrorF("Output DAC%d setup failed\n", num);
    return ATOM_NOT_IMPLEMENTED;

}

static int
atombios_output_tv_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    TV_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    memset(&disp_data,0, sizeof(disp_data));

    disp_data.sTVEncoder.ucAction = ATOM_ENABLE;

    if (radeon_output->active_device & (ATOM_DEVICE_CV_SUPPORT))
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
	ErrorF("Output TV setup success\n");
	return ATOM_SUCCESS;
    }

    ErrorF("Output TV setup failed\n");
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

    memset(&disp_data,0, sizeof(disp_data));

    disp_data.sXTmdsEncoder.ucEnable = ATOM_ENABLE;

    if (mode->Clock > 165000)
	disp_data.sXTmdsEncoder.ucMisc = PANEL_ENCODER_MISC_DUAL;

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

    memset(&disp_data,0, sizeof(disp_data));

    disp_data.sDVOEncoder.ucAction = ATOM_ENABLE;
    disp_data.sDVOEncoder.usPixelClock = cpu_to_le16(mode->Clock / 10);

    if (mode->Clock > 165000)
	disp_data.sDVOEncoder.usDevAttr.sDigAttrib.ucAttribute = PANEL_ENCODER_MISC_DUAL;

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
atombios_output_digital_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    radeon_encoder_ptr radeon_encoder = radeon_get_encoder(output);
    LVDS_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    LVDS_ENCODER_CONTROL_PS_ALLOCATION_V2 disp_data2;
    AtomBiosArgRec data;
    unsigned char *space;
    int index;
    int major, minor;

    if (radeon_encoder == NULL)
	return ATOM_NOT_IMPLEMENTED;

    memset(&disp_data,0, sizeof(disp_data));
    memset(&disp_data2,0, sizeof(disp_data2));

    switch (radeon_encoder->encoder_id) {
    case ENCODER_OBJECT_ID_INTERNAL_LVDS:
	index = GetIndexIntoMasterTable(COMMAND, LVDSEncoderControl);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_TMDS1:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_TMDS1:
	index = GetIndexIntoMasterTable(COMMAND, TMDS1EncoderControl);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_LVTM1:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_LVTMA:
	if (radeon_output->active_device & (ATOM_DEVICE_LCD_SUPPORT))
	    index = GetIndexIntoMasterTable(COMMAND, LVDSEncoderControl);
	else
	    index = GetIndexIntoMasterTable(COMMAND, TMDS2EncoderControl);
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
	    if ((radeon_output->ConnectorType == CONNECTOR_HDMI_TYPE_A) ||
		(radeon_output->ConnectorType == CONNECTOR_HDMI_TYPE_B))
		disp_data.ucMisc |= PANEL_ENCODER_MISC_HDMI_TYPE;
	    disp_data.usPixelClock = cpu_to_le16(mode->Clock / 10);
	    if (radeon_output->active_device & (ATOM_DEVICE_LCD_SUPPORT)) {
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
	    if ((radeon_output->ConnectorType == CONNECTOR_HDMI_TYPE_A) ||
		(radeon_output->ConnectorType == CONNECTOR_HDMI_TYPE_B))
		disp_data2.ucMisc |= PANEL_ENCODER_MISC_HDMI_TYPE;
	    disp_data2.usPixelClock = cpu_to_le16(mode->Clock / 10);
	    disp_data2.ucTruncate = 0;
	    disp_data2.ucSpatial = 0;
	    disp_data2.ucTemporal = 0;
	    disp_data2.ucFRC = 0;
	    if (radeon_output->active_device & (ATOM_DEVICE_LCD_SUPPORT)) {
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

int
atombios_get_encoder_mode(xf86OutputPtr output)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    /* DVI should really be atombios_maybe_hdmi_mode() as well */
    switch (radeon_output->ConnectorType) {
    case CONNECTOR_DVI_I:
	if (radeon_output->active_device & (ATOM_DEVICE_DFP_SUPPORT))
	    return ATOM_ENCODER_MODE_DVI;
	else
	    return ATOM_ENCODER_MODE_CRT;
	break;
    case CONNECTOR_DVI_D:
    default:
	return ATOM_ENCODER_MODE_DVI;
	break;
    case CONNECTOR_HDMI_TYPE_A:
    case CONNECTOR_HDMI_TYPE_B:
	return atombios_maybe_hdmi_mode(output);
	break;
    case CONNECTOR_LVDS:
	return ATOM_ENCODER_MODE_LVDS;
	break;
    case CONNECTOR_DISPLAY_PORT:
	return ATOM_ENCODER_MODE_DP;
	break;
    case CONNECTOR_DVI_A:
    case CONNECTOR_VGA:
    case CONNECTOR_STV:
    case CONNECTOR_CTV:
    case CONNECTOR_DIN:
	if (radeon_output->active_device & (ATOM_DEVICE_TV_SUPPORT))
	    return ATOM_ENCODER_MODE_TV;
	else if (radeon_output->active_device & (ATOM_DEVICE_CV_SUPPORT))
	    return ATOM_ENCODER_MODE_CV;
	else
	    return ATOM_ENCODER_MODE_CRT;
	break;
    }

}

static int
atombios_output_dig_encoder_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONCrtcPrivatePtr radeon_crtc = output->crtc->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    radeon_encoder_ptr radeon_encoder = radeon_get_encoder(output);
    DIG_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;
    int index, major, minor, num = 0;

    if (radeon_encoder == NULL)
	return ATOM_NOT_IMPLEMENTED;

    memset(&disp_data,0, sizeof(disp_data));

    if (IS_DCE32_VARIANT) {
	if (radeon_crtc->crtc_id)
	    index = GetIndexIntoMasterTable(COMMAND, DIG2EncoderControl);
	else
	    index = GetIndexIntoMasterTable(COMMAND, DIG1EncoderControl);
	num = radeon_crtc->crtc_id + 1;
    } else {
	switch (radeon_encoder->encoder_id) {
	case ENCODER_OBJECT_ID_INTERNAL_TMDS1:
	case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_TMDS1:
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:
	    index = GetIndexIntoMasterTable(COMMAND, DIG1EncoderControl);
	    num = 1;
	    break;
	case ENCODER_OBJECT_ID_INTERNAL_LVDS:
	case ENCODER_OBJECT_ID_INTERNAL_LVTM1:
	case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_LVTMA:
	    index = GetIndexIntoMasterTable(COMMAND, DIG2EncoderControl);
	    num = 2;
	    break;
	}
    }

    atombios_get_command_table_version(info->atomBIOS, index, &major, &minor);

    disp_data.ucAction = ATOM_ENABLE;
    disp_data.usPixelClock = cpu_to_le16(mode->Clock / 10);

    if (IS_DCE32_VARIANT) {
	switch (radeon_encoder->encoder_id) {
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:
	    disp_data.ucConfig = ATOM_ENCODER_CONFIG_V2_TRANSMITTER1;
	    break;
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY1:
	    disp_data.ucConfig = ATOM_ENCODER_CONFIG_V2_TRANSMITTER2;
	    break;
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY2:
	    disp_data.ucConfig = ATOM_ENCODER_CONFIG_V2_TRANSMITTER3;
	    break;
	}
    } else {
	switch (radeon_encoder->encoder_id) {
	case ENCODER_OBJECT_ID_INTERNAL_TMDS1:
	case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_TMDS1:
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:
	    disp_data.ucConfig = ATOM_ENCODER_CONFIG_TRANSMITTER1;
	    break;
	case ENCODER_OBJECT_ID_INTERNAL_LVDS:
	case ENCODER_OBJECT_ID_INTERNAL_LVTM1:
	case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_LVTMA:
	    disp_data.ucConfig = ATOM_ENCODER_CONFIG_TRANSMITTER2;
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

    disp_data.ucEncoderMode = atombios_get_encoder_mode(output);

    data.exec.index = index;
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output DIG%d encoder setup success\n", num);
	return ATOM_SUCCESS;
    }

    ErrorF("Output DIG%d setup failed\n", num);
    return ATOM_NOT_IMPLEMENTED;

}

union dig_transmitter_control {
    DIG_TRANSMITTER_CONTROL_PS_ALLOCATION v1;
    DIG_TRANSMITTER_CONTROL_PARAMETERS_V2 v2;
};

static int
atombios_output_dig_transmitter_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONCrtcPrivatePtr radeon_crtc = output->crtc->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    radeon_encoder_ptr radeon_encoder = radeon_get_encoder(output);
    union dig_transmitter_control disp_data;
    AtomBiosArgRec data;
    unsigned char *space;
    int index, num = 0;
    int major, minor;

    if (radeon_encoder == NULL)
        return ATOM_NOT_IMPLEMENTED;

    memset(&disp_data,0, sizeof(disp_data));

    if (IS_DCE32_VARIANT)
	index = GetIndexIntoMasterTable(COMMAND, UNIPHYTransmitterControl);
    else {
	switch (radeon_encoder->encoder_id) {
	case ENCODER_OBJECT_ID_INTERNAL_TMDS1:
	case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_TMDS1:
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:
	    index = GetIndexIntoMasterTable(COMMAND, DIG1TransmitterControl);
	    break;
	case ENCODER_OBJECT_ID_INTERNAL_LVDS:
	case ENCODER_OBJECT_ID_INTERNAL_LVTM1:
	case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_LVTMA:
	    index = GetIndexIntoMasterTable(COMMAND, DIG2TransmitterControl);
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

	switch (radeon_encoder->encoder_id) {
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:
	    disp_data.v2.acConfig.ucTransmitterSel = 0;
	    num = 0;
	    break;
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY1:
	    disp_data.v2.acConfig.ucTransmitterSel = 1;
	    num = 1;
	    break;
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY2:
	    disp_data.v2.acConfig.ucTransmitterSel = 2;
	    num = 2;
	    break;
	}

	if (radeon_output->active_device & (ATOM_DEVICE_DFP_SUPPORT)) {
	    if (radeon_output->coherent_mode) {
		disp_data.v2.acConfig.fCoherentMode = 1;
		xf86DrvMsg(output->scrn->scrnIndex, X_INFO, "UNIPHY%d transmitter: Coherent Mode enabled\n",disp_data.v2.acConfig.ucTransmitterSel);
	    } else
		xf86DrvMsg(output->scrn->scrnIndex, X_INFO, "UNIPHY%d transmitter: Coherent Mode disabled\n",disp_data.v2.acConfig.ucTransmitterSel);
	}
    } else {
	disp_data.v1.ucConfig = ATOM_TRANSMITTER_CONFIG_CLKSRC_PPLL;
	disp_data.v1.usPixelClock = cpu_to_le16((mode->Clock) / 10);

	switch (radeon_encoder->encoder_id) {
	case ENCODER_OBJECT_ID_INTERNAL_TMDS1:
	case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_TMDS1:
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:
	    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_DIG1_ENCODER;
	    if (info->IsIGP) {
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
		else {
		    /* XXX */
		    if (radeon_output->linkb)
			disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LINKB | ATOM_TRANSMITTER_CONFIG_LANE_0_3;
		    else
			disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LINKA | ATOM_TRANSMITTER_CONFIG_LANE_0_3;
		}
	    }
	    break;
	case ENCODER_OBJECT_ID_INTERNAL_LVDS:
	case ENCODER_OBJECT_ID_INTERNAL_LVTM1:
	case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_LVTMA:
	    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_DIG2_ENCODER;
	    if (mode->Clock > 165000)
		disp_data.v1.ucConfig |= (ATOM_TRANSMITTER_CONFIG_8LANE_LINK |
					  ATOM_TRANSMITTER_CONFIG_LINKA_B |
					  ATOM_TRANSMITTER_CONFIG_LANE_0_7);
	    else {
		/* XXX */
		if (radeon_output->linkb)
		    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LINKB | ATOM_TRANSMITTER_CONFIG_LANE_0_3;
		else
		    disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_LINKA | ATOM_TRANSMITTER_CONFIG_LANE_0_3;
	    }
	    break;
	}

	if (radeon_output->active_device & (ATOM_DEVICE_DFP_SUPPORT)) {
	    if (radeon_output->coherent_mode) {
		disp_data.v1.ucConfig |= ATOM_TRANSMITTER_CONFIG_COHERENT;
		xf86DrvMsg(output->scrn->scrnIndex, X_INFO, "DIG%d transmitter: Coherent Mode enabled\n", num);
	    } else
		xf86DrvMsg(output->scrn->scrnIndex, X_INFO, "DIG%d transmitter: Coherent Mode disabled\n", num);
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

    if (radeon_output->active_device & (ATOM_DEVICE_TV_SUPPORT)) {
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
    } else if (radeon_output->active_device & (ATOM_DEVICE_CV_SUPPORT)) {
	disp_data.ucTVStandard = ATOM_TV_CV;
	disp_data.ucEnable = SCALER_ENABLE_MULTITAP_MODE;
        ErrorF("Using CV scaler %x %x\n", disp_data.ucTVStandard, disp_data.ucEnable);
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

static int
atombios_dig_dpms(xf86OutputPtr output, int mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    radeon_encoder_ptr radeon_encoder = radeon_get_encoder(output);
    DIG_TRANSMITTER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    if (radeon_encoder == NULL)
	return ATOM_NOT_IMPLEMENTED;

    memset(&disp_data, 0, sizeof(disp_data));

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

    if (IS_DCE32_VARIANT)
	data.exec.index = GetIndexIntoMasterTable(COMMAND, UNIPHYTransmitterControl);
    else {
	switch (radeon_encoder->encoder_id) {
	case ENCODER_OBJECT_ID_INTERNAL_TMDS1:
	case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_TMDS1:
	case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:
	    data.exec.index = GetIndexIntoMasterTable(COMMAND, DIG1TransmitterControl);
	    break;
	case ENCODER_OBJECT_ID_INTERNAL_LVDS:
	case ENCODER_OBJECT_ID_INTERNAL_LVTM1:
	case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_LVTMA:
	    data.exec.index = GetIndexIntoMasterTable(COMMAND, DIG2TransmitterControl);
	    break;
	}
    }
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output DIG dpms success\n");
	return ATOM_SUCCESS;
    }

    ErrorF("Output DIG dpms failed\n");
    return ATOM_NOT_IMPLEMENTED;

}

void
atombios_output_dpms(xf86OutputPtr output, int mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    radeon_encoder_ptr radeon_encoder = radeon_get_encoder(output);
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;
    int index = 0;
    Bool is_dig = FALSE;

    if (radeon_encoder == NULL)
        return;

    switch (radeon_encoder->encoder_id) {
    case ENCODER_OBJECT_ID_INTERNAL_TMDS1:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_TMDS1:
	index = GetIndexIntoMasterTable(COMMAND, TMDSAOutputControl);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:
    case ENCODER_OBJECT_ID_INTERNAL_UNIPHY1:
    case ENCODER_OBJECT_ID_INTERNAL_UNIPHY2:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_LVTMA:
	is_dig = TRUE;
	break;
    case ENCODER_OBJECT_ID_INTERNAL_DVO1:
    case ENCODER_OBJECT_ID_INTERNAL_DDI:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DVO1:
	index = GetIndexIntoMasterTable(COMMAND, DVOOutputControl);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_LVDS:
	index = GetIndexIntoMasterTable(COMMAND, LCD1OutputControl);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_LVTM1:
	if (radeon_output->active_device & (ATOM_DEVICE_LCD_SUPPORT))
	    index = GetIndexIntoMasterTable(COMMAND, LCD1OutputControl);
	else
	    index = GetIndexIntoMasterTable(COMMAND, LVTMAOutputControl);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_DAC1:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DAC1:
	if (radeon_output->active_device & (ATOM_DEVICE_TV_SUPPORT))
	    index = GetIndexIntoMasterTable(COMMAND, TV1OutputControl);
	else if (radeon_output->active_device & (ATOM_DEVICE_CV_SUPPORT))
	    index = GetIndexIntoMasterTable(COMMAND, CV1OutputControl);
	else
	    index = GetIndexIntoMasterTable(COMMAND, DAC1OutputControl);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_DAC2:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DAC2:
	if (radeon_output->active_device & (ATOM_DEVICE_TV_SUPPORT))
	    index = GetIndexIntoMasterTable(COMMAND, TV1OutputControl);
	else if (radeon_output->active_device & (ATOM_DEVICE_CV_SUPPORT))
	    index = GetIndexIntoMasterTable(COMMAND, CV1OutputControl);
	else
	    index = GetIndexIntoMasterTable(COMMAND, DAC2OutputControl);
	break;
    }

    switch (mode) {
    case DPMSModeOn:
	if (is_dig)
	    (void)atombios_dig_dpms(output, mode);
	else {
	    disp_data.ucAction = ATOM_ENABLE;
	    data.exec.index = index;
	    data.exec.dataSpace = (void *)&space;
	    data.exec.pspace = &disp_data;

	    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS)
		ErrorF("Output %d enable success\n", index);
	    else
		ErrorF("Output %d enable failed\n", index);
	}
	radeon_encoder->use_count++;
	break;
    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
	if (radeon_encoder->use_count < 2) {
	    if (is_dig)
		(void)atombios_dig_dpms(output, mode);
	    else {
		disp_data.ucAction = ATOM_DISABLE;
		data.exec.index = index;
		data.exec.dataSpace = (void *)&space;
		data.exec.pspace = &disp_data;

		if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data)
		    == ATOM_SUCCESS)
		    ErrorF("Output %d disable success\n", index);
		else
		    ErrorF("Output %d disable failed\n", index);
	    }
	}
	if (radeon_encoder->use_count > 0)
	    radeon_encoder->use_count--;
	break;
    }
}

static void
atombios_set_output_crtc_source(xf86OutputPtr output)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONCrtcPrivatePtr radeon_crtc = output->crtc->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    radeon_encoder_ptr radeon_encoder = radeon_get_encoder(output);
    AtomBiosArgRec data;
    unsigned char *space;
    SELECT_CRTC_SOURCE_PS_ALLOCATION crtc_src_param;
    SELECT_CRTC_SOURCE_PARAMETERS_V2 crtc_src_param2;
    int index = GetIndexIntoMasterTable(COMMAND, SelectCRTC_Source);
    int major, minor;

    if (radeon_encoder == NULL)
	return;

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
	    crtc_src_param.ucDevice = radeon_get_device_index(radeon_output->active_device);
	    data.exec.pspace = &crtc_src_param;
	    /*ErrorF("device sourced: 0x%x\n", crtc_src_param.ucDevice);*/
	    break;
	case 2:
	    crtc_src_param2.ucCRTC = radeon_crtc->crtc_id;
	    crtc_src_param2.ucEncodeMode = atombios_get_encoder_mode(output);
	    switch (radeon_encoder->encoder_id) {
	    case ENCODER_OBJECT_ID_INTERNAL_TMDS1:
	    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_TMDS1:
		if (IS_DCE3_VARIANT)
		    crtc_src_param2.ucEncoderID = ASIC_INT_DIG2_ENCODER_ID;
		else
		    crtc_src_param2.ucEncoderID = radeon_get_device_index(radeon_output->active_device);
		break;
	    case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:
	    case ENCODER_OBJECT_ID_INTERNAL_UNIPHY1:
	    case ENCODER_OBJECT_ID_INTERNAL_UNIPHY2:
		if (IS_DCE32_VARIANT) {
		    if (radeon_crtc->crtc_id)
			crtc_src_param2.ucEncoderID = ASIC_INT_DIG2_ENCODER_ID;
		    else
			crtc_src_param2.ucEncoderID = ASIC_INT_DIG1_ENCODER_ID;
		} else
		    crtc_src_param2.ucEncoderID = ASIC_INT_DIG1_ENCODER_ID;
		break;
	    case ENCODER_OBJECT_ID_INTERNAL_DVO1:
	    case ENCODER_OBJECT_ID_INTERNAL_DDI:
	    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DVO1:
		crtc_src_param2.ucEncoderID = radeon_get_device_index(radeon_output->active_device);
		break;
	    case ENCODER_OBJECT_ID_INTERNAL_LVDS:
	    case ENCODER_OBJECT_ID_INTERNAL_LVTM1:
	    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_LVTMA:
		if (IS_DCE3_VARIANT)
		    crtc_src_param2.ucEncoderID = ASIC_INT_DIG2_ENCODER_ID;
		else
		    crtc_src_param2.ucEncoderID = radeon_get_device_index(radeon_output->active_device);
		break;
	    case ENCODER_OBJECT_ID_INTERNAL_DAC1:
	    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DAC1:
		if (radeon_output->active_device & (ATOM_DEVICE_TV_SUPPORT))
		    crtc_src_param2.ucEncoderID = ASIC_INT_TV_ENCODER_ID;
		else if (radeon_output->active_device & (ATOM_DEVICE_CV_SUPPORT))
		    crtc_src_param2.ucEncoderID = ASIC_INT_TV_ENCODER_ID;
		else
		    crtc_src_param2.ucEncoderID = ASIC_INT_DAC1_ENCODER_ID;
		break;
	    case ENCODER_OBJECT_ID_INTERNAL_DAC2:
	    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DAC2:
		if (radeon_output->active_device & (ATOM_DEVICE_TV_SUPPORT))
		    crtc_src_param2.ucEncoderID = ASIC_INT_TV_ENCODER_ID;
		else if (radeon_output->active_device & (ATOM_DEVICE_CV_SUPPORT))
		    crtc_src_param2.ucEncoderID = ASIC_INT_TV_ENCODER_ID;
		else
		    crtc_src_param2.ucEncoderID = ASIC_INT_DAC2_ENCODER_ID;
		break;
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
atombios_apply_output_quirks(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONCrtcPrivatePtr radeon_crtc = output->crtc->driver_private;
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

    /* set scaler clears this on some chips */
    if (mode->Flags & V_INTERLACE)
	OUTREG(AVIVO_D1MODE_DATA_FORMAT + radeon_crtc->crtc_offset, AVIVO_D1MODE_INTERLEAVE_EN);
}

void
atombios_output_mode_set(xf86OutputPtr output,
			 DisplayModePtr mode,
			 DisplayModePtr adjusted_mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    radeon_encoder_ptr radeon_encoder = radeon_get_encoder(output);

    if (radeon_encoder == NULL)
        return;

    atombios_output_scaler_setup(output, mode);
    atombios_set_output_crtc_source(output);

    switch (radeon_encoder->encoder_id) {
    case ENCODER_OBJECT_ID_INTERNAL_TMDS1:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_TMDS1:
    case ENCODER_OBJECT_ID_INTERNAL_LVDS:
    case ENCODER_OBJECT_ID_INTERNAL_LVTM1:
	atombios_output_digital_setup(output, adjusted_mode);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_UNIPHY:
    case ENCODER_OBJECT_ID_INTERNAL_UNIPHY1:
    case ENCODER_OBJECT_ID_INTERNAL_UNIPHY2:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_LVTMA:
	atombios_output_dig_encoder_setup(output, adjusted_mode);
	atombios_output_dig_transmitter_setup(output, adjusted_mode);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_DDI:
	atombios_output_ddia_setup(output, adjusted_mode);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_DVO1:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DVO1:
	atombios_external_tmds_setup(output, adjusted_mode);
	break;
    case ENCODER_OBJECT_ID_INTERNAL_DAC1:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DAC1:
    case ENCODER_OBJECT_ID_INTERNAL_DAC2:
    case ENCODER_OBJECT_ID_INTERNAL_KLDSCP_DAC2:
	atombios_output_dac_setup(output, adjusted_mode);
	if (radeon_output->active_device & (ATOM_DEVICE_TV_SUPPORT | ATOM_DEVICE_CV_SUPPORT))
	    atombios_output_tv_setup(output, adjusted_mode);
	break;
    }
    atombios_apply_output_quirks(output, adjusted_mode);
}

static AtomBiosResult
atom_bios_dac_load_detect(atomBiosHandlePtr atomBIOS, xf86OutputPtr output)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DAC_LOAD_DETECTION_PS_ALLOCATION dac_data;
    AtomBiosArgRec data;
    unsigned char *space;
    int major, minor;
    int index = GetIndexIntoMasterTable(COMMAND, DAC_LoadDetection);

    atombios_get_command_table_version(info->atomBIOS, index, &major, &minor);

    dac_data.sDacload.ucMisc = 0;

    if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT) {
	dac_data.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_CRT1_SUPPORT);
	if (info->encoders[ATOM_DEVICE_CRT1_INDEX] &&
	    (info->encoders[ATOM_DEVICE_CRT1_INDEX]->encoder_id == ENCODER_OBJECT_ID_INTERNAL_DAC1))
	    dac_data.sDacload.ucDacType = ATOM_DAC_A;
	else
	    dac_data.sDacload.ucDacType = ATOM_DAC_B;
    } else if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT) {
	dac_data.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_CRT2_SUPPORT);
	if (info->encoders[ATOM_DEVICE_CRT2_INDEX] &&
	    (info->encoders[ATOM_DEVICE_CRT2_INDEX]->encoder_id == ENCODER_OBJECT_ID_INTERNAL_DAC1))
	    dac_data.sDacload.ucDacType = ATOM_DAC_A;
	else
	    dac_data.sDacload.ucDacType = ATOM_DAC_B;
    } else if (radeon_output->devices & ATOM_DEVICE_CV_SUPPORT) {
	dac_data.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_CV_SUPPORT);
	if (info->encoders[ATOM_DEVICE_CV_INDEX] &&
	    (info->encoders[ATOM_DEVICE_CV_INDEX]->encoder_id == ENCODER_OBJECT_ID_INTERNAL_DAC1))
	    dac_data.sDacload.ucDacType = ATOM_DAC_A;
	else
	    dac_data.sDacload.ucDacType = ATOM_DAC_B;
	if (minor >= 3)
	    dac_data.sDacload.ucMisc = DAC_LOAD_MISC_YPrPb;
    } else if (radeon_output->devices & ATOM_DEVICE_TV1_SUPPORT) {
	dac_data.sDacload.usDeviceID = cpu_to_le16(ATOM_DEVICE_TV1_SUPPORT);
	if (info->encoders[ATOM_DEVICE_TV1_INDEX] &&
	    (info->encoders[ATOM_DEVICE_TV1_INDEX]->encoder_id == ENCODER_OBJECT_ID_INTERNAL_DAC1))
	    dac_data.sDacload.ucDacType = ATOM_DAC_A;
	else
	    dac_data.sDacload.ucDacType = ATOM_DAC_B;
    } else {
	ErrorF("invalid output device for dac detection\n");
	return ATOM_NOT_IMPLEMENTED;
    }


    data.exec.index = index;
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
atombios_dac_detect(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONMonitorType MonType = MT_NONE;
    AtomBiosResult ret;
    uint32_t bios_0_scratch;

    if (radeon_output->devices & ATOM_DEVICE_TV1_SUPPORT) {
	if (xf86ReturnOptValBool(info->Options, OPTION_FORCE_TVOUT, FALSE)) {
	    if (radeon_output->ConnectorType == CONNECTOR_STV)
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

