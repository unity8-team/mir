/*
 * Copyright © 2010 Intel Corporation
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
 *	Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xaarop.h"
#include "i830.h"
#include "i915_reg.h"
#include "i915_3d.h"

#define FLOATS_PER_VERTEX 6

Bool
i915_check_trapezoids(int width, int height, int depth)
{
	return width <= 2048 && height <= 2048 && depth == 8;
}

static inline float
line_x_for_y(xLineFixed *l, xFixed y)
{
	if (y == l->p1.y)
		return l->p1.x;
	if (y == l->p2.y)
		return l->p2.x;
	if (l->p2.x == l->p1.x)
		return l->p1.x;

	return l->p1.x + (y - l->p1.y) * (float) (l->p2.x - l->p1.x) / (l->p2.y - l->p1.y);
}

#define OUT_TRAP_VERTEX(x, y) do { \
	xFixed fy = IntToxFixed(y); \
	float sf = 1. / xFixed1; \
	OUT_VERTEX(x + dst_x); \
	OUT_VERTEX(y + dst_y); \
	OUT_VERTEX(y - trap->top*sf); \
	OUT_VERTEX(trap->bottom*sf - y); \
	OUT_VERTEX(x - sf*line_x_for_y(&trap->left, fy)); \
	OUT_VERTEX(sf*line_x_for_y(&trap->right, fy) - x); \
} while (0)

static void
i915_trapezoids_set_target(intel_screen_private *intel, PixmapPtr pixmap)
{
	if (intel->last_3d == LAST_3D_OTHER)
		I915EmitInvarientState(intel);
	intel->last_3d = LAST_3D_RENDER;

	if (intel->render_current_dest != pixmap) {
	    uint32_t tiling_bits;

	    tiling_bits = 0;
	    switch(i830_get_pixmap_intel(pixmap)->tiling) {
	    case I915_TILING_NONE: break;
	    case I915_TILING_Y: tiling_bits |= BUF_3D_TILE_WALK_Y;
	    case I915_TILING_X: tiling_bits |= BUF_3D_TILED_SURFACE; break;
	    }

	    OUT_BATCH(_3DSTATE_BUF_INFO_CMD);
	    OUT_BATCH(BUF_3D_ID_COLOR_BACK | tiling_bits |
		      BUF_3D_PITCH(intel_get_pixmap_pitch(pixmap)));
	    OUT_RELOC_PIXMAP(pixmap, I915_GEM_DOMAIN_RENDER,
			     I915_GEM_DOMAIN_RENDER, 0);

	    OUT_BATCH(_3DSTATE_DST_BUF_VARS_CMD);
	    OUT_BATCH(COLR_BUF_8BIT);

	    /* draw rect is unconditional */
	    OUT_BATCH(_3DSTATE_DRAW_RECT_CMD);
	    OUT_BATCH(0x00000000);
	    OUT_BATCH(0x00000000);	/* ymin, xmin */
	    OUT_BATCH(DRAW_YMAX(pixmap->drawable.height - 1) |
		      DRAW_XMAX(pixmap->drawable.width - 1));
	    /* yorig, xorig (relate to color buffer?) */
	    OUT_BATCH(0x00000000);

	    intel->render_current_dest = pixmap;
	}
}

static void
i915_trapezoids_set_shader(intel_screen_private *intel)
{
	FS_LOCALS();

	OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(2) | I1_LOAD_S(6) | 1);
	OUT_BATCH(~S2_TEXCOORD_FMT(0, TEXCOORDFMT_NOT_PRESENT) | S2_TEXCOORD_FMT(0, TEXCOORDFMT_4D));
	OUT_BATCH(S6_CBUF_BLEND_ENABLE | S6_COLOR_WRITE_ENABLE |
		  (BLENDFUNC_ADD << S6_CBUF_BLEND_FUNC_SHIFT) |
		  (BLENDFACT_ONE << S6_CBUF_SRC_BLEND_FACT_SHIFT) |
		  (BLENDFACT_ONE << S6_CBUF_DST_BLEND_FACT_SHIFT));

	FS_BEGIN();
	i915_fs_dcl(FS_T0);
	i915_fs_min(FS_U0,
		    i915_fs_operand(FS_R0, ZERO, ONE, ZERO, ONE),
		    i915_fs_operand_reg(FS_T0));
	i915_fs_add(FS_U0,
		    i915_fs_operand(FS_U0, X, Z, ZERO, ZERO),
		    i915_fs_operand(FS_U0, Y, W, ZERO, ZERO));
	i915_fs_mul(FS_OC,
		    i915_fs_operand(FS_U0, X, X, X, X),
		    i915_fs_operand(FS_U0, Y, Y, Y, Y));
	FS_END();
}

static void
i915_trapezoids_set_vertices(intel_screen_private *intel)
{
	intel->floats_per_vertex = FLOATS_PER_VERTEX;
	if (intel_vertex_space(intel) < 3*4*FLOATS_PER_VERTEX) {
		intel_next_vertex(intel);

		OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
			  I1_LOAD_S(0) | I1_LOAD_S(1) | 1);
		OUT_RELOC(intel->vertex_bo, I915_GEM_DOMAIN_VERTEX, 0, 0);
		OUT_BATCH((FLOATS_PER_VERTEX << S1_VERTEX_WIDTH_SHIFT) |
			  (FLOATS_PER_VERTEX << S1_VERTEX_PITCH_SHIFT));
	} else if (FLOATS_PER_VERTEX != intel->last_floats_per_vertex){
		OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
			  I1_LOAD_S(1) | 0);
		OUT_BATCH((FLOATS_PER_VERTEX << S1_VERTEX_WIDTH_SHIFT) |
			  (FLOATS_PER_VERTEX << S1_VERTEX_PITCH_SHIFT));

		intel->vertex_index =
			(intel->vertex_used + FLOATS_PER_VERTEX - 1) / FLOATS_PER_VERTEX;
		intel->vertex_used = intel->vertex_index * FLOATS_PER_VERTEX;
	}
	intel->last_floats_per_vertex = FLOATS_PER_VERTEX;
}

Bool
i915_rasterize_trapezoids(PixmapPtr pixmap, Bool clear,
			  int ntrap, xTrapezoid *trap,
			  int dst_x, int dst_y)
{
	ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (pixmap->drawable.width > 2048 || pixmap->drawable.height > 2048)
		return FALSE;

	if(!intel_check_pitch_3d(pixmap))
		return FALSE;

	intel_batch_require_space(scrn, intel, 150);
	i915_trapezoids_set_target(intel, pixmap);

	if (clear) {
#if 1
		FS_LOCALS();

		OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(2) | I1_LOAD_S(6) | 1);
		OUT_BATCH(~0);
		OUT_BATCH(S6_COLOR_WRITE_ENABLE);

		FS_BEGIN();
		i915_fs_mov(FS_OC, i915_fs_operand_zero());
		FS_END();

		OUT_BATCH(PRIM3D_RECTLIST | 5);
		OUT_BATCH_F(pixmap->drawable.width);
		OUT_BATCH_F(pixmap->drawable.height);
		OUT_BATCH_F(0);
		OUT_BATCH_F(pixmap->drawable.height);
		OUT_BATCH_F(0);
		OUT_BATCH_F(0);
#else
		OUT_BATCH(XY_COLOR_BLT_CMD);
		OUT_BATCH(ROP_0 | intel_get_pixmap_pitch(pixmap));
		OUT_BATCH(0);
		OUT_BATCH((pixmap->drawable.height << 16) | pixmap->drawable.width);
		OUT_RELOC_PIXMAP_FENCED(pixmap, I915_GEM_DOMAIN_RENDER,
					I915_GEM_DOMAIN_RENDER, 0);
		OUT_BATCH(0);
#endif
	}

	i915_trapezoids_set_shader(intel);
	i915_trapezoids_set_vertices(intel);

	for (; ntrap--; trap++) {
		int x1, x2, y1, y2;

		if (!xTrapezoidValid(trap))
			continue;

		x1 = xFixedToInt(min(trap->left.p1.x, trap->left.p2.x));
		x2 = xFixedToInt(xFixedCeil(max(trap->right.p1.x, trap->right.p2.x)));
		y1 = xFixedToInt(trap->top);
		y2 = xFixedToInt(xFixedCeil(trap->bottom));

		if (x2 + dst_x <= 0 || x1 + dst_x >= pixmap->drawable.width ||
		    y2 + dst_y <= 0 || y1 + dst_y >= pixmap->drawable.height)
			continue;

		if (intel_vertex_space(intel) < 3*4*FLOATS_PER_VERTEX) {
			i915_vertex_flush(intel);

			if (intel_batch_require_space(scrn, intel, 16)) {
				i915_trapezoids_set_target(intel, pixmap);
				i915_trapezoids_set_shader(intel);

				intel_next_vertex(intel);
				OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
					  I1_LOAD_S(0) | I1_LOAD_S(1) | 1);
				OUT_RELOC(intel->vertex_bo, I915_GEM_DOMAIN_VERTEX, 0, 0);
				OUT_BATCH((FLOATS_PER_VERTEX << S1_VERTEX_WIDTH_SHIFT) |
					  (FLOATS_PER_VERTEX << S1_VERTEX_PITCH_SHIFT));

				intel->last_floats_per_vertex = FLOATS_PER_VERTEX;
				intel->floats_per_vertex = FLOATS_PER_VERTEX;
			} else {
				intel_next_vertex(intel);
				OUT_BATCH(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
					  I1_LOAD_S(0) | 0);
				OUT_RELOC(intel->vertex_bo, I915_GEM_DOMAIN_VERTEX, 0, 0);
			}
		}

		OUT_TRAP_VERTEX(x2, y2);
		OUT_TRAP_VERTEX(x1, y2);
		OUT_TRAP_VERTEX(x1, y1);
		intel->vertex_count += 3;
	}

	i915_vertex_flush(intel);
	return TRUE;
}
