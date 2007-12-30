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
	case TV_STD_NTSC:
	case TV_STD_NTSC_J:
	case TV_STD_PAL_60:
	    disp_data.ucDacStandard = ATOM_DAC1_NTSC;
	    break;
	case TV_STD_PAL:
	case TV_STD_PAL_M:
	case TV_STD_SCART_PAL:
	case TV_STD_SECAM:
	case TV_STD_PAL_CN:
	    disp_data.ucDacStandard = ATOM_DAC1_PAL;
	    break;
	default:
	    disp_data.ucDacStandard = ATOM_DAC1_NTSC;
	    break;
	}
    }

    disp_data.usPixelClock = mode->Clock / 10;
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

    disp_data.usPixelClock = mode->Clock / 10;
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

    disp_data.sTVEncoder.usPixelClock = mode->Clock / 10;
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
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.sXTmdsEncoder.ucEnable = 1;

    if (mode->Clock > 165000)
	disp_data.sXTmdsEncoder.ucMisc = 1;
    else
	disp_data.sXTmdsEncoder.ucMisc = 0;

    if (!info->dac6bits)
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
atombios_output_tmds1_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    TMDS1_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.ucAction = 1;
    if (mode->Clock > 165000)
	disp_data.ucMisc = 1;
    else
	disp_data.ucMisc = 0;
    disp_data.usPixelClock = mode->Clock / 10;
    data.exec.index = GetIndexIntoMasterTable(COMMAND, TMDS1EncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;
    
    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output TMDS1 setup success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output TMDS1 setup failed\n");
    return ATOM_NOT_IMPLEMENTED;

}

static int
atombios_output_tmds2_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    TMDS2_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.ucAction = 1;
    if (mode->Clock > 165000)
	disp_data.ucMisc = 1;
    else
	disp_data.ucMisc = 0;
    disp_data.usPixelClock = mode->Clock / 10;
    data.exec.index = GetIndexIntoMasterTable(COMMAND, TMDS2EncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;
    
    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output TMDS2 setup success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output TMDS2 setup failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

static int
atombios_output_lvds_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    LVDS_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.ucAction = 1;
    if (mode->Clock > 165000)
	disp_data.ucMisc = 1;
    else
	disp_data.ucMisc = 0;
    disp_data.usPixelClock = mode->Clock / 10;
    data.exec.index = GetIndexIntoMasterTable(COMMAND, LVDSEncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;
    
    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output LVDS setup success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output LVDS setup failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

static AtomBiosResult
atombios_display_device_control(atomBiosHandlePtr atomBIOS, int device, Bool state)
{
    DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.ucAction = state;
    data.exec.index = device;
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;
    
    if (RHDAtomBiosFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output %d %s success\n", device, state? "enable":"disable");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output %d %s failed\n", device, state? "enable":"disable");
    return ATOM_NOT_IMPLEMENTED;
}

static void
atombios_device_dpms(xf86OutputPtr output, int device, int mode)
{
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    int index;

    switch (device) {
    case ATOM_DEVICE_CRT1_SUPPORT:
	index = GetIndexIntoMasterTable(COMMAND, DAC1OutputControl);
	break;
    case ATOM_DEVICE_CRT2_SUPPORT:
	index = GetIndexIntoMasterTable(COMMAND, DAC2OutputControl);
	break;
    case ATOM_DEVICE_DFP1_SUPPORT:
	index = GetIndexIntoMasterTable(COMMAND, TMDSAOutputControl);
	break;
    case ATOM_DEVICE_DFP2_SUPPORT:
	index = GetIndexIntoMasterTable(COMMAND, DVOOutputControl);
	break;
    case ATOM_DEVICE_DFP3_SUPPORT:
	index = GetIndexIntoMasterTable(COMMAND, LVTMAOutputControl);
	break;
    case ATOM_DEVICE_LCD1_SUPPORT:
	index = GetIndexIntoMasterTable(COMMAND, LCD1OutputControl);
	break;
    case ATOM_DEVICE_TV1_SUPPORT:
	index = GetIndexIntoMasterTable(COMMAND, TV1OutputControl);
	break;
    case ATOM_DEVICE_CV_SUPPORT:
	index = GetIndexIntoMasterTable(COMMAND, CV1OutputControl);
	break;
    default:
	return;
    }

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
    unsigned char *RADEONMMIO = info->MMIO;
    int tmp, count;

#if 1
    /* try to grab card lock or at least somethings that looks like a lock
     * if it fails more than 5 times with 1000ms wait btw each try than we
     * assume we can process.
     */
    count = 0;
    tmp = INREG(0x0028);
    while((tmp & 0x100) && (count < 5)) {
        tmp = INREG(0x0028);
        count++;
        usleep(1000);
    }
    if (count >= 5) {
        xf86DrvMsg(output->scrn->scrnIndex, X_INFO,
                   "%s (WARNING) failed to grab card lock process anyway.\n",
                   __func__);
    }
    OUTREG(0x0028, tmp | 0x100);
#endif

    ErrorF("AGD: output dpms %d\n", mode);

   if (radeon_output->MonType == MT_LCD) {
       if (radeon_output->devices & ATOM_DEVICE_LCD1_SUPPORT)
	   atombios_device_dpms(output, ATOM_DEVICE_LCD1_SUPPORT, mode);
   } else if (radeon_output->MonType == MT_DFP) {
       ErrorF("AGD: tmds dpms\n");
       if (radeon_output->devices & ATOM_DEVICE_DFP1_SUPPORT)
	   atombios_device_dpms(output, ATOM_DEVICE_DFP1_SUPPORT, mode);
       else if (radeon_output->devices & ATOM_DEVICE_DFP2_SUPPORT)
	   atombios_device_dpms(output, ATOM_DEVICE_DFP2_SUPPORT, mode);
       else if (radeon_output->devices & ATOM_DEVICE_DFP3_SUPPORT)
	   atombios_device_dpms(output, ATOM_DEVICE_DFP3_SUPPORT, mode);
   } else if (radeon_output->MonType == MT_CRT) {
       ErrorF("AGD: dac dpms\n");
       if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT)
	   atombios_device_dpms(output, ATOM_DEVICE_CRT1_SUPPORT, mode);
       else if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT)
	   atombios_device_dpms(output, ATOM_DEVICE_CRT2_SUPPORT, mode);
   }

#if 1
    /* release card lock */
    tmp = INREG(0x0028);
    OUTREG(0x0028, tmp & (~0x100));
#endif
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
    int index = GetIndexIntoMasterTable(COMMAND, SelectCRTC_Source);
    int major, minor;
    
    atombios_get_command_table_version(info->atomBIOS, index, &major, &minor);
    
    ErrorF("select crtc source table is %d %d\n", major, minor);

    crtc_src_param.ucCRTC = radeon_crtc->crtc_id;
    crtc_src_param.ucDevice = 0;

    switch(major) {
    case 1: {
	switch(minor) {
	case 0:
	case 1:
	default:
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
	    } else if (radeon_output->MonType == MT_LCD) {
		if (radeon_output->devices & ATOM_DEVICE_LCD1_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_LCD1_INDEX;
	    } else if (OUTPUT_IS_TV || (radeon_output->MonType == MT_CV)) {
		if (radeon_output->devices & ATOM_DEVICE_TV1_SUPPORT)
		    crtc_src_param.ucDevice = ATOM_DEVICE_TV1_INDEX;
	    }
	    break;
	}
	break;
    }
    default:
	break;
    }

    ErrorF("device sourced: 0x%x\n", crtc_src_param.ucDevice);

    data.exec.index = index;
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &crtc_src_param;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Set CRTC %d Source success\n", radeon_crtc->crtc_id);
	return;
    }

    ErrorF("Set CRTC Source failed\n");
    return;
}

void
atombios_output_mode_set(xf86OutputPtr output,
			 DisplayModePtr mode,
			 DisplayModePtr adjusted_mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    atombios_set_output_crtc_source(output);

    if (radeon_output->MonType == MT_CRT) {
       if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT)
	   atombios_output_dac1_setup(output, adjusted_mode);
       else if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT)
	   atombios_output_dac2_setup(output, adjusted_mode);
    } else if (radeon_output->MonType == MT_DFP) {
       if (radeon_output->devices & ATOM_DEVICE_DFP1_SUPPORT)
	   atombios_output_tmds1_setup(output, adjusted_mode);
       else if (radeon_output->devices & ATOM_DEVICE_DFP2_SUPPORT)
	   atombios_external_tmds_setup(output, adjusted_mode);
       else if (radeon_output->devices & ATOM_DEVICE_DFP3_SUPPORT)
	   atombios_output_tmds2_setup(output, adjusted_mode);
    } else if (radeon_output->MonType == MT_LCD) {
       if (radeon_output->devices & ATOM_DEVICE_LCD1_SUPPORT)
	   atombios_output_lvds_setup(output, adjusted_mode);
    } else if (OUTPUT_IS_TV || (radeon_output->MonType == MT_CV)) {
	atombios_output_dac2_setup(output, adjusted_mode);
	atombios_output_tv1_setup(output, adjusted_mode);
    }

}

static AtomBiosResult
atom_bios_dac_load_detect(atomBiosHandlePtr atomBIOS, xf86OutputPtr output)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    DAC_LOAD_DETECTION_PS_ALLOCATION dac_data;
    AtomBiosArgRec data;
    unsigned char *space;

    if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT) {
	dac_data.sDacload.usDeviceID = ATOM_DEVICE_CRT1_SUPPORT;
	dac_data.sDacload.ucDacType = ATOM_DAC_A;
    } else if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT) {
	dac_data.sDacload.usDeviceID = ATOM_DEVICE_CRT2_SUPPORT;
	dac_data.sDacload.ucDacType = ATOM_DAC_B;
    } else if (radeon_output->devices & ATOM_DEVICE_CV_SUPPORT) {
	dac_data.sDacload.usDeviceID = ATOM_DEVICE_CV_SUPPORT;
	dac_data.sDacload.ucDacType = ATOM_DAC_B;
    } else if (radeon_output->devices & ATOM_DEVICE_TV1_SUPPORT) {
	dac_data.sDacload.usDeviceID = ATOM_DEVICE_TV1_SUPPORT;
	dac_data.sDacload.ucDacType = ATOM_DAC_B;
    } else {
	ErrorF("invalid output device for dac detection\n");
	return ATOM_NOT_IMPLEMENTED;
    }

    dac_data.sDacload.ucMisc = 0;

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

    ret = atom_bios_dac_load_detect(info->atomBIOS, output);
    if (ret == ATOM_SUCCESS) {
	bios_0_scratch = INREG(RADEON_BIOS_0_SCRATCH);
	ErrorF("DAC connect %08X\n", (unsigned int)bios_0_scratch);

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

