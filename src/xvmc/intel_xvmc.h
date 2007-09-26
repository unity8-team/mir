
#ifndef INTEL_XVMC_H
#define INTEL_XVMC_H

#define DEBUG 0

#define XVMC_ERR(s, arg...)					\
    do {							\
	fprintf(stderr, "intel_xvmc err: " s "\n", ##arg);	\
    } while (0)

#define XVMC_INFO(s, arg...)					\
    do {							\
	fprintf(stderr, "intel_xvmc info: " s "\n", ##arg);	\
    } while (0)

#define XVMC_DBG(s, arg...)						\
    do {								\
	if (DEBUG)							\
	    fprintf(stderr, "intel_xvmc debug: " s "\n", ##arg);	\
    } while (0)

#endif
