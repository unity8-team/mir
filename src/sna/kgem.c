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

#ifdef HAVE_VALGRIND
#include <valgrind.h>
#include <memcheck.h>
#endif

static struct kgem_bo *
search_linear_cache(struct kgem *kgem, unsigned int size, unsigned flags);

static inline void _list_del(struct list *list)
{
	assert(list->prev->next == list);
	assert(list->next->prev == list);
	__list_del(list->prev, list->next);
}

static inline void list_move(struct list *list, struct list *head)
{
	_list_del(list);
	list_add(list, head);
}

static inline void list_move_tail(struct list *list, struct list *head)
{
	_list_del(list);
	list_add_tail(list, head);
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
#define DBG_NO_SEMAPHORES 0
#define DBG_NO_MADV 0
#define DBG_NO_MAP_UPLOAD 0
#define DBG_NO_RELAXED_FENCING 0
#define DBG_DUMP 0

#define NO_CACHE 0

#if DEBUG_KGEM
#undef DBG
#define DBG(x) ErrorF x
#endif

#define PAGE_SIZE 4096
#define PAGE_ALIGN(x) ALIGN(x, PAGE_SIZE)
#define MAX_GTT_VMA_CACHE 512
#define MAX_CPU_VMA_CACHE INT16_MAX
#define MAP_PRESERVE_TIME 10

#define IS_CPU_MAP(ptr) ((uintptr_t)(ptr) & 1)
#define CPU_MAP(ptr) ((void*)((uintptr_t)(ptr) & ~1))
#define MAKE_CPU_MAP(ptr) ((void*)((uintptr_t)(ptr) | 1))

struct kgem_partial_bo {
	struct kgem_bo base;
	void *mem;
	uint32_t used;
	uint32_t need_io : 1;
	uint32_t write : 2;
	uint32_t mmapped : 1;
};

static struct kgem_bo *__kgem_freed_bo;
static struct drm_i915_gem_exec_object2 _kgem_dummy_exec;

#ifndef NDEBUG
static bool validate_partials(struct kgem *kgem)
{
	struct kgem_partial_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &kgem->partial, base.list) {
		if (bo->base.list.next == &kgem->partial)
			return true;
		if (bo->base.size - bo->used < next->base.size - next->used) {
			ErrorF("this rem: %d, next rem: %d\n",
			       bo->base.size - bo->used,
			       next->base.size - next->used);
			goto err;
		}
	}
	return true;

err:
	list_for_each_entry(bo, &kgem->partial, base.list)
		ErrorF("bo: used=%d / %d, rem=%d\n",
		       bo->used, bo->base.size, bo->base.size - bo->used);
	return false;
}
#else
#define validate_partials(kgem) 1
#endif

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

	VG_CLEAR(set_tiling);
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
	void *ptr;

	DBG(("%s(handle=%d, size=%d, prot=%s)\n", __FUNCTION__,
	     handle, size, prot & PROT_WRITE ? "read/write" : "read-only"));

	VG_CLEAR(mmap_arg);
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

	return ptr;
}

static int __gem_write(int fd, uint32_t handle,
		       int offset, int length,
		       const void *src)
{
	struct drm_i915_gem_pwrite pwrite;

	DBG(("%s(handle=%d, offset=%d, len=%d)\n", __FUNCTION__,
	     handle, offset, length));

	VG_CLEAR(pwrite);
	pwrite.handle = handle;
	pwrite.offset = offset;
	pwrite.size = length;
	pwrite.data_ptr = (uintptr_t)src;
	return drmIoctl(fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite);
}

static int gem_write(int fd, uint32_t handle,
		     int offset, int length,
		     const void *src)
{
	struct drm_i915_gem_pwrite pwrite;

	DBG(("%s(handle=%d, offset=%d, len=%d)\n", __FUNCTION__,
	     handle, offset, length));

	VG_CLEAR(pwrite);
	pwrite.handle = handle;
	/* align the transfer to cachelines; fortuitously this is safe! */
	if ((offset | length) & 63) {
		pwrite.offset = offset & ~63;
		pwrite.size = ALIGN(offset+length, 64) - pwrite.offset;
		pwrite.data_ptr = (uintptr_t)src + pwrite.offset - offset;
	} else {
		pwrite.offset = offset;
		pwrite.size = length;
		pwrite.data_ptr = (uintptr_t)src;
	}
	return drmIoctl(fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite);
}

static int gem_read(int fd, uint32_t handle, const void *dst,
		    int offset, int length)
{
	struct drm_i915_gem_pread pread;
	int ret;

	DBG(("%s(handle=%d, len=%d)\n", __FUNCTION__,
	     handle, length));

	VG_CLEAR(pread);
	pread.handle = handle;
	pread.offset = offset;
	pread.size = length;
	pread.data_ptr = (uintptr_t)dst;
	ret = drmIoctl(fd, DRM_IOCTL_I915_GEM_PREAD, &pread);
	if (ret) {
		DBG(("%s: failed, errno=%d\n", __FUNCTION__, errno));
		assert(0);
		return ret;
	}

	VG(VALGRIND_MAKE_MEM_DEFINED(dst, length));
	return 0;
}

static bool
kgem_busy(struct kgem *kgem, int handle)
{
	struct drm_i915_gem_busy busy;

	VG_CLEAR(busy);
	busy.handle = handle;
	busy.busy = !kgem->wedged;
	(void)drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_BUSY, &busy);

	return busy.busy;
}

static void kgem_bo_retire(struct kgem *kgem, struct kgem_bo *bo)
{
	assert(!kgem_busy(kgem, bo->handle));

	if (bo->domain == DOMAIN_GPU)
		kgem_retire(kgem);

	if (bo->exec == NULL) {
		DBG(("%s: retiring bo handle=%d (needed flush? %d)\n",
		     __FUNCTION__, bo->handle, bo->needs_flush));
		bo->rq = NULL;
		list_del(&bo->request);
		bo->needs_flush = bo->flush;
	}
}

Bool kgem_bo_write(struct kgem *kgem, struct kgem_bo *bo,
		   const void *data, int length)
{
	assert(bo->refcnt);
	assert(!bo->purged);
	assert(!kgem_busy(kgem, bo->handle));

	assert(length <= bo->size);
	if (gem_write(kgem->fd, bo->handle, 0, length, data))
		return FALSE;

	DBG(("%s: flush=%d, domain=%d\n", __FUNCTION__, bo->flush, bo->domain));
	kgem_bo_retire(kgem, bo);
	bo->domain = DOMAIN_NONE;
	return TRUE;
}

static uint32_t gem_create(int fd, int size)
{
	struct drm_i915_gem_create create;

#if DEBUG_KGEM
	assert((size & (PAGE_SIZE-1)) == 0);
#endif

	VG_CLEAR(create);
	create.handle = 0;
	create.size = size;
	(void)drmIoctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create);

	return create.handle;
}

static bool
kgem_bo_set_purgeable(struct kgem *kgem, struct kgem_bo *bo)
{
#if DBG_NO_MADV
	return true;
#else
	struct drm_i915_gem_madvise madv;

	assert(bo->exec == NULL);
	assert(!bo->purged);

	VG_CLEAR(madv);
	madv.handle = bo->handle;
	madv.madv = I915_MADV_DONTNEED;
	if (drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_MADVISE, &madv) == 0) {
		bo->purged = 1;
		kgem->need_purge |= !madv.retained && bo->domain == DOMAIN_GPU;
		return madv.retained;
	}

	return true;
#endif
}

static bool
kgem_bo_is_retained(struct kgem *kgem, struct kgem_bo *bo)
{
#if DBG_NO_MADV
	return true;
#else
	struct drm_i915_gem_madvise madv;

	if (!bo->purged)
		return true;

	VG_CLEAR(madv);
	madv.handle = bo->handle;
	madv.madv = I915_MADV_DONTNEED;
	if (drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_MADVISE, &madv) == 0)
		return madv.retained;

	return false;
#endif
}

static bool
kgem_bo_clear_purgeable(struct kgem *kgem, struct kgem_bo *bo)
{
#if DBG_NO_MADV
	return true;
#else
	struct drm_i915_gem_madvise madv;

	assert(bo->purged);

	VG_CLEAR(madv);
	madv.handle = bo->handle;
	madv.madv = I915_MADV_WILLNEED;
	if (drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_MADVISE, &madv) == 0) {
		bo->purged = !madv.retained;
		kgem->need_purge |= !madv.retained && bo->domain == DOMAIN_GPU;
		return madv.retained;
	}

	return false;
#endif
}

static void gem_close(int fd, uint32_t handle)
{
	struct drm_gem_close close;

	VG_CLEAR(close);
	close.handle = handle;
	(void)drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &close);
}

static inline unsigned long __fls(unsigned long word)
{
	asm("bsr %1,%0"
	    : "=r" (word)
	    : "rm" (word));
	return word;
}

constant inline static int cache_bucket(int size)
{
	uint32_t order = __fls(size / PAGE_SIZE);
	assert(order < NUM_CACHE_BUCKETS);
	return order;
}

static struct kgem_bo *__kgem_bo_init(struct kgem_bo *bo,
				      int handle, int size)
{
	assert(size);
	memset(bo, 0, sizeof(*bo));

	bo->refcnt = 1;
	bo->handle = handle;
	bo->size = size;
	bo->bucket = cache_bucket(size);
	assert(bo->size < 1 << (12 + bo->bucket + 1));
	bo->reusable = true;
	bo->domain = DOMAIN_CPU;
	list_init(&bo->request);
	list_init(&bo->list);
	list_init(&bo->vma);

	return bo;
}

static struct kgem_bo *__kgem_bo_alloc(int handle, int size)
{
	struct kgem_bo *bo;

	if (__kgem_freed_bo) {
		bo = __kgem_freed_bo;
		__kgem_freed_bo = *(struct kgem_bo **)bo;
	} else {
		bo = malloc(sizeof(*bo));
		if (bo == NULL)
			return NULL;
	}

	return __kgem_bo_init(bo, handle, size);
}

static struct kgem_request _kgem_static_request;

static struct kgem_request *__kgem_request_alloc(void)
{
	struct kgem_request *rq;

	rq = malloc(sizeof(*rq));
	if (rq == NULL)
		rq = &_kgem_static_request;

	list_init(&rq->buffers);

	return rq;
}

static struct list *inactive(struct kgem *kgem, int size)
{
	return &kgem->inactive[cache_bucket(size)];
}

static struct list *active(struct kgem *kgem, int size)
{
	return &kgem->active[cache_bucket(size)];
}

static size_t
agp_aperture_size(struct pci_device *dev, int gen)
{
	return dev->regions[gen < 30 ? 0 : 2].size;
}

static size_t
cpu_cache_size(void)
{
	FILE *file = fopen("/proc/cpuinfo", "r");
	size_t size = -1;
	if (file) {
		size_t len = 0;
		char *line = NULL;
		while (getline(&line, &len, file) != -1) {
			int mb;
			if (sscanf(line, "cache size : %d KB", &mb) == 1) {
				size = mb * 1024;
				break;
			}
		}
		free(line);
		fclose(file);
	}
	if (size == -1)
		ErrorF("Unknown CPU cache size\n");
	return size;
}

static int gem_param(struct kgem *kgem, int name)
{
	drm_i915_getparam_t gp;
	int v = 0;

	VG_CLEAR(gp);
	gp.param = name;
	gp.value = &v;
	drmIoctl(kgem->fd, DRM_IOCTL_I915_GETPARAM, &gp);

	VG(VALGRIND_MAKE_MEM_DEFINED(&v, sizeof(v)));
	return v;
}

static bool semaphores_enabled(void)
{
	FILE *file;
	bool detected = false;

	if (DBG_NO_SEMAPHORES)
		return false;

	file = fopen("/sys/module/i915/parameters/semaphores", "r");
	if (file) {
		int value;
		if (fscanf(file, "%d", &value) == 1)
			detected = value > 0;
		fclose(file);
	}

	return detected;
}

void kgem_init(struct kgem *kgem, int fd, struct pci_device *dev, int gen)
{
	struct drm_i915_gem_get_aperture aperture;
	unsigned int i, j;

	memset(kgem, 0, sizeof(*kgem));

	kgem->fd = fd;
	kgem->gen = gen;
	kgem->wedged = drmCommandNone(kgem->fd, DRM_I915_GEM_THROTTLE) == -EIO;
	kgem->wedged |= DBG_NO_HW;

	kgem->max_batch_size = ARRAY_SIZE(kgem->batch);
	if (gen == 22)
		/* 865g cannot handle a batch spanning multiple pages */
		kgem->max_batch_size = PAGE_SIZE / sizeof(uint32_t);

	kgem->half_cpu_cache_pages = cpu_cache_size() >> 13;

	list_init(&kgem->partial);
	list_init(&kgem->requests);
	list_init(&kgem->flushing);
	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++)
		list_init(&kgem->inactive[i]);
	for (i = 0; i < ARRAY_SIZE(kgem->active); i++)
		list_init(&kgem->active[i]);
	for (i = 0; i < ARRAY_SIZE(kgem->vma); i++) {
		for (j = 0; j < ARRAY_SIZE(kgem->vma[i].inactive); j++)
			list_init(&kgem->vma[i].inactive[j]);
	}
	kgem->vma[MAP_GTT].count = -MAX_GTT_VMA_CACHE;
	kgem->vma[MAP_CPU].count = -MAX_CPU_VMA_CACHE;

	kgem->next_request = __kgem_request_alloc();

#if defined(USE_VMAP) && defined(I915_PARAM_HAS_VMAP)
	if (!DBG_NO_VMAP)
		kgem->has_vmap = gem_param(kgem, I915_PARAM_HAS_VMAP) > 0;
#endif
	DBG(("%s: using vmap=%d\n", __FUNCTION__, kgem->has_vmap));

	if (gen < 40) {
		if (!DBG_NO_RELAXED_FENCING) {
			kgem->has_relaxed_fencing =
				gem_param(kgem, I915_PARAM_HAS_RELAXED_FENCING);
		}
	} else
		kgem->has_relaxed_fencing = 1;
	DBG(("%s: has relaxed fencing? %d\n", __FUNCTION__,
	     kgem->has_relaxed_fencing));

	kgem->has_semaphores = false;
	if (gen >= 60 && semaphores_enabled())
		kgem->has_semaphores = true;
	DBG(("%s: semaphores enabled? %d\n", __FUNCTION__,
	     kgem->has_semaphores));

	VG_CLEAR(aperture);
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
	DBG(("%s: aperture mappable=%d [%d MiB]\n", __FUNCTION__,
	     kgem->aperture_mappable, kgem->aperture_mappable / (1024*1024)));

	kgem->partial_buffer_size = 64 * 1024;
	while (kgem->partial_buffer_size < kgem->aperture_mappable >> 10)
		kgem->partial_buffer_size *= 2;
	DBG(("%s: partial buffer size=%d [%d KiB]\n", __FUNCTION__,
	     kgem->partial_buffer_size, kgem->partial_buffer_size / 1024));

	kgem->min_alignment = 4;
	if (gen < 60)
		/* XXX workaround an issue where we appear to fail to
		 * disable dual-stream mode */
		kgem->min_alignment = 64;

	kgem->max_object_size = kgem->aperture_mappable / 2;
	if (kgem->max_object_size > kgem->aperture_low)
		kgem->max_object_size = kgem->aperture_low;
	if (kgem->max_object_size > MAX_OBJECT_SIZE)
		kgem->max_object_size = MAX_OBJECT_SIZE;
	DBG(("%s: max object size %d\n", __FUNCTION__, kgem->max_object_size));

	kgem->fence_max = gem_param(kgem, I915_PARAM_NUM_FENCES_AVAIL) - 2;
	if ((int)kgem->fence_max < 0)
		kgem->fence_max = 5; /* minimum safe value for all hw */
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

static uint32_t kgem_untiled_pitch(struct kgem *kgem,
				   uint32_t width, uint32_t bpp,
				   bool scanout)
{
	width = width * bpp >> 3;
	return ALIGN(width, scanout ? 64 : kgem->min_alignment);
}

static uint32_t kgem_surface_size(struct kgem *kgem,
				  bool relaxed_fencing,
				  bool scanout,
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
			tile_width = scanout ? 64 : kgem->min_alignment;
			tile_height = 2;
		}
	} else switch (tiling) {
	default:
	case I915_TILING_NONE:
		tile_width = scanout ? 64 : kgem->min_alignment;
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

	/* If it is too wide for the blitter, don't even bother.  */
	*pitch = ALIGN(width * bpp / 8, tile_width);
	if (kgem->gen < 40) {
		if (tiling != I915_TILING_NONE) {
			if (*pitch > 8192)
				return 0;
			for (size = tile_width; size < *pitch; size <<= 1)
				;
			*pitch = size;
		} else {
			if (*pitch >= 32768)
				return 0;
		}
	} else {
		int limit = 32768;
		if (tiling)
			limit *= 4;
		if (*pitch >= limit)
			return 0;
	}
	height = ALIGN(height, tile_height);
	if (height >= 65536)
		return 0;

	size = *pitch * height;
	if (relaxed_fencing || tiling == I915_TILING_NONE || kgem->gen >= 40)
		return PAGE_ALIGN(size);

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

	DBG(("%s: handle=%d, index=%d\n",
	     __FUNCTION__, bo->handle, kgem->nexec));

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

	list_move(&bo->request, &kgem->next_request->buffers);

	/* XXX is it worth working around gcc here? */
	kgem->flush |= bo->flush;
	kgem->sync |= bo->sync;
	kgem->scanout |= bo->scanout;
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
			kgem->reloc[n].presumed_offset = bo->presumed_offset;
			kgem->batch[kgem->reloc[n].offset/sizeof(kgem->batch[0])] =
				kgem->reloc[n].delta + bo->presumed_offset;
		}
	}
}

static void kgem_bo_binding_free(struct kgem *kgem, struct kgem_bo *bo)
{
	struct kgem_bo_binding *b;

	b = bo->binding.next;
	while (b) {
		struct kgem_bo_binding *next = b->next;
		free (b);
		b = next;
	}
}

static void kgem_bo_release_map(struct kgem *kgem, struct kgem_bo *bo)
{
	int type = IS_CPU_MAP(bo->map);

	DBG(("%s: releasing %s vma for handle=%d, count=%d\n",
	     __FUNCTION__, type ? "CPU" : "GTT",
	     bo->handle, kgem->vma[type].count));

	munmap(CPU_MAP(bo->map), bo->size);
	bo->map = NULL;

	list_del(&bo->vma);
	kgem->vma[type].count--;
}

static void kgem_bo_free(struct kgem *kgem, struct kgem_bo *bo)
{
	DBG(("%s: handle=%d\n", __FUNCTION__, bo->handle));
	assert(bo->refcnt == 0);
	assert(bo->exec == NULL);

	kgem_bo_binding_free(kgem, bo);

	if (bo->map)
		kgem_bo_release_map(kgem, bo);
	assert(list_is_empty(&bo->vma));

	_list_del(&bo->list);
	_list_del(&bo->request);
	gem_close(kgem->fd, bo->handle);

	if (!bo->io) {
		*(struct kgem_bo **)bo = __kgem_freed_bo;
		__kgem_freed_bo = bo;
	} else
		free(bo);
}

inline static void kgem_bo_move_to_inactive(struct kgem *kgem,
					    struct kgem_bo *bo)
{
	assert(!kgem_busy(kgem, bo->handle));
	assert(!bo->proxy);
	assert(!bo->io);
	assert(!bo->needs_flush);
	assert(bo->rq == NULL);
	assert(bo->domain != DOMAIN_GPU);

	list_move(&bo->list, &kgem->inactive[bo->bucket]);
	if (bo->map) {
		int type = IS_CPU_MAP(bo->map);
		if (!type && !kgem_bo_is_mappable(kgem, bo)) {
			list_del(&bo->vma);
			munmap(CPU_MAP(bo->map), bo->size);
			bo->map = NULL;
		}
		if (bo->map) {
			list_move(&bo->vma, &kgem->vma[type].inactive[bo->bucket]);
			kgem->vma[type].count++;
		}
	}

	kgem->need_expire = true;
}

inline static void kgem_bo_remove_from_inactive(struct kgem *kgem,
						struct kgem_bo *bo)
{
	list_del(&bo->list);
	assert(bo->rq == NULL);
	if (bo->map) {
		assert(!list_is_empty(&bo->vma));
		list_del(&bo->vma);
		kgem->vma[IS_CPU_MAP(bo->map)].count--;
	}
}

inline static void kgem_bo_remove_from_active(struct kgem *kgem,
					      struct kgem_bo *bo)
{
	list_del(&bo->list);
	if (bo->rq == &_kgem_static_request)
		list_del(&bo->request);
	assert(list_is_empty(&bo->vma));
}

static void __kgem_bo_destroy(struct kgem *kgem, struct kgem_bo *bo)
{
	DBG(("%s: handle=%d\n", __FUNCTION__, bo->handle));

	assert(list_is_empty(&bo->list));
	assert(bo->refcnt == 0);

	bo->binding.offset = 0;

	if (NO_CACHE)
		goto destroy;

	if (bo->io) {
		struct kgem_bo *base;

		base = malloc(sizeof(*base));
		if (base) {
			DBG(("%s: transferring io handle=%d to bo\n",
			     __FUNCTION__, bo->handle));
			/* transfer the handle to a minimum bo */
			memcpy(base, bo, sizeof (*base));
			base->reusable = true;
			base->io = false;
			list_init(&base->list);
			list_replace(&bo->request, &base->request);
			list_replace(&bo->vma, &base->vma);
			free(bo);
			bo = base;
		}
	}

	if (!bo->reusable) {
		DBG(("%s: handle=%d, not reusable\n",
		     __FUNCTION__, bo->handle));
		goto destroy;
	}

	assert(list_is_empty(&bo->vma));
	assert(list_is_empty(&bo->list));
	assert(bo->vmap == false && bo->sync == false);
	assert(bo->io == false);

	bo->scanout = bo->flush = false;
	if (bo->rq) {
		DBG(("%s: handle=%d -> active\n", __FUNCTION__, bo->handle));
		list_add(&bo->list, &kgem->active[bo->bucket]);
		return;
	}

	assert(bo->exec == NULL);
	assert(list_is_empty(&bo->request));

	if (bo->needs_flush) {
		if ((bo->needs_flush = kgem_busy(kgem, bo->handle))) {
			DBG(("%s: handle=%d -> flushing\n",
			     __FUNCTION__, bo->handle));
			list_add(&bo->request, &kgem->flushing);
			list_add(&bo->list, &kgem->active[bo->bucket]);
			bo->rq = &_kgem_static_request;
			return;
		}

		bo->domain = DOMAIN_NONE;
	}

	if (!IS_CPU_MAP(bo->map)) {
		if (!kgem_bo_set_purgeable(kgem, bo))
			goto destroy;
		DBG(("%s: handle=%d, purged\n",
		     __FUNCTION__, bo->handle));
	}

	DBG(("%s: handle=%d -> inactive\n", __FUNCTION__, bo->handle));
	kgem_bo_move_to_inactive(kgem, bo);
	return;

destroy:
	if (!bo->exec)
		kgem_bo_free(kgem, bo);
}

static void kgem_bo_unref(struct kgem *kgem, struct kgem_bo *bo)
{
	if (--bo->refcnt == 0)
		__kgem_bo_destroy(kgem, bo);
}

bool kgem_retire(struct kgem *kgem)
{
	struct kgem_bo *bo, *next;
	bool retired = false;

	DBG(("%s\n", __FUNCTION__));

	list_for_each_entry_safe(bo, next, &kgem->flushing, request) {
		assert(bo->refcnt == 0);
		assert(bo->rq == &_kgem_static_request);
		assert(bo->domain == DOMAIN_GPU);
		assert(bo->exec == NULL);

		if (kgem_busy(kgem, bo->handle))
			break;

		DBG(("%s: moving %d from flush to inactive\n",
		     __FUNCTION__, bo->handle));
		if (kgem_bo_set_purgeable(kgem, bo)) {
			bo->needs_flush = false;
			bo->domain = DOMAIN_NONE;
			bo->rq = NULL;
			list_del(&bo->request);
			kgem_bo_move_to_inactive(kgem, bo);
		} else
			kgem_bo_free(kgem, bo);

		retired = true;
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

			assert(bo->rq == rq);
			assert(bo->exec == NULL);
			assert(bo->domain == DOMAIN_GPU);

			list_del(&bo->request);
			bo->rq = NULL;

			if (bo->needs_flush)
				bo->needs_flush = kgem_busy(kgem, bo->handle);
			if (!bo->needs_flush)
				bo->domain = DOMAIN_NONE;

			if (bo->refcnt)
				continue;

			if (!bo->reusable) {
				DBG(("%s: closing %d\n",
				     __FUNCTION__, bo->handle));
				kgem_bo_free(kgem, bo);
				continue;
			}

			if (bo->needs_flush) {
				DBG(("%s: moving %d to flushing\n",
				     __FUNCTION__, bo->handle));
				list_add(&bo->request, &kgem->flushing);
				bo->rq = &_kgem_static_request;
			} else if (kgem_bo_set_purgeable(kgem, bo)) {
				DBG(("%s: moving %d to inactive\n",
				     __FUNCTION__, bo->handle));
				kgem_bo_move_to_inactive(kgem, bo);
				retired = true;
			} else {
				DBG(("%s: closing %d\n",
				     __FUNCTION__, bo->handle));
				kgem_bo_free(kgem, bo);
			}
		}

		rq->bo->refcnt--;
		assert(rq->bo->refcnt == 0);
		assert(rq->bo->rq == NULL);
		assert(list_is_empty(&rq->bo->request));
		if (kgem_bo_set_purgeable(kgem, rq->bo)) {
			kgem_bo_move_to_inactive(kgem, rq->bo);
			retired = true;
		} else
			kgem_bo_free(kgem, rq->bo);

		_list_del(&rq->list);
		free(rq);
	}

	kgem->need_retire = !list_is_empty(&kgem->requests);
	if (!kgem->need_retire && kgem->ring)
		kgem->ring = kgem->mode;
	DBG(("%s -- need_retire=%d\n", __FUNCTION__, kgem->need_retire));

	return retired;
}

static void kgem_commit(struct kgem *kgem)
{
	struct kgem_request *rq = kgem->next_request;
	struct kgem_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &rq->buffers, request) {
		DBG(("%s: release handle=%d (proxy? %d), dirty? %d flush? %d\n",
		     __FUNCTION__, bo->handle, bo->proxy != NULL,
		     bo->dirty, bo->needs_flush));

		assert(!bo->purged);
		assert(bo->proxy || bo->rq == rq);

		bo->presumed_offset = bo->exec->offset;
		bo->exec = NULL;

		if (!bo->refcnt && !bo->reusable) {
			kgem_bo_free(kgem, bo);
			continue;
		}

		bo->binding.offset = 0;
		bo->domain = DOMAIN_GPU;
		bo->dirty = false;

		if (bo->proxy) {
			/* proxies are not used for domain tracking */
			list_del(&bo->request);
			bo->rq = NULL;
		}
	}

	if (rq == &_kgem_static_request) {
		struct drm_i915_gem_set_domain set_domain;

		DBG(("%s: syncing due to allocation failure\n", __FUNCTION__));

		VG_CLEAR(set_domain);
		set_domain.handle = rq->bo->handle;
		set_domain.read_domains = I915_GEM_DOMAIN_GTT;
		set_domain.write_domain = I915_GEM_DOMAIN_GTT;
		if (drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain)) {
			DBG(("%s: sync: GPU hang detected\n", __FUNCTION__));
			kgem->wedged = 1;
		}

		kgem_retire(kgem);
		gem_close(kgem->fd, rq->bo->handle);
	} else {
		list_add_tail(&rq->list, &kgem->requests);
		kgem->need_retire = 1;
	}

	kgem->next_request = NULL;
}

static void kgem_close_list(struct kgem *kgem, struct list *head)
{
	while (!list_is_empty(head))
		kgem_bo_free(kgem, list_first_entry(head, struct kgem_bo, list));
}

static void kgem_close_inactive(struct kgem *kgem)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++)
		kgem_close_list(kgem, &kgem->inactive[i]);
}

static void bubble_sort_partial(struct kgem *kgem, struct kgem_partial_bo *bo)
{
	int remain = bo->base.size - bo->used;

	while (bo->base.list.prev != &kgem->partial) {
		struct kgem_partial_bo *p;

		p = list_entry(bo->base.list.prev,
			       struct kgem_partial_bo,
			       base.list);
		if (remain <= p->base.size - p->used)
			break;

		assert(p->base.list.next == &bo->base.list);
		bo->base.list.prev = p->base.list.prev;
		p->base.list.prev->next = &bo->base.list;
		p->base.list.prev = &bo->base.list;

		p->base.list.next = bo->base.list.next;
		bo->base.list.next->prev = &p->base.list;
		bo->base.list.next = &p->base.list;

		assert(p->base.list.next->prev == &p->base.list);
		assert(bo->base.list.prev->next == &bo->base.list);
	}
}

static void kgem_finish_partials(struct kgem *kgem)
{
	struct kgem_partial_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &kgem->partial, base.list) {
		if (!bo->write) {
			assert(bo->base.exec);
			goto decouple;
		}

		assert(bo->base.domain != DOMAIN_GPU);
		if (!bo->base.exec)
			continue;

		if (!bo->used) {
			/* Unless we replace the handle in the execbuffer,
			 * then this bo will become active. So decouple it
			 * from the partial list and track it in the normal
			 * manner.
			 */
			goto decouple;
		}

		assert(bo->base.rq == kgem->next_request);
		if (bo->base.refcnt == 1 && bo->used < bo->base.size / 2) {
			struct kgem_bo *shrink;

			shrink = search_linear_cache(kgem,
						     PAGE_ALIGN(bo->used),
						     CREATE_INACTIVE);
			if (shrink) {
				int n;

				DBG(("%s: used=%d, shrinking %d to %d, handle %d to %d\n",
				     __FUNCTION__,
				     bo->used, bo->base.size, shrink->size,
				     bo->base.handle, shrink->handle));

				assert(bo->used <= shrink->size);
				gem_write(kgem->fd, shrink->handle,
					  0, bo->used, bo->mem);

				for (n = 0; n < kgem->nreloc; n++) {
					if (kgem->reloc[n].target_handle == bo->base.handle) {
						kgem->reloc[n].target_handle = shrink->handle;
						kgem->reloc[n].presumed_offset = shrink->presumed_offset;
						kgem->batch[kgem->reloc[n].offset/sizeof(kgem->batch[0])] =
							kgem->reloc[n].delta + shrink->presumed_offset;
					}
				}

				bo->base.exec->handle = shrink->handle;
				bo->base.exec->offset = shrink->presumed_offset;
				shrink->exec = bo->base.exec;
				shrink->rq = bo->base.rq;
				list_replace(&bo->base.request,
					     &shrink->request);
				list_init(&bo->base.request);
				shrink->needs_flush = bo->base.dirty;

				bo->base.exec = NULL;
				bo->base.rq = NULL;
				bo->base.dirty = false;
				bo->base.needs_flush = false;
				bo->used = 0;

				bubble_sort_partial(kgem, bo);
				continue;
			}
		}

		if (bo->need_io) {
			DBG(("%s: handle=%d, uploading %d/%d\n",
			     __FUNCTION__, bo->base.handle, bo->used, bo->base.size));
			assert(!kgem_busy(kgem, bo->base.handle));
			assert(bo->used <= bo->base.size);
			gem_write(kgem->fd, bo->base.handle,
				  0, bo->used, bo->mem);
			bo->need_io = 0;
		}

		VG(VALGRIND_MAKE_MEM_NOACCESS(bo->mem, bo->base.size));
decouple:
		list_del(&bo->base.list);
		kgem_bo_unref(kgem, &bo->base);
	}

	assert(validate_partials(kgem));
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
			bo->domain = DOMAIN_NONE;
			if (bo->refcnt == 0)
				kgem_bo_free(kgem, bo);
		}

		_list_del(&rq->list);
		free(rq);
	}

	kgem_close_inactive(kgem);
}

static int kgem_batch_write(struct kgem *kgem, uint32_t handle, uint32_t size)
{
	int ret;

	assert(!kgem_busy(kgem, handle));

	/* If there is no surface data, just upload the batch */
	if (kgem->surface == kgem->max_batch_size)
		return gem_write(kgem->fd, handle,
				 0, sizeof(uint32_t)*kgem->nbatch,
				 kgem->batch);

	/* Are the batch pages conjoint with the surface pages? */
	if (kgem->surface < kgem->nbatch + PAGE_SIZE/4) {
		assert(size == sizeof(kgem->batch));
		return gem_write(kgem->fd, handle,
				 0, sizeof(kgem->batch),
				 kgem->batch);
	}

	/* Disjoint surface/batch, upload separately */
	ret = gem_write(kgem->fd, handle,
			0, sizeof(uint32_t)*kgem->nbatch,
			kgem->batch);
	if (ret)
		return ret;

	assert(kgem->nbatch*sizeof(uint32_t) <=
	       sizeof(uint32_t)*kgem->surface - (sizeof(kgem->batch)-size));
	return __gem_write(kgem->fd, handle,
			sizeof(uint32_t)*kgem->surface - (sizeof(kgem->batch)-size),
			sizeof(kgem->batch) - sizeof(uint32_t)*kgem->surface,
			kgem->batch + kgem->surface);
}

void kgem_reset(struct kgem *kgem)
{
	if (kgem->next_request) {
		struct kgem_request *rq = kgem->next_request;

		while (!list_is_empty(&rq->buffers)) {
			struct kgem_bo *bo =
				list_first_entry(&rq->buffers,
						 struct kgem_bo,
						 request);
			list_del(&bo->request);

			bo->binding.offset = 0;
			bo->exec = NULL;
			bo->dirty = false;
			bo->rq = NULL;
			bo->domain = DOMAIN_NONE;

			if (!bo->refcnt) {
				DBG(("%s: discarding handle=%d\n",
				     __FUNCTION__, bo->handle));
				kgem_bo_free(kgem, bo);
			}
		}

		if (kgem->next_request != &_kgem_static_request)
			free(kgem->next_request);
	}

	kgem->nfence = 0;
	kgem->nexec = 0;
	kgem->nreloc = 0;
	kgem->aperture = 0;
	kgem->aperture_fenced = 0;
	kgem->nbatch = 0;
	kgem->surface = kgem->max_batch_size;
	kgem->mode = KGEM_NONE;
	if (kgem->has_semaphores)
		kgem->ring = KGEM_NONE;
	kgem->flush = 0;
	kgem->scanout = 0;

	kgem->next_request = __kgem_request_alloc();

	kgem_sna_reset(kgem);
}

static int compact_batch_surface(struct kgem *kgem)
{
	int size, shrink, n;

	/* See if we can pack the contents into one or two pages */
	size = kgem->max_batch_size - kgem->surface + kgem->nbatch;
	if (size > 2048)
		return sizeof(kgem->batch);
	else if (size > 1024)
		size = 8192, shrink = 2*4096;
	else
		size = 4096, shrink = 3*4096;

	for (n = 0; n < kgem->nreloc; n++) {
		if (kgem->reloc[n].read_domains == I915_GEM_DOMAIN_INSTRUCTION &&
		    kgem->reloc[n].target_handle == 0)
			kgem->reloc[n].delta -= shrink;

		if (kgem->reloc[n].offset >= size)
			kgem->reloc[n].offset -= shrink;
	}

	return size;
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

	assert(kgem->nbatch <= kgem->max_batch_size);
	assert(kgem->nbatch <= kgem->surface);
	assert(kgem->nreloc <= ARRAY_SIZE(kgem->reloc));
	assert(kgem->nexec < ARRAY_SIZE(kgem->exec));
	assert(kgem->nfence <= kgem->fence_max);

	kgem_finish_partials(kgem);

#if DEBUG_BATCH
	__kgem_batch_debug(kgem, batch_end);
#endif

	rq = kgem->next_request;
	if (kgem->surface != kgem->max_batch_size)
		size = compact_batch_surface(kgem);
	else
		size = kgem->nbatch * sizeof(kgem->batch[0]);
	rq->bo = kgem_create_linear(kgem, size);
	if (rq->bo) {
		uint32_t handle = rq->bo->handle;
		int i;

		assert(!rq->bo->needs_flush);

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
		rq->bo->rq = rq; /* useful sanity check */
		list_add(&rq->bo->request, &rq->buffers);

		kgem_fixup_self_relocs(kgem, rq->bo);

		if (kgem_batch_write(kgem, handle, size) == 0) {
			struct drm_i915_gem_execbuffer2 execbuf;
			int ret, retry = 3;

			VG_CLEAR(execbuf);
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
			while (ret == -1 && errno == EBUSY && retry--) {
				drmCommandNone(kgem->fd, DRM_I915_GEM_THROTTLE);
				ret = drmIoctl(kgem->fd,
					       DRM_IOCTL_I915_GEM_EXECBUFFER2,
					       &execbuf);
			}
			if (ret == -1 && (errno == EIO || errno == EBUSY)) {
				DBG(("%s: GPU hang detected\n", __FUNCTION__));
				kgem->wedged = 1;
				ret = 0;
			}
#if !NDEBUG
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
					       found ? found->purged : -1);
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
				FatalError("SNA: failed to submit batchbuffer: ret=%d\n",
					   errno);
			}
#endif

			if (DEBUG_FLUSH_SYNC) {
				struct drm_i915_gem_set_domain set_domain;

				DBG(("%s: debug sync\n", __FUNCTION__));

				VG_CLEAR(set_domain);
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

		kgem_commit(kgem);
	}
	if (kgem->wedged)
		kgem_cleanup(kgem);

	kgem->flush_now = kgem->scanout;
	kgem_reset(kgem);

	assert(kgem->next_request != NULL);
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

		DBG(("%s: discarding unused partial buffer: %d/%d, write? %d\n",
		     __FUNCTION__, bo->used, bo->base.size, bo->write));
		list_del(&bo->base.list);
		kgem_bo_unref(kgem, &bo->base);
	}
}

void kgem_purge_cache(struct kgem *kgem)
{
	struct kgem_bo *bo, *next;
	int i;

	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++) {
		list_for_each_entry_safe(bo, next, &kgem->inactive[i], list)
			if (!kgem_bo_is_retained(kgem, bo))
				kgem_bo_free(kgem, bo);
	}

	kgem->need_purge = false;
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

	if (kgem->need_purge)
		kgem_purge_cache(kgem);

	time(&now);
	expire = 0;

	idle = true;
	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++) {
		idle &= list_is_empty(&kgem->inactive[i]);
		list_for_each_entry(bo, &kgem->inactive[i], list) {
			if (bo->delta) {
				expire = now - MAX_INACTIVE_TIME;
				break;
			}

			bo->delta = now;
		}
	}
	if (idle) {
		DBG(("%s: idle\n", __FUNCTION__));
		kgem->need_expire = false;
		return false;
	}
	if (expire == 0)
		return true;

	idle = true;
	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++) {
		struct list preserve;

		list_init(&preserve);
		while (!list_is_empty(&kgem->inactive[i])) {
			bo = list_last_entry(&kgem->inactive[i],
					     struct kgem_bo, list);

			if (bo->delta > expire) {
				idle = false;
				break;
			}

			if (bo->map && bo->delta + MAP_PRESERVE_TIME > expire) {
				idle = false;
				list_move_tail(&bo->list, &preserve);
			} else {
				count++;
				size += bo->size;
				kgem_bo_free(kgem, bo);
			}
		}
		if (!list_is_empty(&preserve)) {
			preserve.prev->next = kgem->inactive[i].next;
			kgem->inactive[i].next->prev = preserve.prev;
			kgem->inactive[i].next = preserve.next;
			preserve.next->prev = &kgem->inactive[i];
		}
	}

	DBG(("%s: expired %d objects, %d bytes, idle? %d\n",
	     __FUNCTION__, count, size, idle));

	kgem->need_expire = !idle;
	return !idle;
	(void)count;
	(void)size;
}

void kgem_cleanup_cache(struct kgem *kgem)
{
	unsigned int i;

	/* sync to the most recent request */
	if (!list_is_empty(&kgem->requests)) {
		struct kgem_request *rq;
		struct drm_i915_gem_set_domain set_domain;

		rq = list_first_entry(&kgem->requests,
				      struct kgem_request,
				      list);

		DBG(("%s: sync on cleanup\n", __FUNCTION__));

		VG_CLEAR(set_domain);
		set_domain.handle = rq->bo->handle;
		set_domain.read_domains = I915_GEM_DOMAIN_GTT;
		set_domain.write_domain = I915_GEM_DOMAIN_GTT;
		drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
	}

	kgem_retire(kgem);
	kgem_cleanup(kgem);
	kgem_expire_partial(kgem);

	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++) {
		while (!list_is_empty(&kgem->inactive[i]))
			kgem_bo_free(kgem,
				     list_last_entry(&kgem->inactive[i],
						     struct kgem_bo, list));
	}

	kgem->need_purge = false;
	kgem->need_expire = false;
}

static struct kgem_bo *
search_linear_cache(struct kgem *kgem, unsigned int size, unsigned flags)
{
	struct kgem_bo *bo, *next, *first = NULL;
	bool use_active = (flags & CREATE_INACTIVE) == 0;
	struct list *cache;

	if (!use_active &&
	    list_is_empty(inactive(kgem, size)) &&
	    !list_is_empty(active(kgem, size)) &&
	    !kgem_retire(kgem))
		return NULL;

	if (!use_active && flags & (CREATE_CPU_MAP | CREATE_GTT_MAP)) {
		int for_cpu = !!(flags & CREATE_CPU_MAP);
		cache = &kgem->vma[for_cpu].inactive[cache_bucket(size)];
		list_for_each_entry(bo, cache, vma) {
			assert(IS_CPU_MAP(bo->map) == for_cpu);
			assert(bo->bucket == cache_bucket(size));

			if (size > bo->size) {
				DBG(("inactive too small: %d < %d\n",
				     bo->size, size));
				continue;
			}

			if (bo->purged && !kgem_bo_clear_purgeable(kgem, bo)) {
				kgem_bo_free(kgem, bo);
				break;
			}

			if (I915_TILING_NONE != bo->tiling &&
			    gem_set_tiling(kgem->fd, bo->handle,
					   I915_TILING_NONE, 0) != I915_TILING_NONE)
				continue;

			kgem_bo_remove_from_inactive(kgem, bo);

			bo->tiling = I915_TILING_NONE;
			bo->pitch = 0;
			bo->delta = 0;
			DBG(("  %s: found handle=%d (size=%d) in linear vma cache\n",
			     __FUNCTION__, bo->handle, bo->size));
			assert(use_active || bo->domain != DOMAIN_GPU);
			assert(!bo->needs_flush);
			//assert(!kgem_busy(kgem, bo->handle));
			return bo;
		}
	}

	cache = use_active ? active(kgem, size) : inactive(kgem, size);
	list_for_each_entry_safe(bo, next, cache, list) {
		assert(bo->refcnt == 0);
		assert(bo->reusable);
		assert(!!bo->rq == !!use_active);

		if (size > bo->size)
			continue;

		if (use_active && bo->tiling != I915_TILING_NONE)
			continue;

		if (bo->purged && !kgem_bo_clear_purgeable(kgem, bo)) {
			kgem_bo_free(kgem, bo);
			continue;
		}

		if (bo->map) {
			if (flags & (CREATE_CPU_MAP | CREATE_GTT_MAP)) {
				int for_cpu = !!(flags & CREATE_CPU_MAP);
				if (IS_CPU_MAP(bo->map) != for_cpu) {
					if (first != NULL)
						break;

					first = bo;
					continue;
				}
			} else {
				if (first != NULL)
					break;

				first = bo;
				continue;
			}
		} else {
			if (flags & (CREATE_CPU_MAP | CREATE_GTT_MAP)) {
				if (first != NULL)
					break;

				first = bo;
				continue;
			}
		}

		if (I915_TILING_NONE != bo->tiling) {
			if (use_active)
				continue;

			if (gem_set_tiling(kgem->fd, bo->handle,
					   I915_TILING_NONE, 0) != I915_TILING_NONE)
				continue;
		}

		if (use_active)
			kgem_bo_remove_from_active(kgem, bo);
		else
			kgem_bo_remove_from_inactive(kgem, bo);

		bo->tiling = I915_TILING_NONE;
		bo->pitch = 0;
		bo->delta = 0;
		DBG(("  %s: found handle=%d (size=%d) in linear %s cache\n",
		     __FUNCTION__, bo->handle, bo->size,
		     use_active ? "active" : "inactive"));
		assert(use_active || bo->domain != DOMAIN_GPU);
		assert(!bo->needs_flush || use_active);
		//assert(use_active || !kgem_busy(kgem, bo->handle));
		return bo;
	}

	if (first) {
		if (I915_TILING_NONE != first->tiling) {
			if (use_active)
				return NULL;

			if (gem_set_tiling(kgem->fd, first->handle,
					   I915_TILING_NONE, 0) != I915_TILING_NONE)
				return NULL;

			if (first->map)
				kgem_bo_release_map(kgem, first);
		}

		if (use_active)
			kgem_bo_remove_from_active(kgem, first);
		else
			kgem_bo_remove_from_inactive(kgem, first);

		first->tiling = I915_TILING_NONE;
		first->pitch = 0;
		first->delta = 0;
		DBG(("  %s: found handle=%d (size=%d) in linear %s cache\n",
		     __FUNCTION__, first->handle, first->size,
		     use_active ? "active" : "inactive"));
		assert(use_active || first->domain != DOMAIN_GPU);
		assert(!first->needs_flush || use_active);
		//assert(use_active || !kgem_busy(kgem, first->handle));
		return first;
	}

	return NULL;
}

struct kgem_bo *kgem_create_for_name(struct kgem *kgem, uint32_t name)
{
	struct drm_gem_open open_arg;
	struct kgem_bo *bo;

	DBG(("%s(name=%d)\n", __FUNCTION__, name));

	VG_CLEAR(open_arg);
	open_arg.name = name;
	if (drmIoctl(kgem->fd, DRM_IOCTL_GEM_OPEN, &open_arg))
		return NULL;

	DBG(("%s: new handle=%d\n", __FUNCTION__, open_arg.handle));
	bo = __kgem_bo_alloc(open_arg.handle, open_arg.size);
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

	size = PAGE_ALIGN(size);
	bo = search_linear_cache(kgem, size, CREATE_INACTIVE);
	if (bo)
		return kgem_bo_reference(bo);

	handle = gem_create(kgem->fd, size);
	if (handle == 0)
		return NULL;

	DBG(("%s: new handle=%d\n", __FUNCTION__, handle));
	bo = __kgem_bo_alloc(handle, size);
	if (bo == NULL) {
		gem_close(kgem->fd, size);
		return NULL;
	}

	return bo;
}

int kgem_choose_tiling(struct kgem *kgem, int tiling, int width, int height, int bpp)
{
	uint32_t pitch;

	if (DBG_NO_TILING)
		return tiling < 0 ? tiling : I915_TILING_NONE;

	if (kgem->gen < 40) {
		if (tiling) {
			if (width * bpp > 8192 * 8) {
				DBG(("%s: pitch too large for tliing [%d]\n",
				     __FUNCTION__, width*bpp/8));
				tiling = I915_TILING_NONE;
				goto done;
			} else if ((width|height) > 2048) {
				DBG(("%s: large buffer (%dx%d), forcing TILING_X\n",
				     __FUNCTION__, width, height));
				tiling = -I915_TILING_X;
			}
		}
	} else {
		if (width*bpp > (MAXSHORT-512) * 8) {
			DBG(("%s: large pitch [%d], forcing TILING_X\n",
			     __FUNCTION__, width*bpp/8));
			tiling = -I915_TILING_X;
		} else if (tiling && (width|height) > 8192) {
			DBG(("%s: large tiled buffer [%dx%d], forcing TILING_X\n",
			     __FUNCTION__, width, height));
			tiling = -I915_TILING_X;
		}
	}

	/* First check that we can fence the whole object */
	if (tiling &&
	    kgem_surface_size(kgem, false, false,
			      width, height, bpp, tiling,
			      &pitch) > kgem->max_object_size) {
		DBG(("%s: too large (%dx%d) to be fenced, discarding tiling\n",
		     __FUNCTION__, width, height));
		tiling = I915_TILING_NONE;
		goto done;
	}

	if (tiling < 0)
		return tiling;

	if (tiling == I915_TILING_Y && height <= 16) {
		DBG(("%s: too short [%d] for TILING_Y\n",
		     __FUNCTION__,height));
		tiling = I915_TILING_X;
	}
	if (tiling && width * bpp > 8 * (4096 - 64)) {
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
		goto done;
	}

	if (tiling == I915_TILING_X && width * bpp <= 8*512/2) {
		DBG(("%s: too thin [width %d, %d bpp] for TILING_X\n",
		     __FUNCTION__, width, bpp));
		tiling = I915_TILING_NONE;
		goto done;
	}
	if (tiling == I915_TILING_Y && width * bpp <= 8*128/2) {
		DBG(("%s: too thin [%d] for TILING_Y\n",
		     __FUNCTION__, width));
		tiling = I915_TILING_NONE;
		goto done;
	}

	if (tiling && ALIGN(height, 2) * ALIGN(width*bpp, 8*64) <= 4096 * 8) {
		DBG(("%s: too small [%d bytes] for TILING_%c\n", __FUNCTION__,
		     ALIGN(height, 2) * ALIGN(width*bpp, 8*64) / 8,
		     tiling == I915_TILING_X ? 'X' : 'Y'));
		tiling = I915_TILING_NONE;
		goto done;
	}

	if (tiling && width * bpp >= 8 * 4096 / 2) {
		DBG(("%s: TLB near-miss between lines %dx%d (pitch=%d), forcing tiling %d\n",
		     __FUNCTION__,
		     width, height, width*bpp/8,
		     tiling));
		return -tiling;
	}

done:
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

	size = kgem_surface_size(kgem, false, false,
				 width, height, bpp, tiling, &pitch);
	if (size == 0 || size >= kgem->max_object_size)
		size = kgem_surface_size(kgem, false, false,
					 width, height, bpp,
					 I915_TILING_NONE, &pitch);
	return size > 0 && size < kgem->max_object_size;
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

inline int kgem_bo_fenced_size(struct kgem *kgem, struct kgem_bo *bo)
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
	uint32_t pitch, untiled_pitch, tiled_height[3], size;
	uint32_t handle;
	int i;

	if (tiling < 0)
		tiling = -tiling, flags |= CREATE_EXACT;

	DBG(("%s(%dx%d, bpp=%d, tiling=%d, exact=%d, inactive=%d, cpu-mapping=%d, gtt-mapping=%d, scanout?=%d)\n", __FUNCTION__,
	     width, height, bpp, tiling,
	     !!(flags & CREATE_EXACT),
	     !!(flags & CREATE_INACTIVE),
	     !!(flags & CREATE_CPU_MAP),
	     !!(flags & CREATE_GTT_MAP),
	     !!(flags & CREATE_SCANOUT)));

	assert(_kgem_can_create_2d(kgem, width, height, bpp, flags & CREATE_EXACT ? -tiling : tiling));
	size = kgem_surface_size(kgem,
				 kgem->has_relaxed_fencing,
				 flags & CREATE_SCANOUT,
				 width, height, bpp, tiling, &pitch);
	assert(size && size <= kgem->max_object_size);

	if (flags & (CREATE_CPU_MAP | CREATE_GTT_MAP)) {
		int for_cpu = !!(flags & CREATE_CPU_MAP);
		/* We presume that we will need to upload to this bo,
		 * and so would prefer to have an active VMA.
		 */
		cache = &kgem->vma[for_cpu].inactive[cache_bucket(size)];
		do {
			list_for_each_entry(bo, cache, vma) {
				assert(bo->bucket == cache_bucket(size));
				assert(bo->refcnt == 0);
				assert(bo->map);
				assert(IS_CPU_MAP(bo->map) == for_cpu);
				assert(bo->rq == NULL);
				assert(list_is_empty(&bo->request));

				if (size > bo->size) {
					DBG(("inactive too small: %d < %d\n",
					     bo->size, size));
					continue;
				}

				if (bo->tiling != tiling ||
				    (tiling != I915_TILING_NONE && bo->pitch != pitch)) {
					DBG(("inactive vma with wrong tiling: %d < %d\n",
					     bo->tiling, tiling));
					continue;
				}

				if (bo->purged && !kgem_bo_clear_purgeable(kgem, bo)) {
					kgem_bo_free(kgem, bo);
					break;
				}

				bo->pitch = pitch;
				bo->delta = 0;
				bo->unique_id = kgem_get_unique_id(kgem);

				kgem_bo_remove_from_inactive(kgem, bo);

				DBG(("  from inactive vma: pitch=%d, tiling=%d: handle=%d, id=%d\n",
				     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
				assert(bo->reusable);
				assert(bo->domain != DOMAIN_GPU && !kgem_busy(kgem, bo->handle));
				return kgem_bo_reference(bo);
			}
		} while (!list_is_empty(cache) && kgem_retire(kgem));
	}

	if (flags & CREATE_INACTIVE)
		goto skip_active_search;

	untiled_pitch = kgem_untiled_pitch(kgem,
					   width, bpp,
					   flags & CREATE_SCANOUT);
	for (i = 0; i <= I915_TILING_Y; i++)
		tiled_height[i] = kgem_aligned_height(kgem, height, i);

	next = NULL;
	cache = active(kgem, size);
search_active: /* Best active match first */
	list_for_each_entry(bo, cache, list) {
		uint32_t s;

		assert(bo->bucket == cache_bucket(size));

		if (bo->tiling) {
			if (bo->pitch < pitch) {
				DBG(("tiled and pitch too small: tiling=%d, (want %d), pitch=%d, need %d\n",
				     bo->tiling, tiling,
				     bo->pitch, pitch));
				continue;
			}
		} else
			bo->pitch = untiled_pitch;

		s = bo->pitch * tiled_height[bo->tiling];
		if (s <= bo->size) {
			if (bo->tiling != tiling) {
				if (next == NULL && bo->tiling < tiling)
					next = bo;
				continue;
			}

			if (bo->purged && !kgem_bo_clear_purgeable(kgem, bo)) {
				kgem_bo_free(kgem, bo);
				bo = NULL;
				goto search_active;
			}

			kgem_bo_remove_from_active(kgem, bo);

			bo->unique_id = kgem_get_unique_id(kgem);
			bo->delta = 0;
			DBG(("  1:from active: pitch=%d, tiling=%d, handle=%d, id=%d\n",
			     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
			assert(bo->refcnt == 0);
			assert(bo->reusable);
			return kgem_bo_reference(bo);
		}
	}

	if (next && (flags & CREATE_EXACT) == 0) {
		list_del(&next->list);
		if (next->rq == &_kgem_static_request)
			list_del(&next->request);

		if (next->purged && !kgem_bo_clear_purgeable(kgem, next)) {
			kgem_bo_free(kgem, next);
		} else {
			kgem_bo_remove_from_active(kgem, next);

			next->unique_id = kgem_get_unique_id(kgem);
			next->delta = 0;
			DBG(("  2:from active: pitch=%d, tiling=%d, handle=%d, id=%d\n",
			     next->pitch, next->tiling, next->handle, next->unique_id));
			assert(next->refcnt == 0);
			assert(next->reusable);
			return kgem_bo_reference(next);
		}
	}

skip_active_search:
	/* Now just look for a close match and prefer any currently active */
	cache = inactive(kgem, size);
	list_for_each_entry_safe(bo, next, cache, list) {
		assert(bo->bucket == cache_bucket(size));

		if (size > bo->size) {
			DBG(("inactive too small: %d < %d\n",
			     bo->size, size));
			continue;
		}

		if (bo->tiling != tiling ||
		    (tiling != I915_TILING_NONE && bo->pitch != pitch)) {
			if (tiling != gem_set_tiling(kgem->fd,
						     bo->handle,
						     tiling, pitch)) {
				kgem_bo_free(kgem, bo);
				continue;
			}

			if (bo->map)
				kgem_bo_release_map(kgem, bo);
		}

		if (bo->purged && !kgem_bo_clear_purgeable(kgem, bo)) {
			kgem_bo_free(kgem, bo);
			continue;
		}

		kgem_bo_remove_from_inactive(kgem, bo);

		bo->pitch = pitch;
		bo->tiling = tiling;

		bo->delta = 0;
		bo->unique_id = kgem_get_unique_id(kgem);
		assert(bo->pitch);
		DBG(("  from inactive: pitch=%d, tiling=%d: handle=%d, id=%d\n",
		     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
		assert(bo->refcnt == 0);
		assert(bo->reusable);
		assert((flags & CREATE_INACTIVE) == 0 || bo->domain != DOMAIN_GPU);
		assert((flags & CREATE_INACTIVE) == 0 ||
		       !kgem_busy(kgem, bo->handle));
		return kgem_bo_reference(bo);
	}

	if (flags & CREATE_INACTIVE && !list_is_empty(&kgem->requests)) {
		if (kgem_retire(kgem)) {
			flags &= ~CREATE_INACTIVE;
			goto skip_active_search;
		}
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

	assert(bo->size >= bo->pitch * kgem_aligned_height(kgem, height, bo->tiling));

	DBG(("  new pitch=%d, tiling=%d, handle=%d, id=%d\n",
	     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
	return bo;
}

static void _kgem_bo_delete_partial(struct kgem *kgem, struct kgem_bo *bo)
{
	struct kgem_partial_bo *io = (struct kgem_partial_bo *)bo->proxy;

	if (list_is_empty(&io->base.list))
		return;

	DBG(("%s: size=%d, offset=%d, parent used=%d\n",
	     __FUNCTION__, bo->size, bo->delta, io->used));

	if (bo->delta + bo->size == io->used) {
		io->used = bo->delta;
		bubble_sort_partial(kgem, io);
	}
}

void _kgem_bo_destroy(struct kgem *kgem, struct kgem_bo *bo)
{
	if (bo->proxy) {
		assert(bo->map == NULL);
		if (bo->io && bo->exec == NULL)
			_kgem_bo_delete_partial(kgem, bo);
		kgem_bo_unref(kgem, bo->proxy);
		kgem_bo_binding_free(kgem, bo);
		_list_del(&bo->request);
		free(bo);
		return;
	}

	if (bo->vmap)
		kgem_bo_sync__cpu(kgem, bo);

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

	DBG(("%s: handle=%d, pos=%d, delta=%d, domains=%08x\n",
	     __FUNCTION__, bo ? bo->handle : 0, pos, delta, read_write_domain));

	assert((read_write_domain & 0x7fff) == 0 || bo != NULL);

	index = kgem->nreloc++;
	assert(index < ARRAY_SIZE(kgem->reloc));
	kgem->reloc[index].offset = pos * sizeof(kgem->batch[0]);
	if (bo) {
		assert(bo->refcnt);
		assert(!bo->purged);

		delta += bo->delta;
		if (bo->proxy) {
			DBG(("%s: adding proxy for handle=%d\n",
			     __FUNCTION__, bo->handle));
			assert(bo->handle == bo->proxy->handle);
			/* need to release the cache upon batch submit */
			list_move(&bo->request, &kgem->next_request->buffers);
			bo->exec = &_kgem_dummy_exec;
			bo = bo->proxy;
		}

		assert(!bo->purged);

		if (bo->exec == NULL)
			_kgem_add_bo(kgem, bo);

		if (kgem->gen < 40 && read_write_domain & KGEM_RELOC_FENCED) {
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

		if (read_write_domain & 0x7fff) {
			DBG(("%s: marking handle=%d dirty\n",
			     __FUNCTION__, bo->handle));
			bo->needs_flush = bo->dirty = true;
		}

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

static void kgem_trim_vma_cache(struct kgem *kgem, int type, int bucket)
{
	int i, j;

	if (kgem->vma[type].count <= 0)
	       return;

	if (kgem->need_purge)
		kgem_purge_cache(kgem);

	/* vma are limited on a per-process basis to around 64k.
	 * This includes all malloc arenas as well as other file
	 * mappings. In order to be fair and not hog the cache,
	 * and more importantly not to exhaust that limit and to
	 * start failing mappings, we keep our own number of open
	 * vma to within a conservative value.
	 */
	i = 0;
	while (kgem->vma[type].count > 0) {
		struct kgem_bo *bo = NULL;

		for (j = 0;
		     bo == NULL && j < ARRAY_SIZE(kgem->vma[type].inactive);
		     j++) {
			struct list *head = &kgem->vma[type].inactive[i++%ARRAY_SIZE(kgem->vma[type].inactive)];
			if (!list_is_empty(head))
				bo = list_last_entry(head, struct kgem_bo, vma);
		}
		if (bo == NULL)
			break;

		DBG(("%s: discarding inactive %s vma cache for %d\n",
		     __FUNCTION__,
		     IS_CPU_MAP(bo->map) ? "CPU" : "GTT", bo->handle));
		assert(IS_CPU_MAP(bo->map) == type);
		assert(bo->map);
		assert(bo->rq == NULL);

		munmap(CPU_MAP(bo->map), bo->size);
		bo->map = NULL;
		list_del(&bo->vma);
		kgem->vma[type].count--;

		if (!bo->purged && !kgem_bo_set_purgeable(kgem, bo))
			kgem_bo_free(kgem, bo);
	}
}

void *kgem_bo_map(struct kgem *kgem, struct kgem_bo *bo)
{
	void *ptr;

	assert(!bo->purged);
	assert(bo->exec == NULL);
	assert(list_is_empty(&bo->list));

	if (IS_CPU_MAP(bo->map))
		kgem_bo_release_map(kgem, bo);

	ptr = bo->map;
	if (ptr == NULL) {
		kgem_trim_vma_cache(kgem, MAP_GTT, bo->bucket);

		ptr = gem_mmap(kgem->fd, bo->handle, bo->size,
			       PROT_READ | PROT_WRITE);
		if (ptr == NULL)
			return NULL;

		/* Cache this mapping to avoid the overhead of an
		 * excruciatingly slow GTT pagefault. This is more an
		 * issue with compositing managers which need to frequently
		 * flush CPU damage to their GPU bo.
		 */
		bo->map = ptr;
		DBG(("%s: caching GTT vma for %d\n", __FUNCTION__, bo->handle));
	}

	if (bo->domain != DOMAIN_GTT) {
		struct drm_i915_gem_set_domain set_domain;

		DBG(("%s: sync: needs_flush? %d, domain? %d\n", __FUNCTION__,
		     bo->needs_flush, bo->domain));

		/* XXX use PROT_READ to avoid the write flush? */

		VG_CLEAR(set_domain);
		set_domain.handle = bo->handle;
		set_domain.read_domains = I915_GEM_DOMAIN_GTT;
		set_domain.write_domain = I915_GEM_DOMAIN_GTT;
		drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);

		kgem_bo_retire(kgem, bo);
		bo->domain = DOMAIN_GTT;
	}

	return ptr;
}

void *kgem_bo_map__debug(struct kgem *kgem, struct kgem_bo *bo)
{
	if (bo->map)
		return bo->map;

	kgem_trim_vma_cache(kgem, MAP_GTT, bo->bucket);
	return bo->map = gem_mmap(kgem->fd, bo->handle, bo->size,
				  PROT_READ | PROT_WRITE);
}

void *kgem_bo_map__cpu(struct kgem *kgem, struct kgem_bo *bo)
{
	struct drm_i915_gem_mmap mmap_arg;

	DBG(("%s(handle=%d, size=%d)\n", __FUNCTION__, bo->handle, bo->size));
	assert(!bo->purged);
	assert(list_is_empty(&bo->list));

	if (IS_CPU_MAP(bo->map))
		return CPU_MAP(bo->map);

	if (bo->map)
		kgem_bo_release_map(kgem, bo);

	kgem_trim_vma_cache(kgem, MAP_CPU, bo->bucket);

	VG_CLEAR(mmap_arg);
	mmap_arg.handle = bo->handle;
	mmap_arg.offset = 0;
	mmap_arg.size = bo->size;
	if (drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_MMAP, &mmap_arg)) {
		assert(0);
		return NULL;
	}

	DBG(("%s: caching CPU vma for %d\n", __FUNCTION__, bo->handle));
	bo->map = MAKE_CPU_MAP(mmap_arg.addr_ptr);
	return (void *)(uintptr_t)mmap_arg.addr_ptr;
}

uint32_t kgem_bo_flink(struct kgem *kgem, struct kgem_bo *bo)
{
	struct drm_gem_flink flink;
	int ret;

	VG_CLEAR(flink);
	flink.handle = bo->handle;
	ret = drmIoctl(kgem->fd, DRM_IOCTL_GEM_FLINK, &flink);
	if (ret)
		return 0;

	/* Ordinarily giving the name aware makes the buffer non-reusable.
	 * However, we track the lifetime of all clients and their hold
	 * on the buffer, and *presuming* they do not pass it on to a third
	 * party, we track the lifetime accurately.
	 */
	bo->reusable = false;

	/* The bo is outside of our control, so presume it is written to */
	bo->needs_flush = true;

	/* Henceforth, we need to broadcast all updates to clients and
	 * flush our rendering before doing so.
	 */
	bo->flush = true;
	if (bo->exec)
		kgem->flush = 1;

	return flink.name;
}

#if defined(USE_VMAP) && defined(I915_PARAM_HAS_VMAP)
static uint32_t gem_vmap(int fd, void *ptr, int size, int read_only)
{
	struct drm_i915_gem_vmap vmap;

	VG_CLEAR(vmap);
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
	bo->vmap = true;
	bo->sync = true;

	DBG(("%s(ptr=%p, size=%d, read_only=%d) => handle=%d\n",
	     __FUNCTION__, ptr, size, read_only, handle));
	return bo;
}
#else
struct kgem_bo *kgem_create_map(struct kgem *kgem,
				void *ptr, uint32_t size,
				bool read_only)
{
	return 0;
}
#endif

void kgem_bo_sync__cpu(struct kgem *kgem, struct kgem_bo *bo)
{
	kgem_bo_submit(kgem, bo);

	/* XXX assumes bo is snoopable */
	if (bo->domain != DOMAIN_CPU) {
		struct drm_i915_gem_set_domain set_domain;

		DBG(("%s: sync: needs_flush? %d, domain? %d, busy? %d\n", __FUNCTION__,
		     bo->needs_flush, bo->domain, kgem_busy(kgem, bo->handle)));

		VG_CLEAR(set_domain);
		set_domain.handle = bo->handle;
		set_domain.read_domains = I915_GEM_DOMAIN_CPU;
		set_domain.write_domain = I915_GEM_DOMAIN_CPU;

		drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);

		kgem_bo_retire(kgem, bo);
		bo->domain = DOMAIN_CPU;
	}
}

void kgem_sync(struct kgem *kgem)
{
	DBG(("%s\n", __FUNCTION__));

	if (!list_is_empty(&kgem->requests)) {
		struct drm_i915_gem_set_domain set_domain;
		struct kgem_request *rq;

		rq = list_first_entry(&kgem->requests,
				      struct kgem_request,
				      list);

		VG_CLEAR(set_domain);
		set_domain.handle = rq->bo->handle;
		set_domain.read_domains = I915_GEM_DOMAIN_GTT;
		set_domain.write_domain = I915_GEM_DOMAIN_GTT;

		drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
		kgem_retire(kgem);
	}

	kgem->sync = false;
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
	DBG(("%s: target handle=%d, offset=%d, length=%d, io=%d\n",
	     __FUNCTION__, target->handle, offset, length, target->io));

	bo = __kgem_bo_alloc(target->handle, length);
	if (bo == NULL)
		return NULL;

	bo->io = target->io;
	bo->reusable = false;
	bo->proxy = kgem_bo_reference(target);
	bo->delta = offset;
	return bo;
}

static struct kgem_partial_bo *partial_bo_alloc(int size)
{
	struct kgem_partial_bo *bo;

	bo = malloc(sizeof(*bo) + 128 + size);
	if (bo) {
		bo->mem = (void *)ALIGN((uintptr_t)bo + sizeof(*bo), 64);
		bo->mmapped = false;
	}

	return bo;
}

struct kgem_bo *kgem_create_buffer(struct kgem *kgem,
				   uint32_t size, uint32_t flags,
				   void **ret)
{
	struct kgem_partial_bo *bo;
	unsigned offset, alloc;

	DBG(("%s: size=%d, flags=%x [write?=%d, inplace?=%d, last?=%d]\n",
	     __FUNCTION__, size, flags,
	     !!(flags & KGEM_BUFFER_WRITE),
	     !!(flags & KGEM_BUFFER_INPLACE),
	     !!(flags & KGEM_BUFFER_LAST)));
	assert(size);

	list_for_each_entry(bo, &kgem->partial, base.list) {
		if (flags == KGEM_BUFFER_LAST && bo->write) {
			/* We can reuse any write buffer which we can fit */
			if (size <= bo->base.size) {
				if (bo->base.refcnt == 1 && bo->base.exec) {
					DBG(("%s: reusing write buffer for read of %d bytes? used=%d, total=%d\n",
					     __FUNCTION__, size, bo->used, bo->base.size));
					offset = 0;
					goto done;
				} else if (bo->used + size <= bo->base.size) {
					DBG(("%s: reusing unfinished write buffer for read of %d bytes? used=%d, total=%d\n",
					     __FUNCTION__, size, bo->used, bo->base.size));
					offset = bo->used;
					goto done;
				}
			}
		}

		if ((bo->write & KGEM_BUFFER_WRITE) != (flags & KGEM_BUFFER_WRITE) ||
		    (bo->write & ~flags) & KGEM_BUFFER_INPLACE) {
			DBG(("%s: skip write %x buffer, need %x\n",
			     __FUNCTION__, bo->write, flags));
			continue;
		}

		if (bo->used + size <= bo->base.size) {
			DBG(("%s: reusing partial buffer? used=%d + size=%d, total=%d\n",
			     __FUNCTION__, bo->used, size, bo->base.size));
			offset = bo->used;
			bo->used += size;
			goto done;
		}

		DBG(("%s: too small (%d < %d)\n",
		     __FUNCTION__, bo->base.size - bo->used, size));
		break;
	}

	/* Be a little more generous and hope to hold fewer mmappings */
	alloc = ALIGN(size, kgem->partial_buffer_size);
	bo = NULL;

#if !DBG_NO_MAP_UPLOAD
	if (!DEBUG_NO_LLC && kgem->gen >= 60) {
		struct kgem_bo *old;

		bo = malloc(sizeof(*bo));
		if (bo == NULL)
			return NULL;

		old = NULL;
		if ((flags & KGEM_BUFFER_WRITE) == 0)
			old = search_linear_cache(kgem, alloc, CREATE_CPU_MAP);
		if (old == NULL)
			old = search_linear_cache(kgem, alloc, CREATE_INACTIVE | CREATE_CPU_MAP);
		if (old) {
			DBG(("%s: reusing handle=%d for buffer\n",
			     __FUNCTION__, old->handle));

			memcpy(&bo->base, old, sizeof(*old));
			if (old->rq)
				list_replace(&old->request, &bo->base.request);
			else
				list_init(&bo->base.request);
			list_replace(&old->vma, &bo->base.vma);
			list_init(&bo->base.list);
			free(old);
			bo->base.refcnt = 1;
		} else {
			if (!__kgem_bo_init(&bo->base,
					    gem_create(kgem->fd, alloc),
					    alloc)) {
				free(bo);
				return NULL;
			}
			DBG(("%s: created handle=%d for buffer\n",
			     __FUNCTION__, bo->base.handle));
		}

		bo->mem = kgem_bo_map__cpu(kgem, &bo->base);
		if (bo->mem) {
			if (flags & KGEM_BUFFER_WRITE)
				kgem_bo_sync__cpu(kgem, &bo->base);

			bo->need_io = false;
			bo->base.io = true;
			bo->mmapped = true;

			alloc = bo->base.size;
		} else {
			bo->base.refcnt = 0; /* for valgrind */
			kgem_bo_free(kgem, &bo->base);
			bo = NULL;
		}
	} else if ((flags & KGEM_BUFFER_WRITE_INPLACE) == KGEM_BUFFER_WRITE_INPLACE) {
		struct kgem_bo *old;

		/* The issue with using a GTT upload buffer is that we may
		 * cause eviction-stalls in order to free up some GTT space.
		 * An is-mappable? ioctl could help us detect when we are
		 * about to block, or some per-page magic in the kernel.
		 *
		 * XXX This is especially noticeable on memory constrained
		 * devices like gen2 or with relatively slow gpu like i3.
		 */
		old = search_linear_cache(kgem, alloc,
					  CREATE_INACTIVE | CREATE_GTT_MAP);
#if HAVE_I915_GEM_BUFFER_INFO
		if (old) {
			struct drm_i915_gem_buffer_info info;

			/* An example of such a non-blocking ioctl might work */

			VG_CLEAR(info);
			info.handle = handle;
			if (drmIoctl(kgem->fd,
				     DRM_IOCTL_I915_GEM_BUFFER_INFO,
				     &fino) == 0) {
				old->presumed_offset = info.addr;
				if ((info.flags & I915_GEM_MAPPABLE) == 0) {
					kgem_bo_move_to_inactive(kgem, old);
					old = NULL;
				}
			}
		}
#endif
		if (old) {
			DBG(("%s: reusing handle=%d for buffer\n",
			     __FUNCTION__, old->handle));

			bo = malloc(sizeof(*bo));
			if (bo == NULL)
				return NULL;

			memcpy(&bo->base, old, sizeof(*old));
			if (old->rq)
				list_replace(&old->request, &bo->base.request);
			else
				list_init(&bo->base.request);
			list_replace(&old->vma, &bo->base.vma);
			list_init(&bo->base.list);
			free(old);

			bo->mem = kgem_bo_map(kgem, &bo->base);
			if (bo->mem) {
				bo->need_io = false;
				bo->base.io = true;
				bo->mmapped = true;
				bo->base.refcnt = 1;

				alloc = bo->base.size;
			} else {
				kgem_bo_free(kgem, &bo->base);
				bo = NULL;
			}
		}
	}
#endif

	if (bo == NULL) {
		struct kgem_bo *old;

		/* Be more parsimonious with pwrite/pread buffers */
		if ((flags & KGEM_BUFFER_INPLACE) == 0)
			alloc = PAGE_ALIGN(size);
		flags &= ~KGEM_BUFFER_INPLACE;

		old = NULL;
		if ((flags & KGEM_BUFFER_WRITE) == 0)
			old = search_linear_cache(kgem, alloc, 0);
		if (old == NULL)
			old = search_linear_cache(kgem, alloc, CREATE_INACTIVE);
		if (old) {
			alloc = old->size;
			bo = partial_bo_alloc(alloc);
			if (bo == NULL)
				return NULL;

			memcpy(&bo->base, old, sizeof(*old));
			if (old->rq)
				list_replace(&old->request,
					     &bo->base.request);
			else
				list_init(&bo->base.request);
			list_replace(&old->vma, &bo->base.vma);
			list_init(&bo->base.list);
			free(old);
			bo->base.refcnt = 1;
		} else {
			bo = partial_bo_alloc(alloc);
			if (bo == NULL)
				return NULL;

			if (!__kgem_bo_init(&bo->base,
					    gem_create(kgem->fd, alloc),
					    alloc)) {
				free(bo);
				return NULL;
			}
		}
		bo->need_io = flags & KGEM_BUFFER_WRITE;
		bo->base.io = true;
	}
	bo->base.reusable = false;
	assert(bo->base.size == alloc);
	assert(!bo->need_io || !bo->base.needs_flush);
	assert(!bo->need_io || bo->base.domain != DOMAIN_GPU);

	bo->used = size;
	bo->write = flags & KGEM_BUFFER_WRITE_INPLACE;
	offset = 0;

	list_add(&bo->base.list, &kgem->partial);
	DBG(("%s(size=%d) new handle=%d\n",
	     __FUNCTION__, alloc, bo->base.handle));

done:
	/* adjust the position within the list to maintain decreasing order */
	alloc = bo->base.size - bo->used;
	{
		struct kgem_partial_bo *p, *first;

		first = p = list_first_entry(&bo->base.list,
					     struct kgem_partial_bo,
					     base.list);
		while (&p->base.list != &kgem->partial &&
		       alloc < p->base.size - p->used) {
			DBG(("%s: this=%d, right=%d\n",
			     __FUNCTION__, alloc, p->base.size -p->used));
			p = list_first_entry(&p->base.list,
					     struct kgem_partial_bo,
					     base.list);
		}
		if (p != first)
			list_move_tail(&bo->base.list, &p->base.list);
		assert(validate_partials(kgem));
	}
	*ret = (char *)bo->mem + offset;
	return kgem_create_proxy(&bo->base, offset, size);
}

struct kgem_bo *kgem_create_buffer_2d(struct kgem *kgem,
				      int width, int height, int bpp,
				      uint32_t flags,
				      void **ret)
{
	struct kgem_bo *bo;
	int stride;

	stride = ALIGN(width, 2) * bpp >> 3;
	stride = ALIGN(stride, kgem->min_alignment);

	DBG(("%s: %dx%d, %d bpp, stride=%d\n",
	     __FUNCTION__, width, height, bpp, stride));

	bo = kgem_create_buffer(kgem, stride * ALIGN(height, 2), flags, ret);
	if (bo == NULL)
		return NULL;

	if (height & 1) {
		struct kgem_partial_bo *io = (struct kgem_partial_bo *)bo->proxy;

		/* Having padded this surface to ensure that accesses to
		 * the last pair of rows is valid, remove the padding so
		 * that it can be allocated to other pixmaps.
		 */
		io->used -= stride;
		bo->size -= stride;
		bubble_sort_partial(kgem, io);
	}

	bo->pitch = stride;
	return bo;
}

struct kgem_bo *kgem_upload_source_image(struct kgem *kgem,
					 const void *data,
					 BoxPtr box,
					 int stride, int bpp)
{
	int width = box->x2 - box->x1;
	int height = box->y2 - box->y1;
	struct kgem_bo *bo;
	void *dst;

	DBG(("%s : (%d, %d), (%d, %d), stride=%d, bpp=%d\n",
	     __FUNCTION__, box->x1, box->y1, box->x2, box->y2, stride, bpp));

	bo = kgem_create_buffer_2d(kgem,
				   width, height, bpp,
				   KGEM_BUFFER_WRITE_INPLACE, &dst);
	if (bo)
		memcpy_blt(data, dst, bpp,
			   stride, bo->pitch,
			   box->x1, box->y1,
			   0, 0,
			   width, height);

	return bo;
}

struct kgem_bo *kgem_upload_source_image_halved(struct kgem *kgem,
						pixman_format_code_t format,
						const void *data,
						int x, int y,
						int width, int height,
						int stride, int bpp)
{
	struct kgem_bo *bo;
	pixman_image_t *src_image, *dst_image;
	pixman_transform_t t;
	void *dst;

	DBG(("%s : (%d, %d), (%d, %d), stride=%d, bpp=%d\n",
	     __FUNCTION__, x, y, width, height, stride, bpp));

	bo = kgem_create_buffer_2d(kgem,
				   width, height, bpp,
				   KGEM_BUFFER_WRITE_INPLACE,
				   &dst);
	if (bo == NULL)
		return NULL;

	dst_image = pixman_image_create_bits(format, width/2, height/2,
					     dst, bo->pitch);
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

	assert(_bo->exec == NULL);
	if (_bo->proxy)
		_bo = _bo->proxy;
	assert(_bo->exec == NULL);

	bo = (struct kgem_partial_bo *)_bo;

	DBG(("%s(offset=%d, length=%d, vmap=%d)\n", __FUNCTION__,
	     offset, length, bo->base.vmap));

	if (bo->mmapped) {
		struct drm_i915_gem_set_domain set_domain;

		DBG(("%s: sync: needs_flush? %d, domain? %d, busy? %d\n",
		     __FUNCTION__,
		     bo->base.needs_flush,
		     bo->base.domain,
		     kgem_busy(kgem, bo->base.handle)));

		VG_CLEAR(set_domain);
		set_domain.handle = bo->base.handle;
		if (IS_CPU_MAP(bo->base.map)) {
			set_domain.read_domains = I915_GEM_DOMAIN_CPU;
			set_domain.write_domain = I915_GEM_DOMAIN_CPU;
			bo->base.domain = DOMAIN_CPU;
		} else {
			set_domain.read_domains = I915_GEM_DOMAIN_GTT;
			set_domain.write_domain = I915_GEM_DOMAIN_GTT;
			bo->base.domain = DOMAIN_GTT;
		}

		drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain);
	} else {
		gem_read(kgem->fd,
			 bo->base.handle, (char *)bo->mem+offset,
			 offset, length);
		bo->base.domain = DOMAIN_NONE;
		bo->base.reusable = false; /* in the CPU cache now */
	}
	kgem_bo_retire(kgem, &bo->base);
}

uint32_t kgem_bo_get_binding(struct kgem_bo *bo, uint32_t format)
{
	struct kgem_bo_binding *b;

	for (b = &bo->binding; b && b->offset; b = b->next)
		if (format == b->format)
			return b->offset;

	return 0;
}

void kgem_bo_set_binding(struct kgem_bo *bo, uint32_t format, uint16_t offset)
{
	struct kgem_bo_binding *b;

	for (b = &bo->binding; b; b = b->next) {
		if (b->offset)
			continue;

		b->offset = offset;
		b->format = format;

		if (b->next)
			b->next->offset = 0;

		return;
	}

	b = malloc(sizeof(*b));
	if (b) {
		b->next = bo->binding.next;
		b->format = format;
		b->offset = offset;
		bo->binding.next = b;
	}
}

struct kgem_bo *
kgem_replace_bo(struct kgem *kgem,
		struct kgem_bo *src,
		uint32_t width,
		uint32_t height,
		uint32_t pitch,
		uint32_t bpp)
{
	struct kgem_bo *dst;
	uint32_t br00, br13;
	uint32_t handle;
	uint32_t size;
	uint32_t *b;

	DBG(("%s: replacing bo handle=%d, size=%dx%d pitch=%d, with pitch=%d\n",
	     __FUNCTION__, src->handle,  width, height, src->pitch, pitch));

	/* We only expect to be called to fixup small buffers, hence why
	 * we only attempt to allocate a linear bo.
	 */
	assert(src->tiling == I915_TILING_NONE);

	size = height * pitch;
	size = PAGE_ALIGN(size);

	dst = search_linear_cache(kgem, size, 0);
	if (dst == NULL)
		dst = search_linear_cache(kgem, size, CREATE_INACTIVE);
	if (dst == NULL) {
		handle = gem_create(kgem->fd, size);
		if (handle == 0)
			return NULL;

		dst = __kgem_bo_alloc(handle, size);
		if (dst== NULL) {
			gem_close(kgem->fd, handle);
			return NULL;
		}
	}
	dst->pitch = pitch;
	dst->unique_id = kgem_get_unique_id(kgem);
	dst->refcnt = 1;

	kgem_set_mode(kgem, KGEM_BLT);
	if (!kgem_check_batch(kgem, 8) ||
	    !kgem_check_reloc(kgem, 2) ||
	    !kgem_check_bo_fenced(kgem, src, dst, NULL)) {
		_kgem_submit(kgem);
		_kgem_set_mode(kgem, KGEM_BLT);
	}

	br00 = XY_SRC_COPY_BLT_CMD;
	br13 = pitch;
	pitch = src->pitch;
	if (kgem->gen >= 40 && src->tiling) {
		br00 |= BLT_SRC_TILED;
		pitch >>= 2;
	}

	br13 |= 0xcc << 16;
	switch (bpp) {
	default:
	case 32: br00 |= BLT_WRITE_ALPHA | BLT_WRITE_RGB;
		 br13 |= 1 << 25; /* RGB8888 */
	case 16: br13 |= 1 << 24; /* RGB565 */
	case 8: break;
	}

	b = kgem->batch + kgem->nbatch;
	b[0] = br00;
	b[1] = br13;
	b[2] = 0;
	b[3] = height << 16 | width;
	b[4] = kgem_add_reloc(kgem, kgem->nbatch + 4, dst,
			      I915_GEM_DOMAIN_RENDER << 16 |
			      I915_GEM_DOMAIN_RENDER |
			      KGEM_RELOC_FENCED,
			      0);
	b[5] = 0;
	b[6] = pitch;
	b[7] = kgem_add_reloc(kgem, kgem->nbatch + 7, src,
			      I915_GEM_DOMAIN_RENDER << 16 |
			      KGEM_RELOC_FENCED,
			      0);
	kgem->nbatch += 8;

	return dst;
}
