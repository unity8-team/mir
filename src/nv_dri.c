/*
 * Copyright 2006 Stephane Marchesin
 * Copyright 2006 Ben Skeggs
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

#include "nv_include.h"

#define _XF86DRI_SERVER_
#include "GL/glxint.h"
#include "GL/glxtokens.h"
#include "sarea.h"
#include "xf86drm.h"
#include "dri.h"
#include "nv_dripriv.h"
#include "nv_dri.h"

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
	int db,depth,alpha,stencil;
	int depths[]={24,16,0};
	int num_configs,i;

	switch(pScrn->depth)
	{
		case 16:
		case 24:
			num_configs=2*3*((pScrn->depth==24)?2:1)*2; /* db*depth*alpha*stencil */
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
			for(alpha=0;alpha<((pScrn->depth==24)?2:1);alpha++)
			for(stencil=0;stencil<2;stencil++)
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
				if (depths[depth] == 24 && stencil) {
					pConfigs[i].depthSize          = depths[depth];
					pConfigs[i].stencilSize        = 8;
				} else {
					pConfigs[i].depthSize          = depths[depth];
					pConfigs[i].stencilSize        = 0;
				}
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
		default:
			xf86DrvMsg(pScreen->myNum, X_ERROR, "[dri] no DRI at %d bpp ",pScrn->depth);
			return FALSE;
	}
	GlxSetVisualConfigs(num_configs, pConfigs, (void**)pNVConfigPtrs);
	return TRUE;
}

Bool NVDRIGetVersion(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int errmaj, errmin;
	pointer ret;

	ret = LoadSubModule(pScrn->module, "dri", NULL, NULL, NULL,
			    NULL, &errmaj, &errmin);
	if (!ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"error %d\n", errmaj);
		LoaderErrorMsg(pScrn->name, "dri", errmaj, errmin);
	}

	if (!ret && errmaj != LDR_ONCEONLY)
		return FALSE;

	xf86LoaderReqSymLists(drmSymbols, NULL);
	xf86LoaderReqSymLists(driSymbols, NULL);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Loaded DRI module\n");

	/* Check the lib version */
	if (xf86LoaderCheckSymbol("drmGetLibVersion"))
		pNv->pLibDRMVersion = drmGetLibVersion(0);
	if (pNv->pLibDRMVersion == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		"NVDRIGetVersion failed because libDRM is really "
		"way to old to even get a version number out of it.\n"
		"[dri] Disabling DRI.\n");
		return FALSE;
	}

	return TRUE;
}

Bool NVDRICheckModules(ScrnInfoPtr pScrn)
{
	if (!xf86LoaderCheckSymbol("GlxSetVisualConfigs")) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "[dri] GlxSetVisualConfigs not found.\n");
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "      NVIDIA's glx present, or glx not loaded.\n");
		return FALSE;
	}

	return TRUE;
}

Bool NVDRIScreenInit(ScrnInfoPtr pScrn)
{
	DRIInfoPtr     pDRIInfo;
	NOUVEAUDRIPtr  pNOUVEAUDRI;
	NVPtr pNv = NVPTR(pScrn);
	ScreenPtr pScreen;
	pScreen = screenInfo.screens[pScrn->scrnIndex];
	int drm_page_size;

	if (!NVDRICheckModules(pScrn))
		return FALSE;

	drm_page_size = getpagesize();
	if (!(pDRIInfo = DRICreateInfoRec())) return FALSE;

	pDRIInfo->drmDriverName              = "nouveau";
	pDRIInfo->clientDriverName           = "nouveau";
	pDRIInfo->busIdString                = DRICreatePCIBusID(pNv->PciInfo);

	pDRIInfo->ddxDriverMajorVersion      = NV_MAJOR_VERSION;
	pDRIInfo->ddxDriverMinorVersion      = NV_MINOR_VERSION;
	pDRIInfo->ddxDriverPatchVersion      = NV_PATCHLEVEL;

	pDRIInfo->frameBufferSize            = pNv->FB->size;
	pDRIInfo->frameBufferPhysicalAddress = (void *)pNv->VRAMPhysical +
					       (pNv->FB->offset -
						pNv->dev->vm_vram_base);
	pDRIInfo->frameBufferStride          = pScrn->displayWidth * pScrn->bitsPerPixel/8;

	pDRIInfo->ddxDrawableTableEntry      = 1;
	pDRIInfo->maxDrawableTableEntry      = 1;

	if (!(pNOUVEAUDRI = (NOUVEAUDRIPtr)xcalloc(sizeof(NOUVEAUDRIRec), 1))) {
		DRIDestroyInfoRec(pDRIInfo);
		return FALSE;
	}
	pDRIInfo->devPrivate                 = pNOUVEAUDRI; 
	pDRIInfo->devPrivateSize             = sizeof(NOUVEAUDRIRec);
	pDRIInfo->contextSize                = sizeof(NVDRIContextRec);
	pDRIInfo->SAREASize                  = (drm_page_size > SAREA_MAX) ? drm_page_size : SAREA_MAX;

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

	pDRIInfo->keepFDOpen = TRUE;

	if (!DRIScreenInit(pScreen, pDRIInfo, &nouveau_device(pNv->dev)->fd)) {
		xf86DrvMsg(pScreen->myNum, X_ERROR,
				"[dri] DRIScreenInit failed.  Disabling DRI.\n");
		xfree(pDRIInfo->devPrivate);
		pDRIInfo->devPrivate = NULL;
		DRIDestroyInfoRec(pDRIInfo);
		return FALSE;
	}

	if (!NVDRIInitVisualConfigs(pScreen)) {
		xf86DrvMsg(pScreen->myNum, X_ERROR,
			   "[dri] NVDRIInitVisualConfigs failed."
			   "  Disabling DRI.\n");
		DRICloseScreen(pScreen);
		xfree(pDRIInfo->devPrivate);
		pDRIInfo->devPrivate = NULL;
		DRIDestroyInfoRec(pDRIInfo);
		return FALSE;
	}

	pNv->pDRIInfo = pDRIInfo;
	return TRUE;
}

Bool NVDRIFinishScreenInit(ScrnInfoPtr pScrn)
{
	ScreenPtr      pScreen = screenInfo.screens[pScrn->scrnIndex];
	NVPtr          pNv = NVPTR(pScrn);
	NOUVEAUDRIPtr  pNOUVEAUDRI;
	int ret;

	if (!pNv->pDRIInfo)
		return TRUE;

	if (!DRIFinishScreenInit(pScreen))
		return FALSE;

	pNOUVEAUDRI 			= (NOUVEAUDRIPtr)pNv->pDRIInfo->devPrivate;

	pNOUVEAUDRI->device_id		= pNv->Chipset;

	pNOUVEAUDRI->width		= pScrn->virtualX;
	pNOUVEAUDRI->height		= pScrn->virtualY;
	pNOUVEAUDRI->depth		= pScrn->depth;
	pNOUVEAUDRI->bpp		= pScrn->bitsPerPixel;

	ret = nouveau_bo_handle_get(pNv->FB, &pNOUVEAUDRI->front_offset);
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "[dri] unable to reference front buffer: %d\n", ret);
		return FALSE;
	}
	pNOUVEAUDRI->front_pitch	= pScrn->displayWidth;
	/* back/depth buffers will likely be allocated on a per-drawable
	 * basis, but these may be useful if we want to support shared back
	 * buffers at some point.
	 */
	pNOUVEAUDRI->back_offset	= 0;
	pNOUVEAUDRI->back_pitch		= 0;
	pNOUVEAUDRI->depth_offset	= 0;
	pNOUVEAUDRI->depth_pitch	= 0;

	return TRUE;
}

void NVDRICloseScreen(ScrnInfoPtr pScrn)
{
	ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];
	NVPtr pNv = NVPTR(pScrn);

	if (pNv->NoAccel)
		return;

	DRICloseScreen(pScreen);

	if (pNv->pDRIInfo) {
		if (pNv->pDRIInfo->devPrivate) {
			xfree(pNv->pDRIInfo->devPrivate);
			pNv->pDRIInfo->devPrivate = NULL;
		}
		DRIDestroyInfoRec(pNv->pDRIInfo);
		pNv->pDRIInfo = NULL;
	}
}

