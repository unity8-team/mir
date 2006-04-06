#include "xf86.h"
#include "xf86_ansic.h"
#include "i830.h"

static int i830_clock(int refclk, int m1, int m2, int n, int p1, int p2)
{
    return (refclk * (5 * (m1 + 2) + (m2 + 2)) / (n + 2)) / (p1 * p2);
}

static void
i830PrintPll(char *prefix, int refclk, int m1, int m2, int n, int p1, int p2)
{
    int dotclock;

    dotclock = i830_clock(refclk, m1, m2, n, p1, p2);

    ErrorF("%s: dotclock %d ((%d, %d), %d, (%d, %d))\n", prefix, dotclock,
	   m1, m2, n, p1, p2);
}

static Bool
i830PllIsValid(int refclk, int m1, int m2, int n, int p1, int p2)
{
    int p, m, vco, dotclock;

    p = p1 + p2;
    m = 5 * (m1 + 2) + (m2 + 2);
    vco = refclk * m / (n + 2);
    dotclock = i830_clock(refclk, m1, m2, n, p1, p2);

    if (p1 < 1 || p1 > 8)
	return FALSE;
    if (p < 5 || p > 80) /* XXX: 7-98 for LVDS */
	return FALSE;
    if (m2 < 5 || m2 > 9)
	return FALSE;
    if (m1 < 10 || m1 > 20)
	return FALSE;
    if (m1 <= m2)
	return FALSE;
    if (m < 70 || m > 120)
	return FALSE;
    if (n + 2 < 3 || n + 2 > 8)	/*XXX: Is the +2 right? */
	return FALSE;
    if (vco < 1400000 || vco > 2800000)
	return FALSE;
    /* XXX: We may need to be checking "Dot clock" depending on the multiplier,
     * output, etc., rather than just a single range.
     */
    if (dotclock < 20000 || dotclock > 400000)
	return FALSE;

    return TRUE;
}

#if 0
int
i830ReadAndReportPLL(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 temp, dpll;
    int refclk, m1, m2, n, p1, p2;

    refclk = 96000;	/* XXX: The refclk may be 100000 for the LVDS */

    dpll = INREG(DPLL_A);
    switch ((dpll & DPLL_FPA01_P1_POST_DIV_MASK) >> 16) {
    case 0x01:
	p1 = 1;
	break;
    case 0x02:
	p1 = 2;
	break;
    case 0x04:
	p1 = 3;
	break;
    case 0x08:
	p1 = 4;
	break;
    case 0x10:
	p1 = 5;
	break;
    case 0x20:
	p1 = 6;
	break;
    case 0x40:
	p1 = 7;
	break;
    case 0x80:
	p1 = 8;
	break;
    default:
	FatalError("Unknown p1 clock div: 0x%x\n",
		   dpll & DPLL_FPA01_P1_POST_DIV_MASK);
    }

    switch (dpll & DPLL_P2_CLOCK_DIV_MASK) {
    case DPLL_DAC_SERIAL_P2_CLOCK_DIV_5:
	p2 = 5;
	break;
    case DPLL_DAC_SERIAL_P2_CLOCK_DIV_10:
	p2 = 10;
	break;
/* XXX:
    case DPLLB_LVDS_P2_CLOCK_DIV_7:
	p2 = 7;
	break;
    case DPLLB_LVDS_P2_CLOCK_DIV_14:
	p2 = 14;
	break;
*/
    default:
	FatalError("Unknown p2 clock div: 0x%x\n", dpll & DPLL_P2_CLOCK_DIV_MASK);
    }

    if (dpll & DISPLAY_RATE_SELECT_FPA1)
	temp = INREG(FPA1);
    else
	temp = INREG(FPA0);
    n = (temp & FP_N_DIV_MASK) >> 16;
    m1 = (temp & FP_M1_DIV_MASK) >> 8;
    m2 = (temp & FP_M2_DIV_MASK);

    i830PrintPll("FPA", refclk, m1, m2, n, p1, p2);
    ErrorF("clock settings for FPA0 look %s\n",
	   i830PllIsValid(refclk, m1, m2, n, p1, p2) ? "good" : "bad");
    ErrorF("clock regs: 0x%08x, 0x%08x\n", dpll, temp);    
}
#endif

static Bool
i830FindBestPLL(int target, int refclk, int *outm1, int *outm2, int *outn,
		int *outp1, int *outp2)
{
    int m1, m2, n, p1, p2;
    int err = target;

    if (target < 200000)	/* XXX: LVDS */
	p2 = 10;
    else
	p2 = 5;
    for (m1 = 10; m1 <= 20; m1++) {
	for (m2 = 5; m2 < 9; m2++) {
	    for (n = 1; n <= 6; n++) {
		for (p1 = 1; p1 <= 8; p1++) {
		    int clock, this_err;

		    if (!i830PllIsValid(refclk, m1, m2, n, p1, p2))
			continue;

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

/*
 * xf86SetModeCrtc
 *
 * Copied from xf86Mode.c because it's static there, and i830 likes to hand us
 * these hand-rolled modes.
 *
 * Initialises the Crtc parameters for a mode.  The initialisation includes
 * adjustments for interlaced and double scan modes.
 */
static void
xf86SetModeCrtc(DisplayModePtr p, int adjustFlags)
{
    if ((p == NULL) /* XXX: || ((p->type & M_T_CRTC_C) == M_T_BUILTIN)*/)
	return;

    p->CrtcHDisplay             = p->HDisplay;
    p->CrtcHSyncStart           = p->HSyncStart;
    p->CrtcHSyncEnd             = p->HSyncEnd;
    p->CrtcHTotal               = p->HTotal;
    p->CrtcHSkew                = p->HSkew;
    p->CrtcVDisplay             = p->VDisplay;
    p->CrtcVSyncStart           = p->VSyncStart;
    p->CrtcVSyncEnd             = p->VSyncEnd;
    p->CrtcVTotal               = p->VTotal;
    if (p->Flags & V_INTERLACE) {
	if (adjustFlags & INTERLACE_HALVE_V) {
	    p->CrtcVDisplay         /= 2;
	    p->CrtcVSyncStart       /= 2;
	    p->CrtcVSyncEnd         /= 2;
	    p->CrtcVTotal           /= 2;
	}
	/* Force interlaced modes to have an odd VTotal */
	/* maybe we should only do this when INTERLACE_HALVE_V is set? */
	p->CrtcVTotal |= 1;
    }

    if (p->Flags & V_DBLSCAN) {
        p->CrtcVDisplay         *= 2;
        p->CrtcVSyncStart       *= 2;
        p->CrtcVSyncEnd         *= 2;
        p->CrtcVTotal           *= 2;
    }
    if (p->VScan > 1) {
        p->CrtcVDisplay         *= p->VScan;
        p->CrtcVSyncStart       *= p->VScan;
        p->CrtcVSyncEnd         *= p->VScan;
        p->CrtcVTotal           *= p->VScan;
    }
    p->CrtcHAdjusted = FALSE;
    p->CrtcVAdjusted = FALSE;

    /*
     * XXX
     *
     * The following is taken from VGA, but applies to other cores as well.
     */
    p->CrtcVBlankStart = min(p->CrtcVSyncStart, p->CrtcVDisplay);
    p->CrtcVBlankEnd = max(p->CrtcVSyncEnd, p->CrtcVTotal);
    if ((p->CrtcVBlankEnd - p->CrtcVBlankStart) >= 127) {
        /* 
         * V Blanking size must be < 127.
         * Moving blank start forward is safer than moving blank end
         * back, since monitors clamp just AFTER the sync pulse (or in
         * the sync pulse), but never before.
         */
        p->CrtcVBlankStart = p->CrtcVBlankEnd - 127;
	/*
	 * If VBlankStart is now > VSyncStart move VBlankStart
	 * to VSyncStart using the maximum width that fits into
	 * VTotal.
	 */
	if (p->CrtcVBlankStart > p->CrtcVSyncStart) {
	    p->CrtcVBlankStart = p->CrtcVSyncStart;
	    p->CrtcVBlankEnd = min(p->CrtcHBlankStart + 127, p->CrtcVTotal);
	}
    }
    p->CrtcHBlankStart = min(p->CrtcHSyncStart, p->CrtcHDisplay);
    p->CrtcHBlankEnd = max(p->CrtcHSyncEnd, p->CrtcHTotal);

    if ((p->CrtcHBlankEnd - p->CrtcHBlankStart) >= 63 * 8) {
        /*
         * H Blanking size must be < 63*8. Same remark as above.
         */
        p->CrtcHBlankStart = p->CrtcHBlankEnd - 63 * 8;
	if (p->CrtcHBlankStart > p->CrtcHSyncStart) {
	    p->CrtcHBlankStart = p->CrtcHSyncStart;
	    p->CrtcHBlankEnd = min(p->CrtcHBlankStart + 63 * 8, p->CrtcHTotal);
	}
    }
}

void
i830SetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int m1, m2, n, p1, p2;
    CARD32 dpll = 0, fp = 0, temp;
    CARD32 htot, hblank, hsync, vtot, vblank, vsync;
    CARD32 pipesrc, dspsize;
    Bool ok;
    int refclk = 96000;

    xf86SetModeCrtc(pMode, 0);

    ErrorF("Requested pix clock: %d\n", pMode->Clock);

    ok = i830FindBestPLL(pMode->Clock, refclk, &m1, &m2, &n, &p1, &p2);
    if (!ok)
	FatalError("Couldn't find PLL settings for mode!\n");

    dpll = DPLL_VCO_ENABLE | DPLL_VGA_MODE_DIS;
    dpll |= DPLLB_MODE_DAC_SERIAL; /* XXX: LVDS */
    dpll |= (1 << (p1 - 1)) << 16;
    if (p2 == 5 || p2 == 7)
	dpll |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_5;
    dpll |= PLL_REF_INPUT_DREFCLK; /* XXX: TV/LVDS */
    dpll |= SDV0_DEFAULT_MULTIPLIER;

    fp = (n << 16) | (m1 << 8) | m2;

    htot = (pMode->CrtcHDisplay - 1) | ((pMode->CrtcHTotal - 1) << 16);
    hblank = (pMode->CrtcHBlankStart - 1) | ((pMode->CrtcHBlankEnd - 1) << 16);
    hsync = (pMode->CrtcHSyncStart - 1) | ((pMode->CrtcHSyncEnd - 1) << 16);
    vtot = (pMode->CrtcVDisplay - 1) | ((pMode->CrtcVTotal - 1) << 16);
    vblank = (pMode->CrtcVBlankStart - 1) | ((pMode->CrtcVBlankEnd - 1) << 16);
    vsync = (pMode->CrtcVSyncStart - 1) | ((pMode->CrtcVSyncEnd - 1) << 16);
    pipesrc = ((pMode->HDisplay - 1) << 16) | (pMode->VDisplay - 1);
    dspsize = ((pMode->VDisplay - 1) << 16) | (pMode->HDisplay - 1);

    i830PrintPll("chosen", refclk, m1, m2, n, p1, p2);
    ErrorF("clock settings for chosen look %s\n",
	   i830PllIsValid(refclk, m1, m2, n, p1, p2) ? "good" : "bad");
    ErrorF("clock regs: 0x%08x, 0x%08x\n", dpll, fp);

    /* First, disable display planes */
    temp = INREG(DSPACNTR);
    OUTREG(DSPACNTR, temp & ~DISPLAY_PLANE_ENABLE);
    temp = INREG(DSPBCNTR);
    OUTREG(DSPBCNTR, temp & ~DISPLAY_PLANE_ENABLE);

    /* Next, disable display pipes */
    temp = INREG(PIPEACONF);
    OUTREG(PIPEACONF, temp & ~PIPEACONF_ENABLE);
    temp = INREG(PIPEBCONF);
    OUTREG(PIPEBCONF, temp & ~PIPEBCONF_ENABLE);

    /* XXX: Wait for a vblank */
    sleep(1);

    /* Set up display timings and PLLs for the pipe.  XXX: Choose pipe! */
    OUTREG(FPA0, fp);
    OUTREG(DPLL_A, dpll);
    OUTREG(HTOTAL_A, htot);
    OUTREG(HBLANK_A, hblank);
    OUTREG(HSYNC_A, hsync);
    OUTREG(VTOTAL_A, vtot);
    OUTREG(VBLANK_A, vblank);
    OUTREG(VSYNC_A, vsync);
    OUTREG(DSPABASE, 0); /* XXX: Base placed elsewhere? */
    /*OUTREG(DSPASTRIDE, pScrn->displayWidth);*/
    /*OUTREG(DSPAPOS, 0);*/
    OUTREG(PIPEASRC, pipesrc);
    OUTREG(DSPASIZE, dspsize);

    /* Turn pipes and planes back on */
    /*if (pI830->planeEnabled[0]) {*/
	temp = INREG(PIPEACONF);
	OUTREG(PIPEACONF, temp | PIPEACONF_ENABLE);
	temp = INREG(DSPACNTR);
	OUTREG(DSPACNTR, temp | DISPLAY_PLANE_ENABLE);
    /*}

    if (pI830->planeEnabled[1]) {
	temp = INREG(PIPEBCONF);
	OUTREG(PIPEBCONF, temp | PIPEBCONF_ENABLE);
	temp = INREG(DSPBCNTR);
	OUTREG(DSPBCNTR, temp | DISPLAY_PLANE_ENABLE);
    }*/
}
