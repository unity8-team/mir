#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "nouveau_drmif.h"

int
nouveau_device_open_existing(struct nouveau_device **userdev, int close,
			     int fd, drm_context_t ctx)
{
	struct nouveau_device_priv *nv;

	if (!userdev || *userdev)
	    return -EINVAL;

	nv = calloc(1, sizeof(*nv));
	if (!nv)
	    return -ENOMEM;
	nv->fd = fd;
	nv->ctx = ctx;
	nv->needs_close = close;

	drmCommandNone(nv->fd, DRM_NOUVEAU_CARD_INIT);

	*userdev = &nv->base;
	return 0;
}

int
nouveau_device_open(struct nouveau_device **userdev, const char *busid)
{
	drm_context_t ctx;
	int fd, ret;

	if (!userdev || *userdev)
		return -EINVAL;

	fd = drmOpen("nouveau", busid);
	if (fd < 0)
		return -EINVAL;

	if ((ret = drmCreateContext(fd, &ctx))) {
		drmClose(fd);
		return ret;
	}

	if ((ret = nouveau_device_open_existing(userdev, 1, fd, ctx))) {
	    drmDestroyContext(fd, ctx);
	    drmClose(fd);
	    return ret;
	}

	return 0;
}

void
nouveau_device_close(struct nouveau_device **userdev)
{
	struct nouveau_device_priv *nv;

	if (userdev || !*userdev)
		return;
	nv = (struct nouveau_device_priv *)*userdev;
	*userdev = NULL;

	if (nv->needs_close) {
		drmDestroyContext(nv->fd, nv->ctx);
		drmClose(nv->fd);
	}
	free(nv);
}

int
nouveau_device_get_param(struct nouveau_device *userdev,
			 uint64_t param, uint64_t *value)
{
	struct nouveau_device_priv *nv = (struct nouveau_device_priv *)userdev;
	struct drm_nouveau_getparam getp;
	int ret;

	if (!nv || !value)
		return -EINVAL;

	getp.param = param;
	if ((ret = drmCommandWriteRead(nv->fd, DRM_NOUVEAU_GETPARAM,
				       &getp, sizeof(getp))))
		return ret;

	*value = getp.value;
	return 0;
}

int
nouveau_device_set_param(struct nouveau_device *userdev,
			 uint64_t param, uint64_t value)
{
	struct nouveau_device_priv *nv = (struct nouveau_device_priv *)userdev;
	struct drm_nouveau_setparam setp;
	int ret;

	if (!nv)
		return -EINVAL;

	setp.param = param;
	setp.value = value;
	if ((ret = drmCommandWriteRead(nv->fd, DRM_NOUVEAU_SETPARAM,
				       &setp, sizeof(setp))))
		return ret;

	return 0;
}

