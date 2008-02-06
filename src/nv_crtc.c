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
static void nv_crtc_load_state_palette(xf86CrtcPtr crtc, RIVA_HW_STATE *state);
static void nv_crtc_save_state_palette(xf86CrtcPtr crtc, RIVA_HW_STATE *state);

uint32_t NVReadCRTC(NVPtr pNv, uint8_t head, uint32_t reg)
{
	if (head)
		reg += NV_PCRTC0_SIZE;
	DDXMMIOH("NVReadCRTC: head %d reg %08x val %08x\n", head, reg, (uint32_t)MMIO_IN32(pNv->REGS, reg));
	return MMIO_IN32(pNv->REGS, reg);
}

void NVWriteCRTC(NVPtr pNv, uint8_t head, uint32_t reg, uint32_t val)
{
	if (head)
		reg += NV_PCRTC0_SIZE;
	DDXMMIOH("NVWriteCRTC: head %d reg %08x val %08x\n", head, reg, val);
	MMIO_OUT32(pNv->REGS, reg, val);
}

uint32_t NVCrtcReadCRTC(xf86CrtcPtr crtc, uint32_t reg)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	return NVReadCRTC(pNv, nv_crtc->head, reg);
}

void NVCrtcWriteCRTC(xf86CrtcPtr crtc, uint32_t reg, uint32_t val)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	NVWriteCRTC(pNv, nv_crtc->head, reg, val);
}

uint32_t NVReadRAMDAC(NVPtr pNv, uint8_t head, uint32_t reg)
{
	if (head)
		reg += NV_PRAMDAC0_SIZE;
	DDXMMIOH("NVReadRamdac: head %d reg %08x val %08x\n", head, reg, (uint32_t)MMIO_IN32(pNv->REGS, reg));
	return MMIO_IN32(pNv->REGS, reg);
}

void NVWriteRAMDAC(NVPtr pNv, uint8_t head, uint32_t reg, uint32_t val)
{
	if (head)
		reg += NV_PRAMDAC0_SIZE;
	DDXMMIOH("NVWriteRamdac: head %d reg %08x val %08x\n", head, reg, val);
	MMIO_OUT32(pNv->REGS, reg, val);
}

uint32_t NVCrtcReadRAMDAC(xf86CrtcPtr crtc, uint32_t reg)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	return NVReadRAMDAC(pNv, nv_crtc->head, reg);
}

void NVCrtcWriteRAMDAC(xf86CrtcPtr crtc, uint32_t reg, uint32_t val)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	NVWriteRAMDAC(pNv, nv_crtc->head, reg, val);
}

static uint8_t NVReadPVIO(xf86CrtcPtr crtc, uint32_t address)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	/* Only NV4x have two pvio ranges */
	if (nv_crtc->head == 1 && pNv->Architecture == NV_ARCH_40) {
		DDXMMIOH("NVReadPVIO: head %d reg %08x val %02x\n", 1, address + NV_PVIO_OFFSET + NV_PVIO_SIZE, NV_RD08(pNv->PVIO1, address));
		return NV_RD08(pNv->PVIO1, address);
	} else {
		DDXMMIOH("NVReadPVIO: head %d reg %08x val %02x\n", 0, address + NV_PVIO_OFFSET, NV_RD08(pNv->PVIO0, address));
		return NV_RD08(pNv->PVIO0, address);
	}
}

static void NVWritePVIO(xf86CrtcPtr crtc, uint32_t address, uint8_t value)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	/* Only NV4x have two pvio ranges */
	if (nv_crtc->head == 1 && pNv->Architecture == NV_ARCH_40) {
		DDXMMIOH("NVWritePVIO: head %d reg %08x val %02x\n", nv_crtc->head, address + NV_PVIO_OFFSET + NV_PVIO_SIZE, value);
		NV_WR08(pNv->PVIO1, address, value);
	} else {
		DDXMMIOH("NVWritePVIO: head %d reg %08x val %02x\n", nv_crtc->head, address + NV_PVIO_OFFSET, value);
		NV_WR08(pNv->PVIO0, address, value);
	}
}

void NVWriteVGA(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	volatile uint8_t *pCRTCReg = head ? pNv->PCIO1 : pNv->PCIO0;

	DDXMMIOH("NVWriteVGA: head %d index 0x%02x data 0x%02x\n", head, index, value);
	NV_WR08(pCRTCReg, CRTC_INDEX, index);
	NV_WR08(pCRTCReg, CRTC_DATA, value);
}

uint8_t NVReadVGA(NVPtr pNv, int head, uint8_t index)
{
	volatile uint8_t *pCRTCReg = head ? pNv->PCIO1 : pNv->PCIO0;

	NV_WR08(pCRTCReg, CRTC_INDEX, index);
	DDXMMIOH("NVReadVGA: head %d index 0x%02x data 0x%02x\n", head, index, NV_RD08(pCRTCReg, CRTC_DATA));
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
 * 0x03		bit0 set for dual link (LVDS, possibly elsewhere too)
 * 0x08 or 0x09	pxclk in MHz
 * 0x0f		laptop panel info -	low nibble for PEXTDEV_BOOT_0 strap
 * 					high nibble for xlat strap value
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

void NVWriteVgaCrtc(xf86CrtcPtr crtc, uint8_t index, uint8_t value)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	NVWriteVGA(pNv, nv_crtc->head, index, value);
}

uint8_t NVReadVgaCrtc(xf86CrtcPtr crtc, uint8_t index)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);

	return NVReadVGA(pNv, nv_crtc->head, index);
}

static void NVWriteVgaSeq(xf86CrtcPtr crtc, uint8_t index, uint8_t value)
{
	NVWritePVIO(crtc, VGA_SEQ_INDEX, index);
	NVWritePVIO(crtc, VGA_SEQ_DATA, value);
}

static uint8_t NVReadVgaSeq(xf86CrtcPtr crtc, uint8_t index)
{
	NVWritePVIO(crtc, VGA_SEQ_INDEX, index);
	return NVReadPVIO(crtc, VGA_SEQ_DATA);
}

static void NVWriteVgaGr(xf86CrtcPtr crtc, uint8_t index, uint8_t value)
{
	NVWritePVIO(crtc, VGA_GRAPH_INDEX, index);
	NVWritePVIO(crtc, VGA_GRAPH_DATA, value);
}

static uint8_t NVReadVgaGr(xf86CrtcPtr crtc, uint8_t index)
{
	NVWritePVIO(crtc, VGA_GRAPH_INDEX, index);
	return NVReadPVIO(crtc, VGA_GRAPH_DATA);
} 


static void NVWriteVgaAttr(xf86CrtcPtr crtc, uint8_t index, uint8_t value)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);
	volatile uint8_t *pCRTCReg = nv_crtc->head ? pNv->PCIO1 : pNv->PCIO0;

	DDXMMIOH("NVWriteVgaAttr: head %d reg 0x%04x data 0x%02x\n", nv_crtc->head, CRTC_IN_STAT_1, NV_RD08(pCRTCReg, CRTC_IN_STAT_1));
	NV_RD08(pCRTCReg, CRTC_IN_STAT_1);
	if (nv_crtc->paletteEnabled)
		index &= ~0x20;
	else
		index |= 0x20;

	DDXMMIOH("NVWriteVgaAttr: head %d index 0x%02x data 0x%02x\n", nv_crtc->head, index, value);
	NV_WR08(pCRTCReg, VGA_ATTR_INDEX, index);
	NV_WR08(pCRTCReg, VGA_ATTR_DATA_W, value);
}

static uint8_t NVReadVgaAttr(xf86CrtcPtr crtc, uint8_t index)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);
	volatile uint8_t *pCRTCReg = nv_crtc->head ? pNv->PCIO1 : pNv->PCIO0;

	DDXMMIOH("NVReadVgaAttr: head %d reg 0x%04x data 0x%02x\n", nv_crtc->head, CRTC_IN_STAT_1, NV_RD08(pCRTCReg, CRTC_IN_STAT_1));
	NV_RD08(pCRTCReg, CRTC_IN_STAT_1);
	if (nv_crtc->paletteEnabled)
		index &= ~0x20;
	else
		index |= 0x20;

	NV_WR08(pCRTCReg, VGA_ATTR_INDEX, index);
	DDXMMIOH("NVReadVgaAttr: head %d index 0x%02x data 0x%02x\n", nv_crtc->head, index, NV_RD08(pCRTCReg, VGA_ATTR_DATA_R));
	return NV_RD08(pCRTCReg, VGA_ATTR_DATA_R);
}

void NVSetOwner(ScrnInfoPtr pScrn, uint8_t head)
{
	NVPtr pNv = NVPTR(pScrn);
	/* CRTCX_OWNER is always changed on CRTC0 */
	NVWriteVGA(pNv, 0, NV_VGA_CRTCX_OWNER, head*0x3);

	ErrorF("Setting owner: 0x%X\n", head*0x3);
}

static void NVCrtcSetOwner(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	/* CRTCX_OWNER is always changed on CRTC0 */
	NVSetOwner(pScrn, nv_crtc->head);
}

static void
NVEnablePalette(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);
	volatile uint8_t *pCRTCReg = nv_crtc->head ? pNv->PCIO1 : pNv->PCIO0;

	DDXMMIOH("NVEnablePalette: head %d reg 0x%04x data 0x%02x\n", nv_crtc->head, CRTC_IN_STAT_1, NV_RD08(pCRTCReg, CRTC_IN_STAT_1));
	NV_RD08(pCRTCReg, CRTC_IN_STAT_1);
	DDXMMIOH("NVEnablePalette: head %d reg 0x%04x data 0x%02x\n", nv_crtc->head, VGA_ATTR_INDEX, 0);
	NV_WR08(pCRTCReg, VGA_ATTR_INDEX, 0);
	nv_crtc->paletteEnabled = TRUE;
}

static void
NVDisablePalette(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);
	volatile uint8_t *pCRTCReg = nv_crtc->head ? pNv->PCIO1 : pNv->PCIO0;

	DDXMMIOH("NVDisablePalette: head %d reg 0x%04x data 0x%02x\n", nv_crtc->head, NV_PCIO0_OFFSET + (nv_crtc->head ? NV_PCIO0_SIZE : 0) + CRTC_IN_STAT_1, NV_RD08(pCRTCReg, CRTC_IN_STAT_1));
	NV_RD08(pCRTCReg, CRTC_IN_STAT_1);
	DDXMMIOH("NVDisablePalette: head %d reg 0x%04x data 0x%02x\n", nv_crtc->head, NV_PCIO0_OFFSET + (nv_crtc->head ? NV_PCIO0_SIZE : 0) + VGA_ATTR_INDEX, 0x20);
	NV_WR08(pCRTCReg, VGA_ATTR_INDEX, 0x20);
	nv_crtc->paletteEnabled = FALSE;
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
	uint8_t tmp;

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

void NVLockUnlockHead(ScrnInfoPtr pScrn, uint8_t head, Bool lock)
{
	NVPtr pNv = NVPTR(pScrn);
	uint8_t cr11;

	if (pNv->twoHeads)
		NVSetOwner(pScrn, head);

	NVWriteVGA(pNv, head, NV_VGA_CRTCX_LOCK, lock ? 0x99 : 0x57);
	cr11 = NVReadVGA(pNv, head, NV_VGA_CRTCX_VSYNCE);
	if (lock) cr11 |= 0x80;
	else cr11 &= ~0x80;
	NVWriteVGA(pNv, head, NV_VGA_CRTCX_VSYNCE, cr11);
}

void NVCrtcLockUnlock(xf86CrtcPtr crtc, Bool lock)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ScrnInfoPtr pScrn = crtc->scrn;

	NVLockUnlockHead(pScrn, nv_crtc->head, lock);
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
		if (nv_crtc->head == index)
			return crtc;
	}

	return NULL;
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

static void nv40_crtc_save_state_pll(NVPtr pNv, RIVA_HW_STATE *state)
{
	state->vpll1_a = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL);
	state->vpll1_b = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B);
	state->vpll2_a = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2);
	state->vpll2_b = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B);
	state->pllsel = NVReadRAMDAC(pNv, 0, NV_RAMDAC_PLL_SELECT);
	state->sel_clk = NVReadRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK);
	state->reg580 = NVReadRAMDAC(pNv, 0, NV_RAMDAC_580);
	state->reg594 = NVReadRAMDAC(pNv, 0, NV_RAMDAC_594);
}

static void nv40_crtc_load_state_pll(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	uint32_t fp_debug_0[2];
	uint32_t index[2];
	fp_debug_0[0] = NVReadRAMDAC(pNv, 0, NV_RAMDAC_FP_DEBUG_0);
	fp_debug_0[1] = NVReadRAMDAC(pNv, 1, NV_RAMDAC_FP_DEBUG_0);

	/* The TMDS_PLL switch is on the actual ramdac */
	if (state->crosswired) {
		index[0] = 1;
		index[1] = 0;
		ErrorF("Crosswired pll state load\n");
	} else {
		index[0] = 0;
		index[1] = 1;
	}

	if (state->vpll2_b && state->vpll_changed[1]) {
		NVWriteRAMDAC(pNv, index[1], NV_RAMDAC_FP_DEBUG_0,
			fp_debug_0[index[1]] | NV_RAMDAC_FP_DEBUG_0_PWRDOWN_TMDS_PLL);

		/* Wait for the situation to stabilise */
		usleep(5000);

		uint32_t reg_c040 = pNv->misc_info.reg_c040;
		/* for vpll2 change bits 18 and 19 are disabled */
		reg_c040 &= ~(0x3 << 18);
		nvWriteMC(pNv, 0xc040, reg_c040);

		ErrorF("writing vpll2_a %08X\n", state->vpll2_a);
		ErrorF("writing vpll2_b %08X\n", state->vpll2_b);

		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL2, state->vpll2_a);
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B, state->vpll2_b);

		ErrorF("writing pllsel %08X\n", state->pllsel);
		/* Don't turn vpll1 off. */
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_PLL_SELECT, state->pllsel);

		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_580, state->reg580);
		ErrorF("writing reg580 %08X\n", state->reg580);

		/* We need to wait a while */
		usleep(5000);
		nvWriteMC(pNv, 0xc040, pNv->misc_info.reg_c040);

		NVWriteRAMDAC(pNv, index[1], NV_RAMDAC_FP_DEBUG_0, fp_debug_0[index[1]]);

		/* Wait for the situation to stabilise */
		usleep(5000);
	}

	if (state->vpll1_b && state->vpll_changed[0]) {
		NVWriteRAMDAC(pNv, index[0], NV_RAMDAC_FP_DEBUG_0,
			fp_debug_0[index[0]] | NV_RAMDAC_FP_DEBUG_0_PWRDOWN_TMDS_PLL);

		/* Wait for the situation to stabilise */
		usleep(5000);

		uint32_t reg_c040 = pNv->misc_info.reg_c040;
		/* for vpll2 change bits 16 and 17 are disabled */
		reg_c040 &= ~(0x3 << 16);
		nvWriteMC(pNv, 0xc040, reg_c040);

		ErrorF("writing vpll1_a %08X\n", state->vpll1_a);
		ErrorF("writing vpll1_b %08X\n", state->vpll1_b);

		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL, state->vpll1_a);
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B, state->vpll1_b);

		ErrorF("writing pllsel %08X\n", state->pllsel);
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_PLL_SELECT, state->pllsel);

		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_580, state->reg580);
		ErrorF("writing reg580 %08X\n", state->reg580);

		/* We need to wait a while */
		usleep(5000);
		nvWriteMC(pNv, 0xc040, pNv->misc_info.reg_c040);

		NVWriteRAMDAC(pNv, index[0], NV_RAMDAC_FP_DEBUG_0, fp_debug_0[index[0]]);

		/* Wait for the situation to stabilise */
		usleep(5000);
	}

	ErrorF("writing sel_clk %08X\n", state->sel_clk);
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK, state->sel_clk);

	ErrorF("writing reg594 %08X\n", state->reg594);
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_594, state->reg594);

	/* All clocks have been set at this point. */
	state->vpll_changed[0] = FALSE;
	state->vpll_changed[1] = FALSE;
}

static void nv_crtc_save_state_pll(NVPtr pNv, RIVA_HW_STATE *state)
{
	state->vpll1_a = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL);
	if (pNv->twoHeads) {
		state->vpll2_a = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2);
	}
	if (pNv->twoStagePLL && pNv->NVArch != 0x30) {
		state->vpll1_b = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B);
		state->vpll2_b = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B);
	}
	state->pllsel = NVReadRAMDAC(pNv, 0, NV_RAMDAC_PLL_SELECT);
	state->sel_clk = NVReadRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK);
}


static void nv_crtc_load_state_pll(NVPtr pNv, RIVA_HW_STATE *state)
{
	/* This sequence is important, the NV28 is very sensitive in this area. */
	/* Keep pllsel last and sel_clk first. */
	ErrorF("writing sel_clk %08X\n", state->sel_clk);
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK, state->sel_clk);

	if (state->vpll2_a && state->vpll_changed[1]) {
		if (pNv->twoHeads) {
			ErrorF("writing vpll2_a %08X\n", state->vpll2_a);
			NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL2, state->vpll2_a);
		}
		if (pNv->twoStagePLL && pNv->NVArch != 0x30) {
			ErrorF("writing vpll2_b %08X\n", state->vpll2_b);
			NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B, state->vpll2_b);
		}
	}

	if (state->vpll1_a && state->vpll_changed[0]) {
		ErrorF("writing vpll1_a %08X\n", state->vpll1_a);
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL, state->vpll1_a);
		if (pNv->twoStagePLL && pNv->NVArch != 0x30) {
			ErrorF("writing vpll1_b %08X\n", state->vpll1_b);
			NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B, state->vpll1_b);
		}
	}

	ErrorF("writing pllsel %08X\n", state->pllsel);
	NVWriteRAMDAC(pNv, 0, NV_RAMDAC_PLL_SELECT, state->pllsel);

	/* All clocks have been set at this point. */
	state->vpll_changed[0] = FALSE;
	state->vpll_changed[1] = FALSE;
}

static void nv_crtc_mode_set_sel_clk(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(crtc->scrn);
	xf86OutputPtr output = NVGetOutputFromCRTC(crtc);
	NVOutputPrivatePtr nv_output;
	int i;

	/* Don't change SEL_CLK on NV0x/NV1x/NV2x cards */
	if (pNv->Architecture < NV_ARCH_30) {
		state->sel_clk = pNv->misc_info.sel_clk;
		return;
	}

	/* SEL_CLK is only used on the primary ramdac */
	/* This seems to be needed to select the proper clocks, otherwise bad things happen */
	if (!state->sel_clk)
		state->sel_clk = pNv->misc_info.sel_clk & ~(0xf << 16);

	if (!output)
		return;
	nv_output = output->driver_private;

	/* Only let digital outputs mess further with SEL_CLK, otherwise strange output routings may mess it up. */
	if (nv_output->type == OUTPUT_TMDS || nv_output->type == OUTPUT_LVDS) {
		Bool crossed_clocks = nv_output->preferred_output ^ nv_crtc->head;

		state->sel_clk &= ~(0xf << 16);
		/* Even with two dvi, this should not conflict. */
		if (crossed_clocks)
			state->sel_clk |= (0x1 << 16);
		else
			state->sel_clk |= (0x4 << 16);

		/* nv30:
		 *	bit 0		NVClk spread spectrum on/off
		 *	bit 2		MemClk spread spectrum on/off
		 *	bit 4		PixClk1 spread spectrum on/off
		 *	bit 6		PixClk2 spread spectrum on/off
		 *
		 * nv40 (observations from bios behaviour and mmio traces):
		 * 	bit 4		seems to get set when output is on head A - likely related to PixClk1
		 * 	bit 6		seems to get set when output is on head B - likely related to PixClk2
		 * 	bits 5&7	set as for bits 4&6, but do not appear on cards using 4&6
		 *
		 * 	bits 8&10	seen on dual dvi outputs; possibly means "bits 4&6, dual dvi"
		 *
		 * 	Note that the circumstances for setting the bits at all is unclear
		 */
		if (pNv->Architecture == NV_ARCH_40) {
			for (i = 1; i <= 2; i++) {
				uint32_t var = (state->sel_clk >> 4*i) & 0xf;
				int shift = 0; /* assume (var & 0x5) by default */

				if (!var)
					continue;
				if (var & 0xa)
					shift = 1;

				state->sel_clk &= ~(0xf << 4*i);
				if (crossed_clocks)
					state->sel_clk |= (0x4 << (4*i + shift));
				else
					state->sel_clk |= (0x1 << (4*i + shift));
			}
		}
	}
}

/*
 * Calculate extended mode parameters (SVGA) and save in a 
 * mode state structure.
 * State is not specific to a single crtc, but shared.
 */
void nv_crtc_calc_state_ext(
	xf86CrtcPtr 		crtc,
	DisplayModePtr	mode,
	int				bpp,
	int				DisplayWidth, /* Does this change after setting the mode? */
	int				CrtcHDisplay,
	int				CrtcVDisplay,
	int				dotClock,
	int				flags
)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	uint32_t pixelDepth, VClk = 0;
	uint32_t CursorStart;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	NVCrtcRegPtr regp;
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state;
	int num_crtc_enabled, i;
	uint32_t old_clock_a = 0, old_clock_b = 0;
	struct pll_lims pll_lim;
	int NM1 = 0xbeef, NM2 = 0xdead, log2P = 0;
	uint32_t g70_pll_special_bits = 0;
	Bool nv4x_single_stage_pll_mode = FALSE;

	state = &pNv->ModeReg;

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];

	xf86OutputPtr output = NVGetOutputFromCRTC(crtc);
	NVOutputPrivatePtr nv_output = NULL;
	if (output)
		nv_output = output->driver_private;

	/* Store old clock. */
	if (nv_crtc->head == 1) {
		old_clock_a = state->vpll2_a;
		old_clock_b = state->vpll2_b;
	} else {
		old_clock_a = state->vpll1_a;
		old_clock_b = state->vpll1_b;
	}

	/*
	 * Extended RIVA registers.
	 */
	/* This is pitch related, not mode related. */
	pixelDepth = (bpp + 1)/8;

	if (nv_crtc->head == 0) {
		if (!get_pll_limits(pScrn, VPLL1, &pll_lim))
			return;
	} else
		if (!get_pll_limits(pScrn, VPLL2, &pll_lim))
			return;

	if (pNv->twoStagePLL) {
		if (dotClock < pll_lim.vco1.maxfreq && pNv->NVArch > 0x40) { /* use a single VCO */
			nv4x_single_stage_pll_mode = TRUE;
			/* Turn the second set of divider and multiplier off */
			/* Bogus data, the same nvidia uses */
			NM2 = 0x11f;
			VClk = getMNP_single(pScrn, &pll_lim, dotClock, &NM1, &log2P);
		} else
			VClk = getMNP_double(pScrn, &pll_lim, dotClock, &NM1, &NM2, &log2P);
	} else
		VClk = getMNP_single(pScrn, &pll_lim, dotClock, &NM1, &log2P);

	/* Are these all the (relevant) G70 cards? */
	if (pNv->NVArch == 0x4B || pNv->NVArch == 0x46 || pNv->NVArch == 0x47 || pNv->NVArch == 0x49) {
		/* This is a big guess, but should be reasonable until we can narrow it down. */
		/* What exactly are the purpose of the upper 2 bits of pll_a and pll_b? */
		if (nv4x_single_stage_pll_mode)
			g70_pll_special_bits = 0x1;
		else
			g70_pll_special_bits = 0x3;
	}

	if (pNv->NVArch == 0x30)
		/* See nvregisters.xml for details. */
		state->pll = log2P << 16 | NM1 | (NM2 & 7) << 4 | ((NM2 >> 8) & 7) << 19 | ((NM2 >> 11) & 3) << 24 | NV30_RAMDAC_ENABLE_VCO2;
	else
		state->pll = g70_pll_special_bits << 30 | log2P << 16 | NM1;
	state->pllB = NV31_RAMDAC_ENABLE_VCO2 | NM2;

	/* Does register 0x580 already have a value? */
	if (!state->reg580)
		state->reg580 = pNv->misc_info.ramdac_0_reg_580;
	if (nv4x_single_stage_pll_mode) {
		if (nv_crtc->head == 0)
			state->reg580 |= NV_RAMDAC_580_VPLL1_ACTIVE;
		else
			state->reg580 |= NV_RAMDAC_580_VPLL2_ACTIVE;
	} else {
		if (nv_crtc->head == 0)
			state->reg580 &= ~NV_RAMDAC_580_VPLL1_ACTIVE;
		else
			state->reg580 &= ~NV_RAMDAC_580_VPLL2_ACTIVE;
	}

	if (!pNv->twoStagePLL || nv4x_single_stage_pll_mode)
		ErrorF("vpll: n %d m %d log2p %d\n", NM1 >> 8, NM1 & 0xff, log2P);
	else
		ErrorF("vpll: n1 %d n2 %d m1 %d m2 %d log2p %d\n", NM1 >> 8, NM2 >> 8, NM1 & 0xff, NM2 & 0xff, log2P);

	if (nv_crtc->head == 1) {
		state->vpll2_a = state->pll;
		state->vpll2_b = state->pllB;
	} else {
		state->vpll1_a = state->pll;
		state->vpll1_b = state->pllB;
	}

	/* always reset vpll, just to be sure. */
	state->vpll_changed[nv_crtc->head] = TRUE;

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
		state->pllsel |= NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2 | NV_RAMDAC_PLL_SELECT_PLL_SOURCE_ALL; 
		state->config = 0x00001114;
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

		if (nv_crtc->head == 1) {
			CursorStart = pNv->Cursor2->offset;
		} else {
			CursorStart = pNv->Cursor->offset;
		}

		if (!NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
			regp->CRTC[NV_VGA_CRTCX_CURCTL0] = 0x80 | (CursorStart >> 17);
			regp->CRTC[NV_VGA_CRTCX_CURCTL1] = (CursorStart >> 11) << 2;
			regp->CRTC[NV_VGA_CRTCX_CURCTL2] = CursorStart >> 24;
		} else {
			regp->CRTC[NV_VGA_CRTCX_CURCTL0] = 0x0;
			regp->CRTC[NV_VGA_CRTCX_CURCTL1] = 0x0;
			regp->CRTC[NV_VGA_CRTCX_CURCTL2] = 0x0;
		}

		if (flags & V_DBLSCAN) 
			regp->CRTC[NV_VGA_CRTCX_CURCTL1] |= 2;

		state->config   = nvReadFB(pNv, NV_PFB_CFG0);
		regp->CRTC[NV_VGA_CRTCX_REPAINT1] = CrtcHDisplay < 1280 ? 0x04 : 0x00;
		break;
	}

	if (NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
		/* This is a bit of a guess. */
		regp->CRTC[NV_VGA_CRTCX_REPAINT1] |= 0xB8;
	}

	/* okay do we have 2 CRTCs running ? */
	num_crtc_enabled = 0;
	for (i = 0; i < xf86_config->num_crtc; i++) {
		if (xf86_config->crtc[i]->enabled) {
			num_crtc_enabled++;
		}
	}

	ErrorF("There are %d CRTC's enabled\n", num_crtc_enabled);

	/* Are we crosswired? */
	if (output && nv_crtc->head != nv_output->preferred_output) {
		state->crosswired = TRUE;
	} else
		state->crosswired = FALSE;

	/* The NV40 seems to have more similarities to NV3x than other cards. */
	if (pNv->NVArch < 0x41) {
		state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_NVPLL;
		state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_MPLL;
	}

	if (nv_crtc->head == 1) {
		if (!nv4x_single_stage_pll_mode) {
			state->pllsel |= NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2;
		} else {
			state->pllsel &= ~NV_RAMDAC_PLL_SELECT_VCLK2_RATIO_DB2;
		}
		state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL2;
	} else {
		if (!nv4x_single_stage_pll_mode) {
			state->pllsel |= NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2;
		} else {
			state->pllsel &= ~NV_RAMDAC_PLL_SELECT_VCLK_RATIO_DB2;
		}
		state->pllsel |= NV_RAMDAC_PLL_SELECT_PLL_SOURCE_VPLL;
	}

	/* The blob uses this always, so let's do the same */
	if (pNv->Architecture == NV_ARCH_40) {
		state->pllsel |= NV_RAMDAC_PLL_SELECT_USE_VPLL2_TRUE;
	}

	/* The primary output resource doesn't seem to care */
	if (output && pNv->Architecture == NV_ARCH_40 && nv_output->output_resource == 1) { /* This is the "output" */
		/* non-zero values are for analog, don't know about tv-out and the likes */
		if (output && nv_output->type != OUTPUT_ANALOG) {
			state->reg594 = 0x0;
		} else if (output) {
			/* Are we a flexible output? */
			if (ffs(pNv->dcb_table.entry[nv_output->dcb_entry].or) & OUTPUT_0) {
				state->reg594 = 0x1;
				pNv->restricted_mode = FALSE;
			} else {
				state->reg594 = 0x0;
				pNv->restricted_mode = TRUE;
			}

			/* More values exist, but they seem related to the 3rd dac (tv-out?) somehow */
			/* bit 16-19 are bits that are set on some G70 cards */
			/* Those bits are also set to the 3rd OUTPUT register */
			if (nv_crtc->head == 1) {
				state->reg594 |= 0x100;
			}
		}
	}

	regp->CRTC[NV_VGA_CRTCX_FIFO0] = state->arbitration0;
	regp->CRTC[NV_VGA_CRTCX_FIFO_LWM] = state->arbitration1 & 0xff;
	if (pNv->Architecture >= NV_ARCH_30) {
		regp->CRTC[NV_VGA_CRTCX_FIFO_LWM_NV30] = state->arbitration1 >> 8;
	}

	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->CRTC[NV_VGA_CRTCX_REPAINT0] = ((CrtcHDisplay/16) & 0x700) >> 3;
	} else if (NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
		regp->CRTC[NV_VGA_CRTCX_REPAINT0] = (((CrtcHDisplay*bpp)/64) & 0x700) >> 3;
	} else { /* framebuffer can be larger than crtc scanout area. */
		regp->CRTC[NV_VGA_CRTCX_REPAINT0] = (((DisplayWidth/8) * pixelDepth) & 0x700) >> 3;
	}
	regp->CRTC[NV_VGA_CRTCX_PIXEL] = (pixelDepth > 2) ? 3 : pixelDepth;
}

static void
nv_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	ErrorF("nv_crtc_dpms is called for CRTC %d with mode %d\n", nv_crtc->head, mode);

	if (nv_crtc->last_dpms == mode) /* Don't do unnecesary mode changes. */
		return;

	nv_crtc->last_dpms = mode;

	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	unsigned char seq1 = 0, crtc17 = 0;
	unsigned char crtc1A;

	if (pNv->twoHeads)
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
	ErrorF("nv_crtc_mode_fixup is called for CRTC %d\n", nv_crtc->head);

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

	/* This is pitch/memory size related. */
	if (NVMatchModePrivate(mode, NV_MODE_CONSOLE))
		depth = pNv->console_mode[nv_crtc->head].bpp;

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

	xf86OutputPtr output = NVGetOutputFromCRTC(crtc);
	NVOutputPrivatePtr nv_output = NULL;
	if (output)
		nv_output = output->driver_private;

	ErrorF("Mode clock: %d\n", mode->Clock);
	ErrorF("Adjusted mode clock: %d\n", adjusted_mode->Clock);

	/* Reverted to what nv did, because that works for all resolutions on flatpanels */
	if (output && (nv_output->type == OUTPUT_LVDS || nv_output->type == OUTPUT_TMDS)) {
		vertStart = vertTotal - 3;  
		vertEnd = vertTotal - 2;
		vertBlankStart = vertStart;
		horizStart = horizTotal - 5;
		horizEnd = horizTotal - 2;
		horizBlankEnd = horizTotal + 4;
		if (pNv->overlayAdaptor && pNv->Architecture >= NV_ARCH_10) {
			/* This reportedly works around Xv some overlay bandwidth problems*/
			horizTotal += 2;
		}
	}

	if (mode->Flags & V_INTERLACE) 
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
	regp->Sequencer[0] = 0x00;
	/* 0x20 disables the sequencer */
	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		if (mode->HDisplay == 720) {
			regp->Sequencer[1] = 0x21; /* enable 9/8 mode */
		} else {
			regp->Sequencer[1] = 0x20;
		}
	} else {
		if (mode->Flags & V_CLKDIV2) {
			regp->Sequencer[1] = 0x29;
		} else {
			regp->Sequencer[1] = 0x21;
		}
	}
	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->Sequencer[2] = 0x03; /* select 2 out of 4 planes */
	} else {
		regp->Sequencer[2] = 0x0F;
	}
	regp->Sequencer[3] = 0x00;                     /* Font select */
	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->Sequencer[4] = 0x02;
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
				| ((mode->Flags & V_DBLSCAN) ? 0x80 : 0x00)
				| (NVMatchModePrivate(mode, NV_MODE_VGA) ? 0xF : 0x00); /* 8x15 chars */
	if (NVMatchModePrivate(mode, NV_MODE_VGA)) { /* Were do these cursor offsets come from? */
		regp->CRTC[NV_VGA_CRTCX_VGACURSTART] = 0xD; /* start scanline */
		regp->CRTC[NV_VGA_CRTCX_VGACUREND] = 0xE; /* end scanline */
	} else {
		regp->CRTC[NV_VGA_CRTCX_VGACURSTART] = 0x00;
		regp->CRTC[NV_VGA_CRTCX_VGACUREND] = 0x00;
	}
	regp->CRTC[NV_VGA_CRTCX_FBSTADDH] = 0x00;
	regp->CRTC[NV_VGA_CRTCX_FBSTADDL] = 0x00;
	regp->CRTC[0xe] = 0x00;
	regp->CRTC[0xf] = 0x00;
	regp->CRTC[NV_VGA_CRTCX_VSYNCS] = Set8Bits(vertStart);
	/* What is the meaning of bit5, it is empty in the vga spec. */
	regp->CRTC[NV_VGA_CRTCX_VSYNCE] = SetBitField(vertEnd,3:0,3:0) |
									(NVMatchModePrivate(mode, NV_MODE_VGA) ? 0 : SetBit(5));
	regp->CRTC[NV_VGA_CRTCX_VDISPE] = Set8Bits(vertDisplay);
	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->CRTC[NV_VGA_CRTCX_PITCHL] = (mode->CrtcHDisplay/16);
	} else if (NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
		regp->CRTC[NV_VGA_CRTCX_PITCHL] = ((mode->CrtcHDisplay*depth)/64);
	} else { /* framebuffer can be larger than crtc scanout area. */
		regp->CRTC[NV_VGA_CRTCX_PITCHL] = ((pScrn->displayWidth/8)*(pLayout->bitsPerPixel/8));
	}
	if (depth == 4) { /* How can these values be calculated? */
		regp->CRTC[NV_VGA_CRTCX_UNDERLINE] = 0x1F;
	} else {
		regp->CRTC[NV_VGA_CRTCX_UNDERLINE] = 0x00;
	}
	regp->CRTC[NV_VGA_CRTCX_VBLANKS] = Set8Bits(vertBlankStart);
	regp->CRTC[NV_VGA_CRTCX_VBLANKE] = Set8Bits(vertBlankEnd);
	/* 0x80 enables the sequencer, we don't want that */
	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->CRTC[NV_VGA_CRTCX_MODECTL] = 0xA3 & ~0x80;
	} else if (NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
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
	* Graphics Display Controller
	*/
	regp->Graphics[0] = 0x00;
	regp->Graphics[1] = 0x00;
	regp->Graphics[2] = 0x00;
	regp->Graphics[3] = 0x00;
	regp->Graphics[4] = 0x00;
	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->Graphics[5] = 0x10;
		regp->Graphics[6] = 0x0E; /* map 32k mem */
		regp->Graphics[7] = 0x00;
	} else {
		regp->Graphics[5] = 0x40; /* 256 color mode */
		regp->Graphics[6] = 0x05; /* map 64k mem + graphic mode */
		regp->Graphics[7] = 0x0F;
	}
	regp->Graphics[8] = 0xFF;

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
	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->Attribute[16] = 0x0C; /* Line Graphics Enable + Blink enable */
	} else {
		regp->Attribute[16] = 0x01; /* Enable graphic mode */
	}
	/* Non-vga */
	regp->Attribute[17] = 0x00;
	regp->Attribute[18] = 0x0F; /* enable all color planes */
	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->Attribute[19] = 0x08; /* shift bits by 8 */
	} else {
		regp->Attribute[19] = 0x00;
	}
	regp->Attribute[20] = 0x00;
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
nv_crtc_mode_set_regs(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVFBLayout *pLayout = &pNv->CurrentLayout;
	NVCrtcRegPtr regp, savep;
	uint32_t i, depth;
	Bool is_fp = FALSE;
	Bool is_lvds = FALSE;

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];    
	savep = &pNv->SavedReg.crtc_reg[nv_crtc->head];

	xf86OutputPtr output = NVGetOutputFromCRTC(crtc);
	NVOutputPrivatePtr nv_output = NULL;
	if (output) {
		nv_output = output->driver_private;

		if ((nv_output->type == OUTPUT_LVDS) || (nv_output->type == OUTPUT_TMDS))
			is_fp = TRUE;

		if (nv_output->type == OUTPUT_LVDS)
			is_lvds = TRUE;
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
			regp->CRTC[NV_VGA_CRTCX_LCD] |= (1 << 0);
			if (!NVMatchModePrivate(mode, NV_MODE_VGA)) {
				regp->CRTC[NV_VGA_CRTCX_LCD] |= (1 << 1);
			}
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
	* Will only be written when depth != 8.
	*/
	for (i = 0; i < 256; i++) {
		regp->DAC[i*3] = i;
		regp->DAC[(i*3)+1] = i;
		regp->DAC[(i*3)+2] = i;
	}

	/*
	* Calculate the extended registers.
	*/

	if (pLayout->depth < 24) {
		depth = pLayout->depth;
	} else {
		depth = 32;
	}

	if (NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
		/* bpp is pitch related. */
		depth = pNv->console_mode[nv_crtc->head].bpp;
	}

	/* What is the meaning of this register? */
	/* A few popular values are 0x18, 0x1c, 0x38, 0x3c */ 
	regp->CRTC[NV_VGA_CRTCX_FIFO1] = savep->CRTC[NV_VGA_CRTCX_FIFO1] & ~(1<<5);

	regp->head = 0;

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
		/* Most G70 cards have FPP2 set on the secondary CRTC. */
		if (nv_crtc->head == 1 && pNv->NVArch > 0x44) {
			regp->head |= NV_CRTC_FSEL_FPP2;
		}
	}

	/* Except for rare conditions I2C is enabled on the primary crtc */
	if (nv_crtc->head == 0) {
		regp->head |= NV_CRTC_FSEL_I2C;
	}

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
	if (pNv->alphaCursor && !NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
		regp->cursorConfig |= 	(NV_CRTC_CURSOR_CONFIG_32BPP |
							NV_CRTC_CURSOR_CONFIG_64PIXELS |
							NV_CRTC_CURSOR_CONFIG_64LINES |
							NV_CRTC_CURSOR_CONFIG_ALPHA_BLEND);
	} else {
		regp->cursorConfig |= NV_CRTC_CURSOR_CONFIG_32LINES;
	}

	/* Unblock some timings */
	regp->CRTC[NV_VGA_CRTCX_FP_HTIMING] = 0;
	regp->CRTC[NV_VGA_CRTCX_FP_VTIMING] = 0;

	/* What is the purpose of this register? */
	/* 0x14 may be disabled? */
	regp->CRTC[NV_VGA_CRTCX_26] = 0x20;

	/* 0x00 is disabled, 0x11 is lvds, 0x22 crt and 0x88 tmds */
	if (is_lvds) {
		regp->CRTC[NV_VGA_CRTCX_3B] = 0x11;
	} else if (is_fp) {
		regp->CRTC[NV_VGA_CRTCX_3B] = 0x88;
	} else {
		regp->CRTC[NV_VGA_CRTCX_3B] = 0x22;
	}

	/* These values seem to vary */
	/* This register seems to be used by the bios to make certain decisions on some G70 cards? */
	regp->CRTC[NV_VGA_CRTCX_SCRATCH4] = savep->CRTC[NV_VGA_CRTCX_SCRATCH4];

	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->CRTC[NV_VGA_CRTCX_45] = 0x0;
	} else {
		regp->CRTC[NV_VGA_CRTCX_45] = 0x80;
	}

	/* What does this do?:
	 * bit0: crtc0
	 * bit6: lvds
	 * bit7: lvds + tmds (only in X)
	 */
	if (nv_crtc->head == 0)
		regp->CRTC[NV_VGA_CRTCX_4B] = 0x1;
	else 
		regp->CRTC[NV_VGA_CRTCX_4B] = 0x0;

	if (is_lvds)
		regp->CRTC[NV_VGA_CRTCX_4B] |= 0x40;

	if (is_fp && !NVMatchModePrivate(mode, NV_MODE_VGA))
		regp->CRTC[NV_VGA_CRTCX_4B] |= 0x80;

	if (NVMatchModePrivate(mode, NV_MODE_CONSOLE)) { /* we need consistent restore. */
		regp->CRTC[NV_VGA_CRTCX_52] = pNv->misc_info.crtc_reg_52[nv_crtc->head];
	} else {
		/* The blob seems to take the current value from crtc 0, add 4 to that and reuse the old value for crtc 1.*/
		if (nv_crtc->head == 1) {
			regp->CRTC[NV_VGA_CRTCX_52] = pNv->misc_info.crtc_reg_52[0];
		} else {
			regp->CRTC[NV_VGA_CRTCX_52] = pNv->misc_info.crtc_reg_52[0] + 4;
		}
	}

	if (pNv->twoHeads)
		/* The exact purpose of this register is unknown, but we copy value from crtc0 */
		regp->gpio_ext = NVReadCRTC(pNv, 0, NV_PCRTC_GPIO_EXT);

	if (NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->unk830 = 0;
		regp->unk834 = 0;
	} else {
		regp->unk830 = mode->CrtcVDisplay - 3;
		regp->unk834 = mode->CrtcVDisplay - 1;
	}

	if (pNv->twoHeads)
		/* This is what the blob does */
		regp->unk850 = NVReadCRTC(pNv, 0, NV_CRTC_0850);

	/* Never ever modify gpio, unless you know very well what you're doing */
	regp->gpio = NVReadCRTC(pNv, 0, NV_CRTC_GPIO);

	if (NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
		regp->config = 0x0; /* VGA mode */
	} else {
		regp->config = 0x2; /* HSYNC mode */
	}

	/* Some misc regs */
	regp->CRTC[NV_VGA_CRTCX_43] = 0x1;
	if (pNv->Architecture == NV_ARCH_40) {
		regp->CRTC[NV_VGA_CRTCX_85] = 0xFF;
		regp->CRTC[NV_VGA_CRTCX_86] = 0x1;
	}

	/*
	 * Calculate the state that is common to all crtc's (stored in the state struct).
	 */
	ErrorF("crtc %d %d %d\n", nv_crtc->head, mode->CrtcHDisplay, pScrn->displayWidth);
	nv_crtc_calc_state_ext(crtc,
				mode,
				depth,
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
	NVCrtcRegPtr regp, savep;
	NVPtr pNv = NVPTR(pScrn);
	NVFBLayout *pLayout = &pNv->CurrentLayout;
	Bool is_fp = FALSE;
	Bool is_lvds = FALSE;
	float aspect_ratio, panel_ratio;
	uint32_t h_scale, v_scale;

	regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];
	savep = &pNv->SavedReg.crtc_reg[nv_crtc->head];

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
		/* This is what the blob does. */
		regp->fp_horiz_regs[REG_DISP_CRTC] = adjusted_mode->HSyncStart - 75 - 1;
		regp->fp_horiz_regs[REG_DISP_SYNC_START] = adjusted_mode->HSyncStart - 1;
		regp->fp_horiz_regs[REG_DISP_SYNC_END] = adjusted_mode->HSyncEnd - 1;
		regp->fp_horiz_regs[REG_DISP_VALID_START] = adjusted_mode->HSkew;
		regp->fp_horiz_regs[REG_DISP_VALID_END] = adjusted_mode->HDisplay - 1;

		regp->fp_vert_regs[REG_DISP_END] = adjusted_mode->VDisplay - 1;
		regp->fp_vert_regs[REG_DISP_TOTAL] = adjusted_mode->VTotal - 1;
		/* This is what the blob does. */
		regp->fp_vert_regs[REG_DISP_CRTC] = adjusted_mode->VTotal - 5 - 1;
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
	* bit8: enable center mode
	* bit9: enable native mode
	* bit24: 12/24 bit interface (12bit=on, 24bit=off)
	* bit26: a bit sometimes seen on some g70 cards
	* bit28: fp display enable bit
	* bit31: set for dual link LVDS
	* nv10reg contains a few more things, but i don't quite get what it all means.
	*/

	if (pNv->Architecture >= NV_ARCH_30)
		regp->fp_control[nv_crtc->head] = 0x00100000;
	else
		regp->fp_control[nv_crtc->head] = 0x00000000;

	/* Deal with vsync/hsync polarity */
	/* LVDS screens do set this, but modes with +ve syncs are very rare */
	if (is_fp) {
		if (adjusted_mode->Flags & V_PVSYNC)
			regp->fp_control[nv_crtc->head] |= NV_RAMDAC_FP_CONTROL_VSYNC_POS;
		if (adjusted_mode->Flags & V_PHSYNC)
			regp->fp_control[nv_crtc->head] |= NV_RAMDAC_FP_CONTROL_HSYNC_POS;
	} else {
		/* The blob doesn't always do this, but often */
		regp->fp_control[nv_crtc->head] |= NV_RAMDAC_FP_CONTROL_VSYNC_DISABLE;
		regp->fp_control[nv_crtc->head] |= NV_RAMDAC_FP_CONTROL_HSYNC_DISABLE;
	}

	if (is_fp) {
		if (NVMatchModePrivate(mode, NV_MODE_CONSOLE)) /* seems to be used almost always */
			regp->fp_control[nv_crtc->head] |= NV_RAMDAC_FP_CONTROL_MODE_SCALE;
		else if (nv_output->scaling_mode == SCALE_PANEL) /* panel needs to scale */
			regp->fp_control[nv_crtc->head] |= NV_RAMDAC_FP_CONTROL_MODE_CENTER;
		/* This is also true for panel scaling, so we must put the panel scale check first */
		else if (mode->Clock == adjusted_mode->Clock) /* native mode */
			regp->fp_control[nv_crtc->head] |= NV_RAMDAC_FP_CONTROL_MODE_NATIVE;
		else /* gpu needs to scale */
			regp->fp_control[nv_crtc->head] |= NV_RAMDAC_FP_CONTROL_MODE_SCALE;
	}

	if (nvReadEXTDEV(pNv, NV_PEXTDEV_BOOT_0) & NV_PEXTDEV_BOOT_0_STRAP_FP_IFACE_12BIT)
		regp->fp_control[nv_crtc->head] |= NV_PRAMDAC_FP_TG_CONTROL_WIDTH_12;

	/* If the special bit exists, it exists on both ramdacs */
	regp->fp_control[nv_crtc->head] |= NVReadRAMDAC(pNv, 0, NV_RAMDAC_FP_CONTROL) & (1 << 26);

	if (is_fp)
		regp->fp_control[nv_crtc->head] |= NV_PRAMDAC_FP_TG_CONTROL_DISPEN_POS;
	else
		regp->fp_control[nv_crtc->head] |= NV_PRAMDAC_FP_TG_CONTROL_DISPEN_DISABLE;

	Bool lvds_use_straps = pNv->dcb_table.entry[nv_output->dcb_entry].lvdsconf.use_straps_for_mode;
	if (is_lvds && ((lvds_use_straps && pNv->VBIOS.fp.dual_link) || (!lvds_use_straps && adjusted_mode->Clock >= pNv->VBIOS.fp.duallink_transition_clk)))
		regp->fp_control[nv_crtc->head] |= (8 << 28);

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

		/* This can override HTOTAL and VTOTAL */
		regp->debug_2 = 0;

		/* We want automatic scaling */
		regp->debug_1 = 0;

		regp->fp_hvalid_start = 0;
		regp->fp_hvalid_end = (nv_output->fpWidth - 1);

		regp->fp_vvalid_start = 0;
		regp->fp_vvalid_end = (nv_output->fpHeight - 1);

		/* 0 = panel scaling */
		if (nv_output->scaling_mode == SCALE_PANEL) {
			ErrorF("Flat panel is doing the scaling.\n");
		} else {
			ErrorF("GPU is doing the scaling.\n");

			if (nv_output->scaling_mode == SCALE_ASPECT) {
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

	if (!is_fp && NVMatchModePrivate(mode, NV_MODE_VGA)) {
		regp->debug_1 = 0x08000800;
	}

	if (pNv->Architecture >= NV_ARCH_10) {
		/* Only bit that bios and blob set. */
		regp->nv10_cursync = (1<<25);
	}

	/* These are the common blob values, minus a few fp specific bit's */
	/* Let's keep the TMDS pll and fpclock running in all situations */
	regp->debug_0[nv_crtc->head] = 0x1101100;

	if (is_fp && nv_output->scaling_mode != SCALE_NOSCALE) {
		regp->debug_0[nv_crtc->head] |= NV_RAMDAC_FP_DEBUG_0_XSCALE_ENABLED;
		regp->debug_0[nv_crtc->head] |= NV_RAMDAC_FP_DEBUG_0_YSCALE_ENABLED;
	} else if (is_fp) { /* no_scale mode, so we must center it */
		uint32_t diff;

		diff = nv_output->fpWidth - mode->HDisplay;
		regp->fp_hvalid_start = diff/2;
		regp->fp_hvalid_end = (nv_output->fpWidth - diff/2 - 1);

		diff = nv_output->fpHeight - mode->VDisplay;
		regp->fp_vvalid_start = diff/2;
		regp->fp_vvalid_end = (nv_output->fpHeight - diff/2 - 1);
	}

	/* Is this crtc bound or output bound? */
	/* Does the bios TMDS script try to change this sometimes? */
	if (is_fp) {
		/* I am not completely certain, but seems to be set only for dfp's */
		regp->debug_0[nv_crtc->head] |= NV_RAMDAC_FP_DEBUG_0_TMDS_ENABLED;
	}

	if (output)
		ErrorF("output %d debug_0 %08X\n", nv_output->output_resource, regp->debug_0[nv_crtc->head]);

	/* Flatpanel support needs at least a NV10 */
	if (pNv->twoHeads) {
		if (pNv->FPDither || (is_lvds && !pNv->VBIOS.fp.if_is_24bit)) {
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
		} else
			regp->dither = savep->dither;
	}

	uint8_t depth;
	/* This is mode related, not pitch. */
	if (NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
		depth = pNv->console_mode[nv_crtc->head].depth;
	} else {
		depth = pLayout->depth;
	}

	switch (depth) {
		case 4:
			regp->general = 0x00000100;
			break;
		case 24:
		case 15:
			regp->general = 0x00100100;
			break;
		case 32:
		case 16:
		case 8:
		default:
			regp->general = 0x00101100;
			break;
	}

	if (depth > 8 && !NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
		regp->general |= 0x30; /* enable palette mode */
	}

	if (pNv->alphaCursor && !NVMatchModePrivate(mode, NV_MODE_CONSOLE)) {
		/* PIPE_LONG mode, something to do with the size of the cursor? */
		regp->general |= (1<<29);
	}

	/* Some values the blob sets */
	/* This may apply to the real ramdac that is being used (for crosswired situations) */
	/* Nevertheless, it's unlikely to cause many problems, since the values are equal for both */
	regp->unk_a20 = 0x0;
	regp->unk_a24 = 0xfffff;
	regp->unk_a34 = 0x1;

	if (pNv->twoHeads) {
		/* Do we also "own" the other register pair? */
		/* If we own neither, they will just be ignored at load time. */
		uint8_t other_head = (~nv_crtc->head) & 1;
		if (pNv->fp_regs_owner[other_head] == nv_crtc->head) {
			if (nv_output->type == OUTPUT_TMDS || nv_output->type == OUTPUT_LVDS) {
				regp->fp_control[other_head] = regp->fp_control[nv_crtc->head];
				regp->debug_0[other_head] = regp->debug_0[nv_crtc->head];
				/* Set TMDS_PLL and FPCLK, only seen for a NV31M so far. */
				regp->debug_0[nv_crtc->head] |= NV_RAMDAC_FP_DEBUG_0_PWRDOWN_FPCLK;
				regp->debug_0[other_head] |= NV_RAMDAC_FP_DEBUG_0_PWRDOWN_TMDS_PLL;
			} else {
				ErrorF("This is BAD, we own more than one fp reg set, but are not a LVDS or TMDS output.\n");
			}
		}
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
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);
	NVFBLayout *pLayout = &pNv->CurrentLayout;

	ErrorF("nv_crtc_mode_set is called for CRTC %d\n", nv_crtc->head);

	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Mode on CRTC %d\n", nv_crtc->head);
	xf86PrintModeline(pScrn->scrnIndex, mode);
	if (pNv->twoHeads)
		NVCrtcSetOwner(crtc);

	nv_crtc_mode_set_vga(crtc, mode, adjusted_mode);

	/* set sel_clk before calculating PLLs */
	nv_crtc_mode_set_sel_clk(crtc, &pNv->ModeReg);
	if (pNv->Architecture == NV_ARCH_40) {
		ErrorF("writing sel_clk %08X\n", pNv->ModeReg.sel_clk);
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_SEL_CLK, pNv->ModeReg.sel_clk);
	}
	nv_crtc_mode_set_regs(crtc, mode, adjusted_mode);
	nv_crtc_mode_set_ramdac_regs(crtc, mode, adjusted_mode);

	NVVgaProtect(crtc, TRUE);
	nv_crtc_load_state_ramdac(crtc, &pNv->ModeReg);
	nv_crtc_load_state_ext(crtc, &pNv->ModeReg, FALSE);
	if (pLayout->depth > 8)
		nv_crtc_load_state_palette(crtc, &pNv->ModeReg);
	nv_crtc_load_state_vga(crtc, &pNv->ModeReg);
	if (pNv->Architecture == NV_ARCH_40) {
		nv40_crtc_load_state_pll(crtc, &pNv->ModeReg);
	} else {
		nv_crtc_load_state_pll(pNv, &pNv->ModeReg);
	}

	NVVgaProtect(crtc, FALSE);

	NVCrtcSetBase(crtc, x, y, NVMatchModePrivate(mode, NV_MODE_CONSOLE));

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

	ErrorF("nv_crtc_save is called for CRTC %d\n", nv_crtc->head);

	/* We just came back from terminal, so unlock */
	NVCrtcLockUnlock(crtc, FALSE);

	nv_crtc_save_state_ramdac(crtc, &pNv->SavedReg);
	nv_crtc_save_state_vga(crtc, &pNv->SavedReg);
	nv_crtc_save_state_palette(crtc, &pNv->SavedReg);
	nv_crtc_save_state_ext(crtc, &pNv->SavedReg);
	if (pNv->Architecture == NV_ARCH_40) {
		nv40_crtc_save_state_pll(pNv, &pNv->SavedReg);
	} else {
		nv_crtc_save_state_pll(pNv, &pNv->SavedReg);
	}
}

void nv_crtc_restore(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);
	RIVA_HW_STATE *state;
	NVCrtcRegPtr savep;

	state = &pNv->SavedReg;
	savep = &pNv->SavedReg.crtc_reg[nv_crtc->head];

	ErrorF("nv_crtc_restore is called for CRTC %d\n", nv_crtc->head);

	/* Just to be safe */
	NVCrtcLockUnlock(crtc, FALSE);

	NVVgaProtect(crtc, TRUE);
	nv_crtc_load_state_ramdac(crtc, &pNv->SavedReg);
	nv_crtc_load_state_ext(crtc, &pNv->SavedReg, TRUE);
	nv_crtc_load_state_palette(crtc, &pNv->SavedReg);
	nv_crtc_load_state_vga(crtc, &pNv->SavedReg);

	/* Force restoring vpll. */
	state->vpll_changed[nv_crtc->head] = TRUE;

	if (pNv->Architecture == NV_ARCH_40) {
		nv40_crtc_load_state_pll(crtc, &pNv->SavedReg);
	} else {
		nv_crtc_load_state_pll(pNv, &pNv->SavedReg);
	}
	NVVgaProtect(crtc, FALSE);

	nv_crtc->last_dpms = NV_DPMS_CLEARED;
}

static void
NVResetCrtcConfig(xf86CrtcPtr crtc, Bool set)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);

	if (pNv->twoHeads) {
		uint32_t val = 0;

		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		if (set) {
			NVCrtcRegPtr regp;

			regp = &pNv->ModeReg.crtc_reg[nv_crtc->head];
			val = regp->head;
		}

		NVCrtcWriteCRTC(crtc, NV_CRTC_FSEL, val);
	}
}

void nv_crtc_prepare(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	ErrorF("nv_crtc_prepare is called for CRTC %d\n", nv_crtc->head);

	/* Just in case */
	NVCrtcLockUnlock(crtc, 0);

	NVResetCrtcConfig(crtc, FALSE);

	crtc->funcs->dpms(crtc, DPMSModeOff);

	/* Sync the engine before adjust mode */
	if (pNv->EXADriverPtr) {
		exaMarkSync(pScrn->pScreen);
		exaWaitSync(pScrn->pScreen);
	}

	NVCrtcBlankScreen(crtc, FALSE); /* Blank screen */

	/* Some more preperation. */
	NVCrtcWriteCRTC(crtc, NV_CRTC_CONFIG, 0x1); /* Go to non-vga mode/out of enhanced mode */
	if (pNv->Architecture == NV_ARCH_40) {
		uint32_t reg900 = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_900);
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_900, reg900 & ~0x10000);
	}
}

void nv_crtc_commit(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ErrorF("nv_crtc_commit for CRTC %d\n", nv_crtc->head);

	crtc->funcs->dpms (crtc, DPMSModeOn);

	if (crtc->scrn->pScreen != NULL)
		xf86_reload_cursors (crtc->scrn->pScreen);

	NVResetCrtcConfig(crtc, TRUE);
}

static Bool nv_crtc_lock(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ErrorF("nv_crtc_lock is called for CRTC %d\n", nv_crtc->head);

	return FALSE;
}

static void nv_crtc_unlock(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	ErrorF("nv_crtc_unlock is called for CRTC %d\n", nv_crtc->head);
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

	nv_crtc_load_state_palette(crtc, &pNv->ModeReg);
}

/**
 * Allocates memory for a locked-in-framebuffer shadow of the given
 * width and height for this CRTC's rotated shadow framebuffer.
 */
 
static void *
nv_crtc_shadow_allocate (xf86CrtcPtr crtc, int width, int height)
{
	ErrorF("nv_crtc_shadow_allocate is called\n");
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
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
		ErrorF("Failed to allocate memory for shadow buffer!\n");
		return NULL;
	}

	if (nv_crtc->shadow && nouveau_bo_map(nv_crtc->shadow, NOUVEAU_BO_RDWR)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"Failed to map shadow buffer.\n");
		return NULL;
	}

	offset = nv_crtc->shadow->map;
#else
	nv_crtc->shadow = exaOffscreenAlloc(pScreen, size, align, TRUE, NULL, NULL);
	if (nv_crtc->shadow == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Couldn't allocate shadow memory for rotated CRTC\n");
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
	ErrorF("nv_crtc_shadow_create is called\n");
	ScrnInfoPtr pScrn = crtc->scrn;
#if NOUVEAU_EXA_PIXMAPS
	ScreenPtr pScreen = pScrn->pScreen;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
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
		ErrorF("No shadow private, stage 1\n");
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
		ErrorF("No shadow private, stage 2\n");
#endif /* NOUVEAU_EXA_PIXMAPS */

	return rotate_pixmap;
}

static void
nv_crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr rotate_pixmap, void *data)
{
	ErrorF("nv_crtc_shadow_destroy is called\n");
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
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
	.shadow_create = nv_crtc_shadow_create,
	.shadow_allocate = nv_crtc_shadow_allocate,
	.shadow_destroy = nv_crtc_shadow_destroy,
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
	.shadow_create = nv_crtc_shadow_create,
	.shadow_allocate = nv_crtc_shadow_allocate,
	.shadow_destroy = nv_crtc_shadow_destroy,
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
	nv_crtc->head = crtc_num;
	nv_crtc->last_dpms = NV_DPMS_CLEARED;
	pNv->fp_regs_owner[nv_crtc->head] = nv_crtc->head;

	crtc->driver_private = nv_crtc;

	NVCrtcLockUnlock(crtc, FALSE);
}

static void nv_crtc_load_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	int i;
	NVCrtcRegPtr regp;

	regp = &state->crtc_reg[nv_crtc->head];

	NVWritePVIO(crtc, VGA_MISC_OUT_W, regp->MiscOutReg);

	for (i = 0; i < 5; i++)
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

static void nv_crtc_load_state_ext(xf86CrtcPtr crtc, RIVA_HW_STATE *state, Bool override)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);    
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVCrtcRegPtr regp;
	int i;

	regp = &state->crtc_reg[nv_crtc->head];

	if (pNv->Architecture >= NV_ARCH_10) {
		nvWriteVIDEO(pNv, NV_PVIDEO_STOP, 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_INTR_EN, 0);
		nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(0), 0);
		nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(1), 0);
		nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(0), pNv->VRAMPhysicalSize - 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(1), pNv->VRAMPhysicalSize - 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(0), pNv->VRAMPhysicalSize - 1);
		nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(1), pNv->VRAMPhysicalSize - 1);
		nvWriteMC(pNv, NV_PBUS_POWERCTRL_2, 0);

		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_BUFFER, regp->CRTC[NV_VGA_CRTCX_BUFFER]);
		NVCrtcWriteCRTC(crtc, NV_CRTC_CURSOR_CONFIG, regp->cursorConfig);
		NVCrtcWriteCRTC(crtc, NV_CRTC_0830, regp->unk830);
		NVCrtcWriteCRTC(crtc, NV_CRTC_0834, regp->unk834);
		if (pNv->Architecture == NV_ARCH_40) {
			NVCrtcWriteCRTC(crtc, NV_CRTC_0850, regp->unk850);
			NVCrtcWriteCRTC(crtc, NV_PCRTC_GPIO_EXT, regp->gpio_ext);
		}

		if (pNv->Architecture == NV_ARCH_40) {
			uint32_t reg900 = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_900);
			if (regp->config == 0x2) { /* enhanced "horizontal only" non-vga mode */
				NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_900, reg900 | 0x10000);
			} else {
				NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_900, reg900 & ~0x10000);
			}
		}
	}

	NVCrtcWriteCRTC(crtc, NV_CRTC_CONFIG, regp->config);
	NVCrtcWriteCRTC(crtc, NV_CRTC_GPIO, regp->gpio);

	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_REPAINT0, regp->CRTC[NV_VGA_CRTCX_REPAINT0]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_REPAINT1, regp->CRTC[NV_VGA_CRTCX_REPAINT1]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_LSR, regp->CRTC[NV_VGA_CRTCX_LSR]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_PIXEL, regp->CRTC[NV_VGA_CRTCX_PIXEL]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_LCD, regp->CRTC[NV_VGA_CRTCX_LCD]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_HEB, regp->CRTC[NV_VGA_CRTCX_HEB]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FIFO1, regp->CRTC[NV_VGA_CRTCX_FIFO1]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FIFO0, regp->CRTC[NV_VGA_CRTCX_FIFO0]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FIFO_LWM, regp->CRTC[NV_VGA_CRTCX_FIFO_LWM]);
	if (pNv->Architecture >= NV_ARCH_30)
		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FIFO_LWM_NV30, regp->CRTC[NV_VGA_CRTCX_FIFO_LWM_NV30]);

	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL0, regp->CRTC[NV_VGA_CRTCX_CURCTL0]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL1, regp->CRTC[NV_VGA_CRTCX_CURCTL1]);
	if (pNv->Architecture == NV_ARCH_40) /* HW bug */
		nv_crtc_fix_nv40_hw_cursor(pScrn, nv_crtc->head);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL2, regp->CRTC[NV_VGA_CRTCX_CURCTL2]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_INTERLACE, regp->CRTC[NV_VGA_CRTCX_INTERLACE]);

	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_26, regp->CRTC[NV_VGA_CRTCX_26]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_3B, regp->CRTC[NV_VGA_CRTCX_3B]);
	NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_SCRATCH4, regp->CRTC[NV_VGA_CRTCX_SCRATCH4]);
	if (pNv->Architecture >= NV_ARCH_10) {
		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_EXTRA, regp->CRTC[NV_VGA_CRTCX_EXTRA]);
		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_43, regp->CRTC[NV_VGA_CRTCX_43]);
		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_45, regp->CRTC[NV_VGA_CRTCX_45]);
		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_4B, regp->CRTC[NV_VGA_CRTCX_4B]);
		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_52, regp->CRTC[NV_VGA_CRTCX_52]);
	}
	/* NV11 and NV20 stop at 0x52. */
	if (pNv->NVArch >= 0x17 && pNv->twoHeads) {
		if (override)
			for (i = 0; i < 0x10; i++)
				NVWriteVGACR5758(pNv, nv_crtc->head, i, regp->CR58[i]);

		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FP_HTIMING, regp->CRTC[NV_VGA_CRTCX_FP_HTIMING]);
		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_FP_VTIMING, regp->CRTC[NV_VGA_CRTCX_FP_VTIMING]);

		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_59, regp->CRTC[NV_VGA_CRTCX_59]);

		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_85, regp->CRTC[NV_VGA_CRTCX_85]);
		NVWriteVgaCrtc(crtc, NV_VGA_CRTCX_86, regp->CRTC[NV_VGA_CRTCX_86]);
	}

	/* Setting 1 on this value gives you interrupts for every vblank period. */
	NVCrtcWriteCRTC(crtc, NV_CRTC_INTR_EN_0, 0);
	NVCrtcWriteCRTC(crtc, NV_CRTC_INTR_0, NV_CRTC_INTR_VBLANK);

	pNv->CurrentState = state;
}

static void nv_crtc_save_state_vga(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	int i;
	NVCrtcRegPtr regp;

	regp = &state->crtc_reg[nv_crtc->head];

	regp->MiscOutReg = NVReadPVIO(crtc, VGA_MISC_OUT_R);

	for (i = 0; i < 25; i++)
		regp->CRTC[i] = NVReadVgaCrtc(crtc, i);

	NVEnablePalette(crtc);
	for (i = 0; i < 21; i++)
		regp->Attribute[i] = NVReadVgaAttr(crtc, i);
	NVDisablePalette(crtc);

	for (i = 0; i < 9; i++)
		regp->Graphics[i] = NVReadVgaGr(crtc, i);

	for (i = 0; i < 5; i++)
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
	regp->CRTC[NV_VGA_CRTCX_BUFFER] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_BUFFER);
	if (pNv->Architecture >= NV_ARCH_30)
		regp->CRTC[NV_VGA_CRTCX_FIFO_LWM_NV30] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FIFO_LWM_NV30);
	regp->CRTC[NV_VGA_CRTCX_CURCTL0] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL0);
	regp->CRTC[NV_VGA_CRTCX_CURCTL1] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL1);
	regp->CRTC[NV_VGA_CRTCX_CURCTL2] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_CURCTL2);
	regp->CRTC[NV_VGA_CRTCX_INTERLACE] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_INTERLACE);

	if (pNv->Architecture >= NV_ARCH_10) {
		regp->unk830 = NVCrtcReadCRTC(crtc, NV_CRTC_0830);
		regp->unk834 = NVCrtcReadCRTC(crtc, NV_CRTC_0834);
		if (pNv->Architecture == NV_ARCH_40) {
			regp->unk850 = NVCrtcReadCRTC(crtc, NV_CRTC_0850);
			regp->gpio_ext = NVCrtcReadCRTC(crtc, NV_PCRTC_GPIO_EXT);
		}
		if (pNv->twoHeads) {
			regp->head = NVCrtcReadCRTC(crtc, NV_CRTC_FSEL);
			regp->crtcOwner = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_OWNER);
		}
		regp->cursorConfig = NVCrtcReadCRTC(crtc, NV_CRTC_CURSOR_CONFIG);
	}

	regp->gpio = NVCrtcReadCRTC(crtc, NV_CRTC_GPIO);
	regp->config = NVCrtcReadCRTC(crtc, NV_CRTC_CONFIG);

	regp->CRTC[NV_VGA_CRTCX_26] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_26);
	regp->CRTC[NV_VGA_CRTCX_3B] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_3B);
	regp->CRTC[NV_VGA_CRTCX_SCRATCH4] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_SCRATCH4);
	if (pNv->Architecture >= NV_ARCH_10) {
		regp->CRTC[NV_VGA_CRTCX_EXTRA] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_EXTRA);
		regp->CRTC[NV_VGA_CRTCX_43] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_43);
		regp->CRTC[NV_VGA_CRTCX_45] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_45);
		regp->CRTC[NV_VGA_CRTCX_4B] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_4B);
		regp->CRTC[NV_VGA_CRTCX_52] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_52);
	}
	/* NV11 and NV20 don't have this, they stop at 0x52. */
	if (pNv->NVArch >= 0x17 && pNv->twoHeads) {
		for (i = 0; i < 0x10; i++)
			regp->CR58[i] = NVReadVGACR5758(pNv, nv_crtc->head, i);

		regp->CRTC[NV_VGA_CRTCX_59] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_59);
		regp->CRTC[NV_VGA_CRTCX_FP_HTIMING] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FP_HTIMING);
		regp->CRTC[NV_VGA_CRTCX_FP_VTIMING] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_FP_VTIMING);

		regp->CRTC[NV_VGA_CRTCX_85] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_85);
		regp->CRTC[NV_VGA_CRTCX_86] = NVReadVgaCrtc(crtc, NV_VGA_CRTCX_86);
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

	regp->general = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_GENERAL_CONTROL);

	regp->fp_control[0]	= NVReadRAMDAC(pNv, 0, NV_RAMDAC_FP_CONTROL);
	regp->debug_0[0]	= NVReadRAMDAC(pNv, 0, NV_RAMDAC_FP_DEBUG_0);

	if (pNv->twoHeads) {
		regp->fp_control[1]	= NVReadRAMDAC(pNv, 1, NV_RAMDAC_FP_CONTROL);
		regp->debug_0[1]	= NVReadRAMDAC(pNv, 1, NV_RAMDAC_FP_DEBUG_0);

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

	regp->fp_hvalid_start = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_HVALID_START);
	regp->fp_hvalid_end = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_HVALID_END);
	regp->fp_vvalid_start = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_VVALID_START);
	regp->fp_vvalid_end = NVCrtcReadRAMDAC(crtc, NV_RAMDAC_FP_VVALID_END);
}

static void nv_crtc_load_state_ramdac(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);    
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVCrtcRegPtr regp;
	int i;

	regp = &state->crtc_reg[nv_crtc->head];

	NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_GENERAL_CONTROL, regp->general);

	if (pNv->fp_regs_owner[0] == nv_crtc->head) {
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_FP_CONTROL, regp->fp_control[0]);
		NVWriteRAMDAC(pNv, 0, NV_RAMDAC_FP_DEBUG_0, regp->debug_0[0]);
	}
	if (pNv->twoHeads) {
		if (pNv->fp_regs_owner[1] == nv_crtc->head) {
			NVWriteRAMDAC(pNv, 1, NV_RAMDAC_FP_CONTROL, regp->fp_control[1]);
			NVWriteRAMDAC(pNv, 1, NV_RAMDAC_FP_DEBUG_0, regp->debug_0[1]);
		}
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

	if (pNv->NVArch == 0x11) {
		NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_DITHER_NV11, regp->dither);
	} else if (pNv->twoHeads) {
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

	NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_HVALID_START, regp->fp_hvalid_start);
	NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_HVALID_END, regp->fp_hvalid_end);
	NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_VVALID_START, regp->fp_vvalid_start);
	NVCrtcWriteRAMDAC(crtc, NV_RAMDAC_FP_VVALID_END, regp->fp_vvalid_end);
}

void
NVCrtcSetBase (xf86CrtcPtr crtc, int x, int y, Bool bios_restore)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);    
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVFBLayout *pLayout = &pNv->CurrentLayout;
	uint32_t start = 0;

	ErrorF("NVCrtcSetBase: x: %d y: %d\n", x, y);

	if (bios_restore) {
		start = pNv->console_mode[nv_crtc->head].fb_start;
	} else {
		start += ((y * pScrn->displayWidth + x) * (pLayout->bitsPerPixel/8));
		if (crtc->rotatedData != NULL) { /* we do not exist on the real framebuffer */
#if NOUVEAU_EXA_PIXMAPS
			start = nv_crtc->shadow->offset;
#else
			start = pNv->FB->offset + nv_crtc->shadow->offset; /* We do exist relative to the framebuffer */
#endif
		} else {
			start += pNv->FB->offset;
		}
	}

	/* 30 bits addresses in 32 bits according to haiku */
	NVCrtcWriteCRTC(crtc, NV_CRTC_START, start & 0xfffffffc);

	/* set NV4/NV10 byte adress: (bit0 - 1) */
	NVWriteVgaAttr(crtc, 0x13, (start & 0x3) << 1);

	crtc->x = x;
	crtc->y = y;
}

static void nv_crtc_save_state_palette(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(crtc->scrn);
	volatile uint8_t *pDACReg = nv_crtc->head ? pNv->PDIO1 : pNv->PDIO0;
	int i;

	VGA_WR08(pDACReg, VGA_DAC_MASK, 0xff);
	VGA_WR08(pDACReg, VGA_DAC_READ_ADDR, 0x0);

	for (i = 0; i < 768; i++) {
		state->crtc_reg[nv_crtc->head].DAC[i] = NV_RD08(pDACReg, VGA_DAC_DATA);
		DDXMMIOH("nv_crtc_save_state_palette: head %d reg 0x%04x data 0x%02x\n", nv_crtc->head, NV_PDIO0_OFFSET + (nv_crtc->head ? NV_PDIO0_SIZE : 0) + VGA_DAC_DATA, state->crtc_reg[nv_crtc->head].DAC[i]);
	}

	NVDisablePalette(crtc);
}
static void nv_crtc_load_state_palette(xf86CrtcPtr crtc, RIVA_HW_STATE *state)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(crtc->scrn);
	volatile uint8_t *pDACReg = nv_crtc->head ? pNv->PDIO1 : pNv->PDIO0;
	int i;

	VGA_WR08(pDACReg, VGA_DAC_MASK, 0xff);
	VGA_WR08(pDACReg, VGA_DAC_WRITE_ADDR, 0x0);

	for (i = 0; i < 768; i++) {
		DDXMMIOH("nv_crtc_load_state_palette: head %d reg 0x%04x data 0x%02x\n", nv_crtc->head, NV_PDIO0_OFFSET + (nv_crtc->head ? NV_PDIO0_SIZE : 0) + VGA_DAC_DATA, state->crtc_reg[nv_crtc->head].DAC[i]);
		NV_WR08(pDACReg, VGA_DAC_DATA, state->crtc_reg[nv_crtc->head].DAC[i]);
	}

	NVDisablePalette(crtc);
}

/* on = unblank */
void NVCrtcBlankScreen(xf86CrtcPtr crtc, Bool on)
{
	NVPtr pNv = NVPTR(crtc->scrn);
	unsigned char scrn;

	if (pNv->twoHeads)
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

/* Reset a mode after a drastic output resource change for example. */
void NVCrtcModeFix(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	Bool need_unlock;

	if (!crtc->enabled)
		return;

	if (!xf86ModesEqual(&crtc->mode, &crtc->desiredMode)) /* not currently in X */
		return;

	DisplayModePtr adjusted_mode = xf86DuplicateMode(&crtc->mode);
	uint8_t dpms_mode = nv_crtc->last_dpms;

	/* Set the crtc mode again. */
	crtc->funcs->dpms(crtc, DPMSModeOff);
	need_unlock = crtc->funcs->lock(crtc);
	crtc->funcs->mode_fixup(crtc, &crtc->mode, adjusted_mode);
	crtc->funcs->prepare(crtc);
	crtc->funcs->mode_set(crtc, &crtc->mode, adjusted_mode, crtc->x, crtc->y);
	crtc->funcs->commit(crtc);
	if (need_unlock)
		crtc->funcs->unlock(crtc);
	crtc->funcs->dpms(crtc, dpms_mode);

	/* Free mode. */
	xfree(adjusted_mode);
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
