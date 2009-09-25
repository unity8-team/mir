/*
 * Copyright 2008 Advanced Micro Devices, Inc.
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

#include "xf86.h"

#include "exa.h"

#include "radeon.h"
#include "radeon_reg.h"
#include "r600_shader.h"
#include "r600_reg.h"
#include "r600_state.h"

#include "radeon_video.h"

#include <X11/extensions/Xv.h>
#include "fourcc.h"

#include "damage.h"

/* Parameters for ITU-R BT.601 and ITU-R BT.709 colour spaces
   note the difference to the parameters used in overlay are due
   to 10bit vs. float calcs */
static REF_TRANSFORM trans[2] =
{
    {1.1643, 0.0, 1.5960, -0.3918, -0.8129, 2.0172, 0.0}, /* BT.601 */
    {1.1643, 0.0, 1.7927, -0.2132, -0.5329, 2.1124, 0.0}  /* BT.709 */
};

static void
R600DoneTexturedVideo(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    draw_config_t   draw_conf;
    vtx_resource_t  vtx_res;

    CLEAR (draw_conf);
    CLEAR (vtx_res);

    if (accel_state->vb_index == 0) {
        R600IBDiscard(pScrn, accel_state->ib);
        r600_vb_discard(pScrn);
        return;
    }

    accel_state->vb_size = accel_state->vb_index * 16;

    /* flush vertex cache */
    if ((info->ChipFamily == CHIP_FAMILY_RV610) ||
	(info->ChipFamily == CHIP_FAMILY_RV620) ||
	(info->ChipFamily == CHIP_FAMILY_RS780) ||
	(info->ChipFamily == CHIP_FAMILY_RS880) ||
	(info->ChipFamily == CHIP_FAMILY_RV710))
	cp_set_surface_sync(pScrn, accel_state->ib, TC_ACTION_ENA_bit,
			    accel_state->vb_size, accel_state->vb_mc_addr,
			    accel_state->vb_bo, RADEON_GEM_DOMAIN_GTT, 0);
    else
	cp_set_surface_sync(pScrn, accel_state->ib, VC_ACTION_ENA_bit,
			    accel_state->vb_size, accel_state->vb_mc_addr,
			    accel_state->vb_bo, RADEON_GEM_DOMAIN_GTT, 0);

    /* Vertex buffer setup */
    vtx_res.id              = SQ_VTX_RESOURCE_vs;
    vtx_res.vtx_size_dw     = 16 / 4;
    vtx_res.vtx_num_entries = accel_state->vb_size / 4;
    vtx_res.mem_req_size    = 1;
    vtx_res.vb_addr         = accel_state->vb_mc_addr;
    vtx_res.bo              = accel_state->vb_bo;
    set_vtx_resource        (pScrn, accel_state->ib, &vtx_res);

    draw_conf.prim_type          = DI_PT_RECTLIST;
    draw_conf.vgt_draw_initiator = DI_SRC_SEL_AUTO_INDEX;
    draw_conf.num_instances      = 1;
    draw_conf.num_indices        = vtx_res.vtx_num_entries / vtx_res.vtx_size_dw;
    draw_conf.index_type         = DI_INDEX_SIZE_16_BIT;

    draw_auto(pScrn, accel_state->ib, &draw_conf);

    wait_3d_idle_clean(pScrn, accel_state->ib);

    /* sync destination surface */
    cp_set_surface_sync(pScrn, accel_state->ib, (CB_ACTION_ENA_bit | CB0_DEST_BASE_ENA_bit),
			accel_state->dst_size, accel_state->dst_mc_addr,
			accel_state->dst_bo, 0, RADEON_GEM_DOMAIN_VRAM);

    R600CPFlushIndirect(pScrn, accel_state->ib);
}

void
R600DisplayTexturedVideo(ScrnInfoPtr pScrn, RADEONPortPrivPtr pPriv)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_accel_state *accel_state = info->accel_state;
    PixmapPtr pPixmap = pPriv->pPixmap;
    BoxPtr pBox = REGION_RECTS(&pPriv->clip);
    int nBox = REGION_NUM_RECTS(&pPriv->clip);
    int dstxoff, dstyoff;
    cb_config_t     cb_conf;
    tex_resource_t  tex_res;
    tex_sampler_t   tex_samp;
    shader_config_t vs_conf, ps_conf;
    /*
     * y' = y - .0625
     * u' = u - .5
     * v' = v - .5;
     *
     * r = 1.1643 * y' + 0.0     * u' + 1.5958  * v'
     * g = 1.1643 * y' - 0.39173 * u' - 0.81290 * v'
     * b = 1.1643 * y' + 2.017   * u' + 0.0     * v'
     *
     * DP3 might look like the straightforward solution
     * but we'd need to move the texture yuv values in
     * the same reg for this to work. Therefore use MADs.
     * Brightness just adds to the off constant.
     * Contrast is multiplication of luminance.
     * Saturation and hue change the u and v coeffs.
     * Default values (before adjustments - depend on colorspace):
     * yco = 1.1643
     * uco = 0, -0.39173, 2.017
     * vco = 1.5958, -0.8129, 0
     * off = -0.0625 * yco + -0.5 * uco[r] + -0.5 * vco[r],
     *       -0.0625 * yco + -0.5 * uco[g] + -0.5 * vco[g],
     *       -0.0625 * yco + -0.5 * uco[b] + -0.5 * vco[b],
     *
     * temp = MAD(yco, yuv.yyyy, off)
     * temp = MAD(uco, yuv.uuuu, temp)
     * result = MAD(vco, yuv.vvvv, temp)
     */
    /* TODO: calc consts in the shader */
    const float Loff = -0.0627;
    const float Coff = -0.502;
    float uvcosf, uvsinf;
    float yco;
    float uco[3], vco[3], off[3];
    float bright, cont, gamma;
    int ref = pPriv->transform_index;
    Bool needgamma = FALSE;
    float ps_alu_consts[12];
    float vs_alu_consts[4];

    cont = RTFContrast(pPriv->contrast);
    bright = RTFBrightness(pPriv->brightness);
    gamma = (float)pPriv->gamma / 1000.0;
    uvcosf = RTFSaturation(pPriv->saturation) * cos(RTFHue(pPriv->hue));
    uvsinf = RTFSaturation(pPriv->saturation) * sin(RTFHue(pPriv->hue));
    /* overlay video also does pre-gamma contrast/sat adjust, should we? */

    yco = trans[ref].RefLuma * cont;
    uco[0] = -trans[ref].RefRCr * uvsinf;
    uco[1] = trans[ref].RefGCb * uvcosf - trans[ref].RefGCr * uvsinf;
    uco[2] = trans[ref].RefBCb * uvcosf;
    vco[0] = trans[ref].RefRCr * uvcosf;
    vco[1] = trans[ref].RefGCb * uvsinf + trans[ref].RefGCr * uvcosf;
    vco[2] = trans[ref].RefBCb * uvsinf;
    off[0] = Loff * yco + Coff * (uco[0] + vco[0]) + bright;
    off[1] = Loff * yco + Coff * (uco[1] + vco[1]) + bright;
    off[2] = Loff * yco + Coff * (uco[2] + vco[2]) + bright;

    // XXX
    gamma = 1.0;

    if (gamma != 1.0) {
	needgamma = TRUE;
	/* note: gamma correction is out = in ^ gamma;
	   gpu can only do LG2/EX2 therefore we transform into
	   in ^ gamma = 2 ^ (log2(in) * gamma).
	   Lots of scalar ops, unfortunately (better solution?) -
	   without gamma that's 3 inst, with gamma it's 10...
	   could use different gamma factors per channel,
	   if that's of any use. */
    }

    /* setup the ps consts */
    ps_alu_consts[0] = off[0];
    ps_alu_consts[1] = off[1];
    ps_alu_consts[2] = off[2];
    ps_alu_consts[3] = yco;

    ps_alu_consts[4] = uco[0];
    ps_alu_consts[5] = uco[1];
    ps_alu_consts[6] = uco[2];
    ps_alu_consts[7] = gamma;

    ps_alu_consts[8] = vco[0];
    ps_alu_consts[9] = vco[1];
    ps_alu_consts[10] = vco[2];
    ps_alu_consts[11] = 0.0;

    CLEAR (cb_conf);
    CLEAR (tex_res);
    CLEAR (tex_samp);
    CLEAR (vs_conf);
    CLEAR (ps_conf);

#if defined(XF86DRM_MODE)
    if (info->cs) {
	accel_state->dst_mc_addr = 0;
	accel_state->src_mc_addr[0] = 0;
	accel_state->src_bo[0] = pPriv->src_bo;
	accel_state->src_bo[1] = NULL;
	accel_state->dst_bo = radeon_get_pixmap_bo(pPixmap);

	radeon_cs_space_reset_bos(info->cs);
	radeon_cs_space_add_persistent_bo(info->cs, accel_state->shaders_bo,
					  RADEON_GEM_DOMAIN_VRAM, 0);
	radeon_cs_space_add_persistent_bo(info->cs, accel_state->src_bo[0],
					  RADEON_GEM_DOMAIN_GTT | RADEON_GEM_DOMAIN_VRAM, 0);
	radeon_cs_space_add_persistent_bo(info->cs, accel_state->dst_bo,
					  0, RADEON_GEM_DOMAIN_VRAM);
	if (radeon_cs_space_check(info->cs))
	    return;
    } else
#endif
    {
	accel_state->dst_mc_addr = exaGetPixmapOffset(pPixmap) + info->fbLocation + pScrn->fbOffset;
	accel_state->src_mc_addr[0] = pPriv->src_offset + info->fbLocation + pScrn->fbOffset;
    }
    accel_state->dst_pitch = exaGetPixmapPitch(pPixmap) / (pPixmap->drawable.bitsPerPixel / 8);
    accel_state->src_pitch[0] = pPriv->src_pitch;

    /* bad pitch */
    if (accel_state->src_pitch[0] & 7)
	return;
    if (accel_state->dst_pitch & 7)
	return;

#ifdef COMPOSITE
    dstxoff = -pPixmap->screen_x + pPixmap->drawable.x;
    dstyoff = -pPixmap->screen_y + pPixmap->drawable.y;
#else
    dstxoff = 0;
    dstyoff = 0;
#endif

    r600_cp_start(pScrn);

    /* Init */
#if defined(XF86DRM_MODE)
    if (info->cs)
	accel_state->XInited3D = FALSE;
#endif
    start_3d(pScrn, accel_state->ib);

    set_default_state(pScrn, accel_state->ib);

    set_generic_scissor(pScrn, accel_state->ib, 0, 0, pPixmap->drawable.width, pPixmap->drawable.height);
    set_screen_scissor(pScrn, accel_state->ib, 0, 0, pPixmap->drawable.width, pPixmap->drawable.height);
    set_window_scissor(pScrn, accel_state->ib, 0, 0, pPixmap->drawable.width, pPixmap->drawable.height);

#if defined(XF86DRM_MODE)
    if (info->cs) {
	accel_state->vs_mc_addr = accel_state->xv_vs_offset;
	accel_state->ps_mc_addr = accel_state->xv_ps_offset;
    } else
#endif
    {
	accel_state->vs_mc_addr = info->fbLocation + pScrn->fbOffset + accel_state->shaders->offset +
	    accel_state->xv_vs_offset;
	accel_state->ps_mc_addr = info->fbLocation + pScrn->fbOffset + accel_state->shaders->offset +
	    accel_state->xv_ps_offset;
    }

    /* PS bool constant */
    switch(pPriv->id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	set_bool_consts(pScrn, accel_state->ib, SQ_BOOL_CONST_ps, (1 << 0));
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	set_bool_consts(pScrn, accel_state->ib, SQ_BOOL_CONST_ps, (0 << 0));
	break;
    }

    accel_state->vs_size = 512;
    accel_state->ps_size = 512;

    /* Shader */

    /* flush SQ cache */
    cp_set_surface_sync(pScrn, accel_state->ib, SH_ACTION_ENA_bit,
			accel_state->vs_size, accel_state->vs_mc_addr,
			accel_state->shaders_bo, RADEON_GEM_DOMAIN_VRAM, 0);

    vs_conf.shader_addr         = accel_state->vs_mc_addr;
    vs_conf.num_gprs            = 2;
    vs_conf.stack_size          = 0;
    vs_conf.bo                  = accel_state->shaders_bo;
    vs_setup                    (pScrn, accel_state->ib, &vs_conf);

    /* flush SQ cache */
    cp_set_surface_sync(pScrn, accel_state->ib, SH_ACTION_ENA_bit,
			accel_state->ps_size, accel_state->ps_mc_addr,
			accel_state->shaders_bo, RADEON_GEM_DOMAIN_VRAM, 0);

    ps_conf.shader_addr         = accel_state->ps_mc_addr;
    ps_conf.num_gprs            = 3;
    ps_conf.stack_size          = 1;
    ps_conf.uncached_first_inst = 1;
    ps_conf.clamp_consts        = 0;
    ps_conf.export_mode         = 2;
    ps_conf.bo                  = accel_state->shaders_bo;
    ps_setup                    (pScrn, accel_state->ib, &ps_conf);

    /* PS alu constants */
    set_alu_consts(pScrn, accel_state->ib, SQ_ALU_CONSTANT_ps,
		   sizeof(ps_alu_consts) / SQ_ALU_CONSTANT_offset, ps_alu_consts);

    /* Texture */
    switch(pPriv->id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	accel_state->src_size[0] = accel_state->src_pitch[0] * pPriv->h;

	/* flush texture cache */
	cp_set_surface_sync(pScrn, accel_state->ib, TC_ACTION_ENA_bit, accel_state->src_size[0],
			    accel_state->src_mc_addr[0],
			    accel_state->src_bo[0], RADEON_GEM_DOMAIN_VRAM, 0);

	/* Y texture */
	tex_res.id                  = 0;
	tex_res.w                   = pPriv->w;
	tex_res.h                   = pPriv->h;
	tex_res.pitch               = accel_state->src_pitch[0];
	tex_res.depth               = 0;
	tex_res.dim                 = SQ_TEX_DIM_2D;
	tex_res.base                = accel_state->src_mc_addr[0];
	tex_res.mip_base            = accel_state->src_mc_addr[0];
	tex_res.bo                  = accel_state->src_bo[0];
	tex_res.mip_bo              = accel_state->src_bo[0];

	tex_res.format              = FMT_8;
	tex_res.dst_sel_x           = SQ_SEL_X; /* Y */
	tex_res.dst_sel_y           = SQ_SEL_1;
	tex_res.dst_sel_z           = SQ_SEL_1;
	tex_res.dst_sel_w           = SQ_SEL_1;

	tex_res.request_size        = 1;
	tex_res.base_level          = 0;
	tex_res.last_level          = 0;
	tex_res.perf_modulation     = 0;
	tex_res.interlaced          = 0;
	set_tex_resource            (pScrn, accel_state->ib, &tex_res);

	/* Y sampler */
	tex_samp.id                 = 0;
	tex_samp.clamp_x            = SQ_TEX_CLAMP_LAST_TEXEL;
	tex_samp.clamp_y            = SQ_TEX_CLAMP_LAST_TEXEL;
	tex_samp.clamp_z            = SQ_TEX_WRAP;

	/* xxx: switch to bicubic */
	tex_samp.xy_mag_filter      = SQ_TEX_XY_FILTER_BILINEAR;
	tex_samp.xy_min_filter      = SQ_TEX_XY_FILTER_BILINEAR;

	tex_samp.z_filter           = SQ_TEX_Z_FILTER_NONE;
	tex_samp.mip_filter         = 0;			/* no mipmap */
	set_tex_sampler             (pScrn, accel_state->ib, &tex_samp);

	/* U or V texture */
	cp_set_surface_sync(pScrn, accel_state->ib, TC_ACTION_ENA_bit,
			    accel_state->src_size[0] / 4,
			    accel_state->src_mc_addr[0] + pPriv->planev_offset,
			    accel_state->src_bo[0], RADEON_GEM_DOMAIN_VRAM, 0);

	tex_res.id                  = 1;
	tex_res.format              = FMT_8;
	tex_res.w                   = pPriv->w >> 1;
	tex_res.h                   = pPriv->h >> 1;
	tex_res.pitch               = ((accel_state->src_pitch[0] >> 1) + 255) & ~255;
	tex_res.dst_sel_x           = SQ_SEL_X; /* V or U */
	tex_res.dst_sel_y           = SQ_SEL_1;
	tex_res.dst_sel_z           = SQ_SEL_1;
	tex_res.dst_sel_w           = SQ_SEL_1;
	tex_res.interlaced          = 0;

	tex_res.base                = accel_state->src_mc_addr[0] + pPriv->planev_offset;
	tex_res.mip_base            = accel_state->src_mc_addr[0] + pPriv->planev_offset;
	set_tex_resource            (pScrn, accel_state->ib, &tex_res);

	/* U or V sampler */
	tex_samp.id                 = 1;
	set_tex_sampler             (pScrn, accel_state->ib, &tex_samp);

	/* U or V texture */
	cp_set_surface_sync(pScrn, accel_state->ib, TC_ACTION_ENA_bit,
			    accel_state->src_size[0] / 4,
			    accel_state->src_mc_addr[0] + pPriv->planeu_offset,
			    accel_state->src_bo[0], RADEON_GEM_DOMAIN_VRAM, 0);

	tex_res.id                  = 2;
	tex_res.format              = FMT_8;
	tex_res.w                   = pPriv->w >> 1;
	tex_res.h                   = pPriv->h >> 1;
	tex_res.pitch               = ((accel_state->src_pitch[0] >> 1) + 255) & ~255;
	tex_res.dst_sel_x           = SQ_SEL_X; /* V or U */
	tex_res.dst_sel_y           = SQ_SEL_1;
	tex_res.dst_sel_z           = SQ_SEL_1;
	tex_res.dst_sel_w           = SQ_SEL_1;
	tex_res.interlaced          = 0;

	tex_res.base                = accel_state->src_mc_addr[0] + pPriv->planeu_offset;
	tex_res.mip_base            = accel_state->src_mc_addr[0] + pPriv->planeu_offset;
	set_tex_resource            (pScrn, accel_state->ib, &tex_res);

	/* UV sampler */
	tex_samp.id                 = 2;
	set_tex_sampler             (pScrn, accel_state->ib, &tex_samp);
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	accel_state->src_size[0] = accel_state->src_pitch[0] * pPriv->h;

	/* flush texture cache */
	cp_set_surface_sync(pScrn, accel_state->ib, TC_ACTION_ENA_bit, accel_state->src_size[0],
			    accel_state->src_mc_addr[0],
			    accel_state->src_bo[0], RADEON_GEM_DOMAIN_VRAM, 0);

	/* Y texture */
	tex_res.id                  = 0;
	tex_res.w                   = pPriv->w;
	tex_res.h                   = pPriv->h;
	tex_res.pitch               = accel_state->src_pitch[0] >> 1;
	tex_res.depth               = 0;
	tex_res.dim                 = SQ_TEX_DIM_2D;
	tex_res.base                = accel_state->src_mc_addr[0];
	tex_res.mip_base            = accel_state->src_mc_addr[0];
	tex_res.bo                  = accel_state->src_bo[0];
	tex_res.mip_bo              = accel_state->src_bo[0];

	tex_res.format              = FMT_8_8;
	if (pPriv->id == FOURCC_UYVY)
	    tex_res.dst_sel_x           = SQ_SEL_Y; /* Y */
	else
	    tex_res.dst_sel_x           = SQ_SEL_X; /* Y */
	tex_res.dst_sel_y           = SQ_SEL_1;
	tex_res.dst_sel_z           = SQ_SEL_1;
	tex_res.dst_sel_w           = SQ_SEL_1;

	tex_res.request_size        = 1;
	tex_res.base_level          = 0;
	tex_res.last_level          = 0;
	tex_res.perf_modulation     = 0;
	tex_res.interlaced          = 0;
	set_tex_resource            (pScrn, accel_state->ib, &tex_res);

	/* Y sampler */
	tex_samp.id                 = 0;
	tex_samp.clamp_x            = SQ_TEX_CLAMP_LAST_TEXEL;
	tex_samp.clamp_y            = SQ_TEX_CLAMP_LAST_TEXEL;
	tex_samp.clamp_z            = SQ_TEX_WRAP;

	/* xxx: switch to bicubic */
	tex_samp.xy_mag_filter      = SQ_TEX_XY_FILTER_BILINEAR;
	tex_samp.xy_min_filter      = SQ_TEX_XY_FILTER_BILINEAR;

	tex_samp.z_filter           = SQ_TEX_Z_FILTER_NONE;
	tex_samp.mip_filter         = 0;			/* no mipmap */
	set_tex_sampler             (pScrn, accel_state->ib, &tex_samp);

	/* UV texture */
	tex_res.id                  = 1;
	tex_res.format              = FMT_8_8_8_8;
	tex_res.w                   = pPriv->w >> 1;
	tex_res.h                   = pPriv->h;
	tex_res.pitch               = accel_state->src_pitch[0] >> 2;
	if (pPriv->id == FOURCC_UYVY) {
	    tex_res.dst_sel_x           = SQ_SEL_X; /* V */
	    tex_res.dst_sel_y           = SQ_SEL_Z; /* U */
	} else {
	    tex_res.dst_sel_x           = SQ_SEL_Y; /* V */
	    tex_res.dst_sel_y           = SQ_SEL_W; /* U */
	}
	tex_res.dst_sel_z           = SQ_SEL_1;
	tex_res.dst_sel_w           = SQ_SEL_1;
	tex_res.interlaced          = 0;

	tex_res.base                = accel_state->src_mc_addr[0];
	tex_res.mip_base            = accel_state->src_mc_addr[0];
	set_tex_resource            (pScrn, accel_state->ib, &tex_res);

	/* UV sampler */
	tex_samp.id                 = 1;
	set_tex_sampler             (pScrn, accel_state->ib, &tex_samp);
	break;
    }

    /* Render setup */
    BEGIN_BATCH(6);
    EREG(accel_state->ib, CB_SHADER_MASK,                      (0x0f << OUTPUT0_ENABLE_shift));
    EREG(accel_state->ib, CB_COLOR_CONTROL,                    (0xcc << ROP3_shift)); /* copy */
    END_BATCH();

    cb_conf.id = 0;
    cb_conf.w = accel_state->dst_pitch;
    cb_conf.h = pPixmap->drawable.height;
    cb_conf.base = accel_state->dst_mc_addr;
    cb_conf.bo = accel_state->dst_bo;

    switch (pPixmap->drawable.bitsPerPixel) {
    case 16:
	if (pPixmap->drawable.depth == 15) {
	    cb_conf.format = COLOR_1_5_5_5;
	    cb_conf.comp_swap = 1; /* ARGB */
	} else {
	    cb_conf.format = COLOR_5_6_5;
	    cb_conf.comp_swap = 2; /* RGB */
	}
	break;
    case 32:
	cb_conf.format = COLOR_8_8_8_8;
	cb_conf.comp_swap = 1; /* ARGB */
	break;
    default:
	return;
    }

    cb_conf.source_format = 1;
    cb_conf.blend_clamp = 1;
    set_render_target(pScrn, accel_state->ib, &cb_conf);

    /* Interpolator setup */
    /* export tex coords from VS */
    BEGIN_BATCH(18);
    EREG(accel_state->ib, SPI_VS_OUT_CONFIG, ((1 - 1) << VS_EXPORT_COUNT_shift));
    EREG(accel_state->ib, SPI_VS_OUT_ID_0, (0 << SEMANTIC_0_shift));

    /* Enabling flat shading needs both FLAT_SHADE_bit in SPI_PS_INPUT_CNTL_x
     * *and* FLAT_SHADE_ENA_bit in SPI_INTERP_CONTROL_0 */
    EREG(accel_state->ib, SPI_PS_IN_CONTROL_0,                 ((1 << NUM_INTERP_shift)));
    EREG(accel_state->ib, SPI_PS_IN_CONTROL_1,                 0);
    EREG(accel_state->ib, SPI_PS_INPUT_CNTL_0 + (0 <<2),       ((0    << SEMANTIC_shift)	|
								(0x03 << DEFAULT_VAL_shift)	|
								SEL_CENTROID_bit));
    EREG(accel_state->ib, SPI_INTERP_CONTROL_0,                0);
    END_BATCH();

    vs_alu_consts[0] = 1.0 / pPriv->w;
    vs_alu_consts[1] = 1.0 / pPriv->h;
    vs_alu_consts[2] = 0.0;
    vs_alu_consts[3] = 0.0;

    /* VS alu constants */
    set_alu_consts(pScrn, accel_state->ib, SQ_ALU_CONSTANT_vs,
		   sizeof(vs_alu_consts) / SQ_ALU_CONSTANT_offset, vs_alu_consts);

    if (pPriv->vsync) {
	xf86CrtcPtr crtc = radeon_xv_pick_best_crtc(pScrn,
						    pPriv->drw_x,
						    pPriv->drw_x + pPriv->dst_w,
						    pPriv->drw_y,
						    pPriv->drw_y + pPriv->dst_h);
	if (crtc) {
	    RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;

	    cp_wait_vline_sync(pScrn, accel_state->ib, pPixmap,
			       radeon_crtc->crtc_id,
			       pPriv->drw_y - crtc->y,
			       (pPriv->drw_y - crtc->y) + pPriv->dst_h);
	}
    }

    while (nBox--) {
	int srcX, srcY, srcw, srch;
	int dstX, dstY, dstw, dsth;
	float *vb;

        if (((accel_state->vb_index + 3) * 16) > accel_state->vb_total) {
            R600DoneTexturedVideo(pScrn);
	    r600_cp_start(pScrn);
        }

        vb = (pointer)((char*)accel_state->vb_ptr+accel_state->vb_index*16);

	dstX = pBox->x1 + dstxoff;
	dstY = pBox->y1 + dstyoff;
	dstw = pBox->x2 - pBox->x1;
	dsth = pBox->y2 - pBox->y1;

	srcX = pPriv->src_x;
	srcX += ((pBox->x1 - pPriv->drw_x) *
		 pPriv->src_w) / pPriv->dst_w;
	srcY = pPriv->src_y;
	srcY += ((pBox->y1 - pPriv->drw_y) *
		 pPriv->src_h) / pPriv->dst_h;

	srcw = (pPriv->src_w * dstw) / pPriv->dst_w;
	srch = (pPriv->src_h * dsth) / pPriv->dst_h;

	vb[0] = (float)dstX;
	vb[1] = (float)dstY;
	vb[2] = (float)srcX;
	vb[3] = (float)srcY;

	vb[4] = (float)dstX;
	vb[5] = (float)(dstY + dsth);
	vb[6] = (float)srcX;
	vb[7] = (float)(srcY + srch);

	vb[8] = (float)(dstX + dstw);
	vb[9] = (float)(dstY + dsth);
	vb[10] = (float)(srcX + srcw);
	vb[11] = (float)(srcY + srch);

	accel_state->vb_index += 3;

	pBox++;
    }

    R600DoneTexturedVideo(pScrn);

    DamageDamageRegion(pPriv->pDraw, &pPriv->clip);
}
