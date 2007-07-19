/***************************************************************************

Copyright 2001 Intel Corporation.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL INTEL, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#ifndef _I915XVMC_H
#define _I915XVMC_H

/* #define XVMC_DEBUG(x) do {x; }while(0); */
#define XVMC_DEBUG(x)

#include "xf86drm.h"
#include "i830_common.h"
#include "i915_hwmc.h"
#include <X11/Xlibint.h>
#include <X11/Xutil.h>

#define I915_SUBPIC_PALETTE_SIZE        16

/***************************************************************************
// i915XvMCDrmMap: Holds the data about the DRM maps
***************************************************************************/
typedef struct _i915XvMCDrmMap {
    drm_handle_t handle;
    unsigned offset;
    unsigned size;
    drmAddress map;
} i915XvMCDrmMap, *i915XvMCDrmMapPtr;

/***************************************************************************
// i915XvMCContext: Private Context data referenced via the privData
//  pointer in the XvMCContext structure.
***************************************************************************/
typedef struct _i915XvMCContext {
    unsigned ctxno;
    int fd;   /* File descriptor for /dev/dri */
    unsigned last_render;
    unsigned last_flip;
    unsigned dual_prime; /* Flag to identify when dual prime is in use. */
    unsigned yStride;
    unsigned uvStride;
    unsigned short ref;
    pthread_mutex_t ctxmutex;
    char busIdString[21]; /* PCI:0:1:0 or PCI:0:2:0 */
    int lock;   /* Lightweight lock to avoid locking twice */
    int locked;
    volatile drmI830Sarea *sarea;

    drmLock *driHwLock;
    drm_context_t hHWContext; /* drmcontext; */
    drm_handle_t hsarea;                /* Handle to drm shared memory area */
    drmAddress sarea_address;	        /* Virtual address of shared memory area */
    unsigned sarea_size;                /* Size of drm shared memory area */
    unsigned sarea_priv_offset;	        /* Offset in sarea to private part */
    unsigned screen;
    unsigned depth;
    XvPortID port;		       /* Xv Port ID when displaying */
    I915XvMCAttrHolder attrib;          /* This contexts attributes and their values */
    XvAttribute attribDesc[I915_NUM_XVMC_ATTRIBUTES];    /* Attribute decriptions */
    int haveXv;                        /* Have I initialized the Xv
                                        * connection for this surface? */
    XvImage *xvImage;                  /* Fake Xv Image used for command
                                        * buffer transport to the X server */
    int attribChanged;                 /* Attributes have changed and need to
                                        * be uploaded to Xv at next frame
                                        * display */
    GC  gc;                            /* X GC needed for displaying */
    Drawable draw;                     /* Drawable to undisplay from */
    XID id;
    XVisualInfo visualInfo;
    void *drawHash;

    i915XvMCDrmMap sis;
    i915XvMCDrmMap msb;
    i915XvMCDrmMap ssb;
    i915XvMCDrmMap psp;
    i915XvMCDrmMap psc;
    i915XvMCDrmMap corrdata;

    struct {
        unsigned start_offset;
        unsigned size;
        unsigned space;
        unsigned char *ptr;
    } batch;

    struct 
    {
        void *ptr;
        unsigned size;
        unsigned offset;
        unsigned active_buf;
        unsigned irq_emitted;
    } alloc;
} i915XvMCContext;

/***************************************************************************
// i915XvMCSubpicture: Private data structure for each XvMCSubpicture. This
//  structure is referenced by the privData pointer in the XvMCSubpicture
//  structure.
***************************************************************************/
typedef struct _i915XvMCSubpicture {
    unsigned srfNo;
    unsigned last_render;
    unsigned last_flip;
    unsigned pitch;
    unsigned char palette[3][16];
    i915XvMCDrmMap srf;
    i915XvMCContext *privContext;
} i915XvMCSubpicture;

/***************************************************************************
// i915XvMCSurface: Private data structure for each XvMCSurface. This
//  structure is referenced by the privData pointer in the XvMCSurface
//  structure.
***************************************************************************/
#define I830_MAX_BUFS 2                   /*Number of YUV buffers per surface */
typedef struct _i915XvMCSurface {
    unsigned srfNo;                    /* XvMC private surface numbers */
    unsigned last_render;
    unsigned last_flip;
    unsigned yStride;                  /* Stride of YUV420 Y component. */
    unsigned uvStride;
    unsigned width;                    /* Dimensions */
    unsigned height;
    i915XvMCDrmMap srf;
    i915XvMCContext *privContext;
    i915XvMCSubpicture *privSubPic;     /* Subpicture to be blended when
                                         * displaying. NULL if none. */
} i915XvMCSurface;

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

#endif /* _I915XVMC_H */
