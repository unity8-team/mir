/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.
Copyright (c) 2005 Jesse Barnes <jbarnes@virtuousgeek.org>
  Based on code from i830_xaa.c.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xaarop.h"
#include "i830.h"
#include "i810_reg.h"
#include "i915_drm.h"
#include <string.h>
#include <sys/mman.h>

const int I830CopyROP[16] = {
	ROP_0,			/* GXclear */
	ROP_DSa,		/* GXand */
	ROP_SDna,		/* GXandReverse */
	ROP_S,			/* GXcopy */
	ROP_DSna,		/* GXandInverted */
	ROP_D,			/* GXnoop */
	ROP_DSx,		/* GXxor */
	ROP_DSo,		/* GXor */
	ROP_DSon,		/* GXnor */
	ROP_DSxn,		/* GXequiv */
	ROP_Dn,			/* GXinvert */
	ROP_SDno,		/* GXorReverse */
	ROP_Sn,			/* GXcopyInverted */
	ROP_DSno,		/* GXorInverted */
	ROP_DSan,		/* GXnand */
	ROP_1			/* GXset */
};

const int I830PatternROP[16] = {
	ROP_0,
	ROP_DPa,
	ROP_PDna,
	ROP_P,
	ROP_DPna,
	ROP_D,
	ROP_DPx,
	ROP_DPo,
	ROP_DPon,
	ROP_PDxn,
	ROP_Dn,
	ROP_PDno,
	ROP_Pn,
	ROP_DPno,
	ROP_DPan,
	ROP_1
};

static int uxa_pixmap_index;

/**
 * Returns whether a given pixmap is tiled or not.
 *
 * Currently, we only have one pixmap that might be tiled, which is the front
 * buffer.  At the point where we are tiling some pixmaps managed by the
 * general allocator, we should move this to using pixmap privates.
 */
Bool i830_pixmap_tiled(PixmapPtr pixmap)
{
	dri_bo *bo;
	uint32_t tiling_mode, swizzle_mode;
	int ret;

	bo = i830_get_pixmap_bo(pixmap);
	assert(bo != NULL);

	ret = drm_intel_bo_get_tiling(bo, &tiling_mode, &swizzle_mode);
	if (ret != 0) {
		FatalError("Couldn't get tiling on bo %p: %s\n",
			   bo, strerror(-ret));
	}

	return tiling_mode != I915_TILING_NONE;
}

Bool
i830_get_aperture_space(ScrnInfoPtr scrn, drm_intel_bo ** bo_table,
			int num_bos)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (intel->batch_bo == NULL) {
		intel_debug_fallback(scrn, "VT inactive\n");
		return FALSE;
	}

	bo_table[0] = intel->batch_bo;
	if (drm_intel_bufmgr_check_aperture_space(bo_table, num_bos) != 0) {
		intel_batch_flush(scrn, FALSE);
		bo_table[0] = intel->batch_bo;
		if (drm_intel_bufmgr_check_aperture_space(bo_table, num_bos) !=
		    0) {
			intel_debug_fallback(scrn, "Couldn't get aperture "
					    "space for BOs\n");
			return FALSE;
		}
	}
	return TRUE;
}

static unsigned long i830_pixmap_pitch(PixmapPtr pixmap)
{
	return pixmap->devKind;
}

static int i830_pixmap_pitch_is_aligned(PixmapPtr pixmap)
{
	ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	return i830_pixmap_pitch(pixmap) %
	    intel->accel_pixmap_pitch_alignment == 0;
}

/**
 * Sets up hardware state for a series of solid fills.
 */
static Bool
i830_uxa_prepare_solid(PixmapPtr pixmap, int alu, Pixel planemask, Pixel fg)
{
	ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	unsigned long pitch;
	drm_intel_bo *bo_table[] = {
		NULL,		/* batch_bo */
		i830_get_pixmap_bo(pixmap),
	};

	if (!UXA_PM_IS_SOLID(&pixmap->drawable, planemask)) {
		intel_debug_fallback(scrn, "planemask is not solid\n");
		return FALSE;
	}

	if (pixmap->drawable.bitsPerPixel == 24) {
		intel_debug_fallback(scrn, "solid 24bpp unsupported!\n");
		return FALSE;
	}

	if (pixmap->drawable.bitsPerPixel < 8) {
		intel_debug_fallback(scrn, "under 8bpp pixmaps unsupported\n");
		return FALSE;
	}

	if (!intel_check_pitch_2d(pixmap))
		return FALSE;

	pitch = i830_pixmap_pitch(pixmap);

	if (!i830_pixmap_pitch_is_aligned(pixmap)) {
		intel_debug_fallback(scrn, "pixmap pitch not aligned");
		return FALSE;
	}

	if (!i830_get_aperture_space(scrn, bo_table, ARRAY_SIZE(bo_table)))
		return FALSE;

	intel->BR[13] = (I830PatternROP[alu] & 0xff) << 16;
	switch (pixmap->drawable.bitsPerPixel) {
	case 8:
		break;
	case 16:
		/* RGB565 */
		intel->BR[13] |= (1 << 24);
		break;
	case 32:
		/* RGB8888 */
		intel->BR[13] |= ((1 << 24) | (1 << 25));
		break;
	}
	intel->BR[16] = fg;
	return TRUE;
}

static void i830_uxa_solid(PixmapPtr pixmap, int x1, int y1, int x2, int y2)
{
	ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	unsigned long pitch;
	uint32_t cmd;

	pitch = i830_pixmap_pitch(pixmap);

	{
		BEGIN_BATCH(6);

		cmd = XY_COLOR_BLT_CMD;

		if (pixmap->drawable.bitsPerPixel == 32)
			cmd |=
			    XY_COLOR_BLT_WRITE_ALPHA | XY_COLOR_BLT_WRITE_RGB;

		if (IS_I965G(intel) && i830_pixmap_tiled(pixmap)) {
			assert((pitch % 512) == 0);
			pitch >>= 2;
			cmd |= XY_COLOR_BLT_TILED;
		}

		OUT_BATCH(cmd);

		OUT_BATCH(intel->BR[13] | pitch);
		OUT_BATCH((y1 << 16) | (x1 & 0xffff));
		OUT_BATCH((y2 << 16) | (x2 & 0xffff));
		OUT_RELOC_PIXMAP(pixmap, I915_GEM_DOMAIN_RENDER,
				 I915_GEM_DOMAIN_RENDER, 0);
		OUT_BATCH(intel->BR[16]);
		ADVANCE_BATCH();
	}
}

static void i830_uxa_done_solid(PixmapPtr pixmap)
{
	ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];

	i830_debug_flush(scrn);
}

/**
 * TODO:
 *   - support planemask using FULL_BLT_CMD?
 */
static Bool
i830_uxa_prepare_copy(PixmapPtr source, PixmapPtr dest, int xdir,
		      int ydir, int alu, Pixel planemask)
{
	ScrnInfoPtr scrn = xf86Screens[dest->drawable.pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	drm_intel_bo *bo_table[] = {
		NULL,		/* batch_bo */
		i830_get_pixmap_bo(source),
		i830_get_pixmap_bo(dest),
	};

	if (!UXA_PM_IS_SOLID(&source->drawable, planemask)) {
		intel_debug_fallback(scrn, "planemask is not solid");
		return FALSE;
	}

	if (dest->drawable.bitsPerPixel < 8) {
		intel_debug_fallback(scrn, "under 8bpp pixmaps unsupported\n");
		return FALSE;
	}

	if (!i830_get_aperture_space(scrn, bo_table, ARRAY_SIZE(bo_table)))
		return FALSE;

	if (!intel_check_pitch_2d(source))
		return FALSE;
	if (!intel_check_pitch_2d(dest))
		return FALSE;

	intel->render_source = source;

	intel->BR[13] = I830CopyROP[alu] << 16;

	switch (source->drawable.bitsPerPixel) {
	case 8:
		break;
	case 16:
		intel->BR[13] |= (1 << 24);
		break;
	case 32:
		intel->BR[13] |= ((1 << 25) | (1 << 24));
		break;
	}
	return TRUE;
}

static void
i830_uxa_copy(PixmapPtr dest, int src_x1, int src_y1, int dst_x1,
	      int dst_y1, int w, int h)
{
	ScrnInfoPtr scrn = xf86Screens[dest->drawable.pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	uint32_t cmd;
	int dst_x2, dst_y2;
	unsigned int dst_pitch, src_pitch;

	dst_x2 = dst_x1 + w;
	dst_y2 = dst_y1 + h;

	dst_pitch = i830_pixmap_pitch(dest);
	src_pitch = i830_pixmap_pitch(intel->render_source);

	{
		BEGIN_BATCH(8);

		cmd = XY_SRC_COPY_BLT_CMD;

		if (dest->drawable.bitsPerPixel == 32)
			cmd |=
			    XY_SRC_COPY_BLT_WRITE_ALPHA |
			    XY_SRC_COPY_BLT_WRITE_RGB;

		if (IS_I965G(intel)) {
			if (i830_pixmap_tiled(dest)) {
				assert((dst_pitch % 512) == 0);
				dst_pitch >>= 2;
				cmd |= XY_SRC_COPY_BLT_DST_TILED;
			}

			if (i830_pixmap_tiled(intel->render_source)) {
				assert((src_pitch % 512) == 0);
				src_pitch >>= 2;
				cmd |= XY_SRC_COPY_BLT_SRC_TILED;
			}
		}

		OUT_BATCH(cmd);

		OUT_BATCH(intel->BR[13] | dst_pitch);
		OUT_BATCH((dst_y1 << 16) | (dst_x1 & 0xffff));
		OUT_BATCH((dst_y2 << 16) | (dst_x2 & 0xffff));
		OUT_RELOC_PIXMAP(dest, I915_GEM_DOMAIN_RENDER,
				 I915_GEM_DOMAIN_RENDER, 0);
		OUT_BATCH((src_y1 << 16) | (src_x1 & 0xffff));
		OUT_BATCH(src_pitch);
		OUT_RELOC_PIXMAP(intel->render_source, I915_GEM_DOMAIN_RENDER, 0,
				 0);

		ADVANCE_BATCH();
	}
}

static void i830_uxa_done_copy(PixmapPtr dest)
{
	ScrnInfoPtr scrn = xf86Screens[dest->drawable.pScreen->myNum];

	i830_debug_flush(scrn);
}

/**
 * Do any cleanup from the Composite operation.
 *
 * This is shared between i830 through i965.
 */
void i830_done_composite(PixmapPtr dest)
{
	ScrnInfoPtr scrn = xf86Screens[dest->drawable.pScreen->myNum];

	i830_debug_flush(scrn);
}

#define xFixedToFloat(val) \
	((float)xFixedToInt(val) + ((float)xFixedFrac(val) / 65536.0))

static Bool
_i830_transform_point(PictTransformPtr transform,
		      float x, float y, float result[3])
{
	int j;

	for (j = 0; j < 3; j++) {
		result[j] = (xFixedToFloat(transform->matrix[j][0]) * x +
			     xFixedToFloat(transform->matrix[j][1]) * y +
			     xFixedToFloat(transform->matrix[j][2]));
	}
	if (!result[2])
		return FALSE;
	return TRUE;
}

/**
 * Returns the floating-point coordinates transformed by the given transform.
 *
 * transform may be null.
 */
Bool
i830_get_transformed_coordinates(int x, int y, PictTransformPtr transform,
				 float *x_out, float *y_out)
{
	if (transform == NULL) {
		*x_out = x;
		*y_out = y;
	} else {
		float result[3];

		if (!_i830_transform_point
		    (transform, (float)x, (float)y, result))
			return FALSE;
		*x_out = result[0] / result[2];
		*y_out = result[1] / result[2];
	}
	return TRUE;
}

/**
 * Returns the un-normalized floating-point coordinates transformed by the given transform.
 *
 * transform may be null.
 */
Bool
i830_get_transformed_coordinates_3d(int x, int y, PictTransformPtr transform,
				    float *x_out, float *y_out, float *w_out)
{
	if (transform == NULL) {
		*x_out = x;
		*y_out = y;
		*w_out = 1;
	} else {
		float result[3];

		if (!_i830_transform_point
		    (transform, (float)x, (float)y, result))
			return FALSE;
		*x_out = result[0];
		*y_out = result[1];
		*w_out = result[2];
	}
	return TRUE;
}

/**
 * Returns whether the provided transform is affine.
 *
 * transform may be null.
 */
Bool i830_transform_is_affine(PictTransformPtr t)
{
	if (t == NULL)
		return TRUE;
	return t->matrix[2][0] == 0 && t->matrix[2][1] == 0;
}

dri_bo *i830_get_pixmap_bo(PixmapPtr pixmap)
{
	return dixLookupPrivate(&pixmap->devPrivates, &uxa_pixmap_index);
}

void i830_set_pixmap_bo(PixmapPtr pixmap, dri_bo * bo)
{
	dri_bo *old_bo = i830_get_pixmap_bo(pixmap);

	if (old_bo)
		dri_bo_unreference(old_bo);
	if (bo != NULL)
		dri_bo_reference(bo);
	dixSetPrivate(&pixmap->devPrivates, &uxa_pixmap_index, bo);
}

static void i830_uxa_set_pixmap_bo(PixmapPtr pixmap, dri_bo * bo)
{
	dixSetPrivate(&pixmap->devPrivates, &uxa_pixmap_index, bo);
}

static Bool i830_uxa_prepare_access(PixmapPtr pixmap, uxa_access_t access)
{
	dri_bo *bo = i830_get_pixmap_bo(pixmap);
	ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	intel_batch_flush(scrn, FALSE);

	/* No VT sema or GEM?  No GTT mapping. */
	if (!scrn->vtSema) {
		if (dri_bo_map(bo, access == UXA_ACCESS_RW) != 0)
			return FALSE;
		pixmap->devPrivate.ptr = bo->virtual;
		return TRUE;
	}

	/* Kernel manages fences at GTT map/fault time */
	if (bo->size < intel->max_gtt_map_size) {
		if (drm_intel_gem_bo_map_gtt(bo)) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "%s: bo map failed\n", __FUNCTION__);
			return FALSE;
		}
	} else {
		if (dri_bo_map(bo, access == UXA_ACCESS_RW) != 0) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "%s: bo map failed\n", __FUNCTION__);
			return FALSE;
		}
	}
	pixmap->devPrivate.ptr = bo->virtual;

	return TRUE;
}

static void i830_uxa_finish_access(PixmapPtr pixmap)
{
	dri_bo *bo = i830_get_pixmap_bo(pixmap);

	if (bo) {
		ScreenPtr screen = pixmap->drawable.pScreen;
		ScrnInfoPtr scrn = xf86Screens[screen->myNum];
		intel_screen_private *intel = intel_get_screen_private(scrn);

		if (bo == intel->front_buffer->bo)
			intel->need_flush = TRUE;

		if (!scrn->vtSema) {
			dri_bo_unmap(bo);
			pixmap->devPrivate.ptr = NULL;
			return;
		}

		if (bo->size < intel->max_gtt_map_size)
			drm_intel_gem_bo_unmap_gtt(bo);
		else
			dri_bo_unmap(bo);
		pixmap->devPrivate.ptr = NULL;
	}
}

void i830_uxa_block_handler(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (intel->need_flush) {
		dri_bo_wait_rendering(intel->front_buffer->bo);
		intel->need_flush = FALSE;
	}
}

static Bool i830_uxa_pixmap_is_offscreen(PixmapPtr pixmap)
{
	ScreenPtr screen = pixmap->drawable.pScreen;

	/* The front buffer is always in memory and pinned */
	if (screen->GetScreenPixmap(screen) == pixmap)
		return TRUE;

	return i830_get_pixmap_bo(pixmap) != NULL;
}

static PixmapPtr
i830_uxa_create_pixmap(ScreenPtr screen, int w, int h, int depth,
		       unsigned usage)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	dri_bo *bo;
	int stride;
	PixmapPtr pixmap;

	if (w > 32767 || h > 32767)
		return NullPixmap;

	if (usage == CREATE_PIXMAP_USAGE_GLYPH_PICTURE)
		return fbCreatePixmap(screen, w, h, depth, usage);

	pixmap = fbCreatePixmap(screen, 0, 0, depth, usage);

	if (w && h) {
		unsigned int size;
		uint32_t tiling = I915_TILING_NONE;
		int pitch_align;

		if (usage == INTEL_CREATE_PIXMAP_TILING_X) {
			tiling = I915_TILING_X;
			pitch_align = 512;
		} else if (usage == INTEL_CREATE_PIXMAP_TILING_Y) {
			tiling = I915_TILING_Y;
			pitch_align = 512;
		} else {
			pitch_align = intel->accel_pixmap_pitch_alignment;
		}

		stride = ROUND_TO((w * pixmap->drawable.bitsPerPixel + 7) / 8,
				  pitch_align);

		if (tiling == I915_TILING_NONE) {
			/* Round the height up so that the GPU's access to a 2x2 aligned
			 * subspan doesn't address an invalid page offset beyond the
			 * end of the GTT.
			 */
			size = stride * ALIGN(h, 2);
		} else {
			int aligned_h = h;
			if (tiling == I915_TILING_X)
				aligned_h = ALIGN(h, 8);
			else
				aligned_h = ALIGN(h, 32);

			stride = i830_get_fence_pitch(intel, stride, tiling);
			/* Round the object up to the size of the fence it will live in
			 * if necessary.  We could potentially make the kernel allocate
			 * a larger aperture space and just bind the subset of pages in,
			 * but this is easier and also keeps us out of trouble (as much)
			 * with drm_intel_bufmgr_check_aperture().
			 */
			size = i830_get_fence_size(intel, stride * aligned_h);
			assert(size >= stride * aligned_h);
		}

		/* Fail very large allocations on 32-bit systems.  Large BOs will
		 * tend to hit SW fallbacks frequently, and also will tend to fail
		 * to successfully map when doing SW fallbacks because we overcommit
		 * address space for BO access.
		 *
		 * Note that size should fit in 32 bits.  We throw out >32767x32767x4,
		 * and pitch alignment could get us up to 32768x32767x4.
		 */
		if (sizeof(unsigned long) == 4 &&
		    size > (unsigned int)(1024 * 1024 * 1024)) {
			fbDestroyPixmap(pixmap);
			return NullPixmap;
		}

		if (usage == UXA_CREATE_PIXMAP_FOR_MAP)
			bo = drm_intel_bo_alloc(intel->bufmgr, "pixmap", size,
						0);
		else
			bo = drm_intel_bo_alloc_for_render(intel->bufmgr,
							   "pixmap", size, 0);
		if (!bo) {
			fbDestroyPixmap(pixmap);
			return NullPixmap;
		}

		if (tiling != I915_TILING_NONE)
			drm_intel_bo_set_tiling(bo, &tiling, stride);

		screen->ModifyPixmapHeader(pixmap, w, h, 0, 0, stride, NULL);

		i830_uxa_set_pixmap_bo(pixmap, bo);
	}

	return pixmap;
}

static Bool i830_uxa_destroy_pixmap(PixmapPtr pixmap)
{
	if (pixmap->refcnt == 1) {
		dri_bo *bo = i830_get_pixmap_bo(pixmap);

		if (bo)
			dri_bo_unreference(bo);
	}
	fbDestroyPixmap(pixmap);
	return TRUE;
}

void i830_uxa_create_screen_resources(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	dri_bo *bo = intel->front_buffer->bo;

	if (bo != NULL) {
		PixmapPtr pixmap = screen->GetScreenPixmap(screen);
		i830_uxa_set_pixmap_bo(pixmap, bo);
		dri_bo_reference(bo);
	}
}

Bool i830_uxa_init(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (!dixRequestPrivate(&uxa_pixmap_index, 0))
		return FALSE;

	intel->uxa_driver = uxa_driver_alloc();
	if (intel->uxa_driver == NULL)
		return FALSE;

	memset(intel->uxa_driver, 0, sizeof(*intel->uxa_driver));

	intel->bufferOffset = 0;
	intel->uxa_driver->uxa_major = 1;
	intel->uxa_driver->uxa_minor = 0;

	/* Solid fill */
	intel->uxa_driver->prepare_solid = i830_uxa_prepare_solid;
	intel->uxa_driver->solid = i830_uxa_solid;
	intel->uxa_driver->done_solid = i830_uxa_done_solid;

	/* Copy */
	intel->uxa_driver->prepare_copy = i830_uxa_prepare_copy;
	intel->uxa_driver->copy = i830_uxa_copy;
	intel->uxa_driver->done_copy = i830_uxa_done_copy;

	/* Composite */
	if (!IS_I9XX(intel)) {
		intel->uxa_driver->check_composite = i830_check_composite;
		intel->uxa_driver->prepare_composite = i830_prepare_composite;
		intel->uxa_driver->composite = i830_composite;
		intel->uxa_driver->done_composite = i830_done_composite;
	} else if (IS_I915G(intel) || IS_I915GM(intel) ||
		   IS_I945G(intel) || IS_I945GM(intel) || IS_G33CLASS(intel)) {
		intel->uxa_driver->check_composite = i915_check_composite;
		intel->uxa_driver->prepare_composite = i915_prepare_composite;
		intel->uxa_driver->composite = i915_composite;
		intel->uxa_driver->done_composite = i830_done_composite;
	} else {
		intel->uxa_driver->check_composite = i965_check_composite;
		intel->uxa_driver->prepare_composite = i965_prepare_composite;
		intel->uxa_driver->composite = i965_composite;
		intel->uxa_driver->done_composite = i830_done_composite;
	}

	intel->uxa_driver->prepare_access = i830_uxa_prepare_access;
	intel->uxa_driver->finish_access = i830_uxa_finish_access;
	intel->uxa_driver->pixmap_is_offscreen = i830_uxa_pixmap_is_offscreen;

	if (!uxa_driver_init(screen, intel->uxa_driver)) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "UXA initialization failed\n");
		xfree(intel->uxa_driver);
		return FALSE;
	}

	screen->CreatePixmap = i830_uxa_create_pixmap;
	screen->DestroyPixmap = i830_uxa_destroy_pixmap;

	uxa_set_fallback_debug(screen, intel->fallback_debug);

	return TRUE;
}
