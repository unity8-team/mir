/*
 * Copyright © 2010-2011 Intel Corporation
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
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sna.h"
#include "sna_render.h"
#include "sna_render_inline.h"
#include "sna_reg.h"
#include "sna_video.h"

#include "gen3_render.h"

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

enum {
	SHADER_NONE = 0,
	SHADER_ZERO,
	SHADER_BLACK,
	SHADER_WHITE,
	SHADER_CONSTANT,
	SHADER_LINEAR,
	SHADER_RADIAL,
	SHADER_TEXTURE,
	SHADER_OPACITY,
};

#define OUT_BATCH(v) batch_emit(sna, v)
#define OUT_BATCH_F(v) batch_emit_float(sna, v)
#define OUT_VERTEX(v) vertex_emit(sna, v)

enum gen3_radial_mode {
	RADIAL_ONE,
	RADIAL_TWO
};

static const struct blendinfo {
	Bool dst_alpha;
	Bool src_alpha;
	uint32_t src_blend;
	uint32_t dst_blend;
} gen3_blend_op[] = {
	/* Clear */	{0, 0, BLENDFACT_ZERO, BLENDFACT_ZERO},
	/* Src */	{0, 0, BLENDFACT_ONE, BLENDFACT_ZERO},
	/* Dst */	{0, 0, BLENDFACT_ZERO, BLENDFACT_ONE},
	/* Over */	{0, 1, BLENDFACT_ONE, BLENDFACT_INV_SRC_ALPHA},
	/* OverReverse */ {1, 0, BLENDFACT_INV_DST_ALPHA, BLENDFACT_ONE},
	/* In */	{1, 0, BLENDFACT_DST_ALPHA, BLENDFACT_ZERO},
	/* InReverse */ {0, 1, BLENDFACT_ZERO, BLENDFACT_SRC_ALPHA},
	/* Out */	{1, 0, BLENDFACT_INV_DST_ALPHA, BLENDFACT_ZERO},
	/* OutReverse */ {0, 1, BLENDFACT_ZERO, BLENDFACT_INV_SRC_ALPHA},
	/* Atop */	{1, 1, BLENDFACT_DST_ALPHA, BLENDFACT_INV_SRC_ALPHA},
	/* AtopReverse */ {1, 1, BLENDFACT_INV_DST_ALPHA, BLENDFACT_SRC_ALPHA},
	/* Xor */	{1, 1, BLENDFACT_INV_DST_ALPHA, BLENDFACT_INV_SRC_ALPHA},
	/* Add */	{0, 0, BLENDFACT_ONE, BLENDFACT_ONE},
};

static const struct formatinfo {
	unsigned int fmt, xfmt;
	uint32_t card_fmt;
	Bool rb_reversed;
} gen3_tex_formats[] = {
	{PICT_a8, 0, MAPSURF_8BIT | MT_8BIT_A8, FALSE},
	{PICT_a8r8g8b8, 0, MAPSURF_32BIT | MT_32BIT_ARGB8888, FALSE},
	{PICT_x8r8g8b8, 0, MAPSURF_32BIT | MT_32BIT_XRGB8888, FALSE},
	{PICT_a8b8g8r8, 0, MAPSURF_32BIT | MT_32BIT_ABGR8888, FALSE},
	{PICT_x8b8g8r8, 0, MAPSURF_32BIT | MT_32BIT_XBGR8888, FALSE},
	{PICT_a2r10g10b10, PICT_x2r10g10b10, MAPSURF_32BIT | MT_32BIT_ARGB2101010, FALSE},
	{PICT_a2b10g10r10, PICT_x2b10g10r10, MAPSURF_32BIT | MT_32BIT_ABGR2101010, FALSE},
	{PICT_r5g6b5, 0, MAPSURF_16BIT | MT_16BIT_RGB565, FALSE},
	{PICT_b5g6r5, 0, MAPSURF_16BIT | MT_16BIT_RGB565, TRUE},
	{PICT_a1r5g5b5, PICT_x1r5g5b5, MAPSURF_16BIT | MT_16BIT_ARGB1555, FALSE},
	{PICT_a1b5g5r5, PICT_x1b5g5r5, MAPSURF_16BIT | MT_16BIT_ARGB1555, TRUE},
	{PICT_a4r4g4b4, PICT_x4r4g4b4, MAPSURF_16BIT | MT_16BIT_ARGB4444, FALSE},
	{PICT_a4b4g4r4, PICT_x4b4g4r4, MAPSURF_16BIT | MT_16BIT_ARGB4444, TRUE},
};

#define xFixedToDouble(f) pixman_fixed_to_double(f)

static inline uint32_t gen3_buf_tiling(uint32_t tiling)
{
	uint32_t v = 0;
	switch (tiling) {
	case I915_TILING_Y: v |= BUF_3D_TILE_WALK_Y;
	case I915_TILING_X: v |= BUF_3D_TILED_SURFACE;
	case I915_TILING_NONE: break;
	}
	return v;
}

static inline Bool
gen3_check_pitch_3d(struct kgem_bo *bo)
{
	return bo->pitch <= 8192;
}

static uint32_t gen3_get_blend_cntl(int op,
				    Bool has_component_alpha,
				    uint32_t dst_format)
{
	uint32_t sblend = gen3_blend_op[op].src_blend;
	uint32_t dblend = gen3_blend_op[op].dst_blend;

	/* If there's no dst alpha channel, adjust the blend op so that we'll
	 * treat it as always 1.
	 */
	if (gen3_blend_op[op].dst_alpha) {
		if (PICT_FORMAT_A(dst_format) == 0) {
			if (sblend == BLENDFACT_DST_ALPHA)
				sblend = BLENDFACT_ONE;
			else if (sblend == BLENDFACT_INV_DST_ALPHA)
				sblend = BLENDFACT_ZERO;
		}

		/* gen3 engine reads 8bit color buffer into green channel
		 * in cases like color buffer blending etc., and also writes
		 * back green channel.  So with dst_alpha blend we should use
		 * color factor. See spec on "8-bit rendering".
		 */
		if (dst_format == PICT_a8) {
			if (sblend == BLENDFACT_DST_ALPHA)
				sblend = BLENDFACT_DST_COLR;
			else if (sblend == BLENDFACT_INV_DST_ALPHA)
				sblend = BLENDFACT_INV_DST_COLR;
		}
	}

	/* If the source alpha is being used, then we should only be in a case
	 * where the source blend factor is 0, and the source blend value is the
	 * mask channels multiplied by the source picture's alpha.
	 */
	if (has_component_alpha && gen3_blend_op[op].src_alpha) {
		if (dblend == BLENDFACT_SRC_ALPHA)
			dblend = BLENDFACT_SRC_COLR;
		else if (dblend == BLENDFACT_INV_SRC_ALPHA)
			dblend = BLENDFACT_INV_SRC_COLR;
	}

	return (S6_CBUF_BLEND_ENABLE | S6_COLOR_WRITE_ENABLE |
		BLENDFUNC_ADD << S6_CBUF_BLEND_FUNC_SHIFT |
		sblend << S6_CBUF_SRC_BLEND_FACT_SHIFT |
		dblend << S6_CBUF_DST_BLEND_FACT_SHIFT);
}

static Bool gen3_check_dst_format(uint32_t format)
{
	switch (format) {
	case PICT_a8r8g8b8:
	case PICT_x8r8g8b8:
	case PICT_a8b8g8r8:
	case PICT_x8b8g8r8:
	case PICT_r5g6b5:
	case PICT_b5g6r5:
	case PICT_a1r5g5b5:
	case PICT_x1r5g5b5:
	case PICT_a1b5g5r5:
	case PICT_x1b5g5r5:
	case PICT_a2r10g10b10:
	case PICT_x2r10g10b10:
	case PICT_a2b10g10r10:
	case PICT_x2b10g10r10:
	case PICT_a8:
	case PICT_a4r4g4b4:
	case PICT_x4r4g4b4:
	case PICT_a4b4g4r4:
	case PICT_x4b4g4r4:
		return TRUE;
	default:
		return FALSE;
	}
}

static Bool gen3_dst_rb_reversed(uint32_t format)
{
	switch (format) {
	case PICT_a8r8g8b8:
	case PICT_x8r8g8b8:
	case PICT_r5g6b5:
	case PICT_a1r5g5b5:
	case PICT_x1r5g5b5:
	case PICT_a2r10g10b10:
	case PICT_x2r10g10b10:
	case PICT_a8:
	case PICT_a4r4g4b4:
	case PICT_x4r4g4b4:
		return FALSE;
	default:
		return TRUE;
	}
}

#define DSTORG_HORT_BIAS(x)             ((x)<<20)
#define DSTORG_VERT_BIAS(x)             ((x)<<16)

static uint32_t gen3_get_dst_format(uint32_t format)
{
#define BIAS (DSTORG_HORT_BIAS(0x8) | DSTORG_VERT_BIAS(0x8))
	switch (format) {
	default:
	case PICT_a8r8g8b8:
	case PICT_x8r8g8b8:
	case PICT_a8b8g8r8:
	case PICT_x8b8g8r8:
		return BIAS | COLR_BUF_ARGB8888;
	case PICT_r5g6b5:
	case PICT_b5g6r5:
		return BIAS | COLR_BUF_RGB565;
	case PICT_a1r5g5b5:
	case PICT_x1r5g5b5:
	case PICT_a1b5g5r5:
	case PICT_x1b5g5r5:
		return BIAS | COLR_BUF_ARGB1555;
	case PICT_a2r10g10b10:
	case PICT_x2r10g10b10:
	case PICT_a2b10g10r10:
	case PICT_x2b10g10r10:
		return BIAS | COLR_BUF_ARGB2AAA;
	case PICT_a8:
		return BIAS | COLR_BUF_8BIT;
	case PICT_a4r4g4b4:
	case PICT_x4r4g4b4:
	case PICT_a4b4g4r4:
	case PICT_x4b4g4r4:
		return BIAS | COLR_BUF_ARGB4444;
	}
#undef BIAS
}

static uint32_t gen3_texture_repeat(uint32_t repeat)
{
#define REPEAT(x) \
	(SS3_NORMALIZED_COORDS | \
	 TEXCOORDMODE_##x << SS3_TCX_ADDR_MODE_SHIFT | \
	 TEXCOORDMODE_##x << SS3_TCY_ADDR_MODE_SHIFT)
	switch (repeat) {
	default:
	case RepeatNone:
		return REPEAT(CLAMP_BORDER);
	case RepeatNormal:
		return REPEAT(WRAP);
	case RepeatPad:
		return REPEAT(CLAMP_EDGE);
	case RepeatReflect:
		return REPEAT(MIRROR);
	}
#undef REPEAT
}

static uint32_t gen3_gradient_repeat(uint32_t repeat)
{
#define REPEAT(x) \
	(SS3_NORMALIZED_COORDS | \
	 TEXCOORDMODE_##x  << SS3_TCX_ADDR_MODE_SHIFT | \
	 TEXCOORDMODE_WRAP << SS3_TCY_ADDR_MODE_SHIFT)
	switch (repeat) {
	default:
	case RepeatNone:
		return REPEAT(CLAMP_BORDER);
	case RepeatNormal:
		return REPEAT(WRAP);
	case RepeatPad:
		return REPEAT(CLAMP_EDGE);
	case RepeatReflect:
		return REPEAT(MIRROR);
	}
#undef REPEAT
}

static Bool gen3_check_repeat(uint32_t repeat)
{
	switch (repeat) {
	case RepeatNone:
	case RepeatNormal:
	case RepeatPad:
	case RepeatReflect:
		return TRUE;
	default:
		return FALSE;
	}
}

static uint32_t gen3_filter(uint32_t filter)
{
	switch (filter) {
	default:
		assert(0);
	case PictFilterNearest:
		return (FILTER_NEAREST << SS2_MAG_FILTER_SHIFT |
			FILTER_NEAREST << SS2_MIN_FILTER_SHIFT |
			MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT);
	case PictFilterBilinear:
		return (FILTER_LINEAR  << SS2_MAG_FILTER_SHIFT |
			FILTER_LINEAR  << SS2_MIN_FILTER_SHIFT |
			MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT);
	}
}

static bool gen3_check_filter(uint32_t filter)
{
	switch (filter) {
	case PictFilterNearest:
	case PictFilterBilinear:
		return TRUE;
	default:
		return FALSE;
	}
}

static inline void
gen3_emit_composite_dstcoord(struct sna *sna, int16_t dstX, int16_t dstY)
{
	OUT_VERTEX(dstX);
	OUT_VERTEX(dstY);
}

fastcall static void
gen3_emit_composite_primitive_constant(struct sna *sna,
				       const struct sna_composite_op *op,
				       const struct sna_composite_rectangles *r)
{
	int16_t dst_x = r->dst.x + op->dst.x;
	int16_t dst_y = r->dst.y + op->dst.y;

	gen3_emit_composite_dstcoord(sna, dst_x + r->width, dst_y + r->height);
	gen3_emit_composite_dstcoord(sna, dst_x, dst_y + r->height);
	gen3_emit_composite_dstcoord(sna, dst_x, dst_y);
}

fastcall static void
gen3_emit_composite_primitive_identity_gradient(struct sna *sna,
						const struct sna_composite_op *op,
						const struct sna_composite_rectangles *r)
{
	int16_t dst_x, dst_y;
	int16_t src_x, src_y;

	dst_x = r->dst.x + op->dst.x;
	dst_y = r->dst.y + op->dst.y;
	src_x = r->src.x + op->src.offset[0];
	src_y = r->src.y + op->src.offset[1];

	gen3_emit_composite_dstcoord(sna, dst_x + r->width, dst_y + r->height);
	OUT_VERTEX(src_x + r->width);
	OUT_VERTEX(src_y + r->height);

	gen3_emit_composite_dstcoord(sna, dst_x, dst_y + r->height);
	OUT_VERTEX(src_x);
	OUT_VERTEX(src_y + r->height);

	gen3_emit_composite_dstcoord(sna, dst_x, dst_y);
	OUT_VERTEX(src_x);
	OUT_VERTEX(src_y);
}

fastcall static void
gen3_emit_composite_primitive_affine_gradient(struct sna *sna,
					      const struct sna_composite_op *op,
					      const struct sna_composite_rectangles *r)
{
	PictTransform *transform = op->src.transform;
	int16_t dst_x, dst_y;
	int16_t src_x, src_y;
	float sx, sy;

	dst_x = r->dst.x + op->dst.x;
	dst_y = r->dst.y + op->dst.y;
	src_x = r->src.x + op->src.offset[0];
	src_y = r->src.y + op->src.offset[1];

	sna_get_transformed_coordinates(src_x + r->width, src_y + r->height,
					transform,
					&sx, &sy);
	gen3_emit_composite_dstcoord(sna, dst_x + r->width, dst_y + r->height);
	OUT_VERTEX(sx);
	OUT_VERTEX(sy);

	sna_get_transformed_coordinates(src_x, src_y + r->height,
					transform,
					&sx, &sy);
	gen3_emit_composite_dstcoord(sna, dst_x, dst_y + r->height);
	OUT_VERTEX(sx);
	OUT_VERTEX(sy);

	sna_get_transformed_coordinates(src_x, src_y,
					transform,
					&sx, &sy);
	gen3_emit_composite_dstcoord(sna, dst_x, dst_y);
	OUT_VERTEX(sx);
	OUT_VERTEX(sy);
}

fastcall static void
gen3_emit_composite_primitive_identity_source(struct sna *sna,
					      const struct sna_composite_op *op,
					      const struct sna_composite_rectangles *r)
{
	float w = r->width;
	float h = r->height;
	float *v;

	v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 12;

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
gen3_emit_composite_primitive_affine_source(struct sna *sna,
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

	gen3_emit_composite_dstcoord(sna, dst_x + r->width, dst_y + r->height);
	OUT_VERTEX(sx * op->src.scale[0]);
	OUT_VERTEX(sy * op->src.scale[1]);

	_sna_get_transformed_coordinates(src_x, src_y + r->height,
					 transform,
					 &sx, &sy);
	gen3_emit_composite_dstcoord(sna, dst_x, dst_y + r->height);
	OUT_VERTEX(sx * op->src.scale[0]);
	OUT_VERTEX(sy * op->src.scale[1]);

	_sna_get_transformed_coordinates(src_x, src_y,
					 transform,
					 &sx, &sy);
	gen3_emit_composite_dstcoord(sna, dst_x, dst_y);
	OUT_VERTEX(sx * op->src.scale[0]);
	OUT_VERTEX(sy * op->src.scale[1]);
}

fastcall static void
gen3_emit_composite_primitive_constant_identity_mask(struct sna *sna,
						     const struct sna_composite_op *op,
						     const struct sna_composite_rectangles *r)
{
	float w = r->width;
	float h = r->height;
	float *v;

	v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 12;

	v[8] = v[4] = r->dst.x + op->dst.x;
	v[0] = v[4] + w;

	v[9] = r->dst.y + op->dst.y;
	v[5] = v[1] = v[9] + h;

	v[10] = v[6] = (r->mask.x + op->mask.offset[0]) * op->mask.scale[0];
	v[2] = v[6] + w * op->mask.scale[0];

	v[11] = (r->mask.y + op->mask.offset[1]) * op->mask.scale[1];
	v[7] = v[3] = v[11] + h * op->mask.scale[1];
}

fastcall static void
gen3_emit_composite_primitive_identity_source_mask(struct sna *sna,
						   const struct sna_composite_op *op,
						   const struct sna_composite_rectangles *r)
{
	float dst_x, dst_y;
	float src_x, src_y;
	float msk_x, msk_y;
	float w, h;
	float *v;

	dst_x = r->dst.x + op->dst.x;
	dst_y = r->dst.y + op->dst.y;
	src_x = r->src.x + op->src.offset[0];
	src_y = r->src.y + op->src.offset[1];
	msk_x = r->mask.x + op->mask.offset[0];
	msk_y = r->mask.y + op->mask.offset[1];
	w = r->width;
	h = r->height;

	v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 18;

	v[0] = dst_x + w;
	v[1] = dst_y + h;
	v[2] = (src_x + w) * op->src.scale[0];
	v[3] = (src_y + h) * op->src.scale[1];
	v[4] = (msk_x + w) * op->mask.scale[0];
	v[5] = (msk_y + h) * op->mask.scale[1];

	v[6] = dst_x;
	v[7] = v[1];
	v[8] = src_x * op->src.scale[0];
	v[9] = v[3];
	v[10] = msk_x * op->mask.scale[0];
	v[11] =v[5];

	v[12] = v[6];
	v[13] = dst_y;
	v[14] = v[8];
	v[15] = src_y * op->src.scale[1];
	v[16] = v[10];
	v[17] = msk_y * op->mask.scale[1];
}

fastcall static void
gen3_emit_composite_primitive_affine_source_mask(struct sna *sna,
						 const struct sna_composite_op *op,
						 const struct sna_composite_rectangles *r)
{
	int16_t src_x, src_y;
	float dst_x, dst_y;
	float msk_x, msk_y;
	float w, h;
	float *v;

	dst_x = r->dst.x + op->dst.x;
	dst_y = r->dst.y + op->dst.y;
	src_x = r->src.x + op->src.offset[0];
	src_y = r->src.y + op->src.offset[1];
	msk_x = r->mask.x + op->mask.offset[0];
	msk_y = r->mask.y + op->mask.offset[1];
	w = r->width;
	h = r->height;

	v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 18;

	v[0] = dst_x + w;
	v[1] = dst_y + h;
	sna_get_transformed_coordinates(src_x + r->width, src_y + r->height,
					op->src.transform,
					&v[2], &v[3]);
	v[2] *= op->src.scale[0];
	v[3] *= op->src.scale[1];
	v[4] = (msk_x + w) * op->mask.scale[0];
	v[5] = (msk_y + h) * op->mask.scale[1];

	v[6] = dst_x;
	v[7] = v[1];
	sna_get_transformed_coordinates(src_x, src_y + r->height,
					op->src.transform,
					&v[8], &v[9]);
	v[8] *= op->src.scale[0];
	v[9] *= op->src.scale[1];
	v[10] = msk_x * op->mask.scale[0];
	v[11] =v[5];

	v[12] = v[6];
	v[13] = dst_y;
	sna_get_transformed_coordinates(src_x, src_y,
					op->src.transform,
					&v[14], &v[15]);
	v[14] *= op->src.scale[0];
	v[15] *= op->src.scale[1];
	v[16] = v[10];
	v[17] = msk_y * op->mask.scale[1];
}

static void
gen3_emit_composite_texcoord(struct sna *sna,
			     const struct sna_composite_channel *channel,
			     int16_t x, int16_t y)
{
	float s = 0, t = 0, w = 1;

	switch (channel->u.gen3.type) {
	case SHADER_OPACITY:
	case SHADER_NONE:
	case SHADER_ZERO:
	case SHADER_BLACK:
	case SHADER_WHITE:
	case SHADER_CONSTANT:
		break;

	case SHADER_LINEAR:
	case SHADER_RADIAL:
	case SHADER_TEXTURE:
		x += channel->offset[0];
		y += channel->offset[1];
		if (channel->is_affine) {
			sna_get_transformed_coordinates(x, y,
							channel->transform,
							&s, &t);
			OUT_VERTEX(s * channel->scale[0]);
			OUT_VERTEX(t * channel->scale[1]);
		} else {
			sna_get_transformed_coordinates_3d(x, y,
							   channel->transform,
							   &s, &t, &w);
			OUT_VERTEX(s * channel->scale[0]);
			OUT_VERTEX(t * channel->scale[1]);
			OUT_VERTEX(0);
			OUT_VERTEX(w);
		}
		break;
	}
}

static void
gen3_emit_composite_vertex(struct sna *sna,
			   const struct sna_composite_op *op,
			   int16_t srcX, int16_t srcY,
			   int16_t maskX, int16_t maskY,
			   int16_t dstX, int16_t dstY)
{
	gen3_emit_composite_dstcoord(sna, dstX, dstY);
	gen3_emit_composite_texcoord(sna, &op->src, srcX, srcY);
	gen3_emit_composite_texcoord(sna, &op->mask, maskX, maskY);
}

fastcall static void
gen3_emit_composite_primitive(struct sna *sna,
			      const struct sna_composite_op *op,
			      const struct sna_composite_rectangles *r)
{
	gen3_emit_composite_vertex(sna, op,
				   r->src.x + r->width,
				   r->src.y + r->height,
				   r->mask.x + r->width,
				   r->mask.y + r->height,
				   op->dst.x + r->dst.x + r->width,
				   op->dst.y + r->dst.y + r->height);
	gen3_emit_composite_vertex(sna, op,
				   r->src.x,
				   r->src.y + r->height,
				   r->mask.x,
				   r->mask.y + r->height,
				   op->dst.x + r->dst.x,
				   op->dst.y + r->dst.y + r->height);
	gen3_emit_composite_vertex(sna, op,
				   r->src.x,
				   r->src.y,
				   r->mask.x,
				   r->mask.y,
				   op->dst.x + r->dst.x,
				   op->dst.y + r->dst.y);
}

static inline void
gen3_2d_perspective(struct sna *sna, int in, int out)
{
	gen3_fs_rcp(out, 0, gen3_fs_operand(in, W, W, W, W));
	gen3_fs_mul(out,
		    gen3_fs_operand(in, X, Y, ZERO, ONE),
		    gen3_fs_operand_reg(out));
}

static inline void
gen3_linear_coord(struct sna *sna,
		  const struct sna_composite_channel *channel,
		  int in, int out)
{
	int c = channel->u.gen3.constants;

	if (!channel->is_affine) {
		gen3_2d_perspective(sna, in, FS_U0);
		in = FS_U0;
	}

	gen3_fs_mov(out, gen3_fs_operand_zero());
	gen3_fs_dp3(out, MASK_X,
		    gen3_fs_operand(in, X, Y, ONE, ZERO),
		    gen3_fs_operand_reg(c));
}

static void
gen3_radial_coord(struct sna *sna,
		  const struct sna_composite_channel *channel,
		  int in, int out)
{
	int c = channel->u.gen3.constants;

	if (!channel->is_affine) {
		gen3_2d_perspective(sna, in, FS_U0);
		in = FS_U0;
	}

	switch (channel->u.gen3.mode) {
	case RADIAL_ONE:
		/*
		   pdx = (x - c1x) / dr, pdy = (y - c1y) / dr;
		   r² = pdx*pdx + pdy*pdy
		   t = r²/sqrt(r²) - r1/dr;
		   */
		gen3_fs_mad(FS_U0, MASK_X | MASK_Y,
			    gen3_fs_operand(in, X, Y, ZERO, ZERO),
			    gen3_fs_operand(c, Z, Z, ZERO, ZERO),
			    gen3_fs_operand(c, NEG_X, NEG_Y, ZERO, ZERO));
		gen3_fs_dp2add(FS_U0, MASK_X,
			       gen3_fs_operand(FS_U0, X, Y, ZERO, ZERO),
			       gen3_fs_operand(FS_U0, X, Y, ZERO, ZERO),
			       gen3_fs_operand_zero());
		gen3_fs_rsq(out, MASK_X, gen3_fs_operand(FS_U0, X, X, X, X));
		gen3_fs_mad(out, 0,
			    gen3_fs_operand(FS_U0, X, ZERO, ZERO, ZERO),
			    gen3_fs_operand(out, X, ZERO, ZERO, ZERO),
			    gen3_fs_operand(c, W, ZERO, ZERO, ZERO));
		break;

	case RADIAL_TWO:
		/*
		   pdx = x - c1x, pdy = y - c1y;
		   A = dx² + dy² - dr²
		   B = -2*(pdx*dx + pdy*dy + r1*dr);
		   C = pdx² + pdy² - r1²;
		   det = B*B - 4*A*C;
		   t = (-B + sqrt (det)) / (2 * A)
		   */

		/* u0.x = pdx, u0.y = pdy, u[0].z = r1; */
		gen3_fs_add(FS_U0,
			    gen3_fs_operand(in, X, Y, ZERO, ZERO),
			    gen3_fs_operand(c, X, Y, Z, ZERO));
		/* u0.x = pdx, u0.y = pdy, u[0].z = r1, u[0].w = B; */
		gen3_fs_dp3(FS_U0, MASK_W,
			    gen3_fs_operand(FS_U0, X, Y, ONE, ZERO),
			    gen3_fs_operand(c+1, X, Y, Z, ZERO));
		/* u1.x = pdx² + pdy² - r1²; [C] */
		gen3_fs_dp3(FS_U1, MASK_X,
			    gen3_fs_operand(FS_U0, X, Y, Z, ZERO),
			    gen3_fs_operand(FS_U0, X, Y, NEG_Z, ZERO));
		/* u1.x = C, u1.y = B, u1.z=-4*A; */
		gen3_fs_mov_masked(FS_U1, MASK_Y, gen3_fs_operand(FS_U0, W, W, W, W));
		gen3_fs_mov_masked(FS_U1, MASK_Z, gen3_fs_operand(c, W, W, W, W));
		/* u1.x = B² - 4*A*C */
		gen3_fs_dp2add(FS_U1, MASK_X,
			       gen3_fs_operand(FS_U1, X, Y, ZERO, ZERO),
			       gen3_fs_operand(FS_U1, Z, Y, ZERO, ZERO),
			       gen3_fs_operand_zero());
		/* out.x = -B + sqrt (B² - 4*A*C), */
		gen3_fs_rsq(out, MASK_X, gen3_fs_operand(FS_U1, X, X, X, X));
		gen3_fs_mad(out, MASK_X,
			    gen3_fs_operand(out, X, ZERO, ZERO, ZERO),
			    gen3_fs_operand(FS_U1, X, ZERO, ZERO, ZERO),
			    gen3_fs_operand(FS_U0, NEG_W, ZERO, ZERO, ZERO));
		/* out.x = (-B + sqrt (B² - 4*A*C)) / (2 * A), */
		gen3_fs_mul(out,
			    gen3_fs_operand(out, X, ZERO, ZERO, ZERO),
			    gen3_fs_operand(c+1, W, ZERO, ZERO, ZERO));
		break;
	}
}

static void
gen3_composite_emit_shader(struct sna *sna,
			   const struct sna_composite_op *op,
			   uint8_t blend)
{
	Bool dst_is_alpha = PIXMAN_FORMAT_RGB(op->dst.format) == 0;
	const struct sna_composite_channel *src, *mask;
	struct gen3_render_state *state = &sna->render_state.gen3;
	uint32_t shader_offset, id;
	int src_reg, mask_reg;
	int t, length;

	src = &op->src;
	mask = &op->mask;
	if (mask->u.gen3.type == SHADER_NONE)
		mask = NULL;

	if (mask && src->is_opaque &&
	    gen3_blend_op[blend].src_alpha &&
	    op->has_component_alpha) {
		src = mask;
		mask = NULL;
	}

	id = (src->u.gen3.type |
	      src->is_affine << 4 |
	      src->alpha_fixup << 5 |
	      src->rb_reversed << 6);
	if (mask) {
		id |= (mask->u.gen3.type << 8 |
		       mask->is_affine << 12 |
		       gen3_blend_op[blend].src_alpha << 13 |
		       op->has_component_alpha << 14 |
		       mask->alpha_fixup << 15 |
		       mask->rb_reversed << 16);
	}
	id |= dst_is_alpha << 24;
	id |= op->rb_reversed << 25;

	if (id == state->last_shader)
		return;

	state->last_shader = id;

	shader_offset = sna->kgem.nbatch++;
	t = 0;
	switch (src->u.gen3.type) {
	case SHADER_NONE:
	case SHADER_OPACITY:
		assert(0);
	case SHADER_ZERO:
	case SHADER_BLACK:
	case SHADER_WHITE:
		break;
	case SHADER_CONSTANT:
		gen3_fs_dcl(FS_T8);
		src_reg = FS_T8;
		break;
	case SHADER_TEXTURE:
	case SHADER_RADIAL:
	case SHADER_LINEAR:
		gen3_fs_dcl(FS_S0);
		gen3_fs_dcl(FS_T0);
		t++;
		break;
	}

	if (mask == NULL) {
		switch (src->u.gen3.type) {
		case SHADER_ZERO:
			gen3_fs_mov(FS_OC, gen3_fs_operand_zero());
			goto done;
		case SHADER_BLACK:
			gen3_fs_mov(FS_OC, gen3_fs_operand(FS_R0, ZERO, ZERO, ZERO, ONE));
			goto done;
		case SHADER_WHITE:
			gen3_fs_mov(FS_OC, gen3_fs_operand_one());
			goto done;
		}
		if (src->alpha_fixup && dst_is_alpha) {
			gen3_fs_mov(FS_OC, gen3_fs_operand_one());
			goto done;
		}
		/* No mask, so load directly to output color */
		if (src->u.gen3.type != SHADER_CONSTANT) {
			if (dst_is_alpha || src->rb_reversed ^ op->rb_reversed)
				src_reg = FS_R0;
			else
				src_reg = FS_OC;
		}
		switch (src->u.gen3.type) {
		case SHADER_LINEAR:
			gen3_linear_coord(sna, src, FS_T0, FS_R0);
			gen3_fs_texld(src_reg, FS_S0, FS_R0);
			break;

		case SHADER_RADIAL:
			gen3_radial_coord(sna, src, FS_T0, FS_R0);
			gen3_fs_texld(src_reg, FS_S0, FS_R0);
			break;

		case SHADER_TEXTURE:
			if (src->is_affine)
				gen3_fs_texld(src_reg, FS_S0, FS_T0);
			else
				gen3_fs_texldp(src_reg, FS_S0, FS_T0);
			break;

		case SHADER_NONE:
		case SHADER_CONSTANT:
		case SHADER_WHITE:
		case SHADER_BLACK:
		case SHADER_ZERO:
			assert(0);
			break;
		}

		if (src_reg != FS_OC) {
			if (src->alpha_fixup)
				gen3_fs_mov(FS_OC,
					    src->rb_reversed ^ op->rb_reversed ?
					    gen3_fs_operand(src_reg, Z, Y, X, ONE) :
					    gen3_fs_operand(src_reg, X, Y, Z, ONE));
			else if (dst_is_alpha)
				gen3_fs_mov(FS_OC, gen3_fs_operand(src_reg, W, W, W, W));
			else if (src->rb_reversed ^ op->rb_reversed)
				gen3_fs_mov(FS_OC, gen3_fs_operand(src_reg, Z, Y, X, W));
			else
				gen3_fs_mov(FS_OC, gen3_fs_operand_reg(src_reg));
		} else if (src->alpha_fixup)
			gen3_fs_mov_masked(FS_OC, MASK_W, gen3_fs_operand_one());
	} else {
		int out_reg = FS_OC;
		if (op->rb_reversed)
			out_reg = FS_U0;

		switch (mask->u.gen3.type) {
		case SHADER_CONSTANT:
			gen3_fs_dcl(FS_T9);
			mask_reg = FS_T9;
			break;
		case SHADER_TEXTURE:
		case SHADER_LINEAR:
		case SHADER_RADIAL:
			gen3_fs_dcl(FS_S0 + t);
		case SHADER_OPACITY:
			gen3_fs_dcl(FS_T0 + t);
			break;
		case SHADER_NONE:
		case SHADER_ZERO:
		case SHADER_BLACK:
		case SHADER_WHITE:
			assert(0);
			break;
		}

		t = 0;
		switch (src->u.gen3.type) {
		case SHADER_LINEAR:
			gen3_linear_coord(sna, src, FS_T0, FS_R0);
			gen3_fs_texld(FS_R0, FS_S0, FS_R0);
			src_reg = FS_R0;
			t++;
			break;

		case SHADER_RADIAL:
			gen3_radial_coord(sna, src, FS_T0, FS_R0);
			gen3_fs_texld(FS_R0, FS_S0, FS_R0);
			src_reg = FS_R0;
			t++;
			break;

		case SHADER_TEXTURE:
			if (src->is_affine)
				gen3_fs_texld(FS_R0, FS_S0, FS_T0);
			else
				gen3_fs_texldp(FS_R0, FS_S0, FS_T0);
			src_reg = FS_R0;
			t++;
			break;

		case SHADER_CONSTANT:
		case SHADER_NONE:
		case SHADER_ZERO:
		case SHADER_BLACK:
		case SHADER_WHITE:
			break;
		}
		if (src->alpha_fixup)
			gen3_fs_mov_masked(src_reg, MASK_W, gen3_fs_operand_one());
		if (src->rb_reversed)
			gen3_fs_mov(src_reg, gen3_fs_operand(src_reg, Z, Y, X, W));

		switch (mask->u.gen3.type) {
		case SHADER_LINEAR:
			gen3_linear_coord(sna, mask, FS_T0 + t, FS_R1);
			gen3_fs_texld(FS_R1, FS_S0 + t, FS_R1);
			mask_reg = FS_R1;
			break;

		case SHADER_RADIAL:
			gen3_radial_coord(sna, mask, FS_T0 + t, FS_R1);
			gen3_fs_texld(FS_R1, FS_S0 + t, FS_R1);
			mask_reg = FS_R1;
			break;

		case SHADER_TEXTURE:
			if (mask->is_affine)
				gen3_fs_texld(FS_R1, FS_S0 + t, FS_T0 + t);
			else
				gen3_fs_texldp(FS_R1, FS_S0 + t, FS_T0 + t);
			mask_reg = FS_R1;
			break;

		case SHADER_OPACITY:
			switch (src->u.gen3.type) {
			case SHADER_BLACK:
			case SHADER_WHITE:
				if (dst_is_alpha || src->u.gen3.type == SHADER_WHITE) {
					gen3_fs_mov(out_reg,
						    gen3_fs_operand(FS_T0 + t, X, X, X, X));
				} else {
					gen3_fs_mov(out_reg,
						    gen3_fs_operand(FS_T0 + t, ZERO, ZERO, ZERO, X));
				}
				break;
			default:
				if (dst_is_alpha) {
					gen3_fs_mul(out_reg,
						    gen3_fs_operand(src_reg, W, W, W, W),
						    gen3_fs_operand(FS_T0 + t, X, X, X, X));
				} else {
					gen3_fs_mul(out_reg,
						    gen3_fs_operand(src_reg, X, Y, Z, W),
						    gen3_fs_operand(FS_T0 + t, X, X, X, X));
				}
			}
			goto mask_done;

		case SHADER_CONSTANT:
		case SHADER_ZERO:
		case SHADER_BLACK:
		case SHADER_WHITE:
			assert(0);
		case SHADER_NONE:
			break;
		}
		if (mask->alpha_fixup)
			gen3_fs_mov_masked(mask_reg, MASK_W, gen3_fs_operand_one());
		if (mask->rb_reversed)
			gen3_fs_mov(mask_reg, gen3_fs_operand(mask_reg, Z, Y, X, W));

		if (dst_is_alpha) {
			switch (src->u.gen3.type) {
			case SHADER_BLACK:
			case SHADER_WHITE:
				gen3_fs_mov(out_reg,
					    gen3_fs_operand(mask_reg, W, W, W, W));
				break;
			default:
				gen3_fs_mul(out_reg,
					    gen3_fs_operand(src_reg, W, W, W, W),
					    gen3_fs_operand(mask_reg, W, W, W, W));
				break;
			}
		} else {
			/* If component alpha is active in the mask and the blend
			 * operation uses the source alpha, then we know we don't
			 * need the source value (otherwise we would have hit a
			 * fallback earlier), so we provide the source alpha (src.A *
			 * mask.X) as output color.
			 * Conversely, if CA is set and we don't need the source alpha,
			 * then we produce the source value (src.X * mask.X) and the
			 * source alpha is unused.  Otherwise, we provide the non-CA
			 * source value (src.X * mask.A).
			 */
			if (op->has_component_alpha) {
				switch (src->u.gen3.type) {
				case SHADER_WHITE:
				case SHADER_BLACK:
					if (gen3_blend_op[blend].src_alpha)
						gen3_fs_mov(out_reg,
							    gen3_fs_operand_reg(mask_reg));
					else
						gen3_fs_mov(out_reg,
							    gen3_fs_operand(mask_reg, ZERO, ZERO, ZERO, W));
					break;
				default:
					if (gen3_blend_op[blend].src_alpha)
						gen3_fs_mul(out_reg,
							    gen3_fs_operand(src_reg, W, W, W, W),
							    gen3_fs_operand_reg(mask_reg));
					else
						gen3_fs_mul(out_reg,
							    gen3_fs_operand_reg(src_reg),
							    gen3_fs_operand_reg(mask_reg));
					break;
				}
			} else {
				switch (src->u.gen3.type) {
				case SHADER_WHITE:
					gen3_fs_mov(out_reg,
						    gen3_fs_operand(mask_reg, W, W, W, W));
					break;
				case SHADER_BLACK:
					gen3_fs_mov(out_reg,
						    gen3_fs_operand(mask_reg, ZERO, ZERO, ZERO, W));
					break;
				default:
					gen3_fs_mul(out_reg,
						    gen3_fs_operand_reg(src_reg),
						    gen3_fs_operand(mask_reg, W, W, W, W));
					break;
				}
			}
		}
mask_done:
		if (op->rb_reversed)
			gen3_fs_mov(FS_OC, gen3_fs_operand(FS_U0, Z, Y, X, W));
	}

done:
	length = sna->kgem.nbatch - shader_offset;
	sna->kgem.batch[shader_offset] =
		_3DSTATE_PIXEL_SHADER_PROGRAM | (length - 2);
}

static uint32_t gen3_ms_tiling(uint32_t tiling)
{
	uint32_t v = 0;
	switch (tiling) {
	case I915_TILING_Y: v |= MS3_TILE_WALK;
	case I915_TILING_X: v |= MS3_TILED_SURFACE;
	case I915_TILING_NONE: break;
	}
	return v;
}

static void gen3_emit_invariant(struct sna *sna)
{
	/* Disable independent alpha blend */
	OUT_BATCH(_3DSTATE_INDEPENDENT_ALPHA_BLEND_CMD | IAB_MODIFY_ENABLE |
		  IAB_MODIFY_FUNC | BLENDFUNC_ADD << IAB_FUNC_SHIFT |
		  IAB_MODIFY_SRC_FACTOR | BLENDFACT_ONE << IAB_SRC_FACTOR_SHIFT |
		  IAB_MODIFY_DST_FACTOR | BLENDFACT_ZERO << IAB_DST_FACTOR_SHIFT);

	OUT_BATCH(_3DSTATE_COORD_SET_BINDINGS |
		  CSB_TCB(0, 0) |
		  CSB_TCB(1, 1) |
		  CSB_TCB(2, 2) |
		  CSB_TCB(3, 3) |
		  CSB_TCB(4, 4) |
		  CSB_TCB(5, 5) |
		  CSB_TCB(6, 6) |
		  CSB_TCB(7, 7));

	OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(3) | I1_LOAD_S(4) | I1_LOAD_S(5) | 2);
	OUT_BATCH(0); /* Disable texture coordinate wrap-shortest */
	OUT_BATCH((1 << S4_POINT_WIDTH_SHIFT) |
		  S4_LINE_WIDTH_ONE |
		  S4_CULLMODE_NONE |
		  S4_VFMT_XY);
	OUT_BATCH(0); /* Disable fog/stencil. *Enable* write mask. */

	OUT_BATCH(_3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT);
	OUT_BATCH(_3DSTATE_DEPTH_SUBRECT_DISABLE);

	OUT_BATCH(_3DSTATE_LOAD_INDIRECT);
	OUT_BATCH(0x00000000);

	OUT_BATCH(_3DSTATE_STIPPLE);
	OUT_BATCH(0x00000000);

	sna->render_state.gen3.need_invariant = FALSE;
}

static void
gen3_get_batch(struct sna *sna)
{
#define MAX_OBJECTS 3 /* worst case: dst + src + mask  */

	kgem_set_mode(&sna->kgem, KGEM_RENDER);

	if (!kgem_check_batch(&sna->kgem, 200)) {
		DBG(("%s: flushing batch: size %d > %d\n",
		     __FUNCTION__, 200,
		     sna->kgem.surface-sna->kgem.nbatch));
		kgem_submit(&sna->kgem);
	}

	if (sna->kgem.nreloc > KGEM_RELOC_SIZE(&sna->kgem) - MAX_OBJECTS) {
		DBG(("%s: flushing batch: reloc %d >= %d\n",
		     __FUNCTION__,
		     sna->kgem.nreloc,
		     (int)KGEM_RELOC_SIZE(&sna->kgem) - MAX_OBJECTS));
		kgem_submit(&sna->kgem);
	}

	if (sna->kgem.nexec > KGEM_EXEC_SIZE(&sna->kgem) - MAX_OBJECTS - 1) {
		DBG(("%s: flushing batch: exec %d >= %d\n",
		     __FUNCTION__,
		     sna->kgem.nexec,
		     (int)KGEM_EXEC_SIZE(&sna->kgem) - MAX_OBJECTS - 1));
		kgem_submit(&sna->kgem);
	}

	if (sna->render_state.gen3.need_invariant)
		gen3_emit_invariant(sna);
#undef MAX_OBJECTS
}

static void gen3_emit_target(struct sna *sna,
			     struct kgem_bo *bo,
			     int width,
			     int height,
			     int format)
{
	struct gen3_render_state *state = &sna->render_state.gen3;

	/* BUF_INFO is an implicit flush, so skip if the target is unchanged. */
	if (bo->unique_id != state->current_dst) {
		uint32_t v;

		OUT_BATCH(_3DSTATE_BUF_INFO_CMD);
		OUT_BATCH(BUF_3D_ID_COLOR_BACK |
			  gen3_buf_tiling(bo->tiling) |
			  bo->pitch);
		OUT_BATCH(kgem_add_reloc(&sna->kgem, sna->kgem.nbatch,
					 bo,
					 I915_GEM_DOMAIN_RENDER << 16 |
					 I915_GEM_DOMAIN_RENDER,
					 0));

		OUT_BATCH(_3DSTATE_DST_BUF_VARS_CMD);
		OUT_BATCH(gen3_get_dst_format(format));

		v = DRAW_YMAX(height - 1) | DRAW_XMAX(width - 1);
		if (v != state->last_drawrect_limit) {
			OUT_BATCH(_3DSTATE_DRAW_RECT_CMD);
			OUT_BATCH(0); /* XXX dither origin? */
			OUT_BATCH(0);
			OUT_BATCH(v);
			OUT_BATCH(0);
			state->last_drawrect_limit = v;
		}

		state->current_dst = bo->unique_id;
	}
	kgem_bo_mark_dirty(bo);
}

static void gen3_emit_composite_state(struct sna *sna,
				      const struct sna_composite_op *op)
{
	struct gen3_render_state *state = &sna->render_state.gen3;
	uint32_t map[4];
	uint32_t sampler[4];
	struct kgem_bo *bo[2];
	unsigned int tex_count, n;
	uint32_t ss2;

	gen3_get_batch(sna);

	gen3_emit_target(sna,
			 op->dst.bo,
			 op->dst.width,
			 op->dst.height,
			 op->dst.format);

	ss2 = ~0;
	tex_count = 0;
	switch (op->src.u.gen3.type) {
	case SHADER_OPACITY:
	case SHADER_NONE:
		assert(0);
	case SHADER_ZERO:
	case SHADER_BLACK:
	case SHADER_WHITE:
		break;
	case SHADER_CONSTANT:
		if (op->src.u.gen3.mode != state->last_diffuse) {
			OUT_BATCH(_3DSTATE_DFLT_DIFFUSE_CMD);
			OUT_BATCH(op->src.u.gen3.mode);
			state->last_diffuse = op->src.u.gen3.mode;
		}
		break;
	case SHADER_LINEAR:
	case SHADER_RADIAL:
	case SHADER_TEXTURE:
		ss2 &= ~S2_TEXCOORD_FMT(tex_count, TEXCOORDFMT_NOT_PRESENT);
		ss2 |= S2_TEXCOORD_FMT(tex_count,
				       op->src.is_affine ? TEXCOORDFMT_2D : TEXCOORDFMT_4D);
		map[tex_count * 2 + 0] =
			op->src.card_format |
			gen3_ms_tiling(op->src.bo->tiling) |
			(op->src.height - 1) << MS3_HEIGHT_SHIFT |
			(op->src.width - 1) << MS3_WIDTH_SHIFT;
		map[tex_count * 2 + 1] =
			(op->src.bo->pitch / 4 - 1) << MS4_PITCH_SHIFT;

		sampler[tex_count * 2 + 0] = op->src.filter;
		sampler[tex_count * 2 + 1] =
			op->src.repeat |
			tex_count << SS3_TEXTUREMAP_INDEX_SHIFT;
		bo[tex_count] = op->src.bo;
		tex_count++;
		break;
	}
	switch (op->mask.u.gen3.type) {
	case SHADER_NONE:
	case SHADER_ZERO:
	case SHADER_BLACK:
	case SHADER_WHITE:
		break;
	case SHADER_CONSTANT:
		if (op->mask.u.gen3.mode != state->last_specular) {
			OUT_BATCH(_3DSTATE_DFLT_SPEC_CMD);
			OUT_BATCH(op->mask.u.gen3.mode);
			state->last_specular = op->mask.u.gen3.mode;
		}
		break;
	case SHADER_LINEAR:
	case SHADER_RADIAL:
	case SHADER_TEXTURE:
		ss2 &= ~S2_TEXCOORD_FMT(tex_count, TEXCOORDFMT_NOT_PRESENT);
		ss2 |= S2_TEXCOORD_FMT(tex_count,
				       op->mask.is_affine ? TEXCOORDFMT_2D : TEXCOORDFMT_4D);
		map[tex_count * 2 + 0] =
			op->mask.card_format |
			gen3_ms_tiling(op->mask.bo->tiling) |
			(op->mask.height - 1) << MS3_HEIGHT_SHIFT |
			(op->mask.width - 1) << MS3_WIDTH_SHIFT;
		map[tex_count * 2 + 1] =
			(op->mask.bo->pitch / 4 - 1) << MS4_PITCH_SHIFT;

		sampler[tex_count * 2 + 0] = op->mask.filter;
		sampler[tex_count * 2 + 1] =
			op->mask.repeat |
			tex_count << SS3_TEXTUREMAP_INDEX_SHIFT;
		bo[tex_count] = op->mask.bo;
		tex_count++;
		break;
	case SHADER_OPACITY:
		ss2 &= ~S2_TEXCOORD_FMT(tex_count, TEXCOORDFMT_NOT_PRESENT);
		ss2 |= S2_TEXCOORD_FMT(tex_count, TEXCOORDFMT_1D);
		break;
	}

	{
		uint32_t blend_offset = sna->kgem.nbatch;

		OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(2) | I1_LOAD_S(6) | 1);
		OUT_BATCH(ss2);
		OUT_BATCH(gen3_get_blend_cntl(op->op,
					      op->has_component_alpha,
					      op->dst.format));

		if (memcmp(sna->kgem.batch + state->last_blend + 1,
			   sna->kgem.batch + blend_offset + 1,
			   2 * 4) == 0)
			sna->kgem.nbatch = blend_offset;
		else
			state->last_blend = blend_offset;
	}

	if (op->u.gen3.num_constants) {
		int count = op->u.gen3.num_constants;
		if (state->last_constants) {
			int last = sna->kgem.batch[state->last_constants+1];
			if (last == (1 << (count >> 2)) - 1 &&
			    memcmp(&sna->kgem.batch[state->last_constants+2],
				   op->u.gen3.constants,
				   count * sizeof(uint32_t)) == 0)
				count = 0;
		}
		if (count) {
			state->last_constants = sna->kgem.nbatch;
			OUT_BATCH(_3DSTATE_PIXEL_SHADER_CONSTANTS | count);
			OUT_BATCH((1 << (count >> 2)) - 1);

			memcpy(sna->kgem.batch + sna->kgem.nbatch,
			       op->u.gen3.constants,
			       count * sizeof(uint32_t));
			sna->kgem.nbatch += count;
		}
	}

	if (tex_count != 0) {
		uint32_t rewind;

		n = 0;
		if (tex_count == state->tex_count) {
			for (; n < tex_count; n++) {
				if (map[2*n+0] != state->tex_map[2*n+0] ||
				    map[2*n+1] != state->tex_map[2*n+1] ||
				    state->tex_handle[n] != bo[n]->handle ||
				    state->tex_delta[n] != bo[n]->delta)
					break;
			}
		}
		if (n < tex_count) {
			OUT_BATCH(_3DSTATE_MAP_STATE | (3 * tex_count));
			OUT_BATCH((1 << tex_count) - 1);
			for (n = 0; n < tex_count; n++) {
				OUT_BATCH(kgem_add_reloc(&sna->kgem,
							 sna->kgem.nbatch,
							 bo[n],
							 I915_GEM_DOMAIN_SAMPLER<< 16,
							 0));
				OUT_BATCH(map[2*n + 0]);
				OUT_BATCH(map[2*n + 1]);

				state->tex_map[2*n+0] = map[2*n+0];
				state->tex_map[2*n+1] = map[2*n+1];
				state->tex_handle[n] = bo[n]->handle;
				state->tex_delta[n] = bo[n]->delta;
			}
			state->tex_count = n;
		}

		rewind = sna->kgem.nbatch;
		OUT_BATCH(_3DSTATE_SAMPLER_STATE | (3 * tex_count));
		OUT_BATCH((1 << tex_count) - 1);
		for (n = 0; n < tex_count; n++) {
			OUT_BATCH(sampler[2*n + 0]);
			OUT_BATCH(sampler[2*n + 1]);
			OUT_BATCH(0);
		}
		if (state->last_sampler &&
		    memcmp(&sna->kgem.batch[state->last_sampler+1],
			   &sna->kgem.batch[rewind + 1],
			   (3*tex_count + 1)*sizeof(uint32_t)) == 0)
			sna->kgem.nbatch = rewind;
		else
			state->last_sampler = rewind;
	}

	gen3_composite_emit_shader(sna, op, op->op);
}

static void gen3_magic_ca_pass(struct sna *sna,
			       const struct sna_composite_op *op)
{
	if (!op->need_magic_ca_pass)
		return;

	DBG(("%s(%d)\n", __FUNCTION__,
	     sna->render.vertex_index - sna->render.vertex_start));

	OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(6) | 0);
	OUT_BATCH(gen3_get_blend_cntl(PictOpAdd, TRUE, op->dst.format));
	gen3_composite_emit_shader(sna, op, PictOpAdd);

	OUT_BATCH(PRIM3D_RECTLIST | PRIM3D_INDIRECT_SEQUENTIAL |
		  (sna->render.vertex_index - sna->render.vertex_start));
	OUT_BATCH(sna->render.vertex_start);

	sna->render_state.gen3.last_blend = 0;
}

static void gen3_vertex_flush(struct sna *sna)
{
	if (sna->render_state.gen3.vertex_offset == 0 ||
	    sna->render.vertex_index == sna->render.vertex_start)
		return;

	DBG(("%s[%x] = %d\n", __FUNCTION__,
	     4*sna->render_state.gen3.vertex_offset,
	     sna->render.vertex_index - sna->render.vertex_start));

	sna->kgem.batch[sna->render_state.gen3.vertex_offset] =
		PRIM3D_RECTLIST | PRIM3D_INDIRECT_SEQUENTIAL |
		(sna->render.vertex_index - sna->render.vertex_start);
	sna->kgem.batch[sna->render_state.gen3.vertex_offset + 1] =
		sna->render.vertex_start;

	if (sna->render.op)
		gen3_magic_ca_pass(sna, sna->render.op);

	sna->render_state.gen3.vertex_offset = 0;
}

static void gen3_vertex_finish(struct sna *sna, Bool last)
{
	struct kgem_bo *bo;
	int delta;

	DBG(("%s: last? %d\n", __FUNCTION__, last));

	gen3_vertex_flush(sna);
	if (!sna->render.vertex_used)
		return;

	if (last && sna->kgem.nbatch + sna->render.vertex_used <= sna->kgem.surface) {
		DBG(("%s: copy to batch: %d @ %d\n", __FUNCTION__,
		     sna->render.vertex_used, sna->kgem.nbatch));
		memcpy(sna->kgem.batch + sna->kgem.nbatch,
		       sna->render.vertex_data,
		       sna->render.vertex_used * 4);
		delta = sna->kgem.nbatch * 4;
		bo = NULL;
		sna->kgem.nbatch += sna->render.vertex_used;
	} else {
		bo = kgem_create_linear(&sna->kgem, 4*sna->render.vertex_used);
		if (bo && !kgem_bo_write(&sna->kgem, bo,
					 sna->render.vertex_data,
					 4*sna->render.vertex_used)) {
			kgem_bo_destroy(&sna->kgem, bo);
			return;
		}
		delta = 0;
		DBG(("%s: new vbo: %d\n", __FUNCTION__,
		     sna->render.vertex_used));
	}

	DBG(("%s: reloc = %d\n", __FUNCTION__,
	     sna->render.vertex_reloc[0]));

	sna->kgem.batch[sna->render.vertex_reloc[0]] =
		kgem_add_reloc(&sna->kgem,
			       sna->render.vertex_reloc[0],
			       bo,
			       I915_GEM_DOMAIN_VERTEX << 16,
			       delta);
	sna->render.vertex_reloc[0] = 0;
	sna->render.vertex_used = 0;
	sna->render.vertex_index = 0;

	if (bo)
		kgem_bo_destroy(&sna->kgem, bo);
}

static bool gen3_rectangle_begin(struct sna *sna,
				 const struct sna_composite_op *op)
{
	struct gen3_render_state *state = &sna->render_state.gen3;
	int ndwords, i1_cmd = 0, i1_len = 0;

	ndwords = 2;
	if (op->need_magic_ca_pass)
		ndwords += 100;
	if (sna->render.vertex_reloc[0] == 0)
		i1_len++, i1_cmd |= I1_LOAD_S(0), ndwords++;
	if (state->floats_per_vertex != op->floats_per_vertex)
		i1_len++, i1_cmd |= I1_LOAD_S(1), ndwords++;

	if (!kgem_check_batch(&sna->kgem, ndwords+1))
		return false;

	if (i1_cmd) {
		OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | i1_cmd | (i1_len - 1));
		if (sna->render.vertex_reloc[0] == 0)
			sna->render.vertex_reloc[0] = sna->kgem.nbatch++;
		if (state->floats_per_vertex != op->floats_per_vertex) {
			state->floats_per_vertex = op->floats_per_vertex;
			OUT_BATCH(state->floats_per_vertex << S1_VERTEX_WIDTH_SHIFT |
				  state->floats_per_vertex << S1_VERTEX_PITCH_SHIFT);
		}
	}

	if (sna->kgem.nbatch == 2 + state->last_vertex_offset) {
		state->vertex_offset = state->last_vertex_offset;
	} else {
		state->vertex_offset = sna->kgem.nbatch;
		OUT_BATCH(MI_NOOP); /* to be filled later */
		OUT_BATCH(MI_NOOP);
		sna->render.vertex_start = sna->render.vertex_index;
		state->last_vertex_offset = state->vertex_offset;
	}

	return true;
}

static int gen3_get_rectangles__flush(struct sna *sna, bool ca)
{
	if (!kgem_check_batch(&sna->kgem, ca ? 105: 5))
		return 0;
	if (sna->kgem.nexec > KGEM_EXEC_SIZE(&sna->kgem) - 2)
		return 0;
	if (sna->kgem.nreloc > KGEM_RELOC_SIZE(&sna->kgem) - 1)
		return 0;

	gen3_vertex_finish(sna, FALSE);
	assert(sna->render.vertex_index == 0);
	assert(sna->render.vertex_used == 0);
	return ARRAY_SIZE(sna->render.vertex_data);
}

inline static int gen3_get_rectangles(struct sna *sna,
				      const struct sna_composite_op *op,
				      int want)
{
	int rem = vertex_space(sna);

	DBG(("%s: want=%d, rem=%d\n",
	     __FUNCTION__, want*op->floats_per_rect, rem));

	assert(sna->render.vertex_index * op->floats_per_vertex == sna->render.vertex_used);
	if (op->floats_per_rect > rem) {
		DBG(("flushing vbo for %s: %d < %d\n",
		     __FUNCTION__, rem, op->floats_per_rect));
		rem = gen3_get_rectangles__flush(sna, op->need_magic_ca_pass);
		if (rem == 0)
			return 0;
	}

	if (sna->render_state.gen3.vertex_offset == 0 &&
	    !gen3_rectangle_begin(sna, op)) {
		DBG(("%s: flushing batch\n", __FUNCTION__));
		return 0;
	}

	if (want > 1 && want * op->floats_per_rect > rem)
		want = rem / op->floats_per_rect;
	sna->render.vertex_index += 3*want;

	assert(want);
	assert(sna->render.vertex_index * op->floats_per_vertex <= ARRAY_SIZE(sna->render.vertex_data));
	return want;
}

fastcall static void
gen3_render_composite_blt(struct sna *sna,
			  const struct sna_composite_op *op,
			  const struct sna_composite_rectangles *r)
{
	DBG(("%s: src=(%d, %d)+(%d, %d), mask=(%d, %d)+(%d, %d), dst=(%d, %d)+(%d, %d), size=(%d, %d)\n", __FUNCTION__,
	     r->src.x, r->src.y, op->src.offset[0], op->src.offset[1],
	     r->mask.x, r->mask.y, op->mask.offset[0], op->mask.offset[1],
	     r->dst.x, r->dst.y, op->dst.x, op->dst.y,
	     r->width, r->height));

	if (!gen3_get_rectangles(sna, op, 1)) {
		gen3_emit_composite_state(sna, op);
		gen3_get_rectangles(sna, op, 1);
	}

	op->prim_emit(sna, op, r);
}

fastcall static void
gen3_render_composite_box(struct sna *sna,
			  const struct sna_composite_op *op,
			  const BoxRec *box)
{
	struct sna_composite_rectangles r;

	DBG(("%s: src=+(%d, %d), mask=+(%d, %d), dst=+(%d, %d)\n",
	     __FUNCTION__,
	     op->src.offset[0], op->src.offset[1],
	     op->mask.offset[0], op->mask.offset[1],
	     op->dst.x, op->dst.y));

	if (!gen3_get_rectangles(sna, op, 1)) {
		gen3_emit_composite_state(sna, op);
		gen3_get_rectangles(sna, op, 1);
	}

	r.dst.x  = box->x1;
	r.dst.y  = box->y1;
	r.width  = box->x2 - box->x1;
	r.height = box->y2 - box->y1;
	r.src = r.mask = r.dst;

	op->prim_emit(sna, op, &r);
}

static void
gen3_render_composite_boxes(struct sna *sna,
			    const struct sna_composite_op *op,
			    const BoxRec *box, int nbox)
{
	DBG(("%s: nbox=%d, src=+(%d, %d), mask=+(%d, %d), dst=+(%d, %d)\n",
	     __FUNCTION__, nbox,
	     op->src.offset[0], op->src.offset[1],
	     op->mask.offset[0], op->mask.offset[1],
	     op->dst.x, op->dst.y));

	do {
		int nbox_this_time;

		nbox_this_time = gen3_get_rectangles(sna, op, nbox);
		if (nbox_this_time == 0) {
			gen3_emit_composite_state(sna, op);
			nbox_this_time = gen3_get_rectangles(sna, op, nbox);
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

static void
gen3_render_composite_done(struct sna *sna,
			   const struct sna_composite_op *op)
{
	assert(sna->render.op == op);

	gen3_vertex_flush(sna);
	sna->render.op = NULL;
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);

	DBG(("%s()\n", __FUNCTION__));

	sna_render_composite_redirect_done(sna, op);

	if (op->src.bo)
		kgem_bo_destroy(&sna->kgem, op->src.bo);
	if (op->mask.bo)
		kgem_bo_destroy(&sna->kgem, op->mask.bo);
}

static void
gen3_render_reset(struct sna *sna)
{
	struct gen3_render_state *state = &sna->render_state.gen3;

	state->need_invariant = TRUE;
	state->current_dst = 0;
	state->tex_count = 0;
	state->last_drawrect_limit = ~0U;
	state->last_target = 0;
	state->last_blend = 0;
	state->last_constants = 0;
	state->last_sampler = 0;
	state->last_shader = -1;
	state->last_diffuse = 0xcc00ffee;
	state->last_specular = 0xcc00ffee;

	state->floats_per_vertex = 0;
	state->last_floats_per_vertex = 0;
	state->last_vertex_offset = 0;
	state->vertex_offset = 0;

	assert(sna->render.vertex_used == 0);
	assert(sna->render.vertex_index == 0);
	assert(sna->render.vertex_reloc[0] == 0);
}

static Bool gen3_composite_channel_set_format(struct sna_composite_channel *channel,
					      CARD32 format)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(gen3_tex_formats); i++) {
		if (gen3_tex_formats[i].fmt == format) {
			channel->card_format = gen3_tex_formats[i].card_fmt;
			channel->rb_reversed = gen3_tex_formats[i].rb_reversed;
			return TRUE;
		}
	}
	return FALSE;
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

static Bool gen3_composite_channel_set_xformat(PicturePtr picture,
					       struct sna_composite_channel *channel,
					       int x, int y,
					       int width, int height)
{
	unsigned int i;

	if (PICT_FORMAT_A(picture->format) != 0)
		return FALSE;

	if (width == 0 || height == 0)
		return FALSE;

	if (!source_is_covered(picture, x, y, width, height))
		return FALSE;

	for (i = 0; i < ARRAY_SIZE(gen3_tex_formats); i++) {
		if (gen3_tex_formats[i].xfmt == picture->format) {
			channel->card_format = gen3_tex_formats[i].card_fmt;
			channel->rb_reversed = gen3_tex_formats[i].rb_reversed;
			channel->alpha_fixup = true;
			return TRUE;
		}
	}

	return FALSE;
}

static int
gen3_init_solid(struct sna_composite_channel *channel, uint32_t color)
{
	channel->u.gen3.mode = color;
	channel->u.gen3.type = SHADER_CONSTANT;
	if (color == 0)
		channel->u.gen3.type = SHADER_ZERO;
	else if (color == 0xff000000)
		channel->u.gen3.type = SHADER_BLACK;
	else if (color == 0xffffffff)
		channel->u.gen3.type = SHADER_WHITE;
	if ((color & 0xff000000) == 0xff000000)
		channel->is_opaque = true;

	/* for consistency */
	channel->repeat = RepeatNormal;
	channel->filter = PictFilterNearest;
	channel->pict_format = PICT_a8r8g8b8;
	channel->card_format = MAPSURF_32BIT | MT_32BIT_ARGB8888;

	return 1;
}

static void gen3_composite_channel_convert(struct sna_composite_channel *channel)
{
	if (channel->u.gen3.type == SHADER_TEXTURE)
		channel->repeat = gen3_texture_repeat(channel->repeat);
	else
		channel->repeat = gen3_gradient_repeat(channel->repeat);

	channel->filter = gen3_filter(channel->filter);
	if (channel->card_format == 0)
		gen3_composite_channel_set_format(channel, channel->pict_format);
}

static Bool gen3_gradient_setup(struct sna *sna,
				PicturePtr picture,
				struct sna_composite_channel *channel,
				int16_t ox, int16_t oy)
{
	int16_t dx, dy;

	if (picture->repeat == 0) {
		channel->repeat = RepeatNone;
	} else switch (picture->repeatType) {
	case RepeatNone:
	case RepeatNormal:
	case RepeatPad:
	case RepeatReflect:
		channel->repeat = picture->repeatType;
		break;
	default:
		return FALSE;
	}

	channel->bo =
		sna_render_get_gradient(sna,
					(PictGradient *)picture->pSourcePict);
	if (channel->bo == NULL)
		return FALSE;

	channel->pict_format = PICT_a8r8g8b8;
	channel->card_format = MAPSURF_32BIT | MT_32BIT_ARGB8888;
	channel->filter = PictFilterBilinear;
	channel->is_affine = sna_transform_is_affine(picture->transform);
	if (sna_transform_is_integer_translation(picture->transform, &dx, &dy)) {
		DBG(("%s: integer translation (%d, %d), removing\n",
		     __FUNCTION__, dx, dy));
		ox += dx;
		oy += dy;
		channel->transform = NULL;
	} else
		channel->transform = picture->transform;
	channel->width  = channel->bo->pitch / 4;
	channel->height = 1;
	channel->offset[0] = ox;
	channel->offset[1] = oy;
	channel->scale[0] = channel->scale[1] = 1;
	return TRUE;
}

static int
gen3_init_linear(struct sna *sna,
		 PicturePtr picture,
		 struct sna_composite_op *op,
		 struct sna_composite_channel *channel,
		 int ox, int oy)
{
	PictLinearGradient *linear =
		(PictLinearGradient *)picture->pSourcePict;
	float x0, y0, sf;
	float dx, dy, offset;
	int n;

	DBG(("%s: p1=(%f, %f), p2=(%f, %f)\n",
	     __FUNCTION__,
	     xFixedToDouble(linear->p1.x), xFixedToDouble(linear->p1.y),
	     xFixedToDouble(linear->p2.x), xFixedToDouble(linear->p2.y)));

	if (linear->p2.x == linear->p1.x && linear->p2.y == linear->p1.y)
		return 0;

	dx = xFixedToDouble(linear->p2.x - linear->p1.x);
	dy = xFixedToDouble(linear->p2.y - linear->p1.y);
	sf = dx*dx + dy*dy;
	dx /= sf;
	dy /= sf;

	x0 = xFixedToDouble(linear->p1.x);
	y0 = xFixedToDouble(linear->p1.y);
	offset = dx*x0 + dy*y0;

	n = op->u.gen3.num_constants;
	channel->u.gen3.constants = FS_C0 + n / 4;
	op->u.gen3.constants[n++] = dx;
	op->u.gen3.constants[n++] = dy;
	op->u.gen3.constants[n++] = -offset;
	op->u.gen3.constants[n++] = 0;

	if (!gen3_gradient_setup(sna, picture, channel, ox, oy))
		return 0;

	channel->u.gen3.type = SHADER_LINEAR;
	op->u.gen3.num_constants = n;

	DBG(("%s: dx=%f, dy=%f, offset=%f, constants=%d\n",
	     __FUNCTION__, dx, dy, -offset, channel->u.gen3.constants - FS_C0));
	return 1;
}

static int
gen3_init_radial(struct sna *sna,
		 PicturePtr picture,
		 struct sna_composite_op *op,
		 struct sna_composite_channel *channel,
		 int ox, int oy)
{
	PictRadialGradient *radial = (PictRadialGradient *)picture->pSourcePict;
	double dx, dy, dr, r1;
	int n;

	dx = xFixedToDouble(radial->c2.x - radial->c1.x);
	dy = xFixedToDouble(radial->c2.y - radial->c1.y);
	dr = xFixedToDouble(radial->c2.radius - radial->c1.radius);

	r1 = xFixedToDouble(radial->c1.radius);

	n = op->u.gen3.num_constants;
	channel->u.gen3.constants = FS_C0 + n / 4;
	if (radial->c2.x == radial->c1.x && radial->c2.y == radial->c1.y) {
		if (radial->c2.radius == radial->c1.radius) {
			channel->u.gen3.type = SHADER_ZERO;
			return 1;
		}

		op->u.gen3.constants[n++] = xFixedToDouble(radial->c1.x) / dr;
		op->u.gen3.constants[n++] = xFixedToDouble(radial->c1.y) / dr;
		op->u.gen3.constants[n++] = 1. / dr;
		op->u.gen3.constants[n++] = -r1 / dr;

		channel->u.gen3.mode = RADIAL_ONE;
	} else {
		op->u.gen3.constants[n++] = -xFixedToDouble(radial->c1.x);
		op->u.gen3.constants[n++] = -xFixedToDouble(radial->c1.y);
		op->u.gen3.constants[n++] = r1;
		op->u.gen3.constants[n++] = -4 * (dx*dx + dy*dy - dr*dr);

		op->u.gen3.constants[n++] = -2 * dx;
		op->u.gen3.constants[n++] = -2 * dy;
		op->u.gen3.constants[n++] = -2 * r1 * dr;
		op->u.gen3.constants[n++] = 1 / (2 * (dx*dx + dy*dy - dr*dr));

		channel->u.gen3.mode = RADIAL_TWO;
	}

	if (!gen3_gradient_setup(sna, picture, channel, ox, oy))
		return 0;

	channel->u.gen3.type = SHADER_RADIAL;
	op->u.gen3.num_constants = n;
	return 1;
}

static Bool
gen3_composite_picture(struct sna *sna,
		       PicturePtr picture,
		       struct sna_composite_op *op,
		       struct sna_composite_channel *channel,
		       int16_t x, int16_t y,
		       int16_t w, int16_t h,
		       int16_t dst_x, int16_t dst_y)
{
	PixmapPtr pixmap;
	uint32_t color;
	int16_t dx, dy;

	DBG(("%s: (%d, %d)x(%d, %d), dst=(%d, %d)\n",
	     __FUNCTION__, x, y, w, h, dst_x, dst_y));

	channel->card_format = 0;

	if (picture->pDrawable == NULL) {
		SourcePict *source = picture->pSourcePict;
		int ret = 0;

		switch (source->type) {
		case SourcePictTypeSolidFill:
			ret = gen3_init_solid(channel, source->solidFill.color);
			break;

		case SourcePictTypeLinear:
			ret = gen3_init_linear(sna, picture, op, channel,
					       x - dst_x, y - dst_y);
			break;

		case SourcePictTypeRadial:
			ret = gen3_init_radial(sna, picture, op, channel,
					       x - dst_x, y - dst_y);
			break;
		}

		if (ret == 0)
			ret = sna_render_picture_fixup(sna, picture, channel,
						       x, y, w, h, dst_x, dst_y);
		return ret;
	}

	if (sna_picture_is_solid(picture, &color))
		return gen3_init_solid(channel, color);

	if (!gen3_check_repeat(picture->repeat))
		return sna_render_picture_fixup(sna, picture, channel,
						x, y, w, h, dst_x, dst_y);

	if (!gen3_check_filter(picture->filter))
		return sna_render_picture_fixup(sna, picture, channel,
						x, y, w, h, dst_x, dst_y);

	channel->repeat = picture->repeat ? picture->repeatType : RepeatNone;
	channel->filter = picture->filter;
	channel->pict_format = picture->format;

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

	if (!gen3_composite_channel_set_format(channel, picture->format) &&
	    !gen3_composite_channel_set_xformat(picture, channel, x, y, w, h))
		return sna_render_picture_convert(sna, picture, channel, pixmap,
						  x, y, w, h, dst_x, dst_y);

	if (pixmap->drawable.width > 2048 || pixmap->drawable.height > 2048)
		return sna_render_picture_extract(sna, picture, channel,
						  x, y, w, h, dst_x, dst_y);

	return sna_render_pixmap_bo(sna, channel, pixmap,
				    x, y, w, h, dst_x, dst_y);
}

static inline Bool
picture_is_cpu(PicturePtr picture)
{
	if (!picture->pDrawable)
		return FALSE;

	/* If it is a solid, try to use the render paths */
	if (picture->pDrawable->width  == 1 &&
	    picture->pDrawable->height == 1 &&
	    picture->repeat)
		return FALSE;

	return is_cpu(picture->pDrawable);
}

static Bool
try_blt(struct sna *sna,
	PicturePtr source,
	int width, int height)
{
	if (sna->kgem.mode != KGEM_RENDER) {
		DBG(("%s: already performing BLT\n", __FUNCTION__));
		return TRUE;
	}

	if (width > 2048 || height > 2048) {
		DBG(("%s: operation too large for 3D pipe (%d, %d)\n",
		     __FUNCTION__, width, height));
		return TRUE;
	}

	/* If we can sample directly from user-space, do so */
	if (sna->kgem.has_vmap)
		return FALSE;

	/* is the source picture only in cpu memory e.g. a shm pixmap? */
	return picture_is_cpu(source);
}

static void
gen3_align_vertex(struct sna *sna,
		  struct sna_composite_op *op)
{
	if (op->floats_per_vertex != sna->render_state.gen3.last_floats_per_vertex) {
		DBG(("aligning vertex: was %d, now %d floats per vertex, %d->%d\n",
		     sna->render_state.gen3.last_floats_per_vertex,
		     op->floats_per_vertex,
		     sna->render.vertex_index,
		     (sna->render.vertex_used + op->floats_per_vertex - 1) / op->floats_per_vertex));
		sna->render.vertex_index = (sna->render.vertex_used + op->floats_per_vertex - 1) / op->floats_per_vertex;
		sna->render.vertex_used = sna->render.vertex_index * op->floats_per_vertex;
		sna->render_state.gen3.last_floats_per_vertex = op->floats_per_vertex;
	}
}

static void
reduce_damage(struct sna_composite_op *op,
	      int dst_x, int dst_y,
	      int width, int height)
{
	BoxRec r;

	if (op->damage == NULL)
		return;

	r.x1 = dst_x + op->dst.x;
	r.x2 = r.x1 + width;

	r.y1 = dst_y + op->dst.y;
	r.y2 = r.y1 + height;

	if (sna_damage_contains_box(*op->damage, &r) == PIXMAN_REGION_IN)
		op->damage = NULL;
}

static Bool
gen3_composite_set_target(struct sna_composite_op *op, PicturePtr dst)
{
	struct sna_pixmap *priv;

	op->dst.pixmap = get_drawable_pixmap(dst->pDrawable);
	op->dst.format = dst->format;
	op->dst.width = op->dst.pixmap->drawable.width;
	op->dst.height = op->dst.pixmap->drawable.height;
	priv = sna_pixmap(op->dst.pixmap);

	priv = sna_pixmap_force_to_gpu(op->dst.pixmap);
	if (priv == NULL)
		return FALSE;

	op->dst.bo = priv->gpu_bo;
	if (!priv->gpu_only &&
	    !sna_damage_is_all(&priv->gpu_damage, op->dst.width, op->dst.height))
		op->damage = &priv->gpu_damage;

	get_drawable_deltas(dst->pDrawable, op->dst.pixmap,
			    &op->dst.x, &op->dst.y);

	DBG(("%s: pixmap=%p, format=%08x, size=%dx%d, pitch=%d, delta=(%d,%d)\n",
	     __FUNCTION__,
	     op->dst.pixmap, (int)op->dst.format,
	     op->dst.width, op->dst.height,
	     op->dst.bo->pitch,
	     op->dst.x, op->dst.y));

	return TRUE;
}

static inline uint8_t mult(uint32_t s, uint32_t m, int shift)
{
	s = (s >> shift) & 0xff;
	m = (m >> shift) & 0xff;
	return (s * m) >> 8;
}

static inline bool is_constant_ps(uint32_t type)
{
	switch (type) {
	case SHADER_NONE: /* be warned! */
	case SHADER_ZERO:
	case SHADER_BLACK:
	case SHADER_WHITE:
	case SHADER_CONSTANT:
		return true;
	default:
		return false;
	}
}

static Bool
gen3_render_composite(struct sna *sna,
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

#if NO_COMPOSITE
	if (mask)
		return FALSE;

	return sna_blt_composite(sna, op,
				 src, dst,
				 src_x, src_y,
				 dst_x, dst_y,
				 width, height, tmp);
#endif

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

	if (op >= ARRAY_SIZE(gen3_blend_op)) {
		DBG(("%s: fallback due to unhandled blend op: %d\n",
		     __FUNCTION__, op));
		return FALSE;
	}

	if (!gen3_check_dst_format(dst->format)) {
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

	memset(&tmp->u.gen3, 0, sizeof(tmp->u.gen3));

	if (!gen3_composite_set_target(tmp, dst)) {
		DBG(("%s: unable to set render target\n",
		     __FUNCTION__));
		return FALSE;
	}

	if (width && height)
		reduce_damage(tmp, dst_x, dst_y, width, height);

	tmp->op = op;
	tmp->rb_reversed = gen3_dst_rb_reversed(tmp->dst.format);
	if (tmp->dst.width > 2048 || tmp->dst.height > 2048 ||
	    !gen3_check_pitch_3d(tmp->dst.bo)) {
		if (!sna_render_composite_redirect(sna, tmp,
						   dst_x, dst_y, width, height))
			return FALSE;
	}

	tmp->src.u.gen3.type = SHADER_TEXTURE;
	tmp->src.is_affine = TRUE;
	DBG(("%s: preparing source\n", __FUNCTION__));
	switch (gen3_composite_picture(sna, src, tmp, &tmp->src,
				       src_x, src_y,
				       width, height,
				       dst_x, dst_y)) {
	case -1:
		goto cleanup_dst;
	case 0:
		tmp->src.u.gen3.type = SHADER_ZERO;
		break;
	case 1:
		gen3_composite_channel_convert(&tmp->src);
		break;
	}
	DBG(("%s: source type=%d\n", __FUNCTION__, tmp->src.u.gen3.type));

	tmp->mask.u.gen3.type = SHADER_NONE;
	tmp->mask.is_affine = TRUE;
	tmp->need_magic_ca_pass = FALSE;
	tmp->has_component_alpha = FALSE;
	if (mask && tmp->src.u.gen3.type != SHADER_ZERO) {
		tmp->mask.u.gen3.type = SHADER_TEXTURE;
		DBG(("%s: preparing mask\n", __FUNCTION__));
		switch (gen3_composite_picture(sna, mask, tmp, &tmp->mask,
					       mask_x, mask_y,
					       width,  height,
					       dst_x,  dst_y)) {
		case -1:
			goto cleanup_src;
		case 0:
			tmp->mask.u.gen3.type = SHADER_ZERO;
			break;
		case 1:
			gen3_composite_channel_convert(&tmp->mask);
			break;
		}
		DBG(("%s: mask type=%d\n", __FUNCTION__, tmp->mask.u.gen3.type));

		if (tmp->mask.u.gen3.type == SHADER_ZERO) {
			if (tmp->src.bo) {
				kgem_bo_destroy(&sna->kgem,
						tmp->src.bo);
				tmp->src.bo = NULL;
			}
			tmp->src.u.gen3.type = SHADER_ZERO;
			tmp->mask.u.gen3.type = SHADER_NONE;
		}

		if (tmp->mask.u.gen3.type != SHADER_NONE &&
		    mask->componentAlpha && PICT_FORMAT_RGB(mask->format)) {
			/* Check if it's component alpha that relies on a source alpha
			 * and on the source value.  We can only get one of those
			 * into the single source value that we get to blend with.
			 */
			tmp->has_component_alpha = TRUE;
			if (tmp->mask.u.gen3.type == SHADER_WHITE) {
				tmp->mask.u.gen3.type = SHADER_NONE;
				tmp->has_component_alpha = FALSE;
			} else if (tmp->src.u.gen3.type == SHADER_WHITE) {
				tmp->src = tmp->mask;
				tmp->mask.u.gen3.type = SHADER_NONE;
				tmp->mask.bo = NULL;
				tmp->has_component_alpha = FALSE;
			} else if (is_constant_ps(tmp->src.u.gen3.type) &&
				   is_constant_ps(tmp->mask.u.gen3.type)) {
				uint32_t a,r,g,b;

				a = mult(tmp->src.u.gen3.mode,
					 tmp->mask.u.gen3.mode,
					 24);
				r = mult(tmp->src.u.gen3.mode,
					 tmp->mask.u.gen3.mode,
					 16);
				g = mult(tmp->src.u.gen3.mode,
					 tmp->mask.u.gen3.mode,
					 8);
				b = mult(tmp->src.u.gen3.mode,
					 tmp->mask.u.gen3.mode,
					 0);

				DBG(("%s: combining constant source/mask: %x x %x -> %x\n",
				     __FUNCTION__,
				     tmp->src.u.gen3.mode,
				     tmp->mask.u.gen3.mode,
				     a << 24 | r << 16 | g << 8 | b));

				tmp->src.u.gen3.type = SHADER_CONSTANT;
				tmp->src.u.gen3.mode =
					a << 24 | r << 16 | g << 8 | b;

				tmp->mask.u.gen3.type = SHADER_NONE;
				tmp->has_component_alpha = FALSE;
			} else if (gen3_blend_op[op].src_alpha &&
				   (gen3_blend_op[op].src_blend != BLENDFACT_ZERO)) {
				if (op != PictOpOver)
					goto cleanup_mask;

				tmp->need_magic_ca_pass = TRUE;
				tmp->op = PictOpOutReverse;
				sna->render.vertex_start = sna->render.vertex_index;
			}
		}
	}
	DBG(("%s: final src/mask type=%d/%d, affine=%d/%d\n", __FUNCTION__,
	     tmp->src.u.gen3.type, tmp->mask.u.gen3.type,
	     tmp->src.is_affine, tmp->mask.is_affine));

	tmp->prim_emit = gen3_emit_composite_primitive;
	if (is_constant_ps(tmp->mask.u.gen3.type)) {
		switch (tmp->src.u.gen3.type) {
		case SHADER_NONE:
		case SHADER_ZERO:
		case SHADER_BLACK:
		case SHADER_WHITE:
		case SHADER_CONSTANT:
			tmp->prim_emit = gen3_emit_composite_primitive_constant;
			break;
		case SHADER_LINEAR:
		case SHADER_RADIAL:
			if (tmp->src.transform == NULL)
				tmp->prim_emit = gen3_emit_composite_primitive_identity_gradient;
			else if (tmp->src.is_affine)
				tmp->prim_emit = gen3_emit_composite_primitive_affine_gradient;
			break;
		case SHADER_TEXTURE:
			if (tmp->src.transform == NULL)
				tmp->prim_emit = gen3_emit_composite_primitive_identity_source;
			else if (tmp->src.is_affine)
				tmp->prim_emit = gen3_emit_composite_primitive_affine_source;
			break;
		}
	} else if (tmp->mask.u.gen3.type == SHADER_TEXTURE) {
		if (tmp->mask.transform == NULL) {
			if (is_constant_ps(tmp->src.u.gen3.type))
				tmp->prim_emit = gen3_emit_composite_primitive_constant_identity_mask;
			else if (tmp->src.transform == NULL)
				tmp->prim_emit = gen3_emit_composite_primitive_identity_source_mask;
			else if (tmp->src.is_affine)
				tmp->prim_emit = gen3_emit_composite_primitive_affine_source_mask;
		}
	}

	tmp->floats_per_vertex = 2;
	if (!is_constant_ps(tmp->src.u.gen3.type))
		tmp->floats_per_vertex += tmp->src.is_affine ? 2 : 4;
	if (!is_constant_ps(tmp->mask.u.gen3.type))
		tmp->floats_per_vertex += tmp->mask.is_affine ? 2 : 4;
	DBG(("%s: floats_per_vertex = 2 + %d + %d = %d\n", __FUNCTION__,
	     !is_constant_ps(tmp->src.u.gen3.type) ? tmp->src.is_affine ? 2 : 4 : 0,
	     !is_constant_ps(tmp->mask.u.gen3.type) ? tmp->mask.is_affine ? 2 : 4 : 0,
	     tmp->floats_per_vertex));
	tmp->floats_per_rect = 3 * tmp->floats_per_vertex;

	tmp->blt   = gen3_render_composite_blt;
	tmp->box   = gen3_render_composite_box;
	tmp->boxes = gen3_render_composite_boxes;
	tmp->done  = gen3_render_composite_done;

	if (!kgem_check_bo(&sna->kgem,
			   tmp->dst.bo, tmp->src.bo, tmp->mask.bo,
			   NULL))
		kgem_submit(&sna->kgem);

	if (kgem_bo_is_dirty(tmp->src.bo) || kgem_bo_is_dirty(tmp->mask.bo)) {
		if (tmp->src.bo == tmp->dst.bo || tmp->mask.bo == tmp->dst.bo) {
			kgem_emit_flush(&sna->kgem);
		} else {
			OUT_BATCH(_3DSTATE_MODES_5_CMD |
				  PIPELINE_FLUSH_RENDER_CACHE |
				  PIPELINE_FLUSH_TEXTURE_CACHE);
			kgem_clear_dirty(&sna->kgem);
		}
	}

	gen3_emit_composite_state(sna, tmp);
	gen3_align_vertex(sna, tmp);

	sna->render.op = tmp;
	return TRUE;

cleanup_mask:
	if (tmp->mask.bo)
		kgem_bo_destroy(&sna->kgem, tmp->mask.bo);
cleanup_src:
	if (tmp->src.bo)
		kgem_bo_destroy(&sna->kgem, tmp->src.bo);
cleanup_dst:
	if (tmp->redirect.real_bo)
		kgem_bo_destroy(&sna->kgem, tmp->dst.bo);
	return FALSE;
}

static void
gen3_emit_composite_spans_vertex(struct sna *sna,
				 const struct sna_composite_spans_op *op,
				 int16_t x, int16_t y,
				 float opacity)
{
	gen3_emit_composite_dstcoord(sna, x + op->base.dst.x, y + op->base.dst.y);
	gen3_emit_composite_texcoord(sna, &op->base.src, x, y);
	OUT_VERTEX(opacity);
}

fastcall static void
gen3_emit_composite_spans_primitive_zero(struct sna *sna,
					 const struct sna_composite_spans_op *op,
					 const BoxRec *box,
					 float opacity)
{
	float *v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 6;

	v[0] = op->base.dst.x + box->x2;
	v[1] = op->base.dst.y + box->y2;

	v[2] = op->base.dst.x + box->x1;
	v[3] = v[1];

	v[4] = v[2];
	v[5] = op->base.dst.x + box->y1;
}

fastcall static void
gen3_emit_composite_spans_primitive_zero_no_offset(struct sna *sna,
						   const struct sna_composite_spans_op *op,
						   const BoxRec *box,
						   float opacity)
{
	float *v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 6;

	v[0] = box->x2;
	v[3] = v[1] = box->y2;
	v[4] = v[2] = box->x1;
	v[5] = box->y1;
}

fastcall static void
gen3_emit_composite_spans_primitive_constant(struct sna *sna,
					     const struct sna_composite_spans_op *op,
					     const BoxRec *box,
					     float opacity)
{
	float *v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 9;

	v[0] = op->base.dst.x + box->x2;
	v[6] = v[3] = op->base.dst.x + box->x1;
	v[4] = v[1] = op->base.dst.y + box->y2;
	v[7] = op->base.dst.y + box->y1;
	v[8] = v[5] = v[2] = opacity;
}

fastcall static void
gen3_emit_composite_spans_primitive_constant_no_offset(struct sna *sna,
						       const struct sna_composite_spans_op *op,
						       const BoxRec *box,
						       float opacity)
{
	float *v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 9;

	v[0] = box->x2;
	v[6] = v[3] = box->x1;
	v[4] = v[1] = box->y2;
	v[7] = box->y1;
	v[8] = v[5] = v[2] = opacity;
}

fastcall static void
gen3_emit_composite_spans_primitive_identity_source(struct sna *sna,
						    const struct sna_composite_spans_op *op,
						    const BoxRec *box,
						    float opacity)
{
	float *v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 15;

	v[0] = op->base.dst.x + box->x2;
	v[1] = op->base.dst.y + box->y2;
	v[2] = (op->base.src.offset[0] + box->x2) * op->base.src.scale[0];
	v[3] = (op->base.src.offset[1] + box->y2) * op->base.src.scale[1];
	v[4] = opacity;

	v[5] = op->base.dst.x + box->x1;
	v[6] = v[1];
	v[7] = (op->base.src.offset[0] + box->x1) * op->base.src.scale[0];
	v[8] = v[3];
	v[9] = opacity;

	v[10] = v[5];
	v[11] = op->base.dst.y + box->y1;
	v[12] = v[7];
	v[13] = (op->base.src.offset[1] + box->y1) * op->base.src.scale[1];
	v[14] = opacity;
}

fastcall static void
gen3_emit_composite_spans_primitive_affine_source(struct sna *sna,
						  const struct sna_composite_spans_op *op,
						  const BoxRec *box,
						  float opacity)
{
	PictTransform *transform = op->base.src.transform;
	float x, y, *v;

	v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 15;

	v[0]  = op->base.dst.x + box->x2;
	v[6]  = v[1] = op->base.dst.y + box->y2;
	v[10] = v[5] = op->base.dst.x + box->x1;
	v[11] = op->base.dst.y + box->y1;
	v[4]  = opacity;
	v[9]  = opacity;
	v[14] = opacity;

	_sna_get_transformed_coordinates((int)op->base.src.offset[0] + box->x2,
					 (int)op->base.src.offset[1] + box->y2,
					 transform,
					 &x, &y);
	v[2] = x * op->base.src.scale[0];
	v[3] = y * op->base.src.scale[1];

	_sna_get_transformed_coordinates((int)op->base.src.offset[0] + box->x1,
					 (int)op->base.src.offset[1] + box->y2,
					 transform,
					 &x, &y);
	v[7] = x * op->base.src.scale[0];
	v[8] = y * op->base.src.scale[1];

	_sna_get_transformed_coordinates((int)op->base.src.offset[0] + box->x1,
					 (int)op->base.src.offset[1] + box->y1,
					 transform,
					 &x, &y);
	v[12] = x * op->base.src.scale[0];
	v[13] = y * op->base.src.scale[1];
}

fastcall static void
gen3_emit_composite_spans_primitive_identity_gradient(struct sna *sna,
						      const struct sna_composite_spans_op *op,
						      const BoxRec *box,
						      float opacity)
{
	float *v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 15;

	v[0] = op->base.dst.x + box->x2;
	v[1] = op->base.dst.y + box->y2;
	v[2] = op->base.src.offset[0] + box->x2;
	v[3] = op->base.src.offset[1] + box->y2;
	v[4] = opacity;

	v[5] = op->base.dst.x + box->x1;
	v[6] = v[1];
	v[7] = op->base.src.offset[0] + box->x1;
	v[8] = v[3];
	v[9] = opacity;

	v[10] = v[5];
	v[11] = op->base.dst.y + box->y1;
	v[12] = v[7];
	v[13] = op->base.src.offset[1] + box->y1;
	v[14] = opacity;
}

fastcall static void
gen3_emit_composite_spans_primitive_affine_gradient(struct sna *sna,
						    const struct sna_composite_spans_op *op,
						    const BoxRec *box,
						    float opacity)
{
	PictTransform *transform = op->base.src.transform;
	float *v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 15;

	v[0] = op->base.dst.x + box->x2;
	v[1] = op->base.dst.y + box->y2;
	_sna_get_transformed_coordinates((int)op->base.src.offset[0] + box->x2,
					 (int)op->base.src.offset[1] + box->y2,
					 transform,
					 &v[2], &v[3]);
	v[4] = opacity;

	v[5] = op->base.dst.x + box->x1;
	v[6] = v[1];
	_sna_get_transformed_coordinates((int)op->base.src.offset[0] + box->x1,
					 (int)op->base.src.offset[1] + box->y2,
					 transform,
					 &v[7], &v[8]);
	v[9] = opacity;

	v[10] = v[5];
	v[11] = op->base.dst.y + box->y1;
	_sna_get_transformed_coordinates((int)op->base.src.offset[0] + box->x1,
					 (int)op->base.src.offset[1] + box->y1,
					 transform,
					 &v[12], &v[13]);
	v[14] = opacity;
}

fastcall static void
gen3_emit_composite_spans_primitive(struct sna *sna,
				    const struct sna_composite_spans_op *op,
				    const BoxRec *box,
				    float opacity)
{
	gen3_emit_composite_spans_vertex(sna, op,
					 box->x2, box->y2,
					 opacity);
	gen3_emit_composite_spans_vertex(sna, op,
					 box->x1, box->y2,
					 opacity);
	gen3_emit_composite_spans_vertex(sna, op,
					 box->x1, box->y1,
					 opacity);
}

fastcall static void
gen3_render_composite_spans_box(struct sna *sna,
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

	if (gen3_get_rectangles(sna, &op->base, 1) == 0) {
		gen3_emit_composite_state(sna, &op->base);
		gen3_get_rectangles(sna, &op->base, 1);
	}

	op->prim_emit(sna, op, box, opacity);
}

static void
gen3_render_composite_spans_boxes(struct sna *sna,
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

		nbox_this_time = gen3_get_rectangles(sna, &op->base, nbox);
		if (nbox_this_time == 0) {
			gen3_emit_composite_state(sna, &op->base);
			nbox_this_time = gen3_get_rectangles(sna, &op->base, nbox);
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
gen3_render_composite_spans_done(struct sna *sna,
				 const struct sna_composite_spans_op *op)
{
	gen3_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);

	DBG(("%s()\n", __FUNCTION__));

	sna_render_composite_redirect_done(sna, &op->base);
	if (op->base.src.bo)
		kgem_bo_destroy(&sna->kgem, op->base.src.bo);
}

static Bool
gen3_render_composite_spans(struct sna *sna,
			    uint8_t op,
			    PicturePtr src,
			    PicturePtr dst,
			    int16_t src_x,  int16_t src_y,
			    int16_t dst_x,  int16_t dst_y,
			    int16_t width,  int16_t height,
			    struct sna_composite_spans_op *tmp)
{
	bool no_offset;

	DBG(("%s(src=(%d, %d), dst=(%d, %d), size=(%d, %d))\n", __FUNCTION__,
	     src_x, src_y, dst_x, dst_y, width, height));

#if NO_COMPOSITE_SPANS
	return FALSE;
#endif

	if (op >= ARRAY_SIZE(gen3_blend_op)) {
		DBG(("%s: fallback due to unhandled blend op: %d\n",
		     __FUNCTION__, op));
		return FALSE;
	}

	if (!gen3_check_dst_format(dst->format)) {
		DBG(("%s: fallback due to unhandled dst format: %x\n",
		     __FUNCTION__, dst->format));
		return FALSE;
	}

	if (need_tiling(sna, width, height))
		return FALSE;

	if (!gen3_composite_set_target(&tmp->base, dst)) {
		DBG(("%s: unable to set render target\n",
		     __FUNCTION__));
		return FALSE;
	}

	if (width && height)
		reduce_damage(&tmp->base, dst_x, dst_y, width, height);

	tmp->base.op = op;
	tmp->base.rb_reversed = gen3_dst_rb_reversed(tmp->base.dst.format);
	if (tmp->base.dst.width > 2048 || tmp->base.dst.height > 2048 ||
	    !gen3_check_pitch_3d(tmp->base.dst.bo)) {
		if (!sna_render_composite_redirect(sna, &tmp->base,
						   dst_x, dst_y, width, height))
			return FALSE;
	}

	tmp->base.src.u.gen3.type = SHADER_TEXTURE;
	tmp->base.src.is_affine = TRUE;
	DBG(("%s: preparing source\n", __FUNCTION__));
	switch (gen3_composite_picture(sna, src, &tmp->base, &tmp->base.src,
				       src_x, src_y,
				       width, height,
				       dst_x, dst_y)) {
	case -1:
		goto cleanup_dst;
	case 0:
		tmp->base.src.u.gen3.type = SHADER_ZERO;
		break;
	case 1:
		gen3_composite_channel_convert(&tmp->base.src);
		break;
	}
	DBG(("%s: source type=%d\n", __FUNCTION__, tmp->base.src.u.gen3.type));

	if (tmp->base.src.u.gen3.type != SHADER_ZERO)
		tmp->base.mask.u.gen3.type = SHADER_OPACITY;

	no_offset = tmp->base.dst.x == 0 && tmp->base.dst.y == 0;
	tmp->prim_emit = gen3_emit_composite_spans_primitive;
	switch (tmp->base.src.u.gen3.type) {
	case SHADER_NONE:
		assert(0);
	case SHADER_ZERO:
		tmp->prim_emit = no_offset ? gen3_emit_composite_spans_primitive_zero_no_offset : gen3_emit_composite_spans_primitive_zero;
		break;
	case SHADER_BLACK:
	case SHADER_WHITE:
	case SHADER_CONSTANT:
		tmp->prim_emit = no_offset ? gen3_emit_composite_spans_primitive_constant_no_offset : gen3_emit_composite_spans_primitive_constant;
		break;
	case SHADER_LINEAR:
	case SHADER_RADIAL:
		if (tmp->base.src.transform == NULL)
			tmp->prim_emit = gen3_emit_composite_spans_primitive_identity_gradient;
		else if (tmp->base.src.is_affine)
			tmp->prim_emit = gen3_emit_composite_spans_primitive_affine_gradient;
		break;
	case SHADER_TEXTURE:
		if (tmp->base.src.transform == NULL)
			tmp->prim_emit = gen3_emit_composite_spans_primitive_identity_source;
		else if (tmp->base.src.is_affine)
			tmp->prim_emit = gen3_emit_composite_spans_primitive_affine_source;
		break;
	}

	tmp->base.floats_per_vertex = 2;
	if (!is_constant_ps(tmp->base.src.u.gen3.type))
		tmp->base.floats_per_vertex += tmp->base.src.is_affine ? 2 : 3;
	tmp->base.floats_per_vertex +=
		tmp->base.mask.u.gen3.type == SHADER_OPACITY;
	tmp->base.floats_per_rect = 3 * tmp->base.floats_per_vertex;

	tmp->box   = gen3_render_composite_spans_box;
	tmp->boxes = gen3_render_composite_spans_boxes;
	tmp->done  = gen3_render_composite_spans_done;

	if (!kgem_check_bo(&sna->kgem,
			   tmp->base.dst.bo, tmp->base.src.bo,
			   NULL))
		kgem_submit(&sna->kgem);

	if (kgem_bo_is_dirty(tmp->base.src.bo)) {
		if (tmp->base.src.bo == tmp->base.dst.bo) {
			kgem_emit_flush(&sna->kgem);
		} else {
			OUT_BATCH(_3DSTATE_MODES_5_CMD |
				  PIPELINE_FLUSH_RENDER_CACHE |
				  PIPELINE_FLUSH_TEXTURE_CACHE);
			kgem_clear_dirty(&sna->kgem);
		}
	}

	gen3_emit_composite_state(sna, &tmp->base);
	gen3_align_vertex(sna, &tmp->base);
	return TRUE;

cleanup_dst:
	if (tmp->base.redirect.real_bo)
		kgem_bo_destroy(&sna->kgem, tmp->base.dst.bo);
	return FALSE;
}

static void
gen3_emit_video_state(struct sna *sna,
		      struct sna_video *video,
		      struct sna_video_frame *frame,
		      PixmapPtr pixmap,
		      struct kgem_bo *dst_bo,
		      int width, int height)
{
	uint32_t shader_offset;
	uint32_t ms3;

	gen3_emit_target(sna, dst_bo, width, height,
			 sna_format_for_depth(pixmap->drawable.depth));

	/* XXX share with composite? Is it worth the effort? */
	OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
		  I1_LOAD_S(1) | I1_LOAD_S(2) | I1_LOAD_S(6) |
		  2);
	OUT_BATCH((4 << S1_VERTEX_WIDTH_SHIFT) | (4 << S1_VERTEX_PITCH_SHIFT));
	OUT_BATCH(S2_TEXCOORD_FMT(0, TEXCOORDFMT_2D) |
		  S2_TEXCOORD_FMT(1, TEXCOORDFMT_NOT_PRESENT) |
		  S2_TEXCOORD_FMT(2, TEXCOORDFMT_NOT_PRESENT) |
		  S2_TEXCOORD_FMT(3, TEXCOORDFMT_NOT_PRESENT) |
		  S2_TEXCOORD_FMT(4, TEXCOORDFMT_NOT_PRESENT) |
		  S2_TEXCOORD_FMT(5, TEXCOORDFMT_NOT_PRESENT) |
		  S2_TEXCOORD_FMT(6, TEXCOORDFMT_NOT_PRESENT) |
		  S2_TEXCOORD_FMT(7, TEXCOORDFMT_NOT_PRESENT));
	OUT_BATCH((2 << S6_CBUF_SRC_BLEND_FACT_SHIFT) |
		  (1 << S6_CBUF_DST_BLEND_FACT_SHIFT) |
		  S6_COLOR_WRITE_ENABLE);

	sna->render_state.gen3.last_blend = 0;
	sna->render_state.gen3.last_sampler = 0;
	sna->render_state.gen3.floats_per_vertex = 4;
	sna->render_state.gen3.last_shader = -1;
	sna->render_state.gen3.last_constants = 0;

	if (!is_planar_fourcc(frame->id)) {
		OUT_BATCH(_3DSTATE_PIXEL_SHADER_CONSTANTS | 4);
		OUT_BATCH(0x0000001);	/* constant 0 */
		/* constant 0: brightness/contrast */
		OUT_BATCH_F(video->brightness / 128.0);
		OUT_BATCH_F(video->contrast / 255.0);
		OUT_BATCH_F(0.0);
		OUT_BATCH_F(0.0);

		OUT_BATCH(_3DSTATE_SAMPLER_STATE | 3);
		OUT_BATCH(0x00000001);
		OUT_BATCH(SS2_COLORSPACE_CONVERSION |
			  (FILTER_LINEAR << SS2_MAG_FILTER_SHIFT) |
			  (FILTER_LINEAR << SS2_MIN_FILTER_SHIFT));
		OUT_BATCH((TEXCOORDMODE_CLAMP_EDGE <<
			   SS3_TCX_ADDR_MODE_SHIFT) |
			  (TEXCOORDMODE_CLAMP_EDGE <<
			   SS3_TCY_ADDR_MODE_SHIFT) |
			  (0 << SS3_TEXTUREMAP_INDEX_SHIFT) |
			  SS3_NORMALIZED_COORDS);
		OUT_BATCH(0x00000000);

		OUT_BATCH(_3DSTATE_MAP_STATE | 3);
		OUT_BATCH(0x00000001);	/* texture map #1 */
		OUT_BATCH(kgem_add_reloc(&sna->kgem, sna->kgem.nbatch,
					 frame->bo,
					 I915_GEM_DOMAIN_SAMPLER << 16,
					 0));

		ms3 = MAPSURF_422;
		switch (frame->id) {
		case FOURCC_YUY2:
			ms3 |= MT_422_YCRCB_NORMAL;
			break;
		case FOURCC_UYVY:
			ms3 |= MT_422_YCRCB_SWAPY;
			break;
		}
		ms3 |= (frame->height - 1) << MS3_HEIGHT_SHIFT;
		ms3 |= (frame->width - 1) << MS3_WIDTH_SHIFT;
		OUT_BATCH(ms3);
		OUT_BATCH(((frame->pitch[0] / 4) - 1) << MS4_PITCH_SHIFT);

		shader_offset = sna->kgem.nbatch++;

		gen3_fs_dcl(FS_S0);
		gen3_fs_dcl(FS_T0);
		gen3_fs_texld(FS_OC, FS_S0, FS_T0);
		if (video->brightness != 0) {
			gen3_fs_add(FS_OC,
				    gen3_fs_operand_reg(FS_OC),
				    gen3_fs_operand(FS_C0, X, X, X, ZERO));
		}
	} else {
		/* For the planar formats, we set up three samplers --
		 * one for each plane, in a Y8 format.  Because I
		 * couldn't get the special PLANAR_TO_PACKED
		 * shader setup to work, I did the manual pixel shader:
		 *
		 * y' = y - .0625
		 * u' = u - .5
		 * v' = v - .5;
		 *
		 * r = 1.1643 * y' + 0.0     * u' + 1.5958  * v'
		 * g = 1.1643 * y' - 0.39173 * u' - 0.81290 * v'
		 * b = 1.1643 * y' + 2.017   * u' + 0.0     * v'
		 *
		 * register assignment:
		 * r0 = (y',u',v',0)
		 * r1 = (y,y,y,y)
		 * r2 = (u,u,u,u)
		 * r3 = (v,v,v,v)
		 * OC = (r,g,b,1)
		 */
		OUT_BATCH(_3DSTATE_PIXEL_SHADER_CONSTANTS | (22 - 2));
		OUT_BATCH(0x000001f);	/* constants 0-4 */
		/* constant 0: normalization offsets */
		OUT_BATCH_F(-0.0625);
		OUT_BATCH_F(-0.5);
		OUT_BATCH_F(-0.5);
		OUT_BATCH_F(0.0);
		/* constant 1: r coefficients */
		OUT_BATCH_F(1.1643);
		OUT_BATCH_F(0.0);
		OUT_BATCH_F(1.5958);
		OUT_BATCH_F(0.0);
		/* constant 2: g coefficients */
		OUT_BATCH_F(1.1643);
		OUT_BATCH_F(-0.39173);
		OUT_BATCH_F(-0.81290);
		OUT_BATCH_F(0.0);
		/* constant 3: b coefficients */
		OUT_BATCH_F(1.1643);
		OUT_BATCH_F(2.017);
		OUT_BATCH_F(0.0);
		OUT_BATCH_F(0.0);
		/* constant 4: brightness/contrast */
		OUT_BATCH_F(video->brightness / 128.0);
		OUT_BATCH_F(video->contrast / 255.0);
		OUT_BATCH_F(0.0);
		OUT_BATCH_F(0.0);

		OUT_BATCH(_3DSTATE_SAMPLER_STATE | 9);
		OUT_BATCH(0x00000007);
		/* sampler 0 */
		OUT_BATCH((FILTER_LINEAR << SS2_MAG_FILTER_SHIFT) |
			  (FILTER_LINEAR << SS2_MIN_FILTER_SHIFT));
		OUT_BATCH((TEXCOORDMODE_CLAMP_EDGE <<
			   SS3_TCX_ADDR_MODE_SHIFT) |
			  (TEXCOORDMODE_CLAMP_EDGE <<
			   SS3_TCY_ADDR_MODE_SHIFT) |
			  (0 << SS3_TEXTUREMAP_INDEX_SHIFT) |
			  SS3_NORMALIZED_COORDS);
		OUT_BATCH(0x00000000);
		/* sampler 1 */
		OUT_BATCH((FILTER_LINEAR << SS2_MAG_FILTER_SHIFT) |
			  (FILTER_LINEAR << SS2_MIN_FILTER_SHIFT));
		OUT_BATCH((TEXCOORDMODE_CLAMP_EDGE <<
			   SS3_TCX_ADDR_MODE_SHIFT) |
			  (TEXCOORDMODE_CLAMP_EDGE <<
			   SS3_TCY_ADDR_MODE_SHIFT) |
			  (1 << SS3_TEXTUREMAP_INDEX_SHIFT) |
			  SS3_NORMALIZED_COORDS);
		OUT_BATCH(0x00000000);
		/* sampler 2 */
		OUT_BATCH((FILTER_LINEAR << SS2_MAG_FILTER_SHIFT) |
			  (FILTER_LINEAR << SS2_MIN_FILTER_SHIFT));
		OUT_BATCH((TEXCOORDMODE_CLAMP_EDGE <<
			   SS3_TCX_ADDR_MODE_SHIFT) |
			  (TEXCOORDMODE_CLAMP_EDGE <<
			   SS3_TCY_ADDR_MODE_SHIFT) |
			  (2 << SS3_TEXTUREMAP_INDEX_SHIFT) |
			  SS3_NORMALIZED_COORDS);
		OUT_BATCH(0x00000000);

		OUT_BATCH(_3DSTATE_MAP_STATE | 9);
		OUT_BATCH(0x00000007);

		OUT_BATCH(kgem_add_reloc(&sna->kgem, sna->kgem.nbatch,
					 frame->bo,
					 I915_GEM_DOMAIN_SAMPLER << 16,
					 0));

		ms3 = MAPSURF_8BIT | MT_8BIT_I8;
		ms3 |= (frame->height - 1) << MS3_HEIGHT_SHIFT;
		ms3 |= (frame->width - 1) << MS3_WIDTH_SHIFT;
		OUT_BATCH(ms3);
		/* check to see if Y has special pitch than normal
		 * double u/v pitch, e.g i915 XvMC hw requires at
		 * least 1K alignment, so Y pitch might
		 * be same as U/V's.*/
		if (frame->pitch[1])
			OUT_BATCH(((frame->pitch[1] / 4) - 1) << MS4_PITCH_SHIFT);
		else
			OUT_BATCH(((frame->pitch[0] * 2 / 4) - 1) << MS4_PITCH_SHIFT);

		OUT_BATCH(kgem_add_reloc(&sna->kgem, sna->kgem.nbatch,
					 frame->bo,
					 I915_GEM_DOMAIN_SAMPLER << 16,
					 frame->UBufOffset));

		ms3 = MAPSURF_8BIT | MT_8BIT_I8;
		ms3 |= (frame->height / 2 - 1) << MS3_HEIGHT_SHIFT;
		ms3 |= (frame->width / 2 - 1) << MS3_WIDTH_SHIFT;
		OUT_BATCH(ms3);
		OUT_BATCH(((frame->pitch[0] / 4) - 1) << MS4_PITCH_SHIFT);

		OUT_BATCH(kgem_add_reloc(&sna->kgem, sna->kgem.nbatch,
					 frame->bo,
					 I915_GEM_DOMAIN_SAMPLER << 16,
					 frame->VBufOffset));

		ms3 = MAPSURF_8BIT | MT_8BIT_I8;
		ms3 |= (frame->height / 2 - 1) << MS3_HEIGHT_SHIFT;
		ms3 |= (frame->width / 2 - 1) << MS3_WIDTH_SHIFT;
		OUT_BATCH(ms3);
		OUT_BATCH(((frame->pitch[0] / 4) - 1) << MS4_PITCH_SHIFT);

		shader_offset = sna->kgem.nbatch++;

		/* Declare samplers */
		gen3_fs_dcl(FS_S0);	/* Y */
		gen3_fs_dcl(FS_S1);	/* U */
		gen3_fs_dcl(FS_S2);	/* V */
		gen3_fs_dcl(FS_T0);	/* normalized coords */

		/* Load samplers to temporaries. */
		gen3_fs_texld(FS_R1, FS_S0, FS_T0);
		gen3_fs_texld(FS_R2, FS_S1, FS_T0);
		gen3_fs_texld(FS_R3, FS_S2, FS_T0);

		/* Move the sampled YUV data in R[123] to the first
		 * 3 channels of R0.
		 */
		gen3_fs_mov_masked(FS_R0, MASK_X,
				   gen3_fs_operand_reg(FS_R1));
		gen3_fs_mov_masked(FS_R0, MASK_Y,
				   gen3_fs_operand_reg(FS_R2));
		gen3_fs_mov_masked(FS_R0, MASK_Z,
				   gen3_fs_operand_reg(FS_R3));

		/* Normalize the YUV data */
		gen3_fs_add(FS_R0, gen3_fs_operand_reg(FS_R0),
			    gen3_fs_operand_reg(FS_C0));
		/* dot-product the YUV data in R0 by the vectors of
		 * coefficients for calculating R, G, and B, storing
		 * the results in the R, G, or B channels of the output
		 * color.  The OC results are implicitly clamped
		 * at the end of the program.
		 */
		gen3_fs_dp3(FS_OC, MASK_X,
			    gen3_fs_operand_reg(FS_R0),
			    gen3_fs_operand_reg(FS_C1));
		gen3_fs_dp3(FS_OC, MASK_Y,
			    gen3_fs_operand_reg(FS_R0),
			    gen3_fs_operand_reg(FS_C2));
		gen3_fs_dp3(FS_OC, MASK_Z,
			    gen3_fs_operand_reg(FS_R0),
			    gen3_fs_operand_reg(FS_C3));
		/* Set alpha of the output to 1.0, by wiring W to 1
		 * and not actually using the source.
		 */
		gen3_fs_mov_masked(FS_OC, MASK_W,
				   gen3_fs_operand_one());

		if (video->brightness != 0) {
			gen3_fs_add(FS_OC,
				    gen3_fs_operand_reg(FS_OC),
				    gen3_fs_operand(FS_C4, X, X, X, ZERO));
		}
	}

	sna->kgem.batch[shader_offset] =
		_3DSTATE_PIXEL_SHADER_PROGRAM |
		(sna->kgem.nbatch - shader_offset - 2);
}

static void
gen3_video_get_batch(struct sna *sna)
{
	if (!kgem_check_batch(&sna->kgem, 120)) {
		DBG(("%s: flushing batch: nbatch %d < %d\n",
		     __FUNCTION__,
		     batch_space(sna), 120));
		kgem_submit(&sna->kgem);
	}

	if (sna->kgem.nreloc + 4 > KGEM_RELOC_SIZE(&sna->kgem)) {
		DBG(("%s: flushing batch: reloc %d >= %d\n",
		     __FUNCTION__,
		     sna->kgem.nreloc + 4,
		     (int)KGEM_RELOC_SIZE(&sna->kgem)));
		kgem_submit(&sna->kgem);
	}

	if (sna->kgem.nexec + 2 > KGEM_EXEC_SIZE(&sna->kgem)) {
		DBG(("%s: flushing batch: exec %d >= %d\n",
		     __FUNCTION__,
		     sna->kgem.nexec + 2,
		     (int)KGEM_EXEC_SIZE(&sna->kgem)));
		kgem_submit(&sna->kgem);
	}

	if (sna->render_state.gen3.need_invariant)
		gen3_emit_invariant(sna);
}

static int
gen3_get_inline_rectangles(struct sna *sna, int want, int floats_per_vertex)
{
	int size = floats_per_vertex * 3;
	int rem = batch_space(sna) - 1;

	if (size * want > rem)
		want = rem / size;

	return want;
}

static Bool
gen3_render_video(struct sna *sna,
		  struct sna_video *video,
		  struct sna_video_frame *frame,
		  RegionPtr dstRegion,
		  short src_w, short src_h,
		  short drw_w, short drw_h,
		  PixmapPtr pixmap)
{
	BoxPtr pbox = REGION_RECTS(dstRegion);
	int nbox = REGION_NUM_RECTS(dstRegion);
	int dxo = dstRegion->extents.x1;
	int dyo = dstRegion->extents.y1;
	int width = dstRegion->extents.x2 - dxo;
	int height = dstRegion->extents.y2 - dyo;
	float src_scale_x, src_scale_y;
	int pix_xoff, pix_yoff;
	struct kgem_bo *dst_bo;
	int copy = 0;

	DBG(("%s: %dx%d -> %dx%d\n", __FUNCTION__, src_w, src_h, drw_w, drw_h));

	if (pixmap->drawable.width > 2048 ||
	    pixmap->drawable.height > 2048 ||
	    !gen3_check_pitch_3d(sna_pixmap_get_bo(pixmap))) {
		int bpp = pixmap->drawable.bitsPerPixel;

		dst_bo = kgem_create_2d(&sna->kgem,
					width, height, bpp,
					kgem_choose_tiling(&sna->kgem,
							   I915_TILING_X,
							   width, height, bpp),
					0);
		if (!dst_bo)
			return FALSE;

		pix_xoff = -dxo;
		pix_yoff = -dyo;
		copy = 1;
	} else {
		dst_bo = sna_pixmap_get_bo(pixmap);

		width = pixmap->drawable.width;
		height = pixmap->drawable.height;

		/* Set up the offset for translating from the given region
		 * (in screen coordinates) to the backing pixmap.
		 */
#ifdef COMPOSITE
		pix_xoff = -pixmap->screen_x + pixmap->drawable.x;
		pix_yoff = -pixmap->screen_y + pixmap->drawable.y;
#else
		pix_xoff = 0;
		pix_yoff = 0;
#endif
	}

	src_scale_x = ((float)src_w / frame->width) / drw_w;
	src_scale_y = ((float)src_h / frame->height) / drw_h;

	DBG(("%s: src offset=(%d, %d), scale=(%f, %f), dst offset=(%d, %d)\n",
	     __FUNCTION__,
	     dxo, dyo, src_scale_x, src_scale_y, pix_xoff, pix_yoff));

	gen3_video_get_batch(sna);
	gen3_emit_video_state(sna, video, frame, pixmap,
			      dst_bo, width, height);
	do {
		int nbox_this_time = gen3_get_inline_rectangles(sna, nbox, 4);
		if (nbox_this_time == 0) {
			gen3_video_get_batch(sna);
			gen3_emit_video_state(sna, video, frame, pixmap,
					      dst_bo, width, height);
			nbox_this_time = gen3_get_inline_rectangles(sna, nbox, 4);
		}
		nbox -= nbox_this_time;

		OUT_BATCH(PRIM3D_RECTLIST | (12 * nbox_this_time - 1));
		while (nbox_this_time--) {
			int box_x1 = pbox->x1;
			int box_y1 = pbox->y1;
			int box_x2 = pbox->x2;
			int box_y2 = pbox->y2;

			pbox++;

			DBG(("%s: box (%d, %d), (%d, %d)\n",
			     __FUNCTION__, box_x1, box_y1, box_x2, box_y2));

			/* bottom right */
			OUT_BATCH_F(box_x2 + pix_xoff);
			OUT_BATCH_F(box_y2 + pix_yoff);
			OUT_BATCH_F((box_x2 - dxo) * src_scale_x);
			OUT_BATCH_F((box_y2 - dyo) * src_scale_y);

			/* bottom left */
			OUT_BATCH_F(box_x1 + pix_xoff);
			OUT_BATCH_F(box_y2 + pix_yoff);
			OUT_BATCH_F((box_x1 - dxo) * src_scale_x);
			OUT_BATCH_F((box_y2 - dyo) * src_scale_y);

			/* top left */
			OUT_BATCH_F(box_x1 + pix_xoff);
			OUT_BATCH_F(box_y1 + pix_yoff);
			OUT_BATCH_F((box_x1 - dxo) * src_scale_x);
			OUT_BATCH_F((box_y1 - dyo) * src_scale_y);
		}
	} while (nbox);

	if (copy) {
#ifdef COMPOSITE
		pix_xoff = -pixmap->screen_x + pixmap->drawable.x;
		pix_yoff = -pixmap->screen_y + pixmap->drawable.y;
#else
		pix_xoff = 0;
		pix_yoff = 0;
#endif
		sna_blt_copy_boxes(sna, GXcopy,
				   dst_bo, -dxo, -dyo,
				   sna_pixmap_get_bo(pixmap), pix_xoff, pix_yoff,
				   pixmap->drawable.bitsPerPixel,
				   REGION_RECTS(dstRegion),
				   REGION_NUM_RECTS(dstRegion));

		kgem_bo_destroy(&sna->kgem, dst_bo);
	}

	return TRUE;
}

static void
gen3_render_copy_setup_source(struct sna_composite_channel *channel,
			      PixmapPtr pixmap,
			      struct kgem_bo *bo)
{
	channel->u.gen3.type = SHADER_TEXTURE;
	channel->filter = gen3_filter(PictFilterNearest);
	channel->repeat = gen3_texture_repeat(RepeatNone);
	channel->width  = pixmap->drawable.width;
	channel->height = pixmap->drawable.height;
	channel->scale[0] = 1.f/pixmap->drawable.width;
	channel->scale[1] = 1.f/pixmap->drawable.height;
	channel->offset[0] = 0;
	channel->offset[1] = 0;
	gen3_composite_channel_set_format(channel,
					  sna_format_for_depth(pixmap->drawable.depth));
	channel->bo = bo;
	channel->is_affine = 1;
}

static Bool
gen3_render_copy_boxes(struct sna *sna, uint8_t alu,
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

	if (sna_blt_compare_depth(&src->drawable, &dst->drawable) &&
	    sna_blt_copy_boxes(sna, alu,
			       src_bo, src_dx, src_dy,
			       dst_bo, dst_dx, dst_dy,
			       dst->drawable.bitsPerPixel,
			       box, n))
		return TRUE;

	if (!(alu == GXcopy || alu == GXclear) ||
	    src_bo == dst_bo || /* XXX handle overlap using 3D ? */
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
	tmp.op = alu == GXcopy ? PictOpSrc : PictOpClear;

	tmp.dst.pixmap = dst;
	tmp.dst.width = dst->drawable.width;
	tmp.dst.height = dst->drawable.height;
	tmp.dst.format = sna_format_for_depth(dst->drawable.depth);
	tmp.dst.bo = dst_bo;

	gen3_render_copy_setup_source(&tmp.src, src, src_bo);

	tmp.floats_per_vertex = 4;
	tmp.floats_per_rect = 12;
	tmp.mask.u.gen3.type = SHADER_NONE;

	gen3_emit_composite_state(sna, &tmp);
	gen3_align_vertex(sna, &tmp);

	do {
		int n_this_time;

		n_this_time = gen3_get_rectangles(sna, &tmp, n);
		if (n_this_time == 0) {
			gen3_emit_composite_state(sna, &tmp);
			n_this_time = gen3_get_rectangles(sna, &tmp, n);
		}
		n -= n_this_time;

		do {
			DBG(("	(%d, %d) -> (%d, %d) + (%d, %d)\n",
			     box->x1 + src_dx, box->y1 + src_dy,
			     box->x1 + dst_dx, box->y1 + dst_dy,
			     box->x2 - box->x1, box->y2 - box->y1));
			OUT_VERTEX(box->x2 + dst_dx);
			OUT_VERTEX(box->y2 + dst_dy);
			OUT_VERTEX((box->x2 + src_dx) * tmp.src.scale[0]);
			OUT_VERTEX((box->y2 + src_dy) * tmp.src.scale[1]);

			OUT_VERTEX(box->x1 + dst_dx);
			OUT_VERTEX(box->y2 + dst_dy);
			OUT_VERTEX((box->x1 + src_dx) * tmp.src.scale[0]);
			OUT_VERTEX((box->y2 + src_dy) * tmp.src.scale[1]);

			OUT_VERTEX(box->x1 + dst_dx);
			OUT_VERTEX(box->y1 + dst_dy);
			OUT_VERTEX((box->x1 + src_dx) * tmp.src.scale[0]);
			OUT_VERTEX((box->y1 + src_dy) * tmp.src.scale[1]);

			box++;
		} while (--n_this_time);
	} while (n);

	gen3_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
	return TRUE;
}

static void
gen3_render_copy_blt(struct sna *sna,
		     const struct sna_copy_op *op,
		     int16_t sx, int16_t sy,
		     int16_t w, int16_t h,
		     int16_t dx, int16_t dy)
{
	if (!gen3_get_rectangles(sna, &op->base, 1)) {
		gen3_emit_composite_state(sna, &op->base);
		gen3_get_rectangles(sna, &op->base, 1);
	}

	OUT_VERTEX(dx+w);
	OUT_VERTEX(dy+h);
	OUT_VERTEX((sx+w)*op->base.src.scale[0]);
	OUT_VERTEX((sy+h)*op->base.src.scale[1]);

	OUT_VERTEX(dx);
	OUT_VERTEX(dy+h);
	OUT_VERTEX(sx*op->base.src.scale[0]);
	OUT_VERTEX((sy+h)*op->base.src.scale[1]);

	OUT_VERTEX(dx);
	OUT_VERTEX(dy);
	OUT_VERTEX(sx*op->base.src.scale[0]);
	OUT_VERTEX(sy*op->base.src.scale[1]);
}

static void
gen3_render_copy_done(struct sna *sna, const struct sna_copy_op *op)
{
	gen3_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
}

static Bool
gen3_render_copy(struct sna *sna, uint8_t alu,
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
	if (sna->kgem.mode != KGEM_RENDER &&
	    sna_blt_compare_depth(&src->drawable, &dst->drawable) &&
	    sna_blt_copy(sna, alu,
			 src_bo, dst_bo,
			 dst->drawable.bitsPerPixel,
			 tmp))
		return TRUE;

	/* Must use the BLT if we can't RENDER... */
	if (!(alu == GXcopy || alu == GXclear) ||
	    src->drawable.width > 2048 || src->drawable.height > 2048 ||
	    dst->drawable.width > 2048 || dst->drawable.height > 2048 ||
	    src_bo->pitch > 8192 || dst_bo->pitch > 8192) {
		if (!sna_blt_compare_depth(&src->drawable, &dst->drawable))
			return FALSE;

		return sna_blt_copy(sna, alu, src_bo, dst_bo,
				    dst->drawable.bitsPerPixel,
				    tmp);
	}

	tmp->base.op = alu == GXcopy ? PictOpSrc : PictOpClear;

	tmp->base.dst.pixmap = dst;
	tmp->base.dst.width = dst->drawable.width;
	tmp->base.dst.height = dst->drawable.height;
	tmp->base.dst.format = sna_format_for_depth(dst->drawable.depth);
	tmp->base.dst.bo = dst_bo;

	gen3_render_copy_setup_source(&tmp->base.src, src, src_bo);

	tmp->base.floats_per_vertex = 4;
	tmp->base.floats_per_rect = 12;
	tmp->base.mask.u.gen3.type = SHADER_NONE;

	if (!kgem_check_bo(&sna->kgem, dst_bo, src_bo, NULL))
		kgem_submit(&sna->kgem);

	if (kgem_bo_is_dirty(src_bo))
		kgem_emit_flush(&sna->kgem);

	tmp->blt  = gen3_render_copy_blt;
	tmp->done = gen3_render_copy_done;

	gen3_emit_composite_state(sna, &tmp->base);
	gen3_align_vertex(sna, &tmp->base);
	return TRUE;
}

static Bool
gen3_render_fill_boxes_try_blt(struct sna *sna,
			       CARD8 op, PictFormat format,
			       const xRenderColor *color,
			       PixmapPtr dst, struct kgem_bo *dst_bo,
			       const BoxRec *box, int n)
{
	uint8_t alu = GXcopy;
	uint32_t pixel;

	if (dst_bo->tiling == I915_TILING_Y)
		return FALSE;

	if (color->alpha >= 0xff00) {
		if (op == PictOpOver)
			op = PictOpSrc;
		else if (op == PictOpOutReverse)
			op = PictOpClear;
		else if (op == PictOpAdd &&
			 (color->red & color->green & color->blue) >= 0xff00)
			op = PictOpSrc;
	}

	pixel = 0;
	if (op == PictOpClear) {
		alu = GXclear;
	} else if (op == PictOpSrc) {
		if (color->alpha <= 0x00ff)
			alu = GXclear;
		else if (!sna_get_pixel_from_rgba(&pixel,
						    color->red,
						    color->green,
						    color->blue,
						    color->alpha,
						    format))
			return FALSE;
	} else
		return FALSE;


	return sna_blt_fill_boxes(sna, alu,
				  dst_bo, dst->drawable.bitsPerPixel,
				  pixel, box, n);
}

static inline Bool prefer_fill_blt(struct sna *sna)
{
#if PREFER_BLT_FILL
	return true;
#else
	return sna->kgem.mode != KGEM_RENDER;
#endif
}

static inline void set_fill_shader(struct sna_composite_channel *c,
				   uint32_t pixel)
{
	if (pixel == 0)
		c->u.gen3.type = SHADER_ZERO;
	else if (pixel == 0xff000000)
		c->u.gen3.type = SHADER_BLACK;
	else if (pixel == 0xffffffff)
		c->u.gen3.type = SHADER_WHITE;
	else
		c->u.gen3.type = SHADER_CONSTANT;
	c->u.gen3.mode = pixel;
	c->is_affine = 1;
	c->alpha_fixup = 0;
	c->rb_reversed = 0;
}

static Bool
gen3_render_fill_boxes(struct sna *sna,
		       CARD8 op,
		       PictFormat format,
		       const xRenderColor *color,
		       PixmapPtr dst, struct kgem_bo *dst_bo,
		       const BoxRec *box, int n)
{
	struct sna_composite_op tmp;
	uint32_t pixel;

#if NO_FILL_BOXES
	return gen3_render_fill_boxes_try_blt(sna, op, format, color,
					      dst, dst_bo,
					      box, n);
#endif

	DBG(("%s (op=%d, format=%x, color=(%04x,%04x,%04x, %04x))\n",
	     __FUNCTION__, op, (int)format,
	     color->red, color->green, color->blue, color->alpha));

	if (op >= ARRAY_SIZE(gen3_blend_op)) {
		DBG(("%s: fallback due to unhandled blend op: %d\n",
		     __FUNCTION__, op));
		return FALSE;
	}

	if (dst->drawable.width > 2048 ||
	    dst->drawable.height > 2048 ||
	    dst_bo->pitch > 8192 ||
	    !gen3_check_dst_format(format))
		return gen3_render_fill_boxes_try_blt(sna, op, format, color,
						      dst, dst_bo,
						      box, n);

	if (prefer_fill_blt(sna) &&
	    gen3_render_fill_boxes_try_blt(sna, op, format, color,
					   dst, dst_bo,
					   box, n))
		return TRUE;

	if (op == PictOpClear) {
		pixel = 0;
	} else {
		if (!sna_get_pixel_from_rgba(&pixel,
					     color->red,
					     color->green,
					     color->blue,
					     color->alpha,
					     PICT_a8r8g8b8))
			return FALSE;
	}
	DBG(("%s: using shader for op=%d, format=%x, pixel=%x\n",
	     __FUNCTION__, op, (int)format, pixel));

	tmp.op = op;
	tmp.dst.pixmap = dst;
	tmp.dst.width = dst->drawable.width;
	tmp.dst.height = dst->drawable.height;
	tmp.dst.format = format;
	tmp.dst.bo = dst_bo;
	tmp.floats_per_vertex = 2;
	tmp.floats_per_rect = 6;
	tmp.rb_reversed = 0;

	set_fill_shader(&tmp.src, pixel);
	tmp.mask.u.gen3.type = SHADER_NONE;
	tmp.u.gen3.num_constants = 0;

	if (!kgem_check_bo(&sna->kgem, dst_bo, NULL))
		kgem_submit(&sna->kgem);

	gen3_emit_composite_state(sna, &tmp);
	gen3_align_vertex(sna, &tmp);

	do {
		int n_this_time = gen3_get_rectangles(sna, &tmp, n);
		if (n_this_time == 0) {
			gen3_emit_composite_state(sna, &tmp);
			n_this_time = gen3_get_rectangles(sna, &tmp, n);
		}
		n -= n_this_time;

		do {
			DBG(("	(%d, %d), (%d, %d): %x\n",
			     box->x1, box->y1, box->x2, box->y2, pixel));
			OUT_VERTEX(box->x2);
			OUT_VERTEX(box->y2);
			OUT_VERTEX(box->x1);
			OUT_VERTEX(box->y2);
			OUT_VERTEX(box->x1);
			OUT_VERTEX(box->y1);
			box++;
		} while (--n_this_time);
	} while (n);

	gen3_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
	return TRUE;
}

static void
gen3_render_fill_op_blt(struct sna *sna,
			const struct sna_fill_op *op,
			int16_t x, int16_t y, int16_t w, int16_t h)
{
	if (!gen3_get_rectangles(sna, &op->base, 1)) {
		gen3_emit_composite_state(sna, &op->base);
		gen3_get_rectangles(sna, &op->base, 1);
	}

	OUT_VERTEX(x+w);
	OUT_VERTEX(y+h);
	OUT_VERTEX(x);
	OUT_VERTEX(y+h);
	OUT_VERTEX(x);
	OUT_VERTEX(y);
}

fastcall static void
gen3_render_fill_op_box(struct sna *sna,
			const struct sna_fill_op *op,
			const BoxRec *box)
{
	if (!gen3_get_rectangles(sna, &op->base, 1)) {
		gen3_emit_composite_state(sna, &op->base);
		gen3_get_rectangles(sna, &op->base, 1);
	}

	OUT_VERTEX(box->x2);
	OUT_VERTEX(box->y2);
	OUT_VERTEX(box->x1);
	OUT_VERTEX(box->y2);
	OUT_VERTEX(box->x1);
	OUT_VERTEX(box->y1);
}

fastcall static void
gen3_render_fill_op_boxes(struct sna *sna,
			  const struct sna_fill_op *op,
			  const BoxRec *box,
			  int nbox)
{
	DBG(("%s: (%d, %d),(%d, %d)... x %d\n", __FUNCTION__,
	     box->x1, box->y1, box->x2, box->y2, nbox));

	do {
		int nbox_this_time = gen3_get_rectangles(sna, &op->base, nbox);
		if (nbox_this_time == 0) {
			gen3_emit_composite_state(sna, &op->base);
			nbox_this_time = gen3_get_rectangles(sna, &op->base, nbox);
		}
		nbox -= nbox_this_time;

		do {
			OUT_VERTEX(box->x2);
			OUT_VERTEX(box->y2);
			OUT_VERTEX(box->x1);
			OUT_VERTEX(box->y2);
			OUT_VERTEX(box->x1);
			OUT_VERTEX(box->y1);
			box++;
		} while (--nbox_this_time);
	} while (nbox);
}

static void
gen3_render_fill_op_done(struct sna *sna, const struct sna_fill_op *op)
{
	gen3_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
}

static Bool
gen3_render_fill(struct sna *sna, uint8_t alu,
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
	if (prefer_fill_blt(sna) &&
	    sna_blt_fill(sna, alu,
			 dst_bo, dst->drawable.bitsPerPixel,
			 color,
			 tmp))
		return TRUE;

	/* Must use the BLT if we can't RENDER... */
	if (!(alu == GXcopy || alu == GXclear) ||
	    dst->drawable.width > 2048 || dst->drawable.height > 2048 ||
	    dst_bo->pitch > 8192)
		return sna_blt_fill(sna, alu,
				    dst_bo, dst->drawable.bitsPerPixel,
				    color,
				    tmp);

	if (alu == GXclear)
		color = 0;

	tmp->base.op = color == 0 ? PictOpClear : PictOpSrc;
	tmp->base.dst.pixmap = dst;
	tmp->base.dst.width = dst->drawable.width;
	tmp->base.dst.height = dst->drawable.height;
	tmp->base.dst.format = sna_format_for_depth(dst->drawable.depth);
	tmp->base.dst.bo = dst_bo;
	tmp->base.floats_per_vertex = 2;
	tmp->base.floats_per_rect = 6;
	tmp->base.need_magic_ca_pass = 0;
	tmp->base.has_component_alpha = 0;
	tmp->base.rb_reversed = 0;

	set_fill_shader(&tmp->base.src,
			sna_rgba_for_color(color, dst->drawable.depth));
	tmp->base.mask.u.gen3.type = SHADER_NONE;
	tmp->base.u.gen3.num_constants = 0;

	if (!kgem_check_bo(&sna->kgem, dst_bo, NULL))
		kgem_submit(&sna->kgem);

	tmp->blt   = gen3_render_fill_op_blt;
	tmp->box   = gen3_render_fill_op_box;
	tmp->boxes = gen3_render_fill_op_boxes;
	tmp->done  = gen3_render_fill_op_done;

	gen3_emit_composite_state(sna, &tmp->base);
	gen3_align_vertex(sna, &tmp->base);
	return TRUE;
}

static Bool
gen3_render_fill_one_try_blt(struct sna *sna, PixmapPtr dst, struct kgem_bo *bo,
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
gen3_render_fill_one(struct sna *sna, PixmapPtr dst, struct kgem_bo *bo,
		     uint32_t color,
		     int16_t x1, int16_t y1,
		     int16_t x2, int16_t y2,
		     uint8_t alu)
{
	struct sna_composite_op tmp;

#if NO_FILL_ONE
	return gen3_render_fill_one_try_blt(sna, dst, bo, color,
					    x1, y1, x2, y2, alu);
#endif

	/* Prefer to use the BLT if already engaged */
	if (prefer_fill_blt(sna) &&
	    gen3_render_fill_one_try_blt(sna, dst, bo, color,
					 x1, y1, x2, y2, alu))
		return TRUE;

	/* Must use the BLT if we can't RENDER... */
	if (!(alu == GXcopy || alu == GXclear) ||
	    dst->drawable.width > 2048 || dst->drawable.height > 2048 ||
	    bo->pitch > 8192)
		return gen3_render_fill_one_try_blt(sna, dst, bo, color,
						    x1, y1, x2, y2, alu);

	if (alu == GXclear)
		color = 0;

	tmp.op = color == 0 ? PictOpClear : PictOpSrc;
	tmp.dst.pixmap = dst;
	tmp.dst.width = dst->drawable.width;
	tmp.dst.height = dst->drawable.height;
	tmp.dst.format = sna_format_for_depth(dst->drawable.depth);
	tmp.dst.bo = bo;
	tmp.floats_per_vertex = 2;
	tmp.floats_per_rect = 6;
	tmp.need_magic_ca_pass = 0;
	tmp.has_component_alpha = 0;
	tmp.rb_reversed = 0;

	set_fill_shader(&tmp.src,
			sna_rgba_for_color(color, dst->drawable.depth));
	tmp.mask.u.gen3.type = SHADER_NONE;
	tmp.u.gen3.num_constants = 0;

	if (!kgem_check_bo(&sna->kgem, bo, NULL)) {
		kgem_submit(&sna->kgem);
		if (gen3_render_fill_one_try_blt(sna, dst, bo, color,
						 x1, y1, x2, y2, alu))
			return TRUE;
	}

	gen3_emit_composite_state(sna, &tmp);
	gen3_align_vertex(sna, &tmp);
	gen3_get_rectangles(sna, &tmp, 1);
	DBG(("	(%d, %d), (%d, %d): %x\n", x1, y1, x2, y2, color));
	OUT_VERTEX(x2);
	OUT_VERTEX(y2);
	OUT_VERTEX(x1);
	OUT_VERTEX(y2);
	OUT_VERTEX(x1);
	OUT_VERTEX(y1);
	gen3_vertex_flush(sna);

	return TRUE;
}

static void gen3_render_flush(struct sna *sna)
{
	gen3_vertex_finish(sna, TRUE);
}

static void
gen3_render_fini(struct sna *sna)
{
}

Bool gen3_render_init(struct sna *sna)
{
	struct sna_render *render = &sna->render;

	render->composite = gen3_render_composite;
	render->composite_spans = gen3_render_composite_spans;

	render->video = gen3_render_video;

	render->copy_boxes = gen3_render_copy_boxes;
	render->copy = gen3_render_copy;

	render->fill_boxes = gen3_render_fill_boxes;
	render->fill = gen3_render_fill;
	render->fill_one = gen3_render_fill_one;

	render->reset = gen3_render_reset;
	render->flush = gen3_render_flush;
	render->fini = gen3_render_fini;

	render->max_3d_size = 2048;
	return TRUE;
}
