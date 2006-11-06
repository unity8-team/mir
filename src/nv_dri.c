#include "nv_include.h"
#ifdef XF86DRI
#define _XF86DRI_SERVER_
#include "GL/glxint.h"
#include "GL/glxtokens.h"
#include "sarea.h"
#include "xf86drm.h"
#include "dri.h"
#include "nv_dripriv.h"

Bool NVDRMSetParam(NVPtr pNv, unsigned int param, unsigned int value)
{
	drm_nouveau_setparam_t setparam;
	int ret;

	setparam.param = param;
	setparam.value = value;
	ret = drmCommandWriteRead(pNv->drm_fd, DRM_NOUVEAU_SETPARAM, &setparam,
			sizeof(setparam));
	if (ret)
		return FALSE;
	return TRUE;
}

unsigned int NVDRMGetParam(NVPtr pNv, unsigned int param)
{
	drm_nouveau_getparam_t getparam;

	getparam.param = param;
	drmCommandWriteRead(pNv->drm_fd, DRM_NOUVEAU_GETPARAM, &getparam, sizeof(getparam));

	return getparam.value;
}

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

static Bool NVDRIInitVisualConfigs(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn=xf86Screens[pScreen->myNum];
	__GLXvisualConfig* pConfigs = NULL;
	NVConfigPrivPtr pNVConfigs = NULL;
	NVConfigPrivPtr* pNVConfigPtrs = NULL;
	int db,depth,alpha;
	int depths[]={24,16,0};
	int num_configs,i;

	switch(pScrn->depth)
	{
		case 8:
		case 15:
			xf86DrvMsg(pScreen->myNum, X_ERROR, "[dri] no DRI at %d bpp ",pScrn->depth);
			break;
		case 16:
		case 24:
			num_configs=2*3*((pScrn->depth==24)?2:1); /* db*depth*alpha */
			if (!(pConfigs=(__GLXvisualConfig*)xcalloc(sizeof(__GLXvisualConfig),num_configs)))
				return FALSE;
			if (!(pNVConfigs=(NVConfigPrivPtr)xcalloc(sizeof(NVConfigPrivRec), num_configs))) {
				xfree(pConfigs);
				return FALSE;
			}
			if (!(pNVConfigPtrs=(NVConfigPrivPtr *)xcalloc(sizeof(NVConfigPrivPtr),num_configs))) {
				xfree(pConfigs);
				xfree(pNVConfigs);
				return FALSE;
			}

			i = 0;
			for(db=1;db>=0;db--)
			for(depth=0;depth<3;depth++)
			for(alpha=0;alpha<((pScrn->depth==24)?1:0);alpha++)
			{
				pConfigs[i].vid                = (VisualID)(-1);
				pConfigs[i].class              = -1;
				pConfigs[i].rgba               = TRUE;
				if (pScrn->depth==16)
				{					
					pConfigs[i].redSize            = 5;
					pConfigs[i].greenSize          = 6;
					pConfigs[i].blueSize           = 5;
					pConfigs[i].alphaSize          = 0;
					pConfigs[i].redMask            = 0x0000F800;
					pConfigs[i].greenMask          = 0x000007E0;
					pConfigs[i].blueMask           = 0x0000001F;
					pConfigs[i].alphaMask          = 0x00000000;
				} else {
					pConfigs[i].redSize            = 8;
					pConfigs[i].greenSize          = 8;
					pConfigs[i].blueSize           = 8;
					pConfigs[i].redMask            = 0x00FF0000;
					pConfigs[i].greenMask          = 0x0000FF00;
					pConfigs[i].blueMask           = 0x000000FF;
					if (alpha) {
						pConfigs[i].alphaSize          = 8;
						pConfigs[i].alphaMask          = 0xFF000000;
					} else {
						pConfigs[i].alphaSize          = 0;
						pConfigs[i].alphaMask          = 0x00000000;
					}
				}

				pConfigs[i].accumRedSize   = 0;
				pConfigs[i].accumGreenSize = 0;
				pConfigs[i].accumBlueSize  = 0;
				pConfigs[i].accumAlphaSize = 0;
				if (db)
					pConfigs[i].doubleBuffer   = TRUE;
				else
					pConfigs[i].doubleBuffer   = FALSE;
				pConfigs[i].stereo             = FALSE;
				pConfigs[i].bufferSize         = pScrn->depth;
				pConfigs[i].depthSize          = depths[depth];
				pConfigs[i].stencilSize        = 0;
				pConfigs[i].auxBuffers         = 0;
				pConfigs[i].level              = 0;
				pConfigs[i].visualRating       = GLX_NONE;
				pConfigs[i].transparentPixel   = GLX_NONE;
				pConfigs[i].transparentRed     = 0;
				pConfigs[i].transparentGreen   = 0;
				pConfigs[i].transparentBlue    = 0;
				pConfigs[i].transparentAlpha   = 0;
				pConfigs[i].transparentIndex   = 0;
				i++;
			}
			break;
	}
	GlxSetVisualConfigs(num_configs, pConfigs, (void**)pNVConfigPtrs);
	return TRUE;
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

#if 0
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

    agp_size = drmAgpSize(pNv->drm_fd);
	if (agp_size==0)
		return FALSE;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"AGP: aperture is %dMB\n", (unsigned int)(agp_size>>20));

	if (agp_size > 16*1024*1024)
		agp_size = 16*1024*1024;

	pNv->AGPScratch = NVAllocateMemory(pNv, NOUVEAU_MEM_AGP, agp_size);
	if (!pNv->AGPScratch) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Unable to alloc AGP memory - DMA transfers disabled\n");
		return FALSE;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"AGP: mapped %dMB at %p\n",
			(unsigned int)(pNv->AGPScratch->size>>20),
			pNv->AGPScratch->map);

    return TRUE;
}
#endif




