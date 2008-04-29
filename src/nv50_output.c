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

#include <string.h>

#include "nv_include.h"

#include <xf86DDC.h>

int
NV50OrOffset(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	return ffs(nv_output->dcb->or) - 1;
}

void
NV50OutputSetPClk(xf86OutputPtr output, int pclk)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	if (nv_output->type == OUTPUT_TMDS)
		NV50SorSetPClk(output, pclk);

	if (nv_output->type == OUTPUT_ANALOG)
		NV50DacSetPClk(output, pclk);
}

int
nv50_output_mode_valid(xf86OutputPtr output, DisplayModePtr mode)
{
	if (mode->Clock > 400000)
		return MODE_CLOCK_HIGH;
	if (mode->Clock < 25000)
		return MODE_CLOCK_LOW;

	return MODE_OK;
}

void
NV50OutputInvalidateCache(ScrnInfoPtr pScrn)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	for (i = 0; i < xf86_config->num_output; i++) {
		NVOutputPrivatePtr nv_output = xf86_config->output[i]->driver_private;
		nv_output->valid_cache = FALSE;
	}
}

/* This also handles NULL partners. */
#define IS_OUTPUT_TYPE(outputPriv, typeReq) (outputPriv && outputPriv->type == typeReq)

xf86OutputStatus
nv50_output_detect(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	if (nv_output->valid_cache)
		return output->status;

	xf86OutputPtr output_partner = nv_output->partner;
	NVOutputPrivatePtr nv_output_partner = NULL;
	Bool load[2] = {FALSE, FALSE};

	if (output_partner)
		nv_output_partner = output_partner->driver_private;

	xf86MonPtr ddc_mon;

	if (!nv_output->pDDCBus)
		return XF86OutputStatusDisconnected;

	ddc_mon = NV50OutputGetEDID(output, nv_output->pDDCBus);

	/* Don't directly write the values, as any changes have to stay within this function. */
	/* We don't want the system to freak out. */
	if (!ddc_mon) {
		if (IS_OUTPUT_TYPE(nv_output, OUTPUT_ANALOG) && NV50DacLoadDetect(output))
			load[0] = TRUE;
		if (IS_OUTPUT_TYPE(nv_output_partner, OUTPUT_ANALOG) && NV50DacLoadDetect(output_partner))
			load[1] = TRUE;
	}

	/* Do this just before writing back the new values. */
	output->status = XF86OutputStatusDisconnected;
	if (output_partner)
		output_partner->status = XF86OutputStatusDisconnected;

	if (ddc_mon) {
		if (IS_OUTPUT_TYPE(nv_output, OUTPUT_ANALOG) && !ddc_mon->features.input_type)
			output->status = XF86OutputStatusConnected;
		if (IS_OUTPUT_TYPE(nv_output, OUTPUT_TMDS) && ddc_mon->features.input_type)
			output->status = XF86OutputStatusConnected;
		if (IS_OUTPUT_TYPE(nv_output_partner, OUTPUT_ANALOG) && !ddc_mon->features.input_type)
			output_partner->status = XF86OutputStatusConnected;
		if (IS_OUTPUT_TYPE(nv_output_partner, OUTPUT_TMDS) && ddc_mon->features.input_type)
			output_partner->status = XF86OutputStatusConnected;
	} else {
		if (load[0])
			output->status = XF86OutputStatusConnected;
		if (load[1])
			output_partner->status = XF86OutputStatusConnected;
	}

	nv_output->valid_cache = TRUE;
	if (output_partner)
		nv_output_partner->valid_cache = TRUE;

	return output->status;
}

void
nv50_output_prepare(xf86OutputPtr output)
{
}

void
nv50_output_commit(xf86OutputPtr output)
{
}

xf86MonPtr
NV50OutputGetEDID(xf86OutputPtr output, I2CBusPtr pDDCBus)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	xf86MonPtr rval = NULL;

	NVWrite(pNv, NV50_I2C_PORT(pDDCBus->DriverPrivate.val), NV50_I2C_START);

	rval = xf86OutputGetEDID(output, pDDCBus);

	NVWrite(pNv, NV50_I2C_PORT(pDDCBus->DriverPrivate.val), NV50_I2C_STOP);

	return rval;
}

DisplayModePtr
nv50_output_get_ddc_modes(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	xf86MonPtr ddc_mon;
	DisplayModePtr ddc_modes;

	ddc_mon = NV50OutputGetEDID(output, nv_output->pDDCBus);

	if (!ddc_mon)
		return NULL;

	xf86OutputSetEDID(output, ddc_mon);

	ddc_modes = xf86OutputGetEDIDModes(output);

	/* NV5x hardware can also do scaling on analog connections. */
	if (nv_output->type != OUTPUT_LVDS && ddc_modes) {
		xf86DeleteMode(&nv_output->native_mode, nv_output->native_mode);

		/* Use the first preferred mode as native mode. */
		DisplayModePtr mode;

		/* Find the preferred mode. */
		for (mode = ddc_modes; mode != NULL; mode = mode->next) {
			if (mode->type & M_T_PREFERRED) {
				xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 5,
						"%s: preferred mode is %s\n",
						output->name, mode->name);
				break;
			}
		}

		/* TODO: Scaling needs a native mode, maybe fail in a better way. */
		if (!mode) {
			mode = ddc_modes;
			xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 5,
				"%s: no preferred mode found, using %s\n",
				output->name, mode->name);
		}

		nv_output->native_mode = xf86DuplicateMode(mode);
		nv_output->fpWidth = nv_output->native_mode->HDisplay;
		nv_output->fpHeight = nv_output->native_mode->VDisplay;
	}

	return ddc_modes;
}

void
NV50OutputDestroy(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	if (nv_output->pDDCBus)
		xf86DestroyI2CBusRec(nv_output->pDDCBus, TRUE, TRUE);

	nv_output->pDDCBus = NULL;
}
