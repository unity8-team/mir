#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_ansic.h"
#include "i830.h"

/* XXX: What was the syntax for sticking quotes around the "reg" argument? */
#define DEFINEREG(reg) \
	{ reg, NULL, 0 }

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
