/**************************************************************************

Copyright 2001 VA Linux Systems Inc., Fremont, California.
Copyright © 2002 by David Dawes

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
ATI, VA LINUX SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors: Jeff Hartmann <jhartmann@valinux.com>
 *          David Dawes <dawes@xfree86.org>
 *          Keith Whitwell <keith@tungstengraphics.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Priv.h"

#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "windowstr.h"
#include "shadow.h"

#include "GL/glxtokens.h"

#include "i830.h"
#include "i830_dri.h"

#include "i915_drm.h"

#include "dri2.h"

#ifdef DRI2
#if DRI2INFOREC_VERSION >= 1
#define USE_DRI2_1_1_0
#endif

extern XF86ModuleData dri2ModuleData;
#endif

typedef struct {
    PixmapPtr pPixmap;
    unsigned int attachment;
} I830DRI2BufferPrivateRec, *I830DRI2BufferPrivatePtr;

#ifndef USE_DRI2_1_1_0
static DRI2BufferPtr
I830DRI2CreateBuffers(DrawablePtr pDraw, unsigned int *attachments, int count)
{
    ScreenPtr pScreen = pDraw->pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    DRI2BufferPtr buffers;
    dri_bo *bo;
    int i;
    I830DRI2BufferPrivatePtr privates;
    PixmapPtr pPixmap, pDepthPixmap;

    buffers = xcalloc(count, sizeof *buffers);
    if (buffers == NULL)
	return NULL;
    privates = xcalloc(count, sizeof *privates);
    if (privates == NULL) {
	xfree(buffers);
	return NULL;
    }

    pDepthPixmap = NULL;
    for (i = 0; i < count; i++) {
	if (attachments[i] == DRI2BufferFrontLeft) {
	    if (pDraw->type == DRAWABLE_PIXMAP)
		pPixmap = (PixmapPtr) pDraw;
	    else
		pPixmap = (*pScreen->GetWindowPixmap)((WindowPtr) pDraw);
	    pPixmap->refcnt++;
	} else if (attachments[i] == DRI2BufferStencil && pDepthPixmap) {
	    pPixmap = pDepthPixmap;
	    pPixmap->refcnt++;
	} else {
	    unsigned int hint = 0;

	    switch (attachments[i]) {
	    case DRI2BufferDepth:
		if (SUPPORTS_YTILING(pI830))
		    hint = INTEL_CREATE_PIXMAP_TILING_Y;
		else
		    hint = INTEL_CREATE_PIXMAP_TILING_X;
		break;
	    case DRI2BufferFakeFrontLeft:
	    case DRI2BufferFakeFrontRight:
	    case DRI2BufferBackLeft:
	    case DRI2BufferBackRight:
		    hint = INTEL_CREATE_PIXMAP_TILING_X;
		break;
	    }

	    if (!pI830->tiling ||
		(!IS_I965G(pI830) && !pI830->kernel_exec_fencing))
		hint = 0;

	    pPixmap = (*pScreen->CreatePixmap)(pScreen,
					       pDraw->width,
					       pDraw->height,
					       pDraw->depth,
					       hint);

	}

	if (attachments[i] == DRI2BufferDepth)
	    pDepthPixmap = pPixmap;

	buffers[i].attachment = attachments[i];
	buffers[i].pitch = pPixmap->devKind;
	buffers[i].cpp = pPixmap->drawable.bitsPerPixel / 8;
	buffers[i].driverPrivate = &privates[i];
	buffers[i].flags = 0; /* not tiled */
	privates[i].pPixmap = pPixmap;
	privates[i].attachment = attachments[i];

	bo = i830_get_pixmap_bo (pPixmap);
	if (dri_bo_flink(bo, &buffers[i].name) != 0) {
	    /* failed to name buffer */
	}

    }

    return buffers;
}

#else

static DRI2BufferPtr
I830DRI2CreateBuffer(DrawablePtr pDraw, unsigned int attachment,
		     unsigned int format)
{
    ScreenPtr pScreen = pDraw->pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    DRI2BufferPtr buffers;
    dri_bo *bo;
    I830DRI2BufferPrivatePtr privates;
    PixmapPtr pPixmap;

    buffers = xcalloc(1, sizeof *buffers);
    if (buffers == NULL)
	return NULL;
    privates = xcalloc(1, sizeof *privates);
    if (privates == NULL) {
	xfree(buffers);
	return NULL;
    }

    if (attachment == DRI2BufferFrontLeft) {
	if (pDraw->type == DRAWABLE_PIXMAP)
	    pPixmap = (PixmapPtr) pDraw;
	else
	    pPixmap = (*pScreen->GetWindowPixmap)((WindowPtr) pDraw);
	pPixmap->refcnt++;
    } else {
	unsigned int hint = 0;

	switch (attachment) {
	case DRI2BufferDepth:
	case DRI2BufferDepthStencil:
	    if (SUPPORTS_YTILING(pI830))
		hint = INTEL_CREATE_PIXMAP_TILING_Y;
	    else
		hint = INTEL_CREATE_PIXMAP_TILING_X;
	    break;
	case DRI2BufferFakeFrontLeft:
	case DRI2BufferFakeFrontRight:
	case DRI2BufferBackLeft:
	case DRI2BufferBackRight:
	    hint = INTEL_CREATE_PIXMAP_TILING_X;
	    break;
	}

	if (!pI830->tiling ||
	    (!IS_I965G(pI830) && !pI830->kernel_exec_fencing))
	    hint = 0;

	pPixmap = (*pScreen->CreatePixmap)(pScreen,
					   pDraw->width,
					   pDraw->height,
					   (format != 0)?format:pDraw->depth,
					   hint);

    }


    buffers->attachment = attachment;
    buffers->pitch = pPixmap->devKind;
    buffers->cpp = pPixmap->drawable.bitsPerPixel / 8;
    buffers->driverPrivate = privates;
    buffers->format = format;
    buffers->flags = 0; /* not tiled */
    privates->pPixmap = pPixmap;
    privates->attachment = attachment;

    bo = i830_get_pixmap_bo (pPixmap);
    if (dri_bo_flink(bo, &buffers->name) != 0) {
	/* failed to name buffer */
    }

    return buffers;
}

#endif

#ifndef USE_DRI2_1_1_0

static void
I830DRI2DestroyBuffers(DrawablePtr pDraw, DRI2BufferPtr buffers, int count)
{
    ScreenPtr pScreen = pDraw->pScreen;
    I830DRI2BufferPrivatePtr private;
    int i;

    for (i = 0; i < count; i++)
    {
	private = buffers[i].driverPrivate;
	(*pScreen->DestroyPixmap)(private->pPixmap);
    }

    if (buffers)
    {
	xfree(buffers[0].driverPrivate);
	xfree(buffers);
    }
}

#else

static void
I830DRI2DestroyBuffer(DrawablePtr pDraw, DRI2BufferPtr buffer)
{
    if (buffer) {
	I830DRI2BufferPrivatePtr private = buffer->driverPrivate;
	ScreenPtr pScreen = pDraw->pScreen;

	(*pScreen->DestroyPixmap)(private->pPixmap);

	xfree(private);
	xfree(buffer);
    }
}

#endif

static void
I830DRI2CopyRegion(DrawablePtr pDraw, RegionPtr pRegion,
		   DRI2BufferPtr pDstBuffer, DRI2BufferPtr pSrcBuffer)
{
    I830DRI2BufferPrivatePtr srcPrivate = pSrcBuffer->driverPrivate;
    I830DRI2BufferPrivatePtr dstPrivate = pDstBuffer->driverPrivate;
    ScreenPtr pScreen = pDraw->pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    PixmapPtr pSrcPixmap = (srcPrivate->attachment == DRI2BufferFrontLeft)
	? (PixmapPtr) pDraw : srcPrivate->pPixmap;
    PixmapPtr pDstPixmap = (dstPrivate->attachment == DRI2BufferFrontLeft)
	? (PixmapPtr) pDraw : dstPrivate->pPixmap;
    RegionPtr pCopyClip;
    GCPtr pGC;

    pGC = GetScratchGC(pDraw->depth, pScreen);
    pCopyClip = REGION_CREATE(pScreen, NULL, 0);
    REGION_COPY(pScreen, pCopyClip, pRegion);
    (*pGC->funcs->ChangeClip) (pGC, CT_REGION, pCopyClip, 0);
    ValidateGC(&pDstPixmap->drawable, pGC);
    (*pGC->ops->CopyArea)(&pSrcPixmap->drawable, &pDstPixmap->drawable,
			  pGC, 0, 0, pDraw->width, pDraw->height, 0, 0);
    FreeScratchGC(pGC);

    /* Emit a flush of the rendering cache, or on the 965 and beyond
     * rendering results may not hit the framebuffer until significantly
     * later.
     */
    I830EmitFlush(pScrn);
    pI830->need_mi_flush = FALSE;

    /* We can't rely on getting into the block handler before the DRI
     * client gets to run again so flush now. */
    intel_batch_flush(pScrn, TRUE);
#if ALWAYS_SYNC
    I830Sync(pScrn);
#endif
    drmCommandNone(pI830->drmSubFD, DRM_I915_GEM_THROTTLE);

}

Bool I830DRI2ScreenInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    DRI2InfoRec info;
    char *p, buf[64];
    int i;
    struct stat sbuf;
    dev_t d;
#ifdef USE_DRI2_1_1_0
    int dri2_major = 1;
    int dri2_minor = 0;
#endif

    if (pI830->accel != ACCEL_UXA) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "DRI2 requires UXA\n");
	return FALSE;
    }

#ifdef USE_DRI2_1_1_0
    if (xf86LoaderCheckSymbol("DRI2Version")) {
	DRI2Version(& dri2_major, & dri2_minor);
    }

    if (dri2_minor < 1) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "DRI2 requires DRI2 module version 1.1.0 or later\n");
	return FALSE;
    }
#endif

    sprintf(buf, "pci:%04x:%02x:%02x.%d",
	    pI830->PciInfo->domain,
	    pI830->PciInfo->bus,
	    pI830->PciInfo->dev,
	    pI830->PciInfo->func);

    /* Use the already opened (master) fd from modesetting */
    if (pI830->use_drm_mode) {
	info.fd = pI830->drmSubFD;
    } else {
	info.fd = drmOpen("i915", buf);
	drmSetVersion sv;
	int err;

	/* Check that what we opened was a master or a master-capable FD,
	 * by setting the version of the interface we'll use to talk to it.
	 * (see DRIOpenDRMMaster() in DRI1)
	 */
	sv.drm_di_major = 1;
	sv.drm_di_minor = 1;
	sv.drm_dd_major = -1;
	err = drmSetInterfaceVersion(info.fd, &sv);
	if (err != 0)
	    return FALSE;
    }

    if (info.fd < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Failed to open DRM device\n");
	return FALSE;
    }

    /* The whole drmOpen thing is a fiasco and we need to find a way
     * back to just using open(2).  For now, however, lets just make
     * things worse with even more ad hoc directory walking code to
     * discover the device file name. */

    fstat(info.fd, &sbuf);
    d = sbuf.st_rdev;

    p = pI830->deviceName;
    for (i = 0; i < DRM_MAX_MINOR; i++) {
	sprintf(p, DRM_DEV_NAME, DRM_DIR_NAME, i);
	if (stat(p, &sbuf) == 0 && sbuf.st_rdev == d)
	    break;
    }
    if (i == DRM_MAX_MINOR) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "DRI2: failed to open drm device\n");
	return FALSE;
    }

    info.driverName = IS_I965G(pI830) ? "i965" : "i915";
    info.deviceName = p;

#ifdef USE_DRI2_1_1_0
    info.version = 2;
    info.CreateBuffers = NULL;
    info.DestroyBuffers = NULL;
    info.CreateBuffer = I830DRI2CreateBuffer;
    info.DestroyBuffer = I830DRI2DestroyBuffer;
#else
    info.version = 1;
    info.CreateBuffers = I830DRI2CreateBuffers;
    info.DestroyBuffers = I830DRI2DestroyBuffers;
#endif

    info.CopyRegion = I830DRI2CopyRegion;

    pI830->drmSubFD = info.fd;

    return DRI2ScreenInit(pScreen, &info);
}

void I830DRI2CloseScreen(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    DRI2CloseScreen(pScreen);
    pI830->directRenderingType = DRI_NONE;
}
