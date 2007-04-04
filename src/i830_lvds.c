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
#include "i830_display.h"
#include "X11/Xatom.h"

/**
 * Sets the backlight level.
 *
 * \param level backlight level, from 0 to i830_lvds_get_max_backlight().
 */
static void
i830_lvds_set_backlight(ScrnInfoPtr pScrn, int level)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 blc_pwm_ctl;

    blc_pwm_ctl = INREG(BLC_PWM_CTL) & ~BACKLIGHT_DUTY_CYCLE_MASK;
    OUTREG(BLC_PWM_CTL, blc_pwm_ctl | (level << BACKLIGHT_DUTY_CYCLE_SHIFT));
}

/**
 * Returns the maximum level of the backlight duty cycle field.
 */
static CARD32
i830_lvds_get_max_backlight(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    
    return ((INREG(BLC_PWM_CTL) & BACKLIGHT_MODULATION_FREQ_MASK) >>
	BACKLIGHT_MODULATION_FREQ_SHIFT) * 2;
}

/**
 * Sets the power state for the panel.
 */
static void
i830SetLVDSPanelPower(ScrnInfoPtr pScrn, Bool on)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 pp_status;

    if (on) {
	OUTREG(PP_CONTROL, INREG(PP_CONTROL) | POWER_TARGET_ON);
	do {
	    pp_status = INREG(PP_STATUS);
	} while ((pp_status & PP_ON) == 0);

	i830_lvds_set_backlight(pScrn, pI830->backlight_duty_cycle);
    } else {
	i830_lvds_set_backlight(pScrn, 0);

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

    /* XXX: We never power down the LVDS pairs. */
}

static void
i830_lvds_save (xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    I830Ptr	pI830 = I830PTR(pScrn);

    pI830->savePP_ON = INREG(LVDSPP_ON);
    pI830->savePP_OFF = INREG(LVDSPP_OFF);
    pI830->savePP_CONTROL = INREG(PP_CONTROL);
    pI830->savePP_CYCLE = INREG(PP_CYCLE);
    pI830->saveBLC_PWM_CTL = INREG(BLC_PWM_CTL);
    pI830->backlight_duty_cycle = (pI830->saveBLC_PWM_CTL &
				   BACKLIGHT_DUTY_CYCLE_MASK);

    /*
     * If the light is off at server startup, just make it full brightness
     */
    if (pI830->backlight_duty_cycle == 0)
	pI830->backlight_duty_cycle = i830_lvds_get_max_backlight(pScrn);
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
    OUTREG(PP_CONTROL, pI830->savePP_CONTROL);
    if (pI830->savePP_CONTROL & POWER_TARGET_ON)
	i830SetLVDSPanelPower(pScrn, TRUE);
    else
	i830SetLVDSPanelPower(pScrn, FALSE);
}

static int
i830_lvds_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
    ScrnInfoPtr pScrn = output->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    DisplayModePtr  pFixedMode = pI830->panel_fixed_mode;

    if (pFixedMode)
    {
	if (pMode->HDisplay > pFixedMode->HDisplay)
	    return MODE_PANEL;
	if (pMode->VDisplay > pFixedMode->VDisplay)
	    return MODE_PANEL;
    }

    return MODE_OK;
}

static Bool
i830_lvds_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    I830Ptr pI830 = I830PTR(pScrn);
    I830CrtcPrivatePtr intel_crtc = output->crtc->driver_private;
    int i;

    for (i = 0; i < xf86_config->num_output; i++) {
	xf86OutputPtr other_output = xf86_config->output[i];

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
    if (pI830->panel_fixed_mode != NULL) {
	adjusted_mode->HDisplay = pI830->panel_fixed_mode->HDisplay;
	adjusted_mode->HSyncStart = pI830->panel_fixed_mode->HSyncStart;
	adjusted_mode->HSyncEnd = pI830->panel_fixed_mode->HSyncEnd;
	adjusted_mode->HTotal = pI830->panel_fixed_mode->HTotal;
	adjusted_mode->VDisplay = pI830->panel_fixed_mode->VDisplay;
	adjusted_mode->VSyncStart = pI830->panel_fixed_mode->VSyncStart;
	adjusted_mode->VSyncEnd = pI830->panel_fixed_mode->VSyncEnd;
	adjusted_mode->VTotal = pI830->panel_fixed_mode->VTotal;
	adjusted_mode->Clock = pI830->panel_fixed_mode->Clock;
	xf86SetModeCrtc(adjusted_mode, INTERLACE_HALVE_V);
    }

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
    I830CrtcPrivatePtr intel_crtc = output->crtc->driver_private;
    CARD32 pfit_control;

    /* The LVDS pin pair will already have been turned on in the
     * i830_crtc_mode_set since it has a large impact on the DPLL settings.
     */

    /* Enable automatic panel scaling so that non-native modes fill the
     * screen.  Should be enabled before the pipe is enabled, according to
     * register description and PRM.
     */
    pfit_control = (PFIT_ENABLE |
		    VERT_AUTO_SCALE | HORIZ_AUTO_SCALE |
		    VERT_INTERP_BILINEAR | HORIZ_INTERP_BILINEAR);

    if (!IS_I965G(pI830)) {
	if (pI830->panel_wants_dither)
	    pfit_control |= PANEL_8TO6_DITHER_ENABLE;
    } else {
	pfit_control |= intel_crtc->pipe << PFIT_PIPE_SHIFT;
    }

    OUTREG(PFIT_CONTROL, pfit_control);
}

/**
 * Detect the LVDS connection.
 *
 * This always returns OUTPUT_STATUS_CONNECTED.  This output should only have
 * been set up if the LVDS was actually connected anyway.
 */
static xf86OutputStatus
i830_lvds_detect(xf86OutputPtr output)
{
    return XF86OutputStatusConnected;
}

/**
 * Return the list of DDC modes if available, or the BIOS fixed mode otherwise.
 */
static DisplayModePtr
i830_lvds_get_modes(xf86OutputPtr output)
{
    I830OutputPrivatePtr    intel_output = output->driver_private;
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    xf86MonPtr		    edid_mon;
    DisplayModePtr	    modes;

    edid_mon = xf86OutputGetEDID (output, intel_output->pDDCBus);
    xf86OutputSetEDID (output, edid_mon);
    
    modes = xf86OutputGetEDIDModes (output);
    if (modes != NULL)
	return modes;

    if (!output->MonInfo)
    {
	edid_mon = xcalloc (1, sizeof (xf86Monitor));
	if (edid_mon)
	{
	    /* Set wide sync ranges so we get all modes
	     * handed to valid_mode for checking
	     */
	    edid_mon->det_mon[0].type = DS_RANGES;
	    edid_mon->det_mon[0].section.ranges.min_v = 0;
	    edid_mon->det_mon[0].section.ranges.max_v = 200;
	    edid_mon->det_mon[0].section.ranges.min_h = 0;
	    edid_mon->det_mon[0].section.ranges.max_h = 200;
	    
	    output->MonInfo = edid_mon;
	}
    }

    if (pI830->panel_fixed_mode != NULL)
	return xf86DuplicateMode(pI830->panel_fixed_mode);

    return NULL;
}

static void
i830_lvds_destroy (xf86OutputPtr output)
{
    I830OutputPrivatePtr    intel_output = output->driver_private;

    if (intel_output)
	xfree (intel_output);
}

#ifdef RANDR_12_INTERFACE
#define BACKLIGHT_NAME	"BACKLIGHT"
static Atom backlight_atom;
#endif /* RANDR_12_INTERFACE */

static void
i830_lvds_create_resources(xf86OutputPtr output)
{
#ifdef RANDR_12_INTERFACE
    ScrnInfoPtr pScrn = output->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    INT32 range[2];
    int data, err;

    /* Set up the backlight property, which takes effect immediately
     * and accepts values only within the range.
     *
     * XXX: Currently, RandR doesn't verify that properties set are
     * within the range.
     */
    backlight_atom = MakeAtom(BACKLIGHT_NAME, sizeof(BACKLIGHT_NAME) - 1,
	TRUE);

    range[0] = 0;
    range[1] = i830_lvds_get_max_backlight(pScrn);
    err = RRConfigureOutputProperty(output->randr_output, backlight_atom,
				    FALSE, TRUE, FALSE, 2, range);
    if (err != 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "RRConfigureOutputProperty error, %d\n", err);
    }
    /* Set the current value of the backlight property */
    data = pI830->backlight_duty_cycle;
    err = RRChangeOutputProperty(output->randr_output, backlight_atom,
				 XA_INTEGER, 32, PropModeReplace, 1, &data,
				 FALSE, TRUE);
    if (err != 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "RRChangeOutputProperty error, %d\n", err);
    }

#endif /* RANDR_12_INTERFACE */
}

#ifdef RANDR_12_INTERFACE
static Bool
i830_lvds_set_property(xf86OutputPtr output, Atom property,
		       RRPropertyValuePtr value)
{
    ScrnInfoPtr pScrn = output->scrn;
    I830Ptr pI830 = I830PTR(pScrn);
    
    if (property == backlight_atom) {
	INT32 val;

	if (value->type != XA_INTEGER || value->format != 32 ||
	    value->size != 1)
	{
	    return FALSE;
	}

	val = *(INT32 *)value->data;
	if (val < 0 || val > i830_lvds_get_max_backlight(pScrn))
	    return FALSE;

	if (val != pI830->backlight_duty_cycle)
	{
	    i830_lvds_set_backlight(pScrn, val);
	    pI830->backlight_duty_cycle = val;
	}
	return TRUE;
    }

    return TRUE;
}
#endif /* RANDR_12_INTERFACE */

static const xf86OutputFuncsRec i830_lvds_output_funcs = {
    .create_resources = i830_lvds_create_resources,
    .dpms = i830_lvds_dpms,
    .save = i830_lvds_save,
    .restore = i830_lvds_restore,
    .mode_valid = i830_lvds_mode_valid,
    .mode_fixup = i830_lvds_mode_fixup,
    .prepare = i830_output_prepare,
    .mode_set = i830_lvds_mode_set,
    .commit = i830_output_commit,
    .detect = i830_lvds_detect,
    .get_modes = i830_lvds_get_modes,
#ifdef RANDR_12_INTERFACE
    .set_property = i830_lvds_set_property,
#endif
    .destroy = i830_lvds_destroy
};

void
i830_lvds_init(ScrnInfoPtr pScrn)
{
    I830Ptr		    pI830 = I830PTR(pScrn);
    xf86OutputPtr	    output;
    I830OutputPrivatePtr    intel_output;
    DisplayModePtr	    modes, scan, bios_mode;

    output = xf86OutputCreate (pScrn, &i830_lvds_output_funcs, "LVDS");
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
    output->subpixel_order = SubPixelHorizontalRGB;
    output->interlaceAllowed = FALSE;
    output->doubleScanAllowed = FALSE;

    /* Set up the LVDS DDC channel.  Most panels won't support it, but it can
     * be useful if available.
     */
    I830I2CInit(pScrn, &intel_output->pDDCBus, GPIOC, "LVDSDDC_C");

    /* Attempt to get the fixed panel mode from DDC.  Assume that the preferred
     * mode is the right one.
     */
    modes = i830_ddc_get_modes(output);
    for (scan = modes; scan != NULL; scan = scan->next) {
	if (scan->type & M_T_PREFERRED)
	    break;
    }
    if (scan != NULL) {
	/* Pull our chosen mode out and make it the fixed mode */
	if (modes == scan)
	    modes = modes->next;
	if (scan->prev != NULL)
	    scan->prev = scan->next;
	if (scan->next != NULL)
	    scan->next = scan->prev;
	pI830->panel_fixed_mode = scan;
    }
    /* Delete the mode list */
    while (modes != NULL)
	xf86DeleteMode(&modes, modes);

    /* If we didn't get EDID, try checking if the panel is already turned on.
     * If so, assume that whatever is currently programmed is the correct mode.
     */
    if (pI830->panel_fixed_mode == NULL) {
	CARD32 lvds = INREG(LVDS);
	int pipe = (lvds & LVDS_PIPEB_SELECT) ? 1 : 0;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	xf86CrtcPtr crtc = xf86_config->crtc[pipe];

	if (lvds & LVDS_PORT_EN) {
	    pI830->panel_fixed_mode = i830_crtc_mode_get(pScrn, crtc);
	    if (pI830->panel_fixed_mode != NULL)
		pI830->panel_fixed_mode->type |= M_T_PREFERRED;
	}
    }

    /* Get the LVDS fixed mode out of the BIOS.  We should support LVDS with
     * the BIOS being unavailable or broken, but lack the configuration options
     * for now.
     */
    bios_mode = i830_bios_get_panel_mode(pScrn);
    if (bios_mode != NULL) {
	if (pI830->panel_fixed_mode != NULL) {
	    if (pI830->debug_modes &&
		!xf86ModesEqual(pI830->panel_fixed_mode, bios_mode))
	    {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "BIOS panel mode data doesn't match probed data, "
			   "continuing with probed.\n");
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BIOS mode:\n");
		xf86PrintModeline(pScrn->scrnIndex, bios_mode);
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "probed mode:\n");
		xf86PrintModeline(pScrn->scrnIndex, pI830->panel_fixed_mode);
		xfree(bios_mode->name);
		xfree(bios_mode);
	    }
	}  else {
	    pI830->panel_fixed_mode = bios_mode;
	}
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Couldn't detect panel mode.  Disabling panel\n");
	goto disable_exit;
    }

    /* Blacklist machines with BIOSes that list an LVDS panel without actually
     * having one.
     */
    if (pI830->PciInfo->chipType == PCI_CHIP_I945_GM) {
	if (pI830->PciInfo->subsysVendor == 0xa0a0)  /* aopen mini pc */
	    goto disable_exit;

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

	    if (pI830->panel_fixed_mode != NULL &&
		pI830->panel_fixed_mode->HDisplay == 800 &&
		pI830->panel_fixed_mode->VDisplay == 600)
	    {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Suspected Mac Mini, ignoring the LVDS\n");
		goto disable_exit;
	    }
	}
    }

    return;

disable_exit:
    xf86DestroyI2CBusRec(intel_output->pDDCBus, TRUE, TRUE);
    xf86OutputDestroy(output);
}
