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
    DEFINEREG(PIPEACONF),
    DEFINEREG(PIPEASRC),

    DEFINEREG(FPA0),
    DEFINEREG(FPA1),
    DEFINEREG(DPLL_A),
    DEFINEREG(HTOTAL_A),
    DEFINEREG(HBLANK_A),
    DEFINEREG(HSYNC_A),
    DEFINEREG(VTOTAL_A),
    DEFINEREG(VBLANK_A),
    DEFINEREG(VSYNC_A),

    DEFINEREG(DSPBCNTR),
    DEFINEREG(DSPBSTRIDE),
    DEFINEREG(DSPBPOS),
    DEFINEREG(DSPBSIZE),
    DEFINEREG(DSPBBASE),
    DEFINEREG(PIPEBCONF),
    DEFINEREG(PIPEBSRC),

    DEFINEREG(FPB0),
    DEFINEREG(FPB1),
    DEFINEREG(DPLL_B),
    DEFINEREG(HTOTAL_B),
    DEFINEREG(HBLANK_B),
    DEFINEREG(HSYNC_B),
    DEFINEREG(VTOTAL_B),
    DEFINEREG(VBLANK_B),
    DEFINEREG(VSYNC_B),

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

void i830DumpRegs (ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int i;

    for (i = 0; i < NUM_I830_SNAPSHOTREGS; i++) {
	xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "%10.10s: 0x%08x\n",
		    i830_snapshot[i].name, (unsigned int) INREG(i830_snapshot[i].reg));
    }
}
