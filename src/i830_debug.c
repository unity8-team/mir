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

#include "xf86.h"
#include "i830.h"
#include "i830_debug.h"

#define DEBUGSTRING(func) static char *func(I830Ptr pI830, int reg, CARD32 val)

DEBUGSTRING(i830_debug_xyminus1)
{
    return XNFprintf("%d, %d", (val & 0xffff) + 1,
		     ((val & 0xffff0000) >> 16) + 1);
}

DEBUGSTRING(i830_debug_xy)
{
    return XNFprintf("%d, %d", (val & 0xffff),
		     ((val & 0xffff0000) >> 16));
}

DEBUGSTRING(i830_debug_dspstride)
{
    return XNFprintf("%d bytes", val);
}

DEBUGSTRING(i830_debug_dspcntr)
{
    char *enabled = val & DISPLAY_PLANE_ENABLE ? "enabled" : "disabled";
    char plane = val & DISPPLANE_SEL_PIPE_B ? 'B' : 'A';
    return XNFprintf("%s, pipe %c", enabled, plane);
}

DEBUGSTRING(i830_debug_pipeconf)
{
    char *enabled = val & PIPEACONF_ENABLE ? "enabled" : "disabled";
    char *wide = val & PIPEACONF_DOUBLE_WIDE ? "double-wide" : "single-wide";
    return XNFprintf("%s, %s", enabled, wide);
}

DEBUGSTRING(i830_debug_hvtotal)
{
    return XNFprintf("%d active, %d total", (val & 0xffff) + 1,
		     ((val & 0xffff0000) >> 16) + 1);
}

DEBUGSTRING(i830_debug_hvsyncblank)
{
    return XNFprintf("%d start, %d end", (val & 0xffff) + 1,
		     ((val & 0xffff0000) >> 16) + 1);
}

DEBUGSTRING(i830_debug_vgacntrl)
{
    return XNFprintf("%s", val & VGA_DISP_DISABLE ? "disabled" : "enabled");
}

DEBUGSTRING(i830_debug_fp)
{
    return XNFprintf("n = %d, m1 = %d, m2 = %d",
		     ((val & FP_N_DIV_MASK) >> FP_N_DIV_SHIFT),
		     ((val & FP_M1_DIV_MASK) >> FP_M1_DIV_SHIFT),
		     ((val & FP_M2_DIV_MASK) >> FP_M2_DIV_SHIFT));
}

DEBUGSTRING(i830_debug_pp_status)
{
    char *status = val & PP_ON ? "on" : "off";
    char *ready = val & PP_READY ? "ready" : "not ready";
    char *seq = "unknown";

    switch (val & PP_SEQUENCE_MASK) {
    case PP_SEQUENCE_NONE:
	seq = "idle";
	break;
    case PP_SEQUENCE_ON:
	seq = "on";
	break;
    case PP_SEQUENCE_OFF:
	seq = "off";
	break;
    }

    return XNFprintf("%s, %s, sequencing %s", status, ready, seq);
}

DEBUGSTRING(i830_debug_pp_control)
{
    return XNFprintf("power target: %s",
		     val & POWER_TARGET_ON ? "on" : "off");
}

DEBUGSTRING(i830_debug_dpll)
{
    char *enabled = val & DPLL_VCO_ENABLE ? "enabled" : "disabled";
    char *dvomode = val & DPLL_DVO_HIGH_SPEED ? "dvo" : "non-dvo";
    char *vgamode = val & DPLL_VGA_MODE_DIS ? "" : ", VGA";
    char *mode = "unknown";
    char *clock = "unknown";
    char *fpextra = val & DISPLAY_RATE_SELECT_FPA1 ? ", using FPx1!" : "";
    char sdvoextra[3];
    int p1, p2 = 0;

    p1 = ffs((val & DPLL_FPA01_P1_POST_DIV_MASK) >>
	     DPLL_FPA01_P1_POST_DIV_SHIFT);
    switch (val & DPLL_MODE_MASK) {
    case DPLLB_MODE_DAC_SERIAL:
	mode = "dac/serial";
	p2 = val & DPLL_DAC_SERIAL_P2_CLOCK_DIV_5 ? 5 : 10;
	break;
    case DPLLB_MODE_LVDS:
	mode = "LVDS";
	p2 = val & DPLLB_LVDS_P2_CLOCK_DIV_7 ? 7 : 14;
	break;
    }
    switch (val & PLL_REF_INPUT_MASK) {
    case PLL_REF_INPUT_DREFCLK:
	clock = "default";
	break;
    case PLL_REF_INPUT_TVCLKINA:
	clock = "TV A";
	break;
    case PLL_REF_INPUT_TVCLKINBC:
	clock = "TV B/C";
	break;
    }
    if (IS_I945G(pI830) || IS_I945GM(pI830)) {
	sprintf(sdvoextra, "SDVO mult %d",
		(int)(val & SDVO_MULTIPLIER_MASK) >>
		SDVO_MULTIPLIER_SHIFT_HIRES);
    } else {
	sdvoextra[0] = '\0';
    }

    return XNFprintf("%s, %s%s, %s mode, %s clock, p1 = %d, "
		     "p2 = %d%s%s",
		     enabled, dvomode, vgamode, mode, clock, p1, p2,
		     fpextra, sdvoextra);
}

DEBUGSTRING(i830_debug_lvds)
{
    char pipe = val & LVDS_PIPEB_SELECT ? 'B' : 'A';
    char *enable = val & LVDS_PORT_EN ? "enabled" : "disabled";

    return XNFprintf("%s, pipe %c", enable, pipe);
}

#define DEFINEREG(reg) \
	{ reg, #reg, NULL, 0 }
#define DEFINEREG2(reg, func) \
	{ reg, #reg, func, 0 }

static struct i830SnapshotRec {
    int reg;
    char *name;
    char *(*debug_output)(I830Ptr pI830, int reg, CARD32 val);
    CARD32 val;
} i830_snapshot[] = {
    DEFINEREG(VCLK_DIVISOR_VGA0),
    DEFINEREG(VCLK_DIVISOR_VGA1),
    DEFINEREG(VCLK_POST_DIV),
    DEFINEREG(DPLL_TEST),
    DEFINEREG(D_STATE),
    DEFINEREG(DSPCLK_GATE_D),
    DEFINEREG(RENCLK_GATE_D1),
    DEFINEREG(RENCLK_GATE_D2),
/*  DEFINEREG(RAMCLK_GATE_D),	CRL only */
    DEFINEREG(SDVOB),
    DEFINEREG(SDVOC),
/*    DEFINEREG(UDIB_SVB_SHB_CODES), CRL only */
/*    DEFINEREG(UDIB_SHA_BLANK_CODES), CRL only */
    DEFINEREG(SDVOUDI),
    DEFINEREG(DSPARB),
    DEFINEREG(DSPFW1),
    DEFINEREG(DSPFW2),
    DEFINEREG(DSPFW3),

    DEFINEREG(ADPA),
    DEFINEREG2(LVDS, i830_debug_lvds),
    DEFINEREG(DVOA),
    DEFINEREG(DVOB),
    DEFINEREG(DVOC),
    DEFINEREG(DVOA_SRCDIM),
    DEFINEREG(DVOB_SRCDIM),
    DEFINEREG(DVOC_SRCDIM),

    DEFINEREG2(PP_CONTROL, i830_debug_pp_control),
    DEFINEREG2(PP_STATUS, i830_debug_pp_status),
    DEFINEREG(PFIT_CONTROL),
    DEFINEREG(PFIT_PGM_RATIOS),
    DEFINEREG(PORT_HOTPLUG_EN),
    DEFINEREG(PORT_HOTPLUG_STAT),

    DEFINEREG2(DSPACNTR, i830_debug_dspcntr),
    DEFINEREG2(DSPASTRIDE, i830_debug_dspstride),
    DEFINEREG2(DSPAPOS, i830_debug_xy),
    DEFINEREG2(DSPASIZE, i830_debug_xyminus1),
    DEFINEREG(DSPABASE),
    DEFINEREG(DSPASURF),
    DEFINEREG(DSPATILEOFF),
    DEFINEREG2(PIPEACONF, i830_debug_pipeconf),
    DEFINEREG2(PIPEASRC, i830_debug_xyminus1),

    DEFINEREG2(FPA0, i830_debug_fp),
    DEFINEREG2(FPA1, i830_debug_fp),
    DEFINEREG2(DPLL_A, i830_debug_dpll),
    DEFINEREG(DPLL_A_MD),
    DEFINEREG2(HTOTAL_A, i830_debug_hvtotal),
    DEFINEREG2(HBLANK_A, i830_debug_hvsyncblank),
    DEFINEREG2(HSYNC_A, i830_debug_hvsyncblank),
    DEFINEREG2(VTOTAL_A, i830_debug_hvtotal),
    DEFINEREG2(VBLANK_A, i830_debug_hvsyncblank),
    DEFINEREG2(VSYNC_A, i830_debug_hvsyncblank),
    DEFINEREG(BCLRPAT_A),
    DEFINEREG(VSYNCSHIFT_A),

    DEFINEREG2(DSPBCNTR, i830_debug_dspcntr),
    DEFINEREG2(DSPBSTRIDE, i830_debug_dspstride),
    DEFINEREG2(DSPBPOS, i830_debug_xy),
    DEFINEREG2(DSPBSIZE, i830_debug_xyminus1),
    DEFINEREG(DSPBBASE),
    DEFINEREG(DSPBSURF),
    DEFINEREG(DSPBTILEOFF),
    DEFINEREG2(PIPEBCONF, i830_debug_pipeconf),
    DEFINEREG2(PIPEBSRC, i830_debug_xyminus1),

    DEFINEREG2(FPB0, i830_debug_fp),
    DEFINEREG2(FPB1, i830_debug_fp),
    DEFINEREG2(DPLL_B, i830_debug_dpll),
    DEFINEREG(DPLL_B_MD),
    DEFINEREG2(HTOTAL_B, i830_debug_hvtotal),
    DEFINEREG2(HBLANK_B, i830_debug_hvsyncblank),
    DEFINEREG2(HSYNC_B, i830_debug_hvsyncblank),
    DEFINEREG2(VTOTAL_B, i830_debug_hvtotal),
    DEFINEREG2(VBLANK_B, i830_debug_hvsyncblank),
    DEFINEREG2(VSYNC_B, i830_debug_hvsyncblank),
    DEFINEREG(BCLRPAT_B),
    DEFINEREG(VSYNCSHIFT_B),

    DEFINEREG(VCLK_DIVISOR_VGA0),
    DEFINEREG(VCLK_DIVISOR_VGA1),
    DEFINEREG(VCLK_POST_DIV),
    DEFINEREG2(VGACNTRL, i830_debug_vgacntrl),

    DEFINEREG(TV_CTL),
    DEFINEREG(TV_DAC),
    DEFINEREG(TV_CSC_Y),
    DEFINEREG(TV_CSC_Y2),
    DEFINEREG(TV_CSC_U),
    DEFINEREG(TV_CSC_U2),
    DEFINEREG(TV_CSC_V),
    DEFINEREG(TV_CSC_V2),
    DEFINEREG(TV_CLR_KNOBS),
    DEFINEREG(TV_CLR_LEVEL),
    DEFINEREG(TV_H_CTL_1),
    DEFINEREG(TV_H_CTL_2),
    DEFINEREG(TV_H_CTL_3),
    DEFINEREG(TV_V_CTL_1),
    DEFINEREG(TV_V_CTL_2),
    DEFINEREG(TV_V_CTL_3),
    DEFINEREG(TV_V_CTL_4),
    DEFINEREG(TV_V_CTL_5),
    DEFINEREG(TV_V_CTL_6),
    DEFINEREG(TV_V_CTL_7),
    DEFINEREG(TV_SC_CTL_1),
    DEFINEREG(TV_SC_CTL_2),
    DEFINEREG(TV_SC_CTL_3),
    DEFINEREG(TV_WIN_POS),
    DEFINEREG(TV_WIN_SIZE),
    DEFINEREG(TV_FILTER_CTL_1),
    DEFINEREG(TV_FILTER_CTL_2),
    DEFINEREG(TV_FILTER_CTL_3),
    DEFINEREG(TV_CC_CONTROL),
    DEFINEREG(TV_CC_DATA),
    DEFINEREG(TV_H_LUMA_0),
    DEFINEREG(TV_H_LUMA_59),
    DEFINEREG(TV_H_CHROMA_0),
    DEFINEREG(TV_H_CHROMA_59),
};
#undef DEFINEREG
#define NUM_I830_SNAPSHOTREGS (sizeof(i830_snapshot) / sizeof(i830_snapshot[0]))

void i830TakeRegSnapshot(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int i;

    for (i = 0; i < NUM_I830_SNAPSHOTREGS; i++) {
	i830_snapshot[i].val = INREG(i830_snapshot[i].reg);
    }
}

void i830CompareRegsToSnapshot(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int i;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Comparing regs before/after X's VT usage\n");
    for (i = 0; i < NUM_I830_SNAPSHOTREGS; i++) {
	CARD32 val = INREG(i830_snapshot[i].reg);
	if (i830_snapshot[i].val == val)
	    continue;

	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Register 0x%x (%s) changed from 0x%08x to 0x%08x\n",
		   i830_snapshot[i].reg, i830_snapshot[i].name,
		   (int)i830_snapshot[i].val, (int)val);

	if (i830_snapshot[i].debug_output != NULL) {
	    char *before, *after;

	    before = i830_snapshot[i].debug_output(pI830,
						   i830_snapshot[i].reg,
						   i830_snapshot[i].val);
	    after = i830_snapshot[i].debug_output(pI830,
						  i830_snapshot[i].reg,
						  val);
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "%s before: %s", i830_snapshot[i].name, before);
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "%s after: %s", i830_snapshot[i].name, after);

	}
    }
}

static void i830DumpIndexed (ScrnInfoPtr pScrn, char *name, int id, int val, int min, int max)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int	i;

    for (i = min; i <= max; i++) {
	OUTREG8 (id, i);
	xf86DrvMsg (pScrn->scrnIndex, X_INFO, "%18.18s%02x: 0x%02x\n",
		    name, i, INREG8(val));
    }
}

void i830DumpRegs (ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int i;
    int	fp, dpll;
    int pipe;
    int	n, m1, m2, m, p1, p2;
    int ref;
    int	dot;
    int phase;
    int msr;
    int crt;

    xf86DrvMsg (pScrn->scrnIndex, X_INFO, "DumpRegsBegin\n");
    for (i = 0; i < NUM_I830_SNAPSHOTREGS; i++) {
	CARD32 val = INREG(i830_snapshot[i].reg);

	if (i830_snapshot[i].debug_output != NULL) {
	    char *debug = i830_snapshot[i].debug_output(pI830,
							i830_snapshot[i].reg,
							val);
	    xf86DrvMsg (pScrn->scrnIndex, X_INFO, "%20.20s: 0x%08x (%s)\n",
			i830_snapshot[i].name, (unsigned int)val, debug);
	    xfree(debug);
	} else {
	    xf86DrvMsg (pScrn->scrnIndex, X_INFO, "%20.20s: 0x%08x\n",
			i830_snapshot[i].name, (unsigned int)val);
	}
    }
    i830DumpIndexed (pScrn, "SR", 0x3c4, 0x3c5, 0, 7);
    msr = INREG8(0x3cc);
    xf86DrvMsg (pScrn->scrnIndex, X_INFO, "%20.20s: 0x%02x\n",
		    "MSR", (unsigned int) msr);

    if (msr & 1)
	crt = 0x3d0;
    else
	crt = 0x3b0;
    i830DumpIndexed (pScrn, "CR", crt + 4, crt + 5, 0, 0x24);
    for (pipe = 0; pipe <= 1; pipe++)
    {
	fp = INREG(pipe == 0 ? FPA0 : FPB0);
	dpll = INREG(pipe == 0 ? DPLL_A : DPLL_B);
	switch ((dpll >> 24) & 0x3) {
	case 0:
	    p2 = 10;
	    break;
	case 1:
	    p2 = 5;
	    break;
	default:
	    p2 = 1;
	    xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "p2 out of range\n");
	    break;
	}
	switch ((dpll >> 16) & 0xff) {
	case 1:
	    p1 = 1; break;
	case 2:
	    p1 = 2; break;
	case 4:
	    p1 = 3; break;
	case 8:
	    p1 = 4; break;
	case 16:
	    p1 = 5; break;
	case 32:
	    p1 = 6; break;
	case 64:
	    p1 = 7; break;
	case 128:
	    p1 = 8; break;
	default:
	    p1 = 1;
	    xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "p1 out of range\n");
	    break;
	}
	switch ((dpll >> 13) & 0x3) {
	case 0:
	    ref = 96000;
	    break;
	default:
	    ref = 0;
	    xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "ref out of range\n");
	    break;
	}
	phase = (dpll >> 9) & 0xf;
	switch (phase) {
	case 6:
	    break;
	default:
	    xf86DrvMsg (pScrn->scrnIndex, X_INFO,
			"SDVO phase shift %d out of range -- probobly not "
			"an issue.\n", phase);
	    break;
	}
	switch ((dpll >> 8) & 1) {
	case 0:
	    break;
	default:
	    xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "fp select out of range\n");
	    break;
	}
	n = ((fp >> 16) & 0x3f);
	m1 = ((fp >> 8) & 0x3f);
	m2 = ((fp >> 0) & 0x3f);
	m = 5 * (m1 + 2) + (m2 + 2);
	dot = (ref * (5 * (m1 + 2) + (m2 + 2)) / (n + 2)) / (p1 * p2);
	xf86DrvMsg (pScrn->scrnIndex, X_INFO, "pipe %s dot %d n %d m1 %d m2 %d p1 %d p2 %d\n",
		    pipe == 0 ? "A" : "B", dot, n, m1, m2, p1, p2);
    }
    xf86DrvMsg (pScrn->scrnIndex, X_INFO, "DumpRegsEnd\n");
}
