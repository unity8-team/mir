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
	NVWriteTMDS(pNv, nv_output->preferred_ramdac, tmds_reg, val);
}

CARD8 NVOutputReadTMDS(xf86OutputPtr output, CARD32 tmds_reg)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must read from the "bus" of the output */
	return NVReadTMDS(pNv, nv_output->preferred_ramdac, tmds_reg);
}

void NVOutputWriteTMDS2(xf86OutputPtr output, CARD32 tmds_reg, CARD32 val)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must write to the "bus" of the output */
	NVWriteTMDS2(pNv, nv_output->preferred_ramdac, tmds_reg, val);
}

CARD8 NVOutputReadTMDS2(xf86OutputPtr output, CARD32 tmds_reg)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	/* We must read from the "bus" of the output */
	return NVReadTMDS2(pNv, nv_output->preferred_ramdac, tmds_reg);
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

static void dpms_update_output_ramdac(NVPtr pNv, NVOutputPrivatePtr nv_output, int mode)
{
	if (nv_output->ramdac == -1)
		return;

	/* We may be going for modesetting, so we must reset the ramdacs */
	if (mode == DPMSModeOff) {
		pNv->ramdac_active[nv_output->ramdac] = FALSE;
		NVWriteVGACR5758(pNv, nv_output->ramdac, 0, 0x7f);
		NVWriteVGACR5758(pNv, nv_output->ramdac, 2, 0);
		ErrorF("Deactivating ramdac %d\n", nv_output->ramdac);
		return;
	}

	/* The previous call was not a modeset, but a normal dpms call */
	pNv->ramdac_active[nv_output->ramdac] = TRUE;
	NVWriteVGACR5758(pNv, nv_output->ramdac, 0, pNv->dcb_table.entry[nv_output->dcb_entry].type);
	NVWriteVGACR5758(pNv, nv_output->ramdac, 2, pNv->dcb_table.entry[nv_output->dcb_entry].or);
	ErrorF("Activating ramdac %d\n", nv_output->ramdac);
}

static void
nv_lvds_output_dpms(xf86OutputPtr output, int mode)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVPtr pNv = NVPTR(output->scrn);

	dpms_update_output_ramdac(pNv, nv_output, mode);

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
}

static void
nv_analog_output_dpms(xf86OutputPtr output, int mode)
{
	xf86CrtcPtr crtc = output->crtc;
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVPtr pNv = NVPTR(output->scrn);

	ErrorF("nv_analog_output_dpms is called with mode %d\n", mode);

	dpms_update_output_ramdac(pNv, nv_output, mode);

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

	dpms_update_output_ramdac(pNv, nv_output, mode);

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
}

/* This sequence is an optimized/shortened version of what the blob does */
/* 0x40 and 0x43 are dual link dvi/lvds related, so don't touch them for now */
uint32_t tmds_regs_nv40[] = { 0x04, 0x05, 0x2f, 0x30, 0x31, 0x32, 0x33, /* 0x40, 0x43,*/ 0x00, 0x01, 0x02, 0x2e, 0x2f, 0x3a, 0x2b };
uint32_t tmds_regs_nv30[] = { 0x04, 0x05, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x29, 0x2a, 0x00, 0x01, 0x02, 0x2e, 0x2f, 0x3a, 0x2b };

#define TMDS_REGS(index) ( tmds_regs(pNv, index) )

uint32_t tmds_regs(NVPtr pNv, int i)
{
	if (pNv->Architecture == NV_ARCH_40) {
		return tmds_regs_nv40[i];
	} else {
		return tmds_regs_nv30[i];
	}
}

#define TMDS_SIZE ( tmds_size(pNv) )

uint32_t tmds_size(NVPtr pNv)
{
	if (pNv->Architecture == NV_ARCH_40) {
		return(sizeof(tmds_regs_nv40)/sizeof(tmds_regs_nv40[0]));
	} else {
		return(sizeof(tmds_regs_nv30)/sizeof(tmds_regs_nv30[0]));
	}
}

/* Does anyone know the precise purpose of this second register set? */
uint32_t tmds2_regs_nv40[] = { 0x2b };
uint32_t tmds2_regs_nv30[] = { 0x2b };

#define TMDS2_REGS(index) ( tmds2_regs(pNv, index) )

uint32_t tmds2_regs(NVPtr pNv, int i)
{
	if (pNv->Architecture == NV_ARCH_40) {
		return tmds2_regs_nv40[i];
	} else {
		return tmds2_regs_nv30[i];
	}
}

#define TMDS2_SIZE ( tmds2_size(pNv) )

uint32_t tmds2_size(NVPtr pNv)
{
	if (pNv->Architecture == NV_ARCH_40) {
		return(sizeof(tmds2_regs_nv40)/sizeof(tmds2_regs_nv40[0]));
	} else {
		return(sizeof(tmds2_regs_nv30)/sizeof(tmds2_regs_nv30[0]));
	}
}

void nv_output_save_state_ext(xf86OutputPtr output, RIVA_HW_STATE *state, Bool override)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputRegPtr regp;
	int i;

	regp = &state->dac_reg[nv_output->ramdac];
	state->config       = nvReadFB(pNv, NV_PFB_CFG0);

	/* This exists purely for proper text mode restore */
	if (override) regp->output = NVOutputReadRAMDAC(output, NV_RAMDAC_OUTPUT);

	for (i = 0; i < TMDS_SIZE; i++) {
		regp->TMDS[TMDS_REGS(i)] = NVOutputReadTMDS(output, TMDS_REGS(i));
	}

	/* If the data ever changes between TMDS and TMDS2, then regp data needs a new set */
	/* TMDS2 is related to dual link dvi/lvds, leave it untouched for the moment */
	//for (i = 0; i < TMDS2_SIZE; i++) {
	//	regp->TMDS[TMDS2_REGS(i)] = NVOutputReadTMDS2(output, TMDS2_REGS(i));
	//}
}

void nv_output_load_state_ext(xf86OutputPtr output, RIVA_HW_STATE *state, Bool override)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	NVOutputRegPtr regp;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	int i;

	regp = &state->dac_reg[nv_output->ramdac];

	/* This exists purely for proper text mode restore */
	if (override) NVOutputWriteRAMDAC(output, NV_RAMDAC_OUTPUT, regp->output);

	for (i = 0; i < TMDS_SIZE; i++) {
		NVOutputWriteTMDS(output, TMDS_REGS(i), regp->TMDS[TMDS_REGS(i)]);
	}

	/* If the data ever changes between TMDS and TMDS2, then regp data needs a new set */
	/* TMDS2 is related to dual link dvi/lvds, leave it untouched for the moment */
	//for (i = 0; i < TMDS2_SIZE; i++) {
	//	NVOutputWriteTMDS(output, TMDS2_REGS(i), regp->TMDS[TMDS2_REGS(i)]);
	//}
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
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state, *sv_state;
	Bool is_fp = FALSE;
	Bool is_lvds = FALSE;
	NVOutputRegPtr regp, regp2, savep;
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	xf86CrtcPtr crtc = output->crtc;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	state = &pNv->ModeReg;
	regp = &state->dac_reg[nv_output->ramdac];
	/* The other ramdac */
	regp2 = &state->dac_reg[(~(nv_output->ramdac)) & 1];

	sv_state = &pNv->SavedReg;
	savep = &sv_state->dac_reg[nv_output->ramdac];

	if ((nv_output->type == OUTPUT_LVDS) || (nv_output->type == OUTPUT_TMDS)) {
		is_fp = TRUE;
		if (nv_output->type == OUTPUT_LVDS) {
			is_lvds = TRUE;
		}
	}

	/* This is just a guess, there are probably more registers which need setting */
	/* But we must start somewhere ;-) */
	if (is_fp) {
		regp->TMDS[0x4] = 0x80;
		/* Enable crosswired mode */
		/* This will upset the monitor, trust me, i know it :-( */
		/* Now allowed for non-bios inited systems */
		//if (nv_output->ramdac != nv_output->preferred_ramdac) {
		if (nv_crtc->head != nv_output->preferred_ramdac) {
			regp->TMDS[0x4] |= (1 << 3);
		}

		if (is_lvds) {
			regp->TMDS[0x4] |= (1 << 0);
		}
	}

	/* The TMDS game begins */
	/* A few registers are also programmed on non-tmds monitors */
	/* At the moment i can't give rationale for these values */
	if (!is_fp) {
		regp->TMDS[0x2b] = 0x7d;
	} else {
		uint32_t pll_setup_control = nvReadRAMDAC(pNv, 0, NV_RAMDAC_PLL_SETUP_CONTROL);
		regp->TMDS[0x2b] = 0x7d;
		regp->TMDS[0x2c] = 0x0;
		/* Various combinations exist for lvds, 0x08, 0x48, 0xc8, 0x88 */
		/* 0x88 seems most popular and (maybe) the end setting */
		if (is_lvds) {
			if (pNv->Architecture == NV_ARCH_40) {
				regp->TMDS[0x2e] = 0x88;
			} else {
				/* observed on nv31m */
				regp->TMDS[0x2e] = 0x0;
			}
		} else {
			/* 0x81 is also observed, but it's much rarer */
			regp->TMDS[0x2e] = 0x85;
		}
		/* 0x08 is also seen for lvds */
		regp->TMDS[0x2f] = 0x21;
		regp->TMDS[0x30] = 0x0;
		regp->TMDS[0x31] = 0x0;
		regp->TMDS[0x32] = 0x0;
		regp->TMDS[0x33] = 0xf0;
		regp->TMDS[0x3a] = 0x0;

		/* Sometimes 0x68 is also used */
		regp->TMDS[0x5] = 0x6e;

		if (is_lvds) {
			if (pNv->Architecture == NV_ARCH_40) {
				regp->TMDS[0x0] = 0x60;
			} else {
				/* observed on a nv31m */
				regp->TMDS[0x0] = 0x70;
			}
		} else {
			/* This seems to be related to PLL_SETUP_CONTROL */
			/* When PLL_SETUP_CONTROL ends with 0x1c, then this value is 0xcX */
			/* Otherwise 0xfX */
			if ((pll_setup_control & 0xff) == 0x1c) {
				regp->TMDS[0x0] = 0xc0;
			} else {
				regp->TMDS[0x0] = 0xf0;
			}
		}

		/* Not a 100% sure if this is not crtc determined */
		if (nv_output->preferred_ramdac != nv_output->ramdac) {
			regp->TMDS[0x0] |= 0xa;
		} else {
			regp->TMDS[0x0] |= 0x1;
		}

		if (pll_setup_control == 0) {
			regp->TMDS[0x1] = 0x0;
		} else {
			/* Also seen: 0x47, what does this register do? */
			if (nv_output->ramdac != nv_output->preferred_ramdac) {
				regp->TMDS[0x1] = 0x42;
			} else {
				regp->TMDS[0x1] = 0x41;
			}
		}

		if (is_lvds) {
			regp->TMDS[0x2] = 0x0;
		} else if (pll_setup_control == 0) {
			regp->TMDS[0x2] = 0x90;
		} else {
			if (nv_output->preferred_ramdac == 1) { /* This is the "output" */
				regp->TMDS[0x2] = 0xa9;
			} else {
				regp->TMDS[0x2] = 0x89;
			}
		}

		/* I assume they are zero for !is_lvds */
		if (is_lvds && pNv->Architecture == NV_ARCH_40) {
			/* Observed values are 0x11 and 0x14, TODO: this needs refinement */
			regp->TMDS[0x40] = 0x14;
			regp->TMDS[0x43] = 0xb0;
		}
	}

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

		ErrorF("%d: crtc %d ramdac %d twocrt %d twomon %d\n", is_fp, nv_crtc->crtc, nv_output->ramdac, two_crt, two_mon);
	}
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

	uint32_t output_reg[2];

	if ((nv_output->type == OUTPUT_LVDS) || (nv_output->type == OUTPUT_TMDS)) {
		is_fp = TRUE;
	}

	if (pNv->Architecture == NV_ARCH_40) {
		/* NV4x cards have strange ways of dealing with dualhead */
		/* Also see reg594 in nv_crtc.c */
		output_reg[0] = NV_RAMDAC_OUTPUT_DAC_ENABLE;
		/* So far only dual dvi cards(or lvds + dvi i think) seem to use (and need?) this */
		if (pNv->dual_dvi)
			output_reg[1] = NV_RAMDAC_OUTPUT_DAC_ENABLE;
	} else {
		if (!is_fp) {
			output_reg[nv_output->preferred_ramdac] = NV_RAMDAC_OUTPUT_DAC_ENABLE;
		} else { 
			output_reg[nv_output->preferred_ramdac] = 0x0;
		}
	}

	/* Only one can be on crtc1 */
	if (nv_crtc->head == 1) {
		output_reg[nv_output->preferred_ramdac] |= NV_RAMDAC_OUTPUT_SELECT_CRTC1;
	} else {
		output_reg[(~nv_output->preferred_ramdac) & 1] |= NV_RAMDAC_OUTPUT_SELECT_CRTC1;
	}

	if (pNv->Architecture == NV_ARCH_40) {
		/* The registers can't be considered seperately on nv40 */
		nvWriteRAMDAC(pNv, 0, NV_RAMDAC_OUTPUT, output_reg[0]);
		nvWriteRAMDAC(pNv, 1, NV_RAMDAC_OUTPUT, output_reg[1]);
	} else {
		nvWriteRAMDAC(pNv, nv_output->preferred_ramdac, NV_RAMDAC_OUTPUT, output_reg[nv_output->preferred_ramdac]);
	}

	/* This could use refinement for flatpanels, but it should work this way */
	if (pNv->NVArch < 0x44) {
		nvWriteRAMDAC(pNv, nv_output->preferred_ramdac, NV_RAMDAC_TEST_CONTROL, 0xf0000000);
		if (pNv->Architecture == NV_ARCH_40)
			nvWriteRAMDAC(pNv, 0, NV_RAMDAC_670, 0xf0000000);
	} else {
		nvWriteRAMDAC(pNv, nv_output->preferred_ramdac, NV_RAMDAC_TEST_CONTROL, 0x00100000);
		nvWriteRAMDAC(pNv, 0, NV_RAMDAC_670, 0x00100000);
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
	int ramdac;

	/* Usually these outputs are native to ramdac 1 */
	if (nv_output->valid_ramdac & RAMDAC_0 && nv_output->valid_ramdac & RAMDAC_1) {
		ramdac = 1;
	} else if (nv_output->valid_ramdac & RAMDAC_1) {
		ramdac = 1;
	} else if (nv_output->valid_ramdac & RAMDAC_0) {
		ramdac = 0;
	} else {
		return FALSE;
	}

	/* For some reason we get false positives on ramdac 1, maybe due tv-out? */
	if (ramdac == 1) {
		return FALSE;
	}

	if (nv_output->pDDCBus != NULL) {
		xf86MonPtr ddc_mon = xf86OutputGetEDID(output, nv_output->pDDCBus);
		/* Is there a digital flatpanel on this channel? */
		if (ddc_mon && ddc_mon->features.input_type) {
			return FALSE;
		}
	}

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

	present = (nvReadRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_CONTROL) & (1 << 28)) ? TRUE : FALSE;

	temp = NVOutputReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL);
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_CONTROL, temp & 0x000EFFF);

	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_OUTPUT, reg_output);
	nvWriteRAMDAC(pNv, ramdac, NV_RAMDAC_TEST_CONTROL, reg_test_ctrl);

	if (present) {
		ErrorF("A crt was detected on ramdac %d with no ddc support\n", ramdac);
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

static xf86OutputPtr
nv_find_output_and_clear_ramdac_from_outputs(xf86OutputPtr output, int ramdac)
{
	int i;
	ScrnInfoPtr pScrn = output->scrn;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	xf86OutputPtr output2 = NULL;
	NVOutputPrivatePtr nv_output2;
	for (i = 0; i < xf86_config->num_output; i++) {
		output2 = xf86_config->output[i];
		nv_output2 = output2->driver_private;
		if (nv_output2->ramdac == ramdac && output != output2) {
			nv_output2->ramdac = -1;
			break;
		}
	}

	return output2;
}

static void
nv_output_prepare(xf86OutputPtr output)
{
	ErrorF("nv_output_prepare is called\n");
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	Bool stole_ramdac = FALSE;
	xf86OutputPtr output2 = NULL;
	int ramdac;

	output->funcs->dpms(output, DPMSModeOff);

	/* We need ramdac 0, so let's steal it */
	if (!(nv_output->valid_ramdac & RAMDAC_1) && pNv->ramdac_active[0]) {
		ErrorF("Stealing ramdac0 ;-)\n");
		output2 = nv_find_output_and_clear_ramdac_from_outputs(output, 0);
		pNv->ramdac_active[0] = FALSE;
		stole_ramdac = TRUE;
	}

	/* We need ramdac 1, so let's steal it */
	if (!(nv_output->valid_ramdac & RAMDAC_0) && pNv->ramdac_active[1]) {
		ErrorF("Stealing ramdac1 ;-)\n");
		output2 = nv_find_output_and_clear_ramdac_from_outputs(output, 1);
		pNv->ramdac_active[1] = FALSE;
		stole_ramdac = TRUE;
	}

	/* TODO: figure out what ramdac 2 is and how it is identified */

	/* At this point we already stole ramdac 0 or 1 if we need it */
	if (!pNv->ramdac_active[0] && (nv_output->valid_ramdac & RAMDAC_0))
		ramdac = 0;
	else
		ramdac = 1;

	ErrorF("Activating ramdac %d\n", ramdac);
	pNv->ramdac_active[ramdac] = TRUE;
	nv_output->ramdac = ramdac;
	NVWriteVGACR5758(pNv, ramdac, 0, pNv->dcb_table.entry[nv_output->dcb_entry].type);
	NVWriteVGACR5758(pNv, ramdac, 2, pNv->dcb_table.entry[nv_output->dcb_entry].or);
	if (!stole_ramdac)
		nv_find_output_and_clear_ramdac_from_outputs(output, ramdac);

	if (stole_ramdac) {
		ErrorF("Resetting the stolen ramdac\n");
		DisplayModePtr adjusted_mode = xf86DuplicateMode(&(output2->crtc->desiredMode));
		xf86CrtcPtr crtc2 = output2->crtc;
		/* Assign a ramdac */
		output2->funcs->prepare(output2);
		/* We must set the vpll's to ensure they are properly set */
		crtc2->funcs->mode_fixup(crtc2, &(crtc2->desiredMode), adjusted_mode);
		crtc2->funcs->mode_set(crtc2, &(crtc2->desiredMode), adjusted_mode, crtc2->x, crtc2->y);
		output2->funcs->mode_set(output2, &(crtc2->desiredMode), adjusted_mode);
		/* Anyone know were this mode is stored, so we don't accidentally wake up a screen that is DPMSModeOff? */
		crtc2->funcs->dpms(crtc2, DPMSModeOn);
		output2->funcs->commit(output2);
		xfree(adjusted_mode);
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

#ifdef RANDR_12_INTERFACE
/*
 * Two scaling modes exist, let the user choose.
 */
#define SCALING_MODE_NAME "SCALING_MODE"
#define NUM_SCALING_METHODS 2
static char *scaling_mode_names[] = {
	"gpu",
	"panel",
};
static Atom scaling_mode_atom;

static int
nv_scaling_mode_lookup(char *name, int size)
{
	int i;
	const int len = strlen(name);

	for (i = 0; i < NUM_SCALING_METHODS; i++)
		if (size == len && !strncmp(name, scaling_mode_names[i], len))
			return i;

	return -1;
}

static void
nv_tmds_create_resources(xf86OutputPtr output)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	int error;

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

	error = RRChangeOutputProperty(output->randr_output, scaling_mode_atom,
					XA_STRING, 8, PropModeReplace, 
					strlen(scaling_mode_names[nv_output->scaling_mode]),
					scaling_mode_names[nv_output->scaling_mode],
					FALSE, TRUE);

	if (error != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Failed to set scaling mode, %d\n", error);
	}
}

static Bool
nv_tmds_set_property(xf86OutputPtr output, Atom property,
				RRPropertyValuePtr value)
{
	NVOutputPrivatePtr nv_output = output->driver_private;

	if (property == scaling_mode_atom) {
		int32_t ret;
		char *name;

		if (value->type != XA_STRING || value->format != 8)
			return FALSE;

		name = (char*) value->data;

		/* Match a string to a scaling mode */
		ret = nv_scaling_mode_lookup(name, value->size);
		if (ret < 0)
			return FALSE;

		nv_output->scaling_mode = ret;
		return TRUE;
	}

	return TRUE;
}

#endif /* RANDR_12_INTERFACE */

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
#ifdef RANDR_12_INTERFACE
	.create_resources = nv_tmds_create_resources,
	.set_property = nv_tmds_set_property,
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

	if (pNv->VBIOS.fp_native_mode || nv_ddc_detect(output))
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
	if (pNv->VBIOS.fp_native_mode == NULL)
		return NULL;

	nv_output->fpWidth = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_HDISP_END) + 1;
	nv_output->fpHeight = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_VDISP_END) + 1;
	nv_output->fpSyncs = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_CONTROL) & 0x30000033;

	if (pNv->VBIOS.fp_native_mode->HDisplay != nv_output->fpWidth ||
		pNv->VBIOS.fp_native_mode->VDisplay != nv_output->fpHeight) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"Panel size mismatch; ignoring RAMDAC\n");
		nv_output->fpWidth = pNv->VBIOS.fp_native_mode->HDisplay;
		nv_output->fpHeight = pNv->VBIOS.fp_native_mode->VDisplay;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Panel size is %u x %u\n",
		nv_output->fpWidth, nv_output->fpHeight);

	nv_output->native_mode = xf86DuplicateMode(pNv->VBIOS.fp_native_mode);

	return xf86DuplicateMode(pNv->VBIOS.fp_native_mode);
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

	/* order:
	 * bit0: RAMDAC_0 valid
	 * bit1: RAMDAC_1 valid
	 * So lowest order has highest priority.
	 * Below is guesswork:
	 * bit2: All ramdac's valid?
	 * FIXME: this probably wrong
	 */
	nv_output->valid_ramdac = ffs(pNv->dcb_table.entry[dcb_entry].or);

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

	/* This is only to facilitate proper output routing for dvi */
	/* See sel_clk assignment in nv_crtc.c */
	if (nv_output->valid_ramdac & RAMDAC_1) {
		nv_output->preferred_ramdac = 1;
	} else {
		nv_output->preferred_ramdac = 0;
	}

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

	/* order:
	 * bit0: RAMDAC_0 valid
	 * bit1: RAMDAC_1 valid
	 * So lowest order has highest priority.
	 * Below is guesswork:
	 * bit2: All ramdac's valid?
	 * FIXME: this probably wrong
	 */
	nv_output->valid_ramdac = ffs(pNv->dcb_table.entry[dcb_entry].or);

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

	/* This is only to facilitate proper output routing for dvi */
	/* See sel_clk assignment in nv_crtc.c */
	if (nv_output->valid_ramdac & RAMDAC_1) {
		nv_output->preferred_ramdac = 1;
	} else {
		nv_output->preferred_ramdac = 0;
	}

	if (pNv->fpScaler) {
		/* Aspect ratio */
		nv_output->scaling_mode = 0;
	} else {
		/* "Panel mode" fully filled */
		nv_output->scaling_mode = 1;
	}

	output->possible_crtcs = pNv->dcb_table.entry[dcb_entry].heads;

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Adding output %s\n", outputname);
}

void NvDCBSetupOutputs(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int i, type, bus_count[0xf], digital_counter = 0;

	memset(bus_count, 0, sizeof(bus_count));
	for (i = 0 ; i < pNv->dcb_table.entries; i++)
		bus_count[pNv->dcb_table.entry[i].bus]++;

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
			nv_add_analog_output(pScrn, i, (bus_count[pNv->dcb_table.entry[i].bus] > 1));
			break;
		case OUTPUT_TMDS:
			nv_add_digital_output(pScrn, i, 0);
			digital_counter++;
			break;
		case OUTPUT_LVDS:
			nv_add_digital_output(pScrn, i, 1);
			/* I'm assuming that lvds+dvi has the same effect as dual dvi */
			digital_counter++;
			break;
		default:
			break;
		}
	}

	if (digital_counter > 1) {
		pNv->dual_dvi = TRUE;
	} else {
		pNv->dual_dvi = FALSE;
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
