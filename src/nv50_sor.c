/*
 * Copyright 2007 NVIDIA, Corporation
 * Copyright 2008 Maarten Maathuis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nouveau_modeset.h"
#include "nouveau_crtc.h"
#include "nouveau_output.h"
#include "nouveau_connector.h"

static int
NV50SorModeValid(nouveauOutputPtr output, DisplayModePtr mode)
{
	int high_limit;

	if (output->type == OUTPUT_LVDS)
		high_limit = 400000;
	else
		high_limit = 165000; /* no dual link dvi until we figure it out completely */

	if (mode->Clock > high_limit)
		return MODE_CLOCK_HIGH;

	if (mode->Clock < 25000)
		return MODE_CLOCK_LOW;

	if (mode->Flags & V_DBLSCAN)
		return MODE_NO_DBLESCAN;

	if (mode->HDisplay > output->native_mode->HDisplay || mode->VDisplay > output->native_mode->VDisplay)
		return MODE_PANEL;

	return MODE_OK;
}

static void
NV50SorModeSet(nouveauOutputPtr output, DisplayModePtr mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50SorModeSet is called.\n");

	const int sorOff = 0x40 * NV50OrOffset(output);
	uint32_t mode_ctl = NV50_SOR_MODE_CTRL_OFF;

	if (!mode) {
		/* Disconnect the SOR */
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disconnecting SOR.\n");
		NV50DisplayCommand(pScrn, NV50_SOR0_MODE_CTRL + sorOff, mode_ctl);
		return;
	}

	/* Anyone know a more appropriate name? */
	DisplayModePtr desired_mode = output->crtc->use_native_mode ? output->crtc->native_mode : mode;

	if (output->type == OUTPUT_LVDS) {
		mode_ctl |= NV50_SOR_MODE_CTRL_LVDS;
	} else {
		mode_ctl |= NV50_SOR_MODE_CTRL_TMDS;
		if (desired_mode->Clock > 165000)
			mode_ctl |= NV50_SOR_MODE_CTRL_TMDS_DUAL_LINK;
	}

	if (output->crtc) {
		if (output->crtc->index == 1)
			mode_ctl |= NV50_SOR_MODE_CTRL_CRTC1;
		else
			mode_ctl |= NV50_SOR_MODE_CTRL_CRTC0;
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Warning, output has no crtc.\n");
		return;
	}

	if (desired_mode->Flags & V_NHSYNC)
		mode_ctl |= NV50_SOR_MODE_CTRL_NHSYNC;

	if (desired_mode->Flags & V_NVSYNC)
		mode_ctl |= NV50_SOR_MODE_CTRL_NVSYNC;

	// This wouldn't be necessary, but the server is stupid and calls
	// nv50_sor_dpms after the output is disconnected, even though the hardware
	// turns it off automatically.
	output->SetPowerMode(output, DPMSModeOn);

	NV50DisplayCommand(pScrn, NV50_SOR0_MODE_CTRL + sorOff, mode_ctl);

	output->crtc->SetScaleMode(output->crtc, output->scale_mode);
}

static void
NV50SorSetClockMode(nouveauOutputPtr output, int clock)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	const int limit = output->dcb->type == OUTPUT_LVDS ? 112000 : 165000;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "NV50SorSetClockMode is called.\n");

#if 0
	ret = nouveau_bios_run_display_table(pScrn, output->dcb, clock);
	if (ret)
		NV_WARN(pScrn, "failed to run output script, may hang!\n");
#else
	/* just to keep the original behaviour until we can reliably run
	 * the output scripts...
	 */
	if (output->dcb->type == OUTPUT_LVDS)
		return;
#endif

	/* 0x70000 was a late addition to nv, mentioned as fixing tmds
	 * initialisation on certain gpu's. */
	/* I presume it's some kind of clock setting, but what precisely i
	 * do not know.
	 */
	NVWrite(pNv, NV50_SOR0_CLK_CTRL2 + NV50OrOffset(output) * 0x800,
		     0x70000 | ((clock > limit) ? 0x101 : 0));
}

static int
NV50SorSense(nouveauOutputPtr output)
{
	switch (output->type) {
		case OUTPUT_TMDS:
		case OUTPUT_LVDS:
			return output->type;
		default:
			return OUTPUT_NONE;
	}
}

static void
NV50SorSetPowerMode(nouveauOutputPtr output, int mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50SorSetPowerMode is called with mode %d.\n", mode);

	NVPtr pNv = NVPTR(pScrn);
	uint32_t tmp;

	while ((NVRead(pNv, NV50_SOR0_DPMS_CTRL + NV50OrOffset(output) * 0x800) & NV50_SOR_DPMS_CTRL_PENDING));

	tmp = NVRead(pNv, NV50_SOR0_DPMS_CTRL + NV50OrOffset(output) * 0x800);
	tmp |= NV50_SOR_DPMS_CTRL_PENDING;

	if (mode == DPMSModeOn)
		tmp |= NV50_SOR_DPMS_CTRL_MODE_ON;
	else
		tmp &= ~NV50_SOR_DPMS_CTRL_MODE_ON;

	NVWrite(pNv, NV50_SOR0_DPMS_CTRL + NV50OrOffset(output) * 0x800, tmp);
	while ((NVRead(pNv, NV50_SOR0_DPMS_STATE + NV50OrOffset(output) * 0x800) & NV50_SOR_DPMS_STATE_WAIT));
}

static Bool
NV50SorDetect(nouveauOutputPtr output)
{
	if (output->type == OUTPUT_LVDS) /* assume connected */
		return TRUE;

	return FALSE;
}

static nouveauCrtcPtr
NV50SorGetCurrentCrtc(nouveauOutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50SorGetCurrentCrtc is called.\n");

	NVPtr pNv = NVPTR(pScrn);
	uint32_t mode_ctrl = NVRead(pNv, NV50_SOR0_MODE_CTRL_VAL + NV50OrOffset(output) * 0x8);

	/* 
	 * MODE_CTRL values only contain one instance of crtc0 and of crtc1.
	 * This is because we disconnect outputs upon modeset.
	 * Crtc might be off even if we get a positive return.
	 * But we are still associated with that crtc.
	 */
	if (mode_ctrl & NV50_SOR_MODE_CTRL_CRTC0)
		return pNv->crtc[0];
	else if (mode_ctrl & NV50_SOR_MODE_CTRL_CRTC1)
		return pNv->crtc[1];

	return NULL;
}

void
NV50SorSetFunctionPointers(nouveauOutputPtr output)
{
	output->ModeValid = NV50SorModeValid;
	output->ModeSet = NV50SorModeSet;
	output->SetClockMode = NV50SorSetClockMode;
	output->Sense = NV50SorSense;
	output->Detect = NV50SorDetect;
	output->SetPowerMode = NV50SorSetPowerMode;
	output->GetCurrentCrtc = NV50SorGetCurrentCrtc;
}

