/*
 * Copyright (c) 2007 NVIDIA, Corporation
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
#include "nv50_display.h"
#include "nv50_output.h"

void
NV50DacSetPClk(xf86OutputPtr output, int pclk)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVWrite(pNv, 0x00614280 + ffs(nv_output->or) * 0x800, 0);
}

static void
NV50DacDPMSSet(xf86OutputPtr output, int mode)
{
	CARD32 tmp;
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/*
	 * DPMSModeOn       everything on
	 * DPMSModeStandby  hsync disabled, vsync enabled
	 * DPMSModeSuspend  hsync enabled, vsync disabled
	 * DPMSModeOff      sync disabled
	 */
	while(NVRead(pNv, 0x0061a004 + ffs(nv_output->or) * 0x800) & 0x80000000);

	tmp = NVRead(pNv, 0x0061a004 + ffs(nv_output->or) * 0x800);
	tmp &= ~0x7f;
	tmp |= 0x80000000;

	if(mode == DPMSModeStandby || mode == DPMSModeOff)
		tmp |= 1;
	if(mode == DPMSModeSuspend || mode == DPMSModeOff)
		tmp |= 4;
	if(mode != DPMSModeOn)
		tmp |= 0x10;
	if(mode == DPMSModeOff)
		tmp |= 0x40;

	NVWrite(pNv, 0x0061a004 + ffs(nv_output->or) * 0x800, tmp);
}

Bool
NV50DacModeFixup(xf86OutputPtr output, DisplayModePtr mode,
		 DisplayModePtr adjusted_mode)
{
	return TRUE;
}

static void
NV50DacModeSet(xf86OutputPtr output, DisplayModePtr mode,
		DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVOutputPrivatePtr nv_output = output->driver_private;
	const int dacOff = 0x80 * ffs(nv_output->or);

	if(!adjusted_mode) {
		NV50DisplayCommand(pScrn, 0x400 + dacOff, 0);
		return;
	}

	// This wouldn't be necessary, but the server is stupid and calls
	// NV50DacDPMSSet after the output is disconnected, even though the hardware
	// turns it off automatically.
	NV50DacDPMSSet(output, DPMSModeOn);

	NV50DisplayCommand(pScrn, 0x400 + dacOff,
		(NV50CrtcGetHead(output->crtc) == HEAD0 ? 1 : 2) | 0x40);

	NV50DisplayCommand(pScrn, 0x404 + dacOff,
		(adjusted_mode->Flags & V_NHSYNC) ? 1 : 0 |
		(adjusted_mode->Flags & V_NVSYNC) ? 2 : 0);

	NV50CrtcSetScale(output->crtc, adjusted_mode, SCALE_PANEL);
}

/*
 * Perform DAC load detection to determine if there is a connected display.
 */
static xf86OutputStatus
NV50DacDetect(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	xf86MonPtr ddc_mon;

	if (nv_output->pDDCBus == NULL)
		return XF86OutputStatusDisconnected;

	ddc_mon = xf86OutputGetEDID(output, nv_output->pDDCBus);
	if (!ddc_mon && !NV50DacLoadDetect(output))
		return XF86OutputStatusDisconnected;

	if (ddc_mon && ddc_mon->features.input_type) /* DVI? */
		return XF86OutputStatusDisconnected;

	if (ddc_mon)
		xf86OutputSetEDID(output, ddc_mon);

	return XF86OutputStatusConnected;
}

Bool
NV50DacLoadDetect(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputPrivatePtr nv_output = output->driver_private;
	const int scrnIndex = pScrn->scrnIndex;
	int sigstate;
	CARD32 load, tmp, tmp2;

	xf86DrvMsg(scrnIndex, X_PROBED, "Trying load detection on VGA%i ... ",
		ffs(nv_output->or));

	NVWrite(pNv, 0x0061a010 + ffs(nv_output->or) * 0x800, 0x00000001);
	tmp2 = NVRead(pNv, 0x0061a004 + ffs(nv_output->or) * 0x800);

	NVWrite(pNv, 0x0061a004 + ffs(nv_output->or) * 0x800, 0x80150000);
	while(NVRead(pNv, 0x0061a004 + ffs(nv_output->or) * 0x800) & 0x80000000);
	tmp = (pNv->NVArch == 0x50) ? 420 : 340;
	NVWrite(pNv, 0x0061a00c + ffs(nv_output->or) * 0x800, tmp | 0x100000);
	sigstate = xf86BlockSIGIO();
	usleep(45000);
	xf86UnblockSIGIO(sigstate);
	load = NVRead(pNv, 0x0061a00c + ffs(nv_output->or) * 0x800);
	NVWrite(pNv, 0x0061a00c + ffs(nv_output->or) * 0x800, 0);
	NVWrite(pNv, 0x0061a004 + ffs(nv_output->or) * 0x800, 0x80000000 | tmp2);

	// Use this DAC if all three channels show load.
	if((load & 0x38000000) == 0x38000000) {
		xf86ErrorF("found one!\n");
		return TRUE;
	}

	xf86ErrorF("nothing.\n");
	return FALSE;
}

static void
NV50DacDestroy(xf86OutputPtr output)
{
	NV50OutputDestroy(output);

	xfree(output->driver_private);
	output->driver_private = NULL;
}

static const xf86OutputFuncsRec NV50DacOutputFuncs = {
	.dpms = NV50DacDPMSSet,
	.save = NULL,
	.restore = NULL,
	.mode_valid = NV50OutputModeValid,
	.mode_fixup = NV50DacModeFixup,
	.prepare = NV50OutputPrepare,
	.commit = NV50OutputCommit,
	.mode_set = NV50DacModeSet,
	.detect = NV50DacDetect,
	.get_modes = NV50OutputGetDDCModes,
	.destroy = NV50DacDestroy,
};

const xf86OutputFuncsRec * nv50_get_analog_output_funcs()
{
	return &NV50DacOutputFuncs;
}

