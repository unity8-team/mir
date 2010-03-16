/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright © 2002 by David Dawes.

All Rights Reserved.

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
IN NO EVENT SHALL THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *   David Dawes <dawes@xfree86.org>
 *
 * Updated for Dual Head capabilities:
 *   Alan Hourihane <alanh@tungstengraphics.com>
 */

/**
 * @file i830_memory.c
 *
 * This is the video memory allocator.  Our memory allocation is different from
 * other graphics chips, where you have a fixed amount of graphics memory
 * available that you want to put to the best use.  Instead, we have almost no
 * memory pre-allocated, and we have to choose an appropriate amount of sytem
 * memory to use.
 *
 * The allocations we might do:
 *
 * - Ring buffer
 * - HW cursor block (either one block or four)
 * - Overlay registers
 * - Front buffer (screen 1)
 * - Front buffer (screen 2, only in zaphod mode)
 * - Back/depth buffer (3D only)
 * - Compatibility texture pool (optional, more is always better)
 * - New texture pool (optional, more is always better.  aperture allocation
 *     only)
 *
 * The user may request a specific amount of memory to be used
 * (intel->pEnt->videoRam != 0), in which case allocations have to fit within
 * that much aperture.  If not, the individual allocations will be
 * automatically sized, and will be fit within the maximum aperture size.
 * Only the actual memory used (not alignment padding) will get actual AGP
 * memory allocated.
 *
 * Given that the allocations listed are generally a page or more than a page,
 * our allocator will only return page-aligned offsets, simplifying the memory
 * binding process.  For smaller allocations, the acceleration architecture's
 * linear allocator is preferred.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "xf86.h"
#include "xf86_OSproc.h"

#include "i830.h"
#include "i810_reg.h"
#include "i915_drm.h"

/**
 * Returns the fence size for a tiled area of the given size.
 */
unsigned long i830_get_fence_size(intel_screen_private *intel, unsigned long size)
{
	unsigned long i;
	unsigned long start;

	if (IS_I965G(intel)) {
		/* The 965 can have fences at any page boundary. */
		return ALIGN(size, GTT_PAGE_SIZE);
	} else {
		/* Align the size to a power of two greater than the smallest fence
		 * size.
		 */
		if (IS_I9XX(intel))
			start = MB(1);
		else
			start = KB(512);

		for (i = start; i < size; i <<= 1) ;

		return i;
	}
}

/**
 * On some chips, pitch width has to be a power of two tile width, so
 * calculate that here.
 */
unsigned long
i830_get_fence_pitch(intel_screen_private *intel, unsigned long pitch,
		     uint32_t tiling_mode)
{
	unsigned long i;
	unsigned long tile_width = (tiling_mode == I915_TILING_Y) ? 128 : 512;

	if (tiling_mode == I915_TILING_NONE)
		return pitch;

	/* 965 is flexible */
	if (IS_I965G(intel))
		return ROUND_TO(pitch, tile_width);

	/* Pre-965 needs power of two tile width */
	for (i = tile_width; i < pitch; i <<= 1) ;

	return i;
}

static Bool
i830_check_display_stride(ScrnInfoPtr scrn, int stride, Bool tiling)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int limit = KB(32);

	/* 8xx spec has always 8K limit, but tests show larger limit in
	   non-tiling mode, which makes large monitor work. */
	if ((IS_845G(intel) || IS_I85X(intel)) && tiling)
		limit = KB(8);

	if (IS_I915(intel) && tiling)
		limit = KB(8);

	if (IS_I965G(intel) && tiling)
		limit = KB(16);

	if (IS_IGDNG(intel) && tiling)
		limit = KB(32);

	if (stride <= limit)
		return TRUE;
	else
		return FALSE;
}

/* Resets the state of the aperture allocator, freeing all memory that had
 * been allocated.
 */
void i830_reset_allocations(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	/* Null out the pointers for all the allocations we just freed.  This is
	 * kind of gross, but at least it's just one place now.
	 */
	drm_intel_bo_unreference(intel->front_buffer);
	intel->front_buffer = NULL;
}

static Bool IsTileable(ScrnInfoPtr scrn, int pitch)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (IS_I965G(intel)) {
		if (pitch / 512 * 512 == pitch && pitch <= KB(128))
			return TRUE;
		else
			return FALSE;
	}

	/*
	 * Allow tiling for pitches that are a power of 2 multiple of 128 bytes,
	 * up to 64 * 128 (= 8192) bytes.
	 */
	switch (pitch) {
	case 128:
	case 256:
		if (IS_I945G(intel) || IS_I945GM(intel) || IS_G33CLASS(intel))
			return TRUE;
		else
			return FALSE;
	case 512:
	case KB(1):
	case KB(2):
	case KB(4):
	case KB(8):
		return TRUE;
	default:
		return FALSE;
	}
}

/**
 * Allocates a framebuffer for a screen.
 *
 * Used once for each X screen, so once with RandR 1.2 and twice with classic
 * dualhead.
 */
drm_intel_bo *i830_allocate_framebuffer(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	unsigned int pitch = scrn->displayWidth * intel->cpp;
	long size, fb_height;
	int flags, ret;
	drm_intel_bo *front_buffer = NULL;
	uint32_t tiling_mode, requested_tiling_mode;

	flags = ALLOW_SHARING | DISABLE_REUSE;

	/* We'll allocate the fb such that the root window will fit regardless of
	 * rotation.
	 */
	fb_height = scrn->virtualY;

	size = ROUND_TO_PAGE(pitch * fb_height);

	if (intel->tiling && IsTileable(scrn, pitch))
		tiling_mode = I915_TILING_X;
	else
		tiling_mode = I915_TILING_NONE;

	if (!i830_check_display_stride(scrn, pitch,
				       tiling_mode != I915_TILING_NONE)) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Front buffer stride %d kB "
			   "exceed display limit\n", pitch / 1024);
		return NULL;
	}

	if (tiling_mode != I915_TILING_NONE) {
		/* round to size necessary for the fence register to work */
		size = i830_get_fence_size(intel, size);
	}

	front_buffer = drm_intel_bo_alloc(intel->bufmgr, "front buffer",
					  size, GTT_PAGE_SIZE);

	if (front_buffer == NULL) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to allocate framebuffer.\n");
		return NULL;
	}

	requested_tiling_mode = tiling_mode;
	ret = drm_intel_bo_set_tiling(front_buffer, &tiling_mode, pitch);
	if (ret != 0 || tiling_mode != requested_tiling_mode) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to set tiling on frontbuffer: %s\n",
			   ret == 0 ? "rejected by kernel" : strerror(-ret));
	}

	drm_intel_bo_disable_reuse(front_buffer);

	i830_set_gem_max_sizes(scrn);

	return front_buffer;
}

static void i830_set_max_gtt_map_size(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct drm_i915_gem_get_aperture aperture;
	int ret;

	/* Default low value in case it gets used during server init. */
	intel->max_gtt_map_size = 16 * 1024 * 1024;

	ret =
	    ioctl(intel->drmSubFD, DRM_IOCTL_I915_GEM_GET_APERTURE, &aperture);
	if (ret == 0) {
		/* Let objects up get bound up to the size where only 2 would fit in
		 * the aperture, but then leave slop to account for alignment like
		 * libdrm does.
		 */
		intel->max_gtt_map_size =
		    aperture.aper_available_size * 3 / 4 / 2;
	}
}

static void i830_set_max_tiling_size(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct drm_i915_gem_get_aperture aperture;
	int ret;

	/* Default low value in case it gets used during server init. */
	intel->max_tiling_size = 4 * 1024 * 1024;

	ret =
	    ioctl(intel->drmSubFD, DRM_IOCTL_I915_GEM_GET_APERTURE, &aperture);
	if (ret == 0) {
		/* Let objects be tiled up to the size where only 4 would fit in
		 * the aperture, presuming worst case alignment.
		 */
		intel->max_tiling_size = aperture.aper_available_size / 4;
		if (!IS_I965G(intel))
			intel->max_tiling_size /= 2;
	}
}

void i830_set_gem_max_sizes(ScrnInfoPtr scrn)
{
	i830_set_max_gtt_map_size(scrn);
	i830_set_max_tiling_size(scrn);
}
