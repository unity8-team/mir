/*
 * Copyright 2007 Ben Skeggs
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

#include "nv_include.h"

NVAllocRec *NVAllocateMemory(NVPtr pNv, int type, int size)
{
	struct nouveau_device_priv *nv = (struct nouveau_device_priv *)pNv->dev;
	struct drm_nouveau_mem_alloc memalloc;
	NVAllocRec *mem;

	mem = malloc(sizeof(NVAllocRec));
	if (!mem)
		return NULL;

	memalloc.flags         = type | NOUVEAU_MEM_MAPPED;
	memalloc.size          = size;
	memalloc.alignment     = 0;
	if (drmCommandWriteRead(nv->fd, DRM_NOUVEAU_MEM_ALLOC, &memalloc,
				sizeof(memalloc))) {
		ErrorF("NOUVEAU_MEM_ALLOC failed.  "
			"flags=0x%08x, size=%lld (%d)\n",
			memalloc.flags, (long long)memalloc.size, errno);
		free(mem);
		return NULL;
	}
	mem->type   = memalloc.flags;
	mem->size   = memalloc.size;
	mem->offset = memalloc.offset;

	if (drmMap(nv->fd, memalloc.map_handle, mem->size, &mem->map)) {
		ErrorF("drmMap() failed. handle=0x%x, size=%lld (%d)\n",
		       memalloc.map_handle, (long long)mem->size, errno);
		mem->map  = NULL;
		NVFreeMemory(pNv, mem);
		return NULL;
	}

	return mem;
}

void NVFreeMemory(NVPtr pNv, NVAllocRec *mem)
{
	struct nouveau_device_priv *nv = (struct nouveau_device_priv *)pNv->dev;
	struct drm_nouveau_mem_free memfree;

	if (mem) {
		if (mem->map) {
			if (drmUnmap(mem->map, mem->size))
				ErrorF("drmUnmap() failed.  "
					"map=%p, size=%lld\n",
					mem->map, (long long)mem->size);
		}

		memfree.flags = mem->type;
		memfree.offset = mem->offset;

		if (drmCommandWriteRead(nv->fd,
					DRM_NOUVEAU_MEM_FREE, &memfree,
					sizeof(memfree))) {
			ErrorF("NOUVEAU_MEM_FREE failed.  "
				"flags=0x%08x, offset=0x%llx (%d)\n",
				mem->type, (long long)mem->size, errno);
		}
		free(mem);
	}
}

