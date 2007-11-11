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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_RANDR12

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
	NV50CrtcPrivPtr nv_crtc = crtc->driver_private;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	int lo_n, lo_m, hi_n, hi_m, p, i;
	CARD32 lo = NV50CrtcRead(crtc, 0x4104);
	CARD32 hi = NV50CrtcRead(crtc, 0x4108);

	NV50CrtcWrite(crtc, 0x4100, 0x10000610);
	lo &= 0xff00ff00;
	hi &= 0x8000ff00;

	NV50CalcPLL(nv_crtc->pclk, &lo_n, &lo_m, &hi_n, &hi_m, &p);

	lo |= (lo_m << 16) | lo_n;
	hi |= (p << 28) | (hi_m << 16) | hi_n;
	NV50CrtcWrite(crtc, 0x4104, lo);
	NV50CrtcWrite(crtc, 0x4108, hi);
	NV50CrtcWrite(crtc, 0x4200, 0);

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
	NV50CrtcPrivPtr nv_crtc = crtc->driver_private;
	return nv_crtc->head;
}

Bool
NV50DispPreInit(ScrnInfoPtr pScrn)
{
	/* These labels are guesswork based on symmetry (2 SOR's and 3 DAC's exist)*/
	NV50DisplayWrite(pScrn, 0x184, NV50DisplayRead(pScrn, 0x4004));
	NV50DisplayWrite(pScrn, 0x190, NV50OrRead(pScrn, SOR0, 0x6100));
	NV50DisplayWrite(pScrn, 0x1a0, NV50OrRead(pScrn, SOR1, 0x6100));
	NV50DisplayWrite(pScrn, 0x194, NV50OrRead(pScrn, SOR0, 0x6104));
	NV50DisplayWrite(pScrn, 0x1a4, NV50OrRead(pScrn, SOR1, 0x6104));
	NV50DisplayWrite(pScrn, 0x198, NV50OrRead(pScrn, SOR0, 0x6108));
	NV50DisplayWrite(pScrn, 0x1a8, NV50OrRead(pScrn, SOR1, 0x6108));
	NV50DisplayWrite(pScrn, 0x19c, NV50OrRead(pScrn, SOR0, 0x610c));
	NV50DisplayWrite(pScrn, 0x1ac, NV50OrRead(pScrn, SOR1, 0x610c));
	NV50DisplayWrite(pScrn, 0x1d0, NV50OrRead(pScrn, DAC0, 0xa000));
	NV50DisplayWrite(pScrn, 0x1d4, NV50OrRead(pScrn, DAC1, 0xa000));
	NV50DisplayWrite(pScrn, 0x1d8, NV50OrRead(pScrn, DAC2, 0xa000));
	NV50DisplayWrite(pScrn, 0x1e0, NV50OrRead(pScrn, SOR0, 0xc000));
	NV50DisplayWrite(pScrn, 0x1e4, NV50OrRead(pScrn, SOR1, 0xc000));
	NV50OrWrite(pScrn, DAC0, 0xa004, 0x80550000);
	NV50OrWrite(pScrn, DAC0, 0xa010, 0x00000001);
	NV50OrWrite(pScrn, DAC1, 0xa004, 0x80550000);
	NV50OrWrite(pScrn, DAC1, 0xa010, 0x00000001);
	NV50OrWrite(pScrn, DAC2, 0xa004, 0x80550000);
	NV50OrWrite(pScrn, DAC2, 0xa010, 0x00000001);

	return TRUE;
}

Bool
NV50DispInit(ScrnInfoPtr pScrn)
{
	if (NV50DisplayRead(pScrn, 0x24) & 0x100) {
		NV50DisplayWrite(pScrn, 0x24, 0x100);
		NV50DisplayWrite(pScrn, 0x94e8, NV50DisplayRead(pScrn, 0x94e8) & ~1);
		while (NV50DisplayRead(pScrn, 0x94e8) & 2);
	}

	NV50DisplayWrite(pScrn, 0x200, 0x2b00);
	/* A bugfix (#12637) from the nv driver, to unlock the driver if it's left in a poor state */
	do {
		CARD32 val = NV50DisplayRead(pScrn, 0x200);
		if ((val & 0x9f0000) == 0x20000)
			NV50DisplayWrite(pScrn, 0x200, val | 0x800000);

		if ((val & 0x3f0000) == 0x30000)
			NV50DisplayWrite(pScrn, 0x200, val | 0x200000);
	} while ((NV50DisplayRead(pScrn, 0x200) & 0x1e0000) != 0);
	NV50DisplayWrite(pScrn, 0x300, 0x1);
	NV50DisplayWrite(pScrn, 0x200, 0x1000b03);
	while (!(NV50DisplayRead(pScrn, 0x200) & 0x40000000));

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

			NV50DisplayWrite(pScrn, 0x24, mask);
			while(!(NV50DisplayRead(pScrn, 0x24) & mask));
		}
	}

	NV50DisplayWrite(pScrn, 0x200, 0x0);
	NV50DisplayWrite(pScrn, 0x300, 0x0);
	while ((NV50DisplayRead(pScrn, 0x200) & 0x1e0000) != 0);
	while ((NV50OrRead(pScrn, SOR0, 0xc030) & 0x10000000));
	while ((NV50OrRead(pScrn, SOR1, 0xc030) & 0x10000000));
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
	NV50CrtcPrivPtr nv_crtc = crtc->driver_private;

	if (nv_crtc->skipModeFixup)
		return TRUE;

	NV50CrtcDoModeFixup(adjusted_mode, mode);
	return TRUE;
}

void
NV50CrtcModeSet(xf86CrtcPtr crtc, DisplayModePtr mode, DisplayModePtr adjusted_mode, int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NV50CrtcPrivPtr nv_crtc = crtc->driver_private;
	const int HDisplay = adjusted_mode->HDisplay;
	const int VDisplay = adjusted_mode->VDisplay;

	nv_crtc->pclk = adjusted_mode->Clock;

	/* NV50CrtcCommand includes head offset */
	NV50CrtcCommand(crtc, 0x804, adjusted_mode->Clock | 0x800000);
	NV50CrtcCommand(crtc, 0x808, (adjusted_mode->Flags & V_INTERLACE) ? 2 : 0);
	NV50CrtcCommand(crtc, 0x810, 0);
	NV50CrtcCommand(crtc, 0x82c, 0);
	NV50CrtcCommand(crtc, 0x814, adjusted_mode->CrtcHBlankStart);
	NV50CrtcCommand(crtc, 0x818, adjusted_mode->CrtcHSyncEnd);
	NV50CrtcCommand(crtc, 0x81c, adjusted_mode->CrtcHBlankEnd);
	NV50CrtcCommand(crtc, 0x820, adjusted_mode->CrtcHTotal);
	if(adjusted_mode->Flags & V_INTERLACE) {
		NV50CrtcCommand(crtc, 0x824, adjusted_mode->CrtcHSkew);
	}
	NV50CrtcCommand(crtc, 0x868, pScrn->virtualY << 16 | pScrn->virtualX);
	NV50CrtcCommand(crtc, 0x86c, pScrn->displayWidth * (pScrn->bitsPerPixel / 8) | 0x100000);
	switch(pScrn->depth) {
		case 8: NV50CrtcCommand(crtc, 0x870, 0x1e00); break;
		case 15: NV50CrtcCommand(crtc, 0x870, 0xe900); break;
		case 16: NV50CrtcCommand(crtc, 0x870, 0xe800); break;
		case 24: NV50CrtcCommand(crtc, 0x870, 0xcf00); break;
	}
	NV50CrtcSetDither(crtc, nv_crtc->dither, FALSE);
	NV50CrtcCommand(crtc, 0x8a8, 0x40000);
	NV50CrtcCommand(crtc, 0x8c0, y << 16 | x);
	NV50CrtcCommand(crtc, 0x8c8, VDisplay << 16 | HDisplay);
	NV50CrtcCommand(crtc, 0x8d4, 0);

	NV50CrtcBlankScreen(crtc, FALSE);
}

void
NV50CrtcBlankScreen(xf86CrtcPtr crtc, Bool blank)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	NV50CrtcPrivPtr nv_crtc = crtc->driver_private;

	if(blank) {
		NV50CrtcShowHideCursor(crtc, FALSE, FALSE);

		NV50CrtcCommand(crtc, 0x840, 0);
		NV50CrtcCommand(crtc, 0x844, 0);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, 0x85c, 0);
		NV50CrtcCommand(crtc, 0x874, 0);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, 0x89c, 0);
	} else {
		NV50CrtcCommand(crtc, 0x860, pNv->FB->offset >> 8);
		NV50CrtcCommand(crtc, 0x864, 0);
		NV50DisplayWrite(pScrn, 0x380, 0);
		/*XXX: in "nv" this is total vram size.  our RamAmountKBytes is clamped
		*     to 256MiB.
		*/
		NV50DisplayWrite(pScrn, 0x384, pNv->RamAmountKBytes * 1024 - 1);
		NV50DisplayWrite(pScrn, 0x388, 0x1500000);
		NV50DisplayWrite(pScrn, 0x38C, 0);
		NV50CrtcCommand(crtc, 0x884, pNv->Cursor->offset >> 8);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, 0x89c, 1);
		if(nv_crtc->cursorVisible)
			NV50CrtcShowHideCursor(crtc, TRUE, FALSE);
		NV50CrtcCommand(crtc, 0x840, pScrn->depth == 8 ? 0x80000000 : 0xc0000000);
		NV50CrtcCommand(crtc, 0x844, pNv->CLUT->offset >> 8);
		if(pNv->NVArch != 0x50)
			NV50CrtcCommand(crtc, 0x85c, 1);
		NV50CrtcCommand(crtc, 0x874, 1);
     }
}

/******************************** Cursor stuff ********************************/
static void NV50CrtcShowHideCursor(xf86CrtcPtr crtc, Bool show, Bool update)
{
	NV50CrtcPrivPtr nv_crtc = crtc->driver_private;

	NV50CrtcCommand(crtc, 0x880, show ? 0x85000000 : 0x5000000);
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
	NV50CrtcPrivPtr nv_crtc = crtc->driver_private;
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
	NV50CrtcPrivPtr nv_crtc = crtc->driver_private;
	nv_crtc->skipModeFixup = TRUE;
}

void
NV50CrtcSetDither(xf86CrtcPtr crtc, Bool dither, Bool update)
{
	NV50CrtcPrivPtr nv_crtc = crtc->driver_private;

	nv_crtc->dither = dither;

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

void NV50CrtcSetScale(xf86CrtcPtr crtc, DisplayModePtr mode, enum NV50ScaleMode scale)
{
	int outX = 0, outY = 0;

	switch(scale) {
		case NV50_SCALE_ASPECT:
			ComputeAspectScale(mode, &outX, &outY);
			break;
		case NV50_SCALE_OFF:
		case NV50_SCALE_FILL:
			outX = mode->CrtcHDisplay;
			outY = mode->CrtcVDisplay;
			break;
		case NV50_SCALE_CENTER:
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

#endif /* ENABLE_RANDR12 */
