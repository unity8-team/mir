#ifndef __NOUVEAU_GROBJ_H__
#define __NOUVEAU_GROBJ_H__

#include "nouveau_channel.h"

struct nouveau_grobj {
	struct nouveau_channel *channel;
	int grclass;
	uint32_t handle;

	enum {
		NOUVEAU_GROBJ_UNBOUND = 0,
		NOUVEAU_GROBJ_BOUND = 1,
		NOUVEAU_GROBJ_EXPLICIT_BIND = 2,
	} bound;
	int subc;
};

#endif
