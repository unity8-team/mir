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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _INTEL_XVMC_SERVER_
#include "i830.h"
#include "intel_bufmgr.h"
#include "i830_hwmc.h"
#include "i915_hwmc.h"
#include "i965_hwmc.h"

#include <X11/extensions/Xv.h>
#include <X11/extensions/XvMC.h>
#include <fourcc.h>

struct intel_xvmc_driver *xvmc_driver;

/* set global current driver for xvmc */
static Bool intel_xvmc_set_driver(struct intel_xvmc_driver *d)
{
	if (xvmc_driver) {
		ErrorF("XvMC driver already set!\n");
		return FALSE;
	} else
		xvmc_driver = d;
	return TRUE;
}

/* check chip type and load xvmc driver */
/* This must be first called! */
Bool intel_xvmc_probe(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	Bool ret = FALSE;

	if (!intel->XvMCEnabled)
		return FALSE;

	/* Needs KMS support. */
	if (IS_I915G(intel) || IS_I915GM(intel))
		return FALSE;

	if (IS_I9XX(intel)) {
		if (IS_I915(intel))
			ret = intel_xvmc_set_driver(&i915_xvmc_driver);
		else if (IS_G4X(intel) || IS_IGDNG(intel))
			ret = intel_xvmc_set_driver(&vld_xvmc_driver);
		else
			ret = intel_xvmc_set_driver(&i965_xvmc_driver);
	} else {
		ErrorF("Your chipset doesn't support XvMC.\n");
		return FALSE;
	}
	return TRUE;
}

void intel_xvmc_finish(ScrnInfoPtr scrn)
{
	if (!xvmc_driver)
		return;
	(*xvmc_driver->fini) (scrn);
}

Bool intel_xvmc_driver_init(ScreenPtr pScreen, XF86VideoAdaptorPtr xv_adaptor)
{
	ScrnInfoPtr scrn = xf86Screens[pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct drm_i915_setparam sp;
	int ret;

	if (!xvmc_driver) {
		ErrorF("Failed to probe XvMC driver.\n");
		return FALSE;
	}

	if (!(*xvmc_driver->init) (scrn, xv_adaptor)) {
		ErrorF("XvMC driver initialize failed.\n");
		return FALSE;
	}

	return TRUE;
}

Bool intel_xvmc_screen_init(ScreenPtr pScreen)
{
	ScrnInfoPtr scrn = xf86Screens[pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	char buf[64];

	if (!xvmc_driver)
		return FALSE;

	if (xf86XvMCScreenInit(pScreen, 1, &xvmc_driver->adaptor)) {
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "[XvMC] %s driver initialized.\n",
			   xvmc_driver->name);
	} else {
		intel_xvmc_finish(scrn);
		intel->XvMCEnabled = FALSE;
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "[XvMC] Failed to initialize XvMC.\n");
		return FALSE;
	}

	sprintf(buf, "pci:%04x:%02x:%02x.%d",
		intel->PciInfo->domain,
		intel->PciInfo->bus, intel->PciInfo->dev, intel->PciInfo->func);

	xf86XvMCRegisterDRInfo(pScreen, INTEL_XVMC_LIBNAME,
			       buf,
			       INTEL_XVMC_MAJOR, INTEL_XVMC_MINOR,
			       INTEL_XVMC_PATCHLEVEL);
	return TRUE;
}

/* i915 hwmc support */
#define _INTEL_XVMC_SERVER_

#define I915_XVMC_MAX_BUFFERS 2
#define I915_XVMC_MAX_CONTEXTS 4
#define I915_XVMC_MAX_SURFACES 20

typedef struct _I915XvMC {
	PutImageFuncPtr savePutImage;
} I915XvMC, *I915XvMCPtr;

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

/* i965 and later hwmc support */
#define STRIDE(w)               (w)
#define SIZE_YUV420(w, h)       (h * (STRIDE(w) + STRIDE(w >> 1)))
#define VLD_MAX_SLICE_LEN	(32*1024)

#ifndef XVMC_VLD
#define XVMC_VLD  0x00020000
#endif

static PutImageFuncPtr savedXvPutImage;

static int create_context(ScrnInfoPtr scrn,
			  XvMCContextPtr context, int *num_privates,
			  CARD32 ** private)
{
	struct i965_xvmc_context *private_context, *context_dup;
	intel_screen_private *intel = intel_get_screen_private(scrn);

	unsigned int blocknum =
	    (((context->width + 15) / 16) * ((context->height + 15) / 16));
	unsigned int blocksize = 6 * blocknum * 64 * sizeof(short);
	blocksize = (blocksize + 4095) & (~4095);
	if ((private_context = Xcalloc(sizeof(*private_context))) == NULL) {
		ErrorF("XVMC Can not allocate private context\n");
		return BadAlloc;
	}

	if ((context_dup = Xcalloc(sizeof(*private_context))) == NULL) {
		ErrorF("XVMC Can not allocate private context\n");
		return BadAlloc;
	}

	private_context->is_g4x = IS_G4X(intel);
	private_context->is_965_q = IS_965_Q(intel);
	private_context->is_igdng = IS_IGDNG(intel);
	private_context->comm.type = xvmc_driver->flag;

	*num_privates = sizeof(*private_context) / sizeof(CARD32);
	*private = (CARD32 *) private_context;
	memcpy(context_dup, private_context, sizeof(*private_context));
	context->driver_priv = context_dup;

	return Success;
}

static void destroy_context(ScrnInfoPtr scrn, XvMCContextPtr context)
{
	struct i965_xvmc_context *private_context;
	private_context = context->driver_priv;
	Xfree(private_context);
}

static int create_surface(ScrnInfoPtr scrn, XvMCSurfacePtr surface,
			  int *num_priv, CARD32 ** priv)
{
	XvMCContextPtr ctx = surface->context;

	struct i965_xvmc_surface *priv_surface, *surface_dup;
	struct i965_xvmc_context *priv_ctx = ctx->driver_priv;
	int i;
	for (i = 0; i < I965_MAX_SURFACES; i++) {
		if (priv_ctx->surfaces[i] == NULL) {
			priv_surface = Xcalloc(sizeof(*priv_surface));
			if (priv_surface == NULL)
				return BadAlloc;
			surface_dup = Xcalloc(sizeof(*priv_surface));
			if (surface_dup == NULL)
				return BadAlloc;

			priv_surface->no = i;
			priv_surface->handle = priv_surface;
			priv_surface->w = ctx->width;
			priv_surface->h = ctx->height;
			priv_ctx->surfaces[i] = surface->driver_priv
			    = priv_surface;
			memcpy(surface_dup, priv_surface,
			       sizeof(*priv_surface));
			*num_priv = sizeof(*priv_surface) / sizeof(CARD32);
			*priv = (CARD32 *) surface_dup;
			break;
		}
	}

	if (i >= I965_MAX_SURFACES) {
		ErrorF("I965 XVMC too many surfaces in one context\n");
		return BadAlloc;
	}

	return Success;
}

static void destory_surface(ScrnInfoPtr scrn, XvMCSurfacePtr surface)
{
	XvMCContextPtr ctx = surface->context;
	struct i965_xvmc_surface *priv_surface = surface->driver_priv;
	struct i965_xvmc_context *priv_ctx = ctx->driver_priv;
	priv_ctx->surfaces[priv_surface->no] = NULL;
	Xfree(priv_surface);
}

static int create_subpicture(ScrnInfoPtr scrn, XvMCSubpicturePtr subpicture,
			     int *num_priv, CARD32 ** priv)
{
	return Success;
}

static void destroy_subpicture(ScrnInfoPtr scrn, XvMCSubpicturePtr subpicture)
{
}

static int put_image(ScrnInfoPtr scrn,
		     short src_x, short src_y,
		     short drw_x, short drw_y, short src_w,
		     short src_h, short drw_w, short drw_h,
		     int id, unsigned char *buf, short width,
		     short height, Bool sync, RegionPtr clipBoxes, pointer data,
		     DrawablePtr drawable)
{
	struct intel_xvmc_command *cmd = (struct intel_xvmc_command *)buf;

	if (id == FOURCC_XVMC) {
		/* Pass the GEM object name through the pointer arg. */
		buf = (void *)(uintptr_t)cmd->handle;
	}

	savedXvPutImage(scrn, src_x, src_y, drw_x, drw_y, src_w, src_h,
			drw_w, drw_h, id, buf,
			width, height, sync, clipBoxes,
			data, drawable);

	return Success;
}

static Bool init(ScrnInfoPtr screen_info, XF86VideoAdaptorPtr adaptor)
{
	savedXvPutImage = adaptor->PutImage;
	adaptor->PutImage = put_image;

	return TRUE;
}

static void fini(ScrnInfoPtr screen_info)
{
}

static XF86MCSurfaceInfoRec yv12_mpeg2_vld_surface = {
	FOURCC_YV12,
	XVMC_CHROMA_FORMAT_420,
	0,
	1936,
	1096,
	1920,
	1080,
	XVMC_MPEG_2 | XVMC_VLD,
	XVMC_INTRA_UNSIGNED,
	NULL
};

static XF86MCSurfaceInfoRec yv12_mpeg2_surface = {
	FOURCC_YV12,
	XVMC_CHROMA_FORMAT_420,
	0,
	1936,
	1096,
	1920,
	1080,
	XVMC_MPEG_2 | XVMC_MOCOMP,
	/* XVMC_OVERLAID_SURFACE | XVMC_SUBPICTURE_INDEPENDENT_SCALING, */
	XVMC_INTRA_UNSIGNED,
	/* &yv12_subpicture_list */
	NULL
};

static XF86MCSurfaceInfoRec yv12_mpeg1_surface = {
	FOURCC_YV12,
	XVMC_CHROMA_FORMAT_420,
	0,
	1920,
	1080,
	1920,
	1080,
	XVMC_MPEG_1 | XVMC_MOCOMP,
	/*XVMC_OVERLAID_SURFACE | XVMC_SUBPICTURE_INDEPENDENT_SCALING |
	   XVMC_INTRA_UNSIGNED, */
	XVMC_INTRA_UNSIGNED,

	/*&yv12_subpicture_list */
	NULL
};

static XF86MCSurfaceInfoPtr surface_info[] = {
	&yv12_mpeg2_surface,
	&yv12_mpeg1_surface
};

static XF86MCSurfaceInfoPtr surface_info_vld[] = {
	&yv12_mpeg2_vld_surface,
	&yv12_mpeg2_surface,
};

static XF86MCAdaptorRec adaptor_vld = {
	.name = "Intel(R) Textured Video",
	.num_surfaces = sizeof(surface_info_vld) / sizeof(surface_info_vld[0]),
	.surfaces = surface_info_vld,

	.CreateContext = create_context,
	.DestroyContext = destroy_context,
	.CreateSurface = create_surface,
	.DestroySurface = destory_surface,
	.CreateSubpicture = create_subpicture,
	.DestroySubpicture = destroy_subpicture
};

static XF86MCAdaptorRec adaptor = {
	.name = "Intel(R) Textured Video",
	.num_surfaces = sizeof(surface_info) / sizeof(surface_info[0]),
	.surfaces = surface_info,

	.CreateContext = create_context,
	.DestroyContext = destroy_context,
	.CreateSurface = create_surface,
	.DestroySurface = destory_surface,
	.CreateSubpicture = create_subpicture,
	.DestroySubpicture = destroy_subpicture
};

struct intel_xvmc_driver i965_xvmc_driver = {
	.name = "i965_xvmc",
	.adaptor = &adaptor,
	.flag = XVMC_I965_MPEG2_MC,
	.init = init,
	.fini = fini
};

struct intel_xvmc_driver vld_xvmc_driver = {
	.name = "xvmc_vld",
	.adaptor = &adaptor_vld,
	.flag = XVMC_I965_MPEG2_VLD,
	.init = init,
	.fini = fini
};
