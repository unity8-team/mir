/*
 * Copyright 2008 Alex Deucher
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 * Based on radeon_exa_render.c and kdrive ati_video.c by Eric Anholt
 *
 */

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

#define VTX_DWORD_COUNT 4

#ifdef ACCEL_CP

#define VTX_OUT(_dstX, _dstY, _srcX, _srcY)	\
do {								\
    OUT_VIDEO_RING_F(_dstX);						\
    OUT_VIDEO_RING_F(_dstY);						\
    OUT_VIDEO_RING_F(_srcX);						\
    OUT_VIDEO_RING_F(_srcY);						\
} while (0)

#else /* ACCEL_CP */

#define VTX_OUT(_dstX, _dstY, _srcX, _srcY)	\
do {								\
    OUT_VIDEO_REG_F(RADEON_SE_PORT_DATA0, _dstX);		\
    OUT_VIDEO_REG_F(RADEON_SE_PORT_DATA0, _dstY);		\
    OUT_VIDEO_REG_F(RADEON_SE_PORT_DATA0, _srcX);		\
    OUT_VIDEO_REG_F(RADEON_SE_PORT_DATA0, _srcY);		\
} while (0)

#endif /* !ACCEL_CP */

static void
FUNC_NAME(RADEONDisplayTexturedVideo)(ScrnInfoPtr pScrn, RADEONPortPrivPtr pPriv)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    PixmapPtr pPixmap = pPriv->pPixmap;
    CARD32 txformat;
    CARD32 txfilter, txformat0, txformat1, txoffset, txpitch;
    CARD32 dst_offset, dst_pitch, dst_format;
    CARD32 txenable, colorpitch;
    CARD32 blendcntl;
    int dstxoff, dstyoff, pixel_shift;
    VIDEO_PREAMBLE();

    BoxPtr pBox = REGION_RECTS(&pPriv->clip);
    int nBox = REGION_NUM_RECTS(&pPriv->clip);

    pixel_shift = pPixmap->drawable.bitsPerPixel >> 4;

#ifdef USE_EXA
    if (info->useEXA) {
	dst_offset = exaGetPixmapOffset(pPixmap) + info->fbLocation;
	dst_pitch = exaGetPixmapPitch(pPixmap);
    } else
#endif
	{
	    dst_offset = (pPixmap->devPrivate.ptr - info->FB) +
		info->fbLocation + pScrn->fbOffset;
	    dst_pitch = pPixmap->devKind;
	}

#ifdef COMPOSITE
    dstxoff = -pPixmap->screen_x + pPixmap->drawable.x;
    dstyoff = -pPixmap->screen_y + pPixmap->drawable.y;
#else
    dstxoff = 0;
    dstyoff = 0;
#endif

#if 0
    ErrorF("dst_offset: 0x%x\n", dst_offset);
    ErrorF("dst_pitch: 0x%x\n", dst_pitch);
    ErrorF("dstxoff: 0x%x\n", dstxoff);
    ErrorF("dstyoff: 0x%x\n", dstyoff);
    ErrorF("src_offset: 0x%x\n", pPriv->src_offset);
    ErrorF("src_pitch: 0x%x\n", pPriv->src_pitch);
#endif

    if (!info->XInited3D)
	RADEONInit3DEngine(pScrn);

    /* we can probably improve this */
    BEGIN_VIDEO(2);
    OUT_VIDEO_REG(RADEON_RB3D_DSTCACHE_CTLSTAT, RADEON_RB3D_DC_FLUSH);
    /* We must wait for 3d to idle, in case source was just written as a dest. */
    OUT_VIDEO_REG(RADEON_WAIT_UNTIL,
		RADEON_WAIT_HOST_IDLECLEAN | RADEON_WAIT_2D_IDLECLEAN | RADEON_WAIT_3D_IDLECLEAN);
    FINISH_VIDEO();

    if (IS_R300_VARIANT) {

	switch (pPixmap->drawable.bitsPerPixel) {
	case 16:
	    if (pPixmap->drawable.depth == 15)
		dst_format = R300_COLORFORMAT_ARGB1555;
	    else
		dst_format = R300_COLORFORMAT_RGB565;
	    break;
	case 32:
	    dst_format = R300_COLORFORMAT_ARGB8888;
	    break;
	default:
	    return;
	}

	colorpitch = dst_pitch >> pixel_shift;
	colorpitch |= dst_format;

	if (RADEONTilingEnabled(pScrn, pPixmap))
	    colorpitch |= R300_COLORTILE;

	if (pPriv->id == FOURCC_UYVY)
	    txformat1 = R300_TX_FORMAT_YVYU422;
	else
	    txformat1 = R300_TX_FORMAT_VYUY422;

	txformat1 |= R300_TX_FORMAT_YUV_TO_RGB_CLAMP;

	txformat0 = (((RADEONPow2(pPriv->src_w) - 1) << R300_TXWIDTH_SHIFT) |
		     ((RADEONPow2(pPriv->src_h) - 1) << R300_TXHEIGHT_SHIFT));

	txformat0 |= R300_TXPITCH_EN;

	info->texW[0] = RADEONPow2(pPriv->src_w);
	info->texH[0] = RADEONPow2(pPriv->src_h);

	txfilter = (R300_TX_MAG_FILTER_LINEAR | R300_TX_MIN_FILTER_LINEAR);

	txpitch = pPriv->src_w * 4;
	txpitch >>= pixel_shift;
	txpitch -= 1;

	txoffset = pPriv->src_offset;

	BEGIN_VIDEO(6);
	OUT_VIDEO_REG(R300_TX_FILTER0_0, txfilter);
	OUT_VIDEO_REG(R300_TX_FILTER1_0, 0x0);
	OUT_VIDEO_REG(R300_TX_FORMAT0_0, txformat0);
	OUT_VIDEO_REG(R300_TX_FORMAT1_0, txformat1);
	OUT_VIDEO_REG(R300_TX_FORMAT2_0, txpitch);
	OUT_VIDEO_REG(R300_TX_OFFSET_0, txoffset);
	FINISH_VIDEO();

	txenable = R300_TEX_0_ENABLE;

	/* setup vertex shader */
	BEGIN_VIDEO(26);
	OUT_VIDEO_REG(R300_VAP_CNTL_STATUS, 0x0);
	OUT_VIDEO_REG(R300_VAP_PVS_STATE_FLUSH_REG, 0x0);
	OUT_VIDEO_REG(R300_VAP_CNTL, 0x300456);
	OUT_VIDEO_REG(R300_VAP_VTE_CNTL, 0x300);
	OUT_VIDEO_REG(R300_VAP_PSC_SGN_NORM_CNTL, 0x0);
	OUT_VIDEO_REG(R300_VAP_PROG_STREAM_CNTL_0, 0x6a014001);
	OUT_VIDEO_REG(R300_VAP_PROG_STREAM_CNTL_EXT_0, 0xf688f688);
	OUT_VIDEO_REG(R300_VAP_PVS_CODE_CNTL_0, 0x100400);
	OUT_VIDEO_REG(R300_VAP_PVS_CODE_CNTL_1, 0x1);
	OUT_VIDEO_REG(R300_VAP_PVS_VECTOR_INDX_REG, 0);
	OUT_VIDEO_REG(R300_VAP_PVS_VECTOR_DATA_REG,0x00f00203);
	OUT_VIDEO_REG(R300_VAP_PVS_VECTOR_DATA_REG,0x00d10001);
	OUT_VIDEO_REG(R300_VAP_PVS_VECTOR_DATA_REG,0x01248001);
	OUT_VIDEO_REG(R300_VAP_PVS_VECTOR_DATA_REG,0x01248001);
	OUT_VIDEO_REG(R300_VAP_PVS_VECTOR_DATA_REG,0x00f02203);
	OUT_VIDEO_REG(R300_VAP_PVS_VECTOR_DATA_REG,0x00d10141);
	OUT_VIDEO_REG(R300_VAP_PVS_VECTOR_DATA_REG,0x01248141);
	OUT_VIDEO_REG(R300_VAP_PVS_VECTOR_DATA_REG,0x01248141);

	OUT_VIDEO_REG(R300_VAP_PVS_FLOW_CNTL_OPC, 0x0);
	OUT_VIDEO_REG(R300_VAP_OUT_VTX_FMT_0, 0x1);
	OUT_VIDEO_REG(R300_VAP_OUT_VTX_FMT_1, 0x2);

	OUT_VIDEO_REG(R300_VAP_GB_VERT_CLIP_ADJ, 0x3f800000);
	OUT_VIDEO_REG(R300_VAP_GB_VERT_DISC_ADJ, 0x3f800000);
	OUT_VIDEO_REG(R300_VAP_GB_HORZ_CLIP_ADJ, 0x3f800000);
	OUT_VIDEO_REG(R300_VAP_GB_HORZ_DISC_ADJ, 0x3f800000);
	OUT_VIDEO_REG(R300_VAP_CLIP_CNTL, 0x10000);
	FINISH_VIDEO();

	/* setup pixel shader */
	BEGIN_VIDEO(12);
	OUT_VIDEO_REG(R300_US_CONFIG, 0x8);
	OUT_VIDEO_REG(R300_US_PIXSIZE, 0x0);
	OUT_VIDEO_REG(R300_US_CODE_OFFSET, 0x40040);
	OUT_VIDEO_REG(R300_US_CODE_ADDR_0, 0x0);
	OUT_VIDEO_REG(R300_US_CODE_ADDR_1, 0x0);
	OUT_VIDEO_REG(R300_US_CODE_ADDR_2, 0x0);
	OUT_VIDEO_REG(R300_US_CODE_ADDR_3, 0x400000);
	OUT_VIDEO_REG(R300_US_TEX_INST_0, 0x8000);
	OUT_VIDEO_REG(R300_US_ALU_RGB_ADDR_0, 0x1f800000);
	OUT_VIDEO_REG(R300_US_ALU_RGB_INST_0, 0x50a80);
	OUT_VIDEO_REG(R300_US_ALU_ALPHA_ADDR_0, 0x1800000);
	OUT_VIDEO_REG(R300_US_ALU_ALPHA_INST_0, 0x00040889);
	FINISH_VIDEO();

	BEGIN_VIDEO(6);
	OUT_VIDEO_REG(R300_TX_INVALTAGS, 0x0);
	OUT_VIDEO_REG(R300_TX_ENABLE, txenable);

	OUT_VIDEO_REG(R300_RB3D_COLOROFFSET0, dst_offset);
	OUT_VIDEO_REG(R300_RB3D_COLORPITCH0, colorpitch);

	blendcntl = RADEON_SRC_BLEND_GL_ONE | RADEON_DST_BLEND_GL_ZERO;
	OUT_VIDEO_REG(R300_RB3D_BLENDCNTL, blendcntl);
	OUT_VIDEO_REG(R300_RB3D_ABLENDCNTL, 0x0);
	FINISH_VIDEO();

	BEGIN_VIDEO(1);
	OUT_VIDEO_REG(R300_VAP_VTX_SIZE, VTX_DWORD_COUNT);
	FINISH_VIDEO();

    } else {

	/* Same for R100/R200 */
	switch (pPixmap->drawable.bitsPerPixel) {
	case 16:
	    if (pPixmap->drawable.depth == 15)
		dst_format = RADEON_COLOR_FORMAT_ARGB1555;
	    else
		dst_format = RADEON_COLOR_FORMAT_RGB565;
	    break;
	case 32:
	    dst_format = RADEON_COLOR_FORMAT_ARGB8888;
	    break;
	default:
	    return;
	}

	if (pPriv->id == FOURCC_UYVY)
	    txformat = RADEON_TXFORMAT_YVYU422;
	else
	    txformat = RADEON_TXFORMAT_VYUY422;

	txformat |= RADEON_TXFORMAT_NON_POWER2;

	colorpitch = dst_pitch >> pixel_shift;

	if (RADEONTilingEnabled(pScrn, pPixmap))
	    colorpitch |= RADEON_COLOR_TILE_ENABLE;

	BEGIN_VIDEO(5);

	OUT_VIDEO_REG(RADEON_PP_CNTL,
		    RADEON_TEX_0_ENABLE | RADEON_TEX_BLEND_0_ENABLE);
	OUT_VIDEO_REG(RADEON_RB3D_CNTL,
		    dst_format | RADEON_ALPHA_BLEND_ENABLE);
	OUT_VIDEO_REG(RADEON_RB3D_COLOROFFSET, dst_offset);

	OUT_VIDEO_REG(RADEON_RB3D_COLORPITCH, colorpitch);

	OUT_VIDEO_REG(RADEON_RB3D_BLENDCNTL,
		    RADEON_SRC_BLEND_GL_ONE | RADEON_DST_BLEND_GL_ZERO);

	FINISH_VIDEO();


	if ((info->ChipFamily == CHIP_FAMILY_RV250) ||
	    (info->ChipFamily == CHIP_FAMILY_RV280) ||
	    (info->ChipFamily == CHIP_FAMILY_RS300) ||
	    (info->ChipFamily == CHIP_FAMILY_R200)) {

	    info->texW[0] = pPriv->src_w;
	    info->texH[0] = pPriv->src_h;

	    BEGIN_VIDEO(12);

	    OUT_VIDEO_REG(R200_SE_VTX_FMT_0, R200_VTX_XY);
	    OUT_VIDEO_REG(R200_SE_VTX_FMT_1,
			(2 << R200_VTX_TEX0_COMP_CNT_SHIFT));

	    OUT_VIDEO_REG(R200_PP_TXFILTER_0,
			R200_MAG_FILTER_LINEAR |
			R200_MIN_FILTER_LINEAR |
			R200_YUV_TO_RGB);
	    OUT_VIDEO_REG(R200_PP_TXFORMAT_0, txformat);
	    OUT_VIDEO_REG(R200_PP_TXFORMAT_X_0, 0);
	    OUT_VIDEO_REG(R200_PP_TXSIZE_0,
			(pPriv->src_w - 1) |
			((pPriv->src_h - 1) << RADEON_TEX_VSIZE_SHIFT));
	    OUT_VIDEO_REG(R200_PP_TXPITCH_0, pPriv->src_pitch - 32);

	    OUT_VIDEO_REG(R200_PP_TXOFFSET_0, pPriv->src_offset);

	    OUT_VIDEO_REG(R200_PP_TXCBLEND_0,
			R200_TXC_ARG_A_ZERO |
			R200_TXC_ARG_B_ZERO |
			R200_TXC_ARG_C_R0_COLOR |
			R200_TXC_OP_MADD);
	    OUT_VIDEO_REG(R200_PP_TXCBLEND2_0,
			R200_TXC_CLAMP_0_1 | R200_TXC_OUTPUT_REG_R0);
	    OUT_VIDEO_REG(R200_PP_TXABLEND_0,
			R200_TXA_ARG_A_ZERO |
			R200_TXA_ARG_B_ZERO |
			R200_TXA_ARG_C_R0_ALPHA |
			R200_TXA_OP_MADD);
	    OUT_VIDEO_REG(R200_PP_TXABLEND2_0,
			R200_TXA_CLAMP_0_1 | R200_TXA_OUTPUT_REG_R0);
	    FINISH_VIDEO();
	} else {

	    info->texW[0] = 1;
	    info->texH[0] = 1;

	    BEGIN_VIDEO(8);

	    OUT_VIDEO_REG(RADEON_SE_VTX_FMT, RADEON_SE_VTX_FMT_XY |
			RADEON_SE_VTX_FMT_ST0);

	    OUT_VIDEO_REG(RADEON_PP_TXFILTER_0, RADEON_MAG_FILTER_LINEAR |
			RADEON_MIN_FILTER_LINEAR |
			RADEON_YUV_TO_RGB);
	    OUT_VIDEO_REG(RADEON_PP_TXFORMAT_0, txformat);
	    OUT_VIDEO_REG(RADEON_PP_TXOFFSET_0, pPriv->src_offset);
	    OUT_VIDEO_REG(RADEON_PP_TXCBLEND_0,
			RADEON_COLOR_ARG_A_ZERO |
			RADEON_COLOR_ARG_B_ZERO |
			RADEON_COLOR_ARG_C_T0_COLOR |
			RADEON_BLEND_CTL_ADD |
			RADEON_CLAMP_TX);
	    OUT_VIDEO_REG(RADEON_PP_TXABLEND_0,
			RADEON_ALPHA_ARG_A_ZERO |
			RADEON_ALPHA_ARG_B_ZERO |
			RADEON_ALPHA_ARG_C_T0_ALPHA |
			RADEON_BLEND_CTL_ADD |
			RADEON_CLAMP_TX);

	    OUT_VIDEO_REG(RADEON_PP_TEX_SIZE_0,
			(pPriv->src_w - 1) |
			((pPriv->src_h - 1) << RADEON_TEX_VSIZE_SHIFT));
	    OUT_VIDEO_REG(RADEON_PP_TEX_PITCH_0,
			pPriv->src_pitch - 32);
	    FINISH_VIDEO();
	}
    }

    while (nBox--) {
	int srcX, srcY, srcw, srch;
	int dstX, dstY, dstw, dsth;
	xPointFixed srcTopLeft, srcTopRight, srcBottomLeft, srcBottomRight;
	dstX = pBox->x1 + dstxoff;
	dstY = pBox->y1 + dstyoff;
	dstw = pBox->x2 - pBox->x1;
	dsth = pBox->y2 - pBox->y1;
	srcX = (pBox->x1 - pPriv->dst_x1) *
	    pPriv->src_w / pPriv->dst_w;
	srcY = (pBox->y1 - pPriv->dst_y1) *
	    pPriv->src_h / pPriv->dst_h;

	srcw = (pPriv->src_w * dstw) / pPriv->dst_w;
	srch = (pPriv->src_h * dsth) / pPriv->dst_h;

	srcTopLeft.x     = IntToxFixed(srcX);
	srcTopLeft.y     = IntToxFixed(srcY);
	srcTopRight.x    = IntToxFixed(srcX + srcw);
	srcTopRight.y    = IntToxFixed(srcY);
	srcBottomLeft.x  = IntToxFixed(srcX);
	srcBottomLeft.y  = IntToxFixed(srcY + srch);
	srcBottomRight.x = IntToxFixed(srcX + srcw);
	srcBottomRight.y = IntToxFixed(srcY + srch);

#if 0
	ErrorF("dst: %d, %d, %d, %d\n", dstX, dstY, dstw, dsth);
	ErrorF("src: %d, %d, %d, %d\n", srcX, srcY, srcw, srch);
#endif

#ifdef ACCEL_CP
	if (info->ChipFamily < CHIP_FAMILY_R200) {
	    BEGIN_RING(4 * VTX_DWORD_COUNT + 3);
	    OUT_RING(CP_PACKET3(RADEON_CP_PACKET3_3D_DRAW_IMMD,
				4 * VTX_DWORD_COUNT + 1));
	    OUT_RING(RADEON_CP_VC_FRMT_XY |
		     RADEON_CP_VC_FRMT_ST0);
	    OUT_RING(RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_FAN |
		     RADEON_CP_VC_CNTL_PRIM_WALK_RING |
		     RADEON_CP_VC_CNTL_MAOS_ENABLE |
		     RADEON_CP_VC_CNTL_VTX_FMT_RADEON_MODE |
		     (4 << RADEON_CP_VC_CNTL_NUM_SHIFT));
	} else {
	    if (IS_R300_VARIANT)
		BEGIN_RING(4 * VTX_DWORD_COUNT + 6);
	    else
		BEGIN_RING(4 * VTX_DWORD_COUNT + 2);
	    OUT_RING(CP_PACKET3(R200_CP_PACKET3_3D_DRAW_IMMD_2,
				4 * VTX_DWORD_COUNT));
	    OUT_RING(RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_FAN |
		     RADEON_CP_VC_CNTL_PRIM_WALK_RING |
		     (4 << RADEON_CP_VC_CNTL_NUM_SHIFT));
	}
#else /* ACCEL_CP */
	if (IS_R300_VARIANT)
	    BEGIN_VIDEO(3 + VTX_DWORD_COUNT * 4);
	else
	    BEGIN_VIDEO(1 + VTX_DWORD_COUNT * 4);

	if (info->ChipFamily < CHIP_FAMILY_R200) {
	    OUT_VIDEO_REG(RADEON_SE_VF_CNTL, (RADEON_VF_PRIM_TYPE_TRIANGLE_FAN |
					      RADEON_VF_PRIM_WALK_DATA |
					      RADEON_VF_RADEON_MODE |
					      4 << RADEON_VF_NUM_VERTICES_SHIFT));
	} else {
	    OUT_VIDEO_REG(RADEON_SE_VF_CNTL, (RADEON_VF_PRIM_TYPE_QUAD_LIST |
					      RADEON_VF_PRIM_WALK_DATA |
					      4 << RADEON_VF_NUM_VERTICES_SHIFT));
	}
#endif

	VTX_OUT((float)dstX,                                      (float)dstY,
		xFixedToFloat(srcTopLeft.x) / info->texW[0],      xFixedToFloat(srcTopLeft.y) / info->texH[0]);
	VTX_OUT((float)dstX,                                      (float)(dstY + dsth),
		xFixedToFloat(srcBottomLeft.x) / info->texW[0],   xFixedToFloat(srcBottomLeft.y) / info->texH[0]);
	VTX_OUT((float)(dstX + dstw),                                (float)(dstY + dsth),
		xFixedToFloat(srcBottomRight.x) / info->texW[0],  xFixedToFloat(srcBottomRight.y) / info->texH[0]);
	VTX_OUT((float)(dstX + dstw),                                (float)dstY,
		xFixedToFloat(srcTopRight.x) / info->texW[0],     xFixedToFloat(srcTopRight.y) / info->texH[0]);

	if (IS_R300_VARIANT) {
	    OUT_VIDEO_REG(R300_RB3D_DSTCACHE_CTLSTAT, 0xA);
	    OUT_VIDEO_REG(RADEON_WAIT_UNTIL, RADEON_WAIT_3D_IDLECLEAN);
	}

#ifdef ACCEL_CP
	ADVANCE_RING();
#else
	FINISH_VIDEO();
#endif /* !ACCEL_CP */

	pBox++;
    }

    DamageDamageRegion(pPriv->pDraw, &pPriv->clip);
}

#undef VTX_OUT
#undef FUNC_NAME
