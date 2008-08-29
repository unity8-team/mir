/* -*- c-basic-offset: 4 -*- */
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
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#include "xf86.h"
#include "i830.h"
#include "i830_ring.h"
#include "i915_drm.h"

static void
intel_next_batch(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    /* The 865 has issues with larger-than-page-sized batch buffers. */
    if (IS_I865G(pI830))
	pI830->batch_bo = dri_bo_alloc(pI830->bufmgr, "batch", 4096, 4096);
    else
	pI830->batch_bo = dri_bo_alloc(pI830->bufmgr, "batch", 4096 * 4, 4096);

    dri_bo_map(pI830->batch_bo, 1);
    pI830->batch_used = 0;
    pI830->batch_ptr = pI830->batch_bo->virtual;
}

void
intel_batch_init(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->batch_emit_start = 0;
    pI830->batch_emitting = 0;

    intel_next_batch(pScrn);
}

void
intel_batch_teardown(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (pI830->batch_ptr != NULL) {
	dri_bo_unmap(pI830->batch_bo);
	pI830->batch_ptr = NULL;
    }
}

void
intel_batch_flush(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (pI830->batch_used == 0)
	return;

    /* Emit a padding dword if we aren't going to be quad-word aligned. */
    if ((pI830->batch_used & 4) == 0) {
	*(uint32_t *)(pI830->batch_ptr + pI830->batch_used) = MI_NOOP;
	pI830->batch_used += 4;
    }

    /* Mark the end of the batchbuffer. */
    *(uint32_t *)(pI830->batch_ptr + pI830->batch_used) = MI_BATCH_BUFFER_END;
    pI830->batch_used += 4;

    dri_bo_unmap(pI830->batch_bo);
    pI830->batch_ptr = NULL;

    if (pI830->memory_manager) {
	struct drm_i915_gem_execbuffer *exec;
	int ret;

	exec = dri_process_relocs(pI830->batch_bo);

	exec->batch_start_offset = 0;
	exec->batch_len = pI830->batch_used;
	exec->cliprects_ptr = 0;
	exec->num_cliprects = 0;
	exec->DR1 = 0;
	exec->DR4 = 0xffffffff;

	do {
	    ret = drmCommandWriteRead(pI830->drmSubFD, DRM_I915_GEM_EXECBUFFER,
				      exec, sizeof(*exec));
	} while (ret == -EINTR);
	if (ret != 0)
	    FatalError("Failed to submit batchbuffer: %s\n", strerror(errno));
    } else {
	dri_process_relocs(pI830->batch_bo);

	if (pI830->directRenderingEnabled) {
	    drm_i915_batchbuffer_t batch;
	    int ret;

	    batch.start = pI830->batch_bo->offset;
	    batch.used = pI830->batch_used;
	    batch.cliprects = NULL;
	    batch.num_cliprects = 0;
	    batch.DR1 = 0;
	    batch.DR4 = 0xffffffff;

	    ret = drmCommandWrite(pI830->drmSubFD, DRM_I915_BATCHBUFFER,
				  &batch, sizeof(batch));
	    if (ret != 0)
		FatalError("Failed to submit batchbuffer: %s\n", strerror(errno));

	    i830_refresh_ring(pScrn);
	} else {
	    if (!IS_I830(pI830) && !IS_845G(pI830)) {
		BEGIN_LP_RING(2);
		OUT_RING(MI_BATCH_BUFFER_START | (2 << 6));
		OUT_RING(pI830->batch_bo->offset);
		ADVANCE_LP_RING();
	    } else {
		BEGIN_LP_RING(4);
		OUT_RING(MI_BATCH_BUFFER);
		OUT_RING(pI830->batch_bo->offset);
		OUT_RING(pI830->batch_bo->offset + pI830->batch_used - 4);
		OUT_RING(MI_NOOP);
		ADVANCE_LP_RING();
	    }
	}
    }

    dri_post_submit(pI830->batch_bo);

    dri_bo_unreference(pI830->batch_bo);
    intel_next_batch(pScrn);

    /* Mark that we need to flush whatever potential rendering we've done in the
     * blockhandler.  We could set this less often, but it's probably not worth
     * the work.
     */
    pI830->need_mi_flush = TRUE;
}
