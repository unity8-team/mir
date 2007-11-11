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

#ifdef ENABLE_RANDR12

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

void NVOutputWriteTMDS(xf86OutputPtr output, CARD32 tmds_reg, CARD32 val)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	int ramdac;

	/* Is TMDS programmed on a different output? */
	/* Always choose the prefered ramdac, since that one contains the tmds stuff */
	/* Assumption: there is always once output that can only run of the primary ramdac */
	if (nv_output->valid_ramdac & RAMDAC_1) {
		ramdac = 1;
	} else {
		ramdac = 0;
	}

	NVWriteTMDS(pNv, ramdac, tmds_reg, val);
}

CARD8 NVOutputReadTMDS(xf86OutputPtr output, CARD32 tmds_reg)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	int ramdac;

	/* Is TMDS programmed on a different output? */
	/* Always choose the prefered ramdac, since that one contains the tmds stuff */
	/* Assumption: there is always once output that can only run of the primary ramdac */
	if (nv_output->valid_ramdac & RAMDAC_1) {
		ramdac = 1;
	} else {
		ramdac = 0;
	}

	return NVReadTMDS(pNv, ramdac, tmds_reg);
}

void NVOutputWriteRAMDAC(xf86OutputPtr output, CARD32 ramdac_reg, CARD32 val)
{
    NVOutputPrivatePtr nv_output = output->driver_private;
    ScrnInfoPtr	pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);

    nvWriteRAMDAC(pNv, nv_output->ramdac, ramdac_reg, val);
}

CARD32 NVOutputReadRAMDAC(xf86OutputPtr output, CARD32 ramdac_reg)
{
    NVOutputPrivatePtr nv_output = output->driver_private;
    ScrnInfoPtr	pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);

    return nvReadRAMDAC(pNv, nv_output->ramdac, ramdac_reg);
}

static void nv_output_backlight_enable(xf86OutputPtr output,  Bool on)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	ErrorF("nv_output_backlight_enable is called for output %s to turn %s\n", output->name, on ? "on" : "off");

	/* This is done differently on each laptop.  Here we
	 * define the ones we know for sure. */

#if defined(__powerpc__)
	if ((pNv->Chipset == 0x10DE0179) ||
	    (pNv->Chipset == 0x10DE0189) ||
	    (pNv->Chipset == 0x10DE0329)) {
		/* NV17,18,34 Apple iMac, iBook, PowerBook */
		CARD32 tmp_pmc, tmp_pcrt;
		tmp_pmc = nvReadMC(pNv, 0x10F0) & 0x7FFFFFFF;
		tmp_pcrt = nvReadCRTC0(pNv, NV_CRTC_081C) & 0xFFFFFFFC;
		if (on) {
			tmp_pmc |= (1 << 31);
			tmp_pcrt |= 0x1;
		}
		nvWriteMC(pNv, 0x10F0, tmp_pmc);
		nvWriteCRTC0(pNv, NV_CRTC_081C, tmp_pcrt);
	}
#endif

	if(pNv->twoHeads && ((pNv->Chipset & 0x0ff0) != CHIPSET_NV11))
		nvWriteMC(pNv, 0x130C, on ? 3 : 7);
}

static void
nv_lvds_output_dpms(xf86OutputPtr output, int mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVPtr pNv = NVPTR(output->scrn);

	if (nv_output->ramdac != -1 && mode != DPMSModeOff) {
		/* This was not a modeset, but a normal dpms call */
		pNv->ramdac_active[nv_output->ramdac] = TRUE;
		ErrorF("Activating ramdac %d\n", nv_output->ramdac);
		nv_output->ramdac_assigned = TRUE;
	}

	switch (mode) {
	case DPMSModeStandby:
	case DPMSModeSuspend:
	case DPMSModeOff:
		nv_output_backlight_enable(output, 0);
		break;
	case DPMSModeOn:
		nv_output_backlight_enable(output, 1);
	default:
		break;
	}

	/* We may be going for modesetting, so we must reset the ramdacs */
	if (nv_output->ramdac != -1 && mode == DPMSModeOff) {
		pNv->ramdac_active[nv_output->ramdac] = FALSE;
		ErrorF("Deactivating ramdac %d\n", nv_output->ramdac);
		nv_output->ramdac_assigned = FALSE;
	}
}

static void
nv_analog_output_dpms(xf86OutputPtr output, int mode)
{
	xf86CrtcPtr crtc = output->crtc;
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVPtr pNv = NVPTR(output->scrn);

	ErrorF("nv_analog_output_dpms is called with mode %d\n", mode);

	if (nv_output->ramdac != -1) {
		/* We may be going for modesetting, so we must reset the ramdacs */
		if (mode == DPMSModeOff) {
			pNv->ramdac_active[nv_output->ramdac] = FALSE;
			ErrorF("Deactivating ramdac %d\n", nv_output->ramdac);
			nv_output->ramdac_assigned = FALSE;
			/* This was not a modeset, but a normal dpms call */
		} else {
			pNv->ramdac_active[nv_output->ramdac] = TRUE;
			ErrorF("Activating ramdac %d\n", nv_output->ramdac);
			nv_output->ramdac_assigned = TRUE;
		}
	}

	if (crtc) {
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		ErrorF("nv_analog_output_dpms is called for CRTC %d with mode %d\n", nv_crtc->crtc, mode);
	}
}

static void
nv_tmds_output_dpms(xf86OutputPtr output, int mode)
{
	xf86CrtcPtr crtc = output->crtc;
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVPtr pNv = NVPTR(output->scrn);

	ErrorF("nv_tmds_output_dpms is called with mode %d\n", mode);

	/* We just woke up again from an actual monitor dpms and not a modeset prepare */
	/* Put here since we actually need our ramdac to wake up again ;-) */
	if (nv_output->ramdac != -1 && mode != DPMSModeOff) {
		pNv->ramdac_active[nv_output->ramdac] = TRUE;
		nv_output->ramdac_assigned = TRUE;
		ErrorF("Activating ramdac %d\n", nv_output->ramdac);
	}

	/* Are we assigned a ramdac already?, else we will be activated during mode set */
	if (crtc && nv_output->ramdac != -1) {
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		ErrorF("nv_tmds_output_dpms is called for CRTC %d with mode %d\n", nv_crtc->crtc, mode);

		CARD32 fpcontrol = nvReadRAMDAC(pNv, nv_output->ramdac, NV_RAMDAC_FP_CONTROL);
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
		nvWriteRAMDAC(pNv, nv_output->ramdac, NV_RAMDAC_FP_CONTROL, fpcontrol);
	}

	/* We may be going for modesetting, so we must reset the ramdacs */
	if (nv_output->ramdac != -1 && mode == DPMSModeOff) {
		pNv->ramdac_active[nv_output->ramdac] = FALSE;
		nv_output->ramdac_assigned = FALSE;
		ErrorF("Deactivating ramdac %d\n", nv_output->ramdac);
	}
}

/* Some registers are not set, because they are zero. */
/* This sequence matters, this is how the blob does it */
int tmds_regs[] = { 0x2f, 0x2e, 0x33, 0x04, 0x05, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x00, 0x01, 0x02, 0x2e, 0x2f, 0x04, 0x3a, 0x33, 0x04 };

void nv_output_save_state_ext(xf86OutputPtr output, RIVA_HW_STATE *state, Bool override)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputRegPtr regp;
	int i;

	regp = &state->dac_reg[nv_output->ramdac];
	regp->general       = NVOutputReadRAMDAC(output, NV_RAMDAC_GENERAL_CONTROL);
	regp->test_control = NVOutputReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL);
	regp->fp_control    = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_CONTROL);
	regp->debug_0	= NVOutputReadRAMDAC(output, NV_RAMDAC_FP_DEBUG_0);
	regp->debug_1	= NVOutputReadRAMDAC(output, NV_RAMDAC_FP_DEBUG_1);
	regp->debug_2	= NVOutputReadRAMDAC(output, NV_RAMDAC_FP_DEBUG_2);
	state->config       = nvReadFB(pNv, NV_PFB_CFG0);

	regp->unk_a20 = NVOutputReadRAMDAC(output, NV_RAMDAC_A20);
	regp->unk_a24 = NVOutputReadRAMDAC(output, NV_RAMDAC_A24);
	regp->unk_a34 = NVOutputReadRAMDAC(output, NV_RAMDAC_A34);

	regp->output = NVOutputReadRAMDAC(output, NV_RAMDAC_OUTPUT);

	if ((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
		regp->dither = NVOutputReadRAMDAC(output, NV_RAMDAC_DITHER_NV11);
	} else if (pNv->twoHeads) {
		regp->dither = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_DITHER);
	}
	regp->nv10_cursync = NVOutputReadRAMDAC(output, NV_RAMDAC_NV10_CURSYNC);

	/* I want to be able reset TMDS registers for DVI-D/DVI-A pairs for example */
	/* Also write on VT restore */
	if (nv_output->type != OUTPUT_LVDS || override )
		for (i = 0; i < sizeof(tmds_regs)/sizeof(tmds_regs[0]); i++) {
			regp->TMDS[tmds_regs[i]] = NVOutputReadTMDS(output, tmds_regs[i]);
		}

	/* The regs below are 0 for non-flatpanels, so you can load and save them */

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_HDISP_END + (i * 4);
		regp->fp_horiz_regs[i] = NVOutputReadRAMDAC(output, ramdac_reg);
	}

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_VDISP_END + (i * 4);
		regp->fp_vert_regs[i] = NVOutputReadRAMDAC(output, ramdac_reg);
	}

	regp->fp_hvalid_start = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_HVALID_START);
	regp->fp_hvalid_end = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_HVALID_END);
	regp->fp_vvalid_start = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_VVALID_START);
	regp->fp_vvalid_end = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_VVALID_END);
}

void nv_output_load_state_ext(xf86OutputPtr output, RIVA_HW_STATE *state, Bool override)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputRegPtr regp;
	int i;

	regp = &state->dac_reg[nv_output->ramdac];

	if (nv_output->type == OUTPUT_LVDS) {
		ErrorF("Writing %08X to RAMDAC_FP_DEBUG_0\n", regp->debug_0);
		ErrorF("Writing %08X to RAMDAC_FP_DEBUG_1\n", regp->debug_1);
		ErrorF("Writing %08X to RAMDAC_FP_DEBUG_2\n", regp->debug_2);
		ErrorF("Writing %08X to RAMDAC_OUTPUT\n", regp->output);
		ErrorF("Writing %08X to RAMDAC_FP_CONTROL\n", regp->fp_control);
	}
	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_DEBUG_0, regp->debug_0);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_DEBUG_1, regp->debug_1);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_DEBUG_2, regp->debug_2);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_OUTPUT, regp->output);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_CONTROL, regp->fp_control);

	NVOutputWriteRAMDAC(output, NV_RAMDAC_A20, regp->unk_a20);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_A24, regp->unk_a24);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_A34, regp->unk_a34);

	if ((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
		NVOutputWriteRAMDAC(output, NV_RAMDAC_DITHER_NV11, regp->dither);
	} else if (pNv->twoHeads) {
		NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_DITHER, regp->dither);
	}

	NVOutputWriteRAMDAC(output, NV_RAMDAC_GENERAL_CONTROL, regp->general);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_TEST_CONTROL, regp->test_control);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_NV10_CURSYNC, regp->nv10_cursync);

	/* I want to be able reset TMDS registers for DVI-D/DVI-A pairs for example */
	/* Also write on VT restore */
	if (nv_output->type != OUTPUT_LVDS || override )
		for (i = 0; i < sizeof(tmds_regs)/sizeof(tmds_regs[0]); i++) {
			NVOutputWriteTMDS(output, tmds_regs[i], regp->TMDS[tmds_regs[i]]);
		}

	/* The regs below are 0 for non-flatpanels, so you can load and save them */

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_HDISP_END + (i * 4);
		NVOutputWriteRAMDAC(output, ramdac_reg, regp->fp_horiz_regs[i]);
	}

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_VDISP_END + (i * 4);
		NVOutputWriteRAMDAC(output, ramdac_reg, regp->fp_vert_regs[i]);
	}

	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_HVALID_START, regp->fp_hvalid_start);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_HVALID_END, regp->fp_hvalid_end);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_VVALID_START, regp->fp_vvalid_start);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_VVALID_END, regp->fp_vvalid_end);
}

/* NOTE: Don't rely on this data for anything other than restoring VT's */

static void
nv_output_save (xf86OutputPtr output)
{
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state;
	NVOutputPrivatePtr nv_output = output->driver_private;
	int ramdac_backup = nv_output->ramdac;

	ErrorF("nv_output_save is called\n");

	/* This is early init and we have not yet been assigned a ramdac */
	/* Always choose the prefered ramdac, for consistentcy */
	/* Assumption: there is always once output that can only run of the primary ramdac */
	if (nv_output->valid_ramdac & RAMDAC_1) {
		nv_output->ramdac = 1;
	} else {
		nv_output->ramdac = 0;
	}

	state = &pNv->SavedReg;

	/* Due to strange mapping of outputs we could have swapped analog and digital */
	/* So we force save all the registers */
	nv_output_save_state_ext(output, state, TRUE);

	/* restore previous state */
	nv_output->ramdac = ramdac_backup;
}

static void
nv_output_restore (xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state;
	NVOutputPrivatePtr nv_output = output->driver_private;
	int ramdac_backup = nv_output->ramdac;

	ErrorF("nv_output_restore is called\n");

	/* We want consistent mode restoring and the ramdac entry is variable */
	/* Always choose the prefered ramdac, for consistentcy */
	/* Assumption: there is always once output that can only run of the primary ramdac */
	if (nv_output->valid_ramdac & RAMDAC_1) {
		nv_output->ramdac = 1;
	} else {
		nv_output->ramdac = 0;
	}

	state = &pNv->SavedReg;

	/* Due to strange mapping of outputs we could have swapped analog and digital */
	/* So we force load all the registers */
	nv_output_load_state_ext(output, state, TRUE);

	/* restore previous state */
	nv_output->ramdac = ramdac_backup;
}

static int
nv_output_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

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
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputPrivatePtr nv_output = output->driver_private;

	ErrorF("nv_output_mode_fixup is called\n");

	/* For internal panels and gpu scaling on DVI we need the native mode */
	if ((nv_output->type == OUTPUT_LVDS) || (pNv->fpScaler && (nv_output->type == OUTPUT_TMDS))) {
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

		xf86SetModeCrtc(adjusted_mode, INTERLACE_HALVE_V);
	}

	return TRUE;
}

static int
nv_output_tweak_panel(xf86OutputPtr output, NVRegPtr state)
{
    NVOutputPrivatePtr nv_output = output->driver_private;
    ScrnInfoPtr pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);
    NVOutputRegPtr regp;
    int tweak = 0;
  
    regp = &state->dac_reg[nv_output->ramdac];
    if (pNv->usePanelTweak) {
	tweak = pNv->PanelTweak;
    } else {
	/* begin flat panel hacks */
	/* This is unfortunate, but some chips need this register
	   tweaked or else you get artifacts where adjacent pixels are
	   swapped.  There are no hard rules for what to set here so all
	   we can do is experiment and apply hacks. */
    
	if(((pNv->Chipset & 0xffff) == 0x0328) && (regp->bpp == 32)) {
	    /* At least one NV34 laptop needs this workaround. */
	    tweak = -1;
	}
		
	if((pNv->Chipset & 0xfff0) == CHIPSET_NV31) {
	    tweak = 1;
	}
	/* end flat panel hacks */
    }
    return tweak;
}

static void
nv_output_mode_set_regs(xf86OutputPtr output, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	int bpp;
	NVPtr pNv = NVPTR(pScrn);
	NVFBLayout *pLayout = &pNv->CurrentLayout;
	RIVA_HW_STATE *state, *sv_state;
	Bool is_fp = FALSE;
	NVOutputRegPtr regp, regp2, savep;
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(pScrn);
	float aspect_ratio, panel_ratio;
	uint32_t h_scale, v_scale;
	int i;

	state = &pNv->ModeReg;
	regp = &state->dac_reg[nv_output->ramdac];
	/* The other ramdac */
	regp2 = &state->dac_reg[(~(nv_output->ramdac)) & 1];

	sv_state = &pNv->SavedReg;
	savep = &sv_state->dac_reg[nv_output->ramdac];

	if ((nv_output->type == OUTPUT_LVDS) || (nv_output->type == OUTPUT_TMDS)) {
		is_fp = TRUE;

		regp->fp_horiz_regs[REG_DISP_END] = adjusted_mode->HDisplay - 1;
		regp->fp_horiz_regs[REG_DISP_TOTAL] = adjusted_mode->HTotal - 1;
		regp->fp_horiz_regs[REG_DISP_CRTC] = adjusted_mode->HDisplay;
		regp->fp_horiz_regs[REG_DISP_SYNC_START] = adjusted_mode->HSyncStart - 1;
		regp->fp_horiz_regs[REG_DISP_SYNC_END] = adjusted_mode->HSyncEnd - 1;
		regp->fp_horiz_regs[REG_DISP_VALID_START] = adjusted_mode->HSkew;
		regp->fp_horiz_regs[REG_DISP_VALID_END] = adjusted_mode->HDisplay - 1;

		regp->fp_vert_regs[REG_DISP_END] = adjusted_mode->VDisplay - 1;
		regp->fp_vert_regs[REG_DISP_TOTAL] = adjusted_mode->VTotal - 1;
		regp->fp_vert_regs[REG_DISP_CRTC] = adjusted_mode->VDisplay;
		regp->fp_vert_regs[REG_DISP_SYNC_START] = adjusted_mode->VSyncStart - 1;
		regp->fp_vert_regs[REG_DISP_SYNC_END] = adjusted_mode->VSyncEnd - 1;
		regp->fp_vert_regs[REG_DISP_VALID_START] = 0;
		regp->fp_vert_regs[REG_DISP_VALID_END] = adjusted_mode->VDisplay - 1;

		ErrorF("Horizontal:\n");
		ErrorF("REG_DISP_END: 0x%X\n", regp->fp_horiz_regs[REG_DISP_END]);
		ErrorF("REG_DISP_TOTAL: 0x%X\n", regp->fp_horiz_regs[REG_DISP_TOTAL]);
		ErrorF("REG_DISP_CRTC: 0x%X\n", regp->fp_horiz_regs[REG_DISP_CRTC]);
		ErrorF("REG_DISP_SYNC_START: 0x%X\n", regp->fp_horiz_regs[REG_DISP_SYNC_START]);
		ErrorF("REG_DISP_SYNC_END: 0x%X\n", regp->fp_horiz_regs[REG_DISP_SYNC_END]);
		ErrorF("REG_DISP_VALID_START: 0x%X\n", regp->fp_horiz_regs[REG_DISP_VALID_START]);
		ErrorF("REG_DISP_VALID_END: 0x%X\n", regp->fp_horiz_regs[REG_DISP_VALID_END]);

		ErrorF("Vertical:\n");
		ErrorF("REG_DISP_END: 0x%X\n", regp->fp_vert_regs[REG_DISP_END]);
		ErrorF("REG_DISP_TOTAL: 0x%X\n", regp->fp_vert_regs[REG_DISP_TOTAL]);
		ErrorF("REG_DISP_CRTC: 0x%X\n", regp->fp_vert_regs[REG_DISP_CRTC]);
		ErrorF("REG_DISP_SYNC_START: 0x%X\n", regp->fp_vert_regs[REG_DISP_SYNC_START]);
		ErrorF("REG_DISP_SYNC_END: 0x%X\n", regp->fp_vert_regs[REG_DISP_SYNC_END]);
		ErrorF("REG_DISP_VALID_START: 0x%X\n", regp->fp_vert_regs[REG_DISP_VALID_START]);
		ErrorF("REG_DISP_VALID_END: 0x%X\n", regp->fp_vert_regs[REG_DISP_VALID_END]);
	}

	/* This seems to be a common mode
	* bit0: positive vsync
	* bit4: positive hsync
	* bit8: enable panel scaling 
	* bit31: sometimes seen on LVDS panels
	* This must also be set for non-flatpanels
	*/
	regp->fp_control = 0x11100000;
	if (nv_output->type == OUTPUT_LVDS);
		regp->fp_control = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_CONTROL) & 0xfff00000;

	/* Deal with vsync/hsync polarity */
	if (adjusted_mode->Flags & V_PVSYNC) {
		regp->fp_control |= (1 << 0);
	}

	if (adjusted_mode->Flags & V_PHSYNC) {
		regp->fp_control |= (1 << 4);
	}

	if (is_fp) {
		ErrorF("Pre-panel scaling\n");
		ErrorF("panel-size:%dx%d\n", nv_output->fpWidth, nv_output->fpHeight);
		panel_ratio = (nv_output->fpWidth)/(float)(nv_output->fpHeight);
		ErrorF("panel_ratio=%f\n", panel_ratio);
		aspect_ratio = (mode->HDisplay)/(float)(mode->VDisplay);
		ErrorF("aspect_ratio=%f\n", aspect_ratio);
		/* Scale factors is the so called 20.12 format, taken from Haiku */
		h_scale = ((1 << 12) * mode->HDisplay)/nv_output->fpWidth;
		v_scale = ((1 << 12) * mode->VDisplay)/nv_output->fpHeight;
		ErrorF("h_scale=%d\n", h_scale);
		ErrorF("v_scale=%d\n", v_scale);

		/* Don't limit last fetched line */
		regp->debug_2 = 0;

		/* We want automatic scaling */
		regp->debug_1 = 0;

		regp->fp_hvalid_start = 0;
		regp->fp_hvalid_end = (nv_output->fpWidth - 1);

		regp->fp_vvalid_start = 0;
		regp->fp_vvalid_end = (nv_output->fpHeight - 1);

		if (!pNv->fpScaler) {
			ErrorF("Flat panel is doing the scaling.\n");
			regp->fp_control |= (1 << 8);
		} else {
			ErrorF("GPU is doing the scaling.\n");
			/* GPU scaling happens automaticly at a ratio of 1.33 */
			/* A 1280x1024 panel has a ratio of 1.25, we don't want to scale that at 4:3 resolutions */
			if (h_scale != (1 << 12) && (panel_ratio > (aspect_ratio + 0.10))) {
				uint32_t diff;

				ErrorF("Scaling resolution on a widescreen panel\n");

				/* Scaling in both directions needs to the same */
				h_scale = v_scale;

				/* Set a new horizontal scale factor and enable testmode (bit12) */
				regp->debug_1 = ((h_scale >> 1) & 0xfff) | (1 << 12);

				diff = nv_output->fpWidth - (((1 << 12) * mode->HDisplay)/h_scale);
				regp->fp_hvalid_start = diff/2;
				regp->fp_hvalid_end = nv_output->fpWidth - (diff/2) - 1;
			}

			/* Same scaling, just for panels with aspect ratio's smaller than 1 */
			if (v_scale != (1 << 12) && (panel_ratio < (aspect_ratio - 0.10))) {
				uint32_t diff;

				ErrorF("Scaling resolution on a portrait panel\n");

				/* Scaling in both directions needs to the same */
				v_scale = h_scale;

				/* Set a new vertical scale factor and enable testmode (bit28) */
				regp->debug_1 = (((v_scale >> 1) & 0xfff) << 16) | (1 << (12 + 16));

				diff = nv_output->fpHeight - (((1 << 12) * mode->VDisplay)/v_scale);
				regp->fp_vvalid_start = diff/2;
				regp->fp_vvalid_end = nv_output->fpHeight - (diff/2) - 1;
			}
		}

		ErrorF("Post-panel scaling\n");
	}

	if (pNv->Architecture >= NV_ARCH_10) {
		/* Bios and blob don't seem to do anything (else) */
		regp->nv10_cursync = (1<<25);
	}

	/* These are the common blob values, minus a few fp specific bit's */
	/* The OR mask is in case the powerdown switch was enabled from the other output */
	regp->debug_0 |= 0x1101111;

	if(is_fp) {
		/* I am not completely certain, but seems to be set only for dfp's */
		regp->debug_0 |= NV_RAMDAC_FP_DEBUG_0_TMDS_ENABLED;
	}

	/* We must ensure that we never disable the wrong tmds control */
	/* Assumption: one output can only run of ramdac 0 */
	if ((nv_output->ramdac == 0) && (nv_output->valid_ramdac & RAMDAC_1)) {
		if (is_fp) {
			regp2->debug_0 &= ~NV_RAMDAC_FP_DEBUG_0_PWRDOWN_BOTH;
		} else {
			regp2->debug_0 |= NV_RAMDAC_FP_DEBUG_0_PWRDOWN_BOTH;
		}
	} else {
		if (is_fp) {
			regp->debug_0 &= ~NV_RAMDAC_FP_DEBUG_0_PWRDOWN_BOTH;
		} else {
			regp->debug_0 |= NV_RAMDAC_FP_DEBUG_0_PWRDOWN_BOTH;
		}
	}

	ErrorF("output %d debug_0 %08X\n", nv_output->ramdac, regp->debug_0);

	/* This is just a guess, there are probably more registers which need setting */
	/* But we must start somewhere ;-) */
	if (is_fp) {
		regp->TMDS[0x4] = 0x80;
		/* Enable crosswired mode */
		/* As far as i know, this may never be set on ramdac 0 tmds registers (ramdac 1 -> crosswired -> ramdac 0 tmds regs) */
		/* This will upset the monitor, trust me, i know it :-( */
		/* Now allowed for non-bios inited systems */
		if ((nv_output->ramdac == 0) && (nv_output->valid_ramdac & RAMDAC_1)) {
			regp->TMDS[0x4] |= (1 << 3);
		}
	}

	/* The TMDS game begins */
	/* A few registers are also programmed on non-tmds monitors */
	/* At the moment i can't give rationale for these values */
	if (!is_fp) {
		regp->TMDS[0x2e] = 0x80;
		regp->TMDS[0x2f] = 0xff;
		regp->TMDS[0x33] = 0xfe;
	} else {
		NVCrtcPrivatePtr nv_crtc = output->crtc->driver_private;
		uint32_t pll_setup_control = nvReadRAMDAC(pNv, 0, NV_RAMDAC_PLL_SETUP_CONTROL);
		regp->TMDS[0x2b] = 0x7d;
		regp->TMDS[0x2c] = 0x0;
		if (nv_crtc->head == 1) {
			regp->TMDS[0x2e] = 0x81;
		} else {
			regp->TMDS[0x2e] = 0x85;
		}
		regp->TMDS[0x2f] = 0x21;
		regp->TMDS[0x30] = 0x0;
		regp->TMDS[0x31] = 0x0;
		regp->TMDS[0x32] = 0x0;
		regp->TMDS[0x33] = 0xf0;
		regp->TMDS[0x3a] = 0x80;

		/* Here starts the registers that may cause problems for some */
		/* This an educated guess */
		if (pNv->misc_info.reg_c040 & (1 << 10)) {
			regp->TMDS[0x5] = 0x68;
		} else {
			regp->TMDS[0x5] = 0x6e;
		}

		/* This seems to be related to PLL_SETUP_CONTROL */
		/* When PLL_SETUP_CONTROL ends with 0x1c, then this value is 0xc1 */
		/* Otherwise 0xf1 */
		if ((pll_setup_control & 0xff) == 0x1c) {
			regp->TMDS[0x0] = 0xc1;
		} else {
			regp->TMDS[0x0] = 0xf1;
		}

		/* This is also related to PLL_SETUP_CONTROL, exactly how is unknown */
		if (pll_setup_control == 0) {
			regp->TMDS[0x1] = 0x0;
		} else {
			if (nvReadRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK) & (1<<12)) {
				regp->TMDS[0x1] = 0x41;
			} else {
				regp->TMDS[0x1] = 0x42;
			}
		}

		if (pll_setup_control == 0x0) {
			regp->TMDS[0x2] = 0x90;
		} else {
			regp->TMDS[0x2] = 0x89;
		}
		/* This test is not needed for me although the blob sets this value */
		/* It may be wrong, but i'm leaving it for historical reference */
		/*if (pNv->misc_info.reg_c040 == 0x3c0bc003 || pNv->misc_info.reg_c040 == 0x3c0bc333) {
			regp->TMDS[0x2] = 0xa9;
		}*/
	}

	/* Flatpanel support needs at least a NV10 */
	if(pNv->twoHeads) {
		/* Instead of 1, several other values are also used: 2, 7, 9 */
		/* The purpose is unknown */
		if(pNv->FPDither) {
			regp->dither = 0x00010000;
		}
	}

	if(pLayout->depth < 24) {
		bpp = pLayout->depth;
	} else {
		bpp = 32;
	}

	/* Kindly borrowed from haiku driver */
	/* bit4 and bit5 activate indirect mode trough color palette */
	switch (pLayout->depth) {
		case 32:
		case 16:
			regp->general = 0x00101130;
			break;
		case 24:
		case 15:
			regp->general = 0x00100130;
			break;
		case 8:
		default:
			regp->general = 0x00101100;
			break;
	}

	if (pNv->alphaCursor) {
		regp->general |= (1<<29);
	}

	regp->bpp = bpp;    /* this is not bitsPerPixel, it's 8,15,16,32 */

	/* Some values the blob sets */
	/* This may apply to the real ramdac that is being used (for crosswired situations) */
	/* Nevertheless, it's unlikely to cause many problems, since the values are equal for both */
	regp->unk_a20 = 0x0;
	regp->unk_a24 = 0xfffff;
	regp->unk_a34 = 0x1;

	/* Put test control into what seems to be the neutral position */
	regp->test_control = 0xf0000000;

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

		if (is_fp == TRUE) {
			regp->output = 0x0;
		} else { 
			regp->output = NV_RAMDAC_OUTPUT_DAC_ENABLE;
		}

		if (nv_crtc->head == 1) {
			regp->output |= NV_RAMDAC_OUTPUT_SELECT_VPLL2;
		} else {
			regp->output &= ~NV_RAMDAC_OUTPUT_SELECT_VPLL2;
		}

		ErrorF("%d: crtc %d output%d: %04X: twocrt %d twomon %d\n", is_fp, nv_crtc->crtc, nv_output->ramdac, regp->output, two_crt, two_mon);
	}
}

static void
nv_output_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		   DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr	pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);
    RIVA_HW_STATE *state;

	ErrorF("nv_output_mode_set is called\n");

    state = &pNv->ModeReg;

    nv_output_mode_set_regs(output, mode, adjusted_mode);
    nv_output_load_state_ext(output, state, FALSE);
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
	Bool present[2];
	present[0] = FALSE;
	present[1] = FALSE;
	int ramdac;

	/* Restrict to primary ramdac for now, because i get false positives on the secondary */
	for (ramdac = 0; ramdac < 1; ramdac++) {
		reg_output = nvReadRAMDAC(pNv, ramdac, NV_RAMDAC_OUTPUT);
		reg_test_ctrl = nvReadRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_CONTROL);

		nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_CONTROL, (reg_test_ctrl & ~0x00010000));

		nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_OUTPUT, (reg_output & 0x0000FEEE));
		usleep(1000);

		temp = nvReadRAMDAC(pNv, ramdac, NV_RAMDAC_OUTPUT);
		nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_OUTPUT, temp | 1);

		nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_DATA, 0x94050140);
		temp = nvReadRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_CONTROL);
		nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_CONTROL, temp | 0x1000);

		usleep(1000);

		present[ramdac] = (nvReadRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_CONTROL) & (1 << 28)) ? TRUE : FALSE;

		temp = NVOutputReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL);
		nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_CONTROL, temp & 0x000EFFF);

		nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_OUTPUT, reg_output);
		nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_CONTROL, reg_test_ctrl);
	}

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "CRT detect returned %d for ramdac0\n", present[0]);
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "CRT detect returned %d for ramdac1\n", present[1]);

	/* Can we only be ramdac0 ?*/
	if (!(nv_output->valid_ramdac & RAMDAC_1)) {
		if (present[0]) 
			return TRUE;
	} else {
		if (present[1])
			return TRUE;
		/* What do with a secondary output running of the primary ramdac? */
	}

	return FALSE;
}

static xf86OutputStatus
nv_tmds_output_detect(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	ErrorF("nv_tmds_output_detect is called\n");

	if (nv_ddc_detect(output))
		return XF86OutputStatusConnected;

	return XF86OutputStatusDisconnected;
}


static xf86OutputStatus
nv_analog_output_detect(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	ErrorF("nv_analog_output_detect is called\n");

	if (nv_ddc_detect(output))
		return XF86OutputStatusConnected;

	/* This may not work in all cases, but it's the best that can be done */
	/* Example: Secondary output running of primary ramdac, what to do? */
	//if (nv_crt_load_detect(output))
	//	return XF86OutputStatusConnected;

	return XF86OutputStatusDisconnected;
}

static DisplayModePtr
nv_output_get_modes(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	xf86MonPtr ddc_mon;
	DisplayModePtr ddc_modes;
	ScrnInfoPtr pScrn = output->scrn;

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
		/* We want the new mode to be preferred */
		for (mode = ddc_modes; mode != NULL; mode = mode->next) {
			if (mode->type & M_T_PREFERRED) {
				mode->type &= ~M_T_PREFERRED;
			}
		}
		ddc_modes = xf86ModesAdd(ddc_modes, nv_output->native_mode);
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
nv_clear_ramdac_from_outputs(xf86OutputPtr output, int ramdac)
{
	int i;
	ScrnInfoPtr pScrn = output->scrn;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	xf86OutputPtr output2;
	NVOutputPrivatePtr nv_output2;
	for (i = 0; i < xf86_config->num_output; i++) {
		output2 = xf86_config->output[i];
		nv_output2 = output2->driver_private;
		if (nv_output2->ramdac == ramdac && output != output2) {
			nv_output2->ramdac = -1;
			nv_output2->ramdac_assigned = FALSE;
			break;
		}
	}
}

static void
nv_output_prepare(xf86OutputPtr output)
{
	ErrorF("nv_output_prepare is called\n");
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcPtr crtc = output->crtc;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	output->funcs->dpms(output, DPMSModeOff);

	if (nv_output->ramdac_assigned) {
		ErrorF("We already have a ramdac.\n");
		return;
	}

	/* We need this ramdac, so let's steal it */
	if (!(nv_output->valid_ramdac & RAMDAC_1) && pNv->ramdac_active[0]) {
		ErrorF("Stealing ramdac0 ;-)\n");
		int i;
		xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
		xf86OutputPtr output2;
		NVOutputPrivatePtr nv_output2;
		for (i = 0; i < xf86_config->num_output; i++) {
			output2 = xf86_config->output[i];
			nv_output2 = output2->driver_private;
			if (nv_output2->ramdac == 0 && output != output2) {
				nv_output2->ramdac = -1;
				nv_output2->ramdac_assigned = FALSE;
				break;
			}
		}
		pNv->ramdac_active[0] = FALSE;
	}

	/* I sometimes get the strange feeling that ramdac's like to be paired with their matching crtc */
	if ((nv_output->valid_ramdac & RAMDAC_0) && !(pNv->ramdac_active[0]) && nv_crtc->head == 0) {
		ErrorF("Activating ramdac %d\n", 0);
		pNv->ramdac_active[0] = TRUE;
		nv_output->ramdac = 0;
	} else if ((nv_output->valid_ramdac & RAMDAC_1) && !(pNv->ramdac_active[1]) && nv_crtc->head == 1) {
		ErrorF("Activating ramdac %d\n", 1);
		pNv->ramdac_active[1] = TRUE;
		nv_output->ramdac = 1;
	}

	if (nv_output->ramdac != -1) {
		nv_output->ramdac_assigned = TRUE;
		nv_clear_ramdac_from_outputs(output, nv_output->ramdac);
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
    .get_modes = nv_output_get_modes,
    .destroy = nv_output_destroy,
    .prepare = nv_output_prepare,
    .commit = nv_output_commit,
};

static const xf86OutputFuncsRec nv_tmds_output_funcs = {
    .dpms = nv_tmds_output_dpms,
    .save = nv_output_save,
    .restore = nv_output_restore,
    .mode_valid = nv_output_mode_valid,
    .mode_fixup = nv_output_mode_fixup,
    .mode_set = nv_output_mode_set,
    .detect = nv_tmds_output_detect,
    .get_modes = nv_output_get_modes,
    .destroy = nv_output_destroy,
    .prepare = nv_output_prepare,
    .commit = nv_output_commit,
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

	if (pNv->fp_native_mode || nv_ddc_detect(output))
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

	if (modes = nv_output_get_modes(output))
		return modes;

	/* it is possible to set up a mode from what we can read from the
	 * RAMDAC registers, but if we can't read the BIOS table correctly
	 * we might as well give up */
	if (pNv->fp_native_mode == NULL)
		return NULL;

	nv_output->fpWidth = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_HDISP_END) + 1;
	nv_output->fpHeight = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_VDISP_END) + 1;
	nv_output->fpSyncs = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_CONTROL) & 0x30000033;

	if (pNv->fp_native_mode->HDisplay != nv_output->fpWidth ||
		pNv->fp_native_mode->VDisplay != nv_output->fpHeight) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"Panel size mismatch; ignoring RAMDAC\n");
		nv_output->fpWidth = pNv->fp_native_mode->HDisplay;
		nv_output->fpHeight = pNv->fp_native_mode->VDisplay;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Panel size is %lu x %lu\n",
		nv_output->fpWidth, nv_output->fpHeight);

	nv_output->native_mode = xf86DuplicateMode(pNv->fp_native_mode);

	return xf86DuplicateMode(pNv->fp_native_mode);
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
};

static void nv_add_analog_output(ScrnInfoPtr pScrn, int order, int i2c_index, Bool dvi_pair)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86OutputPtr	    output;
	NVOutputPrivatePtr    nv_output;
	char outputname[20];
	int crtc_mask = 0;
	int real_index;
	Bool create_output = TRUE;

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

	if (pNv->dcb_table.i2c_read[i2c_index] && pNv->pI2CBus[i2c_index] == NULL)
		NV_I2CInit(pScrn, &pNv->pI2CBus[i2c_index], pNv->dcb_table.i2c_read[i2c_index], xstrdup(outputname));

	nv_output->type = OUTPUT_ANALOG;

	/* order:
	 * bit0: RAMDAC_0 valid
	 * bit1: RAMDAC_1 valid
	 * So lowest order has highest priority.
	 */
	nv_output->valid_ramdac = order;

	/* Some early nvidia cards have outputs only valid on secondary */
	if (nv_output->valid_ramdac & RAMDAC_0) 
		crtc_mask |= (1<<0);

	/* Restricting this will cause a full mode set when trying to squeeze in the primary mode */
	if (nv_output->valid_ramdac & RAMDAC_1) 
		crtc_mask |= (1<<1);

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

	nv_output->ramdac = -1;

	output->possible_crtcs = crtc_mask;
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Adding output %s\n", outputname);
}

static void nv_add_digital_output(ScrnInfoPtr pScrn, int order, int i2c_index, int lvds)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86OutputPtr	    output;
	NVOutputPrivatePtr    nv_output;
	char outputname[20];
	int crtc_mask = 0;
	Bool create_output = TRUE;
	int index = i2c_index;

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

	if (pNv->dcb_table.i2c_read[i2c_index] && pNv->pI2CBus[i2c_index] == NULL)
		NV_I2CInit(pScrn, &pNv->pI2CBus[i2c_index], pNv->dcb_table.i2c_read[i2c_index], xstrdup(outputname));

	nv_output->pDDCBus = pNv->pI2CBus[i2c_index];

	/* order:
	 * bit0: RAMDAC_0 valid
	 * bit1: RAMDAC_1 valid
	 * So lowest order has highest priority.
	 */
	nv_output->valid_ramdac = order;

	/* Some early nvidia cards have outputs only valid on secondary */
	if (nv_output->valid_ramdac & RAMDAC_0) 
		crtc_mask |= (1<<0);

	/* Restricting this will cause a full mode set when trying to squeeze in the primary mode */
	if (nv_output->valid_ramdac & RAMDAC_1) 
		crtc_mask |= (1<<1);

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

	nv_output->ramdac = -1;

	output->possible_crtcs = crtc_mask;
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Adding output %s\n", outputname);
}

void NvDCBSetupOutputs(ScrnInfoPtr pScrn)
{
	unsigned char type, i2c_index, old_i2c_index, or;
	NVPtr pNv = NVPTR(pScrn);
	int i;
	Bool dvi_pair[MAX_NUM_DCB_ENTRIES];

	/* check how many TMDS ports there are */
	if (pNv->dcb_table.entries) {
		for (i = 0 ; i < pNv->dcb_table.entries; i++) {
			type = pNv->dcb_table.connection[i] & 0xf;
			old_i2c_index = i2c_index;
			i2c_index = (pNv->dcb_table.connection[i] >> 4) & 0xf;

			dvi_pair[i] = FALSE;

			/* Are we on the same i2c index? */
			if (i2c_index != 0xf && i2c_index == old_i2c_index) {
				/* Have we passed the analog connector or not? */
				if (type == OUTPUT_TMDS) {
					dvi_pair[i - 1] = TRUE;
				} else if (type == OUTPUT_ANALOG) {
					dvi_pair[i ] = TRUE;
				}
			}
		}
	}

	/* It's time to gather some information */

	/* Being slaved indicates we're a flatpanel (or tv-out) */
	if (NVReadVGA0(pNv, NV_VGA_CRTCX_PIXEL) & 0x80) {
		pNv->output_info |= OUTPUT_0_SLAVED;
	}
	if (NVReadVGA1(pNv, NV_VGA_CRTCX_PIXEL) & 0x80) {
		pNv->output_info |= OUTPUT_1_SLAVED;
	}
	/* This is an educated guess */
	if (NVReadTMDS(pNv, 0, 0x4) & (1 << 3)) {
		pNv->output_info |= OUTPUT_0_CROSSWIRED_TMDS;
	}
	if (NVReadTMDS(pNv, 1, 0x4) & (1 << 3)) {
		pNv->output_info |= OUTPUT_1_CROSSWIRED_TMDS;
	}
	/* Are we LVDS? */
	if (NVReadTMDS(pNv, 0, 0x4) & (1 << 0)) {
		pNv->output_info |= OUTPUT_0_LVDS;
	}
	if (NVReadTMDS(pNv, 1, 0x4) & (1 << 0)) {
		pNv->output_info |= OUTPUT_1_LVDS;
	}

	/* we setup the outputs up from the BIOS table */
	for (i = 0 ; i < pNv->dcb_table.entries; i++) {
		type = pNv->dcb_table.connection[i] & 0xf;
		i2c_index = (pNv->dcb_table.connection[i] >> 4) & 0xf;
		or = ffs((pNv->dcb_table.connection[i] >> 24) & 0xf);

		if (type < 4) {
			xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "DCB entry: %d: %08X type: %d, i2c_index: %d, or: %d\n", i, pNv->dcb_table.connection[i], type, i2c_index, or);

			switch(type) {
			case OUTPUT_ANALOG:
				nv_add_analog_output(pScrn, or, i2c_index, dvi_pair[i]);
				break;
			case OUTPUT_TMDS:
				nv_add_digital_output(pScrn, or, i2c_index, 0);
				break;
			case OUTPUT_LVDS:
				nv_add_digital_output(pScrn, or, i2c_index, 1);
				break;
			default:
				break;
			}
		}
	}
}

void NvSetupOutputs(ScrnInfoPtr pScrn)
{
	int i;
	NVPtr pNv = NVPTR(pScrn);

	pNv->Television = FALSE;

	memset(pNv->pI2CBus, 0, sizeof(pNv->pI2CBus));
	NvDCBSetupOutputs(pScrn);

#if 0
	xf86OutputPtr output;
	NVOutputPrivatePtr nv_output;

    if (pNv->Mobile) {
	output = xf86OutputCreate(pScrn, &nv_output_funcs, OutputType[OUTPUT_LVDS]);
	if (!output)
	    return;

	nv_output = xnfcalloc(sizeof(NVOutputPrivateRec), 1);
	if (!nv_output) {
	    xf86OutputDestroy(output);
	    return;
	}

	output->driver_private = nv_output;
	nv_output->type = output_type;

	output->possible_crtcs = i ? 1 : crtc_mask;
    }
#endif
}

#endif /* ENABLE_RANDR12 */

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
