/*
 * Copyright 1993-2003 NVIDIA, Corporation
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

/****************************************************************************\
*                                                                            *
*                          RIVA Mode State Routines                          *
*                                                                            *
\****************************************************************************/

/*
 * Calculate the Video Clock parameters for the PLL.
 */
static void CalcVClock (
    int           clockIn,
    int          *clockOut,
    CARD32         *pllOut,
    NVPtr        pNv
)
{
    unsigned lowM, highM;
    unsigned DeltaNew, DeltaOld;
    unsigned VClk, Freq;
    unsigned M, N, P;
    
    DeltaOld = 0xFFFFFFFF;

    VClk = (unsigned)clockIn;
    
    if (pNv->CrystalFreqKHz == 13500) {
        lowM  = 7;
        highM = 13;
    } else {
        lowM  = 8;
        highM = 14;
    }

    for (P = 0; P <= 4; P++) {
        Freq = VClk << P;
        if ((Freq >= 128000) && (Freq <= 350000)) {
            for (M = lowM; M <= highM; M++) {
                N = ((VClk << P) * M) / pNv->CrystalFreqKHz;
                if(N <= 255) {
                    Freq = ((pNv->CrystalFreqKHz * N) / M) >> P;
                    if (Freq > VClk)
                        DeltaNew = Freq - VClk;
                    else
                        DeltaNew = VClk - Freq;
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
    int           clockIn,
    int          *clockOut,
    CARD32         *pllOut,
    CARD32         *pllBOut,
    NVPtr        pNv
)
{
    unsigned DeltaNew, DeltaOld;
    unsigned VClk, Freq;
    unsigned M, N, P;

    DeltaOld = 0xFFFFFFFF;

    *pllBOut = 0x80000401;  /* fixed at x4 for now */

    VClk = (unsigned)clockIn;

    for (P = 0; P <= 6; P++) {
        Freq = VClk << P;
        if ((Freq >= 400000) && (Freq <= 1000000)) {
            for (M = 1; M <= 13; M++) {
                N = ((VClk << P) * M) / (pNv->CrystalFreqKHz << 2);
                if((N >= 5) && (N <= 255)) {
                    Freq = (((pNv->CrystalFreqKHz << 2) * N) / M) >> P;
                    if (Freq > VClk)
                        DeltaNew = Freq - VClk;
                    else
                        DeltaNew = VClk - Freq;
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

/*
 * Calculate extended mode parameters (SVGA) and save in a 
 * mode state structure.
 */
void NVCalcStateExt (
    ScrnInfoPtr pScrn,
    RIVA_HW_STATE *state,
    int            bpp,
    int            width,
    int            hDisplaySize,
    int            height,
    int            dotClock,
    int		   flags 
)
{
	NVPtr pNv = NVPTR(pScrn);
    int pixelDepth, VClk = 0;
	CARD32 CursorStart;

    /*
     * Save mode parameters.
     */
    state->bpp    = bpp;    /* this is not bitsPerPixel, it's 8,15,16,32 */
    state->width  = width;
    state->height = height;
    /*
     * Extended RIVA registers.
     */
    pixelDepth = (bpp + 1)/8;
    if(pNv->two_reg_pll)
        CalcVClock2Stage(dotClock, &VClk, &state->pll, &state->pllB, pNv);
    else
        CalcVClock(dotClock, &VClk, &state->pll, pNv);

    nouveau_calc_arb(pScrn, VClk, pixelDepth * 8, &state->arbitration0, &state->arbitration1);
    switch (pNv->Architecture)
    {
        case NV_ARCH_04:
            state->cursor0  = 0x00;
            state->cursor1  = 0xbC;
	    if (flags & V_DBLSCAN)
		state->cursor1 |= 2;
            state->cursor2  = 0x00000000;
            state->pllsel   = 0x10000700;
            state->general  = bpp == 16 ? 0x00101100 : 0x00100100;
            state->repaint1 = hDisplaySize < 1280 ? 0x04 : 0x00;
            break;
        case NV_ARCH_10:
        case NV_ARCH_20:
        case NV_ARCH_30:
        default:
            CursorStart = pNv->Cursor->offset;
            state->cursor0  = 0x80 | (CursorStart >> 17);
            state->cursor1  = (CursorStart >> 11) << 2;
	    state->cursor2  = CursorStart >> 24;
	    if (flags & V_DBLSCAN) 
		state->cursor1 |= 2;
            state->pllsel   = 0x10000700;
            state->general  = bpp == 16 ? 0x00101100 : 0x00100100;
            state->repaint1 = hDisplaySize < 1280 ? 0x04 : 0x00;
            break;
    }

    if(bpp != 8) /* DirectColor */
	state->general |= 0x00000030;

    state->repaint0 = (((width / 8) * pixelDepth) & 0x700) >> 3;
    state->pixel    = (pixelDepth > 2) ? 3 : pixelDepth;
}


void NVLoadStateExt (
    ScrnInfoPtr pScrn,
    RIVA_HW_STATE *state
)
{
    NVPtr pNv = NVPTR(pScrn);
    CARD32 temp;

    if(pNv->Architecture >= NV_ARCH_40) {
        switch(pNv->Chipset & 0xfff0) {
        case CHIPSET_NV44:
        case CHIPSET_NV44A:
        case CHIPSET_C51:
        case CHIPSET_G70:
        case CHIPSET_G71:
        case CHIPSET_G72:
        case CHIPSET_G73:
        case CHIPSET_C512:
             temp = nvReadCurRAMDAC(pNv, NV_PRAMDAC_TEST_CONTROL);
             nvWriteCurRAMDAC(pNv, NV_PRAMDAC_TEST_CONTROL, temp | 0x00100000);
             break;
        default:
             break;
        };
    }

    if(pNv->Architecture >= NV_ARCH_10) {
        if(pNv->twoHeads) {
           NVWriteCRTC(pNv, 0, NV_PCRTC_ENGINE_CTRL, state->head);
           NVWriteCRTC(pNv, 1, NV_PCRTC_ENGINE_CTRL, state->head2);
        }
        temp = nvReadCurRAMDAC(pNv, NV_RAMDAC_NV10_CURSYNC);
        nvWriteCurRAMDAC(pNv, NV_RAMDAC_NV10_CURSYNC, temp | (1 << 25));
    
        nvWriteVIDEO(pNv, NV_PVIDEO_STOP, 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_INTR_EN, 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(0), 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(1), 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(0), pNv->VRAMPhysicalSize - 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_LIMIT(1), pNv->VRAMPhysicalSize - 1);
	nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(0), pNv->VRAMPhysicalSize - 1);
        nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_LIMIT(1), pNv->VRAMPhysicalSize - 1);
        nvWriteMC(pNv, NV_PBUS_POWERCTRL_2, 0);

        nvWriteCurCRTC(pNv, NV_PCRTC_CURSOR_CONFIG, state->cursorConfig);
        nvWriteCurCRTC(pNv, NV_PCRTC_830, state->displayV - 3);
        nvWriteCurCRTC(pNv, NV_PCRTC_834, state->displayV - 1);
    
        if(pNv->FlatPanel) {
           if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
               nvWriteCurRAMDAC(pNv, NV_RAMDAC_DITHER_NV11, state->dither);
           } else 
           if(pNv->twoHeads) {
               nvWriteCurRAMDAC(pNv, NV_RAMDAC_FP_DITHER, state->dither);
           }
    
	   nvWriteCurVGA(pNv, NV_CIO_CRE_53, state->timingH);
	   nvWriteCurVGA(pNv, NV_CIO_CRE_54, state->timingV);
	   nvWriteCurVGA(pNv, NV_CIO_CRE_21, 0xfa);
        }

	nvWriteCurVGA(pNv, NV_CIO_CRE_EBR_INDEX, state->extra);
    }

    nvWriteCurVGA(pNv, NV_CIO_CRE_RPC0_INDEX, state->repaint0);
    nvWriteCurVGA(pNv, NV_CIO_CRE_RPC1_INDEX, state->repaint1);
    nvWriteCurVGA(pNv, NV_CIO_CRE_LSR_INDEX, state->screen);
    nvWriteCurVGA(pNv, NV_CIO_CRE_PIXEL_INDEX, state->pixel);
    nvWriteCurVGA(pNv, NV_CIO_CRE_HEB__INDEX, state->horiz);
    nvWriteCurVGA(pNv, NV_CIO_CRE_ENH_INDEX, state->fifo);
    nvWriteCurVGA(pNv, NV_CIO_CRE_FF_INDEX, state->arbitration0);
    nvWriteCurVGA(pNv, NV_CIO_CRE_FFLWM__INDEX, state->arbitration1);
    if(pNv->Architecture >= NV_ARCH_30) {
      nvWriteCurVGA(pNv, NV_CIO_CRE_47, state->arbitration1 >> 8);
    }

    nvWriteCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR0_INDEX, state->cursor0);
    nvWriteCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR1_INDEX, state->cursor1);
    if(pNv->Architecture == NV_ARCH_40) {  /* HW bug */
       volatile CARD32 curpos = nvReadCurRAMDAC(pNv, NV_PRAMDAC_CU_START_POS);
       nvWriteCurRAMDAC(pNv, NV_PRAMDAC_CU_START_POS, curpos);
    }
    nvWriteCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR2_INDEX, state->cursor2);
    nvWriteCurVGA(pNv, NV_CIO_CRE_ILACE__INDEX, state->interlace);

    if(!pNv->FlatPanel) {
       NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_PLL_COEFF_SELECT, state->pllsel);
       NVWriteRAMDAC(pNv, 0, NV_PRAMDAC_VPLL_COEFF, state->vpll);
       if(pNv->twoHeads)
          NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL2, state->vpll2);
       if(pNv->two_reg_pll) {
          NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B, state->vpllB);
          NVWriteRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B, state->vpll2B);
       }
    } else {
       nvWriteCurRAMDAC(pNv, NV_PRAMDAC_FP_TG_CONTROL, state->scale);
       nvWriteCurRAMDAC(pNv, NV_PRAMDAC_FP_HCRTC, state->crtcSync);
    }
    nvWriteCurRAMDAC(pNv, NV_PRAMDAC_GENERAL_CONTROL, state->general);

    nvWriteCurCRTC(pNv, NV_PCRTC_INTR_EN_0, 0);
    nvWriteCurCRTC(pNv, NV_PCRTC_INTR_0, NV_PCRTC_INTR_0_VBLANK);
}

void NVUnloadStateExt
(
    NVPtr pNv,
    RIVA_HW_STATE *state
)
{
    state->repaint0     = nvReadCurVGA(pNv, NV_CIO_CRE_RPC0_INDEX);
    state->repaint1     = nvReadCurVGA(pNv, NV_CIO_CRE_RPC1_INDEX);
    state->screen       = nvReadCurVGA(pNv, NV_CIO_CRE_LSR_INDEX);
    state->pixel        = nvReadCurVGA(pNv, NV_CIO_CRE_PIXEL_INDEX);
    state->horiz        = nvReadCurVGA(pNv, NV_CIO_CRE_HEB__INDEX);
    state->fifo         = nvReadCurVGA(pNv, NV_CIO_CRE_ENH_INDEX);
    state->arbitration0 = nvReadCurVGA(pNv, NV_CIO_CRE_FF_INDEX);
    state->arbitration1 = nvReadCurVGA(pNv, NV_CIO_CRE_FFLWM__INDEX);
    if(pNv->Architecture >= NV_ARCH_30) {
       state->arbitration1 |= (nvReadCurVGA(pNv, NV_CIO_CRE_47) & 1) << 8;
    }
    state->cursor0      = nvReadCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR0_INDEX);
    state->cursor1      = nvReadCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR1_INDEX);
    state->cursor2      = nvReadCurVGA(pNv, NV_CIO_CRE_HCUR_ADDR2_INDEX);
    state->interlace    = nvReadCurVGA(pNv, NV_CIO_CRE_ILACE__INDEX);

    state->vpll         = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_VPLL_COEFF);
    if(pNv->twoHeads)
       state->vpll2     = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2);
    if(pNv->two_reg_pll) {
        state->vpllB    = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B);
        state->vpll2B   = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B);
    }
    state->pllsel       = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_PLL_COEFF_SELECT);
    state->general      = nvReadCurRAMDAC(pNv, NV_PRAMDAC_GENERAL_CONTROL);
    state->scale        = nvReadCurRAMDAC(pNv, NV_PRAMDAC_FP_TG_CONTROL);

    if(pNv->Architecture >= NV_ARCH_10) {
        if(pNv->twoHeads) {
           state->head     = NVReadCRTC(pNv, 0, NV_PCRTC_ENGINE_CTRL);
           state->head2    = NVReadCRTC(pNv, 1, NV_PCRTC_ENGINE_CTRL);
           state->crtcOwner = nvReadCurVGA(pNv, NV_CIO_CRE_44);
        }
        state->extra = nvReadCurVGA(pNv, NV_CIO_CRE_EBR_INDEX);

        state->cursorConfig = nvReadCurCRTC(pNv, NV_PCRTC_CURSOR_CONFIG);

        if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
           state->dither = nvReadCurRAMDAC(pNv, NV_RAMDAC_DITHER_NV11);
        } else 
        if(pNv->twoHeads) {
            state->dither = nvReadCurRAMDAC(pNv, NV_RAMDAC_FP_DITHER);
        }

        if(pNv->FlatPanel) {
           state->timingH = nvReadCurVGA(pNv, NV_CIO_CRE_53);
           state->timingV = nvReadCurVGA(pNv, NV_CIO_CRE_54);
        }
    }

    if(pNv->FlatPanel) {
       state->crtcSync = nvReadCurRAMDAC(pNv, NV_PRAMDAC_FP_HCRTC);
    }
}

void NVSetStartAddress (
    NVPtr   pNv,
    CARD32 start
)
{
    nvWriteCurCRTC(pNv, NV_PCRTC_START, start);
}
