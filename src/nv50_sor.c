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
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	const int limit = 165000;

	NVWrite(pNv, 0x00614300 + nv_output->output_resource * 0x800, (pclk > limit) ? 0x101 : 0);
}

static void
NV50SorDPMSSet(xf86OutputPtr output, int mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	CARD32 tmp;

	while((NVRead(pNv, 0x0061c004 + nv_output->output_resource * 0x800) & 0x80000000));

	tmp = NVRead(pNv, 0x0061c004 + nv_output->output_resource * 0x800);
	tmp |= 0x80000000;

	if(mode == DPMSModeOn)
		tmp |= 1;
	else
		tmp &= ~1;

	NVWrite(pNv, 0x0061c004 + nv_output->output_resource * 0x800, tmp);
	while((NVRead(pNv, 0x0061c030 + nv_output->output_resource * 0x800) & 0x10000000));
}

static int
NV50TMDSModeValid(xf86OutputPtr output, DisplayModePtr mode)
{
	// Disable dual-link modes until I can find a way to make them work
	// reliably.
	if (mode->Clock > 165000)
		return MODE_CLOCK_HIGH;

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
	NVOutputPrivatePtr nv_output = output->driver_private;
	const int sorOff = 0x40 * nv_output->output_resource;
	CARD32 type;

	if(!adjusted_mode) {
		/* Disconnect the SOR */
		NV50DisplayCommand(pScrn, 0x600 + sorOff, 0);
		return;
	}

	if (nv_output->type == OUTPUT_LVDS) {
		type = 0;
	} else
	if (adjusted_mode->Clock > 165000) {
		type = 0x500;
	} else {
		type = 0x100;
	}

	// This wouldn't be necessary, but the server is stupid and calls
	// NV50SorDPMSSet after the output is disconnected, even though the hardware
	// turns it off automatically.
	NV50SorDPMSSet(output, DPMSModeOn);

	NV50DisplayCommand(pScrn, 0x600 + sorOff,
		(NV50CrtcGetHead(output->crtc) == HEAD0 ? 1 : 2) | type |
		((adjusted_mode->Flags & V_NHSYNC) ? 0x1000 : 0) |
		((adjusted_mode->Flags & V_NVSYNC) ? 0x2000 : 0));

	NV50CrtcSetScale(output->crtc, adjusted_mode, nv_output->scaling_mode);
}

static xf86OutputStatus
NV50SorDetect(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	xf86MonPtr ddc_mon;

	if (nv_output->pDDCBus == NULL)
		return XF86OutputStatusDisconnected;

	ddc_mon = xf86OutputGetEDID(output, nv_output->pDDCBus);
	if (!ddc_mon)
		return XF86OutputStatusDisconnected;

	if (!ddc_mon->features.input_type) /* Analog? */
		return XF86OutputStatusDisconnected;

	if (ddc_mon)
		xf86OutputSetEDID(output, ddc_mon);

	return XF86OutputStatusConnected;
}

static xf86OutputStatus
NV50SorLVDSDetect(xf86OutputPtr output)
{
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

static void
NV50SorSetModeBackend(DisplayModePtr dst, const DisplayModePtr src)
{
	// Stash the backend mode timings from src into dst
	dst->Clock           = src->Clock;
	dst->Flags           = src->Flags;
	dst->CrtcHDisplay    = src->CrtcHDisplay;
	dst->CrtcHBlankStart = src->CrtcHBlankStart;
	dst->CrtcHSyncStart  = src->CrtcHSyncStart;
	dst->CrtcHSyncEnd    = src->CrtcHSyncEnd;
	dst->CrtcHBlankEnd   = src->CrtcHBlankEnd;
	dst->CrtcHTotal      = src->CrtcHTotal;
	dst->CrtcHSkew       = src->CrtcHSkew;
	dst->CrtcVDisplay    = src->CrtcVDisplay;
	dst->CrtcVBlankStart = src->CrtcVBlankStart;
	dst->CrtcVSyncStart  = src->CrtcVSyncStart;
	dst->CrtcVSyncEnd    = src->CrtcVSyncEnd;
	dst->CrtcVBlankEnd   = src->CrtcVBlankEnd;
	dst->CrtcVTotal      = src->CrtcVTotal;
	dst->CrtcHAdjusted   = src->CrtcHAdjusted;
	dst->CrtcVAdjusted   = src->CrtcVAdjusted;
}

static Bool
NV50SorModeFixup(xf86OutputPtr output, DisplayModePtr mode,
		 DisplayModePtr adjusted_mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	DisplayModePtr native = nv_output->native_mode;

	if(native && nv_output->scaling_mode != SCALE_PANEL) {
		NV50SorSetModeBackend(adjusted_mode, native);
		// This mode is already "fixed"
		NV50CrtcSkipModeFixup(output->crtc);
	}

	return TRUE;
}

static Bool
NV50SorTMDSModeFixup(xf86OutputPtr output, DisplayModePtr mode,
			DisplayModePtr adjusted_mode)
{
	int scrnIndex = output->scrn->scrnIndex;
	NVOutputPrivatePtr nv_output = output->driver_private;
	DisplayModePtr modes = output->probed_modes;

	xf86DeleteMode(&nv_output->native_mode, nv_output->native_mode);

	if(modes) {
		// Find the preferred mode and use that as the "native" mode.
		// If no preferred mode is available, use the first one.
		DisplayModePtr mode;

		// Find the preferred mode.
		for(mode = modes; mode; mode = mode->next) {
			if(mode->type & M_T_PREFERRED) {
				xf86DrvMsgVerb(scrnIndex, X_INFO, 5,
						"%s: preferred mode is %s\n",
						output->name, mode->name);
				break;
			}
		}

		// XXX: May not want to allow scaling if no preferred mode is found.
		if(!mode) {
			mode = modes;
			xf86DrvMsgVerb(scrnIndex, X_INFO, 5,
				"%s: no preferred mode found, using %s\n",
				output->name, mode->name);
		}

		nv_output->native_mode = xf86DuplicateMode(mode);
		NV50CrtcDoModeFixup(nv_output->native_mode, mode);
	}

	return NV50SorModeFixup(output, mode, adjusted_mode);
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
	.mode_fixup = NV50SorTMDSModeFixup,
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

	mode->HDisplay = mode->CrtcHDisplay = width;
	mode->VDisplay = mode->CrtcVDisplay = height;
	mode->Clock = NVRead(pNv, 0x00610ad4 + off) & 0x3fffff;
	mode->CrtcHBlankStart = NVRead(pNv, 0x00610afc + off);
	mode->CrtcHSyncEnd = NVRead(pNv, 0x00610b04 + off);
	mode->CrtcHBlankEnd = NVRead(pNv, 0x00610ae8 + off);
	mode->CrtcHTotal = NVRead(pNv, 0x00610af4 + off);

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
	CARD32 val = NVRead(pNv, 0x00610050);

	if ((val & 0x3) == 0x2) {
		return ReadLVDSNativeMode(pScrn, 0);
	} else if ((val & 0x300) == 0x200) {
		return ReadLVDSNativeMode(pScrn, 0x540);
	}

	return NULL;
}
