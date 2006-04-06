#include "xf86.h"
#include "xf86_ansic.h"
#include "i830.h"
#include "i830_display.h"

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
i830PllIsValid(ScrnInfoPtr pScrn, int outputs, int refclk, int m1, int m2,
	       int n, int p1, int p2)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int p, m, vco, dotclock;
    int min_m1, max_m1, min_m2, max_m2, min_m, max_m, min_n, max_n;
    int min_p, max_p;

    min_p = 5;
    max_p = 80;
    if (pI830->PciInfo->chipType >= PCI_CHIP_I915_G) {
	min_m1 = 10;
	max_m1 = 20;
	min_m2 = 5;
	max_m2 = 9;
	min_m = 70;
	max_m = 120;
	min_n = 3;
	max_n = 8;
	if (outputs & PIPE_LCD_ACTIVE) {
	    min_p = 7;
	    max_p = 98;
	}
    } else {
	min_m1 = 16;
	max_m1 = 24;
	min_m2 = 7;
	max_m2 = 11;
	min_m = 90;
	max_m = 130;
	min_n = 4;
	max_n = 8;
	if (outputs & PIPE_LCD_ACTIVE) {
	    min_n = 3;
	    min_m = 88;
	}
    }

    p = p1 + p2;
    m = 5 * (m1 + 2) + (m2 + 2);
    vco = refclk * m / (n + 2);
    dotclock = i830_clock(refclk, m1, m2, n, p1, p2);

    if (p1 < 1 || p1 > 8)
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
    if (n + 2 < min_n || n + 2 > max_n)	/*XXX: Is the +2 right? */
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
i830FindBestPLL(ScrnInfoPtr pScrn, int outputs, int target, int refclk,
		int *outm1, int *outm2, int *outn, int *outp1, int *outp2)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int m1, m2, n, p1, p2;
    int err = target;
    int min_m1, max_m1, min_m2, max_m2;

    if (pI830->PciInfo->chipType >= PCI_CHIP_I915_G) {
	min_m1 = 10;
	max_m1 = 20;
	min_m2 = 5;
	max_m2 = 9;
    } else {
	min_m1 = 16;
	max_m1 = 24;
	min_m2 = 7;
	max_m2 = 11;
    }

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

    for (m1 = min_m1; m1 <= max_m1; m1++) {
	for (m2 = min_m2; m2 < max_m2; m2++) {
	    for (n = 1; n <= 6; n++) {
		for (p1 = 1; p1 <= 8; p1++) {
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

    if (I830IsPrimary(pScrn))
	Start = pI830->FrontBuffer.Start;
    else {
	I830Ptr pI8301 = I830PTR(pI830->entityPrivate->pScrn_1);
	Start = pI8301->FrontBuffer2.Start;
    }

    if (pipe == 0)
	OUTREG(DSPABASE, Start + ((y * pScrn->displayWidth + x) * pI830->cpp));
    else
	OUTREG(DSPBBASE, Start + ((y * pScrn->displayWidth + x) * pI830->cpp));
}

/**
 * Sets the given video mode on the given pipe.  Assumes that plane A feeds
 * pipe A, and plane B feeds pipe B.  Should not affect the other planes/pipes.
 */
static Bool
i830PipeSetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode, int pipe)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int m1, m2, n, p1, p2;
    CARD32 dpll = 0, fp = 0, temp;
    CARD32 htot, hblank, hsync, vtot, vblank, vsync, dspcntr;
    CARD32 pipesrc, dspsize, adpa;
    Bool ok;
    int refclk = 96000;
    int outputs;

    ErrorF("Requested pix clock: %d\n", pMode->Clock);

    if (pipe == 0)
	outputs = pI830->operatingDevices & 0xff;
    else
	outputs = (pI830->operatingDevices >> 8) & 0xff;

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

    ok = i830FindBestPLL(pScrn, outputs, pMode->Clock, refclk, &m1, &m2, &n,
			 &p1, &p2);
    if (!ok) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Couldn't find PLL settings for mode!\n");
	return FALSE;
    }

    dpll = DPLL_VCO_ENABLE | DPLL_VGA_MODE_DIS;
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
    if (outputs & (PIPE_TV_ACTIVE | PIPE_TV2_ACTIVE))
	dpll |= PLL_REF_INPUT_TVCLKIN;
    else
	dpll |= PLL_REF_INPUT_DREFCLK;
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

    adpa = INREG(ADPA);
    adpa &= ~(ADPA_HSYNC_ACTIVE_HIGH | ADPA_VSYNC_ACTIVE_HIGH);
    adpa &= ~(ADPA_VSYNC_CNTL_DISABLE | ADPA_HSYNC_CNTL_DISABLE);
    adpa |= ADPA_DAC_ENABLE;
    if (pMode->Flags & V_PHSYNC)
	adpa |= ADPA_HSYNC_ACTIVE_HIGH;
    if (pMode->Flags & V_PVSYNC)
	adpa |= ADPA_VSYNC_ACTIVE_HIGH;

    i830PrintPll("chosen", refclk, m1, m2, n, p1, p2);
    ErrorF("clock settings for chosen look %s\n",
	   i830PllIsValid(pScrn, outputs, refclk, m1, m2, n, p1, p2) ?
			  "good" : "bad");
    ErrorF("clock regs: 0x%08x, 0x%08x\n", dpll, fp);

    dspcntr = DISPLAY_PLANE_ENABLE;
    switch (pScrn->bitsPerPixel) {
    case 8:
	dspcntr |= DISPPLANE_8BPP | DISPPLANE_GAMMA_ENABLE;
	break;
    case 16:
	if (pScrn->depth == 15)
	    dspcntr |= DISPPLANE_16BPP;
	else
	    dspcntr |= DISPPLANE_15_16BPP;
	break;
    case 32:
	dspcntr |= DISPPLANE_32BPP_NO_ALPHA;
	break;
    default:
	FatalError("unknown display bpp\n");
    }

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
	i830PipeSetBase(pScrn, pipe, pScrn->frameX0, pScrn->frameY0);
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
	i830PipeSetBase(pScrn, pipe, pScrn->frameX0, pScrn->frameY0);
	OUTREG(PIPEBSRC, pipesrc);

	/* Then, turn the pipe on first */
	temp = INREG(PIPEBCONF);
	OUTREG(PIPEBCONF, temp | PIPEBCONF_ENABLE);

	/* And then turn the plane on */
	OUTREG(DSPBCNTR, dspcntr);

	if (outputs & PIPE_LCD_ACTIVE)
	    i830SetLVDSPanelPower(pScrn, TRUE);
    }

    if (outputs & PIPE_CRT_ACTIVE)
	OUTREG(ADPA, adpa);

    return TRUE;
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

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Mode bandwidth is %d Mpixel/s\n",
	       (int)(pMode->HDisplay * pMode->VDisplay *
		     pMode->VRefresh / 1000000));

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

done:
#ifdef XF86DRI
    if (didLock)
	I830DRIUnlock(pScrn);
#endif

    return ok;
}

Bool
i830DetectCRT(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 temp;

    temp = INREG(PORT_HOTPLUG_EN);
    OUTREG(PORT_HOTPLUG_EN, temp | CRT_HOTPLUG_FORCE_DETECT);

    /* Wait for the bit to clear to signal detection finished. */
    while (INREG(PORT_HOTPLUG_EN) & CRT_HOTPLUG_FORCE_DETECT)
	;

    return ((INREG(PORT_HOTPLUG_STAT) & CRT_HOTPLUG_INT_STATUS));
}

/**
 * Sets the power state for the panel.
 */
void
i830SetLVDSPanelPower(ScrnInfoPtr pScrn, Bool on)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 pp_status, pp_control;

    if (on) {
	OUTREG(PP_STATUS, INREG(PP_STATUS) | PP_ON);
	OUTREG(PP_CONTROL, INREG(PP_CONTROL) | POWER_TARGET_ON);
	do {
	    pp_status = INREG(PP_STATUS);
	    pp_control = INREG(PP_CONTROL);
	} while (!(pp_status & PP_ON) && !(pp_control & POWER_TARGET_ON));
    } else {
	OUTREG(PP_STATUS, INREG(PP_STATUS) & ~PP_ON);
	OUTREG(PP_CONTROL, INREG(PP_CONTROL) & ~POWER_TARGET_ON);
	do {
	    pp_status = INREG(PP_STATUS);
	    pp_control = INREG(PP_CONTROL);
	} while ((pp_status & PP_ON) || (pp_control & POWER_TARGET_ON));
    }
}
