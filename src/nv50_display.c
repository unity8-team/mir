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

static void
NV50CrtcSetPClk(xf86CrtcPtr crtc)
{
    NVPtr pNv = NVPTR(crtc->scrn);
    NV50CrtcPrivPtr pPriv = crtc->driver_private;
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
    const int headOff = 0x800 * pPriv->head;
    int lo_n, lo_m, hi_n, hi_m, p, i;
    CARD32 lo = pNv->REGS[(0x00614104+headOff)/4];
    CARD32 hi = pNv->REGS[(0x00614108+headOff)/4];

    pNv->REGS[(0x00614100+headOff)/4] = 0x10000610;
    lo &= 0xff00ff00;
    hi &= 0x8000ff00;

    NV50CalcPLL(pPriv->pclk, &lo_n, &lo_m, &hi_n, &hi_m, &p);

    lo |= (lo_m << 16) | lo_n;
    hi |= (p << 28) | (hi_m << 16) | hi_n;
    pNv->REGS[(0x00614104+headOff)/4] = lo;
    pNv->REGS[(0x00614108+headOff)/4] = hi;
    pNv->REGS[(0x00614200+headOff)/4] = 0;

    for(i = 0; i < xf86_config->num_output; i++) {
        xf86OutputPtr output = xf86_config->output[i];

        if(output->crtc != crtc)
            continue;
        NV50OutputSetPClk(output, pPriv->pclk);
    }
}

void
NV50DispCommand(ScrnInfoPtr pScrn, CARD32 addr, CARD32 data)
{
    NVPtr pNv = NVPTR(pScrn);

    pNv->REGS[0x00610304/4] = data;
    pNv->REGS[0x00610300/4] = addr | 0x80010001;

    while(pNv->REGS[0x00610300/4] & 0x80000000) {
        const int super = ffs((pNv->REGS[0x00610024/4] >> 4) & 7);

        if(super) {
            if(super == 2) {
                xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
                const CARD32 r = pNv->REGS[0x00610030/4];
                int i;

                for(i = 0; i < xf86_config->num_crtc; i++)
                {
                    xf86CrtcPtr crtc = xf86_config->crtc[i];
                    NV50CrtcPrivPtr pPriv = crtc->driver_private;

                    if(r & (0x200 << pPriv->head))
                        NV50CrtcSetPClk(crtc);
                }
            }

            pNv->REGS[0x00610024/4] = 8 << super;
            pNv->REGS[0x00610030/4] = 0x80000000;
        }
    }
}

Head
NV50CrtcGetHead(xf86CrtcPtr crtc)
{
    NV50CrtcPrivPtr pPriv = crtc->driver_private;
    return pPriv->head;
}

Bool
NV50DispPreInit(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);

    pNv->REGS[0x00610184/4] = pNv->REGS[0x00614004/4];
    pNv->REGS[0x00610190/4] = pNv->REGS[0x00616100/4];
    pNv->REGS[0x006101a0/4] = pNv->REGS[0x00616900/4];
    pNv->REGS[0x00610194/4] = pNv->REGS[0x00616104/4];
    pNv->REGS[0x006101a4/4] = pNv->REGS[0x00616904/4];
    pNv->REGS[0x00610198/4] = pNv->REGS[0x00616108/4];
    pNv->REGS[0x006101a8/4] = pNv->REGS[0x00616908/4];
    pNv->REGS[0x0061019C/4] = pNv->REGS[0x0061610C/4];
    pNv->REGS[0x006101ac/4] = pNv->REGS[0x0061690c/4];
    pNv->REGS[0x006101D0/4] = pNv->REGS[0x0061A000/4];
    pNv->REGS[0x006101D4/4] = pNv->REGS[0x0061A800/4];
    pNv->REGS[0x006101D8/4] = pNv->REGS[0x0061B000/4];
    pNv->REGS[0x006101E0/4] = pNv->REGS[0x0061C000/4];
    pNv->REGS[0x006101E4/4] = pNv->REGS[0x0061C800/4];
    pNv->REGS[0x0061A004/4] = 0x80550000;
    pNv->REGS[0x0061A010/4] = 0x00000001;
    pNv->REGS[0x0061A804/4] = 0x80550000;
    pNv->REGS[0x0061A810/4] = 0x00000001;
    pNv->REGS[0x0061B004/4] = 0x80550000;
    pNv->REGS[0x0061B010/4] = 0x00000001;

    return TRUE;
}

Bool
NV50DispInit(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);

    if(pNv->REGS[0x00610024/4] & 0x100) {
        pNv->REGS[0x00610024/4] = 0x100;
        pNv->REGS[0x006194E8/4] &= ~1;
        while(pNv->REGS[0x006194E8/4] & 2);
    }

    pNv->REGS[0x00610200/4] = 0x2b00;
    while((pNv->REGS[0x00610200/4] & 0x1e0000) != 0);
    pNv->REGS[0x00610300/4] = 1;
    pNv->REGS[0x00610200/4] = 0x1000b03;
    while(!(pNv->REGS[0x00610200/4] & 0x40000000));

    C(0x00000084, 0);
    C(0x00000088, 0);
    C(0x00000874, 0);
    C(0x00000800, 0);
    C(0x00000810, 0);
    C(0x0000082C, 0);

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

    C(0x00000080, 0);

    for(i = 0; i < xf86_config->num_crtc; i++) {
        xf86CrtcPtr crtc = xf86_config->crtc[i];

        if(crtc->enabled) {
            const CARD32 mask = 4 << NV50CrtcGetHead(crtc);

            pNv->REGS[0x00610024/4] = mask;
            while(!(pNv->REGS[0x00610024/4] & mask));
        }
    }

    pNv->REGS[0x00610200/4] = 0;
    pNv->REGS[0x00610300/4] = 0;
    while ((pNv->REGS[0x00610200/4] & 0x1e0000) != 0);
    while ((pNv->REGS[0x0061C030/4] & 0x10000000));
    while ((pNv->REGS[0x0061C830/4] & 0x10000000));
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
NV50CrtcModeFixup(xf86CrtcPtr crtc,
                 DisplayModePtr mode, DisplayModePtr adjusted_mode)
{
    NV50CrtcPrivPtr pPriv = crtc->driver_private;

    if (pPriv->skipModeFixup)
	return TRUE;

    NV50CrtcDoModeFixup(adjusted_mode, mode);
    return TRUE;
}

void
NV50CrtcModeSet(xf86CrtcPtr crtc, DisplayModePtr mode,
               DisplayModePtr adjusted_mode, int x, int y)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    NV50CrtcPrivPtr pPriv = crtc->driver_private;
    const int HDisplay = adjusted_mode->HDisplay;
    const int VDisplay = adjusted_mode->VDisplay;
    const int headOff = 0x400 * NV50CrtcGetHead(crtc);

    pPriv->pclk = adjusted_mode->Clock;

    C(0x00000804 + headOff, adjusted_mode->Clock | 0x800000);
    C(0x00000808 + headOff, (adjusted_mode->Flags & V_INTERLACE) ? 2 : 0);
    C(0x00000810 + headOff, 0);
    C(0x0000082C + headOff, 0);
    C(0x00000814 + headOff, adjusted_mode->CrtcHBlankStart);
    C(0x00000818 + headOff, adjusted_mode->CrtcHSyncEnd);
    C(0x0000081C + headOff, adjusted_mode->CrtcHBlankEnd);
    C(0x00000820 + headOff, adjusted_mode->CrtcHTotal);
    if(adjusted_mode->Flags & V_INTERLACE)
        C(0x00000824 + headOff, adjusted_mode->CrtcHSkew);
    C(0x00000868 + headOff, pScrn->virtualY << 16 | pScrn->virtualX);
    C(0x0000086C + headOff, pScrn->displayWidth * (pScrn->bitsPerPixel / 8) | 0x100000);
    switch(pScrn->depth) {
        case  8: C(0x00000870 + headOff, 0x1E00); break;
        case 15: C(0x00000870 + headOff, 0xE900); break;
        case 16: C(0x00000870 + headOff, 0xE800); break;
        case 24: C(0x00000870 + headOff, 0xCF00); break;
    }
    NV50CrtcSetDither(crtc, pPriv->dither, FALSE);
    C(0x000008A8 + headOff, 0x40000);
    C(0x000008C0 + headOff, y << 16 | x);
    C(0x000008C8 + headOff, VDisplay << 16 | HDisplay);
    C(0x000008D4 + headOff, 0);

    NV50CrtcBlankScreen(crtc, FALSE);
}

void
NV50CrtcBlankScreen(xf86CrtcPtr crtc, Bool blank)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    NVPtr pNv = NVPTR(pScrn);
    NV50CrtcPrivPtr pPriv = crtc->driver_private;
    const int headOff = 0x400 * pPriv->head;

    if(blank) {
        NV50CrtcShowHideCursor(crtc, FALSE, FALSE);

        C(0x00000840 + headOff, 0);
        C(0x00000844 + headOff, 0);
        if(pNv->NVArch != 0x50)
            C(0x0000085C + headOff, 0);
        C(0x00000874 + headOff, 0);
        if(pNv->NVArch != 0x50)
            C(0x0000089C + headOff, 0);
    } else {
        C(0x00000860 + headOff, pNv->FB->offset >> 8);
        C(0x00000864 + headOff, 0);
        pNv->REGS[0x00610380/4] = 0;
	/*XXX: in "nv" this is total vram size.  our RamAmountKBytes is clamped
	 *     to 256MiB.
	 */
        pNv->REGS[0x00610384/4] = pNv->RamAmountKBytes * 1024 - 1;
        pNv->REGS[0x00610388/4] = 0x150000;
        pNv->REGS[0x0061038C/4] = 0;
        C(0x00000884 + headOff, pNv->Cursor->offset >> 8);
        if(pNv->NVArch != 0x50)
            C(0x0000089C + headOff, 1);
        if(pPriv->cursorVisible)
            NV50CrtcShowHideCursor(crtc, TRUE, FALSE);
        C(0x00000840 + headOff, pScrn->depth == 8 ? 0x80000000 : 0xc0000000);
        C(0x00000844 + headOff, pNv->CLUT->offset >> 8);
        if(pNv->NVArch != 0x50)
            C(0x0000085C + headOff, 1);
        C(0x00000874 + headOff, 1);
    }
}

/******************************** Cursor stuff ********************************/
static void NV50CrtcShowHideCursor(xf86CrtcPtr crtc, Bool show, Bool update)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    NV50CrtcPrivPtr pPriv = crtc->driver_private;
    const int headOff = 0x400 * NV50CrtcGetHead(crtc);

    C(0x00000880 + headOff, show ? 0x85000000 : 0x5000000);
    if(update) {
        pPriv->cursorVisible = show;
        C(0x00000080, 0);
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
    NV50CrtcPrivPtr pPriv = crtc->driver_private;
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int i;

    for(i = 0; i < xf86_config->num_output; i++) {
        xf86OutputPtr output = xf86_config->output[i];

        if(!output->crtc)
            output->funcs->mode_set(output, NULL, NULL);
    }

    pPriv->skipModeFixup = FALSE;
}

void
NV50CrtcSkipModeFixup(xf86CrtcPtr crtc)
{
    NV50CrtcPrivPtr pPriv = crtc->driver_private;
    pPriv->skipModeFixup = TRUE;
}

void
NV50CrtcSetDither(xf86CrtcPtr crtc, Bool dither, Bool update)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    NV50CrtcPrivPtr pPriv = crtc->driver_private;
    const int headOff = 0x400 * NV50CrtcGetHead(crtc);

    pPriv->dither = dither;

    C(0x000008A0 + headOff, dither ? 0x11 : 0);
    if(update) C(0x00000080, 0);
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

void NV50CrtcSetScale(xf86CrtcPtr crtc, DisplayModePtr mode,
                     enum NV50ScaleMode scale)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    NV50CrtcPrivPtr pPriv = crtc->driver_private;
    const int headOff = 0x400 * pPriv->head;
    int outX, outY;

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

    if((mode->Flags & V_DBLSCAN) || (mode->Flags & V_INTERLACE) ||
       mode->HDisplay != outX || mode->VDisplay != outY) {
        C(0x000008A4 + headOff, 9);
    } else {
        C(0x000008A4 + headOff, 0);
    }
    C(0x000008D8 + headOff, outY << 16 | outX);
    C(0x000008DC + headOff, outY << 16 | outX);
}

void
NV50CrtcCommit(xf86CrtcPtr crtc)
{
    ScrnInfoPtr pScrn = crtc->scrn;
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
    int i, crtc_mask = 0;

    /* If any heads are unused, blank them */
    for(i = 0; i < xf86_config->num_output; i++) {
        xf86OutputPtr output = xf86_config->output[i];

        if(output->crtc)
            /* XXXagp: This assumes that xf86_config->crtc[i] is HEADi */
            crtc_mask |= 1 << NV50CrtcGetHead(output->crtc);
    }

    for(i = 0; i < xf86_config->num_crtc; i++)
        if(!((1 << i) & crtc_mask))
            NV50CrtcBlankScreen(xf86_config->crtc[i], TRUE);

    C(0x00000080, 0);
}

#endif /* ENABLE_RANDR12 */
