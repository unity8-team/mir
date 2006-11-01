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
    CARD32 pp_status, pp_control;
    CARD32 blc_pwm_ctl;
    int backlight_duty_cycle;

    blc_pwm_ctl = INREG (BLC_PWM_CTL);
    backlight_duty_cycle = blc_pwm_ctl & BACKLIGHT_DUTY_CYCLE_MASK;
    if (backlight_duty_cycle)
        pI830->backlight_duty_cycle = backlight_duty_cycle;

    if (on) {
	OUTREG(PP_STATUS, INREG(PP_STATUS) | PP_ON);
	OUTREG(PP_CONTROL, INREG(PP_CONTROL) | POWER_TARGET_ON);
	do {
	    pp_status = INREG(PP_STATUS);
	    pp_control = INREG(PP_CONTROL);
	} while (!(pp_status & PP_ON) && !(pp_control & POWER_TARGET_ON));
	OUTREG(BLC_PWM_CTL,
	       (blc_pwm_ctl & ~BACKLIGHT_DUTY_CYCLE_MASK) |
	       pI830->backlight_duty_cycle);
    } else {
	OUTREG(BLC_PWM_CTL,
	       (blc_pwm_ctl & ~BACKLIGHT_DUTY_CYCLE_MASK));

	OUTREG(PP_STATUS, INREG(PP_STATUS) & ~PP_ON);
	OUTREG(PP_CONTROL, INREG(PP_CONTROL) & ~POWER_TARGET_ON);
	do {
	    pp_status = INREG(PP_STATUS);
	    pp_control = INREG(PP_CONTROL);
	} while ((pp_status & PP_ON) || (pp_control & POWER_TARGET_ON));
    }
}

static void
i830_lvds_dpms(ScrnInfoPtr pScrn, I830OutputPtr output, int mode)
{
    if (mode == DPMSModeOn)
	i830SetLVDSPanelPower(pScrn, TRUE);
    else
	i830SetLVDSPanelPower(pScrn, FALSE);
}

static void
i830_lvds_save(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->savePFIT_CONTROL = INREG(PFIT_CONTROL);
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
i830_lvds_restore(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);

    OUTREG(BLC_PWM_CTL, pI830->saveBLC_PWM_CTL);
    OUTREG(LVDSPP_ON, pI830->savePP_ON);
    OUTREG(LVDSPP_OFF, pI830->savePP_OFF);
    OUTREG(PP_CYCLE, pI830->savePP_CYCLE);
    OUTREG(PFIT_CONTROL, pI830->savePFIT_CONTROL);
    OUTREG(LVDS, pI830->saveLVDS);
    OUTREG(PP_CONTROL, pI830->savePP_CONTROL);
    if (pI830->savePP_CONTROL & POWER_TARGET_ON)
	i830SetLVDSPanelPower(pScrn, TRUE);
    else
	i830SetLVDSPanelPower(pScrn, FALSE);
}

static int
i830_lvds_mode_valid(ScrnInfoPtr pScrn, I830OutputPtr output,
		    DisplayModePtr pMode)
{
   return MODE_OK;
}

static void
i830_lvds_pre_set_mode(ScrnInfoPtr pScrn, I830OutputPtr output,
		       DisplayModePtr pMode)
{
    /* Always make sure the LVDS is off before we play with DPLLs and pipe
     * configuration.  We can skip this in some cases (for example, going
     * between hi-res modes with automatic panel scaling are fine), but be
     * conservative for now.
     */
    i830SetLVDSPanelPower(pScrn, FALSE);
}

static void
i830_lvds_post_set_mode(ScrnInfoPtr pScrn, I830OutputPtr output,
			DisplayModePtr pMode)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32  pfit_control;

    /* Enable automatic panel scaling so that non-native modes fill the
     * screen.  Should be enabled before the pipe is enabled, according to
     * register description.
     */
    pfit_control = (PFIT_ENABLE |
		    VERT_AUTO_SCALE | HORIZ_AUTO_SCALE |
		    VERT_INTERP_BILINEAR | HORIZ_INTERP_BILINEAR);

    if (pI830->panel_wants_dither)
	pfit_control |= PANEL_8TO6_DITHER_ENABLE;

    OUTREG(PFIT_CONTROL, pfit_control);

    /* Disable the PLL before messing with LVDS enable */
    OUTREG(FPB0, INREG(FPB0) & ~DPLL_VCO_ENABLE);

    /* LVDS must be powered on before PLL is enabled and before power
     * sequencing the panel.
     */
    OUTREG(LVDS, INREG(LVDS) | LVDS_PORT_EN | LVDS_PIPEB_SELECT);

    /* Re-enable the PLL */
    OUTREG(FPB0, INREG(FPB0) | DPLL_VCO_ENABLE);

    i830SetLVDSPanelPower(pScrn, TRUE);
}

/**
 * Detect the LVDS connection.
 *
 * This always returns OUTPUT_STATUS_CONNECTED.  This output should only have
 * been set up if the LVDS was actually connected anyway.
 */
static enum detect_status
i830_lvds_detect(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    return OUTPUT_STATUS_CONNECTED;
}

void
i830_lvds_init(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

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

    pI830->output[pI830->num_outputs].type = I830_OUTPUT_LVDS;
    pI830->output[pI830->num_outputs].dpms = i830_lvds_dpms;
    pI830->output[pI830->num_outputs].save = i830_lvds_save;
    pI830->output[pI830->num_outputs].restore = i830_lvds_restore;
    pI830->output[pI830->num_outputs].mode_valid = i830_lvds_mode_valid;
    pI830->output[pI830->num_outputs].pre_set_mode = i830_lvds_pre_set_mode;
    pI830->output[pI830->num_outputs].post_set_mode = i830_lvds_post_set_mode;
    pI830->output[pI830->num_outputs].detect = i830_lvds_detect;
    /* This will usually return NULL on laptop panels, which is no good.
     * We need to construct a mode from the fixed panel info, and return a copy
     * of that when DDC is unavailable.
     */
    pI830->output[pI830->num_outputs].get_modes = i830_ddc_get_modes;

    /* Set up the LVDS DDC channel.  Most panels won't support it, but it can
     * be useful if available.
     */
    I830I2CInit(pScrn, &pI830->output[pI830->num_outputs].pDDCBus,
		GPIOC, "LVDSDDC_C");

    pI830->num_outputs++;
}
