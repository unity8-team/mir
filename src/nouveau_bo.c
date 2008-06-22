/*
 * Copyright 2007 Nouveau Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdint.h>
#include <errno.h>

#include "nouveau_drmif.h"
#include "nouveau_dma.h"
#include "nouveau_local.h"

int
nouveau_bo_new(struct nouveau_device *userdev, uint32_t flags, int align,
	       int size, struct nouveau_bo **userbo)
{
	struct nouveau_device_priv *nv = nouveau_device(userdev);
	struct nouveau_bo_priv *bo;
	int ret;

	if (!nv || !userbo || *userbo)
		return -EINVAL;

	bo = calloc(1, sizeof(*bo));
	if (!bo)
		return -ENOMEM;
	bo->base.device = userdev;

	if (flags & NOUVEAU_BO_VRAM)
		bo->drm.flags |= NOUVEAU_MEM_FB;
	if (flags & NOUVEAU_BO_GART)
		bo->drm.flags |= (NOUVEAU_MEM_AGP | NOUVEAU_MEM_PCI);
	bo->drm.flags |= NOUVEAU_MEM_MAPPED;

	bo->drm.size = size;
	bo->drm.alignment = align;

	ret = drmCommandWriteRead(nv->fd, DRM_NOUVEAU_MEM_ALLOC, &bo->drm,
				  sizeof(bo->drm));
	if (ret) {
		free(bo);
		return ret;
	}

	ret = drmMap(nv->fd, bo->drm.map_handle, bo->drm.size, &bo->map);
	if (ret) {
		bo->map = NULL;
		nouveau_bo_del((void *)&bo);
		return ret;
	}

	bo->base.size = bo->drm.size;
	bo->base.offset = bo->drm.offset;
	bo->base.handle = (unsigned long)bo;
	bo->base.map_handle = bo->drm.map_handle;
	bo->refcount = 1;
	*userbo = &bo->base;
	return 0;
}

int
nouveau_bo_ref(struct nouveau_device *userdev, uint64_t handle,
	       struct nouveau_bo **userbo)
{
	struct nouveau_bo_priv *bo = (void *)(unsigned long)handle;

	if (!bo || !userbo || *userbo)
		return -EINVAL;

	bo->refcount++;
	*userbo = &bo->base;
	return 0;
}

void
nouveau_bo_del(struct nouveau_bo **userbo)
{
	struct drm_nouveau_mem_free f;
	struct nouveau_bo_priv *bo;

	if (!userbo || !*userbo)
		return;
	bo = nouveau_bo(*userbo);
	*userbo = NULL;

	if (--bo->refcount)
		return;

	if (bo->map) {
		drmUnmap(bo->map, bo->drm.size);
		bo->map = NULL;
	}

	f.flags = bo->drm.flags;
	f.offset = bo->drm.offset;
	drmCommandWrite(nouveau_device(bo->base.device)->fd,
			DRM_NOUVEAU_MEM_FREE, &f, sizeof(f));

	free(bo);
}

int
nouveau_bo_map(struct nouveau_bo *userbo, uint32_t flags)
{
	struct nouveau_bo_priv *bo = nouveau_bo(userbo);

	if (!bo)
		return -EINVAL;
	userbo->map = bo->map;
	return 0;
}

void
nouveau_bo_unmap(struct nouveau_bo *userbo)
{
	userbo->map = NULL;
}

void
nouveau_bo_emit_reloc(struct nouveau_channel *userchan, void *ptr,
		      struct nouveau_bo *userbo, uint32_t data, uint32_t flags,
		      uint32_t vor, uint32_t tor)
{
	struct nouveau_channel_priv *chan = nouveau_channel(userchan);
	struct nouveau_bo_priv *bo = nouveau_bo(userbo);
	struct nouveau_bo_reloc *r;

	if (chan->num_relocs >= chan->max_relocs)
		FIRE_RING_CH(userchan);
	r = &chan->relocs[chan->num_relocs++];

	r->ptr = ptr;
	r->bo = bo;
	r->data = data;
	r->flags = flags;
	r->vor = vor;
	r->tor = tor;
}

void
nouveau_bo_validate(struct nouveau_channel *userchan)
{
}

