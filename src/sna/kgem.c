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

#if HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>
#endif

static struct kgem_bo *
search_linear_cache(struct kgem *kgem, unsigned int num_pages, unsigned flags);


#define DBG_NO_HW 0
#define DBG_NO_TILING 0
#define DBG_NO_VMAP 0
#define DBG_NO_LLC 0
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

#define PAGE_ALIGN(x) ALIGN(x, PAGE_SIZE)
#define NUM_PAGES(x) (((x) + PAGE_SIZE-1) / PAGE_SIZE)

#define MAX_GTT_VMA_CACHE 512
#define MAX_CPU_VMA_CACHE INT16_MAX
#define MAP_PRESERVE_TIME 10

#define MAP(ptr) ((void*)((uintptr_t)(ptr) & ~3))
#define MAKE_CPU_MAP(ptr) ((void*)((uintptr_t)(ptr) | 1))
#define MAKE_VMAP_MAP(ptr) ((void*)((uintptr_t)(ptr) | 3))
#define IS_VMAP_MAP(ptr) ((uintptr_t)(ptr) & 2)

#if defined(USE_VMAP) && !defined(I915_PARAM_HAS_VMAP)
#define DRM_I915_GEM_VMAP       0x2c
#define DRM_IOCTL_I915_GEM_VMAP DRM_IOWR (DRM_COMMAND_BASE + DRM_I915_GEM_VMAP, struct drm_i915_gem_vmap)
#define I915_PARAM_HAS_VMAP              19
struct drm_i915_gem_vmap {
	uint64_t user_ptr;
	uint32_t user_size;
	uint32_t flags;
#define I915_VMAP_READ_ONLY 0x1
	uint32_t handle;
};
#endif

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

static inline int bytes(struct kgem_bo *bo)
{
	return kgem_bo_size(bo);
}

#define bucket(B) (B)->size.pages.bucket
#define num_pages(B) (B)->size.pages.count

#ifndef NDEBUG
static bool validate_partials(struct kgem *kgem)
{
	struct kgem_partial_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &kgem->active_partials, base.list) {
		assert(next->base.list.prev == &bo->base.list);
		assert(bo->base.refcnt >= 1);
		assert(bo->base.io);

		if (&next->base.list == &kgem->active_partials)
			break;

		if (bytes(&bo->base) - bo->used < bytes(&next->base) - next->used) {
			ErrorF("active error: this rem: %d, next rem: %d\n",
			       bytes(&bo->base) - bo->used,
			       bytes(&next->base) - next->used);
			goto err;
		}
	}

	list_for_each_entry_safe(bo, next, &kgem->inactive_partials, base.list) {
		assert(next->base.list.prev == &bo->base.list);
		assert(bo->base.io);
		assert(bo->base.refcnt == 1);

		if (&next->base.list == &kgem->inactive_partials)
			break;

		if (bytes(&bo->base) - bo->used < bytes(&next->base) - next->used) {
			ErrorF("inactive error: this rem: %d, next rem: %d\n",
			       bytes(&bo->base) - bo->used,
			       bytes(&next->base) - next->used);
			goto err;
		}
	}

	return true;

err:
	ErrorF("active partials:\n");
	list_for_each_entry(bo, &kgem->active_partials, base.list)
		ErrorF("bo handle=%d: used=%d / %d, rem=%d\n",
		       bo->base.handle, bo->used, bytes(&bo->base), bytes(&bo->base) - bo->used);
	ErrorF("inactive partials:\n");
	list_for_each_entry(bo, &kgem->inactive_partials, base.list)
		ErrorF("bo handle=%d: used=%d / %d, rem=%d\n",
		       bo->base.handle, bo->used, bytes(&bo->base), bytes(&bo->base) - bo->used);
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
	DBG(("%s: handle=%d, domain=%d\n",
	     __FUNCTION__, bo->handle, bo->domain));
	assert(bo->flush || !kgem_busy(kgem, bo->handle));

	if (bo->rq)
		kgem_retire(kgem);

	if (bo->exec == NULL) {
		DBG(("%s: retiring bo handle=%d (needed flush? %d), rq? %d\n",
		     __FUNCTION__, bo->handle, bo->needs_flush, bo->rq != NULL));
		assert(list_is_empty(&bo->vma));
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
	assert(bo->flush || !kgem_busy(kgem, bo->handle));
	assert(bo->proxy == NULL);

	assert(length <= bytes(bo));
	if (gem_write(kgem->fd, bo->handle, 0, length, data))
		return FALSE;

	DBG(("%s: flush=%d, domain=%d\n", __FUNCTION__, bo->flush, bo->domain));
	kgem_bo_retire(kgem, bo);
	bo->domain = DOMAIN_NONE;
	return TRUE;
}

static uint32_t gem_create(int fd, int num_pages)
{
	struct drm_i915_gem_create create;

	VG_CLEAR(create);
	create.handle = 0;
	create.size = PAGE_SIZE * num_pages;
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

constant inline static unsigned long __fls(unsigned long word)
{
	asm("bsr %1,%0"
	    : "=r" (word)
	    : "rm" (word));
	return word;
}

constant inline static int cache_bucket(int num_pages)
{
	return __fls(num_pages);
}

static struct kgem_bo *__kgem_bo_init(struct kgem_bo *bo,
				      int handle, int num_pages)
{
	assert(num_pages);
	memset(bo, 0, sizeof(*bo));

	bo->refcnt = 1;
	bo->handle = handle;
	num_pages(bo) = num_pages;
	bucket(bo) = cache_bucket(num_pages);
	bo->reusable = true;
	bo->domain = DOMAIN_CPU;
	list_init(&bo->request);
	list_init(&bo->list);
	list_init(&bo->vma);

	return bo;
}

static struct kgem_bo *__kgem_bo_alloc(int handle, int num_pages)
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

	return __kgem_bo_init(bo, handle, num_pages);
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

static struct list *inactive(struct kgem *kgem, int num_pages)
{
	return &kgem->inactive[cache_bucket(num_pages)];
}

static struct list *active(struct kgem *kgem, int num_pages, int tiling)
{
	return &kgem->active[cache_bucket(num_pages)][tiling];
}

static size_t
agp_aperture_size(struct pci_device *dev, int gen)
{
	return dev->regions[gen < 30 ? 0 : 2].size;
}

static size_t
total_ram_size(void)
{
#if HAVE_SYS_SYSINFO_H
	struct sysinfo info;
	if (sysinfo(&info) == 0)
		return info.totalram * info.mem_unit;
#endif

	return 0;
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
	int v = -1; /* No param uses the sign bit, reserve it for errors */

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
			detected = value != 0;
		fclose(file);
	}

	return detected;
}

void kgem_init(struct kgem *kgem, int fd, struct pci_device *dev, int gen)
{
	struct drm_i915_gem_get_aperture aperture;
	size_t totalram;
	unsigned half_gpu_max;
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

	list_init(&kgem->active_partials);
	list_init(&kgem->inactive_partials);
	list_init(&kgem->requests);
	list_init(&kgem->flushing);
	list_init(&kgem->sync_list);
	list_init(&kgem->large);
	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++)
		list_init(&kgem->inactive[i]);
	for (i = 0; i < ARRAY_SIZE(kgem->active); i++) {
		for (j = 0; j < ARRAY_SIZE(kgem->active[i]); j++)
			list_init(&kgem->active[i][j]);
	}
	for (i = 0; i < ARRAY_SIZE(kgem->vma); i++) {
		for (j = 0; j < ARRAY_SIZE(kgem->vma[i].inactive); j++)
			list_init(&kgem->vma[i].inactive[j]);
	}
	kgem->vma[MAP_GTT].count = -MAX_GTT_VMA_CACHE;
	kgem->vma[MAP_CPU].count = -MAX_CPU_VMA_CACHE;

	kgem->next_request = __kgem_request_alloc();

#if defined(USE_VMAP)
	if (!DBG_NO_VMAP)
		kgem->has_vmap = gem_param(kgem, I915_PARAM_HAS_VMAP) > 0;
	if (gen == 40)
		kgem->has_vmap = false; /* sampler dies with snoopable memory */
#endif
	DBG(("%s: using vmap=%d\n", __FUNCTION__, kgem->has_vmap));

	if (gen < 40) {
		if (!DBG_NO_RELAXED_FENCING) {
			kgem->has_relaxed_fencing =
				gem_param(kgem, I915_PARAM_HAS_RELAXED_FENCING) > 0;
		}
	} else
		kgem->has_relaxed_fencing = 1;
	DBG(("%s: has relaxed fencing? %d\n", __FUNCTION__,
	     kgem->has_relaxed_fencing));

	kgem->has_llc = false;
	if (!DBG_NO_LLC) {
		int has_llc = -1;
#if defined(I915_PARAM_HAS_LLC) /* Expected in libdrm-2.4.31 */
		has_llc = gem_param(kgem, I915_PARAM_HAS_LLC);
#endif
		if (has_llc == -1) {
			DBG(("%s: no kernel/drm support for HAS_LLC, assuming support for LLC based on GPU generation\n", __FUNCTION__));
			has_llc = gen >= 60;
		}
		kgem->has_llc = has_llc;
	}
	DBG(("%s: cpu bo enabled %d: llc? %d, vmap? %d\n", __FUNCTION__,
	     kgem->has_llc | kgem->has_vmap, kgem->has_llc, kgem->has_vmap));

	kgem->has_semaphores = false;
	if (gen >= 60 && semaphores_enabled())
		kgem->has_semaphores = true;
	DBG(("%s: semaphores enabled? %d\n", __FUNCTION__,
	     kgem->has_semaphores));

	VG_CLEAR(aperture);
	aperture.aper_size = 64*1024*1024;
	(void)drmIoctl(fd, DRM_IOCTL_I915_GEM_GET_APERTURE, &aperture);

	kgem->aperture_total = aperture.aper_size;
	kgem->aperture_high = aperture.aper_size * 3/4;
	kgem->aperture_low = aperture.aper_size * 1/3;
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

	kgem->max_object_size = 2 * aperture.aper_size / 3;
	kgem->max_gpu_size = kgem->max_object_size;
	if (!kgem->has_llc)
		kgem->max_gpu_size = MAX_CACHE_SIZE;
	if (gen < 40) {
		/* If we have to use fences for blitting, we have to make
		 * sure we can fit them into the aperture.
		 */
		kgem->max_gpu_size = kgem->aperture_mappable / 2;
		if (kgem->max_gpu_size > kgem->aperture_low)
			kgem->max_gpu_size = kgem->aperture_low;
	}

	totalram = total_ram_size();
	if (totalram == 0) {
		DBG(("%s: total ram size unknown, assuming maximum of total aperture\n",
		     __FUNCTION__));
		totalram = kgem->aperture_total;
	}
	DBG(("%s: total ram=%ld\n", __FUNCTION__, (long)totalram));
	if (kgem->max_object_size > totalram / 2)
		kgem->max_object_size = totalram / 2;
	if (kgem->max_gpu_size > totalram / 4)
		kgem->max_gpu_size = totalram / 4;

	half_gpu_max = kgem->max_gpu_size / 2;
	if (kgem->gen >= 40)
		kgem->max_cpu_size = half_gpu_max;
	else
		kgem->max_cpu_size = kgem->max_object_size;

	kgem->max_copy_tile_size = (MAX_CACHE_SIZE + 1)/2;
	if (kgem->max_copy_tile_size > half_gpu_max)
		kgem->max_copy_tile_size = half_gpu_max;

	if (kgem->has_llc)
		kgem->max_upload_tile_size = kgem->max_copy_tile_size;
	else
		kgem->max_upload_tile_size = kgem->aperture_mappable / 4;
	if (kgem->max_upload_tile_size > half_gpu_max)
		kgem->max_upload_tile_size = half_gpu_max;

	kgem->large_object_size = MAX_CACHE_SIZE;
	if (kgem->large_object_size > kgem->max_gpu_size)
		kgem->large_object_size = kgem->max_gpu_size;
	if (kgem->has_llc | kgem->has_vmap) {
		if (kgem->large_object_size > kgem->max_cpu_size)
			kgem->large_object_size = kgem->max_cpu_size;
	} else
		kgem->max_cpu_size = 0;

	DBG(("%s: maximum object size=%d\n",
	     __FUNCTION__, kgem->max_object_size));
	DBG(("%s: large object thresold=%d\n",
	     __FUNCTION__, kgem->large_object_size));
	DBG(("%s: max object sizes (gpu=%d, cpu=%d, tile upload=%d, copy=%d)\n",
	     __FUNCTION__,
	     kgem->max_gpu_size, kgem->max_cpu_size,
	     kgem->max_upload_tile_size, kgem->max_copy_tile_size));

	/* Convert the aperture thresholds to pages */
	kgem->aperture_low /= PAGE_SIZE;
	kgem->aperture_high /= PAGE_SIZE;

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
	width = ALIGN(width, 4) * bpp >> 3;
	return ALIGN(width, scanout ? 64 : 4);
}

void kgem_get_tile_size(struct kgem *kgem, int tiling,
			int *tile_width, int *tile_height, int *tile_size)
{
	if (kgem->gen <= 30) {
		if (tiling) {
			*tile_width = 512;
			if (kgem->gen < 30) {
				*tile_height = 16;
				*tile_size = 2048;
			} else {
				*tile_height = 8;
				*tile_size = 4096;
			}
		} else {
			*tile_width = 1;
			*tile_height = 1;
			*tile_size = 1;
		}
	} else switch (tiling) {
	default:
	case I915_TILING_NONE:
		*tile_width = 1;
		*tile_height = 1;
		*tile_size = 1;
		break;
	case I915_TILING_X:
		*tile_width = 512;
		*tile_height = 8;
		*tile_size = 4096;
		break;
	case I915_TILING_Y:
		*tile_width = 128;
		*tile_height = 32;
		*tile_size = 4096;
		break;
	}
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

	assert(width <= MAXSHORT);
	assert(height <= MAXSHORT);

	if (kgem->gen <= 30) {
		if (tiling) {
			tile_width = 512;
			tile_height = kgem->gen < 30 ? 16 : 8;
		} else {
			tile_width = scanout ? 64 : 4 * bpp >> 3;
			tile_height = 4;
		}
	} else switch (tiling) {
	default:
	case I915_TILING_NONE:
		tile_width = scanout ? 64 : 4 * bpp >> 3;
		tile_height = 2;
		break;
	case I915_TILING_X:
		tile_width = 512;
		tile_height = 8;
		break;
	case I915_TILING_Y:
		tile_width = 128;
		tile_height = 32;
		break;
	}

	*pitch = ALIGN(width * bpp / 8, tile_width);
	height = ALIGN(height, tile_height);
	if (kgem->gen >= 40)
		return PAGE_ALIGN(*pitch * height);

	/* If it is too wide for the blitter, don't even bother.  */
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

	size = *pitch * height;
	if (relaxed_fencing || tiling == I915_TILING_NONE)
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

	if (kgem->gen <= 30) {
		tile_height = tiling ? kgem->gen < 30 ? 16 : 8 : 1;
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

	kgem->aperture += num_pages(bo);

	return exec;
}

void _kgem_add_bo(struct kgem *kgem, struct kgem_bo *bo)
{
	bo->exec = kgem_add_handle(kgem, bo);
	bo->rq = kgem->next_request;

	list_move(&bo->request, &kgem->next_request->buffers);

	/* XXX is it worth working around gcc here? */
	kgem->flush |= bo->flush;
	kgem->scanout |= bo->scanout;

	if (bo->sync)
		kgem->sync = kgem->next_request;
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

	assert(!IS_VMAP_MAP(bo->map));

	DBG(("%s: releasing %s vma for handle=%d, count=%d\n",
	     __FUNCTION__, type ? "CPU" : "GTT",
	     bo->handle, kgem->vma[type].count));

	VG(if (type) VALGRIND_FREELIKE_BLOCK(MAP(bo->map), 0));
	munmap(MAP(bo->map), bytes(bo));
	bo->map = NULL;

	if (!list_is_empty(&bo->vma)) {
		list_del(&bo->vma);
		kgem->vma[type].count--;
	}
}

static void kgem_bo_free(struct kgem *kgem, struct kgem_bo *bo)
{
	DBG(("%s: handle=%d\n", __FUNCTION__, bo->handle));
	assert(bo->refcnt == 0);
	assert(bo->exec == NULL);
	assert(!bo->vmap || bo->rq == NULL);

	kgem_bo_binding_free(kgem, bo);

	if (IS_VMAP_MAP(bo->map)) {
		assert(bo->rq == NULL);
		free(MAP(bo->map));
		bo->map = NULL;
	}
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
	assert(bo->reusable);
	assert(bo->rq == NULL);
	assert(bo->domain != DOMAIN_GPU);
	assert(!kgem_busy(kgem, bo->handle));
	assert(!bo->proxy);
	assert(!bo->io);
	assert(!bo->needs_flush);
	assert(list_is_empty(&bo->vma));

	if (bucket(bo) >= NUM_CACHE_BUCKETS) {
		kgem_bo_free(kgem, bo);
		return;
	}

	list_move(&bo->list, &kgem->inactive[bucket(bo)]);
	if (bo->map) {
		int type = IS_CPU_MAP(bo->map);
		if (bucket(bo) >= NUM_CACHE_BUCKETS ||
		    (!type && !kgem_bo_is_mappable(kgem, bo))) {
			munmap(MAP(bo->map), bytes(bo));
			bo->map = NULL;
		}
		if (bo->map) {
			list_add(&bo->vma, &kgem->vma[type].inactive[bucket(bo)]);
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

	if (bo->vmap) {
		assert(!bo->flush);
		DBG(("%s: handle=%d is vmapped, tracking until free\n",
		     __FUNCTION__, bo->handle));
		if (bo->rq == NULL) {
			if (bo->needs_flush && kgem_busy(kgem, bo->handle)) {
				list_add(&bo->request, &kgem->flushing);
				bo->rq = &_kgem_static_request;
			} else
				kgem_bo_free(kgem, bo);
		} else {
			assert(!bo->sync);
		}
		return;
	}

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

	if (!kgem->has_llc && IS_CPU_MAP(bo->map) && bo->domain != DOMAIN_CPU)
		kgem_bo_release_map(kgem, bo);

	assert(list_is_empty(&bo->vma));
	assert(list_is_empty(&bo->list));
	assert(bo->vmap == false);
	assert(bo->io == false);
	assert(bo->scanout == false);
	assert(bo->flush == false);

	if (bo->rq) {
		struct list *cache;

		DBG(("%s: handle=%d -> active\n", __FUNCTION__, bo->handle));
		if (bucket(bo) < NUM_CACHE_BUCKETS)
			cache = &kgem->active[bucket(bo)][bo->tiling];
		else
			cache = &kgem->large;
		list_add(&bo->list, cache);
		return;
	}

	assert(bo->exec == NULL);
	assert(list_is_empty(&bo->request));

	if (bo->needs_flush) {
		if ((bo->needs_flush = kgem_busy(kgem, bo->handle))) {
			struct list *cache;

			DBG(("%s: handle=%d -> flushing\n",
			     __FUNCTION__, bo->handle));

			list_add(&bo->request, &kgem->flushing);
			if (bucket(bo) < NUM_CACHE_BUCKETS)
				cache = &kgem->active[bucket(bo)][bo->tiling];
			else
				cache = &kgem->large;
			list_add(&bo->list, cache);
			bo->rq = &_kgem_static_request;
			return;
		}

		bo->domain = DOMAIN_NONE;
	}

	if (!IS_CPU_MAP(bo->map)) {
		if (!kgem_bo_set_purgeable(kgem, bo))
			goto destroy;

		if (!kgem->has_llc && bo->domain == DOMAIN_CPU)
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

static void bubble_sort_partial(struct list *head, struct kgem_partial_bo *bo)
{
	int remain = bytes(&bo->base) - bo->used;

	while (bo->base.list.prev != head) {
		struct kgem_partial_bo *p;

		p = list_entry(bo->base.list.prev,
			       struct kgem_partial_bo,
			       base.list);
		if (remain <= bytes(&p->base) - p->used)
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

static void kgem_partial_buffer_release(struct kgem *kgem,
					struct kgem_partial_bo *bo)
{
	while (!list_is_empty(&bo->base.vma)) {
		struct kgem_bo *cached;

		cached = list_first_entry(&bo->base.vma, struct kgem_bo, vma);
		assert(cached->proxy == &bo->base);
		list_del(&cached->vma);

		assert(*(struct kgem_bo **)cached->map == cached);
		*(struct kgem_bo **)cached->map = NULL;
		cached->map = NULL;

		kgem_bo_destroy(kgem, cached);
	}
}

static void kgem_retire_partials(struct kgem *kgem)
{
	struct kgem_partial_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &kgem->active_partials, base.list) {
		assert(next->base.list.prev == &bo->base.list);
		assert(bo->base.io);

		if (bo->base.rq)
			continue;

		DBG(("%s: releasing upload cache for handle=%d? %d\n",
		     __FUNCTION__, bo->base.handle, !list_is_empty(&bo->base.vma)));
		kgem_partial_buffer_release(kgem, bo);

		assert(bo->base.refcnt > 0);
		if (bo->base.refcnt != 1)
			continue;

		DBG(("%s: handle=%d, used %d/%d\n", __FUNCTION__,
		     bo->base.handle, bo->used, bytes(&bo->base)));

		assert(bo->base.refcnt == 1);
		assert(bo->base.exec == NULL);
		if (!bo->mmapped || bo->base.presumed_offset == 0) {
			list_del(&bo->base.list);
			kgem_bo_unref(kgem, &bo->base);
			continue;
		}

		bo->base.dirty = false;
		bo->base.needs_flush = false;
		bo->used = 0;

		DBG(("%s: transferring partial handle=%d to inactive\n",
		     __FUNCTION__, bo->base.handle));
		list_move_tail(&bo->base.list, &kgem->inactive_partials);
		bubble_sort_partial(&kgem->inactive_partials, bo);
	}
	assert(validate_partials(kgem));
}

bool kgem_retire(struct kgem *kgem)
{
	struct kgem_bo *bo, *next;
	bool retired = false;

	DBG(("%s\n", __FUNCTION__));

	list_for_each_entry_safe(bo, next, &kgem->flushing, request) {
		assert(bo->refcnt == 0);
		assert(bo->rq == &_kgem_static_request);
		assert(bo->exec == NULL);

		if (kgem_busy(kgem, bo->handle))
			break;

		DBG(("%s: moving %d from flush to inactive\n",
		     __FUNCTION__, bo->handle));
		if (bo->reusable && kgem_bo_set_purgeable(kgem, bo)) {
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
		} else {
			DBG(("%s: closing %d\n",
			     __FUNCTION__, rq->bo->handle));
			kgem_bo_free(kgem, rq->bo);
		}

		if (kgem->sync == rq)
			kgem->sync = NULL;

		_list_del(&rq->list);
		free(rq);
	}

	kgem_retire_partials(kgem);

	kgem->need_retire = !list_is_empty(&kgem->requests);
	DBG(("%s -- need_retire=%d\n", __FUNCTION__, kgem->need_retire));

	kgem->retire(kgem);

	return retired;
}

static void kgem_commit(struct kgem *kgem)
{
	struct kgem_request *rq = kgem->next_request;
	struct kgem_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &rq->buffers, request) {
		assert(next->request.prev == &bo->request);

		DBG(("%s: release handle=%d (proxy? %d), dirty? %d flush? %d -> offset=%x\n",
		     __FUNCTION__, bo->handle, bo->proxy != NULL,
		     bo->dirty, bo->needs_flush, (unsigned)bo->exec->offset));

		assert(!bo->purged);
		assert(bo->rq == rq || (bo->proxy->rq == rq));

		bo->presumed_offset = bo->exec->offset;
		bo->exec = NULL;

		if (!bo->refcnt && !bo->reusable && !bo->vmap) {
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
			bo->exec = &_kgem_dummy_exec;
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

static void kgem_finish_partials(struct kgem *kgem)
{
	struct kgem_partial_bo *bo, *next;

	list_for_each_entry_safe(bo, next, &kgem->active_partials, base.list) {
		DBG(("%s: partial handle=%d, used=%d, exec?=%d, write=%d, mmapped=%d\n",
		     __FUNCTION__, bo->base.handle, bo->used, bo->base.exec!=NULL,
		     bo->write, bo->mmapped));

		assert(next->base.list.prev == &bo->base.list);
		assert(bo->base.io);
		assert(bo->base.refcnt >= 1);

		if (!bo->base.exec) {
			DBG(("%s: skipping unattached handle=%d, used=%d\n",
			     __FUNCTION__, bo->base.handle, bo->used));
			continue;
		}

		if (!bo->write) {
			assert(bo->base.exec || bo->base.refcnt > 1);
			goto decouple;
		}

		if (bo->mmapped) {
			assert(!bo->need_io);
			if (kgem->has_llc || !IS_CPU_MAP(bo->base.map)) {
				DBG(("%s: retaining partial upload buffer (%d/%d)\n",
				     __FUNCTION__, bo->used, bytes(&bo->base)));
				continue;
			}
			goto decouple;
		}

		if (!bo->used) {
			/* Unless we replace the handle in the execbuffer,
			 * then this bo will become active. So decouple it
			 * from the partial list and track it in the normal
			 * manner.
			 */
			goto decouple;
		}

		assert(bo->need_io);
		assert(bo->base.rq == kgem->next_request);
		assert(bo->base.domain != DOMAIN_GPU);

		if (bo->base.refcnt == 1 &&
		    bo->base.size.pages.count > 1 &&
		    bo->used < bytes(&bo->base) / 2) {
			struct kgem_bo *shrink;

			shrink = search_linear_cache(kgem,
						     PAGE_ALIGN(bo->used),
						     CREATE_INACTIVE | CREATE_NO_RETIRE);
			if (shrink) {
				int n;

				DBG(("%s: used=%d, shrinking %d to %d, handle %d to %d\n",
				     __FUNCTION__,
				     bo->used, bytes(&bo->base), bytes(shrink),
				     bo->base.handle, shrink->handle));

				assert(bo->used <= bytes(shrink));
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

				goto decouple;
			}
		}

		DBG(("%s: handle=%d, uploading %d/%d\n",
		     __FUNCTION__, bo->base.handle, bo->used, bytes(&bo->base)));
		assert(!kgem_busy(kgem, bo->base.handle));
		assert(bo->used <= bytes(&bo->base));
		gem_write(kgem->fd, bo->base.handle,
			  0, bo->used, bo->mem);
		bo->need_io = 0;

decouple:
		DBG(("%s: releasing handle=%d\n",
		     __FUNCTION__, bo->base.handle));
		list_del(&bo->base.list);
		kgem_bo_unref(kgem, &bo->base);
	}

	assert(validate_partials(kgem));
}

static void kgem_cleanup(struct kgem *kgem)
{
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
	rq->bo = kgem_create_linear(kgem, size, 0);
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

				ErrorF("batch[%d/%d]: %d %d %d, nreloc=%d, nexec=%d, nfence=%d, aperture=%d: errno=%d\n",
				       kgem->mode, kgem->ring, batch_end, kgem->nbatch, kgem->surface,
				       kgem->nreloc, kgem->nexec, kgem->nfence, kgem->aperture, errno);

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
					       found ? bytes(found) : -1,
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
				FatalError("SNA: failed to submit batchbuffer\n");
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
	kgem_retire_partials(kgem);
	while (!list_is_empty(&kgem->inactive_partials)) {
		struct kgem_partial_bo *bo =
			list_first_entry(&kgem->inactive_partials,
					 struct kgem_partial_bo,
					 base.list);

		DBG(("%s: discarding unused partial buffer: %d, last write? %d\n",
		     __FUNCTION__, bytes(&bo->base), bo->write));
		assert(bo->base.list.prev == &kgem->inactive_partials);
		assert(bo->base.io);
		assert(bo->base.refcnt == 1);
		list_del(&bo->base.list);
		kgem_bo_unref(kgem, &bo->base);
	}
}

void kgem_purge_cache(struct kgem *kgem)
{
	struct kgem_bo *bo, *next;
	int i;

	for (i = 0; i < ARRAY_SIZE(kgem->inactive); i++) {
		list_for_each_entry_safe(bo, next, &kgem->inactive[i], list) {
			if (!kgem_bo_is_retained(kgem, bo)) {
				DBG(("%s: purging %d\n",
				     __FUNCTION__, bo->handle));
				kgem_bo_free(kgem, bo);
			}
		}
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

	while (__kgem_freed_bo) {
		bo = __kgem_freed_bo;
		__kgem_freed_bo = *(struct kgem_bo **)bo;
		free(bo);
	}

	kgem_retire(kgem);
	if (kgem->wedged)
		kgem_cleanup(kgem);

	kgem_expire_partial(kgem);

	if (kgem->need_purge)
		kgem_purge_cache(kgem);

	time(&now);
	expire = 0;

	idle = !kgem->need_retire;
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

	idle = !kgem->need_retire;
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
				size += bytes(bo);
				kgem_bo_free(kgem, bo);
				DBG(("%s: expiring %d\n",
				     __FUNCTION__, bo->handle));
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
search_linear_cache(struct kgem *kgem, unsigned int num_pages, unsigned flags)
{
	struct kgem_bo *bo, *first = NULL;
	bool use_active = (flags & CREATE_INACTIVE) == 0;
	struct list *cache;

	DBG(("%s: num_pages=%d, flags=%x, use_active? %d\n",
	     __FUNCTION__, num_pages, flags, use_active));

	if (num_pages >= MAX_CACHE_SIZE / PAGE_SIZE)
		return NULL;

	if (!use_active && list_is_empty(inactive(kgem, num_pages))) {
		DBG(("%s: inactive and cache bucket empty\n",
		     __FUNCTION__));

		if (flags & CREATE_NO_RETIRE) {
			DBG(("%s: can not retire\n", __FUNCTION__));
			return NULL;
		}

		if (!kgem->need_retire || !kgem_retire(kgem)) {
			DBG(("%s: nothing retired\n", __FUNCTION__));
			return NULL;
		}

		if (list_is_empty(inactive(kgem, num_pages))) {
			DBG(("%s: active cache bucket still empty after retire\n",
			     __FUNCTION__));
			return NULL;
		}
	}

	if (!use_active && flags & (CREATE_CPU_MAP | CREATE_GTT_MAP)) {
		int for_cpu = !!(flags & CREATE_CPU_MAP);
		DBG(("%s: searching for inactive %s map\n",
		     __FUNCTION__, for_cpu ? "cpu" : "gtt"));
		cache = &kgem->vma[for_cpu].inactive[cache_bucket(num_pages)];
		list_for_each_entry(bo, cache, vma) {
			assert(IS_CPU_MAP(bo->map) == for_cpu);
			assert(bucket(bo) == cache_bucket(num_pages));

			if (num_pages > num_pages(bo)) {
				DBG(("inactive too small: %d < %d\n",
				     num_pages(bo), num_pages));
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
			DBG(("  %s: found handle=%d (num_pages=%d) in linear vma cache\n",
			     __FUNCTION__, bo->handle, num_pages(bo)));
			assert(use_active || bo->domain != DOMAIN_GPU);
			assert(!bo->needs_flush);
			//assert(!kgem_busy(kgem, bo->handle));
			return bo;
		}

		if (flags & CREATE_EXACT)
			return NULL;
	}

	cache = use_active ? active(kgem, num_pages, I915_TILING_NONE) : inactive(kgem, num_pages);
	list_for_each_entry(bo, cache, list) {
		assert(bo->refcnt == 0);
		assert(bo->reusable);
		assert(!!bo->rq == !!use_active);

		if (num_pages > num_pages(bo))
			continue;

		if (use_active &&
		    kgem->gen <= 40 &&
		    bo->tiling != I915_TILING_NONE)
			continue;

		if (bo->purged && !kgem_bo_clear_purgeable(kgem, bo)) {
			kgem_bo_free(kgem, bo);
			break;
		}

		if (I915_TILING_NONE != bo->tiling) {
			if (flags & (CREATE_CPU_MAP | CREATE_GTT_MAP))
				continue;

			if (first)
				continue;

			if (gem_set_tiling(kgem->fd, bo->handle,
					   I915_TILING_NONE, 0) != I915_TILING_NONE)
				continue;

			bo->tiling = I915_TILING_NONE;
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

		if (use_active)
			kgem_bo_remove_from_active(kgem, bo);
		else
			kgem_bo_remove_from_inactive(kgem, bo);

		assert(bo->tiling == I915_TILING_NONE);
		bo->pitch = 0;
		bo->delta = 0;
		DBG(("  %s: found handle=%d (num_pages=%d) in linear %s cache\n",
		     __FUNCTION__, bo->handle, num_pages(bo),
		     use_active ? "active" : "inactive"));
		assert(use_active || bo->domain != DOMAIN_GPU);
		assert(!bo->needs_flush || use_active);
		//assert(use_active || !kgem_busy(kgem, bo->handle));
		return bo;
	}

	if (first) {
		assert(first->tiling == I915_TILING_NONE);

		if (use_active)
			kgem_bo_remove_from_active(kgem, first);
		else
			kgem_bo_remove_from_inactive(kgem, first);

		first->pitch = 0;
		first->delta = 0;
		DBG(("  %s: found handle=%d (num_pages=%d) in linear %s cache\n",
		     __FUNCTION__, first->handle, num_pages(first),
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
	bo = __kgem_bo_alloc(open_arg.handle, open_arg.size / PAGE_SIZE);
	if (bo == NULL) {
		gem_close(kgem->fd, open_arg.handle);
		return NULL;
	}

	bo->reusable = false;
	bo->flush = true;
	return bo;
}

struct kgem_bo *kgem_create_linear(struct kgem *kgem, int size, unsigned flags)
{
	struct kgem_bo *bo;
	uint32_t handle;

	DBG(("%s(%d)\n", __FUNCTION__, size));

	if (flags & CREATE_GTT_MAP && kgem->has_llc) {
		flags &= ~CREATE_GTT_MAP;
		flags |= CREATE_CPU_MAP;
	}

	size = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	bo = search_linear_cache(kgem, size, CREATE_INACTIVE | flags);
	if (bo) {
		bo->refcnt = 1;
		return bo;
	}

	handle = gem_create(kgem->fd, size);
	if (handle == 0)
		return NULL;

	DBG(("%s: new handle=%d, num_pages=%d\n", __FUNCTION__, handle, size));
	bo = __kgem_bo_alloc(handle, size);
	if (bo == NULL) {
		gem_close(kgem->fd, handle);
		return NULL;
	}

	return bo;
}

int kgem_choose_tiling(struct kgem *kgem, int tiling, int width, int height, int bpp)
{
	if (DBG_NO_TILING)
		return tiling < 0 ? tiling : I915_TILING_NONE;

	if (kgem->gen < 40) {
		if (tiling && width * bpp > 8192 * 8) {
			DBG(("%s: pitch too large for tliing [%d]\n",
			     __FUNCTION__, width*bpp/8));
			tiling = I915_TILING_NONE;
			goto done;
		}
	} else {
		if (width*bpp > (MAXSHORT-512) * 8) {
			DBG(("%s: large pitch [%d], forcing TILING_X\n",
			     __FUNCTION__, width*bpp/8));
			if (tiling > 0)
				tiling = -tiling;
			else if (tiling == 0)
				tiling = -I915_TILING_X;
		} else if (tiling && (width|height) > 8192) {
			DBG(("%s: large tiled buffer [%dx%d], forcing TILING_X\n",
			     __FUNCTION__, width, height));
			tiling = -I915_TILING_X;
		}
	}

	if (tiling < 0)
		return tiling;

	if (tiling && height == 1) {
		DBG(("%s: disabling tiling [%d] for single row\n",
		     __FUNCTION__,height));
		tiling = I915_TILING_NONE;
		goto done;
	}
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

unsigned kgem_can_create_2d(struct kgem *kgem,
			    int width, int height, int depth)
{
	int bpp = BitsPerPixel(depth);
	uint32_t pitch, size;
	unsigned flags = 0;

	if (depth < 8) {
		DBG(("%s: unhandled depth %d\n", __FUNCTION__, depth));
		return 0;
	}

	if (width > MAXSHORT || height > MAXSHORT) {
		DBG(("%s: unhandled size %dx%d\n",
		     __FUNCTION__, width, height));
		return 0;
	}

	size = kgem_surface_size(kgem, false, false,
				 width, height, bpp,
				 I915_TILING_NONE, &pitch);
	if (size > 0 && size <= kgem->max_cpu_size)
		flags |= KGEM_CAN_CREATE_CPU | KGEM_CAN_CREATE_GPU;
	if (size > kgem->large_object_size)
		flags |= KGEM_CAN_CREATE_LARGE;
	if (size > kgem->max_object_size) {
		DBG(("%s: too large (untiled) %d > %d\n",
		     __FUNCTION__, size, kgem->max_object_size));
		return 0;
	}

	size = kgem_surface_size(kgem, false, false,
				 width, height, bpp,
				 kgem_choose_tiling(kgem, I915_TILING_X,
						    width, height, bpp),
				 &pitch);
	if (size > 0 && size <= kgem->max_gpu_size)
		flags |= KGEM_CAN_CREATE_GPU;
	if (size > kgem->large_object_size)
		flags |= KGEM_CAN_CREATE_LARGE;
	if (size > kgem->max_object_size) {
		DBG(("%s: too large (tiled) %d > %d\n",
		     __FUNCTION__, size, kgem->max_object_size));
		return 0;
	}

	return flags;
}

inline int kgem_bo_fenced_size(struct kgem *kgem, struct kgem_bo *bo)
{
	unsigned int size;

	assert(bo->tiling);
	assert(kgem->gen < 40);

	if (kgem->gen < 30)
		size = 512 * 1024;
	else
		size = 1024 * 1024;
	while (size < bytes(bo))
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
	uint32_t pitch, untiled_pitch, tiled_height, size;
	uint32_t handle;
	int i, bucket, retry;

	if (tiling < 0)
		tiling = -tiling, flags |= CREATE_EXACT;

	DBG(("%s(%dx%d, bpp=%d, tiling=%d, exact=%d, inactive=%d, cpu-mapping=%d, gtt-mapping=%d, scanout?=%d, temp?=%d)\n", __FUNCTION__,
	     width, height, bpp, tiling,
	     !!(flags & CREATE_EXACT),
	     !!(flags & CREATE_INACTIVE),
	     !!(flags & CREATE_CPU_MAP),
	     !!(flags & CREATE_GTT_MAP),
	     !!(flags & CREATE_SCANOUT),
	     !!(flags & CREATE_TEMPORARY)));

	size = kgem_surface_size(kgem,
				 kgem->has_relaxed_fencing,
				 flags & CREATE_SCANOUT,
				 width, height, bpp, tiling, &pitch);
	assert(size && size <= kgem->max_object_size);
	size /= PAGE_SIZE;
	bucket = cache_bucket(size);

	if (bucket >= NUM_CACHE_BUCKETS) {
		DBG(("%s: large bo num pages=%d, bucket=%d\n",
		     __FUNCTION__, size, bucket));

		if (flags & CREATE_INACTIVE)
			goto create;

		tiled_height = kgem_aligned_height(kgem, height, I915_TILING_Y);
		untiled_pitch = kgem_untiled_pitch(kgem,
						   width, bpp,
						   flags & CREATE_SCANOUT);

		list_for_each_entry(bo, &kgem->large, list) {
			assert(!bo->purged);
			assert(bo->refcnt == 0);
			assert(bo->reusable);

			if (bo->tiling) {
				if (bo->pitch < pitch) {
					DBG(("tiled and pitch too small: tiling=%d, (want %d), pitch=%d, need %d\n",
					     bo->tiling, tiling,
					     bo->pitch, pitch));
					continue;
				}
			} else
				bo->pitch = untiled_pitch;

			if (bo->pitch * tiled_height > bytes(bo))
				continue;

			kgem_bo_remove_from_active(kgem, bo);

			bo->unique_id = kgem_get_unique_id(kgem);
			bo->delta = 0;
			DBG(("  1:from active: pitch=%d, tiling=%d, handle=%d, id=%d\n",
			     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
			assert(bo->pitch*kgem_aligned_height(kgem, height, bo->tiling) <= kgem_bo_size(bo));
			bo->refcnt = 1;
			return bo;
		}

		goto create;
	}

	if (flags & (CREATE_CPU_MAP | CREATE_GTT_MAP)) {
		int for_cpu = !!(flags & CREATE_CPU_MAP);
		if (kgem->has_llc && tiling == I915_TILING_NONE)
			for_cpu = 1;
		/* We presume that we will need to upload to this bo,
		 * and so would prefer to have an active VMA.
		 */
		cache = &kgem->vma[for_cpu].inactive[bucket];
		do {
			list_for_each_entry(bo, cache, vma) {
				assert(bucket(bo) == bucket);
				assert(bo->refcnt == 0);
				assert(bo->map);
				assert(IS_CPU_MAP(bo->map) == for_cpu);
				assert(bo->rq == NULL);
				assert(list_is_empty(&bo->request));

				if (size > num_pages(bo)) {
					DBG(("inactive too small: %d < %d\n",
					     num_pages(bo), size));
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
				assert(bo->pitch*kgem_aligned_height(kgem, height, bo->tiling) <= kgem_bo_size(bo));
				bo->refcnt = 1;
				return bo;
			}
		} while (!list_is_empty(cache) && kgem_retire(kgem));
	}

	if (flags & CREATE_INACTIVE)
		goto skip_active_search;

	/* Best active match */
	retry = NUM_CACHE_BUCKETS - bucket;
	if (retry > 3 && (flags & CREATE_TEMPORARY) == 0)
		retry = 3;
search_again:
	assert(bucket < NUM_CACHE_BUCKETS);
	cache = &kgem->active[bucket][tiling];
	if (tiling) {
		tiled_height = kgem_aligned_height(kgem, height, tiling);
		list_for_each_entry(bo, cache, list) {
			assert(!bo->purged);
			assert(bo->refcnt == 0);
			assert(bucket(bo) == bucket);
			assert(bo->reusable);
			assert(bo->tiling == tiling);

			if (kgem->gen < 40) {
				if (bo->pitch < pitch) {
					DBG(("tiled and pitch too small: tiling=%d, (want %d), pitch=%d, need %d\n",
					     bo->tiling, tiling,
					     bo->pitch, pitch));
					continue;
				}

				if (bo->pitch * tiled_height > bytes(bo))
					continue;
			} else {
				if (num_pages(bo) < size)
					continue;

				if (bo->pitch != pitch) {
					gem_set_tiling(kgem->fd,
						       bo->handle,
						       tiling, pitch);

					bo->pitch = pitch;
				}
			}

			kgem_bo_remove_from_active(kgem, bo);

			bo->unique_id = kgem_get_unique_id(kgem);
			bo->delta = 0;
			DBG(("  1:from active: pitch=%d, tiling=%d, handle=%d, id=%d\n",
			     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
			assert(bo->pitch*kgem_aligned_height(kgem, height, bo->tiling) <= kgem_bo_size(bo));
			bo->refcnt = 1;
			return bo;
		}
	} else {
		list_for_each_entry(bo, cache, list) {
			assert(bucket(bo) == bucket);
			assert(!bo->purged);
			assert(bo->refcnt == 0);
			assert(bo->reusable);
			assert(bo->tiling == tiling);

			if (num_pages(bo) < size)
				continue;

			kgem_bo_remove_from_active(kgem, bo);

			bo->pitch = pitch;
			bo->unique_id = kgem_get_unique_id(kgem);
			bo->delta = 0;
			DBG(("  1:from active: pitch=%d, tiling=%d, handle=%d, id=%d\n",
			     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
			assert(bo->pitch*kgem_aligned_height(kgem, height, bo->tiling) <= kgem_bo_size(bo));
			bo->refcnt = 1;
			return bo;
		}
	}

	if (--retry && flags & CREATE_EXACT) {
		if (kgem->gen >= 40) {
			for (i = I915_TILING_NONE; i <= I915_TILING_Y; i++) {
				if (i == tiling)
					continue;

				cache = &kgem->active[bucket][i];
				list_for_each_entry(bo, cache, list) {
					assert(!bo->purged);
					assert(bo->refcnt == 0);
					assert(bo->reusable);

					if (num_pages(bo) < size)
						continue;

					if (tiling != gem_set_tiling(kgem->fd,
								     bo->handle,
								     tiling, pitch))
						continue;

					kgem_bo_remove_from_active(kgem, bo);

					bo->unique_id = kgem_get_unique_id(kgem);
					bo->pitch = pitch;
					bo->tiling = tiling;
					bo->delta = 0;
					DBG(("  1:from active: pitch=%d, tiling=%d, handle=%d, id=%d\n",
					     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
					assert(bo->pitch*kgem_aligned_height(kgem, height, bo->tiling) <= kgem_bo_size(bo));
					bo->refcnt = 1;
					return bo;
				}
			}
		}

		bucket++;
		goto search_again;
	}

	if ((flags & CREATE_EXACT) == 0) { /* allow an active near-miss? */
		untiled_pitch = kgem_untiled_pitch(kgem,
						   width, bpp,
						   flags & CREATE_SCANOUT);
		i = tiling;
		while (--i >= 0) {
			tiled_height = kgem_surface_size(kgem,
							 kgem->has_relaxed_fencing,
							 flags & CREATE_SCANOUT,
							 width, height, bpp, tiling, &pitch);
			cache = active(kgem, tiled_height / PAGE_SIZE, i);
			tiled_height = kgem_aligned_height(kgem, height, i);
			list_for_each_entry(bo, cache, list) {
				assert(!bo->purged);
				assert(bo->refcnt == 0);
				assert(bo->reusable);

				if (bo->tiling) {
					if (bo->pitch < pitch) {
						DBG(("tiled and pitch too small: tiling=%d, (want %d), pitch=%d, need %d\n",
						     bo->tiling, tiling,
						     bo->pitch, pitch));
						continue;
					}
				} else
					bo->pitch = untiled_pitch;

				if (bo->pitch * tiled_height > bytes(bo))
					continue;

				kgem_bo_remove_from_active(kgem, bo);

				bo->unique_id = kgem_get_unique_id(kgem);
				bo->delta = 0;
				DBG(("  1:from active: pitch=%d, tiling=%d, handle=%d, id=%d\n",
				     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
				assert(bo->pitch*kgem_aligned_height(kgem, height, bo->tiling) <= kgem_bo_size(bo));
				bo->refcnt = 1;
				return bo;
			}
		}
	}

skip_active_search:
	bucket = cache_bucket(size);
	retry = NUM_CACHE_BUCKETS - bucket;
	if (retry > 3)
		retry = 3;
search_inactive:
	/* Now just look for a close match and prefer any currently active */
	assert(bucket < NUM_CACHE_BUCKETS);
	cache = &kgem->inactive[bucket];
	list_for_each_entry_safe(bo, next, cache, list) {
		assert(bucket(bo) == bucket);
		assert(bo->reusable);

		if (size > num_pages(bo)) {
			DBG(("inactive too small: %d < %d\n",
			     num_pages(bo), size));
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
		assert((flags & CREATE_INACTIVE) == 0 || !kgem_busy(kgem, bo->handle));
		assert(bo->pitch*kgem_aligned_height(kgem, height, bo->tiling) <= kgem_bo_size(bo));
		bo->refcnt = 1;
		return bo;
	}

	if (flags & CREATE_INACTIVE && !list_is_empty(&kgem->requests)) {
		if (kgem_retire(kgem)) {
			flags &= ~CREATE_INACTIVE;
			goto search_inactive;
		}
	}

	if (--retry) {
		bucket++;
		goto search_inactive;
	}

create:
	handle = gem_create(kgem->fd, size);
	if (handle == 0)
		return NULL;

	bo = __kgem_bo_alloc(handle, size);
	if (!bo) {
		gem_close(kgem->fd, handle);
		return NULL;
	}

	bo->domain = DOMAIN_CPU;
	bo->unique_id = kgem_get_unique_id(kgem);
	bo->pitch = pitch;
	if (tiling != I915_TILING_NONE)
		bo->tiling = gem_set_tiling(kgem->fd, handle, tiling, pitch);

	assert(bytes(bo) >= bo->pitch * kgem_aligned_height(kgem, height, bo->tiling));

	DBG(("  new pitch=%d, tiling=%d, handle=%d, id=%d\n",
	     bo->pitch, bo->tiling, bo->handle, bo->unique_id));
	return bo;
}

struct kgem_bo *kgem_create_cpu_2d(struct kgem *kgem,
				   int width,
				   int height,
				   int bpp,
				   uint32_t flags)
{
	struct kgem_bo *bo;

	DBG(("%s(%dx%d, bpp=%d)\n", __FUNCTION__, width, height, bpp));

	if (kgem->has_llc) {
		bo = kgem_create_2d(kgem, width, height, bpp,
				    I915_TILING_NONE, flags);
		if (bo == NULL)
			return bo;

		if (kgem_bo_map__cpu(kgem, bo) == NULL) {
			_kgem_bo_destroy(kgem, bo);
			return NULL;
		}

		return bo;
	}

	if (kgem->has_vmap) {
		int stride, size;
		void *ptr;

		stride = ALIGN(width, 2) * bpp >> 3;
		stride = ALIGN(stride, 4);
		size = ALIGN(height, 2) * stride;

		assert(size >= PAGE_SIZE);

		/* XXX */
		//if (posix_memalign(&ptr, 64, ALIGN(size, 64)))
		if (posix_memalign(&ptr, PAGE_SIZE, ALIGN(size, PAGE_SIZE)))
			return NULL;

		bo = kgem_create_map(kgem, ptr, size, false);
		if (bo == NULL) {
			free(ptr);
			return NULL;
		}

		bo->map = MAKE_VMAP_MAP(ptr);
		bo->pitch = stride;
		return bo;
	}

	return NULL;
}

static void _kgem_bo_delete_partial(struct kgem *kgem, struct kgem_bo *bo)
{
	struct kgem_partial_bo *io = (struct kgem_partial_bo *)bo->proxy;

	if (list_is_empty(&io->base.list))
		return;

	DBG(("%s: size=%d, offset=%d, parent used=%d\n",
	     __FUNCTION__, bo->size.bytes, bo->delta, io->used));

	if (ALIGN(bo->delta + bo->size.bytes, 64) == io->used) {
		io->used = bo->delta;
		bubble_sort_partial(&kgem->active_partials, io);
	}
}

void _kgem_bo_destroy(struct kgem *kgem, struct kgem_bo *bo)
{
	DBG(("%s: handle=%d, proxy? %d\n",
	     __FUNCTION__, bo->handle, bo->proxy != NULL));

	if (bo->proxy) {
		_list_del(&bo->vma);
		_list_del(&bo->request);
		if (bo->io && (bo->exec == NULL || bo->proxy->rq == NULL))
			_kgem_bo_delete_partial(kgem, bo);
		kgem_bo_unref(kgem, bo->proxy);
		kgem_bo_binding_free(kgem, bo);
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
	int num_pages = 0;

	va_start(ap, kgem);
	while ((bo = va_arg(ap, struct kgem_bo *))) {
		if (bo->exec)
			continue;

		if (bo->proxy) {
			bo = bo->proxy;
			if (bo->exec)
				continue;
		}
		num_pages += num_pages(bo);
		num_exec++;
	}
	va_end(ap);

	if (!num_pages)
		return true;

	if (kgem->aperture > kgem->aperture_low)
		return false;

	if (num_pages + kgem->aperture > kgem->aperture_high)
		return false;

	if (kgem->nexec + num_exec >= KGEM_EXEC_SIZE(kgem))
		return false;

	return true;
}

bool kgem_check_bo_fenced(struct kgem *kgem, struct kgem_bo *bo)
{
	uint32_t size;

	if (bo->proxy)
		bo = bo->proxy;
	if (bo->exec) {
		if (kgem->gen < 40 &&
		    bo->tiling != I915_TILING_NONE &&
		    (bo->exec->flags & EXEC_OBJECT_NEEDS_FENCE) == 0) {
			if (kgem->nfence >= kgem->fence_max)
				return false;

			size = kgem->aperture_fenced;
			size += kgem_bo_fenced_size(kgem, bo);
			if (size > kgem->aperture_mappable)
				return false;
		}

		return true;
	}

	if (kgem->aperture > kgem->aperture_low)
		return false;

	if (kgem->nexec >= KGEM_EXEC_SIZE(kgem) - 1)
		return false;

	if (kgem->gen < 40 &&
	    bo->tiling != I915_TILING_NONE &&
	    kgem->nfence >= kgem->fence_max)
		return false;

	size = kgem->aperture;
	size += num_pages(bo);
	return size <= kgem->aperture_high;
}

bool kgem_check_many_bo_fenced(struct kgem *kgem, ...)
{
	va_list ap;
	struct kgem_bo *bo;
	int num_fence = 0;
	int num_exec = 0;
	int num_pages = 0;
	int fenced_size = 0;

	va_start(ap, kgem);
	while ((bo = va_arg(ap, struct kgem_bo *))) {
		if (bo->proxy)
			bo = bo->proxy;
		if (bo->exec) {
			if (kgem->gen >= 40 || bo->tiling == I915_TILING_NONE)
				continue;

			if ((bo->exec->flags & EXEC_OBJECT_NEEDS_FENCE) == 0) {
				fenced_size += kgem_bo_fenced_size(kgem, bo);
				num_fence++;
			}

			continue;
		}

		num_pages += num_pages(bo);
		num_exec++;
		if (kgem->gen < 40 && bo->tiling) {
			fenced_size += kgem_bo_fenced_size(kgem, bo);
			num_fence++;
		}
	}
	va_end(ap);

	if (fenced_size + kgem->aperture_fenced > kgem->aperture_mappable)
		return false;

	if (kgem->nfence + num_fence > kgem->fence_max)
		return false;

	if (!num_pages)
		return true;

	if (kgem->aperture > kgem->aperture_low)
		return false;

	if (num_pages + kgem->aperture > kgem->aperture_high)
		return false;

	if (kgem->nexec + num_exec >= KGEM_EXEC_SIZE(kgem))
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

	DBG(("%s: type=%d, count=%d (bucket: %d)\n",
	     __FUNCTION__, type, kgem->vma[type].count, bucket));
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

		VG(if (type) VALGRIND_FREELIKE_BLOCK(MAP(bo->map), 0));
		munmap(MAP(bo->map), bytes(bo));
		bo->map = NULL;
		list_del(&bo->vma);
		kgem->vma[type].count--;

		if (!bo->purged && !kgem_bo_set_purgeable(kgem, bo)) {
			DBG(("%s: freeing unpurgeable old mapping\n",
			     __FUNCTION__));
			kgem_bo_free(kgem, bo);
		}
	}
}

void *kgem_bo_map(struct kgem *kgem, struct kgem_bo *bo)
{
	void *ptr;

	DBG(("%s: handle=%d, offset=%d, tiling=%d, map=%p, domain=%d\n", __FUNCTION__,
	     bo->handle, bo->presumed_offset, bo->tiling, bo->map, bo->domain));

	assert(!bo->purged);
	assert(bo->proxy == NULL);
	assert(bo->exec == NULL);
	assert(list_is_empty(&bo->list));

	if (bo->tiling == I915_TILING_NONE && !bo->scanout &&
	    (kgem->has_llc || bo->domain == DOMAIN_CPU)) {
		DBG(("%s: converting request for GTT map into CPU map\n",
		     __FUNCTION__));
		ptr = kgem_bo_map__cpu(kgem, bo);
		kgem_bo_sync__cpu(kgem, bo);
		return ptr;
	}

	if (IS_CPU_MAP(bo->map))
		kgem_bo_release_map(kgem, bo);

	ptr = bo->map;
	if (ptr == NULL) {
		assert(kgem_bo_size(bo) <= kgem->aperture_mappable / 2);

		kgem_trim_vma_cache(kgem, MAP_GTT, bucket(bo));

		ptr = gem_mmap(kgem->fd, bo->handle, bytes(bo),
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

		DBG(("%s: sync: needs_flush? %d, domain? %d, busy? %d\n", __FUNCTION__,
		     bo->needs_flush, bo->domain, kgem_busy(kgem, bo->handle)));

		/* XXX use PROT_READ to avoid the write flush? */

		VG_CLEAR(set_domain);
		set_domain.handle = bo->handle;
		set_domain.read_domains = I915_GEM_DOMAIN_GTT;
		set_domain.write_domain = I915_GEM_DOMAIN_GTT;
		if (drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain) == 0) {
			kgem_bo_retire(kgem, bo);
			bo->domain = DOMAIN_GTT;
		}
	}

	return ptr;
}

void *kgem_bo_map__gtt(struct kgem *kgem, struct kgem_bo *bo)
{
	void *ptr;

	DBG(("%s: handle=%d, offset=%d, tiling=%d, map=%p, domain=%d\n", __FUNCTION__,
	     bo->handle, bo->presumed_offset, bo->tiling, bo->map, bo->domain));

	assert(!bo->purged);
	assert(bo->exec == NULL);
	assert(list_is_empty(&bo->list));

	if (IS_CPU_MAP(bo->map))
		kgem_bo_release_map(kgem, bo);

	ptr = bo->map;
	if (ptr == NULL) {
		assert(bytes(bo) <= kgem->aperture_mappable / 4);

		kgem_trim_vma_cache(kgem, MAP_GTT, bucket(bo));

		ptr = gem_mmap(kgem->fd, bo->handle, bytes(bo),
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

	return ptr;
}

void *kgem_bo_map__debug(struct kgem *kgem, struct kgem_bo *bo)
{
	if (bo->map)
		return MAP(bo->map);

	kgem_trim_vma_cache(kgem, MAP_GTT, bucket(bo));
	return bo->map = gem_mmap(kgem->fd, bo->handle, bytes(bo),
				  PROT_READ | PROT_WRITE);
}

void *kgem_bo_map__cpu(struct kgem *kgem, struct kgem_bo *bo)
{
	struct drm_i915_gem_mmap mmap_arg;

	DBG(("%s(handle=%d, size=%d)\n", __FUNCTION__, bo->handle, bytes(bo)));
	assert(!bo->purged);
	assert(list_is_empty(&bo->list));
	assert(!bo->scanout);

	if (IS_CPU_MAP(bo->map))
		return MAP(bo->map);

	if (bo->map)
		kgem_bo_release_map(kgem, bo);

	kgem_trim_vma_cache(kgem, MAP_CPU, bucket(bo));

	VG_CLEAR(mmap_arg);
	mmap_arg.handle = bo->handle;
	mmap_arg.offset = 0;
	mmap_arg.size = bytes(bo);
	if (drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_MMAP, &mmap_arg)) {
		ErrorF("%s: failed to mmap %d, %d bytes, into CPU domain\n",
		       __FUNCTION__, bo->handle, bytes(bo));
		return NULL;
	}

	VG(VALGRIND_MALLOCLIKE_BLOCK(mmap_arg.addr_ptr, bytes(bo), 0, 1));

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

#if defined(USE_VMAP)
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

	bo = __kgem_bo_alloc(handle, NUM_PAGES(size));
	if (bo == NULL) {
		gem_close(kgem->fd, handle);
		return NULL;
	}

	bo->reusable = false;
	bo->vmap = true;

	DBG(("%s(ptr=%p, size=%d, pages=%d, read_only=%d) => handle=%d\n",
	     __FUNCTION__, ptr, size, NUM_PAGES(size), read_only, handle));
	return bo;
}
#else
static uint32_t gem_vmap(int fd, void *ptr, int size, int read_only)
{
	assert(0);
	return 0;
}
struct kgem_bo *kgem_create_map(struct kgem *kgem,
				void *ptr, uint32_t size,
				bool read_only)
{
	assert(0);
	return 0;
}
#endif

void kgem_bo_sync__cpu(struct kgem *kgem, struct kgem_bo *bo)
{
	assert(bo->proxy == NULL);
	kgem_bo_submit(kgem, bo);

	if (bo->domain != DOMAIN_CPU) {
		struct drm_i915_gem_set_domain set_domain;

		DBG(("%s: sync: needs_flush? %d, domain? %d, busy? %d\n", __FUNCTION__,
		     bo->needs_flush, bo->domain, kgem_busy(kgem, bo->handle)));

		VG_CLEAR(set_domain);
		set_domain.handle = bo->handle;
		set_domain.read_domains = I915_GEM_DOMAIN_CPU;
		set_domain.write_domain = I915_GEM_DOMAIN_CPU;

		if (drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain) == 0) {
			kgem_bo_retire(kgem, bo);
			bo->domain = DOMAIN_CPU;
		}
	}
}

void kgem_bo_sync__gtt(struct kgem *kgem, struct kgem_bo *bo)
{
	assert(bo->proxy == NULL);
	kgem_bo_submit(kgem, bo);

	if (bo->domain != DOMAIN_GTT) {
		struct drm_i915_gem_set_domain set_domain;

		DBG(("%s: sync: needs_flush? %d, domain? %d, busy? %d\n", __FUNCTION__,
		     bo->needs_flush, bo->domain, kgem_busy(kgem, bo->handle)));

		VG_CLEAR(set_domain);
		set_domain.handle = bo->handle;
		set_domain.read_domains = I915_GEM_DOMAIN_GTT;
		set_domain.write_domain = I915_GEM_DOMAIN_GTT;

		if (drmIoctl(kgem->fd, DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain) == 0) {
			kgem_bo_retire(kgem, bo);
			bo->domain = DOMAIN_GTT;
		}
	}
}

void kgem_bo_set_sync(struct kgem *kgem, struct kgem_bo *bo)
{
	assert(!bo->reusable);
	assert(list_is_empty(&bo->list));
	list_add(&bo->list, &kgem->sync_list);
	bo->sync = true;
}

void kgem_sync(struct kgem *kgem)
{
	struct kgem_request *rq;
	struct kgem_bo *bo;

	DBG(("%s\n", __FUNCTION__));

	rq = kgem->sync;
	if (rq == NULL)
		return;

	if (rq == kgem->next_request)
		_kgem_submit(kgem);

	kgem_bo_sync__gtt(kgem, rq->bo);
	list_for_each_entry(bo, &kgem->sync_list, list)
		kgem_bo_sync__cpu(kgem, bo);

	assert(kgem->sync == NULL);
}

void kgem_clear_dirty(struct kgem *kgem)
{
	struct kgem_request *rq = kgem->next_request;
	struct kgem_bo *bo;

	list_for_each_entry(bo, &rq->buffers, request)
		bo->dirty = false;
}

struct kgem_bo *kgem_create_proxy(struct kgem_bo *target,
				  int offset, int length)
{
	struct kgem_bo *bo;

	DBG(("%s: target handle=%d, offset=%d, length=%d, io=%d\n",
	     __FUNCTION__, target->handle, offset, length, target->io));

	bo = __kgem_bo_alloc(target->handle, length);
	if (bo == NULL)
		return NULL;

	bo->reusable = false;
	bo->size.bytes = length;

	bo->io = target->io;
	bo->dirty = target->dirty;
	bo->tiling = target->tiling;
	bo->pitch = target->pitch;

	if (target->proxy) {
		offset += target->delta;
		target = target->proxy;
	}
	bo->proxy = kgem_bo_reference(target);
	bo->delta = offset;
	return bo;
}

static struct kgem_partial_bo *partial_bo_alloc(int num_pages)
{
	struct kgem_partial_bo *bo;

	bo = malloc(sizeof(*bo) + 128 + num_pages * PAGE_SIZE);
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
	struct kgem_bo *old;

	DBG(("%s: size=%d, flags=%x [write?=%d, inplace?=%d, last?=%d]\n",
	     __FUNCTION__, size, flags,
	     !!(flags & KGEM_BUFFER_WRITE),
	     !!(flags & KGEM_BUFFER_INPLACE),
	     !!(flags & KGEM_BUFFER_LAST)));
	assert(size);
	/* we should never be asked to create anything TOO large */
	assert(size <= kgem->max_object_size);

	if (kgem->has_llc)
		flags &= ~KGEM_BUFFER_INPLACE;

	list_for_each_entry(bo, &kgem->active_partials, base.list) {
		assert(bo->base.io);
		assert(bo->base.refcnt >= 1);

		/* We can reuse any write buffer which we can fit */
		if (flags == KGEM_BUFFER_LAST &&
		    bo->write == KGEM_BUFFER_WRITE &&
		    !bo->mmapped && size <= bytes(&bo->base)) {
			DBG(("%s: reusing write buffer for read of %d bytes? used=%d, total=%d\n",
			     __FUNCTION__, size, bo->used, bytes(&bo->base)));
			gem_write(kgem->fd, bo->base.handle,
				  0, bo->used, bo->mem);
			kgem_partial_buffer_release(kgem, bo);
			bo->need_io = 0;
			bo->write = 0;
			offset = 0;
			bo->used = size;
			bubble_sort_partial(&kgem->active_partials, bo);
			goto done;
		}

		if (flags & KGEM_BUFFER_WRITE) {
			if ((bo->write & KGEM_BUFFER_WRITE) == 0 ||
			    (((bo->write & ~flags) & KGEM_BUFFER_INPLACE) &&
			     !bo->base.vmap)) {
				DBG(("%s: skip write %x buffer, need %x\n",
				     __FUNCTION__, bo->write, flags));
				continue;
			}
			assert(bo->mmapped || bo->need_io);
		} else {
			if (bo->write & KGEM_BUFFER_WRITE) {
				DBG(("%s: skip write %x buffer, need %x\n",
				     __FUNCTION__, bo->write, flags));
				continue;
			}
		}

		if (bo->used && bo->base.rq == NULL && bo->base.refcnt == 1) {
			bo->used = 0;
			bubble_sort_partial(&kgem->active_partials, bo);
		}

		if (bo->used + size <= bytes(&bo->base)) {
			DBG(("%s: reusing partial buffer? used=%d + size=%d, total=%d\n",
			     __FUNCTION__, bo->used, size, bytes(&bo->base)));
			offset = bo->used;
			bo->used += size;
			goto done;
		}

		DBG(("%s: too small (%d < %d)\n",
		     __FUNCTION__, bytes(&bo->base) - bo->used, size));
		break;
	}

	if (flags & KGEM_BUFFER_WRITE) {
		do list_for_each_entry_reverse(bo, &kgem->inactive_partials, base.list) {
			assert(bo->base.io);
			assert(bo->base.refcnt == 1);

			if (size > bytes(&bo->base))
				continue;

			if (((bo->write & ~flags) & KGEM_BUFFER_INPLACE) &&
			    !bo->base.vmap) {
				DBG(("%s: skip write %x buffer, need %x\n",
				     __FUNCTION__, bo->write, flags));
				continue;
			}

			DBG(("%s: reusing inactive partial buffer? size=%d, total=%d\n",
			     __FUNCTION__, size, bytes(&bo->base)));
			offset = 0;
			bo->used = size;
			list_move(&bo->base.list, &kgem->active_partials);

			if (bo->mmapped) {
				if (IS_CPU_MAP(bo->base.map))
					kgem_bo_sync__cpu(kgem, &bo->base);
				else
					kgem_bo_sync__gtt(kgem, &bo->base);
			}

			goto done;
		} while (kgem_retire(kgem));
	}

#if !DBG_NO_MAP_UPLOAD
	/* Be a little more generous and hope to hold fewer mmappings */
	alloc = ALIGN(2*size, kgem->partial_buffer_size);
	if (alloc > MAX_CACHE_SIZE)
		alloc = ALIGN(size, kgem->partial_buffer_size);
	if (alloc > MAX_CACHE_SIZE)
		alloc = PAGE_ALIGN(size);
	alloc /= PAGE_SIZE;
	if (kgem->has_llc) {
		bo = malloc(sizeof(*bo));
		if (bo == NULL)
			return NULL;

		old = NULL;
		if ((flags & KGEM_BUFFER_WRITE) == 0)
			old = search_linear_cache(kgem, alloc, CREATE_CPU_MAP);
		if (old == NULL)
			old = search_linear_cache(kgem, alloc, CREATE_INACTIVE | CREATE_CPU_MAP);
		if (old == NULL)
			old = search_linear_cache(kgem, NUM_PAGES(size), CREATE_INACTIVE | CREATE_CPU_MAP);
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

			alloc = num_pages(&bo->base);
			goto init;
		} else {
			bo->base.refcnt = 0; /* for valgrind */
			kgem_bo_free(kgem, &bo->base);
			bo = NULL;
		}
	}

	if (PAGE_SIZE * alloc > kgem->aperture_mappable / 4)
		flags &= ~KGEM_BUFFER_INPLACE;

	if ((flags & KGEM_BUFFER_WRITE_INPLACE) == KGEM_BUFFER_WRITE_INPLACE) {
		/* The issue with using a GTT upload buffer is that we may
		 * cause eviction-stalls in order to free up some GTT space.
		 * An is-mappable? ioctl could help us detect when we are
		 * about to block, or some per-page magic in the kernel.
		 *
		 * XXX This is especially noticeable on memory constrained
		 * devices like gen2 or with relatively slow gpu like i3.
		 */
		old = search_linear_cache(kgem, alloc,
					  CREATE_EXACT | CREATE_INACTIVE | CREATE_GTT_MAP);
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
		if (old == NULL)
			old = search_linear_cache(kgem, NUM_PAGES(size),
						  CREATE_EXACT | CREATE_INACTIVE | CREATE_GTT_MAP);
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

			assert(bo->base.tiling == I915_TILING_NONE);
			assert(num_pages(&bo->base) >= NUM_PAGES(size));

			bo->mem = kgem_bo_map(kgem, &bo->base);
			if (bo->mem) {
				bo->need_io = false;
				bo->base.io = true;
				bo->mmapped = true;
				bo->base.refcnt = 1;

				alloc = num_pages(&bo->base);
				goto init;
			} else {
				kgem_bo_free(kgem, &bo->base);
				bo = NULL;
			}
		}
	}
#else
	flags &= ~KGEM_BUFFER_INPLACE;
#endif
	/* Be more parsimonious with pwrite/pread buffers */
	if ((flags & KGEM_BUFFER_INPLACE) == 0)
		alloc = NUM_PAGES(size);
	flags &= ~KGEM_BUFFER_INPLACE;

	if (kgem->has_vmap) {
		bo = partial_bo_alloc(alloc);
		if (bo) {
			if (!__kgem_bo_init(&bo->base,
					    gem_vmap(kgem->fd, bo->mem,
						     alloc * PAGE_SIZE, false),
					    alloc)) {
				free(bo);
				return NULL;
			}

			DBG(("%s: created vmap handle=%d for buffer\n",
			     __FUNCTION__, bo->base.handle));

			bo->need_io = false;
			bo->base.io = true;
			bo->base.vmap = true;
			bo->mmapped = true;

			goto init;
		}
	}

	old = NULL;
	if ((flags & KGEM_BUFFER_WRITE) == 0)
		old = search_linear_cache(kgem, alloc, 0);
	if (old == NULL)
		old = search_linear_cache(kgem, alloc, CREATE_INACTIVE);
	if (old) {
		DBG(("%s: reusing ordinary handle %d for io\n",
		     __FUNCTION__, old->handle));
		alloc = num_pages(old);
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

		bo->need_io = flags & KGEM_BUFFER_WRITE;
		bo->base.io = true;
	} else {
		bo = malloc(sizeof(*bo));
		if (bo == NULL)
			return NULL;

		old = search_linear_cache(kgem, alloc,
					  CREATE_INACTIVE | CREATE_CPU_MAP);
		if (old) {
			DBG(("%s: reusing cpu map handle=%d for buffer\n",
			     __FUNCTION__, old->handle));
			alloc = num_pages(old);

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
		if (bo->mem != NULL) {
			if (flags & KGEM_BUFFER_WRITE)
				kgem_bo_sync__cpu(kgem, &bo->base);

			bo->need_io = false;
			bo->base.io = true;
			bo->mmapped = true;
			goto init;
		}

		DBG(("%s: failing back to new pwrite buffer\n", __FUNCTION__));
		old = &bo->base;
		bo = partial_bo_alloc(alloc);
		if (bo == NULL) {
			free(old);
			return NULL;
		}

		memcpy(&bo->base, old, sizeof(*old));
		free(old);

		assert(bo->mem);
		assert(!bo->mmapped);

		list_init(&bo->base.request);
		list_init(&bo->base.vma);
		list_init(&bo->base.list);
		bo->base.refcnt = 1;
		bo->need_io = flags & KGEM_BUFFER_WRITE;
		bo->base.io = true;
	}
init:
	bo->base.reusable = false;
	assert(num_pages(&bo->base) == alloc);
	assert(bo->base.io);
	assert(!bo->need_io || !bo->base.needs_flush);
	assert(!bo->need_io || bo->base.domain != DOMAIN_GPU);

	bo->used = size;
	bo->write = flags & KGEM_BUFFER_WRITE_INPLACE;
	offset = 0;

	assert(list_is_empty(&bo->base.list));
	list_add(&bo->base.list, &kgem->active_partials);
	DBG(("%s(pages=%d) new handle=%d\n",
	     __FUNCTION__, alloc, bo->base.handle));

done:
	bo->used = ALIGN(bo->used, 64);
	/* adjust the position within the list to maintain decreasing order */
	alloc = bytes(&bo->base) - bo->used;
	{
		struct kgem_partial_bo *p, *first;

		first = p = list_first_entry(&bo->base.list,
					     struct kgem_partial_bo,
					     base.list);
		while (&p->base.list != &kgem->active_partials &&
		       alloc < bytes(&p->base) - p->used) {
			DBG(("%s: this=%d, right=%d\n",
			     __FUNCTION__, alloc, bytes(&p->base) -p->used));
			p = list_first_entry(&p->base.list,
					     struct kgem_partial_bo,
					     base.list);
		}
		if (p != first)
			list_move_tail(&bo->base.list, &p->base.list);
		assert(validate_partials(kgem));
	}
	assert(bo->mem);
	*ret = (char *)bo->mem + offset;
	return kgem_create_proxy(&bo->base, offset, size);
}

bool kgem_buffer_is_inplace(struct kgem_bo *_bo)
{
	struct kgem_partial_bo *bo = (struct kgem_partial_bo *)_bo->proxy;
	return bo->write & KGEM_BUFFER_WRITE_INPLACE;
}

struct kgem_bo *kgem_create_buffer_2d(struct kgem *kgem,
				      int width, int height, int bpp,
				      uint32_t flags,
				      void **ret)
{
	struct kgem_bo *bo;
	int stride;

	assert(width > 0 && height > 0);
	assert(ret != NULL);
	stride = ALIGN(width, 2) * bpp >> 3;
	stride = ALIGN(stride, 4);

	DBG(("%s: %dx%d, %d bpp, stride=%d\n",
	     __FUNCTION__, width, height, bpp, stride));

	bo = kgem_create_buffer(kgem, stride * ALIGN(height, 2), flags, ret);
	if (bo == NULL) {
		DBG(("%s: allocation failure for upload buffer\n",
		     __FUNCTION__));
		return NULL;
	}
	assert(*ret != NULL);

	if (height & 1) {
		struct kgem_partial_bo *io = (struct kgem_partial_bo *)bo->proxy;
		int min;

		assert(io->used);

		/* Having padded this surface to ensure that accesses to
		 * the last pair of rows is valid, remove the padding so
		 * that it can be allocated to other pixmaps.
		 */
		min = bo->delta + height * stride;
		min = ALIGN(min, 64);
		if (io->used != min) {
			DBG(("%s: trimming partial buffer from %d to %d\n",
			     __FUNCTION__, io->used, min));
			io->used = min;
			bubble_sort_partial(&kgem->active_partials, io);
		}
		bo->size.bytes -= stride;
	}

	bo->pitch = stride;
	bo->unique_id = kgem_get_unique_id(kgem);
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

	assert(data);
	assert(width > 0);
	assert(height > 0);
	assert(stride);
	assert(bpp);

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

void kgem_proxy_bo_attach(struct kgem_bo *bo,
			  struct kgem_bo **ptr)
{
	DBG(("%s: handle=%d\n", __FUNCTION__, bo->handle));
	assert(bo->map == NULL);
	assert(bo->proxy);
	list_add(&bo->vma, &bo->proxy->vma);
	bo->map = ptr;
	*ptr = kgem_bo_reference(bo);
}

void kgem_buffer_read_sync(struct kgem *kgem, struct kgem_bo *_bo)
{
	struct kgem_partial_bo *bo;
	uint32_t offset = _bo->delta, length = _bo->size.bytes;

	assert(_bo->io);
	assert(_bo->exec == &_kgem_dummy_exec);
	assert(_bo->rq == NULL);
	assert(_bo->proxy);

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
		set_domain.write_domain = 0;
		set_domain.read_domains =
			IS_CPU_MAP(bo->base.map) ? I915_GEM_DOMAIN_CPU : I915_GEM_DOMAIN_GTT;

		if (drmIoctl(kgem->fd,
			     DRM_IOCTL_I915_GEM_SET_DOMAIN, &set_domain))
			return;
	} else {
		if (gem_read(kgem->fd,
			     bo->base.handle, (char *)bo->mem+offset,
			     offset, length))
			return;

		kgem_bo_map__cpu(kgem, &bo->base);
	}
	kgem_bo_retire(kgem, &bo->base);
	bo->base.domain = DOMAIN_NONE;
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

void kgem_bo_clear_scanout(struct kgem *kgem, struct kgem_bo *bo)
{
	bo->needs_flush = true;
	bo->reusable = true;
	bo->flush = false;

	if (!bo->scanout)
		return;

	bo->scanout = false;
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
	size = PAGE_ALIGN(size) / PAGE_SIZE;

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
	    !kgem_check_many_bo_fenced(kgem, src, dst, NULL)) {
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
