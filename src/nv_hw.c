/*
 * Copyright 1993-2003 NVIDIA, Corporation
 * Copyright 2008 Stuart Bennett
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

void NVWriteVgaSeq(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	NVWritePRMVIO(pNv, head, NV_PRMVIO_SRX, index);
	NVWritePRMVIO(pNv, head, NV_PRMVIO_SR, value);
}

uint8_t NVReadVgaSeq(NVPtr pNv, int head, uint8_t index)
{
	NVWritePRMVIO(pNv, head, NV_PRMVIO_SRX, index);
	return NVReadPRMVIO(pNv, head, NV_PRMVIO_SR);
}

void NVWriteVgaGr(NVPtr pNv, int head, uint8_t index, uint8_t value)
{
	NVWritePRMVIO(pNv, head, NV_PRMVIO_GRX, index);
	NVWritePRMVIO(pNv, head, NV_PRMVIO_GX, value);
}

uint8_t NVReadVgaGr(NVPtr pNv, int head, uint8_t index)
{
	NVWritePRMVIO(pNv, head, NV_PRMVIO_GRX, index);
	return NVReadPRMVIO(pNv, head, NV_PRMVIO_GX);
}

/* CR44 takes values 0 (head A), 3 (head B) and 4 (heads tied)
 * it affects only the 8 bit vga io regs, which we access using mmio at
 * 0xc{0,2}3c*, 0x60{1,3}3*, and 0x68{1,3}3d*
 * in general, the set value of cr44 does not matter: reg access works as
 * expected and values can be set for the appropriate head by using a 0x2000
 * offset as required
 * however:
 * a) pre nv40, the head B range of PRMVIO regs at 0xc23c* was not exposed and
 *    cr44 must be set to 0 or 3 for accessing values on the correct head
 *    through the common 0xc03c* addresses
 * b) in tied mode (4) head B is programmed to the values set on head A, and
 *    access using the head B addresses can have strange results, ergo we leave
 *    tied mode in init once we know to what cr44 should be restored on exit
 *
 * the owner parameter is slightly abused:
 * 0 and 1 are treated as head values and so the set value is (owner * 3)
 * other values are treated as literal values to set
 */
void NVSetOwner(NVPtr pNv, int owner)
{
	if (owner == 1)
		owner *= 3;
	/* CR44 is always changed on CRTC0 */
	NVWriteVgaCrtc(pNv, 0, NV_CIO_CRE_44, owner);
	if (pNv->NVArch == 0x11) {	/* set me harder */
		NVWriteVgaCrtc(pNv, 0, NV_CIO_CRE_2E, owner);
		NVWriteVgaCrtc(pNv, 0, NV_CIO_CRE_2E, owner);
	}
}

void NVBlankScreen(NVPtr pNv, int head, bool blank)
{
	unsigned char seq1;

	if (pNv->twoHeads)
		NVSetOwner(pNv, head);

	seq1 = NVReadVgaSeq(pNv, head, NV_VIO_SR_CLOCK_INDEX);

	NVVgaSeqReset(pNv, head, true);
	if (blank)
		NVWriteVgaSeq(pNv, head, NV_VIO_SR_CLOCK_INDEX, seq1 | 0x20);
	else
		NVWriteVgaSeq(pNv, head, NV_VIO_SR_CLOCK_INDEX, seq1 & ~0x20);
	NVVgaSeqReset(pNv, head, false);
}

static void nouveau_hw_decode_pll(NVPtr pNv, uint32_t reg1,
				  uint32_t pll1, uint32_t pll2,
				  struct nouveau_pll_vals *pllvals)
{
	/* to force parsing as single stage (i.e. nv40 vplls) pass pll2 as 0 */

	/* log2P is & 0x7 as never more than 6, and nv30/35 only uses 3 bits */
	pllvals->log2P = (pll1 >> 16) & 0x7;
	pllvals->N2 = pllvals->M2 = 1;

	if (reg1 <= 0x405c) {
		pllvals->NM1 = pll2 & 0xffff;
		/* single stage NVPLL and VPLLs use 1 << 8, MPLL uses 1 << 12 */
		if (!(pll1 & 0x1100))
			pllvals->NM2 = pll2 >> 16;
	} else {
		pllvals->NM1 = pll1 & 0xffff;
		if (pNv->two_reg_pll && pll2 & NV31_RAMDAC_ENABLE_VCO2)
			pllvals->NM2 = pll2 & 0xffff;
		else if (pNv->NVArch == 0x30 || pNv->NVArch == 0x35) {
			pllvals->M1 &= 0xf; /* only 4 bits */
			if (pll1 & NV30_RAMDAC_ENABLE_VCO2) {
				pllvals->M2 = (pll1 >> 4) & 0x7;
				pllvals->N2 = ((pll1 >> 21) & 0x18) |
					      ((pll1 >> 19) & 0x7);
			}
		}
	}
}

int nouveau_hw_get_pllvals(ScrnInfoPtr pScrn, enum pll_types plltype,
			   struct nouveau_pll_vals *pllvals)
{
	NVPtr pNv = NVPTR(pScrn);
	const uint32_t nv04_regs[MAX_PLL_TYPES] = { NV_PRAMDAC_NVPLL_COEFF,
						    NV_PRAMDAC_MPLL_COEFF,
						    NV_PRAMDAC_VPLL_COEFF,
						    NV_RAMDAC_VPLL2 };
	const uint32_t nv40_regs[MAX_PLL_TYPES] = { 0x4000,
						    0x4020,
						    NV_PRAMDAC_VPLL_COEFF,
						    NV_RAMDAC_VPLL2 };
	uint32_t reg1, pll1, pll2 = 0;
	struct pll_lims pll_lim;
	int ret;

	if (pNv->Architecture < NV_ARCH_40)
		reg1 = nv04_regs[plltype];
	else
		reg1 = nv40_regs[plltype];

	pll1 = nvReadMC(pNv, reg1);

	if (reg1 <= 0x405c)
		pll2 = nvReadMC(pNv, reg1 + 4);
	else if (pNv->two_reg_pll) {
		uint32_t reg2 = reg1 + (reg1 == NV_RAMDAC_VPLL2 ? 0x5c : 0x70);

		pll2 = nvReadMC(pNv, reg2);
	}

	if (pNv->Architecture == 0x40 && reg1 >= NV_PRAMDAC_VPLL_COEFF) {
		uint32_t ramdac580 = NVReadRAMDAC(pNv, 0, NV_PRAMDAC_580);

		/* check whether vpll has been forced into single stage mode */
		if (reg1 == NV_PRAMDAC_VPLL_COEFF) {
			if (ramdac580 & NV_RAMDAC_580_VPLL1_ACTIVE)
				pll2 = 0;
		} else
			if (ramdac580 & NV_RAMDAC_580_VPLL2_ACTIVE)
				pll2 = 0;
	}

	nouveau_hw_decode_pll(pNv, reg1, pll1, pll2, pllvals);

	if ((ret = get_pll_limits(pScrn, plltype, &pll_lim)))
		return ret;

	pllvals->refclk = pll_lim.refclk;

	return 0;
}

int nouveau_hw_pllvals_to_clk(struct nouveau_pll_vals *pv)
{
	/* Avoid divide by zero if called at an inappropriate time */
	if (!pv->M1 || !pv->M2)
		return 0;

	return (pv->N1 * pv->N2 * pv->refclk / (pv->M1 * pv->M2) >> pv->log2P);
}

int nouveau_hw_get_clock(ScrnInfoPtr pScrn, enum pll_types plltype)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pll_vals pllvals;

	if (plltype == MPLL && (pNv->Chipset & 0x0ff0) == CHIPSET_NFORCE) {
		uint32_t mpllP = (PCI_SLOT_READ_LONG(3, 0x6c) >> 8) & 0xf;

		if (!mpllP)
			mpllP = 4;
		return 400000 / mpllP;
	} else if (plltype == MPLL && (pNv->Chipset & 0xff0) == CHIPSET_NFORCE2)
		return PCI_SLOT_READ_LONG(5, 0x4c) / 1000;

	nouveau_hw_get_pllvals(pScrn, plltype, &pllvals);

	return nouveau_hw_pllvals_to_clk(&pllvals);
}

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

void nv_save_restore_vga_fonts(ScrnInfoPtr pScrn, bool save)
{
	NVPtr pNv = NVPTR(pScrn);
	bool graphicsmode;
	uint8_t misc, gr4, gr5, gr6, seq2, seq4;
	int i;

	if (pNv->twoHeads)
		NVSetOwner(pNv, 0);

	NVSetEnablePalette(pNv, 0, true);
	graphicsmode = NVReadVgaAttr(pNv, 0, NV_CIO_AR_MODE_INDEX) & 1;
	NVSetEnablePalette(pNv, 0, false);

	if (graphicsmode)	/* graphics mode => framebuffer => no need to save */
		return;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%sing VGA fonts\n", save ? "Sav" : "Restor");
	if (pNv->twoHeads)
		NVBlankScreen(pNv, 1, true);
	NVBlankScreen(pNv, 0, true);

	/* save control regs */
	misc = NVReadPRMVIO(pNv, 0, NV_PRMVIO_MISC__READ);
	seq2 = NVReadVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX);
	seq4 = NVReadVgaSeq(pNv, 0, NV_VIO_SR_MEM_MODE_INDEX);
	gr4 = NVReadVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX);
	gr5 = NVReadVgaGr(pNv, 0, NV_VIO_GX_MODE_INDEX);
	gr6 = NVReadVgaGr(pNv, 0, NV_VIO_GX_MISC_INDEX);

	NVWritePRMVIO(pNv, 0, NV_PRMVIO_MISC__WRITE, 0x67);
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_MEM_MODE_INDEX, 0x6);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_MODE_INDEX, 0x0);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_MISC_INDEX, 0x5);

	/* store font in plane 0 */
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX, 0x1);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX, 0x0);
	for (i = 0; i < 16384; i++)
		if (save)
			pNv->saved_vga_font[0][i] = MMIO_IN32(pNv->FB_BAR, i * 4);
		else
			MMIO_OUT32(pNv->FB_BAR, i * 4, pNv->saved_vga_font[0][i]);

	/* store font in plane 1 */
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX, 0x2);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX, 0x1);
	for (i = 0; i < 16384; i++)
		if (save)
			pNv->saved_vga_font[1][i] = MMIO_IN32(pNv->FB_BAR, i * 4);
		else
			MMIO_OUT32(pNv->FB_BAR, i * 4, pNv->saved_vga_font[1][i]);

	/* store font in plane 2 */
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX, 0x4);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX, 0x2);
	for (i = 0; i < 16384; i++)
		if (save)
			pNv->saved_vga_font[2][i] = MMIO_IN32(pNv->FB_BAR, i * 4);
		else
			MMIO_OUT32(pNv->FB_BAR, i * 4, pNv->saved_vga_font[2][i]);

	/* store font in plane 3 */
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX, 0x8);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX, 0x3);
	for (i = 0; i < 16384; i++)
		if (save)
			pNv->saved_vga_font[3][i] = MMIO_IN32(pNv->FB_BAR, i * 4);
		else
			MMIO_OUT32(pNv->FB_BAR, i * 4, pNv->saved_vga_font[3][i]);

	/* restore control regs */
	NVWritePRMVIO(pNv, 0, NV_PRMVIO_MISC__WRITE, misc);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_READ_MAP_INDEX, gr4);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_MODE_INDEX, gr5);
	NVWriteVgaGr(pNv, 0, NV_VIO_GX_MISC_INDEX, gr6);
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_PLANE_MASK_INDEX, seq2);
	NVWriteVgaSeq(pNv, 0, NV_VIO_SR_MEM_MODE_INDEX, seq4);

	if (pNv->twoHeads)
		NVBlankScreen(pNv, 1, false);
	NVBlankScreen(pNv, 0, false);
}
