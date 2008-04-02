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
#include "nv50_type.h"
#include "nv50_display.h"
#include "nv50_output.h"

#include <xf86DDC.h>

int
NV50OrOffset(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	return ffs(nv_output->or) - 1;
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
NV50OutputModeValid(xf86OutputPtr output, DisplayModePtr mode)
{
	if (mode->Clock > 400000)
		return MODE_CLOCK_HIGH;
	if (mode->Clock < 25000)
		return MODE_CLOCK_LOW;

	return MODE_OK;
}

void
NV50OutputPrepare(xf86OutputPtr output)
{
}

void
NV50OutputCommit(xf86OutputPtr output)
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
NV50OutputGetDDCModes(xf86OutputPtr output)
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

	if (nv_output->type == OUTPUT_TMDS && ddc_modes) {
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
