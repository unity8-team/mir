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
#include "i830_bios.h"

/**
 * Sets the power state for the panel.
 */
static void
i830SetLVDSPanelPower(ScrnInfoPtr pScrn, Bool on)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 pp_status;
    CARD32 blc_pwm_ctl;
    int backlight_duty_cycle;

    blc_pwm_ctl = INREG (BLC_PWM_CTL);
    backlight_duty_cycle = blc_pwm_ctl & BACKLIGHT_DUTY_CYCLE_MASK;
    if (backlight_duty_cycle)
        pI830->backlight_duty_cycle = backlight_duty_cycle;

    if (on) {
	OUTREG(PP_CONTROL, INREG(PP_CONTROL) | POWER_TARGET_ON);
	do {
	    pp_status = INREG(PP_STATUS);
	} while ((pp_status & PP_ON) == 0);
	OUTREG(BLC_PWM_CTL,
	       (blc_pwm_ctl & ~BACKLIGHT_DUTY_CYCLE_MASK) |
	       pI830->backlight_duty_cycle);
    } else {
	OUTREG(BLC_PWM_CTL,
	       (blc_pwm_ctl & ~BACKLIGHT_DUTY_CYCLE_MASK));

	OUTREG(PP_CONTROL, INREG(PP_CONTROL) & ~POWER_TARGET_ON);
	do {
	    pp_status = INREG(PP_STATUS);
	} while (pp_status & PP_ON);
    }
}

static void
i830_lvds_dpms (xf86OutputPtr output, int mode)
{
    ScrnInfoPtr	pScrn = output->scrn;

    if (mode == DPMSModeOn)
	i830SetLVDSPanelPower(pScrn, TRUE);
    else
	i830SetLVDSPanelPower(pScrn, FALSE);

    /* XXX: We never power down the LVDS pair. */
}

static void
i830_lvds_save (xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    I830Ptr	pI830 = I830PTR(pScrn);

    pI830->savePP_ON = INREG(LVDSPP_ON);
    pI830->savePP_OFF = INREG(LVDSPP_OFF);
    pI830->saveLVDS = INREG(LVDS);
    pI830->savePP_CONTROL = INREG(PP_CONTROL);
    pI830->savePP_CYCLE = INREG(PP_CYCLE);
    pI830->saveBLC_PWM_CTL = INREG(BLC_PWM_CTL);
    pI830->backlight_duty_cycle = (pI830->saveBLC_PWM_CTL &
				   BACKLIGHT_DUTY_CYCLE_MASK);

    /*
     * If the light is off at server startup, just make it full brightness
     */
    if (pI830->backlight_duty_cycle == 0) {
	pI830->backlight_duty_cycle =
	    (pI830->saveBLC_PWM_CTL & BACKLIGHT_MODULATION_FREQ_MASK) >>
	    BACKLIGHT_MODULATION_FREQ_SHIFT;
    }
}

static void
i830_lvds_restore(xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    I830Ptr	pI830 = I830PTR(pScrn);

    OUTREG(BLC_PWM_CTL, pI830->saveBLC_PWM_CTL);
    OUTREG(LVDSPP_ON, pI830->savePP_ON);
    OUTREG(LVDSPP_OFF, pI830->savePP_OFF);
    OUTREG(PP_CYCLE, pI830->savePP_CYCLE);
    OUTREG(LVDS, pI830->saveLVDS);
    OUTREG(PP_CONTROL, pI830->savePP_CONTROL);
    if (pI830->savePP_CONTROL & POWER_TARGET_ON)
	i830SetLVDSPanelPower(pScrn, TRUE);
    else
	i830SetLVDSPanelPower(pScrn, FALSE);
}

static int
i830_lvds_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
   return MODE_OK;
}

static Bool
i830_lvds_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    I830CrtcPrivatePtr intel_crtc = output->crtc->driver_private;
    int i;

    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr other_output = pI830->xf86_config.output[i];

	if (other_output != output && other_output->crtc == output->crtc) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Can't enable LVDS and another output on the same "
		       "pipe\n");
	    return FALSE;
	}
    }

    if (intel_crtc->pipe == 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Can't support LVDS on pipe A\n");
	return FALSE;
    }

    /* If we have timings from the BIOS for the panel, put them in
     * to the adjusted mode.  The CRTC will be set up for this mode,
     * with the panel scaling set up to source from the H/VDisplay
     * of the original mode.
     */
    if (pI830->panel_fixed_hactive != 0) {
	adjusted_mode->HDisplay = pI830->panel_fixed_hactive;
	adjusted_mode->HTotal = adjusted_mode->HDisplay +
	    pI830->panel_fixed_hblank;
	adjusted_mode->HSyncStart = adjusted_mode->HDisplay +
	    pI830->panel_fixed_hsyncoff;
	adjusted_mode->HSyncStart = adjusted_mode->HSyncStart +
	    pI830->panel_fixed_hsyncwidth;
	adjusted_mode->VDisplay = pI830->panel_fixed_vactive;
	adjusted_mode->VTotal = adjusted_mode->VDisplay +
	    pI830->panel_fixed_hblank;
	adjusted_mode->VSyncStart = adjusted_mode->VDisplay +
	    pI830->panel_fixed_hsyncoff;
	adjusted_mode->VSyncStart = adjusted_mode->VSyncStart +
	    pI830->panel_fixed_hsyncwidth;
	adjusted_mode->Clock = pI830->panel_fixed_clock;
	xf86SetModeCrtc(adjusted_mode, INTERLACE_HALVE_V);
    }

    /* XXX: if we don't have BIOS fixed timings (or we have
     * a preferred mode from DDC, probably), we should use the
     * DDC mode as the fixed timing.
     */

    /* XXX: It would be nice to support lower refresh rates on the
     * panels to reduce power consumption, and perhaps match the
     * user's requested refresh rate.
     */

    return TRUE;
}

static void
i830_lvds_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		   DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 pfit_control;

    /* The LVDS pin pair needs to be on before the DPLLs are enabled.
     * This is an exception to the general rule that mode_set doesn't turn
     * things on.
     */
    OUTREG(LVDS, LVDS_PORT_EN | LVDS_PIPEB_SELECT);

    /* Enable automatic panel scaling so that non-native modes fill the
     * screen.  Should be enabled before the pipe is enabled, according to
     * register description and PRM.
     */
    pfit_control = (PFIT_ENABLE |
		    VERT_AUTO_SCALE | HORIZ_AUTO_SCALE |
		    VERT_INTERP_BILINEAR | HORIZ_INTERP_BILINEAR);

    if (pI830->panel_wants_dither)
	pfit_control |= PANEL_8TO6_DITHER_ENABLE;

    OUTREG(PFIT_CONTROL, pfit_control);
}

/**
 * Detect the LVDS connection.
 *
 * This always returns OUTPUT_STATUS_CONNECTED.  This output should only have
 * been set up if the LVDS was actually connected anyway.
 */
static enum detect_status
i830_lvds_detect(xf86OutputPtr output)
{
    return OUTPUT_STATUS_CONNECTED;
}

/**
 * Return the list of DDC modes if available, or the BIOS fixed mode otherwise.
 */
static DisplayModePtr
i830_lvds_get_modes(xf86OutputPtr output)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    I830Ptr	    pI830 = I830PTR(pScrn);
    DisplayModePtr  modes, new;
    char	    stmp[32];

    modes = i830_ddc_get_modes(output);
    if (modes != NULL)
	return modes;

    new             = xnfcalloc(1, sizeof (DisplayModeRec));
    sprintf(stmp, "%dx%d", pI830->PanelXRes, pI830->PanelYRes);
    new->name       = xnfalloc(strlen(stmp) + 1);
    strcpy(new->name, stmp);
    new->HDisplay   = pI830->PanelXRes;
    new->VDisplay   = pI830->PanelYRes;
    new->HSyncStart = pI830->panel_fixed_hactive + pI830->panel_fixed_hsyncoff;
    new->HSyncEnd   = new->HSyncStart + pI830->panel_fixed_hsyncwidth;
    new->HTotal     = new->HSyncEnd + 1;
    new->VSyncStart = pI830->panel_fixed_vactive + pI830->panel_fixed_vsyncoff;
    new->VSyncEnd   = new->VSyncStart + pI830->panel_fixed_vsyncwidth;
    new->VTotal     = new->VSyncEnd + 1;
    new->Clock      = pI830->panel_fixed_clock;

    new->type       = M_T_PREFERRED;

    return new;
}

static void
i830_lvds_destroy (xf86OutputPtr output)
{
    I830OutputPrivatePtr    intel_output = output->driver_private;

    if (intel_output)
	xfree (intel_output);
}

static const xf86OutputFuncsRec i830_lvds_output_funcs = {
    .dpms = i830_lvds_dpms,
    .save = i830_lvds_save,
    .restore = i830_lvds_restore,
    .mode_valid = i830_lvds_mode_valid,
    .mode_fixup = i830_lvds_mode_fixup,
    .mode_set = i830_lvds_mode_set,
    .detect = i830_lvds_detect,
    .get_modes = i830_lvds_get_modes,
    .destroy = i830_lvds_destroy
};

void
i830_lvds_init(ScrnInfoPtr pScrn)
{
    I830Ptr		    pI830 = I830PTR(pScrn);
    xf86OutputPtr	    output;
    I830OutputPrivatePtr    intel_output;


    /* Get the LVDS fixed mode out of the BIOS.  We should support LVDS with
     * the BIOS being unavailable or broken, but lack the configuration options
     * for now.
     */
    if (!i830GetLVDSInfoFromBIOS(pScrn))
	return;

    /* Blacklist machines with BIOSes that list an LVDS panel without actually
     * having one.
     */
    if (pI830->PciInfo->chipType == PCI_CHIP_I945_GM) {
	if (pI830->PciInfo->subsysVendor == 0xa0a0)  /* aopen mini pc */
	    return;

	if ((pI830->PciInfo->subsysVendor == 0x8086) &&
	    (pI830->PciInfo->subsysCard == 0x7270)) {
	    /* It's a Mac Mini or Macbook Pro.
	     *
	     * Apple hardware is out to get us.  The macbook pro has a real
	     * LVDS panel, but the mac mini does not, and they have the same
	     * device IDs.  We'll distinguish by panel size, on the assumption
	     * that Apple isn't about to make any machines with an 800x600
	     * display.
	     */

	    if (pI830->PanelXRes == 800 && pI830->PanelYRes == 600) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Suspected Mac Mini, ignoring the LVDS\n");
		return;
	    }
	}
   }

    output = xf86OutputCreate (pScrn, &i830_lvds_output_funcs, "Built-in LCD panel");
    if (!output)
	return;
    intel_output = xnfcalloc (sizeof (I830OutputPrivateRec), 1);
    if (!intel_output)
    {
	xf86OutputDestroy (output);
	return;
    }
    intel_output->type = I830_OUTPUT_LVDS;
    output->driver_private = intel_output;

    /* Set up the LVDS DDC channel.  Most panels won't support it, but it can
     * be useful if available.
     */
    I830I2CInit(pScrn, &intel_output->pDDCBus, GPIOC, "LVDSDDC_C");
}
