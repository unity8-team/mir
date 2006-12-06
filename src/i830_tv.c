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
    TV_TYPE_NONE,
    TV_TYPE_UNKNOWN,
    TV_TYPE_COMPOSITE,
    TV_TYPE_SVIDEO,
    TV_TYPE_COMPONENT
};

/** Private structure for the integrated TV support */
struct i830_tv_priv {
    int type;
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

static void
i830_tv_dpms(xf86OutputPtr output, int mode)
{
    ScrnInfoPtr pScrn = output->scrn;
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
i830_tv_save(xf86OutputPtr output)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    I830OutputPrivatePtr    intel_output = output->driver_private;
    struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;

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
i830_tv_restore(xf86OutputPtr output)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    I830OutputPrivatePtr    intel_output = output->driver_private;
    struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;

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
i830_tv_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
    return MODE_OK;
}

static void
i830_tv_pre_set_mode(xf86OutputPtr output, DisplayModePtr pMode)
{
    ScrnInfoPtr pScrn = output->scrn;
    I830Ptr pI830 = I830PTR(pScrn);

    /* Disable the encoder while we set up the pipe. */
    OUTREG(TV_CTL, INREG(TV_CTL) & ~TV_ENC_ENABLE);
    /* XXX match BIOS for now */
    OUTREG(ADPA, 0x40008C18);
}

static const CARD32 h_luma[60] = {
    0xB1403000, 0x2E203500, 0x35002E20, 0x3000B140,
    0x35A0B160, 0x2DC02E80, 0xB1403480, 0xB1603000,
    0x2EA03640, 0x34002D80, 0x3000B120, 0x36E0B160,
    0x2D202EF0, 0xB1203380, 0xB1603000, 0x2F303780,
    0x33002CC0, 0x3000B100, 0x3820B160, 0x2C802F50,
    0xB10032A0, 0xB1603000, 0x2F9038C0, 0x32202C20,
    0x3000B0E0, 0x3980B160, 0x2BC02FC0, 0xB0E031C0,
    0xB1603000, 0x2FF03A20, 0x31602B60, 0xB020B0C0,
    0x3AE0B160, 0x2B001810, 0xB0C03120, 0xB140B020,
    0x18283BA0, 0x30C02A80, 0xB020B0A0, 0x3C60B140,
    0x2A201838, 0xB0A03080, 0xB120B020, 0x18383D20,
    0x304029C0, 0xB040B080, 0x3DE0B100, 0x29601848,
    0xB0803000, 0xB100B040, 0x18483EC0, 0xB0402900,
    0xB040B060, 0x3F80B0C0, 0x28801858, 0xB060B080,
    0xB0A0B060, 0x18602820, 0xB0A02820, 0x0000B060,
};

static const CARD32 h_chroma[60] = {
    0xB1403000, 0x2E203500, 0x35002E20, 0x3000B140,
    0x35A0B160, 0x2DC02E80, 0xB1403480, 0xB1603000,
    0x2EA03640, 0x34002D80, 0x3000B120, 0x36E0B160,
    0x2D202EF0, 0xB1203380, 0xB1603000, 0x2F303780,
    0x33002CC0, 0x3000B100, 0x3820B160, 0x2C802F50,
    0xB10032A0, 0xB1603000, 0x2F9038C0, 0x32202C20,
    0x3000B0E0, 0x3980B160, 0x2BC02FC0, 0xB0E031C0,
    0xB1603000, 0x2FF03A20, 0x31602B60, 0xB020B0C0,
    0x3AE0B160, 0x2B001810, 0xB0C03120, 0xB140B020,
    0x18283BA0, 0x30C02A80, 0xB020B0A0, 0x3C60B140,
    0x2A201838, 0xB0A03080, 0xB120B020, 0x18383D20,
    0x304029C0, 0xB040B080, 0x3DE0B100, 0x29601848,
    0xB0803000, 0xB100B040, 0x18483EC0, 0xB0402900,
    0xB040B060, 0x3F80B0C0, 0x28801858, 0xB060B080,
    0xB0A0B060, 0x18602820, 0xB0A02820, 0x0000B060,
};

static const CARD32 v_luma[43] = {
    0x36403000, 0x2D002CC0, 0x30003640, 0x2D0036C0,
    0x35C02CC0, 0x37403000, 0x2C802D40, 0x30003540,
    0x2D8037C0, 0x34C02C40, 0x38403000, 0x2BC02E00,
    0x30003440, 0x2E2038C0, 0x34002B80, 0x39803000,
    0x2B402E40, 0x30003380, 0x2E603A00, 0x33402B00,
    0x3A803040, 0x2A802EA0, 0x30403300, 0x2EC03B40,
    0x32802A40, 0x3C003040, 0x2A002EC0, 0x30803240,
    0x2EC03C80, 0x320029C0, 0x3D403080, 0x29402F00,
    0x308031C0, 0x2F203DC0, 0x31802900, 0x3E8030C0,
    0x28802F40, 0x30C03140, 0x2F203F40, 0x31402840,
    0x28003100, 0x28002F00, 0x00003100,
};

static const CARD32 v_chroma[43] = {
    0x36403000, 0x2D002CC0, 0x30003640, 0x2D0036C0,
    0x35C02CC0, 0x37403000, 0x2C802D40, 0x30003540,
    0x2D8037C0, 0x34C02C40, 0x38403000, 0x2BC02E00,
    0x30003440, 0x2E2038C0, 0x34002B80, 0x39803000,
    0x2B402E40, 0x30003380, 0x2E603A00, 0x33402B00,
    0x3A803040, 0x2A802EA0, 0x30403300, 0x2EC03B40,
    0x32802A40, 0x3C003040, 0x2A002EC0, 0x30803240,
    0x2EC03C80, 0x320029C0, 0x3D403080, 0x29402F00,
    0x308031C0, 0x2F203DC0, 0x31802900, 0x3E8030C0,
    0x28802F40, 0x30C03140, 0x2F203F40, 0x31402840,
    0x28003100, 0x28002F00, 0x00003100,
};

static Bool
i830_tv_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		 DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    int i;

    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr other_output = pI830->xf86_config.output[i];

	if (other_output != output && other_output->crtc == output->crtc) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Can't enable TV and another output on the same "
		       "pipe\n");
	    return FALSE;
	}
    }

    /* XXX: fill me in */

    return TRUE;
}

static void
i830_tv_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		 DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    xf86CrtcPtr	    crtc = output->crtc;
    I830OutputPrivatePtr    intel_output = output->driver_private;
    I830CrtcPrivatePtr	    intel_crtc = crtc->driver_private;
    struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;
    enum tv_type	    type;
    const struct tv_mode    *tv_mode;
    const struct tv_sc_mode *sc_mode;
    CARD32		    tv_ctl, tv_filter_ctl;
    CARD32		    hctl1, hctl2, hctl3;
    CARD32		    vctl1, vctl2, vctl3, vctl4, vctl5, vctl6, vctl7;
    CARD32		    scctl1, scctl2, scctl3;
    int			    i;

    /* Need to actually choose or construct the appropriate
     * mode.  For now, just set the first one in the list, with
     * NTSC format.
     */
    tv_mode = &tv_modes[0];
    sc_mode = &tv_sc_modes[TV_SC_NTSC_MJ];

    type = dev_priv->type;

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

    tv_ctl = 0;
    if (intel_crtc->pipe == 1)
	tv_ctl |= TV_ENC_PIPEB_SELECT;

    switch (type) {
    case TV_TYPE_COMPOSITE:
	tv_ctl |= TV_ENC_OUTPUT_COMPOSITE;
	break;
    case TV_TYPE_COMPONENT:
	tv_ctl |= TV_ENC_OUTPUT_COMPONENT;
	break;
    case TV_TYPE_SVIDEO:
	tv_ctl |= TV_ENC_OUTPUT_SVIDEO;
	break;
    default:
    case TV_TYPE_UNKNOWN:
	tv_ctl |= TV_ENC_OUTPUT_SVIDEO_COMPOSITE;
	break;
    }
    tv_ctl |= tv_mode->oversample;
    if (tv_mode->progressive)
	tv_ctl |= TV_PROGRESSIVE;
    if (sc_mode->pal_burst)
	tv_ctl |= TV_PAL_BURST;

    scctl1 = TV_SC_DDA1_EN | TV_SC_DDA2_EN;
    if (sc_mode->dda3_size != 0)
	scctl1 |= TV_SC_DDA3_EN;
    scctl1 |= sc_mode->sc_reset;
    /* XXX: set the burst level */
    scctl1 |= 113 << TV_BURST_LEVEL_SHIFT;    /* from BIOS */
    scctl1 |= sc_mode->dda1_inc << TV_SCDDA1_INC_SHIFT;

    scctl2 = sc_mode->dda2_size << TV_SCDDA2_SIZE_SHIFT |
	sc_mode->dda2_inc << TV_SCDDA2_INC_SHIFT;

    scctl3 = sc_mode->dda3_size << TV_SCDDA3_SIZE_SHIFT |
	sc_mode->dda3_inc << TV_SCDDA3_INC_SHIFT;

    /* Enable two fixes for the chips that need them. */
    if (pI830->PciInfo->chipType < PCI_CHIP_I945_G)
	tv_ctl |= TV_ENC_C0_FIX | TV_ENC_SDP_FIX;

    tv_filter_ctl = TV_AUTO_SCALE;
    if (mode->HDisplay > 1024)
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
    /* XXX match BIOS */
    OUTREG(TV_CSC_Y, 0x0332012D);
    OUTREG(TV_CSC_Y2, 0x07D30133);
    OUTREG(TV_CSC_U, 0x076A0564);
    OUTREG(TV_CSC_U2, 0x030D0200);
    OUTREG(TV_CSC_V, 0x037A033D);
    OUTREG(TV_CSC_V2, 0x06F60200);
    OUTREG(TV_CLR_KNOBS, 0x00606000);
    OUTREG(TV_CLR_LEVEL, 0x013C010A);
    OUTREG(TV_WIN_POS, 0x00360024);
    OUTREG(TV_WIN_SIZE, 0x02640198);
    OUTREG(TV_FILTER_CTL_1, 0x8000085E);
    OUTREG(TV_FILTER_CTL_2, 0x00017878);
    OUTREG(TV_FILTER_CTL_3, 0x0000BC3C);
    for (i = 0; i < 60; i++)
	OUTREG(TV_H_LUMA_0 + (i <<2), h_luma[i]);
    for (i = 0; i < 60; i++)
	OUTREG(TV_H_CHROMA_0 + (i <<2), h_chroma[i]);
    for (i = 0; i < 43; i++)
	OUTREG(TV_V_LUMA_0 + (i <<2), v_luma[i]);
    for (i = 0; i < 43; i++)
	OUTREG(TV_V_CHROMA_0 + (i <<2), v_chroma[i]);

    OUTREG(TV_DAC, 0);
    OUTREG(TV_CTL, tv_ctl);
}

static const DisplayModeRec tvModes[] = {
    {
	.name = "NTSC 480i",
	.Clock = 108000,
	
	.HDisplay   = 1024,
	.HSyncStart = 1048,
	.HSyncEnd   = 1184,
	.HTotal     = 1344,

	.VDisplay   = 768,
	.VSyncStart = 771,
	.VSyncEnd   = 777,
	.VTotal     = 806,

	.type       = M_T_DEFAULT
    }
};

/**
 * Detects TV presence by checking for load.
 *
 * Requires that the current pipe's DPLL is active.
 
 * \return TRUE if TV is connected.
 * \return FALSE if TV is disconnected.
 */
static void
i830_tv_detect_type (xf86CrtcPtr    crtc,
		     xf86OutputPtr  output)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    I830OutputPrivatePtr    intel_output = output->driver_private;
    struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;
    CARD32		    tv_ctl, save_tv_ctl;
    CARD32		    tv_dac, save_tv_dac;
    int			    type = TV_TYPE_UNKNOWN;

    tv_dac = INREG(TV_DAC);
    /*
     * Detect TV by polling)
     */
    if (intel_output->load_detect_temp)
    {
	/* TV not currently running, prod it with destructive detect */
	save_tv_dac = tv_dac;
	tv_ctl = INREG(TV_CTL);
	save_tv_ctl = tv_ctl;
	tv_ctl &= ~TV_ENC_ENABLE;
	tv_ctl &= ~TV_TEST_MODE_MASK;
	tv_ctl |= TV_TEST_MODE_MONITOR_DETECT;
	tv_dac &= ~TVDAC_SENSE_MASK;
	tv_dac |= (TVDAC_STATE_CHG_EN |
		   TVDAC_A_SENSE_CTL |
		   TVDAC_B_SENSE_CTL |
		   TVDAC_C_SENSE_CTL);
	tv_dac = DAC_CTL_OVERRIDE | DAC_A_0_7_V | DAC_B_0_7_V | DAC_C_0_7_V;
	OUTREG(TV_CTL, tv_ctl);
	OUTREG(TV_DAC, tv_dac);
	i830WaitForVblank(pScrn);
	tv_dac = INREG(TV_DAC);
	OUTREG(TV_DAC, save_tv_dac);
	OUTREG(TV_CTL, save_tv_ctl);
    }
    /*
     *  A B C
     *  0 1 1 Composite
     *  1 0 X svideo
     *  0 0 0 Component
     */
    if ((tv_dac & TVDAC_SENSE_MASK) == (TVDAC_B_SENSE | TVDAC_C_SENSE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Detected Composite TV connection\n");
	type = TV_TYPE_COMPOSITE;
    } else if ((tv_dac & (TVDAC_A_SENSE|TVDAC_B_SENSE)) == TVDAC_A_SENSE) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Detected S-Video TV connection\n");
	type = TV_TYPE_SVIDEO;
    } else if ((tv_dac & TVDAC_SENSE_MASK) == 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Detected Component TV connection\n");
	type = TV_TYPE_COMPONENT;
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Couldn't detect TV connection\n");
	type = TV_TYPE_NONE;
    }
    
    dev_priv->type = type;
}

/**
 * Detect the TV connection.
 *
 * Currently this always returns OUTPUT_STATUS_UNKNOWN, as we need to be sure
 * we have a pipe programmed in order to probe the TV.
 */
static xf86OutputStatus
i830_tv_detect(xf86OutputPtr output)
{
    xf86CrtcPtr		    crtc;
    DisplayModeRec	    mode;
    I830OutputPrivatePtr    intel_output = output->driver_private;
    struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;

    crtc = i830GetLoadDetectPipe (output);
    if (crtc)
    {
	if (intel_output->load_detect_temp)
	{
	    mode = tvModes[0];
	    xf86SetModeCrtc (&mode, INTERLACE_HALVE_V);
	    i830PipeSetMode (crtc, &mode, FALSE);
	}
	i830_tv_detect_type (crtc, output);
	i830ReleaseLoadDetectPipe (output);
    }
    
    switch (dev_priv->type) {
    case TV_TYPE_NONE:
	return XF86OutputStatusDisconnected;
    case TV_TYPE_UNKNOWN:
	return XF86OutputStatusUnknown;
    default:
	return XF86OutputStatusConnected;
    }
}

/**
 * Stub get_modes function.
 *
 * This should probably return a set of fixed modes, unless we can figure out
 * how to probe modes off of TV connections.
 */
static DisplayModePtr
i830_tv_get_modes(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    DisplayModePtr new;
    char stmp[32];

    (void) pI830;
    new             = xnfcalloc(1, sizeof (DisplayModeRec));
    sprintf(stmp, "480i");
    new->name       = xnfalloc(strlen(stmp) + 1);
    strcpy(new->name, stmp);
    
    new->Clock      = 108000;
    
    /*
    new->HDisplay   = 640;
    new->HSyncStart = 664;
    new->HSyncEnd   = 704;
    new->HTotal     = 832;
    
    new->VDisplay   = 480;
    new->VSyncStart = 489;
    new->VSyncEnd   = 491;
    new->VTotal     = 520;
     */
    new->HDisplay   = 1024;
    new->HSyncStart = 1048;
    new->HSyncEnd   = 1184;
    new->HTotal     = 1344;
    
    new->VDisplay   = 768;
    new->VSyncStart = 771;
    new->VSyncEnd   = 777;
    new->VTotal     = 806;

    new->type       = M_T_PREFERRED;

    return new;
}

static void
i830_tv_destroy (xf86OutputPtr output)
{
    if (output->driver_private)
	xfree (output->driver_private);
}

static const xf86OutputFuncsRec i830_tv_output_funcs = {
    .dpms = i830_tv_dpms,
    .save = i830_tv_save,
    .restore = i830_tv_restore,
    .mode_valid = i830_tv_mode_valid,
    .mode_fixup = i830_tv_mode_fixup,
    .mode_set = i830_tv_mode_set,
    .detect = i830_tv_detect,
    .get_modes = i830_tv_get_modes,
    .destroy = i830_tv_destroy
};

void
i830_tv_init(ScrnInfoPtr pScrn)
{
    I830Ptr		    pI830 = I830PTR(pScrn);
    xf86OutputPtr	    output;
    I830OutputPrivatePtr    intel_output;
    struct i830_tv_priv	    *dev_priv;
 
    if ((INREG(TV_CTL) & TV_FUSE_STATE_MASK) == TV_FUSE_STATE_DISABLED)
	return;

    output = xf86OutputCreate (pScrn, &i830_tv_output_funcs, "TV");
    
    if (!output)
	return;
    
    intel_output = xnfcalloc (sizeof (I830OutputPrivateRec) +
			      sizeof (struct i830_tv_priv), 1);
    if (!intel_output)
    {
	xf86OutputDestroy (output);
	return;
    }
    dev_priv = (struct i830_tv_priv *) (intel_output + 1);
    intel_output->type = I830_OUTPUT_SDVO;
    intel_output->dev_priv = dev_priv;
    dev_priv->type = TV_TYPE_UNKNOWN;
    
    output->driver_private = intel_output;
}
