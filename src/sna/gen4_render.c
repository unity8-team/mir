/*
 * Copyright © 2006,2008,2011 Intel Corporation
 * Copyright © 2007 Red Hat, Inc.
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
 *    Wang Zhenyu <zhenyu.z.wang@sna.com>
 *    Eric Anholt <eric@anholt.net>
 *    Carl Worth <cworth@redhat.com>
 *    Keith Packard <keithp@keithp.com>
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xf86.h>

#include "sna.h"
#include "sna_reg.h"
#include "sna_render.h"
#include "sna_render_inline.h"
#include "sna_video.h"

#include "gen4_render.h"

#if DEBUG_RENDER
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

/* gen4 has a serious issue with its shaders that we need to flush
 * after every rectangle... So until that is resolved, prefer
 * the BLT engine.
 */
#define PREFER_BLT 1
#define FLUSH_EVERY_VERTEX 1

#define NO_COMPOSITE 0
#define NO_COPY 0
#define NO_COPY_BOXES 0
#define NO_FILL 0
#define NO_FILL_BOXES 0

#if FLUSH_EVERY_VERTEX
#define FLUSH() do { \
	gen4_vertex_flush(sna); \
	OUT_BATCH(MI_FLUSH | MI_INHIBIT_RENDER_CACHE_FLUSH); \
} while (0)
#else
#define FLUSH()
#endif

#define GEN4_GRF_BLOCKS(nreg)    ((nreg + 15) / 16 - 1)

/* Set up a default static partitioning of the URB, which is supposed to
 * allow anything we would want to do, at potentially lower performance.
 */
#define URB_CS_ENTRY_SIZE     1
#define URB_CS_ENTRIES	      0

#define URB_VS_ENTRY_SIZE     1	// each 512-bit row
#define URB_VS_ENTRIES	      8	// we needs at least 8 entries

#define URB_GS_ENTRY_SIZE     0
#define URB_GS_ENTRIES	      0

#define URB_CLIP_ENTRY_SIZE   0
#define URB_CLIP_ENTRIES      0

#define URB_SF_ENTRY_SIZE     2
#define URB_SF_ENTRIES	      1

/*
 * this program computes dA/dx and dA/dy for the texture coordinates along
 * with the base texture coordinate. It was extracted from the Mesa driver
 */

#define SF_KERNEL_NUM_GRF  16
#define SF_MAX_THREADS	   2

#define PS_KERNEL_NUM_GRF   32
#define PS_MAX_THREADS	    48

static const uint32_t sf_kernel[][4] = {
#include "exa_sf.g4b"
};

static const uint32_t sf_kernel_mask[][4] = {
#include "exa_sf_mask.g4b"
};

static const uint32_t ps_kernel_nomask_affine[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_affine.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_nomask_projective[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_projective.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_maskca_affine[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_affine.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_mask_affine.g4b"
#include "exa_wm_mask_sample_argb.g4b"
#include "exa_wm_ca.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_maskca_projective[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_projective.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_mask_projective.g4b"
#include "exa_wm_mask_sample_argb.g4b"
#include "exa_wm_ca.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_maskca_srcalpha_affine[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_affine.g4b"
#include "exa_wm_src_sample_a.g4b"
#include "exa_wm_mask_affine.g4b"
#include "exa_wm_mask_sample_argb.g4b"
#include "exa_wm_ca_srcalpha.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_maskca_srcalpha_projective[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_projective.g4b"
#include "exa_wm_src_sample_a.g4b"
#include "exa_wm_mask_projective.g4b"
#include "exa_wm_mask_sample_argb.g4b"
#include "exa_wm_ca_srcalpha.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_masknoca_affine[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_affine.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_mask_affine.g4b"
#include "exa_wm_mask_sample_a.g4b"
#include "exa_wm_noca.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_masknoca_projective[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_projective.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_mask_projective.g4b"
#include "exa_wm_mask_sample_a.g4b"
#include "exa_wm_noca.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_packed_static[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_affine.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_yuv_rgb.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_planar_static[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_affine.g4b"
#include "exa_wm_src_sample_planar.g4b"
#include "exa_wm_yuv_rgb.g4b"
#include "exa_wm_write.g4b"
};

#define KERNEL(kernel_enum, kernel, masked) \
    [kernel_enum] = {&kernel, sizeof(kernel), masked}
static const struct wm_kernel_info {
	const void *data;
	unsigned int size;
	Bool has_mask;
} wm_kernels[] = {
	KERNEL(WM_KERNEL, ps_kernel_nomask_affine, FALSE),
	KERNEL(WM_KERNEL_PROJECTIVE, ps_kernel_nomask_projective, FALSE),

	KERNEL(WM_KERNEL_MASK, ps_kernel_masknoca_affine, TRUE),
	KERNEL(WM_KERNEL_MASK_PROJECTIVE, ps_kernel_masknoca_projective, TRUE),

	KERNEL(WM_KERNEL_MASKCA, ps_kernel_maskca_affine, TRUE),
	KERNEL(WM_KERNEL_MASKCA_PROJECTIVE, ps_kernel_maskca_projective, TRUE),

	KERNEL(WM_KERNEL_MASKCA_SRCALPHA,
	       ps_kernel_maskca_srcalpha_affine, TRUE),
	KERNEL(WM_KERNEL_MASKCA_SRCALPHA_PROJECTIVE,
	       ps_kernel_maskca_srcalpha_projective, TRUE),

	KERNEL(WM_KERNEL_VIDEO_PLANAR, ps_kernel_planar_static, FALSE),
	KERNEL(WM_KERNEL_VIDEO_PACKED, ps_kernel_packed_static, FALSE),
};
#undef KERNEL

static const struct blendinfo {
	Bool src_alpha;
	uint32_t src_blend;
	uint32_t dst_blend;
} gen4_blend_op[] = {
	/* Clear */	{0, GEN4_BLENDFACTOR_ZERO, GEN4_BLENDFACTOR_ZERO},
	/* Src */	{0, GEN4_BLENDFACTOR_ONE, GEN4_BLENDFACTOR_ZERO},
	/* Dst */	{0, GEN4_BLENDFACTOR_ZERO, GEN4_BLENDFACTOR_ONE},
	/* Over */	{1, GEN4_BLENDFACTOR_ONE, GEN4_BLENDFACTOR_INV_SRC_ALPHA},
	/* OverReverse */ {0, GEN4_BLENDFACTOR_INV_DST_ALPHA, GEN4_BLENDFACTOR_ONE},
	/* In */	{0, GEN4_BLENDFACTOR_DST_ALPHA, GEN4_BLENDFACTOR_ZERO},
	/* InReverse */	{1, GEN4_BLENDFACTOR_ZERO, GEN4_BLENDFACTOR_SRC_ALPHA},
	/* Out */	{0, GEN4_BLENDFACTOR_INV_DST_ALPHA, GEN4_BLENDFACTOR_ZERO},
	/* OutReverse */ {1, GEN4_BLENDFACTOR_ZERO, GEN4_BLENDFACTOR_INV_SRC_ALPHA},
	/* Atop */	{1, GEN4_BLENDFACTOR_DST_ALPHA, GEN4_BLENDFACTOR_INV_SRC_ALPHA},
	/* AtopReverse */ {1, GEN4_BLENDFACTOR_INV_DST_ALPHA, GEN4_BLENDFACTOR_SRC_ALPHA},
	/* Xor */	{1, GEN4_BLENDFACTOR_INV_DST_ALPHA, GEN4_BLENDFACTOR_INV_SRC_ALPHA},
	/* Add */	{0, GEN4_BLENDFACTOR_ONE, GEN4_BLENDFACTOR_ONE},
};

/**
 * Highest-valued BLENDFACTOR used in gen4_blend_op.
 *
 * This leaves out GEN4_BLENDFACTOR_INV_DST_COLOR,
 * GEN4_BLENDFACTOR_INV_CONST_{COLOR,ALPHA},
 * GEN4_BLENDFACTOR_INV_SRC1_{COLOR,ALPHA}
 */
#define GEN4_BLENDFACTOR_COUNT (GEN4_BLENDFACTOR_INV_DST_ALPHA + 1)

static const struct formatinfo {
	CARD32 pict_fmt;
	uint32_t card_fmt;
} gen4_tex_formats[] = {
	{PICT_a8, GEN4_SURFACEFORMAT_A8_UNORM},
	{PICT_a8r8g8b8, GEN4_SURFACEFORMAT_B8G8R8A8_UNORM},
	{PICT_x8r8g8b8, GEN4_SURFACEFORMAT_B8G8R8X8_UNORM},
	{PICT_a8b8g8r8, GEN4_SURFACEFORMAT_R8G8B8A8_UNORM},
	{PICT_x8b8g8r8, GEN4_SURFACEFORMAT_R8G8B8X8_UNORM},
	{PICT_r8g8b8, GEN4_SURFACEFORMAT_R8G8B8_UNORM},
	{PICT_r5g6b5, GEN4_SURFACEFORMAT_B5G6R5_UNORM},
	{PICT_a1r5g5b5, GEN4_SURFACEFORMAT_B5G5R5A1_UNORM},
	{PICT_a2r10g10b10, GEN4_SURFACEFORMAT_B10G10R10A2_UNORM},
	{PICT_x2r10g10b10, GEN4_SURFACEFORMAT_B10G10R10X2_UNORM},
	{PICT_a2b10g10r10, GEN4_SURFACEFORMAT_R10G10B10A2_UNORM},
	{PICT_x2r10g10b10, GEN4_SURFACEFORMAT_B10G10R10X2_UNORM},
	{PICT_a4r4g4b4, GEN4_SURFACEFORMAT_B4G4R4A4_UNORM},
};

#define BLEND_OFFSET(s, d) \
	(((s) * GEN4_BLENDFACTOR_COUNT + (d)) * 64)

#define SAMPLER_OFFSET(sf, se, mf, me, k) \
	((((((sf) * EXTEND_COUNT + (se)) * FILTER_COUNT + (mf)) * EXTEND_COUNT + (me)) * KERNEL_COUNT + (k)) * 64)

static void
gen4_emit_pipelined_pointers(struct sna *sna,
			     const struct sna_composite_op *op,
			     int blend, int kernel);

#define OUT_BATCH(v) batch_emit(sna, v)
#define OUT_VERTEX(x,y) vertex_emit_2s(sna, x,y)
#define OUT_VERTEX_F(v) vertex_emit(sna, v)

static int
gen4_choose_composite_kernel(int op, Bool has_mask, Bool is_ca, Bool is_affine)
{
	int base;

	if (has_mask) {
		if (is_ca) {
			if (gen4_blend_op[op].src_alpha)
				base = WM_KERNEL_MASKCA_SRCALPHA;
			else
				base = WM_KERNEL_MASKCA;
		} else
			base = WM_KERNEL_MASK;
	} else
		base = WM_KERNEL;

	return base + !is_affine;
}

static void gen4_magic_ca_pass(struct sna *sna,
			       const struct sna_composite_op *op)
{
	struct gen4_render_state *state = &sna->render_state.gen4;

	if (!op->need_magic_ca_pass)
		return;

	DBG(("%s: CA fixup\n", __FUNCTION__));
	assert(op->mask.bo != NULL);
	assert(op->has_component_alpha);

	if (FLUSH_EVERY_VERTEX)
		OUT_BATCH(MI_FLUSH | MI_INHIBIT_RENDER_CACHE_FLUSH);

	gen4_emit_pipelined_pointers(sna, op, PictOpAdd,
				     gen4_choose_composite_kernel(PictOpAdd,
								  TRUE, TRUE, op->is_affine));

	OUT_BATCH(GEN4_3DPRIMITIVE |
		  GEN4_3DPRIMITIVE_VERTEX_SEQUENTIAL |
		  (_3DPRIM_RECTLIST << GEN4_3DPRIMITIVE_TOPOLOGY_SHIFT) |
		  (0 << 9) |
		  4);
	OUT_BATCH(sna->render.vertex_index - sna->render.vertex_start);
	OUT_BATCH(sna->render.vertex_start);
	OUT_BATCH(1);	/* single instance */
	OUT_BATCH(0);	/* start instance location */
	OUT_BATCH(0);	/* index buffer offset, ignored */

	state->last_primitive = sna->kgem.nbatch;
}

static void gen4_vertex_flush(struct sna *sna)
{
	if (sna->render_state.gen4.vertex_offset == 0)
		return;

	DBG(("%s[%x] = %d\n", __FUNCTION__,
	     4*sna->render_state.gen4.vertex_offset,
	     sna->render.vertex_index - sna->render.vertex_start));
	sna->kgem.batch[sna->render_state.gen4.vertex_offset] =
		sna->render.vertex_index - sna->render.vertex_start;
	sna->render_state.gen4.vertex_offset = 0;

	if (sna->render.op)
		gen4_magic_ca_pass(sna, sna->render.op);
}

static void gen4_vertex_finish(struct sna *sna, Bool last)
{
	struct kgem_bo *bo;
	int i, delta;

	gen4_vertex_flush(sna);
	if (!sna->render.vertex_used)
		return;

	/* Note: we only need dword alignment (currently) */

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

	for (i = 0; i < ARRAY_SIZE(sna->render.vertex_reloc); i++) {
		if (sna->render.vertex_reloc[i]) {
			DBG(("%s: reloc[%d] = %d\n", __FUNCTION__,
			     i, sna->render.vertex_reloc[i]));

			sna->kgem.batch[sna->render.vertex_reloc[i]] =
				kgem_add_reloc(&sna->kgem,
					       sna->render.vertex_reloc[i],
					       bo,
					       I915_GEM_DOMAIN_VERTEX << 16,
					       delta);
			sna->render.vertex_reloc[i] = 0;
		}
	}

	if (bo)
		kgem_bo_destroy(&sna->kgem, bo);

	sna->render.vertex_used = 0;
	sna->render.vertex_index = 0;
	sna->render_state.gen4.vb_id = 0;
}

static uint32_t gen4_get_blend(int op,
			       Bool has_component_alpha,
			       uint32_t dst_format)
{
	uint32_t src, dst;

	src = gen4_blend_op[op].src_blend;
	dst = gen4_blend_op[op].dst_blend;

	/* If there's no dst alpha channel, adjust the blend op so that we'll treat
	 * it as always 1.
	 */
	if (PICT_FORMAT_A(dst_format) == 0) {
		if (src == GEN4_BLENDFACTOR_DST_ALPHA)
			src = GEN4_BLENDFACTOR_ONE;
		else if (src == GEN4_BLENDFACTOR_INV_DST_ALPHA)
			src = GEN4_BLENDFACTOR_ZERO;
	}

	/* If the source alpha is being used, then we should only be in a
	 * case where the source blend factor is 0, and the source blend
	 * value is the mask channels multiplied by the source picture's alpha.
	 */
	if (has_component_alpha && gen4_blend_op[op].src_alpha) {
		if (dst == GEN4_BLENDFACTOR_SRC_ALPHA)
			dst = GEN4_BLENDFACTOR_SRC_COLOR;
		else if (dst == GEN4_BLENDFACTOR_INV_SRC_ALPHA)
			dst = GEN4_BLENDFACTOR_INV_SRC_COLOR;
	}

	DBG(("blend op=%d, dst=%x [A=%d] => src=%d, dst=%d => offset=%x\n",
	     op, dst_format, PICT_FORMAT_A(dst_format),
	     src, dst, BLEND_OFFSET(src, dst)));
	return BLEND_OFFSET(src, dst);
}

static uint32_t gen4_get_dest_format(PictFormat format)
{
	switch (format) {
	default:
		assert(0);
	case PICT_a8r8g8b8:
	case PICT_x8r8g8b8:
		return GEN4_SURFACEFORMAT_B8G8R8A8_UNORM;
	case PICT_a8b8g8r8:
	case PICT_x8b8g8r8:
		return GEN4_SURFACEFORMAT_R8G8B8A8_UNORM;
	case PICT_a2r10g10b10:
	case PICT_x2r10g10b10:
		return GEN4_SURFACEFORMAT_B10G10R10A2_UNORM;
	case PICT_r5g6b5:
		return GEN4_SURFACEFORMAT_B5G6R5_UNORM;
	case PICT_x1r5g5b5:
	case PICT_a1r5g5b5:
		return GEN4_SURFACEFORMAT_B5G5R5A1_UNORM;
	case PICT_a8:
		return GEN4_SURFACEFORMAT_A8_UNORM;
	case PICT_a4r4g4b4:
	case PICT_x4r4g4b4:
		return GEN4_SURFACEFORMAT_B4G4R4A4_UNORM;
	}
}

static Bool gen4_check_dst_format(PictFormat format)
{
	switch (format) {
	case PICT_a8r8g8b8:
	case PICT_x8r8g8b8:
	case PICT_a8b8g8r8:
	case PICT_x8b8g8r8:
	case PICT_a2r10g10b10:
	case PICT_x2r10g10b10:
	case PICT_r5g6b5:
	case PICT_x1r5g5b5:
	case PICT_a1r5g5b5:
	case PICT_a8:
	case PICT_a4r4g4b4:
	case PICT_x4r4g4b4:
		return TRUE;
	}
	return FALSE;
}

static bool gen4_format_is_dst(uint32_t format)
{
	switch (format) {
	case GEN4_SURFACEFORMAT_B8G8R8A8_UNORM:
	case GEN4_SURFACEFORMAT_R8G8B8A8_UNORM:
	case GEN4_SURFACEFORMAT_B10G10R10A2_UNORM:
	case GEN4_SURFACEFORMAT_B5G6R5_UNORM:
	case GEN4_SURFACEFORMAT_B5G5R5A1_UNORM:
	case GEN4_SURFACEFORMAT_A8_UNORM:
	case GEN4_SURFACEFORMAT_B4G4R4A4_UNORM:
		return true;
	default:
		return false;
	}
}

typedef struct gen4_surface_state_padded {
	struct gen4_surface_state state;
	char pad[32 - sizeof(struct gen4_surface_state)];
} gen4_surface_state_padded;

static void null_create(struct sna_static_stream *stream)
{
	/* A bunch of zeros useful for legacy border color and depth-stencil */
	sna_static_stream_map(stream, 64, 64);
}

static void
sampler_state_init(struct gen4_sampler_state *sampler_state,
		   sampler_filter_t filter,
		   sampler_extend_t extend)
{
	sampler_state->ss0.lod_preclamp = 1;	/* GL mode */

	/* We use the legacy mode to get the semantics specified by
	 * the Render extension. */
	sampler_state->ss0.border_color_mode = GEN4_BORDER_COLOR_MODE_LEGACY;

	switch (filter) {
	default:
	case SAMPLER_FILTER_NEAREST:
		sampler_state->ss0.min_filter = GEN4_MAPFILTER_NEAREST;
		sampler_state->ss0.mag_filter = GEN4_MAPFILTER_NEAREST;
		break;
	case SAMPLER_FILTER_BILINEAR:
		sampler_state->ss0.min_filter = GEN4_MAPFILTER_LINEAR;
		sampler_state->ss0.mag_filter = GEN4_MAPFILTER_LINEAR;
		break;
	}

	switch (extend) {
	default:
	case SAMPLER_EXTEND_NONE:
		sampler_state->ss1.r_wrap_mode = GEN4_TEXCOORDMODE_CLAMP_BORDER;
		sampler_state->ss1.s_wrap_mode = GEN4_TEXCOORDMODE_CLAMP_BORDER;
		sampler_state->ss1.t_wrap_mode = GEN4_TEXCOORDMODE_CLAMP_BORDER;
		break;
	case SAMPLER_EXTEND_REPEAT:
		sampler_state->ss1.r_wrap_mode = GEN4_TEXCOORDMODE_WRAP;
		sampler_state->ss1.s_wrap_mode = GEN4_TEXCOORDMODE_WRAP;
		sampler_state->ss1.t_wrap_mode = GEN4_TEXCOORDMODE_WRAP;
		break;
	case SAMPLER_EXTEND_PAD:
		sampler_state->ss1.r_wrap_mode = GEN4_TEXCOORDMODE_CLAMP;
		sampler_state->ss1.s_wrap_mode = GEN4_TEXCOORDMODE_CLAMP;
		sampler_state->ss1.t_wrap_mode = GEN4_TEXCOORDMODE_CLAMP;
		break;
	case SAMPLER_EXTEND_REFLECT:
		sampler_state->ss1.r_wrap_mode = GEN4_TEXCOORDMODE_MIRROR;
		sampler_state->ss1.s_wrap_mode = GEN4_TEXCOORDMODE_MIRROR;
		sampler_state->ss1.t_wrap_mode = GEN4_TEXCOORDMODE_MIRROR;
		break;
	}
}

static uint32_t gen4_get_card_format(PictFormat format)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gen4_tex_formats); i++) {
		if (gen4_tex_formats[i].pict_fmt == format)
			return gen4_tex_formats[i].card_fmt;
	}
	return -1;
}

static uint32_t gen4_filter(uint32_t filter)
{
	switch (filter) {
	default:
		assert(0);
	case PictFilterNearest:
		return SAMPLER_FILTER_NEAREST;
	case PictFilterBilinear:
		return SAMPLER_FILTER_BILINEAR;
	}
}

static uint32_t gen4_check_filter(PicturePtr picture)
{
	switch (picture->filter) {
	case PictFilterNearest:
	case PictFilterBilinear:
		return TRUE;
	default:
		return FALSE;
	}
}

static uint32_t gen4_repeat(uint32_t repeat)
{
	switch (repeat) {
	default:
		assert(0);
	case RepeatNone:
		return SAMPLER_EXTEND_NONE;
	case RepeatNormal:
		return SAMPLER_EXTEND_REPEAT;
	case RepeatPad:
		return SAMPLER_EXTEND_PAD;
	case RepeatReflect:
		return SAMPLER_EXTEND_REFLECT;
	}
}

static bool gen4_check_repeat(PicturePtr picture)
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

/**
 * Sets up the common fields for a surface state buffer for the given
 * picture in the given surface state buffer.
 */
static int
gen4_bind_bo(struct sna *sna,
	     struct kgem_bo *bo,
	     uint32_t width,
	     uint32_t height,
	     uint32_t format,
	     Bool is_dst)
{
	struct gen4_surface_state *ss;
	uint32_t domains;
	uint16_t offset;

	/* After the first bind, we manage the cache domains within the batch */
	if (is_dst) {
		domains = I915_GEM_DOMAIN_RENDER << 16 | I915_GEM_DOMAIN_RENDER;
		kgem_bo_mark_dirty(bo);
	} else {
		domains = I915_GEM_DOMAIN_SAMPLER << 16;
		is_dst = gen4_format_is_dst(format);
	}

	offset = sna->kgem.surface - sizeof(struct gen4_surface_state_padded) / sizeof(uint32_t);
	offset *= sizeof(uint32_t);

	if (is_dst) {
		if (bo->dst_bound)
			return bo->dst_bound;

		bo->dst_bound = offset;
	} else {
		if (bo->src_bound)
			return bo->src_bound;

		bo->src_bound = offset;
	}

	sna->kgem.surface -=
		sizeof(struct gen4_surface_state_padded) / sizeof(uint32_t);
	ss = memset(sna->kgem.batch + sna->kgem.surface, 0, sizeof(*ss));

	ss->ss0.surface_type = GEN4_SURFACE_2D;
	ss->ss0.surface_format = format;

	ss->ss0.data_return_format = GEN4_SURFACERETURNFORMAT_FLOAT32;
	ss->ss0.color_blend = 1;
	ss->ss1.base_addr =
		kgem_add_reloc(&sna->kgem,
			       sna->kgem.surface + 1,
			       bo, domains, 0);

	ss->ss2.height = height - 1;
	ss->ss2.width  = width - 1;
	ss->ss3.pitch  = bo->pitch - 1;
	ss->ss3.tiled_surface = bo->tiling != I915_TILING_NONE;
	ss->ss3.tile_walk     = bo->tiling == I915_TILING_Y;

	DBG(("[%x] bind bo(handle=%d, addr=%d), format=%d, width=%d, height=%d, pitch=%d, tiling=%d -> %s\n",
	     offset, bo->handle, ss->ss1.base_addr,
	     ss->ss0.surface_format, width, height, bo->pitch, bo->tiling,
	     domains & 0xffff ? "render" : "sampler"));

	return offset;
}

fastcall static void
gen4_emit_composite_primitive_solid(struct sna *sna,
				    const struct sna_composite_op *op,
				    const struct sna_composite_rectangles *r)
{
	float *v;
	union {
		struct sna_coordinate p;
		float f;
	} dst;

	v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 9;

	dst.p.x = r->dst.x + r->width;
	dst.p.y = r->dst.y + r->height;
	v[0] = dst.f;
	v[1] = 1.;
	v[2] = 1.;

	dst.p.x = r->dst.x;
	v[3] = dst.f;
	v[4] = 0.;
	v[5] = 1.;

	dst.p.y = r->dst.y;
	v[6] = dst.f;
	v[7] = 0.;
	v[8] = 0.;
}

fastcall static void
gen4_emit_composite_primitive_identity_source(struct sna *sna,
					      const struct sna_composite_op *op,
					      const struct sna_composite_rectangles *r)
{
	const float *sf = op->src.scale;
	float sx, sy, *v;
	union {
		struct sna_coordinate p;
		float f;
	} dst;

	v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 9;

	sx = r->src.x + op->src.offset[0];
	sy = r->src.y + op->src.offset[1];

	dst.p.x = r->dst.x + r->width;
	dst.p.y = r->dst.y + r->height;
	v[0] = dst.f;
	v[1] = (sx + r->width) * sf[0];
	v[2] = (sy + r->height) * sf[1];

	dst.p.x = r->dst.x;
	v[3] = dst.f;
	v[4] = sx * sf[0];
	v[5] = v[2];

	dst.p.y = r->dst.y;
	v[6] = dst.f;
	v[7] = v[4];
	v[8] = sy * sf[1];
}

fastcall static void
gen4_emit_composite_primitive_affine_source(struct sna *sna,
					    const struct sna_composite_op *op,
					    const struct sna_composite_rectangles *r)
{
	union {
		struct sna_coordinate p;
		float f;
	} dst;
	float *v;

	v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 9;

	dst.p.x = r->dst.x + r->width;
	dst.p.y = r->dst.y + r->height;
	v[0] = dst.f;
	_sna_get_transformed_coordinates(op->src.offset[0] + r->src.x + r->width,
					 op->src.offset[1] + r->src.y + r->height,
					 op->src.transform,
					 &v[1], &v[2]);
	v[1] *= op->src.scale[0];
	v[2] *= op->src.scale[1];

	dst.p.x = r->dst.x;
	v[3] = dst.f;
	_sna_get_transformed_coordinates(op->src.offset[0] + r->src.x,
					 op->src.offset[1] + r->src.y + r->height,
					 op->src.transform,
					 &v[4], &v[5]);
	v[4] *= op->src.scale[0];
	v[5] *= op->src.scale[1];

	dst.p.y = r->dst.y;
	v[6] = dst.f;
	_sna_get_transformed_coordinates(op->src.offset[0] + r->src.x,
					 op->src.offset[1] + r->src.y,
					 op->src.transform,
					 &v[7], &v[8]);
	v[7] *= op->src.scale[0];
	v[8] *= op->src.scale[1];
}

fastcall static void
gen4_emit_composite_primitive_identity_source_mask(struct sna *sna,
						   const struct sna_composite_op *op,
						   const struct sna_composite_rectangles *r)
{
	union {
		struct sna_coordinate p;
		float f;
	} dst;
	float src_x, src_y;
	float msk_x, msk_y;
	float w, h;
	float *v;

	src_x = r->src.x + op->src.offset[0];
	src_y = r->src.y + op->src.offset[1];
	msk_x = r->mask.x + op->mask.offset[0];
	msk_y = r->mask.y + op->mask.offset[1];
	w = r->width;
	h = r->height;

	v = sna->render.vertex_data + sna->render.vertex_used;
	sna->render.vertex_used += 15;

	dst.p.x = r->dst.x + r->width;
	dst.p.y = r->dst.y + r->height;
	v[0] = dst.f;
	v[1] = (src_x + w) * op->src.scale[0];
	v[2] = (src_y + h) * op->src.scale[1];
	v[3] = (msk_x + w) * op->mask.scale[0];
	v[4] = (msk_y + h) * op->mask.scale[1];

	dst.p.x = r->dst.x;
	v[5] = dst.f;
	v[6] = src_x * op->src.scale[0];
	v[7] = v[2];
	v[8] = msk_x * op->mask.scale[0];
	v[9] = v[4];

	dst.p.y = r->dst.y;
	v[10] = dst.f;
	v[11] = v[6];
	v[12] = src_y * op->src.scale[1];
	v[13] = v[8];
	v[14] = msk_y * op->mask.scale[1];
}

fastcall static void
gen4_emit_composite_primitive(struct sna *sna,
			      const struct sna_composite_op *op,
			      const struct sna_composite_rectangles *r)
{
	float src_x[3], src_y[3], src_w[3], mask_x[3], mask_y[3], mask_w[3];
	Bool is_affine = op->is_affine;
	const float *src_sf = op->src.scale;
	const float *mask_sf = op->mask.scale;

	if (is_affine) {
		sna_get_transformed_coordinates(r->src.x + op->src.offset[0],
						r->src.y + op->src.offset[1],
						op->src.transform,
						&src_x[0],
						&src_y[0]);

		sna_get_transformed_coordinates(r->src.x + op->src.offset[0],
						r->src.y + op->src.offset[1] + r->height,
						op->src.transform,
						&src_x[1],
						&src_y[1]);

		sna_get_transformed_coordinates(r->src.x + op->src.offset[0] + r->width,
						r->src.y + op->src.offset[1] + r->height,
						op->src.transform,
						&src_x[2],
						&src_y[2]);
	} else {
		if (!sna_get_transformed_coordinates_3d(r->src.x + op->src.offset[0],
							r->src.y + op->src.offset[1],
							op->src.transform,
							&src_x[0],
							&src_y[0],
							&src_w[0]))
			return;

		if (!sna_get_transformed_coordinates_3d(r->src.x + op->src.offset[0],
							r->src.y + op->src.offset[1] + r->height,
							op->src.transform,
							&src_x[1],
							&src_y[1],
							&src_w[1]))
			return;

		if (!sna_get_transformed_coordinates_3d(r->src.x + op->src.offset[0] + r->width,
							r->src.y + op->src.offset[1] + r->height,
							op->src.transform,
							&src_x[2],
							&src_y[2],
							&src_w[2]))
			return;
	}

	if (op->mask.bo) {
		if (is_affine) {
			sna_get_transformed_coordinates(r->mask.x + op->mask.offset[0],
							r->mask.y + op->mask.offset[1],
							op->mask.transform,
							&mask_x[0],
							&mask_y[0]);

			sna_get_transformed_coordinates(r->mask.x + op->mask.offset[0],
							r->mask.y + op->mask.offset[1] + r->height,
							op->mask.transform,
							&mask_x[1],
							&mask_y[1]);

			sna_get_transformed_coordinates(r->mask.x + op->mask.offset[0] + r->width,
							r->mask.y + op->mask.offset[1] + r->height,
							op->mask.transform,
							&mask_x[2],
							&mask_y[2]);
		} else {
			if (!sna_get_transformed_coordinates_3d(r->mask.x + op->mask.offset[0],
								r->mask.y + op->mask.offset[1],
								op->mask.transform,
								&mask_x[0],
								&mask_y[0],
								&mask_w[0]))
				return;

			if (!sna_get_transformed_coordinates_3d(r->mask.x + op->mask.offset[0],
								r->mask.y + op->mask.offset[1] + r->height,
								op->mask.transform,
								&mask_x[1],
								&mask_y[1],
								&mask_w[1]))
				return;

			if (!sna_get_transformed_coordinates_3d(r->mask.x + op->mask.offset[0] + r->width,
								r->mask.y + op->mask.offset[1] + r->height,
								op->mask.transform,
								&mask_x[2],
								&mask_y[2],
								&mask_w[2]))
				return;
		}
	}

	OUT_VERTEX(r->dst.x + r->width, r->dst.y + r->height);
	OUT_VERTEX_F(src_x[2] * src_sf[0]);
	OUT_VERTEX_F(src_y[2] * src_sf[1]);
	if (!is_affine)
		OUT_VERTEX_F(src_w[2]);
	if (op->mask.bo) {
		OUT_VERTEX_F(mask_x[2] * mask_sf[0]);
		OUT_VERTEX_F(mask_y[2] * mask_sf[1]);
		if (!is_affine)
			OUT_VERTEX_F(mask_w[2]);
	}

	OUT_VERTEX(r->dst.x, r->dst.y + r->height);
	OUT_VERTEX_F(src_x[1] * src_sf[0]);
	OUT_VERTEX_F(src_y[1] * src_sf[1]);
	if (!is_affine)
		OUT_VERTEX_F(src_w[1]);
	if (op->mask.bo) {
		OUT_VERTEX_F(mask_x[1] * mask_sf[0]);
		OUT_VERTEX_F(mask_y[1] * mask_sf[1]);
		if (!is_affine)
			OUT_VERTEX_F(mask_w[1]);
	}

	OUT_VERTEX(r->dst.x, r->dst.y);
	OUT_VERTEX_F(src_x[0] * src_sf[0]);
	OUT_VERTEX_F(src_y[0] * src_sf[1]);
	if (!is_affine)
		OUT_VERTEX_F(src_w[0]);
	if (op->mask.bo) {
		OUT_VERTEX_F(mask_x[0] * mask_sf[0]);
		OUT_VERTEX_F(mask_y[0] * mask_sf[1]);
		if (!is_affine)
			OUT_VERTEX_F(mask_w[0]);
	}
}

static void gen4_emit_vertex_buffer(struct sna *sna,
				    const struct sna_composite_op *op)
{
	int id = op->u.gen4.ve_id;

	OUT_BATCH(GEN4_3DSTATE_VERTEX_BUFFERS | 3);
	OUT_BATCH((id << VB0_BUFFER_INDEX_SHIFT) | VB0_VERTEXDATA |
		  (4*op->floats_per_vertex << VB0_BUFFER_PITCH_SHIFT));
	sna->render.vertex_reloc[id] = sna->kgem.nbatch;
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);

	sna->render_state.gen4.vb_id |= 1 << id;
}

static void gen4_emit_primitive(struct sna *sna)
{
	if (sna->kgem.nbatch == sna->render_state.gen4.last_primitive) {
		sna->render_state.gen4.vertex_offset = sna->kgem.nbatch - 5;
		return;
	}

	OUT_BATCH(GEN4_3DPRIMITIVE |
		  GEN4_3DPRIMITIVE_VERTEX_SEQUENTIAL |
		  (_3DPRIM_RECTLIST << GEN4_3DPRIMITIVE_TOPOLOGY_SHIFT) |
		  (0 << 9) |
		  4);
	sna->render_state.gen4.vertex_offset = sna->kgem.nbatch;
	OUT_BATCH(0);	/* vertex count, to be filled in later */
	OUT_BATCH(sna->render.vertex_index);
	OUT_BATCH(1);	/* single instance */
	OUT_BATCH(0);	/* start instance location */
	OUT_BATCH(0);	/* index buffer offset, ignored */
	sna->render.vertex_start = sna->render.vertex_index;

	sna->render_state.gen4.last_primitive = sna->kgem.nbatch;
}

static bool gen4_rectangle_begin(struct sna *sna,
				 const struct sna_composite_op *op)
{
	int id = op->u.gen4.ve_id;
	int ndwords;

	ndwords = 0;
	if ((sna->render_state.gen4.vb_id & (1 << id)) == 0)
		ndwords += 5;
	if (sna->render_state.gen4.vertex_offset == 0)
		ndwords += op->need_magic_ca_pass ? 20 : 6;
	if (ndwords == 0)
		return true;

	if (!kgem_check_batch(&sna->kgem, ndwords))
		return false;

	if ((sna->render_state.gen4.vb_id & (1 << id)) == 0)
		gen4_emit_vertex_buffer(sna, op);
	if (sna->render_state.gen4.vertex_offset == 0)
		gen4_emit_primitive(sna);

	return true;
}

static int gen4_get_rectangles__flush(struct sna *sna)
{
	if (!kgem_check_batch(&sna->kgem, 25))
		return 0;
	if (sna->kgem.nexec > KGEM_EXEC_SIZE(&sna->kgem) - 1)
		return 0;
	if (sna->kgem.nreloc > KGEM_RELOC_SIZE(&sna->kgem) - 1)
		return 0;

	gen4_vertex_finish(sna, FALSE);
	sna->render.vertex_index = 0;

	return ARRAY_SIZE(sna->render.vertex_data);
}

inline static int gen4_get_rectangles(struct sna *sna,
				      const struct sna_composite_op *op,
				      int want)
{
	int rem = vertex_space(sna);

	if (rem < 3*op->floats_per_vertex) {
		DBG(("flushing vbo for %s: %d < %d\n",
		     __FUNCTION__, rem, 3*op->floats_per_vertex));
		rem = gen4_get_rectangles__flush(sna);
		if (rem == 0)
			return 0;
	}

	if (!gen4_rectangle_begin(sna, op))
		return 0;

	if (want > 1 && want * op->floats_per_vertex*3 > rem)
		want = rem / (3*op->floats_per_vertex);

	sna->render.vertex_index += 3*want;
	return want;
}

static uint32_t *gen4_composite_get_binding_table(struct sna *sna,
						  const struct sna_composite_op *op,
						  uint16_t *offset)
{
	sna->kgem.surface -=
		sizeof(struct gen4_surface_state_padded) / sizeof(uint32_t);

	DBG(("%s(%x)\n", __FUNCTION__, 4*sna->kgem.surface));

	/* Clear all surplus entries to zero in case of prefetch */
	*offset = sna->kgem.surface;
	return memset(sna->kgem.batch + sna->kgem.surface,
		      0, sizeof(struct gen4_surface_state_padded));
}

static void
gen4_emit_sip(struct sna *sna)
{
	/* Set system instruction pointer */
	OUT_BATCH(GEN4_STATE_SIP | 0);
	OUT_BATCH(0);
}

static void
gen4_emit_urb(struct sna *sna)
{
	int urb_vs_start, urb_vs_size;
	int urb_gs_start, urb_gs_size;
	int urb_clip_start, urb_clip_size;
	int urb_sf_start, urb_sf_size;
	int urb_cs_start, urb_cs_size;

	if (!sna->render_state.gen4.needs_urb)
		return;

	urb_vs_start = 0;
	urb_vs_size = URB_VS_ENTRIES * URB_VS_ENTRY_SIZE;
	urb_gs_start = urb_vs_start + urb_vs_size;
	urb_gs_size = URB_GS_ENTRIES * URB_GS_ENTRY_SIZE;
	urb_clip_start = urb_gs_start + urb_gs_size;
	urb_clip_size = URB_CLIP_ENTRIES * URB_CLIP_ENTRY_SIZE;
	urb_sf_start = urb_clip_start + urb_clip_size;
	urb_sf_size = URB_SF_ENTRIES * URB_SF_ENTRY_SIZE;
	urb_cs_start = urb_sf_start + urb_sf_size;
	urb_cs_size = URB_CS_ENTRIES * URB_CS_ENTRY_SIZE;

	OUT_BATCH(GEN4_URB_FENCE |
		  UF0_CS_REALLOC |
		  UF0_SF_REALLOC |
		  UF0_CLIP_REALLOC |
		  UF0_GS_REALLOC |
		  UF0_VS_REALLOC |
		  1);
	OUT_BATCH(((urb_clip_start + urb_clip_size) << UF1_CLIP_FENCE_SHIFT) |
		  ((urb_gs_start + urb_gs_size) << UF1_GS_FENCE_SHIFT) |
		  ((urb_vs_start + urb_vs_size) << UF1_VS_FENCE_SHIFT));
	OUT_BATCH(((urb_cs_start + urb_cs_size) << UF2_CS_FENCE_SHIFT) |
		  ((urb_sf_start + urb_sf_size) << UF2_SF_FENCE_SHIFT));

	/* Constant buffer state */
	OUT_BATCH(GEN4_CS_URB_STATE | 0);
	OUT_BATCH((URB_CS_ENTRY_SIZE - 1) << 4 | URB_CS_ENTRIES << 0);

	sna->render_state.gen4.needs_urb = false;
}

static void
gen4_emit_state_base_address(struct sna *sna)
{
	assert(sna->render_state.gen4.general_bo->proxy == NULL);
	OUT_BATCH(GEN4_STATE_BASE_ADDRESS | 4);
	OUT_BATCH(kgem_add_reloc(&sna->kgem, /* general */
				 sna->kgem.nbatch,
				 sna->render_state.gen4.general_bo,
				 I915_GEM_DOMAIN_INSTRUCTION << 16,
				 BASE_ADDRESS_MODIFY));
	OUT_BATCH(kgem_add_reloc(&sna->kgem, /* surface */
				 sna->kgem.nbatch,
				 NULL,
				 I915_GEM_DOMAIN_INSTRUCTION << 16,
				 BASE_ADDRESS_MODIFY));
	OUT_BATCH(0); /* media */

	/* upper bounds, all disabled */
	OUT_BATCH(BASE_ADDRESS_MODIFY);
	OUT_BATCH(0);
}

static void
gen4_emit_invariant(struct sna *sna)
{
	OUT_BATCH(MI_FLUSH | MI_INHIBIT_RENDER_CACHE_FLUSH);
	if (sna->kgem.gen >= 45)
		OUT_BATCH(NEW_PIPELINE_SELECT | PIPELINE_SELECT_3D);
	else
		OUT_BATCH(GEN4_PIPELINE_SELECT | PIPELINE_SELECT_3D);

	gen4_emit_sip(sna);
	gen4_emit_state_base_address(sna);

	sna->render_state.gen4.needs_invariant = FALSE;
}

static void
gen4_get_batch(struct sna *sna)
{
	kgem_set_mode(&sna->kgem, KGEM_RENDER);

	if (!kgem_check_batch_with_surfaces(&sna->kgem, 150, 4)) {
		DBG(("%s: flushing batch: %d < %d+%d\n",
		     __FUNCTION__, sna->kgem.surface - sna->kgem.nbatch,
		     150, 4*8));
		kgem_submit(&sna->kgem);
	}

	if (sna->render_state.gen4.needs_invariant)
		gen4_emit_invariant(sna);
}

static void
gen4_align_vertex(struct sna *sna, const struct sna_composite_op *op)
{
	if (op->floats_per_vertex != sna->render_state.gen4.floats_per_vertex) {
		DBG(("aligning vertex: was %d, now %d floats per vertex, %d->%d\n",
		     sna->render_state.gen4.floats_per_vertex,
		     op->floats_per_vertex,
		     sna->render.vertex_index,
		     (sna->render.vertex_used + op->floats_per_vertex - 1) / op->floats_per_vertex));
		sna->render.vertex_index = (sna->render.vertex_used + op->floats_per_vertex - 1) / op->floats_per_vertex;
		sna->render.vertex_used = sna->render.vertex_index * op->floats_per_vertex;
		sna->render_state.gen4.floats_per_vertex = op->floats_per_vertex;
	}
}

static void
gen4_emit_binding_table(struct sna *sna, uint16_t offset)
{
	if (sna->render_state.gen4.surface_table == offset)
		return;

	sna->render_state.gen4.surface_table = offset;

	/* Binding table pointers */
	OUT_BATCH(GEN4_3DSTATE_BINDING_TABLE_POINTERS | 4);
	OUT_BATCH(0);		/* vs */
	OUT_BATCH(0);		/* gs */
	OUT_BATCH(0);		/* clip */
	OUT_BATCH(0);		/* sf */
	/* Only the PS uses the binding table */
	OUT_BATCH(offset*4);
}

static void
gen4_emit_pipelined_pointers(struct sna *sna,
			     const struct sna_composite_op *op,
			     int blend, int kernel)
{
	uint16_t offset = sna->kgem.nbatch, last;

	DBG(("%s: has_mask=%d, src=(%d, %d), mask=(%d, %d),kernel=%d, blend=%d, ca=%d, format=%x\n",
	     __FUNCTION__, op->mask.bo != NULL,
	     op->src.filter, op->src.repeat,
	     op->mask.filter, op->mask.repeat,
	     kernel, blend, op->has_component_alpha, op->dst.format));

	OUT_BATCH(GEN4_3DSTATE_PIPELINED_POINTERS | 5);
	OUT_BATCH(sna->render_state.gen4.vs);
	OUT_BATCH(GEN4_GS_DISABLE); /* passthrough */
	OUT_BATCH(GEN4_CLIP_DISABLE); /* passthrough */
	OUT_BATCH(sna->render_state.gen4.sf[op->mask.bo != NULL]);
	OUT_BATCH(sna->render_state.gen4.wm +
		  SAMPLER_OFFSET(op->src.filter, op->src.repeat,
				 op->mask.filter, op->mask.repeat,
				 kernel));
	OUT_BATCH(sna->render_state.gen4.cc +
		  gen4_get_blend(blend, op->has_component_alpha, op->dst.format));

	last = sna->render_state.gen4.last_pipelined_pointers;
	if (last &&
	    sna->kgem.batch[offset + 4] == sna->kgem.batch[last + 4] &&
	    sna->kgem.batch[offset + 5] == sna->kgem.batch[last + 5] &&
	    sna->kgem.batch[offset + 6] == sna->kgem.batch[last + 6]) {
		sna->kgem.nbatch = offset;
	} else {
		sna->render_state.gen4.last_pipelined_pointers = offset;
		gen4_emit_urb(sna);
	}
}

static void
gen4_emit_drawing_rectangle(struct sna *sna, const struct sna_composite_op *op)
{
	uint32_t limit = (op->dst.height - 1) << 16 | (op->dst.width - 1);
	uint32_t offset = (uint16_t)op->dst.y << 16 | (uint16_t)op->dst.x;

	if (sna->render_state.gen4.drawrect_limit == limit &&
	    sna->render_state.gen4.drawrect_offset == offset)
		return;
	sna->render_state.gen4.drawrect_offset = offset;
	sna->render_state.gen4.drawrect_limit = limit;

	OUT_BATCH(GEN4_3DSTATE_DRAWING_RECTANGLE | (4 - 2));
	OUT_BATCH(0x00000000);
	OUT_BATCH(limit);
	OUT_BATCH(offset);
}

static void
gen4_emit_vertex_elements(struct sna *sna,
			  const struct sna_composite_op *op)
{
	/*
	 * vertex data in vertex buffer
	 *    position: (x, y)
	 *    texture coordinate 0: (u0, v0) if (is_affine is TRUE) else (u0, v0, w0)
	 *    texture coordinate 1 if (has_mask is TRUE): same as above
	 */
	struct gen4_render_state *render = &sna->render_state.gen4;
	Bool has_mask = op->mask.bo != NULL;
	int nelem = has_mask ? 2 : 1;
	int selem;
	uint32_t w_component;
	uint32_t src_format;
	int id = op->u.gen4.ve_id;

	if (render->ve_id == id)
		return;

	render->ve_id = id;

	if (op->is_affine) {
		src_format = GEN4_SURFACEFORMAT_R32G32_FLOAT;
		w_component = GEN4_VFCOMPONENT_STORE_1_FLT;
		selem = 2;
	} else {
		src_format = GEN4_SURFACEFORMAT_R32G32B32_FLOAT;
		w_component = GEN4_VFCOMPONENT_STORE_SRC;
		selem = 3;
	}

	/* The VUE layout
	 *    dword 0-3: position (x, y, 1.0, 1.0),
	 *    dword 4-7: texture coordinate 0 (u0, v0, w0, 1.0)
	 *    [optional] dword 8-11: texture coordinate 1 (u1, v1, w1, 1.0)
	 */
	OUT_BATCH(GEN4_3DSTATE_VERTEX_ELEMENTS | (2 * (1 + nelem) - 1));

	/* x,y */
	OUT_BATCH(id << VE0_VERTEX_BUFFER_INDEX_SHIFT | VE0_VALID |
		  GEN4_SURFACEFORMAT_R16G16_SSCALED << VE0_FORMAT_SHIFT |
		  0 << VE0_OFFSET_SHIFT); /* offsets vb in bytes */
	OUT_BATCH(GEN4_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT |
		  GEN4_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT |
		  GEN4_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_2_SHIFT |
		  GEN4_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT |
		  (1*4) << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT);       /* VUE offset in dwords */

	/* u0, v0, w0 */
	OUT_BATCH(id << VE0_VERTEX_BUFFER_INDEX_SHIFT | VE0_VALID |
		  src_format << VE0_FORMAT_SHIFT |
		  4 << VE0_OFFSET_SHIFT);	/* offset vb in bytes */
	OUT_BATCH(GEN4_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT |
		  GEN4_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT |
		  w_component << VE1_VFCOMPONENT_2_SHIFT |
		  GEN4_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT |
		  (2*4) << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT);       /* VUE offset in dwords */

	/* u1, v1, w1 */
	if (has_mask) {
		OUT_BATCH(id << VE0_VERTEX_BUFFER_INDEX_SHIFT | VE0_VALID |
			  src_format << VE0_FORMAT_SHIFT |
			  ((1 + selem) * 4) << VE0_OFFSET_SHIFT); /* vb offset in bytes */
		OUT_BATCH(GEN4_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT |
			  GEN4_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT |
			  w_component << VE1_VFCOMPONENT_2_SHIFT |
			  GEN4_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT |
			  (3*4) << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT);       /* VUE offset in dwords */
	}
}

static void
gen4_emit_state(struct sna *sna,
		const struct sna_composite_op *op,
	       	uint16_t wm_binding_table)
{
	gen4_emit_binding_table(sna, wm_binding_table);
	gen4_emit_pipelined_pointers(sna, op, op->op, op->u.gen4.wm_kernel);
	gen4_emit_vertex_elements(sna, op);
	gen4_emit_drawing_rectangle(sna, op);
}

static void
gen4_bind_surfaces(struct sna *sna,
		   const struct sna_composite_op *op)
{
	uint32_t *binding_table;
	uint16_t offset;

	gen4_get_batch(sna);

	binding_table = gen4_composite_get_binding_table(sna, op, &offset);

	binding_table[0] =
		gen4_bind_bo(sna,
			    op->dst.bo, op->dst.width, op->dst.height,
			    gen4_get_dest_format(op->dst.format),
			    TRUE);
	binding_table[1] =
		gen4_bind_bo(sna,
			     op->src.bo, op->src.width, op->src.height,
			     op->src.card_format,
			     FALSE);
	if (op->mask.bo)
		binding_table[2] =
			gen4_bind_bo(sna,
				     op->mask.bo,
				     op->mask.width,
				     op->mask.height,
				     op->mask.card_format,
				     FALSE);

	if (sna->kgem.surface == offset &&
	    *(uint64_t *)(sna->kgem.batch + sna->render_state.gen4.surface_table) == *(uint64_t*)binding_table &&
	    (op->mask.bo == NULL ||
	     sna->kgem.batch[sna->render_state.gen4.surface_table+2] == binding_table[2])) {
		sna->kgem.surface += sizeof(struct gen4_surface_state_padded) / sizeof(uint32_t);
		offset = sna->render_state.gen4.surface_table;
	}

	gen4_emit_state(sna, op, offset);
}

fastcall static void
gen4_render_composite_blt(struct sna *sna,
			  const struct sna_composite_op *op,
			  const struct sna_composite_rectangles *r)
{
	DBG(("%s: src=(%d, %d)+(%d, %d), mask=(%d, %d)+(%d, %d), dst=(%d, %d)+(%d, %d), size=(%d, %d)\n",
	     __FUNCTION__,
	     r->src.x, r->src.y, op->src.offset[0], op->src.offset[1],
	     r->mask.x, r->mask.y, op->mask.offset[0], op->mask.offset[1],
	     r->dst.x, r->dst.y, op->dst.x, op->dst.y,
	     r->width, r->height));

	if (FLUSH_EVERY_VERTEX && op->need_magic_ca_pass)
		/* We have to reset the state after every FLUSH */
		gen4_emit_pipelined_pointers(sna, op,
					     op->op, op->u.gen4.wm_kernel);

	if (!gen4_get_rectangles(sna, op, 1)) {
		gen4_bind_surfaces(sna, op);
		gen4_get_rectangles(sna, op, 1);
	}

	op->prim_emit(sna, op, r);

	/* XXX are the shaders fubar? */
	FLUSH();
}

static void
gen4_render_composite_boxes(struct sna *sna,
			    const struct sna_composite_op *op,
			    const BoxRec *box, int nbox)
{
	DBG(("%s(%d) delta=(%d, %d), src=(%d, %d)/(%d, %d), mask=(%d, %d)/(%d, %d)\n",
	     __FUNCTION__, nbox, op->dst.x, op->dst.y,
	     op->src.offset[0], op->src.offset[1],
	     op->src.width, op->src.height,
	     op->mask.offset[0], op->mask.offset[1],
	     op->mask.width, op->mask.height));

	do {
		struct sna_composite_rectangles r;

		r.dst.x = box->x1;
		r.dst.y = box->y1;
		r.width  = box->x2 - box->x1;
		r.height = box->y2 - box->y1;
		r.mask = r.src = r.dst;
		gen4_render_composite_blt(sna, op, &r);
		box++;
	} while (--nbox);
}

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

static uint32_t gen4_bind_video_source(struct sna *sna,
				       struct kgem_bo *src_bo,
				       uint32_t src_offset,
				       int src_width,
				       int src_height,
				       int src_pitch,
				       uint32_t src_surf_format)
{
	struct gen4_surface_state *ss;

	sna->kgem.surface -= sizeof(struct gen4_surface_state_padded) / sizeof(uint32_t);

	ss = memset(sna->kgem.batch + sna->kgem.surface, 0, sizeof(*ss));
	ss->ss0.surface_type = GEN4_SURFACE_2D;
	ss->ss0.surface_format = src_surf_format;
	ss->ss0.color_blend = 1;

	ss->ss1.base_addr =
		kgem_add_reloc(&sna->kgem,
			       sna->kgem.surface + 1,
			       src_bo,
			       I915_GEM_DOMAIN_SAMPLER << 16,
			       src_offset);

	ss->ss2.width  = src_width - 1;
	ss->ss2.height = src_height - 1;
	ss->ss3.pitch  = src_pitch - 1;

	return sna->kgem.surface * sizeof(uint32_t);
}

static void gen4_video_bind_surfaces(struct sna *sna,
				     const struct sna_composite_op *op,
				     struct sna_video_frame *frame)
{
	uint32_t src_surf_format;
	uint32_t src_surf_base[6];
	int src_width[6];
	int src_height[6];
	int src_pitch[6];
	uint32_t *binding_table;
	uint16_t offset;
	int n_src, n;

	src_surf_base[0] = frame->YBufOffset;
	src_surf_base[1] = frame->YBufOffset;
	src_surf_base[2] = frame->VBufOffset;
	src_surf_base[3] = frame->VBufOffset;
	src_surf_base[4] = frame->UBufOffset;
	src_surf_base[5] = frame->UBufOffset;

	if (is_planar_fourcc(frame->id)) {
		src_surf_format = GEN4_SURFACEFORMAT_R8_UNORM;
		src_width[1]  = src_width[0]  = frame->width;
		src_height[1] = src_height[0] = frame->height;
		src_pitch[1]  = src_pitch[0]  = frame->pitch[1];
		src_width[4]  = src_width[5]  = src_width[2]  = src_width[3] =
			frame->width / 2;
		src_height[4] = src_height[5] = src_height[2] = src_height[3] =
			frame->height / 2;
		src_pitch[4]  = src_pitch[5]  = src_pitch[2]  = src_pitch[3] =
			frame->pitch[0];
		n_src = 6;
	} else {
		if (frame->id == FOURCC_UYVY)
			src_surf_format = GEN4_SURFACEFORMAT_YCRCB_SWAPY;
		else
			src_surf_format = GEN4_SURFACEFORMAT_YCRCB_NORMAL;

		src_width[0]  = frame->width;
		src_height[0] = frame->height;
		src_pitch[0]  = frame->pitch[0];
		n_src = 1;
	}

	gen4_get_batch(sna);

	binding_table = gen4_composite_get_binding_table(sna, op, &offset);

	binding_table[0] =
		gen4_bind_bo(sna,
			     op->dst.bo, op->dst.width, op->dst.height,
			     gen4_get_dest_format(op->dst.format),
			     TRUE);
	for (n = 0; n < n_src; n++) {
		binding_table[1+n] =
			gen4_bind_video_source(sna,
					       frame->bo,
					       src_surf_base[n],
					       src_width[n],
					       src_height[n],
					       src_pitch[n],
					       src_surf_format);
	}

	gen4_emit_state(sna, op, offset);
}

static Bool
gen4_render_video(struct sna *sna,
		  struct sna_video *video,
		  struct sna_video_frame *frame,
		  RegionPtr dstRegion,
		  short src_w, short src_h,
		  short drw_w, short drw_h,
		  PixmapPtr pixmap)
{
	struct sna_composite_op tmp;
	int nbox, dxo, dyo, pix_xoff, pix_yoff;
	float src_scale_x, src_scale_y;
	struct sna_pixmap *priv;
	BoxPtr box;

	DBG(("%s: %dx%d -> %dx%d\n", __FUNCTION__, src_w, src_h, drw_w, drw_h));

	priv = sna_pixmap_force_to_gpu(pixmap);
	if (priv == NULL)
		return FALSE;

	memset(&tmp, 0, sizeof(tmp));

	tmp.op = PictOpSrc;
	tmp.dst.pixmap = pixmap;
	tmp.dst.width  = pixmap->drawable.width;
	tmp.dst.height = pixmap->drawable.height;
	tmp.dst.format = sna_format_for_depth(pixmap->drawable.depth);
	tmp.dst.bo = priv->gpu_bo;

	tmp.src.filter = SAMPLER_FILTER_BILINEAR;
	tmp.src.repeat = SAMPLER_EXTEND_NONE;
	tmp.u.gen4.wm_kernel =
		is_planar_fourcc(frame->id) ? WM_KERNEL_VIDEO_PLANAR : WM_KERNEL_VIDEO_PACKED;
	tmp.is_affine = TRUE;
	tmp.floats_per_vertex = 3;
	tmp.u.gen4.ve_id = 1;

	if (!kgem_check_bo(&sna->kgem, tmp.dst.bo))
		kgem_submit(&sna->kgem);
	if (!kgem_check_bo(&sna->kgem, frame->bo))
		kgem_submit(&sna->kgem);

	if (!kgem_bo_is_dirty(frame->bo))
		kgem_emit_flush(&sna->kgem);

	gen4_video_bind_surfaces(sna, &tmp, frame);
	gen4_align_vertex(sna, &tmp);

	/* Set up the offset for translating from the given region (in screen
	 * coordinates) to the backing pixmap.
	 */
#ifdef COMPOSITE
	pix_xoff = -pixmap->screen_x + pixmap->drawable.x;
	pix_yoff = -pixmap->screen_y + pixmap->drawable.y;
#else
	pix_xoff = 0;
	pix_yoff = 0;
#endif

	dxo = dstRegion->extents.x1;
	dyo = dstRegion->extents.y1;

	/* Use normalized texture coordinates */
	src_scale_x = ((float)src_w / frame->width) / (float)drw_w;
	src_scale_y = ((float)src_h / frame->height) / (float)drw_h;

	box = REGION_RECTS(dstRegion);
	nbox = REGION_NUM_RECTS(dstRegion);
	while (nbox--) {
		BoxRec r;

		r.x1 = box->x1 + pix_xoff;
		r.x2 = box->x2 + pix_xoff;
		r.y1 = box->y1 + pix_yoff;
		r.y2 = box->y2 + pix_yoff;

		if (!gen4_get_rectangles(sna, &tmp, 1)) {
			gen4_video_bind_surfaces(sna, &tmp, frame);
			gen4_get_rectangles(sna, &tmp, 1);
		}

		OUT_VERTEX(r.x2, r.y2);
		OUT_VERTEX_F((box->x2 - dxo) * src_scale_x);
		OUT_VERTEX_F((box->y2 - dyo) * src_scale_y);

		OUT_VERTEX(r.x1, r.y2);
		OUT_VERTEX_F((box->x1 - dxo) * src_scale_x);
		OUT_VERTEX_F((box->y2 - dyo) * src_scale_y);

		OUT_VERTEX(r.x1, r.y1);
		OUT_VERTEX_F((box->x1 - dxo) * src_scale_x);
		OUT_VERTEX_F((box->y1 - dyo) * src_scale_y);

		FLUSH();

		sna_damage_add_box(&priv->gpu_damage, &r);
		sna_damage_subtract_box(&priv->cpu_damage, &r);
		box++;
	}

	return TRUE;
}

static Bool
gen4_composite_solid_init(struct sna *sna,
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
	channel->card_format = GEN4_SURFACEFORMAT_B8G8R8A8_UNORM;

	channel->bo = sna_render_get_solid(sna, color);

	channel->scale[0]  = channel->scale[1]  = 1;
	channel->offset[0] = channel->offset[1] = 0;
	return channel->bo != NULL;
}

static int
gen4_composite_picture(struct sna *sna,
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
		return gen4_composite_solid_init(sna, channel, color);

	if (picture->pDrawable == NULL)
		return sna_render_picture_fixup(sna, picture, channel,
						x, y, w, h, dst_x, dst_y);

	if (!gen4_check_repeat(picture))
		return sna_render_picture_fixup(sna, picture, channel,
						x, y, w, h, dst_x, dst_y);

	if (!gen4_check_filter(picture))
		return sna_render_picture_fixup(sna, picture, channel,
						x, y, w, h, dst_x, dst_y);

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

	channel->card_format = gen4_get_card_format(picture->format);
	if (channel->card_format == -1)
		return sna_render_picture_convert(sna, picture, channel, pixmap,
						  x, y, w, h, dst_x, dst_y);

	if (pixmap->drawable.width > 8192 || pixmap->drawable.height > 8192)
		return sna_render_picture_extract(sna, picture, channel,
						  x, y, w, h, dst_x, dst_y);

	return sna_render_pixmap_bo(sna, channel, pixmap,
				    x, y, w, h, dst_x, dst_y);
}

static void gen4_composite_channel_convert(struct sna_composite_channel *channel)
{
	channel->repeat = gen4_repeat(channel->repeat);
	channel->filter = gen4_filter(channel->filter);
	if (channel->card_format == -1)
		channel->card_format = gen4_get_card_format(channel->pict_format);
}

static void
gen4_render_composite_done(struct sna *sna,
			   const struct sna_composite_op *op)
{
	gen4_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
	sna->render.op = NULL;

	DBG(("%s()\n", __FUNCTION__));

	sna_render_composite_redirect_done(sna, op);

	if (op->src.bo)
		kgem_bo_destroy(&sna->kgem, op->src.bo);
	if (op->mask.bo)
		kgem_bo_destroy(&sna->kgem, op->mask.bo);
}

static Bool
gen4_composite_set_target(struct sna *sna,
			  PicturePtr dst,
			  struct sna_composite_op *op)
{
	struct sna_pixmap *priv;

	if (!gen4_check_dst_format(dst->format)) {
		DBG(("%s: incompatible render target format %08x\n",
		     __FUNCTION__, dst->format));
		return FALSE;
	}

	op->dst.pixmap = get_drawable_pixmap(dst->pDrawable);
	op->dst.width  = op->dst.pixmap->drawable.width;
	op->dst.height = op->dst.pixmap->drawable.height;
	op->dst.format = dst->format;
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

static inline Bool
picture_is_cpu(PicturePtr picture)
{
	if (!picture->pDrawable)
		return FALSE;

	/* If it is a solid, try to use the render paths */
	if (picture->pDrawable->width == 1 &&
	    picture->pDrawable->height == 1 &&
	    picture->repeat)
		return FALSE;

	return is_cpu(picture->pDrawable);
}

static inline bool prefer_blt(struct sna *sna)
{
#if PREFER_BLT
	return true;
#else
	return sna->kgem.mode != KGEM_RENDER;
#endif
}

static Bool
try_blt(struct sna *sna,
	PicturePtr dst,
	PicturePtr source,
	int width, int height)
{
	if (prefer_blt(sna)) {
		DBG(("%s: already performing BLT\n", __FUNCTION__));
		return TRUE;
	}

	if (width > 8192 || height > 8192) {
		DBG(("%s: operation too large for 3D pipe (%d, %d)\n",
		     __FUNCTION__, width, height));
		return TRUE;
	}

	/* is the source picture only in cpu memory e.g. a shm pixmap? */
	return picture_is_cpu(source);
}

static Bool
gen4_render_composite(struct sna *sna,
		      uint8_t op,
		      PicturePtr src,
		      PicturePtr mask,
		      PicturePtr dst,
		      int16_t src_x, int16_t src_y,
		      int16_t msk_x, int16_t msk_y,
		      int16_t dst_x, int16_t dst_y,
		      int16_t width, int16_t height,
		      struct sna_composite_op *tmp)
{
	DBG(("%s: %dx%d, current mode=%d\n", __FUNCTION__,
	     width, height, sna->kgem.mode));

#if NO_COMPOSITE
	if (mask)
		return FALSE;

	return sna_blt_composite(sna, op,
				 src, dst,
				 src_x, src_y,
				 dst_x, dst_y,
				 width, height, tmp);
#endif

	if (mask == NULL &&
	    try_blt(sna, dst, src, width, height) &&
	    sna_blt_composite(sna, op,
			      src, dst,
			      src_x, src_y,
			      dst_x, dst_y,
			      width, height, tmp))
		return TRUE;

	if (op >= ARRAY_SIZE(gen4_blend_op))
		return FALSE;

	if (need_tiling(sna, width, height))
		return sna_tiling_composite(sna,
					    op, src, mask, dst,
					    src_x, src_y,
					    msk_x, msk_y,
					    dst_x, dst_y,
					    width, height,
					    tmp);

	if (!gen4_composite_set_target(sna, dst, tmp))
		return FALSE;

	if (tmp->dst.width > 8192 || tmp->dst.height > 8192) {
		if (!sna_render_composite_redirect(sna, tmp,
						   dst_x, dst_y, width, height))
			return FALSE;
	}

	switch (gen4_composite_picture(sna, src, &tmp->src,
				       src_x, src_y,
				       width, height,
				       dst_x, dst_y)) {
	case -1:
		DBG(("%s: failed to prepare source\n", __FUNCTION__));
		goto cleanup_dst;
	case 0:
		gen4_composite_solid_init(sna, &tmp->src, 0);
	case 1:
		gen4_composite_channel_convert(&tmp->src);
		break;
	}

	tmp->op = op;
	tmp->is_affine = tmp->src.is_affine;
	tmp->has_component_alpha = FALSE;
	tmp->need_magic_ca_pass = FALSE;

	tmp->prim_emit = gen4_emit_composite_primitive;
	if (mask) {
		if (mask->componentAlpha && PICT_FORMAT_RGB(mask->format)) {
			tmp->has_component_alpha = TRUE;

			/* Check if it's component alpha that relies on a source alpha and on
			 * the source value.  We can only get one of those into the single
			 * source value that we get to blend with.
			 */
			if (gen4_blend_op[op].src_alpha &&
			    (gen4_blend_op[op].src_blend != GEN4_BLENDFACTOR_ZERO)) {
				if (op != PictOpOver) {
					DBG(("%s -- fallback: unhandled component alpha blend\n",
					     __FUNCTION__));

					goto cleanup_src;
				}

				tmp->need_magic_ca_pass = TRUE;
				tmp->op = PictOpOutReverse;
			}
		}

		switch (gen4_composite_picture(sna, mask, &tmp->mask,
					       msk_x, msk_y,
					       width, height,
					       dst_x, dst_y)) {
		case -1:
			DBG(("%s: failed to prepare mask\n", __FUNCTION__));
			goto cleanup_src;
		case 0:
			gen4_composite_solid_init(sna, &tmp->mask, 0);
		case 1:
			gen4_composite_channel_convert(&tmp->mask);
			break;
		}

		tmp->is_affine &= tmp->mask.is_affine;

		if (tmp->src.transform == NULL && tmp->mask.transform == NULL)
			tmp->prim_emit = gen4_emit_composite_primitive_identity_source_mask;

		tmp->floats_per_vertex = 5 + 2 * !tmp->is_affine;
	} else {
		if (tmp->src.is_solid)
			tmp->prim_emit = gen4_emit_composite_primitive_solid;
		else if (tmp->src.transform == NULL)
			tmp->prim_emit = gen4_emit_composite_primitive_identity_source;
		else if (tmp->src.is_affine)
			tmp->prim_emit = gen4_emit_composite_primitive_affine_source;

		tmp->mask.filter = SAMPLER_FILTER_NEAREST;
		tmp->mask.repeat = SAMPLER_EXTEND_NONE;

		tmp->floats_per_vertex = 3 + !tmp->is_affine;
	}

	tmp->u.gen4.wm_kernel =
		gen4_choose_composite_kernel(tmp->op,
					     tmp->mask.bo != NULL,
					     tmp->has_component_alpha,
					     tmp->is_affine);
	tmp->u.gen4.ve_id = (tmp->mask.bo != NULL) << 1 | tmp->is_affine;

	tmp->blt   = gen4_render_composite_blt;
	tmp->boxes = gen4_render_composite_boxes;
	tmp->done  = gen4_render_composite_done;

	if (!kgem_check_bo(&sna->kgem, tmp->dst.bo))
		kgem_submit(&sna->kgem);
	if (!kgem_check_bo(&sna->kgem, tmp->src.bo))
		kgem_submit(&sna->kgem);
	if (!kgem_check_bo(&sna->kgem, tmp->mask.bo))
		kgem_submit(&sna->kgem);

	if (kgem_bo_is_dirty(tmp->src.bo) || kgem_bo_is_dirty(tmp->mask.bo))
		kgem_emit_flush(&sna->kgem);

	gen4_bind_surfaces(sna, tmp);
	gen4_align_vertex(sna, tmp);

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

static uint32_t gen4_get_dest_format_for_depth(int depth)
{
	switch (depth) {
	case 32:
	case 24:
	default: return GEN4_SURFACEFORMAT_B8G8R8A8_UNORM;
	case 30: return GEN4_SURFACEFORMAT_B10G10R10A2_UNORM;
	case 16: return GEN4_SURFACEFORMAT_B5G6R5_UNORM;
	case 8:  return GEN4_SURFACEFORMAT_A8_UNORM;
	}
}

static uint32_t gen4_get_card_format_for_depth(int depth)
{
	switch (depth) {
	case 32:
	default: return GEN4_SURFACEFORMAT_B8G8R8A8_UNORM;
	case 30: return GEN4_SURFACEFORMAT_B10G10R10A2_UNORM;
	case 24: return GEN4_SURFACEFORMAT_B8G8R8X8_UNORM;
	case 16: return GEN4_SURFACEFORMAT_B5G6R5_UNORM;
	case 8:  return GEN4_SURFACEFORMAT_A8_UNORM;
	}
}

static void
gen4_copy_bind_surfaces(struct sna *sna, const struct sna_composite_op *op)
{
	uint32_t *binding_table;
	uint16_t offset;

	gen4_get_batch(sna);

	binding_table = gen4_composite_get_binding_table(sna, op, &offset);

	binding_table[0] =
		gen4_bind_bo(sna,
			     op->dst.bo, op->dst.width, op->dst.height,
			     gen4_get_dest_format_for_depth(op->dst.pixmap->drawable.depth),
			     TRUE);
	binding_table[1] =
		gen4_bind_bo(sna,
			     op->src.bo, op->src.width, op->src.height,
			     op->src.card_format,
			     FALSE);

	if (sna->kgem.surface == offset &&
	    *(uint64_t *)(sna->kgem.batch + sna->render_state.gen4.surface_table) == *(uint64_t*)binding_table) {
		sna->kgem.surface += sizeof(struct gen4_surface_state_padded) / sizeof(uint32_t);
		offset = sna->render_state.gen4.surface_table;
	}

	gen4_emit_state(sna, op, offset);
}

static void
gen4_render_copy_one(struct sna *sna,
		     const struct sna_composite_op *op,
		     int sx, int sy,
		     int w, int h,
		     int dx, int dy)
{
	if (!gen4_get_rectangles(sna, op, 1)) {
		gen4_copy_bind_surfaces(sna, op);
		gen4_get_rectangles(sna, op, 1);
	}

	OUT_VERTEX(dx+w, dy+h);
	OUT_VERTEX_F((sx+w)*op->src.scale[0]);
	OUT_VERTEX_F((sy+h)*op->src.scale[1]);

	OUT_VERTEX(dx, dy+h);
	OUT_VERTEX_F(sx*op->src.scale[0]);
	OUT_VERTEX_F((sy+h)*op->src.scale[1]);

	OUT_VERTEX(dx, dy);
	OUT_VERTEX_F(sx*op->src.scale[0]);
	OUT_VERTEX_F(sy*op->src.scale[1]);

	FLUSH();
}

static Bool
gen4_render_copy_boxes(struct sna *sna, uint8_t alu,
		       PixmapPtr src, struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
		       PixmapPtr dst, struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
		       const BoxRec *box, int n)
{
	struct sna_composite_op tmp;

#if NO_COPY_BOXES
	return sna_blt_copy_boxes(sna, alu,
				  src_bo, src_dx, src_dy,
				  dst_bo, dst_dx, dst_dy,
				  dst->drawable.bitsPerPixel,
				  box, n);
#endif

	if (prefer_blt(sna) &&
	    sna_blt_copy_boxes(sna, alu,
			       src_bo, src_dx, src_dy,
			       dst_bo, dst_dx, dst_dy,
			       dst->drawable.bitsPerPixel,
			       box, n))
		return TRUE;

	if (!(alu == GXcopy || alu == GXclear) || src_bo == dst_bo ||
	    src->drawable.width > 8192 || src->drawable.height > 8192 ||
	    dst->drawable.width > 8192 || dst->drawable.height > 8192)
		return sna_blt_copy_boxes(sna, alu,
					  src_bo, src_dx, src_dy,
					  dst_bo, dst_dx, dst_dy,
					  dst->drawable.bitsPerPixel,
					  box, n);

	DBG(("%s (%d, %d)->(%d, %d) x %d\n",
	     __FUNCTION__, src_dx, src_dy, dst_dx, dst_dy, n));

	memset(&tmp, 0, sizeof(tmp));

	tmp.op = alu == GXcopy ? PictOpSrc : PictOpClear;

	tmp.dst.pixmap = dst;
	tmp.dst.width  = dst->drawable.width;
	tmp.dst.height = dst->drawable.height;
	tmp.dst.format = sna_format_for_depth(dst->drawable.depth);
	tmp.dst.bo = dst_bo;

	tmp.src.bo = src_bo;
	tmp.src.filter = SAMPLER_FILTER_NEAREST;
	tmp.src.repeat = SAMPLER_EXTEND_NONE;
	tmp.src.card_format =
		gen4_get_card_format_for_depth(src->drawable.depth),
	tmp.src.width  = src->drawable.width;
	tmp.src.height = src->drawable.height;

	tmp.is_affine = TRUE;
	tmp.floats_per_vertex = 3;
	tmp.u.gen4.wm_kernel = WM_KERNEL;
	tmp.u.gen4.ve_id = 1;

	if (!kgem_check_bo(&sna->kgem, dst_bo))
		kgem_submit(&sna->kgem);
	if (!kgem_check_bo(&sna->kgem, src_bo))
		kgem_submit(&sna->kgem);

	if (kgem_bo_is_dirty(src_bo))
		kgem_emit_flush(&sna->kgem);

	gen4_copy_bind_surfaces(sna, &tmp);
	gen4_align_vertex(sna, &tmp);

	tmp.src.scale[0] = 1. / src->drawable.width;
	tmp.src.scale[1] = 1. / src->drawable.height;
	do {
		gen4_render_copy_one(sna, &tmp,
				     box->x1 + src_dx, box->y1 + src_dy,
				     box->x2 - box->x1, box->y2 - box->y1,
				     box->x1 + dst_dx, box->y1 + dst_dy);
		box++;
	} while (--n);

	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
	return TRUE;
}

static void
gen4_render_copy_blt(struct sna *sna,
		     const struct sna_copy_op *op,
		     int16_t sx, int16_t sy,
		     int16_t w,  int16_t h,
		     int16_t dx, int16_t dy)
{
	gen4_render_copy_one(sna, &op->base, sx, sy, w, h, dx, dy);
}

static void
gen4_render_copy_done(struct sna *sna, const struct sna_copy_op *op)
{
	gen4_vertex_flush(sna);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
}

static Bool
gen4_render_copy(struct sna *sna, uint8_t alu,
		 PixmapPtr src, struct kgem_bo *src_bo,
		 PixmapPtr dst, struct kgem_bo *dst_bo,
		 struct sna_copy_op *op)
{
#if NO_COPY
	return sna_blt_copy(sna, alu,
			    src_bo, dst_bo,
			    dst->drawable.bitsPerPixel,
			    op);
#endif

	if (prefer_blt(sna) &&
	    sna_blt_copy(sna, alu,
			 src_bo, dst_bo,
			 dst->drawable.bitsPerPixel,
			 op))
		return TRUE;

	if (!(alu == GXcopy || alu == GXclear) || src_bo == dst_bo ||
	    src->drawable.width > 8192 || src->drawable.height > 8192 ||
	    dst->drawable.width > 8192 || dst->drawable.height > 8192)
		return sna_blt_copy(sna, alu, src_bo, dst_bo,
				    dst->drawable.bitsPerPixel,
				    op);

	op->base.op = alu == GXcopy ? PictOpSrc : PictOpClear;

	op->base.dst.pixmap = dst;
	op->base.dst.width  = dst->drawable.width;
	op->base.dst.height = dst->drawable.height;
	op->base.dst.format = sna_format_for_depth(dst->drawable.depth);
	op->base.dst.bo = dst_bo;

	op->base.src.bo = src_bo;
	op->base.src.card_format =
		gen4_get_card_format_for_depth(src->drawable.depth),
	op->base.src.width  = src->drawable.width;
	op->base.src.height = src->drawable.height;
	op->base.src.scale[0] = 1./src->drawable.width;
	op->base.src.scale[1] = 1./src->drawable.height;
	op->base.src.filter = SAMPLER_FILTER_NEAREST;
	op->base.src.repeat = SAMPLER_EXTEND_NONE;

	op->base.is_affine = true;
	op->base.floats_per_vertex = 3;
	op->base.u.gen4.wm_kernel = WM_KERNEL;
	op->base.u.gen4.ve_id = 1;

	if (!kgem_check_bo(&sna->kgem, dst_bo))
		kgem_submit(&sna->kgem);
	if (!kgem_check_bo(&sna->kgem, src_bo))
		kgem_submit(&sna->kgem);

	if (kgem_bo_is_dirty(src_bo))
		kgem_emit_flush(&sna->kgem);

	gen4_copy_bind_surfaces(sna, &op->base);
	gen4_align_vertex(sna, &op->base);

	op->blt  = gen4_render_copy_blt;
	op->done = gen4_render_copy_done;
	return TRUE;
}

static void
gen4_fill_bind_surfaces(struct sna *sna, const struct sna_composite_op *op)
{
	uint32_t *binding_table;
	uint16_t offset;

	gen4_get_batch(sna);

	binding_table = gen4_composite_get_binding_table(sna, op, &offset);

	binding_table[0] =
		gen4_bind_bo(sna,
			     op->dst.bo, op->dst.width, op->dst.height,
			     gen4_get_dest_format_for_depth(op->dst.pixmap->drawable.depth),
			     TRUE);
	binding_table[1] =
		gen4_bind_bo(sna,
			     op->src.bo, 1, 1,
			     GEN4_SURFACEFORMAT_B8G8R8A8_UNORM,
			     FALSE);

	if (sna->kgem.surface == offset &&
	    *(uint64_t *)(sna->kgem.batch + sna->render_state.gen4.surface_table) == *(uint64_t*)binding_table) {
		sna->kgem.surface +=
			sizeof(struct gen4_surface_state_padded)/sizeof(uint32_t);
		offset = sna->render_state.gen4.surface_table;
	}

	gen4_emit_state(sna, op, offset);
}

static void
gen4_render_fill_one(struct sna *sna,
		     const struct sna_composite_op *op,
		     int x, int y, int w, int h)
{
	if (!gen4_get_rectangles(sna, op, 1)) {
		gen4_fill_bind_surfaces(sna, op);
		gen4_get_rectangles(sna, op, 1);
	}

	OUT_VERTEX(x+w, y+h);
	OUT_VERTEX_F(1);
	OUT_VERTEX_F(1);

	OUT_VERTEX(x, y+h);
	OUT_VERTEX_F(0);
	OUT_VERTEX_F(1);

	OUT_VERTEX(x, y);
	OUT_VERTEX_F(0);
	OUT_VERTEX_F(0);

	FLUSH();
}

static Bool
gen4_render_fill_boxes(struct sna *sna,
		       CARD8 op,
		       PictFormat format,
		       const xRenderColor *color,
		       PixmapPtr dst, struct kgem_bo *dst_bo,
		       const BoxRec *box, int n)
{
	struct sna_composite_op tmp;
	uint32_t pixel;

	if (op >= ARRAY_SIZE(gen4_blend_op)) {
		DBG(("%s: fallback due to unhandled blend op: %d\n",
		     __FUNCTION__, op));
		return FALSE;
	}

	if (prefer_blt(sna) ||
	    dst->drawable.width > 8192 ||
	    dst->drawable.height > 8192 ||
	    !gen4_check_dst_format(format)) {
		uint8_t alu = GXcopy;

		if (op == PictOpClear) {
			alu = GXclear;
			pixel = 0;
			op = PictOpSrc;
		}

		if (op == PictOpOver && color->alpha >= 0xff00)
			op = PictOpSrc;

		if (op == PictOpSrc &&
		    sna_get_pixel_from_rgba(&pixel,
					    color->red,
					    color->green,
					    color->blue,
					    color->alpha,
					    format) &&
		    sna_blt_fill_boxes(sna, alu,
				       dst_bo, dst->drawable.bitsPerPixel,
				       pixel, box, n))
			return TRUE;

		if (dst->drawable.width > 8192 ||
		    dst->drawable.height > 8192 ||
		    !gen4_check_dst_format(format))
			return FALSE;
	}

#if NO_FILL_BOXES
	return FALSE;
#endif

	if (!sna_get_pixel_from_rgba(&pixel,
				     color->red,
				     color->green,
				     color->blue,
				     color->alpha,
				     PICT_a8r8g8b8))
		return FALSE;

	DBG(("%s(%08x x %d)\n", __FUNCTION__, pixel, n));

	memset(&tmp, 0, sizeof(tmp));

	tmp.op = op;

	tmp.dst.pixmap = dst;
	tmp.dst.width  = dst->drawable.width;
	tmp.dst.height = dst->drawable.height;
	tmp.dst.format = format;
	tmp.dst.bo = dst_bo;

	tmp.src.bo = sna_render_get_solid(sna, pixel);
	tmp.src.filter = SAMPLER_FILTER_NEAREST;
	tmp.src.repeat = SAMPLER_EXTEND_REPEAT;

	tmp.is_affine = TRUE;
	tmp.floats_per_vertex = 3;
	tmp.u.gen4.wm_kernel = WM_KERNEL;
	tmp.u.gen4.ve_id = 1;

	if (!kgem_check_bo(&sna->kgem, dst_bo))
		kgem_submit(&sna->kgem);

	gen4_fill_bind_surfaces(sna, &tmp);
	gen4_align_vertex(sna, &tmp);

	do {
		gen4_render_fill_one(sna, &tmp,
				     box->x1, box->y1,
				     box->x2 - box->x1, box->y2 - box->y1);
		box++;
	} while (--n);

	kgem_bo_destroy(&sna->kgem, tmp.src.bo);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
	return TRUE;
}

static void
gen4_render_fill_blt(struct sna *sna, const struct sna_fill_op *op,
		     int16_t x, int16_t y, int16_t w, int16_t h)
{
	gen4_render_fill_one(sna, &op->base, x, y, w, h);
}

static void
gen4_render_fill_done(struct sna *sna, const struct sna_fill_op *op)
{
	gen4_vertex_flush(sna);
	kgem_bo_destroy(&sna->kgem, op->base.src.bo);
	_kgem_set_mode(&sna->kgem, KGEM_RENDER);
}

static Bool
gen4_render_fill(struct sna *sna, uint8_t alu,
		 PixmapPtr dst, struct kgem_bo *dst_bo,
		 uint32_t color,
		 struct sna_fill_op *op)
{
#if NO_FILL
	return sna_blt_fill(sna, alu,
			    dst_bo, dst->drawable.bitsPerPixel,
			    color,
			    op);
#endif

	if (prefer_blt(sna) &&
	    sna_blt_fill(sna, alu,
			 dst_bo, dst->drawable.bitsPerPixel,
			 color,
			 op))
		return TRUE;

	if (!(alu == GXcopy || alu == GXclear) ||
	    dst->drawable.width > 8192 || dst->drawable.height > 8192)
		return sna_blt_fill(sna, alu,
				    dst_bo, dst->drawable.bitsPerPixel,
				    color,
				    op);

	if (alu == GXclear)
		color = 0;

	op->base.op = color == 0 ? PictOpClear : PictOpSrc;

	op->base.dst.pixmap = dst;
	op->base.dst.width  = dst->drawable.width;
	op->base.dst.height = dst->drawable.height;
	op->base.dst.format = sna_format_for_depth(dst->drawable.depth);
	op->base.dst.bo = dst_bo;

	op->base.src.bo =
		sna_render_get_solid(sna,
				     sna_rgba_for_color(color,
							dst->drawable.depth));
	op->base.src.filter = SAMPLER_FILTER_NEAREST;
	op->base.src.repeat = SAMPLER_EXTEND_REPEAT;

	op->base.is_affine = TRUE;
	op->base.floats_per_vertex = 3;
	op->base.u.gen4.wm_kernel = WM_KERNEL;
	op->base.u.gen4.ve_id = 1;

	if (!kgem_check_bo(&sna->kgem, dst_bo))
		kgem_submit(&sna->kgem);

	gen4_fill_bind_surfaces(sna, &op->base);
	gen4_align_vertex(sna, &op->base);

	op->blt  = gen4_render_fill_blt;
	op->done = gen4_render_fill_done;
	return TRUE;
}

static void
gen4_render_flush(struct sna *sna)
{
	gen4_vertex_finish(sna, TRUE);
}

static void gen4_render_reset(struct sna *sna)
{
	sna->render_state.gen4.needs_invariant = TRUE;
	sna->render_state.gen4.needs_urb = TRUE;
	sna->render_state.gen4.vb_id = 0;
	sna->render_state.gen4.ve_id = -1;
	sna->render_state.gen4.last_primitive = -1;
	sna->render_state.gen4.last_pipelined_pointers = 0;

	sna->render_state.gen4.drawrect_offset = -1;
	sna->render_state.gen4.drawrect_limit = -1;
	sna->render_state.gen4.surface_table = -1;
}

static void gen4_render_fini(struct sna *sna)
{
	kgem_bo_destroy(&sna->kgem, sna->render_state.gen4.general_bo);
}

static uint32_t gen4_create_vs_unit_state(struct sna_static_stream *stream)
{
	struct gen4_vs_unit_state *vs = sna_static_stream_map(stream, sizeof(*vs), 32);

	/* Set up the vertex shader to be disabled (passthrough) */
	vs->thread4.nr_urb_entries = URB_VS_ENTRIES;
	vs->thread4.urb_entry_allocation_size = URB_VS_ENTRY_SIZE - 1;
	vs->vs6.vs_enable = 0;
	vs->vs6.vert_cache_disable = 1;

	return sna_static_stream_offsetof(stream, vs);
}

static uint32_t gen4_create_sf_state(struct sna_static_stream *stream,
				     uint32_t kernel)
{
	struct gen4_sf_unit_state *sf_state;

	sf_state = sna_static_stream_map(stream, sizeof(*sf_state), 32);

	sf_state->thread0.grf_reg_count = GEN4_GRF_BLOCKS(SF_KERNEL_NUM_GRF);
	sf_state->thread0.kernel_start_pointer = kernel >> 6;
	sf_state->sf1.single_program_flow = 1;
	/* scratch space is not used in our kernel */
	sf_state->thread2.scratch_space_base_pointer = 0;
	sf_state->thread3.const_urb_entry_read_length = 0;	/* no const URBs */
	sf_state->thread3.const_urb_entry_read_offset = 0;	/* no const URBs */
	sf_state->thread3.urb_entry_read_length = 1;	/* 1 URB per vertex */
	/* don't smash vertex header, read start from dw8 */
	sf_state->thread3.urb_entry_read_offset = 1;
	sf_state->thread3.dispatch_grf_start_reg = 3;
	sf_state->thread4.max_threads = SF_MAX_THREADS - 1;
	sf_state->thread4.urb_entry_allocation_size = URB_SF_ENTRY_SIZE - 1;
	sf_state->thread4.nr_urb_entries = URB_SF_ENTRIES;
	sf_state->thread4.stats_enable = 1;
	sf_state->sf5.viewport_transform = FALSE;	/* skip viewport */
	sf_state->sf6.cull_mode = GEN4_CULLMODE_NONE;
	sf_state->sf6.scissor = 0;
	sf_state->sf7.trifan_pv = 2;
	sf_state->sf6.dest_org_vbias = 0x8;
	sf_state->sf6.dest_org_hbias = 0x8;

	return sna_static_stream_offsetof(stream, sf_state);
}

static uint32_t gen4_create_sampler_state(struct sna_static_stream *stream,
					  sampler_filter_t src_filter,
					  sampler_extend_t src_extend,
					  sampler_filter_t mask_filter,
					  sampler_extend_t mask_extend)
{
	struct gen4_sampler_state *sampler_state;

	sampler_state = sna_static_stream_map(stream,
					      sizeof(struct gen4_sampler_state) * 2,
					      32);
	sampler_state_init(&sampler_state[0], src_filter, src_extend);
	sampler_state_init(&sampler_state[1], mask_filter, mask_extend);

	return sna_static_stream_offsetof(stream, sampler_state);
}

static void gen4_init_wm_state(struct gen4_wm_unit_state *state,
			       Bool has_mask,
			       uint32_t kernel,
			       uint32_t sampler)
{
	state->thread0.grf_reg_count = GEN4_GRF_BLOCKS(PS_KERNEL_NUM_GRF);
	state->thread0.kernel_start_pointer = kernel >> 6;

	state->thread1.single_program_flow = 0;

	/* scratch space is not used in our kernel */
	state->thread2.scratch_space_base_pointer = 0;
	state->thread2.per_thread_scratch_space = 0;

	state->thread3.const_urb_entry_read_length = 0;
	state->thread3.const_urb_entry_read_offset = 0;

	state->thread3.urb_entry_read_offset = 0;
	/* wm kernel use urb from 3, see wm_program in compiler module */
	state->thread3.dispatch_grf_start_reg = 3;	/* must match kernel */

	state->wm4.sampler_count = 1;	/* 1-4 samplers */

	state->wm4.sampler_state_pointer = sampler >> 5;
	state->wm5.max_threads = PS_MAX_THREADS - 1;
	state->wm5.transposed_urb_read = 0;
	state->wm5.thread_dispatch_enable = 1;
	/* just use 16-pixel dispatch (4 subspans), don't need to change kernel
	 * start point
	 */
	state->wm5.enable_16_pix = 1;
	state->wm5.enable_8_pix = 0;
	state->wm5.early_depth_test = 1;

	/* Each pair of attributes (src/mask coords) is two URB entries */
	if (has_mask) {
		state->thread1.binding_table_entry_count = 3;	/* 2 tex and fb */
		state->thread3.urb_entry_read_length = 4;
	} else {
		state->thread1.binding_table_entry_count = 2;	/* 1 tex and fb */
		state->thread3.urb_entry_read_length = 2;
	}
}

static uint32_t gen4_create_cc_viewport(struct sna_static_stream *stream)
{
	struct gen4_cc_viewport vp;

	vp.min_depth = -1.e35;
	vp.max_depth = 1.e35;

	return sna_static_stream_add(stream, &vp, sizeof(vp), 32);
}

static uint32_t gen4_create_cc_unit_state(struct sna_static_stream *stream)
{
	uint8_t *ptr, *base;
	uint32_t vp;
	int i, j;

	vp = gen4_create_cc_viewport(stream);
	base = ptr =
		sna_static_stream_map(stream,
				      GEN4_BLENDFACTOR_COUNT*GEN4_BLENDFACTOR_COUNT*64,
				      64);

	for (i = 0; i < GEN4_BLENDFACTOR_COUNT; i++) {
		for (j = 0; j < GEN4_BLENDFACTOR_COUNT; j++) {
			struct gen4_cc_unit_state *state =
				(struct gen4_cc_unit_state *)ptr;

			state->cc3.blend_enable = 1;	/* enable color blend */
			state->cc4.cc_viewport_state_offset = vp >> 5;

			state->cc5.logicop_func = 0xc;	/* COPY */
			state->cc5.ia_blend_function = GEN4_BLENDFUNCTION_ADD;

			/* Fill in alpha blend factors same as color, for the future. */
			state->cc5.ia_src_blend_factor = i;
			state->cc5.ia_dest_blend_factor = j;

			state->cc6.blend_function = GEN4_BLENDFUNCTION_ADD;
			state->cc6.clamp_post_alpha_blend = 1;
			state->cc6.clamp_pre_alpha_blend = 1;
			state->cc6.src_blend_factor = i;
			state->cc6.dest_blend_factor = j;

			ptr += 64;
		}
	}

	return sna_static_stream_offsetof(stream, base);
}

static Bool gen4_render_setup(struct sna *sna)
{
	struct gen4_render_state *state = &sna->render_state.gen4;
	struct sna_static_stream general;
	struct gen4_wm_unit_state_padded *wm_state;
	uint32_t sf[2], wm[KERNEL_COUNT];
	int i, j, k, l, m;

	sna_static_stream_init(&general);

	/* Zero pad the start. If you see an offset of 0x0 in the batchbuffer
	 * dumps, you know it points to zero.
	 */
	null_create(&general);

	/* Set up the two SF states (one for blending with a mask, one without) */
	sf[0] = sna_static_stream_add(&general,
				      sf_kernel,
				      sizeof(sf_kernel),
				      64);
	sf[1] = sna_static_stream_add(&general,
				      sf_kernel_mask,
				      sizeof(sf_kernel_mask),
				      64);
	for (m = 0; m < KERNEL_COUNT; m++) {
		wm[m] = sna_static_stream_add(&general,
					      wm_kernels[m].data,
					      wm_kernels[m].size,
					      64);
	}

	state->vs = gen4_create_vs_unit_state(&general);

	state->sf[0] = gen4_create_sf_state(&general, sf[0]);
	state->sf[1] = gen4_create_sf_state(&general, sf[1]);


	/* Set up the WM states: each filter/extend type for source and mask, per
	 * kernel.
	 */
	wm_state = sna_static_stream_map(&general,
					  sizeof(*wm_state) * KERNEL_COUNT *
					  FILTER_COUNT * EXTEND_COUNT *
					  FILTER_COUNT * EXTEND_COUNT,
					  64);
	state->wm = sna_static_stream_offsetof(&general, wm_state);
	for (i = 0; i < FILTER_COUNT; i++) {
		for (j = 0; j < EXTEND_COUNT; j++) {
			for (k = 0; k < FILTER_COUNT; k++) {
				for (l = 0; l < EXTEND_COUNT; l++) {
					uint32_t sampler_state;

					sampler_state =
						gen4_create_sampler_state(&general,
									  i, j,
									  k, l);

					for (m = 0; m < KERNEL_COUNT; m++) {
						gen4_init_wm_state(&wm_state->state,
								   wm_kernels[m].has_mask,
								   wm[m],
								   sampler_state);
						wm_state++;
					}
				}
			}
		}
	}

	state->cc = gen4_create_cc_unit_state(&general);

	state->general_bo = sna_static_stream_fini(sna, &general);
	return state->general_bo != NULL;
}

Bool gen4_render_init(struct sna *sna)
{
	if (!gen4_render_setup(sna))
		return FALSE;

	gen4_render_reset(sna);

	sna->render.composite = gen4_render_composite;
	sna->render.video = gen4_render_video;

	sna->render.copy_boxes = gen4_render_copy_boxes;
	sna->render.copy = gen4_render_copy;

	sna->render.fill_boxes = gen4_render_fill_boxes;
	sna->render.fill = gen4_render_fill;

	sna->render.flush = gen4_render_flush;
	sna->render.reset = gen4_render_reset;
	sna->render.fini = gen4_render_fini;

	sna->render.max_3d_size = 8192;
	return TRUE;
}
