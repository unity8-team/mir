/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i830_memory.c,v 1.9 2003/09/24 03:16:54 dawes Exp $ */
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
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

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
 * - XAA linear allocator (optional)
 * - EXA 965 state buffer
 * - XAA scratch (screen 1)
 * - XAA scratch (screen 2, only in zaphod mode)
 * - Front buffer (screen 1, more is better for XAA)
 * - Front buffer (screen 2, only in zaphod mode, more is better for XAA)
 * - Back/depth buffer (3D only)
 * - Compatibility texture pool (optional, more is always better)
 * - New texture pool (optional, more is always better.  aperture allocation
 *     only)
 * - EXA offscreen pool (more is always better)
 *
 * We also want to be able to resize the front/back/depth buffers, and then
 * resize the EXA and texture memory pools appropriately.
 *
 * The user may request a specific amount of memory to be used
 * (pI830->pEnt->videoRam != 0), in which case allocations have to fit within
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
#include <string.h>

#include "xf86.h"
#include "xf86_OSproc.h"

#include "i830.h"
#include "i810_reg.h"
#include "i830_reg.h"

#define ALIGN(i,m)    (((i) + (m) - 1) & ~((m) - 1))

enum tile_format {
    TILING_NONE,
    TILING_XMAJOR,
    TILING_YMAJOR
};

static void i830_set_fence(ScrnInfoPtr pScrn, int nr, unsigned int offset,
			   unsigned int pitch, unsigned int size,
			   enum tile_format tile_format);

/**
 * Returns the fence size for a tiled area of the given size.
 */
static unsigned long
i830_get_fence_size(ScrnInfoPtr pScrn, unsigned long size)
{
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long i;
    unsigned long start;

    if (IS_I965G(pI830)) {
	/* The 965 can have fences at any page boundary. */
	return ALIGN(size, GTT_PAGE_SIZE);
    } else {
	/* Align the size to a power of two greater than the smallest fence
	 * size.
	 */
	if (IS_I9XX(pI830))
	    start = MB(1);
	else
	    start = KB(512);

	for (i = start; i < size; i <<= 1)
	    ;

	return i;
    }
}

static Bool
i830_bind_memory(ScrnInfoPtr pScrn, i830_memory *mem)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (mem == NULL || mem->key == -1 || mem->bound || !pI830->gtt_acquired)
	return TRUE;

    if (xf86BindGARTMemory(pScrn->scrnIndex, mem->key, mem->agp_offset)) {
	mem->bound = TRUE;
	return TRUE;
    } else {
	return FALSE;
    }

    return TRUE;
}

static Bool
i830_unbind_memory(ScrnInfoPtr pScrn, i830_memory *mem)
{
    if (mem == NULL || mem->key == -1 || !mem->bound)
	return TRUE;

    if (xf86UnbindGARTMemory(pScrn->scrnIndex, mem->key)) {
	mem->bound = FALSE;
	return TRUE;
    } else {
	return FALSE;
    }
}

void
i830_free_memory(ScrnInfoPtr pScrn, i830_memory *mem)
{
    if (mem == NULL)
	return;

    /* Disconnect from the list of allocations */
    if (mem->prev != NULL)
	mem->prev->next = mem->next;
    if (mem->next != NULL)
	mem->next->prev = mem->prev;

    /* Free any AGP memory. */
    i830_unbind_memory(pScrn, mem);

    if (mem->key != -1) {
	xf86DeallocateGARTMemory(pScrn->scrnIndex, mem->key);
	mem->key = -1;
    }

    xfree(mem->name);
    xfree(mem);
}

/* Resets the state of the aperture allocator, freeing all memory that had
 * been allocated.
 */
void
i830_reset_allocations(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int	    p;

    /* While there is any memory between the start and end markers, free it. */
    while (pI830->memory_list->next->next != NULL)
	i830_free_memory(pScrn, pI830->memory_list->next);

    /* Null out the pointers for all the allocations we just freed.  This is
     * kind of gross, but at least it's just one place now.
     */
    pI830->cursor_mem = NULL;
    for (p = 0; p < 2; p++) {
	pI830->cursor_mem_classic[p] = NULL;
	pI830->cursor_mem_argb[p] = NULL;
    }
    pI830->front_buffer = NULL;
    pI830->front_buffer_2 = NULL;
    pI830->xaa_scratch = NULL;
    pI830->xaa_scratch_2 = NULL;
    pI830->exa_offscreen = NULL;
    pI830->exa_965_state = NULL;
    pI830->overlay_regs = NULL;
    pI830->logical_context = NULL;
#ifdef XF86DRI
    pI830->back_buffer = NULL;
    pI830->third_buffer = NULL;
    pI830->depth_buffer = NULL;
    pI830->textures = NULL;
    pI830->memory_manager = NULL;
#endif
    pI830->LpRing->mem = NULL;

    /* Reset the fence register allocation. */
    pI830->next_fence = 0;
    memset(pI830->fence, 0, sizeof(pI830->fence));
}

void
i830_free_3d_memory(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

#ifdef XF86DRI
    i830_free_memory(pScrn, pI830->back_buffer);
    pI830->back_buffer = NULL;
    i830_free_memory(pScrn, pI830->third_buffer);
    pI830->third_buffer = NULL;
    i830_free_memory(pScrn, pI830->depth_buffer);
    pI830->depth_buffer = NULL;
    i830_free_memory(pScrn, pI830->textures);
    pI830->textures = NULL;
    i830_free_memory(pScrn, pI830->memory_manager);
    pI830->memory_manager = NULL;
#endif
}

/**
 * Initialize's the driver's video memory allocator to allocate in the
 * given range.
 */
Bool
i830_allocator_init(ScrnInfoPtr pScrn, unsigned long offset,
		    unsigned long size)
{
    I830Ptr pI830 = I830PTR(pScrn);
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

    start->key = -1;
    start->offset = offset;
    start->end = start->offset;
    start->size = 0;
    start->next = end;
    end->key = -1;
    end->offset = offset + size;
    end->end = end->offset;
    end->size = 0;
    end->prev = start;

    pI830->memory_list = start;

    return TRUE;
}

/**
 * Reads a GTT entry for the memory at the given offset and returns the
 * physical address.
 *
 * \return physical address if successful.
 * \return (uint64_t)-1 if unsuccessful.
 */
static uint64_t
i830_get_gtt_physical(ScrnInfoPtr pScrn, unsigned long offset)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 gttentry;

    /* We don't have GTTBase set up on i830 yet. */
    if (pI830->GTTBase == NULL)
	return -1;

    gttentry = INGTT(offset / 1024);

    /* Mask out these reserved bits on this hardware. */
    if (!IS_I9XX(pI830) || IS_I915G(pI830) || IS_I915GM(pI830) ||
	IS_I945G(pI830) || IS_I945GM(pI830))
    {
	gttentry &= ~PTE_ADDRESS_MASK_HIGH;
    }

    /* If it's not a mapping type we know, then bail. */
    if ((gttentry & PTE_MAPPING_TYPE_MASK) != PTE_MAPPING_TYPE_UNCACHED &&
	(gttentry & PTE_MAPPING_TYPE_MASK) != PTE_MAPPING_TYPE_CACHED)
    {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Unusable physical mapping type 0x%08x\n",
		   (unsigned int)(gttentry & PTE_MAPPING_TYPE_MASK));
	return -1;
    }
    assert((gttentry & PTE_VALID) != 0);

    return (gttentry & PTE_ADDRESS_MASK) |
	((uint64_t)(gttentry & PTE_ADDRESS_MASK_HIGH) << (32 - 4));
}

/**
 * Reads the GTT entries for stolen memory at the given offset, returning the
 * physical address.
 *
 * \return physical address if successful.
 * \return (uint64_t)-1 if unsuccessful.
 */
static uint64_t
i830_get_stolen_physical(ScrnInfoPtr pScrn, unsigned long offset,
			 unsigned long size)
{
    I830Ptr pI830 = I830PTR(pScrn);
    uint64_t physical;
    unsigned long scan;

    /* Check that the requested region is within stolen memory. */
    if (offset + size >= pI830->stolen_size)
	return -1;

    physical = i830_get_gtt_physical(pScrn, offset);
    if (physical == -1)
	return -1;

    /* Check that the following pages in our allocation follow the first page
     * contiguously.
     */
    for (scan = offset + 4096; scan < offset + size; scan += 4096) {
	uint64_t scan_physical = i830_get_gtt_physical(pScrn, scan);

	if ((scan - offset) != (scan_physical - physical)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Non-contiguous GTT entries: (%ld,0x16%llx) vs "
		       "(%ld,0x%16llx)\n",
		       scan, scan_physical, offset, physical);
	    return -1;
	}
    }

    return physical;
}

/* Allocate aperture space for the given size and alignment, and returns the
 * memory allocation.
 *
 * Allocations are a minimum of a page, and will be at least page-aligned.
 */
static i830_memory *
i830_allocate_aperture(ScrnInfoPtr pScrn, const char *name,
		       long size, unsigned long alignment, int flags)
{
    I830Ptr pI830 = I830PTR(pScrn);
    i830_memory *mem, *scan;

    mem = xcalloc(1, sizeof(*mem));
    if (mem == NULL)
	return NULL;

    /* No memory allocated to back the region */
    mem->key = -1;

    mem->name = xstrdup(name);
    if (mem->name == NULL) {
	xfree(mem);
	return NULL;
    }
    /* Only allocate page-sized increments. */
    size = ALIGN(size, GTT_PAGE_SIZE);
    mem->size = size;

    if (alignment < GTT_PAGE_SIZE)
	alignment = GTT_PAGE_SIZE;

    for (scan = pI830->memory_list; scan->next != NULL; scan = scan->next) {
	mem->offset = ROUND_TO(scan->end, alignment);
	if ((flags & NEED_PHYSICAL_ADDR) && mem->offset < pI830->stolen_size) {
	    /* If the allocation is entirely within stolen memory, and we're
	     * able to get the physical addresses out of the GTT and check that
	     * it's contiguous (it ought to be), then we can do our physical
	     * allocations there and not bother the kernel about it.  This
	     * helps avoid aperture fragmentation from our physical
	     * allocations.
	     */
	    mem->bus_addr = i830_get_stolen_physical(pScrn, mem->offset,
						     mem->size);

	    if (mem->bus_addr == ((uint64_t)-1)) {
		/* Move the start of the allocation to just past the end of
		 * stolen memory.
		 */
		mem->offset = ROUND_TO(pI830->stolen_size, alignment);
	    }
	}

	mem->end = mem->offset + size;
	if (flags & ALIGN_BOTH_ENDS)
	    mem->end = ROUND_TO(mem->end, alignment);
	if (mem->end <= scan->next->offset)
	    break;
    }
    if (scan->next == NULL) {
	/* Reached the end of the list, and didn't find space */
	xfree(mem->name);
	xfree(mem);
	return NULL;
    }
    /* Insert new allocation into the list */
    mem->prev = scan;
    mem->next = scan->next;
    scan->next = mem;
    mem->next->prev = mem;

    return mem;
}

/**
 * Allocates the AGP memory necessary for the part of a memory allocation not
 * already covered by the stolen memory.
 *
 * The memory is automatically bound if we have the VT.
 */
static Bool
i830_allocate_agp_memory(ScrnInfoPtr pScrn, i830_memory *mem, int flags)
{
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long size;

    if (mem->key != -1)
	return TRUE;

    if (mem->offset + mem->size <= pI830->stolen_size)
	return TRUE;

    if (mem->offset < pI830->stolen_size)
	mem->agp_offset = pI830->stolen_size;
    else
	mem->agp_offset = mem->offset;

    size = mem->size - (mem->agp_offset - mem->offset);

    if (flags & NEED_PHYSICAL_ADDR) {
	unsigned long agp_bus_addr;

	mem->key = xf86AllocateGARTMemory(pScrn->scrnIndex, size, 2,
					  &agp_bus_addr);
	mem->bus_addr = agp_bus_addr;
    } else {
	mem->key = xf86AllocateGARTMemory(pScrn->scrnIndex, size, 0, NULL);
    }
    if (mem->key == -1 || ((flags & NEED_PHYSICAL_ADDR) && mem->bus_addr == 0))
    {
	return FALSE;
    }

    if (!i830_bind_memory(pScrn, mem)) {
	return FALSE;
    }

    return TRUE;
}


/* Allocates video memory at the given size and alignment.
 *
 * The memory will be bound automatically when the driver is in control of the
 * VT.
 */
static i830_memory *
i830_allocate_memory(ScrnInfoPtr pScrn, const char *name,
		     unsigned long size, unsigned long alignment, int flags)
{
    i830_memory *mem;

    mem = i830_allocate_aperture(pScrn, name, size, alignment, flags);
    if (mem == NULL)
	return NULL;

    if (!i830_allocate_agp_memory(pScrn, mem, flags)) {
	i830_free_memory(pScrn, mem);
	return NULL;
    }

    return mem;
}

/* Allocate a tiled region with the given size and pitch.
 *
 * As is, we might miss out on tiling some allocations on older hardware with
 * large framebuffer size and a small aperture size, where the first
 * allocations use a large alignment even though we've got fences to spare, and
 * the later allocations can't find enough aperture space left.  We could do
 * some search across all allocation options to fix this, probably, but that
 * would be another rewrite.
 */
static i830_memory *
i830_allocate_memory_tiled(ScrnInfoPtr pScrn, const char *name,
			   unsigned long size, unsigned long pitch,
			   unsigned long alignment, int flags,
			   enum tile_format tile_format)
{
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long aper_size;
    unsigned long aper_align;
    i830_memory *mem;
    int fence_divide, i;

    if (tile_format == TILING_NONE)
	return i830_allocate_memory(pScrn, name, size, alignment, flags);

    /* Only allocate page-sized increments. */
    size = ALIGN(size, GTT_PAGE_SIZE);

    /* Check for maximum tiled region size */
    if (IS_I9XX(pI830)) {
	if (size > MB(128))
	    return NULL;
    } else {
	if (size > MB(64))
	    return NULL;
    }

    aper_size = i830_get_fence_size(pScrn, size);
    if (IS_I965G(pI830)) {
	aper_align = GTT_PAGE_SIZE;
    } else {
	/* The offset has to be aligned to at least the size of the fence
	 * region.
	 */
	aper_align = aper_size;
    }
    if (aper_align < alignment)
	aper_align = alignment;

    fence_divide = 1;
    mem = i830_allocate_aperture(pScrn, name, aper_size, aper_align, flags);
    if (mem == NULL && !IS_I965G(pI830)) {
	/* For the older hardware with stricter fencing limits, if we
	 * couldn't allocate with the large alignment, try relaxing the
	 * alignment requirements and using more fences to cover the area.
	 */
	for (fence_divide = 2; fence_divide <= 4 && mem == NULL;
	     fence_divide *= 2)
	{
	    /* Check that it's not too small for fencing. */
	    if (i830_get_fence_size(pScrn, aper_align / fence_divide) !=
		aper_align / fence_divide)
	    {
		break;
	    }

	    mem = i830_allocate_aperture(pScrn, name, aper_size,
					 aper_align / fence_divide, flags);
	}
    }

    if (mem == NULL)
	return NULL;

    /* Make sure we've got enough free fence regs.  It's pretty hard to run
     * out, luckily, with 8 even on older hardware and us only tiling
     * front/back/depth buffers.
     */
    if (pI830->next_fence + fence_divide >
	(IS_I965G(pI830) ? FENCE_NEW_NR : FENCE_NR))
    {
	i830_free_memory(pScrn, mem);
	return NULL;
    }

    /* Allocate any necessary AGP memory to back this allocation */
    if (!i830_allocate_agp_memory(pScrn, mem, flags)) {
	i830_free_memory(pScrn, mem);
	return NULL;
    }

    /* Set up the fence registers. */
    for (i = 0; i < fence_divide; i++) {
	i830_set_fence(pScrn, pI830->next_fence++,
		       mem->offset + mem->size * i / fence_divide, pitch,
		       mem->size / fence_divide, tile_format);
    }

    mem->size = size;

    return mem;
}

static void
i830_describe_tiling(ScrnInfoPtr pScrn, int verbosity, const char *prefix,
		     i830_memory *mem, unsigned int tiling_mode)
{
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		   "%s%s is %stiled\n", prefix, mem->name,
		   (tiling_mode == FENCE_LINEAR) ? "not " : "");
}

void
i830_describe_allocations(ScrnInfoPtr pScrn, int verbosity, const char *prefix)
{
    I830Ptr pI830 = I830PTR(pScrn);
    i830_memory *mem;

    if (pI830->memory_list == NULL) {
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		       "%sMemory allocator not initialized\n", prefix);
	return;
    }

    if (pI830->memory_list->next->next == NULL) {
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		       "%sNo memory allocations\n", prefix);
	return;
    }

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		   "%sMemory allocation layout:\n", prefix);

    for (mem = pI830->memory_list->next; mem->next != NULL; mem = mem->next) {

	if (mem->offset >= pI830->stolen_size &&
	    mem->prev->offset < pI830->stolen_size)
	{
	    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
			   "%s0x%08lx:            end of stolen memory\n",
			   prefix, pI830->stolen_size);
	}

	if (mem->bus_addr == 0) {
	    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
			   "%s0x%08lx-0x%08lx: %s (%ld kB)\n", prefix,
			   mem->offset, mem->end - 1, mem->name,
			   mem->size / 1024);
	} else {
	    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
			   "%s0x%08lx-0x%08lx: %s "
			   "(%ld kB, 0x%16llx physical)\n",
			   prefix,
			   mem->offset, mem->end - 1, mem->name,
			   mem->size / 1024, mem->bus_addr);
	}
    }
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, verbosity,
		   "%s0x%08lx:            end of aperture\n",
		   prefix, pI830->FbMapSize);

    if (pI830->front_buffer != NULL) {
	i830_describe_tiling(pScrn, verbosity, prefix, pI830->front_buffer,
			     pI830->front_tiled);
    }
#ifdef XF86DRI
    if (pI830->back_buffer != NULL) {
	i830_describe_tiling(pScrn, verbosity, prefix, pI830->back_buffer,
			     pI830->back_tiled);
    }
    if (pI830->third_buffer != NULL) {
	i830_describe_tiling(pScrn, verbosity, prefix, pI830->third_buffer,
			     pI830->third_tiled);
    }
    if (pI830->depth_buffer != NULL) {
	i830_describe_tiling(pScrn, verbosity, prefix, pI830->depth_buffer,
			     pI830->depth_tiled);
    }
#endif
}

static Bool
i830_allocate_ringbuffer(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (pI830->noAccel || pI830->LpRing->mem != NULL)
	return TRUE;

    pI830->LpRing->mem = i830_allocate_memory(pScrn, "ring buffer",
					      PRIMARY_RINGBUFFER_SIZE,
					      GTT_PAGE_SIZE, 0);
    if (pI830->LpRing->mem == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Failed to allocate Ring Buffer space\n");
	return FALSE;
    }

    pI830->LpRing->tail_mask = pI830->LpRing->mem->size - 1;
    return TRUE;
}

#ifdef I830_XV
/**
 * Allocate space for overlay registers and XAA linear allocator (if
 * requested)
 */
static Bool
i830_allocate_overlay(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int flags = NEED_PHYSICAL_ADDR;

    /* Only allocate if overlay is going to be enabled. */
    if (!pI830->XvEnabled)
	return TRUE;

    if (OVERLAY_NOPHYSICAL(pI830))
	flags = 0;

    if (!IS_I965G(pI830)) {
	pI830->overlay_regs = i830_allocate_memory(pScrn, "overlay registers",
						   OVERLAY_SIZE, GTT_PAGE_SIZE,
						   flags);
	if (pI830->overlay_regs == NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Failed to allocate Overlay register space.\n");
	    /* This failure isn't fatal. */
	}
    }

    return TRUE;
}
#endif

static Bool
IsTileable(ScrnInfoPtr pScrn, int pitch)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (IS_I965G(pI830)) {
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
	if (IS_I945G(pI830) || IS_I945GM(pI830) || IS_G33CLASS(pI830))
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

/* This is the 2D rendering vertical coordinate limit.  We can ignore
 * the 3D rendering limits in our 2d pixmap cache allocation, because XAA
 * doesn't do any 3D rendering to/from the cache lines when using an offset
 * at the start of framebuffer.
 */
#define MAX_2D_HEIGHT		65536

/**
 * Allocates a framebuffer for a screen.
 *
 * Used once for each X screen, so once with RandR 1.2 and twice with classic
 * dualhead.
 *
 * \param pScrn ScrnInfoPtr for the screen being allocated
 * \param pI830 I830Ptr for the screen being allocated.
 * \param FbMemBox
 */
static i830_memory *
i830_allocate_framebuffer(ScrnInfoPtr pScrn, I830Ptr pI830, BoxPtr FbMemBox,
			  Bool secondary, int flags)
{
    unsigned int pitch = pScrn->displayWidth * pI830->cpp;
    unsigned long minspace, avail;
    int cacheLines, maxCacheLines;
    int align;
    long size, fb_height;
    char *name;
    i830_memory *front_buffer = NULL;

    /* Clear everything first. */
    memset(FbMemBox, 0, sizeof(*FbMemBox));

    /* We'll allocate the fb such that the root window will fit regardless of
     * rotation.
     */
    if (pScrn->virtualX > pScrn->virtualY)
	fb_height = pScrn->virtualX;
    else
	fb_height = pScrn->virtualY;

    FbMemBox->x1 = 0;
    FbMemBox->x2 = pScrn->displayWidth;
    FbMemBox->y1 = 0;
    FbMemBox->y2 = fb_height;

    /* Calculate how much framebuffer memory to allocate.  For the
     * initial allocation, calculate a reasonable minimum.  This is
     * enough for the virtual screen size, plus some pixmap cache
     * space if we're using XAA.
     */
    minspace = pitch * pScrn->virtualY;
    avail = pScrn->videoRam * 1024;

    if (!pI830->useEXA) {
	maxCacheLines = (avail - minspace) / pitch;
	/* This shouldn't happen. */
	if (maxCacheLines < 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Internal Error: "
		       "maxCacheLines < 0 in i830_allocate_2d_memory()\n");
	    maxCacheLines = 0;
	}
	if (maxCacheLines > (MAX_2D_HEIGHT - pScrn->virtualY))
	    maxCacheLines = MAX_2D_HEIGHT - pScrn->virtualY;

	if (pI830->CacheLines >= 0) {
	    cacheLines = pI830->CacheLines;
	} else {
	    int size;

	    size = 3 * pitch * pScrn->virtualY;
	    size += 1920 * 1088 * 2 * 2;
	    size = ROUND_TO_PAGE(size);

	    cacheLines = (size + pitch - 1) / pitch;
	}
	if (cacheLines > maxCacheLines)
	    cacheLines = maxCacheLines;

	FbMemBox->y2 += cacheLines;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Allocating %d scanlines for pixmap cache\n",
		   cacheLines);
    } else {
	/* For EXA, we have a separate allocation for the linear allocator
	 * which also does the pixmap cache.
	 */
	cacheLines = 0;
    }

    size = pitch * (fb_height + cacheLines);
    size = ROUND_TO_PAGE(size);

    name = secondary ? "secondary front buffer" : "front buffer";

    /* Attempt to allocate it tiled first if we have page flipping on. */
    if (pI830->tiling && IsTileable(pScrn, pitch)) {
	/* XXX: probably not the case on 965 */
	if (IS_I9XX(pI830))
	    align = MB(1);
	else
	    align = KB(512);
	front_buffer = i830_allocate_memory_tiled(pScrn, name, size,
						  pitch, align,
						  0, TILING_XMAJOR);
	pI830->front_tiled = FENCE_XMAJOR;
    }

    /* If not, attempt it linear */
    if (front_buffer == NULL) {
	front_buffer = i830_allocate_memory(pScrn, name, size, KB(64), flags);
	pI830->front_tiled = FENCE_LINEAR;
    }

    if (front_buffer == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to allocate "
		   "%sframebuffer. Is your VideoRAM set too low?\n",
		   secondary ? "secondary " : "");
	return NULL;
    }

    if (pI830->FbBase)
	memset (pI830->FbBase + front_buffer->offset, 0, size);
    return front_buffer;
}

static Bool
i830_allocate_cursor_buffers(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int flags = pI830->CursorNeedsPhysical ? NEED_PHYSICAL_ADDR : 0;
    int i;
    long size;

    /* Try to allocate one big blob for our cursor memory.  This works
     * around a limitation in the FreeBSD AGP driver that allows only one
     * physical allocation larger than a page, and could allos us
     * to pack the cursors smaller.
     */
    size = xf86_config->num_crtc * (HWCURSOR_SIZE + HWCURSOR_SIZE_ARGB);

    pI830->cursor_mem = i830_allocate_memory(pScrn, "HW cursors",
					     size, GTT_PAGE_SIZE,
					     flags);
    if (pI830->cursor_mem != NULL) {
	unsigned long cursor_offset_base = pI830->cursor_mem->offset;
	unsigned long cursor_addr_base, offset = 0;

	if (pI830->CursorNeedsPhysical) {
	    /* On any hardware that requires physical addresses for cursors,
	     * the PTEs don't support memory above 4GB, so we can safely
	     * ignore the top 32 bits of cursor_mem->bus_addr.
	     */
	    cursor_addr_base = (unsigned long)pI830->cursor_mem->bus_addr;
	} else
	    cursor_addr_base = pI830->cursor_mem->offset;

	/* Set up the offsets for our cursors in each CRTC. */
	for (i = 0; i < xf86_config->num_crtc; i++) {
	    xf86CrtcPtr crtc = xf86_config->crtc[i];
	    I830CrtcPrivatePtr intel_crtc = crtc->driver_private;

	    intel_crtc->cursor_argb_addr = cursor_addr_base + offset;
	    intel_crtc->cursor_argb_offset = cursor_offset_base + offset;
	    offset += HWCURSOR_SIZE_ARGB;

	    intel_crtc->cursor_addr = cursor_addr_base + offset;
	    intel_crtc->cursor_offset = cursor_offset_base + offset;
	    offset += HWCURSOR_SIZE;
	}

	return TRUE;
    }

    /*
     * Allocate four separate buffers when the kernel doesn't support
     * large allocations as on Linux. If any of these fail, just
     * bail back to software cursors everywhere
     */
    for (i = 0; i < xf86_config->num_crtc; i++)
    {
	xf86CrtcPtr	    crtc = xf86_config->crtc[i];
	I830CrtcPrivatePtr  intel_crtc = crtc->driver_private;
	
	pI830->cursor_mem_classic[i] = i830_allocate_memory (pScrn, 
							     "Core cursor",
							     HWCURSOR_SIZE,
							     GTT_PAGE_SIZE,
							     flags);
	if (!pI830->cursor_mem_classic[i])
	    return FALSE;
	pI830->cursor_mem_argb[i] = i830_allocate_memory (pScrn, "ARGB cursor",
							  HWCURSOR_SIZE_ARGB,
							  GTT_PAGE_SIZE,
							  flags);
	if (!pI830->cursor_mem_argb[i])
	    return FALSE;

	/*
	 * Set up the pointers into the allocations
	 */
	if (pI830->CursorNeedsPhysical)
	{
	    intel_crtc->cursor_addr = pI830->cursor_mem_classic[i]->bus_addr;
	    intel_crtc->cursor_argb_addr = pI830->cursor_mem_argb[i]->bus_addr;
	}
	else
	{
	    intel_crtc->cursor_addr = pI830->cursor_mem_classic[i]->offset;
	    intel_crtc->cursor_argb_addr = pI830->cursor_mem_argb[i]->offset;
	}
	intel_crtc->cursor_offset = pI830->cursor_mem_classic[i]->offset;
	intel_crtc->cursor_argb_offset = pI830->cursor_mem_argb[i]->offset;
    }
    return TRUE;
}

static void i830_setup_fb_compression(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    /* Only mobile chips since 845 support this feature */
    if (!IS_MOBILE(pI830)) {
	pI830->fb_compression = FALSE;
	goto out;
    }

    /* Clear out any stale state */
    OUTREG(FBC_CFB_BASE, 0);
    OUTREG(FBC_LL_BASE, 0);
    OUTREG(FBC_CONTROL2, 0);
    OUTREG(FBC_CONTROL, 0);

    /*
     * Compressed framebuffer limitations:
     *   - contiguous, physical, uncached memory
     *   - ideally as large as the front buffer(s), smaller sizes cache less
     *   - uncompressed buffer must be tiled w/pitch 2k-16k
     *   - uncompressed fb is <= 2048 in width, 0 mod 8
     *   - uncompressed fb is <= 1536 in height, 0 mod 2
     *   - compressed fb stride is <= uncompressed stride
     *   - SR display watermarks must be equal between 16bpp and 32bpp?
     *   - both compressed and line buffers must be in stolen memory
     */
    pI830->compressed_front_buffer =
	i830_allocate_memory(pScrn, "compressed frame buffer",
			     MB(6), KB(4), NEED_PHYSICAL_ADDR);

    if (!pI830->compressed_front_buffer) {
	pI830->fb_compression = FALSE;
	goto out;
    }

    pI830->compressed_ll_buffer =
	i830_allocate_memory(pScrn, "compressed ll buffer",
			     FBC_LL_SIZE + FBC_LL_PAD, KB(4),
			     NEED_PHYSICAL_ADDR);
    if (!pI830->compressed_ll_buffer) {
	i830_free_memory(pScrn, pI830->compressed_front_buffer);
	pI830->fb_compression = FALSE;
	goto out;
    }

out:
    if (pI830->fb_compression)
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Framebuffer compression "
		   "enabled\n");
    else
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Allocation error, framebuffer"
		   " compression disabled\n");
	
    return;
}
/*
 * Allocate memory for 2D operation.  This includes the (front) framebuffer,
 * ring buffer, scratch memory, HW cursor.
 */
Bool
i830_allocate_2d_memory(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned int pitch = pScrn->displayWidth * pI830->cpp;
    long size;

    if (!pI830->StolenOnly &&
	(!xf86AgpGARTSupported() || !xf86AcquireGART(pScrn->scrnIndex))) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "AGP GART support is either not available or cannot "
		   "be used.\n"
		   "\tMake sure your kernel has agpgart support or has\n"
		   "\tthe agpgart module loaded.\n");
	return FALSE;
    }

    /* Allocate the ring buffer first, so it ends up in stolen mem. */
    i830_allocate_ringbuffer(pScrn);

    if (pI830->fb_compression)
	i830_setup_fb_compression(pScrn);

    /* Next, allocate other fixed-size allocations we have. */
    if (!pI830->SWCursor && !i830_allocate_cursor_buffers(pScrn)) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Disabling HW cursor because the cursor memory "
		   "allocation failed.\n");
	pI830->SWCursor = TRUE;
    }

    /* Space for the X Server's 3D context.  32k is fine for right now. */
    pI830->logical_context = i830_allocate_memory(pScrn, "logical 3D context",
						  KB(32), GTT_PAGE_SIZE, 0);
    if (pI830->logical_context == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Failed to allocate logical context space.\n");
	return FALSE;
    }

    /* even in XAA, 965G needs state mem buffer for rendering */
    if (IS_I965G(pI830) && !pI830->noAccel && pI830->exa_965_state == NULL) {
	pI830->exa_965_state =
	    i830_allocate_memory(pScrn, "exa G965 state buffer",
		    EXA_LINEAR_EXTRA, GTT_PAGE_SIZE, 0);
	if (pI830->exa_965_state == NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		    "Failed to allocate exa state buffer for 965.\n");
	    return FALSE;
	}
    }

#ifdef I830_XV
    /* Allocate overlay register space and optional XAA linear allocator
     * space.  The second head in zaphod mode will share the space.
     */
    if (I830IsPrimary(pScrn))
	i830_allocate_overlay(pScrn);
#endif

    if (pI830->entityPrivate && pI830->entityPrivate->pScrn_2) {
	I830EntPtr pI830Ent = pI830->entityPrivate;
	I830Ptr pI8302 = I830PTR(pI830Ent->pScrn_2);

	pI830->front_buffer_2 =
	    i830_allocate_framebuffer(pI830Ent->pScrn_2, pI8302,
				      &pI830->FbMemBox2, TRUE, 0);
	if (pI830->front_buffer_2 == NULL)
	    return FALSE;
    }
    pI830->front_buffer =
	i830_allocate_framebuffer(pScrn, pI830, &pI830->FbMemBox, FALSE, 0);
    if (pI830->front_buffer == NULL)
	return FALSE;

#ifdef I830_USE_EXA
    if (pI830->useEXA) {
	if (pI830->exa_offscreen == NULL) {
	    /* Default EXA to having 3 screens worth of offscreen memory space
	     * (for pixmaps), plus a double-buffered, 1920x1088 video's worth.
	     *
	     * XXX: It would be nice to auto-size it larger if the user
	     * specified a larger size, or to fit along with texture and FB
	     * memory if a low videoRam is specified.
	     */
	    size = 3 * pitch * pScrn->virtualY;
	    size += 1920 * 1088 * 2 * 2;
	    size = ROUND_TO_PAGE(size);

	    pI830->exa_offscreen = i830_allocate_memory(pScrn, "exa offscreen",
							size, 1, 0);
	    if (pI830->exa_offscreen == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Failed to allocate EXA offscreen memory.");
		return FALSE;
	    }
	}
    }
#endif /* I830_USE_EXA */

    if (!pI830->noAccel && !pI830->useEXA) {
	pI830->xaa_scratch =
	    i830_allocate_memory(pScrn, "xaa scratch", MAX_SCRATCH_BUFFER_SIZE,
				 GTT_PAGE_SIZE, 0);
	if (pI830->xaa_scratch == NULL) {
	    pI830->xaa_scratch =
		i830_allocate_memory(pScrn, "xaa scratch",
				     MIN_SCRATCH_BUFFER_SIZE, GTT_PAGE_SIZE,
				     0);
	    if (pI830->xaa_scratch == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Failed to allocate scratch buffer space\n");
		return FALSE;
	    }
	}

	/* Let's allocate another scratch buffer for the second head */
	/* Again, this code won't execute on the dry run pass */
	if (pI830->entityPrivate && pI830->entityPrivate->pScrn_2)
	{
	    pI830->xaa_scratch_2 =
		i830_allocate_memory(pScrn, "xaa scratch 2",
				     MAX_SCRATCH_BUFFER_SIZE, GTT_PAGE_SIZE,
				     0);
	    if (pI830->xaa_scratch_2 == NULL) {
		pI830->xaa_scratch_2 =
		    i830_allocate_memory(pScrn, "xaa scratch 2",
					 MIN_SCRATCH_BUFFER_SIZE,
					 GTT_PAGE_SIZE, 0);
		if (pI830->xaa_scratch_2 == NULL) {
		    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			       "Failed to allocate secondary scratch "
			       "buffer space\n");
		    return FALSE;
		}
	    }
	}
    }

    return TRUE;
}

#ifdef XF86DRI
static unsigned int
myLog2(unsigned int n)
{
    unsigned int log2 = 1;

    while (n > 1) {
	n >>= 1;
	log2++;
    }
    return log2;
}

static Bool
i830_allocate_backbuffer(ScrnInfoPtr pScrn, i830_memory **buffer,
			 unsigned int *tiled, const char *name)
{
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned int pitch = pScrn->displayWidth * pI830->cpp;
    unsigned long size;
    int height;

    if (pI830->rotation & (RR_Rotate_0 | RR_Rotate_180))
	height = pScrn->virtualY;
    else
	height = pScrn->virtualX;

    /* Try to allocate on the best tile-friendly boundaries. */
    if (pI830->tiling && IsTileable(pScrn, pitch))
    {
	size = ROUND_TO_PAGE(pitch * ALIGN(height, 16));
	*buffer = i830_allocate_memory_tiled(pScrn, name, size, pitch,
					     GTT_PAGE_SIZE, ALIGN_BOTH_ENDS,
					     TILING_XMAJOR);
	*tiled = FENCE_XMAJOR;
    }

    /* Otherwise, just allocate it linear */
    if (*buffer == NULL) {
	size = ROUND_TO_PAGE(pitch * height);
	*buffer = i830_allocate_memory(pScrn, name, size, GTT_PAGE_SIZE,
				       ALIGN_BOTH_ENDS);
	*tiled = FENCE_LINEAR;
    }

    if (*buffer == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Failed to allocate %s space.\n", name);
	return FALSE;
    }

    return TRUE;
}

static Bool
i830_allocate_depthbuffer(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long size;
    unsigned int pitch = pScrn->displayWidth * pI830->cpp;
    int height;

    /* XXX: this rotation stuff is bogus */
    if (pI830->rotation & (RR_Rotate_0 | RR_Rotate_180))
	height = pScrn->virtualY;
    else
	height = pScrn->virtualX;

    /* First try allocating it tiled */
    if (pI830->tiling && IsTileable(pScrn, pitch))
    {
	enum tile_format tile_format;

	size = ROUND_TO_PAGE(pitch * ALIGN(height, 16));

	/* The 965 requires that the depth buffer be in Y Major format, while
	 * the rest appear to fail when handed that format.
	 */
	tile_format = IS_I965G(pI830) ? TILING_YMAJOR: TILING_XMAJOR;

	pI830->depth_buffer =
	    i830_allocate_memory_tiled(pScrn, "depth buffer", size, pitch,
				       GTT_PAGE_SIZE, ALIGN_BOTH_ENDS,
				       tile_format);
	pI830->depth_tiled = (tile_format == TILING_YMAJOR) ? FENCE_YMAJOR :
	    FENCE_XMAJOR;
    }

    /* Otherwise, allocate it linear. */
    if (pI830->depth_buffer == NULL) {
	size = ROUND_TO_PAGE(pitch * height);
	pI830->depth_buffer =
	    i830_allocate_memory(pScrn, "depth buffer", size, GTT_PAGE_SIZE,
				 0);
	pI830->depth_tiled = FENCE_LINEAR;
    }

    if (pI830->depth_buffer == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Failed to allocate depth buffer space.\n");
	return FALSE;
    }

    return TRUE;
}

Bool
i830_allocate_texture_memory(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long size;
    int i;

    if (pI830->mmModeFlags & I830_KERNEL_MM) {
	pI830->memory_manager =
	    i830_allocate_aperture(pScrn, "DRI memory manager",
				   pI830->mmSize * KB(1), GTT_PAGE_SIZE,
				   ALIGN_BOTH_ENDS);
	/* XXX: try memory manager size backoff here? */
	if (pI830->memory_manager == NULL)
	    return FALSE;
    }

    if (pI830->mmModeFlags & I830_KERNEL_TEX) {
	/* XXX: auto-sizing */
	size = MB(32);
	i = myLog2(size / I830_NR_TEX_REGIONS);
	if (i < I830_LOG_MIN_TEX_REGION_SIZE)
	    i = I830_LOG_MIN_TEX_REGION_SIZE;
	pI830->TexGranularity = i;
	/* Truncate size */
	size >>= i;
	size <<= i;
	if (size < KB(512)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Less than 512 kBytes for texture space (real %ld"
		       "kBytes).\n",
		       size / 1024);
	    return FALSE;
	}
	pI830->textures = i830_allocate_memory(pScrn, "textures", size,
					       GTT_PAGE_SIZE, 0);
	if (pI830->textures == NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Failed to allocate texture space.\n");
	    return FALSE;
	}
    }

    return TRUE;
}

static Bool
i830_allocate_hwstatus(ScrnInfoPtr pScrn)
{
#define HWSTATUS_PAGE_SIZE (4*1024)
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->hw_status = i830_allocate_memory(pScrn, "G33 hw status",
	    HWSTATUS_PAGE_SIZE, GTT_PAGE_SIZE, 0);
    if (pI830->hw_status == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		"Failed to allocate hw status page for G33.\n");
	return FALSE;
    }
    return TRUE;
}

Bool
i830_allocate_3d_memory(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    DPRINTF(PFX, "i830_allocate_3d_memory\n");

    if (IS_G33CLASS(pI830)) {
	if (!i830_allocate_hwstatus(pScrn))
	    return FALSE;
    }

    if (!i830_allocate_backbuffer(pScrn, &pI830->back_buffer,
				  &pI830->back_tiled, "back buffer"))
	return FALSE;

    if (pI830->TripleBuffer && !i830_allocate_backbuffer(pScrn,
							 &pI830->third_buffer,
							 &pI830->third_tiled,
							 "third buffer")) {
       xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  "Failed to allocate third buffer, triple buffering "
		  "inactive\n");
    }

    if (!i830_allocate_depthbuffer(pScrn))
	return FALSE;

    if (!i830_allocate_texture_memory(pScrn))
	return FALSE;

    return TRUE;
}
#endif

/**
 * Sets up a fence area for the hardware.
 *
 * The fences control automatic tiled address swizzling for CPU access of the
 * framebuffer.
 */
static void
i830_set_fence(ScrnInfoPtr pScrn, int nr, unsigned int offset,
	       unsigned int pitch, unsigned int size,
	       enum tile_format tile_format)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 val;
    CARD32 fence_mask = 0;
    unsigned int fence_pitch;

    DPRINTF(PFX, "i830_set_fence(): %d, 0x%08x, %d, %d kByte\n",
	    nr, offset, pitch, size / 1024);

    assert(tile_format != TILING_NONE);

    if (IS_I965G(pI830)) {
	if (nr < 0 || nr >= FENCE_NEW_NR) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "i830_set_fence(): fence %d out of range\n",nr);
	    return;
	}

	switch (tile_format) {
	case TILING_XMAJOR:
            pI830->fence[nr] = (((pitch / 128) - 1) << 2) | offset | 1;
	    pI830->fence[nr] |= I965_FENCE_X_MAJOR;
            break;
	case TILING_YMAJOR:
            /* YMajor can be 128B aligned but the current code dictates
             * otherwise. This isn't a problem apart from memory waste.
             * FIXME */
            pI830->fence[nr] = (((pitch / 128) - 1) << 2) | offset | 1;
	    pI830->fence[nr] |= I965_FENCE_Y_MAJOR;
            break;
	case TILING_NONE:
            break;
	}

	/* The end marker is the address of the last page in the allocation. */
	pI830->fence[FENCE_NEW_NR + nr] = offset + size - 4096;
	return;
    }

    if (nr < 0 || nr >= FENCE_NR) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "i830_set_fence(): fence %d out of range\n",nr);
	return;
    }

    pI830->fence[nr] = 0;

    if (IS_I9XX(pI830))
   	fence_mask = ~I915G_FENCE_START_MASK;
    else
   	fence_mask = ~I830_FENCE_START_MASK;

    if (offset & fence_mask) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "i830_set_fence(): %d: offset (0x%08x) is not %s aligned\n",
		   nr, offset, (IS_I9XX(pI830)) ? "1MB" : "512k");
	return;
    }

    if (offset % size) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "i830_set_fence(): %d: offset (0x%08x) is not size (%dk) "
		   "aligned\n",
		   nr, offset, size / 1024);
	return;
    }

    if (pitch & 127) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "i830_set_fence(): %d: pitch (%d) not a multiple of 128 "
		   "bytes\n",
		   nr, pitch);
	return;
    }

    val = offset | FENCE_VALID;

    switch (tile_format) {
    case TILING_XMAJOR:
	val |= FENCE_X_MAJOR;
	break;
    case TILING_YMAJOR:
	val |= FENCE_Y_MAJOR;
	break;
    case TILING_NONE:
	break;
    }

    if (IS_I9XX(pI830)) {
   	switch (size) {
	case MB(1):
	    val |= I915G_FENCE_SIZE_1M;
	    break;
	case MB(2):
	    val |= I915G_FENCE_SIZE_2M;
	    break;
	case MB(4):
	    val |= I915G_FENCE_SIZE_4M;
	    break;
	case MB(8):
	    val |= I915G_FENCE_SIZE_8M;
	    break;
	case MB(16):
	    val |= I915G_FENCE_SIZE_16M;
	    break;
	case MB(32):
	    val |= I915G_FENCE_SIZE_32M;
	    break;
	case MB(64):
	    val |= I915G_FENCE_SIZE_64M;
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "i830_set_fence(): %d: illegal size (%d kByte)\n",
		       nr, size / 1024);
	    return;
   	}
    } else {
   	switch (size) {
	case KB(512):
	    val |= FENCE_SIZE_512K;
	    break;
	case MB(1):
	    val |= FENCE_SIZE_1M;
	    break;
	case MB(2):
	    val |= FENCE_SIZE_2M;
	    break;
	case MB(4):
	    val |= FENCE_SIZE_4M;
	    break;
	case MB(8):
	    val |= FENCE_SIZE_8M;
	    break;
	case MB(16):
	    val |= FENCE_SIZE_16M;
	    break;
	case MB(32):
	    val |= FENCE_SIZE_32M;
	    break;
	case MB(64):
	    val |= FENCE_SIZE_64M;
	    break;
	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "i830_set_fence(): %d: illegal size (%d kByte)\n",
		       nr, size / 1024);
	    return;
   	}
    }

    if ((IS_I945G(pI830) || IS_I945GM(pI830) || IS_G33CLASS(pI830))
	    && tile_format == TILING_YMAJOR)
	fence_pitch = pitch / 128;
    else if (IS_I9XX(pI830))
	fence_pitch = pitch / 512;
    else
	fence_pitch = pitch / 128;

    switch (fence_pitch) {
    case 1:
	val |= FENCE_PITCH_1;
	break;
    case 2:
	val |= FENCE_PITCH_2;
	break;
    case 4:
	val |= FENCE_PITCH_4;
	break;
    case 8:
	val |= FENCE_PITCH_8;
	break;
    case 16:
	val |= FENCE_PITCH_16;
	break;
    case 32:
	val |= FENCE_PITCH_32;
	break;
    case 64:
	val |= FENCE_PITCH_64;
	break;
    default:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "i830_set_fence(): %d: illegal pitch (%d)\n", nr, pitch);
	return;
    }

    pI830->fence[nr] = val;
}

/**
 * Called at EnterVT to grab the AGP GART and bind our allocations.
 *
 * In zaphod mode, this will walk the list trying to bind twice, since each
 * pI830 points to the same allocation list, but the bind_memory will just
 * no-op then.
 */
Bool
i830_bind_all_memory(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (pI830->StolenOnly == TRUE || pI830->memory_list == NULL)
	return TRUE;

    if (xf86AgpGARTSupported() && !pI830->gtt_acquired) {
	i830_memory *mem;

	if (!xf86AcquireGART(pScrn->scrnIndex))
	    return FALSE;

	pI830->gtt_acquired = TRUE;

	for (mem = pI830->memory_list->next; mem->next != NULL;
	     mem = mem->next)
	{
	    if (!i830_bind_memory(pScrn, mem)) {
		/* This shouldn't happen */
		FatalError("Couldn't bind memory for %s\n", mem->name);
	    }
	}
    }

    return TRUE;
}

/** Called at LeaveVT, to unbind all of our AGP allocations. */
Bool
i830_unbind_all_memory(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (pI830->StolenOnly == TRUE)
	return TRUE;

    if (xf86AgpGARTSupported() && pI830->gtt_acquired) {
	i830_memory *mem;

	for (mem = pI830->memory_list->next; mem->next != NULL;
	     mem = mem->next)
	{
	    i830_unbind_memory(pScrn, mem);
	}

	pI830->gtt_acquired = FALSE;

	if (!xf86ReleaseGART(pScrn->scrnIndex))
	    return FALSE;
    }

    return TRUE;
}

/**
 * Returns the amount of system memory that could potentially be allocated
 * from AGP, in kB.
 */
long
I830CheckAvailableMemory(ScrnInfoPtr pScrn)
{
    AgpInfoPtr agpinf;
    int maxPages;

    if (!xf86AgpGARTSupported() ||
	!xf86AcquireGART(pScrn->scrnIndex) ||
	(agpinf = xf86GetAGPInfo(pScrn->scrnIndex)) == NULL ||
	!xf86ReleaseGART(pScrn->scrnIndex))
	return -1;

    maxPages = agpinf->totalPages - agpinf->usedPages;
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 2, "%s: %d kB available\n",
		   "I830CheckAvailableMemory", maxPages * 4);

    return maxPages * 4;
}

#ifdef I830_USE_XAA
/**
 * Allocates memory from the XF86 linear allocator, but also purges
 * memory if possible to cause the allocation to succeed.
 */
FBLinearPtr
i830_xf86AllocateOffscreenLinear(ScreenPtr pScreen, int length,
				 int granularity,
				 MoveLinearCallbackProcPtr moveCB,
				 RemoveLinearCallbackProcPtr removeCB,
				 pointer privData)
{
    FBLinearPtr linear;
    int max_size;

    linear = xf86AllocateOffscreenLinear(pScreen, length, granularity, moveCB,
					 removeCB, privData);
    if (linear != NULL)
	return linear;

    /* The above allocation didn't succeed, so purge unlocked stuff and try
     * again.
     */
    xf86QueryLargestOffscreenLinear(pScreen, &max_size, granularity,
				    PRIORITY_EXTREME);

    if (max_size < length)
	return NULL;

    xf86PurgeUnlockedOffscreenAreas(pScreen);

    linear = xf86AllocateOffscreenLinear(pScreen, length, granularity, moveCB,
					 removeCB, privData);

    return linear;
}
#endif
