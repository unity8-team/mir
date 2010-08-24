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
ATI, VA LINUX SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

/*
 * Authors: Jeff Hartmann <jhartmann@valinux.com>
 *          David Dawes <dawes@xfree86.org>
 *          Keith Whitwell <keith@tungstengraphics.com>
 */

/*
 * This driver does AGP memory allocation a little differently from most
 * others.  The 2D and 3D allocations have been unified (see ums_memory.c).
 * The driver does the AGP allocations and binding directly, then passes
 * on the mappings to the DRM module.  The DRM's AGP interfaces are not used.
 * The main difference with this is that the offsets need to include
 * the AGP aperture base address because that won't be known or added on
 * by the DRM module.
 *
 * DHD 07/2002
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>

#include "GL/glxtokens.h"

#include "ums.h"

#if (DRIINFO_MAJOR_VERSION > 5 || \
     (DRIINFO_MAJOR_VERSION == 5 && DRIINFO_MINOR_VERSION >= 4))
#define DRI_DRIVER_FRAMEBUFFER_MAP 1
#else
#define DRI_DRIVER_FRAMEBUFFER_MAP 0
#endif

extern void GlxSetVisualConfigs(int nconfigs,
				__GLXvisualConfig * configs,
				void **configprivs);

static Bool
ums_I830CheckDRIAvailable(ScrnInfoPtr pScrn)
{
   /* Hardware 3D rendering only implemented for 16bpp and 32 bpp */
   if (((pScrn->bitsPerPixel / 8) != 2 && pScrn->depth != 16) &&
       (pScrn->bitsPerPixel / 8) != 4) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "[drm] Direct rendering only supported in 16 and 32 bpp modes\n");
      return FALSE;
   }

   /* Check that the GLX, DRI, and DRM modules have been loaded by testing
    * for known symbols in each module. */
   if (!xf86LoaderCheckSymbol("GlxSetVisualConfigs")) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "[dri] %s failed: glx not loaded\n", __FUNCTION__);
      return FALSE;
   }
   if (!xf86LoaderCheckSymbol("DRIScreenInit")) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "[dri] %s failed: dri not loaded\n", __FUNCTION__);
      return FALSE;
   }
   if (!xf86LoaderCheckSymbol("drmAvailable")) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "[dri] %s failed: libdrm not loaded\n", __FUNCTION__);
      return FALSE;
   }
   if (!xf86LoaderCheckSymbol("DRIQueryVersion")) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "[dri] %s failed (libdri.a too old)\n", __FUNCTION__);
      return FALSE;
   }

   /* Check the DRI version */
   {
      int major, minor, patch;

      DRIQueryVersion(&major, &minor, &patch);
      if (major != DRIINFO_MAJOR_VERSION || minor < DRIINFO_MINOR_VERSION ||
	  major < 5) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "[dri] %s failed because of a version mismatch.\n"
		    "[dri] libDRI version is %d.%d.%d but version %d.%d.x is needed.\n"
		    "[dri] Disabling DRI.\n",
		    "I830CheckDRIAvailable", major, minor, patch,
		     DRIINFO_MAJOR_VERSION, DRIINFO_MINOR_VERSION);
	 return FALSE;
      }
   }

   return TRUE;
}

static Bool
ums_I830CreateContext(ScreenPtr pScreen, VisualPtr visual,
		  drm_context_t hwContext, void *pVisualConfigPriv,
		  DRIContextType contextStore)
{
   return TRUE;
}

static void
ums_I830DestroyContext(ScreenPtr pScreen, drm_context_t hwContext,
		   DRIContextType contextStore)
{
}

static void
ums_I830DRISwapContext(ScreenPtr pScreen, DRISyncType syncType,
		   DRIContextType oldContextType, void *oldContext,
		   DRIContextType newContextType, void *newContext)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    if (syncType == DRI_3D_SYNC &&
	oldContextType == DRI_2D_CONTEXT && newContextType == DRI_2D_CONTEXT) {

	if (UMS_DEBUG & DEBUG_VERBOSE_DRI)
	    ErrorF("i830DRISwapContext (in)\n");

	pI830->last_3d = LAST_3D_OTHER;

	if (!pScrn->vtSema)
	    return;
	pI830->LockHeld = 1;
	ums_refresh_ring(pScrn);

    } else if (syncType == DRI_2D_SYNC &&
	       oldContextType == DRI_NO_CONTEXT &&
	       newContextType == DRI_2D_CONTEXT) {
	if (UMS_DEBUG & DEBUG_VERBOSE_DRI)
	    ErrorF("i830DRISwapContext (out)\n");

	if (!pScrn->vtSema)
	    return;
	pI830->LockHeld = 0;
    } else if (UMS_DEBUG & DEBUG_VERBOSE_DRI)
	ErrorF("i830DRISwapContext (other)\n");
}

static void
ums_I830DRIInitBuffers(WindowPtr pWin, RegionPtr prgn, CARD32 index)
{
   ScreenPtr pScreen = pWin->drawable.pScreen;
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   BoxPtr pbox;
   int nbox;
   int buffer, first_buffer, last_buffer;

   if (UMS_DEBUG & DEBUG_VERBOSE_DRI)
      ErrorF("I830DRIInitBuffers\n");

   first_buffer = I830_SELECT_BACK;
   last_buffer = I830_SELECT_DEPTH;

   for (buffer = first_buffer; buffer <= last_buffer; buffer++) {
      pbox = REGION_RECTS(prgn);
      nbox = REGION_NUM_RECTS(prgn);

      if (!ums_I830SelectBuffer(pScrn, buffer))
	 continue;

      if (buffer == I830_SELECT_DEPTH) {
	 switch (pScrn->bitsPerPixel) {
	 case 16:
	    ums_I830SetupForSolidFill(pScrn, 0xffff, GXcopy, -1);
	    break;
	 case 32:
	    ums_I830SetupForSolidFill(pScrn, 0xffffff, GXcopy, -1);
	    break;
	 }
      } else
	  ums_I830SetupForSolidFill(pScrn, 0, GXcopy, -1);
      while (nbox--) {
	  ums_I830SubsequentSolidFillRect(pScrn, pbox->x1, pbox->y1,
				     pbox->x2 - pbox->x1, pbox->y2 - pbox->y1);
	 pbox++;
      }
   }

   ums_I830SelectBuffer(pScrn, I830_SELECT_FRONT);
   ums_i830MarkSync(pScrn);
}

/* This routine is a modified form of XAADoBitBlt with the calls to
 * ScreenToScreenBitBlt built in. My routine has the prgnSrc as source
 * instead of destination. My origin is upside down so the ydir cases
 * are reversed.
 */
static void
ums_I830DRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
		   RegionPtr prgnSrc, CARD32 index)
{
   ScreenPtr pScreen = pParent->drawable.pScreen;
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   BoxPtr pboxTmp, pboxNext, pboxBase;
   DDXPointPtr pptTmp, pptNew2 = NULL;
   int xdir, ydir;

#if 0
   int screenwidth = pScrn->virtualX;
   int screenheight = pScrn->virtualY;
#else
   int screenwidth = pScreen->width;
   int screenheight = pScreen->height;
#endif

   BoxPtr pbox = REGION_RECTS(prgnSrc);
   int nbox = REGION_NUM_RECTS(prgnSrc);

   BoxPtr pboxNew1 = NULL;
   BoxPtr pboxNew2 = NULL;
   DDXPointPtr pptNew1 = NULL;
   DDXPointPtr pptSrc = &ptOldOrg;

   int dx = pParent->drawable.x - ptOldOrg.x;
   int dy = pParent->drawable.y - ptOldOrg.y;
   int buffer, first_buffer, last_buffer;

   /* If the copy will overlap in Y, reverse the order */
   if (dy > 0) {
      ydir = -1;

      if (nbox > 1) {
	 /* Keep ordering in each band, reverse order of bands */
	 pboxNew1 = (BoxPtr) malloc(sizeof(BoxRec) * nbox);
	 if (!pboxNew1)
	    return;
	 pptNew1 = (DDXPointPtr) malloc(sizeof(DDXPointRec) * nbox);
	 if (!pptNew1) {
	    free(pboxNew1);
	    return;
	 }
	 pboxBase = pboxNext = pbox + nbox - 1;
	 while (pboxBase >= pbox) {
	    while ((pboxNext >= pbox) && (pboxBase->y1 == pboxNext->y1))
	       pboxNext--;
	    pboxTmp = pboxNext + 1;
	    pptTmp = pptSrc + (pboxTmp - pbox);
	    while (pboxTmp <= pboxBase) {
	       *pboxNew1++ = *pboxTmp++;
	       *pptNew1++ = *pptTmp++;
	    }
	    pboxBase = pboxNext;
	 }
	 pboxNew1 -= nbox;
	 pbox = pboxNew1;
	 pptNew1 -= nbox;
	 pptSrc = pptNew1;
      }
   } else {
      /* No changes required */
      ydir = 1;
   }

   /* If the regions will overlap in X, reverse the order */
   if (dx > 0) {
      xdir = -1;

      if (nbox > 1) {
	 /*reverse orderof rects in each band */
	 pboxNew2 = (BoxPtr) malloc(sizeof(BoxRec) * nbox);
	 pptNew2 = (DDXPointPtr) malloc(sizeof(DDXPointRec) * nbox);
	 if (!pboxNew2 || !pptNew2) {
	    if (pptNew2)
	       free(pptNew2);
	    if (pboxNew2)
	       free(pboxNew2);
	    if (pboxNew1) {
	       free(pptNew1);
	       free(pboxNew1);
	    }
	    return;
	 }
	 pboxBase = pboxNext = pbox;
	 while (pboxBase < pbox + nbox) {
	    while ((pboxNext < pbox + nbox) && (pboxNext->y1 == pboxBase->y1))
	       pboxNext++;
	    pboxTmp = pboxNext;
	    pptTmp = pptSrc + (pboxTmp - pbox);
	    while (pboxTmp != pboxBase) {
	       *pboxNew2++ = *--pboxTmp;
	       *pptNew2++ = *--pptTmp;
	    }
	    pboxBase = pboxNext;
	 }
	 pboxNew2 -= nbox;
	 pbox = pboxNew2;
	 pptNew2 -= nbox;
	 pptSrc = pptNew2;
      }
   } else {
      /* No changes are needed */
      xdir = 1;
   }

   /* SelectBuffer isn't really a good concept for the i810.
    */
   ums_I830EmitFlush(pScrn);
   first_buffer = I830_SELECT_BACK;
   last_buffer = I830_SELECT_DEPTH;

   for (buffer = first_buffer; buffer <= last_buffer; buffer++) {
      if (!ums_I830SelectBuffer(pScrn, buffer))
	 continue;
      ums_I830SetupForScreenToScreenCopy(pScrn, xdir, ydir, GXcopy, -1, -1);
      pbox = REGION_RECTS(prgnSrc);
      nbox = REGION_NUM_RECTS(prgnSrc);
      for (; nbox--; pbox++) {

	 int x1 = pbox->x1;
	 int y1 = pbox->y1;
	 int destx = x1 + dx;
	 int desty = y1 + dy;
	 int w = pbox->x2 - x1 + 1;
	 int h = pbox->y2 - y1 + 1;

	 if (destx < 0)
	    x1 -= destx, w += destx, destx = 0;
	 if (desty < 0)
	    y1 -= desty, h += desty, desty = 0;
	 if (destx + w > screenwidth)
	    w = screenwidth - destx;
	 if (desty + h > screenheight)
	    h = screenheight - desty;
	 if (w <= 0)
	    continue;
	 if (h <= 0)
	    continue;

	 if (UMS_DEBUG & DEBUG_VERBOSE_DRI)
	    ErrorF("MoveBuffers %d,%d %dx%d dx: %d dy: %d\n",
		   x1, y1, w, h, dx, dy);

	 ums_I830SubsequentScreenToScreenCopy(pScrn, x1, y1, destx, desty, w, h);
      }
   }

   ums_I830SelectBuffer(pScrn, I830_SELECT_FRONT);
   ums_I830EmitFlush(pScrn);

   if (pboxNew2) {
      free(pptNew2);
      free(pboxNew2);
   }
   if (pboxNew1) {
      free(pptNew1);
      free(pboxNew1);
   }
   ums_i830MarkSync(pScrn);
}

static void
ums_I830DRITransitionTo3d(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);

   pI830->want_vblank_interrupts = TRUE;
   ums_I830DRISetVBlankInterrupt(pScrn, TRUE);
}

static void
ums_I830DRITransitionTo2d(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);

   pI830->want_vblank_interrupts = FALSE;
   ums_I830DRISetVBlankInterrupt(pScrn, FALSE);
}

Bool
ums_I830DRIScreenInit(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   DRIInfoPtr pDRIInfo;
   I830DRIPtr pI830DRI;

   DPRINTF(PFX, "I830DRIScreenInit\n");

   if (!ums_I830CheckDRIAvailable(pScrn))
      return FALSE;

   pDRIInfo = DRICreateInfoRec();
   if (!pDRIInfo) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] DRICreateInfoRec failed. Disabling DRI.\n");
      return FALSE;
   }

   pI830->pDRIInfo = pDRIInfo;
   pI830->LockHeld = 0;

   pDRIInfo->drmDriverName = "i915";
   if (IS_I965G(pI830))
      pDRIInfo->clientDriverName = "i965";
   else
      pDRIInfo->clientDriverName = "i915";

   if (xf86LoaderCheckSymbol("DRICreatePCIBusID")) {
      pDRIInfo->busIdString = DRICreatePCIBusID(pI830->PciInfo);
   } else {
      pDRIInfo->busIdString = malloc(64);
      sprintf(pDRIInfo->busIdString, "PCI:%d:%d:%d",
#if XSERVER_LIBPCIACCESS
	      ((pI830->PciInfo->domain << 8) | pI830->PciInfo->bus),
	      pI830->PciInfo->dev, pI830->PciInfo->func
#else
	      ((pciConfigPtr) pI830->PciInfo->thisCard)->busnum,
	      ((pciConfigPtr) pI830->PciInfo->thisCard)->devnum,
	      ((pciConfigPtr) pI830->PciInfo->thisCard)->funcnum
#endif
	      );
   }
   pDRIInfo->ddxDriverMajorVersion = I830_MAJOR_VERSION;
   pDRIInfo->ddxDriverMinorVersion = I830_MINOR_VERSION;
   pDRIInfo->ddxDriverPatchVersion = I830_PATCHLEVEL;
   pDRIInfo->ddxDrawableTableEntry = I830_MAX_DRAWABLES;

   if (SAREA_MAX_DRAWABLES < I830_MAX_DRAWABLES)
      pDRIInfo->maxDrawableTableEntry = SAREA_MAX_DRAWABLES;
   else
      pDRIInfo->maxDrawableTableEntry = I830_MAX_DRAWABLES;

   if (sizeof(XF86DRISAREARec) + sizeof(drmI830Sarea) > SAREA_MAX) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] Data does not fit in SAREA\n");
      return FALSE;
   }
   /* This is a hack for now.  We have to have more than a 4k page here
    * because of the size of the state.  However, the state should be
    * in a per-context mapping.  This will be added in the Mesa 3.5 port
    * of the I830 driver.
    */
   pDRIInfo->SAREASize = SAREA_MAX;

   if (!(pI830DRI = (I830DRIPtr) calloc(sizeof(I830DRIRec), 1))) {
      DRIDestroyInfoRec(pI830->pDRIInfo);
      pI830->pDRIInfo = NULL;
      return FALSE;
   }
   pDRIInfo->devPrivate = pI830DRI;
   pDRIInfo->devPrivateSize = sizeof(I830DRIRec);
   pDRIInfo->contextSize = sizeof(I830DRIContextRec);

   pDRIInfo->CreateContext = ums_I830CreateContext;
   pDRIInfo->DestroyContext = ums_I830DestroyContext;
   pDRIInfo->SwapContext = ums_I830DRISwapContext;
   pDRIInfo->InitBuffers = ums_I830DRIInitBuffers;
   pDRIInfo->MoveBuffers = ums_I830DRIMoveBuffers;
   pDRIInfo->bufferRequests = DRI_ALL_WINDOWS;

#if DRIINFO_MAJOR_VERSION > 5 || \
    (DRIINFO_MAJOR_VERSION == 5 && DRIINFO_MINOR_VERSION >= 3)
	 pDRIInfo->texOffsetStart = ums_I830TexOffsetStart;
#endif

#if DRI_DRIVER_FRAMEBUFFER_MAP
   /* DRI version is high enough that we can get the DRI code to not
    * try to manage the framebuffer.
    */
   pDRIInfo->frameBufferPhysicalAddress = 0;
   pDRIInfo->frameBufferSize = 0;
   pDRIInfo->frameBufferStride = 0;
   pDRIInfo->dontMapFrameBuffer = TRUE;
#else
   /* Supply a dummy mapping info required by DRI setup.
    */
   pDRIInfo->frameBufferPhysicalAddress = (char *) pI830->LinearAddr;
   pDRIInfo->frameBufferSize = GTT_PAGE_SIZE;
   pDRIInfo->frameBufferStride = 1;
#endif

   pDRIInfo->TransitionTo2d = ums_I830DRITransitionTo2d;
   pDRIInfo->TransitionTo3d = ums_I830DRITransitionTo3d;

   /* do driver-independent DRI screen initialization here */
   if (!DRIScreenInit(pScreen, pDRIInfo, &pI830->drmSubFD)) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] DRIScreenInit failed. Disabling DRI.\n");
      free(pDRIInfo->devPrivate);
      pDRIInfo->devPrivate = NULL;
      DRIDestroyInfoRec(pI830->pDRIInfo);
      pI830->pDRIInfo = NULL;
      return FALSE;
   }

   /* Now, nuke dri.c's dummy frontbuffer map setup if we did that. */
   if (pDRIInfo->frameBufferSize != 0) {
       int tmp;
       drm_handle_t fb_handle;
       void *ptmp;

       /* With the compat method, it will continue to report
	* the wrong map out of GetDeviceInfo, which will break AIGLX.
	*/
       DRIGetDeviceInfo(pScreen, &fb_handle, &tmp, &tmp, &tmp, &tmp, &ptmp);
       drmRmMap(pI830->drmSubFD, fb_handle);

       xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  "Removed DRI frontbuffer mapping in compatibility mode.\n");
       xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		  "DRIGetDeviceInfo will report incorrect frontbuffer "
		  "handle.\n");
   }

   /* Check the i915 DRM versioning */
   {
      drmVersionPtr version;

      /* Check the DRM lib version.
       * drmGetLibVersion was not supported in version 1.0, so check for
       * symbol first to avoid possible crash or hang.
       */
      if (xf86LoaderCheckSymbol("drmGetLibVersion")) {
	 version = drmGetLibVersion(pI830->drmSubFD);
      } else
      {
	 /* drmlib version 1.0.0 didn't have the drmGetLibVersion
	  * entry point.  Fake it by allocating a version record
	  * via drmGetVersion and changing it to version 1.0.0
	  */
	 version = drmGetVersion(pI830->drmSubFD);
	 version->version_major = 1;
	 version->version_minor = 0;
	 version->version_patchlevel = 0;
      }

#define REQ_MAJ 1
#define REQ_MIN 1
      if (version) {
	 if (version->version_major != REQ_MAJ ||
	     version->version_minor < REQ_MIN) {
	    /* incompatible drm library version */
	    xf86DrvMsg(pScreen->myNum, X_ERROR,
		       "[dri] I830DRIScreenInit failed because of a version mismatch.\n"
		       "[dri] libdrm.a module version is %d.%d.%d but version %d.%d.x is needed.\n"
		       "[dri] Disabling DRI.\n",
		       version->version_major,
		       version->version_minor, version->version_patchlevel,
		       REQ_MAJ, REQ_MIN);
	    drmFreeVersion(version);
	    ums_I830DRICloseScreen(pScreen);
	    return FALSE;
	 }
	 drmFreeVersion(version);
      }

      /* Check the i915 DRM version */
      version = drmGetVersion(pI830->drmSubFD);
      if (version) {
	 if (version->version_major != 1 || version->version_minor < 3) {
	    /* incompatible drm version */
	    xf86DrvMsg(pScreen->myNum, X_ERROR,
		       "[dri] %s failed because of a version mismatch.\n"
		       "[dri] i915 kernel module version is %d.%d.%d but version 1.3 or greater is needed.\n"
		       "[dri] Disabling DRI.\n",
		       "I830DRIScreenInit",
		       version->version_major,
		       version->version_minor, version->version_patchlevel);
	    ums_I830DRICloseScreen(pScreen);
	    drmFreeVersion(version);
	    return FALSE;
	 }
	 /* Check whether the kernel module attached to the device isn't the
	  * one we expected (meaning it's the old i830 module).
	  */
	 if (strncmp(version->name, pDRIInfo->drmDriverName,
		     strlen(pDRIInfo->drmDriverName)))
	 {
	    xf86DrvMsg(pScreen->myNum, X_WARNING,
		       "Detected i830 kernel module.  The i915 kernel module "
		       "is required for DRI.  Aborting.\n");
	    ums_I830DRICloseScreen(pScreen);
	    drmFreeVersion(version);
	    return FALSE;
	 }
	 pI830->drmMinor = version->version_minor;
	 drmFreeVersion(version);
      }
   }

   return TRUE;
}

Bool
ums_I830DRIFinishScreenInit(ScreenPtr pScreen)
{
   DPRINTF(PFX, "I830DRIFinishScreenInit\n");

   if (!DRIFinishScreenInit(pScreen))
      return FALSE;

   /* move irq initialize later in EnterVT, as then we
    * would finish binding possible hw status page, which
    * requires irq ctrl ioctl not be called that early.
    */
   return TRUE;
}

/*
 * Map registers & ring buffer
 */
static Bool
ums_I830DRIMapHW(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   DRIInfoPtr pDRIInfo = pI830->pDRIInfo;
   I830DRIPtr pI830DRI = pDRIInfo->devPrivate;

   DPRINTF(PFX, "I830DRIMapHW\n");
   pI830DRI->regsSize = I830_REG_SIZE;
   if (drmAddMap(pI830->drmSubFD, (drm_handle_t)pI830->MMIOAddr,
		 pI830DRI->regsSize, DRM_REGISTERS, 0,
		 (drmAddress) &pI830DRI->regs) < 0) {
      xf86DrvMsg(pScreen->myNum, X_ERROR, "[drm] drmAddMap(regs) failed\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }
   xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] Registers = 0x%08x\n",
	      (int)pI830DRI->regs);

   if (drmAddMap(pI830->drmSubFD,
		 (drm_handle_t)pI830->ring.mem->offset +
		 pI830->LinearAddr,
		 pI830->ring.mem->size, DRM_AGP, 0,
		 (drmAddress) &pI830->ring_map) < 0) {
       xf86DrvMsg(pScreen->myNum, X_ERROR,
		  "[drm] drmAddMap(ring_map) failed. Disabling DRI\n");
       DRICloseScreen(pScreen);
       return FALSE;
   }
   xf86DrvMsg(pScreen->myNum, X_INFO, "[drm] ring buffer = 0x%08x\n",
	      (int)pI830->ring_map);

   return TRUE;
}

static Bool
ums_I830InitDma(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830RingBuffer *ring = &pI830->ring;
   I830DRIPtr pI830DRI = (I830DRIPtr) pI830->pDRIInfo->devPrivate;
   drmI830Init info;

   memset(&info, 0, sizeof(drmI830Init));
   info.func = I830_INIT_DMA;

   /* Initialize fields that are used in the absence of GEM */
   info.ring_start = ring->mem->offset + pI830->LinearAddr;
   info.ring_end = ring->mem->end + pI830->LinearAddr;
   info.ring_size = ring->mem->size;

   /* Not used as of the middle of GEM development. */
   info.mmio_offset = (unsigned int)pI830DRI->regs;

   /* Not used as of before GEM development */
   info.front_offset = pI830->front_buffer->offset;
   info.back_offset = pI830->back_buffer->offset;
   info.depth_offset = pI830->depth_buffer->offset;
   info.pitch = pScrn->displayWidth;
   info.back_pitch = pScrn->displayWidth;
   info.depth_pitch = pScrn->displayWidth;
   info.w = pScrn->virtualX;
   info.h = pScrn->virtualY;


   info.sarea_priv_offset = sizeof(XF86DRISAREARec);

   /* This should probably have been moved alongside offset/pitch in the sarea.
    */
   info.cpp = pI830->cpp;

   if (drmCommandWrite(pI830->drmSubFD, DRM_I830_INIT,
		       &info, sizeof(drmI830Init))) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "I830 Dma Initialization Failed\n");
      return FALSE;
   }

   return TRUE;
}

static Bool
ums_I830SetParam(ScrnInfoPtr pScrn, int param, int value)
{
   I830Ptr pI830 = I830PTR(pScrn);
   drmI830SetParam sp;

   memset(&sp, 0, sizeof(sp));
   sp.param = param;
   sp.value = value;

   if (drmCommandWrite(pI830->drmSubFD, DRM_I830_SETPARAM, &sp, sizeof(sp))) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "I830 SetParam Failed\n");
      return FALSE;
   }

   return TRUE;
}

Bool
ums_I830DRISetHWS(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   drmI830HWS hws;

   hws.addr = pI830->hw_status->offset;

   if (drmCommandWrite(pI830->drmSubFD, DRM_I830_HWS_PAGE_ADDR,
		       &hws, sizeof(drmI830HWS))) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		 "hw status page initialization Failed\n");
      return FALSE;
   }
   return TRUE;
}


static void
ums_I830InitTextureHeap(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   drmI830MemInitHeap drmHeap;

   if (pI830->textures == NULL)
       return;

   /* Start up the simple memory manager for agp space */
   drmHeap.region = I830_MEM_REGION_AGP;
   drmHeap.start  = 0;
   drmHeap.size   = pI830->textures->size;

   if (drmCommandWrite(pI830->drmSubFD, DRM_I830_INIT_HEAP,
			  &drmHeap, sizeof(drmHeap))) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "[drm] Failed to initialized agp heap manager\n");
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "[drm] Initialized kernel agp heap manager, %ld\n",
		    pI830->textures->size);

      ums_I830SetParam(pScrn, I830_SETPARAM_TEX_LRU_LOG_GRANULARITY,
		   pI830->TexGranularity);
   }
}

static Bool
ums_I830InitVisualConfigs(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   int numConfigs = 0;
   __GLXvisualConfig *pConfigs = NULL;
   I830ConfigPrivPtr pI830Configs = NULL;
   I830ConfigPrivPtr *pI830ConfigPtrs = NULL;
   int accum, stencil, db, depth;
   int i;

   switch (pScrn->bitsPerPixel) {
   case 8:
   case 24:
      break;

   case 16:
      numConfigs = 8;

      pConfigs =
	    (__GLXvisualConfig *) calloc(sizeof(__GLXvisualConfig),
					  numConfigs);
      if (!pConfigs)
	 return FALSE;

      pI830Configs =
	    (I830ConfigPrivPtr) calloc(sizeof(I830ConfigPrivRec),
					numConfigs);
      if (!pI830Configs) {
	 free(pConfigs);
	 return FALSE;
      }

      pI830ConfigPtrs =
	    (I830ConfigPrivPtr *) calloc(sizeof(I830ConfigPrivPtr),
					  numConfigs);
      if (!pI830ConfigPtrs) {
	 free(pConfigs);
	 free(pI830Configs);
	 return FALSE;
      }

      for (i = 0; i < numConfigs; i++)
	 pI830ConfigPtrs[i] = &pI830Configs[i];

      i = 0;
      depth = 1;
      for (accum = 0; accum <= 1; accum++) {
	 for (stencil = 0; stencil <= 1; stencil++) {
	    for (db = 1; db >= 0; db--) {
	       pConfigs[i].vid = -1;
	       pConfigs[i].class = -1;
	       pConfigs[i].rgba = TRUE;
	       pConfigs[i].redSize = 5;
	       pConfigs[i].greenSize = 6;
	       pConfigs[i].blueSize = 5;
	       pConfigs[i].alphaSize = 0;
	       pConfigs[i].redMask = 0x0000F800;
	       pConfigs[i].greenMask = 0x000007E0;
	       pConfigs[i].blueMask = 0x0000001F;
	       pConfigs[i].alphaMask = 0;
	       if (accum) {
		  pConfigs[i].accumRedSize = 16;
		  pConfigs[i].accumGreenSize = 16;
		  pConfigs[i].accumBlueSize = 16;
		  pConfigs[i].accumAlphaSize = 0;
	       } else {
		  pConfigs[i].accumRedSize = 0;
		  pConfigs[i].accumGreenSize = 0;
		  pConfigs[i].accumBlueSize = 0;
		  pConfigs[i].accumAlphaSize = 0;
	       }
	       pConfigs[i].doubleBuffer = db ? TRUE : FALSE;
	       pConfigs[i].stereo = FALSE;
	       pConfigs[i].bufferSize = 16;
	       if (depth)
		  pConfigs[i].depthSize = 16;
	       else
		  pConfigs[i].depthSize = 0;
	       if (stencil)
		  pConfigs[i].stencilSize = 8;
	       else
		  pConfigs[i].stencilSize = 0;
	       pConfigs[i].auxBuffers = 0;
	       pConfigs[i].level = 0;
	       if (stencil || accum)
		  pConfigs[i].visualRating = GLX_SLOW_VISUAL_EXT;
	       else
		  pConfigs[i].visualRating = GLX_NONE_EXT;
	       pConfigs[i].transparentPixel = GLX_NONE_EXT;
	       pConfigs[i].transparentRed = 0;
	       pConfigs[i].transparentGreen = 0;
	       pConfigs[i].transparentBlue = 0;
	       pConfigs[i].transparentAlpha = 0;
	       pConfigs[i].transparentIndex = 0;
	       i++;
	    }
	 }
      }
      assert(i == numConfigs);
      break;

   case 32:
      numConfigs = 8;

      pConfigs = (__GLXvisualConfig *) calloc(sizeof(__GLXvisualConfig),
					       numConfigs);
      if (!pConfigs) {
	 return FALSE;
      }

      pI830Configs = (I830ConfigPrivPtr) calloc(sizeof(I830ConfigPrivRec),
						 numConfigs);
      if (!pI830Configs) {
	 free(pConfigs);
	 return FALSE;
      }

      pI830ConfigPtrs = (I830ConfigPrivPtr *)
	    calloc(sizeof(I830ConfigPrivPtr), numConfigs);
      if (!pI830ConfigPtrs) {
	 free(pConfigs);
	 free(pI830Configs);
	 return FALSE;
      }

      for (i = 0; i < numConfigs; i++) {
	 pI830ConfigPtrs[i] = &pI830Configs[i];
      }

      i = 0;
      for (accum = 0; accum <= 1; accum++) {
	 for (depth = 1; depth >= 0; depth--) {	/* and stencil */
	    for (db = 1; db >= 0; db--) {
	       pConfigs[i].vid = -1;
	       pConfigs[i].class = -1;
	       pConfigs[i].rgba = TRUE;
	       pConfigs[i].redSize = 8;
	       pConfigs[i].greenSize = 8;
	       pConfigs[i].blueSize = 8;
	       pConfigs[i].alphaSize = 8;
	       pConfigs[i].redMask = 0x00FF0000;
	       pConfigs[i].greenMask = 0x0000FF00;
	       pConfigs[i].blueMask = 0x000000FF;
	       pConfigs[i].alphaMask = 0xFF000000;
	       if (accum) {
		  pConfigs[i].accumRedSize = 16;
		  pConfigs[i].accumGreenSize = 16;
		  pConfigs[i].accumBlueSize = 16;
		  pConfigs[i].accumAlphaSize = 16;
	       } else {
		  pConfigs[i].accumRedSize = 0;
		  pConfigs[i].accumGreenSize = 0;
		  pConfigs[i].accumBlueSize = 0;
		  pConfigs[i].accumAlphaSize = 0;
	       }
	       if (db) {
		  pConfigs[i].doubleBuffer = TRUE;
	       } else {
		  pConfigs[i].doubleBuffer = FALSE;
	       }
	       pConfigs[i].stereo = FALSE;
	       pConfigs[i].bufferSize = 32;
	       if (depth) {
		  pConfigs[i].depthSize = 24;
		  pConfigs[i].stencilSize = 8;
	       } else {
		  pConfigs[i].depthSize = 0;
		  pConfigs[i].stencilSize = 0;
	       }
	       pConfigs[i].auxBuffers = 0;
	       pConfigs[i].level = 0;
	       if (accum) {
		  pConfigs[i].visualRating = GLX_SLOW_VISUAL_EXT;
	       } else {
		  pConfigs[i].visualRating = GLX_NONE_EXT;
	       }
	       pConfigs[i].transparentPixel = GLX_NONE_EXT;
	       pConfigs[i].transparentRed = 0;
	       pConfigs[i].transparentGreen = 0;
	       pConfigs[i].transparentBlue = 0;
	       pConfigs[i].transparentAlpha = 0;
	       pConfigs[i].transparentIndex = 0;
	       i++;
	    }
	 }
      }
      if (i != numConfigs) {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "[drm] Incorrect initialization of visuals\n");
	 return FALSE;
      }
      break;

   }
   pI830->numVisualConfigs = numConfigs;
   pI830->pVisualConfigs = pConfigs;
   pI830->pVisualConfigsPriv = pI830Configs;
   GlxSetVisualConfigs(numConfigs, pConfigs, (void **)pI830ConfigPtrs);
   return TRUE;
}

/**
 * Sets up mappings for static, lifetime-fixed allocations, and inital SAREA
 * setup.
 */
Bool
ums_I830DRIDoMappings(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   DRIInfoPtr pDRIInfo = pI830->pDRIInfo;
   I830DRIPtr pI830DRI = pDRIInfo->devPrivate;
   drmI830Sarea *sarea = (drmI830Sarea *) DRIGetSAREAPrivate(pScreen);

   if (!ums_I830DRIMapHW(pScreen)) {
       DRICloseScreen(pScreen);
       return FALSE;
   }

   if (!ums_I830InitDma(pScrn)) {
      DRICloseScreen(pScreen);
      return FALSE;
   }

   intel_bufmgr_fake_set_last_dispatch(pI830->bufmgr,
				       (volatile unsigned int *)
				       &sarea->last_dispatch);

   /* init to zero to be safe */
   sarea->front_handle = 0;
   sarea->back_handle = 0;
   sarea->depth_handle = 0;
   sarea->tex_handle = 0;

   /* Assign pScreen */
   pScrn->pScreen = pScreen;

   /* Need to initialize pScreen now to let RandR know. */
   pScrn->pScreen->width = pScrn->virtualX;
   pScrn->pScreen->height = pScrn->virtualY;

  if (!ums_update_dri_buffers(pScrn)) {
     /* screen mappings probably failed */
     xf86DrvMsg(pScreen->myNum, X_ERROR,
		"[drm] drmAddMap(screen mappings) failed. "
		"Disabling DRI\n");
     DRICloseScreen(pScreen);
     return FALSE;
  }

   if (pI830->allocate_classic_textures)
      ums_I830InitTextureHeap(pScrn);

   if (DEVICE_ID(pI830->PciInfo) != PCI_CHIP_845_G &&
       DEVICE_ID(pI830->PciInfo) != PCI_CHIP_I830_M) {
      ums_I830SetParam(pScrn, I830_SETPARAM_USE_MI_BATCHBUFFER_START, 1 );
   }

   pI830DRI = (I830DRIPtr) pI830->pDRIInfo->devPrivate;
   pI830DRI->deviceID = DEVICE_ID(pI830->PciInfo);
   pI830DRI->width = pScrn->virtualX;
   pI830DRI->height = pScrn->virtualY;
   pI830DRI->mem = pScrn->videoRam * 1024;
   pI830DRI->cpp = pI830->cpp;

   pI830DRI->bitsPerPixel = pScrn->bitsPerPixel;

   pI830DRI->sarea_priv_offset = sizeof(XF86DRISAREARec);

   if (!(ums_I830InitVisualConfigs(pScreen))) {
      xf86DrvMsg(pScreen->myNum, X_ERROR,
		 "[dri] I830InitVisualConfigs failed. Disabling DRI\n");
      DRICloseScreen(pScreen);
      return FALSE;
   }

   xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[dri] visual configs initialized\n");
   pI830->pDRIInfo->driverSwapMethod = DRI_HIDE_X_CONTEXT;

   return TRUE;
}

static int
ums_name_buffer (ScrnInfoPtr pScrn, i830_memory *mem)
{
    if (mem && mem->bo)
    {
	if (!mem->gem_name)
	{
	    int ret;
	    ret = dri_bo_flink(mem->bo, &mem->gem_name);
	    if (ret != 0)
	    {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "[drm] failed to name buffer %d\n", -errno);
		return -1;
	    }
	}
	return mem->gem_name;
    }
    return -1;
}

/**
 * Update the SAREA fields with current buffer information.
 *
 * Most of the SAREA fields are already updated by ums_do_addmap().
 *
 * This does include other SAREA initialization which will actually be constant
 * over the lifetime of the server.
 */
static void
ums_update_sarea(ScrnInfoPtr pScrn, drmI830Sarea *sarea)
{
   ScreenPtr pScreen = pScrn->pScreen;
   I830Ptr pI830 = I830PTR(pScrn);

   sarea->width = pScreen->width;
   sarea->height = pScreen->height;
   sarea->pitch = pScrn->displayWidth;
   sarea->virtualX = pScrn->virtualX;
   sarea->virtualY = pScrn->virtualY;

   sarea->front_tiled = (pI830->front_buffer->tiling != TILE_NONE);
   sarea->back_tiled = (pI830->back_buffer->tiling != TILE_NONE);
   sarea->depth_tiled = (pI830->depth_buffer->tiling != TILE_NONE);
   sarea->rotated_tiled = FALSE;

   sarea->log_tex_granularity = pI830->TexGranularity;

   sarea->front_bo_handle = ums_name_buffer (pScrn, pI830->front_buffer);
   sarea->back_bo_handle = ums_name_buffer (pScrn, pI830->back_buffer);
   sarea->depth_bo_handle = ums_name_buffer (pScrn, pI830->depth_buffer);

   /* The rotation is now handled entirely by the X Server, so just leave the
    * DRI unaware.
    */
   sarea->rotation = 0;
   sarea->rotated_offset = -1;
   sarea->rotated_size = 0;
   sarea->rotated_pitch = pScrn->displayWidth;
}

/**
 * Updates the DRI mapping for the given i830_memory struct, with the given
 * flags.
 */
static int
ums_do_addmap(ScrnInfoPtr pScrn, i830_memory *mem,
	       drm_handle_t *sarea_handle, int *sarea_size, int *sarea_offset)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int size = mem->size;

    if (mem == pI830->front_buffer) {
	/* Workaround for XAA pixmap cache: Don't use front_buffer->size
	 * and instead, calculate the visible frontbuffer size and round to
	 * avoid irritating the assertions of older DRI drivers.
	 */
	size = ROUND_TO_PAGE(pScrn->displayWidth * pScrn->virtualY *
			     pI830->cpp);
    }

    if (*sarea_handle != 0 &&
	(*sarea_size != size || *sarea_offset != mem->offset))
    {
	drmRmMap(pI830->drmSubFD, *sarea_handle);
	*sarea_handle = 0;
	*sarea_size = 0;
	*sarea_offset = 0;
    }

    if (*sarea_handle == 0) {
	int ret;

	ret = drmAddMap(pI830->drmSubFD,
			(drm_handle_t)(mem->offset + pI830->LinearAddr),
			size, DRM_AGP, 0,
			(drmAddress) sarea_handle);
	if (ret == 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "[drm] mapped %s at 0x%08lx, handle = 0x%08x\n",
		       mem->name, mem->offset + pI830->LinearAddr,
		       (int)*sarea_handle);
	    *sarea_size = size;
	    *sarea_offset = mem->offset;
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "[drm] failed to map %s at 0x%08lx\n",
		       mem->name, mem->offset + pI830->LinearAddr);
	    return FALSE;
	}
    }

    return TRUE;
}

static Bool
ums_update_dri_mappings(ScrnInfoPtr pScrn, drmI830Sarea *sarea)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (!ums_do_addmap(pScrn, pI830->front_buffer, &sarea->front_handle,
		       &sarea->front_size, &sarea->front_offset)) {
       xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling DRI.\n");
       return FALSE;
   }

   if (!ums_do_addmap(pScrn, pI830->back_buffer, &sarea->back_handle,
		       &sarea->back_size, &sarea->back_offset)) {
       xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling DRI.\n");
       return FALSE;
   }

   sarea->third_handle = 0;
   sarea->third_offset = 0;
   sarea->third_size = 0;

   if (!ums_do_addmap(pScrn, pI830->depth_buffer, &sarea->depth_handle,
		       &sarea->depth_size, &sarea->depth_offset)) {
       xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling DRI.\n");
       return FALSE;
   }

   if (pI830->allocate_classic_textures) {
       if (!ums_do_addmap(pScrn, pI830->textures, &sarea->tex_handle,
			   &sarea->tex_size, &sarea->tex_offset)) {
	   xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Disabling DRI.\n");
	   return FALSE;
       }
   }

   return TRUE;
}

static void
ums_update_screen_private(ScrnInfoPtr pScrn, drmI830Sarea *sarea)
{
   I830Ptr pI830 = I830PTR(pScrn);

   pI830->pDRIInfo->frameBufferPhysicalAddress = (char *) pI830->LinearAddr;
   pI830->pDRIInfo->frameBufferStride = pScrn->displayWidth * pI830->cpp;
   pI830->pDRIInfo->frameBufferSize = sarea->front_size;
#if DRI_DRIVER_FRAMEBUFFER_MAP
   pI830->pDRIInfo->hFrameBuffer = sarea->front_handle;
#endif
}

Bool
ums_update_dri_buffers(ScrnInfoPtr pScrn)
{
   ScreenPtr pScreen = pScrn->pScreen;
   I830Ptr pI830 = I830PTR(pScrn);
   drmI830Sarea *sarea;
   Bool success;

   if (pI830->directRenderingType != DRI_XF86DRI)
       return TRUE;

   sarea = (drmI830Sarea *) DRIGetSAREAPrivate(pScreen);

   success = ums_update_dri_mappings(pScrn, sarea);
   if (!success)
       return FALSE;
   ums_update_sarea(pScrn, sarea);
   ums_update_screen_private(pScrn, sarea);

   return TRUE;
}

Bool
ums_I830DRISetVBlankInterrupt (ScrnInfoPtr pScrn, Bool on)
{
    I830Ptr pI830 = I830PTR(pScrn);
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    drmI830VBlankPipe pipe;

    /* If we have no 3d running, then don't bother enabling the vblank
     * interrupt.
     */
    if (!pI830->want_vblank_interrupts)
	on = FALSE;

    if (pI830->directRenderingType == DRI_XF86DRI && pI830->drmMinor >= 5) {
	if (on) {
	    if (xf86_config->num_crtc > 1 && xf86_config->crtc[1]->enabled)
		if (pI830->drmMinor >= 6)
		    pipe.pipe = DRM_I830_VBLANK_PIPE_A | DRM_I830_VBLANK_PIPE_B;
		else
		    pipe.pipe = DRM_I830_VBLANK_PIPE_B;
	    else
		pipe.pipe = DRM_I830_VBLANK_PIPE_A;
	} else {
	    pipe.pipe = 0;
	}
	if (drmCommandWrite(pI830->drmSubFD, DRM_I830_SET_VBLANK_PIPE,
			    &pipe, sizeof (pipe))) {
	    return FALSE;
	}
    }

    return TRUE;
}

Bool
ums_I830DRILock(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (pI830->directRenderingType == DRI_XF86DRI && !pI830->LockHeld) {
      DRILock(screenInfo.screens[pScrn->scrnIndex], 0);
      pI830->LockHeld = 1;
      ums_refresh_ring(pScrn);
      return TRUE;
   }
   else
      return FALSE;
}

void
ums_I830DRIUnlock(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);

   if (pI830->directRenderingType == DRI_XF86DRI && pI830->LockHeld) {
      DRIUnlock(screenInfo.screens[pScrn->scrnIndex]);
      pI830->LockHeld = 0;
   }
}

Bool
ums_I830DRIInstIrqHandler(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830DRIPtr pI830DRI = (I830DRIPtr) pI830->pDRIInfo->devPrivate;

   pI830DRI->irq = drmGetInterruptFromBusID(pI830->drmSubFD,
#if XSERVER_LIBPCIACCESS
					       ((pI830->PciInfo->domain << 8) |
						pI830->PciInfo->bus),
					       pI830->PciInfo->dev,
					       pI830->PciInfo->func
#else
					       ((pciConfigPtr) pI830->
						PciInfo->thisCard)->busnum,
					       ((pciConfigPtr) pI830->
						PciInfo->thisCard)->devnum,
					       ((pciConfigPtr) pI830->
						PciInfo->thisCard)->funcnum
#endif
					       );

   if (drmCtlInstHandler(pI830->drmSubFD, pI830DRI->irq)) {
       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	       "[drm] failure adding irq handler\n");
       pI830DRI->irq = 0;
       return FALSE;
   } else
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "[drm] dma control initialized, using IRQ %d\n",
	       pI830DRI->irq);

   return TRUE;
}

static Bool
ums_I830ResumeDma(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   drmI830Init info;

   memset(&info, 0, sizeof(drmI830Init));
   info.func = I830_RESUME_DMA;

   if (drmCommandWrite(pI830->drmSubFD, DRM_I830_INIT,
		       &info, sizeof(drmI830Init))) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "I830 Dma Resume Failed\n");
      return FALSE;
   }

   return TRUE;
}

Bool
ums_I830DRIResume(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

   DPRINTF(PFX, "I830DRIResume\n");

   ums_I830ResumeDma(pScrn);
   ums_I830DRIInstIrqHandler(pScrn);

   return TRUE;
}

static Bool
ums_I830CleanupDma(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   drmI830Init info;

   memset(&info, 0, sizeof(drmI830Init));
   info.func = I830_CLEANUP_DMA;

   if (drmCommandWrite(pI830->drmSubFD, DRM_I830_INIT,
		       &info, sizeof(drmI830Init))) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "I830 Dma Cleanup Failed\n");
      return FALSE;
   }

   return TRUE;
}

void
ums_I830DRICloseScreen(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   I830DRIPtr pI830DRI = (I830DRIPtr) pI830->pDRIInfo->devPrivate;

   DPRINTF(PFX, "I830DRICloseScreen\n");

   if (pI830DRI->irq) {
       drmCtlUninstHandler(pI830->drmSubFD);
       pI830DRI->irq = 0;
   }

   ums_I830CleanupDma(pScrn);

   DRICloseScreen(pScreen);

   if (pI830->pDRIInfo) {
      if (pI830->pDRIInfo->devPrivate) {
	 free(pI830->pDRIInfo->devPrivate);
	 pI830->pDRIInfo->devPrivate = NULL;
      }
      DRIDestroyInfoRec(pI830->pDRIInfo);
      pI830->pDRIInfo = NULL;
   }
   if (pI830->pVisualConfigs)
      free(pI830->pVisualConfigs);
   if (pI830->pVisualConfigsPriv)
      free(pI830->pVisualConfigsPriv);
   pI830->directRenderingType = DRI_NONE;
}
