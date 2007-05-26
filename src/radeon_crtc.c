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

void radeon_crtc_load_lut(xf86CrtcPtr crtc);

static void
radeon_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
    int mask;
    ScrnInfoPtr pScrn = crtc->scrn;
    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    
    mask = radeon_crtc->crtc_id ? (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS | RADEON_CRTC2_HSYNC_DIS | RADEON_CRTC2_DISP_REQ_EN_B) : (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS | RADEON_CRTC_VSYNC_DIS);


    switch(mode) {
    case DPMSModeOn:
	if (radeon_crtc->crtc_id) {
	    OUTREGP(RADEON_CRTC2_GEN_CNTL, 0, ~mask);
	} else {
	    OUTREGP(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_DISP_REQ_EN_B);
	    OUTREGP(RADEON_CRTC_EXT_CNTL, 0, ~mask);
	}
	break;
    case DPMSModeStandby:
	if (radeon_crtc->crtc_id) {
	    OUTREGP(RADEON_CRTC2_GEN_CNTL, (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_HSYNC_DIS), ~mask);
	} else {
	    OUTREGP(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_DISP_REQ_EN_B);
	    OUTREGP(RADEON_CRTC_EXT_CNTL, (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_HSYNC_DIS), ~mask);
	}
	break;
    case DPMSModeSuspend:
	if (radeon_crtc->crtc_id) {
	    OUTREGP(RADEON_CRTC2_GEN_CNTL, (RADEON_CRTC2_DISP_DIS | RADEON_CRTC2_VSYNC_DIS), ~mask);
	} else {
	    OUTREGP(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_DISP_REQ_EN_B);
	    OUTREGP(RADEON_CRTC_EXT_CNTL, (RADEON_CRTC_DISPLAY_DIS | RADEON_CRTC_VSYNC_DIS), ~mask);
	}
	break;
    case DPMSModeOff:
	if (radeon_crtc->crtc_id) {
	    OUTREGP(RADEON_CRTC2_GEN_CNTL, mask, ~mask);
	} else {
	    OUTREGP(RADEON_CRTC_GEN_CNTL, RADEON_CRTC_DISP_REQ_EN_B, ~RADEON_CRTC_DISP_REQ_EN_B);
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
    radeon_crtc_dpms(crtc, DPMSModeOff);
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
    double         dot_clock = 0;

    for (i = 0; i < xf86_config->num_output; i++) {
	xf86OutputPtr output = xf86_config->output[i];
	RADEONOutputPrivatePtr radeon_output = output->driver_private;

	if (output->crtc == crtc) {
	    montype = radeon_output->MonType;
	}
    }

    ErrorF("init memmap\n");
    RADEONInitMemMapRegisters(pScrn, &info->ModeReg, info);
    ErrorF("init common\n");
    RADEONInitCommonRegisters(&info->ModeReg, info);

    switch (radeon_crtc->crtc_id) {
    case 0:
	ErrorF("init crtc1\n");
	RADEONInitCrtcRegisters(crtc, &info->ModeReg, adjusted_mode, x, y);
        dot_clock = adjusted_mode->Clock / 1000.0;
        if (dot_clock) {
	    ErrorF("init pll1\n");
	    RADEONInitPLLRegisters(pScrn, info, &info->ModeReg, &info->pll, dot_clock);
        } else {
            info->ModeReg.ppll_ref_div = info->SavedReg.ppll_ref_div;
            info->ModeReg.ppll_div_3   = info->SavedReg.ppll_div_3;
            info->ModeReg.htotal_cntl  = info->SavedReg.htotal_cntl;
        }
	break;
    case 1:
	ErrorF("init crtc2\n");
        RADEONInitCrtc2Registers(crtc, &info->ModeReg, adjusted_mode, x, y);
        dot_clock = adjusted_mode->Clock / 1000.0;
        if (dot_clock) {
	    ErrorF("init pll2\n");
	    RADEONInitPLL2Registers(pScrn, &info->ModeReg, &info->pll, dot_clock, montype != MT_CRT);
        }
	break;
    }
    
    ErrorF("restore memmap\n");
    RADEONRestoreMemMapRegisters(pScrn, &info->ModeReg);
    ErrorF("restore common\n");
    RADEONRestoreCommonRegisters(pScrn, &info->ModeReg);    

    switch (radeon_crtc->crtc_id) {
    case 0:
	ErrorF("restore crtc1\n");
	RADEONRestoreCrtcRegisters(pScrn, &info->ModeReg);
	ErrorF("restore pll1\n");
	RADEONRestorePLLRegisters(pScrn, &info->ModeReg);
	break;
    case 1:
	ErrorF("restore crtc2\n");
	RADEONRestoreCrtc2Registers(pScrn, &info->ModeReg);
	ErrorF("restore pll2\n");
	RADEONRestorePLL2Registers(pScrn, &info->ModeReg);
	break;
    }

    if (info->DispPriority)
        RADEONInitDispBandwidth(pScrn);

}

static void
radeon_crtc_mode_commit(xf86CrtcPtr crtc)
{
    radeon_crtc_dpms(crtc, DPMSModeOn);
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

#ifdef XF86DRI
    if (info->CPStarted && pScrn->pScreen) {
	DRILock(pScrn->pScreen, 0);
	return TRUE;
    } else {
	return FALSE;
    }
#endif

    return FALSE;

}

static void
radeon_crtc_unlock(xf86CrtcPtr crtc)
{
    ScrnInfoPtr		pScrn = crtc->scrn;
    RADEONInfoPtr  info = RADEONPTR(pScrn);

#ifdef XF86DRI
	if (info->CPStarted && pScrn->pScreen) DRIUnlock(pScrn->pScreen);
#endif

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
    .load_cursor_argb = radeon_crtc_load_cursor_argb,
    .destroy = NULL, /* XXX */
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

