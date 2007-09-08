/* $XdotOrg: driver/xf86-video-nv/src/nv_driver.c,v 1.21 2006/01/24 16:45:29 aplattner Exp $ */
/* $XConsortium: nv_driver.c /main/3 1996/10/28 05:13:37 kaleb $ */
/*
 * Copyright 1996-1997  David J. McKay
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
 * DAVID J. MCKAY BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Hacked together from mga driver and 3.3.4 NVIDIA driver by Jarno Paananen
   <jpaana@s2.org> */

/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_driver.c,v 1.144 2006/06/16 00:19:32 mvojkovi Exp $ */

#include "nv_include.h"

#include "xf86int10.h"

#include "xf86drm.h"

extern DisplayModePtr xf86ModesAdd(DisplayModePtr Modes, DisplayModePtr Additions);

/*
 * Forward definitions for the functions that make up the driver.
 */
/* Mandatory functions */
static const OptionInfoRec * NVAvailableOptions(int chipid, int busid);
static void    NVIdentify(int flags);
#ifndef XSERVER_LIBPCIACCESS
static Bool    NVProbe(DriverPtr drv, int flags);
#endif /* XSERVER_LIBPCIACCESS */
static Bool    NVPreInit(ScrnInfoPtr pScrn, int flags);
static Bool    NVScreenInit(int Index, ScreenPtr pScreen, int argc,
                            char **argv);
static Bool    NVEnterVT(int scrnIndex, int flags);
static void    NVLeaveVT(int scrnIndex, int flags);
static Bool    NVCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool    NVSaveScreen(ScreenPtr pScreen, int mode);

/* Optional functions */
static void    NVFreeScreen(int scrnIndex, int flags);
static ModeStatus NVValidMode(int scrnIndex, DisplayModePtr mode,
			      Bool verbose, int flags);
#ifdef RANDR
static Bool    NVDriverFunc(ScrnInfoPtr pScrnInfo, xorgDriverFuncOp op,
			      pointer data);
#endif

/* Internally used functions */

static Bool	NVMapMem(ScrnInfoPtr pScrn);
static Bool	NVUnmapMem(ScrnInfoPtr pScrn);
static void	NVSave(ScrnInfoPtr pScrn);
static void	NVRestore(ScrnInfoPtr pScrn);
static Bool	NVModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode);

#ifdef XSERVER_LIBPCIACCESS

#define NOUVEAU_PCI_DEVICE(_vendor_id, _device_id) \
	{ (_vendor_id), (_device_id), PCI_MATCH_ANY, PCI_MATCH_ANY, 0x00030000, 0x00ffffff, 0 }

static const struct pci_id_match nouveau_device_match[] = {
	NOUVEAU_PCI_DEVICE(PCI_VENDOR_NVIDIA, PCI_MATCH_ANY),
	NOUVEAU_PCI_DEVICE(PCI_VENDOR_NVIDIA_SGS, PCI_MATCH_ANY),
	{ 0, 0, 0 },
};

static Bool NVPciProbe (	DriverPtr 		drv,
				int 			entity_num,
				struct pci_device	*dev,
				intptr_t		match_data	);

#endif /* XSERVER_LIBPCIACCESS */

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
#ifdef XSERVER_LIBPCIACCESS
	NULL,
#else
	NVProbe,
#endif /* XSERVER_LIBPCIACCESS */
	NVAvailableOptions,
	NULL,
	0,
	NULL,
#ifdef XSERVER_LIBPCIACCESS
	nouveau_device_match,
	NVPciProbe
#endif /* XSERVER_LIBPCIACCESS */
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

/*
 * List of symbols from other modules that this module references.  This
 * list is used to tell the loader that it is OK for symbols here to be
 * unresolved providing that it hasn't been told that they haven't been
 * told that they are essential via a call to xf86LoaderReqSymbols() or
 * xf86LoaderReqSymLists().  The purpose is this is to avoid warnings about
 * unresolved symbols that are not required.
 */

static const char *vgahwSymbols[] = {
    "vgaHWUnmapMem",
    "vgaHWDPMSSet",
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIndex",
    "vgaHWInit",
    "vgaHWMapMem",
    "vgaHWProtect",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWSaveScreen",
    NULL
};

static const char *fbSymbols[] = {
    "fbPictureInit",
    "fbScreenInit",
    NULL
};

static const char *exaSymbols[] = {
    "exaDriverInit",
    "exaOffscreenInit",
    NULL
};

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    "xf86InitCursor",
    NULL
};

static const char *ddcSymbols[] = {
    "xf86PrintEDID",
    "xf86DoEDID_DDC2",
    "xf86SetDDCproperties",
    NULL
};

static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeFree",
    "vbeDoEDID",
    NULL
};

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

static const char *shadowSymbols[] = {
    "ShadowFBInit",
    NULL
};

static const char *int10Symbols[] = {
    "xf86FreeInt10",
    "xf86InitInt10",
    NULL
};

const char *drmSymbols[] = {
    "drmOpen", 
    "drmAddBufs",
    "drmAddMap",
    "drmAgpAcquire",
    "drmAgpVersionMajor",
    "drmAgpVersionMinor",
    "drmAgpAlloc",
    "drmAgpBind",
    "drmAgpEnable",
    "drmAgpFree",
    "drmAgpRelease",
    "drmAgpUnbind",
    "drmAuthMagic",
    "drmCommandNone",
    "drmCommandWrite",
    "drmCommandWriteRead",
    "drmCreateContext",
    "drmCtlInstHandler",
    "drmCtlUninstHandler",
    "drmDestroyContext",
    "drmFreeVersion",
    "drmGetInterruptFromBusID",
    "drmGetLibVersion",
    "drmGetVersion",
    NULL
};

const char *driSymbols[] = {
    "DRICloseScreen",
    "DRICreateInfoRec",
    "DRIDestroyInfoRec",
    "DRIFinishScreenInit",
    "DRIGetSAREAPrivate",
    "DRILock",
    "DRIQueryVersion",
    "DRIScreenInit",
    "DRIUnlock",
    "GlxSetVisualConfigs",
    "DRICreatePCIBusID",
    NULL
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


/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */
static int pix24bpp = 0;

static Bool
NVGetRec(ScrnInfoPtr pScrn)
{
    /*
     * Allocate an NVRec, and hook it into pScrn->driverPrivate.
     * pScrn->driverPrivate is initialised to NULL, so we can check if
     * the allocation has already been done.
     */
    if (pScrn->driverPrivate != NULL)
        return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(NVRec), 1);
    /* Initialise it */

    return TRUE;
}

static void
NVFreeRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate == NULL)
        return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}


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
		 * Modules that this driver always requires may be loaded here
		 * by calling LoadSubModule().
		 */
		/*
		 * Tell the loader about symbols from other modules that this module
		 * might refer to.
		 */
		LoaderRefSymLists(vgahwSymbols, exaSymbols, fbSymbols,
#ifdef XF86DRI
				drmSymbols, 
#endif
				ramdacSymbols, shadowSymbols,
				i2cSymbols, ddcSymbols, vbeSymbols,
				int10Symbols, NULL);

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


static Bool
NVGetScrnInfoRec(PciChipsets *chips, int chip)
{
    ScrnInfoPtr pScrn;

    pScrn = xf86ConfigPciEntity(NULL, 0, chip,
                                chips, NULL, NULL, NULL,
                                NULL, NULL);

    if(!pScrn) return FALSE;

    pScrn->driverVersion    = NV_VERSION;
    pScrn->driverName       = NV_DRIVER_NAME;
    pScrn->name             = NV_NAME;

#ifndef XSERVER_LIBPCIACCESS
	pScrn->Probe = NVProbe;
#else
	pScrn->Probe = NULL;
#endif
    pScrn->PreInit          = NVPreInit;
    pScrn->ScreenInit       = NVScreenInit;
    pScrn->SwitchMode       = NVSwitchMode;
    pScrn->AdjustFrame      = NVAdjustFrame;
    pScrn->EnterVT          = NVEnterVT;
    pScrn->LeaveVT          = NVLeaveVT;
    pScrn->FreeScreen       = NVFreeScreen;
    pScrn->ValidMode        = NVValidMode;

    return TRUE;
}

/* This returns architecture in hexdecimal, so NV40 is 0x40 */
static int NVGetArchitecture (volatile CARD32 *regs)
{
	int architecture = 0;

	/* We're dealing with >=NV10 */
	if ((regs[0] & 0x0f000000) > 0 ) {
		/* Bit 27-20 contain the architecture in hex */
		architecture = (regs[0] & 0xff00000) >> 20;
	/* NV04 or NV05 */
	} else if ((regs[0] & 0xff00fff0) == 0x20004000) {
		architecture = 0x04;
	}

	return architecture;
}

/* Reading the pci_id from the card registers is the most reliable way */
static CARD32 NVGetPCIID (volatile CARD32 *regs)
{
	CARD32 pci_id;

	int architecture = NVGetArchitecture(regs);

	/* Dealing with an unknown or unsupported card */
	if (architecture == 0) {
		return 0;
	}

	if (architecture >= 0x40)
		pci_id = regs[0x88000/4];
	else
		pci_id = regs[0x1800/4];

	/* A pci-id can be inverted, we must correct this */
	if ((pci_id & 0xffff) == PCI_VENDOR_NVIDIA) {
		pci_id = (PCI_VENDOR_NVIDIA << 16) | (pci_id >> 16);
	} else if ((pci_id & 0xffff) == PCI_VENDOR_NVIDIA_SGS) {
		pci_id = (PCI_VENDOR_NVIDIA_SGS << 16) | (pci_id >> 16);
	/* Checking endian issues */
	} else {
		/* PCI_VENDOR_NVIDIA = 0x10DE */
		if ((pci_id & (0xffff << 16)) == (0xDE10 << 16)) { /* wrong endian */
			pci_id = (PCI_VENDOR_NVIDIA << 16) | ((pci_id << 8) & 0x0000ff00) |
				((pci_id >> 8) & 0x000000ff);
		/* PCI_VENDOR_NVIDIA_SGS = 0x12D2 */
		} else if ((pci_id & (0xffff << 16)) == (0xD212 << 16)) { /* wrong endian */
			pci_id = (PCI_VENDOR_NVIDIA_SGS << 16) | ((pci_id << 8) & 0x0000ff00) |
				((pci_id >> 8) & 0x000000ff);
		}
	}

	return pci_id;
}

#ifdef XSERVER_LIBPCIACCESS

static Bool NVPciProbe (	DriverPtr 		drv,
				int 			entity_num,
				struct pci_device	*dev,
				intptr_t		match_data	)
{
	ScrnInfoPtr pScrn = NULL;

	volatile CARD32 *regs = NULL;

	/* Temporary mapping to discover the architecture */
	pci_device_map_range(dev, PCI_DEV_MEM_BASE(dev, 0), 0x90000, 0, &regs);

	/* Bit 27-20 contain the architecture in hex */
	char architecture = (regs[0] & 0xff00000) >> 20;

	CARD32 pci_id = NVGetPCIID(regs);

	pci_device_unmap_range(dev, regs, 0x90000);

	/* Currently NV04 up to NV83 is supported */
	/* For safety the fictional NV8F is used */
	if (architecture >= 0x04 && architecture <= 0x8F) {

		/* At this stage the pci_id should be ok, so we generate this to avoid list duplication */
		/* AGP bridge chips need their bridge chip id to be detected */
		const PciChipsets NVChipsets[] = {
			{ pci_id, PCI_DEV_PCI_ID(dev), RES_SHARED_VGA },
			{ -1, -1, RES_UNDEFINED }
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
			pScrn->ValidMode        = NVValidMode;

			return TRUE;
		}
	}

	return FALSE;
}

#endif /* XSERVER_LIBPCIACCESS */

#define MAX_CHIPS MAXSCREENS

#ifndef XSERVER_LIBPCIACCESS
/* Mandatory */
static Bool
NVProbe(DriverPtr drv, int flags)
{
	int i;
	GDevPtr *devSections;
	int *usedChips;
	SymTabRec NVChipsets[MAX_CHIPS + 1];
	PciChipsets NVPciChipsets[MAX_CHIPS + 1];
	pciVideoPtr *ppPci;
	int numDevSections;
	int numUsed;
	Bool foundScreen = FALSE;

	if ((numDevSections = xf86MatchDevice(NV_DRIVER_NAME, &devSections)) <= 0) 
		return FALSE;  /* no matching device section */

	if (!(ppPci = xf86GetPciVideoInfo())) 
		return FALSE;  /* no PCI cards found */

	numUsed = 0;

	/* Create the NVChipsets and NVPciChipsets from found devices */
	while (*ppPci && (numUsed < MAX_CHIPS)) {
		if (((*ppPci)->vendor == PCI_VENDOR_NVIDIA_SGS) || 
			((*ppPci)->vendor == PCI_VENDOR_NVIDIA)) 
		{
			volatile CARD32 *regs;
			CARD32 pcicmd;

			PCI_DEV_READ_LONG(*ppPci, PCI_CMD_STAT_REG, &pcicmd);
			/* Enable reading memory? */
			PCI_DEV_WRITE_LONG(*ppPci, PCI_CMD_STAT_REG, pcicmd | PCI_CMD_MEM_ENABLE);

			regs = xf86MapPciMem(-1, VIDMEM_MMIO, PCI_DEV_TAG(*ppPci), PCI_DEV_MEM_BASE(*ppPci, 0), 0x90000);
			int pciid = NVGetPCIID(regs);

			int architecture = NVGetArchitecture(regs);
			char name[25];
			sprintf(name, "NVIDIA NV%02X", architecture);
			/* NV04 upto NV83 is supported, NV8F is fictive limit */
			if (architecture >= 0x04 && architecture <= 0x8F) {
				NVChipsets[numUsed].token = pciid;
				NVChipsets[numUsed].name = name;
				NVPciChipsets[numUsed].numChipset = pciid;
				/* AGP bridge chips need their bridge chip id to be detected */
				NVPciChipsets[numUsed].PCIid = PCI_DEV_PCI_ID(*ppPci);
				NVPciChipsets[numUsed].resList = RES_SHARED_VGA;
				numUsed++;
			}
			xf86UnMapVidMem(-1, (pointer)regs, 0x90000);

			/* Reset previous state */
			PCI_DEV_WRITE_LONG(*ppPci, PCI_CMD_STAT_REG, pcicmd);
		}
		ppPci++;
	}

	/* terminate the list */
	NVChipsets[numUsed].token = -1;
	NVChipsets[numUsed].name = NULL; 
	NVPciChipsets[numUsed].numChipset = -1;
	NVPciChipsets[numUsed].PCIid = -1;
	NVPciChipsets[numUsed].resList = RES_UNDEFINED;

	numUsed = xf86MatchPciInstances(NV_NAME, 0, NVChipsets, NVPciChipsets,
					devSections, numDevSections, drv,
					&usedChips);

	if (numUsed <= 0) {
		return FALSE;
	}

	if (flags & PROBE_DETECT) {
		foundScreen = TRUE;
	} else {
		for (i = 0; i < numUsed; i++) {
			pciVideoPtr pPci;

			pPci = xf86GetPciInfoForEntity(usedChips[i]);
			if (NVGetScrnInfoRec(NVPciChipsets, usedChips[i])) {
				foundScreen = TRUE;
			}
		}
	}

	xfree(devSections);
	xfree(usedChips);

	return foundScreen;
}
#endif /* XSERVER_LIBPCIACCESS */

/*
 * This function is needed by the XF86VideMode extension which is used by
 * the current pre-randr clients. The API covers only one screen, but
 * implementing the latest modesetting framework like done in the Intel
 * driver is more than a few lines of patch, the randr-1.2 branch in its
 * current form cannot the mode switching in a perfect way right now.
 *
 * As there are effors to bring modesetting into the kernel, controlled
 * thru the drm module, of which nouveu currently requires its own version,
 * one could even try to go one step further and try to bring the nouveau
 * modesetting into the nouveau kernel module.c (as a first step which does
 * not require a kernel patch), which would increase the chances that the
 * text console is properly restored after X dies as the kernel can simply
 * restore the text console when the process which has changed modes thru
 * /dev/drm has been disconnected from the device.
 *
 * The current implementation simply tries to set each crtc to the mode
 * for which the application asks for, hoping that one of them gives a
 * usable monitor display (no error handling implemented), and sets
 * the viewport of each crtc to (0,0), which means essentially clone
 * mode with all monitors which managed to switch to the mode showing
 * top left area of the framebuffer memory if the application's window
 * is there. This is essentially what the Intel driver did in earlyer
 * versions. To restore a LeftOf/RightOf layout, you two randr calls
 * seem to be neccessary, one which sets the reversed layout, followed
 * by one which sets the desired layout:
 *
 * xrandr --output Digital-1 --left-of Digital-0
 * xrandr --output Digital-0 --left-of Digital-1
 *
 * FIXME: This could be fixed by getting the current viewports for the
 * CRTCs and use these during mode settings, or (preferably) by getting
 * the current screen layout and adapting the new viewports so that
 * a new, continuos screen layout with the same monitor arrangement,
 * but in the new mode is set up.
 */
Bool
NVSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
    NVPtr pNv = NVPTR(pScrn);
    Bool ret = TRUE;

    if (pNv->randr12_enable) {
	NVFBLayout *pLayout = &pNv->CurrentLayout;

	if (pLayout->mode != mode) {
		/* This needs to be fixed with error handling */
		NVSetMode(pScrn, mode);
		pLayout->mode = mode;
	}

	pLayout->mode = mode;
	return ret;
    } else {
	return NVModeInit(xf86Screens[scrnIndex], mode);
    }
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
    int startAddr;
    NVPtr pNv = NVPTR(pScrn);
    NVFBLayout *pLayout = &pNv->CurrentLayout;

    if (pNv->randr12_enable) {
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(pScrn);
	int startAddr;
	xf86CrtcPtr crtc = config->output[config->compat_output]->crtc;
	
	if (crtc && crtc->enabled) {
	    NVCrtcSetBase(crtc, x, y);
	}
    } else {
	startAddr = (((y*pLayout->displayWidth)+x)*(pLayout->bitsPerPixel/8));
	startAddr += pNv->FB->offset;
	NVSetStartAddress(pNv, startAddr);
    }
}

void
NVResetCrtcConfig(ScrnInfoPtr pScrn, int set)
{
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(pScrn);
	NVPtr pNv = NVPTR(pScrn);
	int i;
	CARD32 val = 0;

	for (i = 0; i < config->num_crtc; i++) {
		xf86CrtcPtr crtc = config->crtc[i];
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		if (set) {
			NVCrtcRegPtr regp;

			regp = &pNv->ModeReg.crtc_reg[nv_crtc->crtc];
			val = regp->head;
		}

		nvWriteCRTC(pNv, nv_crtc->crtc, NV_CRTC_FSEL, val);
	}
}

static Bool
NV50AcquireDisplay(ScrnInfoPtr pScrn)
{
	if (!NV50DispInit(pScrn))
		return FALSE;
	if (!NV50CursorAcquire(pScrn))
		return FALSE;
	xf86SetDesiredModes(pScrn);

	return TRUE;
}

static Bool
NV50ReleaseDisplay(ScrnInfoPtr pScrn)
{
	NV50CursorRelease(pScrn);
	NV50DispShutdown(pScrn);

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
    
    if (pNv->randr12_enable) { 
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;
	pScrn->vtSema = TRUE;

	if (pNv->Architecture == NV_ARCH_50) {
		if (!NV50AcquireDisplay(pScrn))
			return FALSE;
		return TRUE;
	}

	/* Save the current state */
	if (pNv->SaveGeneration != serverGeneration) {
		pNv->SaveGeneration = serverGeneration;
		NVSave(pScrn);
	}

	for (i = 0; i < xf86_config->num_crtc; i++) {
		NVCrtcLockUnlock(xf86_config->crtc[i], 0);
	}

	NVResetCrtcConfig(pScrn, 0);
	if (!xf86SetDesiredModes(pScrn))
		return FALSE;
	NVResetCrtcConfig(pScrn, 1);

    } else {
	if (!NVModeInit(pScrn, pScrn->currentMode))
	    return FALSE;

    }
    NVAdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
    if(pNv->overlayAdaptor)
	NVResetVideo(pScrn);
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

    if (pNv->Architecture == NV_ARCH_50) {
	NV50ReleaseDisplay(pScrn);
	return;
    }
    NVSync(pScrn);
    NVRestore(pScrn);
    if (!pNv->randr12_enable)
	NVLockUnlock(pNv, 1);
}



static void 
NVBlockHandler (
    int i, 
    pointer blockData, 
    pointer pTimeout,
    pointer pReadmask
)
{
    ScreenPtr     pScreen = screenInfo.screens[i];
    ScrnInfoPtr   pScrnInfo = xf86Screens[i];
    NVPtr         pNv = NVPTR(pScrnInfo);

    if (pNv->DMAKickoffCallback)
        (*pNv->DMAKickoffCallback)(pNv);
    
    pScreen->BlockHandler = pNv->BlockHandler;
    (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);
    pScreen->BlockHandler = NVBlockHandler;

    if (pNv->VideoTimerCallback) 
        (*pNv->VideoTimerCallback)(pScrnInfo, currentTime.milliseconds);

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

    if (pScrn->vtSema) {
        pScrn->vtSema = FALSE;
	if (pNv->Architecture == NV_ARCH_50) {
	    NV50ReleaseDisplay(pScrn);
	} else {
	    NVSync(pScrn);
	    NVRestore(pScrn);
	    if (!pNv->randr12_enable)
		NVLockUnlock(pNv, 1);
	}
    }

    NVUnmapMem(pScrn);
    vgaHWUnmapMem(pScrn);
    if (pNv->CursorInfoRec)
        xf86DestroyCursorInfoRec(pNv->CursorInfoRec);
    if (pNv->ShadowPtr)
        xfree(pNv->ShadowPtr);
    if (pNv->overlayAdaptor)
	xfree(pNv->overlayAdaptor);
    if (pNv->blitAdaptor)
        xfree(pNv->blitAdaptor);

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
    if (xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	vgaHWFreeHWRec(xf86Screens[scrnIndex]);
    NVFreeRec(xf86Screens[scrnIndex]);
}


/* Checks if a mode is suitable for the selected chipset. */

/* Optional */
static ModeStatus
NVValidMode(int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
    NVPtr pNv = NVPTR(xf86Screens[scrnIndex]);

    if(pNv->fpWidth && pNv->fpHeight)
      if((pNv->fpWidth < mode->HDisplay) || (pNv->fpHeight < mode->VDisplay))
        return (MODE_PANEL);

    return (MODE_OK);
}

static void
nvProbeDDC(ScrnInfoPtr pScrn, int index)
{
    vbeInfoPtr pVbe;

    if (xf86LoadSubModule(pScrn, "vbe")) {
        pVbe = VBEInit(NULL,index);
        ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
	vbeFree(pVbe);
    }
}


Bool NVI2CInit(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	char *mod = "i2c";

	if (xf86LoadSubModule(pScrn, mod)) {
		xf86LoaderReqSymLists(i2cSymbols,NULL);

		mod = "ddc";
		if(xf86LoadSubModule(pScrn, mod)) {
			xf86LoaderReqSymLists(ddcSymbols, NULL);
			/* randr-1.2 clients have their DDC's initialized elsewhere */
			if (pNv->randr12_enable) {
				return TRUE;
			} else {
				return NVDACi2cInit(pScrn);
			}
		} 
	}

	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		"Couldn't load %s module.  DDC probing can't be done\n", mod);

	return FALSE;
}

static Bool NVPreInitDRI(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

    	if (!NVDRIGetVersion(pScrn))
		return FALSE;

 	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"[dri] Found DRI library version %d.%d.%d and kernel"
		" module version %d.%d.%d\n",
		pNv->pLibDRMVersion->version_major,
		pNv->pLibDRMVersion->version_minor,
		pNv->pLibDRMVersion->version_patchlevel,
		pNv->pKernelDRMVersion->version_major,
		pNv->pKernelDRMVersion->version_minor,
		pNv->pKernelDRMVersion->version_patchlevel);

	return TRUE;
}


static Bool
nv_xf86crtc_resize(ScrnInfoPtr scrn, int width, int height)
{
	scrn->virtualX = width;
	scrn->virtualY = height;
	return TRUE;
}

static const xf86CrtcConfigFuncsRec nv_xf86crtc_config_funcs = {
	nv_xf86crtc_resize
};

#define NVPreInitFail(fmt, args...) do {                                    \
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "%d: "fmt, __LINE__, ##args); \
	if (pNv->pInt10)                                                    \
		xf86FreeInt10(pNv->pInt10);                                 \
	NVFreeRec(pScrn);                                                   \
	return FALSE;                                                       \
} while(0)

/* Mandatory */
Bool
NVPreInit(ScrnInfoPtr pScrn, int flags)
{
    xf86CrtcConfigPtr xf86_config;
    NVPtr pNv;
    MessageType from;
    int i, max_width, max_height;
    ClockRangePtr clockRanges;
    const char *s;
    int config_mon_rates = FALSE;
    int num_crtc;

    if (flags & PROBE_DETECT) {
        EntityInfoPtr pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

        if (!pEnt)
            return FALSE;

        i = pEnt->index;
        xfree(pEnt);

        nvProbeDDC(pScrn, i);
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
    if (!NVGetRec(pScrn)) {
	return FALSE;
    }
    pNv = NVPTR(pScrn);

    /* Get the entity, and make sure it is PCI. */
    pNv->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);
    if (pNv->pEnt->location.type != BUS_PCI)
	return FALSE;
 
    /* Find the PCI info for this screen */
    pNv->PciInfo = xf86GetPciInfoForEntity(pNv->pEnt->index);
#ifndef XSERVER_LIBPCIACCESS
    pNv->PciTag = pciTag(pNv->PciInfo->bus, pNv->PciInfo->device,
			  pNv->PciInfo->func);
#endif /* XSERVER_LIBPCIACCESS */

    pNv->Primary = xf86IsPrimaryPci(pNv->PciInfo);

    /* Initialize the card through int10 interface if needed */
    if (xf86LoadSubModule(pScrn, "int10")) {
 	xf86LoaderReqSymLists(int10Symbols, NULL);
#if !defined(__alpha__) && !defined(__powerpc__)
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Initializing int10\n");
        pNv->pInt10 = xf86InitInt10(pNv->pEnt->index);
#endif
    }
   
    xf86SetOperatingState(resVgaIo, pNv->pEnt->index, ResUnusedOpr);
    xf86SetOperatingState(resVgaMem, pNv->pEnt->index, ResDisableOpr);

    /* Set pScrn->monitor */
    pScrn->monitor = pScrn->confScreen->monitor;

	volatile CARD32 *regs = NULL;
#ifdef XSERVER_LIBPCIACCESS
	pci_device_map_range(pNv->PciInfo, PCI_DEV_MEM_BASE(pNv->PciInfo, 0), 0x90000, 0, &regs);
	pNv->Chipset = NVGetPCIID(regs) & 0xffff;
	pNv->NVArch = NVGetArchitecture(regs);
	pci_device_unmap_range(pNv->PciInfo, regs, 0x90000);
#else
	CARD32 pcicmd;
	PCI_DEV_READ_LONG(pNv->PciInfo, PCI_CMD_STAT_REG, &pcicmd);
	/* Enable reading memory? */
	PCI_DEV_WRITE_LONG(pNv->PciInfo, PCI_CMD_STAT_REG, pcicmd | PCI_CMD_MEM_ENABLE);
	regs = xf86MapPciMem(-1, VIDMEM_MMIO, pNv->PciTag, PCI_DEV_MEM_BASE(pNv->PciInfo, 0), 0x90000);
	pNv->Chipset = NVGetPCIID(regs) & 0xffff;
	pNv->NVArch = NVGetArchitecture(regs);
	xf86UnMapVidMem(-1, (pointer)regs, 0x90000);
	/* Reset previous state */
	PCI_DEV_WRITE_LONG(pNv->PciInfo, PCI_CMD_STAT_REG, pcicmd);
#endif /* XSERVER_LIBPCIACCESS */

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
	if (pNv->NVArch >= 0x50) {
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

    /*
     * The first thing we should figure out is the depth, bpp, etc.
     */

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb)) {
	NVPreInitFail("\n");
    } else {
	/* Check that the returned depth is one we support */
	switch (pScrn->depth) {
            case 8:
            case 15:
            case 16:
            case 24:
                /* OK */
                break;
            default:
		NVPreInitFail("Given depth (%d) is not supported by this driver\n",
			      pScrn->depth);
	}
    }
    xf86PrintDepthBpp(pScrn);

    /* Get the depth24 pixmap format */
    if (pScrn->depth == 24 && pix24bpp == 0)
	pix24bpp = xf86GetBppFromDepth(pScrn, 24);

    /*
     * This must happen after pScrn->display has been set because
     * xf86SetWeight references it.
     */
    if (pScrn->depth > 8) {
	/* The defaults are OK for us */
	rgb zeros = {0, 0, 0};

	if (!xf86SetWeight(pScrn, zeros, zeros)) {
	    NVPreInitFail("\n");
	}
    }

    if (!xf86SetDefaultVisual(pScrn, -1)) {
	NVPreInitFail("\n");
    } else {
	/* We don't currently support DirectColor at > 8bpp */
	if (pScrn->depth > 8 && (pScrn->defaultVisual != TrueColor)) {
	    NVPreInitFail("Given default visual"
		       " (%s) is not supported at depth %d\n",
		       xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
	    
	}
    }

    /* The vgahw module should be loaded here when needed */
    if (!xf86LoadSubModule(pScrn, "vgahw")) {
	NVPreInitFail("\n");
    }
    
    xf86LoaderReqSymLists(vgahwSymbols, NULL);

    /*
     * Allocate a vgaHWRec
     */
    if (!vgaHWGetHWRec(pScrn)) {
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

    /* Set the bits per RGB for 8bpp mode */
    if (pScrn->depth == 8)
	pScrn->rgbBits = 8;

    from = X_DEFAULT;

    if (pNv->Architecture == NV_ARCH_50) {
	    pNv->randr12_enable = TRUE;
    } else {
	pNv->randr12_enable = FALSE;
	if (xf86ReturnOptValBool(pNv->Options, OPTION_RANDR12, FALSE)) {
	    pNv->randr12_enable = TRUE;
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Randr1.2 support %sabled\n", pNv->randr12_enable ? "en" : "dis");

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
    
    pNv->Rotate = 0;
    pNv->RandRRotation = FALSE;
    if ((s = xf86GetOptValString(pNv->Options, OPTION_ROTATE))) {
      if(!xf86NameCmp(s, "CW")) {
	pNv->ShadowFB = TRUE;
	pNv->NoAccel = TRUE;
	pNv->HWCursor = FALSE;
	pNv->Rotate = 1;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"Rotating screen clockwise - acceleration disabled\n");
      } else
      if(!xf86NameCmp(s, "CCW")) {
	pNv->ShadowFB = TRUE;
	pNv->NoAccel = TRUE;
	pNv->HWCursor = FALSE;
	pNv->Rotate = -1;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"Rotating screen counter clockwise - acceleration disabled\n");
      } else
      if(!xf86NameCmp(s, "RandR")) {
#ifdef RANDR
	pNv->ShadowFB = TRUE;
	pNv->NoAccel = TRUE;
	pNv->HWCursor = FALSE;
	pNv->RandRRotation = TRUE;
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		"Using RandR rotation - acceleration disabled\n");
#else
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		"This driver was not compiled with support for the Resize and "
		"Rotate extension.  Cannot honor 'Option \"Rotate\" "
		"\"RandR\"'.\n");
#endif
      } else {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, 
		"\"%s\" is not a valid value for Option \"Rotate\"\n", s);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		"Valid options are \"CW\", \"CCW\", and \"RandR\"\n");
      }
    }

    if(xf86GetOptValInteger(pNv->Options, OPTION_VIDEO_KEY, &(pNv->videoKey))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video key set to 0x%x\n",
                                pNv->videoKey);
    } else {
        pNv->videoKey =  (1 << pScrn->offset.red) | 
                          (1 << pScrn->offset.green) |
        (((pScrn->mask.blue >> pScrn->offset.blue) - 1) << pScrn->offset.blue); 
    }

    if (xf86GetOptValBool(pNv->Options, OPTION_FLAT_PANEL, &(pNv->FlatPanel))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "forcing %s usage\n",
                   pNv->FlatPanel ? "DFP" : "CRTC");
    } else {
        pNv->FlatPanel = -1;   /* autodetect later */
    }

    pNv->FPDither = FALSE;
    if (xf86GetOptValBool(pNv->Options, OPTION_FP_DITHER, &(pNv->FPDither))) 
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "enabling flat panel dither\n");

    //if (xf86GetOptValInteger(pNv->Options, OPTION_CRTC_NUMBER,
   //                          &pNv->CRTCnumber)) 
    //{
	//pNv->crtc_active[0] = FALSE;
	//pNv->crtc_active[1] = FALSE;
    //}


    if (xf86GetOptValInteger(pNv->Options, OPTION_FP_TWEAK, 
                             &pNv->PanelTweak))
    {
        pNv->usePanelTweak = TRUE;
    } else {
        pNv->usePanelTweak = FALSE;
    }
    
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
	if (PCI_DEV_MEM_BASE(pNv->PciInfo, 1) != 0) {
	    pNv->VRAMPhysical = PCI_DEV_MEM_BASE(pNv->PciInfo, 1) & 0xff800000;
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
	if (PCI_DEV_MEM_BASE(pNv->PciInfo, 0) != 0) {
	    pNv->IOAddress = PCI_DEV_MEM_BASE(pNv->PciInfo, 0) & 0xffffc000;
	    from = X_PROBED;
	} else {
	    NVPreInitFail("No valid MMIO address in PCI config space\n");
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "MMIO registers at 0x%lX\n",
	       (unsigned long)pNv->IOAddress);
     
	if (xf86RegisterResources(pNv->pEnt->index, NULL, ResExclusive)) {
		NVPreInitFail("xf86RegisterResources() found resource conflicts\n");
    }

    pNv->alphaCursor = (pNv->Architecture >= NV_ARCH_10) &&
                       ((pNv->Chipset & 0x0ff0) != CHIPSET_NV10);

    if (pNv->randr12_enable) {
	/* Allocate an xf86CrtcConfig */
	xf86CrtcConfigInit(pScrn, &nv_xf86crtc_config_funcs);
	xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	
	max_width = 16384;
	xf86CrtcSetSizeRange(pScrn, 320, 200, max_width, 2048);
    }

    if (NVPreInitDRI(pScrn) == FALSE) {
	NVPreInitFail("\n");
    }

    if (!pNv->randr12_enable) {
	if ((pScrn->monitor->nHsync == 0) && 
	    (pScrn->monitor->nVrefresh == 0))
	    config_mon_rates = FALSE;
	else
	    config_mon_rates = TRUE;
    }

    NVCommonSetup(pScrn);

    if (pNv->randr12_enable) {
	if (pNv->Architecture < NV_ARCH_50) {
	    NVI2CInit(pScrn);
	    
	    num_crtc = pNv->twoHeads ? 2 : 1;
	    for (i = 0; i < num_crtc; i++) {
		nv_crtc_init(pScrn, i);
	    }
	    
	    NvSetupOutputs(pScrn);
	} else {
	    if (!NV50DispPreInit(pScrn))
		NVPreInitFail("\n");
	    if (!NV50CreateOutputs(pScrn))
		NVPreInitFail("\n");
	    NV50DispCreateCrtcs(pScrn);
	}

	if (!xf86InitialConfiguration(pScrn, FALSE))
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

    {
	Gamma zeros = {0.0, 0.0, 0.0};

	if (!xf86SetGamma(pScrn, zeros)) {
	    NVPreInitFail("\n");
	}
    }

    /*
     * Setup the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */

    clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = pNv->MinVClockFreqKHz;
    clockRanges->maxClock = pNv->MaxVClockFreqKHz;
    clockRanges->clockIndex = -1;		/* programmable */
    clockRanges->doubleScanAllowed = TRUE;
    if((pNv->Architecture == NV_ARCH_20) ||
         ((pNv->Architecture == NV_ARCH_10) && 
           ((pNv->Chipset & 0x0ff0) != CHIPSET_NV10) &&
           ((pNv->Chipset & 0x0ff0) != CHIPSET_NV15)))
    {
       /* HW is broken */
       clockRanges->interlaceAllowed = FALSE;
    } else {
       clockRanges->interlaceAllowed = TRUE;
    }

    if(pNv->FlatPanel == 1) {
       clockRanges->interlaceAllowed = FALSE;
       clockRanges->doubleScanAllowed = FALSE;
    }

    if(pNv->Architecture < NV_ARCH_10) {
       max_width = (pScrn->bitsPerPixel > 16) ? 2032 : 2048;
       max_height = 2048;
    } else {
       max_width = (pScrn->bitsPerPixel > 16) ? 4080 : 4096;
       max_height = 4096;
    }

#ifdef M_T_DRIVER
    /* If DFP, add a modeline corresponding to its panel size */
    if (pNv->FlatPanel && !pNv->Television && pNv->fpWidth && pNv->fpHeight) {
	DisplayModePtr Mode;

	Mode = xnfcalloc(1, sizeof(DisplayModeRec));
	Mode = xf86CVTMode(pNv->fpWidth, pNv->fpHeight, 60.00, TRUE, FALSE);
	Mode->type = M_T_DRIVER;
	pScrn->monitor->Modes = xf86ModesAdd(pScrn->monitor->Modes, Mode);

	if (!config_mon_rates) {
	    if (!Mode->HSync)
            	Mode->HSync = ((float) Mode->Clock ) / ((float) Mode->HTotal);
            if (!Mode->VRefresh)
            	Mode->VRefresh = (1000.0 * ((float) Mode->Clock)) /
                    ((float) (Mode->HTotal * Mode->VTotal));

 	    if (Mode->HSync < pScrn->monitor->hsync[0].lo)
            	pScrn->monitor->hsync[0].lo = Mode->HSync;
            if (Mode->HSync > pScrn->monitor->hsync[0].hi)
            	pScrn->monitor->hsync[0].hi = Mode->HSync;
            if (Mode->VRefresh < pScrn->monitor->vrefresh[0].lo)
            	pScrn->monitor->vrefresh[0].lo = Mode->VRefresh;
            if (Mode->VRefresh > pScrn->monitor->vrefresh[0].hi)
            	pScrn->monitor->vrefresh[0].hi = Mode->VRefresh;

	    pScrn->monitor->nHsync = 1;
	    pScrn->monitor->nVrefresh = 1;
	}
    }
#endif

    /*
     * xf86ValidateModes will check that the mode HTotal and VTotal values
     * don't exceed the chipset's limit if pScrn->maxHValue and
     * pScrn->maxVValue are set.  Since our NVValidMode() already takes
     * care of this, we don't worry about setting them here.
     */
    i = xf86ValidateModes(pScrn, pScrn->monitor->Modes,
                          pScrn->display->modes, clockRanges,
                          NULL, 256, max_width,
                          512, 128, max_height,
                          pScrn->display->virtualX,
                          pScrn->display->virtualY,
                          pNv->VRAMPhysicalSize / 2,
                          LOOKUP_BEST_REFRESH);

    if (i == -1) {
	NVPreInitFail("\n");
    }

    /* Prune the modes marked as invalid */
    xf86PruneDriverModes(pScrn);

    if (i == 0 || pScrn->modes == NULL) {
	NVPreInitFail("No valid modes found\n");
    }

    /*
     * Set the CRTC parameters for all of the modes based on the type
     * of mode, and the chipset's interlace requirements.
     *
     * Calling this is required if the mode->Crtc* values are used by the
     * driver and if the driver doesn't provide code to set them.  They
     * are not pre-initialised at all.
     */
    xf86SetCrtcForModes(pScrn, 0);

    /* Set the current mode to the first in the list */
    pScrn->currentMode = pScrn->modes;

    /* Print the list of modes being used */
    xf86PrintModes(pScrn);

    /* Set display resolution */
    xf86SetDpi(pScrn, 0, 0);


    /*
     * XXX This should be taken into account in some way in the mode valdation
     * section.
     */

    if (xf86LoadSubModule(pScrn, "fb") == NULL) {
	NVPreInitFail("\n");
    }

    xf86LoaderReqSymLists(fbSymbols, NULL);
    
    /* Load EXA if needed */
    if (!pNv->NoAccel) {
	if (!xf86LoadSubModule(pScrn, "exa")) {
	    NVPreInitFail("\n");
	}
	xf86LoaderReqSymLists(exaSymbols, NULL);
    }

    /* Load ramdac if needed */
    if (pNv->HWCursor) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) {
	    NVPreInitFail("\n");
	}
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }

    /* Load shadowfb if needed */
    if (pNv->ShadowFB) {
	if (!xf86LoadSubModule(pScrn, "shadowfb")) {
	    NVPreInitFail("\n");
	}
	xf86LoaderReqSymLists(shadowSymbols, NULL);
    }

    pNv->CurrentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    pNv->CurrentLayout.depth = pScrn->depth;
    pNv->CurrentLayout.displayWidth = pScrn->displayWidth;
    pNv->CurrentLayout.weight.red = pScrn->weight.red;
    pNv->CurrentLayout.weight.green = pScrn->weight.green;
    pNv->CurrentLayout.weight.blue = pScrn->weight.blue;
    pNv->CurrentLayout.mode = pScrn->currentMode;

    xf86FreeInt10(pNv->pInt10);

    pNv->pInt10 = NULL;
    return TRUE;
}


/*
 * Map the framebuffer and MMIO memory.
 */

static Bool
NVMapMem(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	pNv->FB = NVAllocateMemory(pNv, NOUVEAU_MEM_FB, pNv->VRAMPhysicalSize/2);
	if (!pNv->FB) {
		ErrorF("Failed to allocate memory for framebuffer!\n");
		return FALSE;
	}
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Allocated %dMiB VRAM for framebuffer + offscreen pixmaps\n",
		   (unsigned int)(pNv->FB->size >> 20));

	/*XXX: have to get these after we've allocated something, otherwise
	 *     they're uninitialised in the DRM!
	 */
	pNv->VRAMSize     = NVDRMGetParam(pNv, NOUVEAU_GETPARAM_FB_SIZE);
	pNv->VRAMPhysical = NVDRMGetParam(pNv, NOUVEAU_GETPARAM_FB_PHYSICAL);
	pNv->AGPSize      = NVDRMGetParam(pNv, NOUVEAU_GETPARAM_AGP_SIZE);
	pNv->AGPPhysical  = NVDRMGetParam(pNv, NOUVEAU_GETPARAM_AGP_PHYSICAL);
	if ( ! pNv->AGPSize ) /*if no AGP*/
		/*use PCI*/
		pNv->SGPhysical  = NVDRMGetParam(pNv, NOUVEAU_GETPARAM_PCI_PHYSICAL);

	int gart_scratch_size;

	if (pNv->AGPSize) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "AGPGART: %dMiB available\n",
			   (unsigned int)(pNv->AGPSize >> 20));

		if (pNv->AGPSize > (16*1024*1024))
			gart_scratch_size = 16*1024*1024;
		else
			gart_scratch_size = pNv->AGPSize;

		}
	else {

		gart_scratch_size = (4 << 20) - (1 << 18) ;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "GART: PCI DMA - using %dKiB\n", gart_scratch_size >> 10);
		
	}

	/*The DRM allocates AGP memory, PCI as a fallback */
	pNv->GARTScratch = NVAllocateMemory(pNv, NOUVEAU_MEM_AGP | NOUVEAU_MEM_PCI_ACCEPTABLE,
							gart_scratch_size);
	if (!pNv->GARTScratch) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Unable to allocate GART memory\n");
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "GART: mapped %dMiB at %p, offset is %d\n",
			   (unsigned int)(pNv->GARTScratch->size >> 20),
			   pNv->GARTScratch->map, pNv->GARTScratch->offset);
	}


	pNv->Cursor = NVAllocateMemory(pNv, NOUVEAU_MEM_FB, 64*1024);
	if (!pNv->Cursor) {
		ErrorF("Failed to allocate memory for hardware cursor\n");
		return FALSE;
	}

	pNv->ScratchBuffer = NVAllocateMemory(pNv, NOUVEAU_MEM_FB,
			pNv->Architecture <NV_ARCH_10 ? 8192 : 16384);
	if (!pNv->ScratchBuffer) {
		ErrorF("Failed to allocate memory for scratch buffer\n");
		return FALSE;
	}

	if (pNv->Architecture >= NV_ARCH_50) {
		pNv->CLUT = NVAllocateMemory(pNv, NOUVEAU_MEM_FB, 0x1000);
		if (!pNv->CLUT) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Failed to allocate memory for CLUT\n");
			return FALSE;
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

	NVFreeMemory(pNv, pNv->FB);
	NVFreeMemory(pNv, pNv->ScratchBuffer);
	NVFreeMemory(pNv, pNv->Cursor);

    return TRUE;
}


/*
 * Initialise a new mode. 
 */

static Bool
NVModeInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    vgaHWPtr hwp = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg;
    NVPtr pNv = NVPTR(pScrn);
    NVRegPtr nvReg;

    /* Initialise the ModeReg values */
    if (!vgaHWInit(pScrn, mode))
	return FALSE;
    pScrn->vtSema = TRUE;

    vgaReg = &hwp->ModeReg;
    nvReg = &pNv->ModeReg;

    if(!NVDACInit(pScrn, mode))
        return FALSE;

    NVLockUnlock(pNv, 0);
    if(pNv->twoHeads) {
        nvWriteVGA(pNv, NV_VGA_CRTCX_OWNER, nvReg->crtcOwner);
        NVLockUnlock(pNv, 0);
    }

    /* Program the registers */
    vgaHWProtect(pScrn, TRUE);

    NVDACRestore(pScrn, vgaReg, nvReg, FALSE);

#if X_BYTE_ORDER == X_BIG_ENDIAN
    /* turn on LFB swapping */
    {
	unsigned char tmp;

	tmp = nvReadVGA(pNv, NV_VGA_CRTCX_SWAPPING);
	tmp |= (1 << 7);
	nvWriteVGA(pNv, NV_VGA_CRTCX_SWAPPING, tmp);
    }
#endif

    if (!pNv->NoAccel)
	    NVResetGraphics(pScrn);

    vgaHWProtect(pScrn, FALSE);

    pNv->CurrentLayout.mode = mode;

    return TRUE;
}

/*
 * Restore the initial (text) mode.
 */
static void 
NVRestore(ScrnInfoPtr pScrn)
{
	vgaHWPtr hwp = VGAHWPTR(pScrn);
	vgaRegPtr vgaReg = &hwp->SavedReg;
	NVPtr pNv = NVPTR(pScrn);
	NVRegPtr nvReg = &pNv->SavedReg;

	if (pNv->randr12_enable) {
		xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
		int i;
		int vgaflags = VGA_SR_CMAP | VGA_SR_MODE;

		for (i = 0; i < xf86_config->num_crtc; i++) {
			NVCrtcLockUnlock(xf86_config->crtc[i], 0);
		}

		for (i = 0; i < xf86_config->num_crtc; i++) {
			xf86_config->crtc[i]->funcs->restore(xf86_config->crtc[i]);
		}

		for (i = 0; i < xf86_config->num_output; i++) {
			xf86_config->output[i]->funcs->restore(xf86_config->
							       output[i]);
		}

#ifndef __powerpc__
		vgaflags |= VGA_SR_FONTS;
#endif
		vgaHWRestore(pScrn, vgaReg, vgaflags);
		vgaHWLock(hwp);

		for (i = 0; i < xf86_config->num_crtc; i++) {
			NVCrtcLockUnlock(xf86_config->crtc[i], 1);
		}
	} else {
		NVLockUnlock(pNv, 0);

		if(pNv->twoHeads) {
			nvWriteVGA(pNv, NV_VGA_CRTCX_OWNER, pNv->crtc_active[1] * 0x3);
			NVLockUnlock(pNv, 0);
		}

		/* Only restore text mode fonts/text for the primary card */
		vgaHWProtect(pScrn, TRUE);
		NVDACRestore(pScrn, vgaReg, nvReg, pNv->Primary);
		if(pNv->twoHeads) {
			nvWriteVGA(pNv, NV_VGA_CRTCX_OWNER, pNv->vtOWNER);
		}
		vgaHWProtect(pScrn, FALSE);
	}
}


#define DEPTH_SHIFT(val, w) ((val << (8 - w)) | (val >> ((w << 1) - 8)))
#define MAKE_INDEX(in, w) (DEPTH_SHIFT(in, w) * 3)

static void
NVLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
	      LOCO * colors, VisualPtr pVisual)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int c;
	NVPtr pNv = NVPTR(pScrn);
	int i, index;

	for (c = 0; c < xf86_config->num_crtc; c++) {
		xf86CrtcPtr crtc = xf86_config->crtc[c];
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
		NVCrtcRegPtr regp;

		regp = &pNv->ModeReg.crtc_reg[nv_crtc->crtc];

		if (crtc->enabled == 0)
			continue;

		switch (pNv->CurrentLayout.depth) {
		case 15:
			for (i = 0; i < numColors; i++) {
				index = indices[i];
				regp->DAC[MAKE_INDEX(index, 5) + 0] =
				    colors[index].red;
				regp->DAC[MAKE_INDEX(index, 5) + 1] =
				    colors[index].green;
				regp->DAC[MAKE_INDEX(index, 5) + 2] =
				    colors[index].blue;
			}
			break;
		case 16:
			for (i = 0; i < numColors; i++) {
				index = indices[i];
				regp->DAC[MAKE_INDEX(index, 6) + 1] =
				    colors[index].green;
				if (index < 32) {
					regp->DAC[MAKE_INDEX(index, 5) +
						  0] = colors[index].red;
					regp->DAC[MAKE_INDEX(index, 5) +
						  2] = colors[index].blue;
				}
			}
			break;
		default:
			for (i = 0; i < numColors; i++) {
				index = indices[i];
				regp->DAC[index * 3] = colors[index].red;
				regp->DAC[(index * 3) + 1] =
				    colors[index].green;
				regp->DAC[(index * 3) + 2] =
				    colors[index].blue;
			}
			break;
		}

		NVCrtcLoadPalette(crtc);
	}
}

//#define DEPTH_SHIFT(val, w) ((val << (8 - w)) | (val >> ((w << 1) - 8)))
#define COLOR(c) (unsigned int)(0x3fff * ((c)/255.0))
static void
NV50LoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices,
		LOCO * colors, VisualPtr pVisual)
{
	NVPtr pNv = NVPTR(pScrn);
	int i, index;
	volatile struct {
		unsigned short red, green, blue, unused;
	} *lut = (void *) pNv->CLUT->map;

	switch (pScrn->depth) {
	case 15:
		for (i = 0; i < numColors; i++) {
			index = indices[i];
			lut[DEPTH_SHIFT(index, 5)].red =
			    COLOR(colors[index].red);
			lut[DEPTH_SHIFT(index, 5)].green =
			    COLOR(colors[index].green);
			lut[DEPTH_SHIFT(index, 5)].blue =
			    COLOR(colors[index].blue);
		}
		break;
	case 16:
		for (i = 0; i < numColors; i++) {
			index = indices[i];
			lut[DEPTH_SHIFT(index, 6)].green =
			    COLOR(colors[index].green);
			if (index < 32) {
				lut[DEPTH_SHIFT(index, 5)].red =
				    COLOR(colors[index].red);
				lut[DEPTH_SHIFT(index, 5)].blue =
				    COLOR(colors[index].blue);
			}
		}
		break;
	default:
		for (i = 0; i < numColors; i++) {
			index = indices[i];
			lut[index].red = COLOR(colors[index].red);
			lut[index].green = COLOR(colors[index].green);
			lut[index].blue = COLOR(colors[index].blue);
		}
		break;
	}
}


static void NVBacklightEnable(NVPtr pNv,  Bool on)
{
    /* This is done differently on each laptop.  Here we
       define the ones we know for sure. */

#if defined(__powerpc__)
    if((pNv->Chipset == 0x10DE0179) || 
       (pNv->Chipset == 0x10DE0189) || 
       (pNv->Chipset == 0x10DE0329))
    {
       /* NV17,18,34 Apple iMac, iBook, PowerBook */
      CARD32 tmp_pmc, tmp_pcrt;
      tmp_pmc = nvReadMC(pNv, 0x10F0) & 0x7FFFFFFF;
      tmp_pcrt = nvReadCRTC0(pNv, NV_CRTC_081C) & 0xFFFFFFFC;
      if(on) {
          tmp_pmc |= (1 << 31);
          tmp_pcrt |= 0x1;
      }
      nvWriteMC(pNv, 0x10F0, tmp_pmc);
      nvWriteCRTC0(pNv, NV_CRTC_081C, tmp_pcrt);
    }
#endif
    
    if(pNv->LVDS) {
       if(pNv->twoHeads && ((pNv->Chipset & 0x0ff0) != CHIPSET_NV11)) {
           nvWriteMC(pNv, 0x130C, on ? 3 : 7);
       }
    } else {
       CARD32 fpcontrol;

       fpcontrol = nvReadCurRAMDAC(pNv, 0x848) & 0xCfffffCC;

       /* cut the TMDS output */
       if(on) fpcontrol |= pNv->fpSyncs;
       else fpcontrol |= 0x20000022;

       nvWriteCurRAMDAC(pNv, 0x0848, fpcontrol);
    }
}

static void
NVDPMSSetLCD(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
  NVPtr pNv = NVPTR(pScrn);

  if (!pScrn->vtSema) return;

  vgaHWDPMSSet(pScrn, PowerManagementMode, flags);

  switch (PowerManagementMode) {
  case DPMSModeStandby:  /* HSync: Off, VSync: On */
  case DPMSModeSuspend:  /* HSync: On, VSync: Off */
  case DPMSModeOff:      /* HSync: Off, VSync: Off */
    NVBacklightEnable(pNv, 0);
    break;
  case DPMSModeOn:       /* HSync: On, VSync: On */
    NVBacklightEnable(pNv, 1);
  default:
    break;
  }
}


static void
NVDPMSSet(ScrnInfoPtr pScrn, int PowerManagementMode, int flags)
{
  unsigned char crtc1A;
  vgaHWPtr hwp = VGAHWPTR(pScrn);

  if (!pScrn->vtSema) return;

  crtc1A = hwp->readCrtc(hwp, 0x1A) & ~0xC0;

  switch (PowerManagementMode) {
  case DPMSModeStandby:  /* HSync: Off, VSync: On */
    crtc1A |= 0x80;
    break;
  case DPMSModeSuspend:  /* HSync: On, VSync: Off */
    crtc1A |= 0x40;
    break;
  case DPMSModeOff:      /* HSync: Off, VSync: Off */
    crtc1A |= 0xC0;
    break;
  case DPMSModeOn:       /* HSync: On, VSync: On */
  default:
    break;
  }

  /* vgaHWDPMSSet will merely cut the dac output */
  vgaHWDPMSSet(pScrn, PowerManagementMode, flags);

  hwp->writeCrtc(hwp, 0x1A, crtc1A);
}


/* Mandatory */

/* This gets called at the start of each server generation */

static Bool
NVScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
    ScrnInfoPtr pScrn;
    vgaHWPtr hwp;
    NVPtr pNv;
    int ret;
    VisualPtr visual;
    unsigned char *FBStart;
    int width, height, displayWidth, shadowHeight;

    /* 
     * First get the ScrnInfoRec
     */
    pScrn = xf86Screens[pScreen->myNum];

    hwp = VGAHWPTR(pScrn);
    pNv = NVPTR(pScrn);

    /* Map the VGA memory when the primary video */
    if (pNv->Primary) {
	hwp->MapSize = 0x10000;
	if (!vgaHWMapMem(pScrn))
	    return FALSE;
    }
    
    /* First init DRI/DRM */
    if (!NVDRIScreenInit(pScrn))
	return FALSE;
    
    ret = drmCommandNone(pNv->drm_fd, DRM_NOUVEAU_CARD_INIT);
    if (ret) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Error initialising the nouveau kernel module: %d\n",
		   ret);
	return FALSE;
    }
    
    /* Allocate and map memory areas we need */
    if (!NVMapMem(pScrn))
	return FALSE;
    
    if (!pNv->NoAccel) {
	/* Init DRM - Alloc FIFO */
	if (!NVInitDma(pScrn))
	    return FALSE;
	
	/* setup graphics objects */
	if (!NVAccelCommonInit(pScrn))
	    return FALSE;
    }
    
    if (!pNv->randr12_enable) {
	/* Save the current state */
	NVSave(pScrn);
	/* Initialise the first mode */
	if (!NVModeInit(pScrn, pScrn->currentMode))
	    return FALSE;

	/* Darken the screen for aesthetic reasons and set the viewport */
	
	NVSaveScreen(pScreen, SCREEN_SAVER_ON);
	pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);
	
    } else {
	pScrn->memPhysBase = pNv->VRAMPhysical;
	pScrn->fbOffset = 0;
	
	if (!NVEnterVT(scrnIndex, 0))
	    return FALSE;
    }


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
                          miGetDefaultVisualMask(pScrn->depth), 8,
                          pScrn->defaultVisual))
	  return FALSE;
    if (!miSetPixmapDepths ()) return FALSE;

    /*
     * Call the framebuffer layer's ScreenInit function, and fill in other
     * pScreen fields.
     */

    width = pScrn->virtualX;
    height = pScrn->virtualY;
    displayWidth = pScrn->displayWidth;


    if(pNv->Rotate) {
	height = pScrn->virtualX;
	width = pScrn->virtualY;
    }

    /* If RandR rotation is enabled, leave enough space in the
     * framebuffer for us to rotate the screen dimensions without
     * changing the pitch.
     */
    if(pNv->RandRRotation)
        shadowHeight = max(width, height);
    else
        shadowHeight = height;

    if(pNv->ShadowFB) {
 	pNv->ShadowPitch = BitmapBytePad(pScrn->bitsPerPixel * width);
        pNv->ShadowPtr = xalloc(pNv->ShadowPitch * shadowHeight);
	displayWidth = pNv->ShadowPitch / (pScrn->bitsPerPixel >> 3);
        FBStart = pNv->ShadowPtr;
    } else {
	pNv->ShadowPtr = NULL;
	FBStart = pNv->FB->map;
    }

    switch (pScrn->bitsPerPixel) {
        case 8:
        case 16:
        case 32:
            ret = fbScreenInit(pScreen, FBStart, width, height,
                               pScrn->xDpi, pScrn->yDpi,
                               displayWidth, pScrn->bitsPerPixel);
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

    fbPictureInit (pScreen, 0, 0);
    
    xf86SetBlackWhitePixels(pScreen);

    if (!pNv->NoAccel) {
	    NVExaInit(pScreen);
	    NVResetGraphics(pScrn);
    }
    
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);
    xf86SetSilkenMouse(pScreen);

    /* Finish DRI init */
    NVDRIFinishScreenInit(pScrn);

    /* Initialize software cursor.  
	Must precede creation of the default colormap */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    /* Initialize HW cursor layer. 
	Must follow software cursor initialization*/
    if (pNv->HWCursor) { 
	if (pNv->Architecture < NV_ARCH_50)
	    ret = NVCursorInit(pScreen);
	else
	    ret = NV50CursorInit(pScreen);

	if (ret != TRUE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, 
		"Hardware cursor initialization failed\n");
	    pNv->HWCursor = FALSE;
	}
    }

    /* Initialise default colourmap */
    if (!miCreateDefColormap(pScreen))
	return FALSE;

    /* Initialize colormap layer.  
       Must follow initialization of the default colormap */
    if (!pNv->randr12_enable) {
	if(!xf86HandleColormaps(pScreen, 256, 8, NVDACLoadPalette,
				NULL, CMAP_RELOAD_ON_MODE_SWITCH | CMAP_PALETTED_TRUECOLOR))
	    return FALSE;
    } else {
	if (pNv->Architecture < NV_ARCH_50) {
	    if (!xf86HandleColormaps(pScreen, 256, 8, NVLoadPalette,
				     NULL,
				     CMAP_RELOAD_ON_MODE_SWITCH |
				     CMAP_PALETTED_TRUECOLOR))
		return FALSE;
	} else {
	    if (!xf86HandleColormaps(pScreen, 256, 8, NV50LoadPalette,
				     NULL, CMAP_PALETTED_TRUECOLOR))
		return FALSE;
	}
    }

    if (pNv->randr12_enable) {
	xf86DPMSInit(pScreen, xf86DPMSSet, 0);
	
	if (!xf86CrtcScreenInit(pScreen))
	    return FALSE;

	pNv->PointerMoved = pScrn->PointerMoved;
	pScrn->PointerMoved = NVPointerMoved;
    }

    if(pNv->ShadowFB) {
	RefreshAreaFuncPtr refreshArea = NVRefreshArea;

	if(pNv->Rotate || pNv->RandRRotation) {
	   pNv->PointerMoved = pScrn->PointerMoved;
	   if(pNv->Rotate)
	       pScrn->PointerMoved = NVPointerMoved;

	   switch(pScrn->bitsPerPixel) {
               case 8:	refreshArea = NVRefreshArea8;	break;
               case 16:	refreshArea = NVRefreshArea16;	break;
               case 32:	refreshArea = NVRefreshArea32;	break;
	   }
           if(!pNv->RandRRotation) {
               xf86DisableRandR();
               xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                          "Driver rotation enabled, RandR disabled\n");
           }
	}

	ShadowFBInit(pScreen, refreshArea);
    }

    if (!pNv->randr12_enable) {
	if(pNv->FlatPanel)
	    xf86DPMSInit(pScreen, NVDPMSSetLCD, 0);
	else
	    xf86DPMSInit(pScreen, NVDPMSSet, 0);
    }

    pScrn->memPhysBase = pNv->VRAMPhysical;
    pScrn->fbOffset = 0;

    if(pNv->Rotate == 0 && !pNv->RandRRotation)
       NVInitVideo(pScreen);

    pScreen->SaveScreen = NVSaveScreen;

    /* Wrap the current CloseScreen function */
    pNv->CloseScreen = pScreen->CloseScreen;
    pScreen->CloseScreen = NVCloseScreen;

    pNv->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = NVBlockHandler;

#ifdef RANDR
    /* Install our DriverFunc.  We have to do it this way instead of using the
     * HaveDriverFuncs argument to xf86AddDriver, because InitOutput clobbers
     * pScrn->DriverFunc */
    if (!pNv->randr12_enable)
	pScrn->DriverFunc = NVDriverFunc;
#endif

    /* Report any unused options (only for the first generation) */
    if (serverGeneration == 1) {
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);
    }
    return TRUE;
}

static Bool
NVSaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    NVPtr pNv = NVPTR(pScrn);
    int i;
    Bool on = xf86IsUnblank(mode);
    
    if (pNv->randr12_enable) {
	if (pScrn->vtSema) {
	    for (i = 0; i < xf86_config->num_crtc; i++) {
		
		if (xf86_config->crtc[i]->enabled) {
		    NVCrtcBlankScreen(xf86_config->crtc[i],
				      on);
		}
	    }
	    
	}
	return TRUE;
    } else
	return vgaHWSaveScreen(pScreen, mode);
}

static void
NVSave(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    NVRegPtr nvReg = &pNv->SavedReg;
    vgaHWPtr pVga = VGAHWPTR(pScrn);
    vgaRegPtr vgaReg = &pVga->SavedReg;
    int i;

    if (pNv->randr12_enable) {
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int vgaflags = VGA_SR_CMAP | VGA_SR_MODE;

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86_config->crtc[i]->funcs->save(xf86_config->crtc[i]);
	}

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86_config->output[i]->funcs->save(xf86_config->
						    output[i]);
	}

	vgaHWUnlock(pVga);
#ifndef __powerpc__
	vgaflags |= VGA_SR_FONTS;
#endif
	vgaHWSave(pScrn, vgaReg, vgaflags);
    } else {
	NVLockUnlock(pNv, 0);
	if(pNv->twoHeads) {
	    nvWriteVGA(pNv, NV_VGA_CRTCX_OWNER, pNv->crtc_active[1] * 0x3);
	    NVLockUnlock(pNv, 0);
	}

	NVDACSave(pScrn, vgaReg, nvReg, pNv->Primary);
    }
}

#ifdef RANDR
static Bool
NVRandRGetInfo(ScrnInfoPtr pScrn, Rotation *rotations)
{
    NVPtr pNv = NVPTR(pScrn);

    if(pNv->RandRRotation)
       *rotations = RR_Rotate_0 | RR_Rotate_90 | RR_Rotate_270;
    else
       *rotations = RR_Rotate_0;

    return TRUE;
}

static Bool
NVRandRSetConfig(ScrnInfoPtr pScrn, xorgRRConfig *config)
{
    NVPtr pNv = NVPTR(pScrn);

    switch(config->rotation) {
        case RR_Rotate_0:
            pNv->Rotate = 0;
            pScrn->PointerMoved = pNv->PointerMoved;
            break;

        case RR_Rotate_90:
            pNv->Rotate = -1;
            pScrn->PointerMoved = NVPointerMoved;
            break;

        case RR_Rotate_270:
            pNv->Rotate = 1;
            pScrn->PointerMoved = NVPointerMoved;
            break;

        default:
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "Unexpected rotation in NVRandRSetConfig!\n");
            pNv->Rotate = 0;
            pScrn->PointerMoved = pNv->PointerMoved;
            return FALSE;
    }

    return TRUE;
}

static Bool
NVDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer data)
{
    switch(op) {
       case RR_GET_INFO:
          return NVRandRGetInfo(pScrn, (Rotation*)data);
       case RR_SET_CONFIG:
          return NVRandRSetConfig(pScrn, (xorgRRConfig*)data);
       default:
          return FALSE;
    }

    return FALSE;
}
#endif
