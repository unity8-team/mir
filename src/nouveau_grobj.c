#include <stdlib.h>
#include <errno.h>

#include "nouveau_drmif.h"

int
nouveau_grobj_alloc(struct nouveau_channel *userchan, uint32_t handle,
		    int class, struct nouveau_grobj **usergrobj)
{
	struct nouveau_device_priv *nv = nouveau_device(userchan->device);
	struct nouveau_grobj_priv *gr;
	struct drm_nouveau_grobj_alloc g;
	int ret;

	if (!nv || !usergrobj || *usergrobj)
		return -EINVAL;

	gr = calloc(1, sizeof(*gr));
	if (!gr)
		return -ENOMEM;
	gr->base.channel = userchan;
	gr->base.handle  = handle;
	gr->base.grclass = class;

	g.channel = userchan->id;
	g.handle  = handle;
	g.class   = class;
	ret = drmCommandWrite(nv->fd, DRM_NOUVEAU_GROBJ_ALLOC, &g, sizeof(g));
	if (ret) {
		nouveau_grobj_free((void *)&gr);
		return ret;
	}

	*usergrobj = &gr->base;
	return 0;
}

void
nouveau_grobj_free(struct nouveau_grobj **usergrobj)
{
	struct nouveau_grobj_priv *gr;

	if (!usergrobj)
		return;
	gr = nouveau_grobj(*usergrobj);
	*usergrobj = NULL;

	if (gr) {
		struct nouveau_channel_priv *chan;
		struct nouveau_device_priv *nv;
		struct drm_nouveau_gpuobj_free f;

		chan = nouveau_channel(gr->base.channel);
		nv   = nouveau_device(chan->base.device);

		f.channel = chan->drm.channel;
		f.handle  = gr->base.handle;
		drmCommandWrite(nv->fd, DRM_NOUVEAU_GPUOBJ_FREE, &f, sizeof(f));		
		free(gr);
	}
}

