#include "intel_xvmc.h"

#define I965_MAX_SURFACES		12
struct i965_xvmc_surface {
	struct intel_xvmc_surface comm;
	dri_bo *bo;
};
