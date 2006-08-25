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


Bool NVDRIScreenInit(ScrnInfoPtr pScrn)
{
    DRIInfoPtr     pDRIInfo;
    NVPtr pNv = NVPTR(pScrn);
    drmVersionPtr drm_version;
    ScreenPtr pScreen;
    pScreen = screenInfo.screens[pScrn->scrnIndex];

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

    pDRIInfo->frameBufferPhysicalAddress = (void *)pNv->FbAddress;
    pDRIInfo->frameBufferSize            = pNv->FbUsableSize;
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

    return TRUE;
}

Bool NVInitAGP(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    int agp_mode;
    unsigned long agp_size;

    if (drmAgpAcquire(pNv->drm_fd)) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "Could not access AGP, disabling DMA transfers\n");
        return FALSE;
    }        

    agp_size = drmAgpSize(pNv->drm_fd);
    pNv->agpSize = agp_size < 16*0x100000 ? agp_size : 16*0x100000;
    pNv->agpPhysical = drmAgpBase(pNv->drm_fd);
    
    agp_mode = drmAgpGetMode(pNv->drm_fd);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "AGP: version %d.%d\n"
               "     mode %x\n"
               "     base=%08lx size=%08lx (%08lx)\n"
               , drmAgpVersionMajor(pNv->drm_fd),
               drmAgpVersionMinor(pNv->drm_fd), agp_mode,
               pNv->agpPhysical, agp_size, pNv->agpSize);

    if (drmAgpEnable(pNv->drm_fd, agp_mode) < 0) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "could not enable AGP\n");
        return FALSE;
    }

    if (drmAgpAlloc(pNv->drm_fd, pNv->agpSize, 0, 0, &pNv->drm_agp_handle)) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "could not allocate AGP memory\n");
        return FALSE;
    }

    if (drmAgpBind(pNv->drm_fd, pNv->drm_agp_handle, 0)) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "could not bind AGP memory\n");
        return FALSE;
    }

    if (drmAddMap(pNv->drm_fd, 0, pNv->agpSize, /* agp_size, */
                  DRM_AGP, 0,
                  &pNv->drm_agp_map_handle)) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "could not add AGP map, %s\n", strerror(errno));
        return FALSE;
    }        
    
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
               "agp map handle=%08lx\n", pNv->drm_agp_map_handle);
    /* no idea why the handle is set to 0 in the addMap call. */
    
    if (drmMap(pNv->drm_fd, pNv->agpPhysical, pNv->agpSize /* agp_size */, (drmAddressPtr)&pNv->agpMemory)) {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                   "could not map AGP memory: %s\n", strerror(errno));
        return FALSE;
    }

    return TRUE;
    
#if 0
    {
        char *tmp = malloc(pNv->agpSize);
        struct timeval tv, tv2;
        gettimeofday(&tv, 0);
        memcpy(tmp, pNv->agpMemory, pNv->agpSize);
        gettimeofday(&tv2, 0);
        xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                   "agp: Memory benchmark %f MB/s\n",
                   (pNv->agpSize/(1024.*1024.))*1000000
                   /((int)tv2.tv_usec- (int)tv.tv_usec + 1000000*(tv2.tv_sec-tv.tv_sec)+1));
        free(tmp);
    }
#endif
}
#endif




