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
NV50DacModeValid(nouveauOutputPtr output, DisplayModePtr mode)
{
	if (mode->Clock > 400000)
		return MODE_CLOCK_HIGH;

	if (mode->Clock < 25000)
		return MODE_CLOCK_LOW;

	return MODE_OK;
}

static void
NV50DacModeSet(nouveauOutputPtr output, DisplayModePtr mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50DacModeSet is called.\n");

	const int dacOff = 0x80 * NV50OrOffset(output);
	uint32_t mode_ctl = NV50_DAC_MODE_CTRL_OFF;
	uint32_t mode_ctl2 = 0;

	if (!mode) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disconnecting DAC.\n");
		NV50DisplayCommand(pScrn, NV50_DAC0_MODE_CTRL + dacOff, mode_ctl);
		return;
	}

	/* Anyone know a more appropriate name? */
	DisplayModePtr desired_mode = output->crtc->use_native_mode ? output->crtc->native_mode : mode;

	if (output->crtc) {
		if (output->crtc->index == 1)
			mode_ctl |= NV50_DAC_MODE_CTRL_CRTC1;
		else
			mode_ctl |= NV50_DAC_MODE_CTRL_CRTC0;
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Warning, output has no crtc.\n");
		return;
	}

	if (output->type == OUTPUT_ANALOG) {
		/* What is this? */
		mode_ctl |= 0x40;
	} else if (output->type == OUTPUT_TV) {
		mode_ctl |= 0x100;
	}

	if (desired_mode->Flags & V_NHSYNC)
		mode_ctl2 |= NV50_DAC_MODE_CTRL2_NHSYNC;

	if (desired_mode->Flags & V_NVSYNC)
		mode_ctl2 |= NV50_DAC_MODE_CTRL2_NVSYNC;

	// This wouldn't be necessary, but the server is stupid and calls
	// nv50_dac_dpms after the output is disconnected, even though the hardware
	// turns it off automatically.
	output->SetPowerMode(output, DPMSModeOn);

	NV50DisplayCommand(pScrn, NV50_DAC0_MODE_CTRL + dacOff, mode_ctl);

	NV50DisplayCommand(pScrn, NV50_DAC0_MODE_CTRL2 + dacOff, mode_ctl2);

	output->crtc->SetScaleMode(output->crtc, output->scale_mode);
}

static void
NV50DacSetClockMode(nouveauOutputPtr output, int clock)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50DacSetClockMode is called.\n");

	NVPtr pNv = NVPTR(pScrn);
	NVWrite(pNv, NV50_DAC0_CLK_CTRL1 + NV50OrOffset(output) * 0x800, 0);
}

static int
NV50DacSense(nouveauOutputPtr output)
{
	switch (output->type) {
		case OUTPUT_ANALOG:
		case OUTPUT_TV:
			return output->type;
		default:
			return OUTPUT_NONE;
	}
}

static Bool
NV50DacDetect (nouveauOutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	const int scrnIndex = pScrn->scrnIndex;
	int sigstate;
	uint32_t load, tmp, tmp2;

	xf86DrvMsg(scrnIndex, X_PROBED, "Trying load detection on VGA%i ... ",
		NV50OrOffset(output));

	NVWrite(pNv, NV50_DAC0_CLK_CTRL2 + NV50OrOffset(output) * 0x800, 0x00000001);
	tmp2 = NVRead(pNv, NV50_DAC0_DPMS_CTRL + NV50OrOffset(output) * 0x800);

	NVWrite(pNv, NV50_DAC0_DPMS_CTRL + NV50OrOffset(output) * 0x800, NV50_DAC_DPMS_CTRL_DEFAULT_STATE | NV50_DAC_DPMS_CTRL_PENDING);
	while (NVRead(pNv, NV50_DAC0_DPMS_CTRL + NV50OrOffset(output) * 0x800) & NV50_DAC_DPMS_CTRL_PENDING);
	/* The blob seems to try various patterns after each other. */
	tmp = (pNv->NVArch == 0x50) ? 420 : 340;
	NVWrite(pNv, NV50_DAC0_LOAD_CTRL + NV50OrOffset(output) * 0x800, tmp | NV50_DAC_LOAD_CTRL_ACTIVE);
	/* Why is this needed, load detect is almost instantanious and seemingly reliable for me. */
	sigstate = xf86BlockSIGIO();
	usleep(45000);
	xf86UnblockSIGIO(sigstate);
	load = NVRead(pNv, NV50_DAC0_LOAD_CTRL + NV50OrOffset(output) * 0x800);
	NVWrite(pNv, NV50_DAC0_LOAD_CTRL + NV50OrOffset(output) * 0x800, 0);
	NVWrite(pNv, NV50_DAC0_DPMS_CTRL + NV50OrOffset(output) * 0x800, NV50_DAC_DPMS_CTRL_PENDING | tmp2);

	// Use this DAC if all three channels show load.
	if ((load & NV50_DAC_LOAD_CTRL_PRESENT) == NV50_DAC_LOAD_CTRL_PRESENT) {
		xf86ErrorF("found one!\n");
		return TRUE;
	}

	xf86ErrorF("nothing.\n");
	return FALSE;
}

static void
NV50DacSetPowerMode(nouveauOutputPtr output, int mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50DacSetPowerMode is called with mode %d.\n", mode);

	uint32_t tmp;
	NVPtr pNv = NVPTR(pScrn);

	/*
	 * DPMSModeOn       everything on
	 * DPMSModeStandby  hsync disabled, vsync enabled
	 * DPMSModeSuspend  hsync enabled, vsync disabled
	 * DPMSModeOff      sync disabled
	 */
	while(NVRead(pNv, NV50_DAC0_DPMS_CTRL + NV50OrOffset(output) * 0x800) & NV50_DAC_DPMS_CTRL_PENDING);

	tmp = NVRead(pNv, NV50_DAC0_DPMS_CTRL + NV50OrOffset(output) * 0x800);
	tmp &= ~0x7f;
	tmp |= NV50_DAC_DPMS_CTRL_PENDING;

	if (mode == DPMSModeStandby || mode == DPMSModeOff)
		tmp |= NV50_DAC_DPMS_CTRL_HSYNC_OFF;
	if (mode == DPMSModeSuspend || mode == DPMSModeOff)
		tmp |= NV50_DAC_DPMS_CTRL_VSYNC_OFF;
	if (mode != DPMSModeOn)
		tmp |= NV50_DAC_DPMS_CTRL_BLANK;
	if (mode == DPMSModeOff)
		tmp |= NV50_DAC_DPMS_CTRL_OFF;

	NVWrite(pNv, NV50_DAC0_DPMS_CTRL + NV50OrOffset(output) * 0x800, tmp);
}

void
NV50DacSetFunctionPointers(nouveauOutputPtr output)
{
	output->ModeValid = NV50DacModeValid;
	output->ModeSet = NV50DacModeSet;
	output->SetClockMode = NV50DacSetClockMode;
	output->Sense = NV50DacSense;
	output->Detect = NV50DacDetect;
	output->GetFixedMode = NULL;
	output->SetPowerMode = NV50DacSetPowerMode;
}
