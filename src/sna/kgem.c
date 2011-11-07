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
#include "sna_reg.h"

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>

static inline void list_move(struct list *list, struct list *head)
{
	__list_del(list->prev, list->next);
	list_add(list, head);
}

static inline void list_replace(struct list *old,
				struct list *new)
{
	new->next = old->next;
	new->next->prev = new;
	new->prev = old->prev;
	new->prev->next = new;
}

#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

#define list_for_each(pos, head)				\
    for (pos = (head)->next; pos != (head); pos = pos->next)


#define DBG_NO_HW 0
#define DBG_NO_TILING 0
#define DBG_NO_VMAP 0
#define DBG_NO_RELAXED_FENCING 0
#define DBG_DUMP 0

#define NO_CACHE 0

#if DEBUG_KGEM
#undef DBG
#define DBG(x) ErrorF x
#else
#define NDEBUG 1
#endif

#define PAGE_SIZE 4096

struct kgem_partial_bo {
	struct kgem_bo base;
	uint32_t used, alloc;
	uint32_t need_io : 1;
	uint32_t write : 1;
};

static struct drm_i915_gem_exec_object2 _kgem_dummy_exec;

static void kgem_sna_reset(struct kgem *kgem)
{
	struct sna *sna = container_of(kgem, struct sna, kgem);

	sna->render.reset(sna);
	sna->blt_state.fill_bo = 0;
}

static void kgem_sna_flush(struct kgem *kgem)
{
	struct sna *sna = container_of(kgem, struct sna, kgem);

	sna->render.flush(sna);

	if (sna->render.solid_cache.dirty)
		sna_render_flush_solid(sna);
}

static int gem_set_tiling(int fd, uint32_t handle, int tiling, int stride)
{
	struct drm_i915_gem_set_tiling set_tiling;
	int ret;

	if (DBG_NO_TILING)
		return I915_TILING_NONE;

	do {
		set_tiling.handle = handle;
		set_tiling.tiling_mode = tiling;
		set_tiling.stride = stride;

		ret = ioctl(fd, DRM_IOCTL_I915_GEM_SET_TILING, &set_tiling);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));
	return set_tiling.tiling_mode;
}

static void *gem_mmap(int fd, uint32_t handle, int size, int prot)
{
	struct drm_i915_gem_mmap_gtt mmap_arg;
	struct drm_i915_gem_set_domain set_domain;
	void *ptr;

	DBG(("%s(handle=%d, size=%d, prot=%s)\n", __FUNCTION__,
	     handle, size, prot & PROT_WRITE ? "read/write" : "read-only"));

	mmap_arg.handle = handle;
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_MMAP_GTT, &mmap_arg)) {
		assert(0);
		return NULL;
	}

	ptr = mmap(0, size, prot, MAP_SHARED, fd, mmap_arg.offset);
	if (ptr == MAP_FAILED) {
		assert(0);
		ptr = NULL;
	}

	set_domain.handle = handle;
	set_domain.read_domains = I915_GEM_DOMAIN_GTT;
	set_domain.write_domain = prot & PROT_WRITE ? I915_GEM_DOMAIN_GTT : 0;
	drmIoctl(fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);

	return ptr;
}

static int gem_write(int fd, uint32_t handle,
		     int offset, int length,
		     const void *src)
{
	struct drm_i915_gem_pwrite pwrite;

	DBG(("%s(handle=%d, offset=%d, len=%d)\n", __FUNCTION__,
	     handle, offset, length));

	pwrite.handle = handle;
	pwrite.offset = offset;
	pwrite.size = length;
	pwrite.data_ptr = (uintptr_t)src;
	return drmIoctl(fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite);
}

static int gem_read(int fd, uint32_t handle, const void *dst, int length)
{
	struct drm_i915_gem_pread pread;

	DBG(("%s(handle=%d, len=%d)\n", __FUNCTION__,
	     handle, length));

	pread.handle = handle;
	pread.offset = 0;
	pread.size = length;
	pread.data_ptr = (uintptr_t)dst;
	return drmIoctl(fd, DRM_IOCTL_I915_GEM_PREAD, &pread);
}

static bool
kgem_busy(struct kgem *kgem, int handle)
{
	struct drm_i915_gem_busy busy;

	busy.handle = handle;
	busy.busy = !kgem->wedged;
	(void)drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_BUSY, &busy);

	return busy.busy;
}

Bool kgem_bo_write(struct kgem *kgem, struct kgem_bo *bo,
		   const void *data, int length)
{
	assert(!kgem_busy(kgem, bo->handle));

	if (gem_write(kgem->fd, bo->handle, 0, length, data))
		return FALSE;

	bo->needs_flush = false;
	if (bo->gpu)
		kgem_retire(kgem);
	return TRUE;
}

static uint32_t gem_create(int fd, int size)
{
	struct drm_i915_gem_create create;

#if DEBUG_KGEM
	assert((size & (PAGE_SIZE-1)) == 0);
#endif

	create.handle = 0;
	create.size = size;
	(void)drmIoctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create);

	return create.handle;
}

static bool
gem_madvise(int fd, uint32_t handle, uint32_t state)
{
	struct drm_i915_gem_madvise madv;
	int ret;

	madv.handle = handle;
	madv.madv = state;
	madv.retained = 1;
	ret = drmIoctl(fd, DRM_IOCTL_I915_GEM_MADVISE, &madv);

	return madv.retained;
	(void)ret;
}

static void gem_close(int fd, uint32_t handle)
{
	struct drm_gem_close close;

	close.handle = handle;
	(void)drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &close);
}

static struct kgem_bo *__kgem_bo_init(struct kgem_bo *bo,
				      int handle, int size)
{
	memset(bo, 0, sizeof(*bo));

	bo->refcnt = 1;
	bo->handle = handle;
	bo->size = size;
	bo->reusable = true;
	bo->cpu_read = true;
	bo->cpu_write = true;
	list_init(&bo->request);
	list_init(&bo->list);

	return bo;
}

static struct kgem_bo *__kgem_bo_alloc(int handle, int size)
{
	struct kgem_bo *bo;

	bo = malloc(sizeof(*bo));
	if (bo == NULL)
		return NULL;

	return __kgem_bo_init(bo, handle, size);
}

static struct kgem_request *__kgem_request_alloc(void)
{
	struct kgem_request *rq;

	rq = malloc(sizeof(*rq));
	assert(rq);
	if (rq == NULL)
		return rq;

	list_init(&rq->buffers);

	return rq;
}

static inline unsigned long __fls(unsigned long word)
{
	asm("bsr %1,%0"
	    : "=r" (word)
	    : "rm" (word));
	return word;
}

static struct list *inactive(struct kgem *kgem,
			     int size)
{
	uint32_t order = __fls(size / PAGE_SIZE);
	if (order >= ARRAY_SIZE(kgem->inactive))
		order = ARRAY_SIZE(kgem->inactive)-1;
	return &kgem->inactive[order];
}

static struct list *active(struct kgem *kgem,
			     int size)
{
	uint32_t order = __fls(size / PAGE_SIZE);
	if (order >= ARRAY_SIZE(kgem->active))
		order = ARRAY_SIZE(kgem->active)-1;
	return &kgem->active[order];
}

static size_t
agp_aperture_size(struct pci_device *dev, int gen)
{
	return dev->regions[gen < 30 ? 0 : 2].size;
}

void kgem_init(struct kgem *kgem, int fd, struct pci_device *dev, int gen)
{
	drm_i915_getparam_t gp;
	struct drm_i915_gem_get_aperture aperture;
	unsigned int i;
	int v;

	memset(kgem, 0, sizeof(*kgem));

	kgem->fd = fd;
	kgem->gen = gen;
	kgem->wedged = drmCommandNone(kgem->fd, DRM_I915_GEM_THROTTLE) == -EIO;
	kgem->wedged |= DBG_NO_HW;

	list_init(&kgem->partial);
	list_init(&kgem->requests);
	list_init(&kgem->flushing);
	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++)
		list_init(&kgem->inactive[i]);
	for (i = 0; i < ARRAY_SIZE(kgem->active); i++)
		list_init(&kgem->active[i]);

	kgem->next_request = __kgem_request_alloc();

#if defined(USE_VMAP) && defined(I915_PARAM_HAS_VMAP)
	if (!DBG_NO_VMAP) {
		drm_i915_getparam_t gp;

		gp.param = I915_PARAM_HAS_VMAP;
		gp.value = &v;
		kgem->has_vmap =
			drmIoctl(kgem->fd, DRM_IOCTL_I915_GETPARAM, &gp) == 0 &&
			v > 0;
	}
#endif
	DBG(("%s: using vmap=%d\n", __FUNCTION__, kgem->has_vmap));

	if (gen < 40) {
		if (!DBG_NO_RELAXED_FENCING) {
			drm_i915_getparam_t gp;

			gp.param = I915_PARAM_HAS_RELAXED_FENCING;
			gp.value = &v;
			if (drmIoctl(kgem->fd, DRM_IOCTL_I915_GETPARAM, &gp) == 0)
				kgem->has_relaxed_fencing = v > 0;
		}
	} else
		kgem->has_relaxed_fencing = 1;
	DBG(("%s: has relaxed fencing=%d\n", __FUNCTION__,
	     kgem->has_relaxed_fencing));

	aperture.aper_size = 64*1024*1024;
	(void)drmIoctl(fd, DRM_IOCTL_I915_GEM_GET_APERTURE, &aperture);

	kgem->aperture_high = aperture.aper_size * 3/4;
	kgem->aperture_low = aperture.aper_size * 1/4;
	DBG(("%s: aperture low=%d [%d], high=%d [%d]\n", __FUNCTION__,
	     kgem->aperture_low, kgem->aperture_low / (1024*1024),
	     kgem->aperture_high, kgem->aperture_high / (1024*1024)));

	kgem->aperture_mappable = agp_aperture_size(dev, gen);
	if (kgem->aperture_mappable == 0 ||
	    kgem->aperture_mappable > aperture.aper_size)
		kgem->aperture_mappable = aperture.aper_size;
	DBG(("%s: aperture mappable=%d [%d]\n", __FUNCTION__,
	     kgem->aperture_mappable, kgem->aperture_mappable / (1024*1024)));

	kgem->max_object_size = kgem->aperture_mappable / 2;
	if (kgem->max_object_size > kgem->aperture_low)
		kgem->max_object_size = kgem->aperture_low;
	DBG(("%s: max object size %d\n", __FUNCTION__, kgem->max_object_size));

	v = 8;
	gp.param = I915_PARAM_NUM_FENCES_AVAIL;
	gp.value = &v;
	(void)drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &gp);
	kgem->fence_max = v - 2;

	DBG(("%s: max fences=%d\n", __FUNCTION__, kgem->fence_max));
}

/* XXX hopefully a good approximation */
static uint32_t kgem_get_unique_id(struct kgem *kgem)
{
	uint32_t id;
	id = ++kgem->unique_id;
	if (id == 0)
		id = ++kgem->unique_id;
	return id;
}

static uint32_t kgem_surface_size(struct kgem *kgem,
				  uint32_t width,
				  uint32_t height,
				  uint32_t bpp,
				  uint32_t tiling,
				  uint32_t *pitch)
{
	uint32_t tile_width, tile_height;
	uint32_t size;

	if (kgem->gen < 30) {
		if (tiling) {
			tile_width = 512;
			tile_height = 16;
		} else {
			tile_width = 64;
			tile_height = 2;
		}
	} else switch (tiling) {
	default:
	case I915_TILING_NONE:
		tile_width = 64;
		tile_height = 2;
		break;
	case I915_TILING_X:
		tile_width = 512;
		tile_height = 8;
		break;
	case I915_TILING_Y:
		tile_width = kgem->gen <= 30 ? 512 : 128;
		tile_height = 32;
		break;
	}

	*pitch = ALIGN(width * bpp / 8, tile_width);
	if (kgem->gen < 40 && tiling != I915_TILING_NONE) {
		if (*pitch > 8192)
			return 0;
		for (size = tile_width; size < *pitch; size <<= 1)
			;
		*pitch = size;
	}

	size = *pitch * ALIGN(height, tile_height);
	if (kgem->has_relaxed_fencing || tiling == I915_TILING_NONE)
		return ALIGN(size, PAGE_SIZE);

	/*  We need to allocate a pot fence region for a tiled buffer. */
	if (kgem->gen < 30)
		tile_width = 512 * 1024;
	else
		tile_width = 1024 * 1024;
	while (tile_width < size)
		tile_width *= 2;
	return tile_width;
}

static uint32_t kgem_aligned_height(struct kgem *kgem,
				    uint32_t height, uint32_t tiling)
{
	uint32_t tile_height;

	if (kgem->gen < 30) {
		tile_height = tiling ? 16 : 2;
	} else switch (tiling) {
	default:
	case I915_TILING_NONE:
		tile_height = 2;
		break;
	case I915_TILING_X:
		tile_height = 8;
		break;
	case I915_TILING_Y:
		tile_height = 32;
		break;
	}

	return ALIGN(height, tile_height);
}

static struct drm_i915_gem_exec_object2 *
kgem_add_handle(struct kgem *kgem, struct kgem_bo *bo)
{
	struct drm_i915_gem_exec_object2 *exec;

	assert(kgem->nexec < ARRAY_SIZE(kgem->exec));
	exec = memset(&kgem->exec[kgem->nexec++], 0, sizeof(*exec));
	exec->handle = bo->handle;
	exec->offset = bo->presumed_offset;

	kgem->aperture += bo->size;

	return exec;
}

void _kgem_add_bo(struct kgem *kgem, struct kgem_bo *bo)
{
	bo->exec = kgem_add_handle(kgem, bo);
	bo->rq = kgem->next_request;
	bo->gpu = true;
	list_move(&bo->request, &kgem->next_request->buffers);
	kgem->flush |= bo->flush;
}

static uint32_t kgem_end_batch(struct kgem *kgem)
{
	kgem->context_switch(kgem, KGEM_NONE);

	kgem->batch[kgem->nbatch++] = MI_BATCH_BUFFER_END;
	if (kgem->nbatch & 1)
		kgem->batch[kgem->nbatch++] = MI_NOOP;

	return kgem->nbatch;
}

static void kgem_fixup_self_relocs(struct kgem *kgem, struct kgem_bo *bo)
{
	int n;

	for (n = 0; n < kgem->nreloc; n++) {
		if (kgem->reloc[n].target_handle == 0) {
			kgem->reloc[n].target_handle = bo->handle;
			kgem->batch[kgem->reloc[n].offset/sizeof(kgem->batch[0])] =
				kgem->reloc[n].delta + bo->presumed_offset;
		}
	}
}

static void __kgem_bo_destroy(struct kgem *kgem, struct kgem_bo *bo)
{
	assert(list_is_empty(&bo->list));
	assert(bo->refcnt == 0);

	bo->src_bound = bo->dst_bound = 0;

	if (NO_CACHE)
		goto destroy;

	if(!bo->reusable)
		goto destroy;

	if (!bo->deleted && !bo->exec) {
		if (!gem_madvise(kgem->fd, bo->handle, I915_MADV_DONTNEED)) {
			kgem->need_purge |= bo->gpu;
			goto destroy;
		}

		bo->deleted = 1;
	}

	kgem->need_expire = true;
	if (bo->rq) {
		list_move(&bo->list, active(kgem, bo->size));
	} else if (bo->needs_flush | bo->gpu) {
		assert(list_is_empty(&bo->request));
		list_add(&bo->request, &kgem->flushing);
		list_move(&bo->list, active(kgem, bo->size));
	} else {
		list_move(&bo->list, inactive(kgem, bo->size));
	}

	return;

destroy:
	if (!bo->exec) {
		list_del(&bo->request);
		gem_close(kgem->fd, bo->handle);
		free(bo);
	}
}

static void kgem_bo_unref(struct kgem *kgem, struct kgem_bo *bo)
{
	if (--bo->refcnt == 0)
		__kgem_bo_destroy(kgem, bo);
}

void kgem_retire(struct kgem *kgem)
{
	struct kgem_bo *bo, *next;

	DBG(("%s\n", __FUNCTION__));

	list_for_each_entry_safe(bo, next, &kgem->flushing, request) {
		if (kgem_busy(kgem, bo->handle))
			break;

		DBG(("%s: moving %d from flush to inactive\n",
		     __FUNCTION__, bo->handle));
		bo->needs_flush = 0;
		bo->gpu = false;
		list_move(&bo->list, inactive(kgem, bo->size));
		list_del(&bo->request);
	}

	while (!list_is_empty(&kgem->requests)) {
		struct kgem_request *rq;

		rq = list_first_entry(&kgem->requests,
				      struct kgem_request,
				      list);
		if (kgem_busy(kgem, rq->bo->handle))
			break;

		DBG(("%s: request %d complete\n",
		     __FUNCTION__, rq->bo->handle));

		while (!list_is_empty(&rq->buffers)) {
			bo = list_first_entry(&rq->buffers,
					      struct kgem_bo,
					      request);
			list_del(&bo->request);
			bo->rq = NULL;

#if 0
			/* XXX we loose track of a write-flush somewhere? */
			if (!bo->needs_flush)
				bo->needs_flush = kgem_busy(kgem, bo->handle);
#endif
			bo->gpu = bo->needs_flush;

			if (bo->refcnt == 0) {
				assert(bo->deleted);
				if (bo->needs_flush) {
					DBG(("%s: moving %d to flushing\n",
					     __FUNCTION__, bo->handle));
					list_add(&bo->request, &kgem->flushing);
				} else if (bo->reusable) {
					DBG(("%s: moving %d to inactive\n",
					     __FUNCTION__, bo->handle));
					list_move(&bo->list,
						  inactive(kgem, bo->size));
				} else {
					DBG(("%s: closing %d\n",
					     __FUNCTION__, bo->handle));
					gem_close(kgem->fd, bo->handle);
					free(bo);
				}
			}
		}

		rq->bo->refcnt--;
		assert(rq->bo->refcnt == 0);
		if (gem_madvise(kgem->fd, rq->bo->handle, I915_MADV_DONTNEED)) {
			rq->bo->deleted = 1;
			assert(rq->bo->gpu == 0);
			list_move(&rq->bo->list,
				  inactive(kgem, rq->bo->size));
		} else {
			kgem->need_purge = 1;
			gem_close(kgem->fd, rq->bo->handle);
			free(rq->bo);
		}

		list_del(&rq->list);
		free(rq);
	}

	if (kgem->ring && list_is_empty(&kgem->requests))
		kgem->ring = kgem->mode;
}

static void kgem_commit(struct kgem *kgem)
{
	struct kgem_request *rq = kgem->next_request;
	struct kgem_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &rq->buffers, request) {
		bo->src_bound = bo->dst_bound = 0;
		bo->presumed_offset = bo->exec->offset;
		bo->exec = NULL;
		bo->dirty = false;
		bo->cpu_read = false;
		bo->cpu_write = false;

		if (!bo->refcnt) {
			if (!bo->reusable) {
destroy:
				list_del(&bo->list);
				list_del(&bo->request);
				gem_close(kgem->fd, bo->handle);
				free(bo);
				continue;
			}
			if (!bo->deleted) {
				if (!gem_madvise(kgem->fd, bo->handle,
						 I915_MADV_DONTNEED)) {
					kgem->need_purge = 1;
					goto destroy;
				}
				bo->deleted = 1;
			}
		}
	}

	list_add_tail(&rq->list, &kgem->requests);
	kgem->next_request = __kgem_request_alloc();
}

static void kgem_close_list(struct kgem *kgem, struct list *head)
{
	while (!list_is_empty(head)) {
		struct kgem_bo *bo;

		bo = list_first_entry(head, struct kgem_bo, list);
		gem_close(kgem->fd, bo->handle);
		list_del(&bo->list);
		list_del(&bo->request);
		free(bo);
	}
}

static void kgem_close_inactive(struct kgem *kgem)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++)
		kgem_close_list(kgem, &kgem->inactive[i]);
}

static void kgem_finish_partials(struct kgem *kgem)
{
	struct kgem_partial_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &kgem->partial, base.list) {
		if (!bo->base.exec) {
			if (bo->base.refcnt == 1) {
				DBG(("%s: discarding unused partial array: %d/%d\n",
				     __FUNCTION__, bo->used, bo->alloc));

				list_del(&bo->base.list);
				gem_close(kgem->fd, bo->base.handle);
				free(bo);
			}

			continue;
		}

		list_del(&bo->base.list);
		if (bo->write && bo->need_io) {
			DBG(("%s: handle=%d, uploading %d/%d\n",
			     __FUNCTION__, bo->base.handle, bo->used, bo->alloc));
			assert(!kgem_busy(kgem, bo->base.handle));
			gem_write(kgem->fd, bo->base.handle,
				  0, bo->used, bo+1);
			bo->need_io = 0;
		}

		/* transfer the handle to a minimum bo */
		if (bo->base.refcnt == 1) {
			struct kgem_bo *base = malloc(sizeof(*base));
			if (base) {
				memcpy(base, &bo->base, sizeof (*base));
				base->reusable = true;
				list_init(&base->list);
				list_replace(&bo->base.request, &base->request);
				free(bo);
				bo = (struct kgem_partial_bo *)base;
			}
		}
		kgem_bo_unref(kgem, &bo->base);
	}
}

static void kgem_cleanup(struct kgem *kgem)
{
	while (!list_is_empty(&kgem->partial)) {
		struct kgem_bo *bo;

		bo = list_first_entry(&kgem->partial,
				      struct kgem_bo,
				      list);
		list_del(&bo->list);
		kgem_bo_unref(kgem, bo);
	}

	while (!list_is_empty(&kgem->requests)) {
		struct kgem_request *rq;

		rq = list_first_entry(&kgem->requests,
				      struct kgem_request,
				      list);
		while (!list_is_empty(&rq->buffers)) {
			struct kgem_bo *bo;

			bo = list_first_entry(&rq->buffers,
					      struct kgem_bo,
					      request);
			list_del(&bo->request);
			bo->rq = NULL;
			bo->gpu = false;
			if (bo->refcnt == 0) {
				list_del(&bo->list);
				gem_close(kgem->fd, bo->handle);
				free(bo);
			}
		}

		list_del(&rq->list);
		free(rq);
	}

	kgem_close_inactive(kgem);
}

static int kgem_batch_write(struct kgem *kgem, uint32_t handle)
{
	int ret;

	assert(!kgem_busy(kgem, handle));

	/* If there is no surface data, just upload the batch */
	if (kgem->surface == ARRAY_SIZE(kgem->batch))
		return gem_write(kgem->fd, handle,
				 0, sizeof(uint32_t)*kgem->nbatch,
				 kgem->batch);

	/* Are the batch pages conjoint with the surface pages? */
	if (kgem->surface < kgem->nbatch + PAGE_SIZE/4)
		return gem_write(kgem->fd, handle,
				 0, sizeof(kgem->batch),
				 kgem->batch);

	/* Disjoint surface/batch, upload separately */
	ret = gem_write(kgem->fd, handle,
			0, sizeof(uint32_t)*kgem->nbatch,
			kgem->batch);
	if (ret)
		return ret;

	return gem_write(kgem->fd, handle,
			sizeof(uint32_t)*kgem->surface,
			sizeof(kgem->batch) - sizeof(uint32_t)*kgem->surface,
			kgem->batch + kgem->surface);
}

void kgem_reset(struct kgem *kgem)
{
	struct kgem_request *rq = kgem->next_request;
	struct kgem_bo *bo;

	while (!list_is_empty(&rq->buffers)) {
		bo = list_first_entry(&rq->buffers, struct kgem_bo, request);

		bo->src_bound = bo->dst_bound = 0;
		bo->exec = NULL;
		bo->dirty = false;
		bo->cpu_read = false;
		bo->cpu_write = false;

		list_del(&bo->request);
	}

	kgem->nfence = 0;
	kgem->nexec = 0;
	kgem->nreloc = 0;
	kgem->aperture = 0;
	kgem->aperture_fenced = 0;
	kgem->nbatch = 0;
	kgem->surface = ARRAY_SIZE(kgem->batch);
	kgem->mode = KGEM_NONE;
	kgem->flush = 0;

	kgem_sna_reset(kgem);
}

void _kgem_submit(struct kgem *kgem)
{
	struct kgem_request *rq;
	uint32_t batch_end;
	int size;

	assert(!DBG_NO_HW);

	assert(kgem->nbatch);
	assert(kgem->nbatch <= KGEM_BATCH_SIZE(kgem));
	assert(kgem->nbatch <= kgem->surface);

	batch_end = kgem_end_batch(kgem);
	kgem_sna_flush(kgem);

	DBG(("batch[%d/%d]: %d %d %d, nreloc=%d, nexec=%d, nfence=%d, aperture=%d\n",
	     kgem->mode, kgem->ring, batch_end, kgem->nbatch, kgem->surface,
	     kgem->nreloc, kgem->nexec, kgem->nfence, kgem->aperture));

	assert(kgem->nbatch <= ARRAY_SIZE(kgem->batch));
	assert(kgem->nbatch <= kgem->surface);
	assert(kgem->nreloc <= ARRAY_SIZE(kgem->reloc));
	assert(kgem->nexec < ARRAY_SIZE(kgem->exec));
	assert(kgem->nfence <= kgem->fence_max);
#if DEBUG_BATCH
	__kgem_batch_debug(kgem, batch_end);
#endif

	rq = kgem->next_request;
	if (kgem->surface != ARRAY_SIZE(kgem->batch))
		size = sizeof(kgem->batch);
	else
		size = kgem->nbatch * sizeof(kgem->batch[0]);
	rq->bo = kgem_create_linear(kgem, size);
	if (rq->bo) {
		uint32_t handle = rq->bo->handle;
		int i;

		i = kgem->nexec++;
		kgem->exec[i].handle = handle;
		kgem->exec[i].relocation_count = kgem->nreloc;
		kgem->exec[i].relocs_ptr = (uintptr_t)kgem->reloc;
		kgem->exec[i].alignment = 0;
		kgem->exec[i].offset = 0;
		kgem->exec[i].flags = 0;
		kgem->exec[i].rsvd1 = 0;
		kgem->exec[i].rsvd2 = 0;

		rq->bo->exec = &kgem->exec[i];
		list_add(&rq->bo->request, &rq->buffers);

		kgem_fixup_self_relocs(kgem, rq->bo);
		kgem_finish_partials(kgem);

		assert(rq->bo->gpu == 0);
		if (kgem_batch_write(kgem, handle) == 0) {
			struct drm_i915_gem_execbuffer2 execbuf;
			int ret;

			execbuf.buffers_ptr = (uintptr_t)kgem->exec;
			execbuf.buffer_count = kgem->nexec;
			execbuf.batch_start_offset = 0;
			execbuf.batch_len = batch_end*4;
			execbuf.cliprects_ptr = 0;
			execbuf.num_cliprects = 0;
			execbuf.DR1 = 0;
			execbuf.DR4 = 0;
			execbuf.flags = kgem->ring;
			execbuf.rsvd1 = 0;
			execbuf.rsvd2 = 0;

			if (DBG_DUMP) {
				int fd = open("/tmp/i915-batchbuffers.dump",
					      O_WRONLY | O_CREAT | O_APPEND,
					      0666);
				if (fd != -1) {
					ret = write(fd, kgem->batch, batch_end*4);
					fd = close(fd);
				}
			}

			ret = drmIoctl(kgem->fd,
				       DRM_IOCTL_I915_GEM_EXECBUFFER2,
				       &execbuf);
			while (ret == -1 && errno == EBUSY) {
				drmCommandNone(kgem->fd, DRM_I915_GEM_THROTTLE);
				ret = drmIoctl(kgem->fd,
					       DRM_IOCTL_I915_GEM_EXECBUFFER2,
					       &execbuf);
			}
			if (ret == -1 && errno == EIO) {
				DBG(("%s: GPU hang detected\n", __FUNCTION__));
				kgem->wedged = 1;
				ret = 0;
			}
#if DEBUG_KGEM
			if (ret < 0) {
				int i;
				ErrorF("batch (end=%d, size=%d) submit failed: %d\n",
				       batch_end, size, errno);

				i = open("/tmp/batchbuffer", O_WRONLY | O_CREAT | O_APPEND, 0666);
				if (i != -1) {
					ret = write(i, kgem->batch, batch_end*4);
					close(i);
				}

				for (i = 0; i < kgem->nexec; i++) {
					struct kgem_request *rq = kgem->next_request;
					struct kgem_bo *bo, *found = NULL;

					list_for_each_entry(bo, &rq->buffers, request) {
						if (bo->handle == kgem->exec[i].handle) {
							found = bo;
							break;
						}
					}
					ErrorF("exec[%d] = handle:%d, presumed offset: %x, size: %d, tiling %d, fenced %d, deleted %d\n",
					       i,
					       kgem->exec[i].handle,
					       (int)kgem->exec[i].offset,
					       found ? found->size : -1,
					       found ? found->tiling : -1,
					       (int)(kgem->exec[i].flags & EXEC_OBJECT_NEEDS_FENCE),
					       found ? found->deleted : -1);
				}
				for (i = 0; i < kgem->nreloc; i++) {
					ErrorF("reloc[%d] = pos:%d, target:%d, delta:%d, read:%x, write:%x, offset:%x\n",
					       i,
					       (int)kgem->reloc[i].offset,
					       kgem->reloc[i].target_handle,
					       kgem->reloc[i].delta,
					       kgem->reloc[i].read_domains,
					       kgem->reloc[i].write_domain,
					       (int)kgem->reloc[i].presumed_offset);
				}
				abort();
			}
#endif
			assert(ret == 0);

			if (DEBUG_FLUSH_SYNC) {
				struct drm_i915_gem_set_domain set_domain;
				int ret;

				set_domain.handle = handle;
				set_domain.read_domains = I915_GEM_DOMAIN_GTT;
				set_domain.write_domain = I915_GEM_DOMAIN_GTT;

				ret = drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
				if (ret == -1) {
					DBG(("%s: sync: GPU hang detected\n", __FUNCTION__));
					kgem->wedged = 1;
				}
			}
		}
	}

	kgem_retire(kgem);
	kgem_commit(kgem);
	if (kgem->wedged)
		kgem_cleanup(kgem);

	kgem_reset(kgem);
	kgem->busy = 1;
}

void kgem_throttle(struct kgem *kgem)
{
	static int warned;

	kgem->wedged |= drmCommandNone(kgem->fd, DRM_I915_GEM_THROTTLE) == -EIO;

	if (kgem->wedged && !warned) {
		struct sna *sna = container_of(kgem, struct sna, kgem);
		xf86DrvMsg(sna->scrn->scrnIndex, X_ERROR,
			   "Detected a hung GPU, disabling acceleration.\n");
		xf86DrvMsg(sna->scrn->scrnIndex, X_ERROR,
			   "When reporting this, please include i915_error_state from debugfs and the full dmesg.\n");
		warned = 1;
	}
}

static void kgem_expire_partial(struct kgem *kgem)
{
	struct kgem_partial_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &kgem->partial, base.list) {
		if (bo->base.refcnt > 1 || bo->base.exec)
			continue;

		DBG(("%s: discarding unused partial array: %d/%d\n",
		     __FUNCTION__, bo->used, bo->alloc));
		list_del(&bo->base.list);
		kgem_bo_unref(kgem, &bo->base);
	}
}

bool kgem_expire_cache(struct kgem *kgem)
{
	time_t now, expire;
	struct kgem_bo *bo;
	unsigned int size = 0, count = 0;
	bool idle;
	unsigned int i;

	kgem_retire(kgem);
	if (kgem->wedged)
		kgem_cleanup(kgem);

	kgem_expire_partial(kgem);

	time(&now);
	expire = 0;

	idle = true;
	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++) {
		idle &= list_is_empty(&kgem->inactive[i]);
		list_for_each_entry(bo, &kgem->inactive[i], list) {
			assert(bo->deleted);
			if (bo->delta) {
				expire = now - 5;
				break;
			}

			bo->delta = now;
		}
	}
	if (!kgem->need_purge) {
		if (idle) {
			DBG(("%s: idle\n", __FUNCTION__));
			kgem->need_expire = false;
			return false;
		}
		if (expire == 0)
			return true;
	}

	idle = true;
	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++) {
		while (!list_is_empty(&kgem->inactive[i])) {
			bo = list_last_entry(&kgem->inactive[i],
					     struct kgem_bo, list);

			if (gem_madvise(kgem->fd, bo->handle,
					I915_MADV_DONTNEED) &&
			    bo->delta > expire) {
				idle = false;
				break;
			}

			count++;
			size += bo->size;

			gem_close(kgem->fd, bo->handle);
			list_del(&bo->list);
			free(bo);
		}
	}

	DBG(("%s: purge? %d -- expired %d objects, %d bytes, idle? %d\n",
	     __FUNCTION__, kgem->need_purge,  count, size, idle));

	kgem->need_purge = false;
	kgem->need_expire = !idle;
	return !idle;
	(void)count;
	(void)size;
}

void kgem_cleanup_cache(struct kgem *kgem)
{
	struct kgem_bo *bo;
	unsigned int i;

	/* sync to the most recent request */
	if (!list_is_empty(&kgem->requests)) {
		struct kgem_request *rq;
		struct drm_i915_gem_set_domain set_domain;

		rq = list_first_entry(&kgem->requests,
				      struct kgem_request,
				      list);

		set_domain.handle = rq->bo->handle;
		set_domain.read_domains = I915_GEM_DOMAIN_GTT;
		set_domain.write_domain = I915_GEM_DOMAIN_GTT;
		drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
	}

	kgem_retire(kgem);
	kgem_cleanup(kgem);
	kgem_expire_partial(kgem);

	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++) {
		while (!list_is_empty(&kgem->inactive[i])) {
			bo = list_last_entry(&kgem->inactive[i],
					     struct kgem_bo, list);

			gem_close(kgem->fd, bo->handle);
			list_del(&bo->list);
			free(bo);
		}
	}

	kgem->need_purge = false;
	kgem->need_expire = false;
}

static struct kgem_bo *
search_linear_cache(struct kgem *kgem, unsigned int size, bool use_active)
{
	struct kgem_bo *bo;
	struct list *cache;

	cache = use_active ? active(kgem, size): inactive(kgem, size);
	list_for_each_entry(bo, cache, list) {
		if (size > bo->size)
			continue;

		if (use_active && bo->tiling != I915_TILING_NONE)
			continue;

		if (bo->deleted) {
			if (!gem_madvise(kgem->fd, bo->handle,
					 I915_MADV_WILLNEED)) {
				kgem->need_purge |= bo->gpu;
				continue;
			}

			bo->deleted = 0;
		}

		if (I915_TILING_NONE != bo->tiling &&
		    gem_set_tiling(kgem->fd, bo->handle,
				   I915_TILING_NONE, 0) != I915_TILING_NONE)
			continue;

		list_del(&bo->list);
		if (bo->rq == NULL)
			list_del(&bo->request);

		bo->tiling = I915_TILING_NONE;
		bo->pitch = 0;
		bo->delta = 0;
		DBG(("  %s: found handle=%d (size=%d) in linear %s cache\n",
		     __FUNCTION__, bo->handle, bo->size,
		     use_active ? "active" : "inactive"));
		assert(bo->refcnt == 0);
		assert(bo->reusable);
		assert(use_active || bo->gpu == 0);
		//assert(use_active || !kgem_busy(kgem, bo->handle));
		return bo;
	}

	return NULL;
}

struct kgem_bo *kgem_create_for_name(struct kgem *kgem, uint32_t name)
{
	struct drm_gem_open open_arg;
	struct kgem_bo *bo;

	DBG(("%s(name=%d)\n", __FUNCTION__, name));

	memset(&open_arg, 0, sizeof(open_arg));
	open_arg.name = name;
	if (drmIoctl(kgem->fd, DRM_IOCTL_GEM_OPEN, &open_arg))
		return NULL;

	DBG(("%s: new handle=%d\n", __FUNCTION__, open_arg.handle));
	bo = __kgem_bo_alloc(open_arg.handle, 0);
	if (bo == NULL) {
		gem_close(kgem->fd, open_arg.handle);
		return NULL;
	}

	bo->reusable = false;
	return bo;
}

struct kgem_bo *kgem_create_linear(struct kgem *kgem, int size)
{
	struct kgem_bo *bo;
	uint32_t handle;

	DBG(("%s(%d)\n", __FUNCTION__, size));

	size = ALIGN(size, PAGE_SIZE);
	bo = search_linear_cache(kgem, size, false);
	if (bo)
		return kgem_bo_reference(bo);

	if (!list_is_empty(&kgem->requests)) {
		kgem_retire(kgem);
		bo = search_linear_cache(kgem, size, false);
		if (bo)
			return kgem_bo_reference(bo);
	}

	handle = gem_create(kgem->fd, size);
	if (handle == 0)
		return NULL;

	DBG(("%s: new handle=%d\n", __FUNCTION__, handle));
	return __kgem_bo_alloc(handle, size);
}

int kgem_choose_tiling(struct kgem *kgem, int tiling, int width, int height, int bpp)
{
	if (DBG_NO_TILING)
		return I915_TILING_NONE;

	if (kgem->gen < 40) {
		if (tiling) {
			if (width * bpp > 8192 * 8) {
				DBG(("%s: pitch too large for tliing [%d]\n",
				     __FUNCTION__, width*bpp/8));
				return I915_TILING_NONE;
			}

			if (width > 2048 || height > 2048) {
				DBG(("%s: large buffer (%dx%d), forcing TILING_X\n",
				     __FUNCTION__, width, height));
				return -I915_TILING_X;
			}
		}
	} else {
		if (width*bpp > (MAXSHORT-512) * 8) {
			DBG(("%s: large pitch [%d], forcing TILING_X\n",
			     __FUNCTION__, width*bpp/8));
			return -I915_TILING_X;
		}

		if (tiling && (width > 8192 || height > 8192)) {
			DBG(("%s: large tiled buffer [%dx%d], forcing TILING_X\n",
			     __FUNCTION__, width, height));
			return -I915_TILING_X;
		}
	}

	if (tiling < 0)
		return tiling;

	if (tiling == I915_TILING_Y && height <= 16) {
		DBG(("%s: too short [%d] for TILING_Y\n",
		     __FUNCTION__,height));
		tiling = I915_TILING_X;
	}
	if (tiling && width * bpp >= 8 * 4096) {
		DBG(("%s: TLB miss between lines %dx%d (pitch=%d), forcing tiling %d\n",
		     __FUNCTION__,
		     width, height, width*bpp/8,
		     tiling));
		return -tiling;
	}
	if (tiling == I915_TILING_X && height < 4) {
		DBG(("%s: too short [%d] for TILING_X\n",
		     __FUNCTION__, height));
		tiling = I915_TILING_NONE;
	}

	/* Before the G33, we only have a small GTT to play with and tiled
	 * surfaces always require full fence regions and so cause excessive
	 * aperture thrashing.
	 */
	if (kgem->gen < 33) {
		if (tiling == I915_TILING_X && width * bpp < 8*512/2) {
			DBG(("%s: too thin [%d] for TILING_X\n",
			     __FUNCTION__, width));
			tiling = I915_TILING_NONE;
		}
		if (tiling == I915_TILING_Y && width * bpp < 8*32/2) {
			DBG(("%s: too thin [%d] for TILING_Y\n",
			     __FUNCTION__, width));
			tiling = I915_TILING_NONE;
		}
	}

	DBG(("%s: %dx%d -> %d\n", __FUNCTION__, width, height, tiling));
	return tiling;
}

static bool _kgem_can_create_2d(struct kgem *kgem,
				int width, int height, int bpp, int tiling)
{
	uint32_t pitch, size;

	if (bpp < 8)
		return false;

	if (tiling >= 0 && kgem->wedged)
		return false;

	if (tiling < 0)
		tiling = -tiling;

	size = kgem_surface_size(kgem, width, height, bpp, tiling, &pitch);
	if (size == 0 || size > kgem->max_object_size)
		size = kgem_surface_size(kgem, width, height, bpp, I915_TILING_NONE, &pitch);
	return size > 0 && size <= kgem->max_object_size;
}

#if DEBUG_KGEM
bool kgem_can_create_2d(struct kgem *kgem,
			int width, int height, int bpp, int tiling)
{
	bool ret = _kgem_can_create_2d(kgem, width, height, bpp, tiling);
	DBG(("%s(%dx%d, bpp=%d, tiling=%d) = %d\n", __FUNCTION__,
	     width, height, bpp, tiling, ret));
	return ret;
}
#else
bool kgem_can_create_2d(struct kgem *kgem,
			int width, int height, int bpp, int tiling)
{
	return _kgem_can_create_2d(kgem, width, height, bpp, tiling);
}
#endif

static int kgem_bo_fenced_size(struct kgem *kgem, struct kgem_bo *bo)
{
	unsigned int size;

	assert(bo->tiling);
	assert(kgem->gen < 40);

	if (kgem->gen < 30)
		size = 512 * 1024;
	else
		size = 1024 * 1024;
	while (size < bo->size)
		size *= 2;

	return size;
}

struct kgem_bo *kgem_create_2d(struct kgem *kgem,
			       int width,
			       int height,
			       int bpp,
			       int tiling,
			       uint32_t flags)
{
	struct list *cache;
	struct kgem_bo *bo, *next;
	uint32_t pitch, tiled_height[3], size;
	uint32_t handle;
	int exact = flags & CREATE_EXACT;
	int i;

	if (tiling < 0)
		tiling = -tiling, exact = 1;

	DBG(("%s(%dx%d, bpp=%d, tiling=%d, exact=%d, inactive=%d)\n", __FUNCTION__,
	     width, height, bpp, tiling, !!exact, !!(flags & CREATE_INACTIVE)));

	assert(_kgem_can_create_2d(kgem, width, height, bpp, tiling));
	size = kgem_surface_size(kgem, width, height, bpp, tiling, &pitch);
	assert(size && size <= kgem->max_object_size);
	if (flags & CREATE_INACTIVE)
		goto skip_active_search;

	for (i = 0; i <= I915_TILING_Y; i++)
		tiled_height[i] = kgem_aligned_height(kgem, height, i);

search_active: /* Best active match first */
	list_for_each_entry(bo, active(kgem, size), list) {
		uint32_t s;

		if (exact) {
			if (bo->tiling != tiling)
				continue;
		} else {
			if (bo->tiling > tiling)
				continue;
		}

		if (bo->tiling) {
			if (bo->pitch < pitch) {
				DBG(("tiled and pitch too small: tiling=%d, (want %d), pitch=%d, need %d\n",
				     bo->tiling, tiling,
				     bo->pitch, pitch));
				continue;
			}
		} else
			bo->pitch = pitch;

		s = bo->pitch * tiled_height[bo->tiling];
		if (s <= bo->size) {
			list_del(&bo->list);
			if (bo->rq == NULL)
				list_del(&bo->request);

			if (bo->deleted) {
				if (!gem_madvise(kgem->fd, bo->handle,
						 I915_MADV_WILLNEED)) {
					kgem->need_purge |= bo->gpu;
					gem_close(kgem->fd, bo->handle);
					list_del(&bo->request);
					free(bo);
					bo = NULL;
					goto search_active;
				}

				bo->deleted = 0;
			}

			bo->unique_id = kgem_get_unique_id(kgem);
			bo->delta = 0;
			DBG(("  from active: pitch=%d, tiling=%d, handle=%d, id=%d\n",
			     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
			assert(bo->refcnt == 0);
			assert(bo->reusable);
			return kgem_bo_reference(bo);
		}
	}

skip_active_search:
	/* Now just look for a close match and prefer any currently active */
	cache = inactive(kgem, size);
	list_for_each_entry_safe(bo, next, cache, list) {
		if (size > bo->size) {
			DBG(("inactive too small: %d < %d\n",
			     bo->size, size));
			continue;
		}

		if (bo->tiling != tiling ||
		    (tiling != I915_TILING_NONE && bo->pitch != pitch)) {
			if (tiling != gem_set_tiling(kgem->fd,
						     bo->handle,
						     tiling, pitch))
				goto next_bo;
		}

		bo->pitch = pitch;
		bo->tiling = tiling;

		list_del(&bo->list);
		assert(list_is_empty(&bo->request));

		if (bo->deleted) {
			if (!gem_madvise(kgem->fd, bo->handle,
					 I915_MADV_WILLNEED)) {
				kgem->need_purge |= bo->gpu;
				goto next_bo;
			}

			bo->deleted = 0;
		}

		bo->delta = 0;
		bo->unique_id = kgem_get_unique_id(kgem);
		assert(bo->pitch);
		DBG(("  from inactive: pitch=%d, tiling=%d: handle=%d, id=%d\n",
		     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
		assert(bo->refcnt == 0);
		assert(bo->reusable);
		assert((flags & CREATE_INACTIVE) == 0 || bo->gpu == 0);
		assert((flags & CREATE_INACTIVE) == 0 ||
		       !kgem_busy(kgem, bo->handle));
		return kgem_bo_reference(bo);

next_bo:
		gem_close(kgem->fd, bo->handle);
		list_del(&bo->request);
		free(bo);
		continue;
	}

	if (flags & CREATE_INACTIVE && !list_is_empty(&kgem->requests)) {
		kgem_retire(kgem);
		flags &= ~CREATE_INACTIVE;
		goto skip_active_search;
	}

	handle = gem_create(kgem->fd, size);
	if (handle == 0)
		return NULL;

	bo = __kgem_bo_alloc(handle, size);
	if (!bo) {
		gem_close(kgem->fd, handle);
		return NULL;
	}

	bo->unique_id = kgem_get_unique_id(kgem);
	bo->pitch = pitch;
	if (tiling != I915_TILING_NONE)
		bo->tiling = gem_set_tiling(kgem->fd, handle, tiling, pitch);

	assert (bo->size >= bo->pitch * kgem_aligned_height(kgem, height, bo->tiling));

	DBG(("  new pitch=%d, tiling=%d, handle=%d, id=%d\n",
	     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
	return bo;
}

void _kgem_bo_destroy(struct kgem *kgem, struct kgem_bo *bo)
{
	if (bo->proxy) {
		kgem_bo_unref(kgem, bo->proxy);
		list_del(&bo->request);
		free(bo);
		return;
	}

	__kgem_bo_destroy(kgem, bo);
}

void __kgem_flush(struct kgem *kgem, struct kgem_bo *bo)
{
	/* The kernel will emit a flush *and* update its own flushing lists. */
	kgem_busy(kgem, bo->handle);
}

bool kgem_check_bo(struct kgem *kgem, ...)
{
	va_list ap;
	struct kgem_bo *bo;
	int num_exec = 0;
	int size = 0;

	if (kgem->aperture > kgem->aperture_low)
		return false;

	va_start(ap, kgem);
	while ((bo = va_arg(ap, struct kgem_bo *))) {
		if (bo->exec)
			continue;

		size += bo->size;
		num_exec++;
	}
	va_end(ap);

	if (size + kgem->aperture > kgem->aperture_high)
		return false;

	if (kgem->nexec + num_exec >= KGEM_EXEC_SIZE(kgem))
		return false;

	return true;
}

bool kgem_check_bo_fenced(struct kgem *kgem, ...)
{
	va_list ap;
	struct kgem_bo *bo;
	int num_fence = 0;
	int num_exec = 0;
	int size = 0;
	int fenced_size = 0;

	if (unlikely (kgem->aperture > kgem->aperture_low))
		return false;

	va_start(ap, kgem);
	while ((bo = va_arg(ap, struct kgem_bo *))) {
		if (bo->exec) {
			if (kgem->gen >= 40 || bo->tiling == I915_TILING_NONE)
				continue;

			if ((bo->exec->flags & EXEC_OBJECT_NEEDS_FENCE) == 0) {
				fenced_size += kgem_bo_fenced_size(kgem, bo);
				num_fence++;
			}

			continue;
		}

		size += bo->size;
		num_exec++;
		if (kgem->gen < 40 && bo->tiling) {
			fenced_size += kgem_bo_fenced_size(kgem, bo);
			num_fence++;
		}
	}
	va_end(ap);

	if (fenced_size + kgem->aperture_fenced > kgem->aperture_mappable)
		return false;

	if (size + kgem->aperture > kgem->aperture_high)
		return false;

	if (kgem->nexec + num_exec >= KGEM_EXEC_SIZE(kgem))
		return false;

	if (kgem->nfence + num_fence >= kgem->fence_max)
		return false;

	return true;
}

uint32_t kgem_add_reloc(struct kgem *kgem,
			uint32_t pos,
			struct kgem_bo *bo,
			uint32_t read_write_domain,
			uint32_t delta)
{
	int index;

	assert ((read_write_domain & 0x7fff) == 0 || bo != NULL);

	index = kgem->nreloc++;
	assert(index < ARRAY_SIZE(kgem->reloc));
	kgem->reloc[index].offset = pos * sizeof(kgem->batch[0]);
	if (bo) {
		assert(!bo->deleted);

		delta += bo->delta;
		if (bo->proxy) {
			/* need to release the cache upon batch submit */
			list_move(&bo->request, &kgem->next_request->buffers);
			bo->exec = &_kgem_dummy_exec;
			bo = bo->proxy;
		}

		assert(!bo->deleted);

		if (bo->exec == NULL) {
			_kgem_add_bo(kgem, bo);
			if (bo->needs_flush &&
			    (read_write_domain >> 16) != I915_GEM_DOMAIN_RENDER)
				bo->needs_flush = false;
		}

		if (read_write_domain & KGEM_RELOC_FENCED && kgem->gen < 40) {
			if (bo->tiling &&
			    (bo->exec->flags & EXEC_OBJECT_NEEDS_FENCE) == 0) {
				assert(kgem->nfence < kgem->fence_max);
				kgem->aperture_fenced +=
					kgem_bo_fenced_size(kgem, bo);
				kgem->nfence++;
			}
			bo->exec->flags |= EXEC_OBJECT_NEEDS_FENCE;
		}

		kgem->reloc[index].delta = delta;
		kgem->reloc[index].target_handle = bo->handle;
		kgem->reloc[index].presumed_offset = bo->presumed_offset;

		if (read_write_domain & 0x7fff)
			bo->needs_flush = bo->dirty = true;

		delta += bo->presumed_offset;
	} else {
		kgem->reloc[index].delta = delta;
		kgem->reloc[index].target_handle = 0;
		kgem->reloc[index].presumed_offset = 0;
	}
	kgem->reloc[index].read_domains = read_write_domain >> 16;
	kgem->reloc[index].write_domain = read_write_domain & 0x7fff;

	return delta;
}

void *kgem_bo_map(struct kgem *kgem, struct kgem_bo *bo, int prot)
{
	void *ptr;

	ptr = gem_mmap(kgem->fd, bo->handle, bo->size, prot);
	if (ptr == NULL)
		return NULL;

	bo->needs_flush = false;
	if (prot & PROT_WRITE && bo->gpu)
		kgem_retire(kgem);

	return ptr;
}

uint32_t kgem_bo_flink(struct kgem *kgem, struct kgem_bo *bo)
{
	struct drm_gem_flink flink;
	int ret;

	memset(&flink, 0, sizeof(flink));
	flink.handle = bo->handle;
	ret = drmIoctl(kgem->fd, DRM_IOCTL_GEM_FLINK, &flink);
	if (ret)
		return 0;

	/* Ordinarily giving the name aware makes the buffer non-reusable.
	 * However, we track the lifetime of all clients and their hold
	 * on the buffer, and *presuming* they do not pass it on to a third
	 * party, we track the lifetime accurately.
	 */
	return flink.name;
}

#if defined(USE_VMAP) && defined(I915_PARAM_HAS_VMAP)
static uint32_t gem_vmap(int fd, void *ptr, int size, int read_only)
{
	struct drm_i915_gem_vmap vmap;

	vmap.user_ptr = (uintptr_t)ptr;
	vmap.user_size = size;
	vmap.flags = 0;
	if (read_only)
		vmap.flags |= I915_VMAP_READ_ONLY;

	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_VMAP, &vmap))
		return 0;

	return vmap.handle;
}

struct kgem_bo *kgem_create_map(struct kgem *kgem,
				void *ptr, uint32_t size,
				bool read_only)
{
	struct kgem_bo *bo;
	uint32_t handle;

	if (!kgem->has_vmap)
		return NULL;

	handle = gem_vmap(kgem->fd, ptr, size, read_only);
	if (handle == 0)
		return NULL;

	bo = __kgem_bo_alloc(handle, size);
	if (bo == NULL) {
		gem_close(kgem->fd, handle);
		return NULL;
	}

	bo->reusable = false;
	bo->sync = true;
	DBG(("%s(ptr=%p, size=%d, read_only=%d) => handle=%d\n",
	     __FUNCTION__, ptr, size, read_only, handle));
	return bo;
}
#else
static uint32_t gem_vmap(int fd, void *ptr, int size, int read_only)
{
	return 0;
	(void)fd;
	(void)ptr;
	(void)size;
	(void)read_only;
}

struct kgem_bo *kgem_create_map(struct kgem *kgem,
				void *ptr, uint32_t size,
				bool read_only)
{
	return NULL;
	(void)kgem;
	(void)ptr;
	(void)size;
	(void)read_only;
}
#endif

void kgem_bo_sync(struct kgem *kgem, struct kgem_bo *bo, bool for_write)
{
	struct drm_i915_gem_set_domain set_domain;

	kgem_bo_submit(kgem, bo);
	if (for_write ? bo->cpu_write : bo->cpu_read)
		return;

	set_domain.handle = bo->handle;
	set_domain.read_domains = I915_GEM_DOMAIN_CPU;
	set_domain.write_domain = for_write ? I915_GEM_DOMAIN_CPU : 0;

	drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
	bo->needs_flush = false;
	if (bo->gpu)
		kgem_retire(kgem);
	bo->cpu_read = true;
	if (for_write)
		bo->cpu_write = true;
}

void kgem_clear_dirty(struct kgem *kgem)
{
	struct kgem_request *rq = kgem->next_request;
	struct kgem_bo *bo;

	list_for_each_entry(bo, &rq->buffers, request)
		bo->dirty = false;
}

/* Flush the contents of the RenderCache and invalidate the TextureCache */
void kgem_emit_flush(struct kgem *kgem)
{
	if (kgem->nbatch == 0)
		return;

	if (!kgem_check_batch(kgem,  4)) {
		_kgem_submit(kgem);
		return;
	}

	DBG(("%s()\n", __FUNCTION__));

	if (kgem->ring == KGEM_BLT) {
		kgem->batch[kgem->nbatch++] = MI_FLUSH_DW | 2;
		kgem->batch[kgem->nbatch++] = 0;
		kgem->batch[kgem->nbatch++] = 0;
		kgem->batch[kgem->nbatch++] = 0;
	} else if (kgem->gen >= 50 && 0) {
		kgem->batch[kgem->nbatch++] = PIPE_CONTROL | 2;
		kgem->batch[kgem->nbatch++] =
			PIPE_CONTROL_WC_FLUSH |
			PIPE_CONTROL_TC_FLUSH |
			PIPE_CONTROL_NOWRITE;
		kgem->batch[kgem->nbatch++] = 0;
		kgem->batch[kgem->nbatch++] = 0;
	} else {
		if ((kgem->batch[kgem->nbatch-1] & (0xff<<23)) == MI_FLUSH)
			kgem->nbatch--;
		kgem->batch[kgem->nbatch++] = MI_FLUSH | MI_INVALIDATE_MAP_CACHE;
	}

	kgem_clear_dirty(kgem);
}

struct kgem_bo *kgem_create_proxy(struct kgem_bo *target,
				  int offset, int length)
{
	struct kgem_bo *bo;

	assert(target->proxy == NULL);

	bo = __kgem_bo_alloc(target->handle, length);
	if (bo == NULL)
		return NULL;

	bo->reusable = false;
	bo->proxy = kgem_bo_reference(target);
	bo->delta = offset;
	return bo;
}

#ifndef NDEBUG
static bool validate_partials(struct kgem *kgem)
{
	struct kgem_partial_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &kgem->partial, base.list) {
		if (bo->base.list.next == &kgem->partial)
			return true;
		if (bo->alloc - bo->used < next->alloc - next->used) {
			ErrorF("this rem: %d, next rem: %d\n",
			       bo->alloc - bo->used,
			       next->alloc - next->used);
			goto err;
		}
	}
	return true;

err:
	list_for_each_entry(bo, &kgem->partial, base.list)
		ErrorF("bo: used=%d / %d, rem=%d\n",
		       bo->used, bo->alloc, bo->alloc - bo->used);
	return false;
}
#endif

struct kgem_bo *kgem_create_buffer(struct kgem *kgem,
				   uint32_t size, uint32_t flags,
				   void **ret)
{
	struct kgem_partial_bo *bo;
	bool write = !!(flags & KGEM_BUFFER_WRITE);
	int offset, alloc;
	uint32_t handle;

	DBG(("%s: size=%d, flags=%x [write=%d, last=%d]\n",
	     __FUNCTION__, size, flags, write, flags & KGEM_BUFFER_LAST));

	list_for_each_entry(bo, &kgem->partial, base.list) {
		if (flags == KGEM_BUFFER_LAST && bo->write) {
			/* We can reuse any write buffer which we can fit */
			if (size < bo->alloc) {
				DBG(("%s: reusing write buffer for read of %d bytes? used=%d, total=%d\n",
				     __FUNCTION__, size, bo->used, bo->alloc));
				offset = 0;
				goto done;
			}
		}

		if (bo->write != write) {
			DBG(("%s: skip write %d buffer, need %d\n",
			     __FUNCTION__, bo->write, write));
			continue;
		}

		if (bo->base.refcnt == 1 && bo->base.exec == NULL)
			/* no users, so reset */
			bo->used = 0;

		if (bo->used + size < bo->alloc) {
			DBG(("%s: reusing partial buffer? used=%d + size=%d, total=%d\n",
			     __FUNCTION__, bo->used, size, bo->alloc));
			offset = bo->used;
			bo->used += size;
			goto done;
		}

		DBG(("%s: too small (%d < %d)\n",
		     __FUNCTION__, bo->alloc - bo->used, size));
		break;
	}

	alloc = (flags & KGEM_BUFFER_LAST) ? 4096 : 32 * 1024;
	alloc = ALIGN(size, alloc);

	bo = malloc(sizeof(*bo) + alloc);
	if (bo == NULL)
		return NULL;

	handle = 0;
	if (kgem->has_vmap)
		handle = gem_vmap(kgem->fd, bo+1, alloc, write);
	if (handle == 0) {
		struct kgem_bo *old;

		old = NULL;
		if (!write)
			old = search_linear_cache(kgem, alloc, true);
		if (old == NULL)
			old = search_linear_cache(kgem, alloc, false);
		if (old) {
			memcpy(&bo->base, old, sizeof(*old));
			if (old->rq)
				list_replace(&old->request,
					     &bo->base.request);
			else
				list_init(&bo->base.request);
			free(old);
			bo->base.refcnt = 1;
		} else {
			if (!__kgem_bo_init(&bo->base,
					    gem_create(kgem->fd, alloc),
					    alloc)) {
				free(bo);
				return NULL;
			}
		}
		bo->need_io = true;
	} else {
		__kgem_bo_init(&bo->base, handle, alloc);
		bo->base.sync = true;
		bo->need_io = 0;
	}
	bo->base.reusable = false;

	bo->alloc = alloc;
	bo->used = size;
	bo->write = write;
	offset = 0;

	list_add(&bo->base.list, &kgem->partial);
	DBG(("%s(size=%d) new handle=%d\n",
	     __FUNCTION__, alloc, bo->base.handle));

done:
	/* adjust the position within the list to maintain decreasing order */
	alloc = bo->alloc - bo->used;
	{
		struct kgem_partial_bo *p, *first;

		first = p = list_first_entry(&bo->base.list,
					     struct kgem_partial_bo,
					     base.list);
		while (&p->base.list != &kgem->partial &&
		       alloc < p->alloc - p->used) {
			DBG(("%s: this=%d, right=%d\n",
			     __FUNCTION__, alloc, p->alloc -p->used));
			p = list_first_entry(&p->base.list,
					     struct kgem_partial_bo,
					     base.list);
		}
		if (p != first) {
			__list_del(bo->base.list.prev, bo->base.list.next);
			list_add_tail(&bo->base.list, &p->base.list);
		}
		assert(validate_partials(kgem));
	}
	*ret = (char *)(bo+1) + offset;
	return kgem_create_proxy(&bo->base, offset, size);
}

struct kgem_bo *kgem_upload_source_image(struct kgem *kgem,
					 const void *data,
					 BoxPtr box,
					 int stride, int bpp)
{
	int width = box->x2 - box->x1;
	int height = box->y2 - box->y1;
	int dst_stride = ALIGN(width * bpp, 32) >> 3;
	int size = dst_stride * height;
	struct kgem_bo *bo;
	void *dst;

	DBG(("%s : (%d, %d), (%d, %d), stride=%d, bpp=%d\n",
	     __FUNCTION__, box->x1, box->y1, box->x2, box->y2, stride, bpp));

	bo = kgem_create_buffer(kgem, size, KGEM_BUFFER_WRITE, &dst);
	if (bo == NULL)
		return NULL;

	memcpy_blt(data, dst, bpp,
		   stride, dst_stride,
		   box->x1, box->y1,
		   0, 0,
		   width, height);

	bo->pitch = dst_stride;
	return bo;
}

struct kgem_bo *kgem_upload_source_image_halved(struct kgem *kgem,
						pixman_format_code_t format,
						const void *data,
						int x, int y,
						int width, int height,
						int stride, int bpp)
{
	int dst_stride = ALIGN(width * bpp / 2, 32) >> 3;
	int size = dst_stride * height / 2;
	struct kgem_bo *bo;
	pixman_image_t *src_image, *dst_image;
	pixman_transform_t t;
	void *dst;

	DBG(("%s : (%d, %d), (%d, %d), stride=%d, bpp=%d\n",
	     __FUNCTION__, x, y, width, height, stride, bpp));

	bo = kgem_create_buffer(kgem, size, KGEM_BUFFER_WRITE, &dst);
	if (bo == NULL)
		return NULL;

	dst_image = pixman_image_create_bits(format, width/2, height/2,
					     dst, dst_stride);
	if (dst_image == NULL)
		goto cleanup_bo;

	src_image = pixman_image_create_bits(format, width, height,
					     (uint32_t*)data, stride);
	if (src_image == NULL)
		goto cleanup_dst;

	memset(&t, 0, sizeof(t));
	t.matrix[0][0] = 2 << 16;
	t.matrix[1][1] = 2 << 16;
	t.matrix[2][2] = 1 << 16;
	pixman_image_set_transform(src_image, &t);
	pixman_image_set_filter(src_image, PIXMAN_FILTER_BILINEAR, NULL, 0);

	pixman_image_composite(PIXMAN_OP_SRC,
			       src_image, NULL, dst_image,
			       x, y,
			       0, 0,
			       0, 0,
			       width/2, height/2);

	pixman_image_unref(src_image);
	pixman_image_unref(dst_image);

	bo->pitch = dst_stride;
	return bo;

cleanup_dst:
	pixman_image_unref(dst_image);
cleanup_bo:
	kgem_bo_destroy(kgem, bo);
	return NULL;
}

void kgem_buffer_read_sync(struct kgem *kgem, struct kgem_bo *_bo)
{
	struct kgem_partial_bo *bo;
	uint32_t offset = _bo->delta, length = _bo->size;

	if (_bo->proxy)
		_bo = _bo->proxy;

	bo = (struct kgem_partial_bo *)_bo;

	DBG(("%s(offset=%d, length=%d, sync=%d)\n", __FUNCTION__,
	     offset, length, bo->base.sync));

	if (!bo->base.sync) {
		gem_read(kgem->fd, bo->base.handle,
			 (char *)(bo+1)+offset, length);
		bo->base.needs_flush = false;
		if (bo->base.gpu)
			kgem_retire(kgem);
	} else
		kgem_bo_sync(kgem, &bo->base, false);
}
