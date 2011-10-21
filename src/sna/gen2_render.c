/*
 * Copyright © 2006,2011 Intel Corporation
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
 * Authors:
 *    Wang Zhenyu <zhenyu.z.wang@intel.com>
 *    Eric Anholt <eric@anholt.net>
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sna.h"
#include "sna_reg.h"
#include "sna_render.h"
#include "sna_render_inline.h"

#include "gen2_render.h"

#if DEBUG_RENDER
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

#define NO_COMPOSITE 0
#define NO_COMPOSITE_SPANS 0
#define NO_COPY 0
#define NO_COPY_BOXES 0
#define NO_FILL 0
#define NO_FILL_ONE 0
#define NO_FILL_BOXES 0

#define PREFER_BLT_FILL 1
#define PREFER_BLT_COPY 1

#define BATCH(v) batch_emit(sna, v)
#define BATCH_F(v) batch_emit_float(sna, v)
#define VERTEX(v) batch_emit_float(sna, v)

/* TODO: Remaining items for the sufficiently motivated reader
 *
 * - Linear gradients (radial do require pixel shaders)
 *   - generate 1-d ramp for texture
 *   - compute 1-d texture coordinate using a linear projection matrix
 *   - issues? 1-stop, degenerate, fallback.
 *
 * - vmap
 *   - the texture sampler can use any type of memory apparently.
 *
 * - memory compaction?
 */

static const struct blendinfo {
	Bool dst_alpha;
	Bool src_alpha;
	uint32_t src_blend;
	uint32_t dst_blend;
} gen2_blend_op[] = {
	/* Clear */
	{0, 0, BLENDFACTOR_ZERO, BLENDFACTOR_ZERO},
	/* Src */
	{0, 0, BLENDFACTOR_ONE, BLENDFACTOR_ZERO},
	/* Dst */
	{0, 0, BLENDFACTOR_ZERO, BLENDFACTOR_ONE},
	/* Over */
	{0, 1, BLENDFACTOR_ONE, BLENDFACTOR_INV_SRC_ALPHA},
	/* OverReverse */
	{1, 0, BLENDFACTOR_INV_DST_ALPHA, BLENDFACTOR_ONE},
	/* In */
	{1, 0, BLENDFACTOR_DST_ALPHA, BLENDFACTOR_ZERO},
	/* InReverse */
	{0, 1, BLENDFACTOR_ZERO, BLENDFACTOR_SRC_ALPHA},
	/* Out */
	{1, 0, BLENDFACTOR_INV_DST_ALPHA, BLENDFACTOR_ZERO},
	/* OutReverse */
	{0, 1, BLENDFACTOR_ZERO, BLENDFACTOR_INV_SRC_ALPHA},
	/* Atop */
	{1, 1, BLENDFACTOR_DST_ALPHA, BLENDFACTOR_INV_SRC_ALPHA},
	/* AtopReverse */
	{1, 1, BLENDFACTOR_INV_DST_ALPHA, BLENDFACTOR_SRC_ALPHA},
	/* Xor */
	{1, 1, BLENDFACTOR_INV_DST_ALPHA, BLENDFACTOR_INV_SRC_ALPHA},
	/* Add */
	{0, 0, BLENDFACTOR_ONE, BLENDFACTOR_ONE},
};

static const struct formatinfo {
	unsigned int fmt;
	uint32_t card_fmt;
} i8xx_tex_formats[] = {
	{PICT_a8, MAPSURF_8BIT | MT_8BIT_A8},
	{PICT_a8r8g8b8, MAPSURF_32BIT | MT_32BIT_ARGB8888},
	{PICT_a8b8g8r8, MAPSURF_32BIT | MT_32BIT_ABGR8888},
	{PICT_r5g6b5, MAPSURF_16BIT | MT_16BIT_RGB565},
	{PICT_a1r5g5b5, MAPSURF_16BIT | MT_16BIT_ARGB1555},
	{PICT_a4r4g4b4, MAPSURF_16BIT | MT_16BIT_ARGB4444},
}, i85x_tex_formats[] = {
	{PICT_x8r8g8b8, MAPSURF_32BIT | MT_32BIT_XRGB8888},
	{PICT_x8b8g8r8, MAPSURF_32BIT | MT_32BIT_XBGR8888},
};

static inline uint32_t
gen2_buf_tiling(uint32_t tiling)
{
	uint32_t v = 0;
	switch (tiling) {
	default: assert(0);
	case I915_TILING_Y: v |= BUF_3D_TILE_WALK_Y;
	case I915_TILING_X: v |= BUF_3D_TILED_SURFACE;
	case I915_TILING_NONE: break;
	}
	return v;
}

static uint32_t
gen2_get_dst_format(uint32_t format)
{
#define BIAS DSTORG_HORT_BIAS(0x8) | DSTORG_VERT_BIAS(0x8)
	switch (format) {
	default:
		assert(0);
	case PICT_a8r8g8b8:
	case PICT_x8r8g8b8:
		return COLR_BUF_ARGB8888 | BIAS;
	case PICT_r5g6b5:
		return COLR_BUF_RGB565 | BIAS;
	case PICT_a1r5g5b5:
	case PICT_x1r5g5b5:
		return COLR_BUF_ARGB1555 | BIAS;
	case PICT_a8:
		return COLR_BUF_8BIT | BIAS;
	case PICT_a4r4g4b4:
	case PICT_x4r4g4b4:
		return COLR_BUF_ARGB4444 | BIAS;
	}
#undef BIAS
}

static Bool
gen2_check_dst_format(uint32_t format)
{
	switch (format) {
	case PICT_a8r8g8b8:
	case PICT_x8r8g8b8:
	case PICT_r5g6b5:
	case PICT_a1r5g5b5:
	case PICT_x1r5g5b5:
	case PICT_a8:
	case PICT_a4r4g4b4:
	case PICT_x4r4g4b4:
		return TRUE;
	default:
		return FALSE;
	}
}

static uint32_t
gen2_get_card_format(struct sna *sna, uint32_t format)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(i8xx_tex_formats); i++)
		if (i8xx_tex_formats[i].fmt == format)
			return i8xx_tex_formats[i].card_fmt;

	if (sna->kgem.gen < 21) {
		/* Whilst these are not directly supported on 830/845,
		 * we only enable them when we can implicitly convert
		 * them to a supported variant through the texture
		 * combiners.
		 */
		for (i = 0; i < ARRAY_SIZE(i85x_tex_formats); i++)
			if (i85x_tex_formats[i].fmt == format)
				return i8xx_tex_formats[1+i].card_fmt;
	} else {
		for (i = 0; i < ARRAY_SIZE(i85x_tex_formats); i++)
			if (i85x_tex_formats[i].fmt == format)
				return i85x_tex_formats[i].card_fmt;
	}

	assert(0);
	return 0;
}

static uint32_t
gen2_sampler_tiling_bits(uint32_t tiling)
{
	uint32_t bits = 0;
	switch (tiling) {
	default:
		assert(0);
	case I915_TILING_Y:
		bits |= TM0S1_TILE_WALK;
	case I915_TILING_X:
		bits |= TM0S1_TILED_SURFACE;
	case I915_TILING_NONE:
		break;
	}
	return bits;
}

static Bool
gen2_check_filter(PicturePtr picture)
{
	switch (picture->filter) {
	case PictFilterNearest:
	case PictFilterBilinear:
		return TRUE;
	default:
		return FALSE;
	}
}

static Bool
gen2_check_repeat(PicturePtr picture)
{
	if (!picture->repeat)
		return TRUE;

	switch (picture->repeatType) {
	case RepeatNone:
	case RepeatNormal:
	case RepeatPad:
	case RepeatReflect:
		return TRUE;
	default:
		return FALSE;
	}
}

static void
gen2_emit_texture(struct sna *sna,
		  const struct sna_composite_channel *channel,
		  int unit)
{
	uint32_t filter;
	uint32_t wrap_mode;
	uint32_t texcoordtype;

	if (channel->is_affine)
		texcoordtype = TEXCOORDTYPE_CARTESIAN;
	else
		texcoordtype = TEXCOORDTYPE_HOMOGENEOUS;

	switch (channel->repeat) {
	default:
		assert(0);
	case RepeatNone:
		wrap_mode = TEXCOORDMODE_CLAMP_BORDER;
		break;
	case RepeatNormal:
		wrap_mode = TEXCOORDMODE_WRAP;
		break;
	case RepeatPad:
		wrap_mode = TEXCOORDMODE_CLAMP;
		break;
	case RepeatReflect:
		wrap_mode = TEXCOORDMODE_MIRROR;
		break;
	}

	switch (channel->filter) {
	default:
		assert(0);
	case PictFilterNearest:
		filter = (FILTER_NEAREST << TM0S3_MAG_FILTER_SHIFT |
			  FILTER_NEAREST << TM0S3_MIN_FILTER_SHIFT |
			  MIPFILTER_NONE << TM0S3_MIP_FILTER_SHIFT);
		break;
	case PictFilterBilinear:
		filter = (FILTER_LINEAR << TM0S3_MAG_FILTER_SHIFT |
			  FILTER_LINEAR << TM0S3_MIN_FILTER_SHIFT |
			  MIPFILTER_NONE << TM0S3_MIP_FILTER_SHIFT);
		break;
	}

	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_2 | LOAD_TEXTURE_MAP(unit) | 4);
	BATCH(kgem_add_reloc(&sna->kgem, sna->kgem.nbatch,
			     channel->bo,
			     I915_GEM_DOMAIN_SAMPLER << 16,
			     0));
	BATCH(((channel->height - 1) << TM0S1_HEIGHT_SHIFT) |
	      ((channel->width - 1) << TM0S1_WIDTH_SHIFT) |
	      gen2_get_card_format(sna, channel->pict_format) |
	      gen2_sampler_tiling_bits(channel->bo->tiling));
	BATCH((channel->bo->pitch / 4 - 1) << TM0S2_PITCH_SHIFT | TM0S2_MAP_2D);
	BATCH(filter);
	BATCH(0);	/* default color */

	BATCH(_3DSTATE_MAP_COORD_SET_CMD | TEXCOORD_SET(unit) |
	      ENABLE_TEXCOORD_PARAMS | TEXCOORDS_ARE_NORMAL |
	      texcoordtype |
	      ENABLE_ADDR_V_CNTL | TEXCOORD_ADDR_V_MODE(wrap_mode) |
	      ENABLE_ADDR_U_CNTL | TEXCOORD_ADDR_U_MODE(wrap_mode));
}

static void
gen2_get_blend_factors(const struct sna_composite_op *op,
		       int blend,
		       uint32_t *c_out,
		       uint32_t *a_out)
{
	uint32_t cblend, ablend;

	/* If component alpha is active in the mask and the blend operation
	 * uses the source alpha, then we know we don't need the source
	 * value (otherwise we would have hit a fallback earlier), so we
	 * provide the source alpha (src.A * mask.X) as output color.
	 * Conversely, if CA is set and we don't need the source alpha, then
	 * we produce the source value (src.X * mask.X) and the source alpha
	 * is unused..  Otherwise, we provide the non-CA source value
	 * (src.X * mask.A).
	 *
	 * The PICT_FORMAT_RGB(pict) == 0 fixups are not needed on 855+'s a8
	 * pictures, but we need to implement it for 830/845 and there's no
	 * harm done in leaving it in.
	 */
	cblend = TB0C_LAST_STAGE | TB0C_RESULT_SCALE_1X | TB0C_OUTPUT_WRITE_CURRENT;
	ablend = TB0A_RESULT_SCALE_1X | TB0A_OUTPUT_WRITE_CURRENT;


	/* Get the source picture's channels into TBx_ARG1 */
	if ((op->has_component_alpha && gen2_blend_op[blend].src_alpha) ||
	    op->dst.format == PICT_a8) {
		/* Producing source alpha value, so the first set of channels
		 * is src.A instead of src.X.  We also do this if the destination
		 * is a8, in which case src.G is what's written, and the other
		 * channels are ignored.
		 */
		if (op->src.is_solid) {
			ablend |= TB0A_ARG1_SEL_DIFFUSE;
			cblend |= TB0C_ARG1_SEL_DIFFUSE | TB0C_ARG1_REPLICATE_ALPHA;
		} else {
			ablend |= TB0A_ARG1_SEL_TEXEL0;
			cblend |= TB0C_ARG1_SEL_TEXEL0 | TB0C_ARG1_REPLICATE_ALPHA;
		}
	} else {
		if (op->src.is_solid)
			cblend |= TB0C_ARG1_SEL_DIFFUSE;
		else if (PICT_FORMAT_RGB(op->src.pict_format) != 0)
			cblend |= TB0C_ARG1_SEL_TEXEL0;
		else
			cblend |= TB0C_ARG1_SEL_ONE | TB0C_ARG1_INVERT;	/* 0.0 */
		if (op->src.is_solid)
			ablend |= TB0A_ARG1_SEL_DIFFUSE;
		else if (op->src.is_opaque)
			ablend |= TB0A_ARG1_SEL_ONE;
		else
			ablend |= TB0A_ARG1_SEL_TEXEL0;
	}

	if (op->mask.bo) {
		if (op->src.is_solid) {
			cblend |= TB0C_ARG2_SEL_TEXEL0;
			ablend |= TB0A_ARG2_SEL_TEXEL0;
		} else {
			cblend |= TB0C_ARG2_SEL_TEXEL1;
			ablend |= TB0A_ARG2_SEL_TEXEL1;
		}

		if (op->dst.format == PICT_a8 ||
		    !op->has_component_alpha ||
		    PICT_FORMAT_RGB(op->mask.pict_format) == 0)
			cblend |= TB0C_ARG2_REPLICATE_ALPHA;

		cblend |= TB0C_OP_MODULATE;
		ablend |= TB0A_OP_MODULATE;
	} else {
		cblend |= TB0C_OP_ARG1;
		ablend |= TB0A_OP_ARG1;
	}

	*c_out = cblend;
	*a_out = ablend;
}

static uint32_t gen2_get_blend_cntl(int op,
				    Bool has_component_alpha,
				    uint32_t dst_format)
{
	uint32_t sblend, dblend;

	sblend = gen2_blend_op[op].src_blend;
	dblend = gen2_blend_op[op].dst_blend;

	/* If there's no dst alpha channel, adjust the blend op so that
	 * we'll treat it as always 1.
	 */
	if (PICT_FORMAT_A(dst_format) == 0 && gen2_blend_op[op].dst_alpha) {
		if (sblend == BLENDFACTOR_DST_ALPHA)
			sblend = BLENDFACTOR_ONE;
		else if (sblend == BLENDFACTOR_INV_DST_ALPHA)
			sblend = BLENDFACTOR_ZERO;
	}

	/* If the source alpha is being used, then we should only be in a case
	 * where the source blend factor is 0, and the source blend value is
	 * the mask channels multiplied by the source picture's alpha.
	 */
	if (has_component_alpha && gen2_blend_op[op].src_alpha) {
		if (dblend == BLENDFACTOR_SRC_ALPHA)
			dblend = BLENDFACTOR_SRC_COLR;
		else if (dblend == BLENDFACTOR_INV_SRC_ALPHA)
			dblend = BLENDFACTOR_INV_SRC_COLR;
	}

	return (sblend << S8_SRC_BLEND_FACTOR_SHIFT |
		dblend << S8_DST_BLEND_FACTOR_SHIFT);
}

static void gen2_emit_invariant(struct sna *sna)
{
	int i;

	for (i = 0; i < 4; i++) {
		BATCH(_3DSTATE_MAP_CUBE | MAP_UNIT(i));
		BATCH(_3DSTATE_MAP_TEX_STREAM_CMD | MAP_UNIT(i) |
		      DISABLE_TEX_STREAM_BUMP |
		      ENABLE_TEX_STREAM_COORD_SET | TEX_STREAM_COORD_SET(i) |
		      ENABLE_TEX_STREAM_MAP_IDX | TEX_STREAM_MAP_IDX(i));
		BATCH(_3DSTATE_MAP_COORD_TRANSFORM);
		BATCH(DISABLE_TEX_TRANSFORM | TEXTURE_SET(i));
	}

	BATCH(_3DSTATE_MAP_COORD_SETBIND_CMD);
	BATCH(TEXBIND_SET3(TEXCOORDSRC_VTXSET_3) |
	      TEXBIND_SET2(TEXCOORDSRC_VTXSET_2) |
	      TEXBIND_SET1(TEXCOORDSRC_VTXSET_1) |
	      TEXBIND_SET0(TEXCOORDSRC_VTXSET_0));

	BATCH(_3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT);

	BATCH(_3DSTATE_VERTEX_TRANSFORM);
	BATCH(DISABLE_VIEWPORT_TRANSFORM | DISABLE_PERSPECTIVE_DIVIDE);

	BATCH(_3DSTATE_W_STATE_CMD);
	BATCH(MAGIC_W_STATE_DWORD1);
	BATCH_F(1.0);

	BATCH(_3DSTATE_INDPT_ALPHA_BLEND_CMD |
	      DISABLE_INDPT_ALPHA_BLEND |
	      ENABLE_ALPHA_BLENDFUNC | ABLENDFUNC_ADD);

	BATCH(_3DSTATE_CONST_BLEND_COLOR_CMD);
	BATCH(0);

	BATCH(_3DSTATE_MODES_1_CMD |
	      ENABLE_COLR_BLND_FUNC | BLENDFUNC_ADD |
	      ENABLE_SRC_BLND_FACTOR | SRC_BLND_FACT(BLENDFACTOR_ONE) |
	      ENABLE_DST_BLND_FACTOR | DST_BLND_FACT(BLENDFACTOR_ZERO));

	BATCH(_3DSTATE_ENABLES_1_CMD |
	      DISABLE_LOGIC_OP |
	      DISABLE_STENCIL_TEST |
	      DISABLE_DEPTH_BIAS |
	      DISABLE_SPEC_ADD |
	      DISABLE_FOG |
	      DISABLE_ALPHA_TEST |
	      DISABLE_DEPTH_TEST |
	      ENABLE_COLOR_BLEND);

	BATCH(_3DSTATE_ENABLES_2_CMD |
	      DISABLE_STENCIL_WRITE |
	      DISABLE_DITHER |
	      DISABLE_DEPTH_WRITE |
	      ENABLE_COLOR_MASK |
	      ENABLE_COLOR_WRITE |
	      ENABLE_TEX_CACHE);

	sna->render_state.gen2.need_invariant = FALSE;
}

static void
gen2_get_batch(struct sna *sna)
{
	kgem_set_mode(&sna->kgem, KGEM_RENDER);

	if (!kgem_check_batch(&sna->kgem, 28+40)) {
		DBG(("%s: flushing batch: size %d > %d\n",
		     __FUNCTION__, 28+40,
		     sna->kgem.surface-sna->kgem.nbatch));
		kgem_submit(&sna->kgem);
	}

	if (sna->kgem.nreloc + 3 > KGEM_RELOC_SIZE(&sna->kgem)) {
		DBG(("%s: flushing batch: reloc %d >= %d\n",
		     __FUNCTION__,
		     sna->kgem.nreloc + 3,
		     (int)KGEM_RELOC_SIZE(&sna->kgem)));
		kgem_submit(&sna->kgem);
	}

	if (sna->kgem.nexec + 3 > KGEM_EXEC_SIZE(&sna->kgem)) {
		DBG(("%s: flushing batch: exec %d >= %d\n",
		     __FUNCTION__,
		     sna->kgem.nexec + 1,
		     (int)KGEM_EXEC_SIZE(&sna->kgem)));
		kgem_submit(&sna->kgem);
	}

	if (sna->render_state.gen2.need_invariant)
		gen2_emit_invariant(sna);
}

static void gen2_emit_target(struct sna *sna, const struct sna_composite_op *op)
{
	assert (sna->render_state.gen2.vertex_offset == 0);

	if (sna->render_state.gen2.target == op->dst.bo->unique_id) {
		kgem_bo_mark_dirty(op->dst.bo);
		return;
	}

	BATCH(_3DSTATE_BUF_INFO_CMD);
	BATCH(BUF_3D_ID_COLOR_BACK |
	      gen2_buf_tiling(op->dst.bo->tiling) |
	      BUF_3D_PITCH(op->dst.bo->pitch));
	BATCH(kgem_add_reloc(&sna->kgem, sna->kgem.nbatch,
			     op->dst.bo,
			     I915_GEM_DOMAIN_RENDER << 16 |
			     I915_GEM_DOMAIN_RENDER,
			     0));

	BATCH(_3DSTATE_DST_BUF_VARS_CMD);
	BATCH(gen2_get_dst_format(op->dst.format));

	BATCH(_3DSTATE_DRAW_RECT_CMD);
	BATCH(0);
	BATCH(0);	/* ymin, xmin */
	BATCH(DRAW_YMAX(op->dst.height - 1) |
	      DRAW_XMAX(op->dst.width - 1));
	BATCH(0);	/* yorig, xorig */

	sna->render_state.gen2.target = op->dst.bo->unique_id;
}

static void gen2_disable_logic_op(struct sna *sna)
{
	if (!sna->render_state.gen2.logic_op_enabled)
		return;

	BATCH(_3DSTATE_ENABLES_1_CMD |
	      DISABLE_LOGIC_OP | ENABLE_COLOR_BLEND);

	sna->render_state.gen2.logic_op_enabled = 0;
}

static void gen2_enable_logic_op(struct sna *sna, int op)
{
	static const uint8_t logic_op[] = {
		LOGICOP_CLEAR,		/* GXclear */
		LOGICOP_AND,		/* GXand */
		LOGICOP_AND_RVRSE, 	/* GXandReverse */
		LOGICOP_COPY,		/* GXcopy */
		LOGICOP_AND_INV,	/* GXandInverted */
		LOGICOP_NOOP,		/* GXnoop */
		LOGICOP_XOR,		/* GXxor */
		LOGICOP_OR,		/* GXor */
		LOGICOP_NOR,		/* GXnor */
		LOGICOP_EQUIV,		/* GXequiv */
		LOGICOP_INV,		/* GXinvert */
		LOGICOP_OR_RVRSE,	/* GXorReverse */
		LOGICOP_COPY_INV,	/* GXcopyInverted */
		LOGICOP_OR_INV,		/* GXorInverted */
		LOGICOP_NAND,		/* GXnand */
		LOGICOP_SET		/* GXset */
	};

	if (sna->render_state.gen2.logic_op_enabled != op+1) {
		if (!sna->render_state.gen2.logic_op_enabled)
			BATCH(_3DSTATE_ENABLES_1_CMD |
			      ENABLE_LOGIC_OP | DISABLE_COLOR_BLEND);

		BATCH(_3DSTATE_MODES_4_CMD |
		      ENABLE_LOGIC_OP_FUNC | LOGIC_OP_FUNC(logic_op[op]));
		sna->render_state.gen2.logic_op_enabled = op+1;
	}
}

static void gen2_emit_composite_state(struct sna *sna,
				      const struct sna_composite_op *op)
{
	uint32_t texcoordfmt, v, unwind;
	uint32_t cblend, ablend;
	int tex;

	gen2_get_batch(sna);
	gen2_emit_target(sna, op);

	unwind = sna->kgem.nbatch;
	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	      I1_LOAD_S(2) | I1_LOAD_S(3) | I1_LOAD_S(8) | 2);
	BATCH((!op->src.is_solid + (op->mask.bo != NULL)) << 12);
	BATCH(S3_CULLMODE_NONE | S3_VERTEXHAS_XY);
	BATCH(S8_ENABLE_COLOR_BLEND | S8_BLENDFUNC_ADD |
	      gen2_get_blend_cntl(op->op,
				  op->has_component_alpha,
				  op->dst.format) |
	      S8_ENABLE_COLOR_BUFFER_WRITE);
	if (memcmp (sna->kgem.batch + sna->render_state.gen2.ls1,
		    sna->kgem.batch + unwind,
		    4 * sizeof(uint32_t)) == 0)
		sna->kgem.nbatch = unwind;
	else
		sna->render_state.gen2.ls1 = unwind;

	gen2_disable_logic_op(sna);

	gen2_get_blend_factors(op, op->op, &cblend, &ablend);
	unwind = sna->kgem.nbatch;
	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_2 |
	      LOAD_TEXTURE_BLEND_STAGE(0) | 1);
	BATCH(cblend);
	BATCH(ablend);
	if (memcmp (sna->kgem.batch + sna->render_state.gen2.ls2 + 1,
		    sna->kgem.batch + unwind + 1,
		    2 * sizeof(uint32_t)) == 0)
		sna->kgem.nbatch = unwind;
	else
		sna->render_state.gen2.ls2 = unwind;

	tex = texcoordfmt = 0;
	if (!op->src.is_solid) {
		if (op->src.is_affine)
			texcoordfmt |= TEXCOORDFMT_2D << (2*tex);
		else
			texcoordfmt |= TEXCOORDFMT_3D << (2*tex);
		gen2_emit_texture(sna, &op->src, tex++);
	} else {
		if (op->src.u.gen2.pixel != sna->render_state.gen2.diffuse) {
			BATCH(_3DSTATE_DFLT_DIFFUSE_CMD);
			BATCH(op->src.u.gen2.pixel);
			sna->render_state.gen2.diffuse = op->src.u.gen2.pixel;
		}
	}
	if (op->mask.bo) {
		if (op->mask.is_affine)
			texcoordfmt |= TEXCOORDFMT_2D << (2*tex);
		else
			texcoordfmt |= TEXCOORDFMT_3D << (2*tex);
		gen2_emit_texture(sna, &op->mask, tex++);
	}

	v = _3DSTATE_VERTEX_FORMAT_2_CMD | texcoordfmt;
	if (sna->render_state.gen2.vft != v) {
		BATCH(v);
		sna->render_state.gen2.vft = v;
	}
}

static inline void
gen2_emit_composite_dstcoord(struct sna *sna, int dstX, int dstY)
{
	VERTEX(dstX);
	VERTEX(dstY);
}

static void
gen2_emit_composite_texcoord(struct sna *sna,
			     const struct sna_composite_channel *channel,
			     int16_t x, int16_t y)
{
	float s = 0, t = 0, w = 1;

	x += channel->offset[0];
	y += channel->offset[1];

	if (channel->is_affine) {
		sna_get_transformed_coordinates(x, y,
						channel->transform,
						&s, &t);
		VERTEX(s * channel->scale[0]);
		VERTEX(t * channel->scale[1]);
	} else {
		sna_get_transformed_coordinates_3d(x, y,
						   channel->transform,
						   &s, &t, &w);
		VERTEX(s * channel->scale[0]);
		VERTEX(t * channel->scale[1]);
		VERTEX(w);
	}
}

static void
gen2_emit_composite_vertex(struct sna *sna,
			   const struct sna_composite_op *op,
			   int16_t srcX, int16_t srcY,
			   int16_t mskX, int16_t mskY,
			   int16_t dstX, int16_t dstY)
{
	gen2_emit_composite_dstcoord(sna, dstX, dstY);
	if (!op->src.is_solid)
		gen2_emit_composite_texcoord(sna, &op->src, srcX, srcY);
	if (op->mask.bo)
		gen2_emit_composite_texcoord(sna, &op->mask, mskX, mskY);
}

fastcall static void
gen2_emit_composite_primitive(struct sna *sna,
			      const struct sna_composite_op *op,
			      const struct sna_composite_rectangles *r)
{
	gen2_emit_composite_vertex(sna, op,
				   r->src.x + r->width,
				   r->src.y + r->height,
				   r->mask.x + r->width,
				   r->mask.y + r->height,
				   op->dst.x + r->dst.x + r->width,
				   op->dst.y + r->dst.y + r->height);
	gen2_emit_composite_vertex(sna, op,
				   r->src.x,
				   r->src.y + r->height,
				   r->mask.x,
				   r->mask.y + r->height,
				   op->dst.x + r->dst.x,
				   op->dst.y + r->dst.y + r->height);
	gen2_emit_composite_vertex(sna, op,
				   r->src.x,
				   r->src.y,
				   r->mask.x,
				   r->mask.y,
				   op->dst.x + r->dst.x,
				   op->dst.y + r->dst.y);
}

fastcall static void
gen2_emit_composite_primitive_constant(struct sna *sna,
				       const struct sna_composite_op *op,
				       const struct sna_composite_rectangles *r)
{
	int16_t dst_x = r->dst.x + op->dst.x;
	int16_t dst_y = r->dst.y + op->dst.y;

	gen2_emit_composite_dstcoord(sna, dst_x + r->width, dst_y + r->height);
	gen2_emit_composite_dstcoord(sna, dst_x, dst_y + r->height);
	gen2_emit_composite_dstcoord(sna, dst_x, dst_y);
}

fastcall static void
gen2_emit_composite_primitive_identity(struct sna *sna,
				       const struct sna_composite_op *op,
				       const struct sna_composite_rectangles *r)
{
	float w = r->width;
	float h = r->height;
	float *v;

	v = (float *)sna->kgem.batch + sna->kgem.nbatch;
	sna->kgem.nbatch += 12;

	v[8] = v[4] = r->dst.x + op->dst.x;
	v[0] = v[4] + w;

	v[9] = r->dst.y + op->dst.y;
	v[5] = v[1] = v[9] + h;

	v[10] = v[6] = (r->src.x + op->src.offset[0]) * op->src.scale[0];
	v[2] = v[6] + w * op->src.scale[0];

	v[11] = (r->src.y + op->src.offset[1]) * op->src.scale[1];
	v[7] = v[3] = v[11] + h * op->src.scale[1];
}

fastcall static void
gen2_emit_composite_primitive_affine(struct sna *sna,
				     const struct sna_composite_op *op,
				     const struct sna_composite_rectangles *r)
{
	PictTransform *transform = op->src.transform;
	int16_t dst_x = r->dst.x + op->dst.x;
	int16_t dst_y = r->dst.y + op->dst.y;
	int src_x = r->src.x + (int)op->src.offset[0];
	int src_y = r->src.y + (int)op->src.offset[1];
	float sx, sy;

	_sna_get_transformed_coordinates(src_x + r->width, src_y + r->height,
					 transform,
					 &sx, &sy);

	gen2_emit_composite_dstcoord(sna, dst_x + r->width, dst_y + r->height);
	VERTEX(sx * op->src.scale[0]);
	VERTEX(sy * op->src.scale[1]);

	_sna_get_transformed_coordinates(src_x, src_y + r->height,
					 transform,
					 &sx, &sy);
	gen2_emit_composite_dstcoord(sna, dst_x, dst_y + r->height);
	VERTEX(sx * op->src.scale[0]);
	VERTEX(sy * op->src.scale[1]);

	_sna_get_transformed_coordinates(src_x, src_y,
					 transform,
					 &sx, &sy);
	gen2_emit_composite_dstcoord(sna, dst_x, dst_y);
	VERTEX(sx * op->src.scale[0]);
	VERTEX(sy * op->src.scale[1]);
}

fastcall static void
gen2_emit_composite_primitive_constant_identity_mask(struct sna *sna,
						     const struct sna_composite_op *op,
						     const struct sna_composite_rectangles *r)
{
	float w = r->width;
	float h = r->height;
	float *v;

	v = (float *)sna->kgem.batch + sna->kgem.nbatch;
	sna->kgem.nbatch += 12;

	v[8] = v[4] = r->dst.x + op->dst.x;
	v[0] = v[4] + w;

	v[9] = r->dst.y + op->dst.y;
	v[5] = v[1] = v[9] + h;

	v[10] = v[6] = (r->mask.x + op->mask.offset[0]) * op->mask.scale[0];
	v[2] = v[6] + w * op->mask.scale[0];

	v[11] = (r->mask.y + op->mask.offset[1]) * op->mask.scale[1];
	v[7] = v[3] = v[11] + h * op->mask.scale[1];
}

static void gen2_magic_ca_pass(struct sna *sna,
			       const struct sna_composite_op *op)
{
	uint32_t ablend, cblend;

	if (!op->need_magic_ca_pass)
		return;

	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(8) | 0);
	BATCH(S8_ENABLE_COLOR_BLEND | S8_BLENDFUNC_ADD |
	      gen2_get_blend_cntl(PictOpAdd, TRUE, op->dst.format) |
	      S8_ENABLE_COLOR_BUFFER_WRITE);
	sna->render_state.gen2.ls1 = 0;

	gen2_get_blend_factors(op, PictOpAdd, &cblend, &ablend);
	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_2 |
	      LOAD_TEXTURE_BLEND_STAGE(0) | 1);
	BATCH(cblend);
	BATCH(ablend);
	sna->render_state.gen2.ls2 = 0;

	memcpy(sna->kgem.batch + sna->kgem.nbatch,
	       sna->kgem.batch + sna->render_state.gen2.vertex_offset,
	       (1 + sna->render.vertex_index)*sizeof(uint32_t));
	sna->kgem.nbatch += 1 + sna->render.vertex_index;
}

static void gen2_vertex_flush(struct sna *sna)
{
	if (sna->render.vertex_index == 0)
		return;

	sna->kgem.batch[sna->render_state.gen2.vertex_offset] |=
		sna->render.vertex_index - 1;

	if (sna->render.op)
		gen2_magic_ca_pass(sna, sna->render.op);

	sna->render_state.gen2.vertex_offset = 0;
	sna->render.vertex_index = 0;
}

inline static int gen2_get_rectangles(struct sna *sna,
				      const const struct sna_composite_op *op,
				      int want)
{
	struct gen2_render_state *state = &sna->render_state.gen2;
	int rem = batch_space(sna), size, need;

	DBG(("%s: want=%d, floats_per_vertex=%d, rem=%d\n",
	     __FUNCTION__, want, op->floats_per_vertex, rem));

	assert(op->floats_per_vertex);

	need = 1;
	size = op->floats_per_rect;
	if (op->need_magic_ca_pass)
		need += 6 + size*sna->render.vertex_index, size *= 2;

	DBG(("%s: want=%d, need=%d,size=%d, rem=%d\n",
	     __FUNCTION__, want, need, size, rem));
	if (rem < need + size) {
		kgem_submit (&sna->kgem);
		return 0;
	}

	rem -= need;
	if (state->vertex_offset == 0) {
		if ((sna->kgem.batch[sna->kgem.nbatch-1] & ~0xffff) ==
		    (PRIM3D_INLINE | PRIM3D_RECTLIST)) {
			uint32_t *b = &sna->kgem.batch[sna->kgem.nbatch-1];
			sna->render.vertex_index = 1 + (*b & 0xffff);
			*b = PRIM3D_INLINE | PRIM3D_RECTLIST;
			state->vertex_offset = sna->kgem.nbatch - 1;
			assert(!op->need_magic_ca_pass);
		} else {
			state->vertex_offset = sna->kgem.nbatch;
			BATCH(PRIM3D_INLINE | PRIM3D_RECTLIST);
		}
	}

	if (want > 1 && want * size > rem)
		want = rem / size;

	assert(want);
	sna->render.vertex_index += want*op->floats_per_rect;
	return want;
}

fastcall static void
gen2_render_composite_blt(struct sna *sna,
			  const struct sna_composite_op *op,
			  const struct sna_composite_rectangles *r)
{
	if (!gen2_get_rectangles(sna, op, 1)) {
		gen2_emit_composite_state(sna, op);
		gen2_get_rectangles(sna, op, 1);
	}

	op->prim_emit(sna, op, r);
}

fastcall static void
gen2_render_composite_box(struct sna *sna,
			  const struct sna_composite_op *op,
			  const BoxRec *box)
{
	struct sna_composite_rectangles r;

	if (!gen2_get_rectangles(sna, op, 1)) {
		gen2_emit_composite_state(sna, op);
		gen2_get_rectangles(sna, op, 1);
	}

	DBG(("  %s: (%d, %d) x (%d, %d)\n", __FUNCTION__,
	     box->x1, box->y1,
	     box->x2 - box->x1,
	     box->y2 - box->y1));

	r.dst.x  = box->x1; r.dst.y  = box->y1;
	r.width = box->x2 - box->x1;
	r.height = box->y2 - box->y1;
	r.src = r.mask = r.dst;

	op->prim_emit(sna, op, &r);
}

static void
gen2_render_composite_boxes(struct sna *sna,
			    const struct sna_composite_op *op,
			    const BoxRec *box, int nbox)
{
	do {
		int nbox_this_time;

		nbox_this_time = gen2_get_rectangles(sna, op, nbox);
		if (nbox_this_time == 0) {
			gen2_emit_composite_state(sna, op);
			nbox_this_time = gen2_get_rectangles(sna, op, nbox);
		}
		nbox -= nbox_this_time;

		do {
			struct sna_composite_rectangles r;

			DBG(("  %s: (%d, %d) x (%d, %d)\n", __FUNCTION__,
			     box->x1, box->y1,
			     box->x2 - box->x1,
			     box->y2 - box->y1));

			r.dst.x  = box->x1; r.dst.y  = box->y1;
			r.width = box->x2 - box->x1;
			r.height = box->y2 - box->y1;
			r.src = r.mask = r.dst;

			op->prim_emit(sna, op, &r);
			box++;
		} while (--nbox_this_time);
	} while (nbox);
}

static void gen2_render_composite_done(struct sna *sna,
				       const struct sna_composite_op *op)
{
	gen2_vertex_flush(sna);
	sna->render.op = NULL;
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);

	sna_render_composite_redirect_done(sna, op);

	if (op->src.bo)
		kgem_bo_destroy(&sna->kgem, op->src.bo);
	if (op->mask.bo)
		kgem_bo_destroy(&sna->kgem, op->mask.bo);
}

static Bool
gen2_composite_solid_init(struct sna *sna,
			  struct sna_composite_channel *channel,
			  uint32_t color)
{
	channel->filter = PictFilterNearest;
	channel->repeat = RepeatNormal;
	channel->is_affine = TRUE;
	channel->is_solid  = TRUE;
	channel->transform = NULL;
	channel->width  = 1;
	channel->height = 1;
	channel->pict_format = PICT_a8r8g8b8;

	channel->bo = sna_render_get_solid(sna, color);
	channel->u.gen2.pixel = color;

	channel->scale[0]  = channel->scale[1]  = 1;
	channel->offset[0] = channel->offset[1] = 0;
	return channel->bo != NULL;
}

static Bool source_is_covered(PicturePtr picture,
			      int x, int y,
			      int width, int height)
{
	int x1, y1, x2, y2;

	if (picture->repeat && picture->repeatType != RepeatNone)
		return TRUE;

	if (picture->pDrawable == NULL)
		return FALSE;

	if (picture->transform) {
		pixman_box16_t sample;

		sample.x1 = x;
		sample.y1 = y;
		sample.x2 = x + width;
		sample.y2 = y + height;

		pixman_transform_bounds(picture->transform, &sample);

		x1 = sample.x1;
		x2 = sample.x2;
		y1 = sample.y1;
		y2 = sample.y2;
	} else {
		x1 = x;
		y1 = y;
		x2 = x + width;
		y2 = y + height;
	}

	return
		x1 >= 0 && y1 >= 0 &&
		x2 <= picture->pDrawable->width &&
		y2 <= picture->pDrawable->height;
}

static Bool
gen2_check_card_format(struct sna *sna,
		       PicturePtr picture,
		       struct sna_composite_channel *channel,
		       int x, int y, int w, int h)
{
	uint32_t format = picture->format;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(i8xx_tex_formats); i++) {
		if (i8xx_tex_formats[i].fmt == format)
			return TRUE;
	}

	for (i = 0; i < ARRAY_SIZE(i85x_tex_formats); i++) {
		if (i85x_tex_formats[i].fmt == format) {
			if (sna->kgem.gen >= 21)
				return TRUE;

			if ( source_is_covered(picture, x, y, w,h)) {
				channel->is_opaque = true;
				return TRUE;
			}

			return FALSE;
		}
	}

	return FALSE;
}

static int
gen2_composite_picture(struct sna *sna,
		       PicturePtr picture,
		       struct sna_composite_channel *channel,
		       int x, int y,
		       int w, int h,
		       int dst_x, int dst_y)
{
	PixmapPtr pixmap;
	uint32_t color;
	int16_t dx, dy;

	DBG(("%s: (%d, %d)x(%d, %d), dst=(%d, %d)\n",
	     __FUNCTION__, x, y, w, h, dst_x, dst_y));

	channel->is_solid = FALSE;
	channel->card_format = -1;

	if (sna_picture_is_solid(picture, &color))
		return gen2_composite_solid_init(sna, channel, color);

	if (picture->pDrawable == NULL) {
		DBG(("%s -- fallback, unhandled source %d\n",
		     __FUNCTION__, picture->pSourcePict->type));
		return sna_render_picture_fixup(sna, picture, channel,
						x, y, w, h, dst_x, dst_y);
	}

	if (!gen2_check_repeat(picture)) {
		DBG(("%s -- fallback, unhandled repeat %d\n",
		     __FUNCTION__, picture->repeat));
		return sna_render_picture_fixup(sna, picture, channel,
						x, y, w, h, dst_x, dst_y);
	}

	if (!gen2_check_filter(picture)) {
		DBG(("%s -- fallback, unhandled filter %d\n",
		     __FUNCTION__, picture->filter));
		return sna_render_picture_fixup(sna, picture, channel,
						x, y, w, h, dst_x, dst_y);
	}

	channel->repeat = picture->repeat ? picture->repeatType : RepeatNone;
	channel->filter = picture->filter;

	pixmap = get_drawable_pixmap(picture->pDrawable);
	get_drawable_deltas(picture->pDrawable, pixmap, &dx, &dy);

	x += dx + picture->pDrawable->x;
	y += dy + picture->pDrawable->y;

	channel->is_affine = sna_transform_is_affine(picture->transform);
	if (sna_transform_is_integer_translation(picture->transform, &dx, &dy)) {
		DBG(("%s: integer translation (%d, %d), removing\n",
		     __FUNCTION__, dx, dy));
		x += dx;
		y += dy;
		channel->transform = NULL;
		channel->filter = PictFilterNearest;
	} else
		channel->transform = picture->transform;

	if (!gen2_check_card_format(sna, picture, channel, x,  y, w ,h))
		return sna_render_picture_convert(sna, picture, channel, pixmap,
						  x, y, w, h, dst_x, dst_y);

	channel->pict_format = picture->format;
	if (pixmap->drawable.width > 2048 || pixmap->drawable.height > 2048)
		return sna_render_picture_extract(sna, picture, channel,
						  x, y, w, h, dst_x, dst_y);

	return sna_render_pixmap_bo(sna, channel, pixmap,
				    x, y, w, h, dst_x, dst_y);
}

static Bool
gen2_composite_set_target(struct sna_composite_op *op,
			  PicturePtr dst)
{
	struct sna_pixmap *priv;

	op->dst.pixmap = get_drawable_pixmap(dst->pDrawable);
	op->dst.format = dst->format;
	op->dst.width  = op->dst.pixmap->drawable.width;
	op->dst.height = op->dst.pixmap->drawable.height;

	priv = sna_pixmap_force_to_gpu(op->dst.pixmap);
	if (priv == NULL)
		return FALSE;

	op->dst.bo = priv->gpu_bo;
	if (!priv->gpu_only)
		op->damage = &priv->gpu_damage;

	get_drawable_deltas(dst->pDrawable, op->dst.pixmap,
			    &op->dst.x, &op->dst.y);
	return TRUE;
}

static Bool
try_blt(struct sna *sna,
	PicturePtr source,
	int width, int height)
{
	uint32_t color;

	if (sna->kgem.mode != KGEM_RENDER) {
		DBG(("%s: already performing BLT\n", __FUNCTION__));
		return TRUE;
	}

	if (width > 2048 || height > 2048) {
		DBG(("%s: operation too large for 3D pipe (%d, %d)\n",
		     __FUNCTION__, width, height));
		return TRUE;
	}

	/* If it is a solid, try to use the BLT paths */
	if (sna_picture_is_solid(source, &color))
		return TRUE;

	if (!source->pDrawable)
		return FALSE;

	return is_cpu(source->pDrawable);
}

static Bool
gen2_render_composite(struct sna *sna,
		      uint8_t op,
		      PicturePtr src,
		      PicturePtr mask,
		      PicturePtr dst,
		      int16_t src_x,  int16_t src_y,
		      int16_t mask_x, int16_t mask_y,
		      int16_t dst_x,  int16_t dst_y,
		      int16_t width,  int16_t height,
		      struct sna_composite_op *tmp)
{
	DBG(("%s()\n", __FUNCTION__));

	/* Try to use the BLT engine unless it implies a
	 * 3D -> 2D context switch.
	 */
	if (mask == NULL &&
	    try_blt(sna, src, width, height) &&
	    sna_blt_composite(sna,
			      op, src, dst,
			      src_x, src_y,
			      dst_x, dst_y,
			      width, height,
			      tmp))
		return TRUE;

	if (op >= ARRAY_SIZE(gen2_blend_op)) {
		DBG(("%s: fallback due to unhandled blend op: %d\n",
		     __FUNCTION__, op));
		return FALSE;
	}

	if (!gen2_check_dst_format(dst->format)) {
		DBG(("%s: fallback due to unhandled dst format: %x\n",
		     __FUNCTION__, dst->format));
		return FALSE;
	}

	if (need_tiling(sna, width, height))
		return sna_tiling_composite(op, src, mask, dst,
					    src_x,  src_y,
					    mask_x, mask_y,
					    dst_x,  dst_y,
					    width,  height,
					    tmp);

	if (!gen2_composite_set_target(tmp, dst)) {
		DBG(("%s: unable to set render target\n",
		     __FUNCTION__));
		return FALSE;
	}

	tmp->op = op;
	if (tmp->dst.width > 2048 ||
	    tmp->dst.height > 2048 ||
	    tmp->dst.bo->pitch > 8192) {
		if (!sna_render_composite_redirect(sna, tmp,
						   dst_x, dst_y, width, height))
			return FALSE;
	}

	switch (gen2_composite_picture(sna, src, &tmp->src,
				       src_x, src_y,
				       width, height,
				       dst_x, dst_y)) {
	case -1:
		goto cleanup_dst;
	case 0:
		gen2_composite_solid_init(sna, &tmp->src, 0);
	case 1:
		break;
	}

	if (mask) {
		switch (gen2_composite_picture(sna, mask, &tmp->mask,
					       mask_x, mask_y,
					       width,  height,
					       dst_x,  dst_y)) {
		case -1:
			goto cleanup_src;
		case 0:
			gen2_composite_solid_init(sna, &tmp->mask, 0);
		case 1:
			break;
		}

		if (mask->componentAlpha && PICT_FORMAT_RGB(mask->format)) {
			/* Check if it's component alpha that relies on a source alpha
			 * and on the source value.  We can only get one of those
			 * into the single source value that we get to blend with.
			 */
			tmp->has_component_alpha = TRUE;
			if (gen2_blend_op[op].src_alpha &&
			    (gen2_blend_op[op].src_blend != BLENDFACTOR_ZERO)) {
				if (op != PictOpOver)
					return FALSE;

				tmp->need_magic_ca_pass = TRUE;
				tmp->op = PictOpOutReverse;
			}
		}
	}

	tmp->floats_per_vertex = 2;
	if (!tmp->src.is_solid)
		tmp->floats_per_vertex += tmp->src.is_affine ? 2 : 3;
	if (tmp->mask.bo)
		tmp->floats_per_vertex += tmp->mask.is_affine ? 2 : 3;
	tmp->floats_per_rect = 3*tmp->floats_per_vertex;

	tmp->prim_emit = gen2_emit_composite_primitive;
	if (tmp->mask.bo) {
		if (tmp->mask.transform == NULL) {
			if (tmp->src.is_solid)
				tmp->prim_emit = gen2_emit_composite_primitive_constant_identity_mask;
		}
	} else {
		if (tmp->src.is_solid)
			tmp->prim_emit = gen2_emit_composite_primitive_constant;
		else if (tmp->src.transform == NULL)
			tmp->prim_emit = gen2_emit_composite_primitive_identity;
		else if (tmp->src.is_affine)
			tmp->prim_emit = gen2_emit_composite_primitive_affine;
	}

	tmp->blt   = gen2_render_composite_blt;
	tmp->box   = gen2_render_composite_box;
	tmp->boxes = gen2_render_composite_boxes;
	tmp->done  = gen2_render_composite_done;

	if (!kgem_check_bo(&sna->kgem,
			   tmp->dst.bo, tmp->src.bo, tmp->mask.bo,
			   NULL))
		kgem_submit(&sna->kgem);

	if (kgem_bo_is_dirty(tmp->src.bo) || kgem_bo_is_dirty(tmp->mask.bo)) {
		if (tmp->src.bo == tmp->dst.bo || tmp->mask.bo == tmp->dst.bo) {
			kgem_emit_flush(&sna->kgem);
		} else {
			BATCH(_3DSTATE_MODES_5_CMD |
			      PIPELINE_FLUSH_RENDER_CACHE |
			      PIPELINE_FLUSH_TEXTURE_CACHE);
			kgem_clear_dirty(&sna->kgem);
		}
	}

	gen2_emit_composite_state(sna, tmp);

	sna->render.op = tmp;
	return TRUE;

cleanup_src:
	if (tmp->src.bo)
		kgem_bo_destroy(&sna->kgem, tmp->src.bo);
cleanup_dst:
	if (tmp->redirect.real_bo)
		kgem_bo_destroy(&sna->kgem, tmp->dst.bo);
	return FALSE;
}

fastcall static void
gen2_emit_composite_spans_primitive_constant(struct sna *sna,
					     const struct sna_composite_spans_op *op,
					     const BoxRec *box,
					     float opacity)
{
	float *v = (float *)sna->kgem.batch + sna->kgem.nbatch;
	uint32_t alpha = (uint8_t)(255 * opacity) << 24;
	sna->kgem.nbatch += 9;

	v[0] = op->base.dst.x + box->x2;
	v[1] = op->base.dst.y + box->y2;
	*((uint32_t *)v + 2) = alpha;

	v[3] = op->base.dst.x + box->x1;
	v[4] = v[1];
	*((uint32_t *)v + 5) = alpha;

	v[6] = v[3];
	v[7] = op->base.dst.y + box->y1;
	*((uint32_t *)v + 8) = alpha;
}

fastcall static void
gen2_emit_composite_spans_primitive_identity_source(struct sna *sna,
						    const struct sna_composite_spans_op *op,
						    const BoxRec *box,
						    float opacity)
{
	float *v = (float *)sna->kgem.batch + sna->kgem.nbatch;
	uint32_t alpha = (uint8_t)(255 * opacity) << 24;
	sna->kgem.nbatch += 15;

	v[0] = op->base.dst.x + box->x2;
	v[1] = op->base.dst.y + box->y2;
	*((uint32_t *)v + 2) = alpha;
	v[3] = (op->base.src.offset[0] + box->x2) * op->base.src.scale[0];
	v[4] = (op->base.src.offset[1] + box->y2) * op->base.src.scale[1];

	v[5] = op->base.dst.x + box->x1;
	v[6] = v[1];
	*((uint32_t *)v + 7) = alpha;
	v[8] = (op->base.src.offset[0] + box->x1) * op->base.src.scale[0];
	v[9] = v[4];

	v[10] = v[5];
	v[11] = op->base.dst.y + box->y1;
	*((uint32_t *)v + 12) = alpha;
	v[13] = v[8];
	v[14] = (op->base.src.offset[1] + box->y1) * op->base.src.scale[1];
}

fastcall static void
gen2_emit_composite_spans_primitive_affine_source(struct sna *sna,
						  const struct sna_composite_spans_op *op,
						  const BoxRec *box,
						  float opacity)
{
	PictTransform *transform = op->base.src.transform;
	uint32_t alpha = (uint8_t)(255 * opacity) << 24;
	float x, y, *v;
       
	v = (float *)sna->kgem.batch + sna->kgem.nbatch;
	sna->kgem.nbatch += 15;

	v[0]  = op->base.dst.x + box->x2;
	v[6]  = v[1] = op->base.dst.y + box->y2;
	v[10] = v[5] = op->base.dst.x + box->x1;
	v[11] = op->base.dst.y + box->y1;
	*((uint32_t *)v + 2) = alpha;
	*((uint32_t *)v + 7) = alpha;
	*((uint32_t *)v + 12) = alpha;

	_sna_get_transformed_coordinates((int)op->base.src.offset[0] + box->x2,
					 (int)op->base.src.offset[1] + box->y2,
					 transform,
					 &x, &y);
	v[3] = x * op->base.src.scale[0];
	v[4] = y * op->base.src.scale[1];

	_sna_get_transformed_coordinates((int)op->base.src.offset[0] + box->x1,
					 (int)op->base.src.offset[1] + box->y2,
					 transform,
					 &x, &y);
	v[8] = x * op->base.src.scale[0];
	v[9] = y * op->base.src.scale[1];

	_sna_get_transformed_coordinates((int)op->base.src.offset[0] + box->x1,
					 (int)op->base.src.offset[1] + box->y1,
					 transform,
					 &x, &y);
	v[13] = x * op->base.src.scale[0];
	v[14] = y * op->base.src.scale[1];
}

static void
gen2_emit_composite_spans_vertex(struct sna *sna,
				 const struct sna_composite_spans_op *op,
				 int16_t x, int16_t y,
				 float opacity)
{
	gen2_emit_composite_dstcoord(sna, x + op->base.dst.x, y + op->base.dst.y);
	BATCH((uint8_t)(opacity * 255) << 24);
	gen2_emit_composite_texcoord(sna, &op->base.src, x, y);
}

fastcall static void
gen2_emit_composite_spans_primitive(struct sna *sna,
				    const struct sna_composite_spans_op *op,
				    const BoxRec *box,
				    float opacity)
{
	gen2_emit_composite_spans_vertex(sna, op, box->x2, box->y2, opacity);
	gen2_emit_composite_spans_vertex(sna, op, box->x1, box->y2, opacity);
	gen2_emit_composite_spans_vertex(sna, op, box->x1, box->y1, opacity);
}

static void
gen2_emit_spans_pipeline(struct sna *sna,
			 const struct sna_composite_spans_op *op)
{
	uint32_t cblend, ablend;
	uint32_t unwind;

	cblend =
		TB0C_LAST_STAGE | TB0C_RESULT_SCALE_1X | TB0C_OP_MODULATE |
	       	TB0C_ARG1_SEL_DIFFUSE | TB0C_ARG1_REPLICATE_ALPHA |
		TB0C_OUTPUT_WRITE_CURRENT;
	ablend =
		TB0A_RESULT_SCALE_1X | TB0A_OP_MODULATE |
		TB0A_ARG1_SEL_DIFFUSE |
		TB0A_OUTPUT_WRITE_CURRENT;

	if (op->base.src.is_solid) {
		ablend |= TB0A_ARG2_SEL_SPECULAR;
		cblend |= TB0C_ARG2_SEL_SPECULAR;
		if (op->base.dst.format == PICT_a8)
			cblend |= TB0C_ARG2_REPLICATE_ALPHA;
	} else if (op->base.dst.format == PICT_a8) {
		ablend |= TB0A_ARG2_SEL_TEXEL0;
		cblend |= TB0C_ARG2_SEL_TEXEL0 | TB0C_ARG2_REPLICATE_ALPHA;
	} else {
		if (PICT_FORMAT_RGB(op->base.src.pict_format) != 0)
			cblend |= TB0C_ARG2_SEL_TEXEL0;
		else
			cblend |= TB0C_ARG2_SEL_ONE | TB0C_ARG2_INVERT;

		if (op->base.src.is_opaque)
			ablend |= TB0A_ARG2_SEL_ONE;
		else
			ablend |= TB0A_ARG2_SEL_TEXEL0;
	}

	unwind = sna->kgem.nbatch;
	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_2 |
	      LOAD_TEXTURE_BLEND_STAGE(0) | 1);
	BATCH(cblend);
	BATCH(ablend);
	if (memcmp (sna->kgem.batch + sna->render_state.gen2.ls2 + 1,
		    sna->kgem.batch + unwind + 1,
		    2 * sizeof(uint32_t)) == 0)
		sna->kgem.nbatch = unwind;
	else
		sna->render_state.gen2.ls2 = unwind;
}

static void gen2_emit_composite_spans_state(struct sna *sna,
					    const struct sna_composite_spans_op *op)
{
	uint32_t unwind;

	gen2_get_batch(sna);
	gen2_emit_target(sna, &op->base);

	unwind = sna->kgem.nbatch;
	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	      I1_LOAD_S(2) | I1_LOAD_S(3) | I1_LOAD_S(8) | 2);
	BATCH(!op->base.src.is_solid << 12);
	BATCH(S3_CULLMODE_NONE | S3_VERTEXHAS_XY | S3_DIFFUSE_PRESENT);
	BATCH(S8_ENABLE_COLOR_BLEND | S8_BLENDFUNC_ADD |
	      gen2_get_blend_cntl(op->base.op, FALSE, op->base.dst.format) |
	      S8_ENABLE_COLOR_BUFFER_WRITE);
	if (memcmp (sna->kgem.batch + sna->render_state.gen2.ls1,
		    sna->kgem.batch + unwind,
		    4 * sizeof(uint32_t)) == 0)
		sna->kgem.nbatch = unwind;
	else
		sna->render_state.gen2.ls1 = unwind;

	gen2_disable_logic_op(sna);
	gen2_emit_spans_pipeline(sna, op);

	if (op->base.src.is_solid) {
		BATCH(_3DSTATE_DFLT_SPECULAR_CMD);
		BATCH(op->base.src.u.gen2.pixel);
	} else {
		uint32_t v =_3DSTATE_VERTEX_FORMAT_2_CMD |
			(op->base.src.is_affine ? TEXCOORDFMT_2D : TEXCOORDFMT_3D);
		if (sna->render_state.gen2.vft != v) {
			BATCH(v);
			sna->render_state.gen2.vft = v;
		}
		gen2_emit_texture(sna, &op->base.src, 0);
	}
}

fastcall static void
gen2_render_composite_spans_box(struct sna *sna,
				const struct sna_composite_spans_op *op,
				const BoxRec *box, float opacity)
{
	DBG(("%s: src=+(%d, %d), opacity=%f, dst=+(%d, %d), box=(%d, %d) x (%d, %d)\n",
	     __FUNCTION__,
	     op->base.src.offset[0], op->base.src.offset[1],
	     opacity,
	     op->base.dst.x, op->base.dst.y,
	     box->x1, box->y1,
	     box->x2 - box->x1,
	     box->y2 - box->y1));

	if (gen2_get_rectangles(sna, &op->base, 1) == 0) {
		gen2_emit_composite_spans_state(sna, op);
		gen2_get_rectangles(sna, &op->base, 1);
	}

	op->prim_emit(sna, op, box, opacity);
}

static void
gen2_render_composite_spans_boxes(struct sna *sna,
				  const struct sna_composite_spans_op *op,
				  const BoxRec *box, int nbox,
				  float opacity)
{
	DBG(("%s: nbox=%d, src=+(%d, %d), opacity=%f, dst=+(%d, %d)\n",
	     __FUNCTION__, nbox,
	     op->base.src.offset[0], op->base.src.offset[1],
	     opacity,
	     op->base.dst.x, op->base.dst.y));

	do {
		int nbox_this_time;

		nbox_this_time = gen2_get_rectangles(sna, &op->base, nbox);
		if (nbox_this_time == 0) {
			gen2_emit_composite_spans_state(sna, op);
			nbox_this_time = gen2_get_rectangles(sna, &op->base, nbox);
		}
		nbox -= nbox_this_time;

		do {
			DBG(("  %s: (%d, %d) x (%d, %d)\n", __FUNCTION__,
			     box->x1, box->y1,
			     box->x2 - box->x1,
			     box->y2 - box->y1));

			op->prim_emit(sna, op, box++, opacity);
		} while (--nbox_this_time);
	} while (nbox);
}

fastcall static void
gen2_render_composite_spans_done(struct sna *sna,
				 const struct sna_composite_spans_op *op)
{
	gen2_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);

	DBG(("%s()\n", __FUNCTION__));

	sna_render_composite_redirect_done(sna, &op->base);
	if (op->base.src.bo)
		kgem_bo_destroy(&sna->kgem, op->base.src.bo);
}

static Bool
gen2_render_composite_spans(struct sna *sna,
			    uint8_t op,
			    PicturePtr src,
			    PicturePtr dst,
			    int16_t src_x,  int16_t src_y,
			    int16_t dst_x,  int16_t dst_y,
			    int16_t width,  int16_t height,
			    struct sna_composite_spans_op *tmp)
{
	DBG(("%s(src=(%d, %d), dst=(%d, %d), size=(%d, %d))\n", __FUNCTION__,
	     src_x, src_y, dst_x, dst_y, width, height));

#if NO_COMPOSITE_SPANS
	return FALSE;
#endif

	if (op >= ARRAY_SIZE(gen2_blend_op)) {
		DBG(("%s: fallback due to unhandled blend op: %d\n",
		     __FUNCTION__, op));
		return FALSE;
	}

	if (!gen2_check_dst_format(dst->format)) {
		DBG(("%s: fallback due to unhandled dst format: %x\n",
		     __FUNCTION__, dst->format));
		return FALSE;
	}

	if (need_tiling(sna, width, height))
		return FALSE;

	if (!gen2_composite_set_target(&tmp->base, dst)) {
		DBG(("%s: unable to set render target\n",
		     __FUNCTION__));
		return FALSE;
	}

	tmp->base.op = op;
	if (tmp->base.dst.width > 2048 ||
	    tmp->base.dst.height > 2048 ||
	    tmp->base.dst.bo->pitch > 8192) {
		if (!sna_render_composite_redirect(sna, &tmp->base,
						   dst_x, dst_y, width, height))
			return FALSE;
	}

	switch (gen2_composite_picture(sna, src, &tmp->base.src,
				       src_x, src_y,
				       width, height,
				       dst_x, dst_y)) {
	case -1:
		goto cleanup_dst;
	case 0:
		gen2_composite_solid_init(sna, &tmp->base.src, 0);
	case 1:
		break;
	}

	tmp->prim_emit = gen2_emit_composite_spans_primitive;
	tmp->base.floats_per_vertex = 3;
	if (tmp->base.src.is_solid) {
		tmp->prim_emit = gen2_emit_composite_spans_primitive_constant;
	} else {
		assert(tmp->base.src.bo);
		tmp->base.floats_per_vertex += tmp->base.src.is_affine ? 2 : 3;
		if (tmp->base.src.transform == NULL)
			tmp->prim_emit = gen2_emit_composite_spans_primitive_identity_source;
		else if (tmp->base.src.is_affine)
			tmp->prim_emit = gen2_emit_composite_spans_primitive_affine_source;

		if (kgem_bo_is_dirty(tmp->base.src.bo)) {
			if (tmp->base.src.bo == tmp->base.dst.bo) {
				kgem_emit_flush(&sna->kgem);
			} else {
				BATCH(_3DSTATE_MODES_5_CMD |
				      PIPELINE_FLUSH_RENDER_CACHE |
				      PIPELINE_FLUSH_TEXTURE_CACHE);
				kgem_clear_dirty(&sna->kgem);
			}
		}
	}
	tmp->base.floats_per_rect = 3*tmp->base.floats_per_vertex;

	tmp->box   = gen2_render_composite_spans_box;
	tmp->boxes = gen2_render_composite_spans_boxes;
	tmp->done  = gen2_render_composite_spans_done;

	if (!kgem_check_bo(&sna->kgem,
			   tmp->base.dst.bo, tmp->base.src.bo,
			   NULL))
		kgem_submit(&sna->kgem);

	gen2_emit_composite_spans_state(sna, tmp);
	return TRUE;

cleanup_dst:
	if (tmp->base.redirect.real_bo)
		kgem_bo_destroy(&sna->kgem, tmp->base.dst.bo);
	return FALSE;
}

static void
gen2_emit_fill_pipeline(struct sna *sna, const struct sna_composite_op *op)
{
	uint32_t blend, unwind;

	unwind = sna->kgem.nbatch;
	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_2 |
	      LOAD_TEXTURE_BLEND_STAGE(0) | 1);

	blend = TB0C_LAST_STAGE | TB0C_RESULT_SCALE_1X | TB0C_OP_ARG1 |
		TB0C_ARG1_SEL_DIFFUSE |
		TB0C_OUTPUT_WRITE_CURRENT;
	if (op->dst.format == PICT_a8)
		blend |= TB0C_ARG1_REPLICATE_ALPHA;
	BATCH(blend);

	BATCH(TB0A_RESULT_SCALE_1X | TB0A_OP_ARG1 |
	      TB0A_ARG1_SEL_DIFFUSE |
	      TB0A_OUTPUT_WRITE_CURRENT);

	if (memcmp (sna->kgem.batch + sna->render_state.gen2.ls2 + 1,
		    sna->kgem.batch + unwind + 1,
		    2 * sizeof(uint32_t)) == 0)
		sna->kgem.nbatch = unwind;
	else
		sna->render_state.gen2.ls2 = unwind;
}

static void gen2_emit_fill_composite_state(struct sna *sna,
					   const struct sna_composite_op *op,
					   uint32_t pixel)
{
	uint32_t ls1;

	gen2_get_batch(sna);
	gen2_emit_target(sna, op);

	ls1 = sna->kgem.nbatch;
	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	      I1_LOAD_S(2) | I1_LOAD_S(3) | I1_LOAD_S(8) | 2);
	BATCH(0);
	BATCH(S3_CULLMODE_NONE | S3_VERTEXHAS_XY);
	BATCH(S8_ENABLE_COLOR_BLEND | S8_BLENDFUNC_ADD |
	      gen2_get_blend_cntl(op->op, FALSE, op->dst.format) |
	      S8_ENABLE_COLOR_BUFFER_WRITE);
	if (memcmp (sna->kgem.batch + sna->render_state.gen2.ls1,
		    sna->kgem.batch + ls1,
		    4 * sizeof(uint32_t)) == 0)
		sna->kgem.nbatch = ls1;
	else
		sna->render_state.gen2.ls1 = ls1;

	gen2_emit_fill_pipeline(sna, op);

	if (pixel != sna->render_state.gen2.diffuse) {
		BATCH(_3DSTATE_DFLT_DIFFUSE_CMD);
		BATCH(pixel);
		sna->render_state.gen2.diffuse = pixel;
	}
}

static Bool
gen2_render_fill_boxes_try_blt(struct sna *sna,
			       CARD8 op, PictFormat format,
			       const xRenderColor *color,
			       PixmapPtr dst, struct kgem_bo *dst_bo,
			       const BoxRec *box, int n)
{
	uint8_t alu = GXcopy;
	uint32_t pixel;

	if (!sna_get_pixel_from_rgba(&pixel,
				     color->red,
				     color->green,
				     color->blue,
				     color->alpha,
				     format))
		return FALSE;

	if (op == PictOpClear) {
		alu = GXclear;
		pixel = 0;
		op = PictOpSrc;
	}

	if (op == PictOpOver) {
		if ((pixel & 0xff000000) == 0xff000000)
			op = PictOpSrc;
	}

	if (op != PictOpSrc)
		return FALSE;

	return sna_blt_fill_boxes(sna, alu,
				  dst_bo, dst->drawable.bitsPerPixel,
				  pixel, box, n);
}

static inline Bool prefer_blt_fill(struct sna *sna)
{
#if PREFER_BLT_FILL
	return true;
#else
	return sna->kgem.mode != KGEM_RENDER;
#endif
}

static inline Bool prefer_blt_copy(struct sna *sna)
{
#if PREFER_BLT_COPY
	return true;
#else
	return sna->kgem.mode != KGEM_RENDER;
#endif
}

static Bool
gen2_render_fill_boxes(struct sna *sna,
		       CARD8 op,
		       PictFormat format,
		       const xRenderColor *color,
		       PixmapPtr dst, struct kgem_bo *dst_bo,
		       const BoxRec *box, int n)
{
	struct sna_composite_op tmp;
	uint32_t pixel;

#if NO_FILL_BOXES
	return gen2_render_fill_boxes_try_blt(sna, op, format, color,
					      dst, dst_bo,
					      box, n);
#endif

	DBG(("%s (op=%d, format=%x, color=(%04x,%04x,%04x, %04x))\n",
	     __FUNCTION__, op, (int)format,
	     color->red, color->green, color->blue, color->alpha));

	if (op >= ARRAY_SIZE(gen2_blend_op)) {
		DBG(("%s: fallback due to unhandled blend op: %d\n",
		     __FUNCTION__, op));
		return FALSE;
	}

	if (dst->drawable.width > 2048 ||
	    dst->drawable.height > 2048 ||
	    dst_bo->pitch > 8192 ||
	    !gen2_check_dst_format(format))
		return gen2_render_fill_boxes_try_blt(sna, op, format, color,
						      dst, dst_bo,
						      box, n);

	if (prefer_blt_fill(sna) &&
	    gen2_render_fill_boxes_try_blt(sna, op, format, color,
					   dst, dst_bo,
					   box, n))
		return TRUE;

	if (!sna_get_pixel_from_rgba(&pixel,
				     color->red,
				     color->green,
				     color->blue,
				     color->alpha,
				     PICT_a8r8g8b8))
		return FALSE;

	DBG(("%s: using shader for op=%d, format=%x, pixel=%x\n",
	     __FUNCTION__, op, (int)format, pixel));

	if (pixel == 0)
		op = PictOpClear;

	memset(&tmp, 0, sizeof(tmp));
	tmp.op = op;
	tmp.dst.pixmap = dst;
	tmp.dst.width = dst->drawable.width;
	tmp.dst.height = dst->drawable.height;
	tmp.dst.format = format;
	tmp.dst.bo = dst_bo;
	tmp.floats_per_vertex = 2;
	tmp.floats_per_rect = 6;

	if (!kgem_check_bo(&sna->kgem, dst_bo, NULL))
		kgem_submit(&sna->kgem);

	gen2_emit_fill_composite_state(sna, &tmp, pixel);

	do {
		int n_this_time = gen2_get_rectangles(sna, &tmp, n);
		if (n_this_time == 0) {
			gen2_emit_fill_composite_state(sna, &tmp, pixel);
			n_this_time = gen2_get_rectangles(sna, &tmp, n);
		}
		n -= n_this_time;

		do {
			DBG(("	(%d, %d), (%d, %d): %x\n",
			     box->x1, box->y1, box->x2, box->y2, pixel));
			VERTEX(box->x2);
			VERTEX(box->y2);
			VERTEX(box->x1);
			VERTEX(box->y2);
			VERTEX(box->x1);
			VERTEX(box->y1);
			box++;
		} while (--n_this_time);
	} while (n);

	gen2_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
	return TRUE;
}

static void gen2_emit_fill_state(struct sna *sna,
				 const struct sna_composite_op *op)
{
	uint32_t ls1;

	gen2_get_batch(sna);
	gen2_emit_target(sna, op);

	ls1 = sna->kgem.nbatch;
	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	      I1_LOAD_S(2) | I1_LOAD_S(3) | I1_LOAD_S(8) | 2);
	BATCH(0);
	BATCH(S3_CULLMODE_NONE | S3_VERTEXHAS_XY);
	BATCH(S8_ENABLE_COLOR_BUFFER_WRITE);
	if (memcmp (sna->kgem.batch + sna->render_state.gen2.ls1,
		    sna->kgem.batch + ls1,
		    4 * sizeof(uint32_t)) == 0)
		sna->kgem.nbatch = ls1;
	else
		sna->render_state.gen2.ls1 = ls1;

	gen2_enable_logic_op(sna, op->op);
	gen2_emit_fill_pipeline(sna, op);

	if (op->src.u.gen2.pixel != sna->render_state.gen2.diffuse) {
		BATCH(_3DSTATE_DFLT_DIFFUSE_CMD);
		BATCH(op->src.u.gen2.pixel);
		sna->render_state.gen2.diffuse = op->src.u.gen2.pixel;
	}
}

static void
gen2_render_fill_blt(struct sna *sna,
		     const struct sna_fill_op *op,
		     int16_t x, int16_t y, int16_t w, int16_t h)
{
	if (!gen2_get_rectangles(sna, &op->base, 1)) {
		gen2_emit_fill_state(sna, &op->base);
		gen2_get_rectangles(sna, &op->base, 1);
	}

	VERTEX(x+w);
	VERTEX(y+h);
	VERTEX(x);
	VERTEX(y+h);
	VERTEX(x);
	VERTEX(y);
}

fastcall static void
gen2_render_fill_box(struct sna *sna,
		     const struct sna_fill_op *op,
		     const BoxRec *box)
{
	if (!gen2_get_rectangles(sna, &op->base, 1)) {
		gen2_emit_fill_state(sna, &op->base);
		gen2_get_rectangles(sna, &op->base, 1);
	}

	VERTEX(box->x2);
	VERTEX(box->y2);
	VERTEX(box->x1);
	VERTEX(box->y2);
	VERTEX(box->x1);
	VERTEX(box->y1);
}

static void
gen2_render_fill_done(struct sna *sna, const struct sna_fill_op *op)
{
	gen2_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
}

static Bool
gen2_render_fill(struct sna *sna, uint8_t alu,
		 PixmapPtr dst, struct kgem_bo *dst_bo,
		 uint32_t color,
		 struct sna_fill_op *tmp)
{
#if NO_FILL
	return sna_blt_fill(sna, alu,
			    dst_bo, dst->drawable.bitsPerPixel,
			    color,
			    tmp);
#endif

	/* Prefer to use the BLT if already engaged */
	if (prefer_blt_fill(sna) &&
	    sna_blt_fill(sna, alu,
			 dst_bo, dst->drawable.bitsPerPixel,
			 color,
			 tmp))
		return TRUE;

	/* Must use the BLT if we can't RENDER... */
	if (dst->drawable.width > 2048 ||
	    dst->drawable.height > 2048 ||
	    dst_bo->pitch > 8192)
		return sna_blt_fill(sna, alu,
				    dst_bo, dst->drawable.bitsPerPixel,
				    color,
				    tmp);

	tmp->base.op = alu;
	tmp->base.dst.pixmap = dst;
	tmp->base.dst.width = dst->drawable.width;
	tmp->base.dst.height = dst->drawable.height;
	tmp->base.dst.format = sna_format_for_depth(dst->drawable.depth);
	tmp->base.dst.bo = dst_bo;
	tmp->base.dst.x = tmp->base.dst.y = 0;
	tmp->base.floats_per_vertex = 2;
	tmp->base.floats_per_rect = 6;

	tmp->base.src.u.gen2.pixel =
		sna_rgba_for_color(color, dst->drawable.depth);

	if (!kgem_check_bo(&sna->kgem, dst_bo, NULL)) {
		kgem_submit(&sna->kgem);
		return sna_blt_fill(sna, alu,
				    dst_bo, dst->drawable.bitsPerPixel,
				    color,
				    tmp);
	}

	tmp->blt  = gen2_render_fill_blt;
	tmp->box  = gen2_render_fill_box;
	tmp->done = gen2_render_fill_done;

	gen2_emit_fill_state(sna, &tmp->base);
	return TRUE;
}

static Bool
gen2_render_fill_one_try_blt(struct sna *sna, PixmapPtr dst, struct kgem_bo *bo,
			     uint32_t color,
			     int16_t x1, int16_t y1, int16_t x2, int16_t y2,
			     uint8_t alu)
{
	BoxRec box;

	box.x1 = x1;
	box.y1 = y1;
	box.x2 = x2;
	box.y2 = y2;

	return sna_blt_fill_boxes(sna, alu,
				  bo, dst->drawable.bitsPerPixel,
				  color, &box, 1);
}

static Bool
gen2_render_fill_one(struct sna *sna, PixmapPtr dst, struct kgem_bo *bo,
		     uint32_t color,
		     int16_t x1, int16_t y1,
		     int16_t x2, int16_t y2,
		     uint8_t alu)
{
	struct sna_composite_op tmp;

#if NO_FILL_ONE
	return gen2_render_fill_one_try_blt(sna, dst, bo, color,
					    x1, y1, x2, y2, alu);
#endif

	/* Prefer to use the BLT if already engaged */
	if (prefer_blt_fill(sna) &&
	    gen2_render_fill_one_try_blt(sna, dst, bo, color,
					 x1, y1, x2, y2, alu))
		return TRUE;

	/* Must use the BLT if we can't RENDER... */
	if (dst->drawable.width > 2048 || dst->drawable.height > 2048 ||
	    bo->pitch > 8192)
		return gen2_render_fill_one_try_blt(sna, dst, bo, color,
						    x1, y1, x2, y2, alu);

	if (!kgem_check_bo(&sna->kgem, bo, NULL)) {
		kgem_submit(&sna->kgem);
		if (gen2_render_fill_one_try_blt(sna, dst, bo, color,
						 x1, y1, x2, y2, alu))
			return TRUE;
	}

	tmp.op = alu;
	tmp.dst.pixmap = dst;
	tmp.dst.width = dst->drawable.width;
	tmp.dst.height = dst->drawable.height;
	tmp.dst.format = sna_format_for_depth(dst->drawable.depth);
	tmp.dst.bo = bo;
	tmp.floats_per_vertex = 2;
	tmp.floats_per_rect = 6;
	tmp.need_magic_ca_pass = false;

	tmp.src.u.gen2.pixel =
		sna_rgba_for_color(color, dst->drawable.depth);

	gen2_emit_fill_state(sna, &tmp);
	gen2_get_rectangles(sna, &tmp, 1);
	DBG(("%s: (%d, %d), (%d, %d): %x\n", __FUNCTION__,
	     x1, y1, x2, y2, tmp.src.u.gen2.pixel));
	VERTEX(x2);
	VERTEX(y2);
	VERTEX(x1);
	VERTEX(y2);
	VERTEX(x1);
	VERTEX(y1);
	gen2_vertex_flush(sna);

	return TRUE;
}

static void
gen2_render_copy_setup_source(struct sna_composite_channel *channel,
			      PixmapPtr pixmap,
			      struct kgem_bo *bo)
{
	channel->filter = PictFilterNearest;
	channel->repeat = RepeatNone;
	channel->width  = pixmap->drawable.width;
	channel->height = pixmap->drawable.height;
	channel->scale[0] = 1.f/pixmap->drawable.width;
	channel->scale[1] = 1.f/pixmap->drawable.height;
	channel->offset[0] = 0;
	channel->offset[1] = 0;
	channel->pict_format = sna_format_for_depth(pixmap->drawable.depth);
	channel->bo = bo;
	channel->is_affine = 1;
}

static void
gen2_emit_copy_pipeline(struct sna *sna, const struct sna_composite_op *op)
{
	uint32_t blend, unwind;

	unwind = sna->kgem.nbatch;
	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_2 |
	      LOAD_TEXTURE_BLEND_STAGE(0) | 1);

	blend = TB0C_LAST_STAGE | TB0C_RESULT_SCALE_1X | TB0C_OP_ARG1 |
		TB0C_OUTPUT_WRITE_CURRENT;
	if (op->dst.format == PICT_a8)
		blend |= TB0C_ARG1_REPLICATE_ALPHA;
	else if (PICT_FORMAT_RGB(op->src.pict_format) != 0)
		blend |= TB0C_ARG1_SEL_TEXEL0;
	else
		blend |= TB0C_ARG1_SEL_ONE | TB0C_ARG1_INVERT;	/* 0.0 */
	BATCH(blend);

	blend = TB0A_RESULT_SCALE_1X | TB0A_OP_ARG1 |
		TB0A_OUTPUT_WRITE_CURRENT;
	if (PICT_FORMAT_A(op->src.pict_format) == 0)
		blend |= TB0A_ARG1_SEL_ONE;
	else
		blend |= TB0A_ARG1_SEL_TEXEL0;
	BATCH(blend);

	if (memcmp (sna->kgem.batch + sna->render_state.gen2.ls2 + 1,
		    sna->kgem.batch + unwind + 1,
		    2 * sizeof(uint32_t)) == 0)
		sna->kgem.nbatch = unwind;
	else
		sna->render_state.gen2.ls2 = unwind;
}

static void gen2_emit_copy_state(struct sna *sna, const struct sna_composite_op *op)
{
	uint32_t ls1, v;

	gen2_get_batch(sna);
	gen2_emit_target(sna, op);

	ls1 = sna->kgem.nbatch;
	BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	      I1_LOAD_S(2) | I1_LOAD_S(3) | I1_LOAD_S(8) | 2);
	BATCH(1<<12);
	BATCH(S3_CULLMODE_NONE | S3_VERTEXHAS_XY);
	BATCH(S8_ENABLE_COLOR_BUFFER_WRITE);
	if (memcmp (sna->kgem.batch + sna->render_state.gen2.ls1,
		    sna->kgem.batch + ls1,
		    4 * sizeof(uint32_t)) == 0)
		sna->kgem.nbatch = ls1;
	else
		sna->render_state.gen2.ls1 = ls1;

	gen2_enable_logic_op(sna, op->op);
	gen2_emit_copy_pipeline(sna, op);

	v = _3DSTATE_VERTEX_FORMAT_2_CMD | TEXCOORDFMT_2D;
	if (sna->render_state.gen2.vft != v) {
		BATCH(v);
		sna->render_state.gen2.vft = v;
	}

	gen2_emit_texture(sna, &op->src, 0);
}

static Bool
gen2_render_copy_boxes(struct sna *sna, uint8_t alu,
		       PixmapPtr src, struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
		       PixmapPtr dst, struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
		       const BoxRec *box, int n)
{
	struct sna_composite_op tmp;

#if NO_COPY_BOXES
	if (!sna_blt_compare_depth(&src->drawable, &dst->drawable))
		return FALSE;

	return sna_blt_copy_boxes(sna, alu,
				  src_bo, src_dx, src_dy,
				  dst_bo, dst_dx, dst_dy,
				  dst->drawable.bitsPerPixel,
				  box, n);
#endif

	DBG(("%s (%d, %d)->(%d, %d) x %d\n",
	     __FUNCTION__, src_dx, src_dy, dst_dx, dst_dy, n));

	if (prefer_blt_copy(sna) &&
	    sna_blt_compare_depth(&src->drawable, &dst->drawable) &&
	    sna_blt_copy_boxes(sna, alu,
			       src_bo, src_dx, src_dy,
			       dst_bo, dst_dx, dst_dy,
			       dst->drawable.bitsPerPixel,
			       box, n))
		return TRUE;

	if (src_bo == dst_bo || /* XXX handle overlap using 3D ? */
	    src_bo->pitch > 8192 ||
	    src->drawable.width > 2048 ||
	    src->drawable.height > 2048 ||
	    dst_bo->pitch > 8192 ||
	    dst->drawable.width > 2048 ||
	    dst->drawable.height > 2048) {
		if (!sna_blt_compare_depth(&src->drawable, &dst->drawable))
			return FALSE;

		return sna_blt_copy_boxes(sna, alu,
					  src_bo, src_dx, src_dy,
					  dst_bo, dst_dx, dst_dy,
					  dst->drawable.bitsPerPixel,
					  box, n);
	}

	if (!kgem_check_bo(&sna->kgem, dst_bo, src_bo, NULL))
		kgem_submit(&sna->kgem);

	if (kgem_bo_is_dirty(src_bo))
		kgem_emit_flush(&sna->kgem);

	memset(&tmp, 0, sizeof(tmp));
	tmp.op = alu;

	tmp.dst.pixmap = dst;
	tmp.dst.width = dst->drawable.width;
	tmp.dst.height = dst->drawable.height;
	tmp.dst.format = sna_format_for_depth(dst->drawable.depth);
	tmp.dst.bo = dst_bo;

	tmp.floats_per_vertex = 4;
	tmp.floats_per_rect = 12;

	gen2_render_copy_setup_source(&tmp.src, src, src_bo);
	gen2_emit_copy_state(sna, &tmp);
	do {
		int n_this_time;

		n_this_time = gen2_get_rectangles(sna, &tmp, n);
		if (n_this_time == 0) {
			gen2_emit_copy_state(sna, &tmp);
			n_this_time = gen2_get_rectangles(sna, &tmp, n);
		}
		n -= n_this_time;

		do {
			DBG(("	(%d, %d) -> (%d, %d) + (%d, %d)\n",
			     box->x1 + src_dx, box->y1 + src_dy,
			     box->x1 + dst_dx, box->y1 + dst_dy,
			     box->x2 - box->x1, box->y2 - box->y1));
			VERTEX(box->x2 + dst_dx);
			VERTEX(box->y2 + dst_dy);
			VERTEX((box->x2 + src_dx) * tmp.src.scale[0]);
			VERTEX((box->y2 + src_dy) * tmp.src.scale[1]);

			VERTEX(box->x1 + dst_dx);
			VERTEX(box->y2 + dst_dy);
			VERTEX((box->x1 + src_dx) * tmp.src.scale[0]);
			VERTEX((box->y2 + src_dy) * tmp.src.scale[1]);

			VERTEX(box->x1 + dst_dx);
			VERTEX(box->y1 + dst_dy);
			VERTEX((box->x1 + src_dx) * tmp.src.scale[0]);
			VERTEX((box->y1 + src_dy) * tmp.src.scale[1]);

			box++;
		} while (--n_this_time);
	} while (n);

	gen2_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
	return TRUE;
}

static void
gen2_render_copy_blt(struct sna *sna,
		     const struct sna_copy_op *op,
		     int16_t sx, int16_t sy,
		     int16_t w, int16_t h,
		     int16_t dx, int16_t dy)
{
	if (!gen2_get_rectangles(sna, &op->base, 1)) {
		gen2_emit_copy_state(sna, &op->base);
		gen2_get_rectangles(sna, &op->base, 1);
	}

	VERTEX(dx+w);
	VERTEX(dy+h);
	VERTEX((sx+w)*op->base.src.scale[0]);
	VERTEX((sy+h)*op->base.src.scale[1]);

	VERTEX(dx);
	VERTEX(dy+h);
	VERTEX(sx*op->base.src.scale[0]);
	VERTEX((sy+h)*op->base.src.scale[1]);

	VERTEX(dx);
	VERTEX(dy);
	VERTEX(sx*op->base.src.scale[0]);
	VERTEX(sy*op->base.src.scale[1]);
}

static void
gen2_render_copy_done(struct sna *sna, const struct sna_copy_op *op)
{
	gen2_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
}

static Bool
gen2_render_copy(struct sna *sna, uint8_t alu,
		 PixmapPtr src, struct kgem_bo *src_bo,
		 PixmapPtr dst, struct kgem_bo *dst_bo,
		 struct sna_copy_op *tmp)
{
#if NO_COPY
	if (!sna_blt_compare_depth(&src->drawable, &dst->drawable))
		return FALSE;

	return sna_blt_copy(sna, alu,
			    src_bo, dst_bo,
			    dst->drawable.bitsPerPixel,
			    tmp);
#endif

	/* Prefer to use the BLT */
	if (prefer_blt_copy(sna) &&
	    sna_blt_compare_depth(&src->drawable, &dst->drawable) &&
	    sna_blt_copy(sna, alu,
			 src_bo, dst_bo,
			 dst->drawable.bitsPerPixel,
			 tmp))
		return TRUE;

	/* Must use the BLT if we can't RENDER... */
	if (src->drawable.width > 2048 || src->drawable.height > 2048 ||
	    dst->drawable.width > 2048 || dst->drawable.height > 2048 ||
	    src_bo->pitch > 8192 || dst_bo->pitch > 8192) {
		if (!sna_blt_compare_depth(&src->drawable, &dst->drawable))
			return FALSE;

		return sna_blt_copy(sna, alu, src_bo, dst_bo,
				    dst->drawable.bitsPerPixel,
				    tmp);
	}

	tmp->base.op = alu;

	tmp->base.dst.pixmap = dst;
	tmp->base.dst.width = dst->drawable.width;
	tmp->base.dst.height = dst->drawable.height;
	tmp->base.dst.format = sna_format_for_depth(dst->drawable.depth);
	tmp->base.dst.bo = dst_bo;

	gen2_render_copy_setup_source(&tmp->base.src, src, src_bo);

	tmp->base.floats_per_vertex = 4;
	tmp->base.floats_per_rect = 12;

	if (!kgem_check_bo(&sna->kgem, dst_bo, src_bo, NULL))
		kgem_submit(&sna->kgem);

	if (kgem_bo_is_dirty(src_bo))
		kgem_emit_flush(&sna->kgem);

	tmp->blt  = gen2_render_copy_blt;
	tmp->done = gen2_render_copy_done;

	gen2_emit_composite_state(sna, &tmp->base);
	return TRUE;
}

static void
gen2_render_reset(struct sna *sna)
{
	sna->render_state.gen2.need_invariant = TRUE;
	sna->render_state.gen2.logic_op_enabled = FALSE;
	sna->render_state.gen2.vertex_offset = 0;
	sna->render_state.gen2.target = 0;

	sna->render_state.gen2.ls1 = 0;
	sna->render_state.gen2.ls2 = 0;
	sna->render_state.gen2.vft = 0;

	sna->render_state.gen2.diffuse = 0x0c0ffee0;
}

static void
gen2_render_flush(struct sna *sna)
{
	gen2_vertex_flush(sna);
}

Bool gen2_render_init(struct sna *sna)
{
	struct sna_render *render = &sna->render;

	/* Use the BLT (and overlay) for everything except when forced to
	 * use the texture combiners.
	 */
	render->composite = gen2_render_composite;
	render->composite_spans = gen2_render_composite_spans;
	render->fill_boxes = gen2_render_fill_boxes;
	render->fill = gen2_render_fill;
	render->fill_one = gen2_render_fill_one;
	render->copy = gen2_render_copy;
	render->copy_boxes = gen2_render_copy_boxes;

	/* XXX YUV color space conversion for video? */

	render->reset = gen2_render_reset;
	render->flush = gen2_render_flush;

	render->max_3d_size = 2048;
	return TRUE;
}
