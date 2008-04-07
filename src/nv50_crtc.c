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

#include "nv_include.h"

/* Don't call the directly, only load state should do this on the long run*/
void NV50CheckWriteVClk(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int t_start = GetTimeInMillis();

	while (NVRead(pNv, NV50_DISPLAY_CTRL_STATE) & NV50_DISPLAY_CTRL_STATE_PENDING) {
		/* An educated guess. */
		const uint32_t supervisor = NVRead(pNv, NV50_DISPLAY_SUPERVISOR);

		/* Just in case something goes bad, at least you can blindly restart your machine. */
		if ((GetTimeInMillis() - t_start) > 5000) {
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "NV50CheckWriteVClk() timed out.\n");
			xf86DrvMsg(pScrn->scrnIndex, X_INFO, "A reboot is probably required now.\n");
			break;
		}

		/* Simply acknowledge it, maybe we should do more? */
		if (supervisor & NV50_DISPLAY_SUPERVISOR_CRTCn) {
			NVWrite(pNv, NV50_DISPLAY_SUPERVISOR, supervisor & NV50_DISPLAY_SUPERVISOR_CRTCn);
		}

		if (supervisor & NV50_DISPLAY_SUPERVISOR_CLK_MASK) {
			if (supervisor & NV50_DISPLAY_SUPERVISOR_CLK_UPDATE) {
				xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
				const uint32_t clockvar = NVRead(pNv, NV50_DISPLAY_UNK30_CTRL);
				int i;

				for(i = 0; i < xf86_config->num_crtc; i++) {
					xf86CrtcPtr crtc = xf86_config->crtc[i];
					NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
					uint32_t mask = 0;

					if (nv_crtc->head == 1)
						mask = NV50_DISPLAY_UNK30_CTRL_UPDATE_VCLK1;
					else
						mask = NV50_DISPLAY_UNK30_CTRL_UPDATE_VCLK0;

					/* Always do something if the supervisor wants a clock change. */
					/* This is needed because you get a deadlock if you don't kick the NV50_CRTC0_CLK_CTRL2 register. */
					if (nv_crtc->modeset_lock || (clockvar & mask))
						NV50CrtcSetPClk(crtc, !!(clockvar & mask));
				}
			}

			NVWrite(pNv, NV50_DISPLAY_SUPERVISOR, 1 << (ffs(supervisor & NV50_DISPLAY_SUPERVISOR_CLK_MASK) - 1));
			NVWrite(pNv, NV50_DISPLAY_UNK30_CTRL, NV50_DISPLAY_UNK30_CTRL_PENDING);
		}
	}
}

void NV50DisplayCommand(ScrnInfoPtr pScrn, uint32_t addr, uint32_t value)
{
	DDXMMIOH("NV50DisplayCommand: head %d addr 0x%X value 0x%X\n", 0, addr, value);
	NVPtr pNv = NVPTR(pScrn);
	NVWrite(pNv, NV50_DISPLAY_CTRL_VAL, value);
	NVWrite(pNv, NV50_DISPLAY_CTRL_STATE, addr | 0x10000 | NV50_DISPLAY_CTRL_STATE_ENABLE | NV50_DISPLAY_CTRL_STATE_PENDING);
	NV50CheckWriteVClk(pScrn);
}

void NV50CrtcCommand(xf86CrtcPtr crtc, uint32_t addr, uint32_t value)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	/* This head dependent offset may not be true everywere */
	NV50DisplayCommand(pScrn, addr + 0x400 * nv_crtc->head, value);
}

void NV50CrtcSetPClk(xf86CrtcPtr crtc, Bool update)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcSetPClk is called (%s).\n", update ? "update" : "no update");

	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	NVPtr pNv = NVPTR(pScrn);
	int i;

	/* Sometimes NV50_CRTC0_CLK_CTRL2 needs a kick, but the clock needs no update. */
	if (update) {
		/* I don't know why exactly, but these were in my table. */
		uint32_t pll_reg = nv_crtc->head ? NV50_CRTC1_CLK_CTRL1 : NV50_CRTC0_CLK_CTRL1;

		int NM1 = 0xbeef, NM2 = 0xdead, log2P;
		struct pll_lims pll_lim;
		get_pll_limits(pScrn, pll_reg, &pll_lim);
		/* NV5x hardware doesn't seem to support a single vco mode, otherwise the blob is hiding it well. */
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
	}

	/* There seem to be a few indicator bits, which are similar to the SOR_CTRL bits. */
	NVWrite(pNv, NV50_CRTC0_CLK_CTRL2 + nv_crtc->head * 0x800, 0);

	for(i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if(output->crtc != crtc)
			continue;
		NV50OutputSetPClk(output, nv_crtc->pclk);
	}
}

static void
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

static void
nv50_crtc_mode_set(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode, int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_mode_set is called for %s with position (%d, %d).\n", nv_crtc->head ? "CRTC1" : "CRTC0", x, y);

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
	NV50CrtcCommand(crtc, NV50_CRTC0_UNK82C, 0);
	NV50CrtcCommand(crtc, NV50_CRTC0_DISPLAY_TOTAL, adjusted_mode->CrtcVTotal << 16 | adjusted_mode->CrtcHTotal);
	NV50CrtcCommand(crtc, NV50_CRTC0_SYNC_DURATION, (vsync_dur - 1) << 16 | (hsync_dur - 1));
	NV50CrtcCommand(crtc, NV50_CRTC0_SYNC_START_TO_BLANK_END, (vsync_start_to_end - 1) << 16 | (hsync_start_to_end - 1));
	NV50CrtcCommand(crtc, NV50_CRTC0_MODE_UNK1, (vunk1 - 1) << 16 | (hunk1 - 1));
	if (adjusted_mode->Flags & V_INTERLACE) {
		NV50CrtcCommand(crtc, NV50_CRTC0_MODE_UNK2, (vunk2b - 1) << 16 | (vunk2a - 1));
	}
	NV50CrtcCommand(crtc, NV50_CRTC0_FB_SIZE, pScrn->virtualY << 16 | pScrn->virtualX);
	NV50CrtcCommand(crtc, NV50_CRTC0_PITCH, pScrn->displayWidth * (pScrn->bitsPerPixel / 8) | 0x100000);
	switch (pScrn->depth) {
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
	NV50CrtcCommand(crtc, NV50_CRTC0_COLOR_CTRL, NV50_CRTC_COLOR_CTRL_MODE_COLOR);
	NV50CrtcCommand(crtc, NV50_CRTC0_FB_POS, y << 16 | x);
	/* This is the actual resolution of the mode. */
	NV50CrtcCommand(crtc, NV50_CRTC0_REAL_RES, (mode->VDisplay << 16) | mode->HDisplay);
	NV50CrtcCommand(crtc, NV50_CRTC0_SCALE_CENTER_OFFSET, NV50_CRTC_SCALE_CENTER_OFFSET_VAL(0,0));

	NV50CrtcBlankScreen(crtc, FALSE);
}

void
NV50CrtcBlankScreen(xf86CrtcPtr crtc, Bool blank)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcBlankScreen is called (%s).\n", blank ? "blanked" : "unblanked");

	NVPtr pNv = NVPTR(pScrn);
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	if (blank) {
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
		NVWrite(pNv, NV50_DISPLAY_UNK_380, 0);
		/* RAM is clamped to 256 MiB. */
		NVWrite(pNv, NV50_DISPLAY_RAM_AMOUNT, pNv->RamAmountKBytes * 1024 - 1);
		NVWrite(pNv, NV50_DISPLAY_UNK_388, 0x150000);
		NVWrite(pNv, NV50_DISPLAY_UNK_38C, 0);
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

static void
nv50_crtc_prepare(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_prepare is called.\n");

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

	/* Got a better name for SCALER_ACTIVE? */
	if ((mode->Flags & V_DBLSCAN) || (mode->Flags & V_INTERLACE) ||
		mode->HDisplay != outX || mode->VDisplay != outY) {
		NV50CrtcCommand(crtc, NV50_CRTC0_SCALE_CTRL, NV50_CRTC0_SCALE_CTRL_SCALER_ACTIVE);
	} else {
		NV50CrtcCommand(crtc, NV50_CRTC0_SCALE_CTRL, NV50_CRTC0_SCALE_CTRL_SCALER_INACTIVE);
	}
	NV50CrtcCommand(crtc, NV50_CRTC0_SCALE_RES1, outY << 16 | outX);
	NV50CrtcCommand(crtc, NV50_CRTC0_SCALE_RES2, outY << 16 | outX);
}

static void
nv50_crtc_commit(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_commit is called.\n");

	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	int i, crtc_mask = 0;

	/* If any heads are unused, blank them */
	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if (output->crtc) {
			NVCrtcPrivatePtr nv_crtc = output->crtc->driver_private;
			crtc_mask |= (1 << nv_crtc->head);
		}
	}

	for (i = 0; i < xf86_config->num_crtc; i++) {
		if(!((1 << i) & crtc_mask)) {
			NV50CrtcBlankScreen(xf86_config->crtc[i], TRUE);
		}
	}

	xf86_reload_cursors (pScrn->pScreen);

	NV50DisplayCommand(pScrn, NV50_UPDATE_DISPLAY, 0);

	nv_crtc->modeset_lock = FALSE;
}

static Bool nv50_crtc_lock(xf86CrtcPtr crtc)
{
	return FALSE;
}

/*
 * The indices are a bit strange, but i'll assume it's correct (taken from nv).
 * The LUT resolution seems to be 14 bits on NV50 as opposed to the 8 bits of previous hardware.
 */
#define NV50_LUT_INDEX(val, w) ((val << (8 - w)) | (val >> ((w << 1) - 8)))
static void
nv50_crtc_gamma_set(xf86CrtcPtr crtc, CARD16 *red, CARD16 *green, CARD16 *blue,
					int size)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "nv50_crtc_gamma_set is called.\n");

	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	NVPtr pNv = NVPTR(pScrn);
	uint32_t index, i;
	void * CLUT = NULL;

	/* Each CRTC has it's own CLUT. */
	if (nv_crtc->head == 1)
		CLUT = pNv->CLUT1->map;
	else
		CLUT = pNv->CLUT0->map;

	volatile struct {
		unsigned short red, green, blue, unused;
	} *lut = (void *) CLUT;

	switch (pScrn->depth) {
	case 15:
		/* R5G5B5 */
		for (i = 0; i < 32; i++) {
			index = NV50_LUT_INDEX(i, 5);
			lut[index].red = red[i] >> 2;
			lut[index].green = green[i] >> 2;
			lut[index].blue = blue[i] >> 2;
		}
		break;
	case 16:
		/* R5G6B5 */
		for (i = 0; i < 32; i++) {
			index = NV50_LUT_INDEX(i, 5);
			lut[index].red = red[i] >> 2;
			lut[index].blue = blue[i] >> 2;
		}

		/* Green has an extra bit. */
		for (i = 0; i < 64; i++) {
			index = NV50_LUT_INDEX(i, 6);
			lut[index].green = green[i] >> 2;
		}
		break;
	default:
		/* R8G8B8 */
		for (i = 0; i < 256; i++) {
			lut[i].red = red[i] >> 2;
			lut[i].green = green[i] >> 2;
			lut[i].blue = blue[i] >> 2;
		}
		break;
	}
}

static void
nv50_crtc_dpms_set(xf86CrtcPtr crtc, int mode)
{
}

static Bool
nv50_crtc_mode_fixup(xf86CrtcPtr crtc, DisplayModePtr mode,
		     DisplayModePtr adjusted_mode)
{
	return TRUE;
}

static const xf86CrtcFuncsRec nv50_crtc_funcs = {
	.dpms = nv50_crtc_dpms_set,
	.save = NULL,
	.restore = NULL,
	.lock = nv50_crtc_lock,
	.unlock = NULL,
	.mode_fixup = nv50_crtc_mode_fixup,
	.prepare = nv50_crtc_prepare,
	.mode_set = nv50_crtc_mode_set,
	.gamma_set = nv50_crtc_gamma_set,
	.commit = nv50_crtc_commit,
	.shadow_create = NULL,
	.shadow_destroy = NULL,
	.set_cursor_position = nv50_crtc_set_cursor_position,
	.show_cursor = nv50_crtc_show_cursor,
	.hide_cursor = nv50_crtc_hide_cursor,
	.load_cursor_argb = nv50_crtc_load_cursor_argb,
	.destroy = NULL,
};

const xf86CrtcFuncsRec * nv50_get_crtc_funcs()
{
	return &nv50_crtc_funcs;
}

