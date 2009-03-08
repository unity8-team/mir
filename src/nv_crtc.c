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

static void nv_crtc_load_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_load_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_load_state_ramdac(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_save_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_save_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_save_state_ramdac(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_load_state_palette(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_save_state_palette(xf86CrtcPtr crtc, RIVA_HW_STATE *state);

static uint32_t NVCrtcReadCRTC(xf86CrtcPtr crtc, uint32_t reg)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(pScrn);

	return NVReadCRTC(pNv, nv_crtc->head, reg);
}

static void NVCrtcWriteCRTC(xf86CrtcPtr crtc, uint32_t reg, uint32_t val)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(pScrn);

	NVWriteCRTC(pNv, nv_crtc->head, reg, val);
}

static uint32_t NVCrtcReadRAMDAC(xf86CrtcPtr crtc, uint32_t reg)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(pScrn);

	return NVReadRAMDAC(pNv, nv_crtc->head, reg);
}

static void NVCrtcWriteRAMDAC(xf86CrtcPtr crtc, uint32_t reg, uint32_t val)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(pScrn);

	NVWriteRAMDAC(pNv, nv_crtc->head, reg, val);
}

static void crtc_rd_cio_state(xf86CrtcPtr crtc, NVCrtcRegPtr crtcstate, int index)
{
	crtcstate->CRTC[index] = NVReadVgaCrtc(NVPTR(crtc->scrn),
					       to_nouveau_crtc(crtc)->head,
					       index);
}

static void crtc_wr_cio_state(xf86CrtcPtr crtc, NVCrtcRegPtr crtcstate, int index)
{
	NVWriteVgaCrtc(NVPTR(crtc->scrn), to_nouveau_crtc(crtc)->head, index,
		       crtcstate->CRTC[index]);
}

/* Even though they are not yet used, i'm adding some notes about some of the 0x4000 regs */
/* They are only valid for NV4x, appearantly reordered for NV5x */
/* gpu pll: 0x4000 + 0x4004
 * unknown pll: 0x4008 + 0x400c
 * vpll1: 0x4010 + 0x4014
 * vpll2: 0x4018 + 0x401c
 * unknown pll: 0x4020 + 0x4024
 * unknown pll: 0x4038 + 0x403c
 * Some of the unknown's are probably memory pll's.
 * The vpll's use two set's of multipliers and dividers. I refer to them as a and b.
 * 1 and 2 refer to the registers of each pair. There is only one post divider.
 * Logic: clock = reference_clock * ((n(a) * n(b))/(m(a) * m(b))) >> p
 * 1) bit 0-7: familiar values, but redirected from were? (similar to PLL_SETUP_CONTROL)
 *     bit8: A switch that turns of the second divider and multiplier off.
 *     bit12: Also a switch, i haven't seen it yet.
 *     bit16-19: p-divider
 *     but 28-31: Something related to the mode that is used (see bit8).
 * 2) bit0-7: m-divider (a)
 *     bit8-15: n-multiplier (a)
 *     bit16-23: m-divider (b)
 *     bit24-31: n-multiplier (b)
 */

/* Modifying the gpu pll for example requires:
 * - Disable value 0x333 (inverse AND mask) on the 0xc040 register.
 * This is not needed for the vpll's which have their own bits.
 */

static void nv_crtc_save_state_pll(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVCrtcRegPtr regp = &state->crtc_reg[nv_crtc->head];
	NVPtr pNv = NVPTR(crtc->scrn);
	enum pll_types plltype = nv_crtc->head ? VPLL2 : VPLL1;

	if (nv_crtc->head) {
		regp->vpll_a = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2);
		if (pNv->two_reg_pll)
			regp->vpll_b = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B);
	} else {
		regp->vpll_a = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL);
		if (pNv->two_reg_pll)
			regp->vpll_b = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B);
	}
	nouveau_hw_get_pllvals(crtc->scrn, plltype, &regp->pllvals);
	if (pNv->twoHeads)
		state->sel_clk = NVReadRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK);
	state->pllsel = NVReadRAMDAC(pNv, 0, NV_RAMDAC_PLL_SELECT);
	if (pNv->Architecture == NV_ARCH_40)
		state->reg580 = NVReadRAMDAC(pNv, 0, NV_RAMDAC_580);
}

static void nv_crtc_load_state_pll(xf86CrtcPtr crtc, RIVA_HW_STATE *state, struct nouveau_pll_vals *pllvals)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVCrtcRegPtr regp = &state->crtc_reg[nv_crtc->head];
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	uint32_t savedc040 = 0;

	/* This sequence is important, the NV28 is very sensitive in this area. */
	/* Keep pllsel last and sel_clk first. */
	if (pNv->twoHeads)
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK, state->sel_clk);

	if (pllvals) {
		uint32_t pllreg = nv_crtc->head ? NV_RAMDAC_VPLL2 : NV_RAMDAC_VPLL;

		nouveau_bios_setpll(pScrn, pllreg, pllvals->NM1, pllvals->NM2, pllvals->log2P);
	} else {	// XXX wrecked indentation, nm
	if (pNv->Architecture == NV_ARCH_40) {
		savedc040 = nvReadMC(pNv, 0xc040);

		/* for vpll1 change bits 16 and 17 are disabled */
		/* for vpll2 change bits 18 and 19 are disabled */
		nvWriteMC(pNv, 0xc040, savedc040 & ~(3 << (16 + nv_crtc->head * 2)));
	}

	if (nv_crtc->head) {
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL2, regp->vpll_a);
		if (pNv->two_reg_pll)
			NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B, regp->vpll_b);
	} else {
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL, regp->vpll_a);
		if (pNv->two_reg_pll)
			NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B, regp->vpll_b);
	}

	if (pNv->Architecture == NV_ARCH_40) {
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_580, state->reg580);

		/* We need to wait a while */
		usleep(5000);
		nvWriteMC(pNv, 0xc040, savedc040);
	}
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Writing NV_RAMDAC_PLL_SELECT %08X\n", state->pllsel);
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_PLL_SELECT, state->pllsel);
}

static void nv_crtc_cursor_set(xf86CrtcPtr crtc)
{
	NVPtr pNv = NVPTR(crtc->scrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	uint32_t cursor_start;
	NVCrtcRegPtr regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	if (pNv->Architecture == NV_ARCH_04)
		cursor_start = 0x5E00 << 2;
	else
		cursor_start = nv_crtc->head ? pNv->Cursor2->offset : pNv->Cursor->offset;

	regp->CRTC[NV_CIO_CRE_HCUR_ADDR0_INDEX] = cursor_start >> 17;
	if (pNv->Architecture != NV_ARCH_04)
		regp->CRTC[NV_CIO_CRE_HCUR_ADDR0_INDEX] |= NV_CIO_CRE_HCUR_ASI;
	regp->CRTC[NV_CIO_CRE_HCUR_ADDR1_INDEX] = (cursor_start >> 11) << 2;
	if (crtc->mode.Flags & V_DBLSCAN)
		regp->CRTC[NV_CIO_CRE_HCUR_ADDR1_INDEX] |= NV_CIO_CRE_HCUR_ADDR1_CUR_DBL;
	regp->CRTC[NV_CIO_CRE_HCUR_ADDR2_INDEX] = cursor_start >> 24;

	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR0_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR1_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR2_INDEX);
	if (pNv->Architecture == NV_ARCH_40)
		nv_fix_nv40_hw_cursor(pNv, nv_crtc->head);
}

static void nv_crtc_calc_state_ext(xf86CrtcPtr crtc, DisplayModePtr mode, int dot_clock, struct nouveau_pll_vals *pllvals)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	RIVA_HW_STATE *state = &pNv->ModeReg;
	NVCrtcRegPtr regp = &state->crtc_reg[nv_crtc->head];
	struct pll_lims pll_lim;
	bool using_two_pll_stages = false;
	/* NM2 == 0 is used to determine single stage mode on two stage plls */
	int NM1, NM2 = 0, log2P, vclk;
	uint32_t g70_pll_special_bits = 0;
	uint8_t arbitration0;
	uint16_t arbitration1;

	if (get_pll_limits(pScrn, nv_crtc->head ? VPLL2 : VPLL1, &pll_lim))
		return;

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
	if ((pNv->two_reg_pll || pNv->NVArch == 0x30 || pNv->NVArch == 0x35) &&
	    (pNv->NVArch < 0x41 || dot_clock > (pll_lim.vco1.maxfreq / 2)))
		using_two_pll_stages = true;
	else if (pNv->NVArch > 0x40)
		memset(&pll_lim.vco2, 0, sizeof(pll_lim.vco2));

	vclk = nouveau_bios_getmnp(pScrn, &pll_lim, dot_clock, &NM1, &NM2, &log2P);
	if (!vclk)
		return;

	/* magic bits set by the blob (but not the bios), purpose unknown */
	if (pNv->NVArch == 0x46 || pNv->NVArch == 0x49 || pNv->NVArch == 0x4b)
		g70_pll_special_bits = (using_two_pll_stages ? 0xc : 0x4);

	if (pNv->NVArch == 0x30 || pNv->NVArch == 0x35)
		/* See nvregisters.xml for details. */
		regp->vpll_a = (NM2 & (0x18 << 8)) << 13 | (NM2 & (0x7 << 8)) << 11 | log2P << 16 | NV30_RAMDAC_ENABLE_VCO2 | (NM2 & 7) << 4 | NM1;
	else
		regp->vpll_a = g70_pll_special_bits << 28 | log2P << 16 | NM1;
	regp->vpll_b = NV31_RAMDAC_ENABLE_VCO2 | NM2;

	/* The blob uses this always, so let's do the same */
	if (pNv->Architecture == NV_ARCH_40)
		state->pllsel |= NV_RAMDAC_PLL_SELECT_USE_VPLL2_TRUE;

	/* again nv40 and some nv43 act more like nv3x as described above */
	if (pNv->NVArch < 0x41)
		state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_MPLL |
				 NV_RAMDAC_PLL_SELECT_PLL_SOURCE_NVPLL;

	state->pllsel |= (nv_crtc->head ? NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL2 |
					  NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2 :
					  NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL |
					  NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2);

	if (pNv->NVArch >= 0x40) {
		if (using_two_pll_stages)
			state->reg580 &= (nv_crtc->head ? ~NV_RAMDAC_580_VPLL2_ACTIVE :
							  ~NV_RAMDAC_580_VPLL1_ACTIVE);
		else
			state->reg580 |= (nv_crtc->head ? NV_RAMDAC_580_VPLL2_ACTIVE :
							  NV_RAMDAC_580_VPLL1_ACTIVE);
	}

	if (using_two_pll_stages)
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "vpll: n1 %d n2 %d m1 %d m2 %d log2p %d\n", NM1 >> 8, NM2 >> 8, NM1 & 0xff, NM2 & 0xff, log2P);
	else
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "vpll: n %d m %d log2p %d\n", NM1 >> 8, NM1 & 0xff, log2P);

	pllvals->NM1 = NM1;
	pllvals->NM2 = NM2;
	pllvals->log2P = log2P;

	if (pNv->Architecture < NV_ARCH_30)
		nv4_10UpdateArbitrationSettings(pScrn, vclk, pScrn->bitsPerPixel, &arbitration0, &arbitration1);
	else if ((pNv->Chipset & 0xfff0) == CHIPSET_C51 ||
		 (pNv->Chipset & 0xfff0) == CHIPSET_C512) {
		arbitration0 = 128;
		arbitration1 = 0x0480;
	} else
		nv30UpdateArbitrationSettings(&arbitration0, &arbitration1);

	regp->CRTC[NV_CIO_CRE_FF_INDEX] = arbitration0;
	regp->CRTC[NV_CIO_CRE_FFLWM__INDEX] = arbitration1 & 0xff;
	if (pNv->Architecture >= NV_ARCH_30)
		regp->CRTC[NV_CIO_CRE_47] = arbitration1 >> 8;

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

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Setting dpms mode %d on CRTC %d\n", mode, nv_crtc->head);

	if (nv_crtc->last_dpms == mode) /* Don't do unnecesary mode changes. */
		return;

	nv_crtc->last_dpms = mode;

	if (pNv->twoHeads)
		NVSetOwner(pNv, nv_crtc->head);

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
	* CRTC Controller
	*/
	regp->CRTC[NV_CIO_CR_HDT_INDEX]  = Set8Bits(horizTotal);
	regp->CRTC[NV_CIO_CR_HDE_INDEX]  = Set8Bits(horizDisplay);
	regp->CRTC[NV_CIO_CR_HBS_INDEX]  = Set8Bits(horizBlankStart);
	regp->CRTC[NV_CIO_CR_HBE_INDEX]  = SetBitField(horizBlankEnd,4:0,4:0)
				| SetBit(7);
	regp->CRTC[NV_CIO_CR_HRS_INDEX]  = Set8Bits(horizStart);
	regp->CRTC[NV_CIO_CR_HRE_INDEX]  = SetBitField(horizBlankEnd,5:5,7:7)
				| SetBitField(horizEnd,4:0,4:0);
	regp->CRTC[NV_CIO_CR_VDT_INDEX]  = SetBitField(vertTotal,7:0,7:0);
	regp->CRTC[NV_CIO_CR_OVL_INDEX]  = SetBitField(vertTotal,8:8,0:0)
				| SetBitField(vertDisplay,8:8,1:1)
				| SetBitField(vertStart,8:8,2:2)
				| SetBitField(vertBlankStart,8:8,3:3)
				| SetBit(4)
				| SetBitField(vertTotal,9:9,5:5)
				| SetBitField(vertDisplay,9:9,6:6)
				| SetBitField(vertStart,9:9,7:7);
	regp->CRTC[NV_CIO_CR_RSAL_INDEX]  = 0x00;
	regp->CRTC[NV_CIO_CR_CELL_HT_INDEX]  = SetBitField(vertBlankStart,9:9,5:5)
				| SetBit(6)
				| ((mode->Flags & V_DBLSCAN) ? NV_CIO_CR_CELL_HT_SCANDBL : 0);
	regp->CRTC[NV_CIO_CR_CURS_ST_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_CURS_END_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_SA_HI_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_SA_LO_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_TCOFF_HI_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_TCOFF_LO_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_VRS_INDEX] = Set8Bits(vertStart);
	/* What is the meaning of bit5, it is empty in the vga spec. */
	regp->CRTC[NV_CIO_CR_VRE_INDEX] = SetBitField(vertEnd,3:0,3:0) | SetBit(5);
	regp->CRTC[NV_CIO_CR_VDE_INDEX] = Set8Bits(vertDisplay);
	/* framebuffer can be larger than crtc scanout area. */
	regp->CRTC[NV_CIO_CR_OFFSET_INDEX] = pScrn->displayWidth / 8 * pScrn->bitsPerPixel / 8;
	regp->CRTC[NV_CIO_CR_ULINE_INDEX] = 0x00;
	regp->CRTC[NV_CIO_CR_VBS_INDEX] = Set8Bits(vertBlankStart);
	regp->CRTC[NV_CIO_CR_VBE_INDEX] = Set8Bits(vertBlankEnd);
	regp->CRTC[NV_CIO_CR_MODE_INDEX] = 0x43;
	regp->CRTC[NV_CIO_CR_LCOMP_INDEX] = 0xff;

	/* 
	 * Some extended CRTC registers (they are not saved with the rest of the vga regs).
	 */

	/* framebuffer can be larger than crtc scanout area. */
	regp->CRTC[NV_CIO_CRE_RPC0_INDEX] = ((pScrn->displayWidth / 8 * pScrn->bitsPerPixel / 8) & 0x700) >> 3;
	regp->CRTC[NV_CIO_CRE_RPC1_INDEX] = mode->CrtcHDisplay < 1280 ? 0x04 : 0x00;
	regp->CRTC[NV_CIO_CRE_LSR_INDEX] = SetBitField(horizBlankEnd,6:6,4:4)
				| SetBitField(vertBlankStart,10:10,3:3)
				| SetBitField(vertStart,10:10,2:2)
				| SetBitField(vertDisplay,10:10,1:1)
				| SetBitField(vertTotal,10:10,0:0);

	regp->CRTC[NV_CIO_CRE_HEB__INDEX] = SetBitField(horizTotal,8:8,0:0)
				| SetBitField(horizDisplay,8:8,1:1)
				| SetBitField(horizBlankStart,8:8,2:2)
				| SetBitField(horizStart,8:8,3:3);

	regp->CRTC[NV_CIO_CRE_EBR_INDEX] = SetBitField(vertTotal,11:11,0:0)
				| SetBitField(vertDisplay,11:11,2:2)
				| SetBitField(vertStart,11:11,4:4)
				| SetBitField(vertBlankStart,11:11,6:6);

	if(mode->Flags & V_INTERLACE) {
		horizTotal = (horizTotal >> 1) & ~1;
		regp->CRTC[NV_CIO_CRE_ILACE__INDEX] = Set8Bits(horizTotal);
		regp->CRTC[NV_CIO_CRE_HEB__INDEX] |= SetBitField(horizTotal,8:8,4:4);
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
	bool lvds_output = false, tmds_output = false;
	int i;

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		struct nouveau_encoder *nv_encoder = to_nouveau_encoder(output);

		if (output->crtc == crtc && nv_encoder->dcb->type == OUTPUT_LVDS)
			lvds_output = true;
		if (output->crtc == crtc && nv_encoder->dcb->type == OUTPUT_TMDS)
			tmds_output = true;
	}

	/* Registers not directly related to the (s)vga mode */

	/* bit2 = 0 -> fine pitched crtc granularity */
	/* The rest disables double buffering on CRTC access */
	regp->CRTC[NV_CIO_CRE_21] = 0xfa;

	/* the blob sometimes sets |= 0x10 (which is the same as setting |=
	 * 1 << 30 on 0x60.830), for no apparent reason */
	regp->CRTC[NV_CIO_CRE_59] = 0x0;
	if (tmds_output && pNv->Architecture < NV_ARCH_40)
		regp->CRTC[NV_CIO_CRE_59] |= 0x1;

	/* What is the meaning of this register? */
	/* A few popular values are 0x18, 0x1c, 0x38, 0x3c */ 
	regp->CRTC[NV_CIO_CRE_ENH_INDEX] = savep->CRTC[NV_CIO_CRE_ENH_INDEX] & ~(1<<5);

	regp->head = 0;
	/* Except for rare conditions I2C is enabled on the primary crtc */
	if (nv_crtc->head == 0)
		regp->head |= NV_CRTC_FSEL_I2C;
	/* Set overlay to desired crtc. */
	if (pNv->overlayAdaptor) {
		NVPortPrivPtr pPriv = GET_OVERLAY_PRIVATE(pNv);
		if (pPriv->overlayCRTC == nv_crtc->head)
			regp->head |= NV_CRTC_FSEL_OVERLAY;
	}

	/* This is not what nv does, but it is what the blob does (for nv4x at least) */
	/* This fixes my cursor corruption issue */
	regp->cursorConfig = 0x0;
	if(mode->Flags & V_DBLSCAN)
		regp->cursorConfig |= NV_CRTC_CURSOR_CONFIG_DOUBLE_SCAN;
	if (pNv->alphaCursor) {
		regp->cursorConfig |= NV_CRTC_CURSOR_CONFIG_32BPP |
				      NV_CRTC_CURSOR_CONFIG_64PIXELS |
				      NV_CRTC_CURSOR_CONFIG_64LINES |
				      NV_CRTC_CURSOR_CONFIG_ALPHA_BLEND;
	} else
		regp->cursorConfig |= NV_CRTC_CURSOR_CONFIG_32LINES;

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

	regp->CRTC[NV_CIO_CRE_CSB] = 0x80;

	/* probably a scratch reg, but kept for cargo-cult purposes:
	 * bit0: crtc0?, head A
	 * bit6: lvds, head A
	 * bit7: (only in X), head A
	 */
	if (nv_crtc->head == 0)
		regp->CRTC[NV_CIO_CRE_4B] = savep->CRTC[NV_CIO_CRE_4B] | 0x80;

	/* The blob seems to take the current value from crtc 0, add 4 to that
	 * and reuse the old value for crtc 1 */
	regp->CRTC[NV_CIO_CRE_52] = pNv->SavedReg.crtc_reg[0].CRTC[NV_CIO_CRE_52];
	if (!nv_crtc->head)
		regp->CRTC[NV_CIO_CRE_52] += 4;

	regp->unk830 = mode->CrtcVDisplay - 3;
	regp->unk834 = mode->CrtcVDisplay - 1;

	if (pNv->twoHeads)
		/* This is what the blob does */
		regp->unk850 = NVReadCRTC(pNv, 0, NV_CRTC_0850);

	/* Never ever modify gpio, unless you know very well what you're doing */
	regp->gpio = NVReadCRTC(pNv, 0, NV_CRTC_GPIO);

	if (pNv->twoHeads)
		regp->gpio_ext = NVReadCRTC(pNv, 0, NV_CRTC_GPIO_EXT);

	regp->config = NV_PCRTC_CONFIG_START_ADDRESS_HSYNC;

	/* Some misc regs */
	if (pNv->Architecture == NV_ARCH_40) {
		regp->CRTC[NV_CIO_CRE_85] = 0xFF;
		regp->CRTC[NV_CIO_CRE_86] = 0x1;
	}

	regp->CRTC[NV_CIO_CRE_PIXEL_INDEX] = (pScrn->depth + 1) / 8;
	/* Enable slaved mode */
	if (lvds_output || tmds_output)
		regp->CRTC[NV_CIO_CRE_PIXEL_INDEX] |= (1 << 7);

	/* Generic PRAMDAC regs */

	if (pNv->Architecture >= NV_ARCH_10)
		/* Only bit that bios and blob set. */
		regp->nv10_cursync = (1 << 25);

	switch (pScrn->depth) {
		case 24:
		case 15:
			regp->general = 0x00100130;
			break;
		case 16:
		default:
			regp->general = 0x00101130;
			break;
	}
	if (pNv->alphaCursor)
		/* PIPE_LONG mode, something to do with the size of the cursor? */
		regp->general |= 1 << 29;

	regp->unk_630 = 0; /* turn off green mode (tv test pattern?) */

	/* Some values the blob sets */
	regp->unk_a20 = 0x0;
	regp->unk_a24 = 0xfffff;
	regp->unk_a34 = 0x1;
}

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
		/* assuming one fp output per crtc seems ok */
		nv_encoder = to_nouveau_encoder(xf86_config->output[i]);
		if (xf86_config->output[i]->crtc != crtc)
			continue;

		if (nv_encoder->dcb->type == OUTPUT_LVDS ||
		    nv_encoder->dcb->type == OUTPUT_TMDS)
			break;
	}
	if (i == xf86_config->num_output)
		return;

	regp->fp_horiz_regs[REG_DISP_END] = adjusted_mode->HDisplay - 1;
	regp->fp_horiz_regs[REG_DISP_TOTAL] = adjusted_mode->HTotal - 1;
	if ((adjusted_mode->HSyncStart - adjusted_mode->HDisplay) >= pNv->vbios->digital_min_front_porch)
		regp->fp_horiz_regs[REG_DISP_CRTC] = adjusted_mode->HDisplay;
	else
		regp->fp_horiz_regs[REG_DISP_CRTC] = adjusted_mode->HSyncStart - pNv->vbios->digital_min_front_porch - 1;
	regp->fp_horiz_regs[REG_DISP_SYNC_START] = adjusted_mode->HSyncStart - 1;
	regp->fp_horiz_regs[REG_DISP_SYNC_END] = adjusted_mode->HSyncEnd - 1;
	regp->fp_horiz_regs[REG_DISP_VALID_START] = adjusted_mode->HSkew;
	regp->fp_horiz_regs[REG_DISP_VALID_END] = adjusted_mode->HDisplay - 1;

	regp->fp_vert_regs[REG_DISP_END] = adjusted_mode->VDisplay - 1;
	regp->fp_vert_regs[REG_DISP_TOTAL] = adjusted_mode->VTotal - 1;
	regp->fp_vert_regs[REG_DISP_CRTC] = adjusted_mode->VTotal - 5 - 1;
	regp->fp_vert_regs[REG_DISP_SYNC_START] = adjusted_mode->VSyncStart - 1;
	regp->fp_vert_regs[REG_DISP_SYNC_END] = adjusted_mode->VSyncEnd - 1;
	regp->fp_vert_regs[REG_DISP_VALID_START] = 0;
	regp->fp_vert_regs[REG_DISP_VALID_END] = adjusted_mode->VDisplay - 1;

	/*
	* bit0: positive vsync
	* bit4: positive hsync
	* bit8: enable center mode
	* bit9: enable native mode
	* bit24: 12/24 bit interface (12bit=on, 24bit=off)
	* bit26: a bit sometimes seen on some g70 cards
	* bit28: fp display enable bit
	* bit31: set for dual link
	*/

	regp->fp_control = (savep->fp_control & 0x04100000) |
			   NV_RAMDAC_FP_CONTROL_DISPEN_POS;

	/* Deal with vsync/hsync polarity */
	/* LVDS screens do set this, but modes with +ve syncs are very rare */
	if (adjusted_mode->Flags & V_PVSYNC)
		regp->fp_control |= NV_RAMDAC_FP_CONTROL_VSYNC_POS;
	if (adjusted_mode->Flags & V_PHSYNC)
		regp->fp_control |= NV_RAMDAC_FP_CONTROL_HSYNC_POS;

	/* panel scaling first, as native would get set otherwise */
	if (nv_encoder->scaling_mode == SCALE_PANEL ||
	    nv_encoder->scaling_mode == SCALE_NOSCALE)	/* panel handles it */
		regp->fp_control |= NV_RAMDAC_FP_CONTROL_MODE_CENTER;
	else if (mode->HDisplay == adjusted_mode->HDisplay &&
		 mode->VDisplay == adjusted_mode->VDisplay) /* native mode */
		regp->fp_control |= NV_RAMDAC_FP_CONTROL_MODE_NATIVE;
	else /* gpu needs to scale */
		regp->fp_control |= NV_RAMDAC_FP_CONTROL_MODE_SCALE;

	if (nvReadEXTDEV(pNv, NV_PEXTDEV_BOOT_0) & NV_PEXTDEV_BOOT_0_STRAP_FP_IFACE_12BIT)
		regp->fp_control |= NV_RAMDAC_FP_CONTROL_WIDTH_12;

	if (nv_encoder->dual_link)
		regp->fp_control |= (8 << 28);

	/* Use the generic value, and enable x-scaling, y-scaling, and the TMDS enable bit */
	regp->debug_0 = 0x01101191;
	/* We want automatic scaling */
	regp->debug_1 = 0;
	/* This can override HTOTAL and VTOTAL */
	regp->debug_2 = 0;

	/* Use 20.12 fixed point format to avoid floats */
	mode_ratio = (1 << 12) * mode->HDisplay / mode->VDisplay;
	panel_ratio = (1 << 12) * adjusted_mode->HDisplay / adjusted_mode->VDisplay;
	/* if ratios are equal, SCALE_ASPECT will automatically (and correctly)
	 * get treated the same as SCALE_FULLSCREEN */
	if (nv_encoder->scaling_mode == SCALE_ASPECT && mode_ratio != panel_ratio) {
		uint32_t diff, scale;

		if (mode_ratio < panel_ratio) {
			/* vertical needs to expand to glass size (automatic)
			 * horizontal needs to be scaled at vertical scale factor
			 * to maintain aspect */
	
			scale = (1 << 12) * mode->VDisplay / adjusted_mode->VDisplay;
			regp->debug_1 = 1 << 12 | ((scale >> 1) & 0xfff);

			/* restrict area of screen used, horizontally */
			diff = adjusted_mode->HDisplay -
			       adjusted_mode->VDisplay * mode_ratio / (1 << 12);
			regp->fp_horiz_regs[REG_DISP_VALID_START] += diff / 2;
			regp->fp_horiz_regs[REG_DISP_VALID_END] -= diff / 2;
		}

		if (mode_ratio > panel_ratio) {
			/* horizontal needs to expand to glass size (automatic)
			 * vertical needs to be scaled at horizontal scale factor
			 * to maintain aspect */

			scale = (1 << 12) * mode->HDisplay / adjusted_mode->HDisplay;
			regp->debug_1 = 1 << 28 | ((scale >> 1) & 0xfff) << 16;
			
			/* restrict area of screen used, vertically */
			diff = adjusted_mode->VDisplay -
			       (1 << 12) * adjusted_mode->HDisplay / mode_ratio;
			regp->fp_vert_regs[REG_DISP_VALID_START] += diff / 2;
			regp->fp_vert_regs[REG_DISP_VALID_END] -= diff / 2;
		}
	}

	/* Flatpanel support needs at least a NV10 */
	if (pNv->twoHeads) {
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
	} else
		regp->dither = savep->dither;
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
	struct nouveau_pll_vals pllvals;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CTRC mode on CRTC %d:\n", nv_crtc->head);
	xf86PrintModeline(pScrn->scrnIndex, mode);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Output mode on CRTC %d:\n", nv_crtc->head);
	xf86PrintModeline(pScrn->scrnIndex, adjusted_mode);

	nv_crtc_mode_set_vga(crtc, mode, adjusted_mode);

	/* calculated in output_prepare, nv40 needs it written before calculating PLLs */
	if (pNv->Architecture == NV_ARCH_40)
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK, pNv->ModeReg.sel_clk);
	nv_crtc_mode_set_regs(crtc, mode);
	nv_crtc_mode_set_fp_regs(crtc, mode, adjusted_mode);
	nv_crtc_calc_state_ext(crtc, mode, adjusted_mode->Clock, &pllvals);

	NVVgaProtect(pNv, nv_crtc->head, true);
	nv_crtc_load_state_ramdac(crtc, &pNv->ModeReg);
	nv_crtc_load_state_ext(crtc, &pNv->ModeReg);
	nv_crtc_load_state_palette(crtc, &pNv->ModeReg);
	nv_crtc_load_state_vga(crtc, &pNv->ModeReg);
	nv_crtc_load_state_pll(crtc, &pNv->ModeReg, &pllvals);

	NVVgaProtect(pNv, nv_crtc->head, false);

	NVCrtcSetBase(crtc, x, y);

#if X_BYTE_ORDER == X_BIG_ENDIAN
	/* turn on LFB swapping */
	{
		uint8_t tmp = NVReadVgaCrtc(pNv, nv_crtc->head, NV_CIO_CRE_RCR);
		tmp |= NV_CIO_CRE_RCR_ENDIAN_BIG;
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

	nv_crtc_save_state_ramdac(crtc, &pNv->SavedReg);
	nv_crtc_save_state_vga(crtc, &pNv->SavedReg);
	nv_crtc_save_state_palette(crtc, &pNv->SavedReg);
	nv_crtc_save_state_ext(crtc, &pNv->SavedReg);
	nv_crtc_save_state_pll(crtc, &pNv->SavedReg);

	/* init some state to saved value */
	pNv->ModeReg.reg580 = pNv->SavedReg.reg580;
	pNv->ModeReg.sel_clk = pNv->SavedReg.sel_clk & ~(0x5 << 16);
	pNv->ModeReg.crtc_reg[nv_crtc->head].CRTC[NV_CIO_CRE_LCD__INDEX] = pNv->SavedReg.crtc_reg[nv_crtc->head].CRTC[NV_CIO_CRE_LCD__INDEX];
}

static void nv_crtc_restore(xf86CrtcPtr crtc)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(crtc->scrn);

	if (pNv->twoHeads)
		NVSetOwner(pNv, nv_crtc->head);

	NVVgaProtect(pNv, nv_crtc->head, true);
	nv_crtc_load_state_ramdac(crtc, &pNv->SavedReg);
	nv_crtc_load_state_ext(crtc, &pNv->SavedReg);
	nv_crtc_load_state_palette(crtc, &pNv->SavedReg);
	nv_crtc_load_state_vga(crtc, &pNv->SavedReg);
	nv_crtc_load_state_pll(crtc, &pNv->SavedReg, &pNv->SavedReg.crtc_reg[nv_crtc->head].pllvals);
	NVVgaProtect(pNv, nv_crtc->head, false);

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
	NVCrtcWriteCRTC(crtc, NV_CRTC_CONFIG, NV_PCRTC_CONFIG_START_ADDRESS_NON_VGA);
	if (pNv->Architecture == NV_ARCH_40) {
		uint32_t reg900 = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_900);
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_900, reg900 & ~0x10000);
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

	nv_crtc_load_state_palette(crtc, &pNv->ModeReg);
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
	if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_PIN,
			align, size, &nv_crtc->shadow)) {
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
	offset = pNv->FB->map + nv_crtc->shadow->offset;
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
	.set_cursor_colors = NULL, /* Alpha cursors do not need this */
	.set_cursor_position = nv_crtc_set_cursor_position,
	.show_cursor = nv_crtc_show_cursor,
	.hide_cursor = nv_crtc_hide_cursor,
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

	/* NV04-NV10 doesn't support alpha cursors */
	if (pNv->NVArch < 0x11) {
		crtcfuncs.set_cursor_colors = nv_crtc_set_cursor_colors;
		crtcfuncs.load_cursor_image = nv_crtc_load_cursor_image;
		crtcfuncs.load_cursor_argb = NULL;
	}
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

static void nv_crtc_load_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	int i;
	NVCrtcRegPtr regp = &state->crtc_reg[nv_crtc->head];

	NVWritePRMVIO(pNv, nv_crtc->head, NV_PRMVIO_MISC__WRITE, regp->MiscOutReg);

	for (i = 0; i < 5; i++)
		NVWriteVgaSeq(pNv, nv_crtc->head, i, regp->Sequencer[i]);

	nv_lock_vga_crtc_base(pNv, nv_crtc->head, false);
	for (i = 0; i < 25; i++)
		crtc_wr_cio_state(crtc, regp, i);
	nv_lock_vga_crtc_base(pNv, nv_crtc->head, true);

	for (i = 0; i < 9; i++)
		NVWriteVgaGr(pNv, nv_crtc->head, i, regp->Graphics[i]);

	NVSetEnablePalette(pNv, nv_crtc->head, true);
	for (i = 0; i < 21; i++)
		NVWriteVgaAttr(pNv, nv_crtc->head, i, regp->Attribute[i]);

	NVSetEnablePalette(pNv, nv_crtc->head, false);
}

static void nv_crtc_load_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);    
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVCrtcRegPtr regp;
	int i;

	regp = &state->crtc_reg[nv_crtc->head];

	if (pNv->Architecture >= NV_ARCH_10) {
		if (pNv->twoHeads)
			/* setting FSEL *must* come before CIO_CRE_LCD, as writing CIO_CRE_LCD sets some
			 * bits (16 & 17) in FSEL that should not be overwritten by writing FSEL */
			NVCrtcWriteCRTC(crtc, NV_CRTC_FSEL, regp->head);

		nvWriteVIDEO(pNv, NV_PVIDEO_STOP, 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_INTR_EN, 0);
		nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(0), 0);
		nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(1), 0);
		nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(0), pNv->VRAMPhysicalSize - 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(1), pNv->VRAMPhysicalSize - 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(0), pNv->VRAMPhysicalSize - 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(1), pNv->VRAMPhysicalSize - 1);
		nvWriteMC(pNv, NV_PBUS_POWERCTRL_2, 0);

		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_21);
		NVCrtcWriteCRTC(crtc, NV_CRTC_CURSOR_CONFIG, regp->cursorConfig);
		NVCrtcWriteCRTC(crtc, NV_CRTC_0830, regp->unk830);
		NVCrtcWriteCRTC(crtc, NV_CRTC_0834, regp->unk834);
		if (pNv->Architecture == NV_ARCH_40) {
			NVCrtcWriteCRTC(crtc, NV_CRTC_0850, regp->unk850);
			NVCrtcWriteCRTC(crtc, NV_CRTC_GPIO_EXT, regp->gpio_ext);
		}

		if (pNv->Architecture == NV_ARCH_40) {
			uint32_t reg900 = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_900);
			if (regp->config == NV_PCRTC_CONFIG_START_ADDRESS_HSYNC)
				NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_900, reg900 | 0x10000);
			else
				NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_900, reg900 & ~0x10000);
		}
	}

	NVCrtcWriteCRTC(crtc, NV_CRTC_CONFIG, regp->config);
	NVCrtcWriteCRTC(crtc, NV_CRTC_GPIO, regp->gpio);

	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_RPC0_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_RPC1_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_LSR_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_PIXEL_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_LCD__INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_HEB__INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_ENH_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_FF_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_FFLWM__INDEX);
	if (pNv->Architecture >= NV_ARCH_30)
		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_47);

	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR0_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR1_INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR2_INDEX);
	if (pNv->Architecture == NV_ARCH_40)
		nv_fix_nv40_hw_cursor(pNv, nv_crtc->head);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_ILACE__INDEX);

	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_SCRATCH3__INDEX);
	crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_SCRATCH4__INDEX);
	if (pNv->Architecture >= NV_ARCH_10) {
		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_EBR_INDEX);
		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_CSB);
		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_4B);
		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_52);
	}
	/* NV11 and NV20 stop at 0x52. */
	if (pNv->NVArch >= 0x17 && pNv->twoHeads) {
		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_53);
		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_54);

		for (i = 0; i < 0x10; i++)
			NVWriteVgaCrtc5758(pNv, nv_crtc->head, i, regp->CR58[i]);
		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_59);

		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_85);
		crtc_wr_cio_state(crtc, regp, NV_CIO_CRE_86);
	}

	NVCrtcWriteCRTC(crtc, NV_CRTC_START, regp->fb_start);

	/* Setting 1 on this value gives you interrupts for every vblank period. */
	NVCrtcWriteCRTC(crtc, NV_CRTC_INTR_EN_0, 0);
	NVCrtcWriteCRTC(crtc, NV_CRTC_INTR_0, NV_CRTC_INTR_VBLANK);
}

static void nv_crtc_save_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	int i;
	NVCrtcRegPtr regp = &state->crtc_reg[nv_crtc->head];

	regp->MiscOutReg = NVReadPRMVIO(pNv, nv_crtc->head, NV_PRMVIO_MISC__READ);

	for (i = 0; i < 25; i++)
		crtc_rd_cio_state(crtc, regp, i);

	NVSetEnablePalette(pNv, nv_crtc->head, true);
	for (i = 0; i < 21; i++)
		regp->Attribute[i] = NVReadVgaAttr(pNv, nv_crtc->head, i);
	NVSetEnablePalette(pNv, nv_crtc->head, false);

	for (i = 0; i < 9; i++)
		regp->Graphics[i] = NVReadVgaGr(pNv, nv_crtc->head, i);

	for (i = 0; i < 5; i++)
		regp->Sequencer[i] = NVReadVgaSeq(pNv, nv_crtc->head, i);
}

static void nv_crtc_save_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVCrtcRegPtr regp;
	int i;

	regp = &state->crtc_reg[nv_crtc->head];

	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_LCD__INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_RPC0_INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_RPC1_INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_LSR_INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_PIXEL_INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_HEB__INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_ENH_INDEX);

	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_FF_INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_FFLWM__INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_21);
	if (pNv->Architecture >= NV_ARCH_30)
		crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_47);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR0_INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR1_INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_HCUR_ADDR2_INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_ILACE__INDEX);

	if (pNv->Architecture >= NV_ARCH_10) {
		regp->unk830 = NVCrtcReadCRTC(crtc, NV_CRTC_0830);
		regp->unk834 = NVCrtcReadCRTC(crtc, NV_CRTC_0834);
		if (pNv->Architecture == NV_ARCH_40) {
			regp->unk850 = NVCrtcReadCRTC(crtc, NV_CRTC_0850);
			regp->gpio_ext = NVCrtcReadCRTC(crtc, NV_CRTC_GPIO_EXT);
		}
		if (pNv->twoHeads)
			regp->head = NVCrtcReadCRTC(crtc, NV_CRTC_FSEL);
		regp->cursorConfig = NVCrtcReadCRTC(crtc, NV_CRTC_CURSOR_CONFIG);
	}

	regp->gpio = NVCrtcReadCRTC(crtc, NV_CRTC_GPIO);
	regp->config = NVCrtcReadCRTC(crtc, NV_CRTC_CONFIG);

	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_SCRATCH3__INDEX);
	crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_SCRATCH4__INDEX);
	if (pNv->Architecture >= NV_ARCH_10) {
		crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_EBR_INDEX);
		crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_CSB);
		crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_4B);
		crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_52);
	}
	/* NV11 and NV20 don't have this, they stop at 0x52. */
	if (pNv->NVArch >= 0x17 && pNv->twoHeads) {
		for (i = 0; i < 0x10; i++)
			regp->CR58[i] = NVReadVgaCrtc5758(pNv, nv_crtc->head, i);

		crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_59);
		crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_53);
		crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_54);

		crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_85);
		crtc_rd_cio_state(crtc, regp, NV_CIO_CRE_86);
	}

	regp->fb_start = NVCrtcReadCRTC(crtc, NV_CRTC_START);
}

static void nv_crtc_save_state_ramdac(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);    
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVCrtcRegPtr regp;
	int i;

	regp = &state->crtc_reg[nv_crtc->head];

	regp->general = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_GENERAL_CONTROL);

	if (pNv->twoHeads) {
		if (pNv->NVArch >= 0x17)
			regp->unk_630 = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_630);
		regp->fp_control	= NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_CONTROL);
		regp->debug_0	= NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_DEBUG_0);
		regp->debug_1	= NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_DEBUG_1);
		regp->debug_2	= NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_DEBUG_2);

		regp->unk_a20 = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_A20);
		regp->unk_a24 = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_A24);
		regp->unk_a34 = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_A34);
	}

	if (pNv->NVArch == 0x11) {
		regp->dither = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_DITHER_NV11);
	} else if (pNv->twoHeads) {
		regp->dither = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_DITHER);
		for (i = 0; i < 3; i++) {
			regp->dither_regs[i] = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_850 + i * 4);
			regp->dither_regs[i + 3] = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_85C + i * 4);
		}
	}
	if (pNv->Architecture >= NV_ARCH_10)
		regp->nv10_cursync = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_NV10_CURSYNC);

	/* The regs below are 0 for non-flatpanels, so you can load and save them */

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_HDISP_END + (i * 4);
		regp->fp_horiz_regs[i] = NVCrtcReadRAMDAC(crtc, ramdac_reg);
	}

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_VDISP_END + (i * 4);
		regp->fp_vert_regs[i] = NVCrtcReadRAMDAC(crtc, ramdac_reg);
	}
}

static void nv_crtc_load_state_ramdac(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);    
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVCrtcRegPtr regp;
	int i;

	regp = &state->crtc_reg[nv_crtc->head];

	NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_GENERAL_CONTROL, regp->general);

	if (pNv->twoHeads) {
		if (pNv->NVArch >= 0x17)
			NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_630, regp->unk_630);
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_CONTROL, regp->fp_control);
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_DEBUG_0, regp->debug_0);
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_DEBUG_1, regp->debug_1);
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_DEBUG_2, regp->debug_2);
		if (pNv->NVArch == 0x30) { /* For unknown purposes. */
			uint32_t reg890 = NVCrtcReadRAMDAC(crtc, NV30_RAMDAC_890);
			NVCrtcWriteRAMDAC(crtc, NV30_RAMDAC_89C, reg890);
		}

		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_A20, regp->unk_a20);
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_A24, regp->unk_a24);
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_A34, regp->unk_a34);
	}

	if (pNv->NVArch == 0x11)
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_DITHER_NV11, regp->dither);
	else if (pNv->twoHeads) {
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_DITHER, regp->dither);
		for (i = 0; i < 3; i++) {
			NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_850 + i * 4, regp->dither_regs[i]);
			NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_85C + i * 4, regp->dither_regs[i + 3]);
		}
	}
	if (pNv->Architecture >= NV_ARCH_10)
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_NV10_CURSYNC, regp->nv10_cursync);

	/* The regs below are 0 for non-flatpanels, so you can load and save them */

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_HDISP_END + (i * 4);
		NVCrtcWriteRAMDAC(crtc, ramdac_reg, regp->fp_horiz_regs[i]);
	}

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_VDISP_END + (i * 4);
		NVCrtcWriteRAMDAC(crtc, ramdac_reg, regp->fp_vert_regs[i]);
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

	/* 30 bits addresses in 32 bits according to haiku */
	start &= ~3;
	pNv->ModeReg.crtc_reg[nv_crtc->head].fb_start = start;
	NVCrtcWriteCRTC(crtc, NV_CRTC_START, start);

	crtc->x = x;
	crtc->y = y;
}

static void nv_crtc_save_state_palette(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(crtc->scrn);
	int head_offset = nv_crtc->head * NV_PRMDIO_SIZE, i;

	VGA_WR08(pNv->REGS, NV_PRMDIO_PIXEL_MASK + head_offset, NV_PRMDIO_PIXEL_MASK_MASK);
	VGA_WR08(pNv->REGS, NV_PRMDIO_READ_MODE_ADDRESS + head_offset, 0x0);

	for (i = 0; i < 768; i++) {
		state->crtc_reg[nv_crtc->head].DAC[i] = NV_RD08(pNv->REGS, NV_PRMDIO_PALETTE_DATA + head_offset);
		DDXMMIOH("nv_crtc_save_state_palette: head %d reg 0x%04x data 0x%02x\n", nv_crtc->head, NV_PRMDIO_PALETTE_DATA + head_offset, state->crtc_reg[nv_crtc->head].DAC[i]);
	}

	NVSetEnablePalette(pNv, nv_crtc->head, false);
}
static void nv_crtc_load_state_palette(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	struct nouveau_crtc *nv_crtc = to_nouveau_crtc(crtc);
	NVPtr pNv = NVPTR(crtc->scrn);
	int head_offset = nv_crtc->head * NV_PRMDIO_SIZE, i;

	VGA_WR08(pNv->REGS, NV_PRMDIO_PIXEL_MASK + head_offset, NV_PRMDIO_PIXEL_MASK_MASK);
	VGA_WR08(pNv->REGS, NV_PRMDIO_WRITE_MODE_ADDRESS + head_offset, 0x0);

	for (i = 0; i < 768; i++) {
		DDXMMIOH("nv_crtc_load_state_palette: head %d reg 0x%04x data 0x%02x\n", nv_crtc->head, NV_PRMDIO_PALETTE_DATA + head_offset, state->crtc_reg[nv_crtc->head].DAC[i]);
		NV_WR08(pNv->REGS, NV_PRMDIO_PALETTE_DATA + head_offset, state->crtc_reg[nv_crtc->head].DAC[i]);
	}

	NVSetEnablePalette(pNv, nv_crtc->head, false);
}
