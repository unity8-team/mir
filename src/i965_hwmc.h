#define I965_MC_STATIC_BUFFER_SIZE	(1024*512)
#define I965_MAX_SURFACES		12
struct i965_xvmc_surface {
	int w, h;
	unsigned int no;
	void *handle;
	dri_bo *bo;
};

struct i965_xvmc_context {
	struct _intel_xvmc_common comm;
	struct i965_xvmc_surface *surfaces[I965_MAX_SURFACES];
	unsigned int is_g4x:1;
	unsigned int is_965_q:1;
	unsigned int is_igdng:1;
};
