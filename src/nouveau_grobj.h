#ifndef __NOUVEAU_GROBJ_H__
#define __NOUVEAU_GROBJ_H__

#include "nouveau_channel.h"

struct nouveau_grobj {
	struct nouveau_channel *channel;
	int grclass;
	uint32_t handle;
};

#endif
