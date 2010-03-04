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
 *    Xiang Haihao <haihao.xiang@intel.com>
 *
 */

#include <sys/ioctl.h>

#include "i915_xvmc.h"
#include "i915_structs.h"
#include "i915_program.h"

#define YOFFSET(surface)        (surface->srf.offset)
#define UOFFSET(surface)        (surface->srf.offset + \
                                 SIZE_Y420(surface->width, surface->height) + \
                                 SIZE_UV420(surface->width, surface->height))
#define VOFFSET(surface)        (surface->srf.offset + \
                                 SIZE_Y420(surface->width, surface->height))

typedef union {
	int16_t component[2];
	int32_t v;
} vector_t;

static void i915_inst_arith(unsigned int *inst,
			    unsigned int op,
			    unsigned int dest,
			    unsigned int mask,
			    unsigned int saturate,
			    unsigned int src0, unsigned int src1,
			    unsigned int src2)
{
	dest = UREG(GET_UREG_TYPE(dest), GET_UREG_NR(dest));
	*inst = (op | A0_DEST(dest) | mask | saturate | A0_SRC0(src0));
	inst++;
	*inst = (A1_SRC0(src0) | A1_SRC1(src1));
	inst++;
	*inst = (A2_SRC1(src1) | A2_SRC2(src2));
}

static void i915_inst_decl(unsigned int *inst,
			   unsigned int type,
			   unsigned int nr, unsigned int d0_flags)
{
	unsigned int reg = UREG(type, nr);

	*inst = (D0_DCL | D0_DEST(reg) | d0_flags);
	inst++;
	*inst = D1_MBZ;
	inst++;
	*inst = D2_MBZ;
}

static void i915_inst_texld(unsigned int *inst,
			    unsigned int op,
			    unsigned int dest,
			    unsigned int coord, unsigned int sampler)
{
	dest = UREG(GET_UREG_TYPE(dest), GET_UREG_NR(dest));
	*inst = (op | T0_DEST(dest) | T0_SAMPLER(sampler));
	inst++;
	*inst = T1_ADDRESS_REG(coord);
	inst++;
	*inst = T2_MBZ;
}

static void i915_emit_batch(void *data, int size, int flag)
{
	intelBatchbufferData(data, size, flag);
}

/* one time context initialization buffer */
static uint32_t *one_time_load_state_imm1;
static uint32_t *one_time_load_indirect;
static int one_time_load_state_imm1_size, one_time_load_indirect_size;

/* load indirect buffer for mc rendering */
static uint32_t *mc_render_load_indirect;
static int mc_render_load_indirect_size;

static void i915_mc_one_time_context_init(XvMCContext * context)
{
	unsigned int dest, src0, src1, src2;
	i915XvMCContext *pI915XvMC = (i915XvMCContext *) context->privData;
	int i;
	struct i915_3dstate_sampler_state *sampler_state;
	struct i915_3dstate_pixel_shader_program *pixel_shader_program;
	struct i915_3dstate_pixel_shader_constants *pixel_shader_constants;

	/* sampler static state */
	sampler_state = (struct i915_3dstate_sampler_state *)pI915XvMC->ssb.map;
	/* pixel shader static state */
	pixel_shader_program =
	    (struct i915_3dstate_pixel_shader_program *)pI915XvMC->psp.map;
	/* pixel shader contant static state */
	pixel_shader_constants =
	    (struct i915_3dstate_pixel_shader_constants *)pI915XvMC->psc.map;

	memset(sampler_state, 0, sizeof(*sampler_state));
	sampler_state->dw0.type = CMD_3D;
	sampler_state->dw0.opcode = OPC_3DSTATE_SAMPLER_STATE;
	sampler_state->dw0.length = 6;
	sampler_state->dw1.sampler_masker = SAMPLER_SAMPLER0 | SAMPLER_SAMPLER1;

	sampler_state->sampler0.ts0.reverse_gamma = 0;
	sampler_state->sampler0.ts0.planar2packet = 0;
	sampler_state->sampler0.ts0.color_conversion = 0;
	sampler_state->sampler0.ts0.chromakey_index = 0;
	sampler_state->sampler0.ts0.base_level = 0;
	sampler_state->sampler0.ts0.mip_filter = MIPFILTER_NONE;	/* NONE */
	sampler_state->sampler0.ts0.mag_filter = MAPFILTER_LINEAR;	/* LINEAR */
	sampler_state->sampler0.ts0.min_filter = MAPFILTER_LINEAR;	/* LINEAR */
	sampler_state->sampler0.ts0.lod_bias = 0;	/* 0.0 */
	sampler_state->sampler0.ts0.shadow_enable = 0;
	sampler_state->sampler0.ts0.max_anisotropy = ANISORATIO_2;
	sampler_state->sampler0.ts0.shadow_function = PREFILTEROP_ALWAYS;
	sampler_state->sampler0.ts1.min_lod = 0;	/* 0.0 Maximum Mip Level */
	sampler_state->sampler0.ts1.kill_pixel = 0;
	sampler_state->sampler0.ts1.keyed_texture_filter = 0;
	sampler_state->sampler0.ts1.chromakey_enable = 0;
	sampler_state->sampler0.ts1.tcx_control = TEXCOORDMODE_CLAMP;
	sampler_state->sampler0.ts1.tcy_control = TEXCOORDMODE_CLAMP;
	sampler_state->sampler0.ts1.tcz_control = TEXCOORDMODE_CLAMP;
	sampler_state->sampler0.ts1.normalized_coor = 0;
	sampler_state->sampler0.ts1.map_index = 0;
	sampler_state->sampler0.ts1.east_deinterlacer = 0;
	sampler_state->sampler0.ts2.default_color = 0;

	sampler_state->sampler1.ts0.reverse_gamma = 0;
	sampler_state->sampler1.ts0.planar2packet = 0;
	sampler_state->sampler1.ts0.color_conversion = 0;
	sampler_state->sampler1.ts0.chromakey_index = 0;
	sampler_state->sampler1.ts0.base_level = 0;
	sampler_state->sampler1.ts0.mip_filter = MIPFILTER_NONE;	/* NONE */
	sampler_state->sampler1.ts0.mag_filter = MAPFILTER_LINEAR;	/* LINEAR */
	sampler_state->sampler1.ts0.min_filter = MAPFILTER_LINEAR;	/* LINEAR */
	sampler_state->sampler1.ts0.lod_bias = 0;	/* 0.0 */
	sampler_state->sampler1.ts0.shadow_enable = 0;
	sampler_state->sampler1.ts0.max_anisotropy = ANISORATIO_2;
	sampler_state->sampler1.ts0.shadow_function = PREFILTEROP_ALWAYS;
	sampler_state->sampler1.ts1.min_lod = 0;	/* 0.0 Maximum Mip Level */
	sampler_state->sampler1.ts1.kill_pixel = 0;
	sampler_state->sampler1.ts1.keyed_texture_filter = 0;
	sampler_state->sampler1.ts1.chromakey_enable = 0;
	sampler_state->sampler1.ts1.tcx_control = TEXCOORDMODE_CLAMP;
	sampler_state->sampler1.ts1.tcy_control = TEXCOORDMODE_CLAMP;
	sampler_state->sampler1.ts1.tcz_control = TEXCOORDMODE_CLAMP;
	sampler_state->sampler1.ts1.normalized_coor = 0;
	sampler_state->sampler1.ts1.map_index = 1;
	sampler_state->sampler1.ts1.east_deinterlacer = 0;
	sampler_state->sampler1.ts2.default_color = 0;

	memset(pixel_shader_program, 0, sizeof(*pixel_shader_program));
	pixel_shader_program->shader0.type = CMD_3D;
	pixel_shader_program->shader0.opcode = OPC_3DSTATE_PIXEL_SHADER_PROGRAM;
	pixel_shader_program->shader0.retain = 1;
	pixel_shader_program->shader0.length = 2;	/* 1 inst */
	i = 0;

	dest = UREG(REG_TYPE_OC, 0);
	src0 = UREG(REG_TYPE_CONST, 0);
	src1 = 0;
	src2 = 0;
	i915_inst_arith(&pixel_shader_program->inst0[i], A0_MOV,
			dest, A0_DEST_CHANNEL_ALL, A0_DEST_SATURATE, src0, src1,
			src2);

	pixel_shader_program->shader1.type = CMD_3D;
	pixel_shader_program->shader1.opcode = OPC_3DSTATE_PIXEL_SHADER_PROGRAM;
	pixel_shader_program->shader1.retain = 1;
	pixel_shader_program->shader1.length = 14;	/* 5 inst */
	i = 0;
	/* dcl t0.xy */
	i915_inst_decl(&pixel_shader_program->inst1[i], REG_TYPE_T, T_TEX0,
		       D0_CHANNEL_XY);
	i += 3;
	/* dcl t1.xy */
	i915_inst_decl(&pixel_shader_program->inst1[i], REG_TYPE_T, T_TEX1,
		       D0_CHANNEL_XY);
	/* dcl_2D s0 */
	i += 3;
	i915_inst_decl(&pixel_shader_program->inst1[i], REG_TYPE_S, 0,
		       D0_SAMPLE_TYPE_2D);
	/* texld r0, t0, s0 */
	i += 3;
	dest = UREG(REG_TYPE_R, 0);
	src0 = UREG(REG_TYPE_T, 0);	/* COORD */
	src1 = UREG(REG_TYPE_S, 0);	/* SAMPLER */
	i915_inst_texld(&pixel_shader_program->inst1[i], T0_TEXLD, dest, src0,
			src1);
	/* mov oC, r0 */
	i += 3;
	dest = UREG(REG_TYPE_OC, 0);
	src0 = UREG(REG_TYPE_R, 0);
	src1 = src2 = 0;
	i915_inst_arith(&pixel_shader_program->inst1[i], A0_MOV, dest,
			A0_DEST_CHANNEL_ALL, A0_DEST_SATURATE, src0, src1,
			src2);

	pixel_shader_program->shader2.type = CMD_3D;
	pixel_shader_program->shader2.opcode = OPC_3DSTATE_PIXEL_SHADER_PROGRAM;
	pixel_shader_program->shader2.retain = 1;
	pixel_shader_program->shader2.length = 14;	/* 5 inst */
	i = 0;
	/* dcl t2.xy */
	i915_inst_decl(&pixel_shader_program->inst2[i], REG_TYPE_T, T_TEX2,
		       D0_CHANNEL_XY);
	/* dcl t3.xy */
	i += 3;
	i915_inst_decl(&pixel_shader_program->inst2[i], REG_TYPE_T, T_TEX3,
		       D0_CHANNEL_XY);
	/* dcl_2D s1 */
	i += 3;
	i915_inst_decl(&pixel_shader_program->inst2[i], REG_TYPE_S, 1,
		       D0_SAMPLE_TYPE_2D);
	/* texld r0, t2, s1 */
	i += 3;
	dest = UREG(REG_TYPE_R, 0);
	src0 = UREG(REG_TYPE_T, 2);	/* COORD */
	src1 = UREG(REG_TYPE_S, 1);	/* SAMPLER */
	i915_inst_texld(&pixel_shader_program->inst2[i], T0_TEXLD, dest, src0,
			src1);
	/* mov oC, r0 */
	i += 3;
	dest = UREG(REG_TYPE_OC, 0);
	src0 = UREG(REG_TYPE_R, 0);
	src1 = src2 = 0;
	i915_inst_arith(&pixel_shader_program->inst2[i], A0_MOV, dest,
			A0_DEST_CHANNEL_ALL, A0_DEST_SATURATE, src0, src1,
			src2);

	/* Shader 3 */
	pixel_shader_program->shader3.type = CMD_3D;
	pixel_shader_program->shader3.opcode = OPC_3DSTATE_PIXEL_SHADER_PROGRAM;
	pixel_shader_program->shader3.retain = 1;
	pixel_shader_program->shader3.length = 29;	/* 10 inst */
	i = 0;
	/* dcl t0.xy */
	i915_inst_decl(&pixel_shader_program->inst3[i], REG_TYPE_T, T_TEX0,
		       D0_CHANNEL_XY);
	/* dcl t1.xy */
	i += 3;
	i915_inst_decl(&pixel_shader_program->inst3[i], REG_TYPE_T, T_TEX1,
		       D0_CHANNEL_XY);
	/* dcl t2.xy */
	i += 3;
	i915_inst_decl(&pixel_shader_program->inst3[i], REG_TYPE_T, T_TEX2,
		       D0_CHANNEL_XY);
	/* dcl t3.xy */
	i += 3;
	i915_inst_decl(&pixel_shader_program->inst3[i], REG_TYPE_T, T_TEX3,
		       D0_CHANNEL_XY);
	/* dcl_2D s0 */
	i += 3;
	i915_inst_decl(&pixel_shader_program->inst3[i], REG_TYPE_S, 0,
		       D0_SAMPLE_TYPE_2D);
	/* dcl_2D s1 */
	i += 3;
	i915_inst_decl(&pixel_shader_program->inst3[i], REG_TYPE_S, 1,
		       D0_SAMPLE_TYPE_2D);
	/* texld r0, t0, s0 */
	i += 3;
	dest = UREG(REG_TYPE_R, 0);
	src0 = UREG(REG_TYPE_T, 0);	/* COORD */
	src1 = UREG(REG_TYPE_S, 0);	/* SAMPLER */
	i915_inst_texld(&pixel_shader_program->inst3[i], T0_TEXLD, dest, src0,
			src1);
	/* texld r1, t2, s1 */
	i += 3;
	dest = UREG(REG_TYPE_R, 1);
	src0 = UREG(REG_TYPE_T, 2);	/* COORD */
	src1 = UREG(REG_TYPE_S, 1);	/* SAMPLER */
	i915_inst_texld(&pixel_shader_program->inst3[i], T0_TEXLD, dest, src0,
			src1);
	/* add r0, r0, r1 */
	i += 3;
	dest = UREG(REG_TYPE_R, 0);
	src0 = UREG(REG_TYPE_R, 0);
	src1 = UREG(REG_TYPE_R, 1);
	src2 = 0;
	i915_inst_arith(&pixel_shader_program->inst3[i], A0_ADD, dest,
			A0_DEST_CHANNEL_ALL, 0 /* A0_DEST_SATURATE */ , src0,
			src1, src2);
	/* mul oC, r0, c0 */
	i += 3;
	dest = UREG(REG_TYPE_OC, 0);
	src0 = UREG(REG_TYPE_R, 0);
	src1 = UREG(REG_TYPE_CONST, 0);
	src2 = 0;
	i915_inst_arith(&pixel_shader_program->inst3[i], A0_MUL, dest,
			A0_DEST_CHANNEL_ALL, A0_DEST_SATURATE, src0, src1,
			src2);

	memset(pixel_shader_constants, 0, sizeof(*pixel_shader_constants));
	pixel_shader_constants->dw0.type = CMD_3D;
	pixel_shader_constants->dw0.opcode = OPC_3DSTATE_PIXEL_SHADER_CONSTANTS;
	pixel_shader_constants->dw0.length = 4;
	pixel_shader_constants->dw1.reg_mask = REG_CR0;
	pixel_shader_constants->value.x = 0.5;
	pixel_shader_constants->value.y = 0.5;
	pixel_shader_constants->value.z = 0.5;
	pixel_shader_constants->value.w = 0.5;

}

static void i915_mc_one_time_state_init(XvMCContext * context)
{
	struct s3_dword *s3 = NULL;
	struct s6_dword *s6 = NULL;
	dis_state *dis = NULL;
	ssb_state *ssb = NULL;
	psp_state *psp = NULL;
	psc_state *psc = NULL;
	i915XvMCContext *pI915XvMC = (i915XvMCContext *) context->privData;
	struct i915_3dstate_load_state_immediate_1 *load_state_immediate_1;
	struct i915_3dstate_load_indirect *load_indirect;
	int mem_select;

	/* 3DSTATE_LOAD_STATE_IMMEDIATE_1 */
	one_time_load_state_imm1_size =
	    sizeof(*load_state_immediate_1) + sizeof(*s3) + sizeof(*s6);
	one_time_load_state_imm1 = calloc(1, one_time_load_state_imm1_size);
	load_state_immediate_1 = (struct i915_3dstate_load_state_immediate_1 *)
	    one_time_load_state_imm1;
	load_state_immediate_1->dw0.type = CMD_3D;
	load_state_immediate_1->dw0.opcode = OPC_3DSTATE_LOAD_STATE_IMMEDIATE_1;
	load_state_immediate_1->dw0.load_s3 = 1;
	load_state_immediate_1->dw0.load_s6 = 1;
	load_state_immediate_1->dw0.length =
	    (one_time_load_state_imm1_size >> 2) - 2;

	s3 = (struct s3_dword *)(++load_state_immediate_1);
	s3->set0_pcd = 1;
	s3->set1_pcd = 1;
	s3->set2_pcd = 1;
	s3->set3_pcd = 1;
	s3->set4_pcd = 1;
	s3->set5_pcd = 1;
	s3->set6_pcd = 1;
	s3->set7_pcd = 1;

	s6 = (struct s6_dword *)(++s3);
	s6->alpha_test_enable = 0;
	s6->alpha_test_function = 0;
	s6->alpha_reference_value = 0;
	s6->depth_test_enable = 1;
	s6->depth_test_function = 0;
	s6->color_buffer_blend = 0;
	s6->color_blend_function = 0;
	s6->src_blend_factor = 1;
	s6->dest_blend_factor = 1;
	s6->depth_buffer_write = 0;
	s6->color_buffer_write = 1;
	s6->triangle_pv = 0;

	/* 3DSTATE_LOAD_INDIRECT */
	one_time_load_indirect_size =
	    sizeof(*load_indirect) + sizeof(*dis) + sizeof(*ssb) +
	    sizeof(*psp) + sizeof(*psc);
	one_time_load_indirect = calloc(1, one_time_load_indirect_size);
	load_indirect =
	    (struct i915_3dstate_load_indirect *)one_time_load_indirect;
	load_indirect->dw0.type = CMD_3D;
	load_indirect->dw0.opcode = OPC_3DSTATE_LOAD_INDIRECT;
	load_indirect->dw0.block_mask =
	    BLOCK_DIS | BLOCK_SSB | BLOCK_PSP | BLOCK_PSC;
	load_indirect->dw0.length = (one_time_load_indirect_size >> 2) - 2;

	if (pI915XvMC->deviceID == PCI_CHIP_I915_G ||
	    pI915XvMC->deviceID == PCI_CHIP_I915_GM)
		mem_select = 0;	/* use physical address */
	else
		mem_select = 1;	/* use gfx address */

	load_indirect->dw0.mem_select = mem_select;

	/* Dynamic indirect state buffer */
	dis = (dis_state *) (++load_indirect);
	dis->dw0.valid = 0;
	dis->dw0.reset = 0;
	dis->dw0.buffer_address = 0;

	/* Sample state buffer */
	ssb = (ssb_state *) (++dis);
	ssb->dw0.valid = 1;
	ssb->dw0.force = 1;
	ssb->dw1.length = 7;	/* 8 - 1 */

	if (mem_select)
		ssb->dw0.buffer_address = (pI915XvMC->ssb.offset >> 2);
	else
		ssb->dw0.buffer_address = (pI915XvMC->ssb.bus_addr >> 2);

	/* Pixel shader program buffer */
	psp = (psp_state *) (++ssb);
	psp->dw0.valid = 1;
	psp->dw0.force = 1;
	psp->dw1.length = 66;	/* 4 + 16 + 16 + 31 - 1 */

	if (mem_select)
		psp->dw0.buffer_address = (pI915XvMC->psp.offset >> 2);
	else
		psp->dw0.buffer_address = (pI915XvMC->psp.bus_addr >> 2);

	/* Pixel shader constant buffer */
	psc = (psc_state *) (++psp);
	psc->dw0.valid = 1;
	psc->dw0.force = 1;
	psc->dw1.length = 5;	/* 6 - 1 */

	if (mem_select)
		psc->dw0.buffer_address = (pI915XvMC->psc.offset >> 2);
	else
		psc->dw0.buffer_address = (pI915XvMC->psc.bus_addr >> 2);
}

static void i915_mc_one_time_state_emit(void)
{
	i915_emit_batch(one_time_load_state_imm1, one_time_load_state_imm1_size,
			0);
	i915_emit_batch(one_time_load_indirect, one_time_load_indirect_size, 0);
}

static void i915_mc_static_indirect_state_init(XvMCContext * context)
{
	i915XvMCContext *pI915XvMC = (i915XvMCContext *) context->privData;
	struct i915_mc_static_indirect_state_buffer *buffer_info =
	    (struct i915_mc_static_indirect_state_buffer *)pI915XvMC->sis.map;

	memset(buffer_info, 0, sizeof(*buffer_info));
	/* dest Y */
	buffer_info->dest_y.dw0.type = CMD_3D;
	buffer_info->dest_y.dw0.opcode = OPC_3DSTATE_BUFFER_INFO;
	buffer_info->dest_y.dw0.length = 1;
	buffer_info->dest_y.dw1.aux_id = 0;
	buffer_info->dest_y.dw1.buffer_id = BUFFERID_COLOR_BACK;
	buffer_info->dest_y.dw1.fence_regs = 0;	/* disabled *//* FIXME: tiled y for performance */
	buffer_info->dest_y.dw1.tiled_surface = 0;	/* linear */
	buffer_info->dest_y.dw1.walk = TILEWALK_XMAJOR;

	/* dest U */
	buffer_info->dest_u.dw0.type = CMD_3D;
	buffer_info->dest_u.dw0.opcode = OPC_3DSTATE_BUFFER_INFO;
	buffer_info->dest_u.dw0.length = 1;
	buffer_info->dest_u.dw1.aux_id = 0;
	buffer_info->dest_u.dw1.buffer_id = BUFFERID_COLOR_AUX;
	buffer_info->dest_u.dw1.fence_regs = 0;
	buffer_info->dest_u.dw1.tiled_surface = 0;
	buffer_info->dest_u.dw1.walk = TILEWALK_XMAJOR;

	/* dest V */
	buffer_info->dest_v.dw0.type = CMD_3D;
	buffer_info->dest_v.dw0.opcode = OPC_3DSTATE_BUFFER_INFO;
	buffer_info->dest_v.dw0.length = 1;
	buffer_info->dest_v.dw1.aux_id = 1;
	buffer_info->dest_v.dw1.buffer_id = BUFFERID_COLOR_AUX;
	buffer_info->dest_v.dw1.fence_regs = 0;
	buffer_info->dest_v.dw1.tiled_surface = 0;
	buffer_info->dest_v.dw1.walk = TILEWALK_XMAJOR;

	buffer_info->dest_buf.dw0.type = CMD_3D;
	buffer_info->dest_buf.dw0.opcode = OPC_3DSTATE_DEST_BUFFER_VARIABLES;
	buffer_info->dest_buf.dw0.length = 0;
	buffer_info->dest_buf.dw1.dest_v_bias = 8;	/* 0.5 */
	buffer_info->dest_buf.dw1.dest_h_bias = 8;	/* 0.5 */
	buffer_info->dest_buf.dw1.color_fmt = COLORBUFFER_8BIT;
	buffer_info->dest_buf.dw1.v_ls = 0;	/* fill later */
	buffer_info->dest_buf.dw1.v_ls_offset = 0;	/* fill later */

	buffer_info->dest_buf_mpeg.dw0.type = CMD_3D;
	buffer_info->dest_buf_mpeg.dw0.opcode =
	    OPC_3DSTATE_DEST_BUFFER_VARIABLES_MPEG;
	buffer_info->dest_buf_mpeg.dw0.length = 1;
	buffer_info->dest_buf_mpeg.dw1.decode_mode = MPEG_DECODE_MC;
	buffer_info->dest_buf_mpeg.dw1.rcontrol = 0;	/* for MPEG-1/MPEG-2 */
	buffer_info->dest_buf_mpeg.dw1.bidir_avrg_control = 0;	/* for MPEG-1/MPEG-2/MPEG-4 */
	buffer_info->dest_buf_mpeg.dw1.abort_on_error = 1;
	buffer_info->dest_buf_mpeg.dw1.intra8 = 0;	/* 16-bit formatted correction data */
	buffer_info->dest_buf_mpeg.dw1.tff = 1;	/* fill later */

	buffer_info->dest_buf_mpeg.dw1.v_subsample_factor = MC_SUB_1V;
	buffer_info->dest_buf_mpeg.dw1.h_subsample_factor = MC_SUB_1H;

	buffer_info->corr.dw0.type = CMD_3D;
	buffer_info->corr.dw0.opcode = OPC_3DSTATE_BUFFER_INFO;
	buffer_info->corr.dw0.length = 1;
	buffer_info->corr.dw1.aux_id = 0;
	buffer_info->corr.dw1.buffer_id = BUFFERID_MC_INTRA_CORR;
	buffer_info->corr.dw1.aux_id = 0;
	buffer_info->corr.dw1.fence_regs = 0;
	buffer_info->corr.dw1.tiled_surface = 0;
	buffer_info->corr.dw1.walk = 0;
	buffer_info->corr.dw1.pitch = 0;
	buffer_info->corr.dw2.base_address = (pI915XvMC->corrdata.offset >> 2);	/* starting DWORD address */
}

static void i915_mc_static_indirect_state_set(XvMCContext * context,
					      XvMCSurface * dest,
					      unsigned int picture_structure,
					      unsigned int flags,
					      unsigned int picture_coding_type)
{
	i915XvMCContext *pI915XvMC = (i915XvMCContext *) context->privData;
	i915XvMCSurface *pI915Surface = (i915XvMCSurface *) dest->privData;
	struct i915_mc_static_indirect_state_buffer *buffer_info =
	    (struct i915_mc_static_indirect_state_buffer *)pI915XvMC->sis.map;
	unsigned int w = dest->width;

	buffer_info->dest_y.dw1.pitch = (pI915Surface->yStride >> 2);	/* in DWords */
	buffer_info->dest_y.dw2.base_address = (YOFFSET(pI915Surface) >> 2);	/* starting DWORD address */
	buffer_info->dest_u.dw1.pitch = (pI915Surface->uvStride >> 2);	/* in DWords */
	buffer_info->dest_u.dw2.base_address = (UOFFSET(pI915Surface) >> 2);	/* starting DWORD address */
	buffer_info->dest_v.dw1.pitch = (pI915Surface->uvStride >> 2);	/* in Dwords */
	buffer_info->dest_v.dw2.base_address = (VOFFSET(pI915Surface) >> 2);	/* starting DWORD address */

	if ((picture_structure & XVMC_FRAME_PICTURE) == XVMC_FRAME_PICTURE) {
		;
	} else if ((picture_structure & XVMC_FRAME_PICTURE) == XVMC_TOP_FIELD) {
		buffer_info->dest_buf.dw1.v_ls = 1;
	} else if ((picture_structure & XVMC_FRAME_PICTURE) ==
		   XVMC_BOTTOM_FIELD) {
		buffer_info->dest_buf.dw1.v_ls = 1;
		buffer_info->dest_buf.dw1.v_ls_offset = 1;
	}

	if (picture_structure & XVMC_FRAME_PICTURE) {
		;
	} else if (picture_structure & XVMC_TOP_FIELD) {
		if (flags & XVMC_SECOND_FIELD)
			buffer_info->dest_buf_mpeg.dw1.tff = 0;
		else
			buffer_info->dest_buf_mpeg.dw1.tff = 1;
	} else if (picture_structure & XVMC_BOTTOM_FIELD) {
		if (flags & XVMC_SECOND_FIELD)
			buffer_info->dest_buf_mpeg.dw1.tff = 1;
		else
			buffer_info->dest_buf_mpeg.dw1.tff = 0;
	}

	buffer_info->dest_buf_mpeg.dw1.picture_width = (dest->width >> 4);	/* in macroblocks */
	buffer_info->dest_buf_mpeg.dw2.picture_coding_type =
	    picture_coding_type;
}

static void i915_mc_map_state_init(XvMCContext * context)
{
	i915XvMCContext *pI915XvMC = (i915XvMCContext *) context->privData;
	unsigned int w = context->width;
	unsigned int h = context->height;
	struct i915_mc_map_state *map_state;

	map_state = (struct i915_mc_map_state *)pI915XvMC->msb.map;

	memset(map_state, 0, sizeof(*map_state));

	/* 3DSATE_MAP_STATE: Y */
	map_state->y_map.dw0.type = CMD_3D;
	map_state->y_map.dw0.opcode = OPC_3DSTATE_MAP_STATE;
	map_state->y_map.dw0.retain = 1;
	map_state->y_map.dw0.length = 6;
	map_state->y_map.dw1.map_mask = MAP_MAP0 | MAP_MAP1;

	/* Y Forward (Past) */
	map_state->y_forward.tm0.v_ls_offset = 0;
	map_state->y_forward.tm0.v_ls = 0;
	map_state->y_forward.tm1.tile_walk = TILEWALK_XMAJOR;
	map_state->y_forward.tm1.tiled_surface = 0;
	map_state->y_forward.tm1.utilize_fence_regs = 0;
	map_state->y_forward.tm1.texel_fmt = 0;	/* 8bit */
	map_state->y_forward.tm1.surface_fmt = 1;	/* 8bit */
	map_state->y_forward.tm1.width = w - 1;
	map_state->y_forward.tm1.height = h - 1;
	map_state->y_forward.tm2.depth = 0;
	map_state->y_forward.tm2.max_lod = 0;
	map_state->y_forward.tm2.cube_face = 0;

	/* Y Backward (Future) */
	map_state->y_backward.tm0.v_ls_offset = 0;
	map_state->y_backward.tm0.v_ls = 0;
	map_state->y_backward.tm1.tile_walk = TILEWALK_XMAJOR;
	map_state->y_backward.tm1.tiled_surface = 0;
	map_state->y_backward.tm1.utilize_fence_regs = 0;
	map_state->y_backward.tm1.texel_fmt = 0;	/* 8bit */
	map_state->y_backward.tm1.surface_fmt = 1;	/* 8bit */
	map_state->y_backward.tm1.width = w - 1;
	map_state->y_backward.tm1.height = h - 1;
	map_state->y_backward.tm2.depth = 0;
	map_state->y_backward.tm2.max_lod = 0;
	map_state->y_backward.tm2.cube_face = 0;

	/* 3DSATE_MAP_STATE: U */
	map_state->u_map.dw0.type = CMD_3D;
	map_state->u_map.dw0.opcode = OPC_3DSTATE_MAP_STATE;
	map_state->u_map.dw0.retain = 1;
	map_state->u_map.dw0.length = 6;
	map_state->u_map.dw1.map_mask = MAP_MAP0 | MAP_MAP1;

	/* U Forward */
	map_state->u_forward.tm0.v_ls_offset = 0;
	map_state->u_forward.tm0.v_ls = 0;
	map_state->u_forward.tm1.tile_walk = TILEWALK_XMAJOR;
	map_state->u_forward.tm1.tiled_surface = 0;
	map_state->u_forward.tm1.utilize_fence_regs = 0;
	map_state->u_forward.tm1.texel_fmt = 0;	/* 8bit */
	map_state->u_forward.tm1.surface_fmt = 1;	/* 8bit */
	map_state->u_forward.tm1.width = (w >> 1) - 1;
	map_state->u_forward.tm1.height = (h >> 1) - 1;
	map_state->u_forward.tm2.depth = 0;
	map_state->u_forward.tm2.max_lod = 0;
	map_state->u_forward.tm2.cube_face = 0;

	/* U Backward */
	map_state->u_backward.tm0.v_ls_offset = 0;
	map_state->u_backward.tm0.v_ls = 0;
	map_state->u_backward.tm1.tile_walk = TILEWALK_XMAJOR;
	map_state->u_backward.tm1.tiled_surface = 0;
	map_state->u_backward.tm1.utilize_fence_regs = 0;
	map_state->u_backward.tm1.texel_fmt = 0;
	map_state->u_backward.tm1.surface_fmt = 1;
	map_state->u_backward.tm1.width = (w >> 1) - 1;
	map_state->u_backward.tm1.height = (h >> 1) - 1;
	map_state->u_backward.tm2.depth = 0;
	map_state->u_backward.tm2.max_lod = 0;
	map_state->u_backward.tm2.cube_face = 0;

	/* 3DSATE_MAP_STATE: V */
	map_state->v_map.dw0.type = CMD_3D;
	map_state->v_map.dw0.opcode = OPC_3DSTATE_MAP_STATE;
	map_state->v_map.dw0.retain = 1;
	map_state->v_map.dw0.length = 6;
	map_state->v_map.dw1.map_mask = MAP_MAP0 | MAP_MAP1;

	/* V Forward */
	map_state->v_forward.tm0.v_ls_offset = 0;
	map_state->v_forward.tm0.v_ls = 0;
	map_state->v_forward.tm1.tile_walk = TILEWALK_XMAJOR;
	map_state->v_forward.tm1.tiled_surface = 0;
	map_state->v_forward.tm1.utilize_fence_regs = 0;
	map_state->v_forward.tm1.texel_fmt = 0;
	map_state->v_forward.tm1.surface_fmt = 1;
	map_state->v_forward.tm1.width = (w >> 1) - 1;
	map_state->v_forward.tm1.height = (h >> 1) - 1;
	map_state->v_forward.tm2.depth = 0;
	map_state->v_forward.tm2.max_lod = 0;
	map_state->v_forward.tm2.cube_face = 0;

	/* V Backward */
	map_state->v_backward.tm0.v_ls_offset = 0;
	map_state->v_backward.tm0.v_ls = 0;
	map_state->v_backward.tm1.tile_walk = TILEWALK_XMAJOR;
	map_state->v_backward.tm1.tiled_surface = 0;
	map_state->v_backward.tm1.utilize_fence_regs = 0;
	map_state->v_backward.tm1.texel_fmt = 0;
	map_state->v_backward.tm1.surface_fmt = 1;
	map_state->v_backward.tm1.width = (w >> 1) - 1;
	map_state->v_backward.tm1.height = (h >> 1) - 1;
	map_state->v_backward.tm2.depth = 0;
	map_state->v_backward.tm2.max_lod = 0;
	map_state->v_backward.tm2.cube_face = 0;
}

static void i915_mc_map_state_set(XvMCContext * context,
				  i915XvMCSurface * privPast,
				  i915XvMCSurface * privFuture)
{
	i915XvMCContext *pI915XvMC = (i915XvMCContext *) context->privData;
	struct i915_mc_map_state *map_state;

	map_state = (struct i915_mc_map_state *)pI915XvMC->msb.map;

	map_state->y_forward.tm0.base_address = (YOFFSET(privPast) >> 2);
	map_state->y_forward.tm2.pitch = (privPast->yStride >> 2) - 1;	/* in DWords - 1 */
	map_state->y_backward.tm0.base_address = (YOFFSET(privFuture) >> 2);
	map_state->y_backward.tm2.pitch = (privFuture->yStride >> 2) - 1;
	map_state->u_forward.tm0.base_address = (UOFFSET(privPast) >> 2);
	map_state->u_forward.tm2.pitch = (privPast->uvStride >> 2) - 1;	/* in DWords - 1 */
	map_state->u_backward.tm0.base_address = (UOFFSET(privFuture) >> 2);
	map_state->u_backward.tm2.pitch = (privFuture->uvStride >> 2) - 1;
	map_state->v_forward.tm0.base_address = (VOFFSET(privPast) >> 2);
	map_state->v_forward.tm2.pitch = (privPast->uvStride >> 2) - 1;	/* in DWords - 1 */
	map_state->v_backward.tm0.base_address = (VOFFSET(privFuture) >> 2);
	map_state->v_backward.tm2.pitch = (privFuture->uvStride >> 2) - 1;
}

static void i915_flush(int map, int render)
{
	struct i915_mi_flush mi_flush;

	memset(&mi_flush, 0, sizeof(mi_flush));
	mi_flush.dw0.type = CMD_MI;
	mi_flush.dw0.opcode = OPC_MI_FLUSH;
	mi_flush.dw0.map_cache_invalidate = map;
	mi_flush.dw0.render_cache_flush_inhibit = render;

	intelBatchbufferData(&mi_flush, sizeof(mi_flush), 0);
}

static void i915_mc_load_indirect_render_init(XvMCContext * context)
{
	i915XvMCContext *pI915XvMC = (i915XvMCContext *) context->privData;
	sis_state *sis;
	msb_state *msb;
	struct i915_3dstate_load_indirect *load_indirect;
	int mem_select;

	mc_render_load_indirect_size = sizeof(*load_indirect) + sizeof(*sis)
	    + sizeof(*msb);
	mc_render_load_indirect = calloc(1, mc_render_load_indirect_size);

	load_indirect =
	    (struct i915_3dstate_load_indirect *)mc_render_load_indirect;
	load_indirect->dw0.type = CMD_3D;
	load_indirect->dw0.opcode = OPC_3DSTATE_LOAD_INDIRECT;
	load_indirect->dw0.block_mask = BLOCK_SIS | BLOCK_MSB;
	load_indirect->dw0.length = (mc_render_load_indirect_size >> 2) - 2;

	if (pI915XvMC->deviceID == PCI_CHIP_I915_G ||
	    pI915XvMC->deviceID == PCI_CHIP_I915_GM)
		mem_select = 0;
	else
		mem_select = 1;

	load_indirect->dw0.mem_select = mem_select;

	/* Static Indirect state buffer (dest buffer info) */
	sis = (sis_state *) (++load_indirect);
	sis->dw0.valid = 1;
	sis->dw0.force = 1;
	sis->dw1.length = 16;	/* 4 * 3 + 2 + 3 - 1 */

	if (mem_select)
		sis->dw0.buffer_address = (pI915XvMC->sis.offset >> 2);
	else
		sis->dw0.buffer_address = (pI915XvMC->sis.bus_addr >> 2);

	/* Map state buffer (reference buffer info) */
	msb = (msb_state *) (++sis);
	msb->dw0.valid = 1;
	msb->dw0.force = 1;
	msb->dw1.length = 23;	/* 3 * 8 - 1 */

	if (mem_select)
		msb->dw0.buffer_address = (pI915XvMC->msb.offset >> 2);
	else
		msb->dw0.buffer_address = (pI915XvMC->msb.bus_addr >> 2);
}

static void i915_mc_load_indirect_render_emit(void)
{
	i915_emit_batch(mc_render_load_indirect, mc_render_load_indirect_size,
			0);
}

static void i915_mc_mpeg_set_origin(XvMCContext * context, XvMCMacroBlock * mb)
{
	struct i915_3dmpeg_set_origin set_origin;

	/* 3DMPEG_SET_ORIGIN */
	memset(&set_origin, 0, sizeof(set_origin));
	set_origin.dw0.type = CMD_3D;
	set_origin.dw0.opcode = OPC_3DMPEG_SET_ORIGIN;
	set_origin.dw0.length = 0;
	set_origin.dw1.h_origin = mb->x;
	set_origin.dw1.v_origin = mb->y;

	intelBatchbufferData(&set_origin, sizeof(set_origin), 0);
}

static void i915_mc_mpeg_macroblock_ipicture(XvMCContext * context,
					     XvMCMacroBlock * mb)
{
	struct i915_3dmpeg_macroblock_ipicture macroblock_ipicture;

	/* 3DMPEG_MACROBLOCK_IPICTURE */
	memset(&macroblock_ipicture, 0, sizeof(macroblock_ipicture));
	macroblock_ipicture.dw0.type = CMD_3D;
	macroblock_ipicture.dw0.opcode = OPC_3DMPEG_MACROBLOCK_IPICTURE;
	macroblock_ipicture.dw0.dct_type =
	    (mb->dct_type == XVMC_DCT_TYPE_FIELD);

	intelBatchbufferData(&macroblock_ipicture, sizeof(macroblock_ipicture),
			     0);
}

static void i915_mc_mpeg_macroblock_1fbmv(XvMCContext * context,
					  XvMCMacroBlock * mb)
{
	struct i915_3dmpeg_macroblock_1fbmv macroblock_1fbmv;
	vector_t mv0[2];

	/* 3DMPEG_MACROBLOCK(1fbmv) */
	memset(&macroblock_1fbmv, 0, sizeof(macroblock_1fbmv));
	macroblock_1fbmv.header.dw0.type = CMD_3D;
	macroblock_1fbmv.header.dw0.opcode = OPC_3DMPEG_MACROBLOCK;
	macroblock_1fbmv.header.dw0.length = 2;
	macroblock_1fbmv.header.dw1.mb_intra = 0;	/* should be 0 */
	macroblock_1fbmv.header.dw1.forward =
	    ((mb->macroblock_type & XVMC_MB_TYPE_MOTION_FORWARD) ? 1 : 0);
	macroblock_1fbmv.header.dw1.backward =
	    ((mb->macroblock_type & XVMC_MB_TYPE_MOTION_BACKWARD) ? 1 : 0);
	macroblock_1fbmv.header.dw1.h263_4mv = 0;	/* should be 0 */
	macroblock_1fbmv.header.dw1.dct_type =
	    (mb->dct_type == XVMC_DCT_TYPE_FIELD);

	if (!(mb->coded_block_pattern & 0x3f))
		macroblock_1fbmv.header.dw1.dct_type = XVMC_DCT_TYPE_FRAME;

	macroblock_1fbmv.header.dw1.motion_type = (mb->motion_type & 0x03);
	macroblock_1fbmv.header.dw1.vertical_field_select =
	    (mb->motion_vertical_field_select & 0x0f);
	macroblock_1fbmv.header.dw1.coded_block_pattern =
	    mb->coded_block_pattern;
	macroblock_1fbmv.header.dw1.skipped_macroblocks = 0;

	mv0[0].component[0] = mb->PMV[0][0][0];
	mv0[0].component[1] = mb->PMV[0][0][1];
	mv0[1].component[0] = mb->PMV[0][1][0];
	mv0[1].component[1] = mb->PMV[0][1][1];

	macroblock_1fbmv.dw2 = mv0[0].v;
	macroblock_1fbmv.dw3 = mv0[1].v;

	intelBatchbufferData(&macroblock_1fbmv, sizeof(macroblock_1fbmv), 0);
}

static void i915_mc_mpeg_macroblock_2fbmv(XvMCContext * context,
					  XvMCMacroBlock * mb, unsigned int ps)
{
	struct i915_3dmpeg_macroblock_2fbmv macroblock_2fbmv;
	vector_t mv0[2];
	vector_t mv1[2];

	/* 3DMPEG_MACROBLOCK(2fbmv) */
	memset(&macroblock_2fbmv, 0, sizeof(macroblock_2fbmv));
	macroblock_2fbmv.header.dw0.type = CMD_3D;
	macroblock_2fbmv.header.dw0.opcode = OPC_3DMPEG_MACROBLOCK;
	macroblock_2fbmv.header.dw0.length = 4;
	macroblock_2fbmv.header.dw1.mb_intra = 0;	/* should be 0 */
	macroblock_2fbmv.header.dw1.forward =
	    ((mb->macroblock_type & XVMC_MB_TYPE_MOTION_FORWARD) ? 1 : 0);
	macroblock_2fbmv.header.dw1.backward =
	    ((mb->macroblock_type & XVMC_MB_TYPE_MOTION_BACKWARD) ? 1 : 0);
	macroblock_2fbmv.header.dw1.h263_4mv = 0;	/* should be 0 */
	macroblock_2fbmv.header.dw1.dct_type =
	    (mb->dct_type == XVMC_DCT_TYPE_FIELD);

	if (!(mb->coded_block_pattern & 0x3f))
		macroblock_2fbmv.header.dw1.dct_type = XVMC_DCT_TYPE_FRAME;

	macroblock_2fbmv.header.dw1.motion_type = (mb->motion_type & 0x03);
	macroblock_2fbmv.header.dw1.vertical_field_select =
	    (mb->motion_vertical_field_select & 0x0f);
	macroblock_2fbmv.header.dw1.coded_block_pattern =
	    mb->coded_block_pattern;
	macroblock_2fbmv.header.dw1.skipped_macroblocks = 0;

	mv0[0].component[0] = mb->PMV[0][0][0];
	mv0[0].component[1] = mb->PMV[0][0][1];
	mv0[1].component[0] = mb->PMV[0][1][0];
	mv0[1].component[1] = mb->PMV[0][1][1];
	mv1[0].component[0] = mb->PMV[1][0][0];
	mv1[0].component[1] = mb->PMV[1][0][1];
	mv1[1].component[0] = mb->PMV[1][1][0];
	mv1[1].component[1] = mb->PMV[1][1][1];

	if ((ps & XVMC_FRAME_PICTURE) == XVMC_FRAME_PICTURE) {
		if ((mb->motion_type & 3) == XVMC_PREDICTION_FIELD) {
			mv0[0].component[1] = mb->PMV[0][0][1] >> 1;
			mv0[1].component[1] = mb->PMV[0][1][1] >> 1;
			mv1[0].component[1] = mb->PMV[1][0][1] >> 1;
			mv1[1].component[1] = mb->PMV[1][1][1] >> 1;
		} else if ((mb->motion_type & 3) == XVMC_PREDICTION_DUAL_PRIME) {
			mv0[0].component[1] = mb->PMV[0][0][1] >> 1;
			mv0[1].component[1] = mb->PMV[0][1][1] >> 1;	// MPEG2 MV[0][1] isn't used
			mv1[0].component[1] = mb->PMV[1][0][1] >> 1;
			mv1[1].component[1] = mb->PMV[1][1][1] >> 1;
		}
	}

	macroblock_2fbmv.dw2 = mv0[0].v;
	macroblock_2fbmv.dw3 = mv0[1].v;
	macroblock_2fbmv.dw4 = mv1[0].v;
	macroblock_2fbmv.dw5 = mv1[1].v;

	intelBatchbufferData(&macroblock_2fbmv, sizeof(macroblock_2fbmv), 0);
}

static int i915_xvmc_map_buffers(i915XvMCContext * pI915XvMC)
{
	if (drmMap(xvmc_driver->fd,
		   pI915XvMC->sis.handle,
		   pI915XvMC->sis.size,
		   (drmAddress *) & pI915XvMC->sis.map) != 0) {
		return -1;
	}

	if (drmMap(xvmc_driver->fd,
		   pI915XvMC->ssb.handle,
		   pI915XvMC->ssb.size,
		   (drmAddress *) & pI915XvMC->ssb.map) != 0) {
		return -1;
	}

	if (drmMap(xvmc_driver->fd,
		   pI915XvMC->msb.handle,
		   pI915XvMC->msb.size,
		   (drmAddress *) & pI915XvMC->msb.map) != 0) {
		return -1;
	}

	if (drmMap(xvmc_driver->fd,
		   pI915XvMC->psp.handle,
		   pI915XvMC->psp.size,
		   (drmAddress *) & pI915XvMC->psp.map) != 0) {
		return -1;
	}

	if (drmMap(xvmc_driver->fd,
		   pI915XvMC->psc.handle,
		   pI915XvMC->psc.size,
		   (drmAddress *) & pI915XvMC->psc.map) != 0) {
		return -1;
	}

	if (drmMap(xvmc_driver->fd,
		   pI915XvMC->corrdata.handle,
		   pI915XvMC->corrdata.size,
		   (drmAddress *) & pI915XvMC->corrdata.map) != 0) {
		return -1;
	}

	return 0;
}

static void i915_xvmc_unmap_buffers(i915XvMCContext * pI915XvMC)
{
	if (pI915XvMC->sis.map) {
		drmUnmap(pI915XvMC->sis.map, pI915XvMC->sis.size);
		pI915XvMC->sis.map = NULL;
	}

	if (pI915XvMC->ssb.map) {
		drmUnmap(pI915XvMC->ssb.map, pI915XvMC->ssb.size);
		pI915XvMC->ssb.map = NULL;
	}

	if (pI915XvMC->msb.map) {
		drmUnmap(pI915XvMC->msb.map, pI915XvMC->msb.size);
		pI915XvMC->msb.map = NULL;
	}

	if (pI915XvMC->psp.map) {
		drmUnmap(pI915XvMC->psp.map, pI915XvMC->psp.size);
		pI915XvMC->psp.map = NULL;
	}

	if (pI915XvMC->psc.map) {
		drmUnmap(pI915XvMC->psc.map, pI915XvMC->psc.size);
		pI915XvMC->psc.map = NULL;
	}

	if (pI915XvMC->corrdata.map) {
		drmUnmap(pI915XvMC->corrdata.map, pI915XvMC->corrdata.size);
		pI915XvMC->corrdata.map = NULL;
	}
}

/*
 * Function: i915_release_resource
 */
static void i915_release_resource(Display * display, XvMCContext * context)
{
	i915XvMCContext *pI915XvMC;

	if (!(pI915XvMC = context->privData))
		return;

	pI915XvMC->ref--;
	i915_xvmc_unmap_buffers(pI915XvMC);

	free(pI915XvMC);
	context->privData = NULL;
}

static Status i915_xvmc_mc_create_context(Display * display,
					  XvMCContext * context, int priv_count,
					  CARD32 * priv_data)
{
	i915XvMCContext *pI915XvMC = NULL;
	I915XvMCCreateContextRec *tmpComm = NULL;

	XVMC_DBG("%s\n", __FUNCTION__);

	if (priv_count != (sizeof(I915XvMCCreateContextRec) >> 2)) {
		XVMC_ERR
		    ("_xvmc_create_context() returned incorrect data size!");
		XVMC_INFO("\tExpected %d, got %d",
			  (int)(sizeof(I915XvMCCreateContextRec) >> 2),
			  priv_count);
		_xvmc_destroy_context(display, context);
		XFree(priv_data);
		context->privData = NULL;
		return BadValue;
	}

	context->privData = (void *)calloc(1, sizeof(i915XvMCContext));
	if (!context->privData) {
		XVMC_ERR("Unable to allocate resources for XvMC context.");
		return BadAlloc;
	}
	pI915XvMC = (i915XvMCContext *) context->privData;

	tmpComm = (I915XvMCCreateContextRec *) priv_data;
	pI915XvMC->ctxno = tmpComm->ctxno;
	pI915XvMC->deviceID = tmpComm->deviceID;
	pI915XvMC->sis.handle = tmpComm->sis.handle;
	pI915XvMC->sis.offset = tmpComm->sis.offset;
	pI915XvMC->sis.size = tmpComm->sis.size;
	pI915XvMC->ssb.handle = tmpComm->ssb.handle;
	pI915XvMC->ssb.offset = tmpComm->ssb.offset;
	pI915XvMC->ssb.size = tmpComm->ssb.size;
	pI915XvMC->msb.handle = tmpComm->msb.handle;
	pI915XvMC->msb.offset = tmpComm->msb.offset;
	pI915XvMC->msb.size = tmpComm->msb.size;
	pI915XvMC->psp.handle = tmpComm->psp.handle;
	pI915XvMC->psp.offset = tmpComm->psp.offset;
	pI915XvMC->psp.size = tmpComm->psp.size;
	pI915XvMC->psc.handle = tmpComm->psc.handle;
	pI915XvMC->psc.offset = tmpComm->psc.offset;
	pI915XvMC->psc.size = tmpComm->psc.size;

	if (pI915XvMC->deviceID == PCI_CHIP_I915_G ||
	    pI915XvMC->deviceID == PCI_CHIP_I915_GM) {
		pI915XvMC->sis.bus_addr = tmpComm->sis.bus_addr;
		pI915XvMC->ssb.bus_addr = tmpComm->ssb.bus_addr;
		pI915XvMC->msb.bus_addr = tmpComm->msb.bus_addr;
		pI915XvMC->psp.bus_addr = tmpComm->psp.bus_addr;
		pI915XvMC->psc.bus_addr = tmpComm->psc.bus_addr;
	}

	pI915XvMC->corrdata.handle = tmpComm->corrdata.handle;
	pI915XvMC->corrdata.offset = tmpComm->corrdata.offset;
	pI915XvMC->corrdata.size = tmpComm->corrdata.size;

	/* Must free the private data we were passed from X */
	XFree(priv_data);
	priv_data = NULL;

	if (i915_xvmc_map_buffers(pI915XvMC)) {
		i915_xvmc_unmap_buffers(pI915XvMC);
		free(pI915XvMC);
		context->privData = NULL;
		return BadAlloc;
	}

	/* Initialize private context values */
	pI915XvMC->yStride = STRIDE(context->width);
	pI915XvMC->uvStride = STRIDE(context->width >> 1);
	pI915XvMC->haveXv = 0;
	pI915XvMC->dual_prime = 0;
	pI915XvMC->last_flip = 0;
	pI915XvMC->port = context->port;
	pI915XvMC->ref = 1;

	/* pre-init state buffers */
	i915_mc_one_time_context_init(context);
	i915_mc_one_time_state_init(context);

	i915_mc_static_indirect_state_init(context);

	i915_mc_map_state_init(context);

	i915_mc_load_indirect_render_init(context);
	return Success;
}

static int i915_xvmc_mc_destroy_context(Display * display,
					XvMCContext * context)
{
	i915XvMCContext *pI915XvMC;

	if (!(pI915XvMC = context->privData))
		return XvMCBadContext;

	/* Pass Control to the X server to destroy the drm_context_t */
	i915_release_resource(display, context);

	free(one_time_load_state_imm1);
	free(one_time_load_indirect);
	free(mc_render_load_indirect);
	return Success;
}

static Status i915_xvmc_mc_create_surface(Display * display,
					  XvMCContext * context,
					  XvMCSurface * surface, int priv_count,
					  CARD32 * priv_data)
{
	i915XvMCContext *pI915XvMC;
	i915XvMCSurface *pI915Surface;
	I915XvMCCreateSurfaceRec *tmpComm = NULL;

	if (!(pI915XvMC = context->privData))
		return XvMCBadContext;

	XVMC_DBG("%s\n", __FUNCTION__);

	if (priv_count != (sizeof(I915XvMCCreateSurfaceRec) >> 2)) {
		XVMC_ERR
		    ("_xvmc_create_surface() returned incorrect data size!");
		XVMC_INFO("\tExpected %d, got %d",
			  (int)(sizeof(I915XvMCCreateSurfaceRec) >> 2),
			  priv_count);
		_xvmc_destroy_surface(display, surface);
		XFree(priv_data);
		return BadAlloc;
	}

	PPTHREAD_MUTEX_LOCK();
	surface->privData = (i915XvMCSurface *) malloc(sizeof(i915XvMCSurface));

	if (!(pI915Surface = surface->privData)) {
		PPTHREAD_MUTEX_UNLOCK();
		return BadAlloc;
	}

	/* Initialize private values */
	pI915Surface->last_render = 0;
	pI915Surface->last_flip = 0;
	pI915Surface->yStride = pI915XvMC->yStride;
	pI915Surface->uvStride = pI915XvMC->uvStride;
	pI915Surface->width = context->width;
	pI915Surface->height = context->height;
	pI915Surface->privContext = pI915XvMC;
	pI915Surface->privSubPic = NULL;
	pI915Surface->srf.map = NULL;

	tmpComm = (I915XvMCCreateSurfaceRec *) priv_data;

	pI915Surface->srfNo = tmpComm->srfno;
	pI915Surface->srf.handle = tmpComm->srf.handle;
	pI915Surface->srf.offset = tmpComm->srf.offset;
	pI915Surface->srf.size = tmpComm->srf.size;

	XFree(priv_data);

	if (drmMap(xvmc_driver->fd,
		   pI915Surface->srf.handle,
		   pI915Surface->srf.size,
		   (drmAddress *) & pI915Surface->srf.map) != 0) {
		XVMC_ERR("mapping surface memory failed!\n");
		_xvmc_destroy_surface(display, surface);
		free(pI915Surface);
		surface->privData = NULL;
		PPTHREAD_MUTEX_UNLOCK();
		return BadAlloc;
	}

	pI915XvMC->ref++;
	PPTHREAD_MUTEX_UNLOCK();
	return 0;
}

static int i915_xvmc_mc_destroy_surface(Display * display,
					XvMCSurface * surface)
{
	i915XvMCSurface *pI915Surface;
	i915XvMCContext *pI915XvMC;

	if (!display || !surface)
		return BadValue;

	if (!(pI915Surface = surface->privData))
		return XvMCBadSurface;

	if (!(pI915XvMC = pI915Surface->privContext))
		return XvMCBadSurface;

	if (pI915Surface->last_flip)
		XvMCSyncSurface(display, surface);

	if (pI915Surface->srf.map)
		drmUnmap(pI915Surface->srf.map, pI915Surface->srf.size);

	free(pI915Surface);
	surface->privData = NULL;
	pI915XvMC->ref--;

	return Success;
}

static int i915_xvmc_mc_render_surface(Display * display, XvMCContext * context,
				       unsigned int picture_structure,
				       XvMCSurface * target_surface,
				       XvMCSurface * past_surface,
				       XvMCSurface * future_surface,
				       unsigned int flags,
				       unsigned int num_macroblocks,
				       unsigned int first_macroblock,
				       XvMCMacroBlockArray * macroblock_array,
				       XvMCBlockArray * blocks)
{
	int i;
	int picture_coding_type = MPEG_I_PICTURE;
	/* correction data buffer */
	char *corrdata_ptr;
	int corrdata_size = 0;

	/* Block Pointer */
	short *block_ptr;
	/* Current Macroblock Pointer */
	XvMCMacroBlock *mb;

	intel_xvmc_context_ptr intel_ctx;

	i915XvMCSurface *privTarget = NULL;
	i915XvMCSurface *privFuture = NULL;
	i915XvMCSurface *privPast = NULL;
	i915XvMCContext *pI915XvMC = NULL;

	XVMC_DBG("%s\n", __FUNCTION__);

	/* Check Parameters for validity */
	if (!display || !context || !target_surface) {
		XVMC_ERR("Invalid Display, Context or Target!");
		return BadValue;
	}

	if (!num_macroblocks)
		return Success;

	if (!macroblock_array || !blocks) {
		XVMC_ERR("Invalid block data!");
		return BadValue;
	}

	if (macroblock_array->num_blocks < (num_macroblocks + first_macroblock)) {
		XVMC_ERR("Too many macroblocks requested for MB array size.");
		return BadValue;
	}

	if (!(pI915XvMC = context->privData))
		return XvMCBadContext;

	if (!(privTarget = target_surface->privData))
		return XvMCBadSurface;

	if (context->surface_type_id >= SURFACE_TYPE_MAX) {
		XVMC_ERR("Unsupprted surface_type_id %d.",
			 context->surface_type_id);
		return BadValue;
	}

	intel_ctx = intel_xvmc_find_context(context->context_id);
	if (!intel_ctx) {
		XVMC_ERR("Can't find intel xvmc context\n");
		return BadValue;
	}

	/* P Frame Test */
	if (!past_surface) {
		/* Just to avoid some ifs later. */
		privPast = privTarget;
	} else {
		if (!(privPast = past_surface->privData)) {
			XVMC_ERR("Invalid Past Surface!");
			return XvMCBadSurface;
		}
		picture_coding_type = MPEG_P_PICTURE;
	}

	/* B Frame Test */
	if (!future_surface) {
		privFuture = privPast;	// privTarget;
	} else {
		if (!past_surface) {
			XVMC_ERR("No Past Surface!");
			return BadValue;
		}

		if (!(privFuture = future_surface->privData)) {
			XVMC_ERR("Invalid Future Surface!");
			return XvMCBadSurface;
		}

		picture_coding_type = MPEG_B_PICTURE;
	}

	LOCK_HARDWARE(intel_ctx->hw_context);
	corrdata_ptr = pI915XvMC->corrdata.map;
	corrdata_size = 0;

	for (i = first_macroblock; i < (num_macroblocks + first_macroblock);
	     i++) {
		int bspm = 0;
		mb = &macroblock_array->macro_blocks[i];
		block_ptr = &(blocks->blocks[mb->index << 6]);

		/* Lockup can happen if the coordinates are too far out of range */
		if (mb->x > (target_surface->width >> 4)) {
			mb->x = 0;
			XVMC_INFO("reset x");
		}

		if (mb->y > (target_surface->height >> 4)) {
			mb->y = 0;
			XVMC_INFO("reset y");
		}

		/* Catch no pattern case */
		if (!(mb->macroblock_type & XVMC_MB_TYPE_PATTERN) &&
		    !(mb->macroblock_type & XVMC_MB_TYPE_INTRA) &&
		    mb->coded_block_pattern) {
			mb->coded_block_pattern = 0;
			XVMC_INFO("no coded blocks present!");
		}

		bspm = mb_bytes_420[mb->coded_block_pattern];

		if (!bspm)
			continue;

		corrdata_size += bspm;

		if (corrdata_size > pI915XvMC->corrdata.size) {
			XVMC_ERR("correction data buffer overflow.");
			break;
		}
		memcpy(corrdata_ptr, block_ptr, bspm);
		corrdata_ptr += bspm;
	}

	i915_flush(1, 0);
	// i915_mc_invalidate_subcontext_buffers(context, BLOCK_SIS | BLOCK_DIS | BLOCK_SSB
	// | BLOCK_MSB | BLOCK_PSP | BLOCK_PSC);

	i915_mc_one_time_state_emit();

	i915_mc_static_indirect_state_set(context, target_surface,
					  picture_structure, flags,
					  picture_coding_type);
	/* setup reference surfaces */
	i915_mc_map_state_set(context, privPast, privFuture);

	i915_mc_load_indirect_render_emit();

	i915_mc_mpeg_set_origin(context,
				&macroblock_array->macro_blocks
				[first_macroblock]);

	for (i = first_macroblock; i < (num_macroblocks + first_macroblock);
	     i++) {
		mb = &macroblock_array->macro_blocks[i];

		/* Intra Blocks */
		if (mb->macroblock_type & XVMC_MB_TYPE_INTRA) {
			i915_mc_mpeg_macroblock_ipicture(context, mb);
		} else if ((picture_structure & XVMC_FRAME_PICTURE) ==
			   XVMC_FRAME_PICTURE) {
			/* Frame Picture */
			switch (mb->motion_type & 3) {
			case XVMC_PREDICTION_FIELD:	/* Field Based */
				i915_mc_mpeg_macroblock_2fbmv(context, mb,
							      picture_structure);
				break;

			case XVMC_PREDICTION_FRAME:	/* Frame Based */
				i915_mc_mpeg_macroblock_1fbmv(context, mb);
				break;

			case XVMC_PREDICTION_DUAL_PRIME:	/* Dual Prime */
				i915_mc_mpeg_macroblock_2fbmv(context, mb,
							      picture_structure);
				break;

			default:	/* No Motion Type */
				XVMC_ERR
				    ("Invalid Macroblock Parameters found.");
				break;
			}
		} else {	/* Field Picture */
			switch (mb->motion_type & 3) {
			case XVMC_PREDICTION_FIELD:	/* Field Based */
				i915_mc_mpeg_macroblock_1fbmv(context, mb);
				break;

			case XVMC_PREDICTION_16x8:	/* 16x8 MC */
				i915_mc_mpeg_macroblock_2fbmv(context, mb,
							      picture_structure);
				break;

			case XVMC_PREDICTION_DUAL_PRIME:	/* Dual Prime */
				i915_mc_mpeg_macroblock_1fbmv(context, mb);
				break;

			default:	/* No Motion Type */
				XVMC_ERR
				    ("Invalid Macroblock Parameters found.");
				break;
			}
		}
	}

	intelFlushBatch(TRUE);
	xvmc_driver->last_render = xvmc_driver->alloc.irq_emitted;
	privTarget->last_render = xvmc_driver->last_render;

	UNLOCK_HARDWARE(intel_ctx->hw_context);
	return 0;
}

static int i915_xvmc_mc_put_surface(Display * display, XvMCSurface * surface,
				    Drawable draw, short srcx, short srcy,
				    unsigned short srcw, unsigned short srch,
				    short destx, short desty,
				    unsigned short destw, unsigned short desth,
				    int flags, struct intel_xvmc_command *data)
{
	i915XvMCContext *pI915XvMC;
	i915XvMCSurface *pI915Surface;
	i915XvMCSubpicture *pI915SubPic;

	if (!(pI915Surface = surface->privData))
		return XvMCBadSurface;

	if (!(pI915XvMC = pI915Surface->privContext))
		return XvMCBadSurface;

	PPTHREAD_MUTEX_LOCK();

	data->command = INTEL_XVMC_COMMAND_DISPLAY;
	data->ctxNo = pI915XvMC->ctxno;
	data->srfNo = pI915Surface->srfNo;
	pI915SubPic = pI915Surface->privSubPic;
	data->subPicNo = (!pI915SubPic ? 0 : pI915SubPic->srfNo);
	data->real_id = FOURCC_YV12;
	data->flags = flags;

	PPTHREAD_MUTEX_UNLOCK();

	return 0;
}

static int i915_xvmc_mc_get_surface_status(Display * display,
					   XvMCSurface * surface, int *stat)
{
	i915XvMCSurface *pI915Surface;
	i915XvMCContext *pI915XvMC;

	if (!display || !surface || !stat)
		return BadValue;

	*stat = 0;

	if (!(pI915Surface = surface->privData))
		return XvMCBadSurface;

	if (!(pI915XvMC = pI915Surface->privContext))
		return XvMCBadSurface;

	PPTHREAD_MUTEX_LOCK();
	if (pI915Surface->last_flip) {
		/* This can not happen */
		if (pI915XvMC->last_flip < pI915Surface->last_flip) {
			XVMC_ERR
			    ("Context last flip is less than surface last flip.");
			PPTHREAD_MUTEX_UNLOCK();
			return BadValue;
		}

		/*
		   If the context has 2 or more flips after this surface it
		   cannot be displaying. Don't bother to check.
		 */
		if (!(pI915XvMC->last_flip > (pI915Surface->last_flip + 1))) {
			/*
			   If this surface was the last flipped it is either displaying
			   or about to be so don't bother checking.
			 */
			if (pI915XvMC->last_flip == pI915Surface->last_flip) {
				*stat |= XVMC_DISPLAYING;
			}
		}
	}

	PPTHREAD_MUTEX_UNLOCK();
	return 0;
}

struct _intel_xvmc_driver i915_xvmc_mc_driver = {
	.type = XVMC_I915_MPEG2_MC,
	.num_ctx = 0,
	.ctx_list = NULL,
	.create_context = i915_xvmc_mc_create_context,
	.destroy_context = i915_xvmc_mc_destroy_context,
	.create_surface = i915_xvmc_mc_create_surface,
	.destroy_surface = i915_xvmc_mc_destroy_surface,
	.render_surface = i915_xvmc_mc_render_surface,
	.put_surface = i915_xvmc_mc_put_surface,
	.get_surface_status = i915_xvmc_mc_get_surface_status,
};
