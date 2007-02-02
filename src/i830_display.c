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
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int		i;

    for (i = 0; i < xf86_config->num_output; i++)
    {
	xf86OutputPtr  output = xf86_config->output[i];
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
	for (clock.m2 = limit->m2.min; clock.m2 < clock.m1 && clock.m2 <= limit->m2.max; clock.m2++) 
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

    if (crtc->rotatedPixmap != NULL) {
	Start = (char *)crtc->rotatedPixmap->devPrivate.ptr -
	    (char *)pI830->FbBase;
    } else if (I830IsPrimary(pScrn)) {
	Start = pI830->FrontBuffer.Start;
    } else {
	I830Ptr pI8301 = I830PTR(pI830->entityPrivate->pScrn_1);
	Start = pI8301->FrontBuffer2.Start;
    }

    if (IS_I965G(pI830)) {
        OUTREG(dspbase, ((y * pScrn->displayWidth + x) * pI830->cpp));
	(void) INREG(dspbase);
        OUTREG(dspsurf, Start);
	(void) INREG(dspsurf);
    } else {
	OUTREG(dspbase, Start + ((y * pScrn->displayWidth + x) * pI830->cpp));
	(void) INREG(dspbase);
    }
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
 * Sets the power management mode of the pipe and plane.
 *
 * This code should probably grow support for turning the cursor off and back
 * on appropriately at the same time as we're turning the pipe off/on.
 */
static void
i830_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    I830CrtcPrivatePtr intel_crtc = crtc->driver_private;
    int pipe = intel_crtc->pipe;
    int dpll_reg = (pipe == 0) ? DPLL_A : DPLL_B;
    int dspcntr_reg = (pipe == 0) ? DSPACNTR : DSPBCNTR;
    int dspbase_reg = (pipe == 0) ? DSPABASE : DSPBBASE;
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
	if ((temp & DPLL_VCO_ENABLE) == 0)
	{
	    OUTREG(dpll_reg, temp);
	    /* Wait for the clocks to stabilize. */
	    usleep(150);
	    OUTREG(dpll_reg, temp | DPLL_VCO_ENABLE);
	    /* Wait for the clocks to stabilize. */
	    usleep(150);
	    OUTREG(dpll_reg, temp | DPLL_VCO_ENABLE);
	    /* Wait for the clocks to stabilize. */
	    usleep(150);
	}

	/* Enable the pipe */
	temp = INREG(pipeconf_reg);
	if ((temp & PIPEACONF_ENABLE) == 0)
	    OUTREG(pipeconf_reg, temp | PIPEACONF_ENABLE);

	/* Enable the plane */
	temp = INREG(dspcntr_reg);
	if ((temp & DISPLAY_PLANE_ENABLE) == 0)
	{
	    OUTREG(dspcntr_reg, temp | DISPLAY_PLANE_ENABLE);
	    /* Flush the plane changes */
	    OUTREG(dspbase_reg, INREG(dspbase_reg));
	}

	i830_crtc_load_lut(crtc);

	/* Give the overlay scaler a chance to enable if it's on this pipe */
	i830_crtc_dpms_video(crtc, TRUE);
	break;
    case DPMSModeOff:
	/* Give the overlay scaler a chance to disable if it's on this pipe */
	i830_crtc_dpms_video(crtc, FALSE);

	/* Disable the VGA plane that we never use */
	OUTREG(VGACNTRL, VGA_DISP_DISABLE);

	/* Disable display plane */
	temp = INREG(dspcntr_reg);
	if ((temp & DISPLAY_PLANE_ENABLE) != 0)
	{
	    OUTREG(dspcntr_reg, temp & ~DISPLAY_PLANE_ENABLE);
	    /* Flush the plane changes */
	    OUTREG(dspbase_reg, INREG(dspbase_reg));
	}

	if (!IS_I9XX(pI830)) {
	    /* Wait for vblank for the disable to take effect */
	    i830WaitForVblank(pScrn);
	}

	/* Next, disable display pipes */
	temp = INREG(pipeconf_reg);
	if ((temp & PIPEACONF_ENABLE) != 0)
	    OUTREG(pipeconf_reg, temp & ~PIPEACONF_ENABLE);

	/* Wait for vblank for the disable to take effect. */
	i830WaitForVblank(pScrn);

	temp = INREG(dpll_reg);
	if ((temp & DPLL_VCO_ENABLE) != 0)
	    OUTREG(dpll_reg, temp & ~DPLL_VCO_ENABLE);

	/* Wait for the clocks to turn off. */
	usleep(150);
	break;
    }
}

static Bool
i830_crtc_lock (xf86CrtcPtr crtc)
{
   /* Sync the engine before mode switch */
   i830WaitSync(crtc->scrn);

#ifdef XF86DRI
    return I830DRILock(crtc->scrn);
#else
    return FALSE;
#endif
}

static void
i830_crtc_unlock (xf86CrtcPtr crtc)
{
#ifdef XF86DRI
    I830DRIUnlock (crtc->scrn);
#endif
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
		   DisplayModePtr adjusted_mode,
		   int x, int y)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
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
    int i;
    int refclk;
    intel_clock_t clock;
    CARD32 dpll = 0, fp = 0, dspcntr, pipeconf;
    Bool ok, is_sdvo = FALSE, is_dvo = FALSE;
    Bool is_crt = FALSE, is_lvds = FALSE, is_tv = FALSE;

    /* Set up some convenient bools for what outputs are connected to
     * our pipe, used in DPLL setup.
     */
    for (i = 0; i < xf86_config->num_output; i++) {
	xf86OutputPtr  output = xf86_config->output[i];
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

    ok = i830FindBestPLL(crtc, adjusted_mode->Clock, refclk, &clock);
    if (!ok)
	FatalError("Couldn't find PLL settings for mode!\n");

    fp = clock.n << 16 | clock.m1 << 8 | clock.m2;

    dpll = DPLL_VGA_MODE_DIS;
    if (IS_I9XX(pI830)) {
	if (is_lvds)
	    dpll |= DPLLB_MODE_LVDS;
	else
	    dpll |= DPLLB_MODE_DAC_SERIAL;
	if (is_sdvo)
	{
	    dpll |= DPLL_DVO_HIGH_SPEED;
	    if (IS_I945G(pI830) || IS_I945GM(pI830))
	    {
		int sdvo_pixel_multiply = adjusted_mode->Clock / mode->Clock;
		dpll |= (sdvo_pixel_multiply - 1) << SDVO_MULTIPLIER_SHIFT_HIRES;
	    }
	}
	
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

    /* Set up the display plane register */
    dspcntr = DISPPLANE_GAMMA_ENABLE;
    switch (pScrn->bitsPerPixel) {
    case 8:
	dspcntr |= DISPPLANE_8BPP;
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

    if (pipe == 0)
	dspcntr |= DISPPLANE_SEL_PIPE_A;
    else
	dspcntr |= DISPPLANE_SEL_PIPE_B;

    pipeconf = INREG(pipeconf_reg);
    if (pipe == 0) 
    {
	/*
	 * The docs say this is needed when the dot clock is > 90% of the
	 * core speed. Core speeds are indicated by bits in the PCI
	 * config space, but that's a pain to go read, so we just guess
	 * based on the hardware age. AGP hardware is assumed to run
	 * at 133MHz while PCI-E hardware is assumed to run at 200MHz
	 */
	int core_clock;
	
	if (IS_I9XX(pI830))
	    core_clock = 200000;
	else
	    core_clock = 133000;
	
	if (mode->Clock > core_clock * 9 / 10)
	    pipeconf |= PIPEACONF_DOUBLE_WIDE;
	else
	    pipeconf &= ~PIPEACONF_DOUBLE_WIDE;
    }
#if 1
    dspcntr |= DISPLAY_PLANE_ENABLE;
    pipeconf |= PIPEACONF_ENABLE;
    dpll |= DPLL_VCO_ENABLE;
#endif

    if (is_lvds)
    {
	/* The LVDS pin pair needs to be on before the DPLLs are enabled.
	 * This is an exception to the general rule that mode_set doesn't turn
	 * things on.
	 */
	OUTREG(LVDS, INREG(LVDS) | LVDS_PORT_EN | LVDS_PIPEB_SELECT);
    }
    
    /* Disable the panel fitter if it was on our pipe */
    if (!IS_I830(pI830) && ((INREG(PFIT_CONTROL) >> 29) & 0x3) == pipe)
	OUTREG(PFIT_CONTROL, 0);

    i830PrintPll("chosen", &clock);
    ErrorF("clock regs: 0x%08x, 0x%08x\n", (int)dpll, (int)fp);

    if (dpll & DPLL_VCO_ENABLE)
    {
	OUTREG(fp_reg, fp);
	OUTREG(dpll_reg, dpll & ~DPLL_VCO_ENABLE);
	usleep(150);
    }
    OUTREG(fp_reg, fp);
    OUTREG(dpll_reg, dpll);
    /* Wait for the clocks to stabilize. */
    usleep(150);
    
    if (IS_I965G(pI830)) {
	int sdvo_pixel_multiply = adjusted_mode->Clock / mode->Clock;
	OUTREG(dpll_md_reg, (0 << DPLL_MD_UDI_DIVIDER_SHIFT) |
	       ((sdvo_pixel_multiply - 1) << DPLL_MD_UDI_MULTIPLIER_SHIFT));
    } else {
       /* write it again -- the BIOS does, after all */
       OUTREG(dpll_reg, dpll);
    }
    /* Wait for the clocks to stabilize. */
    usleep(150);

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
    OUTREG(dspsize_reg, ((mode->VDisplay - 1) << 16) | (mode->HDisplay - 1));
    OUTREG(dsppos_reg, 0);
    OUTREG(pipesrc_reg, ((mode->HDisplay - 1) << 16) | (mode->VDisplay - 1));
    OUTREG(pipeconf_reg, pipeconf);
    i830WaitForVblank(pScrn);
    
    OUTREG(dspcntr_reg, dspcntr);
    /* Flush the plane changes */
    i830PipeSetBase(crtc, x, y);
    
    i830WaitForVblank(pScrn);
}


/** Loads the palette/gamma unit for the CRTC with the prepared values */
void
i830_crtc_load_lut(xf86CrtcPtr crtc)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    I830CrtcPrivatePtr intel_crtc = crtc->driver_private;
    int palreg = (intel_crtc->pipe == 0) ? PALETTE_A : PALETTE_B;
    int i;

    /* The clocks have to be on to load the palette. */
    if (!crtc->enabled)
	return;

    for (i = 0; i < 256; i++) {
	OUTREG(palreg + 4 * i,
	       (intel_crtc->lut_r[i] << 16) |
	       (intel_crtc->lut_g[i] << 8) |
	       intel_crtc->lut_b[i]);
    }
}

/** Sets the color ramps on behalf of RandR */
static void
i830_crtc_gamma_set(xf86CrtcPtr crtc, CARD16 *red, CARD16 *green, CARD16 *blue,
		    int size)
{
    I830CrtcPrivatePtr intel_crtc = crtc->driver_private;
    int i;

    assert(size == 256);

    for (i = 0; i < 256; i++) {
	intel_crtc->lut_r[i] = red[i] >> 8;
	intel_crtc->lut_g[i] = green[i] >> 8;
	intel_crtc->lut_b[i] = blue[i] >> 8;
    }

    i830_crtc_load_lut(crtc);
}

/**
 * Creates a locked-in-framebuffer pixmap of the given width and height for
 * this CRTC's rotated shadow framebuffer.
 *
 * The current implementation uses fixed buffers allocated at startup at the
 * maximal size.
 */
static PixmapPtr
i830_crtc_shadow_create(xf86CrtcPtr crtc, int width, int height)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    ScreenPtr pScreen = pScrn->pScreen;
    I830Ptr pI830 = I830PTR(pScrn);
    I830CrtcPrivatePtr intel_crtc = crtc->driver_private;
    unsigned long rotate_pitch;
    PixmapPtr rotate_pixmap;
    unsigned long rotate_offset;
    int align = KB(4), size;

    rotate_pitch = pI830->displayWidth * pI830->cpp;
    size = rotate_pitch * height;

#ifdef I830_USE_EXA
    /* We could get close to what we want here by just creating a pixmap like
     * normal, but we have to lock it down in framebuffer, and there is no
     * setter for offscreen area locking in EXA currently.  So, we just
     * allocate offscreen memory and fake up a pixmap header for it.
     */
    if (pI830->useEXA) {
	assert(intel_crtc->rotate_mem_exa == NULL);

	intel_crtc->rotate_mem_exa = exaOffscreenAlloc(pScreen, size, align,
						       TRUE, NULL, NULL);
	if (intel_crtc->rotate_mem_exa == NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Couldn't allocate shadow memory for rotated CRTC\n");
	    return NULL;
	}
	rotate_offset = intel_crtc->rotate_mem_exa->offset;
    }
#endif /* I830_USE_EXA */
#ifdef I830_USE_XAA
    if (!pI830->useEXA) {
	/* The XFree86 linear allocator operates in units of screen pixels,
	 * sadly.
	 */
	size = (size + pI830->cpp - 1) / pI830->cpp;
	align = (align + pI830->cpp - 1) / pI830->cpp;

	assert(intel_crtc->rotate_mem_xaa == NULL);

	intel_crtc->rotate_mem_xaa =
	    i830_xf86AllocateOffscreenLinear(pScreen, size, align,
					     NULL, NULL, NULL);
	if (intel_crtc->rotate_mem_xaa == NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Couldn't allocate shadow memory for rotated CRTC\n");
	    return NULL;
	}
	rotate_offset = pI830->FrontBuffer.Start +
	    intel_crtc->rotate_mem_xaa->offset * pI830->cpp;
    }
#endif /* I830_USE_XAA */

    rotate_pixmap = GetScratchPixmapHeader(pScrn->pScreen,
					   width, height,
					   pScrn->depth,
					   pScrn->bitsPerPixel,
					   rotate_pitch,
					   pI830->FbBase + rotate_offset);
    if (rotate_pixmap == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Couldn't allocate shadow pixmap for rotated CRTC\n");
    }
    return rotate_pixmap;
}

static void
i830_crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr rotate_pixmap)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    I830CrtcPrivatePtr intel_crtc = crtc->driver_private;

    FreeScratchPixmapHeader(rotate_pixmap);
#ifdef I830_USE_EXA
    if (pI830->useEXA && intel_crtc->rotate_mem_exa != NULL) {
	exaOffscreenFree(pScrn->pScreen, intel_crtc->rotate_mem_exa);
	intel_crtc->rotate_mem_exa = NULL;
    }
#endif /* I830_USE_EXA */
#ifdef I830_USE_XAA
    if (!pI830->useEXA) {
	xf86FreeOffscreenLinear(intel_crtc->rotate_mem_xaa);
	intel_crtc->rotate_mem_xaa = NULL;
    }
#endif /* I830_USE_XAA */
}


/**
 * This function configures the screens in clone mode on
 * all active outputs using a mode similar to the specified mode.
 */
Bool
i830SetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode, Rotation rotation)
{
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(pScrn);
    Bool ok = TRUE;
    xf86CrtcPtr crtc = config->output[config->compat_output]->crtc;

    DPRINTF(PFX, "i830SetMode\n");

    if (crtc && crtc->enabled)
    {
	ok = xf86CrtcSetMode(crtc,
			     i830PipeFindClosestMode(crtc, pMode),
			     rotation, 0, 0);
	if (!ok)
	    goto done;
	crtc->desiredMode = *pMode;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Mode bandwidth is %d Mpixel/s\n",
	       (int)(pMode->HDisplay * pMode->VDisplay *
		     pMode->VRefresh / 1000000));

    xf86DisableUnusedFunctions(pScrn);

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
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    I830Ptr pI830 = I830PTR(pScrn);
    int i;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Output configuration:\n");

    for (i = 0; i < xf86_config->num_crtc; i++) {
	xf86CrtcPtr crtc = xf86_config->crtc[i];
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

    for (i = 0; i < xf86_config->num_output; i++) {
	xf86OutputPtr	output = xf86_config->output[i];
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
    xf86CrtcConfigPtr	    xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    I830OutputPrivatePtr    intel_output = output->driver_private;
    xf86CrtcPtr	    crtc;
    int			    i;

    if (output->crtc) 
	return output->crtc;

    for (i = 0; i < xf86_config->num_crtc; i++)
	if (!xf86CrtcInUse (xf86_config->crtc[i]))
	    break;

    if (i == xf86_config->num_crtc)
	return NULL;

    crtc = xf86_config->crtc[i];

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
	xf86DisableUnusedFunctions(pScrn);
    }
}

static const xf86CrtcFuncsRec i830_crtc_funcs = {
    .dpms = i830_crtc_dpms,
    .save = NULL, /* XXX */
    .restore = NULL, /* XXX */
    .lock = i830_crtc_lock,
    .unlock = i830_crtc_unlock,
    .mode_fixup = i830_crtc_mode_fixup,
    .mode_set = i830_crtc_mode_set,
    .gamma_set = i830_crtc_gamma_set,
    .shadow_create = i830_crtc_shadow_create,
    .shadow_destroy = i830_crtc_shadow_destroy,
    .destroy = NULL, /* XXX */
};

void
i830_crtc_init(ScrnInfoPtr pScrn, int pipe)
{
    xf86CrtcPtr crtc;
    I830CrtcPrivatePtr intel_crtc;
    int i;

    crtc = xf86CrtcCreate (pScrn, &i830_crtc_funcs);
    if (crtc == NULL)
	return;

    intel_crtc = xnfcalloc (sizeof (I830CrtcPrivateRec), 1);
    intel_crtc->pipe = pipe;

    /* Initialize the LUTs for when we turn on the CRTC. */
    for (i = 0; i < 256; i++) {
	intel_crtc->lut_r[i] = i;
	intel_crtc->lut_g[i] = i;
	intel_crtc->lut_b[i] = i;
    }
    crtc->driver_private = intel_crtc;
}

