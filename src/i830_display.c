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
 * Returns whether the given set of divisors are valid for a given refclk with
 * the given outputs.
 *
 * The equation for these divisors would be:
 * clk = refclk * (5 * m1 + m2) / n / (p1 * p2)
 */
static Bool
i830PllIsValid(ScrnInfoPtr pScrn, int outputs, int refclk, int m1, int m2,
	       int n, int p1, int p2)
{
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
	if (outputs & PIPE_LCD_ACTIVE) {
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
i830FindBestPLL(ScrnInfoPtr pScrn, int outputs, int target, int refclk,
		int *outm1, int *outm2, int *outn, int *outp1, int *outp2)
{
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
	if (outputs & PIPE_LCD_ACTIVE) {
	    if (target < 200000) /* XXX: Is this the right cutoff? */
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

		    if (!i830PllIsValid(pScrn, outputs, refclk, m1, m2, n,
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

static void
i830WaitForVblank(ScrnInfoPtr pScreen)
{
    /* Wait for 20ms, i.e. one cycle at 50hz. */
    usleep(20000);
}

void
i830PipeSetBase(ScrnInfoPtr pScrn, int pipe, int x, int y)
{
    I830Ptr pI830 = I830PTR(pScrn);
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
	OUTREG(dspbase, 0);
	OUTREG(dspsurf, Start + ((y * pScrn->displayWidth + x) * pI830->cpp));
    } else {
	OUTREG(dspbase, Start + ((y * pScrn->displayWidth + x) * pI830->cpp));
    }

    pI830->pipeX[pipe] = x;
    pI830->pipeY[pipe] = y;
}

/**
 * Sets the given video mode on the given pipe.  Assumes that plane A feeds
 * pipe A, and plane B feeds pipe B.  Should not affect the other planes/pipes.
 */
Bool
i830PipeSetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode, int pipe)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int m1 = 0, m2 = 0, n = 0, p1 = 0, p2 = 0;
    CARD32 dpll = 0, fp = 0, temp;
    CARD32 htot, hblank, hsync, vtot, vblank, vsync, dspcntr;
    CARD32 pipesrc, dspsize, adpa;
    CARD32 sdvob = 0, sdvoc = 0, dvo = 0;
    Bool ok, is_sdvo, is_dvo;
    int refclk, pixel_clock, sdvo_pixel_multiply;
    int outputs;
    DisplayModePtr pMasterMode = pMode;

    assert(pMode->VRefresh != 0.0);
    /* If we've got a list of modes probed for the device, find the best match
     * in there to the requested mode.
     */
    if (pI830->pipeMon[pipe] != NULL) {
	DisplayModePtr pBest = NULL, pScan;

	assert(pScan->VRefresh != 0.0);
	for (pScan = pI830->pipeMon[pipe]->Modes; pScan != NULL;
	     pScan = pScan->next)
	{
	    /* If there's an exact match, we're done. */
	    if (I830ModesEqual(pScan, pMode)) {
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
		fabs(pBest->VRefresh - pMode->VRefresh)))
	    {
		pBest = pScan;
	    }
	}
	if (pBest != NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Choosing pipe %d's mode %dx%d@%.1f instead of xf86 "
		       "mode %dx%d@%.1f\n", pipe,
		       pBest->HDisplay, pBest->VDisplay, pBest->VRefresh,
		       pMode->HDisplay, pMode->VDisplay, pMode->VRefresh);
	    pMode = pBest;
	}
    }
    if (pipe == 0)
	outputs = pI830->operatingDevices & 0xff;
    else
	outputs = (pI830->operatingDevices >> 8) & 0xff;

    if (outputs & PIPE_LCD_ACTIVE) {
	if (I830ModesEqual(&pI830->pipeCurMode[pipe], pMasterMode))
	    return TRUE;
    } else {
	if (I830ModesEqual(&pI830->pipeCurMode[pipe], pMode))
	    return TRUE;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Requested pix clock: %d\n",
	       pMode->Clock);

    if ((outputs & PIPE_LCD_ACTIVE) && (outputs & ~PIPE_LCD_ACTIVE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Can't enable LVDS and non-LVDS on the same pipe\n");
	return FALSE;
    }
    if (((outputs & PIPE_TV_ACTIVE) && (outputs & ~PIPE_TV_ACTIVE)) ||
	((outputs & PIPE_TV2_ACTIVE) && (outputs & ~PIPE_TV2_ACTIVE))) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Can't enable a TV and any other output on the same pipe\n");
	return FALSE;
    }
    if (pipe == 0 && (outputs & PIPE_LCD_ACTIVE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Can't support LVDS on pipe A\n");
	return FALSE;
    }
    if ((outputs & PIPE_DFP_ACTIVE) || (outputs & PIPE_DFP2_ACTIVE)) {
	/* We'll change how we control outputs soon, but to get the SDVO code up
	 * and running, just check for these two possibilities.
	 */
	if (IS_I9XX(pI830)) {
	    is_sdvo = TRUE;
	    is_dvo = FALSE;
	} else {
	    is_dvo = TRUE;
	    is_sdvo = FALSE;
	}
    } else {
	is_sdvo = FALSE;
	is_dvo = FALSE;
    }

    htot = (pMode->CrtcHDisplay - 1) | ((pMode->CrtcHTotal - 1) << 16);
    hblank = (pMode->CrtcHBlankStart - 1) | ((pMode->CrtcHBlankEnd - 1) << 16);
    hsync = (pMode->CrtcHSyncStart - 1) | ((pMode->CrtcHSyncEnd - 1) << 16);
    vtot = (pMode->CrtcVDisplay - 1) | ((pMode->CrtcVTotal - 1) << 16);
    vblank = (pMode->CrtcVBlankStart - 1) | ((pMode->CrtcVBlankEnd - 1) << 16);
    vsync = (pMode->CrtcVSyncStart - 1) | ((pMode->CrtcVSyncEnd - 1) << 16);
    pipesrc = ((pMode->HDisplay - 1) << 16) | (pMode->VDisplay - 1);
    dspsize = ((pMode->VDisplay - 1) << 16) | (pMode->HDisplay - 1);
    pixel_clock = pMode->Clock;
    if (outputs & PIPE_LCD_ACTIVE && pI830->panel_fixed_hactive != 0)
    {
	/* To enable panel fitting, we need to set the pipe timings to that of
	 * the screen at its full resolution.  So, drop the timings from the
	 * BIOS VBT tables here.
	 */
	htot = (pI830->panel_fixed_hactive - 1) |
		((pI830->panel_fixed_hactive + pI830->panel_fixed_hblank - 1)
		 << 16);
	hblank = (pI830->panel_fixed_hactive - 1) |
		((pI830->panel_fixed_hactive + pI830->panel_fixed_hblank - 1)
		 << 16);
	hsync = (pI830->panel_fixed_hactive + pI830->panel_fixed_hsyncoff - 1) |
		((pI830->panel_fixed_hactive + pI830->panel_fixed_hsyncoff +
		  pI830->panel_fixed_hsyncwidth - 1) << 16);

	vtot = (pI830->panel_fixed_vactive - 1) |
		((pI830->panel_fixed_vactive + pI830->panel_fixed_vblank - 1)
		 << 16);
	vblank = (pI830->panel_fixed_vactive - 1) |
		((pI830->panel_fixed_vactive + pI830->panel_fixed_vblank - 1)
		 << 16);
	vsync = (pI830->panel_fixed_vactive + pI830->panel_fixed_vsyncoff - 1) |
		((pI830->panel_fixed_vactive + pI830->panel_fixed_vsyncoff +
		  pI830->panel_fixed_vsyncwidth - 1) << 16);
	pixel_clock = pI830->panel_fixed_clock;

	if (pMasterMode->HDisplay <= pI830->panel_fixed_hactive &&
	    pMasterMode->HDisplay <= pI830->panel_fixed_vactive)
	{
	    pipesrc = ((pMasterMode->HDisplay - 1) << 16) |
		       (pMasterMode->VDisplay - 1);
	    dspsize = ((pMasterMode->VDisplay - 1) << 16) |
		       (pMasterMode->HDisplay - 1);
	}
    }

    if (pMode->Clock >= 100000)
	sdvo_pixel_multiply = 1;
    else if (pMode->Clock >= 50000)
	sdvo_pixel_multiply = 2;
    else
	sdvo_pixel_multiply = 4;

    /* In SDVO, we need to keep the clock on the bus between 1Ghz and 2Ghz.
     * The clock on the bus is 10 times the pixel clock normally.  If that
     * would be too low, we run the DPLL at a multiple of the pixel clock, and
     * tell the SDVO device the multiplier so it can throw away the dummy bytes.
     */
    if (is_sdvo) {
	pixel_clock *= sdvo_pixel_multiply;
    }

    if (IS_I9XX(pI830)) {
	refclk = 96000;
    } else {
	refclk = 48000;
    }
    ok = i830FindBestPLL(pScrn, outputs, pixel_clock, refclk, &m1, &m2, &n,
			 &p1, &p2);
    if (!ok) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Couldn't find PLL settings for mode!\n");
	return FALSE;
    }

    dpll = DPLL_VCO_ENABLE | DPLL_VGA_MODE_DIS;
    if (IS_I9XX(pI830)) {
	if (outputs & PIPE_LCD_ACTIVE)
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
    } else {
	dpll |= (p1 - 2) << 16;
	if (p2 == 4)
	    dpll |= PLL_P2_DIVIDE_BY_4;
    }

    if (outputs & (PIPE_TV_ACTIVE | PIPE_TV2_ACTIVE))
	dpll |= PLL_REF_INPUT_TVCLKINBC;
#if 0    
    else if (outputs & (PIPE_LCD_ACTIVE))
	dpll |= PLLB_REF_INPUT_SPREADSPECTRUMIN;
#endif
    else	
	dpll |= PLL_REF_INPUT_DREFCLK;

    if (is_dvo) {
	dpll |= DPLL_DVO_HIGH_SPEED;

	/* Save the data order, since I don't know what it should be set to. */
	dvo = INREG(DVOC) & (DVO_PRESERVE_MASK | DVO_DATA_ORDER_GBRG);
	dvo |= DVO_ENABLE;
	dvo |= DVO_DATA_ORDER_FP | DVO_BORDER_ENABLE | DVO_BLANK_ACTIVE_HIGH;

	if (pipe == 1)
	    dvo |= DVO_PIPE_B_SELECT;

	if (pMode->Flags & V_PHSYNC)
	    dvo |= DVO_HSYNC_ACTIVE_HIGH;
	if (pMode->Flags & V_PVSYNC)
	    dvo |= DVO_VSYNC_ACTIVE_HIGH;

	OUTREG(DVOC, dvo & ~DVO_ENABLE);
    }

    if (is_sdvo) {
	dpll |= DPLL_DVO_HIGH_SPEED;

	ErrorF("DVOB: %08x\nDVOC: %08x\n", (int)INREG(SDVOB), (int)INREG(SDVOC));

        sdvob = INREG(SDVOB) & SDVOB_PRESERVE_MASK;
	sdvoc = INREG(SDVOC) & SDVOC_PRESERVE_MASK;
	sdvob |= SDVO_ENABLE | (9 << 19) | SDVO_BORDER_ENABLE;
	sdvoc |= 9 << 19;
	if (pipe == 1)
	    sdvob |= SDVO_PIPE_B_SELECT;

	if (IS_I945G(pI830) || IS_I945GM(pI830))
	    dpll |= (sdvo_pixel_multiply - 1) << SDVO_MULTIPLIER_SHIFT_HIRES;
	else
	    sdvob |= (sdvo_pixel_multiply - 1) << SDVO_PORT_MULTIPLY_SHIFT;

	OUTREG(SDVOC, INREG(SDVOC) & ~SDVO_ENABLE);
	OUTREG(SDVOB, INREG(SDVOB) & ~SDVO_ENABLE);
    }

    fp = ((n - 2) << 16) | ((m1 - 2) << 8) | (m2 - 2);

#if 1
    ErrorF("hact: %d htot: %d hbstart: %d hbend: %d hsyncstart: %d hsyncend: %d\n",
	(int)(htot & 0xffff) + 1, (int)(htot >> 16) + 1,
	(int)(hblank & 0xffff) + 1, (int)(hblank >> 16) + 1,
	(int)(hsync & 0xffff) + 1, (int)(hsync >> 16) + 1);
    ErrorF("vact: %d vtot: %d vbstart: %d vbend: %d vsyncstart: %d vsyncend: %d\n",
	(int)(vtot & 0xffff) + 1, (int)(vtot >> 16) + 1,
	(int)(vblank & 0xffff) + 1, (int)(vblank >> 16) + 1,
	(int)(vsync & 0xffff) + 1, (int)(vsync >> 16) + 1);
    ErrorF("pipesrc: %dx%d, dspsize: %dx%d\n",
	(int)(pipesrc >> 16) + 1, (int)(pipesrc & 0xffff) + 1,
	(int)(dspsize & 0xffff) + 1, (int)(dspsize >> 16) + 1);
#endif

    i830PrintPll("chosen", refclk, m1, m2, n, p1, p2);
    ErrorF("clock regs: 0x%08x, 0x%08x\n", (int)dpll, (int)fp);

    dspcntr = DISPLAY_PLANE_ENABLE;
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

    if (pI830->gammaEnabled[pipe]) {
 	dspcntr |= DISPPLANE_GAMMA_ENABLE;
    }

    if (is_sdvo)
	adpa = ADPA_DAC_DISABLE;
    else
	adpa = ADPA_DAC_ENABLE;
    if (pMode->Flags & V_PHSYNC)
	adpa |= ADPA_HSYNC_ACTIVE_HIGH;
    if (pMode->Flags & V_PVSYNC)
	adpa |= ADPA_VSYNC_ACTIVE_HIGH;
    
    if (pipe == 0) {
	dspcntr |= DISPPLANE_SEL_PIPE_A;
	adpa |= ADPA_PIPE_A_SELECT;
    } else {
	dspcntr |= DISPPLANE_SEL_PIPE_B;
	adpa |= ADPA_PIPE_B_SELECT;
    }

    OUTREG(VGACNTRL, VGA_DISP_DISABLE);

    /* Set up display timings and PLLs for the pipe. */
    if (pipe == 0) {
	/* First, disable display planes */
	temp = INREG(DSPACNTR);
	OUTREG(DSPACNTR, temp & ~DISPLAY_PLANE_ENABLE);

	/* Wait for vblank for the disable to take effect */
	i830WaitForVblank(pScrn);

	/* Next, disable display pipes */
	temp = INREG(PIPEACONF);
	OUTREG(PIPEACONF, temp & ~PIPEACONF_ENABLE);

	OUTREG(FPA0, fp);
	OUTREG(DPLL_A, dpll);

	OUTREG(HTOTAL_A, htot);
	OUTREG(HBLANK_A, hblank);
	OUTREG(HSYNC_A, hsync);
	OUTREG(VTOTAL_A, vtot);
	OUTREG(VBLANK_A, vblank);
	OUTREG(VSYNC_A, vsync);
	OUTREG(DSPASTRIDE, pScrn->displayWidth * pI830->cpp);
	OUTREG(DSPASIZE, dspsize);
	OUTREG(DSPAPOS, 0);
	i830PipeSetBase(pScrn, pipe, pI830->pipeX[pipe], pI830->pipeX[pipe]);
	OUTREG(PIPEASRC, pipesrc);

	/* Then, turn the pipe on first */
	temp = INREG(PIPEACONF);
	OUTREG(PIPEACONF, temp | PIPEACONF_ENABLE);

	/* And then turn the plane on */
	OUTREG(DSPACNTR, dspcntr);
    } else {
	/* Always make sure the LVDS is off before we play with DPLLs and pipe
	 * configuration.
	 */
	i830SetLVDSPanelPower(pScrn, FALSE);

	/* First, disable display planes */
	temp = INREG(DSPBCNTR);
	OUTREG(DSPBCNTR, temp & ~DISPLAY_PLANE_ENABLE);

	/* Wait for vblank for the disable to take effect */
	i830WaitForVblank(pScrn);

	/* Next, disable display pipes */
	temp = INREG(PIPEBCONF);
	OUTREG(PIPEBCONF, temp & ~PIPEBCONF_ENABLE);

	if (outputs & PIPE_LCD_ACTIVE) {
	    /* Disable the PLL before messing with LVDS enable */
	    OUTREG(FPB0, fp & ~DPLL_VCO_ENABLE);

	    /* LVDS must be powered on before PLL is enabled and before power
	     * sequencing the panel.
	     */
	    temp = INREG(LVDS);
	    OUTREG(LVDS, temp | LVDS_PORT_EN | LVDS_PIPEB_SELECT);
	}

	OUTREG(FPB0, fp);
	OUTREG(DPLL_B, dpll);
	OUTREG(HTOTAL_B, htot);
	OUTREG(HBLANK_B, hblank);
	OUTREG(HSYNC_B, hsync);
	OUTREG(VTOTAL_B, vtot);
	OUTREG(VBLANK_B, vblank);
	OUTREG(VSYNC_B, vsync);
	OUTREG(DSPBSTRIDE, pScrn->displayWidth * pI830->cpp);
	OUTREG(DSPBSIZE, dspsize);
	OUTREG(DSPBPOS, 0);
	i830PipeSetBase(pScrn, pipe, pI830->pipeX[pipe], pI830->pipeY[pipe]);
	OUTREG(PIPEBSRC, pipesrc);

	if (outputs & PIPE_LCD_ACTIVE) {
	    CARD32  pfit_control;
	    
	    /* Enable automatic panel scaling so that non-native modes fill the
	     * screen.
	     */
	    /* XXX: Allow (auto-?) enabling of 8-to-6 dithering */
	    pfit_control = (PFIT_ENABLE |
			    VERT_AUTO_SCALE | HORIZ_AUTO_SCALE |
			    VERT_INTERP_BILINEAR | HORIZ_INTERP_BILINEAR);
	    if (pI830->panel_wants_dither)
		pfit_control |= PANEL_8TO6_DITHER_ENABLE;
	    OUTREG(PFIT_CONTROL, pfit_control);
	}

	/* Then, turn the pipe on first */
	temp = INREG(PIPEBCONF);
	OUTREG(PIPEBCONF, temp | PIPEBCONF_ENABLE);

	/* And then turn the plane on */
	OUTREG(DSPBCNTR, dspcntr);

	if (outputs & PIPE_LCD_ACTIVE) {
	    i830SetLVDSPanelPower(pScrn, TRUE);
	}
    }

    if (outputs & PIPE_CRT_ACTIVE)
	OUTREG(ADPA, adpa);

    if (is_dvo) {
	/*OUTREG(DVOB_SRCDIM, (pMode->HDisplay << DVO_SRCDIM_HORIZONTAL_SHIFT) |
	    (pMode->VDisplay << DVO_SRCDIM_VERTICAL_SHIFT));*/
	OUTREG(DVOC_SRCDIM, (pMode->HDisplay << DVO_SRCDIM_HORIZONTAL_SHIFT) |
	    (pMode->VDisplay << DVO_SRCDIM_VERTICAL_SHIFT));
	/*OUTREG(DVOB, dvo);*/
	OUTREG(DVOC, dvo);
    }

    if (is_sdvo) {
	OUTREG(SDVOB, sdvob);
	OUTREG(SDVOC, sdvoc);
    }

    if (outputs & PIPE_LCD_ACTIVE) {
	pI830->pipeCurMode[pipe] = *pMasterMode;
    } else {
	pI830->pipeCurMode[pipe] = *pMode;
    }

    return TRUE;
}

void
i830DisableUnusedFunctions(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int outputsA, outputsB;

    outputsA = pI830->operatingDevices & 0xff;
    outputsB = (pI830->operatingDevices >> 8) & 0xff;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling unused functions\n");

    /* First, disable the unused outputs */
    if ((outputsA & PIPE_CRT_ACTIVE) == 0 &&
	(outputsB & PIPE_CRT_ACTIVE) == 0)
    {
	CARD32 adpa = INREG(ADPA);

	if (adpa & ADPA_DAC_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling CRT output\n");
	    OUTREG(ADPA, adpa & ~ADPA_DAC_ENABLE);
	}
    }

    if ((outputsB & PIPE_LCD_ACTIVE) == 0) {
	CARD32 pp_status = INREG(PP_STATUS);

	if (pp_status & PP_ON) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling LVDS output\n");
	    i830SetLVDSPanelPower(pScrn, FALSE);
	}
    }

    if (IS_I9XX(pI830) && ((outputsA & PIPE_DFP_ACTIVE) == 0 &&
	(outputsB & PIPE_DFP_ACTIVE) == 0))
    {
	CARD32 sdvob = INREG(SDVOB);
	CARD32 sdvoc = INREG(SDVOC);

	if (sdvob & SDVO_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling SDVOB output\n");
	    OUTREG(SDVOB, sdvob & ~SDVO_ENABLE);
	}
	if (sdvoc & SDVO_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling SDVOC output\n");
	    OUTREG(SDVOC, sdvoc & ~SDVO_ENABLE);
	}
    }

    /* Now, any unused plane, pipe, and DPLL (FIXME: except for DVO, i915
     * internal TV) should have no outputs trying to pull data out of it, so
     * we're ready to turn those off.
     */
    if (!pI830->planeEnabled[0]) {
	CARD32 dspcntr, pipeconf, dpll;

	dspcntr = INREG(DSPACNTR);
	if (dspcntr & DISPLAY_PLANE_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling plane A\n");
	    OUTREG(DSPACNTR, dspcntr & ~DISPLAY_PLANE_ENABLE);

	    /* Wait for vblank for the disable to take effect */
	    i830WaitForVblank(pScrn);
	}

	pipeconf = INREG(PIPEACONF);
	if (pipeconf & PIPEACONF_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling pipe A\n");
	   OUTREG(PIPEACONF, pipeconf & ~PIPEACONF_ENABLE);
	}

	dpll = INREG(DPLL_A);
	if (dpll & DPLL_VCO_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling DPLL A\n");
	    OUTREG(DPLL_A, dpll & ~DPLL_VCO_ENABLE);
	}

	memset(&pI830->pipeCurMode[0], 0, sizeof(pI830->pipeCurMode[0]));
    }

    if (!pI830->planeEnabled[1]) {
	CARD32 dspcntr, pipeconf, dpll;

	dspcntr = INREG(DSPBCNTR);
	if (dspcntr & DISPLAY_PLANE_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling plane B\n");
	    OUTREG(DSPBCNTR, dspcntr & ~DISPLAY_PLANE_ENABLE);

	    /* Wait for vblank for the disable to take effect */
	    i830WaitForVblank(pScrn);
	}

	pipeconf = INREG(PIPEBCONF);
	if (pipeconf & PIPEBCONF_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling pipe B\n");
	   OUTREG(PIPEBCONF, pipeconf & ~PIPEBCONF_ENABLE);
	}

	dpll = INREG(DPLL_B);
	if (dpll & DPLL_VCO_ENABLE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling DPLL B\n");
	    OUTREG(DPLL_B, dpll & ~DPLL_VCO_ENABLE);
	}

	memset(&pI830->pipeCurMode[1], 0, sizeof(pI830->pipeCurMode[1]));
    }
}

/**
 * This function sets the given mode on the active pipes.
 */
Bool
i830SetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    I830Ptr pI830 = I830PTR(pScrn);
    Bool ok = TRUE;
    CARD32 planeA, planeB;
#ifdef XF86DRI
    Bool didLock = FALSE;
#endif
    int i;

    DPRINTF(PFX, "i830SetMode\n");

#ifdef XF86DRI
    didLock = I830DRILock(pScrn);
#endif

    if (pI830->operatingDevices & 0xff) {
	pI830->planeEnabled[0] = 1;
    } else {
	pI830->planeEnabled[0] = 0;
    }

    if (pI830->operatingDevices & 0xff00) {
	pI830->planeEnabled[1] = 1;
    } else {
	pI830->planeEnabled[1] = 0;
    }

    for (i = 0; i < pI830->num_outputs; i++) {
	struct _I830OutputRec *output = &pI830->output[i];

	if (output->sdvo_drv)
	    I830SDVOPreSetMode(output->sdvo_drv, pMode);

	if (output->i2c_drv != NULL)
	    output->i2c_drv->vid_rec->Mode(output->i2c_drv->dev_priv,
					   pMode);
    }

    if (pI830->planeEnabled[0]) {
	ok = i830PipeSetMode(pScrn, pMode, 0);
	if (!ok)
	    goto done;
    }
    if (pI830->planeEnabled[1]) {
	ok = i830PipeSetMode(pScrn, pMode, 1);
	if (!ok)
	    goto done;
    }
    for (i = 0; i < pI830->num_outputs; i++) {
	if (pI830->output[i].sdvo_drv)
	    I830SDVOPostSetMode(pI830->output[i].sdvo_drv, pMode);
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
	if ((pI830->operatingDevices & 0x00ff) &&
	    (pI830->operatingDevices & 0xff00))
	{
	    pI830->Clone = TRUE;
	} else {
	    pI830->Clone = FALSE;
	}

	/* If HW cursor currently showing, reset cursor state */
	if (pI830->CursorInfoRec && !pI830->SWCursor && pI830->cursorOn)
	    pI830->CursorInfoRec->ShowCursor(pScrn);
    }

    i830DisableUnusedFunctions(pScrn);

    planeA = INREG(DSPACNTR);
    planeB = INREG(DSPBCNTR);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Display plane A is now %s and connected to %s.\n",
	       pI830->planeEnabled[0] ? "enabled" : "disabled",
	       planeA & DISPPLANE_SEL_PIPE_MASK ? "Pipe B" : "Pipe A");
    if (pI830->availablePipes == 2)
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Display plane B is now %s and connected to %s.\n",
		   pI830->planeEnabled[1] ? "enabled" : "disabled",
		   planeB & DISPPLANE_SEL_PIPE_MASK ? "Pipe B" : "Pipe A");

#ifdef XF86DRI
   I830DRISetVBlankInterrupt (pScrn, TRUE);
#endif
done:
#ifdef XF86DRI
    if (didLock)
	I830DRIUnlock(pScrn);
#endif

    i830DumpRegs (pScrn);
    I830DumpSDVO (pScrn);
    return ok;
}

/**
 * Uses CRT_HOTPLUG_EN and CRT_HOTPLUG_STAT to detect CRT presence.
 *
 * Only for I945G/GM.
 */
static Bool
i830HotplugDetectCRT(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 temp;
    const int timeout_ms = 1000;
    int starttime, curtime;

    temp = INREG(PORT_HOTPLUG_EN);

    OUTREG(PORT_HOTPLUG_EN, temp | CRT_HOTPLUG_FORCE_DETECT | (1 << 5));

    for (curtime = starttime = GetTimeInMillis();
	 (curtime - starttime) < timeout_ms; curtime = GetTimeInMillis())
    {
	if ((INREG(PORT_HOTPLUG_EN) & CRT_HOTPLUG_FORCE_DETECT) == 0)
	    break;
    }

    if ((INREG(PORT_HOTPLUG_STAT) & CRT_HOTPLUG_MONITOR_MASK) ==
	CRT_HOTPLUG_MONITOR_COLOR)
    {
	return TRUE;
    } else {
	return FALSE;
    }
}

/**
 * Detects CRT presence by checking for load.
 *
 * Requires that the current pipe's DPLL is active.  This will cause flicker
 * on the CRT, so it should not be used while the display is being used.  Only
 * color (not monochrome) displays are detected.
 */
static Bool
i830LoadDetectCRT(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 adpa, pipeconf;
    CARD8 st00;
    int pipeconf_reg, bclrpat_reg, dpll_reg;
    int pipe;

    pipe = pI830->pipe;
    if (pipe == 0) {
	bclrpat_reg = BCLRPAT_A;
	pipeconf_reg = PIPEACONF;
	dpll_reg = DPLL_A;
    } else {
	bclrpat_reg = BCLRPAT_B;
	pipeconf_reg = PIPEBCONF;
	dpll_reg = DPLL_B;
    }

    /* Don't try this if the DPLL isn't running. */
    if (!(INREG(dpll_reg) & DPLL_VCO_ENABLE))
	return FALSE;

    adpa = INREG(ADPA);

    /* Enable CRT output if disabled. */
    if (!(adpa & ADPA_DAC_ENABLE)) {
	OUTREG(ADPA, adpa | ADPA_DAC_ENABLE |
	       ((pipe == 1) ? ADPA_PIPE_B_SELECT : 0));
    }

    /* Set the border color to red, green.  Maybe we should save/restore this
     * reg.
     */
    OUTREG(bclrpat_reg, 0x00500050);

    /* Force the border color through the active region */
    pipeconf = INREG(pipeconf_reg);
    OUTREG(pipeconf_reg, pipeconf | PIPECONF_FORCE_BORDER);

    /* Read the ST00 VGA status register */
    st00 = pI830->readStandard(pI830, 0x3c2);

    /* Restore previous settings */
    OUTREG(pipeconf_reg, pipeconf);
    OUTREG(ADPA, adpa);

    if (st00 & (1 << 4))
	return TRUE;
    else
	return FALSE;
}

/**
 * Detects CRT presence by probing for a response on the DDC address.
 *
 * This takes approximately 5ms in testing on an i915GM, with CRT connected or
 * not.
 */
static Bool
i830DDCDetectCRT(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    struct _I830OutputRec *output;

    output = &pI830->output[0];
    /* CRT should always be at 0, but check anyway */
    if (output->type != I830_OUTPUT_ANALOG)
	return FALSE;

    return xf86I2CProbeAddress(output->pDDCBus, 0x00A0);
}

/**
 * Attempts to detect CRT presence through any method available.
 *
 * @param allow_disturb enables detection methods that may cause flickering
 *        on active displays.
 */
Bool
i830DetectCRT(ScrnInfoPtr pScrn, Bool allow_disturb)
{
    I830Ptr pI830 = I830PTR(pScrn);
    Bool found_ddc;

    if (IS_I945G(pI830) || IS_I945GM(pI830))
	return i830HotplugDetectCRT(pScrn);

    found_ddc = i830DDCDetectCRT(pScrn);
    if (found_ddc)
	return TRUE;

    /* Use the load-detect method if we're not currently outputting to the CRT,
     * or we don't care.
     *
     * Actually, the method is unreliable currently.  We need to not share a
     * pipe, as it seems having other outputs on that pipe will result in a
     * false positive.
     */
    if (0 && (allow_disturb || !(INREG(ADPA) & !ADPA_DAC_ENABLE))) {
	return i830LoadDetectCRT(pScrn);
    }

    return FALSE;
}

/**
 * Sets the power state for the panel.
 */
void
i830SetLVDSPanelPower(ScrnInfoPtr pScrn, Bool on)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 pp_status, pp_control;
    CARD32 blc_pwm_ctl;
    int backlight_duty_cycle;

    blc_pwm_ctl = INREG (BLC_PWM_CTL);
    backlight_duty_cycle = blc_pwm_ctl & BACKLIGHT_DUTY_CYCLE_MASK;
    if (backlight_duty_cycle)
        pI830->backlight_duty_cycle = backlight_duty_cycle;
    
    if (on) {
	OUTREG(PP_STATUS, INREG(PP_STATUS) | PP_ON);
	OUTREG(PP_CONTROL, INREG(PP_CONTROL) | POWER_TARGET_ON);
	do {
	    pp_status = INREG(PP_STATUS);
	    pp_control = INREG(PP_CONTROL);
	} while (!(pp_status & PP_ON) && !(pp_control & POWER_TARGET_ON));
	OUTREG(BLC_PWM_CTL,
	       (blc_pwm_ctl & ~BACKLIGHT_DUTY_CYCLE_MASK) |
	       pI830->backlight_duty_cycle);
    } else {
	OUTREG(BLC_PWM_CTL,
	       (blc_pwm_ctl & ~BACKLIGHT_DUTY_CYCLE_MASK));
	       
	OUTREG(PP_STATUS, INREG(PP_STATUS) & ~PP_ON);
	OUTREG(PP_CONTROL, INREG(PP_CONTROL) & ~POWER_TARGET_ON);
	do {
	    pp_status = INREG(PP_STATUS);
	    pp_control = INREG(PP_CONTROL);
	} while ((pp_status & PP_ON) || (pp_control & POWER_TARGET_ON));
    }
}
