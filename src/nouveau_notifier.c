#include <stdlib.h>
#include <errno.h>

#include "nouveau_drmif.h"
#include "nouveau_local.h"

#define NOTIFIER(__v)                                                          \
	struct nouveau_notifier_priv *notifier = nouveau_notifier(user);       \
	volatile uint32_t *n = (void*)notifier->map + (id * 32)

int
nouveau_notifier_alloc(struct nouveau_channel *userchan, uint32_t handle,
		       int count, struct nouveau_notifier **usernotifier)
{
	struct nouveau_notifier_priv *notifier;
	int ret;

	if (!userchan || !usernotifier || *usernotifier)
		return -EINVAL;

	notifier = calloc(1, sizeof(*notifier));
	if (!notifier)
		return -ENOMEM;
	notifier->base.channel = userchan;
	notifier->base.handle  = handle;

	notifier->drm.channel = userchan->id;
	notifier->drm.handle  = handle;
	notifier->drm.count   = count;
	if ((ret = drmCommandWriteRead(nouveau_device(userchan->device)->fd,
				       DRM_NOUVEAU_NOTIFIEROBJ_ALLOC,
				       &notifier->drm,
				       sizeof(notifier->drm)))) {
		nouveau_notifier_free((void *)&notifier);
		return ret;
	}

	notifier->map = (void *)nouveau_channel(userchan)->notifier_block +
				notifier->drm.offset;
	*usernotifier = &notifier->base;
	return 0;
}

void
nouveau_notifier_free(struct nouveau_notifier **usernotifier)
{

	struct nouveau_notifier_priv *notifier;

	if (!usernotifier)
		return;
	notifier = nouveau_notifier(*usernotifier);
	*usernotifier = NULL;

	if (notifier) {
		struct nouveau_channel_priv *chan;
		struct nouveau_device_priv *nv;
		struct drm_nouveau_gpuobj_free f;

		chan = nouveau_channel(notifier->base.channel);
		nv   = nouveau_device(chan->base.device);

		f.channel = chan->drm.channel;
		f.handle  = notifier->base.handle;
		drmCommandWrite(nv->fd, DRM_NOUVEAU_GPUOBJ_FREE, &f, sizeof(f));		
		free(notifier);
	}
}

void
nouveau_notifier_reset(struct nouveau_notifier *user, int id)
{
	NOTIFIER(n);

	n[NV_NOTIFY_TIME_0      /4] = 0x00000000;
	n[NV_NOTIFY_TIME_1      /4] = 0x00000000;
	n[NV_NOTIFY_RETURN_VALUE/4] = 0x00000000;
	n[NV_NOTIFY_STATE       /4] = (NV_NOTIFY_STATE_STATUS_IN_PROCESS <<
				       NV_NOTIFY_STATE_STATUS_SHIFT);
}

uint32_t
nouveau_notifier_status(struct nouveau_notifier *user, int id)
{
	NOTIFIER(n);

	return n[NV_NOTIFY_STATE/4] >> NV_NOTIFY_STATE_STATUS_SHIFT;
}

uint32_t
nouveau_notifier_return_val(struct nouveau_notifier *user, int id)
{
	NOTIFIER(n);

	return n[NV_NOTIFY_RETURN_VALUE/4];
}

int
nouveau_notifier_wait_status(struct nouveau_notifier *user, int id,
			     int status, int timeout)
{
	NOTIFIER(n);
	uint32_t time = 0, t_start = NOUVEAU_TIME_MSEC();

	while (time <= timeout) {
		uint32_t v;

		v = n[NV_NOTIFY_STATE/4] >> NV_NOTIFY_STATE_STATUS_SHIFT;
		if (v == status)
			return 0;

		if (timeout)
			time = NOUVEAU_TIME_MSEC() - t_start;
	}

	return -EBUSY;
}

