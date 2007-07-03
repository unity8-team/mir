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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "i830.h"
#include "xf86Modes.h"
#include "i830_display.h"

static void
i830_crt_dpms(xf86OutputPtr output, int mode)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    I830Ptr	    pI830 = I830PTR(pScrn);
    CARD32	    temp;

    temp = INREG(ADPA);
    temp &= ~(ADPA_HSYNC_CNTL_DISABLE | ADPA_VSYNC_CNTL_DISABLE);
    temp &= ~ADPA_DAC_ENABLE;

    switch(mode) {
    case DPMSModeOn:
	temp |= ADPA_DAC_ENABLE;
	break;
    case DPMSModeStandby:
	temp |= ADPA_DAC_ENABLE | ADPA_HSYNC_CNTL_DISABLE;
	break;
    case DPMSModeSuspend:
	temp |= ADPA_DAC_ENABLE | ADPA_VSYNC_CNTL_DISABLE;
	break;
    case DPMSModeOff:
	temp |= ADPA_HSYNC_CNTL_DISABLE | ADPA_VSYNC_CNTL_DISABLE;
	break;
    }

    OUTREG(ADPA, temp);
}

static void
i830_crt_save (xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    I830Ptr	pI830 = I830PTR(pScrn);

    pI830->saveADPA = INREG(ADPA);
}

static void
i830_crt_restore (xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    I830Ptr	pI830 = I830PTR(pScrn);

    OUTREG(ADPA, pI830->saveADPA);
}

static int
i830_crt_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
    if (pMode->Flags & V_DBLSCAN)
	return MODE_NO_DBLESCAN;

    if (pMode->Clock > 400000 || pMode->Clock < 25000)
	return MODE_CLOCK_RANGE;

    return MODE_OK;
}

static Bool
i830_crt_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		    DisplayModePtr adjusted_mode)
{
    /* disable blanking so we can detect monitors */
    adjusted_mode->CrtcVBlankStart = adjusted_mode->CrtcVTotal - 3;
    return TRUE;
}

static void
i830_crt_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		  DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    xf86CrtcPtr		    crtc = output->crtc;
    I830CrtcPrivatePtr	    i830_crtc = crtc->driver_private;
    int			    dpll_md_reg;
    CARD32		    adpa, dpll_md;

    if (i830_crtc->pipe == 0) 
	dpll_md_reg = DPLL_A_MD;
    else
	dpll_md_reg = DPLL_B_MD;
    /*
     * Disable separate mode multiplier used when cloning SDVO to CRT
     * XXX this needs to be adjusted when we really are cloning
     */
    if (IS_I965G(pI830))
    {
	dpll_md = INREG(dpll_md_reg);
	OUTREG(dpll_md_reg, dpll_md & ~DPLL_MD_UDI_MULTIPLIER_MASK);
    }

    adpa = 0;
    if (adjusted_mode->Flags & V_PHSYNC)
	adpa |= ADPA_HSYNC_ACTIVE_HIGH;
    if (adjusted_mode->Flags & V_PVSYNC)
	adpa |= ADPA_VSYNC_ACTIVE_HIGH;

    if (i830_crtc->pipe == 0)
    {
	adpa |= ADPA_PIPE_A_SELECT;
	OUTREG(BCLRPAT_A, 0);
    }
    else
    {
	adpa |= ADPA_PIPE_B_SELECT;
	OUTREG(BCLRPAT_B, 0);
    }

    OUTREG(ADPA, adpa);
}

/**
 * Uses CRT_HOTPLUG_EN and CRT_HOTPLUG_STAT to detect CRT presence.
 *
 * Only for I945G/GM.
 *
 * \return TRUE if CRT is connected.
 * \return FALSE if CRT is disconnected.
 */
static Bool
i830_crt_detect_hotplug(xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    I830Ptr	pI830 = I830PTR(pScrn);
    CARD32	temp;
    const int	timeout_ms = 1000;
    int		starttime, curtime;

    temp = INREG(PORT_HOTPLUG_EN);

    OUTREG(PORT_HOTPLUG_EN, temp | CRT_HOTPLUG_FORCE_DETECT | (1 << 5));

    for (curtime = starttime = GetTimeInMillis();
	 (curtime - starttime) < timeout_ms; curtime = GetTimeInMillis())
    {
	if ((INREG(PORT_HOTPLUG_EN) & CRT_HOTPLUG_FORCE_DETECT) == 0)
	    break;
    }

    if ((INREG(PORT_HOTPLUG_STAT) & CRT_HOTPLUG_MONITOR_MASK) ==
	CRT_HOTPLUG_MONITOR_COLOR)
    {
	return TRUE;
    } else {
	return FALSE;
    }
}

/**
 * Detects CRT presence by checking for load.
 *
 * Requires that the current pipe's DPLL is active.  This will cause flicker
 * on the CRT, so it should not be used while the display is being used.  Only
 * color (not monochrome) displays are detected.
 *
 * \return TRUE if CRT is connected.
 * \return FALSE if CRT is disconnected.
 */
static Bool
i830_crt_detect_load (xf86CrtcPtr	    crtc,
		      xf86OutputPtr    output)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    I830CrtcPrivatePtr	    i830_crtc = I830CrtcPrivate(crtc);
    CARD32		    save_bclrpat;
    CARD32		    save_vtotal;
    CARD32		    vtotal, vactive;
    CARD32		    vsample;
    CARD32		    vblank, vblank_start, vblank_end;
    CARD32		    dsl;
    CARD8		    st00;
    int			    bclrpat_reg, pipeconf_reg, pipe_dsl_reg;
    int			    vtotal_reg;
    int			    vblank_reg;
    int			    pipe = i830_crtc->pipe;
    int			    count, detect;
    Bool		    present;

    if (pipe == 0) 
    {
	bclrpat_reg = BCLRPAT_A;
	vtotal_reg = VTOTAL_A;
	vblank_reg = VBLANK_A;
	pipeconf_reg = PIPEACONF;
	pipe_dsl_reg = PIPEA_DSL;
    }
    else 
    {
	bclrpat_reg = BCLRPAT_B;
	vtotal_reg = VTOTAL_B;
	vblank_reg = VBLANK_B;
	pipeconf_reg = PIPEBCONF;
	pipe_dsl_reg = PIPEB_DSL;
    }

    save_bclrpat = INREG(bclrpat_reg);
    save_vtotal = INREG(vtotal_reg);
    vblank = INREG(vblank_reg);
    
    vtotal = ((save_vtotal >> 16) & 0xfff) + 1;
    vactive = (save_vtotal & 0x7ff) + 1;

    vblank_start = (vblank & 0xfff) + 1;
    vblank_end = ((vblank >> 16) & 0xfff) + 1;
    
    /* sample in the vertical border, selecting the larger one */
    if (vblank_start - vactive >= vtotal - vblank_end)
	vsample = (vblank_start + vactive) >> 1;
    else
	vsample = (vtotal + vblank_end) >> 1;

    /* Set the border color to purple. */
    OUTREG(bclrpat_reg, 0x500050);
    
    /*
     * Wait for the border to be displayed
     */
    while (INREG(pipe_dsl_reg) >= vactive)
	;
    while ((dsl = INREG(pipe_dsl_reg)) <= vsample)
	;
    /*
     * Watch ST00 for an entire scanline
     */
    detect = 0;
    count = 0;
    do {
	count++;
	/* Read the ST00 VGA status register */
	st00 = pI830->readStandard(pI830, 0x3c2);
	if (st00 & (1 << 4))
	    detect++;
    } while ((INREG(pipe_dsl_reg) == dsl));

    /* Restore previous settings */
    OUTREG(bclrpat_reg, save_bclrpat);

    /*
     * If more than 3/4 of the scanline detected a monitor,
     * then it is assumed to be present. This works even on i830,
     * where there isn't any way to force the border color across
     * the screen
     */
    present = detect * 4 > count * 3;
    return present;
}

/**
 * Detects CRT presence by probing for a response on the DDC address.
 *
 * This takes approximately 5ms in testing on an i915GM, with CRT connected or
 * not.
 *
 * \return TRUE if the CRT is connected and responded to DDC.
 * \return FALSE if no DDC response was detected.
 */
static Bool
i830_crt_detect_ddc(xf86OutputPtr output)
{
    I830OutputPrivatePtr    i830_output = output->driver_private;

    /* CRT should always be at 0, but check anyway */
    if (i830_output->type != I830_OUTPUT_ANALOG)
	return FALSE;

    return xf86I2CProbeAddress(i830_output->pDDCBus, 0x00A0);
}

/**
 * Attempts to detect CRT presence through any method available.
 *
 * @param allow_disturb enables detection methods that may cause flickering
 *        on active displays.
 */
static xf86OutputStatus
i830_crt_detect(xf86OutputPtr output)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    xf86CrtcPtr	    crtc;

    if (IS_I945G(pI830) || IS_I945GM(pI830) || IS_I965G(pI830) ||
	    IS_G33CLASS(pI830)) {
	if (i830_crt_detect_hotplug(output))
	    return XF86OutputStatusConnected;
	else
	    return XF86OutputStatusDisconnected;
    }

    if (i830_crt_detect_ddc(output))
	return XF86OutputStatusConnected;

    /* Use the load-detect method if we have no other way of telling. */
    crtc = i830GetLoadDetectPipe (output);
    
    if (crtc)
    {
	/* VESA 640x480x72Hz mode to set on the pipe */
	static DisplayModeRec   mode = {
	    NULL, NULL, "640x480", MODE_OK, M_T_DEFAULT,
	    31500,
	    640, 664, 704, 832, 0,
	    480, 489, 491, 520, 0,
	    V_NHSYNC | V_NVSYNC,
	    0, 0,
	    0, 0, 0, 0, 0, 0, 0,
	    0, 0, 0, 0, 0, 0,
	    FALSE, FALSE, 0, NULL, 0, 0.0, 0.0
	};
	Bool			connected;
	I830OutputPrivatePtr	intel_output = output->driver_private;
	
	if (!crtc->enabled)
	{
	    xf86SetModeCrtc (&mode, INTERLACE_HALVE_V);
	    xf86CrtcSetMode (crtc, &mode, RR_Rotate_0, 0, 0);
	}
	else if (intel_output->load_detect_temp)
	{
	    output->funcs->mode_set (output, &crtc->mode, &crtc->mode);
	    output->funcs->commit (output);
	}
	connected = i830_crt_detect_load (crtc, output);

	i830ReleaseLoadDetectPipe (output);
	if (connected)
	    return XF86OutputStatusConnected;
	else
	    return XF86OutputStatusDisconnected;
    }

    return XF86OutputStatusUnknown;
}

static void
i830_crt_destroy (xf86OutputPtr output)
{
    if (output->driver_private)
	xfree (output->driver_private);
}

static const xf86OutputFuncsRec i830_crt_output_funcs = {
    .dpms = i830_crt_dpms,
    .save = i830_crt_save,
    .restore = i830_crt_restore,
    .mode_valid = i830_crt_mode_valid,
    .mode_fixup = i830_crt_mode_fixup,
    .prepare = i830_output_prepare,
    .mode_set = i830_crt_mode_set,
    .commit = i830_output_commit,
    .detect = i830_crt_detect,
    .get_modes = i830_ddc_get_modes,
    .destroy = i830_crt_destroy
};

void
i830_crt_init(ScrnInfoPtr pScrn)
{
    xf86OutputPtr	    output;
    I830OutputPrivatePtr    i830_output;
    I830Ptr		    pI830 = I830PTR(pScrn);

    output = xf86OutputCreate (pScrn, &i830_crt_output_funcs, "VGA");
    if (!output)
	return;
    i830_output = xnfcalloc (sizeof (I830OutputPrivateRec), 1);
    if (!i830_output)
    {
	xf86OutputDestroy (output);
	return;
    }
    i830_output->type = I830_OUTPUT_ANALOG;
    /* i830 (almador) cannot place the analog adaptor on pipe B */
    if (IS_I830(pI830))
	i830_output->pipe_mask = (1 << 0);
    else
	i830_output->pipe_mask = ((1 << 0) | (1 << 1));
    i830_output->clone_mask = ((1 << I830_OUTPUT_ANALOG) |
			       (1 << I830_OUTPUT_DVO_TMDS));
    
    output->driver_private = i830_output;
    output->interlaceAllowed = FALSE;
    output->doubleScanAllowed = FALSE;

    /* Set up the DDC bus. */
    I830I2CInit(pScrn, &i830_output->pDDCBus, GPIOA, "CRTDDC_A");
}
