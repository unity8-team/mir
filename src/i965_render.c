/*
 * Copyright © 2006,2008 Intel Corporation
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
 *    Wang Zhenyu <zhenyu.z.wang@intel.com>
 *    Eric Anholt <eric@anholt.net>
 *    Carl Worth <cworth@redhat.com>
 *    Keith Packard <keithp@keithp.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include "xf86.h"
#include "intel.h"
#include "i830_reg.h"
#include "i965_reg.h"

/* bring in brw structs */
#include "brw_defines.h"
#include "brw_structs.h"

// refer vol2, 3d rasterization 3.8.1

/* defined in brw_defines.h */
static const struct blendinfo {
	Bool dst_alpha;
	Bool src_alpha;
	uint32_t src_blend;
	uint32_t dst_blend;
} i965_blend_op[] = {
	/* Clear */
	{0, 0, BRW_BLENDFACTOR_ZERO, BRW_BLENDFACTOR_ZERO},
	/* Src */
	{0, 0, BRW_BLENDFACTOR_ONE, BRW_BLENDFACTOR_ZERO},
	/* Dst */
	{0, 0, BRW_BLENDFACTOR_ZERO, BRW_BLENDFACTOR_ONE},
	/* Over */
	{0, 1, BRW_BLENDFACTOR_ONE, BRW_BLENDFACTOR_INV_SRC_ALPHA},
	/* OverReverse */
	{1, 0, BRW_BLENDFACTOR_INV_DST_ALPHA, BRW_BLENDFACTOR_ONE},
	/* In */
	{1, 0, BRW_BLENDFACTOR_DST_ALPHA, BRW_BLENDFACTOR_ZERO},
	/* InReverse */
	{0, 1, BRW_BLENDFACTOR_ZERO, BRW_BLENDFACTOR_SRC_ALPHA},
	/* Out */
	{1, 0, BRW_BLENDFACTOR_INV_DST_ALPHA, BRW_BLENDFACTOR_ZERO},
	/* OutReverse */
	{0, 1, BRW_BLENDFACTOR_ZERO, BRW_BLENDFACTOR_INV_SRC_ALPHA},
	/* Atop */
	{1, 1, BRW_BLENDFACTOR_DST_ALPHA, BRW_BLENDFACTOR_INV_SRC_ALPHA},
	/* AtopReverse */
	{1, 1, BRW_BLENDFACTOR_INV_DST_ALPHA, BRW_BLENDFACTOR_SRC_ALPHA},
	/* Xor */
	{1, 1, BRW_BLENDFACTOR_INV_DST_ALPHA, BRW_BLENDFACTOR_INV_SRC_ALPHA},
	/* Add */
	{0, 0, BRW_BLENDFACTOR_ONE, BRW_BLENDFACTOR_ONE},
};

/**
 * Highest-valued BLENDFACTOR used in i965_blend_op.
 *
 * This leaves out BRW_BLENDFACTOR_INV_DST_COLOR,
 * BRW_BLENDFACTOR_INV_CONST_{COLOR,ALPHA},
 * BRW_BLENDFACTOR_INV_SRC1_{COLOR,ALPHA}
 */
#define BRW_BLENDFACTOR_COUNT (BRW_BLENDFACTOR_INV_DST_ALPHA + 1)

/* FIXME: surface format defined in brw_defines.h, shared Sampling engine
 * 1.7.2
 */
static const struct formatinfo {
	int fmt;
	uint32_t card_fmt;
} i965_tex_formats[] = {
	{PICT_a8, BRW_SURFACEFORMAT_A8_UNORM},
	{PICT_a8r8g8b8, BRW_SURFACEFORMAT_B8G8R8A8_UNORM},
	{PICT_x8r8g8b8, BRW_SURFACEFORMAT_B8G8R8X8_UNORM},
	{PICT_a8b8g8r8, BRW_SURFACEFORMAT_R8G8B8A8_UNORM},
	{PICT_x8b8g8r8, BRW_SURFACEFORMAT_R8G8B8X8_UNORM},
	{PICT_r8g8b8, BRW_SURFACEFORMAT_R8G8B8_UNORM},
	{PICT_r5g6b5, BRW_SURFACEFORMAT_B5G6R5_UNORM},
	{PICT_a1r5g5b5, BRW_SURFACEFORMAT_B5G5R5A1_UNORM},
#if XORG_VERSION_CURRENT >= 10699900
	{PICT_a2r10g10b10, BRW_SURFACEFORMAT_B10G10R10A2_UNORM},
	{PICT_x2r10g10b10, BRW_SURFACEFORMAT_B10G10R10X2_UNORM},
	{PICT_a2b10g10r10, BRW_SURFACEFORMAT_R10G10B10A2_UNORM},
	{PICT_x2r10g10b10, BRW_SURFACEFORMAT_B10G10R10X2_UNORM},
#endif
	{PICT_a4r4g4b4, BRW_SURFACEFORMAT_B4G4R4A4_UNORM},
};

static void i965_get_blend_cntl(int op, PicturePtr mask, uint32_t dst_format,
				uint32_t * sblend, uint32_t * dblend)
{

	*sblend = i965_blend_op[op].src_blend;
	*dblend = i965_blend_op[op].dst_blend;

	/* If there's no dst alpha channel, adjust the blend op so that we'll treat
	 * it as always 1.
	 */
	if (PICT_FORMAT_A(dst_format) == 0 && i965_blend_op[op].dst_alpha) {
		if (*sblend == BRW_BLENDFACTOR_DST_ALPHA)
			*sblend = BRW_BLENDFACTOR_ONE;
		else if (*sblend == BRW_BLENDFACTOR_INV_DST_ALPHA)
			*sblend = BRW_BLENDFACTOR_ZERO;
	}

	/* If the source alpha is being used, then we should only be in a case where
	 * the source blend factor is 0, and the source blend value is the mask
	 * channels multiplied by the source picture's alpha.
	 */
	if (mask && mask->componentAlpha && PICT_FORMAT_RGB(mask->format)
	    && i965_blend_op[op].src_alpha) {
		if (*dblend == BRW_BLENDFACTOR_SRC_ALPHA) {
			*dblend = BRW_BLENDFACTOR_SRC_COLOR;
		} else if (*dblend == BRW_BLENDFACTOR_INV_SRC_ALPHA) {
			*dblend = BRW_BLENDFACTOR_INV_SRC_COLOR;
		}
	}

}

static uint32_t i965_get_dest_format(PicturePtr dest_picture)
{

	switch (dest_picture->format) {
	case PICT_a8r8g8b8:
	case PICT_x8r8g8b8:
		return BRW_SURFACEFORMAT_B8G8R8A8_UNORM;
	case PICT_a8b8g8r8:
	case PICT_x8b8g8r8:
		return BRW_SURFACEFORMAT_R8G8B8A8_UNORM;
#if XORG_VERSION_CURRENT >= 10699900
	case PICT_a2r10g10b10:
	case PICT_x2r10g10b10:
		return BRW_SURFACEFORMAT_B10G10R10A2_UNORM;
#endif
	case PICT_r5g6b5:
		return BRW_SURFACEFORMAT_B5G6R5_UNORM;
	case PICT_x1r5g5b5:
	case PICT_a1r5g5b5:
		return BRW_SURFACEFORMAT_B5G5R5A1_UNORM;
	case PICT_a8:
		return BRW_SURFACEFORMAT_A8_UNORM;
	case PICT_a4r4g4b4:
	case PICT_x4r4g4b4:
		return BRW_SURFACEFORMAT_B4G4R4A4_UNORM;
	default:
		return 0;
	}
}

Bool
i965_check_composite(int op,
		     PicturePtr source_picture,
		     PicturePtr mask_picture,
		     PicturePtr dest_picture,
		     int width, int height)
{
	ScrnInfoPtr scrn = xf86Screens[dest_picture->pDrawable->pScreen->myNum];

	/* Check for unsupported compositing operations. */
	if (op >= sizeof(i965_blend_op) / sizeof(i965_blend_op[0])) {
		intel_debug_fallback(scrn,
				     "Unsupported Composite op 0x%x\n", op);
		return FALSE;
	}

	if (mask_picture && mask_picture->componentAlpha &&
	    PICT_FORMAT_RGB(mask_picture->format)) {
		/* Check if it's component alpha that relies on a source alpha and on
		 * the source value.  We can only get one of those into the single
		 * source value that we get to blend with.
		 */
		if (i965_blend_op[op].src_alpha &&
		    (i965_blend_op[op].src_blend != BRW_BLENDFACTOR_ZERO)) {
			intel_debug_fallback(scrn,
					     "Component alpha not supported "
					     "with source alpha and source "
					     "value blending.\n");
			return FALSE;
		}
	}

	if (!i965_get_dest_format(dest_picture)) {
		intel_debug_fallback(scrn, "Usupported Color buffer format 0x%x\n",
				     (int)dest_picture->format);
		return FALSE;
	}

	return TRUE;
}

Bool
i965_check_composite_texture(ScreenPtr screen, PicturePtr picture)
{
	if (picture->repeatType > RepeatReflect) {
		ScrnInfoPtr scrn = xf86Screens[screen->myNum];
		intel_debug_fallback(scrn,
				     "extended repeat (%d) not supported\n",
				     picture->repeatType);
		return FALSE;
	}

	if (picture->filter != PictFilterNearest &&
	    picture->filter != PictFilterBilinear) {
		ScrnInfoPtr scrn = xf86Screens[screen->myNum];
		intel_debug_fallback(scrn, "Unsupported filter 0x%x\n",
				     picture->filter);
		return FALSE;
	}

	if (picture->pDrawable) {
		int w, h, i;

		w = picture->pDrawable->width;
		h = picture->pDrawable->height;
		if ((w > 8192) || (h > 8192)) {
			ScrnInfoPtr scrn = xf86Screens[screen->myNum];
			intel_debug_fallback(scrn,
					     "Picture w/h too large (%dx%d)\n",
					     w, h);
			return FALSE;
		}

		for (i = 0;
		     i < sizeof(i965_tex_formats) / sizeof(i965_tex_formats[0]);
		     i++) {
			if (i965_tex_formats[i].fmt == picture->format)
				break;
		}
		if (i == sizeof(i965_tex_formats) / sizeof(i965_tex_formats[0]))
		{
			ScrnInfoPtr scrn = xf86Screens[screen->myNum];
			intel_debug_fallback(scrn,
					     "Unsupported picture format "
					     "0x%x\n",
					     (int)picture->format);
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}


#define BRW_GRF_BLOCKS(nreg)    ((nreg + 15) / 16 - 1)

/* Set up a default static partitioning of the URB, which is supposed to
 * allow anything we would want to do, at potentially lower performance.
 */
#define URB_CS_ENTRY_SIZE     0
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

static const uint32_t sf_kernel_static[][4] = {
#include "exa_sf.g4b"
};

static const uint32_t sf_kernel_mask_static[][4] = {
#include "exa_sf_mask.g4b"
};

/* ps kernels */
#define PS_KERNEL_NUM_GRF   32
#define PS_MAX_THREADS	    48

static const uint32_t ps_kernel_nomask_affine_static[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_affine.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_nomask_projective_static[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_projective.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_maskca_affine_static[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_affine.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_mask_affine.g4b"
#include "exa_wm_mask_sample_argb.g4b"
#include "exa_wm_ca.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_maskca_projective_static[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_projective.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_mask_projective.g4b"
#include "exa_wm_mask_sample_argb.g4b"
#include "exa_wm_ca.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_maskca_srcalpha_affine_static[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_affine.g4b"
#include "exa_wm_src_sample_a.g4b"
#include "exa_wm_mask_affine.g4b"
#include "exa_wm_mask_sample_argb.g4b"
#include "exa_wm_ca_srcalpha.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_maskca_srcalpha_projective_static[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_projective.g4b"
#include "exa_wm_src_sample_a.g4b"
#include "exa_wm_mask_projective.g4b"
#include "exa_wm_mask_sample_argb.g4b"
#include "exa_wm_ca_srcalpha.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_masknoca_affine_static[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_affine.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_mask_affine.g4b"
#include "exa_wm_mask_sample_a.g4b"
#include "exa_wm_noca.g4b"
#include "exa_wm_write.g4b"
};

static const uint32_t ps_kernel_masknoca_projective_static[][4] = {
#include "exa_wm_xy.g4b"
#include "exa_wm_src_projective.g4b"
#include "exa_wm_src_sample_argb.g4b"
#include "exa_wm_mask_projective.g4b"
#include "exa_wm_mask_sample_a.g4b"
#include "exa_wm_noca.g4b"
#include "exa_wm_write.g4b"
};

/* new programs for Ironlake */
static const uint32_t sf_kernel_static_gen5[][4] = {
#include "exa_sf.g4b.gen5"
};

static const uint32_t sf_kernel_mask_static_gen5[][4] = {
#include "exa_sf_mask.g4b.gen5"
};

static const uint32_t ps_kernel_nomask_affine_static_gen5[][4] = {
#include "exa_wm_xy.g4b.gen5"
#include "exa_wm_src_affine.g4b.gen5"
#include "exa_wm_src_sample_argb.g4b.gen5"
#include "exa_wm_write.g4b.gen5"
};

static const uint32_t ps_kernel_nomask_projective_static_gen5[][4] = {
#include "exa_wm_xy.g4b.gen5"
#include "exa_wm_src_projective.g4b.gen5"
#include "exa_wm_src_sample_argb.g4b.gen5"
#include "exa_wm_write.g4b.gen5"
};

static const uint32_t ps_kernel_maskca_affine_static_gen5[][4] = {
#include "exa_wm_xy.g4b.gen5"
#include "exa_wm_src_affine.g4b.gen5"
#include "exa_wm_src_sample_argb.g4b.gen5"
#include "exa_wm_mask_affine.g4b.gen5"
#include "exa_wm_mask_sample_argb.g4b.gen5"
#include "exa_wm_ca.g4b.gen5"
#include "exa_wm_write.g4b.gen5"
};

static const uint32_t ps_kernel_maskca_projective_static_gen5[][4] = {
#include "exa_wm_xy.g4b.gen5"
#include "exa_wm_src_projective.g4b.gen5"
#include "exa_wm_src_sample_argb.g4b.gen5"
#include "exa_wm_mask_projective.g4b.gen5"
#include "exa_wm_mask_sample_argb.g4b.gen5"
#include "exa_wm_ca.g4b.gen5"
#include "exa_wm_write.g4b.gen5"
};

static const uint32_t ps_kernel_maskca_srcalpha_affine_static_gen5[][4] = {
#include "exa_wm_xy.g4b.gen5"
#include "exa_wm_src_affine.g4b.gen5"
#include "exa_wm_src_sample_a.g4b.gen5"
#include "exa_wm_mask_affine.g4b.gen5"
#include "exa_wm_mask_sample_argb.g4b.gen5"
#include "exa_wm_ca_srcalpha.g4b.gen5"
#include "exa_wm_write.g4b.gen5"
};

static const uint32_t ps_kernel_maskca_srcalpha_projective_static_gen5[][4] = {
#include "exa_wm_xy.g4b.gen5"
#include "exa_wm_src_projective.g4b.gen5"
#include "exa_wm_src_sample_a.g4b.gen5"
#include "exa_wm_mask_projective.g4b.gen5"
#include "exa_wm_mask_sample_argb.g4b.gen5"
#include "exa_wm_ca_srcalpha.g4b.gen5"
#include "exa_wm_write.g4b.gen5"
};

static const uint32_t ps_kernel_masknoca_affine_static_gen5[][4] = {
#include "exa_wm_xy.g4b.gen5"
#include "exa_wm_src_affine.g4b.gen5"
#include "exa_wm_src_sample_argb.g4b.gen5"
#include "exa_wm_mask_affine.g4b.gen5"
#include "exa_wm_mask_sample_a.g4b.gen5"
#include "exa_wm_noca.g4b.gen5"
#include "exa_wm_write.g4b.gen5"
};

static const uint32_t ps_kernel_masknoca_projective_static_gen5[][4] = {
#include "exa_wm_xy.g4b.gen5"
#include "exa_wm_src_projective.g4b.gen5"
#include "exa_wm_src_sample_argb.g4b.gen5"
#include "exa_wm_mask_projective.g4b.gen5"
#include "exa_wm_mask_sample_a.g4b.gen5"
#include "exa_wm_noca.g4b.gen5"
#include "exa_wm_write.g4b.gen5"
};

/* programs for GEN6 */
static const uint32_t ps_kernel_nomask_affine_static_gen6[][4] = {
#include "exa_wm_src_affine.g6b"
#include "exa_wm_src_sample_argb.g6b"
#include "exa_wm_write.g6b"
};

static const uint32_t ps_kernel_nomask_projective_static_gen6[][4] = {
#include "exa_wm_src_projective.g6b"
#include "exa_wm_src_sample_argb.g6b"
#include "exa_wm_write.g6b"
};

static const uint32_t ps_kernel_maskca_affine_static_gen6[][4] = {
#include "exa_wm_src_affine.g6b"
#include "exa_wm_src_sample_argb.g6b"
#include "exa_wm_mask_affine.g6b"
#include "exa_wm_mask_sample_argb.g6b"
#include "exa_wm_ca.g6b"
#include "exa_wm_write.g6b"
};

static const uint32_t ps_kernel_maskca_projective_static_gen6[][4] = {
#include "exa_wm_src_projective.g6b"
#include "exa_wm_src_sample_argb.g6b"
#include "exa_wm_mask_projective.g6b"
#include "exa_wm_mask_sample_argb.g6b"
#include "exa_wm_ca.g4b.gen5"
#include "exa_wm_write.g6b"
};

static const uint32_t ps_kernel_maskca_srcalpha_affine_static_gen6[][4] = {
#include "exa_wm_src_affine.g6b"
#include "exa_wm_src_sample_a.g6b"
#include "exa_wm_mask_affine.g6b"
#include "exa_wm_mask_sample_argb.g6b"
#include "exa_wm_ca_srcalpha.g6b"
#include "exa_wm_write.g6b"
};

static const uint32_t ps_kernel_maskca_srcalpha_projective_static_gen6[][4] = {
#include "exa_wm_src_projective.g6b"
#include "exa_wm_src_sample_a.g6b"
#include "exa_wm_mask_projective.g6b"
#include "exa_wm_mask_sample_argb.g6b"
#include "exa_wm_ca_srcalpha.g6b"
#include "exa_wm_write.g6b"
};

static const uint32_t ps_kernel_masknoca_affine_static_gen6[][4] = {
#include "exa_wm_src_affine.g6b"
#include "exa_wm_src_sample_argb.g6b"
#include "exa_wm_mask_affine.g6b"
#include "exa_wm_mask_sample_a.g6b"
#include "exa_wm_noca.g6b"
#include "exa_wm_write.g6b"
};

static const uint32_t ps_kernel_masknoca_projective_static_gen6[][4] = {
#include "exa_wm_src_projective.g6b"
#include "exa_wm_src_sample_argb.g6b"
#include "exa_wm_mask_projective.g6b"
#include "exa_wm_mask_sample_a.g6b"
#include "exa_wm_noca.g6b"
#include "exa_wm_write.g6b"
};

typedef enum {
	SAMPLER_STATE_FILTER_NEAREST,
	SAMPLER_STATE_FILTER_BILINEAR,
	SAMPLER_STATE_FILTER_COUNT
} sampler_state_filter_t;

typedef enum {
	SAMPLER_STATE_EXTEND_NONE,
	SAMPLER_STATE_EXTEND_REPEAT,
	SAMPLER_STATE_EXTEND_PAD,
	SAMPLER_STATE_EXTEND_REFLECT,
	SAMPLER_STATE_EXTEND_COUNT
} sampler_state_extend_t;

typedef enum {
	WM_KERNEL_NOMASK_AFFINE,
	WM_KERNEL_NOMASK_PROJECTIVE,
	WM_KERNEL_MASKCA_AFFINE,
	WM_KERNEL_MASKCA_PROJECTIVE,
	WM_KERNEL_MASKCA_SRCALPHA_AFFINE,
	WM_KERNEL_MASKCA_SRCALPHA_PROJECTIVE,
	WM_KERNEL_MASKNOCA_AFFINE,
	WM_KERNEL_MASKNOCA_PROJECTIVE,
	WM_KERNEL_COUNT
} wm_kernel_t;

#define KERNEL(kernel_enum, kernel, masked) \
    [kernel_enum] = {&kernel, sizeof(kernel), masked}
struct wm_kernel_info {
	const void *data;
	unsigned int size;
	Bool has_mask;
};

static const struct wm_kernel_info wm_kernels_gen4[] = {
	KERNEL(WM_KERNEL_NOMASK_AFFINE,
	       ps_kernel_nomask_affine_static, FALSE),
	KERNEL(WM_KERNEL_NOMASK_PROJECTIVE,
	       ps_kernel_nomask_projective_static, FALSE),
	KERNEL(WM_KERNEL_MASKCA_AFFINE,
	       ps_kernel_maskca_affine_static, TRUE),
	KERNEL(WM_KERNEL_MASKCA_PROJECTIVE,
	       ps_kernel_maskca_projective_static, TRUE),
	KERNEL(WM_KERNEL_MASKCA_SRCALPHA_AFFINE,
	       ps_kernel_maskca_srcalpha_affine_static, TRUE),
	KERNEL(WM_KERNEL_MASKCA_SRCALPHA_PROJECTIVE,
	       ps_kernel_maskca_srcalpha_projective_static, TRUE),
	KERNEL(WM_KERNEL_MASKNOCA_AFFINE,
	       ps_kernel_masknoca_affine_static, TRUE),
	KERNEL(WM_KERNEL_MASKNOCA_PROJECTIVE,
	       ps_kernel_masknoca_projective_static, TRUE),
};

static const struct wm_kernel_info wm_kernels_gen5[] = {
	KERNEL(WM_KERNEL_NOMASK_AFFINE,
	       ps_kernel_nomask_affine_static_gen5, FALSE),
	KERNEL(WM_KERNEL_NOMASK_PROJECTIVE,
	       ps_kernel_nomask_projective_static_gen5, FALSE),
	KERNEL(WM_KERNEL_MASKCA_AFFINE,
	       ps_kernel_maskca_affine_static_gen5, TRUE),
	KERNEL(WM_KERNEL_MASKCA_PROJECTIVE,
	       ps_kernel_maskca_projective_static_gen5, TRUE),
	KERNEL(WM_KERNEL_MASKCA_SRCALPHA_AFFINE,
	       ps_kernel_maskca_srcalpha_affine_static_gen5, TRUE),
	KERNEL(WM_KERNEL_MASKCA_SRCALPHA_PROJECTIVE,
	       ps_kernel_maskca_srcalpha_projective_static_gen5, TRUE),
	KERNEL(WM_KERNEL_MASKNOCA_AFFINE,
	       ps_kernel_masknoca_affine_static_gen5, TRUE),
	KERNEL(WM_KERNEL_MASKNOCA_PROJECTIVE,
	       ps_kernel_masknoca_projective_static_gen5, TRUE),
};

static const struct wm_kernel_info wm_kernels_gen6[] = {
	KERNEL(WM_KERNEL_NOMASK_AFFINE,
	       ps_kernel_nomask_affine_static_gen6, FALSE),
	KERNEL(WM_KERNEL_NOMASK_PROJECTIVE,
	       ps_kernel_nomask_projective_static_gen6, FALSE),
	KERNEL(WM_KERNEL_MASKCA_AFFINE,
	       ps_kernel_maskca_affine_static_gen6, TRUE),
	KERNEL(WM_KERNEL_MASKCA_PROJECTIVE,
	       ps_kernel_maskca_projective_static_gen6, TRUE),
	KERNEL(WM_KERNEL_MASKCA_SRCALPHA_AFFINE,
	       ps_kernel_maskca_srcalpha_affine_static_gen6, TRUE),
	KERNEL(WM_KERNEL_MASKCA_SRCALPHA_PROJECTIVE,
	       ps_kernel_maskca_srcalpha_projective_static_gen6, TRUE),
	KERNEL(WM_KERNEL_MASKNOCA_AFFINE,
	       ps_kernel_masknoca_affine_static_gen6, TRUE),
	KERNEL(WM_KERNEL_MASKNOCA_PROJECTIVE,
	       ps_kernel_masknoca_projective_static_gen6, TRUE),
};

#undef KERNEL

struct i965_static_stream {
	uint32_t size, used;
	uint8_t *data;
};

static int i965_static_stream_init(struct i965_static_stream *stream)
{
	stream->used = 0;
	stream->size = 64*1024;

	stream->data = malloc(stream->size);
	return stream->data != NULL;
}

static uint32_t i965_static_stream_add(struct i965_static_stream *stream,
				       const void *data, uint32_t len, uint32_t align)
{
	uint32_t offset = ALIGN(stream->used, align);
	if (offset + len > stream->size) {
		do
			stream->size *= 2;
		while (stream->size < offset + len);

		stream->data = realloc(stream->data, stream->size);
	}

	memcpy(stream->data + offset, data, len);
	stream->used = offset + len;
	return offset;
}

static void *i965_static_stream_map(struct i965_static_stream *stream,
				    uint32_t len, uint32_t align)
{
	uint32_t offset = ALIGN(stream->used, align);
	if (offset + len > stream->size) {
		do
			stream->size *= 2;
		while (stream->size < offset + len);

		stream->data = realloc(stream->data, stream->size);
	}

	stream->used = offset + len;
	return memset(stream->data + offset, 0, len);
}

static uint32_t i965_static_stream_offsetof(struct i965_static_stream *stream, void *ptr)
{
	return (uint8_t *)ptr - stream->data;
}

static drm_intel_bo *i965_static_stream_fini(struct intel_screen_private *intel,
					     struct i965_static_stream *stream)
{
	drm_intel_bo *bo = NULL;

	if (stream->used) {
		bo = drm_intel_bo_alloc(intel->bufmgr, "stream", stream->used, 0);
		if (bo) {
			if (drm_intel_bo_subdata(bo, 0, stream->used, stream->data)) {
				drm_intel_bo_unreference(bo);
				bo = NULL;
			}
		}
	}

	free(stream->data);
	memset(stream, 0, sizeof(*stream));

	return bo;
}

typedef struct _brw_cc_unit_state_padded {
	struct brw_cc_unit_state state;
	char pad[64 - sizeof(struct brw_cc_unit_state)];
} brw_cc_unit_state_padded;

typedef struct brw_surface_state_padded {
	struct brw_surface_state state;
	char pad[32 - sizeof(struct brw_surface_state)];
} brw_surface_state_padded;

struct gen4_cc_unit_state {
	/* Index by [src_blend][dst_blend] */
	brw_cc_unit_state_padded cc_state[BRW_BLENDFACTOR_COUNT][BRW_BLENDFACTOR_COUNT];
};

typedef struct gen4_composite_op {
	int op;
	sampler_state_filter_t src_filter;
	sampler_state_filter_t mask_filter;
	sampler_state_extend_t src_extend;
	sampler_state_extend_t mask_extend;
	Bool is_affine;
	wm_kernel_t wm_kernel;
	int vertex_id;
} gen4_composite_op;

/** Private data for gen4 render accel implementation. */
struct gen4_render_state {
	drm_intel_bo *general_bo;
	drm_intel_bo *instruction_bo;

	uint32_t vs_state;
	uint32_t sf_state;
	uint32_t sf_mask_state;
	uint32_t cc_state;
	uint32_t wm_state[WM_KERNEL_COUNT][SAMPLER_STATE_FILTER_COUNT][SAMPLER_STATE_EXTEND_COUNT][SAMPLER_STATE_FILTER_COUNT][SAMPLER_STATE_EXTEND_COUNT];
	uint32_t wm_kernel[WM_KERNEL_COUNT];

	uint32_t gen6_cc_state;
	uint32_t gen6_cc_vp;
	uint32_t gen6_cc_blend;
	uint32_t gen6_cc_depth_stencil;
	gen4_composite_op composite_op;
};

static void gen6_emit_composite_state(ScrnInfoPtr scrn);
static void gen6_render_state_init(ScrnInfoPtr scrn);

/**
 * Sets up the SF state pointing at an SF kernel.
 *
 * The SF kernel does coord interp: for each attribute,
 * calculate dA/dx and dA/dy.  Hand these interpolation coefficients
 * back to SF which then hands pixels off to WM.
 */
static uint32_t gen4_create_sf_state(struct intel_screen_private *intel,
				     struct i965_static_stream *stream,
				     uint32_t kernel)
{
	struct brw_sf_unit_state *sf_state;

	sf_state = i965_static_stream_map(stream, sizeof(*sf_state), 32);

	sf_state->thread0.grf_reg_count = BRW_GRF_BLOCKS(SF_KERNEL_NUM_GRF);
	sf_state->thread0.kernel_start_pointer = kernel >> 6;
	sf_state->sf1.single_program_flow = 1;
	sf_state->sf1.binding_table_entry_count = 0;
	sf_state->sf1.thread_priority = 0;
	sf_state->sf1.floating_point_mode = 0;	/* Mesa does this */
	sf_state->sf1.illegal_op_exception_enable = 1;
	sf_state->sf1.mask_stack_exception_enable = 1;
	sf_state->sf1.sw_exception_enable = 1;
	sf_state->thread2.per_thread_scratch_space = 0;
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
	sf_state->sf6.cull_mode = BRW_CULLMODE_NONE;
	sf_state->sf6.scissor = 0;
	sf_state->sf7.trifan_pv = 2;
	sf_state->sf6.dest_org_vbias = 0x8;
	sf_state->sf6.dest_org_hbias = 0x8;

	return i965_static_stream_offsetof(stream, sf_state);
}

static uint32_t sampler_border_color_create(struct i965_static_stream *stream)
{
	struct brw_sampler_legacy_border_color *color;

	/* Set up the sampler border color (always transparent black) */
	color = i965_static_stream_map(stream, sizeof(*color), 32);

	return i965_static_stream_offsetof(stream, color);
}

static void
sampler_state_init(struct brw_sampler_state *sampler_state,
		   sampler_state_filter_t filter,
		   sampler_state_extend_t extend,
		   uint32_t border_color)
{
	sampler_state->ss0.lod_preclamp = 1;	/* GL mode */

	/* We use the legacy mode to get the semantics specified by
	 * the Render extension. */
	sampler_state->ss0.border_color_mode = BRW_BORDER_COLOR_MODE_LEGACY;

	switch (filter) {
	default:
	case SAMPLER_STATE_FILTER_NEAREST:
		sampler_state->ss0.min_filter = BRW_MAPFILTER_NEAREST;
		sampler_state->ss0.mag_filter = BRW_MAPFILTER_NEAREST;
		break;
	case SAMPLER_STATE_FILTER_BILINEAR:
		sampler_state->ss0.min_filter = BRW_MAPFILTER_LINEAR;
		sampler_state->ss0.mag_filter = BRW_MAPFILTER_LINEAR;
		break;
	}

	switch (extend) {
	default:
	case SAMPLER_STATE_EXTEND_NONE:
		sampler_state->ss1.r_wrap_mode = BRW_TEXCOORDMODE_CLAMP_BORDER;
		sampler_state->ss1.s_wrap_mode = BRW_TEXCOORDMODE_CLAMP_BORDER;
		sampler_state->ss1.t_wrap_mode = BRW_TEXCOORDMODE_CLAMP_BORDER;
		break;
	case SAMPLER_STATE_EXTEND_REPEAT:
		sampler_state->ss1.r_wrap_mode = BRW_TEXCOORDMODE_WRAP;
		sampler_state->ss1.s_wrap_mode = BRW_TEXCOORDMODE_WRAP;
		sampler_state->ss1.t_wrap_mode = BRW_TEXCOORDMODE_WRAP;
		break;
	case SAMPLER_STATE_EXTEND_PAD:
		sampler_state->ss1.r_wrap_mode = BRW_TEXCOORDMODE_CLAMP;
		sampler_state->ss1.s_wrap_mode = BRW_TEXCOORDMODE_CLAMP;
		sampler_state->ss1.t_wrap_mode = BRW_TEXCOORDMODE_CLAMP;
		break;
	case SAMPLER_STATE_EXTEND_REFLECT:
		sampler_state->ss1.r_wrap_mode = BRW_TEXCOORDMODE_MIRROR;
		sampler_state->ss1.s_wrap_mode = BRW_TEXCOORDMODE_MIRROR;
		sampler_state->ss1.t_wrap_mode = BRW_TEXCOORDMODE_MIRROR;
		break;
	}

	sampler_state->ss2.border_color_pointer = border_color >> 5;
	sampler_state->ss3.chroma_key_enable = 0;	/* disable chromakey */
}

static uint32_t gen4_create_sampler_state(struct i965_static_stream *stream,
					  sampler_state_filter_t src_filter,
					  sampler_state_extend_t src_extend,
					  sampler_state_filter_t mask_filter,
					  sampler_state_extend_t mask_extend,
					  uint32_t border_color)
{
	struct brw_sampler_state *sampler_state;

	sampler_state = i965_static_stream_map(stream,
					       sizeof(struct brw_sampler_state) * 2,
					       32);
	sampler_state_init(&sampler_state[0], src_filter, src_extend, border_color);
	sampler_state_init(&sampler_state[1], mask_filter, mask_extend, border_color);

	return i965_static_stream_offsetof(stream, sampler_state);
}

static uint32_t gen4_create_wm_state(struct intel_screen_private *intel,
				     struct i965_static_stream *stream,
				     Bool has_mask,
				     uint32_t kernel,
				     uint32_t sampler)
{
	struct brw_wm_unit_state *state;

	state = i965_static_stream_map(stream, sizeof(*state), 32);

	state->thread0.grf_reg_count = BRW_GRF_BLOCKS(PS_KERNEL_NUM_GRF);
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

	if (IS_GEN5(intel))
		state->wm4.sampler_count = 0;	/* hardware requirement */
	else
		state->wm4.sampler_count = 1;	/* 1-4 samplers used */

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

	/* binding table entry count is only used for prefetching, and it has to
	 * be set 0 for Ironlake
	 */
	if (IS_GEN5(intel))
		state->thread1.binding_table_entry_count = 0;

	return i965_static_stream_offsetof(stream, state);
}

static uint32_t gen4_create_vs_unit_state(struct intel_screen_private *intel,
					  struct i965_static_stream *stream)
{
	struct brw_vs_unit_state *vs = i965_static_stream_map(stream, sizeof(*vs), 32);

	/* Set up the vertex shader to be disabled (passthrough) */
	if (IS_GEN5(intel))
		vs->thread4.nr_urb_entries = URB_VS_ENTRIES >> 2;	/* hardware requirement */
	else
		vs->thread4.nr_urb_entries = URB_VS_ENTRIES;
	vs->thread4.urb_entry_allocation_size = URB_VS_ENTRY_SIZE - 1;
	vs->vs6.vs_enable = 0;
	vs->vs6.vert_cache_disable = 1;

	return i965_static_stream_offsetof(stream, vs);
}

/**
 * Set up all combinations of cc state: each blendfactor for source and
 * dest.
 */
static void
cc_state_init(struct brw_cc_unit_state *state,
	      int src_blend, int dst_blend,
	      uint32_t vp)
{
	state->cc0.stencil_enable = 0;	/* disable stencil */
	state->cc2.depth_test = 0;	/* disable depth test */
	state->cc2.logicop_enable = 0;	/* disable logic op */
	state->cc3.ia_blend_enable = 0;	/* blend alpha same as colors */
	state->cc3.blend_enable = 1;	/* enable color blend */
	state->cc3.alpha_test = 0;	/* disable alpha test */

	state->cc4.cc_viewport_state_offset = vp >> 5;

	state->cc5.dither_enable = 0;	/* disable dither */
	state->cc5.logicop_func = 0xc;	/* COPY */
	state->cc5.statistics_enable = 1;
	state->cc5.ia_blend_function = BRW_BLENDFUNCTION_ADD;

	/* Fill in alpha blend factors same as color, for the future. */
	state->cc5.ia_src_blend_factor = src_blend;
	state->cc5.ia_dest_blend_factor = dst_blend;

	state->cc6.blend_function = BRW_BLENDFUNCTION_ADD;
	state->cc6.clamp_post_alpha_blend = 1;
	state->cc6.clamp_pre_alpha_blend = 1;
	state->cc6.clamp_range = 0;	/* clamp range [0,1] */

	state->cc6.src_blend_factor = src_blend;
	state->cc6.dest_blend_factor = dst_blend;
}

static uint32_t gen4_create_cc_viewport(struct i965_static_stream *stream)
{
	struct brw_cc_viewport vp;

	vp.min_depth = -1.e35;
	vp.max_depth = 1.e35;

	return i965_static_stream_add(stream, &vp, sizeof(vp), 32);
}

static uint32_t gen4_create_cc_unit_state(struct i965_static_stream *stream)
{
	struct gen4_cc_unit_state *state;
	uint32_t vp;
	int i, j;

	vp = gen4_create_cc_viewport(stream);
	state = i965_static_stream_map(stream, sizeof(*state), 64);

	for (i = 0; i < BRW_BLENDFACTOR_COUNT; i++) {
		for (j = 0; j < BRW_BLENDFACTOR_COUNT; j++) {
			cc_state_init(&state->cc_state[i][j].state, i, j, vp);
		}
	}

	return i965_static_stream_offsetof(stream, state);
}

static uint32_t i965_get_card_format(PicturePtr picture)
{
	int i;

	for (i = 0; i < sizeof(i965_tex_formats) / sizeof(i965_tex_formats[0]);
	     i++) {
		if (i965_tex_formats[i].fmt == picture->format)
			break;
	}
	assert(i != sizeof(i965_tex_formats) / sizeof(i965_tex_formats[0]));

	return i965_tex_formats[i].card_fmt;
}

static sampler_state_filter_t sampler_state_filter_from_picture(int filter)
{
	switch (filter) {
	case PictFilterNearest:
		return SAMPLER_STATE_FILTER_NEAREST;
	case PictFilterBilinear:
		return SAMPLER_STATE_FILTER_BILINEAR;
	default:
		return -1;
	}
}

static sampler_state_extend_t sampler_state_extend_from_picture(int repeat_type)
{
	switch (repeat_type) {
	case RepeatNone:
		return SAMPLER_STATE_EXTEND_NONE;
	case RepeatNormal:
		return SAMPLER_STATE_EXTEND_REPEAT;
	case RepeatPad:
		return SAMPLER_STATE_EXTEND_PAD;
	case RepeatReflect:
		return SAMPLER_STATE_EXTEND_REFLECT;
	default:
		return -1;
	}
}

/**
 * Sets up the common fields for a surface state buffer for the given
 * picture in the given surface state buffer.
 */
static int
i965_set_picture_surface_state(intel_screen_private *intel,
			       PicturePtr picture, PixmapPtr pixmap,
			       Bool is_dst)
{
	struct intel_pixmap *priv = intel_get_pixmap_private(pixmap);
	struct brw_surface_state *ss;
	uint32_t write_domain, read_domains;
	int offset;

	if (is_dst) {
		write_domain = I915_GEM_DOMAIN_RENDER;
		read_domains = I915_GEM_DOMAIN_RENDER;
	} else {
		write_domain = 0;
		read_domains = I915_GEM_DOMAIN_SAMPLER;
	}
	intel_batch_mark_pixmap_domains(intel, priv,
					read_domains, write_domain);
	if (is_dst) {
		if (priv->dst_bound)
			return priv->dst_bound;
	} else {
		if (priv->src_bound)
			return priv->src_bound;
	}

	ss = (struct brw_surface_state *)
		(intel->surface_data + intel->surface_used);

	memset(ss, 0, sizeof(*ss));
	ss->ss0.surface_type = BRW_SURFACE_2D;
	if (is_dst)
		ss->ss0.surface_format = i965_get_dest_format(picture);
	else
		ss->ss0.surface_format = i965_get_card_format(picture);

	ss->ss0.data_return_format = BRW_SURFACERETURNFORMAT_FLOAT32;
	ss->ss0.color_blend = 1;
	ss->ss1.base_addr = priv->bo->offset;

	ss->ss2.height = pixmap->drawable.height - 1;
	ss->ss2.width = pixmap->drawable.width - 1;
	ss->ss3.pitch = intel_pixmap_pitch(pixmap) - 1;
	ss->ss3.tile_walk = 0;	/* Tiled X */
	ss->ss3.tiled_surface = intel_pixmap_tiled(pixmap) ? 1 : 0;

	dri_bo_emit_reloc(intel->surface_bo,
			  read_domains, write_domain,
			  0,
			  intel->surface_used +
			  offsetof(struct brw_surface_state, ss1),
			  priv->bo);

	offset = intel->surface_used;
	intel->surface_used += sizeof(struct brw_surface_state_padded);

	if (is_dst)
		priv->dst_bound = offset;
	else
		priv->src_bound = offset;

	return offset;
}

static void gen4_composite_vertex_elements(struct intel_screen_private *intel)
{
	struct gen4_render_state *render_state = intel->gen4_render_state;
	gen4_composite_op *composite_op = &render_state->composite_op;
	Bool has_mask = intel->render_mask != NULL;
	Bool is_affine = composite_op->is_affine;
	/*
	 * number of extra parameters per vertex
	 */
	int nelem = has_mask ? 2 : 1;
	/*
	 * size of extra parameters:
	 *  3 for homogenous (xyzw)
	 *  2 for cartesian (xy)
	 */
	int selem = is_affine ? 2 : 3;
	uint32_t w_component;
	uint32_t src_format;
	int id;

	id = has_mask << 1 | is_affine;

	if (composite_op->vertex_id == id)
		return;

	composite_op->vertex_id = id;

	if (is_affine) {
		src_format = BRW_SURFACEFORMAT_R32G32_FLOAT;
		w_component = BRW_VFCOMPONENT_STORE_1_FLT;
	} else {
		src_format = BRW_SURFACEFORMAT_R32G32B32_FLOAT;
		w_component = BRW_VFCOMPONENT_STORE_SRC;
	}

	if (IS_GEN5(intel)) {
		/*
		 * The reason to add this extra vertex element in the header is that
		 * Ironlake has different vertex header definition and origin method to
		 * set destination element offset doesn't exist anymore, which means
		 * hardware requires a predefined vertex element layout.
		 *
		 * haihao proposed this approach to fill the first vertex element, so
		 * origin layout for Gen4 doesn't need to change, and origin shader
		 * programs behavior is also kept.
		 *
		 * I think this is not bad. - zhenyu
		 */

		OUT_BATCH(BRW_3DSTATE_VERTEX_ELEMENTS |
			  ((2 * (2 + nelem)) - 1));
		OUT_BATCH((id << VE0_VERTEX_BUFFER_INDEX_SHIFT) | VE0_VALID |
			  (BRW_SURFACEFORMAT_R32G32_FLOAT << VE0_FORMAT_SHIFT) |
			  (0 << VE0_OFFSET_SHIFT));

		OUT_BATCH((BRW_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_0_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_1_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_2_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_3_SHIFT));
	} else {
		/* Set up our vertex elements, sourced from the single vertex buffer.
		 * that will be set up later.
		 */
		OUT_BATCH(BRW_3DSTATE_VERTEX_ELEMENTS |
			  ((2 * (1 + nelem)) - 1));
	}

	/* x,y */
	OUT_BATCH((id << VE0_VERTEX_BUFFER_INDEX_SHIFT) | VE0_VALID |
		  (BRW_SURFACEFORMAT_R32G32_FLOAT << VE0_FORMAT_SHIFT) |
		  (0 << VE0_OFFSET_SHIFT));

	if (IS_GEN5(intel))
		OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_2_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT));
	else
		OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_2_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT) |
			  (4 << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT));
	/* u0, v0, w0 */
	OUT_BATCH((id << VE0_VERTEX_BUFFER_INDEX_SHIFT) | VE0_VALID |
		  (src_format << VE0_FORMAT_SHIFT) |
		  ((2 * 4) << VE0_OFFSET_SHIFT));	/* offset vb in bytes */

	if (IS_GEN5(intel))
		OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
			  (w_component << VE1_VFCOMPONENT_2_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT));
	else
		OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
			  (w_component << VE1_VFCOMPONENT_2_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT) |
			  ((4 + 4) << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT));	/* VUE offset in dwords */
	/* u1, v1, w1 */
	if (has_mask) {
		OUT_BATCH((id << VE0_VERTEX_BUFFER_INDEX_SHIFT) | VE0_VALID |
			  (src_format << VE0_FORMAT_SHIFT) |
			  (((2 + selem) * 4) << VE0_OFFSET_SHIFT));	/* vb offset in bytes */

		if (IS_GEN5(intel))
			OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
				  (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
				  (w_component << VE1_VFCOMPONENT_2_SHIFT) |
				  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT));
		else
			OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
				  (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
				  (w_component << VE1_VFCOMPONENT_2_SHIFT) |
				  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT) |
				  ((4 + 4 + 4) << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT));	/* VUE offset in dwords */
	}
}

static void i965_emit_composite_state(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct gen4_render_state *render_state = intel->gen4_render_state;
	gen4_composite_op *composite_op = &render_state->composite_op;
	int op = composite_op->op;
	PicturePtr mask_picture = intel->render_mask_picture;
	PicturePtr dest_picture = intel->render_dest_picture;
	PixmapPtr mask = intel->render_mask;
	PixmapPtr dest = intel->render_dest;
	sampler_state_filter_t src_filter = composite_op->src_filter;
	sampler_state_filter_t mask_filter = composite_op->mask_filter;
	sampler_state_extend_t src_extend = composite_op->src_extend;
	sampler_state_extend_t mask_extend = composite_op->mask_extend;
	uint32_t src_blend, dst_blend;

	intel->needs_render_state_emit = FALSE;

	/* Begin the long sequence of commands needed to set up the 3D
	 * rendering pipe
	 */

	if (intel->needs_3d_invariant) {
		if (IS_GEN5(intel)) {
			/* Ironlake errata workaround: Before disabling the clipper,
			 * you have to MI_FLUSH to get the pipeline idle.
			 */
			OUT_BATCH(MI_FLUSH | MI_INHIBIT_RENDER_CACHE_FLUSH);
		}

		/* Match Mesa driver setup */
		if (INTEL_INFO(intel)->gen >= 45)
			OUT_BATCH(NEW_PIPELINE_SELECT | PIPELINE_SELECT_3D);
		else
			OUT_BATCH(BRW_PIPELINE_SELECT | PIPELINE_SELECT_3D);

		/* Set system instruction pointer */
		OUT_BATCH(BRW_STATE_SIP | 0);
		OUT_BATCH(0);

		intel->needs_3d_invariant = FALSE;
	}

	if (intel->surface_reloc == 0) {
		/* Zero out the two base address registers so all offsets are
		 * absolute.
		 */
		if (IS_GEN5(intel)) {
			OUT_BATCH(BRW_STATE_BASE_ADDRESS | 6);
			OUT_RELOC(intel->gen4_render_state->general_bo,
				  I915_GEM_DOMAIN_INSTRUCTION, 0,
				  BASE_ADDRESS_MODIFY);
			intel->surface_reloc = intel->batch_used;
			intel_batch_emit_dword(intel,
					       intel->surface_bo->offset | BASE_ADDRESS_MODIFY);
			OUT_BATCH(0 | BASE_ADDRESS_MODIFY);	/* media base addr, don't care */
			OUT_RELOC(intel->gen4_render_state->instruction_bo,
				  I915_GEM_DOMAIN_INSTRUCTION, 0,
				  BASE_ADDRESS_MODIFY);
			/* general state max addr, disabled */
			OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
			/* media object state max addr, disabled */
			OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
			/* Instruction max addr, disabled */
			OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
		} else {
			OUT_BATCH(BRW_STATE_BASE_ADDRESS | 4);
			OUT_RELOC(intel->gen4_render_state->general_bo,
				  I915_GEM_DOMAIN_INSTRUCTION, 0,
				  BASE_ADDRESS_MODIFY);
			intel->surface_reloc = intel->batch_used;
			intel_batch_emit_dword(intel,
					       intel->surface_bo->offset | BASE_ADDRESS_MODIFY);
			OUT_BATCH(0 | BASE_ADDRESS_MODIFY);	/* media base addr, don't care */
			/* general state max addr, disabled */
			OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
			/* media object state max addr, disabled */
			OUT_BATCH(0 | BASE_ADDRESS_MODIFY);
		}
	}

	i965_get_blend_cntl(op, mask_picture, dest_picture->format,
			    &src_blend, &dst_blend);

	{
		/* Binding table pointers */
		OUT_BATCH(BRW_3DSTATE_BINDING_TABLE_POINTERS | 4);
		OUT_BATCH(0);	/* vs */
		OUT_BATCH(0);	/* gs */
		OUT_BATCH(0);	/* clip */
		OUT_BATCH(0);	/* sf */
		/* Only the PS uses the binding table */
		OUT_BATCH(intel->surface_table);

		/* The drawing rectangle clipping is always on.  Set it to values that
		 * shouldn't do any clipping.
		 */
		OUT_BATCH(BRW_3DSTATE_DRAWING_RECTANGLE | 2);
		OUT_BATCH(0x00000000);	/* ymin, xmin */
		OUT_BATCH(DRAW_YMAX(dest->drawable.height - 1) |
			  DRAW_XMAX(dest->drawable.width - 1));	/* ymax, xmax */
		OUT_BATCH(0x00000000);	/* yorigin, xorigin */

		/* skip the depth buffer */
		/* skip the polygon stipple */
		/* skip the polygon stipple offset */
		/* skip the line stipple */

		/* Set the pointers to the 3d pipeline state */
		OUT_BATCH(BRW_3DSTATE_PIPELINED_POINTERS | 5);
		OUT_BATCH(render_state->vs_state);
		OUT_BATCH(BRW_GS_DISABLE);	/* disable GS, resulting in passthrough */
		OUT_BATCH(BRW_CLIP_DISABLE);	/* disable CLIP, resulting in passthrough */
		if (mask) {
			OUT_BATCH(render_state->sf_mask_state);
		} else {
			OUT_BATCH(render_state->sf_state);
		}

		OUT_BATCH(render_state->wm_state[composite_op->wm_kernel][src_filter][src_extend][mask_filter][mask_extend]);
		OUT_BATCH(render_state->cc_state +
			  offsetof(struct gen4_cc_unit_state,
				   cc_state[src_blend][dst_blend]));
	}

	{
		int urb_vs_start, urb_vs_size;
		int urb_gs_start, urb_gs_size;
		int urb_clip_start, urb_clip_size;
		int urb_sf_start, urb_sf_size;
		int urb_cs_start, urb_cs_size;

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

		/* Erratum (Vol 1a, p32):
		 *   URB_FENCE must not cross a cache-line (64 bytes).
		 */
		if ((intel->batch_used & 15) > (16 - 3)) {
			int cnt = 16 - (intel->batch_used & 15);
			while (cnt--)
				OUT_BATCH(MI_NOOP);
		}

		OUT_BATCH(BRW_URB_FENCE |
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
		OUT_BATCH(BRW_CS_URB_STATE | 0);
		OUT_BATCH(((URB_CS_ENTRY_SIZE - 1) << 4) |
			  (URB_CS_ENTRIES << 0));
	}

	gen4_composite_vertex_elements(intel);
}

/**
 * Returns whether the current set of composite state plus vertex buffer is
 * expected to fit in the aperture.
 */
static Bool i965_composite_check_aperture(struct intel_screen_private *intel)
{
	drm_intel_bo *bo_table[] = {
		intel->batch_bo,
		intel->vertex_bo,
		intel->surface_bo,
		intel->gen4_render_state->general_bo,
		intel->gen4_render_state->instruction_bo,
	};
	return drm_intel_bufmgr_check_aperture_space(bo_table,
						     ARRAY_SIZE(bo_table)) == 0;
}

static void i965_surface_flush(struct intel_screen_private *intel)
{
	struct intel_pixmap *priv;

	drm_intel_bo_subdata(intel->surface_bo,
			     0, intel->surface_used,
			     intel->surface_data);
	intel->surface_used = 0;

	assert (intel->surface_reloc != 0);
	drm_intel_bo_emit_reloc(intel->batch_bo,
				intel->surface_reloc * 4,
				intel->surface_bo, BASE_ADDRESS_MODIFY,
				I915_GEM_DOMAIN_INSTRUCTION, 0);
	intel->surface_reloc = 0;

	drm_intel_bo_unreference(intel->surface_bo);
	intel->surface_bo =
		drm_intel_bo_alloc(intel->bufmgr, "surface data",
				   sizeof(intel->surface_data), 4096);

	list_foreach_entry(priv, struct intel_pixmap, &intel->batch_pixmaps, batch)
		priv->dst_bound = priv->src_bound = 0;
}

Bool
i965_prepare_composite(int op, PicturePtr source_picture,
		       PicturePtr mask_picture, PicturePtr dest_picture,
		       PixmapPtr source, PixmapPtr mask, PixmapPtr dest)
{
	ScrnInfoPtr scrn = xf86Screens[dest_picture->pDrawable->pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct gen4_render_state *render_state = intel->gen4_render_state;
	gen4_composite_op *composite_op = &render_state->composite_op;

	composite_op->src_filter =
	    sampler_state_filter_from_picture(source_picture->filter);
	if (composite_op->src_filter < 0) {
		intel_debug_fallback(scrn, "Bad src filter 0x%x\n",
				     source_picture->filter);
		return FALSE;
	}
	composite_op->src_extend =
	    sampler_state_extend_from_picture(source_picture->repeatType);
	if (composite_op->src_extend < 0) {
		intel_debug_fallback(scrn, "Bad src repeat 0x%x\n",
				     source_picture->repeatType);
		return FALSE;
	}

	if (mask_picture) {
		if (mask_picture->componentAlpha &&
		    PICT_FORMAT_RGB(mask_picture->format)) {
			/* Check if it's component alpha that relies on a source alpha and on
			 * the source value.  We can only get one of those into the single
			 * source value that we get to blend with.
			 */
			if (i965_blend_op[op].src_alpha &&
			    (i965_blend_op[op].src_blend != BRW_BLENDFACTOR_ZERO)) {
				intel_debug_fallback(scrn,
						     "Component alpha not supported "
						     "with source alpha and source "
						     "value blending.\n");
				return FALSE;
			}
		}

		composite_op->mask_filter =
		    sampler_state_filter_from_picture(mask_picture->filter);
		if (composite_op->mask_filter < 0) {
			intel_debug_fallback(scrn, "Bad mask filter 0x%x\n",
					     mask_picture->filter);
			return FALSE;
		}
		composite_op->mask_extend =
		    sampler_state_extend_from_picture(mask_picture->repeatType);
		if (composite_op->mask_extend < 0) {
			intel_debug_fallback(scrn, "Bad mask repeat 0x%x\n",
					     mask_picture->repeatType);
			return FALSE;
		}
	} else {
		composite_op->mask_filter = SAMPLER_STATE_FILTER_NEAREST;
		composite_op->mask_extend = SAMPLER_STATE_EXTEND_NONE;
	}

	/* Flush any pending writes prior to relocating the textures. */
	if (intel_pixmap_is_dirty(source) ||
	    (mask && intel_pixmap_is_dirty(mask)))
		intel_batch_emit_flush(scrn);

	composite_op->op = op;
	intel->render_source_picture = source_picture;
	intel->render_mask_picture = mask_picture;
	intel->render_dest_picture = dest_picture;
	intel->render_source = source;
	intel->render_mask = mask;
	intel->render_dest = dest;

	intel->scale_units[0][0] = 1. / source->drawable.width;
	intel->scale_units[0][1] = 1. / source->drawable.height;

	intel->transform[0] = source_picture->transform;
	composite_op->is_affine = intel_transform_is_affine(intel->transform[0]);

	if (!mask) {
		intel->transform[1] = NULL;
		intel->scale_units[1][0] = -1;
		intel->scale_units[1][1] = -1;
	} else {
		intel->transform[1] = mask_picture->transform;
		intel->scale_units[1][0] = 1. / mask->drawable.width;
		intel->scale_units[1][1] = 1. / mask->drawable.height;
		composite_op->is_affine &=
		    intel_transform_is_affine(intel->transform[1]);
	}

	if (mask) {
		if (mask_picture->componentAlpha &&
		    PICT_FORMAT_RGB(mask_picture->format)) {
			if (i965_blend_op[op].src_alpha) {
				if (composite_op->is_affine)
					composite_op->wm_kernel =
					    WM_KERNEL_MASKCA_SRCALPHA_AFFINE;
				else
					composite_op->wm_kernel =
					    WM_KERNEL_MASKCA_SRCALPHA_PROJECTIVE;
			} else {
				if (composite_op->is_affine)
					composite_op->wm_kernel =
					    WM_KERNEL_MASKCA_AFFINE;
				else
					composite_op->wm_kernel =
					    WM_KERNEL_MASKCA_PROJECTIVE;
			}
		} else {
			if (composite_op->is_affine)
				composite_op->wm_kernel =
				    WM_KERNEL_MASKNOCA_AFFINE;
			else
				composite_op->wm_kernel =
				    WM_KERNEL_MASKNOCA_PROJECTIVE;
		}
	} else {
		if (composite_op->is_affine)
			composite_op->wm_kernel = WM_KERNEL_NOMASK_AFFINE;
		else
			composite_op->wm_kernel = WM_KERNEL_NOMASK_PROJECTIVE;
	}

	intel->floats_per_vertex =
		2 + (mask ? 2 : 1) * (composite_op->is_affine ? 2: 3);

	if (!i965_composite_check_aperture(intel)) {
		intel_batch_submit(scrn);
		if (!i965_composite_check_aperture(intel)) {
			intel_debug_fallback(scrn,
					     "Couldn't fit render operation "
					     "in aperture\n");
			return FALSE;
		}
	}

	if (sizeof(intel->surface_data) - intel->surface_used <
	    4 * sizeof(struct brw_surface_state_padded))
		i965_surface_flush(intel);

	intel->needs_render_state_emit = TRUE;

	return TRUE;
}

static void i965_select_vertex_buffer(struct intel_screen_private *intel)
{
	int id = intel->gen4_render_state->composite_op.vertex_id;

	if (intel->vertex_id & (1 << id))
		return;

	/* Set up the pointer to our (single) vertex buffer */
	OUT_BATCH(BRW_3DSTATE_VERTEX_BUFFERS | 3);

	/* XXX could use multiple vbo to reduce relocations if
	 * frequently switching between vertex sizes, like rgb10text.
	 */
	if (INTEL_INFO(intel)->gen >= 60) {
		OUT_BATCH((id << GEN6_VB0_BUFFER_INDEX_SHIFT) |
			  GEN6_VB0_VERTEXDATA |
			  (4*intel->floats_per_vertex << VB0_BUFFER_PITCH_SHIFT));
	} else {
		OUT_BATCH((id << VB0_BUFFER_INDEX_SHIFT) |
			  VB0_VERTEXDATA |
			  (4*intel->floats_per_vertex << VB0_BUFFER_PITCH_SHIFT));
	}
	OUT_RELOC(intel->vertex_bo, I915_GEM_DOMAIN_VERTEX, 0, 0);
	if (INTEL_INFO(intel)->gen >= 50)
		OUT_RELOC(intel->vertex_bo,
			  I915_GEM_DOMAIN_VERTEX, 0,
			  sizeof(intel->vertex_ptr) - 1);
	else
		OUT_BATCH(0);
	OUT_BATCH(0);		// ignore for VERTEXDATA, but still there

	intel->vertex_id |= 1 << id;
}

static void i965_bind_surfaces(struct intel_screen_private *intel)
{
	uint32_t *binding_table;

	assert(intel->surface_used + 4 * sizeof(struct brw_surface_state_padded) <= sizeof(intel->surface_data));

	binding_table = (uint32_t*) (intel->surface_data + intel->surface_used);
	intel->surface_table = intel->surface_used;
	intel->surface_used += sizeof(struct brw_surface_state_padded);

	binding_table[0] =
		i965_set_picture_surface_state(intel,
					       intel->render_dest_picture,
					       intel->render_dest,
					       TRUE);
	binding_table[1] =
		i965_set_picture_surface_state(intel,
					       intel->render_source_picture,
					       intel->render_source,
					       FALSE);
	if (intel->render_mask) {
		binding_table[2] =
			i965_set_picture_surface_state(intel,
						       intel->render_mask_picture,
						       intel->render_mask,
						       FALSE);
	}
}

void
i965_composite(PixmapPtr dest, int srcX, int srcY, int maskX, int maskY,
	       int dstX, int dstY, int w, int h)
{
	ScrnInfoPtr scrn = xf86Screens[dest->drawable.pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct gen4_render_state *render_state = intel->gen4_render_state;
	Bool has_mask;
	float src_x[3], src_y[3], src_w[3], mask_x[3], mask_y[3], mask_w[3];
	Bool is_affine = render_state->composite_op.is_affine;

	if (is_affine) {
		if (!intel_get_transformed_coordinates(srcX, srcY,
						      intel->transform[0],
						      &src_x[0], &src_y[0]))
			return;
		if (!intel_get_transformed_coordinates(srcX, srcY + h,
						      intel->transform[0],
						      &src_x[1], &src_y[1]))
			return;
		if (!intel_get_transformed_coordinates(srcX + w, srcY + h,
						      intel->transform[0],
						      &src_x[2], &src_y[2]))
			return;
	} else {
		if (!intel_get_transformed_coordinates_3d(srcX, srcY,
							 intel->transform[0],
							 &src_x[0], &src_y[0],
							 &src_w[0]))
			return;
		if (!intel_get_transformed_coordinates_3d(srcX, srcY + h,
							 intel->transform[0],
							 &src_x[1], &src_y[1],
							 &src_w[1]))
			return;
		if (!intel_get_transformed_coordinates_3d(srcX + w, srcY + h,
							 intel->transform[0],
							 &src_x[2], &src_y[2],
							 &src_w[2]))
			return;
	}

	if (intel->render_mask) {
		has_mask = TRUE;
		if (is_affine) {
			if (!intel_get_transformed_coordinates(maskX, maskY,
							      intel->
							      transform[1],
							      &mask_x[0],
							      &mask_y[0]))
				return;
			if (!intel_get_transformed_coordinates(maskX, maskY + h,
							      intel->
							      transform[1],
							      &mask_x[1],
							      &mask_y[1]))
				return;
			if (!intel_get_transformed_coordinates
			    (maskX + w, maskY + h, intel->transform[1],
			     &mask_x[2], &mask_y[2]))
				return;
		} else {
			if (!intel_get_transformed_coordinates_3d(maskX, maskY,
								 intel->
								 transform[1],
								 &mask_x[0],
								 &mask_y[0],
								 &mask_w[0]))
				return;
			if (!intel_get_transformed_coordinates_3d
			    (maskX, maskY + h, intel->transform[1], &mask_x[1],
			     &mask_y[1], &mask_w[1]))
				return;
			if (!intel_get_transformed_coordinates_3d
			    (maskX + w, maskY + h, intel->transform[1],
			     &mask_x[2], &mask_y[2], &mask_w[2]))
				return;
		}
	} else {
		has_mask = FALSE;
	}

	if (!i965_composite_check_aperture(intel))
		intel_batch_submit(scrn);

	intel_batch_start_atomic(scrn, 200);
	if (intel->needs_render_state_emit) {
		i965_bind_surfaces(intel);

		if (INTEL_INFO(intel)->gen >= 60)
			gen6_emit_composite_state(scrn);
		else
			i965_emit_composite_state(scrn);
	}

	if (intel->floats_per_vertex != intel->last_floats_per_vertex) {
		intel->vertex_index = (intel->vertex_used + intel->floats_per_vertex - 1) / intel->floats_per_vertex;
		intel->vertex_used = intel->vertex_index * intel->floats_per_vertex;
		intel->last_floats_per_vertex = intel->floats_per_vertex;
	}
	if (intel_vertex_space(intel) < 3*4*intel->floats_per_vertex) {
		i965_vertex_flush(intel);
		intel_next_vertex(intel);
		intel->vertex_index = 0;
	}
	i965_select_vertex_buffer(intel);

	if (intel->vertex_offset == 0) {
		OUT_BATCH(BRW_3DPRIMITIVE |
			  BRW_3DPRIMITIVE_VERTEX_SEQUENTIAL |
			  (_3DPRIM_RECTLIST << BRW_3DPRIMITIVE_TOPOLOGY_SHIFT) |
			  (0 << 9) |
			  4);
		intel->vertex_offset = intel->batch_used;
		OUT_BATCH(0);	/* vertex count, to be filled in later */
		OUT_BATCH(intel->vertex_index);
		OUT_BATCH(1);	/* single instance */
		OUT_BATCH(0);	/* start instance location */
		OUT_BATCH(0);	/* index buffer offset, ignored */
		intel->vertex_count = intel->vertex_index;
	}

	OUT_VERTEX(dstX + w);
	OUT_VERTEX(dstY + h);
	OUT_VERTEX(src_x[2] * intel->scale_units[0][0]);
	OUT_VERTEX(src_y[2] * intel->scale_units[0][1]);
	if (!is_affine)
		OUT_VERTEX(src_w[2]);
	if (has_mask) {
		OUT_VERTEX(mask_x[2] * intel->scale_units[1][0]);
		OUT_VERTEX(mask_y[2] * intel->scale_units[1][1]);
		if (!is_affine)
			OUT_VERTEX(mask_w[2]);
	}

	/* rect (x1,y2) */
	OUT_VERTEX(dstX);
	OUT_VERTEX(dstY + h);
	OUT_VERTEX(src_x[1] * intel->scale_units[0][0]);
	OUT_VERTEX(src_y[1] * intel->scale_units[0][1]);
	if (!is_affine)
		OUT_VERTEX(src_w[1]);
	if (has_mask) {
		OUT_VERTEX(mask_x[1] * intel->scale_units[1][0]);
		OUT_VERTEX(mask_y[1] * intel->scale_units[1][1]);
		if (!is_affine)
			OUT_VERTEX(mask_w[1]);
	}

	/* rect (x1,y1) */
	OUT_VERTEX(dstX);
	OUT_VERTEX(dstY);
	OUT_VERTEX(src_x[0] * intel->scale_units[0][0]);
	OUT_VERTEX(src_y[0] * intel->scale_units[0][1]);
	if (!is_affine)
		OUT_VERTEX(src_w[0]);
	if (has_mask) {
		OUT_VERTEX(mask_x[0] * intel->scale_units[1][0]);
		OUT_VERTEX(mask_y[0] * intel->scale_units[1][1]);
		if (!is_affine)
			OUT_VERTEX(mask_w[0]);
	}
	intel->vertex_index += 3;

	if (INTEL_INFO(intel)->gen < 50) {
	    /* XXX OMG! */
	    i965_vertex_flush(intel);
	    OUT_BATCH(MI_FLUSH | MI_INHIBIT_RENDER_CACHE_FLUSH);
	}

	intel_batch_end_atomic(scrn);
}

void i965_batch_commit_notify(intel_screen_private *intel)
{
	intel->needs_render_state_emit = TRUE;
	intel->needs_3d_invariant = TRUE;
	intel->last_floats_per_vertex = 0;
	intel->vertex_index = 0;

	intel->gen6_render_state.num_sf_outputs = 0;
	intel->gen6_render_state.samplers = -1;
	intel->gen6_render_state.blend = -1;
	intel->gen6_render_state.kernel = -1;
	intel->gen6_render_state.vertex_size = 0;
	intel->gen6_render_state.vertex_type = 0;
	intel->gen6_render_state.drawrect = -1;
}

/**
 * Called at EnterVT so we can set up our offsets into the state buffer.
 */
void gen4_render_state_init(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct gen4_render_state *render;
	struct i965_static_stream stream[2];
	struct i965_static_stream *instruction, *general;
	const struct wm_kernel_info *wm_kernels;
	uint32_t sf_kernel, sf_kernel_mask;
	uint32_t border_color;
	int i, j, k, l, m;

	intel->needs_3d_invariant = TRUE;

	intel->surface_bo =
		drm_intel_bo_alloc(intel->bufmgr, "surface data",
				   sizeof(intel->surface_data), 4096);
	intel->surface_used = 0;

	if (INTEL_INFO(intel)->gen >= 60)
		return gen6_render_state_init(scrn);

	if (intel->gen4_render_state == NULL)
		intel->gen4_render_state = calloc(sizeof(*render), 1);

	render = intel->gen4_render_state;

	i965_static_stream_init(general = &stream[0]);
	if (IS_GEN5(intel))
		i965_static_stream_init(instruction = &stream[1]);
	else
		instruction = general;

	render->vs_state = gen4_create_vs_unit_state(intel, general);

	/* Set up the two SF states (one for blending with a mask, one without) */
	if (IS_GEN5(intel)) {
		sf_kernel =
			i965_static_stream_add(instruction,
					       sf_kernel_static_gen5,
					       sizeof (sf_kernel_static_gen5),
					       64);
		sf_kernel_mask =
			i965_static_stream_add(instruction,
					       sf_kernel_mask_static_gen5,
					       sizeof (sf_kernel_mask_static_gen5),
					       64);
	} else {
		sf_kernel =
			i965_static_stream_add(instruction,
					       sf_kernel_static,
					       sizeof (sf_kernel_static),
					       64);
		sf_kernel_mask =
			i965_static_stream_add(instruction,
					       sf_kernel_mask_static,
					       sizeof (sf_kernel_mask_static),
					       64);
	}
	render->sf_state =
	       	gen4_create_sf_state(intel, general, sf_kernel);
	render->sf_mask_state =
	       	gen4_create_sf_state(intel, general, sf_kernel_mask);

	wm_kernels = IS_GEN5(intel) ? wm_kernels_gen5 : wm_kernels_gen4;
	for (m = 0; m < WM_KERNEL_COUNT; m++) {
		render->wm_kernel[m] =
			i965_static_stream_add(instruction,
					       wm_kernels[m].data,
					       wm_kernels[m].size,
					       64);
	}

	/* Set up the WM states: each filter/extend type for source and mask, per
	 * kernel.
	 */
	border_color = sampler_border_color_create(general);
	for (i = 0; i < SAMPLER_STATE_FILTER_COUNT; i++) {
		for (j = 0; j < SAMPLER_STATE_EXTEND_COUNT; j++) {
			for (k = 0; k < SAMPLER_STATE_FILTER_COUNT; k++) {
				for (l = 0; l < SAMPLER_STATE_EXTEND_COUNT; l++) {
					uint32_t sampler_state;

					sampler_state =
						gen4_create_sampler_state(general,
									  i, j,
									  k, l,
									  border_color);

					for (m = 0; m < WM_KERNEL_COUNT; m++) {
						render->wm_state[m][i][j][k][l] =
							gen4_create_wm_state(intel, general,
									     wm_kernels[m].has_mask,
									     render->wm_kernel[m],
									     sampler_state);
					}
				}
			}
		}
	}

	render->cc_state = gen4_create_cc_unit_state(general);

	render->general_bo = i965_static_stream_fini(intel, general);
	render->instruction_bo = i965_static_stream_fini(intel, instruction);
}

/**
 * Called at LeaveVT.
 */
void gen4_render_state_cleanup(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct gen4_render_state *render_state = intel->gen4_render_state;

	drm_intel_bo_unreference(intel->surface_bo);
	drm_intel_bo_unreference(render_state->general_bo);
	drm_intel_bo_unreference(render_state->instruction_bo);

	free(intel->gen4_render_state);
	intel->gen4_render_state = NULL;
}

/*
 * for GEN6+
 */
#define GEN6_BLEND_STATE_PADDED_SIZE	ALIGN(sizeof(struct gen6_blend_state), 64)

static uint32_t
gen6_composite_create_cc_state(struct i965_static_stream *stream)
{
	struct gen6_color_calc_state *state = i965_static_stream_map(stream, sizeof(*state), 64);

	state->constant_r = 1.0;
	state->constant_g = 0.0;
	state->constant_b = 1.0;
	state->constant_a = 1.0;

	return i965_static_stream_offsetof(stream, state);
}

static uint32_t
gen6_composite_create_blend_state(struct i965_static_stream *stream)
{
	char *base;
	int src_blend, dst_blend;

	base = i965_static_stream_map(stream,
				       BRW_BLENDFACTOR_COUNT * BRW_BLENDFACTOR_COUNT * GEN6_BLEND_STATE_PADDED_SIZE,
				       64);

	for (src_blend = 0; src_blend < BRW_BLENDFACTOR_COUNT; src_blend++) {
		for (dst_blend = 0; dst_blend < BRW_BLENDFACTOR_COUNT; dst_blend++) {
			struct gen6_blend_state *blend_state = (struct gen6_blend_state *)(base +
				((src_blend * BRW_BLENDFACTOR_COUNT) + dst_blend) * GEN6_BLEND_STATE_PADDED_SIZE);

			blend_state->blend0.dest_blend_factor = dst_blend;
			blend_state->blend0.source_blend_factor = src_blend;
			blend_state->blend0.blend_func = BRW_BLENDFUNCTION_ADD;
			blend_state->blend0.ia_blend_enable = 0;
			blend_state->blend0.blend_enable = 1;

			blend_state->blend1.post_blend_clamp_enable = 1;
			blend_state->blend1.pre_blend_clamp_enable = 1;
			blend_state->blend1.clamp_range = 0; /* clamp range [0, 1] */
			blend_state->blend1.dither_enable = 0;
			blend_state->blend1.logic_op_enable = 0;
			blend_state->blend1.alpha_test_enable = 0;
		}
	}

	return i965_static_stream_offsetof(stream, base);
}

static uint32_t
gen6_composite_create_depth_stencil_state(struct i965_static_stream *stream)
{
	struct gen6_depth_stencil_state *state;

	state = i965_static_stream_map(stream, sizeof(*state), 64);
	return i965_static_stream_offsetof(stream, state);
}

static void
gen6_composite_invariant_states(intel_screen_private *intel)
{
	OUT_BATCH(NEW_PIPELINE_SELECT | PIPELINE_SELECT_3D);

	OUT_BATCH(GEN6_3DSTATE_MULTISAMPLE | (3 - 2));
	OUT_BATCH(GEN6_3DSTATE_MULTISAMPLE_PIXEL_LOCATION_CENTER |
		  GEN6_3DSTATE_MULTISAMPLE_NUMSAMPLES_1); /* 1 sample/pixel */
	OUT_BATCH(0);

	OUT_BATCH(GEN6_3DSTATE_SAMPLE_MASK | (2 - 2));
	OUT_BATCH(1);

	/* Set system instruction pointer */
	OUT_BATCH(BRW_STATE_SIP | 0);
	OUT_BATCH(0);

	OUT_BATCH(GEN6_3DSTATE_URB | (3 - 2));
	OUT_BATCH(((1 - 1) << GEN6_3DSTATE_URB_VS_SIZE_SHIFT) |
		  (24 << GEN6_3DSTATE_URB_VS_ENTRIES_SHIFT)); /* at least 24 on GEN6 */
	OUT_BATCH((0 << GEN6_3DSTATE_URB_GS_SIZE_SHIFT) |
		(0 << GEN6_3DSTATE_URB_GS_ENTRIES_SHIFT)); /* no GS thread */
}

static void
gen6_composite_state_base_address(intel_screen_private *intel)
{
	OUT_BATCH(BRW_STATE_BASE_ADDRESS | (10 - 2));
	OUT_BATCH(BASE_ADDRESS_MODIFY); /* General state base address */
	intel->surface_reloc = intel->batch_used;
	intel_batch_emit_dword(intel,
			       intel->surface_bo->offset | BASE_ADDRESS_MODIFY);
	OUT_RELOC(intel->gen4_render_state->general_bo,
		  I915_GEM_DOMAIN_INSTRUCTION, 0,
		  BASE_ADDRESS_MODIFY);
	OUT_BATCH(BASE_ADDRESS_MODIFY); /* Indirect object base address */
	OUT_RELOC(intel->gen4_render_state->instruction_bo,
		  I915_GEM_DOMAIN_INSTRUCTION, 0,
		  BASE_ADDRESS_MODIFY);
	OUT_BATCH(BASE_ADDRESS_MODIFY); /* General state upper bound */
	OUT_BATCH(BASE_ADDRESS_MODIFY); /* Dynamic state upper bound */
	OUT_BATCH(BASE_ADDRESS_MODIFY); /* Indirect object upper bound */
	OUT_BATCH(BASE_ADDRESS_MODIFY); /* Instruction access upper bound */
}

static void
gen6_composite_viewport_state_pointers(intel_screen_private *intel,
				       uint32_t cc_vp)
{
	OUT_BATCH(GEN6_3DSTATE_VIEWPORT_STATE_POINTERS |
		  GEN6_3DSTATE_VIEWPORT_STATE_MODIFY_CC |
		  (4 - 2));
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(cc_vp);
}

static void
gen6_composite_cc_state_pointers(intel_screen_private *intel,
				 uint32_t blend_offset)
{
	struct gen4_render_state *render_state = intel->gen4_render_state;

	if (intel->gen6_render_state.blend == blend_offset)
		return;

	OUT_BATCH(GEN6_3DSTATE_CC_STATE_POINTERS | (4 - 2));
	OUT_BATCH((render_state->gen6_cc_blend + blend_offset) | 1);
	if (intel->gen6_render_state.blend == -1) {
		OUT_BATCH(render_state->gen6_cc_depth_stencil | 1);
		OUT_BATCH(render_state->gen6_cc_state | 1);
	} else {
		OUT_BATCH(0);
		OUT_BATCH(0);
	}

	intel->gen6_render_state.blend = blend_offset;
}

static void
gen6_composite_sampler_state_pointers(intel_screen_private *intel,
				      uint32_t state)
{
	if (intel->gen6_render_state.samplers == state)
		return;

	intel->gen6_render_state.samplers = state;

	OUT_BATCH(GEN6_3DSTATE_SAMPLER_STATE_POINTERS |
		  GEN6_3DSTATE_SAMPLER_STATE_MODIFY_PS |
		  (4 - 2));
	OUT_BATCH(0); /* VS */
	OUT_BATCH(0); /* GS */
	OUT_BATCH(state);
}

static void
gen6_composite_vs_state(intel_screen_private *intel)
{
	/* disable VS constant buffer */
	OUT_BATCH(GEN6_3DSTATE_CONSTANT_VS | (5 - 2));
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);

	OUT_BATCH(GEN6_3DSTATE_VS | (6 - 2));
	OUT_BATCH(0); /* without VS kernel */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0); /* pass-through */
}

static void
gen6_composite_gs_state(intel_screen_private *intel)
{
	/* disable GS constant buffer */
	OUT_BATCH(GEN6_3DSTATE_CONSTANT_GS | (5 - 2));
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);

	OUT_BATCH(GEN6_3DSTATE_GS | (7 - 2));
	OUT_BATCH(0); /* without GS kernel */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0); /* pass-through */
}

static void
gen6_composite_wm_constants(intel_screen_private *intel)
{
	/* disable WM constant buffer */
	OUT_BATCH(GEN6_3DSTATE_CONSTANT_PS | (5 - 2));
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
}

static void
gen6_composite_clip_state(intel_screen_private *intel)
{
	OUT_BATCH(GEN6_3DSTATE_CLIP | (4 - 2));
	OUT_BATCH(0);
	OUT_BATCH(0); /* pass-through */
	OUT_BATCH(0);
}

static void
gen6_composite_sf_state(intel_screen_private *intel,
			Bool has_mask)
{
	int num_sf_outputs = has_mask ? 2 : 1;

	if (intel->gen6_render_state.num_sf_outputs == num_sf_outputs)
		return;

	intel->gen6_render_state.num_sf_outputs = num_sf_outputs;

	OUT_BATCH(GEN6_3DSTATE_SF | (20 - 2));
	OUT_BATCH((num_sf_outputs << GEN6_3DSTATE_SF_NUM_OUTPUTS_SHIFT) |
		(1 << GEN6_3DSTATE_SF_URB_ENTRY_READ_LENGTH_SHIFT) |
		(1 << GEN6_3DSTATE_SF_URB_ENTRY_READ_OFFSET_SHIFT));
	OUT_BATCH(0);
	OUT_BATCH(GEN6_3DSTATE_SF_CULL_NONE);
	OUT_BATCH(2 << GEN6_3DSTATE_SF_TRIFAN_PROVOKE_SHIFT); /* DW4 */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0); /* DW9 */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0); /* DW14 */
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0); /* DW19 */
}

static void
gen6_composite_wm_state(intel_screen_private *intel,
			Bool has_mask,
			uint32_t kernel)
{
	int num_surfaces = has_mask ? 3 : 2;
	int num_sf_outputs = has_mask ? 2 : 1;

	if (intel->gen6_render_state.kernel == kernel)
		return;

	intel->gen6_render_state.kernel = kernel;

	OUT_BATCH(GEN6_3DSTATE_WM | (9 - 2));
	OUT_BATCH(kernel);
	OUT_BATCH((1 << GEN6_3DSTATE_WM_SAMPLER_COUNT_SHITF) |
		(num_surfaces << GEN6_3DSTATE_WM_BINDING_TABLE_ENTRY_COUNT_SHIFT));
	OUT_BATCH(0);
	OUT_BATCH((6 << GEN6_3DSTATE_WM_DISPATCH_START_GRF_0_SHIFT)); /* DW4 */
	OUT_BATCH(((40 - 1) << GEN6_3DSTATE_WM_MAX_THREADS_SHIFT) |
		  GEN6_3DSTATE_WM_DISPATCH_ENABLE |
		  GEN6_3DSTATE_WM_16_DISPATCH_ENABLE);
	OUT_BATCH((num_sf_outputs << GEN6_3DSTATE_WM_NUM_SF_OUTPUTS_SHIFT) |
		  GEN6_3DSTATE_WM_PERSPECTIVE_PIXEL_BARYCENTRIC);
	OUT_BATCH(0);
	OUT_BATCH(0);
}

static void
gen6_composite_binding_table_pointers(intel_screen_private *intel)
{
	/* Binding table pointers */
	OUT_BATCH(BRW_3DSTATE_BINDING_TABLE_POINTERS |
		  GEN6_3DSTATE_BINDING_TABLE_MODIFY_PS |
		  (4 - 2));
	OUT_BATCH(0);		/* vs */
	OUT_BATCH(0);		/* gs */
	/* Only the PS uses the binding table */
	OUT_BATCH(intel->surface_table);
}

static void
gen6_composite_depth_buffer_state(intel_screen_private *intel)
{
	OUT_BATCH(BRW_3DSTATE_DEPTH_BUFFER | (7 - 2));
	OUT_BATCH((BRW_SURFACE_NULL << BRW_3DSTATE_DEPTH_BUFFER_TYPE_SHIFT) |
		(BRW_DEPTHFORMAT_D32_FLOAT << BRW_3DSTATE_DEPTH_BUFFER_FORMAT_SHIFT));
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);
	OUT_BATCH(0);

	OUT_BATCH(BRW_3DSTATE_CLEAR_PARAMS | (2 - 2));
	OUT_BATCH(0);
}

static void
gen6_composite_drawing_rectangle(intel_screen_private *intel,
				 PixmapPtr dest)

{
	uint32_t dw =
		DRAW_YMAX(dest->drawable.height - 1) |
		DRAW_XMAX(dest->drawable.width - 1);

	/* XXX cacomposite depends upon the implicit non-pipelined flush */
	if (0 && intel->gen6_render_state.drawrect == dw)
		return;
	intel->gen6_render_state.drawrect = dw;

	OUT_BATCH(BRW_3DSTATE_DRAWING_RECTANGLE | (4 - 2));
	OUT_BATCH(0x00000000);	/* ymin, xmin */
	OUT_BATCH(dw);	/* ymax, xmax */
	OUT_BATCH(0x00000000);	/* yorigin, xorigin */
}

static void
gen6_composite_vertex_element_state(intel_screen_private *intel,
				    Bool has_mask,
				    Bool is_affine)
{
	/*
	 * vertex data in vertex buffer
	 *    position: (x, y)
	 *    texture coordinate 0: (u0, v0) if (is_affine is TRUE) else (u0, v0, w0)
	 *    texture coordinate 1 if (has_mask is TRUE): same as above
	 */
	gen4_composite_op *composite_op = &intel->gen4_render_state->composite_op;
	int nelem = has_mask ? 2 : 1;
	int selem = is_affine ? 2 : 3;
	uint32_t w_component;
	uint32_t src_format;
	int id;

	id = has_mask << 1 | is_affine;

	if (composite_op->vertex_id == id)
		return;

	composite_op->vertex_id = id;

	if (is_affine) {
		src_format = BRW_SURFACEFORMAT_R32G32_FLOAT;
		w_component = BRW_VFCOMPONENT_STORE_1_FLT;
	} else {
		src_format = BRW_SURFACEFORMAT_R32G32B32_FLOAT;
		w_component = BRW_VFCOMPONENT_STORE_SRC;
	}

	/* The VUE layout
	 *    dword 0-3: pad (0.0, 0.0, 0.0. 0.0)
	 *    dword 4-7: position (x, y, 1.0, 1.0),
	 *    dword 8-11: texture coordinate 0 (u0, v0, w0, 1.0)
	 *    dword 12-15: texture coordinate 1 (u1, v1, w1, 1.0)
	 *
	 * dword 4-15 are fetched from vertex buffer
	 */
	OUT_BATCH(BRW_3DSTATE_VERTEX_ELEMENTS |
		((2 * (2 + nelem)) + 1 - 2));

	OUT_BATCH((id << GEN6_VE0_VERTEX_BUFFER_INDEX_SHIFT) |
		  GEN6_VE0_VALID |
		  (BRW_SURFACEFORMAT_R32G32_FLOAT << VE0_FORMAT_SHIFT) |
		  (0 << VE0_OFFSET_SHIFT));
	OUT_BATCH((BRW_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_0_SHIFT) |
		  (BRW_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_1_SHIFT) |
		  (BRW_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_2_SHIFT) |
		  (BRW_VFCOMPONENT_STORE_0 << VE1_VFCOMPONENT_3_SHIFT));

	/* x,y */
	OUT_BATCH((id << GEN6_VE0_VERTEX_BUFFER_INDEX_SHIFT) |
		  GEN6_VE0_VALID |
		  (BRW_SURFACEFORMAT_R32G32_FLOAT << VE0_FORMAT_SHIFT) |
		  (0 << VE0_OFFSET_SHIFT)); /* offsets vb in bytes */
	OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
		  (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
		  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_2_SHIFT) |
		  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT));

	/* u0, v0, w0 */
	OUT_BATCH((id << GEN6_VE0_VERTEX_BUFFER_INDEX_SHIFT) |
		  GEN6_VE0_VALID |
		  (src_format << VE0_FORMAT_SHIFT) |
		  ((2 * 4) << VE0_OFFSET_SHIFT));	/* offset vb in bytes */
	OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
		  (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
		  (w_component << VE1_VFCOMPONENT_2_SHIFT) |
		  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT));

	/* u1, v1, w1 */
	if (has_mask) {
		OUT_BATCH((id << GEN6_VE0_VERTEX_BUFFER_INDEX_SHIFT) |
			  GEN6_VE0_VALID |
			  (src_format << VE0_FORMAT_SHIFT) |
			  (((2 + selem) * 4) << VE0_OFFSET_SHIFT)); /* vb offset in bytes */
		OUT_BATCH((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
			  (w_component << VE1_VFCOMPONENT_2_SHIFT) |
			  (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT));
	}
}

static void
gen6_emit_composite_state(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct gen4_render_state *render_state = intel->gen4_render_state;
	gen4_composite_op *composite_op = &render_state->composite_op;
	int op = composite_op->op;
	PicturePtr mask_picture = intel->render_mask_picture;
	PicturePtr dest_picture = intel->render_dest_picture;
	PixmapPtr mask = intel->render_mask;
	PixmapPtr dest = intel->render_dest;
	sampler_state_filter_t src_filter = composite_op->src_filter;
	sampler_state_filter_t mask_filter = composite_op->mask_filter;
	sampler_state_extend_t src_extend = composite_op->src_extend;
	sampler_state_extend_t mask_extend = composite_op->mask_extend;
	Bool is_affine = composite_op->is_affine;
	uint32_t src_blend, dst_blend;

	intel->needs_render_state_emit = FALSE;
	if (intel->needs_3d_invariant) {
		gen6_composite_invariant_states(intel);
		intel->needs_3d_invariant = FALSE;
	}

	i965_get_blend_cntl(op,
			    mask_picture,
			    dest_picture->format,
			    &src_blend,
			    &dst_blend);

	if (intel->surface_reloc == 0) {
		gen6_composite_state_base_address(intel);

		/* need to reload all relocations after modifying base */
		gen6_composite_viewport_state_pointers(intel, render_state->gen6_cc_vp);
		gen6_composite_vs_state(intel);
		gen6_composite_gs_state(intel);
		gen6_composite_clip_state(intel);
		gen6_composite_wm_constants(intel);
		gen6_composite_depth_buffer_state(intel);
	}

	gen6_composite_cc_state_pointers(intel,
					((src_blend * BRW_BLENDFACTOR_COUNT) + dst_blend) * GEN6_BLEND_STATE_PADDED_SIZE);
	gen6_composite_sampler_state_pointers(intel,
					      render_state->wm_state[0][src_filter][src_extend][mask_filter][mask_extend]);
	gen6_composite_sf_state(intel, mask != 0);
	gen6_composite_wm_state(intel,
				mask != 0,
				render_state->wm_kernel[composite_op->wm_kernel]);
	gen6_composite_binding_table_pointers(intel);

	gen6_composite_drawing_rectangle(intel, dest);
	gen6_composite_vertex_element_state(intel, mask != 0, is_affine);
}

static void
gen6_render_state_init(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct gen4_render_state *render;
	struct i965_static_stream instruction, dynamic;
	uint32_t border_color;
	int i, j, k, l, m;

	intel->gen6_render_state.num_sf_outputs = 0;
	intel->gen6_render_state.samplers = -1;
	intel->gen6_render_state.blend = -1;
	intel->gen6_render_state.kernel = -1;
	intel->gen6_render_state.vertex_size = 0;
	intel->gen6_render_state.vertex_type = 0;
	intel->gen6_render_state.drawrect = -1;

	if (intel->gen4_render_state == NULL)
		intel->gen4_render_state = calloc(sizeof(*render), 1);

	i965_static_stream_init(&instruction);
	i965_static_stream_init(&dynamic);

	render = intel->gen4_render_state;

	for (m = 0; m < WM_KERNEL_COUNT; m++) {
		render->wm_kernel[m] =
			i965_static_stream_add(&instruction,
					       wm_kernels_gen6[m].data,
					       wm_kernels_gen6[m].size,
					       64);
	}

	border_color = sampler_border_color_create(&dynamic);
	for (i = 0; i < SAMPLER_STATE_FILTER_COUNT; i++) {
		for (j = 0; j < SAMPLER_STATE_EXTEND_COUNT; j++) {
			for (k = 0; k < SAMPLER_STATE_FILTER_COUNT; k++) {
				for (l = 0; l < SAMPLER_STATE_EXTEND_COUNT; l++) {
					render->wm_state[0][i][j][k][l] =
						gen4_create_sampler_state(&dynamic,
									  i, j,
									  k, l,
									  border_color);
				}
			}
		}
	}

	render->gen6_cc_vp = gen4_create_cc_viewport(&dynamic);
	render->gen6_cc_state = gen6_composite_create_cc_state(&dynamic);
	render->gen6_cc_blend = gen6_composite_create_blend_state(&dynamic);
	render->gen6_cc_depth_stencil = gen6_composite_create_depth_stencil_state(&dynamic);

	render->instruction_bo = i965_static_stream_fini(intel, &instruction);
	render->general_bo = i965_static_stream_fini(intel, &dynamic);
}

void i965_vertex_flush(struct intel_screen_private *intel)
{
	if (intel->vertex_offset) {
		intel->batch_ptr[intel->vertex_offset] =
			intel->vertex_index - intel->vertex_count;
		intel->vertex_offset = 0;
	}
}

void i965_batch_flush(struct intel_screen_private *intel)
{
	if (intel->surface_used)
		i965_surface_flush(intel);
}
