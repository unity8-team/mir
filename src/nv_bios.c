/*
 * Copyright 2005-2006 Erik Waling
 * Copyright 2006 Stephane Marchesin
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

#if defined(__FreeBSD__) || defined(__NetBSD__)
#define bswap_16 bswap16
#define bswap_32 bswap32
#else
#include <byteswap.h>
#endif

typedef struct nouveau_bios bios_t;

/* these defines are made up */
#define NV_CIO_CRE_44_HEADA 0x0
#define NV_CIO_CRE_44_HEADB 0x3
#define FEATURE_MOBILE 0x10

#if 0
#define BIOSLOG(sip, fmt, arg...) xf86DrvMsg(sip->scrnIndex, X_INFO, fmt, ##arg)
#else
#define BIOSLOG(sip, fmt, arg...)
#endif
#define LOG_OLD_VALUE(x) //x

#define BIOS_USLEEP(n) usleep(n)

static int crtchead = 0;

/* this will need remembering across a suspend */
static uint32_t saved_nv_pfb_cfg0;

typedef struct {
	bool execute;
	bool repeat;
} init_exec_t;

static uint16_t le16_to_cpu(const uint16_t x)
{
#if X_BYTE_ORDER == X_BIG_ENDIAN
	return bswap_16(x);
#else
	return x;
#endif
}

static uint32_t le32_to_cpu(const uint32_t x)
{
#if X_BYTE_ORDER == X_BIG_ENDIAN
	return bswap_32(x);
#else
	return x;
#endif
}

static bool nv_cksum(const uint8_t *data, unsigned int length)
{
	/* there's a few checksums in the BIOS, so here's a generic checking function */
	int i;
	uint8_t sum = 0;

	for (i = 0; i < length; i++)
		sum += data[i];

	if (sum)
		return true;

	return false;
}

static int score_vbios(ScrnInfoPtr pScrn, const uint8_t *data)
{
	if (!(data[0] == 0x55 && data[1] == 0xAA)) {
		xf86DrvMsg(pScrn->scrnIndex, X_NOTICE, "... BIOS signature not found\n");
		return 0;
	}

	if (nv_cksum(data, data[2] * 512)) {
		xf86DrvMsg(pScrn->scrnIndex, X_NOTICE, "... BIOS checksum invalid\n");
		return 1;
	} else
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "... appears to be valid\n");

	return 2;
}

static void load_vbios_prom(NVPtr pNv, uint8_t *data)
{
	uint32_t pci_nv_20 = nvReadMC(pNv, NV_PBUS_PCI_NV_20);
	int pcir_ptr;
	int i;

	/* enable ROM access */
	nvWriteMC(pNv, NV_PBUS_PCI_NV_20,
		  pci_nv_20 & ~NV_PBUS_PCI_NV_20_ROM_SHADOW_ENABLED);

	/* bail if no rom signature */
	if (NV_RD08(pNv->REGS, NV_PROM_OFFSET) != 0x55 ||
	    NV_RD08(pNv->REGS, NV_PROM_OFFSET + 1) != 0xaa)
		goto out;

	/* additional check (see note below) - read PCI record header */
	pcir_ptr = NV_RD08(pNv->REGS, NV_PROM_OFFSET + 0x18) |
		   NV_RD08(pNv->REGS, NV_PROM_OFFSET + 0x19) << 8;
	if (NV_RD08(pNv->REGS, NV_PROM_OFFSET + pcir_ptr) != 'P' ||
	    NV_RD08(pNv->REGS, NV_PROM_OFFSET + pcir_ptr + 1) != 'C' ||
	    NV_RD08(pNv->REGS, NV_PROM_OFFSET + pcir_ptr + 2) != 'I' ||
	    NV_RD08(pNv->REGS, NV_PROM_OFFSET + pcir_ptr + 3) != 'R')
		goto out;

	/* on some 6600GT/6800LE prom reads are messed up.  nvclock alleges a
	 * a good read may be obtained by waiting or re-reading (cargocult: 5x)
	 * each byte.  we'll hope pramin has something usable instead
	 */
	for (i = 0; i < NV_PROM_SIZE; i++)
		data[i] = NV_RD08(pNv->REGS, NV_PROM_OFFSET + i);

out:
	/* disable ROM access */
	nvWriteMC(pNv, NV_PBUS_PCI_NV_20,
		  pci_nv_20 | NV_PBUS_PCI_NV_20_ROM_SHADOW_ENABLED);
}

static void load_vbios_pramin(NVPtr pNv, uint8_t *data)
{
	uint32_t old_bar0_pramin = 0;
	int i;

	if (pNv->Architecture >= NV_ARCH_50) {
		uint32_t vbios_vram = (NV_RD32(pNv->REGS, 0x619f04) & ~0xff) << 8;

		if (!vbios_vram)
			vbios_vram = (NV_RD32(pNv->REGS, 0x1700) << 16) + 0xf0000;

		old_bar0_pramin = NV_RD32(pNv->REGS, 0x1700);
		NV_WR32(pNv->REGS, 0x1700, vbios_vram >> 16);
	}

	/* bail if no rom signature */
	if (NV_RD08(pNv->REGS, NV_PRAMIN_OFFSET) != 0x55 ||
	    NV_RD08(pNv->REGS, NV_PRAMIN_OFFSET + 1) != 0xaa)
		goto out;

	for (i = 0; i < NV_PROM_SIZE; i++)
		data[i] = NV_RD08(pNv->REGS, NV_PRAMIN_OFFSET + i);

out:
	if (pNv->Architecture >= NV_ARCH_50)
		NV_WR32(pNv->REGS, 0x1700, old_bar0_pramin);
}

static void load_vbios_pci(NVPtr pNv, uint8_t *data)
{
#if XSERVER_LIBPCIACCESS
	pci_device_read_rom(pNv->PciInfo, data);
#else
	xf86ReadPciBIOS(0, pNv->PciTag, 0, data, NV_PROM_SIZE);
#endif
}

static bool NVShadowVBIOS(ScrnInfoPtr pScrn, uint8_t *data)
{
	NVPtr pNv = NVPTR(pScrn);
	struct methods {
		const char desc[8];
		void (*loadbios)(NVPtr, uint8_t *);
		int score;
	} method[] = {
#ifndef __powerpc__
		{ "PROM", load_vbios_prom },
#endif
		{ "PRAMIN", load_vbios_pramin },
#ifndef __powerpc__
		{ "PCI ROM", load_vbios_pci }
#endif
	};
	int i;

	for (i = 0; i < sizeof(method) / sizeof(struct methods); i++) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Attempting to load BIOS image from %s\n", method[i].desc);
		method[i].loadbios(pNv, data);
		if ((method[i].score = score_vbios(pScrn, data)) == 2)
			return true;
	}

	for (i = 0; i < sizeof(method) / sizeof(struct methods); i++)
		if (method[i].score == 1) {
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				   "Using BIOS image from %s\n", method[i].desc);
			method[i].loadbios(pNv, data);
			return true;
		}

	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid BIOS image found\n");

	return false;
}

typedef struct {
	char* name;
	uint8_t id;
	int length;
	int length_offset;
	int length_multiplier;
	bool (*handler)(ScrnInfoPtr pScrn, bios_t *, uint16_t, init_exec_t *);
} init_tbl_entry_t;

typedef struct {
	uint8_t id[2];
	uint16_t length;
	uint16_t offset;
} bit_entry_t;

static int parse_init_table(ScrnInfoPtr pScrn, bios_t *bios, unsigned int offset, init_exec_t *iexec);

#define MACRO_INDEX_SIZE	2
#define MACRO_SIZE		8
#define CONDITION_SIZE		12
#define IO_FLAG_CONDITION_SIZE	9
#define MEM_INIT_SIZE		66

static void still_alive(void)
{
//	sync();
//	BIOS_USLEEP(2000);
}

static int nv_valid_reg(ScrnInfoPtr pScrn, uint32_t reg)
{
	NVPtr pNv = NVPTR(pScrn);

	/* C51 has misaligned regs on purpose. Marvellous */
	if ((reg & 0x3 && pNv->VBIOS.chip_version != 0x51) ||
	    (reg & 0x2 && pNv->VBIOS.chip_version == 0x51)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "========== misaligned reg 0x%08X ==========\n", reg);
		return 0;
	}
	/* warn on C51 regs that haven't been verified accessible in mmiotracing */
	if (reg & 0x1 && pNv->VBIOS.chip_version == 0x51 &&
	    reg != 0x130d && reg != 0x1311 && reg != 0x60081d)
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "==== C51 misaligned reg 0x%08X not verified ====\n", reg);

	#define WITHIN(x,y,z) ((x>=y)&&(x<=y+z))
	if (WITHIN(reg,NV_PMC_OFFSET,NV_PMC_SIZE))
		return 1;
	if (WITHIN(reg,NV_PBUS_OFFSET,NV_PBUS_SIZE))
		return 1;
	if (WITHIN(reg,NV_PFIFO_OFFSET,NV_PFIFO_SIZE))
		return 1;
	/* maybe a little large, but it will do for the moment. */
	if (pNv->VBIOS.chip_version >= 0x80 && WITHIN(reg, 0x1000, 0xEFFF))
		return 1;
	if (pNv->VBIOS.chip_version >= 0x30 && WITHIN(reg,0x4000,0x600))
		return 1;
	if (pNv->VBIOS.chip_version >= 0x40 && WITHIN(reg,0xc000,0x48))
		return 1;
	if (pNv->VBIOS.chip_version >= 0x17 && reg == 0x0000d204)
		return 1;
	if (pNv->VBIOS.chip_version >= 0x40) {
		if (reg == 0x00011014 || reg == 0x00020328)
			return 1;
		if (WITHIN(reg,0x88000,NV_PBUS_SIZE)) /* new PBUS */
			return 1;
	}
	if (WITHIN(reg,NV_PFB_OFFSET,NV_PFB_SIZE))
		return 1;
	if (WITHIN(reg,NV_PEXTDEV_OFFSET,NV_PEXTDEV_SIZE))
		return 1;
	if (WITHIN(reg,NV_PCRTC0_OFFSET,NV_PCRTC0_SIZE * 2))
		return 1;
	if (pNv->VBIOS.chip_version >= 0x80 && WITHIN(reg, NV50_DISPLAY_OFFSET, NV50_DISPLAY_SIZE))
		return 1;
	if (WITHIN(reg,NV_PRAMDAC0_OFFSET,NV_PRAMDAC0_SIZE * 2))
		return 1;
	if (pNv->VBIOS.chip_version >= 0x17 && reg == 0x0070fff0)
		return 1;
	if (pNv->VBIOS.chip_version == 0x51 && WITHIN(reg,NV_PRAMIN_OFFSET,NV_PRAMIN_SIZE))
		return 1;
	#undef WITHIN

	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "========== unknown reg 0x%08X ==========\n", reg);

	return 0;
}

static bool nv_valid_idx_port(ScrnInfoPtr pScrn, uint16_t port)
{
	/* if adding more ports here, the read/write functions below will need
	 * updating so that the correct mmio range (PRMCIO, PRMDIO, PRMVIO) is
	 * used for the port in question
	 */
	if (port == NV_CIO_CRX__COLOR)
		return true;
	if (port == NV_VIO_SRX)
		return true;

	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "========== unknown indexed io port 0x%04X ==========\n", port);

	return false;
}

static bool nv_valid_port(ScrnInfoPtr pScrn, uint16_t port)
{
	/* if adding more ports here, the read/write functions below will need
	 * updating so that the correct mmio range (PRMCIO, PRMDIO, PRMVIO) is
	 * used for the port in question
	 */
	if (port == NV_VIO_VSE2)
		return true;

	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "========== unknown io port 0x%04X ==========\n", port);

	return false;
}

static uint32_t nv32_rd(ScrnInfoPtr pScrn, uint32_t reg)
{
	NVPtr pNv = NVPTR(pScrn);
	uint32_t data;

	if (!nv_valid_reg(pScrn, reg))
		return 0;

	/* C51 sometimes uses regs with bit0 set in the address. For these
	 * cases there should exist a translation in a BIOS table to an IO
	 * port address which the BIOS uses for accessing the reg
	 *
	 * These only seem to appear for the power control regs to a flat panel,
	 * and the GPIO regs at 0x60081*.  In C51 mmio traces the normal regs
	 * for 0x1308 and 0x1310 are used - hence the mask below.  An S3
	 * suspend-resume mmio trace from a C51 will be required to see if this
	 * is true for the power microcode in 0x14.., or whether the direct IO
	 * port access method is needed
	 */
	if (reg & 0x1)
		reg &= ~0x1;

	data = NV_RD32(pNv->REGS, reg);

	BIOSLOG(pScrn, "	Read:  Reg: 0x%08X, Data: 0x%08X\n", reg, data);

	return data;
}

static void nv32_wr(ScrnInfoPtr pScrn, uint32_t reg, uint32_t data)
{
	NVPtr pNv = NVPTR(pScrn);

	if (!nv_valid_reg(pScrn, reg))
		return;

	/* see note in nv32_rd */
	if (reg & 0x1)
		reg &= 0xfffffffe;

	LOG_OLD_VALUE(nv32_rd(pScrn, reg));
	BIOSLOG(pScrn, "	Write: Reg: 0x%08X, Data: 0x%08X\n", reg, data);

	if (pNv->VBIOS.execute) {
		still_alive();
		NV_WR32(pNv->REGS, reg, data);
	}
}

static uint8_t nv_idx_port_rd(ScrnInfoPtr pScrn, uint16_t port, uint8_t index)
{
	NVPtr pNv = NVPTR(pScrn);
	uint8_t data;

	if (!nv_valid_idx_port(pScrn, port))
		return 0;

	if (port == NV_VIO_SRX)
		data = NVReadVgaSeq(pNv, crtchead, index);
	else	/* assume NV_CIO_CRX__COLOR */
		data = NVReadVgaCrtc(pNv, crtchead, index);

	BIOSLOG(pScrn, "	Indexed IO read:  Port: 0x%04X, Index: 0x%02X, Head: 0x%02X, Data: 0x%02X\n",
		port, index, crtchead, data);

	return data;
}

static void nv_idx_port_wr(ScrnInfoPtr pScrn, uint16_t port, uint8_t index, uint8_t data)
{
	NVPtr pNv = NVPTR(pScrn);

	if (!nv_valid_idx_port(pScrn, port))
		return;

	/* The current head is maintained in a file scope variable crtchead.
	 * We trap changes to CR44 and update the head variable and hence the
	 * register set written.
	 * As CR44 only exists on CRTC0, we update crtchead to head0 in advance
	 * of the write, and to head1 after the write
	 */
	if (port == NV_CIO_CRX__COLOR && index == NV_CIO_CRE_44 && data != NV_CIO_CRE_44_HEADB)
		crtchead = 0;

	LOG_OLD_VALUE(nv_idx_port_rd(pScrn, port, index));
	BIOSLOG(pScrn, "	Indexed IO write: Port: 0x%04X, Index: 0x%02X, Head: 0x%02X, Data: 0x%02X\n",
		port, index, crtchead, data);

	if (pNv->VBIOS.execute) {
		still_alive();
		if (port == NV_VIO_SRX)
			NVWriteVgaSeq(pNv, crtchead, index, data);
		else	/* assume NV_CIO_CRX__COLOR */
			NVWriteVgaCrtc(pNv, crtchead, index, data);
	}

	if (port == NV_CIO_CRX__COLOR && index == NV_CIO_CRE_44 && data == NV_CIO_CRE_44_HEADB)
		crtchead = 1;
}

static uint8_t nv_port_rd(ScrnInfoPtr pScrn, uint16_t port)
{
	NVPtr pNv = NVPTR(pScrn);
	uint8_t data;

	if (!nv_valid_port(pScrn, port))
		return 0;

	data = NVReadPRMVIO(pNv, crtchead, NV_PRMVIO0_OFFSET + port);

	BIOSLOG(pScrn, "	IO read:  Port: 0x%04X, Head: 0x%02X, Data: 0x%02X\n",
		port, crtchead, data);

	return data;
}

static void nv_port_wr(ScrnInfoPtr pScrn, uint16_t port, uint8_t data)
{
	NVPtr pNv = NVPTR(pScrn);

	if (!nv_valid_port(pScrn, port))
		return;

	LOG_OLD_VALUE(nv_port_rd(pScrn, port));
	BIOSLOG(pScrn, "	IO write: Port: 0x%04X, Head: 0x%02X, Data: 0x%02X\n",
		port, crtchead, data);

	if (pNv->VBIOS.execute) {
		still_alive();
		NVWritePRMVIO(pNv, crtchead, NV_PRMVIO0_OFFSET + port, data);
	}
}

static bool bios_condition_met(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, uint8_t cond)
{
	/* The condition table entry has 4 bytes for the address of the
	 * register to check, 4 bytes for a mask to apply to the register and
	 * 4 for a test comparison value
	 */

	uint16_t condptr = bios->condition_tbl_ptr + cond * CONDITION_SIZE;
	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[condptr])));
	uint32_t mask = le32_to_cpu(*((uint32_t *)(&bios->data[condptr + 4])));
	uint32_t cmpval = le32_to_cpu(*((uint32_t *)(&bios->data[condptr + 8])));
	uint32_t data;

	BIOSLOG(pScrn, "0x%04X: Cond: 0x%02X, Reg: 0x%08X, Mask: 0x%08X\n",
		offset, cond, reg, mask);

       	data = nv32_rd(pScrn, reg) & mask;

	BIOSLOG(pScrn, "0x%04X: Checking if 0x%08X equals 0x%08X\n",
		offset, data, cmpval);

	return (data == cmpval);
}

static bool io_flag_condition_met(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, uint8_t cond)
{
	/* The IO flag condition entry has 2 bytes for the CRTC port; 1 byte
	 * for the CRTC index; 1 byte for the mask to apply to the value
	 * retrieved from the CRTC; 1 byte for the shift right to apply to the
	 * masked CRTC value; 2 bytes for the offset to the flag array, to
	 * which the shifted value is added; 1 byte for the mask applied to the
	 * value read from the flag array; and 1 byte for the value to compare
	 * against the masked byte from the flag table.
	 */

	uint16_t condptr = bios->io_flag_condition_tbl_ptr + cond * IO_FLAG_CONDITION_SIZE;
	uint16_t crtcport = le16_to_cpu(*((uint16_t *)(&bios->data[condptr])));
	uint8_t crtcindex = bios->data[condptr + 2];
	uint8_t mask = bios->data[condptr + 3];
	uint8_t shift = bios->data[condptr + 4];
	uint16_t flagarray = le16_to_cpu(*((uint16_t *)(&bios->data[condptr + 5])));
	uint8_t flagarraymask = bios->data[condptr + 7];
	uint8_t cmpval = bios->data[condptr + 8];
	uint8_t data;

	BIOSLOG(pScrn, "0x%04X: Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X, Shift: 0x%02X, FlagArray: 0x%04X, FAMask: 0x%02X, Cmpval: 0x%02X\n",
		offset, crtcport, crtcindex, mask, shift, flagarray, flagarraymask, cmpval);

	data = nv_idx_port_rd(pScrn, crtcport, crtcindex);

	data = bios->data[flagarray + ((data & mask) >> shift)];
	data &= flagarraymask;

	BIOSLOG(pScrn, "0x%04X: Checking if 0x%02X equals 0x%02X\n", offset, data, cmpval);

	return (data == cmpval);
}

int getMNP_single(ScrnInfoPtr pScrn, struct pll_lims *pll_lim, int clk, int *bestNM, int *bestlog2P)
{
	/* Find M, N and P for a single stage PLL
	 *
	 * Note that some bioses (NV3x) have lookup tables of precomputed MNP
	 * values, but we're too lazy to use those atm
	 *
	 * "clk" parameter in kHz
	 * returns calculated clock
	 */

	bios_t *bios = &NVPTR(pScrn)->VBIOS;
	int minvco = pll_lim->vco1.minfreq, maxvco = pll_lim->vco1.maxfreq;
	int minM = pll_lim->vco1.min_m, maxM = pll_lim->vco1.max_m;
	int minN = pll_lim->vco1.min_n, maxN = pll_lim->vco1.max_n;
	int minU = pll_lim->vco1.min_inputfreq, maxU = pll_lim->vco1.max_inputfreq;
	int maxlog2P;
	int crystal = pll_lim->refclk;
	int M, N, log2P, P;
	int clkP, calcclk;
	int delta, bestdelta = INT_MAX;
	int bestclk = 0;

	/* this division verified for nv20, nv18, nv28 (Haiku), and nv34 */
	/* possibly correlated with introduction of 27MHz crystal */
	if (bios->chip_version <= 0x16 || bios->chip_version == 0x20) {
		if (clk > 250000)
			maxM = 6;
		if (clk > 340000)
			maxM = 2;
		maxlog2P = 4;
	} else if (bios->chip_version < 0x40) {
		if (clk > 150000)
			maxM = 6;
		if (clk > 200000)
			maxM = 4;
		if (clk > 340000)
			maxM = 2;
		maxlog2P = 5;
	} else /* nv4x may be subject to the nv17+ limits, but assume not for now */
		maxlog2P = 6;

	if ((clk << maxlog2P) < minvco) {
		minvco = clk << maxlog2P;
		maxvco = minvco * 2;
	}
	if (clk + clk/200 > maxvco)	/* +0.5% */
		maxvco = clk + clk/200;

	/* NV34 goes maxlog2P->0, NV20 goes 0->maxlog2P */
	for (log2P = 0; log2P <= maxlog2P; log2P++) {
		P = 1 << log2P;
		clkP = clk * P;

		if (clkP < minvco)
			continue;
		if (clkP > maxvco)
			return bestclk;

		for (M = minM; M <= maxM; M++) {
			if (crystal/M < minU)
				return bestclk;
			if (crystal/M > maxU)
				continue;

			/* add crystal/2 to round better */
			N = (clkP * M + crystal/2) / crystal;

			if (N < minN)
				continue;
			if (N > maxN)
				break;

			/* more rounding additions */
			calcclk = ((N * crystal + P/2) / P + M/2) / M;
			delta = abs(calcclk - clk);
			/* we do an exhaustive search rather than terminating
			 * on an optimality condition...
			 */
			if (delta < bestdelta) {
				bestdelta = delta;
				bestclk = calcclk;
				*bestNM = N << 8 | M;
				*bestlog2P = log2P;
				if (delta == 0)	/* except this one */
					return bestclk;
			}
		}
	}

	return bestclk;
}

int getMNP_double(ScrnInfoPtr pScrn, struct pll_lims *pll_lim, int clk, int *bestNM1, int *bestNM2, int *bestlog2P)
{
	/* Find M, N and P for a two stage PLL
	 *
	 * Note that some bioses (NV30+) have lookup tables of precomputed MNP
	 * values, but we're too lazy to use those atm
	 *
	 * "clk" parameter in kHz
	 * returns calculated clock
	 */

	int minvco1 = pll_lim->vco1.minfreq, maxvco1 = pll_lim->vco1.maxfreq;
	int minvco2 = pll_lim->vco2.minfreq, maxvco2 = pll_lim->vco2.maxfreq;
	int minU1 = pll_lim->vco1.min_inputfreq, minU2 = pll_lim->vco2.min_inputfreq;
	int maxU1 = pll_lim->vco1.max_inputfreq, maxU2 = pll_lim->vco2.max_inputfreq;
	int minM1 = pll_lim->vco1.min_m, maxM1 = pll_lim->vco1.max_m;
	int minN1 = pll_lim->vco1.min_n, maxN1 = pll_lim->vco1.max_n;
	int minM2 = pll_lim->vco2.min_m, maxM2 = pll_lim->vco2.max_m;
	int minN2 = pll_lim->vco2.min_n, maxN2 = pll_lim->vco2.max_n;
	int crystal = pll_lim->refclk;
	bool fixedgain2 = (minM2 == maxM2 && minN2 == maxN2);
	int M1, N1, M2, N2, log2P;
	int clkP, calcclk1, calcclk2, calcclkout;
	int delta, bestdelta = INT_MAX;
	int bestclk = 0;

	int vco2 = (maxvco2 - maxvco2/200) / 2;
	for (log2P = 0; clk && log2P < 6 && clk <= (vco2 >> log2P); log2P++) /* log2P is maximum of 6 */
		;
	clkP = clk << log2P;

	if (maxvco2 < clk + clk/200)	/* +0.5% */
		maxvco2 = clk + clk/200;

	for (M1 = minM1; M1 <= maxM1; M1++) {
		if (crystal/M1 < minU1)
			return bestclk;
		if (crystal/M1 > maxU1)
			continue;

		for (N1 = minN1; N1 <= maxN1; N1++) {
			calcclk1 = crystal * N1 / M1;
			if (calcclk1 < minvco1)
				continue;
			if (calcclk1 > maxvco1)
				break;

			for (M2 = minM2; M2 <= maxM2; M2++) {
				if (calcclk1/M2 < minU2)
					break;
				if (calcclk1/M2 > maxU2)
					continue;

				/* add calcclk1/2 to round better */
				N2 = (clkP * M2 + calcclk1/2) / calcclk1;
				if (N2 < minN2)
					continue;
				if (N2 > maxN2)
					break;

				if (!fixedgain2) {
					if (NVPTR(pScrn)->VBIOS.chip_version < 0x60)
						if (N2/M2 < 4 || N2/M2 > 10)
							continue;

					calcclk2 = calcclk1 * N2 / M2;
					if (calcclk2 < minvco2)
						break;
					if (calcclk2 > maxvco2)
						continue;
				} else
					calcclk2 = calcclk1;

				calcclkout = calcclk2 >> log2P;
				delta = abs(calcclkout - clk);
				/* we do an exhaustive search rather than terminating
				 * on an optimality condition...
				 */
				if (delta < bestdelta) {
					bestdelta = delta;
					bestclk = calcclkout;
					*bestNM1 = N1 << 8 | M1;
					*bestNM2 = N2 << 8 | M2;
					*bestlog2P = log2P;
					if (delta == 0)	/* except this one */
						return bestclk;
				}
			}
		}
	}

	return bestclk;
}

static void setPLL_single(ScrnInfoPtr pScrn, uint32_t reg, int NM, int log2P)
{
	bios_t *bios = &NVPTR(pScrn)->VBIOS;
	uint32_t oldpll = nv32_rd(pScrn, reg);
	uint32_t pll = (oldpll & 0xfff80000) | log2P << 16 | NM;
	uint32_t saved_powerctrl_1 = 0;
	int shift_powerctrl_1 = -4;

	if (oldpll == pll)
		return;	/* already set */

	/* nv18 doesn't change POWERCTRL_1 for VPLL*; does gf4 need special-casing? */
	if (bios->chip_version >= 0x17 && bios->chip_version != 0x20) {
		switch (reg) {
		case NV_RAMDAC_VPLL2:
			shift_powerctrl_1 += 4;
		case NV_RAMDAC_VPLL:
			shift_powerctrl_1 += 4;
		case NV_RAMDAC_MPLL:
			shift_powerctrl_1 += 4;
		case NV_RAMDAC_NVPLL:
			shift_powerctrl_1 += 4;
		}

		if (shift_powerctrl_1 >= 0) {
			saved_powerctrl_1 = nv32_rd(pScrn, NV_PBUS_POWERCTRL_1);
			nv32_wr(pScrn, NV_PBUS_POWERCTRL_1, (saved_powerctrl_1 & ~(0xf << shift_powerctrl_1)) | 1 << shift_powerctrl_1);
		}
	}

	/* write NM first */
	nv32_wr(pScrn, reg, (oldpll & 0xffff0000) | NM);

	/* wait a bit */
	BIOS_USLEEP(64000);
	nv32_rd(pScrn, reg);

	/* then write P as well */
	nv32_wr(pScrn, reg, pll);

	if (shift_powerctrl_1 >= 0)
		nv32_wr(pScrn, NV_PBUS_POWERCTRL_1, saved_powerctrl_1);
}

static void setPLL_double_highregs(ScrnInfoPtr pScrn, uint32_t reg1, int NM1, int NM2, int log2P)
{
	bios_t *bios = &NVPTR(pScrn)->VBIOS;
	bool nv3035 = bios->chip_version == 0x30 || bios->chip_version == 0x35;
	uint32_t reg2 = reg1 + ((reg1 == NV_RAMDAC_VPLL2) ? 0x5c : 0x70);
	uint32_t oldpll1 = nv32_rd(pScrn, reg1), oldpll2 = !nv3035 ? nv32_rd(pScrn, reg2) : 0;
	uint32_t pll1 = (oldpll1 & 0xfff80000) | log2P << 16 | NM1;
	uint32_t pll2 = !nv3035 ? (oldpll2 & 0x7fff0000) | 1 << 31 | NM2 : 0;
	uint32_t saved_powerctrl_1 = 0, savedc040 = 0, maskc040 = ~0;
	int shift_powerctrl_1 = -1;
	bool single_stage = !NM2 || (((NM2 >> 8) & 0xff) == (NM2 & 0xff));

	if (nv3035)
		pll1 = (pll1 & 0xfcc7ffff) | (NM2 & (0x18 << 8)) << 13 | (NM2 & (0x7 << 8)) << 11 | 8 << 4 | (NM2 & 7) << 4;
	
	if (oldpll1 == pll1 && oldpll2 == pll2)
		return;	/* already set */

	if (reg1 == NV_RAMDAC_NVPLL) {
		shift_powerctrl_1 = 0;
		maskc040 = ~(3 << 20);
	}
	if (reg1 == NV_RAMDAC_MPLL) {
		shift_powerctrl_1 = 4;
		maskc040 = ~(3 << 22);
	}
	if (shift_powerctrl_1 >= 0) {
		saved_powerctrl_1 = nv32_rd(pScrn, NV_PBUS_POWERCTRL_1);
		nv32_wr(pScrn, NV_PBUS_POWERCTRL_1, (saved_powerctrl_1 & ~(0xf << shift_powerctrl_1)) | 1 << shift_powerctrl_1);
	}

	if (bios->chip_version >= 0x40) {
		savedc040 = nv32_rd(pScrn, 0xc040);
		nv32_wr(pScrn, 0xc040, savedc040 & maskc040);

		if (!single_stage) {
			if (reg1 == NV_RAMDAC_VPLL)
				nv32_wr(pScrn, NV_RAMDAC_580, nv32_rd(pScrn, NV_RAMDAC_580) & ~NV_RAMDAC_580_VPLL1_ACTIVE);
			if (reg1 == NV_RAMDAC_VPLL2)
				nv32_wr(pScrn, NV_RAMDAC_580, nv32_rd(pScrn, NV_RAMDAC_580) & ~NV_RAMDAC_580_VPLL2_ACTIVE);
		} else {
			if (reg1 == NV_RAMDAC_VPLL)
				nv32_wr(pScrn, NV_RAMDAC_580, nv32_rd(pScrn, NV_RAMDAC_580) | NV_RAMDAC_580_VPLL1_ACTIVE);
			if (reg1 == NV_RAMDAC_VPLL2)
				nv32_wr(pScrn, NV_RAMDAC_580, nv32_rd(pScrn, NV_RAMDAC_580) | NV_RAMDAC_580_VPLL2_ACTIVE);
			pll2 |= 0x011f;
		}
	}

	if (!nv3035)
		nv32_wr(pScrn, reg2, pll2);
	nv32_wr(pScrn, reg1, pll1);

	if (shift_powerctrl_1 >= 0) {
		nv32_wr(pScrn, NV_PBUS_POWERCTRL_1, saved_powerctrl_1);
		if (bios->chip_version >= 0x40)
			nv32_wr(pScrn, 0xc040, savedc040);
	}
}

static void setPLL_double_lowregs(ScrnInfoPtr pScrn, uint32_t NMNMreg, int NM1, int NM2, int log2P)
{
	/* When setting PLLs, there is a merry game of disabling and enabling
	 * various bits of hardware during the process. This function is a
	 * synthesis of six nv40 traces, nearly each card doing a subtly
	 * different thing. With luck all the necessary bits for each card are
	 * combined herein. Without luck it deviates from each card's formula
	 * so as to not work on any :)
	 */

	uint32_t Preg = NMNMreg - 4;
	uint32_t oldPval = nv32_rd(pScrn, Preg);
	uint32_t NMNM = NM2 << 16 | NM1;
	uint32_t Pval = (oldPval & ((Preg == 0x4020) ? ~(0x11 << 16) : ~(1 << 16))) | 0xc << 28 | log2P << 16;
	uint32_t saved4600 = 0;
	/* some cards have different maskc040s */
	uint32_t maskc040 = ~(3 << 14), savedc040;
	bool single_stage = !NM2 || (((NM2 >> 8) & 0xff) == (NM2 & 0xff));

	if (nv32_rd(pScrn, NMNMreg) == NMNM && (oldPval & 0xc0070000) == Pval)
		return;

	if (Preg == 0x4000)
		maskc040 = ~0x333;
	if (Preg == 0x4058)
		maskc040 = ~(0xc << 24);

	if (Preg == 0x4020) {
		struct pll_lims pll_lim;
		uint8_t Pval2;

		if (get_pll_limits(pScrn, Preg, &pll_lim))
			return;

		Pval2 = log2P + pll_lim.log2p_bias;
		if (Pval2 > pll_lim.max_log2p_bias)
			Pval2 = pll_lim.max_log2p_bias;
		Pval |= 1 << 28 | Pval2 << 20;

		saved4600 = nv32_rd(pScrn, 0x4600);
		nv32_wr(pScrn, 0x4600, saved4600 | 8 << 28);
	}
	if (single_stage)
		Pval |= (Preg == 0x4020) ? 1 << 12 : 1 << 8;

	nv32_wr(pScrn, Preg, oldPval | 1 << 28);
	nv32_wr(pScrn, Preg, Pval & ~(4 << 28));
	if (Preg == 0x4020) {
		Pval |= 8 << 20;
		nv32_wr(pScrn, 0x4020, Pval & ~(0xc << 28));
		nv32_wr(pScrn, 0x4038, Pval & ~(0xc << 28));
	}

	savedc040 = nv32_rd(pScrn, 0xc040);
	nv32_wr(pScrn, 0xc040, savedc040 & maskc040);

	nv32_wr(pScrn, NMNMreg, NMNM);
	if (NMNMreg == 0x4024)
		nv32_wr(pScrn, 0x403c, NMNM);

	nv32_wr(pScrn, Preg, Pval);
	if (Preg == 0x4020) {
		Pval &= ~(8 << 20);
		nv32_wr(pScrn, 0x4020, Pval);
		nv32_wr(pScrn, 0x4038, Pval);
		nv32_wr(pScrn, 0x4600, saved4600);
	}

	nv32_wr(pScrn, 0xc040, savedc040);

	if (Preg == 0x4020) {
		nv32_wr(pScrn, 0x4020, Pval & ~(1 << 28));
		nv32_wr(pScrn, 0x4038, Pval & ~(1 << 28));
	}
}

static int setPLL(ScrnInfoPtr pScrn, bios_t *bios, uint32_t reg, uint32_t clk)
{
	/* clk in kHz */
	struct pll_lims pll_lim;
	int ret, NM1 = 0xbeef, NM2 = 0xdead, log2P;

	/* high regs (such as in the mac g5 table) are not -= 4 */
	if ((ret = get_pll_limits(pScrn, reg > 0x405c ? reg : reg - 4, &pll_lim)))
		return ret;

	if (bios->chip_version >= 0x40 || bios->chip_version == 0x30 ||
	    bios->chip_version == 0x31 || bios->chip_version == 0x35 ||
	    bios->chip_version == 0x36) {
		getMNP_double(pScrn, &pll_lim, clk, &NM1, &NM2, &log2P);
		if (NM2 == 0xdead) {
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
				   "Could not find a suitable set of PLL coefficients, giving up\n");
			return -ERANGE;
		}
		if (reg > 0x405c)
			setPLL_double_highregs(pScrn, reg, NM1, NM2, log2P);
		else
			setPLL_double_lowregs(pScrn, reg, NM1, NM2, log2P);
	} else {
		getMNP_single(pScrn, &pll_lim, clk, &NM1, &log2P);
		setPLL_single(pScrn, reg, NM1, log2P);
	}

	return 0;
}

#if 0
static bool init_prog(ScrnInfoPtr pScrn, bios_t *bios, CARD16 offset, init_exec_t *iexec)
{
	/* INIT_PROG   opcode: 0x31
	 * 
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): reg
	 * offset + 5  (32 bit): and mask
	 * offset + 9  (8  bit): shift right
	 * offset + 10 (8  bit): number of configurations
	 * offset + 11 (32 bit): register
	 * offset + 15 (32 bit): configuration 1
	 * ...
	 * 
	 * Starting at offset + 15 there are "number of configurations"
	 * 32 bit values. To find out which configuration value to use
	 * read "CRTC reg" on the CRTC controller with index "CRTC index"
	 * and bitwise AND this value with "and mask" and then bit shift the
	 * result "shift right" bits to the right.
	 * Assign "register" with appropriate configuration value.
	 */

	CARD32 reg = *((CARD32 *) (&bios->data[offset + 1]));
	CARD32 and = *((CARD32 *) (&bios->data[offset + 5]));
	CARD8 shiftr = *((CARD8 *) (&bios->data[offset + 9]));
	CARD8 nr = *((CARD8 *) (&bios->data[offset + 10]));
	CARD32 reg2 = *((CARD32 *) (&bios->data[offset + 11]));
	CARD8 configuration;
	CARD32 configval, tmp;

	if (iexec->execute) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,  "0x%04X: REG: 0x%04X\n", offset, 
				reg);

		tmp = nv32_rd(pScrn, reg);
		configuration = (tmp & and) >> shiftr;

		xf86DrvMsg(pScrn->scrnIndex, X_INFO,  "0x%04X: CONFIGURATION TO USE: 0x%02X\n", 
				offset, configuration);

		if (configuration <= nr) {

			configval = 
				*((CARD32 *) (&bios->data[offset + 15 + configuration * 4]));

			xf86DrvMsg(pScrn->scrnIndex, X_INFO,  "0x%04X: REG: 0x%08X, VALUE: 0x%08X\n", offset, 
					reg2, configval);
			
			tmp = nv32_rd(pScrn, reg2);
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,  "0x%04X: CURRENT VALUE IS: 0x%08X\n",
				offset, tmp);
			nv32_wr(pScrn, reg2, configval);
		}
	}
	return true;
}
#endif

static bool init_io_restrict_prog(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_IO_RESTRICT_PROG   opcode: 0x32 ('2')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (16 bit): CRTC port
	 * offset + 3  (8  bit): CRTC index
	 * offset + 4  (8  bit): mask
	 * offset + 5  (8  bit): shift
	 * offset + 6  (8  bit): count
	 * offset + 7  (32 bit): register
	 * offset + 11 (32 bit): configuration 1
	 * ...
	 *
	 * Starting at offset + 11 there are "count" 32 bit values.
	 * To find out which value to use read index "CRTC index" on "CRTC port",
	 * AND this value with "mask" and then bit shift right "shift" bits.
	 * Read the appropriate value using this index and write to "register"
	 */

	uint16_t crtcport = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 1])));
	uint8_t crtcindex = bios->data[offset + 3];
	uint8_t mask = bios->data[offset + 4];
	uint8_t shift = bios->data[offset + 5];
	uint8_t count = bios->data[offset + 6];
	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 7])));
	uint8_t config;
	uint32_t configval;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X, Shift: 0x%02X, Count: 0x%02X, Reg: 0x%08X\n",
		offset, crtcport, crtcindex, mask, shift, count, reg);

	config = (nv_idx_port_rd(pScrn, crtcport, crtcindex) & mask) >> shift;
	if (config > count) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "0x%04X: Config 0x%02X exceeds maximal bound 0x%02X\n",
			   offset, config, count);
		return false;
	}

	configval = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 11 + config * 4])));

	BIOSLOG(pScrn, "0x%04X: Writing config %02X\n", offset, config);

	nv32_wr(pScrn, reg, configval);

	return true;
}

static bool init_repeat(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_REPEAT   opcode: 0x33 ('3')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): count
	 *
	 * Execute script following this opcode up to INIT_REPEAT_END
	 * "count" times
	 */

	uint8_t count = bios->data[offset + 1];
	uint8_t i;

	/* no iexec->execute check by design */

	BIOSLOG(pScrn, "0x%04X: Repeating following segment %d times\n", offset, count);

	iexec->repeat = true;

	/* count - 1, as the script block will execute once when we leave this
	 * opcode -- this is compatible with bios behaviour as:
	 * a) the block is always executed at least once, even if count == 0
	 * b) the bios interpreter skips to the op following INIT_END_REPEAT,
	 * while we don't
	 */
	for (i = 0; i < count - 1; i++)
		parse_init_table(pScrn, bios, offset + 2, iexec);

	iexec->repeat = false;

	return true;
}

static bool init_io_restrict_pll(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_IO_RESTRICT_PLL   opcode: 0x34 ('4')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (16 bit): CRTC port
	 * offset + 3  (8  bit): CRTC index
	 * offset + 4  (8  bit): mask
	 * offset + 5  (8  bit): shift
	 * offset + 6  (8  bit): IO flag condition index
	 * offset + 7  (8  bit): count
	 * offset + 8  (32 bit): register
	 * offset + 12 (16 bit): frequency 1
	 * ...
	 *
	 * Starting at offset + 12 there are "count" 16 bit frequencies (10kHz).
	 * Set PLL register "register" to coefficients for frequency n,
	 * selected by reading index "CRTC index" of "CRTC port" ANDed with
	 * "mask" and shifted right by "shift". If "IO flag condition index" > 0,
	 * and condition met, double frequency before setting it.
	 */

	uint16_t crtcport = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 1])));
	uint8_t crtcindex = bios->data[offset + 3];
	uint8_t mask = bios->data[offset + 4];
	uint8_t shift = bios->data[offset + 5];
	int8_t io_flag_condition_idx = bios->data[offset + 6];
	uint8_t count = bios->data[offset + 7];
	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 8])));
	uint8_t config;
	uint16_t freq;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X, Shift: 0x%02X, IO Flag Condition: 0x%02X, Count: 0x%02X, Reg: 0x%08X\n",
		offset, crtcport, crtcindex, mask, shift, io_flag_condition_idx, count, reg);

	config = (nv_idx_port_rd(pScrn, crtcport, crtcindex) & mask) >> shift;
	if (config > count) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "0x%04X: Config 0x%02X exceeds maximal bound 0x%02X\n",
			   offset, config, count);
		return false;
	}

	freq = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 12 + config * 2])));

	if (io_flag_condition_idx > 0) {
		if (io_flag_condition_met(pScrn, bios, offset, io_flag_condition_idx)) {
			BIOSLOG(pScrn, "0x%04X: Condition fulfilled -- frequency doubled\n", offset);
			freq *= 2;
		} else
			BIOSLOG(pScrn, "0x%04X: Condition not fulfilled -- frequency unchanged\n", offset);
	}

	BIOSLOG(pScrn, "0x%04X: Reg: 0x%08X, Config: 0x%02X, Freq: %d0kHz\n",
		offset, reg, config, freq);

	setPLL(pScrn, bios, reg, freq * 10);

	return true;
}

static bool init_end_repeat(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_END_REPEAT   opcode: 0x36 ('6')
	 *
	 * offset      (8 bit): opcode
	 *
	 * Marks the end of the block for INIT_REPEAT to repeat
	 */

	/* no iexec->execute check by design */

	/* iexec->repeat flag necessary to go past INIT_END_REPEAT opcode when
	 * we're not in repeat mode
	 */
	if (iexec->repeat)
		return false;

	return true;
}

static bool init_copy(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_COPY   opcode: 0x37 ('7')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): register
	 * offset + 5  (8  bit): shift
	 * offset + 6  (8  bit): srcmask
	 * offset + 7  (16 bit): CRTC port
	 * offset + 9  (8 bit): CRTC index
	 * offset + 10  (8 bit): mask
	 *
	 * Read index "CRTC index" on "CRTC port", AND with "mask", OR with
	 * (REGVAL("register") >> "shift" & "srcmask") and write-back to CRTC port
	 */

	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint8_t shift = bios->data[offset + 5];
	uint8_t srcmask = bios->data[offset + 6];
	uint16_t crtcport = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 7])));
	uint8_t crtcindex = bios->data[offset + 9];
	uint8_t mask = bios->data[offset + 10];
	uint32_t data;
	uint8_t crtcdata;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Reg: 0x%08X, Shift: 0x%02X, SrcMask: 0x%02X, Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X\n",
		offset, reg, shift, srcmask, crtcport, crtcindex, mask);

	data = nv32_rd(pScrn, reg);

	if (shift < 0x80)
		data >>= shift;
	else
		data <<= (0x100 - shift);

	data &= srcmask;

	crtcdata = (nv_idx_port_rd(pScrn, crtcport, crtcindex) & mask) | (uint8_t)data;
	nv_idx_port_wr(pScrn, crtcport, crtcindex, crtcdata);

	return true;
}

static bool init_not(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_NOT   opcode: 0x38 ('8')
	 *
	 * offset      (8  bit): opcode
	 *
	 * Invert the current execute / no-execute condition (i.e. "else")
	 */
	if (iexec->execute)
		BIOSLOG(pScrn, "0x%04X: ------ Skipping following commands  ------\n", offset);
	else
		BIOSLOG(pScrn, "0x%04X: ------ Executing following commands ------\n", offset);

	iexec->execute = !iexec->execute;
	return true;
}

static bool init_io_flag_condition(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_IO_FLAG_CONDITION   opcode: 0x39 ('9')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): condition number
	 *
	 * Check condition "condition number" in the IO flag condition table.
	 * If condition not met skip subsequent opcodes until condition is
	 * inverted (INIT_NOT), or we hit INIT_RESUME
	 */

	uint8_t cond = bios->data[offset + 1];

	if (!iexec->execute)
		return true;

	if (io_flag_condition_met(pScrn, bios, offset, cond))
		BIOSLOG(pScrn, "0x%04X: Condition fulfilled -- continuing to execute\n", offset);
	else {
		BIOSLOG(pScrn, "0x%04X: Condition not fulfilled -- skipping following commands\n", offset);
		iexec->execute = false;
	}

	return true;
}

static bool init_idx_addr_latched(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_INDEX_ADDRESS_LATCHED   opcode: 0x49 ('I')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): control register
	 * offset + 5  (32 bit): data register
	 * offset + 9  (32 bit): mask
	 * offset + 13 (32 bit): data
	 * offset + 17 (8  bit): count
	 * offset + 18 (8  bit): address 1
	 * offset + 19 (8  bit): data 1
	 * ...
	 *
	 * For each of "count" address and data pairs, write "data n" to "data register",
	 * read the current value of "control register", and write it back once ANDed
	 * with "mask", ORed with "data", and ORed with "address n"
	 */

	uint32_t controlreg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint32_t datareg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 5])));
	uint32_t mask = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 9])));
	uint32_t data = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 13])));
	uint8_t count = bios->data[offset + 17];
	uint32_t value;
	int i;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: ControlReg: 0x%08X, DataReg: 0x%08X, Mask: 0x%08X, Data: 0x%08X, Count: 0x%02X\n",
		offset, controlreg, datareg, mask, data, count);

	for (i = 0; i < count; i++) {
		uint8_t instaddress = bios->data[offset + 18 + i * 2];
		uint8_t instdata = bios->data[offset + 19 + i * 2];

		BIOSLOG(pScrn, "0x%04X: Address: 0x%02X, Data: 0x%02X\n", offset, instaddress, instdata);

		nv32_wr(pScrn, datareg, instdata);
		value = (nv32_rd(pScrn, controlreg) & mask) | data | instaddress;
		nv32_wr(pScrn, controlreg, value);
	}

	return true;
}

static bool init_io_restrict_pll2(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_IO_RESTRICT_PLL2   opcode: 0x4A ('J')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (16 bit): CRTC port
	 * offset + 3  (8  bit): CRTC index
	 * offset + 4  (8  bit): mask
	 * offset + 5  (8  bit): shift
	 * offset + 6  (8  bit): count
	 * offset + 7  (32 bit): register
	 * offset + 11 (32 bit): frequency 1
	 * ...
	 *
	 * Starting at offset + 11 there are "count" 32 bit frequencies (kHz).
	 * Set PLL register "register" to coefficients for frequency n,
	 * selected by reading index "CRTC index" of "CRTC port" ANDed with
	 * "mask" and shifted right by "shift".
	 */

	uint16_t crtcport = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 1])));
	uint8_t crtcindex = bios->data[offset + 3];
	uint8_t mask = bios->data[offset + 4];
	uint8_t shift = bios->data[offset + 5];
	uint8_t count = bios->data[offset + 6];
	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 7])));
	uint8_t config;
	uint32_t freq;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X, Shift: 0x%02X, Count: 0x%02X, Reg: 0x%08X\n",
		offset, crtcport, crtcindex, mask, shift, count, reg);

	if (!reg)
		return true;

	config = (nv_idx_port_rd(pScrn, crtcport, crtcindex) & mask) >> shift;
	if (config > count) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "0x%04X: Config 0x%02X exceeds maximal bound 0x%02X\n",
			   offset, config, count);
		return false;
	}

	freq = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 11 + config * 4])));

	BIOSLOG(pScrn, "0x%04X: Reg: 0x%08X, Config: 0x%02X, Freq: %dkHz\n",
		offset, reg, config, freq);

	setPLL(pScrn, bios, reg, freq);

	return true;
}

static bool init_pll2(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_PLL2   opcode: 0x4B ('K')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): register
	 * offset + 5  (32 bit): freq
	 *
	 * Set PLL register "register" to coefficients for frequency "freq"
	 */

	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint32_t freq = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 5])));

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Reg: 0x%04X, Freq: %dkHz\n",
		offset, reg, freq);

	setPLL(pScrn, bios, reg, freq);

	return true;
}

static int dcb_entry_idx_from_crtchead(ScrnInfoPtr pScrn)
{
	/* for the results of this function to be correct, CR44 must have been
	 * set (using nv_idx_port_wr to set crtchead), CR58 set for CR57 = 0,
	 * and the DCB table parsed, before the script calling the function is
	 * run.  run_digital_op_script is example of how to do such setup
	 */

	uint8_t dcb_entry = NVReadVgaCrtc5758(NVPTR(pScrn), crtchead, 0);

	if (dcb_entry > NVPTR(pScrn)->dcb_table.entries) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "CR58 doesn't have a valid DCB entry currently (%02X)\n",
			   dcb_entry);
		dcb_entry = 0x7f;	/* unused / invalid marker */
	}

	return dcb_entry;
}

static int init_dcb_i2c_entry(ScrnInfoPtr pScrn, bios_t *bios, int index);

static int
create_i2c_device(ScrnInfoPtr pScrn, bios_t *bios, int i2c_index, int address, I2CDevRec *i2cdev)
{
	NVPtr pNv = NVPTR(pScrn);
	int ret;

	if (i2c_index == 0xff) {
		/* note: dcb_entry_idx_from_crtchead needs pre-script set-up */
		int idx = dcb_entry_idx_from_crtchead(pScrn), shift = 0;
		int default_indices = pNv->dcb_table.i2c_default_indices;

		if (idx != 0x7f && pNv->dcb_table.entry[idx].i2c_upper_default)
			shift = 4;

		i2c_index = (default_indices >> shift) & 0xf;
	}

	if ((ret = init_dcb_i2c_entry(pScrn, bios, i2c_index)))
		return ret;

	memset(i2cdev, 0, sizeof(I2CDevRec));
	i2cdev->DevName = "init script device";
	i2cdev->pI2CBus = pNv->dcb_table.i2c[i2c_index].chan;
	i2cdev->SlaveAddr = address;
	if (!xf86I2CDevInit(i2cdev)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Couldn't add I2C device\n");
		return -EINVAL;
	}

	return 0;
}

static bool init_i2c_byte(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_I2C_BYTE   opcode: 0x4C ('L')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): DCB I2C table entry index
	 * offset + 2  (8 bit): I2C slave address
	 * offset + 3  (8 bit): count
	 * offset + 4  (8 bit): I2C register 1
	 * offset + 5  (8 bit): mask 1
	 * offset + 6  (8 bit): data 1
	 * ...
	 *
	 * For each of "count" registers given by "I2C register n" on the device
	 * addressed by "I2C slave address" on the I2C bus given by
	 * "DCB I2C table entry index", read the register, AND the result with
	 * "mask n" and OR it with "data n" before writing it back to the device
	 */

	uint8_t i2c_index = bios->data[offset + 1];
	uint8_t i2c_address = bios->data[offset + 2];
	uint8_t count = bios->data[offset + 3];
	I2CDevRec i2cdev;
	int i;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: DCBI2CIndex: 0x%02X, I2CAddress: 0x%02X, Count: 0x%02X\n",
		offset, i2c_index, i2c_address, count);

	if (create_i2c_device(pScrn, bios, i2c_index, i2c_address, &i2cdev))
		return false;

	for (i = 0; i < count; i++) {
		uint8_t i2c_reg = bios->data[offset + 4 + i * 3];
		uint8_t mask = bios->data[offset + 5 + i * 3];
		uint8_t data = bios->data[offset + 6 + i * 3];
		uint8_t value;

		xf86I2CReadByte(&i2cdev, i2c_reg, &value);

		BIOSLOG(pScrn, "0x%04X: I2CReg: 0x%02X, Value: 0x%02X, Mask: 0x%02X, Data: 0x%02X\n",
			offset, i2c_reg, value, mask, data);

		value = (value & mask) | data;

		if (bios->execute)
			xf86I2CWriteByte(&i2cdev, i2c_reg, value);
	}

	xf86DestroyI2CDevRec(&i2cdev, FALSE);

	return true;
}

static bool init_zm_i2c_byte(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_ZM_I2C_BYTE   opcode: 0x4D ('M')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): DCB I2C table entry index
	 * offset + 2  (8 bit): I2C slave address
	 * offset + 3  (8 bit): count
	 * offset + 4  (8 bit): I2C register 1
	 * offset + 5  (8 bit): data 1
	 * ...
	 *
	 * For each of "count" registers given by "I2C register n" on the device
	 * addressed by "I2C slave address" on the I2C bus given by
	 * "DCB I2C table entry index", set the register to "data n"
	 */

	uint8_t i2c_index = bios->data[offset + 1];
	uint8_t i2c_address = bios->data[offset + 2];
	uint8_t count = bios->data[offset + 3];
	I2CDevRec i2cdev;
	int i;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: DCBI2CIndex: 0x%02X, I2CAddress: 0x%02X, Count: 0x%02X\n",
		offset, i2c_index, i2c_address, count);

	if (create_i2c_device(pScrn, bios, i2c_index, i2c_address, &i2cdev))
		return false;

	for (i = 0; i < count; i++) {
		uint8_t i2c_reg = bios->data[offset + 4 + i * 2];
		uint8_t data = bios->data[offset + 5 + i * 2];

		BIOSLOG(pScrn, "0x%04X: I2CReg: 0x%02X, Data: 0x%02X\n",
			offset, i2c_reg, data);

		if (bios->execute)
			if (!xf86I2CWriteByte(&i2cdev, i2c_reg, data))
				break;
	}

	xf86DestroyI2CDevRec(&i2cdev, FALSE);

	return true;
}

static bool init_zm_i2c(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_ZM_I2C   opcode: 0x4E ('N')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): DCB I2C table entry index
	 * offset + 2  (8 bit): I2C slave address
	 * offset + 3  (8 bit): count
	 * offset + 4  (8 bit): data 1
	 * ...
	 *
	 * Send "count" bytes ("data n") to the device addressed by "I2C slave
	 * address" on the I2C bus given by "DCB I2C table entry index"
	 */

	uint8_t i2c_index = bios->data[offset + 1];
	uint8_t i2c_address = bios->data[offset + 2];
	uint8_t count = bios->data[offset + 3];
	I2CDevRec i2cdev;
	uint8_t data[UINT8_MAX];
	int i;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: DCBI2CIndex: 0x%02X, I2CAddress: 0x%02X, Count: 0x%02X\n",
		offset, i2c_index, i2c_address, count);

	if (create_i2c_device(pScrn, bios, i2c_index, i2c_address, &i2cdev))
		return false;

	for (i = 0; i < count; i++) {
		data[i] = bios->data[offset + 4 + i];

		BIOSLOG(pScrn, "0x%04X: Data: 0x%02X\n", offset, data[i]);
	}

	if (bios->execute)
		xf86I2CWrite(&i2cdev, data, count);

	xf86DestroyI2CDevRec(&i2cdev, FALSE);

	return true;
}

static uint32_t get_tmds_index_reg(ScrnInfoPtr pScrn, uint8_t mlv)
{
	/* For mlv < 0x80, it is an index into a table of TMDS base addresses
	 * For mlv == 0x80 use the "or" value of the dcb_entry indexed by CR58 for CR57 = 0
	 * to index a table of offsets to the basic 0x6808b0 address
	 * For mlv == 0x81 use the "or" value of the dcb_entry indexed by CR58 for CR57 = 0
	 * to index a table of offsets to the basic 0x6808b0 address, and then flip the offset by 8
	 */

	NVPtr pNv = NVPTR(pScrn);
	const int pramdac_offset[13] = {0, 0, 0x8, 0, 0x2000, 0, 0, 0, 0x2008, 0, 0, 0, 0x2000};
	const uint32_t pramdac_table[4] = {0x6808b0, 0x6808b8, 0x6828b0, 0x6828b8};

	if (mlv >= 0x80) {
		int dcb_entry, dacoffset;

		/* note: dcb_entry_idx_from_crtchead needs pre-script set-up */
		if ((dcb_entry = dcb_entry_idx_from_crtchead(pScrn)) == 0x7f)
			return 0;
		dacoffset = pramdac_offset[pNv->dcb_table.entry[dcb_entry].or];
		if (mlv == 0x81)
			dacoffset ^= 8;
		return (0x6808b0 + dacoffset);
	} else {
		if (mlv > (sizeof(pramdac_table) / sizeof(uint32_t))) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Magic Lookup Value too big (%02X)\n", mlv);
			return 0;
		}
		return pramdac_table[mlv];
	}
}

static bool init_tmds(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_TMDS   opcode: 0x4F ('O')	(non-canon name)
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): magic lookup value
	 * offset + 2  (8 bit): TMDS address
	 * offset + 3  (8 bit): mask
	 * offset + 4  (8 bit): data
	 *
	 * Read the data reg for TMDS address "TMDS address", AND it with mask
	 * and OR it with data, then write it back
	 * "magic lookup value" determines which TMDS base address register is used --
	 * see get_tmds_index_reg()
	 */

	uint8_t mlv = bios->data[offset + 1];
	uint32_t tmdsaddr = bios->data[offset + 2];
	uint8_t mask = bios->data[offset + 3];
	uint8_t data = bios->data[offset + 4];
	uint32_t reg, value;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: MagicLookupValue: 0x%02X, TMDSAddr: 0x%02X, Mask: 0x%02X, Data: 0x%02X\n",
		offset, mlv, tmdsaddr, mask, data);

	if (!(reg = get_tmds_index_reg(pScrn, mlv)))
		return false;

	nv32_wr(pScrn, reg, tmdsaddr | NV_RAMDAC_FP_TMDS_CONTROL_WRITE_DISABLE);
	value = (nv32_rd(pScrn, reg + 4) & mask) | data;
	nv32_wr(pScrn, reg + 4, value);
	nv32_wr(pScrn, reg, tmdsaddr);

	return true;
}

static bool init_zm_tmds_group(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_ZM_TMDS_GROUP   opcode: 0x50 ('P')	(non-canon name)
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): magic lookup value
	 * offset + 2  (8 bit): count
	 * offset + 3  (8 bit): addr 1
	 * offset + 4  (8 bit): data 1
	 * ...
	 *
	 * For each of "count" TMDS address and data pairs write "data n" to "addr n"
	 * "magic lookup value" determines which TMDS base address register is used --
	 * see get_tmds_index_reg()
	 */

	uint8_t mlv = bios->data[offset + 1];
	uint8_t count = bios->data[offset + 2];
	uint32_t reg;
	int i;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: MagicLookupValue: 0x%02X, Count: 0x%02X\n",
		offset, mlv, count);

	if (!(reg = get_tmds_index_reg(pScrn, mlv)))
		return false;

	for (i = 0; i < count; i++) {
		uint8_t tmdsaddr = bios->data[offset + 3 + i * 2];
		uint8_t tmdsdata = bios->data[offset + 4 + i * 2];

		nv32_wr(pScrn, reg + 4, tmdsdata);
		nv32_wr(pScrn, reg, tmdsaddr);
	}

	return true;
}

static bool init_cr_idx_adr_latch(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_CR_INDEX_ADDRESS_LATCHED   opcode: 0x51 ('Q')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): CRTC index1
	 * offset + 2  (8 bit): CRTC index2
	 * offset + 3  (8 bit): baseaddr
	 * offset + 4  (8 bit): count
	 * offset + 5  (8 bit): data 1
	 * ...
	 *
	 * For each of "count" address and data pairs, write "baseaddr + n" to
	 * "CRTC index1" and "data n" to "CRTC index2"
	 * Once complete, restore initial value read from "CRTC index1"
	 */
	uint8_t crtcindex1 = bios->data[offset + 1];
	uint8_t crtcindex2 = bios->data[offset + 2];
	uint8_t baseaddr = bios->data[offset + 3];
	uint8_t count = bios->data[offset + 4];
	uint8_t oldaddr, data;
	int i;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Index1: 0x%02X, Index2: 0x%02X, BaseAddr: 0x%02X, Count: 0x%02X\n",
		offset, crtcindex1, crtcindex2, baseaddr, count);

	oldaddr = nv_idx_port_rd(pScrn, NV_CIO_CRX__COLOR, crtcindex1);

	for (i = 0; i < count; i++) {
		nv_idx_port_wr(pScrn, NV_CIO_CRX__COLOR, crtcindex1, baseaddr + i);

		data = bios->data[offset + 5 + i];
		nv_idx_port_wr(pScrn, NV_CIO_CRX__COLOR, crtcindex2, data);
	}

	nv_idx_port_wr(pScrn, NV_CIO_CRX__COLOR, crtcindex1, oldaddr);

	return true;
}

static bool init_cr(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_CR   opcode: 0x52 ('R')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (8  bit): CRTC index
	 * offset + 2  (8  bit): mask
	 * offset + 3  (8  bit): data
	 *
	 * Assign the value of at "CRTC index" ANDed with mask and ORed with data
	 * back to "CRTC index"
	 */

	uint8_t crtcindex = bios->data[offset + 1];
	uint8_t mask = bios->data[offset + 2];
	uint8_t data = bios->data[offset + 3];
	uint8_t value;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Index: 0x%02X, Mask: 0x%02X, Data: 0x%02X\n",
		offset, crtcindex, mask, data);

	value = (nv_idx_port_rd(pScrn, NV_CIO_CRX__COLOR, crtcindex) & mask) | data;
	nv_idx_port_wr(pScrn, NV_CIO_CRX__COLOR, crtcindex, value);

	return true;
}

static bool init_zm_cr(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_ZM_CR   opcode: 0x53 ('S')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): CRTC index
	 * offset + 2  (8 bit): value
	 *
	 * Assign "value" to CRTC register with index "CRTC index".
	 */

	uint8_t crtcindex = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint8_t data = bios->data[offset + 2];

	if (!iexec->execute)
		return true;

	nv_idx_port_wr(pScrn, NV_CIO_CRX__COLOR, crtcindex, data);

	return true;
}

static bool init_zm_cr_group(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_ZM_CR_GROUP   opcode: 0x54 ('T')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): count
	 * offset + 2  (8 bit): CRTC index 1
	 * offset + 3  (8 bit): value 1
	 * ...
	 *
	 * For "count", assign "value n" to CRTC register with index "CRTC index n".
	 */
    
	uint8_t count = bios->data[offset + 1];
	int i;

	if (!iexec->execute)
		return true;

	for (i = 0; i < count; i++)
		init_zm_cr(pScrn, bios, offset + 2 + 2 * i - 1, iexec);

	return true;
}

static bool init_condition_time(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_CONDITION_TIME   opcode: 0x56 ('V')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): condition number
	 * offset + 2  (8 bit): retries / 50
	 *
	 * Check condition "condition number" in the condition table.
	 * Bios code then sleeps for 2ms if the condition is not met, and
	 * repeats up to "retries" times, but on one C51 this has proved
	 * insufficient.  In mmiotraces the driver sleeps for 20ms, so we do
	 * this, and bail after "retries" times, or 2s, whichever is less.
	 * If still not met after retries, clear execution flag for this table.
	 */

	uint8_t cond = bios->data[offset + 1];
	uint16_t retries = bios->data[offset + 2] * 50;

	if (!iexec->execute)
		return true;

	if (retries > 100)
		retries = 100;

	BIOSLOG(pScrn, "0x%04X: Condition: 0x%02X, Retries: 0x%02X\n", offset, cond, retries);

	for (; retries > 0; retries--)
		if (bios_condition_met(pScrn, bios, offset, cond)) {
			BIOSLOG(pScrn, "0x%04X: Condition met, continuing\n", offset);
			break;
		} else {
			BIOSLOG(pScrn, "0x%04X: Condition not met, sleeping for 20ms\n", offset);
			BIOS_USLEEP(20000);
		}

	if (!bios_condition_met(pScrn, bios, offset, cond)) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "0x%04X: Condition still not met after %dms, skiping following opcodes\n",
			   offset, 20 * retries);
		iexec->execute = false;
	}

	return true;
}

static bool init_zm_reg_sequence(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_ZM_REG_SEQUENCE   opcode: 0x58 ('X')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): base register
	 * offset + 5  (8  bit): count
	 * offset + 6  (32 bit): value 1
	 * ...
	 *
	 * Starting at offset + 6 there are "count" 32 bit values.
	 * For "count" iterations set "base register" + 4 * current_iteration
	 * to "value current_iteration"
	 */

	uint32_t basereg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint32_t count = bios->data[offset + 5];
	int i;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: BaseReg: 0x%08X, Count: 0x%02X\n", offset, basereg, count);

	for (i = 0; i < count; i++) {
		uint32_t reg = basereg + i * 4;
		uint32_t data = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 6 + i * 4])));

		nv32_wr(pScrn, reg, data);
	}

	return true;
}

#if 0
static bool init_indirect_reg(ScrnInfoPtr pScrn, bios_t *bios, CARD16 offset, init_exec_t *iexec)
{
	/* INIT_INDIRECT_REG opcode: 0x5A
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): register
	 * offset + 5  (16 bit): adress offset (in bios)
	 *
	 * Lookup value at offset data in the bios and write it to reg
	 */
	CARD32 reg = *((CARD32 *) (&bios->data[offset + 1]));
	CARD16 data = le16_to_cpu(*((CARD16 *) (&bios->data[offset + 5])));
	CARD32 data2 = bios->data[data];

	if (iexec->execute) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,  
				"0x%04X: REG: 0x%04X, DATA AT: 0x%04X, VALUE IS: 0x%08X\n", 
				offset, reg, data, data2);

		if (DEBUGLEVEL >= 6) {
			CARD32 tmpval;
			tmpval = nv32_rd(pScrn, reg);
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,  "0x%04X: CURRENT VALUE IS: 0x%08X\n", offset, tmpval);
		}

		nv32_wr(pScrn, reg, data2);
	}
	return true;
}
#endif

static bool init_sub_direct(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_SUB_DIRECT   opcode: 0x5B ('[')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (16 bit): subroutine offset (in bios)
	 *
	 * Calls a subroutine that will execute commands until INIT_DONE
	 * is found. 
	 */

	uint16_t sub_offset = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 1])));

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Executing subroutine at 0x%04X\n", offset, sub_offset);

	parse_init_table(pScrn, bios, sub_offset, iexec);

	BIOSLOG(pScrn, "0x%04X: End of 0x%04X subroutine\n", offset, sub_offset);

	return true;
}

static bool init_copy_nv_reg(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_COPY_NV_REG   opcode: 0x5F ('_')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): src reg
	 * offset + 5  (8  bit): shift
	 * offset + 6  (32 bit): src mask
	 * offset + 10 (32 bit): xor
	 * offset + 14 (32 bit): dst reg
	 * offset + 18 (32 bit): dst mask
	 *
	 * Shift REGVAL("src reg") right by (signed) "shift", AND result with
	 * "src mask", then XOR with "xor". Write this OR'd with
	 * (REGVAL("dst reg") AND'd with "dst mask") to "dst reg"
	 */

	uint32_t srcreg = *((uint32_t *)(&bios->data[offset + 1]));
	uint8_t shift = bios->data[offset + 5];
	uint32_t srcmask = *((uint32_t *)(&bios->data[offset + 6]));
	uint32_t xor = *((uint32_t *)(&bios->data[offset + 10]));
	uint32_t dstreg = *((uint32_t *)(&bios->data[offset + 14]));
	uint32_t dstmask = *((uint32_t *)(&bios->data[offset + 18]));
	uint32_t srcvalue, dstvalue;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: SrcReg: 0x%08X, Shift: 0x%02X, SrcMask: 0x%08X, Xor: 0x%08X, DstReg: 0x%08X, DstMask: 0x%08X\n",
		offset, srcreg, shift, srcmask, xor, dstreg, dstmask);

	srcvalue = nv32_rd(pScrn, srcreg);

	if (shift < 0x80)
		srcvalue >>= shift;
	else
		srcvalue <<= (0x100 - shift);

	srcvalue = (srcvalue & srcmask) ^ xor;

	dstvalue = nv32_rd(pScrn, dstreg) & dstmask;

	nv32_wr(pScrn, dstreg, dstvalue | srcvalue);

	return true;
}

static bool init_zm_index_io(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_ZM_INDEX_IO   opcode: 0x62 ('b')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (16 bit): CRTC port
	 * offset + 3  (8  bit): CRTC index
	 * offset + 4  (8  bit): data
	 *
	 * Write "data" to index "CRTC index" of "CRTC port"
	 */
	uint16_t crtcport = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 1])));
	uint8_t crtcindex = bios->data[offset + 3];
	uint8_t data = bios->data[offset + 4];

	if (!iexec->execute)
		return true;

	nv_idx_port_wr(pScrn, crtcport, crtcindex, data);

	return true;
}

static bool init_compute_mem(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_COMPUTE_MEM   opcode: 0x63 ('c')
	 *
	 * offset      (8 bit): opcode
	 *
	 * This opcode is meant to set NV_PFB_CFG0 (0x100200) appropriately so
	 * that the hardware can correctly calculate how much VRAM it has
	 * (and subsequently report that value in NV_PFB_CSTATUS (0x10020C))
	 *
	 * The implementation of this opcode in general consists of two parts:
	 * 1) determination of the memory bus width
	 * 2) determination of how many of the card's RAM pads have ICs attached
	 *
	 * 1) is done by a cunning combination of writes to offsets 0x1c and
	 * 0x3c in the framebuffer, and seeing whether the written values are
	 * read back correctly. This then affects bits 4-7 of NV_PFB_CFG0
	 *
	 * 2) is done by a cunning combination of writes to an offset slightly
	 * less than the maximum memory reported by NV_PFB_CSTATUS, then seeing
	 * if the test pattern can be read back. This then affects bits 12-15 of
	 * NV_PFB_CFG0
	 *
	 * In this context a "cunning combination" may include multiple reads
	 * and writes to varying locations, often alternating the test pattern
	 * and 0, doubtless to make sure buffers are filled, residual charges
	 * on tracks are removed etc.
	 *
	 * Unfortunately, the "cunning combination"s mentioned above, and the
	 * changes to the bits in NV_PFB_CFG0 differ with nearly every bios
	 * trace I have.
	 *
	 * Therefore, we cheat and assume the value of NV_PFB_CFG0 with which
	 * we started was correct, and use that instead
	 */

	/* no iexec->execute check by design */

	/* on every card I've seen, this step gets done for us earlier in the init scripts
	uint8_t crdata = nv_idx_port_rd(pScrn, NV_VIO_SRX, 0x01);
	nv_idx_port_wr(pScrn, NV_VIO_SRX, 0x01, crdata | 0x20);
	*/

	/* this also has probably been done in the scripts, but an mmio trace of
	 * s3 resume shows nvidia doing it anyway (unlike the NV_VIO_SRX write)
	 */
	nv32_wr(pScrn, NV_PFB_REFCTRL, NV_PFB_REFCTRL_VALID_1);

	/* write back the saved configuration value */
	nv32_wr(pScrn, NV_PFB_CFG0, saved_nv_pfb_cfg0);

	return true;
}

static bool init_reset(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_RESET   opcode: 0x65 ('e')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): register
	 * offset + 5  (32 bit): value1
	 * offset + 9  (32 bit): value2
	 *
	 * Assign "value1" to "register", then assign "value2" to "register"
	 */

	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint32_t value1 = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 5])));
	uint32_t value2 = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 9])));
	uint32_t pci_nv_19, pci_nv_20;

	/* no iexec->execute check by design */

	pci_nv_19 = nv32_rd(pScrn, NV_PBUS_PCI_NV_19);
	nv32_wr(pScrn, NV_PBUS_PCI_NV_19, 0);
	nv32_wr(pScrn, reg, value1);

	BIOS_USLEEP(10);

	nv32_wr(pScrn, reg, value2);
	nv32_wr(pScrn, NV_PBUS_PCI_NV_19, pci_nv_19);

	pci_nv_20 = nv32_rd(pScrn, NV_PBUS_PCI_NV_20);
	pci_nv_20 &= ~NV_PBUS_PCI_NV_20_ROM_SHADOW_ENABLED;	/* 0xfffffffe */
	nv32_wr(pScrn, NV_PBUS_PCI_NV_20, pci_nv_20);

	return true;
}

static bool init_configure_mem(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_CONFIGURE_MEM   opcode: 0x66 ('f')
	 *
	 * offset      (8 bit): opcode
	 *
	 * Equivalent to INIT_DONE on bios version 3 or greater.
	 * For early bios versions, sets up the memory registers, using values
	 * taken from the memory init table
	 */

	/* no iexec->execute check by design */

	uint16_t meminitoffs = bios->legacy.mem_init_tbl_ptr + MEM_INIT_SIZE * (nv_idx_port_rd(pScrn, NV_CIO_CRX__COLOR, NV_CIO_CRE_SCRATCH4__INDEX) >> 4);
	uint16_t seqtbloffs = bios->legacy.sdr_seq_tbl_ptr, meminitdata = meminitoffs + 6;
	uint32_t reg, data;

	if (bios->major_version > 2)
		return false;

	nv_idx_port_wr(pScrn, NV_VIO_SRX, NV_VIO_SR_CLOCK_INDEX,
		       nv_idx_port_rd(pScrn, NV_VIO_SRX, NV_VIO_SR_CLOCK_INDEX) | 0x20);

	if (bios->data[meminitoffs] & 1)
		seqtbloffs = bios->legacy.ddr_seq_tbl_ptr;

	for (reg = le32_to_cpu(*(uint32_t *)&bios->data[seqtbloffs]);
	     reg != 0xffffffff;
	     reg = le32_to_cpu(*(uint32_t *)&bios->data[seqtbloffs += 4])) {

		switch (reg) {
		case NV_PFB_PRE:
			data = NV_PFB_PRE_CMD_PRECHARGE;
			break;
		case NV_PFB_PAD:
			data = NV_PFB_PAD_CKE_NORMAL;
			break;
		case NV_PFB_REF:
			data = NV_PFB_REF_CMD_REFRESH;
			break;
		default:
			data = le32_to_cpu(*(uint32_t *)&bios->data[meminitdata]);
			meminitdata += 4;
			if (data == 0xffffffff)
				continue;
		}

		nv32_wr(pScrn, reg, data);
	}

	return true;
}

static bool init_configure_clk(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_CONFIGURE_CLK   opcode: 0x67 ('g')
	 *
	 * offset      (8 bit): opcode
	 *
	 * Equivalent to INIT_DONE on bios version 3 or greater.
	 * For early bios versions, sets up the NVClk and MClk PLLs, using
	 * values taken from the memory init table
	 */

	/* no iexec->execute check by design */

	uint16_t meminitoffs = bios->legacy.mem_init_tbl_ptr + MEM_INIT_SIZE * (nv_idx_port_rd(pScrn, NV_CIO_CRX__COLOR, NV_CIO_CRE_SCRATCH4__INDEX) >> 4);
	int clock;

	if (bios->major_version > 2)
		return false;

	clock = le16_to_cpu(*(uint16_t *)&bios->data[meminitoffs + 4]) * 10;
	setPLL(pScrn, bios, NV_RAMDAC_NVPLL, clock);

	clock = le16_to_cpu(*(uint16_t *)&bios->data[meminitoffs + 2]) * 10;
	if (bios->data[meminitoffs] & 1) /* DDR */
		clock *= 2;
	setPLL(pScrn, bios, NV_RAMDAC_MPLL, clock);

	return true;
}

static bool init_configure_preinit(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_CONFIGURE_PREINIT   opcode: 0x68 ('h')
	 *
	 * offset      (8 bit): opcode
	 *
	 * Equivalent to INIT_DONE on bios version 3 or greater.
	 * For early bios versions, does early init, loading ram and crystal
	 * configuration from straps into CR3C
	 */

	/* no iexec->execute check by design */

	uint32_t straps = nv32_rd(pScrn, NV_PEXTDEV_BOOT_0);
	uint8_t cr3c = ((straps << 2) & 0xf0) | (straps & (1 << 6));

	if (bios->major_version > 2)
		return false;

	nv_idx_port_wr(pScrn, NV_CIO_CRX__COLOR, NV_CIO_CRE_SCRATCH4__INDEX, cr3c);

	return true;
}

static bool init_io(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_IO   opcode: 0x69 ('i')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (16 bit): CRTC port
	 * offset + 3  (8  bit): mask
	 * offset + 4  (8  bit): data
	 *
	 * Assign ((IOVAL("crtc port") & "mask") | "data") to "crtc port"
	 */

	uint16_t crtcport = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 1])));
	uint8_t mask = bios->data[offset + 3];
	uint8_t data = bios->data[offset + 4];

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Port: 0x%04X, Mask: 0x%02X, Data: 0x%02X\n",
		offset, crtcport, mask, data);

	nv_port_wr(pScrn, crtcport, (nv_port_rd(pScrn, crtcport) & mask) | data);

	return true;
}

static bool init_sub(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_SUB   opcode: 0x6B ('k')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): script number
	 *
	 * Execute script number "script number", as a subroutine
	 */

	uint8_t sub = bios->data[offset + 1];

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Calling script %d\n", offset, sub);

	parse_init_table(pScrn, bios,
			 le16_to_cpu(*((uint16_t *)(&bios->data[bios->init_script_tbls_ptr + sub * 2]))),
			 iexec);

	BIOSLOG(pScrn, "0x%04X: End of script %d\n", offset, sub);

	return true;
}

static bool init_ram_condition(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_RAM_CONDITION   opcode: 0x6D ('m')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): mask
	 * offset + 2  (8 bit): cmpval
	 *
	 * Test if (NV_PFB_BOOT_0 & "mask") equals "cmpval".
	 * If condition not met skip subsequent opcodes until condition is
	 * inverted (INIT_NOT), or we hit INIT_RESUME
	 */

	uint8_t mask = bios->data[offset + 1];
	uint8_t cmpval = bios->data[offset + 2];
	uint8_t data;

	if (!iexec->execute)
		return true;

	data = nv32_rd(pScrn, NV_PFB_BOOT_0) & mask;

	BIOSLOG(pScrn, "0x%04X: Checking if 0x%08X equals 0x%08X\n", offset, data, cmpval);

	if (data == cmpval)
		BIOSLOG(pScrn, "0x%04X: Condition fulfilled -- continuing to execute\n", offset);
	else {
		BIOSLOG(pScrn, "0x%04X: Condition not fulfilled -- skipping following commands\n", offset);
		iexec->execute = false;
	}

	return true;
}

static bool init_nv_reg(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_NV_REG   opcode: 0x6E ('n')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): register
	 * offset + 5  (32 bit): mask
	 * offset + 9  (32 bit): data
	 *
	 * Assign ((REGVAL("register") & "mask") | "data") to "register"
	 */

	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint32_t mask = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 5])));
	uint32_t data = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 9])));

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Reg: 0x%08X, Mask: 0x%08X, Data: 0x%08X\n", offset, reg, mask, data);

	nv32_wr(pScrn, reg, (nv32_rd(pScrn, reg) & mask) | data);

	return true;
}

static bool init_macro(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_MACRO   opcode: 0x6F ('o')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): macro number
	 *
	 * Look up macro index "macro number" in the macro index table.
	 * The macro index table entry has 1 byte for the index in the macro table,
	 * and 1 byte for the number of times to repeat the macro.
	 * The macro table entry has 4 bytes for the register address and
	 * 4 bytes for the value to write to that register
	 */

	uint8_t macro_index_tbl_idx = bios->data[offset + 1];
	uint16_t tmp = bios->macro_index_tbl_ptr + (macro_index_tbl_idx * MACRO_INDEX_SIZE);
	uint8_t macro_tbl_idx = bios->data[tmp];
	uint8_t count = bios->data[tmp + 1];
	uint32_t reg, data;
	int i;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Macro: 0x%02X, MacroTableIndex: 0x%02X, Count: 0x%02X\n",
		offset, macro_index_tbl_idx, macro_tbl_idx, count);

	for (i = 0; i < count; i++) {
		uint16_t macroentryptr = bios->macro_tbl_ptr + (macro_tbl_idx + i) * MACRO_SIZE;

		reg = le32_to_cpu(*((uint32_t *)(&bios->data[macroentryptr])));
		data = le32_to_cpu(*((uint32_t *)(&bios->data[macroentryptr + 4])));

		nv32_wr(pScrn, reg, data);
	}

	return true;
}

static bool init_done(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_DONE   opcode: 0x71 ('q')
	 *
	 * offset      (8  bit): opcode
	 *
	 * End the current script
	 */

	/* mild retval abuse to stop parsing this table */
	return false;
}

static bool init_resume(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_RESUME   opcode: 0x72 ('r')
	 *
	 * offset      (8  bit): opcode
	 *
	 * End the current execute / no-execute condition
	 */

	if (iexec->execute)
		return true;

	iexec->execute = true;
	BIOSLOG(pScrn, "0x%04X: ---- Executing following commands ----\n", offset);

	return true;
}

#if 0
static bool init_ram_condition2(ScrnInfoPtr pScrn, bios_t *bios, CARD16 offset, init_exec_t *iexec)
{
	/* INIT_RAM_CONDITION2   opcode: 0x73
	 * 
	 * offset      (8  bit): opcode
	 * offset + 1  (8  bit): and mask
	 * offset + 2  (8  bit): cmpval
	 *
	 * Test if (NV_EXTDEV_BOOT & and mask) matches cmpval
	 */
	NVPtr pNv = NVPTR(pScrn);
	CARD32 and = *((CARD32 *) (&bios->data[offset + 1]));
	CARD32 cmpval = *((CARD32 *) (&bios->data[offset + 5]));
	CARD32 data;

	if (iexec->execute) {
		data=(nvReadEXTDEV(pNv, NV_PEXTDEV_BOOT))&and;
		
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,  
				"0x%04X: CHECKING IF REGVAL: 0x%08X equals COND: 0x%08X\n",
				offset, data, cmpval);

		if (data == cmpval) {
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,  
					"0x%04X: CONDITION FULFILLED - CONTINUING TO EXECUTE\n",
					offset);
		} else {
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,  "0x%04X: CONDITION IS NOT FULFILLED\n", offset);
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,  
					"0x%04X: ------ SKIPPING FOLLOWING COMMANDS  ------\n", offset);
			iexec->execute = false;
		}
	}
	return true;
}
#endif

static bool init_time(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_TIME   opcode: 0x74 ('t')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (16 bit): time
	 *
	 * Sleep for "time" microseconds.
	 */

	uint16_t time = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 1])));

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Sleeping for 0x%04X microseconds\n", offset, time);

	BIOS_USLEEP(time);

	return true;
}

static bool init_condition(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_CONDITION   opcode: 0x75 ('u')
	 *
	 * offset      (8 bit): opcode
	 * offset + 1  (8 bit): condition number
	 *
	 * Check condition "condition number" in the condition table.
	 * If condition not met skip subsequent opcodes until condition is
	 * inverted (INIT_NOT), or we hit INIT_RESUME
	 */

	uint8_t cond = bios->data[offset + 1];

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Condition: 0x%02X\n", offset, cond);

	if (bios_condition_met(pScrn, bios, offset, cond))
		BIOSLOG(pScrn, "0x%04X: Condition fulfilled -- continuing to execute\n", offset);
	else {
		BIOSLOG(pScrn, "0x%04X: Condition not fulfilled -- skipping following commands\n", offset);
		iexec->execute = false;
	}

	return true;
}

static bool init_index_io(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_INDEX_IO   opcode: 0x78 ('x')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (16 bit): CRTC port
	 * offset + 3  (8  bit): CRTC index
	 * offset + 4  (8  bit): mask
	 * offset + 5  (8  bit): data
	 *
	 * Read value at index "CRTC index" on "CRTC port", AND with "mask", OR with "data", write-back
	 */

	uint16_t crtcport = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 1])));
	uint8_t crtcindex = bios->data[offset + 3];
	uint8_t mask = bios->data[offset + 4];
	uint8_t data = bios->data[offset + 5];
	uint8_t value;

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Port: 0x%04X, Index: 0x%02X, Mask: 0x%02X, Data: 0x%02X\n",
		offset, crtcport, crtcindex, mask, data);

	value = (nv_idx_port_rd(pScrn, crtcport, crtcindex) & mask) | data;
	nv_idx_port_wr(pScrn, crtcport, crtcindex, value);

	return true;
}

static bool init_pll(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_PLL   opcode: 0x79 ('y')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): register
	 * offset + 5  (16 bit): freq
	 *
	 * Set PLL register "register" to coefficients for frequency (10kHz) "freq"
	 */

	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint16_t freq = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 5])));

	if (!iexec->execute)
		return true;

	BIOSLOG(pScrn, "0x%04X: Reg: 0x%08X, Freq: %d0kHz\n", offset, reg, freq);

	setPLL(pScrn, bios, reg, freq * 10);

	return true;
}

static bool init_zm_reg(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_ZM_REG   opcode: 0x7A ('z')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): register
	 * offset + 5  (32 bit): value
	 *
	 * Assign "value" to "register"
	 */

	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint32_t value = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 5])));

	if (!iexec->execute)
		return true;

	nv32_wr(pScrn, reg, value);

	return true;
}

static bool init_8e(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_8E   opcode: 0x8E ('')
	 *
	 * offset      (8 bit): opcode
	 *
	 * The purpose of this opcode is unclear (being for nv50 cards), and
	 * the literal functionality can be seen in the code below.
	 *
	 * A brief synopsis is that for each entry in a table pointed to by the
	 * DCB table header, depending on the settings of various bits, various
	 * other bits in registers 0xe100, 0xe104, and 0xe108, are set or
	 * cleared.
	 */

	uint16_t dcbptr = le16_to_cpu(*(uint16_t *)&bios->data[0x36]);
	uint16_t init8etblptr = le16_to_cpu(*(uint16_t *)&bios->data[dcbptr + 10]);
	uint8_t headerlen = bios->data[init8etblptr + 1];
	uint8_t entries = bios->data[init8etblptr + 2];
	uint8_t recordlen = bios->data[init8etblptr + 3];
	int i;

	if (!dcbptr) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No Display Configuration Block pointer found\n");
		return false;
	}
	if (bios->data[dcbptr] != 0x40) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "DCB table not version 4.0\n");
		return false;
	}
	if (!init8etblptr) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Invalid pointer to INIT_8E table\n");
		return false;
	}

	for (i = 0; i < entries; i++) {
		uint32_t entry = le32_to_cpu(*(uint32_t *)&bios->data[init8etblptr + headerlen + recordlen * i]);
		int shift = (entry & 0x1f) * 4;
		uint32_t mask;
		uint32_t reg = 0xe104;
		uint32_t data;

		if ((entry & 0xff00) == 0xff00)
			continue;

		if (shift >= 32) {
			reg += 4;
			shift -= 32;
		}
		shift %= 32;

		mask = ~(3 << shift);
		if (entry & (1 << 24))
			data = (entry >> 21);
		else
			data = (entry >> 19);
		data = ((data & 3) ^ 2) << shift;

		BIOSLOG(pScrn, "0x%04X: Entry: 0x%08X, Reg: 0x%08X, Shift: 0x%02X, Mask: 0x%08X, Data: 0x%08X\n",
			offset, entry, reg, shift, mask, data);

		nv32_wr(pScrn, reg, (nv32_rd(pScrn, reg) & mask) | data);

		reg = 0xe100;
		shift = entry & 0x1f;

		mask = ~(1 << 16 | 1);
		mask = mask << shift | mask >> (32 - shift);
		data = 0;
		if ((entry & (3 << 25)) == (1 << 25))
			data |= 1;
		if ((entry & (3 << 25)) == (2 << 25))
			data |= 0x10000;
		data <<= shift;

		BIOSLOG(pScrn, "0x%04X: Entry: 0x%08X, Reg: 0x%08X, Shift: 0x%02X, Mask: 0x%08X, Data: 0x%08X\n",
			offset, entry, reg, shift, mask, data);

		nv32_wr(pScrn, reg, (nv32_rd(pScrn, reg) & mask) | data);
	}

	return true;
}

/* hack to avoid moving the itbl_entry array before this function */
int init_ram_restrict_zm_reg_group_blocklen = 0;

static bool init_ram_restrict_zm_reg_group(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_RAM_RESTRICT_ZM_REG_GROUP   opcode: 0x8F ('')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): reg
	 * offset + 5  (8  bit): regincrement
	 * offset + 6  (8  bit): count
	 * offset + 7  (32 bit): value 1,1
	 * ...
	 *
	 * Use the RAMCFG strap of PEXTDEV_BOOT as an index into the table at
	 * ram_restrict_table_ptr. The value read from here is 'n', and
	 * "value 1,n" gets written to "reg". This repeats "count" times and on
	 * each iteration 'm', "reg" increases by "regincrement" and
	 * "value m,n" is used. The extent of n is limited by a number read
	 * from the 'M' BIT table, herein called "blocklen"
	 */

	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint8_t regincrement = bios->data[offset + 5];
	uint8_t count = bios->data[offset + 6];
	uint32_t strap_ramcfg, data;
	uint16_t blocklen;
	uint8_t index;
	int i;

	/* previously set by 'M' BIT table */
	blocklen = init_ram_restrict_zm_reg_group_blocklen;

	if (!iexec->execute)
		return true;

	if (!blocklen) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "0x%04X: Zero block length - has the M table been parsed?\n", offset);
		return false;
	}

	strap_ramcfg = (nv32_rd(pScrn, NV_PEXTDEV_BOOT_0) >> 2) & 0xf;
	index = bios->data[bios->ram_restrict_tbl_ptr + strap_ramcfg];

	BIOSLOG(pScrn, "0x%04X: Reg: 0x%08X, RegIncrement: 0x%02X, Count: 0x%02X, StrapRamCfg: 0x%02X, Index: 0x%02X\n",
		offset, reg, regincrement, count, strap_ramcfg, index);

	for (i = 0; i < count; i++) {
		data = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 7 + index * 4 + blocklen * i])));

		nv32_wr(pScrn, reg, data);

		reg += regincrement;
	}

	return true;
}

static bool init_copy_zm_reg(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_COPY_ZM_REG   opcode: 0x90 ('')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): src reg
	 * offset + 5  (32 bit): dst reg
	 *
	 * Put contents of "src reg" into "dst reg"
	 */

	uint32_t srcreg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint32_t dstreg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 5])));

	if (!iexec->execute)
		return true;

	nv32_wr(pScrn, dstreg, nv32_rd(pScrn, srcreg));

	return true;
}

static bool init_zm_reg_group_addr_latched(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_ZM_REG_GROUP_ADDRESS_LATCHED   opcode: 0x91 ('')
	 *
	 * offset      (8  bit): opcode
	 * offset + 1  (32 bit): dst reg
	 * offset + 5  (8  bit): count
	 * offset + 6  (32 bit): data 1
	 * ...
	 *
	 * For each of "count" values write "data n" to "dst reg"
	 */

	uint32_t reg = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 1])));
	uint8_t count = bios->data[offset + 5];
	int i;

	if (!iexec->execute)
		return true;

	for (i = 0; i < count; i++) {
		uint32_t data = le32_to_cpu(*((uint32_t *)(&bios->data[offset + 6 + 4 * i])));
		nv32_wr(pScrn, reg, data);
	}

	return true;
}

static bool init_reserved(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset, init_exec_t *iexec)
{
	/* INIT_RESERVED   opcode: 0x92 ('')
	 *
	 * offset      (8 bit): opcode
	 *
	 * Seemingly does nothing
	 */

	return true;
}

static init_tbl_entry_t itbl_entry[] = {
	/* command name                       , id  , length  , offset  , mult    , command handler                 */
//	{ "INIT_PROG"                         , 0x31, 15      , 10      , 4       , init_prog                       },
	{ "INIT_IO_RESTRICT_PROG"             , 0x32, 11      , 6       , 4       , init_io_restrict_prog           },
	{ "INIT_REPEAT"                       , 0x33, 2       , 0       , 0       , init_repeat                     },
	{ "INIT_IO_RESTRICT_PLL"              , 0x34, 12      , 7       , 2       , init_io_restrict_pll            },
	{ "INIT_END_REPEAT"                   , 0x36, 1       , 0       , 0       , init_end_repeat                 },
	{ "INIT_COPY"                         , 0x37, 11      , 0       , 0       , init_copy                       },
	{ "INIT_NOT"                          , 0x38, 1       , 0       , 0       , init_not                        },
	{ "INIT_IO_FLAG_CONDITION"            , 0x39, 2       , 0       , 0       , init_io_flag_condition          },
	{ "INIT_INDEX_ADDRESS_LATCHED"        , 0x49, 18      , 17      , 2       , init_idx_addr_latched           },
	{ "INIT_IO_RESTRICT_PLL2"             , 0x4A, 11      , 6       , 4       , init_io_restrict_pll2           },
	{ "INIT_PLL2"                         , 0x4B, 9       , 0       , 0       , init_pll2                       },
	{ "INIT_I2C_BYTE"                     , 0x4C, 4       , 3       , 3       , init_i2c_byte                   },
	{ "INIT_ZM_I2C_BYTE"                  , 0x4D, 4       , 3       , 2       , init_zm_i2c_byte                },
	{ "INIT_ZM_I2C"                       , 0x4E, 4       , 3       , 1       , init_zm_i2c                     },
	{ "INIT_TMDS"                         , 0x4F, 5       , 0       , 0       , init_tmds                       },
	{ "INIT_ZM_TMDS_GROUP"                , 0x50, 3       , 2       , 2       , init_zm_tmds_group              },
	{ "INIT_CR_INDEX_ADDRESS_LATCHED"     , 0x51, 5       , 4       , 1       , init_cr_idx_adr_latch           },
	{ "INIT_CR"                           , 0x52, 4       , 0       , 0       , init_cr                         },
	{ "INIT_ZM_CR"                        , 0x53, 3       , 0       , 0       , init_zm_cr                      },
	{ "INIT_ZM_CR_GROUP"                  , 0x54, 2       , 1       , 2       , init_zm_cr_group                },
	{ "INIT_CONDITION_TIME"               , 0x56, 3       , 0       , 0       , init_condition_time             },
	{ "INIT_ZM_REG_SEQUENCE"              , 0x58, 6       , 5       , 4       , init_zm_reg_sequence            },
//	{ "INIT_INDIRECT_REG"                 , 0x5A, 7       , 0       , 0       , init_indirect_reg               },
	{ "INIT_SUB_DIRECT"                   , 0x5B, 3       , 0       , 0       , init_sub_direct                 },
	{ "INIT_COPY_NV_REG"                  , 0x5F, 22      , 0       , 0       , init_copy_nv_reg                },
	{ "INIT_ZM_INDEX_IO"                  , 0x62, 5       , 0       , 0       , init_zm_index_io                },
	{ "INIT_COMPUTE_MEM"                  , 0x63, 1       , 0       , 0       , init_compute_mem                },
	{ "INIT_RESET"                        , 0x65, 13      , 0       , 0       , init_reset                      },
	{ "INIT_CONFIGURE_MEM"                , 0x66, 1       , 0       , 0       , init_configure_mem              },
	{ "INIT_CONFIGURE_CLK"                , 0x67, 1       , 0       , 0       , init_configure_clk              },
	{ "INIT_CONFIGURE_PREINIT"            , 0x68, 1       , 0       , 0       , init_configure_preinit          },
	{ "INIT_IO"                           , 0x69, 5       , 0       , 0       , init_io                         },
	{ "INIT_SUB"                          , 0x6B, 2       , 0       , 0       , init_sub                        },
	{ "INIT_RAM_CONDITION"                , 0x6D, 3       , 0       , 0       , init_ram_condition              },
	{ "INIT_NV_REG"                       , 0x6E, 13      , 0       , 0       , init_nv_reg                     },
	{ "INIT_MACRO"                        , 0x6F, 2       , 0       , 0       , init_macro                      },
	{ "INIT_DONE"                         , 0x71, 1       , 0       , 0       , init_done                       },
	{ "INIT_RESUME"                       , 0x72, 1       , 0       , 0       , init_resume                     },
//	{ "INIT_RAM_CONDITION2"               , 0x73, 9       , 0       , 0       , init_ram_condition2             },
	{ "INIT_TIME"                         , 0x74, 3       , 0       , 0       , init_time                       },
	{ "INIT_CONDITION"                    , 0x75, 2       , 0       , 0       , init_condition                  },
/*	{ "INIT_IO_CONDITION"                 , 0x76, x       , x       , x       , init_io_condition               }, */
	{ "INIT_INDEX_IO"                     , 0x78, 6       , 0       , 0       , init_index_io                   },
	{ "INIT_PLL"                          , 0x79, 7       , 0       , 0       , init_pll                        },
	{ "INIT_ZM_REG"                       , 0x7A, 9       , 0       , 0       , init_zm_reg                     },
	{ "INIT_8E"                           , 0x8E, 1       , 0       , 0       , init_8e                         },
	/* INIT_RAM_RESTRICT_ZM_REG_GROUP's mult is loaded by M table in BIT */
	{ "INIT_RAM_RESTRICT_ZM_REG_GROUP"    , 0x8F, 7       , 6       , 0       , init_ram_restrict_zm_reg_group  },
	{ "INIT_COPY_ZM_REG"                  , 0x90, 9       , 0       , 0       , init_copy_zm_reg                },
	{ "INIT_ZM_REG_GROUP_ADDRESS_LATCHED" , 0x91, 6       , 5       , 4       , init_zm_reg_group_addr_latched  },
	{ "INIT_RESERVED"                     , 0x92, 1       , 0       , 0       , init_reserved                   },
	{ 0                                   , 0   , 0       , 0       , 0       , 0                               }
};

static unsigned int get_init_table_entry_length(bios_t *bios, unsigned int offset, int i)
{
	/* Calculates the length of a given init table entry. */
	return itbl_entry[i].length + bios->data[offset + itbl_entry[i].length_offset]*itbl_entry[i].length_multiplier;
}

#define MAX_TABLE_OPS 1000

static int parse_init_table(ScrnInfoPtr pScrn, bios_t *bios, unsigned int offset, init_exec_t *iexec)
{
	/* Parses all commands in an init table.
	 *
	 * We start out executing all commands found in the init table. Some
	 * opcodes may change the status of iexec->execute to SKIP, which will
	 * cause the following opcodes to perform no operation until the value
	 * is changed back to EXECUTE.
	 */

	int count = 0, i;
	uint8_t id;

	/* Loop until INIT_DONE causes us to break out of the loop
	 * (or until offset > bios length just in case... )
	 * (and no more than MAX_TABLE_OPS iterations, just in case... ) */
	while ((offset < bios->length) && (count++ < MAX_TABLE_OPS)) {
		id = bios->data[offset];

		/* Find matching id in itbl_entry */
		for (i = 0; itbl_entry[i].name && (itbl_entry[i].id != id); i++)
			;

		if (itbl_entry[i].name) {
			BIOSLOG(pScrn, "0x%04X: [ (0x%02X) - %s ]\n",
				offset, itbl_entry[i].id, itbl_entry[i].name);

			/* execute eventual command handler */
			if (itbl_entry[i].handler)
				if (!(*itbl_entry[i].handler)(pScrn, bios, offset, iexec))
					break;
		} else {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "0x%04X: Init table command not found: 0x%02X\n", offset, id);
			return -ENOENT;
		}

		/* Add the offset of the current command including all data
		 * of that command. The offset will then be pointing on the
		 * next op code.
		 */
		offset += get_init_table_entry_length(bios, offset, i);
	}

	if (offset >= bios->length)
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Offset 0x%04X greater than known bios image length."
			   "  Corrupt image?\n", offset);
	if (count >= MAX_TABLE_OPS)
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "More than %d opcodes to a table is unlikely, "
			   "is the bios image corrupt?\n", MAX_TABLE_OPS);

	return 0;
}

static void parse_init_tables(ScrnInfoPtr pScrn, bios_t *bios)
{
	/* Loops and calls parse_init_table() for each present table. */

	int i = 0;
	uint16_t table;
	init_exec_t iexec = {true, false};

	if (bios->old_style_init) {
		if (bios->init_script_tbls_ptr)
			parse_init_table(pScrn, bios, bios->init_script_tbls_ptr, &iexec);
		if (bios->extra_init_script_tbl_ptr)
			parse_init_table(pScrn, bios, bios->extra_init_script_tbl_ptr, &iexec);

		return;
	}

	while ((table = le16_to_cpu(*((uint16_t *)(&bios->data[bios->init_script_tbls_ptr + i]))))) {
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			   "Parsing VBIOS init table %d at offset 0x%04X\n", i / 2, table);
		BIOSLOG(pScrn, "0x%04X: ------ Executing following commands ------\n", table);

		parse_init_table(pScrn, bios, table, &iexec);
		i += 2;
	}
}

static void link_head_and_output(ScrnInfoPtr pScrn, struct dcb_entry *dcbent, int head)
{
	/* The BIOS scripts don't do this for us, sadly
	 * Luckily we do know the values ;-)
	 *
	 * head < 0 indicates we wish to force a setting with the overrideval
	 * (for VT restore etc.)
	 */

	NVPtr pNv = NVPTR(pScrn);
	int ramdac = (dcbent->or & OUTPUT_C) >> 2;
	uint8_t tmds04 = 0x80;

	if (head != ramdac)
		tmds04 = 0x88;

	if (dcbent->type == OUTPUT_LVDS)
		tmds04 |= 0x01;

	nv_write_tmds(pNv, dcbent->or, 0, 0x04, tmds04);

	if (dcbent->type == OUTPUT_LVDS && pNv->VBIOS.fp.dual_link)
		nv_write_tmds(pNv, dcbent->or, 1, 0x04, tmds04 ^ 0x08);
}

static uint16_t clkcmptable(bios_t *bios, uint16_t clktable, int pxclk)
{
	int compare_record_len, i = 0;
	uint16_t compareclk, scriptptr = 0;

	if (bios->major_version < 5) /* pre BIT */
		compare_record_len = 3;
	else
		compare_record_len = 4;

	do {
		compareclk = le16_to_cpu(*((uint16_t *)&bios->data[clktable + compare_record_len * i]));
		if (pxclk >= compareclk * 10) {
			if (bios->major_version < 5) {
				uint8_t tmdssub = bios->data[clktable + 2 + compare_record_len * i];
				scriptptr = le16_to_cpu(*((uint16_t *)(&bios->data[bios->init_script_tbls_ptr + tmdssub * 2])));
			} else
				scriptptr = le16_to_cpu(*((uint16_t *)&bios->data[clktable + 2 + compare_record_len * i]));
			break;
		}
		i++;
	} while (compareclk);

	return scriptptr;
}

static void run_digital_op_script(ScrnInfoPtr pScrn, uint16_t scriptptr, struct dcb_entry *dcbent, int head)
{
	bios_t *bios = &NVPTR(pScrn)->VBIOS;
	init_exec_t iexec = {true, false};

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "0x%04X: Parsing digital output script table\n", scriptptr);
	nv_idx_port_wr(pScrn, NV_CIO_CRX__COLOR, NV_CIO_CRE_44,
		       head ? NV_CIO_CRE_44_HEADB : NV_CIO_CRE_44_HEADA);
	NVWriteVgaCrtc5758(NVPTR(pScrn), head, 0, dcbent->index);
	parse_init_table(pScrn, bios, scriptptr, &iexec);

	link_head_and_output(pScrn, dcbent, head);
}

static int call_lvds_manufacturer_script(ScrnInfoPtr pScrn, struct dcb_entry *dcbent, int head, enum LVDS_script script)
{
	NVPtr pNv = NVPTR(pScrn);
	bios_t *bios = &pNv->VBIOS;
	uint8_t sub = bios->data[bios->fp.xlated_entry + script] + (bios->fp.link_c_increment && dcbent->or & OUTPUT_C ? 1 : 0);
	uint16_t scriptofs = le16_to_cpu(*((uint16_t *)(&bios->data[bios->init_script_tbls_ptr + sub * 2])));

	if (!bios->fp.xlated_entry || !sub || !scriptofs)
		return -EINVAL;

	run_digital_op_script(pScrn, scriptofs, dcbent, head);

	if (script == LVDS_PANEL_OFF)
		/* off-on delay in ms */
		BIOS_USLEEP(le16_to_cpu(*(uint16_t *)&bios->data[bios->fp.xlated_entry + 7]));
#ifdef __powerpc__
	/* Powerbook specific quirks */
	if (script == LVDS_RESET && ((pNv->Chipset & 0xffff) == 0x0179 || (pNv->Chipset & 0xffff) == 0x0329))
		nv_write_tmds(pNv, dcbent->or, 0, 0x02, 0x72);
	if ((pNv->Chipset & 0xffff) == 0x0179 || (pNv->Chipset & 0xffff) == 0x0189 || (pNv->Chipset & 0xffff) == 0x0329) {
		if (script == LVDS_PANEL_ON) {
			nv32_wr(pScrn, NV_PBUS_DEBUG_DUALHEAD_CTL, nv32_rd(pScrn, NV_PBUS_DEBUG_DUALHEAD_CTL) | (1 << 31));
			nv32_wr(pScrn, NV_CRTC_GPIO_EXT, nv32_rd(pScrn, NV_CRTC_GPIO_EXT) | 1);
		}
		if (script == LVDS_PANEL_OFF) {
			nv32_wr(pScrn, NV_PBUS_DEBUG_DUALHEAD_CTL, nv32_rd(pScrn, NV_PBUS_DEBUG_DUALHEAD_CTL) & ~(1 << 31));
			nv32_wr(pScrn, NV_CRTC_GPIO_EXT, nv32_rd(pScrn, NV_CRTC_GPIO_EXT) & ~3);
		}
	}
#endif

	return 0;
}

static int run_lvds_table(ScrnInfoPtr pScrn, struct dcb_entry *dcbent, int head, enum LVDS_script script, int pxclk)
{
	/* The BIT LVDS table's header has the information to setup the
	 * necessary registers. Following the standard 4 byte header are:
	 * A bitmask byte and a dual-link transition pxclk value for use in
	 * selecting the init script when not using straps; 4 script pointers
	 * for panel power, selected by output and on/off; and 8 table pointers
	 * for panel init, the needed one determined by output, and bits in the
	 * conf byte. These tables are similar to the TMDS tables, consisting
	 * of a list of pxclks and script pointers.
	 */

	NVPtr pNv = NVPTR(pScrn);
	bios_t *bios = &pNv->VBIOS;
	unsigned int outputset = (dcbent->or == 4) ? 1 : 0;
	uint16_t scriptptr = 0, clktable;
	uint8_t clktableptr = 0;

	/* for now we assume version 3.0 table - g80 support will need some changes */

	switch (script) {
	case LVDS_INIT:
		return 0;
	case LVDS_BACKLIGHT_ON:
	case LVDS_PANEL_ON:
		scriptptr = le16_to_cpu(*(uint16_t *)&bios->data[bios->fp.lvdsmanufacturerpointer + 7 + outputset * 2]);
		break;
	case LVDS_BACKLIGHT_OFF:
	case LVDS_PANEL_OFF:
		scriptptr = le16_to_cpu(*(uint16_t *)&bios->data[bios->fp.lvdsmanufacturerpointer + 11 + outputset * 2]);
		break;
	case LVDS_RESET:
		if (dcbent->lvdsconf.use_straps_for_mode) {
			if (bios->fp.dual_link)
				clktableptr += 2;
			if (bios->fp.BITbit1)
				clktableptr++;
		} else {
			uint8_t fallback = bios->data[bios->fp.lvdsmanufacturerpointer + 4];
			int fallbackcmpval = (dcbent->or == 4) ? 4 : 1;

			if (bios->fp.dual_link) {
				clktableptr += 2;
				fallbackcmpval *= 2;
			}
			if (fallbackcmpval & fallback)
				clktableptr++;
		}

		/* adding outputset * 8 may not be correct */
		clktable = le16_to_cpu(*(uint16_t *)&bios->data[bios->fp.lvdsmanufacturerpointer + 15 + clktableptr * 2 + outputset * 8]);
		if (!clktable) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Pixel clock comparison table not found\n");
			return -ENOENT;
		}
		scriptptr = clkcmptable(bios, clktable, pxclk);
	}

	if (!scriptptr) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "LVDS output init script not found\n");
		return -ENOENT;
	}
	run_digital_op_script(pScrn, scriptptr, dcbent, head);

	return 0;
}

int call_lvds_script(ScrnInfoPtr pScrn, struct dcb_entry *dcbent, int head, enum LVDS_script script, int pxclk)
{
	/* LVDS operations are multiplexed in an effort to present a single API
	 * which works with two vastly differing underlying structures.
	 * This acts as the demux
	 */

	bios_t *bios = &NVPTR(pScrn)->VBIOS;
	uint8_t lvds_ver = bios->data[bios->fp.lvdsmanufacturerpointer];
	uint32_t sel_clk_binding;
	static int last_invoc = 0;
	int ret;

	if (last_invoc == (script << 1 | head) || !lvds_ver)
		return 0;

	if (script == LVDS_PANEL_ON && bios->fp.reset_after_pclk_change)
		call_lvds_script(pScrn, dcbent, head, LVDS_RESET, pxclk);
	if (script == LVDS_RESET && bios->fp.power_off_for_reset)
		call_lvds_script(pScrn, dcbent, head, LVDS_PANEL_OFF, pxclk);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Calling LVDS script %d:\n", script);

	/* don't let script change pll->head binding */
	sel_clk_binding = nv32_rd(pScrn, NV_RAMDAC_SEL_CLK) & 0x50000;

	if (lvds_ver < 0x30)
		ret = call_lvds_manufacturer_script(pScrn, dcbent, head, script);
	else
		ret = run_lvds_table(pScrn, dcbent, head, script, pxclk);

	last_invoc = (script << 1 | head);

	nv32_wr(pScrn, NV_RAMDAC_SEL_CLK, (nv32_rd(pScrn, NV_RAMDAC_SEL_CLK) & ~0x50000) | sel_clk_binding);
	/* some scripts set a value in NV_PBUS_POWERCTRL_2 and break video overlay */
	nv32_wr(pScrn, NV_PBUS_POWERCTRL_2, 0);

	return ret;
}

struct lvdstableheader {
	uint8_t lvds_ver, headerlen, recordlen;
};

static int parse_lvds_manufacturer_table_header(ScrnInfoPtr pScrn, bios_t *bios, struct lvdstableheader *lth)
{
	/* BMP version (0xa) LVDS table has a simple header of version and
	 * record length. The BIT LVDS table has the typical BIT table header:
	 * version byte, header length byte, record length byte, and a byte for
	 * the maximum number of records that can be held in the table */

	uint8_t lvds_ver, headerlen, recordlen;

	memset(lth, 0, sizeof(struct lvdstableheader));

	if (bios->fp.lvdsmanufacturerpointer == 0x0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Pointer to LVDS manufacturer table invalid\n");
		return -EINVAL;
	}

	lvds_ver = bios->data[bios->fp.lvdsmanufacturerpointer];

	switch (lvds_ver) {
	case 0x0a:	/* pre NV40 */
		headerlen = 2;
		recordlen = bios->data[bios->fp.lvdsmanufacturerpointer + 1];
		break;
	case 0x30:	/* NV4x */
		headerlen = bios->data[bios->fp.lvdsmanufacturerpointer + 1];
		if (headerlen < 0x1f) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "LVDS table header not understood\n");
			return -EINVAL;
		}
		recordlen = bios->data[bios->fp.lvdsmanufacturerpointer + 2];
		break;
	case 0x40:	/* G80/G90 */
		headerlen = bios->data[bios->fp.lvdsmanufacturerpointer + 1];
		if (headerlen < 0x7) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "LVDS table header not understood\n");
			return -EINVAL;
		}
		recordlen = bios->data[bios->fp.lvdsmanufacturerpointer + 2];
		break;
	default:
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "LVDS table revision %d.%d not currently supported\n",
			   lvds_ver >> 4, lvds_ver & 0xf);
		return -ENOSYS;
	}

	lth->lvds_ver = lvds_ver;
	lth->headerlen = headerlen;
	lth->recordlen = recordlen;

	return 0;
}

static int parse_fp_mode_table(ScrnInfoPtr pScrn, bios_t *bios)
{
	uint8_t *fptable;
	uint8_t fptable_ver, headerlen = 0, recordlen, fpentries = 0xf, fpindex;
	int ret, ofs;
	struct lvdstableheader lth;
	uint16_t modeofs;
	DisplayModePtr mode;

	if (bios->fp.fptablepointer == 0x0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Pointer to flat panel table invalid\n");
		return -EINVAL;
	}

	fptable = &bios->data[bios->fp.fptablepointer];
	fptable_ver = fptable[0];

	switch (fptable_ver) {
	/* BMP version 0x5.0x11 BIOSen have version 1 like tables, but no version field,
	 * and miss one of the spread spectrum/PWM bytes.
	 * This could affect early GF2Go parts (not seen any appropriate ROMs though).
	 * Here we assume that a version of 0x05 matches this case (combining with a
	 * BMP version check would be better), as the common case for the panel type
	 * field is 0x0005, and that is in fact what we are reading the first byte of. */
	case 0x05:	/* some NV10, 11, 15, 16 */
		recordlen = 42;
		ofs = 6;
		break;
	case 0x10:	/* some NV15/16, and NV11+ */
		recordlen = 44;
		ofs = 7;
		break;
	case 0x20:	/* NV40+ */
		headerlen = fptable[1];
		recordlen = fptable[2];
		fpentries = fptable[3];
		/* fptable[4] is the minimum RAMDAC_FP_HCRTC->RAMDAC_FP_HSYNC_START gap */
		bios->digital_min_front_porch = fptable[4];
		ofs = 0;
		break;
	default:
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "FP table revision %d.%d not currently supported\n",
			   fptable_ver >> 4, fptable_ver & 0xf);
		return -ENOSYS;
	}

	/* non mobile only needs to set digital_min_front_porch */
	if (!(bios->feature_byte & FEATURE_MOBILE))
		return 0;

	if ((ret = parse_lvds_manufacturer_table_header(pScrn, bios, &lth)))
		return ret;

	switch (lth.lvds_ver) {
	case 0x0a:
		/* make sure to match the 0xff strapping check below */
		if ((bios->fp.strapping & 0xf) == 0xf)
			bios->data[bios->fp.fpxlatetableptr + 0xf] = 0xf;
		break;
	case 0x30:
	case 0x40:
		bios->fp.fpxlatetableptr = bios->fp.lvdsmanufacturerpointer + lth.headerlen + 1;
		bios->fp.xlatwidth = lth.recordlen;
	}
	if (bios->fp.fpxlatetableptr == 0x0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Pointer to flat panel xlat table invalid\n");
		return -EINVAL;
	}

	/* Query all modes and find one with a matching clock. */
	/* Note that this only serves as a backup solution if ddc fails. */
	if (lth.lvds_ver == 0x40) {
		uint32_t clock, needed_clock;
		int i, index = 0xF, matches = 0;
		needed_clock = nv32_rd(pScrn, 0x00616404) & 0xFFFFF;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "LVDS clock seems to be %d KHz.\n", needed_clock);

		for (i = 0; i < fpentries; i++) {
			modeofs = headerlen + recordlen * i;
			clock = le16_to_cpu(*(uint16_t *)&fptable[modeofs]) * 10;
			if (clock == needed_clock) {
				matches++;
				index = i;
			}
		}

		if (matches == 1)
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Found a mode with matching clock\n");
		else
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Found %d modes, this is not useful\n", matches);

		if (matches != 1)
			index = 0xF;

		fpindex = bios->data[bios->fp.fpxlatetableptr + index * bios->fp.xlatwidth];
		bios->fp.strapping = ((fpindex & 0xF) << 4) | (fpindex & 0xF);
	} else {
		fpindex = bios->data[bios->fp.fpxlatetableptr + bios->fp.strapping * bios->fp.xlatwidth];
		bios->fp.strapping |= fpindex << 4;
	}

	if (fpindex > fpentries) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Bad flat panel table index\n");
		return -ENOENT;
	}

	/* reserved values - means that ddc or hard coded edid should be used */
	if (bios->fp.strapping == 0xff) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Ignoring FP table\n");
		return 0;
	}

	if (!(mode = xcalloc(1, sizeof(DisplayModeRec))))
		return -ENOMEM;

	modeofs = headerlen + recordlen * fpindex + ofs;
	mode->Clock = le16_to_cpu(*(uint16_t *)&fptable[modeofs]) * 10;
	mode->HDisplay = le16_to_cpu(*(uint16_t *)&fptable[modeofs + 4] + 1);
	mode->HSyncStart = le16_to_cpu(*(uint16_t *)&fptable[modeofs + 10] + 1);
	mode->HSyncEnd = le16_to_cpu(*(uint16_t *)&fptable[modeofs + 12] + 1);
	mode->HTotal = le16_to_cpu(*(uint16_t *)&fptable[modeofs + 14] + 1);
	mode->VDisplay = le16_to_cpu(*(uint16_t *)&fptable[modeofs + 18] + 1);
	mode->VSyncStart = le16_to_cpu(*(uint16_t *)&fptable[modeofs + 24] + 1);
	mode->VSyncEnd = le16_to_cpu(*(uint16_t *)&fptable[modeofs + 26] + 1);
	mode->VTotal = le16_to_cpu(*(uint16_t *)&fptable[modeofs + 28] + 1);
	mode->Flags |= (fptable[modeofs + 30] & 0x10) ? V_PHSYNC : V_NHSYNC;
	mode->Flags |= (fptable[modeofs + 30] & 0x1) ? V_PVSYNC : V_NVSYNC;

	/* for version 1.0:
	 * bytes 1-2 are "panel type", including bits on whether Colour/mono, single/dual link, and type (TFT etc.)
	 * bytes 3-6 are bits per colour in RGBX
	 *  9-10 is HActive
	 * 11-12 is HDispEnd
	 * 13-14 is HValid Start
	 * 15-16 is HValid End
	 * bytes 38-39 relate to spread spectrum settings
	 * bytes 40-43 are something to do with PWM */

	mode->status = MODE_OK;
	mode->type = M_T_DRIVER | M_T_PREFERRED;
	xf86SetModeDefaultName(mode);

//	if (XF86_CRTC_CONFIG_PTR(pScrn)->debug_modes) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Found flat panel mode in BIOS tables:\n");
		xf86PrintModeline(pScrn->scrnIndex, mode);
//	}

	bios->fp.native_mode = mode;

	return 0;
}

int parse_lvds_manufacturer_table(ScrnInfoPtr pScrn, int pxclk)
{
	/* The LVDS table header is (mostly) described in
	 * parse_lvds_manufacturer_table_header(): the BIT header additionally
	 * contains the dual-link transition pxclk (in 10s kHz), at byte 5 - if
	 * straps are not being used for the panel, this specifies the frequency
	 * at which modes should be set up in the dual link style.
	 *
	 * Following the header, the BMP (ver 0xa) table has several records,
	 * indexed by a seperate xlat table, indexed in turn by the fp strap in
	 * EXTDEV_BOOT. Each record had a config byte, followed by 6 script
	 * numbers for use by INIT_SUB which controlled panel init and power,
	 * and finally a dword of ms to sleep between power off and on
	 * operations.
	 *
	 * In the BIT versions, the table following the header serves as an
	 * integrated config and xlat table: the records in the table are
	 * indexed by the FP strap nibble in EXTDEV_BOOT, and each record has
	 * two bytes - the first as a config byte, the second for indexing the
	 * fp mode table pointed to by the BIT 'D' table
	 *
	 * Due to the stage at which DDC is used in X's DDX design, the EDID res
	 * for a panel isn't known at init, so the tests against the pixel clock
	 * in the EDID case for selection of the correct table entry and setting
	 * of the dual link flag cannot be done until later - this function may
	 * be called at runtime with a non-zero pxclk argument to perform these
	 * tests.
	 */

	bios_t *bios = &NVPTR(pScrn)->VBIOS;
	unsigned int lvdsmanufacturerindex = 0;
	struct lvdstableheader lth;
	uint16_t lvdsofs;
	int ret;

	if ((ret = parse_lvds_manufacturer_table_header(pScrn, bios, &lth)))
		return ret;

	switch (lth.lvds_ver) {
	case 0x0a:	/* pre NV40 */
		lvdsmanufacturerindex = bios->data[bios->fp.fpxlatemanufacturertableptr + (bios->fp.strapping & 0xf)];

		/* we're done if this isn't the EDID panel case */
		if (pxclk == 0 || (bios->fp.strapping & 0xf) != 0xf)
			break;

		/* change in behaviour guessed at nv30; see datapoints below */
		if (bios->chip_version < 0x30) {
			/* nv17 behaviour */
			/* it seems the old style lvds script pointer is reused
			 * to select 18/24 bit colour depth for EDID panels */
			lvdsmanufacturerindex = (bios->legacy.lvds_single_a_script_ptr & 1) ? 2 : 0;
			if (pxclk >= bios->fp.duallink_transition_clk)
				lvdsmanufacturerindex++;
		} else {
			/* nv31, nv34 behaviour */
			lvdsmanufacturerindex = 0;
			if (pxclk >= bios->fp.duallink_transition_clk)
				lvdsmanufacturerindex = 2;
			if (pxclk >= 140000)
				lvdsmanufacturerindex = 3;
		}

		/* nvidia set the high nibble of (cr57=f, cr58) to
		 * lvdsmanufacturerindex in this case; we don't */
		break;
	case 0x30:	/* NV4x */
		lvdsmanufacturerindex = bios->fp.strapping & 0xf;
		break;
	case 0x40:	/* G80/G90 */
		lvdsmanufacturerindex = bios->fp.strapping & 0xf;
		break;
	default:
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "LVDS table revision not currently supported\n");
		return -ENOSYS;
	}

	lvdsofs = bios->fp.xlated_entry = bios->fp.lvdsmanufacturerpointer + lth.headerlen + lth.recordlen * lvdsmanufacturerindex;
	switch (lth.lvds_ver) {
	case 0x0a:
		bios->fp.power_off_for_reset = bios->data[lvdsofs] & 1;
		bios->fp.reset_after_pclk_change = bios->data[lvdsofs] & 2;
		bios->fp.dual_link = bios->data[lvdsofs] & 4;
		bios->fp.link_c_increment = bios->data[lvdsofs] & 8;
		bios->fp.if_is_24bit = bios->data[lvdsofs] & 16;
		break;
	case 0x30:
		/* My money would be on there being a 24 bit interface bit in this table,
		 * but I have no example of a laptop bios with a 24 bit panel to confirm that.
		 * Hence we shout loudly if any bit other than bit 0 is set (I've not even
		 * seen bit 1)
		 */
		if (bios->data[lvdsofs] > 1)
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "You have a very unusual laptop display; please report it\n");
		/* no sign of the "power off for reset" or "reset for panel on" bits, but it's safer to assume we should */
		bios->fp.power_off_for_reset = true;
		bios->fp.reset_after_pclk_change = true;
		bios->fp.dual_link = bios->data[lvdsofs] & 1;
		bios->fp.BITbit1 = bios->data[lvdsofs] & 2;
		bios->fp.duallink_transition_clk = le16_to_cpu(*(uint16_t *)&bios->data[bios->fp.lvdsmanufacturerpointer + 5]) * 10;
		break;
	case 0x40:
		/* fairly sure, but not 100% */
		bios->fp.dual_link = bios->data[lvdsofs] & 1;
		bios->fp.duallink_transition_clk = le16_to_cpu(*(uint16_t *)&bios->data[bios->fp.lvdsmanufacturerpointer + 5]) * 10;
		break;
	}

	/* set dual_link flag for EDID case */
	if ((bios->fp.strapping & 0xf) == 0xf && pxclk) {
		if (pxclk >= bios->fp.duallink_transition_clk)
			bios->fp.dual_link = true;
		else
			bios->fp.dual_link = false;
	}

	return 0;
}

int run_tmds_table(ScrnInfoPtr pScrn, struct dcb_entry *dcbent, int head, int pxclk)
{
	/* the pxclk parameter is in kHz
	 *
	 * This runs the TMDS regs setting code found on BIT bios cards
	 *
	 * For ffs(or) == 1 use the first table, for ffs(or) == 2 and
	 * ffs(or) == 3, use the second.
	 */

	NVPtr pNv = NVPTR(pScrn);
	bios_t *bios = &pNv->VBIOS;
	uint16_t clktable = 0, scriptptr;
	uint32_t sel_clk_binding;

	if (dcbent->location != LOC_ON_CHIP)
		return 0;

	switch (ffs(dcbent->or)) {
	case 1:
		clktable = bios->tmds.output0_script_ptr;
		break;
	case 2:
	case 3:
		clktable = bios->tmds.output1_script_ptr;
		break;
	}

	if (!clktable) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Pixel clock comparison table not found\n");
		return -EINVAL;
	}

	scriptptr = clkcmptable(bios, clktable, pxclk);

	if (!scriptptr) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "TMDS output init script not found\n");
		return -ENOENT;
	}

	/* don't let script change pll->head binding */
	sel_clk_binding = nv32_rd(pScrn, NV_RAMDAC_SEL_CLK) & 0x50000;
	run_digital_op_script(pScrn, scriptptr, dcbent, head);
	nv32_wr(pScrn, NV_RAMDAC_SEL_CLK, (nv32_rd(pScrn, NV_RAMDAC_SEL_CLK) & ~0x50000) | sel_clk_binding);

	return 0;
}

static int get_fp_strap(ScrnInfoPtr pScrn, bios_t *bios)
{
	/* the fp strap is normally dictated by the "User Strap" in
	 * PEXTDEV_BOOT_0[20:16], but on BMP cards when bit 2 of the
	 * Internal_Flags struct at 0x48 is set, the user strap gets overriden
	 * by the PCI subsystem ID during POST, but not before the previous user
	 * strap has been committed to CR58 for CR57=0xf on head A, which may be
	 * read and used instead
	 */

	/* Now comes the G80/G90 story, i've only got one hint.
	 * I can read back the clock freq from register 0x00616404.
	 * So for the moment just write 0xF here.
	 */

	if (bios->chip_version >= 0x80)
		return 0xF;

	if (bios->major_version < 5 && bios->data[0x48] & 0x4)
		return (NVReadVgaCrtc5758(NVPTR(pScrn), 0, 0xf) & 0xf);

	return ((nv32_rd(pScrn, NV_PEXTDEV_BOOT_0) >> 16) & 0xf);
}

static void parse_bios_version(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset)
{
	/* offset + 0  (8 bits): Micro version
	 * offset + 1  (8 bits): Minor version
	 * offset + 2  (8 bits): Chip version
	 * offset + 3  (8 bits): Major version
	 */

	bios->major_version = bios->data[offset + 3];
	bios->chip_version = bios->data[offset + 2];
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Bios version %02x.%02x.%02x.%02x\n",
		   bios->data[offset + 3], bios->data[offset + 2],
		   bios->data[offset + 1], bios->data[offset]);
}

static void parse_script_table_pointers(ScrnInfoPtr pScrn, bios_t *bios, uint16_t offset)
{
	/* Parses the init table segment for pointers used in script execution.
	 *
	 * offset + 0  (16 bits): init script tables pointer
	 * offset + 2  (16 bits): macro index table pointer
	 * offset + 4  (16 bits): macro table pointer
	 * offset + 6  (16 bits): condition table pointer
	 * offset + 8  (16 bits): io condition table pointer
	 * offset + 10 (16 bits): io flag condition table pointer
	 * offset + 12 (16 bits): init function table pointer
	 */

	bios->init_script_tbls_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[offset])));
	bios->macro_index_tbl_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 2])));
	bios->macro_tbl_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 4])));
	bios->condition_tbl_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 6])));
	bios->io_condition_tbl_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 8])));
	bios->io_flag_condition_tbl_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 10])));
	bios->init_function_tbl_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 12])));
}

int get_pll_limits(ScrnInfoPtr pScrn, uint32_t limit_match, struct pll_lims *pll_lim)
{
	/* PLL limits table
	 *
	 * Version 0x10: NV31
	 * One byte header (version), one record of 24 bytes
	 * Version 0x11: NV36 - Not implemented
	 * Seems to have same record style as 0x10, but 3 records rather than 1
	 * Version 0x20: Found on Geforce 6 cards
	 * Trivial 4 byte BIT header. 31 (0x1f) byte record length
	 * Version 0x21: Found on Geforce 7, 8 and some Geforce 6 cards
	 * 5 byte header, fifth byte of unknown purpose. 35 (0x23) byte record
	 * length in general, some (integrated) have an extra configuration byte
	 */

	NVPtr pNv = NVPTR(pScrn);
	bios_t *bios = &pNv->VBIOS;
	uint8_t pll_lim_ver = 0, headerlen = 0, recordlen = 0, entries = 0;
	int pllindex = 0;
	uint32_t crystal_strap_mask, crystal_straps;

	if (!bios->pll_limit_tbl_ptr) {
		if (bios->chip_version >= 0x40 || bios->chip_version == 0x31 || bios->chip_version == 0x36) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Pointer to PLL limits table invalid\n");
			return -EINVAL;
		}
	} else
		pll_lim_ver = bios->data[bios->pll_limit_tbl_ptr];

	crystal_strap_mask = 1 << 6;
        /* open coded pNv->twoHeads test */
        if (bios->chip_version > 0x10 && bios->chip_version != 0x15 &&
            bios->chip_version != 0x1a && bios->chip_version != 0x20)
                crystal_strap_mask |= 1 << 22;
	crystal_straps = nvReadEXTDEV(pNv, NV_PEXTDEV_BOOT_0) & crystal_strap_mask;

	switch (pll_lim_ver) {
	/* we use version 0 to indicate a pre limit table bios (single stage pll)
	 * and load the hard coded limits instead */
	case 0:
		break;
	case 0x10:
	case 0x11: /* strictly v0x11 has 3 entries, but the last two don't seem to get used */
		headerlen = 1;
		recordlen = 0x18;
		entries = 1;
		pllindex = 0;
		break;
	case 0x20:
	case 0x21:
		headerlen = bios->data[bios->pll_limit_tbl_ptr + 1];
		recordlen = bios->data[bios->pll_limit_tbl_ptr + 2];
		entries = bios->data[bios->pll_limit_tbl_ptr + 3];
		break;
	default:
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "PLL limits table revision 0x%X not currently supported\n", pll_lim_ver);
		return -ENOSYS;
	}

	/* initialize all members to zero */
	memset(pll_lim, 0, sizeof(struct pll_lims));

	if (pll_lim_ver == 0x10 || pll_lim_ver == 0x11) {
		uint16_t plloffs = bios->pll_limit_tbl_ptr + headerlen + recordlen * pllindex;

		pll_lim->vco1.minfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs])));
		pll_lim->vco1.maxfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + 4])));
		pll_lim->vco2.minfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + 8])));
		pll_lim->vco2.maxfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + 12])));
		pll_lim->vco1.min_inputfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + 16])));
		pll_lim->vco2.min_inputfreq = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + 20])));
		pll_lim->vco1.max_inputfreq = pll_lim->vco2.max_inputfreq = INT_MAX;

		/* these values taken from nv30/31/36 */
		pll_lim->vco1.min_n = 0x1;
		if (bios->chip_version == 0x36)
			pll_lim->vco1.min_n = 0x5;
		pll_lim->vco1.max_n = 0xff;
		pll_lim->vco1.min_m = 0x1;
		pll_lim->vco1.max_m = 0xd;
		pll_lim->vco2.min_n = 0x4;
		/* on nv30, 31, 36 (i.e. all cards with two stage PLLs with this
		 * table version (apart from nv35)), N2 is compared to
		 * maxN2 (0x46) and 10 * maxM2 (0x4), so set maxN2 to 0x28 and
		 * save a comparison
		 */
		pll_lim->vco2.max_n = 0x28;
		if (bios->chip_version == 0x30 || bios->chip_version == 0x35)
		       /* only 5 bits available for N2 on nv30/35 */
			pll_lim->vco2.max_n = 0x1f;
		pll_lim->vco2.min_m = 0x1;
		pll_lim->vco2.max_m = 0x4;
	} else if (pll_lim_ver) {	/* ver 0x20, 0x21 */
		uint16_t plloffs = bios->pll_limit_tbl_ptr + headerlen;
		uint32_t reg = 0; /* default match */
		int i;

		/* first entry is default match, if nothing better. warn if reg field nonzero */
		if (le32_to_cpu(*((uint32_t *)&bios->data[plloffs])))
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
				   "Default PLL limit entry has non-zero register field\n");

		if (limit_match > MAX_PLL_TYPES)
			/* we've been passed a reg as the match */
			reg = limit_match;
		else /* limit match is a pll type */
			for (i = 1; i < entries && !reg; i++) {
				uint32_t cmpreg = le32_to_cpu(*((uint32_t *)(&bios->data[plloffs + recordlen * i])));

				if (limit_match == NVPLL && (cmpreg == NV_RAMDAC_NVPLL || cmpreg == 0x4000))
					reg = cmpreg;
				if (limit_match == MPLL && (cmpreg == NV_RAMDAC_MPLL || cmpreg == 0x4020))
					reg = cmpreg;
				if (limit_match == VPLL1 && (cmpreg == NV_RAMDAC_VPLL || cmpreg == 0x4010))
					reg = cmpreg;
				if (limit_match == VPLL2 && (cmpreg == NV_RAMDAC_VPLL2 || cmpreg == 0x4018))
					reg = cmpreg;
			}

		for (i = 1; i < entries; i++)
			if (le32_to_cpu(*((uint32_t *)&bios->data[plloffs + recordlen * i])) == reg) {
				pllindex = i;
				break;
			}

		plloffs += recordlen * pllindex;

		BIOSLOG(pScrn, "Loading PLL limits for reg 0x%08x\n", pllindex ? reg : 0);

		/* frequencies are stored in tables in MHz, kHz are more useful, so we convert */

		/* What output frequencies can each VCO generate? */
		pll_lim->vco1.minfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 4]))) * 1000;
		pll_lim->vco1.maxfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 6]))) * 1000;
		pll_lim->vco2.minfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 8]))) * 1000;
		pll_lim->vco2.maxfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 10]))) * 1000;

		/* What input frequencies do they accept (past the m-divider)? */
		pll_lim->vco1.min_inputfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 12]))) * 1000;
		pll_lim->vco2.min_inputfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 14]))) * 1000;
		pll_lim->vco1.max_inputfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 16]))) * 1000;
		pll_lim->vco2.max_inputfreq = le16_to_cpu(*((uint16_t *)(&bios->data[plloffs + 18]))) * 1000;

		/* What values are accepted as multiplier and divider? */
		pll_lim->vco1.min_n = bios->data[plloffs + 20];
		pll_lim->vco1.max_n = bios->data[plloffs + 21];
		pll_lim->vco1.min_m = bios->data[plloffs + 22];
		pll_lim->vco1.max_m = bios->data[plloffs + 23];
		pll_lim->vco2.min_n = bios->data[plloffs + 24];
		pll_lim->vco2.max_n = bios->data[plloffs + 25];
		pll_lim->vco2.min_m = bios->data[plloffs + 26];
		pll_lim->vco2.max_m = bios->data[plloffs + 27];

		pll_lim->unk1c = bios->data[plloffs + 28];
		pll_lim->max_log2p_bias = bios->data[plloffs + 29];
		pll_lim->log2p_bias = bios->data[plloffs + 30];

		if (recordlen > 0x22)
			pll_lim->refclk = le32_to_cpu(*((uint32_t *)&bios->data[plloffs + 31]));

		if (recordlen > 0x23)
			if (bios->data[plloffs + 35])
				xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
					   "Bits set in PLL configuration byte (%x)\n", bios->data[plloffs + 35]);

		/* C51 special not seen elsewhere */
		if (bios->chip_version == 0x51 && !pll_lim->refclk) {
			uint32_t sel_clk = nv32_rd(pScrn, NV_RAMDAC_SEL_CLK);

			if (((limit_match == NV_RAMDAC_VPLL || limit_match == VPLL1) && sel_clk & 0x20) ||
			    ((limit_match == NV_RAMDAC_VPLL2 || limit_match == VPLL2) && sel_clk & 0x80)) {
				if (nv_idx_port_rd(pScrn, NV_CIO_CRX__COLOR, NV_CIO_CRE_CHIP_ID_INDEX) < 0xa3)
					pll_lim->refclk = 200000;
				else
					pll_lim->refclk = 25000;
			}
		}
	}

	/* By now any valid limit table ought to have set a max frequency for
	 * vco1, so if it's zero it's either a pre limit table bios, or one
	 * with an empty limit table (seen on nv18)
	 */
	if (!pll_lim->vco1.maxfreq) {
		pll_lim->vco1.minfreq = bios->fminvco;
		pll_lim->vco1.maxfreq = bios->fmaxvco;
		pll_lim->vco1.min_inputfreq = 0;
		pll_lim->vco1.max_inputfreq = INT_MAX;
		pll_lim->vco1.min_n = 0x1;
		pll_lim->vco1.max_n = 0xff;
		pll_lim->vco1.min_m = 0x1;
		if (crystal_straps == 0) {
			/* nv05 does this, nv11 doesn't, nv10 unknown */
			if (bios->chip_version < 0x11)
				pll_lim->vco1.min_m = 0x7;
			pll_lim->vco1.max_m = 0xd;
		} else {
			if (bios->chip_version < 0x11)
				pll_lim->vco1.min_m = 0x8;
			pll_lim->vco1.max_m = 0xe;
		}
	}

	if (!pll_lim->refclk)
		switch (crystal_straps) {
		case 0:
			pll_lim->refclk = 13500;
			break;
		case (1 << 6):
			pll_lim->refclk = 14318;
			break;
		case (1 << 22):
			pll_lim->refclk = 27000;
			break;
		case (1 << 22 | 1 << 6):
			pll_lim->refclk = 25000;
			break;
		}

#if 0 /* for easy debugging */
	ErrorF("pll.vco1.minfreq: %d\n", pll_lim->vco1.minfreq);
	ErrorF("pll.vco1.maxfreq: %d\n", pll_lim->vco1.maxfreq);
	ErrorF("pll.vco2.minfreq: %d\n", pll_lim->vco2.minfreq);
	ErrorF("pll.vco2.maxfreq: %d\n", pll_lim->vco2.maxfreq);

	ErrorF("pll.vco1.min_inputfreq: %d\n", pll_lim->vco1.min_inputfreq);
	ErrorF("pll.vco1.max_inputfreq: %d\n", pll_lim->vco1.max_inputfreq);
	ErrorF("pll.vco2.min_inputfreq: %d\n", pll_lim->vco2.min_inputfreq);
	ErrorF("pll.vco2.max_inputfreq: %d\n", pll_lim->vco2.max_inputfreq);

	ErrorF("pll.vco1.min_n: %d\n", pll_lim->vco1.min_n);
	ErrorF("pll.vco1.max_n: %d\n", pll_lim->vco1.max_n);
	ErrorF("pll.vco1.min_m: %d\n", pll_lim->vco1.min_m);
	ErrorF("pll.vco1.max_m: %d\n", pll_lim->vco1.max_m);
	ErrorF("pll.vco2.min_n: %d\n", pll_lim->vco2.min_n);
	ErrorF("pll.vco2.max_n: %d\n", pll_lim->vco2.max_n);
	ErrorF("pll.vco2.min_m: %d\n", pll_lim->vco2.min_m);
	ErrorF("pll.vco2.max_m: %d\n", pll_lim->vco2.max_m);

	ErrorF("pll.unk1c: %d\n", pll_lim->unk1c);
	ErrorF("pll.max_log2p_bias: %d\n", pll_lim->max_log2p_bias);
	ErrorF("pll.log2p_bias: %d\n", pll_lim->log2p_bias);

	ErrorF("pll.refclk: %d\n", pll_lim->refclk);
#endif

	return 0;
}

static int parse_bit_A_tbl_entry(ScrnInfoPtr pScrn, bios_t *bios, bit_entry_t *bitentry)
{
	/* Parses the load detect values for g80 cards.
	 *
	 * offset + 0 (16 bits): loadval table pointer
	 */

	uint16_t load_table_ptr;
	uint8_t version, headerlen, entrylen, num_entries;

	if (bitentry->length != 3) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Do not understand BIT A table\n");
		return -EINVAL;
	}

	load_table_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[bitentry->offset])));

	if (load_table_ptr == 0x0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Pointer to BIT loadval table invalid\n");
		return -EINVAL;
	}

	version = bios->data[load_table_ptr];

	if (version != 0x10) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "BIT loadval table version %d.%d not supported\n",
			version >> 4, version & 0xF);
		return -ENOSYS;
	}

	headerlen = bios->data[load_table_ptr + 1];
	entrylen = bios->data[load_table_ptr + 2];
	num_entries = bios->data[load_table_ptr + 3];

	if (headerlen != 4 || entrylen != 4 || num_entries != 2) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Do not understand BIT loadval table\n");
		return -EINVAL;
	}

	/* First entry is normal dac, 2nd tv-out perhaps? */
	bios->dactestval = le32_to_cpu(*((uint32_t *)&bios->data[load_table_ptr + headerlen])) & 0x3FF;

	return 0;
}

static int parse_bit_C_tbl_entry(ScrnInfoPtr pScrn, bios_t *bios, bit_entry_t *bitentry)
{
	/* offset + 8  (16 bits): PLL limits table pointer
	 *
	 * There's more in here, but that's unknown.
	 */

	if (bitentry->length < 10) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Do not understand BIT C table\n");
		return -EINVAL;
	}

	bios->pll_limit_tbl_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[bitentry->offset + 8])));

	return 0;
}

static int parse_bit_display_tbl_entry(ScrnInfoPtr pScrn, bios_t *bios, bit_entry_t *bitentry)
{
	/* Parses the flat panel table segment that the bit entry points to.
	 * Starting at bitentry->offset:
	 *
	 * offset + 0  (16 bits): ??? table pointer - seems to have 18 byte records beginning with a freq
	 * offset + 2  (16 bits): mode table pointer
	 */

	if (bitentry->length != 4) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Do not understand BIT display table\n");
		return -EINVAL;
	}

	bios->fp.fptablepointer = le16_to_cpu(*((uint16_t *)(&bios->data[bitentry->offset + 2])));

	return 0;
}

static int parse_bit_init_tbl_entry(ScrnInfoPtr pScrn, bios_t *bios, bit_entry_t *bitentry)
{
	/* Parses the init table segment that the bit entry points to.
	 * 
	 * See parse_script_table_pointers for layout
	 */

	if (bitentry->length < 14) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Do not understand init table\n");
		return -EINVAL;
	}

	parse_script_table_pointers(pScrn, bios, bitentry->offset);

	return 0;
}

static int parse_bit_i_tbl_entry(ScrnInfoPtr pScrn, bios_t *bios, bit_entry_t *bitentry)
{
	/* BIT 'i' (info?) table
	 *
	 * offset + 0  (32 bits): BIOS version dword (as in B table)
	 * offset + 5  (8  bits): BIOS feature byte (same as for BMP?)
	 * offset + 13 (16 bits): pointer to table containing DAC load detection comparison values
	 *
	 * There's other things in the table, purpose unknown
	 */

	uint16_t daccmpoffset;
	uint8_t dacversion, dacheaderlen;

	if (bitentry->length < 6) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "BIT i table not long enough for BIOS version and feature byte\n");
		return -EINVAL;
	}

	parse_bios_version(pScrn, bios, bitentry->offset);

	/* bit 4 seems to indicate a mobile bios, other bits possibly as for BMP feature byte */
	bios->feature_byte = bios->data[bitentry->offset + 5];

	if (bitentry->length < 15) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "BIT i table not long enough for DAC load detection comparison table\n");
		return -EINVAL;
	}

	daccmpoffset = le16_to_cpu(*((uint16_t *)(&bios->data[bitentry->offset + 13])));

	/* doesn't exist on g80 */
	if (!daccmpoffset)
		return 0;

	/* The first value in the table, following the header, is the comparison value
	 * Purpose of subsequent values unknown -- TV load detection?
	 */

	dacversion = bios->data[daccmpoffset];
	dacheaderlen = bios->data[daccmpoffset + 1];

	if (dacversion != 0x00 && dacversion != 0x10) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "DAC load detection comparison table version %d.%d not known\n",
			   dacversion >> 4, dacversion & 0xf);
		return -ENOSYS;
	}

	bios->dactestval = le32_to_cpu(*((uint32_t *)(&bios->data[daccmpoffset + dacheaderlen])));

	return 0;
}

static int parse_bit_lvds_tbl_entry(ScrnInfoPtr pScrn, bios_t *bios, bit_entry_t *bitentry)
{
	/* Parses the LVDS table segment that the bit entry points to.
	 * Starting at bitentry->offset:
	 *
	 * offset + 0  (16 bits): LVDS strap xlate table pointer
	 */

	if (bitentry->length != 2) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Do not understand BIT LVDS table\n");
		return -EINVAL;
	}

	/* no idea if it's still called the LVDS manufacturer table, but the concept's close enough */
	bios->fp.lvdsmanufacturerpointer = le16_to_cpu(*((uint16_t *)(&bios->data[bitentry->offset])));

	return 0;
}

static int parse_bit_M_tbl_entry(ScrnInfoPtr pScrn, bios_t *bios, bit_entry_t *bitentry)
{
	/* offset + 2  (8  bits): number of options in an INIT_RAM_RESTRICT_ZM_REG_GROUP opcode option set
	 * offset + 3  (16 bits): pointer to strap xlate table for RAM restrict option selection
	 *
	 * There's a bunch of bits in this table other than the RAM restrict
	 * stuff that we don't use - their use currently unknown
	 */

	int i;

	/* Older bios versions don't have a sufficiently long table for what we want */
	if (bitentry->length < 0x5)
		return 0;

	/* set up multiplier for INIT_RAM_RESTRICT_ZM_REG_GROUP */
	for (i = 0; itbl_entry[i].name && (itbl_entry[i].id != 0x8f); i++)
		;
	itbl_entry[i].length_multiplier = bios->data[bitentry->offset + 2] * 4;
	init_ram_restrict_zm_reg_group_blocklen = itbl_entry[i].length_multiplier;

	bios->ram_restrict_tbl_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[bitentry->offset + 3])));

	return 0;
}

static int parse_bit_tmds_tbl_entry(ScrnInfoPtr pScrn, bios_t *bios, bit_entry_t *bitentry)
{
	/* Parses the pointer to the TMDS table
	 *
	 * Starting at bitentry->offset:
	 *
	 * offset + 0  (16 bits): TMDS table pointer
	 *
	 * The TMDS table is typically found just before the DCB table, with a
	 * characteristic signature of 0x11,0x13 (1.1 being version, 0x13 being
	 * length?)
	 *
	 * At offset +7 is a pointer to a script, which I don't know how to run yet
	 * At offset +9 is a pointer to another script, likewise
	 * Offset +11 has a pointer to a table where the first word is a pxclk
	 * frequency and the second word a pointer to a script, which should be
	 * run if the comparison pxclk frequency is less than the pxclk desired.
	 * This repeats for decreasing comparison frequencies
	 * Offset +13 has a pointer to a similar table
	 * The selection of table (and possibly +7/+9 script) is dictated by
	 * "or" from the DCB.
	 */

	uint16_t tmdstableptr, script1, script2;

	if (bitentry->length != 2) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Do not understand BIT TMDS table\n");
		return -EINVAL;
	}

	tmdstableptr = le16_to_cpu(*((uint16_t *)(&bios->data[bitentry->offset])));

	if (tmdstableptr == 0x0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Pointer to TMDS table invalid\n");
		return -EINVAL;
	}

	/* nv50+ has v2.0, but we don't parse it atm */
	if (bios->data[tmdstableptr] != 0x11) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "TMDS table revision %d.%d not currently supported\n",
			   bios->data[tmdstableptr] >> 4, bios->data[tmdstableptr] & 0xf);
		return -ENOSYS;
	}

	/* These two scripts are odd: they don't seem to get run even when they are not stubbed */
	script1 = le16_to_cpu(*((uint16_t *)&bios->data[tmdstableptr + 7]));
	script2 = le16_to_cpu(*((uint16_t *)&bios->data[tmdstableptr + 9]));
	if (bios->data[script1] != 'q' || bios->data[script2] != 'q')
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "TMDS table script pointers not stubbed\n");

	bios->tmds.output0_script_ptr = le16_to_cpu(*((uint16_t *)&bios->data[tmdstableptr + 11]));
	bios->tmds.output1_script_ptr = le16_to_cpu(*((uint16_t *)&bios->data[tmdstableptr + 13]));

	return 0;
}

struct bit_table {
	const char id;
	int (* const parse_fn)(ScrnInfoPtr, bios_t *, bit_entry_t *);
};

#define BIT_TABLE(id, funcid) ((struct bit_table){ id, parse_bit_##funcid##_tbl_entry })

static int parse_bit_table(ScrnInfoPtr pScrn, bios_t *bios, const uint16_t bitoffset, struct bit_table *table)
{
	uint8_t maxentries = bios->data[bitoffset + 4];
	int i, offset;
	bit_entry_t bitentry;

	for (i = 0, offset = bitoffset + 6; i < maxentries; i++, offset += 6) {
		bitentry.id[0] = bios->data[offset];

		if (bitentry.id[0] != table->id)
			continue;

		bitentry.id[1] = bios->data[offset + 1];
		bitentry.length = le16_to_cpu(*((uint16_t *)&bios->data[offset + 2]));
		bitentry.offset = le16_to_cpu(*((uint16_t *)&bios->data[offset + 4]));

		return table->parse_fn(pScrn, bios, &bitentry);
	}

	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "BIT table '%c' not found\n", table->id);

	return -ENOSYS;
}

static int parse_bit_structure(ScrnInfoPtr pScrn, bios_t *bios, const uint16_t bitoffset)
{
	int ret;

	/* the only restriction on parsing order currently is having 'i' first
	 * for use of bios->*_version or bios->feature_byte while parsing;
	 * functions shouldn't be actually *doing* anything apart from pulling
	 * data from the image into the bios struct, thus no interdependencies
	 */
	if ((ret = parse_bit_table(pScrn, bios, bitoffset, &BIT_TABLE('i', i)))) /* info? */
		return ret;
	if (bios->major_version >= 0x60) /* g80+ */
		parse_bit_table(pScrn, bios, bitoffset, &BIT_TABLE('A', A));
	if ((ret = parse_bit_table(pScrn, bios, bitoffset, &BIT_TABLE('C', C))))
		return ret;
	parse_bit_table(pScrn, bios, bitoffset, &BIT_TABLE('D', display));
	if ((ret = parse_bit_table(pScrn, bios, bitoffset, &BIT_TABLE('I', init))))
		return ret;
	parse_bit_table(pScrn, bios, bitoffset, &BIT_TABLE('M', M)); /* memory? */
	parse_bit_table(pScrn, bios, bitoffset, &BIT_TABLE('L', lvds));
	parse_bit_table(pScrn, bios, bitoffset, &BIT_TABLE('T', tmds));

	return 0;
}

static int parse_bmp_structure(ScrnInfoPtr pScrn, bios_t *bios, unsigned int offset)
{
	/* Parses the BMP structure for useful things, but does not act on them
	 *
	 * offset +   5: BMP major version
	 * offset +   6: BMP minor version
	 * offset +  10: BCD encoded BIOS version
	 *
	 * offset +  18: init script table pointer (for bios versions < 5.10h)
	 * offset +  20: extra init script table pointer (for bios versions < 5.10h)
	 *
	 * offset +  24: memory init table pointer (used on early bios versions)
	 * offset +  26: SDR memory sequencing setup data table
	 * offset +  28: DDR memory sequencing setup data table
	 *
	 * offset +  54: index of I2C CRTC pair to use for CRT output
	 * offset +  55: index of I2C CRTC pair to use for TV output
	 * offset +  56: index of I2C CRTC pair to use for flat panel output
	 * offset +  58: write CRTC index for I2C pair 0
	 * offset +  59: read CRTC index for I2C pair 0
	 * offset +  60: write CRTC index for I2C pair 1
	 * offset +  61: read CRTC index for I2C pair 1
	 *
	 * offset +  67: maximum internal PLL frequency (single stage PLL)
	 * offset +  71: minimum internal PLL frequency (single stage PLL)
	 *
	 * offset +  75: script table pointers, as described in parse_script_table_pointers
	 *
	 * offset +  89: TMDS single link output A table pointer
	 * offset +  91: TMDS single link output B table pointer
	 * offset + 105: flat panel timings table pointer
	 * offset + 107: flat panel strapping translation table pointer
	 * offset + 117: LVDS manufacturer panel config table pointer
	 * offset + 119: LVDS manufacturer strapping translation table pointer
	 *
	 * offset + 142: PLL limits table pointer
	 */

	NVPtr pNv = NVPTR(pScrn);
	uint8_t bmp_version_major, bmp_version_minor;
	uint16_t bmplength;
	uint16_t legacy_scripts_offset, legacy_i2c_offset;

	/* load needed defaults in case we can't parse this info */
	pNv->dcb_table.i2c[0].write = NV_CIO_CRE_DDC_WR__INDEX;
	pNv->dcb_table.i2c[0].read = NV_CIO_CRE_DDC_STATUS__INDEX;
	pNv->dcb_table.i2c[1].write = NV_CIO_CRE_DDC0_WR__INDEX;
	pNv->dcb_table.i2c[1].read = NV_CIO_CRE_DDC0_STATUS__INDEX;
	bios->digital_min_front_porch = 0x4b;
	bios->fmaxvco = 256000;
	bios->fminvco = 128000;
	bios->fp.duallink_transition_clk = 90000;

	bmp_version_major = bios->data[offset + 5];
	bmp_version_minor = bios->data[offset + 6];

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BMP version %d.%d\n",
		   bmp_version_major, bmp_version_minor);

	/* Make sure that 0x36 is blank and can't be mistaken for a DCB pointer on early versions */
	if (bmp_version_major < 5)
		*(uint16_t *)&bios->data[0x36] = 0;

	/* Seems that the minor version was 1 for all major versions prior to 5 */
	/* Version 6 could theoretically exist, but I suspect BIT happened instead */
	if ((bmp_version_major < 5 && bmp_version_minor != 1) || bmp_version_major > 5) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "You have an unsupported BMP version. Please send in your bios\n");
		return -ENOSYS;
	}

	if (bmp_version_major == 0) /* nothing that's currently useful in this version */
		return 0;
	else if (bmp_version_major == 1)
		bmplength = 44; /* exact for 1.01 */
	else if (bmp_version_major == 2)
		bmplength = 48; /* exact for 2.01 */
	else if (bmp_version_major == 3)
		bmplength = 54; /* guessed - mem init tables added in this version */
	else if (bmp_version_major == 4 || bmp_version_minor < 0x1) /* don't know if 5.0 exists... */
		bmplength = 62; /* guessed - BMP I2C indices added in version 4*/
	else if (bmp_version_minor < 0x6)
		bmplength = 67; /* exact for 5.01 */
	else if (bmp_version_minor < 0x10)
		bmplength = 75; /* exact for 5.06 */
	else if (bmp_version_minor == 0x10)
		bmplength = 89; /* exact for 5.10h */
	else if (bmp_version_minor < 0x14)
		bmplength = 118; /* exact for 5.11h */
	else if (bmp_version_minor < 0x24) /* not sure of version where pll limits came in;
					    * certainly exist by 0x24 though */
		/* length not exact: this is long enough to get lvds members */
		bmplength = 123;
	else if (bmp_version_minor < 0x27)
		/* length not exact: this is long enough to get pll limit member */
		bmplength = 144;
	else
		/* length not exact: this is long enough to get dual link transition clock */
		bmplength = 158;

	/* checksum */
	if (nv_cksum(bios->data + offset, 8)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Bad BMP checksum\n");
		return -EINVAL;
	}

	/* bit 4 seems to indicate a mobile bios, bit 5 that the flat panel
	 * tables are present, and bit 6 a tv bios */
	bios->feature_byte = bios->data[offset + 9];

	parse_bios_version(pScrn, bios, offset + 10);

	if (bmp_version_major < 5 || bmp_version_minor < 0x10)
		bios->old_style_init = true;
	legacy_scripts_offset = offset + 18;
	if (bmp_version_major < 2)
		legacy_scripts_offset -= 4;
	bios->init_script_tbls_ptr = le16_to_cpu(*(uint16_t *)&bios->data[legacy_scripts_offset]);
	bios->extra_init_script_tbl_ptr = le16_to_cpu(*(uint16_t *)&bios->data[legacy_scripts_offset + 2]);

	if (bmp_version_major > 2) {	/* appears in BMP 3 */
		bios->legacy.mem_init_tbl_ptr = le16_to_cpu(*(uint16_t *)&bios->data[offset + 24]);
		bios->legacy.sdr_seq_tbl_ptr = le16_to_cpu(*(uint16_t *)&bios->data[offset + 26]);
		bios->legacy.ddr_seq_tbl_ptr = le16_to_cpu(*(uint16_t *)&bios->data[offset + 28]);
	}

	legacy_i2c_offset = 0x48;	/* BMP version 2 & 3 */
	if (bmplength > 61)
		legacy_i2c_offset = offset + 54;
	bios->legacy.i2c_indices.crt = bios->data[legacy_i2c_offset];
	bios->legacy.i2c_indices.tv = bios->data[legacy_i2c_offset + 1];
	bios->legacy.i2c_indices.panel = bios->data[legacy_i2c_offset + 2];
	pNv->dcb_table.i2c[0].write = bios->data[legacy_i2c_offset + 4];
	pNv->dcb_table.i2c[0].read = bios->data[legacy_i2c_offset + 5];
	pNv->dcb_table.i2c[1].write = bios->data[legacy_i2c_offset + 6];
	pNv->dcb_table.i2c[1].read = bios->data[legacy_i2c_offset + 7];

	if (bmplength > 74) {
		bios->fmaxvco = le32_to_cpu(*((uint32_t *)&bios->data[offset + 67]));
		bios->fminvco = le32_to_cpu(*((uint32_t *)&bios->data[offset + 71]));
	}
	if (bmplength > 88)
		parse_script_table_pointers(pScrn, bios, offset + 75);
	if (bmplength > 94) {
		bios->tmds.output0_script_ptr = le16_to_cpu(*((uint16_t *)&bios->data[offset + 89]));
		bios->tmds.output1_script_ptr = le16_to_cpu(*((uint16_t *)&bios->data[offset + 91]));
		/* never observed in use with lvds scripts, but is reused for
		 * 18/24 bit panel interface default for EDID equipped panels
		 * (if_is_24bit not set directly to avoid any oscillation) */
		bios->legacy.lvds_single_a_script_ptr = le16_to_cpu(*((uint16_t *)&bios->data[offset + 95]));
	}
	if (bmplength > 108) {
		bios->fp.fptablepointer = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 105])));
		bios->fp.fpxlatetableptr = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 107])));
		bios->fp.xlatwidth = 1;
	}
	if (bmplength > 120) {
		bios->fp.lvdsmanufacturerpointer = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 117])));
		bios->fp.fpxlatemanufacturertableptr = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 119])));
	}
	if (bmplength > 143)
		bios->pll_limit_tbl_ptr = le16_to_cpu(*((uint16_t *)(&bios->data[offset + 142])));

	if (bmplength > 157)
		bios->fp.duallink_transition_clk = le16_to_cpu(*((uint16_t *)&bios->data[offset + 156])) * 10;

	return 0;
}

static uint16_t findstr(uint8_t *data, int n, const uint8_t *str, int len)
{
	int i, j;

	for (i = 0; i <= (n - len); i++) {
		for (j = 0; j < len; j++)
			if (data[i + j] != str[j])
				break;
		if (j == len)
			return i;
	}

	return 0;
}

static int
read_dcb_i2c_entry(ScrnInfoPtr pScrn, int dcb_version, uint16_t i2ctabptr, int index)
{
	NVPtr pNv = NVPTR(pScrn);
	uint8_t *i2ctable = &pNv->VBIOS.data[i2ctabptr];
	uint8_t dcb_i2c_ver = dcb_version, headerlen = 0, entry_len = 4;
	int i2c_entries = MAX_NUM_DCB_ENTRIES;
	int recordoffset = 0, rdofs = 1, wrofs = 0;
	uint8_t port_type = 0;

	if (!i2ctabptr)
		return -EINVAL;

	if (dcb_version >= 0x30) {
		if (i2ctable[0] != dcb_version) /* necessary? */
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
				   "DCB I2C table version mismatch (%02X vs %02X)\n",
				   i2ctable[0], dcb_version);
		dcb_i2c_ver = i2ctable[0];
		headerlen = i2ctable[1];
		i2c_entries = i2ctable[2];
		entry_len = i2ctable[3];
		/* XXX this requires a dcb entry calls read_dcb_i2c_entry */
		pNv->dcb_table.i2c_default_indices = i2ctable[4];
	}
	/* it's your own fault if you call this function on a DCB 1.1 BIOS --
	 * the test below is for DCB 1.2
	 */
	if (dcb_version < 0x14) {
		recordoffset = 2;
		rdofs = 0;
		wrofs = 1;
	}

	if (index == 0xf)
		return 0;
	if (index > i2c_entries) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "DCB I2C index too big (%d > %d)\n",
			   index, i2ctable[2]);
		return -ENOENT;
	}
	if (i2ctable[headerlen + entry_len * index + 3] == 0xff) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "DCB I2C entry invalid\n");
		return -EINVAL;
	}

	if (dcb_i2c_ver >= 0x30) {
		port_type = i2ctable[headerlen + recordoffset + 3 + entry_len * index];

		/* fixup for chips using same address offset for read and write */
		if (port_type == 4)	/* seen on C51 */
			rdofs = wrofs = 1;
		if (port_type == 5)	/* G80+ */
			rdofs = wrofs = 0;
	}
	if (dcb_i2c_ver >= 0x40 && port_type != 5)
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "DCB I2C table has port type %d\n", port_type);

	pNv->dcb_table.i2c[index].port_type = port_type;
	pNv->dcb_table.i2c[index].read = i2ctable[headerlen + recordoffset + rdofs + entry_len * index];
	pNv->dcb_table.i2c[index].write = i2ctable[headerlen + recordoffset + wrofs + entry_len * index];

	return 0;
}

static int init_dcb_i2c_entry(ScrnInfoPtr pScrn, bios_t *bios, int index)
{
	NVPtr pNv = NVPTR(pScrn);
	uint16_t dcbptr = le16_to_cpu(*(uint16_t *)&bios->data[0x36]);
	uint8_t dcb_version = bios->data[dcbptr];
	uint16_t i2ctabptr = le16_to_cpu(*(uint16_t *)&bios->data[dcbptr + ((dcb_version < 0x30) ? 2 : 4)]);
	int ret;
	char adaptorname[11];

	if (pNv->dcb_table.i2c[index].chan)
		return 0;

	if (!dcbptr) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No Display Configuration Block pointer found\n");
		return -EINVAL;
	}
	if (dcb_version < 0x12) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "DCB table not version 1.2 or greater\n");
		return -ENOSYS;
	}
	if (!i2ctabptr) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No pointer to DCB I2C port table\n");
		return -EINVAL;
	}

	if ((ret = read_dcb_i2c_entry(pScrn, dcb_version, i2ctabptr, index)))
		return ret;

	snprintf(adaptorname, 11, "DCB-I2C-%d", index);

	return NV_I2CInit(pScrn, &pNv->dcb_table.i2c[index].chan, &pNv->dcb_table.i2c[index], xstrdup(adaptorname));
}

static bool
parse_dcb_entry(ScrnInfoPtr pScrn, int index, uint8_t dcb_version, uint16_t i2ctabptr, uint32_t conn, uint32_t conf)
{
	NVPtr pNv = NVPTR(pScrn);
	struct dcb_entry *entry = &pNv->dcb_table.entry[index];

	memset(entry, 0, sizeof (struct dcb_entry));

	entry->index = index;
	/* safe defaults for a crt */
	entry->type = 0;
	entry->i2c_index = 0;
	entry->heads = 1;
	entry->bus = 0;
	entry->location = LOC_ON_CHIP;
	entry->or = 1;
	entry->duallink_possible = false;

	if (dcb_version >= 0x20) {
		entry->type = conn & 0xf;
		entry->i2c_index = (conn >> 4) & 0xf;
		entry->heads = (conn >> 8) & 0xf;
		entry->bus = (conn >> 16) & 0xf;
		entry->location = (conn >> 20) & 0xf;
		entry->or = (conn >> 24) & 0xf;
		/* Normal entries consist of a single bit, but dual link has the
		 * adjacent more significant bit set too
		 */
		if ((1 << (ffs(entry->or) - 1)) * 3 == entry->or)
			entry->duallink_possible = true;

		switch (entry->type) {
		case OUTPUT_LVDS:
			{
			uint32_t mask;
			if (conf & 0x1)
				entry->lvdsconf.use_straps_for_mode = true;
			if (dcb_version < 0x22) {
				mask = ~0xd;
				/* both 0x4 and 0x8 show up in v2.0 tables; assume they mean
				 * the same thing, which is probably wrong, but might work */
				if (conf & 0x4 || conf & 0x8)
					entry->lvdsconf.use_power_scripts = true;
			} else {
				mask = ~0x5;
				if (conf & 0x4)
					entry->lvdsconf.use_power_scripts = true;
			}
			if (conf & mask) {
				/* I'm bored of getting this reported; left as a reminder for someone to fix it */
				if (dcb_version >= 0x40) {
					xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
						   "G80+ LVDS not initialized by driver; ignoring conf bits\n");
					break;
				}
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
					   "Unknown LVDS configuration bits, please report\n");
				/* cause output setting to fail, so message is seen */
				pNv->dcb_table.entries = 0;
				return false;
			}
			break;
			}
		case 0xe:
			/* weird type that appears on g80 mobile bios; nv driver treats it as a terminator */
			return false;
		}
		/* unsure what DCB version introduces this, 3.0? */
		if (conf & 0x100000)
			entry->i2c_upper_default = true;

		read_dcb_i2c_entry(pScrn, dcb_version, i2ctabptr, entry->i2c_index);
	} else if (dcb_version >= 0x14 ) {
		if (conn != 0xf0003f00 && conn != 0xf2247f10 &&
		    conn != 0xf2204001 && conn != 0xf2204301 && conn != 0xf2204311 && conn != 0xf2208001 && conn != 0xf2244001 && conn != 0xf2244301 && conn != 0xf2244311 && conn != 0xf4204011 && conn != 0xf4208011 && conn != 0xf4248011 &&
		    conn != 0xf2045f14 && conn != 0xf2205004) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Unknown DCB 1.4 / 1.5 entry, please report\n");

			/* cause output setting to fail for non-TVs, so message is seen */
			if ((conn & 0xf) != 0x1)
				pNv->dcb_table.entries = 0;

			return false;
		}
		/* most of the below is a "best guess" atm */
		entry->type = conn & 0xf;
		if (entry->type == 4) { /* digital */
			if (conn & 0x10)
				entry->type = OUTPUT_LVDS;
			else
				entry->type = OUTPUT_TMDS;
		}
		/* what's in bits 5-13? could be some brooktree/chrontel/philips thing, in tv case */
		entry->i2c_index = (conn >> 14) & 0xf;
		/* raw heads field is in range 0-1, so move to 1-2 */
		entry->heads = ((conn >> 18) & 0x7) + 1;
		entry->location = (conn >> 21) & 0xf;
		entry->bus = (conn >> 25) & 0x7;
		/* set or to be same as heads -- hopefully safe enough */
		entry->or = entry->heads;

		switch (entry->type) {
		case OUTPUT_LVDS:
			/* this is probably buried in conn's unknown bits */
			entry->lvdsconf.use_power_scripts = true;
			break;
		case OUTPUT_TMDS:
			/* invent a DVI-A output, by copying the fields of the DVI-D output
			 * reported to work by math_b on an NV20(!) */
			memcpy(&entry[1], &entry[0], sizeof(struct dcb_entry));
			entry[1].index = ++index;
			entry[1].type = OUTPUT_ANALOG;
			xf86DrvMsg(pScrn->scrnIndex, X_NOTICE,
				   "Concocting additional DCB entry for analogue encoder on DVI output\n");
			pNv->dcb_table.entries++;
		}
		read_dcb_i2c_entry(pScrn, dcb_version, i2ctabptr, entry->i2c_index);
	} else if (dcb_version >= 0x12) {
		/* v1.2 tables normally have the same 5 entries, which are not
		 * specific to the card, so use the defaults for a crt */
		/* DCB v1.2 does have an I2C table that read_dcb_i2c_table can handle, but cards
		 * exist (seen on nv11) where the pointer to the table points to the wrong
		 * place, so for now, we rely on the indices parsed in parse_bmp_structure
		 */
		entry->i2c_index = pNv->VBIOS.legacy.i2c_indices.crt;
	} else { /* pre DCB / v1.1 - use the safe defaults for a crt */
		xf86DrvMsg(pScrn->scrnIndex, X_NOTICE,
			   "No information in BIOS output table; assuming a CRT output exists\n");
		entry->i2c_index = pNv->VBIOS.legacy.i2c_indices.crt;
	}

	if (entry->type == OUTPUT_LVDS && pNv->VBIOS.fp.strapping != 0xff)
		entry->lvdsconf.use_straps_for_mode = true;

	pNv->dcb_table.entries++;

	return true;
}

void merge_like_dcb_entries(ScrnInfoPtr pScrn)
{
	/* DCB v2.0 lists each output combination separately.
	 * Here we merge compatible entries to have fewer outputs, with more options
	 */

	NVPtr pNv = NVPTR(pScrn);
	int i, newentries = 0;

	for (i = 0; i < pNv->dcb_table.entries; i++) {
		struct dcb_entry *ient = &pNv->dcb_table.entry[i];
		int j;

		for (j = i + 1; j < pNv->dcb_table.entries; j++) {
			struct dcb_entry *jent = &pNv->dcb_table.entry[j];

			if (jent->type == 100) /* already merged entry */
				continue;

			/* merge heads field when all other fields the same */
			if (jent->i2c_index == ient->i2c_index && jent->type == ient->type && jent->location == ient->location && jent->or == ient->or) {
				xf86DrvMsg(pScrn->scrnIndex, X_INFO,
					   "Merging DCB entries %d and %d\n", i, j);
				ient->heads |= jent->heads;
				jent->type = 100; /* dummy value */
			}
		}
	}

	/* Compact entries merged into others out of dcb_table */
	for (i = 0; i < pNv->dcb_table.entries; i++) {
		if ( pNv->dcb_table.entry[i].type == 100 )
			continue;

		if (newentries != i)
			memcpy(&pNv->dcb_table.entry[newentries], &pNv->dcb_table.entry[i], sizeof(struct dcb_entry));
		newentries++;
	}

	pNv->dcb_table.entries = newentries;
}

static int parse_dcb_table(ScrnInfoPtr pScrn, bios_t *bios)
{
	NVPtr pNv = NVPTR(pScrn);
	uint16_t dcbptr, i2ctabptr = 0;
	uint8_t *dcbtable;
	uint8_t dcb_version, headerlen = 0x4, entries = MAX_NUM_DCB_ENTRIES;
	bool configblock = true;
	int recordlength = 8, confofs = 4;
	int i;

	pNv->dcb_table.entries = 0;

	/* get the offset from 0x36 */
	dcbptr = le16_to_cpu(*(uint16_t *)&bios->data[0x36]);

	if (dcbptr == 0x0) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "No Display Configuration Block pointer found\n");
		/* this situation likely means a really old card, pre DCB, so we'll add the safe CRT entry */
		parse_dcb_entry(pScrn, 0, 0, 0, 0, 0);
		return 0;
	}

	dcbtable = &bios->data[dcbptr];

	/* get DCB version */
	dcb_version = dcbtable[0];
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Found Display Configuration Block version %d.%d\n",
		   dcb_version >> 4, dcb_version & 0xf);

	if (dcb_version >= 0x20) { /* NV17+ */
		uint32_t sig;

		if (dcb_version >= 0x30) { /* NV40+ */
			headerlen = dcbtable[1];
			entries = dcbtable[2];
			recordlength = dcbtable[3];
			i2ctabptr = le16_to_cpu(*(uint16_t *)&dcbtable[4]);
			sig = le32_to_cpu(*(uint32_t *)&dcbtable[6]);
		} else {
			i2ctabptr = le16_to_cpu(*(uint16_t *)&dcbtable[2]);
			sig = le32_to_cpu(*(uint32_t *)&dcbtable[4]);
			headerlen = 8;
		}

		if (sig != 0x4edcbdcb) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Bad Display Configuration Block signature (%08X)\n", sig);
			return -EINVAL;
		}
	} else if (dcb_version >= 0x14) { /* some NV15/16, and NV11+ */
		char sig[8] = { 0 };

		strncpy(sig, (char *)&dcbtable[-7], 7);
		i2ctabptr = le16_to_cpu(*(uint16_t *)&dcbtable[2]);
		recordlength = 10;
		confofs = 6;

		if (strcmp(sig, "DEV_REC")) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Bad Display Configuration Block signature (%s)\n", sig);
			return -EINVAL;
		}
	} else if (dcb_version >= 0x12) { /* some NV6/10, and NV15+ */
		i2ctabptr = le16_to_cpu(*(uint16_t *)&dcbtable[2]);
		configblock = false;
	} else {	/* NV5+, maybe NV4 */
		/* DCB 1.1 seems to be quite unhelpful - we'll just add the safe CRT entry */
		parse_dcb_entry(pScrn, 0, dcb_version, 0, 0, 0);
		return 0;
	}

	if (!i2ctabptr)
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "No pointer to DCB I2C port table\n");

	if (entries >= MAX_NUM_DCB_ENTRIES)
		entries = MAX_NUM_DCB_ENTRIES;

	for (i = 0; i < entries; i++) {
		uint32_t connection, config = 0;

		connection = le32_to_cpu(*(uint32_t *)&dcbtable[headerlen + recordlength * i]);
		if (configblock)
			config = le32_to_cpu(*(uint32_t *)&dcbtable[headerlen + confofs + recordlength * i]);

		/* Should we allow discontinuous DCBs? Certainly DCB I2C tables can be discontinuous */
		if ((connection & 0x0000000f) == 0x0000000f) /* end of records */
			break;
		if (connection == 0x00000000) /* seen on an NV11 with DCB v1.5 */
			break;

		xf86DrvMsg(pScrn->scrnIndex, X_NOTICE, "Raw DCB entry %d: %08x %08x\n",
			   pNv->dcb_table.entries, connection, config);

		if (!parse_dcb_entry(pScrn, pNv->dcb_table.entries, dcb_version, i2ctabptr, connection, config))
			break;
	}

	merge_like_dcb_entries(pScrn);

	return 0;
}

static int load_nv17_hwsq_ucode_entry(ScrnInfoPtr pScrn, bios_t *bios, uint16_t hwsq_offset, int entry)
{
	/* The header following the "HWSQ" signature has the number of entries,
	 * and the entry size
	 *
	 * An entry consists of a dword to write to the sequencer control reg
	 * (0x00001304), followed by the ucode bytes, written sequentially,
	 * starting at reg 0x00001400
	 */

	uint8_t bytes_to_write;
	uint16_t hwsq_entry_offset;
	int i;

	if (bios->data[hwsq_offset] <= entry) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Too few entries in HW sequencer table for requested entry\n");
		return -ENOENT;
	}

	bytes_to_write = bios->data[hwsq_offset + 1];

	if (bytes_to_write != 36) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Unknown HW sequencer entry size\n");
		return -EINVAL;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Loading NV17 power sequencing microcode\n");

	hwsq_entry_offset = hwsq_offset + 2 + entry * bytes_to_write;

	/* set sequencer control */
	nv32_wr(pScrn, 0x00001304, le32_to_cpu(*(uint32_t *)&bios->data[hwsq_entry_offset]));
	bytes_to_write -= 4;

	/* write ucode */
	for (i = 0; i < bytes_to_write; i += 4)
		nv32_wr(pScrn, 0x00001400 + i, le32_to_cpu(*(uint32_t *)&bios->data[hwsq_entry_offset + i + 4]));

	/* twiddle NV_PBUS_DEBUG_4 */
	nv32_wr(pScrn, NV_PBUS_DEBUG_4, nv32_rd(pScrn, NV_PBUS_DEBUG_4) | 0x18);

	return 0;
}

static int load_nv17_hw_sequencer_ucode(ScrnInfoPtr pScrn, bios_t *bios)
{
	/* BMP based cards, from NV17, need a microcode loading to correctly
	 * control the GPIO etc for LVDS panels
	 *
	 * BIT based cards seem to do this directly in the init scripts
	 *
	 * The microcode entries are found by the "HWSQ" signature.
	 */

	const uint8_t hwsq_signature[] = { 'H', 'W', 'S', 'Q' };
	int hwsq_offset;

	if (!(hwsq_offset = findstr(bios->data, bios->length, hwsq_signature,
				    sizeof(hwsq_signature))))
		return 0;

	/* always use entry 0? */
	return load_nv17_hwsq_ucode_entry(pScrn, bios,
					  hwsq_offset + sizeof(hwsq_signature), 0);
}

static int read_bios_edid(ScrnInfoPtr pScrn)
{
	bios_t *bios = &NVPTR(pScrn)->VBIOS;
	const uint8_t edid_sig[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
	uint16_t offset = 0, newoffset;
	int searchlen = NV_PROM_SIZE, i;

	while (searchlen) {
		if (!(newoffset = findstr(&bios->data[offset], searchlen, edid_sig, 8)))
			return -ENOENT;
		offset += newoffset;
		if (!nv_cksum(&bios->data[offset], EDID1_LEN))
			break;

		searchlen -= offset;
		offset++;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Found EDID in BIOS\n");

	if (!(bios->fp.edid = xalloc(EDID1_LEN)))
		return -ENOMEM;
	for (i = 0; i < EDID1_LEN; i++)
		bios->fp.edid[i] = bios->data[offset + i];

	return 0;
}

bool NVInitVBIOS(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	memset(&pNv->VBIOS, 0, sizeof(bios_t));

	if (!NVShadowVBIOS(pScrn, pNv->VBIOS.data))
		return false;

	pNv->VBIOS.length = pNv->VBIOS.data[2] * 512;
	if (pNv->VBIOS.length > NV_PROM_SIZE)
		pNv->VBIOS.length = NV_PROM_SIZE;

	return true;
}

int NVRunVBIOSInit(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_bios *bios = &NVPTR(pScrn)->VBIOS;
	const uint8_t bit_signature[] = { 0xff, 0xb8, 'B', 'I', 'T' };
	const uint8_t bmp_signature[] = { 0xff, 0x7f, 'N', 'V', 0x0 };
	int offset, ret;

	if ((offset = findstr(bios->data, bios->length, bit_signature, sizeof(bit_signature)))) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BIT BIOS found\n");
		ret = parse_bit_structure(pScrn, bios, offset + 6);
	} else if ((offset = findstr(bios->data, bios->length, bmp_signature, sizeof(bmp_signature)))) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BMP BIOS found\n");
		ret = parse_bmp_structure(pScrn, bios, offset);
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No known BIOS signature found\n");
		ret = -ENODEV;
	}

	if (ret || bios->major_version == 0) /* we don't parse version 0 bios */
		return ret;

	NVLockVgaCrtcs(pNv, false);
	if (pNv->twoHeads)
		NVSetOwner(pNv, crtchead);

	if (bios->major_version < 5)	/* BMP only */
		load_nv17_hw_sequencer_ucode(pScrn, bios);

	parse_init_tables(pScrn, bios);

	/* all BIT systems need parse_fp_mode.. for digital_min_front_porch */
	if (bios->feature_byte & FEATURE_MOBILE || bios->major_version >= 5) {
#ifdef __powerpc__
		/* PPC cards don't have the fp table; the laptops use DDC */
		bios->digital_min_front_porch = 0x4b;
#else
		bios->fp.strapping = get_fp_strap(pScrn, bios);
		if ((ret = parse_fp_mode_table(pScrn, bios)))
			goto out;
#endif
	}
	if (bios->feature_byte & FEATURE_MOBILE)
		ret = parse_lvds_manufacturer_table(pScrn, 0);

out:
	NVLockVgaCrtcs(pNv, true);
	if (pNv->twoHeads)
		NVSetOwner(pNv, crtchead);

	return ret;
}

int NVParseBios(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	uint32_t saved_nv_pextdev_boot_0;
	int i, ret;

	if (!NVInitVBIOS(pScrn))
		return -ENODEV;

	/* these will need remembering across a suspend */
	saved_nv_pextdev_boot_0 = nv32_rd(pScrn, NV_PEXTDEV_BOOT_0);
	saved_nv_pfb_cfg0 = nv32_rd(pScrn, NV_PFB_CFG0);

	/* init script execution disabled */
	pNv->VBIOS.execute = false;

	nv32_wr(pScrn, NV_PEXTDEV_BOOT_0, saved_nv_pextdev_boot_0);

	if ((ret = NVRunVBIOSInit(pScrn)))
		return ret;

	parse_dcb_table(pScrn, &pNv->VBIOS);

	for (i = 0 ; i < pNv->dcb_table.entries; i++)
		if (pNv->dcb_table.entry[i].type == OUTPUT_LVDS)
			call_lvds_script(pScrn, &pNv->dcb_table.entry[i], nv_get_digital_bound_head(pNv, pNv->dcb_table.entry[i].or), LVDS_INIT, 0);

	if (pNv->VBIOS.feature_byte & FEATURE_MOBILE && !pNv->VBIOS.fp.native_mode)
		read_bios_edid(pScrn);

	/* allow subsequent scripts to execute */
	pNv->VBIOS.execute = true;

	return 0;
}
