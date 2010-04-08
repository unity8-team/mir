#define I965_MC_STATIC_BUFFER_SIZE	(1024*512)
struct i965_xvmc_context {
	struct _intel_xvmc_common comm;
	unsigned int is_g4x:1;
	unsigned int is_965_q:1;
	unsigned int is_igdng:1;
};
