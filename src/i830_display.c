/* -*- c-basic-offset: 4 -*- */
/*
 * Copyright © 2006 Intel Corporation
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
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include "xf86.h"
#include "i830.h"
#include "i830_bios.h"
#include "i830_display.h"
#include "i830_debug.h"
#include "i830_xf86Modes.h"

/** Returns the pixel clock for the given refclk and divisors. */
static int i830_clock(int refclk, int m1, int m2, int n, int p1, int p2)
{
    return refclk * (5 * m1 + m2) / n / (p1 * p2);
}

static void
i830PrintPll(char *prefix, int refclk, int m1, int m2, int n, int p1, int p2)
{
    int dotclock;

    dotclock = i830_clock(refclk, m1, m2, n, p1, p2);

    ErrorF("%s: dotclock %d ((%d, %d), %d, (%d, %d))\n", prefix, dotclock,
	   m1, m2, n, p1, p2);
}

/**
 * Returns whether any output on the specified pipe is of the specified type
 */
Bool
i830PipeHasType (xf86CrtcPtr crtc, int type)
{
    ScrnInfoPtr	pScrn = crtc->scrn;
    I830Ptr	pI830 = I830PTR(pScrn);
    int		i;

    for (i = 0; i < pI830->xf86_config.num_output; i++)
    {
	xf86OutputPtr  output = pI830->xf86_config.output[i];
	if (output->crtc == crtc)
	{
	    I830OutputPrivatePtr    intel_output = output->driver_private;
	    if (intel_output->type == type)
		return TRUE;
	}
    }
    return FALSE;
}

/**
 * Returns whether the given set of divisors are valid for a given refclk with
 * the given outputs.
 *
 * The equation for these divisors would be:
 * clk = refclk * (5 * m1 + m2) / n / (p1 * p2)
 */
static Bool
i830PllIsValid(xf86CrtcPtr crtc, int refclk, int m1, int m2,
	       int n, int p1, int p2)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    int p, m, vco, dotclock;
    int min_m1, max_m1, min_m2, max_m2, min_m, max_m, min_n, max_n;
    int min_p1, max_p1, min_p, max_p, min_vco, max_vco, min_dot, max_dot;

    if (IS_I9XX(pI830)) {
	min_m1 = 10;
	max_m1 = 20;
	min_m2 = 5;
	max_m2 = 9;
	min_m = 70;
	max_m = 120;
	min_n = 3;
	max_n = 8;
	min_p1 = 1;
	max_p1 = 8;
	if (i830PipeHasType (crtc, I830_OUTPUT_LVDS)) {
	    min_p = 7;
	    max_p = 98;
	} else {
	    min_p = 5;
	    max_p = 80;
	}
	min_vco = 1400000;
	max_vco = 2800000;
	min_dot = 20000;
	max_dot = 400000;
    } else {
	min_m1 = 18;
	max_m1 = 26;
	min_m2 = 6;
	max_m2 = 16;
	min_m = 96;
	max_m = 140;
	min_n = 3;
	max_n = 16;
	min_p1 = 2;
	max_p1 = 18;
	min_vco = 930000;
	max_vco = 1400000;
	min_dot = 20000;
	max_dot = 350000;
	min_p = 4;
	max_p = 128;
    }

    p = p1 * p2;
    m = 5 * m1 + m2;
    vco = refclk * m / n;
    dotclock = i830_clock(refclk, m1, m2, n, p1, p2);

    if (p1 < min_p1 || p1 > max_p1)
	return FALSE;
    if (p < min_p || p > max_p)
	return FALSE;
    if (m2 < min_m2 || m2 > max_m2)
	return FALSE;
    if (m1 < min_m1 || m1 > max_m1)
	return FALSE;
    if (m1 <= m2)
	return FALSE;
    if (m < min_m || m > max_m)
	return FALSE;
    if (n < min_n || n > max_n)
	return FALSE;
    if (vco < min_vco || vco > max_vco)
	return FALSE;
    /* XXX: We may need to be checking "Dot clock" depending on the multiplier,
     * output, etc., rather than just a single range.
     */
    if (dotclock < min_dot || dotclock > max_dot)
	return FALSE;

    return TRUE;
}

/**
 * Returns a set of divisors for the desired target clock with the given refclk,
 * or FALSE.  Divisor values are the actual divisors for
 * clk = refclk * (5 * m1 + m2) / n / (p1 * p2)
 */
static Bool
i830FindBestPLL(xf86CrtcPtr crtc, int target, int refclk,
		int *outm1, int *outm2, int *outn, int *outp1, int *outp2)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    int m1, m2, n, p1, p2;
    int err = target;
    int min_m1, max_m1, min_m2, max_m2, min_n, max_n, min_p1, max_p1;

    if (IS_I9XX(pI830)) {
	min_m1 = 10;
	max_m1 = 20;
	min_m2 = 5;
	max_m2 = 9;
	min_n = 3;
	max_n = 8;
	min_p1 = 1;
	max_p1 = 8;
	if (i830PipeHasType (crtc, I830_OUTPUT_LVDS)) {
	    /* The single-channel range is 25-112Mhz, and dual-channel
	     * is 80-224Mhz.  Prefer single channel as much as possible.
	     */
	    if (target < 112000)
		p2 = 14;
	    else
		p2 = 7;
	} else {
	    if (target < 200000)
		p2 = 10;
	    else
		p2 = 5;
	}
    } else {
	min_m1 = 18;
	max_m1 = 26;
	min_m2 = 6;
	max_m2 = 16;
	min_n = 3;
	max_n = 16;
	min_p1 = 2;
	max_p1 = 18;
	if (target < 165000)
	    p2 = 4;
	else
	    p2 = 2;
    }


    for (m1 = min_m1; m1 <= max_m1; m1++) {
	for (m2 = min_m2; m2 < max_m2; m2++) {
	    for (n = min_n; n <= max_n; n++) {
		for (p1 = min_p1; p1 <= max_p1; p1++) {
		    int clock, this_err;

		    if (!i830PllIsValid(crtc, refclk, m1, m2, n,
					p1, p2)) {
			continue;
		    }

		    clock = i830_clock(refclk, m1, m2, n, p1, p2);
		    this_err = abs(clock - target);
		    if (this_err < err) {
			*outm1 = m1;
			*outm2 = m2;
			*outn = n;
			*outp1 = p1;
			*outp2 = p2;
			err = this_err;
		    }
		}
	    }
	}
    }

    return (err != target);
}

void
i830WaitForVblank(ScrnInfoPtr pScreen)
{
    /* Wait for 20ms, i.e. one cycle at 50hz. */
    usleep(20000);
}

void
i830PipeSetBase(xf86CrtcPtr crtc, int x, int y)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    I830CrtcPrivatePtr	intel_crtc = crtc->driver_private;
    int pipe = intel_crtc->pipe;
    unsigned long Start;
    int dspbase = (pipe == 0 ? DSPABASE : DSPBBASE);
    int dspsurf = (pipe == 0 ? DSPASURF : DSPBSURF);

    if (I830IsPrimary(pScrn))
	Start = pI830->FrontBuffer.Start;
    else {
	I830Ptr pI8301 = I830PTR(pI830->entityPrivate->pScrn_1);
	Start = pI8301->FrontBuffer2.Start;
    }

    if (IS_I965G(pI830)) {
	OUTREG(dspbase, ((y * pScrn->displayWidth + x) * pI830->cpp));
	OUTREG(dspsurf, Start);
    } else {
	OUTREG(dspbase, Start + ((y * pScrn->displayWidth + x) * pI830->cpp));
    }

    crtc->x = x;
    crtc->y = y;
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
i830PipeFindClosestMode(xf86CrtcPtr crtc, DisplayModePtr pMode)
{
    ScrnInfoPtr	pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    DisplayModePtr pBest = NULL, pScan = NULL;
    int i;

    /* Assume that there's only one output connected to the given CRTC. */
    for (i = 0; i < pI830->xf86_config.num_output; i++) 
    {
	xf86OutputPtr  output = pI830->xf86_config.output[i];
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
	I830CrtcPrivatePtr  intel_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "No pipe mode list for pipe %d,"
		   "continuing with desired mode\n", intel_crtc->pipe);
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
	I830CrtcPrivatePtr  intel_crtc = crtc->driver_private;
	int		    pipe = intel_crtc->pipe;
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Choosing pipe %d's mode %dx%d@%.1f instead of xf86 "
		   "mode %dx%d@%.1f\n", pipe,
		   pBest->HDisplay, pBest->VDisplay, pBest->VRefresh,
		   pMode->HDisplay, pMode->VDisplay, pMode->VRefresh);
	pMode = pBest;
    }
    return pMode;
}

/**
 * Return whether any outputs are connected to the specified pipe
 */

Bool
i830PipeInUse (xf86CrtcPtr crtc)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    int	i;
    
    for (i = 0; i < pI830->xf86_config.num_output; i++)
	if (pI830->xf86_config.output[i]->crtc == crtc)
	    return TRUE;
    return FALSE;
}

static void
i830_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    I830CrtcPrivatePtr intel_crtc = crtc->driver_private;
    int pipe = intel_crtc->pipe;
    int dpll_reg = (pipe == 0) ? DPLL_A : DPLL_B;
    int dspcntr_reg = (pipe == 0) ? DSPACNTR : DSPBCNTR;
    int pipeconf_reg = (pipe == 0) ? PIPEACONF : PIPEBCONF;
    CARD32 temp;

    /* XXX: When our outputs are all unaware of DPMS modes other than off and
     * on, we should map those modes to DPMSModeOff in the CRTC.
     */
    switch (mode) {
    case DPMSModeOn:
    case DPMSModeStandby:
    case DPMSModeSuspend:
	/* Enable the DPLL */
	temp = INREG(dpll_reg);
	OUTREG(dpll_reg, temp | DPLL_VCO_ENABLE);

	/* Wait for the clocks to stabilize. */
	usleep(150);

	/* Enable the pipe */
	temp = INREG(pipeconf_reg);
	OUTREG(pipeconf_reg, temp | PIPEACONF_ENABLE);

	/* Enable the plane */
	temp = INREG(dspcntr_reg);
	OUTREG(dspcntr_reg, temp | DISPLAY_PLANE_ENABLE);
	break;
    case DPMSModeOff:
	/* Disable display plane */
	temp = INREG(dspcntr_reg);
	OUTREG(dspcntr_reg, temp & ~DISPLAY_PLANE_ENABLE);

	/* Disable the VGA plane that we never use */
	OUTREG(VGACNTRL, VGA_DISP_DISABLE);

	if (!IS_I9XX(pI830)) {
	    /* Wait for vblank for the disable to take effect */
	    i830WaitForVblank(pScrn);
	}

	/* Next, disable display pipes */
	temp = INREG(pipeconf_reg);
	OUTREG(pipeconf_reg, temp & ~PIPEACONF_ENABLE);

	/* Wait for vblank for the disable to take effect. */
	i830WaitForVblank(pScrn);

	temp = INREG(dpll_reg);
	OUTREG(dpll_reg, temp & ~DPLL_VCO_ENABLE);
	break;
    }
}

static Bool
i830_crtc_mode_fixup(xf86CrtcPtr crtc, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode)
{
    return TRUE;
}

/**
 * Sets up registers for the given mode/adjusted_mode pair.
 *
 * The clocks, CRTCs and outputs attached to this CRTC must be off.
 *
 * This shouldn't enable any clocks, CRTCs, or outputs, but they should
 * be easily turned on/off after this.
 */
static void
i830_crtc_mode_set(xf86CrtcPtr crtc, DisplayModePtr mode,
		   DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    I830CrtcPrivatePtr intel_crtc = crtc->driver_private;
    int pipe = intel_crtc->pipe;
    int fp_reg = (pipe == 0) ? FPA0 : FPB0;
    int dpll_reg = (pipe == 0) ? DPLL_A : DPLL_B;
    int dpll_md_reg = (intel_crtc->pipe == 0) ? DPLL_A_MD : DPLL_B_MD;
    int dspcntr_reg = (pipe == 0) ? DSPACNTR : DSPBCNTR;
    int pipeconf_reg = (pipe == 0) ? PIPEACONF : PIPEBCONF;
    int htot_reg = (pipe == 0) ? HTOTAL_A : HTOTAL_B;
    int hblank_reg = (pipe == 0) ? HBLANK_A : HBLANK_B;
    int hsync_reg = (pipe == 0) ? HSYNC_A : HSYNC_B;
    int vtot_reg = (pipe == 0) ? VTOTAL_A : VTOTAL_B;
    int vblank_reg = (pipe == 0) ? VBLANK_A : VBLANK_B;
    int vsync_reg = (pipe == 0) ? VSYNC_A : VSYNC_B;
    int dspsize_reg = (pipe == 0) ? DSPASIZE : DSPBSIZE;
    int dspstride_reg = (pipe == 0) ? DSPASTRIDE : DSPBSTRIDE;
    int dsppos_reg = (pipe == 0) ? DSPAPOS : DSPBPOS;
    int pipesrc_reg = (pipe == 0) ? PIPEASRC : PIPEBSRC;
    int m1 = 0, m2 = 0, n = 0, p1 = 0, p2 = 0;
    int i;
    int refclk;
    CARD32 dpll = 0, fp = 0, temp, dspcntr;
    Bool ok, is_sdvo = FALSE, is_dvo = FALSE;
    Bool is_crt = FALSE, is_lvds = FALSE, is_tv = FALSE;

    /* Set up some convenient bools for what outputs are connected to
     * our pipe, used in DPLL setup.
     */
    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr  output = pI830->xf86_config.output[i];
	I830OutputPrivatePtr intel_output = output->driver_private;

	if (output->crtc != crtc)
	    continue;

	switch (intel_output->type) {
	case I830_OUTPUT_LVDS:
	    is_lvds = TRUE;
	    break;
	case I830_OUTPUT_SDVO:
	    is_sdvo = TRUE;
	    break;
	case I830_OUTPUT_DVO:
	    is_dvo = TRUE;
	    break;
	case I830_OUTPUT_TVOUT:
	    is_tv = TRUE;
	    break;
	case I830_OUTPUT_ANALOG:
	    is_crt = TRUE;
	    break;
	}
    }

    if (IS_I9XX(pI830)) {
	refclk = 96000;
    } else {
	refclk = 48000;
    }

    ok = i830FindBestPLL(crtc, adjusted_mode->Clock, refclk, &m1, &m2, &n,
			 &p1, &p2);
    if (!ok)
	FatalError("Couldn't find PLL settings for mode!\n");

    fp = ((n - 2) << 16) | ((m1 - 2) << 8) | (m2 - 2);

    i830PrintPll("chosen", refclk, m1, m2, n, p1, p2);
    ErrorF("clock regs: 0x%08x, 0x%08x\n", (int)dpll, (int)fp);

    dpll = DPLL_VGA_MODE_DIS;
    if (IS_I9XX(pI830)) {
	if (is_lvds)
	    dpll |= DPLLB_MODE_LVDS;
	else
	    dpll |= DPLLB_MODE_DAC_SERIAL;
	dpll |= (1 << (p1 - 1)) << 16;
	switch (p2) {
	case 5:
	    dpll |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_5;
	    break;
	case 7:
	    dpll |= DPLLB_LVDS_P2_CLOCK_DIV_7;
	    break;
	case 10:
	    dpll |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_10;
	    break;
	case 14:
	    dpll |= DPLLB_LVDS_P2_CLOCK_DIV_14;
	    break;
	}
	if (IS_I965G(pI830))
	    dpll |= (6 << PLL_LOAD_PULSE_PHASE_SHIFT);
    } else {
	dpll |= (p1 - 2) << 16;
	if (p2 == 4)
	    dpll |= PLL_P2_DIVIDE_BY_4;
    }

    if (is_tv)
    {
	/* XXX: just matching BIOS for now */
/*	dpll |= PLL_REF_INPUT_TVCLKINBC; */
	dpll |= 3;
    }
#if 0
    else if (is_lvds)
	dpll |= PLLB_REF_INPUT_SPREADSPECTRUMIN;
#endif
    else
	dpll |= PLL_REF_INPUT_DREFCLK;

    /* Set up the display plane register */
    dspcntr = 0;
    switch (pScrn->bitsPerPixel) {
    case 8:
	dspcntr |= DISPPLANE_8BPP | DISPPLANE_GAMMA_ENABLE;
	break;
    case 16:
	if (pScrn->depth == 15)
	    dspcntr |= DISPPLANE_15_16BPP;
	else
	    dspcntr |= DISPPLANE_16BPP;
	break;
    case 32:
	dspcntr |= DISPPLANE_32BPP_NO_ALPHA;
	break;
    default:
	FatalError("unknown display bpp\n");
    }

    if (intel_crtc->gammaEnabled) {
 	dspcntr |= DISPPLANE_GAMMA_ENABLE;
    }

    if (pipe == 0)
	dspcntr |= DISPPLANE_SEL_PIPE_A;
    else
	dspcntr |= DISPPLANE_SEL_PIPE_B;

    OUTREG(fp_reg, fp);
    OUTREG(dpll_reg, dpll);
    if (IS_I965G(pI830)) {
	/* Set the SDVO multiplier/divider to 1x for the sake of analog output.
	 * It will be updated by the SDVO code if SDVO had fixed up the clock
	 * for a higher multiplier.
	 */
	OUTREG(dpll_md_reg, 0);
    }

    OUTREG(htot_reg, (adjusted_mode->CrtcHDisplay - 1) |
	((adjusted_mode->CrtcHTotal - 1) << 16));
    OUTREG(hblank_reg, (adjusted_mode->CrtcHBlankStart - 1) |
	((adjusted_mode->CrtcHBlankEnd - 1) << 16));
    OUTREG(hsync_reg, (adjusted_mode->CrtcHSyncStart - 1) |
	((adjusted_mode->CrtcHSyncEnd - 1) << 16));
    OUTREG(vtot_reg, (adjusted_mode->CrtcVDisplay - 1) |
	((adjusted_mode->CrtcVTotal - 1) << 16));
    OUTREG(vblank_reg, (adjusted_mode->CrtcVBlankStart - 1) |
	((adjusted_mode->CrtcVBlankEnd - 1) << 16));
    OUTREG(vsync_reg, (adjusted_mode->CrtcVSyncStart - 1) |
	((adjusted_mode->CrtcVSyncEnd - 1) << 16));
    OUTREG(dspstride_reg, pScrn->displayWidth * pI830->cpp);
    /* pipesrc and dspsize control the size that is scaled from, which should
     * always be the user's requested size.
     */
    OUTREG(dspsize_reg, ((mode->HDisplay - 1) << 16) | (mode->VDisplay - 1));
    OUTREG(dsppos_reg, 0);
    i830PipeSetBase(crtc, crtc->x, crtc->y);
    OUTREG(pipesrc_reg, ((mode->HDisplay - 1) << 16) | (mode->VDisplay - 1));
    temp = INREG(pipeconf_reg);
    OUTREG(pipeconf_reg, temp);
    OUTREG(dspcntr_reg, dspcntr);

    /* Disable the panel fitter if it was on our pipe */
    if (!IS_I830(pI830) && ((INREG(PFIT_CONTROL) >> 29) & 0x3) == pipe)
	OUTREG(PFIT_CONTROL, 0);
}

/**
 * Sets the given video mode on the given pipe.
 *
 * Plane A is always output to pipe A, and plane B to pipe B.  The plane
 * will not be enabled if plane_enable is FALSE, which is used for
 * load detection, when something else will be output to the pipe other than
 * display data.
 */
Bool
i830PipeSetMode(xf86CrtcPtr crtc, DisplayModePtr pMode,
		Bool plane_enable)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    int i;
    Bool ret = FALSE;
#ifdef XF86DRI
    Bool didLock = FALSE;
#endif
    DisplayModePtr adjusted_mode;

    /* XXX: curMode */

    adjusted_mode = xf86DuplicateMode(pMode);

    crtc->enabled = i830PipeInUse (crtc);
    
    if (!crtc->enabled)
    {
	/* XXX disable crtc? */
	return TRUE;
    }

#ifdef XF86DRI
    didLock = I830DRILock(pScrn);
#endif

    /* Pass our mode to the outputs and the CRTC to give them a chance to
     * adjust it according to limitations or output properties, and also
     * a chance to reject the mode entirely.
     */
    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr output = pI830->xf86_config.output[i];

	if (output->crtc != crtc)
	    continue;

	if (!output->funcs->mode_fixup(output, pMode, adjusted_mode)) {
	    ret = FALSE;
	    goto done;
	}
    }

    if (!crtc->funcs->mode_fixup(crtc, pMode, adjusted_mode)) {
	ret = FALSE;
	goto done;
    }

    /* Disable the outputs and CRTCs before setting the mode. */
    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr output = pI830->xf86_config.output[i];

	if (output->crtc != crtc)
	    continue;

	/* Disable the output as the first thing we do. */
	output->funcs->dpms(output, DPMSModeOff);
    }

    crtc->funcs->dpms(crtc, DPMSModeOff);

    /* Set up the DPLL and any output state that needs to adjust or depend
     * on the DPLL.
     */
    crtc->funcs->mode_set(crtc, pMode, adjusted_mode);
    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr output = pI830->xf86_config.output[i];
	if (output->crtc == crtc)
	    output->funcs->mode_set(output, pMode, adjusted_mode);
    }

    /* Now, enable the clocks, plane, pipe, and outputs that we set up. */
    crtc->funcs->dpms(crtc, DPMSModeOn);
    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr output = pI830->xf86_config.output[i];
	if (output->crtc == crtc)
	    output->funcs->dpms(output, DPMSModeOn);
    }

    crtc->curMode = *pMode;

    i830DumpRegs(pScrn);

    /* XXX free adjustedmode */
    ret = TRUE;
done:
#ifdef XF86DRI
    if (didLock)
	I830DRIUnlock(pScrn);
#endif
    return ret;
}

void
i830DisableUnusedFunctions(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int o, pipe;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling unused functions\n");

    for (o = 0; o < pI830->xf86_config.num_output; o++) 
    {
	xf86OutputPtr  output = pI830->xf86_config.output[o];
	if (!output->crtc)
	    (*output->funcs->dpms)(output, DPMSModeOff);
    }

    /* Now, any unused plane, pipe, and DPLL (FIXME: except for DVO, i915
     * internal TV) should have no outputs trying to pull data out of it, so
     * we're ready to turn those off.
     */
    for (pipe = 0; pipe < pI830->xf86_config.num_crtc; pipe++) 
    {
	xf86CrtcPtr crtc = pI830->xf86_config.crtc[pipe];
	I830CrtcPrivatePtr  intel_crtc = crtc->driver_private;
	int		    pipe = intel_crtc->pipe;
	int	    dspcntr_reg = pipe == 0 ? DSPACNTR : DSPBCNTR;
	int	    pipeconf_reg = pipe == 0 ? PIPEACONF : PIPEBCONF;
	int	    dpll_reg = pipe == 0 ? DPLL_A : DPLL_B;
	CARD32	    dspcntr, pipeconf, dpll;
	char	    *pipe_name = pipe == 0 ? "A" : "B";

	if (crtc->enabled)
	    continue;
	
	dspcntr = INREG(dspcntr_reg);
	if (dspcntr & DISPLAY_PLANE_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling plane %s\n",
		       pipe_name);
	    
	    OUTREG(dspcntr_reg, dspcntr & ~DISPLAY_PLANE_ENABLE);

	    /* Wait for vblank for the disable to take effect */
	    i830WaitForVblank(pScrn);
	}

	pipeconf = INREG(pipeconf_reg);
	if (pipeconf & PIPEACONF_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling pipe %s\n",
		       pipe_name);
	   OUTREG(pipeconf_reg, pipeconf & ~PIPEACONF_ENABLE);
	}

	dpll = INREG(dpll_reg);
	if (dpll & DPLL_VCO_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling DPLL %s\n",
		       pipe_name);
	    OUTREG(dpll_reg, dpll & ~DPLL_VCO_ENABLE);
	}

	memset(&crtc->curMode, 0, sizeof(crtc->curMode));
    }
}

/**
 * This function configures the screens in clone mode on
 * all active outputs using a mode similar to the specified mode.
 */
Bool
i830SetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    I830Ptr pI830 = I830PTR(pScrn);
    Bool ok = TRUE;
    int i;

    DPRINTF(PFX, "i830SetMode\n");

    for (i = 0; i < pI830->xf86_config.num_crtc; i++)
    {
	xf86CrtcPtr    crtc = pI830->xf86_config.crtc[i];
	ok = i830PipeSetMode(crtc,
			     i830PipeFindClosestMode(crtc, pMode), 
			     TRUE);
	if (!ok)
	    goto done;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Mode bandwidth is %d Mpixel/s\n",
	       (int)(pMode->HDisplay * pMode->VDisplay *
		     pMode->VRefresh / 1000000));

    if (pI830->savedCurrentMode) {
	/* We're done with the currentMode that the last randr probe had left
	 * behind, so free it.
	 */
	xfree(pI830->savedCurrentMode->name);
	xfree(pI830->savedCurrentMode);
	pI830->savedCurrentMode = NULL;
	    
	/* If we might have enabled/disabled some pipes, we need to reset
	 * cloning mode support.
	 */
	if (pI830->xf86_config.num_crtc >= 2 && 
	    pI830->xf86_config.crtc[0]->enabled &&
	    pI830->xf86_config.crtc[1]->enabled)
	    pI830->Clone = TRUE;
	else
	    pI830->Clone = FALSE;

	/* If HW cursor currently showing, reset cursor state */
	if (pI830->CursorInfoRec && !pI830->SWCursor && pI830->cursorOn)
	    pI830->CursorInfoRec->ShowCursor(pScrn);
    }

    i830DisableUnusedFunctions(pScrn);

    i830DescribeOutputConfiguration(pScrn);

#ifdef XF86DRI
   I830DRISetVBlankInterrupt (pScrn, TRUE);
#endif
done:
    i830DumpRegs (pScrn);
    i830_sdvo_dump(pScrn);
    return ok;
}

void
i830DescribeOutputConfiguration(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int i;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Output configuration:\n");

    for (i = 0; i < pI830->xf86_config.num_crtc; i++) {
	xf86CrtcPtr crtc = pI830->xf86_config.crtc[i];
	CARD32 dspcntr = INREG(DSPACNTR + (DSPBCNTR - DSPACNTR) * i);
	CARD32 pipeconf = INREG(PIPEACONF + (PIPEBCONF - PIPEACONF) * i);
	Bool hw_plane_enable = (dspcntr & DISPLAY_PLANE_ENABLE) != 0;
	Bool hw_pipe_enable = (pipeconf & PIPEACONF_ENABLE) != 0;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  Pipe %c is %s\n",
		   'A' + i, crtc->enabled ? "on" : "off");
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  Display plane %c is now %s and connected to pipe %c.\n",
		   'A' + i,
		   crtc->enabled ? "enabled" : "disabled",
		   dspcntr & DISPPLANE_SEL_PIPE_MASK ? 'B' : 'A');
	if (hw_pipe_enable != crtc->enabled) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "  Hardware claims pipe %c is %s while software "
		       "believes it is %s\n",
		       'A' + i, hw_pipe_enable ? "on" : "off",
		       crtc->enabled ? "on" : "off");
	}
	if (hw_plane_enable != crtc->enabled) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "  Hardware claims plane %c is %s while software "
		       "believes it is %s\n",
		       'A' + i, hw_plane_enable ? "on" : "off",
		       crtc->enabled ? "on" : "off");
	}
    }

    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr	output = pI830->xf86_config.output[i];
	xf86CrtcPtr	crtc = output->crtc;
	I830CrtcPrivatePtr	intel_crtc = crtc ? crtc->driver_private : NULL;
	
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "  Output %s is connected to pipe %s\n",
		   output->name, intel_crtc == NULL ? "none" :
		   (intel_crtc->pipe == 0 ? "A" : "B"));
    }
}

/**
 * Get a pipe with a simple mode set on it for doing load-based monitor
 * detection.
 *
 * It will be up to the load-detect code to adjust the pipe as appropriate for
 * its requirements.  The pipe will be connected to no other outputs.
 *
 * Currently this code will only succeed if there is a pipe with no outputs
 * configured for it.  In the future, it could choose to temporarily disable
 * some outputs to free up a pipe for its use.
 *
 * \return crtc, or NULL if no pipes are available.
 */
    
xf86CrtcPtr
i830GetLoadDetectPipe(xf86OutputPtr output)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    I830OutputPrivatePtr    intel_output = output->driver_private;
    xf86CrtcPtr	    crtc;
    int			    i;

    if (output->crtc) 
	return output->crtc;

    for (i = 0; i < pI830->xf86_config.num_crtc; i++)
	if (!i830PipeInUse(pI830->xf86_config.crtc[i]))
	    break;

    if (i == pI830->xf86_config.num_crtc)
	return NULL;

    crtc = pI830->xf86_config.crtc[i];

    output->crtc = crtc;
    intel_output->load_detect_temp = TRUE;

    return crtc;
}

void
i830ReleaseLoadDetectPipe(xf86OutputPtr output)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830OutputPrivatePtr    intel_output = output->driver_private;
    
    if (intel_output->load_detect_temp) 
    {
	output->crtc = NULL;
	intel_output->load_detect_temp = FALSE;
	i830DisableUnusedFunctions(pScrn);
    }
}

static const xf86CrtcFuncsRec i830_crtc_funcs = {
    .dpms = i830_crtc_dpms,
    .save = NULL, /* XXX */
    .restore = NULL, /* XXX */
    .mode_fixup = i830_crtc_mode_fixup,
    .mode_set = i830_crtc_mode_set,
    .destroy = NULL, /* XXX */
};

void
i830_crtc_init(ScrnInfoPtr pScrn, int pipe)
{
    xf86CrtcPtr crtc;
    I830CrtcPrivatePtr intel_crtc;

    crtc = xf86CrtcCreate (pScrn, &i830_crtc_funcs);
    if (crtc == NULL)
	return;

    intel_crtc = xnfcalloc (sizeof (I830CrtcPrivateRec), 1);
    intel_crtc->pipe = pipe;

    crtc->driver_private = intel_crtc;
}

