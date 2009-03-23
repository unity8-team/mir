/*
 * Copyright 1993-2003 NVIDIA, Corporation
 * Copyright 2008 Stuart Bennett
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

uint32_t NVRead(NVPtr pNv, uint32_t reg)
{
	DDXMMIOW("NVRead: reg %08x val %08x\n", reg, (uint32_t)NV_RD32(pNv->REGS, reg));
	return NV_RD32(pNv->REGS, reg);
}

void NVWrite(NVPtr pNv, uint32_t reg, uint32_t val)
{
	DDXMMIOW("NVWrite: reg %08x val %08x\n", reg, NV_WR32(pNv->REGS, reg, val));
}

uint32_t NVReadCRTC(NVPtr pNv, int head, uint32_t reg)
{
	if (head)
		reg += NV_PCRTC0_SIZE;
	DDXMMIOH("NVReadCRTC: head %d reg %08x val %08x\n", head, reg, (uint32_t)NV_RD32(pNv->REGS, reg));
	return NV_RD32(pNv->REGS, reg);
}

void NVWriteCRTC(NVPtr pNv, int head, uint32_t reg, uint32_t val)
{
	if (head)
		reg += NV_PCRTC0_SIZE;
	DDXMMIOH("NVWriteCRTC: head %d reg %08x val %08x\n", head, reg, val);
	NV_WR32(pNv->REGS, reg, val);
}

uint32_t NVReadRAMDAC(NVPtr pNv, int head, uint32_t reg)
{
	if (head)
		reg += NV_PRAMDAC0_SIZE;
	DDXMMIOH("NVReadRamdac: head %d reg %08x val %08x\n", head, reg, (uint32_t)NV_RD32(pNv->REGS, reg));
	return NV_RD32(pNv->REGS, reg);
}

void NVWriteRAMDAC(NVPtr pNv, int head, uint32_t reg, uint32_t val)
{
	if (head)
		reg += NV_PRAMDAC0_SIZE;
	DDXMMIOH("NVWriteRamdac: head %d reg %08x val %08x\n", head, reg, val);
	NV_WR32(pNv->REGS, reg, val);
}

uint8_t nv_read_tmds(NVPtr pNv, int or, int dl, uint8_t address)
{
	int ramdac = (or & OUTPUT_C) >> 2;

	NVWriteRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_CONTROL + dl * 8,
		      NV_PRAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE | address);
	return NVReadRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_DATA + dl * 8);
}

int nv_get_digital_bound_head(NVPtr pNv, int or)
{
	/* special case of nv_read_tmds to find crtc associated with an output.
	 * this does not give a correct answer for off-chip dvi, but there's no
	 * use for such an answer anyway
	 */
	int ramdac = (or & OUTPUT_C) >> 2;

	NVWriteRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_CONTROL,
		      NV_PRAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE | 0x4);
	return (((NVReadRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_DATA) & 0x8) >> 3) ^ ramdac);
}

void nv_write_tmds(NVPtr pNv, int or, int dl, uint8_t address, uint8_t data)
{
	int ramdac = (or & OUTPUT_C) >> 2;

	NVWriteRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_DATA + dl * 8, data);
	NVWriteRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_CONTROL + dl * 8, address);
}

void NVWriteVgaCrtc(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	DDXMMIOH("NVWriteVgaCrtc: head %d index 0x%02x data 0x%02x\n", head, index, value);
	NV_WR08(pNv->REGS, NV_PRMCIO_CRX__COLOR + head * NV_PRMCIO_SIZE, index);
	NV_WR08(pNv->REGS, NV_PRMCIO_CR__COLOR + head * NV_PRMCIO_SIZE, value);
}

uint8_t NVReadVgaCrtc(NVPtr pNv, int head, uint8_t index)
{
	NV_WR08(pNv->REGS, NV_PRMCIO_CRX__COLOR + head * NV_PRMCIO_SIZE, index);
	DDXMMIOH("NVReadVgaCrtc: head %d index 0x%02x data 0x%02x\n", head, index, NV_RD08(pNv->REGS, NV_PRMCIO_CR__COLOR + head * NV_PRMCIO_SIZE));
	return NV_RD08(pNv->REGS, NV_PRMCIO_CR__COLOR + head * NV_PRMCIO_SIZE);
}

/* CR57 and CR58 are a fun pair of regs. CR57 provides an index (0-0xf) for CR58
 * I suspect they in fact do nothing, but are merely a way to carry useful
 * per-head variables around
 *
 * Known uses:
 * CR57		CR58
 * 0x00		index to the appropriate dcb entry (or 7f for inactive)
 * 0x02		dcb entry's "or" value (or 00 for inactive)
 * 0x03		bit0 set for dual link (LVDS, possibly elsewhere too)
 * 0x08 or 0x09	pxclk in MHz
 * 0x0f		laptop panel info -	low nibble for PEXTDEV_BOOT_0 strap
 * 					high nibble for xlat strap value
 */

void NVWriteVgaCrtc5758(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	NVWriteVgaCrtc(pNv, head, NV_CIO_CRE_57, index);
	NVWriteVgaCrtc(pNv, head, NV_CIO_CRE_58, value);
}

uint8_t NVReadVgaCrtc5758(NVPtr pNv, int head, uint8_t index)
{
	NVWriteVgaCrtc(pNv, head, NV_CIO_CRE_57, index);
	return NVReadVgaCrtc(pNv, head, NV_CIO_CRE_58);
}

uint8_t NVReadPRMVIO(NVPtr pNv, int head, uint32_t reg)
{
	/* Only NV4x have two pvio ranges; other twoHeads cards MUST call
	 * NVSetOwner for the relevant head to be programmed */
	if (head && pNv->Architecture == NV_ARCH_40)
		reg += NV_PRMVIO_SIZE;

	DDXMMIOH("NVReadPRMVIO: head %d reg %08x val %02x\n", head, reg, NV_RD08(pNv->REGS, reg));
	return NV_RD08(pNv->REGS, reg);
}

void NVWritePRMVIO(NVPtr pNv, int head, uint32_t reg, uint8_t value)
{
	/* Only NV4x have two pvio ranges; other twoHeads cards MUST call
	 * NVSetOwner for the relevant head to be programmed */
	if (head && pNv->Architecture == NV_ARCH_40)
		reg += NV_PRMVIO_SIZE;

	DDXMMIOH("NVWritePRMVIO: head %d reg %08x val %02x\n", head, reg, value);
	NV_WR08(pNv->REGS, reg, value);
}

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

void NVSetEnablePalette(NVPtr pNv, int head, bool enable)
{
	VGA_RD08(pNv->REGS, NV_PRMCIO_INP0__COLOR + head * NV_PRMCIO_SIZE);
	VGA_WR08(pNv->REGS, NV_PRMCIO_ARX + head * NV_PRMCIO_SIZE, enable ? 0 : 0x20);
}

static bool NVGetEnablePalette(NVPtr pNv, int head)
{
	VGA_RD08(pNv->REGS, NV_PRMCIO_INP0__COLOR + head * NV_PRMCIO_SIZE);
	return !(VGA_RD08(pNv->REGS, NV_PRMCIO_ARX + head * NV_PRMCIO_SIZE) & 0x20);
}

void NVWriteVgaAttr(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	if (NVGetEnablePalette(pNv, head))
		index &= ~0x20;
	else
		index |= 0x20;

	NV_RD08(pNv->REGS, NV_PRMCIO_INP0__COLOR + head * NV_PRMCIO_SIZE);
	DDXMMIOH("NVWriteVgaAttr: head %d index 0x%02x data 0x%02x\n", head, index, value);
	NV_WR08(pNv->REGS, NV_PRMCIO_ARX + head * NV_PRMCIO_SIZE, index);
	NV_WR08(pNv->REGS, NV_PRMCIO_AR__WRITE + head * NV_PRMCIO_SIZE, value);
}

uint8_t NVReadVgaAttr(NVPtr pNv, int head, uint8_t index)
{
	if (NVGetEnablePalette(pNv, head))
		index &= ~0x20;
	else
		index |= 0x20;

	NV_RD08(pNv->REGS, NV_PRMCIO_INP0__COLOR + head * NV_PRMCIO_SIZE);
	NV_WR08(pNv->REGS, NV_PRMCIO_ARX + head * NV_PRMCIO_SIZE, index);
	DDXMMIOH("NVReadVgaAttr: head %d index 0x%02x data 0x%02x\n", head, index, NV_RD08(pNv->REGS, NV_PRMCIO_AR__READ + head * NV_PRMCIO_SIZE));
	return NV_RD08(pNv->REGS, NV_PRMCIO_AR__READ + head * NV_PRMCIO_SIZE);
}

void NVVgaSeqReset(NVPtr pNv, int head, bool start)
{
	NVWriteVgaSeq(pNv, head, NV_VIO_SR_RESET_INDEX, start ? 0x1 : 0x3);
}

void NVVgaProtect(NVPtr pNv, int head, bool protect)
{
	uint8_t seq1 = NVReadVgaSeq(pNv, head, NV_VIO_SR_CLOCK_INDEX);

	if (protect) {
		NVVgaSeqReset(pNv, head, true);
		NVWriteVgaSeq(pNv, head, NV_VIO_SR_CLOCK_INDEX, seq1 | 0x20);
	} else {
		/* Reenable sequencer, then turn on screen */
		NVWriteVgaSeq(pNv, head, NV_VIO_SR_CLOCK_INDEX, seq1 & ~0x20);   /* reenable display */
		NVVgaSeqReset(pNv, head, false);
	}
	NVSetEnablePalette(pNv, head, protect);
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

bool nv_heads_tied(NVPtr pNv)
{
	if (pNv->NVArch == 0x11)
		return !!(nvReadMC(pNv, NV_PBUS_DEBUG_1) & (1 << 28));

	return (NVReadVgaCrtc(pNv, 0, NV_CIO_CRE_44) == 0x4);
}

/* makes cr0-7 on the specified head read-only */
bool nv_lock_vga_crtc_base(NVPtr pNv, int head, bool lock)
{
	uint8_t cr11 = NVReadVgaCrtc(pNv, head, NV_CIO_CR_VRE_INDEX);
	bool waslocked = cr11 & 0x80;

	if (lock)
		cr11 |= 0x80;
	else
		cr11 &= ~0x80;
	NVWriteVgaCrtc(pNv, head, NV_CIO_CR_VRE_INDEX, cr11);

	return waslocked;
}

/* renders the extended crtc regs (cr19+) on all crtcs impervious:
 * immutable and unreadable
 */
bool NVLockVgaCrtcs(NVPtr pNv, bool lock)
{
	bool waslocked = !NVReadVgaCrtc(pNv, 0, NV_CIO_SR_LOCK_INDEX);

	NVWriteVgaCrtc(pNv, 0, NV_CIO_SR_LOCK_INDEX,
		       lock ? NV_CIO_SR_LOCK_VALUE : NV_CIO_SR_UNLOCK_RW_VALUE);
	/* NV11 has independently lockable extended crtcs, except when tied */
	if (pNv->NVArch == 0x11 && !nv_heads_tied(pNv))
		NVWriteVgaCrtc(pNv, 1, NV_CIO_SR_LOCK_INDEX,
			       lock ? NV_CIO_SR_LOCK_VALUE : NV_CIO_SR_UNLOCK_RW_VALUE);

	return waslocked;
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

void nv_fix_nv40_hw_cursor(NVPtr pNv, int head)
{
	/* on some nv40 (such as the "true" (in the NV_PFB_BOOT_0 sense) nv40,
	 * the gf6800gt) a hardware bug requires a write to PRAMDAC_CURSOR_POS
	 * for changes to the CRTC CURCTL regs to take effect, whether changing
	 * the pixmap location, or just showing/hiding the cursor
	 */
	volatile uint32_t curpos = NVReadRAMDAC(pNv, head, NV_PRAMDAC_CU_START_POS);
	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_CU_START_POS, curpos);
}

void nv_show_cursor(NVPtr pNv, int head, bool show)
{
	uint8_t *curctl1 = &pNv->ModeReg.crtc_reg[head].CRTC[NV_CIO_CRE_HCUR_ADDR1_INDEX];

	if (show)
		*curctl1 |= MASK(NV_CIO_CRE_HCUR_ADDR1_ENABLE);
	else
		*curctl1 &= ~MASK(NV_CIO_CRE_HCUR_ADDR1_ENABLE);
	NVWriteVgaCrtc(pNv, head, NV_CIO_CRE_HCUR_ADDR1_INDEX, *curctl1);

	if (pNv->Architecture == NV_ARCH_40)
		nv_fix_nv40_hw_cursor(pNv, head);
}

static void nouveau_hw_decode_pll(NVPtr pNv, uint32_t reg1,
				  uint32_t pll1, uint32_t pll2,
				  struct nouveau_pll_vals *pllvals)
{
	/* to force parsing as single stage (i.e. nv40 vplls) pass pll2 as 0 */

	/* log2P is & 0x7 as never more than 6, and nv30/35 only uses 3 bits */
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
				pllvals->N2 = ((pll2 >> 21) & 0x18) |
					      ((pll2 >> 19) & 0x7);
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

static int nv_get_clock(ScrnInfoPtr pScrn, enum pll_types plltype)
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

/****************************************************************************\
*                                                                            *
* The video arbitration routines calculate some "magic" numbers.  Fixes      *
* the snow seen when accessing the framebuffer without it.                   *
* It just works (I hope).                                                    *
*                                                                            *
\****************************************************************************/

struct nv_fifo_info {
	int graphics_lwm;
	int video_lwm;
	int graphics_burst_size;
	int video_burst_size;
	bool valid;
};

struct nv_sim_state {
	int pclk_khz;
	int mclk_khz;
	int nvclk_khz;
	int pix_bpp;
	bool enable_mp;
	bool enable_video;
	int mem_page_miss;
	int mem_latency;
	int memory_type;
	int memory_width;
};

static void nv4CalcArbitration(struct nv_fifo_info *fifo, struct nv_sim_state *arb)
{
	int pagemiss, cas, width, video_enable, bpp;
	int nvclks, mclks, pclks, vpagemiss, crtpagemiss, vbs;
	int found, mclk_extra, mclk_loop, cbs, m1, p1;
	int mclk_freq, pclk_freq, nvclk_freq, mp_enable;
	int us_m, us_n, us_p, video_drain_rate, crtc_drain_rate;
	int vpm_us, us_video, vlwm, video_fill_us, cpm_us, us_crt, clwm;

	pclk_freq = arb->pclk_khz;
	mclk_freq = arb->mclk_khz;
	nvclk_freq = arb->nvclk_khz;
	pagemiss = arb->mem_page_miss;
	cas = arb->mem_latency;
	width = arb->memory_width >> 6;
	video_enable = arb->enable_video;
	bpp = arb->pix_bpp;
	mp_enable = arb->enable_mp;
	clwm = 0;
	vlwm = 0;
	cbs = 128;
	pclks = 2;
	nvclks = 2;
	nvclks += 2;
	nvclks += 1;
	mclks = 5;
	mclks += 3;
	mclks += 1;
	mclks += cas;
	mclks += 1;
	mclks += 1;
	mclks += 1;
	mclks += 1;
	mclk_extra = 3;
	nvclks += 2;
	nvclks += 1;
	nvclks += 1;
	nvclks += 1;
	if (mp_enable)
		mclks += 4;
	nvclks += 0;
	pclks += 0;
	found = 0;
	vbs = 0;
	while (found != 1) {
		fifo->valid = true;
		found = 1;
		mclk_loop = mclks + mclk_extra;
		us_m = mclk_loop * 1000 * 1000 / mclk_freq;
		us_n = nvclks * 1000 * 1000 / nvclk_freq;
		us_p = nvclks * 1000 * 1000 / pclk_freq;
		if (video_enable) {
			video_drain_rate = pclk_freq * 2;
			crtc_drain_rate = pclk_freq * bpp / 8;
			vpagemiss = 2;
			vpagemiss += 1;
			crtpagemiss = 2;
			vpm_us = vpagemiss * pagemiss * 1000 * 1000 / mclk_freq;
			if (nvclk_freq * 2 > mclk_freq * width)
				video_fill_us = cbs * 1000 * 1000 / 16 / nvclk_freq;
			else
				video_fill_us = cbs * 1000 * 1000 / (8 * width) / mclk_freq;
			us_video = vpm_us + us_m + us_n + us_p + video_fill_us;
			vlwm = us_video * video_drain_rate / (1000 * 1000);
			vlwm++;
			vbs = 128;
			if (vlwm > 128)
				vbs = 64;
			if (vlwm > (256 - 64))
				vbs = 32;
			if (nvclk_freq * 2 > mclk_freq * width)
				video_fill_us = vbs * 1000 * 1000 / 16 / nvclk_freq;
			else
				video_fill_us = vbs * 1000 * 1000 / (8 * width) / mclk_freq;
			cpm_us = crtpagemiss * pagemiss * 1000 * 1000 / mclk_freq;
			us_crt = us_video + video_fill_us + cpm_us + us_m + us_n + us_p;
			clwm = us_crt * crtc_drain_rate / (1000 * 1000);
			clwm++;
		} else {
			crtc_drain_rate = pclk_freq * bpp / 8;
			crtpagemiss = 2;
			crtpagemiss += 1;
			cpm_us = crtpagemiss * pagemiss * 1000 * 1000 / mclk_freq;
			us_crt = cpm_us + us_m + us_n + us_p;
			clwm = us_crt * crtc_drain_rate / (1000 * 1000);
			clwm++;
		}
		m1 = clwm + cbs - 512;
		p1 = m1 * pclk_freq / mclk_freq;
		p1 = p1 * bpp / 8;
		if ((p1 < m1 && m1 > 0) ||
		    (video_enable && (clwm > 511 || vlwm > 255)) ||
		    (!video_enable && clwm > 519)) {
			fifo->valid = false;
			found = !mclk_extra;
			mclk_extra--;
		}
		if (clwm < 384)
			clwm = 384;
		if (vlwm < 128)
			vlwm = 128;
		fifo->graphics_lwm = clwm;
		fifo->graphics_burst_size = 128;
		fifo->video_lwm = vlwm + 15;
		fifo->video_burst_size = vbs;
	}
}

static void nv10CalcArbitration(struct nv_fifo_info *fifo, struct nv_sim_state *arb)
{
	int pagemiss, width, video_enable, bpp;
	int nvclks, mclks, pclks, vpagemiss, crtpagemiss;
	int nvclk_fill;
	int found, mclk_extra, mclk_loop, cbs, m1;
	int mclk_freq, pclk_freq, nvclk_freq, mp_enable;
	int us_m, us_m_min, us_n, us_p, crtc_drain_rate;
	int vus_m;
	int vpm_us, us_video, cpm_us, us_crt, clwm;
	int clwm_rnd_down;
	int m2us, us_pipe_min, p1clk, p2;
	int min_mclk_extra;
	int us_min_mclk_extra;

	pclk_freq = arb->pclk_khz;	/* freq in KHz */
	mclk_freq = arb->mclk_khz;
	nvclk_freq = arb->nvclk_khz;
	pagemiss = arb->mem_page_miss;
	width = arb->memory_width / 64;
	video_enable = arb->enable_video;
	bpp = arb->pix_bpp;
	mp_enable = arb->enable_mp;
	clwm = 0;
	cbs = 512;
	pclks = 4;	/* lwm detect. */
	nvclks = 3;	/* lwm -> sync. */
	nvclks += 2;	/* fbi bus cycles (1 req + 1 busy) */
	mclks = 1;	/* 2 edge sync.  may be very close to edge so just put one. */
	mclks += 1;	/* arb_hp_req */
	mclks += 5;	/* ap_hp_req   tiling pipeline */
	mclks += 2;	/* tc_req     latency fifo */
	mclks += 2;	/* fb_cas_n_  memory request to fbio block */
	mclks += 7;	/* sm_d_rdv   data returned from fbio block */

	/* fb.rd.d.Put_gc   need to accumulate 256 bits for read */
	if (arb->memory_type == 0) {
		if (arb->memory_width == 64)	/* 64 bit bus */
			mclks += 4;
		else
			mclks += 2;
	} else if (arb->memory_width == 64)	/* 64 bit bus */
		mclks += 2;
	else
		mclks += 1;

	if (!video_enable && arb->memory_width == 128) {
		mclk_extra = (bpp == 32) ? 31 : 42;	/* Margin of error */
		min_mclk_extra = 17;
	} else {
		mclk_extra = (bpp == 32) ? 8 : 4;	/* Margin of error */
		/* mclk_extra = 4; *//* Margin of error */
		min_mclk_extra = 18;
	}

	nvclks += 1;	/* 2 edge sync.  may be very close to edge so just put one. */
	nvclks += 1;	/* fbi_d_rdv_n */
	nvclks += 1;	/* Fbi_d_rdata */
	nvclks += 1;	/* crtfifo load */

	if (mp_enable)
		mclks += 4;	/* Mp can get in with a burst of 8. */
	/* Extra clocks determined by heuristics */

	nvclks += 0;
	pclks += 0;
	found = 0;
	while (found != 1) {
		fifo->valid = true;
		found = 1;
		mclk_loop = mclks + mclk_extra;
		us_m = mclk_loop * 1000 * 1000 / mclk_freq;	/* Mclk latency in us */
		us_m_min = mclks * 1000 * 1000 / mclk_freq;	/* Minimum Mclk latency in us */
		us_min_mclk_extra = min_mclk_extra * 1000 * 1000 / mclk_freq;
		us_n = nvclks * 1000 * 1000 / nvclk_freq;	/* nvclk latency in us */
		us_p = pclks * 1000 * 1000 / pclk_freq;	/* nvclk latency in us */
		us_pipe_min = us_m_min + us_n + us_p;

		vus_m = mclk_loop * 1000 * 1000 / mclk_freq;	/* Mclk latency in us */

		if (video_enable) {
			crtc_drain_rate = pclk_freq * bpp / 8;	/* MB/s */

			vpagemiss = 1;	/* self generating page miss */
			vpagemiss += 1;	/* One higher priority before */

			crtpagemiss = 2;	/* self generating page miss */
			if (mp_enable)
				crtpagemiss += 1;	/* if MA0 conflict */

			vpm_us = vpagemiss * pagemiss * 1000 * 1000 / mclk_freq;

			us_video = vpm_us + vus_m;	/* Video has separate read return path */

			cpm_us = crtpagemiss * pagemiss * 1000 * 1000 / mclk_freq;
			us_crt = us_video	/* Wait for video */
				 + cpm_us	/* CRT Page miss */
				 + us_m + us_n + us_p;	/* other latency */

			clwm = us_crt * crtc_drain_rate / (1000 * 1000);
			clwm++;	/* fixed point <= float_point - 1.  Fixes that */
		} else {
			crtc_drain_rate = pclk_freq * bpp / 8;	/* bpp * pclk/8 */

			crtpagemiss = 1;	/* self generating page miss */
			crtpagemiss += 1;	/* MA0 page miss */
			if (mp_enable)
				crtpagemiss += 1;	/* if MA0 conflict */
			cpm_us = crtpagemiss * pagemiss * 1000 * 1000 / mclk_freq;
			us_crt = cpm_us + us_m + us_n + us_p;
			clwm = us_crt * crtc_drain_rate / (1000 * 1000);
			clwm++;	/* fixed point <= float_point - 1.  Fixes that */

			/* Finally, a heuristic check when width == 64 bits */
			if (width == 1) {
				nvclk_fill = nvclk_freq * 8;
				if (crtc_drain_rate * 100 >= nvclk_fill * 102)
					clwm = 0xfff;	/* Large number to fail */
				else if (crtc_drain_rate * 100 >= nvclk_fill * 98) {
					clwm = 1024;
					cbs = 512;
				}
			}
		}

		/*
		 * Overfill check:
		 */

		clwm_rnd_down = (clwm / 8) * 8;
		if (clwm_rnd_down < clwm)
			clwm += 8;

		m1 = clwm + cbs - 1024;	/* Amount of overfill */
		m2us = us_pipe_min + us_min_mclk_extra;

		/* pclk cycles to drain */
		p1clk = m2us * pclk_freq / (1000 * 1000);
		p2 = p1clk * bpp / 8;	/* bytes drained. */

		if (p2 < m1 && m1 > 0) {
			fifo->valid = false;
			found = 0;
			if (min_mclk_extra == 0) {
				if (cbs <= 32)
					found = 1;	/* Can't adjust anymore! */
				else
					cbs = cbs / 2;	/* reduce the burst size */
			} else
				min_mclk_extra--;
		} else if (clwm > 1023) {	/* Have some margin */
			fifo->valid = false;
			found = 0;
			if (min_mclk_extra == 0)
				found = 1;	/* Can't adjust anymore! */
			else
				min_mclk_extra--;
		}

		if (clwm < (1024 - cbs + 8))
			clwm = 1024 - cbs + 8;
		/*  printf("CRT LWM: prog: 0x%x, bs: 256\n", clwm); */
		fifo->graphics_lwm = clwm;
		fifo->graphics_burst_size = cbs;

		fifo->video_lwm = 1024;
		fifo->video_burst_size = 512;
	}
}

void nv4_10UpdateArbitrationSettings(ScrnInfoPtr pScrn, int VClk, int bpp, uint8_t *burst, uint16_t *lwm)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nv_fifo_info fifo_data;
	struct nv_sim_state sim_data;
	int MClk = nv_get_clock(pScrn, MPLL);
	int NVClk = nv_get_clock(pScrn, NVPLL);
	uint32_t cfg1 = nvReadFB(pNv, NV_PFB_CFG1);

	sim_data.pclk_khz = VClk;
	sim_data.mclk_khz = MClk;
	sim_data.nvclk_khz = NVClk;
	sim_data.pix_bpp = bpp;
	sim_data.enable_mp = false;
	if ((pNv->Chipset & 0xffff) == CHIPSET_NFORCE ||
	    (pNv->Chipset & 0xffff) == CHIPSET_NFORCE2) {
		sim_data.enable_video = false;
		sim_data.memory_type = (PCI_SLOT_READ_LONG(1, 0x7c) >> 12) & 1;
		sim_data.memory_width = 64;
		sim_data.mem_latency = 3;
		sim_data.mem_page_miss = 10;
	} else {
		sim_data.enable_video = (pNv->Architecture != NV_ARCH_04);
		sim_data.memory_type = nvReadFB(pNv, NV_PFB_CFG0) & 0x1;
		sim_data.memory_width = (nvReadEXTDEV(pNv, NV_PEXTDEV_BOOT_0) & 0x10) ? 128 : 64;
		sim_data.mem_latency = cfg1 & 0xf;
		sim_data.mem_page_miss = ((cfg1 >> 4) & 0xf) + ((cfg1 >> 31) & 0x1);
	}

	if (pNv->Architecture == NV_ARCH_04)
		nv4CalcArbitration(&fifo_data, &sim_data);
	else
		nv10CalcArbitration(&fifo_data, &sim_data);

	if (fifo_data.valid) {
		int b = fifo_data.graphics_burst_size >> 4;
		*burst = 0;
		while (b >>= 1)
			(*burst)++;
		*lwm = fifo_data.graphics_lwm >> 3;
	}
}

void nv30UpdateArbitrationSettings(uint8_t *burst, uint16_t *lwm)
{
	unsigned int fifo_size, burst_size, graphics_lwm;

	fifo_size = 2048;
	burst_size = 512;
	graphics_lwm = fifo_size - burst_size;

	*burst = 0;
	burst_size >>= 5;
	while (burst_size >>= 1)
		(*burst)++;
	*lwm = graphics_lwm >> 3;
}

/****************************************************************************\
*                                                                            *
*                          RIVA Mode State Routines                          *
*                                                                            *
\****************************************************************************/

/*
 * Calculate the Video Clock parameters for the PLL.
 */
static void CalcVClock (
    int           clockIn,
    int          *clockOut,
    CARD32         *pllOut,
    NVPtr        pNv
)
{
    unsigned lowM, highM;
    unsigned DeltaNew, DeltaOld;
    unsigned VClk, Freq;
    unsigned M, N, P;
    
    DeltaOld = 0xFFFFFFFF;

    VClk = (unsigned)clockIn;
    
    if (pNv->CrystalFreqKHz == 13500) {
        lowM  = 7;
        highM = 13;
    } else {
        lowM  = 8;
        highM = 14;
    }

    for (P = 0; P <= 4; P++) {
        Freq = VClk << P;
        if ((Freq >= 128000) && (Freq <= 350000)) {
            for (M = lowM; M <= highM; M++) {
                N = ((VClk << P) * M) / pNv->CrystalFreqKHz;
                if(N <= 255) {
                    Freq = ((pNv->CrystalFreqKHz * N) / M) >> P;
                    if (Freq > VClk)
                        DeltaNew = Freq - VClk;
                    else
                        DeltaNew = VClk - Freq;
                    if (DeltaNew < DeltaOld) {
                        *pllOut   = (P << 16) | (N << 8) | M;
                        *clockOut = Freq;
                        DeltaOld  = DeltaNew;
                    }
                }
            }
        }
    }
}

static void CalcVClock2Stage (
    int           clockIn,
    int          *clockOut,
    CARD32         *pllOut,
    CARD32         *pllBOut,
    NVPtr        pNv
)
{
    unsigned DeltaNew, DeltaOld;
    unsigned VClk, Freq;
    unsigned M, N, P;

    DeltaOld = 0xFFFFFFFF;

    *pllBOut = 0x80000401;  /* fixed at x4 for now */

    VClk = (unsigned)clockIn;

    for (P = 0; P <= 6; P++) {
        Freq = VClk << P;
        if ((Freq >= 400000) && (Freq <= 1000000)) {
            for (M = 1; M <= 13; M++) {
                N = ((VClk << P) * M) / (pNv->CrystalFreqKHz << 2);
                if((N >= 5) && (N <= 255)) {
                    Freq = (((pNv->CrystalFreqKHz << 2) * N) / M) >> P;
                    if (Freq > VClk)
                        DeltaNew = Freq - VClk;
                    else
                        DeltaNew = VClk - Freq;
                    if (DeltaNew < DeltaOld) {
                        *pllOut   = (P << 16) | (N << 8) | M;
                        *clockOut = Freq;
                        DeltaOld  = DeltaNew;
                    }
                }
            }
        }
    }
}

/*
 * Calculate extended mode parameters (SVGA) and save in a 
 * mode state structure.
 */
void NVCalcStateExt (
    ScrnInfoPtr pScrn,
    RIVA_HW_STATE *state,
    int            bpp,
    int            width,
    int            hDisplaySize,
    int            height,
    int            dotClock,
    int		   flags 
)
{
	NVPtr pNv = NVPTR(pScrn);
    int pixelDepth, VClk = 0;
	CARD32 CursorStart;

    /*
     * Save mode parameters.
     */
    state->bpp    = bpp;    /* this is not bitsPerPixel, it's 8,15,16,32 */
    state->width  = width;
    state->height = height;
    /*
     * Extended RIVA registers.
     */
    pixelDepth = (bpp + 1)/8;
    if(pNv->two_reg_pll)
        CalcVClock2Stage(dotClock, &VClk, &state->pll, &state->pllB, pNv);
    else
        CalcVClock(dotClock, &VClk, &state->pll, pNv);

    switch (pNv->Architecture)
    {
        case NV_ARCH_04:
            nv4_10UpdateArbitrationSettings(pScrn, VClk,
                                         pixelDepth * 8, 
                                        &(state->arbitration0),
                                        &(state->arbitration1));
            state->cursor0  = 0x00;
            state->cursor1  = 0xbC;
	    if (flags & V_DBLSCAN)
		state->cursor1 |= 2;
            state->cursor2  = 0x00000000;
            state->pllsel   = 0x10000700;
            state->general  = bpp == 16 ? 0x00101100 : 0x00100100;
            state->repaint1 = hDisplaySize < 1280 ? 0x04 : 0x00;
            break;
        case NV_ARCH_10:
        case NV_ARCH_20:
        case NV_ARCH_30:
        default:
            if(((pNv->Chipset & 0xfff0) == CHIPSET_C51) ||
               ((pNv->Chipset & 0xfff0) == CHIPSET_C512))
            {
                state->arbitration0 = 128; 
                state->arbitration1 = 0x0480; 
            } else if(pNv->Architecture < NV_ARCH_30) {
                nv4_10UpdateArbitrationSettings(pScrn, VClk,
                                          pixelDepth * 8, 
                                         &(state->arbitration0),
                                         &(state->arbitration1));
            } else {
                nv30UpdateArbitrationSettings(&(state->arbitration0),
                                         &(state->arbitration1));
            }
            CursorStart = pNv->Cursor->offset;
            state->cursor0  = 0x80 | (CursorStart >> 17);
            state->cursor1  = (CursorStart >> 11) << 2;
	    state->cursor2  = CursorStart >> 24;
	    if (flags & V_DBLSCAN) 
		state->cursor1 |= 2;
            state->pllsel   = 0x10000700;
            state->general  = bpp == 16 ? 0x00101100 : 0x00100100;
            state->repaint1 = hDisplaySize < 1280 ? 0x04 : 0x00;
            break;
    }

    if(bpp != 8) /* DirectColor */
	state->general |= 0x00000030;

    state->repaint0 = (((width / 8) * pixelDepth) & 0x700) >> 3;
    state->pixel    = (pixelDepth > 2) ? 3 : pixelDepth;
}


void NVLoadStateExt (
    ScrnInfoPtr pScrn,
    RIVA_HW_STATE *state
)
{
    NVPtr pNv = NVPTR(pScrn);
    CARD32 temp;

    if(pNv->Architecture >= NV_ARCH_40) {
        switch(pNv->Chipset & 0xfff0) {
        case CHIPSET_NV44:
        case CHIPSET_NV44A:
        case CHIPSET_C51:
        case CHIPSET_G70:
        case CHIPSET_G71:
        case CHIPSET_G72:
        case CHIPSET_G73:
        case CHIPSET_C512:
             temp = nvReadCurRAMDAC(pNv, NV_PRAMDAC_TEST_CONTROL);
             nvWriteCurRAMDAC(pNv, NV_PRAMDAC_TEST_CONTROL, temp | 0x00100000);
             break;
        default:
             break;
        };
    }

    if(pNv->Architecture >= NV_ARCH_10) {
        if(pNv->twoHeads) {
           NVWriteCRTC(pNv, 0, NV_PCRTC_ENGINE_CTRL, state->head);
           NVWriteCRTC(pNv, 1, NV_PCRTC_ENGINE_CTRL, state->head2);
        }
        temp = nvReadCurRAMDAC(pNv, NV_RAMDAC_NV10_CURSYNC);
        nvWriteCurRAMDAC(pNv, NV_RAMDAC_NV10_CURSYNC, temp | (1 << 25));
    
        nvWriteVIDEO(pNv, NV_PVIDEO_STOP, 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_INTR_EN, 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(0), 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(1), 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(0), pNv->VRAMPhysicalSize - 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(1), pNv->VRAMPhysicalSize - 1);
	nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(0), pNv->VRAMPhysicalSize - 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(1), pNv->VRAMPhysicalSize - 1);
        nvWriteMC(pNv, NV_PBUS_POWERCTRL_2, 0);

        nvWriteCurCRTC(pNv, NV_PCRTC_CURSOR_CONFIG, state->cursorConfig);
        nvWriteCurCRTC(pNv, NV_PCRTC_830, state->displayV - 3);
        nvWriteCurCRTC(pNv, NV_PCRTC_834, state->displayV - 1);
    
        if(pNv->FlatPanel) {
           if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
               nvWriteCurRAMDAC(pNv, NV_RAMDAC_DITHER_NV11, state->dither);
           } else 
           if(pNv->twoHeads) {
               nvWriteCurRAMDAC(pNv, NV_RAMDAC_FP_DITHER, state->dither);
           }
    
	   nvWriteCurVGA(pNv, NV_CIO_CRE_53, state->timingH);
	   nvWriteCurVGA(pNv, NV_CIO_CRE_54, state->timingV);
	   nvWriteCurVGA(pNv, NV_CIO_CRE_21, 0xfa);
        }

	nvWriteCurVGA(pNv, NV_CIO_CRE_EBR_INDEX, state->extra);
    }

    nvWriteCurVGA(pNv, NV_CIO_CRE_RPC0_INDEX, state->repaint0);
    nvWriteCurVGA(pNv, NV_CIO_CRE_RPC1_INDEX, state->repaint1);
    nvWriteCurVGA(pNv, NV_CIO_CRE_LSR_INDEX, state->screen);
    nvWriteCurVGA(pNv, NV_CIO_CRE_PIXEL_INDEX, state->pixel);
    nvWriteCurVGA(pNv, NV_CIO_CRE_HEB__INDEX, state->horiz);
    nvWriteCurVGA(pNv, NV_CIO_CRE_ENH_INDEX, state->fifo);
    nvWriteCurVGA(pNv, NV_CIO_CRE_FF_INDEX, state->arbitration0);
    nvWriteCurVGA(pNv, NV_CIO_CRE_FFLWM__INDEX, state->arbitration1);
    if(pNv->Architecture >= NV_ARCH_30) {
      nvWriteCurVGA(pNv, NV_CIO_CRE_47, state->arbitration1 >> 8);
    }

    nvWriteCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR0_INDEX, state->cursor0);
    nvWriteCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR1_INDEX, state->cursor1);
    if(pNv->Architecture == NV_ARCH_40) {  /* HW bug */
       volatile CARD32 curpos = nvReadCurRAMDAC(pNv, NV_PRAMDAC_CU_START_POS);
       nvWriteCurRAMDAC(pNv, NV_PRAMDAC_CU_START_POS, curpos);
    }
    nvWriteCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR2_INDEX, state->cursor2);
    nvWriteCurVGA(pNv, NV_CIO_CRE_ILACE__INDEX, state->interlace);

    if(!pNv->FlatPanel) {
       NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_PLL_COEFF_SELECT, state->pllsel);
       NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_VPLL_COEFF, state->vpll);
       if(pNv->twoHeads)
          NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL2, state->vpll2);
       if(pNv->two_reg_pll) {
          NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B, state->vpllB);
          NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B, state->vpll2B);
       }
    } else {
       nvWriteCurRAMDAC(pNv, NV_PRAMDAC_FP_TG_CONTROL, state->scale);
       nvWriteCurRAMDAC(pNv, NV_PRAMDAC_FP_HCRTC, state->crtcSync);
    }
    nvWriteCurRAMDAC(pNv, NV_PRAMDAC_GENERAL_CONTROL, state->general);

    nvWriteCurCRTC(pNv, NV_PCRTC_INTR_EN_0, 0);
    nvWriteCurCRTC(pNv, NV_PCRTC_INTR_0, NV_PCRTC_INTR_0_VBLANK);
}

void NVUnloadStateExt
(
    NVPtr pNv,
    RIVA_HW_STATE *state
)
{
    state->repaint0     = nvReadCurVGA(pNv, NV_CIO_CRE_RPC0_INDEX);
    state->repaint1     = nvReadCurVGA(pNv, NV_CIO_CRE_RPC1_INDEX);
    state->screen       = nvReadCurVGA(pNv, NV_CIO_CRE_LSR_INDEX);
    state->pixel        = nvReadCurVGA(pNv, NV_CIO_CRE_PIXEL_INDEX);
    state->horiz        = nvReadCurVGA(pNv, NV_CIO_CRE_HEB__INDEX);
    state->fifo         = nvReadCurVGA(pNv, NV_CIO_CRE_ENH_INDEX);
    state->arbitration0 = nvReadCurVGA(pNv, NV_CIO_CRE_FF_INDEX);
    state->arbitration1 = nvReadCurVGA(pNv, NV_CIO_CRE_FFLWM__INDEX);
    if(pNv->Architecture >= NV_ARCH_30) {
       state->arbitration1 |= (nvReadCurVGA(pNv, NV_CIO_CRE_47) & 1) << 8;
    }
    state->cursor0      = nvReadCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR0_INDEX);
    state->cursor1      = nvReadCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR1_INDEX);
    state->cursor2      = nvReadCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR2_INDEX);
    state->interlace    = nvReadCurVGA(pNv, NV_CIO_CRE_ILACE__INDEX);

    state->vpll         = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_VPLL_COEFF);
    if(pNv->twoHeads)
       state->vpll2     = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2);
    if(pNv->two_reg_pll) {
        state->vpllB    = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B);
        state->vpll2B   = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B);
    }
    state->pllsel       = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_PLL_COEFF_SELECT);
    state->general      = nvReadCurRAMDAC(pNv, NV_PRAMDAC_GENERAL_CONTROL);
    state->scale        = nvReadCurRAMDAC(pNv, NV_PRAMDAC_FP_TG_CONTROL);

    if(pNv->Architecture >= NV_ARCH_10) {
        if(pNv->twoHeads) {
           state->head     = NVReadCRTC(pNv, 0, NV_PCRTC_ENGINE_CTRL);
           state->head2    = NVReadCRTC(pNv, 1, NV_PCRTC_ENGINE_CTRL);
           state->crtcOwner = nvReadCurVGA(pNv, NV_CIO_CRE_44);
        }
        state->extra = nvReadCurVGA(pNv, NV_CIO_CRE_EBR_INDEX);

        state->cursorConfig = nvReadCurCRTC(pNv, NV_PCRTC_CURSOR_CONFIG);

        if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
           state->dither = nvReadCurRAMDAC(pNv, NV_RAMDAC_DITHER_NV11);
        } else 
        if(pNv->twoHeads) {
            state->dither = nvReadCurRAMDAC(pNv, NV_RAMDAC_FP_DITHER);
        }

        if(pNv->FlatPanel) {
           state->timingH = nvReadCurVGA(pNv, NV_CIO_CRE_53);
           state->timingV = nvReadCurVGA(pNv, NV_CIO_CRE_54);
        }
    }

    if(pNv->FlatPanel) {
       state->crtcSync = nvReadCurRAMDAC(pNv, NV_PRAMDAC_FP_HCRTC);
    }
}

void NVSetStartAddress (
    NVPtr   pNv,
    CARD32 start
)
{
    nvWriteCurCRTC(pNv, NV_PCRTC_START, start);
}

uint32_t nv_pitch_align(NVPtr pNv, uint32_t width, int bpp)
{
	int mask;

	if (bpp == 15)
		bpp = 16;
	if (bpp == 24)
		bpp = 8;

	/* Alignment requirements taken from the Haiku driver */
	if (pNv->Architecture == NV_ARCH_04)
		mask = 128 / bpp - 1;
	else
		mask = 512 / bpp - 1;

	return (width + mask) & ~mask;
}

void nv_save_restore_vga_fonts(ScrnInfoPtr pScrn, bool save)
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

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%sing VGA fonts\n", save ? "Sav" : "Restor");
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
