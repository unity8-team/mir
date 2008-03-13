/*
 * Copyright (c) 2007 NVIDIA, Corporation
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

/*
 * PLL calculation.  pclk is in kHz.
 */
static void
NV50CalcPLL(float pclk, int *pNA, int *pMA, int *pNB, int *pMB, int *pP)
{
    const float refclk = 27000.0f;
    const float minVcoA = 100000;
    const float maxVcoA = 400000;
    const float minVcoB = 600000;
    float maxVcoB = 1400000;
    const float minUA = 2000;
    const float maxUA = 400000;
    const float minUB = 50000;
    const float maxUB = 200000;
    const int minNA = 1, maxNA = 255;
    const int minNB = 1, maxNB = 31;
    const int minMA = 1, maxMA = 255;
    const int minMB = 1, maxMB = 31;
    const int minP = 0, maxP = 6;
    int lowP, highP;
    float vcoB;

    int na, ma, nb, mb, p;
    float bestError = FLT_MAX;

    *pNA = *pMA = *pNB = *pMB = *pP = 0;

    if(maxVcoB < pclk + pclk / 200)
        maxVcoB = pclk + pclk / 200;
    if(minVcoB / (1 << maxP) > pclk)
        pclk = minVcoB / (1 << maxP);

    vcoB = maxVcoB - maxVcoB / 200;
    lowP = minP;
    vcoB /= 1 << (lowP + 1);

    while(pclk <= vcoB && lowP < maxP)
    {
        vcoB /= 2;
        lowP++;
    }

    vcoB = maxVcoB + maxVcoB / 200;
    highP = lowP;
    vcoB /= 1 << (highP + 1);

    while(pclk <= vcoB && highP < maxP)
    {
        vcoB /= 2;
        highP++;
    }

    for(p = lowP; p <= highP; p++)
    {
        for(ma = minMA; ma <= maxMA; ma++)
        {
            if(refclk / ma < minUA)
                break;
            else if(refclk / ma > maxUA)
                continue;

            for(na = minNA; na <= maxNA; na++)
            {
                if(refclk * na / ma < minVcoA || refclk * na / ma > maxVcoA)
                    continue;

                for(mb = minMB; mb <= maxMB; mb++)
                {
                    if(refclk * na / ma / mb < minUB)
                        break;
                    else if(refclk * na / ma / mb > maxUB)
                        continue;

                    nb = rint(pclk * (1 << p) * (ma / (float)na) * mb / refclk);

                    if(nb > maxNB)
                        break;
                    else if(nb < minNB)
                        continue;
                    else
                    {
                        float freq = refclk * (na / (float)ma) * (nb / (float)mb) / (1 << p);
                        float error = fabsf(pclk - freq);
                        if(error < bestError) {
                            *pNA = na;
                            *pMA = ma;
                            *pNB = nb;
                            *pMB = mb;
                            *pP = p;
                            bestError = error;
                        }
                    }
                }
            }
        }
    }
}

void NV50CrtcSetPClk(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	int lo_n, lo_m, hi_n, hi_m, p, i;
	/* These clocks are probably rerouted from the 0x4000 range to the 0x610000 range */
	CARD32 lo = NVRead(pNv, nv_crtc->head ? NV50_CRTC_VPLL2_A : NV50_CRTC_VPLL1_A);
	CARD32 hi = NVRead(pNv, nv_crtc->head ? NV50_CRTC_VPLL2_B : NV50_CRTC_VPLL1_B);

	NVWrite(pNv, 0x00614100 + nv_crtc->head * 0x800, 0x10000610);
	lo &= 0xff00ff00;
	hi &= 0x8000ff00;

	NV50CalcPLL(nv_crtc->pclk, &lo_n, &lo_m, &hi_n, &hi_m, &p);

	lo |= (lo_m << 16) | lo_n;
	hi |= (p << 28) | (hi_m << 16) | hi_n;
	NVWrite(pNv, nv_crtc->head ? NV50_CRTC_VPLL2_A : NV50_CRTC_VPLL1_A, lo);
	NVWrite(pNv, nv_crtc->head ? NV50_CRTC_VPLL2_B : NV50_CRTC_VPLL1_B, hi);
	NVWrite(pNv, 0x00614200 + nv_crtc->head * 0x800, 0);

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
	NVWrite(pNv, 0x0061a004 + DAC0 * 0x800, 0x80550000);
	NVWrite(pNv, 0x0061a010 + DAC0 * 0x800, 0x00000001);
	NVWrite(pNv, 0x0061a004 + DAC1 * 0x800, 0x80550000);
	NVWrite(pNv, 0x0061a010 + DAC1 * 0x800, 0x00000001);
	NVWrite(pNv, 0x0061a004 + DAC2 * 0x800, 0x80550000);
	NVWrite(pNv, 0x0061a010 + DAC2 * 0x800, 0x00000001);

	return TRUE;
}

Bool
NV50DispInit(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	if (NVRead(pNv, 0x00610024) & 0x100) {
		NVWrite(pNv, 0x00610024, 0x100);
		NVWrite(pNv, 0x006194e8, NVRead(pNv, 0x006194e8) & ~1);
		while (NVRead(pNv, 0x006194e8) & 2);
	}

	NVWrite(pNv, 0x00610200, 0x2b00);
	/* A bugfix (#12637) from the nv driver, to unlock the driver if it's left in a poor state */
	do {
		CARD32 val = NVRead(pNv, 0x00610200);
		if ((val & 0x9f0000) == 0x20000)
			NVWrite(pNv, 0x00610200, val | 0x800000);

		if ((val & 0x3f0000) == 0x30000)
			NVWrite(pNv, 0x00610200, val | 0x200000);
	} while ((NVRead(pNv, 0x00610200) & 0x1e0000) != 0);
	NVWrite(pNv, 0x00610300, 0x1);
	NVWrite(pNv, 0x00610200, 0x1000b03);
	while (!(NVRead(pNv, 0x00610200) & 0x40000000));

	NV50DisplayCommand(pScrn, 0x84, 0);
	NV50DisplayCommand(pScrn, 0x88, 0);
	NV50DisplayCommand(pScrn, 0x874, 0);
	NV50DisplayCommand(pScrn, 0x800, 0);
	NV50DisplayCommand(pScrn, 0x810, 0);
	NV50DisplayCommand(pScrn, 0x82c, 0);

	return TRUE;
}

void
NV50DispShutdown(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	for(i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		NV50CrtcBlankScreen(crtc, TRUE);
	}

	NV50DisplayCommand(pScrn, 0x80, 0);

	for(i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		if(crtc->enabled) {
			const CARD32 mask = 4 << NV50CrtcGetHead(crtc);

			NVWrite(pNv, 0x00610024, mask);
			while(!(NVRead(pNv, 0x00610024) & mask));
		}
	}

	NVWrite(pNv, 0x00610200, 0x0);
	NVWrite(pNv, 0x00610300, 0x0);
	while ((NVRead(pNv, 0x00610200) & 0x1e0000) != 0);
	while ((NVRead(pNv, 0x0061c030 + SOR0 * 0x800) & 0x10000000));
	while ((NVRead(pNv, 0x0061c030 + SOR1 * 0x800) & 0x10000000));
}

void
NV50CrtcDoModeFixup(DisplayModePtr dst, const DisplayModePtr src)
{
	/* Magic mode timing fudge factor */
	const int fudge = ((src->Flags & V_INTERLACE) && (src->Flags & V_DBLSCAN)) ? 2 : 1;
	const int interlaceDiv = (src->Flags & V_INTERLACE) ? 2 : 1;

	/* Stash the src timings in the Crtc fields in dst */
	dst->CrtcHBlankStart = src->CrtcVTotal << 16 | src->CrtcHTotal;
	dst->CrtcHSyncEnd = ((src->CrtcVSyncEnd - src->CrtcVSyncStart) / interlaceDiv - 1) << 16 |
		(src->CrtcHSyncEnd - src->CrtcHSyncStart - 1);
	dst->CrtcHBlankEnd = ((src->CrtcVBlankEnd - src->CrtcVSyncStart) / interlaceDiv - fudge) << 16 |
		(src->CrtcHBlankEnd - src->CrtcHSyncStart - 1);
	dst->CrtcHTotal = ((src->CrtcVTotal - src->CrtcVSyncStart + src->CrtcVBlankStart) / interlaceDiv - fudge) << 16 |
		(src->CrtcHTotal - src->CrtcHSyncStart + src->CrtcHBlankStart - 1);
	dst->CrtcHSkew = ((src->CrtcVTotal + src->CrtcVBlankEnd - src->CrtcVSyncStart) / 2 - 2) << 16 |
		((2*src->CrtcVTotal - src->CrtcVSyncStart + src->CrtcVBlankStart) / 2 - 2);
}

Bool
NV50CrtcModeFixup(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	if (nv_crtc->skipModeFixup)
		return TRUE;

	NV50CrtcDoModeFixup(adjusted_mode, mode);
	return TRUE;
}

void
NV50CrtcModeSet(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode, int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	const int HDisplay = adjusted_mode->HDisplay;
	const int VDisplay = adjusted_mode->VDisplay;

	nv_crtc->pclk = adjusted_mode->Clock;

	/* NV50CrtcCommand includes head offset */
	NV50CrtcCommand(crtc, NV50_CRTC0_CLOCK, adjusted_mode->Clock | 0x800000);
	NV50CrtcCommand(crtc, NV50_CRTC0_INTERLACE, (adjusted_mode->Flags & V_INTERLACE) ? 2 : 0);
	NV50CrtcCommand(crtc, 0x810, 0);
	NV50CrtcCommand(crtc, 0x82c, 0);
	/* This confirms my suspicion that recent nvidia hardware does no vertical programming */
	/* NV40 still has it as a legacy mode, and i don't know how to do the "new" way, but it definately exists */
	NV50CrtcCommand(crtc, NV50_CRTC0_HBLANK_START, adjusted_mode->CrtcHBlankStart);
	NV50CrtcCommand(crtc, NV50_CRTC0_HSYNC_END, adjusted_mode->CrtcHSyncEnd);
	NV50CrtcCommand(crtc, NV50_CRTC0_HBLANK_END, adjusted_mode->CrtcHBlankEnd);
	NV50CrtcCommand(crtc, NV50_CRTC0_HTOTAL, adjusted_mode->CrtcHTotal);
	if(adjusted_mode->Flags & V_INTERLACE) {
		NV50CrtcCommand(crtc, 0x824, adjusted_mode->CrtcHSkew);
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
	NV50CrtcSetDither(crtc, nv_crtc->ditherEnabled, FALSE);
	NV50CrtcCommand(crtc, 0x8a8, 0x40000);
	NV50CrtcCommand(crtc, NV50_CRTC0_FB_POS, y << 16 | x);
	NV50CrtcCommand(crtc, NV50_CRTC0_SCRN_SIZE, VDisplay << 16 | HDisplay);
	NV50CrtcCommand(crtc, 0x8d4, 0);

	NV50CrtcBlankScreen(crtc, FALSE);
}

void
NV50CrtcBlankScreen(xf86CrtcPtr crtc, Bool blank)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	if(blank) {
		NV50CrtcShowHideCursor(crtc, FALSE, FALSE);

		NV50CrtcCommand(crtc, NV50_CRTC0_CLUT_MODE, NV50_CRTC0_CLUT_MODE_BLANK);
		NV50CrtcCommand(crtc, NV50_CRTC0_CLUT_OFFSET, 0);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, 0x85c, 0);
		NV50CrtcCommand(crtc, 0x874, 0);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, 0x89c, 0);
	} else {
		NV50CrtcCommand(crtc, NV50_CRTC0_FB_OFFSET, pNv->FB->offset >> 8);
		NV50CrtcCommand(crtc, 0x864, 0);
		NVWrite(pNv, 0x00610380, 0);
		/* RAM is clamped to 256 MiB. */
		NVWrite(pNv, NV50_CRTC0_RAM_AMOUNT, pNv->RamAmountKBytes * 1024 - 1);
		NVWrite(pNv, 0x00610388, 0x150000);
		NVWrite(pNv, 0x0061038C, 0);
		NV50CrtcCommand(crtc, NV50_CRTC0_CURSOR_OFFSET, pNv->Cursor->offset >> 8);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, 0x89c, 1);
		if(nv_crtc->cursorVisible)
			NV50CrtcShowHideCursor(crtc, TRUE, FALSE);
		NV50CrtcCommand(crtc, NV50_CRTC0_CLUT_MODE, 
			pScrn->depth == 8 ? NV50_CRTC0_CLUT_MODE_OFF : NV50_CRTC0_CLUT_MODE_ON);
		NV50CrtcCommand(crtc, NV50_CRTC0_CLUT_OFFSET, pNv->CLUT->offset >> 8);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, 0x85c, 1);
		NV50CrtcCommand(crtc, 0x874, 1);
     }
}

/******************************** Cursor stuff ********************************/
static void NV50CrtcShowHideCursor(xf86CrtcPtr crtc, Bool show, Bool update)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	NV50CrtcCommand(crtc, NV50_CRTC0_CURSOR0, 
		show ? NV50_CRTC0_CURSOR0_SHOW : NV50_CRTC0_CURSOR0_HIDE);
	if(update) {
		nv_crtc->cursorVisible = show;
		NV50CrtcCommand(crtc, 0x80, 0);
	}
}

void NV50CrtcShowCursor(xf86CrtcPtr crtc)
{
	NV50CrtcShowHideCursor(crtc, TRUE, TRUE);
}

void NV50CrtcHideCursor(xf86CrtcPtr crtc)
{
	NV50CrtcShowHideCursor(crtc, FALSE, TRUE);
}

/******************************** CRTC stuff ********************************/

void
NV50CrtcPrepare(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	for(i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if(!output->crtc)
			output->funcs->mode_set(output, NULL, NULL);
	}

	nv_crtc->skipModeFixup = FALSE;
}

void
NV50CrtcSkipModeFixup(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	nv_crtc->skipModeFixup = TRUE;
}

void
NV50CrtcSetDither(xf86CrtcPtr crtc, Bool dither, Bool update)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	nv_crtc->ditherEnabled = dither;

	NV50CrtcCommand(crtc, 0x8a0, dither ? 0x11 : 0);
	if(update) 
		NV50CrtcCommand(crtc, 0x80, 0);
}

static void ComputeAspectScale(DisplayModePtr mode, int *outX, int *outY)
{
	float scaleX, scaleY, scale;

	scaleX = mode->CrtcHDisplay / (float)mode->HDisplay;
	scaleY = mode->CrtcVDisplay / (float)mode->VDisplay;

	if(scaleX > scaleY)
		scale = scaleY;
	else
		scale = scaleX;

	*outX = mode->HDisplay * scale;
	*outY = mode->VDisplay * scale;
}

void NV50CrtcSetScale(xf86CrtcPtr crtc, DisplayModePtr mode, enum scaling_modes scale)
{
	int outX = 0, outY = 0;

	switch(scale) {
		case SCALE_ASPECT:
			ComputeAspectScale(mode, &outX, &outY);
			break;
		case SCALE_PANEL:
		case SCALE_FULLSCREEN:
			outX = mode->CrtcHDisplay;
			outY = mode->CrtcVDisplay;
			break;
		case SCALE_NOSCALE:
		default:
			outX = mode->HDisplay;
			outY = mode->VDisplay;
			break;
	}

	if ((mode->Flags & V_DBLSCAN) || (mode->Flags & V_INTERLACE) ||
		mode->HDisplay != outX || mode->VDisplay != outY) {
		NV50CrtcCommand(crtc, 0x8a4, 9);
	} else {
		NV50CrtcCommand(crtc, 0x8a4, 0);
	}
	NV50CrtcCommand(crtc, 0x8d8, outY << 16 | outX);
	NV50CrtcCommand(crtc, 0x8dc, outY << 16 | outX);
}

void
NV50CrtcCommit(xf86CrtcPtr crtc)
{
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

	NV50CrtcCommand(crtc, 0x80, 0);
}

