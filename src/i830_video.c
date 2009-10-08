/***************************************************************************

 Copyright 2000 Intel Corporation.  All Rights Reserved. 

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

/*
 * i830_video.c: i830/i845 Xv driver. 
 *
 * Copyright © 2002 by Alan Hourihane and David Dawes
 *
 * Authors: 
 *	Alan Hourihane <alanh@tungstengraphics.com>
 *	David Dawes <dawes@xfree86.org>
 *
 * Derived from i810 Xv driver:
 *
 * Authors of i810 code:
 * 	Jonathan Bian <jonathan.bian@intel.com>
 *      Offscreen Images:
 *        Matt Sottek <matthew.j.sottek@intel.com>
 */

/*
 * XXX Could support more formats.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"
#include "randrstr.h"
#include "windowstr.h"
#include "damage.h"
#include "i830.h"
#include "i830_video.h"
#include "xf86xv.h"
#include <X11/extensions/Xv.h>
#include "dixstruct.h"
#include "fourcc.h"

#ifdef INTEL_XVMC
#define _INTEL_XVMC_SERVER_
#include "i830_hwmc.h"
#include "i915_hwmc.h"
#endif

#define OFF_DELAY 	250	/* milliseconds */
#define FREE_DELAY 	15000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

static XF86VideoAdaptorPtr I830SetupImageVideoOverlay(ScreenPtr);
static XF86VideoAdaptorPtr I830SetupImageVideoTextured(ScreenPtr);
static void I830StopVideo(ScrnInfoPtr, pointer, Bool);
static int I830SetPortAttributeOverlay(ScrnInfoPtr, Atom, INT32, pointer);
static int I830SetPortAttributeTextured(ScrnInfoPtr, Atom, INT32, pointer);
static int I830GetPortAttribute(ScrnInfoPtr, Atom, INT32 *, pointer);
static void I830QueryBestSize(ScrnInfoPtr, Bool,
			      short, short, short, short, unsigned int *,
			      unsigned int *, pointer);
static int I830PutImage(ScrnInfoPtr, short, short, short, short, short, short,
			short, short, int, unsigned char *, short, short,
			Bool, RegionPtr, pointer, DrawablePtr);
static int I830QueryImageAttributes(ScrnInfoPtr, int, unsigned short *,
				    unsigned short *, int *, int *);

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvBrightness, xvContrast, xvSaturation, xvColorKey, xvPipe;
static Atom xvGamma0, xvGamma1, xvGamma2, xvGamma3, xvGamma4, xvGamma5;
static Atom xvSyncToVblank;

/* Limits for the overlay/textured video source sizes.  The documented hardware
 * limits are 2048x2048 or better for overlay and both of our textured video
 * implementations.  Additionally, on the 830 and 845, larger sizes resulted in
 * the card hanging, so we keep the limits lower there.
 */
#define IMAGE_MAX_WIDTH		2048
#define IMAGE_MAX_HEIGHT	2048
#define IMAGE_MAX_WIDTH_LEGACY	1024
#define IMAGE_MAX_HEIGHT_LEGACY	1088

/* overlay debugging printf function */
#if 0
#define OVERLAY_DEBUG ErrorF
#else
#define OVERLAY_DEBUG if (0) ErrorF
#endif

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[1] = {
	{
	 0,
	 "XV_IMAGE",
	 IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
	 {1, 1}
	 }
};

#define NUM_FORMATS 3

static XF86VideoFormatRec Formats[NUM_FORMATS] = {
	{15, TrueColor}, {16, TrueColor}, {24, TrueColor}
};

#define CLONE_ATTRIBUTES 1
static XF86AttributeRec CloneAttributes[CLONE_ATTRIBUTES] = {
	{XvSettable | XvGettable, -1, 1, "XV_PIPE"}
};

#define NUM_ATTRIBUTES 4
static XF86AttributeRec Attributes[NUM_ATTRIBUTES] = {
	{XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
	{XvSettable | XvGettable, -128, 127, "XV_BRIGHTNESS"},
	{XvSettable | XvGettable, 0, 255, "XV_CONTRAST"},
	{XvSettable | XvGettable, 0, 1023, "XV_SATURATION"}
};

#define NUM_TEXTURED_ATTRIBUTES 3
static XF86AttributeRec TexturedAttributes[NUM_TEXTURED_ATTRIBUTES] = {
	{XvSettable | XvGettable, -128, 127, "XV_BRIGHTNESS"},
	{XvSettable | XvGettable, 0, 255, "XV_CONTRAST"},
	{XvSettable | XvGettable, -1, 1, "XV_SYNC_TO_VBLANK"},
};

#define GAMMA_ATTRIBUTES 6
static XF86AttributeRec GammaAttributes[GAMMA_ATTRIBUTES] = {
	{XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA0"},
	{XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA1"},
	{XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA2"},
	{XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA3"},
	{XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA4"},
	{XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA5"}
};

#define NUM_IMAGES 5

static XF86ImageRec Images[NUM_IMAGES] = {
	XVIMAGE_YUY2,
	XVIMAGE_YV12,
	XVIMAGE_I420,
	XVIMAGE_UYVY,
#ifdef INTEL_XVMC
	{
	 /*
	  * Below, a dummy picture type that is used in XvPutImage only to do
	  * an overlay update. Introduced for the XvMC client lib.
	  * Defined to have a zero data size.
	  */
	 FOURCC_XVMC,
	 XvYUV,
	 LSBFirst,
	 {'X', 'V', 'M', 'C',
	  0x00, 0x00, 0x00, 0x10, 0x80, 0x00, 0x00, 0xAA, 0x00,
	  0x38, 0x9B, 0x71},
	 12,
	 XvPlanar,
	 3,
	 0, 0, 0, 0,
	 8, 8, 8,
	 1, 2, 2,
	 1, 2, 2,
	 {'Y', 'V', 'U',
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	  0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	 XvTopToBottom},
#endif
};

#if VIDEO_DEBUG
static void CompareOverlay(intel_screen_private *intel, uint32_t * overlay, int size)
{
	int i;
	uint32_t val;
	int bad = 0;

	for (i = 0; i < size; i += 4) {
		val = INREG(0x30100 + i);
		if (val != overlay[i / 4]) {
			OVERLAY_DEBUG
			    ("0x%05x value doesn't match (0x%lx != 0x%lx)\n",
			     0x30100 + i, val, overlay[i / 4]);
			bad++;
		}
	}
	if (!bad)
		OVERLAY_DEBUG("CompareOverlay: no differences\n");
}
#endif

/* kernel modesetting overlay functions */
static Bool drmmode_has_overlay(ScrnInfoPtr scrn)
{
#ifdef DRM_MODE_OVERLAY_LANDED
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct drm_i915_getparam gp;
	int has_overlay = 0;

	gp.param = I915_PARAM_HAS_OVERLAY;
	gp.value = &has_overlay;
	drmCommandWriteRead(p830->drmSubFD, DRM_I915_GETPARAM, &gp, sizeof(gp));

	return has_overlay ? TRUE : FALSE;
#else
	return FALSE;
#endif
}

static void drmmode_overlay_update_attrs(ScrnInfoPtr scrn)
{
#ifdef DRM_MODE_OVERLAY_LANDED
	intel_screen_private *intel = intel_get_screen_private(scrn);
	I830PortPrivPtr pPriv = GET_PORT_PRIVATE(scrn);
	struct drm_intel_overlay_attrs attrs;
	int ret;

	attrs.flags = I915_OVERLAY_UPDATE_ATTRS;
	attrs.brightness = pPriv->brightness;
	attrs.contrast = pPriv->contrast;
	attrs.saturation = pPriv->saturation;
	attrs.color_key = pPriv->colorKey;
	attrs.gamma0 = pPriv->gamma0;
	attrs.gamma1 = pPriv->gamma1;
	attrs.gamma2 = pPriv->gamma2;
	attrs.gamma3 = pPriv->gamma3;
	attrs.gamma4 = pPriv->gamma4;
	attrs.gamma5 = pPriv->gamma5;

	ret = drmCommandWriteRead(p830->drmSubFD, DRM_I915_OVERLAY_ATTRS,
				  &attrs, sizeof(attrs));

	if (ret != 0)
		OVERLAY_DEBUG("overlay attrs ioctl failed: %i\n", ret);
#endif
}

static void drmmode_overlay_off(ScrnInfoPtr scrn)
{
#ifdef DRM_MODE_OVERLAY_LANDED
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct drm_intel_overlay_put_image request;
	int ret;

	request.flags = 0;

	ret = drmCommandWrite(p830->drmSubFD, DRM_I915_OVERLAY_PUT_IMAGE,
			      &request, sizeof(request));

	if (ret != 0)
		OVERLAY_DEBUG("overlay switch-off ioctl failed: %i\n", ret);
#endif
}

static Bool
drmmode_overlay_put_image(ScrnInfoPtr scrn, xf86CrtcPtr crtc,
			  int id, short width, short height,
			  int dstPitch, int x1, int y1, int x2, int y2,
			  BoxPtr dstBox, short src_w, short src_h, short drw_w,
			  short drw_h)
{
#ifdef DRM_MODE_OVERLAY_LANDED
	intel_screen_private *intel = intel_get_screen_private(scrn);
	I830PortPrivPtr pPriv = GET_PORT_PRIVATE(scrn);
	struct drm_intel_overlay_put_image request;
	int ret;
	int planar = is_planar_fourcc(id);
	float scale;

	request.flags = I915_OVERLAY_ENABLE;

	request.bo_handle = pPriv->buf->handle;
	if (planar) {
		request.stride_Y = dstPitch * 2;
		request.stride_UV = dstPitch;
	} else {
		request.stride_Y = dstPitch;
		request.stride_UV = 0;
	}
	request.offset_Y = pPriv->YBufOffset;
	request.offset_U = pPriv->UBufOffset;
	request.offset_V = pPriv->VBufOffset;
	OVERLAY_DEBUG("off_Y: %i, off_U: %i, off_V: %i\n", request.offset_Y,
		      request.offset_U, request.offset_V);

	request.crtc_id = drmmode_crtc_id(crtc);
	request.dst_x = dstBox->x1;
	request.dst_y = dstBox->y1;
	request.dst_width = dstBox->x2 - dstBox->x1;
	request.dst_height = dstBox->y2 - dstBox->y1;

	request.src_width = width;
	request.src_height = height;
	/* adjust src dimensions */
	if (request.dst_height > 1) {
		scale = ((float)request.dst_height - 1) / ((float)drw_h - 1);
		request.src_scan_height = src_h * scale;
	} else
		request.src_scan_height = 1;

	if (request.dst_width > 1) {
		scale = ((float)request.dst_width - 1) / ((float)drw_w - 1);
		request.src_scan_width = src_w * scale;
	} else
		request.src_scan_width = 1;

	if (planar) {
		request.flags |= I915_OVERLAY_YUV_PLANAR | I915_OVERLAY_YUV420;
	} else {
		request.flags |= I915_OVERLAY_YUV_PACKED | I915_OVERLAY_YUV422;
		if (id == FOURCC_UYVY)
			request.flags |= I915_OVERLAY_Y_SWAP;
	}

	ret = drmCommandWrite(p830->drmSubFD, DRM_I915_OVERLAY_PUT_IMAGE,
			      &request, sizeof(request));

	/* drop the newly displaying buffer right away */
	drm_intel_bo_disable_reuse(pPriv->buf);
	drm_intel_bo_unreference(pPriv->buf);
	pPriv->buf = NULL;

	if (ret != 0) {
		OVERLAY_DEBUG("overlay put-image ioctl failed: %i\n", ret);
		return FALSE;
	} else
		return TRUE;
#else
	return FALSE;
#endif
}

void I830InitVideo(ScreenPtr pScreen)
{
	ScrnInfoPtr scrn = xf86Screens[pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
	XF86VideoAdaptorPtr overlayAdaptor = NULL, texturedAdaptor = NULL;
	int num_adaptors;
#ifdef INTEL_XVMC
	Bool xvmc_status = FALSE;
#endif

	num_adaptors = xf86XVListGenericAdaptors(scrn, &adaptors);
	/* Give our adaptor list enough space for the overlay and/or texture video
	 * adaptors.
	 */
	newAdaptors =
	    xalloc((num_adaptors + 2) * sizeof(XF86VideoAdaptorPtr *));
	if (newAdaptors == NULL)
		return;

	memcpy(newAdaptors, adaptors,
	       num_adaptors * sizeof(XF86VideoAdaptorPtr));
	adaptors = newAdaptors;

	/* Add the adaptors supported by our hardware.  First, set up the atoms
	 * that will be used by both output adaptors.
	 */
	xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
	xvContrast = MAKE_ATOM("XV_CONTRAST");

	/* Set up textured video if we can do it at this depth and we are on
	 * supported hardware.
	 */
	if (scrn->bitsPerPixel >= 16 && (IS_I9XX(intel) || IS_I965G(intel)) &&
	    !(!IS_I965G(intel) && scrn->displayWidth > 2048)) {
		texturedAdaptor = I830SetupImageVideoTextured(pScreen);
		if (texturedAdaptor != NULL) {
			xf86DrvMsg(scrn->scrnIndex, X_INFO,
				   "Set up textured video\n");
		} else {
			xf86DrvMsg(scrn->scrnIndex, X_ERROR,
				   "Failed to set up textured video\n");
		}
	}

	/* Set up overlay video if we can do it at this depth. */
	if (!OVERLAY_NOEXIST(intel) && scrn->bitsPerPixel != 8) {
		intel->use_drmmode_overlay = drmmode_has_overlay(scrn);
		if (intel->use_drmmode_overlay) {
			overlayAdaptor = I830SetupImageVideoOverlay(pScreen);
			if (overlayAdaptor != NULL) {
				xf86DrvMsg(scrn->scrnIndex, X_INFO,
					   "Set up overlay video\n");
			} else {
				xf86DrvMsg(scrn->scrnIndex, X_ERROR,
					   "Failed to set up overlay video\n");
			}
		}
	}

	if (overlayAdaptor && intel->XvPreferOverlay)
		adaptors[num_adaptors++] = overlayAdaptor;

	if (texturedAdaptor)
		adaptors[num_adaptors++] = texturedAdaptor;

	if (overlayAdaptor && !intel->XvPreferOverlay)
		adaptors[num_adaptors++] = overlayAdaptor;

#ifdef INTEL_XVMC
	if (intel_xvmc_probe(scrn)) {
		if (texturedAdaptor)
			xvmc_status =
			    intel_xvmc_driver_init(pScreen, texturedAdaptor);
	}
#endif

	if (num_adaptors) {
		xf86XVScreenInit(pScreen, adaptors, num_adaptors);
	} else {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "Disabling Xv because no adaptors could be initialized.\n");
		intel->XvEnabled = FALSE;
	}

#ifdef INTEL_XVMC
	if (xvmc_status)
		intel_xvmc_screen_init(pScreen);
#endif
	xfree(adaptors);
}

#define PFIT_CONTROLS 0x61230
#define PFIT_AUTOVSCALE_MASK 0x200
#define PFIT_ON_MASK 0x80000000
#define PFIT_AUTOSCALE_RATIO 0x61238
#define PFIT_PROGRAMMED_SCALE_RATIO 0x61234

static XF86VideoAdaptorPtr I830SetupImageVideoOverlay(ScreenPtr pScreen)
{
	ScrnInfoPtr scrn = xf86Screens[pScreen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	XF86VideoAdaptorPtr adapt;
	I830PortPrivPtr pPriv;
	XF86AttributePtr att;

	OVERLAY_DEBUG("I830SetupImageVideoOverlay\n");

	if (!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			      sizeof(I830PortPrivRec) + sizeof(DevUnion))))
		return NULL;

	adapt->type = XvWindowMask | XvInputMask | XvImageMask;
	adapt->flags = VIDEO_OVERLAID_IMAGES /*| VIDEO_CLIP_TO_VIEWPORT */ ;
	adapt->name = "Intel(R) Video Overlay";
	adapt->nEncodings = 1;
	adapt->pEncodings = DummyEncoding;
	/* update the DummyEncoding for these two chipsets */
	if (IS_845G(intel) || IS_I830(intel)) {
		adapt->pEncodings->width = IMAGE_MAX_WIDTH_LEGACY;
		adapt->pEncodings->height = IMAGE_MAX_HEIGHT_LEGACY;
	}
	adapt->nFormats = NUM_FORMATS;
	adapt->pFormats = Formats;
	adapt->nPorts = 1;
	adapt->pPortPrivates = (DevUnion *) (&adapt[1]);

	pPriv = (I830PortPrivPtr) (&adapt->pPortPrivates[1]);

	adapt->pPortPrivates[0].ptr = (pointer) (pPriv);
	adapt->nAttributes = NUM_ATTRIBUTES;
	adapt->nAttributes += CLONE_ATTRIBUTES;
	if (IS_I9XX(intel))
		adapt->nAttributes += GAMMA_ATTRIBUTES;	/* has gamma */
	adapt->pAttributes =
	    xnfalloc(sizeof(XF86AttributeRec) * adapt->nAttributes);
	/* Now copy the attributes */
	att = adapt->pAttributes;
	memcpy((char *)att, (char *)Attributes,
	       sizeof(XF86AttributeRec) * NUM_ATTRIBUTES);
	att += NUM_ATTRIBUTES;
	memcpy((char *)att, (char *)CloneAttributes,
	       sizeof(XF86AttributeRec) * CLONE_ATTRIBUTES);
	att += CLONE_ATTRIBUTES;
	if (IS_I9XX(intel)) {
		memcpy((char *)att, (char *)GammaAttributes,
		       sizeof(XF86AttributeRec) * GAMMA_ATTRIBUTES);
		att += GAMMA_ATTRIBUTES;
	}
	adapt->nImages = NUM_IMAGES;
	adapt->pImages = Images;
	adapt->PutVideo = NULL;
	adapt->PutStill = NULL;
	adapt->GetVideo = NULL;
	adapt->GetStill = NULL;
	adapt->StopVideo = I830StopVideo;
	adapt->SetPortAttribute = I830SetPortAttributeOverlay;
	adapt->GetPortAttribute = I830GetPortAttribute;
	adapt->QueryBestSize = I830QueryBestSize;
	adapt->PutImage = I830PutImage;
	adapt->QueryImageAttributes = I830QueryImageAttributes;

	pPriv->textured = FALSE;
	pPriv->colorKey = intel->colorKey & ((1 << scrn->depth) - 1);
	pPriv->videoStatus = 0;
	pPriv->brightness = -19;	/* (255/219) * -16 */
	pPriv->contrast = 75;	/* 255/219 * 64 */
	pPriv->saturation = 146;	/* 128/112 * 128 */
	pPriv->current_crtc = NULL;
	pPriv->desired_crtc = NULL;
	pPriv->buf = NULL;
	pPriv->oldBuf = NULL;
	pPriv->oldBuf_pinned = FALSE;
	pPriv->gamma5 = 0xc0c0c0;
	pPriv->gamma4 = 0x808080;
	pPriv->gamma3 = 0x404040;
	pPriv->gamma2 = 0x202020;
	pPriv->gamma1 = 0x101010;
	pPriv->gamma0 = 0x080808;

	pPriv->rotation = RR_Rotate_0;

	/* gotta uninit this someplace */
	REGION_NULL(pScreen, &pPriv->clip);

	intel->adaptor = adapt;

	/* With LFP's we need to detect whether we're in One Line Mode, which
	 * essentially means a resolution greater than 1024x768, and fix up
	 * the scaler accordingly. */
	pPriv->scaleRatio = 0x10000;
	pPriv->oneLineMode = FALSE;

	/*
	 * Initialise pPriv->overlayOK.  Set it to TRUE here so that a warning will
	 * be generated if i830_crtc_dpms_video() sets it to FALSE during mode
	 * setup.
	 */
	pPriv->overlayOK = TRUE;

	xvColorKey = MAKE_ATOM("XV_COLORKEY");
	xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
	xvContrast = MAKE_ATOM("XV_CONTRAST");
	xvSaturation = MAKE_ATOM("XV_SATURATION");

	/* Allow the pipe to be switched from pipe A to B when in clone mode */
	xvPipe = MAKE_ATOM("XV_PIPE");

	if (IS_I9XX(intel)) {
		xvGamma0 = MAKE_ATOM("XV_GAMMA0");
		xvGamma1 = MAKE_ATOM("XV_GAMMA1");
		xvGamma2 = MAKE_ATOM("XV_GAMMA2");
		xvGamma3 = MAKE_ATOM("XV_GAMMA3");
		xvGamma4 = MAKE_ATOM("XV_GAMMA4");
		xvGamma5 = MAKE_ATOM("XV_GAMMA5");
	}

	drmmode_overlay_update_attrs(scrn);

	return adapt;
}

static XF86VideoAdaptorPtr I830SetupImageVideoTextured(ScreenPtr pScreen)
{
	XF86VideoAdaptorPtr adapt;
	XF86AttributePtr attrs;
	I830PortPrivPtr portPrivs;
	DevUnion *devUnions;
	int nports = 16, i;
	int nAttributes;

	OVERLAY_DEBUG("I830SetupImageVideoOverlay\n");

	nAttributes = NUM_TEXTURED_ATTRIBUTES;

	adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec));
	portPrivs = xcalloc(nports, sizeof(I830PortPrivRec));
	devUnions = xcalloc(nports, sizeof(DevUnion));
	attrs = xcalloc(nAttributes, sizeof(XF86AttributeRec));
	if (adapt == NULL || portPrivs == NULL || devUnions == NULL ||
	    attrs == NULL) {
		xfree(adapt);
		xfree(portPrivs);
		xfree(devUnions);
		xfree(attrs);
		return NULL;
	}

	adapt->type = XvWindowMask | XvInputMask | XvImageMask;
	adapt->flags = 0;
	adapt->name = "Intel(R) Textured Video";
	adapt->nEncodings = 1;
	adapt->pEncodings = DummyEncoding;
	adapt->nFormats = NUM_FORMATS;
	adapt->pFormats = Formats;
	adapt->nPorts = nports;
	adapt->pPortPrivates = devUnions;
	adapt->nAttributes = nAttributes;
	adapt->pAttributes = attrs;
	memcpy(attrs, TexturedAttributes,
	       nAttributes * sizeof(XF86AttributeRec));
	adapt->nImages = NUM_IMAGES;
	adapt->pImages = Images;
	adapt->PutVideo = NULL;
	adapt->PutStill = NULL;
	adapt->GetVideo = NULL;
	adapt->GetStill = NULL;
	adapt->StopVideo = I830StopVideo;
	adapt->SetPortAttribute = I830SetPortAttributeTextured;
	adapt->GetPortAttribute = I830GetPortAttribute;
	adapt->QueryBestSize = I830QueryBestSize;
	adapt->PutImage = I830PutImage;
	adapt->QueryImageAttributes = I830QueryImageAttributes;

	for (i = 0; i < nports; i++) {
		I830PortPrivPtr pPriv = &portPrivs[i];

		pPriv->textured = TRUE;
		pPriv->videoStatus = 0;
		pPriv->buf = NULL;
		pPriv->oldBuf = NULL;
		pPriv->oldBuf_pinned = FALSE;

		pPriv->rotation = RR_Rotate_0;
		pPriv->SyncToVblank = 1;

		/* gotta uninit this someplace, XXX: shouldn't be necessary for textured */
		REGION_NULL(pScreen, &pPriv->clip);

		adapt->pPortPrivates[i].ptr = (pointer) (pPriv);
	}

	xvSyncToVblank = MAKE_ATOM("XV_SYNC_TO_VBLANK");

	return adapt;
}

static void i830_free_video_buffers(I830PortPrivPtr pPriv)
{
	if (pPriv->buf) {
		drm_intel_bo_unreference(pPriv->buf);
		pPriv->buf = NULL;
	}

	if (pPriv->oldBuf) {
		if (pPriv->oldBuf_pinned)
			drm_intel_bo_unpin(pPriv->oldBuf);
		drm_intel_bo_unreference(pPriv->oldBuf);
		pPriv->oldBuf = NULL;
		pPriv->oldBuf_pinned = FALSE;
	}
}

static void I830StopVideo(ScrnInfoPtr scrn, pointer data, Bool shutdown)
{
	I830PortPrivPtr pPriv = (I830PortPrivPtr) data;

	if (pPriv->textured)
		return;

	OVERLAY_DEBUG("I830StopVideo\n");

	REGION_EMPTY(scrn->pScreen, &pPriv->clip);

	if (shutdown) {
		if (pPriv->videoStatus & CLIENT_VIDEO_ON)
			drmmode_overlay_off(scrn);

		i830_free_video_buffers(pPriv);
		pPriv->videoStatus = 0;
	} else {
		if (pPriv->videoStatus & CLIENT_VIDEO_ON) {
			pPriv->videoStatus |= OFF_TIMER;
			pPriv->offTime = currentTime.milliseconds + OFF_DELAY;
		}
	}

}

static int
I830SetPortAttributeTextured(ScrnInfoPtr scrn,
			     Atom attribute, INT32 value, pointer data)
{
	I830PortPrivPtr pPriv = (I830PortPrivPtr) data;

	if (attribute == xvBrightness) {
		if ((value < -128) || (value > 127))
			return BadValue;
		pPriv->brightness = value;
		return Success;
	} else if (attribute == xvContrast) {
		if ((value < 0) || (value > 255))
			return BadValue;
		pPriv->contrast = value;
		return Success;
	} else if (attribute == xvSyncToVblank) {
		if ((value < -1) || (value > 1))
			return BadValue;
		pPriv->SyncToVblank = value;
		return Success;
	} else {
		return BadMatch;
	}
}

static int
I830SetPortAttributeOverlay(ScrnInfoPtr scrn,
			    Atom attribute, INT32 value, pointer data)
{
	I830PortPrivPtr pPriv = (I830PortPrivPtr) data;
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (attribute == xvBrightness) {
		if ((value < -128) || (value > 127))
			return BadValue;
		pPriv->brightness = value;
		OVERLAY_DEBUG("BRIGHTNESS\n");
	} else if (attribute == xvContrast) {
		if ((value < 0) || (value > 255))
			return BadValue;
		pPriv->contrast = value;
		OVERLAY_DEBUG("CONTRAST\n");
	} else if (attribute == xvSaturation) {
		if ((value < 0) || (value > 1023))
			return BadValue;
		pPriv->saturation = value;
	} else if (attribute == xvPipe) {
		xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
		if ((value < -1) || (value > xf86_config->num_crtc))
			return BadValue;
		if (value < 0)
			pPriv->desired_crtc = NULL;
		else
			pPriv->desired_crtc = xf86_config->crtc[value];
		/*
		 * Leave this to be updated at the next frame
		 */
	} else if (attribute == xvGamma0 && (IS_I9XX(intel))) {
		pPriv->gamma0 = value;
	} else if (attribute == xvGamma1 && (IS_I9XX(intel))) {
		pPriv->gamma1 = value;
	} else if (attribute == xvGamma2 && (IS_I9XX(intel))) {
		pPriv->gamma2 = value;
	} else if (attribute == xvGamma3 && (IS_I9XX(intel))) {
		pPriv->gamma3 = value;
	} else if (attribute == xvGamma4 && (IS_I9XX(intel))) {
		pPriv->gamma4 = value;
	} else if (attribute == xvGamma5 && (IS_I9XX(intel))) {
		pPriv->gamma5 = value;
	} else if (attribute == xvColorKey) {
		pPriv->colorKey = value;
		OVERLAY_DEBUG("COLORKEY\n");
	} else
		return BadMatch;

	/* Ensure that the overlay is off, ready for updating */
	if ((attribute == xvGamma0 ||
	     attribute == xvGamma1 ||
	     attribute == xvGamma2 ||
	     attribute == xvGamma3 ||
	     attribute == xvGamma4 ||
	     attribute == xvGamma5) && (IS_I9XX(intel))) {
		OVERLAY_DEBUG("GAMMA\n");
	}

	drmmode_overlay_update_attrs(scrn);

	if (attribute == xvColorKey)
		REGION_EMPTY(scrn->pScreen, &pPriv->clip);

	return Success;
}

static int
I830GetPortAttribute(ScrnInfoPtr scrn,
		     Atom attribute, INT32 * value, pointer data)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	I830PortPrivPtr pPriv = (I830PortPrivPtr) data;

	if (attribute == xvBrightness) {
		*value = pPriv->brightness;
	} else if (attribute == xvContrast) {
		*value = pPriv->contrast;
	} else if (attribute == xvSaturation) {
		*value = pPriv->saturation;
	} else if (attribute == xvPipe) {
		int c;
		xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
		for (c = 0; c < xf86_config->num_crtc; c++)
			if (xf86_config->crtc[c] == pPriv->desired_crtc)
				break;
		if (c == xf86_config->num_crtc)
			c = -1;
		*value = c;
	} else if (attribute == xvGamma0 && (IS_I9XX(intel))) {
		*value = pPriv->gamma0;
	} else if (attribute == xvGamma1 && (IS_I9XX(intel))) {
		*value = pPriv->gamma1;
	} else if (attribute == xvGamma2 && (IS_I9XX(intel))) {
		*value = pPriv->gamma2;
	} else if (attribute == xvGamma3 && (IS_I9XX(intel))) {
		*value = pPriv->gamma3;
	} else if (attribute == xvGamma4 && (IS_I9XX(intel))) {
		*value = pPriv->gamma4;
	} else if (attribute == xvGamma5 && (IS_I9XX(intel))) {
		*value = pPriv->gamma5;
	} else if (attribute == xvColorKey) {
		*value = pPriv->colorKey;
	} else if (attribute == xvSyncToVblank) {
		*value = pPriv->SyncToVblank;
	} else
		return BadMatch;

	return Success;
}

static void
I830QueryBestSize(ScrnInfoPtr scrn,
		  Bool motion,
		  short vid_w, short vid_h,
		  short drw_w, short drw_h,
		  unsigned int *p_w, unsigned int *p_h, pointer data)
{
	if (vid_w > (drw_w << 1))
		drw_w = vid_w >> 1;
	if (vid_h > (drw_h << 1))
		drw_h = vid_h >> 1;

	*p_w = drw_w;
	*p_h = drw_h;
}

static void
I830CopyPackedData(I830PortPrivPtr pPriv,
		   unsigned char *buf,
		   int srcPitch, int dstPitch, int top, int left, int h, int w)
{
	unsigned char *src, *dst, *dst_base;
	int i, j;
	unsigned char *s;

#if 0
	ErrorF("I830CopyPackedData: (%d,%d) (%d,%d)\n"
	       "srcPitch: %d, dstPitch: %d\n", top, left, h, w,
	       srcPitch, dstPitch);
#endif

	src = buf + (top * srcPitch) + (left << 1);

	drm_intel_bo_map(pPriv->buf, TRUE);
	dst_base = pPriv->buf->virtual;

	dst = dst_base + pPriv->YBufOffset;

	switch (pPriv->rotation) {
	case RR_Rotate_0:
		w <<= 1;
		for (i = 0; i < h; i++) {
			memcpy(dst, src, w);
			src += srcPitch;
			dst += dstPitch;
		}
		break;
	case RR_Rotate_90:
		h <<= 1;
		for (i = 0; i < h; i += 2) {
			s = src;
			for (j = 0; j < w; j++) {
				/* Copy Y */
				dst[(i + 0) + ((w - j - 1) * dstPitch)] = *s++;
				(void)*s++;
			}
			src += srcPitch;
		}
		h >>= 1;
		src = buf + (top * srcPitch) + (left << 1);
		for (i = 0; i < h; i += 2) {
			for (j = 0; j < w; j += 2) {
				/* Copy U */
				dst[((i * 2) + 1) + ((w - j - 1) * dstPitch)] =
				    src[(j * 2) + 1 + (i * srcPitch)];
				dst[((i * 2) + 1) + ((w - j - 2) * dstPitch)] =
				    src[(j * 2) + 1 + ((i + 1) * srcPitch)];
				/* Copy V */
				dst[((i * 2) + 3) + ((w - j - 1) * dstPitch)] =
				    src[(j * 2) + 3 + (i * srcPitch)];
				dst[((i * 2) + 3) + ((w - j - 2) * dstPitch)] =
				    src[(j * 2) + 3 + ((i + 1) * srcPitch)];
			}
		}
		break;
	case RR_Rotate_180:
		w <<= 1;
		for (i = 0; i < h; i++) {
			s = src;
			for (j = 0; j < w; j += 4) {
				dst[(w - j - 4) + ((h - i - 1) * dstPitch)] =
				    *s++;
				dst[(w - j - 3) + ((h - i - 1) * dstPitch)] =
				    *s++;
				dst[(w - j - 2) + ((h - i - 1) * dstPitch)] =
				    *s++;
				dst[(w - j - 1) + ((h - i - 1) * dstPitch)] =
				    *s++;
			}
			src += srcPitch;
		}
		break;
	case RR_Rotate_270:
		h <<= 1;
		for (i = 0; i < h; i += 2) {
			s = src;
			for (j = 0; j < w; j++) {
				/* Copy Y */
				dst[(h - i - 2) + (j * dstPitch)] = *s++;
				(void)*s++;
			}
			src += srcPitch;
		}
		h >>= 1;
		src = buf + (top * srcPitch) + (left << 1);
		for (i = 0; i < h; i += 2) {
			for (j = 0; j < w; j += 2) {
				/* Copy U */
				dst[(((h - i) * 2) - 3) + (j * dstPitch)] =
				    src[(j * 2) + 1 + (i * srcPitch)];
				dst[(((h - i) * 2) - 3) +
				    ((j - 1) * dstPitch)] =
				    src[(j * 2) + 1 + ((i + 1) * srcPitch)];
				/* Copy V */
				dst[(((h - i) * 2) - 1) + (j * dstPitch)] =
				    src[(j * 2) + 3 + (i * srcPitch)];
				dst[(((h - i) * 2) - 1) +
				    ((j - 1) * dstPitch)] =
				    src[(j * 2) + 3 + ((i + 1) * srcPitch)];
			}
		}
		break;
	}

	drm_intel_bo_unmap(pPriv->buf);
}

static void i830_memcpy_plane(unsigned char *dst, unsigned char *src,
			      int height, int width,
			      int dstPitch, int srcPitch, Rotation rotation)
{
	int i, j = 0;
	unsigned char *s;

	switch (rotation) {
	case RR_Rotate_0:
		/* optimise for the case of no clipping */
		if (srcPitch == dstPitch && srcPitch == width)
			memcpy(dst, src, srcPitch * height);
		else
			for (i = 0; i < height; i++) {
				memcpy(dst, src, width);
				src += srcPitch;
				dst += dstPitch;
			}
		break;
	case RR_Rotate_90:
		for (i = 0; i < height; i++) {
			s = src;
			for (j = 0; j < width; j++) {
				dst[(i) + ((width - j - 1) * dstPitch)] = *s++;
			}
			src += srcPitch;
		}
		break;
	case RR_Rotate_180:
		for (i = 0; i < height; i++) {
			s = src;
			for (j = 0; j < width; j++) {
				dst[(width - j - 1) +
				    ((height - i - 1) * dstPitch)] = *s++;
			}
			src += srcPitch;
		}
		break;
	case RR_Rotate_270:
		for (i = 0; i < height; i++) {
			s = src;
			for (j = 0; j < width; j++) {
				dst[(height - i - 1) + (j * dstPitch)] = *s++;
			}
			src += srcPitch;
		}
		break;
	}
}

static void
I830CopyPlanarData(I830PortPrivPtr pPriv,
		   unsigned char *buf, int srcPitch,
		   int srcPitch2, int dstPitch, int srcH, int top, int left,
		   int h, int w, int id)
{
	unsigned char *src1, *src2, *src3, *dst_base, *dst1, *dst2, *dst3;
	int dstPitch2 = dstPitch << 1;

#if 0
	ErrorF("I830CopyPlanarData: srcPitch %d, srcPitch %d, dstPitch %d\n"
	       "nlines %d, npixels %d, top %d, left %d\n",
	       srcPitch, srcPitch2, dstPitch, h, w, top, left);
#endif

	/* Copy Y data */
	src1 = buf + (top * srcPitch) + left;
#if 0
	ErrorF("src1 is %p, offset is %ld\n", src1,
	       (unsigned long)src1 - (unsigned long)buf);
#endif

	drm_intel_bo_map(pPriv->buf, TRUE);
	dst_base = pPriv->buf->virtual;

	dst1 = dst_base + pPriv->YBufOffset;

	i830_memcpy_plane(dst1, src1, h, w, dstPitch2, srcPitch,
			  pPriv->rotation);

	/* Copy V data for YV12, or U data for I420 */
	src2 = buf +		/* start of YUV data */
	    (srcH * srcPitch) +	/* move over Luma plane */
	    ((top * srcPitch) >> 2) +	/* move down from by top lines */
	    (left >> 1);	/* move left by left pixels */

#if 0
	ErrorF("src2 is %p, offset is %ld\n", src2,
	       (unsigned long)src2 - (unsigned long)buf);
#endif
	if (id == FOURCC_I420)
		dst2 = dst_base + pPriv->UBufOffset;
	else
		dst2 = dst_base + pPriv->VBufOffset;

	i830_memcpy_plane(dst2, src2, h / 2, w / 2,
			  dstPitch, srcPitch2, pPriv->rotation);

	/* Copy U data for YV12, or V data for I420 */
	src3 = buf +		/* start of YUV data */
	    (srcH * srcPitch) +	/* move over Luma plane */
	    ((srcH >> 1) * srcPitch2) +	/* move over Chroma plane */
	    ((top * srcPitch) >> 2) +	/* move down from by top lines */
	    (left >> 1);	/* move left by left pixels */
#if 0
	ErrorF("src3 is %p, offset is %ld\n", src3,
	       (unsigned long)src3 - (unsigned long)buf);
#endif
	if (id == FOURCC_I420)
		dst3 = dst_base + pPriv->VBufOffset;
	else
		dst3 = dst_base + pPriv->UBufOffset;

	i830_memcpy_plane(dst3, src3, h / 2, w / 2,
			  dstPitch, srcPitch2, pPriv->rotation);

	drm_intel_bo_unmap(pPriv->buf);
}

typedef struct {
	uint8_t sign;
	uint16_t mantissa;
	uint8_t exponent;
} coeffRec, *coeffPtr;

static void i830_box_intersect(BoxPtr dest, BoxPtr a, BoxPtr b)
{
	dest->x1 = a->x1 > b->x1 ? a->x1 : b->x1;
	dest->x2 = a->x2 < b->x2 ? a->x2 : b->x2;
	dest->y1 = a->y1 > b->y1 ? a->y1 : b->y1;
	dest->y2 = a->y2 < b->y2 ? a->y2 : b->y2;
	if (dest->x1 >= dest->x2 || dest->y1 >= dest->y2)
		dest->x1 = dest->x2 = dest->y1 = dest->y2 = 0;
}

static void i830_crtc_box(xf86CrtcPtr crtc, BoxPtr crtc_box)
{
	if (crtc->enabled) {
		crtc_box->x1 = crtc->x;
		crtc_box->x2 =
		    crtc->x + xf86ModeWidth(&crtc->mode, crtc->rotation);
		crtc_box->y1 = crtc->y;
		crtc_box->y2 =
		    crtc->y + xf86ModeHeight(&crtc->mode, crtc->rotation);
	} else
		crtc_box->x1 = crtc_box->x2 = crtc_box->y1 = crtc_box->y2 = 0;
}

static int i830_box_area(BoxPtr box)
{
	return (int)(box->x2 - box->x1) * (int)(box->y2 - box->y1);
}

/*
 * Return the crtc covering 'box'. If two crtcs cover a portion of
 * 'box', then prefer 'desired'. If 'desired' is NULL, then prefer the crtc
 * with greater coverage
 */

xf86CrtcPtr
i830_covering_crtc(ScrnInfoPtr scrn,
		   BoxPtr box, xf86CrtcPtr desired, BoxPtr crtc_box_ret)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	xf86CrtcPtr crtc, best_crtc;
	int coverage, best_coverage;
	int c;
	BoxRec crtc_box, cover_box;

	best_crtc = NULL;
	best_coverage = 0;
	crtc_box_ret->x1 = 0;
	crtc_box_ret->x2 = 0;
	crtc_box_ret->y1 = 0;
	crtc_box_ret->y2 = 0;
	for (c = 0; c < xf86_config->num_crtc; c++) {
		crtc = xf86_config->crtc[c];

		/* If the CRTC is off, treat it as not covering */
		if (!i830_crtc_on(crtc))
			continue;

		i830_crtc_box(crtc, &crtc_box);
		i830_box_intersect(&cover_box, &crtc_box, box);
		coverage = i830_box_area(&cover_box);
		if (coverage && crtc == desired) {
			*crtc_box_ret = crtc_box;
			return crtc;
		}
		if (coverage > best_coverage) {
			*crtc_box_ret = crtc_box;
			best_crtc = crtc;
			best_coverage = coverage;
		}
	}
	return best_crtc;
}

static void
i830_update_dst_box_to_crtc_coords(ScrnInfoPtr scrn, xf86CrtcPtr crtc,
				   BoxPtr dstBox)
{
	int tmp;

	/* for overlay, we should take it from crtc's screen
	 * coordinate to current crtc's display mode.
	 * yeah, a bit confusing.
	 */
	switch (crtc->rotation & 0xf) {
	case RR_Rotate_0:
		dstBox->x1 -= crtc->x;
		dstBox->x2 -= crtc->x;
		dstBox->y1 -= crtc->y;
		dstBox->y2 -= crtc->y;
		break;
	case RR_Rotate_90:
		tmp = dstBox->x1;
		dstBox->x1 = dstBox->y1 - crtc->x;
		dstBox->y1 = scrn->virtualX - tmp - crtc->y;
		tmp = dstBox->x2;
		dstBox->x2 = dstBox->y2 - crtc->x;
		dstBox->y2 = scrn->virtualX - tmp - crtc->y;
		tmp = dstBox->y1;
		dstBox->y1 = dstBox->y2;
		dstBox->y2 = tmp;
		break;
	case RR_Rotate_180:
		tmp = dstBox->x1;
		dstBox->x1 = scrn->virtualX - dstBox->x2 - crtc->x;
		dstBox->x2 = scrn->virtualX - tmp - crtc->x;
		tmp = dstBox->y1;
		dstBox->y1 = scrn->virtualY - dstBox->y2 - crtc->y;
		dstBox->y2 = scrn->virtualY - tmp - crtc->y;
		break;
	case RR_Rotate_270:
		tmp = dstBox->x1;
		dstBox->x1 = scrn->virtualY - dstBox->y1 - crtc->x;
		dstBox->y1 = tmp - crtc->y;
		tmp = dstBox->x2;
		dstBox->x2 = scrn->virtualY - dstBox->y2 - crtc->x;
		dstBox->y2 = tmp - crtc->y;
		tmp = dstBox->x1;
		dstBox->x1 = dstBox->x2;
		dstBox->x2 = tmp;
		break;
	}

	return;
}

int is_planar_fourcc(int id)
{
	switch (id) {
	case FOURCC_YV12:
	case FOURCC_I420:
#ifdef INTEL_XVMC
	case FOURCC_XVMC:
#endif
		return 1;
	case FOURCC_UYVY:
	case FOURCC_YUY2:
		return 0;
	default:
		ErrorF("Unknown format 0x%x\n", id);
		return 0;
	}
}

static int xvmc_passthrough(int id, Rotation rotation)
{
#ifdef INTEL_XVMC
	return id == FOURCC_XVMC && rotation == RR_Rotate_0;
#else
	return 0;
#endif
}

static Bool
i830_display_overlay(ScrnInfoPtr scrn, xf86CrtcPtr crtc,
		     int id, short width, short height,
		     int dstPitch, int x1, int y1, int x2, int y2,
		     BoxPtr dstBox, short src_w, short src_h, short drw_w,
		     short drw_h)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	I830PortPrivPtr pPriv = intel->adaptor->pPortPrivates[0].ptr;
	int tmp;

	OVERLAY_DEBUG("I830DisplayVideo: %dx%d (pitch %d)\n", width, height,
		      dstPitch);

#if VIDEO_DEBUG
	CompareOverlay(intel, (uint32_t *) overlay, 0x100);
#endif

	/*
	 * If the video isn't visible on any CRTC, turn it off
	 */
	if (!crtc) {
		pPriv->current_crtc = NULL;
		drmmode_overlay_off(scrn);

		return TRUE;
	}

	i830_update_dst_box_to_crtc_coords(scrn, crtc, dstBox);

	if (crtc->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
		tmp = width;
		width = height;
		height = tmp;
		tmp = drw_w;
		drw_w = drw_h;
		drw_h = tmp;
		tmp = src_w;
		src_w = src_h;
		src_h = tmp;
	}

	return drmmode_overlay_put_image(scrn, crtc, id, width, height,
					 dstPitch, x1, y1, x2, y2, dstBox,
					 src_w, src_h, drw_w, drw_h);
}

static Bool
i830_clip_video_helper(ScrnInfoPtr scrn,
		       I830PortPrivPtr pPriv,
		       xf86CrtcPtr * crtc_ret,
		       BoxPtr dst,
		       INT32 * xa,
		       INT32 * xb,
		       INT32 * ya,
		       INT32 * yb, RegionPtr reg, INT32 width, INT32 height)
{
	Bool ret;
	RegionRec crtc_region_local;
	RegionPtr crtc_region = reg;

	/*
	 * For overlay video, compute the relevant CRTC and
	 * clip video to that
	 */
	if (crtc_ret) {
		BoxRec crtc_box;
		xf86CrtcPtr crtc = i830_covering_crtc(scrn, dst,
						      pPriv->desired_crtc,
						      &crtc_box);

		/* For textured video, we don't actually want to clip at all. */
		if (crtc && !pPriv->textured) {
			REGION_INIT(pScreen, &crtc_region_local, &crtc_box, 1);
			crtc_region = &crtc_region_local;
			REGION_INTERSECT(pScreen, crtc_region, crtc_region,
					 reg);
		}
		*crtc_ret = crtc;
	}
	ret = xf86XVClipVideoHelper(dst, xa, xb, ya, yb,
				    crtc_region, width, height);
	if (crtc_region != reg)
		REGION_UNINIT(pScreen, &crtc_region_local);
	return ret;
}

static void
i830_fill_colorkey(ScreenPtr pScreen, uint32_t key, RegionPtr clipboxes)
{
	DrawablePtr root = &WindowTable[pScreen->myNum]->drawable;
	XID pval[2];
	BoxPtr pbox = REGION_RECTS(clipboxes);
	int i, nbox = REGION_NUM_RECTS(clipboxes);
	xRectangle *rects;
	GCPtr gc;

	if (!xf86Screens[pScreen->myNum]->vtSema)
		return;

	gc = GetScratchGC(root->depth, pScreen);
	pval[0] = key;
	pval[1] = IncludeInferiors;
	(void)ChangeGC(gc, GCForeground | GCSubwindowMode, pval);
	ValidateGC(root, gc);

	rects = xalloc(nbox * sizeof(xRectangle));

	for (i = 0; i < nbox; i++, pbox++) {
		rects[i].x = pbox->x1;
		rects[i].y = pbox->y1;
		rects[i].width = pbox->x2 - pbox->x1;
		rects[i].height = pbox->y2 - pbox->y1;
	}

	(*gc->ops->PolyFillRect) (root, gc, nbox, rects);

	xfree(rects);
	FreeScratchGC(gc);
}

static void
i830_wait_for_scanline(ScrnInfoPtr scrn, PixmapPtr pPixmap,
		       xf86CrtcPtr crtc, RegionPtr clipBoxes)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	BoxPtr box;
	pixman_box16_t box_in_crtc_coordinates;
	int pipe = -1, event, load_scan_lines_pipe;

	if (pixmap_is_scanout(pPixmap))
		pipe = i830_crtc_to_pipe(crtc);

	if (pipe >= 0) {
		if (pipe == 0) {
			event = MI_WAIT_FOR_PIPEA_SCAN_LINE_WINDOW;
			load_scan_lines_pipe = MI_LOAD_SCAN_LINES_DISPLAY_PIPEA;
		} else {
			event = MI_WAIT_FOR_PIPEB_SCAN_LINE_WINDOW;
			load_scan_lines_pipe = MI_LOAD_SCAN_LINES_DISPLAY_PIPEB;
		}

		box = REGION_EXTENTS(unused, clipBoxes);
		box_in_crtc_coordinates = *box;
		if (crtc->transform_in_use)
			pixman_f_transform_bounds(&crtc->f_framebuffer_to_crtc,
						  &box_in_crtc_coordinates);

		BEGIN_BATCH(5);
		/* The documentation says that the LOAD_SCAN_LINES command
		 * always comes in pairs. Don't ask me why. */
		OUT_BATCH(MI_LOAD_SCAN_LINES_INCL | load_scan_lines_pipe);
		OUT_BATCH((box_in_crtc_coordinates.
			   y1 << 16) | box_in_crtc_coordinates.y2);
		OUT_BATCH(MI_LOAD_SCAN_LINES_INCL | load_scan_lines_pipe);
		OUT_BATCH((box_in_crtc_coordinates.
			   y1 << 16) | box_in_crtc_coordinates.y2);
		OUT_BATCH(MI_WAIT_FOR_EVENT | event);
		ADVANCE_BATCH();
	}
}

static Bool
i830_setup_video_buffer(ScrnInfoPtr scrn, I830PortPrivPtr pPriv,
			int alloc_size, int id)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	/* Free the current buffer if we're going to have to reallocate */
	if (pPriv->buf && pPriv->buf->size < alloc_size) {
		drm_intel_bo_unreference(pPriv->buf);
		pPriv->buf = NULL;
	}

	if (xvmc_passthrough(id, pPriv->rotation)) {
		i830_free_video_buffers(pPriv);
	} else {
		if (pPriv->buf == NULL) {
			pPriv->buf = drm_intel_bo_alloc(intel->bufmgr,
							"xv buffer", alloc_size,
							4096);
			if (pPriv->buf == NULL)
				return FALSE;
		}
	}

	return TRUE;
}

static void
i830_dst_pitch_and_size(ScrnInfoPtr scrn, I830PortPrivPtr pPriv, short width,
			short height, int *dstPitch, int *dstPitch2, int *size,
			int id)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int pitchAlignMask;

	/* Only needs to be DWORD-aligned for textured on i915, but overlay has
	 * stricter requirements.
	 */
	if (pPriv->textured) {
		pitchAlignMask = 3;
#ifdef INTEL_XVMC
		/* for i915 xvmc, hw requires at least 1kb aligned surface */
		if ((id == FOURCC_XVMC) && IS_I915(intel))
			pitchAlignMask = 0x3ff;
#endif
	} else {
		if (IS_I965G(intel))
			pitchAlignMask = 255;
		else
			pitchAlignMask = 63;
	}

	/* Determine the desired destination pitch (representing the chroma's pitch,
	 * in the planar case.
	 */
	switch (id) {
	case FOURCC_YV12:
	case FOURCC_I420:
		if (pPriv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
			*dstPitch =
			    ((height / 2) + pitchAlignMask) & ~pitchAlignMask;
			*size = *dstPitch * width * 3;
		} else {
			*dstPitch =
			    ((width / 2) + pitchAlignMask) & ~pitchAlignMask;
			*size = *dstPitch * height * 3;
		}
		break;
	case FOURCC_UYVY:
	case FOURCC_YUY2:

		if (pPriv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
			*dstPitch =
			    ((height << 1) + pitchAlignMask) & ~pitchAlignMask;
			*size = *dstPitch * width;
		} else {
			*dstPitch =
			    ((width << 1) + pitchAlignMask) & ~pitchAlignMask;
			*size = *dstPitch * height;
		}
		break;
#ifdef INTEL_XVMC
	case FOURCC_XVMC:
		*dstPitch = ((width / 2) + pitchAlignMask) & ~pitchAlignMask;
		*dstPitch2 = (width + pitchAlignMask) & ~pitchAlignMask;
		*size = 0;
		break;
#endif
	default:
		*dstPitch = 0;
		*size = 0;
		break;
	}
#if 0
	ErrorF("srcPitch: %d, dstPitch: %d, size: %d\n", srcPitch, *dstPitch,
	       size);
#endif
}

static Bool
i830_copy_video_data(ScrnInfoPtr scrn, I830PortPrivPtr pPriv,
		     short width, short height, int *dstPitch, int *dstPitch2,
		     INT32 x1, INT32 y1, INT32 x2, INT32 y2,
		     int id, unsigned char *buf)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int srcPitch = 0, srcPitch2 = 0;
	int top, left, npixels, nlines, size;

	if (is_planar_fourcc(id)) {
		srcPitch = (width + 0x3) & ~0x3;
		srcPitch2 = ((width >> 1) + 0x3) & ~0x3;
	} else {
		srcPitch = width << 1;
	}

	i830_dst_pitch_and_size(scrn, pPriv, width, height, dstPitch,
				dstPitch2, &size, id);

	if (!i830_setup_video_buffer(scrn, pPriv, size, id))
		return FALSE;

	/* fixup pointers */
#ifdef INTEL_XVMC
	if (id == FOURCC_XVMC && IS_I915(intel)) {
		pPriv->YBufOffset = (uint32_t) ((uintptr_t) buf);
		pPriv->VBufOffset = pPriv->YBufOffset + (*dstPitch2 * height);
		pPriv->UBufOffset =
		    pPriv->VBufOffset + (*dstPitch * height / 2);
	} else {
#endif
		pPriv->YBufOffset = 0;

		if (pPriv->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
			pPriv->UBufOffset =
			    pPriv->YBufOffset + (*dstPitch * 2 * width);
			pPriv->VBufOffset =
			    pPriv->UBufOffset + (*dstPitch * width / 2);
		} else {
			pPriv->UBufOffset =
			    pPriv->YBufOffset + (*dstPitch * 2 * height);
			pPriv->VBufOffset =
			    pPriv->UBufOffset + (*dstPitch * height / 2);
		}
#ifdef INTEL_XVMC
	}
#endif

	/* copy data */
	top = y1 >> 16;
	left = (x1 >> 16) & ~1;
	npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;

	if (is_planar_fourcc(id)) {
		if (!xvmc_passthrough(id, pPriv->rotation)) {
			top &= ~1;
			nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
			I830CopyPlanarData(pPriv, buf, srcPitch, srcPitch2,
					   *dstPitch, height, top, left, nlines,
					   npixels, id);
		}
	} else {
		nlines = ((y2 + 0xffff) >> 16) - top;
		I830CopyPackedData(pPriv, buf, srcPitch, *dstPitch, top, left,
				   nlines, npixels);
	}

	return TRUE;
}

/*
 * The source rectangle of the video is defined by (src_x, src_y, src_w, src_h).
 * The dest rectangle of the video is defined by (drw_x, drw_y, drw_w, drw_h).
 * id is a fourcc code for the format of the video.
 * buf is the pointer to the source data in system memory.
 * width and height are the w/h of the source data.
 * If "sync" is TRUE, then we must be finished with *buf at the point of return
 * (which we always are).
 * clipBoxes is the clipping region in screen space.
 * data is a pointer to our port private.
 * pDraw is a Drawable, which might not be the screen in the case of
 * compositing.  It's a new argument to the function in the 1.1 server.
 */
static int
I830PutImage(ScrnInfoPtr scrn,
	     short src_x, short src_y,
	     short drw_x, short drw_y,
	     short src_w, short src_h,
	     short drw_w, short drw_h,
	     int id, unsigned char *buf,
	     short width, short height,
	     Bool sync, RegionPtr clipBoxes, pointer data, DrawablePtr pDraw)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	I830PortPrivPtr pPriv = (I830PortPrivPtr) data;
	ScreenPtr pScreen = screenInfo.screens[scrn->scrnIndex];
	PixmapPtr pPixmap = get_drawable_pixmap(pDraw);;
	INT32 x1, x2, y1, y2;
	int dstPitch;
	int dstPitch2 = 0;
	BoxRec dstBox;
	xf86CrtcPtr crtc;

#if 0
	ErrorF("I830PutImage: src: (%d,%d)(%d,%d), dst: (%d,%d)(%d,%d)\n"
	       "width %d, height %d\n", src_x, src_y, src_w, src_h, drw_x,
	       drw_y, drw_w, drw_h, width, height);
#endif

	if (!pPriv->textured) {
		/* If dst width and height are less than 1/8th the src size, the
		 * src/dst scale factor becomes larger than 8 and doesn't fit in
		 * the scale register. */
		if (src_w >= (drw_w * 8))
			drw_w = src_w / 7;

		if (src_h >= (drw_h * 8))
			drw_h = src_h / 7;
	}

	/* Clip */
	x1 = src_x;
	x2 = src_x + src_w;
	y1 = src_y;
	y2 = src_y + src_h;

	dstBox.x1 = drw_x;
	dstBox.x2 = drw_x + drw_w;
	dstBox.y1 = drw_y;
	dstBox.y2 = drw_y + drw_h;

	if (!i830_clip_video_helper(scrn,
				    pPriv,
				    &crtc,
				    &dstBox, &x1, &x2, &y1, &y2, clipBoxes,
				    width, height))
		return Success;

	if (!pPriv->textured) {
		/* texture video handles rotation differently. */
		if (crtc)
			pPriv->rotation = crtc->rotation;
		else {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "Fail to clip video to any crtc!\n");
			return Success;
		}
	}

	if (!i830_copy_video_data(scrn, pPriv, width, height,
				  &dstPitch, &dstPitch2,
				  x1, y1, x2, y2, id, buf))
		return BadAlloc;

	if (!pPriv->textured) {
		if (!i830_display_overlay
		    (scrn, crtc, id, width, height, dstPitch, x1, y1, x2, y2,
		     &dstBox, src_w, src_h, drw_w, drw_h))
			return BadAlloc;

		/* update cliplist */
		if (!REGION_EQUAL(scrn->pScreen, &pPriv->clip, clipBoxes)) {
			REGION_COPY(scrn->pScreen, &pPriv->clip, clipBoxes);
			i830_fill_colorkey(pScreen, pPriv->colorKey, clipBoxes);
		}
	} else {
		if (crtc && pPriv->SyncToVblank != 0) {
			i830_wait_for_scanline(scrn, pPixmap, crtc, clipBoxes);
		}

		if (IS_I965G(intel)) {
			if (xvmc_passthrough(id, pPriv->rotation)) {
				/* XXX: KMS */
				pPriv->YBufOffset = (uintptr_t) buf;
				pPriv->UBufOffset =
				    pPriv->YBufOffset + height * width;
				pPriv->VBufOffset =
				    pPriv->UBufOffset + height * width / 4;
			}
			I965DisplayVideoTextured(scrn, pPriv, id, clipBoxes,
						 width, height, dstPitch, x1,
						 y1, x2, y2, src_w, src_h,
						 drw_w, drw_h, pPixmap);
		} else {
			I915DisplayVideoTextured(scrn, pPriv, id, clipBoxes,
						 width, height, dstPitch,
						 dstPitch2, x1, y1, x2, y2,
						 src_w, src_h, drw_w, drw_h,
						 pPixmap);
		}

		DamageDamageRegion(pDraw, clipBoxes);
	}

	pPriv->videoStatus = CLIENT_VIDEO_ON;

	return Success;
}

static int
I830QueryImageAttributes(ScrnInfoPtr scrn,
			 int id,
			 unsigned short *w, unsigned short *h,
			 int *pitches, int *offsets)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int size, tmp;

#if 0
	ErrorF("I830QueryImageAttributes: w is %d, h is %d\n", *w, *h);
#endif

	if (IS_845G(intel) || IS_I830(intel)) {
		if (*w > IMAGE_MAX_WIDTH_LEGACY)
			*w = IMAGE_MAX_WIDTH_LEGACY;
		if (*h > IMAGE_MAX_HEIGHT_LEGACY)
			*h = IMAGE_MAX_HEIGHT_LEGACY;
	} else {
		if (*w > IMAGE_MAX_WIDTH)
			*w = IMAGE_MAX_WIDTH;
		if (*h > IMAGE_MAX_HEIGHT)
			*h = IMAGE_MAX_HEIGHT;
	}

	*w = (*w + 1) & ~1;
	if (offsets)
		offsets[0] = 0;

	switch (id) {
		/* IA44 is for XvMC only */
	case FOURCC_IA44:
	case FOURCC_AI44:
		if (pitches)
			pitches[0] = *w;
		size = *w * *h;
		break;
	case FOURCC_YV12:
	case FOURCC_I420:
		*h = (*h + 1) & ~1;
		size = (*w + 3) & ~3;
		if (pitches)
			pitches[0] = size;
		size *= *h;
		if (offsets)
			offsets[1] = size;
		tmp = ((*w >> 1) + 3) & ~3;
		if (pitches)
			pitches[1] = pitches[2] = tmp;
		tmp *= (*h >> 1);
		size += tmp;
		if (offsets)
			offsets[2] = size;
		size += tmp;
#if 0
		if (pitches)
			ErrorF("pitch 0 is %d, pitch 1 is %d, pitch 2 is %d\n",
			       pitches[0], pitches[1], pitches[2]);
		if (offsets)
			ErrorF("offset 1 is %d, offset 2 is %d\n", offsets[1],
			       offsets[2]);
		if (offsets)
			ErrorF("size is %d\n", size);
#endif
		break;
#ifdef INTEL_XVMC
	case FOURCC_XVMC:
		*h = (*h + 1) & ~1;
		size = sizeof(struct intel_xvmc_command);
		if (pitches)
			pitches[0] = size;
		break;
#endif
	case FOURCC_UYVY:
	case FOURCC_YUY2:
	default:
		size = *w << 1;
		if (pitches)
			pitches[0] = size;
		size *= *h;
		break;
	}

	return size;
}

void
I830VideoBlockHandler(int i, pointer blockData, pointer pTimeout,
		      pointer pReadmask)
{
	ScrnInfoPtr scrn = xf86Screens[i];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	I830PortPrivPtr pPriv;

	/* no overlay */
	if (intel->adaptor == NULL)
		return;

	pPriv = GET_PORT_PRIVATE(scrn);

	if (pPriv->videoStatus & TIMER_MASK) {
#if 1
		Time now = currentTime.milliseconds;
#else
		UpdateCurrentTime();
#endif
		if (pPriv->videoStatus & OFF_TIMER) {
			if (pPriv->offTime < now) {
				/* Turn off the overlay */
				OVERLAY_DEBUG("BLOCKHANDLER\n");

				drmmode_overlay_off(scrn);

				pPriv->videoStatus = FREE_TIMER;
				pPriv->freeTime = now + FREE_DELAY;
			}
		} else {	/* FREE_TIMER */
			if (pPriv->freeTime < now) {
				i830_free_video_buffers(pPriv);
				pPriv->videoStatus = 0;
			}
		}
	}
}
