/*
 * Copyright 1993-2003 NVIDIA, Corporation
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

#include "nv_include.h"

static void
crtc_wr_cio_state(xf86CrtcPtr crtc, NVCrtcRegPtr crtcstate, int index)
{
	NVWriteVgaCrtc(NVPTR(crtc->scrn), to_nouveau_crtc(crtc)->head, index,
		       crtcstate->CRTC[index]);
}

void nv_crtc_set_digital_vibrance(xf86CrtcPtr crtc, int level)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(crtc->scrn);
	NVCrtcRegPtr regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	regp->CRTC[NV_CIO_CRE_CSB] = nv_crtc->saturation = level;
	if (nv_crtc->saturation && pNv->gf4_disp_arch) {
		regp->CRTC[NV_CIO_CRE_CSB] = 0x80;
		regp->CRTC[NV_CIO_CRE_5B] = nv_crtc->saturation << 2;
		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_5B);
	}
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_CSB);
}

void nv_crtc_set_image_sharpening(xf86CrtcPtr crtc, int level)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(crtc->scrn);
	NVCrtcRegPtr regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	nv_crtc->sharpness = level;
	if (level < 0)	/* blur is in hw range 0x3f -> 0x20 */
		level += 0x40;
	regp->ramdac_634 = level;
	NVWriteRAMDAC(pNv, nv_crtc->head, NV_PRAMDAC_634, regp->ramdac_634);
}

/* NV4x 0x40.. pll notes:
 * gpu pll: 0x4000 + 0x4004
 * ?gpu? pll: 0x4008 + 0x400c
 * vpll1: 0x4010 + 0x4014
 * vpll2: 0x4018 + 0x401c
 * mpll: 0x4020 + 0x4024
 * mpll: 0x4038 + 0x403c
 *
 * the first register of each pair has some unknown details:
 * bits 0-7: redirected values from elsewhere? (similar to PLL_SETUP_CONTROL?)
 * bits 20-23: (mpll) something to do with post divider?
 * bits 28-31: related to single stage mode? (bit 8/12)
 */

static void nv_crtc_cursor_set(xf86CrtcPtr crtc)
{
	NVPtr pNv = NVPTR(crtc->scrn);
	int head = to_nouveau_crtc(crtc)->head;
	NVCrtcRegPtr regp = &pNv->ModeReg.crtc_reg[head];
	uint32_t cursor_start = head ? pNv->Cursor2->offset :
				       pNv->Cursor->offset;

	regp->CRTC[NV_CIO_CRE_HCUR_ADDR0_INDEX] = MASK(NV_CIO_CRE_HCUR_ASI) |
						  XLATE(cursor_start, 17,
							NV_CIO_CRE_HCUR_ADDR0_ADR);
	regp->CRTC[NV_CIO_CRE_HCUR_ADDR1_INDEX] = XLATE(cursor_start, 11,
							NV_CIO_CRE_HCUR_ADDR1_ADR);
	if (crtc->mode.Flags & V_DBLSCAN)
		regp->CRTC[NV_CIO_CRE_HCUR_ADDR1_INDEX] |= MASK(NV_CIO_CRE_HCUR_ADDR1_CUR_DBL);
	regp->CRTC[NV_CIO_CRE_HCUR_ADDR2_INDEX] = cursor_start >> 24;

	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR0_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR1_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR2_INDEX);
	if (pNv->Architecture == NV_ARCH_40)
		nv_fix_nv40_hw_cursor(pNv, head);
}

static void nv_crtc_calc_state_ext(xf86CrtcPtr crtc, DisplayModePtr mode, int dot_clock)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	RIVA_HW_STATE *state = &pNv->ModeReg;
	NVCrtcRegPtr regp = &state->crtc_reg[nv_crtc->head];
	struct nouveau_pll_vals *pv = &regp->pllvals;
	struct pll_lims pll_lim;
	int vclk, arb_burst, arb_fifo_lwm;

	if (get_pll_limits(pScrn, nv_crtc->head ? VPLL2 : VPLL1, &pll_lim))
		return;

	/* NM2 == 0 is used to determine single stage mode on two stage plls */
	pv->NM2 = 0;

	/* for newer nv4x the blob uses only the first stage of the vpll below a
	 * certain clock.  for a certain nv4b this is 150MHz.  since the max
	 * output frequency of the first stage for this card is 300MHz, it is
	 * assumed the threshold is given by vco1 maxfreq/2
	 */
	/* for early nv4x, specifically nv40 and *some* nv43 (devids 0 and 6,
	 * not 8, others unknown), the blob always uses both plls.  no problem
	 * has yet been observed in allowing the use a single stage pll on all
	 * nv43 however.  the behaviour of single stage use is untested on nv40
	 */
	if (pNv->NVArch > 0x40 && dot_clock <= (pll_lim.vco1.maxfreq / 2))
		memset(&pll_lim.vco2, 0, sizeof(pll_lim.vco2));

	if (!(vclk = nouveau_calc_pll_mnp(pScrn, &pll_lim, dot_clock, pv)))
		return;

	/* The blob uses this always, so let's do the same */
	if (pNv->Architecture == NV_ARCH_40)
		state->pllsel |= NV_RAMDAC_PLL_SELECT_USE_VPLL2_TRUE;
	/* again nv40 and some nv43 act more like nv3x as described above */
	if (pNv->NVArch < 0x41)
		state->pllsel |= NV_PRAMDAC_PLL_COEFF_SELECT_SOURCE_PROG_MPLL |
				 NV_PRAMDAC_PLL_COEFF_SELECT_SOURCE_PROG_NVPLL;
	state->pllsel |= (nv_crtc->head ? NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL2 |
					  NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2 :
					  NV_PRAMDAC_PLL_COEFF_SELECT_SOURCE_PROG_VPLL |
					  NV_PRAMDAC_PLL_COEFF_SELECT_VCLK_RATIO_DB2);

	if (pv->NM2)
		NV_TRACE(pScrn, "vpll: n1 %d n2 %d m1 %d m2 %d log2p %d\n",
			 pv->N1, pv->N2, pv->M1, pv->M2, pv->log2P);
	else
		NV_TRACE(pScrn, "vpll: n %d m %d log2p %d\n",
			 pv->N1, pv->M1, pv->log2P);

	nouveau_calc_arb(pScrn, vclk, pScrn->bitsPerPixel, &arb_burst, &arb_fifo_lwm);

	regp->CRTC[NV_CIO_CRE_FF_INDEX] = arb_burst;
	regp->CRTC[NV_CIO_CRE_FFLWM__INDEX] = arb_fifo_lwm & 0xff;
	if (pNv->Architecture >= NV_ARCH_30)
		regp->CRTC[NV_CIO_CRE_47] = arb_fifo_lwm >> 8;

	nv_crtc_cursor_set(crtc);
}

static void
nv_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	unsigned char seq1 = 0, crtc17 = 0;
	unsigned char crtc1A;

	NV_TRACE(pScrn, "Setting dpms mode %d on CRTC %d\n", mode, nv_crtc->head);

	if (nv_crtc->last_dpms == mode) /* Don't do unnecesary mode changes. */
		return;

	nv_crtc->last_dpms = mode;

	if (pNv->twoHeads)
		NVSetOwner(pNv, nv_crtc->head);

	/* nv4ref indicates these two RPC1 bits inhibit h/v sync */
	crtc1A = NVReadVgaCrtc(pNv, nv_crtc->head, NV_CIO_CRE_RPC1_INDEX) & ~0xC0;
	switch(mode) {
	case DPMSModeStandby:
		/* Screen: Off; HSync: Off, VSync: On -- Not Supported */
		seq1 = 0x20;
		crtc17 = 0x80;
		crtc1A |= 0x80;
		break;
	case DPMSModeSuspend:
		/* Screen: Off; HSync: On, VSync: Off -- Not Supported */
		seq1 = 0x20;
		crtc17 = 0x80;
		crtc1A |= 0x40;
		break;
	case DPMSModeOff:
		/* Screen: Off; HSync: Off, VSync: Off */
		seq1 = 0x20;
		crtc17 = 0x00;
		crtc1A |= 0xC0;
		break;
	case DPMSModeOn:
	default:
		/* Screen: On; HSync: On, VSync: On */
		seq1 = 0x00;
		crtc17 = 0x80;
		break;
	}

	NVVgaSeqReset(pNv, nv_crtc->head, true);
	/* Each head has it's own sequencer, so we can turn it off when we want */
	seq1 |= (NVReadVgaSeq(pNv, nv_crtc->head, NV_VIO_SR_CLOCK_INDEX) & ~0x20);
	NVWriteVgaSeq(pNv, nv_crtc->head, NV_VIO_SR_CLOCK_INDEX, seq1);
	crtc17 |= (NVReadVgaCrtc(pNv, nv_crtc->head, NV_CIO_CR_MODE_INDEX) & ~0x80);
	usleep(10000);
	NVWriteVgaCrtc(pNv, nv_crtc->head, NV_CIO_CR_MODE_INDEX, crtc17);
	NVVgaSeqReset(pNv, nv_crtc->head, false);

	NVWriteVgaCrtc(pNv, nv_crtc->head, NV_CIO_CRE_RPC1_INDEX, crtc1A);
}

static Bool
nv_crtc_mode_fixup(xf86CrtcPtr crtc, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode)
{
	return TRUE;
}

static void
nv_crtc_mode_set_vga(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVCrtcRegPtr regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	/* Calculate our timings */
	int horizDisplay	= (mode->CrtcHDisplay >> 3) 	- 1;
	int horizStart		= (mode->CrtcHSyncStart >> 3) 	- 1;
	int horizEnd		= (mode->CrtcHSyncEnd >> 3) 	- 1;
	int horizTotal		= (mode->CrtcHTotal >> 3)		- 5;
	int horizBlankStart	= (mode->CrtcHDisplay >> 3)		- 1;
	int horizBlankEnd	= (mode->CrtcHTotal >> 3)		- 1;
	int vertDisplay		= mode->CrtcVDisplay			- 1;
	int vertStart		= mode->CrtcVSyncStart 		- 1;
	int vertEnd		= mode->CrtcVSyncEnd			- 1;
	int vertTotal		= mode->CrtcVTotal 			- 2;
	int vertBlankStart	= mode->CrtcVDisplay 			- 1;
	int vertBlankEnd	= mode->CrtcVTotal			- 1;

	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	bool fp_output = false;
	int i;

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);

		if (output->crtc == crtc && (nv_encoder->dcb->type == OUTPUT_LVDS ||
					     nv_encoder->dcb->type == OUTPUT_TMDS))
			fp_output = true;
	}

	if (fp_output) {
		vertStart = vertTotal - 3;  
		vertEnd = vertTotal - 2;
		vertBlankStart = vertStart;
		horizStart = horizTotal - 5;
		horizEnd = horizTotal - 2;
		horizBlankEnd = horizTotal + 4;
		if (pNv->overlayAdaptor && pNv->Architecture >= NV_ARCH_10)
			/* This reportedly works around some video overlay bandwidth problems */
			horizTotal += 2;
	}

	if (mode->Flags & V_INTERLACE) 
		vertTotal |= 1;

#if 0
	ErrorF("horizDisplay: 0x%X \n", horizDisplay);
	ErrorF("horizStart: 0x%X \n", horizStart);
	ErrorF("horizEnd: 0x%X \n", horizEnd);
	ErrorF("horizTotal: 0x%X \n", horizTotal);
	ErrorF("horizBlankStart: 0x%X \n", horizBlankStart);
	ErrorF("horizBlankEnd: 0x%X \n", horizBlankEnd);
	ErrorF("vertDisplay: 0x%X \n", vertDisplay);
	ErrorF("vertStart: 0x%X \n", vertStart);
	ErrorF("vertEnd: 0x%X \n", vertEnd);
	ErrorF("vertTotal: 0x%X \n", vertTotal);
	ErrorF("vertBlankStart: 0x%X \n", vertBlankStart);
	ErrorF("vertBlankEnd: 0x%X \n", vertBlankEnd);
#endif

	/*
	* compute correct Hsync & Vsync polarity 
	*/
	if ((mode->Flags & (V_PHSYNC | V_NHSYNC))
		&& (mode->Flags & (V_PVSYNC | V_NVSYNC))) {

		regp->MiscOutReg = 0x23;
		if (mode->Flags & V_NHSYNC) regp->MiscOutReg |= 0x40;
		if (mode->Flags & V_NVSYNC) regp->MiscOutReg |= 0x80;
	} else {
		int VDisplay = mode->VDisplay;
		if (mode->Flags & V_DBLSCAN)
			VDisplay *= 2;
		if (mode->VScan > 1)
			VDisplay *= mode->VScan;
		if (VDisplay < 400)
			regp->MiscOutReg = 0xA3;		/* +hsync -vsync */
		else if (VDisplay < 480)
			regp->MiscOutReg = 0x63;		/* -hsync +vsync */
		else if (VDisplay < 768)
			regp->MiscOutReg = 0xE3;		/* -hsync -vsync */
		else
			regp->MiscOutReg = 0x23;		/* +hsync +vsync */
	}

	regp->MiscOutReg |= (mode->ClockIndex & 0x03) << 2;

	/*
	 * Time Sequencer
	 */
	regp->Sequencer[NV_VIO_SR_RESET_INDEX] = 0x00;
	/* 0x20 disables the sequencer */
	if (mode->Flags & V_CLKDIV2)
		regp->Sequencer[NV_VIO_SR_CLOCK_INDEX] = 0x29;
	else
		regp->Sequencer[NV_VIO_SR_CLOCK_INDEX] = 0x21;
	regp->Sequencer[NV_VIO_SR_PLANE_MASK_INDEX] = 0x0F;
	regp->Sequencer[NV_VIO_SR_CHAR_MAP_INDEX] = 0x00;
	regp->Sequencer[NV_VIO_SR_MEM_MODE_INDEX] = 0x0E;

	/*
	 * CRTC
	 */
	regp->CRTC[NV_CIO_CR_HDT_INDEX] = horizTotal;
	regp->CRTC[NV_CIO_CR_HDE_INDEX] = horizDisplay;
	regp->CRTC[NV_CIO_CR_HBS_INDEX] = horizBlankStart;
	regp->CRTC[NV_CIO_CR_HBE_INDEX] = (1 << 7) |
					  XLATE(horizBlankEnd, 0, NV_CIO_CR_HBE_4_0);
	regp->CRTC[NV_CIO_CR_HRS_INDEX] = horizStart;
	regp->CRTC[NV_CIO_CR_HRE_INDEX] = XLATE(horizBlankEnd, 5, NV_CIO_CR_HRE_HBE_5) |
					  XLATE(horizEnd, 0, NV_CIO_CR_HRE_4_0);
	regp->CRTC[NV_CIO_CR_VDT_INDEX] = vertTotal;
	regp->CRTC[NV_CIO_CR_OVL_INDEX] = XLATE(vertStart, 9, NV_CIO_CR_OVL_VRS_9) |
					  XLATE(vertDisplay, 9, NV_CIO_CR_OVL_VDE_9) |
					  XLATE(vertTotal, 9, NV_CIO_CR_OVL_VDT_9) |
					  (1 << 4) |
					  XLATE(vertBlankStart, 8, NV_CIO_CR_OVL_VBS_8) |
					  XLATE(vertStart, 8, NV_CIO_CR_OVL_VRS_8) |
					  XLATE(vertDisplay, 8, NV_CIO_CR_OVL_VDE_8) |
					  XLATE(vertTotal, 8, NV_CIO_CR_OVL_VDT_8);
	regp->CRTC[NV_CIO_CR_RSAL_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_CELL_HT_INDEX] = ((mode->Flags & V_DBLSCAN) ? MASK(NV_CIO_CR_CELL_HT_SCANDBL) : 0) |
					      1 << 6 |
					      XLATE(vertBlankStart, 9, NV_CIO_CR_CELL_HT_VBS_9);
	regp->CRTC[NV_CIO_CR_CURS_ST_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_CURS_END_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_SA_HI_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_SA_LO_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_TCOFF_HI_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_TCOFF_LO_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_VRS_INDEX] = vertStart;
	regp->CRTC[NV_CIO_CR_VRE_INDEX] = 1 << 5 | XLATE(vertEnd, 0, NV_CIO_CR_VRE_3_0);
	regp->CRTC[NV_CIO_CR_VDE_INDEX] = vertDisplay;
	/* framebuffer can be larger than crtc scanout area. */
	regp->CRTC[NV_CIO_CR_OFFSET_INDEX] = pScrn->displayWidth / 8 * pScrn->bitsPerPixel / 8;
	regp->CRTC[NV_CIO_CR_ULINE_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_VBS_INDEX] = vertBlankStart;
	regp->CRTC[NV_CIO_CR_VBE_INDEX] = vertBlankEnd;
	regp->CRTC[NV_CIO_CR_MODE_INDEX] = 0x43;
	regp->CRTC[NV_CIO_CR_LCOMP_INDEX] = 0xff;

	/* 
	 * Some extended CRTC registers (they are not saved with the rest of the vga regs).
	 */

	/* framebuffer can be larger than crtc scanout area. */
	regp->CRTC[NV_CIO_CRE_RPC0_INDEX] = XLATE(pScrn->displayWidth / 8 * pScrn->bitsPerPixel / 8, 8, NV_CIO_CRE_RPC0_OFFSET_10_8);
	regp->CRTC[NV_CIO_CRE_RPC1_INDEX] = mode->CrtcHDisplay < 1280 ?
					    MASK(NV_CIO_CRE_RPC1_LARGE) : 0x00;
	regp->CRTC[NV_CIO_CRE_LSR_INDEX] = XLATE(horizBlankEnd, 6, NV_CIO_CRE_LSR_HBE_6) |
					   XLATE(vertBlankStart, 10, NV_CIO_CRE_LSR_VBS_10) |
					   XLATE(vertStart, 10, NV_CIO_CRE_LSR_VRS_10) |
					   XLATE(vertDisplay, 10, NV_CIO_CRE_LSR_VDE_10) |
					   XLATE(vertTotal, 10, NV_CIO_CRE_LSR_VDT_10);
	regp->CRTC[NV_CIO_CRE_HEB__INDEX] = XLATE(horizStart, 8, NV_CIO_CRE_HEB_HRS_8) |
					    XLATE(horizBlankStart, 8, NV_CIO_CRE_HEB_HBS_8) |
					    XLATE(horizDisplay, 8, NV_CIO_CRE_HEB_HDE_8) |
					    XLATE(horizTotal, 8, NV_CIO_CRE_HEB_HDT_8);
	regp->CRTC[NV_CIO_CRE_EBR_INDEX] = XLATE(vertBlankStart, 11, NV_CIO_CRE_EBR_VBS_11) |
					   XLATE(vertStart, 11, NV_CIO_CRE_EBR_VRS_11) |
					   XLATE(vertDisplay, 11, NV_CIO_CRE_EBR_VDE_11) |
					   XLATE(vertTotal, 11, NV_CIO_CRE_EBR_VDT_11);

	if (mode->Flags & V_INTERLACE) {
		horizTotal = (horizTotal >> 1) & ~1;
		regp->CRTC[NV_CIO_CRE_ILACE__INDEX] = horizTotal;
		regp->CRTC[NV_CIO_CRE_HEB__INDEX] |= XLATE(horizTotal, 8, NV_CIO_CRE_HEB_ILC_8);
	} else
		regp->CRTC[NV_CIO_CRE_ILACE__INDEX] = 0xff;  /* interlace off */

	/*
	* Graphics Display Controller
	*/
	regp->Graphics[NV_VIO_GX_SR_INDEX] = 0x00;
	regp->Graphics[NV_VIO_GX_SREN_INDEX] = 0x00;
	regp->Graphics[NV_VIO_GX_CCOMP_INDEX] = 0x00;
	regp->Graphics[NV_VIO_GX_ROP_INDEX] = 0x00;
	regp->Graphics[NV_VIO_GX_READ_MAP_INDEX] = 0x00;
	regp->Graphics[NV_VIO_GX_MODE_INDEX] = 0x40; /* 256 color mode */
	regp->Graphics[NV_VIO_GX_MISC_INDEX] = 0x05; /* map 64k mem + graphic mode */
	regp->Graphics[NV_VIO_GX_DONT_CARE_INDEX] = 0x0F;
	regp->Graphics[NV_VIO_GX_BIT_MASK_INDEX] = 0xFF;

	regp->Attribute[0]  = 0x00; /* standard colormap translation */
	regp->Attribute[1]  = 0x01;
	regp->Attribute[2]  = 0x02;
	regp->Attribute[3]  = 0x03;
	regp->Attribute[4]  = 0x04;
	regp->Attribute[5]  = 0x05;
	regp->Attribute[6]  = 0x06;
	regp->Attribute[7]  = 0x07;
	regp->Attribute[8]  = 0x08;
	regp->Attribute[9]  = 0x09;
	regp->Attribute[10] = 0x0A;
	regp->Attribute[11] = 0x0B;
	regp->Attribute[12] = 0x0C;
	regp->Attribute[13] = 0x0D;
	regp->Attribute[14] = 0x0E;
	regp->Attribute[15] = 0x0F;
	regp->Attribute[NV_CIO_AR_MODE_INDEX] = 0x01; /* Enable graphic mode */
	/* Non-vga */
	regp->Attribute[NV_CIO_AR_OSCAN_INDEX] = 0x00;
	regp->Attribute[NV_CIO_AR_PLANE_INDEX] = 0x0F; /* enable all color planes */
	regp->Attribute[NV_CIO_AR_HPP_INDEX] = 0x00;
	regp->Attribute[NV_CIO_AR_CSEL_INDEX] = 0x00;
}

/**
 * Sets up registers for the given mode/adjusted_mode pair.
 *
 * The clocks, CRTCs and outputs attached to this CRTC must be off.
 *
 * This shouldn't enable any clocks, CRTCs, or outputs, but they should
 * be easily turned on/off after this.
 */
static void
nv_crtc_mode_set_regs(xf86CrtcPtr crtc, DisplayModePtr mode)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVCrtcRegPtr regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];
	NVCrtcRegPtr savep = &pNv->SavedReg.crtc_reg[nv_crtc->head];
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	bool lvds_output = false, tmds_output = false, off_chip_digital = false;
	int i;

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);
		bool digital = false;

		if (output->crtc != crtc || !nv_encoder)
			continue;

		if (nv_encoder->dcb->type == OUTPUT_LVDS)
			digital = lvds_output = true;
		if (nv_encoder->dcb->type == OUTPUT_TMDS)
			digital = tmds_output = true;
		if (nv_encoder->dcb->location != DCB_LOC_ON_CHIP && digital)
			off_chip_digital = true;
	}

	/* Registers not directly related to the (s)vga mode */

	/* What is the meaning of this register? */
	/* A few popular values are 0x18, 0x1c, 0x38, 0x3c */ 
	regp->CRTC[NV_CIO_CRE_ENH_INDEX] = savep->CRTC[NV_CIO_CRE_ENH_INDEX] & ~(1<<5);

	regp->crtc_eng_ctrl = 0;
	/* Except for rare conditions I2C is enabled on the primary crtc */
	if (nv_crtc->head == 0)
		regp->crtc_eng_ctrl |= NV_CRTC_FSEL_I2C;
	/* Set overlay to desired crtc. */
	if (pNv->overlayAdaptor) {
		NVPortPrivPtr pPriv = GET_OVERLAY_PRIVATE(pNv);
		if (pPriv->overlayCRTC == nv_crtc->head)
			regp->crtc_eng_ctrl |= NV_CRTC_FSEL_OVERLAY;
	}

	/* ADDRESS_SPACE_PNVM is the same as setting HCUR_ASI */
	regp->cursor_cfg = NV_PCRTC_CURSOR_CONFIG_CUR_LINES_64 |
			     NV_PCRTC_CURSOR_CONFIG_CUR_PIXELS_64 |
			     NV_PCRTC_CURSOR_CONFIG_ADDRESS_SPACE_PNVM;
	if (pNv->alphaCursor)
		regp->cursor_cfg |= NV_PCRTC_CURSOR_CONFIG_CUR_BPP_32;
	if (mode->Flags & V_DBLSCAN)
		regp->cursor_cfg |= NV_PCRTC_CURSOR_CONFIG_DOUBLE_SCAN_ENABLE;

	/* Unblock some timings */
	regp->CRTC[NV_CIO_CRE_53] = 0;
	regp->CRTC[NV_CIO_CRE_54] = 0;

	/* 0x00 is disabled, 0x11 is lvds, 0x22 crt and 0x88 tmds */
	if (lvds_output)
		regp->CRTC[NV_CIO_CRE_SCRATCH3__INDEX] = 0x11;
	else if (tmds_output)
		regp->CRTC[NV_CIO_CRE_SCRATCH3__INDEX] = 0x88;
	else
		regp->CRTC[NV_CIO_CRE_SCRATCH3__INDEX] = 0x22;

	/* These values seem to vary */
	/* This register seems to be used by the bios to make certain decisions on some G70 cards? */
	regp->CRTC[NV_CIO_CRE_SCRATCH4__INDEX] = savep->CRTC[NV_CIO_CRE_SCRATCH4__INDEX];

	nv_crtc_set_digital_vibrance(crtc, nv_crtc->saturation);

	/* probably a scratch reg, but kept for cargo-cult purposes:
	 * bit0: crtc0?, head A
	 * bit6: lvds, head A
	 * bit7: (only in X), head A
	 */
	if (nv_crtc->head == 0)
		regp->CRTC[NV_CIO_CRE_4B] = savep->CRTC[NV_CIO_CRE_4B] | 0x80;

	/* The blob seems to take the current value from crtc 0, add 4 to that
	 * and reuse the old value for crtc 1 */
	regp->CRTC[NV_CIO_CRE_TVOUT_LATENCY] = pNv->SavedReg.crtc_reg[0].CRTC[NV_CIO_CRE_TVOUT_LATENCY];
	if (!nv_crtc->head)
		regp->CRTC[NV_CIO_CRE_TVOUT_LATENCY] += 4;

	/* the blob sometimes sets |= 0x10 (which is the same as setting |=
	 * 1 << 30 on 0x60.830), for no apparent reason */
	regp->CRTC[NV_CIO_CRE_59] = off_chip_digital;

	regp->crtc_830 = mode->CrtcVDisplay - 3;
	regp->crtc_834 = mode->CrtcVDisplay - 1;

	if (pNv->Architecture == NV_ARCH_40)
		/* This is what the blob does */
		regp->crtc_850 = NVReadCRTC(pNv, 0, NV_PCRTC_850);

	if (pNv->Architecture == NV_ARCH_40)
		regp->gpio_ext = NVReadCRTC(pNv, 0, NV_PCRTC_GPIO_EXT);

	regp->crtc_cfg = NV_PCRTC_CONFIG_START_ADDRESS_HSYNC;

	/* Some misc regs */
	if (pNv->Architecture == NV_ARCH_40) {
		regp->CRTC[NV_CIO_CRE_85] = 0xFF;
		regp->CRTC[NV_CIO_CRE_86] = 0x1;
	}

	regp->CRTC[NV_CIO_CRE_PIXEL_INDEX] = (pScrn->depth + 1) / 8;
	/* Enable slaved mode (called MODE_TV in nv4ref.h) */
	if (lvds_output || tmds_output)
		regp->CRTC[NV_CIO_CRE_PIXEL_INDEX] |= (1 << 7);

	/* Generic PRAMDAC regs */

	if (pNv->Architecture >= NV_ARCH_10)
		/* Only bit that bios and blob set. */
		regp->nv10_cursync = (1 << 25);

	regp->ramdac_gen_ctrl = NV_PRAMDAC_GENERAL_CONTROL_BPC_8BITS |
				NV_PRAMDAC_GENERAL_CONTROL_VGA_STATE_SEL |
				NV_PRAMDAC_GENERAL_CONTROL_PIXMIX_ON;
	if (pScrn->depth == 16)
		regp->ramdac_gen_ctrl |= NV_PRAMDAC_GENERAL_CONTROL_ALT_MODE_SEL;
	if (pNv->alphaCursor)
		regp->ramdac_gen_ctrl |= NV_PRAMDAC_GENERAL_CONTROL_PIPE_LONG;

	regp->ramdac_630 = 0; /* turn off green mode (tv test pattern?) */

	nv_crtc_set_image_sharpening(crtc, nv_crtc->sharpness);

	/* Some values the blob sets */
	regp->ramdac_a20 = 0x0;
	regp->ramdac_a24 = 0xfffff;
	regp->ramdac_a34 = 0x1;
}

enum fp_display_regs {
	FP_DISPLAY_END,
	FP_TOTAL,
	FP_CRTC,
	FP_SYNC_START,
	FP_SYNC_END,
	FP_VALID_START,
	FP_VALID_END
};

/* this could be set in nv_output, but would require some rework of load/save */
static void
nv_crtc_mode_set_fp_regs(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVCrtcRegPtr regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];
	NVCrtcRegPtr savep = &pNv->SavedReg.crtc_reg[nv_crtc->head];
	struct nouveau_encoder *nv_encoder = NULL;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	uint32_t mode_ratio, panel_ratio;
	int i;

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		struct nouveau_encoder *tmp_nve = to_nouveau_encoder(output);

		if (output->crtc != crtc)
			continue;

		if (tmp_nve->dcb->type == OUTPUT_LVDS ||
		    tmp_nve->dcb->type == OUTPUT_TMDS) {
			/* assumes a maximum of one fp output per crtc */
			nv_encoder = tmp_nve;
			break;
		}
	}
	if (!nv_encoder)
		return;

	regp->fp_horiz_regs[FP_DISPLAY_END] = adjusted_mode->HDisplay - 1;
	regp->fp_horiz_regs[FP_TOTAL] = adjusted_mode->HTotal - 1;
	if (!pNv->gf4_disp_arch ||
	    (adjusted_mode->HSyncStart - adjusted_mode->HDisplay) >=
					pNv->vbios->digital_min_front_porch)
		regp->fp_horiz_regs[FP_CRTC] = adjusted_mode->HDisplay;
	else
		regp->fp_horiz_regs[FP_CRTC] = adjusted_mode->HSyncStart - pNv->vbios->digital_min_front_porch - 1;
	regp->fp_horiz_regs[FP_SYNC_START] = adjusted_mode->HSyncStart - 1;
	regp->fp_horiz_regs[FP_SYNC_END] = adjusted_mode->HSyncEnd - 1;
	regp->fp_horiz_regs[FP_VALID_START] = adjusted_mode->HSkew;
	regp->fp_horiz_regs[FP_VALID_END] = adjusted_mode->HDisplay - 1;

	regp->fp_vert_regs[FP_DISPLAY_END] = adjusted_mode->VDisplay - 1;
	regp->fp_vert_regs[FP_TOTAL] = adjusted_mode->VTotal - 1;
	regp->fp_vert_regs[FP_CRTC] = adjusted_mode->VTotal - 5 - 1;
	regp->fp_vert_regs[FP_SYNC_START] = adjusted_mode->VSyncStart - 1;
	regp->fp_vert_regs[FP_SYNC_END] = adjusted_mode->VSyncEnd - 1;
	regp->fp_vert_regs[FP_VALID_START] = 0;
	regp->fp_vert_regs[FP_VALID_END] = adjusted_mode->VDisplay - 1;

	/* bit26: a bit seen on some g7x, no as yet discernable purpose */
	regp->fp_control = NV_PRAMDAC_FP_TG_CONTROL_DISPEN_POS |
			   (savep->fp_control & (1 << 26 | NV_PRAMDAC_FP_TG_CONTROL_READ_PROG));
	/* Deal with vsync/hsync polarity */
	/* LVDS screens do set this, but modes with +ve syncs are very rare */
	if (adjusted_mode->Flags & V_PVSYNC)
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_VSYNC_POS;
	if (adjusted_mode->Flags & V_PHSYNC)
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_HSYNC_POS;
	/* panel scaling first, as native would get set otherwise */
	if (nv_encoder->scaling_mode == SCALE_PANEL ||
	    nv_encoder->scaling_mode == SCALE_NOSCALE)	/* panel handles it */
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_MODE_CENTER;
	else if (mode->HDisplay == adjusted_mode->HDisplay &&
		 mode->VDisplay == adjusted_mode->VDisplay) /* native mode */
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_MODE_NATIVE;
	else /* gpu needs to scale */
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_MODE_SCALE;
	if (nvReadEXTDEV(pNv, NV_PEXTDEV_BOOT_0) & NV_PEXTDEV_BOOT_0_STRAP_FP_IFACE_12BIT)
		regp->fp_control |= NV_PRAMDAC_FP_TG_CONTROL_WIDTH_12;
	if (nv_encoder->dcb->location != DCB_LOC_ON_CHIP &&
	    adjusted_mode->Clock > 165000)
		regp->fp_control |= (2 << 24);
	if (nv_encoder->dual_link)
		regp->fp_control |= (8 << 28);

	regp->fp_debug_0 = NV_PRAMDAC_FP_DEBUG_0_YWEIGHT_ROUND |
			   NV_PRAMDAC_FP_DEBUG_0_XWEIGHT_ROUND |
			   NV_PRAMDAC_FP_DEBUG_0_YINTERP_BILINEAR |
			   NV_PRAMDAC_FP_DEBUG_0_XINTERP_BILINEAR |
			   NV_RAMDAC_FP_DEBUG_0_TMDS_ENABLED |
			   NV_PRAMDAC_FP_DEBUG_0_YSCALE_ENABLE |
			   NV_PRAMDAC_FP_DEBUG_0_XSCALE_ENABLE;

	/* We want automatic scaling */
	regp->fp_debug_1 = 0;
	/* This can override HTOTAL and VTOTAL */
	regp->fp_debug_2 = 0;

	/* Use 20.12 fixed point format to avoid floats */
	mode_ratio = (1 << 12) * mode->HDisplay / mode->VDisplay;
	panel_ratio = (1 << 12) * adjusted_mode->HDisplay / adjusted_mode->VDisplay;
	/* if ratios are equal, SCALE_ASPECT will automatically (and correctly)
	 * get treated the same as SCALE_FULLSCREEN */
	if (nv_encoder->scaling_mode == SCALE_ASPECT && mode_ratio != panel_ratio) {
		uint32_t diff, scale;
		bool divide_by_2 = pNv->gf4_disp_arch;

		if (mode_ratio < panel_ratio) {
			/* vertical needs to expand to glass size (automatic)
			 * horizontal needs to be scaled at vertical scale factor
			 * to maintain aspect */

			scale = (1 << 12) * mode->VDisplay / adjusted_mode->VDisplay;
			regp->fp_debug_1 = NV_PRAMDAC_FP_DEBUG_1_XSCALE_TESTMODE_ENABLE |
					   XLATE(scale, divide_by_2, NV_PRAMDAC_FP_DEBUG_1_XSCALE_VALUE);

			/* restrict area of screen used, horizontally */
			diff = adjusted_mode->HDisplay -
			       adjusted_mode->VDisplay * mode_ratio / (1 << 12);
			regp->fp_horiz_regs[FP_VALID_START] += diff / 2;
			regp->fp_horiz_regs[FP_VALID_END] -= diff / 2;
		}

		if (mode_ratio > panel_ratio) {
			/* horizontal needs to expand to glass size (automatic)
			 * vertical needs to be scaled at horizontal scale factor
			 * to maintain aspect */

			scale = (1 << 12) * mode->HDisplay / adjusted_mode->HDisplay;
			regp->fp_debug_1 = NV_PRAMDAC_FP_DEBUG_1_YSCALE_TESTMODE_ENABLE |
					   XLATE(scale, divide_by_2, NV_PRAMDAC_FP_DEBUG_1_YSCALE_VALUE);

			/* restrict area of screen used, vertically */
			diff = adjusted_mode->VDisplay -
			       (1 << 12) * adjusted_mode->HDisplay / mode_ratio;
			regp->fp_vert_regs[FP_VALID_START] += diff / 2;
			regp->fp_vert_regs[FP_VALID_END] -= diff / 2;
		}
	}

	/* Output property. */
	if (nv_encoder && nv_encoder->dithering) {
		if (pNv->NVArch == 0x11)
			regp->dither = savep->dither | 0x00010000;
		else {
			int i;
			regp->dither = savep->dither | 0x00000001;
			for (i = 0; i < 3; i++) {
				regp->dither_regs[i] = 0xe4e4e4e4;
				regp->dither_regs[i + 3] = 0x44444444;
			}
		}
	} else {
		if (pNv->NVArch != 0x11) {
			/* reset them */
			int i;
			for (i = 0; i < 3; i++) {
				regp->dither_regs[i] = savep->dither_regs[i];
				regp->dither_regs[i + 3] = savep->dither_regs[i + 3];
			}
		}
		regp->dither = savep->dither;
	}
}

/**
 * Sets up registers for the given mode/adjusted_mode pair.
 *
 * The clocks, CRTCs and outputs attached to this CRTC must be off.
 *
 * This shouldn't enable any clocks, CRTCs, or outputs, but they should
 * be easily turned on/off after this.
 */
static void
nv_crtc_mode_set(xf86CrtcPtr crtc, DisplayModePtr mode,
		 DisplayModePtr adjusted_mode,
		 int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(pScrn);

	NV_TRACE(pScrn, "CTRC mode on CRTC %d:\n", nv_crtc->head);
	xf86PrintModeline(pScrn->scrnIndex, mode);
	NV_TRACE(pScrn, "Output mode on CRTC %d:\n", nv_crtc->head);
	xf86PrintModeline(pScrn->scrnIndex, adjusted_mode);

	/* unlock must come after turning off FP_TG_CONTROL in output_prepare */
	nv_lock_vga_crtc_shadow(pNv, nv_crtc->head, -1);

	nv_crtc_mode_set_vga(crtc, mode, adjusted_mode);
	/* calculated in output_prepare, nv40 needs it written before calculating PLLs */
	if (pNv->Architecture == NV_ARCH_40)
		NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_SEL_CLK, pNv->ModeReg.sel_clk);
	nv_crtc_mode_set_regs(crtc, mode);
	nv_crtc_mode_set_fp_regs(crtc, mode, adjusted_mode);
	nv_crtc_calc_state_ext(crtc, mode, adjusted_mode->Clock);

	nouveau_hw_load_state(pScrn, nv_crtc->head, &pNv->ModeReg);

	NVCrtcSetBase(crtc, x, y);

#if X_BYTE_ORDER == X_BIG_ENDIAN
	/* turn on LFB swapping */
	{
		uint8_t tmp = NVReadVgaCrtc(pNv, nv_crtc->head, NV_CIO_CRE_RCR);
		tmp |= MASK(NV_CIO_CRE_RCR_ENDIAN_BIG);
		NVWriteVgaCrtc(pNv, nv_crtc->head, NV_CIO_CRE_RCR, tmp);
	}
#endif
}

static void nv_crtc_save(xf86CrtcPtr crtc)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(crtc->scrn);

	if (pNv->twoHeads)
		NVSetOwner(pNv, nv_crtc->head);

	nouveau_hw_save_state(crtc->scrn, nv_crtc->head, &pNv->SavedReg);

	/* init some state to saved value */
	pNv->ModeReg.sel_clk = pNv->SavedReg.sel_clk & ~(0x5 << 16);
	pNv->ModeReg.crtc_reg[nv_crtc->head].CRTC[NV_CIO_CRE_LCD__INDEX] = pNv->SavedReg.crtc_reg[nv_crtc->head].CRTC[NV_CIO_CRE_LCD__INDEX];
}

static void nv_crtc_restore(xf86CrtcPtr crtc)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(crtc->scrn);
	int head = nv_crtc->head;
	uint8_t saved_cr21 = pNv->SavedReg.crtc_reg[head].CRTC[NV_CIO_CRE_21];

	if (pNv->twoHeads)
		NVSetOwner(pNv, head);

	nouveau_hw_load_state(crtc->scrn, head, &pNv->SavedReg);
	nv_lock_vga_crtc_shadow(pNv, head, saved_cr21);

	nv_crtc->last_dpms = NV_DPMS_CLEARED;
}

static void nv_crtc_prepare(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);

	if (pNv->twoHeads)
		NVSetOwner(pNv, nv_crtc->head);

	crtc->funcs->dpms(crtc, DPMSModeOff);

	/* Sync the engine before adjust mode */
	if (pNv->EXADriverPtr) {
		exaMarkSync(pScrn->pScreen);
		exaWaitSync(pScrn->pScreen);
	}

	NVBlankScreen(pNv, nv_crtc->head, true);

	/* Some more preperation. */
	NVWriteCRTC(pNv, nv_crtc->head, NV_PCRTC_CONFIG, NV_PCRTC_CONFIG_START_ADDRESS_NON_VGA);
	if (pNv->Architecture == NV_ARCH_40) {
		uint32_t reg900 = NVReadRAMDAC(pNv, nv_crtc->head, NV_PRAMDAC_900);
		NVWriteRAMDAC(pNv, nv_crtc->head, NV_PRAMDAC_900, reg900 & ~0x10000);
	}
}

static void nv_crtc_commit(xf86CrtcPtr crtc)
{
	crtc->funcs->dpms (crtc, DPMSModeOn);

	if (crtc->scrn->pScreen != NULL) {
		NVPtr pNv = NVPTR(crtc->scrn);

		xf86_reload_cursors (crtc->scrn->pScreen);
		if (!pNv->alphaCursor) {
			/* this works round the fact that xf86_reload_cursors
			 * will quite happily show the hw cursor when it knows
			 * the hardware can't do alpha, and the current cursor
			 * has an alpha channel
			 */
			xf86ForceHWCursor(crtc->scrn->pScreen, 1);
			xf86ForceHWCursor(crtc->scrn->pScreen, 0);
		}
	}
}

static void nv_crtc_destroy(xf86CrtcPtr crtc)
{
	xfree(to_nouveau_crtc(crtc));
}

static Bool nv_crtc_lock(xf86CrtcPtr crtc)
{
	return FALSE;
}

static void nv_crtc_unlock(xf86CrtcPtr crtc)
{
}

#define DEPTH_SHIFT(val, w) ((val << (8 - w)) | (val >> ((w << 1) - 8)))

static void
nv_crtc_gamma_set(xf86CrtcPtr crtc, CARD16 *red, CARD16 *green, CARD16 *blue,
					int size)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(crtc->scrn);
	struct rgb { uint8_t r, g, b; } __attribute__((packed)) *rgbs;
	int i;

	rgbs = (struct rgb *)pNv->ModeReg.crtc_reg[nv_crtc->head].DAC;

	switch (crtc->scrn->depth) {
	case 15:
		/* R5G5B5 */
		/* spread 5 bits per colour (32 colours) over 256 (per colour) registers */
		for (i = 0; i < 32; i++) {
			rgbs[DEPTH_SHIFT(i, 5)].r = red[i] >> 8;
			rgbs[DEPTH_SHIFT(i, 5)].g = green[i] >> 8;
			rgbs[DEPTH_SHIFT(i, 5)].b = blue[i] >> 8;
		}
		break;
	case 16:
		/* R5G6B5 */
		for (i = 0; i < 64; i++) {
			/* set 64 regs for green's 6 bits of colour */
			rgbs[DEPTH_SHIFT(i, 6)].g = green[i] >> 8;
			if (i < 32) {
				rgbs[DEPTH_SHIFT(i, 5)].r = red[i] >> 8;
				rgbs[DEPTH_SHIFT(i, 5)].b = blue[i] >> 8;
			}
		}
		break;
	default:
		/* R8G8B8 */
		for (i = 0; i < 256; i++) {
			rgbs[i].r = red[i] >> 8;
			rgbs[i].g = green[i] >> 8;
			rgbs[i].b = blue[i] >> 8;
		}
	}

	nouveau_hw_load_state_palette(pNv, nv_crtc->head, &pNv->ModeReg);
}

/**
 * Allocates memory for a locked-in-framebuffer shadow of the given
 * width and height for this CRTC's rotated shadow framebuffer.
 */
 
static void *
nv_crtc_shadow_allocate (xf86CrtcPtr crtc, int width, int height)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	ScrnInfoPtr pScrn = crtc->scrn;
#if !NOUVEAU_EXA_PIXMAPS
	ScreenPtr pScreen = pScrn->pScreen;
#endif /* !NOUVEAU_EXA_PIXMAPS */
	NVPtr pNv = NVPTR(pScrn);
	void *offset;

	unsigned long rotate_pitch;
	int size, align = 64;

	rotate_pitch = pScrn->displayWidth * (pScrn->bitsPerPixel/8);
	size = rotate_pitch * height;

	assert(nv_crtc->shadow == NULL);
#if NOUVEAU_EXA_PIXMAPS
	if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_PIN |
			   NOUVEAU_BO_MAP, align, size, &nv_crtc->shadow)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to allocate memory for shadow buffer!\n");
		return NULL;
	}

	if (nv_crtc->shadow && nouveau_bo_map(nv_crtc->shadow, NOUVEAU_BO_RDWR)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"Failed to map shadow buffer.\n");
		return NULL;
	}

	offset = nv_crtc->shadow->map;
#else
	if (!pScreen) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Can't allocate shadow memory for rotated CRTC at server regeneration\n");
		return NULL;
	}
	nv_crtc->shadow = exaOffscreenAlloc(pScreen, size, align, TRUE, NULL, NULL);
	if (nv_crtc->shadow == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Couldn't allocate shadow memory for rotated CRTC.\n");
		return NULL;
	}
	offset = pNv->FBMap + nv_crtc->shadow->offset;
#endif /* NOUVEAU_EXA_PIXMAPS */

	return offset;
}

/**
 * Creates a pixmap for this CRTC's rotated shadow framebuffer.
 */
static PixmapPtr
nv_crtc_shadow_create(xf86CrtcPtr crtc, void *data, int width, int height)
{
	ScrnInfoPtr pScrn = crtc->scrn;
#if NOUVEAU_EXA_PIXMAPS
	ScreenPtr pScreen = pScrn->pScreen;
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
#endif /* NOUVEAU_EXA_PIXMAPS */
	unsigned long rotate_pitch;
	PixmapPtr rotate_pixmap;
#if NOUVEAU_EXA_PIXMAPS
	struct nouveau_pixmap *nvpix;
#endif /* NOUVEAU_EXA_PIXMAPS */

	if (!data)
		data = crtc->funcs->shadow_allocate (crtc, width, height);

	rotate_pitch = pScrn->displayWidth * (pScrn->bitsPerPixel/8);

#if NOUVEAU_EXA_PIXMAPS
	/* Create a dummy pixmap, to get a private that will be accepted by the system.*/
	rotate_pixmap = pScreen->CreatePixmap(pScreen, 
								0, /* width */
								0, /* height */
	#ifdef CREATE_PIXMAP_USAGE_SCRATCH /* there seems to have been no api bump */
								pScrn->depth,
								0);
	#else
								pScrn->depth);
	#endif /* CREATE_PIXMAP_USAGE_SCRATCH */
#else
	rotate_pixmap = GetScratchPixmapHeader(pScrn->pScreen,
								width, height,
								pScrn->depth,
								pScrn->bitsPerPixel,
								rotate_pitch,
								data);
#endif /* NOUVEAU_EXA_PIXMAPS */

	if (rotate_pixmap == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Couldn't allocate shadow pixmap for rotated CRTC\n");
	}

#if NOUVEAU_EXA_PIXMAPS
	nvpix = exaGetPixmapDriverPrivate(rotate_pixmap);
	if (!nvpix) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No initial shadow private available for rotation.\n");
	} else {
		nvpix->bo = nv_crtc->shadow;
		nvpix->mapped = TRUE;
	}

	/* Modify the pixmap to actually be the one we need. */
	pScreen->ModifyPixmapHeader(rotate_pixmap,
					width,
					height,
					pScrn->depth,
					pScrn->bitsPerPixel,
					rotate_pitch,
					data);

	nvpix = exaGetPixmapDriverPrivate(rotate_pixmap);
	if (!nvpix || !nvpix->bo)
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No final shadow private available for rotation.\n");
#endif /* NOUVEAU_EXA_PIXMAPS */

	return rotate_pixmap;
}

static void
nv_crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr rotate_pixmap, void *data)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	ScreenPtr pScreen = pScrn->pScreen;

	if (rotate_pixmap) { /* This should also unmap the buffer object if relevant. */
		pScreen->DestroyPixmap(rotate_pixmap);
	}

#if !NOUVEAU_EXA_PIXMAPS
	if (data && nv_crtc->shadow) {
		exaOffscreenFree(pScreen, nv_crtc->shadow);
	}
#endif /* !NOUVEAU_EXA_PIXMAPS */

	nv_crtc->shadow = NULL;
}

static const xf86CrtcFuncsRec nv_crtc_funcs = {
	.dpms = nv_crtc_dpms,
	.save = nv_crtc_save,
	.restore = nv_crtc_restore,
	.mode_fixup = nv_crtc_mode_fixup,
	.mode_set = nv_crtc_mode_set,
	.prepare = nv_crtc_prepare,
	.commit = nv_crtc_commit,
	.destroy = nv_crtc_destroy,
	.lock = nv_crtc_lock,
	.unlock = nv_crtc_unlock,
	.set_cursor_colors = nv_crtc_set_cursor_colors,
	.set_cursor_position = nv_crtc_set_cursor_position,
	.show_cursor = nv_crtc_show_cursor,
	.hide_cursor = nv_crtc_hide_cursor,
	.load_cursor_image = nv_crtc_load_cursor_image,
	.load_cursor_argb = nv_crtc_load_cursor_argb,
	.gamma_set = nv_crtc_gamma_set,
	.shadow_create = nv_crtc_shadow_create,
	.shadow_allocate = nv_crtc_shadow_allocate,
	.shadow_destroy = nv_crtc_shadow_destroy,
#ifdef RANDR_13_INTERFACE
	.set_origin = NVCrtcSetBase,
#endif
};

void
nv_crtc_init(ScrnInfoPtr pScrn, int crtc_num)
{
	NVPtr pNv = NVPTR(pScrn);
	static xf86CrtcFuncsRec crtcfuncs;
	xf86CrtcPtr crtc;
	struct nouveau_crtc *nv_crtc;
	NVCrtcRegPtr regp = &pNv->ModeReg.crtc_reg[crtc_num];
	int i;

	crtcfuncs = nv_crtc_funcs;

	if (!pNv->alphaCursor)
		crtcfuncs.load_cursor_argb = NULL;
	if (pNv->NoAccel) {
		crtcfuncs.shadow_create = NULL;
		crtcfuncs.shadow_allocate = NULL;
		crtcfuncs.shadow_destroy = NULL;
	}
	
	if (!(crtc = xf86CrtcCreate(pScrn, &crtcfuncs)))
		return;

	if (!(nv_crtc = xcalloc(1, sizeof (struct nouveau_crtc)))) {
		xf86CrtcDestroy(crtc);
		return;
	}

	nv_crtc->head = crtc_num;
	nv_crtc->last_dpms = NV_DPMS_CLEARED;

	crtc->driver_private = nv_crtc;

	/* Initialise the default LUT table. */
	for (i = 0; i < 256; i++) {
		regp->DAC[i*3] = i;
		regp->DAC[(i*3)+1] = i;
		regp->DAC[(i*3)+2] = i;
	}
}

void NVCrtcSetBase(xf86CrtcPtr crtc, int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	uint32_t start = (y * pScrn->displayWidth + x) * pScrn->bitsPerPixel / 8;

	if (crtc->rotatedData != NULL) /* we do not exist on the real framebuffer */
#if NOUVEAU_EXA_PIXMAPS
		start = nv_crtc->shadow->offset;
#else
		start = pNv->FB->offset + nv_crtc->shadow->offset; /* We do exist relative to the framebuffer */
#endif
	else
		start += pNv->FB->offset;

	start &= ~3;
	pNv->ModeReg.crtc_reg[nv_crtc->head].fb_start = start;
	NVWriteCRTC(pNv, nv_crtc->head, NV_PCRTC_START, start);

	crtc->x = x;
	crtc->y = y;
}
