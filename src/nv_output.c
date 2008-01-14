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

void NVWriteTMDS(NVPtr pNv, int ramdac, uint32_t tmds_reg, uint32_t val)
{
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE);

	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_DATA, val & 0xff);

	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL, tmds_reg & 0xff);
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE);
}

uint8_t NVReadTMDS(NVPtr pNv, int ramdac, uint32_t tmds_reg)
{
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE);

	return (nvReadRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_DATA) & 0xff);
}

/* Two register sets exist, this one is only used for dual link dvi/lvds */

void NVWriteTMDS2(NVPtr pNv, int ramdac, uint32_t tmds_reg, uint32_t val)
{
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL_2, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_2_WRITE_DISABLE);

	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_DATA_2, val & 0xff);

	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL_2, tmds_reg & 0xff);
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL_2, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_2_WRITE_DISABLE);
}

uint8_t NVReadTMDS2(NVPtr pNv, int ramdac, uint32_t tmds_reg)
{
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_CONTROL_2, 
		(tmds_reg & 0xff) | NV_RAMDAC_FP_TMDS_CONTROL_2_WRITE_DISABLE);

	return (nvReadRAMDAC(pNv, ramdac, NV_RAMDAC_FP_TMDS_DATA_2) & 0xff);
}

void NVOutputWriteTMDS(xf86OutputPtr output, uint32_t tmds_reg, uint32_t val)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must write to the "bus" of the output */
	NVWriteTMDS(pNv, nv_output->preferred_output, tmds_reg, val);
}

uint8_t NVOutputReadTMDS(xf86OutputPtr output, uint32_t tmds_reg)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must read from the "bus" of the output */
	return NVReadTMDS(pNv, nv_output->preferred_output, tmds_reg);
}

void NVOutputWriteTMDS2(xf86OutputPtr output, uint32_t tmds_reg, uint32_t val)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must write to the "bus" of the output */
	NVWriteTMDS2(pNv, nv_output->preferred_output, tmds_reg, val);
}

uint8_t NVOutputReadTMDS2(xf86OutputPtr output, uint32_t tmds_reg)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must read from the "bus" of the output */
	return NVReadTMDS2(pNv, nv_output->preferred_output, tmds_reg);
}

/* These functions now write into the output, instead of a specific ramdac */

void NVOutputWriteRAMDAC(xf86OutputPtr output, uint32_t ramdac_reg, uint32_t val)
{
    NVOutputPrivatePtr nv_output = output->driver_private;
    ScrnInfoPtr	pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);

    nvWriteRAMDAC(pNv, nv_output->preferred_output, ramdac_reg, val);
}

uint32_t NVOutputReadRAMDAC(xf86OutputPtr output, uint32_t ramdac_reg)
{
    NVOutputPrivatePtr nv_output = output->driver_private;
    ScrnInfoPtr	pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);

    return nvReadRAMDAC(pNv, nv_output->preferred_output, ramdac_reg);
}

static Bool dpms_common(xf86OutputPtr output, int mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	if (nv_output->last_dpms == mode) /* Don't do unnecesary mode changes. */
		return FALSE;

	nv_output->last_dpms = mode;

	NVPtr pNv = NVPTR(output->scrn);
	xf86CrtcPtr crtc = output->crtc;
	if (!crtc)	/* we need nv_crtc, so give up */
		return TRUE;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	if (pNv->NVArch >= 0x17 && pNv->twoHeads) {
		/* We may be going for modesetting, so we must reset our output binding */
		if (mode == DPMSModeOff) {
			NVWriteVGACR5758(pNv, nv_crtc->head, 0, 0x7f);
			NVWriteVGACR5758(pNv, nv_crtc->head, 2, 0);
		} else {
			NVWriteVGACR5758(pNv, nv_crtc->head, 0, pNv->dcb_table.entry[nv_output->dcb_entry].type);
			NVWriteVGACR5758(pNv, nv_crtc->head, 2, pNv->dcb_table.entry[nv_output->dcb_entry].or);
		}
	}

	return TRUE;
}

static void
nv_lvds_output_dpms(xf86OutputPtr output, int mode)
{
	NVPtr pNv = NVPTR(output->scrn);
	NVOutputPrivatePtr nv_output = output->driver_private;

	ErrorF("nv_lvds_output_dpms is called with mode %d\n", mode);

	if (!dpms_common(output, mode))
		return;

	if (pNv->dcb_table.entry[nv_output->dcb_entry].lvdsconf.use_power_scripts) {
		xf86CrtcPtr crtc = output->crtc;
		if (!crtc)	/* we need nv_crtc, so give up */
			return;
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
		int pclk = nv_calc_tmds_clock_from_pll(output);

		switch (mode) {
		case DPMSModeStandby:
		case DPMSModeSuspend:
			call_lvds_script(output->scrn, nv_crtc->head, nv_output->dcb_entry, LVDS_BACKLIGHT_OFF, pclk);
			break;
		case DPMSModeOff:
			call_lvds_script(output->scrn, nv_crtc->head, nv_output->dcb_entry, LVDS_PANEL_OFF, pclk);
			break;
		case DPMSModeOn:
			call_lvds_script(output->scrn, nv_crtc->head, nv_output->dcb_entry, LVDS_PANEL_ON, pclk);
		default:
			break;
		}
	}
}

static void
nv_analog_output_dpms(xf86OutputPtr output, int mode)
{
	ErrorF("nv_analog_output_dpms is called with mode %d\n", mode);

	dpms_common(output, mode);
}

static void
nv_tmds_output_dpms(xf86OutputPtr output, int mode)
{
	xf86CrtcPtr crtc = output->crtc;

	ErrorF("nv_tmds_output_dpms is called with mode %d\n", mode);

	if (!dpms_common(output, mode))
		return;

	/* Are we assigned a ramdac already?, else we will be activated during mode set */
	if (crtc) {
		NVPtr pNv = NVPTR(output->scrn);
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		ErrorF("nv_tmds_output_dpms is called for CRTC %d with mode %d\n", nv_crtc->head, mode);

		uint32_t fpcontrol = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_CONTROL);
		switch (mode) {
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

static void nv_output_load_state_ext(xf86OutputPtr output, RIVA_HW_STATE *state, Bool override)
{
	NVPtr pNv = NVPTR(output->scrn);

	/* This exists purely for proper text mode restore */
	if (override && pNv->twoHeads) {
		NVOutputPrivatePtr nv_output = output->driver_private;
		NVOutputRegPtr regp = &state->dac_reg[nv_output->output_resource];
		
		NVOutputWriteRAMDAC(output, NV_RAMDAC_OUTPUT, regp->output);
	}
}

/* NOTE: Don't rely on this data for anything other than restoring VT's */

static void
nv_output_save (xf86OutputPtr output)
{
	NVPtr pNv = NVPTR(output->scrn);
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVOutputRegPtr regp;

	ErrorF("nv_output_save is called\n");

	/* Due to strange mapping of outputs we could have swapped analog and digital */
	/* So we force save all the registers */
	regp = &pNv->SavedReg.dac_reg[nv_output->output_resource];

	if (pNv->twoHeads)
		regp->output = NVOutputReadRAMDAC(output, NV_RAMDAC_OUTPUT);

	/* NV11's don't seem to like this, so let's restrict it to digital outputs only. */
	if (nv_output->type == OUTPUT_TMDS || nv_output->type == OUTPUT_LVDS) {
		int i;

		/* Store the registers for helping with VT restore */
		for (i = 0; i < 0xFF; i++)
			regp->TMDS[i] = NVOutputReadTMDS(output, i);

#if 0
		// disabled, as nothing uses the TMDS2 regs currently
		for (i = 0; i < 0xFF; i++)
			regp->TMDS2[i] = NVOutputReadTMDS2(output, i);
#endif
	}
}

uint32_t nv_get_clock_from_crtc(ScrnInfoPtr pScrn, uint8_t crtc)
{
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state = &pNv->SavedReg;
	Bool vpllb_disabled = FALSE;
	uint32_t vplla = crtc ? state->vpll2_a : state->vpll1_a;
	uint32_t vpllb = crtc ? state->vpll2_b : state->vpll1_b;

	if (!pNv->twoStagePLL)
		vpllb_disabled = TRUE;

	/* This is the dummy value nvidia sets when vpll is disabled */
	if ((vpllb & 0xFFFF) == 0x11F)
		vpllb_disabled = TRUE;

	if (!(vpllb & NV31_RAMDAC_ENABLE_VCO2) && pNv->NVArch != 0x30)
		vpllb_disabled = TRUE;

	if (!(vplla & NV30_RAMDAC_ENABLE_VCO2) && pNv->NVArch == 0x30)
		vpllb_disabled = TRUE;

	uint8_t m1, m2, n1, n2, p;

	if (pNv->NVArch == 0x30) {
		m1 = vplla & 0x7;
		n1 = (vplla >> 8) & 0xFF;
		p = (vplla >> 16) & 0x7;
	} else {
		m1 = vplla & 0xFF;
		n1 = (vplla >> 8) & 0xFF;
		p = (vplla >> 16) & 0x7;
	}

	if (vpllb_disabled) {
		m2 = 1;
		n2 = 1;
	} else {
		if (pNv->NVArch == 0x30) {
			m2 = (vplla >> 4) & 0x7;
			n2 = ((vplla >> 19) & 0x7) | (((vplla >> 24) & 0x3) << 3);
		} else {
			m2 = vpllb & 0xFF;
			n2 = (vpllb >> 8) & 0xFF;
		}
	}

	uint32_t clock = ((pNv->CrystalFreqKHz * n1 * n2)/(m1 * m2)) >> p;
	ErrorF("The original bios clock seems to have been %d kHz\n", clock);
	return clock;
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
	/* So or = 3 still means it uses the "ramdac0" regs. */
	regp = &state->dac_reg[nv_output->preferred_output];

	Bool swapped_clock = FALSE;
	/* Bit3 swaps crtc (clocks are bound to crtc) and output */
	if (regp->TMDS[0x4] & (1 << 3)) {
		swapped_clock = TRUE;
	}

	uint8_t vpll_num = swapped_clock ^ nv_output->preferred_output;

	return nv_get_clock_from_crtc(pScrn, vpll_num);
}

void nv_set_tmds_registers(xf86OutputPtr output, uint32_t clock, Bool override, Bool crosswired)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
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
			run_tmds_table(pScrn, nv_output->dcb_entry, nv_crtc->head, clock);
		/* on panels where we do reset after pclk change, DPMS on will do this */
		else if (!pNv->VBIOS.fp.reset_after_pclk_change)
			call_lvds_script(pScrn, nv_crtc->head, nv_output->dcb_entry, LVDS_RESET, clock);
	} else {
		/*
		 * We have no crtc, but we do know what output we are and if we were crosswired.
		 * We can determine our crtc from this.
		 */
		if (nv_output->type == OUTPUT_TMDS)
			run_tmds_table(pScrn, nv_output->dcb_entry, nv_output->preferred_output ^ crosswired, clock);
		else {
			if (!pNv->VBIOS.fp.reset_after_pclk_change)
				call_lvds_script(pScrn, nv_output->preferred_output ^ crosswired, nv_output->dcb_entry, LVDS_RESET, clock);
			call_lvds_script(pScrn, nv_output->preferred_output ^ crosswired, nv_output->dcb_entry, LVDS_PANEL_ON, clock);
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
	ErrorF("nv_output_restore is called\n");

	state = &pNv->SavedReg;

	/* Due to strange mapping of outputs we could have swapped analog and digital */
	/* So we force load all the registers */
	nv_output_load_state_ext(output, state, TRUE);

	nv_output->last_dpms = NV_DPMS_CLEARED;
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
	NVOutputPrivatePtr nv_output = output->driver_private;
	ErrorF("nv_output_mode_fixup is called\n");

	/* For internal panels and gpu scaling on DVI we need the native mode */
	if ((nv_output->type == OUTPUT_LVDS || (nv_output->type == OUTPUT_TMDS && nv_output->scaling_mode != SCALE_PANEL))) {
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

		xf86SetModeCrtc(adjusted_mode, INTERLACE_HALVE_V);
	}

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
	//regp = &state->dac_reg[nv_output->output_resource];

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

		ErrorF("%d: crtc %d output %d twocrt %d twomon %d\n", is_fp, nv_crtc->head, nv_output->output_resource, two_crt, two_mon);
	}
}

/* Only return if output is active (=have a crtc). */

static Bool 
nv_have_duallink(ScrnInfoPtr pScrn)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	NVPtr pNv = NVPTR(pScrn);
	int i;

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		NVOutputPrivatePtr nv_output = output->driver_private;
		if (pNv->dcb_table.entry[nv_output->dcb_entry].duallink_possible && 
			(pNv->dcb_table.entry[nv_output->dcb_entry].type == OUTPUT_LVDS || 
			pNv->dcb_table.entry[nv_output->dcb_entry].type == OUTPUT_TMDS) &&
			output->crtc) {

			return TRUE;
		}
	}

	return FALSE;
}

static void
nv_output_mode_set_routing(xf86OutputPtr output, Bool bios_restore)
{
	xf86CrtcPtr crtc = output->crtc;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	if (pNv->twoHeads) {
		xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
		NVOutputPrivatePtr nv_output = output->driver_private;
		Bool strange_mode = FALSE;
		uint32_t output_reg[2] = {0, 0};
		int i;

		for (i = 0; i < xf86_config->num_output; i++) {
			xf86OutputPtr output2 = xf86_config->output[i];
			NVOutputPrivatePtr nv_output2 = output2->driver_private;
			if (output2->crtc) { /* enabled? */
				uint8_t ors = nv_output2->output_resource;
				if (nv_output2->type == OUTPUT_ANALOG)
					output_reg[ors] = NV_RAMDAC_OUTPUT_DAC_ENABLE;
				if (ors != nv_output2->preferred_output)
					if (pNv->Architecture == NV_ARCH_40)
						strange_mode = TRUE;
			}
		}

		/* Some (most?) pre-NV30 cards have switchable crtc's. */
		if (pNv->switchable_crtc) {
			uint8_t crtc0_index = nv_output->output_resource ^ nv_crtc->head;
			output_reg[~(crtc0_index) & 1] |= NV_RAMDAC_OUTPUT_SELECT_CRTC1;

			if (strange_mode)
				output_reg[crtc0_index] |= NV_RAMDAC_OUTPUT_SELECT_CRTC1;
		}

		ErrorF("output reg: 0x%X 0x%X\n", output_reg[0], output_reg[1]);

		/* The registers can't be considered seperately on most cards */
		nvWriteRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT, output_reg[0]);
		nvWriteRAMDAC(pNv, 1, NV_RAMDAC_OUTPUT, output_reg[1]);
	}

	/* This could use refinement for flatpanels, but it should work this way */
	if (pNv->NVArch < 0x44) {
		nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_TEST_CONTROL, 0xf0000000);
		if (pNv->Architecture == NV_ARCH_40)
			nvWriteRAMDAC(pNv, 0, NV_RAMDAC_670, 0xf0000000);
	} else {
		nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_TEST_CONTROL, 0x00100000);
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

	nv_output_mode_set_routing(output, mode->PrivFlags & NV_MODE_CONSOLE);
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
nv_load_detect(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVPtr pNv = NVPTR(pScrn);
	uint32_t testval, regoffset = 0;
	uint32_t saved1588 = 0, saved1590 = 0, saved_routput, saved_rtest_ctrl, temp;
	int present = 0;

	if (nv_output->pDDCBus != NULL) {
		xf86MonPtr ddc_mon = xf86OutputGetEDID(output, nv_output->pDDCBus);
		/* Is there a digital flatpanel on this channel? */
		if (ddc_mon && ddc_mon->features.input_type) {
			return FALSE;
		}
	}

#define RGB_TEST_DATA(r,g,b) (r << 0 | g << 10 | b << 20)
	testval = RGB_TEST_DATA(0x140, 0x140, 0x140); /* 0x94050140 */
	if (pNv->VBIOS.dactestval)
		testval = pNv->VBIOS.dactestval;

	/* something more clever than this, using output_resource, might be
	 * required, as we might not be on the preferred output */
	switch (pNv->dcb_table.entry[nv_output->dcb_entry].or) {
	case 1:
		regoffset = 0;
		break;
	case 2:
		regoffset = 0x2000;
		break;
	case 4:
		/* this gives rise to RAMDAC_670 and RAMDAC_594 */
		regoffset = 0x68;
		break;
	}

	saved_rtest_ctrl = nvReadRAMDAC0(pNv, NV_RAMDAC_TEST_CONTROL + regoffset);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_TEST_CONTROL + regoffset, saved_rtest_ctrl & ~0x00010000);

	if (pNv->NVArch >= 0x17) {
		saved1588 = nvReadMC(pNv, 0x1588);

		nvWriteMC(pNv, 0x1588, saved1588 & 0xd7ffffff);
		if (regoffset == 0x68) {
			saved1590 = nvReadMC(pNv, 0x1590);
			nvWriteMC(pNv, 0x1590, saved1590 & 0xffffffcf);
		}
	}

	usleep(4000);

	saved_routput = nvReadRAMDAC0(pNv, NV_RAMDAC_OUTPUT + regoffset);
	/* nv driver and nv31 use 0xfffffeee
	 * nv34 and 6600 use 0xfffffece */
	nvWriteRAMDAC0(pNv, NV_RAMDAC_OUTPUT + regoffset, saved_routput & 0xfffffece);
	usleep(1000);

	temp = nvReadRAMDAC0(pNv, NV_RAMDAC_OUTPUT + regoffset);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_OUTPUT + regoffset, temp | 1);

	/* no regoffset on purpose */
	nvWriteRAMDAC0(pNv, NV_RAMDAC_TEST_DATA, 1 << 31 | testval);
	temp = nvReadRAMDAC0(pNv, NV_RAMDAC_TEST_CONTROL);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_TEST_CONTROL, temp | 0x1000);
	usleep(1000);

	present = nvReadRAMDAC0(pNv, NV_RAMDAC_TEST_CONTROL + regoffset) & (1 << 28);

	/* no regoffset on purpose */
	temp = nvReadRAMDAC0(pNv, NV_RAMDAC_TEST_CONTROL);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_TEST_CONTROL, temp & 0xffffefff);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_TEST_DATA, 0);

	/* bios does something more complex for restoring, but I think this is good enough */
	nvWriteRAMDAC0(pNv, NV_RAMDAC_OUTPUT + regoffset, saved_routput);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_TEST_CONTROL + regoffset, saved_rtest_ctrl);
	if (pNv->NVArch >= 0x17) {
		if (regoffset == 0x68)
			nvWriteMC(pNv, 0x1590, saved1590);
		nvWriteMC(pNv, 0x1588, saved1588);
	}

	if (present) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Load detected on output %d\n", nv_output->preferred_output);
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
	NVPtr pNv = NVPTR(output->scrn);

	ErrorF("nv_analog_output_detect is called\n");

	/* assume a CRT connection on non dualhead cards */
	if (!pNv->twoHeads)
		return XF86OutputStatusConnected;

	if (nv_ddc_detect(output))
		return XF86OutputStatusConnected;

	if (pNv->twoHeads && nv_load_detect(output))
		return XF86OutputStatusConnected;

	return XF86OutputStatusDisconnected;
}

static DisplayModePtr
nv_output_get_modes(xf86OutputPtr output, xf86MonPtr mon)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	DisplayModePtr ddc_modes;

	ErrorF("nv_output_get_modes is called\n");

	xf86OutputSetEDID(output, mon);

	ddc_modes = xf86OutputGetEDIDModes(output);

	if (nv_output->type == OUTPUT_TMDS || nv_output->type == OUTPUT_LVDS) {
		int i;
		DisplayModePtr mode;

		for (i = 0; i < 4; i++) {
			/* We only look at detailed timings atm */
			if (mon->det_mon[i].type != DT)
				continue;
			/* Selecting only based on width ok? */
			if (mon->det_mon[i].section.d_timings.h_active > nv_output->fpWidth) {
				nv_output->fpWidth = mon->det_mon[i].section.d_timings.h_active;
				nv_output->fpHeight = mon->det_mon[i].section.d_timings.v_active;
			}
		}

		if (nv_output->native_mode)
			xfree(nv_output->native_mode);
		nv_output->native_mode = NULL;
		/* Disabled for the moment, because it's not essential and caused problems with "newrestore". */
		//if (nv_output->type == OUTPUT_TMDS) {
		//	DisplayModePtr cvtmode;
			/* Add a native resolution mode that is preferred */
			/* Reduced blanking should be fine on DVI monitor */
		//	cvtmode = xf86CVTMode(nv_output->fpWidth, nv_output->fpHeight, 60.0, TRUE, FALSE);
		//	cvtmode->type = M_T_DRIVER | M_T_PREFERRED;

			/* can xf86CVTMode generate invalid modes? */
		//	if (output->funcs->mode_valid(output, cvtmode) == MODE_OK) {
		//		ddc_modes = xf86ModesAdd(ddc_modes, cvtmode);
		//		nv_output->native_mode = xf86DuplicateMode(cvtmode);
		//	} else {
		//		xf86DeleteMode(&cvtmode, cvtmode);
		//	}
		//}

		if (!nv_output->native_mode)
			for (mode = ddc_modes; mode != NULL; mode = mode->next)
				if (mode->HDisplay == nv_output->fpWidth &&
				    mode->VDisplay == nv_output->fpHeight) {
					nv_output->native_mode = xf86DuplicateMode(mode);
					break;
				}
		if (!nv_output->native_mode) {
			ErrorF("Really bad stuff happening, CVT mode bad and no other native mode can be found.\n");
			ErrorF("Bailing out\n");
			return NULL;
		}

		/* We want the new mode to be the only preferred one */
		//for (mode = ddc_modes; mode != NULL; mode = mode->next)
		//	if (mode->type & M_T_PREFERRED && !xf86ModesEqual(mode, nv_output->native_mode))
		//		mode->type &= ~M_T_PREFERRED;
	}

	return ddc_modes;
}

static DisplayModePtr
nv_output_get_ddc_modes(xf86OutputPtr output)
{
	xf86MonPtr ddc_mon;

	ErrorF("nv_output_get_ddc_modes is called\n");

	ddc_mon = nv_get_edid(output);

	if (ddc_mon == NULL)
		return NULL;

	return nv_output_get_modes(output, ddc_mon);
}

static void
nv_output_destroy (xf86OutputPtr output)
{
	ErrorF("nv_output_destroy is called\n");
	NVOutputPrivatePtr nv_output = output->driver_private;

	if (nv_output) {
		if (nv_output->native_mode)
			xfree(nv_output->native_mode);
		xfree(output->driver_private);
	}
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
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	Bool reset_other_crtc = FALSE;
	int i;

	output->funcs->dpms(output, DPMSModeOff);

	/*
	 * Here we detect output resource conflicts.
	 * We do this based on connected monitors, since we need to catch this before something important happens.
	 */

	uint8_t output_resource_mask = 0;
	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output2 = xf86_config->output[i];
		NVOutputPrivatePtr nv_output2 = output2->driver_private;

		/* I don't know how well this will deal with triple connected output situations. */
		if (output2 != output && output2->crtc) { /* output in use? */
			output_resource_mask |= (nv_output2->output_resource + 1); /* +1 to actually get a non-zero value */
		}
	}

	uint8_t or = pNv->dcb_table.entry[nv_output->dcb_entry].or;
	/* Do we have a output resource conflict? */
	if (output_resource_mask & (nv_output->output_resource + 1)) {
		if (or == (1 << (ffs(or) - 1))) { /* we need this output resource */
			for (i = 0; i < xf86_config->num_output; i++) { /* let's find the other */
				xf86OutputPtr output2 = xf86_config->output[i];
				NVOutputPrivatePtr nv_output2 = output2->driver_private;

				if (output2 != output && output2->status == XF86OutputStatusConnected) {
					if (nv_output->output_resource == nv_output2->output_resource) {
						nv_output2->output_resource ^= 1;
						reset_other_crtc = TRUE;
						break; /* We don't deal with triple outputs yet */
					}
				}
			}
		} else { /* we have alternatives */
			nv_output->output_resource ^= 1;
			reset_other_crtc = TRUE;
		}
	}

	if (nv_output->output_resource != nv_output->preferred_output) {
		if (nv_output->type == OUTPUT_TMDS || nv_output->type == OUTPUT_LVDS) {
			/* Observed on a NV31M, some regs are claimed by one output/crtc. */
			if (nv_crtc->head == 1) {
				pNv->fp_regs_owner[0] = 1;
			}
		}
	}

	/* Output resource changes require resetting the other crtc as well. */
	if (reset_other_crtc) {
		uint8_t other_crtc = (~nv_crtc->head) & 1;
		xf86CrtcPtr crtc2 = nv_find_crtc_by_index(pScrn, other_crtc);
		/* Only do this when the other crtc is in it's desired mode, when hotplugging something. */
		if (crtc2 && xf86ModesEqual(&crtc2->mode, &crtc2->desiredMode))
			NVCrtcModeFix(crtc2);
	}
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
    .get_modes = nv_output_get_ddc_modes,
    .destroy = nv_output_destroy,
    .prepare = nv_output_prepare,
    .commit = nv_output_commit,
};

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

		/* LVDS must always use gpu scaling. */
		if (ret == SCALE_PANEL && nv_output->type == OUTPUT_LVDS)
			return FALSE;

		nv_output->scaling_mode = ret;
		return TRUE;
	}

	return TRUE;
}

static int 
nv_tmds_output_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputPrivatePtr nv_output = output->driver_private;

	/* We can't exceed the native mode.*/
	if (pMode->HDisplay > nv_output->fpWidth || pMode->VDisplay > nv_output->fpHeight)
		return MODE_PANEL;

	if (pNv->dcb_table.entry[nv_output->dcb_entry].duallink_possible) {
		if (pMode->Clock > 330000) /* 2x165 MHz */
			return MODE_CLOCK_RANGE;
	} else {
		if (pMode->Clock > 165000) /* 165 MHz */
			return MODE_CLOCK_RANGE;
	}

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
	.get_modes = nv_output_get_ddc_modes,
	.destroy = nv_output_destroy,
	.prepare = nv_output_prepare,
	.commit = nv_output_commit,
	.create_resources = nv_digital_output_create_resources,
	.set_property = nv_digital_output_set_property,
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
	ErrorF("nv_lvds_output_detect is called\n");
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputPrivatePtr nv_output = output->driver_private;

	if (pNv->dcb_table.entry[nv_output->dcb_entry].lvdsconf.use_straps_for_mode &&
	    pNv->VBIOS.fp.native_mode)
		return XF86OutputStatusConnected;
	if (nv_ddc_detect(output))
		return XF86OutputStatusConnected;
	if (pNv->VBIOS.fp.edid)
		return XF86OutputStatusConnected;

	return XF86OutputStatusDisconnected;
}

static DisplayModePtr
nv_lvds_output_get_modes(xf86OutputPtr output)
{
	ErrorF("nv_lvds_output_get_modes is called\n");
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputPrivatePtr nv_output = output->driver_private;
	DisplayModePtr modes;

	if ((modes = nv_output_get_ddc_modes(output)))
		return modes;

	if (!pNv->dcb_table.entry[nv_output->dcb_entry].lvdsconf.use_straps_for_mode ||
	    (pNv->VBIOS.fp.native_mode == NULL)) {
		if (!pNv->VBIOS.fp.edid)
			return NULL;

		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using hardcoded BIOS FP EDID\n");
		xf86MonPtr edid_mon = xf86InterpretEDID(pScrn->scrnIndex, pNv->VBIOS.fp.edid);
		return nv_output_get_modes(output, edid_mon);
	}

	nv_output->fpWidth = pNv->VBIOS.fp.native_mode->HDisplay;
	nv_output->fpHeight = pNv->VBIOS.fp.native_mode->VDisplay;

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
	.create_resources = nv_digital_output_create_resources,
	.set_property = nv_digital_output_set_property,
};

static void nv_add_output(ScrnInfoPtr pScrn, int dcb_entry, const xf86OutputFuncsRec *output_funcs, char *outputname)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86OutputPtr output;
	NVOutputPrivatePtr nv_output;

	int i2c_index = pNv->dcb_table.entry[dcb_entry].i2c_index;
	if (pNv->dcb_table.i2c_read[i2c_index] && pNv->pI2CBus[i2c_index] == NULL)
		NV_I2CInit(pScrn, &pNv->pI2CBus[i2c_index], pNv->dcb_table.i2c_read[i2c_index], xstrdup(outputname));

	if (!(output = xf86OutputCreate(pScrn, output_funcs, outputname)))
		return;

	if (!(nv_output = xnfcalloc(sizeof(NVOutputPrivateRec), 1)))
		return;

	output->driver_private = nv_output;

	nv_output->pDDCBus = pNv->pI2CBus[i2c_index];
	nv_output->dcb_entry = dcb_entry;
	nv_output->type = pNv->dcb_table.entry[dcb_entry].type;
	nv_output->last_dpms = NV_DPMS_CLEARED;

	/* output route:
	 * bit0: OUTPUT_0 valid
	 * bit1: OUTPUT_1 valid
	 * So lowest order has highest priority.
	 * Below is guesswork:
	 * bit2: All outputs valid
	 */
	/* We choose the preferred output resource initially. */
	if (ffs(pNv->dcb_table.entry[dcb_entry].or) & OUTPUT_1) {
		nv_output->preferred_output = 1;
		nv_output->output_resource = 1;
	} else {
		nv_output->preferred_output = 0;
		nv_output->output_resource = 0;
	}

	if (nv_output->type == OUTPUT_LVDS || nv_output->type == OUTPUT_TMDS) {
		if (pNv->fpScaler) /* GPU Scaling */
			nv_output->scaling_mode = SCALE_ASPECT;
		else /* Panel scaling */
			nv_output->scaling_mode = SCALE_PANEL;

		if (xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE)) {
			nv_output->scaling_mode = nv_scaling_mode_lookup(xf86GetOptValString(pNv->Options, OPTION_SCALING_MODE), -1);
			if (nv_output->scaling_mode == SCALE_INVALID)
				nv_output->scaling_mode = SCALE_ASPECT; /* default */
		}
	}

	/* Due to serious problems we have to restrict the crtc's for certain types of outputs. */
	/* This is a result of problems with G70 cards that have a dvi with ffs(or) == 1 */
	/* Anyone know what the solution for this is? */
	/* This does not apply to NV31 LVDS with or == 3. */
	if ((nv_output->type == OUTPUT_LVDS || nv_output->type == OUTPUT_TMDS) && nv_output->preferred_output == 0 && pNv->Architecture == NV_ARCH_40) {
		output->possible_crtcs = (1 << 0);
	} else {
		if (pNv->switchable_crtc)
			output->possible_crtcs = pNv->dcb_table.entry[dcb_entry].heads;
		else
			output->possible_crtcs = (1 << nv_output->preferred_output);
	}

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Added output %s\n", outputname);
}

void NvSetupOutputs(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int i, type, i2c_count[0xf];
	char outputname[20];
	int vga_count = 0, tv_count = 0, dvia_count = 0, dvid_count = 0, lvds_count = 0;

	memset(pNv->pI2CBus, 0, sizeof(pNv->pI2CBus));

	pNv->switchable_crtc = FALSE;
	if (pNv->NVArch > 0x11 && pNv->twoHeads)
		pNv->switchable_crtc = TRUE;

	memset(i2c_count, 0, sizeof(i2c_count));
	for (i = 0 ; i < pNv->dcb_table.entries; i++)
		i2c_count[pNv->dcb_table.entry[i].i2c_index]++;

	/* we setup the outputs up from the BIOS table */
	for (i = 0 ; i < pNv->dcb_table.entries; i++) {
		type = pNv->dcb_table.entry[i].type;

		xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "DCB entry %d: type: %d, i2c_index: %d, heads: %d, bus: %d, or: %d\n", i, type, pNv->dcb_table.entry[i].i2c_index, pNv->dcb_table.entry[i].heads, pNv->dcb_table.entry[i].bus, pNv->dcb_table.entry[i].or);

		switch (type) {
		case OUTPUT_ANALOG:
			if (i2c_count[pNv->dcb_table.entry[i].i2c_index] == 1)
				sprintf(outputname, "VGA-%d", vga_count++);
			else
				sprintf(outputname, "DVI-A-%d", dvia_count++);
			nv_add_output(pScrn, i, &nv_analog_output_funcs, outputname);
			break;
		case OUTPUT_TMDS:
			sprintf(outputname, "DVI-D-%d", dvid_count++);
			nv_add_output(pScrn, i, &nv_tmds_output_funcs, outputname);
			break;
		case OUTPUT_TV:
			sprintf(outputname, "TV-%d", tv_count++);
//			nv_add_output(pScrn, i, &nv_tv_output_funcs, outputname);
			break;
		case OUTPUT_LVDS:
			sprintf(outputname, "LVDS-%d", lvds_count++);
			nv_add_output(pScrn, i, &nv_lvds_output_funcs, outputname);
			break;
		default:
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "DCB type %d not known\n", type);
			break;
		}
	}
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
