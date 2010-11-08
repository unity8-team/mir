/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "intel.h"

#include "i830_reg.h"

void I830EmitInvarientState(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	assert(intel->in_batch_atomic);

	OUT_BATCH(_3DSTATE_MAP_CUBE | MAP_UNIT(0));
	OUT_BATCH(_3DSTATE_MAP_CUBE | MAP_UNIT(1));
	OUT_BATCH(_3DSTATE_MAP_CUBE | MAP_UNIT(2));
	OUT_BATCH(_3DSTATE_MAP_CUBE | MAP_UNIT(3));

	OUT_BATCH(_3DSTATE_DFLT_DIFFUSE_CMD);
	OUT_BATCH(0);

	OUT_BATCH(_3DSTATE_DFLT_SPEC_CMD);
	OUT_BATCH(0);

	OUT_BATCH(_3DSTATE_DFLT_Z_CMD);
	OUT_BATCH(0);

	OUT_BATCH(_3DSTATE_FOG_MODE_CMD);
	OUT_BATCH(FOGFUNC_ENABLE |
		  FOG_LINEAR_CONST | FOGSRC_INDEX_Z | ENABLE_FOG_DENSITY);
	OUT_BATCH(0);
	OUT_BATCH(0);

	OUT_BATCH(_3DSTATE_MAP_TEX_STREAM_CMD |
		  MAP_UNIT(0) |
		  DISABLE_TEX_STREAM_BUMP |
		  ENABLE_TEX_STREAM_COORD_SET |
		  TEX_STREAM_COORD_SET(0) |
		  ENABLE_TEX_STREAM_MAP_IDX | TEX_STREAM_MAP_IDX(0));
	OUT_BATCH(_3DSTATE_MAP_TEX_STREAM_CMD |
		  MAP_UNIT(1) |
		  DISABLE_TEX_STREAM_BUMP |
		  ENABLE_TEX_STREAM_COORD_SET |
		  TEX_STREAM_COORD_SET(1) |
		  ENABLE_TEX_STREAM_MAP_IDX | TEX_STREAM_MAP_IDX(1));
	OUT_BATCH(_3DSTATE_MAP_TEX_STREAM_CMD |
		  MAP_UNIT(2) |
		  DISABLE_TEX_STREAM_BUMP |
		  ENABLE_TEX_STREAM_COORD_SET |
		  TEX_STREAM_COORD_SET(2) |
		  ENABLE_TEX_STREAM_MAP_IDX | TEX_STREAM_MAP_IDX(2));
	OUT_BATCH(_3DSTATE_MAP_TEX_STREAM_CMD |
		  MAP_UNIT(3) |
		  DISABLE_TEX_STREAM_BUMP |
		  ENABLE_TEX_STREAM_COORD_SET |
		  TEX_STREAM_COORD_SET(3) |
		  ENABLE_TEX_STREAM_MAP_IDX | TEX_STREAM_MAP_IDX(3));

	OUT_BATCH(_3DSTATE_MAP_COORD_TRANSFORM);
	OUT_BATCH(DISABLE_TEX_TRANSFORM | TEXTURE_SET(0));
	OUT_BATCH(_3DSTATE_MAP_COORD_TRANSFORM);
	OUT_BATCH(DISABLE_TEX_TRANSFORM | TEXTURE_SET(1));
	OUT_BATCH(_3DSTATE_MAP_COORD_TRANSFORM);
	OUT_BATCH(DISABLE_TEX_TRANSFORM | TEXTURE_SET(2));
	OUT_BATCH(_3DSTATE_MAP_COORD_TRANSFORM);
	OUT_BATCH(DISABLE_TEX_TRANSFORM | TEXTURE_SET(3));

	OUT_BATCH(_3DSTATE_RASTER_RULES_CMD |
		  ENABLE_POINT_RASTER_RULE |
		  OGL_POINT_RASTER_RULE |
		  ENABLE_LINE_STRIP_PROVOKE_VRTX |
		  ENABLE_TRI_FAN_PROVOKE_VRTX |
		  ENABLE_TRI_STRIP_PROVOKE_VRTX |
		  LINE_STRIP_PROVOKE_VRTX(1) |
		  TRI_FAN_PROVOKE_VRTX(2) | TRI_STRIP_PROVOKE_VRTX(2));

	OUT_BATCH(_3DSTATE_SCISSOR_ENABLE_CMD | DISABLE_SCISSOR_RECT);

	OUT_BATCH(_3DSTATE_SCISSOR_RECT_0_CMD);
	OUT_BATCH(0);
	OUT_BATCH(0);

	OUT_BATCH(_3DSTATE_VERTEX_TRANSFORM);
	OUT_BATCH(DISABLE_VIEWPORT_TRANSFORM | DISABLE_PERSPECTIVE_DIVIDE);

	OUT_BATCH(_3DSTATE_W_STATE_CMD);
	OUT_BATCH(MAGIC_W_STATE_DWORD1);
	OUT_BATCH(0x3f800000 /* 1.0 in IEEE float */ );

	OUT_BATCH(_3DSTATE_COLOR_FACTOR_CMD);
	OUT_BATCH(0x80808080);	/* .5 required in alpha for GL_DOT3_RGBA_EXT */

	OUT_BATCH(_3DSTATE_MAP_COORD_SETBIND_CMD);
	OUT_BATCH(TEXBIND_SET3(TEXCOORDSRC_VTXSET_3) |
		  TEXBIND_SET2(TEXCOORDSRC_VTXSET_2) |
		  TEXBIND_SET1(TEXCOORDSRC_VTXSET_1) |
		  TEXBIND_SET0(TEXCOORDSRC_VTXSET_0));

	/* copy from mesa */
	OUT_BATCH(_3DSTATE_INDPT_ALPHA_BLEND_CMD |
		  DISABLE_INDPT_ALPHA_BLEND |
		  ENABLE_ALPHA_BLENDFUNC | ABLENDFUNC_ADD);

	OUT_BATCH(_3DSTATE_FOG_COLOR_CMD |
		  FOG_COLOR_RED(0) | FOG_COLOR_GREEN(0) | FOG_COLOR_BLUE(0));

	OUT_BATCH(_3DSTATE_CONST_BLEND_COLOR_CMD);
	OUT_BATCH(0);

	OUT_BATCH(_3DSTATE_MODES_1_CMD |
		  ENABLE_COLR_BLND_FUNC |
		  BLENDFUNC_ADD |
		  ENABLE_SRC_BLND_FACTOR |
		  SRC_BLND_FACT(BLENDFACTOR_ONE) |
		  ENABLE_DST_BLND_FACTOR | DST_BLND_FACT(BLENDFACTOR_ZERO));
	OUT_BATCH(_3DSTATE_MODES_2_CMD | ENABLE_GLOBAL_DEPTH_BIAS | GLOBAL_DEPTH_BIAS(0) | ENABLE_ALPHA_TEST_FUNC | ALPHA_TEST_FUNC(0) |	/* always */
		  ALPHA_REF_VALUE(0));
	OUT_BATCH(_3DSTATE_MODES_3_CMD |
		  ENABLE_DEPTH_TEST_FUNC |
		  DEPTH_TEST_FUNC(0x2) |	/* COMPAREFUNC_LESS */
		  ENABLE_ALPHA_SHADE_MODE |
		  ALPHA_SHADE_MODE(SHADE_MODE_LINEAR) |
		  ENABLE_FOG_SHADE_MODE |
		  FOG_SHADE_MODE(SHADE_MODE_LINEAR) |
		  ENABLE_SPEC_SHADE_MODE |
		  SPEC_SHADE_MODE(SHADE_MODE_LINEAR) |
		  ENABLE_COLOR_SHADE_MODE |
		  COLOR_SHADE_MODE(SHADE_MODE_LINEAR) |
		  ENABLE_CULL_MODE | CULLMODE_NONE);

	OUT_BATCH(_3DSTATE_MODES_4_CMD |
		  ENABLE_LOGIC_OP_FUNC |
		  LOGIC_OP_FUNC(LOGICOP_COPY) |
		  ENABLE_STENCIL_TEST_MASK |
		  STENCIL_TEST_MASK(0xff) |
		  ENABLE_STENCIL_WRITE_MASK | STENCIL_WRITE_MASK(0xff));

	OUT_BATCH(_3DSTATE_STENCIL_TEST_CMD |
		  ENABLE_STENCIL_PARMS |
		  STENCIL_FAIL_OP(0) |	/* STENCILOP_KEEP */
		  STENCIL_PASS_DEPTH_FAIL_OP(0) |	/* STENCILOP_KEEP */
		  STENCIL_PASS_DEPTH_PASS_OP(0) |	/* STENCILOP_KEEP */
		  ENABLE_STENCIL_TEST_FUNC |
		  STENCIL_TEST_FUNC(0) |	/* COMPAREFUNC_ALWAYS */
		  ENABLE_STENCIL_REF_VALUE |
		  STENCIL_REF_VALUE(0));

	OUT_BATCH(_3DSTATE_MODES_5_CMD |
		  FLUSH_TEXTURE_CACHE |
		  ENABLE_SPRITE_POINT_TEX | SPRITE_POINT_TEX_OFF |
		  ENABLE_FIXED_LINE_WIDTH | FIXED_LINE_WIDTH(0x2) | /* 1.0 */
		  ENABLE_FIXED_POINT_WIDTH | FIXED_POINT_WIDTH(1));

	OUT_BATCH(_3DSTATE_ENABLES_1_CMD |
		  DISABLE_LOGIC_OP |
		  DISABLE_STENCIL_TEST |
		  DISABLE_DEPTH_BIAS |
		  DISABLE_SPEC_ADD |
		  DISABLE_FOG |
		  DISABLE_ALPHA_TEST | ENABLE_COLOR_BLEND | DISABLE_DEPTH_TEST);
	OUT_BATCH(_3DSTATE_ENABLES_2_CMD |
		  DISABLE_STENCIL_WRITE |
		  ENABLE_TEX_CACHE |
		  DISABLE_DITHER |
		  ENABLE_COLOR_MASK | ENABLE_COLOR_WRITE | DISABLE_DEPTH_WRITE);

	OUT_BATCH(_3DSTATE_STIPPLE);

	/* Set default blend state */
	OUT_BATCH(_3DSTATE_MAP_BLEND_OP_CMD(0) |
		  TEXPIPE_COLOR |
		  ENABLE_TEXOUTPUT_WRT_SEL |
		  TEXOP_OUTPUT_CURRENT |
		  DISABLE_TEX_CNTRL_STAGE |
		  TEXOP_SCALE_1X |
		  TEXOP_MODIFY_PARMS | TEXOP_LAST_STAGE | TEXBLENDOP_ARG1);
	OUT_BATCH(_3DSTATE_MAP_BLEND_OP_CMD(0) |
		  TEXPIPE_ALPHA |
		  ENABLE_TEXOUTPUT_WRT_SEL |
		  TEXOP_OUTPUT_CURRENT |
		  TEXOP_SCALE_1X | TEXOP_MODIFY_PARMS | TEXBLENDOP_ARG1);
	OUT_BATCH(_3DSTATE_MAP_BLEND_ARG_CMD(0) |
		  TEXPIPE_COLOR |
		  TEXBLEND_ARG1 |
		  TEXBLENDARG_MODIFY_PARMS | TEXBLENDARG_DIFFUSE);
	OUT_BATCH(_3DSTATE_MAP_BLEND_ARG_CMD(0) |
		  TEXPIPE_ALPHA |
		  TEXBLEND_ARG1 |
		  TEXBLENDARG_MODIFY_PARMS | TEXBLENDARG_DIFFUSE);

	OUT_BATCH(_3DSTATE_AA_CMD |
		  AA_LINE_ECAAR_WIDTH_ENABLE |
		  AA_LINE_ECAAR_WIDTH_1_0 |
		  AA_LINE_REGION_WIDTH_ENABLE |
		  AA_LINE_REGION_WIDTH_1_0 | AA_LINE_DISABLE);
}
