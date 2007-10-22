
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
#if 0
    drm_handle_t hsarea;	/* DRI open connect */
    unsigned int sarea_size;
    drmAddress sarea_address;
#endif
    char busID[32];
    int fd;			/* drm file handler */
    void *private;
    /* XXX: api hooks */
    int (*init)(/*XXX*/);
    void (*fini)(/*XXX*/);
    int (*create_context)(Display* display, XvMCContext *context, int priv_count, CARD32 *priv_data);
    int (*destroy_context)(Display* display, XvMCContext *context);
    int (*create_surface)(Display* display, XvMCContext *context, XvMCSurface *surface);
    int (*destroy_surface)(Display* display, XvMCSurface *surface);
    int (*render_surface)(Display *display, XvMCContext *context,
	    unsigned int picture_structure,
	    XvMCSurface *target_surface,
	    XvMCSurface *past_surface,
	    XvMCSurface *future_surface,
	    unsigned int flags,
	    unsigned int num_macroblocks,
	    unsigned int first_macroblock,
	    XvMCMacroBlockArray *macroblock_array,
	    XvMCBlockArray *blocks);
    int (*put_surface)(Display *display,XvMCSurface *surface,
	    Drawable draw, short srcx, short srcy,
	    unsigned short srcw, unsigned short srch,
	    short destx, short desty,
	    unsigned short destw, unsigned short desth,
	    int flags);
    int (*get_surface_status)(Display *display, XvMCSurface *surface, int *stat);
};

extern struct _intel_xvmc_driver i915_xvmc_mc_driver;

#endif
