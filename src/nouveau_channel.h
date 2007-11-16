#ifndef __NOUVEAU_CHANNEL_H__
#define __NOUVEAU_CHANNEL_H__

#include "nouveau_device.h"

struct nouveau_channel {
	struct nouveau_device *device;
	int id;
};

#endif
