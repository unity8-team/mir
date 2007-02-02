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
#include <strings.h>

#define DEBUGSTRING(func) static char *func(I830Ptr pI830, int reg, CARD32 val)

DEBUGSTRING(i830_debug_xyminus1)
{
    return XNFprintf("%d, %d", (val & 0xffff) + 1,
		     ((val & 0xffff0000) >> 16) + 1);
}

DEBUGSTRING(i830_debug_yxminus1)
{
    return XNFprintf("%d, %d", ((val & 0xffff0000) >> 16) + 1,
		     (val & 0xffff) + 1);
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
    char sdvoextra[20];
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
	sprintf(sdvoextra, ", SDVO mult %d",
		(int)((val & SDVO_MULTIPLIER_MASK) >>
		SDVO_MULTIPLIER_SHIFT_HIRES) + 1);
    } else {
	sdvoextra[0] = '\0';
    }

    return XNFprintf("%s, %s%s, %s mode, %s clock, p1 = %d, "
		     "p2 = %d%s%s",
		     enabled, dvomode, vgamode, mode, clock, p1, p2,
		     fpextra, sdvoextra);
}

DEBUGSTRING(i830_debug_dpll_test)
{
    char *dpllandiv = val & DPLLA_TEST_N_BYPASS ? ", DPLLA N bypassed" : "";
    char *dpllamdiv = val & DPLLA_TEST_M_BYPASS ? ", DPLLA M bypassed" : "";
    char *dpllainput = val & DPLLA_INPUT_BUFFER_ENABLE ?
	"" : ", DPLLA input buffer disabled";
    char *dpllbndiv = val & DPLLB_TEST_N_BYPASS ? ", DPLLB N bypassed" : "";
    char *dpllbmdiv = val & DPLLB_TEST_M_BYPASS ? ", DPLLB M bypassed" : "";
    char *dpllbinput = val & DPLLB_INPUT_BUFFER_ENABLE ?
	"" : ", DPLLB input buffer disabled";

    return XNFprintf("%s%s%s%s%s%s",
		     dpllandiv, dpllamdiv, dpllainput,
		     dpllbndiv, dpllbmdiv, dpllbinput);
}

DEBUGSTRING(i830_debug_lvds)
{
    char pipe = val & LVDS_PIPEB_SELECT ? 'B' : 'A';
    char *enable = val & LVDS_PORT_EN ? "enabled" : "disabled";

    return XNFprintf("%s, pipe %c", enable, pipe);
}

DEBUGSTRING(i830_debug_sdvo)
{
    char *enable = val & SDVO_ENABLE ? "enabled" : "disabled";
    char pipe = val & SDVO_PIPE_B_SELECT ? 'B' : 'A';
    char *stall = val & SDVO_STALL_SELECT ? "enabled" : "disabled";
    char *detected = val & SDVO_DETECTED ? "" : "not ";
    char *gang = val & SDVOC_GANG_MODE ? ", gang mode" : "";
    char sdvoextra[20];

    if (IS_I915G(pI830) || IS_I915GM(pI830)) {
	sprintf(sdvoextra, ", SDVO mult %d",
		(int)((val & SDVO_PORT_MULTIPLY_MASK) >>
		SDVO_PORT_MULTIPLY_SHIFT) + 1);
    } else {
	sdvoextra[0] = '\0';
    }

    return XNFprintf("%s, pipe %c, stall %s, %sdetected%s%s",
		     enable, pipe, stall, detected, sdvoextra, gang);
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
    DEFINEREG2(DPLL_TEST, i830_debug_dpll_test),
    DEFINEREG(D_STATE),
    DEFINEREG(DSPCLK_GATE_D),
    DEFINEREG(RENCLK_GATE_D1),
    DEFINEREG(RENCLK_GATE_D2),
/*  DEFINEREG(RAMCLK_GATE_D),	CRL only */
    DEFINEREG2(SDVOB, i830_debug_sdvo),
    DEFINEREG2(SDVOC, i830_debug_sdvo),
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
    DEFINEREG2(PIPEASRC, i830_debug_yxminus1),

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
    DEFINEREG2(PIPEBSRC, i830_debug_yxminus1),

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

void i830CompareRegsToSnapshot(ScrnInfoPtr pScrn, char *where)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int i;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Comparing regs from server start up to %s\n", where);
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
		       "%s before: %s\n", i830_snapshot[i].name, before);
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "%s after: %s\n", i830_snapshot[i].name, after);

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
	    xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "p2 out of range\n");
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
	    xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "p1 out of range\n");
	    break;
	}
	switch ((dpll >> 13) & 0x3) {
	case 0:
	    ref = 96000;
	    break;
	case 3:
	    ref = 100000;
	    break;
	default:
	    ref = 0;
	    xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "ref out of range\n");
	    break;
	}
	if (IS_I965G(pI830)) {
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
	}
	switch ((dpll >> 8) & 1) {
	case 0:
	    break;
	default:
	    xf86DrvMsg (pScrn->scrnIndex, X_WARNING,
			"fp select out of range\n");
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

/* Famous last words
 */
void
i830_dump_error_state(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    ErrorF("pgetbl_ctl: 0x%lx pgetbl_err: 0x%lx\n",
	   (unsigned long)INREG(PGETBL_CTL), (unsigned long)INREG(PGE_ERR));

    ErrorF("ipeir: %lx iphdr: %lx\n", (unsigned long)INREG(IPEIR),
	   (unsigned long)INREG(IPEHR));

    ErrorF("LP ring tail: %lx head: %lx len: %lx start %lx\n",
	   (unsigned long)INREG(LP_RING + RING_TAIL),
	   (unsigned long)INREG(LP_RING + RING_HEAD) & HEAD_ADDR,
	   (unsigned long)INREG(LP_RING + RING_LEN),
	   (unsigned long)INREG(LP_RING + RING_START));

    ErrorF("eir: %x esr: %x emr: %x\n",
	   INREG16(EIR), INREG16(ESR), INREG16(EMR));

    ErrorF("instdone: %x instpm: %x\n", INREG16(INST_DONE), INREG8(INST_PM));

    ErrorF("memmode: %lx instps: %lx\n", (unsigned long)INREG(MEMMODE),
	   (unsigned long)INREG(INST_PS));

    ErrorF("hwstam: %x ier: %x imr: %x iir: %x\n",
	   INREG16(HWSTAM), INREG16(IER), INREG16(IMR), INREG16(IIR));
}

void
i965_dump_error_state(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    ErrorF("pgetbl_ctl: 0x%lx pgetbl_err: 0x%lx\n",
	   INREG(PGETBL_CTL), INREG(PGE_ERR));

    ErrorF("ipeir: %lx iphdr: %lx\n", INREG(IPEIR_I965), INREG(IPEHR_I965));

    ErrorF("LP ring tail: %lx head: %lx len: %lx start %lx\n",
	   INREG(LP_RING + RING_TAIL),
	   INREG(LP_RING + RING_HEAD) & HEAD_ADDR,
	   INREG(LP_RING + RING_LEN), INREG(LP_RING + RING_START));

    ErrorF("Err ID (eir): %x Err Status (esr): %x Err Mask (emr): %x\n",
	   (int)INREG(EIR), (int)INREG(ESR), (int)INREG(EMR));

    ErrorF("instdone: %x instdone_1: %x\n", (int)INREG(INST_DONE_I965),
	   (int)INREG(INST_DONE_1));
    ErrorF("instpm: %x\n", (int)INREG(INST_PM));

    ErrorF("memmode: %lx instps: %lx\n", INREG(MEMMODE), INREG(INST_PS_I965));

    ErrorF("HW Status mask (hwstam): %x\nIRQ enable (ier): %x "
	   "imr: %x iir: %x\n",
	   (int)INREG(HWSTAM), (int)INREG(IER), (int)INREG(IMR),
	   (int)INREG(IIR));

    ErrorF("acthd: %lx dma_fadd_p: %lx\n", INREG(ACTHD), INREG(DMA_FADD_P));
    ErrorF("ecoskpd: %lx excc: %lx\n", INREG(ECOSKPD), INREG(EXCC));

    ErrorF("cache_mode: %x/%x\n", (int)INREG(CACHE_MODE_0),
	   (int)INREG(CACHE_MODE_1));
    ErrorF("mi_arb_state: %x\n", (int)INREG(MI_ARB_STATE));

    ErrorF("IA_VERTICES_COUNT_QW %x/%x\n",
	   (int)INREG(IA_VERTICES_COUNT_QW),
	   (int)INREG(IA_VERTICES_COUNT_QW+4));
    ErrorF("IA_PRIMITIVES_COUNT_QW %x/%x\n",
	   (int)INREG(IA_PRIMITIVES_COUNT_QW),
	   (int)INREG(IA_PRIMITIVES_COUNT_QW+4));

    ErrorF("VS_INVOCATION_COUNT_QW %x/%x\n",
	   (int)INREG(VS_INVOCATION_COUNT_QW),
	   (int)INREG(VS_INVOCATION_COUNT_QW+4));

    ErrorF("GS_INVOCATION_COUNT_QW %x/%x\n",
	   (int)INREG(GS_INVOCATION_COUNT_QW),
	   (int)INREG(GS_INVOCATION_COUNT_QW+4));
    ErrorF("GS_PRIMITIVES_COUNT_QW %x/%x\n",
	   (int)INREG(GS_PRIMITIVES_COUNT_QW),
	   (int)INREG(GS_PRIMITIVES_COUNT_QW+4));

    ErrorF("CL_INVOCATION_COUNT_QW %x/%x\n",
	   (int)INREG(CL_INVOCATION_COUNT_QW),
	   (int)INREG(CL_INVOCATION_COUNT_QW+4));
    ErrorF("CL_PRIMITIVES_COUNT_QW %x/%x\n",
	   (int)INREG(CL_PRIMITIVES_COUNT_QW),
	   (int)INREG(CL_PRIMITIVES_COUNT_QW+4));

    ErrorF("PS_INVOCATION_COUNT_QW %x/%x\n",
	   (int)INREG(PS_INVOCATION_COUNT_QW),
	   (int)INREG(PS_INVOCATION_COUNT_QW+4));
    ErrorF("PS_DEPTH_COUNT_QW %x/%x\n",
	   (int)INREG(PS_DEPTH_COUNT_QW),
	   (int)INREG(PS_DEPTH_COUNT_QW+4));

    ErrorF("WIZ_CTL %x\n", (int)INREG(WIZ_CTL));
    ErrorF("TS_CTL %x  TS_DEBUG_DATA %x\n", (int)INREG(TS_CTL),
	   (int)INREG(TS_DEBUG_DATA));
    ErrorF("TD_CTL %x / %x\n", (int)INREG(TD_CTL), (int)INREG(TD_CTL2));
}

/**
 * Checks the hardware error state bits.
 *
 * \return TRUE if any errors were found.
 */
Bool
i830_check_error_state(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int errors = 0;
    unsigned long temp, head, tail;

    if (!I830IsPrimary(pScrn)) return TRUE;

    temp = INREG16(ESR);
    if (temp != 0) {
	Bool vertex_max = !IS_I965G(pI830) && (temp & ERR_VERTEX_MAX);
	Bool pgtbl = temp & ERR_PGTBL_ERROR;
	Bool underrun = !IS_I965G(pI830) &&
	    (temp & ERR_DISPLAY_OVERLAY_UNDERRUN);
	Bool instruction = !IS_I965G(pI830) && (temp & ERR_INSTRUCTION_ERROR);

	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "ESR is 0x%08lx%s%s%s%s\n", temp,
		   vertex_max ? ", max vertices exceeded" : "",
		   pgtbl ? ", page table error" : "",
		   underrun ? ", display/overlay underrun" : "",
		   instruction ? ", instruction error" : "");
	errors++;
    }
    /* Check first for page table errors */
    if (!IS_I9XX(pI830)) {
	temp = INREG(PGE_ERR);
	if (temp != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "PGTBL_ER is 0x%08lx\n", temp);
	    errors++;
	}
    } else {
	temp = INREG(PGTBL_ER);
	if (temp != 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "PGTBL_ER is 0x%08lx"
		       "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n", temp,
		       temp & PGTBL_ERR_HOST_GTT_PTE ? ", host gtt pte" : "",
		       temp & PGTBL_ERR_HOST_PTE_DATA ? ", host pte data" : "",
		       temp & PGTBL_ERR_DISPA_GTT_PTE ? ", display A pte" : "",
		       temp & PGTBL_ERR_DISPA_TILING ?
		       ", display A tiling" : "",
		       temp & PGTBL_ERR_DISPB_GTT_PTE ? ", display B pte" : "",
		       temp & PGTBL_ERR_DISPB_TILING ?
		       ", display B tiling" : "",
		       temp & PGTBL_ERR_DISPC_GTT_PTE ? ", display C pte" : "",
		       temp & PGTBL_ERR_DISPC_TILING ?
		       ", display C tiling" : "",
		       temp & PGTBL_ERR_OVERLAY_GTT_PTE ?
		       ", overlay GTT PTE" : "",
		       temp & PGTBL_ERR_OVERLAY_TILING ?
		       ", overlay tiling" : "",
		       temp & PGTBL_ERR_CS_GTT ? ", CS GTT" : "",
		       temp & PGTBL_ERR_CS_INSTRUCTION_GTT_PTE ?
		       ", CS instruction GTT PTE" : "",
		       temp & PGTBL_ERR_CS_VERTEXDATA_GTT_PTE ?
		       ", CS vertex data GTT PTE" : "",
		       temp & PGTBL_ERR_BIN_INSTRUCTION_GTT_PTE ?
		       ", BIN instruction GTT PTE" : "",
		       temp & PGTBL_ERR_BIN_VERTEXDATA_GTT_PTE ?
		       ", BIN vertex data GTT PTE" : "",
		       temp & PGTBL_ERR_LC_GTT_PTE ? ", LC pte" : "",
		       temp & PGTBL_ERR_LC_TILING ? ", LC tiling" : "",
		       temp & PGTBL_ERR_MT_GTT_PTE ? ", MT pte" : "",
		       temp & PGTBL_ERR_MT_TILING ? ", MT tiling" : "");
	    errors++;
	}
    }
    temp = INREG(PGETBL_CTL);
    if (!(temp & 1)) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "PGTBL_CTL (0x%08lx) indicates GTT is disabled\n", temp);
	errors++;
    }
    temp = INREG(LP_RING + RING_LEN);
    if (temp & 1) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "PRB0_CTL (0x%08lx) indicates ring buffer enabled\n", temp);
	errors++;
    }
    head = INREG(LP_RING + RING_HEAD);
    tail = INREG(LP_RING + RING_TAIL);
    if ((tail & I830_TAIL_MASK) != (head & I830_HEAD_MASK)) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "PRB0_HEAD (0x%08lx) and PRB0_TAIL (0x%08lx) indicate "
		   "ring buffer not flushed\n", head, tail);
	errors++;
    }

#if 0
    if (errors) {
	if (IS_I965G(pI830))
	    i965_dump_error_state(pScrn);
	else
	    i830_dump_error_state(pScrn);
    }
#endif

    return (errors != 0);
}
