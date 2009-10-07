/*
 * Copyright 1996-1997 David J. McKay
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>

#include "nv_include.h"

#include "xorg-server.h"
#include "xf86int10.h"
#include "xf86drm.h"
#ifdef XF86DRM_MODE
#include "xf86drmMode.h"
#endif

/*
 * Forward definitions for the functions that make up the driver.
 */
/* Mandatory functions */
static const OptionInfoRec * NVAvailableOptions(int chipid, int busid);
static void    NVIdentify(int flags);
static Bool    NVPreInit(ScrnInfoPtr pScrn, int flags);
static Bool    NVScreenInit(int Index, ScreenPtr pScreen, int argc,
                            char **argv);
static Bool    NVEnterVT(int scrnIndex, int flags);
static void    NVLeaveVT(int scrnIndex, int flags);
static Bool    NVCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool    NVSaveScreen(ScreenPtr pScreen, int mode);
static void    NVCloseDRM(ScrnInfoPtr);

/* Optional functions */
static Bool    NVSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);
static void    NVAdjustFrame(int scrnIndex, int x, int y, int flags);
static void    NVFreeScreen(int scrnIndex, int flags);

/* Internally used functions */

static Bool	NVMapMem(ScrnInfoPtr pScrn);
static Bool	NVUnmapMem(ScrnInfoPtr pScrn);
static void	NVSave(ScrnInfoPtr pScrn);
static void	NVRestore(ScrnInfoPtr pScrn);

#define NOUVEAU_PCI_DEVICE(_vendor_id, _device_id)                             \
	{ (_vendor_id), (_device_id), PCI_MATCH_ANY, PCI_MATCH_ANY,            \
	  0x00030000, 0x00ffffff, 0 }

static const struct pci_id_match nouveau_device_match[] = {
	NOUVEAU_PCI_DEVICE(PCI_VENDOR_NVIDIA, PCI_MATCH_ANY),
	NOUVEAU_PCI_DEVICE(PCI_VENDOR_NVIDIA_SGS, PCI_MATCH_ANY),
	{ 0, 0, 0 },
};

static Bool NVPciProbe (	DriverPtr 		drv,
				int 			entity_num,
				struct pci_device	*dev,
				intptr_t		match_data	);

/*
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

_X_EXPORT DriverRec NV = {
	NV_VERSION,
	NV_DRIVER_NAME,
	NVIdentify,
	NULL,
	NVAvailableOptions,
	NULL,
	0,
	NULL,
	nouveau_device_match,
	NVPciProbe
};

struct NvFamily
{
  char *name;
  char *chipset;
};

static struct NvFamily NVKnownFamilies[] =
{
  { "RIVA TNT",    "NV04" },
  { "RIVA TNT2",   "NV05" },
  { "GeForce 256", "NV10" },
  { "GeForce 2",   "NV11, NV15" },
  { "GeForce 4MX", "NV17, NV18" },
  { "GeForce 3",   "NV20" },
  { "GeForce 4Ti", "NV25, NV28" },
  { "GeForce FX",  "NV3x" },
  { "GeForce 6",   "NV4x" },
  { "GeForce 7",   "G7x" },
  { "GeForce 8",   "G8x" },
  { NULL, NULL}
};

static MODULESETUPPROTO(nouveauSetup);

static XF86ModuleVersionInfo nouveauVersRec =
{
    "nouveau",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    NV_MAJOR_VERSION, NV_MINOR_VERSION, NV_PATCHLEVEL,
    ABI_CLASS_VIDEODRV,                     /* This is a video driver */
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0,0,0,0}
};

_X_EXPORT XF86ModuleData nouveauModuleData = { &nouveauVersRec, nouveauSetup, NULL };

static pointer
nouveauSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
	static Bool setupDone = FALSE;

	/* This module should be loaded only once, but check to be sure. */

	if (!setupDone) {
		setupDone = TRUE;
		/* The 1 here is needed to turn off a backwards compatibility mode */
		/* Otherwise NVPciProbe() is not called */
		xf86AddDriver(&NV, module, 1);

		/*
		 * The return value must be non-NULL on success even though there
		 * is no TearDownProc.
		 */
		return (pointer)1;
	} else {
		if (errmaj) *errmaj = LDR_ONCEONLY;
		return NULL;
	}
}

static const OptionInfoRec *
NVAvailableOptions(int chipid, int busid)
{
    return NVOptions;
}

/* Mandatory */
static void
NVIdentify(int flags)
{
    struct NvFamily *family;
    size_t maxLen=0;

    xf86DrvMsg(0, X_INFO, NV_NAME " driver " NV_DRIVER_DATE "\n");
    xf86DrvMsg(0, X_INFO, NV_NAME " driver for NVIDIA chipset families :\n");

    /* maximum length for alignment */
    family = NVKnownFamilies;
    while(family->name && family->chipset)
    {
        maxLen = max(maxLen, strlen(family->name));
        family++;
    }

    /* display */
    family = NVKnownFamilies;
    while(family->name && family->chipset)
    {
        size_t len = strlen(family->name);
        xf86ErrorF("\t%s", family->name);
        while(len<maxLen+1)
        {
            xf86ErrorF(" ");
            len++;
        }
        xf86ErrorF("(%s)\n", family->chipset);
        family++;
    }
}

/* This returns architecture in hexdecimal, so NV40 is 0x40 */
static int NVGetArchitecture(volatile uint32_t *regs)
{
	int architecture = 0;

	/* We're dealing with >=NV10 */
	if ((regs[0] & 0x0f000000) > 0 )
		/* Bit 27-20 contain the architecture in hex */
		architecture = (regs[0] & 0xff00000) >> 20;
	/* NV04 or NV05 */
	else if ((regs[0] & 0xff00fff0) == 0x20004000)
		architecture = 0x04;

	return architecture;
}

/* Reading the pci_id from the card registers is the most reliable way */
static uint32_t NVGetPCIID(volatile uint32_t *regs)
{
	int architecture = NVGetArchitecture(regs);
	uint32_t pci_id;

	/* Dealing with an unknown or unsupported card */
	if (architecture == 0)
		return 0;

	if (architecture >= 0x40)
		pci_id = regs[0x88000/4];
	else
		pci_id = regs[0x1800/4];

	/* A pci-id can be inverted, we must correct this */
	if ((pci_id & 0xffff) == PCI_VENDOR_NVIDIA)
		pci_id = (PCI_VENDOR_NVIDIA << 16) | (pci_id >> 16);
	else if ((pci_id & 0xffff) == PCI_VENDOR_NVIDIA_SGS)
		pci_id = (PCI_VENDOR_NVIDIA_SGS << 16) | (pci_id >> 16);
	/* Checking endian issues */
	else {
		/* PCI_VENDOR_NVIDIA = 0x10DE */
		if ((pci_id & (0xffff << 16)) == (0xDE10 << 16)) /* wrong endian */
			pci_id = (PCI_VENDOR_NVIDIA << 16) | ((pci_id << 8) & 0x0000ff00) |
				((pci_id >> 8) & 0x000000ff);
		/* PCI_VENDOR_NVIDIA_SGS = 0x12D2 */
		else if ((pci_id & (0xffff << 16)) == (0xD212 << 16)) /* wrong endian */
			pci_id = (PCI_VENDOR_NVIDIA_SGS << 16) | ((pci_id << 8) & 0x0000ff00) |
				((pci_id >> 8) & 0x000000ff);
	}

	return pci_id;
}

static Bool NVPciProbe (	DriverPtr 		drv,
				int 			entity_num,
				struct pci_device	*dev,
				intptr_t		match_data	)
{
	ScrnInfoPtr pScrn = NULL;

	volatile uint32_t *regs = NULL;

	/* Temporary mapping to discover the architecture */
	pci_device_map_range(dev, dev->regions[0].base_addr, 0x90000, 0,
			     (void *) &regs);

	uint8_t architecture = NVGetArchitecture(regs);

	CARD32 pci_id = NVGetPCIID(regs);

	pci_device_unmap_range(dev, (void *) regs, 0x90000);

	/* Currently NV04 up to NVAA is known. */
	/* Using 0xAF as upper bound for some margin. */
	if (architecture >= 0x04 && architecture <= 0xAF) {

		/* At this stage the pci_id should be ok, so we generate this
		 * to avoid list duplication */
		/* AGP bridge chips need their bridge chip id to be detected */
		PciChipsets NVChipsets[] = {
			{ pci_id, (dev->vendor_id << 16) | dev->device_id, NULL },
			{ -1, -1, NULL }
		};

		pScrn = xf86ConfigPciEntity(pScrn, 0, entity_num, NVChipsets, 
						NULL, NULL, NULL, NULL, NULL);

		if (pScrn != NULL) {
			pScrn->driverVersion    = NV_VERSION;
			pScrn->driverName       = NV_DRIVER_NAME;
			pScrn->name             = NV_NAME;

			pScrn->Probe            = NULL;
			pScrn->PreInit          = NVPreInit;
			pScrn->ScreenInit       = NVScreenInit;
			pScrn->SwitchMode       = NVSwitchMode;
			pScrn->AdjustFrame      = NVAdjustFrame;
			pScrn->EnterVT          = NVEnterVT;
			pScrn->LeaveVT          = NVLeaveVT;
			pScrn->FreeScreen       = NVFreeScreen;

			return TRUE;
		}
	}

	return FALSE;
}

#define MAX_CHIPS MAXSCREENS

Bool
NVSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];

	return xf86SetSingleMode(pScrn, mode, RR_Rotate_0);
}

/*
 * This function is used to initialize the Start Address - the first
 * displayed location in the video memory.
 */
/* Usually mandatory */
void 
NVAdjustFrame(int scrnIndex, int x, int y, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	NVPtr pNv = NVPTR(pScrn);

	if (pNv->kms_enable) {
		drmmode_adjust_frame(pScrn, x, y, flags);
	} else {
		xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(pScrn);
		xf86CrtcPtr crtc = config->output[config->compat_output]->crtc;

		if (crtc && crtc->enabled)
			NVCrtcSetBase(crtc, x, y);
	}
}

static Bool
NV50AcquireDisplay(ScrnInfoPtr pScrn)
{
	if (!NV50DispInit(pScrn))
		return FALSE;
	if (!NV50CursorAcquire(pScrn))
		return FALSE;

	return TRUE;
}

static Bool
NV50ReleaseDisplay(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	NV50CursorRelease(pScrn);
	NV50DispShutdown(pScrn);

	if (pNv->pInt10 && pNv->Int10Mode) {
		xf86Int10InfoPtr pInt10 = pNv->pInt10;

		pInt10->num = 0x10;
		pInt10->ax  = 0x4f02;
		pInt10->bx  = pNv->Int10Mode | 0x8000;
		pInt10->cx  =
		pInt10->dx  = 0;
		xf86ExecX86int10(pInt10);
	}

	return TRUE;
}

/*
 * This is called when VT switching back to the X server.  Its job is
 * to reinitialise the video mode.
 *
 * We may wish to unmap video/MMIO memory too.
 */

/* Mandatory */
static Bool
NVEnterVT(int scrnIndex, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	NVPtr pNv = NVPTR(pScrn);
	int ret;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NVEnterVT is called.\n");

	ret = drmSetMaster(nouveau_device(pNv->dev)->fd);
	if (ret)
		ErrorF("Unable to get master: %d\n", ret);

	if (!pNv->NoAccel)
		NVAccelCommonInit(pScrn);

	if (!pNv->kms_enable) {
		/* Save current state, VGA fonts etc */
		NVSave(pScrn);

		/* Clear the framebuffer, we don't want to see garbage
		 * on-screen up until X decides to draw something
		 */
		nouveau_bo_map(pNv->scanout, NOUVEAU_BO_WR);
		memset(pNv->scanout->map, 0, pNv->scanout->size);
		nouveau_bo_unmap(pNv->scanout);

		if (pNv->Architecture == NV_ARCH_50) {
			if (!NV50AcquireDisplay(pScrn))
				return FALSE;
		}
	}

	pNv->allow_dpms = FALSE;
	if (!xf86SetDesiredModes(pScrn))
		return FALSE;
	pNv->allow_dpms = TRUE;

	if (pNv->overlayAdaptor && pNv->Architecture != NV_ARCH_04)
		NV10WriteOverlayParameters(pScrn);

	return TRUE;
}

/*
 * This is called when VT switching away from the X server.  Its job is
 * to restore the previous (text) mode.
 *
 * We may wish to remap video/MMIO memory too.
 */

/* Mandatory */
static void
NVLeaveVT(int scrnIndex, int flags)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	NVPtr pNv = NVPTR(pScrn);
	int ret;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NVLeaveVT is called.\n");

	NVSync(pScrn);

	ret = drmDropMaster(nouveau_device(pNv->dev)->fd);
	if (ret)
		ErrorF("Error dropping master: %d\n", ret);

	if (!pNv->kms_enable) {
		if (pNv->Architecture < NV_ARCH_50)
			NVRestore(pScrn);
		else
			NV50ReleaseDisplay(pScrn);
	}
}

static void 
NVBlockHandler (
	int i, 
	pointer blockData, 
	pointer pTimeout,
	pointer pReadmask
)
{
	ScreenPtr pScreen = screenInfo.screens[i];
	ScrnInfoPtr pScrnInfo = xf86Screens[i];
	NVPtr pNv = NVPTR(pScrnInfo);

	if (pScrnInfo->vtSema && !pNv->NoAccel)
		FIRE_RING (pNv->chan);

	pScreen->BlockHandler = pNv->BlockHandler;
	(*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);
	pScreen->BlockHandler = NVBlockHandler;

	if (pNv->VideoTimerCallback) 
		(*pNv->VideoTimerCallback)(pScrnInfo, currentTime.milliseconds);
}

static Bool
NVCreateScreenResources(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	PixmapPtr ppix;

	pScreen->CreateScreenResources = pNv->CreateScreenResources;
	if (!(*pScreen->CreateScreenResources)(pScreen))
		return FALSE;
	pScreen->CreateScreenResources = NVCreateScreenResources;

	if (pNv->exa_driver_pixmaps) {
		ppix = pScreen->GetScreenPixmap(pScreen);
		nouveau_bo_ref(pNv->scanout, &nouveau_pixmap(ppix)->bo);
	}

	return TRUE;
}

/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should also unmap the video memory, and free
 * any per-generation data allocated by the driver.  It should finish
 * by unwrapping and calling the saved CloseScreen function.
 */

/* Mandatory */
static Bool
NVCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	NVPtr pNv = NVPTR(pScrn);

	if (!pNv->exa_driver_pixmaps)
		NVDRICloseScreen(pScrn);
	else
		nouveau_dri2_fini(pScreen);

	if (pScrn->vtSema) {
		NVLeaveVT(scrnIndex, 0);
		pScrn->vtSema = FALSE;
	}
	if (pScrn->vtSema) {
		NVLeaveVT(scrnIndex, 0);
		pScrn->vtSema = FALSE;
	}

	NVAccelFree(pScrn);
	NVTakedownVideo(pScrn);
	NVTakedownDma(pScrn);
	NVUnmapMem(pScrn);

	xf86_cursors_fini(pScreen);

	if (pNv->ShadowPtr) {
		xfree(pNv->ShadowPtr);
		pNv->ShadowPtr = NULL;
	}
	if (pNv->overlayAdaptor) {
		xfree(pNv->overlayAdaptor);
		pNv->overlayAdaptor = NULL;
	}
	if (pNv->blitAdaptor) {
		xfree(pNv->blitAdaptor);
		pNv->blitAdaptor = NULL;
	}
	if (pNv->textureAdaptor[0]) {
		xfree(pNv->textureAdaptor[0]);
		pNv->textureAdaptor[0] = NULL;
	}
	if (pNv->textureAdaptor[1]) {
		xfree(pNv->textureAdaptor[1]);
		pNv->textureAdaptor[1] = NULL;
	}
	if (pNv->EXADriverPtr) {
		exaDriverFini(pScreen);
		xfree(pNv->EXADriverPtr);
		pNv->EXADriverPtr = NULL;
	}

	pScrn->vtSema = FALSE;
	pScreen->CloseScreen = pNv->CloseScreen;
	pScreen->BlockHandler = pNv->BlockHandler;
	return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

/* Free up any persistent data structures */

/* Optional */
static void
NVFreeScreen(int scrnIndex, int flags)
{
	/*
	 * This only gets called when a screen is being deleted.  It does not
	 * get called routinely at the end of a server generation.
	 */

	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	NVPtr pNv = NVPTR(pScrn);

	if (!pNv)
		return;

	NVCloseDRM(pScrn);

	if (pNv->Architecture == NV_ARCH_50 && !pNv->kms_enable) {
		NV50ConnectorDestroy(pScrn);
		NV50OutputDestroy(pScrn);
		NV50CrtcDestroy(pScrn);
	}

	/* Free this here and not in CloseScreen, as it's needed after the first server generation. */
	if (pNv->pInt10)
		xf86FreeInt10(pNv->pInt10);

	xfree(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
}

static Bool
nouveau_xf86crtc_resize(ScrnInfoPtr scrn, int width, int height)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	ScreenPtr screen = screenInfo.screens[scrn->scrnIndex];
	NVPtr pNv = NVPTR(scrn);
	uint32_t pitch, old_width, old_height, old_pitch;
	struct nouveau_bo *old_bo = NULL;
	uint32_t tile_mode = 0, tile_flags = 0, ah = height;
	PixmapPtr ppix = screen->GetScreenPixmap(screen);
	int ret, i;

	ErrorF("resize called %d %d\n", width, height);

	if (scrn->virtualX == width && scrn->virtualY == height)
		return TRUE;

	pitch  = nv_pitch_align(pNv, width, scrn->depth);
	pitch *= (scrn->bitsPerPixel >> 3);

	old_width = scrn->virtualX;
	old_height = scrn->virtualY;
	old_pitch = scrn->displayWidth;
	nouveau_bo_ref(pNv->scanout, &old_bo);
	nouveau_bo_ref(NULL, &pNv->scanout);

	scrn->virtualX = width;
	scrn->virtualY = height;
	scrn->displayWidth = pitch / (scrn->bitsPerPixel >> 3);

	ret = nouveau_bo_new_tile(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_MAP,
				  0, pitch * ah, tile_mode, tile_flags,
				  &pNv->scanout);
	if (ret)
		goto fail;

	if (pNv->ShadowPtr) {
		xfree(pNv->ShadowPtr);
		pNv->ShadowPitch = pitch;
		pNv->ShadowPtr = xalloc(pNv->ShadowPitch * height);
	}

	nouveau_bo_map(pNv->scanout, NOUVEAU_BO_RDWR);
	screen->ModifyPixmapHeader(ppix, width, height, -1, -1, pitch,
				   (!pNv->NoAccel || pNv->ShadowFB) ?
				   pNv->ShadowPtr : pNv->scanout->map);
	nouveau_bo_unmap(pNv->scanout);

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		if (!crtc->enabled)
			continue;

		xf86CrtcSetMode(crtc, &crtc->mode, crtc->rotation,
				crtc->x, crtc->y);
	}

	nouveau_bo_ref(NULL, &old_bo);

	NVDRIFinishScreenInit(scrn, true);
	return TRUE;

 fail:
	nouveau_bo_ref(old_bo, &pNv->scanout);
	scrn->virtualX = old_width;
	scrn->virtualY = old_height;
	scrn->displayWidth = old_pitch;

	return FALSE;
}

static const xf86CrtcConfigFuncsRec nv_xf86crtc_config_funcs = {
	nouveau_xf86crtc_resize
};

#define NVPreInitFail(fmt, args...) do {                                    \
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "%d: "fmt, __LINE__, ##args); \
	NVFreeScreen(pScrn->scrnIndex, 0);                                  \
	return FALSE;                                                       \
} while(0)

static void
NVCloseDRM(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	nouveau_device_close(&pNv->dev);
}

static Bool
NVPreInitDRM(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	drmVersion *version;
	char *bus_id;
	int ret;

	if (!NVDRIGetVersion(pScrn))
		return FALSE;

	/* Load the kernel module, and open the DRM */
	bus_id = DRICreatePCIBusID(pNv->PciInfo);
	ret = DRIOpenDRMMaster(pScrn, SAREA_MAX, bus_id, "nouveau");
	if (!ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "[drm] error opening the drm\n");
		xfree(bus_id);
		return FALSE;
	}

	/* Check the version reported by the kernel module.  In theory we
	 * shouldn't have to do this, as libdrm_nouveau will do its own checks.
	 * But, we're currently using the kernel patchlevel to also version
	 * the DRI interface.
	 */
	version = drmGetVersion(DRIMasterFD(pScrn));
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "[drm] nouveau interface version: %d.%d.%d\n",
		   version->version_major, version->version_minor,
		   version->version_patchlevel);

	ret = !(version->version_patchlevel == NOUVEAU_DRM_HEADER_PATCHLEVEL);
	drmFree(version);
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "[drm] wrong version, expecting 0.0.%d\n",
			   NOUVEAU_DRM_HEADER_PATCHLEVEL);
		xfree(bus_id);
		return FALSE;
	}

	/* Initialise libdrm_nouveau */
	ret = nouveau_device_open_existing(&pNv->dev, 1, DRIMasterFD(pScrn), 0);
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "[drm] error creating device\n");
		xfree(bus_id);
		return FALSE;
	}

	/* Check if KMS is enabled before we do anything, we don't want to
	 * go stomping on registers behind its back
	 */
#ifdef XF86DRM_MODE
	pNv->kms_enable = !drmCheckModesettingSupported(bus_id);
#endif
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		   "[drm] kernel modesetting %s\n", pNv->kms_enable ?
		   "in use" : "not available");

	xfree(bus_id);
	return TRUE;
}

/* Mandatory */
Bool
NVPreInit(ScrnInfoPtr pScrn, int flags)
{
	NVPtr pNv;
	MessageType from;
	int ret, i;

	if (flags & PROBE_DETECT) {
		EntityInfoPtr pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

		if (!pEnt)
			return FALSE;

		i = pEnt->index;
		xfree(pEnt);

		if (xf86LoadSubModule(pScrn, "vbe")) {
			vbeInfoPtr pVbe = VBEInit(NULL, i);
			ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
			vbeFree(pVbe);
		}

		return TRUE;
	}

	/*
	 * Note: This function is only called once at server startup, and
	 * not at the start of each server generation.  This means that
	 * only things that are persistent across server generations can
	 * be initialised here.  xf86Screens[] is (pScrn is a pointer to one
	 * of these).  Privates allocated using xf86AllocateScrnInfoPrivateIndex()  
	 * are too, and should be used for data that must persist across
	 * server generations.
	 *
	 * Per-generation data should be allocated with
	 * AllocateScreenPrivateIndex() from the ScreenInit() function.
	 */

	/* Check the number of entities, and fail if it isn't one. */
	if (pScrn->numEntities != 1)
		return FALSE;

	/* Allocate the NVRec driverPrivate */
	if (!(pScrn->driverPrivate = xnfcalloc(1, sizeof(NVRec))))
		return FALSE;
	pNv = NVPTR(pScrn);

	/* Get the entity, and make sure it is PCI. */
	pNv->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
	if (pNv->pEnt->location.type != BUS_PCI)
		return FALSE;
 
	/* Find the PCI info for this screen */
	pNv->PciInfo = xf86GetPciInfoForEntity(pNv->pEnt->index);
	pNv->Primary = xf86IsPrimaryPci(pNv->PciInfo);

	volatile uint32_t *regs = NULL;
	pci_device_map_range(pNv->PciInfo, pNv->PciInfo->regions[0].base_addr,
			     0x90000, 0, (void *)&regs);
	pNv->Chipset = NVGetPCIID(regs) & 0xffff;
	pNv->NVArch = NVGetArchitecture(regs);
	pci_device_unmap_range(pNv->PciInfo, (void *) regs, 0x90000);

	pScrn->chipset = malloc(sizeof(char) * 25);
	sprintf(pScrn->chipset, "NVIDIA NV%02X", pNv->NVArch);

	if(!pScrn->chipset) {
		pScrn->chipset = "Unknown NVIDIA";
	}

	/*
	* This shouldn't happen because such problems should be caught in
	* NVProbe(), but check it just in case.
	*/
	if (pScrn->chipset == NULL)
		NVPreInitFail("ChipID 0x%04X is not recognised\n", pNv->Chipset);

	if (pNv->NVArch < 0x04)
		NVPreInitFail("Chipset \"%s\" is not recognised\n", pScrn->chipset);

	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "Chipset: \"%s\"\n", pScrn->chipset);

	/* The highest architecture currently supported is NV5x */
	if (pNv->NVArch >= 0x80) {
		pNv->Architecture =  NV_ARCH_50;
	} else if (pNv->NVArch >= 0x60) {
		pNv->Architecture =  NV_ARCH_40;
	} else if (pNv->NVArch >= 0x50) {
		pNv->Architecture =  NV_ARCH_50;
	} else if (pNv->NVArch >= 0x40) {
		pNv->Architecture =  NV_ARCH_40;
	} else if (pNv->NVArch >= 0x30) {
		pNv->Architecture = NV_ARCH_30;
	} else if (pNv->NVArch >= 0x20) {
		pNv->Architecture = NV_ARCH_20;
	} else if (pNv->NVArch >= 0x10) {
		pNv->Architecture = NV_ARCH_10;
	} else if (pNv->NVArch >= 0x04) {
		pNv->Architecture = NV_ARCH_04;
	/*  The lowest architecture currently supported is NV04 */
	} else {
		return FALSE;
	}

	/* Initialize the card through int10 interface if needed */
	if (xf86LoadSubModule(pScrn, "int10")) {
#if !defined(__alpha__) && !defined(__powerpc__)
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Initializing int10\n");
		pNv->pInt10 = xf86InitInt10(pNv->pEnt->index);
#endif
	}

	/* Initialise the kernel module */
	if (!NVPreInitDRM(pScrn))
		NVPreInitFail("\n");

	/* Save current console video mode */
	if (pNv->Architecture >= NV_ARCH_50 && pNv->pInt10 && !pNv->kms_enable) {
		const xf86Int10InfoPtr pInt10 = pNv->pInt10;

		pInt10->num = 0x10;
		pInt10->ax  = 0x4f03;
		pInt10->bx  =
		pInt10->cx  =
		pInt10->dx  = 0;
		xf86ExecX86int10(pInt10);
		pNv->Int10Mode = pInt10->bx & 0x3fff;

		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			   "VESA-HACK: Console VGA mode is 0x%x\n",
			   pNv->Int10Mode);
	}

	/* Set pScrn->monitor */
	pScrn->monitor = pScrn->confScreen->monitor;

	/*
	 * The first thing we should figure out is the depth, bpp, etc.
	 */

	if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb)) {
		NVPreInitFail("\n");
	} else {
		/* Check that the returned depth is one we support */
		switch (pScrn->depth) {
		case 16:
		case 24:
			/* OK */
			break;
		case 30:
			/* OK on NV50 KMS */
			if (!pNv->kms_enable || pNv->Architecture != NV_ARCH_50)
				NVPreInitFail("Depth 30 supported on G80+KMS only\n");
			break;
		case 15: /* 15 may get done one day, so leave any code for it in place */
		default:
			NVPreInitFail("Given depth (%d) is not supported by this driver\n",
				pScrn->depth);
		}
	}
	xf86PrintDepthBpp(pScrn);

	/*
	 * This must happen after pScrn->display has been set because
	 * xf86SetWeight references it.
	 */
	rgb rgbzeros = {0, 0, 0};

	if (pScrn->depth == 30) {
		rgb rgbmask;

		rgbmask.red   = 0x000003ff;
		rgbmask.green = 0x000ffc00;
		rgbmask.blue  = 0x3ff00000;
		if (!xf86SetWeight(pScrn, rgbzeros, rgbmask))
			NVPreInitFail("\n");

		/* xf86SetWeight() seems to think ffs(1) == 0... */
		pScrn->offset.red--;
		pScrn->offset.green--;
		pScrn->offset.blue--;
	} else {
		if (!xf86SetWeight(pScrn, rgbzeros, rgbzeros))
			NVPreInitFail("\n");
	}

	if (!xf86SetDefaultVisual(pScrn, -1))
		NVPreInitFail("\n");

	/* We don't support DirectColor */
	if (pScrn->defaultVisual != TrueColor) {
		NVPreInitFail("Given default visual (%s) is not supported at depth %d\n",
			      xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
	}

	/* The vgahw module should be loaded here when needed */
	if (!xf86LoadSubModule(pScrn, "vgahw")) {
		NVPreInitFail("\n");
	}

	/* We use a programmable clock */
	pScrn->progClock = TRUE;

	/* Collect all of the relevant option flags (fill in pScrn->options) */
	xf86CollectOptions(pScrn, NULL);

	/* Process the options */
	if (!(pNv->Options = xalloc(sizeof(NVOptions))))
		return FALSE;
	memcpy(pNv->Options, NVOptions, sizeof(NVOptions));
	xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, pNv->Options);

	from = X_DEFAULT;

	pNv->HWCursor = TRUE;
	/*
	 * The preferred method is to use the "hw cursor" option as a tri-state
	 * option, with the default set above.
	 */
	if (xf86GetOptValBool(pNv->Options, OPTION_HW_CURSOR, &pNv->HWCursor)) {
		from = X_CONFIG;
	}
	/* For compatibility, accept this too (as an override) */
	if (xf86ReturnOptValBool(pNv->Options, OPTION_SW_CURSOR, FALSE)) {
		from = X_CONFIG;
		pNv->HWCursor = FALSE;
	}
	xf86DrvMsg(pScrn->scrnIndex, from, "Using %s cursor\n",
		pNv->HWCursor ? "HW" : "SW");

	pNv->FpScale = TRUE;

	if (xf86GetOptValBool(pNv->Options, OPTION_FP_SCALE, &pNv->FpScale)) {
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Flat panel scaling %s\n",
			pNv->FpScale ? "on" : "off");
	}

	if (xf86ReturnOptValBool(pNv->Options, OPTION_NOACCEL, FALSE)) {
		pNv->NoAccel = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Acceleration disabled\n");
	}

	if (xf86ReturnOptValBool(pNv->Options, OPTION_SHADOW_FB, FALSE)) {
		pNv->ShadowFB = TRUE;
		pNv->NoAccel = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
			"Using \"Shadow Framebuffer\" - acceleration disabled\n");
	}

#if (EXA_VERSION_MAJOR == 2 && EXA_VERSION_MINOR >= 5) || EXA_VERSION_MAJOR > 2
	if (!pNv->NoAccel &&
	    xf86ReturnOptValBool(pNv->Options, OPTION_EXA_PIXMAPS, TRUE)) {
		if (pNv->kms_enable) {
			pNv->exa_driver_pixmaps = TRUE;
		} else {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "EXAPixmaps support requires KMS\n");
		}
	}

	if (!pNv->NoAccel && pNv->kms_enable &&
	     pNv->Architecture >= NV_ARCH_50) {
		pNv->wfb_enabled = TRUE;
		pNv->tiled_scanout = TRUE;
	}
#endif

	if(xf86GetOptValInteger(pNv->Options, OPTION_VIDEO_KEY, &(pNv->videoKey))) {
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video key set to 0x%x\n",
					pNv->videoKey);
	} else {
		pNv->videoKey =  (1 << pScrn->offset.red) | 
					(1 << pScrn->offset.green) |
		(((pScrn->mask.blue >> pScrn->offset.blue) - 1) << pScrn->offset.blue);
	}

	pNv->FPDither = FALSE;
	if (xf86GetOptValBool(pNv->Options, OPTION_FP_DITHER, &(pNv->FPDither))) 
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "enabling flat panel dither\n");

	if (pNv->pEnt->device->MemBase != 0) {
		/* Require that the config file value matches one of the PCI values. */
		if (!xf86CheckPciMemBase(pNv->PciInfo, pNv->pEnt->device->MemBase)) {
			NVPreInitFail(
				"MemBase 0x%08lX doesn't match any PCI base register.\n",
				pNv->pEnt->device->MemBase);
		}
		pNv->VRAMPhysical = pNv->pEnt->device->MemBase;
		from = X_CONFIG;
	} else {
		if (pNv->PciInfo->regions[1].base_addr != 0) {
			pNv->VRAMPhysical = pNv->PciInfo->regions[1].base_addr & 0xff800000;
			from = X_PROBED;
		} else {
			NVPreInitFail("No valid FB address in PCI config space\n");
			return FALSE;
		}
	}
	xf86DrvMsg(pScrn->scrnIndex, from, "Linear framebuffer at 0x%lX\n",
		(unsigned long)pNv->VRAMPhysical);

	if (pNv->pEnt->device->IOBase != 0) {
		/* Require that the config file value matches one of the PCI values. */
		if (!xf86CheckPciMemBase(pNv->PciInfo, pNv->pEnt->device->IOBase)) {
			NVPreInitFail("IOBase 0x%08lX doesn't match any PCI base register.\n",
				pNv->pEnt->device->IOBase);
		}
		pNv->IOAddress = pNv->pEnt->device->IOBase;
		from = X_CONFIG;
	} else {
		if (pNv->PciInfo->regions[0].base_addr != 0) {
			pNv->IOAddress = pNv->PciInfo->regions[0].base_addr & 0xffffc000;
			from = X_PROBED;
		} else {
			NVPreInitFail("No valid MMIO address in PCI config space\n");
		}
	}
	xf86DrvMsg(pScrn->scrnIndex, from, "MMIO registers at 0x%lX\n",
		(unsigned long)pNv->IOAddress);

#ifdef XF86DRM_MODE
	if (pNv->kms_enable){
		ret = drmmode_pre_init(pScrn, nouveau_device(pNv->dev)->fd,
				       pScrn->bitsPerPixel >> 3);
		if (ret == FALSE)
			NVPreInitFail("Kernel modesetting failed to initialize\n");
	} else
#endif
	{
		int max_width, max_height;

		if (pNv->Architecture < NV_ARCH_10) {
			max_width = (pScrn->bitsPerPixel > 16) ? 2032 : 2048;
			max_height = 2048;
		} else if (pNv->Architecture < NV_ARCH_50) {
			max_width = (pScrn->bitsPerPixel > 16) ? 4080 : 4096;
			max_height = 4096;
		} else {
			max_width = (pScrn->bitsPerPixel > 16) ? 8176 : 8192;
			max_height = 8192;
		}

		/* Allocate an xf86CrtcConfig */
		xf86CrtcConfigInit(pScrn, &nv_xf86crtc_config_funcs);
		xf86CrtcSetSizeRange(pScrn, 320, 200, max_width, max_height);
	}

	NVCommonSetup(pScrn);

	if (!pNv->kms_enable) {
		if (pNv->Architecture == NV_ARCH_50)
			if (!NV50DispPreInit(pScrn))
				NVPreInitFail("\n");

		/* This is the internal system, not the randr-1.2 ones. */
		if (pNv->Architecture == NV_ARCH_50) {
			NV50CrtcInit(pScrn);
			NV50ConnectorInit(pScrn);
			NV50OutputSetup(pScrn);
		}

		for (i = 0; i <= pNv->twoHeads; i++) {
			if (pNv->Architecture == NV_ARCH_50)
				nv50_crtc_init(pScrn, i);
			else
				nv_crtc_init(pScrn, i);
		}

		if (pNv->Architecture < NV_ARCH_50) {
			NVLockVgaCrtcs(pNv, false);
			NvSetupOutputs(pScrn);
		} else
			nv50_output_create(pScrn); /* create randr-1.2 "outputs". */

		if (!xf86InitialConfiguration(pScrn, TRUE))
			NVPreInitFail("No valid modes.\n");
	}

	pScrn->videoRam = pNv->RamAmountKBytes;
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED, "VideoRAM: %d kBytes\n",
		pScrn->videoRam);

	pNv->VRAMPhysicalSize = pScrn->videoRam * 1024;

	/*
	 * If the driver can do gamma correction, it should call xf86SetGamma()
	 * here.
	 */
	Gamma gammazeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, gammazeros))
		NVPreInitFail("\n");

	if (pNv->Architecture >= NV_ARCH_50 && pNv->tiled_scanout) {
		int cpp = pScrn->bitsPerPixel >> 3;
		pScrn->displayWidth = pScrn->virtualX * cpp;
		pScrn->displayWidth = NOUVEAU_ALIGN(pScrn->displayWidth, 64);
		pScrn->displayWidth = pScrn->displayWidth / cpp;
	} else {
		pScrn->displayWidth = nv_pitch_align(pNv, pScrn->virtualX,
						     pScrn->depth);
	}

	/* Set the current mode to the first in the list */
	pScrn->currentMode = pScrn->modes;

	/* Print the list of modes being used */
	xf86PrintModes(pScrn);

	/* Set display resolution */
	xf86SetDpi(pScrn, 0, 0);

	if (pNv->wfb_enabled) {
		if (xf86LoadSubModule(pScrn, "wfb") == NULL)
			NVPreInitFail("\n");
	}

	if (xf86LoadSubModule(pScrn, "fb") == NULL)
		NVPreInitFail("\n");

	/* Load EXA if needed */
	if (!pNv->NoAccel) {
		if (!xf86LoadSubModule(pScrn, "exa")) {
			NVPreInitFail("\n");
		}
	}

	/* Load shadowfb */
	if (!xf86LoadSubModule(pScrn, "shadowfb"))
		NVPreInitFail("\n");

	return TRUE;
}


static Bool
NVMapMem(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	uint64_t res;
	uint32_t tile_mode = 0, tile_flags = 0;
	int ret, size;

	nouveau_device_get_param(pNv->dev, NOUVEAU_GETPARAM_FB_SIZE, &res);
	pNv->VRAMSize=res;
	nouveau_device_get_param(pNv->dev, NOUVEAU_GETPARAM_FB_PHYSICAL, &res);
	pNv->VRAMPhysical=res;
	nouveau_device_get_param(pNv->dev, NOUVEAU_GETPARAM_AGP_SIZE, &res);
	pNv->AGPSize=res;

	size = pScrn->displayWidth * (pScrn->bitsPerPixel >> 3);
	if (pNv->Architecture >= NV_ARCH_50 && pNv->tiled_scanout) {
		tile_mode = 4;
		tile_flags = pScrn->bitsPerPixel == 16 ? 0x7000 : 0x7a00;
		size *= NOUVEAU_ALIGN(pScrn->virtualY, (1 << (tile_mode + 2)));
	} else {
		size *= pScrn->virtualY;
	}

	ret = nouveau_bo_new_tile(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_MAP,
				  0, size, tile_mode, tile_flags,
				  &pNv->scanout);
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Error allocating scanout buffer: %d\n", ret);
		return FALSE;
	}

	nouveau_bo_map(pNv->scanout, NOUVEAU_BO_RDWR);
	nouveau_bo_unmap(pNv->scanout);

	if (pNv->NoAccel)
		goto skip_offscreen_gart;

	if (!pNv->exa_driver_pixmaps) {
		size = (pNv->VRAMPhysicalSize / 2) - size;

		if (pNv->Architecture >= NV_ARCH_50) {
			tile_mode = 0;
			tile_flags = 0x7000;
		}

		ret = nouveau_bo_new_tile(pNv->dev, NOUVEAU_BO_VRAM |
					  NOUVEAU_BO_MAP, 0, size, tile_mode,
					  tile_flags, &pNv->offscreen);
		if (ret) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Error allocating"
				   " offscreen pixmap area: %d\n", ret);
			return FALSE;
		}

		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Allocated %dMiB VRAM for offscreen pixmaps\n",
			   (uint32_t)(pNv->offscreen->size >> 20));

		nouveau_bo_map(pNv->offscreen, NOUVEAU_BO_RDWR);
		pNv->offscreen_map = pNv->offscreen->map;
		nouveau_bo_unmap(pNv->offscreen);
	}

	if (pNv->AGPSize) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "AGPGART: %dMiB available\n",
			   (unsigned int)(pNv->AGPSize >> 20));
		if (pNv->AGPSize > (16*1024*1024))
			size = 16*1024*1024;
		else
			/* always leave 512kb for other things like the fifos */
			size = pNv->AGPSize - 512*1024;
	} else {
		size = (4 << 20) - (1 << 18) ;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "GART: PCI DMA - using %dKiB\n", size >> 10);
	}

	if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_GART | NOUVEAU_BO_MAP,
			   0, size, &pNv->GART)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Unable to allocate GART memory\n");
	}
	if (pNv->GART) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "GART: Allocated %dMiB as a scratch buffer\n",
			   (unsigned int)(pNv->GART->size >> 20));
	}

skip_offscreen_gart:
	/* We don't need to allocate cursors / lut here if we're using
	 * kernel modesetting
	 **/
	if (pNv->kms_enable)
		return TRUE;

	if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_PIN |
			   NOUVEAU_BO_MAP, 0, 64 * 64 * 4, &pNv->Cursor)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Failed to allocate memory for hardware cursor\n");
		return FALSE;
	}

	if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_PIN |
			   NOUVEAU_BO_MAP, 0, 64 * 64 * 4, &pNv->Cursor2)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Failed to allocate memory for hardware cursor\n");
		return FALSE;
	}

	/* This is not the ideal solution, but significant changes are needed
	 * otherwise. Ideally you do this once upon preinit, but drm is
	 * closed between screen inits.
	 */
	if (pNv->Architecture == NV_ARCH_50) {
		int i;

		for(i = 0; i < 2; i++) {
			nouveauCrtcPtr crtc = pNv->crtc[i];

			if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM |
					   NOUVEAU_BO_PIN |
					   NOUVEAU_BO_MAP, 0, 0x1000,
					   &crtc->lut)) {
				xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Failed to allocate memory for lut %d\n", i);
				return FALSE;
			}

			/* Copy the last known values. */
			if (crtc->lut_values_valid) {
				nouveau_bo_map(crtc->lut, NOUVEAU_BO_WR);
				memcpy(crtc->lut->map, crtc->lut_values,
				       4 * 256 * sizeof(uint16_t));
				nouveau_bo_unmap(crtc->lut);
			}
		}
	}

	return TRUE;
}

/*
 * Unmap the framebuffer and MMIO memory.
 */

static Bool
NVUnmapMem(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	if (!pNv->dev) {
		pci_device_unmap_range(pNv->PciInfo, pNv->VRAMMap,
				       pNv->PciInfo->regions[1].size);
	}

#ifdef XF86DRM_MODE
	if (pNv->kms_enable)
		drmmode_remove_fb(pScrn);
#endif
	nouveau_bo_ref(NULL, &pNv->scanout);
	nouveau_bo_ref(NULL, &pNv->offscreen);
	nouveau_bo_ref(NULL, &pNv->GART);
	nouveau_bo_ref(NULL, &pNv->Cursor);
	nouveau_bo_ref(NULL, &pNv->Cursor2);

	/* Again not the most ideal way. */
	if (pNv->Architecture == NV_ARCH_50 && !pNv->kms_enable) {
		int i;

		for(i = 0; i < 2; i++) {
			nouveauCrtcPtr crtc = pNv->crtc[i];

			nouveau_bo_ref(NULL, &crtc->lut);
		}
	}

	return TRUE;
}


/*
 * Restore the initial (text) mode.
 */
static void 
NVRestore(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	NVLockVgaCrtcs(pNv, false);

	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Restoring encoders\n");
	for (i = 0; i < pNv->vbios->dcb->entries; i++)
		nv_encoder_restore(pScrn, &pNv->encoders[i]);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Restoring crtcs\n");
	for (i = 0; i < xf86_config->num_crtc; i++)
		xf86_config->crtc[i]->funcs->restore(xf86_config->crtc[i]);

	nouveau_hw_save_vga_fonts(pScrn, 0);

	if (pNv->twoHeads) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Restoring CRTC_OWNER to %d.\n", pNv->vtOWNER);
		NVSetOwner(pNv, pNv->vtOWNER);
	}

	NVLockVgaCrtcs(pNv, true);
}

static void
NVLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
	      LOCO * colors, VisualPtr pVisual)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int c;
	int i, j, index;
	CARD16 lut_r[256], lut_g[256], lut_b[256];

	for (c = 0; c < xf86_config->num_crtc; c++) {
		xf86CrtcPtr crtc = xf86_config->crtc[c];

		/* code borrowed from intel driver */
		switch (pScrn->depth) {
		case 15:
			for (i = 0; i < numColors; i++) {
				index = indices[i];
				for (j = 0; j < 8; j++) {
					lut_r[index * 8 + j] = colors[index].red << 8;
					lut_g[index * 8 + j] = colors[index].green << 8;
					lut_b[index * 8 + j] = colors[index].blue << 8;
				}
			}
		case 16:
			for (i = 0; i < numColors; i++) {
				index = indices[i];

				if (i <= 31) {
					for (j = 0; j < 8; j++) {
						lut_r[index * 8 + j] = colors[index].red << 8;
						lut_b[index * 8 + j] = colors[index].blue << 8;
					}
				}

				for (j = 0; j < 4; j++) {
					lut_g[index * 4 + j] = colors[index].green << 8;
				}
			}
		default:
			for (i = 0; i < numColors; i++) {
				index = indices[i];
				lut_r[index] = colors[index].red << 8;
				lut_g[index] = colors[index].green << 8;
				lut_b[index] = colors[index].blue << 8;
			}
			break;
		}

		if (crtc->randr_crtc)
			/* Make the change through RandR */
			RRCrtcGammaSet(crtc->randr_crtc, lut_r, lut_g, lut_b);
	}
}

/* Mandatory */

/* This gets called at the start of each server generation */
static Bool
NVScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	int ret;
	VisualPtr visual;
	unsigned char *FBStart;
	int displayWidth;

	if (!pNv->NoAccel) {
		if (!NVInitDma(pScrn) || !NVAccelCommonInit(pScrn)) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Error initialising acceleration.  "
				   "Falling back to NoAccel\n");
			pNv->NoAccel = TRUE;
			pNv->ShadowFB = TRUE;
			pNv->exa_driver_pixmaps = FALSE;
			pNv->wfb_enabled = FALSE;
			pNv->tiled_scanout = FALSE;
			pScrn->displayWidth = nv_pitch_align(pNv,
							     pScrn->virtualX,
							     pScrn->depth);
		}
	}

	if (!pNv->NoAccel) {
		if (!pNv->exa_driver_pixmaps)
			NVDRIScreenInit(pScrn);
		else
			nouveau_dri2_init(pScreen);
	}

	/* Allocate and map memory areas we need */
	if (!NVMapMem(pScrn))
		return FALSE;

	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	/* need to point to new screen on server regeneration */
	for (i = 0; i < xf86_config->num_crtc; i++)
		xf86_config->crtc[i]->scrn = pScrn;
	for (i = 0; i < xf86_config->num_output; i++)
		xf86_config->output[i]->scrn = pScrn;

	if (!pNv->kms_enable)
		NVSave(pScrn);

	/*
	 * The next step is to setup the screen's visuals, and initialise the
	 * framebuffer code.  In cases where the framebuffer's default
	 * choices for things like visual layouts and bits per RGB are OK,
	 * this may be as simple as calling the framebuffer's ScreenInit()
	 * function.  If not, the visuals will need to be setup before calling
	 * a fb ScreenInit() function and fixed up after.
	 *
	 * For most PC hardware at depths >= 8, the defaults that fb uses
	 * are not appropriate.  In this driver, we fixup the visuals after.
	 */

	/*
	 * Reset the visual list.
	 */
	miClearVisualTypes();

	/* Setup the visuals we support. */
	if (!miSetVisualTypes(pScrn->depth, 
			      miGetDefaultVisualMask(pScrn->depth),
			      pScrn->rgbBits, pScrn->defaultVisual))
		return FALSE;

	if (!miSetPixmapDepths ())
		return FALSE;

	/*
	 * Call the framebuffer layer's ScreenInit function, and fill in other
	 * pScreen fields.
	 */

	if (pNv->ShadowFB) {
		pNv->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * pScrn->virtualX);
		pNv->ShadowPtr = xalloc(pNv->ShadowPitch * pScrn->virtualY);
		displayWidth = pNv->ShadowPitch / (pScrn->bitsPerPixel >> 3);
		FBStart = pNv->ShadowPtr;
	} else
	if (pNv->NoAccel) {
		pNv->ShadowPtr = NULL;
		displayWidth = pScrn->displayWidth;
		nouveau_bo_map(pNv->scanout, NOUVEAU_BO_RDWR);
		FBStart = pNv->scanout->map;
		nouveau_bo_unmap(pNv->scanout);
	} else {
		pNv->ShadowPtr = NULL;
		displayWidth = pScrn->displayWidth;
		FBStart = NULL;
	}

	switch (pScrn->bitsPerPixel) {
	case 16:
	case 32:
	if (pNv->wfb_enabled) {
		ret = wfbScreenInit(pScreen, FBStart, pScrn->virtualX,
				    pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
				    displayWidth, pScrn->bitsPerPixel,
				    nouveau_wfb_setup_wrap,
				    nouveau_wfb_finish_wrap);
	} else {
		ret = fbScreenInit(pScreen, FBStart, pScrn->virtualX,
				   pScrn->virtualY, pScrn->xDpi, pScrn->yDpi,
				   displayWidth, pScrn->bitsPerPixel);
	}
		break;
	default:
		xf86DrvMsg(scrnIndex, X_ERROR,
			   "Internal error: invalid bpp (%d) in NVScreenInit\n",
			   pScrn->bitsPerPixel);
		ret = FALSE;
		break;
	}
	if (!ret)
		return FALSE;

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

	if (pNv->wfb_enabled)
		wfbPictureInit (pScreen, 0, 0);
	else
		fbPictureInit (pScreen, 0, 0);

	xf86SetBlackWhitePixels(pScreen);

	if (!pNv->NoAccel) {
		if (!nouveau_exa_init(pScreen))
			return FALSE;
	} else if (pNv->VRAMPhysicalSize / 2 < NOUVEAU_ALIGN(pScrn->virtualX, 64) * NOUVEAU_ALIGN(pScrn->virtualY, 64) * (pScrn->bitsPerPixel >> 3)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "The virtual screen size's resolution is too big for the video RAM framebuffer at this colour depth.\n");
		return FALSE;
	}


	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);
	xf86SetSilkenMouse(pScreen);

	/* Finish DRI init */
	if (!NVDRIFinishScreenInit(pScrn, false)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "[dri] NVDRIFinishScreenInit failed, disbling DRI\n");
		NVDRICloseScreen(pScrn);
	}

	/* 
	 * Initialize software cursor.
	 * Must precede creation of the default colormap.
	 */
	miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

	/*
	 * Initialize HW cursor layer. 
	 * Must follow software cursor initialization.
	 */
	if (pNv->HWCursor) { 
		if (pNv->kms_enable)
			ret = drmmode_cursor_init(pScreen);
		else
		if (pNv->Architecture < NV_ARCH_50)
			ret = NVCursorInitRandr12(pScreen);
		else
			ret = NV50CursorInit(pScreen);

		if (ret != TRUE) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
				"Hardware cursor initialization failed\n");
			pNv->HWCursor = FALSE;
		}
	}

	if (pNv->ShadowFB)
		ShadowFBInit(pScreen, NVRefreshArea);

	pScrn->memPhysBase = pNv->VRAMPhysical;
	pScrn->fbOffset = 0;

	NVInitVideo(pScreen);

	/* Wrap the block handler here, if we do it after the EnterVT we
	 * can end up in the unfortunate case where we've wrapped the
	 * xf86RotateBlockHandler which sometimes is not expecting to
	 * be in the wrap chain and calls a NULL pointer...
	 */
	pNv->BlockHandler = pScreen->BlockHandler;
	pScreen->BlockHandler = NVBlockHandler;

	pScrn->vtSema = TRUE;
	pScrn->pScreen = pScreen;
	if (pNv->kms_enable)
		drmmode_fbcon_copy(pScrn);

	if (!NVEnterVT(pScrn->scrnIndex, 0))
		return FALSE;

	xf86DPMSInit(pScreen, xf86DPMSSet, 0);

	/* Wrap the current CloseScreen function */
	pScreen->SaveScreen = NVSaveScreen;
	pNv->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = NVCloseScreen;
	pNv->CreateScreenResources = pScreen->CreateScreenResources;
	pScreen->CreateScreenResources = NVCreateScreenResources;

	if (!xf86CrtcScreenInit(pScreen))
		return FALSE;

	/* Initialise default colourmap */
	if (!miCreateDefColormap(pScreen))
		return FALSE;

	/*
	 * Initialize colormap layer.
	 * Must follow initialization of the default colormap 
	 */
	if (!pNv->kms_enable &&
	    !xf86HandleColormaps(pScreen, 256, 8, NVLoadPalette,
				 NULL, CMAP_PALETTED_TRUECOLOR))
		return FALSE;

	/* Report any unused options (only for the first generation) */
	if (serverGeneration == 1)
		xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

	return TRUE;
}

static Bool
NVSaveScreen(ScreenPtr pScreen, int mode)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	bool on = xf86IsUnblank(mode);
	int i;

	if (pScrn->vtSema && pNv->Architecture < NV_ARCH_50) {
		xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);

		for (i = 0; i < xf86_config->num_crtc; i++) {
			struct nouveau_crtc *nv_crtc = to_nouveau_crtc(xf86_config->crtc[i]);

			if (xf86_config->crtc[i]->enabled)
				NVBlankScreen(pNv, nv_crtc->head, !on);
		}
	}

	return TRUE;
}

static void
NVSave(ScrnInfoPtr pScrn)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	NVPtr pNv = NVPTR(pScrn);
	int i;

	if (pNv->Architecture == NV_ARCH_50)
		return;

	NVLockVgaCrtcs(pNv, false);

	nouveau_hw_save_vga_fonts(pScrn, 1);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Saving crtcs\n");
	for (i = 0; i < xf86_config->num_crtc; i++)
		xf86_config->crtc[i]->funcs->save(xf86_config->crtc[i]);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Saving encoders\n");
	for (i = 0; i < pNv->vbios->dcb->entries; i++)
		nv_encoder_save(pScrn, &pNv->encoders[i]);
}
