/*
 * Copyright © 2006 Intel Corporation
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
 *    Xiang Haihao <haihao.xiang@intel.com>
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "i830.h"
#include "i830_video.h"
#include "xf86xv.h"
#include "xf86xvmc.h"
#include <X11/extensions/Xv.h>
#include <X11/extensions/XvMC.h>
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

#if defined(X_NEED_XVPRIV_H) || defined (_XF86_FOURCC_H_)
#include "xf86xvpriv.h"
#endif

#define _INTEL_XVMC_SERVER_
#include "i915_hwmc.h"

#define I915_XVMC_MAX_BUFFERS 2
#define I915_XVMC_MAX_CONTEXTS 4
#define I915_XVMC_MAX_SURFACES 20

typedef struct _I915XvMC {
	PutImageFuncPtr savePutImage;
} I915XvMC, *I915XvMCPtr;

/*
static int yv12_subpicture_index_list[2] =
{
    FOURCC_IA44,
    FOURCC_AI44
};

static XF86MCImageIDList yv12_subpicture_list =
{
    ARRARY_SIZE(yv12_subpicture_index_list),
    yv12_subpicture_index_list
};
 */

static XF86MCSurfaceInfoRec i915_YV12_mpg2_surface = {
	SURFACE_TYPE_MPEG2_MPML,
	XVMC_CHROMA_FORMAT_420,
	0,
	720,
	576,
	720,
	576,
	XVMC_MPEG_2,
	/* XVMC_OVERLAID_SURFACE | XVMC_SUBPICTURE_INDEPENDENT_SCALING, */
	0,
	/* &yv12_subpicture_list */
	NULL,
};

static XF86MCSurfaceInfoRec i915_YV12_mpg1_surface = {
	SURFACE_TYPE_MPEG1_MPML,
	XVMC_CHROMA_FORMAT_420,
	0,
	720,
	576,
	720,
	576,
	XVMC_MPEG_1,
	/* XVMC_OVERLAID_SURFACE | XVMC_SUBPICTURE_INDEPENDENT_SCALING, */
	0,
	/* &yv12_subpicture_list */
	NULL,
};

static XF86MCSurfaceInfoPtr ppSI[2] = {
	(XF86MCSurfaceInfoPtr) & i915_YV12_mpg2_surface,
	(XF86MCSurfaceInfoPtr) & i915_YV12_mpg1_surface
};

/* Check context size not exceed surface type max */
static void i915_check_context_size(XvMCContextPtr ctx)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ppSI); i++) {
		if (ctx->surface_type_id == ppSI[i]->surface_type_id) {
			if (ctx->width > ppSI[i]->max_width)
				ctx->width = ppSI[i]->max_width;
			if (ctx->height > ppSI[i]->max_height)
				ctx->height = ppSI[i]->max_height;
		}
	}
}

/*
 *  i915_xvmc_create_context
 *
 *  Some info about the private data:
 *
 *  Set *num_priv to the number of 32bit words that make up the size of
 *  of the data that priv will point to.
 *
 *  *priv = (long *) xcalloc (elements, sizeof(element))
 *  *num_priv = (elements * sizeof(element)) >> 2;
 *
 **************************************************************************/

static int i915_xvmc_create_context(ScrnInfoPtr scrn, XvMCContextPtr pContext,
				    int *num_priv, long **priv)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	I915XvMCCreateContextRec *contextRec = NULL;

	*priv = NULL;
	*num_priv = 0;

	if (!intel->XvMCEnabled) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "[XvMC] i915: XvMC disabled!\n");
		return BadAlloc;
	}

	i915_check_context_size(pContext);

	*priv = xcalloc(1, sizeof(I915XvMCCreateContextRec));
	contextRec = (I915XvMCCreateContextRec *) * priv;

	if (!*priv) {
		*num_priv = 0;
		return BadAlloc;
	}

	*num_priv = sizeof(I915XvMCCreateContextRec) >> 2;

	contextRec->comm.type = xvmc_driver->flag;
	/* i915 private context */
	contextRec->deviceID = DEVICE_ID(intel->PciInfo);

	return Success;
}

static int i915_xvmc_create_surface(ScrnInfoPtr scrn, XvMCSurfacePtr pSurf,
				    int *num_priv, long **priv)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (!intel->XvMCEnabled) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "[XvMC] i915: XvMC disabled!\n");
		return BadAlloc;
	}

	*priv = NULL;
	*num_priv = 0;

	return Success;
}

static int i915_xvmc_create_subpict(ScrnInfoPtr scrn, XvMCSubpicturePtr pSubp,
				    int *num_priv, long **priv)
{
	*priv = NULL;
	*num_priv = 0;

	return Success;
}

static void i915_xvmc_destroy_context(ScrnInfoPtr scrn,
				      XvMCContextPtr pContext)
{
	return;
}

static void i915_xvmc_destroy_surface(ScrnInfoPtr scrn, XvMCSurfacePtr pSurf)
{
	return;
}

static void i915_xvmc_destroy_subpict(ScrnInfoPtr scrn,
				      XvMCSubpicturePtr pSubp)
{
	return;
}

static int i915_xvmc_put_image(ScrnInfoPtr scrn,
			       short src_x, short src_y,
			       short drw_x, short drw_y, short src_w,
			       short src_h, short drw_w, short drw_h,
			       int id, unsigned char *buf, short width,
			       short height, Bool sync, RegionPtr clipBoxes,
			       pointer data, DrawablePtr pDraw)
{
	I915XvMCPtr pXvMC = (I915XvMCPtr) xvmc_driver->devPrivate;
	struct intel_xvmc_command *xvmc_cmd = (struct intel_xvmc_command *)buf;
	int ret;

	if (FOURCC_XVMC == id) {
		/* Pass the GEM object name through the pointer arg. */
		buf = (void *)(uintptr_t)xvmc_cmd->handle;
	}

	ret =
	    pXvMC->savePutImage(scrn, src_x, src_y, drw_x, drw_y, src_w, src_h,
				drw_w, drw_h, id, buf, width, height, sync,
				clipBoxes, data, pDraw);
	return ret;
}

static Bool i915_xvmc_init(ScrnInfoPtr scrn, XF86VideoAdaptorPtr XvAdapt)
{
	I915XvMCPtr pXvMC;

	pXvMC = (I915XvMCPtr) xcalloc(1, sizeof(I915XvMC));
	if (!pXvMC) {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "[XvMC] alloc driver private failed!\n");
		return FALSE;
	}
	xvmc_driver->devPrivate = (void *)pXvMC;
	/* set up wrappers */
	pXvMC->savePutImage = XvAdapt->PutImage;
	XvAdapt->PutImage = i915_xvmc_put_image;
	return TRUE;
}

static void i915_xvmc_fini(ScrnInfoPtr scrn)
{
	xfree(xvmc_driver->devPrivate);
}

/* Fill in the device dependent adaptor record.
 * This is named "Intel(R) Textured Video" because this code falls under the
 * XV extenstion, the name must match or it won't be used.
 *
 * Surface and Subpicture - see above
 * Function pointers to functions below
 */
static XF86MCAdaptorRec pAdapt = {
	.name = "Intel(R) Textured Video",
	.num_surfaces = ARRAY_SIZE(ppSI),
	.surfaces = ppSI,
	.num_subpictures = 0,
	.subpictures = NULL,
	.CreateContext =
	    (xf86XvMCCreateContextProcPtr) i915_xvmc_create_context,
	.DestroyContext =
	    (xf86XvMCDestroyContextProcPtr) i915_xvmc_destroy_context,
	.CreateSurface =
	    (xf86XvMCCreateSurfaceProcPtr) i915_xvmc_create_surface,
	.DestroySurface =
	    (xf86XvMCDestroySurfaceProcPtr) i915_xvmc_destroy_surface,
	.CreateSubpicture =
	    (xf86XvMCCreateSubpictureProcPtr) i915_xvmc_create_subpict,
	.DestroySubpicture =
	    (xf86XvMCDestroySubpictureProcPtr) i915_xvmc_destroy_subpict,
};

/* new xvmc driver interface */
struct intel_xvmc_driver i915_xvmc_driver = {
	.name = "i915_xvmc",
	.adaptor = &pAdapt,
	.flag = XVMC_I915_MPEG2_MC,
	.init = i915_xvmc_init,
	.fini = i915_xvmc_fini,
};
