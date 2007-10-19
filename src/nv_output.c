/*
 * Copyright 2006 Dave Airlie
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
 *
 * Authors:
 *  Dave Airlie
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
	ramdac = nv_output->prefered_ramdac;

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
	ramdac = nv_output->prefered_ramdac;

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
    ScrnInfoPtr	pScrn = output->scrn;
    NVPtr pNv = NVPTR(pScrn);   

    /* This is done differently on each laptop.  Here we
       define the ones we know for sure. */
    
#if defined(__powerpc__)
    if((pNv->Chipset == 0x10DE0179) || 
       (pNv->Chipset == 0x10DE0189) || 
       (pNv->Chipset == 0x10DE0329))
    {
	/* NV17,18,34 Apple iMac, iBook, PowerBook */
	CARD32 tmp_pmc, tmp_pcrt;
	tmp_pmc = nvReadMC(pNv, 0x10F0) & 0x7FFFFFFF;
	tmp_pcrt = nvReadCRTC0(pNv, NV_CRTC_081C) & 0xFFFFFFFC;
	if(on) {
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
nv_panel_output_dpms(xf86OutputPtr output, int mode)
{

    switch(mode) {
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

	if (crtc) {
		NVPtr pNv = NVPTR(output->scrn);
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		ErrorF("nv_analog_output_dpms is called for CRTC %d with mode %d\n", nv_crtc->crtc, mode);
	}
}

static void
nv_digital_output_dpms(xf86OutputPtr output, int mode)
{
	xf86CrtcPtr crtc = output->crtc;
	NVOutputPrivatePtr nv_output = output->driver_private;

	/* Are we assigned a ramdac already?, else we will be activeted during mode set */
	if (crtc && nv_output->ramdac_assigned) {
		NVPtr pNv = NVPTR(output->scrn);
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		ErrorF("nv_digital_output_dpms is called for CRTC %d with mode %d\n", nv_crtc->crtc, mode);

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

int tmds_regs[] = { 0x4 };

void nv_output_save_state_ext(xf86OutputPtr output, RIVA_HW_STATE *state)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputRegPtr regp;
	int i;

	regp = &state->dac_reg[nv_output->ramdac];
	regp->general       = NVOutputReadRAMDAC(output, NV_RAMDAC_GENERAL_CONTROL);
	regp->fp_control    = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_CONTROL);
	regp->debug_0	= NVOutputReadRAMDAC(output, NV_RAMDAC_FP_DEBUG_0);
	regp->debug_1	= NVOutputReadRAMDAC(output, NV_RAMDAC_FP_DEBUG_1);
	regp->debug_2	= NVOutputReadRAMDAC(output, NV_RAMDAC_FP_DEBUG_2);
	state->config       = nvReadFB(pNv, NV_PFB_CFG0);

	regp->output = NVOutputReadRAMDAC(output, NV_RAMDAC_OUTPUT);

	if ((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
		regp->dither = NVOutputReadRAMDAC(output, NV_RAMDAC_DITHER_NV11);
	} else if (pNv->twoHeads) {
		regp->dither = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_DITHER);
	}
	regp->nv10_cursync = NVOutputReadRAMDAC(output, NV_RAMDAC_NV10_CURSYNC);

	for (i = 0; i < sizeof(tmds_regs)/sizeof(tmds_regs[0]); i++) {
		regp->TMDS[tmds_regs[i]] = NVOutputReadTMDS(output, tmds_regs[i]);
	}

	if (nv_output->type == OUTPUT_DIGITAL) {

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
}

void nv_output_load_state_ext(xf86OutputPtr output, RIVA_HW_STATE *state)
{
	NVOutputPrivatePtr nv_output = output->driver_private;
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVOutputRegPtr regp;
	int i;

	regp = &state->dac_reg[nv_output->ramdac];

	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_DEBUG_0, regp->debug_0);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_DEBUG_1, regp->debug_1);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_DEBUG_2, regp->debug_2);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_OUTPUT, regp->output);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_CONTROL, regp->fp_control);

	if ((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
		NVOutputWriteRAMDAC(output, NV_RAMDAC_DITHER_NV11, regp->dither);
	} else if (pNv->twoHeads) {
		NVOutputWriteRAMDAC(output, NV_RAMDAC_FP_DITHER, regp->dither);
	}

	NVOutputWriteRAMDAC(output, NV_RAMDAC_GENERAL_CONTROL, regp->general);
	NVOutputWriteRAMDAC(output, NV_RAMDAC_NV10_CURSYNC, regp->nv10_cursync);

	for (i = 0; i < sizeof(tmds_regs)/sizeof(tmds_regs[0]); i++) {
		NVOutputWriteTMDS(output, tmds_regs[i], regp->TMDS[tmds_regs[i]]);
	}

	if (nv_output->type == OUTPUT_DIGITAL) {

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

	
}


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
	nv_output->ramdac = nv_output->prefered_ramdac;

	state = &pNv->SavedReg;

	nv_output_save_state_ext(output, state);

	/* restore previous state */
	nv_output->ramdac = ramdac_backup;
}

static void
nv_output_restore (xf86OutputPtr output)
{
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state;
	NVOutputPrivatePtr nv_output = output->driver_private;
	int ramdac_backup = nv_output->ramdac;

	ErrorF("nv_output_restore is called\n");

	/* We want consistent mode restoring and the ramdac entry is variable */
	nv_output->ramdac = nv_output->prefered_ramdac;

	state = &pNv->SavedReg;

	nv_output_load_state_ext(output, state);

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
nv_output_mode_set_regs(xf86OutputPtr output, DisplayModePtr mode)
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

	if ((nv_output->type == OUTPUT_PANEL) || (nv_output->type == OUTPUT_DIGITAL)) {
		is_fp = TRUE;

		regp->fp_horiz_regs[REG_DISP_END] = mode->CrtcHDisplay - 1;
		regp->fp_horiz_regs[REG_DISP_TOTAL] = mode->CrtcHTotal - 1;
		regp->fp_horiz_regs[REG_DISP_CRTC] = mode->CrtcHDisplay;
		regp->fp_horiz_regs[REG_DISP_SYNC_START] = mode->CrtcHSyncStart - 1;
		regp->fp_horiz_regs[REG_DISP_SYNC_END] = mode->CrtcHSyncEnd - 1;
		regp->fp_horiz_regs[REG_DISP_VALID_START] = mode->CrtcHSkew;
		regp->fp_horiz_regs[REG_DISP_VALID_END] = mode->CrtcHDisplay - 1;

		regp->fp_vert_regs[REG_DISP_END] = mode->CrtcVDisplay - 1;
		regp->fp_vert_regs[REG_DISP_TOTAL] = mode->CrtcVTotal - 1;
		regp->fp_vert_regs[REG_DISP_CRTC] = mode->CrtcVDisplay;
		regp->fp_vert_regs[REG_DISP_SYNC_START] = mode->CrtcVSyncStart - 1;
		regp->fp_vert_regs[REG_DISP_SYNC_END] = mode->CrtcVSyncEnd - 1;
		regp->fp_vert_regs[REG_DISP_VALID_START] = 0;
		regp->fp_vert_regs[REG_DISP_VALID_END] = mode->CrtcVDisplay - 1;

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
	* This must also be set for non-flatpanels
	*/
	regp->fp_control = 0x11100000;

	/* Deal with vsync/hsync ploarity */
	if (mode->Flags & V_PVSYNC) {
		regp->fp_control |= (1 << 0);
	}

	if (mode->Flags & V_PHSYNC) {
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

		/* Enable full width  and height on the flat panel */
		regp->fp_hvalid_start = 0;
		regp->fp_hvalid_end = (nv_output->fpWidth - 1);

		regp->fp_vvalid_start = 0;
		regp->fp_vvalid_end = (nv_output->fpHeight - 1);

		/* When doing vertical scaling, limit the last fetched line */
		if (v_scale != (1 << 12)) {
			regp->debug_2 = (1 << 28) | ((mode->VDisplay - 1) << 16);
		} else {
			regp->debug_2 = 0;
		}

		/* We want automatic scaling */
		regp->debug_1 = 0;

		/* GPU scaling happens automaticly at a ratio of 1:33 */
		/* A 1280x1024 panel has a ratio of 1:25, we don't want to scale that at 4:3 resolutions */
		if (h_scale != (1 << 12) && (panel_ratio > (aspect_ratio + 0.10))) {
			uint32_t diff;

			ErrorF("Scaling resolution on a widescreen panel\n");

			/* Scaling in both directions needs to the same */
			h_scale = v_scale;
			diff = nv_output->fpWidth - ((1 << 12) * mode->HDisplay)/h_scale;
			regp->fp_hvalid_start = diff/2;
			regp->fp_hvalid_end = nv_output->fpWidth - (diff/2) - 1;
		}

		/* Same scaling, just for panels with aspect ratio's smaller than 1 */
		if (v_scale != (1 << 12) && (panel_ratio < (aspect_ratio - 0.10))) {
			uint32_t diff;

			ErrorF("Scaling resolution on a portrait panel\n");

			/* Scaling in both directions needs to the same */
			v_scale = h_scale;
			diff = nv_output->fpHeight - ((1 << 12) * mode->VDisplay)/v_scale;
			regp->fp_vvalid_start = diff/2;
			regp->fp_vvalid_end = nv_output->fpHeight - (diff/2) - 1;
		}

		ErrorF("Post-panel scaling\n");
	} else {
		/* copy a bunch of things from the current state for non-dfp's */
		regp->debug_2 = savep->debug_2;
	}

	if (pNv->Architecture >= NV_ARCH_10) {
		/* Bios and blob don't seem to do anything (else) */
		regp->nv10_cursync = (1<<25);
	}

	/* These are the common blob values, minus a few fp specific bit's */
	/* The OR mask is in case the powerdown switch was enabled from the other output */
	regp->debug_0 |= 0x1101111;
	regp->debug_1 = savep->debug_1;
	if(is_fp) {
		/* I am not completely certain, but seems to be set only for dfp's */
		regp->debug_0 |= NV_RAMDAC_FP_DEBUG_0_TMDS_ENABLED;
	}

	/* We must ensure that we never disable the wrong tmds control */
	if (nv_output->ramdac != nv_output->prefered_ramdac) {
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
		/* Restricting to cards that had this setup at bootup time, until i am certain it's ok to use */
		if (nv_output->ramdac != nv_output->prefered_ramdac && nv_output->ramdac == 0
			&& pNv->output_info & OUTPUT_1_CROSSWIRED_TMDS) {
			regp->TMDS[0x4] |= (1 << 3);
		}
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

	regp->general = bpp == 16 ? 0x00101100 : 0x00100100;

	if (pNv->alphaCursor) {
		regp->general |= (1<<29);
	}

	if(bpp != 8) {/* DirectColor */
		regp->general |= 0x00000030;
	}

	regp->bpp = bpp;    /* this is not bitsPerPixel, it's 8,15,16,32 */

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
			regp->output |= NV_RAMDAC_OUTPUT_SELECT_CRTC1;
		} else {
			regp->output &= ~NV_RAMDAC_OUTPUT_SELECT_CRTC1;
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

    nv_output_mode_set_regs(output, mode);
    nv_output_load_state_ext(output, state);
}

static Bool
nv_ddc_detect(xf86OutputPtr output)
{
	/* no use for shared DDC output */
	NVOutputPrivatePtr nv_output = output->driver_private;
	xf86MonPtr ddc_mon;

	ddc_mon = xf86OutputGetEDID(output, nv_output->pDDCBus);
	if (!ddc_mon)
		return FALSE;

	if (ddc_mon->features.input_type && (nv_output->type == OUTPUT_ANALOG))
		return FALSE;

	if ((!ddc_mon->features.input_type) && (nv_output->type == OUTPUT_DIGITAL)) {
		return FALSE;
	}

	xf86PrintEDID(ddc_mon);

	if (nv_output->type == OUTPUT_DIGITAL) {
		int i;
		for (i = 0; i < 8; i++) {
			/* Selecting only based on width ok? */
			if (ddc_mon->timings2[i].hsize > nv_output->fpWidth) {
				nv_output->fpWidth = ddc_mon->timings2[i].hsize;
				nv_output->fpHeight = ddc_mon->timings2[i].vsize;
			}
		}
	}

	return TRUE;
}

static Bool
nv_crt_load_detect(xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    CARD32 reg_output, reg_test_ctrl, temp;
    int present = FALSE;
	  
    reg_output = NVOutputReadRAMDAC(output, NV_RAMDAC_OUTPUT);
    reg_test_ctrl = NVOutputReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL);

    NVOutputWriteRAMDAC(output, NV_RAMDAC_TEST_CONTROL, (reg_test_ctrl & ~0x00010000));
    
    NVOutputWriteRAMDAC(output, NV_RAMDAC_OUTPUT, (reg_output & 0x0000FEEE));
    usleep(1000);
	  
    temp = NVOutputReadRAMDAC(output, NV_RAMDAC_OUTPUT);
    NVOutputWriteRAMDAC(output, NV_RAMDAC_OUTPUT, temp | 1);

    NVOutputWriteRAMDAC(output, NV_RAMDAC_TEST_DATA, 0x94050140);
    temp = NVOutputReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL);
    NVOutputWriteRAMDAC(output, NV_RAMDAC_TEST_CONTROL, temp | 0x1000);

    usleep(1000);
	  
    present = (NVOutputReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL) & (1 << 28)) ? TRUE : FALSE;
	  
    temp = NVOutputReadRAMDAC(output, NV_RAMDAC_TEST_CONTROL);
    NVOutputWriteRAMDAC(output, NV_RAMDAC_TEST_CONTROL, temp & 0x000EFFF);
	  
    NVOutputWriteRAMDAC(output, NV_RAMDAC_OUTPUT, reg_output);
    NVOutputWriteRAMDAC(output, NV_RAMDAC_TEST_CONTROL, reg_test_ctrl);
    
    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "CRT detect returned %d\n",
	       present);

    return present;

}

static xf86OutputStatus
nv_digital_output_detect(xf86OutputPtr output)
{
    NVOutputPrivatePtr nv_output = output->driver_private;

	ErrorF("nv_digital_output_detect is called\n");

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

	/* Disabled for now, since we don't actually have a ramdac yet and i need time to figure out a nice way to do this ;-) */
	/* seems a bit flaky on ramdac 1 */
	//if ((nv_output->ramdac==0) && nv_crt_load_detect(output))
		//return XF86OutputStatusConnected;

    return XF86OutputStatusDisconnected;
}

static DisplayModePtr
nv_output_get_modes(xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    NVOutputPrivatePtr nv_output = output->driver_private;
    xf86MonPtr ddc_mon;
    DisplayModePtr ddc_modes, mode;
    int i;

	ErrorF("nv_output_get_modes is called\n");

    ddc_mon = xf86OutputGetEDID(output, nv_output->pDDCBus);

    if (ddc_mon == NULL) {
	xf86OutputSetEDID(output, ddc_mon);
	return NULL;
    }

    if (ddc_mon->features.input_type && (nv_output->type == OUTPUT_ANALOG)) {
	xf86OutputSetEDID(output, NULL);
	return NULL;
    }

    if ((!ddc_mon->features.input_type) && (nv_output->type == OUTPUT_DIGITAL)) {
	xf86OutputSetEDID(output, NULL);
	return NULL;
    }

    xf86OutputSetEDID(output, ddc_mon);

    ddc_modes = xf86OutputGetEDIDModes (output);	  
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
	ScrnInfoPtr	pScrn = output->scrn;
	NVPtr pNv = NVPTR(pScrn);

	if (nv_output->ramdac_assigned) {
		return;
	}

	/* This may cause problems if later on ramdac 0 is disabled */
	/* Select ramdac */
	if (pNv->ramdac_count) {
		pNv->ramdac_count = 2;
		nv_output->ramdac = 1;
	} else {
		pNv->ramdac_count = 1;
		nv_output->ramdac = 0;
	}

	nv_output->ramdac_assigned = TRUE;
}

static void
nv_output_commit(xf86OutputPtr output)
{
	ErrorF("nv_output_commit is called\n");
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

static const xf86OutputFuncsRec nv_digital_output_funcs = {
    .dpms = nv_digital_output_dpms,
    .save = nv_output_save,
    .restore = nv_output_restore,
    .mode_valid = nv_output_mode_valid,
    .mode_fixup = nv_output_mode_fixup,
    .mode_set = nv_output_mode_set,
    .detect = nv_digital_output_detect,
    .get_modes = nv_output_get_modes,
    .destroy = nv_output_destroy,
    .prepare = nv_output_prepare,
    .commit = nv_output_commit,
};

static xf86OutputStatus
nv_output_lvds_detect(xf86OutputPtr output)
{
    return XF86OutputStatusUnknown;    
}

static DisplayModePtr
nv_output_lvds_get_modes(xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    NVOutputPrivatePtr nv_output = output->driver_private;

    //    nv_output->fpWidth = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_HDISP_END) + 1;
    //    nv_output->fpHeight = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_VDISP_END) + 1;
    nv_output->fpSyncs = NVOutputReadRAMDAC(output, NV_RAMDAC_FP_CONTROL) & 0x30000033;
    //    xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Panel size is %i x %i\n",
    //	       nv_output->fpWidth, nv_output->fpHeight);

    return NULL;

}

static const xf86OutputFuncsRec nv_lvds_output_funcs = {
    .dpms = nv_panel_output_dpms,
    .save = nv_output_save,
    .restore = nv_output_restore,
    .mode_valid = nv_output_mode_valid,
    .mode_fixup = nv_output_mode_fixup,
    .mode_set = nv_output_mode_set,
    .detect = nv_output_lvds_detect,
    .get_modes = nv_output_lvds_get_modes,
    .destroy = nv_output_destroy,
    .prepare = nv_output_prepare,
    .commit = nv_output_commit,
};


static void nv_add_analog_output(ScrnInfoPtr pScrn, int index, int i2c_index, Bool dvi_pair)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86OutputPtr	    output;
	NVOutputPrivatePtr    nv_output;
	char outputname[20];
	int crtc_mask = (1<<0) | (1<<1);
	int real_index;

	sprintf(outputname, "Analog-%d", pNv->analog_count);
	output = xf86OutputCreate (pScrn, &nv_analog_output_funcs, outputname);
	if (!output)
		return;
	nv_output = xnfcalloc (sizeof (NVOutputPrivateRec), 1);
	if (!nv_output) {
		xf86OutputDestroy (output);
		return;
	}

	output->driver_private = nv_output;
	nv_output->type = OUTPUT_ANALOG;

	/* dvi outputs share their i2c port with their analog output on the same port */
	/* But they can never work at the same time, so it's convient to share ramdac index */
	nv_output->prefered_ramdac = index;

	/* Is anyone crosswired?, pick the other index */
	if (pNv->output_info & OUTPUT_0_CROSSWIRED_TMDS || 
		pNv->output_info & OUTPUT_1_CROSSWIRED_TMDS) {

		real_index = (~index) & 1; 
	} else {
		real_index = index;
	}

	nv_output->pDDCBus = pNv->pI2CBus[real_index];

	output->possible_crtcs = crtc_mask;
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Adding output %s\n", outputname);

	pNv->analog_count++;
}


static void nv_add_digital_output(ScrnInfoPtr pScrn, int index, int i2c_index, Bool dual_dvi, int lvds)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86OutputPtr	    output;
	NVOutputPrivatePtr    nv_output;
	char outputname[20];
	int crtc_mask = (1<<0) | (1<<1);

	sprintf(outputname, "Digital-%d", pNv->digital_count);
	if (lvds)
		output = xf86OutputCreate (pScrn, &nv_lvds_output_funcs, outputname);
	else
		output = xf86OutputCreate (pScrn, &nv_digital_output_funcs, outputname);
	if (!output)
		return;
	nv_output = xnfcalloc (sizeof (NVOutputPrivateRec), 1);
	if (!nv_output) {
		xf86OutputDestroy (output);
		return;
	}

	output->driver_private = nv_output;
	nv_output->type = OUTPUT_DIGITAL;

	nv_output->prefered_ramdac = index;

	/* Sorry, i don't know what to do with lvds */
	if (!lvds) {
		int real_index;
		/* Is anyone crosswired?, pick the other index */
		if (pNv->output_info & OUTPUT_0_CROSSWIRED_TMDS || 
			pNv->output_info & OUTPUT_1_CROSSWIRED_TMDS) {

			real_index = (~index) & 1; 
		} else {
			real_index = index;
		}
		/* This is a suspusion, please do tell if this causes problems for anyone */
		nv_output->pDDCBus = pNv->pI2CBus[real_index];
		/* At the moment something must be already active, before we do anything */
		if (nvReadRAMDAC(pNv,  real_index, NV_RAMDAC_FP_DEBUG_0) & NV_RAMDAC_FP_DEBUG_0_TMDS_ENABLED) {
			/* We're not supposed to be LVDS */
			/* Using index, because this is part of the TMDS programming */
			if (pNv->output_info & (OUTPUT_0_LVDS << index)) {
				xfree(nv_output);
				xf86OutputDestroy (output);
				return;
			}
			/* we should be slaved to a ramdac, otherwise we don't exist */
			if (!(pNv->output_info & (OUTPUT_0_SLAVED << real_index))) {
				xfree(nv_output);
				xf86OutputDestroy (output);
				return;
			}
		} else {
			xfree(nv_output);
			xf86OutputDestroy (output);
			return;
		}
	} else {
		xfree(nv_output);
		xf86OutputDestroy (output);
		return;
	}

	output->possible_crtcs = crtc_mask;
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Adding output %s\n", outputname);

	pNv->digital_count++;
}
/**
 * Set up the outputs according to what type of chip we are.
 *
 * Some outputs may not initialize, due to allocation failure or because a
 * controller chip isn't found.
 */

void Nv20SetupOutputs(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    xf86OutputPtr	    output;
    NVOutputPrivatePtr    nv_output;
    int i;
    int num_analog_outputs = pNv->twoHeads ? 2 : 1;
    int num_digital_outputs = 1;

    for (i = 0 ; i < num_analog_outputs; i++) {
      nv_add_analog_output(pScrn, i, i, FALSE);
    }

    for (i = 0 ; i < num_digital_outputs; i++) {
      nv_add_digital_output(pScrn, i, i, FALSE, 0);
    }
}

void NvDCBSetupOutputs(ScrnInfoPtr pScrn)
{
	unsigned char type, port, or, i2c_index;
	NVPtr pNv = NVPTR(pScrn);
	int i;
	int num_digital = 0;
	Bool dual_dvi = FALSE;
	Bool dvi_pair = FALSE;

	/* check how many TMDS ports there are */
	if (pNv->dcb_entries) {
		for (i = 0 ; i < pNv->dcb_entries; i++) {
			type = pNv->dcb_table[i] & 0xf;
			port = (pNv->dcb_table[i] >> 4) & 0xf;
			/* TMDS */
			if (type == 2 && port != 0xf) {
				num_digital++;
			}
		}
	}

	if (num_digital > 1) {
		dual_dvi = TRUE;
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
	if (pNv->dcb_entries) {
		for (i = 0 ; i < pNv->dcb_entries; i++) {
			type = pNv->dcb_table[i] & 0xf;
			port = (pNv->dcb_table[i] >> 4) & 0xf;
			or = ffs((pNv->dcb_table[i] >> 24) & 0xf) - 1;
			if (pNv->Architecture == NV_ARCH_40) {
				/* Ports seem to be inverse on nv4x */
				i2c_index = (~port) & 1;
			} else {
				i2c_index = port;
			}

			if (type < 4)
				xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "DCB entry: %d: %08X type: %d, port %d:, or %d\n", i, pNv->dcb_table[i], type, port, or);
			/* I have no clue what to do with anything but port 0 and 1 */
			if (type < 4 && port < 2) {
				switch(type) {
				case 0: /* Analog */
					nv_add_analog_output(pScrn, port, i2c_index, dvi_pair);
					dvi_pair = FALSE;
					break;
				case 2: /* TMDS */
					dvi_pair = TRUE;
					nv_add_digital_output(pScrn, port, i2c_index, dual_dvi, 0);
					break;
				case 3: /* LVDS */
					nv_add_digital_output(pScrn, port, i2c_index, dual_dvi, 1);
					break;
				default:
					break;
				}
			}
		}
	} else {
		Nv20SetupOutputs(pScrn);
	}
}

struct nv_i2c_struct {
	int reg;
	char *name;
} nv_i2c_buses[] = { 
	{ 0x3e, "DDC1" },
	{ 0x36, "DDC2" },
	{ 0x50, "TV" },
};

void NvSetupOutputs(ScrnInfoPtr pScrn)
{
    int i;
    NVPtr pNv = NVPTR(pScrn);
    xf86OutputPtr	    output;
    NVOutputPrivatePtr    nv_output;

    int num_outputs = pNv->twoHeads ? 2 : 1;
    char outputname[20];
    pNv->Television = FALSE;

	/* add the 3 I2C buses */
	for (i = 0; i < NV_I2C_BUSES; i++) {
		NV_I2CInit(pScrn, &pNv->pI2CBus[i], nv_i2c_buses[i].reg, nv_i2c_buses[i].name);
	}

    NvDCBSetupOutputs(pScrn);

#if 0
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
