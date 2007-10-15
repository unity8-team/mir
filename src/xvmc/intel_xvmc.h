
#ifndef INTEL_XVMC_H
#define INTEL_XVMC_H

#include "xf86drm.h"
#include "i830_common.h"
#include "i830_hwmc.h"
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <signal.h>

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
				   int *priv_count, uint **priv_data);

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

#endif
