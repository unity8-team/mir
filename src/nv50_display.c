/*
 * Copyright (c) 2007 NVIDIA, Corporation
 * Copyright (c) 2008 Maarten Maathuis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <float.h>
#include <math.h>
#include <strings.h>
#include <unistd.h>

#include "nv_include.h"
#include "nv50_type.h"
#include "nv50_cursor.h"
#include "nv50_display.h"
#include "nv50_output.h"

static void NV50CrtcShowHideCursor(xf86CrtcPtr crtc, Bool show, Bool update);

void NV50CrtcSetPClk(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcSetPClk is called.\n");

	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	NVPtr pNv = NVPTR(pScrn);
	int i;

	/* I don't know why exactly, but these were in my table. */
	uint32_t pll_reg = nv_crtc->head ? NV50_CRTC1_CLK_CTRL1 : NV50_CRTC0_CLK_CTRL1;

	int NM1 = 0xbeef, NM2 = 0xdead, log2P;
	struct pll_lims pll_lim;
	get_pll_limits(pScrn, pll_reg, &pll_lim);
	getMNP_double(pScrn, &pll_lim, nv_crtc->pclk, &NM1, &NM2, &log2P);

	uint32_t reg1 = NVRead(pNv, pll_reg + 4);
	uint32_t reg2 = NVRead(pNv, pll_reg + 8);

	/* bit0: The blob (and bios) seem to have this on (almost) always.
	 *          I'm hoping this (experiment) will fix my image stability issues.
	 */
	NVWrite(pNv, NV50_CRTC0_CLK_CTRL1 + nv_crtc->head * 0x800, NV50_CRTC_CLK_CTRL1_CONNECTED | 0x10000011);

	/* Eventually we should learn ourselves what all the bits should be. */
	reg1 &= 0xff00ff00;
	reg2 &= 0x8000ff00;

	uint8_t N1 = (NM1 >> 8) & 0xFF;
	uint8_t M1 = NM1 & 0xFF;
	uint8_t N2 = (NM2 >> 8) & 0xFF;
	uint8_t M2 = NM2 & 0xFF;

	reg1 |= (M1 << 16) | N1;
	reg2 |= (log2P << 28) | (M2 << 16) | N2;

	NVWrite(pNv, pll_reg + 4, reg1);
	NVWrite(pNv, pll_reg + 8, reg2);

	/* There seem to be a few indicator bits, which are similar to the SOR_CTRL bits. */
	NVWrite(pNv, NV50_CRTC0_CLK_CTRL2 + nv_crtc->head * 0x800, 0);

	for(i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if(output->crtc != crtc)
			continue;
		NV50OutputSetPClk(output, nv_crtc->pclk);
	}
}

Head
NV50CrtcGetHead(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	return nv_crtc->head;
}

Bool
NV50DispPreInit(ScrnInfoPtr pScrn)
{
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50DispPreInit is called.\n");

	NVPtr pNv = NVPTR(pScrn);
	/* These labels are guesswork based on symmetry (2 SOR's and 3 DAC's exist)*/
	NVWrite(pNv, 0x00610184, NVRead(pNv, 0x00614004));
	NVWrite(pNv, 0x00610190 + SOR0 * 0x10, NVRead(pNv, 0x00616100 + SOR0 * 0x800));
	NVWrite(pNv, 0x00610190 + SOR1 * 0x10, NVRead(pNv, 0x00616100 + SOR1 * 0x800));
	NVWrite(pNv, 0x00610194 + SOR0 * 0x10, NVRead(pNv, 0x00616104 + SOR0 * 0x800));
	NVWrite(pNv, 0x00610194 + SOR1 * 0x10, NVRead(pNv, 0x00616104 + SOR1 * 0x800));
	NVWrite(pNv, 0x00610198 + SOR0 * 0x10, NVRead(pNv, 0x00616108 + SOR0 * 0x800));
	NVWrite(pNv, 0x00610198 + SOR1 * 0x10, NVRead(pNv, 0x00616108 + SOR1 * 0x800));
	NVWrite(pNv, 0x0061019c + SOR0 * 0x10, NVRead(pNv, 0x0061610c + SOR0 * 0x800));
	NVWrite(pNv, 0x0061019c + SOR1 * 0x10, NVRead(pNv, 0x0061610c + SOR1 * 0x800));
	NVWrite(pNv, 0x006101d0 + DAC0 * 0x4, NVRead(pNv, 0x0061a000 + DAC0 * 0x800));
	NVWrite(pNv, 0x006101d0 + DAC1 * 0x4, NVRead(pNv, 0x0061a000 + DAC1 * 0x800));
	NVWrite(pNv, 0x006101d0 + DAC2 * 0x4, NVRead(pNv, 0x0061a000 + DAC2 * 0x800));
	NVWrite(pNv, 0x006101e0 + SOR0 * 0x4, NVRead(pNv, 0x0061c000 + SOR0 * 0x800));
	NVWrite(pNv, 0x006101e0 + SOR1 * 0x4, NVRead(pNv, 0x0061c000 + SOR1 * 0x800));
	NVWrite(pNv, NV50_DAC0_DPMS_CTRL, 0x00550000 | NV50_DAC_DPMS_CTRL_PENDING);
	NVWrite(pNv, NV50_DAC0_CLK_CTRL2, 0x00000001);
	NVWrite(pNv, NV50_DAC1_DPMS_CTRL, 0x00550000 | NV50_DAC_DPMS_CTRL_PENDING);
	NVWrite(pNv, NV50_DAC1_CLK_CTRL2, 0x00000001);
	NVWrite(pNv, NV50_DAC2_DPMS_CTRL, 0x00550000 | NV50_DAC_DPMS_CTRL_PENDING);
	NVWrite(pNv, NV50_DAC2_CLK_CTRL2, 0x00000001);

	return TRUE;
}

Bool
NV50DispInit(ScrnInfoPtr pScrn)
{
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50DispInit is called.\n");

	NVPtr pNv = NVPTR(pScrn);
	uint32_t val;
	if (NVRead(pNv, NV50_DISPLAY_SUPERVISOR) & 0x100) {
		NVWrite(pNv, NV50_DISPLAY_SUPERVISOR, 0x100);
		NVWrite(pNv, 0x006194e8, NVRead(pNv, 0x006194e8) & ~1);
		while (NVRead(pNv, 0x006194e8) & 2);
	}

	NVWrite(pNv, NV50_DISPLAY_UNK200_CTRL, 0x2b00);
	/* A bugfix (#12637) from the nv driver, to unlock the driver if it's left in a poor state */
	do {
		val = NVRead(pNv, NV50_DISPLAY_UNK200_CTRL);
		if ((val & 0x9f0000) == 0x20000)
			NVWrite(pNv, NV50_DISPLAY_UNK200_CTRL, val | 0x800000);

		if ((val & 0x3f0000) == 0x30000)
			NVWrite(pNv, NV50_DISPLAY_UNK200_CTRL, val | 0x200000);
	} while ((val & 0x1e0000) != 0);
	NVWrite(pNv, NV50_DISPLAY_CTRL_STATE, NV50_DISPLAY_CTRL_STATE_ENABLE);
	NVWrite(pNv, NV50_DISPLAY_UNK200_CTRL, 0x1000b03);
	while (!(NVRead(pNv, NV50_DISPLAY_UNK200_CTRL) & 0x40000000));

	NV50DisplayCommand(pScrn, 0x84, 0);
	NV50DisplayCommand(pScrn, 0x88, 0);
	/* The GetLVDSNativeMode() function is proof that more than crtc0 is used by the bios. */
	NV50DisplayCommand(pScrn, NV50_CRTC0_BLANK_CTRL, NV50_CRTC0_BLANK_CTRL_BLANK);
	NV50DisplayCommand(pScrn, 0x800, 0);
	NV50DisplayCommand(pScrn, NV50_CRTC0_DISPLAY_START, 0);
	NV50DisplayCommand(pScrn, 0x82c, 0);

	return TRUE;
}

void
NV50DispShutdown(ScrnInfoPtr pScrn)
{
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50DispShutdown is called.\n");
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	for(i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		NV50CrtcBlankScreen(crtc, TRUE);
	}

	NV50DisplayCommand(pScrn, NV50_UPDATE_DISPLAY, 0);

	for(i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		/* I think this is some kind of trigger to reinitialise the supervisor. */
		/* Without anything active (this is shutdown) it's likely to disable itself. */
		/* The blob doesn't do it quite this way., it seems to do 0x30C as init and end. */
		/* It doesn't wait for a non-zero value either. */
		if (crtc->enabled) {
			uint32_t mask = 0;
			if (nv_crtc->head == 1)
				mask = NV50_DISPLAY_SUPERVISOR_CRTC1;
			else 
				mask = NV50_DISPLAY_SUPERVISOR_CRTC0;

			NVWrite(pNv, NV50_DISPLAY_SUPERVISOR, mask);
			while(!(NVRead(pNv, NV50_DISPLAY_SUPERVISOR) & mask));
		}
	}

	NVWrite(pNv, NV50_DISPLAY_UNK200_CTRL, 0x0);
	NVWrite(pNv, NV50_DISPLAY_CTRL_STATE, NV50_DISPLAY_CTRL_STATE_DISABLE);
	while ((NVRead(pNv, NV50_DISPLAY_UNK200_CTRL) & 0x1e0000) != 0);
	while ((NVRead(pNv, 0x0061c030 + SOR0 * 0x800) & 0x10000000));
	while ((NVRead(pNv, 0x0061c030 + SOR1 * 0x800) & 0x10000000));
}

void
NV50CrtcModeSet(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode, int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcModeSet is called with position (%d, %d).\n", x, y);

	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	nv_crtc->pclk = adjusted_mode->Clock;

	uint32_t hsync_dur = adjusted_mode->CrtcHSyncEnd - adjusted_mode->CrtcHSyncStart;
	uint32_t vsync_dur = adjusted_mode->CrtcVSyncEnd - adjusted_mode->CrtcVSyncStart;
	uint32_t hsync_start_to_end = adjusted_mode->CrtcHBlankEnd - adjusted_mode->CrtcHSyncStart;
	uint32_t vsync_start_to_end = adjusted_mode->CrtcVBlankEnd - adjusted_mode->CrtcVSyncStart;
	/* I can't give this a proper name, anyone else can? */
	uint32_t hunk1 = adjusted_mode->CrtcHTotal - adjusted_mode->CrtcHSyncStart + adjusted_mode->CrtcHBlankStart;
	uint32_t vunk1 = adjusted_mode->CrtcVTotal - adjusted_mode->CrtcVSyncStart + adjusted_mode->CrtcVBlankStart;
	/* Another strange value, this time only for interlaced modes. */
	uint32_t vunk2a = 2*adjusted_mode->CrtcVTotal - adjusted_mode->CrtcVSyncStart + adjusted_mode->CrtcVBlankStart;
	uint32_t vunk2b = adjusted_mode->CrtcVTotal - adjusted_mode->CrtcVSyncStart + adjusted_mode->CrtcVBlankEnd;

	if (adjusted_mode->Flags & V_INTERLACE) {
		vsync_dur /= 2;
		vsync_start_to_end  /= 2;
		vunk1 /= 2;
		vunk2a /= 2;
		vunk2b /= 2;
		/* magic */
		if (adjusted_mode->Flags & V_DBLSCAN) {
			vsync_start_to_end -= 1;
			vunk1 -= 1;
			vunk2a -= 1;
			vunk2b -= 1;
		}
	}

	/* NV50CrtcCommand includes head offset */
	/* This is the native mode when DFP && !SCALE_PANEL */
	NV50CrtcCommand(crtc, NV50_CRTC0_CLOCK, adjusted_mode->Clock | 0x800000);
	NV50CrtcCommand(crtc, NV50_CRTC0_INTERLACE, (adjusted_mode->Flags & V_INTERLACE) ? 2 : 0);
	NV50CrtcCommand(crtc, NV50_CRTC0_DISPLAY_START, 0);
	NV50CrtcCommand(crtc, 0x82c, 0);
	NV50CrtcCommand(crtc, NV50_CRTC0_DISPLAY_TOTAL, adjusted_mode->CrtcVTotal << 16 | adjusted_mode->CrtcHTotal);
	NV50CrtcCommand(crtc, NV50_CRTC0_SYNC_DURATION, (vsync_dur - 1) << 16 | (hsync_dur - 1));
	NV50CrtcCommand(crtc, NV50_CRTC0_SYNC_START_TO_BLANK_END, (vsync_start_to_end - 1) << 16 | (hsync_start_to_end - 1));
	NV50CrtcCommand(crtc, NV50_CRTC0_MODE_UNK1, (vunk1 - 1) << 16 | (hunk1 - 1));
	if (adjusted_mode->Flags & V_INTERLACE) {
		NV50CrtcCommand(crtc, NV50_CRTC0_MODE_UNK2, (vunk2b - 1) << 16 | (vunk2a - 1));
	}
	NV50CrtcCommand(crtc, NV50_CRTC0_FB_SIZE, pScrn->virtualY << 16 | pScrn->virtualX);
	NV50CrtcCommand(crtc, NV50_CRTC0_PITCH, pScrn->displayWidth * (pScrn->bitsPerPixel / 8) | 0x100000);
	switch(pScrn->depth) {
		case 8:
			NV50CrtcCommand(crtc, NV50_CRTC0_DEPTH, NV50_CRTC0_DEPTH_8BPP); 
			break;
		case 15:
			NV50CrtcCommand(crtc, NV50_CRTC0_DEPTH, NV50_CRTC0_DEPTH_15BPP);
			break;
		case 16:
			NV50CrtcCommand(crtc, NV50_CRTC0_DEPTH, NV50_CRTC0_DEPTH_16BPP);
			break;
		case 24:
			NV50CrtcCommand(crtc, NV50_CRTC0_DEPTH, NV50_CRTC0_DEPTH_24BPP); 
			break;
	}
	NV50CrtcSetDither(crtc, FALSE);
	NV50CrtcCommand(crtc, NV50_CRTC0_UNK_8A8, 0x40000);
	NV50CrtcCommand(crtc, NV50_CRTC0_FB_POS, y << 16 | x);
	/* This is the actual resolution of the mode. */
	NV50CrtcCommand(crtc, NV50_CRTC0_SCRN_SIZE, (mode->VDisplay << 16) | mode->HDisplay);
	NV50CrtcCommand(crtc, 0x8d4, 0);

	NV50CrtcBlankScreen(crtc, FALSE);
}

void
NV50CrtcBlankScreen(xf86CrtcPtr crtc, Bool blank)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcBlankScreen is called (%s).\n", blank ? "blanked" : "unblanked");

	NVPtr pNv = NVPTR(pScrn);
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	if(blank) {
		NV50CrtcShowHideCursor(crtc, FALSE, FALSE);

		NV50CrtcCommand(crtc, NV50_CRTC0_CLUT_MODE, NV50_CRTC0_CLUT_MODE_BLANK);
		NV50CrtcCommand(crtc, NV50_CRTC0_CLUT_OFFSET, 0);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, NV84_CRTC0_BLANK_UNK1, NV84_CRTC0_BLANK_UNK1_BLANK);
		NV50CrtcCommand(crtc, NV50_CRTC0_BLANK_CTRL, NV50_CRTC0_BLANK_CTRL_BLANK);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, NV84_CRTC0_BLANK_UNK2, NV84_CRTC0_BLANK_UNK2_BLANK);
	} else {
		NV50CrtcCommand(crtc, NV50_CRTC0_FB_OFFSET, pNv->FB->offset >> 8);
		NV50CrtcCommand(crtc, 0x864, 0);
		NVWrite(pNv, 0x00610380, 0);
		/* RAM is clamped to 256 MiB. */
		NVWrite(pNv, NV50_CRTC0_RAM_AMOUNT, pNv->RamAmountKBytes * 1024 - 1);
		NVWrite(pNv, 0x00610388, 0x150000);
		NVWrite(pNv, 0x0061038C, 0);
		if (nv_crtc->head == 1)
			NV50CrtcCommand(crtc, NV50_CRTC0_CURSOR_OFFSET, pNv->Cursor2->offset >> 8);
		else
			NV50CrtcCommand(crtc, NV50_CRTC0_CURSOR_OFFSET, pNv->Cursor->offset >> 8);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, NV84_CRTC0_BLANK_UNK2, NV84_CRTC0_BLANK_UNK2_UNBLANK);
		if(nv_crtc->cursorVisible)
			NV50CrtcShowHideCursor(crtc, TRUE, FALSE);
		NV50CrtcCommand(crtc, NV50_CRTC0_CLUT_MODE, 
			pScrn->depth == 8 ? NV50_CRTC0_CLUT_MODE_OFF : NV50_CRTC0_CLUT_MODE_ON);
		/* Each CRTC has it's own CLUT. */
		if (nv_crtc->head == 1)
			NV50CrtcCommand(crtc, NV50_CRTC0_CLUT_OFFSET, pNv->CLUT1->offset >> 8);
		else
			NV50CrtcCommand(crtc, NV50_CRTC0_CLUT_OFFSET, pNv->CLUT0->offset >> 8);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, NV84_CRTC0_BLANK_UNK1, NV84_CRTC0_BLANK_UNK1_UNBLANK);
		NV50CrtcCommand(crtc, NV50_CRTC0_BLANK_CTRL, NV50_CRTC0_BLANK_CTRL_UNBLANK);
     }
}

/******************************** Cursor stuff ********************************/
static void NV50CrtcShowHideCursor(xf86CrtcPtr crtc, Bool show, Bool update)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcShowHideCursor is called (%s, %s).\n", show ? "show" : "hide", update ? "update" : "no update");

	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	NV50CrtcCommand(crtc, NV50_CRTC0_CURSOR, 
		show ? NV50_CRTC0_CURSOR_SHOW : NV50_CRTC0_CURSOR_HIDE);
	if (update) {
		nv_crtc->cursorVisible = show;
		NV50DisplayCommand(pScrn, NV50_UPDATE_DISPLAY, 0);
	}
}

void NV50CrtcShowCursor(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	/* Calling NV50_UPDATE_DISPLAY during modeset will lock up everything. */
	if (nv_crtc->modeset_lock)
		return;

	NV50CrtcShowHideCursor(crtc, TRUE, TRUE);
}

void NV50CrtcHideCursor(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	/* Calling NV50_UPDATE_DISPLAY during modeset will lock up everything. */
	if (nv_crtc->modeset_lock)
		return;

	NV50CrtcShowHideCursor(crtc, FALSE, TRUE);
}

/******************************** CRTC stuff ********************************/

void
NV50CrtcPrepare(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcPrepare is called.\n");

	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	nv_crtc->modeset_lock = TRUE;

	for(i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if (!output->crtc)
			output->funcs->mode_set(output, NULL, NULL);
	}
}

void
NV50CrtcSetDither(xf86CrtcPtr crtc, Bool update)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcSetDither is called (%s).\n", update ? "update" : "no update");

	xf86OutputPtr output = NULL;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	for (i = 0; i < xf86_config->num_output; i++) {
		if (xf86_config->output[i]->crtc == crtc) {
			output = xf86_config->output[i];
			break;
		}
	}

	if (!output)
		return;

	NVOutputPrivatePtr nv_output = output->driver_private;

	NV50CrtcCommand(crtc, NV50_CRTC0_DITHERING_CTRL, nv_output->dithering ? 
			NV50_CRTC0_DITHERING_CTRL_ON : NV50_CRTC0_DITHERING_CTRL_OFF);
	if (update) 
		NV50DisplayCommand(pScrn, NV50_UPDATE_DISPLAY, 0);
}

static void ComputeAspectScale(DisplayModePtr mode, DisplayModePtr adjusted_mode, int *outX, int *outY)
{
	float scaleX, scaleY, scale;

	scaleX = adjusted_mode->HDisplay / (float)mode->HDisplay;
	scaleY = adjusted_mode->VDisplay / (float)mode->VDisplay;

	if(scaleX > scaleY)
		scale = scaleY;
	else
		scale = scaleX;

	*outX = mode->HDisplay * scale;
	*outY = mode->VDisplay * scale;
}

void NV50CrtcSetScale(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode, enum scaling_modes scale)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcSetScale is called.\n");

	int outX = 0, outY = 0;

	switch(scale) {
		case SCALE_ASPECT:
			ComputeAspectScale(mode, adjusted_mode, &outX, &outY);
			break;
		case SCALE_PANEL:
		case SCALE_FULLSCREEN:
			outX = adjusted_mode->HDisplay;
			outY = adjusted_mode->VDisplay;
			break;
		case SCALE_NOSCALE:
		default:
			outX = mode->HDisplay;
			outY = mode->VDisplay;
			break;
	}

	/* What kind of mode is this precisely? */
	if ((mode->Flags & V_DBLSCAN) || (mode->Flags & V_INTERLACE) ||
		mode->HDisplay != outX || mode->VDisplay != outY) {
		NV50CrtcCommand(crtc, NV50_CRTC0_SCALE_CTRL, 9);
	} else {
		NV50CrtcCommand(crtc, NV50_CRTC0_SCALE_CTRL, 0);
	}
	NV50CrtcCommand(crtc, NV50_CRTC0_SCALE_REG1, outY << 16 | outX);
	NV50CrtcCommand(crtc, NV50_CRTC0_SCALE_REG2, outY << 16 | outX);
}

void
NV50CrtcCommit(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcCommit is called.\n");

	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	int i, crtc_mask = 0;

	/* If any heads are unused, blank them */
	for(i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if (output->crtc) {
			/* XXXagp: This assumes that xf86_config->crtc[i] is HEADi */
			crtc_mask |= 1 << NV50CrtcGetHead(output->crtc);
		}
	}

	for(i = 0; i < xf86_config->num_crtc; i++) {
		if(!((1 << i) & crtc_mask)) {
			NV50CrtcBlankScreen(xf86_config->crtc[i], TRUE);
		}
	}

	xf86_reload_cursors (pScrn->pScreen);

	NV50DisplayCommand(pScrn, NV50_UPDATE_DISPLAY, 0);

	nv_crtc->modeset_lock = FALSE;
}

