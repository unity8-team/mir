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

typedef struct {
    /* given values */    
    int n;
    int m1, m2;
    int p1, p2;
    /* derived values */
    int	dot;
    int	vco;
    int	m;
    int	p;
} intel_clock_t;

typedef struct {
    int	min, max;
} intel_range_t;

typedef struct {
    int	dot_limit;
    int	p2_slow, p2_fast;
} intel_p2_t;

#define INTEL_P2_NUM		      2

typedef struct {
    intel_range_t   dot, vco, n, m, m1, m2, p, p1;
    intel_p2_t	    p2;
} intel_limit_t;

#define I8XX_DOT_MIN		  25000
#define I8XX_DOT_MAX		 350000
#define I8XX_VCO_MIN		 930000
#define I8XX_VCO_MAX		1400000
#define I8XX_N_MIN		      3
#define I8XX_N_MAX		     16
#define I8XX_M_MIN		     96
#define I8XX_M_MAX		    140
#define I8XX_M1_MIN		     18
#define I8XX_M1_MAX		     26
#define I8XX_M2_MIN		      6
#define I8XX_M2_MAX		     16
#define I8XX_P_MIN		      4
#define I8XX_P_MAX		    128
#define I8XX_P1_MIN		      0
#define I8XX_P1_MAX		     30
#define I8XX_P2_SLOW		      1
#define I8XX_P2_FAST		      0
#define I8XX_P2_SLOW_LIMIT	 165000

#define I9XX_DOT_MIN		  20000
#define I9XX_DOT_MAX		 400000
#define I9XX_VCO_MIN		1400000
#define I9XX_VCO_MAX		2800000
#define I9XX_N_MIN		      3
#define I9XX_N_MAX		      8
#define I9XX_M_MIN		     70
#define I9XX_M_MAX		    120
#define I9XX_M1_MIN		     10
#define I9XX_M1_MAX		     20
#define I9XX_M2_MIN		      5
#define I9XX_M2_MAX		      9
#define I9XX_P_SDVO_DAC_MIN	      5
#define I9XX_P_SDVO_DAC_MAX	     80
#define I9XX_P_LVDS_MIN		      7
#define I9XX_P_LVDS_MAX		     98
#define I9XX_P1_MIN		      1
#define I9XX_P1_MAX		      8
#define I9XX_P2_SDVO_DAC_SLOW		     10
#define I9XX_P2_SDVO_DAC_FAST		      5
#define I9XX_P2_SDVO_DAC_SLOW_LIMIT	 200000
#define I9XX_P2_LVDS_SLOW		     14
#define I9XX_P2_LVDS_FAST		      7
#define I9XX_P2_LVDS_SLOW_LIMIT		 112000

#define INTEL_LIMIT_I8XX	    0
#define INTEL_LIMIT_I9XX_SDVO_DAC   1
#define INTEL_LIMIT_I9XX_LVDS	    2

static const intel_limit_t intel_limits[] = {
    {
        .dot = { .min = I8XX_DOT_MIN,		.max = I8XX_DOT_MAX },
        .vco = { .min = I8XX_VCO_MIN,		.max = I8XX_VCO_MAX },
        .n   = { .min = I8XX_N_MIN,		.max = I8XX_N_MAX },
        .m   = { .min = I8XX_M_MIN,		.max = I8XX_M_MAX },
        .m1  = { .min = I8XX_M1_MIN,		.max = I8XX_M1_MAX },
        .m2  = { .min = I8XX_M2_MIN,		.max = I8XX_M2_MAX },
        .p   = { .min = I8XX_P_MIN,		.max = I8XX_P_MAX },
        .p1  = { .min = I8XX_P1_MIN,		.max = I8XX_P1_MAX },
	.p2  = { .dot_limit = I8XX_P2_SLOW_LIMIT,
		 .p2_slow = I8XX_P2_SLOW,	.p2_fast = I8XX_P2_FAST },
    },
    {
        .dot = { .min = I9XX_DOT_MIN,		.max = I9XX_DOT_MAX },
        .vco = { .min = I9XX_VCO_MIN,		.max = I9XX_VCO_MAX },
        .n   = { .min = I9XX_N_MIN,		.max = I9XX_N_MAX },
        .m   = { .min = I9XX_M_MIN,		.max = I9XX_M_MAX },
        .m1  = { .min = I9XX_M1_MIN,		.max = I9XX_M1_MAX },
        .m2  = { .min = I9XX_M2_MIN,		.max = I9XX_M2_MAX },
        .p   = { .min = I9XX_P_SDVO_DAC_MIN,	.max = I9XX_P_SDVO_DAC_MAX },
        .p1  = { .min = I9XX_P1_MIN,		.max = I9XX_P1_MAX },
	.p2  = { .dot_limit = I9XX_P2_SDVO_DAC_SLOW_LIMIT,
		 .p2_slow = I9XX_P2_SDVO_DAC_SLOW,	.p2_fast = I9XX_P2_SDVO_DAC_FAST },
    },
    {
        .dot = { .min = I9XX_DOT_MIN,		.max = I9XX_DOT_MAX },
        .vco = { .min = I9XX_VCO_MIN,		.max = I9XX_VCO_MAX },
        .n   = { .min = I9XX_N_MIN,		.max = I9XX_N_MAX },
        .m   = { .min = I9XX_M_MIN,		.max = I9XX_M_MAX },
        .m1  = { .min = I9XX_M1_MIN,		.max = I9XX_M1_MAX },
        .m2  = { .min = I9XX_M2_MIN,		.max = I9XX_M2_MAX },
        .p   = { .min = I9XX_P_LVDS_MIN,	.max = I9XX_P_LVDS_MAX },
        .p1  = { .min = I9XX_P1_MIN,		.max = I9XX_P1_MAX },
	/* The single-channel range is 25-112Mhz, and dual-channel
	 * is 80-224Mhz.  Prefer single channel as much as possible.
	 */
	.p2  = { .dot_limit = I9XX_P2_LVDS_SLOW_LIMIT,
		 .p2_slow = I9XX_P2_LVDS_SLOW,	.p2_fast = I9XX_P2_LVDS_FAST },
    },
};

static const intel_limit_t *intel_limit (xf86CrtcPtr crtc)
{
    ScrnInfoPtr	pScrn = crtc->scrn;
    I830Ptr	pI830 = I830PTR(pScrn);
    const intel_limit_t *limit;

    if (IS_I9XX(pI830)) 
    {
	if (i830PipeHasType (crtc, I830_OUTPUT_LVDS))
	    limit = &intel_limits[INTEL_LIMIT_I9XX_LVDS];
	else
	    limit = &intel_limits[INTEL_LIMIT_I9XX_SDVO_DAC];
    }
    else
        limit = &intel_limits[INTEL_LIMIT_I8XX];
    return limit;
}

/** Derive the pixel clock for the given refclk and divisors for 8xx chips. */

static void i8xx_clock(int refclk, intel_clock_t *clock)
{
    clock->m = 5 * (clock->m1 + 2) + (clock->m2 + 2);
    clock->p = (clock->p1 + 2) << (clock->p2 + 1);
    clock->vco = refclk * clock->m / (clock->n + 2);
    clock->dot = clock->vco / clock->p;
}

/** Derive the pixel clock for the given refclk and divisors for 9xx chips. */

static void i9xx_clock(int refclk, intel_clock_t *clock)
{
    clock->m = 5 * (clock->m1 + 2) + (clock->m2 + 2);
    clock->p = clock->p1 * clock->p2;
    clock->vco = refclk * clock->m / (clock->n + 2);
    clock->dot = clock->vco / clock->p;
}

static void intel_clock(I830Ptr pI830, int refclk, intel_clock_t *clock)
{
    if (IS_I9XX(pI830))
	return i9xx_clock (refclk, clock);
    else
	return i8xx_clock (refclk, clock);
}

static void
i830PrintPll(char *prefix, intel_clock_t *clock)
{
    ErrorF("%s: dotclock %d vco %d ((m %d, m1 %d, m2 %d), n %d, (p %d, p1 %d, p2 %d))\n",
	   prefix, clock->dot, clock->vco,
	   clock->m, clock->m1, clock->m2,
	   clock->n, 
	   clock->p, clock->p1, clock->p2);
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

#define i830PllInvalid(s)   { /* ErrorF (s) */; return FALSE; }
/**
 * Returns whether the given set of divisors are valid for a given refclk with
 * the given outputs.
 */

static Bool
i830PllIsValid(xf86CrtcPtr crtc, intel_clock_t *clock)
{
    const intel_limit_t *limit = intel_limit (crtc);

    if (clock->p1  < limit->p1.min  || limit->p1.max  < clock->p1)
	i830PllInvalid ("p1 out of range\n");
    if (clock->p   < limit->p.min   || limit->p.max   < clock->p)
	i830PllInvalid ("p out of range\n");
    if (clock->m2  < limit->m2.min  || limit->m2.max  < clock->m2)
	i830PllInvalid ("m2 out of range\n");
    if (clock->m1  < limit->m1.min  || limit->m1.max  < clock->m1)
	i830PllInvalid ("m1 out of range\n");
    if (clock->m1 <= clock->m2)
	i830PllInvalid ("m1 <= m2\n");
    if (clock->m   < limit->m.min   || limit->m.max   < clock->m)
	i830PllInvalid ("m out of range\n");
    if (clock->n   < limit->n.min   || limit->n.max   < clock->n)
	i830PllInvalid ("n out of range\n");
    if (clock->vco < limit->vco.min || limit->vco.max < clock->vco)
	i830PllInvalid ("vco out of range\n");
    /* XXX: We may need to be checking "Dot clock" depending on the multiplier,
     * output, etc., rather than just a single range.
     */
    if (clock->dot < limit->dot.min || limit->dot.max < clock->dot)
	i830PllInvalid ("dot out of range\n");

    return TRUE;
}

/**
 * Returns a set of divisors for the desired target clock with the given refclk,
 * or FALSE.  Divisor values are the actual divisors for
 */
static Bool
i830FindBestPLL(xf86CrtcPtr crtc, int target, int refclk, intel_clock_t *best_clock)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    intel_clock_t   clock;
    const intel_limit_t   *limit = intel_limit (crtc);
    int err = target;

    if (target < limit->p2.dot_limit)
	clock.p2 = limit->p2.p2_slow;
    else
	clock.p2 = limit->p2.p2_fast;

    memset (best_clock, 0, sizeof (*best_clock));

    for (clock.m1 = limit->m1.min; clock.m1 <= limit->m1.max; clock.m1++) 
    {
	for (clock.m2 = limit->m2.min; clock.m2 < clock.m1 && clock.m2 < limit->m2.max; clock.m2++) 
	{
	    for (clock.n = limit->n.min; clock.n <= limit->n.max; clock.n++) 
	    {
		for (clock.p1 = limit->p1.min; clock.p1 <= limit->p1.max; clock.p1++) 
		{
		    int this_err;

		    intel_clock (pI830, refclk, &clock);
		    
		    if (!i830PllIsValid(crtc, &clock))
			continue;

		    this_err = abs(clock.dot - target);
		    if (this_err < err) {
			*best_clock = clock;
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
    I830CrtcPrivatePtr intel_crtc = crtc->driver_private;
    int pipe = intel_crtc->pipe;
    intel_clock_t clock;
    CARD32 dpll = 0, fp = 0, temp;
    CARD32 htot, hblank, hsync, vtot, vblank, vsync, dspcntr;
    CARD32 pipesrc, dspsize;
    Bool ok, is_sdvo = FALSE, is_dvo = FALSE;
    Bool is_crt = FALSE, is_lvds = FALSE, is_tv = FALSE;
    int refclk, pixel_clock;
    int i;
    int dspcntr_reg = (pipe == 0) ? DSPACNTR : DSPBCNTR;
    int pipeconf_reg = (pipe == 0) ? PIPEACONF : PIPEBCONF;
    int fp_reg = (pipe == 0) ? FPA0 : FPB0;
    int dpll_reg = (pipe == 0) ? DPLL_A : DPLL_B;
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
    int pipestat_reg = (pipe == 0) ? PIPEASTAT : PIPEBSTAT;
    Bool ret = FALSE;
#ifdef XF86DRI
    Bool didLock = FALSE;
#endif

    if (xf86ModesEqual(&crtc->curMode, pMode))
	return TRUE;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Requested pix clock: %d\n",
	       pMode->Clock);

    crtc->enabled = i830PipeInUse (crtc);
    
    if (!crtc->enabled)
    {
	/* XXX disable crtc? */
	return TRUE;
    }

#ifdef XF86DRI
    didLock = I830DRILock(pScrn);
#endif
    
    for (i = 0; i < pI830->xf86_config.num_output; i++) 
    {
	xf86OutputPtr  output = pI830->xf86_config.output[i];
	I830OutputPrivatePtr	intel_output = output->driver_private;
	if (output->crtc != crtc)
	    continue;

	(*output->funcs->pre_set_mode)(output, pMode);
	
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

    if (is_lvds && (is_sdvo || is_dvo || is_tv || is_crt)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Can't enable LVDS and non-LVDS on the same pipe\n");
	goto done;
    }
    if (is_tv && (is_sdvo || is_dvo || is_crt || is_lvds)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Can't enable a TV and any other output on the same "
		   "pipe\n");
	goto done;
    }
    if (pipe == 0 && is_lvds) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Can't support LVDS on pipe A\n");
	goto done;
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

    if (is_lvds && pI830->panel_fixed_hactive != 0) {
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

	if (pMode->HDisplay <= pI830->panel_fixed_hactive &&
	    pMode->HDisplay <= pI830->panel_fixed_vactive)
	{
	    pipesrc = ((pMode->HDisplay - 1) << 16) |
		       (pMode->VDisplay - 1);
	    dspsize = ((pMode->VDisplay - 1) << 16) |
		       (pMode->HDisplay - 1);
	}
    }

    /* Adjust the clock for pixel multiplication.
     * See DPLL_MD_UDI_MULTIPLIER_MASK.
     */
    if (is_sdvo) {
	pixel_clock *= i830_sdvo_get_pixel_multiplier(pMode);
    }

    if (IS_I9XX(pI830)) {
	refclk = 96000;
    } else {
	refclk = 48000;
    }
    
    ok = i830FindBestPLL(crtc, pixel_clock, refclk, &clock);
    if (!ok) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Couldn't find PLL settings for mode!\n");
	goto done;
    }

    dpll = DPLL_VCO_ENABLE | DPLL_VGA_MODE_DIS;
    if (IS_I9XX(pI830)) {
	if (is_lvds)
	    dpll |= DPLLB_MODE_LVDS;
	else
	    dpll |= DPLLB_MODE_DAC_SERIAL;
	/* compute bitmask from p1 value */
	dpll |= (1 << (clock.p1 - 1)) << 16;
	switch (clock.p2) {
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
	dpll |= clock.p1 << 16;
	dpll |= clock.p2 << 23;
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

    fp = clock.n << 16 | clock.m1 << 8 | clock.m2;

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

    i830PrintPll("chosen", &clock);
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

    if (intel_crtc->gammaEnabled) {
 	dspcntr |= DISPPLANE_GAMMA_ENABLE;
    }

    if (pipe == 0)
	dspcntr |= DISPPLANE_SEL_PIPE_A;
    else
	dspcntr |= DISPPLANE_SEL_PIPE_B;

    OUTREG(VGACNTRL, VGA_DISP_DISABLE);

    /* Finally, set the mode. */
    /* First, disable display planes */
    temp = INREG(dspcntr_reg);
    OUTREG(dspcntr_reg, temp & ~DISPLAY_PLANE_ENABLE);

    /* Wait for vblank for the disable to take effect */
    i830WaitForVblank(pScrn);

    /* Next, disable display pipes */
    temp = INREG(pipeconf_reg);
    OUTREG(pipeconf_reg, temp & ~PIPEACONF_ENABLE);

    OUTREG(fp_reg, fp);
    OUTREG(dpll_reg, dpll);

    /*
     * If the panel fitter is stuck on our pipe, turn it off.
     * The LVDS output will set it as necessary in post_set_mode.
     */
    if (!IS_I830(pI830)) {
	if (((INREG(PFIT_CONTROL) >> 29) & 0x3) == pipe)
	    OUTREG(PFIT_CONTROL, 0);
    }

    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr  output = pI830->xf86_config.output[i];
	if (output->crtc == crtc)
	    (*output->funcs->post_set_mode)(output, pMode);
    }

    OUTREG(htot_reg, htot);
    OUTREG(hblank_reg, hblank);
    OUTREG(hsync_reg, hsync);
    OUTREG(vtot_reg, vtot);
    OUTREG(vblank_reg, vblank);
    OUTREG(vsync_reg, vsync);
    OUTREG(dspstride_reg, pScrn->displayWidth * pI830->cpp);
    OUTREG(dspsize_reg, dspsize);
    OUTREG(dsppos_reg, 0);
    i830PipeSetBase(crtc, crtc->x, crtc->y);
    OUTREG(pipesrc_reg, pipesrc);

    /* Then, turn the pipe on first */
    temp = INREG(pipeconf_reg);
    temp |= PIPEACONF_ENABLE;
    if (!IS_I9XX(pI830) && pipe == 0)
    {
	/*
	 * The docs say this is needed when the dot clock is > 90% of the
	 * core speed. Core speeds are indicated by bits in the PCI
	 * config space and don't seem to ever be less than 200MHz,
	 * which is a bit confusing.
	 *
	 * However, For one little 855/852 card I have, 135000 requires
	 * double wide mode, but 108000 does not. That makes no sense
	 * but we're used to that. It may be affected by pixel size,
	 * but the BIOS mode setting code doesn't appear to use that.
	 *
	 * It doesn't seem to cause any harm, although it
	 * does restrict some output options.
	 */
	if (pixel_clock > 108000)
	    temp |= PIPEACONF_DOUBLE_WIDE;
	else
	    temp &= ~PIPEACONF_DOUBLE_WIDE;
    }
    OUTREG(pipeconf_reg, temp);

    if (plane_enable) {
	/* And then turn the plane on */
	OUTREG(dspcntr_reg, dspcntr);
    }

#if 0
    /*
     * If the display isn't solid, it may be running out
     * of memory bandwidth. This code will dump out the
     * pipe status, if bit 31 is on, the fifo underran
     */
    for (i = 0; i < 4; i++) {
	i830WaitForVblank(pScrn);
    
	OUTREG(pipestat_reg, INREG(pipestat_reg) | 0x80000000);
    
	i830WaitForVblank(pScrn);
    
	temp = INREG(pipestat_reg);
	ErrorF ("pipe status 0x%x\n", temp);
    }
#endif
    
    crtc->curMode = *pMode;

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
