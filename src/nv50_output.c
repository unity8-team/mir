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

static Bool NV50ReadPortMapping(int scrnIndex, NVPtr pNv)
{
	 unsigned const char *VBIOS = (unsigned const char *)pNv->VBIOS.data;
	 unsigned char *table2;
	 unsigned char headerSize, entries;
	 int i;
	 CARD16 a;
	 CARD32 b;

	 if (!VBIOS)
		goto fail;

	 /* Clear the i2c map to invalid */
	 for (i = 0; i < 4; i++)
		pNv->i2cMap[i].dac = pNv->i2cMap[i].sor = -1;

	if (*(CARD16*)VBIOS != 0xaa55) goto fail;

	a = *(CARD16*)(VBIOS + 0x36);
	table2 = (unsigned char*)VBIOS + a;

	if (table2[0] != 0x40) goto fail;

	b = *(CARD32*)(table2 + 6);
	if (b != 0x4edcbdcb) goto fail;

	headerSize = table2[1];
	entries = table2[2];

	for(i = 0; i < entries; i++) {
		int type, port;
		ORNum or;

		b = *(CARD32*)&table2[headerSize + 8*i];
		type = b & 0xf;
		port = (b >> 4) & 0xf;
		or = ffs((b >> 24) & 0xf) - 1;

		if (type == 0xe)
			break;

		if(type < 4) {
			switch(type) {
				case 0: /* CRT */
					if(pNv->i2cMap[port].dac != -1) {
						xf86DrvMsg(scrnIndex, X_WARNING,
							"DDC routing table corrupt!  DAC %i -> %i "
							"for port %i\n",
							or, pNv->i2cMap[port].dac, port);
					}
					pNv->i2cMap[port].dac = or;
					break;
				case 1: /* TV */
					break;
				case 2: /* TMDS */
					if(pNv->i2cMap[port].sor != -1)
						xf86DrvMsg(scrnIndex, X_WARNING,
							"DDC routing table corrupt!  SOR %i -> %i "
							"for port %i\n",
							or, pNv->i2cMap[port].sor, port);
					pNv->i2cMap[port].sor = or;
					break;
				case 3: /* LVDS */
					pNv->lvds.present = TRUE;
					pNv->lvds.or = or;
					break;
			}
		}
	}

	xf86DrvMsg(scrnIndex, X_PROBED, "Connector map:\n");
	if (pNv->lvds.present) {
		xf86DrvMsg(scrnIndex, X_PROBED,
			"  [N/A] -> SOR%i (LVDS)\n", pNv->lvds.or);
	}
	 for(i = 0; i < 4; i++) {
		if(pNv->i2cMap[i].dac != -1)
			xf86DrvMsg(scrnIndex, X_PROBED, "  Bus %i -> DAC%i\n", i, pNv->i2cMap[i].dac);
		if(pNv->i2cMap[i].sor != -1)
			xf86DrvMsg(scrnIndex, X_PROBED, "  Bus %i -> SOR%i\n", i, pNv->i2cMap[i].sor);
	}

	return TRUE;

	fail:
		xf86DrvMsg(scrnIndex, X_ERROR, "Couldn't find the DDC routing table.  "
			"Mode setting will probably fail!\n");
		return FALSE;
}

static void NV50_I2CPutBits(I2CBusPtr b, int clock, int data)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	const int off = b->DriverPrivate.val * 0x18;

	pNv->REGS[(0x0000E138+off)/4] = 4 | clock | data << 1;
}

static void NV50_I2CGetBits(I2CBusPtr b, int *clock, int *data)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	const int off = b->DriverPrivate.val * 0x18;
	unsigned char val;

	val = pNv->REGS[(0x0000E138+off)/4];
	*clock = !!(val & 1);
	*data = !!(val & 2);
}

static I2CBusPtr
NV50I2CInit(ScrnInfoPtr pScrn, const char *name, const int port)
{
	I2CBusPtr i2c;

	/* Allocate the I2C bus structure */
	i2c = xf86CreateI2CBusRec();
	if(!i2c) return NULL;

	i2c->BusName = strdup(name);
	i2c->scrnIndex = pScrn->scrnIndex;
	i2c->I2CPutBits = NV50_I2CPutBits;
	i2c->I2CGetBits = NV50_I2CGetBits;
	i2c->ByteTimeout = 2200; /* VESA DDC spec 3 p. 43 (+10 %) */
	i2c->StartTimeout = 550;
	i2c->BitTimeout = 40;
	i2c->ByteTimeout = 40;
	i2c->AcknTimeout = 40;
	i2c->DriverPrivate.val = port;

	if(xf86I2CBusInit(i2c)) {
		return i2c;
	} else {
		xfree(i2c);
		return NULL;
	}
}

void
NV50OutputSetPClk(xf86OutputPtr output, int pclk)
{
	NV50OutputPrivPtr nv_output = output->driver_private;

	if (nv_output->set_pclk)
		nv_output->set_pclk(output, pclk);
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

/*
 * Read an EDID from the i2c port.  Perform load detection on the DAC (if
 * present) to see if the display is connected via VGA.  Sets the cached status
 * of both outputs.  The status is marked dirty again in the BlockHandler.
 */
void NV50OutputPartnersDetect(xf86OutputPtr dac, xf86OutputPtr sor, I2CBusPtr i2c)
{
	xf86MonPtr monInfo = ProbeDDC(i2c);
	xf86OutputPtr connected = NULL;
	Bool load = dac && NV50DacLoadDetect(dac);

	if(dac) {
		NV50OutputPrivPtr nv_output = dac->driver_private;

		if(load) {
			nv_output->cached_status = XF86OutputStatusConnected;
			connected = dac;
		} else {
			nv_output->cached_status = XF86OutputStatusDisconnected;
		}
	}

	if(sor) {
		NV50OutputPrivPtr nv_output = sor->driver_private;

		if(monInfo && !load) {
			nv_output->cached_status = XF86OutputStatusConnected;
			connected = sor;
		} else {
			nv_output->cached_status = XF86OutputStatusDisconnected;
		}
	}

	if(connected)
		xf86OutputSetEDID(connected, monInfo);
}

/*
 * Reset the cached output status for all outputs.  Called from NV50BlockHandler.
 */
void
NV50OutputResetCachedStatus(ScrnInfoPtr pScrn)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	for(i = 0; i < xf86_config->num_output; i++) {
		NV50OutputPrivPtr nv_output = xf86_config->output[i]->driver_private;
		nv_output->cached_status = XF86OutputStatusUnknown;
	}
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
	NV50OutputPrivPtr nv_output = output->driver_private;

	if(nv_output->partner)
		((NV50OutputPrivPtr)nv_output->partner->driver_private)->partner = NULL;
	else
		xf86DestroyI2CBusRec(nv_output->i2c, TRUE, TRUE);
	nv_output->i2c = NULL;
}

Bool
NV50CreateOutputs(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	if(!NV50ReadPortMapping(pScrn->scrnIndex, pNv))
		return FALSE;

	/* For each DDC port, create an output for the attached ORs */
	for (i = 0; i < 4; i++) {
		xf86OutputPtr dac = NULL, sor = NULL;
		I2CBusPtr i2c;
		char i2cName[16];

		if(pNv->i2cMap[i].dac == -1 && pNv->i2cMap[i].sor == -1) {
			/* No outputs on this port */
			continue;
		}

		snprintf(i2cName, sizeof(i2cName), "I2C%i", i);
		i2c = NV50I2CInit(pScrn, i2cName, i);
		if (!i2c) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"Failed to initialize I2C for port %i.\n",
				i);
			continue;
		}

		if (pNv->i2cMap[i].dac != -1)
			dac = NV50CreateDac(pScrn, pNv->i2cMap[i].dac);
		if (pNv->i2cMap[i].sor != -1)
			sor = NV50CreateSor(pScrn, pNv->i2cMap[i].sor, OUTPUT_TMDS);

		if (dac) {
			NV50OutputPrivPtr nv_output = dac->driver_private;

			nv_output->partner = sor;
			nv_output->i2c = i2c;
			nv_output->scale = SCALE_PANEL;
		}
		if (sor) {
			NV50OutputPrivPtr nv_output = sor->driver_private;

			nv_output->partner = dac;
			nv_output->i2c = i2c;
			nv_output->scale = SCALE_ASPECT;
		}
	}

	if (pNv->lvds.present) {
		xf86OutputPtr lvds = NV50CreateSor(pScrn, pNv->lvds.or, OUTPUT_LVDS);
		NV50OutputPrivPtr nv_output = lvds->driver_private;

		nv_output->scale = SCALE_ASPECT;
	}

	/* For each output, set the crtc and clone masks */
	for(i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		/* Any output can connect to any head */
		output->possible_crtcs = 0x3;
		output->possible_clones = 0;
	}

	return TRUE;
}

