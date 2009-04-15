/*
 * Copyright 2009 Advanced Micro Devices, Inc.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) OR
 * AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Alex Deucher <alexander.deucher@amd.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

				/* Driver data structures */
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_atombios.h"

static int calc_div(int num, int den)
{
    int div = (num + (den / 2)) / den;

    if ((div < 2) || (div > 0xff))
	return 0;
    else
	return div;

}

/* 10 khz */
static void
RADEONSetEngineClock(ScrnInfoPtr pScrn, int eng_clock)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONPLLPtr pll = &info->pll;
    uint32_t ref_div, fb_div;
    uint32_t m_spll_ref_fb_div;

    RADEONWaitForIdleMMIO(pScrn);

    m_spll_ref_fb_div = INPLL(pScrn, RADEON_M_SPLL_REF_FB_DIV);
    ref_div = m_spll_ref_fb_div & RADEON_M_SPLL_REF_DIV_MASK;
    fb_div = calc_div(eng_clock * ref_div, pll->reference_freq);

    if (!fb_div)
	return;

    m_spll_ref_fb_div &= ~(RADEON_SPLL_FB_DIV_MASK << RADEON_SPLL_FB_DIV_SHIFT);
    m_spll_ref_fb_div |= (fb_div & RADEON_SPLL_FB_DIV_MASK) << RADEON_SPLL_FB_DIV_SHIFT;
    OUTPLL(pScrn, RADEON_M_SPLL_REF_FB_DIV, m_spll_ref_fb_div);
    usleep(16000); /* Let the pll settle */
}

/* 10 khz */
static void
RADEONSetMemoryClock(ScrnInfoPtr pScrn, int mem_clock)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONPLLPtr pll = &info->pll;
    uint32_t ref_div, fb_div;
    uint32_t m_spll_ref_fb_div;

    if (info->IsIGP)
	return;

    RADEONWaitForIdleMMIO(pScrn);

    m_spll_ref_fb_div = INPLL(pScrn, RADEON_M_SPLL_REF_FB_DIV);
    ref_div = m_spll_ref_fb_div & RADEON_M_SPLL_REF_DIV_MASK;
    fb_div = calc_div(mem_clock * ref_div, pll->reference_freq);

    if (!fb_div)
	return;

    m_spll_ref_fb_div &= ~(RADEON_MPLL_FB_DIV_MASK << RADEON_MPLL_FB_DIV_SHIFT);
    m_spll_ref_fb_div |= (fb_div & RADEON_MPLL_FB_DIV_MASK) << RADEON_MPLL_FB_DIV_SHIFT;
    OUTPLL(pScrn, RADEON_M_SPLL_REF_FB_DIV, m_spll_ref_fb_div);
    usleep(16000); /* Let the pll settle */
}

static void LegacySetClockGating(ScrnInfoPtr pScrn, Bool enable)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    uint32_t tmp;

    if (enable) {
	if (!pRADEONEnt->HasCRTC2) {
	    tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
	    if ((INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) >
		RADEON_CFG_ATI_REV_A13) {
		tmp &= ~(RADEON_SCLK_FORCE_CP | RADEON_SCLK_FORCE_RB);
	    }
	    tmp &= ~(RADEON_SCLK_FORCE_HDP  | RADEON_SCLK_FORCE_DISP1 |
		     RADEON_SCLK_FORCE_TOP  | RADEON_SCLK_FORCE_SE   |
		     RADEON_SCLK_FORCE_IDCT | RADEON_SCLK_FORCE_RE   |
		     RADEON_SCLK_FORCE_PB   | RADEON_SCLK_FORCE_TAM  |
		     RADEON_SCLK_FORCE_TDM);
	    OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);
	} else if (IS_R300_VARIANT) {
	    if (info->ChipFamily >= CHIP_FAMILY_RV350) {
		tmp = INPLL(pScrn, R300_SCLK_CNTL2);
		tmp &= ~(R300_SCLK_FORCE_TCL |
			 R300_SCLK_FORCE_GA  |
			 R300_SCLK_FORCE_CBA);
		tmp |=  (R300_SCLK_TCL_MAX_DYN_STOP_LAT |
			 R300_SCLK_GA_MAX_DYN_STOP_LAT  |
			 R300_SCLK_CBA_MAX_DYN_STOP_LAT);
		OUTPLL(pScrn, R300_SCLK_CNTL2, tmp);

		tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
		tmp &= ~(RADEON_SCLK_FORCE_DISP2 | RADEON_SCLK_FORCE_CP      |
			 RADEON_SCLK_FORCE_HDP   | RADEON_SCLK_FORCE_DISP1   |
			 RADEON_SCLK_FORCE_TOP   | RADEON_SCLK_FORCE_E2      |
			 R300_SCLK_FORCE_VAP     | RADEON_SCLK_FORCE_IDCT    |
			 RADEON_SCLK_FORCE_VIP   | R300_SCLK_FORCE_SR        |
			 R300_SCLK_FORCE_PX      | R300_SCLK_FORCE_TX        |
			 R300_SCLK_FORCE_US      | RADEON_SCLK_FORCE_TV_SCLK |
			 R300_SCLK_FORCE_SU      | RADEON_SCLK_FORCE_OV0);
		tmp |=  RADEON_DYN_STOP_LAT_MASK;
		OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);

		tmp = INPLL(pScrn, RADEON_SCLK_MORE_CNTL);
		tmp &= ~RADEON_SCLK_MORE_FORCEON;
		tmp |=  RADEON_SCLK_MORE_MAX_DYN_STOP_LAT;
		OUTPLL(pScrn, RADEON_SCLK_MORE_CNTL, tmp);

		tmp = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);
		tmp |= (RADEON_PIXCLK_ALWAYS_ONb |
			RADEON_PIXCLK_DAC_ALWAYS_ONb);
		OUTPLL(pScrn, RADEON_VCLK_ECP_CNTL, tmp);

		tmp = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
		tmp |= (RADEON_PIX2CLK_ALWAYS_ONb         |
			RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
			RADEON_DISP_TVOUT_PIXCLK_TV_ALWAYS_ONb |
			R300_DVOCLK_ALWAYS_ONb            |
			RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
			RADEON_PIXCLK_GV_ALWAYS_ONb       |
			R300_PIXCLK_DVO_ALWAYS_ONb        |
			RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
			RADEON_PIXCLK_TMDS_ALWAYS_ONb     |
			R300_PIXCLK_TRANS_ALWAYS_ONb      |
			R300_PIXCLK_TVO_ALWAYS_ONb        |
			R300_P2G2CLK_ALWAYS_ONb           |
			R300_P2G2CLK_ALWAYS_ONb);
		OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmp);

		tmp = INPLL(pScrn, RADEON_MCLK_MISC);
		tmp |= (RADEON_MC_MCLK_DYN_ENABLE |
			RADEON_IO_MCLK_DYN_ENABLE);
		OUTPLL(pScrn, RADEON_MCLK_MISC, tmp);

		tmp = INPLL(pScrn, RADEON_MCLK_CNTL);
		tmp |= (RADEON_FORCEON_MCLKA |
			RADEON_FORCEON_MCLKB);

		tmp &= ~(RADEON_FORCEON_YCLKA  |
			 RADEON_FORCEON_YCLKB  |
			 RADEON_FORCEON_MC);

		/* Some releases of vbios have set DISABLE_MC_MCLKA
		   and DISABLE_MC_MCLKB bits in the vbios table.  Setting these
		   bits will cause H/W hang when reading video memory with dynamic clocking
		   enabled. */
		if ((tmp & R300_DISABLE_MC_MCLKA) &&
		    (tmp & R300_DISABLE_MC_MCLKB)) {
		    /* If both bits are set, then check the active channels */
		    tmp = INPLL(pScrn, RADEON_MCLK_CNTL);
		    if (info->RamWidth == 64) {
			if (INREG(RADEON_MEM_CNTL) & R300_MEM_USE_CD_CH_ONLY)
			    tmp &= ~R300_DISABLE_MC_MCLKB;
			else
			    tmp &= ~R300_DISABLE_MC_MCLKA;
		    } else {
			tmp &= ~(R300_DISABLE_MC_MCLKA |
				 R300_DISABLE_MC_MCLKB);
		    }
		}

		OUTPLL(pScrn, RADEON_MCLK_CNTL, tmp);
	    } else {
		tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
		tmp &= ~(R300_SCLK_FORCE_VAP);
		tmp |= RADEON_SCLK_FORCE_CP;
		OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);
		usleep(15000);

		tmp = INPLL(pScrn, R300_SCLK_CNTL2);
		tmp &= ~(R300_SCLK_FORCE_TCL |
			 R300_SCLK_FORCE_GA  |
			 R300_SCLK_FORCE_CBA);
		OUTPLL(pScrn, R300_SCLK_CNTL2, tmp);
	    }
	} else {
	    tmp = INPLL(pScrn, RADEON_CLK_PWRMGT_CNTL);

	    tmp &= ~(RADEON_ACTIVE_HILO_LAT_MASK     |
		     RADEON_DISP_DYN_STOP_LAT_MASK   |
		     RADEON_DYN_STOP_MODE_MASK);

	    tmp |= (RADEON_ENGIN_DYNCLK_MODE |
		    (0x01 << RADEON_ACTIVE_HILO_LAT_SHIFT));
	    OUTPLL(pScrn, RADEON_CLK_PWRMGT_CNTL, tmp);
	    usleep(15000);

	    tmp = INPLL(pScrn, RADEON_CLK_PIN_CNTL);
	    tmp |= RADEON_SCLK_DYN_START_CNTL;
	    OUTPLL(pScrn, RADEON_CLK_PIN_CNTL, tmp);
	    usleep(15000);

	    /* When DRI is enabled, setting DYN_STOP_LAT to zero can cause some R200
	       to lockup randomly, leave them as set by BIOS.
	    */
	    tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
	    /*tmp &= RADEON_SCLK_SRC_SEL_MASK;*/
	    tmp &= ~RADEON_SCLK_FORCEON_MASK;

	    /*RAGE_6::A11 A12 A12N1 A13, RV250::A11 A12, R300*/
	    if (((info->ChipFamily == CHIP_FAMILY_RV250) &&
		 ((INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <
		  RADEON_CFG_ATI_REV_A13)) ||
		((info->ChipFamily == CHIP_FAMILY_RV100) &&
		 ((INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <=
		  RADEON_CFG_ATI_REV_A13))) {
		tmp |= RADEON_SCLK_FORCE_CP;
		tmp |= RADEON_SCLK_FORCE_VIP;
	    }

	    OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);

	    if ((info->ChipFamily == CHIP_FAMILY_RV200) ||
		(info->ChipFamily == CHIP_FAMILY_RV250) ||
		(info->ChipFamily == CHIP_FAMILY_RV280)) {
		tmp = INPLL(pScrn, RADEON_SCLK_MORE_CNTL);
		tmp &= ~RADEON_SCLK_MORE_FORCEON;

		/* RV200::A11 A12 RV250::A11 A12 */
		if (((info->ChipFamily == CHIP_FAMILY_RV200) ||
		     (info->ChipFamily == CHIP_FAMILY_RV250)) &&
		    ((INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <
		     RADEON_CFG_ATI_REV_A13)) {
		    tmp |= RADEON_SCLK_MORE_FORCEON;
		}
		OUTPLL(pScrn, RADEON_SCLK_MORE_CNTL, tmp);
		usleep(15000);
	    }

	    /* RV200::A11 A12, RV250::A11 A12 */
	    if (((info->ChipFamily == CHIP_FAMILY_RV200) ||
		 (info->ChipFamily == CHIP_FAMILY_RV250)) &&
		((INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <
		 RADEON_CFG_ATI_REV_A13)) {
		tmp = INPLL(pScrn, RADEON_PLL_PWRMGT_CNTL);
		tmp |= RADEON_TCL_BYPASS_DISABLE;
		OUTPLL(pScrn, RADEON_PLL_PWRMGT_CNTL, tmp);
	    }
	    usleep(15000);

	    /*enable dynamic mode for display clocks (PIXCLK and PIX2CLK)*/
	    tmp = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
	    tmp |=  (RADEON_PIX2CLK_ALWAYS_ONb         |
		     RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
		     RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
		     RADEON_PIXCLK_GV_ALWAYS_ONb       |
		     RADEON_PIXCLK_DIG_TMDS_ALWAYS_ONb |
		     RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
		     RADEON_PIXCLK_TMDS_ALWAYS_ONb);

	    OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmp);
	    usleep(15000);

	    tmp = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);
	    tmp |= (RADEON_PIXCLK_ALWAYS_ONb  |
		    RADEON_PIXCLK_DAC_ALWAYS_ONb);

	    OUTPLL(pScrn, RADEON_VCLK_ECP_CNTL, tmp);
	    usleep(15000);
	}
    } else {
	/* Turn everything OFF (ForceON to everything)*/
	if ( !pRADEONEnt->HasCRTC2 ) {
	    tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
	    tmp |= (RADEON_SCLK_FORCE_CP   | RADEON_SCLK_FORCE_HDP |
		    RADEON_SCLK_FORCE_DISP1 | RADEON_SCLK_FORCE_TOP |
		    RADEON_SCLK_FORCE_E2   | RADEON_SCLK_FORCE_SE  |
		    RADEON_SCLK_FORCE_IDCT | RADEON_SCLK_FORCE_VIP |
		    RADEON_SCLK_FORCE_RE   | RADEON_SCLK_FORCE_PB  |
		    RADEON_SCLK_FORCE_TAM  | RADEON_SCLK_FORCE_TDM |
		    RADEON_SCLK_FORCE_RB);
	    OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);
	} else if (info->ChipFamily >= CHIP_FAMILY_RV350) {
	    /* for RV350/M10, no delays are required. */
	    tmp = INPLL(pScrn, R300_SCLK_CNTL2);
	    tmp |= (R300_SCLK_FORCE_TCL |
		    R300_SCLK_FORCE_GA  |
		    R300_SCLK_FORCE_CBA);
	    OUTPLL(pScrn, R300_SCLK_CNTL2, tmp);

	    tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
	    tmp |= (RADEON_SCLK_FORCE_DISP2 | RADEON_SCLK_FORCE_CP      |
		    RADEON_SCLK_FORCE_HDP   | RADEON_SCLK_FORCE_DISP1   |
		    RADEON_SCLK_FORCE_TOP   | RADEON_SCLK_FORCE_E2      |
		    R300_SCLK_FORCE_VAP     | RADEON_SCLK_FORCE_IDCT    |
		    RADEON_SCLK_FORCE_VIP   | R300_SCLK_FORCE_SR        |
		    R300_SCLK_FORCE_PX      | R300_SCLK_FORCE_TX        |
		    R300_SCLK_FORCE_US      | RADEON_SCLK_FORCE_TV_SCLK |
		    R300_SCLK_FORCE_SU      | RADEON_SCLK_FORCE_OV0);
	    OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);

	    tmp = INPLL(pScrn, RADEON_SCLK_MORE_CNTL);
	    tmp |= RADEON_SCLK_MORE_FORCEON;
	    OUTPLL(pScrn, RADEON_SCLK_MORE_CNTL, tmp);

	    tmp = INPLL(pScrn, RADEON_MCLK_CNTL);
	    tmp |= (RADEON_FORCEON_MCLKA |
		    RADEON_FORCEON_MCLKB |
		    RADEON_FORCEON_YCLKA |
		    RADEON_FORCEON_YCLKB |
		    RADEON_FORCEON_MC);
	    OUTPLL(pScrn, RADEON_MCLK_CNTL, tmp);

	    tmp = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);
	    tmp &= ~(RADEON_PIXCLK_ALWAYS_ONb  |
		     RADEON_PIXCLK_DAC_ALWAYS_ONb |
		     R300_DISP_DAC_PIXCLK_DAC_BLANK_OFF);
	    OUTPLL(pScrn, RADEON_VCLK_ECP_CNTL, tmp);

	    tmp = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
	    tmp &= ~(RADEON_PIX2CLK_ALWAYS_ONb         |
		     RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
		     RADEON_DISP_TVOUT_PIXCLK_TV_ALWAYS_ONb |
		     R300_DVOCLK_ALWAYS_ONb            |
		     RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
		     RADEON_PIXCLK_GV_ALWAYS_ONb       |
		     R300_PIXCLK_DVO_ALWAYS_ONb        |
		     RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
		     RADEON_PIXCLK_TMDS_ALWAYS_ONb     |
		     R300_PIXCLK_TRANS_ALWAYS_ONb      |
		     R300_PIXCLK_TVO_ALWAYS_ONb        |
		     R300_P2G2CLK_ALWAYS_ONb            |
		     R300_P2G2CLK_ALWAYS_ONb           |
		     R300_DISP_DAC_PIXCLK_DAC2_BLANK_OFF);
	    OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmp);
	}  else {
	    tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
	    tmp |= (RADEON_SCLK_FORCE_CP | RADEON_SCLK_FORCE_E2);
	    tmp |= RADEON_SCLK_FORCE_SE;

	    if ( !pRADEONEnt->HasCRTC2 ) {
		tmp |= ( RADEON_SCLK_FORCE_RB    |
			 RADEON_SCLK_FORCE_TDM   |
			 RADEON_SCLK_FORCE_TAM   |
			 RADEON_SCLK_FORCE_PB    |
			 RADEON_SCLK_FORCE_RE    |
			 RADEON_SCLK_FORCE_VIP   |
			 RADEON_SCLK_FORCE_IDCT  |
			 RADEON_SCLK_FORCE_TOP   |
			 RADEON_SCLK_FORCE_DISP1 |
			 RADEON_SCLK_FORCE_DISP2 |
			 RADEON_SCLK_FORCE_HDP    );
	    } else if ((info->ChipFamily == CHIP_FAMILY_R300) ||
		       (info->ChipFamily == CHIP_FAMILY_R350)) {
		tmp |= ( RADEON_SCLK_FORCE_HDP   |
			 RADEON_SCLK_FORCE_DISP1 |
			 RADEON_SCLK_FORCE_DISP2 |
			 RADEON_SCLK_FORCE_TOP   |
			 RADEON_SCLK_FORCE_IDCT  |
			 RADEON_SCLK_FORCE_VIP);
	    }
	    OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);

	    usleep(16000);

	    if ((info->ChipFamily == CHIP_FAMILY_R300) ||
		(info->ChipFamily == CHIP_FAMILY_R350)) {
		tmp = INPLL(pScrn, R300_SCLK_CNTL2);
		tmp |= ( R300_SCLK_FORCE_TCL |
			 R300_SCLK_FORCE_GA  |
			 R300_SCLK_FORCE_CBA);
		OUTPLL(pScrn, R300_SCLK_CNTL2, tmp);
		usleep(16000);
	    }

	    if (info->IsIGP) {
		tmp = INPLL(pScrn, RADEON_MCLK_CNTL);
		tmp &= ~(RADEON_FORCEON_MCLKA |
			 RADEON_FORCEON_YCLKA);
		OUTPLL(pScrn, RADEON_MCLK_CNTL, tmp);
		usleep(16000);
	    }

	    if ((info->ChipFamily == CHIP_FAMILY_RV200) ||
		(info->ChipFamily == CHIP_FAMILY_RV250) ||
		(info->ChipFamily == CHIP_FAMILY_RV280)) {
		tmp = INPLL(pScrn, RADEON_SCLK_MORE_CNTL);
		tmp |= RADEON_SCLK_MORE_FORCEON;
		OUTPLL(pScrn, RADEON_SCLK_MORE_CNTL, tmp);
		usleep(16000);
	    }

	    tmp = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
	    tmp &= ~(RADEON_PIX2CLK_ALWAYS_ONb         |
		     RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
		     RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
		     RADEON_PIXCLK_GV_ALWAYS_ONb       |
		     RADEON_PIXCLK_DIG_TMDS_ALWAYS_ONb |
		     RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
		     RADEON_PIXCLK_TMDS_ALWAYS_ONb);

	    OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmp);
	    usleep(16000);

	    tmp = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);
	    tmp &= ~(RADEON_PIXCLK_ALWAYS_ONb  |
		     RADEON_PIXCLK_DAC_ALWAYS_ONb);
	    OUTPLL(pScrn, RADEON_VCLK_ECP_CNTL, tmp);
	}
    }
}

void RADEONForceSomeClocks(ScrnInfoPtr pScrn)
{
    /* It appears from r300 and rv100 may need some clocks forced-on */
     uint32_t tmp;

     tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
     tmp |= RADEON_SCLK_FORCE_CP | RADEON_SCLK_FORCE_VIP;
     OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);
}

void
RADEONSetClockGating(ScrnInfoPtr pScrn, Bool enable)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

    RADEONWaitForIdleMMIO(pScrn);

    if (info->ChipFamily >= CHIP_FAMILY_R600)
	atombios_static_pwrmgt_setup(pScrn, enable);
    else {
	if (info->IsAtomBios) {
	    atombios_static_pwrmgt_setup(pScrn, enable);
	    atombios_clk_gating_setup(pScrn, enable);
	} else if (info->IsMobility)
	    LegacySetClockGating(pScrn, enable);

	if (IS_R300_VARIANT || IS_RV100_VARIANT)
	    RADEONForceSomeClocks(pScrn);
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Dynamic Clock Gating %sabled\n",
	       enable ? "En" : "Dis");
}

