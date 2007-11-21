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
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DAC_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.ucAction = 1;
    disp_data.ucDacStandard = 1;
    disp_data.usPixelClock = mode->Clock / 10;
    data.exec.index = GetIndexIntoMasterTable(COMMAND, DAC1EncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;
    
    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output DAC1 enable success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output DAC1 enable failed\n");
    return ATOM_NOT_IMPLEMENTED;

}

static int
atombios_output_dac2_setup(xf86OutputPtr output, DisplayModePtr mode)
{
    RADEONInfoPtr info       = RADEONPTR(output->scrn);
    DAC_ENCODER_CONTROL_PS_ALLOCATION disp_data;
    AtomBiosArgRec data;
    unsigned char *space;

    disp_data.ucAction = 1;
    disp_data.ucDacStandard = 1;
    disp_data.usPixelClock = mode->Clock / 10;
    data.exec.index = GetIndexIntoMasterTable(COMMAND, DAC2EncoderControl);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &disp_data;
    
    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Output DAC2 enable success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output DAC2 enable failed\n");
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
	ErrorF("External TMDS enable success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("External TMDS enable failed\n");
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
	ErrorF("Output TMDS1 enable success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output TMDS1 enable failed\n");
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
	ErrorF("Output TMDS2 enable success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output TMDS2 enable failed\n");
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
	ErrorF("Output LVDS enable success\n");
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output LVDS enable failed\n");
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
	ErrorF("Output %d enable success\n", device);
	return ATOM_SUCCESS;
    }
    
    ErrorF("Output %d enable failed\n", device);
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

    ErrorF("AGD: output dpms\n");

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

void
atombios_output_mode_set(xf86OutputPtr output,
			 DisplayModePtr mode,
			 DisplayModePtr adjusted_mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

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
    }
}

static AtomBiosResult
atom_bios_dac_load_detect(atomBiosHandlePtr atomBIOS, int dac)
{
  DAC_LOAD_DETECTION_PS_ALLOCATION dac_data;
  AtomBiosArgRec data;
  unsigned char *space;

  dac_data.sDacload.usDeviceID = 0;
  dac_data.sDacload.ucDacType = 0;
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

    ret = atom_bios_dac_load_detect(info->atomBIOS, radeon_output->DACType);
    if (ret == ATOM_SUCCESS) {
      ErrorF("DAC connect %08X\n", (unsigned int)INREG(RADEON_BIOS_0_SCRATCH));
	bios_0_scratch = INREG(RADEON_BIOS_0_SCRATCH);
	
	if (radeon_output->devices & ATOM_DEVICE_CRT1_SUPPORT) {
	    if (bios_0_scratch & ATOM_S0_CRT1_COLOR)
		MonType = MT_CRT;
	} else if (radeon_output->devices & ATOM_DEVICE_CRT2_SUPPORT) {
	    if (bios_0_scratch & ATOM_S0_CRT2_COLOR)
		MonType = MT_CRT;
	}
    }
    return MonType;
}

