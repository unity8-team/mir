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

#include "nouveau_crtc.h"
#include "nouveau_output.h"
#include "nouveau_connector.h"

int
NV50OrOffset(nouveauOutputPtr output)
{
	return ffs(output->dcb->or) - 1;
}

static void
NV50OutputInit(ScrnInfoPtr pScrn, struct dcb_entry *dcbent)
{
	NVPtr pNv = NVPTR(pScrn);
	nouveauConnectorPtr connector = pNv->connector[dcbent->i2c_index];
	nouveauOutputPtr output = NULL;
	char name[20];
	int i;

	switch (dcbent->type) {
	case OUTPUT_LVDS:
	case OUTPUT_TMDS:
		sprintf(name, "SOR-%d", ffs(dcbent->or) - 1);
		break;
	case OUTPUT_ANALOG:
		sprintf(name, "DAC-%d", ffs(dcbent->or) - 1);
		break;
	default:
		return;
	}

	output = xnfcalloc(sizeof(nouveauOutputRec), 1);
	output->name = xstrdup(name);
	output->dcb = dcbent;
	output->type = dcbent->type;
	output->scrn = pScrn;

	/* Put the output in the connector's list of outputs. */
	for (i = 0; i < MAX_OUTPUTS_PER_CONNECTOR; i++) {
		if (connector->outputs[i])
			continue;
		connector->outputs[i] = output;

		break;
	}

	/* Put ourselves in the main output list. */
	if (!pNv->output) {
		pNv->output = output;
	} else {
		nouveauOutputPtr output_link = pNv->output;
		if (output_link->next) {
			do {
				output_link = output_link->next;
			} while (output_link->next);
		}
		output_link->next = output;
	}

	/* Output property for tmds and lvds. */
	output->dithering = (pNv->FPDither || output->type == OUTPUT_LVDS);

	if (output->type == OUTPUT_LVDS || output->type == OUTPUT_TMDS) {
		if (pNv->fpScaler) /* GPU Scaling */
			output->scale_mode = SCALE_ASPECT;
		else if (output->type == OUTPUT_LVDS)
			output->scale_mode = SCALE_NOSCALE;
		else
			output->scale_mode = SCALE_PANEL;

		if (xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE)) {
			output->scale_mode = nv_scaling_mode_lookup(xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE), -1);
			if (output->scale_mode == SCALE_INVALID)
				output->scale_mode = SCALE_ASPECT; /* default */
		}
	}

	/* NV5x scaling hardware seems to work fine for analog too. */
	if (output->type == OUTPUT_ANALOG) {
		output->scale_mode = SCALE_PANEL;

		if (xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE)) {
			output->scale_mode = nv_scaling_mode_lookup(xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE), -1);
			if (output->scale_mode == SCALE_INVALID)
				output->scale_mode = SCALE_PANEL; /* default */
		}
	}

	/* Usually 3, which means both crtc's. */
	output->allowed_crtc = output->dcb->heads;

	if (output->type == OUTPUT_TMDS) {
		NVWrite(pNv, NV50_SOR0_UNK00C + NV50OrOffset(output) * 0x800, 0x03010700);
		NVWrite(pNv, NV50_SOR0_UNK010 + NV50OrOffset(output) * 0x800, 0x0000152f);
		NVWrite(pNv, NV50_SOR0_UNK014 + NV50OrOffset(output) * 0x800, 0x00000000);
		NVWrite(pNv, NV50_SOR0_UNK018 + NV50OrOffset(output) * 0x800, 0x00245af8);
	}

#if 0
	if (output->type == OUTPUT_LVDS) {
		/* now fpindex should be known, so reread the table here. */
		/* not that we do a lot with the information. */
		parse_lvds_manufacturer_table(pScrn, 0);
	}
#endif

	/* This needs to be handled in the same way as pre-NV5x on the long run. */
	//if (output->type == OUTPUT_LVDS)
	//	pNv->vbios->fp.native_mode = GetLVDSNativeMode(pScrn);

	/* Function pointers. */
	if (output->type == OUTPUT_TMDS || output->type == OUTPUT_LVDS) {
		NV50SorSetFunctionPointers(output);
	} else if (output->type == OUTPUT_ANALOG || output->type == OUTPUT_TV) {
		NV50DacSetFunctionPointers(output);
	}
}

#define MULTIPLE_ENCODERS(e) ((e) & ((e) - 1))
void
NV50OutputSetup(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct parsed_dcb *dcb = pNv->vbios->dcb;
	int lvds = 0, dvi_i = 0, dvi_d = 0, vga = 0;
	uint16_t connector[16] = {};
	char name[20];
	int i;

	for (i = 0; i < dcb->entries; i++) {
		struct dcb_entry *dcbent = &dcb->entry[i];

		switch (dcbent->type) {
		case OUTPUT_ANALOG:
		case OUTPUT_TMDS:
		case OUTPUT_LVDS:
			break;
		default:
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
				   "DCB encoder type %d not known\n",
				   dcbent->type);
			continue;
		}

		connector[dcbent->i2c_index] |= (1 << i);
		NV50OutputInit(pScrn, dcbent);
	}

	for (i = 0; i < dcb->entries; i++) {
		struct dcb_entry *dcbent = &dcb->entry[i];
		nouveauConnectorPtr c = pNv->connector[dcbent->i2c_index];
		uint16_t encoders;
		
		encoders = connector[dcbent->i2c_index];
		connector[dcbent->i2c_index] = 0;

		if (!encoders)
			continue;

		switch (dcbent->type) {
		case OUTPUT_ANALOG:
			if (!MULTIPLE_ENCODERS(encoders))
				sprintf(name, "VGA-%d", vga++);
			else
				sprintf(name, "DVI-I-%d", dvi_i++);
			break;
		case OUTPUT_TMDS:
			if (!MULTIPLE_ENCODERS(encoders))
				sprintf(name, "DVI-D-%d", dvi_d++);
			else
				sprintf(name, "DVI-I-%d", dvi_i++);
			break;
		case OUTPUT_LVDS:
			sprintf(name, "LVDS-%d", lvds++);
			break;
		default:
			continue;
		}

		if (dcbent->i2c_index != 0xf) {
			NV_I2CInit(pScrn, &pNv->pI2CBus[dcbent->i2c_index],
				   &dcb->i2c[dcbent->i2c_index],
				   xstrdup(name));
		}

		xfree(c->name);
		c->name = xstrdup(name);
		c->i2c_index = dcbent->i2c_index;
		c->pDDCBus = pNv->pI2CBus[dcbent->i2c_index];
	}
}

void
NV50OutputDestroy(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	nouveauOutputPtr output, next;

	for (output = pNv->output; output != NULL; output = next) {
		next = output->next;
		xfree(output->name);
		xfree(output);
	}

	pNv->output = NULL;
}
