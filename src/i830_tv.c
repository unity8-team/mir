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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

/** @file
 * Integrated TV-out support for the 915GM and 945GM.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "i830.h"
#include "i830_display.h"

enum tv_type {
    TV_TYPE_UNKNOWN,
    TV_TYPE_COMPOSITE,
    TV_TYPE_SVIDEO,
    TV_TYPE_COMPONENT
};

/** Private structure for the integrated TV support */
struct i830_tv_priv {
    CARD32 save_TV_H_CTL_1;
    CARD32 save_TV_H_CTL_2;
    CARD32 save_TV_H_CTL_3;
    CARD32 save_TV_V_CTL_1;
    CARD32 save_TV_V_CTL_2;
    CARD32 save_TV_V_CTL_3;
    CARD32 save_TV_V_CTL_4;
    CARD32 save_TV_V_CTL_5;
    CARD32 save_TV_V_CTL_6;
    CARD32 save_TV_V_CTL_7;
    CARD32 save_TV_SC_CTL_1, save_TV_SC_CTL_2, save_TV_SC_CTL_3;
    CARD32 save_TV_DAC;
    CARD32 save_TV_CTL;
};

enum burst_modes {
    TV_SC_NTSC_MJ,
    TV_SC_PAL,
    TV_SC_PAL_NC,
    TV_SC_PAL_M,
    TV_SC_NTSC_443
};

const struct tv_sc_mode {
    char *name;
    int dda2_size, dda3_size, dda1_inc, dda2_inc, dda3_inc;
    CARD32 sc_reset;
    Bool pal_burst;
} tv_sc_modes[] = {
    [TV_SC_NTSC_MJ] = {
	"NTSC M/J",
	27456, 0, 135, 20800, 0,
	TV_SC_RESET_EVERY_4,
	FALSE
    },
    [TV_SC_PAL] = {
	"PAL",
	27648, 625, 168, 4122, 67,
	TV_SC_RESET_EVERY_8,
	TRUE
    },
    [TV_SC_PAL_NC] = {
	"PAL Nc",
	27648, 625, 135, 23578, 134,
	TV_SC_RESET_EVERY_8,
	TRUE
    },
    [TV_SC_PAL_M] = {
	"PAL M",
	27456, 0, 135, 16704, 0,
	TV_SC_RESET_EVERY_8,
	TRUE
    },
    [TV_SC_NTSC_443] = {
	"NTSC-4.43",
	27456, 525, 168, 4093, 310,
	TV_SC_RESET_NEVER,
	FALSE
    },
};

/**
 * Register programming values for TV modes.
 *
 * These values account for -1s required.
 */
const struct tv_mode {
    char *name;
    CARD32 oversample;
    int hsync_end, hblank_end, hblank_start, htotal;
    Bool progressive;
    int vsync_start_f1, vsync_start_f2, vsync_len;
    Bool veq_ena;
    int veq_start_f1, veq_start_f2, veq_len;
    int vi_end_f1, vi_end_f2, nbr_end;
    Bool burst_ena;
    int hburst_start, hburst_len;
    int vburst_start_f1, vburst_end_f1;
    int vburst_start_f2, vburst_end_f2;
    int vburst_start_f3, vburst_end_f3;
    int vburst_start_f4, vburst_end_f4;
} tv_modes[] = {
    {
	"480i",
	TV_OVERSAMPLE_8X,
	64, 124, 836, 857,
	FALSE,
	6, 7, 6,
	TRUE, 0, 1, 18,
	20, 21, 240,
	TRUE,
	72, 34, 9, 240, 10, 240, 9, 240, 10, 240
    }
};


static int
i830_tv_detect_type(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    CARD32 save_tv_ctl, save_tv_dac;
    CARD32 tv_ctl, tv_dac;
    I830Ptr pI830 = I830PTR(pScrn);

    save_tv_ctl = INREG(TV_CTL);
    save_tv_dac = INREG(TV_DAC);

    /* First, we have to disable the encoder but source from the right pipe,
     * which is already enabled.
     */
    tv_ctl = INREG(TV_CTL) & ~(TV_ENC_ENABLE | TV_ENC_PIPEB_SELECT);
    if (output->pipe == 1)
	tv_ctl |= TV_ENC_PIPEB_SELECT;
    OUTREG(TV_CTL, tv_ctl);

    /* Then set the voltage overrides. */
    tv_dac = DAC_CTL_OVERRIDE | DAC_A_0_7_V | DAC_B_0_7_V | DAC_C_0_7_V;
    OUTREG(TV_DAC, tv_dac);

    /* Enable sensing of the load. */
    tv_ctl |= TV_TEST_MODE_MONITOR_DETECT;
    OUTREG(TV_CTL, tv_ctl);

    tv_dac |= TVDAC_STATE_CHG_EN | TVDAC_A_SENSE_CTL | TVDAC_B_SENSE_CTL |
        TVDAC_C_SENSE_CTL;
    OUTREG(TV_DAC, tv_dac);

    /* Wait for things to take effect. */
    i830WaitForVblank(pScrn);

    tv_dac = INREG(TV_DAC);

    OUTREG(TV_DAC, save_tv_dac);
    OUTREG(TV_CTL, save_tv_ctl);

    if ((tv_dac & TVDAC_SENSE_MASK) == (TVDAC_B_SENSE | TVDAC_C_SENSE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Detected Composite TV connection\n");
	return TV_TYPE_COMPOSITE;
    } else if ((tv_dac & TVDAC_SENSE_MASK) == TVDAC_A_SENSE) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Detected S-Video TV connection\n");
	return TV_TYPE_SVIDEO;
    } else if ((tv_dac & TVDAC_SENSE_MASK) == 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Detected Component TV connection\n");
	return TV_TYPE_COMPONENT;
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Couldn't detect TV connection\n");
	return TV_TYPE_UNKNOWN;
    }
}

static void
i830_tv_dpms(ScrnInfoPtr pScrn, I830OutputPtr output, int mode)
{
    I830Ptr pI830 = I830PTR(pScrn);

    switch(mode) {
    case DPMSModeOn:
	OUTREG(TV_CTL, INREG(TV_CTL) | TV_ENC_ENABLE);
	break;
    case DPMSModeStandby:
    case DPMSModeSuspend:
    case DPMSModeOff:
	OUTREG(TV_CTL, INREG(TV_CTL) & ~TV_ENC_ENABLE);
	break;
    }
}

static void
i830_tv_save(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);
    struct i830_tv_priv *dev_priv = output->dev_priv;

    dev_priv->save_TV_H_CTL_1 = INREG(TV_H_CTL_1);
    dev_priv->save_TV_H_CTL_2 = INREG(TV_H_CTL_2);
    dev_priv->save_TV_H_CTL_3 = INREG(TV_H_CTL_3);
    dev_priv->save_TV_V_CTL_1 = INREG(TV_V_CTL_1);
    dev_priv->save_TV_V_CTL_2 = INREG(TV_V_CTL_2);
    dev_priv->save_TV_V_CTL_3 = INREG(TV_V_CTL_3);
    dev_priv->save_TV_V_CTL_4 = INREG(TV_V_CTL_4);
    dev_priv->save_TV_V_CTL_5 = INREG(TV_V_CTL_5);
    dev_priv->save_TV_V_CTL_6 = INREG(TV_V_CTL_6);
    dev_priv->save_TV_V_CTL_7 = INREG(TV_V_CTL_7);
    dev_priv->save_TV_SC_CTL_1 = INREG(TV_SC_CTL_1);
    dev_priv->save_TV_SC_CTL_2 = INREG(TV_SC_CTL_2);
    dev_priv->save_TV_SC_CTL_3 = INREG(TV_SC_CTL_3);

    dev_priv->save_TV_DAC = INREG(TV_DAC);
    dev_priv->save_TV_CTL = INREG(TV_CTL);
}

static void
i830_tv_restore(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);
    struct i830_tv_priv *dev_priv = output->dev_priv;

    OUTREG(TV_H_CTL_1, dev_priv->save_TV_H_CTL_1);
    OUTREG(TV_H_CTL_2, dev_priv->save_TV_H_CTL_2);
    OUTREG(TV_H_CTL_3, dev_priv->save_TV_H_CTL_3);
    OUTREG(TV_V_CTL_1, dev_priv->save_TV_V_CTL_1);
    OUTREG(TV_V_CTL_2, dev_priv->save_TV_V_CTL_2);
    OUTREG(TV_V_CTL_3, dev_priv->save_TV_V_CTL_3);
    OUTREG(TV_V_CTL_4, dev_priv->save_TV_V_CTL_4);
    OUTREG(TV_V_CTL_5, dev_priv->save_TV_V_CTL_5);
    OUTREG(TV_V_CTL_6, dev_priv->save_TV_V_CTL_6);
    OUTREG(TV_V_CTL_7, dev_priv->save_TV_V_CTL_7);
    OUTREG(TV_SC_CTL_1, dev_priv->save_TV_SC_CTL_1);
    OUTREG(TV_SC_CTL_2, dev_priv->save_TV_SC_CTL_2);
    OUTREG(TV_SC_CTL_3, dev_priv->save_TV_SC_CTL_3);

    OUTREG(TV_DAC, dev_priv->save_TV_DAC);
    OUTREG(TV_CTL, dev_priv->save_TV_CTL);
}

static int
i830_tv_mode_valid(ScrnInfoPtr pScrn, I830OutputPtr output,
		   DisplayModePtr pMode)
{
    return MODE_OK;
}

static void
i830_tv_pre_set_mode(ScrnInfoPtr pScrn, I830OutputPtr output,
		     DisplayModePtr pMode)
{
    I830Ptr pI830 = I830PTR(pScrn);

    /* Disable the encoder while we set up the pipe. */
    OUTREG(TV_CTL, INREG(TV_CTL) & ~TV_ENC_ENABLE);
}

static void
i830_tv_post_set_mode(ScrnInfoPtr pScrn, I830OutputPtr output,
		      DisplayModePtr pMode)
{
    I830Ptr pI830 = I830PTR(pScrn);
    enum tv_type type;
    const struct tv_mode *tv_mode;
    const struct tv_sc_mode *sc_mode;
    CARD32 tv_ctl, tv_filter_ctl;
    CARD32 hctl1, hctl2, hctl3;
    CARD32 vctl1, vctl2, vctl3, vctl4, vctl5, vctl6, vctl7;
    CARD32 scctl1, scctl2, scctl3;

    /* Need to actually choose or construct the appropriate
     * mode.  For now, just set the first one in the list, with
     * NTSC format.
     */
    tv_mode = &tv_modes[0];
    sc_mode = &tv_sc_modes[TV_SC_NTSC_MJ];

    type = i830_tv_detect_type(pScrn, output);
    if (type == TV_TYPE_UNKNOWN) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Defaulting TV to SVIDEO\n");
	type = TV_TYPE_SVIDEO;
    }

    hctl1 = (tv_mode->hsync_end << TV_HSYNC_END_SHIFT) |
	(tv_mode->htotal << TV_HTOTAL_SHIFT);

    hctl2 = (tv_mode->hburst_start << 16) |
	(tv_mode->hburst_len << TV_HBURST_LEN_SHIFT);
    if (tv_mode->burst_ena)
	hctl2 |= TV_BURST_ENA;

    hctl3 = (tv_mode->hblank_start << TV_HBLANK_START_SHIFT) |
	(tv_mode->hblank_end << TV_HBLANK_END_SHIFT);

    vctl1 = (tv_mode->nbr_end << TV_NBR_END_SHIFT) |
	(tv_mode->vi_end_f1 << TV_VI_END_F1_SHIFT) |
	(tv_mode->vi_end_f2 << TV_VI_END_F2_SHIFT);

    vctl2 = (tv_mode->vsync_len << TV_VSYNC_LEN_SHIFT) |
	(tv_mode->vsync_start_f1 << TV_VSYNC_START_F1_SHIFT) |
	(tv_mode->vsync_start_f2 << TV_VSYNC_START_F2_SHIFT);

    vctl3 = (tv_mode->veq_len << TV_VEQ_LEN_SHIFT) |
	(tv_mode->veq_start_f1 << TV_VEQ_START_F1_SHIFT) |
	(tv_mode->veq_start_f2 << TV_VEQ_START_F2_SHIFT);
    if (tv_mode->veq_ena)
	vctl3 |= TV_EQUAL_ENA;

    vctl4 = (tv_mode->vburst_start_f1 << TV_VBURST_START_F1_SHIFT) |
	(tv_mode->vburst_end_f1 << TV_VBURST_END_F1_SHIFT);

    vctl5 = (tv_mode->vburst_start_f2 << TV_VBURST_START_F2_SHIFT) |
	(tv_mode->vburst_end_f2 << TV_VBURST_END_F2_SHIFT);

    vctl6 = (tv_mode->vburst_start_f3 << TV_VBURST_START_F3_SHIFT) |
	(tv_mode->vburst_end_f3 << TV_VBURST_END_F3_SHIFT);

    vctl7 = (tv_mode->vburst_start_f4 << TV_VBURST_START_F4_SHIFT) |
	(tv_mode->vburst_end_f4 << TV_VBURST_END_F4_SHIFT);

    tv_ctl = TV_ENC_ENABLE;
    if (output->pipe == 1)
	tv_ctl |= TV_ENC_PIPEB_SELECT;

    switch (type) {
    case TV_TYPE_COMPOSITE:
	tv_ctl |= TV_ENC_OUTPUT_COMPOSITE;
	break;
    case TV_TYPE_COMPONENT:
	tv_ctl |= TV_ENC_OUTPUT_COMPONENT;
	break;
    default:
    case TV_TYPE_SVIDEO:
	tv_ctl |= TV_ENC_OUTPUT_SVIDEO;
	break;
    }
    tv_ctl |= tv_mode->oversample;
    if (tv_mode->progressive)
	tv_ctl |= TV_PROGRESSIVE;
    if (sc_mode->pal_burst)
	tv_ctl |= TV_PAL_BURST;

    scctl1 = TV_SC_DDA1_EN | TV_SC_DDA1_EN;
    if (sc_mode->dda3_size != 0)
	scctl1 |= TV_SC_DDA3_EN;
    scctl1 |= sc_mode->sc_reset;
    /* XXX: set the burst level */
    scctl1 |= sc_mode->dda1_inc << TV_SCDDA1_INC_SHIFT;

    scctl2 = sc_mode->dda2_size << TV_SCDDA2_SIZE_SHIFT |
	sc_mode->dda2_inc << TV_SCDDA2_INC_SHIFT;

    scctl3 = sc_mode->dda3_size << TV_SCDDA3_SIZE_SHIFT |
	sc_mode->dda3_inc << TV_SCDDA3_INC_SHIFT;

    /* Enable two fixes for the chips that need them. */
    if (pI830->PciInfo->chipType < PCI_CHIP_I945_G)
	tv_ctl |= TV_ENC_C0_FIX | TV_ENC_SDP_FIX;

    tv_filter_ctl = TV_AUTO_SCALE;
    if (pMode->HDisplay > 1024)
	tv_ctl |= TV_V_FILTER_BYPASS;

    OUTREG(TV_H_CTL_1, hctl1);
    OUTREG(TV_H_CTL_2, hctl2);
    OUTREG(TV_H_CTL_3, hctl3);
    OUTREG(TV_V_CTL_1, vctl1);
    OUTREG(TV_V_CTL_2, vctl2);
    OUTREG(TV_V_CTL_3, vctl3);
    OUTREG(TV_V_CTL_4, vctl4);
    OUTREG(TV_V_CTL_5, vctl5);
    OUTREG(TV_V_CTL_6, vctl6);
    OUTREG(TV_V_CTL_7, vctl7);
    OUTREG(TV_SC_CTL_1, scctl1);
    OUTREG(TV_SC_CTL_2, scctl2);
    OUTREG(TV_SC_CTL_3, scctl3);

    OUTREG(TV_DAC, 0);
    OUTREG(TV_CTL, tv_ctl);
}

void
i830_tv_init(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if ((INREG(TV_CTL) & TV_FUSE_STATE_MASK) == TV_FUSE_STATE_DISABLED)
	return;

    pI830->output[pI830->num_outputs].dev_priv =
	malloc(sizeof(struct i830_tv_priv));
    if (pI830->output[pI830->num_outputs].dev_priv == NULL)
	return;

    pI830->output[pI830->num_outputs].type = I830_OUTPUT_ANALOG;
    pI830->output[pI830->num_outputs].dpms = i830_tv_dpms;
    pI830->output[pI830->num_outputs].save = i830_tv_save;
    pI830->output[pI830->num_outputs].restore = i830_tv_restore;
    pI830->output[pI830->num_outputs].mode_valid = i830_tv_mode_valid;
    pI830->output[pI830->num_outputs].pre_set_mode = i830_tv_pre_set_mode;
    pI830->output[pI830->num_outputs].post_set_mode = i830_tv_post_set_mode;

    pI830->num_outputs++;
}
