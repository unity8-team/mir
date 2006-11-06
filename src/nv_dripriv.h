#ifndef NV_DRIPRIV_H_
#define NV_DRIPRIV_H_

#include "GL/glxint.h"
#include "xf86drm.h"

extern void GlxSetVisualConfigs(int nconfigs, __GLXvisualConfig *configs,
				void **configprivs);

typedef struct {
    /* Nothing here yet */
    int dummy;
} NVConfigPrivRec, *NVConfigPrivPtr;

typedef struct {
    /* Nothing here yet */
    int dummy;
} NVDRIContextRec, *NVDRIContextPtr;

#endif

