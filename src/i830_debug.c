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

#define DEFINEREG(reg) \
	{ reg, #reg, 0 }

static struct i830SnapshotRec {
    int reg;
    char *name;
    CARD32 regval;
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
    DEFINEREG(LVDS),
    DEFINEREG(DVOA),
    DEFINEREG(DVOB),
    DEFINEREG(DVOC),
    DEFINEREG(DVOA_SRCDIM),
    DEFINEREG(DVOB_SRCDIM),
    DEFINEREG(DVOC_SRCDIM),

    DEFINEREG(PP_CONTROL),
    DEFINEREG(PP_STATUS),
    DEFINEREG(PFIT_CONTROL),
    DEFINEREG(PFIT_PGM_RATIOS),
    DEFINEREG(PORT_HOTPLUG_EN),
    DEFINEREG(PORT_HOTPLUG_STAT),

    DEFINEREG(DSPACNTR),
    DEFINEREG(DSPASTRIDE),
    DEFINEREG(DSPAPOS),
    DEFINEREG(DSPASIZE),
    DEFINEREG(DSPABASE),
    DEFINEREG(DSPASURF),
    DEFINEREG(DSPATILEOFF),
    DEFINEREG(PIPEACONF),
    DEFINEREG(PIPEASRC),

    DEFINEREG(FPA0),
    DEFINEREG(FPA1),
    DEFINEREG(DPLL_A),
    DEFINEREG(DPLLAMD),
    DEFINEREG(HTOTAL_A),
    DEFINEREG(HBLANK_A),
    DEFINEREG(HSYNC_A),
    DEFINEREG(VTOTAL_A),
    DEFINEREG(VBLANK_A),
    DEFINEREG(VSYNC_A),
    DEFINEREG(BCLRPAT_A),
    DEFINEREG(VSYNCSHIFT_A),

    DEFINEREG(DSPBCNTR),
    DEFINEREG(DSPBSTRIDE),
    DEFINEREG(DSPBPOS),
    DEFINEREG(DSPBSIZE),
    DEFINEREG(DSPBBASE),
    DEFINEREG(DSPBSURF),
    DEFINEREG(DSPBTILEOFF),
    DEFINEREG(PIPEBCONF),
    DEFINEREG(PIPEBSRC),

    DEFINEREG(FPB0),
    DEFINEREG(FPB1),
    DEFINEREG(DPLL_B),
    DEFINEREG(DPLLBMD),
    DEFINEREG(HTOTAL_B),
    DEFINEREG(HBLANK_B),
    DEFINEREG(HSYNC_B),
    DEFINEREG(VTOTAL_B),
    DEFINEREG(VBLANK_B),
    DEFINEREG(VSYNC_B),
    DEFINEREG(BCLRPAT_B),
    DEFINEREG(VSYNCSHIFT_B),

    DEFINEREG(VCLK_DIVISOR_VGA0),
    DEFINEREG(VCLK_DIVISOR_VGA1),
    DEFINEREG(VCLK_POST_DIV),
    DEFINEREG(VGACNTRL),
};
#undef DEFINEREG
#define NUM_I830_SNAPSHOTREGS (sizeof(i830_snapshot) / sizeof(i830_snapshot[0]))

void i830TakeRegSnapshot(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int i;

    for (i = 0; i < NUM_I830_SNAPSHOTREGS; i++) {
	i830_snapshot[i].regval = INREG(i830_snapshot[i].reg);
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
	if (i830_snapshot[i].regval != val) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Register 0x%x (%s) changed from 0x%08x to 0x%08x\n",
		       i830_snapshot[i].reg, i830_snapshot[i].name,
		       (int)i830_snapshot[i].regval, (int)val);
	}
    }
}

static void i830DumpIndexed (ScrnInfoPtr pScrn, char *name, int id, int val, int min, int max)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int	i;

    for (i = min; i <= max; i++) {
	OUTREG8 (id, i);
	xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "%18.18s%02x: 0x%02x\n",
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

    xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "DumpRegsBegin\n");
    for (i = 0; i < NUM_I830_SNAPSHOTREGS; i++) {
	xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "%20.20s: 0x%08x\n",
		    i830_snapshot[i].name, (unsigned int) INREG(i830_snapshot[i].reg));
    }
    i830DumpIndexed (pScrn, "SR", 0x3c4, 0x3c5, 0, 7);
    msr = INREG8(0x3cc);
    xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "%20.20s: 0x%02x\n",
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
	    xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "phase %d out of range\n", phase);
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
	xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "pipe %s dot %d n %d m1 %d m2 %d p1 %d p2 %d\n",
		    pipe == 0 ? "A" : "B", dot, n, m1, m2, p1, p2);
    }
    xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "DumpRegsEnd\n");
}
