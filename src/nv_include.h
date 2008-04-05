/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_include.h,v 1.9 2000/10/06 12:31:03 eich Exp $ */

#ifndef __NV_INCLUDE_H__
#define __NV_INCLUDE_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

/* All drivers should typically include these */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#define PPC_MMIO_IS_BE
#include "compiler.h"

/* Drivers for PCI hardware need this */
#include "xf86PciInfo.h"

/* Drivers that need to access the PCI config space directly need this */
#include "xf86Pci.h"

/* All drivers initialising the SW cursor need this */
#include "mipointer.h"

/* All drivers implementing backing store need this */
#include "mibstore.h"

#include "micmap.h"

#include "xf86DDC.h"

#include "vbe.h"

#include "xf86RAC.h"

#include "xf86RandR12.h"

#include "nv_const.h"

#include "dixstruct.h"
#include "scrnintstr.h"

#include "fb.h"

#include "xf86cmap.h"
#include "shadowfb.h"

#include "xf86xv.h"
#include <X11/extensions/Xv.h>

#include "vgaHW.h"

#include "xf86Cursor.h"
#include "xf86DDC.h"

#include "region.h"

#include <X11/extensions/randr.h>

#define NV_DMA_DEBUG 0

#include "nv_local.h"
#include "nv_type.h"
#include "nv_proto.h"
#include "nv_dma.h"
#include "nouveau_drm.h"
#include "nouveau_class.h"
#include "nvreg.h"
#include "nv50reg.h"

#include "nouveau_device.h"
#include "nouveau_drmif.h"
#include "nouveau_dma.h"

#endif /* __NV_INCLUDE_H__ */
