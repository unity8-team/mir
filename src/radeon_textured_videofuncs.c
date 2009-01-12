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
 * Based on radeon_exa_render.c and kdrive ati_video.c by Eric Anholt, et al.
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

#ifdef ACCEL_CP

#define VTX_OUT_FILTER(_dstX, _dstY, _srcX, _srcY, _maskX, _maskY)	\
do {									\
    OUT_RING_F(_dstX);						\
    OUT_RING_F(_dstY);						\
    OUT_RING_F(_srcX);						\
    OUT_RING_F(_srcY);						\
    OUT_RING_F(_maskX);						\
    OUT_RING_F(_maskY);						\
} while (0)

#define VTX_OUT(_dstX, _dstY, _srcX, _srcY)	\
do {								\
    OUT_RING_F(_dstX);						\
    OUT_RING_F(_dstY);						\
    OUT_RING_F(_srcX);						\
    OUT_RING_F(_srcY);						\
} while (0)

#else /* ACCEL_CP */

#define VTX_OUT_FILTER(_dstX, _dstY, _srcX, _srcY, _maskX, _maskY)	\
do {									\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _dstX);			\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _dstY);			\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _srcX);			\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _srcY);			\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _maskX);			\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _maskY);			\
} while (0)

#define VTX_OUT(_dstX, _dstY, _srcX, _srcY)	\
do {								\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _dstX);		\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _dstY);		\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _srcX);		\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _srcY);		\
} while (0)

#endif /* !ACCEL_CP */

static void
FUNC_NAME(RADEONDisplayTexturedVideo)(ScrnInfoPtr pScrn, RADEONPortPrivPtr pPriv)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    PixmapPtr pPixmap = pPriv->pPixmap;
    uint32_t txformat;
    uint32_t txfilter, txformat0, txformat1, txoffset, txpitch;
    uint32_t dst_offset, dst_pitch, dst_format;
    uint32_t txenable, colorpitch;
    uint32_t blendcntl;
    int dstxoff, dstyoff, pixel_shift, vtx_count;
    BoxPtr pBox = REGION_RECTS(&pPriv->clip);
    int nBox = REGION_NUM_RECTS(&pPriv->clip);
    ACCEL_PREAMBLE();

    pixel_shift = pPixmap->drawable.bitsPerPixel >> 4;

#ifdef USE_EXA
    if (info->useEXA) {
	dst_offset = exaGetPixmapOffset(pPixmap) + info->fbLocation + pScrn->fbOffset;
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

#ifdef USE_EXA
    if (info->useEXA) {
	RADEON_SWITCH_TO_3D();
    } else
#endif
	{
	    BEGIN_ACCEL(2);
	    if (IS_R300_3D || IS_R500_3D)
		OUT_ACCEL_REG(R300_RB3D_DSTCACHE_CTLSTAT, R300_DC_FLUSH_3D);
	    else
		OUT_ACCEL_REG(RADEON_RB3D_DSTCACHE_CTLSTAT, RADEON_RB3D_DC_FLUSH);
	    /* We must wait for 3d to idle, in case source was just written as a dest. */
	    OUT_ACCEL_REG(RADEON_WAIT_UNTIL,
			  RADEON_WAIT_HOST_IDLECLEAN |
			  RADEON_WAIT_2D_IDLECLEAN |
			  RADEON_WAIT_3D_IDLECLEAN |
			  RADEON_WAIT_DMA_GUI_IDLE);
	    FINISH_ACCEL();

	    if (!info->accel_state->XInited3D)
		RADEONInit3DEngine(pScrn);
	}

    if (pPriv->bicubic_enabled)
	vtx_count = 6;
    else
	vtx_count = 4;

    if (IS_R300_3D || IS_R500_3D) {
	uint32_t output_fmt;

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

	output_fmt = (R300_OUT_FMT_C4_8 |
		      R300_OUT_FMT_C0_SEL_BLUE |
		      R300_OUT_FMT_C1_SEL_GREEN |
		      R300_OUT_FMT_C2_SEL_RED |
		      R300_OUT_FMT_C3_SEL_ALPHA);

	colorpitch = dst_pitch >> pixel_shift;
	colorpitch |= dst_format;

	if (RADEONTilingEnabled(pScrn, pPixmap))
	    colorpitch |= R300_COLORTILE;

	if (pPriv->id == FOURCC_UYVY)
	    txformat1 = R300_TX_FORMAT_YVYU422;
	else
	    txformat1 = R300_TX_FORMAT_VYUY422;

	txformat1 |= R300_TX_FORMAT_YUV_TO_RGB_CLAMP;

	txformat0 = ((((pPriv->w - 1) & 0x7ff) << R300_TXWIDTH_SHIFT) |
		     (((pPriv->h - 1) & 0x7ff) << R300_TXHEIGHT_SHIFT) |
		     R300_TXPITCH_EN);

	info->accel_state->texW[0] = pPriv->w;
	info->accel_state->texH[0] = pPriv->h;

	txfilter = (R300_TX_CLAMP_S(R300_TX_CLAMP_CLAMP_LAST) |
		    R300_TX_CLAMP_T(R300_TX_CLAMP_CLAMP_LAST) |
		    R300_TX_MAG_FILTER_LINEAR |
		    R300_TX_MIN_FILTER_LINEAR |
		    (0 << R300_TX_ID_SHIFT));

	/* pitch is in pixels */
	txpitch = pPriv->src_pitch / 2;
	txpitch -= 1;

	if (IS_R500_3D && ((pPriv->w - 1) & 0x800))
	    txpitch |= R500_TXWIDTH_11;

	if (IS_R500_3D && ((pPriv->h - 1) & 0x800))
	    txpitch |= R500_TXHEIGHT_11;

	txoffset = pPriv->src_offset;

	BEGIN_ACCEL(6);
	OUT_ACCEL_REG(R300_TX_FILTER0_0, txfilter);
	OUT_ACCEL_REG(R300_TX_FILTER1_0, 0);
	OUT_ACCEL_REG(R300_TX_FORMAT0_0, txformat0);
	OUT_ACCEL_REG(R300_TX_FORMAT1_0, txformat1);
	OUT_ACCEL_REG(R300_TX_FORMAT2_0, txpitch);
	OUT_ACCEL_REG(R300_TX_OFFSET_0, txoffset);
	FINISH_ACCEL();

	txenable = R300_TEX_0_ENABLE;

	if (pPriv->bicubic_enabled) {
		/* Size is 128x1 */
		txformat0 = ((0x7f << R300_TXWIDTH_SHIFT) |
			     (0x0 << R300_TXHEIGHT_SHIFT) |
			     R300_TXPITCH_EN);
		/* Format is 32-bit floats, 4bpp */
		txformat1 = R300_EASY_TX_FORMAT(Z, Y, X, W, FL_R16G16B16A16);
		/* Pitch is 127 (128-1) */
		txpitch = 0x7f;
		/* Tex filter */
		txfilter = (R300_TX_CLAMP_S(R300_TX_CLAMP_WRAP) |
			    R300_TX_CLAMP_T(R300_TX_CLAMP_WRAP) |
			    R300_TX_MIN_FILTER_NEAREST |
			    R300_TX_MAG_FILTER_NEAREST |
			    (1 << R300_TX_ID_SHIFT));

		BEGIN_ACCEL(6);
		OUT_ACCEL_REG(R300_TX_FILTER0_1, txfilter);
		OUT_ACCEL_REG(R300_TX_FILTER1_1, 0);
		OUT_ACCEL_REG(R300_TX_FORMAT0_1, txformat0);
		OUT_ACCEL_REG(R300_TX_FORMAT1_1, txformat1);
		OUT_ACCEL_REG(R300_TX_FORMAT2_1, txpitch);
		OUT_ACCEL_REG(R300_TX_OFFSET_1, pPriv->bicubic_src_offset);
		FINISH_ACCEL();

		/* Enable tex 1 */
		txenable |= R300_TEX_1_ENABLE;
	}

	/* setup the VAP */
	if (info->accel_state->has_tcl) {
	    if (pPriv->bicubic_enabled)
		BEGIN_ACCEL(7);
	    else
		BEGIN_ACCEL(6);
	} else {
	    if (pPriv->bicubic_enabled)
		BEGIN_ACCEL(5);
	    else
		BEGIN_ACCEL(4);
	}

	/* These registers define the number, type, and location of data submitted
	 * to the PVS unit of GA input (when PVS is disabled)
	 * DST_VEC_LOC is the slot in the PVS input vector memory when PVS/TCL is
	 * enabled.  This memory provides the imputs to the vertex shader program
	 * and ordering is not important.  When PVS/TCL is disabled, this field maps
	 * directly to the GA input memory and the order is signifigant.  In
	 * PVS_BYPASS mode the order is as follows:
	 * Position
	 * Point Size
	 * Color 0-3
	 * Textures 0-7
	 * Fog
	 */
	if (pPriv->bicubic_enabled) {
	    OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_0,
			  ((R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_0_SHIFT) |
			   (0 << R300_SKIP_DWORDS_0_SHIFT) |
			   (0 << R300_DST_VEC_LOC_0_SHIFT) |
			   R300_SIGNED_0 |
			   (R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_1_SHIFT) |
			   (0 << R300_SKIP_DWORDS_1_SHIFT) |
			   (6 << R300_DST_VEC_LOC_1_SHIFT) |
			   R300_SIGNED_1));
	    OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_1,
			  ((R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_2_SHIFT) |
			   (0 << R300_SKIP_DWORDS_2_SHIFT) |
			   (7 << R300_DST_VEC_LOC_2_SHIFT) |
			   R300_LAST_VEC_2 |
			   R300_SIGNED_2));
	} else {
	    OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_0,
			  ((R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_0_SHIFT) |
			   (0 << R300_SKIP_DWORDS_0_SHIFT) |
			   (0 << R300_DST_VEC_LOC_0_SHIFT) |
			   R300_SIGNED_0 |
			   (R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_1_SHIFT) |
			   (0 << R300_SKIP_DWORDS_1_SHIFT) |
			   (6 << R300_DST_VEC_LOC_1_SHIFT) |
			   R300_LAST_VEC_1 |
			   R300_SIGNED_1));
	}

	/* load the vertex shader
	 * We pre-load vertex programs in RADEONInit3DEngine():
	 * - exa mask/Xv bicubic
	 * - exa no mask
	 * - Xv
	 * Here we select the offset of the vertex program we want to use
	 */
	if (info->accel_state->has_tcl) {
	    if (pPriv->bicubic_enabled) {
		OUT_ACCEL_REG(R300_VAP_PVS_CODE_CNTL_0,
			      ((0 << R300_PVS_FIRST_INST_SHIFT) |
			       (2 << R300_PVS_XYZW_VALID_INST_SHIFT) |
			       (2 << R300_PVS_LAST_INST_SHIFT)));
		OUT_ACCEL_REG(R300_VAP_PVS_CODE_CNTL_1,
			      (2 << R300_PVS_LAST_VTX_SRC_INST_SHIFT));
	    } else {
		OUT_ACCEL_REG(R300_VAP_PVS_CODE_CNTL_0,
			      ((5 << R300_PVS_FIRST_INST_SHIFT) |
			       (6 << R300_PVS_XYZW_VALID_INST_SHIFT) |
			       (6 << R300_PVS_LAST_INST_SHIFT)));
		OUT_ACCEL_REG(R300_VAP_PVS_CODE_CNTL_1,
			      (6 << R300_PVS_LAST_VTX_SRC_INST_SHIFT));
	    }
	}

	/* Position and one set of 2 texture coordinates */
	OUT_ACCEL_REG(R300_VAP_OUT_VTX_FMT_0, R300_VTX_POS_PRESENT);
	if (pPriv->bicubic_enabled)
	    OUT_ACCEL_REG(R300_VAP_OUT_VTX_FMT_1, ((2 << R300_TEX_0_COMP_CNT_SHIFT) |
						   (2 << R300_TEX_1_COMP_CNT_SHIFT)));
	else
	    OUT_ACCEL_REG(R300_VAP_OUT_VTX_FMT_1, (2 << R300_TEX_0_COMP_CNT_SHIFT));

	OUT_ACCEL_REG(R300_US_OUT_FMT_0, output_fmt);
	FINISH_ACCEL();

	/* setup pixel shader */
	if (IS_R300_3D) {
	    if (pPriv->bicubic_enabled) {
		BEGIN_ACCEL(79);

		/* 4 components: 2 for tex0 and 2 for tex1 */
		OUT_ACCEL_REG(R300_RS_COUNT, ((4 << R300_RS_COUNT_IT_COUNT_SHIFT) |
						   R300_RS_COUNT_HIRES_EN));

		/* R300_INST_COUNT_RS - highest RS instruction used */
		OUT_ACCEL_REG(R300_RS_INST_COUNT, R300_INST_COUNT_RS(1) | R300_TX_OFFSET_RS(6));

		/* Pixel stack frame size. */
		OUT_ACCEL_REG(R300_US_PIXSIZE, 5);

		/* Indirection levels */
		OUT_ACCEL_REG(R300_US_CONFIG, ((2 << R300_NLEVEL_SHIFT) |
							R300_FIRST_TEX));

		/* Set nodes. */
		OUT_ACCEL_REG(R300_US_CODE_OFFSET, (R300_ALU_CODE_OFFSET(0) |
							R300_ALU_CODE_SIZE(14) |
							R300_TEX_CODE_OFFSET(0) |
							R300_TEX_CODE_SIZE(6)));

		/* Nodes are allocated highest first, but executed lowest first */
		OUT_ACCEL_REG(R300_US_CODE_ADDR_0, 0);
		OUT_ACCEL_REG(R300_US_CODE_ADDR_1, (R300_ALU_START(0) |
							R300_ALU_SIZE(0) |
							R300_TEX_START(0) |
							R300_TEX_SIZE(0)));
		OUT_ACCEL_REG(R300_US_CODE_ADDR_2, (R300_ALU_START(1) |
							R300_ALU_SIZE(9) |
							R300_TEX_START(1) |
							R300_TEX_SIZE(0)));
		OUT_ACCEL_REG(R300_US_CODE_ADDR_3, (R300_ALU_START(11) |
							R300_ALU_SIZE(2) |
							R300_TEX_START(2) |
							R300_TEX_SIZE(3) |
							R300_RGBA_OUT));

		/* ** BICUBIC FP ** */

		/* texcoord0 => temp0
		 * texcoord1 => temp1 */

		// first node
		/* TEX temp2, temp1.rrr0, tex1, 1D */
		OUT_ACCEL_REG(R300_US_TEX_INST(0), (R300_TEX_INST(R300_TEX_INST_LD) |
						   R300_TEX_ID(1) |
						   R300_TEX_SRC_ADDR(1) |
						   R300_TEX_DST_ADDR(2)));

		/* MOV temp1.r, temp1.ggg0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(0), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_GGG) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_1_0) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(0), (R300_ALU_RGB_ADDR0(1) |
						   R300_ALU_RGB_ADDRD(1) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(0), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(0), (R300_ALU_ALPHA_ADDRD(1) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));


		// second node
		/* TEX temp1, temp1, tex1, 1D */
		OUT_ACCEL_REG(R300_US_TEX_INST(1), (R300_TEX_INST(R300_TEX_INST_LD) |
						   R300_TEX_ID(1) |
						   R300_TEX_SRC_ADDR(1) |
						   R300_TEX_DST_ADDR(1)));

		/* MUL temp3.rg, temp2.ggg0, const0.rgb0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(1), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_GGG) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_SRC1_RGB) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(1), (R300_ALU_RGB_ADDR0(2) |
						   R300_ALU_RGB_ADDR1(R300_ALU_RGB_CONST(0)) |
						   R300_ALU_RGB_ADDRD(3) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R | R300_ALU_RGB_MASK_G)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(1), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(1), (R300_ALU_ALPHA_ADDRD(3) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));


		/* MUL temp2.rg, temp2.rrr0, const0.rgb */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(2), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_RRR) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_SRC1_RGB) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(2), (R300_ALU_RGB_ADDR0(2) |
						   R300_ALU_RGB_ADDR1(R300_ALU_RGB_CONST(0)) |
						   R300_ALU_RGB_ADDRD(2) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R | R300_ALU_RGB_MASK_G)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(2), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(2), (R300_ALU_ALPHA_ADDRD(2) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));

		/* MAD temp4.rg, temp1.ggg0, const1.rgb, temp3.rgb0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(3), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_GGG) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_SRC1_RGB) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC2_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(3), (R300_ALU_RGB_ADDR0(1) |
						   R300_ALU_RGB_ADDR1(R300_ALU_RGB_CONST(1)) |
						   R300_ALU_RGB_ADDR2(3) |
						   R300_ALU_RGB_ADDRD(4) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R | R300_ALU_RGB_MASK_G)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(3), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(3), (R300_ALU_ALPHA_ADDRD(4) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));

		/* MAD temp5.rg, temp1.ggg0, const1.rgb, temp2.rgb0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(4), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_GGG) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_SRC1_RGB) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC2_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(4), (R300_ALU_RGB_ADDR0(1) |
						   R300_ALU_RGB_ADDR1(R300_ALU_RGB_CONST(1)) |
						   R300_ALU_RGB_ADDR2(2) |
						   R300_ALU_RGB_ADDRD(5) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R | R300_ALU_RGB_MASK_G)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(4), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(4), (R300_ALU_ALPHA_ADDRD(5) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));

		/* MAD temp3.rg, temp1.rrr0, const1.rgb, temp3.rgb0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(5), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_RRR) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_SRC1_RGB) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC2_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(5), (R300_ALU_RGB_ADDR0(1) |
						   R300_ALU_RGB_ADDR1(R300_ALU_RGB_CONST(1)) |
						   R300_ALU_RGB_ADDR2(3) |
						   R300_ALU_RGB_ADDRD(3) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R | R300_ALU_RGB_MASK_G)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(5), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(5), (R300_ALU_ALPHA_ADDRD(3) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));

		/* MAD temp1.rg, temp1.rrr0, const1.rgb, temp2.rgb0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(6), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_RRR) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_SRC1_RGB) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC2_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(6), (R300_ALU_RGB_ADDR0(1) |
						   R300_ALU_RGB_ADDR1(R300_ALU_RGB_CONST(1)) |
						   R300_ALU_RGB_ADDR2(2) |
						   R300_ALU_RGB_ADDRD(1) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R | R300_ALU_RGB_MASK_G)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(6), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(6), (R300_ALU_ALPHA_ADDRD(1) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));

		/* ADD temp1.rg, temp0.rgb0, temp1.rgb0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(7), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_RGB) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_1_0) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC2_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(7), (R300_ALU_RGB_ADDR0(0) |
						   R300_ALU_RGB_ADDR2(1) |
						   R300_ALU_RGB_ADDRD(1) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R | R300_ALU_RGB_MASK_G)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(7), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(7), (R300_ALU_ALPHA_ADDRD(1) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));

		/* ADD temp2.rg, temp0.rgb0, temp3.rgb0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(8), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_RGB) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_1_0) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC2_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(8), (R300_ALU_RGB_ADDR0(0) |
						   R300_ALU_RGB_ADDR2(3) |
						   R300_ALU_RGB_ADDRD(2) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R | R300_ALU_RGB_MASK_G)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(8), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(8), (R300_ALU_ALPHA_ADDRD(2) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));

		/* ADD temp3.rg, temp0.rgb0, temp5.rgb0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(9), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_RGB) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_1_0) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC2_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(9), (R300_ALU_RGB_ADDR0(0) |
						   R300_ALU_RGB_ADDR2(5) |
						   R300_ALU_RGB_ADDRD(3) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R | R300_ALU_RGB_MASK_G)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(9), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(9), (R300_ALU_ALPHA_ADDRD(3) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));

		/* ADD temp0.rg, temp0.rgb0, temp4.rgb0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(10), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_RGB) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_1_0) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC2_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(10), (R300_ALU_RGB_ADDR0(0) |
						   R300_ALU_RGB_ADDR2(4) |
						   R300_ALU_RGB_ADDRD(0) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_R | R300_ALU_RGB_MASK_G)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(10), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(10), (R300_ALU_ALPHA_ADDRD(0) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_NONE)));


		// third node
		/* TEX temp4, temp1.rg--, tex0, 1D */
		OUT_ACCEL_REG(R300_US_TEX_INST(2), (R300_TEX_INST(R300_TEX_INST_LD) |
						   R300_TEX_ID(0) |
						   R300_TEX_SRC_ADDR(1) |
						   R300_TEX_DST_ADDR(4)));

		/* TEX temp3, temp3.rg--, tex0, 1D */
		OUT_ACCEL_REG(R300_US_TEX_INST(3), (R300_TEX_INST(R300_TEX_INST_LD) |
						   R300_TEX_ID(0) |
						   R300_TEX_SRC_ADDR(3) |
						   R300_TEX_DST_ADDR(3)));

		/* TEX temp5, temp2.rg--, tex0, 1D */
		OUT_ACCEL_REG(R300_US_TEX_INST(4), (R300_TEX_INST(R300_TEX_INST_LD) |
						   R300_TEX_ID(0) |
						   R300_TEX_SRC_ADDR(2) |
						   R300_TEX_DST_ADDR(5)));

		/* TEX temp0, temp0.rg--, tex0, 1D */
		OUT_ACCEL_REG(R300_US_TEX_INST(5), (R300_TEX_INST(R300_TEX_INST_LD) |
						   R300_TEX_ID(0) |
						   R300_TEX_SRC_ADDR(0) |
						   R300_TEX_DST_ADDR(0)));

		/* LRP temp3, temp1.bbbb, temp4, temp3 ->
		 * - PRESUB temps, temp4 - temp3
		 * - MAD temp3, temp1.bbbb, temps, temp3 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(11), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC2_BBB) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_SRCP_RGB) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC0_RGB) |
						   R300_ALU_RGB_SRCP_OP(R300_ALU_RGB_SRCP_OP_RGB1_MINUS_RGB0)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(11), (R300_ALU_RGB_ADDR0(3) |
						   R300_ALU_RGB_ADDR1(4) |
						   R300_ALU_RGB_ADDR2(1) |
						   R300_ALU_RGB_ADDRD(3) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(11), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_SRC2_B) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_SRCP_A) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_SRC0_A)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(11), (R300_ALU_ALPHA_ADDR0(3) |
						   R300_ALU_ALPHA_ADDR1(4) |
						   R300_ALU_ALPHA_ADDR2(1) |
						   R300_ALU_ALPHA_ADDRD(3) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_A)));

		/* LRP temp0, temp1.bbbb, temp5, temp0 ->
		 * - PRESUB temps, temp5 - temp0
		 * - MAD temp0, temp1.bbbb, temps, temp0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(12), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC2_BBB) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_SRCP_RGB) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC0_RGB) |
						   R300_ALU_RGB_SRCP_OP(R300_ALU_RGB_SRCP_OP_RGB1_MINUS_RGB0) |
						   R300_ALU_RGB_INSERT_NOP));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(12), (R300_ALU_RGB_ADDR0(0) |
						   R300_ALU_RGB_ADDR1(5) |
						   R300_ALU_RGB_ADDR2(1) |
						   R300_ALU_RGB_ADDRD(0) |
						   R300_ALU_RGB_WMASK(R300_ALU_RGB_MASK_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(12), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_SRC2_B) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_SRCP_A) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_SRC0_A)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(12), (R300_ALU_ALPHA_ADDR0(0) |
						   R300_ALU_ALPHA_ADDR1(5) |
						   R300_ALU_ALPHA_ADDR2(1) |
						   R300_ALU_ALPHA_ADDRD(0) |
						   R300_ALU_ALPHA_WMASK(R300_ALU_ALPHA_MASK_A)));

		/* LRP output, temp2.bbbb, temp3, temp0 ->
		 * - PRESUB temps, temp3 - temp0
		 * - MAD output, temp2.bbbb, temps, temp0 */
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST(13), (R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC2_BBB) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_SRCP_RGB) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_SRC0_RGB) |
						   R300_ALU_RGB_SRCP_OP(R300_ALU_RGB_SRCP_OP_RGB1_MINUS_RGB0)));
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR(13), (R300_ALU_RGB_ADDR0(0) |
						   R300_ALU_RGB_ADDR1(3) |
						   R300_ALU_RGB_ADDR2(2) |
						   R300_ALU_RGB_OMASK(R300_ALU_RGB_MASK_RGB)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST(13), (R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_SRC2_B) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_SRCP_A) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_SRC0_A)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR(13), (R300_ALU_ALPHA_ADDR0(0) |
						   R300_ALU_ALPHA_ADDR1(3) |
						   R300_ALU_ALPHA_ADDR2(2) |
						   R300_ALU_ALPHA_OMASK(R300_ALU_ALPHA_MASK_A)));

		/* Shader constants. */
		OUT_ACCEL_REG(R300_US_ALU_CONST_R(0), F_TO_24(1.0/(float)pPriv->w));
		OUT_ACCEL_REG(R300_US_ALU_CONST_G(0), 0);
		OUT_ACCEL_REG(R300_US_ALU_CONST_B(0), 0);
		OUT_ACCEL_REG(R300_US_ALU_CONST_A(0), 0);

		OUT_ACCEL_REG(R300_US_ALU_CONST_R(1), 0);
		OUT_ACCEL_REG(R300_US_ALU_CONST_G(1), F_TO_24(1.0/(float)pPriv->h));
		OUT_ACCEL_REG(R300_US_ALU_CONST_B(1), 0);
		OUT_ACCEL_REG(R300_US_ALU_CONST_A(1), 0);

		FINISH_ACCEL();
	    } else {
		BEGIN_ACCEL(11);
		/* 2 components: 2 for tex0 */
		OUT_ACCEL_REG(R300_RS_COUNT,
			  ((2 << R300_RS_COUNT_IT_COUNT_SHIFT) |
			   R300_RS_COUNT_HIRES_EN));
		/* R300_INST_COUNT_RS - highest RS instruction used */
		OUT_ACCEL_REG(R300_RS_INST_COUNT, R300_INST_COUNT_RS(0) | R300_TX_OFFSET_RS(6));

		OUT_ACCEL_REG(R300_US_PIXSIZE, 0); /* highest temp used */

		/* Indirection levels */
		OUT_ACCEL_REG(R300_US_CONFIG, ((0 << R300_NLEVEL_SHIFT) |
							R300_FIRST_TEX));

		OUT_ACCEL_REG(R300_US_CODE_OFFSET, (R300_ALU_CODE_OFFSET(0) |
						   R300_ALU_CODE_SIZE(1) |
						   R300_TEX_CODE_OFFSET(0) |
						   R300_TEX_CODE_SIZE(1)));

		OUT_ACCEL_REG(R300_US_CODE_ADDR_3, (R300_ALU_START(0) |
						   R300_ALU_SIZE(0) |
						   R300_TEX_START(0) |
						   R300_TEX_SIZE(0) |
						   R300_RGBA_OUT));

		/* tex inst */
		OUT_ACCEL_REG(R300_US_TEX_INST_0, (R300_TEX_SRC_ADDR(0) |
						  R300_TEX_DST_ADDR(0) |
						  R300_TEX_ID(0) |
						  R300_TEX_INST(R300_TEX_INST_LD)));

		/* ALU inst */
		/* RGB */
		OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR_0, (R300_ALU_RGB_ADDR0(0) |
						   R300_ALU_RGB_ADDR1(0) |
						   R300_ALU_RGB_ADDR2(0) |
						   R300_ALU_RGB_ADDRD(0) |
						   R300_ALU_RGB_OMASK((R300_ALU_RGB_MASK_R |
						   R300_ALU_RGB_MASK_G |
						   R300_ALU_RGB_MASK_B)) |
						   R300_ALU_RGB_TARGET_A));
		OUT_ACCEL_REG(R300_US_ALU_RGB_INST_0, (R300_ALU_RGB_SEL_A(R300_ALU_RGB_SRC0_RGB) |
						   R300_ALU_RGB_MOD_A(R300_ALU_RGB_MOD_NOP) |
						   R300_ALU_RGB_SEL_B(R300_ALU_RGB_1_0) |
						   R300_ALU_RGB_MOD_B(R300_ALU_RGB_MOD_NOP) |
						   R300_ALU_RGB_SEL_C(R300_ALU_RGB_0_0) |
						   R300_ALU_RGB_MOD_C(R300_ALU_RGB_MOD_NOP) |
						   R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
						   R300_ALU_RGB_OMOD(R300_ALU_RGB_OMOD_NONE) |
						   R300_ALU_RGB_CLAMP));
		/* Alpha */
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR_0, (R300_ALU_ALPHA_ADDR0(0) |
						   R300_ALU_ALPHA_ADDR1(0) |
						   R300_ALU_ALPHA_ADDR2(0) |
						   R300_ALU_ALPHA_ADDRD(0) |
						   R300_ALU_ALPHA_OMASK(R300_ALU_ALPHA_MASK_A) |
						   R300_ALU_ALPHA_TARGET_A |
						   R300_ALU_ALPHA_OMASK_W(R300_ALU_ALPHA_MASK_NONE)));
		OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST_0, (R300_ALU_ALPHA_SEL_A(R300_ALU_ALPHA_SRC0_A) |
						   R300_ALU_ALPHA_MOD_A(R300_ALU_ALPHA_MOD_NOP) |
						   R300_ALU_ALPHA_SEL_B(R300_ALU_ALPHA_1_0) |
						   R300_ALU_ALPHA_MOD_B(R300_ALU_ALPHA_MOD_NOP) |
						   R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0) |
						   R300_ALU_ALPHA_MOD_C(R300_ALU_ALPHA_MOD_NOP) |
						   R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
						   R300_ALU_ALPHA_OMOD(R300_ALU_ALPHA_OMOD_NONE) |
						   R300_ALU_ALPHA_CLAMP));
		FINISH_ACCEL();
		}
	} else {
	    if (pPriv->bicubic_enabled) {
		BEGIN_ACCEL(7);

		/* 4 components: 2 for tex0 and 2 for tex1 */
		OUT_ACCEL_REG(R300_RS_COUNT,
			      ((4 << R300_RS_COUNT_IT_COUNT_SHIFT) |
			       R300_RS_COUNT_HIRES_EN));

		/* R300_INST_COUNT_RS - highest RS instruction used */
		OUT_ACCEL_REG(R300_RS_INST_COUNT, R300_INST_COUNT_RS(1) | R300_TX_OFFSET_RS(6));

		/* Pixel stack frame size. */
		OUT_ACCEL_REG(R300_US_PIXSIZE, 5);

		/* FP length. */
		OUT_ACCEL_REG(R500_US_CODE_ADDR, (R500_US_CODE_START_ADDR(0) |
						  R500_US_CODE_END_ADDR(13)));
		OUT_ACCEL_REG(R500_US_CODE_RANGE, (R500_US_CODE_RANGE_ADDR(0) |
						   R500_US_CODE_RANGE_SIZE(13)));

		/* Prepare for FP emission. */
		OUT_ACCEL_REG(R500_US_CODE_OFFSET, 0);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_INDEX, R500_US_VECTOR_INST_INDEX(0));
		FINISH_ACCEL();

		BEGIN_ACCEL(89);
		/* Pixel shader.
		 * I've gone ahead and annotated each instruction, since this
		 * thing is MASSIVE. :3
		 * Note: In order to avoid buggies with temps and multiple
		 * inputs, all temps are offset by 2. temp0 -> register2. */

		/* TEX temp2, input1.xxxx, tex1, 1D */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_TEX |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_ID(1) |
						       R500_TEX_INST_LD |
						       R500_TEX_IGNORE_UNCOVERED));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_SRC_ADDR(1) |
						       R500_TEX_SRC_S_SWIZ_R |
						       R500_TEX_SRC_T_SWIZ_R |
						       R500_TEX_SRC_R_SWIZ_R |
						       R500_TEX_SRC_Q_SWIZ_R |
						       R500_TEX_DST_ADDR(2) |
						       R500_TEX_DST_R_SWIZ_R |
						       R500_TEX_DST_G_SWIZ_G |
						       R500_TEX_DST_B_SWIZ_B |
						       R500_TEX_DST_A_SWIZ_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);

		/* TEX temp5, input1.yyyy, tex1, 1D */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_TEX |
						       R500_INST_TEX_SEM_WAIT |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_ID(1) |
						       R500_TEX_INST_LD |
						       R500_TEX_SEM_ACQUIRE |
						       R500_TEX_IGNORE_UNCOVERED));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_SRC_ADDR(1) |
						       R500_TEX_SRC_S_SWIZ_G |
						       R500_TEX_SRC_T_SWIZ_G |
						       R500_TEX_SRC_R_SWIZ_G |
						       R500_TEX_SRC_Q_SWIZ_G |
						       R500_TEX_DST_ADDR(5) |
						       R500_TEX_DST_R_SWIZ_R |
						       R500_TEX_DST_G_SWIZ_G |
						       R500_TEX_DST_B_SWIZ_B |
						       R500_TEX_DST_A_SWIZ_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);

		/* MUL temp4, const0.x0x0, temp2.yyxx */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_ALU |
						       R500_INST_TEX_SEM_WAIT |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_RGB_ADDR0(0) |
						       R500_RGB_ADDR0_CONST |
						       R500_RGB_ADDR1(2)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDR0(0) |
						       R500_ALPHA_ADDR0_CONST |
						       R500_ALPHA_ADDR1(2)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGB_SEL_A_SRC0 |
						       R500_ALU_RGB_R_SWIZ_A_R |
						       R500_ALU_RGB_G_SWIZ_A_0 |
						       R500_ALU_RGB_B_SWIZ_A_R |
						       R500_ALU_RGB_SEL_B_SRC1 |
						       R500_ALU_RGB_R_SWIZ_B_G |
						       R500_ALU_RGB_G_SWIZ_B_G |
						       R500_ALU_RGB_B_SWIZ_B_R));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDRD(4) |
						       R500_ALPHA_OP_MAD |
						       R500_ALPHA_SEL_A_SRC0 |
						       R500_ALPHA_SWIZ_A_0 |
						       R500_ALPHA_SEL_B_SRC1 |
						       R500_ALPHA_SWIZ_B_R));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGBA_ADDRD(4) |
						       R500_ALU_RGBA_OP_MAD |
						       R500_ALU_RGBA_R_SWIZ_0 |
						       R500_ALU_RGBA_G_SWIZ_0 |
						       R500_ALU_RGBA_B_SWIZ_0 |
						       R500_ALU_RGBA_A_SWIZ_0));

		/* MAD temp3, const0.0y0y, temp5.xxxx, temp4 */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_ALU |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_RGB_ADDR0(0) |
						       R500_RGB_ADDR0_CONST |
						       R500_RGB_ADDR1(5) |
						       R500_RGB_ADDR2(4)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDR0(0) |
						       R500_ALPHA_ADDR0_CONST |
						       R500_ALPHA_ADDR1(5) |
						       R500_ALPHA_ADDR2(4)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGB_SEL_A_SRC0 |
						       R500_ALU_RGB_R_SWIZ_A_0 |
						       R500_ALU_RGB_G_SWIZ_A_G |
						       R500_ALU_RGB_B_SWIZ_A_0 |
						       R500_ALU_RGB_SEL_B_SRC1 |
						       R500_ALU_RGB_R_SWIZ_B_R |
						       R500_ALU_RGB_G_SWIZ_B_R |
						       R500_ALU_RGB_B_SWIZ_B_R));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDRD(3) |
						       R500_ALPHA_OP_MAD |
						       R500_ALPHA_SEL_A_SRC0 |
						       R500_ALPHA_SWIZ_A_G |
						       R500_ALPHA_SEL_B_SRC1 |
						       R500_ALPHA_SWIZ_B_R));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGBA_ADDRD(3) |
						       R500_ALU_RGBA_OP_MAD |
						       R500_ALU_RGBA_SEL_C_SRC2 |
						       R500_ALU_RGBA_R_SWIZ_R |
						       R500_ALU_RGBA_G_SWIZ_G |
						       R500_ALU_RGBA_B_SWIZ_B |
						       R500_ALU_RGBA_A_SWIZ_A));

		/* ADD temp3, temp3, input0.xyxy */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_ALU |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_RGB_ADDR1(3) |
						       R500_RGB_ADDR2(0)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDR1(3) |
						       R500_ALPHA_ADDR2(0)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGB_R_SWIZ_A_1 |
						       R500_ALU_RGB_G_SWIZ_A_1 |
						       R500_ALU_RGB_B_SWIZ_A_1 |
						       R500_ALU_RGB_SEL_B_SRC1 |
						       R500_ALU_RGB_R_SWIZ_B_R |
						       R500_ALU_RGB_G_SWIZ_B_G |
						       R500_ALU_RGB_B_SWIZ_B_B));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDRD(3) |
						       R500_ALPHA_OP_MAD |
						       R500_ALPHA_SWIZ_A_1 |
						       R500_ALPHA_SEL_B_SRC1 |
						       R500_ALPHA_SWIZ_B_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGBA_ADDRD(3) |
						       R500_ALU_RGBA_OP_MAD |
						       R500_ALU_RGBA_SEL_C_SRC2 |
						       R500_ALU_RGBA_R_SWIZ_R |
						       R500_ALU_RGBA_G_SWIZ_G |
						       R500_ALU_RGBA_B_SWIZ_R |
						       R500_ALU_RGBA_A_SWIZ_G));

		/* TEX temp1, temp3.zwxy, tex0, 2D */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_TEX |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_ID(0) |
						       R500_TEX_INST_LD |
						       R500_TEX_IGNORE_UNCOVERED));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_SRC_ADDR(3) |
						       R500_TEX_SRC_S_SWIZ_B |
						       R500_TEX_SRC_T_SWIZ_A |
						       R500_TEX_SRC_R_SWIZ_R |
						       R500_TEX_SRC_Q_SWIZ_G |
						       R500_TEX_DST_ADDR(1) |
						       R500_TEX_DST_R_SWIZ_R |
						       R500_TEX_DST_G_SWIZ_G |
						       R500_TEX_DST_B_SWIZ_B |
						       R500_TEX_DST_A_SWIZ_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);

		/* TEX temp3, temp3.xyzw, tex0, 2D */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_TEX |
						       R500_INST_TEX_SEM_WAIT |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_ID(0) |
						       R500_TEX_INST_LD |
						       R500_TEX_SEM_ACQUIRE |
						       R500_TEX_IGNORE_UNCOVERED));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_SRC_ADDR(3) |
						       R500_TEX_SRC_S_SWIZ_R |
						       R500_TEX_SRC_T_SWIZ_G |
						       R500_TEX_SRC_R_SWIZ_B |
						       R500_TEX_SRC_Q_SWIZ_A |
						       R500_TEX_DST_ADDR(3) |
						       R500_TEX_DST_R_SWIZ_R |
						       R500_TEX_DST_G_SWIZ_G |
						       R500_TEX_DST_B_SWIZ_B |
						       R500_TEX_DST_A_SWIZ_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);

		/* MAD temp4, const0.0y0y, temp5.yyyy, temp4 */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_ALU |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_RGB_ADDR0(0) |
						       R500_RGB_ADDR0_CONST |
						       R500_RGB_ADDR1(5) |
						       R500_RGB_ADDR2(4)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDR0(0) |
						       R500_ALPHA_ADDR0_CONST |
						       R500_ALPHA_ADDR1(5) |
						       R500_ALPHA_ADDR2(4)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGB_SEL_A_SRC0 |
						       R500_ALU_RGB_R_SWIZ_A_0 |
						       R500_ALU_RGB_G_SWIZ_A_G |
						       R500_ALU_RGB_B_SWIZ_A_0 |
						       R500_ALU_RGB_SEL_B_SRC1 |
						       R500_ALU_RGB_R_SWIZ_B_G |
						       R500_ALU_RGB_G_SWIZ_B_G |
						       R500_ALU_RGB_B_SWIZ_B_G));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDRD(4) |
						       R500_ALPHA_OP_MAD |
						       R500_ALPHA_SEL_A_SRC0 |
						       R500_ALPHA_SWIZ_A_G |
						       R500_ALPHA_SEL_B_SRC1 |
						       R500_ALPHA_SWIZ_B_G));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGBA_ADDRD(4) |
						       R500_ALU_RGBA_OP_MAD |
						       R500_ALU_RGBA_SEL_C_SRC2 |
						       R500_ALU_RGBA_R_SWIZ_R |
						       R500_ALU_RGBA_G_SWIZ_G |
						       R500_ALU_RGBA_B_SWIZ_B |
						       R500_ALU_RGBA_A_SWIZ_A));

		/* ADD temp0, temp4, input0.xyxy */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_ALU |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_RGB_ADDR1(4) |
						       R500_RGB_ADDR2(0)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDR1(4) |
						       R500_ALPHA_ADDR2(0)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGB_R_SWIZ_A_1 |
						       R500_ALU_RGB_G_SWIZ_A_1 |
						       R500_ALU_RGB_B_SWIZ_A_1 |
						       R500_ALU_RGB_SEL_B_SRC1 |
						       R500_ALU_RGB_R_SWIZ_B_R |
						       R500_ALU_RGB_G_SWIZ_B_G |
						       R500_ALU_RGB_B_SWIZ_B_B));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDRD(0) |
						       R500_ALPHA_OP_MAD |
						       R500_ALPHA_SWIZ_A_1 |
						       R500_ALPHA_SEL_B_SRC1 |
						       R500_ALPHA_SWIZ_B_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGBA_ADDRD(0) |
						       R500_ALU_RGBA_OP_MAD |
						       R500_ALU_RGBA_SEL_C_SRC2 |
						       R500_ALU_RGBA_R_SWIZ_R |
						       R500_ALU_RGBA_G_SWIZ_G |
						       R500_ALU_RGBA_B_SWIZ_R |
						       R500_ALU_RGBA_A_SWIZ_G));

		/* TEX temp4, temp0.zwzw, tex0, 2D */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_TEX |
						       R500_INST_TEX_SEM_WAIT |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_ID(0) |
						       R500_TEX_INST_LD |
						       R500_TEX_IGNORE_UNCOVERED));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_SRC_ADDR(0) |
						       R500_TEX_SRC_S_SWIZ_B |
						       R500_TEX_SRC_T_SWIZ_A |
						       R500_TEX_SRC_R_SWIZ_B |
						       R500_TEX_SRC_Q_SWIZ_A |
						       R500_TEX_DST_ADDR(4) |
						       R500_TEX_DST_R_SWIZ_R |
						       R500_TEX_DST_G_SWIZ_G |
						       R500_TEX_DST_B_SWIZ_B |
						       R500_TEX_DST_A_SWIZ_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);

		/* TEX temp0, temp0.xyzw, tex0, 2D */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_TEX |
						       R500_INST_TEX_SEM_WAIT |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						   R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_ID(0) |
						       R500_TEX_INST_LD |
						       R500_TEX_SEM_ACQUIRE |
						       R500_TEX_IGNORE_UNCOVERED));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_SRC_ADDR(0) |
						       R500_TEX_SRC_S_SWIZ_R |
						       R500_TEX_SRC_T_SWIZ_G |
						       R500_TEX_SRC_R_SWIZ_B |
						       R500_TEX_SRC_Q_SWIZ_A |
						       R500_TEX_DST_ADDR(0) |
						       R500_TEX_DST_R_SWIZ_R |
						       R500_TEX_DST_G_SWIZ_G |
						       R500_TEX_DST_B_SWIZ_B |
						       R500_TEX_DST_A_SWIZ_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);

		/* LRP temp3, temp2.zzzz, temp1, temp3 ->
		 * - PRESUB temps, temp1 - temp3
		 * - MAD temp2.zzzz, temps, temp3 */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_ALU |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_RGB_ADDR0(3) |
						       R500_RGB_SRCP_OP_RGB1_MINUS_RGB0 |
						       R500_RGB_ADDR1(1) |
						       R500_RGB_ADDR2(2)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDR0(3) |
						       R500_ALPHA_SRCP_OP_A1_MINUS_A0 |
						       R500_ALPHA_ADDR1(1) |
						       R500_ALPHA_ADDR2(2)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGB_SEL_A_SRC2 |
						       R500_ALU_RGB_R_SWIZ_A_B |
						       R500_ALU_RGB_G_SWIZ_A_B |
						       R500_ALU_RGB_B_SWIZ_A_B |
						       R500_ALU_RGB_SEL_B_SRCP |
						       R500_ALU_RGB_R_SWIZ_B_R |
						       R500_ALU_RGB_G_SWIZ_B_G |
						       R500_ALU_RGB_B_SWIZ_B_B));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDRD(3) |
						       R500_ALPHA_OP_MAD |
						       R500_ALPHA_SEL_A_SRC2 |
						       R500_ALPHA_SWIZ_A_B |
						       R500_ALPHA_SEL_B_SRCP |
						       R500_ALPHA_SWIZ_B_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGBA_ADDRD(3) |
						       R500_ALU_RGBA_OP_MAD |
						       R500_ALU_RGBA_SEL_C_SRC0 |
						       R500_ALU_RGBA_R_SWIZ_R |
						       R500_ALU_RGBA_G_SWIZ_G |
						       R500_ALU_RGBA_B_SWIZ_B |
						       R500_ALU_RGBA_A_SWIZ_A));

		/* LRP temp0, temp2.zzzz, temp4, temp0 ->
		 * - PRESUB temps, temp4 - temp1
		 * - MAD temp2.zzzz, temps, temp0 */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_ALU |
						       R500_INST_TEX_SEM_WAIT |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_RGB_ADDR0(0) |
						       R500_RGB_SRCP_OP_RGB1_MINUS_RGB0 |
						       R500_RGB_ADDR1(4) |
						       R500_RGB_ADDR2(2)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDR0(0) |
						       R500_ALPHA_SRCP_OP_A1_MINUS_A0 |
						       R500_ALPHA_ADDR1(4) |
						       R500_ALPHA_ADDR2(2)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGB_SEL_A_SRC2 |
						       R500_ALU_RGB_R_SWIZ_A_B |
						       R500_ALU_RGB_G_SWIZ_A_B |
						       R500_ALU_RGB_B_SWIZ_A_B |
						       R500_ALU_RGB_SEL_B_SRCP |
						       R500_ALU_RGB_R_SWIZ_B_R |
						       R500_ALU_RGB_G_SWIZ_B_G |
						       R500_ALU_RGB_B_SWIZ_B_B));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDRD(0) |
						       R500_ALPHA_OP_MAD |
						       R500_ALPHA_SEL_A_SRC2 |
						       R500_ALPHA_SWIZ_A_B |
						       R500_ALPHA_SEL_B_SRCP |
						       R500_ALPHA_SWIZ_B_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGBA_ADDRD(0) |
						       R500_ALU_RGBA_OP_MAD |
						       R500_ALU_RGBA_SEL_C_SRC0 |
						       R500_ALU_RGBA_R_SWIZ_R |
						       R500_ALU_RGBA_G_SWIZ_G |
						       R500_ALU_RGBA_B_SWIZ_B |
						       R500_ALU_RGBA_A_SWIZ_A));

		/* LRP output, temp5.zzzz, temp3, temp0 ->
		 * - PRESUB temps, temp3 - temp0
		 * - MAD temp5.zzzz, temps, temp0 */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_OUT |
						       R500_INST_LAST |
						       R500_INST_TEX_SEM_WAIT |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK |
						       R500_INST_RGB_OMASK_R |
						       R500_INST_RGB_OMASK_G |
						       R500_INST_RGB_OMASK_B |
						       R500_INST_ALPHA_OMASK));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_RGB_ADDR0(0) |
						       R500_RGB_SRCP_OP_RGB1_MINUS_RGB0 |
						       R500_RGB_ADDR1(3) |
						       R500_RGB_ADDR2(5)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDR0(0) |
						       R500_ALPHA_SRCP_OP_A1_MINUS_A0 |
						       R500_ALPHA_ADDR1(3) |
						       R500_ALPHA_ADDR2(5)));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGB_SEL_A_SRC2 |
						       R500_ALU_RGB_R_SWIZ_A_B |
						       R500_ALU_RGB_G_SWIZ_A_B |
						       R500_ALU_RGB_B_SWIZ_A_B |
						       R500_ALU_RGB_SEL_B_SRCP |
						       R500_ALU_RGB_R_SWIZ_B_R |
						       R500_ALU_RGB_G_SWIZ_B_G |
						       R500_ALU_RGB_B_SWIZ_B_B));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDRD(0) |
						       R500_ALPHA_OP_MAD |
						       R500_ALPHA_SEL_A_SRC2 |
						       R500_ALPHA_SWIZ_A_B |
						       R500_ALPHA_SEL_B_SRCP |
						       R500_ALPHA_SWIZ_B_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGBA_ADDRD(0) |
						       R500_ALU_RGBA_OP_MAD |
						       R500_ALU_RGBA_SEL_C_SRC0 |
						       R500_ALU_RGBA_R_SWIZ_R |
						       R500_ALU_RGBA_G_SWIZ_G |
						       R500_ALU_RGBA_B_SWIZ_B |
						       R500_ALU_RGBA_A_SWIZ_A));

		/* Shader constants. */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_INDEX, R500_US_VECTOR_CONST_INDEX(0));

		/* const0 = {1 / texture[0].width, 1 / texture[0].height, 0, 0} */
		OUT_ACCEL_REG_F(R500_GA_US_VECTOR_DATA, (1.0/(float)pPriv->w));
		OUT_ACCEL_REG_F(R500_GA_US_VECTOR_DATA, (1.0/(float)pPriv->h));
		OUT_ACCEL_REG_F(R500_GA_US_VECTOR_DATA, 0x0);
		OUT_ACCEL_REG_F(R500_GA_US_VECTOR_DATA, 0x0);

		FINISH_ACCEL();

	    } else {
		BEGIN_ACCEL(19);
		/* 2 components: 2 for tex0 */
		OUT_ACCEL_REG(R300_RS_COUNT,
			      ((2 << R300_RS_COUNT_IT_COUNT_SHIFT) |
			       R300_RS_COUNT_HIRES_EN));

		/* R300_INST_COUNT_RS - highest RS instruction used */
		OUT_ACCEL_REG(R300_RS_INST_COUNT, R300_INST_COUNT_RS(0) | R300_TX_OFFSET_RS(6));

		/* Pixel stack frame size. */
		OUT_ACCEL_REG(R300_US_PIXSIZE, 0); /* highest temp used */

		/* FP length. */
		OUT_ACCEL_REG(R500_US_CODE_ADDR, (R500_US_CODE_START_ADDR(0) |
						  R500_US_CODE_END_ADDR(1)));
		OUT_ACCEL_REG(R500_US_CODE_RANGE, (R500_US_CODE_RANGE_ADDR(0) |
						   R500_US_CODE_RANGE_SIZE(1)));

		/* Prepare for FP emission. */
		OUT_ACCEL_REG(R500_US_CODE_OFFSET, 0);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_INDEX, R500_US_VECTOR_INST_INDEX(0));

		/* tex inst */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_TEX |
						       R500_INST_TEX_SEM_WAIT |
						       R500_INST_RGB_WMASK_R |
						       R500_INST_RGB_WMASK_G |
						       R500_INST_RGB_WMASK_B |
						       R500_INST_ALPHA_WMASK |
						       R500_INST_RGB_CLAMP |
						       R500_INST_ALPHA_CLAMP));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_ID(0) |
						       R500_TEX_INST_LD |
						       R500_TEX_SEM_ACQUIRE |
						       R500_TEX_IGNORE_UNCOVERED));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_SRC_ADDR(0) |
						       R500_TEX_SRC_S_SWIZ_R |
						       R500_TEX_SRC_T_SWIZ_G |
						       R500_TEX_DST_ADDR(0) |
						       R500_TEX_DST_R_SWIZ_R |
						       R500_TEX_DST_G_SWIZ_G |
						       R500_TEX_DST_B_SWIZ_B |
						       R500_TEX_DST_A_SWIZ_A));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_DX_ADDR(0) |
						       R500_DX_S_SWIZ_R |
						       R500_DX_T_SWIZ_R |
						       R500_DX_R_SWIZ_R |
						       R500_DX_Q_SWIZ_R |
						       R500_DY_ADDR(0) |
						       R500_DY_S_SWIZ_R |
						       R500_DY_T_SWIZ_R |
						       R500_DY_R_SWIZ_R |
						       R500_DY_Q_SWIZ_R));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000);

		/* ALU inst */
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_OUT |
						       R500_INST_TEX_SEM_WAIT |
						       R500_INST_LAST |
						       R500_INST_RGB_OMASK_R |
						       R500_INST_RGB_OMASK_G |
						       R500_INST_RGB_OMASK_B |
						       R500_INST_ALPHA_OMASK |
						       R500_INST_RGB_CLAMP |
						       R500_INST_ALPHA_CLAMP));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_RGB_ADDR0(0) |
						       R500_RGB_ADDR1(0) |
						       R500_RGB_ADDR1_CONST |
						       R500_RGB_ADDR2(0) |
						       R500_RGB_ADDR2_CONST));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDR0(0) |
						       R500_ALPHA_ADDR1(0) |
						       R500_ALPHA_ADDR1_CONST |
						       R500_ALPHA_ADDR2(0) |
						       R500_ALPHA_ADDR2_CONST));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGB_SEL_A_SRC0 |
						       R500_ALU_RGB_R_SWIZ_A_R |
						       R500_ALU_RGB_G_SWIZ_A_G |
						       R500_ALU_RGB_B_SWIZ_A_B |
						       R500_ALU_RGB_SEL_B_SRC0 |
						       R500_ALU_RGB_R_SWIZ_B_1 |
						       R500_ALU_RGB_B_SWIZ_B_1 |
						       R500_ALU_RGB_G_SWIZ_B_1));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_OP_MAD |
						       R500_ALPHA_SWIZ_A_A |
						       R500_ALPHA_SWIZ_B_1));
		OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGBA_OP_MAD |
						       R500_ALU_RGBA_R_SWIZ_0 |
						       R500_ALU_RGBA_G_SWIZ_0 |
						       R500_ALU_RGBA_B_SWIZ_0 |
						       R500_ALU_RGBA_A_SWIZ_0));
		FINISH_ACCEL();
	    }
	}

	BEGIN_ACCEL(6);
	OUT_ACCEL_REG(R300_TX_INVALTAGS, 0);
	OUT_ACCEL_REG(R300_TX_ENABLE, txenable);

	OUT_ACCEL_REG(R300_RB3D_COLOROFFSET0, dst_offset);
	OUT_ACCEL_REG(R300_RB3D_COLORPITCH0, colorpitch);

	blendcntl = RADEON_SRC_BLEND_GL_ONE | RADEON_DST_BLEND_GL_ZERO;
	/* no need to enable blending */
	OUT_ACCEL_REG(R300_RB3D_BLENDCNTL, blendcntl);

	OUT_ACCEL_REG(R300_VAP_VTX_SIZE, vtx_count);
	FINISH_ACCEL();

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

	BEGIN_ACCEL(5);

	OUT_ACCEL_REG(RADEON_PP_CNTL,
		      RADEON_TEX_0_ENABLE | RADEON_TEX_BLEND_0_ENABLE);
	OUT_ACCEL_REG(RADEON_RB3D_CNTL,
		      dst_format | RADEON_ALPHA_BLEND_ENABLE);
	OUT_ACCEL_REG(RADEON_RB3D_COLOROFFSET, dst_offset);

	OUT_ACCEL_REG(RADEON_RB3D_COLORPITCH, colorpitch);

	OUT_ACCEL_REG(RADEON_RB3D_BLENDCNTL,
		      RADEON_SRC_BLEND_GL_ONE | RADEON_DST_BLEND_GL_ZERO);

	FINISH_ACCEL();


	if ((info->ChipFamily == CHIP_FAMILY_RV250) ||
	    (info->ChipFamily == CHIP_FAMILY_RV280) ||
	    (info->ChipFamily == CHIP_FAMILY_RS300) ||
	    (info->ChipFamily == CHIP_FAMILY_R200)) {

	    info->accel_state->texW[0] = pPriv->w;
	    info->accel_state->texH[0] = pPriv->h;

	    BEGIN_ACCEL(12);

	    OUT_ACCEL_REG(R200_SE_VTX_FMT_0, R200_VTX_XY);
	    OUT_ACCEL_REG(R200_SE_VTX_FMT_1,
			  (2 << R200_VTX_TEX0_COMP_CNT_SHIFT));

	    OUT_ACCEL_REG(R200_PP_TXFILTER_0,
			  R200_MAG_FILTER_LINEAR |
			  R200_MIN_FILTER_LINEAR |
			  R200_CLAMP_S_CLAMP_LAST |
			  R200_CLAMP_T_CLAMP_LAST |
			  R200_YUV_TO_RGB);
	    OUT_ACCEL_REG(R200_PP_TXFORMAT_0, txformat);
	    OUT_ACCEL_REG(R200_PP_TXFORMAT_X_0, 0);
	    OUT_ACCEL_REG(R200_PP_TXSIZE_0,
			  (pPriv->w - 1) |
			  ((pPriv->h - 1) << RADEON_TEX_VSIZE_SHIFT));
	    OUT_ACCEL_REG(R200_PP_TXPITCH_0, pPriv->src_pitch - 32);

	    OUT_ACCEL_REG(R200_PP_TXOFFSET_0, pPriv->src_offset);

	    OUT_ACCEL_REG(R200_PP_TXCBLEND_0,
			  R200_TXC_ARG_A_ZERO |
			  R200_TXC_ARG_B_ZERO |
			  R200_TXC_ARG_C_R0_COLOR |
			  R200_TXC_OP_MADD);
	    OUT_ACCEL_REG(R200_PP_TXCBLEND2_0,
			  R200_TXC_CLAMP_0_1 | R200_TXC_OUTPUT_REG_R0);
	    OUT_ACCEL_REG(R200_PP_TXABLEND_0,
			  R200_TXA_ARG_A_ZERO |
			  R200_TXA_ARG_B_ZERO |
			  R200_TXA_ARG_C_R0_ALPHA |
			  R200_TXA_OP_MADD);
	    OUT_ACCEL_REG(R200_PP_TXABLEND2_0,
			  R200_TXA_CLAMP_0_1 | R200_TXA_OUTPUT_REG_R0);
	    FINISH_ACCEL();
	} else {

	    info->accel_state->texW[0] = 1;
	    info->accel_state->texH[0] = 1;

	    BEGIN_ACCEL(8);

	    OUT_ACCEL_REG(RADEON_SE_VTX_FMT, (RADEON_SE_VTX_FMT_XY |
					      RADEON_SE_VTX_FMT_ST0));

	    OUT_ACCEL_REG(RADEON_PP_TXFILTER_0,
			  RADEON_MAG_FILTER_LINEAR |
			  RADEON_MIN_FILTER_LINEAR |
			  RADEON_CLAMP_S_CLAMP_LAST |
			  RADEON_CLAMP_T_CLAMP_LAST |
			  RADEON_YUV_TO_RGB);
	    OUT_ACCEL_REG(RADEON_PP_TXFORMAT_0, txformat);
	    OUT_ACCEL_REG(RADEON_PP_TXOFFSET_0, pPriv->src_offset);
	    OUT_ACCEL_REG(RADEON_PP_TXCBLEND_0,
			  RADEON_COLOR_ARG_A_ZERO |
			  RADEON_COLOR_ARG_B_ZERO |
			  RADEON_COLOR_ARG_C_T0_COLOR |
			  RADEON_BLEND_CTL_ADD |
			  RADEON_CLAMP_TX);
	    OUT_ACCEL_REG(RADEON_PP_TXABLEND_0,
			  RADEON_ALPHA_ARG_A_ZERO |
			  RADEON_ALPHA_ARG_B_ZERO |
			  RADEON_ALPHA_ARG_C_T0_ALPHA |
			  RADEON_BLEND_CTL_ADD |
			  RADEON_CLAMP_TX);

	    OUT_ACCEL_REG(RADEON_PP_TEX_SIZE_0,
			  (pPriv->w - 1) |
			  ((pPriv->h - 1) << RADEON_TEX_VSIZE_SHIFT));
	    OUT_ACCEL_REG(RADEON_PP_TEX_PITCH_0,
			  pPriv->src_pitch - 32);
	    FINISH_ACCEL();
	}
    }

    FUNC_NAME(RADEONWaitForVLine)(pScrn, pPixmap,
				  radeon_covering_crtc_num(pScrn,
							   pPriv->drw_x,
							   pPriv->drw_x + pPriv->dst_w,
							   pPriv->drw_y,
							   pPriv->drw_y + pPriv->dst_h,
							   pPriv->desired_crtc),
				  pPriv->drw_y,
				  pPriv->drw_y + pPriv->dst_h,
				  pPriv->vsync);

    /*
     * Rendering of the actual polygon is done in two different
     * ways depending on chip generation:
     *
     * < R300:
     *
     *     These chips can render a rectangle in one pass, so
     *     handling is pretty straight-forward.
     *
     * >= R300:
     *
     *     These chips can accept a quad, but will render it as
     *     two triangles which results in a diagonal tear. Instead
     *     We render a single, large triangle and use the scissor
     *     functionality to restrict it to the desired rectangle.
     *     Due to guardband limits on r3xx/r4xx, we can only use
     *     the single triangle up to 2880 pixels; above that we
     *     render as a quad.
     */

    while (nBox--) {
	int srcX, srcY, srcw, srch;
	int dstX, dstY, dstw, dsth;
	Bool use_quad = FALSE;
	dstX = pBox->x1 + dstxoff;
	dstY = pBox->y1 + dstyoff;
	dstw = pBox->x2 - pBox->x1;
	dsth = pBox->y2 - pBox->y1;

	srcX = ((pBox->x1 - pPriv->drw_x) *
		pPriv->src_w) / pPriv->dst_w;
	srcY = ((pBox->y1 - pPriv->drw_y) *
		pPriv->src_h) / pPriv->dst_h;

	srcw = (pPriv->src_w * dstw) / pPriv->dst_w;
	srch = (pPriv->src_h * dsth) / pPriv->dst_h;

#if 0
	ErrorF("dst: %d, %d, %d, %d\n", dstX, dstY, dstw, dsth);
	ErrorF("src: %d, %d, %d, %d\n", srcX, srcY, srcw, srch);
#endif

	if (IS_R300_3D || IS_R500_3D) {
	    if (IS_R300_3D && ((dstw+dsth) > 2880))
		use_quad = TRUE;
	    /*
	     * Set up the scissor area to that of the output size.
	     */
	    BEGIN_ACCEL(2);
	    if (IS_R300_3D) {
		/* R300 has an offset */
		OUT_ACCEL_REG(R300_SC_SCISSOR0, (((dstX + 1088) << R300_SCISSOR_X_SHIFT) |
						 ((dstY + 1088) << R300_SCISSOR_Y_SHIFT)));
		OUT_ACCEL_REG(R300_SC_SCISSOR1, (((dstX + dstw + 1088 - 1) << R300_SCISSOR_X_SHIFT) |
						 ((dstY + dsth + 1088 - 1) << R300_SCISSOR_Y_SHIFT)));
	    } else {
		OUT_ACCEL_REG(R300_SC_SCISSOR0, (((dstX) << R300_SCISSOR_X_SHIFT) |
						 ((dstY) << R300_SCISSOR_Y_SHIFT)));
		OUT_ACCEL_REG(R300_SC_SCISSOR1, (((dstX + dstw - 1) << R300_SCISSOR_X_SHIFT) |
						 ((dstY + dsth - 1) << R300_SCISSOR_Y_SHIFT)));
	    }
	    FINISH_ACCEL();
	}

#ifdef ACCEL_CP
	if (info->ChipFamily < CHIP_FAMILY_R200) {
	    BEGIN_RING(3 * vtx_count + 3);
	    OUT_RING(CP_PACKET3(RADEON_CP_PACKET3_3D_DRAW_IMMD,
				3 * vtx_count + 1));
	    OUT_RING(RADEON_CP_VC_FRMT_XY |
		     RADEON_CP_VC_FRMT_ST0);
	    OUT_RING(RADEON_CP_VC_CNTL_PRIM_TYPE_RECT_LIST |
		     RADEON_CP_VC_CNTL_PRIM_WALK_RING |
		     RADEON_CP_VC_CNTL_MAOS_ENABLE |
		     RADEON_CP_VC_CNTL_VTX_FMT_RADEON_MODE |
		     (3 << RADEON_CP_VC_CNTL_NUM_SHIFT));
	} else if (IS_R300_3D || IS_R500_3D) {
	    if (use_quad) {
		BEGIN_RING(4 * vtx_count + 4);
		OUT_RING(CP_PACKET3(R200_CP_PACKET3_3D_DRAW_IMMD_2,
				    4 * vtx_count));
		OUT_RING(RADEON_CP_VC_CNTL_PRIM_TYPE_QUAD_LIST |
			 RADEON_CP_VC_CNTL_PRIM_WALK_RING |
			 (4 << RADEON_CP_VC_CNTL_NUM_SHIFT));
	    } else {
		BEGIN_RING(3 * vtx_count + 4);
		OUT_RING(CP_PACKET3(R200_CP_PACKET3_3D_DRAW_IMMD_2,
				    3 * vtx_count));
		OUT_RING(RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_LIST |
			 RADEON_CP_VC_CNTL_PRIM_WALK_RING |
			 (3 << RADEON_CP_VC_CNTL_NUM_SHIFT));
	    }
	} else {
	    BEGIN_RING(3 * vtx_count + 2);
	    OUT_RING(CP_PACKET3(R200_CP_PACKET3_3D_DRAW_IMMD_2,
				3 * vtx_count));
	    OUT_RING(RADEON_CP_VC_CNTL_PRIM_TYPE_RECT_LIST |
		     RADEON_CP_VC_CNTL_PRIM_WALK_RING |
		     (3 << RADEON_CP_VC_CNTL_NUM_SHIFT));
	}
#else /* ACCEL_CP */
	if (IS_R300_3D || IS_R500_3D) {
	    if (use_quad)
		BEGIN_ACCEL(2 + vtx_count * 4);
	    else
		BEGIN_ACCEL(2 + vtx_count * 3);
	} else
	    BEGIN_ACCEL(1 + vtx_count * 3);

	if (info->ChipFamily < CHIP_FAMILY_R200)
	    OUT_ACCEL_REG(RADEON_SE_VF_CNTL, (RADEON_VF_PRIM_TYPE_RECTANGLE_LIST |
					      RADEON_VF_PRIM_WALK_DATA |
					      RADEON_VF_RADEON_MODE |
					      (3 << RADEON_VF_NUM_VERTICES_SHIFT)));
	else if (IS_R300_3D || IS_R500_3D) {
	    if (use_quad)
		OUT_ACCEL_REG(RADEON_SE_VF_CNTL, (RADEON_VF_PRIM_TYPE_QUAD_LIST |
						  RADEON_VF_PRIM_WALK_DATA |
						  (4 << RADEON_VF_NUM_VERTICES_SHIFT)));
	    else
		OUT_ACCEL_REG(RADEON_SE_VF_CNTL, (RADEON_VF_PRIM_TYPE_TRIANGLE_LIST |
						  RADEON_VF_PRIM_WALK_DATA |
						  (3 << RADEON_VF_NUM_VERTICES_SHIFT)));
	} else
	    OUT_ACCEL_REG(RADEON_SE_VF_CNTL, (RADEON_VF_PRIM_TYPE_RECTANGLE_LIST |
					      RADEON_VF_PRIM_WALK_DATA |
					      (3 << RADEON_VF_NUM_VERTICES_SHIFT)));

#endif
	if (pPriv->bicubic_enabled) {
		/*
		 * This code is only executed on >= R300, so we don't
		 * have to deal with the legacy handling.
		 */
	    if (use_quad) {
		VTX_OUT_FILTER((float)dstX,                                       (float)dstY,
			       (float)srcX / info->accel_state->texW[0],          (float)srcY / info->accel_state->texH[0],
			       (float)srcX + 0.5,                                 (float)srcY + 0.5);
		VTX_OUT_FILTER((float)dstX,                                       (float)(dstY + dsth),
			       (float)srcX / info->accel_state->texW[0],          (float)(srcY + srch) / info->accel_state->texH[0],
			       (float)srcX + 0.5,                                 (float)(srcY + srch) + 0.5);
		VTX_OUT_FILTER((float)(dstX + dstw),                              (float)(dstY + dsth),
			       (float)(srcX + srcw) / info->accel_state->texW[0], (float)(srcY + srch) / info->accel_state->texH[0],
			       (float)(srcX + srcw) + 0.5,                        (float)(srcY + srch) + 0.5);
		VTX_OUT_FILTER((float)(dstX + dstw),                              (float)dstY,
			       (float)(srcX + srcw) / info->accel_state->texW[0], (float)srcY / info->accel_state->texH[0],
			       (float)(srcX + srcw) + 0.5,                        (float)srcY + 0.5);
	    } else {
		VTX_OUT_FILTER((float)dstX,                                       (float)dstY,
			       (float)srcX / info->accel_state->texW[0],          (float)srcY / info->accel_state->texH[0],
			       (float)srcX + 0.5,                                 (float)srcY + 0.5);
		VTX_OUT_FILTER((float)dstX,                                       (float)(dstY + dstw + dsth),
			       (float)srcX / info->accel_state->texW[0],          ((float)srcY + (float)srch * (((float)dstw / (float)dsth) + 1.0)) / info->accel_state->texH[0],
			       (float)srcX + 0.5,                                 (float)srcY + (float)srch * (((float)dstw / (float)dsth) + 1.0) + 0.5);
		VTX_OUT_FILTER((float)(dstX + dstw + dsth),                       (float)dstY,
			       ((float)srcX + (float)srcw * (((float)dsth / (float)dstw) + 1.0)) / info->accel_state->texW[0],
			                                                          (float)srcY / info->accel_state->texH[0],
			       (float)srcX + (float)srcw * (((float)dsth / (float)dstw) + 1.0) + 0.5,
			                                                          (float)srcY + 0.5);
	    }
	} else {
	    if (IS_R300_3D || IS_R500_3D) {
		if (use_quad) {
		    VTX_OUT((float)dstX,                                       (float)dstY,
			    (float)srcX / info->accel_state->texW[0],          (float)srcY / info->accel_state->texH[0]);
		    VTX_OUT((float)dstX,                                       (float)(dstY + dsth),
			    (float)srcX / info->accel_state->texW[0],          (float)(srcY + srch) / info->accel_state->texH[0]);
		    VTX_OUT((float)(dstX + dstw),                              (float)(dstY + dsth),
			    (float)(srcX + srcw) / info->accel_state->texW[0], (float)(srcY + srch) / info->accel_state->texH[0]);
		    VTX_OUT((float)(dstX + dstw),                              (float)dstY,
			    (float)(srcX + srcw) / info->accel_state->texW[0], (float)srcY / info->accel_state->texH[0]);
		} else {
		    /*
		     * Render a big, scissored triangle. This means
		     * increasing the triangle size and adjusting
		     * texture coordinates.
		     */
		    VTX_OUT((float)dstX,                              (float)dstY,
			    (float)srcX / info->accel_state->texW[0], (float)srcY / info->accel_state->texH[0]);
		    VTX_OUT((float)dstX,                              (float)(dstY + dsth + dstw),
			    (float)srcX / info->accel_state->texW[0], ((float)srcY + (float)srch * (((float)dstw / (float)dsth) + 1.0)) / info->accel_state->texH[0]);
			    
		    VTX_OUT((float)(dstX + dstw + dsth),              (float)dstY,
			    ((float)srcX + (float)srcw * (((float)dsth / (float)dstw) + 1.0)) / info->accel_state->texW[0],
			                                              (float)srcY / info->accel_state->texH[0]);
		}
	    } else {
		/*
		 * Just render a rect (using three coords).
		 */
		VTX_OUT((float)dstX,                                       (float)(dstY + dsth),
			(float)srcX / info->accel_state->texW[0],          (float)(srcY + srch) / info->accel_state->texH[0]);
		VTX_OUT((float)(dstX + dstw),                              (float)(dstY + dsth),
			(float)(srcX + srcw) / info->accel_state->texW[0], (float)(srcY + srch) / info->accel_state->texH[0]);
		VTX_OUT((float)(dstX + dstw),                              (float)dstY,
			(float)(srcX + srcw) / info->accel_state->texW[0], (float)srcY / info->accel_state->texH[0]);
	    }
	}

	if (IS_R300_3D || IS_R500_3D)
	    /* flushing is pipelined, free/finish is not */
	    OUT_ACCEL_REG(R300_RB3D_DSTCACHE_CTLSTAT, R300_DC_FLUSH_3D);

#ifdef ACCEL_CP
	ADVANCE_RING();
#else
	FINISH_ACCEL();
#endif /* !ACCEL_CP */

	pBox++;
    }

    if (IS_R300_3D || IS_R500_3D) {
	BEGIN_ACCEL(3);
	OUT_ACCEL_REG(R300_SC_CLIP_RULE, 0xAAAA);
	OUT_ACCEL_REG(R300_RB3D_DSTCACHE_CTLSTAT, R300_RB3D_DC_FLUSH_ALL);
    } else
	BEGIN_ACCEL(1);
    OUT_ACCEL_REG(RADEON_WAIT_UNTIL, RADEON_WAIT_3D_IDLECLEAN);
    FINISH_ACCEL();

    DamageDamageRegion(pPriv->pDraw, &pPriv->clip);
}

#undef VTX_OUT
#undef VTX_OUT_FILTER
#undef FUNC_NAME
