/*
 * Copyright 2006 Dave Airlie
 * Copyright 2007 Maarten Maathuis
 * Copyright 2007-2009 Stuart Bennett
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nv_include.h"

/*
 * misc hw access wrappers/control functions
 */

void NVWriteVgaSeq(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	NVWritePRMVIO(pNv, head, NV_PRMVIO_SRX, index);
	NVWritePRMVIO(pNv, head, NV_PRMVIO_SR, value);
}

uint8_t NVReadVgaSeq(NVPtr pNv, int head, uint8_t index)
{
	NVWritePRMVIO(pNv, head, NV_PRMVIO_SRX, index);
	return NVReadPRMVIO(pNv, head, NV_PRMVIO_SR);
}

void NVWriteVgaGr(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	NVWritePRMVIO(pNv, head, NV_PRMVIO_GRX, index);
	NVWritePRMVIO(pNv, head, NV_PRMVIO_GX, value);
}

uint8_t NVReadVgaGr(NVPtr pNv, int head, uint8_t index)
{
	NVWritePRMVIO(pNv, head, NV_PRMVIO_GRX, index);
	return NVReadPRMVIO(pNv, head, NV_PRMVIO_GX);
}

/* CR44 takes values 0 (head A), 3 (head B) and 4 (heads tied)
 * it affects only the 8 bit vga io regs, which we access using mmio at
 * 0xc{0,2}3c*, 0x60{1,3}3*, and 0x68{1,3}3d*
 * in general, the set value of cr44 does not matter: reg access works as
 * expected and values can be set for the appropriate head by using a 0x2000
 * offset as required
 * however:
 * a) pre nv40, the head B range of PRMVIO regs at 0xc23c* was not exposed and
 *    cr44 must be set to 0 or 3 for accessing values on the correct head
 *    through the common 0xc03c* addresses
 * b) in tied mode (4) head B is programmed to the values set on head A, and
 *    access using the head B addresses can have strange results, ergo we leave
 *    tied mode in init once we know to what cr44 should be restored on exit
 *
 * the owner parameter is slightly abused:
 * 0 and 1 are treated as head values and so the set value is (owner * 3)
 * other values are treated as literal values to set
 */
void NVSetOwner(NVPtr pNv, int owner)
{
	if (owner == 1)
		owner *= 3;
	/* CR44 is always changed on CRTC0 */
	NVWriteVgaCrtc(pNv, 0, NV_CIO_CRE_44, owner);
	if (pNv->NVArch == 0x11) {	/* set me harder */
		NVWriteVgaCrtc(pNv, 0, NV_CIO_CRE_2E, owner);
		NVWriteVgaCrtc(pNv, 0, NV_CIO_CRE_2E, owner);
	}
}

/*
 * on nv11 this may not be reliable
 * returned value is suitable for directly programming back into cr44
 */
int nouveau_hw_get_current_head(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int cr44;

	if (pNv->NVArch != 0x11)
		return NVReadVgaCrtc(pNv, 0, NV_CIO_CRE_44);

	/* reading CR44 is broken on nv11, so we attempt to infer it */
	if (nvReadMC(pNv, NV_PBUS_DEBUG_1) & (1 << 28))	/* heads tied, restore both */
		cr44 = 0x4;
	else {
		bool waslocked, slaved_on_A, tvA, slaved_on_B, tvB;

		waslocked = NVLockVgaCrtcs(pNv, false);

		slaved_on_A = NVReadVgaCrtc(pNv, 0, NV_CIO_CRE_PIXEL_INDEX) & 0x80;
		if (slaved_on_A)
			tvA = !(NVReadVgaCrtc(pNv, 0, NV_CIO_CRE_LCD__INDEX) & MASK(NV_CIO_CRE_LCD_LCD_SELECT));

		slaved_on_B = NVReadVgaCrtc(pNv, 1, NV_CIO_CRE_PIXEL_INDEX) & 0x80;
		if (slaved_on_B)
			tvB = !(NVReadVgaCrtc(pNv, 1, NV_CIO_CRE_LCD__INDEX) & MASK(NV_CIO_CRE_LCD_LCD_SELECT));

		if (waslocked)
			NVLockVgaCrtcs(pNv, true);

		if (slaved_on_A && !tvA)
			cr44 = 0x0;
		else if (slaved_on_B && !tvB)
			cr44 = 0x3;
		else if (slaved_on_A)
			cr44 = 0x0;
		else if (slaved_on_B)
			cr44 = 0x3;
		else
			cr44 = 0x0;
	}

	return cr44;
}

void NVBlankScreen(NVPtr pNv, int head, bool blank)
{
	unsigned char seq1;

	if (pNv->twoHeads)
		NVSetOwner(pNv, head);

	seq1 = NVReadVgaSeq(pNv, head, NV_VIO_SR_CLOCK_INDEX);

	NVVgaSeqReset(pNv, head, true);
	if (blank)
		NVWriteVgaSeq(pNv, head, NV_VIO_SR_CLOCK_INDEX, seq1 | 0x20);
	else
		NVWriteVgaSeq(pNv, head, NV_VIO_SR_CLOCK_INDEX, seq1 & ~0x20);
	NVVgaSeqReset(pNv, head, false);
}

/*
 * PLL setting
 */

static int powerctrl_1_shift(int chip_version, int reg)
{
	int shift = -4;

	if (chip_version < 0x17 || chip_version == 0x1a || chip_version == 0x20)
		return shift;

	switch (reg) {
	case NV_RAMDAC_VPLL2:
		shift += 4;
	case NV_PRAMDAC_VPLL_COEFF:
		shift += 4;
	case NV_PRAMDAC_MPLL_COEFF:
		shift += 4;
	case NV_PRAMDAC_NVPLL_COEFF:
		shift += 4;
	}

	/*
	 * the shift for vpll regs is only used for nv3x chips with a single
	 * stage pll
	 */
	if (shift > 4 && (chip_version < 0x32 || chip_version == 0x35 ||
			  chip_version == 0x36 || chip_version >= 0x40))
		shift = -4;

	return shift;
}

static void setPLL_single(ScrnInfoPtr pScrn, uint32_t reg,
			  struct nouveau_pll_vals *pv)
{
	NVPtr pNv = NVPTR(pScrn);
	int chip_version = pNv->vbios->chip_version;
	uint32_t oldpll = NVReadRAMDAC(pNv, 0, reg);
	int oldN = (oldpll >> 8) & 0xff, oldM = oldpll & 0xff;
	uint32_t pll = (oldpll & 0xfff80000) | pv->log2P << 16 | pv->NM1;
	uint32_t saved_powerctrl_1 = 0;
	int shift_powerctrl_1 = powerctrl_1_shift(chip_version, reg);

	if (oldpll == pll)
		return;	/* already set */

	if (shift_powerctrl_1 >= 0) {
		saved_powerctrl_1 = nvReadMC(pNv, NV_PBUS_POWERCTRL_1);
		nvWriteMC(pNv, NV_PBUS_POWERCTRL_1,
			(saved_powerctrl_1 & ~(0xf << shift_powerctrl_1)) |
			1 << shift_powerctrl_1);
	}

	if (oldM && pv->M1 && (oldN / oldM < pv->N1 / pv->M1))
		/* upclock -- write new post divider first */
		NVWriteRAMDAC(pNv, 0, reg, pv->log2P << 16 | (oldpll & 0xffff));
	else
		/* downclock -- write new NM first */
		NVWriteRAMDAC(pNv, 0, reg, (oldpll & 0xffff0000) | pv->NM1);

	if (chip_version < 0x17 && chip_version != 0x11)
		/* wait a bit on older chips */
		usleep(64000);
	NVReadRAMDAC(pNv, 0, reg);

	/* then write the other half as well */
	NVWriteRAMDAC(pNv, 0, reg, pll);

	if (shift_powerctrl_1 >= 0)
		nvWriteMC(pNv, NV_PBUS_POWERCTRL_1, saved_powerctrl_1);
}

static uint32_t new_ramdac580(uint32_t reg1, bool ss, uint32_t ramdac580)
{
	bool head_a = (reg1 == NV_PRAMDAC_VPLL_COEFF);

	if (ss)	/* single stage pll mode */
		ramdac580 |= head_a ? NV_RAMDAC_580_VPLL1_ACTIVE :
				      NV_RAMDAC_580_VPLL2_ACTIVE;
	else
		ramdac580 &= head_a ? ~NV_RAMDAC_580_VPLL1_ACTIVE :
				      ~NV_RAMDAC_580_VPLL2_ACTIVE;

	return ramdac580;
}

static void setPLL_double_highregs(ScrnInfoPtr pScrn, uint32_t reg1,
				   struct nouveau_pll_vals *pv)
{
	NVPtr pNv = NVPTR(pScrn);
	int chip_version = pNv->vbios->chip_version;
	bool nv3035 = chip_version == 0x30 || chip_version == 0x35;
	uint32_t reg2 = reg1 + ((reg1 == NV_RAMDAC_VPLL2) ? 0x5c : 0x70);
	uint32_t oldpll1 = NVReadRAMDAC(pNv, 0, reg1);
	uint32_t oldpll2 = !nv3035 ? NVReadRAMDAC(pNv, 0, reg2) : 0;
	uint32_t pll1 = (oldpll1 & 0xfff80000) | pv->log2P << 16 | pv->NM1;
	uint32_t pll2 = (oldpll2 & 0x7fff0000) | 1 << 31 | pv->NM2;
	uint32_t oldramdac580 = 0, ramdac580 = 0;
	bool single_stage = !pv->NM2 || pv->N2 == pv->M2;	/* nv41+ only */
	uint32_t saved_powerctrl_1 = 0, savedc040 = 0;
	int shift_powerctrl_1 = powerctrl_1_shift(chip_version, reg1);

	/* model specific additions to generic pll1 and pll2 set up above */
	if (nv3035) {
		pll1 = (pll1 & 0xfcc7ffff) | (pv->N2 & 0x18) << 21 |
		       (pv->N2 & 0x7) << 19 | 8 << 4 | (pv->M2 & 7) << 4;
		pll2 = 0;
	}
	if (chip_version > 0x40 && reg1 >= NV_PRAMDAC_VPLL_COEFF) { /* !nv40 */
		oldramdac580 = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_580);
		ramdac580 = new_ramdac580(reg1, single_stage, oldramdac580);
		if (oldramdac580 != ramdac580)
			oldpll1 = ~0;	/* force mismatch */
		if (single_stage)
			/* magic value used by nvidia in single stage mode */
			pll2 |= 0x011f;
	}
	if (chip_version > 0x70)
		/* magic bits set by the blob (but not the bios) on g71-73 */
		pll1 = (pll1 & 0x7fffffff) | (single_stage ? 0x4 : 0xc) << 28;

	if (oldpll1 == pll1 && oldpll2 == pll2)
		return;	/* already set */

	if (shift_powerctrl_1 >= 0) {
		saved_powerctrl_1 = nvReadMC(pNv, NV_PBUS_POWERCTRL_1);
		nvWriteMC(pNv, NV_PBUS_POWERCTRL_1,
			(saved_powerctrl_1 & ~(0xf << shift_powerctrl_1)) |
			1 << shift_powerctrl_1);
	}

	if (chip_version >= 0x40) {
		int shift_c040 = 14;

		switch (reg1) {
		case NV_PRAMDAC_MPLL_COEFF:
			shift_c040 += 2;
		case NV_PRAMDAC_NVPLL_COEFF:
			shift_c040 += 2;
		case NV_RAMDAC_VPLL2:
			shift_c040 += 2;
		case NV_PRAMDAC_VPLL_COEFF:
			shift_c040 += 2;
		}

		savedc040 = nvReadMC(pNv, 0xc040);
		if (shift_c040 != 14)
			nvWriteMC(pNv, 0xc040, savedc040 & ~(3 << shift_c040));
	}

	if (oldramdac580 != ramdac580)
		NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_580, ramdac580);

	if (!nv3035)
		NVWriteRAMDAC(pNv, 0, reg2, pll2);
	NVWriteRAMDAC(pNv, 0, reg1, pll1);

	if (shift_powerctrl_1 >= 0)
		nvWriteMC(pNv, NV_PBUS_POWERCTRL_1, saved_powerctrl_1);
	if (chip_version >= 0x40)
		nvWriteMC(pNv, 0xc040, savedc040);
}

static void setPLL_double_lowregs(ScrnInfoPtr pScrn, uint32_t NMNMreg,
				  struct nouveau_pll_vals *pv)
{
	/* When setting PLLs, there is a merry game of disabling and enabling
	 * various bits of hardware during the process. This function is a
	 * synthesis of six nv4x traces, nearly each card doing a subtly
	 * different thing. With luck all the necessary bits for each card are
	 * combined herein. Without luck it deviates from each card's formula
	 * so as to not work on any :)
	 */

	NVPtr pNv = NVPTR(pScrn);
	uint32_t Preg = NMNMreg - 4;
	bool mpll = Preg == 0x4020;
	uint32_t oldPval = nvReadMC(pNv, Preg);
	uint32_t NMNM = pv->NM2 << 16 | pv->NM1;
	uint32_t Pval = (oldPval & (mpll ? ~(0x11 << 16) : ~(1 << 16))) |
			0xc << 28 | pv->log2P << 16;
	uint32_t saved4600 = 0;
	/* some cards have different maskc040s */
	uint32_t maskc040 = ~(3 << 14), savedc040;
	bool single_stage = !pv->NM2 || pv->N2 == pv->M2;

	if (nvReadMC(pNv, NMNMreg) == NMNM && (oldPval & 0xc0070000) == Pval)
		return;

	if (Preg == 0x4000)
		maskc040 = ~0x333;
	if (Preg == 0x4058)
		maskc040 = ~(0xc << 24);

	if (mpll) {
		struct pll_lims pll_lim;
		uint8_t Pval2;

		if (get_pll_limits(pScrn, Preg, &pll_lim))
			return;

		Pval2 = pv->log2P + pll_lim.log2p_bias;
		if (Pval2 > pll_lim.max_log2p)
			Pval2 = pll_lim.max_log2p;
		Pval |= 1 << 28 | Pval2 << 20;

		saved4600 = nvReadMC(pNv, 0x4600);
		nvWriteMC(pNv, 0x4600, saved4600 | 8 << 28);
	}
	if (single_stage)
		Pval |= mpll ? 1 << 12 : 1 << 8;

	nvWriteMC(pNv, Preg, oldPval | 1 << 28);
	nvWriteMC(pNv, Preg, Pval & ~(4 << 28));
	if (mpll) {
		Pval |= 8 << 20;
		nvWriteMC(pNv, 0x4020, Pval & ~(0xc << 28));
		nvWriteMC(pNv, 0x4038, Pval & ~(0xc << 28));
	}

	savedc040 = nvReadMC(pNv, 0xc040);
	nvWriteMC(pNv, 0xc040, savedc040 & maskc040);

	nvWriteMC(pNv, NMNMreg, NMNM);
	if (NMNMreg == 0x4024)
		nvWriteMC(pNv, 0x403c, NMNM);

	nvWriteMC(pNv, Preg, Pval);
	if (mpll) {
		Pval &= ~(8 << 20);
		nvWriteMC(pNv, 0x4020, Pval);
		nvWriteMC(pNv, 0x4038, Pval);
		nvWriteMC(pNv, 0x4600, saved4600);
	}

	nvWriteMC(pNv, 0xc040, savedc040);

	if (mpll) {
		nvWriteMC(pNv, 0x4020, Pval & ~(1 << 28));
		nvWriteMC(pNv, 0x4038, Pval & ~(1 << 28));
	}
}

void nouveau_hw_setpll(ScrnInfoPtr pScrn, uint32_t reg1,
			 struct nouveau_pll_vals *pv)
{
	int cv = NVPTR(pScrn)->vbios->chip_version;

	if (cv == 0x30 || cv == 0x31 || cv == 0x35 || cv == 0x36 ||
	    cv >= 0x40) {
		if (reg1 > 0x405c)
			setPLL_double_highregs(pScrn, reg1, pv);
		else
			setPLL_double_lowregs(pScrn, reg1, pv);
	} else
		setPLL_single(pScrn, reg1, pv);
}

/*
 * PLL getting
 */

static void nouveau_hw_decode_pll(NVPtr pNv, uint32_t reg1,
				  uint32_t pll1, uint32_t pll2,
				  struct nouveau_pll_vals *pllvals)
{
	/* to force parsing as single stage (i.e. nv40 vplls) pass pll2 as 0 */

	/* log2P is & 0x7 as never more than 7, and nv30/35 only uses 3 bits */
	pllvals->log2P = (pll1 >> 16) & 0x7;
	pllvals->N2 = pllvals->M2 = 1;

	if (reg1 <= 0x405c) {
		pllvals->NM1 = pll2 & 0xffff;
		/* single stage NVPLL and VPLLs use 1 << 8, MPLL uses 1 << 12 */
		if (!(pll1 & 0x1100))
			pllvals->NM2 = pll2 >> 16;
	} else {
		pllvals->NM1 = pll1 & 0xffff;
		if (pNv->two_reg_pll && pll2 & NV31_RAMDAC_ENABLE_VCO2)
			pllvals->NM2 = pll2 & 0xffff;
		else if (pNv->NVArch == 0x30 || pNv->NVArch == 0x35) {
			pllvals->M1 &= 0xf; /* only 4 bits */
			if (pll1 & NV30_RAMDAC_ENABLE_VCO2) {
				pllvals->M2 = (pll1 >> 4) & 0x7;
				pllvals->N2 = ((pll1 >> 21) & 0x18) |
					      ((pll1 >> 19) & 0x7);
			}
		}
	}
}

int nouveau_hw_get_pllvals(ScrnInfoPtr pScrn, enum pll_types plltype,
			   struct nouveau_pll_vals *pllvals)
{
	NVPtr pNv = NVPTR(pScrn);
	const uint32_t nv04_regs[MAX_PLL_TYPES] = { NV_PRAMDAC_NVPLL_COEFF,
						    NV_PRAMDAC_MPLL_COEFF,
						    NV_PRAMDAC_VPLL_COEFF,
						    NV_RAMDAC_VPLL2 };
	const uint32_t nv40_regs[MAX_PLL_TYPES] = { 0x4000,
						    0x4020,
						    NV_PRAMDAC_VPLL_COEFF,
						    NV_RAMDAC_VPLL2 };
	uint32_t reg1, pll1, pll2 = 0;
	struct pll_lims pll_lim;
	int ret;

	if (pNv->Architecture < NV_ARCH_40)
		reg1 = nv04_regs[plltype];
	else
		reg1 = nv40_regs[plltype];

	pll1 = nvReadMC(pNv, reg1);

	if (reg1 <= 0x405c)
		pll2 = nvReadMC(pNv, reg1 + 4);
	else if (pNv->two_reg_pll) {
		uint32_t reg2 = reg1 + (reg1 == NV_RAMDAC_VPLL2 ? 0x5c : 0x70);

		pll2 = nvReadMC(pNv, reg2);
	}

	if (pNv->Architecture == 0x40 && reg1 >= NV_PRAMDAC_VPLL_COEFF) {
		uint32_t ramdac580 = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_580);

		/* check whether vpll has been forced into single stage mode */
		if (reg1 == NV_PRAMDAC_VPLL_COEFF) {
			if (ramdac580 & NV_RAMDAC_580_VPLL1_ACTIVE)
				pll2 = 0;
		} else
			if (ramdac580 & NV_RAMDAC_580_VPLL2_ACTIVE)
				pll2 = 0;
	}

	nouveau_hw_decode_pll(pNv, reg1, pll1, pll2, pllvals);

	if ((ret = get_pll_limits(pScrn, plltype, &pll_lim)))
		return ret;

	pllvals->refclk = pll_lim.refclk;

	return 0;
}

int nouveau_hw_pllvals_to_clk(struct nouveau_pll_vals *pv)
{
	/* Avoid divide by zero if called at an inappropriate time */
	if (!pv->M1 || !pv->M2)
		return 0;

	return (pv->N1 * pv->N2 * pv->refclk / (pv->M1 * pv->M2) >> pv->log2P);
}

int nouveau_hw_get_clock(ScrnInfoPtr pScrn, enum pll_types plltype)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pll_vals pllvals;

	if (plltype == MPLL && (pNv->Chipset & 0x0ff0) == CHIPSET_NFORCE) {
		uint32_t mpllP = (PCI_SLOT_READ_LONG(3, 0x6c) >> 8) & 0xf;

		if (!mpllP)
			mpllP = 4;
		return 400000 / mpllP;
	} else if (plltype == MPLL && (pNv->Chipset & 0xff0) == CHIPSET_NFORCE2)
		return PCI_SLOT_READ_LONG(5, 0x4c) / 1000;

	nouveau_hw_get_pllvals(pScrn, plltype, &pllvals);

	return nouveau_hw_pllvals_to_clk(&pllvals);
}

static void nouveau_hw_fix_bad_vpll(ScrnInfoPtr pScrn, int head)
{
	/* the vpll on an unused head can come up with a random value, way
	 * beyond the pll limits.  for some reason this causes the chip to
	 * lock up when reading the dac palette regs, so set a valid pll here
	 * when such a condition detected.  only seen on nv11 to date
	 */

	struct pll_lims pll_lim;
	struct nouveau_pll_vals pv;
	uint32_t pllreg = head ? NV_RAMDAC_VPLL2 : NV_PRAMDAC_VPLL_COEFF;

	if (get_pll_limits(pScrn, head ? VPLL2 : VPLL1, &pll_lim))
		return;
	nouveau_hw_get_pllvals(pScrn, head ? VPLL2 : VPLL1, &pv);

	if (pv.M1 >= pll_lim.vco1.min_m && pv.M1 <= pll_lim.vco1.max_m &&
	    pv.N1 >= pll_lim.vco1.min_n && pv.N1 <= pll_lim.vco1.max_n &&
	    pv.log2P <= pll_lim.max_log2p)
		return;

	NV_WARN(pScrn, "VPLL %d outwith limits, attempting to fix\n", head + 1);

	/* set lowest clock within static limits */
	pv.M1 = pll_lim.vco1.max_m;
	pv.N1 = pll_lim.vco1.min_n;
	pv.log2P = pll_lim.max_usable_log2p;
	nouveau_hw_setpll(pScrn, pllreg, &pv);
}

/*
 * vga font save/restore
 */

void nouveau_hw_save_vga_fonts(ScrnInfoPtr pScrn, bool save)
{
	NVPtr pNv = NVPTR(pScrn);
	bool graphicsmode;
	uint8_t misc, gr4, gr5, gr6, seq2, seq4;
	int i;

	if (pNv->twoHeads)
		NVSetOwner(pNv, 0);

	NVSetEnablePalette(pNv, 0, true);
	graphicsmode = NVReadVgaAttr(pNv, 0, NV_CIO_AR_MODE_INDEX) & 1;
	NVSetEnablePalette(pNv, 0, false);

	if (graphicsmode)	/* graphics mode => framebuffer => no need to save */
		return;

	NV_TRACE(pScrn, "%sing VGA fonts\n", save ? "Sav" : "Restor");
	if (pNv->twoHeads)
		NVBlankScreen(pNv, 1, true);
	NVBlankScreen(pNv, 0, true);

	/* save control regs */
	misc = NVReadPRMVIO(pNv, 0, NV_PRMVIO_MISC__READ);
	seq2 = NVReadVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX);
	seq4 = NVReadVgaSeq(pNv, 0, NV_VIO_SR_MEM_MODE_INDEX);
	gr4 = NVReadVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX);
	gr5 = NVReadVgaGr(pNv, 0, NV_VIO_GX_MODE_INDEX);
	gr6 = NVReadVgaGr(pNv, 0, NV_VIO_GX_MISC_INDEX);

	NVWritePRMVIO(pNv, 0, NV_PRMVIO_MISC__WRITE, 0x67);
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_MEM_MODE_INDEX, 0x6);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_MODE_INDEX, 0x0);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_MISC_INDEX, 0x5);

	/* store font in plane 0 */
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX, 0x1);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX, 0x0);
	for (i = 0; i < 16384; i++)
		if (save)
			pNv->saved_vga_font[0][i] = MMIO_IN32(pNv->FB_BAR, i * 4);
		else
			MMIO_OUT32(pNv->FB_BAR, i * 4, pNv->saved_vga_font[0][i]);

	/* store font in plane 1 */
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX, 0x2);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX, 0x1);
	for (i = 0; i < 16384; i++)
		if (save)
			pNv->saved_vga_font[1][i] = MMIO_IN32(pNv->FB_BAR, i * 4);
		else
			MMIO_OUT32(pNv->FB_BAR, i * 4, pNv->saved_vga_font[1][i]);

	/* store font in plane 2 */
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX, 0x4);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX, 0x2);
	for (i = 0; i < 16384; i++)
		if (save)
			pNv->saved_vga_font[2][i] = MMIO_IN32(pNv->FB_BAR, i * 4);
		else
			MMIO_OUT32(pNv->FB_BAR, i * 4, pNv->saved_vga_font[2][i]);

	/* store font in plane 3 */
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX, 0x8);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX, 0x3);
	for (i = 0; i < 16384; i++)
		if (save)
			pNv->saved_vga_font[3][i] = MMIO_IN32(pNv->FB_BAR, i * 4);
		else
			MMIO_OUT32(pNv->FB_BAR, i * 4, pNv->saved_vga_font[3][i]);

	/* restore control regs */
	NVWritePRMVIO(pNv, 0, NV_PRMVIO_MISC__WRITE, misc);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX, gr4);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_MODE_INDEX, gr5);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_MISC_INDEX, gr6);
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX, seq2);
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_MEM_MODE_INDEX, seq4);

	if (pNv->twoHeads)
		NVBlankScreen(pNv, 1, false);
	NVBlankScreen(pNv, 0, false);
}

/*
 * mode state save/load
 */

static void rd_cio_state(NVPtr pNv, int head,
			 struct nouveau_crtc_state *crtcstate, int index)
{
	crtcstate->CRTC[index] = NVReadVgaCrtc(pNv, head, index);
}

static void wr_cio_state(NVPtr pNv, int head,
			 struct nouveau_crtc_state *crtcstate, int index)
{
	NVWriteVgaCrtc(pNv, head, index, crtcstate->CRTC[index]);
}

static void
nv_save_state_ramdac(ScrnInfoPtr pScrn, int head, struct nouveau_mode_state *state)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc_state *regp = &state->head[head];
	int i;

	if (pNv->Architecture >= NV_ARCH_10)
		regp->nv10_cursync = NVReadRAMDAC(pNv, head, NV_RAMDAC_NV10_CURSYNC);

	nouveau_hw_get_pllvals(pScrn, head ? VPLL2 : VPLL1, &regp->pllvals);
	state->pllsel = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_PLL_COEFF_SELECT);
	if (pNv->twoHeads)
		state->sel_clk = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_SEL_CLK);
	if (pNv->NVArch == 0x11)
		regp->dither = NVReadRAMDAC(pNv, head, NV_RAMDAC_DITHER_NV11);

	regp->ramdac_gen_ctrl = NVReadRAMDAC(pNv, head, NV_PRAMDAC_GENERAL_CONTROL);

	if (pNv->gf4_disp_arch)
		regp->ramdac_630 = NVReadRAMDAC(pNv, head, NV_PRAMDAC_630);
	if (pNv->NVArch >= 0x30)
		regp->ramdac_634 = NVReadRAMDAC(pNv, head, NV_PRAMDAC_634);

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_PRAMDAC_FP_VDISPLAY_END + (i * 4);

		regp->fp_vert_regs[i] = NVReadRAMDAC(pNv, head, ramdac_reg);
		regp->fp_horiz_regs[i] = NVReadRAMDAC(pNv, head, ramdac_reg + 0x20);
	}

	if (pNv->gf4_disp_arch) {
		regp->dither = NVReadRAMDAC(pNv, head, NV_RAMDAC_FP_DITHER);
		for (i = 0; i < 3; i++) {
			regp->dither_regs[i] = NVReadRAMDAC(pNv, head, NV_PRAMDAC_850 + i * 4);
			regp->dither_regs[i + 3] = NVReadRAMDAC(pNv, head, NV_PRAMDAC_85C + i * 4);
		}
	}

	regp->fp_control = NVReadRAMDAC(pNv, head, NV_PRAMDAC_FP_TG_CONTROL);
	regp->fp_debug_0 = NVReadRAMDAC(pNv, head, NV_PRAMDAC_FP_DEBUG_0);
	if (!pNv->gf4_disp_arch && head == 0)
		/* early chips don't allow access to PRAMDAC_TMDS_* without
		 * the head A FPCLK on (nv11 even locks up) */
		NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_FP_DEBUG_0, regp->fp_debug_0 &
					~NV_PRAMDAC_FP_DEBUG_0_PWRDOWN_FPCLK);
	regp->fp_debug_1 = NVReadRAMDAC(pNv, head, NV_PRAMDAC_FP_DEBUG_1);
	regp->fp_debug_2 = NVReadRAMDAC(pNv, head, NV_PRAMDAC_FP_DEBUG_2);

	if (pNv->Architecture == NV_ARCH_40) {
		regp->ramdac_a20 = NVReadRAMDAC(pNv, head, NV_PRAMDAC_A20);
		regp->ramdac_a24 = NVReadRAMDAC(pNv, head, NV_PRAMDAC_A24);
		regp->ramdac_a34 = NVReadRAMDAC(pNv, head, NV_PRAMDAC_A34);
	}
}

static void nv_load_state_ramdac(ScrnInfoPtr pScrn, int head, struct nouveau_mode_state *state)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_crtc_state *regp = &state->head[head];
	uint32_t pllreg = head ? NV_RAMDAC_VPLL2 : NV_PRAMDAC_VPLL_COEFF;
	int i;

	if (pNv->Architecture >= NV_ARCH_10)
		NVWriteRAMDAC(pNv, head, NV_RAMDAC_NV10_CURSYNC, regp->nv10_cursync);

	nouveau_hw_setpll(pScrn, pllreg, &regp->pllvals);
	NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_PLL_COEFF_SELECT, state->pllsel);
	if (pNv->twoHeads)
		NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_SEL_CLK, state->sel_clk);
	if (pNv->NVArch == 0x11)
		NVWriteRAMDAC(pNv, head, NV_RAMDAC_DITHER_NV11, regp->dither);

	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_GENERAL_CONTROL, regp->ramdac_gen_ctrl);

	if (pNv->gf4_disp_arch)
		NVWriteRAMDAC(pNv, head, NV_PRAMDAC_630, regp->ramdac_630);
	if (pNv->NVArch >= 0x30)
		NVWriteRAMDAC(pNv, head, NV_PRAMDAC_634, regp->ramdac_634);

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_PRAMDAC_FP_VDISPLAY_END + (i * 4);

		NVWriteRAMDAC(pNv, head, ramdac_reg, regp->fp_vert_regs[i]);
		NVWriteRAMDAC(pNv, head, ramdac_reg + 0x20, regp->fp_horiz_regs[i]);
	}

	if (pNv->gf4_disp_arch) {
		NVWriteRAMDAC(pNv, head, NV_RAMDAC_FP_DITHER, regp->dither);
		for (i = 0; i < 3; i++) {
			NVWriteRAMDAC(pNv, head, NV_PRAMDAC_850 + i * 4, regp->dither_regs[i]);
			NVWriteRAMDAC(pNv, head, NV_PRAMDAC_85C + i * 4, regp->dither_regs[i + 3]);
		}
	}

	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_FP_TG_CONTROL, regp->fp_control);
	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_FP_DEBUG_0, regp->fp_debug_0);
	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_FP_DEBUG_1, regp->fp_debug_1);
	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_FP_DEBUG_2, regp->fp_debug_2);

	if (pNv->Architecture == NV_ARCH_40) {
		NVWriteRAMDAC(pNv, head, NV_PRAMDAC_A20, regp->ramdac_a20);
		NVWriteRAMDAC(pNv, head, NV_PRAMDAC_A24, regp->ramdac_a24);
		NVWriteRAMDAC(pNv, head, NV_PRAMDAC_A34, regp->ramdac_a34);
	}
}

static void
nv_save_state_vga(NVPtr pNv, int head, struct nouveau_mode_state *state)
{
	struct nouveau_crtc_state *regp = &state->head[head];
	int i;

	regp->MiscOutReg = NVReadPRMVIO(pNv, head, NV_PRMVIO_MISC__READ);

	for (i = 0; i < 25; i++)
		rd_cio_state(pNv, head, regp, i);

	NVSetEnablePalette(pNv, head, true);
	for (i = 0; i < 21; i++)
		regp->Attribute[i] = NVReadVgaAttr(pNv, head, i);
	NVSetEnablePalette(pNv, head, false);

	for (i = 0; i < 9; i++)
		regp->Graphics[i] = NVReadVgaGr(pNv, head, i);

	for (i = 0; i < 5; i++)
		regp->Sequencer[i] = NVReadVgaSeq(pNv, head, i);
}

static void nv_load_state_vga(NVPtr pNv, int head, struct nouveau_mode_state *state)
{
	struct nouveau_crtc_state *regp = &state->head[head];
	int i;

	NVWritePRMVIO(pNv, head, NV_PRMVIO_MISC__WRITE, regp->MiscOutReg);

	for (i = 0; i < 5; i++)
		NVWriteVgaSeq(pNv, head, i, regp->Sequencer[i]);

	nv_lock_vga_crtc_base(pNv, head, false);
	for (i = 0; i < 25; i++)
		wr_cio_state(pNv, head, regp, i);
	nv_lock_vga_crtc_base(pNv, head, true);

	for (i = 0; i < 9; i++)
		NVWriteVgaGr(pNv, head, i, regp->Graphics[i]);

	NVSetEnablePalette(pNv, head, true);
	for (i = 0; i < 21; i++)
		NVWriteVgaAttr(pNv, head, i, regp->Attribute[i]);
	NVSetEnablePalette(pNv, head, false);
}

static void
nv_save_state_ext(NVPtr pNv, int head, struct nouveau_mode_state *state)
{
	struct nouveau_crtc_state *regp = &state->head[head];
	int i;

	rd_cio_state(pNv, head, regp, NV_CIO_CRE_LCD__INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_RPC0_INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_RPC1_INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_LSR_INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_PIXEL_INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_HEB__INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_ENH_INDEX);

	rd_cio_state(pNv, head, regp, NV_CIO_CRE_FF_INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_FFLWM__INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_21);
	if (pNv->Architecture >= NV_ARCH_30)
		rd_cio_state(pNv, head, regp, NV_CIO_CRE_47);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_HCUR_ADDR0_INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_HCUR_ADDR1_INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_HCUR_ADDR2_INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_ILACE__INDEX);

	if (pNv->Architecture >= NV_ARCH_10) {
		regp->crtc_830 = NVReadCRTC(pNv, head, NV_PCRTC_830);
		regp->crtc_834 = NVReadCRTC(pNv, head, NV_PCRTC_834);
		if (pNv->Architecture == NV_ARCH_40) {
			regp->crtc_850 = NVReadCRTC(pNv, head, NV_PCRTC_850);
			regp->gpio_ext = NVReadCRTC(pNv, head, NV_PCRTC_GPIO_EXT);
		}
		if (pNv->twoHeads)
			regp->crtc_eng_ctrl = NVReadCRTC(pNv, head, NV_PCRTC_ENGINE_CTRL);
		regp->cursor_cfg = NVReadCRTC(pNv, head, NV_PCRTC_CURSOR_CONFIG);
	}

	regp->crtc_cfg = NVReadCRTC(pNv, head, NV_PCRTC_CONFIG);

	rd_cio_state(pNv, head, regp, NV_CIO_CRE_SCRATCH3__INDEX);
	rd_cio_state(pNv, head, regp, NV_CIO_CRE_SCRATCH4__INDEX);
	if (pNv->Architecture >= NV_ARCH_10) {
		rd_cio_state(pNv, head, regp, NV_CIO_CRE_EBR_INDEX);
		rd_cio_state(pNv, head, regp, NV_CIO_CRE_CSB);
		rd_cio_state(pNv, head, regp, NV_CIO_CRE_4B);
		rd_cio_state(pNv, head, regp, NV_CIO_CRE_TVOUT_LATENCY);
	}
	if (pNv->gf4_disp_arch) {
		rd_cio_state(pNv, head, regp, NV_CIO_CRE_53);
		rd_cio_state(pNv, head, regp, NV_CIO_CRE_54);

		for (i = 0; i < 0x10; i++)
			regp->CR58[i] = NVReadVgaCrtc5758(pNv, head, i);
		rd_cio_state(pNv, head, regp, NV_CIO_CRE_59);
		rd_cio_state(pNv, head, regp, NV_CIO_CRE_5B);

		rd_cio_state(pNv, head, regp, NV_CIO_CRE_85);
		rd_cio_state(pNv, head, regp, NV_CIO_CRE_86);
	}

	regp->fb_start = NVReadCRTC(pNv, head, NV_PCRTC_START);
}

static void nv_load_state_ext(NVPtr pNv, int head, struct nouveau_mode_state *state)
{
	struct nouveau_crtc_state *regp = &state->head[head];
	int i;

	if (pNv->Architecture >= NV_ARCH_10) {
		if (pNv->twoHeads)
			/* setting ENGINE_CTRL (EC) *must* come before
			 * CIO_CRE_LCD, as writing CRE_LCD sets bits 16 & 17 in
			 * EC that should not be overwritten by writing stale EC
			 */
			NVWriteCRTC(pNv, head, NV_PCRTC_ENGINE_CTRL, regp->crtc_eng_ctrl);

		nvWriteVIDEO(pNv, NV_PVIDEO_STOP, 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_INTR_EN, 0);
		nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(0), 0);
		nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(1), 0);
		nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(0), pNv->VRAMPhysicalSize - 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(1), pNv->VRAMPhysicalSize - 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(0), pNv->VRAMPhysicalSize - 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(1), pNv->VRAMPhysicalSize - 1);
		nvWriteMC(pNv, NV_PBUS_POWERCTRL_2, 0);

		NVWriteCRTC(pNv, head, NV_PCRTC_CURSOR_CONFIG, regp->cursor_cfg);
		NVWriteCRTC(pNv, head, NV_PCRTC_830, regp->crtc_830);
		NVWriteCRTC(pNv, head, NV_PCRTC_834, regp->crtc_834);
		if (pNv->Architecture == NV_ARCH_40) {
			NVWriteCRTC(pNv, head, NV_PCRTC_850, regp->crtc_850);
			NVWriteCRTC(pNv, head, NV_PCRTC_GPIO_EXT, regp->gpio_ext);
		}

		if (pNv->Architecture == NV_ARCH_40) {
			uint32_t reg900 = NVReadRAMDAC(pNv, head, NV_PRAMDAC_900);
			if (regp->crtc_cfg == NV_PCRTC_CONFIG_START_ADDRESS_HSYNC)
				NVWriteRAMDAC(pNv, head, NV_PRAMDAC_900, reg900 | 0x10000);
			else
				NVWriteRAMDAC(pNv, head, NV_PRAMDAC_900, reg900 & ~0x10000);
		}
	}

	NVWriteCRTC(pNv, head, NV_PCRTC_CONFIG, regp->crtc_cfg);

	wr_cio_state(pNv, head, regp, NV_CIO_CRE_RPC0_INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_RPC1_INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_LSR_INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_PIXEL_INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_LCD__INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_HEB__INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_ENH_INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_FF_INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_FFLWM__INDEX);
	if (pNv->Architecture >= NV_ARCH_30)
		wr_cio_state(pNv, head, regp, NV_CIO_CRE_47);

	wr_cio_state(pNv, head, regp, NV_CIO_CRE_HCUR_ADDR0_INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_HCUR_ADDR1_INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_HCUR_ADDR2_INDEX);
	if (pNv->Architecture == NV_ARCH_40)
		nv_fix_nv40_hw_cursor(pNv, head);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_ILACE__INDEX);

	wr_cio_state(pNv, head, regp, NV_CIO_CRE_SCRATCH3__INDEX);
	wr_cio_state(pNv, head, regp, NV_CIO_CRE_SCRATCH4__INDEX);
	if (pNv->Architecture >= NV_ARCH_10) {
		wr_cio_state(pNv, head, regp, NV_CIO_CRE_EBR_INDEX);
		wr_cio_state(pNv, head, regp, NV_CIO_CRE_CSB);
		wr_cio_state(pNv, head, regp, NV_CIO_CRE_4B);
		wr_cio_state(pNv, head, regp, NV_CIO_CRE_TVOUT_LATENCY);
	}
	if (pNv->gf4_disp_arch) {
		wr_cio_state(pNv, head, regp, NV_CIO_CRE_53);
		wr_cio_state(pNv, head, regp, NV_CIO_CRE_54);

		for (i = 0; i < 0x10; i++)
			NVWriteVgaCrtc5758(pNv, head, i, regp->CR58[i]);
		wr_cio_state(pNv, head, regp, NV_CIO_CRE_59);
		wr_cio_state(pNv, head, regp, NV_CIO_CRE_5B);

		wr_cio_state(pNv, head, regp, NV_CIO_CRE_85);
		wr_cio_state(pNv, head, regp, NV_CIO_CRE_86);
	}

	NVWriteCRTC(pNv, head, NV_PCRTC_START, regp->fb_start);

	/* Setting 1 on this value gives you interrupts for every vblank period. */
	NVWriteCRTC(pNv, head, NV_PCRTC_INTR_EN_0, 0);
	NVWriteCRTC(pNv, head, NV_PCRTC_INTR_0, NV_PCRTC_INTR_0_VBLANK);
}

static void
nv_save_state_palette(NVPtr pNv, int head, struct nouveau_mode_state *state)
{
	int head_offset = head * NV_PRMDIO_SIZE, i;

	VGA_WR08(pNv->REGS, NV_PRMDIO_PIXEL_MASK + head_offset, NV_PRMDIO_PIXEL_MASK_MASK);
	VGA_WR08(pNv->REGS, NV_PRMDIO_READ_MODE_ADDRESS + head_offset, 0x0);

	for (i = 0; i < 768; i++) {
		state->head[head].DAC[i] = NV_RD08(pNv->REGS, NV_PRMDIO_PALETTE_DATA + head_offset);
		DDXMMIOH("nv_save_state_palette: head %d reg 0x%04x data 0x%02x\n", head, NV_PRMDIO_PALETTE_DATA + head_offset, state->head[head].DAC[i]);
	}

	NVSetEnablePalette(pNv, head, false);
}

void nouveau_hw_load_state_palette(NVPtr pNv, int head,
				   struct nouveau_mode_state *state)
{
	int head_offset = head * NV_PRMDIO_SIZE, i;

	VGA_WR08(pNv->REGS, NV_PRMDIO_PIXEL_MASK + head_offset, NV_PRMDIO_PIXEL_MASK_MASK);
	VGA_WR08(pNv->REGS, NV_PRMDIO_WRITE_MODE_ADDRESS + head_offset, 0x0);

	for (i = 0; i < 768; i++) {
		DDXMMIOH("nouveau_mode_state_load_palette: head %d reg 0x%04x data 0x%02x\n", head, NV_PRMDIO_PALETTE_DATA + head_offset, state->head[head].DAC[i]);
		NV_WR08(pNv->REGS, NV_PRMDIO_PALETTE_DATA + head_offset, state->head[head].DAC[i]);
	}

	NVSetEnablePalette(pNv, head, false);
}

void nouveau_hw_save_state(ScrnInfoPtr pScrn, int head,
			   struct nouveau_mode_state *state)
{
	NVPtr pNv = NVPTR(pScrn);

	if (pNv->NVArch == 0x11)
		/* NB: no attempt is made to restore the bad pll later on */
		nouveau_hw_fix_bad_vpll(pScrn, head);
	nv_save_state_ramdac(pScrn, head, state);
	nv_save_state_vga(pNv, head, state);
	nv_save_state_palette(pNv, head, state);
	nv_save_state_ext(pNv, head, state);
}

void nouveau_hw_load_state(ScrnInfoPtr pScrn, int head,
			   struct nouveau_mode_state *state)
{
	NVPtr pNv = NVPTR(pScrn);

	NVVgaProtect(pNv, head, true);
	nv_load_state_ramdac(pScrn, head, state);
	nv_load_state_ext(pNv, head, state);
	nouveau_hw_load_state_palette(pNv, head, state);
	nv_load_state_vga(pNv, head, state);
	NVVgaProtect(pNv, head, false);
}
