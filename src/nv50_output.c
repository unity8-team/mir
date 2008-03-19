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

static xf86MonPtr
ProbeDDC(I2CBusPtr i2c)
{
	ScrnInfoPtr pScrn = xf86Screens[i2c->scrnIndex];
	NVPtr pNv = NVPTR(pScrn);
	xf86MonPtr monInfo = NULL;
	const int bus = i2c->DriverPrivate.val, off = bus * 0x18;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"Probing for EDID on I2C bus %i...\n", bus);
	pNv->REGS[(0x0000E138+off)/4] = 7;
	/* Should probably use xf86OutputGetEDID here */
	monInfo = xf86DoEDID_DDC2(pScrn->scrnIndex, i2c);
	pNv->REGS[(0x0000E138+off)/4] = 3;

	if(monInfo) {
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			"DDC detected a %s:\n", monInfo->features.input_type ?
			"DFP" : "CRT");
		xf86PrintEDID(monInfo);
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "  ... none found\n");
	}

	return monInfo;
}

DisplayModePtr
NV50OutputGetDDCModes(xf86OutputPtr output)
{
	/* The EDID is read as part of the detect step */
	output->funcs->detect(output);
	return xf86OutputGetEDIDModes(output);
}

void
NV50OutputDestroy(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	if (nv_output->pDDCBus)
		xf86DestroyI2CBusRec(nv_output->pDDCBus, TRUE, TRUE);

	nv_output->pDDCBus = NULL;
}
