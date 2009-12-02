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

/* Our hardware status area is just a single page */
#define HWSTATUS_PAGE_SIZE GTT_PAGE_SIZE

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

void i830_free_memory(ScrnInfoPtr scrn, i830_memory * mem)
{
	if (mem == NULL)
		return;

	if (mem->bo != NULL) {
		intel_screen_private *intel = intel_get_screen_private(scrn);
		dri_bo_unreference(mem->bo);
		if (intel->bo_list == mem) {
			intel->bo_list = mem->next;
			if (mem->next)
				mem->next->prev = NULL;
		} else {
			if (mem->prev)
				mem->prev->next = mem->next;
			if (mem->next)
				mem->next->prev = mem->prev;
		}
		xfree(mem->name);
		xfree(mem);
		return;
	}
	/* Disconnect from the list of allocations */
	if (mem->prev != NULL)
		mem->prev->next = mem->next;
	if (mem->next != NULL)
		mem->next->prev = mem->prev;

	xfree(mem->name);
	xfree(mem);
}

/* Resets the state of the aperture allocator, freeing all memory that had
 * been allocated.
 */
void i830_reset_allocations(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int p;

	/* While there is any memory between the start and end markers, free it. */
	while (intel->memory_list->next->next != NULL) {
		i830_memory *mem = intel->memory_list->next;

		i830_free_memory(scrn, mem);
	}

	/* Free any allocations in buffer objects */
	while (intel->bo_list != NULL)
		i830_free_memory(scrn, intel->bo_list);

	/* Null out the pointers for all the allocations we just freed.  This is
	 * kind of gross, but at least it's just one place now.
	 */
	for (p = 0; p < 2; p++)
		intel->cursor_mem_argb[p] = NULL;

	intel->front_buffer = NULL;
}

/**
 * Initialize's the driver's video memory allocator to allocate in the
 * given range.
 *
 * This sets up the kernel memory manager to manage as much of the memory
 * as we think it can, while leaving enough to us to fulfill our non-GEM
 * static allocations.  Some of these exist because of the need for physical
 * addresses to reference.
 */
Bool i830_allocator_init(ScrnInfoPtr scrn, unsigned long size)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	i830_memory *start, *end;

	start = xcalloc(1, sizeof(*start));
	if (start == NULL)
		return FALSE;
	start->name = xstrdup("start marker");
	if (start->name == NULL) {
		xfree(start);
		return FALSE;
	}
	end = xcalloc(1, sizeof(*end));
	if (end == NULL) {
		xfree(start->name);
		xfree(start);
		return FALSE;
	}
	end->name = xstrdup("end marker");
	if (end->name == NULL) {
		xfree(start->name);
		xfree(start);
		xfree(end);
		return FALSE;
	}

	start->offset = 0;
	start->end = start->offset;
	start->size = 0;
	start->next = end;
	end->offset = size;
	end->end = end->offset;
	end->size = 0;
	end->prev = start;

	intel->memory_list = start;

	return TRUE;
}

void i830_allocator_fini(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	/* Free most of the allocations */
	i830_reset_allocations(scrn);

	/* Free the start/end markers */
	free(intel->memory_list->next);
	free(intel->memory_list);
	intel->memory_list = NULL;
}

/* Allocates video memory at the given size, pitch, alignment and tile format.
 *
 * The memory will be bound automatically when the driver is in control of the
 * VT.  When the kernel memory manager is available and compatible with flags
 * (that is, flags doesn't say that the allocation must include a physical
 * address), that will be used for the allocation.
 *
 * flags:
 * - NEED_PHYSICAL_ADDR: Allocates the memory physically contiguous, and return
 *   the bus address for that memory.
 * - NEED_NON-STOLEN: don't allow any part of the memory allocation to lie
 *   within stolen memory
 * - NEED_LIFETIME_FIXED: don't allow the buffer object to move throughout
 *   the entire Screen lifetime.  This means not using buffer objects, which
 *   get their offsets chosen at each EnterVT time.
 */
i830_memory *i830_allocate_memory(ScrnInfoPtr scrn, const char *name,
				  unsigned long size, unsigned long pitch,
				  int flags, uint32_t tiling_mode)
{
	i830_memory *mem;
	intel_screen_private *intel = intel_get_screen_private(scrn);
	uint32_t requested_tiling_mode = tiling_mode;
	int ret;

	/* Manage tile alignment and size constraints */
	if (tiling_mode != I915_TILING_NONE) {
		/* Only allocate page-sized increments. */
		size = ALIGN(size, GTT_PAGE_SIZE);

		/* Check for maximum tiled region size */
		if (IS_I9XX(intel)) {
			if (size > MB(128))
				return NULL;
		} else {
			if (size > MB(64))
				return NULL;
		}

		/* round to size necessary for the fence register to work */
		size = i830_get_fence_size(intel, size);
	}

	assert((flags & NEED_PHYSICAL_ADDR) == 0);

	/* Only allocate page-sized increments. */
	size = ALIGN(size, GTT_PAGE_SIZE);

	mem = xcalloc(1, sizeof(*mem));
	if (mem == NULL)
		return NULL;

	mem->name = xstrdup(name);
	if (mem->name == NULL) {
		xfree(mem);
		return NULL;
	}

	mem->bo = dri_bo_alloc(intel->bufmgr, name, size, GTT_PAGE_SIZE);

	if (!mem->bo) {
		xfree(mem->name);
		xfree(mem);
		return NULL;
	}

	/* Give buffer obviously wrong offset/end until it's pinned. */
	mem->offset = -1;
	mem->end = -1;
	mem->size = size;
	mem->pitch = pitch;

	ret = drm_intel_bo_set_tiling(mem->bo, &tiling_mode, pitch);
	if (ret != 0 || tiling_mode != requested_tiling_mode) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to set tiling on %s: %s\n", mem->name,
			   ret == 0 ? "rejected by kernel" : strerror(-ret));
	}
	mem->tiling_mode = tiling_mode;

	if (flags & DISABLE_REUSE)
		drm_intel_bo_disable_reuse(mem->bo);

	/* Insert new allocation into the list */
	mem->prev = NULL;
	mem->next = intel->bo_list;
	if (intel->bo_list != NULL)
		intel->bo_list->prev = mem;
	intel->bo_list = mem;

	return mem;
}

void
i830_describe_allocations(ScrnInfoPtr scrn, int verbosity, const char *prefix)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	i830_memory *mem;

	if (intel->memory_list == NULL) {
		xf86DrvMsgVerb(scrn->scrnIndex, X_INFO, verbosity,
			       "%sMemory allocator not initialized\n", prefix);
		return;
	}

	if (intel->memory_list->next->next == NULL) {
		xf86DrvMsgVerb(scrn->scrnIndex, X_INFO, verbosity,
			       "%sNo memory allocations\n", prefix);
		return;
	}

	xf86DrvMsgVerb(scrn->scrnIndex, X_INFO, verbosity,
		       "%sFixed memory allocation layout:\n", prefix);

	for (mem = intel->memory_list->next; mem->next != NULL; mem = mem->next) {
		char phys_suffix[32] = "";
		char *tile_suffix = "";

		if (mem->tiling_mode == I915_TILING_X)
			tile_suffix = " X tiled";
		else if (mem->tiling_mode == I915_TILING_Y)
			tile_suffix = " Y tiled";

		xf86DrvMsgVerb(scrn->scrnIndex, X_INFO, verbosity,
			       "%s0x%08lx-0x%08lx: %s (%ld kB%s)%s\n", prefix,
			       mem->offset, mem->end - 1, mem->name,
			       mem->size / 1024, phys_suffix, tile_suffix);
	}
	xf86DrvMsgVerb(scrn->scrnIndex, X_INFO, verbosity,
		       "%s0x%08lx:            end of aperture\n",
		       prefix, intel->FbMapSize);

	xf86DrvMsgVerb(scrn->scrnIndex, X_INFO, verbosity,
		       "%sBO memory allocation layout:\n", prefix);
	for (mem = intel->bo_list; mem != NULL; mem = mem->next) {
		char *tile_suffix = "";

		if (mem->tiling_mode == I915_TILING_X)
			tile_suffix = " X tiled";
		else if (mem->tiling_mode == I915_TILING_Y)
			tile_suffix = " Y tiled";

		xf86DrvMsgVerb(scrn->scrnIndex, X_INFO, verbosity,
			       "%sunpinned          : %s (%ld kB)%s\n", prefix,
			       mem->name, mem->size / 1024, tile_suffix);
	}
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
i830_memory *i830_allocate_framebuffer(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	unsigned int pitch = scrn->displayWidth * intel->cpp;
	unsigned long minspace;
	long size, fb_height;
	int flags;
	i830_memory *front_buffer = NULL;
	uint32_t tiling_mode;

	flags = ALLOW_SHARING | DISABLE_REUSE;

	/* We'll allocate the fb such that the root window will fit regardless of
	 * rotation.
	 */
	fb_height = scrn->virtualY;

	/* Calculate how much framebuffer memory to allocate.  For the
	 * initial allocation, calculate a reasonable minimum.  This is
	 * enough for the virtual screen size.
	 */
	minspace = pitch * scrn->virtualY;

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

	front_buffer = i830_allocate_memory(scrn, "front buffer", size,
					    pitch, flags, tiling_mode);

	if (front_buffer == NULL) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to allocate framebuffer.\n");
		return NULL;
	}

	i830_set_max_gtt_map_size(scrn);
	i830_set_max_tiling_size(scrn);

	return front_buffer;
}

static Bool i830_allocate_cursor_buffers(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	int i;

	/*
	 * Allocate four separate buffers when the kernel doesn't support
	 * large allocations as on Linux. If any of these fail, just
	 * bail back to software cursors everywhere
	 */
	for (i = 0; i < xf86_config->num_crtc; i++) {
		intel->cursor_mem_argb[i] =
		    i830_allocate_memory(scrn, "ARGB cursor",
					 HWCURSOR_SIZE_ARGB, PITCH_NONE,
					 DISABLE_REUSE, I915_TILING_NONE);
		if (!intel->cursor_mem_argb[i])
			return FALSE;

	}
	return TRUE;
}

/*
 * Allocate memory for 2D operation.  This includes the (front) framebuffer,
 * ring buffer, scratch memory, HW cursor.
 */
Bool i830_allocate_2d_memory(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	/* Next, allocate other fixed-size allocations we have. */
	if (!i830_allocate_cursor_buffers(scrn)) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to allocate HW cursor space.\n");
		return FALSE;
	}

	intel->front_buffer = i830_allocate_framebuffer(scrn);
	if (intel->front_buffer == NULL)
		return FALSE;

	return TRUE;
}

/**
 * Called at EnterVT to grab the AGP GART and bind our allocations.
 *
 * In zaphod mode, this will walk the list trying to bind twice, since each
 * intel points to the same allocation list, but the bind_memory will just
 * no-op then.
 */
Bool i830_bind_all_memory(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	int i;

	if (intel->memory_list == NULL)
		return TRUE;

	for (i = 0; i < xf86_config->num_crtc; i++)
		drmmode_crtc_set_cursor_bo(xf86_config->crtc[i],
					   intel->cursor_mem_argb[i]->bo);

	i830_set_max_gtt_map_size(scrn);
	i830_set_max_tiling_size(scrn);

	if (intel->front_buffer)
		scrn->fbOffset = intel->front_buffer->offset;

	return TRUE;
}

#ifdef INTEL_XVMC
/*
 * Allocate memory for MC compensation
 */
Bool i830_allocate_xvmc_buffer(ScrnInfoPtr scrn, const char *name,
			       i830_memory ** buffer, unsigned long size,
			       int flags)
{
	*buffer = i830_allocate_memory(scrn, name, size, PITCH_NONE,
				       flags, I915_TILING_NONE);

	if (!*buffer) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to allocate memory for %s.\n", name);
		return FALSE;
	}

	if ((*buffer)->bo) {
		if (drm_intel_bo_pin((*buffer)->bo, GTT_PAGE_SIZE)) {
			i830_free_memory(scrn, *buffer);
			xf86DrvMsg(scrn->scrnIndex, X_ERROR,
				   "Failed to bind XvMC buffer bo!\n");
			return FALSE;
		}

		(*buffer)->offset = (*buffer)->bo->offset;
	}

	return TRUE;
}

void i830_free_xvmc_buffer(ScrnInfoPtr scrn, i830_memory * buffer)
{
	if (buffer->bo)
		drm_intel_bo_unpin(buffer->bo);

	i830_free_memory(scrn, buffer);
}

#endif

void i830_set_max_gtt_map_size(ScrnInfoPtr scrn)
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

void i830_set_max_tiling_size(ScrnInfoPtr scrn)
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
