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

#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#include <X11/Xatom.h>

#include "nv_include.h"
#include "nv50_type.h"
#include "nv50_display.h"
#include "nv50_output.h"

void
NV50SorSetPClk(xf86OutputPtr output, int pclk)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50SorSetPClk is called.\n");

	NVPtr pNv = NVPTR(pScrn);
	const int limit = 165000;

	/* 0x70000 was a late addition to nv, mentioned as fixing tmds initialisation on certain gpu's. */
	/* This *may* have solved my shaking image problem, but i am not sure. */
	/* I presume it's some kind of clock setting, but what precisely i do not know. */
	NVWrite(pNv, NV50_SOR0_CLK_CTRL1 + NV50OrOffset(output) * 0x800, 0x70000 | ((pclk > limit) ? 0x101 : 0));
}

static void
NV50SorDPMSSet(xf86OutputPtr output, int mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50SorDPMSSet is called with mode %d.\n", mode);

	NVPtr pNv = NVPTR(pScrn);
	CARD32 tmp;

	while((NVRead(pNv, NV50_SOR0_DPMS_CTRL + NV50OrOffset(output) * 0x800) & NV50_SOR_DPMS_CTRL_PENDING));

	tmp = NVRead(pNv, NV50_SOR0_DPMS_CTRL + NV50OrOffset(output) * 0x800);
	tmp |= NV50_SOR_DPMS_CTRL_PENDING;

	if(mode == DPMSModeOn)
		tmp |= NV50_SOR_DPMS_CTRL_MODE_ON;
	else
		tmp &= ~NV50_SOR_DPMS_CTRL_MODE_ON;

	NVWrite(pNv, NV50_SOR0_DPMS_CTRL + NV50OrOffset(output) * 0x800, tmp);
	while((NVRead(pNv, 0x0061c030 + NV50OrOffset(output) * 0x800) & 0x10000000));
}

static int
NV50TMDSModeValid(xf86OutputPtr output, DisplayModePtr mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	// Disable dual-link modes until I can find a way to make them work
	// reliably.
	if (mode->Clock > 165000)
		return MODE_CLOCK_HIGH;

	if (mode->HDisplay > nv_output->fpWidth || mode->VDisplay > nv_output->fpHeight)
		return MODE_PANEL;

	return NV50OutputModeValid(output, mode);
}

static int
NV50LVDSModeValid(xf86OutputPtr output, DisplayModePtr mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	DisplayModePtr native = nv_output->native_mode;

	// Ignore modes larger than the native res.
	if (mode->HDisplay > native->HDisplay || mode->VDisplay > native->VDisplay)
		return MODE_PANEL;

	return NV50OutputModeValid(output, mode);
}

static void
NV50SorModeSet(xf86OutputPtr output, DisplayModePtr mode,
		DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50SorModeSet is called.\n");

	NVOutputPrivatePtr nv_output = output->driver_private;
	const int sorOff = 0x40 * NV50OrOffset(output);
	uint32_t mode_ctl = NV50_SOR_MODE_CTRL_OFF;

	if (!adjusted_mode) {
		/* Disconnect the SOR */
		NV50DisplayCommand(pScrn, NV50_SOR0_MODE_CTRL + sorOff, mode_ctl);
		return;
	}

	if (nv_output->type == OUTPUT_LVDS) {
		mode_ctl |= NV50_SOR_MODE_CTRL_LVDS;
	} else {
		mode_ctl |= NV50_SOR_MODE_CTRL_TMDS;
		if (adjusted_mode->Clock > 165000)
			mode_ctl |= NV50_SOR_MODE_CTRL_TMDS_DUAL_LINK;
	}

	if (output->crtc) {
		NVCrtcPrivatePtr nv_crtc = output->crtc->driver_private;
		if (nv_crtc->head == 1)
			mode_ctl |= NV50_SOR_MODE_CTRL_CRTC1;
		else
			mode_ctl |= NV50_SOR_MODE_CTRL_CRTC0;
	} else {
		return;
	}

	if (adjusted_mode->Flags & V_NHSYNC)
		mode_ctl |= NV50_SOR_MODE_CTRL_NHSYNC;

	if (adjusted_mode->Flags & V_NVSYNC)
		mode_ctl |= NV50_SOR_MODE_CTRL_NVSYNC;

	// This wouldn't be necessary, but the server is stupid and calls
	// NV50SorDPMSSet after the output is disconnected, even though the hardware
	// turns it off automatically.
	NV50SorDPMSSet(output, DPMSModeOn);

	NV50DisplayCommand(pScrn, NV50_SOR0_MODE_CTRL + sorOff, mode_ctl);

	NV50CrtcSetScale(output->crtc, mode, adjusted_mode, nv_output->scaling_mode);
}

static xf86OutputStatus
NV50SorDetect(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50SorDetect is called.\n");

	NVOutputPrivatePtr nv_output = output->driver_private;
	xf86MonPtr ddc_mon;

	if (nv_output->pDDCBus == NULL)
		return XF86OutputStatusDisconnected;

	ddc_mon = NV50OutputGetEDID(output, nv_output->pDDCBus);
	if (!ddc_mon)
		return XF86OutputStatusDisconnected;

	if (!ddc_mon->features.input_type) /* Analog? */
		return XF86OutputStatusDisconnected;

	return XF86OutputStatusConnected;
}

static xf86OutputStatus
NV50SorLVDSDetect(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50SorLVDSDetect is called.\n");

	/* Assume LVDS is always connected */
	return XF86OutputStatusConnected;
}

static void
NV50SorDestroy(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	NV50OutputDestroy(output);

	xf86DeleteMode(&nv_output->native_mode, nv_output->native_mode);

	xfree(output->driver_private);
	output->driver_private = NULL;
}

static Bool
NV50SorModeFixup(xf86OutputPtr output, DisplayModePtr mode,
		 DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50SorModeFixup is called.\n");

	NVOutputPrivatePtr nv_output = output->driver_private;

	if (nv_output->native_mode && nv_output->scaling_mode != SCALE_PANEL) {
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

static DisplayModePtr
NV50SorGetLVDSModes(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	return xf86DuplicateMode(nv_output->native_mode);
}

static const xf86OutputFuncsRec NV50SorTMDSOutputFuncs = {
	.dpms = NV50SorDPMSSet,
	.save = NULL,
	.restore = NULL,
	.mode_valid = NV50TMDSModeValid,
	.mode_fixup = NV50SorModeFixup,
	.prepare = NV50OutputPrepare,
	.commit = NV50OutputCommit,
	.mode_set = NV50SorModeSet,
	.detect = NV50SorDetect,
	.get_modes = NV50OutputGetDDCModes,
	.create_resources = nv_digital_output_create_resources,
	.set_property = nv_digital_output_set_property,
	.destroy = NV50SorDestroy,
};

static const xf86OutputFuncsRec NV50SorLVDSOutputFuncs = {
	.dpms = NV50SorDPMSSet,
	.save = NULL,
	.restore = NULL,
	.mode_valid = NV50LVDSModeValid,
	.mode_fixup = NV50SorModeFixup,
	.prepare = NV50OutputPrepare,
	.commit = NV50OutputCommit,
	.mode_set = NV50SorModeSet,
	.detect = NV50SorLVDSDetect,
	.get_modes = NV50SorGetLVDSModes,
	.create_resources = nv_digital_output_create_resources,
	.set_property = nv_digital_output_set_property,
	.destroy = NV50SorDestroy,
};

const xf86OutputFuncsRec * nv50_get_tmds_output_funcs()
{
	return &NV50SorTMDSOutputFuncs;
}

const xf86OutputFuncsRec * nv50_get_lvds_output_funcs()
{
	return &NV50SorLVDSOutputFuncs;
}

static DisplayModePtr
ReadLVDSNativeMode(ScrnInfoPtr pScrn, const int off)
{
	NVPtr pNv = NVPTR(pScrn);
	DisplayModePtr mode = xnfcalloc(1, sizeof(DisplayModeRec));
	const CARD32 size = NVRead(pNv, 0x00610b4c + off);
	const int width = size & 0x3fff;
	const int height = (size >> 16) & 0x3fff;

	mode->HDisplay = width;
	mode->VDisplay = height;
	mode->Clock = NVRead(pNv, 0x00610ad4 + off) & 0x3fffff;

	/* We should investigate what else is found in these register ranges. */
	uint32_t unk1 = NVRead(pNv, NV50_CRTC0_DISPLAY_TOTAL_VAL + off);
	uint32_t unk2 = NVRead(pNv, NV50_CRTC0_SYNC_DURATION_VAL + off);
	uint32_t unk3 = NVRead(pNv, NV50_CRTC0_SYNC_START_TO_BLANK_END_VAL + off);
	/*uint32_t unk4 = NVRead(pNv, NV50_CRTC0_MODE_UNK1_VAL + off);*/

	/* Recontruct our mode, so it can be handled normally. */
	mode->HTotal = (unk1 & 0xFFFF);
	mode->VTotal = (unk1 >> 16);

	/* Assuming no interlacing. */
	mode->HSyncStart = mode->HTotal - (unk3 & 0xFFFF) - 1;
	mode->VSyncStart = mode->VTotal - (unk3 >> 16) - 1;

	mode->HSyncEnd = mode->HSyncStart + (unk2 & 0xFFFF) + 1;
	mode->VSyncEnd = mode->VSyncStart + (unk2 >> 16) + 1;

	mode->next = mode->prev = NULL;
	mode->status = MODE_OK;
	mode->type = M_T_DRIVER | M_T_PREFERRED;

	xf86SetModeDefaultName(mode);

	return mode;
}

DisplayModePtr
GetLVDSNativeMode(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	uint32_t val = NVRead(pNv, NV50_DISPLAY_UNK50_CTRL);

	/* This is rather crude imo, i wonder if it always works. */
	if ((val & NV50_DISPLAY_UNK50_CTRL_CRTC0_MASK) == NV50_DISPLAY_UNK50_CTRL_CRTC0_ACTIVE) {
		return ReadLVDSNativeMode(pScrn, 0);
	} else if ((val & NV50_DISPLAY_UNK50_CTRL_CRTC1_MASK) == NV50_DISPLAY_UNK50_CTRL_CRTC1_ACTIVE) {
		return ReadLVDSNativeMode(pScrn, 0x540);
	}

	return NULL;
}
