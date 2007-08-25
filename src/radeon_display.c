/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
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
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>

/* X and server generic header files */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "vgaHW.h"
#include "xf86Modes.h"

/* Driver data structures */
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_version.h"

extern int getRADEONEntityIndex(void);

static const CARD32 default_tvdac_adj [CHIP_FAMILY_LAST] =
{
    0x00000000,   /* unknown */
    0x00000000,   /* legacy */
    0x00000000,   /* r100 */
    0x00280000,   /* rv100 */
    0x00000000,   /* rs100 */
    0x00880000,   /* rv200 */
    0x00000000,   /* rs200 */
    0x00000000,   /* r200 */
    0x00770000,   /* rv250 */
    0x00290000,   /* rs300 */
    0x00560000,   /* rv280 */
    0x00780000,   /* r300 */
    0x00770000,   /* r350 */
    0x00780000,   /* rv350 */
    0x00780000,   /* rv380 */
    0x01080000,   /* r420 */
    0x01080000,   /* rv410 */ /* FIXME: just values from r420 used... */
    0x00780000,   /* rs400 */ /* FIXME: just values from rv380 used... */
};

void RADEONSetSyncRangeFromEdid(ScrnInfoPtr pScrn, int flag)
{
    MonPtr      mon = pScrn->monitor;
    xf86MonPtr  ddc = mon->DDC;
    int         i;

    if (flag) { /* HSync */
	for (i = 0; i < 4; i++) {
	    if (ddc->det_mon[i].type == DS_RANGES) {
		mon->nHsync = 1;
		mon->hsync[0].lo = ddc->det_mon[i].section.ranges.min_h;
		mon->hsync[0].hi = ddc->det_mon[i].section.ranges.max_h;
		return;
	    }
	}
	/* If no sync ranges detected in detailed timing table, let's
	 * try to derive them from supported VESA modes.  Are we doing
	 * too much here!!!?  */
	i = 0;
	if (ddc->timings1.t1 & 0x02) { /* 800x600@56 */
	    mon->hsync[i].lo = mon->hsync[i].hi = 35.2;
	    i++;
	}
	if (ddc->timings1.t1 & 0x04) { /* 640x480@75 */
	    mon->hsync[i].lo = mon->hsync[i].hi = 37.5;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x08) || (ddc->timings1.t1 & 0x01)) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 37.9;
	    i++;
	}
	if (ddc->timings1.t2 & 0x40) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 46.9;
	    i++;
	}
	if ((ddc->timings1.t2 & 0x80) || (ddc->timings1.t2 & 0x08)) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 48.1;
	    i++;
	}
	if (ddc->timings1.t2 & 0x04) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 56.5;
	    i++;
	}
	if (ddc->timings1.t2 & 0x02) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 60.0;
	    i++;
	}
	if (ddc->timings1.t2 & 0x01) {
	    mon->hsync[i].lo = mon->hsync[i].hi = 64.0;
	    i++;
	}
	mon->nHsync = i;
    } else {  /* Vrefresh */
	for (i = 0; i < 4; i++) {
	    if (ddc->det_mon[i].type == DS_RANGES) {
		mon->nVrefresh = 1;
		mon->vrefresh[0].lo = ddc->det_mon[i].section.ranges.min_v;
		mon->vrefresh[0].hi = ddc->det_mon[i].section.ranges.max_v;
		return;
	    }
	}

	i = 0;
	if (ddc->timings1.t1 & 0x02) { /* 800x600@56 */
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 56;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x01) || (ddc->timings1.t2 & 0x08)) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 60;
	    i++;
	}
	if (ddc->timings1.t2 & 0x04) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 70;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x08) || (ddc->timings1.t2 & 0x80)) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 72;
	    i++;
	}
	if ((ddc->timings1.t1 & 0x04) || (ddc->timings1.t2 & 0x40) ||
	    (ddc->timings1.t2 & 0x02) || (ddc->timings1.t2 & 0x01)) {
	    mon->vrefresh[i].lo = mon->vrefresh[i].hi = 75;
	    i++;
	}
	mon->nVrefresh = i;
    }
}

void RADEONGetTVDacAdjInfo(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    
    /* Todo: get this setting from BIOS */
    radeon_output->tv_dac_adj = default_tvdac_adj[info->ChipFamily];
    if (info->IsMobility) { /* some mobility chips may different */
	if (info->ChipFamily == CHIP_FAMILY_RV250)
	    radeon_output->tv_dac_adj = 0x00880000;
    }
}

/*
 * Powering done DAC, needed for DPMS problem with ViewSonic P817 (or its variant).
 *
 */
static void RADEONDacPowerSet(ScrnInfoPtr pScrn, Bool IsOn, Bool IsPrimaryDAC)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    if (IsPrimaryDAC) {
	CARD32 dac_cntl;
	CARD32 dac_macro_cntl = 0;
	dac_cntl = INREG(RADEON_DAC_CNTL);
	dac_macro_cntl = INREG(RADEON_DAC_MACRO_CNTL);
	if (IsOn) {
	    dac_cntl &= ~RADEON_DAC_PDWN;
	    dac_macro_cntl &= ~(RADEON_DAC_PDWN_R |
				RADEON_DAC_PDWN_G |
				RADEON_DAC_PDWN_B);
	} else {
	    dac_cntl |= RADEON_DAC_PDWN;
	    dac_macro_cntl |= (RADEON_DAC_PDWN_R |
			       RADEON_DAC_PDWN_G |
			       RADEON_DAC_PDWN_B);
	}
	OUTREG(RADEON_DAC_CNTL, dac_cntl);
	OUTREG(RADEON_DAC_MACRO_CNTL, dac_macro_cntl);
    } else {
	CARD32 tv_dac_cntl;
	CARD32 fp2_gen_cntl;
	
	switch(info->ChipFamily)
	{
	case CHIP_FAMILY_R420:
	case CHIP_FAMILY_RV410:
	    tv_dac_cntl = INREG(RADEON_TV_DAC_CNTL);
	    if (IsOn) {
		tv_dac_cntl &= ~(R420_TV_DAC_RDACPD |
				 R420_TV_DAC_GDACPD |
				 R420_TV_DAC_BDACPD |
				 RADEON_TV_DAC_BGSLEEP);
	    } else {
		tv_dac_cntl |= (R420_TV_DAC_RDACPD |
				R420_TV_DAC_GDACPD |
				R420_TV_DAC_BDACPD |
				RADEON_TV_DAC_BGSLEEP);
	    }
	    OUTREG(RADEON_TV_DAC_CNTL, tv_dac_cntl);
	    break;
	case CHIP_FAMILY_R200:
	    fp2_gen_cntl = INREG(RADEON_FP2_GEN_CNTL);
	    if (IsOn) {
		fp2_gen_cntl |= RADEON_FP2_DVO_EN;
	    } else {
		fp2_gen_cntl &= ~RADEON_FP2_DVO_EN;
	    }
	    OUTREG(RADEON_FP2_GEN_CNTL, fp2_gen_cntl);
	    break;

	default:
	    tv_dac_cntl = INREG(RADEON_TV_DAC_CNTL);
	    if (IsOn) {
		tv_dac_cntl &= ~(RADEON_TV_DAC_RDACPD |
				 RADEON_TV_DAC_GDACPD |
				 RADEON_TV_DAC_BDACPD |
				 RADEON_TV_DAC_BGSLEEP);
	    } else {
		tv_dac_cntl |= (RADEON_TV_DAC_RDACPD |
				RADEON_TV_DAC_GDACPD |
				RADEON_TV_DAC_BDACPD |
				RADEON_TV_DAC_BGSLEEP);
	    }
	    OUTREG(RADEON_TV_DAC_CNTL, tv_dac_cntl);
	    break;
	}
    }
}

/* disable all ouputs before enabling the ones we want */
void RADEONDisableDisplays(ScrnInfoPtr pScrn) {
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char * RADEONMMIO = info->MMIO;
    unsigned long tmp, tmpPixclksCntl;


    /* primary DAC */
    tmp = INREG(RADEON_CRTC_EXT_CNTL);
    tmp &= ~RADEON_CRTC_CRT_ON;                    
    OUTREG(RADEON_CRTC_EXT_CNTL, tmp);
    RADEONDacPowerSet(pScrn, FALSE, TRUE);

    /* Secondary DAC */
    if (info->ChipFamily == CHIP_FAMILY_R200) {
        tmp = INREG(RADEON_FP2_GEN_CNTL);
        tmp &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
        OUTREG(RADEON_FP2_GEN_CNTL, tmp);
    } else {
        tmp = INREG(RADEON_CRTC2_GEN_CNTL);
        tmp &= ~RADEON_CRTC2_CRT2_ON;  
        OUTREG(RADEON_CRTC2_GEN_CNTL, tmp);
    }
    RADEONDacPowerSet(pScrn, FALSE, FALSE);

    /* turn off tv-out */
    if (info->InternalTVOut) {
	tmp = INREG(RADEON_TV_MASTER_CNTL);
	tmp &= ~RADEON_TV_ON;
	OUTREG(RADEON_TV_MASTER_CNTL, tmp);
    }

    /* FP 1 */
    tmp = INREG(RADEON_FP_GEN_CNTL);
    tmp &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
    OUTREG(RADEON_FP_GEN_CNTL, tmp);

    /* FP 2 */
    tmp = INREG(RADEON_FP2_GEN_CNTL);
    tmp &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
    OUTREG(RADEON_FP2_GEN_CNTL, tmp);

    /* LVDS */
    if (info->IsMobility) {
	tmpPixclksCntl = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
	if (info->IsMobility || info->IsIGP) {
	    /* Asic bug, when turning off LVDS_ON, we have to make sure
	       RADEON_PIXCLK_LVDS_ALWAYS_ON bit is off
	    */
	    OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
	}
	tmp = INREG(RADEON_LVDS_GEN_CNTL);
	tmp |= RADEON_LVDS_DISPLAY_DIS;
	tmp &= ~(RADEON_LVDS_ON | RADEON_LVDS_BLON);
	OUTREG(RADEON_LVDS_GEN_CNTL, tmp);
	if (info->IsMobility || info->IsIGP) {
	    OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmpPixclksCntl);
	}
    }

}

/* This is to be used enable/disable displays dynamically */
void RADEONEnableDisplay(xf86OutputPtr output, BOOL bEnable)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONSavePtr save = &info->ModeReg;
    unsigned char * RADEONMMIO = info->MMIO;
    unsigned long tmp;
    RADEONOutputPrivatePtr radeon_output;
    int tv_dac_change = 0;
    radeon_output = output->driver_private;

    if (bEnable) {
	ErrorF("enable montype: %d\n", radeon_output->MonType);
        if (radeon_output->MonType == MT_CRT) {
            if (radeon_output->DACType == DAC_PRIMARY) {
                tmp = INREG(RADEON_CRTC_EXT_CNTL);
                tmp |= RADEON_CRTC_CRT_ON;                    
                OUTREG(RADEON_CRTC_EXT_CNTL, tmp);
                save->crtc_ext_cntl |= RADEON_CRTC_CRT_ON;
                RADEONDacPowerSet(pScrn, bEnable, (radeon_output->DACType == DAC_PRIMARY));
            } else if (radeon_output->DACType == DAC_TVDAC) {
                if (info->ChipFamily == CHIP_FAMILY_R200) {
                    tmp = INREG(RADEON_FP2_GEN_CNTL);
                    tmp |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                    OUTREG(RADEON_FP2_GEN_CNTL, tmp);
                    save->fp2_gen_cntl |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                } else {
                    tmp = INREG(RADEON_CRTC2_GEN_CNTL);
                    tmp |= RADEON_CRTC2_CRT2_ON;
                    OUTREG(RADEON_CRTC2_GEN_CNTL, tmp);
                    save->crtc2_gen_cntl |= RADEON_CRTC2_CRT2_ON;
                }
                tv_dac_change = 1;
            }
        } else if (radeon_output->MonType == MT_DFP) {
            if (radeon_output->TMDSType == TMDS_INT) {
                tmp = INREG(RADEON_FP_GEN_CNTL);
                tmp |= (RADEON_FP_FPON | RADEON_FP_TMDS_EN);
                OUTREG(RADEON_FP_GEN_CNTL, tmp);
                save->fp_gen_cntl |= (RADEON_FP_FPON | RADEON_FP_TMDS_EN);
            } else if (radeon_output->TMDSType == TMDS_EXT) {
                tmp = INREG(RADEON_FP2_GEN_CNTL);
                tmp |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                OUTREG(RADEON_FP2_GEN_CNTL, tmp);
                save->fp2_gen_cntl |= (RADEON_FP2_ON | RADEON_FP2_DVO_EN);
            }
        } else if (radeon_output->MonType == MT_LCD) {
            tmp = INREG(RADEON_LVDS_GEN_CNTL);
            tmp |= (RADEON_LVDS_ON | RADEON_LVDS_BLON);
            tmp &= ~(RADEON_LVDS_DISPLAY_DIS);
	    usleep (radeon_output->PanelPwrDly * 1000);
            OUTREG(RADEON_LVDS_GEN_CNTL, tmp);
            save->lvds_gen_cntl |= (RADEON_LVDS_ON | RADEON_LVDS_BLON);
            save->lvds_gen_cntl &= ~(RADEON_LVDS_DISPLAY_DIS);
        } else if (radeon_output->MonType == MT_STV ||
		   radeon_output->MonType == MT_CTV) {
	    tmp = INREG(RADEON_TV_MASTER_CNTL);
	    tmp |= RADEON_TV_ON;
	    OUTREG(RADEON_TV_MASTER_CNTL, tmp);
            tv_dac_change = 2;
	}
    } else {
	ErrorF("disable montype: %d\n", radeon_output->MonType);
        if (radeon_output->MonType == MT_CRT) {
            if (radeon_output->DACType == DAC_PRIMARY) {
                tmp = INREG(RADEON_CRTC_EXT_CNTL);
                tmp &= ~RADEON_CRTC_CRT_ON;
                OUTREG(RADEON_CRTC_EXT_CNTL, tmp);
                save->crtc_ext_cntl &= ~RADEON_CRTC_CRT_ON;
                RADEONDacPowerSet(pScrn, bEnable, (radeon_output->DACType == DAC_PRIMARY));
            } else if (radeon_output->DACType == DAC_TVDAC) {
                if (info->ChipFamily == CHIP_FAMILY_R200) {
                    tmp = INREG(RADEON_FP2_GEN_CNTL);
                    tmp &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                    OUTREG(RADEON_FP2_GEN_CNTL, tmp);
                    save->fp2_gen_cntl &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                } else {
                    tmp = INREG(RADEON_CRTC2_GEN_CNTL);
                    tmp &= ~RADEON_CRTC2_CRT2_ON;  
                    OUTREG(RADEON_CRTC2_GEN_CNTL, tmp);
                    save->crtc2_gen_cntl &= ~RADEON_CRTC2_CRT2_ON;
                }
                tv_dac_change = 1;
            }
        } else if (radeon_output->MonType == MT_DFP) {
            if (radeon_output->TMDSType == TMDS_INT) {
                tmp = INREG(RADEON_FP_GEN_CNTL);
                tmp &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
                OUTREG(RADEON_FP_GEN_CNTL, tmp);
                save->fp_gen_cntl &= ~(RADEON_FP_FPON | RADEON_FP_TMDS_EN);
            } else if (radeon_output->TMDSType == TMDS_EXT) {
                tmp = INREG(RADEON_FP2_GEN_CNTL);
                tmp &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
                OUTREG(RADEON_FP2_GEN_CNTL, tmp);
                save->fp2_gen_cntl &= ~(RADEON_FP2_ON | RADEON_FP2_DVO_EN);
            }
        } else if (radeon_output->MonType == MT_LCD) {
	    unsigned long tmpPixclksCntl = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
	    if (info->IsMobility || info->IsIGP) {
	    /* Asic bug, when turning off LVDS_ON, we have to make sure
	       RADEON_PIXCLK_LVDS_ALWAYS_ON bit is off
	    */
		OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL, 0, ~RADEON_PIXCLK_LVDS_ALWAYS_ONb);
	    }
            tmp = INREG(RADEON_LVDS_GEN_CNTL);
            tmp |= RADEON_LVDS_DISPLAY_DIS;
            tmp &= ~(RADEON_LVDS_ON | RADEON_LVDS_BLON);
            OUTREG(RADEON_LVDS_GEN_CNTL, tmp);
            save->lvds_gen_cntl |= RADEON_LVDS_DISPLAY_DIS;
            save->lvds_gen_cntl &= ~(RADEON_LVDS_ON | RADEON_LVDS_BLON);
	    if (info->IsMobility || info->IsIGP) {
		OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmpPixclksCntl);
	    }
        } else if (radeon_output->MonType == MT_STV || radeon_output->MonType == MT_CTV) {
	    tmp = INREG(RADEON_TV_MASTER_CNTL);
	    tmp &= ~RADEON_TV_ON;
	    OUTREG(RADEON_TV_MASTER_CNTL, tmp);
            tv_dac_change = 2;
	}
    }

    if (tv_dac_change) {
	if (bEnable)
		info->tv_dac_enable_mask |= tv_dac_change;
	else
		info->tv_dac_enable_mask &= ~tv_dac_change;

	if (bEnable && info->tv_dac_enable_mask)
	    RADEONDacPowerSet(pScrn, bEnable, (radeon_output->DACType == DAC_PRIMARY));
	else if (!bEnable && info->tv_dac_enable_mask == 0)
	    RADEONDacPowerSet(pScrn, bEnable, (radeon_output->DACType == DAC_PRIMARY));

    }
}

/* Calculate display buffer watermark to prevent buffer underflow */
void RADEONInitDispBandwidth2(ScrnInfoPtr pScrn, RADEONInfoPtr info, int pixel_bytes2, DisplayModePtr mode1, DisplayModePtr mode2)
{
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    CARD32 temp, data, mem_trcd, mem_trp, mem_tras, mem_trbs=0;
    float mem_tcas;
    int k1, c;
    CARD32 MemTrcdExtMemCntl[4]     = {1, 2, 3, 4};
    CARD32 MemTrpExtMemCntl[4]      = {1, 2, 3, 4};
    CARD32 MemTrasExtMemCntl[8]     = {1, 2, 3, 4, 5, 6, 7, 8};

    CARD32 MemTrcdMemTimingCntl[8]     = {1, 2, 3, 4, 5, 6, 7, 8};
    CARD32 MemTrpMemTimingCntl[8]      = {1, 2, 3, 4, 5, 6, 7, 8};
    CARD32 MemTrasMemTimingCntl[16]    = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

    float MemTcas[8]  = {0, 1, 2, 3, 0, 1.5, 2.5, 0};
    float MemTcas2[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    float MemTrbs[8]  = {1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5};

    float mem_bw, peak_disp_bw;
    float min_mem_eff = 0.8;
    float sclk_eff, sclk_delay;
    float mc_latency_mclk, mc_latency_sclk, cur_latency_mclk, cur_latency_sclk;
    float disp_latency, disp_latency_overhead, disp_drain_rate, disp_drain_rate2;
    float pix_clk, pix_clk2; /* in MHz */
    int cur_size = 16;       /* in octawords */
    int critical_point, critical_point2;
    int stop_req, max_stop_req;
    float read_return_rate, time_disp1_drop_priority;

    /* 
     * Set display0/1 priority up on r3/4xx in the memory controller for 
     * high res modes if the user specifies HIGH for displaypriority 
     * option.
     */
    if ((info->DispPriority == 2) && IS_R300_VARIANT) {
        CARD32 mc_init_misc_lat_timer = INREG(R300_MC_INIT_MISC_LAT_TIMER);
	if (pRADEONEnt->pCrtc[1]->enabled) {
	    mc_init_misc_lat_timer |= 0x1100; /* display 0 and 1 */
	} else {
	    mc_init_misc_lat_timer |= 0x0100; /* display 0 only */
	}
	OUTREG(R300_MC_INIT_MISC_LAT_TIMER, mc_init_misc_lat_timer);
    }


    /* R420 and RV410 family not supported yet */
    if (info->ChipFamily == CHIP_FAMILY_R420 || info->ChipFamily == CHIP_FAMILY_RV410) return; 

    /*
     * Determine if there is enough bandwidth for current display mode
     */
    mem_bw = info->mclk * (info->RamWidth / 8) * (info->IsDDR ? 2 : 1);

    pix_clk = mode1->Clock/1000.0;
    if (mode2)
	pix_clk2 = mode2->Clock/1000.0;
    else
	pix_clk2 = 0;

    peak_disp_bw = (pix_clk * info->CurrentLayout.pixel_bytes);
    if (pixel_bytes2)
      peak_disp_bw += (pix_clk2 * pixel_bytes2);

    if (peak_disp_bw >= mem_bw * min_mem_eff) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
		   "You may not have enough display bandwidth for current mode\n"
		   "If you have flickering problem, try to lower resolution, refresh rate, or color depth\n");
    } 

    /*  CRTC1
        Set GRPH_BUFFER_CNTL register using h/w defined optimal values.
    	GRPH_STOP_REQ <= MIN[ 0x7C, (CRTC_H_DISP + 1) * (bit depth) / 0x10 ]
    */
    stop_req = mode1->HDisplay * info->CurrentLayout.pixel_bytes / 16;

    /* setup Max GRPH_STOP_REQ default value */
    if (IS_RV100_VARIANT)
	max_stop_req = 0x5c;
    else
	max_stop_req  = 0x7c;
    if (stop_req > max_stop_req)
	stop_req = max_stop_req;
      
    /*  Get values from the EXT_MEM_CNTL register...converting its contents. */
    temp = INREG(RADEON_MEM_TIMING_CNTL);
    if ((info->ChipFamily == CHIP_FAMILY_RV100) || info->IsIGP) { /* RV100, M6, IGPs */
	mem_trcd      = MemTrcdExtMemCntl[(temp & 0x0c) >> 2];
	mem_trp       = MemTrpExtMemCntl[ (temp & 0x03) >> 0];
	mem_tras      = MemTrasExtMemCntl[(temp & 0x70) >> 4];
    } else { /* RV200 and later */
	mem_trcd      = MemTrcdMemTimingCntl[(temp & 0x07) >> 0];
	mem_trp       = MemTrpMemTimingCntl[ (temp & 0x700) >> 8];
	mem_tras      = MemTrasMemTimingCntl[(temp & 0xf000) >> 12];
    }
    
    /* Get values from the MEM_SDRAM_MODE_REG register...converting its */ 
    temp = INREG(RADEON_MEM_SDRAM_MODE_REG);
    data = (temp & (7<<20)) >> 20;
    if ((info->ChipFamily == CHIP_FAMILY_RV100) || info->IsIGP) { /* RV100, M6, IGPs */
	mem_tcas = MemTcas [data];
    } else {
	mem_tcas = MemTcas2 [data];
    }

    if (IS_R300_VARIANT) {

	/* on the R300, Tcas is included in Trbs.
	*/
	temp = INREG(RADEON_MEM_CNTL);
	data = (R300_MEM_NUM_CHANNELS_MASK & temp);
	if (data == 1) {
	    if (R300_MEM_USE_CD_CH_ONLY & temp) {
		temp  = INREG(R300_MC_IND_INDEX);
		temp &= ~R300_MC_IND_ADDR_MASK;
		temp |= R300_MC_READ_CNTL_CD_mcind;
		OUTREG(R300_MC_IND_INDEX, temp);
		temp  = INREG(R300_MC_IND_DATA);
		data = (R300_MEM_RBS_POSITION_C_MASK & temp);
	    } else {
		temp = INREG(R300_MC_READ_CNTL_AB);
		data = (R300_MEM_RBS_POSITION_A_MASK & temp);
	    }
	} else {
	    temp = INREG(R300_MC_READ_CNTL_AB);
	    data = (R300_MEM_RBS_POSITION_A_MASK & temp);
	}

	mem_trbs = MemTrbs[data];
	mem_tcas += mem_trbs;
    }

    if ((info->ChipFamily == CHIP_FAMILY_RV100) || info->IsIGP) { /* RV100, M6, IGPs */
	/* DDR64 SCLK_EFF = SCLK for analysis */
	sclk_eff = info->sclk;
    } else {
#ifdef XF86DRI
	if (info->directRenderingEnabled)
	    sclk_eff = info->sclk - (info->agpMode * 50.0 / 3.0);
	else
#endif
	    sclk_eff = info->sclk;
    }

    /* Find the memory controller latency for the display client.
    */
    if (IS_R300_VARIANT) {
	/*not enough for R350 ???*/
	/*
	if (!mode2) sclk_delay = 150;
	else {
	    if (info->RamWidth == 256) sclk_delay = 87;
	    else sclk_delay = 97;
	}
	*/
	sclk_delay = 250;
    } else {
	if ((info->ChipFamily == CHIP_FAMILY_RV100) ||
	    info->IsIGP) {
	    if (info->IsDDR) sclk_delay = 41;
	    else sclk_delay = 33;
	} else {
	    if (info->RamWidth == 128) sclk_delay = 57;
	    else sclk_delay = 41;
	}
    }

    mc_latency_sclk = sclk_delay / sclk_eff;
	
    if (info->IsDDR) {
	if (info->RamWidth == 32) {
	    k1 = 40;
	    c  = 3;
	} else {
	    k1 = 20;
	    c  = 1;
	}
    } else {
	k1 = 40;
	c  = 3;
    }
    mc_latency_mclk = ((2.0*mem_trcd + mem_tcas*c + 4.0*mem_tras + 4.0*mem_trp + k1) /
		       info->mclk) + (4.0 / sclk_eff);

    /*
      HW cursor time assuming worst case of full size colour cursor.
    */
    cur_latency_mclk = (mem_trp + MAX(mem_tras, (mem_trcd + 2*(cur_size - (info->IsDDR+1))))) / info->mclk;
    cur_latency_sclk = cur_size / sclk_eff;

    /*
      Find the total latency for the display data.
    */
    disp_latency_overhead = 8.0 / info->sclk;
    mc_latency_mclk = mc_latency_mclk + disp_latency_overhead + cur_latency_mclk;
    mc_latency_sclk = mc_latency_sclk + disp_latency_overhead + cur_latency_sclk;
    disp_latency = MAX(mc_latency_mclk, mc_latency_sclk);

    /*
      Find the drain rate of the display buffer.
    */
    disp_drain_rate = pix_clk / (16.0/info->CurrentLayout.pixel_bytes);
    if (pixel_bytes2)
	disp_drain_rate2 = pix_clk2 / (16.0/pixel_bytes2);
    else
	disp_drain_rate2 = 0;

    /*
      Find the critical point of the display buffer.
    */
    critical_point= (CARD32)(disp_drain_rate * disp_latency + 0.5); 

    /* ???? */
    /*
    temp = (info->SavedReg.grph_buffer_cntl & RADEON_GRPH_CRITICAL_POINT_MASK) >> RADEON_GRPH_CRITICAL_POINT_SHIFT;
    if (critical_point < temp) critical_point = temp;
    */
    if (info->DispPriority == 2) {
	critical_point = 0;
    }

    /*
      The critical point should never be above max_stop_req-4.  Setting
      GRPH_CRITICAL_CNTL = 0 will thus force high priority all the time.
    */
    if (max_stop_req - critical_point < 4) critical_point = 0; 

    if (critical_point == 0 && mode2 && info->ChipFamily == CHIP_FAMILY_R300) {
	/* some R300 cards have problem with this set to 0, when CRTC2 is enabled.*/
	critical_point = 0x10;
    }

    temp = info->SavedReg.grph_buffer_cntl;
    temp &= ~(RADEON_GRPH_STOP_REQ_MASK);
    temp |= (stop_req << RADEON_GRPH_STOP_REQ_SHIFT);
    temp &= ~(RADEON_GRPH_START_REQ_MASK);
    if ((info->ChipFamily == CHIP_FAMILY_R350) &&
	(stop_req > 0x15)) {
	stop_req -= 0x10;
    }
    temp |= (stop_req << RADEON_GRPH_START_REQ_SHIFT);

    temp |= RADEON_GRPH_BUFFER_SIZE;
    temp &= ~(RADEON_GRPH_CRITICAL_CNTL   |
	      RADEON_GRPH_CRITICAL_AT_SOF |
	      RADEON_GRPH_STOP_CNTL);
    /*
      Write the result into the register.
    */
    OUTREG(RADEON_GRPH_BUFFER_CNTL, ((temp & ~RADEON_GRPH_CRITICAL_POINT_MASK) |
				     (critical_point << RADEON_GRPH_CRITICAL_POINT_SHIFT)));

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "GRPH_BUFFER_CNTL from %x to %x\n",
		   (unsigned int)info->SavedReg.grph_buffer_cntl,
		   INREG(RADEON_GRPH_BUFFER_CNTL));

    if (mode2) {
	stop_req = mode2->HDisplay * pixel_bytes2 / 16;

	if (stop_req > max_stop_req) stop_req = max_stop_req;

	temp = info->SavedReg.grph2_buffer_cntl;
	temp &= ~(RADEON_GRPH_STOP_REQ_MASK);
	temp |= (stop_req << RADEON_GRPH_STOP_REQ_SHIFT);
	temp &= ~(RADEON_GRPH_START_REQ_MASK);
	if ((info->ChipFamily == CHIP_FAMILY_R350) &&
	    (stop_req > 0x15)) {
	    stop_req -= 0x10;
	}
	temp |= (stop_req << RADEON_GRPH_START_REQ_SHIFT);
	temp |= RADEON_GRPH_BUFFER_SIZE;
	temp &= ~(RADEON_GRPH_CRITICAL_CNTL   |
		  RADEON_GRPH_CRITICAL_AT_SOF |
		  RADEON_GRPH_STOP_CNTL);

	if ((info->ChipFamily == CHIP_FAMILY_RS100) || 
	    (info->ChipFamily == CHIP_FAMILY_RS200))
	    critical_point2 = 0;
	else {
	    read_return_rate = MIN(info->sclk, info->mclk*(info->RamWidth*(info->IsDDR+1)/128));
	    time_disp1_drop_priority = critical_point / (read_return_rate - disp_drain_rate);

	    critical_point2 = (CARD32)((disp_latency + time_disp1_drop_priority + 
					disp_latency) * disp_drain_rate2 + 0.5);

	    if (info->DispPriority == 2) {
		critical_point2 = 0;
	    }

	    if (max_stop_req - critical_point2 < 4) critical_point2 = 0;

	}

	if (critical_point2 == 0 && info->ChipFamily == CHIP_FAMILY_R300) {
	    /* some R300 cards have problem with this set to 0 */
	    critical_point2 = 0x10;
	}

	OUTREG(RADEON_GRPH2_BUFFER_CNTL, ((temp & ~RADEON_GRPH_CRITICAL_POINT_MASK) |
					  (critical_point2 << RADEON_GRPH_CRITICAL_POINT_SHIFT)));

	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		       "GRPH2_BUFFER_CNTL from %x to %x\n",
		       (unsigned int)info->SavedReg.grph2_buffer_cntl,
		       INREG(RADEON_GRPH2_BUFFER_CNTL));
    }
}

void RADEONInitDispBandwidth(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    DisplayModePtr mode1, mode2;
    int pixel_bytes2 = 0;

    mode1 = info->CurrentLayout.mode;
    mode2 = NULL;
    pixel_bytes2 = info->CurrentLayout.pixel_bytes;

    if (xf86_config->num_crtc == 2) {
      pixel_bytes2 = 0;
      mode2 = NULL;

      if (xf86_config->crtc[1]->enabled && xf86_config->crtc[0]->enabled) {
	pixel_bytes2 = info->CurrentLayout.pixel_bytes;
	mode1 = &xf86_config->crtc[0]->mode;
	mode2 = &xf86_config->crtc[1]->mode;
      } else if (xf86_config->crtc[0]->enabled) {
	mode1 = &xf86_config->crtc[0]->mode;
      } else if (xf86_config->crtc[1]->enabled) {
	mode1 = &xf86_config->crtc[1]->mode;
      } else
	return;
    }

    RADEONInitDispBandwidth2(pScrn, info, pixel_bytes2, mode1, mode2);
}

void RADEONBlank(ScrnInfoPtr pScrn)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    xf86OutputPtr output;
    xf86CrtcPtr crtc;
    int o, c;

    for (c = 0; c < xf86_config->num_crtc; c++) {
	crtc = xf86_config->crtc[c];
    	for (o = 0; o < xf86_config->num_output; o++) {
	    output = xf86_config->output[o];
	    if (output->crtc != crtc)
		continue;

	    output->funcs->dpms(output, DPMSModeOff);
	}
	crtc->funcs->dpms(crtc, DPMSModeOff);
    }
}

void RADEONUnblank(ScrnInfoPtr pScrn)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    xf86OutputPtr output;
    xf86CrtcPtr crtc;
    int o, c;

    for (c = 0; c < xf86_config->num_crtc; c++) {
	crtc = xf86_config->crtc[c];
	if(!crtc->enabled)
		continue;
	crtc->funcs->dpms(crtc, DPMSModeOn);
    	for (o = 0; o < xf86_config->num_output; o++) {
	    output = xf86_config->output[o];
	    if (output->crtc != crtc)
		continue;

	    output->funcs->dpms(output, DPMSModeOn);
	}
    }
}
