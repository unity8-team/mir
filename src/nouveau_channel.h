#ifndef __NOUVEAU_CHANNEL_H__
#define __NOUVEAU_CHANNEL_H__

#include "nouveau_device.h"

struct nouveau_channel {
	struct nouveau_device *device;
	int id;

	uint32_t vram_handle;
	uint32_t gart_handle;
};

#endif
