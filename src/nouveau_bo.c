#include <stdint.h>
#include <errno.h>

#include "nouveau_drmif.h"
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
	*userbo = &bo->base;
	return 0;
}

int
nouveau_bo_ref(struct nouveau_device *userdev, uint32_t handle,
	       struct nouveau_bo **userbo)
{
	return -EINVAL;
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

	if (!bo || userbo->map)
		return -EINVAL;
	userbo->map = bo->map;
	return 0;
}

void
nouveau_bo_unmap(struct nouveau_bo *userbo)
{
	userbo->map = NULL;
}

