/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i830_dri.h,v 1.6 2003/09/28 20:15:59 alanh Exp $ */

#ifndef _I830_DRI_H
#define _I830_DRI_H

#include "xf86dri.h"
#include "xf86drm.h"
#include "i830_common.h"

#define I830_MAX_DRAWABLES 256

#define I830_MAJOR_VERSION 1
#define I830_MINOR_VERSION 5
#define I830_PATCHLEVEL 1

#define I830_REG_SIZE 0x80000

typedef struct _I830DRIRec {
   drm_handle_t regs;
   drmSize regsSize;

   int deviceID;
   int width;
   int height;
   int mem;
   int cpp;
   int bitsPerPixel;

   int fbOffset;
   int fbStride;

   int irq;
   int sarea_priv_offset;
} I830DRIRec, *I830DRIPtr;

typedef struct {
   /* Nothing here yet */
   int dummy;
} I830ConfigPrivRec, *I830ConfigPrivPtr;

typedef struct {
   /* Nothing here yet */
   int dummy;
} I830DRIContextRec, *I830DRIContextPtr;


#endif
