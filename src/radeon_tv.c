/*
 * Integrated TV out support based on the GATOS code by
 * Federico Ulivi <fulivi@lycos.com>
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
#include "radeon_tv.h"

/**********************************************************************
 *
 * ModeConstants
 *
 * Storage of constants related to a single video mode
 *
 **********************************************************************/

typedef struct
{
    CARD16 horResolution;
    CARD16 verResolution;
    TVStd  standard;
    CARD16 horTotal;
    CARD16 verTotal;
    CARD16 horStart;
    CARD16 horSyncStart;
    CARD16 verSyncStart;
    unsigned defRestart;
    CARD16 crtcPLL_N;
    CARD8  crtcPLL_M;
    CARD8  crtcPLL_postDiv;
    unsigned pixToTV;
} TVModeConstants;

static const CARD16 hor_timing_NTSC[] =
{
    0x0007,
    0x003f,
    0x0263,
    0x0a24,
    0x2a6b,
    0x0a36,
    0x126d, /* H_TABLE_POS1 */
    0x1bfe,
    0x1a8f, /* H_TABLE_POS2 */
    0x1ec7,
    0x3863,
    0x1bfe,
    0x1bfe,
    0x1a2a,
    0x1e95,
    0x0e31,
    0x201b,
    0
};

static const CARD16 vert_timing_NTSC[] =
{
    0x2001,
    0x200d,
    0x1006,
    0x0c06,
    0x1006,
    0x1818,
    0x21e3,
    0x1006,
    0x0c06,
    0x1006,
    0x1817,
    0x21d4,
    0x0002,
    0
};

static const CARD16 hor_timing_PAL[] =
{
    0x0007,
    0x0058,
    0x027c,
    0x0a31,
    0x2a77,
    0x0a95,
    0x124f, /* H_TABLE_POS1 */
    0x1bfe,
    0x1b22, /* H_TABLE_POS2 */
    0x1ef9,
    0x387c,
    0x1bfe,
    0x1bfe,
    0x1b31,
    0x1eb5,
    0x0e43,
    0x201b,
    0
};

static const CARD16 vert_timing_PAL[] =
{
    0x2001,
    0x200c,
    0x1005,
    0x0c05,
    0x1005,
    0x1401,
    0x1821,
    0x2240,
    0x1005,
    0x0c05,
    0x1005,
    0x1401,
    0x1822,
    0x2230,
    0x0002,
    0
};

/**********************************************************************
 *
 * availableModes
 *
 * Table of all allowed modes for tv output
 *
 **********************************************************************/
static const TVModeConstants availableTVModes[] =
{
    {
	800,                /* horResolution */
	600,                /* verResolution */
	TV_STD_NTSC,        /* standard */
	990,                /* horTotal */
	740,                /* verTotal */
	813,                /* horStart */
	824,                /* horSyncStart */
	632,                /* verSyncStart */
	625592,             /* defRestart */
	592,                /* crtcPLL_N */
	91,                 /* crtcPLL_M */
	4,                  /* crtcPLL_postDiv */
	1022,               /* pixToTV */
    },
    {
	800,               /* horResolution */
	600,               /* verResolution */
	TV_STD_PAL,        /* standard */
	1144,              /* horTotal */
	706,               /* verTotal */
	812,               /* horStart */
	824,               /* horSyncStart */
	669,               /* verSyncStart */
	696700,            /* defRestart */
	1382,              /* crtcPLL_N */
	231,               /* crtcPLL_M */
	4,                 /* crtcPLL_postDiv */
	759,               /* pixToTV */
    }
};

#define N_AVAILABLE_MODES (sizeof(availableModes) / sizeof(availableModes[ 0 ]))

static long YCOEF_value[5] = { 2, 2, 0, 4, 0 };
static long YCOEF_EN_value[5] = { 1, 1, 0, 1, 0 };
static long SLOPE_value[5] = { 1, 2, 2, 4, 8 };
static long SLOPE_limit[5] = { 6, 5, 4, 3, 2 };


/* Compute F,V,H restarts from default restart position and hPos & vPos
 * Return TRUE when code timing table was changed
 */
static Bool RADEONInitTVRestarts(xf86OutputPtr output, RADEONSavePtr save,
				 DisplayModePtr mode)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int restart;
    unsigned hTotal;
    unsigned vTotal;
    unsigned fTotal;
    int vOffset;
    int hOffset;
    CARD16 p1;
    CARD16 p2;
    Bool hChanged;
    CARD16 hInc;
    const TVModeConstants *constPtr;

    /* FIXME: need to revisit this when we add more modes */
    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
        radeon_output->tvStd == TV_STD_PAL_M)
	constPtr = &availableTVModes[0];
    else
	constPtr = &availableTVModes[1];

    hTotal = constPtr->horTotal;
    vTotal = constPtr->verTotal;
    
    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
        radeon_output->tvStd == TV_STD_PAL_M ||
        radeon_output->tvStd == TV_STD_PAL_60)
	fTotal = NTSC_TV_VFTOTAL + 1;
    else
	fTotal = PAL_TV_VFTOTAL + 1;

    /* Adjust positions 1&2 in hor. code timing table */
    hOffset = radeon_output->hPos * H_POS_UNIT;

    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
	radeon_output->tvStd == TV_STD_PAL_M) {
	p1 = hor_timing_NTSC[ H_TABLE_POS1 ];
	p2 = hor_timing_NTSC[ H_TABLE_POS2 ];
    } else {
	p1 = hor_timing_PAL[ H_TABLE_POS1 ];
	p2 = hor_timing_PAL[ H_TABLE_POS2 ];
    }


    p1 = (CARD16)((int)p1 + hOffset);
    p2 = (CARD16)((int)p2 - hOffset);

    hChanged = (p1 != save->h_code_timing[ H_TABLE_POS1 ] || 
		p2 != save->h_code_timing[ H_TABLE_POS2 ]);

    save->h_code_timing[ H_TABLE_POS1 ] = p1;
    save->h_code_timing[ H_TABLE_POS2 ] = p2;

    /* Convert hOffset from n. of TV clock periods to n. of CRTC clock periods (CRTC pixels) */
    hOffset = (hOffset * (int)(constPtr->pixToTV)) / 1000;

    /* Adjust restart */
    restart = constPtr->defRestart;
 
    /*
     * Convert vPos TV lines to n. of CRTC pixels
     * Be verrrrry careful when mixing signed & unsigned values in C..
     */
    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
	radeon_output->tvStd == TV_STD_PAL_M ||
	radeon_output->tvStd == TV_STD_PAL_60)
	vOffset = ((int)(vTotal * hTotal) * 2 * radeon_output->vPos) / (int)(NTSC_TV_LINES_PER_FRAME);
    else
	vOffset = ((int)(vTotal * hTotal) * 2 * radeon_output->vPos) / (int)(PAL_TV_LINES_PER_FRAME);

    restart -= vOffset + hOffset;

    ErrorF("computeRestarts: def = %u, h = %d, v = %d, p1=%04x, p2=%04x, restart = %d\n",
	   constPtr->defRestart , radeon_output->hPos , radeon_output->vPos , p1 , p2 , restart);

    save->tv_hrestart = restart % hTotal;
    restart /= hTotal;
    save->tv_vrestart = restart % vTotal;
    restart /= vTotal;
    save->tv_frestart = restart % fTotal;

    ErrorF("computeRestarts: F/H/V=%u,%u,%u\n",
	   (unsigned)save->tv_frestart, (unsigned)save->tv_vrestart,
	   (unsigned)save->tv_hrestart);

    /* Compute H_INC from hSize */
    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
	radeon_output->tvStd == TV_STD_PAL_M)
	hInc = (CARD16)((int)(constPtr->horResolution * 4096 * NTSC_TV_CLOCK_T) /
			(radeon_output->hSize * (int)(NTSC_TV_H_SIZE_UNIT) + (int)(NTSC_TV_ZERO_H_SIZE)));
    else
	hInc = (CARD16)((int)(constPtr->horResolution * 4096 * PAL_TV_CLOCK_T) /
			(radeon_output->hSize * (int)(PAL_TV_H_SIZE_UNIT) + (int)(PAL_TV_ZERO_H_SIZE)));

    save->tv_timing_cntl = (save->tv_timing_cntl & ~RADEON_H_INC_MASK) |
	((CARD32)hInc << RADEON_H_INC_SHIFT);

    ErrorF("computeRestarts: hSize=%d,hInc=%u\n" , radeon_output->hSize , hInc);

    return hChanged;
}

/* intit TV-out regs */
void RADEONInitTVRegisters(xf86OutputPtr output, RADEONSavePtr save,
                                  DisplayModePtr mode, BOOL IsPrimary)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    unsigned i;
    unsigned long vert_space, flicker_removal;
    CARD32 tmp;
    const TVModeConstants *constPtr;
    const CARD16 *hor_timing;
    const CARD16 *vert_timing;


    /* FIXME: need to revisit this when we add more modes */
    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
	radeon_output->tvStd == TV_STD_PAL_M)
	constPtr = &availableTVModes[0];
    else
	constPtr = &availableTVModes[1];

    save->tv_crc_cntl = 0;

    save->tv_gain_limit_settings = (0x17f << RADEON_UV_GAIN_LIMIT_SHIFT) | 
	                           (0x5ff << RADEON_Y_GAIN_LIMIT_SHIFT);

    save->tv_hdisp = constPtr->horResolution - 1;
    save->tv_hstart = constPtr->horStart;
    save->tv_htotal = constPtr->horTotal - 1;

    save->tv_linear_gain_settings = (0x100 << RADEON_UV_GAIN_SHIFT) |
	                            (0x100 << RADEON_Y_GAIN_SHIFT);

    save->tv_master_cntl = (RADEON_VIN_ASYNC_RST
			    | RADEON_CRT_FIFO_CE_EN
			    | RADEON_TV_FIFO_CE_EN
			    | RADEON_TV_ON);

    if (!IS_R300_VARIANT)
	save->tv_master_cntl |= RADEON_TVCLK_ALWAYS_ONb;

    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J)
	save->tv_master_cntl |= RADEON_RESTART_PHASE_FIX;

    save->tv_modulator_cntl1 = RADEON_SLEW_RATE_LIMIT
	                       | RADEON_SYNC_TIP_LEVEL
	                       | RADEON_YFLT_EN
	                       | RADEON_UVFLT_EN
	                       | (6 << RADEON_CY_FILT_BLEND_SHIFT);

    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J) {
	save->tv_modulator_cntl1 |= (0x46 << RADEON_SET_UP_LEVEL_SHIFT)
	                            | (0x3b << RADEON_BLANK_LEVEL_SHIFT);
	save->tv_modulator_cntl2 = (-111 & RADEON_TV_U_BURST_LEVEL_MASK) |
	    ((0 & RADEON_TV_V_BURST_LEVEL_MASK) << RADEON_TV_V_BURST_LEVEL_SHIFT);
    } else if (radeon_output->tvStd == TV_STD_SCART_PAL) {
	save->tv_modulator_cntl1 |= RADEON_ALT_PHASE_EN;
	save->tv_modulator_cntl2 = (0 & RADEON_TV_U_BURST_LEVEL_MASK) |
	    ((0 & RADEON_TV_V_BURST_LEVEL_MASK) << RADEON_TV_V_BURST_LEVEL_SHIFT);
    } else {
	save->tv_modulator_cntl1 |= RADEON_ALT_PHASE_EN
	                            | (0x3b << RADEON_SET_UP_LEVEL_SHIFT)
	                            | (0x3b << RADEON_BLANK_LEVEL_SHIFT);
	save->tv_modulator_cntl2 = (-78 & RADEON_TV_U_BURST_LEVEL_MASK) |
	    ((62 & RADEON_TV_V_BURST_LEVEL_MASK) << RADEON_TV_V_BURST_LEVEL_SHIFT);
    }

    save->pll_test_cntl = 0;

    save->tv_pre_dac_mux_cntl = (RADEON_Y_RED_EN
				 | RADEON_C_GRN_EN
				 | RADEON_CMP_BLU_EN
				 | RADEON_DAC_DITHER_EN);

    save->tv_rgb_cntl = (RADEON_RGB_DITHER_EN
			 | RADEON_TVOUT_SCALE_EN
			 | (0x0b << RADEON_UVRAM_READ_MARGIN_SHIFT)
			 | (0x07 << RADEON_FIFORAM_FFMACRO_READ_MARGIN_SHIFT));

    if (IsPrimary) {
	if (radeon_output->Flags & RADEON_USE_RMX)
	    save->tv_rgb_cntl |= RADEON_RGB_SRC_SEL_RMX;
	else
	    save->tv_rgb_cntl |= RADEON_RGB_SRC_SEL_CRTC1;
    } else {
	save->tv_rgb_cntl |= RADEON_RGB_SRC_SEL_CRTC2;
    }

    save->tv_sync_cntl = RADEON_SYNC_PUB | RADEON_TV_SYNC_IO_DRIVE;

    save->tv_sync_size = constPtr->horResolution + 8;

    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
	radeon_output->tvStd == TV_STD_PAL_M ||
	radeon_output->tvStd == TV_STD_PAL_60)
	vert_space = constPtr->verTotal * 2 * 10000 / NTSC_TV_LINES_PER_FRAME;
    else
	vert_space = constPtr->verTotal * 2 * 10000 / PAL_TV_LINES_PER_FRAME;

    save->tv_vscaler_cntl1 = RADEON_Y_W_EN;
    save->tv_vscaler_cntl1 =
	(save->tv_vscaler_cntl1 & 0xe3ff0000) | (vert_space * (1 << FRAC_BITS) / 10000);
    save->tv_vscaler_cntl1 |= RADEON_RESTART_FIELD;
    if (constPtr->horResolution == 1024)
	save->tv_vscaler_cntl1 |= (4 << RADEON_Y_DEL_W_SIG_SHIFT);
    else
	save->tv_vscaler_cntl1 |= (2 << RADEON_Y_DEL_W_SIG_SHIFT);

    if (radeon_output->tvStd == TV_STD_NTSC ||
        radeon_output->tvStd == TV_STD_NTSC_J ||
        radeon_output->tvStd == TV_STD_PAL_M ||
        radeon_output->tvStd == TV_STD_PAL_60)
	flicker_removal =
	    (float) constPtr->verTotal * 2.0 / NTSC_TV_LINES_PER_FRAME + 0.5;
    else
	flicker_removal =
	    (float) constPtr->verTotal * 2.0 / PAL_TV_LINES_PER_FRAME + 0.5;

    if (flicker_removal < 3)
	flicker_removal = 3;
    for (i = 0; i < 6; ++i) {
	if (flicker_removal == SLOPE_limit[i])
	    break;
    }
    save->tv_y_saw_tooth_cntl =
	(vert_space * SLOPE_value[i] * (1 << (FRAC_BITS - 1)) + 5001) / 10000 / 8
	| ((SLOPE_value[i] * (1 << (FRAC_BITS - 1)) / 8) << 16);
    save->tv_y_fall_cntl =
	(YCOEF_EN_value[i] << 17) | ((YCOEF_value[i] * (1 << 8) / 8) << 24) |
	RADEON_Y_FALL_PING_PONG | (272 * SLOPE_value[i] / 8) * (1 << (FRAC_BITS - 1)) /
	1024;
    save->tv_y_rise_cntl =
	RADEON_Y_RISE_PING_PONG
	| (flicker_removal * 1024 - 272) * SLOPE_value[i] / 8 * (1 << (FRAC_BITS - 1)) / 1024;

    save->tv_vscaler_cntl2 = ((save->tv_vscaler_cntl2 & 0x00fffff0)
			      | (0x10 << 24)
			      | RADEON_DITHER_MODE
			      | RADEON_Y_OUTPUT_DITHER_EN
			      | RADEON_UV_OUTPUT_DITHER_EN
			      | RADEON_UV_TO_BUF_DITHER_EN);

    tmp = (save->tv_vscaler_cntl1 >> RADEON_UV_INC_SHIFT) & RADEON_UV_INC_MASK;
    tmp = ((16384 * 256 * 10) / tmp + 5) / 10;
    tmp = (tmp << RADEON_UV_OUTPUT_POST_SCALE_SHIFT) | 0x000b0000;
    save->tv_timing_cntl = tmp;

    save->tv_dac_cntl = (RADEON_TV_DAC_NBLANK |
			 RADEON_TV_DAC_NHOLD |
			 radeon_output->tv_dac_adj /*(8 << 16) | (6 << 20)*/);

    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J)
	save->tv_dac_cntl |= RADEON_TV_DAC_STD_NTSC;
    else
	save->tv_dac_cntl |= RADEON_TV_DAC_STD_PAL;

#if 0
    /* needs fixes for r4xx */
    save->tv_dac_cntl |= (RADEON_TV_DAC_RDACPD | RADEON_TV_DAC_GDACPD
	                 | RADEON_TV_DAC_BDACPD);

    if (radeon_output->MonType == MT_CTV) {
	save->tv_dac_cntl &= ~RADEON_TV_DAC_BDACPD;
    }

    if (radeon_output->MonType == MT_STV) {
	save->tv_dac_cntl &= ~(RADEON_TV_DAC_RDACPD |
			       RADEON_TV_DAC_GDACPD);
    }
#endif

    if (radeon_output->tvStd == TV_STD_NTSC ||
        radeon_output->tvStd == TV_STD_NTSC_J)
	save->tv_pll_cntl = (NTSC_TV_PLL_M & RADEON_TV_M0LO_MASK) |
	    (((NTSC_TV_PLL_M >> 8) & RADEON_TV_M0HI_MASK) << RADEON_TV_M0HI_SHIFT) |
	    ((NTSC_TV_PLL_N & RADEON_TV_N0LO_MASK) << RADEON_TV_N0LO_SHIFT) |
	    (((NTSC_TV_PLL_N >> 8) & RADEON_TV_N0HI_MASK) << RADEON_TV_N0HI_SHIFT) |
	    ((NTSC_TV_PLL_P & RADEON_TV_P_MASK) << RADEON_TV_P_SHIFT);
    else
	save->tv_pll_cntl = (PAL_TV_PLL_M & RADEON_TV_M0LO_MASK) |
	    (((PAL_TV_PLL_M >> 8) & RADEON_TV_M0HI_MASK) << RADEON_TV_M0HI_SHIFT) |
	    ((PAL_TV_PLL_N & RADEON_TV_N0LO_MASK) << RADEON_TV_N0LO_SHIFT) |
	    (((PAL_TV_PLL_N >> 8) & RADEON_TV_N0HI_MASK) << RADEON_TV_N0HI_SHIFT) |
	    ((PAL_TV_PLL_P & RADEON_TV_P_MASK) << RADEON_TV_P_SHIFT);

    save->tv_pll_cntl1 =  (((4 & RADEON_TVPCP_MASK)<< RADEON_TVPCP_SHIFT) |
			   ((4 & RADEON_TVPVG_MASK) << RADEON_TVPVG_SHIFT) |
			   ((1 & RADEON_TVPDC_MASK)<< RADEON_TVPDC_SHIFT) |
			   RADEON_TVCLK_SRC_SEL_TVPLL |
			   RADEON_TVPLL_TEST_DIS);

    save->tv_upsamp_and_gain_cntl = RADEON_YUPSAMP_EN | RADEON_UVUPSAMP_EN;

    save->tv_uv_adr = 0xc8;

    save->tv_vdisp = constPtr->verResolution - 1;

    if (radeon_output->tvStd == TV_STD_NTSC ||
        radeon_output->tvStd == TV_STD_NTSC_J ||
        radeon_output->tvStd == TV_STD_PAL_M ||
        radeon_output->tvStd == TV_STD_PAL_60)
	save->tv_ftotal = NTSC_TV_VFTOTAL;
    else
	save->tv_ftotal = PAL_TV_VFTOTAL;

    save->tv_vtotal = constPtr->verTotal - 1;

    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
	radeon_output->tvStd == TV_STD_PAL_M) {
	hor_timing = hor_timing_NTSC;
    } else {
	hor_timing = hor_timing_PAL;
    }

    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
	radeon_output->tvStd == TV_STD_PAL_M ||
	radeon_output->tvStd == TV_STD_PAL_60) {
	vert_timing = vert_timing_NTSC;
    } else {
	vert_timing = vert_timing_PAL;
    }

    for (i = 0; i < MAX_H_CODE_TIMING_LEN; i++) {
	if ((save->h_code_timing[ i ] = hor_timing[ i ]) == 0)
	    break;
    }

    for (i = 0; i < MAX_V_CODE_TIMING_LEN; i++) {
	if ((save->v_code_timing[ i ] = vert_timing[ i ]) == 0)
	    break;
    }

    /*
     * This must be called AFTER loading timing tables as they are modified by this function
     */
    RADEONInitTVRestarts(output, save, mode);

    save->dac_cntl &= ~RADEON_DAC_TVO_EN;

    if (IS_R300_VARIANT)
        save->gpiopad_a = info->SavedReg.gpiopad_a & ~1;

    if (IsPrimary) {
	save->disp_output_cntl &= ~RADEON_DISP_TVDAC_SOURCE_MASK;
	save->disp_output_cntl |= (RADEON_DISP_TVDAC_SOURCE_CRTC
				   | RADEON_DISP_TV_SOURCE_CRTC);
    	if (info->ChipFamily >= CHIP_FAMILY_R200) {
	    save->disp_tv_out_cntl &= ~RADEON_DISP_TV_PATH_SRC_CRTC2;
    	} else {
            save->disp_hw_debug |= RADEON_CRT2_DISP1_SEL;
    	}
    } else {
	save->disp_output_cntl &= ~RADEON_DISP_DAC_SOURCE_MASK;
	save->disp_output_cntl |= RADEON_DISP_TV_SOURCE_CRTC;

    	if (info->ChipFamily >= CHIP_FAMILY_R200) {
	    save->disp_tv_out_cntl |= RADEON_DISP_TV_PATH_SRC_CRTC2;
    	} else {
            save->disp_hw_debug &= ~RADEON_CRT2_DISP1_SEL;
    	}
    }
}


/* Set hw registers for a new h/v position & h size */
void RADEONUpdateHVPosition(xf86OutputPtr output, DisplayModePtr mode)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    Bool reloadTable;
    RADEONSavePtr restore = &info->ModeReg;

    reloadTable = RADEONInitTVRestarts(output, restore, mode);

    RADEONRestoreTVRestarts(pScrn, restore);

    OUTREG(RADEON_TV_TIMING_CNTL, restore->tv_timing_cntl);

    if (reloadTable) {
	 OUTREG(RADEON_TV_MASTER_CNTL, restore->tv_master_cntl
		                       | RADEON_TV_ASYNC_RST
		                       | RADEON_CRT_ASYNC_RST
		                       | RADEON_RESTART_PHASE_FIX);

	RADEONRestoreTVTimingTables(pScrn, restore);

	OUTREG(RADEON_TV_MASTER_CNTL, restore->tv_master_cntl);
    }
}

void RADEONAdjustCrtcRegistersForTV(ScrnInfoPtr pScrn, RADEONSavePtr save,
				    DisplayModePtr mode, xf86OutputPtr output)
{
    const TVModeConstants *constPtr;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    /* FIXME: need to revisit this when we add more modes */
    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
        radeon_output->tvStd == TV_STD_PAL_M)
	constPtr = &availableTVModes[0];
    else
	constPtr = &availableTVModes[1];

    save->crtc_h_total_disp = (((constPtr->horResolution / 8) - 1) << RADEON_CRTC_H_DISP_SHIFT) |
	(((constPtr->horTotal / 8) - 1) << RADEON_CRTC_H_TOTAL_SHIFT);

    save->crtc_h_sync_strt_wid = (save->crtc_h_sync_strt_wid 
				  & ~(RADEON_CRTC_H_SYNC_STRT_PIX | RADEON_CRTC_H_SYNC_STRT_CHAR)) |
	(((constPtr->horSyncStart / 8) - 1) << RADEON_CRTC_H_SYNC_STRT_CHAR_SHIFT) |
	(constPtr->horSyncStart & 7);

    save->crtc_v_total_disp = ((constPtr->verResolution - 1) << RADEON_CRTC_V_DISP_SHIFT) |
	((constPtr->verTotal - 1) << RADEON_CRTC_V_TOTAL_SHIFT);

    save->crtc_v_sync_strt_wid = (save->crtc_v_sync_strt_wid & ~RADEON_CRTC_V_SYNC_STRT) |
	((constPtr->verSyncStart - 1) << RADEON_CRTC_V_SYNC_STRT_SHIFT);

}

void RADEONAdjustPLLRegistersForTV(ScrnInfoPtr pScrn, RADEONSavePtr save,
				   DisplayModePtr mode, xf86OutputPtr output)
{
    unsigned postDiv;
    const TVModeConstants *constPtr;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    /* FIXME: need to revisit this when we add more modes */
    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
        radeon_output->tvStd == TV_STD_PAL_M)
	constPtr = &availableTVModes[0];
    else
	constPtr = &availableTVModes[1];

    save->htotal_cntl = (constPtr->horTotal & 0x7 /*0xf*/) | RADEON_HTOT_CNTL_VGA_EN;

    save->ppll_ref_div = constPtr->crtcPLL_M;

    switch (constPtr->crtcPLL_postDiv) {
    case 1:
	postDiv = 0;
	break;
    case 2:
	postDiv = 1;
	break;
    case 3:
	postDiv = 4;
	break;
    case 4:
	postDiv = 2;
	break;
    case 6:
	postDiv = 6;
	break;
    case 8:
	postDiv = 3;
	break;
    case 12:
	postDiv = 7;
	break;
    case 16:
    default:
	postDiv = 5;
	break;
    }

    save->ppll_div_3 = (constPtr->crtcPLL_N & 0x7ff) | (postDiv << 16);

    save->pixclks_cntl &= ~(RADEON_PIX2CLK_SRC_SEL_MASK | RADEON_PIXCLK_TV_SRC_SEL);
    save->pixclks_cntl |= RADEON_PIX2CLK_SRC_SEL_P2PLLCLK;

}

void RADEONAdjustCrtc2RegistersForTV(ScrnInfoPtr pScrn, RADEONSavePtr save,
				     DisplayModePtr mode, xf86OutputPtr output)
{
    const TVModeConstants *constPtr;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    /* FIXME: need to revisit this when we add more modes */
    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
        radeon_output->tvStd == TV_STD_PAL_M)
	constPtr = &availableTVModes[0];
    else
	constPtr = &availableTVModes[1];

    save->crtc2_h_total_disp = (((constPtr->horResolution / 8) - 1) << RADEON_CRTC_H_DISP_SHIFT) |
	(((constPtr->horTotal / 8) - 1) << RADEON_CRTC_H_TOTAL_SHIFT);

    save->crtc2_h_sync_strt_wid = (save->crtc2_h_sync_strt_wid 
				  & ~(RADEON_CRTC_H_SYNC_STRT_PIX | RADEON_CRTC_H_SYNC_STRT_CHAR)) |
	(((constPtr->horSyncStart / 8) - 1) << RADEON_CRTC_H_SYNC_STRT_CHAR_SHIFT) |
	(constPtr->horSyncStart & 7);

    save->crtc2_v_total_disp = ((constPtr->verResolution - 1) << RADEON_CRTC_V_DISP_SHIFT) |
	((constPtr->verTotal - 1) << RADEON_CRTC_V_TOTAL_SHIFT);

    save->crtc_v_sync_strt_wid = (save->crtc_v_sync_strt_wid & ~RADEON_CRTC_V_SYNC_STRT) |
	((constPtr->verSyncStart - 1) << RADEON_CRTC_V_SYNC_STRT_SHIFT);

}

void RADEONAdjustPLL2RegistersForTV(ScrnInfoPtr pScrn, RADEONSavePtr save,
				    DisplayModePtr mode, xf86OutputPtr output)
{
    unsigned postDiv;
    const TVModeConstants *constPtr;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;

    /* FIXME: need to revisit this when we add more modes */
    if (radeon_output->tvStd == TV_STD_NTSC ||
	radeon_output->tvStd == TV_STD_NTSC_J ||
        radeon_output->tvStd == TV_STD_PAL_M)
	constPtr = &availableTVModes[0];
    else
	constPtr = &availableTVModes[1];

    save->htotal_cntl2 = (constPtr->horTotal & 0x7); /* 0xf */

    save->p2pll_ref_div = constPtr->crtcPLL_M;

    switch (constPtr->crtcPLL_postDiv) {
    case 1:
	postDiv = 0;
	break;
    case 2:
	postDiv = 1;
	break;
    case 3:
	postDiv = 4;
	break;
    case 4:
	postDiv = 2;
	break;
    case 6:
	postDiv = 6;
	break;
    case 8:
	postDiv = 3;
	break;
    case 12:
	postDiv = 7;
	break;
    case 16:
    default:
	postDiv = 5;
	break;
    }

    save->p2pll_div_0 = (constPtr->crtcPLL_N & 0x7ff) | (postDiv << 16);

    save->pixclks_cntl &= ~RADEON_PIX2CLK_SRC_SEL_MASK;
    save->pixclks_cntl |= (RADEON_PIX2CLK_SRC_SEL_P2PLLCLK
			   | RADEON_PIXCLK_TV_SRC_SEL);

}
