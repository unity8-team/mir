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
#include "i830_xf86Modes.h"

static void
i830_crt_dpms(ScrnInfoPtr pScrn, I830OutputPtr output, int mode)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 temp;

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
i830_crt_save(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->saveADPA = INREG(ADPA);
}

static void
i830_crt_restore(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);

    OUTREG(ADPA, pI830->saveADPA);
}

static int
i830_crt_mode_valid(ScrnInfoPtr pScrn, I830OutputPtr output,
		    DisplayModePtr pMode)
{
    if (pMode->Flags & V_DBLSCAN)
	return MODE_NO_DBLESCAN;

    if (pMode->Clock > 400000 || pMode->Clock < 25000)
	return MODE_CLOCK_RANGE;

    return MODE_OK;
}

static void
i830_crt_pre_set_mode(ScrnInfoPtr pScrn, I830OutputPtr output,
		      DisplayModePtr pMode)
{
}

static void
i830_crt_post_set_mode(ScrnInfoPtr pScrn, I830OutputPtr output,
		       DisplayModePtr pMode)
{
    I830Ptr pI830 = I830PTR(pScrn);

    CARD32 adpa;

    adpa = ADPA_DAC_ENABLE;

    if (pMode->Flags & V_PHSYNC)
	adpa |= ADPA_HSYNC_ACTIVE_HIGH;
    if (pMode->Flags & V_PVSYNC)
	adpa |= ADPA_VSYNC_ACTIVE_HIGH;

    if (output->pipe == 0)
	adpa |= ADPA_PIPE_A_SELECT;
    else
	adpa |= ADPA_PIPE_B_SELECT;

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
i830_crt_detect_hotplug(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 temp;
    const int timeout_ms = 1000;
    int starttime, curtime;

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
i830_crt_detect_load(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 adpa, pipeconf, bclrpat;
    CARD8 st00;
    int pipeconf_reg, bclrpat_reg, dpll_reg;
    int pipe;

    for (pipe = 0; pipe < pI830->num_pipes; pipe++)
	if (!pI830->pipes[pipe].planeEnabled)
	    break;
    
    /* No available pipes for load detection */
    if (pipe == pI830->num_pipes)
	return FALSE;
    
    if (pipe == 0) {
	bclrpat_reg = BCLRPAT_A;
	pipeconf_reg = PIPEACONF;
	dpll_reg = DPLL_A;
    } else {
	bclrpat_reg = BCLRPAT_B;
	pipeconf_reg = PIPEBCONF;
	dpll_reg = DPLL_B;
    }

    /* Don't try this if the DPLL isn't running. */
    if (!(INREG(dpll_reg) & DPLL_VCO_ENABLE))
	return FALSE;

    adpa = INREG(ADPA);

    /* Enable CRT output if disabled. */
    if (!(adpa & ADPA_DAC_ENABLE)) {
	OUTREG(ADPA, adpa | ADPA_DAC_ENABLE |
	       ((pipe == 1) ? ADPA_PIPE_B_SELECT : 0));
    }

    /* Set the border color to purple.  Maybe we should save/restore this
     * reg.
     */
    bclrpat = INREG(bclrpat_reg);
    OUTREG(bclrpat_reg, 0x00500050);

    /* Force the border color through the active region */
    pipeconf = INREG(pipeconf_reg);
    OUTREG(pipeconf_reg, pipeconf | PIPECONF_FORCE_BORDER);

    /* Read the ST00 VGA status register */
    st00 = pI830->readStandard(pI830, 0x3c2);

    /* Restore previous settings */
    OUTREG(bclrpat_reg, bclrpat);
    OUTREG(pipeconf_reg, pipeconf);
    OUTREG(ADPA, adpa);

    if (st00 & (1 << 4))
	return TRUE;
    else
	return FALSE;
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
i830_crt_detect_ddc(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    struct _I830OutputRec *output;

    output = &pI830->output[0];
    /* CRT should always be at 0, but check anyway */
    if (output->type != I830_OUTPUT_ANALOG)
	return FALSE;

    return xf86I2CProbeAddress(output->pDDCBus, 0x00A0);
}

/**
 * Attempts to detect CRT presence through any method available.
 *
 * @param allow_disturb enables detection methods that may cause flickering
 *        on active displays.
 */
static enum detect_status
i830_crt_detect(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (IS_I945G(pI830) || IS_I945GM(pI830) || IS_I965G(pI830)) {
	if (i830_crt_detect_hotplug(pScrn))
	    return OUTPUT_STATUS_CONNECTED;
	else
	    return OUTPUT_STATUS_DISCONNECTED;
    }

    if (i830_crt_detect_ddc(pScrn))
	return OUTPUT_STATUS_CONNECTED;

    /* Use the load-detect method if we're not currently outputting to the CRT,
     * or we don't care.
     *
     * Actually, the method is unreliable currently.  We need to not share a
     * pipe, as it seems having other outputs on that pipe will result in a
     * false positive.
     */
    if (0) {
	if (i830_crt_detect_load(pScrn))
	    return OUTPUT_STATUS_CONNECTED;
	else
	    return OUTPUT_STATUS_DISCONNECTED;
    }

    return OUTPUT_STATUS_UNKNOWN;
}

static DisplayModePtr
i830_crt_get_modes(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    DisplayModePtr modes;
    MonRec fixed_mon;

    modes = i830_ddc_get_modes(pScrn, output);
    if (modes != NULL)
	return modes;

    if (output->detect(pScrn, output) == OUTPUT_STATUS_DISCONNECTED)
	return NULL;

    /* We've got a potentially-connected monitor that we can't DDC.  Return a
     * fixed set of VESA plus user modes for a presumed multisync monitor with
     * some reasonable limits.
     */
    fixed_mon.nHsync = 1;
    fixed_mon.hsync[0].lo = 31.0;
    fixed_mon.hsync[0].hi = 100.0;
    fixed_mon.nVrefresh = 1;
    fixed_mon.vrefresh[0].lo = 50.0;
    fixed_mon.vrefresh[0].hi = 70.0;

    modes = i830xf86DuplicateModes(pScrn, pScrn->monitor->Modes);
    i830xf86ValidateModesSync(pScrn, modes, &fixed_mon);
    i830xf86PruneInvalidModes(pScrn, &modes, TRUE);

    return modes;
}

void
i830_crt_init(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->output[pI830->num_outputs].type = I830_OUTPUT_ANALOG;
    pI830->output[pI830->num_outputs].dpms = i830_crt_dpms;
    pI830->output[pI830->num_outputs].save = i830_crt_save;
    pI830->output[pI830->num_outputs].restore = i830_crt_restore;
    pI830->output[pI830->num_outputs].mode_valid = i830_crt_mode_valid;
    pI830->output[pI830->num_outputs].pre_set_mode = i830_crt_pre_set_mode;
    pI830->output[pI830->num_outputs].post_set_mode = i830_crt_post_set_mode;
    pI830->output[pI830->num_outputs].detect = i830_crt_detect;
    pI830->output[pI830->num_outputs].get_modes = i830_crt_get_modes;

    /* Set up the DDC bus. */
    I830I2CInit(pScrn, &pI830->output[pI830->num_outputs].pDDCBus,
		GPIOA, "CRTDDC_A");

    pI830->num_outputs++;
}
