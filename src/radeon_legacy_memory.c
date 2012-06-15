
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* Driver data structures */
#include "radeon.h"

/* Allocates memory, either by resizing the allocation pointed to by mem_struct,
 * or by freeing mem_struct (if non-NULL) and allocating a new space.  The size
 * is measured in bytes, and the offset from the beginning of card space is
 * returned.
 */
uint32_t
radeon_legacy_allocate_memory(ScrnInfoPtr pScrn,
		       void **mem_struct,
		       int size,
		       int align,
		       int domain)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct radeon_bo *video_bo;

    if (*mem_struct)
        radeon_legacy_free_memory(pScrn, *mem_struct);

    video_bo = radeon_bo_open(info->bufmgr, 0, size, align, domain, 0);

    *mem_struct = video_bo;

    if (!video_bo)
        return 0;

    return (uint32_t)-1;
}

void
radeon_legacy_free_memory(ScrnInfoPtr pScrn,
			  void *mem_struct)
{
    struct radeon_bo *bo = mem_struct;
    radeon_bo_unref(bo);
    return;
}
