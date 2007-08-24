/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *   Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
 * Credits:
 *
 *   Thanks to Ani Joshi <ajoshi@shell.unixbox.com> for providing source
 *   code to his Radeon driver.  Portions of this file are based on the
 *   initialization code for that driver.
 *
 * References:
 *
 * !!!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 * This server does not yet support these XFree86 4.0 features:
 * !!!! FIXME !!!!
 *   DDC1 & DDC2
 *   shadowfb
 *   overlay planes
 *
 * Modified by Marc Aurele La France (tsi@xfree86.org) for ATI driver merge.
 *
 * Mergedfb and pseudo xinerama support added by Alex Deucher (agd5f@yahoo.com)
 * based on the sis driver by Thomas Winischhofer.
 *
 */

#include <string.h>
#include <stdio.h>

				/* Driver data structures */
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_version.h"

#ifdef XF86DRI
#define _XF86DRI_SERVER_
#include "radeon_dri.h"
#include "radeon_sarea.h"
#include "sarea.h"
#endif

#include "fb.h"

				/* colormap initialization */
#include "micmap.h"
#include "dixstruct.h"

				/* X and server generic header files */
#include "xf86.h"
#include "xf86_ansic.h"		/* For xf86getsecs() */
#include "xf86_OSproc.h"
#include "xf86RAC.h"
#include "xf86RandR12.h"
#include "xf86Resources.h"
#include "xf86cmap.h"
#include "vbe.h"

				/* fbdevhw * vgaHW definitions */
#ifdef WITH_VGAHW
#include "vgaHW.h"
#endif
#include "fbdevhw.h"

#define DPMS_SERVER
#include <X11/extensions/dpms.h>

#include "atipciids.h"
#include "radeon_chipset.h"



				/* Forward definitions for driver functions */
static Bool RADEONCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool RADEONSaveScreen(ScreenPtr pScreen, int mode);
static void RADEONSave(ScrnInfoPtr pScrn);

static void RADEONSetDynamicClock(ScrnInfoPtr pScrn, int mode);
static void RADEONForceSomeClocks(ScrnInfoPtr pScrn);
static void RADEONSaveMemMapRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save);

#ifdef XF86DRI
static void RADEONAdjustMemMapRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save);
#endif

DisplayModePtr
RADEONCrtcFindClosestMode(xf86CrtcPtr crtc, DisplayModePtr pMode);

static void RADEONWaitPLLLock(ScrnInfoPtr pScrn, unsigned nTests,
                              unsigned nWaitLoops, unsigned cntThreshold);

static const OptionInfoRec RADEONOptions[] = {
    { OPTION_NOACCEL,        "NoAccel",          OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_SW_CURSOR,      "SWcursor",         OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DAC_6BIT,       "Dac6Bit",          OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DAC_8BIT,       "Dac8Bit",          OPTV_BOOLEAN, {0}, TRUE  },
#ifdef XF86DRI
    { OPTION_BUS_TYPE,       "BusType",          OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_CP_PIO,         "CPPIOMode",        OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_USEC_TIMEOUT,   "CPusecTimeout",    OPTV_INTEGER, {0}, FALSE },
    { OPTION_AGP_MODE,       "AGPMode",          OPTV_INTEGER, {0}, FALSE },
    { OPTION_AGP_FW,         "AGPFastWrite",     OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_GART_SIZE_OLD,  "AGPSize",          OPTV_INTEGER, {0}, FALSE },
    { OPTION_GART_SIZE,      "GARTSize",         OPTV_INTEGER, {0}, FALSE },
    { OPTION_RING_SIZE,      "RingSize",         OPTV_INTEGER, {0}, FALSE },
    { OPTION_BUFFER_SIZE,    "BufferSize",       OPTV_INTEGER, {0}, FALSE },
    { OPTION_DEPTH_MOVE,     "EnableDepthMoves", OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_PAGE_FLIP,      "EnablePageFlip",   OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_NO_BACKBUFFER,  "NoBackBuffer",     OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_XV_DMA,         "DMAForXv",         OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_FBTEX_PERCENT,  "FBTexPercent",     OPTV_INTEGER, {0}, FALSE },
    { OPTION_DEPTH_BITS,     "DepthBits",        OPTV_INTEGER, {0}, FALSE },
    { OPTION_PCIAPER_SIZE,  "PCIAPERSize",      OPTV_INTEGER, {0}, FALSE },
#ifdef USE_EXA
    { OPTION_ACCEL_DFS,      "AccelDFS",         OPTV_BOOLEAN, {0}, FALSE },
#endif
#endif
    { OPTION_DDC_MODE,       "DDCMode",          OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_IGNORE_EDID,    "IgnoreEDID",       OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_FBDEV,          "UseFBDev",         OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DISP_PRIORITY,  "DisplayPriority",  OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_PANEL_SIZE,     "PanelSize",        OPTV_ANYSTR,  {0}, FALSE },
    { OPTION_MIN_DOTCLOCK,   "ForceMinDotClock", OPTV_FREQ,    {0}, FALSE },
    { OPTION_COLOR_TILING,   "ColorTiling",      OPTV_BOOLEAN, {0}, FALSE },
#ifdef XvExtension
    { OPTION_VIDEO_KEY,                   "VideoKey",                 OPTV_INTEGER, {0}, FALSE },
    { OPTION_RAGE_THEATRE_CRYSTAL,        "RageTheatreCrystal",       OPTV_INTEGER, {0}, FALSE },
    { OPTION_RAGE_THEATRE_TUNER_PORT,     "RageTheatreTunerPort",     OPTV_INTEGER, {0}, FALSE },
    { OPTION_RAGE_THEATRE_COMPOSITE_PORT, "RageTheatreCompositePort", OPTV_INTEGER, {0}, FALSE },
    { OPTION_RAGE_THEATRE_SVIDEO_PORT,    "RageTheatreSVideoPort",    OPTV_INTEGER, {0}, FALSE },
    { OPTION_TUNER_TYPE,                  "TunerType",                OPTV_INTEGER, {0}, FALSE },
    { OPTION_RAGE_THEATRE_MICROC_PATH,    "RageTheatreMicrocPath",    OPTV_STRING, {0}, FALSE },
    { OPTION_RAGE_THEATRE_MICROC_TYPE,    "RageTheatreMicrocType",    OPTV_STRING, {0}, FALSE },
    { OPTION_SCALER_WIDTH,                "ScalerWidth",              OPTV_INTEGER, {0}, FALSE }, 
#endif
#ifdef RENDER
    { OPTION_RENDER_ACCEL,   "RenderAccel",      OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_SUBPIXEL_ORDER, "SubPixelOrder",    OPTV_ANYSTR,  {0}, FALSE },
#endif
    { OPTION_SHOWCACHE,      "ShowCache",        OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DYNAMIC_CLOCKS, "DynamicClocks",    OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_VGA_ACCESS,     "VGAAccess",        OPTV_BOOLEAN, {0}, TRUE  },
    { OPTION_REVERSE_DDC,    "ReverseDDC",       OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_LVDS_PROBE_PLL, "LVDSProbePLL",     OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_ACCELMETHOD,    "AccelMethod",      OPTV_STRING,  {0}, FALSE },
    { OPTION_CONSTANTDPI,    "ConstantDPI",	 OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_DRI,            "DRI",       	 OPTV_BOOLEAN, {0}, FALSE },
    { OPTION_CONNECTORTABLE, "ConnectorTable",   OPTV_STRING,  {0}, FALSE },
    { OPTION_DEFAULT_CONNECTOR_TABLE, "DefaultConnectorTable", OPTV_BOOLEAN, {0}, FALSE },
    { -1,                    NULL,               OPTV_NONE,    {0}, FALSE }
};

const OptionInfoRec *RADEONOptionsWeak(void) { return RADEONOptions; }

#ifdef WITH_VGAHW
static const char *vgahwSymbols[] = {
    "vgaHWFreeHWRec",
    "vgaHWGetHWRec",
    "vgaHWGetIndex",
    "vgaHWLock",
    "vgaHWRestore",
    "vgaHWSave",
    "vgaHWUnlock",
    "vgaHWGetIOBase",
    NULL
};
#endif

static const char *fbdevHWSymbols[] = {
    "fbdevHWInit",
    "fbdevHWUseBuildinMode",

    "fbdevHWGetVidmem",

    "fbdevHWDPMSSet",

    /* colormap */
    "fbdevHWLoadPalette",
    /* ScrnInfo hooks */
    "fbdevHWAdjustFrame",
    "fbdevHWEnterVT",
    "fbdevHWLeaveVT",
    "fbdevHWModeInit",
    "fbdevHWRestore",
    "fbdevHWSave",
    "fbdevHWSwitchMode",
    "fbdevHWValidModeWeak",

    "fbdevHWMapMMIO",
    "fbdevHWMapVidmem",
    "fbdevHWUnmapMMIO",
    "fbdevHWUnmapVidmem",

    NULL
};

static const char *ddcSymbols[] = {
    "xf86PrintEDID",
    "xf86DoEDID_DDC1",
    "xf86DoEDID_DDC2",
    NULL
};

static const char *fbSymbols[] = {
    "fbScreenInit",
    "fbPictureInit",
    NULL
};


#ifdef USE_EXA
static const char *exaSymbols[] = {
    "exaDriverAlloc",
    "exaDriverInit",
    "exaDriverFini",
    "exaOffscreenAlloc",
    "exaOffscreenFree",
    "exaGetPixmapOffset",
    "exaGetPixmapPitch",
    "exaGetPixmapSize",
    "exaMarkSync",
    "exaWaitSync",
    NULL
};
#endif /* USE_EXA */

#ifdef USE_XAA
static const char *xaaSymbols[] = {
    "XAACreateInfoRec",
    "XAADestroyInfoRec",
    "XAAInit",
    NULL
};
#endif /* USE_XAA */

#if 0
static const char *xf8_32bppSymbols[] = {
    "xf86Overlay8Plus32Init",
    NULL
};
#endif

static const char *ramdacSymbols[] = {
    "xf86CreateCursorInfoRec",
    "xf86DestroyCursorInfoRec",
    "xf86ForceHWCursor",
    "xf86InitCursor",
    NULL
};

#ifdef XF86DRI
static const char *drmSymbols[] = {
    "drmGetInterruptFromBusID",
    "drmCtlInstHandler",
    "drmCtlUninstHandler",
    "drmAddBufs",
    "drmAddMap",
    "drmAgpAcquire",
    "drmAgpAlloc",
    "drmAgpBase",
    "drmAgpBind",
    "drmAgpDeviceId",
    "drmAgpEnable",
    "drmAgpFree",
    "drmAgpGetMode",
    "drmAgpRelease",
    "drmAgpUnbind",
    "drmAgpVendorId",
    "drmCommandNone",
    "drmCommandRead",
    "drmCommandWrite",
    "drmCommandWriteRead",
    "drmDMA",
    "drmFreeVersion",
    "drmGetLibVersion",
    "drmGetVersion",
    "drmMap",
    "drmMapBufs",
    "drmRadeonCleanupCP",
    "drmRadeonClear",
    "drmRadeonFlushIndirectBuffer",
    "drmRadeonInitCP",
    "drmRadeonResetCP",
    "drmRadeonStartCP",
    "drmRadeonStopCP",
    "drmRadeonWaitForIdleCP",
    "drmScatterGatherAlloc",
    "drmScatterGatherFree",
    "drmUnmap",
    "drmUnmapBufs",
    NULL
};

static const char *driSymbols[] = {
    "DRICloseScreen",
    "DRICreateInfoRec",
    "DRIDestroyInfoRec",
    "DRIFinishScreenInit",
    "DRIGetContext",
    "DRIGetDeviceInfo",
    "DRIGetSAREAPrivate",
    "DRILock",
    "DRIQueryVersion",
    "DRIScreenInit",
    "DRIUnlock",
    "GlxSetVisualConfigs",
    "DRICreatePCIBusID",
    NULL
};
#endif

static const char *vbeSymbols[] = {
    "VBEInit",
    "vbeDoEDID",
    NULL
};

static const char *int10Symbols[] = {
    "xf86InitInt10",
    "xf86FreeInt10",
    "xf86int10Addr",
    "xf86ExecX86int10",
    NULL
};

static const char *i2cSymbols[] = {
    "xf86CreateI2CBusRec",
    "xf86I2CBusInit",
    NULL
};

void RADEONLoaderRefSymLists(void)
{
    /*
     * Tell the loader about symbols from other modules that this module might
     * refer to.
     */
    xf86LoaderRefSymLists(
#ifdef WITH_VGAHW
			  vgahwSymbols,
#endif
			  fbSymbols,
#ifdef USE_EXA
			  exaSymbols,
#endif
#ifdef USE_XAA
			  xaaSymbols,
#endif
#if 0
			  xf8_32bppSymbols,
#endif
			  ramdacSymbols,
#ifdef XF86DRI
			  drmSymbols,
			  driSymbols,
#endif
			  fbdevHWSymbols,
			  vbeSymbols,
			  int10Symbols,
			  i2cSymbols,
			  ddcSymbols,
			  NULL);
}

#ifdef XFree86LOADER
static int getRADEONEntityIndex(void)
{
    int *radeon_entity_index = LoaderSymbol("gRADEONEntityIndex");
    if (!radeon_entity_index)
        return -1;
    else
        return *radeon_entity_index;
}
#else
extern int gRADEONEntityIndex;
static int getRADEONEntityIndex(void)
{
    return gRADEONEntityIndex;
}
#endif

struct RADEONInt10Save {
	CARD32 MEM_CNTL;
	CARD32 MEMSIZE;
	CARD32 MPP_TB_CONFIG;
};

static Bool RADEONMapMMIO(ScrnInfoPtr pScrn);
static Bool RADEONUnmapMMIO(ScrnInfoPtr pScrn);

#if 0
static Bool
RADEONCreateScreenResources (ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   RADEONInfoPtr  info   = RADEONPTR(pScrn);

   pScreen->CreateScreenResources = info->CreateScreenResources;
   if (!(*pScreen->CreateScreenResources)(pScreen))
      return FALSE;

   if (!xf86RandR12CreateScreenResources(pScreen))
      return FALSE;

  return TRUE;
}
#endif

RADEONEntPtr RADEONEntPriv(ScrnInfoPtr pScrn)
{
    DevUnion     *pPriv;
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    pPriv = xf86GetEntityPrivate(info->pEnt->index,
                                 getRADEONEntityIndex());
    return pPriv->ptr;
}

static void
RADEONPreInt10Save(ScrnInfoPtr pScrn, void **pPtr)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 CardTmp;
    static struct RADEONInt10Save SaveStruct = { 0, 0, 0 };

    /* Save the values and zap MEM_CNTL */
    SaveStruct.MEM_CNTL = INREG(RADEON_MEM_CNTL);
    SaveStruct.MEMSIZE = INREG(RADEON_CONFIG_MEMSIZE);
    SaveStruct.MPP_TB_CONFIG = INREG(RADEON_MPP_TB_CONFIG);

    /*
     * Zap MEM_CNTL and set MPP_TB_CONFIG<31:24> to 4
     */
    OUTREG(RADEON_MEM_CNTL, 0);
    CardTmp = SaveStruct.MPP_TB_CONFIG & 0x00ffffffu;
    CardTmp |= 0x04 << 24;
    OUTREG(RADEON_MPP_TB_CONFIG, CardTmp);

    *pPtr = (void *)&SaveStruct;
}

static void
RADEONPostInt10Check(ScrnInfoPtr pScrn, void *ptr)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    struct RADEONInt10Save *pSave = ptr;
    CARD32 CardTmp;

    /* If we don't have a valid (non-zero) saved MEM_CNTL, get out now */
    if (!pSave || !pSave->MEM_CNTL)
	return;

    /*
     * If either MEM_CNTL is currently zero or inconistent (configured for
     * two channels with the two channels configured differently), restore
     * the saved registers.
     */
    CardTmp = INREG(RADEON_MEM_CNTL);
    if (!CardTmp ||
	((CardTmp & 1) &&
	 (((CardTmp >> 8) & 0xff) != ((CardTmp >> 24) & 0xff)))) {
	/* Restore the saved registers */
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Restoring MEM_CNTL (%08lx), setting to %08lx\n",
		   (unsigned long)CardTmp, (unsigned long)pSave->MEM_CNTL);
	OUTREG(RADEON_MEM_CNTL, pSave->MEM_CNTL);

	CardTmp = INREG(RADEON_CONFIG_MEMSIZE);
	if (CardTmp != pSave->MEMSIZE) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Restoring CONFIG_MEMSIZE (%08lx), setting to %08lx\n",
		       (unsigned long)CardTmp, (unsigned long)pSave->MEMSIZE);
	    OUTREG(RADEON_CONFIG_MEMSIZE, pSave->MEMSIZE);
	}
    }

    CardTmp = INREG(RADEON_MPP_TB_CONFIG);
    if ((CardTmp & 0xff000000u) != (pSave->MPP_TB_CONFIG & 0xff000000u)) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	           "Restoring MPP_TB_CONFIG<31:24> (%02lx), setting to %02lx\n",
	 	   (unsigned long)CardTmp >> 24,
		   (unsigned long)pSave->MPP_TB_CONFIG >> 24);
	CardTmp &= 0x00ffffffu;
	CardTmp |= (pSave->MPP_TB_CONFIG & 0xff000000u);
	OUTREG(RADEON_MPP_TB_CONFIG, CardTmp);
    }
}

/* Allocate our private RADEONInfoRec */
static Bool RADEONGetRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate) return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(RADEONInfoRec), 1);
    return TRUE;
}

/* Free our private RADEONInfoRec */
static void RADEONFreeRec(ScrnInfoPtr pScrn)
{
    if (!pScrn || !pScrn->driverPrivate) return;
    xfree(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

/* Memory map the MMIO region.  Used during pre-init and by RADEONMapMem,
 * below
 */
static Bool RADEONMapMMIO(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (info->FBDev) {
	info->MMIO = fbdevHWMapMMIO(pScrn);
    } else {
	info->MMIO = xf86MapPciMem(pScrn->scrnIndex,
				   VIDMEM_MMIO | VIDMEM_READSIDEEFFECT,
				   info->PciTag,
				   info->MMIOAddr,
				   info->MMIOSize);
    }

    if (!info->MMIO) return FALSE;
    return TRUE;
}

/* Unmap the MMIO region.  Used during pre-init and by RADEONUnmapMem,
 * below
 */
static Bool RADEONUnmapMMIO(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (info->FBDev)
	fbdevHWUnmapMMIO(pScrn);
    else {
	xf86UnMapVidMem(pScrn->scrnIndex, info->MMIO, info->MMIOSize);
    }
    info->MMIO = NULL;
    return TRUE;
}

/* Memory map the frame buffer.  Used by RADEONMapMem, below. */
static Bool RADEONMapFB(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (info->FBDev) {
	info->FB = fbdevHWMapVidmem(pScrn);
    } else {
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		       "Map: 0x%08lx, 0x%08lx\n", info->LinearAddr, info->FbMapSize);
	info->FB = xf86MapPciMem(pScrn->scrnIndex,
				 VIDMEM_FRAMEBUFFER,
				 info->PciTag,
				 info->LinearAddr,
				 info->FbMapSize);
    }

    if (!info->FB) return FALSE;
    return TRUE;
}

/* Unmap the frame buffer.  Used by RADEONUnmapMem, below. */
static Bool RADEONUnmapFB(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (info->FBDev)
	fbdevHWUnmapVidmem(pScrn);
    else
	xf86UnMapVidMem(pScrn->scrnIndex, info->FB, info->FbMapSize);
    info->FB = NULL;
    return TRUE;
}

/* Memory map the MMIO region and the frame buffer */
static Bool RADEONMapMem(ScrnInfoPtr pScrn)
{
    if (!RADEONMapMMIO(pScrn)) return FALSE;
    if (!RADEONMapFB(pScrn)) {
	RADEONUnmapMMIO(pScrn);
	return FALSE;
    }
    return TRUE;
}

/* Unmap the MMIO region and the frame buffer */
static Bool RADEONUnmapMem(ScrnInfoPtr pScrn)
{
    if (!RADEONUnmapMMIO(pScrn) || !RADEONUnmapFB(pScrn)) return FALSE;
    return TRUE;
}

void RADEONPllErrataAfterIndex(RADEONInfoPtr info)
{
    unsigned char *RADEONMMIO = info->MMIO;
	
    if (!(info->ChipErrata & CHIP_ERRATA_PLL_DUMMYREADS))
	return;

    /* This workaround is necessary on rv200 and RS200 or PLL
     * reads may return garbage (among others...)
     */
    (void)INREG(RADEON_CLOCK_CNTL_DATA);
    (void)INREG(RADEON_CRTC_GEN_CNTL);
}

void RADEONPllErrataAfterData(RADEONInfoPtr info)
{
    unsigned char *RADEONMMIO = info->MMIO;

    /* This workarounds is necessary on RV100, RS100 and RS200 chips
     * or the chip could hang on a subsequent access
     */
    if (info->ChipErrata & CHIP_ERRATA_PLL_DELAY) {
	/* we can't deal with posted writes here ... */
	usleep(5000);
    }

    /* This function is required to workaround a hardware bug in some (all?)
     * revisions of the R300.  This workaround should be called after every
     * CLOCK_CNTL_INDEX register access.  If not, register reads afterward
     * may not be correct.
     */
    if (info->ChipErrata & CHIP_ERRATA_R300_CG) {
	CARD32         save, tmp;

	save = INREG(RADEON_CLOCK_CNTL_INDEX);
	tmp = save & ~(0x3f | RADEON_PLL_WR_EN);
	OUTREG(RADEON_CLOCK_CNTL_INDEX, tmp);
	tmp = INREG(RADEON_CLOCK_CNTL_DATA);
	OUTREG(RADEON_CLOCK_CNTL_INDEX, save);
    }
}

/* Read PLL register */
unsigned RADEONINPLL(ScrnInfoPtr pScrn, int addr)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32         data;

    OUTREG8(RADEON_CLOCK_CNTL_INDEX, addr & 0x3f);
    RADEONPllErrataAfterIndex(info);
    data = INREG(RADEON_CLOCK_CNTL_DATA);
    RADEONPllErrataAfterData(info);

    return data;
}

/* Write PLL information */
void RADEONOUTPLL(ScrnInfoPtr pScrn, int addr, CARD32 data)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG8(RADEON_CLOCK_CNTL_INDEX, (((addr) & 0x3f) |
				      RADEON_PLL_WR_EN));
    RADEONPllErrataAfterIndex(info);
    OUTREG(RADEON_CLOCK_CNTL_DATA, data);
    RADEONPllErrataAfterData(info);
}


#if 0
/* Read PAL information (only used for debugging) */
static int RADEONINPAL(int idx)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_PALETTE_INDEX, idx << 16);
    return INREG(RADEON_PALETTE_DATA);
}
#endif

/* Wait for vertical sync on primary CRTC */
void RADEONWaitForVerticalSync(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32         crtc_gen_cntl;
    struct timeval timeout;

    crtc_gen_cntl = INREG(RADEON_CRTC_GEN_CNTL);
    if ((crtc_gen_cntl & RADEON_CRTC_DISP_REQ_EN_B) ||
	!(crtc_gen_cntl & RADEON_CRTC_EN))
	return;

    /* Clear the CRTC_VBLANK_SAVE bit */
    OUTREG(RADEON_CRTC_STATUS, RADEON_CRTC_VBLANK_SAVE_CLEAR);

    /* Wait for it to go back up */
    radeon_init_timeout(&timeout, RADEON_VSYNC_TIMEOUT);
    while (!(INREG(RADEON_CRTC_STATUS) & RADEON_CRTC_VBLANK_SAVE) &&
        !radeon_timedout(&timeout))
	usleep(100);
}

/* Wait for vertical sync on secondary CRTC */
void RADEONWaitForVerticalSync2(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32         crtc2_gen_cntl;
    struct timeval timeout;
 
    crtc2_gen_cntl = INREG(RADEON_CRTC2_GEN_CNTL);
    if ((crtc2_gen_cntl & RADEON_CRTC2_DISP_REQ_EN_B) ||
	!(crtc2_gen_cntl & RADEON_CRTC2_EN))
	return;

    /* Clear the CRTC2_VBLANK_SAVE bit */
    OUTREG(RADEON_CRTC2_STATUS, RADEON_CRTC2_VBLANK_SAVE_CLEAR);

    /* Wait for it to go back up */
    radeon_init_timeout(&timeout, RADEON_VSYNC_TIMEOUT);
    while (!(INREG(RADEON_CRTC2_STATUS) & RADEON_CRTC2_VBLANK_SAVE) &&
        !radeon_timedout(&timeout))
	usleep(100);
}


/* Compute log base 2 of val */
int RADEONMinBits(int val)
{
    int  bits;

    if (!val) return 1;
    for (bits = 0; val; val >>= 1, ++bits);
    return bits;
}

/* Compute n/d with rounding */
static int RADEONDiv(int n, int d)
{
    return (n + (d / 2)) / d;
}

static Bool RADEONProbePLLParameters(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONPLLPtr  pll  = &info->pll;
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned char ppll_div_sel;
    unsigned mpll_fb_div, spll_fb_div, M;
    unsigned xclk, tmp, ref_div;
    int hTotal, vTotal, num, denom, m, n;
    float hz, prev_xtal, vclk, xtal, mpll, spll;
    long start_secs, start_usecs, stop_secs, stop_usecs, total_usecs;
    long to1_secs, to1_usecs, to2_secs, to2_usecs;
    unsigned int f1, f2, f3;
    int tries = 0;

    prev_xtal = 0;
 again:
    xtal = 0;
    if (++tries > 10)
           goto failed;

    xf86getsecs(&to1_secs, &to1_usecs);
    f1 = INREG(RADEON_CRTC_CRNT_FRAME);
    for (;;) {
       f2 = INREG(RADEON_CRTC_CRNT_FRAME);
       if (f1 != f2)
	    break;
       xf86getsecs(&to2_secs, &to2_usecs);
       if ((to2_secs - to1_secs) > 1) {
           xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Clock not counting...\n");
           goto failed;
       }
    }
    xf86getsecs(&start_secs, &start_usecs);
    for(;;) {
       f3 = INREG(RADEON_CRTC_CRNT_FRAME);
       if (f3 != f2)
	    break;
       xf86getsecs(&to2_secs, &to2_usecs);
       if ((to2_secs - start_secs) > 1)
           goto failed;
    }
    xf86getsecs(&stop_secs, &stop_usecs);

    if ((stop_secs - start_secs) != 0)
           goto again;
    total_usecs = abs(stop_usecs - start_usecs);
    if (total_usecs == 0)
           goto again;
    hz = 1000000.0/(float)total_usecs;

    hTotal = ((INREG(RADEON_CRTC_H_TOTAL_DISP) & 0x3ff) + 1) * 8;
    vTotal = ((INREG(RADEON_CRTC_V_TOTAL_DISP) & 0xfff) + 1);
    vclk = (float)(hTotal * (float)(vTotal * hz));

    switch((INPLL(pScrn, RADEON_PPLL_REF_DIV) & 0x30000) >> 16) {
    case 0:
    default:
        num = 1;
        denom = 1;
        break;
    case 1:
        n = ((INPLL(pScrn, RADEON_X_MPLL_REF_FB_DIV) >> 16) & 0xff);
        m = (INPLL(pScrn, RADEON_X_MPLL_REF_FB_DIV) & 0xff);
        num = 2*n;
        denom = 2*m;
        break;
    case 2:
        n = ((INPLL(pScrn, RADEON_X_MPLL_REF_FB_DIV) >> 8) & 0xff);
        m = (INPLL(pScrn, RADEON_X_MPLL_REF_FB_DIV) & 0xff);
        num = 2*n;
        denom = 2*m;
        break;
     }

    ppll_div_sel = INREG8(RADEON_CLOCK_CNTL_INDEX + 1) & 0x3;
    RADEONPllErrataAfterIndex(info);

    n = (INPLL(pScrn, RADEON_PPLL_DIV_0 + ppll_div_sel) & 0x7ff);
    m = (INPLL(pScrn, RADEON_PPLL_REF_DIV) & 0x3ff);

    num *= n;
    denom *= m;

    switch ((INPLL(pScrn, RADEON_PPLL_DIV_0 + ppll_div_sel) >> 16) & 0x7) {
    case 1:
        denom *= 2;
        break;
    case 2:
        denom *= 4;
        break;
    case 3:
        denom *= 8;
        break;
    case 4:
        denom *= 3;
        break;
    case 6:
        denom *= 6;
        break;
    case 7:
        denom *= 12;
        break;
    }

    xtal = (int)(vclk *(float)denom/(float)num);

    if ((xtal > 26900000) && (xtal < 27100000))
        xtal = 2700;
    else if ((xtal > 14200000) && (xtal < 14400000))
        xtal = 1432;
    else if ((xtal > 29400000) && (xtal < 29600000))
        xtal = 2950;
    else
       goto again;
 failed:
    if (xtal == 0) {
       xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Failed to probe xtal value ! "
                  "Using default 27Mhz\n");
       xtal = 2700;
    } else {
       if (prev_xtal == 0) {
           prev_xtal = xtal;
           tries = 0;
           goto again;
       } else if (prev_xtal != xtal) {
           prev_xtal = 0;
           goto again;
       }
    }

    tmp = INPLL(pScrn, RADEON_X_MPLL_REF_FB_DIV);
    ref_div = INPLL(pScrn, RADEON_PPLL_REF_DIV) & 0x3ff;

    /* Some sanity check based on the BIOS code .... */
    if (ref_div < 2) {
       CARD32 tmp;
       tmp = INPLL(pScrn, RADEON_PPLL_REF_DIV);
       if (IS_R300_VARIANT || (info->ChipFamily == CHIP_FAMILY_RS300)
			   || (info->ChipFamily == CHIP_FAMILY_RS400))
           ref_div = (tmp & R300_PPLL_REF_DIV_ACC_MASK) >>
                   R300_PPLL_REF_DIV_ACC_SHIFT;
       else
           ref_div = tmp & RADEON_PPLL_REF_DIV_MASK;
       if (ref_div < 2)
           ref_div = 12;
    }

    /* Calculate "base" xclk straight from MPLL, though that isn't
     * really useful (hopefully). This isn't called XCLK anymore on
     * radeon's...
     */
    mpll_fb_div = (tmp & 0xff00) >> 8;
    spll_fb_div = (tmp & 0xff0000) >> 16;
    M = (tmp & 0xff);
    xclk = RADEONDiv((2 * mpll_fb_div * xtal), (M));

    /*
     * Calculate MCLK based on MCLK-A
     */
    mpll = (2.0 * (float)mpll_fb_div * (xtal / 100.0)) / (float)M;
    spll = (2.0 * (float)spll_fb_div * (xtal / 100.0)) / (float)M;

    tmp = INPLL(pScrn, RADEON_MCLK_CNTL) & 0x7;
    switch(tmp) {
    case 1: info->mclk = mpll; break;
    case 2: info->mclk = mpll / 2.0; break;
    case 3: info->mclk = mpll / 4.0; break;
    case 4: info->mclk = mpll / 8.0; break;
    case 7: info->mclk = spll; break;
    default:
           info->mclk = 200.00;
           xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Unsupported MCLKA source"
                      " setting %d, can't probe MCLK value !\n", tmp);
    }

    /*
     * Calculate SCLK
     */
    tmp = INPLL(pScrn, RADEON_SCLK_CNTL) & 0x7;
    switch(tmp) {
    case 1: info->sclk = spll; break;
    case 2: info->sclk = spll / 2.0; break;
    case 3: info->sclk = spll / 4.0; break;
    case 4: info->sclk = spll / 8.0; break;
    case 7: info->sclk = mpll;
    default:
           info->sclk = 200.00;
           xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Unsupported SCLK source"
                      " setting %d, can't probe SCLK value !\n", tmp);
    }

    /* we're done, hopefully these are sane values */
    pll->reference_div = ref_div;
    pll->xclk = xclk;
    pll->reference_freq = xtal;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Probed PLL values: xtal: %f Mhz, "
              "sclk: %f Mhz, mclk: %f Mhz\n", xtal/100.0, info->sclk, info->mclk);

    return TRUE;
}

static void RADEONGetClockInfo(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    RADEONPLLPtr pll = &info->pll;
    double min_dotclock;

    if (RADEONGetClockInfoFromBIOS(pScrn)) {
	if (pll->reference_div < 2) {
	    /* retrive it from register setting for fitting into current PLL algorithm.
	       We'll probably need a new routine to calculate the best ref_div from BIOS 
	       provided min_input_pll and max_input_pll 
	    */
	    CARD32 tmp;
	    tmp = INPLL(pScrn, RADEON_PPLL_REF_DIV);
	    if (IS_R300_VARIANT ||
		(info->ChipFamily == CHIP_FAMILY_RS300) ||
		(info->ChipFamily == CHIP_FAMILY_RS400)) {
		pll->reference_div = (tmp & R300_PPLL_REF_DIV_ACC_MASK) >> R300_PPLL_REF_DIV_ACC_SHIFT;
	    } else {
		pll->reference_div = tmp & RADEON_PPLL_REF_DIV_MASK;
	    }

	    if (pll->reference_div < 2) pll->reference_div = 12;
	}
	
    } else {
	xf86DrvMsg (pScrn->scrnIndex, X_WARNING,
		    "Video BIOS not detected, using default clock settings!\n");

       /* Default min/max PLL values */
       if (info->ChipFamily == CHIP_FAMILY_R420 || info->ChipFamily == CHIP_FAMILY_RV410) {
           pll->min_pll_freq = 20000;
           pll->max_pll_freq = 50000;
       } else {
           pll->min_pll_freq = 12500;
           pll->max_pll_freq = 35000;
       }

       if (RADEONProbePLLParameters(pScrn))
            return;

	if (info->IsIGP)
	    pll->reference_freq = 1432;
	else
	    pll->reference_freq = 2700;

	pll->reference_div = 12;
	pll->xclk = 10300;

        info->sclk = 200.00;
        info->mclk = 200.00;
    }

    if (info->ChipFamily == CHIP_FAMILY_RV100 && !pRADEONEnt->HasCRTC2) {
        /* Avoid RN50 corruption due to memory bandwidth starvation.
         * 18 is an empirical value based on the databook and Windows driver.
         *
	 * Empirical value changed to 24 to raise pixel clock limit and
	 * allow higher resolution modes on capable monitors
	 */
        pll->max_pll_freq = min(pll->max_pll_freq,
                               24 * info->mclk * 100 / pScrn->bitsPerPixel *
                               info->RamWidth / 16);
    }

    xf86DrvMsg (pScrn->scrnIndex, X_INFO,
		"PLL parameters: rf=%d rd=%d min=%ld max=%ld; xclk=%d\n",
		pll->reference_freq,
		pll->reference_div,
		pll->min_pll_freq, pll->max_pll_freq, pll->xclk);

    /* (Some?) Radeon BIOSes seem too lie about their minimum dot
     * clocks.  Allow users to override the detected minimum dot clock
     * value (e.g., and allow it to be suitable for TV sets).
     */
    if (xf86GetOptValFreq(info->Options, OPTION_MIN_DOTCLOCK,
			  OPTUNITS_MHZ, &min_dotclock)) {
	if (min_dotclock < 12 || min_dotclock*100 >= pll->max_pll_freq) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Illegal minimum dotclock specified %.2f MHz "
		       "(option ignored)\n",
		       min_dotclock);
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Forced minimum dotclock to %.2f MHz "
		       "(instead of detected %.2f MHz)\n",
		       min_dotclock, ((double)pll->min_pll_freq/1000));
	    pll->min_pll_freq = min_dotclock * 1000;
	}
    }
}



/* This is called by RADEONPreInit to set up the default visual */
static Bool RADEONPreInitVisual(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb))
	return FALSE;

    switch (pScrn->depth) {
    case 8:
    case 15:
    case 16:
    case 24:
	break;

    default:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Given depth (%d) is not supported by %s driver\n",
		   pScrn->depth, RADEON_DRIVER_NAME);
	return FALSE;
    }

    xf86PrintDepthBpp(pScrn);

    info->fifo_slots                 = 0;
    info->pix24bpp                   = xf86GetBppFromDepth(pScrn,
							   pScrn->depth);
    info->CurrentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    info->CurrentLayout.depth        = pScrn->depth;
    info->CurrentLayout.pixel_bytes  = pScrn->bitsPerPixel / 8;
    info->CurrentLayout.pixel_code   = (pScrn->bitsPerPixel != 16
				       ? pScrn->bitsPerPixel
				       : pScrn->depth);

    if (info->pix24bpp == 24) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Radeon does NOT support 24bpp\n");
	return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Pixel depth = %d bits stored in %d byte%s (%d bpp pixmaps)\n",
	       pScrn->depth,
	       info->CurrentLayout.pixel_bytes,
	       info->CurrentLayout.pixel_bytes > 1 ? "s" : "",
	       info->pix24bpp);

    if (!xf86SetDefaultVisual(pScrn, -1)) return FALSE;

    if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Default visual (%s) is not supported at depth %d\n",
		   xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
	return FALSE;
    }
    return TRUE;
}

/* This is called by RADEONPreInit to handle all color weight issues */
static Bool RADEONPreInitWeight(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

				/* Save flag for 6 bit DAC to use for
				   setting CRTC registers.  Otherwise use
				   an 8 bit DAC, even if xf86SetWeight sets
				   pScrn->rgbBits to some value other than
				   8. */
    info->dac6bits = FALSE;

    if (pScrn->depth > 8) {
	rgb  defaultWeight = { 0, 0, 0 };

	if (!xf86SetWeight(pScrn, defaultWeight, defaultWeight)) return FALSE;
    } else {
	pScrn->rgbBits = 8;
	if (xf86ReturnOptValBool(info->Options, OPTION_DAC_6BIT, FALSE)) {
	    pScrn->rgbBits = 6;
	    info->dac6bits = TRUE;
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Using %d bits per RGB (%d bit DAC)\n",
	       pScrn->rgbBits, info->dac6bits ? 6 : 8);

    return TRUE;
}

void RADEONInitMemMapRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save,
				      RADEONInfoPtr info)
{
    save->mc_fb_location = info->mc_fb_location;
    save->mc_agp_location = info->mc_agp_location;
    save->display_base_addr = info->fbLocation;
    save->display2_base_addr = info->fbLocation;
    save->ov0_base_addr = info->fbLocation;
}

static void RADEONInitMemoryMap(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned long mem_size;
    CARD32 aper_size;

    /* Default to existing values */
    info->mc_fb_location = INREG(RADEON_MC_FB_LOCATION);
    info->mc_agp_location = INREG(RADEON_MC_AGP_LOCATION);

    /* We shouldn't use info->videoRam here which might have been clipped
     * but the real video RAM instead
     */
    mem_size = INREG(RADEON_CONFIG_MEMSIZE);
    aper_size = INREG(RADEON_CONFIG_APER_SIZE);
    if (mem_size == 0)
	    mem_size = 0x800000;

    /* Fix for RN50, M6, M7 with 8/16/32(??) MBs of VRAM - 
       Novell bug 204882 + along with lots of ubuntu ones */
    if (aper_size > mem_size)
	mem_size = aper_size;

#ifdef XF86DRI
    /* Apply memory map limitation if using an old DRI */
    if (info->directRenderingEnabled && !info->newMemoryMap) {
	    if (aper_size < mem_size)
		mem_size = aper_size;
    }
#endif

    /* We won't try to change MC_FB_LOCATION when using fbdev */
    if (!info->FBDev) {
	if (info->IsIGP)
	    info->mc_fb_location = INREG(RADEON_NB_TOM);
	else
#ifdef XF86DRI
	/* Old DRI has restrictions on the memory map */
	if ( info->directRenderingEnabled &&
	     info->pKernelDRMVersion->version_minor < 10 )
	    info->mc_fb_location = (mem_size - 1) & 0xffff0000U;
	else
#endif
	{
	    CARD32 aper0_base = INREG(RADEON_CONFIG_APER_0_BASE);

	    /* Recent chips have an "issue" with the memory controller, the
	     * location must be aligned to the size. We just align it down,
	     * too bad if we walk over the top of system memory, we don't
	     * use DMA without a remapped anyway.
	     * Affected chips are rv280, all r3xx, and all r4xx, but not IGP
	     */
	    if (info->ChipFamily == CHIP_FAMILY_RV280 ||
		info->ChipFamily == CHIP_FAMILY_R300 ||
		info->ChipFamily == CHIP_FAMILY_R350 ||
		info->ChipFamily == CHIP_FAMILY_RV350 ||
		info->ChipFamily == CHIP_FAMILY_RV380 ||
		info->ChipFamily == CHIP_FAMILY_R420 ||
		info->ChipFamily == CHIP_FAMILY_RV410)
		    aper0_base &= ~(mem_size - 1);

	    info->mc_fb_location = (aper0_base >> 16) |
		    ((aper0_base + mem_size - 1) & 0xffff0000U);
	}
    }
    info->fbLocation = (info->mc_fb_location & 0xffff) << 16;
   
    /* Just disable the damn AGP apertures for now, it may be
     * re-enabled later by the DRM
     */
    info->mc_agp_location = 0xffffffc0;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "RADEONInitMemoryMap() : \n");
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "  mem_size         : 0x%08lx\n", mem_size);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "  MC_FB_LOCATION   : 0x%08lx\n", info->mc_fb_location);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "  MC_AGP_LOCATION  : 0x%08lx\n", info->mc_agp_location);
}

static void RADEONGetVRamType(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 tmp;
 
    if (info->IsIGP || (info->ChipFamily >= CHIP_FAMILY_R300) ||
	(INREG(RADEON_MEM_SDRAM_MODE_REG) & (1<<30))) 
	info->IsDDR = TRUE;
    else
	info->IsDDR = FALSE;

    tmp = INREG(RADEON_MEM_CNTL);
    if (IS_R300_VARIANT) {
	tmp &=  R300_MEM_NUM_CHANNELS_MASK;
	switch (tmp) {
	case 0: info->RamWidth = 64; break;
	case 1: info->RamWidth = 128; break;
	case 2: info->RamWidth = 256; break;
	default: info->RamWidth = 128; break;
	}
    } else if ((info->ChipFamily == CHIP_FAMILY_RV100) ||
	       (info->ChipFamily == CHIP_FAMILY_RS100) ||
	       (info->ChipFamily == CHIP_FAMILY_RS200)){
	if (tmp & RV100_HALF_MODE) info->RamWidth = 32;
	else info->RamWidth = 64;
       if (!pRADEONEnt->HasCRTC2) {
           info->RamWidth /= 4;
           info->IsDDR = TRUE;
       }
    } else {
	if (tmp & RADEON_MEM_NUM_CHANNELS_MASK) info->RamWidth = 128;
	else info->RamWidth = 64;
    }

    /* This may not be correct, as some cards can have half of channel disabled 
     * ToDo: identify these cases
     */
}

/*
 * Depending on card genertation, chipset bugs, etc... the amount of vram
 * accessible to the CPU can vary. This function is our best shot at figuring
 * it out. Returns a value in KB.
 */
static CARD32 RADEONGetAccessibleVRAM(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32	   aper_size = INREG(RADEON_CONFIG_APER_SIZE) / 1024;

#ifdef XF86DRI
    /* If we use the DRI, we need to check if it's a version that has the
     * bug of always cropping MC_FB_LOCATION to one aperture, in which case
     * we need to limit the amount of accessible video memory
     */
    if (info->directRenderingEnabled &&
	info->pKernelDRMVersion->version_minor < 23) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "[dri] limiting video memory to one aperture of %ldK\n",
		   aper_size);
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "[dri] detected radeon kernel module version 1.%d but"
		   " 1.23 or newer is required for full memory mapping.\n",
		   info->pKernelDRMVersion->version_minor);
	info->newMemoryMap = FALSE;
	return aper_size;
    }
    info->newMemoryMap = TRUE;
#endif /* XF86DRI */

    /* Set HDP_APER_CNTL only on cards that are known not to be broken,
     * that is has the 2nd generation multifunction PCI interface
     */
    if (info->ChipFamily == CHIP_FAMILY_RV280 ||
	info->ChipFamily == CHIP_FAMILY_RV350 ||
	info->ChipFamily == CHIP_FAMILY_RV380 ||
	info->ChipFamily == CHIP_FAMILY_R420 ||
	info->ChipFamily == CHIP_FAMILY_RV410) {
	    OUTREGP (RADEON_HOST_PATH_CNTL, RADEON_HDP_APER_CNTL,
		     ~RADEON_HDP_APER_CNTL);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Generation 2 PCI interface, using max accessible memory\n");
	    return aper_size * 2;
    }

    /* Older cards have all sorts of funny issues to deal with. First
     * check if it's a multifunction card by reading the PCI config
     * header type... Limit those to one aperture size
     */
    if (pciReadByte(info->PciTag, 0xe) & 0x80) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Generation 1 PCI interface in multifunction mode"
		   ", accessible memory limited to one aperture\n");
	return aper_size;
    }

    /* Single function older card. We read HDP_APER_CNTL to see how the BIOS
     * have set it up. We don't write this as it's broken on some ASICs but
     * we expect the BIOS to have done the right thing (might be too optimistic...)
     */
    if (INREG(RADEON_HOST_PATH_CNTL) & RADEON_HDP_APER_CNTL)
        return aper_size * 2;
    
    return aper_size;
}

static Bool RADEONPreInitVRAM(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    EntityInfoPtr  pEnt   = info->pEnt;
    GDevPtr        dev    = pEnt->device;
    unsigned char *RADEONMMIO = info->MMIO;
    MessageType    from = X_PROBED;
    CARD32         accessible, bar_size;

    if (info->FBDev)
	pScrn->videoRam      = fbdevHWGetVidmem(pScrn) / 1024;
    else if ((info->IsIGP)) {
        CARD32 tom = INREG(RADEON_NB_TOM);

	pScrn->videoRam = (((tom >> 16) -
			    (tom & 0xffff) + 1) << 6);

	OUTREG(RADEON_CONFIG_MEMSIZE, pScrn->videoRam * 1024);
    } else {
	/* Read VRAM size from card */
        pScrn->videoRam      = INREG(RADEON_CONFIG_MEMSIZE) / 1024;

	/* Some production boards of m6 will return 0 if it's 8 MB */
	if (pScrn->videoRam == 0) {
	    pScrn->videoRam = 8192;
	    OUTREG(RADEON_CONFIG_MEMSIZE, 0x800000);
	}
    }

    /* Get accessible memory */
    accessible = RADEONGetAccessibleVRAM(pScrn);

    /* Crop it to the size of the PCI BAR */
    bar_size = (1ul << info->PciInfo->size[0]) / 1024;
    if (bar_size == 0)
	bar_size = 0x20000;
    if (accessible > bar_size)
	accessible = bar_size;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Detected total video RAM=%dK, accessible=%ldK (PCI BAR=%ldK)\n",
	       pScrn->videoRam, accessible, bar_size);
    if (pScrn->videoRam > accessible)
	pScrn->videoRam = accessible;

    info->MemCntl            = INREG(RADEON_SDRAM_MODE_REG);
    info->BusCntl            = INREG(RADEON_BUS_CNTL);

    RADEONGetVRamType(pScrn);

    if (dev->videoRam) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Video RAM override, using %d kB instead of %d kB\n",
		   dev->videoRam,
		   pScrn->videoRam);
	from             = X_CONFIG;
	pScrn->videoRam  = dev->videoRam;
    }

    xf86DrvMsg(pScrn->scrnIndex, from,
	       "Mapped VideoRAM: %d kByte (%d bit %s SDRAM)\n", pScrn->videoRam, info->RamWidth, info->IsDDR?"DDR":"SDR");

    pScrn->videoRam  &= ~1023;
    info->FbMapSize  = pScrn->videoRam * 1024;

    /* if the card is PCI Express reserve the last 32k for the gart table */
#ifdef XF86DRI
    if (info->cardType == CARD_PCIE && info->directRenderingEnabled)
      /* work out the size of pcie aperture */
        info->FbSecureSize = RADEONDRIGetPciAperTableSize(pScrn);
    else
#endif
	info->FbSecureSize = 0;

    return TRUE;
}


/* This is called by RADEONPreInit to handle config file overrides for
 * things like chipset and memory regions.  Also determine memory size
 * and type.  If memory type ever needs an override, put it in this
 * routine.
 */
static Bool RADEONPreInitChipType(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    EntityInfoPtr  pEnt   = info->pEnt;
    GDevPtr        dev    = pEnt->device;
    unsigned char *RADEONMMIO = info->MMIO;
    MessageType    from = X_PROBED;
#ifdef XF86DRI
    const char *s;
#endif

    /* Chipset */
    from = X_PROBED;
    if (dev->chipset && *dev->chipset) {
	info->Chipset  = xf86StringToToken(RADEONChipsets, dev->chipset);
	from           = X_CONFIG;
    } else if (dev->chipID >= 0) {
	info->Chipset  = dev->chipID;
	from           = X_CONFIG;
    } else {
	info->Chipset = info->PciInfo->chipType;
    }

    pScrn->chipset = (char *)xf86TokenToString(RADEONChipsets, info->Chipset);
    if (!pScrn->chipset) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "ChipID 0x%04x is not recognized\n", info->Chipset);
	return FALSE;
    }
    if (info->Chipset < 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Chipset \"%s\" is not recognized\n", pScrn->chipset);
	return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, from,
	       "Chipset: \"%s\" (ChipID = 0x%04x)\n",
	       pScrn->chipset,
	       info->Chipset);

    pRADEONEnt->HasCRTC2 = TRUE;
    info->IsMobility = FALSE;
    info->IsIGP = FALSE;
    info->IsDellServer = FALSE;
    info->HasSingleDAC = FALSE;
    info->InternalTVOut = TRUE;
    switch (info->Chipset) {
    case PCI_CHIP_RADEON_LY:
    case PCI_CHIP_RADEON_LZ:
	info->IsMobility = TRUE;
	info->ChipFamily = CHIP_FAMILY_RV100;
	break;

    case PCI_CHIP_RN50_515E:  /* RN50 is based on the RV100 but 3D isn't guaranteed to work.  YMMV. */
    case PCI_CHIP_RN50_5969:
        pRADEONEnt->HasCRTC2 = FALSE;
    case PCI_CHIP_RV100_QY:
    case PCI_CHIP_RV100_QZ:
	info->ChipFamily = CHIP_FAMILY_RV100;

	/* DELL triple-head configuration. */
	if ((info->PciInfo->subsysVendor == PCI_VENDOR_DELL) &&
	    ((info->PciInfo->subsysCard  == 0x016c) ||
	    (info->PciInfo->subsysCard   == 0x016d) ||
	    (info->PciInfo->subsysCard   == 0x016e) ||
	    (info->PciInfo->subsysCard   == 0x016f) ||
	    (info->PciInfo->subsysCard   == 0x0170) ||
	    (info->PciInfo->subsysCard   == 0x017d) ||
	    (info->PciInfo->subsysCard   == 0x017e) ||
	    (info->PciInfo->subsysCard   == 0x0183) ||
	    (info->PciInfo->subsysCard   == 0x018a) ||
	    (info->PciInfo->subsysCard   == 0x019a))) {
	    info->IsDellServer = TRUE;
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "DELL server detected, force to special setup\n");
	}

	break;

    case PCI_CHIP_RS100_4336:
	info->IsMobility = TRUE;
    case PCI_CHIP_RS100_4136:
	info->ChipFamily = CHIP_FAMILY_RS100;
	info->IsIGP = TRUE;
	break;

    case PCI_CHIP_RS200_4337:
	info->IsMobility = TRUE;
    case PCI_CHIP_RS200_4137:
	info->ChipFamily = CHIP_FAMILY_RS200;
	info->IsIGP = TRUE;
	break;

    case PCI_CHIP_RS250_4437:
	info->IsMobility = TRUE;
    case PCI_CHIP_RS250_4237:
	info->ChipFamily = CHIP_FAMILY_RS200;
	info->IsIGP = TRUE;
	break;

    case PCI_CHIP_R200_BB:
    case PCI_CHIP_R200_BC:
    case PCI_CHIP_R200_QH:
    case PCI_CHIP_R200_QL:
    case PCI_CHIP_R200_QM:
	info->ChipFamily = CHIP_FAMILY_R200;
	info->InternalTVOut = FALSE;
	break;

    case PCI_CHIP_RADEON_LW:
    case PCI_CHIP_RADEON_LX:
	info->IsMobility = TRUE;
    case PCI_CHIP_RV200_QW: /* RV200 desktop */
    case PCI_CHIP_RV200_QX:
	info->ChipFamily = CHIP_FAMILY_RV200;
	break;

    case PCI_CHIP_RV250_Ld:
    case PCI_CHIP_RV250_Lf:
    case PCI_CHIP_RV250_Lg:
	info->IsMobility = TRUE;
    case PCI_CHIP_RV250_If:
    case PCI_CHIP_RV250_Ig:
	info->ChipFamily = CHIP_FAMILY_RV250;
	break;

    case PCI_CHIP_RS300_5835:
    case PCI_CHIP_RS350_7835:
	info->IsMobility = TRUE;
    case PCI_CHIP_RS300_5834:
    case PCI_CHIP_RS350_7834:
	info->ChipFamily = CHIP_FAMILY_RS300;
	info->IsIGP = TRUE;
	info->HasSingleDAC = TRUE;
	break;

    case PCI_CHIP_RV280_5C61:
    case PCI_CHIP_RV280_5C63:
	info->IsMobility = TRUE;
    case PCI_CHIP_RV280_5960:
    case PCI_CHIP_RV280_5961:
    case PCI_CHIP_RV280_5962:
    case PCI_CHIP_RV280_5964:
    case PCI_CHIP_RV280_5965:
	info->ChipFamily = CHIP_FAMILY_RV280;
	break;

    case PCI_CHIP_R300_AD:
    case PCI_CHIP_R300_AE:
    case PCI_CHIP_R300_AF:
    case PCI_CHIP_R300_AG:
    case PCI_CHIP_R300_ND:
    case PCI_CHIP_R300_NE:
    case PCI_CHIP_R300_NF:
    case PCI_CHIP_R300_NG:
	info->ChipFamily = CHIP_FAMILY_R300;
        break;

    case PCI_CHIP_RV350_NP:
    case PCI_CHIP_RV350_NQ:
    case PCI_CHIP_RV350_NR:
    case PCI_CHIP_RV350_NS:
    case PCI_CHIP_RV350_NT:
    case PCI_CHIP_RV350_NV:
	info->IsMobility = TRUE;
    case PCI_CHIP_RV350_AP:
    case PCI_CHIP_RV350_AQ:
    case PCI_CHIP_RV360_AR:
    case PCI_CHIP_RV350_AS:
    case PCI_CHIP_RV350_AT:
    case PCI_CHIP_RV350_AV:
    case PCI_CHIP_RV350_4155:
	info->ChipFamily = CHIP_FAMILY_RV350;
        break;

    case PCI_CHIP_R350_AH:
    case PCI_CHIP_R350_AI:
    case PCI_CHIP_R350_AJ:
    case PCI_CHIP_R350_AK:
    case PCI_CHIP_R350_NH:
    case PCI_CHIP_R350_NI:
    case PCI_CHIP_R350_NK:
    case PCI_CHIP_R360_NJ:
	info->ChipFamily = CHIP_FAMILY_R350;
        break;

    case PCI_CHIP_RV380_3150:
    case PCI_CHIP_RV380_3152:
    case PCI_CHIP_RV380_3154:
        info->IsMobility = TRUE;
    case PCI_CHIP_RV380_3E50:
    case PCI_CHIP_RV380_3E54:
        info->ChipFamily = CHIP_FAMILY_RV380;
        break;

    case PCI_CHIP_RV370_5460:
    case PCI_CHIP_RV370_5462:
    case PCI_CHIP_RV370_5464:
        info->IsMobility = TRUE;
    case PCI_CHIP_RV370_5B60:
    case PCI_CHIP_RV370_5B62:
    case PCI_CHIP_RV370_5B63:
    case PCI_CHIP_RV370_5B64:
    case PCI_CHIP_RV370_5B65:
        info->ChipFamily = CHIP_FAMILY_RV380;
        break;

    case PCI_CHIP_RS400_5A42:
    case PCI_CHIP_RC410_5A62:
    case PCI_CHIP_RS480_5955:
    case PCI_CHIP_RS482_5975:
        info->IsMobility = TRUE;
    case PCI_CHIP_RS400_5A41:
    case PCI_CHIP_RC410_5A61:
    case PCI_CHIP_RS480_5954:
    case PCI_CHIP_RS482_5974:
	info->ChipFamily = CHIP_FAMILY_RS400;
	info->IsIGP = TRUE;
	info->HasSingleDAC = TRUE;
        break;

    case PCI_CHIP_RV410_564A:
    case PCI_CHIP_RV410_564B:
    case PCI_CHIP_RV410_564F:
    case PCI_CHIP_RV410_5652:
    case PCI_CHIP_RV410_5653:
        info->IsMobility = TRUE;
    case PCI_CHIP_RV410_5E48:
    case PCI_CHIP_RV410_5E4B:
    case PCI_CHIP_RV410_5E4A:
    case PCI_CHIP_RV410_5E4D:
    case PCI_CHIP_RV410_5E4C:
    case PCI_CHIP_RV410_5E4F:
        info->ChipFamily = CHIP_FAMILY_RV410;
        break;

    case PCI_CHIP_R420_JN:
        info->IsMobility = TRUE;
    case PCI_CHIP_R420_JH:
    case PCI_CHIP_R420_JI:
    case PCI_CHIP_R420_JJ:
    case PCI_CHIP_R420_JK:
    case PCI_CHIP_R420_JL:
    case PCI_CHIP_R420_JM:
    case PCI_CHIP_R420_JP:
    case PCI_CHIP_R420_4A4F:
        info->ChipFamily = CHIP_FAMILY_R420;
        break;

    case PCI_CHIP_R423_UH:
    case PCI_CHIP_R423_UI:
    case PCI_CHIP_R423_UJ:
    case PCI_CHIP_R423_UK:
    case PCI_CHIP_R423_UQ:
    case PCI_CHIP_R423_UR:
    case PCI_CHIP_R423_UT:
    case PCI_CHIP_R423_5D57:
    case PCI_CHIP_R423_5550:
        info->ChipFamily = CHIP_FAMILY_R420;
        break;

    case PCI_CHIP_R430_5D49:
    case PCI_CHIP_R430_5D4A:
    case PCI_CHIP_R430_5D48:
        info->IsMobility = TRUE;
    case PCI_CHIP_R430_554F:
    case PCI_CHIP_R430_554D:
    case PCI_CHIP_R430_554E:
    case PCI_CHIP_R430_554C:
        info->ChipFamily = CHIP_FAMILY_R420; /*CHIP_FAMILY_R430*/
        break;

    case PCI_CHIP_R480_5D4C:
    case PCI_CHIP_R480_5D50:
    case PCI_CHIP_R480_5D4E:
    case PCI_CHIP_R480_5D4F:
    case PCI_CHIP_R480_5D52:
    case PCI_CHIP_R480_5D4D:
    case PCI_CHIP_R481_4B4B:
    case PCI_CHIP_R481_4B4A:
    case PCI_CHIP_R481_4B49:
    case PCI_CHIP_R481_4B4C:
        info->ChipFamily = CHIP_FAMILY_R420; /*CHIP_FAMILY_R480*/
        break;

    default:
	/* Original Radeon/7200 */
	info->ChipFamily = CHIP_FAMILY_RADEON;
	pRADEONEnt->HasCRTC2 = FALSE;
	info->InternalTVOut = FALSE;
    }


    from               = X_PROBED;
    info->LinearAddr   = info->PciInfo->memBase[0] & 0xfe000000;
    pScrn->memPhysBase = info->LinearAddr;
    if (dev->MemBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Linear address override, using 0x%08lx instead of 0x%08lx\n",
		   dev->MemBase,
		   info->LinearAddr);
	info->LinearAddr = dev->MemBase;
	from             = X_CONFIG;
    } else if (!info->LinearAddr) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "No valid linear framebuffer address\n");
	return FALSE;
    }
    xf86DrvMsg(pScrn->scrnIndex, from,
	       "Linear framebuffer at 0x%08lx\n", info->LinearAddr);

				/* BIOS */
    from              = X_PROBED;
    info->BIOSAddr    = info->PciInfo->biosBase & 0xfffe0000;
    if (dev->BiosBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "BIOS address override, using 0x%08lx instead of 0x%08lx\n",
		   dev->BiosBase,
		   info->BIOSAddr);
	info->BIOSAddr = dev->BiosBase;
	from           = X_CONFIG;
    }
    if (info->BIOSAddr) {
	xf86DrvMsg(pScrn->scrnIndex, from,
		   "BIOS at 0x%08lx\n", info->BIOSAddr);
    }

				/* Read registers used to determine options */
    /* Check chip errata */
    info->ChipErrata = 0;

    if (info->ChipFamily == CHIP_FAMILY_R300 &&
	(INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK)
	== RADEON_CFG_ATI_REV_A11)
	    info->ChipErrata |= CHIP_ERRATA_R300_CG;

    if (info->ChipFamily == CHIP_FAMILY_RV200 ||
	info->ChipFamily == CHIP_FAMILY_RS200)
	    info->ChipErrata |= CHIP_ERRATA_PLL_DUMMYREADS;

    if (info->ChipFamily == CHIP_FAMILY_RV100 ||
	info->ChipFamily == CHIP_FAMILY_RS100 ||
	info->ChipFamily == CHIP_FAMILY_RS200)
	    info->ChipErrata |= CHIP_ERRATA_PLL_DELAY;

#ifdef XF86DRI
				/* AGP/PCI */
    /* Proper autodetection of an AGP capable device requires examining
     * PCI config registers to determine if the device implements extended
     * PCI capabilities, and then walking the capability list as indicated
     * in the PCI 2.2 and AGP 2.0 specifications, to determine if AGP
     * capability is present.  The procedure is outlined as follows:
     *
     * 1) Test bit 4 (CAP_LIST) of the PCI status register of the device
     *    to determine wether or not this device implements any extended
     *    capabilities.  If this bit is zero, then the device is a PCI 2.1
     *    or earlier device and is not AGP capable, and we can conclude it
     *    to be a PCI device.
     *
     * 2) If bit 4 of the status register is set, then the device implements
     *    extended capabilities.  There is an 8 bit wide capabilities pointer
     *    register located at offset 0x34 in PCI config space which points to
     *    the first capability in a linked list of extended capabilities that
     *    this device implements.  The lower two bits of this register are
     *    reserved and MBZ so must be masked out.
     *
     * 3) The extended capabilities list is formed by one or more extended
     *    capabilities structures which are aligned on DWORD boundaries.
     *    The first byte of the structure is the capability ID (CAP_ID)
     *    indicating what extended capability this structure refers to.  The
     *    second byte of the structure is an offset from the beginning of
     *    PCI config space pointing to the next capability in the linked
     *    list (NEXT_PTR) or NULL (0x00) at the end of the list.  The lower
     *    two bits of this pointer are reserved and MBZ.  By examining the
     *    CAP_ID of each capability and walking through the list, we will
     *    either find the AGP_CAP_ID (0x02) indicating this device is an
     *    AGP device, or we'll reach the end of the list, indicating it is
     *    a PCI device.
     *
     * Mike A. Harris <mharris@redhat.com>
     *
     * References:
     *	- PCI Local Bus Specification Revision 2.2, Chapter 6
     *	- AGP Interface Specification Revision 2.0, Section 6.1.5
     */

    info->cardType = CARD_PCI;

    if (pciReadLong(info->PciTag, PCI_CMD_STAT_REG) & RADEON_CAP_LIST) {
	CARD32 cap_ptr, cap_id;
	
	cap_ptr = pciReadLong(info->PciTag,
			      RADEON_CAPABILITIES_PTR_PCI_CONFIG)
	    & RADEON_CAP_PTR_MASK;

	while(cap_ptr != RADEON_CAP_ID_NULL) {
	    cap_id = pciReadLong(info->PciTag, cap_ptr);
	    if ((cap_id & 0xff)== RADEON_CAP_ID_AGP) {
		info->cardType = CARD_AGP;
		break;
	    }
	    if ((cap_id & 0xff)== RADEON_CAP_ID_EXP) {
		info->cardType = CARD_PCIE;
		break;
	    }
	    cap_ptr = (cap_id >> 8) & RADEON_CAP_PTR_MASK;
	}
    }


    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%s card detected\n",
	       (info->cardType==CARD_PCI) ? "PCI" :
		(info->cardType==CARD_PCIE) ? "PCIE" : "AGP");

    /* treat PCIE IGP cards as PCI */
    if (info->cardType == CARD_PCIE && info->IsIGP)
		info->cardType = CARD_PCI;

    if ((s = xf86GetOptValString(info->Options, OPTION_BUS_TYPE))) {
	if (strcmp(s, "AGP") == 0) {
	    info->cardType = CARD_AGP;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forced into AGP mode\n");
	} else if (strcmp(s, "PCI") == 0) {
	    info->cardType = CARD_PCI;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forced into PCI mode\n");
	} else if (strcmp(s, "PCIE") == 0) {
	    info->cardType = CARD_PCIE;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forced into PCI Express mode\n");
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "Invalid BusType option, using detected type\n");
	}
    }
#endif
    xf86GetOptValBool(info->Options, OPTION_SHOWCACHE, &info->showCache);
    if (info->showCache)
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "Option ShowCache enabled\n");

#ifdef RENDER
    info->RenderAccel = xf86ReturnOptValBool(info->Options, OPTION_RENDER_ACCEL,
					     info->Chipset != PCI_CHIP_RN50_515E &&
					     info->Chipset != PCI_CHIP_RN50_5969);
#endif

    return TRUE;
}


static void RADEONPreInitDDC(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
 /* vbeInfoPtr     pVbe; */

    info->ddc1     = FALSE;
    info->ddc_bios = FALSE;
    if (!xf86LoadSubModule(pScrn, "ddc")) {
	info->ddc2 = FALSE;
    } else {
	xf86LoaderReqSymLists(ddcSymbols, NULL);
	info->ddc2 = TRUE;
    }

    /* DDC can use I2C bus */
    /* Load I2C if we have the code to use it */
    if (info->ddc2) {
	if (xf86LoadSubModule(pScrn, "i2c")) {
	    xf86LoaderReqSymLists(i2cSymbols,NULL);
	}
    }
}

/* This is called by RADEONPreInit to initialize gamma correction */
static Bool RADEONPreInitGamma(ScrnInfoPtr pScrn)
{
    Gamma  zeros = { 0.0, 0.0, 0.0 };

    if (!xf86SetGamma(pScrn, zeros)) return FALSE;
    return TRUE;
}

/* This is called by RADEONPreInit to initialize the hardware cursor */
static Bool RADEONPreInitCursor(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (!xf86ReturnOptValBool(info->Options, OPTION_SW_CURSOR, FALSE)) {
	if (!xf86LoadSubModule(pScrn, "ramdac")) return FALSE;
	xf86LoaderReqSymLists(ramdacSymbols, NULL);
    }
    return TRUE;
}

/* This is called by RADEONPreInit to initialize hardware acceleration */
static Bool RADEONPreInitAccel(ScrnInfoPtr pScrn)
{
#ifdef XFree86LOADER
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    MessageType from;
#if defined(USE_EXA) && defined(USE_XAA)
    char *optstr;
#endif

    info->useEXA = FALSE;

    if (!xf86ReturnOptValBool(info->Options, OPTION_NOACCEL, FALSE)) {
	int errmaj = 0, errmin = 0;

	from = X_DEFAULT;
#if defined(USE_EXA)
#if defined(USE_XAA)
	optstr = (char *)xf86GetOptValString(info->Options, OPTION_ACCELMETHOD);
	if (optstr != NULL) {
	    if (xf86NameCmp(optstr, "EXA") == 0) {
		from = X_CONFIG;
		info->useEXA = TRUE;
	    } else if (xf86NameCmp(optstr, "XAA") == 0) {
		from = X_CONFIG;
	    }
	}
#else /* USE_XAA */
	info->useEXA = TRUE;
#endif /* !USE_XAA */
#endif /* USE_EXA */
	xf86DrvMsg(pScrn->scrnIndex, from,
	    "Using %s acceleration architecture\n",
	    info->useEXA ? "EXA" : "XAA");

#ifdef USE_EXA
	if (info->useEXA) {
	    info->exaReq.majorversion = 2;
	    info->exaReq.minorversion = 0;

	    if (!LoadSubModule(pScrn->module, "exa", NULL, NULL, NULL,
			       &info->exaReq, &errmaj, &errmin)) {
		LoaderErrorMsg(NULL, "exa", errmaj, errmin);
		return FALSE;
	    }
	    xf86LoaderReqSymLists(exaSymbols, NULL);
	}
#endif /* USE_EXA */
#ifdef USE_XAA
	if (!info->useEXA) {
	    info->xaaReq.majorversion = 1;
	    info->xaaReq.minorversion = 2;

	    if (!LoadSubModule(pScrn->module, "xaa", NULL, NULL, NULL,
			   &info->xaaReq, &errmaj, &errmin)) {
		info->xaaReq.minorversion = 1;

		if (!LoadSubModule(pScrn->module, "xaa", NULL, NULL, NULL,
			       &info->xaaReq, &errmaj, &errmin)) {
		    info->xaaReq.minorversion = 0;

		    if (!LoadSubModule(pScrn->module, "xaa", NULL, NULL, NULL,
			       &info->xaaReq, &errmaj, &errmin)) {
			LoaderErrorMsg(NULL, "xaa", errmaj, errmin);
			return FALSE;
		    }
		}
	    }
	    xf86LoaderReqSymLists(xaaSymbols, NULL);
	}
#endif /* USE_XAA */
    }
#endif /* XFree86Loader */

    return TRUE;
}

static Bool RADEONPreInitInt10(ScrnInfoPtr pScrn, xf86Int10InfoPtr *ppInt10)
{
#if !defined(__powerpc__)
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 fp2_gen_ctl_save   = 0;

    if (xf86LoadSubModule(pScrn, "int10")) {
	xf86LoaderReqSymLists(int10Symbols, NULL);

	/* The VGA BIOS on the RV100/QY cannot be read when the digital output
	 * is enabled.  Clear and restore FP2_ON around int10 to avoid this.
	 */
	if (info->PciInfo->chipType == PCI_CHIP_RV100_QY) {
	    fp2_gen_ctl_save = INREG(RADEON_FP2_GEN_CNTL);
	    if (fp2_gen_ctl_save & RADEON_FP2_ON) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "disabling digital out\n");
		OUTREG(RADEON_FP2_GEN_CNTL, fp2_gen_ctl_save & ~RADEON_FP2_ON);
	    }
	}
	
	xf86DrvMsg(pScrn->scrnIndex,X_INFO,"initializing int10\n");
	*ppInt10 = xf86InitInt10(info->pEnt->index);

	if (fp2_gen_ctl_save & RADEON_FP2_ON) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "re-enabling digital out\n");
	    OUTREG(RADEON_FP2_GEN_CNTL, fp2_gen_ctl_save);	
	}
    }
#endif
    return TRUE;
}

#ifdef XF86DRI
static Bool RADEONPreInitDRI(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    MessageType    from;
    char          *reason;

    info->directRenderingEnabled = FALSE;
    info->directRenderingInited = FALSE;
    info->CPInUse = FALSE;
    info->CPStarted = FALSE;
    info->pLibDRMVersion = NULL;
    info->pKernelDRMVersion = NULL;

    if (info->Chipset == PCI_CHIP_RN50_515E ||
	info->Chipset == PCI_CHIP_RN50_5969) {
    	if (xf86ReturnOptValBool(info->Options, OPTION_DRI, FALSE)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		"Direct rendering for RN50 forced on -- "
		"This is NOT officially supported at the hardware level "
		"and may cause instability or lockups\n");
    	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"Direct rendering not officially supported on RN50\n");
	    return FALSE;
	}
    }


    if (!xf86ReturnOptValBool(info->Options, OPTION_DRI, TRUE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"Direct rendering forced off\n");
	return FALSE;
    }

    if (xf86ReturnOptValBool(info->Options, OPTION_NOACCEL, FALSE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "[dri] Acceleration disabled, not initializing the DRI\n");
	return FALSE;
    }

    if (!RADEONDRIGetVersion(pScrn))
	return FALSE;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "[dri] Found DRI library version %d.%d.%d and kernel"
	       " module version %d.%d.%d\n",
	       info->pLibDRMVersion->version_major,
	       info->pLibDRMVersion->version_minor,
	       info->pLibDRMVersion->version_patchlevel,
	       info->pKernelDRMVersion->version_major,
	       info->pKernelDRMVersion->version_minor,
	       info->pKernelDRMVersion->version_patchlevel);

    if (info->Chipset == PCI_CHIP_RS400_5A41 ||
	info->Chipset == PCI_CHIP_RS400_5A42 ||
	info->Chipset == PCI_CHIP_RC410_5A61 ||
	info->Chipset == PCI_CHIP_RC410_5A62 ||
	info->Chipset == PCI_CHIP_RS480_5954 ||
	info->Chipset == PCI_CHIP_RS480_5955 ||
	info->Chipset == PCI_CHIP_RS482_5974 ||
	info->Chipset == PCI_CHIP_RS482_5975) {

	if (info->pKernelDRMVersion->version_minor < 27) {
 	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"Direct rendering broken on XPRESS 200 and 200M with DRI less than 1.27\n");
	     return FALSE;
	}
 	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	"Direct rendering experimental on RS400/Xpress 200 enabled\n");
    }

    if (xf86ReturnOptValBool(info->Options, OPTION_CP_PIO, FALSE)) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Forcing CP into PIO mode\n");
	info->CPMode = RADEON_DEFAULT_CP_PIO_MODE;
    } else {
	info->CPMode = RADEON_DEFAULT_CP_BM_MODE;
    }

    info->gartSize      = RADEON_DEFAULT_GART_SIZE;
    info->ringSize      = RADEON_DEFAULT_RING_SIZE;
    info->bufSize       = RADEON_DEFAULT_BUFFER_SIZE;
    info->gartTexSize   = RADEON_DEFAULT_GART_TEX_SIZE;
    info->pciAperSize   = RADEON_DEFAULT_PCI_APER_SIZE;
    info->CPusecTimeout = RADEON_DEFAULT_CP_TIMEOUT;

    if ((xf86GetOptValInteger(info->Options,
			     OPTION_GART_SIZE, (int *)&(info->gartSize))) ||
			     (xf86GetOptValInteger(info->Options,
			     OPTION_GART_SIZE_OLD, (int *)&(info->gartSize)))) {
	switch (info->gartSize) {
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
	case 128:
	case 256:
	    break;

	default:
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Illegal GART size: %d MB\n", info->gartSize);
	    return FALSE;
	}
    }

    if (xf86GetOptValInteger(info->Options,
			     OPTION_RING_SIZE, &(info->ringSize))) {
	if (info->ringSize < 1 || info->ringSize >= (int)info->gartSize) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Illegal ring buffer size: %d MB\n",
		       info->ringSize);
	    return FALSE;
	}
    }

    if (xf86GetOptValInteger(info->Options,
			     OPTION_PCIAPER_SIZE, &(info->pciAperSize))) {
      switch(info->pciAperSize) {
      case 32:
      case 64:
      case 128:
      case 256:
	break;
      default:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Illegal pci aper size: %d MB\n",
		       info->pciAperSize);
	return FALSE;
      }
    }


    if (xf86GetOptValInteger(info->Options,
			     OPTION_BUFFER_SIZE, &(info->bufSize))) {
	if (info->bufSize < 1 || info->bufSize >= (int)info->gartSize) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Illegal vertex/indirect buffers size: %d MB\n",
		       info->bufSize);
	    return FALSE;
	}
	if (info->bufSize > 2) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Illegal vertex/indirect buffers size: %d MB\n",
		       info->bufSize);
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Clamping vertex/indirect buffers size to 2 MB\n");
	    info->bufSize = 2;
	}
    }

    if (info->ringSize + info->bufSize + info->gartTexSize >
	(int)info->gartSize) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Buffers are too big for requested GART space\n");
	return FALSE;
    }

    info->gartTexSize = info->gartSize - (info->ringSize + info->bufSize);

    if (xf86GetOptValInteger(info->Options, OPTION_USEC_TIMEOUT,
			     &(info->CPusecTimeout))) {
	/* This option checked by the RADEON DRM kernel module */
    }

    /* Two options to try and squeeze as much texture memory as possible
     * for dedicated 3d rendering boxes
     */
    info->noBackBuffer = xf86ReturnOptValBool(info->Options,
					      OPTION_NO_BACKBUFFER,
					      FALSE);

    info->allowPageFlip = 0;

#ifdef DAMAGE
    if (info->noBackBuffer) {
	from = X_DEFAULT;
	reason = " because back buffer disabled";
    } else {
	from = xf86GetOptValBool(info->Options, OPTION_PAGE_FLIP,
				 &info->allowPageFlip) ? X_CONFIG : X_DEFAULT;
	reason = "";
    }
#else
    from = X_DEFAULT;
    reason = " because Damage layer not available at build time";
#endif

    xf86DrvMsg(pScrn->scrnIndex, from, "Page Flipping %sabled%s\n",
	       info->allowPageFlip ? "en" : "dis", reason);

    info->DMAForXv = TRUE;
    from = xf86GetOptValBool(info->Options, OPTION_XV_DMA, &info->DMAForXv)
	 ? X_CONFIG : X_INFO;
    xf86DrvMsg(pScrn->scrnIndex, from,
	       "Will %stry to use DMA for Xv image transfers\n",
	       info->DMAForXv ? "" : "not ");

    return TRUE;
}
#endif /* XF86DRI */

static void RADEONPreInitColorTiling(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    info->allowColorTiling = xf86ReturnOptValBool(info->Options,
				        OPTION_COLOR_TILING, TRUE);
    if (IS_R300_VARIANT) {
	/* this may be 4096 on r4xx -- need to double check */
	info->MaxSurfaceWidth = 3968; /* one would have thought 4096...*/
	info->MaxLines = 4096;
    } else {
	info->MaxSurfaceWidth = 2048;
	info->MaxLines = 2048;
    }

    if (!info->allowColorTiling)
	return;

#ifdef XF86DRI
    if (info->directRenderingEnabled &&
	info->pKernelDRMVersion->version_minor < 14) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "[dri] color tiling disabled because of version "
		   "mismatch.\n"
		   "[dri] radeon.o kernel module version is %d.%d.%d but "
		   "1.14.0 or later is required for color tiling.\n",
		   info->pKernelDRMVersion->version_major,
		   info->pKernelDRMVersion->version_minor,
		   info->pKernelDRMVersion->version_patchlevel);
	   info->allowColorTiling = FALSE;
	   return;	   
    }
#endif /* XF86DRI */

    if ((info->allowColorTiling) && (info->FBDev)) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Color tiling not supported with UseFBDev option\n");
	info->allowColorTiling = FALSE;
    }
    else if (info->allowColorTiling) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Color tiling enabled by default\n");
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Color tiling disabled\n");
    }
}


static Bool RADEONPreInitXv(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    CARD16 mm_table;
    CARD16 bios_header;
    CARD16 pll_info_block;
#ifdef XvExtension
    char* microc_path = NULL;
    char* microc_type = NULL;
    MessageType from;

    if (xf86GetOptValInteger(info->Options, OPTION_VIDEO_KEY,
			     &(info->videoKey))) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "video key set to 0x%x\n",
		   info->videoKey);
    } else {
	info->videoKey = 0x1E;
    }

    if(xf86GetOptValInteger(info->Options, OPTION_RAGE_THEATRE_CRYSTAL, &(info->RageTheatreCrystal))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rage Theatre Crystal frequency was specified as %d.%d Mhz\n",
                                info->RageTheatreCrystal/100, info->RageTheatreCrystal % 100);
    } else {
	info->RageTheatreCrystal=-1;
    }

    if(xf86GetOptValInteger(info->Options, OPTION_RAGE_THEATRE_TUNER_PORT, &(info->RageTheatreTunerPort))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rage Theatre tuner port was specified as %d\n",
                                info->RageTheatreTunerPort);
    } else {
	info->RageTheatreTunerPort=-1;
    }

    if(info->RageTheatreTunerPort>5){
         xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Attempt to assign Rage Theatre tuner port to invalid value. Disabling setting\n");
	 info->RageTheatreTunerPort=-1;
	 }

    if(xf86GetOptValInteger(info->Options, OPTION_RAGE_THEATRE_COMPOSITE_PORT, &(info->RageTheatreCompositePort))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rage Theatre composite port was specified as %d\n",
                                info->RageTheatreCompositePort);
    } else {
	info->RageTheatreCompositePort=-1;
    }

    if(info->RageTheatreCompositePort>6){
         xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Attempt to assign Rage Theatre composite port to invalid value. Disabling setting\n");
	 info->RageTheatreCompositePort=-1;
	 }

    if(xf86GetOptValInteger(info->Options, OPTION_RAGE_THEATRE_SVIDEO_PORT, &(info->RageTheatreSVideoPort))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rage Theatre SVideo Port was specified as %d\n",
                                info->RageTheatreSVideoPort);
    } else {
	info->RageTheatreSVideoPort=-1;
    }

    if(info->RageTheatreSVideoPort>6){
         xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Attempt to assign Rage Theatre SVideo port to invalid value. Disabling setting\n");
	 info->RageTheatreSVideoPort=-1;
	 }

    if(xf86GetOptValInteger(info->Options, OPTION_TUNER_TYPE, &(info->tunerType))) {
        xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Tuner type was specified as %d\n",
                                info->tunerType);
    } else {
	info->tunerType=-1;
    }

    if(info->tunerType>31){
         xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Attempt to set tuner type to invalid value. Disabling setting\n");
	 info->tunerType=-1;
	 }

	if((microc_path = xf86GetOptValString(info->Options, OPTION_RAGE_THEATRE_MICROC_PATH)) != NULL)
	{
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rage Theatre Microcode path was specified as %s\n", microc_path);
		info->RageTheatreMicrocPath = microc_path;
    } else {
		info->RageTheatreMicrocPath= NULL;
    }

	if((microc_type = xf86GetOptValString(info->Options, OPTION_RAGE_THEATRE_MICROC_TYPE)) != NULL)
	{
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Rage Theatre Microcode type was specified as %s\n", microc_type);
		info->RageTheatreMicrocType = microc_type;
	} else {
		info->RageTheatreMicrocType= NULL;
	}

    if(xf86GetOptValInteger(info->Options, OPTION_SCALER_WIDTH, &(info->overlay_scaler_buffer_width))) {
	if ((info->overlay_scaler_buffer_width < 1024) ||
	  (info->overlay_scaler_buffer_width > 2048) ||
	  ((info->overlay_scaler_buffer_width % 64) != 0)) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Attempt to set illegal scaler width. Using default\n");
	    from = X_DEFAULT;
	    info->overlay_scaler_buffer_width = 0;
	} else
	    from = X_CONFIG;
    } else {
	from = X_DEFAULT;
	info->overlay_scaler_buffer_width = 0;
    }
    if (!info->overlay_scaler_buffer_width) {
       /* overlay scaler line length differs for different revisions
       this needs to be maintained by hand  */
	switch(info->ChipFamily){
	case CHIP_FAMILY_R200:
	case CHIP_FAMILY_R300:
	case CHIP_FAMILY_RV350:
		info->overlay_scaler_buffer_width = 1920;
		break;
	default:
		info->overlay_scaler_buffer_width = 1536;
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, from, "Assuming overlay scaler buffer width is %d\n",
	info->overlay_scaler_buffer_width);
#endif

    /* Rescue MM_TABLE before VBIOS is freed */
    info->MM_TABLE_valid = FALSE;
    
    if((info->VBIOS==NULL)||(info->VBIOS[0]!=0x55)||(info->VBIOS[1]!=0xaa)){
       xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Cannot access BIOS or it is not valid.\n"
               "\t\tIf your card is TV-in capable you will need to specify options RageTheatreCrystal, RageTheatreTunerPort, \n"
               "\t\tRageTheatreSVideoPort and TunerType in /etc/X11/xorg.conf.\n"
               );
       info->MM_TABLE_valid = FALSE;
       return TRUE;
       }

    bios_header=info->VBIOS[0x48];
    bios_header+=(((int)info->VBIOS[0x49]+0)<<8);           
        
    mm_table=info->VBIOS[bios_header+0x38];
    if(mm_table==0)
    {
        xf86DrvMsg(pScrn->scrnIndex,X_INFO,"No MM_TABLE found - assuming CARD is not TV-in capable.\n");
        info->MM_TABLE_valid = FALSE;
        return TRUE;
    }
    mm_table+=(((int)info->VBIOS[bios_header+0x39]+0)<<8)-2;
    
    if(mm_table>0)
    {
        memcpy(&(info->MM_TABLE), &(info->VBIOS[mm_table]), sizeof(info->MM_TABLE));
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "MM_TABLE: %02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x\n",
            info->MM_TABLE.table_revision,
            info->MM_TABLE.table_size,
            info->MM_TABLE.tuner_type,
            info->MM_TABLE.audio_chip,
            info->MM_TABLE.product_id,
            info->MM_TABLE.tuner_voltage_teletext_fm,
            info->MM_TABLE.i2s_config,
            info->MM_TABLE.video_decoder_type,
            info->MM_TABLE.video_decoder_host_config,
            info->MM_TABLE.input[0],
            info->MM_TABLE.input[1],
            info->MM_TABLE.input[2],
            info->MM_TABLE.input[3],
            info->MM_TABLE.input[4]);
	    
	  /* Is it an MM_TABLE we know about ? */
	  if(info->MM_TABLE.table_size != 0xc){
	       xf86DrvMsg(pScrn->scrnIndex, X_INFO, "This card has MM_TABLE we do not recognize.\n"
			"\t\tIf your card is TV-in capable you will need to specify options RageTheatreCrystal, RageTheatreTunerPort, \n"
			"\t\tRageTheatreSVideoPort and TunerType in /etc/X11/xorg.conf.\n"
			);
		info->MM_TABLE_valid = FALSE;
		return TRUE;
	  	}
        info->MM_TABLE_valid = TRUE;
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "No MM_TABLE found - assuming card is not TV-in capable (mm_table=%d).\n", mm_table);
        info->MM_TABLE_valid = FALSE;
    }

    pll_info_block=info->VBIOS[bios_header+0x30];
    pll_info_block+=(((int)info->VBIOS[bios_header+0x31]+0)<<8);
       
    info->video_decoder_type=info->VBIOS[pll_info_block+0x08];
    info->video_decoder_type+=(((int)info->VBIOS[pll_info_block+0x09]+0)<<8);
    
    return TRUE;
}

static void RADEONPreInitBIOS(ScrnInfoPtr pScrn, xf86Int10InfoPtr  pInt10)
{
    RADEONGetBIOSInfo(pScrn, pInt10);
#if 0
    RADEONGetBIOSInitTableOffsets(pScrn);
    RADEONPostCardFromBIOSTables(pScrn);
#endif
}

static Bool RADEONPreInitControllers(ScrnInfoPtr pScrn)
{
    xf86CrtcConfigPtr   config = XF86_CRTC_CONFIG_PTR(pScrn);
    int i;

    if (!RADEONAllocateControllers(pScrn))
	return FALSE;

    RADEONGetClockInfo(pScrn);

    if (!RADEONSetupConnectors(pScrn)) {
	return FALSE;
    }
      
    RADEONPrintPortMap(pScrn);

    for (i = 0; i < config->num_output; i++) 
    {
      xf86OutputPtr	      output = config->output[i];
      
      output->status = (*output->funcs->detect) (output);
      ErrorF("finished output detect: %d\n", i);
    }
    ErrorF("finished all detect\n");
    return TRUE;
}

static void
RADEONProbeDDC(ScrnInfoPtr pScrn, int indx)
{
    vbeInfoPtr  pVbe;

    if (xf86LoadSubModule(pScrn, "vbe")) {
	pVbe = VBEInit(NULL,indx);
	ConfiguredMonitor = vbeDoEDID(pVbe, NULL);
    }
}

static Bool
RADEONCRTCResize(ScrnInfoPtr scrn, int width, int height)
{
    scrn->virtualX = width;
    scrn->virtualY = height;
    /* RADEONSetPitch(scrn); */
    return TRUE;
}

static const xf86CrtcConfigFuncsRec RADEONCRTCResizeFuncs = {
    RADEONCRTCResize
};

_X_EXPORT Bool RADEONPreInit(ScrnInfoPtr pScrn, int flags)
{
    xf86CrtcConfigPtr   xf86_config;
    RADEONInfoPtr     info;
    xf86Int10InfoPtr  pInt10 = NULL;
    void *int10_save = NULL;
    const char *s;
    MessageType from;
    int crtc_max_X, crtc_max_Y;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONPreInit\n");
    if (pScrn->numEntities != 1) return FALSE;

    if (!RADEONGetRec(pScrn)) return FALSE;

    info               = RADEONPTR(pScrn);
    info->MMIO         = NULL;

    info->pEnt         = xf86GetEntityInfo(pScrn->entityList[pScrn->numEntities - 1]);
    if (info->pEnt->location.type != BUS_PCI) goto fail;

    info->PciInfo = xf86GetPciInfoForEntity(info->pEnt->index);
    info->PciTag  = pciTag(info->PciInfo->bus,
			   info->PciInfo->device,
			   info->PciInfo->func);
    info->MMIOAddr   = info->PciInfo->memBase[2] & 0xffffff00;
    info->MMIOSize  = (1 << info->PciInfo->size[2]);
    if (info->pEnt->device->IOBase) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		   "MMIO address override, using 0x%08lx instead of 0x%08lx\n",
		   info->pEnt->device->IOBase,
		   info->MMIOAddr);
	info->MMIOAddr = info->pEnt->device->IOBase;
    } else if (!info->MMIOAddr) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid MMIO address\n");
	goto fail1;
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "MMIO registers at 0x%08lx: size %ldKB\n", info->MMIOAddr, info->MMIOSize / 1024);

    if(!RADEONMapMMIO(pScrn)) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Memory map the MMIO region failed\n");
	goto fail1;
    }

#if !defined(__alpha__)
    if (xf86GetPciDomain(info->PciTag) ||
	!xf86IsPrimaryPci(info->PciInfo))
	RADEONPreInt10Save(pScrn, &int10_save);
#else
    /* [Alpha] On the primary, the console already ran the BIOS and we're
     *         going to run it again - so make sure to "fix up" the card
     *         so that (1) we can read the BIOS ROM and (2) the BIOS will
     *         get the memory config right.
     */
    RADEONPreInt10Save(pScrn, &int10_save);
#endif

    if (flags & PROBE_DETECT) {
	RADEONProbeDDC(pScrn, info->pEnt->index);
	RADEONPostInt10Check(pScrn, int10_save);
	if(info->MMIO) RADEONUnmapMMIO(pScrn);
	return TRUE;
    }


    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "PCI bus %d card %d func %d\n",
	       info->PciInfo->bus,
	       info->PciInfo->device,
	       info->PciInfo->func);

    if (xf86RegisterResources(info->pEnt->index, 0, ResExclusive))
	goto fail;

    if (xf86SetOperatingState(resVga, info->pEnt->index, ResUnusedOpr))
	goto fail;

    pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_VIEWPORT | RAC_CURSOR;
    pScrn->monitor     = pScrn->confScreen->monitor;

   /* Allocate an xf86CrtcConfig */
    xf86CrtcConfigInit (pScrn, &RADEONCRTCResizeFuncs);
    xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);


    if (!RADEONPreInitVisual(pScrn))
	goto fail;

				/* We can't do this until we have a
				   pScrn->display. */
    xf86CollectOptions(pScrn, NULL);
    if (!(info->Options = xalloc(sizeof(RADEONOptions))))
	goto fail;

    memcpy(info->Options, RADEONOptions, sizeof(RADEONOptions));
    xf86ProcessOptions(pScrn->scrnIndex, pScrn->options, info->Options);

    /* By default, don't do VGA IOs on ppc */
#if defined(__powerpc__) || !defined(WITH_VGAHW)
    info->VGAAccess = FALSE;
#else
    info->VGAAccess = TRUE;
#endif

#ifdef WITH_VGAHW
    xf86GetOptValBool(info->Options, OPTION_VGA_ACCESS, &info->VGAAccess);
    if (info->VGAAccess) {
       if (!xf86LoadSubModule(pScrn, "vgahw"))
           info->VGAAccess = FALSE;
        else {
           xf86LoaderReqSymLists(vgahwSymbols, NULL);
            if (!vgaHWGetHWRec(pScrn))
               info->VGAAccess = FALSE;
       }
       if (!info->VGAAccess)
           xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Loading VGA module failed,"
                      " trying to run without it\n");
    } else
           xf86DrvMsg(pScrn->scrnIndex, X_INFO, "VGAAccess option set to FALSE,"
                      " VGA module load skipped\n");
    if (info->VGAAccess)
        vgaHWGetIOBase(VGAHWPTR(pScrn));
#endif


    if (!RADEONPreInitWeight(pScrn))
	goto fail;

    info->DispPriority = 1; 
    if ((s = xf86GetOptValString(info->Options, OPTION_DISP_PRIORITY))) {
	if (strcmp(s, "AUTO") == 0) {
	    info->DispPriority = 1;
	} else if (strcmp(s, "BIOS") == 0) {
	    info->DispPriority = 0;
	} else if (strcmp(s, "HIGH") == 0) {
	    info->DispPriority = 2;
	} else
	    info->DispPriority = 1; 
    }

    info->constantDPI = -1;
    from = X_DEFAULT;
    if (xf86GetOptValBool(info->Options, OPTION_CONSTANTDPI, &info->constantDPI)) {
       from = X_CONFIG;
    } else {
       if (monitorResolution > 0) {
	  info->constantDPI = TRUE;
	  from = X_CMDLINE;
	  xf86DrvMsg(pScrn->scrnIndex, from,
		"\"-dpi %d\" given in command line, assuming \"ConstantDPI\" set\n",
		monitorResolution);
       } else {
	  info->constantDPI = FALSE;
       }
    }
    xf86DrvMsg(pScrn->scrnIndex, from,
	"X server will %skeep DPI constant for all screen sizes\n",
	info->constantDPI ? "" : "not ");

    if (xf86ReturnOptValBool(info->Options, OPTION_FBDEV, FALSE)) {
	/* check for Linux framebuffer device */

	if (xf86LoadSubModule(pScrn, "fbdevhw")) {
	    xf86LoaderReqSymLists(fbdevHWSymbols, NULL);

	    if (fbdevHWInit(pScrn, info->PciInfo, NULL)) {
		pScrn->ValidMode     = fbdevHWValidModeWeak();
		info->FBDev = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			   "Using framebuffer device\n");
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "fbdevHWInit failed, not using framebuffer device\n");
	    }
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Couldn't load fbdevhw module, not using framebuffer device\n");
	}
    }

    if (!info->FBDev)
	if (!RADEONPreInitInt10(pScrn, &pInt10))
	    goto fail;

    RADEONPostInt10Check(pScrn, int10_save);

    if (!RADEONPreInitChipType(pScrn))
	goto fail;

    RADEONPreInitBIOS(pScrn, pInt10);

#ifdef XF86DRI
    /* PreInit DRI first of all since we need that for getting a proper
     * memory map
     */
    info->directRenderingEnabled = RADEONPreInitDRI(pScrn);
#endif
    if (!RADEONPreInitVRAM(pScrn))
	goto fail;

    RADEONPreInitColorTiling(pScrn);

    /* we really need an FB manager... */
    if (pScrn->display->virtualX) {
	crtc_max_X = pScrn->display->virtualX;
	crtc_max_Y = pScrn->display->virtualY;
	if (info->allowColorTiling) {
	    if (crtc_max_X > info->MaxSurfaceWidth)
		crtc_max_X = info->MaxSurfaceWidth;
	    if (crtc_max_Y > info->MaxLines)
		crtc_max_Y = info->MaxLines;
	} else {
	    if (crtc_max_X > 8192)
		crtc_max_X = 8192;
	    if (crtc_max_Y > 8192)
		crtc_max_Y = 8192;
	}
    } else {
	if (pScrn->videoRam < 16384) {
	    crtc_max_X = 1600;
	    crtc_max_Y = 1200;
	} else if (pScrn->videoRam <= 32768) {
	    crtc_max_X = 2048;
	    crtc_max_Y = 1200;
	} else if (pScrn->videoRam > 32768) {
	    if (IS_R300_VARIANT) {
		crtc_max_X = 2560;
		crtc_max_Y = 2048;
	    } else {
		crtc_max_X = 2048;
		crtc_max_Y = 2048;
	    }
	}
    }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Max desktop size set to %dx%d\n",
	       crtc_max_X, crtc_max_Y);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "For a larger or smaller max desktop size, add a Virtual line to your xorg.conf\n");

    /*xf86CrtcSetSizeRange (pScrn, 320, 200, info->MaxSurfaceWidth, info->MaxLines);*/
    xf86CrtcSetSizeRange (pScrn, 320, 200, crtc_max_X, crtc_max_Y);

    RADEONPreInitDDC(pScrn);

    if (!RADEONPreInitControllers(pScrn))
       goto fail;


    ErrorF("before xf86InitialConfiguration\n");

    if (!xf86InitialConfiguration (pScrn, FALSE))
   {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes.\n");
      goto fail;
   }

    ErrorF("after xf86InitialConfiguration\n");

    RADEONSetPitch(pScrn);

   /* Set display resolution */
   xf86SetDpi(pScrn, 0, 0);

	/* Get ScreenInit function */
    if (!xf86LoadSubModule(pScrn, "fb")) return FALSE;

    xf86LoaderReqSymLists(fbSymbols, NULL);

    if (!RADEONPreInitGamma(pScrn))              goto fail;

    if (!RADEONPreInitCursor(pScrn))             goto fail;

    if (!RADEONPreInitAccel(pScrn))              goto fail;

    if (!RADEONPreInitXv(pScrn))                 goto fail;

    if (!xf86RandR12PreInit (pScrn))
    {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "RandR initialization failure\n");
      goto fail;
    }	
    
    if (pScrn->modes == NULL) {
      xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No modes.\n");
      goto fail;
   }

    /* Free the video bios (if applicable) */
    if (info->VBIOS) {
	xfree(info->VBIOS);
	info->VBIOS = NULL;
    }

				/* Free int10 info */
    if (pInt10)
	xf86FreeInt10(pInt10);

    if(info->MMIO) RADEONUnmapMMIO(pScrn);
    info->MMIO = NULL;

    xf86DrvMsg(pScrn->scrnIndex, X_NOTICE,
	       "For information on using the multimedia capabilities\n\tof this"
	       " adapter, please see http://gatos.sf.net.\n");

    return TRUE;

fail:
				/* Pre-init failed. */
				/* Free the video bios (if applicable) */
    if (info->VBIOS) {
	xfree(info->VBIOS);
	info->VBIOS = NULL;
    }

				/* Free int10 info */
    if (pInt10)
	xf86FreeInt10(pInt10);

#ifdef WITH_VGAHW
    if (info->VGAAccess)
           vgaHWFreeHWRec(pScrn);
#endif

    if(info->MMIO) RADEONUnmapMMIO(pScrn);
    info->MMIO = NULL;

 fail1:
    RADEONFreeRec(pScrn);

    return FALSE;
}

/* Load a palette */
static void RADEONLoadPalette(ScrnInfoPtr pScrn, int numColors,
			      int *indices, LOCO *colors, VisualPtr pVisual)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int            i;
    int            index, j;
    CARD16 lut_r[256], lut_g[256], lut_b[256];
    int c;

#ifdef XF86DRI
    if (info->CPStarted && pScrn->pScreen) DRILock(pScrn->pScreen, 0);
#endif

    if (info->accelOn && pScrn->pScreen)
        RADEON_SYNC(info, pScrn);

    if (info->FBDev) {
	fbdevHWLoadPalette(pScrn, numColors, indices, colors, pVisual);
    } else {

      for (c = 0; c < xf86_config->num_crtc; c++) {
	  xf86CrtcPtr crtc = xf86_config->crtc[c];
	  RADEONCrtcPrivatePtr radeon_crtc = crtc->driver_private;

	  for (i = 0 ; i < 256; i++) {
	      lut_r[i] = radeon_crtc->lut_r[i] << 8;
	      lut_g[i] = radeon_crtc->lut_g[i] << 8;
	      lut_b[i] = radeon_crtc->lut_b[i] << 8;
	  }

	  switch (info->CurrentLayout.depth) {
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

	      /* Make the change through RandR */
#ifdef RANDR_12_INTERFACE
      RRCrtcGammaSet(crtc->randr_crtc, lut_r, lut_g, lut_b);
#else
      crtc->funcs->gamma_set(crtc, lut_r, lut_g, lut_b, 256);
#endif
      }
    }

#ifdef XF86DRI
    if (info->CPStarted && pScrn->pScreen) DRIUnlock(pScrn->pScreen);
#endif
}

static void RADEONBlockHandler(int i, pointer blockData,
			       pointer pTimeout, pointer pReadmask)
{
    ScreenPtr      pScreen = screenInfo.screens[i];
    ScrnInfoPtr    pScrn   = xf86Screens[i];
    RADEONInfoPtr  info    = RADEONPTR(pScrn);

    pScreen->BlockHandler = info->BlockHandler;
    (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);
    pScreen->BlockHandler = RADEONBlockHandler;

    if (info->VideoTimerCallback)
	(*info->VideoTimerCallback)(pScrn, currentTime.milliseconds);

#if defined(RENDER) && defined(USE_XAA)
    if(info->RenderCallback)
	(*info->RenderCallback)(pScrn);
#endif

#ifdef USE_EXA
    info->engineMode = EXA_ENGINEMODE_UNKNOWN;
#endif
}


#ifdef USE_XAA
#ifdef XF86DRI
Bool RADEONSetupMemXAA_DRI(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    int            cpp = info->CurrentLayout.pixel_bytes;
    int            depthCpp = (info->depthBits - 8) / 4;
    int            width_bytes = pScrn->displayWidth * cpp;
    int            bufferSize;
    int            depthSize;
    int            l;
    int            scanlines;
    int            texsizerequest;
    BoxRec         MemBox;
    FBAreaPtr      fbarea;

    info->frontOffset = 0;
    info->frontPitch = pScrn->displayWidth;
    info->backPitch = pScrn->displayWidth;

    /* make sure we use 16 line alignment for tiling (8 might be enough).
     * Might need that for non-XF86DRI too?
     */
    if (info->allowColorTiling) {
	bufferSize = (((pScrn->virtualY + 15) & ~15) * width_bytes
		      + RADEON_BUFFER_ALIGN) & ~RADEON_BUFFER_ALIGN;
    } else {
        bufferSize = (pScrn->virtualY * width_bytes
		      + RADEON_BUFFER_ALIGN) & ~RADEON_BUFFER_ALIGN;
    }

    /* Due to tiling, the Z buffer pitch must be a multiple of 32 pixels,
     * which is always the case if color tiling is used due to color pitch
     * but not necessarily otherwise, and its height a multiple of 16 lines.
     */
    info->depthPitch = (pScrn->displayWidth + 31) & ~31;
    depthSize = ((((pScrn->virtualY + 15) & ~15) * info->depthPitch
		  * depthCpp + RADEON_BUFFER_ALIGN) & ~RADEON_BUFFER_ALIGN);

    switch (info->CPMode) {
    case RADEON_DEFAULT_CP_PIO_MODE:
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CP in PIO mode\n");
	break;
    case RADEON_DEFAULT_CP_BM_MODE:
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CP in BM mode\n");
	break;
    default:
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "CP in UNKNOWN mode\n");
	break;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Using %d MB GART aperture\n", info->gartSize);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Using %d MB for the ring buffer\n", info->ringSize);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Using %d MB for vertex/indirect buffers\n", info->bufSize);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Using %d MB for GART textures\n", info->gartTexSize);

    /* Try for front, back, depth, and three framebuffers worth of
     * pixmap cache.  Should be enough for a fullscreen background
     * image plus some leftovers.
     * If the FBTexPercent option was used, try to achieve that percentage instead,
     * but still have at least one pixmap buffer (get problems with xvideo/render
     * otherwise probably), and never reserve more than 3 offscreen buffers as it's
     * probably useless for XAA.
     */
    if (info->textureSize >= 0) {
	texsizerequest = ((int)info->FbMapSize - 2 * bufferSize - depthSize
			 - 2 * width_bytes - 16384 - info->FbSecureSize)
	/* first divide, then multiply or we'll get an overflow (been there...) */
			 / 100 * info->textureSize;
    }
    else {
	texsizerequest = (int)info->FbMapSize / 2;
    }
    info->textureSize = info->FbMapSize - info->FbSecureSize - 5 * bufferSize - depthSize;

    /* If that gives us less than the requested memory, let's
     * be greedy and grab some more.  Sorry, I care more about 3D
     * performance than playing nicely, and you'll get around a full
     * framebuffer's worth of pixmap cache anyway.
     */
    if (info->textureSize < texsizerequest) {
        info->textureSize = info->FbMapSize - 4 * bufferSize - depthSize;
    }
    if (info->textureSize < texsizerequest) {
        info->textureSize = info->FbMapSize - 3 * bufferSize - depthSize;
    }

    /* If there's still no space for textures, try without pixmap cache, but
     * never use the reserved space, the space hw cursor and PCIGART table might
     * use.
     */
    if (info->textureSize < 0) {
	info->textureSize = info->FbMapSize - 2 * bufferSize - depthSize
	                    - 2 * width_bytes - 16384 - info->FbSecureSize;
    }

    /* Check to see if there is more room available after the 8192nd
     * scanline for textures
     */
    /* FIXME: what's this good for? condition is pretty much impossible to meet */
    if ((int)info->FbMapSize - 8192*width_bytes - bufferSize - depthSize
	> info->textureSize) {
	info->textureSize =
		info->FbMapSize - 8192*width_bytes - bufferSize - depthSize;
    }

    /* If backbuffer is disabled, don't allocate memory for it */
    if (info->noBackBuffer) {
	info->textureSize += bufferSize;
    }

    /* RADEON_BUFFER_ALIGN is not sufficient for backbuffer!
       At least for pageflip + color tiling, need to make sure it's 16 scanlines aligned,
       otherwise the copy-from-front-to-back will fail (width_bytes * 16 will also guarantee
       it's still 4kb aligned for tiled case). Need to round up offset (might get into cursor
       area otherwise).
       This might cause some space at the end of the video memory to be unused, since it
       can't be used (?) due to that log_tex_granularity thing???
       Could use different copyscreentoscreen function for the pageflip copies
       (which would use different src and dst offsets) to avoid this. */   
    if (info->allowColorTiling && !info->noBackBuffer) {
	info->textureSize = info->FbMapSize - ((info->FbMapSize - info->textureSize +
			  width_bytes * 16 - 1) / (width_bytes * 16)) * (width_bytes * 16);
    }
    if (info->textureSize > 0) {
	l = RADEONMinBits((info->textureSize-1) / RADEON_NR_TEX_REGIONS);
	if (l < RADEON_LOG_TEX_GRANULARITY)
	    l = RADEON_LOG_TEX_GRANULARITY;
	/* Round the texture size up to the nearest whole number of
	 * texture regions.  Again, be greedy about this, don't
	 * round down.
	 */
	info->log2TexGran = l;
	info->textureSize = (info->textureSize >> l) << l;
    } else {
	info->textureSize = 0;
    }

    /* Set a minimum usable local texture heap size.  This will fit
     * two 256x256x32bpp textures.
     */
    if (info->textureSize < 512 * 1024) {
	info->textureOffset = 0;
	info->textureSize = 0;
    }

    if (info->allowColorTiling && !info->noBackBuffer) {
	info->textureOffset = ((info->FbMapSize - info->textureSize) /
			       (width_bytes * 16)) * (width_bytes * 16);
    }
    else {
	/* Reserve space for textures */
	info->textureOffset = ((info->FbMapSize - info->textureSize +
				RADEON_BUFFER_ALIGN) &
			       ~(CARD32)RADEON_BUFFER_ALIGN);
    }

    /* Reserve space for the shared depth
     * buffer.
     */
    info->depthOffset = ((info->textureOffset - depthSize +
			  RADEON_BUFFER_ALIGN) &
			 ~(CARD32)RADEON_BUFFER_ALIGN);

    /* Reserve space for the shared back buffer */
    if (info->noBackBuffer) {
       info->backOffset = info->depthOffset;
    } else {
       info->backOffset = ((info->depthOffset - bufferSize +
			    RADEON_BUFFER_ALIGN) &
			   ~(CARD32)RADEON_BUFFER_ALIGN);
    }

    info->backY = info->backOffset / width_bytes;
    info->backX = (info->backOffset - (info->backY * width_bytes)) / cpp;

    scanlines = (info->FbMapSize-info->FbSecureSize) / width_bytes;
    if (scanlines > 8191)
	scanlines = 8191;

    MemBox.x1 = 0;
    MemBox.y1 = 0;
    MemBox.x2 = pScrn->displayWidth;
    MemBox.y2 = scanlines;

    if (!xf86InitFBManager(pScreen, &MemBox)) {
        xf86DrvMsg(scrnIndex, X_ERROR,
		   "Memory manager initialization to "
		   "(%d,%d) (%d,%d) failed\n",
		   MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	return FALSE;
    } else {
	int  width, height;

	xf86DrvMsg(scrnIndex, X_INFO,
		   "Memory manager initialized to (%d,%d) (%d,%d)\n",
		   MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	/* why oh why can't we just request modes which are guaranteed to be 16 lines
	   aligned... sigh */
	if ((fbarea = xf86AllocateOffscreenArea(pScreen,
						pScrn->displayWidth,
						info->allowColorTiling ? 
						((pScrn->virtualY + 15) & ~15)
						- pScrn->virtualY + 2 : 2,
						0, NULL, NULL,
						NULL))) {
	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Reserved area from (%d,%d) to (%d,%d)\n",
		       fbarea->box.x1, fbarea->box.y1,
		       fbarea->box.x2, fbarea->box.y2);
	} else {
	    xf86DrvMsg(scrnIndex, X_ERROR, "Unable to reserve area\n");
	}

	RADEONDRIAllocatePCIGARTTable(pScreen);

	if (xf86QueryLargestOffscreenArea(pScreen, &width,
					  &height, 0, 0, 0)) {
	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Largest offscreen area available: %d x %d\n",
		       width, height);

	    /* Lines in offscreen area needed for depth buffer and
	     * textures
	     */
	    info->depthTexLines = (scanlines
				   - info->depthOffset / width_bytes);
	    info->backLines	    = (scanlines
				       - info->backOffset / width_bytes
				       - info->depthTexLines);
	    info->backArea	    = NULL;
	} else {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Unable to determine largest offscreen area "
		       "available\n");
	    return FALSE;
	}
    }

    xf86DrvMsg(scrnIndex, X_INFO,
	       "Will use front buffer at offset 0x%x\n",
	       info->frontOffset);

    xf86DrvMsg(scrnIndex, X_INFO,
	       "Will use back buffer at offset 0x%x\n",
	       info->backOffset);
    xf86DrvMsg(scrnIndex, X_INFO,
	       "Will use depth buffer at offset 0x%x\n",
	       info->depthOffset);
    if (info->cardType==CARD_PCIE)
    	xf86DrvMsg(scrnIndex, X_INFO,
	           "Will use %d kb for PCI GART table at offset 0x%lx\n",
		   info->pciGartSize/1024, info->pciGartOffset);
    xf86DrvMsg(scrnIndex, X_INFO,
	       "Will use %d kb for textures at offset 0x%x\n",
	       info->textureSize/1024, info->textureOffset);

    info->frontPitchOffset = (((info->frontPitch * cpp / 64) << 22) |
			      ((info->frontOffset + info->fbLocation) >> 10));

    info->backPitchOffset = (((info->backPitch * cpp / 64) << 22) |
			     ((info->backOffset + info->fbLocation) >> 10));

    info->depthPitchOffset = (((info->depthPitch * depthCpp / 64) << 22) |
			      ((info->depthOffset + info->fbLocation) >> 10));
    return TRUE;
}
#endif /* XF86DRI */

Bool RADEONSetupMemXAA(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    BoxRec         MemBox;
    int            y2;

    int width_bytes = pScrn->displayWidth * info->CurrentLayout.pixel_bytes;

    MemBox.x1 = 0;
    MemBox.y1 = 0;
    MemBox.x2 = pScrn->displayWidth;
    y2 = info->FbMapSize / width_bytes;
    if (y2 >= 32768)
	y2 = 32767; /* because MemBox.y2 is signed short */
    MemBox.y2 = y2;
    
    /* The acceleration engine uses 14 bit
     * signed coordinates, so we can't have any
     * drawable caches beyond this region.
     */
    if (MemBox.y2 > 8191)
	MemBox.y2 = 8191;

    if (!xf86InitFBManager(pScreen, &MemBox)) {
	xf86DrvMsg(scrnIndex, X_ERROR,
		   "Memory manager initialization to "
		   "(%d,%d) (%d,%d) failed\n",
		   MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	return FALSE;
    } else {
	int       width, height;
	FBAreaPtr fbarea;

	xf86DrvMsg(scrnIndex, X_INFO,
		   "Memory manager initialized to (%d,%d) (%d,%d)\n",
		   MemBox.x1, MemBox.y1, MemBox.x2, MemBox.y2);
	if ((fbarea = xf86AllocateOffscreenArea(pScreen,
						pScrn->displayWidth,
						info->allowColorTiling ? 
						((pScrn->virtualY + 15) & ~15)
						- pScrn->virtualY + 2 : 2,
						0, NULL, NULL,
						NULL))) {
	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Reserved area from (%d,%d) to (%d,%d)\n",
		       fbarea->box.x1, fbarea->box.y1,
		       fbarea->box.x2, fbarea->box.y2);
	} else {
	    xf86DrvMsg(scrnIndex, X_ERROR, "Unable to reserve area\n");
	}
	if (xf86QueryLargestOffscreenArea(pScreen, &width, &height,
					      0, 0, 0)) {
	    xf86DrvMsg(scrnIndex, X_INFO,
		       "Largest offscreen area available: %d x %d\n",
		       width, height);
	}
	return TRUE;
    }    
}
#endif /* USE_XAA */

static void
RADEONPointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    int newX = x, newY = y;

    switch (info->rotation) {
    case RR_Rotate_0:
	break;
    case RR_Rotate_90:
	newX = y;
	newY = pScrn->pScreen->width - x - 1;
	break;
    case RR_Rotate_180:
	newX = pScrn->pScreen->width - x - 1;
	newY = pScrn->pScreen->height - y - 1;
	break;
    case RR_Rotate_270:
	newX = pScrn->pScreen->height - y - 1;
	newY = x;
	break;
    }

    (*info->PointerMoved)(index, newX, newY);
}

/* Called at the start of each server generation. */
Bool RADEONScreenInit(int scrnIndex, ScreenPtr pScreen,
                                int argc, char **argv)
{
    ScrnInfoPtr    pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int            hasDRI = 0;
#ifdef RENDER
    int            subPixelOrder = SubPixelUnknown;
    char*          s;
#endif

#ifdef XF86DRI
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONScreenInit %lx %ld %d\n",
		   pScrn->memPhysBase, pScrn->fbOffset, info->frontOffset);
#else
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONScreenInit %lx %ld\n",
		   pScrn->memPhysBase, pScrn->fbOffset);
#endif

    info->accelOn      = FALSE;
#ifdef USE_XAA
    info->accel        = NULL;
#endif
#ifdef XF86DRI
    pScrn->fbOffset    = info->frontOffset;
#endif

    if (!RADEONMapMem(pScrn)) return FALSE;

#ifdef XF86DRI
    info->fbX = 0;
    info->fbY = 0;
#endif

    info->PaletteSavedOnVT = FALSE;

    info->crtc_on = FALSE;
    info->crtc2_on = FALSE;

    RADEONSave(pScrn);

    RADEONDisableDisplays(pScrn);

    if (info->IsMobility) {
        if (xf86ReturnOptValBool(info->Options, OPTION_DYNAMIC_CLOCKS, FALSE)) {
	    RADEONSetDynamicClock(pScrn, 1);
        } else {
	    RADEONSetDynamicClock(pScrn, 0);
        }
    }

    if (IS_R300_VARIANT || IS_RV100_VARIANT)
	RADEONForceSomeClocks(pScrn);

    if (info->allowColorTiling && (pScrn->virtualX > info->MaxSurfaceWidth)) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Color tiling not supported with virtual x resolutions larger than %d, disabling\n",
		    info->MaxSurfaceWidth);
	info->allowColorTiling = FALSE;
    }
    if (info->allowColorTiling) {
        info->tilingEnabled = (pScrn->currentMode->Flags & (V_DBLSCAN | V_INTERLACE)) ? FALSE : TRUE;
    }

    /* Visual setup */
    miClearVisualTypes();
    if (!miSetVisualTypes(pScrn->depth,
			  miGetDefaultVisualMask(pScrn->depth),
			  pScrn->rgbBits,
			  pScrn->defaultVisual)) return FALSE;
    miSetPixmapDepths ();

#ifdef XF86DRI
    if (info->directRenderingEnabled) {
	MessageType from;

	info->depthBits = pScrn->depth;

	from = xf86GetOptValInteger(info->Options, OPTION_DEPTH_BITS,
				    &info->depthBits)
	     ? X_CONFIG : X_DEFAULT;

	if (info->depthBits != 16 && info->depthBits != 24) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Value for Option \"DepthBits\" must be 16 or 24\n");
	    info->depthBits = pScrn->depth;
	    from = X_DEFAULT;
	}

	xf86DrvMsg(pScrn->scrnIndex, from,
		   "Using %d bit depth buffer\n", info->depthBits);
    }


    hasDRI = info->directRenderingEnabled;
#endif /* XF86DRI */

    /* Initialize the memory map, this basically calculates the values
     * we'll use later on for MC_FB_LOCATION & MC_AGP_LOCATION
     */
    RADEONInitMemoryMap(pScrn);

    /* empty the surfaces */
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned int i;
    for (i = 0; i < 8; i++) {
	OUTREG(RADEON_SURFACE0_INFO + 16 * i, 0);
	OUTREG(RADEON_SURFACE0_LOWER_BOUND + 16 * i, 0);
	OUTREG(RADEON_SURFACE0_UPPER_BOUND + 16 * i, 0);
    }

#ifdef XF86DRI
    /* Depth moves are disabled by default since they are extremely slow */
    info->depthMoves = xf86ReturnOptValBool(info->Options,
						 OPTION_DEPTH_MOVE, FALSE);
    if (info->depthMoves && info->allowColorTiling) {
	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "Enabling depth moves\n");
    } else if (info->depthMoves) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Depth moves don't work without color tiling, disabled\n");
	info->depthMoves = FALSE;
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Depth moves disabled by default\n");
    }
#endif

    /* Initial setup of surfaces */
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
                   "Setting up initial surfaces\n");
    RADEONChangeSurfaces(pScrn);

				/* Memory manager setup */

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Setting up accel memmap\n");

#ifdef USE_EXA
    if (info->useEXA) {
#ifdef XF86DRI
	MessageType from = X_DEFAULT;

	if (hasDRI) {
	    info->accelDFS = info->cardType != CARD_AGP;

	    if (xf86GetOptValInteger(info->Options, OPTION_ACCEL_DFS,
				     &info->accelDFS)) {
		from = X_CONFIG;
	    }

	    /* Reserve approx. half of offscreen memory for local textures by
	     * default, can be overridden with Option "FBTexPercent".
	     * Round down to a whole number of texture regions.
	     */
	    info->textureSize = 50;

	    if (xf86GetOptValInteger(info->Options, OPTION_FBTEX_PERCENT,
				     &(info->textureSize))) {
		if (info->textureSize < 0 || info->textureSize > 100) {
		    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			       "Illegal texture memory percentage: %dx, setting to default 50%%\n",
			       info->textureSize);
		    info->textureSize = 50;
		}
	    }
	}

	xf86DrvMsg(pScrn->scrnIndex, from,
		   "%ssing accelerated EXA DownloadFromScreen hook\n",
		   info->accelDFS ? "U" : "Not u");
#endif /* XF86DRI */

	if (!RADEONSetupMemEXA(pScreen))
	    return FALSE;
    }
#endif

#if defined(XF86DRI) && defined(USE_XAA)
    if (!info->useEXA && hasDRI) {
	info->textureSize = -1;
	if (xf86GetOptValInteger(info->Options, OPTION_FBTEX_PERCENT,
				 &(info->textureSize))) {
	    if (info->textureSize < 0 || info->textureSize > 100) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Illegal texture memory percentage: %dx, using default behaviour\n",
			   info->textureSize);
		info->textureSize = -1;
	    }
	}
	if (!RADEONSetupMemXAA_DRI(scrnIndex, pScreen))
	    return FALSE;
    	pScrn->fbOffset    = info->frontOffset;
    }
#endif

#ifdef USE_XAA
    if (!info->useEXA && !hasDRI && !RADEONSetupMemXAA(scrnIndex, pScreen))
	return FALSE;
#endif

    info->dst_pitch_offset = (((pScrn->displayWidth * info->CurrentLayout.pixel_bytes / 64)
			       << 22) | ((info->fbLocation + pScrn->fbOffset) >> 10));

    /* Setup DRI after visuals have been established, but before fbScreenInit is
     * called.  fbScreenInit will eventually call the driver's InitGLXVisuals
     * call back. */
#ifdef XF86DRI
    if (info->directRenderingEnabled) {
	/* FIXME: When we move to dynamic allocation of back and depth
	 * buffers, we will want to revisit the following check for 3
	 * times the virtual size of the screen below.
	 */
	int  width_bytes = (pScrn->displayWidth *
			    info->CurrentLayout.pixel_bytes);
	int  maxy        = info->FbMapSize / width_bytes;

	if (maxy <= pScrn->virtualY * 3) {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Static buffer allocation failed.  Disabling DRI.\n");
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "At least %d kB of video memory needed at this "
		       "resolution and depth.\n",
		       (pScrn->displayWidth * pScrn->virtualY *
			info->CurrentLayout.pixel_bytes * 3 + 1023) / 1024);
	    info->directRenderingEnabled = FALSE;
	} else {
	    info->directRenderingEnabled = RADEONDRIScreenInit(pScreen);
	}
    }

    /* Tell DRI about new memory map */
    if (info->directRenderingEnabled && info->newMemoryMap) {
        if (RADEONDRISetParam(pScrn, RADEON_SETPARAM_NEW_MEMMAP, 1) < 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "[drm] failed to enable new memory map\n");
		RADEONDRICloseScreen(pScreen);
		info->directRenderingEnabled = FALSE;		
	}
    }
#endif
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Initializing fb layer\n");

    /* Init fb layer */
    if (!fbScreenInit(pScreen, info->FB + pScrn->fbOffset,
		      pScrn->virtualX, pScrn->virtualY,
		      pScrn->xDpi, pScrn->yDpi, pScrn->displayWidth,
		      pScrn->bitsPerPixel))
	return FALSE;

    xf86SetBlackWhitePixels(pScreen);

    if (pScrn->bitsPerPixel > 8) {
	VisualPtr  visual;

	visual = pScreen->visuals + pScreen->numVisuals;
	while (--visual >= pScreen->visuals) {
	    if ((visual->class | DynamicClass) == DirectColor) {
		visual->offsetRed   = pScrn->offset.red;
		visual->offsetGreen = pScrn->offset.green;
		visual->offsetBlue  = pScrn->offset.blue;
		visual->redMask     = pScrn->mask.red;
		visual->greenMask   = pScrn->mask.green;
		visual->blueMask    = pScrn->mask.blue;
	    }
	}
    }

    /* Must be after RGB order fixed */
    fbPictureInit (pScreen, 0, 0);

#ifdef RENDER
    if ((s = xf86GetOptValString(info->Options, OPTION_SUBPIXEL_ORDER))) {
	if (strcmp(s, "RGB") == 0) subPixelOrder = SubPixelHorizontalRGB;
	else if (strcmp(s, "BGR") == 0) subPixelOrder = SubPixelHorizontalBGR;
	else if (strcmp(s, "NONE") == 0) subPixelOrder = SubPixelNone;
	PictureSetSubpixelOrder (pScreen, subPixelOrder);
    } 
#endif

    pScrn->vtSema = TRUE;
    if (info->FBDev) {
	unsigned char *RADEONMMIO = info->MMIO;

	if (!fbdevHWModeInit(pScrn, pScrn->currentMode)) return FALSE;
	pScrn->displayWidth = fbdevHWGetLineLength(pScrn)
		/ info->CurrentLayout.pixel_bytes;
	RADEONSaveMemMapRegisters(pScrn, &info->ModeReg);
	info->fbLocation = (info->ModeReg.mc_fb_location & 0xffff) << 16;
	info->ModeReg.surface_cntl = INREG(RADEON_SURFACE_CNTL);
	info->ModeReg.surface_cntl &= ~RADEON_SURF_TRANSLATION_DIS;
    } else {
	int i;
	for (i = 0; i < xf86_config->num_crtc; i++)
	{
	    xf86CrtcPtr	crtc = xf86_config->crtc[i];
	    
	    /* Mark that we'll need to re-set the mode for sure */
	    memset(&crtc->mode, 0, sizeof(crtc->mode));
	    if (!crtc->desiredMode.CrtcHDisplay) {
		crtc->desiredMode = *RADEONCrtcFindClosestMode (crtc, pScrn->currentMode);
		crtc->desiredRotation = RR_Rotate_0;
		crtc->desiredX = 0;
		crtc->desiredY = 0;
	    }

	    if (!xf86CrtcSetMode (crtc, &crtc->desiredMode, crtc->desiredRotation, crtc->desiredX, crtc->desiredY))
		return FALSE;

	}
    }

    RADEONSaveScreen(pScreen, SCREEN_SAVER_ON);

    //    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    /* Backing store setup */
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Initializing backing store\n");
    miInitializeBackingStore(pScreen);
    xf86SetBackingStore(pScreen);

    /* DRI finalisation */
#ifdef XF86DRI
    if (info->directRenderingEnabled && info->cardType==CARD_PCIE &&
        info->pKernelDRMVersion->version_minor >= 19)
    {
      if (RADEONDRISetParam(pScrn, RADEON_SETPARAM_PCIGART_LOCATION, info->pciGartOffset) < 0)
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "[drm] failed set pci gart location\n");

      if (info->pKernelDRMVersion->version_minor >= 26) {
	if (RADEONDRISetParam(pScrn, RADEON_SETPARAM_PCIGART_TABLE_SIZE, info->pciGartSize) < 0)
	  xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		     "[drm] failed set pci gart table size\n");
      }
    }
    if (info->directRenderingEnabled) {
        xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		       "DRI Finishing init !\n");
	info->directRenderingEnabled = RADEONDRIFinishScreenInit(pScreen);
    }
    if (info->directRenderingEnabled) {
	/* DRI final init might have changed the memory map, we need to adjust
	 * our local image to make sure we restore them properly on mode
	 * changes or VT switches
	 */
	RADEONAdjustMemMapRegisters(pScrn, &info->ModeReg);

	if ((info->DispPriority == 1) && (info->cardType==CARD_AGP)) {
	    /* we need to re-calculate bandwidth because of AGPMode difference. */ 
	    RADEONInitDispBandwidth(pScrn);
	}
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Direct rendering enabled\n");

	/* we might already be in tiled mode, tell drm about it */
	if (info->directRenderingEnabled && info->tilingEnabled) {
	  if (RADEONDRISetParam(pScrn, RADEON_SETPARAM_SWITCH_TILING, (info->tilingEnabled ? 1 : 0)) < 0)
  	      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			 "[drm] failed changing tiling status\n");
	}
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, 
		   "Direct rendering disabled\n");
    }
#endif

    /* Make sure surfaces are allright since DRI setup may have changed them */
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
                   "Setting up final surfaces\n");

    RADEONChangeSurfaces(pScrn);

    /* Enable aceleration */
    if (!xf86ReturnOptValBool(info->Options, OPTION_NOACCEL, FALSE)) {
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		       "Initializing Acceleration\n");
	if (RADEONAccelInit(pScreen)) {
	    xf86DrvMsg(scrnIndex, X_INFO, "Acceleration enabled\n");
	    info->accelOn = TRUE;
	} else {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Acceleration initialization failed\n");
	    xf86DrvMsg(scrnIndex, X_INFO, "Acceleration disabled\n");
	    info->accelOn = FALSE;
	}
    } else {
	xf86DrvMsg(scrnIndex, X_INFO, "Acceleration disabled\n");
	info->accelOn = FALSE;
    }

    /* Init DPMS */
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Initializing DPMS\n");
    xf86DPMSInit(pScreen, xf86DPMSSet, 0);

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Initializing Cursor\n");

    /* Set Silken Mouse */
    xf86SetSilkenMouse(pScreen);

    /* Cursor setup */
    miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

    /* Hardware cursor setup */
    if (!xf86ReturnOptValBool(info->Options, OPTION_SW_CURSOR, FALSE)) {
	if (RADEONCursorInit(pScreen)) {
#ifdef USE_XAA
	    if (!info->useEXA) {
		int  width, height;

		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Using hardware cursor (scanline %ld)\n",
			   info->cursor_offset / pScrn->displayWidth
			   / info->CurrentLayout.pixel_bytes);
		if (xf86QueryLargestOffscreenArea(pScreen, &width, &height,
					      0, 0, 0)) {
		    xf86DrvMsg(scrnIndex, X_INFO,
			       "Largest offscreen area available: %d x %d\n",
			       width, height);
		}
	    }
#endif /* USE_XAA */
	} else {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "Hardware cursor initialization failed\n");
	    xf86DrvMsg(scrnIndex, X_INFO, "Using software cursor\n");
	}
    } else {
	info->cursor_offset = 0;
	xf86DrvMsg(scrnIndex, X_INFO, "Using software cursor\n");
    }

    /* DGA setup */
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Initializing DGA\n");
    RADEONDGAInit(pScreen);

    /* Init Xv */
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Initializing Xv\n");
    RADEONInitVideo(pScreen);

    /* Provide SaveScreen & wrap BlockHandler and CloseScreen */
    /* Wrap CloseScreen */
    info->CloseScreen    = pScreen->CloseScreen;
    pScreen->CloseScreen = RADEONCloseScreen;
    pScreen->SaveScreen  = RADEONSaveScreen;
    info->BlockHandler = pScreen->BlockHandler;
    pScreen->BlockHandler = RADEONBlockHandler;

   if (!xf86CrtcScreenInit (pScreen))
       return FALSE;

    /* Wrap pointer motion to flip touch screen around */
    info->PointerMoved = pScrn->PointerMoved;
    pScrn->PointerMoved = RADEONPointerMoved;

    /* Colormap setup */
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
                   "Initializing color map\n");
    if (!miCreateDefColormap(pScreen)) return FALSE;
    if (!xf86HandleColormaps(pScreen, 256, info->dac6bits ? 6 : 8,
			     RADEONLoadPalette, NULL,
			     CMAP_PALETTED_TRUECOLOR
#if 0 /* This option messes up text mode! (eich@suse.de) */
			     | CMAP_LOAD_EVEN_IF_OFFSCREEN
#endif
			     | CMAP_RELOAD_ON_MODE_SWITCH)) return FALSE;

    /* Note unused options */
    if (serverGeneration == 1)
	xf86ShowUnusedOptions(pScrn->scrnIndex, pScrn->options);

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONScreenInit finished\n");

    return TRUE;
}

/* Write memory mapping registers */
void RADEONRestoreMemMapRegisters(ScrnInfoPtr pScrn,
					 RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int timeout;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "RADEONRestoreMemMapRegisters() : \n");
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "  MC_FB_LOCATION   : 0x%08lx\n", restore->mc_fb_location);
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "  MC_AGP_LOCATION  : 0x%08lx\n", restore->mc_agp_location);

    /* Write memory mapping registers only if their value change
     * since we must ensure no access is done while they are
     * reprogrammed
     */
    if (INREG(RADEON_MC_FB_LOCATION) != restore->mc_fb_location ||
	INREG(RADEON_MC_AGP_LOCATION) != restore->mc_agp_location) {
	CARD32 crtc_ext_cntl, crtc_gen_cntl, crtc2_gen_cntl=0, ov0_scale_cntl;
	CARD32 old_mc_status, status_idle;

	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		       "  Map Changed ! Applying ...\n");

	/* Make sure engine is idle. We assume the CCE is stopped
	 * at this point
	 */
	RADEONWaitForIdleMMIO(pScrn);

	if (info->IsIGP)
		goto igp_no_mcfb;

	/* Capture MC_STATUS in case things go wrong ... */
	old_mc_status = INREG(RADEON_MC_STATUS);

	/* Stop display & memory access */
	ov0_scale_cntl = INREG(RADEON_OV0_SCALE_CNTL);
	OUTREG(RADEON_OV0_SCALE_CNTL, ov0_scale_cntl & ~RADEON_SCALER_ENABLE);
	crtc_ext_cntl = INREG(RADEON_CRTC_EXT_CNTL);
	OUTREG(RADEON_CRTC_EXT_CNTL, crtc_ext_cntl | RADEON_CRTC_DISPLAY_DIS);
	crtc_gen_cntl = INREG(RADEON_CRTC_GEN_CNTL);
	RADEONWaitForVerticalSync(pScrn);
	OUTREG(RADEON_CRTC_GEN_CNTL,
	       (crtc_gen_cntl
		& ~(RADEON_CRTC_CUR_EN | RADEON_CRTC_ICON_EN))
	       | RADEON_CRTC_DISP_REQ_EN_B | RADEON_CRTC_EXT_DISP_EN);

 	if (pRADEONEnt->HasCRTC2) {
	    crtc2_gen_cntl = INREG(RADEON_CRTC2_GEN_CNTL);
	    RADEONWaitForVerticalSync2(pScrn);
	    OUTREG(RADEON_CRTC2_GEN_CNTL,
		   (crtc2_gen_cntl
		    & ~(RADEON_CRTC2_CUR_EN | RADEON_CRTC2_ICON_EN))
		   | RADEON_CRTC2_DISP_REQ_EN_B);
	}

 	/* Make sure the chip settles down (paranoid !) */ 
 	usleep(100000);

	/* Wait for MC idle */
	if (IS_R300_VARIANT)
	    status_idle = R300_MC_IDLE;
	else
	    status_idle = RADEON_MC_IDLE;

	timeout = 0;
	while (!(INREG(RADEON_MC_STATUS) & status_idle)) {
	    if (++timeout > 1000000) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Timeout trying to update memory controller settings !\n");
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "MC_STATUS = 0x%08x (on entry = 0x%08x)\n",
			   INREG(RADEON_MC_STATUS), (unsigned int)old_mc_status);
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "You will probably crash now ... \n");
		/* Nothing we can do except maybe try to kill the server,
		 * let's wait 2 seconds to leave the above message a chance
		 * to maybe hit the disk and continue trying to setup despite
		 * the MC being non-idle
		 */
		usleep(2000000);
	    }
	    usleep(10);
	}

	/* Update maps, first clearing out AGP to make sure we don't get
	 * a temporary overlap
	 */
 	OUTREG(RADEON_MC_AGP_LOCATION, 0xfffffffc);
	OUTREG(RADEON_MC_FB_LOCATION, restore->mc_fb_location);
    igp_no_mcfb:
 	OUTREG(RADEON_MC_AGP_LOCATION, restore->mc_agp_location);
	/* Make sure map fully reached the chip */
	(void)INREG(RADEON_MC_FB_LOCATION);

	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		       "  Map applied, resetting engine ...\n");

	/* Reset the engine and HDP */
	RADEONEngineReset(pScrn);

	/* Make sure we have sane offsets before re-enabling the CRTCs, disable
	 * stereo, clear offsets, and wait for offsets to catch up with hw
	 */

	OUTREG(RADEON_CRTC_OFFSET_CNTL, RADEON_CRTC_OFFSET_FLIP_CNTL);
	OUTREG(RADEON_CRTC_OFFSET, 0);
	OUTREG(RADEON_CUR_OFFSET, 0);
	timeout = 0;
	while(INREG(RADEON_CRTC_OFFSET) & RADEON_CRTC_OFFSET__GUI_TRIG_OFFSET) {
	    if (timeout++ > 1000000) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Timeout waiting for CRTC offset to update !\n");
		break;
	    }
	    usleep(1000);
	}
	if (pRADEONEnt->HasCRTC2) {
	    OUTREG(RADEON_CRTC2_OFFSET_CNTL, RADEON_CRTC2_OFFSET_FLIP_CNTL);
	    OUTREG(RADEON_CRTC2_OFFSET, 0);
	    OUTREG(RADEON_CUR2_OFFSET, 0);
	    timeout = 0;
	    while(INREG(RADEON_CRTC2_OFFSET) & RADEON_CRTC2_OFFSET__GUI_TRIG_OFFSET) {
		if (timeout++ > 1000000) {
		    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			       "Timeout waiting for CRTC2 offset to update !\n");
		    break;
		}
		usleep(1000);
	    }
	}
    }

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Updating display base addresses...\n");

    OUTREG(RADEON_DISPLAY_BASE_ADDR, restore->display_base_addr);
    if (pRADEONEnt->HasCRTC2)
        OUTREG(RADEON_DISPLAY2_BASE_ADDR, restore->display2_base_addr);
    OUTREG(RADEON_OV0_BASE_ADDR, restore->ov0_base_addr);
    (void)INREG(RADEON_OV0_BASE_ADDR);

    /* More paranoia delays, wait 100ms */
    usleep(100000);

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Memory map updated.\n");
 }

#ifdef XF86DRI
static void RADEONAdjustMemMapRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 fb, agp;

    fb = INREG(RADEON_MC_FB_LOCATION);
    agp = INREG(RADEON_MC_AGP_LOCATION);

    if (fb != info->mc_fb_location || agp != info->mc_agp_location) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "DRI init changed memory map, adjusting ...\n");
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "  MC_FB_LOCATION  was: 0x%08lx is: 0x%08lx\n",
		       info->mc_fb_location, fb);
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "  MC_AGP_LOCATION was: 0x%08lx is: 0x%08lx\n",
		       info->mc_agp_location, agp);
	    info->mc_fb_location = fb;
	    info->mc_agp_location = agp;
	    info->fbLocation = (save->mc_fb_location & 0xffff) << 16;
	    info->dst_pitch_offset =
		    (((pScrn->displayWidth * info->CurrentLayout.pixel_bytes / 64)
		      << 22) | ((info->fbLocation + pScrn->fbOffset) >> 10));


	    RADEONInitMemMapRegisters(pScrn, save, info);

	    /* Adjust the various offsets */
	    RADEONRestoreMemMapRegisters(pScrn, save);
    }

#ifdef USE_EXA
    if (info->accelDFS)
    {
	drmRadeonGetParam gp;
	int gart_base;

	memset(&gp, 0, sizeof(gp));
	gp.param = RADEON_PARAM_GART_BASE;
	gp.value = &gart_base;

	if (drmCommandWriteRead(info->drmFD, DRM_RADEON_GETPARAM, &gp,
				sizeof(gp)) < 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "Failed to determine GART area MC location, not using "
		       "accelerated DownloadFromScreen hook!\n");
	    info->accelDFS = FALSE;
	} else {
	    info->gartLocation = gart_base;
	}
    }
#endif /* USE_EXA */
}
#endif

/* Write common registers */
void RADEONRestoreCommonRegisters(ScrnInfoPtr pScrn,
					 RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_OVR_CLR,            restore->ovr_clr);
    OUTREG(RADEON_OVR_WID_LEFT_RIGHT, restore->ovr_wid_left_right);
    OUTREG(RADEON_OVR_WID_TOP_BOTTOM, restore->ovr_wid_top_bottom);
    OUTREG(RADEON_OV0_SCALE_CNTL,     restore->ov0_scale_cntl);
    OUTREG(RADEON_SUBPIC_CNTL,        restore->subpic_cntl);
    OUTREG(RADEON_VIPH_CONTROL,       restore->viph_control);
    OUTREG(RADEON_I2C_CNTL_1,         restore->i2c_cntl_1);
    OUTREG(RADEON_GEN_INT_CNTL,       restore->gen_int_cntl);
    OUTREG(RADEON_CAP0_TRIG_CNTL,     restore->cap0_trig_cntl);
    OUTREG(RADEON_CAP1_TRIG_CNTL,     restore->cap1_trig_cntl);
    OUTREG(RADEON_BUS_CNTL,           restore->bus_cntl);
    OUTREG(RADEON_SURFACE_CNTL,       restore->surface_cntl);

    /* Workaround for the VT switching problem in dual-head mode.  This
     * problem only occurs on RV style chips, typically when a FP and
     * CRT are connected.
     */
    if (pRADEONEnt->HasCRTC2 &&
	info->ChipFamily != CHIP_FAMILY_R200 &&
	!IS_R300_VARIANT) {
	CARD32        tmp;

	tmp = INREG(RADEON_DAC_CNTL2);
	OUTREG(RADEON_DAC_CNTL2, tmp & ~RADEON_DAC2_DAC_CLK_SEL);
	usleep(100000);
    }
}

/* Write miscellaneous registers which might have been destroyed by an fbdevHW
 * call
 */
static void RADEONRestoreFBDevRegisters(ScrnInfoPtr pScrn,
					 RADEONSavePtr restore)
{
#ifdef XF86DRI
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    /* Restore register for vertical blank interrupts */
    if (info->irq) {
	OUTREG(RADEON_GEN_INT_CNTL, restore->gen_int_cntl);
    }

    /* Restore registers for page flipping */
    if (info->allowPageFlip) {
	OUTREG(RADEON_CRTC_OFFSET_CNTL, restore->crtc_offset_cntl);
	if (pRADEONEnt->HasCRTC2) {
	    OUTREG(RADEON_CRTC2_OFFSET_CNTL, restore->crtc2_offset_cntl);
	}
    }
#endif
}

void RADEONRestoreDACRegisters(ScrnInfoPtr pScrn,
				       RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    if (IS_R300_VARIANT)
	OUTREGP(RADEON_GPIOPAD_A, restore->gpiopad_a, ~1);

    OUTREGP(RADEON_DAC_CNTL,
	    restore->dac_cntl,
	    RADEON_DAC_RANGE_CNTL |
	    RADEON_DAC_BLANKING);

    OUTREG(RADEON_DAC_CNTL2, restore->dac2_cntl);

    if ((info->ChipFamily != CHIP_FAMILY_RADEON) &&
    	(info->ChipFamily != CHIP_FAMILY_R200)) 
    OUTREG (RADEON_TV_DAC_CNTL, restore->tv_dac_cntl);

    OUTREG(RADEON_DISP_OUTPUT_CNTL, restore->disp_output_cntl);

    if ((info->ChipFamily == CHIP_FAMILY_R200) ||
	IS_R300_VARIANT) {
	OUTREG(RADEON_DISP_TV_OUT_CNTL, restore->disp_tv_out_cntl);
    } else {
	OUTREG(RADEON_DISP_HW_DEBUG, restore->disp_hw_debug);
    }

    OUTREG(RADEON_DAC_MACRO_CNTL, restore->dac_macro_cntl);

    /* R200 DAC connected via DVO */
    if (info->ChipFamily == CHIP_FAMILY_R200)
	OUTREG(RADEON_FP2_GEN_CNTL, restore->fp2_gen_cntl);
}

/* Write CRTC registers */
void RADEONRestoreCrtcRegisters(ScrnInfoPtr pScrn,
				       RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Programming CRTC1, offset: 0x%08lx\n",
		   restore->crtc_offset);

    /* We prevent the CRTC from hitting the memory controller until
     * fully programmed
     */
    OUTREG(RADEON_CRTC_GEN_CNTL, restore->crtc_gen_cntl |
	   RADEON_CRTC_DISP_REQ_EN_B);

    OUTREGP(RADEON_CRTC_EXT_CNTL,
	    restore->crtc_ext_cntl,
	    RADEON_CRTC_VSYNC_DIS |
	    RADEON_CRTC_HSYNC_DIS |
	    RADEON_CRTC_DISPLAY_DIS);

    OUTREG(RADEON_CRTC_H_TOTAL_DISP,    restore->crtc_h_total_disp);
    OUTREG(RADEON_CRTC_H_SYNC_STRT_WID, restore->crtc_h_sync_strt_wid);
    OUTREG(RADEON_CRTC_V_TOTAL_DISP,    restore->crtc_v_total_disp);
    OUTREG(RADEON_CRTC_V_SYNC_STRT_WID, restore->crtc_v_sync_strt_wid);

    OUTREG(RADEON_FP_H_SYNC_STRT_WID,   restore->fp_h_sync_strt_wid);
    OUTREG(RADEON_FP_V_SYNC_STRT_WID,   restore->fp_v_sync_strt_wid);
    OUTREG(RADEON_FP_CRTC_H_TOTAL_DISP, restore->fp_crtc_h_total_disp);
    OUTREG(RADEON_FP_CRTC_V_TOTAL_DISP, restore->fp_crtc_v_total_disp);

    if (IS_R300_VARIANT)
	OUTREG(R300_CRTC_TILE_X0_Y0, restore->crtc_tile_x0_y0);
    OUTREG(RADEON_CRTC_OFFSET_CNTL,     restore->crtc_offset_cntl);
    OUTREG(RADEON_CRTC_OFFSET,          restore->crtc_offset);

    OUTREG(RADEON_CRTC_PITCH,           restore->crtc_pitch);
    OUTREG(RADEON_DISP_MERGE_CNTL,      restore->disp_merge_cntl);
    OUTREG(RADEON_CRTC_MORE_CNTL,       restore->crtc_more_cntl);

    if (info->IsDellServer) {
	OUTREG(RADEON_TV_DAC_CNTL, restore->tv_dac_cntl);
	OUTREG(RADEON_DISP_HW_DEBUG, restore->disp_hw_debug);
	OUTREG(RADEON_DAC_CNTL2, restore->dac2_cntl);
	OUTREG(RADEON_CRTC2_GEN_CNTL, restore->crtc2_gen_cntl);
    }

    OUTREG(RADEON_CRTC_GEN_CNTL, restore->crtc_gen_cntl);
}

/* Write CRTC2 registers */
void RADEONRestoreCrtc2Registers(ScrnInfoPtr pScrn,
					RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    /*    CARD32	   crtc2_gen_cntl;*/

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Programming CRTC2, offset: 0x%08lx\n",
		   restore->crtc2_offset);

    /* We prevent the CRTC from hitting the memory controller until
     * fully programmed
     */
    OUTREG(RADEON_CRTC2_GEN_CNTL,
	   restore->crtc2_gen_cntl | RADEON_CRTC2_VSYNC_DIS |
	   RADEON_CRTC2_HSYNC_DIS | RADEON_CRTC2_DISP_DIS |
	   RADEON_CRTC2_DISP_REQ_EN_B);

    OUTREG(RADEON_CRTC2_H_TOTAL_DISP,    restore->crtc2_h_total_disp);
    OUTREG(RADEON_CRTC2_H_SYNC_STRT_WID, restore->crtc2_h_sync_strt_wid);
    OUTREG(RADEON_CRTC2_V_TOTAL_DISP,    restore->crtc2_v_total_disp);
    OUTREG(RADEON_CRTC2_V_SYNC_STRT_WID, restore->crtc2_v_sync_strt_wid);

    OUTREG(RADEON_FP_H2_SYNC_STRT_WID,   restore->fp_h2_sync_strt_wid);
    OUTREG(RADEON_FP_V2_SYNC_STRT_WID,   restore->fp_v2_sync_strt_wid);

    if (IS_R300_VARIANT)
	OUTREG(R300_CRTC2_TILE_X0_Y0, restore->crtc2_tile_x0_y0);
    OUTREG(RADEON_CRTC2_OFFSET_CNTL,     restore->crtc2_offset_cntl);
    OUTREG(RADEON_CRTC2_OFFSET,          restore->crtc2_offset);

    OUTREG(RADEON_CRTC2_PITCH,           restore->crtc2_pitch);
    OUTREG(RADEON_DISP2_MERGE_CNTL,      restore->disp2_merge_cntl);

    if (info->ChipFamily == CHIP_FAMILY_RS400) {
	OUTREG(RADEON_RS480_UNK_e30, restore->rs480_unk_e30);
	OUTREG(RADEON_RS480_UNK_e34, restore->rs480_unk_e34);
	OUTREG(RADEON_RS480_UNK_e38, restore->rs480_unk_e38);
	OUTREG(RADEON_RS480_UNK_e3c, restore->rs480_unk_e3c);
    }
    OUTREG(RADEON_CRTC2_GEN_CNTL, restore->crtc2_gen_cntl);

}

/* Write TMDS registers */
void RADEONRestoreFPRegisters(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_TMDS_PLL_CNTL,        restore->tmds_pll_cntl);
    OUTREG(RADEON_TMDS_TRANSMITTER_CNTL,restore->tmds_transmitter_cntl);
    OUTREG(RADEON_FP_GEN_CNTL,          restore->fp_gen_cntl);

    /* old AIW Radeon has some BIOS initialization problem
     * with display buffer underflow, only occurs to DFP
     */
    if (!pRADEONEnt->HasCRTC2)
	OUTREG(RADEON_GRPH_BUFFER_CNTL,
	       INREG(RADEON_GRPH_BUFFER_CNTL) & ~0x7f0000);

}

/* Write FP2 registers */
void RADEONRestoreFP2Registers(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_FP2_GEN_CNTL,         restore->fp2_gen_cntl);

}

/* Write RMX registers */
void RADEONRestoreRMXRegisters(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_FP_HORZ_STRETCH,      restore->fp_horz_stretch);
    OUTREG(RADEON_FP_VERT_STRETCH,      restore->fp_vert_stretch);

}

/* Write LVDS registers */
void RADEONRestoreLVDSRegisters(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    if (info->IsMobility) {
	OUTREG(RADEON_LVDS_GEN_CNTL,  restore->lvds_gen_cntl);
	/*OUTREG(RADEON_LVDS_PLL_CNTL,  restore->lvds_pll_cntl);*/  
	OUTREG(RADEON_BIOS_4_SCRATCH, restore->bios_4_scratch);
	OUTREG(RADEON_BIOS_5_SCRATCH, restore->bios_5_scratch);
	OUTREG(RADEON_BIOS_6_SCRATCH, restore->bios_6_scratch);
    }

}

/* Write to TV FIFO RAM */
static void RADEONWriteTVFIFO(ScrnInfoPtr pScrn, CARD16 addr,
			      CARD32 value)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 tmp;
    int i = 0;

    OUTREG(RADEON_TV_HOST_WRITE_DATA, value);

    OUTREG(RADEON_TV_HOST_RD_WT_CNTL, addr);
    OUTREG(RADEON_TV_HOST_RD_WT_CNTL, addr | RADEON_HOST_FIFO_WT);

    do {
	tmp = INREG(RADEON_TV_HOST_RD_WT_CNTL);
	if ((tmp & RADEON_HOST_FIFO_WT_ACK) == 0)
	    break;
	i++;
    }
    while (i < 10000);
    /*while ((tmp & RADEON_HOST_FIFO_WT_ACK) == 0);*/

    OUTREG(RADEON_TV_HOST_RD_WT_CNTL, 0);
}

/* Read from TV FIFO RAM */
static CARD32 RADEONReadTVFIFO(ScrnInfoPtr pScrn, CARD16 addr)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 tmp;
    int i = 0;
  
    OUTREG(RADEON_TV_HOST_RD_WT_CNTL, addr);
    OUTREG(RADEON_TV_HOST_RD_WT_CNTL, addr | RADEON_HOST_FIFO_RD);

    do {
	tmp = INREG(RADEON_TV_HOST_RD_WT_CNTL);
	if ((tmp & RADEON_HOST_FIFO_RD_ACK) == 0)
	    break;
	i++;
    }
    while (i < 10000);
    /*while ((tmp & RADEON_HOST_FIFO_RD_ACK) == 0);*/

    OUTREG(RADEON_TV_HOST_RD_WT_CNTL, 0);

    return INREG(RADEON_TV_HOST_READ_DATA);
}

/* Get FIFO addresses of horizontal & vertical code timing tables from
 * settings of uv_adr register. 
 */
static CARD16 RADEONGetHTimingTablesAddr(CARD32 tv_uv_adr)
{
    CARD16 hTable;

    switch ((tv_uv_adr & RADEON_HCODE_TABLE_SEL_MASK) >> RADEON_HCODE_TABLE_SEL_SHIFT) {
    case 0:
	hTable = RADEON_TV_MAX_FIFO_ADDR_INTERNAL;
	break;
    case 1:
	hTable = ((tv_uv_adr & RADEON_TABLE1_BOT_ADR_MASK) >> RADEON_TABLE1_BOT_ADR_SHIFT) * 2;
	break;
    case 2:
	hTable = ((tv_uv_adr & RADEON_TABLE3_TOP_ADR_MASK) >> RADEON_TABLE3_TOP_ADR_SHIFT) * 2;
	break;
    default:
	/* Of course, this should never happen */
	hTable = 0;
	break;
    }
    return hTable;
}

static CARD16 RADEONGetVTimingTablesAddr(CARD32 tv_uv_adr)
{
    CARD16 vTable;

    switch ((tv_uv_adr & RADEON_VCODE_TABLE_SEL_MASK) >> RADEON_VCODE_TABLE_SEL_SHIFT) {
    case 0:
	vTable = ((tv_uv_adr & RADEON_MAX_UV_ADR_MASK) >> RADEON_MAX_UV_ADR_SHIFT) * 2 + 1;
	break;
    case 1:
	vTable = ((tv_uv_adr & RADEON_TABLE1_BOT_ADR_MASK) >> RADEON_TABLE1_BOT_ADR_SHIFT) * 2 + 1;
	break;
    case 2:
	vTable = ((tv_uv_adr & RADEON_TABLE3_TOP_ADR_MASK) >> RADEON_TABLE3_TOP_ADR_SHIFT) * 2 + 1;
	break;
    default:
	/* Of course, this should never happen */
	vTable = 0;
	break;
    }
    return vTable;
}

/* Restore horizontal/vertical timing code tables */
void RADEONRestoreTVTimingTables(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD16 hTable;
    CARD16 vTable;
    CARD32 tmp;
    unsigned i;

    OUTREG(RADEON_TV_UV_ADR, restore->tv_uv_adr);
    hTable = RADEONGetHTimingTablesAddr(restore->tv_uv_adr);
    vTable = RADEONGetVTimingTablesAddr(restore->tv_uv_adr);

    for (i = 0; i < MAX_H_CODE_TIMING_LEN; i += 2, hTable--) {
	tmp = ((CARD32)restore->h_code_timing[ i ] << 14) | ((CARD32)restore->h_code_timing[ i + 1 ]);
	RADEONWriteTVFIFO(pScrn, hTable, tmp);
	if (restore->h_code_timing[ i ] == 0 || restore->h_code_timing[ i + 1 ] == 0)
	    break;
    }

    for (i = 0; i < MAX_V_CODE_TIMING_LEN; i += 2, vTable++) {
	tmp = ((CARD32)restore->v_code_timing[ i + 1 ] << 14) | ((CARD32)restore->v_code_timing[ i ]);
	RADEONWriteTVFIFO(pScrn, vTable, tmp);
	if (restore->v_code_timing[ i ] == 0 || restore->v_code_timing[ i + 1 ] == 0)
	    break;
    }
}

/* restore TV PLLs */
static void RADEONRestoreTVPLLRegisters(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{

    OUTPLLP(pScrn, RADEON_TV_PLL_CNTL1, 0, ~RADEON_TVCLK_SRC_SEL_TVPLL);
    OUTPLL(pScrn, RADEON_TV_PLL_CNTL, restore->tv_pll_cntl);
    OUTPLLP(pScrn, RADEON_TV_PLL_CNTL1, RADEON_TVPLL_RESET, ~RADEON_TVPLL_RESET);

    RADEONWaitPLLLock(pScrn, 200, 800, 135);
  
    OUTPLLP(pScrn, RADEON_TV_PLL_CNTL1, 0, ~RADEON_TVPLL_RESET);

    RADEONWaitPLLLock(pScrn, 300, 160, 27);
    RADEONWaitPLLLock(pScrn, 200, 800, 135);
  
    OUTPLLP(pScrn, RADEON_TV_PLL_CNTL1, 0, ~0xf);
    OUTPLLP(pScrn, RADEON_TV_PLL_CNTL1, RADEON_TVCLK_SRC_SEL_TVPLL, ~RADEON_TVCLK_SRC_SEL_TVPLL);
  
    OUTPLLP(pScrn, RADEON_TV_PLL_CNTL1, (1 << RADEON_TVPDC_SHIFT), ~RADEON_TVPDC_MASK);
    OUTPLLP(pScrn, RADEON_TV_PLL_CNTL1, 0, ~RADEON_TVPLL_SLEEP);
}

/* Restore TV horizontal/vertical settings */
static void RADEONRestoreTVHVRegisters(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_TV_RGB_CNTL, restore->tv_rgb_cntl);

    OUTREG(RADEON_TV_HTOTAL, restore->tv_htotal);
    OUTREG(RADEON_TV_HDISP, restore->tv_hdisp);
    OUTREG(RADEON_TV_HSTART, restore->tv_hstart);

    OUTREG(RADEON_TV_VTOTAL, restore->tv_vtotal);
    OUTREG(RADEON_TV_VDISP, restore->tv_vdisp);

    OUTREG(RADEON_TV_FTOTAL, restore->tv_ftotal);

    OUTREG(RADEON_TV_VSCALER_CNTL1, restore->tv_vscaler_cntl1);
    OUTREG(RADEON_TV_VSCALER_CNTL2, restore->tv_vscaler_cntl2);

    OUTREG(RADEON_TV_Y_FALL_CNTL, restore->tv_y_fall_cntl);
    OUTREG(RADEON_TV_Y_RISE_CNTL, restore->tv_y_rise_cntl);
    OUTREG(RADEON_TV_Y_SAW_TOOTH_CNTL, restore->tv_y_saw_tooth_cntl);
}

/* restore TV RESTART registers */
void RADEONRestoreTVRestarts(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_TV_FRESTART, restore->tv_frestart);
    OUTREG(RADEON_TV_HRESTART, restore->tv_hrestart);
    OUTREG(RADEON_TV_VRESTART, restore->tv_vrestart);
}

/* restore tv standard & output muxes */
static void RADEONRestoreTVOutputStd(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    OUTREG(RADEON_TV_SYNC_CNTL, restore->tv_sync_cntl);
  
    OUTREG(RADEON_TV_TIMING_CNTL, restore->tv_timing_cntl);

    OUTREG(RADEON_TV_MODULATOR_CNTL1, restore->tv_modulator_cntl1);
    OUTREG(RADEON_TV_MODULATOR_CNTL2, restore->tv_modulator_cntl2);
 
    OUTREG(RADEON_TV_PRE_DAC_MUX_CNTL, restore->tv_pre_dac_mux_cntl);

    OUTREG(RADEON_TV_CRC_CNTL, restore->tv_crc_cntl);
}

/* Restore TV out regs */
void RADEONRestoreTVRegisters(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    ErrorF("Entering Restore TV\n");

    OUTREG(RADEON_TV_MASTER_CNTL, (restore->tv_master_cntl
				   | RADEON_TV_ASYNC_RST
				   | RADEON_CRT_ASYNC_RST
				   | RADEON_TV_FIFO_ASYNC_RST));

    /* Temporarily turn the TV DAC off */
    OUTREG(RADEON_TV_DAC_CNTL, ((restore->tv_dac_cntl & ~RADEON_TV_DAC_NBLANK)
				| RADEON_TV_DAC_BGSLEEP
				| RADEON_TV_DAC_RDACPD
				| RADEON_TV_DAC_GDACPD
				| RADEON_TV_DAC_BDACPD));

    ErrorF("Restore TV PLL\n");
    RADEONRestoreTVPLLRegisters(pScrn, restore);

    ErrorF("Restore TVHV\n");
    RADEONRestoreTVHVRegisters(pScrn, restore);

    OUTREG(RADEON_TV_MASTER_CNTL, (restore->tv_master_cntl
				   | RADEON_TV_ASYNC_RST
				   | RADEON_CRT_ASYNC_RST));

    ErrorF("Restore TV Restarts\n");
    RADEONRestoreTVRestarts(pScrn, restore);
  
    ErrorF("Restore Timing Tables\n");
    RADEONRestoreTVTimingTables(pScrn, restore);
  

    OUTREG(RADEON_TV_MASTER_CNTL, (restore->tv_master_cntl
				   | RADEON_TV_ASYNC_RST));

    ErrorF("Restore TV standard\n");
    RADEONRestoreTVOutputStd(pScrn, restore);

    OUTREG(RADEON_TV_MASTER_CNTL, restore->tv_master_cntl);

    OUTREG(RADEON_TV_GAIN_LIMIT_SETTINGS, restore->tv_gain_limit_settings);
    OUTREG(RADEON_TV_LINEAR_GAIN_SETTINGS, restore->tv_linear_gain_settings);

    OUTREG(RADEON_TV_DAC_CNTL, restore->tv_dac_cntl);

    ErrorF("Leaving Restore TV\n");
}

static void RADEONPLLWaitForReadUpdateComplete(ScrnInfoPtr pScrn)
{
    int i = 0;

    /* FIXME: Certain revisions of R300 can't recover here.  Not sure of
       the cause yet, but this workaround will mask the problem for now.
       Other chips usually will pass at the very first test, so the
       workaround shouldn't have any effect on them. */
    for (i = 0;
	 (i < 10000 &&
	  INPLL(pScrn, RADEON_PPLL_REF_DIV) & RADEON_PPLL_ATOMIC_UPDATE_R);
	 i++);
}

static void RADEONPLLWriteUpdate(ScrnInfoPtr pScrn)
{
    while (INPLL(pScrn, RADEON_PPLL_REF_DIV) & RADEON_PPLL_ATOMIC_UPDATE_R);

    OUTPLLP(pScrn, RADEON_PPLL_REF_DIV,
	    RADEON_PPLL_ATOMIC_UPDATE_W,
	    ~(RADEON_PPLL_ATOMIC_UPDATE_W));
}

static void RADEONPLL2WaitForReadUpdateComplete(ScrnInfoPtr pScrn)
{
    int i = 0;

    /* FIXME: Certain revisions of R300 can't recover here.  Not sure of
       the cause yet, but this workaround will mask the problem for now.
       Other chips usually will pass at the very first test, so the
       workaround shouldn't have any effect on them. */
    for (i = 0;
	 (i < 10000 &&
	  INPLL(pScrn, RADEON_P2PLL_REF_DIV) & RADEON_P2PLL_ATOMIC_UPDATE_R);
	 i++);
}

static void RADEONPLL2WriteUpdate(ScrnInfoPtr pScrn)
{
    while (INPLL(pScrn, RADEON_P2PLL_REF_DIV) & RADEON_P2PLL_ATOMIC_UPDATE_R);

    OUTPLLP(pScrn, RADEON_P2PLL_REF_DIV,
	    RADEON_P2PLL_ATOMIC_UPDATE_W,
	    ~(RADEON_P2PLL_ATOMIC_UPDATE_W));
}

static CARD8 RADEONComputePLLGain(CARD16 reference_freq, CARD16 ref_div,
				  CARD16 fb_div)
{
    unsigned vcoFreq;

    if (!ref_div)
	return 1;

    vcoFreq = ((unsigned)reference_freq * fb_div) / ref_div;

    /*
     * This is horribly crude: the VCO frequency range is divided into
     * 3 parts, each part having a fixed PLL gain value.
     */
    if (vcoFreq >= 30000)
	/*
	 * [300..max] MHz : 7
	 */
	return 7;
    else if (vcoFreq >= 18000)
	/*
	 * [180..300) MHz : 4
	 */
        return 4;
    else
	/*
	 * [0..180) MHz : 1
	 */
        return 1;
}

/* Wait for PLLs to lock */
static void RADEONWaitPLLLock(ScrnInfoPtr pScrn, unsigned nTests,
			      unsigned nWaitLoops, unsigned cntThreshold)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 savePLLTest;
    unsigned i;
    unsigned j;

    OUTREG(RADEON_TEST_DEBUG_MUX, (INREG(RADEON_TEST_DEBUG_MUX) & 0xffff60ff) | 0x100);

    savePLLTest = INPLL(pScrn, RADEON_PLL_TEST_CNTL);

    OUTPLL(pScrn, RADEON_PLL_TEST_CNTL, savePLLTest & ~RADEON_PLL_MASK_READ_B);

    /* XXX: these should probably be OUTPLL to avoid various PLL errata */

    OUTREG8(RADEON_CLOCK_CNTL_INDEX, RADEON_PLL_TEST_CNTL);

    for (i = 0; i < nTests; i++) {
	OUTREG8(RADEON_CLOCK_CNTL_DATA + 3, 0);
      
	for (j = 0; j < nWaitLoops; j++)
	    if (INREG8(RADEON_CLOCK_CNTL_DATA + 3) >= cntThreshold)
		break;
    }

    OUTPLL(pScrn, RADEON_PLL_TEST_CNTL, savePLLTest);

    OUTREG(RADEON_TEST_DEBUG_MUX, INREG(RADEON_TEST_DEBUG_MUX) & 0xffffe0ff);
}

/* Write PLL registers */
void RADEONRestorePLLRegisters(ScrnInfoPtr pScrn,
			       RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD8 pllGain;

    pllGain = RADEONComputePLLGain(info->pll.reference_freq,
				   restore->ppll_ref_div & RADEON_PPLL_REF_DIV_MASK,
				   restore->ppll_div_3 & RADEON_PPLL_FB3_DIV_MASK);

    if (info->IsMobility) {
        /* A temporal workaround for the occational blanking on certain laptop panels.
           This appears to related to the PLL divider registers (fail to lock?).
	   It occurs even when all dividers are the same with their old settings.
           In this case we really don't need to fiddle with PLL registers.
           By doing this we can avoid the blanking problem with some panels.
        */
        if ((restore->ppll_ref_div == (INPLL(pScrn, RADEON_PPLL_REF_DIV) & RADEON_PPLL_REF_DIV_MASK)) &&
	    (restore->ppll_div_3 == (INPLL(pScrn, RADEON_PPLL_DIV_3) & 
				     (RADEON_PPLL_POST3_DIV_MASK | RADEON_PPLL_FB3_DIV_MASK)))) {
	    OUTREGP(RADEON_CLOCK_CNTL_INDEX,
		    RADEON_PLL_DIV_SEL,
		    ~(RADEON_PLL_DIV_SEL));
	    RADEONPllErrataAfterIndex(info);
	    return;
	}
    }

    OUTPLLP(pScrn, RADEON_VCLK_ECP_CNTL,
	    RADEON_VCLK_SRC_SEL_CPUCLK,
	    ~(RADEON_VCLK_SRC_SEL_MASK));

    OUTPLLP(pScrn,
	    RADEON_PPLL_CNTL,
	    RADEON_PPLL_RESET
	    | RADEON_PPLL_ATOMIC_UPDATE_EN
	    | RADEON_PPLL_VGA_ATOMIC_UPDATE_EN
	    | ((CARD32)pllGain << RADEON_PPLL_PVG_SHIFT),
	    ~(RADEON_PPLL_RESET
	      | RADEON_PPLL_ATOMIC_UPDATE_EN
	      | RADEON_PPLL_VGA_ATOMIC_UPDATE_EN
	      | RADEON_PPLL_PVG_MASK));

    OUTREGP(RADEON_CLOCK_CNTL_INDEX,
	    RADEON_PLL_DIV_SEL,
	    ~(RADEON_PLL_DIV_SEL));
    RADEONPllErrataAfterIndex(info);

    if (IS_R300_VARIANT ||
	(info->ChipFamily == CHIP_FAMILY_RS300) ||
	(info->ChipFamily == CHIP_FAMILY_RS400)) {
	if (restore->ppll_ref_div & R300_PPLL_REF_DIV_ACC_MASK) {
	    /* When restoring console mode, use saved PPLL_REF_DIV
	     * setting.
	     */
	    OUTPLLP(pScrn, RADEON_PPLL_REF_DIV,
		    restore->ppll_ref_div,
		    0);
	} else {
	    /* R300 uses ref_div_acc field as real ref divider */
	    OUTPLLP(pScrn, RADEON_PPLL_REF_DIV,
		    (restore->ppll_ref_div << R300_PPLL_REF_DIV_ACC_SHIFT),
		    ~R300_PPLL_REF_DIV_ACC_MASK);
	}
    } else {
	OUTPLLP(pScrn, RADEON_PPLL_REF_DIV,
		restore->ppll_ref_div,
		~RADEON_PPLL_REF_DIV_MASK);
    }

    OUTPLLP(pScrn, RADEON_PPLL_DIV_3,
	    restore->ppll_div_3,
	    ~RADEON_PPLL_FB3_DIV_MASK);

    OUTPLLP(pScrn, RADEON_PPLL_DIV_3,
	    restore->ppll_div_3,
	    ~RADEON_PPLL_POST3_DIV_MASK);

    RADEONPLLWriteUpdate(pScrn);
    RADEONPLLWaitForReadUpdateComplete(pScrn);

    OUTPLL(pScrn, RADEON_HTOTAL_CNTL, restore->htotal_cntl);

    OUTPLLP(pScrn, RADEON_PPLL_CNTL,
	    0,
	    ~(RADEON_PPLL_RESET
	      | RADEON_PPLL_SLEEP
	      | RADEON_PPLL_ATOMIC_UPDATE_EN
	      | RADEON_PPLL_VGA_ATOMIC_UPDATE_EN));

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Wrote: 0x%08x 0x%08x 0x%08lx (0x%08x)\n",
		   restore->ppll_ref_div,
		   restore->ppll_div_3,
		   restore->htotal_cntl,
		   INPLL(pScrn, RADEON_PPLL_CNTL));
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Wrote: rd=%d, fd=%d, pd=%d\n",
		   restore->ppll_ref_div & RADEON_PPLL_REF_DIV_MASK,
		   restore->ppll_div_3 & RADEON_PPLL_FB3_DIV_MASK,
		   (restore->ppll_div_3 & RADEON_PPLL_POST3_DIV_MASK) >> 16);

    usleep(50000); /* Let the clock to lock */

    /* OUTPLLP(pScrn, RADEON_VCLK_ECP_CNTL,
	    RADEON_VCLK_SRC_SEL_PPLLCLK,
	    ~(RADEON_VCLK_SRC_SEL_MASK));*/
    OUTPLL(pScrn, RADEON_VCLK_ECP_CNTL, restore->vclk_ecp_cntl);

    ErrorF("finished PLL1\n");

}


/* Write PLL2 registers */
void RADEONRestorePLL2Registers(ScrnInfoPtr pScrn,
				RADEONSavePtr restore)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    CARD8 pllGain;

    pllGain = RADEONComputePLLGain(info->pll.reference_freq,
                                   restore->p2pll_ref_div & RADEON_P2PLL_REF_DIV_MASK,
                                   restore->p2pll_div_0 & RADEON_P2PLL_FB0_DIV_MASK);


    OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL,
	    RADEON_PIX2CLK_SRC_SEL_CPUCLK,
	    ~(RADEON_PIX2CLK_SRC_SEL_MASK));

    OUTPLLP(pScrn,
	    RADEON_P2PLL_CNTL,
	    RADEON_P2PLL_RESET
	    | RADEON_P2PLL_ATOMIC_UPDATE_EN
	    | ((CARD32)pllGain << RADEON_P2PLL_PVG_SHIFT),
	    ~(RADEON_P2PLL_RESET
	      | RADEON_P2PLL_ATOMIC_UPDATE_EN
	      | RADEON_P2PLL_PVG_MASK));


    OUTPLLP(pScrn, RADEON_P2PLL_REF_DIV,
	    restore->p2pll_ref_div,
	    ~RADEON_P2PLL_REF_DIV_MASK);

    OUTPLLP(pScrn, RADEON_P2PLL_DIV_0,
	    restore->p2pll_div_0,
	    ~RADEON_P2PLL_FB0_DIV_MASK);

    OUTPLLP(pScrn, RADEON_P2PLL_DIV_0,
	    restore->p2pll_div_0,
	    ~RADEON_P2PLL_POST0_DIV_MASK);

    RADEONPLL2WriteUpdate(pScrn);
    RADEONPLL2WaitForReadUpdateComplete(pScrn);

    OUTPLL(pScrn, RADEON_HTOTAL2_CNTL, restore->htotal_cntl2);

    OUTPLLP(pScrn, RADEON_P2PLL_CNTL,
	    0,
	    ~(RADEON_P2PLL_RESET
	      | RADEON_P2PLL_SLEEP
	      | RADEON_P2PLL_ATOMIC_UPDATE_EN));

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Wrote2: 0x%08lx 0x%08lx 0x%08lx (0x%08x)\n",
		   restore->p2pll_ref_div,
		   restore->p2pll_div_0,
		   restore->htotal_cntl2,
		   INPLL(pScrn, RADEON_P2PLL_CNTL));
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Wrote2: rd=%ld, fd=%ld, pd=%ld\n",
		   restore->p2pll_ref_div & RADEON_P2PLL_REF_DIV_MASK,
		   restore->p2pll_div_0 & RADEON_P2PLL_FB0_DIV_MASK,
		   (restore->p2pll_div_0 & RADEON_P2PLL_POST0_DIV_MASK) >>16);

    usleep(5000); /* Let the clock to lock */

    /*OUTPLLP(pScrn, RADEON_PIXCLKS_CNTL,
	    RADEON_PIX2CLK_SRC_SEL_P2PLLCLK,
	    ~(RADEON_PIX2CLK_SRC_SEL_MASK));*/
    OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, restore->pixclks_cntl);

    ErrorF("finished PLL2\n");

}


/* restore original surface info (for fb console). */
static void RADEONRestoreSurfaces(ScrnInfoPtr pScrn, RADEONSavePtr restore)
{
    RADEONInfoPtr      info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned int surfnr;
    
    for ( surfnr = 0; surfnr < 8; surfnr++ ) {
	OUTREG(RADEON_SURFACE0_INFO + 16 * surfnr, restore->surfaces[surfnr][0]);
	OUTREG(RADEON_SURFACE0_LOWER_BOUND + 16 * surfnr, restore->surfaces[surfnr][1]);
	OUTREG(RADEON_SURFACE0_UPPER_BOUND + 16 * surfnr, restore->surfaces[surfnr][2]);
    }
}

/* save original surface info (for fb console). */
static void RADEONSaveSurfaces(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr      info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    unsigned int surfnr;
    
    for ( surfnr = 0; surfnr < 8; surfnr++ ) {
	save->surfaces[surfnr][0] = INREG(RADEON_SURFACE0_INFO + 16 * surfnr);
	save->surfaces[surfnr][1] = INREG(RADEON_SURFACE0_LOWER_BOUND + 16 * surfnr);
	save->surfaces[surfnr][2] = INREG(RADEON_SURFACE0_UPPER_BOUND + 16 * surfnr);
    }
}

void RADEONChangeSurfaces(ScrnInfoPtr pScrn)
{
   /* the idea here is to only set up front buffer as tiled, and back/depth buffer when needed.
      Everything else is left as untiled. This means we need to use eplicit src/dst pitch control
      when blitting, based on the src/target address, and can no longer use a default offset.
      But OTOH we don't need to dynamically change surfaces (for xv for instance), and some
      ugly offset / fb reservation (cursor) is gone. And as a bonus, everything actually works...
      For simplicity, just always update everything (just let the ioctl fail - could do better).
      All surface addresses are relative to RADEON_MC_FB_LOCATION */
  
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    int cpp = info->CurrentLayout.pixel_bytes;
    /* depth/front/back pitch must be identical (and the same as displayWidth) */
    int width_bytes = pScrn->displayWidth * cpp;
    int bufferSize = ((((pScrn->virtualY + 15) & ~15) * width_bytes
        + RADEON_BUFFER_ALIGN) & ~RADEON_BUFFER_ALIGN);
    unsigned int color_pattern, swap_pattern;

    if (!info->allowColorTiling)
	return;

    swap_pattern = 0;
#if X_BYTE_ORDER == X_BIG_ENDIAN
    switch (pScrn->bitsPerPixel) {
    case 16:
	swap_pattern = RADEON_SURF_AP0_SWP_16BPP | RADEON_SURF_AP1_SWP_16BPP;
	break;

    case 32:
	swap_pattern = RADEON_SURF_AP0_SWP_32BPP | RADEON_SURF_AP1_SWP_32BPP;
	break;
    }
#endif
    if (info->ChipFamily < CHIP_FAMILY_R200) {
	color_pattern = RADEON_SURF_TILE_COLOR_MACRO;
    } else if (IS_R300_VARIANT) {
       color_pattern = R300_SURF_TILE_COLOR_MACRO;
    } else {
	color_pattern = R200_SURF_TILE_COLOR_MACRO;
    }   
#ifdef XF86DRI
    if (info->directRenderingInited) {
	drmRadeonSurfaceFree drmsurffree;
	drmRadeonSurfaceAlloc drmsurfalloc;
	int retvalue;
	int depthCpp = (info->depthBits - 8) / 4;
	int depth_width_bytes = pScrn->displayWidth * depthCpp;
	int depthBufferSize = ((((pScrn->virtualY + 15) & ~15) * depth_width_bytes
				+ RADEON_BUFFER_ALIGN) & ~RADEON_BUFFER_ALIGN);
	unsigned int depth_pattern;

	drmsurffree.address = info->frontOffset;
	retvalue = drmCommandWrite(info->drmFD, DRM_RADEON_SURF_FREE,
	    &drmsurffree, sizeof(drmsurffree));

	if ((info->ChipFamily != CHIP_FAMILY_RV100) || 
	    (info->ChipFamily != CHIP_FAMILY_RS100) ||
	    (info->ChipFamily != CHIP_FAMILY_RS200)) {
	    drmsurffree.address = info->depthOffset;
	    retvalue = drmCommandWrite(info->drmFD, DRM_RADEON_SURF_FREE,
		&drmsurffree, sizeof(drmsurffree));
	}

	if (!info->noBackBuffer) {
	    drmsurffree.address = info->backOffset;
	    retvalue = drmCommandWrite(info->drmFD, DRM_RADEON_SURF_FREE,
		&drmsurffree, sizeof(drmsurffree));
	}

	drmsurfalloc.size = bufferSize;
	drmsurfalloc.address = info->frontOffset;
	drmsurfalloc.flags = swap_pattern;

	if (info->tilingEnabled) {
	    if (IS_R300_VARIANT)
		drmsurfalloc.flags |= (width_bytes / 8) | color_pattern;
	    else
		drmsurfalloc.flags |= (width_bytes / 16) | color_pattern;
	}
	retvalue = drmCommandWrite(info->drmFD, DRM_RADEON_SURF_ALLOC,
				   &drmsurfalloc, sizeof(drmsurfalloc));
	if (retvalue < 0)
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "drm: could not allocate surface for front buffer!\n");
	
	if ((info->have3DWindows) && (!info->noBackBuffer)) {
	    drmsurfalloc.address = info->backOffset;
	    retvalue = drmCommandWrite(info->drmFD, DRM_RADEON_SURF_ALLOC,
				       &drmsurfalloc, sizeof(drmsurfalloc));
	    if (retvalue < 0)
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "drm: could not allocate surface for back buffer!\n");
	}

	if (info->ChipFamily < CHIP_FAMILY_R200) {
	    if (depthCpp == 2)
		depth_pattern = RADEON_SURF_TILE_DEPTH_16BPP;
	    else
		depth_pattern = RADEON_SURF_TILE_DEPTH_32BPP;
	} else if (IS_R300_VARIANT) {
	    if (depthCpp == 2)
		depth_pattern = R300_SURF_TILE_COLOR_MACRO;
	    else
		depth_pattern = R300_SURF_TILE_COLOR_MACRO | R300_SURF_TILE_DEPTH_32BPP;
	} else {
	    if (depthCpp == 2)
		depth_pattern = R200_SURF_TILE_DEPTH_16BPP;
	    else
		depth_pattern = R200_SURF_TILE_DEPTH_32BPP;
	}

	/* rv100 and probably the derivative igps don't have depth tiling on all the time? */
	if (info->have3DWindows && ((info->ChipFamily != CHIP_FAMILY_RV100) || 
	    (info->ChipFamily != CHIP_FAMILY_RS100) ||
	    (info->ChipFamily != CHIP_FAMILY_RS200))) {
	    drmRadeonSurfaceAlloc drmsurfalloc;
	    drmsurfalloc.size = depthBufferSize;
	    drmsurfalloc.address = info->depthOffset;
            if (IS_R300_VARIANT)
                drmsurfalloc.flags = swap_pattern | (depth_width_bytes / 8) | depth_pattern;
            else
                drmsurfalloc.flags = swap_pattern | (depth_width_bytes / 16) | depth_pattern;
	    retvalue = drmCommandWrite(info->drmFD, DRM_RADEON_SURF_ALLOC,
		&drmsurfalloc, sizeof(drmsurfalloc));
	    if (retvalue < 0)
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "drm: could not allocate surface for depth buffer!\n");
	}
    }
    else
#endif
    {
	unsigned int surf_info = swap_pattern;
	unsigned char *RADEONMMIO = info->MMIO;
	/* we don't need anything like WaitForFifo, no? */
	if (info->tilingEnabled) {
	    if (IS_R300_VARIANT)
		surf_info |= (width_bytes / 8) | color_pattern;
	    else
		surf_info |= (width_bytes / 16) | color_pattern;
	}
	OUTREG(RADEON_SURFACE0_INFO, surf_info);
	OUTREG(RADEON_SURFACE0_LOWER_BOUND, 0);
	OUTREG(RADEON_SURFACE0_UPPER_BOUND, bufferSize - 1);
/*	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		"surface0 set to %x, LB 0x%x UB 0x%x\n",
		surf_info, 0, bufferSize - 1024);*/
    }

    /* Update surface images */
    RADEONSaveSurfaces(pScrn, &info->ModeReg);
}

/* Read memory map */
static void RADEONSaveMemMapRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    save->mc_fb_location     = INREG(RADEON_MC_FB_LOCATION);
    save->mc_agp_location    = INREG(RADEON_MC_AGP_LOCATION);
    save->display_base_addr  = INREG(RADEON_DISPLAY_BASE_ADDR);
    save->display2_base_addr = INREG(RADEON_DISPLAY2_BASE_ADDR);
    save->ov0_base_addr      = INREG(RADEON_OV0_BASE_ADDR);
}

/* Read common registers */
static void RADEONSaveCommonRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    save->ovr_clr            = INREG(RADEON_OVR_CLR);
    save->ovr_wid_left_right = INREG(RADEON_OVR_WID_LEFT_RIGHT);
    save->ovr_wid_top_bottom = INREG(RADEON_OVR_WID_TOP_BOTTOM);
    save->ov0_scale_cntl     = INREG(RADEON_OV0_SCALE_CNTL);
    save->subpic_cntl        = INREG(RADEON_SUBPIC_CNTL);
    save->viph_control       = INREG(RADEON_VIPH_CONTROL);
    save->i2c_cntl_1         = INREG(RADEON_I2C_CNTL_1);
    save->gen_int_cntl       = INREG(RADEON_GEN_INT_CNTL);
    save->cap0_trig_cntl     = INREG(RADEON_CAP0_TRIG_CNTL);
    save->cap1_trig_cntl     = INREG(RADEON_CAP1_TRIG_CNTL);
    save->bus_cntl           = INREG(RADEON_BUS_CNTL);
    save->surface_cntl	     = INREG(RADEON_SURFACE_CNTL);
    save->grph_buffer_cntl   = INREG(RADEON_GRPH_BUFFER_CNTL);
    save->grph2_buffer_cntl  = INREG(RADEON_GRPH2_BUFFER_CNTL);
}

/* Read miscellaneous registers which might be destroyed by an fbdevHW call */
static void RADEONSaveFBDevRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
#ifdef XF86DRI
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    /* Save register for vertical blank interrupts */
    if (info->irq) {
	save->gen_int_cntl = INREG(RADEON_GEN_INT_CNTL);
    }

    /* Save registers for page flipping */
    if (info->allowPageFlip) {
	save->crtc_offset_cntl = INREG(RADEON_CRTC_OFFSET_CNTL);
	if (pRADEONEnt->HasCRTC2) {
	    save->crtc2_offset_cntl = INREG(RADEON_CRTC2_OFFSET_CNTL);
	}
    }
#endif
}

/* Read CRTC registers */
static void RADEONSaveCrtcRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    save->crtc_gen_cntl        = INREG(RADEON_CRTC_GEN_CNTL);
    save->crtc_ext_cntl        = INREG(RADEON_CRTC_EXT_CNTL);
    save->crtc_h_total_disp    = INREG(RADEON_CRTC_H_TOTAL_DISP);
    save->crtc_h_sync_strt_wid = INREG(RADEON_CRTC_H_SYNC_STRT_WID);
    save->crtc_v_total_disp    = INREG(RADEON_CRTC_V_TOTAL_DISP);
    save->crtc_v_sync_strt_wid = INREG(RADEON_CRTC_V_SYNC_STRT_WID);

    save->fp_h_sync_strt_wid   = INREG(RADEON_FP_H_SYNC_STRT_WID);
    save->fp_v_sync_strt_wid   = INREG(RADEON_FP_V_SYNC_STRT_WID);
    save->fp_crtc_h_total_disp = INREG(RADEON_FP_CRTC_H_TOTAL_DISP);
    save->fp_crtc_v_total_disp = INREG(RADEON_FP_CRTC_V_TOTAL_DISP);

    save->crtc_offset          = INREG(RADEON_CRTC_OFFSET);
    save->crtc_offset_cntl     = INREG(RADEON_CRTC_OFFSET_CNTL);
    save->crtc_pitch           = INREG(RADEON_CRTC_PITCH);
    save->disp_merge_cntl      = INREG(RADEON_DISP_MERGE_CNTL);
    save->crtc_more_cntl       = INREG(RADEON_CRTC_MORE_CNTL);

    if (IS_R300_VARIANT)
	save->crtc_tile_x0_y0 =  INREG(R300_CRTC_TILE_X0_Y0);

    if (info->IsDellServer) {
	save->tv_dac_cntl      = INREG(RADEON_TV_DAC_CNTL);
	save->dac2_cntl        = INREG(RADEON_DAC_CNTL2);
	save->disp_hw_debug    = INREG (RADEON_DISP_HW_DEBUG);
	save->crtc2_gen_cntl   = INREG(RADEON_CRTC2_GEN_CNTL);
    }

    /* track if the crtc is enabled for text restore */
    if (save->crtc_ext_cntl & RADEON_CRTC_DISPLAY_DIS)
	info->crtc_on = FALSE;
    else
	info->crtc_on = TRUE;

}

/* Read DAC registers */
static void RADEONSaveDACRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    save->dac_cntl              = INREG(RADEON_DAC_CNTL);
    save->dac2_cntl             = INREG(RADEON_DAC_CNTL2);
    save->tv_dac_cntl           = INREG(RADEON_TV_DAC_CNTL);
    save->disp_output_cntl      = INREG(RADEON_DISP_OUTPUT_CNTL);
    save->disp_tv_out_cntl      = INREG(RADEON_DISP_TV_OUT_CNTL);
    save->disp_hw_debug         = INREG(RADEON_DISP_HW_DEBUG);
    save->dac_macro_cntl        = INREG(RADEON_DAC_MACRO_CNTL);
    save->gpiopad_a             = INREG(RADEON_GPIOPAD_A);

}

/* Read flat panel registers */
static void RADEONSaveFPRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    save->fp_gen_cntl          = INREG(RADEON_FP_GEN_CNTL);
    save->fp2_gen_cntl          = INREG (RADEON_FP2_GEN_CNTL);
    save->fp_horz_stretch      = INREG(RADEON_FP_HORZ_STRETCH);
    save->fp_vert_stretch      = INREG(RADEON_FP_VERT_STRETCH);
    save->lvds_gen_cntl        = INREG(RADEON_LVDS_GEN_CNTL);
    save->lvds_pll_cntl        = INREG(RADEON_LVDS_PLL_CNTL);
    save->tmds_pll_cntl        = INREG(RADEON_TMDS_PLL_CNTL);
    save->tmds_transmitter_cntl= INREG(RADEON_TMDS_TRANSMITTER_CNTL);
    save->bios_4_scratch       = INREG(RADEON_BIOS_4_SCRATCH);
    save->bios_5_scratch       = INREG(RADEON_BIOS_5_SCRATCH);
    save->bios_6_scratch       = INREG(RADEON_BIOS_6_SCRATCH);

    if (info->ChipFamily == CHIP_FAMILY_RV280) {
	/* bit 22 of TMDS_PLL_CNTL is read-back inverted */
	save->tmds_pll_cntl ^= (1 << 22);
    }
}

/* Read CRTC2 registers */
static void RADEONSaveCrtc2Registers(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    save->crtc2_gen_cntl        = INREG(RADEON_CRTC2_GEN_CNTL);
    save->crtc2_h_total_disp    = INREG(RADEON_CRTC2_H_TOTAL_DISP);
    save->crtc2_h_sync_strt_wid = INREG(RADEON_CRTC2_H_SYNC_STRT_WID);
    save->crtc2_v_total_disp    = INREG(RADEON_CRTC2_V_TOTAL_DISP);
    save->crtc2_v_sync_strt_wid = INREG(RADEON_CRTC2_V_SYNC_STRT_WID);
    save->crtc2_offset          = INREG(RADEON_CRTC2_OFFSET);
    save->crtc2_offset_cntl     = INREG(RADEON_CRTC2_OFFSET_CNTL);
    save->crtc2_pitch           = INREG(RADEON_CRTC2_PITCH);

    if (IS_R300_VARIANT)
	save->crtc2_tile_x0_y0 =  INREG(R300_CRTC2_TILE_X0_Y0);

    save->fp_h2_sync_strt_wid   = INREG (RADEON_FP_H2_SYNC_STRT_WID);
    save->fp_v2_sync_strt_wid   = INREG (RADEON_FP_V2_SYNC_STRT_WID);

    if (info->ChipFamily == CHIP_FAMILY_RS400) {
	save->rs480_unk_e30 = INREG(RADEON_RS480_UNK_e30);
	save->rs480_unk_e34 = INREG(RADEON_RS480_UNK_e34);
	save->rs480_unk_e38 = INREG(RADEON_RS480_UNK_e38);
	save->rs480_unk_e3c = INREG(RADEON_RS480_UNK_e3c);
    }
    
    save->disp2_merge_cntl      = INREG(RADEON_DISP2_MERGE_CNTL);

    /* track if the crtc is enabled for text restore */
    if (save->crtc2_gen_cntl & RADEON_CRTC2_DISP_DIS)
	info->crtc2_on = FALSE;
    else
	info->crtc2_on = TRUE;

}

/* Save horizontal/vertical timing code tables */
static void RADEONSaveTVTimingTables(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD16 hTable;
    CARD16 vTable;
    CARD32 tmp;
    unsigned i;

    save->tv_uv_adr = INREG(RADEON_TV_UV_ADR);
    hTable = RADEONGetHTimingTablesAddr(save->tv_uv_adr);
    vTable = RADEONGetVTimingTablesAddr(save->tv_uv_adr);

    /*
     * Reset FIFO arbiter in order to be able to access FIFO RAM
     */

    OUTREG(RADEON_TV_MASTER_CNTL, (RADEON_TV_ASYNC_RST
				   | RADEON_CRT_ASYNC_RST
				   | RADEON_RESTART_PHASE_FIX
				   | RADEON_CRT_FIFO_CE_EN
				   | RADEON_TV_FIFO_CE_EN
				   | RADEON_TV_ON));

    /*OUTREG(RADEON_TV_MASTER_CNTL, save->tv_master_cntl | RADEON_TV_ON);*/

    ErrorF("saveTimingTables: reading timing tables\n");

    for (i = 0; i < MAX_H_CODE_TIMING_LEN; i += 2) {
	tmp = RADEONReadTVFIFO(pScrn, hTable--);
	save->h_code_timing[ i     ] = (CARD16)((tmp >> 14) & 0x3fff);
	save->h_code_timing[ i + 1 ] = (CARD16)(tmp & 0x3fff);

	if (save->h_code_timing[ i ] == 0 || save->h_code_timing[ i + 1 ] == 0)
	    break;
    }

    for (i = 0; i < MAX_V_CODE_TIMING_LEN; i += 2) {
	tmp = RADEONReadTVFIFO(pScrn, vTable++);
	save->v_code_timing[ i     ] = (CARD16)(tmp & 0x3fff);
	save->v_code_timing[ i + 1 ] = (CARD16)((tmp >> 14) & 0x3fff);

	if (save->v_code_timing[ i ] == 0 || save->v_code_timing[ i + 1 ] == 0)
	    break;
    }
}

/* read TV regs */
static void RADEONSaveTVRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;

    ErrorF("Entering TV Save\n");

    save->tv_crc_cntl = INREG(RADEON_TV_CRC_CNTL);
    save->tv_frestart = INREG(RADEON_TV_FRESTART);
    save->tv_hrestart = INREG(RADEON_TV_HRESTART);
    save->tv_vrestart = INREG(RADEON_TV_VRESTART);
    save->tv_gain_limit_settings = INREG(RADEON_TV_GAIN_LIMIT_SETTINGS);
    save->tv_hdisp = INREG(RADEON_TV_HDISP);
    save->tv_hstart = INREG(RADEON_TV_HSTART);
    save->tv_htotal = INREG(RADEON_TV_HTOTAL);
    save->tv_linear_gain_settings = INREG(RADEON_TV_LINEAR_GAIN_SETTINGS);
    save->tv_master_cntl = INREG(RADEON_TV_MASTER_CNTL);
    save->tv_rgb_cntl = INREG(RADEON_TV_RGB_CNTL);
    save->tv_modulator_cntl1 = INREG(RADEON_TV_MODULATOR_CNTL1);
    save->tv_modulator_cntl2 = INREG(RADEON_TV_MODULATOR_CNTL2);
    save->tv_pre_dac_mux_cntl = INREG(RADEON_TV_PRE_DAC_MUX_CNTL);
    save->tv_sync_cntl = INREG(RADEON_TV_SYNC_CNTL);
    save->tv_timing_cntl = INREG(RADEON_TV_TIMING_CNTL);
    save->tv_dac_cntl = INREG(RADEON_TV_DAC_CNTL);
    save->tv_upsamp_and_gain_cntl = INREG(RADEON_TV_UPSAMP_AND_GAIN_CNTL);
    save->tv_vdisp = INREG(RADEON_TV_VDISP);
    save->tv_ftotal = INREG(RADEON_TV_FTOTAL);
    save->tv_vscaler_cntl1 = INREG(RADEON_TV_VSCALER_CNTL1);
    save->tv_vscaler_cntl2 = INREG(RADEON_TV_VSCALER_CNTL2);
    save->tv_vtotal = INREG(RADEON_TV_VTOTAL);
    save->tv_y_fall_cntl = INREG(RADEON_TV_Y_FALL_CNTL);
    save->tv_y_rise_cntl = INREG(RADEON_TV_Y_RISE_CNTL);
    save->tv_y_saw_tooth_cntl = INREG(RADEON_TV_Y_SAW_TOOTH_CNTL);

    save->tv_pll_cntl = INPLL(pScrn, RADEON_TV_PLL_CNTL);
    save->tv_pll_cntl1 = INPLL(pScrn, RADEON_TV_PLL_CNTL1);

    ErrorF("Save TV timing tables\n");

    RADEONSaveTVTimingTables(pScrn, save);

    ErrorF("TV Save done\n");
}

/* Read PLL registers */
static void RADEONSavePLLRegisters(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    save->ppll_ref_div = INPLL(pScrn, RADEON_PPLL_REF_DIV);
    save->ppll_div_3   = INPLL(pScrn, RADEON_PPLL_DIV_3);
    save->htotal_cntl  = INPLL(pScrn, RADEON_HTOTAL_CNTL);
    save->vclk_ecp_cntl = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Read: 0x%08x 0x%08x 0x%08lx\n",
		   save->ppll_ref_div,
		   save->ppll_div_3,
		   save->htotal_cntl);
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Read: rd=%d, fd=%d, pd=%d\n",
		   save->ppll_ref_div & RADEON_PPLL_REF_DIV_MASK,
		   save->ppll_div_3 & RADEON_PPLL_FB3_DIV_MASK,
		   (save->ppll_div_3 & RADEON_PPLL_POST3_DIV_MASK) >> 16);
}

/* Read PLL registers */
static void RADEONSavePLL2Registers(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    save->p2pll_ref_div = INPLL(pScrn, RADEON_P2PLL_REF_DIV);
    save->p2pll_div_0   = INPLL(pScrn, RADEON_P2PLL_DIV_0);
    save->htotal_cntl2  = INPLL(pScrn, RADEON_HTOTAL2_CNTL);
    save->pixclks_cntl  = INPLL(pScrn, RADEON_PIXCLKS_CNTL);

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Read: 0x%08lx 0x%08lx 0x%08lx\n",
		   save->p2pll_ref_div,
		   save->p2pll_div_0,
		   save->htotal_cntl2);
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Read: rd=%ld, fd=%ld, pd=%ld\n",
		   save->p2pll_ref_div & RADEON_P2PLL_REF_DIV_MASK,
		   save->p2pll_div_0 & RADEON_P2PLL_FB0_DIV_MASK,
		   (save->p2pll_div_0 & RADEON_P2PLL_POST0_DIV_MASK) >> 16);
}

/* Read palette data */
static void RADEONSavePalette(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int            i;

#ifdef ENABLE_FLAT_PANEL
    /* Select palette 0 (main CRTC) if using FP-enabled chip */
 /* if (info->Port1 == MT_DFP) PAL_SELECT(1); */
#endif
    PAL_SELECT(1);
    INPAL_START(0);
    for (i = 0; i < 256; i++) save->palette2[i] = INPAL_NEXT();
    PAL_SELECT(0);
    INPAL_START(0);
    for (i = 0; i < 256; i++) save->palette[i] = INPAL_NEXT();
    save->palette_valid = TRUE;
}

/* Save state that defines current video mode */
static void RADEONSaveMode(ScrnInfoPtr pScrn, RADEONSavePtr save)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONSaveMode(%p)\n", save);

    RADEONSaveMemMapRegisters(pScrn, save);
    RADEONSaveCommonRegisters(pScrn, save);
    RADEONSavePLLRegisters(pScrn, save);
    RADEONSaveCrtcRegisters(pScrn, save);
    RADEONSaveFPRegisters(pScrn, save);
    RADEONSaveDACRegisters(pScrn, save);
    RADEONSaveCrtc2Registers(pScrn, save);
    RADEONSavePLL2Registers(pScrn, save);
    if (info->InternalTVOut)
	RADEONSaveTVRegisters(pScrn, save);
    /*RADEONSavePalette(pScrn, save);*/

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONSaveMode returns %p\n", save);
}

/* Save everything needed to restore the original VC state */
static void RADEONSave(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONSavePtr  save       = &info->SavedReg;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONSave\n");

    if (info->FBDev) {
	RADEONSaveMemMapRegisters(pScrn, save);
	fbdevHWSave(pScrn);
	return;
    }


#ifdef WITH_VGAHW
    if (info->VGAAccess) {
	vgaHWPtr hwp = VGAHWPTR(pScrn);

	vgaHWUnlock(hwp);
# if defined(__powerpc__)
	/* temporary hack to prevent crashing on PowerMacs when trying to
	 * read VGA fonts and colormap, will find a better solution
	 * in the future. TODO: Check if there's actually some VGA stuff
	 * setup in the card at all !!
	 */
	vgaHWSave(pScrn, &hwp->SavedReg, VGA_SR_MODE); /* Save mode only */
# else
	/* Save mode * & fonts & cmap */
	vgaHWSave(pScrn, &hwp->SavedReg, VGA_SR_MODE | VGA_SR_FONTS);
# endif
	vgaHWLock(hwp);
    }
#endif
    save->dp_datatype      = INREG(RADEON_DP_DATATYPE);
    save->rbbm_soft_reset  = INREG(RADEON_RBBM_SOFT_RESET);
    save->clock_cntl_index = INREG(RADEON_CLOCK_CNTL_INDEX);
    RADEONPllErrataAfterIndex(info);

    RADEONSaveMode(pScrn, save);
    RADEONSaveSurfaces(pScrn, save);
}

/* Restore the original (text) mode */
void RADEONRestore(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONSavePtr  restore    = &info->SavedReg;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    xf86CrtcPtr crtc;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONRestore\n");

#if X_BYTE_ORDER == X_BIG_ENDIAN
    RADEONWaitForFifo(pScrn, 1);
    OUTREG(RADEON_RBBM_GUICNTL, RADEON_HOST_DATA_SWAP_NONE);
#endif

    if (info->FBDev) {
	fbdevHWRestore(pScrn);
	return;
    }
    RADEONBlank(pScrn);

    OUTREG(RADEON_CLOCK_CNTL_INDEX, restore->clock_cntl_index);
    RADEONPllErrataAfterIndex(info);
    OUTREG(RADEON_RBBM_SOFT_RESET,  restore->rbbm_soft_reset);
    OUTREG(RADEON_DP_DATATYPE,      restore->dp_datatype);
    OUTREG(RADEON_GRPH_BUFFER_CNTL, restore->grph_buffer_cntl);
    OUTREG(RADEON_GRPH2_BUFFER_CNTL, restore->grph2_buffer_cntl);

#if 0
    /* M6 card has trouble restoring text mode for its CRT.
     * This is fixed elsewhere and will be removed in the future.
     */
    if ((xf86IsEntityShared(info->pEnt->index) || info->MergedFB)
	&& info->IsM6)
	OUTREG(RADEON_DAC_CNTL2, restore->dac2_cntl);
#endif

    RADEONRestoreMemMapRegisters(pScrn, restore);
    RADEONRestoreCommonRegisters(pScrn, restore);

    if (pRADEONEnt->HasCRTC2) {
	RADEONRestoreCrtc2Registers(pScrn, restore);
	RADEONRestorePLL2Registers(pScrn, restore);
    }

    RADEONRestoreCrtcRegisters(pScrn, restore);
    RADEONRestorePLLRegisters(pScrn, restore);
    RADEONRestoreRMXRegisters(pScrn, restore);
    RADEONRestoreFPRegisters(pScrn, restore);
    RADEONRestoreFP2Registers(pScrn, restore);
    RADEONRestoreLVDSRegisters(pScrn, restore);

    if (info->InternalTVOut)
	RADEONRestoreTVRegisters(pScrn, restore);

    RADEONRestoreSurfaces(pScrn, restore);

#if 1
    /* Temp fix to "solve" VT switch problems.  When switching VTs on
     * some systems, the console can either hang or the fonts can be
     * corrupted.  This hack solves the problem 99% of the time.  A
     * correct fix is being worked on.
     */
    usleep(100000);
#endif

#ifdef WITH_VGAHW
    if (info->VGAAccess) {
       vgaHWPtr hwp = VGAHWPTR(pScrn);
       vgaHWUnlock(hwp);
# if defined(__powerpc__)
       /* Temporary hack to prevent crashing on PowerMacs when trying to
	* write VGA fonts, will find a better solution in the future
	*/
       vgaHWRestore(pScrn, &hwp->SavedReg, VGA_SR_MODE );
# else
       vgaHWRestore(pScrn, &hwp->SavedReg, VGA_SR_MODE | VGA_SR_FONTS );
# endif
       vgaHWLock(hwp);
    }
#endif

    /* need to make sure we don't enable a crtc by accident or we may get a hang */
    if (info->crtc2_on) {
	crtc = xf86_config->crtc[1];
	crtc->funcs->dpms(crtc, DPMSModeOn);
    }
    if (info->crtc_on) {
	crtc = xf86_config->crtc[0];
	crtc->funcs->dpms(crtc, DPMSModeOn);
    }
    /* to restore console mode, DAC registers should be set after every other registers are set,
     * otherwise,we may get blank screen 
     */
    RADEONRestoreDACRegisters(pScrn, restore);

#if 0
    RADEONWaitForVerticalSync(pScrn);
#endif
}

#if 0
/* Define initial palette for requested video mode.  This doesn't do
 * anything for XFree86 4.0.
 */
static void RADEONInitPalette(RADEONSavePtr save)
{
    save->palette_valid = FALSE;
}
#endif

static Bool RADEONSaveScreen(ScreenPtr pScreen, int mode)
{
    ScrnInfoPtr  pScrn = xf86Screens[pScreen->myNum];
    Bool         unblank;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONSaveScreen(%d)\n", mode);

    unblank = xf86IsUnblank(mode);
    if (unblank) SetTimeSinceLastInputEvent();

    if ((pScrn != NULL) && pScrn->vtSema) {
	if (unblank)
	    RADEONUnblank(pScrn);
	else
	    RADEONBlank(pScrn);
    }
    return TRUE;
}

static void
RADEONResetDPI(ScrnInfoPtr pScrn, Bool force)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];

    if(force					||
       (info->RADEONDPIVX != pScrn->virtualX)	||
       (info->RADEONDPIVY != pScrn->virtualY)
					  ) {

       pScreen->mmWidth = (pScrn->virtualX * 254 + pScrn->xDpi * 5) / (pScrn->xDpi * 10);
       pScreen->mmHeight = (pScrn->virtualY * 254 + pScrn->yDpi * 5) / (pScrn->yDpi * 10);

       info->RADEONDPIVX = pScrn->virtualX;
       info->RADEONDPIVY = pScrn->virtualY;

    }
}

Bool RADEONSwitchMode(int scrnIndex, DisplayModePtr mode, int flags)
{
    ScrnInfoPtr    pScrn       = xf86Screens[scrnIndex];
    RADEONInfoPtr  info        = RADEONPTR(pScrn);
    Bool           tilingOld   = info->tilingEnabled;
    Bool           ret;
#ifdef XF86DRI
    Bool           CPStarted   = info->CPStarted;

    if (CPStarted) {
	DRILock(pScrn->pScreen, 0);
	RADEONCP_STOP(pScrn, info);
    }
#endif

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONSwitchMode() !n");

    if (info->allowColorTiling) {
        info->tilingEnabled = (mode->Flags & (V_DBLSCAN | V_INTERLACE)) ? FALSE : TRUE;
#ifdef XF86DRI	
	if (info->directRenderingEnabled && (info->tilingEnabled != tilingOld)) {
	    RADEONSAREAPrivPtr pSAREAPriv;
	  if (RADEONDRISetParam(pScrn, RADEON_SETPARAM_SWITCH_TILING, (info->tilingEnabled ? 1 : 0)) < 0)
  	      xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			 "[drm] failed changing tiling status\n");
	    pSAREAPriv = DRIGetSAREAPrivate(pScrn->pScreen);
	    info->tilingEnabled = pSAREAPriv->tiling_enabled ? TRUE : FALSE;
	}
#endif
    }

    if (info->accelOn)
        RADEON_SYNC(info, pScrn);

    if (info->FBDev) {
	RADEONSaveFBDevRegisters(pScrn, &info->ModeReg);

	ret = fbdevHWSwitchMode(scrnIndex, mode, flags);
	pScrn->displayWidth = fbdevHWGetLineLength(pScrn)
		/ info->CurrentLayout.pixel_bytes;

	RADEONRestoreFBDevRegisters(pScrn, &info->ModeReg);
    } else {
	ret = xf86SetSingleMode (pScrn, mode, RR_Rotate_0);
    }

    if (info->tilingEnabled != tilingOld) {
	/* need to redraw front buffer, I guess this can be considered a hack ? */
	xf86EnableDisableFBAccess(scrnIndex, FALSE);
	RADEONChangeSurfaces(pScrn);
	xf86EnableDisableFBAccess(scrnIndex, TRUE);
	/* xf86SetRootClip would do, but can't access that here */
    }

    if (info->accelOn) {
        RADEON_SYNC(info, pScrn);
	RADEONEngineRestore(pScrn);
    }

#ifdef XF86DRI
    if (CPStarted) {
	RADEONCP_START(pScrn, info);
	DRIUnlock(pScrn->pScreen);
    }
#endif

    /* Since RandR (indirectly) uses SwitchMode(), we need to
     * update our Xinerama info here, too, in case of resizing
     */
    if(info->constantDPI) {
       RADEONResetDPI(pScrn, FALSE);
    }

    info->ecp_div = -1;

    return ret;
}

#ifdef X_XF86MiscPassMessage
Bool RADEONHandleMessage(int scrnIndex, const char* msgtype,
                                   const char* msgval, char** retmsg)
{
    ErrorF("RADEONHandleMessage(%d, \"%s\", \"%s\", retmsg)\n", scrnIndex,
		    msgtype, msgval);
    *retmsg = "";
    return 0;
}
#endif

/* Used to disallow modes that are not supported by the hardware */
ModeStatus RADEONValidMode(int scrnIndex, DisplayModePtr mode,
                                     Bool verbose, int flag)
{
    /* There are problems with double scan mode at high clocks
     * They're likely related PLL and display buffer settings.
     * Disable these modes for now.
     */
    if (mode->Flags & V_DBLSCAN) {
	if ((mode->CrtcHDisplay >= 1024) || (mode->CrtcVDisplay >= 768))
	    return MODE_CLOCK_RANGE;
    }
    return MODE_OK;
}

/* Adjust viewport into virtual desktop such that (0,0) in viewport
 * space is (x,y) in virtual space.
 */
void RADEONDoAdjustFrame(ScrnInfoPtr pScrn, int x, int y, Bool crtc2)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int            Base, reg, regcntl, crtcoffsetcntl, xytilereg, crtcxytile = 0;
#ifdef XF86DRI
    RADEONSAREAPrivPtr pSAREAPriv;
    XF86DRISAREAPtr pSAREA;
#endif

#if 0 /* Verbose */
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONDoAdjustFrame(%d,%d,%d)\n", x, y, clone);
#endif

    if (info->showCache && y) {
	        int lastline = info->FbMapSize /
		    ((pScrn->displayWidth * pScrn->bitsPerPixel) / 8);

		lastline -= pScrn->currentMode->VDisplay;
		y += (pScrn->virtualY - 1) * (y / 3 + 1);
		if (y > lastline) y = lastline;
    }

    Base = pScrn->fbOffset;

  /* note we cannot really simply use the info->ModeReg.crtc_offset_cntl value, since the
     drm might have set FLIP_CNTL since we wrote that. Unfortunately FLIP_CNTL causes
     flickering when scrolling vertically in a virtual screen, possibly because crtc will
     pick up the new offset value at the end of each scanline, but the new offset_cntl value
     only after a vsync. We'd probably need to wait (in drm) for vsync and only then update
     OFFSET and OFFSET_CNTL, if the y coord has changed. Seems hard to fix. */
    if (crtc2) {
	reg = RADEON_CRTC2_OFFSET;
	regcntl = RADEON_CRTC2_OFFSET_CNTL;
	xytilereg = R300_CRTC2_TILE_X0_Y0;
    } else {
	reg = RADEON_CRTC_OFFSET;
	regcntl = RADEON_CRTC_OFFSET_CNTL;
	xytilereg = R300_CRTC_TILE_X0_Y0;
    }
    crtcoffsetcntl = INREG(regcntl) & ~0xf;
#if 0
    /* try to get rid of flickering when scrolling at least for 2d */
#ifdef XF86DRI
    if (!info->have3DWindows)
#endif
    crtcoffsetcntl &= ~RADEON_CRTC_OFFSET_FLIP_CNTL;
#endif
    if (info->tilingEnabled) {
        if (IS_R300_VARIANT) {
	/* On r300/r400 when tiling is enabled crtc_offset is set to the address of
	 * the surface.  the x/y offsets are handled by the X_Y tile reg for each crtc
	 * Makes tiling MUCH easier.
	 */
             crtcxytile = x | (y << 16);
             Base &= ~0x7ff;
         } else {
             int byteshift = info->CurrentLayout.bitsPerPixel >> 4;
             /* crtc uses 256(bytes)x8 "half-tile" start addresses? */
             int tile_addr = (((y >> 3) * info->CurrentLayout.displayWidth + x) >> (8 - byteshift)) << 11;
             Base += tile_addr + ((x << byteshift) % 256) + ((y % 8) << 8);
             crtcoffsetcntl = crtcoffsetcntl | (y % 16);
         }
    }
    else {
       int offset = y * info->CurrentLayout.displayWidth + x;
       switch (info->CurrentLayout.pixel_code) {
       case 15:
       case 16: offset *= 2; break;
       case 24: offset *= 3; break;
       case 32: offset *= 4; break;
       }
       Base += offset;
    }

    Base &= ~7;                 /* 3 lower bits are always 0 */

#ifdef XF86DRI
    if (info->directRenderingInited) {
	/* note cannot use pScrn->pScreen since this is unitialized when called from
	   RADEONScreenInit, and we need to call from there to get mergedfb + pageflip working */
        /*** NOTE: r3/4xx will need sarea and drm pageflip updates to handle the xytile regs for
	 *** pageflipping!
	 ***/
	pSAREAPriv = DRIGetSAREAPrivate(screenInfo.screens[pScrn->scrnIndex]);
	/* can't get at sarea in a semi-sane way? */
	pSAREA = (void *)((char*)pSAREAPriv - sizeof(XF86DRISAREARec));

	if (crtc2) {
	    pSAREAPriv->crtc2_base = Base;
	}
	else {
	    pSAREA->frame.x = (Base  / info->CurrentLayout.pixel_bytes)
		% info->CurrentLayout.displayWidth;
	    pSAREA->frame.y = (Base / info->CurrentLayout.pixel_bytes)
		/ info->CurrentLayout.displayWidth;
	    pSAREA->frame.width = pScrn->frameX1 - x + 1;
	    pSAREA->frame.height = pScrn->frameY1 - y + 1;
	}

	if (pSAREAPriv->pfCurrentPage == 1) {
	    Base += info->backOffset - info->frontOffset;
	}
    }
#endif

    if (IS_R300_VARIANT) {
	OUTREG(xytilereg, crtcxytile);
    } else {
	OUTREG(regcntl, crtcoffsetcntl);
    }

    OUTREG(reg, Base);
}

void RADEONAdjustFrame(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr    pScrn      = xf86Screens[scrnIndex];
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(pScrn);
    xf86OutputPtr  output = config->output[config->compat_output];
    xf86CrtcPtr	crtc = output->crtc;

#ifdef XF86DRI
    if (info->CPStarted && pScrn->pScreen) DRILock(pScrn->pScreen, 0);
#endif

    if (info->accelOn)
        RADEON_SYNC(info, pScrn);

    if (crtc && crtc->enabled) {
	if (info->FBDev) {
	    fbdevHWAdjustFrame(scrnIndex, crtc->desiredX + x, crtc->desiredY + y, flags);
	} else {
	    if (crtc == pRADEONEnt->pCrtc[0])
		RADEONDoAdjustFrame(pScrn, crtc->desiredX + x, crtc->desiredY + y, FALSE);
	    else
		RADEONDoAdjustFrame(pScrn, crtc->desiredX + x, crtc->desiredY + y, TRUE);
	}
	crtc->x = output->initial_x + x;
	crtc->y = output->initial_y + y;
    }


#ifdef XF86DRI
	if (info->CPStarted && pScrn->pScreen) DRIUnlock(pScrn->pScreen);
#endif
}

/* Called when VT switching back to the X server.  Reinitialize the
 * video mode.
 */
Bool RADEONEnterVT(int scrnIndex, int flags)
{
    ScrnInfoPtr    pScrn = xf86Screens[scrnIndex];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONEnterVT\n");

    if (!info->FBDev && (INREG(RADEON_CONFIG_MEMSIZE) == 0)) { /* Softboot V_BIOS */
       xf86Int10InfoPtr pInt;
       xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                  "zero MEMSIZE, probably at D3cold. Re-POSTing via int10.\n");
       pInt = xf86InitInt10 (info->pEnt->index);
       if (pInt) {
           pInt->num = 0xe6;
           xf86ExecX86int10 (pInt);
           xf86FreeInt10 (pInt);
       }
    }

    /* Makes sure the engine is idle before doing anything */
    RADEONWaitForIdleMMIO(pScrn);

    if (info->FBDev) {
	unsigned char *RADEONMMIO = info->MMIO;
	if (!fbdevHWEnterVT(scrnIndex,flags)) return FALSE;
	info->PaletteSavedOnVT = FALSE;
	info->ModeReg.surface_cntl = INREG(RADEON_SURFACE_CNTL);

	RADEONRestoreFBDevRegisters(pScrn, &info->ModeReg);
    } else {
	int i;

	pScrn->vtSema = TRUE;
	for (i = 0; i < xf86_config->num_crtc; i++)
	{
	    xf86CrtcPtr	crtc = xf86_config->crtc[i];
	    /* Mark that we'll need to re-set the mode for sure */
	    memset(&crtc->mode, 0, sizeof(crtc->mode));
	    if (!crtc->desiredMode.CrtcHDisplay) {
		crtc->desiredMode = *RADEONCrtcFindClosestMode (crtc, pScrn->currentMode);
		crtc->desiredRotation = RR_Rotate_0;
		crtc->desiredX = 0;
		crtc->desiredY = 0;
	    }

	    if (!xf86CrtcSetMode (crtc, &crtc->desiredMode, crtc->desiredRotation,
				    crtc->desiredX, crtc->desiredY))
		return FALSE;

	}
    }

    RADEONRestoreSurfaces(pScrn, &info->ModeReg);
#ifdef XF86DRI
    if (info->directRenderingEnabled) {
    	if (info->cardType == CARD_PCIE && info->pKernelDRMVersion->version_minor >= 19 && info->FbSecureSize)
    	{
      		/* we need to backup the PCIE GART TABLE from fb memory */
	  memcpy(info->FB + info->pciGartOffset, info->pciGartBackup, info->pciGartSize);
    	}

	/* get the DRI back into shape after resume */
	RADEONDRISetVBlankInterrupt (pScrn, TRUE);
	RADEONDRIResume(pScrn->pScreen);
	RADEONAdjustMemMapRegisters(pScrn, &info->ModeReg);

    }
#endif
    /* this will get XVideo going again, but only if XVideo was initialised
       during server startup (hence the info->adaptor if). */
    if (info->adaptor)
	RADEONResetVideo(pScrn);

    if (info->accelOn)
	RADEONEngineRestore(pScrn);

#ifdef XF86DRI
    if (info->directRenderingEnabled) {
	RADEONCP_START(pScrn, info);
	DRIUnlock(pScrn->pScreen);
    }
#endif

    //    pScrn->AdjustFrame(scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

    return TRUE;
}

/* Called when VT switching away from the X server.  Restore the
 * original text mode.
 */
void RADEONLeaveVT(int scrnIndex, int flags)
{
    ScrnInfoPtr    pScrn = xf86Screens[scrnIndex];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    RADEONSavePtr  save  = &info->ModeReg;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONLeaveVT\n");
#ifdef XF86DRI
    if (RADEONPTR(pScrn)->directRenderingInited) {

	RADEONDRISetVBlankInterrupt (pScrn, FALSE);
	DRILock(pScrn->pScreen, 0);
	RADEONCP_STOP(pScrn, info);

        if (info->cardType == CARD_PCIE && info->pKernelDRMVersion->version_minor >= 19 && info->FbSecureSize)
        {
            /* we need to backup the PCIE GART TABLE from fb memory */
            memcpy(info->pciGartBackup, (info->FB + info->pciGartOffset), info->pciGartSize);
        }

	/* Make sure 3D clients will re-upload textures to video RAM */
	if (info->textureSize) {
	    RADEONSAREAPrivPtr pSAREAPriv =
		(RADEONSAREAPrivPtr)DRIGetSAREAPrivate(pScrn->pScreen);
	    drmTextureRegionPtr list = pSAREAPriv->texList[0];
	    int age = ++pSAREAPriv->texAge[0], i = 0;

	    do {
		list[i].age = age;
		i = list[i].next;
	    } while (i != 0);
	}
    }
#endif

    if (info->FBDev) {
	RADEONSavePalette(pScrn, save);
	info->PaletteSavedOnVT = TRUE;

	RADEONSaveFBDevRegisters(pScrn, &info->ModeReg);

	fbdevHWLeaveVT(scrnIndex,flags);
    }

    RADEONRestore(pScrn);

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Ok, leaving now...\n");
}

/* Called at the end of each server generation.  Restore the original
 * text mode, unmap video memory, and unwrap and call the saved
 * CloseScreen function.
 */
static Bool RADEONCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86Screens[scrnIndex];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONCloseScreen\n");

    /* Mark acceleration as stopped or we might try to access the engine at
     * wrong times, especially if we had DRI, after DRI has been stopped
     */
    info->accelOn = FALSE;

#ifdef XF86DRI
#ifdef DAMAGE
    if (info->pDamage) {
	PixmapPtr pPix = pScreen->GetScreenPixmap(pScreen);

	DamageUnregister(&pPix->drawable, info->pDamage);
	DamageDestroy(info->pDamage);
	info->pDamage = NULL;
    }
#endif

    RADEONDRIStop(pScreen);
#endif

#ifdef USE_XAA
    if(!info->useEXA && info->RenderTex) {
        xf86FreeOffscreenLinear(info->RenderTex);
        info->RenderTex = NULL;
    }
#endif /* USE_XAA */

    if (pScrn->vtSema) {
	RADEONRestore(pScrn);
    }

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Disposing accel...\n");
#ifdef USE_EXA
    if (info->exa) {
	exaDriverFini(pScreen);
	xfree(info->exa);
	info->exa = NULL;
    }
#endif /* USE_EXA */
#ifdef USE_XAA
    if (!info->useEXA) {
	if (info->accel)
		XAADestroyInfoRec(info->accel);
	info->accel = NULL;

	if (info->scratch_save)
	    xfree(info->scratch_save);
	info->scratch_save = NULL;
    }
#endif /* USE_XAA */

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Disposing cusor info\n");
    if (info->cursor) xf86DestroyCursorInfoRec(info->cursor);
    info->cursor = NULL;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Disposing DGA\n");
    if (info->DGAModes) xfree(info->DGAModes);
    info->DGAModes = NULL;
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "Unmapping memory\n");
    RADEONUnmapMem(pScrn);

    pScrn->vtSema = FALSE;

    xf86ClearPrimInitDone(info->pEnt->index);

    pScreen->BlockHandler = info->BlockHandler;
    pScreen->CloseScreen = info->CloseScreen;
    return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

void RADEONFreeScreen(int scrnIndex, int flags)
{
    ScrnInfoPtr  pScrn = xf86Screens[scrnIndex];
    RADEONInfoPtr  info  = RADEONPTR(pScrn);
    
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "RADEONFreeScreen\n");

    /* when server quits at PreInit, we don't need do this anymore*/
    if (!info) return;

#ifdef WITH_VGAHW
    if (info->VGAAccess && xf86LoaderCheckSymbol("vgaHWFreeHWRec"))
	vgaHWFreeHWRec(pScrn);
#endif
    RADEONFreeRec(pScrn);
}

static void RADEONForceSomeClocks(ScrnInfoPtr pScrn)
{
    /* It appears from r300 and rv100 may need some clocks forced-on */
     CARD32 tmp;

     tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
     tmp |= RADEON_SCLK_FORCE_CP | RADEON_SCLK_FORCE_VIP;
     OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);
}

static void RADEONSetDynamicClock(ScrnInfoPtr pScrn, int mode)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 tmp;
    switch(mode) {
        case 0: /* Turn everything OFF (ForceON to everything)*/
            if ( !pRADEONEnt->HasCRTC2 ) {
                tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
                tmp |= (RADEON_SCLK_FORCE_CP   | RADEON_SCLK_FORCE_HDP |
			RADEON_SCLK_FORCE_DISP1 | RADEON_SCLK_FORCE_TOP |
                        RADEON_SCLK_FORCE_E2   | RADEON_SCLK_FORCE_SE  |
			RADEON_SCLK_FORCE_IDCT | RADEON_SCLK_FORCE_VIP |
			RADEON_SCLK_FORCE_RE   | RADEON_SCLK_FORCE_PB  |
			RADEON_SCLK_FORCE_TAM  | RADEON_SCLK_FORCE_TDM |
                        RADEON_SCLK_FORCE_RB);
                OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);
            } else if (info->ChipFamily == CHIP_FAMILY_RV350) {
                /* for RV350/M10, no delays are required. */
                tmp = INPLL(pScrn, R300_SCLK_CNTL2);
                tmp |= (R300_SCLK_FORCE_TCL |
                        R300_SCLK_FORCE_GA  |
			R300_SCLK_FORCE_CBA);
                OUTPLL(pScrn, R300_SCLK_CNTL2, tmp);

                tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
                tmp |= (RADEON_SCLK_FORCE_DISP2 | RADEON_SCLK_FORCE_CP      |
                        RADEON_SCLK_FORCE_HDP   | RADEON_SCLK_FORCE_DISP1   |
                        RADEON_SCLK_FORCE_TOP   | RADEON_SCLK_FORCE_E2      |
                        R300_SCLK_FORCE_VAP     | RADEON_SCLK_FORCE_IDCT    |
			RADEON_SCLK_FORCE_VIP   | R300_SCLK_FORCE_SR        |
			R300_SCLK_FORCE_PX      | R300_SCLK_FORCE_TX        |
			R300_SCLK_FORCE_US      | RADEON_SCLK_FORCE_TV_SCLK |
                        R300_SCLK_FORCE_SU      | RADEON_SCLK_FORCE_OV0);
                OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);

                tmp = INPLL(pScrn, RADEON_SCLK_MORE_CNTL);
                tmp |= RADEON_SCLK_MORE_FORCEON;
                OUTPLL(pScrn, RADEON_SCLK_MORE_CNTL, tmp);

                tmp = INPLL(pScrn, RADEON_MCLK_CNTL);
                tmp |= (RADEON_FORCEON_MCLKA |
                        RADEON_FORCEON_MCLKB |
                        RADEON_FORCEON_YCLKA |
			RADEON_FORCEON_YCLKB |
                        RADEON_FORCEON_MC);
                OUTPLL(pScrn, RADEON_MCLK_CNTL, tmp);

                tmp = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);
                tmp &= ~(RADEON_PIXCLK_ALWAYS_ONb  | 
                         RADEON_PIXCLK_DAC_ALWAYS_ONb | 
			 R300_DISP_DAC_PIXCLK_DAC_BLANK_OFF); 
                OUTPLL(pScrn, RADEON_VCLK_ECP_CNTL, tmp);

                tmp = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
                tmp &= ~(RADEON_PIX2CLK_ALWAYS_ONb         | 
			 RADEON_PIX2CLK_DAC_ALWAYS_ONb     | 
			 RADEON_DISP_TVOUT_PIXCLK_TV_ALWAYS_ONb | 
			 R300_DVOCLK_ALWAYS_ONb            | 
			 RADEON_PIXCLK_BLEND_ALWAYS_ONb    | 
			 RADEON_PIXCLK_GV_ALWAYS_ONb       | 
			 R300_PIXCLK_DVO_ALWAYS_ONb        | 
			 RADEON_PIXCLK_LVDS_ALWAYS_ONb     | 
			 RADEON_PIXCLK_TMDS_ALWAYS_ONb     | 
			 R300_PIXCLK_TRANS_ALWAYS_ONb      | 
			 R300_PIXCLK_TVO_ALWAYS_ONb        | 
			 R300_P2G2CLK_ALWAYS_ONb            | 
			 R300_P2G2CLK_ALWAYS_ONb           | 
			 R300_DISP_DAC_PIXCLK_DAC2_BLANK_OFF); 
                OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmp);
            }  else {
                tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
                tmp |= (RADEON_SCLK_FORCE_CP | RADEON_SCLK_FORCE_E2);
                tmp |= RADEON_SCLK_FORCE_SE;

		if ( !pRADEONEnt->HasCRTC2 ) {
                     tmp |= ( RADEON_SCLK_FORCE_RB    |
			      RADEON_SCLK_FORCE_TDM   |
			      RADEON_SCLK_FORCE_TAM   |
			      RADEON_SCLK_FORCE_PB    |
			      RADEON_SCLK_FORCE_RE    |
			      RADEON_SCLK_FORCE_VIP   |
			      RADEON_SCLK_FORCE_IDCT  |
			      RADEON_SCLK_FORCE_TOP   |
			      RADEON_SCLK_FORCE_DISP1 |
			      RADEON_SCLK_FORCE_DISP2 |
			      RADEON_SCLK_FORCE_HDP    );
		} else if ((info->ChipFamily == CHIP_FAMILY_R300) ||
			   (info->ChipFamily == CHIP_FAMILY_R350)) {
		    tmp |= ( RADEON_SCLK_FORCE_HDP   |
			     RADEON_SCLK_FORCE_DISP1 |
			     RADEON_SCLK_FORCE_DISP2 |
			     RADEON_SCLK_FORCE_TOP   |
			     RADEON_SCLK_FORCE_IDCT  |
			     RADEON_SCLK_FORCE_VIP);
		}
                OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);
            
                usleep(16000);

		if ((info->ChipFamily == CHIP_FAMILY_R300) ||
		    (info->ChipFamily == CHIP_FAMILY_R350)) {
                    tmp = INPLL(pScrn, R300_SCLK_CNTL2);
                    tmp |= ( R300_SCLK_FORCE_TCL |
			     R300_SCLK_FORCE_GA  |
			     R300_SCLK_FORCE_CBA);
                    OUTPLL(pScrn, R300_SCLK_CNTL2, tmp);
		    usleep(16000);
		}

                if (info->IsIGP) {
                    tmp = INPLL(pScrn, RADEON_MCLK_CNTL);
                    tmp &= ~(RADEON_FORCEON_MCLKA |
			     RADEON_FORCEON_YCLKA);
                    OUTPLL(pScrn, RADEON_MCLK_CNTL, tmp);
		    usleep(16000);
		}
  
		if ((info->ChipFamily == CHIP_FAMILY_RV200) ||
		    (info->ChipFamily == CHIP_FAMILY_RV250) ||
		    (info->ChipFamily == CHIP_FAMILY_RV280)) {
                    tmp = INPLL(pScrn, RADEON_SCLK_MORE_CNTL);
		    tmp |= RADEON_SCLK_MORE_FORCEON;
                    OUTPLL(pScrn, RADEON_SCLK_MORE_CNTL, tmp);
		    usleep(16000);
		}

                tmp = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
                tmp &= ~(RADEON_PIX2CLK_ALWAYS_ONb         |
                         RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
                         RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
                         RADEON_PIXCLK_GV_ALWAYS_ONb       |
                         RADEON_PIXCLK_DIG_TMDS_ALWAYS_ONb |
                         RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
                         RADEON_PIXCLK_TMDS_ALWAYS_ONb);

		OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmp);
		usleep(16000);

                tmp = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);
                tmp &= ~(RADEON_PIXCLK_ALWAYS_ONb  |
			 RADEON_PIXCLK_DAC_ALWAYS_ONb); 
                OUTPLL(pScrn, RADEON_VCLK_ECP_CNTL, tmp);
	    }
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Dynamic Clock Scaling Disabled\n");
            break;
        case 1:
            if (!pRADEONEnt->HasCRTC2) {
                tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
		if ((INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) >
		    RADEON_CFG_ATI_REV_A13) { 
                    tmp &= ~(RADEON_SCLK_FORCE_CP | RADEON_SCLK_FORCE_RB);
                }
                tmp &= ~(RADEON_SCLK_FORCE_HDP  | RADEON_SCLK_FORCE_DISP1 |
			 RADEON_SCLK_FORCE_TOP  | RADEON_SCLK_FORCE_SE   |
			 RADEON_SCLK_FORCE_IDCT | RADEON_SCLK_FORCE_RE   |
			 RADEON_SCLK_FORCE_PB   | RADEON_SCLK_FORCE_TAM  |
			 RADEON_SCLK_FORCE_TDM);
                OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);
	    } else if ((info->ChipFamily == CHIP_FAMILY_R300) ||
		       (info->ChipFamily == CHIP_FAMILY_R350) ||
		       (info->ChipFamily == CHIP_FAMILY_RV350)) {
		if (info->ChipFamily == CHIP_FAMILY_RV350) {
		    tmp = INPLL(pScrn, R300_SCLK_CNTL2);
		    tmp &= ~(R300_SCLK_FORCE_TCL |
			     R300_SCLK_FORCE_GA  |
			     R300_SCLK_FORCE_CBA);
		    tmp |=  (R300_SCLK_TCL_MAX_DYN_STOP_LAT |
			     R300_SCLK_GA_MAX_DYN_STOP_LAT  |
			     R300_SCLK_CBA_MAX_DYN_STOP_LAT);
		    OUTPLL(pScrn, R300_SCLK_CNTL2, tmp);

		    tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
		    tmp &= ~(RADEON_SCLK_FORCE_DISP2 | RADEON_SCLK_FORCE_CP      |
			     RADEON_SCLK_FORCE_HDP   | RADEON_SCLK_FORCE_DISP1   |
			     RADEON_SCLK_FORCE_TOP   | RADEON_SCLK_FORCE_E2      |
			     R300_SCLK_FORCE_VAP     | RADEON_SCLK_FORCE_IDCT    |
			     RADEON_SCLK_FORCE_VIP   | R300_SCLK_FORCE_SR        |
			     R300_SCLK_FORCE_PX      | R300_SCLK_FORCE_TX        |
			     R300_SCLK_FORCE_US      | RADEON_SCLK_FORCE_TV_SCLK |
			     R300_SCLK_FORCE_SU      | RADEON_SCLK_FORCE_OV0);
		    tmp |=  RADEON_DYN_STOP_LAT_MASK;
		    OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);

		    tmp = INPLL(pScrn, RADEON_SCLK_MORE_CNTL);
		    tmp &= ~RADEON_SCLK_MORE_FORCEON;
		    tmp |=  RADEON_SCLK_MORE_MAX_DYN_STOP_LAT;
		    OUTPLL(pScrn, RADEON_SCLK_MORE_CNTL, tmp);

		    tmp = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);
		    tmp |= (RADEON_PIXCLK_ALWAYS_ONb |
			    RADEON_PIXCLK_DAC_ALWAYS_ONb);   
		    OUTPLL(pScrn, RADEON_VCLK_ECP_CNTL, tmp);

		    tmp = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
		    tmp |= (RADEON_PIX2CLK_ALWAYS_ONb         |
			    RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
			    RADEON_DISP_TVOUT_PIXCLK_TV_ALWAYS_ONb |
			    R300_DVOCLK_ALWAYS_ONb            |   
			    RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
			    RADEON_PIXCLK_GV_ALWAYS_ONb       |
			    R300_PIXCLK_DVO_ALWAYS_ONb        | 
			    RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
			    RADEON_PIXCLK_TMDS_ALWAYS_ONb     |
			    R300_PIXCLK_TRANS_ALWAYS_ONb      |
			    R300_PIXCLK_TVO_ALWAYS_ONb        |
			    R300_P2G2CLK_ALWAYS_ONb           |
			    R300_P2G2CLK_ALWAYS_ONb);
		    OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmp);

		    tmp = INPLL(pScrn, RADEON_MCLK_MISC);
		    tmp |= (RADEON_MC_MCLK_DYN_ENABLE |
			    RADEON_IO_MCLK_DYN_ENABLE);
		    OUTPLL(pScrn, RADEON_MCLK_MISC, tmp);

		    tmp = INPLL(pScrn, RADEON_MCLK_CNTL);
		    tmp |= (RADEON_FORCEON_MCLKA |
			    RADEON_FORCEON_MCLKB);

		    tmp &= ~(RADEON_FORCEON_YCLKA  |
			     RADEON_FORCEON_YCLKB  |
			     RADEON_FORCEON_MC);

		    /* Some releases of vbios have set DISABLE_MC_MCLKA
		       and DISABLE_MC_MCLKB bits in the vbios table.  Setting these
		       bits will cause H/W hang when reading video memory with dynamic clocking
		       enabled. */
		    if ((tmp & R300_DISABLE_MC_MCLKA) &&
			(tmp & R300_DISABLE_MC_MCLKB)) {
			/* If both bits are set, then check the active channels */
			tmp = INPLL(pScrn, RADEON_MCLK_CNTL);
			if (info->RamWidth == 64) {
			    if (INREG(RADEON_MEM_CNTL) & R300_MEM_USE_CD_CH_ONLY)
				tmp &= ~R300_DISABLE_MC_MCLKB;
			    else
				tmp &= ~R300_DISABLE_MC_MCLKA;
			} else {
			    tmp &= ~(R300_DISABLE_MC_MCLKA |
				     R300_DISABLE_MC_MCLKB);
			}
		    }

		    OUTPLL(pScrn, RADEON_MCLK_CNTL, tmp);
		} else {
		    tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
		    tmp &= ~(R300_SCLK_FORCE_VAP);
		    tmp |= RADEON_SCLK_FORCE_CP;
		    OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);
		    usleep(15000);

		    tmp = INPLL(pScrn, R300_SCLK_CNTL2);
		    tmp &= ~(R300_SCLK_FORCE_TCL |
			     R300_SCLK_FORCE_GA  |
			     R300_SCLK_FORCE_CBA);
		    OUTPLL(pScrn, R300_SCLK_CNTL2, tmp);
		}
	    } else {
                tmp = INPLL(pScrn, RADEON_CLK_PWRMGT_CNTL);

                tmp &= ~(RADEON_ACTIVE_HILO_LAT_MASK     | 
			 RADEON_DISP_DYN_STOP_LAT_MASK   | 
			 RADEON_DYN_STOP_MODE_MASK); 

                tmp |= (RADEON_ENGIN_DYNCLK_MODE |
			(0x01 << RADEON_ACTIVE_HILO_LAT_SHIFT));
                OUTPLL(pScrn, RADEON_CLK_PWRMGT_CNTL, tmp);
		usleep(15000);

                tmp = INPLL(pScrn, RADEON_CLK_PIN_CNTL);
                tmp |= RADEON_SCLK_DYN_START_CNTL; 
                OUTPLL(pScrn, RADEON_CLK_PIN_CNTL, tmp);
		usleep(15000);

		/* When DRI is enabled, setting DYN_STOP_LAT to zero can cause some R200 
		   to lockup randomly, leave them as set by BIOS.
		*/
                tmp = INPLL(pScrn, RADEON_SCLK_CNTL);
                /*tmp &= RADEON_SCLK_SRC_SEL_MASK;*/
		tmp &= ~RADEON_SCLK_FORCEON_MASK;

                /*RAGE_6::A11 A12 A12N1 A13, RV250::A11 A12, R300*/
		if (((info->ChipFamily == CHIP_FAMILY_RV250) &&
		     ((INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <
		      RADEON_CFG_ATI_REV_A13)) || 
		    ((info->ChipFamily == CHIP_FAMILY_RV100) &&
		     ((INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <=
		      RADEON_CFG_ATI_REV_A13))){
                    tmp |= RADEON_SCLK_FORCE_CP;
                    tmp |= RADEON_SCLK_FORCE_VIP;
                }

                OUTPLL(pScrn, RADEON_SCLK_CNTL, tmp);

		if ((info->ChipFamily == CHIP_FAMILY_RV200) ||
		    (info->ChipFamily == CHIP_FAMILY_RV250) ||
		    (info->ChipFamily == CHIP_FAMILY_RV280)) {
                    tmp = INPLL(pScrn, RADEON_SCLK_MORE_CNTL);
                    tmp &= ~RADEON_SCLK_MORE_FORCEON;

                    /* RV200::A11 A12 RV250::A11 A12 */
		    if (((info->ChipFamily == CHIP_FAMILY_RV200) ||
			 (info->ChipFamily == CHIP_FAMILY_RV250)) &&
			((INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <
			 RADEON_CFG_ATI_REV_A13)) {
                        tmp |= RADEON_SCLK_MORE_FORCEON;
		    }
                    OUTPLL(pScrn, RADEON_SCLK_MORE_CNTL, tmp);
		    usleep(15000);
                }

                /* RV200::A11 A12, RV250::A11 A12 */
                if (((info->ChipFamily == CHIP_FAMILY_RV200) ||
		     (info->ChipFamily == CHIP_FAMILY_RV250)) &&
		    ((INREG(RADEON_CONFIG_CNTL) & RADEON_CFG_ATI_REV_ID_MASK) <
		     RADEON_CFG_ATI_REV_A13)) {
                    tmp = INPLL(pScrn, RADEON_PLL_PWRMGT_CNTL);
                    tmp |= RADEON_TCL_BYPASS_DISABLE;
                    OUTPLL(pScrn, RADEON_PLL_PWRMGT_CNTL, tmp);
                }
		usleep(15000);

                /*enable dynamic mode for display clocks (PIXCLK and PIX2CLK)*/
		tmp = INPLL(pScrn, RADEON_PIXCLKS_CNTL);
		tmp |=  (RADEON_PIX2CLK_ALWAYS_ONb         |
			 RADEON_PIX2CLK_DAC_ALWAYS_ONb     |
			 RADEON_PIXCLK_BLEND_ALWAYS_ONb    |
			 RADEON_PIXCLK_GV_ALWAYS_ONb       |
			 RADEON_PIXCLK_DIG_TMDS_ALWAYS_ONb |
			 RADEON_PIXCLK_LVDS_ALWAYS_ONb     |
			 RADEON_PIXCLK_TMDS_ALWAYS_ONb);

		OUTPLL(pScrn, RADEON_PIXCLKS_CNTL, tmp);
		usleep(15000);

		tmp = INPLL(pScrn, RADEON_VCLK_ECP_CNTL);
		tmp |= (RADEON_PIXCLK_ALWAYS_ONb  |
		        RADEON_PIXCLK_DAC_ALWAYS_ONb); 

                OUTPLL(pScrn, RADEON_VCLK_ECP_CNTL, tmp);
		usleep(15000);
            }    
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Dynamic Clock Scaling Enabled\n");
	    break;
        default:
	    break;
    }
}
