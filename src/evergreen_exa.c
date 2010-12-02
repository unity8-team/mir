/*
 * Copyright 2010 Advanced Micro Devices, Inc.
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
 * Author: Alex Deucher <alexander.deucher@amd.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef XF86DRM_MODE

#include "xf86.h"

#include "exa.h"

#include "radeon.h"
#include "radeon_macros.h"
#include "radeon_reg.h"
#include "evergreen_shader.h"
#include "evergreen_reg.h"
#include "evergreen_state.h"
#include "radeon_exa_shared.h"
#include "radeon_vbo.h"

uint32_t EVERGREEN_ROP[16] = {
    RADEON_ROP3_ZERO, /* GXclear        */
    RADEON_ROP3_DSa,  /* Gxand          */
    RADEON_ROP3_SDna, /* GXandReverse   */
    RADEON_ROP3_S,    /* GXcopy         */
    RADEON_ROP3_DSna, /* GXandInverted  */
    RADEON_ROP3_D,    /* GXnoop         */
    RADEON_ROP3_DSx,  /* GXxor          */
    RADEON_ROP3_DSo,  /* GXor           */
    RADEON_ROP3_DSon, /* GXnor          */
    RADEON_ROP3_DSxn, /* GXequiv        */
    RADEON_ROP3_Dn,   /* GXinvert       */
    RADEON_ROP3_SDno, /* GXorReverse    */
    RADEON_ROP3_Sn,   /* GXcopyInverted */
    RADEON_ROP3_DSno, /* GXorInverted   */
    RADEON_ROP3_DSan, /* GXnand         */
    RADEON_ROP3_ONE,  /* GXset          */
};

Bool
EVERGREENSetAccelState(ScrnInfoPtr pScrn,
		       struct r600_accel_object *src0,
		       struct r600_accel_object *src1,
		       struct r600_accel_object *dst,
		       uint32_t vs_offset, uint32_t ps_offset,
		       int rop, Pixel planemask)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    int ret;

    if (src0) {
	memcpy(&accel_state->src_obj[0], src0, sizeof(struct r600_accel_object));
	accel_state->src_size[0] = src0->pitch * src0->height * (src0->bpp/8);
    } else {
	memset(&accel_state->src_obj[0], 0, sizeof(struct r600_accel_object));
	accel_state->src_size[0] = 0;
    }

    if (src1) {
	memcpy(&accel_state->src_obj[1], src1, sizeof(struct r600_accel_object));
	accel_state->src_size[1] = src1->pitch * src1->height * (src1->bpp/8);
    } else {
	memset(&accel_state->src_obj[1], 0, sizeof(struct r600_accel_object));
	accel_state->src_size[1] = 0;
    }

    if (dst) {
	memcpy(&accel_state->dst_obj, dst, sizeof(struct r600_accel_object));
	accel_state->dst_size = dst->pitch * dst->height * (dst->bpp/8);
    } else {
	memset(&accel_state->dst_obj, 0, sizeof(struct r600_accel_object));
	accel_state->dst_size = 0;
    }

    accel_state->rop = rop;
    accel_state->planemask = planemask;

    /* bad pitch */
    if (accel_state->src_obj[0].pitch & 7)
	RADEON_FALLBACK(("Bad src pitch 0x%08x\n", accel_state->src_obj[0].pitch));

    /* bad offset */
    if (accel_state->src_obj[0].offset & 0xff)
	RADEON_FALLBACK(("Bad src offset 0x%08x\n", accel_state->src_obj[0].offset));

    /* bad pitch */
    if (accel_state->src_obj[1].pitch & 7)
	RADEON_FALLBACK(("Bad src pitch 0x%08x\n", accel_state->src_obj[1].pitch));

    /* bad offset */
    if (accel_state->src_obj[1].offset & 0xff)
	RADEON_FALLBACK(("Bad src offset 0x%08x\n", accel_state->src_obj[1].offset));

    if (accel_state->dst_obj.pitch & 7)
	RADEON_FALLBACK(("Bad dst pitch 0x%08x\n", accel_state->dst_obj.pitch));

    if (accel_state->dst_obj.offset & 0xff)
	RADEON_FALLBACK(("Bad dst offset 0x%08x\n", accel_state->dst_obj.offset));

    accel_state->vs_size = 512;
    accel_state->ps_size = 512;

    accel_state->vs_mc_addr = vs_offset;
    accel_state->ps_mc_addr = ps_offset;

    radeon_cs_space_reset_bos(info->cs);
    radeon_cs_space_add_persistent_bo(info->cs, accel_state->shaders_bo,
				      RADEON_GEM_DOMAIN_VRAM, 0);
    if (accel_state->src_obj[0].bo)
	radeon_cs_space_add_persistent_bo(info->cs, accel_state->src_obj[0].bo,
					  accel_state->src_obj[0].domain, 0);
    if (accel_state->src_obj[1].bo)
	radeon_cs_space_add_persistent_bo(info->cs, accel_state->src_obj[1].bo,
					  accel_state->src_obj[1].domain, 0);
    if (accel_state->dst_obj.bo)
	radeon_cs_space_add_persistent_bo(info->cs, accel_state->dst_obj.bo,
					  0, accel_state->dst_obj.domain);
    ret = radeon_cs_space_check(info->cs);
    if (ret)
	RADEON_FALLBACK(("Not enough RAM to hw accel operation\n"));

    return TRUE;
}

static void
EVERGREENDoneSolid(PixmapPtr pPix);

static Bool
EVERGREENPrepareSolid(PixmapPtr pPix, int alu, Pixel pm, Pixel fg)
{
    ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    cb_config_t     cb_conf;
    shader_config_t vs_conf, ps_conf;
    int pmask = 0;
    uint32_t a, r, g, b;
    float *ps_alu_consts;
    const_config_t ps_const_conf;
    struct r600_accel_object dst;

    //return FALSE;

    if (!RADEONCheckBPP(pPix->drawable.bitsPerPixel))
	RADEON_FALLBACK(("EVERGREENCheckDatatype failed\n"));
    if (!RADEONValidPM(pm, pPix->drawable.bitsPerPixel))
	RADEON_FALLBACK(("invalid planemask\n"));

    dst.offset = 0;
    dst.bo = radeon_get_pixmap_bo(pPix);

    dst.pitch = exaGetPixmapPitch(pPix) / (pPix->drawable.bitsPerPixel / 8);
    dst.width = pPix->drawable.width;
    dst.height = pPix->drawable.height;
    dst.bpp = pPix->drawable.bitsPerPixel;
    dst.domain = RADEON_GEM_DOMAIN_VRAM;

    if (!EVERGREENSetAccelState(pScrn,
				NULL,
				NULL,
				&dst,
				accel_state->solid_vs_offset, accel_state->solid_ps_offset,
				alu, pm))
	return FALSE;

    CLEAR (cb_conf);
    CLEAR (vs_conf);
    CLEAR (ps_conf);
    CLEAR (ps_const_conf);

    radeon_vbo_check(pScrn, &accel_state->vbo, 16);
    radeon_vbo_check(pScrn, &accel_state->cbuf, 256);
    radeon_cp_start(pScrn);

    evergreen_set_default_state(pScrn);

    evergreen_set_generic_scissor(pScrn, 0, 0, accel_state->dst_obj.width, accel_state->dst_obj.height);
    evergreen_set_screen_scissor(pScrn, 0, 0, accel_state->dst_obj.width, accel_state->dst_obj.height);
    evergreen_set_window_scissor(pScrn, 0, 0, accel_state->dst_obj.width, accel_state->dst_obj.height);

    /* Shader */
    vs_conf.shader_addr         = accel_state->vs_mc_addr;
    vs_conf.shader_size         = accel_state->vs_size;
    vs_conf.num_gprs            = 2;
    vs_conf.stack_size          = 0;
    vs_conf.bo                  = accel_state->shaders_bo;
    evergreen_vs_setup(pScrn, &vs_conf, RADEON_GEM_DOMAIN_VRAM);

    ps_conf.shader_addr         = accel_state->ps_mc_addr;
    ps_conf.shader_size         = accel_state->ps_size;
    ps_conf.num_gprs            = 1;
    ps_conf.stack_size          = 0;
    ps_conf.clamp_consts        = 0;
    ps_conf.export_mode         = 2;
    ps_conf.bo                  = accel_state->shaders_bo;
    evergreen_ps_setup(pScrn, &ps_conf, RADEON_GEM_DOMAIN_VRAM);

    cb_conf.id = 0;
    cb_conf.w = accel_state->dst_obj.pitch;
    cb_conf.h = accel_state->dst_obj.height;
    cb_conf.base = accel_state->dst_obj.offset;
    cb_conf.bo = accel_state->dst_obj.bo;

    if (accel_state->dst_obj.bpp == 8) {
	cb_conf.format = COLOR_8;
	cb_conf.comp_swap = 3; /* A */
    } else if (accel_state->dst_obj.bpp == 16) {
	cb_conf.format = COLOR_5_6_5;
	cb_conf.comp_swap = 2; /* RGB */
    } else {
	cb_conf.format = COLOR_8_8_8_8;
	cb_conf.comp_swap = 1; /* ARGB */
    }
    cb_conf.source_format = EXPORT_4C_16BPC;
    cb_conf.blend_clamp = 1;
    evergreen_set_render_target(pScrn, &cb_conf, accel_state->dst_obj.domain);

    /* Render setup */
    if (accel_state->planemask & 0x000000ff)
	pmask |= 4; /* B */
    if (accel_state->planemask & 0x0000ff00)
	pmask |= 2; /* G */
    if (accel_state->planemask & 0x00ff0000)
	pmask |= 1; /* R */
    if (accel_state->planemask & 0xff000000)
	pmask |= 8; /* A */

    BEGIN_BATCH(23);
    EREG(CB_TARGET_MASK,                      (pmask << TARGET0_ENABLE_shift));
    EREG(CB_COLOR_CONTROL,                    (EVERGREEN_ROP[accel_state->rop] |
					       (CB_NORMAL << CB_COLOR_CONTROL__MODE_shift)));
    EREG(CB_BLEND0_CONTROL,                   0);

    /* Interpolator setup */
    /* one unused export from VS (VS_EXPORT_COUNT is zero based, count minus one) */
    EREG(SPI_VS_OUT_CONFIG, (0 << VS_EXPORT_COUNT_shift));
    EREG(SPI_VS_OUT_ID_0, (0 << SEMANTIC_0_shift));
    /* color semantic id 0 -> GPR[0] */
    EREG(SPI_PS_INPUT_CNTL_0 + (0 << 2),       ((0    << SEMANTIC_shift)	|
						(0x03 << DEFAULT_VAL_shift)	|
						FLAT_SHADE_bit));

    /* Enabling flat shading needs both FLAT_SHADE_bit in SPI_PS_INPUT_CNTL_x
     * *and* FLAT_SHADE_ENA_bit in SPI_INTERP_CONTROL_0 */
    /* no VS exports as PS input (NUM_INTERP is not zero based, no minus one) */
    PACK0(SPI_PS_IN_CONTROL_0, 3);
    E32(((0 << NUM_INTERP_shift) |
	 LINEAR_GRADIENT_ENA_bit)); // SPI_PS_IN_CONTROL_0
    E32(0); // SPI_PS_IN_CONTROL_1
    E32(FLAT_SHADE_ENA_bit); // SPI_INTERP_CONTROL_0
    END_BATCH();


    /* PS alu constants */
    ps_const_conf.size_bytes = 256;
    ps_const_conf.type = SHADER_TYPE_PS;
    ps_alu_consts = radeon_vbo_space(pScrn, &accel_state->cbuf, 256);
    if (accel_state->dst_obj.bpp == 16) {
	r = (fg >> 11) & 0x1f;
	g = (fg >> 5) & 0x3f;
	b = (fg >> 0) & 0x1f;
	ps_alu_consts[0] = (float)r / 31; /* R */
	ps_alu_consts[1] = (float)g / 63; /* G */
	ps_alu_consts[2] = (float)b / 31; /* B */
	ps_alu_consts[3] = 1.0; /* A */
    } else if (accel_state->dst_obj.bpp == 8) {
	a = (fg >> 0) & 0xff;
	ps_alu_consts[0] = 0.0; /* R */
	ps_alu_consts[1] = 0.0; /* G */
	ps_alu_consts[2] = 0.0; /* B */
	ps_alu_consts[3] = (float)a / 255; /* A */
    } else {
	a = (fg >> 24) & 0xff;
	r = (fg >> 16) & 0xff;
	g = (fg >> 8) & 0xff;
	b = (fg >> 0) & 0xff;
	ps_alu_consts[0] = (float)r / 255; /* R */
	ps_alu_consts[1] = (float)g / 255; /* G */
	ps_alu_consts[2] = (float)b / 255; /* B */
	ps_alu_consts[3] = (float)a / 255; /* A */
    }
    radeon_vbo_commit(pScrn, &accel_state->cbuf);

    ps_const_conf.bo = accel_state->cbuf.vb_bo;
    ps_const_conf.const_addr = accel_state->cbuf.vb_mc_addr + accel_state->cbuf.vb_start_op;
    evergreen_set_alu_consts(pScrn, &ps_const_conf, RADEON_GEM_DOMAIN_GTT);

    if (accel_state->vsync)
	RADEONVlineHelperClear(pScrn);

    return TRUE;
}


static void
EVERGREENSolid(PixmapPtr pPix, int x1, int y1, int x2, int y2)
{
    ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    float *vb;

    if (accel_state->vsync)
	RADEONVlineHelperSet(pScrn, x1, y1, x2, y2);

    vb = radeon_vbo_space(pScrn, &accel_state->vbo, 8);

    vb[0] = (float)x1;
    vb[1] = (float)y1;

    vb[2] = (float)x1;
    vb[3] = (float)y2;

    vb[4] = (float)x2;
    vb[5] = (float)y2;

    radeon_vbo_commit(pScrn, &accel_state->vbo);
}

static void
EVERGREENDoneSolid(PixmapPtr pPix)
{
    ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;

    if (accel_state->vsync)
	evergreen_cp_wait_vline_sync(pScrn, pPix,
				     accel_state->vline_crtc,
				     accel_state->vline_y1,
				     accel_state->vline_y2);

    evergreen_finish_op(pScrn, 8);
}

static void
EVERGREENDoPrepareCopy(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    int pmask = 0;
    cb_config_t     cb_conf;
    tex_resource_t  tex_res;
    tex_sampler_t   tex_samp;
    shader_config_t vs_conf, ps_conf;

    CLEAR (cb_conf);
    CLEAR (tex_res);
    CLEAR (tex_samp);
    CLEAR (vs_conf);
    CLEAR (ps_conf);

    radeon_vbo_check(pScrn, &accel_state->vbo, 16);
    radeon_cp_start(pScrn);

    evergreen_set_default_state(pScrn);

    evergreen_set_generic_scissor(pScrn, 0, 0, accel_state->dst_obj.width, accel_state->dst_obj.height);
    evergreen_set_screen_scissor(pScrn, 0, 0, accel_state->dst_obj.width, accel_state->dst_obj.height);
    evergreen_set_window_scissor(pScrn, 0, 0, accel_state->dst_obj.width, accel_state->dst_obj.height);

    /* Shader */
    vs_conf.shader_addr         = accel_state->vs_mc_addr;
    vs_conf.shader_size         = accel_state->vs_size;
    vs_conf.num_gprs            = 2;
    vs_conf.stack_size          = 0;
    vs_conf.bo                  = accel_state->shaders_bo;
    evergreen_vs_setup(pScrn, &vs_conf, RADEON_GEM_DOMAIN_VRAM);

    ps_conf.shader_addr         = accel_state->ps_mc_addr;
    ps_conf.shader_size         = accel_state->ps_size;
    ps_conf.num_gprs            = 1;
    ps_conf.stack_size          = 0;
    ps_conf.clamp_consts        = 0;
    ps_conf.export_mode         = 2;
    ps_conf.bo                  = accel_state->shaders_bo;
    evergreen_ps_setup(pScrn, &ps_conf, RADEON_GEM_DOMAIN_VRAM);

    /* Texture */
    tex_res.id                  = 0;
    tex_res.w                   = accel_state->src_obj[0].width;
    tex_res.h                   = accel_state->src_obj[0].height;
    tex_res.pitch               = accel_state->src_obj[0].pitch;
    tex_res.depth               = 0;
    tex_res.dim                 = SQ_TEX_DIM_2D;
    tex_res.base                = accel_state->src_obj[0].offset;
    tex_res.mip_base            = accel_state->src_obj[0].offset;
    tex_res.size                = accel_state->src_size[0];
    tex_res.bo                  = accel_state->src_obj[0].bo;
    tex_res.mip_bo              = accel_state->src_obj[0].bo;
    if (accel_state->src_obj[0].bpp == 8) {
	tex_res.format              = FMT_8;
	tex_res.dst_sel_x           = SQ_SEL_1; /* R */
	tex_res.dst_sel_y           = SQ_SEL_1; /* G */
	tex_res.dst_sel_z           = SQ_SEL_1; /* B */
	tex_res.dst_sel_w           = SQ_SEL_X; /* A */
    } else if (accel_state->src_obj[0].bpp == 16) {
	tex_res.format              = FMT_5_6_5;
	tex_res.dst_sel_x           = SQ_SEL_Z; /* R */
	tex_res.dst_sel_y           = SQ_SEL_Y; /* G */
	tex_res.dst_sel_z           = SQ_SEL_X; /* B */
	tex_res.dst_sel_w           = SQ_SEL_1; /* A */
    } else {
	tex_res.format              = FMT_8_8_8_8;
	tex_res.dst_sel_x           = SQ_SEL_Z; /* R */
	tex_res.dst_sel_y           = SQ_SEL_Y; /* G */
	tex_res.dst_sel_z           = SQ_SEL_X; /* B */
	tex_res.dst_sel_w           = SQ_SEL_W; /* A */
    }

    tex_res.base_level          = 0;
    tex_res.last_level          = 0;
    tex_res.perf_modulation     = 0;
    evergreen_set_tex_resource(pScrn, &tex_res, accel_state->src_obj[0].domain);

    tex_samp.id                 = 0;
    tex_samp.clamp_x            = SQ_TEX_CLAMP_LAST_TEXEL;
    tex_samp.clamp_y            = SQ_TEX_CLAMP_LAST_TEXEL;
    tex_samp.clamp_z            = SQ_TEX_WRAP;
    tex_samp.xy_mag_filter      = SQ_TEX_XY_FILTER_POINT;
    tex_samp.xy_min_filter      = SQ_TEX_XY_FILTER_POINT;
    tex_samp.z_filter           = SQ_TEX_Z_FILTER_NONE;
    tex_samp.mip_filter         = 0;			/* no mipmap */
    evergreen_set_tex_sampler   (pScrn, &tex_samp);

    cb_conf.id = 0;
    cb_conf.w = accel_state->dst_obj.pitch;
    cb_conf.h = accel_state->dst_obj.height;
    cb_conf.base = accel_state->dst_obj.offset;
    cb_conf.bo = accel_state->dst_obj.bo;
    if (accel_state->dst_obj.bpp == 8) {
	cb_conf.format = COLOR_8;
	cb_conf.comp_swap = 3; /* A */
    } else if (accel_state->dst_obj.bpp == 16) {
	cb_conf.format = COLOR_5_6_5;
	cb_conf.comp_swap = 2; /* RGB */
    } else {
	cb_conf.format = COLOR_8_8_8_8;
	cb_conf.comp_swap = 1; /* ARGB */
    }
    cb_conf.source_format = EXPORT_4C_16BPC;
    cb_conf.blend_clamp = 1;
    evergreen_set_render_target(pScrn, &cb_conf, accel_state->dst_obj.domain);

    /* Render setup */
    if (accel_state->planemask & 0x000000ff)
	pmask |= 4; /* B */
    if (accel_state->planemask & 0x0000ff00)
	pmask |= 2; /* G */
    if (accel_state->planemask & 0x00ff0000)
	pmask |= 1; /* R */
    if (accel_state->planemask & 0xff000000)
	pmask |= 8; /* A */

    BEGIN_BATCH(23);
    EREG(CB_TARGET_MASK,                      (pmask << TARGET0_ENABLE_shift));
    EREG(CB_COLOR_CONTROL,                    (EVERGREEN_ROP[accel_state->rop] |
					       (CB_NORMAL << CB_COLOR_CONTROL__MODE_shift)));
    EREG(CB_BLEND0_CONTROL,                   0);

    /* Interpolator setup */
    /* export tex coord from VS */
    EREG(SPI_VS_OUT_CONFIG, ((1 - 1) << VS_EXPORT_COUNT_shift));
    EREG(SPI_VS_OUT_ID_0, (0 << SEMANTIC_0_shift));
    /* color semantic id 0 -> GPR[0] */
    EREG(SPI_PS_INPUT_CNTL_0 + (0 << 2),       ((0    << SEMANTIC_shift)	|
						(0x01 << DEFAULT_VAL_shift)));

    /* Enabling flat shading needs both FLAT_SHADE_bit in SPI_PS_INPUT_CNTL_x
     * *and* FLAT_SHADE_ENA_bit in SPI_INTERP_CONTROL_0 */
    /* input tex coord from VS */
    PACK0(SPI_PS_IN_CONTROL_0, 3);
    E32(((1 << NUM_INTERP_shift) |
	 LINEAR_GRADIENT_ENA_bit)); // SPI_PS_IN_CONTROL_0
    E32(0); //SPI_PS_IN_CONTROL_1
    E32(0); // SPI_INTERP_CONTROL_0
    END_BATCH();

}

static void
EVERGREENDoCopy(ScrnInfoPtr pScrn)
{
    evergreen_finish_op(pScrn, 16);
}

static void
EVERGREENDoCopyVline(PixmapPtr pPix)
{
    ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;

    if (accel_state->vsync)
	evergreen_cp_wait_vline_sync(pScrn, pPix,
				     accel_state->vline_crtc,
				     accel_state->vline_y1,
				     accel_state->vline_y2);

    evergreen_finish_op(pScrn, 16);
}

static void
EVERGREENAppendCopyVertex(ScrnInfoPtr pScrn,
			  int srcX, int srcY,
			  int dstX, int dstY,
			  int w, int h)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    float *vb;

    vb = radeon_vbo_space(pScrn, &accel_state->vbo, 16);

    vb[0] = (float)dstX;
    vb[1] = (float)dstY;
    vb[2] = (float)srcX;
    vb[3] = (float)srcY;

    vb[4] = (float)dstX;
    vb[5] = (float)(dstY + h);
    vb[6] = (float)srcX;
    vb[7] = (float)(srcY + h);

    vb[8] = (float)(dstX + w);
    vb[9] = (float)(dstY + h);
    vb[10] = (float)(srcX + w);
    vb[11] = (float)(srcY + h);

    radeon_vbo_commit(pScrn, &accel_state->vbo);
}

static Bool
EVERGREENPrepareCopy(PixmapPtr pSrc,   PixmapPtr pDst,
		     int xdir, int ydir,
		     int rop,
		     Pixel planemask)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    struct r600_accel_object src_obj, dst_obj;

    //return FALSE;

    if (!RADEONCheckBPP(pSrc->drawable.bitsPerPixel))
	RADEON_FALLBACK(("EVERGREENCheckDatatype src failed\n"));
    if (!RADEONCheckBPP(pDst->drawable.bitsPerPixel))
	RADEON_FALLBACK(("EVERGREENCheckDatatype dst failed\n"));
    if (!RADEONValidPM(planemask, pDst->drawable.bitsPerPixel))
	RADEON_FALLBACK(("Invalid planemask\n"));

    dst_obj.pitch = exaGetPixmapPitch(pDst) / (pDst->drawable.bitsPerPixel / 8);
    src_obj.pitch = exaGetPixmapPitch(pSrc) / (pSrc->drawable.bitsPerPixel / 8);

    accel_state->same_surface = FALSE;

    src_obj.offset = 0;
    dst_obj.offset = 0;
    src_obj.bo = radeon_get_pixmap_bo(pSrc);
    dst_obj.bo = radeon_get_pixmap_bo(pDst);
    if (radeon_get_pixmap_bo(pSrc) == radeon_get_pixmap_bo(pDst))
	accel_state->same_surface = TRUE;

    src_obj.width = pSrc->drawable.width;
    src_obj.height = pSrc->drawable.height;
    src_obj.bpp = pSrc->drawable.bitsPerPixel;
    src_obj.domain = RADEON_GEM_DOMAIN_VRAM | RADEON_GEM_DOMAIN_GTT;

    dst_obj.width = pDst->drawable.width;
    dst_obj.height = pDst->drawable.height;
    dst_obj.bpp = pDst->drawable.bitsPerPixel;
    dst_obj.domain = RADEON_GEM_DOMAIN_VRAM;

    if (!EVERGREENSetAccelState(pScrn,
				&src_obj,
				NULL,
				&dst_obj,
				accel_state->copy_vs_offset, accel_state->copy_ps_offset,
				rop, planemask))
	return FALSE;

    if (accel_state->same_surface == TRUE) {
	unsigned long size = pDst->drawable.height * accel_state->dst_obj.pitch * pDst->drawable.bitsPerPixel/8;

	if (accel_state->copy_area_bo) {
	    radeon_bo_unref(accel_state->copy_area_bo);
	    accel_state->copy_area_bo = NULL;
	}
	accel_state->copy_area_bo = radeon_bo_open(info->bufmgr, 0, size, 0,
						   RADEON_GEM_DOMAIN_VRAM,
						   0);
	if (accel_state->copy_area_bo == NULL)
	    RADEON_FALLBACK(("temp copy surface alloc failed\n"));

	radeon_cs_space_add_persistent_bo(info->cs, accel_state->copy_area_bo,
					  RADEON_GEM_DOMAIN_VRAM, RADEON_GEM_DOMAIN_VRAM);
	if (radeon_cs_space_check(info->cs)) {
	    radeon_bo_unref(accel_state->copy_area_bo);
	    accel_state->copy_area_bo = NULL;
	    return FALSE;
	}
	accel_state->copy_area = (void*)accel_state->copy_area_bo;
    } else
	EVERGREENDoPrepareCopy(pScrn);

    if (accel_state->vsync)
	RADEONVlineHelperClear(pScrn);

    return TRUE;
}

static void
EVERGREENCopy(PixmapPtr pDst,
	      int srcX, int srcY,
	      int dstX, int dstY,
	      int w, int h)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;

    if (accel_state->same_surface && (srcX == dstX) && (srcY == dstY))
	return;

    if (accel_state->vsync)
	RADEONVlineHelperSet(pScrn, dstX, dstY, dstX + w, dstY + h);

    if (accel_state->same_surface && accel_state->copy_area) {
	uint32_t orig_dst_domain = accel_state->dst_obj.domain;
	uint32_t orig_src_domain = accel_state->src_obj[0].domain;
	struct radeon_bo *orig_bo = accel_state->dst_obj.bo;

	/* src to tmp */
	accel_state->dst_obj.domain = RADEON_GEM_DOMAIN_VRAM;
	accel_state->dst_obj.bo = accel_state->copy_area_bo;
	accel_state->dst_obj.offset = 0;
	EVERGREENDoPrepareCopy(pScrn);
	EVERGREENAppendCopyVertex(pScrn, srcX, srcY, dstX, dstY, w, h);
	EVERGREENDoCopy(pScrn);

	/* tmp to dst */
	accel_state->src_obj[0].domain = RADEON_GEM_DOMAIN_VRAM;
	accel_state->src_obj[0].bo = accel_state->copy_area_bo;
	accel_state->src_obj[0].offset = 0;
	accel_state->dst_obj.domain = orig_dst_domain;
	accel_state->dst_obj.bo = orig_bo;
	accel_state->dst_obj.offset = 0;
	EVERGREENDoPrepareCopy(pScrn);
	EVERGREENAppendCopyVertex(pScrn, dstX, dstY, dstX, dstY, w, h);
	EVERGREENDoCopyVline(pDst);

	/* restore state */
	accel_state->src_obj[0].domain = orig_src_domain;
	accel_state->src_obj[0].bo = orig_bo;
	accel_state->src_obj[0].offset = 0;
    } else
	EVERGREENAppendCopyVertex(pScrn, srcX, srcY, dstX, dstY, w, h);

}

static void
EVERGREENDoneCopy(PixmapPtr pDst)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;

    if (!accel_state->same_surface)
	EVERGREENDoCopyVline(pDst);

    if (accel_state->copy_area)
	accel_state->copy_area = NULL;

}


#define xFixedToFloat(f) (((float) (f)) / 65536)

struct blendinfo {
    Bool dst_alpha;
    Bool src_alpha;
    uint32_t blend_cntl;
};

static struct blendinfo EVERGREENBlendOp[] = {
    /* Clear */
    {0, 0, (BLEND_ZERO << COLOR_SRCBLEND_shift) | (BLEND_ZERO << COLOR_DESTBLEND_shift)},
    /* Src */
    {0, 0, (BLEND_ONE << COLOR_SRCBLEND_shift) | (BLEND_ZERO << COLOR_DESTBLEND_shift)},
    /* Dst */
    {0, 0, (BLEND_ZERO << COLOR_SRCBLEND_shift) | (BLEND_ONE << COLOR_DESTBLEND_shift)},
    /* Over */
    {0, 1, (BLEND_ONE << COLOR_SRCBLEND_shift) | (BLEND_ONE_MINUS_SRC_ALPHA << COLOR_DESTBLEND_shift)},
    /* OverReverse */
    {1, 0, (BLEND_ONE_MINUS_DST_ALPHA << COLOR_SRCBLEND_shift) | (BLEND_ONE << COLOR_DESTBLEND_shift)},
    /* In */
    {1, 0, (BLEND_DST_ALPHA << COLOR_SRCBLEND_shift) | (BLEND_ZERO << COLOR_DESTBLEND_shift)},
    /* InReverse */
    {0, 1, (BLEND_ZERO << COLOR_SRCBLEND_shift) | (BLEND_SRC_ALPHA << COLOR_DESTBLEND_shift)},
    /* Out */
    {1, 0, (BLEND_ONE_MINUS_DST_ALPHA << COLOR_SRCBLEND_shift) | (BLEND_ZERO << COLOR_DESTBLEND_shift)},
    /* OutReverse */
    {0, 1, (BLEND_ZERO << COLOR_SRCBLEND_shift) | (BLEND_ONE_MINUS_SRC_ALPHA << COLOR_DESTBLEND_shift)},
    /* Atop */
    {1, 1, (BLEND_DST_ALPHA << COLOR_SRCBLEND_shift) | (BLEND_ONE_MINUS_SRC_ALPHA << COLOR_DESTBLEND_shift)},
    /* AtopReverse */
    {1, 1, (BLEND_ONE_MINUS_DST_ALPHA << COLOR_SRCBLEND_shift) | (BLEND_SRC_ALPHA << COLOR_DESTBLEND_shift)},
    /* Xor */
    {1, 1, (BLEND_ONE_MINUS_DST_ALPHA << COLOR_SRCBLEND_shift) | (BLEND_ONE_MINUS_SRC_ALPHA << COLOR_DESTBLEND_shift)},
    /* Add */
    {0, 0, (BLEND_ONE << COLOR_SRCBLEND_shift) | (BLEND_ONE << COLOR_DESTBLEND_shift)},
};

struct formatinfo {
    unsigned int fmt;
    uint32_t card_fmt;
};

static struct formatinfo EVERGREENTexFormats[] = {
    {PICT_a8r8g8b8,	FMT_8_8_8_8},
    {PICT_x8r8g8b8,	FMT_8_8_8_8},
    {PICT_a8b8g8r8,	FMT_8_8_8_8},
    {PICT_x8b8g8r8,	FMT_8_8_8_8},
#ifdef PICT_TYPE_BGRA
    {PICT_b8g8r8a8,	FMT_8_8_8_8},
    {PICT_b8g8r8x8,	FMT_8_8_8_8},
#endif
    {PICT_r5g6b5,	FMT_5_6_5},
    {PICT_a1r5g5b5,	FMT_1_5_5_5},
    {PICT_x1r5g5b5,     FMT_1_5_5_5},
    {PICT_a8,		FMT_8},
};

static uint32_t EVERGREENGetBlendCntl(int op, PicturePtr pMask, uint32_t dst_format)
{
    uint32_t sblend, dblend;

    sblend = EVERGREENBlendOp[op].blend_cntl & COLOR_SRCBLEND_mask;
    dblend = EVERGREENBlendOp[op].blend_cntl & COLOR_DESTBLEND_mask;

    /* If there's no dst alpha channel, adjust the blend op so that we'll treat
     * it as always 1.
     */
    if (PICT_FORMAT_A(dst_format) == 0 && EVERGREENBlendOp[op].dst_alpha) {
	if (sblend == (BLEND_DST_ALPHA << COLOR_SRCBLEND_shift))
	    sblend = (BLEND_ONE << COLOR_SRCBLEND_shift);
	else if (sblend == (BLEND_ONE_MINUS_DST_ALPHA << COLOR_SRCBLEND_shift))
	    sblend = (BLEND_ZERO << COLOR_SRCBLEND_shift);
    }

    /* If the source alpha is being used, then we should only be in a case where
     * the source blend factor is 0, and the source blend value is the mask
     * channels multiplied by the source picture's alpha.
     */
    if (pMask && pMask->componentAlpha && EVERGREENBlendOp[op].src_alpha) {
	if (dblend == (BLEND_SRC_ALPHA << COLOR_DESTBLEND_shift)) {
	    dblend = (BLEND_SRC_COLOR << COLOR_DESTBLEND_shift);
	} else if (dblend == (BLEND_ONE_MINUS_SRC_ALPHA << COLOR_DESTBLEND_shift)) {
	    dblend = (BLEND_ONE_MINUS_SRC_COLOR << COLOR_DESTBLEND_shift);
	}
    }

    return sblend | dblend;
}

static Bool EVERGREENGetDestFormat(PicturePtr pDstPicture, uint32_t *dst_format)
{
    switch (pDstPicture->format) {
    case PICT_a8r8g8b8:
    case PICT_x8r8g8b8:
    case PICT_a8b8g8r8:
    case PICT_x8b8g8r8:
#ifdef PICT_TYPE_BGRA
    case PICT_b8g8r8a8:
    case PICT_b8g8r8x8:
#endif
	*dst_format = COLOR_8_8_8_8;
	break;
    case PICT_r5g6b5:
	*dst_format = COLOR_5_6_5;
	break;
    case PICT_a1r5g5b5:
    case PICT_x1r5g5b5:
	*dst_format = COLOR_1_5_5_5;
	break;
    case PICT_a8:
	*dst_format = COLOR_8;
	break;
    default:
	RADEON_FALLBACK(("Unsupported dest format 0x%x\n",
	       (int)pDstPicture->format));
    }
    return TRUE;
}

static Bool EVERGREENCheckCompositeTexture(PicturePtr pPict,
					   PicturePtr pDstPict,
					   int op,
					   int unit)
{
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    unsigned int repeatType = pPict->repeat ? pPict->repeatType : RepeatNone;
    unsigned int i;
    int max_tex_w, max_tex_h;

    max_tex_w = 16384;
    max_tex_h = 16384;

    if ((w > max_tex_w) || (h > max_tex_h))
	RADEON_FALLBACK(("Picture w/h too large (%dx%d)\n", w, h));

    for (i = 0; i < sizeof(EVERGREENTexFormats) / sizeof(EVERGREENTexFormats[0]); i++) {
	if (EVERGREENTexFormats[i].fmt == pPict->format)
	    break;
    }
    if (i == sizeof(EVERGREENTexFormats) / sizeof(EVERGREENTexFormats[0]))
	RADEON_FALLBACK(("Unsupported picture format 0x%x\n",
			 (int)pPict->format));

    if (pPict->filter != PictFilterNearest &&
	pPict->filter != PictFilterBilinear)
	RADEON_FALLBACK(("Unsupported filter 0x%x\n", pPict->filter));

    /* for REPEAT_NONE, Render semantics are that sampling outside the source
     * picture results in alpha=0 pixels. We can implement this with a border color
     * *if* our source texture has an alpha channel, otherwise we need to fall
     * back. If we're not transformed then we hope that upper layers have clipped
     * rendering to the bounds of the source drawable, in which case it doesn't
     * matter. I have not, however, verified that the X server always does such
     * clipping.
     */
    /* FIXME evergreen */
    if (pPict->transform != 0 && repeatType == RepeatNone && PICT_FORMAT_A(pPict->format) == 0) {
	if (!(((op == PictOpSrc) || (op == PictOpClear)) && (PICT_FORMAT_A(pDstPict->format) == 0)))
	    RADEON_FALLBACK(("REPEAT_NONE unsupported for transformed xRGB source\n"));
    }

    return TRUE;
}

static void EVERGREENXFormSetup(PicturePtr pPict, PixmapPtr pPix,
				int unit, float *vs_alu_consts)
{
    ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    int const_offset = unit * 8;

    if (pPict->transform != 0) {
	accel_state->is_transform[unit] = TRUE;
	accel_state->transform[unit] = pPict->transform;

	vs_alu_consts[0 + const_offset] = xFixedToFloat(pPict->transform->matrix[0][0]);
	vs_alu_consts[1 + const_offset] = xFixedToFloat(pPict->transform->matrix[0][1]);
	vs_alu_consts[2 + const_offset] = xFixedToFloat(pPict->transform->matrix[0][2]);
	vs_alu_consts[3 + const_offset] = 1.0 / w;

	vs_alu_consts[4 + const_offset] = xFixedToFloat(pPict->transform->matrix[1][0]);
	vs_alu_consts[5 + const_offset] = xFixedToFloat(pPict->transform->matrix[1][1]);
	vs_alu_consts[6 + const_offset] = xFixedToFloat(pPict->transform->matrix[1][2]);
	vs_alu_consts[7 + const_offset] = 1.0 / h;
    } else {
	accel_state->is_transform[unit] = FALSE;

	vs_alu_consts[0 + const_offset] = 1.0;
	vs_alu_consts[1 + const_offset] = 0.0;
	vs_alu_consts[2 + const_offset] = 0.0;
	vs_alu_consts[3 + const_offset] = 1.0 / w;

	vs_alu_consts[4 + const_offset] = 0.0;
	vs_alu_consts[5 + const_offset] = 1.0;
	vs_alu_consts[6 + const_offset] = 0.0;
	vs_alu_consts[7 + const_offset] = 1.0 / h;
    }

}

static Bool EVERGREENTextureSetup(PicturePtr pPict, PixmapPtr pPix,
				  int unit)
{
    ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    unsigned int repeatType = pPict->repeat ? pPict->repeatType : RepeatNone;
    unsigned int i;
    tex_resource_t  tex_res;
    tex_sampler_t   tex_samp;
    int pix_r, pix_g, pix_b, pix_a;

    CLEAR (tex_res);
    CLEAR (tex_samp);

    for (i = 0; i < sizeof(EVERGREENTexFormats) / sizeof(EVERGREENTexFormats[0]); i++) {
	if (EVERGREENTexFormats[i].fmt == pPict->format)
	    break;
    }

    /* Texture */
    tex_res.id                  = unit;
    tex_res.w                   = w;
    tex_res.h                   = h;
    tex_res.pitch               = accel_state->src_obj[unit].pitch;
    tex_res.depth               = 0;
    tex_res.dim                 = SQ_TEX_DIM_2D;
    tex_res.base                = accel_state->src_obj[unit].offset;
    tex_res.mip_base            = accel_state->src_obj[unit].offset;
    tex_res.size                = accel_state->src_size[unit];
    tex_res.format              = EVERGREENTexFormats[i].card_fmt;
    tex_res.bo                  = accel_state->src_obj[unit].bo;
    tex_res.mip_bo              = accel_state->src_obj[unit].bo;

    /* component swizzles */
    switch (pPict->format) {
    case PICT_a1r5g5b5:
    case PICT_a8r8g8b8:
	pix_r = SQ_SEL_Z; /* R */
	pix_g = SQ_SEL_Y; /* G */
	pix_b = SQ_SEL_X; /* B */
	pix_a = SQ_SEL_W; /* A */
	break;
    case PICT_a8b8g8r8:
	pix_r = SQ_SEL_X; /* R */
	pix_g = SQ_SEL_Y; /* G */
	pix_b = SQ_SEL_Z; /* B */
	pix_a = SQ_SEL_W; /* A */
	break;
    case PICT_x8b8g8r8:
	pix_r = SQ_SEL_X; /* R */
	pix_g = SQ_SEL_Y; /* G */
	pix_b = SQ_SEL_Z; /* B */
	pix_a = SQ_SEL_1; /* A */
	break;
#ifdef PICT_TYPE_BGRA
    case PICT_b8g8r8a8:
	pix_r = SQ_SEL_Y; /* R */
	pix_g = SQ_SEL_Z; /* G */
	pix_b = SQ_SEL_W; /* B */
	pix_a = SQ_SEL_X; /* A */
	break;
    case PICT_b8g8r8x8:
	pix_r = SQ_SEL_Y; /* R */
	pix_g = SQ_SEL_Z; /* G */
	pix_b = SQ_SEL_W; /* B */
	pix_a = SQ_SEL_1; /* A */
	break;
#endif
    case PICT_x1r5g5b5:
    case PICT_x8r8g8b8:
    case PICT_r5g6b5:
	pix_r = SQ_SEL_Z; /* R */
	pix_g = SQ_SEL_Y; /* G */
	pix_b = SQ_SEL_X; /* B */
	pix_a = SQ_SEL_1; /* A */
	break;
    case PICT_a8:
	pix_r = SQ_SEL_0; /* R */
	pix_g = SQ_SEL_0; /* G */
	pix_b = SQ_SEL_0; /* B */
	pix_a = SQ_SEL_X; /* A */
	break;
    default:
	RADEON_FALLBACK(("Bad format 0x%x\n", pPict->format));
    }

    if (unit == 0) {
	if (!accel_state->msk_pic) {
	    if (PICT_FORMAT_RGB(pPict->format) == 0) {
		pix_r = SQ_SEL_0;
		pix_g = SQ_SEL_0;
		pix_b = SQ_SEL_0;
	    }

	    if (PICT_FORMAT_A(pPict->format) == 0)
		pix_a = SQ_SEL_1;
	} else {
	    if (accel_state->component_alpha) {
		if (accel_state->src_alpha) {
		    if (PICT_FORMAT_A(pPict->format) == 0) {
			pix_r = SQ_SEL_1;
			pix_g = SQ_SEL_1;
			pix_b = SQ_SEL_1;
			pix_a = SQ_SEL_1;
		    } else {
			pix_r = pix_a;
			pix_g = pix_a;
			pix_b = pix_a;
		    }
		} else {
		    if (PICT_FORMAT_A(pPict->format) == 0)
			pix_a = SQ_SEL_1;
		}
	    } else {
		if (PICT_FORMAT_RGB(pPict->format) == 0) {
		    pix_r = SQ_SEL_0;
		    pix_g = SQ_SEL_0;
		    pix_b = SQ_SEL_0;
		}

		if (PICT_FORMAT_A(pPict->format) == 0)
		    pix_a = SQ_SEL_1;
	    }
	}
    } else {
	if (accel_state->component_alpha) {
	    if (PICT_FORMAT_A(pPict->format) == 0)
		pix_a = SQ_SEL_1;
	} else {
	    if (PICT_FORMAT_A(pPict->format) == 0) {
		pix_r = SQ_SEL_1;
		pix_g = SQ_SEL_1;
		pix_b = SQ_SEL_1;
		pix_a = SQ_SEL_1;
	    } else {
		pix_r = pix_a;
		pix_g = pix_a;
		pix_b = pix_a;
	    }
	}
    }

    tex_res.dst_sel_x           = pix_r; /* R */
    tex_res.dst_sel_y           = pix_g; /* G */
    tex_res.dst_sel_z           = pix_b; /* B */
    tex_res.dst_sel_w           = pix_a; /* A */

    tex_res.base_level          = 0;
    tex_res.last_level          = 0;
    tex_res.perf_modulation     = 0;
    evergreen_set_tex_resource  (pScrn, &tex_res, accel_state->src_obj[unit].domain);

    tex_samp.id                 = unit;
    tex_samp.border_color       = SQ_TEX_BORDER_COLOR_TRANS_BLACK;

    switch (repeatType) {
    case RepeatNormal:
	tex_samp.clamp_x            = SQ_TEX_WRAP;
	tex_samp.clamp_y            = SQ_TEX_WRAP;
	break;
    case RepeatPad:
	tex_samp.clamp_x            = SQ_TEX_CLAMP_LAST_TEXEL;
	tex_samp.clamp_y            = SQ_TEX_CLAMP_LAST_TEXEL;
	break;
    case RepeatReflect:
	tex_samp.clamp_x            = SQ_TEX_MIRROR;
	tex_samp.clamp_y            = SQ_TEX_MIRROR;
	break;
    case RepeatNone:
	tex_samp.clamp_x            = SQ_TEX_CLAMP_BORDER;
	tex_samp.clamp_y            = SQ_TEX_CLAMP_BORDER;
	break;
    default:
	RADEON_FALLBACK(("Bad repeat 0x%x\n", repeatType));
    }

    switch (pPict->filter) {
    case PictFilterNearest:
	tex_samp.xy_mag_filter      = SQ_TEX_XY_FILTER_POINT;
	tex_samp.xy_min_filter      = SQ_TEX_XY_FILTER_POINT;
	break;
    case PictFilterBilinear:
	tex_samp.xy_mag_filter      = SQ_TEX_XY_FILTER_BILINEAR;
	tex_samp.xy_min_filter      = SQ_TEX_XY_FILTER_BILINEAR;
	break;
    default:
	RADEON_FALLBACK(("Bad filter 0x%x\n", pPict->filter));
    }

    tex_samp.clamp_z            = SQ_TEX_WRAP;
    tex_samp.z_filter           = SQ_TEX_Z_FILTER_NONE;
    tex_samp.mip_filter         = 0;			/* no mipmap */
    evergreen_set_tex_sampler   (pScrn, &tex_samp);

    return TRUE;
}

static Bool EVERGREENCheckComposite(int op, PicturePtr pSrcPicture,
				    PicturePtr pMaskPicture,
				    PicturePtr pDstPicture)
{
    uint32_t tmp1;
    PixmapPtr pSrcPixmap, pDstPixmap;
    int max_tex_w, max_tex_h, max_dst_w, max_dst_h;

    /* Check for unsupported compositing operations. */
    if (op >= (int) (sizeof(EVERGREENBlendOp) / sizeof(EVERGREENBlendOp[0])))
	RADEON_FALLBACK(("Unsupported Composite op 0x%x\n", op));

    if (!pSrcPicture->pDrawable)
	RADEON_FALLBACK(("Solid or gradient pictures not supported yet\n"));

    pSrcPixmap = RADEONGetDrawablePixmap(pSrcPicture->pDrawable);

    max_tex_w = 8192;
    max_tex_h = 8192;
    max_dst_w = 8192;
    max_dst_h = 8192;

    if (pSrcPixmap->drawable.width >= max_tex_w ||
	pSrcPixmap->drawable.height >= max_tex_h) {
	RADEON_FALLBACK(("Source w/h too large (%d,%d).\n",
			 pSrcPixmap->drawable.width,
			 pSrcPixmap->drawable.height));
    }

    pDstPixmap = RADEONGetDrawablePixmap(pDstPicture->pDrawable);

    if (pDstPixmap->drawable.width >= max_dst_w ||
	pDstPixmap->drawable.height >= max_dst_h) {
	RADEON_FALLBACK(("Dest w/h too large (%d,%d).\n",
			 pDstPixmap->drawable.width,
			 pDstPixmap->drawable.height));
    }

    if (pMaskPicture) {
	PixmapPtr pMaskPixmap;

	if (!pMaskPicture->pDrawable)
	    RADEON_FALLBACK(("Solid or gradient pictures not supported yet\n"));

	pMaskPixmap = RADEONGetDrawablePixmap(pMaskPicture->pDrawable);

	if (pMaskPixmap->drawable.width >= max_tex_w ||
	    pMaskPixmap->drawable.height >= max_tex_h) {
	    RADEON_FALLBACK(("Mask w/h too large (%d,%d).\n",
			     pMaskPixmap->drawable.width,
			     pMaskPixmap->drawable.height));
	}

	if (pMaskPicture->componentAlpha) {
	    /* Check if it's component alpha that relies on a source alpha and
	     * on the source value.  We can only get one of those into the
	     * single source value that we get to blend with.
	     */
	    if (EVERGREENBlendOp[op].src_alpha &&
		(EVERGREENBlendOp[op].blend_cntl & COLOR_SRCBLEND_mask) !=
		(BLEND_ZERO << COLOR_SRCBLEND_shift)) {
		RADEON_FALLBACK(("Component alpha not supported with source "
				 "alpha and source value blending.\n"));
	    }
	}

	if (!EVERGREENCheckCompositeTexture(pMaskPicture, pDstPicture, op, 1))
	    return FALSE;
    }

    if (!EVERGREENCheckCompositeTexture(pSrcPicture, pDstPicture, op, 0))
	return FALSE;

    if (!EVERGREENGetDestFormat(pDstPicture, &tmp1))
	return FALSE;

    return TRUE;

}

static Bool EVERGREENPrepareComposite(int op, PicturePtr pSrcPicture,
				      PicturePtr pMaskPicture, PicturePtr pDstPicture,
				      PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst)
{
    ScrnInfoPtr pScrn = xf86Screens[pSrc->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    uint32_t blendcntl, dst_format;
    cb_config_t cb_conf;
    shader_config_t vs_conf, ps_conf;
    const_config_t vs_const_conf;
    struct r600_accel_object src_obj, mask_obj, dst_obj;
    float *cbuf;

    //return FALSE;

    if (pDst->drawable.bitsPerPixel < 8 || pSrc->drawable.bitsPerPixel < 8)
	return FALSE;

    src_obj.offset = 0;
    dst_obj.offset = 0;
    src_obj.bo = radeon_get_pixmap_bo(pSrc);
    dst_obj.bo = radeon_get_pixmap_bo(pDst);

    src_obj.pitch = exaGetPixmapPitch(pSrc) / (pSrc->drawable.bitsPerPixel / 8);
    dst_obj.pitch = exaGetPixmapPitch(pDst) / (pDst->drawable.bitsPerPixel / 8);

    src_obj.width = pSrc->drawable.width;
    src_obj.height = pSrc->drawable.height;
    src_obj.bpp = pSrc->drawable.bitsPerPixel;
    src_obj.domain = RADEON_GEM_DOMAIN_VRAM | RADEON_GEM_DOMAIN_GTT;

    dst_obj.width = pDst->drawable.width;
    dst_obj.height = pDst->drawable.height;
    dst_obj.bpp = pDst->drawable.bitsPerPixel;
    dst_obj.domain = RADEON_GEM_DOMAIN_VRAM;

    if (pMask) {
	mask_obj.offset = 0;
	mask_obj.bo = radeon_get_pixmap_bo(pMask);
	mask_obj.pitch = exaGetPixmapPitch(pMask) / (pMask->drawable.bitsPerPixel / 8);

	mask_obj.width = pMask->drawable.width;
	mask_obj.height = pMask->drawable.height;
	mask_obj.bpp = pMask->drawable.bitsPerPixel;
	mask_obj.domain = RADEON_GEM_DOMAIN_VRAM | RADEON_GEM_DOMAIN_GTT;

	if (!EVERGREENSetAccelState(pScrn,
				    &src_obj,
				    &mask_obj,
				    &dst_obj,
				    accel_state->comp_vs_offset, accel_state->comp_ps_offset,
				    3, 0xffffffff))
	    return FALSE;

	accel_state->msk_pic = pMaskPicture;
	if (pMaskPicture->componentAlpha) {
	    accel_state->component_alpha = TRUE;
	    if (EVERGREENBlendOp[op].src_alpha)
		accel_state->src_alpha = TRUE;
	    else
		accel_state->src_alpha = FALSE;
	} else {
	    accel_state->component_alpha = FALSE;
	    accel_state->src_alpha = FALSE;
	}
    } else {
	if (!EVERGREENSetAccelState(pScrn,
				    &src_obj,
				    NULL,
				    &dst_obj,
				    accel_state->comp_vs_offset, accel_state->comp_ps_offset,
				    3, 0xffffffff))
	    return FALSE;

	accel_state->msk_pic = NULL;
	accel_state->component_alpha = FALSE;
	accel_state->src_alpha = FALSE;
    }

    if (!EVERGREENGetDestFormat(pDstPicture, &dst_format))
	return FALSE;

    CLEAR (cb_conf);
    CLEAR (vs_conf);
    CLEAR (ps_conf);
    CLEAR (vs_const_conf);

    if (pMask)
        radeon_vbo_check(pScrn, &accel_state->vbo, 24);
    else
        radeon_vbo_check(pScrn, &accel_state->vbo, 16);

    radeon_vbo_check(pScrn, &accel_state->cbuf, 256);

    radeon_cp_start(pScrn);

    evergreen_set_default_state(pScrn);

    evergreen_set_generic_scissor(pScrn, 0, 0, accel_state->dst_obj.width, accel_state->dst_obj.height);
    evergreen_set_screen_scissor(pScrn, 0, 0, accel_state->dst_obj.width, accel_state->dst_obj.height);
    evergreen_set_window_scissor(pScrn, 0, 0, accel_state->dst_obj.width, accel_state->dst_obj.height);

    if (!EVERGREENTextureSetup(pSrcPicture, pSrc, 0)) {
        radeon_ib_discard(pScrn);
        radeon_cs_flush_indirect(pScrn);
        radeon_vb_discard(pScrn, &accel_state->vbo);
        radeon_vb_discard(pScrn, &accel_state->cbuf);
        return FALSE;
    }

    if (pMask) {
        if (!EVERGREENTextureSetup(pMaskPicture, pMask, 1)) {
	    radeon_ib_discard(pScrn);
	    radeon_cs_flush_indirect(pScrn);
            radeon_vb_discard(pScrn, &accel_state->vbo);
            radeon_vb_discard(pScrn, &accel_state->cbuf);
            return FALSE;
        }
    } else
        accel_state->is_transform[1] = FALSE;

    if (pMask) {
	evergreen_set_bool_consts(pScrn, SQ_BOOL_CONST_vs, (1 << 0));
	evergreen_set_bool_consts(pScrn, SQ_BOOL_CONST_ps, (1 << 0));
    } else {
	evergreen_set_bool_consts(pScrn, SQ_BOOL_CONST_vs, (0 << 0));
	evergreen_set_bool_consts(pScrn, SQ_BOOL_CONST_ps, (0 << 0));
    }

    /* Shader */
    vs_conf.shader_addr         = accel_state->vs_mc_addr;
    vs_conf.shader_size         = accel_state->vs_size;
    vs_conf.num_gprs            = 5;
    vs_conf.stack_size          = 1;
    vs_conf.bo                  = accel_state->shaders_bo;
    evergreen_vs_setup(pScrn, &vs_conf, RADEON_GEM_DOMAIN_VRAM);

    ps_conf.shader_addr         = accel_state->ps_mc_addr;
    ps_conf.shader_size         = accel_state->ps_size;
    ps_conf.num_gprs            = 3;
    ps_conf.stack_size          = 1;
    ps_conf.clamp_consts        = 0;
    ps_conf.export_mode         = 2;
    ps_conf.bo                  = accel_state->shaders_bo;
    evergreen_ps_setup(pScrn, &ps_conf, RADEON_GEM_DOMAIN_VRAM);

    cb_conf.id = 0;
    cb_conf.w = accel_state->dst_obj.pitch;
    cb_conf.h = accel_state->dst_obj.height;
    cb_conf.base = accel_state->dst_obj.offset;
    cb_conf.format = dst_format;
    cb_conf.bo = accel_state->dst_obj.bo;

    switch (pDstPicture->format) {
    case PICT_a8r8g8b8:
    case PICT_x8r8g8b8:
    case PICT_a1r5g5b5:
    case PICT_x1r5g5b5:
    default:
	cb_conf.comp_swap = 1; /* ARGB */
	break;
    case PICT_a8b8g8r8:
    case PICT_x8b8g8r8:
	cb_conf.comp_swap = 0; /* ABGR */
	break;
#ifdef PICT_TYPE_BGRA
    case PICT_b8g8r8a8:
    case PICT_b8g8r8x8:
	cb_conf.comp_swap = 3; /* BGRA */
	break;
#endif
    case PICT_r5g6b5:
	cb_conf.comp_swap = 2; /* RGB */
	break;
    case PICT_a8:
	cb_conf.comp_swap = 3; /* A */
	break;
    }
    cb_conf.source_format = EXPORT_4C_16BPC;
    cb_conf.blend_clamp = 1;
    evergreen_set_render_target(pScrn, &cb_conf, accel_state->dst_obj.domain);

    blendcntl = EVERGREENGetBlendCntl(op, pMaskPicture, pDstPicture->format);

    BEGIN_BATCH(24);
    EREG(CB_TARGET_MASK,                      (0xf << TARGET0_ENABLE_shift));
    EREG(CB_COLOR_CONTROL,                    (EVERGREEN_ROP[3] |
					       (CB_NORMAL << CB_COLOR_CONTROL__MODE_shift)));
    EREG(CB_BLEND0_CONTROL,                   blendcntl | CB_BLEND0_CONTROL__ENABLE_bit);

    /* Interpolator setup */
    if (pMask) {
	/* export 2 tex coords from VS */
	EREG(SPI_VS_OUT_CONFIG, ((2 - 1) << VS_EXPORT_COUNT_shift));
	/* src = semantic id 0; mask = semantic id 1 */
	EREG(SPI_VS_OUT_ID_0, ((0 << SEMANTIC_0_shift) |
			       (1 << SEMANTIC_1_shift)));
    } else {
	/* export 1 tex coords from VS */
	EREG(SPI_VS_OUT_CONFIG, ((1 - 1) << VS_EXPORT_COUNT_shift));
	/* src = semantic id 0 */
	EREG(SPI_VS_OUT_ID_0,   (0 << SEMANTIC_0_shift));
    }

    PACK0(SPI_PS_INPUT_CNTL_0 + (0 << 2), 2);
    /* SPI_PS_INPUT_CNTL_0 maps to GPR[0] - load with semantic id 0 */
    E32(((0    << SEMANTIC_shift)	|
	 (0x01 << DEFAULT_VAL_shift)));
    /* SPI_PS_INPUT_CNTL_1 maps to GPR[1] - load with semantic id 1 */
    E32(((1    << SEMANTIC_shift)	|
	 (0x01 << DEFAULT_VAL_shift)));

    PACK0(SPI_PS_IN_CONTROL_0, 3);
    if (pMask) {
	/* input 2 tex coords from VS */
	E32(((2 << NUM_INTERP_shift) |
	     LINEAR_GRADIENT_ENA_bit)); // SPI_PS_IN_CONTROL_0
    } else {
	/* input 1 tex coords from VS */
	E32(((1 << NUM_INTERP_shift) |
	     LINEAR_GRADIENT_ENA_bit)); // SPI_PS_IN_CONTROL_0
    }
    E32(0); // SPI_PS_IN_CONTROL_1
    E32(0); // SPI_INTERP_CONTROL_0
    END_BATCH();

    /* VS alu constants */
    vs_const_conf.size_bytes = 256;
    vs_const_conf.type = SHADER_TYPE_VS;
    cbuf = radeon_vbo_space(pScrn, &accel_state->cbuf, 256);

    EVERGREENXFormSetup(pSrcPicture, pSrc, 0, cbuf);
    if (pMask)
        EVERGREENXFormSetup(pMaskPicture, pMask, 1, cbuf);

    radeon_vbo_commit(pScrn, &accel_state->cbuf);

    /* VS alu constants */
    vs_const_conf.bo = accel_state->cbuf.vb_bo;
    vs_const_conf.const_addr = accel_state->cbuf.vb_mc_addr + accel_state->cbuf.vb_start_op;
    evergreen_set_alu_consts(pScrn, &vs_const_conf, RADEON_GEM_DOMAIN_GTT);

    if (accel_state->vsync)
	RADEONVlineHelperClear(pScrn);

    return TRUE;
}

static void EVERGREENComposite(PixmapPtr pDst,
			       int srcX, int srcY,
			       int maskX, int maskY,
			       int dstX, int dstY,
			       int w, int h)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    float *vb;

    if (accel_state->vsync)
	RADEONVlineHelperSet(pScrn, dstX, dstY, dstX + w, dstY + h);

    if (accel_state->msk_pic) {

	vb = radeon_vbo_space(pScrn, &accel_state->vbo, 24);

	vb[0] = (float)dstX;
	vb[1] = (float)dstY;
	vb[2] = (float)srcX;
	vb[3] = (float)srcY;
	vb[4] = (float)maskX;
	vb[5] = (float)maskY;

	vb[6] = (float)dstX;
	vb[7] = (float)(dstY + h);
	vb[8] = (float)srcX;
	vb[9] = (float)(srcY + h);
	vb[10] = (float)maskX;
	vb[11] = (float)(maskY + h);

	vb[12] = (float)(dstX + w);
	vb[13] = (float)(dstY + h);
	vb[14] = (float)(srcX + w);
	vb[15] = (float)(srcY + h);
	vb[16] = (float)(maskX + w);
	vb[17] = (float)(maskY + h);

	radeon_vbo_commit(pScrn, &accel_state->vbo);

    } else {

	vb = radeon_vbo_space(pScrn, &accel_state->vbo, 16);

	vb[0] = (float)dstX;
	vb[1] = (float)dstY;
	vb[2] = (float)srcX;
	vb[3] = (float)srcY;

	vb[4] = (float)dstX;
	vb[5] = (float)(dstY + h);
	vb[6] = (float)srcX;
	vb[7] = (float)(srcY + h);

	vb[8] = (float)(dstX + w);
	vb[9] = (float)(dstY + h);
	vb[10] = (float)(srcX + w);
	vb[11] = (float)(srcY + h);

	radeon_vbo_commit(pScrn, &accel_state->vbo);
    }


}

static void EVERGREENDoneComposite(PixmapPtr pDst)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    int vtx_size;

    if (accel_state->vsync)
       evergreen_cp_wait_vline_sync(pScrn, pDst,
				    accel_state->vline_crtc,
				    accel_state->vline_y1,
				    accel_state->vline_y2);

    vtx_size = accel_state->msk_pic ? 24 : 16;

    evergreen_finish_op(pScrn, vtx_size);
}

static Bool
EVERGREENUploadToScreen(PixmapPtr pDst, int x, int y, int w, int h,
			char *src, int src_pitch)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    struct radeon_exa_pixmap_priv *driver_priv;
    struct radeon_bo *scratch = NULL;
    struct radeon_bo *copy_dst;
    unsigned char *dst;
    unsigned size;
    uint32_t dst_domain;
    int bpp = pDst->drawable.bitsPerPixel;
    uint32_t scratch_pitch = RADEON_ALIGN(w * bpp / 8, 256);
    uint32_t copy_pitch;
    uint32_t src_pitch_hw = scratch_pitch / (bpp / 8);
    uint32_t dst_pitch_hw = exaGetPixmapPitch(pDst) / (bpp / 8);
    int ret;
    Bool flush = TRUE;
    Bool r;
    int i;
    struct r600_accel_object src_obj, dst_obj;

    if (bpp < 8)
	return FALSE;

    driver_priv = exaGetPixmapDriverPrivate(pDst);

    /* If we know the BO won't be busy, don't bother with a scratch */
    copy_dst = driver_priv->bo;
    copy_pitch = pDst->devKind;
    if (!radeon_bo_is_referenced_by_cs(driver_priv->bo, info->cs)) {
	flush = FALSE;
	if (!radeon_bo_is_busy(driver_priv->bo, &dst_domain))
	    goto copy;
    }

    size = scratch_pitch * h;
    scratch = radeon_bo_open(info->bufmgr, 0, size, 0, RADEON_GEM_DOMAIN_GTT, 0);
    if (scratch == NULL) {
	goto copy;
    }

    src_obj.pitch = src_pitch_hw;
    src_obj.width = w;
    src_obj.height = h;
    src_obj.offset = 0;
    src_obj.bpp = bpp;
    src_obj.domain = RADEON_GEM_DOMAIN_GTT;
    src_obj.bo = scratch;

    dst_obj.pitch = dst_pitch_hw;
    dst_obj.width = pDst->drawable.width;
    dst_obj.height = pDst->drawable.height;
    dst_obj.offset = 0;
    dst_obj.bpp = bpp;
    dst_obj.domain = RADEON_GEM_DOMAIN_VRAM;
    dst_obj.bo = radeon_get_pixmap_bo(pDst);

    if (!EVERGREENSetAccelState(pScrn,
				&src_obj,
				NULL,
				&dst_obj,
				accel_state->copy_vs_offset, accel_state->copy_ps_offset,
				3, 0xffffffff)) {
        goto copy;
    }
    copy_dst = scratch;
    copy_pitch = scratch_pitch;
    flush = FALSE;

copy:
    if (flush)
	radeon_cs_flush_indirect(pScrn);

    ret = radeon_bo_map(copy_dst, 0);
    if (ret) {
        r = FALSE;
        goto out;
    }
    r = TRUE;
    size = w * bpp / 8;
    dst = copy_dst->ptr;
    if (copy_dst == driver_priv->bo)
	dst += y * copy_pitch + x * bpp / 8;
    for (i = 0; i < h; i++) {
	memcpy(dst + i * copy_pitch, src, size);
        src += src_pitch;
    }
    radeon_bo_unmap(copy_dst);

    if (copy_dst == scratch) {
	if (info->accel_state->vsync)
	    RADEONVlineHelperSet(pScrn, x, y, x + w, y + h);

	/* blit from gart to vram */
	EVERGREENDoPrepareCopy(pScrn);
	EVERGREENAppendCopyVertex(pScrn, 0, 0, x, y, w, h);
	EVERGREENDoCopyVline(pDst);
    }

out:
    if (scratch)
	radeon_bo_unref(scratch);
    return r;
}

static Bool
EVERGREENDownloadFromScreen(PixmapPtr pSrc, int x, int y, int w,
			    int h, char *dst, int dst_pitch)
{
    ScrnInfoPtr pScrn = xf86Screens[pSrc->drawable.pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    struct radeon_exa_pixmap_priv *driver_priv;
    struct radeon_bo *scratch = NULL;
    struct radeon_bo *copy_src;
    unsigned size;
    uint32_t src_domain = 0;
    int bpp = pSrc->drawable.bitsPerPixel;
    uint32_t scratch_pitch = RADEON_ALIGN(w * bpp / 8, 256);
    uint32_t copy_pitch;
    uint32_t dst_pitch_hw = scratch_pitch / (bpp / 8);
    uint32_t src_pitch_hw = exaGetPixmapPitch(pSrc) / (bpp / 8);
    int ret;
    Bool flush = FALSE;
    Bool r;
    struct r600_accel_object src_obj, dst_obj;
    uint32_t tiling_flags = 0, pitch = 0;

    if (bpp < 8)
	return FALSE;

    driver_priv = exaGetPixmapDriverPrivate(pSrc);

    ret = radeon_bo_get_tiling(driver_priv->bo, &tiling_flags, &pitch);
    if (ret)
	ErrorF("radeon_bo_get_tiling failed\n");

    /* If we know the BO won't end up in VRAM anyway, don't bother with a scratch */
    copy_src = driver_priv->bo;
    copy_pitch = pSrc->devKind;
    if (!(tiling_flags & (RADEON_TILING_MACRO | RADEON_TILING_MICRO))) {
	if (radeon_bo_is_referenced_by_cs(driver_priv->bo, info->cs)) {
	    src_domain = radeon_bo_get_src_domain(driver_priv->bo);
	    if ((src_domain & (RADEON_GEM_DOMAIN_GTT | RADEON_GEM_DOMAIN_VRAM)) ==
		(RADEON_GEM_DOMAIN_GTT | RADEON_GEM_DOMAIN_VRAM))
		src_domain = 0;
	    else /* A write may be scheduled */
		flush = TRUE;
	}
    }

    if (!src_domain)
	radeon_bo_is_busy(driver_priv->bo, &src_domain);

    if (src_domain & ~(uint32_t)RADEON_GEM_DOMAIN_VRAM)
	goto copy;

    if ((info->ChipFamily == CHIP_FAMILY_PALM) &&
	((w < 128) || (h < 128)))
	goto copy;

    size = scratch_pitch * h;
    scratch = radeon_bo_open(info->bufmgr, 0, size, 0, RADEON_GEM_DOMAIN_GTT, 0);
    if (scratch == NULL) {
	goto copy;
    }
    radeon_cs_space_reset_bos(info->cs);
    radeon_cs_space_add_persistent_bo(info->cs, info->accel_state->shaders_bo,
				      RADEON_GEM_DOMAIN_VRAM, 0);
    accel_state->src_obj[0].domain = RADEON_GEM_DOMAIN_GTT | RADEON_GEM_DOMAIN_VRAM;
    radeon_add_pixmap(info->cs, pSrc, info->accel_state->src_obj[0].domain, 0);
    accel_state->dst_obj.domain = RADEON_GEM_DOMAIN_GTT;
    radeon_cs_space_add_persistent_bo(info->cs, scratch, 0, accel_state->dst_obj.domain);
    ret = radeon_cs_space_check(info->cs);
    if (ret) {
	goto copy;
    }

    src_obj.pitch = src_pitch_hw;
    src_obj.width = pSrc->drawable.width;
    src_obj.height = pSrc->drawable.height;
    src_obj.offset = 0;
    src_obj.bpp = bpp;
    src_obj.domain = RADEON_GEM_DOMAIN_VRAM | RADEON_GEM_DOMAIN_GTT;
    src_obj.bo = radeon_get_pixmap_bo(pSrc);

    dst_obj.pitch = dst_pitch_hw;
    dst_obj.width = w;
    dst_obj.height = h;
    dst_obj.offset = 0;
    dst_obj.bo = scratch;
    dst_obj.bpp = bpp;
    dst_obj.domain = RADEON_GEM_DOMAIN_GTT;

    if (!EVERGREENSetAccelState(pScrn,
				&src_obj,
				NULL,
				&dst_obj,
				accel_state->copy_vs_offset, accel_state->copy_ps_offset,
				3, 0xffffffff)) {
	goto copy;
    }

    /* blit from vram to gart */
    EVERGREENDoPrepareCopy(pScrn);
    EVERGREENAppendCopyVertex(pScrn, x, y, 0, 0, w, h);
    EVERGREENDoCopy(pScrn);
    copy_src = scratch;
    copy_pitch = scratch_pitch;
    flush = TRUE;

copy:
    if (flush)
	radeon_cs_flush_indirect(pScrn);

    ret = radeon_bo_map(copy_src, 0);
    if (ret) {
	ErrorF("failed to map pixmap: %d\n", ret);
        r = FALSE;
        goto out;
    }
    r = TRUE;
    w *= bpp / 8;
    if (copy_src == driver_priv->bo)
	size = y * copy_pitch + x * bpp / 8;
    else
	size = 0;
    while (h--) {
	memcpy(dst, copy_src->ptr + size, w);
	size += copy_pitch;
        dst += dst_pitch;
    }
    radeon_bo_unmap(copy_src);
out:
    if (scratch)
	radeon_bo_unref(scratch);
    return r;
}

static int
EVERGREENMarkSync(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;

    return ++accel_state->exaSyncMarker;

}

static void
EVERGREENSync(ScreenPtr pScreen, int marker)
{
    return;
}

static Bool
EVERGREENAllocShaders(ScrnInfoPtr pScrn, ScreenPtr pScreen)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;

    /* 512 bytes per shader for now */
    int size = 512 * 9;

    accel_state->shaders = NULL;

    accel_state->shaders_bo = radeon_bo_open(info->bufmgr, 0, size, 0,
					     RADEON_GEM_DOMAIN_VRAM, 0);
    if (accel_state->shaders_bo == NULL) {
	ErrorF("Allocating shader failed\n");
	return FALSE;
    }
    return TRUE;
}

Bool
EVERGREENLoadShaders(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    RADEONChipFamily ChipSet = info->ChipFamily;
    uint32_t *shader;
    int ret;

    ret = radeon_bo_map(accel_state->shaders_bo, 1);
    if (ret) {
	FatalError("failed to map shader %d\n", ret);
	return FALSE;
    }
    shader = accel_state->shaders_bo->ptr;

    /*  solid vs --------------------------------------- */
    accel_state->solid_vs_offset = 0;
    evergreen_solid_vs(ChipSet, shader + accel_state->solid_vs_offset / 4);

    /*  solid ps --------------------------------------- */
    accel_state->solid_ps_offset = 512;
    evergreen_solid_ps(ChipSet, shader + accel_state->solid_ps_offset / 4);

    /*  copy vs --------------------------------------- */
    accel_state->copy_vs_offset = 1024;
    evergreen_copy_vs(ChipSet, shader + accel_state->copy_vs_offset / 4);

    /*  copy ps --------------------------------------- */
    accel_state->copy_ps_offset = 1536;
    evergreen_copy_ps(ChipSet, shader + accel_state->copy_ps_offset / 4);

    /*  comp vs --------------------------------------- */
    accel_state->comp_vs_offset = 2048;
    evergreen_comp_vs(ChipSet, shader + accel_state->comp_vs_offset / 4);

    /*  comp ps --------------------------------------- */
    accel_state->comp_ps_offset = 2560;
    evergreen_comp_ps(ChipSet, shader + accel_state->comp_ps_offset / 4);

    /*  xv vs --------------------------------------- */
    accel_state->xv_vs_offset = 3072;
    evergreen_xv_vs(ChipSet, shader + accel_state->xv_vs_offset / 4);

    /*  xv ps --------------------------------------- */
    accel_state->xv_ps_offset = 3584;
    evergreen_xv_ps(ChipSet, shader + accel_state->xv_ps_offset / 4);

    radeon_bo_unmap(accel_state->shaders_bo);

    return TRUE;
}

Bool
EVERGREENDrawInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn =  xf86Screens[pScreen->myNum];
    RADEONInfoPtr info   = RADEONPTR(pScrn);

    if (info->accel_state->exa == NULL) {
	xf86DrvMsg(pScreen->myNum, X_ERROR, "Memory map not set up\n");
	return FALSE;
    }

    /* accel requires kms */
    if (!info->cs)
	return FALSE;

    info->accel_state->exa->exa_major = EXA_VERSION_MAJOR;
    info->accel_state->exa->exa_minor = EXA_VERSION_MINOR;

    info->accel_state->exa->PrepareSolid = EVERGREENPrepareSolid;
    info->accel_state->exa->Solid = EVERGREENSolid;
    info->accel_state->exa->DoneSolid = EVERGREENDoneSolid;

    info->accel_state->exa->PrepareCopy = EVERGREENPrepareCopy;
    info->accel_state->exa->Copy = EVERGREENCopy;
    info->accel_state->exa->DoneCopy = EVERGREENDoneCopy;

    info->accel_state->exa->MarkSync = EVERGREENMarkSync;
    info->accel_state->exa->WaitMarker = EVERGREENSync;

    info->accel_state->exa->CreatePixmap = RADEONEXACreatePixmap;
    info->accel_state->exa->DestroyPixmap = RADEONEXADestroyPixmap;
    info->accel_state->exa->PixmapIsOffscreen = RADEONEXAPixmapIsOffscreen;
    info->accel_state->exa->PrepareAccess = RADEONPrepareAccess_CS;
    info->accel_state->exa->FinishAccess = RADEONFinishAccess_CS;
    info->accel_state->exa->UploadToScreen = EVERGREENUploadToScreen;
    info->accel_state->exa->DownloadFromScreen = EVERGREENDownloadFromScreen;
#if (EXA_VERSION_MAJOR == 2 && EXA_VERSION_MINOR >= 5)
    info->accel_state->exa->CreatePixmap2 = RADEONEXACreatePixmap2;
#endif

    info->accel_state->exa->flags = EXA_OFFSCREEN_PIXMAPS;
#ifdef EXA_SUPPORTS_PREPARE_AUX
    info->accel_state->exa->flags |= EXA_SUPPORTS_PREPARE_AUX;
#endif

#ifdef EXA_HANDLES_PIXMAPS
    info->accel_state->exa->flags |= EXA_HANDLES_PIXMAPS;
#ifdef EXA_MIXED_PIXMAPS
    info->accel_state->exa->flags |= EXA_MIXED_PIXMAPS;
#endif
#endif
    info->accel_state->exa->pixmapOffsetAlign = 256;
    info->accel_state->exa->pixmapPitchAlign = 256;

    info->accel_state->exa->CheckComposite = EVERGREENCheckComposite;
    info->accel_state->exa->PrepareComposite = EVERGREENPrepareComposite;
    info->accel_state->exa->Composite = EVERGREENComposite;
    info->accel_state->exa->DoneComposite = EVERGREENDoneComposite;

#if EXA_VERSION_MAJOR > 2 || (EXA_VERSION_MAJOR == 2 && EXA_VERSION_MINOR >= 3)
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Setting EXA maxPitchBytes\n");

    info->accel_state->exa->maxPitchBytes = 32768;
    info->accel_state->exa->maxX = 8192;
#else
    info->accel_state->exa->maxX = 8192;
#endif
    info->accel_state->exa->maxY = 8192;

    /* not supported yet */
    if (xf86ReturnOptValBool(info->Options, OPTION_EXA_VSYNC, FALSE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EXA VSync enabled\n");
	info->accel_state->vsync = TRUE;
    } else
	info->accel_state->vsync = FALSE;

    if (!exaDriverInit(pScreen, info->accel_state->exa)) {
	free(info->accel_state->exa);
	return FALSE;
    }

    info->accel_state->XInited3D = FALSE;
    info->accel_state->copy_area = NULL;
    info->accel_state->src_obj[0].bo = NULL;
    info->accel_state->src_obj[1].bo = NULL;
    info->accel_state->dst_obj.bo = NULL;
    info->accel_state->copy_area_bo = NULL;
    info->accel_state->vbo.vb_start_op = -1;
    info->accel_state->cbuf.vb_start_op = -1;
    info->accel_state->finish_op = evergreen_finish_op;
    info->accel_state->vbo.verts_per_op = 3;
    info->accel_state->cbuf.verts_per_op = 1;
    RADEONVlineHelperClear(pScrn);

    radeon_vbo_init_lists(pScrn);

    if (!EVERGREENAllocShaders(pScrn, pScreen))
	return FALSE;

    if (!EVERGREENLoadShaders(pScrn))
	return FALSE;

    exaMarkSync(pScreen);

    return TRUE;

}

#endif
