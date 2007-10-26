/*
 * Copyright © 2007 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Zhenyu Wang <zhenyu.z.wang@intel.com>
 *
 */
#ifndef INTEL_XVMC_H
#define INTEL_XVMC_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

#include <xf86drm.h>
#include "i830_common.h"
#include "i830_hwmc.h"
#include <X11/X.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <fourcc.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <X11/extensions/XvMC.h>
#include <X11/extensions/XvMClib.h>
#include <drm_sarea.h>

#include "xf86dri.h"
#include "driDrawable.h"

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

/* Subpicture fourcc */
#define FOURCC_IA44 0x34344149

/*
  Definitions for temporary wire protocol hooks to be replaced
  when a HW independent libXvMC is created.
*/
extern Status _xvmc_create_context(Display *dpy, XvMCContext *context,
				   int *priv_count, CARD32 **priv_data);

extern Status _xvmc_destroy_context(Display *dpy, XvMCContext *context);

extern Status _xvmc_create_surface(Display *dpy, XvMCContext *context,
				   XvMCSurface *surface, int *priv_count,
				   uint **priv_data);

extern Status _xvmc_destroy_surface(Display *dpy, XvMCSurface *surface);

extern Status  _xvmc_create_subpicture(Display *dpy, XvMCContext *context,
				       XvMCSubpicture *subpicture,
				       int *priv_count, uint **priv_data);

extern Status   _xvmc_destroy_subpicture(Display *dpy,
					 XvMCSubpicture *subpicture);


struct _intel_xvmc_driver {
    int type;			/* hw xvmc type - i830_hwmc.h */
    int screen;			/* current screen num*/

    int fd;			/* drm file handler */
    drm_handle_t hsarea;	/* DRI open connect */
    char busID[32];

    unsigned int sarea_size;
    drmAddress sarea_address;

    void *private;

    /* XXX: remove? */
    int (*init)(void);
    void (*fini)(void);

    /* driver specific xvmc callbacks */
    Status (*create_context)(Display* display, XvMCContext *context,
	    int priv_count, CARD32 *priv_data);

    Status (*destroy_context)(Display* display, XvMCContext *context);

    Status (*create_surface)(Display* display, XvMCContext *context,
	    XvMCSurface *surface);

    Status (*destroy_surface)(Display* display, XvMCSurface *surface);

    Status (*render_surface)(Display *display, XvMCContext *context,
	    unsigned int picture_structure,
	    XvMCSurface *target_surface,
	    XvMCSurface *past_surface,
	    XvMCSurface *future_surface,
	    unsigned int flags,
	    unsigned int num_macroblocks,
	    unsigned int first_macroblock,
	    XvMCMacroBlockArray *macroblock_array,
	    XvMCBlockArray *blocks);

    /* XXX this should be same for all drivers */
    Status (*put_surface)(Display *display, XvMCSurface *surface,
	    Drawable draw, short srcx, short srcy,
	    unsigned short srcw, unsigned short srch,
	    short destx, short desty,
	    unsigned short destw, unsigned short desth,
	    int flags);

    Status (*get_surface_status)(Display *display, XvMCSurface *surface, int *stat);

    /* XXX more for vld */
};

extern struct _intel_xvmc_driver i915_xvmc_mc_driver;
extern struct _intel_xvmc_driver *xvmc_driver;

#endif
