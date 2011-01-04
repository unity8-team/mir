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
THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors: Jeff Hartmann <jhartmann@valinux.com>
 *          Abraham van der Merwe <abraham@2d3d.co.za>
 *          David Dawes <dawes@xfree86.org>
 *          Alan Hourihane <alanh@tungstengraphics.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef PRINT_MODE_INFO
#define PRINT_MODE_INFO 0
#endif

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Priv.h"
#include "xf86cmap.h"
#include "compiler.h"
#include "mibstore.h"
#include "vgaHW.h"
#include "mipointer.h"
#include "micmap.h"
#include "shadowfb.h"
#include <X11/extensions/randr.h>
#include "fb.h"
#include "miscstruct.h"
#include "dixstruct.h"
#include "xf86xv.h"
#include <X11/extensions/Xv.h>
#include "shadow.h"
#include "intel.h"
#include "intel_video.h"

#ifdef INTEL_XVMC
#define _INTEL_XVMC_SERVER_
#include "intel_hwmc.h"
#endif

#include "legacy/legacy.h"

#include <sys/ioctl.h>
#include "i915_drm.h"
#include <xf86drmMode.h>

#define BIT(x) (1 << (x))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define NB_OF(x) (sizeof (x) / sizeof (*x))

/* *INDENT-OFF* */
/*
 * Note: "ColorKey" is provided for compatibility with the i810 driver.
 * However, the correct option name is "VideoKey".  "ColorKey" usually
 * refers to the tranparency key for 8+24 overlays, not for video overlays.
 */

typedef enum {
   OPTION_ACCELMETHOD,
   OPTION_DRI,
   OPTION_VIDEO_KEY,
   OPTION_COLOR_KEY,
   OPTION_FALLBACKDEBUG,
   OPTION_TILING,
   OPTION_SHADOW,
   OPTION_SWAPBUFFERS_WAIT,
#ifdef INTEL_XVMC
   OPTION_XVMC,
#endif
   OPTION_PREFER_OVERLAY,
   OPTION_DEBUG_FLUSH_BATCHES,
   OPTION_DEBUG_FLUSH_CACHES,
   OPTION_DEBUG_WAIT,
   OPTION_HOTPLUG,
} I830Opts;

static OptionInfoRec I830Options[] = {
   {OPTION_ACCELMETHOD,	"AccelMethod",	OPTV_ANYSTR,	{0},	FALSE},
   {OPTION_DRI,		"DRI",		OPTV_BOOLEAN,	{0},	TRUE},
   {OPTION_COLOR_KEY,	"ColorKey",	OPTV_INTEGER,	{0},	FALSE},
   {OPTION_VIDEO_KEY,	"VideoKey",	OPTV_INTEGER,	{0},	FALSE},
   {OPTION_FALLBACKDEBUG, "FallbackDebug", OPTV_BOOLEAN, {0},	FALSE},
   {OPTION_TILING,	"Tiling",	OPTV_BOOLEAN,	{0},	TRUE},
   {OPTION_SHADOW,	"Shadow",	OPTV_BOOLEAN,	{0},	FALSE},
   {OPTION_SWAPBUFFERS_WAIT, "SwapbuffersWait", OPTV_BOOLEAN,	{0},	TRUE},
#ifdef INTEL_XVMC
   {OPTION_XVMC,	"XvMC",		OPTV_BOOLEAN,	{0},	TRUE},
#endif
   {OPTION_PREFER_OVERLAY, "XvPreferOverlay", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_DEBUG_FLUSH_BATCHES, "DebugFlushBatches", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_DEBUG_FLUSH_CACHES, "DebugFlushCaches", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_DEBUG_WAIT, "DebugWait", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_HOTPLUG,	"HotPlug",	OPTV_BOOLEAN,	{0},	TRUE},
   {-1,			NULL,		OPTV_NONE,	{0},	FALSE}
};
/* *INDENT-ON* */

static void i830AdjustFrame(int scrnIndex, int x, int y, int flags);
static Bool I830CloseScreen(int scrnIndex, ScreenPtr screen);
static Bool I830EnterVT(int scrnIndex, int flags);

/* temporary */
extern void xf86SetCursor(ScreenPtr screen, CursorPtr pCurs, int x, int y);

#ifdef I830DEBUG
void
I830DPRINTF(const char *filename, int line, const char *function,
	    const char *fmt, ...)
{
	va_list ap;

	ErrorF("\n##############################################\n"
	       "*** In function %s, on line %d, in file %s ***\n",
	       function, line, filename);
	va_start(ap, fmt);
	VErrorF(fmt, ap);
	va_end(ap);
	ErrorF("##############################################\n\n");
}
#endif /* #ifdef I830DEBUG */

/* Export I830 options to i830 driver where necessary */
const OptionInfoRec *intel_uxa_available_options(int chipid, int busid)
{
	return I830Options;
}

static void
I830LoadPalette(ScrnInfoPtr scrn, int numColors, int *indices,
		LOCO * colors, VisualPtr pVisual)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	int i, j, index;
	int p;
	uint16_t lut_r[256], lut_g[256], lut_b[256];

	DPRINTF(PFX, "I830LoadPalette: numColors: %d\n", numColors);

	for (p = 0; p < xf86_config->num_crtc; p++) {
		xf86CrtcPtr crtc = xf86_config->crtc[p];
		I830CrtcPrivatePtr intel_crtc = crtc->driver_private;

		/* Initialize to the old lookup table values. */
		for (i = 0; i < 256; i++) {
			lut_r[i] = intel_crtc->lut_r[i] << 8;
			lut_g[i] = intel_crtc->lut_g[i] << 8;
			lut_b[i] = intel_crtc->lut_b[i] << 8;
		}

		switch (scrn->depth) {
		case 15:
			for (i = 0; i < numColors; i++) {
				index = indices[i];
				for (j = 0; j < 8; j++) {
					lut_r[index * 8 + j] =
					    colors[index].red << 8;
					lut_g[index * 8 + j] =
					    colors[index].green << 8;
					lut_b[index * 8 + j] =
					    colors[index].blue << 8;
				}
			}
			break;
		case 16:
			for (i = 0; i < numColors; i++) {
				index = indices[i];

				if (index <= 31) {
					for (j = 0; j < 8; j++) {
						lut_r[index * 8 + j] =
						    colors[index].red << 8;
						lut_b[index * 8 + j] =
						    colors[index].blue << 8;
					}
				}

				for (j = 0; j < 4; j++) {
					lut_g[index * 4 + j] =
					    colors[index].green << 8;
				}
			}
			break;
		default:
			for (i = 0; i < numColors; i++) {
				index = indices[i];
				lut_r[index] = colors[index].red << 8;
				lut_g[index] = colors[index].green << 8;
				lut_b[index] = colors[index].blue << 8;
			}
			break;
		}

		/* Make the change through RandR */
#ifdef RANDR_12_INTERFACE
		RRCrtcGammaSet(crtc->randr_crtc, lut_r, lut_g, lut_b);
#else
		crtc->funcs->gamma_set(crtc, lut_r, lut_g, lut_b, 256);
#endif
	}
}

/**
 * Adjust the screen pixmap for the current location of the front buffer.
 * This is done at EnterVT when buffers are bound as long as the resources
 * have already been created, but the first EnterVT happens before
 * CreateScreenResources.
 */
static Bool i830CreateScreenResources(ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	screen->CreateScreenResources = intel->CreateScreenResources;
	if (!(*screen->CreateScreenResources) (screen))
		return FALSE;

	intel_uxa_create_screen_resources(screen);

	return TRUE;
}

static void PreInitCleanup(ScrnInfoPtr scrn)
{
	if (!scrn || !scrn->driverPrivate)
		return;

	free(scrn->driverPrivate);
	scrn->driverPrivate = NULL;
}

static void intel_check_chipset_option(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	MessageType from = X_PROBED;

	intel_detect_chipset(scrn,
			     intel->PciInfo,
			     &intel->chipset);

	/* Set the Chipset and ChipRev, allowing config file entries to override. */
	if (intel->pEnt->device->chipset && *intel->pEnt->device->chipset) {
		scrn->chipset = intel->pEnt->device->chipset;
		from = X_CONFIG;
	} else if (intel->pEnt->device->chipID >= 0) {
		scrn->chipset = (char *)xf86TokenToString(intel_chipsets,
							   intel->pEnt->device->chipID);
		from = X_CONFIG;
		xf86DrvMsg(scrn->scrnIndex, X_CONFIG,
			   "ChipID override: 0x%04X\n",
			   intel->pEnt->device->chipID);
		DEVICE_ID(intel->PciInfo) = intel->pEnt->device->chipID;
	} else {
		from = X_PROBED;
		scrn->chipset = (char *)xf86TokenToString(intel_chipsets,
							   DEVICE_ID(intel->PciInfo));
	}

	if (intel->pEnt->device->chipRev >= 0) {
		xf86DrvMsg(scrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
			   intel->pEnt->device->chipRev);
	}

	xf86DrvMsg(scrn->scrnIndex, from, "Chipset: \"%s\"\n",
		   (scrn->chipset != NULL) ? scrn->chipset : "Unknown i8xx");
}

static Bool I830GetEarlyOptions(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	/* Process the options */
	xf86CollectOptions(scrn, NULL);
	if (!(intel->Options = malloc(sizeof(I830Options))))
		return FALSE;
	memcpy(intel->Options, I830Options, sizeof(I830Options));
	xf86ProcessOptions(scrn->scrnIndex, scrn->options, intel->Options);

	intel->fallback_debug = xf86ReturnOptValBool(intel->Options,
						     OPTION_FALLBACKDEBUG,
						     FALSE);

	intel->debug_flush = 0;

	if (xf86ReturnOptValBool(intel->Options,
				 OPTION_DEBUG_FLUSH_BATCHES,
				 FALSE))
		intel->debug_flush |= DEBUG_FLUSH_BATCHES;

	if (xf86ReturnOptValBool(intel->Options,
				 OPTION_DEBUG_FLUSH_CACHES,
				 FALSE))
		intel->debug_flush |= DEBUG_FLUSH_CACHES;

	if (xf86ReturnOptValBool(intel->Options,
				 OPTION_DEBUG_WAIT,
				 FALSE))
		intel->debug_flush |= DEBUG_FLUSH_WAIT;

	return TRUE;
}

static void intel_check_dri_option(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	intel->directRenderingType = DRI_NONE;
	if (!xf86ReturnOptValBool(intel->Options, OPTION_DRI, TRUE))
		intel->directRenderingType = DRI_DISABLED;

	if (scrn->depth != 16 && scrn->depth != 24) {
		xf86DrvMsg(scrn->scrnIndex, X_CONFIG,
			   "DRI is disabled because it "
			   "runs only at depths 16 and 24.\n");
		intel->directRenderingType = DRI_DISABLED;
	}
}

static Bool intel_open_drm_master(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct pci_device *dev = intel->PciInfo;
	drmSetVersion sv;
	struct drm_i915_getparam gp;
	int err, has_gem;
	char busid[20];

	snprintf(busid, sizeof(busid), "pci:%04x:%02x:%02x.%d",
		 dev->domain, dev->bus, dev->dev, dev->func);

	intel->drmSubFD = drmOpen("i915", busid);
	if (intel->drmSubFD == -1) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "[drm] Failed to open DRM device for %s: %s\n",
			   busid, strerror(errno));
		return FALSE;
	}

	/* Check that what we opened was a master or a master-capable FD,
	 * by setting the version of the interface we'll use to talk to it.
	 * (see DRIOpenDRMMaster() in DRI1)
	 */
	sv.drm_di_major = 1;
	sv.drm_di_minor = 1;
	sv.drm_dd_major = -1;
	sv.drm_dd_minor = -1;
	err = drmSetInterfaceVersion(intel->drmSubFD, &sv);
	if (err != 0) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "[drm] failed to set drm interface version.\n");
		drmClose(intel->drmSubFD);
		intel->drmSubFD = -1;
		return FALSE;
	}

	has_gem = FALSE;
	gp.param = I915_PARAM_HAS_GEM;
	gp.value = &has_gem;
	(void)drmCommandWriteRead(intel->drmSubFD, DRM_I915_GETPARAM,
				  &gp, sizeof(gp));
	if (!has_gem) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "[drm] Failed to detect GEM.  Kernel 2.6.28 required.\n");
		drmClose(intel->drmSubFD);
		intel->drmSubFD = -1;
		return FALSE;
	}

	return TRUE;
}

static void intel_close_drm_master(intel_screen_private *intel)
{
	if (intel && intel->drmSubFD > 0) {
		drmClose(intel->drmSubFD);
		intel->drmSubFD = -1;
	}
}

static int intel_init_bufmgr(intel_screen_private *intel)
{
	int batch_size;

	batch_size = 4096 * 4;
	if (IS_I865G(intel))
		/* The 865 has issues with larger-than-page-sized batch buffers. */
		batch_size = 4096;

	intel->bufmgr = drm_intel_bufmgr_gem_init(intel->drmSubFD, batch_size);
	if (!intel->bufmgr)
		return FALSE;

	drm_intel_bufmgr_gem_enable_reuse(intel->bufmgr);
	drm_intel_bufmgr_gem_enable_fenced_relocs(intel->bufmgr);

	list_init(&intel->batch_pixmaps);
	list_init(&intel->flush_pixmaps);
	list_init(&intel->in_flight);

	return TRUE;
}

static void intel_bufmgr_fini(intel_screen_private *intel)
{
	drm_intel_bufmgr_destroy(intel->bufmgr);
}

static void I830XvInit(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	MessageType from = X_PROBED;

	intel->XvPreferOverlay =
	    xf86ReturnOptValBool(intel->Options, OPTION_PREFER_OVERLAY, FALSE);

	if (xf86GetOptValInteger(intel->Options, OPTION_VIDEO_KEY,
				 &(intel->colorKey))) {
		from = X_CONFIG;
	} else if (xf86GetOptValInteger(intel->Options, OPTION_COLOR_KEY,
					&(intel->colorKey))) {
		from = X_CONFIG;
	} else {
		intel->colorKey =
		    (1 << scrn->offset.red) | (1 << scrn->offset.green) |
		    (((scrn->mask.blue >> scrn->offset.blue) - 1) <<
		     scrn->offset.blue);
		from = X_DEFAULT;
	}
	xf86DrvMsg(scrn->scrnIndex, from, "video overlay key set to 0x%x\n",
		   intel->colorKey);
}

static Bool can_accelerate_blt(struct intel_screen_private *intel)
{
	if (0 && (IS_I830(intel) || IS_845G(intel))) {
		/* These pair of i8xx chipsets have a crippling erratum
		 * that prevents the use of a PTE entry by the BLT
		 * engine immediately following updating that
		 * entry in the GATT.
		 *
		 * As the BLT is fundamental to our 2D acceleration,
		 * and the workaround is lost in the midst of time,
		 * fallback.
		 *
		 * XXX disabled for release as causes regressions in GL.
		 */
		return FALSE;
	}

	if (INTEL_INFO(intel)->gen >= 60) {
		drm_i915_getparam_t gp;
		int value;

		/* On Sandybridge we need the BLT in order to do anything since
		 * it so frequently used in the acceleration code paths.
		 */
		gp.value = &value;
		gp.param = I915_PARAM_HAS_BLT;
		if (drmIoctl(intel->drmSubFD, DRM_IOCTL_I915_GETPARAM, &gp))
			return FALSE;
	}

	if (INTEL_INFO(intel)->gen == 60) {
		struct pci_device *const device = intel->PciInfo;

		/* Sandybridge rev07 locks up easily, even with the
		 * BLT ring workaround in place.
		 * Thus use shadowfb by default.
		 */
		if (device->revision < 8)
		    return FALSE;
	}

	return TRUE;
}

/**
 * This is called before ScreenInit to do any require probing of screen
 * configuration.
 *
 * This code generally covers probing, module loading, option handling
 * card mapping, and RandR setup.
 *
 * Since xf86InitialConfiguration ends up requiring that we set video modes
 * in order to detect configuration, we end up having to do a lot of driver
 * setup (talking to the DRM, mapping the device, etc.) in this function.
 * As a result, we want to set up that server initialization once rather
 * that doing it per generation.
 */
static Bool I830PreInit(ScrnInfoPtr scrn, int flags)
{
	intel_screen_private *intel;
	rgb defaultWeight = { 0, 0, 0 };
	EntityInfoPtr pEnt;
	int flags24;
	Gamma zeros = { 0.0, 0.0, 0.0 };

	if (scrn->numEntities != 1)
		return FALSE;

	pEnt = xf86GetEntityInfo(scrn->entityList[0]);

	if (flags & PROBE_DETECT)
		return TRUE;

	intel = intel_get_screen_private(scrn);
	if (intel == NULL) {
		intel = xnfcalloc(sizeof(intel_screen_private), 1);
		if (intel == NULL)
			return FALSE;

		scrn->driverPrivate = intel;
	}
	intel->scrn = scrn;
	intel->pEnt = pEnt;

	scrn->displayWidth = 640;	/* default it */

	if (intel->pEnt->location.type != BUS_PCI)
		return FALSE;

	intel->PciInfo = xf86GetPciInfoForEntity(intel->pEnt->index);

	if (!intel_open_drm_master(scrn))
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Failed to become DRM master.\n");

	scrn->monitor = scrn->confScreen->monitor;
	scrn->progClock = TRUE;
	scrn->rgbBits = 8;

	flags24 = Support32bppFb | PreferConvert24to32 | SupportConvert24to32;

	if (!xf86SetDepthBpp(scrn, 0, 0, 0, flags24))
		return FALSE;

	switch (scrn->depth) {
	case 8:
	case 15:
	case 16:
	case 24:
		break;
	default:
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Given depth (%d) is not supported by I830 driver\n",
			   scrn->depth);
		return FALSE;
	}
	xf86PrintDepthBpp(scrn);

	if (!xf86SetWeight(scrn, defaultWeight, defaultWeight))
		return FALSE;
	if (!xf86SetDefaultVisual(scrn, -1))
		return FALSE;

	intel->cpp = scrn->bitsPerPixel / 8;

	if (!I830GetEarlyOptions(scrn))
		return FALSE;

	intel_check_chipset_option(scrn);
	intel_check_dri_option(scrn);

	if (!intel_init_bufmgr(intel)) {
		PreInitCleanup(scrn);
		return FALSE;
	}

	intel->force_fallback =
		drmCommandNone(intel->drmSubFD, DRM_I915_GEM_THROTTLE) != 0;

	/* Enable tiling by default */
	intel->tiling = TRUE;

	/* Allow user override if they set a value */
	if (!ALWAYS_TILING(intel) && xf86IsOptionSet(intel->Options, OPTION_TILING)) {
		if (xf86ReturnOptValBool(intel->Options, OPTION_TILING, FALSE))
			intel->tiling = TRUE;
		else
			intel->tiling = FALSE;
	}

	intel->can_blt = can_accelerate_blt(intel);
	intel->use_shadow = !intel->can_blt;

	if (xf86IsOptionSet(intel->Options, OPTION_SHADOW)) {
		intel->use_shadow =
			xf86ReturnOptValBool(intel->Options,
					     OPTION_SHADOW,
					     FALSE);
	}

	if (intel->use_shadow) {
		xf86DrvMsg(scrn->scrnIndex, X_CONFIG,
			   "Shadow buffer enabled,"
			   " 2D GPU acceleration disabled.\n");
	}

	/* SwapBuffers delays to avoid tearing */
	intel->swapbuffers_wait = TRUE;

	/* Allow user override if they set a value */
	if (xf86IsOptionSet(intel->Options, OPTION_SWAPBUFFERS_WAIT)) {
		if (xf86ReturnOptValBool
		    (intel->Options, OPTION_SWAPBUFFERS_WAIT, FALSE))
			intel->swapbuffers_wait = TRUE;
		else
			intel->swapbuffers_wait = FALSE;
	}

	if (IS_GEN6(intel))
	    intel->swapbuffers_wait = FALSE;

	xf86DrvMsg(scrn->scrnIndex, X_CONFIG, "Tiling %sabled\n",
		   intel->tiling ? "en" : "dis");
	xf86DrvMsg(scrn->scrnIndex, X_CONFIG, "SwapBuffers wait %sabled\n",
		   intel->swapbuffers_wait ? "en" : "dis");

	I830XvInit(scrn);

	if (!intel_mode_pre_init(scrn, intel->drmSubFD, intel->cpp)) {
		PreInitCleanup(scrn);
		return FALSE;
	}

	if (!xf86SetGamma(scrn, zeros)) {
		PreInitCleanup(scrn);
		return FALSE;
	}

	if (scrn->modes == NULL) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR, "No modes.\n");
		PreInitCleanup(scrn);
		return FALSE;
	}
	scrn->currentMode = scrn->modes;

	/* Set display resolution */
	xf86SetDpi(scrn, 0, 0);

	/* Load the required sub modules */
	if (!xf86LoadSubModule(scrn, "fb")) {
		PreInitCleanup(scrn);
		return FALSE;
	}

	/* Load the dri2 module if requested. */
	if (xf86ReturnOptValBool(intel->Options, OPTION_DRI, FALSE) &&
	    intel->directRenderingType != DRI_DISABLED) {
		xf86LoadSubModule(scrn, "dri2");
	}

	return TRUE;
}

enum pipe {
	PIPE_A = 0,
	PIPE_B,
};

/**
 * Intialiazes the hardware for the 3D pipeline use in the 2D driver.
 *
 * Some state caching is performed to avoid redundant state emits.  This
 * function is also responsible for marking the state as clobbered for DRI
 * clients.
 */
void IntelEmitInvarientState(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	/* If we've emitted our state since the last clobber by another client,
	 * skip it.
	 */
	if (intel->last_3d != LAST_3D_OTHER)
		return;

	if (IS_GEN2(intel))
		I830EmitInvarientState(scrn);
	else if IS_GEN3(intel)
		I915EmitInvarientState(scrn);
}

static void
I830BlockHandler(int i, pointer blockData, pointer pTimeout, pointer pReadmask)
{
	ScreenPtr screen = screenInfo.screens[i];
	ScrnInfoPtr scrn = xf86Screens[i];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	screen->BlockHandler = intel->BlockHandler;

	(*screen->BlockHandler) (i, blockData, pTimeout, pReadmask);

	intel->BlockHandler = screen->BlockHandler;
	screen->BlockHandler = I830BlockHandler;

	intel_uxa_block_handler(intel);
	intel_video_block_handler(intel);
}

static void intel_fixup_mtrrs(ScrnInfoPtr scrn)
{
#ifdef HAS_MTRR_SUPPORT
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int fd;
	struct mtrr_gentry gentry;
	struct mtrr_sentry sentry;

	if ((fd = open("/proc/mtrr", O_RDONLY, 0)) != -1) {
		for (gentry.regnum = 0;
		     ioctl(fd, MTRRIOC_GET_ENTRY, &gentry) == 0;
		     ++gentry.regnum) {

			if (gentry.size < 1) {
				/* DISABLED */
				continue;
			}

			/* Check the MTRR range is one we like and if not - remove it.
			 * The Xserver common layer will then setup the right range
			 * for us.
			 */
			if (gentry.base == intel->LinearAddr &&
			    gentry.size < intel->FbMapSize) {

				xf86DrvMsg(scrn->scrnIndex, X_INFO,
					   "Removing bad MTRR range (base 0x%lx, size 0x%x)\n",
					   gentry.base, gentry.size);

				sentry.base = gentry.base;
				sentry.size = gentry.size;
				sentry.type = gentry.type;

				if (ioctl(fd, MTRRIOC_DEL_ENTRY, &sentry) == -1) {
					xf86DrvMsg(scrn->scrnIndex, X_ERROR,
						   "Failed to remove bad MTRR range\n");
				}
			}
		}
		close(fd);
	}
#endif
}

static Bool
intel_init_initial_framebuffer(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int width = scrn->virtualX;
	int height = scrn->virtualY;
	unsigned long pitch;
	uint32_t tiling;

	intel->front_buffer = intel_allocate_framebuffer(scrn,
							 width, height,
							 intel->cpp,
							 &pitch,
							 &tiling);

	if (!intel->front_buffer) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Couldn't allocate initial framebuffer.\n");
		return FALSE;
	}

	intel->front_pitch = pitch;
	intel->front_tiling = tiling;
	scrn->displayWidth = pitch / intel->cpp;

	return TRUE;
}

Bool intel_crtc_on(xf86CrtcPtr crtc)
{
	ScrnInfoPtr scrn = crtc->scrn;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	int i, active_outputs = 0;

	if (!crtc->enabled)
		return FALSE;

	/* Kernel manages CRTC status based out output config */
	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		if (output->crtc == crtc &&
		    intel_output_dpms_status(output) == DPMSModeOn)
			active_outputs++;
	}

	if (active_outputs)
		return TRUE;
	return FALSE;
}

static void
intel_flush_callback(CallbackListPtr *list,
		     pointer user_data, pointer call_data)
{
	ScrnInfoPtr scrn = user_data;
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (scrn->vtSema) {
		/* Emit a flush of the rendering cache, or on the 965
		 * and beyond rendering results may not hit the
		 * framebuffer until significantly later.
		 */
		intel_batch_submit(scrn,
				   intel->need_mi_flush ||
				   !list_is_empty(&intel->flush_pixmaps));
	}
}

#if HAVE_UDEV
static void
I830HandleUEvents(int fd, void *closure)
{
    ScrnInfoPtr scrn = closure;
	intel_screen_private *intel = intel_get_screen_private(scrn);
    struct udev_device *dev;
    const char *hotplug;
    struct stat s;
    dev_t udev_devnum;

    dev = udev_monitor_receive_device(intel->uevent_monitor);
    if (!dev)
	return;

    udev_devnum = udev_device_get_devnum(dev);
    fstat(intel->drmSubFD, &s);
    /*
     * Check to make sure this event is directed at our
     * device (by comparing dev_t values), then make
     * sure it's a hotplug event (HOTPLUG=1)
     */

    hotplug = udev_device_get_property_value(dev, "HOTPLUG");

    if (memcmp(&s.st_rdev, &udev_devnum, sizeof (dev_t)) == 0 &&
	hotplug && atoi(hotplug) == 1)
	RRGetInfo(screenInfo.screens[scrn->scrnIndex], TRUE);

    udev_device_unref(dev);
}

static void
I830UeventInit(ScrnInfoPtr scrn)
{
    intel_screen_private *intel = intel_get_screen_private(scrn);
    struct udev *u;
    struct udev_monitor *mon;
    Bool hotplug;
    MessageType from = X_CONFIG;

    if (!xf86GetOptValBool(intel->Options, OPTION_HOTPLUG, &hotplug)) {
	from = X_DEFAULT;
	hotplug = TRUE;
    }

    xf86DrvMsg(scrn->scrnIndex, from, "hotplug detection: \"%s\"\n",
	       hotplug ? "enabled" : "disabled");
    if (!hotplug)
	return;

    u = udev_new();
    if (!u)
	return;

    mon = udev_monitor_new_from_netlink(u, "udev");

    if (!mon) {
	udev_unref(u);
	return;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(mon,
							"drm",
							"drm_minor") < 0 ||
	udev_monitor_enable_receiving(mon) < 0)
    {
	udev_monitor_unref(mon);
	udev_unref(u);
	return;
    }

    intel->uevent_handler =
	xf86AddGeneralHandler(udev_monitor_get_fd(mon),
			      I830HandleUEvents,
			      scrn);
    if (!intel->uevent_handler) {
	udev_monitor_unref(mon);
	udev_unref(u);
	return;
    }

    intel->uevent_monitor = mon;
}

static void
I830UeventFini(ScrnInfoPtr scrn)
{
    intel_screen_private *intel = intel_get_screen_private(scrn);

    if (intel->uevent_handler)
    {
	struct udev *u = udev_monitor_get_udev(intel->uevent_monitor);

	xf86RemoveGeneralHandler(intel->uevent_handler);

	udev_monitor_unref(intel->uevent_monitor);
	udev_unref(u);
	intel->uevent_handler = NULL;
	intel->uevent_monitor = NULL;
    }
}
#endif /* HAVE_UDEV */

static Bool
I830ScreenInit(int scrnIndex, ScreenPtr screen, int argc, char **argv)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	VisualPtr visual;
#ifdef INTEL_XVMC
	MessageType from;
#endif
	struct pci_device *const device = intel->PciInfo;
	int fb_bar = IS_GEN2(intel) ? 0 : 2;

	/*
	 * The "VideoRam" config file parameter specifies the maximum amount of
	 * memory that will be used/allocated.  When not present, we allow the
	 * driver to allocate as much memory as it wishes to satisfy its
	 * allocations, but if agpgart support isn't available, it gets limited
	 * to the amount of pre-allocated ("stolen") memory.
	 *
	 * Note that in using this value for allocator initialization, we're
	 * limiting aperture allocation to the VideoRam option, rather than limiting
	 * actual memory allocation, so alignment and things will cause less than
	 * VideoRam to be actually used.
	 */
	scrn->videoRam = intel->FbMapSize / KB(1);
	if (intel->pEnt->device->videoRam != 0) {
		if (scrn->videoRam != intel->pEnt->device->videoRam) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "VideoRam configuration found, which is no "
				   "longer used.\n");
			xf86DrvMsg(scrn->scrnIndex, X_INFO,
				   "Continuing with (ignored) %dkB VideoRam "
				   "instead of %d kB.\n",
				   scrn->videoRam,
				   intel->pEnt->device->videoRam);
		}
	}

	scrn->videoRam = device->regions[fb_bar].size / 1024;

	intel->last_3d = LAST_3D_OTHER;
	intel->overlayOn = FALSE;

	/*
	 * Set this so that the overlay allocation is factored in when
	 * appropriate.
	 */
	intel->XvEnabled = TRUE;

	xf86DrvMsg(scrn->scrnIndex,
		   intel->pEnt->device->videoRam ? X_CONFIG : X_DEFAULT,
		   "VideoRam: %d KB\n", scrn->videoRam);

#ifdef DRI2
	if (intel->directRenderingType == DRI_NONE
	    && I830DRI2ScreenInit(screen))
		intel->directRenderingType = DRI_DRI2;
#endif

	if (!intel_init_initial_framebuffer(scrn))
		return FALSE;

	intel_fixup_mtrrs(scrn);

	intel_batch_init(scrn);

	if (INTEL_INFO(intel)->gen >= 40)
		gen4_render_state_init(scrn);

	miClearVisualTypes();
	if (!miSetVisualTypes(scrn->depth,
			      miGetDefaultVisualMask(scrn->depth),
			      scrn->rgbBits, scrn->defaultVisual))
		return FALSE;
	if (!miSetPixmapDepths())
		return FALSE;

	DPRINTF(PFX, "assert( if(!I830EnterVT(scrnIndex, 0)) )\n");

	DPRINTF(PFX, "assert( if(!fbScreenInit(screen, ...) )\n");
	if (!fbScreenInit(screen, NULL,
			  scrn->virtualX, scrn->virtualY,
			  scrn->xDpi, scrn->yDpi,
			  scrn->displayWidth, scrn->bitsPerPixel))
		return FALSE;

	if (scrn->bitsPerPixel > 8) {
		/* Fixup RGB ordering */
		visual = screen->visuals + screen->numVisuals;
		while (--visual >= screen->visuals) {
			if ((visual->class | DynamicClass) == DirectColor) {
				visual->offsetRed = scrn->offset.red;
				visual->offsetGreen = scrn->offset.green;
				visual->offsetBlue = scrn->offset.blue;
				visual->redMask = scrn->mask.red;
				visual->greenMask = scrn->mask.green;
				visual->blueMask = scrn->mask.blue;
			}
		}
	}

	fbPictureInit(screen, NULL, 0);

	xf86SetBlackWhitePixels(screen);

	if (!intel_uxa_init(screen)) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Hardware acceleration initialization failed\n");
		return FALSE;
	}

	miInitializeBackingStore(screen);
	xf86SetBackingStore(screen);
	xf86SetSilkenMouse(screen);
	miDCInitialize(screen, xf86GetPointerScreenFuncs());

	xf86DrvMsg(scrn->scrnIndex, X_INFO, "Initializing HW Cursor\n");
	if (!xf86_cursors_init(screen, I810_CURSOR_X, I810_CURSOR_Y,
			       (HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
				HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
				HARDWARE_CURSOR_INVERT_MASK |
				HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
				HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
				HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64 |
				HARDWARE_CURSOR_UPDATE_UNHIDDEN |
				HARDWARE_CURSOR_ARGB))) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Hardware cursor initialization failed\n");
	}

	/* Must force it before EnterVT, so we are in control of VT and
	 * later memory should be bound when allocating, e.g rotate_mem */
	scrn->vtSema = TRUE;

	if (!I830EnterVT(scrnIndex, 0))
		return FALSE;

	intel->BlockHandler = screen->BlockHandler;
	screen->BlockHandler = I830BlockHandler;

	if (!AddCallback(&FlushCallback, intel_flush_callback, scrn))
		return FALSE;

	screen->SaveScreen = xf86SaveScreen;
	intel->CloseScreen = screen->CloseScreen;
	screen->CloseScreen = I830CloseScreen;
	intel->CreateScreenResources = screen->CreateScreenResources;
	screen->CreateScreenResources = i830CreateScreenResources;

	if (!xf86CrtcScreenInit(screen))
		return FALSE;

	DPRINTF(PFX, "assert( if(!miCreateDefColormap(screen)) )\n");
	if (!miCreateDefColormap(screen))
		return FALSE;

	DPRINTF(PFX, "assert( if(!xf86HandleColormaps(screen, ...)) )\n");
	if (!xf86HandleColormaps(screen, 256, 8, I830LoadPalette, NULL,
				 CMAP_RELOAD_ON_MODE_SWITCH |
				 CMAP_PALETTED_TRUECOLOR)) {
		return FALSE;
	}

	xf86DPMSInit(screen, xf86DPMSSet, 0);

#ifdef INTEL_XVMC
	if (INTEL_INFO(intel)->gen >= 40)
		intel->XvMCEnabled = TRUE;
	from = ((intel->directRenderingType == DRI_DRI2) &&
		xf86GetOptValBool(intel->Options, OPTION_XVMC,
				  &intel->XvMCEnabled) ? X_CONFIG : X_DEFAULT);
	xf86DrvMsg(scrn->scrnIndex, from, "Intel XvMC decoder %sabled\n",
		   intel->XvMCEnabled ? "en" : "dis");
#endif
	/* Init video */
	if (intel->XvEnabled)
		I830InitVideo(screen);

#if defined(DRI2)
	switch (intel->directRenderingType) {
	case DRI_DRI2:
		intel->directRenderingOpen = TRUE;
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "direct rendering: DRI2 Enabled\n");
		break;
	case DRI_DISABLED:
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "direct rendering: Disabled\n");
		break;
	case DRI_NONE:
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "direct rendering: Failed\n");
		break;
	}
#else
	xf86DrvMsg(scrn->scrnIndex, X_INFO,
		   "direct rendering: Not available\n");
#endif

	if (serverGeneration == 1)
		xf86ShowUnusedOptions(scrn->scrnIndex, scrn->options);

	intel_mode_init(intel);

	intel->suspended = FALSE;

#if HAVE_UDEV
	I830UeventInit(scrn);
#endif

	return uxa_resources_init(screen);
}

static void i830AdjustFrame(int scrnIndex, int x, int y, int flags)
{
}

static void I830FreeScreen(int scrnIndex, int flags)
{
	ScrnInfoPtr scrn = xf86Screens[scrnIndex];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	if (intel) {
		intel_mode_fini(intel);
		intel_close_drm_master(intel);
		intel_bufmgr_fini(intel);

		free(intel);
		scrn->driverPrivate = NULL;
	}

	if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
		vgaHWFreeHWRec(xf86Screens[scrnIndex]);
}

static void I830LeaveVT(int scrnIndex, int flags)
{
	ScrnInfoPtr scrn = xf86Screens[scrnIndex];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int ret;

	DPRINTF(PFX, "Leave VT\n");

	xf86RotateFreeShadow(scrn);

	xf86_hide_cursors(scrn);

	ret = drmDropMaster(intel->drmSubFD);
	if (ret)
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "drmDropMaster failed: %s\n", strerror(errno));
}

/*
 * This gets called when gaining control of the VT, and from ScreenInit().
 */
static Bool I830EnterVT(int scrnIndex, int flags)
{
	ScrnInfoPtr scrn = xf86Screens[scrnIndex];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int ret;

	DPRINTF(PFX, "Enter VT\n");

	ret = drmSetMaster(intel->drmSubFD);
	if (ret) {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "drmSetMaster failed: %s\n",
			   strerror(errno));
	}

	intel_set_gem_max_sizes(scrn);

	if (!xf86SetDesiredModes(scrn))
		return FALSE;

	return TRUE;
}

static Bool I830SwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
	ScrnInfoPtr scrn = xf86Screens[scrnIndex];

	return xf86SetSingleMode(scrn, mode, RR_Rotate_0);
}

static Bool I830CloseScreen(int scrnIndex, ScreenPtr screen)
{
	ScrnInfoPtr scrn = xf86Screens[scrnIndex];
	intel_screen_private *intel = intel_get_screen_private(scrn);

#if HAVE_UDEV
	I830UeventFini(scrn);
#endif

	if (scrn->vtSema == TRUE) {
		I830LeaveVT(scrnIndex, 0);
	}

	DeleteCallback(&FlushCallback, intel_flush_callback, scrn);

	if (intel->uxa_driver) {
		uxa_driver_fini(screen);
		free(intel->uxa_driver);
		intel->uxa_driver = NULL;
	}

	if (intel->front_buffer) {
		if (!intel->use_shadow)
			intel_set_pixmap_bo(screen->GetScreenPixmap(screen),
					    NULL);
		intel_mode_remove_fb(intel);
		drm_intel_bo_unreference(intel->front_buffer);
		intel->front_buffer = NULL;
	}

	if (intel->shadow_buffer) {
		free(intel->shadow_buffer);
		intel->shadow_buffer = NULL;
	}

	if (intel->shadow_damage) {
		DamageUnregister(&screen->GetScreenPixmap(screen)->drawable,
				 intel->shadow_damage);
		DamageDestroy(intel->shadow_damage);
		intel->shadow_damage = NULL;
	}

	intel_batch_teardown(scrn);

	if (INTEL_INFO(intel)->gen >= 40)
		gen4_render_state_cleanup(scrn);

	xf86_cursors_fini(screen);

	i965_free_video(scrn);

	screen->CloseScreen = intel->CloseScreen;
	(*screen->CloseScreen) (scrnIndex, screen);

	if (intel->directRenderingOpen
	    && intel->directRenderingType == DRI_DRI2) {
		intel->directRenderingOpen = FALSE;
		I830DRI2CloseScreen(screen);
	}

	xf86GARTCloseScreen(scrnIndex);

	scrn->vtSema = FALSE;
	return TRUE;
}

static ModeStatus
I830ValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
	if (mode->Flags & V_INTERLACE) {
		if (verbose) {
			xf86DrvMsg(scrnIndex, X_PROBED,
				   "Removing interlaced mode \"%s\"\n",
				   mode->name);
		}
		return MODE_BAD;
	}
	return MODE_OK;
}

#ifndef SUSPEND_SLEEP
#define SUSPEND_SLEEP 0
#endif
#ifndef RESUME_SLEEP
#define RESUME_SLEEP 0
#endif

/*
 * This function is only required if we need to do anything differently from
 * DoApmEvent() in common/xf86PM.c, including if we want to see events other
 * than suspend/resume.
 */
static Bool I830PMEvent(int scrnIndex, pmEvent event, Bool undo)
{
	ScrnInfoPtr scrn = xf86Screens[scrnIndex];
	intel_screen_private *intel = intel_get_screen_private(scrn);

	DPRINTF(PFX, "Enter VT, event %d, undo: %s\n", event,
		BOOLTOSTRING(undo));

	switch (event) {
	case XF86_APM_SYS_SUSPEND:
	case XF86_APM_CRITICAL_SUSPEND:	/*do we want to delay a critical suspend? */
	case XF86_APM_USER_SUSPEND:
	case XF86_APM_SYS_STANDBY:
	case XF86_APM_USER_STANDBY:
		if (!undo && !intel->suspended) {
			scrn->LeaveVT(scrnIndex, 0);
			intel->suspended = TRUE;
			sleep(SUSPEND_SLEEP);
		} else if (undo && intel->suspended) {
			sleep(RESUME_SLEEP);
			scrn->EnterVT(scrnIndex, 0);
			intel->suspended = FALSE;
		}
		break;
	case XF86_APM_STANDBY_RESUME:
	case XF86_APM_NORMAL_RESUME:
	case XF86_APM_CRITICAL_RESUME:
		if (intel->suspended) {
			sleep(RESUME_SLEEP);
			scrn->EnterVT(scrnIndex, 0);
			intel->suspended = FALSE;
			/*
			 * Turn the screen saver off when resuming.  This seems to be
			 * needed to stop xscreensaver kicking in (when used).
			 *
			 * XXX DoApmEvent() should probably call this just like
			 * xf86VTSwitch() does.  Maybe do it here only in 4.2
			 * compatibility mode.
			 */
			SaveScreens(SCREEN_SAVER_FORCER, ScreenSaverReset);
		}
		break;
		/* This is currently used for ACPI */
	case XF86_APM_CAPABILITY_CHANGED:
		ErrorF("I830PMEvent: Capability change\n");

		SaveScreens(SCREEN_SAVER_FORCER, ScreenSaverReset);

		break;
	default:
		ErrorF("I830PMEvent: received APM event %d\n", event);
	}
	return TRUE;
}

xf86CrtcPtr intel_pipe_to_crtc(ScrnInfoPtr scrn, int pipe)
{
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(scrn);
	int c;

	for (c = 0; c < config->num_crtc; c++) {
		xf86CrtcPtr crtc = config->crtc[c];
		I830CrtcPrivatePtr intel_crtc = crtc->driver_private;

		if (intel_crtc->pipe == pipe)
			return crtc;
	}

	return NULL;
}

void intel_init_scrn(ScrnInfoPtr scrn)
{
	scrn->PreInit = I830PreInit;
	scrn->ScreenInit = I830ScreenInit;
	scrn->SwitchMode = I830SwitchMode;
	scrn->AdjustFrame = i830AdjustFrame;
	scrn->EnterVT = I830EnterVT;
	scrn->LeaveVT = I830LeaveVT;
	scrn->FreeScreen = I830FreeScreen;
	scrn->ValidMode = I830ValidMode;
	scrn->PMEvent = I830PMEvent;
}
