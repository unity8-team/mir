/*
 * Copyright 2003 NVIDIA, Corporation
 * Copyright 2006 Dave Airlie
 * Copyright 2007 Maarten Maathuis
 * Copyright 2007-2008 Stuart Bennett
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <X11/Xatom.h>
#include "nv_include.h"

#define MULTIPLE_ENCODERS(e) (e & (e - 1))
#define FOR_EACH_ENCODER_IN_CONNECTOR(i, c, e)	for (i = 0; i < pNv->vbios->dcb->entries; i++)	\
							if (c->possible_encoders & (1 << i) &&	\
							    (e = &pNv->encoders[i]))

static int nv_output_ramdac_offset(struct nouveau_encoder *nv_encoder)
{
	int offset = 0;

	if (nv_encoder->dcb->or & (8 | OUTPUT_C))
		offset += 0x68;
	if (nv_encoder->dcb->or & (8 | OUTPUT_B))
		offset += 0x2000;

	return offset;
}

static bool
nv_load_detect(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder)
{
	NVPtr pNv = NVPTR(pScrn);
	uint32_t testval, regoffset = nv_output_ramdac_offset(nv_encoder);
	uint32_t saved_powerctrl_2 = 0, saved_powerctrl_4 = 0, saved_routput, saved_rtest_ctrl, temp;
	int head, present = 0;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);

#define RGB_TEST_DATA(r,g,b) (r << 0 | g << 10 | b << 20)
	testval = RGB_TEST_DATA(0x140, 0x140, 0x140); /* 0x94050140 */
	if (pNv->vbios->dactestval)
		testval = pNv->vbios->dactestval;

	saved_rtest_ctrl = NVReadRAMDAC(pNv, 0, NV_RAMDAC_TEST_CONTROL + regoffset);
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_TEST_CONTROL + regoffset,
		      saved_rtest_ctrl & ~NV_PRAMDAC_TEST_CONTROL_PWRDWN_DAC_OFF);

	if (pNv->NVArch >= 0x17) {
		saved_powerctrl_2 = nvReadMC(pNv, NV_PBUS_POWERCTRL_2);

		nvWriteMC(pNv, NV_PBUS_POWERCTRL_2, saved_powerctrl_2 & 0xd7ffffff);
		if (regoffset == 0x68) {
			saved_powerctrl_4 = nvReadMC(pNv, NV_PBUS_POWERCTRL_4);
			nvWriteMC(pNv, NV_PBUS_POWERCTRL_4, saved_powerctrl_4 & 0xffffffcf);
		}
	}

	usleep(4000);

	saved_routput = NVReadRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + regoffset);
	head = (saved_routput & 0x100) >> 8;
	/* if there's a spare crtc, using it will minimise flicker for the case
	 * where the in-use crtc is in use by an off-chip tmds encoder */
	if (xf86_config->crtc[head]->enabled && !xf86_config->crtc[head ^ 1]->enabled)
		head ^= 1;
	/* nv driver and nv31 use 0xfffffeee, nv34 and 6600 use 0xfffffece */
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + regoffset,
		      (saved_routput & 0xfffffece) | head << 8);
	usleep(1000);

	temp = NVReadRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + regoffset);
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + regoffset, temp | 1);

	NVWriteRAMDAC(pNv, head, NV_RAMDAC_TEST_DATA,
		      NV_PRAMDAC_TESTPOINT_DATA_NOTBLANK | testval);
	temp = NVReadRAMDAC(pNv, head, NV_RAMDAC_TEST_CONTROL);
	NVWriteRAMDAC(pNv, head, NV_RAMDAC_TEST_CONTROL,
		      temp | NV_PRAMDAC_TEST_CONTROL_TP_INS_EN_ASSERTED);
	usleep(1000);

	present = NVReadRAMDAC(pNv, 0, NV_RAMDAC_TEST_CONTROL + regoffset) &
			NV_PRAMDAC_TEST_CONTROL_SENSEB_ALLHI;

	temp = NVReadRAMDAC(pNv, head, NV_RAMDAC_TEST_CONTROL);
	NVWriteRAMDAC(pNv, head, NV_RAMDAC_TEST_CONTROL,
		      temp & ~NV_PRAMDAC_TEST_CONTROL_TP_INS_EN_ASSERTED);
	NVWriteRAMDAC(pNv, head, NV_RAMDAC_TEST_DATA, 0);

	/* bios does something more complex for restoring, but I think this is good enough */
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + regoffset, saved_routput);
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_TEST_CONTROL + regoffset, saved_rtest_ctrl);
	if (pNv->NVArch >= 0x17) {
		if (regoffset == 0x68)
			nvWriteMC(pNv, NV_PBUS_POWERCTRL_4, saved_powerctrl_4);
		nvWriteMC(pNv, NV_PBUS_POWERCTRL_2, saved_powerctrl_2);
	}

	if (present) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Load detected on output %c\n", '@' + ffs(nv_encoder->dcb->or));
		return true;
	}

	return false;
}

static void
update_output_fields(xf86OutputPtr output, struct nouveau_encoder *det_encoder)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	NVPtr pNv = NVPTR(output->scrn);

	if (nv_connector->detected_encoder == det_encoder)
		return;

	nv_connector->detected_encoder = det_encoder;
	output->possible_crtcs = det_encoder->dcb->heads;
	if (det_encoder->dcb->type == OUTPUT_LVDS || det_encoder->dcb->type == OUTPUT_TMDS) {
		output->doubleScanAllowed = false;
		output->interlaceAllowed = false;
	} else {
		output->doubleScanAllowed = true;
		if (pNv->Architecture == NV_ARCH_20 ||
		   (pNv->Architecture == NV_ARCH_10 &&
		    (pNv->Chipset & 0x0ff0) != CHIPSET_NV10 &&
		    (pNv->Chipset & 0x0ff0) != CHIPSET_NV15))
			/* HW is broken */
			output->interlaceAllowed = false;
		else
			output->interlaceAllowed = true;
	}
}

static bool edid_sink_connected(xf86OutputPtr output)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	NVPtr pNv = NVPTR(output->scrn);
	bool waslocked = NVLockVgaCrtcs(pNv, false);
	bool wastied = nv_heads_tied(pNv);

	if (wastied)
		NVSetOwner(pNv, 0);	/* necessary? */

	nv_connector->edid = xf86OutputGetEDID(output, nv_connector->pDDCBus);

	if (wastied)
		NVSetOwner(pNv, 0x4);
	if (waslocked)
		NVLockVgaCrtcs(pNv, true);

	return !!nv_connector->edid;
}

static xf86OutputStatus
nv_output_detect(xf86OutputPtr output)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_encoder *det_encoder;
	xf86OutputStatus ret = XF86OutputStatusDisconnected;

	struct nouveau_encoder *find_encoder_by_type(enum nouveau_encoder_type type)
	{
		int i;
		for (i = 0; i < pNv->vbios->dcb->entries; i++)
			if (nv_connector->possible_encoders & (1 << i) &&
			    (type == OUTPUT_ANY || pNv->encoders[i].dcb->type == type))
				return &pNv->encoders[i];
		return NULL;
	}

	if (nv_connector->pDDCBus && edid_sink_connected(output)) {
		if (MULTIPLE_ENCODERS(nv_connector->possible_encoders)) {
			if (nv_connector->edid->features.input_type)
				det_encoder = find_encoder_by_type(OUTPUT_TMDS);
			else
				det_encoder = find_encoder_by_type(OUTPUT_ANALOG);
		} else
			det_encoder = find_encoder_by_type(OUTPUT_ANY);
		ret = XF86OutputStatusConnected;
	} else if ((det_encoder = find_encoder_by_type(OUTPUT_ANALOG))) {
		/* we don't have a load det function for early cards */
		if (!pNv->twoHeads || pNv->NVArch == 0x11)
			ret = XF86OutputStatusUnknown;
		else if (pNv->twoHeads && nv_load_detect(pScrn, det_encoder))
			ret = XF86OutputStatusConnected;
	} else if ((det_encoder = find_encoder_by_type(OUTPUT_LVDS))) {
		if (det_encoder->dcb->lvdsconf.use_straps_for_mode) {
			if (pNv->vbios->fp.native_mode)
				ret = XF86OutputStatusConnected;
		} else if (pNv->vbios->fp.ddc_permitted &&
			   nouveau_bios_embedded_edid(pScrn)) {
			nv_connector->edid = xf86InterpretEDID(pScrn->scrnIndex,
							       nouveau_bios_embedded_edid(pScrn));
			ret = XF86OutputStatusConnected;
		}
	}

	if (ret != XF86OutputStatusDisconnected)
		update_output_fields(output, det_encoder);

	return ret;
}

static DisplayModePtr
get_native_mode_from_edid(xf86OutputPtr output, DisplayModePtr edid_modes)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	struct nouveau_encoder *nv_encoder = nv_connector->detected_encoder;
	ScrnInfoPtr pScrn = output->scrn;
	int max_h_active = 0, max_v_active = 0;
	int i;
	DisplayModePtr mode;

	for (i = 0; i < DET_TIMINGS; i++) {
		/* We only look at detailed timings atm */
		if (nv_connector->edid->det_mon[i].type != DT)
			continue;
		/* Selecting only based on width ok? */
		if (nv_connector->edid->det_mon[i].section.d_timings.h_active > max_h_active) {
			max_h_active = nv_connector->edid->det_mon[i].section.d_timings.h_active;
			max_v_active = nv_connector->edid->det_mon[i].section.d_timings.v_active;
		}
	}
	if (!(max_h_active && max_v_active)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No EDID detailed timings available for finding native mode\n");
		return NULL;
	}

	if (nv_encoder->native_mode) {
		xfree(nv_encoder->native_mode);
		nv_encoder->native_mode = NULL;
	}

	for (mode = edid_modes; mode != NULL; mode = mode->next) {
		if (mode->HDisplay == max_h_active &&
			mode->VDisplay == max_v_active) {
			/* Take the preferred mode when it exists. */
			if (mode->type & M_T_PREFERRED) {
				nv_encoder->native_mode = xf86DuplicateMode(mode);
				break;
			}
			/* Find the highest refresh mode otherwise. */
			if (!nv_encoder->native_mode || (mode->VRefresh > nv_encoder->native_mode->VRefresh)) {
				if (nv_encoder->native_mode)
					xfree(nv_encoder->native_mode);
				mode->type |= M_T_PREFERRED;
				nv_encoder->native_mode = xf86DuplicateMode(mode);
			}
		}
	}

	return nv_encoder->native_mode;
}

static DisplayModePtr
nv_output_get_edid_modes(xf86OutputPtr output)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	struct nouveau_encoder *nv_encoder = nv_connector->detected_encoder;
	enum nouveau_encoder_type enctype = nv_encoder->dcb->type;
	DisplayModePtr edid_modes;

	if (enctype == OUTPUT_LVDS ||
	    (enctype == OUTPUT_TMDS && nv_encoder->scaling_mode != SCALE_PANEL))
		/* the digital scaler is not limited to modes given in the EDID,
		 * so enable the GTF bit in order that the xserver thinks
		 * continuous timing is available and adds the standard modes
		 */
		nv_connector->edid->features.msc |= 1;

	xf86OutputSetEDID(output, nv_connector->edid);
	if (!(edid_modes = xf86OutputGetEDIDModes(output)))
		return edid_modes;

	if (enctype == OUTPUT_LVDS || enctype == OUTPUT_TMDS)
		if (!get_native_mode_from_edid(output, edid_modes))
			return NULL;

	return edid_modes;
}

static DisplayModePtr
nv_lvds_output_get_modes(xf86OutputPtr output)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	struct nouveau_encoder *nv_encoder = nv_connector->detected_encoder;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	DisplayModeRec *ret_mode = NULL;

	/* panels only have one mode, and it doesn't change */
	if (nv_encoder->native_mode)
		return xf86DuplicateMode(nv_encoder->native_mode);

	if (nv_encoder->dcb->lvdsconf.use_straps_for_mode) {
		if (!pNv->vbios->fp.native_mode)
			return NULL;

		nv_encoder->native_mode = xf86DuplicateMode(pNv->vbios->fp.native_mode);
		ret_mode = xf86DuplicateMode(pNv->vbios->fp.native_mode);
	} else
		ret_mode = nv_output_get_edid_modes(output);

	if (parse_lvds_manufacturer_table(pScrn,
					  nv_encoder->native_mode->Clock))
		return NULL;

	/* because of the pre-existing native mode exit above, this will only
	 * get run at startup (and before create_resources is called in
	 * mode_fixup), so subsequent user dither settings are not overridden
	 */
	nv_encoder->dithering |= !NVPTR(pScrn)->vbios->fp.if_is_24bit;

	return ret_mode;
}

static int nv_output_mode_valid(xf86OutputPtr output, DisplayModePtr mode)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_connector(output)->detected_encoder;
	NVPtr pNv = NVPTR(output->scrn);

	/* mode_valid can be called by someone doing addmode on an output
	 * which is disconnected and so without an encoder; avoid crashing
	 */
	if (!nv_encoder)
		return MODE_ERROR;

	if (!output->doubleScanAllowed && mode->Flags & V_DBLSCAN)
		return MODE_NO_DBLESCAN;
	if (!output->interlaceAllowed && mode->Flags & V_INTERLACE)
		return MODE_NO_INTERLACE;

	if (nv_encoder->dcb->type == OUTPUT_ANALOG) {
		if (mode->Clock > (pNv->two_reg_pll ? 400000 : 350000))
			return MODE_CLOCK_HIGH;
		if (mode->Clock < 12000)
			return MODE_CLOCK_LOW;
	}
	if (nv_encoder->dcb->type == OUTPUT_LVDS || nv_encoder->dcb->type == OUTPUT_TMDS)
		/* No modes > panel's native res */
		if (mode->HDisplay > nv_encoder->native_mode->HDisplay ||
		    mode->VDisplay > nv_encoder->native_mode->VDisplay)
			return MODE_PANEL;
	if (nv_encoder->dcb->type == OUTPUT_TMDS) {
		if (nv_encoder->dcb->duallink_possible) {
			if (mode->Clock > 330000) /* 2x165 MHz */
				return MODE_CLOCK_HIGH;
		} else
			if (mode->Clock > 165000) /* 165 MHz */
				return MODE_CLOCK_HIGH;
	}

	return MODE_OK;
}

static void
nv_output_destroy(xf86OutputPtr output)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	struct nouveau_encoder *nv_encoder;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(output->scrn);
	int i;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%s called\n", __func__);

	if (!nv_connector)
		return;

	if (nv_connector->edid)
		xfree(nv_connector->edid);
	FOR_EACH_ENCODER_IN_CONNECTOR(i, nv_connector, nv_encoder)
		if (nv_encoder->native_mode)
			xfree(nv_encoder->native_mode);
	xfree(nv_connector);
}

static Atom scaling_mode_atom;
#define SCALING_MODE_NAME "SCALING_MODE"
static const struct {
	char *name;
	enum scaling_modes mode;
} scaling_mode[] = {
	{ "panel", SCALE_PANEL },
	{ "fullscreen", SCALE_FULLSCREEN },
	{ "aspect", SCALE_ASPECT },
	{ "noscale", SCALE_NOSCALE },
	{ NULL, SCALE_INVALID}
};

static Atom dithering_atom;
#define DITHERING_MODE_NAME "DITHERING"

static void
nv_output_create_resources(xf86OutputPtr output)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	ScrnInfoPtr pScrn = output->scrn;
	INT32 dithering_range[2] = { 0, 1 };
	int error, i;

	/* may be called before encoder is picked, resources will be created
	 * by update_output_fields()
	 */
	if (!nv_encoder)
		return;

	/* no properties for vga */
	if (nv_encoder->dcb->type == OUTPUT_ANALOG)
		return;

	/*
	 * Setup scaling mode property.
	 */
	scaling_mode_atom = MakeAtom(SCALING_MODE_NAME, sizeof(SCALING_MODE_NAME) - 1, TRUE);

	error = RRConfigureOutputProperty(output->randr_output,
					scaling_mode_atom, TRUE, FALSE, FALSE,
					0, NULL);

	if (error != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"RRConfigureOutputProperty error, %d\n", error);
	}

	char *existing_scale_name = NULL;
	for (i = 0; scaling_mode[i].name; i++)
		if (scaling_mode[i].mode == nv_encoder->scaling_mode)
			existing_scale_name = scaling_mode[i].name;

	error = RRChangeOutputProperty(output->randr_output, scaling_mode_atom,
					XA_STRING, 8, PropModeReplace, 
					strlen(existing_scale_name),
					existing_scale_name, FALSE, TRUE);

	if (error != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Failed to set scaling mode, %d\n", error);
	}

	/*
	 * Setup dithering property.
	 */
	dithering_atom = MakeAtom(DITHERING_MODE_NAME, sizeof(DITHERING_MODE_NAME) - 1, TRUE);

	error = RRConfigureOutputProperty(output->randr_output,
					dithering_atom, TRUE, TRUE, FALSE,
					2, dithering_range);

	if (error != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"RRConfigureOutputProperty error, %d\n", error);
	}

	/* promote bool into int32 to make RandR DIX and big endian happy */
	int32_t existing_dither = nv_encoder->dithering;
	error = RRChangeOutputProperty(output->randr_output, dithering_atom,
					XA_INTEGER, 32, PropModeReplace, 1,
					&existing_dither, FALSE, TRUE);

	if (error != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Failed to set dithering mode, %d\n", error);
	}

	RRPostPendingProperties(output->randr_output);
}

static Bool
nv_output_set_property(xf86OutputPtr output, Atom property,
				RRPropertyValuePtr value)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);

	if (property == scaling_mode_atom) {
		int32_t ret;
		char *name = NULL;

		if (value->type != XA_STRING || value->format != 8)
			return FALSE;

		name = (char *) value->data;

		/* Match a string to a scaling mode */
		ret = nv_scaling_mode_lookup(name, value->size);
		if (ret == SCALE_INVALID)
			return FALSE;

		/* LVDS must always use gpu scaling. */
		if (ret == SCALE_PANEL && nv_encoder->dcb->type == OUTPUT_LVDS)
			return FALSE;

		nv_encoder->scaling_mode = ret;
	} else if (property == dithering_atom) {
		if (value->type != XA_INTEGER || value->format != 32)
			return FALSE;

		int32_t val = *(int32_t *) value->data;

		if (val < 0 || val > 1)
			return FALSE;

		nv_encoder->dithering = val;
	}

	return TRUE;
}

static Bool
nv_output_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);

	if (nv_connector->nv_encoder != nv_connector->detected_encoder) {
		nv_connector->nv_encoder = nv_connector->detected_encoder;
		if (output->randr_output) {
			RRDeleteOutputProperty(output->randr_output, dithering_atom);
			RRDeleteOutputProperty(output->randr_output, scaling_mode_atom);
			output->funcs->create_resources(output);
		}
	}

	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);

	/* For internal panels and gpu scaling on DVI we need the native mode */
	if (nv_encoder->dcb->type == OUTPUT_LVDS ||
	    (nv_encoder->dcb->type == OUTPUT_TMDS && nv_encoder->scaling_mode != SCALE_PANEL)) {
		adjusted_mode->HDisplay = nv_encoder->native_mode->HDisplay;
		adjusted_mode->HSkew = nv_encoder->native_mode->HSkew;
		adjusted_mode->HSyncStart = nv_encoder->native_mode->HSyncStart;
		adjusted_mode->HSyncEnd = nv_encoder->native_mode->HSyncEnd;
		adjusted_mode->HTotal = nv_encoder->native_mode->HTotal;
		adjusted_mode->VDisplay = nv_encoder->native_mode->VDisplay;
		adjusted_mode->VScan = nv_encoder->native_mode->VScan;
		adjusted_mode->VSyncStart = nv_encoder->native_mode->VSyncStart;
		adjusted_mode->VSyncEnd = nv_encoder->native_mode->VSyncEnd;
		adjusted_mode->VTotal = nv_encoder->native_mode->VTotal;
		adjusted_mode->Clock = nv_encoder->native_mode->Clock;
		adjusted_mode->Flags = nv_encoder->native_mode->Flags;

		xf86SetModeCrtc(adjusted_mode, INTERLACE_HALVE_V);
	}

	return TRUE;
}

static void nv_digital_output_prepare_sel_clk(NVPtr pNv, struct nouveau_encoder *nv_encoder, int head)
{
	NVRegPtr state = &pNv->ModeReg;
	uint32_t bits1618 = nv_encoder->dcb->or & OUTPUT_A ? 0x10000 : 0x40000;

	if (nv_encoder->dcb->location != LOC_ON_CHIP)
		return;

	/* SEL_CLK is only used on the primary ramdac
	 * It toggles spread spectrum PLL output and sets the bindings of PLLs
	 * to heads on digital outputs
	 */
	if (head)
		state->sel_clk |= bits1618;
	else
		state->sel_clk &= ~bits1618;

	/* nv30:
	 *	bit 0		NVClk spread spectrum on/off
	 *	bit 2		MemClk spread spectrum on/off
	 * 	bit 4		PixClk1 spread spectrum on/off toggle
	 * 	bit 6		PixClk2 spread spectrum on/off toggle
	 *
	 * nv40 (observations from bios behaviour and mmio traces):
	 * 	bits 4&6	as for nv30
	 * 	bits 5&7	head dependent as for bits 4&6, but do not appear with 4&6;
	 * 			maybe a different spread mode
	 * 	bits 8&10	seen on dual-link dvi outputs, purpose unknown (set by POST scripts)
	 * 	The logic behind turning spread spectrum on/off in the first place,
	 * 	and which bit-pair to use, is unclear on nv40 (for earlier cards, the fp table
	 * 	entry has the necessary info)
	 */
	if (nv_encoder->dcb->type == OUTPUT_LVDS && pNv->SavedReg.sel_clk & 0xf0) {
		int shift = (pNv->SavedReg.sel_clk & 0x50) ? 0 : 1;

		state->sel_clk &= ~0xf0;
		state->sel_clk |= (head ? 0x40 : 0x10) << shift;
	}
}

static void
nv_output_prepare(xf86OutputPtr output)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	NVPtr pNv = NVPTR(output->scrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(output->crtc);
	NVCrtcRegPtr regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	output->funcs->dpms(output, DPMSModeOff);

	/* calculate some output specific CRTC regs now, so that they can be written in nv_crtc_set_mode */
	if (nv_encoder->dcb->type == OUTPUT_LVDS || nv_encoder->dcb->type == OUTPUT_TMDS)
		nv_digital_output_prepare_sel_clk(pNv, nv_encoder, nv_crtc->head);

	/* Some NV4x have unknown values (0x3f, 0x50, 0x54, 0x6b, 0x79, 0x7f etc.) which we don't alter */
	if (!(regp->CRTC[NV_CIO_CRE_LCD__INDEX] & 0x44)) {
		if (nv_encoder->dcb->type == OUTPUT_LVDS || nv_encoder->dcb->type == OUTPUT_TMDS) {
			regp->CRTC[NV_CIO_CRE_LCD__INDEX] &= ~0x30;
			regp->CRTC[NV_CIO_CRE_LCD__INDEX] |= 0x3;
			if (nv_crtc->head == 0)
				regp->CRTC[NV_CIO_CRE_LCD__INDEX] |= 0x8;
			else
				regp->CRTC[NV_CIO_CRE_LCD__INDEX] &= ~0x8;
			if (nv_encoder->dcb->location != LOC_ON_CHIP)
				regp->CRTC[NV_CIO_CRE_LCD__INDEX] |= (nv_encoder->dcb->or << 4) & 0x30;
		} else
			regp->CRTC[NV_CIO_CRE_LCD__INDEX] = 0;
	}
}

static void
nv_output_mode_set(xf86OutputPtr output, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(output->crtc);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "%s called for encoder %d\n", __func__, nv_encoder->dcb->index);

	if (pNv->twoHeads && nv_encoder->dcb->type == OUTPUT_ANALOG) {
		uint32_t dac_offset = nv_output_ramdac_offset(nv_encoder);
		uint32_t otherdac;
		int i;

		/* bit 16-19 are bits that are set on some G70 cards,
		 * but don't seem to have much effect */
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + dac_offset,
			      nv_crtc->head << 8 | NV_RAMDAC_OUTPUT_DAC_ENABLE);
		/* force any other vga encoders to bind to the other crtc */
		for (i = 0; i < pNv->vbios->dcb->entries; i++)
			if (i != nv_encoder->dcb->index && pNv->encoders[i].dcb &&
			    pNv->encoders[i].dcb->type == OUTPUT_ANALOG) {
				dac_offset = nv_output_ramdac_offset(&pNv->encoders[i]);
				otherdac = NVReadRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + dac_offset);
				NVWriteRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + dac_offset,
					      (otherdac & ~0x100) | (nv_crtc->head ^ 1) << 8);
			}
	}
	if (nv_encoder->dcb->type == OUTPUT_TMDS)
		run_tmds_table(pScrn, nv_encoder->dcb, nv_crtc->head, adjusted_mode->Clock);
	else if (nv_encoder->dcb->type == OUTPUT_LVDS)
		call_lvds_script(pScrn, nv_encoder->dcb, nv_crtc->head, LVDS_RESET, adjusted_mode->Clock);

	/* This could use refinement for flatpanels, but it should work this way */
	if (pNv->NVArch < 0x44)
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_TEST_CONTROL + nv_output_ramdac_offset(nv_encoder), 0xf0000000);
	else
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_TEST_CONTROL + nv_output_ramdac_offset(nv_encoder), 0x00100000);

	/* update fp_control state for any changes made by scripts, for dpms */
	pNv->ModeReg.crtc_reg[nv_crtc->head].fp_control =
			NVReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_CONTROL);
}

static void
nv_output_commit(xf86OutputPtr output)
{
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	ScrnInfoPtr pScrn = output->scrn;
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(output->crtc);

	output->funcs->dpms(output, DPMSModeOn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Output %s is running on CRTC %d using output %c\n", output->name, nv_crtc->head, '@' + ffs(nv_encoder->dcb->or));
}

static void dpms_update_fp_control(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder, xf86CrtcPtr crtc, int mode)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc;
	uint32_t *fpc;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	if (mode == DPMSModeOn) {
		nv_crtc = to_nouveau_crtc(crtc);
		fpc = &pNv->ModeReg.crtc_reg[nv_crtc->head].fp_control;

		nv_crtc->fp_users |= 1 << nv_encoder->dcb->index;
		*fpc &= ~NV_PRAMDAC_FP_TG_CONTROL_OFF;
		NVWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_CONTROL, *fpc);
	} else
		for (i = 0; i < xf86_config->num_crtc; i++) {
			nv_crtc = to_nouveau_crtc(xf86_config->crtc[i]);
			fpc = &pNv->ModeReg.crtc_reg[nv_crtc->head].fp_control;

			nv_crtc->fp_users &= ~(1 << nv_encoder->dcb->index);
			if (!nv_crtc->fp_users) {
				/* cut the FP output */
				*fpc |= NV_PRAMDAC_FP_TG_CONTROL_OFF;
				NVWriteRAMDAC(pNv, nv_crtc->head,
					      NV_RAMDAC_FP_CONTROL, *fpc);
			}
		}
}

static void
lvds_encoder_dpms(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder, xf86CrtcPtr crtc, int mode)
{
	NVPtr pNv = NVPTR(pScrn);

	if (nv_encoder->last_dpms == mode)
		return;
	nv_encoder->last_dpms = mode;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Setting dpms mode %d on lvds encoder (output %d)\n", mode, nv_encoder->dcb->index);

	if (nv_encoder->dcb->lvdsconf.use_power_scripts) {
		/* when removing an output, crtc may not be set, but PANEL_OFF must still be run */
		int head = nv_get_digital_bound_head(pNv, nv_encoder->dcb->or);
		int pclk = nv_encoder->native_mode->Clock;

		if (crtc)
			head = to_nouveau_crtc(crtc)->head;

		if (mode == DPMSModeOn)
			call_lvds_script(pScrn, nv_encoder->dcb, head, LVDS_PANEL_ON, pclk);
		else
			call_lvds_script(pScrn, nv_encoder->dcb, head, LVDS_PANEL_OFF, pclk);
	}

	dpms_update_fp_control(pScrn, nv_encoder, crtc, mode);

	if (mode == DPMSModeOn)
		nv_digital_output_prepare_sel_clk(pNv, nv_encoder, to_nouveau_crtc(crtc)->head);
	else {
		pNv->ModeReg.sel_clk = NVReadRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK);
		pNv->ModeReg.sel_clk &= ~0xf0;
	}
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK, pNv->ModeReg.sel_clk);
}

static void
vga_encoder_dpms(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder, xf86CrtcPtr crtc, int mode)
{
	NVPtr pNv = NVPTR(pScrn);

	if (nv_encoder->last_dpms == mode)
		return;
	nv_encoder->last_dpms = mode;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Setting dpms mode %d on vga encoder (output %d)\n", mode, nv_encoder->dcb->index);

	if (pNv->twoHeads) {
		uint32_t outputval = NVReadRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + nv_output_ramdac_offset(nv_encoder));

		if (mode == DPMSModeOff)
			NVWriteRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + nv_output_ramdac_offset(nv_encoder),
				      outputval & ~NV_RAMDAC_OUTPUT_DAC_ENABLE);
		else if (mode == DPMSModeOn)
			NVWriteRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + nv_output_ramdac_offset(nv_encoder),
				      outputval | NV_RAMDAC_OUTPUT_DAC_ENABLE);
	}
}

static void
tmds_encoder_dpms(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder, xf86CrtcPtr crtc, int mode)
{
	NVPtr pNv = NVPTR(pScrn);

	if (nv_encoder->last_dpms == mode)
		return;
	nv_encoder->last_dpms = mode;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Setting dpms mode %d on tmds encoder (output %d)\n", mode, nv_encoder->dcb->index);

	dpms_update_fp_control(pScrn, nv_encoder, crtc, mode);

	if (nv_encoder->dcb->location != LOC_ON_CHIP) {
		struct nouveau_crtc *nv_crtc;
		int i;

		if (mode == DPMSModeOn) {
			nv_crtc = to_nouveau_crtc(crtc);
			NVWriteVgaCrtc(pNv, nv_crtc->head, NV_CIO_CRE_LCD__INDEX,
				       pNv->ModeReg.crtc_reg[nv_crtc->head].CRTC[NV_CIO_CRE_LCD__INDEX]);
		} else
			for (i = 0; i <= pNv->twoHeads; i++)
				NVWriteVgaCrtc(pNv, i, NV_CIO_CRE_LCD__INDEX,
					       NVReadVgaCrtc(pNv, i, NV_CIO_CRE_LCD__INDEX) & ~((nv_encoder->dcb->or << 4) & 0x30));
	}
}

static void nv_output_dpms(xf86OutputPtr output, int mode)
{
	struct nouveau_connector *nv_connector = to_nouveau_connector(output);
	struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
	ScrnInfoPtr pScrn = output->scrn;
	xf86CrtcPtr crtc = output->crtc;
	NVPtr pNv = NVPTR(pScrn);
	int i;
	void (* const encoder_dpms[4])(ScrnInfoPtr, struct nouveau_encoder *, xf86CrtcPtr, int) =
		/* index matches DCB type */
		{ vga_encoder_dpms, NULL, tmds_encoder_dpms, lvds_encoder_dpms };

	struct nouveau_encoder *nv_encoder_i;
	FOR_EACH_ENCODER_IN_CONNECTOR(i, nv_connector, nv_encoder_i)
		if (nv_encoder_i != nv_encoder)
			encoder_dpms[nv_encoder_i->dcb->type](pScrn, nv_encoder_i, crtc, DPMSModeOff);

	if (nv_encoder) /* may be called before encoder is picked, but iteration above solves it */
		encoder_dpms[nv_encoder->dcb->type](pScrn, nv_encoder, crtc, mode);
}

void nv_encoder_save(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder)
{
	NVPtr pNv = NVPTR(pScrn);

	if (!nv_encoder->dcb)	/* uninitialised encoder */
		return;

	if (pNv->twoHeads && nv_encoder->dcb->type == OUTPUT_ANALOG)
		nv_encoder->restore.output = NVReadRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT + nv_output_ramdac_offset(nv_encoder));
	if (nv_encoder->dcb->type == OUTPUT_TMDS || nv_encoder->dcb->type == OUTPUT_LVDS)
		nv_encoder->restore.head = nv_get_digital_bound_head(pNv, nv_encoder->dcb->or);
}

void nv_encoder_restore(ScrnInfoPtr pScrn, struct nouveau_encoder *nv_encoder)
{
	NVPtr pNv = NVPTR(pScrn);
	int head = nv_encoder->restore.head;

	if (!nv_encoder->dcb)	/* uninitialised encoder */
		return;

	if (pNv->twoHeads && nv_encoder->dcb->type == OUTPUT_ANALOG)
		NVWriteRAMDAC(pNv, 0,
			      NV_RAMDAC_OUTPUT + nv_output_ramdac_offset(nv_encoder),
			      nv_encoder->restore.output);
	if (nv_encoder->dcb->type == OUTPUT_LVDS)
		call_lvds_script(pScrn, nv_encoder->dcb, head, LVDS_PANEL_ON,
				 nv_encoder->native_mode->Clock);
	if (nv_encoder->dcb->type == OUTPUT_TMDS) {
		int clock = nouveau_hw_pllvals_to_clk
					(&pNv->SavedReg.crtc_reg[head].pllvals);

		run_tmds_table(pScrn, nv_encoder->dcb, head, clock);
	}

	nv_encoder->last_dpms = NV_DPMS_CLEARED;
}

static const xf86OutputFuncsRec nv_output_funcs = {
	.dpms = nv_output_dpms,
	.mode_valid = nv_output_mode_valid,
	.mode_fixup = nv_output_mode_fixup,
	.mode_set = nv_output_mode_set,
	.detect = nv_output_detect,
	.get_modes = nv_output_get_edid_modes,
	.destroy = nv_output_destroy,
	.prepare = nv_output_prepare,
	.commit = nv_output_commit,
	.create_resources = nv_output_create_resources,
	.set_property = nv_output_set_property,
};

static const xf86OutputFuncsRec nv_lvds_output_funcs = {
	.dpms = nv_output_dpms,
	.mode_valid = nv_output_mode_valid,
	.mode_fixup = nv_output_mode_fixup,
	.mode_set = nv_output_mode_set,
	.detect = nv_output_detect,
	.get_modes = nv_lvds_output_get_modes,
	.destroy = nv_output_destroy,
	.prepare = nv_output_prepare,
	.commit = nv_output_commit,
	.create_resources = nv_output_create_resources,
	.set_property = nv_output_set_property,
};

static void
nv_add_encoder(ScrnInfoPtr pScrn, struct dcb_entry *dcbent)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_encoder *nv_encoder = &pNv->encoders[dcbent->index];

	nv_encoder->dcb = dcbent;
	nv_encoder->last_dpms = NV_DPMS_CLEARED;
	nv_encoder->dithering = pNv->FPDither;
	if (pNv->fpScaler) /* GPU Scaling */
		nv_encoder->scaling_mode = SCALE_ASPECT;
	else if (nv_encoder->dcb->type == OUTPUT_LVDS)
		nv_encoder->scaling_mode = SCALE_NOSCALE;
	else
		nv_encoder->scaling_mode = SCALE_PANEL;
	if (xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE)) {
		nv_encoder->scaling_mode = nv_scaling_mode_lookup(xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE), -1);
		if (nv_encoder->scaling_mode == SCALE_INVALID)
			nv_encoder->scaling_mode = SCALE_ASPECT; /* default */
	}
}

static void
nv_add_connector(ScrnInfoPtr pScrn, int i2c_index, int encoders, const xf86OutputFuncsRec *output_funcs, char *outputname)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86OutputPtr output;
	struct nouveau_connector *nv_connector;

	if (!(output = xf86OutputCreate(pScrn, output_funcs, outputname)))
		return;
	if (!(nv_connector = xcalloc(1, sizeof (struct nouveau_connector)))) {
		xf86OutputDestroy(output);
		return;
	}

	output->driver_private = nv_connector;

	if (i2c_index < 0xf)
		NV_I2CInit(pScrn, &nv_connector->pDDCBus, &pNv->vbios->dcb->i2c[i2c_index], xstrdup(outputname));
	nv_connector->possible_encoders = encoders;
}

void NvSetupOutputs(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct parsed_dcb *dcb = pNv->vbios->dcb;
	uint16_t connectors[0x10] = { 0 };
	int i, vga_count = 0, dvid_count = 0, dvii_count = 0, lvds_count = 0;

	if (!(pNv->encoders = xcalloc(dcb->entries, sizeof (struct nouveau_encoder))))
		return;

	for (i = 0; i < dcb->entries; i++) {
		struct dcb_entry *dcbent = &dcb->entry[i];

		if (dcbent->type == OUTPUT_TV)
			continue;
		if (dcbent->type > 3) {
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "DCB type %d not known\n", dcbent->type);
			continue;
		}

		connectors[dcbent->i2c_index] |= 1 << i;

		nv_add_encoder(pScrn, dcbent);
	}

	for (i = 0; i < dcb->entries; i++) {
		struct dcb_entry *dcbent = &dcb->entry[i];
		int i2c_index = dcbent->i2c_index;
		uint16_t encoders = connectors[i2c_index];
		char outputname[20];
		xf86OutputFuncsRec const *funcs = &nv_output_funcs;

		if (!encoders)
			continue;

		switch (dcbent->type) {
		case OUTPUT_ANALOG:
			if (!MULTIPLE_ENCODERS(encoders))
				sprintf(outputname, "VGA-%d", vga_count++);
			else
				sprintf(outputname, "DVI-I-%d", dvii_count++);
			break;
		case OUTPUT_TMDS:
			if (!MULTIPLE_ENCODERS(encoders))
				sprintf(outputname, "DVI-D-%d", dvid_count++);
			else
				sprintf(outputname, "DVI-I-%d", dvii_count++);
			break;
		case OUTPUT_LVDS:
			sprintf(outputname, "LVDS-%d", lvds_count++);
			funcs = &nv_lvds_output_funcs;
			/* don't create i2c adapter when lvds ddc not allowed */
			if (dcbent->lvdsconf.use_straps_for_mode ||
			    !pNv->vbios->fp.ddc_permitted)
				i2c_index = 0xf;
			break;
		default:
			continue;
		}

		nv_add_connector(pScrn, i2c_index, encoders, funcs, outputname);
		connectors[i2c_index] = 0; /* avoid connectors being added multiply */
	}
}
