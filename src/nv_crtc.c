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

#include <assert.h>
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

#include "vgaHW.h"

#define CRTC_INDEX 0x3d4
#define CRTC_DATA 0x3d5
#define CRTC_IN_STAT_1 0x3da

#define WHITE_VALUE 0x3F
#define BLACK_VALUE 0x00
#define OVERSCAN_VALUE 0x01

static void nv_crtc_load_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_load_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state, Bool override);
static void nv_crtc_load_state_ramdac(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_save_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_save_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_save_state_ramdac(xf86CrtcPtr crtc, RIVA_HW_STATE *state);

static CARD8 NVReadPVIO(xf86CrtcPtr crtc, CARD32 address)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	/* Only NV4x have two pvio ranges */
	if (nv_crtc->head == 1 && pNv->Architecture == NV_ARCH_40) {
		return NV_RD08(pNv->PVIO1, address);
	} else {
		return NV_RD08(pNv->PVIO0, address);
	}
}

static void NVWritePVIO(xf86CrtcPtr crtc, CARD32 address, CARD8 value)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	/* Only NV4x have two pvio ranges */
	if (nv_crtc->head == 1 && pNv->Architecture == NV_ARCH_40) {
		NV_WR08(pNv->PVIO1, address, value);
	} else {
		NV_WR08(pNv->PVIO0, address, value);
	}
}

static void NVWriteMiscOut(xf86CrtcPtr crtc, CARD8 value)
{
	NVWritePVIO(crtc, VGA_MISC_OUT_W, value);
}

static CARD8 NVReadMiscOut(xf86CrtcPtr crtc)
{
	return NVReadPVIO(crtc, VGA_MISC_OUT_R);
}

void NVWriteVGA(NVPtr pNv, int head, CARD8 index, CARD8 value)
{
	volatile CARD8 *pCRTCReg = head ? pNv->PCIO1 : pNv->PCIO0;

	NV_WR08(pCRTCReg, CRTC_INDEX, index);
	NV_WR08(pCRTCReg, CRTC_DATA, value);
}

CARD8 NVReadVGA(NVPtr pNv, int head, CARD8 index)
{
	volatile CARD8 *pCRTCReg = head ? pNv->PCIO1 : pNv->PCIO0;

	NV_WR08(pCRTCReg, CRTC_INDEX, index);
	return NV_RD08(pCRTCReg, CRTC_DATA);
}

/* CR57 and CR58 are a fun pair of regs. CR57 provides an index (0-0xf) for CR58
 * I suspect they in fact do nothing, but are merely a way to carry useful
 * per-head variables around
 *
 * Known uses:
 * CR57		CR58
 * 0x00		index to the appropriate dcb entry (or 7f for inactive)
 * 0x02		dcb entry's "or" value (or 00 for inactive)
 * 0x0f		laptop panel info -	high nibble for PEXTDEV_BOOT strap
 * 					low nibble for xlat strap value
 */

void NVWriteVGACR5758(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	NVWriteVGA(pNv, head, 0x57, index);
	NVWriteVGA(pNv, head, 0x58, value);
}

uint8_t NVReadVGACR5758(NVPtr pNv, int head, uint8_t index)
{
	NVWriteVGA(pNv, head, 0x57, index);
	return NVReadVGA(pNv, head, 0x58);
}

void NVWriteVgaCrtc(xf86CrtcPtr crtc, CARD8 index, CARD8 value)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	NVWriteVGA(pNv, nv_crtc->head, index, value);
}

CARD8 NVReadVgaCrtc(xf86CrtcPtr crtc, CARD8 index)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	return NVReadVGA(pNv, nv_crtc->head, index);
}

static void NVWriteVgaSeq(xf86CrtcPtr crtc, CARD8 index, CARD8 value)
{
	NVWritePVIO(crtc, VGA_SEQ_INDEX, index);
	NVWritePVIO(crtc, VGA_SEQ_DATA, value);
}

static CARD8 NVReadVgaSeq(xf86CrtcPtr crtc, CARD8 index)
{
	NVWritePVIO(crtc, VGA_SEQ_INDEX, index);
	return NVReadPVIO(crtc, VGA_SEQ_DATA);
}

static void NVWriteVgaGr(xf86CrtcPtr crtc, CARD8 index, CARD8 value)
{
	NVWritePVIO(crtc, VGA_GRAPH_INDEX, index);
	NVWritePVIO(crtc, VGA_GRAPH_DATA, value);
}

static CARD8 NVReadVgaGr(xf86CrtcPtr crtc, CARD8 index)
{
	NVWritePVIO(crtc, VGA_GRAPH_INDEX, index);
	return NVReadPVIO(crtc, VGA_GRAPH_DATA);
} 


static void NVWriteVgaAttr(xf86CrtcPtr crtc, CARD8 index, CARD8 value)
{
  ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pCRTCReg = nv_crtc->head ? pNv->PCIO1 : pNv->PCIO0;

  NV_RD08(pCRTCReg, CRTC_IN_STAT_1);
  if (nv_crtc->paletteEnabled)
    index &= ~0x20;
  else
    index |= 0x20;
  NV_WR08(pCRTCReg, VGA_ATTR_INDEX, index);
  NV_WR08(pCRTCReg, VGA_ATTR_DATA_W, value);
}

static CARD8 NVReadVgaAttr(xf86CrtcPtr crtc, CARD8 index)
{
  ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pCRTCReg = nv_crtc->head ? pNv->PCIO1 : pNv->PCIO0;

  NV_RD08(pCRTCReg, CRTC_IN_STAT_1);
  if (nv_crtc->paletteEnabled)
    index &= ~0x20;
  else
    index |= 0x20;
  NV_WR08(pCRTCReg, VGA_ATTR_INDEX, index);
  return NV_RD08(pCRTCReg, VGA_ATTR_DATA_R);
}

void NVCrtcSetOwner(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	/* Non standard beheaviour required by NV11 */
	if (pNv) {
		uint8_t owner = NVReadVGA0(pNv, NV_VGA_CRTCX_OWNER);
		ErrorF("pre-Owner: 0x%X\n", owner);
		if (owner == 0x04) {
			uint32_t pbus84 = nvReadMC(pNv, 0x1084);
			ErrorF("pbus84: 0x%X\n", pbus84);
			pbus84 &= ~(1<<28);
			ErrorF("pbus84: 0x%X\n", pbus84);
			nvWriteMC(pNv, 0x1084, pbus84);
		}
		/* The blob never writes owner to pcio1, so should we */
		if (pNv->NVArch == 0x11) {
			NVWriteVGA0(pNv, NV_VGA_CRTCX_OWNER, 0xff);
		}
		NVWriteVGA0(pNv, NV_VGA_CRTCX_OWNER, nv_crtc->crtc * 0x3);
		owner = NVReadVGA0(pNv, NV_VGA_CRTCX_OWNER);
		ErrorF("post-Owner: 0x%X\n", owner);
	} else {
		ErrorF("pNv pointer is NULL\n");
	}
}

static void
NVEnablePalette(xf86CrtcPtr crtc)
{
  ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pCRTCReg = nv_crtc->head ? pNv->PCIO1 : pNv->PCIO0;

  NV_RD08(pCRTCReg, CRTC_IN_STAT_1);
  NV_WR08(pCRTCReg, VGA_ATTR_INDEX, 0);
  nv_crtc->paletteEnabled = TRUE;
}

static void
NVDisablePalette(xf86CrtcPtr crtc)
{
  ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pCRTCReg = nv_crtc->head ? pNv->PCIO1 : pNv->PCIO0;

  NV_RD08(pCRTCReg, CRTC_IN_STAT_1);
  NV_WR08(pCRTCReg, VGA_ATTR_INDEX, 0x20);
  nv_crtc->paletteEnabled = FALSE;
}

static void NVWriteVgaReg(xf86CrtcPtr crtc, CARD32 reg, CARD8 value)
{
 ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pCRTCReg = nv_crtc->head ? pNv->PCIO1 : pNv->PCIO0;

  NV_WR08(pCRTCReg, reg, value);
}

/* perform a sequencer reset */
static void NVVgaSeqReset(xf86CrtcPtr crtc, Bool start)
{
  if (start)
    NVWriteVgaSeq(crtc, 0x00, 0x1);
  else
    NVWriteVgaSeq(crtc, 0x00, 0x3);

}
static void NVVgaProtect(xf86CrtcPtr crtc, Bool on)
{
	CARD8 tmp;

	if (on) {
		tmp = NVReadVgaSeq(crtc, 0x1);
		NVVgaSeqReset(crtc, TRUE);
		NVWriteVgaSeq(crtc, 0x01, tmp | 0x20);

		NVEnablePalette(crtc);
	} else {
		/*
		 * Reenable sequencer, then turn on screen.
		 */
		tmp = NVReadVgaSeq(crtc, 0x1);
		NVWriteVgaSeq(crtc, 0x01, tmp & ~0x20);	/* reenable display */
		NVVgaSeqReset(crtc, FALSE);

		NVDisablePalette(crtc);
	}
}

void NVCrtcLockUnlock(xf86CrtcPtr crtc, Bool Lock)
{
	CARD8 cr11;

	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_LOCK, Lock ? 0x99 : 0x57);
	cr11 = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_VSYNCE);
	if (Lock) cr11 |= 0x80;
	else cr11 &= ~0x80;
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_VSYNCE, cr11);
}

xf86OutputPtr 
NVGetOutputFromCRTC(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;
	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if (output->crtc == crtc) {
			return output;
		}
	}

	return NULL;
}

xf86CrtcPtr
nv_find_crtc_by_index(ScrnInfoPtr pScrn, int index)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
		if (nv_crtc->crtc == index)
			return crtc;
	}

	return NULL;
}

/*
 * Calculate the Video Clock parameters for the PLL.
 */
static void CalcVClock (
	uint32_t		clockIn,
	uint32_t		*clockOut,
	CARD32		*pllOut,
	NVPtr		pNv
)
{
	unsigned lowM, highM, highP;
	unsigned DeltaNew, DeltaOld;
	unsigned VClk, Freq;
	unsigned M, N, P;

	/* M: PLL reference frequency postscaler divider */
	/* P: PLL VCO output postscaler divider */
	/* N: PLL VCO postscaler setting */

	DeltaOld = 0xFFFFFFFF;

	VClk = (unsigned)clockIn;

	/* Taken from Haiku, after someone with an NV28 had an issue */
	switch(pNv->NVArch) {
		case 0x28:
			lowM = 1;
			highP = 32;
			if (VClk > 340000) {
				highM = 2;
			} else if (VClk > 200000) {
				highM = 4;
			} else if (VClk > 150000) {
				highM = 6;
			} else {
				highM = 14;
			}
			break;
		default:
			lowM = 1;
			highP = 16;
			if (VClk > 340000) {
				highM = 2;
			} else if (VClk > 250000) {
				highM = 6;
			} else {
				highM = 14;
			}
			break;
	}

	for (P = 1; P <= highP; P++) {
		Freq = VClk << P;
		if ((Freq >= 128000) && (Freq <= 350000)) {
			for (M = lowM; M <= highM; M++) {
				N = ((VClk << P) * M) / pNv->CrystalFreqKHz;
				if (N <= 255) {
					Freq = ((pNv->CrystalFreqKHz * N) / M) >> P;
					if (Freq > VClk) {
						DeltaNew = Freq - VClk;
					} else {
						DeltaNew = VClk - Freq;
					}
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
	uint32_t		clockIn,
	uint32_t		*clockOut,
	CARD32		*pllOut,
	CARD32		*pllBOut,
	NVPtr		pNv
)
{
	unsigned DeltaNew, DeltaOld;
	unsigned VClk, Freq;
	unsigned M, N, P;
	unsigned lowM, highM, highP;

	DeltaOld = 0xFFFFFFFF;

	*pllBOut = 0x80000401;  /* fixed at x4 for now */

	VClk = (unsigned)clockIn;

	/* Taken from Haiku, after someone with an NV28 had an issue */
	switch(pNv->NVArch) {
		case 0x28:
			lowM = 1;
			highP = 32;
			if (VClk > 340000) {
				highM = 2;
			} else if (VClk > 200000) {
				highM = 4;
			} else if (VClk > 150000) {
				highM = 6;
			} else {
				highM = 14;
			}
			break;
		default:
			lowM = 1;
			highP = 15;
			if (VClk > 340000) {
				highM = 2;
			} else if (VClk > 250000) {
				highM = 6;
			} else {
				highM = 14;
			}
			break;
	}

	for (P = 0; P <= highP; P++) {
		Freq = VClk << P;
		if ((Freq >= 400000) && (Freq <= 1000000)) {
			for (M = lowM; M <= highM; M++) {
				N = ((VClk << P) * M) / (pNv->CrystalFreqKHz << 2);
				if ((N >= 5) && (N <= 255)) {
					Freq = (((pNv->CrystalFreqKHz << 2) * N) / M) >> P;
					if (Freq > VClk) {
						DeltaNew = Freq - VClk;
					} else {
						DeltaNew = VClk - Freq;
					}
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

/* BIG NOTE: modifying vpll1 and vpll2vpll2 does not work, what bit is the switch to allow it? */

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

static void
CalculateVClkNV4x(
	NVPtr pNv,
	uint32_t requested_clock,
	uint32_t *given_clock,
	uint32_t *pll_a,
	uint32_t *pll_b,
	uint32_t *reg580,
	Bool	*db1_ratio,
	Bool primary
)
{
	uint32_t DeltaOld, DeltaNew;
	uint32_t freq, temp;
	/* We have 2 mulitpliers, 2 dividers and one post divider */
	/* Note that p is only 4 bits */
	uint32_t m1, m2, n1, n2, p;
	uint32_t m1_best = 0, m2_best = 0, n1_best = 0, n2_best = 0, p_best = 0;

	DeltaOld = 0xFFFFFFFF;

	/* This is no solid limit, but a reasonable boundary */
	if (requested_clock < 120000) {
		*db1_ratio = TRUE;
		/* Turn the second set of divider and multiplier off */
		/* Neutral settings */
		n2 = 1;
		m2 = 1;
	} else {
		*db1_ratio = FALSE;
		/* Fixed at x4 for the moment */
		n2 = 4;
		m2 = 1;
	}

	n2_best = n2;
	m2_best = m2;

	/* Single pll */
	if (*db1_ratio) {
		temp = 0.4975 * 250000;
		p = 0;

		while (requested_clock <= temp) {
			temp /= 2;
			p++;
		}

		/* The minimum clock after m1 is 3 Mhz, and the clock is 27 Mhz, so m_max = 9 */
		/* The maximum clock is 25 Mhz */
		for (m1 = 2; m1 <= 9; m1++) {
			n1 = ((requested_clock << p) * m1)/(pNv->CrystalFreqKHz);
			if (n1 > 0 && n1 <= 255) {
				freq = ((pNv->CrystalFreqKHz * n1)/m1) >> p;
				if (freq > requested_clock) {
					DeltaNew = freq - requested_clock;
				} else {
					DeltaNew = requested_clock - freq;
				}
				if (DeltaNew < DeltaOld) {
					m1_best = m1;
					n1_best = n1;
					p_best = p;
					DeltaOld = DeltaNew;
				}
			}
		}
	/* Dual pll */
	} else {
		for (p = 0; p <= 6; p++) {
			/* Assuming a fixed 2nd stage */
			freq = requested_clock << p;
			/* The maximum output frequency of stage 2 is allowed to be between 400 Mhz and 1 GHz */
			if (freq > 400000 && freq < 1000000) {
				/* The minimum clock after m1 is 3 Mhz, and the clock is 27 Mhz, so m_max = 9 */
				/* The maximum clock is 25 Mhz */
				for (m1 = 2; m1 <= 9; m1++) {
					n1 = ((requested_clock << p) * m1 * m2)/(pNv->CrystalFreqKHz * n2);
					if (n1 >= 5 && n1 <= 255) {
						freq = ((pNv->CrystalFreqKHz * n1 * n2)/(m1 * m2)) >> p;
						if (freq > requested_clock) {
							DeltaNew = freq - requested_clock;
						} else {
							DeltaNew = requested_clock - freq;
						}
						if (DeltaNew < DeltaOld) {
							m1_best = m1;
							n1_best = n1;
							p_best = p;
							DeltaOld = DeltaNew;
						}
					}
				}
			}
		}
	}

	if (*db1_ratio) {
		/* Bogus data, the same nvidia uses */
		n2_best = 1;
		m2_best = 31;
	}

	/* What exactly are the purpose of the upper 2 bits of pll_a and pll_b? */
	*pll_a = (p_best << 16) | (n1_best << 8) | (m1_best << 0);
	*pll_b = (1 << 31) | (n2_best << 8) | (m2_best << 0);

	if (*db1_ratio) {
		if (primary) {
			*reg580 |= NV_RAMDAC_580_VPLL1_ACTIVE;
		} else {
			*reg580 |= NV_RAMDAC_580_VPLL2_ACTIVE;
		}
	} else {
		if (primary) {
			*reg580 &= ~NV_RAMDAC_580_VPLL1_ACTIVE;
		} else {
			*reg580 &= ~NV_RAMDAC_580_VPLL2_ACTIVE;
		}
	}

	if (*db1_ratio) {
		ErrorF("vpll: n1 %d m1 %d p %d db1_ratio %d\n", n1_best, m1_best, p_best, *db1_ratio);
	} else {
		ErrorF("vpll: n1 %d n2 %d m1 %d m2 %d p %d db1_ratio %d\n", n1_best, n2_best, m1_best, m2_best, p_best, *db1_ratio);
	}
}

static void nv40_crtc_save_state_pll(NVPtr pNv, RIVA_HW_STATE *state)
{
	state->vpll1_a = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL);
	state->vpll1_b = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL_B);
	state->vpll2_a = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL2);
	state->vpll2_b = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL2_B);
	state->pllsel = nvReadRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT);
	state->sel_clk = nvReadRAMDAC0(pNv, NV_RAMDAC_SEL_CLK);
	state->reg580 = nvReadRAMDAC0(pNv, NV_RAMDAC_580);
	state->reg594 = nvReadRAMDAC0(pNv, NV_RAMDAC_594);
}

static void nv40_crtc_load_state_pll(NVPtr pNv, RIVA_HW_STATE *state)
{
	CARD32 fp_debug_0[2];
	uint32_t index[2];
	fp_debug_0[0] = nvReadRAMDAC(pNv, 0, NV_RAMDAC_FP_DEBUG_0);
	fp_debug_0[1] = nvReadRAMDAC(pNv, 1, NV_RAMDAC_FP_DEBUG_0);

	/* The TMDS_PLL switch is on the actual ramdac */
	if (state->crosswired) {
		index[0] = 1;
		index[1] = 0;
		ErrorF("Crosswired pll state load\n");
	} else {
		index[0] = 0;
		index[1] = 1;
	}

	if (state->vpll2_b) {
		nvWriteRAMDAC(pNv, index[1], NV_RAMDAC_FP_DEBUG_0,
			fp_debug_0[index[1]] | NV_RAMDAC_FP_DEBUG_0_PWRDOWN_TMDS_PLL);

		/* Wait for the situation to stabilise */
		usleep(5000);

		uint32_t reg_c040 = pNv->misc_info.reg_c040;
		/* for vpll2 change bits 18 and 19 are disabled */
		reg_c040 &= ~(0x3 << 18);
		nvWriteMC(pNv, 0xc040, reg_c040);

		ErrorF("writing vpll2_a %08X\n", state->vpll2_a);
		ErrorF("writing vpll2_b %08X\n", state->vpll2_b);

		nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL2, state->vpll2_a);
		nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL2_B, state->vpll2_b);

		ErrorF("writing pllsel %08X\n", state->pllsel & ~NV_RAMDAC_PLL_SELECT_PLL_SOURCE_ALL);
		/* Let's keep the primary vpll off */
		nvWriteRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT, state->pllsel & ~NV_RAMDAC_PLL_SELECT_PLL_SOURCE_ALL);

		nvWriteRAMDAC0(pNv, NV_RAMDAC_580, state->reg580);
		ErrorF("writing reg580 %08X\n", state->reg580);

		/* We need to wait a while */
		usleep(5000);
		nvWriteMC(pNv, 0xc040, pNv->misc_info.reg_c040);

		nvWriteRAMDAC(pNv, index[1], NV_RAMDAC_FP_DEBUG_0, fp_debug_0[index[1]]);

		/* Wait for the situation to stabilise */
		usleep(5000);
	}

	if (state->vpll1_b) {
		nvWriteRAMDAC(pNv, index[0], NV_RAMDAC_FP_DEBUG_0,
			fp_debug_0[index[0]] | NV_RAMDAC_FP_DEBUG_0_PWRDOWN_TMDS_PLL);

		/* Wait for the situation to stabilise */
		usleep(5000);

		uint32_t reg_c040 = pNv->misc_info.reg_c040;
		/* for vpll2 change bits 16 and 17 are disabled */
		reg_c040 &= ~(0x3 << 16);
		nvWriteMC(pNv, 0xc040, reg_c040);

		ErrorF("writing vpll1_a %08X\n", state->vpll1_a);
		ErrorF("writing vpll1_b %08X\n", state->vpll1_b);

		nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL, state->vpll1_a);
		nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL_B, state->vpll1_b);

		ErrorF("writing pllsel %08X\n", state->pllsel);
		nvWriteRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT, state->pllsel);

		nvWriteRAMDAC0(pNv, NV_RAMDAC_580, state->reg580);
		ErrorF("writing reg580 %08X\n", state->reg580);

		/* We need to wait a while */
		usleep(5000);
		nvWriteMC(pNv, 0xc040, pNv->misc_info.reg_c040);

		nvWriteRAMDAC(pNv, index[0], NV_RAMDAC_FP_DEBUG_0, fp_debug_0[index[0]]);

		/* Wait for the situation to stabilise */
		usleep(5000);
	}

	ErrorF("writing sel_clk %08X\n", state->sel_clk);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_SEL_CLK, state->sel_clk);

	ErrorF("writing reg594 %08X\n", state->reg594);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_594, state->reg594);
}

static void nv_crtc_save_state_pll(NVPtr pNv, RIVA_HW_STATE *state)
{
	state->vpll = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL);
	if(pNv->twoHeads) {
		state->vpll2 = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL2);
	}
	if(pNv->twoStagePLL) {
		state->vpllB = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL_B);
		state->vpll2B = nvReadRAMDAC0(pNv, NV_RAMDAC_VPLL2_B);
	}
	state->pllsel = nvReadRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT);
	state->sel_clk = nvReadRAMDAC0(pNv, NV_RAMDAC_SEL_CLK);
}


static void nv_crtc_load_state_pll(NVPtr pNv, RIVA_HW_STATE *state)
{
	if (state->vpll2) {
		if(pNv->twoHeads) {
			ErrorF("writing vpll2 %08X\n", state->vpll2);
			nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL2, state->vpll2);
		}
		if(pNv->twoStagePLL) {
			ErrorF("writing vpll2B %08X\n", state->vpll2B);
			nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL2_B, state->vpll2B);
		}

		ErrorF("writing pllsel %08X\n", state->pllsel);
		/* Let's keep the primary vpll off */
		nvWriteRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT, state->pllsel & ~NV_RAMDAC_PLL_SELECT_PLL_SOURCE_ALL);
	}

	if (state->vpll) {
		ErrorF("writing vpll %08X\n", state->vpll);
		nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL, state->vpll);
		if(pNv->twoStagePLL) {
			ErrorF("writing vpllB %08X\n", state->vpllB);
			nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL_B, state->vpllB);
		}

		ErrorF("writing pllsel %08X\n", state->pllsel);
		nvWriteRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT, state->pllsel);
	}

	ErrorF("writing sel_clk %08X\n", state->sel_clk);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_SEL_CLK, state->sel_clk);
}

#define IS_NV44P (pNv->NVArch >= 0x44 ? 1 : 0)

/*
 * Calculate extended mode parameters (SVGA) and save in a 
 * mode state structure.
 * State is not specific to a single crtc, but shared.
 */
void nv_crtc_calc_state_ext(
	xf86CrtcPtr 	crtc,
	int			bpp,
	int			DisplayWidth, /* Does this change after setting the mode? */
	int			CrtcHDisplay,
	int			CrtcVDisplay,
	int			dotClock,
	int			flags 
)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	uint32_t pixelDepth, VClk = 0;
	CARD32 CursorStart;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	NVCrtcRegPtr regp;
	NVPtr pNv = NVPTR(pScrn);    
	RIVA_HW_STATE *state;
	int num_crtc_enabled, i;

	state = &pNv->ModeReg;

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	xf86OutputPtr output = NVGetOutputFromCRTC(crtc);
	NVOutputPrivatePtr nv_output = NULL;
	if (output) {
		nv_output = output->driver_private;
	}

	/*
	 * Extended RIVA registers.
	 */
	pixelDepth = (bpp + 1)/8;
	if (pNv->Architecture == NV_ARCH_40) {
		/* Does register 0x580 already have a value? */
		if (!state->reg580) {
			state->reg580 = pNv->misc_info.ramdac_0_reg_580;
		}
		if (nv_crtc->head == 1) {
			CalculateVClkNV4x(pNv, dotClock, &VClk, &state->vpll2_a, &state->vpll2_b, &state->reg580, &state->db1_ratio[1], FALSE);
		} else {
			CalculateVClkNV4x(pNv, dotClock, &VClk, &state->vpll1_a, &state->vpll1_b, &state->reg580, &state->db1_ratio[0], TRUE);
		}
	} else if (pNv->twoStagePLL) {
		CalcVClock2Stage(dotClock, &VClk, &state->pll, &state->pllB, pNv);
	} else {
		CalcVClock(dotClock, &VClk, &state->pll, pNv);
	}

	switch (pNv->Architecture) {
	case NV_ARCH_04:
		nv4UpdateArbitrationSettings(VClk, 
						pixelDepth * 8, 
						&(state->arbitration0),
						&(state->arbitration1),
						pNv);
		regp->CRTC[NV_VGA_CRTCX_CURCTL0] = 0x00;
		regp->CRTC[NV_VGA_CRTCX_CURCTL1] = 0xbC;
		if (flags & V_DBLSCAN)
			regp->CRTC[NV_VGA_CRTCX_CURCTL1] |= 2;
		regp->CRTC[NV_VGA_CRTCX_CURCTL2] = 0x00000000;
		state->pllsel   |= NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2 | NV_RAMDAC_PLL_SELECT_PLL_SOURCE_ALL; 
		state->config   = 0x00001114;
		regp->CRTC[NV_VGA_CRTCX_REPAINT1] = CrtcHDisplay < 1280 ? 0x04 : 0x00;
		break;
	case NV_ARCH_10:
	case NV_ARCH_20:
	case NV_ARCH_30:
	default:
		if (((pNv->Chipset & 0xfff0) == CHIPSET_C51) ||
			((pNv->Chipset & 0xfff0) == CHIPSET_C512)) {
			state->arbitration0 = 128; 
			state->arbitration1 = 0x0480; 
		} else if (((pNv->Chipset & 0xffff) == CHIPSET_NFORCE) ||
			((pNv->Chipset & 0xffff) == CHIPSET_NFORCE2)) {
			nForceUpdateArbitrationSettings(VClk,
						pixelDepth * 8,
						&(state->arbitration0),
						&(state->arbitration1),
						pNv);
		} else if (pNv->Architecture < NV_ARCH_30) {
			nv10UpdateArbitrationSettings(VClk, 
						pixelDepth * 8, 
						&(state->arbitration0),
						&(state->arbitration1),
						pNv);
		} else {
			nv30UpdateArbitrationSettings(pNv,
						&(state->arbitration0),
						&(state->arbitration1));
		}

		CursorStart = pNv->Cursor->offset;

		regp->CRTC[NV_VGA_CRTCX_CURCTL0] = 0x80 | (CursorStart >> 17);
		regp->CRTC[NV_VGA_CRTCX_CURCTL1] = (CursorStart >> 11) << 2;
		regp->CRTC[NV_VGA_CRTCX_CURCTL2] = CursorStart >> 24;

		if (flags & V_DBLSCAN) 
			regp->CRTC[NV_VGA_CRTCX_CURCTL1] |= 2;

		state->config   = nvReadFB(pNv, NV_PFB_CFG0);
		regp->CRTC[NV_VGA_CRTCX_REPAINT1] = CrtcHDisplay < 1280 ? 0x04 : 0x00;
		break;
	}

	/* okay do we have 2 CRTCs running ? */
	num_crtc_enabled = 0;
	for (i = 0; i < xf86_config->num_crtc; i++) {
		if (xf86_config->crtc[i]->enabled) {
			num_crtc_enabled++;
		}
	}

	ErrorF("There are %d CRTC's enabled\n", num_crtc_enabled);

	if (pNv->Architecture < NV_ARCH_40) {
		/* We need this before the next code */
		if (nv_crtc->head == 1) {
			state->vpll2 = state->pll;
			state->vpll2B = state->pllB;
		} else {
			state->vpll = state->pll;
			state->vpllB = state->pllB;
		}
	}

	if (pNv->Architecture == NV_ARCH_40) {
		/* This register is only used on the primary ramdac */
		/* This seems to be needed to select the proper clocks, otherwise bad things happen */

		if (!state->sel_clk)
			state->sel_clk = pNv->misc_info.sel_clk & ~(0xfff << 8);

		/* There are a few possibilities:
		 * Early NV4x cards: 0x41000 for example
		 * Later NV4x cards: 0x40100 for example
		 * The lower entry is the first bus, the higher entry is the second bus
		 * 0: No dvi present
		 * 1: Primary clock
		 * 2: Unknown, similar to 4?
		 * 4: Secondary clock
		 */

		/* This won't work when tv-out's come into play */
		state->sel_clk &= ~(0xf << (16 - 4 * !nv_output->preferred_ramdac * (1 + IS_NV44P)));
		if (output && (nv_output->type == OUTPUT_TMDS || nv_output->type == OUTPUT_LVDS)) {
			if (nv_crtc->head == 1) { /* clock */
				state->sel_clk |= 0x4 << (16 - 4 * !nv_output->preferred_ramdac * (1 + IS_NV44P));
			} else {
				state->sel_clk |= 0x1 << (16 - 4 * !nv_output->preferred_ramdac * (1 + IS_NV44P));
			}
		}

		/* The hardware gets upset if for example 0x00100 is set instead of 0x40100 */
		if ((state->sel_clk & (0xff << 8)) && !(state->sel_clk & (0xf << 16))) {
			if ((state->sel_clk & (0xf << (12 -  4*IS_NV44P))) == (0x1 << 16)) {
				state->sel_clk |= (0x4 << 16);
			} else {
				state->sel_clk |= (0x1 << 16);
			}
		}

		/* Are we crosswired? */
		if (output && nv_crtc->head != nv_output->preferred_ramdac && 
			(nv_output->type == OUTPUT_TMDS || nv_output->type == OUTPUT_LVDS)) {
			state->crosswired = TRUE;
		} else if (output && nv_crtc->head != nv_output->preferred_ramdac) {
			state->crosswired = FALSE;
		} else {
			state->crosswired = FALSE;
		}

		if (nv_crtc->head == 1) {
			if (state->db1_ratio[1])
				ErrorF("We are a lover of the DB1 VCLK ratio\n");
		} else if (nv_crtc->head == 0) {
			if (state->db1_ratio[0])
				ErrorF("We are a lover of the DB1 VCLK ratio\n");
		}
	} else {
		/* This seems true for nv34 */
		state->sel_clk = 0x0;
		state->crosswired = FALSE;
	}

	if (nv_crtc->head == 1) {
		if (!state->db1_ratio[1]) {
			state->pllsel |= NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2;
		} else {
			state->pllsel &= ~NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2;
		}
		state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL2;
	} else {
		if (pNv->Architecture < NV_ARCH_40)
			state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_ALL;
		else
			state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL;
		if (!state->db1_ratio[0]) {
			state->pllsel |= NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2;
		} else {
			state->pllsel &= ~NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2;
		}
	}

	/* The blob uses this always, so let's do the same */
	if (pNv->Architecture == NV_ARCH_40) {
		state->pllsel |= NV_RAMDAC_PLL_SELECT_USE_VPLL2_TRUE;
	}

	/* The primary output doesn't seem to care */
	if (nv_output->preferred_ramdac == 1) { /* This is the "output" */
		/* non-zero values are for analog, don't know about tv-out and the likes */
		if (output && nv_output->type != OUTPUT_ANALOG) {
			state->reg594 = 0x0;
		} else {
			/* More values exist, but they seem related to the 3rd dac (tv-out?) somehow */
			/* bit 16-19 are bits that are set on some G70 cards */
			/* Those bits are also set to the 3rd OUTPUT register */
			if (nv_crtc->head == 1) {
				state->reg594 = 0x101;
			} else {
				state->reg594 = 0x1;
			}
		}
	}

	regp->CRTC[NV_VGA_CRTCX_FIFO0] = state->arbitration0;
	regp->CRTC[NV_VGA_CRTCX_FIFO_LWM] = state->arbitration1 & 0xff;
	if (pNv->Architecture >= NV_ARCH_30) {
		regp->CRTC[NV_VGA_CRTCX_FIFO_LWM_NV30] = state->arbitration1 >> 8;
	}

	regp->CRTC[NV_VGA_CRTCX_REPAINT0] = (((DisplayWidth/8) * pixelDepth) & 0x700) >> 3;
	regp->CRTC[NV_VGA_CRTCX_PIXEL] = (pixelDepth > 2) ? 3 : pixelDepth;
}

static void
nv_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	unsigned char seq1 = 0, crtc17 = 0;
	unsigned char crtc1A;

	ErrorF("nv_crtc_dpms is called for CRTC %d with mode %d\n", nv_crtc->crtc, mode);

	NVCrtcSetOwner(crtc);

	crtc1A = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_REPAINT1) & ~0xC0;
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

	NVVgaSeqReset(crtc, TRUE);
	/* Each head has it's own sequencer, so we can turn it off when we want */
	seq1 |= (NVReadVgaSeq(crtc, 0x01) & ~0x20);
	NVWriteVgaSeq(crtc, 0x1, seq1);
	crtc17 |= (NVReadVgaCrtc(crtc, NV_VGA_CRTCX_MODECTL) & ~0x80);
	usleep(10000);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_MODECTL, crtc17);
	NVVgaSeqReset(crtc, FALSE);

	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_REPAINT1, crtc1A);

	/* I hope this is the right place */
	if (crtc->enabled && mode == DPMSModeOn) {
		pNv->crtc_active[nv_crtc->head] = TRUE;
	} else {
		pNv->crtc_active[nv_crtc->head] = FALSE;
	}
}

static Bool
nv_crtc_mode_fixup(xf86CrtcPtr crtc, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ErrorF("nv_crtc_mode_fixup is called for CRTC %d\n", nv_crtc->crtc);

	xf86OutputPtr output = NVGetOutputFromCRTC(crtc);
	NVOutputPrivatePtr nv_output = NULL;
	if (output) {
		nv_output = output->driver_private;
	}

	/* For internal panels and gpu scaling on DVI we need the native mode */
	if (output && ((nv_output->type == OUTPUT_LVDS) || (nv_output->scaling_mode > 0 && (nv_output->type == OUTPUT_TMDS)))) {
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

static void
nv_crtc_mode_set_vga(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVCrtcRegPtr regp;
	NVPtr pNv = NVPTR(pScrn);
	NVFBLayout *pLayout = &pNv->CurrentLayout;
	int depth = pScrn->depth;

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

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

	Bool is_fp = FALSE;

	xf86OutputPtr output = NVGetOutputFromCRTC(crtc);
	NVOutputPrivatePtr nv_output = NULL;
	if (output) {
		nv_output = output->driver_private;

		if ((nv_output->type == OUTPUT_LVDS) || (nv_output->type == OUTPUT_TMDS))
			is_fp = TRUE;
	}

	ErrorF("Mode clock: %d\n", mode->Clock);
	ErrorF("Adjusted mode clock: %d\n", adjusted_mode->Clock);

	/* Reverted to what nv did, because that works for all resolutions on flatpanels */
	if (is_fp) {
		vertStart = vertTotal - 3;  
		vertEnd = vertTotal - 2;
		vertBlankStart = vertStart;
		horizStart = horizTotal - 5;
		horizEnd = horizTotal - 2;   
		horizBlankEnd = horizTotal + 4;   
		if (pNv->overlayAdaptor) { 
			/* This reportedly works around Xv some overlay bandwidth problems*/
			horizTotal += 2;
		}
	}

	if(mode->Flags & V_INTERLACE) 
		vertTotal |= 1;

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
		if (VDisplay < 400) {
			regp->MiscOutReg = 0xA3;		/* +hsync -vsync */
		} else if (VDisplay < 480) {
			regp->MiscOutReg = 0x63;		/* -hsync +vsync */
		} else if (VDisplay < 768) {
			regp->MiscOutReg = 0xE3;		/* -hsync -vsync */
		} else {
			regp->MiscOutReg = 0x23;		/* +hsync +vsync */
		}
	}

	regp->MiscOutReg |= (mode->ClockIndex & 0x03) << 2;

	/*
	* Time Sequencer
	*/
	if (depth == 4) {
		regp->Sequencer[0] = 0x02;
	} else {
		regp->Sequencer[0] = 0x00;
	}
	/* 0x20 disables the sequencer */
	if (mode->Flags & V_CLKDIV2) {
		regp->Sequencer[1] = 0x29;
	} else {
		regp->Sequencer[1] = 0x21;
	}
	if (depth == 1) {
		regp->Sequencer[2] = 1 << BIT_PLANE;
	} else {
		regp->Sequencer[2] = 0x0F;
		regp->Sequencer[3] = 0x00;                     /* Font select */
	}
	if (depth < 8) {
		regp->Sequencer[4] = 0x06;                             /* Misc */
	} else {
		regp->Sequencer[4] = 0x0E;                             /* Misc */
	}

	/*
	* CRTC Controller
	*/
	regp->CRTC[NV_VGA_CRTCX_HTOTAL]  = Set8Bits(horizTotal);
	regp->CRTC[NV_VGA_CRTCX_HDISPE]  = Set8Bits(horizDisplay);
	regp->CRTC[NV_VGA_CRTCX_HBLANKS]  = Set8Bits(horizBlankStart);
	regp->CRTC[NV_VGA_CRTCX_HBLANKE]  = SetBitField(horizBlankEnd,4:0,4:0) 
				| SetBit(7);
	regp->CRTC[NV_VGA_CRTCX_HSYNCS]  = Set8Bits(horizStart);
	regp->CRTC[NV_VGA_CRTCX_HSYNCE]  = SetBitField(horizBlankEnd,5:5,7:7)
				| SetBitField(horizEnd,4:0,4:0);
	regp->CRTC[NV_VGA_CRTCX_VTOTAL]  = SetBitField(vertTotal,7:0,7:0);
	regp->CRTC[NV_VGA_CRTCX_OVERFLOW]  = SetBitField(vertTotal,8:8,0:0)
				| SetBitField(vertDisplay,8:8,1:1)
				| SetBitField(vertStart,8:8,2:2)
				| SetBitField(vertBlankStart,8:8,3:3)
				| SetBit(4)
				| SetBitField(vertTotal,9:9,5:5)
				| SetBitField(vertDisplay,9:9,6:6)
				| SetBitField(vertStart,9:9,7:7);
	regp->CRTC[NV_VGA_CRTCX_PRROWSCN]  = 0x00;
	regp->CRTC[NV_VGA_CRTCX_MAXSCLIN]  = SetBitField(vertBlankStart,9:9,5:5)
				| SetBit(6)
				| ((mode->Flags & V_DBLSCAN) ? 0x80 : 0x00);
	regp->CRTC[NV_VGA_CRTCX_VGACURCTRL] = 0x00;
	regp->CRTC[0xb] = 0x00;
	regp->CRTC[NV_VGA_CRTCX_FBSTADDH] = 0x00;
	regp->CRTC[NV_VGA_CRTCX_FBSTADDL] = 0x00;
	regp->CRTC[0xe] = 0x00;
	regp->CRTC[0xf] = 0x00;
	regp->CRTC[NV_VGA_CRTCX_VSYNCS] = Set8Bits(vertStart);
	regp->CRTC[NV_VGA_CRTCX_VSYNCE] = SetBitField(vertEnd,3:0,3:0) | SetBit(5);
	regp->CRTC[NV_VGA_CRTCX_VDISPE] = Set8Bits(vertDisplay);
	regp->CRTC[0x14] = 0x00;
	regp->CRTC[NV_VGA_CRTCX_PITCHL] = ((pScrn->displayWidth/8)*(pLayout->bitsPerPixel/8));
	regp->CRTC[NV_VGA_CRTCX_VBLANKS] = Set8Bits(vertBlankStart);
	regp->CRTC[NV_VGA_CRTCX_VBLANKE] = Set8Bits(vertBlankEnd);
	/* 0x80 enables the sequencer, we don't want that */
	if (depth < 8) {
		regp->CRTC[NV_VGA_CRTCX_MODECTL] = 0xE3 & ~0x80;
	} else {
		regp->CRTC[NV_VGA_CRTCX_MODECTL] = 0xC3 & ~0x80;
	}
	regp->CRTC[NV_VGA_CRTCX_LINECOMP] = 0xff;

	/* 
	 * Some extended CRTC registers (they are not saved with the rest of the vga regs).
	 */

	regp->CRTC[NV_VGA_CRTCX_LSR] = SetBitField(horizBlankEnd,6:6,4:4)
				| SetBitField(vertBlankStart,10:10,3:3)
				| SetBitField(vertStart,10:10,2:2)
				| SetBitField(vertDisplay,10:10,1:1)
				| SetBitField(vertTotal,10:10,0:0);

	regp->CRTC[NV_VGA_CRTCX_HEB] = SetBitField(horizTotal,8:8,0:0) 
				| SetBitField(horizDisplay,8:8,1:1)
				| SetBitField(horizBlankStart,8:8,2:2)
				| SetBitField(horizStart,8:8,3:3);

	regp->CRTC[NV_VGA_CRTCX_EXTRA] = SetBitField(vertTotal,11:11,0:0)
				| SetBitField(vertDisplay,11:11,2:2)
				| SetBitField(vertStart,11:11,4:4)
				| SetBitField(vertBlankStart,11:11,6:6);

	if(mode->Flags & V_INTERLACE) {
		horizTotal = (horizTotal >> 1) & ~1;
		regp->CRTC[NV_VGA_CRTCX_INTERLACE] = Set8Bits(horizTotal);
		regp->CRTC[NV_VGA_CRTCX_HEB] |= SetBitField(horizTotal,8:8,4:4);
	} else {
		regp->CRTC[NV_VGA_CRTCX_INTERLACE] = 0xff;  /* interlace off */
	}

	/*
	* Theory resumes here....
	*/

	/*
	* Graphics Display Controller
	*/
	regp->Graphics[0] = 0x00;
	regp->Graphics[1] = 0x00;
	regp->Graphics[2] = 0x00;
	regp->Graphics[3] = 0x00;
	if (depth == 1) {
		regp->Graphics[4] = BIT_PLANE;
		regp->Graphics[5] = 0x00;
	} else {
		regp->Graphics[4] = 0x00;
		if (depth == 4) {
			regp->Graphics[5] = 0x02;
		} else {
			regp->Graphics[5] = 0x40;
		}
	}
	regp->Graphics[6] = 0x05;   /* only map 64k VGA memory !!!! */
	regp->Graphics[7] = 0x0F;
	regp->Graphics[8] = 0xFF;

	/* I ditched the mono stuff */
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
	/* These two below are non-vga */
	regp->Attribute[16] = 0x01;
	regp->Attribute[17] = 0x00;
	regp->Attribute[18] = 0x0F;
	regp->Attribute[19] = 0x00;
	regp->Attribute[20] = 0x00;
}

#define MAX_H_VALUE(i) ((0x1ff + i) << 3)
#define MAX_V_VALUE(i) ((0xfff + i) << 0)

/**
 * Sets up registers for the given mode/adjusted_mode pair.
 *
 * The clocks, CRTCs and outputs attached to this CRTC must be off.
 *
 * This shouldn't enable any clocks, CRTCs, or outputs, but they should
 * be easily turned on/off after this.
 */
static void
nv_crtc_mode_set_regs(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVFBLayout *pLayout = &pNv->CurrentLayout;
	NVCrtcRegPtr regp, savep;
	unsigned int i;
	Bool is_fp = FALSE;

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];    
	savep = &pNv->SavedReg.crtc_reg[nv_crtc->head];

	xf86OutputPtr output = NVGetOutputFromCRTC(crtc);
	NVOutputPrivatePtr nv_output = NULL;
	if (output) {
		nv_output = output->driver_private;

		if ((nv_output->type == OUTPUT_LVDS) || (nv_output->type == OUTPUT_TMDS))
			is_fp = TRUE;
	}

	/* Registers not directly related to the (s)vga mode */

	/* bit2 = 0 -> fine pitched crtc granularity */
	/* The rest disables double buffering on CRTC access */
	regp->CRTC[NV_VGA_CRTCX_BUFFER] = 0xfa;

	if (savep->CRTC[NV_VGA_CRTCX_LCD] <= 0xb) {
		/* Common values are 0x0, 0x3, 0x8, 0xb, see logic below */
		if (nv_crtc->head == 0) {
			regp->CRTC[NV_VGA_CRTCX_LCD] = (1 << 3);
		}

		if (is_fp) {
			regp->CRTC[NV_VGA_CRTCX_LCD] |= (1 << 0) | (1 << 1);
		}
	} else {
		/* Let's keep any abnormal value there may be, like 0x54 or 0x79 */
		regp->CRTC[NV_VGA_CRTCX_LCD] = savep->CRTC[NV_VGA_CRTCX_LCD];
	}

	/* Sometimes 0x10 is used, what is this? */
	regp->CRTC[NV_VGA_CRTCX_59] = 0x0;
	/* Some kind of tmds switch for older cards */
	if (pNv->Architecture < NV_ARCH_40) {
		regp->CRTC[NV_VGA_CRTCX_59] |= 0x1;
	}

	/*
	* Initialize DAC palette.
	*/
	if(pLayout->bitsPerPixel != 8 ) {
		for (i = 0; i < 256; i++) {
			regp->DAC[i*3]     = i;
			regp->DAC[(i*3)+1] = i;
			regp->DAC[(i*3)+2] = i;
		}
	}

	/*
	* Calculate the extended registers.
	*/

	if(pLayout->depth < 24) {
		i = pLayout->depth;
	} else {
		i = 32;
	}

	if(pNv->Architecture >= NV_ARCH_10) {
		pNv->CURSOR = (CARD32 *)pNv->Cursor->map;
	}

	/* What is the meaning of this register? */
	/* A few popular values are 0x18, 0x1c, 0x38, 0x3c */ 
	regp->CRTC[NV_VGA_CRTCX_FIFO1] = savep->CRTC[NV_VGA_CRTCX_FIFO1] & ~(1<<5);

	/* NV40's don't set FPP units, unless in special conditions (then they set both) */
	/* But what are those special conditions? */
	if (pNv->Architecture <= NV_ARCH_30) {
		if (is_fp) {
			if(nv_crtc->head == 1) {
				regp->head |= NV_CRTC_FSEL_FPP1;
			} else if (pNv->twoHeads) {
				regp->head |= NV_CRTC_FSEL_FPP2;
			}
		}
	} else {
		/* This is observed on some g70 cards, non-flatpanel's too */
		if (nv_crtc->head == 1 && pNv->NVArch > 0x44) {
			regp->head |= NV_CRTC_FSEL_FPP2;
		}
	}

	/* Except for rare conditions I2C is enabled on the primary crtc */
	if (nv_crtc->head == 0) {
		if (pNv->overlayAdaptor) {
			regp->head |= NV_CRTC_FSEL_OVERLAY;
		}
		regp->head |= NV_CRTC_FSEL_I2C;
	}

	/* This is not what nv does, but it is what the blob does (for nv4x at least) */
	/* This fixes my cursor corruption issue */
	regp->cursorConfig = 0x0;
	if(mode->Flags & V_DBLSCAN)
		regp->cursorConfig |= (1 << 4);
	if (pNv->alphaCursor) {
		/* bit28 means we go into alpha blend mode and not rely on the current ROP */
		regp->cursorConfig |= 0x14011000;
	} else {
		regp->cursorConfig |= 0x02000000;
	}

	/* Unblock some timings */
	regp->CRTC[NV_VGA_CRTCX_FP_HTIMING] = 0;
	regp->CRTC[NV_VGA_CRTCX_FP_VTIMING] = 0;

	/* What is the purpose of this register? */
	if (nv_crtc->head == 1) {
		regp->CRTC[NV_VGA_CRTCX_26] = 0x14;
	} else {
		regp->CRTC[NV_VGA_CRTCX_26] = 0x20;
	}

	/* 0x00 is disabled, 0x22 crt and 0x88 dfp */
	/* 0x11 is LVDS? */
	if (is_fp) {
		regp->CRTC[NV_VGA_CRTCX_3B] = 0x88;
	} else {
		/* 0x20 is also seen sometimes, why? */
		if (nv_crtc->head == 1) {
			regp->CRTC[NV_VGA_CRTCX_3B] = 0x24;
		} else {
			regp->CRTC[NV_VGA_CRTCX_3B] = 0x22;
		}
	}

	/* These values seem to vary */
	if (nv_crtc->head == 1) {
		regp->CRTC[NV_VGA_CRTCX_3C] = 0x0;
	} else {
		regp->CRTC[NV_VGA_CRTCX_3C] = 0x70;
	}

	/* 0x80 seems to be used very often, if not always */
	regp->CRTC[NV_VGA_CRTCX_45] = 0x80;

	if (nv_crtc->head == 1) {
		regp->CRTC[NV_VGA_CRTCX_4B] = 0x0;
	} else {
		regp->CRTC[NV_VGA_CRTCX_4B] = 0x1;
	}

	if (is_fp)
		regp->CRTC[NV_VGA_CRTCX_4B] |= 0x80;

	/* Are these(0x55 and 0x56) also timing related registers, since disabling them does nothing? */
	regp->CRTC[NV_VGA_CRTCX_55] = 0x0;

	/* Common values like 0x14 and 0x04 are converted to 0x10 and 0x00 */
	regp->CRTC[NV_VGA_CRTCX_56] = 0x0;

	/* The blob seems to take the current value from crtc 0, add 4 to that and reuse the old value for crtc 1*/
	if (nv_crtc->head == 1) {
		regp->CRTC[NV_VGA_CRTCX_52] = pNv->misc_info.crtc_0_reg_52;
	} else {
		regp->CRTC[NV_VGA_CRTCX_52] = pNv->misc_info.crtc_0_reg_52 + 4;
	}

	/* The exact purpose of this register is unknown, but we copy value from crtc0 */
	regp->unk81c = nvReadCRTC0(pNv, NV_CRTC_081C);

	regp->unk830 = mode->CrtcVDisplay - 3;
	regp->unk834 = mode->CrtcVDisplay - 1;

	/* This is what the blob does */
	regp->unk850 = nvReadCRTC(pNv, 0, NV_CRTC_0850);

	/* Never ever modify gpio, unless you know very well what you're doing */
	regp->gpio = nvReadCRTC(pNv, 0, NV_CRTC_GPIO);

	/* Switch to non-vga mode (the so called HSYNC mode) */
	regp->config = 0x2;

	/*
	 * Calculate the state that is common to all crtc's (stored in the state struct).
	 */
	ErrorF("crtc %d %d %d\n", nv_crtc->crtc, mode->CrtcHDisplay, pScrn->displayWidth);
	nv_crtc_calc_state_ext(crtc,
				i,
				pScrn->displayWidth,
				mode->CrtcHDisplay,
				mode->CrtcVDisplay,
				adjusted_mode->Clock,
				mode->Flags);

	/* Enable slaved mode */
	if (is_fp) {
		regp->CRTC[NV_VGA_CRTCX_PIXEL] |= (1 << 7);
	}
}

static void
nv_crtc_mode_set_ramdac_regs(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVCrtcRegPtr regp;
	NVPtr pNv = NVPTR(pScrn);
	NVFBLayout *pLayout = &pNv->CurrentLayout;
	Bool is_fp = FALSE;
	Bool is_lvds = FALSE;
	float aspect_ratio, panel_ratio;
	uint32_t h_scale, v_scale;

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	xf86OutputPtr output = NVGetOutputFromCRTC(crtc);
	NVOutputPrivatePtr nv_output = NULL;
	if (output) {
		nv_output = output->driver_private;

		if ((nv_output->type == OUTPUT_LVDS) || (nv_output->type == OUTPUT_TMDS))
			is_fp = TRUE;

		if (nv_output->type == OUTPUT_LVDS)
			is_lvds = TRUE;
	}

	if (is_fp) {
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

	/*
	* bit0: positive vsync
	* bit4: positive hsync
	* bit8: enable panel scaling 
	* bit26: a bit sometimes seen on some g70 cards
	* bit31: set for dual link LVDS
	* This must also be set for non-flatpanels
	* Some bits seem shifted for vga monitors
	*/

	if (is_fp) {
		regp->fp_control = 0x11100000;
	} else {
		regp->fp_control = 0x21100000;
	}

	if (is_lvds && pNv->VBIOS.fp.dual_link)
		regp->fp_control |= 0x80000000;
	else {
		/* If the special bit exists, it exists on both ramdac's */
		regp->fp_control |= nvReadRAMDAC0(pNv, NV_RAMDAC_FP_CONTROL) & (1 << 26);
	}

	/* Deal with vsync/hsync polarity */
	/* These analog monitor offsets are guesswork */
	if (adjusted_mode->Flags & V_PVSYNC) {
		regp->fp_control |= (1 << (0 + !is_fp));
	}

	if (adjusted_mode->Flags & V_PHSYNC) {
		regp->fp_control |= (1 << (4 + !is_fp));
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

		/* 0 = panel scaling */
		if (nv_output->scaling_mode == 0) {
			ErrorF("Flat panel is doing the scaling.\n");
			regp->fp_control |= (1 << 8);
		} else {
			ErrorF("GPU is doing the scaling.\n");

			/* 1 = fullscale gpu */
			/* 2 = aspect ratio scaling */
			if (nv_output->scaling_mode == 2) {
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
		}

		ErrorF("Post-panel scaling\n");
	}

	if (pNv->Architecture >= NV_ARCH_10) {
		/* Bios and blob don't seem to do anything (else) */
		regp->nv10_cursync = (1<<25);
	}

	/* These are the common blob values, minus a few fp specific bit's */
	/* Let's keep the TMDS pll and fpclock running in all situations */
	regp->debug_0 = 0x1101111;

	if(is_fp) {
		/* I am not completely certain, but seems to be set only for dfp's */
		regp->debug_0 |= NV_RAMDAC_FP_DEBUG_0_TMDS_ENABLED;
	}

	if (output)
		ErrorF("output %d debug_0 %08X\n", nv_output->preferred_ramdac, regp->debug_0);

	/* Flatpanel support needs at least a NV10 */
	if(pNv->twoHeads) {
		/* Instead of 1, several other values are also used: 2, 7, 9 */
		/* The purpose is unknown */
		if(pNv->FPDither) {
			regp->dither = 0x00010000;
		}
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

	/* Some values the blob sets */
	/* This may apply to the real ramdac that is being used (for crosswired situations) */
	/* Nevertheless, it's unlikely to cause many problems, since the values are equal for both */
	regp->unk_a20 = 0x0;
	regp->unk_a24 = 0xfffff;
	regp->unk_a34 = 0x1;
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
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	ErrorF("nv_crtc_mode_set is called for CRTC %d\n", nv_crtc->crtc);

	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Mode on CRTC %d\n", nv_crtc->crtc);
	xf86PrintModeline(pScrn->scrnIndex, mode);
	NVCrtcSetOwner(crtc);

	nv_crtc_mode_set_vga(crtc, mode, adjusted_mode);
	nv_crtc_mode_set_regs(crtc, mode, adjusted_mode);
	nv_crtc_mode_set_ramdac_regs(crtc, mode, adjusted_mode);

	/* Just in case */
	NVCrtcLockUnlock(crtc, FALSE);

	NVVgaProtect(crtc, TRUE);
	nv_crtc_load_state_ext(crtc, &pNv->ModeReg, FALSE);
	nv_crtc_load_state_vga(crtc, &pNv->ModeReg);
	if (pNv->Architecture == NV_ARCH_40) {
		nv40_crtc_load_state_pll(pNv, &pNv->ModeReg);
	} else {
		nv_crtc_load_state_pll(pNv, &pNv->ModeReg);
	}
	nv_crtc_load_state_ramdac(crtc, &pNv->ModeReg);

	NVVgaProtect(crtc, FALSE);

	NVCrtcSetBase(crtc, x, y);

#if X_BYTE_ORDER == X_BIG_ENDIAN
	/* turn on LFB swapping */
	{
		unsigned char tmp;

		tmp = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_SWAPPING);
		tmp |= (1 << 7);
		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_SWAPPING, tmp);
	}
#endif
}

void nv_crtc_save(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	ErrorF("nv_crtc_save is called for CRTC %d\n", nv_crtc->crtc);

	/* We just came back from terminal, so unlock */
	NVCrtcLockUnlock(crtc, FALSE);

	NVCrtcSetOwner(crtc);
	nv_crtc_save_state_vga(crtc, &pNv->SavedReg);
	nv_crtc_save_state_ext(crtc, &pNv->SavedReg);
	if (pNv->Architecture == NV_ARCH_40) {
		nv40_crtc_save_state_pll(pNv, &pNv->SavedReg);
	} else {
		nv_crtc_save_state_pll(pNv, &pNv->SavedReg);
	}
	nv_crtc_save_state_ramdac(crtc, &pNv->SavedReg);
}

void nv_crtc_restore(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state;
	NVOutputRegPtr regp;

	state = &pNv->SavedReg;

	/* Some aspects of an output needs to be restore before the crtc. */
	/* In my case this has to do with the mode that i get at very low resolutions. */
	/* If i do this at the end, it will not be restored properly */
	/* Assumption: crtc0 is restored first. */
	if (nv_crtc->head == 0) {
		xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(pScrn);
		int i;
		ErrorF("Restore all TMDS timings, before restoring anything else\n");
		for (i = 0; i < config->num_output; i++) {
			NVOutputPrivatePtr nv_output2 = config->output[i]->driver_private;
			regp = &state->dac_reg[nv_output2->preferred_ramdac];
			/* Let's guess the bios state ;-) */
			if (nv_output2->type == OUTPUT_TMDS || nv_output2->type == OUTPUT_LVDS) {
				uint32_t clock = nv_calc_clock_from_pll(config->output[i]);
				Bool crosswired = regp->TMDS[0x4] & (1 << 3);
				nv_set_tmds_registers(config->output[i], clock, TRUE, crosswired);
			}
		}
	}

	ErrorF("nv_crtc_restore is called for CRTC %d\n", nv_crtc->crtc);

	NVCrtcSetOwner(crtc);

	/* Just to be safe */
	NVCrtcLockUnlock(crtc, FALSE);

	NVVgaProtect(crtc, TRUE);
	nv_crtc_load_state_ext(crtc, &pNv->SavedReg, TRUE);
	nv_crtc_load_state_vga(crtc, &pNv->SavedReg);
	if (pNv->Architecture == NV_ARCH_40) {
		nv40_crtc_load_state_pll(pNv, &pNv->SavedReg);
	} else {
		nv_crtc_load_state_pll(pNv, &pNv->SavedReg);
	}
	nv_crtc_load_state_ramdac(crtc, &pNv->SavedReg);
	nvWriteVGA(pNv, NV_VGA_CRTCX_OWNER, pNv->vtOWNER);
	NVVgaProtect(crtc, FALSE);

	/* We must lock the door if we leave ;-) */
	NVCrtcLockUnlock(crtc, TRUE);
}

void nv_crtc_prepare(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	ErrorF("nv_crtc_prepare is called for CRTC %d\n", nv_crtc->crtc);

	crtc->funcs->dpms(crtc, DPMSModeOff);

	/* Sync the engine before adjust mode */
	if (pNv->EXADriverPtr) {
		exaMarkSync(pScrn->pScreen);
		exaWaitSync(pScrn->pScreen);
	}
}

void nv_crtc_commit(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ErrorF("nv_crtc_commit for CRTC %d\n", nv_crtc->crtc);

	crtc->funcs->dpms (crtc, DPMSModeOn);

	if (crtc->scrn->pScreen != NULL)
		xf86_reload_cursors (crtc->scrn->pScreen);
}

static Bool nv_crtc_lock(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ErrorF("nv_crtc_lock is called for CRTC %d\n", nv_crtc->crtc);

	return FALSE;
}

static void nv_crtc_unlock(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ErrorF("nv_crtc_unlock is called for CRTC %d\n", nv_crtc->crtc);
}

static void
nv_crtc_gamma_set(xf86CrtcPtr crtc, CARD16 *red, CARD16 *green, CARD16 *blue,
					int size)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	int i, j;

	NVCrtcRegPtr regp;
	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	switch (pNv->CurrentLayout.depth) {
	case 15:
		/* R5G5B5 */
		/* We've got 5 bit (32 values) colors and 256 registers for each color */
		for (i = 0; i < 32; i++) {
			for (j = 0; j < 8; j++) {
				regp->DAC[(i*8 + j) * 3 + 0] = red[i] >> 8;
				regp->DAC[(i*8 + j) * 3 + 1] = green[i] >> 8;
				regp->DAC[(i*8 + j) * 3 + 2] = blue[i] >> 8;
			}
		}
		break;
	case 16:
		/* R5G6B5 */
		/* First deal with the 5 bit colors */
		for (i = 0; i < 32; i++) {
			for (j = 0; j < 8; j++) {
				regp->DAC[(i*8 + j) * 3 + 0] = red[i] >> 8;
				regp->DAC[(i*8 + j) * 3 + 2] = blue[i] >> 8;
			}
		}
		/* Now deal with the 6 bit color */
		for (i = 0; i < 64; i++) {
			for (j = 0; j < 4; j++) {
				regp->DAC[(i*4 + j) * 3 + 1] = green[i] >> 8;
			}
		}
		break;
	default:
		/* R8G8B8 */
		for (i = 0; i < 256; i++) {
			regp->DAC[i * 3] = red[i] >> 8;
			regp->DAC[(i * 3) + 1] = green[i] >> 8;
			regp->DAC[(i * 3) + 2] = blue[i] >> 8;
		}
		break;
	}

	NVCrtcLoadPalette(crtc);
}

/* NV04-NV10 doesn't support alpha cursors */
static const xf86CrtcFuncsRec nv_crtc_funcs = {
	.dpms = nv_crtc_dpms,
	.save = nv_crtc_save, /* XXX */
	.restore = nv_crtc_restore, /* XXX */
	.mode_fixup = nv_crtc_mode_fixup,
	.mode_set = nv_crtc_mode_set,
	.prepare = nv_crtc_prepare,
	.commit = nv_crtc_commit,
	.destroy = NULL, /* XXX */
	.lock = nv_crtc_lock,
	.unlock = nv_crtc_unlock,
	.set_cursor_colors = nv_crtc_set_cursor_colors,
	.set_cursor_position = nv_crtc_set_cursor_position,
	.show_cursor = nv_crtc_show_cursor,
	.hide_cursor = nv_crtc_hide_cursor,
	.load_cursor_image = nv_crtc_load_cursor_image,
	.gamma_set = nv_crtc_gamma_set,
};

/* NV11 and up has support for alpha cursors. */ 
/* Due to different maximum sizes we cannot allow it to use normal cursors */
static const xf86CrtcFuncsRec nv11_crtc_funcs = {
	.dpms = nv_crtc_dpms,
	.save = nv_crtc_save, /* XXX */
	.restore = nv_crtc_restore, /* XXX */
	.mode_fixup = nv_crtc_mode_fixup,
	.mode_set = nv_crtc_mode_set,
	.prepare = nv_crtc_prepare,
	.commit = nv_crtc_commit,
	.destroy = NULL, /* XXX */
	.lock = nv_crtc_lock,
	.unlock = nv_crtc_unlock,
	.set_cursor_colors = NULL, /* Alpha cursors do not need this */
	.set_cursor_position = nv_crtc_set_cursor_position,
	.show_cursor = nv_crtc_show_cursor,
	.hide_cursor = nv_crtc_hide_cursor,
	.load_cursor_argb = nv_crtc_load_cursor_argb,
	.gamma_set = nv_crtc_gamma_set,
};


void
nv_crtc_init(ScrnInfoPtr pScrn, int crtc_num)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcPtr crtc;
	NVCrtcPrivatePtr nv_crtc;

	if (pNv->NVArch >= 0x11) {
		crtc = xf86CrtcCreate (pScrn, &nv11_crtc_funcs);
	} else {
		crtc = xf86CrtcCreate (pScrn, &nv_crtc_funcs);
	}
	if (crtc == NULL)
		return;

	nv_crtc = xnfcalloc (sizeof (NVCrtcPrivateRec), 1);
	nv_crtc->crtc = crtc_num;
	nv_crtc->head = crtc_num;

	crtc->driver_private = nv_crtc;

	NVCrtcLockUnlock(crtc, FALSE);
}

static void nv_crtc_load_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
    NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
    int i;
    NVCrtcRegPtr regp;

    regp = &state->crtc_reg[nv_crtc->head];

    NVWriteMiscOut(crtc, regp->MiscOutReg);

    for (i = 1; i < 5; i++)
      NVWriteVgaSeq(crtc, i, regp->Sequencer[i]);
  
    /* Ensure CRTC registers 0-7 are unlocked by clearing bit 7 of CRTC[17] */
    NVWriteVgaCrtc(crtc, 17, regp->CRTC[17] & ~0x80);

    for (i = 0; i < 25; i++)
      NVWriteVgaCrtc(crtc, i, regp->CRTC[i]);

    for (i = 0; i < 9; i++)
      NVWriteVgaGr(crtc, i, regp->Graphics[i]);
    
    NVEnablePalette(crtc);
    for (i = 0; i < 21; i++)
      NVWriteVgaAttr(crtc, i, regp->Attribute[i]);
    NVDisablePalette(crtc);

}

static void nv_crtc_fix_nv40_hw_cursor(xf86CrtcPtr crtc)
{
	/* TODO - implement this properly */
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);

	if (pNv->Architecture == NV_ARCH_40) {  /* HW bug */
		volatile CARD32 curpos = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_CURSOR_POS);
		nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_CURSOR_POS, curpos);
	}
}
static void nv_crtc_load_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state, Bool override)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    NVPtr pNv = NVPTR(pScrn);    
    NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
    NVCrtcRegPtr regp;
    int i;
    
    regp = &state->crtc_reg[nv_crtc->head];

    if(pNv->Architecture >= NV_ARCH_10) {
        if(pNv->twoHeads) {
           nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_FSEL, regp->head);
        }
        nvWriteVIDEO(pNv, NV_PVIDEO_STOP, 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_INTR_EN, 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(0), 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(1), 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(0), pNv->VRAMPhysicalSize - 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(1), pNv->VRAMPhysicalSize - 1);
        nvWriteMC(pNv, 0x1588, 0);

	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_BUFFER, regp->CRTC[NV_VGA_CRTCX_BUFFER]);
        nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_CURSOR_CONFIG, regp->cursorConfig);
	nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_GPIO, regp->gpio);
        nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_0830, regp->unk830);
        nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_0834, regp->unk834);
	nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_0850, regp->unk850);
	nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_081C, regp->unk81c);

	nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_CONFIG, regp->config);

	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FP_HTIMING, regp->CRTC[NV_VGA_CRTCX_FP_HTIMING]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FP_VTIMING, regp->CRTC[NV_VGA_CRTCX_FP_VTIMING]);

	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_26, regp->CRTC[NV_VGA_CRTCX_26]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_3B, regp->CRTC[NV_VGA_CRTCX_3B]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_3C, regp->CRTC[NV_VGA_CRTCX_3C]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_45, regp->CRTC[NV_VGA_CRTCX_45]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_4B, regp->CRTC[NV_VGA_CRTCX_4B]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_52, regp->CRTC[NV_VGA_CRTCX_52]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_56, regp->CRTC[NV_VGA_CRTCX_56]);
	if (override) {
		for (i = 0; i < 0x10; i++)
			NVWriteVGACR5758(pNv, nv_crtc->head, i, regp->CR58[i]);
	}
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_59, regp->CRTC[NV_VGA_CRTCX_59]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_EXTRA, regp->CRTC[NV_VGA_CRTCX_EXTRA]);
    }

    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_REPAINT0, regp->CRTC[NV_VGA_CRTCX_REPAINT0]);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_REPAINT1, regp->CRTC[NV_VGA_CRTCX_REPAINT1]);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_LSR, regp->CRTC[NV_VGA_CRTCX_LSR]);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_PIXEL, regp->CRTC[NV_VGA_CRTCX_PIXEL]);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_LCD, regp->CRTC[NV_VGA_CRTCX_LCD]);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_HEB, regp->CRTC[NV_VGA_CRTCX_HEB]);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FIFO1, regp->CRTC[NV_VGA_CRTCX_FIFO1]);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FIFO0, regp->CRTC[NV_VGA_CRTCX_FIFO0]);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FIFO_LWM, regp->CRTC[NV_VGA_CRTCX_FIFO_LWM]);
    if(pNv->Architecture >= NV_ARCH_30) {
      NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FIFO_LWM_NV30, regp->CRTC[NV_VGA_CRTCX_FIFO_LWM_NV30]);
    }

    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL0, regp->CRTC[NV_VGA_CRTCX_CURCTL0]);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL1, regp->CRTC[NV_VGA_CRTCX_CURCTL1]);
    nv_crtc_fix_nv40_hw_cursor(crtc);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL2, regp->CRTC[NV_VGA_CRTCX_CURCTL2]);
    NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_INTERLACE, regp->CRTC[NV_VGA_CRTCX_INTERLACE]);

    nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_INTR_EN_0, 0);
    nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_INTR_0, NV_CRTC_INTR_VBLANK);

    pNv->CurrentState = state;
}

static void nv_crtc_save_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
    NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
    int i;
    NVCrtcRegPtr regp;

    regp = &state->crtc_reg[nv_crtc->head];

    regp->MiscOutReg = NVReadMiscOut(crtc);

    for (i = 0; i < 25; i++)
	regp->CRTC[i] = NVReadVgaCrtc(crtc, i);

    NVEnablePalette(crtc);
    for (i = 0; i < 21; i++)
	regp->Attribute[i] = NVReadVgaAttr(crtc, i);
    NVDisablePalette(crtc);

    for (i = 0; i < 9; i++)
	regp->Graphics[i] = NVReadVgaGr(crtc, i);

    for (i = 1; i < 5; i++)
	regp->Sequencer[i] = NVReadVgaSeq(crtc, i);
  
}

static void nv_crtc_save_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    NVPtr pNv = NVPTR(pScrn);    
    NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
    NVCrtcRegPtr regp;
    int i;

    regp = &state->crtc_reg[nv_crtc->head];
 
    regp->CRTC[NV_VGA_CRTCX_LCD] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_LCD);
    regp->CRTC[NV_VGA_CRTCX_REPAINT0] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_REPAINT0);
    regp->CRTC[NV_VGA_CRTCX_REPAINT1] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_REPAINT1);
    regp->CRTC[NV_VGA_CRTCX_LSR] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_LSR);
    regp->CRTC[NV_VGA_CRTCX_PIXEL] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_PIXEL);
    regp->CRTC[NV_VGA_CRTCX_HEB] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_HEB);
    regp->CRTC[NV_VGA_CRTCX_FIFO1] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FIFO1);

    regp->CRTC[NV_VGA_CRTCX_FIFO0] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FIFO0);
    regp->CRTC[NV_VGA_CRTCX_FIFO_LWM] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FIFO_LWM);
    if(pNv->Architecture >= NV_ARCH_30) {
         regp->CRTC[NV_VGA_CRTCX_FIFO_LWM_NV30] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FIFO_LWM_NV30);
    }
    regp->CRTC[NV_VGA_CRTCX_CURCTL0] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL0);
    regp->CRTC[NV_VGA_CRTCX_CURCTL1] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL1);
    regp->CRTC[NV_VGA_CRTCX_CURCTL2] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL2);
    regp->CRTC[NV_VGA_CRTCX_INTERLACE] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_INTERLACE);
 
    regp->gpio = nvReadCRTC(pNv, nv_crtc->head, NV_CRTC_GPIO);
    regp->unk830 = nvReadCRTC(pNv, nv_crtc->head, NV_CRTC_0830);
    regp->unk834 = nvReadCRTC(pNv, nv_crtc->head, NV_CRTC_0834);
    regp->unk850 = nvReadCRTC(pNv, nv_crtc->head, NV_CRTC_0850);
    regp->unk81c = nvReadCRTC(pNv, nv_crtc->head, NV_CRTC_081C);

	regp->config = nvReadCRTC(pNv, nv_crtc->head, NV_CRTC_CONFIG);

    if(pNv->Architecture >= NV_ARCH_10) {
        if(pNv->twoHeads) {
           regp->head     = nvReadCRTC(pNv, nv_crtc->head, NV_CRTC_FSEL);
           regp->crtcOwner = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_OWNER);
        }
        regp->CRTC[NV_VGA_CRTCX_EXTRA] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_EXTRA);

        regp->cursorConfig = nvReadCRTC(pNv, nv_crtc->head, NV_CRTC_CURSOR_CONFIG);

	regp->CRTC[NV_VGA_CRTCX_26] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_26);
	regp->CRTC[NV_VGA_CRTCX_3B] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_3B);
	regp->CRTC[NV_VGA_CRTCX_3C] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_3C);
	regp->CRTC[NV_VGA_CRTCX_45] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_45);
	regp->CRTC[NV_VGA_CRTCX_4B] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_4B);
	regp->CRTC[NV_VGA_CRTCX_52] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_52);
	regp->CRTC[NV_VGA_CRTCX_56] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_56);
	for (i = 0; i < 0x10; i++)
		regp->CR58[i] = NVReadVGACR5758(pNv, nv_crtc->head, i);
	regp->CRTC[NV_VGA_CRTCX_59] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_59);
	regp->CRTC[NV_VGA_CRTCX_BUFFER] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_BUFFER);
	regp->CRTC[NV_VGA_CRTCX_FP_HTIMING] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FP_HTIMING);
	regp->CRTC[NV_VGA_CRTCX_FP_VTIMING] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FP_VTIMING);
    }
}

static void nv_crtc_save_state_ramdac(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);    
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVCrtcRegPtr regp;
	int i;

	regp = &state->crtc_reg[nv_crtc->head];

	regp->general = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_GENERAL_CONTROL);

	regp->fp_control	= nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_CONTROL);
	regp->debug_0	= nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_DEBUG_0);
	regp->debug_1	= nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_DEBUG_1);
	regp->debug_2	= nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_DEBUG_2);

	regp->unk_a20 = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_A20);
	regp->unk_a24 = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_A24);
	regp->unk_a34 = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_A34);

	if (pNv->NVArch == 0x11) {
		regp->dither = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_DITHER_NV11);
	} else if (pNv->twoHeads) {
		regp->dither = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_DITHER);
	}
	regp->nv10_cursync = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_NV10_CURSYNC);

	/* The regs below are 0 for non-flatpanels, so you can load and save them */

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_HDISP_END + (i * 4);
		regp->fp_horiz_regs[i] = nvReadRAMDAC(pNv, nv_crtc->head, ramdac_reg);
	}

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_VDISP_END + (i * 4);
		regp->fp_vert_regs[i] = nvReadRAMDAC(pNv, nv_crtc->head, ramdac_reg);
	}

	regp->fp_hvalid_start = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_HVALID_START);
	regp->fp_hvalid_end = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_HVALID_END);
	regp->fp_vvalid_start = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_VVALID_START);
	regp->fp_vvalid_end = nvReadRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_VVALID_END);
}

static void nv_crtc_load_state_ramdac(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);    
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVCrtcRegPtr regp;
	int i;

	regp = &state->crtc_reg[nv_crtc->head];

	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_GENERAL_CONTROL, regp->general);

	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_CONTROL, regp->fp_control);
	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_DEBUG_0, regp->debug_0);
	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_DEBUG_1, regp->debug_1);
	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_DEBUG_2, regp->debug_2);

	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_A20, regp->unk_a20);
	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_A24, regp->unk_a24);
	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_A34, regp->unk_a34);

	if (pNv->NVArch == 0x11) {
		nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_DITHER_NV11, regp->dither);
	} else if (pNv->twoHeads) {
		nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_DITHER, regp->dither);
	}
	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_NV10_CURSYNC, regp->nv10_cursync);

	/* The regs below are 0 for non-flatpanels, so you can load and save them */

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_HDISP_END + (i * 4);
		nvWriteRAMDAC(pNv, nv_crtc->head, ramdac_reg, regp->fp_horiz_regs[i]);
	}

	for (i = 0; i < 7; i++) {
		uint32_t ramdac_reg = NV_RAMDAC_FP_VDISP_END + (i * 4);
		nvWriteRAMDAC(pNv, nv_crtc->head, ramdac_reg, regp->fp_vert_regs[i]);
	}

	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_HVALID_START, regp->fp_hvalid_start);
	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_HVALID_END, regp->fp_hvalid_end);
	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_VVALID_START, regp->fp_vvalid_start);
	nvWriteRAMDAC(pNv, nv_crtc->head, NV_RAMDAC_FP_VVALID_END, regp->fp_vvalid_end);
}

void
NVCrtcSetBase (xf86CrtcPtr crtc, int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);    
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVFBLayout *pLayout = &pNv->CurrentLayout;
	CARD32 start = 0;

	ErrorF("NVCrtcSetBase: x: %d y: %d\n", x, y);

	start += ((y * pScrn->displayWidth + x) * (pLayout->bitsPerPixel/8));
	start += pNv->FB->offset;

	/* 30 bits addresses in 32 bits according to haiku */
	nvWriteCRTC(pNv, nv_crtc->head, NV_CRTC_START, start & 0xfffffffc);

	/* set NV4/NV10 byte adress: (bit0 - 1) */
	NVWriteVgaAttr(crtc, 0x13, (start & 0x3) << 1);

	crtc->x = x;
	crtc->y = y;
}

static void NVCrtcWriteDacMask(xf86CrtcPtr crtc, CARD8 value)
{
  ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pDACReg = nv_crtc->head ? pNv->PDIO1 : pNv->PDIO0;

  NV_WR08(pDACReg, VGA_DAC_MASK, value);
}

static CARD8 NVCrtcReadDacMask(xf86CrtcPtr crtc)
{
  ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pDACReg = nv_crtc->head ? pNv->PDIO1 : pNv->PDIO0;
  
  return NV_RD08(pDACReg, VGA_DAC_MASK);
}

static void NVCrtcWriteDacReadAddr(xf86CrtcPtr crtc, CARD8 value)
{
  ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pDACReg = nv_crtc->head ? pNv->PDIO1 : pNv->PDIO0;

  NV_WR08(pDACReg, VGA_DAC_READ_ADDR, value);
}

static void NVCrtcWriteDacWriteAddr(xf86CrtcPtr crtc, CARD8 value)
{
  ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pDACReg = nv_crtc->head ? pNv->PDIO1 : pNv->PDIO0;

  NV_WR08(pDACReg, VGA_DAC_WRITE_ADDR, value);
}

static void NVCrtcWriteDacData(xf86CrtcPtr crtc, CARD8 value)
{
  ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pDACReg = nv_crtc->head ? pNv->PDIO1 : pNv->PDIO0;

  NV_WR08(pDACReg, VGA_DAC_DATA, value);
}

static CARD8 NVCrtcReadDacData(xf86CrtcPtr crtc, CARD8 value)
{
  ScrnInfoPtr pScrn = crtc->scrn;
  NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
  NVPtr pNv = NVPTR(pScrn);
  volatile CARD8 *pDACReg = nv_crtc->head ? pNv->PDIO1 : pNv->PDIO0;

  return NV_RD08(pDACReg, VGA_DAC_DATA);
}

void NVCrtcLoadPalette(xf86CrtcPtr crtc)
{
	int i;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVCrtcRegPtr regp;
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	NVCrtcSetOwner(crtc);
	NVCrtcWriteDacMask(crtc, 0xff);
	NVCrtcWriteDacWriteAddr(crtc, 0x00);

	for (i = 0; i<768; i++) {
		NVCrtcWriteDacData(crtc, regp->DAC[i]);
	}
	NVDisablePalette(crtc);
}

void NVCrtcBlankScreen(xf86CrtcPtr crtc, Bool on)
{
	unsigned char scrn;

	NVCrtcSetOwner(crtc);

	scrn = NVReadVgaSeq(crtc, 0x01);
	if (on) {
		scrn &= ~0x20;
	} else {
		scrn |= 0x20;
	}

	NVVgaSeqReset(crtc, TRUE);
	NVWriteVgaSeq(crtc, 0x01, scrn);
	NVVgaSeqReset(crtc, FALSE);
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
