#include "nv_include.h"
#ifdef XF86DRI
#define _XF86DRI_SERVER_
#include "GL/glxint.h"
#include "sarea.h"
#include "xf86drm.h"
#include "dri.h"


static Bool NVCreateContext(ScreenPtr pScreen, VisualPtr visual,
		drm_context_t hwContext, void *pVisualConfigPriv,
		DRIContextType contextStore)
{
	return TRUE;
}


static void NVDestroyContext(ScreenPtr pScreen, drm_context_t hwContext,
		DRIContextType contextStore)
{
	return;
}

static void NVDRISwapContext(ScreenPtr pScreen, DRISyncType syncType,
		DRIContextType oldContextType,
		void *oldContext,
		DRIContextType newContextType,
		void *newContext)
{
	/* we really should do something here */
	return;
}

static void NVDRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 indx)
{   
	return;
}

static void NVDRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
		RegionPtr prgnSrc, CARD32 indx)
{
	return;
}	

static void NVDRITransitionTo2d(ScreenPtr pScreen)
{
	return;
}

static void NVDRITransitionTo3d(ScreenPtr pScreen)
{
	return;
}

static void NVDRITransitionSingleToMulti3d(ScreenPtr pScreen)
{           
	return;
}           
        
static void NVDRITransitionMultiToSingle3d(ScreenPtr pScreen)
{       
	return;
}

Bool NVDRIScreenInit(ScrnInfoPtr pScrn)
{
    DRIInfoPtr     pDRIInfo;
    NVPtr pNv = NVPTR(pScrn);
    drmVersionPtr drm_version;
    ScreenPtr pScreen;
    pScreen = screenInfo.screens[pScrn->scrnIndex];
    int irq;

    if (!xf86LoadSubModule(pScrn, "dri")) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "Could not load DRI module\n");
        return FALSE;
    }

    xf86LoaderReqSymLists(drmSymbols, NULL);
    xf86LoaderReqSymLists(driSymbols, NULL);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Loaded DRI module\n");

    if (!(pDRIInfo = DRICreateInfoRec())) return FALSE;
    
    pNv->pDRIInfo                        = pDRIInfo;
    pDRIInfo->drmDriverName              = "nouveau";
    pDRIInfo->clientDriverName           = "nouveau";
    pDRIInfo->busIdString                = DRICreatePCIBusID(pNv->PciInfo);

    pDRIInfo->ddxDriverMajorVersion      = NV_MAJOR_VERSION;
    pDRIInfo->ddxDriverMinorVersion      = NV_MINOR_VERSION;
    pDRIInfo->ddxDriverPatchVersion      = NV_PATCHLEVEL;

    pDRIInfo->frameBufferPhysicalAddress = (void *)pNv->VRAMPhysical;
    pDRIInfo->frameBufferSize            = pNv->VRAMPhysicalSize;
    pDRIInfo->frameBufferStride          = pScrn->displayWidth * pScrn->bitsPerPixel/8;
 
    pDRIInfo->ddxDrawableTableEntry      = 1;
    pDRIInfo->maxDrawableTableEntry      = 1;

    pDRIInfo->devPrivate                 = NULL; 
    pDRIInfo->devPrivateSize             = 0;
    pDRIInfo->contextSize                = 0;
    pDRIInfo->SAREASize                  = SAREA_MAX;
    
    pDRIInfo->CreateContext              = NVCreateContext;
    pDRIInfo->DestroyContext             = NVDestroyContext;
    pDRIInfo->SwapContext                = NVDRISwapContext;
    pDRIInfo->InitBuffers                = NVDRIInitBuffers;
    pDRIInfo->MoveBuffers                = NVDRIMoveBuffers;
    pDRIInfo->bufferRequests             = DRI_ALL_WINDOWS;
    pDRIInfo->TransitionTo2d             = NVDRITransitionTo2d;
    pDRIInfo->TransitionTo3d             = NVDRITransitionTo3d;
    pDRIInfo->TransitionSingleToMulti3D  = NVDRITransitionSingleToMulti3d;
    pDRIInfo->TransitionMultiToSingle3D  = NVDRITransitionMultiToSingle3d;

    pDRIInfo->createDummyCtx     = FALSE;
    pDRIInfo->createDummyCtxPriv = FALSE;

    if (!DRIScreenInit(pScreen, pDRIInfo, &pNv->drm_fd)) {
	xf86DrvMsg(pScreen->myNum, X_ERROR,
	    	    "[dri] DRIScreenInit failed.  Disabling DRI.\n");
	xfree(pDRIInfo->devPrivate);
	pDRIInfo->devPrivate = NULL;
	DRIDestroyInfoRec(pDRIInfo);
	pDRIInfo = NULL;
	return FALSE;
    }
    drm_version = drmGetVersion(pNv->drm_fd);

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "DRM version: %d.%d.%d (name: %s)\n",
               drm_version->version_major, 
               drm_version->version_minor, 
               drm_version->version_patchlevel,
               drm_version->name);

#if 1
    pNv->IRQ = 0;
#else
    /* Ask DRM to install IRQ handler */
    irq = drmGetInterruptFromBusID(pNv->drm_fd,
            ((pciConfigPtr)pNv->PciInfo->thisCard)->busnum,
            ((pciConfigPtr)pNv->PciInfo->thisCard)->devnum,
            ((pciConfigPtr)pNv->PciInfo->thisCard)->funcnum);

    if (drmCtlInstHandler(pNv->drm_fd, irq)) {
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to install IRQ handler\n");
        pNv->IRQ = 0;
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "IRQ handler initialised.  IRQ %d\n", irq);
        pNv->IRQ = irq;
    }
#endif

    return TRUE;
}

Bool NVInitAGP(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    unsigned long agp_size;
	drm_nouveau_mem_alloc_t alloc;

    agp_size = drmAgpSize(pNv->drm_fd);
	if (agp_size==0)
		return FALSE;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"AGP: aperture is %dMB\n", agp_size>>20);

	if (agp_size > 16*1024*1024)
		agp_size = 16*1024*1024;

	pNv->AGPScratch = NVAllocateMemory(pNv, NOUVEAU_MEM_AGP, agp_size);
	if (!pNv->AGPScratch) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Unable to alloc AGP memory - DMA transfers disabled\n");
		return FALSE;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"AGP: mapped %dMB at %p\n", pNv->AGPScratch->size>>20,
			pNv->AGPScratch->map);

    return TRUE;
}
#endif




