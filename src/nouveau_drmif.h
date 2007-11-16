#ifndef __NOUVEAU_DRMIF_H__
#define __NOUVEAU_DRMIF_H__

#include <stdint.h>
#include <xf86drm.h>
#include <nouveau_drm.h>

#include "nouveau_device.h"

struct nouveau_device_priv {
	struct nouveau_device base;

	int fd;
	drm_context_t ctx;
	drmLock *lock;
	int needs_close;
};

extern int  nouveau_device_open_existing(struct nouveau_device **, int close,
					 int fd, drm_context_t ctx);
extern int  nouveau_device_open(struct nouveau_device **, const char *busid);
extern void nouveau_device_close(struct nouveau_device **);
extern int  nouveau_device_get_param(struct nouveau_device *, uint64_t param,
				     uint64_t *v);
extern int  nouveau_device_set_param(struct nouveau_device *, uint64_t param,
				     uint64_t val);

#if 0
extern int nouveau_channel_alloc(struct nouveau_device *,
				 uint32_t fb, uint32_t tt,
				 struct nouveau_channel **);
extern void nouveau_channel_free(struct nouveau_channel **);

extern int nouveau_grobj_alloc(struct nouveau_channel *, uint32_t handle,
			       int class, struct nouveau_grobj **);
extern void nouveau_grobj_free(struct nouveau_grobj **);

extern int nouveau_notifier_alloc(struct nouveau_channel *, uint32_t handle,
				  int count, struct nouveau_grobj **);
extern void nouveau_notifier_free(struct nouveau_grobj **);
extern void nouveau_notifier_reset(struct nouveau_grobj *, int id);
extern uint32_t nouveau_notifier_status(struct nouveau_grobj *, int id);
extern uint32_t nouveau_notifier_return_val(struct nouveau_grobj *, int id);
extern int nouveau_notifier_wait_status(struct nouveau_grobj *, int id,
					int status, int timeout);
#endif

#endif
