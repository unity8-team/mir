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
#include "i830_display.h"
#include "i830_bios.h"
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
   {-1,			NULL,		OPTV_NONE,	{0},	FALSE}
};
/* *INDENT-ON* */

static void i830AdjustFrame(int scrnIndex, int x, int y, int flags);
static Bool I830CloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool I830EnterVT(int scrnIndex, int flags);

/* temporary */
extern void xf86SetCursor(ScreenPtr pScreen, CursorPtr pCurs, int x, int y);

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

static Bool I830GetRec(ScrnInfoPtr pScrn)
{
	I830Ptr pI830;

	if (pScrn->driverPrivate)
		return TRUE;
	pI830 = pScrn->driverPrivate = xnfcalloc(sizeof(I830Rec), 1);
	return TRUE;
}

static void I830FreeRec(ScrnInfoPtr pScrn)
{
	I830Ptr pI830;

	if (!pScrn)
		return;
	if (!pScrn->driverPrivate)
		return;

	pI830 = I830PTR(pScrn);

	xfree(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
}

static void
I830LoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
		LOCO * colors, VisualPtr pVisual)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
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

		switch (pScrn->depth) {
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
static Bool i830CreateScreenResources(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	I830Ptr pI830 = I830PTR(pScrn);

	pScreen->CreateScreenResources = pI830->CreateScreenResources;
	if (!(*pScreen->CreateScreenResources) (pScreen))
		return FALSE;

	i830_uxa_create_screen_resources(pScreen);

	return TRUE;
}

static void PreInitCleanup(ScrnInfoPtr pScrn)
{
	I830FreeRec(pScrn);
}

/*
 * Adjust *width to allow for tiling if possible
 */
Bool i830_tiled_width(I830Ptr i830, int *width, int cpp)
{
	Bool tiled = FALSE;

	/*
	 * Adjust the display width to allow for front buffer tiling if possible
	 */
	if (i830->tiling) {
		if (IS_I965G(i830)) {
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

static Bool i830_xf86crtc_resize(ScrnInfoPtr scrn, int width, int height)
{
#ifdef DRI2
	I830Ptr i830 = I830PTR(scrn);
	int old_width = scrn->displayWidth;
#endif
	int old_x = scrn->virtualX;
	int old_y = scrn->virtualY;

	if (old_x == width && old_y == height)
		return TRUE;

	scrn->virtualX = width;
	scrn->virtualY = height;
#ifdef DRI2
	if (i830->front_buffer) {
		i830_memory *new_front, *old_front;
		Bool tiled;
		ScreenPtr screen = screenInfo.screens[scrn->scrnIndex];

		scrn->displayWidth = i830_pad_drawable_width(width, i830->cpp);
		tiled = i830_tiled_width(i830, &scrn->displayWidth, i830->cpp);
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "Allocate new frame buffer %dx%d stride %d\n", width,
			   height, scrn->displayWidth);
		I830Sync(scrn);
		i830WaitForVblank(scrn);
		new_front = i830_allocate_framebuffer(scrn);
		if (!new_front) {
			scrn->virtualX = old_x;
			scrn->virtualY = old_y;
			scrn->displayWidth = old_width;
			return FALSE;
		}
		old_front = i830->front_buffer;
		i830->front_buffer = new_front;
		i830_set_pixmap_bo(screen->GetScreenPixmap(screen),
				   new_front->bo);
		scrn->fbOffset = i830->front_buffer->offset;

		screen->ModifyPixmapHeader(screen->GetScreenPixmap(screen),
					   width, height, -1, -1,
					   scrn->displayWidth * i830->cpp,
					   NULL);

		/* ick. xf86EnableDisableFBAccess smashes the screen pixmap devPrivate,
		 * so update the value it uses
		 */
		scrn->pixmapPrivate.ptr = NULL;
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "New front buffer at 0x%lx\n",
			   i830->front_buffer->offset);
		i830_set_new_crtc_bo(scrn);
		I830Sync(scrn);
		i830WaitForVblank(scrn);
		i830_free_memory(scrn, old_front);
	}
#endif
	return TRUE;
}

static const xf86CrtcConfigFuncsRec i830_xf86crtc_config_funcs = {
	i830_xf86crtc_resize
};

/*
 * DRM mode setting Linux only at this point... later on we could
 * add a wrapper here.
 */
static Bool i830_kernel_mode_enabled(ScrnInfoPtr pScrn)
{
	struct pci_device *PciInfo;
	EntityInfoPtr pEnt;
	char *busIdString;
	int ret;

	pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
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

static void i830_detect_chipset(ScrnInfoPtr pScrn)
{
	I830Ptr pI830 = I830PTR(pScrn);
	MessageType from = X_PROBED;
	const char *chipname;
	uint32_t capid;

	switch (DEVICE_ID(pI830->PciInfo)) {
	case PCI_CHIP_I830_M:
		chipname = "830M";
		break;
	case PCI_CHIP_845_G:
		chipname = "845G";
		break;
	case PCI_CHIP_I855_GM:
		/* Check capid register to find the chipset variant */
		pci_device_cfg_read_u32(pI830->PciInfo, &capid, I85X_CAPID);
		pI830->variant =
		    (capid >> I85X_VARIANT_SHIFT) & I85X_VARIANT_MASK;
		switch (pI830->variant) {
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
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				   "Unknown 852GM/855GM variant: 0x%x)\n",
				   pI830->variant);
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
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Integrated Graphics Chipset: Intel(R) %s\n", chipname);

	/* Set the Chipset and ChipRev, allowing config file entries to override. */
	if (pI830->pEnt->device->chipset && *pI830->pEnt->device->chipset) {
		pScrn->chipset = pI830->pEnt->device->chipset;
		from = X_CONFIG;
	} else if (pI830->pEnt->device->chipID >= 0) {
		pScrn->chipset = (char *)xf86TokenToString(I830Chipsets,
							   pI830->pEnt->device->
							   chipID);
		from = X_CONFIG;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "ChipID override: 0x%04X\n",
			   pI830->pEnt->device->chipID);
		DEVICE_ID(pI830->PciInfo) = pI830->pEnt->device->chipID;
	} else {
		from = X_PROBED;
		pScrn->chipset = (char *)xf86TokenToString(I830Chipsets,
							   DEVICE_ID(pI830->
								     PciInfo));
	}

	if (pI830->pEnt->device->chipRev >= 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
			   pI830->pEnt->device->chipRev);
	}

	xf86DrvMsg(pScrn->scrnIndex, from, "Chipset: \"%s\"\n",
		   (pScrn->chipset != NULL) ? pScrn->chipset : "Unknown i8xx");
}

static Bool I830GetEarlyOptions(ScrnInfoPtr pScrn)
{
	I830Ptr pI830 = I830PTR(pScrn);

	/* Process the options */
	xf86CollectOptions(pScrn, NULL);
	if (!(pI830->Options = xalloc(sizeof(I830Options))))
		return FALSE;
	memcpy(pI830->Options, I830Options, sizeof(I830Options));
	xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pI830->Options);

	pI830->fallback_debug = xf86ReturnOptValBool(pI830->Options,
						     OPTION_FALLBACKDEBUG,
						     FALSE);

	return TRUE;
}

static void i830_check_dri_option(ScrnInfoPtr pScrn)
{
	I830Ptr pI830 = I830PTR(pScrn);
	pI830->directRenderingType = DRI_NONE;
	if (!xf86ReturnOptValBool(pI830->Options, OPTION_DRI, TRUE))
		pI830->directRenderingType = DRI_DISABLED;

	if (pScrn->depth != 16 && pScrn->depth != 24) {
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "DRI is disabled because it "
			   "runs only at depths 16 and 24.\n");
		pI830->directRenderingType = DRI_DISABLED;
	}
}

static Bool i830_open_drm_master(ScrnInfoPtr scrn)
{
	I830Ptr i830 = I830PTR(scrn);
	struct pci_device *dev = i830->PciInfo;
	char *busid;
	drmSetVersion sv;
	struct drm_i915_getparam gp;
	int err, has_gem;

	/* We wish we had asprintf, but all we get is XNFprintf. */
	busid = XNFprintf("pci:%04x:%02x:%02x.%d",
			  dev->domain, dev->bus, dev->dev, dev->func);

	i830->drmSubFD = drmOpen("i915", busid);
	if (i830->drmSubFD == -1) {
		xfree(busid);
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "[drm] Failed to open DRM device for %s: %s\n",
			   busid, strerror(errno));
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
	err = drmSetInterfaceVersion(i830->drmSubFD, &sv);
	if (err != 0) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "[drm] failed to set drm interface version.\n");
		drmClose(i830->drmSubFD);
		i830->drmSubFD = -1;
		return FALSE;
	}

	has_gem = FALSE;
	gp.param = I915_PARAM_HAS_GEM;
	gp.value = &has_gem;
	(void)drmCommandWriteRead(i830->drmSubFD, DRM_I915_GETPARAM,
				  &gp, sizeof(gp));
	if (!has_gem) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "[drm] Failed to detect GEM.  Kernel 2.6.28 required.\n");
		drmClose(i830->drmSubFD);
		i830->drmSubFD = -1;
		return FALSE;
	}

	return TRUE;
}

static void i830_close_drm_master(ScrnInfoPtr scrn)
{
	I830Ptr i830 = I830PTR(scrn);
	if (i830 && i830->drmSubFD > 0) {
		drmClose(i830->drmSubFD);
		i830->drmSubFD = -1;
	}
}

static Bool I830DrmModeInit(ScrnInfoPtr pScrn)
{
	I830Ptr pI830 = I830PTR(pScrn);

	if (drmmode_pre_init(pScrn, pI830->drmSubFD, pI830->cpp) == FALSE) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Kernel modesetting setup failed\n");
		PreInitCleanup(pScrn);
		return FALSE;
	}

	i830_init_bufmgr(pScrn);

	return TRUE;
}

static void I830XvInit(ScrnInfoPtr pScrn)
{
	I830Ptr pI830 = I830PTR(pScrn);
	MessageType from = X_PROBED;

	pI830->XvPreferOverlay =
	    xf86ReturnOptValBool(pI830->Options, OPTION_PREFER_OVERLAY, FALSE);

	if (xf86GetOptValInteger(pI830->Options, OPTION_VIDEO_KEY,
				 &(pI830->colorKey))) {
		from = X_CONFIG;
	} else if (xf86GetOptValInteger(pI830->Options, OPTION_COLOR_KEY,
					&(pI830->colorKey))) {
		from = X_CONFIG;
	} else {
		pI830->colorKey =
		    (1 << pScrn->offset.red) | (1 << pScrn->offset.green) |
		    (((pScrn->mask.blue >> pScrn->offset.blue) - 1) <<
		     pScrn->offset.blue);
		from = X_DEFAULT;
	}
	xf86DrvMsg(pScrn->scrnIndex, from, "video overlay key set to 0x%x\n",
		   pI830->colorKey);
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
static Bool I830PreInit(ScrnInfoPtr pScrn, int flags)
{
	I830Ptr pI830;
	rgb defaultWeight = { 0, 0, 0 };
	EntityInfoPtr pEnt;
	int flags24;
	Gamma zeros = { 0.0, 0.0, 0.0 };
	int drm_mode_setting;

	if (pScrn->numEntities != 1)
		return FALSE;

	drm_mode_setting = i830_kernel_mode_enabled(pScrn);
	if (!drm_mode_setting) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No kernel modesetting driver detected.\n");
		return FALSE;
	}

	pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

	if (flags & PROBE_DETECT)
		return TRUE;

	/* Allocate driverPrivate */
	if (!I830GetRec(pScrn))
		return FALSE;

	pI830 = I830PTR(pScrn);
	pI830->pEnt = pEnt;

	pScrn->displayWidth = 640;	/* default it */

	if (pI830->pEnt->location.type != BUS_PCI)
		return FALSE;

	pI830->PciInfo = xf86GetPciInfoForEntity(pI830->pEnt->index);

	if (!i830_open_drm_master(pScrn))
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Failed to become DRM master.\n");

	pScrn->monitor = pScrn->confScreen->monitor;
	pScrn->progClock = TRUE;
	pScrn->rgbBits = 8;

	flags24 = Support32bppFb | PreferConvert24to32 | SupportConvert24to32;

	if (!xf86SetDepthBpp(pScrn, 0, 0, 0, flags24))
		return FALSE;

	switch (pScrn->depth) {
	case 8:
	case 15:
	case 16:
	case 24:
		break;
	default:
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Given depth (%d) is not supported by I830 driver\n",
			   pScrn->depth);
		return FALSE;
	}
	xf86PrintDepthBpp(pScrn);

	if (!xf86SetWeight(pScrn, defaultWeight, defaultWeight))
		return FALSE;
	if (!xf86SetDefaultVisual(pScrn, -1))
		return FALSE;

	pI830->cpp = pScrn->bitsPerPixel / 8;

	if (!I830GetEarlyOptions(pScrn))
		return FALSE;

	i830_detect_chipset(pScrn);

	i830_check_dri_option(pScrn);

	I830XvInit(pScrn);

	if (!I830DrmModeInit(pScrn))
		return FALSE;

	if (!xf86SetGamma(pScrn, zeros)) {
		PreInitCleanup(pScrn);
		return FALSE;
	}

	if (pScrn->modes == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No modes.\n");
		PreInitCleanup(pScrn);
		return FALSE;
	}
	pScrn->currentMode = pScrn->modes;

	/* Set display resolution */
	xf86SetDpi(pScrn, 0, 0);

	/* Load the required sub modules */
	if (!xf86LoadSubModule(pScrn, "fb")) {
		PreInitCleanup(pScrn);
		return FALSE;
	}

	/* Load the dri2 module if requested. */
	if (xf86ReturnOptValBool(pI830->Options, OPTION_DRI, FALSE) &&
	    pI830->directRenderingType != DRI_DISABLED) {
		xf86LoadSubModule(pScrn, "dri2");
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
void IntelEmitInvarientState(ScrnInfoPtr pScrn)
{
	I830Ptr pI830 = I830PTR(pScrn);

	/* If we've emitted our state since the last clobber by another client,
	 * skip it.
	 */
	if (pI830->last_3d != LAST_3D_OTHER)
		return;

	if (!IS_I965G(pI830)) {
		if (IS_I9XX(pI830))
			I915EmitInvarientState(pScrn);
		else
			I830EmitInvarientState(pScrn);
	}
}

static void
I830BlockHandler(int i, pointer blockData, pointer pTimeout, pointer pReadmask)
{
	ScreenPtr pScreen = screenInfo.screens[i];
	ScrnInfoPtr pScrn = xf86Screens[i];
	I830Ptr pI830 = I830PTR(pScrn);

	pScreen->BlockHandler = pI830->BlockHandler;

	(*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);

	pI830->BlockHandler = pScreen->BlockHandler;
	pScreen->BlockHandler = I830BlockHandler;

	if (pScrn->vtSema) {
		Bool flushed = FALSE;
		/* Emit a flush of the rendering cache, or on the 965 and beyond
		 * rendering results may not hit the framebuffer until significantly
		 * later.
		 */
		if (pI830->need_mi_flush || pI830->batch_used) {
			flushed = TRUE;
			I830EmitFlush(pScrn);
		}

		/* Flush the batch, so that any rendering is executed in a timely
		 * fashion.
		 */
		intel_batch_flush(pScrn, flushed);
		drmCommandNone(pI830->drmSubFD, DRM_I915_GEM_THROTTLE);

		pI830->need_mi_flush = FALSE;
	}

	i830_uxa_block_handler(pScreen);

	I830VideoBlockHandler(i, blockData, pTimeout, pReadmask);
}

static void i830_fixup_mtrrs(ScrnInfoPtr pScrn)
{
#ifdef HAS_MTRR_SUPPORT
	I830Ptr pI830 = I830PTR(pScrn);
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
			if (gentry.base == pI830->LinearAddr &&
			    gentry.size < pI830->FbMapSize) {

				xf86DrvMsg(pScrn->scrnIndex, X_INFO,
					   "Removing bad MTRR range (base 0x%lx, size 0x%x)\n",
					   gentry.base, gentry.size);

				sentry.base = gentry.base;
				sentry.size = gentry.size;
				sentry.type = gentry.type;

				if (ioctl(fd, MTRRIOC_DEL_ENTRY, &sentry) == -1) {
					xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
						   "Failed to remove bad MTRR range\n");
				}
			}
		}
		close(fd);
	}
#endif
}

static Bool i830_try_memory_allocation(ScrnInfoPtr pScrn)
{
	I830Ptr pI830 = I830PTR(pScrn);
	Bool tiled = pI830->tiling;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Attempting memory allocation with %stiled buffers.\n",
		   tiled ? "" : "un");

	if (!i830_allocate_2d_memory(pScrn))
		goto failed;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%siled allocation successful.\n",
		   tiled ? "T" : "Unt");
	return TRUE;

failed:
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%siled allocation failed.\n",
		   tiled ? "T" : "Unt");
	return FALSE;
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
static Bool i830_memory_init(ScrnInfoPtr pScrn)
{
	I830Ptr pI830 = I830PTR(pScrn);
	int savedDisplayWidth = pScrn->displayWidth;
	Bool tiled = FALSE;

	tiled = i830_tiled_width(pI830, &pScrn->displayWidth, pI830->cpp);
	/* Set up our video memory allocator for the chosen videoRam */
	if (!i830_allocator_init(pScrn, pScrn->videoRam * KB(1))) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Couldn't initialize video memory allocator\n");
		PreInitCleanup(pScrn);
		return FALSE;
	}

	xf86DrvMsg(pScrn->scrnIndex,
		   pI830->pEnt->device->videoRam ? X_CONFIG : X_DEFAULT,
		   "VideoRam: %d KB\n", pScrn->videoRam);

	/* Tiled first if we got a good displayWidth */
	if (tiled) {
		if (i830_try_memory_allocation(pScrn))
			return TRUE;
		else {
			i830_reset_allocations(pScrn);
			pI830->tiling = FALSE;
		}
	}

	/* If tiling fails we have to disable FBC */
	pScrn->displayWidth = savedDisplayWidth;

	if (i830_try_memory_allocation(pScrn))
		return TRUE;

	return FALSE;
}

void i830_init_bufmgr(ScrnInfoPtr pScrn)
{
	I830Ptr pI830 = I830PTR(pScrn);
	int batch_size;

	if (pI830->bufmgr)
		return;

	batch_size = 4096 * 4;

	/* The 865 has issues with larger-than-page-sized batch buffers. */
	if (IS_I865G(pI830))
		batch_size = 4096;

	pI830->bufmgr = intel_bufmgr_gem_init(pI830->drmSubFD, batch_size);
	intel_bufmgr_gem_enable_reuse(pI830->bufmgr);
}

Bool i830_crtc_on(xf86CrtcPtr crtc)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
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
	ScrnInfoPtr pScrn = crtc->scrn;
	I830Ptr pI830 = I830PTR(pScrn);

	return drmmode_get_pipe_from_crtc_id(pI830->bufmgr, crtc);
}

static Bool
I830ScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];;
	I830Ptr pI830 = I830PTR(pScrn);;
	VisualPtr visual;
	MessageType from;
	struct pci_device *const device = pI830->PciInfo;
	int fb_bar = IS_I9XX(pI830) ? 2 : 0;

	pScrn->displayWidth =
	    i830_pad_drawable_width(pScrn->virtualX, pI830->cpp);

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
	if (pI830->pEnt->device->videoRam == 0) {
		from = X_DEFAULT;
		pScrn->videoRam = pI830->FbMapSize / KB(1);
	} else {
#if 0
		from = X_CONFIG;
		pScrn->videoRam = pI830->pEnt->device->videoRam;
#else
		/* Disable VideoRam configuration, at least for now.  Previously,
		 * VideoRam was necessary to avoid overly low limits on allocated
		 * memory, so users created larger, yet still small, fixed allocation
		 * limits in their config files.  Now, the driver wants to allocate more,
		 * and the old intention of the VideoRam lines that had been entered is
		 * obsolete.
		 */
		from = X_DEFAULT;
		pScrn->videoRam = pI830->FbMapSize / KB(1);

		if (pScrn->videoRam != pI830->pEnt->device->videoRam) {
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
				   "VideoRam configuration found, which is no longer "
				   "recommended.\n");
			xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				   "Continuing with default %dkB VideoRam instead of %d "
				   "kB.\n",
				   pScrn->videoRam,
				   pI830->pEnt->device->videoRam);
		}
#endif
	}

	pScrn->videoRam = device->regions[fb_bar].size / 1024;

#ifdef DRI2
	if (pI830->directRenderingType == DRI_NONE
	    && I830DRI2ScreenInit(pScreen))
		pI830->directRenderingType = DRI_DRI2;
#endif

	/* Enable tiling by default */
	pI830->tiling = TRUE;

	/* Allow user override if they set a value */
	if (xf86IsOptionSet(pI830->Options, OPTION_TILING)) {
		if (xf86ReturnOptValBool(pI830->Options, OPTION_TILING, FALSE))
			pI830->tiling = TRUE;
		else
			pI830->tiling = FALSE;
	}

	/* SwapBuffers delays to avoid tearing */
	pI830->swapbuffers_wait = TRUE;

	/* Allow user override if they set a value */
	if (xf86IsOptionSet(pI830->Options, OPTION_SWAPBUFFERS_WAIT)) {
		if (xf86ReturnOptValBool
		    (pI830->Options, OPTION_SWAPBUFFERS_WAIT, FALSE))
			pI830->swapbuffers_wait = TRUE;
		else
			pI830->swapbuffers_wait = FALSE;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Tiling %sabled\n",
		   pI830->tiling ? "en" : "dis");
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "SwapBuffers wait %sabled\n",
		   pI830->swapbuffers_wait ? "en" : "dis");

	pI830->last_3d = LAST_3D_OTHER;
	pI830->overlayOn = FALSE;

	/*
	 * Set this so that the overlay allocation is factored in when
	 * appropriate.
	 */
	pI830->XvEnabled = TRUE;

	if (!i830_memory_init(pScrn)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Couldn't allocate video memory\n");
		return FALSE;
	}

	i830_fixup_mtrrs(pScrn);

	miClearVisualTypes();
	if (!miSetVisualTypes(pScrn->depth,
			      miGetDefaultVisualMask(pScrn->depth),
			      pScrn->rgbBits, pScrn->defaultVisual))
		return FALSE;
	if (!miSetPixmapDepths())
		return FALSE;

	DPRINTF(PFX, "assert( if(!I830EnterVT(scrnIndex, 0)) )\n");

	if (pScrn->virtualX > pScrn->displayWidth)
		pScrn->displayWidth = pScrn->virtualX;

	/* If the front buffer is not a BO, we need to
	 * set the initial framebuffer pixmap to point at
	 * it
	 */
	pScrn->fbOffset = pI830->front_buffer->offset;

	DPRINTF(PFX, "assert( if(!fbScreenInit(pScreen, ...) )\n");
	if (!fbScreenInit(pScreen, NULL,
			  pScrn->virtualX, pScrn->virtualY,
			  pScrn->xDpi, pScrn->yDpi,
			  pScrn->displayWidth, pScrn->bitsPerPixel))
		return FALSE;

	if (pScrn->bitsPerPixel > 8) {
		/* Fixup RGB ordering */
		visual = pScreen->visuals + pScreen->numVisuals;
		while (--visual >= pScreen->visuals) {
			if ((visual->class | DynamicClass) == DirectColor) {
				visual->offsetRed = pScrn->offset.red;
				visual->offsetGreen = pScrn->offset.green;
				visual->offsetBlue = pScrn->offset.blue;
				visual->redMask = pScrn->mask.red;
				visual->greenMask = pScrn->mask.green;
				visual->blueMask = pScrn->mask.blue;
			}
		}
	}

	fbPictureInit(pScreen, NULL, 0);

	xf86SetBlackWhitePixels(pScreen);

	if (!I830AccelInit(pScreen)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Hardware acceleration initialization failed\n");
		return FALSE;
	}

	if (IS_I965G(pI830))
		pI830->batch_flush_notify = i965_batch_flush_notify;
	else if (IS_I9XX(pI830))
		pI830->batch_flush_notify = i915_batch_flush_notify;
	else
		pI830->batch_flush_notify = i830_batch_flush_notify;

	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);
	xf86SetSilkenMouse(pScreen);
	miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Initializing HW Cursor\n");

	if (!xf86_cursors_init(pScreen, I810_CURSOR_X, I810_CURSOR_Y,
			       (HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
				HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
				HARDWARE_CURSOR_INVERT_MASK |
				HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
				HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
				HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64 |
				HARDWARE_CURSOR_UPDATE_UNHIDDEN |
				HARDWARE_CURSOR_ARGB))) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Hardware cursor initialization failed\n");
	}

	/* Must force it before EnterVT, so we are in control of VT and
	 * later memory should be bound when allocating, e.g rotate_mem */
	pScrn->vtSema = TRUE;

	if (!I830EnterVT(scrnIndex, 0))
		return FALSE;

	pI830->BlockHandler = pScreen->BlockHandler;
	pScreen->BlockHandler = I830BlockHandler;

	pScreen->SaveScreen = xf86SaveScreen;
	pI830->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = I830CloseScreen;
	pI830->CreateScreenResources = pScreen->CreateScreenResources;
	pScreen->CreateScreenResources = i830CreateScreenResources;

	if (!xf86CrtcScreenInit(pScreen))
		return FALSE;

	DPRINTF(PFX, "assert( if(!miCreateDefColormap(pScreen)) )\n");
	if (!miCreateDefColormap(pScreen))
		return FALSE;

	DPRINTF(PFX, "assert( if(!xf86HandleColormaps(pScreen, ...)) )\n");
	if (!xf86HandleColormaps(pScreen, 256, 8, I830LoadPalette, NULL,
				 CMAP_RELOAD_ON_MODE_SWITCH |
				 CMAP_PALETTED_TRUECOLOR)) {
		return FALSE;
	}

	xf86DPMSInit(pScreen, xf86DPMSSet, 0);

#ifdef INTEL_XVMC
	pI830->XvMCEnabled = FALSE;
	from = ((pI830->directRenderingType == DRI_DRI2) &&
		xf86GetOptValBool(pI830->Options, OPTION_XVMC,
				  &pI830->XvMCEnabled) ? X_CONFIG : X_DEFAULT);
	xf86DrvMsg(pScrn->scrnIndex, from, "Intel XvMC decoder %sabled\n",
		   pI830->XvMCEnabled ? "en" : "dis");
#endif
	/* Init video */
	if (pI830->XvEnabled)
		I830InitVideo(pScreen);

	/* Setup 3D engine, needed for rotation too */
	IntelEmitInvarientState(pScrn);

#if defined(DRI2)
	switch (pI830->directRenderingType) {
	case DRI_DRI2:
		pI830->directRenderingOpen = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "direct rendering: DRI2 Enabled\n");
		break;
	case DRI_DISABLED:
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "direct rendering: Disabled\n");
		break;
	case DRI_NONE:
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "direct rendering: Failed\n");
		break;
	}
#else
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "direct rendering: Not available\n");
#endif

	if (serverGeneration == 1)
		xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

	pI830->suspended = FALSE;

	return TRUE;
}

static void i830AdjustFrame(int scrnIndex, int x, int y, int flags)
{
}

static void I830FreeScreen(int scrnIndex, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
#ifdef INTEL_XVMC
	I830Ptr pI830 = I830PTR(pScrn);
	if (pI830 && pI830->XvMCEnabled)
		intel_xvmc_finish(xf86Screens[scrnIndex]);
#endif

	i830_close_drm_master(pScrn);

	I830FreeRec(xf86Screens[scrnIndex]);
	if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
		vgaHWFreeHWRec(xf86Screens[scrnIndex]);
}

static void I830LeaveVT(int scrnIndex, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	I830Ptr pI830 = I830PTR(pScrn);
	int ret;

	DPRINTF(PFX, "Leave VT\n");

	xf86RotateFreeShadow(pScrn);

	xf86_hide_cursors(pScrn);

	I830Sync(pScrn);

	intel_batch_teardown(pScrn);

	if (IS_I965G(pI830))
		gen4_render_state_cleanup(pScrn);

	ret = drmDropMaster(pI830->drmSubFD);
	if (ret)
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "drmDropMaster failed: %s\n", strerror(errno));
}

/*
 * This gets called when gaining control of the VT, and from ScreenInit().
 */
static Bool I830EnterVT(int scrnIndex, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	I830Ptr pI830 = I830PTR(pScrn);
	int ret;

	DPRINTF(PFX, "Enter VT\n");

	ret = drmSetMaster(pI830->drmSubFD);
	if (ret) {
		if (errno == EINVAL) {
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
				   "drmSetMaster failed: 2.6.29 or newer kernel required for "
				   "multi-server DRI\n");
		} else {
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
				   "drmSetMaster failed: %s\n",
				   strerror(errno));
		}
	}

	if (!i830_bind_all_memory(pScrn))
		return FALSE;

	i830_describe_allocations(pScrn, 1, "");

	intel_batch_init(pScrn);

	if (IS_I965G(pI830))
		gen4_render_state_init(pScrn);

	if (!xf86SetDesiredModes(pScrn))
		return FALSE;

	/* Mark 3D state as being clobbered and setup the basics */
	pI830->last_3d = LAST_3D_OTHER;
	IntelEmitInvarientState(pScrn);

	return TRUE;
}

static Bool I830SwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

	return xf86SetSingleMode(pScrn, mode, RR_Rotate_0);
}

static Bool I830CloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	I830Ptr pI830 = I830PTR(pScrn);

	if (pScrn->vtSema == TRUE) {
		I830LeaveVT(scrnIndex, 0);
	}

	if (pI830->uxa_driver) {
		uxa_driver_fini(pScreen);
		xfree(pI830->uxa_driver);
		pI830->uxa_driver = NULL;
	}
	if (pI830->front_buffer) {
		i830_set_pixmap_bo(pScreen->GetScreenPixmap(pScreen), NULL);
		i830_free_memory(pScrn, pI830->front_buffer);
		pI830->front_buffer = NULL;
	}

	xf86_cursors_fini(pScreen);

	i830_allocator_fini(pScrn);

	i965_free_video(pScrn);

	pScreen->CloseScreen = pI830->CloseScreen;
	(*pScreen->CloseScreen) (scrnIndex, pScreen);

	if (pI830->directRenderingOpen
	    && pI830->directRenderingType == DRI_DRI2) {
		pI830->directRenderingOpen = FALSE;
		I830DRI2CloseScreen(pScreen);
	}

	xf86GARTCloseScreen(scrnIndex);

	pScrn->vtSema = FALSE;
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
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	I830Ptr pI830 = I830PTR(pScrn);

	DPRINTF(PFX, "Enter VT, event %d, undo: %s\n", event,
		BOOLTOSTRING(undo));

	switch (event) {
	case XF86_APM_SYS_SUSPEND:
	case XF86_APM_CRITICAL_SUSPEND:	/*do we want to delay a critical suspend? */
	case XF86_APM_USER_SUSPEND:
	case XF86_APM_SYS_STANDBY:
	case XF86_APM_USER_STANDBY:
		if (!undo && !pI830->suspended) {
			pScrn->LeaveVT(scrnIndex, 0);
			pI830->suspended = TRUE;
			sleep(SUSPEND_SLEEP);
		} else if (undo && pI830->suspended) {
			sleep(RESUME_SLEEP);
			pScrn->EnterVT(scrnIndex, 0);
			pI830->suspended = FALSE;
		}
		break;
	case XF86_APM_STANDBY_RESUME:
	case XF86_APM_NORMAL_RESUME:
	case XF86_APM_CRITICAL_RESUME:
		if (pI830->suspended) {
			sleep(RESUME_SLEEP);
			pScrn->EnterVT(scrnIndex, 0);
			pI830->suspended = FALSE;
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

xf86CrtcPtr i830_pipe_to_crtc(ScrnInfoPtr pScrn, int pipe)
{
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(pScrn);
	int c;

	for (c = 0; c < config->num_crtc; c++) {
		xf86CrtcPtr crtc = config->crtc[c];
		I830CrtcPrivatePtr intel_crtc = crtc->driver_private;

		if (intel_crtc->pipe == pipe)
			return crtc;
	}

	return NULL;
}

void I830InitpScrn(ScrnInfoPtr pScrn)
{
	pScrn->PreInit = I830PreInit;
	pScrn->ScreenInit = I830ScreenInit;
	pScrn->SwitchMode = I830SwitchMode;
	pScrn->AdjustFrame = i830AdjustFrame;
	pScrn->EnterVT = I830EnterVT;
	pScrn->LeaveVT = I830LeaveVT;
	pScrn->FreeScreen = I830FreeScreen;
	pScrn->ValidMode = I830ValidMode;
	pScrn->PMEvent = I830PMEvent;
}
