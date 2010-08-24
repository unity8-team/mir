/*
 * Copyright © 2006 Intel Corporation
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"

#include "ums.h"
#include "ums_i915_reg.h"
#include "ums_i915_3d.h"

#include "intel_transform.h"

struct formatinfo {
    int fmt;
    uint32_t card_fmt;
};

struct blendinfo {
    Bool dst_alpha;
    Bool src_alpha;
    uint32_t src_blend;
    uint32_t dst_blend;
};

static struct blendinfo i915_blend_op[] = {
    /* Clear */
    {0, 0, BLENDFACT_ZERO,          BLENDFACT_ZERO},
    /* Src */
    {0, 0, BLENDFACT_ONE,           BLENDFACT_ZERO},
    /* Dst */
    {0, 0, BLENDFACT_ZERO,          BLENDFACT_ONE},
    /* Over */
    {0, 1, BLENDFACT_ONE,           BLENDFACT_INV_SRC_ALPHA},
    /* OverReverse */
    {1, 0, BLENDFACT_INV_DST_ALPHA, BLENDFACT_ONE},
    /* In */
    {1, 0, BLENDFACT_DST_ALPHA,     BLENDFACT_ZERO},
    /* InReverse */
    {0, 1, BLENDFACT_ZERO,          BLENDFACT_SRC_ALPHA},
    /* Out */
    {1, 0, BLENDFACT_INV_DST_ALPHA, BLENDFACT_ZERO},
    /* OutReverse */
    {0, 1, BLENDFACT_ZERO,          BLENDFACT_INV_SRC_ALPHA},
    /* Atop */
    {1, 1, BLENDFACT_DST_ALPHA,     BLENDFACT_INV_SRC_ALPHA},
    /* AtopReverse */
    {1, 1, BLENDFACT_INV_DST_ALPHA, BLENDFACT_SRC_ALPHA},
    /* Xor */
    {1, 1, BLENDFACT_INV_DST_ALPHA, BLENDFACT_INV_SRC_ALPHA},
    /* Add */
    {0, 0, BLENDFACT_ONE,           BLENDFACT_ONE},
};

static struct formatinfo i915_tex_formats[] = {
    {PICT_a8, MAPSURF_8BIT | MT_8BIT_A8},
    {PICT_a8r8g8b8, MAPSURF_32BIT | MT_32BIT_ARGB8888},
    {PICT_x8r8g8b8, MAPSURF_32BIT | MT_32BIT_XRGB8888},
    {PICT_a8b8g8r8, MAPSURF_32BIT | MT_32BIT_ABGR8888},
    {PICT_x8b8g8r8, MAPSURF_32BIT | MT_32BIT_XBGR8888},
    {PICT_a2r10g10b10, MAPSURF_32BIT | MT_32BIT_ARGB2101010},
    {PICT_a2b10g10r10, MAPSURF_32BIT | MT_32BIT_ABGR2101010},
    {PICT_r5g6b5, MAPSURF_16BIT | MT_16BIT_RGB565},
    {PICT_a1r5g5b5, MAPSURF_16BIT | MT_16BIT_ARGB1555},
    {PICT_a4r4g4b4, MAPSURF_16BIT | MT_16BIT_ARGB4444},
};

static uint32_t i915_get_blend_cntl(int op, PicturePtr mask,
				    uint32_t dst_format)
{
    uint32_t sblend, dblend;

    sblend = i915_blend_op[op].src_blend;
    dblend = i915_blend_op[op].dst_blend;

    /* If there's no dst alpha channel, adjust the blend op so that we'll treat
     * it as always 1.
     */
    if (PICT_FORMAT_A(dst_format) == 0 && i915_blend_op[op].dst_alpha) {
        if (sblend == BLENDFACT_DST_ALPHA)
            sblend = BLENDFACT_ONE;
        else if (sblend == BLENDFACT_INV_DST_ALPHA)
            sblend = BLENDFACT_ZERO;
    }

    /* i915 engine reads 8bit color buffer into green channel in cases
       like color buffer blending .etc, and also writes back green channel.
       So with dst_alpha blend we should use color factor. See spec on
       "8-bit rendering" */
    if ((dst_format == PICT_a8) && i915_blend_op[op].dst_alpha) {
        if (sblend == BLENDFACT_DST_ALPHA)
            sblend = BLENDFACT_DST_COLR;
        else if (sblend == BLENDFACT_INV_DST_ALPHA)
            sblend = BLENDFACT_INV_DST_COLR;
    }

    /* If the source alpha is being used, then we should only be in a case
     * where the source blend factor is 0, and the source blend value is the
     * mask channels multiplied by the source picture's alpha.
     */
    if (mask && mask->componentAlpha && PICT_FORMAT_RGB(mask->format) &&
	i915_blend_op[op].src_alpha)
    {
        if (dblend == BLENDFACT_SRC_ALPHA) {
	    dblend = BLENDFACT_SRC_COLR;
        } else if (dblend == BLENDFACT_INV_SRC_ALPHA) {
	    dblend = BLENDFACT_INV_SRC_COLR;
        }
    }

    return (S6_CBUF_BLEND_ENABLE | S6_COLOR_WRITE_ENABLE |
	    (BLENDFUNC_ADD << S6_CBUF_BLEND_FUNC_SHIFT) |
	    (sblend << S6_CBUF_SRC_BLEND_FACT_SHIFT) |
	    (dblend << S6_CBUF_DST_BLEND_FACT_SHIFT));
}

#define DSTORG_HORT_BIAS(x)             ((x)<<20)
#define DSTORG_VERT_BIAS(x)             ((x)<<16)

static Bool i915_get_dest_format(PicturePtr dest_picture, uint32_t *dst_format)
{
    ScrnInfoPtr pScrn = xf86Screens[dest_picture->pDrawable->pScreen->myNum];

    switch (dest_picture->format) {
    case PICT_a8r8g8b8:
    case PICT_x8r8g8b8:
	*dst_format = COLR_BUF_ARGB8888;
	break;
    case PICT_r5g6b5:
	*dst_format = COLR_BUF_RGB565;
	break;
    case PICT_a1r5g5b5:
    case PICT_x1r5g5b5:
	*dst_format = COLR_BUF_ARGB1555;
	break;
    case PICT_a2r10g10b10:
    case PICT_x2r10g10b10:
	*dst_format = COLR_BUF_ARGB2AAA;
	break;
    case PICT_a8:
	*dst_format = COLR_BUF_8BIT;
	break;
    case PICT_a4r4g4b4:
    case PICT_x4r4g4b4:
	*dst_format = COLR_BUF_ARGB4444;
	break;
    default:
	I830FALLBACK("Unsupported dest format 0x%x\n",
		     (int)dest_picture->format);
    }

    *dst_format |= DSTORG_HORT_BIAS(0x8) | DSTORG_VERT_BIAS(0x8);
    return TRUE;
}

static Bool i915_check_composite_texture(PicturePtr pPict, int unit)
{
    ScrnInfoPtr pScrn;
    int w, h, i;

    if (! pPict->pDrawable)
	return FALSE;

    pScrn = xf86Screens[pPict->pDrawable->pScreen->myNum];
    w = pPict->pDrawable->width;
    h = pPict->pDrawable->height;

    if ((w > 2048) || (h > 2048))
        I830FALLBACK("Picture w/h too large (%dx%d)\n", w, h);

    for (i = 0; i < sizeof(i915_tex_formats) / sizeof(i915_tex_formats[0]); i++)
        if (i915_tex_formats[i].fmt == pPict->format)
            break;
    if (i == sizeof(i915_tex_formats) / sizeof(i915_tex_formats[0]))
        I830FALLBACK("Unsupported picture format 0x%x\n",
		     (int)pPict->format);

    if (pPict->repeatType > RepeatReflect)
        I830FALLBACK("Unsupported picture repeat %d\n", pPict->repeatType);

    if (pPict->filter != PictFilterNearest &&
        pPict->filter != PictFilterBilinear)
        I830FALLBACK("Unsupported filter 0x%x\n", pPict->filter);

    return TRUE;
}

Bool
ums_i915_check_composite(int op,
			    PicturePtr source_picture,
			    PicturePtr mask_picture,
			    PicturePtr dest_picture)
{
    ScrnInfoPtr pScrn = xf86Screens[dest_picture->pDrawable->pScreen->myNum];
    uint32_t tmp1;

    /* Check for unsupported compositing operations. */
    if (op >= sizeof(i915_blend_op) / sizeof(i915_blend_op[0]))
        I830FALLBACK("Unsupported Composite op 0x%x\n", op);
    if (mask_picture != NULL && mask_picture->componentAlpha &&
	PICT_FORMAT_RGB(mask_picture->format))
    {
        /* Check if it's component alpha that relies on a source alpha and on
         * the source value.  We can only get one of those into the single
         * source value that we get to blend with.
         */
        if (i915_blend_op[op].src_alpha &&
            (i915_blend_op[op].src_blend != BLENDFACT_ZERO))
            	I830FALLBACK("Component alpha not supported with source "
			     "alpha and source value blending.\n");
    }

    if (!i915_check_composite_texture(source_picture, 0))
        I830FALLBACK("Check Src picture texture\n");
    if (mask_picture != NULL && !i915_check_composite_texture(mask_picture, 1))
        I830FALLBACK("Check Mask picture texture\n");

    if (!i915_get_dest_format(dest_picture, &tmp1))
	I830FALLBACK("Get Color buffer format\n");

    return TRUE;
}

static Bool i915_texture_setup(PicturePtr pPict, PixmapPtr pPix, int unit)
{
    ScrnInfoPtr pScrn = xf86Screens[pPict->pDrawable->pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    uint32_t format, filter, wrap;
    int w, h, i;

    w = pPict->pDrawable->width;
    h = pPict->pDrawable->height;
    pI830->scale_units[unit][0] = 1. / w;
    pI830->scale_units[unit][1] = 1. / h;

    for (i = 0; i < sizeof(i915_tex_formats) / sizeof(i915_tex_formats[0]); i++)
        if (i915_tex_formats[i].fmt == pPict->format)
	    break;
    if (i == sizeof(i915_tex_formats)/ sizeof(i915_tex_formats[0]))
	I830FALLBACK("unknown texture format\n");
    format = i915_tex_formats[i].card_fmt;

    switch (pPict->repeatType) {
    case RepeatNone:
	wrap = TEXCOORDMODE_CLAMP_BORDER;
	break;
    case RepeatNormal:
	wrap = TEXCOORDMODE_WRAP;
	break;
    case RepeatPad:
	wrap = TEXCOORDMODE_CLAMP_EDGE;
	break;
    case RepeatReflect:
	wrap = TEXCOORDMODE_MIRROR;
	break;
    default:
	I830FALLBACK("Unkown repeat type %d\n", pPict->repeatType);
    }

    switch (pPict->filter) {
    case PictFilterNearest:
	filter = ((FILTER_NEAREST << SS2_MAG_FILTER_SHIFT) |
		  (FILTER_NEAREST << SS2_MIN_FILTER_SHIFT));
        break;
    case PictFilterBilinear:
	filter = ((FILTER_LINEAR << SS2_MAG_FILTER_SHIFT) |
		  (FILTER_LINEAR << SS2_MIN_FILTER_SHIFT));
        break;
    default:
        I830FALLBACK("Bad filter 0x%x\n", pPict->filter);
    }

    pI830->mapstate[unit * 3 + 0] = 0;
    pI830->mapstate[unit * 3 + 1] = (format | MS3_USE_FENCE_REGS |
				     ((h - 1) << MS3_HEIGHT_SHIFT) |
				     ((w - 1) << MS3_WIDTH_SHIFT));
    pI830->mapstate[unit * 3 + 2] = ((ums_get_pixmap_pitch(pPix) / 4) - 1) << MS4_PITCH_SHIFT;

    pI830->samplerstate[unit * 3 + 0] = (MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT);
    pI830->samplerstate[unit * 3 + 0] |= filter;
    pI830->samplerstate[unit * 3 + 1] = SS3_NORMALIZED_COORDS;
    pI830->samplerstate[unit * 3 + 1] |= wrap << SS3_TCX_ADDR_MODE_SHIFT;
    pI830->samplerstate[unit * 3 + 1] |= wrap << SS3_TCY_ADDR_MODE_SHIFT;
    pI830->samplerstate[unit * 3 + 1] |= unit << SS3_TEXTUREMAP_INDEX_SHIFT;
    pI830->samplerstate[unit * 3 + 2] = 0x00000000; /* border color */

    pI830->transform[unit] = pPict->transform;

    return TRUE;
}

Bool
ums_i915_prepare_composite(int op, PicturePtr source_picture,
			      PicturePtr mask_picture, PicturePtr dest_picture,
			      PixmapPtr source, PixmapPtr mask, PixmapPtr dest)
{
    ScrnInfoPtr pScrn = xf86Screens[source_picture->pDrawable->pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->render_source_picture = source_picture;
    pI830->render_source = source;
    pI830->render_mask_picture = mask_picture;
    pI830->render_mask = mask;
    pI830->render_dest_picture = dest_picture;
    pI830->render_dest = dest;

    ums_exa_check_pitch_3d(source);
    if (mask)
	ums_exa_check_pitch_3d(mask);
    ums_exa_check_pitch_3d(dest);

    if (!i915_get_dest_format(dest_picture,
			      &pI830->render_dest_format))
	return FALSE;

    pI830->transform[0] = NULL;
    pI830->scale_units[0][0] = -1;
    pI830->scale_units[0][1] = -1;
    pI830->transform[1] = NULL;
    pI830->scale_units[1][0] = -1;
    pI830->scale_units[1][1] = -1;

    if (!i915_texture_setup(source_picture, source, 0))
	I830FALLBACK("fail to setup src texture\n");

    if (mask_picture && !i915_texture_setup(mask_picture, mask, 1))
	I830FALLBACK("fail to setup mask texture\n");

    pI830->render_op = op;
    pI830->needs_render_state_emit = TRUE;

    return TRUE;
}

static void
i915_emit_composite_setup(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    PicturePtr mask_picture = pI830->render_mask_picture;
    PicturePtr dest_picture = pI830->render_dest_picture;
    PixmapPtr source = pI830->render_source;
    PixmapPtr mask = pI830->render_mask;
    Bool dest_is_alpha = PIXMAN_FORMAT_RGB(dest_picture->format) == 0;
    int out_reg = FS_OC;
    Bool is_affine_src, is_affine_mask;

    pI830->needs_render_state_emit = FALSE;

    ums_IntelEmitInvarientState(pScrn);
    pI830->last_3d = LAST_3D_RENDER;

    is_affine_src = intel_transform_is_affine (pI830->transform[0]);
    is_affine_mask = intel_transform_is_affine (pI830->transform[1]);

    if (mask == NULL) {
	OUT_BATCH(_3DSTATE_MAP_STATE | 3);
	OUT_BATCH(0x00000001); /* map 0 */
	OUT_RELOC_PIXMAP(source, I915_GEM_DOMAIN_SAMPLER, 0, 0);
	OUT_BATCH(pI830->mapstate[1]);
	OUT_BATCH(pI830->mapstate[2]);

	OUT_BATCH(_3DSTATE_SAMPLER_STATE | 3);
	OUT_BATCH(0x00000001); /* sampler 0 */
	OUT_BATCH(pI830->samplerstate[0]);
	OUT_BATCH(pI830->samplerstate[1]);
	OUT_BATCH(0);
    } else {
	OUT_BATCH(_3DSTATE_MAP_STATE | 6);
	OUT_BATCH(0x00000003); /* map 0,1 */
	OUT_RELOC_PIXMAP(source, I915_GEM_DOMAIN_SAMPLER, 0, 0);
	OUT_BATCH(pI830->mapstate[1]);
	OUT_BATCH(pI830->mapstate[2]);
	OUT_RELOC_PIXMAP(mask, I915_GEM_DOMAIN_SAMPLER, 0, 0);
	OUT_BATCH(pI830->mapstate[4]);
	OUT_BATCH(pI830->mapstate[5]);

	OUT_BATCH(_3DSTATE_SAMPLER_STATE | 6);
	OUT_BATCH(0x00000003); /* sampler 0,1 */
	OUT_BATCH(pI830->samplerstate[0]);
	OUT_BATCH(pI830->samplerstate[1]);
	OUT_BATCH(0);
	OUT_BATCH(pI830->samplerstate[3]);
	OUT_BATCH(pI830->samplerstate[4]);
	OUT_BATCH(0);
    }
    {
	PixmapPtr dest = pI830->render_dest;
	uint32_t ss2;

	OUT_BATCH(_3DSTATE_BUF_INFO_CMD);
	OUT_BATCH(BUF_3D_ID_COLOR_BACK| BUF_3D_USE_FENCE|
		  BUF_3D_PITCH(ums_get_pixmap_pitch(dest)));

	OUT_RELOC_PIXMAP(dest, I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER, 0);

	OUT_BATCH(_3DSTATE_DST_BUF_VARS_CMD);
	OUT_BATCH(pI830->render_dest_format);

	OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(2) |
		  I1_LOAD_S(4) | I1_LOAD_S(5) | I1_LOAD_S(6) | 3);
	ss2 = S2_TEXCOORD_FMT(0, is_affine_src ? TEXCOORDFMT_2D : TEXCOORDFMT_4D);
	if (mask)
		ss2 |= S2_TEXCOORD_FMT(1, is_affine_mask ? TEXCOORDFMT_2D : TEXCOORDFMT_4D);
	else
		ss2 |= S2_TEXCOORD_FMT(1, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(2, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(3, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(4, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(5, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(6, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(7, TEXCOORDFMT_NOT_PRESENT);
	OUT_BATCH(ss2);
	OUT_BATCH((1 << S4_POINT_WIDTH_SHIFT) | S4_LINE_WIDTH_ONE |
		  S4_CULLMODE_NONE| S4_VFMT_XY);
	OUT_BATCH(0x00000000); /* Disable stencil buffer */
	OUT_BATCH(i915_get_blend_cntl(pI830->render_op, mask_picture, dest_picture->format));

	/* draw rect is unconditional */
	OUT_BATCH(_3DSTATE_DRAW_RECT_CMD);
	OUT_BATCH(0x00000000);
	OUT_BATCH(0x00000000);  /* ymin, xmin*/
	OUT_BATCH(DRAW_YMAX(dest->drawable.height - 1) |
		  DRAW_XMAX(dest->drawable.width - 1));
	OUT_BATCH(0x00000000);  /* yorig, xorig (relate to color buffer?)*/
	OUT_BATCH(MI_NOOP);
    }

    if (dest_is_alpha)
	out_reg = FS_U0;

    FS_BEGIN();

    /* Declare the registers necessary for our program.  I don't think the
     * S then T ordering is necessary.
     */
    i915_fs_dcl(FS_S0);
    if (mask)
	i915_fs_dcl(FS_S1);
    i915_fs_dcl(FS_T0);
    if (mask)
	i915_fs_dcl(FS_T1);

    /* Load the source_picture texel */
    if (is_affine_src) {
	i915_fs_texld(FS_R0, FS_S0, FS_T0);
    } else {
	i915_fs_texldp(FS_R0, FS_S0, FS_T0);
    }

    if (!mask) {
	/* No mask, so move to output color */
	i915_fs_mov(out_reg, i915_fs_operand_reg(FS_R0));
    } else {
	/* Load the mask_picture texel */
	if (is_affine_mask) {
	    i915_fs_texld(FS_R1, FS_S1, FS_T1);
	} else {
	    i915_fs_texldp(FS_R1, FS_S1, FS_T1);
	}

	/* If component alpha is active in the mask and the blend operation
	 * uses the source alpha, then we know we don't need the source
	 * value (otherwise we would have hit a fallback earlier), so we
	 * provide the source alpha (src.A * mask.X) as output color.
	 * Conversely, if CA is set and we don't need the source alpha, then
	 * we produce the source value (src.X * mask.X) and the source alpha
	 * is unused..  Otherwise, we provide the non-CA source value
	 * (src.X * mask.A).
	 */
	if (mask_picture->componentAlpha &&
	    PICT_FORMAT_RGB(mask_picture->format))
	{
	    if (i915_blend_op[pI830->render_op].src_alpha) {
		i915_fs_mul(out_reg, i915_fs_operand(FS_R0, W, W, W, W),
			    i915_fs_operand_reg(FS_R1));
	    } else {
		i915_fs_mul(out_reg, i915_fs_operand_reg(FS_R0),
			    i915_fs_operand_reg(FS_R1));
	    }
	} else {
	    i915_fs_mul(out_reg, i915_fs_operand_reg(FS_R0),
			i915_fs_operand(FS_R1, W, W, W, W));
	}
    }
    if (dest_is_alpha)
	i915_fs_mov(FS_OC, i915_fs_operand(out_reg, W, W, W, W));

    FS_END();
}

static void
i915_emit_composite_primitive(PixmapPtr dest,
			      int srcX, int srcY,
			      int maskX, int maskY,
			      int dstX, int dstY,
			      int w, int h)
{
	ScrnInfoPtr pScrn = xf86Screens[dest->drawable.pScreen->myNum];
	I830Ptr pI830 = I830PTR(pScrn);
	Bool is_affine_src, is_affine_mask = TRUE;
	float src_x[3], src_y[3], src_w[3], mask_x[3], mask_y[3], mask_w[3];
	int per_vertex;

	per_vertex = 2;		/* dest x/y */

	is_affine_src = intel_transform_is_affine(pI830->transform[0]);
	if (is_affine_src) {
		if (!intel_get_transformed_coordinates(srcX, srcY,
						      pI830->
						      transform[0],
						      &src_x[0],
						      &src_y[0]))
			return;

		if (!intel_get_transformed_coordinates(srcX, srcY + h,
						      pI830->
						      transform[0],
						      &src_x[1],
						      &src_y[1]))
			return;

		if (!intel_get_transformed_coordinates(srcX + w, srcY + h,
						      pI830->
						      transform[0],
						      &src_x[2],
						      &src_y[2]))
			return;

		per_vertex += 2;	/* src x/y */
	} else {
		if (!intel_get_transformed_coordinates_3d(srcX, srcY,
							 pI830->
							 transform[0],
							 &src_x[0],
							 &src_y[0],
							 &src_w[0]))
			return;

		if (!intel_get_transformed_coordinates_3d(srcX, srcY + h,
							 pI830->
							 transform[0],
							 &src_x[1],
							 &src_y[1],
							 &src_w[1]))
			return;

		if (!intel_get_transformed_coordinates_3d(srcX + w, srcY + h,
							 pI830->
							 transform[0],
							 &src_x[2],
							 &src_y[2],
							 &src_w[2]))
			return;

		per_vertex += 4;	/* src x/y/z/w */
	}

	if (pI830->render_mask) {
		is_affine_mask = intel_transform_is_affine(pI830->transform[1]);
		if (is_affine_mask) {
			if (!intel_get_transformed_coordinates(maskX, maskY,
							      pI830->
							      transform[1],
							      &mask_x[0],
							      &mask_y[0]))
				return;

			if (!intel_get_transformed_coordinates(maskX, maskY + h,
							      pI830->
							      transform[1],
							      &mask_x[1],
							      &mask_y[1]))
				return;

			if (!intel_get_transformed_coordinates(maskX + w, maskY + h,
							      pI830->
							      transform[1],
							      &mask_x[2],
							      &mask_y[2]))
				return;

			per_vertex += 2;	/* mask x/y */
		} else {
			if (!intel_get_transformed_coordinates_3d(maskX, maskY,
								 pI830->
								 transform[1],
								 &mask_x[0],
								 &mask_y[0],
								 &mask_w[0]))
				return;

			if (!intel_get_transformed_coordinates_3d(maskX, maskY + h,
								 pI830->
								 transform[1],
								 &mask_x[1],
								 &mask_y[1],
								 &mask_w[1]))
				return;

			if (!intel_get_transformed_coordinates_3d(maskX + w, maskY + h,
								 pI830->
								 transform[1],
								 &mask_x[2],
								 &mask_y[2],
								 &mask_w[2]))
				return;

			per_vertex += 4;	/* mask x/y/z/w */
		}
	}

	OUT_BATCH(PRIM3D_INLINE | PRIM3D_RECTLIST | (3*per_vertex-1));
	OUT_BATCH_F(dstX + w);
	OUT_BATCH_F(dstY + h);
	OUT_BATCH_F(src_x[2] * pI830->scale_units[0][0]);
	OUT_BATCH_F(src_y[2] * pI830->scale_units[0][1]);
	if (!is_affine_src) {
	    OUT_BATCH_F(0.0);
	    OUT_BATCH_F(src_w[2]);
	}
	if (pI830->render_mask) {
		OUT_BATCH_F(mask_x[2] * pI830->scale_units[1][0]);
		OUT_BATCH_F(mask_y[2] * pI830->scale_units[1][1]);
		if (!is_affine_mask) {
			OUT_BATCH_F(0.0);
			OUT_BATCH_F(mask_w[2]);
		}
	}

	OUT_BATCH_F(dstX);
	OUT_BATCH_F(dstY + h);
	OUT_BATCH_F(src_x[1] * pI830->scale_units[0][0]);
	OUT_BATCH_F(src_y[1] * pI830->scale_units[0][1]);
	if (!is_affine_src) {
	    OUT_BATCH_F(0.0);
	    OUT_BATCH_F(src_w[1]);
	}
	if (pI830->render_mask) {
		OUT_BATCH_F(mask_x[1] * pI830->scale_units[1][0]);
		OUT_BATCH_F(mask_y[1] * pI830->scale_units[1][1]);
		if (!is_affine_mask) {
			OUT_BATCH_F(0.0);
			OUT_BATCH_F(mask_w[1]);
		}
	}

	OUT_BATCH_F(dstX);
	OUT_BATCH_F(dstY);
	OUT_BATCH_F(src_x[0] * pI830->scale_units[0][0]);
	OUT_BATCH_F(src_y[0] * pI830->scale_units[0][1]);
	if (!is_affine_src) {
	    OUT_BATCH_F(0.0);
	    OUT_BATCH_F(src_w[0]);
	}
	if (pI830->render_mask) {
		OUT_BATCH_F(mask_x[0] * pI830->scale_units[1][0]);
		OUT_BATCH_F(mask_y[0] * pI830->scale_units[1][1]);
		if (!is_affine_mask) {
			OUT_BATCH_F(0.0);
			OUT_BATCH_F(mask_w[0]);
		}
	}
}

void
ums_i915_composite(PixmapPtr dest,
		      int srcX, int srcY,
		      int maskX, int maskY,
		      int dstX, int dstY,
		      int w, int h)
{
    ScrnInfoPtr pScrn = xf86Screens[dest->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    ums_batch_start_atomic(pScrn, 150);

    if (pI830->needs_render_state_emit)
	i915_emit_composite_setup(pScrn);

    i915_emit_composite_primitive(dest,
				  srcX, srcY,
				  maskX, maskY,
				  dstX, dstY,
				  w, h);

    ums_batch_end_atomic(pScrn);
}

void
ums_i915_batch_flush_notify(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->needs_render_state_emit = TRUE;
}
