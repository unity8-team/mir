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

#include "ati_pciids_gen.h"

#if defined(ACCEL_MMIO) && defined(ACCEL_CP)
#error Cannot define both MMIO and CP acceleration!
#endif

#if !defined(UNIXCPP) || defined(ANSICPP)
#define FUNC_NAME_CAT(prefix,suffix) prefix##suffix
#else
#define FUNC_NAME_CAT(prefix,suffix) prefix/**/suffix
#endif

#ifdef ACCEL_MMIO
#define FUNC_NAME(prefix) FUNC_NAME_CAT(prefix,MMIO)
#else
#ifdef ACCEL_CP
#define FUNC_NAME(prefix) FUNC_NAME_CAT(prefix,CP)
#else
#error No accel type defined!
#endif
#endif

static void FUNC_NAME(RADEONInit3DEngine)(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    CARD32 gb_tile_config;
    ACCEL_PREAMBLE();

    info->texW[0] = info->texH[0] = info->texW[1] = info->texH[1] = 1;

    if (IS_R300_VARIANT || IS_AVIVO_VARIANT || info->ChipFamily == CHIP_FAMILY_RS690) {

	BEGIN_ACCEL(3);
	OUT_ACCEL_REG(R300_RB3D_DSTCACHE_CTLSTAT, R300_DC_FLUSH_3D | R300_DC_FREE_3D);
	OUT_ACCEL_REG(R300_RB3D_ZCACHE_CTLSTAT, R300_ZC_FLUSH | R300_ZC_FREE);
	OUT_ACCEL_REG(R300_WAIT_UNTIL, R300_WAIT_2D_IDLECLEAN | R300_WAIT_3D_IDLECLEAN);
	FINISH_ACCEL();

	gb_tile_config = (R300_ENABLE_TILING | R300_TILE_SIZE_16 | R300_SUBPIXEL_1_16);

	if ((info->Chipset == PCI_CHIP_RV410_5E4C) ||
	    (info->Chipset == PCI_CHIP_RV410_5E4F)) {
	    /* RV410 SE chips */
	    gb_tile_config |= R300_PIPE_COUNT_RV350;
	} else if ((info->ChipFamily == CHIP_FAMILY_RV350) ||
		   (info->ChipFamily == CHIP_FAMILY_RV380) ||
		   (info->ChipFamily == CHIP_FAMILY_RS400)) {
	    /* RV3xx, RS4xx chips */
	    gb_tile_config |= R300_PIPE_COUNT_RV350;
	} else if ((info->ChipFamily == CHIP_FAMILY_R300) ||
		   (info->ChipFamily == CHIP_FAMILY_R350)) {
	    /* R3xx chips */
	    gb_tile_config |= R300_PIPE_COUNT_R300;
	} else if ((info->ChipFamily == CHIP_FAMILY_RV410) ||
		   (info->ChipFamily == CHIP_FAMILY_RS690)) {
	    /* RV4xx, RS6xx chips */
	    gb_tile_config |= R300_PIPE_COUNT_R420_3P;
	} else {
	    /* R4xx, R5xx chips */
	    gb_tile_config |= R300_PIPE_COUNT_R420;
	}

	BEGIN_ACCEL(3);
	OUT_ACCEL_REG(R300_GB_TILE_CONFIG, gb_tile_config);
	OUT_ACCEL_REG(R300_GB_SELECT, 0);
	OUT_ACCEL_REG(R300_GB_ENABLE, 0);
	FINISH_ACCEL();

	BEGIN_ACCEL(3);
	OUT_ACCEL_REG(R300_RB3D_DSTCACHE_CTLSTAT, R300_DC_FLUSH_3D | R300_DC_FREE_3D);
	OUT_ACCEL_REG(R300_RB3D_ZCACHE_CTLSTAT, R300_ZC_FLUSH | R300_ZC_FREE);
	OUT_ACCEL_REG(R300_WAIT_UNTIL, R300_WAIT_2D_IDLECLEAN | R300_WAIT_3D_IDLECLEAN);
	FINISH_ACCEL();

	BEGIN_ACCEL(5);
	OUT_ACCEL_REG(R300_GB_AA_CONFIG, 0);
	OUT_ACCEL_REG(R300_RB3D_DSTCACHE_CTLSTAT, R300_DC_FLUSH_3D | R300_DC_FREE_3D);
	OUT_ACCEL_REG(R300_RB3D_ZCACHE_CTLSTAT, R300_ZC_FLUSH | R300_ZC_FREE);
	OUT_ACCEL_REG(R300_GB_MSPOS0, ((8 << R300_MS_X0_SHIFT) |
				       (8 << R300_MS_Y0_SHIFT) |
				       (8 << R300_MS_X1_SHIFT) |
				       (8 << R300_MS_Y1_SHIFT) |
				       (8 << R300_MS_X2_SHIFT) |
				       (8 << R300_MS_Y2_SHIFT) |
				       (8 << R300_MSBD0_Y_SHIFT) |
				       (7 << R300_MSBD0_X_SHIFT)));
	OUT_ACCEL_REG(R300_GB_MSPOS1, ((8 << R300_MS_X3_SHIFT) |
				       (8 << R300_MS_Y3_SHIFT) |
				       (8 << R300_MS_X4_SHIFT) |
				       (8 << R300_MS_Y4_SHIFT) |
				       (8 << R300_MS_X5_SHIFT) |
				       (8 << R300_MS_Y5_SHIFT) |
				       (8 << R300_MSBD1_SHIFT)));
	FINISH_ACCEL();

	BEGIN_ACCEL(4);
	OUT_ACCEL_REG(R300_GA_POLY_MODE, R300_FRONT_PTYPE_TRIANGE | R300_BACK_PTYPE_TRIANGE);
	OUT_ACCEL_REG(R300_GA_ROUND_MODE, (R300_GEOMETRY_ROUND_NEAREST |
					   R300_COLOR_ROUND_NEAREST));
	OUT_ACCEL_REG(R300_GA_COLOR_CONTROL, (R300_RGB0_SHADING_GOURAUD |
					      R300_ALPHA0_SHADING_GOURAUD |
					      R300_RGB1_SHADING_GOURAUD |
					      R300_ALPHA1_SHADING_GOURAUD |
					      R300_RGB2_SHADING_GOURAUD |
					      R300_ALPHA2_SHADING_GOURAUD |
					      R300_RGB3_SHADING_GOURAUD |
					      R300_ALPHA3_SHADING_GOURAUD));
	OUT_ACCEL_REG(R300_GA_OFFSET, 0);
	FINISH_ACCEL();

	BEGIN_ACCEL(5);
	OUT_ACCEL_REG(R300_SU_TEX_WRAP, 0);
	OUT_ACCEL_REG(R300_SU_POLY_OFFSET_ENABLE, 0);
	OUT_ACCEL_REG(R300_SU_CULL_MODE, R300_FACE_NEG);
	OUT_ACCEL_REG(R300_SU_DEPTH_SCALE, 0x4b7fffff);
	OUT_ACCEL_REG(R300_SU_DEPTH_OFFSET, 0);
	FINISH_ACCEL();

	BEGIN_ACCEL(5);
	OUT_ACCEL_REG(R300_US_W_FMT, 0);
	OUT_ACCEL_REG(R300_US_OUT_FMT_1, (R300_OUT_FMT_UNUSED |
					  R300_OUT_FMT_C0_SEL_BLUE |
					  R300_OUT_FMT_C1_SEL_GREEN |
					  R300_OUT_FMT_C2_SEL_RED |
					  R300_OUT_FMT_C3_SEL_ALPHA));
	OUT_ACCEL_REG(R300_US_OUT_FMT_2, (R300_OUT_FMT_UNUSED |
					  R300_OUT_FMT_C0_SEL_BLUE |
					  R300_OUT_FMT_C1_SEL_GREEN |
					  R300_OUT_FMT_C2_SEL_RED |
					  R300_OUT_FMT_C3_SEL_ALPHA));
	OUT_ACCEL_REG(R300_US_OUT_FMT_3, (R300_OUT_FMT_UNUSED |
					  R300_OUT_FMT_C0_SEL_BLUE |
					  R300_OUT_FMT_C1_SEL_GREEN |
					  R300_OUT_FMT_C2_SEL_RED |
					  R300_OUT_FMT_C3_SEL_ALPHA));
	OUT_ACCEL_REG(R300_US_OUT_FMT_0, (R300_OUT_FMT_C4_10 |
					  R300_OUT_FMT_C0_SEL_BLUE |
					  R300_OUT_FMT_C1_SEL_GREEN |
					  R300_OUT_FMT_C2_SEL_RED |
					  R300_OUT_FMT_C3_SEL_ALPHA));
	FINISH_ACCEL();


	BEGIN_ACCEL(3);
	OUT_ACCEL_REG(R300_FG_DEPTH_SRC, 0);
	OUT_ACCEL_REG(R300_FG_FOG_BLEND, 0);
	OUT_ACCEL_REG(R300_FG_ALPHA_FUNC, 0);
	FINISH_ACCEL();

	BEGIN_ACCEL(12);
	OUT_ACCEL_REG(R300_RB3D_ZSTENCILCNTL, 0);
	OUT_ACCEL_REG(R300_RB3D_ZCACHE_CTLSTAT, R300_ZC_FLUSH | R300_ZC_FREE);
	OUT_ACCEL_REG(R300_RB3D_BW_CNTL, 0);
	OUT_ACCEL_REG(R300_RB3D_ZCNTL, 0);
	OUT_ACCEL_REG(R300_RB3D_ZTOP, 0);
	OUT_ACCEL_REG(R300_RB3D_ROPCNTL, 0);

	OUT_ACCEL_REG(R300_RB3D_AARESOLVE_CTL, 0);
	OUT_ACCEL_REG(R300_RB3D_COLOR_CHANNEL_MASK, (R300_BLUE_MASK_EN |
						     R300_GREEN_MASK_EN |
						     R300_RED_MASK_EN |
						     R300_ALPHA_MASK_EN));
	OUT_ACCEL_REG(R300_RB3D_DSTCACHE_CTLSTAT, R300_DC_FLUSH_3D | R300_DC_FREE_3D);
	OUT_ACCEL_REG(R300_RB3D_CCTL, 0);
	OUT_ACCEL_REG(R300_RB3D_DITHER_CTL, 0);
	OUT_ACCEL_REG(R300_RB3D_DSTCACHE_CTLSTAT, R300_DC_FLUSH_3D | R300_DC_FREE_3D);
	FINISH_ACCEL();

	BEGIN_ACCEL(7);
	OUT_ACCEL_REG(R300_SC_EDGERULE, 0xA5294A5);
	OUT_ACCEL_REG(R300_SC_SCISSOR0, ((0 << R300_SCISSOR_X_SHIFT) |
					 (0 << R300_SCISSOR_Y_SHIFT)));
	OUT_ACCEL_REG(R300_SC_SCISSOR1, ((8191 << R300_SCISSOR_X_SHIFT) |
					 (8191 << R300_SCISSOR_Y_SHIFT)));

	if (IS_R300_VARIANT || (info->ChipFamily == CHIP_FAMILY_RS690)) {
	    /* clip has offset 1440 */
	    OUT_ACCEL_REG(R300_SC_CLIP_0_A, ((1088 << R300_CLIP_X_SHIFT) |
					     (1088 << R300_CLIP_Y_SHIFT)));
	    OUT_ACCEL_REG(R300_SC_CLIP_0_B, (((1080 + 2920) << R300_CLIP_X_SHIFT) |
					     ((1080 + 2920) << R300_CLIP_Y_SHIFT)));
	} else {
	    OUT_ACCEL_REG(R300_SC_CLIP_0_A, ((0 << R300_CLIP_X_SHIFT) |
					     (0 << R300_CLIP_Y_SHIFT)));
	    OUT_ACCEL_REG(R300_SC_CLIP_0_B, ((4080 << R300_CLIP_X_SHIFT) |
					     (4080 << R300_CLIP_Y_SHIFT)));
	}
	OUT_ACCEL_REG(R300_SC_CLIP_RULE, 0xAAAA);
	OUT_ACCEL_REG(R300_SC_SCREENDOOR, 0xffffff);
	FINISH_ACCEL();
    } else if ((info->ChipFamily == CHIP_FAMILY_RV250) ||
	       (info->ChipFamily == CHIP_FAMILY_RV280) ||
	       (info->ChipFamily == CHIP_FAMILY_RS300) ||
	       (info->ChipFamily == CHIP_FAMILY_R200)) {

	BEGIN_ACCEL(7);
	if (info->ChipFamily == CHIP_FAMILY_RS300) {
	    OUT_ACCEL_REG(R200_SE_VAP_CNTL_STATUS, RADEON_TCL_BYPASS);
	} else {
	    OUT_ACCEL_REG(R200_SE_VAP_CNTL_STATUS, 0);
	}
	OUT_ACCEL_REG(R200_PP_CNTL_X, 0);
	OUT_ACCEL_REG(R200_PP_TXMULTI_CTL_0, 0);
	OUT_ACCEL_REG(R200_SE_VTX_STATE_CNTL, 0);
	OUT_ACCEL_REG(R200_RE_CNTL, 0x0);
	OUT_ACCEL_REG(R200_SE_VTE_CNTL, 0);
	OUT_ACCEL_REG(R200_SE_VAP_CNTL, R200_VAP_FORCE_W_TO_ONE |
	    R200_VAP_VF_MAX_VTX_NUM);
	FINISH_ACCEL();

	BEGIN_ACCEL(5);
	OUT_ACCEL_REG(RADEON_RE_TOP_LEFT, 0);
	OUT_ACCEL_REG(RADEON_RE_WIDTH_HEIGHT, 0x07ff07ff);
	OUT_ACCEL_REG(RADEON_AUX_SC_CNTL, 0);
	OUT_ACCEL_REG(RADEON_RB3D_PLANEMASK, 0xffffffff);
	OUT_ACCEL_REG(RADEON_SE_CNTL, (RADEON_DIFFUSE_SHADE_GOURAUD |
				       RADEON_BFACE_SOLID |
				       RADEON_FFACE_SOLID |
				       RADEON_VTX_PIX_CENTER_OGL |
				       RADEON_ROUND_MODE_ROUND |
				       RADEON_ROUND_PREC_4TH_PIX));
	FINISH_ACCEL();
    } else {
	BEGIN_ACCEL(2);
	if ((info->ChipFamily == CHIP_FAMILY_RADEON) ||
	    (info->ChipFamily == CHIP_FAMILY_RV200))
	    OUT_ACCEL_REG(RADEON_SE_CNTL_STATUS, 0);
	else
	    OUT_ACCEL_REG(RADEON_SE_CNTL_STATUS, RADEON_TCL_BYPASS);
	OUT_ACCEL_REG(RADEON_SE_COORD_FMT,
	    RADEON_VTX_XY_PRE_MULT_1_OVER_W0 |
	    RADEON_VTX_ST0_NONPARAMETRIC |
	    RADEON_VTX_ST1_NONPARAMETRIC |
	    RADEON_TEX1_W_ROUTING_USE_W0);
	FINISH_ACCEL();

	BEGIN_ACCEL(5);
	OUT_ACCEL_REG(RADEON_RE_TOP_LEFT, 0);
	OUT_ACCEL_REG(RADEON_RE_WIDTH_HEIGHT, 0x07ff07ff);
	OUT_ACCEL_REG(RADEON_AUX_SC_CNTL, 0);
	OUT_ACCEL_REG(RADEON_RB3D_PLANEMASK, 0xffffffff);
	OUT_ACCEL_REG(RADEON_SE_CNTL, (RADEON_DIFFUSE_SHADE_GOURAUD |
				       RADEON_BFACE_SOLID |
				       RADEON_FFACE_SOLID |
				       RADEON_VTX_PIX_CENTER_OGL |
				       RADEON_ROUND_MODE_ROUND |
				       RADEON_ROUND_PREC_4TH_PIX));
	FINISH_ACCEL();
    }

}


/* MMIO:
 *
 * Wait for the graphics engine to be completely idle: the FIFO has
 * drained, the Pixel Cache is flushed, and the engine is idle.  This is
 * a standard "sync" function that will make the hardware "quiescent".
 *
 * CP:
 *
 * Wait until the CP is completely idle: the FIFO has drained and the CP
 * is idle.
 */
void FUNC_NAME(RADEONWaitForIdle)(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int            i    = 0;

#ifdef ACCEL_CP
    /* Make sure the CP is idle first */
    if (info->CPStarted) {
	int  ret;

	FLUSH_RING();

	for (;;) {
	    do {
		ret = drmCommandNone(info->drmFD, DRM_RADEON_CP_IDLE);
		if (ret && ret != -EBUSY) {
		    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			       "%s: CP idle %d\n", __FUNCTION__, ret);
		}
	    } while ((ret == -EBUSY) && (i++ < RADEON_TIMEOUT));

	    if (ret == 0) return;

	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Idle timed out, resetting engine...\n");
	    RADEONEngineReset(pScrn);
	    RADEONEngineRestore(pScrn);

	    /* Always restart the engine when doing CP 2D acceleration */
	    RADEONCP_RESET(pScrn, info);
	    RADEONCP_START(pScrn, info);
	}
    }
#endif

#if 0
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "WaitForIdle (entering): %d entries, stat=0x%08x\n",
		   INREG(RADEON_RBBM_STATUS) & RADEON_RBBM_FIFOCNT_MASK,
		   INREG(RADEON_RBBM_STATUS));
#endif

    /* Wait for the engine to go idle */
    RADEONWaitForFifoFunction(pScrn, 64);

    for (;;) {
	for (i = 0; i < RADEON_TIMEOUT; i++) {
	    if (!(INREG(RADEON_RBBM_STATUS) & RADEON_RBBM_ACTIVE)) {
		RADEONEngineFlush(pScrn);
		return;
	    }
	}
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		       "Idle timed out: %u entries, stat=0x%08x\n",
		       (unsigned int)INREG(RADEON_RBBM_STATUS) & RADEON_RBBM_FIFOCNT_MASK,
		       (unsigned int)INREG(RADEON_RBBM_STATUS));
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Idle timed out, resetting engine...\n");
	RADEONEngineReset(pScrn);
	RADEONEngineRestore(pScrn);
#ifdef XF86DRI
	if (info->directRenderingEnabled) {
	    RADEONCP_RESET(pScrn, info);
	    RADEONCP_START(pScrn, info);
	}
#endif
    }
}
