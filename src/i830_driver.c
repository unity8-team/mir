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
#include "i830.h"
#include "i830_video.h"
#if HAVE_SYS_MMAN_H && HAVE_MPROTECT
#include <sys/mman.h>
#endif

#ifdef INTEL_XVMC
#define _INTEL_XVMC_SERVER_
#include "i830_hwmc.h"
#endif

#include <sys/ioctl.h>
#include "i915_drm.h"
#include <xf86drmMode.h>

#define BIT(x) (1 << (x))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define NB_OF(x) (sizeof (x) / sizeof (*x))

/* *INDENT-OFF* */
static SymTabRec I830Chipsets[] = {
   {PCI_CHIP_I830_M,		"i830"},
   {PCI_CHIP_845_G,		"845G"},
   {PCI_CHIP_I855_GM,		"852GM/855GM"},
   {PCI_CHIP_I865_G,		"865G"},
   {PCI_CHIP_I915_G,		"915G"},
   {PCI_CHIP_E7221_G,		"E7221 (i915)"},
   {PCI_CHIP_I915_GM,		"915GM"},
   {PCI_CHIP_I945_G,		"945G"},
   {PCI_CHIP_I945_GM,		"945GM"},
   {PCI_CHIP_I945_GME,		"945GME"},
   {PCI_CHIP_IGD_GM,		"Pineview GM"},
   {PCI_CHIP_IGD_G,		"Pineview G"},
   {PCI_CHIP_I965_G,		"965G"},
   {PCI_CHIP_G35_G,		"G35"},
   {PCI_CHIP_I965_Q,		"965Q"},
   {PCI_CHIP_I946_GZ,		"946GZ"},
   {PCI_CHIP_I965_GM,		"965GM"},
   {PCI_CHIP_I965_GME,		"965GME/GLE"},
   {PCI_CHIP_G33_G,		"G33"},
   {PCI_CHIP_Q35_G,		"Q35"},
   {PCI_CHIP_Q33_G,		"Q33"},
   {PCI_CHIP_GM45_GM,		"GM45"},
   {PCI_CHIP_IGD_E_G,		"4 Series"},
   {PCI_CHIP_G45_G,		"G45/G43"},
   {PCI_CHIP_Q45_G,		"Q45/Q43"},
   {PCI_CHIP_G41_G,		"G41"},
   {PCI_CHIP_B43_G,		"B43"},
   {PCI_CHIP_IGDNG_D_G,		"Clarkdale"},
   {PCI_CHIP_IGDNG_M_G,		"Arrandale"},
   {-1,				NULL}
};

static PciChipsets I830PciChipsets[] = {
   {PCI_CHIP_I830_M,		PCI_CHIP_I830_M,	NULL},
   {PCI_CHIP_845_G,		PCI_CHIP_845_G,		NULL},
   {PCI_CHIP_I855_GM,		PCI_CHIP_I855_GM,	NULL},
   {PCI_CHIP_I865_G,		PCI_CHIP_I865_G,	NULL},
   {PCI_CHIP_I915_G,		PCI_CHIP_I915_G,	NULL},
   {PCI_CHIP_E7221_G,		PCI_CHIP_E7221_G,	NULL},
   {PCI_CHIP_I915_GM,		PCI_CHIP_I915_GM,	NULL},
   {PCI_CHIP_I945_G,		PCI_CHIP_I945_G,	NULL},
   {PCI_CHIP_I945_GM,		PCI_CHIP_I945_GM,	NULL},
   {PCI_CHIP_I945_GME,		PCI_CHIP_I945_GME,	NULL},
   {PCI_CHIP_IGD_GM,		PCI_CHIP_IGD_GM,	NULL},
   {PCI_CHIP_IGD_G,		PCI_CHIP_IGD_G,		NULL},
   {PCI_CHIP_I965_G,		PCI_CHIP_I965_G,	NULL},
   {PCI_CHIP_G35_G,		PCI_CHIP_G35_G,		NULL},
   {PCI_CHIP_I965_Q,		PCI_CHIP_I965_Q,	NULL},
   {PCI_CHIP_I946_GZ,		PCI_CHIP_I946_GZ,	NULL},
   {PCI_CHIP_I965_GM,		PCI_CHIP_I965_GM,	NULL},
   {PCI_CHIP_I965_GME,		PCI_CHIP_I965_GME,	NULL},
   {PCI_CHIP_G33_G,		PCI_CHIP_G33_G,		NULL},
   {PCI_CHIP_Q35_G,		PCI_CHIP_Q35_G,		NULL},
   {PCI_CHIP_Q33_G,		PCI_CHIP_Q33_G,		NULL},
   {PCI_CHIP_GM45_GM,		PCI_CHIP_GM45_GM,	NULL},
   {PCI_CHIP_IGD_E_G,		PCI_CHIP_IGD_E_G,	NULL},
   {PCI_CHIP_G45_G,		PCI_CHIP_G45_G,		NULL},
   {PCI_CHIP_Q45_G,		PCI_CHIP_Q45_G,		NULL},
   {PCI_CHIP_G41_G,		PCI_CHIP_G41_G,		NULL},
   {PCI_CHIP_B43_G,		PCI_CHIP_B43_G,		NULL},
   {PCI_CHIP_IGDNG_D_G,		PCI_CHIP_IGDNG_D_G,		NULL},
   {PCI_CHIP_IGDNG_M_G,		PCI_CHIP_IGDNG_M_G,		NULL},
   {-1,				-1,			NULL}
};

/*
 * Note: "ColorKey" is provided for compatibility with the i810 driver.
 * However, the correct option name is "VideoKey".  "ColorKey" usually
 * refers to the tranparency key for 8+24 overlays, not for video overlays.
 */

typedef enum {
   OPTION_DRI,
   OPTION_VIDEO_KEY,
   OPTION_COLOR_KEY,
   OPTION_FALLBACKDEBUG,
   OPTION_TILING,
   OPTION_SWAPBUFFERS_WAIT,
#ifdef INTEL_XVMC
   OPTION_XVMC,
#endif
   OPTION_PREFER_OVERLAY,
   OPTION_DEBUG_FLUSH_BATCHES,
   OPTION_DEBUG_FLUSH_CACHES,
   OPTION_DEBUG_WAIT,
} I830Opts;

static OptionInfoRec I830Options[] = {
   {OPTION_DRI,		"DRI",		OPTV_BOOLEAN,	{0},	TRUE},
   {OPTION_COLOR_KEY,	"ColorKey",	OPTV_INTEGER,	{0},	FALSE},
   {OPTION_VIDEO_KEY,	"VideoKey",	OPTV_INTEGER,	{0},	FALSE},
   {OPTION_FALLBACKDEBUG, "FallbackDebug", OPTV_BOOLEAN, {0},	FALSE},
   {OPTION_TILING,	"Tiling",	OPTV_BOOLEAN,	{0},	TRUE},
   {OPTION_SWAPBUFFERS_WAIT, "SwapbuffersWait", OPTV_BOOLEAN,	{0},	TRUE},
#ifdef INTEL_XVMC
   {OPTION_XVMC,	"XvMC",		OPTV_BOOLEAN,	{0},	TRUE},
#endif
   {OPTION_PREFER_OVERLAY, "XvPreferOverlay", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_DEBUG_FLUSH_BATCHES, "DebugFlushBatches", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_DEBUG_FLUSH_CACHES, "DebugFlushCaches", OPTV_BOOLEAN, {0}, FALSE},
   {OPTION_DEBUG_WAIT, "DebugWait", OPTV_BOOLEAN, {0}, FALSE},
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
const OptionInfoRec *I830AvailableOptions(int chipid, int busid)
{
	int i;

	for (i = 0; I830PciChipsets[i].PCIid > 0; i++) {
		if (chipid == I830PciChipsets[i].PCIid)
			return I830Options;
	}
	return NULL;
}

static Bool I830GetRec(ScrnInfoPtr scrn)
{
	if (scrn->driverPrivate)
		return TRUE;
	scrn->driverPrivate = xnfcalloc(sizeof(intel_screen_private), 1);

	return TRUE;
}

static void I830FreeRec(ScrnInfoPtr scrn)
{
	if (!scrn)
		return;
	if (!scrn->driverPrivate)
		return;

	xfree(scrn->driverPrivate);
	scrn->driverPrivate = NULL;
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

	i830_uxa_create_screen_resources(screen);

	return TRUE;
}

static void PreInitCleanup(ScrnInfoPtr scrn)
{
	I830FreeRec(scrn);
}

/*
 * Adjust *width to allow for tiling if possible
 */
Bool i830_tiled_width(intel_screen_private *intel, int *width, int cpp)
{
	Bool tiled = FALSE;

	/*
	 * Adjust the display width to allow for front buffer tiling if possible
	 */
	if (intel->tiling) {
		if (IS_I965G(intel)) {
			int tile_pixels = 512 / cpp;
			*width = (*width + tile_pixels - 1) &
			    ~(tile_pixels - 1);
			tiled = TRUE;
		} else {
			/* Good pitches to allow tiling.  Don't care about pitches < 1024
			 * pixels.
			 */
			static const int pitches[] = {
				1024,
				2048,
				4096,
				8192,
				0
			};
			int i;

			for (i = 0; pitches[i] != 0; i++) {
				if (pitches[i] >= *width) {
					*width = pitches[i];
					tiled = TRUE;
					break;
				}
			}
		}
	}
	return tiled;
}

/*
 * Pad to accelerator requirement
 */
int i830_pad_drawable_width(int width, int cpp)
{
	return (width + 63) & ~63;
}

/*
 * DRM mode setting Linux only at this point... later on we could
 * add a wrapper here.
 */
static Bool i830_kernel_mode_enabled(ScrnInfoPtr scrn)
{
	struct pci_device *PciInfo;
	EntityInfoPtr pEnt;
	char *busIdString;
	int ret;

	pEnt = xf86GetEntityInfo(scrn->entityList[0]);
	PciInfo = xf86GetPciInfoForEntity(pEnt->index);

	if (!xf86LoaderCheckSymbol("DRICreatePCIBusID"))
		return FALSE;

	busIdString = DRICreatePCIBusID(PciInfo);

	ret = drmCheckModesettingSupported(busIdString);
	if (ret) {
		if (xf86LoadKernelModule("i915"))
			ret = drmCheckModesettingSupported(busIdString);
	}
	/* Be nice to the user and load fbcon too */
	if (!ret)
		(void)xf86LoadKernelModule("fbcon");
	xfree(busIdString);
	if (ret)
		return FALSE;

	return TRUE;
}

static void i830_detect_chipset(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	MessageType from = X_PROBED;
	const char *chipname;
	uint32_t capid;

	switch (DEVICE_ID(intel->PciInfo)) {
	case PCI_CHIP_I830_M:
		chipname = "830M";
		break;
	case PCI_CHIP_845_G:
		chipname = "845G";
		break;
	case PCI_CHIP_I855_GM:
		/* Check capid register to find the chipset variant */
		pci_device_cfg_read_u32(intel->PciInfo, &capid, I85X_CAPID);
		intel->variant =
		    (capid >> I85X_VARIANT_SHIFT) & I85X_VARIANT_MASK;
		switch (intel->variant) {
		case I855_GM:
			chipname = "855GM";
			break;
		case I855_GME:
			chipname = "855GME";
			break;
		case I852_GM:
			chipname = "852GM";
			break;
		case I852_GME:
			chipname = "852GME";
			break;
		default:
			xf86DrvMsg(scrn->scrnIndex, X_INFO,
				   "Unknown 852GM/855GM variant: 0x%x)\n",
				   intel->variant);
			chipname = "852GM/855GM (unknown variant)";
			break;
		}
		break;
	case PCI_CHIP_I865_G:
		chipname = "865G";
		break;
	case PCI_CHIP_I915_G:
		chipname = "915G";
		break;
	case PCI_CHIP_E7221_G:
		chipname = "E7221 (i915)";
		break;
	case PCI_CHIP_I915_GM:
		chipname = "915GM";
		break;
	case PCI_CHIP_I945_G:
		chipname = "945G";
		break;
	case PCI_CHIP_I945_GM:
		chipname = "945GM";
		break;
	case PCI_CHIP_I945_GME:
		chipname = "945GME";
		break;
	case PCI_CHIP_IGD_GM:
		chipname = "Pineview GM";
		break;
	case PCI_CHIP_IGD_G:
		chipname = "Pineview G";
		break;
	case PCI_CHIP_I965_G:
		chipname = "965G";
		break;
	case PCI_CHIP_G35_G:
		chipname = "G35";
		break;
	case PCI_CHIP_I965_Q:
		chipname = "965Q";
		break;
	case PCI_CHIP_I946_GZ:
		chipname = "946GZ";
		break;
	case PCI_CHIP_I965_GM:
		chipname = "965GM";
		break;
	case PCI_CHIP_I965_GME:
		chipname = "965GME/GLE";
		break;
	case PCI_CHIP_G33_G:
		chipname = "G33";
		break;
	case PCI_CHIP_Q35_G:
		chipname = "Q35";
		break;
	case PCI_CHIP_Q33_G:
		chipname = "Q33";
		break;
	case PCI_CHIP_GM45_GM:
		chipname = "GM45";
		break;
	case PCI_CHIP_IGD_E_G:
		chipname = "4 Series";
		break;
	case PCI_CHIP_G45_G:
		chipname = "G45/G43";
		break;
	case PCI_CHIP_Q45_G:
		chipname = "Q45/Q43";
		break;
	case PCI_CHIP_G41_G:
		chipname = "G41";
		break;
	case PCI_CHIP_B43_G:
		chipname = "B43";
		break;
	case PCI_CHIP_IGDNG_D_G:
		chipname = "Clarkdale";
		break;
	case PCI_CHIP_IGDNG_M_G:
		chipname = "Arrandale";
		break;
	default:
		chipname = "unknown chipset";
		break;
	}
	xf86DrvMsg(scrn->scrnIndex, X_INFO,
		   "Integrated Graphics Chipset: Intel(R) %s\n", chipname);

	/* Set the Chipset and ChipRev, allowing config file entries to override. */
	if (intel->pEnt->device->chipset && *intel->pEnt->device->chipset) {
		scrn->chipset = intel->pEnt->device->chipset;
		from = X_CONFIG;
	} else if (intel->pEnt->device->chipID >= 0) {
		scrn->chipset = (char *)xf86TokenToString(I830Chipsets,
							   intel->pEnt->device->
							   chipID);
		from = X_CONFIG;
		xf86DrvMsg(scrn->scrnIndex, X_CONFIG,
			   "ChipID override: 0x%04X\n",
			   intel->pEnt->device->chipID);
		DEVICE_ID(intel->PciInfo) = intel->pEnt->device->chipID;
	} else {
		from = X_PROBED;
		scrn->chipset = (char *)xf86TokenToString(I830Chipsets,
							   DEVICE_ID(intel->
								     PciInfo));
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
	if (!(intel->Options = xalloc(sizeof(I830Options))))
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

static void i830_check_dri_option(ScrnInfoPtr scrn)
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

static Bool i830_open_drm_master(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct pci_device *dev = intel->PciInfo;
	char *busid;
	drmSetVersion sv;
	struct drm_i915_getparam gp;
	int err, has_gem;

	/* We wish we had asprintf, but all we get is XNFprintf. */
	busid = XNFprintf("pci:%04x:%02x:%02x.%d",
			  dev->domain, dev->bus, dev->dev, dev->func);

	intel->drmSubFD = drmOpen("i915", busid);
	if (intel->drmSubFD == -1) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "[drm] Failed to open DRM device for %s: %s\n",
			   busid, strerror(errno));
		xfree(busid);
		return FALSE;
	}

	xfree(busid);

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

static void i830_close_drm_master(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	if (intel && intel->drmSubFD > 0) {
		drmClose(intel->drmSubFD);
		intel->drmSubFD = -1;
	}
}

static Bool I830DrmModeInit(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);

	i830_init_bufmgr(scrn);

	if (drmmode_pre_init(scrn, intel->drmSubFD, intel->cpp) == FALSE) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Kernel modesetting setup failed\n");
		PreInitCleanup(scrn);
		return FALSE;
	}

	return TRUE;
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
	int drm_mode_setting;

	if (scrn->numEntities != 1)
		return FALSE;

	drm_mode_setting = i830_kernel_mode_enabled(scrn);
	if (!drm_mode_setting) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "No kernel modesetting driver detected.\n");
		return FALSE;
	}

	pEnt = xf86GetEntityInfo(scrn->entityList[0]);

	if (flags & PROBE_DETECT)
		return TRUE;

	/* Allocate driverPrivate */
	if (!I830GetRec(scrn))
		return FALSE;

	intel = intel_get_screen_private(scrn);
	intel->pEnt = pEnt;

	scrn->displayWidth = 640;	/* default it */

	if (intel->pEnt->location.type != BUS_PCI)
		return FALSE;

	intel->PciInfo = xf86GetPciInfoForEntity(intel->pEnt->index);

	if (!i830_open_drm_master(scrn))
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

	i830_detect_chipset(scrn);

	i830_check_dri_option(scrn);

	I830XvInit(scrn);

	if (!I830DrmModeInit(scrn))
		return FALSE;

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

	if (!IS_I965G(intel)) {
		if (IS_I9XX(intel))
			I915EmitInvarientState(scrn);
		else
			I830EmitInvarientState(scrn);
	}
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

	if (scrn->vtSema) {
		/* Emit a flush of the rendering cache, or on the 965 and beyond
		 * rendering results may not hit the framebuffer until significantly
		 * later.
		 *
		 * XXX Under KMS this is only required because tfp does not have
		 * the appropriate synchronisation points, so that outstanding updates
		 * to the pixmap are flushed prior to use as a texture. The framebuffer
		 * should be handled by the kernel domain management...
		 */
		if (intel->need_mi_flush || !list_is_empty(&intel->flush_pixmaps))
			intel_batch_emit_flush(scrn);

		intel_batch_submit(scrn);
		drmCommandNone(intel->drmSubFD, DRM_I915_GEM_THROTTLE);
	}

	i830_uxa_block_handler(screen);

	I830VideoBlockHandler(i, blockData, pTimeout, pReadmask);
}

static void i830_fixup_mtrrs(ScrnInfoPtr scrn)
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

static Bool i830_try_memory_allocation(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	Bool tiled = intel->tiling;

	xf86DrvMsg(scrn->scrnIndex, X_INFO,
		   "Attempting memory allocation with %stiled buffers.\n",
		   tiled ? "" : "un");

	intel->front_buffer = i830_allocate_framebuffer(scrn);
	if (!intel->front_buffer) {
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "%siled allocation failed.\n",
			   tiled ? "T" : "Unt");
		return FALSE;
	}

	xf86DrvMsg(scrn->scrnIndex, X_INFO, "%siled allocation successful.\n",
		   tiled ? "T" : "Unt");
	return TRUE;
}

/*
 * Try to allocate memory in several ways:
 *  1) If direct rendering is enabled, try to allocate enough memory for tiled
 *     surfaces by rounding up the display width to a tileable one.
 *  2) If that fails or the allocations themselves fail, try again with untiled
 *     allocations (if this works DRI will stay enabled).
 *  3) And if all else fails, disable DRI and try just 2D allocations.
 *  4) Give up and fail ScreenInit.
 */
static Bool i830_memory_init(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int savedDisplayWidth = scrn->displayWidth;
	Bool tiled = FALSE;

	tiled = i830_tiled_width(intel, &scrn->displayWidth, intel->cpp);

	xf86DrvMsg(scrn->scrnIndex,
		   intel->pEnt->device->videoRam ? X_CONFIG : X_DEFAULT,
		   "VideoRam: %d KB\n", scrn->videoRam);

	/* Tiled first if we got a good displayWidth */
	if (tiled) {
		if (i830_try_memory_allocation(scrn))
			return TRUE;
		else {
			i830_reset_allocations(scrn);
			intel->tiling = FALSE;
		}
	}

	/* If tiling fails we have to disable FBC */
	scrn->displayWidth = savedDisplayWidth;

	if (i830_try_memory_allocation(scrn))
		return TRUE;

	return FALSE;
}

void i830_init_bufmgr(ScrnInfoPtr scrn)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	int batch_size;

	if (intel->bufmgr)
		return;

	batch_size = 4096 * 4;

	/* The 865 has issues with larger-than-page-sized batch buffers. */
	if (IS_I865G(intel))
		batch_size = 4096;

	intel->bufmgr = intel_bufmgr_gem_init(intel->drmSubFD, batch_size);
	intel_bufmgr_gem_enable_reuse(intel->bufmgr);

	list_init(&intel->batch_pixmaps);
	list_init(&intel->flush_pixmaps);
}

Bool i830_crtc_on(xf86CrtcPtr crtc)
{
	ScrnInfoPtr scrn = crtc->scrn;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	int i, active_outputs = 0;

	/* Kernel manages CRTC status based out output config */
	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		if (output->crtc == crtc &&
		    drmmode_output_dpms_status(output) == DPMSModeOn)
			active_outputs++;
	}

	if (active_outputs)
		return TRUE;
	return FALSE;
}

int i830_crtc_to_pipe(xf86CrtcPtr crtc)
{
	ScrnInfoPtr scrn = crtc->scrn;
	intel_screen_private *intel = intel_get_screen_private(scrn);

	return drmmode_get_pipe_from_crtc_id(intel->bufmgr, crtc);
}

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
	int fb_bar = IS_I9XX(intel) ? 2 : 0;

	scrn->displayWidth =
	    i830_pad_drawable_width(scrn->virtualX, intel->cpp);

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

#ifdef DRI2
	if (intel->directRenderingType == DRI_NONE
	    && I830DRI2ScreenInit(screen))
		intel->directRenderingType = DRI_DRI2;
#endif

	/* Enable tiling by default */
	intel->tiling = TRUE;

	/* Allow user override if they set a value */
	if (xf86IsOptionSet(intel->Options, OPTION_TILING)) {
		if (xf86ReturnOptValBool(intel->Options, OPTION_TILING, FALSE))
			intel->tiling = TRUE;
		else
			intel->tiling = FALSE;
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

	xf86DrvMsg(scrn->scrnIndex, X_CONFIG, "Tiling %sabled\n",
		   intel->tiling ? "en" : "dis");
	xf86DrvMsg(scrn->scrnIndex, X_CONFIG, "SwapBuffers wait %sabled\n",
		   intel->swapbuffers_wait ? "en" : "dis");

	intel->last_3d = LAST_3D_OTHER;
	intel->overlayOn = FALSE;

	/*
	 * Set this so that the overlay allocation is factored in when
	 * appropriate.
	 */
	intel->XvEnabled = TRUE;

	if (!i830_memory_init(scrn)) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Couldn't allocate video memory\n");
		return FALSE;
	}

	i830_fixup_mtrrs(scrn);

	intel_batch_init(scrn);

	if (IS_I965G(intel))
		gen4_render_state_init(scrn);

	miClearVisualTypes();
	if (!miSetVisualTypes(scrn->depth,
			      miGetDefaultVisualMask(scrn->depth),
			      scrn->rgbBits, scrn->defaultVisual))
		return FALSE;
	if (!miSetPixmapDepths())
		return FALSE;

	DPRINTF(PFX, "assert( if(!I830EnterVT(scrnIndex, 0)) )\n");

	if (scrn->virtualX > scrn->displayWidth)
		scrn->displayWidth = scrn->virtualX;

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

	if (!I830AccelInit(screen)) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Hardware acceleration initialization failed\n");
		return FALSE;
	}

	if (IS_I965G(intel))
		intel->batch_flush_notify = i965_batch_flush_notify;
	else if (IS_I9XX(intel))
		intel->batch_flush_notify = i915_batch_flush_notify;
	else
		intel->batch_flush_notify = i830_batch_flush_notify;

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
	if (IS_I965G(intel))
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

	intel->suspended = FALSE;

	return TRUE;
}

static void i830AdjustFrame(int scrnIndex, int x, int y, int flags)
{
}

static void I830FreeScreen(int scrnIndex, int flags)
{
	ScrnInfoPtr scrn = xf86Screens[scrnIndex];
#ifdef INTEL_XVMC
	intel_screen_private *intel = intel_get_screen_private(scrn);
	if (intel && intel->XvMCEnabled)
		intel_xvmc_finish(xf86Screens[scrnIndex]);
#endif

	i830_close_drm_master(scrn);

	I830FreeRec(xf86Screens[scrnIndex]);
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

	i830_set_gem_max_sizes(scrn);

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

	if (scrn->vtSema == TRUE) {
		I830LeaveVT(scrnIndex, 0);
	}

	if (intel->uxa_driver) {
		uxa_driver_fini(screen);
		xfree(intel->uxa_driver);
		intel->uxa_driver = NULL;
	}
	if (intel->front_buffer) {
		i830_set_pixmap_bo(screen->GetScreenPixmap(screen), NULL);
		drmmode_closefb(scrn);
		drm_intel_bo_unreference(intel->front_buffer);
		intel->front_buffer = NULL;
	}

	intel_batch_teardown(scrn);

	if (IS_I965G(intel))
		gen4_render_state_cleanup(scrn);

	xf86_cursors_fini(screen);

	/* Free most of the allocations */
	i830_reset_allocations(scrn);

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

xf86CrtcPtr i830_pipe_to_crtc(ScrnInfoPtr scrn, int pipe)
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
