/*
 * Copyright (c) 2007 NVIDIA, Corporation
 * Copyright (c) 2008 Maarten Maathuis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <unistd.h>

#define DPMS_SERVER
#include <X11/extensions/dpms.h>

#include "nv_include.h"

void
NV50DacSetPClk(xf86OutputPtr output, int pclk)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50DacSetPClk is called.\n");

	NVPtr pNv = NVPTR(pScrn);
	NVWrite(pNv, NV50_DAC0_CLK_CTRL1 + NV50OrOffset(output) * 0x800, 0);
}

static void
nv50_dac_dpms(xf86OutputPtr output, int mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_dac_dpms is called with mode %d.\n", mode);

	CARD32 tmp;
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

	if(mode == DPMSModeStandby || mode == DPMSModeOff)
		tmp |= NV50_DAC_DPMS_CTRL_HSYNC_OFF;
	if(mode == DPMSModeSuspend || mode == DPMSModeOff)
		tmp |= NV50_DAC_DPMS_CTRL_VSYNC_OFF;
	if(mode != DPMSModeOn)
		tmp |= NV50_DAC_DPMS_CTRL_BLANK;
	if(mode == DPMSModeOff)
		tmp |= NV50_DAC_DPMS_CTRL_OFF;

	NVWrite(pNv, NV50_DAC0_DPMS_CTRL + NV50OrOffset(output) * 0x800, tmp);
}

Bool
nv50_dac_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		 DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_dac_mode_fixup is called.\n");

	NVOutputPrivatePtr nv_output = output->driver_private;

	/* NV5x hardware supports scaling on analog outputs as well. */
	if (nv_output->native_mode && nv_output->scaling_mode != SCALE_PANEL) {
		if ((adjusted_mode->VDisplay > nv_output->fpHeight) || 
			(adjusted_mode->HDisplay > nv_output->fpWidth)) {

			/* Let's not do stupid things when the resolution exceeds the "native" resolution. */
			return TRUE;
		}
		adjusted_mode->HDisplay = nv_output->native_mode->HDisplay;
		adjusted_mode->HSkew = nv_output->native_mode->HSkew;
		adjusted_mode->HSyncStart = nv_output->native_mode->HSyncStart;
		adjusted_mode->HSyncEnd = nv_output->native_mode->HSyncEnd;
		adjusted_mode->HTotal = nv_output->native_mode->HTotal;
		adjusted_mode->VDisplay = nv_output->native_mode->VDisplay;
		adjusted_mode->VScan = nv_output->native_mode->VScan;
		adjusted_mode->VSyncStart = nv_output->native_mode->VSyncStart;
		adjusted_mode->VSyncEnd = nv_output->native_mode->VSyncEnd;
		adjusted_mode->VTotal = nv_output->native_mode->VTotal;
		adjusted_mode->Clock = nv_output->native_mode->Clock;
		adjusted_mode->Flags = nv_output->native_mode->Flags;

		/* No INTERLACE_HALVE_V, because we manually correct. */
		xf86SetModeCrtc(adjusted_mode, 0);
	}
	return TRUE;
}

static void
nv50_dac_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_dac_mode_set is called.\n");

	NVOutputPrivatePtr nv_output = output->driver_private;
	const int dacOff = 0x80 * NV50OrOffset(output);
	uint32_t mode_ctl = NV50_DAC_MODE_CTRL_OFF;
	uint32_t mode_ctl2 = 0;

	if (!adjusted_mode) {
		NV50DisplayCommand(pScrn, NV50_DAC0_MODE_CTRL + dacOff, mode_ctl);
		return;
	}

	if (output->crtc) {
		NVCrtcPrivatePtr nv_crtc = output->crtc->driver_private;
		if (nv_crtc->head == 1)
			mode_ctl |= NV50_DAC_MODE_CTRL_CRTC1;
		else
			mode_ctl |= NV50_DAC_MODE_CTRL_CRTC0;
	} else {
		return;
	}

	/* What is this? */
	mode_ctl |= 0x40;

	if (adjusted_mode->Flags & V_NHSYNC)
		mode_ctl2 |= NV50_DAC_MODE_CTRL2_NHSYNC;

	if (adjusted_mode->Flags & V_NVSYNC)
		mode_ctl2 |= NV50_DAC_MODE_CTRL2_NVSYNC;

	// This wouldn't be necessary, but the server is stupid and calls
	// nv50_dac_dpms after the output is disconnected, even though the hardware
	// turns it off automatically.
	nv50_dac_dpms(output, DPMSModeOn);

	NV50DisplayCommand(pScrn, NV50_DAC0_MODE_CTRL + dacOff, mode_ctl);

	NV50DisplayCommand(pScrn, NV50_DAC0_MODE_CTRL2 + dacOff, mode_ctl2);

	NV50CrtcSetScale(output->crtc, mode, adjusted_mode, nv_output->scaling_mode);
}

/*
 * Perform DAC load detection to determine if there is a connected display.
 */
static xf86OutputStatus
nv50_dac_detect(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_dac_detect is called.\n");

	NVOutputPrivatePtr nv_output = output->driver_private;
	xf86MonPtr ddc_mon;

	if (nv_output->pDDCBus == NULL)
		return XF86OutputStatusDisconnected;

	ddc_mon = NV50OutputGetEDID(output, nv_output->pDDCBus);
	if (!ddc_mon && !NV50DacLoadDetect(output))
		return XF86OutputStatusDisconnected;

	if (ddc_mon && ddc_mon->features.input_type) /* DVI? */
		return XF86OutputStatusDisconnected;

	return XF86OutputStatusConnected;
}

Bool
NV50DacLoadDetect(xf86OutputPtr output)
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
	while(NVRead(pNv, NV50_DAC0_DPMS_CTRL + NV50OrOffset(output) * 0x800) & NV50_DAC_DPMS_CTRL_PENDING);
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
nv50_dac_destroy(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	NV50OutputDestroy(output);

	xf86DeleteMode(&nv_output->native_mode, nv_output->native_mode);

	xfree(output->driver_private);
	output->driver_private = NULL;
}

static const xf86OutputFuncsRec nv50_analog_output_funcs = {
	.dpms = nv50_dac_dpms,
	.save = NULL,
	.restore = NULL,
	.mode_valid = nv50_output_mode_valid,
	.mode_fixup = nv50_dac_mode_fixup,
	.prepare = nv50_output_prepare,
	.commit = nv50_output_commit,
	.mode_set = nv50_dac_mode_set,
	.detect = nv50_dac_detect,
	.get_modes = nv50_output_get_ddc_modes,
	.destroy = nv50_dac_destroy,
	.create_resources = nv_output_create_resources,
	.set_property = nv_output_set_property,
};

const xf86OutputFuncsRec * nv50_get_analog_output_funcs()
{
	return &nv50_analog_output_funcs;
}

