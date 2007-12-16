 /*
 * Copyright © 2007 Red Hat, Inc.
 *
 * PLL code is:
 * Copyright 2007  Luc Verhaegen <lverhaegen@novell.com>
 * Copyright 2007  Matthias Hopf <mhopf@novell.com>
 * Copyright 2007  Egbert Eich   <eich@novell.com>
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
 * avivo crtc handling functions. 
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
/* DPMS */
#define DPMS_SERVER
#include <X11/extensions/dpms.h>

#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_atombios.h"

#ifdef XF86DRI
#define _XF86DRI_SERVER_
#include "radeon_dri.h"
#include "radeon_sarea.h"
#include "sarea.h"
#endif

AtomBiosResult
atombios_enable_crtc(atomBiosHandlePtr atomBIOS, int crtc, int state)
{
    ENABLE_CRTC_PS_ALLOCATION crtc_data;
    AtomBiosArgRec data;
    unsigned char *space;

    crtc_data.ucCRTC = crtc;
    crtc_data.ucEnable = state;

    data.exec.index = GetIndexIntoMasterTable(COMMAND, EnableCRTC);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &crtc_data;
    
    if (RHDAtomBiosFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("%s CRTC %d success\n", state? "Enable":"Disable", crtc);
	return ATOM_SUCCESS ;
    }
  
    ErrorF("Enable CRTC failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

AtomBiosResult
atombios_blank_crtc(atomBiosHandlePtr atomBIOS, int crtc, int state)
{
    BLANK_CRTC_PS_ALLOCATION crtc_data;
    unsigned char *space;
    AtomBiosArgRec data;

    memset(&crtc_data, 0, sizeof(crtc_data));
    crtc_data.ucCRTC = crtc;
    crtc_data.ucBlanking = state;

    data.exec.index = offsetof(ATOM_MASTER_LIST_OF_COMMAND_TABLES, BlankCRTC) / sizeof(unsigned short);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &crtc_data;
    
    if (RHDAtomBiosFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("%s CRTC %d success\n", state? "Blank":"Unblank", crtc);
	return ATOM_SUCCESS ;
    }
  
    ErrorF("Blank CRTC failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

static void
atombios_crtc_enable(xf86CrtcPtr crtc, int enable)
{
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
    RADEONInfoPtr  info = RADEONPTR(crtc->scrn);

    atombios_enable_crtc(info->atomBIOS, radeon_crtc->crtc_id, enable);
    
    //TODOavivo_wait_idle(avivo);
}

void
atombios_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
    RADEONInfoPtr  info = RADEONPTR(crtc->scrn);
    switch (mode) {
    case DPMSModeOn:
    case DPMSModeStandby:
    case DPMSModeSuspend:
	atombios_enable_crtc(info->atomBIOS, radeon_crtc->crtc_id, 1);
	atombios_blank_crtc(info->atomBIOS, radeon_crtc->crtc_id, 0);
        break;
    case DPMSModeOff:
	atombios_blank_crtc(info->atomBIOS, radeon_crtc->crtc_id, 1);
	atombios_enable_crtc(info->atomBIOS, radeon_crtc->crtc_id, 0);
        break;
    }
}

static AtomBiosResult
atombios_set_crtc_timing(atomBiosHandlePtr atomBIOS, SET_CRTC_TIMING_PARAMETERS_PS_ALLOCATION *crtc_param)
{
    AtomBiosArgRec data;
    unsigned char *space;

    data.exec.index = GetIndexIntoMasterTable(COMMAND, SetCRTC_Timing);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = crtc_param;
    
    if (RHDAtomBiosFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Set CRTC Timing success\n");
	return ATOM_SUCCESS ;
    }
  
    ErrorF("Set CRTC Timing failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

void
atombios_crtc_set_pll(xf86CrtcPtr crtc, DisplayModePtr mode)
{
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
    RADEONInfoPtr  info = RADEONPTR(crtc->scrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int index = GetIndexIntoMasterTable(COMMAND, SetPixelClock);
    CARD32 sclock = mode->Clock;
    CARD32 ref_div = 0, fb_div = 0, post_div = 0;
    int major, minor;
    SET_PIXEL_CLOCK_PS_ALLOCATION spc_param;
    void *ptr;
    AtomBiosArgRec data;
    unsigned char *space;
    RADEONSavePtr save = info->ModeReg;

    if (IS_AVIVO_VARIANT) {
	CARD32 temp;
	RADEONComputePLL(&info->pll, mode->Clock * 1000, &temp, &fb_div, &ref_div, &post_div);
	sclock = temp / 10000;

	/* disable spread spectrum clocking for now -- thanks Hedy Lamarr */
	if (radeon_crtc->crtc_id == 0) {
	    temp = INREG(AVIVO_P1PLL_INT_SS_CNTL);
	    OUTREG(AVIVO_P1PLL_INT_SS_CNTL, temp & ~1);
	} else {
	    temp = INREG(AVIVO_P2PLL_INT_SS_CNTL);
	    OUTREG(AVIVO_P2PLL_INT_SS_CNTL, temp & ~1);
	}
    } else {
	sclock = save->dot_clock_freq;
	fb_div = save->feedback_div;
	post_div = save->post_div;
	ref_div = save->ppll_ref_div;
    }

    xf86DrvMsg(crtc->scrn->scrnIndex, X_INFO,
	       "crtc(%d) Clock: mode %d, PLL %u\n",
	       radeon_crtc->crtc_id, mode->Clock, sclock * 10);
    xf86DrvMsg(crtc->scrn->scrnIndex, X_INFO,
	       "crtc(%d) PLL  : refdiv %u, fbdiv 0x%X(%u), pdiv %u\n",
	       radeon_crtc->crtc_id, ref_div, fb_div, fb_div, post_div);

    atombios_get_command_table_version(info->atomBIOS, index, &major, &minor);

    ErrorF("table is %d %d\n", major, minor);
    switch(major) {
    case 1:
	switch(minor) {
	case 1:
	case 2: {
	    spc_param.sPCLKInput.usPixelClock = sclock;
	    spc_param.sPCLKInput.usRefDiv = ref_div;
	    spc_param.sPCLKInput.usFbDiv = fb_div;
	    spc_param.sPCLKInput.ucPostDiv = post_div;
	    spc_param.sPCLKInput.ucPpll = radeon_crtc->crtc_id ? ATOM_PPLL2 : ATOM_PPLL1;
	    spc_param.sPCLKInput.ucCRTC = radeon_crtc->crtc_id;
	    spc_param.sPCLKInput.ucRefDivSrc = 1;

	    ptr = &spc_param;
	    break;
	}
	default:
	    ErrorF("Unknown table version\n");
	    exit(-1);
	}
	break;
    default:
	ErrorF("Unknown table version\n");
	exit(-1);
    }

    data.exec.index = GetIndexIntoMasterTable(COMMAND, SetPixelClock);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = ptr;

    if (RHDAtomBiosFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Set CRTC PLL success\n");
	return;
    }

    ErrorF("Set CRTC PLL failed\n");
    return;
}

void
atombios_crtc_mode_set(xf86CrtcPtr crtc,
		       DisplayModePtr mode,
		       DisplayModePtr adjusted_mode,
		       int x, int y)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long fb_location = crtc->scrn->fbOffset + info->fbLocation;
    Bool           tilingOld   = info->tilingEnabled;

    SET_CRTC_TIMING_PARAMETERS_PS_ALLOCATION crtc_timing;

    memset(&crtc_timing, 0, sizeof(crtc_timing));

    if (info->allowColorTiling) {
	info->tilingEnabled = (adjusted_mode->Flags & (V_DBLSCAN | V_INTERLACE)) ? FALSE : TRUE;
#ifdef XF86DRI
	if (info->directRenderingEnabled && (info->tilingEnabled != tilingOld)) {
	    RADEONSAREAPrivPtr pSAREAPriv;
	    if (RADEONDRISetParam(pScrn, RADEON_SETPARAM_SWITCH_TILING, (info->tilingEnabled ? 1 : 0)) < 0)
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "[drm] failed changing tiling status\n");
	    /* if this is called during ScreenInit() we don't have pScrn->pScreen yet */
	    pSAREAPriv = DRIGetSAREAPrivate(screenInfo.screens[pScrn->scrnIndex]);
	    info->tilingEnabled = pSAREAPriv->tiling_enabled ? TRUE : FALSE;
	}
#endif
    }

    crtc_timing.ucCRTC = radeon_crtc->crtc_id;
    crtc_timing.usH_Total = adjusted_mode->CrtcHTotal;
    crtc_timing.usH_Disp = adjusted_mode->CrtcHDisplay;
    crtc_timing.usH_SyncStart = adjusted_mode->CrtcHSyncStart;
    crtc_timing.usH_SyncWidth = adjusted_mode->CrtcHSyncEnd - adjusted_mode->CrtcHSyncStart;

    crtc_timing.usV_Total = adjusted_mode->CrtcVTotal;
    crtc_timing.usV_Disp = adjusted_mode->CrtcVDisplay;
    crtc_timing.usV_SyncStart = adjusted_mode->CrtcVSyncStart;
    crtc_timing.usV_SyncWidth = adjusted_mode->CrtcVSyncEnd - adjusted_mode->CrtcVSyncStart;

    if (adjusted_mode->Flags & V_NVSYNC)
      crtc_timing.susModeMiscInfo.usAccess |= ATOM_VSYNC_POLARITY;

    if (adjusted_mode->Flags & V_NHSYNC)
      crtc_timing.susModeMiscInfo.usAccess |= ATOM_HSYNC_POLARITY;

    ErrorF("Mode %dx%d - %d %d %d\n", adjusted_mode->CrtcHDisplay, adjusted_mode->CrtcVDisplay,
	   adjusted_mode->CrtcHTotal, adjusted_mode->CrtcVTotal, adjusted_mode->Flags);

    RADEONInitMemMapRegisters(pScrn, info->ModeReg, info);
    RADEONRestoreMemMapRegisters(pScrn, info->ModeReg);

    if (IS_AVIVO_VARIANT) {
	radeon_crtc->fb_width = adjusted_mode->CrtcHDisplay;
	radeon_crtc->fb_height = pScrn->virtualY;
	radeon_crtc->fb_pitch = adjusted_mode->CrtcHDisplay;
	radeon_crtc->fb_length = radeon_crtc->fb_pitch * radeon_crtc->fb_height * 4;
	switch (crtc->scrn->bitsPerPixel) {
	case 15:
	    radeon_crtc->fb_format = AVIVO_D1GRPH_CONTROL_DEPTH_16BPP | AVIVO_D1GRPH_CONTROL_16BPP_ARGB1555;
	    break;
	case 16:
 	    radeon_crtc->fb_format = AVIVO_D1GRPH_CONTROL_DEPTH_16BPP | AVIVO_D1GRPH_CONTROL_16BPP_RGB565;
	    break;
	case 24:
	case 32:
 	    radeon_crtc->fb_format = AVIVO_D1GRPH_CONTROL_DEPTH_32BPP | AVIVO_D1GRPH_CONTROL_32BPP_ARGB8888;
	    break;
	default:
	    FatalError("Unsupported screen depth: %d\n", xf86GetDepth());
	}

	if (info->tilingEnabled && (crtc->rotatedData == NULL)) {
	    radeon_crtc->fb_format |= AVIVO_D1GRPH_MACRO_ADDRESS_MODE;
	}

	if (radeon_crtc->crtc_id == 0)
	    OUTREG(AVIVO_D1VGA_CONTROL, 0);
	else
	    OUTREG(AVIVO_D2VGA_CONTROL, 0);

	/* setup fb format and location
	 */
	if (crtc->rotatedData != NULL) {
	    /* x/y offset is already included */
	    x = 0;
	    y = 0;
	    fb_location = fb_location + (char *)crtc->rotatedData - (char *)info->FB;
	}

	/* lock the grph regs */
	OUTREG(AVIVO_D1GRPH_UPDATE + radeon_crtc->crtc_offset, AVIVO_D1GRPH_UPDATE_LOCK);

	OUTREG(AVIVO_D1GRPH_PRIMARY_SURFACE_ADDRESS + radeon_crtc->crtc_offset, fb_location);
	OUTREG(AVIVO_D1GRPH_SECONDARY_SURFACE_ADDRESS + radeon_crtc->crtc_offset, fb_location);
	OUTREG(AVIVO_D1GRPH_CONTROL + radeon_crtc->crtc_offset,
	       radeon_crtc->fb_format);

	OUTREG(AVIVO_D1GRPH_SURFACE_OFFSET_X + radeon_crtc->crtc_offset, 0);
	OUTREG(AVIVO_D1GRPH_SURFACE_OFFSET_Y + radeon_crtc->crtc_offset, 0);
	OUTREG(AVIVO_D1GRPH_X_START + radeon_crtc->crtc_offset, 0);
	OUTREG(AVIVO_D1GRPH_Y_START + radeon_crtc->crtc_offset, 0);
	OUTREG(AVIVO_D1GRPH_X_END + radeon_crtc->crtc_offset,
	       crtc->scrn->virtualX);
	OUTREG(AVIVO_D1GRPH_Y_END + radeon_crtc->crtc_offset,
	       crtc->scrn->virtualY);
	OUTREG(AVIVO_D1GRPH_PITCH + radeon_crtc->crtc_offset,
	       crtc->scrn->displayWidth);
	OUTREG(AVIVO_D1GRPH_ENABLE + radeon_crtc->crtc_offset, 1);

	/* unlock the grph regs */
	OUTREG(AVIVO_D1GRPH_UPDATE + radeon_crtc->crtc_offset, 0);

	/* lock the mode regs */
	OUTREG(AVIVO_D1SCL_UPDATE + radeon_crtc->crtc_offset, AVIVO_D1SCL_UPDATE_LOCK);

	OUTREG(AVIVO_D1MODE_DESKTOP_HEIGHT + radeon_crtc->crtc_offset,
	       crtc->scrn->virtualY);
	OUTREG(AVIVO_D1MODE_VIEWPORT_START + radeon_crtc->crtc_offset, (x << 16) | y);
	OUTREG(AVIVO_D1MODE_VIEWPORT_SIZE + radeon_crtc->crtc_offset,
	       (mode->HDisplay << 16) | mode->VDisplay);
	/* unlock the mode regs */
	OUTREG(AVIVO_D1SCL_UPDATE + radeon_crtc->crtc_offset, 0);

    }

    atombios_crtc_set_pll(crtc, adjusted_mode);

    atombios_set_crtc_timing(info->atomBIOS, &crtc_timing);

    if (info->tilingEnabled != tilingOld) {
	/* need to redraw front buffer, I guess this can be considered a hack ? */
	/* if this is called during ScreenInit() we don't have pScrn->pScreen yet */
	if (pScrn->pScreen)
	    xf86EnableDisableFBAccess(pScrn->scrnIndex, FALSE);
	RADEONChangeSurfaces(pScrn);
	if (pScrn->pScreen)
	    xf86EnableDisableFBAccess(pScrn->scrnIndex, TRUE);
	/* xf86SetRootClip would do, but can't access that here */
    }

}

