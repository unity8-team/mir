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
static void nv_crtc_load_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_save_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_save_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state);

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
	int		clockIn,
	int		*clockOut,
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

	for (P = 0; P <= highP; P++) {
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
	int		clockIn,
	int		*clockOut,
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
	/* This seems to be strictly NV40 */
	if (pNv->Architecture == NV_ARCH_40) {
		state->reg580 = nvReadRAMDAC0(pNv, NV_RAMDAC_580);
	}
}


static void nv_crtc_load_state_pll(NVPtr pNv, RIVA_HW_STATE *state)
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

	if (state->vpll2) {
		if (pNv->Architecture == NV_ARCH_40) {
			nvWriteRAMDAC(pNv, index[1], NV_RAMDAC_FP_DEBUG_0,
				fp_debug_0[index[1]] | NV_RAMDAC_FP_DEBUG_0_PWRDOWN_TMDS_PLL);

			/* Wait for the situation to stabilise */
			usleep(5000);

			CARD32 reg_c040 = pNv->misc_info.reg_c040;
			/* for vpll2 change bits 18 and 19 are disabled */
			reg_c040 &= ~(0x3 << 18);
			nvWriteMC(pNv, 0xc040, reg_c040);
		}

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

		if (pNv->Architecture == NV_ARCH_40) {
			uint32_t reg580 = state->reg580;
			/* Let's keep vpll1 off for the moment */
			if (!(pNv->misc_info.ramdac_0_reg_580 & NV_RAMDAC_580_VPLL1_ACTIVE))
				reg580 &= ~NV_RAMDAC_580_VPLL1_ACTIVE;
			ErrorF("writing reg580 %08X\n", reg580);
			nvWriteRAMDAC0(pNv, NV_RAMDAC_580, reg580);
			/* We need to wait a while */
			usleep(5000);
			nvWriteMC(pNv, 0xc040, pNv->misc_info.reg_c040);

			nvWriteRAMDAC(pNv, index[1], NV_RAMDAC_FP_DEBUG_0, fp_debug_0[index[1]]);

			/* Wait for the situation to stabilise */
			usleep(5000);

			/* A last minute attempt to switch to DB2 ratio */
			if (!(nvReadRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT) & NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2)) {
				uint32_t pllsel = state->pllsel & ~NV_RAMDAC_PLL_SELECT_PLL_SOURCE_ALL;
				nvWriteRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT, pllsel | NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2);
				if (nvReadRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT) & NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2) {
					state->pllsel |= NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2;
					pllsel |= NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2;
				}
				nvWriteRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT, pllsel);
			}
		}
	}

	if (pNv->Architecture == NV_ARCH_40) {
		nvWriteRAMDAC(pNv, index[0], NV_RAMDAC_FP_DEBUG_0,
			fp_debug_0[index[0]] | NV_RAMDAC_FP_DEBUG_0_PWRDOWN_TMDS_PLL);

		/* Wait for the situation to stabilise */
		usleep(5000);

		CARD32 reg_c040 = pNv->misc_info.reg_c040;
		/* for vpll1 change bits 16 and 17 are disabled */
		reg_c040 &= ~(0x3 << 16);
		nvWriteMC(pNv, 0xc040, reg_c040);
	}

	ErrorF("writing vpll %08X\n", state->vpll);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL, state->vpll);
	if(pNv->twoStagePLL) {
		ErrorF("writing vpllB %08X\n", state->vpllB);
		nvWriteRAMDAC0(pNv, NV_RAMDAC_VPLL_B, state->vpllB);
	}

	ErrorF("writing pllsel %08X\n", state->pllsel);
	nvWriteRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT, state->pllsel);

	if (pNv->Architecture == NV_ARCH_40) {
		ErrorF("writing reg580 %08X\n", state->reg580);
		nvWriteRAMDAC0(pNv, NV_RAMDAC_580, state->reg580);
		/* We need to wait a while */
		usleep(5000);
		nvWriteMC(pNv, 0xc040, pNv->misc_info.reg_c040);

		/* This register is only written after the last clock is set */
		nvWriteRAMDAC0(pNv, NV_RAMDAC_SEL_CLK, state->sel_clk);

		nvWriteRAMDAC(pNv, index[0], NV_RAMDAC_FP_DEBUG_0, fp_debug_0[index[0]]);

		/* Wait for the situation to stabilise */
		usleep(5000);

		/* A last minute attempt to switch to DB2 ratio */
		if (!(nvReadRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT) & NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2)) {
			uint32_t pllsel = state->pllsel;
			nvWriteRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT, pllsel | NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2);
			if (nvReadRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT) & NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2) {
				state->pllsel |= NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2;
				pllsel |= NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2;
			}
			nvWriteRAMDAC0(pNv, NV_RAMDAC_PLL_SELECT, pllsel);
		}
	}
}

/*
 * Calculate extended mode parameters (SVGA) and save in a 
 * mode state structure.
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
	int pixelDepth, VClk = 0;
	CARD32 CursorStart;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	NVCrtcRegPtr regp;
	NVPtr pNv = NVPTR(pScrn);    
	RIVA_HW_STATE *state;
	int num_crtc_enabled, i;

	state = &pNv->ModeReg;

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	xf86OutputPtr output = NVGetOutputFromCRTC(crtc);
	NVOutputPrivatePtr nv_output = output->driver_private;

	/*
	 * Extended RIVA registers.
	 */
	pixelDepth = (bpp + 1)/8;
	if(pNv->twoStagePLL)
		CalcVClock2Stage(dotClock, &VClk, &state->pll, &state->pllB, pNv);
	else
		CalcVClock(dotClock, &VClk, &state->pll, pNv);

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

	if (pNv->Architecture == NV_ARCH_40) {
		/* This register is only used on the primary ramdac */
		/* This seems to be needed to select the proper clocks, otherwise bad things happen */
		/* Assumption CRTC1 will overwrite the CRTC0 value */
		/* Also make sure we don't set both bits */
		state->sel_clk = (pNv->misc_info.sel_clk & ~(0xf << 16)) | (1 << 18);
		/* Are we a TMDS running on head 0(=ramdac 0), but native to ramdac 1? */
		if (nv_crtc->head == 0 && nv_output->type == OUTPUT_TMDS && nv_output->valid_ramdac & RAMDAC_1) {
			state->sel_clk = (pNv->misc_info.sel_clk & ~(0xf << 16)) | (1 << 16);
			state->crosswired = TRUE;
		} else if (nv_crtc->head == 0) {
			state->crosswired = FALSE;
		}

		/* Do not remove any present VPLL related bits, that can cause problems */
		/* The meaning of this register is debatable */
		if (!state->reg580) 
			state->reg580 = pNv->misc_info.ramdac_0_reg_580;

		/* For the moment changing the reg580 value causes probems, disable until i really know what's happening */
		Bool vpll1_ok = FALSE;
		Bool vpll2_ok = FALSE;

#if 0
		/* For lack of a better name */
		int magic_factor = (pNv->misc_info.sel_clk & (0xf << 8)) >> 8;

		/* A common situation on G70 cards, many seem to prefer DB1 vclk ratio */
		if ((state->reg580 & 0xff) == 0x3d) {
			switch(magic_factor) {
				case 1:
				case 2:
					vpll1_ok = FALSE;
					break;
				case 4:
					vpll1_ok = FALSE;
					vpll2_ok = FALSE;
					break;
				case 0:
				default:
					break;
			}
		}
#endif

		/* Vclk ratio DB1 is used whenever reg580 is modified for vpll activity */
		if (!(pNv->misc_info.ramdac_0_pllsel & NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2)) {
			ErrorF("We are a lover of the DB1 VCLK ratio\n");
			if (nv_crtc->head == 1 && vpll2_ok) {
				state->reg580 |= NV_RAMDAC_580_VPLL2_ACTIVE;
			} else if (nv_crtc->head == 0 && vpll1_ok) {
				state->reg580 |= NV_RAMDAC_580_VPLL1_ACTIVE;
			}
		}
	}

	/* We've bound crtc's and ramdac's together */
	if (nv_crtc->crtc == 1) {
		state->vpll2 = state->pll;
		state->vpll2B = state->pllB;
		if (pNv->misc_info.ramdac_0_pllsel & NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2) {
			state->pllsel |= NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2;
		} else {
			state->pllsel &= ~NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2;
		}
		state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL2;
	} else {
		state->vpll = state->pll;
		state->vpllB = state->pllB;
		if (pNv->Architecture < NV_ARCH_40)
			state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_ALL;
		else
			state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL;
		if (pNv->misc_info.ramdac_0_pllsel & NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2) {
			state->pllsel |= NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2;
		} else {
			state->pllsel &= ~NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2;
		}
	}

	/* The purpose is unknown */
	if (pNv->Architecture == NV_ARCH_40)
		state->pllsel |= (1 << 2);

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

	return TRUE;
}

static void
nv_crtc_mode_set_vga(xf86CrtcPtr crtc, DisplayModePtr mode)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVCrtcRegPtr regp;
	NVPtr pNv = NVPTR(pScrn);
	int depth = pScrn->depth;
	unsigned int i;

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

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
	regp->CRTC[0]  = (mode->CrtcHTotal >> 3) - 5;
	regp->CRTC[1]  = (mode->CrtcHDisplay >> 3) - 1;
	regp->CRTC[2]  = (mode->CrtcHBlankStart >> 3) - 1;
	regp->CRTC[3]  = (((mode->CrtcHBlankEnd >> 3) - 1) & 0x1F) | 0x80;
	i = (((mode->CrtcHSkew << 2) + 0x10) & ~0x1F);
	if (i < 0x80) {
		regp->CRTC[3] |= i;
	}
	regp->CRTC[4]  = (mode->CrtcHSyncStart >> 3);
	regp->CRTC[5]  = ((((mode->CrtcHBlankEnd >> 3) - 1) & 0x20) << 2)
	| (((mode->CrtcHSyncEnd >> 3)) & 0x1F);
	regp->CRTC[6]  = (mode->CrtcVTotal - 2) & 0xFF;
	regp->CRTC[7]  = (((mode->CrtcVTotal - 2) & 0x100) >> 8)
			| (((mode->CrtcVDisplay - 1) & 0x100) >> 7)
			| ((mode->CrtcVSyncStart & 0x100) >> 6)
			| (((mode->CrtcVBlankStart - 1) & 0x100) >> 5)
			| 0x10
			| (((mode->CrtcVTotal - 2) & 0x200)   >> 4)
			| (((mode->CrtcVDisplay - 1) & 0x200) >> 3)
			| ((mode->CrtcVSyncStart & 0x200) >> 2);
	regp->CRTC[8]  = 0x00;
	regp->CRTC[9]  = (((mode->CrtcVBlankStart - 1) & 0x200) >> 4) | 0x40;
	if (mode->Flags & V_DBLSCAN) {
		regp->CRTC[9] |= 0x80;
	}
	if (mode->VScan >= 32) {
		regp->CRTC[9] |= 0x1F;
	} else if (mode->VScan > 1) {
		regp->CRTC[9] |= mode->VScan - 1;
	}
	regp->CRTC[10] = 0x00;
	regp->CRTC[11] = 0x00;
	regp->CRTC[12] = 0x00;
	regp->CRTC[13] = 0x00;
	regp->CRTC[14] = 0x00;
	regp->CRTC[15] = 0x00;
	regp->CRTC[16] = mode->CrtcVSyncStart & 0xFF;
	regp->CRTC[17] = (mode->CrtcVSyncEnd & 0x0F) | 0x20;
	regp->CRTC[18] = (mode->CrtcVDisplay - 1) & 0xFF;
	regp->CRTC[19] = mode->CrtcHDisplay >> 4;  /* just a guess */
	regp->CRTC[20] = 0x00;
	regp->CRTC[21] = (mode->CrtcVBlankStart - 1) & 0xFF; 
	regp->CRTC[22] = (mode->CrtcVBlankEnd - 1) & 0xFF;
	/* 0x80 enables the sequencer, we don't want that */
	if (depth < 8) {
		regp->CRTC[23] = 0xE3 & ~0x80;
	} else {
		regp->CRTC[23] = 0xC3 & ~0x80;
	}
	regp->CRTC[24] = 0xFF;

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
  
	if (depth == 1) {
		/* Initialise the Mono map according to which bit-plane gets used */

		Bool flipPixels = xf86GetFlipPixels();

		for (i=0; i<16; i++) {
			if (((i & (1 << BIT_PLANE)) != 0) != flipPixels) {
				regp->Attribute[i] = WHITE_VALUE;
			} else {
				regp->Attribute[i] = BLACK_VALUE;
			}
		}

	} else {
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
		if (depth == 4) {
			regp->Attribute[16] = 0x81; /* wrong for the ET4000 */
		} else {
			regp->Attribute[16] = 0x41; /* wrong for the ET4000 */
		}
		if (depth > 4) {
			regp->Attribute[17] = 0xff;
		}
		/* Attribute[17] (overscan) initialised in vgaHWGetHWRec() */
	}
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
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVFBLayout *pLayout = &pNv->CurrentLayout;
	NVCrtcRegPtr regp, savep;
	unsigned int i;
	uint32_t clock = adjusted_mode->Clock;

#if 0
	/* Happily borrowed from haiku driver, as an extra safety */

	/* Make it multiples of 8 */
	mode->CrtcHDisplay &= ~7;
	mode->CrtcHSyncStart &= ~7;
	mode->CrtcHSyncEnd &= ~7;
	mode->CrtcHTotal &= ~7;

	/* Horizontal stuff */

	/* Time for some mode mangling */
	/* We only have 9 bits to store most of this information (mask 0x3f) */
	if (mode->CrtcHDisplay > MAX_H_VALUE(-2))
		mode->CrtcHDisplay = MAX_H_VALUE(-2);

	if (mode->CrtcHSyncStart > MAX_H_VALUE(-1))
		mode->CrtcHSyncStart = MAX_H_VALUE(-1);

	if  (mode->CrtcHSyncEnd > MAX_H_VALUE(0))
		mode->CrtcHSyncEnd = MAX_H_VALUE(0);

	if (mode->CrtcHTotal > MAX_H_VALUE(5))
		mode->CrtcHTotal = MAX_H_VALUE(5);

	/* Make room for a sync pulse if there is not enough room */
	if (mode->CrtcHTotal < mode->CrtcHSyncEnd + 0x50)
		mode->CrtcHTotal = mode->CrtcHSyncEnd + 0x50;

	/* Too large sync pulse? */
	if (mode->CrtcHTotal > mode->CrtcHSyncEnd + 0x3f8)
		mode->CrtcHTotal = mode->CrtcHSyncEnd + 0x3f8;

	/* Is the sync pulse outside the screen? */
	if (mode->CrtcHSyncEnd > mode->CrtcHTotal - 8)
		mode->CrtcHSyncEnd = mode->CrtcHTotal - 8;

	if (mode->CrtcHSyncStart < mode->CrtcHDisplay + 8)
		mode->CrtcHSyncStart = mode->CrtcHDisplay + 8;

	/* We've only got 5 bits to store the sync stuff */
	if (mode->CrtcHSyncEnd > mode->CrtcHSyncStart + (0x1f << 3))
		mode->CrtcHSyncEnd = mode->CrtcHSyncStart + (0x1f << 3);

	/* Vertical stuff */

	/* We've only got 12 bits for this stuff */
	if (mode->CrtcVDisplay > MAX_V_VALUE(-2))
		mode->CrtcVDisplay = MAX_V_VALUE(-2);

	if (mode->CrtcVSyncStart > MAX_V_VALUE(-1))
		mode->CrtcVSyncStart = MAX_V_VALUE(-1);

	if  (mode->CrtcVSyncEnd > MAX_V_VALUE(0))
		mode->CrtcVSyncEnd = MAX_V_VALUE(0);

	if (mode->CrtcVTotal > MAX_V_VALUE(5))
		mode->CrtcVTotal = MAX_V_VALUE(5);

	/* Make room for a sync pulse if there is not enough room */
	if (mode->CrtcVTotal < mode->CrtcVSyncEnd + 0x3)
		mode->CrtcVTotal = mode->CrtcVSyncEnd + 0x3;

	/* Too large sync pulse? */
	if (mode->CrtcVTotal > mode->CrtcVSyncEnd + 0xff)
		mode->CrtcVTotal = mode->CrtcVSyncEnd + 0xff;

	/* Is the sync pulse outside the screen? */
	if (mode->CrtcVSyncEnd > mode->CrtcVTotal - 1)
		mode->CrtcVSyncEnd = mode->CrtcVTotal - 1;

	if (mode->CrtcVSyncStart < mode->CrtcVDisplay + 1)
		mode->CrtcVSyncStart = mode->CrtcVDisplay + 1;

	/* We've only got 4 bits to store the sync stuff */
	if (mode->CrtcVSyncEnd > mode->CrtcVSyncStart + (0x0f << 0))
		mode->CrtcVSyncEnd = mode->CrtcVSyncStart + (0x0f << 0);
#endif

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

	xf86OutputPtr  output;
	NVOutputPrivatePtr nv_output;
	for (i = 0; i < xf86_config->num_output; i++) {
		output = xf86_config->output[i];
		nv_output = output->driver_private;

		if (output->crtc == crtc) {
			if ((nv_output->type == OUTPUT_LVDS) ||
				(nv_output->type == OUTPUT_TMDS)) {

				is_fp = TRUE;
				break;
			}
		}
	}

	ErrorF("Mode clock: %d\n", clock);

	ErrorF("crtc: Pre-sync workaround\n");
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
	ErrorF("crtc: Post-sync workaround\n");

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

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];    
	savep = &pNv->SavedReg.crtc_reg[nv_crtc->head];

	if(mode->Flags & V_INTERLACE) 
		vertTotal |= 1;

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
	regp->CRTC[NV_VGA_CRTCX_MAXSCLIN]  = SetBitField(vertBlankStart,9:9,5:5)
				| SetBit(6)
				| ((mode->Flags & V_DBLSCAN) ? 0x80 : 0x00);
	regp->CRTC[NV_VGA_CRTCX_VSYNCS] = Set8Bits(vertStart);
	regp->CRTC[NV_VGA_CRTCX_VSYNCE] = SetBitField(vertEnd,3:0,3:0) | SetBit(5);
	regp->CRTC[NV_VGA_CRTCX_VDISPE] = Set8Bits(vertDisplay);
	regp->CRTC[NV_VGA_CRTCX_PITCHL] = ((pScrn->displayWidth/8)*(pLayout->bitsPerPixel/8));
	regp->CRTC[NV_VGA_CRTCX_VBLANKS] = Set8Bits(vertBlankStart);
	regp->CRTC[NV_VGA_CRTCX_VBLANKE] = Set8Bits(vertBlankEnd);
	/* Not an extended register */
	regp->CRTC[NV_VGA_CRTCX_LINECOMP] = 0xff;

	regp->Attribute[0x10] = 0x01;
	/* Blob sets this for normal monitors as well */
	regp->Attribute[0x11] = 0x00;

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

	/* I'm trusting haiku driver on this one, they say it enables an external TDMS clock */
	if (is_fp) {
		regp->CRTC[NV_VGA_CRTCX_59] = 0x1;
	} else {
		regp->CRTC[NV_VGA_CRTCX_59] = 0x0;
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

	ErrorF("crtc %d %d %d\n", nv_crtc->crtc, mode->CrtcHDisplay, pScrn->displayWidth);
	nv_crtc_calc_state_ext(crtc,
				i,
				pScrn->displayWidth,
				mode->CrtcHDisplay,
				mode->CrtcVDisplay,
				clock,
				mode->Flags);

	/* Enable slaved mode */
	if (is_fp) {
		regp->CRTC[NV_VGA_CRTCX_PIXEL] |= (1 << 7);
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
		if (nv_crtc->head == 1) {
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

	regp->cursorConfig = 0x00000100;
	if(mode->Flags & V_DBLSCAN)
		regp->cursorConfig |= (1 << 4);
	if(pNv->alphaCursor) {
		if((pNv->Chipset & 0x0ff0) != CHIPSET_NV11) {
			regp->cursorConfig |= 0x04011000;
		} else {
			regp->cursorConfig |= 0x14011000;
		}
	} else {
		regp->cursorConfig |= 0x02000000;
	}

	/* Unblock some timings */
	regp->CRTC[NV_VGA_CRTCX_FP_HTIMING] = 0;
	regp->CRTC[NV_VGA_CRTCX_FP_VTIMING] = 0;

	/* 0x20 seems to be enabled and 0x14 disabled */
	regp->CRTC[NV_VGA_CRTCX_26] = 0x20;

	/* 0x00 is disabled, 0x22 crt and 0x88 dfp */
	/* 0x11 is LVDS? */
	if (is_fp) {
		regp->CRTC[NV_VGA_CRTCX_3B] = 0x88;
	} else {
		regp->CRTC[NV_VGA_CRTCX_3B] = 0x22;
	}

	/* These values seem to vary */
	regp->CRTC[NV_VGA_CRTCX_3C] = savep->CRTC[NV_VGA_CRTCX_3C];

	/* 0x80 seems to be used very often, if not always */
	regp->CRTC[NV_VGA_CRTCX_45] = 0x80;

	/* Are these(0x55 and 0x56) also timing related registers, since disabling them does nothing? */
	regp->CRTC[NV_VGA_CRTCX_55] = 0x0;

	/* Common values like 0x14 and 0x04 are converted to 0x10 and 0x00 */
	//regp->CRTC[NV_VGA_CRTCX_56] = savep->CRTC[NV_VGA_CRTCX_56] & ~(1<<4);
	regp->CRTC[NV_VGA_CRTCX_56] = 0x0;

	regp->CRTC[NV_VGA_CRTCX_57] = 0x0;

	/* bit0: Seems to be mostly used on crtc1 */
	/* bit1: 1=crtc1, 0=crtc, but i'm unsure about this */
	/* 0x7E (crtc0, only seen in one dump) and 0x7F (crtc1) seem to be some kind of disable setting */
	/* This is likely to be incomplete */
	/* This is a very strange register, changed very often by the blob */
	regp->CRTC[NV_VGA_CRTCX_58] = 0x0;

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

	nv_crtc_mode_set_vga(crtc, mode);
	nv_crtc_mode_set_regs(crtc, mode, adjusted_mode);

	/* Just in case */
	NVCrtcLockUnlock(crtc, FALSE);

	NVVgaProtect(crtc, TRUE);
	nv_crtc_load_state_pll(pNv, &pNv->ModeReg);
	nv_crtc_load_state_ext(crtc, &pNv->ModeReg);
	nv_crtc_load_state_vga(crtc, &pNv->ModeReg);

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
	nv_crtc_save_state_pll(pNv, &pNv->SavedReg);
	nv_crtc_save_state_vga(crtc, &pNv->SavedReg);
	nv_crtc_save_state_ext(crtc, &pNv->SavedReg);
}

void nv_crtc_restore(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	ErrorF("nv_crtc_restore is called for CRTC %d\n", nv_crtc->crtc);

	NVCrtcSetOwner(crtc);

	/* Just to be safe */
	NVCrtcLockUnlock(crtc, FALSE);

	NVVgaProtect(crtc, TRUE);
	nv_crtc_load_state_ext(crtc, &pNv->SavedReg);
	nv_crtc_load_state_vga(crtc, &pNv->SavedReg);
	nv_crtc_load_state_pll(pNv, &pNv->SavedReg);
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
	.set_cursor_colors = nv_crtc_set_cursor_colors,
	.set_cursor_position = nv_crtc_set_cursor_position,
	.show_cursor = nv_crtc_show_cursor,
	.hide_cursor = nv_crtc_hide_cursor,
	.load_cursor_argb = nv_crtc_load_cursor_argb,
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
  ScrnInfoPtr pScrn = crtc->scrn;
  NVPtr pNv = NVPTR(pScrn);
   
  if(pNv->Architecture == NV_ARCH_40) {  /* HW bug */
    volatile CARD32 curpos = nvReadCurRAMDAC(pNv, NV_RAMDAC_CURSOR_POS);
    nvWriteCurRAMDAC(pNv, NV_RAMDAC_CURSOR_POS, curpos);
  }

}
static void nv_crtc_load_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    NVPtr pNv = NVPTR(pScrn);    
    NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
    NVCrtcRegPtr regp;
    
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

	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FP_HTIMING, regp->CRTC[NV_VGA_CRTCX_FP_HTIMING]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FP_VTIMING, regp->CRTC[NV_VGA_CRTCX_FP_VTIMING]);

	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_26, regp->CRTC[NV_VGA_CRTCX_26]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_3B, regp->CRTC[NV_VGA_CRTCX_3B]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_3C, regp->CRTC[NV_VGA_CRTCX_3C]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_45, regp->CRTC[NV_VGA_CRTCX_45]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_52, regp->CRTC[NV_VGA_CRTCX_52]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_56, regp->CRTC[NV_VGA_CRTCX_56]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_57, regp->CRTC[NV_VGA_CRTCX_57]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_58, regp->CRTC[NV_VGA_CRTCX_58]);
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
	regp->CRTC[NV_VGA_CRTCX_52] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_52);
	regp->CRTC[NV_VGA_CRTCX_56] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_56);
	regp->CRTC[NV_VGA_CRTCX_57] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_57);
	regp->CRTC[NV_VGA_CRTCX_58] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_58);
	regp->CRTC[NV_VGA_CRTCX_59] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_59);
	regp->CRTC[NV_VGA_CRTCX_BUFFER] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_BUFFER);
	regp->CRTC[NV_VGA_CRTCX_FP_HTIMING] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FP_HTIMING);
	regp->CRTC[NV_VGA_CRTCX_FP_VTIMING] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FP_VTIMING);
    }
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
