/*
 * Copyright © 2007 Dave Airlie
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
atombios_enable_crtc(atomBIOSHandlePtr atomBIOS, int crtc, int state)
{
    ENABLE_CRTC_PS_ALLOCATION crtc_data;
    AtomBIOSArg data;
    unsigned char *space;

    crtc_data.ucCRTC = crtc;
    crtc_data.ucEnable = state;

    data.exec.index = GetIndexIntoMasterTable(COMMAND, EnableCRTC);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &crtc_data;
    
    if (RHDAtomBIOSFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("%s CRTC %d success\n", state? "Enable":"Disable", crtc);
	return ATOM_SUCCESS ;
    }
  
    ErrorF("Enable CRTC failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

AtomBiosResult
atombios_blank_crtc(atomBIOSHandlePtr atomBIOS, int crtc, int state)
{
    BLANK_CRTC_PS_ALLOCATION crtc_data;
    unsigned char *space;
    AtomBIOSArg data;

    memset(&crtc_data, 0, sizeof(crtc_data));
    crtc_data.ucCRTC = crtc;
    crtc_data.ucBlanking = state;

    data.exec.index = offsetof(ATOM_MASTER_LIST_OF_COMMAND_TABLES, BlankCRTC) / sizeof(unsigned short);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = &crtc_data;
    
    if (RHDAtomBIOSFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
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
    unsigned char *RADEONMMIO = info->MMIO;
    int scan_enable, cntl;
    AtomBiosResult res;
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
atombios_set_crtc_timing(atomBIOSHandlePtr atomBIOS, SET_CRTC_TIMING_PARAMETERS_PS_ALLOCATION *crtc_param)
{
    AtomBIOSArg data;
    unsigned char *space;

    data.exec.index = GetIndexIntoMasterTable(COMMAND, SetCRTC_Timing);
    data.exec.dataSpace = (void *)&space;
    data.exec.pspace = crtc_param;
    
    if (RHDAtomBIOSFunc(atomBIOS->scrnIndex, atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
	ErrorF("Set CRTC Timing success\n");
	return ATOM_SUCCESS ;
    }
  
    ErrorF("Set CRTC Timing failed\n");
    return ATOM_NOT_IMPLEMENTED;
}

#define USE_RADEONHD_CODE_FOR_PLL 1
#if USE_RADEONHD_CODE_FOR_PLL

struct rhdPLL {
    int scrnIndex;

/* also used as an index to rhdPtr->PLLs */
#define PLL_ID_PLL1  0
#define PLL_ID_PLL2  1
#define PLL_ID_NONE -1
    int Id;

    CARD32 CurrentClock;
    Bool Active;

    /* from defaults or from atom */
    CARD32 RefClock;
    CARD32 InMin;
    CARD32 InMax;
    CARD32 OutMin;
    CARD32 OutMax;
    CARD32 PixMin;
    CARD32 PixMax;
};

static struct rhdPLL mypll = {
    0, 0, 0, 0,
    27000,
    1000, 13500,
    600000, 1100000,
    16000, 400000
};
/*
 * Calculate the PLL parameters for a given dotclock.
 */
static Bool
PLLCalculate(CARD32 PixelClock,
	     CARD16 *RefDivider, CARD16 *FBDivider, CARD8 *PostDivider)
{
/* limited by the number of bits available */
#define FB_DIV_LIMIT 1024 /* rv6x0 doesn't like 2048 */
#define REF_DIV_LIMIT 1024
#define POST_DIV_LIMIT 128
    struct rhdPLL *PLL = &mypll;
    CARD32 FBDiv, RefDiv, PostDiv, BestDiff = 0xFFFFFFFF;
    float Ratio;

    Ratio = ((float) PixelClock) / ((float) PLL->RefClock);

    for (PostDiv = 2; PostDiv < POST_DIV_LIMIT; PostDiv++) {
	CARD32 VCOOut = PixelClock * PostDiv;

	/* we are conservative and avoid the limits */
	if (VCOOut <= PLL->OutMin)
	    continue;
	if (VCOOut >= PLL->OutMax)
	    break;

        for (RefDiv = 1; RefDiv <= REF_DIV_LIMIT; RefDiv++)
	{
	    CARD32 Diff;

	    FBDiv = (CARD32) ((Ratio * PostDiv * RefDiv) + 0.5);

	    if (FBDiv >= FB_DIV_LIMIT)
	      break;

	    if (FBDiv > (500 + (13 * RefDiv))) /* rv6x0 limit */
		break;

	    Diff = abs( PixelClock - (FBDiv * PLL->RefClock) / (PostDiv * RefDiv) );

	    if (Diff < BestDiff) {
		*FBDivider = FBDiv;
		*RefDivider = RefDiv;
		*PostDivider = PostDiv;
		BestDiff = Diff;
	    }

	    if (BestDiff == 0)
		break;
	}
	if (BestDiff == 0)
	    break;
    }

    if (BestDiff != 0xFFFFFFFF) {
	ErrorF("PLL Calculation: %dkHz = "
		   "(((0x%X / 0x%X) * 0x%X) / 0x%X) (%dkHz off)\n",
		   (int) PixelClock, (unsigned int) PLL->RefClock, *RefDivider,
		   *FBDivider, *PostDivider, (int) BestDiff);
	xf86DrvMsg(PLL->scrnIndex, X_INFO, "PLL for %dkHz uses %dkHz internally.\n",
		   (int) PixelClock,
		   (int) (PLL->RefClock * *FBDivider) / *RefDivider);
	return TRUE;
    } else { /* Should never happen */
	xf86DrvMsg(PLL->scrnIndex, X_ERROR,
		   "%s: Failed to get a valid PLL setting for %dkHz\n",
		   __func__, (int) PixelClock);
	return FALSE;
    }
}
#else // avivo code 


#endif

void
atombios_crtc_set_pll(xf86CrtcPtr crtc, DisplayModePtr mode)
{
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
    RADEONInfoPtr  info = RADEONPTR(crtc->scrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int index = GetIndexIntoMasterTable(COMMAND, SetPixelClock);
    double c;
    int div1, div2, clock;
    int sclock;
    uint16_t ref_div, fb_div;
    uint8_t post_div;
    int mul;
    int major, minor;
    SET_PIXEL_CLOCK_PS_ALLOCATION spc_param;
    void *ptr;
    AtomBIOSArg data;
    unsigned char *space;    
    RADEONSavePtr save = &info->ModeReg;

    if (IS_AVIVO_VARIANT) {
	PLLCalculate(mode->Clock, &ref_div, &fb_div, &post_div);
    } else {
	fb_div = save->feedback_div;
	post_div = save->post_div;
	ref_div = save->ppll_ref_div;
    }

    xf86DrvMsg(crtc->scrn->scrnIndex, X_INFO,
	       "crtc(%d) Clock: mode %d, PLL %d\n",
	       radeon_crtc->crtc_id, mode->Clock, sclock);
    xf86DrvMsg(crtc->scrn->scrnIndex, X_INFO,
	       "crtc(%d) PLL  : refdiv %d, fbdiv 0x%X(%d), pdiv %d\n",
	       radeon_crtc->crtc_id, ref_div, fb_div, fb_div, post_div);

    atombios_get_command_table_version(info->atomBIOS, index, &major, &minor);
    
    ErrorF("table is %d %d\n", major, minor);
    switch(major) {
    case 1:
	switch(minor) {
	case 1:
	case 2: {
	    spc_param.sPCLKInput.usPixelClock = sclock / 10;
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

    if (RHDAtomBIOSFunc(info->atomBIOS->scrnIndex, info->atomBIOS, ATOMBIOS_EXEC, &data) == ATOM_SUCCESS) {
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
    int regval;
    AtomBiosResult atom_res;
    RADEONSavePtr restore = &info->ModeReg;
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

    if (IS_AVIVO_VARIANT) {
	radeon_crtc->fb_width = adjusted_mode->CrtcHDisplay;
	radeon_crtc->fb_height = pScrn->virtualY;
	radeon_crtc->fb_pitch = adjusted_mode->CrtcHDisplay;
	radeon_crtc->fb_length = radeon_crtc->fb_pitch * radeon_crtc->fb_height * 4;
	switch (crtc->scrn->bitsPerPixel) {
	case 15:
	    radeon_crtc->fb_format = AVIVO_CRTC_FORMAT_ARGB15;
	    break;
	case 16:
	    radeon_crtc->fb_format = AVIVO_CRTC_FORMAT_ARGB16;
	    break;
	case 24:
	case 32:
	    radeon_crtc->fb_format = AVIVO_CRTC_FORMAT_ARGB32;
	    break;
	default:
	    FatalError("Unsupported screen depth: %d\n", xf86GetDepth());
	}
	if (info->tilingEnabled) {
	    radeon_crtc->fb_format |= AVIVO_CRTC_MACRO_ADDRESS_MODE;
	}
	/* setup fb format and location
	 */
	OUTREG(AVIVO_CRTC1_EXPANSION_SOURCE + radeon_crtc->crtc_offset,
	       (mode->HDisplay << 16) | mode->VDisplay);

	OUTREG(AVIVO_CRTC1_FB_LOCATION + radeon_crtc->crtc_offset, fb_location);
	OUTREG(AVIVO_CRTC1_FB_END + radeon_crtc->crtc_offset, fb_location);
	OUTREG(AVIVO_CRTC1_FB_FORMAT + radeon_crtc->crtc_offset,
	       radeon_crtc->fb_format);

	OUTREG(AVIVO_CRTC1_X_LENGTH + radeon_crtc->crtc_offset,
	       crtc->scrn->virtualX);
	OUTREG(AVIVO_CRTC1_Y_LENGTH + radeon_crtc->crtc_offset,
	       crtc->scrn->virtualY);
	OUTREG(AVIVO_CRTC1_PITCH + radeon_crtc->crtc_offset,
	       crtc->scrn->displayWidth);

	/* avivo can only shift offset by 4 pixel in x if you program somethings
	 * not multiple of 4 you gonna drive the GPU crazy and likely won't
	 * be able to restore it without cold reboot (vbe post not enough)
	 */
	x = x & ~3;
	OUTREG(AVIVO_CRTC1_OFFSET_END + radeon_crtc->crtc_offset,
	       ((mode->HDisplay + x -128) << 16) | (mode->VDisplay + y - 128));
	OUTREG(AVIVO_CRTC1_OFFSET_START + radeon_crtc->crtc_offset, (x << 16) | y);

	OUTREG(AVIVO_CRTC1_SCAN_ENABLE + radeon_crtc->crtc_offset, 1);

    }

    // moved to output
    //atombios_set_crtc_source(crtc);

    atombios_set_crtc_timing(info->atomBIOS, &crtc_timing);

    atombios_crtc_set_pll(crtc, adjusted_mode);

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

