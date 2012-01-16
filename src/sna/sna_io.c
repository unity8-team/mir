/*
 * Copyright (c) 2011 Intel Corporation
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
 *    Chris Wilson <chris@chris-wilson.co.uk>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sna.h"
#include "sna_render.h"
#include "sna_reg.h"

#include <sys/mman.h>

#if DEBUG_IO
#undef DBG
#define DBG(x) ErrorF x
#endif

#define PITCH(x, y) ALIGN((x)*(y), 4)

/* XXX Need to avoid using GTT fenced access for I915_TILING_Y on 855GM */

static void read_boxes_inplace(struct kgem *kgem,
			       struct kgem_bo *bo, int16_t src_dx, int16_t src_dy,
			       PixmapPtr pixmap, int16_t dst_dx, int16_t dst_dy,
			       const BoxRec *box, int n)
{
	int bpp = pixmap->drawable.bitsPerPixel;
	void *src, *dst = pixmap->devPrivate.ptr;
	int src_pitch = bo->pitch;
	int dst_pitch = pixmap->devKind;

	DBG(("%s x %d, tiling=%d\n", __FUNCTION__, n, bo->tiling));

	kgem_bo_submit(kgem, bo);

	src = kgem_bo_map(kgem, bo);
	if (src == NULL)
		return;

	do {
		assert(box->x1 + src_dx >= 0);
		assert(box->y1 + src_dy >= 0);
		assert(box->x2 + src_dx <= pixmap->drawable.width);
		assert(box->y2 + src_dy <= pixmap->drawable.height);

		assert(box->x1 + dst_dx >= 0);
		assert(box->y1 + dst_dy >= 0);
		assert(box->x2 + dst_dx <= pixmap->drawable.width);
		assert(box->y2 + dst_dy <= pixmap->drawable.height);

		memcpy_blt(src, dst, bpp,
			   src_pitch, dst_pitch,
			   box->x1 + src_dx, box->y1 + src_dy,
			   box->x1 + dst_dx, box->y1 + dst_dy,
			   box->x2 - box->x1, box->y2 - box->y1);
		box++;
	} while (--n);
}

void sna_read_boxes(struct sna *sna,
		    struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
		    PixmapPtr dst, int16_t dst_dx, int16_t dst_dy,
		    const BoxRec *box, int nbox)
{
	struct kgem *kgem = &sna->kgem;
	struct kgem_bo *dst_bo;
	int tmp_nbox;
	const BoxRec *tmp_box;
	char *src;
	void *ptr;
	int src_pitch, cpp, offset;
	int n, cmd, br13;

	DBG(("%s x %d, src=(handle=%d, offset=(%d,%d)), dst=(size=(%d, %d), offset=(%d,%d))\n",
	     __FUNCTION__, nbox, src_bo->handle, src_dx, src_dy,
	     dst->drawable.width, dst->drawable.height, dst_dx, dst_dy));

#ifndef NDEBUG
	for (n = 0; n < nbox; n++) {
		if (box[n].x1 + src_dx < 0 || box[n].y1 + src_dy < 0 ||
		    (box[n].x2 + src_dx) * dst->drawable.bitsPerPixel/8 > src_bo->pitch ||
		    (box[n].y2 + src_dy) * src_bo->pitch > src_bo->size)
		{
			FatalError("source out-of-bounds box[%d]=(%d, %d), (%d, %d) + (%d, %d), pitch=%d, size=%d\n", n,
				   box[n].x1, box[n].y1,
				   box[n].x2, box[n].y2,
				   src_dx, src_dy,
				   src_bo->pitch, src_bo->size);
		}
	}
#endif

	/* XXX The gpu is faster to perform detiling in bulk, but takes
	 * longer to setup and retrieve the results, with an additional
	 * copy. The long term solution is to use snoopable bo and avoid
	 * this path.
	 */

	if (DEBUG_NO_IO || kgem->wedged ||
	    !kgem_bo_map_will_stall(kgem, src_bo) ||
	    src_bo->tiling != I915_TILING_X) {
		read_boxes_inplace(kgem,
				   src_bo, src_dx, src_dy,
				   dst, dst_dx, dst_dy,
				   box, nbox);
		return;
	}

	/* count the total number of bytes to be read and allocate a bo */
	cpp = dst->drawable.bitsPerPixel / 8;
	offset = 0;
	for (n = 0; n < nbox; n++) {
		int height = box[n].y2 - box[n].y1;
		int width = box[n].x2 - box[n].x1;
		offset += PITCH(width, cpp) * height;
	}

	DBG(("    read buffer size=%d\n", offset));

	dst_bo = kgem_create_buffer(kgem, offset, KGEM_BUFFER_LAST, &ptr);
	if (!dst_bo) {
		read_boxes_inplace(kgem,
				   src_bo, src_dx, src_dy,
				   dst, dst_dx, dst_dy,
				   box, nbox);
		return;
	}

	cmd = XY_SRC_COPY_BLT_CMD;
	src_pitch = src_bo->pitch;
	if (kgem->gen >= 40 && src_bo->tiling) {
		cmd |= BLT_SRC_TILED;
		src_pitch >>= 2;
	}

	br13 = 0xcc << 16;
	switch (cpp) {
	default:
	case 4: cmd |= BLT_WRITE_ALPHA | BLT_WRITE_RGB;
		br13 |= 1 << 25; /* RGB8888 */
	case 2: br13 |= 1 << 24; /* RGB565 */
	case 1: break;
	}

	kgem_set_mode(kgem, KGEM_BLT);
	if (kgem->nexec + 2 > KGEM_EXEC_SIZE(kgem) ||
	    kgem->nreloc + 2 > KGEM_RELOC_SIZE(kgem) ||
	    !kgem_check_batch(kgem, 8) ||
	    !kgem_check_bo_fenced(kgem, dst_bo, src_bo, NULL)) {
		_kgem_submit(kgem);
		_kgem_set_mode(kgem, KGEM_BLT);
	}

	tmp_nbox = nbox;
	tmp_box = box;
	offset = 0;
	do {
		int nbox_this_time;

		nbox_this_time = tmp_nbox;
		if (8*nbox_this_time > kgem->surface - kgem->nbatch - KGEM_BATCH_RESERVED)
			nbox_this_time = (kgem->surface - kgem->nbatch - KGEM_BATCH_RESERVED) / 8;
		if (2*nbox_this_time > KGEM_RELOC_SIZE(kgem) - kgem->nreloc)
			nbox_this_time = (KGEM_RELOC_SIZE(kgem) - kgem->nreloc) / 2;
		assert(nbox_this_time);
		tmp_nbox -= nbox_this_time;

		for (n = 0; n < nbox_this_time; n++) {
			int height = tmp_box[n].y2 - tmp_box[n].y1;
			int width = tmp_box[n].x2 - tmp_box[n].x1;
			int pitch = PITCH(width, cpp);
			uint32_t *b = kgem->batch + kgem->nbatch;

			DBG(("    blt offset %x: (%d, %d) x (%d, %d), pitch=%d\n",
			     offset,
			     tmp_box[n].x1 + src_dx,
			     tmp_box[n].y1 + src_dy,
			     width, height, pitch));

			assert(tmp_box[n].x1 + src_dx >= 0);
			assert((tmp_box[n].x2 + src_dx) * dst->drawable.bitsPerPixel/8 <= src_bo->pitch);
			assert(tmp_box[n].y1 + src_dy >= 0);
			assert((tmp_box[n].y2 + src_dy) * src_bo->pitch <= src_bo->size);

			b[0] = cmd;
			b[1] = br13 | pitch;
			b[2] = 0;
			b[3] = height << 16 | width;
			b[4] = kgem_add_reloc(kgem, kgem->nbatch + 4, dst_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      offset);
			b[5] = (tmp_box[n].y1 + src_dy) << 16 | (tmp_box[n].x1 + src_dx);
			b[6] = src_pitch;
			b[7] = kgem_add_reloc(kgem, kgem->nbatch + 7, src_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      KGEM_RELOC_FENCED,
					      0);
			kgem->nbatch += 8;

			offset += pitch * height;
		}

		_kgem_submit(kgem);
		if (!tmp_nbox)
			break;

		_kgem_set_mode(kgem, KGEM_BLT);
		tmp_box += nbox_this_time;
	} while (1);
	assert(offset == dst_bo->size);

	kgem_buffer_read_sync(kgem, dst_bo);

	src = ptr;
	do {
		int height = box->y2 - box->y1;
		int width  = box->x2 - box->x1;
		int pitch = PITCH(width, cpp);

		DBG(("    copy offset %lx [%08x...%08x]: (%d, %d) x (%d, %d), src pitch=%d, dst pitch=%d, bpp=%d\n",
		     (long)((char *)src - (char *)ptr),
		     *(uint32_t*)src, *(uint32_t*)(src+pitch*height - 4),
		     box->x1 + dst_dx,
		     box->y1 + dst_dy,
		     width, height,
		     pitch, dst->devKind, cpp*8));

		assert(box->x1 + dst_dx >= 0);
		assert(box->x2 + dst_dx <= dst->drawable.width);
		assert(box->y1 + dst_dy >= 0);
		assert(box->y2 + dst_dy <= dst->drawable.height);

		memcpy_blt(src, dst->devPrivate.ptr, cpp*8,
			   pitch, dst->devKind,
			   0, 0,
			   box->x1 + dst_dx, box->y1 + dst_dy,
			   width, height);
		box++;

		src += pitch * height;
	} while (--nbox);
	assert(src - (char *)ptr == dst_bo->size);
	kgem_bo_destroy(kgem, dst_bo);
	sna->blt_state.fill_bo = 0;
}

static void write_boxes_inplace(struct kgem *kgem,
				const void *src, int stride, int bpp, int16_t src_dx, int16_t src_dy,
				struct kgem_bo *bo, int16_t dst_dx, int16_t dst_dy,
				const BoxRec *box, int n)
{
	int dst_pitch = bo->pitch;
	int src_pitch = stride;
	void *dst;

	DBG(("%s x %d, tiling=%d\n", __FUNCTION__, n, bo->tiling));

	kgem_bo_submit(kgem, bo);

	dst = kgem_bo_map(kgem, bo);
	if (dst == NULL)
		return;

	do {
		DBG(("%s: (%d, %d) -> (%d, %d) x (%d, %d) [bpp=%d, src_pitch=%d, dst_pitch=%d]\n", __FUNCTION__,
		     box->x1 + src_dx, box->y1 + src_dy,
		     box->x1 + dst_dx, box->y1 + dst_dy,
		     box->x2 - box->x1, box->y2 - box->y1,
		     bpp, src_pitch, dst_pitch));

		memcpy_blt(src, dst, bpp,
			   src_pitch, dst_pitch,
			   box->x1 + src_dx, box->y1 + src_dy,
			   box->x1 + dst_dx, box->y1 + dst_dy,
			   box->x2 - box->x1, box->y2 - box->y1);
		box++;
	} while (--n);
}

static bool upload_inplace(struct kgem *kgem,
			   struct kgem_bo *bo,
			   const BoxRec *box,
			   int n, int bpp)
{
	if (DEBUG_NO_IO)
		return true;

	if (unlikely(kgem->wedged))
		return true;

	/* If we are writing through the GTT, check first if we might be
	 * able to almagamate a series of small writes into a single
	 * operation.
	 */
	if (!bo->map) {
		BoxRec extents;

		extents = box[0];
		while (--n) {
			box++;
			if (box->x1 < extents.x1)
				extents.x1 = box->x1;
			if (box->x2 > extents.x2)
				extents.x2 = box->x2;

			if (box->y1 < extents.y1)
				extents.y1 = box->y1;
			if (box->y2 > extents.y2)
				extents.y2 = box->y2;
		}

		if ((extents.x2 - extents.x1) * (extents.y2 - extents.y1) * bpp >> 12
		    < kgem->half_cpu_cache_pages)
			return false;
	}

	return !kgem_bo_map_will_stall(kgem, bo);
}

void sna_write_boxes(struct sna *sna, PixmapPtr dst,
		     struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
		     const void *src, int stride, int16_t src_dx, int16_t src_dy,
		     const BoxRec *box, int nbox)
{
	struct kgem *kgem = &sna->kgem;
	struct kgem_bo *src_bo;
	void *ptr;
	int offset;
	int n, cmd, br13;

	DBG(("%s x %d\n", __FUNCTION__, nbox));

	if (upload_inplace(kgem, dst_bo, box, nbox, dst->drawable.bitsPerPixel)) {
fallback:
		write_boxes_inplace(kgem,
				    src, stride, dst->drawable.bitsPerPixel, src_dx, src_dy,
				    dst_bo, dst_dx, dst_dy,
				    box, nbox);
		return;
	}

	/* Try to avoid switching rings... */
	if (dst_bo->tiling == I915_TILING_Y || kgem->ring == KGEM_RENDER) {
		PixmapRec tmp;
		BoxRec extents;

		extents = box[0];
		for (n = 1; n < nbox; n++) {
			if (box[n].x1 < extents.x1)
				extents.x1 = box[n].x1;
			if (box[n].x2 > extents.x2)
				extents.x2 = box[n].x2;

			if (box[n].y1 < extents.y1)
				extents.y1 = box[n].y1;
			if (box[n].y2 > extents.y2)
				extents.y2 = box[n].y2;
		}

		tmp.drawable.width  = extents.x2 - extents.x1;
		tmp.drawable.height = extents.y2 - extents.y1;
		tmp.drawable.depth  = dst->drawable.depth;
		tmp.drawable.bitsPerPixel = dst->drawable.bitsPerPixel;
		tmp.devPrivate.ptr = NULL;

		assert(tmp.drawable.width);
		assert(tmp.drawable.height);

		src_bo = kgem_create_buffer_2d(kgem,
					       tmp.drawable.width,
					       tmp.drawable.height,
					       tmp.drawable.bitsPerPixel,
					       KGEM_BUFFER_WRITE_INPLACE,
					       &ptr);
		if (!src_bo)
			goto fallback;

		for (n = 0; n < nbox; n++) {
			memcpy_blt(src, ptr, tmp.drawable.bitsPerPixel,
				   stride, src_bo->pitch,
				   box[n].x1 + src_dx,
				   box[n].y1 + src_dy,
				   box[n].x1 - extents.x1,
				   box[n].y1 - extents.y1,
				   box[n].x2 - box[n].x1,
				   box[n].y2 - box[n].y1);
		}

		n = sna->render.copy_boxes(sna, GXcopy,
					   &tmp, src_bo, -extents.x1, -extents.y1,
					   dst, dst_bo, dst_dx, dst_dy,
					   box, nbox);

		kgem_bo_destroy(&sna->kgem, src_bo);

		if (!n)
			goto fallback;

		return;
	}

	cmd = XY_SRC_COPY_BLT_CMD;
	br13 = dst_bo->pitch;
	if (kgem->gen >= 40 && dst_bo->tiling) {
		cmd |= BLT_DST_TILED;
		br13 >>= 2;
	}
	br13 |= 0xcc << 16;
	switch (dst->drawable.bitsPerPixel) {
	default:
	case 32: cmd |= BLT_WRITE_ALPHA | BLT_WRITE_RGB;
		 br13 |= 1 << 25; /* RGB8888 */
	case 16: br13 |= 1 << 24; /* RGB565 */
	case 8: break;
	}

	kgem_set_mode(kgem, KGEM_BLT);
	if (kgem->nexec + 2 > KGEM_EXEC_SIZE(kgem) ||
	    kgem->nreloc + 2 > KGEM_RELOC_SIZE(kgem) ||
	    !kgem_check_batch(kgem, 8) ||
	    !kgem_check_bo_fenced(kgem, dst_bo, NULL)) {
		_kgem_submit(kgem);
		_kgem_set_mode(kgem, KGEM_BLT);
	}

	do {
		int nbox_this_time;

		nbox_this_time = nbox;
		if (8*nbox_this_time > kgem->surface - kgem->nbatch - KGEM_BATCH_RESERVED)
			nbox_this_time = (kgem->surface - kgem->nbatch - KGEM_BATCH_RESERVED) / 8;
		if (2*nbox_this_time > KGEM_RELOC_SIZE(kgem) - kgem->nreloc)
			nbox_this_time = (KGEM_RELOC_SIZE(kgem) - kgem->nreloc) / 2;
		assert(nbox_this_time);
		nbox -= nbox_this_time;

		/* Count the total number of bytes to be read and allocate a
		 * single buffer large enough. Or if it is very small, combine
		 * with other allocations. */
		offset = 0;
		for (n = 0; n < nbox_this_time; n++) {
			int height = box[n].y2 - box[n].y1;
			int width = box[n].x2 - box[n].x1;
			offset += PITCH(width, dst->drawable.bitsPerPixel >> 3) * height;
		}

		src_bo = kgem_create_buffer(kgem, offset,
					    KGEM_BUFFER_WRITE_INPLACE | (nbox ? KGEM_BUFFER_LAST : 0),
					    &ptr);
		if (!src_bo)
			break;

		offset = 0;
		do {
			int height = box->y2 - box->y1;
			int width = box->x2 - box->x1;
			int pitch = PITCH(width, dst->drawable.bitsPerPixel >> 3);
			uint32_t *b;

			DBG(("  %s: box src=(%d, %d), dst=(%d, %d) size=(%d, %d), dst offset=%d, dst pitch=%d\n",
			     __FUNCTION__,
			     box->x1 + src_dx, box->y1 + src_dy,
			     box->x1 + dst_dx, box->y1 + dst_dy,
			     width, height,
			     offset, pitch));

			assert(box->x1 + src_dx >= 0);
			assert((box->x2 + src_dx)*dst->drawable.bitsPerPixel <= 8*stride);
			assert(box->y1 + src_dy >= 0);

			assert(box->x1 + dst_dx >= 0);
			assert(box->y1 + dst_dy >= 0);

			memcpy_blt(src, (char *)ptr + offset,
				   dst->drawable.bitsPerPixel,
				   stride, pitch,
				   box->x1 + src_dx, box->y1 + src_dy,
				   0, 0,
				   width, height);

			b = kgem->batch + kgem->nbatch;
			b[0] = cmd;
			b[1] = br13;
			b[2] = (box->y1 + dst_dy) << 16 | (box->x1 + dst_dx);
			b[3] = (box->y2 + dst_dy) << 16 | (box->x2 + dst_dx);
			b[4] = kgem_add_reloc(kgem, kgem->nbatch + 4, dst_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      0);
			b[5] = 0;
			b[6] = pitch;
			b[7] = kgem_add_reloc(kgem, kgem->nbatch + 7, src_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      KGEM_RELOC_FENCED,
					      offset);
			kgem->nbatch += 8;

			box++;
			offset += pitch * height;
		} while (--nbox_this_time);
		assert(offset == src_bo->size);

		if (nbox) {
			_kgem_submit(kgem);
			_kgem_set_mode(kgem, KGEM_BLT);
		}

		kgem_bo_destroy(kgem, src_bo);
	} while (nbox);

	sna->blt_state.fill_bo = 0;
}

static void
write_boxes_inplace__xor(struct kgem *kgem,
			 const void *src, int stride, int bpp, int16_t src_dx, int16_t src_dy,
			 struct kgem_bo *bo, int16_t dst_dx, int16_t dst_dy,
			 const BoxRec *box, int n,
			 uint32_t and, uint32_t or)
{
	int dst_pitch = bo->pitch;
	int src_pitch = stride;
	void *dst;

	DBG(("%s x %d, tiling=%d\n", __FUNCTION__, n, bo->tiling));

	kgem_bo_submit(kgem, bo);

	dst = kgem_bo_map(kgem, bo);
	if (dst == NULL)
		return;

	do {
		DBG(("%s: (%d, %d) -> (%d, %d) x (%d, %d) [bpp=%d, src_pitch=%d, dst_pitch=%d]\n", __FUNCTION__,
		     box->x1 + src_dx, box->y1 + src_dy,
		     box->x1 + dst_dx, box->y1 + dst_dy,
		     box->x2 - box->x1, box->y2 - box->y1,
		     bpp, src_pitch, dst_pitch));

		memcpy_xor(src, dst, bpp,
			   src_pitch, dst_pitch,
			   box->x1 + src_dx, box->y1 + src_dy,
			   box->x1 + dst_dx, box->y1 + dst_dy,
			   box->x2 - box->x1, box->y2 - box->y1,
			   and, or);
		box++;
	} while (--n);
}

void sna_write_boxes__xor(struct sna *sna, PixmapPtr dst,
			  struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
			  const void *src, int stride, int16_t src_dx, int16_t src_dy,
			  const BoxRec *box, int nbox,
			  uint32_t and, uint32_t or)
{
	struct kgem *kgem = &sna->kgem;
	struct kgem_bo *src_bo;
	void *ptr;
	int offset;
	int n, cmd, br13;

	DBG(("%s x %d\n", __FUNCTION__, nbox));

	if (upload_inplace(kgem, dst_bo, box, nbox, dst->drawable.bitsPerPixel)) {
fallback:
		write_boxes_inplace__xor(kgem,
					 src, stride, dst->drawable.bitsPerPixel, src_dx, src_dy,
					 dst_bo, dst_dx, dst_dy,
					 box, nbox,
					 and, or);
		return;
	}

	/* Try to avoid switching rings... */
	if (dst_bo->tiling == I915_TILING_Y || kgem->ring == KGEM_RENDER) {
		PixmapRec tmp;
		BoxRec extents;

		/* XXX Composite? Not that we should ever reach here! */

		extents = box[0];
		for (n = 1; n < nbox; n++) {
			if (box[n].x1 < extents.x1)
				extents.x1 = box[n].x1;
			if (box[n].x2 > extents.x2)
				extents.x2 = box[n].x2;

			if (box[n].y1 < extents.y1)
				extents.y1 = box[n].y1;
			if (box[n].y2 > extents.y2)
				extents.y2 = box[n].y2;
		}

		tmp.drawable.width = extents.x2 - extents.x1;
		tmp.drawable.height = extents.y2 - extents.y1;
		tmp.drawable.depth = dst->drawable.depth;
		tmp.drawable.bitsPerPixel = dst->drawable.bitsPerPixel;
		tmp.devPrivate.ptr = NULL;

		assert(tmp.drawable.width);
		assert(tmp.drawable.height);

		src_bo = kgem_create_buffer_2d(kgem,
					       tmp.drawable.width,
					       tmp.drawable.height,
					       tmp.drawable.bitsPerPixel,
					       KGEM_BUFFER_WRITE_INPLACE,
					       &ptr);
		if (!src_bo)
			goto fallback;

		for (n = 0; n < nbox; n++) {
			memcpy_xor(src, ptr, tmp.drawable.bitsPerPixel,
				   stride, src_bo->pitch,
				   box[n].x1 + src_dx,
				   box[n].y1 + src_dy,
				   box[n].x1 - extents.x1,
				   box[n].y1 - extents.y1,
				   box[n].x2 - box[n].x1,
				   box[n].y2 - box[n].y1,
				   and, or);
		}

		n = sna->render.copy_boxes(sna, GXcopy,
					   &tmp, src_bo, -extents.x1, -extents.y1,
					   dst, dst_bo, dst_dx, dst_dy,
					   box, nbox);

		kgem_bo_destroy(&sna->kgem, src_bo);

		if (!n)
			goto fallback;

		return;
	}

	cmd = XY_SRC_COPY_BLT_CMD;
	br13 = dst_bo->pitch;
	if (kgem->gen >= 40 && dst_bo->tiling) {
		cmd |= BLT_DST_TILED;
		br13 >>= 2;
	}
	br13 |= 0xcc << 16;
	switch (dst->drawable.bitsPerPixel) {
	default:
	case 32: cmd |= BLT_WRITE_ALPHA | BLT_WRITE_RGB;
		 br13 |= 1 << 25; /* RGB8888 */
	case 16: br13 |= 1 << 24; /* RGB565 */
	case 8: break;
	}

	kgem_set_mode(kgem, KGEM_BLT);
	if (kgem->nexec + 2 > KGEM_EXEC_SIZE(kgem) ||
	    kgem->nreloc + 2 > KGEM_RELOC_SIZE(kgem) ||
	    !kgem_check_batch(kgem, 8) ||
	    !kgem_check_bo_fenced(kgem, dst_bo, NULL)) {
		_kgem_submit(kgem);
		_kgem_set_mode(kgem, KGEM_BLT);
	}

	do {
		int nbox_this_time;

		nbox_this_time = nbox;
		if (8*nbox_this_time > kgem->surface - kgem->nbatch - KGEM_BATCH_RESERVED)
			nbox_this_time = (kgem->surface - kgem->nbatch - KGEM_BATCH_RESERVED) / 8;
		if (2*nbox_this_time > KGEM_RELOC_SIZE(kgem) - kgem->nreloc)
			nbox_this_time = (KGEM_RELOC_SIZE(kgem) - kgem->nreloc) / 2;
		assert(nbox_this_time);
		nbox -= nbox_this_time;

		/* Count the total number of bytes to be read and allocate a
		 * single buffer large enough. Or if it is very small, combine
		 * with other allocations. */
		offset = 0;
		for (n = 0; n < nbox_this_time; n++) {
			int height = box[n].y2 - box[n].y1;
			int width = box[n].x2 - box[n].x1;
			offset += PITCH(width, dst->drawable.bitsPerPixel >> 3) * height;
		}

		src_bo = kgem_create_buffer(kgem, offset,
					    KGEM_BUFFER_WRITE_INPLACE | (nbox ? KGEM_BUFFER_LAST : 0),
					    &ptr);
		if (!src_bo)
			break;

		offset = 0;
		do {
			int height = box->y2 - box->y1;
			int width = box->x2 - box->x1;
			int pitch = PITCH(width, dst->drawable.bitsPerPixel >> 3);
			uint32_t *b;

			DBG(("  %s: box src=(%d, %d), dst=(%d, %d) size=(%d, %d), dst offset=%d, dst pitch=%d\n",
			     __FUNCTION__,
			     box->x1 + src_dx, box->y1 + src_dy,
			     box->x1 + dst_dx, box->y1 + dst_dy,
			     width, height,
			     offset, pitch));

			assert(box->x1 + src_dx >= 0);
			assert((box->x2 + src_dx)*dst->drawable.bitsPerPixel <= 8*stride);
			assert(box->y1 + src_dy >= 0);

			assert(box->x1 + dst_dx >= 0);
			assert(box->y1 + dst_dy >= 0);

			memcpy_xor(src, (char *)ptr + offset,
				   dst->drawable.bitsPerPixel,
				   stride, pitch,
				   box->x1 + src_dx, box->y1 + src_dy,
				   0, 0,
				   width, height,
				   and, or);

			b = kgem->batch + kgem->nbatch;
			b[0] = cmd;
			b[1] = br13;
			b[2] = (box->y1 + dst_dy) << 16 | (box->x1 + dst_dx);
			b[3] = (box->y2 + dst_dy) << 16 | (box->x2 + dst_dx);
			b[4] = kgem_add_reloc(kgem, kgem->nbatch + 4, dst_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      I915_GEM_DOMAIN_RENDER |
					      KGEM_RELOC_FENCED,
					      0);
			b[5] = 0;
			b[6] = pitch;
			b[7] = kgem_add_reloc(kgem, kgem->nbatch + 7, src_bo,
					      I915_GEM_DOMAIN_RENDER << 16 |
					      KGEM_RELOC_FENCED,
					      offset);
			kgem->nbatch += 8;

			box++;
			offset += pitch * height;
		} while (--nbox_this_time);
		assert(offset == src_bo->size);

		if (nbox) {
			_kgem_submit(kgem);
			_kgem_set_mode(kgem, KGEM_BLT);
		}

		kgem_bo_destroy(kgem, src_bo);
	} while (nbox);

	sna->blt_state.fill_bo = 0;
}

static bool
indirect_replace(struct sna *sna,
		 PixmapPtr pixmap,
		 struct kgem_bo *bo,
		 const void *src, int stride)
{
	struct kgem *kgem = &sna->kgem;
	struct kgem_bo *src_bo;
	void *ptr;
	bool ret;

	DBG(("%s: size=%d vs %d\n",
	     __FUNCTION__,
	     (int)pixmap->devKind * pixmap->drawable.height >> 12,
	     kgem->half_cpu_cache_pages));

	if ((int)pixmap->devKind * pixmap->drawable.height >> 12 > kgem->half_cpu_cache_pages)
		return false;

	if (bo->tiling == I915_TILING_Y || kgem->ring == KGEM_RENDER) {
		BoxRec box;

		src_bo = kgem_create_buffer_2d(kgem,
					       pixmap->drawable.width,
					       pixmap->drawable.height,
					       pixmap->drawable.bitsPerPixel,
					       KGEM_BUFFER_WRITE_INPLACE,
					       &ptr);
		if (!src_bo)
			return false;

		memcpy_blt(src, ptr, pixmap->drawable.bitsPerPixel,
			   stride, src_bo->pitch,
			   0, 0,
			   0, 0,
			   pixmap->drawable.width,
			   pixmap->drawable.height);

		box.x1 = box.y1 = 0;
		box.x2 = pixmap->drawable.width;
		box.y2 = pixmap->drawable.height;

		ret = sna->render.copy_boxes(sna, GXcopy,
					     pixmap, src_bo, 0, 0,
					     pixmap, bo, 0, 0,
					     &box, 1);
	} else {
		uint32_t cmd, br13, *b;
		int pitch;

		pitch = pixmap->drawable.width * pixmap->drawable.bitsPerPixel;
		pitch = ALIGN(pitch, 32) >> 3;

		src_bo = kgem_create_buffer(kgem,
					    pitch * pixmap->drawable.height,
					    KGEM_BUFFER_WRITE_INPLACE,
					    &ptr);
		if (!src_bo)
			return false;

		memcpy_blt(src, ptr, pixmap->drawable.bitsPerPixel,
			   stride, pitch,
			   0, 0,
			   0, 0,
			   pixmap->drawable.width,
			   pixmap->drawable.height);

		cmd = XY_SRC_COPY_BLT_CMD;
		br13 = bo->pitch;
		if (kgem->gen >= 40 && bo->tiling) {
			cmd |= BLT_DST_TILED;
			br13 >>= 2;
		}
		br13 |= 0xcc << 16;
		switch (pixmap->drawable.bitsPerPixel) {
		default:
		case 32: cmd |= BLT_WRITE_ALPHA | BLT_WRITE_RGB;
			 br13 |= 1 << 25; /* RGB8888 */
		case 16: br13 |= 1 << 24; /* RGB565 */
		case 8: break;
		}

		kgem_set_mode(kgem, KGEM_BLT);
		if (kgem->nexec + 2 > KGEM_EXEC_SIZE(kgem) ||
		    kgem->nreloc + 2 > KGEM_RELOC_SIZE(kgem) ||
		    !kgem_check_batch(kgem, 8) ||
		    !kgem_check_bo_fenced(kgem, bo, NULL)) {
			_kgem_submit(kgem);
			_kgem_set_mode(kgem, KGEM_BLT);
		}

		b = kgem->batch + kgem->nbatch;
		b[0] = cmd;
		b[1] = br13;
		b[2] = 0;
		b[3] = pixmap->drawable.height << 16 | pixmap->drawable.width;
		b[4] = kgem_add_reloc(kgem, kgem->nbatch + 4, bo,
				      I915_GEM_DOMAIN_RENDER << 16 |
				      I915_GEM_DOMAIN_RENDER |
				      KGEM_RELOC_FENCED,
				      0);
		b[5] = 0;
		b[6] = pitch;
		b[7] = kgem_add_reloc(kgem, kgem->nbatch + 7, src_bo,
				      I915_GEM_DOMAIN_RENDER << 16 |
				      KGEM_RELOC_FENCED,
				      0);
		kgem->nbatch += 8;
		ret = true;
	}

	kgem_bo_destroy(kgem, src_bo);
	return ret;
}

struct kgem_bo *sna_replace(struct sna *sna,
			    PixmapPtr pixmap,
			    struct kgem_bo *bo,
			    const void *src, int stride)
{
	struct kgem *kgem = &sna->kgem;
	void *dst;

	DBG(("%s(handle=%d, %dx%d, bpp=%d, tiling=%d)\n",
	     __FUNCTION__, bo->handle,
	     pixmap->drawable.width,
	     pixmap->drawable.height,
	     pixmap->drawable.bitsPerPixel,
	     bo->tiling));

	if ((!bo->map || bo->rq) &&
	    indirect_replace(sna, pixmap, bo, src, stride))
		return bo;

	if (kgem_bo_is_busy(bo)) {
		struct kgem_bo *new_bo;

		new_bo = kgem_create_2d(kgem,
					pixmap->drawable.width,
					pixmap->drawable.height,
					pixmap->drawable.bitsPerPixel,
					bo->tiling,
					CREATE_GTT_MAP | CREATE_INACTIVE);
		if (new_bo) {
			kgem_bo_destroy(kgem, bo);
			bo = new_bo;
		}
	}

	if (bo->tiling == I915_TILING_NONE && bo->pitch == stride) {
		kgem_bo_write(kgem, bo, src,
			      (pixmap->drawable.height-1)*stride + pixmap->drawable.width*pixmap->drawable.bitsPerPixel/8);
	} else {
		dst = kgem_bo_map(kgem, bo);
		if (dst) {
			memcpy_blt(src, dst, pixmap->drawable.bitsPerPixel,
				   stride, bo->pitch,
				   0, 0,
				   0, 0,
				   pixmap->drawable.width,
				   pixmap->drawable.height);
		}
	}

	return bo;
}

struct kgem_bo *sna_replace__xor(struct sna *sna,
				 PixmapPtr pixmap,
				 struct kgem_bo *bo,
				 const void *src, int stride,
				 uint32_t and, uint32_t or)
{
	struct kgem *kgem = &sna->kgem;
	void *dst;

	DBG(("%s(handle=%d, %dx%d, bpp=%d, tiling=%d)\n",
	     __FUNCTION__, bo->handle,
	     pixmap->drawable.width,
	     pixmap->drawable.height,
	     pixmap->drawable.bitsPerPixel,
	     bo->tiling));

	if (kgem_bo_is_busy(bo)) {
		struct kgem_bo *new_bo;

		new_bo = kgem_create_2d(kgem,
					pixmap->drawable.width,
					pixmap->drawable.height,
					pixmap->drawable.bitsPerPixel,
					bo->tiling,
					CREATE_GTT_MAP | CREATE_INACTIVE);
		if (new_bo) {
			kgem_bo_destroy(kgem, bo);
			bo = new_bo;
		}
	}

	dst = kgem_bo_map(kgem, bo);
	if (dst) {
		memcpy_xor(src, dst, pixmap->drawable.bitsPerPixel,
			   stride, bo->pitch,
			   0, 0,
			   0, 0,
			   pixmap->drawable.width,
			   pixmap->drawable.height,
			   and, or);
	}

	return bo;
}
