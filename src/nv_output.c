/*
 * Copyright 2006 Dave Airlie
 * Copyright 2007 Maarten Maathuis
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
/*
 * this code uses ideas taken from the NVIDIA nv driver - the nvidia license
 * decleration is at the bottom of this file as it is rather ugly 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "os.h"
#include "mibank.h"
#include "globals.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86DDC.h"
#include "mipointer.h"
#include "windowstr.h"
#include <randrstr.h>
#include <X11/extensions/render.h>
#include "X11/Xatom.h"

#include "xf86Crtc.h"
#include "nv_include.h"

const char *OutputType[] = {
    "None",
    "VGA",
    "DVI",
    "LVDS",
    "S-video",
    "Composite",
};

const char *MonTypeName[7] = {
    "AUTO",
    "NONE",
    "CRT",
    "LVDS",
    "TMDS",
    "CTV",
    "STV"
};

/* 
 * TMDS registers are indirect 8 bit registers.
 * Reading is straightforward, writing a bit odd.
 * Reading: Write adress (+write protect bit, do not forget this), then read value.
 * Writing: Write adress (+write protect bit), write value, write adress again and write it again (+write protect bit).
 */

void NVWriteTMDS(NVPtr pNv, int ramdac, CARD32 tmds_reg, CARD32 val)
{
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE);

	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_DATA, val & 0xff);

	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL, tmds_reg & 0xff);
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE);
}

CARD8 NVReadTMDS(NVPtr pNv, int ramdac, CARD32 tmds_reg)
{
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE);

	return (nvReadRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_DATA) & 0xff);
}

/* Two register sets exist, this one is only used for dual link dvi/lvds */

void NVWriteTMDS2(NVPtr pNv, int ramdac, CARD32 tmds_reg, CARD32 val)
{
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL_2, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_2_WRITE_DISABLE);

	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_DATA_2, val & 0xff);

	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL_2, tmds_reg & 0xff);
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL_2, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_2_WRITE_DISABLE);
}

CARD8 NVReadTMDS2(NVPtr pNv, int ramdac, CARD32 tmds_reg)
{
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL_2, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_2_WRITE_DISABLE);

	return (nvReadRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_DATA_2) & 0xff);
}

void NVOutputWriteTMDS(xf86OutputPtr output, CARD32 tmds_reg, CARD32 val)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must write to the "bus" of the output */
	NVWriteTMDS(pNv, nv_output->preferred_output, tmds_reg, val);
}

CARD8 NVOutputReadTMDS(xf86OutputPtr output, CARD32 tmds_reg)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must read from the "bus" of the output */
	return NVReadTMDS(pNv, nv_output->preferred_output, tmds_reg);
}

void NVOutputWriteTMDS2(xf86OutputPtr output, CARD32 tmds_reg, CARD32 val)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must write to the "bus" of the output */
	NVWriteTMDS2(pNv, nv_output->preferred_output, tmds_reg, val);
}

CARD8 NVOutputReadTMDS2(xf86OutputPtr output, CARD32 tmds_reg)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must read from the "bus" of the output */
	return NVReadTMDS2(pNv, nv_output->preferred_output, tmds_reg);
}

/* These functions now write into the output, instead of a specific ramdac */

void NVOutputWriteRAMDAC(xf86OutputPtr output, CARD32 ramdac_reg, CARD32 val)
{
    NVOutputPrivatePtr nv_output = output->driver_private;
    ScrnInfoPtr	pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);

    nvWriteRAMDAC(pNv, nv_output->preferred_output, ramdac_reg, val);
}

CARD32 NVOutputReadRAMDAC(xf86OutputPtr output, CARD32 ramdac_reg)
{
    NVOutputPrivatePtr nv_output = output->driver_private;
    ScrnInfoPtr	pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);

    return nvReadRAMDAC(pNv, nv_output->preferred_output, ramdac_reg);
}

static void dpms_update_output_ramdac(xf86OutputPtr output, int mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcPtr crtc = output->crtc;
	if (!crtc)	/* we need nv_crtc, so give up */
		return;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	/* We may be going for modesetting, so we must reset our output binding */
	if (mode == DPMSModeOff) {
		NVWriteVGACR5758(pNv, nv_crtc->head, 0, 0x7f);
		NVWriteVGACR5758(pNv, nv_crtc->head, 2, 0);
		return;
	}

	/* The previous call was not a modeset, but a normal dpms call */
	NVWriteVGACR5758(pNv, nv_crtc->head, 0, pNv->dcb_table.entry[nv_output->dcb_entry].type);
	NVWriteVGACR5758(pNv, nv_crtc->head, 2, pNv->dcb_table.entry[nv_output->dcb_entry].or);
}

static void
nv_lvds_output_dpms(xf86OutputPtr output, int mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVPtr pNv = NVPTR(output->scrn);
	xf86CrtcPtr crtc = output->crtc;
	if (!crtc)	/* we need nv_crtc, so give up */
		return;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	ErrorF("nv_lvds_output_dpms is called with mode %d\n", mode);

	dpms_update_output_ramdac(output, mode);

	if (!pNv->dcb_table.entry[nv_output->dcb_entry].lvdsconf.use_power_scripts)
		return;

	switch (mode) {
	case DPMSModeStandby:
	case DPMSModeSuspend:
		call_lvds_script(output->scrn, nv_crtc->head, nv_output->dcb_entry, LVDS_BACKLIGHT_OFF, 0);
		break;
	case DPMSModeOff:
		call_lvds_script(output->scrn, nv_crtc->head, nv_output->dcb_entry, LVDS_PANEL_OFF, 0);
		break;
	case DPMSModeOn:
		call_lvds_script(output->scrn, nv_crtc->head, nv_output->dcb_entry, LVDS_PANEL_ON, 0);
	default:
		break;
	}
}

static void
nv_analog_output_dpms(xf86OutputPtr output, int mode)
{
	xf86CrtcPtr crtc = output->crtc;

	ErrorF("nv_analog_output_dpms is called with mode %d\n", mode);

	dpms_update_output_ramdac(output, mode);

	if (crtc) {
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		ErrorF("nv_analog_output_dpms is called for CRTC %d with mode %d\n", nv_crtc->crtc, mode);
	}
}

static void
nv_tmds_output_dpms(xf86OutputPtr output, int mode)
{
	xf86CrtcPtr crtc = output->crtc;
	NVPtr pNv = NVPTR(output->scrn);

	ErrorF("nv_tmds_output_dpms is called with mode %d\n", mode);

	dpms_update_output_ramdac(output, mode);

	/* Are we assigned a ramdac already?, else we will be activated during mode set */
	if (crtc) {
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		ErrorF("nv_tmds_output_dpms is called for CRTC %d with mode %d\n", nv_crtc->crtc, mode);

		CARD32 fpcontrol = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_CONTROL);
		switch(mode) {
			case DPMSModeStandby:
			case DPMSModeSuspend:
			case DPMSModeOff:
				/* cut the TMDS output */	    
				fpcontrol |= 0x20000022;
				break;
			case DPMSModeOn:
				/* disable cutting the TMDS output */
				fpcontrol &= ~0x20000022;
				break;
		}
		nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_CONTROL, fpcontrol);
	}
}

void nv_output_save_state_ext(xf86OutputPtr output, RIVA_HW_STATE *state)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVOutputRegPtr regp;
	int i;

	regp = &state->dac_reg[nv_output->preferred_output];

	regp->output = NVOutputReadRAMDAC(output, NV_RAMDAC_OUTPUT);

	/* Store the registers in case we need them again for something (like data for VT restore) */
	for (i = 0; i < 0xFF; i++) {
		regp->TMDS[i] = NVOutputReadTMDS(output, i);
	}

	for (i = 0; i < 0xFF; i++) {
		regp->TMDS2[i] = NVOutputReadTMDS2(output, i);
	}
}

void nv_output_load_state_ext(xf86OutputPtr output, RIVA_HW_STATE *state, Bool override)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVOutputRegPtr regp;

	regp = &state->dac_reg[nv_output->preferred_output];

	/* This exists purely for proper text mode restore */
	if (override) NVOutputWriteRAMDAC(output, NV_RAMDAC_OUTPUT, regp->output);
}

/* NOTE: Don't rely on this data for anything other than restoring VT's */

static void
nv_output_save (xf86OutputPtr output)
{
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state;

	ErrorF("nv_output_save is called\n");
	state = &pNv->SavedReg;

	/* Due to strange mapping of outputs we could have swapped analog and digital */
	/* So we force save all the registers */
	nv_output_save_state_ext(output, state);
}

uint32_t nv_calc_tmds_clock_from_pll(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state;
	NVOutputRegPtr regp;
	NVOutputPrivatePtr nv_output = output->driver_private;

	state = &pNv->SavedReg;
	/* Registers are stored by their preferred ramdac */
	regp = &state->dac_reg[nv_output->preferred_output];

	/* Only do it once for a dvi-d/dvi-a pair */
	Bool swapped_clock = FALSE;
	Bool vpllb_disabled = FALSE;
	/* Bit3 swaps crtc (clocks are bound to crtc) and output */
	if (regp->TMDS[0x4] & (1 << 3)) {
		swapped_clock = TRUE;
	}

	uint8_t vpll_num = swapped_clock ^ nv_output->preferred_output;

	uint32_t vplla = 0;
	uint32_t vpllb = 0;

	/* For the moment the syntax is the same for NV40 and earlier */
	if (pNv->Architecture == NV_ARCH_40) {
		vplla = vpll_num ? state->vpll2_a : state->vpll1_a;
		vpllb = vpll_num ? state->vpll2_b : state->vpll1_b;
	} else {
		vplla = vpll_num ? state->vpll2 : state->vpll;
		if (pNv->twoStagePLL)
			vpllb = vpll_num ? state->vpll2B : state->vpllB;
	}

	if (!pNv->twoStagePLL)
		vpllb_disabled = TRUE;

	/* This is the dummy value nvidia sets when vpll is disabled */
	if ((vpllb & 0xFFFF) == 0x11F)
		vpllb_disabled = TRUE;

	uint8_t m1, m2, n1, n2, p;

	m1 = vplla & 0xFF;
	n1 = (vplla >> 8) & 0xFF;
	p = (vplla >> 16) & 0x7;

	if (vpllb_disabled) {
		m2 = 1;
		n2 = 1;
	} else {
		m2 = vpllb & 0xFF;
		n2 = (vpllb >> 8) & 0xFF;
	}

	uint32_t clock = ((pNv->CrystalFreqKHz * n1 * n2)/(m1 * m2)) >> p;
	ErrorF("The original bios clock seems to have been %d kHz\n", clock);
	return clock;
}

void nv_set_tmds_registers(xf86OutputPtr output, uint32_t clock, Bool override, Bool crosswired)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVOutputPrivatePtr nv_output = output->driver_private;
	xf86CrtcPtr crtc = output->crtc;
	/* We have no crtc, so what are we supposed to do now? */
	/* This can only happen during VT restore */
	if (crtc && !override) {
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
		/*
		 * Resetting all registers is a bad idea, it seems to work fine without it.
		 */
		if (nv_output->type == OUTPUT_TMDS)
			run_tmds_table(pScrn, nv_output->dcb_entry, nv_crtc->head, clock/10);
		else
			call_lvds_script(pScrn, nv_crtc->head, nv_output->dcb_entry, LVDS_RESET, clock / 10);
	} else {
		/*
		 * We have no crtc, but we do know what output we are and if we were crosswired.
		 * We can determine our crtc from this.
		 */
		if (nv_output->type == OUTPUT_TMDS)
			run_tmds_table(pScrn, nv_output->dcb_entry, nv_output->preferred_output ^ crosswired, clock/10);
		else {
			call_lvds_script(pScrn, nv_output->preferred_output ^ crosswired, nv_output->dcb_entry, LVDS_RESET, clock / 10);
			call_lvds_script(pScrn, nv_output->preferred_output ^ crosswired, nv_output->dcb_entry, LVDS_PANEL_ON, 0);
		}
	}
}

static void
nv_output_restore (xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state;
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVOutputRegPtr regp;
	ErrorF("nv_output_restore is called\n");

	state = &pNv->SavedReg;
	regp = &state->dac_reg[nv_output->preferred_output];

	/* Due to strange mapping of outputs we could have swapped analog and digital */
	/* So we force load all the registers */
	nv_output_load_state_ext(output, state, TRUE);
}

static int
nv_output_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
	if (pMode->Flags & V_DBLSCAN)
		return MODE_NO_DBLESCAN;

	if (pMode->Clock > 400000 || pMode->Clock < 25000)
		return MODE_CLOCK_RANGE;

	return MODE_OK;
}


static Bool
nv_output_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode)
{
	ErrorF("nv_output_mode_fixup is called\n");

	return TRUE;
}

static void
nv_output_mode_set_regs(xf86OutputPtr output, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	//RIVA_HW_STATE *state;
	//NVOutputRegPtr regp, savep;
	Bool is_fp = FALSE;
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	/* It's getting quiet here, not removing function just yet, we may still need it */

	//state = &pNv->ModeReg;
	//regp = &state->dac_reg[nv_output->preferred_output];

	if (nv_output->type == OUTPUT_TMDS || nv_output->type == OUTPUT_LVDS)
		is_fp = TRUE;

	if (output->crtc) {
		NVCrtcPrivatePtr nv_crtc = output->crtc->driver_private;
		int two_crt = FALSE;
		int two_mon = FALSE;

		for (i = 0; i < config->num_output; i++) {
			NVOutputPrivatePtr nv_output2 = config->output[i]->driver_private;

			/* is it this output ?? */
			if (config->output[i] == output)
				continue;

			/* it the output connected */
			if (config->output[i]->crtc == NULL)
				continue;

			two_mon = TRUE;
			if ((nv_output2->type == OUTPUT_ANALOG) && (nv_output->type == OUTPUT_ANALOG)) {
				two_crt = TRUE;
			}
		}

		ErrorF("%d: crtc %d output %d twocrt %d twomon %d\n", is_fp, nv_crtc->crtc, nv_output->preferred_output, two_crt, two_mon);
	}
}

static Bool 
nv_have_duallink(ScrnInfoPtr	pScrn)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	NVPtr pNv = NVPTR(pScrn);
	int i;

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		NVOutputPrivatePtr nv_output = output->driver_private;
		if (pNv->dcb_table.entry[nv_output->dcb_entry].duallink_possible)
			return TRUE;
	}

	return FALSE;
}

static void
nv_output_mode_set_routing(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	xf86CrtcPtr crtc = output->crtc;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	Bool is_fp = FALSE;

	uint32_t output_reg[2] = {0, 0};

	if ((nv_output->type == OUTPUT_LVDS) || (nv_output->type == OUTPUT_TMDS)) {
		is_fp = TRUE;
	}

	if (pNv->Architecture == NV_ARCH_40) {
		/* NV4x cards have strange ways of dealing with dualhead */
		/* Also see reg594 in nv_crtc.c */
		output_reg[0] = NV_RAMDAC_OUTPUT_DAC_ENABLE;
		/* This seems to be restricted to dual link outputs. */
		/* Some cards have secondary outputs with ffs(or) != 3. */
		if (nv_have_duallink(pScrn) || pNv->restricted_mode)
			output_reg[1] = NV_RAMDAC_OUTPUT_DAC_ENABLE;
	} else {
		/* This is for simplicity */
		output_reg[0] = NV_RAMDAC_OUTPUT_DAC_ENABLE;
		output_reg[1] = NV_RAMDAC_OUTPUT_DAC_ENABLE;
	}

	/* Some pre-NV30 cards have switchable crtc's. */
	if (pNv->switchable_crtc) {
		if (pNv->restricted_mode) { /* some NV4A for example */
			if (nv_output->preferred_output != nv_crtc->head) {
				output_reg[0] |= NV_RAMDAC_OUTPUT_SELECT_CRTC1;
			} else {
				output_reg[1] |= NV_RAMDAC_OUTPUT_SELECT_CRTC1;
			}
		} else {
			output_reg[1] |= NV_RAMDAC_OUTPUT_SELECT_CRTC1;
			/* Does this have something to do with outputs that have ffs(or) == 1? */
			/* I suspect this bit represents more than just a crtc switch. */
			if (nv_crtc->head != nv_output->preferred_output) {
				output_reg[0] |= NV_RAMDAC_OUTPUT_SELECT_CRTC1;
			}
		}
	}

	/* The registers can't be considered seperately on most cards */
	nvWriteRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT, output_reg[0]);
	nvWriteRAMDAC(pNv, 1, NV_RAMDAC_OUTPUT, output_reg[1]);

	/* This could use refinement for flatpanels, but it should work this way */
	if (pNv->NVArch < 0x44) {
		nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_TEST_CONTROL, 0xf0000000);
		if (pNv->Architecture == NV_ARCH_40)
			nvWriteRAMDAC(pNv, 0, NV_RAMDAC_670, 0xf0000000);
	} else {
		nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_TEST_CONTROL, 0x00100000);
		nvWriteRAMDAC(pNv, 0, NV_RAMDAC_670, 0x00100000);
	}
}

static void
nv_output_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		   DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputPrivatePtr nv_output = output->driver_private;
	RIVA_HW_STATE *state;

	ErrorF("nv_output_mode_set is called\n");

	state = &pNv->ModeReg;

	nv_output_mode_set_regs(output, mode, adjusted_mode);
	nv_output_load_state_ext(output, state, FALSE);
	if (nv_output->type == OUTPUT_TMDS || nv_output->type == OUTPUT_LVDS)
		nv_set_tmds_registers(output, adjusted_mode->Clock, FALSE, FALSE);

	nv_output_mode_set_routing(output);
}

static xf86MonPtr
nv_get_edid(xf86OutputPtr output)
{
	/* no use for shared DDC output */
	NVOutputPrivatePtr nv_output = output->driver_private;
	xf86MonPtr ddc_mon;

	if (nv_output->pDDCBus == NULL)
		return NULL;

	ddc_mon = xf86OutputGetEDID(output, nv_output->pDDCBus);
	if (!ddc_mon)
		return NULL;

	if (ddc_mon->features.input_type && (nv_output->type == OUTPUT_ANALOG))
		goto invalid;

	if ((!ddc_mon->features.input_type) && (nv_output->type == OUTPUT_TMDS ||
				nv_output->type == OUTPUT_LVDS))
		goto invalid;

	return ddc_mon;

invalid:
	xfree(ddc_mon);
	return NULL;
}

static Bool
nv_ddc_detect(xf86OutputPtr output)
{
	xf86MonPtr m = nv_get_edid(output);

	if (m == NULL)
		return FALSE;

	xfree(m);
	return TRUE;
}

static Bool
nv_crt_load_detect(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVPtr pNv = NVPTR(pScrn);
	CARD32 reg_output, reg_test_ctrl, temp;
	Bool present = FALSE;

	/* For some reason we get false positives on output 1, maybe due tv-out? */
	if (nv_output->preferred_output == 1) {
		return FALSE;
	}

	if (nv_output->pDDCBus != NULL) {
		xf86MonPtr ddc_mon = xf86OutputGetEDID(output, nv_output->pDDCBus);
		/* Is there a digital flatpanel on this channel? */
		if (ddc_mon && ddc_mon->features.input_type) {
			return FALSE;
		}
	}

	reg_output = nvReadRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_OUTPUT);
	reg_test_ctrl = nvReadRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_TEST_CONTROL);

	nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_TEST_CONTROL, (reg_test_ctrl & ~0x00010000));

	nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_OUTPUT, (reg_output & 0x0000FEEE));
	usleep(1000);

	temp = nvReadRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_OUTPUT);
	nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_OUTPUT, temp | 1);

	nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_TEST_DATA, 0x94050140);
	temp = nvReadRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_TEST_CONTROL);
	nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_TEST_CONTROL, temp | 0x1000);

	usleep(1000);

	present = (nvReadRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_TEST_CONTROL) & (1 << 28)) ? TRUE : FALSE;

	temp = NVOutputReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL);
	nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_TEST_CONTROL, temp & 0x000EFFF);

	nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_OUTPUT, reg_output);
	nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_TEST_CONTROL, reg_test_ctrl);

	if (present) {
		ErrorF("A crt was detected on output %d with no ddc support\n", nv_output->preferred_output);
		return TRUE;
	}

	return FALSE;
}

static xf86OutputStatus
nv_tmds_output_detect(xf86OutputPtr output)
{
	ErrorF("nv_tmds_output_detect is called\n");

	if (nv_ddc_detect(output))
		return XF86OutputStatusConnected;

	return XF86OutputStatusDisconnected;
}


static xf86OutputStatus
nv_analog_output_detect(xf86OutputPtr output)
{
	ErrorF("nv_analog_output_detect is called\n");

	if (nv_ddc_detect(output))
		return XF86OutputStatusConnected;

	if (nv_crt_load_detect(output))
		return XF86OutputStatusConnected;

	return XF86OutputStatusDisconnected;
}

static DisplayModePtr
nv_output_get_modes(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	xf86MonPtr ddc_mon;
	DisplayModePtr ddc_modes;

	ErrorF("nv_output_get_modes is called\n");

	ddc_mon = nv_get_edid(output);

	xf86OutputSetEDID(output, ddc_mon);

	if (ddc_mon == NULL)
		return NULL;

	ddc_modes = xf86OutputGetEDIDModes (output);

	if (nv_output->type == OUTPUT_TMDS || nv_output->type == OUTPUT_LVDS) {
		int i;
		DisplayModePtr mode;

		for (i = 0; i < 4; i++) {
			/* We only look at detailed timings atm */
			if (ddc_mon->det_mon[i].type != DT)
				continue;
			/* Selecting only based on width ok? */
			if (ddc_mon->det_mon[i].section.d_timings.h_active > nv_output->fpWidth) {
				nv_output->fpWidth = ddc_mon->det_mon[i].section.d_timings.h_active;
				nv_output->fpHeight = ddc_mon->det_mon[i].section.d_timings.v_active;
			}
		}

		/* Add a native resolution mode that is preferred */
		/* Reduced blanking should be fine on DVI monitor */
		nv_output->native_mode = xf86CVTMode(nv_output->fpWidth, nv_output->fpHeight, 60.0, TRUE, FALSE);
		nv_output->native_mode->type = M_T_DRIVER | M_T_PREFERRED;

		if (output->funcs->mode_valid(output, nv_output->native_mode) == MODE_OK) {
			/* We want the new mode to be preferred */
			for (mode = ddc_modes; mode != NULL; mode = mode->next) {
				if (mode->type & M_T_PREFERRED) {
					mode->type &= ~M_T_PREFERRED;
				}
			}
			ddc_modes = xf86ModesAdd(ddc_modes, nv_output->native_mode);
		} else { /* invalid mode */
			nv_output->native_mode = NULL;
			for (mode = ddc_modes; mode != NULL; mode = mode->next) {
				if (mode->HDisplay == nv_output->fpWidth &&
					mode->VDisplay == nv_output->fpHeight) {

					nv_output->native_mode = mode;
					break;
				}
			}
			if (!nv_output->native_mode) {
				ErrorF("Really bad stuff happening, CVT mode bad and no other native mode can be found.\n");
				ErrorF("Bailing out\n");
				return NULL;
			} else {
				ErrorF("CVT mode was invalid(=bad), but another mode was found\n");
			}
		}
	}

	return ddc_modes;
}

static void
nv_output_destroy (xf86OutputPtr output)
{
	ErrorF("nv_output_destroy is called\n");
	if (output->driver_private)
		xfree (output->driver_private);
}

static void
nv_output_prepare(xf86OutputPtr output)
{
	ErrorF("nv_output_prepare is called\n");
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	xf86CrtcPtr crtc = output->crtc;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	output->funcs->dpms(output, DPMSModeOff);

	/* Shut down the tmds pll, a short sleep period will happen at crtc prepare. */
	uint32_t debug0 = nvReadRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_FP_DEBUG_0) | 
						NV_RAMDAC_FP_DEBUG_0_PWRDOWN_TMDS_PLL;
	nvWriteRAMDAC(pNv, nv_output->preferred_output, NV_RAMDAC_FP_DEBUG_0, debug0);

	/* Set our output type and output routing possibilities to the right registers */
	NVWriteVGACR5758(pNv, nv_crtc->head, 0, pNv->dcb_table.entry[nv_output->dcb_entry].type);
	NVWriteVGACR5758(pNv, nv_crtc->head, 2, pNv->dcb_table.entry[nv_output->dcb_entry].or);
}

static void
nv_output_commit(xf86OutputPtr output)
{
	ErrorF("nv_output_commit is called\n");

	output->funcs->dpms(output, DPMSModeOn);
}

static const xf86OutputFuncsRec nv_analog_output_funcs = {
    .dpms = nv_analog_output_dpms,
    .save = nv_output_save,
    .restore = nv_output_restore,
    .mode_valid = nv_output_mode_valid,
    .mode_fixup = nv_output_mode_fixup,
    .mode_set = nv_output_mode_set,
    .detect = nv_analog_output_detect,
    .get_modes = nv_output_get_modes,
    .destroy = nv_output_destroy,
    .prepare = nv_output_prepare,
    .commit = nv_output_commit,
};

#ifdef RANDR_12_INTERFACE
/*
 * Several scaling modes exist, let the user choose.
 */
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
static Atom scaling_mode_atom;

static int
nv_scaling_mode_lookup(char *name, int size)
{
	int i;

	/* for when name is zero terminated */
	if (size < 0)
		size = strlen(name);

	for (i = 0; scaling_mode[i].name; i++)
		/* We're getting non-terminated strings */
		if (strlen(scaling_mode[i].name) >= size &&
				!strncasecmp(name, scaling_mode[i].name, size))
			break;

	return scaling_mode[i].mode;
}

static void
nv_digital_output_create_resources(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	int error, i;

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
		if (scaling_mode[i].mode == nv_output->scaling_mode)
			existing_scale_name = scaling_mode[i].name;

	error = RRChangeOutputProperty(output->randr_output, scaling_mode_atom,
					XA_STRING, 8, PropModeReplace, 
					strlen(existing_scale_name),
					existing_scale_name, FALSE, TRUE);

	if (error != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Failed to set scaling mode, %d\n", error);
	}
}

static Bool
nv_digital_output_set_property(xf86OutputPtr output, Atom property,
				RRPropertyValuePtr value)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

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

		nv_output->scaling_mode = ret;
		return TRUE;
	}

	return TRUE;
}

#endif /* RANDR_12_INTERFACE */

static int 
nv_tmds_output_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	/* We can't exceed the native mode.*/
	if (pMode->HDisplay > nv_output->fpWidth || pMode->VDisplay > nv_output->fpHeight)
		return MODE_PANEL;

	return nv_output_mode_valid(output, pMode);
}

static const xf86OutputFuncsRec nv_tmds_output_funcs = {
	.dpms = nv_tmds_output_dpms,
	.save = nv_output_save,
	.restore = nv_output_restore,
	.mode_valid = nv_tmds_output_mode_valid,
	.mode_fixup = nv_output_mode_fixup,
	.mode_set = nv_output_mode_set,
	.detect = nv_tmds_output_detect,
	.get_modes = nv_output_get_modes,
	.destroy = nv_output_destroy,
	.prepare = nv_output_prepare,
	.commit = nv_output_commit,
#ifdef RANDR_12_INTERFACE
	.create_resources = nv_digital_output_create_resources,
	.set_property = nv_digital_output_set_property,
#endif /* RANDR_12_INTERFACE */
};

static int nv_lvds_output_mode_valid
(xf86OutputPtr output, DisplayModePtr pMode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	/* No modes > panel's native res */
	if (pMode->HDisplay > nv_output->fpWidth || pMode->VDisplay > nv_output->fpHeight)
		return MODE_PANEL;

	return nv_output_mode_valid(output, pMode);
}

static xf86OutputStatus
nv_lvds_output_detect(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputPrivatePtr nv_output = output->driver_private;

	if (pNv->dcb_table.entry[nv_output->dcb_entry].lvdsconf.use_straps_for_mode &&
	    pNv->VBIOS.fp.native_mode)
		return XF86OutputStatusConnected;
	if (nv_ddc_detect(output))
		return XF86OutputStatusConnected;

	return XF86OutputStatusDisconnected;
}

static DisplayModePtr
nv_lvds_output_get_modes(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputPrivatePtr nv_output = output->driver_private;
	DisplayModePtr modes;

	if ((modes = nv_output_get_modes(output)))
		return modes;

	/* it is possible to set up a mode from what we can read from the
	 * RAMDAC registers, but if we can't read the BIOS table correctly
	 * we might as well give up */
	if (!pNv->dcb_table.entry[nv_output->dcb_entry].lvdsconf.use_straps_for_mode ||
	    (pNv->VBIOS.fp.native_mode == NULL))
		return NULL;

	nv_output->fpWidth = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_HDISP_END) + 1;
	nv_output->fpHeight = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_VDISP_END) + 1;
	nv_output->fpSyncs = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_CONTROL) & 0x30000033;

	if (pNv->VBIOS.fp.native_mode->HDisplay != nv_output->fpWidth ||
		pNv->VBIOS.fp.native_mode->VDisplay != nv_output->fpHeight) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"Panel size mismatch; ignoring RAMDAC\n");
		nv_output->fpWidth = pNv->VBIOS.fp.native_mode->HDisplay;
		nv_output->fpHeight = pNv->VBIOS.fp.native_mode->VDisplay;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Panel size is %u x %u\n",
		nv_output->fpWidth, nv_output->fpHeight);

	nv_output->native_mode = xf86DuplicateMode(pNv->VBIOS.fp.native_mode);

	return xf86DuplicateMode(pNv->VBIOS.fp.native_mode);
}

static const xf86OutputFuncsRec nv_lvds_output_funcs = {
	.dpms = nv_lvds_output_dpms,
	.save = nv_output_save,
	.restore = nv_output_restore,
	.mode_valid = nv_lvds_output_mode_valid,
	.mode_fixup = nv_output_mode_fixup,
	.mode_set = nv_output_mode_set,
	.detect = nv_lvds_output_detect,
	.get_modes = nv_lvds_output_get_modes,
	.destroy = nv_output_destroy,
	.prepare = nv_output_prepare,
	.commit = nv_output_commit,
#ifdef RANDR_12_INTERFACE
	.create_resources = nv_digital_output_create_resources,
	.set_property = nv_digital_output_set_property,
#endif /* RANDR_12_INTERFACE */
};

static void nv_add_analog_output(ScrnInfoPtr pScrn, int dcb_entry, Bool dvi_pair)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86OutputPtr	    output;
	NVOutputPrivatePtr    nv_output;
	char outputname[20];
	Bool create_output = TRUE;
	int i2c_index = pNv->dcb_table.entry[dcb_entry].i2c_index;

	/* DVI have an analog connector and a digital one, differentiate between that and a normal vga */
	if (dvi_pair) {
		sprintf(outputname, "DVI-A-%d", pNv->dvi_a_count);
		pNv->dvi_a_count++;
	} else {
		sprintf(outputname, "VGA-%d", pNv->vga_count);
		pNv->vga_count++;
	}

	nv_output = xnfcalloc (sizeof (NVOutputPrivateRec), 1);
	if (!nv_output) {
		return;
	}

	nv_output->dcb_entry = dcb_entry;

	if (pNv->dcb_table.i2c_read[i2c_index] && pNv->pI2CBus[i2c_index] == NULL)
		NV_I2CInit(pScrn, &pNv->pI2CBus[i2c_index], pNv->dcb_table.i2c_read[i2c_index], xstrdup(outputname));

	nv_output->type = OUTPUT_ANALOG;

	/* output route:
	 * bit0: OUTPUT_0 valid
	 * bit1: OUTPUT_1 valid
	 * So lowest order has highest priority.
	 * Below is guesswork:
	 * bit2: All outputs valid
	 */
	/* This also facilitates proper output routing for dvi */
	/* See sel_clk assignment in nv_crtc.c */
	if (ffs(pNv->dcb_table.entry[dcb_entry].or) & OUTPUT_1) {
		nv_output->preferred_output = 1;
	} else {
		nv_output->preferred_output = 0;
	}

	nv_output->bus = pNv->dcb_table.entry[dcb_entry].bus;

	if (!create_output) {
		xfree(nv_output);
		return;
	}

	/* Delay creation of output until we actually know we want it */
	output = xf86OutputCreate (pScrn, &nv_analog_output_funcs, outputname);
	if (!output)
		return;

	output->driver_private = nv_output;

	nv_output->pDDCBus = pNv->pI2CBus[i2c_index];

	output->possible_crtcs = pNv->dcb_table.entry[dcb_entry].heads;

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Adding output %s\n", outputname);
}

static void nv_add_digital_output(ScrnInfoPtr pScrn, int dcb_entry, int lvds)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86OutputPtr	    output;
	NVOutputPrivatePtr    nv_output;
	char outputname[20];
	Bool create_output = TRUE;
	int i2c_index = pNv->dcb_table.entry[dcb_entry].i2c_index;

	if (lvds) {
		sprintf(outputname, "LVDS-%d", pNv->lvds_count);
		pNv->lvds_count++;
	} else {
		sprintf(outputname, "DVI-D-%d", pNv->dvi_d_count);
		pNv->dvi_d_count++;
	}

	nv_output = xnfcalloc (sizeof (NVOutputPrivateRec), 1);

	if (!nv_output) {
		return;
	}

	nv_output->dcb_entry = dcb_entry;

	if (pNv->dcb_table.i2c_read[i2c_index] && pNv->pI2CBus[i2c_index] == NULL)
		NV_I2CInit(pScrn, &pNv->pI2CBus[i2c_index], pNv->dcb_table.i2c_read[i2c_index], xstrdup(outputname));

	nv_output->pDDCBus = pNv->pI2CBus[i2c_index];

	/* output route:
	 * bit0: OUTPUT_0 valid
	 * bit1: OUTPUT_1 valid
	 * So lowest order has highest priority.
	 * Below is guesswork:
	 * bit2: All outputs valid
	 */
	/* This also facilitates proper output routing for dvi */
	/* See sel_clk assignment in nv_crtc.c */
	if (ffs(pNv->dcb_table.entry[dcb_entry].or) & OUTPUT_1) {
		nv_output->preferred_output = 1;
	} else {
		nv_output->preferred_output = 0;
	}

	nv_output->bus = pNv->dcb_table.entry[dcb_entry].bus;

	if (lvds) {
		nv_output->type = OUTPUT_LVDS;
		/* comment below two lines to test LVDS under RandR12.
		 * If your screen "blooms" or "bleeds" (i.e. has a developing
		 * white / psychedelic pattern) then KILL X IMMEDIATELY
		 * (ctrl+alt+backspace) & if the effect continues reset power */
		ErrorF("Output refused because we don't accept LVDS at the moment.\n");
		create_output = FALSE;
	} else {
		nv_output->type = OUTPUT_TMDS;
	}

	if (!create_output) {
		xfree(nv_output);
		return;
	}

	/* Delay creation of output until we are certain is desirable */
	if (lvds)
		output = xf86OutputCreate (pScrn, &nv_lvds_output_funcs, outputname);
	else
		output = xf86OutputCreate (pScrn, &nv_tmds_output_funcs, outputname);
	if (!output)
		return;

	output->driver_private = nv_output;

	if (pNv->fpScaler) /* GPU Scaling */
		nv_output->scaling_mode = SCALE_ASPECT;
	else /* Panel scaling */
		nv_output->scaling_mode = SCALE_PANEL;

#ifdef RANDR_12_INTERFACE
	if (xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE)) {
		nv_output->scaling_mode = nv_scaling_mode_lookup(xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE), -1);
		if (nv_output->scaling_mode == SCALE_INVALID)
			nv_output->scaling_mode = SCALE_ASPECT; /* default */
	}
#endif /* RANDR_12_INTERFACE */

	/* Due to serious problems we have to restrict the crtc's for certain types of outputs. */
	/* This is a result of problems with G70 cards that have a dvi with ffs(or) == 1 */
	/* Anyone know what the solution for this is? */
	if (nv_output->preferred_output == 0) {
		output->possible_crtcs = (1 << 0);
	} else {
		output->possible_crtcs = pNv->dcb_table.entry[dcb_entry].heads;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Adding output %s\n", outputname);
}

void NvDCBSetupOutputs(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int i, type, i2c_count[0xf];

	pNv->switchable_crtc = FALSE;

	memset(i2c_count, 0, sizeof(i2c_count));
	for (i = 0 ; i < pNv->dcb_table.entries; i++)
		i2c_count[pNv->dcb_table.entry[i].i2c_index]++;

	/* we setup the outputs up from the BIOS table */
	for (i = 0 ; i < pNv->dcb_table.entries; i++) {
		type = pNv->dcb_table.entry[i].type;
		if (type > 3) {
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "DCB type %d not known\n", type);
			continue;
		}

		xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "DCB entry %d: type: %d, i2c_index: %d, heads: %d, bus: %d, or: %d\n", i, type, pNv->dcb_table.entry[i].i2c_index, pNv->dcb_table.entry[i].heads, pNv->dcb_table.entry[i].bus, pNv->dcb_table.entry[i].or);

		switch(type) {
		case OUTPUT_ANALOG:
			if (pNv->dcb_table.entry[i].heads == 0x3) /* analog is the best criteria */
				pNv->switchable_crtc = TRUE;
			nv_add_analog_output(pScrn, i, (i2c_count[pNv->dcb_table.entry[i].i2c_index] > 1));
			break;
		case OUTPUT_TMDS:
			nv_add_digital_output(pScrn, i, 0);
			break;
		case OUTPUT_LVDS:
			nv_add_digital_output(pScrn, i, 1);
			break;
		default:
			break;
		}
	}
}

void NvSetupOutputs(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	pNv->Television = FALSE;

	memset(pNv->pI2CBus, 0, sizeof(pNv->pI2CBus));
	NvDCBSetupOutputs(pScrn);
}

/*************************************************************************** \
|*                                                                           *|
|*       Copyright 1993-2003 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user documenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 1993-1999 NVIDIA, Corporation.  All rights reserved.      *|
|*                                                                           *|
|*     NVIDIA, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     NVIDIA, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  48 C.F.R. 2.101 (OCT 1995),     *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/
