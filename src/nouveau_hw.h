/*
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

#ifndef __NOUVEAU_HW_H__
#define __NOUVEAU_HW_H__

#define MASK(field) ((0xffffffff >> (31 - ((1?field) - (0?field)))) << (0?field))
#define XLATE(src, srclowbit, outfield) ((((src) >> (srclowbit)) << (0?outfield)) & MASK(outfield))

#define nvReadMC(pNv, reg) DDXMMIOW("nvReadMC: reg %08x val %08x\n", reg, (uint32_t)MMIO_IN32(pNv->REGS, reg))
#define nvWriteMC(pNv, reg, val) MMIO_OUT32(pNv->REGS, reg, DDXMMIOW("nvWriteMC: reg %08x val %08x\n", reg, val))

#define nvReadVIDEO(pNv, reg) DDXMMIOW("nvReadVIDEO: reg %08x val %08x\n", reg, (uint32_t)MMIO_IN32(pNv->REGS, reg))
#define nvWriteVIDEO(pNv, reg, val) MMIO_OUT32(pNv->REGS, reg, DDXMMIOW("nvWriteVIDEO: reg %08x val %08x\n", reg, val))

#define nvReadFB(pNv, reg) DDXMMIOW("nvReadFB: reg %08x val %08x\n", reg, (uint32_t)MMIO_IN32(pNv->REGS, reg))
#define nvWriteFB(pNv, reg, val) MMIO_OUT32(pNv->REGS, reg, DDXMMIOW("nvWriteFB: reg %08x val %08x\n", reg, val))

#define nvReadEXTDEV(pNv, reg) DDXMMIOW("nvReadEXTDEV: reg %08x val %08x\n", reg, (uint32_t)MMIO_IN32(pNv->REGS, reg))
#define nvWriteEXTDEV(pNv, reg, val) MMIO_OUT32(pNv->REGS, reg, DDXMMIOW("nvWriteEXTDEV: reg %08x val %08x\n", reg, val))

static inline uint32_t NVRead(NVPtr pNv, uint32_t reg)
{
	DDXMMIOW("NVRead: reg %08x val %08x\n", reg, (uint32_t)NV_RD32(pNv->REGS, reg));
	return NV_RD32(pNv->REGS, reg);
}

static inline void NVWrite(NVPtr pNv, uint32_t reg, uint32_t val)
{
	DDXMMIOW("NVWrite: reg %08x val %08x\n", reg, NV_WR32(pNv->REGS, reg, val));
}

static inline uint32_t NVReadCRTC(NVPtr pNv, int head, uint32_t reg)
{
	if (head)
		reg += NV_PCRTC0_SIZE;
	DDXMMIOH("NVReadCRTC: head %d reg %08x val %08x\n", head, reg, (uint32_t)NV_RD32(pNv->REGS, reg));
	return NV_RD32(pNv->REGS, reg);
}

static inline void NVWriteCRTC(NVPtr pNv, int head, uint32_t reg, uint32_t val)
{
	if (head)
		reg += NV_PCRTC0_SIZE;
	DDXMMIOH("NVWriteCRTC: head %d reg %08x val %08x\n", head, reg, val);
	NV_WR32(pNv->REGS, reg, val);
}

static inline uint32_t NVReadRAMDAC(NVPtr pNv, int head, uint32_t reg)
{
	if (head)
		reg += NV_PRAMDAC0_SIZE;
	DDXMMIOH("NVReadRamdac: head %d reg %08x val %08x\n", head, reg, (uint32_t)NV_RD32(pNv->REGS, reg));
	return NV_RD32(pNv->REGS, reg);
}

static inline void
NVWriteRAMDAC(NVPtr pNv, int head, uint32_t reg, uint32_t val)
{
	if (head)
		reg += NV_PRAMDAC0_SIZE;
	DDXMMIOH("NVWriteRamdac: head %d reg %08x val %08x\n", head, reg, val);
	NV_WR32(pNv->REGS, reg, val);
}

static inline uint8_t nv_read_tmds(NVPtr pNv, int or, int dl, uint8_t address)
{
	int ramdac = (or & OUTPUT_C) >> 2;

	NVWriteRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_CONTROL + dl * 8,
	NV_PRAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE | address);
	return NVReadRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_DATA + dl * 8);
}

static inline void
nv_write_tmds(NVPtr pNv, int or, int dl, uint8_t address, uint8_t data)
{
	int ramdac = (or & OUTPUT_C) >> 2;

	NVWriteRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_DATA + dl * 8, data);
	NVWriteRAMDAC(pNv, ramdac, NV_PRAMDAC_FP_TMDS_CONTROL + dl * 8, address);
}

static inline void
NVWriteVgaCrtc(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	DDXMMIOH("NVWriteVgaCrtc: head %d index 0x%02x data 0x%02x\n", head, index, value);
	NV_WR08(pNv->REGS, NV_PRMCIO_CRX__COLOR + head * NV_PRMCIO_SIZE, index);
	NV_WR08(pNv->REGS, NV_PRMCIO_CR__COLOR + head * NV_PRMCIO_SIZE, value);
}

static inline uint8_t NVReadVgaCrtc(NVPtr pNv, int head, uint8_t index)
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

static inline void
NVWriteVgaCrtc5758(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	NVWriteVgaCrtc(pNv, head, NV_CIO_CRE_57, index);
	NVWriteVgaCrtc(pNv, head, NV_CIO_CRE_58, value);
}

static inline uint8_t NVReadVgaCrtc5758(NVPtr pNv, int head, uint8_t index)
{
	NVWriteVgaCrtc(pNv, head, NV_CIO_CRE_57, index);
	return NVReadVgaCrtc(pNv, head, NV_CIO_CRE_58);
}

static inline uint8_t NVReadPRMVIO(NVPtr pNv, int head, uint32_t reg)
{
	/* Only NV4x have two pvio ranges; other twoHeads cards MUST call
	 * NVSetOwner for the relevant head to be programmed */
	if (head && pNv->Architecture == NV_ARCH_40)
		reg += NV_PRMVIO_SIZE;

	DDXMMIOH("NVReadPRMVIO: head %d reg %08x val %02x\n", head, reg, NV_RD08(pNv->REGS, reg));
	return NV_RD08(pNv->REGS, reg);
}

static inline void
NVWritePRMVIO(NVPtr pNv, int head, uint32_t reg, uint8_t value)
{
	/* Only NV4x have two pvio ranges; other twoHeads cards MUST call
	 * NVSetOwner for the relevant head to be programmed */
	if (head && pNv->Architecture == NV_ARCH_40)
		reg += NV_PRMVIO_SIZE;

	DDXMMIOH("NVWritePRMVIO: head %d reg %08x val %02x\n", head, reg, value);
	NV_WR08(pNv->REGS, reg, value);
}

static inline void NVSetEnablePalette(NVPtr pNv, int head, bool enable)
{
	VGA_RD08(pNv->REGS, NV_PRMCIO_INP0__COLOR + head * NV_PRMCIO_SIZE);
	VGA_WR08(pNv->REGS, NV_PRMCIO_ARX + head * NV_PRMCIO_SIZE,
		 enable ? 0 : 0x20);
}

static inline bool NVGetEnablePalette(NVPtr pNv, int head)
{
	VGA_RD08(pNv->REGS, NV_PRMCIO_INP0__COLOR + head * NV_PRMCIO_SIZE);
	return !(VGA_RD08(pNv->REGS, NV_PRMCIO_ARX + head * NV_PRMCIO_SIZE) &
		 0x20);
}

static inline void NVWriteVgaAttr(NVPtr pNv, int head, uint8_t index, uint8_t value)
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

static inline uint8_t NVReadVgaAttr(NVPtr pNv, int head, uint8_t index)
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

static inline void NVVgaSeqReset(NVPtr pNv, int head, bool start)
{
	NVWriteVgaSeq(pNv, head, NV_VIO_SR_RESET_INDEX, start ? 0x1 : 0x3);
}

static inline void NVVgaProtect(NVPtr pNv, int head, bool protect)
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

static inline bool nv_heads_tied(NVPtr pNv)
{
	if (pNv->NVArch == 0x11)
		return !!(nvReadMC(pNv, NV_PBUS_DEBUG_1) & (1 << 28));

	return (NVReadVgaCrtc(pNv, 0, NV_CIO_CRE_44) & 0x4);
}

/* makes cr0-7 on the specified head read-only */
static inline bool nv_lock_vga_crtc_base(NVPtr pNv, int head, bool lock)
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

static inline void nv_lock_vga_crtc_shadow(NVPtr pNv, int head, int lock)
{
	/* shadow lock: connects 0x60?3d? regs to "real" 0x3d? regs
         * bit7: unlocks HDT, HBS, HBE, HRS, HRE, HEB
         * bit6: seems to have some effect on CR09 (double scan, VBS_9)
         * bit5: unlocks HDE
         * bit4: unlocks VDE
         * bit3: unlocks VDT, OVL, VRS, ?VRE?, VBS, VBE, LSR, EBR
         * bit2: same as bit 1 of 0x60?804
         * bit0: same as bit 0 of 0x60?804
         */

	uint8_t cr21 = lock;

	if (lock < 0)
		/* 0xfa is generic "unlock all" mask */
		cr21 = NVReadVgaCrtc(pNv, head, NV_CIO_CRE_21) | 0xfa;

	NVWriteVgaCrtc(pNv, head, NV_CIO_CRE_21, cr21);
}

/* renders the extended crtc regs (cr19+) on all crtcs impervious:
 * immutable and unreadable
 */
static inline bool NVLockVgaCrtcs(NVPtr pNv, bool lock)
{
	bool waslocked = !NVReadVgaCrtc(pNv, 0, NV_CIO_SR_LOCK_INDEX);

	NVWriteVgaCrtc(pNv, 0, NV_CIO_SR_LOCK_INDEX,
		       lock ? NV_CIO_SR_LOCK_VALUE : NV_CIO_SR_UNLOCK_RW_VALUE);
	/* NV11 has independently lockable extended crtcs, except when tied */
	if (pNv->NVArch == 0x11 && !nv_heads_tied(pNv))
		NVWriteVgaCrtc(pNv, 1, NV_CIO_SR_LOCK_INDEX,
			       lock ? NV_CIO_SR_LOCK_VALUE :
				      NV_CIO_SR_UNLOCK_RW_VALUE);

	return waslocked;
}

static inline void nv_fix_nv40_hw_cursor(NVPtr pNv, int head)
{
	/* on some nv40 (such as the "true" (in the NV_PFB_BOOT_0 sense) nv40,
	 * the gf6800gt) a hardware bug requires a write to PRAMDAC_CURSOR_POS
	 * for changes to the CRTC CURCTL regs to take effect, whether changing
	 * the pixmap location, or just showing/hiding the cursor
	 */
	volatile uint32_t curpos = NVReadRAMDAC(pNv, head,
						NV_PRAMDAC_CU_START_POS);
	NVWriteRAMDAC(pNv, head, NV_PRAMDAC_CU_START_POS, curpos);
}

static inline void nv_show_cursor(NVPtr pNv, int head, bool show)
{
	uint8_t *curctl1 =
		&pNv->set_state.head[head].CRTC[NV_CIO_CRE_HCUR_ADDR1_INDEX];

	if (show)
		*curctl1 |= MASK(NV_CIO_CRE_HCUR_ADDR1_ENABLE);
	else
		*curctl1 &= ~MASK(NV_CIO_CRE_HCUR_ADDR1_ENABLE);
	NVWriteVgaCrtc(pNv, head, NV_CIO_CRE_HCUR_ADDR1_INDEX, *curctl1);

	if (pNv->Architecture == NV_ARCH_40)
		nv_fix_nv40_hw_cursor(pNv, head);
}

static inline uint32_t nv_pitch_align(NVPtr pNv, uint32_t width, int bpp)
{
	int mask;

	if (bpp == 15)
		bpp = 16;
	if (bpp == 24)
		bpp = 8;

	/* Alignment requirements taken from the Haiku driver */
	if (pNv->Architecture == NV_ARCH_04)
		mask = 128 / bpp - 1;
	if (pNv->Architecture >= NV_ARCH_50)
		mask = 64 / bpp - 1;
	else
		mask = 512 / bpp - 1;

	return (width + mask) & ~mask;
}

#endif	/* __NOUVEAU_HW_H__ */
