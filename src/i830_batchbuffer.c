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

static void intel_next_batch(ScrnInfoPtr pScrn)
{
	intel_screen_private *intel = intel_get_screen_private(pScrn);

	/* The 865 has issues with larger-than-page-sized batch buffers. */
	if (IS_I865G(intel))
		intel->batch_bo =
		    dri_bo_alloc(intel->bufmgr, "batch", 4096, 4096);
	else
		intel->batch_bo =
		    dri_bo_alloc(intel->bufmgr, "batch", 4096 * 4, 4096);

	if (dri_bo_map(intel->batch_bo, 1) != 0)
		FatalError("Failed to map batchbuffer: %s\n", strerror(errno));

	intel->batch_used = 0;
	intel->batch_ptr = intel->batch_bo->virtual;

	/* If we are using DRI2, we don't know when another client has executed,
	 * so we have to reinitialize our 3D state per batch.
	 */
	if (intel->directRenderingType == DRI_DRI2)
		intel->last_3d = LAST_3D_OTHER;
}

void intel_batch_init(ScrnInfoPtr pScrn)
{
	intel_screen_private *intel = intel_get_screen_private(pScrn);

	intel->batch_emit_start = 0;
	intel->batch_emitting = 0;

	intel_next_batch(pScrn);
}

void intel_batch_teardown(ScrnInfoPtr pScrn)
{
	intel_screen_private *intel = intel_get_screen_private(pScrn);

	if (intel->batch_ptr != NULL) {
		dri_bo_unmap(intel->batch_bo);
		intel->batch_ptr = NULL;

		dri_bo_unreference(intel->batch_bo);
		intel->batch_bo = NULL;

		dri_bo_unreference(intel->last_batch_bo);
		intel->last_batch_bo = NULL;
	}
}

void intel_batch_flush(ScrnInfoPtr pScrn, Bool flushed)
{
	intel_screen_private *intel = intel_get_screen_private(pScrn);
	int ret;

	if (intel->batch_used == 0)
		return;

	/* Emit a padding dword if we aren't going to be quad-word aligned. */
	if ((intel->batch_used & 4) == 0) {
		*(uint32_t *) (intel->batch_ptr + intel->batch_used) = MI_NOOP;
		intel->batch_used += 4;
	}

	/* Mark the end of the batchbuffer. */
	*(uint32_t *) (intel->batch_ptr + intel->batch_used) =
	    MI_BATCH_BUFFER_END;
	intel->batch_used += 4;

	dri_bo_unmap(intel->batch_bo);
	intel->batch_ptr = NULL;

	ret =
	    dri_bo_exec(intel->batch_bo, intel->batch_used, NULL, 0,
			0xffffffff);
	if (ret != 0)
		FatalError("Failed to submit batchbuffer: %s\n",
			   strerror(-ret));

	/* Save a ref to the last batch emitted, which we use for syncing
	 * in debug code.
	 */
	dri_bo_unreference(intel->last_batch_bo);
	intel->last_batch_bo = intel->batch_bo;
	intel->batch_bo = NULL;

	intel_next_batch(pScrn);

	/* Mark that we need to flush whatever potential rendering we've done in the
	 * blockhandler.  We could set this less often, but it's probably not worth
	 * the work.
	 */
	intel->need_mi_flush = TRUE;

	if (intel->batch_flush_notify)
		intel->batch_flush_notify(pScrn);
}

/** Waits on the last emitted batchbuffer to be completed. */
void intel_batch_wait_last(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	/* Map it CPU write, which guarantees it's done.  This is a completely
	 * non performance path, so we don't need anything better.
	 */
	drm_intel_bo_map(intel->last_batch_bo, TRUE);
	drm_intel_bo_unmap(intel->last_batch_bo);
}
