
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright © 2002 David Dawes

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i830.h,v 1.12 2004/01/07 03:43:19 dawes Exp $ */

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *   David Dawes <dawes@xfree86.org>
 *
 */

#if 0
#define I830DEBUG
#endif

#ifndef REMAP_RESERVED
#define REMAP_RESERVED 0
#endif

#ifndef _I830_H_
#define _I830_H_

#include "xf86_OSproc.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "i810_reg.h"
#include "xaa.h"
#include "xf86Cursor.h"
#include "xf86xv.h"
#include "xf86int10.h"
#include "vbe.h"
#include "vgaHW.h"
#include "i830_xf86Crtc.h"
#include "i830_randr.h"

#ifdef XF86DRI
#include "xf86drm.h"
#include "sarea.h"
#define _XF86DRI_SERVER_
#include "dri.h"
#include "GL/glxint.h"
#include "i830_dri.h"
#endif

#ifdef I830_USE_EXA
#include "exa.h"
Bool I830EXAInit(ScreenPtr pScreen);
#define EXA_LINEAR_EXTRA	(64*1024)
#endif

#ifdef I830_USE_XAA
Bool I830XAAInit(ScreenPtr pScreen);
#endif

typedef struct _I830OutputRec I830OutputRec, *I830OutputPtr;

#include "common.h"
#include "i830_sdvo.h"
#include "i2c_vid.h"

/* I830 Video support */
#define NEED_REPLIES				/* ? */
#define EXTENSION_PROC_ARGS void *
#include "extnsionst.h" 			/* required */
#include <X11/extensions/panoramiXproto.h> 	/* required */

/*
 * The mode handling is based upon the VESA driver written by
 * Paulo César Pereira de Andrade <pcpa@conectiva.com.br>.
 */

#ifdef XF86DRI
#define I830_MM_MINPAGES 512
#define I830_MM_MAXSIZE  (32*1024)
#define I830_KERNEL_MM  (1 << 0) /* Initialize the kernel memory manager*/
#define I830_KERNEL_TEX (1 << 1) /* Allocate texture memory pool */
#endif

typedef struct _I830Rec *I830Ptr;

typedef void (*I830WriteIndexedByteFunc)(I830Ptr pI830, IOADDRESS addr,
                                         CARD8 index, CARD8 value);
typedef CARD8(*I830ReadIndexedByteFunc)(I830Ptr pI830, IOADDRESS addr,
                                        CARD8 index);
typedef void (*I830WriteByteFunc)(I830Ptr pI830, IOADDRESS addr, CARD8 value);
typedef CARD8(*I830ReadByteFunc)(I830Ptr pI830, IOADDRESS addr);

/* Linear region allocated in framebuffer. */
typedef struct _I830MemPool *I830MemPoolPtr;
typedef struct _I830MemRange *I830MemRangePtr;
typedef struct _I830MemRange {
   long Start;
   long End;
   long Size;
   unsigned long Physical;
   unsigned long Offset;		/* Offset of AGP-allocated portion */
   unsigned long Alignment;
   int Key;
   I830MemPoolPtr Pool;
} I830MemRange;

typedef struct _I830MemPool {
   I830MemRange Total;
   I830MemRange Free;
   I830MemRange Fixed;
   I830MemRange Allocated;
} I830MemPool;

typedef struct {
   int tail_mask;
   I830MemRange mem;
   unsigned char *virtual_start;
   int head;
   int tail;
   int space;
} I830RingBuffer;

typedef struct {
   unsigned int Fence[FENCE_NEW_NR * 2];
} I830RegRec, *I830RegPtr;

typedef struct {
   int            lastInstance;
   int            refCount;
   ScrnInfoPtr    pScrn_1;
   ScrnInfoPtr    pScrn_2;
   int            RingRunning;
#ifdef I830_XV
   int            XvInUse;
#endif
} I830EntRec, *I830EntPtr;

/* store information about an Ixxx DVO */
/* The i830->i865 use multiple DVOs with multiple i2cs */
/* the i915, i945 have a single sDVO i2c bus - which is different */
#define MAX_OUTPUTS 6

#define I830_I2C_BUS_DVO 1
#define I830_I2C_BUS_SDVO 2

/* these are outputs from the chip - integrated only 
   external chips are via DVO or SDVO output */
#define I830_OUTPUT_UNUSED 0
#define I830_OUTPUT_ANALOG 1
#define I830_OUTPUT_DVO 2
#define I830_OUTPUT_SDVO 3
#define I830_OUTPUT_LVDS 4
#define I830_OUTPUT_TVOUT 5

#define I830_DVO_CHIP_NONE 0
#define I830_DVO_CHIP_LVDS 1
#define I830_DVO_CHIP_TMDS 2
#define I830_DVO_CHIP_TVOUT 4

struct _I830DVODriver {
   int type;
   char *modulename;
   char *fntablename;
   int address;
   const char **symbols;
   I830I2CVidOutputRec *vid_rec;
   void *dev_priv;
   pointer modhandle;
};

extern const char *i830_output_type_names[];

typedef struct _I830CrtcPrivateRec {
    int			    pipe;

    /* Lookup table values to be set when the CRTC is enabled */
    CARD8 lut_r[256], lut_g[256], lut_b[256];

#ifdef I830_USE_XAA
    FBLinearPtr rotate_mem_xaa;
#endif
#ifdef I830_USE_EXA
    ExaOffscreenArea *rotate_mem_exa;
#endif

    I830MemRange cursor_mem;
    I830MemRange cursor_mem_argb;
} I830CrtcPrivateRec, *I830CrtcPrivatePtr;

#define I830CrtcPrivate(c) ((I830CrtcPrivatePtr) (c)->driver_private)

typedef struct _I830OutputPrivateRec {
   int			    type;
   I2CBusPtr		    pI2CBus;
   I2CBusPtr		    pDDCBus;
   struct _I830DVODriver    *i2c_drv;
   Bool			    load_detect_temp;
   /** Output-private structure.  Should replace i2c_drv */
   void			    *dev_priv;
} I830OutputPrivateRec, *I830OutputPrivatePtr;

#define I830OutputPrivate(o) ((I830OutputPrivatePtr) (o)->driver_private)

/** enumeration of 3d consumers so some can maintain invariant state. */
enum last_3d {
    LAST_3D_OTHER,
    LAST_3D_VIDEO,
    LAST_3D_RENDER,
    LAST_3D_ROTATION
};

#if 0
typedef struct _I830PipeRec {
   Bool		  enabled;
   int		  x;
   int		  y;
   Bool		  cursorInRange;
   Bool		  cursorShown;
   DisplayModeRec curMode;
   DisplayModeRec desiredMode;
#ifdef RANDR_12_INTERFACE
   RRCrtcPtr	  randr_crtc;
#endif
} I830PipeRec, *I830PipePtr;
#endif

typedef struct _I830Rec {
   unsigned char *MMIOBase;
   unsigned char *FbBase;
   int cpp;

   DisplayModePtr currentMode;

   I830EntPtr entityPrivate;	
   int init;

   unsigned int bufferOffset;		/* for I830SelectBuffer */
   BoxRec FbMemBox;
   BoxRec FbMemBox2;
   int CacheLines;

   /* These are set in PreInit and never changed. */
   long FbMapSize;
   long TotalVideoRam;
   I830MemRange StolenMemory;		/* pre-allocated memory */

   /* These change according to what has been allocated. */
   long FreeMemory;
   I830MemRange MemoryAperture;
   I830MemPool StolenPool;
   long allocatedMemory;

   /* Regions allocated either from the above pools, or from agpgart. */
   /* for single and dual head configurations */
   I830MemRange FrontBuffer;
   I830MemRange FrontBuffer2;
   I830MemRange Scratch;
   I830MemRange Scratch2;
#ifdef I830_USE_EXA
   I830MemRange Offscreen;
   I830MemRange EXAStateMem;  /* specific exa state for G965 */
#endif
   /* Regions allocated either from the above pools, or from agpgart. */
   I830RingBuffer *LpRing;

#if REMAP_RESERVED
   I830MemRange Dummy;
#endif

#ifdef I830_XV
   /* For Xvideo */
   I830MemRange *OverlayMem;
   I830MemRange LinearMem;
#endif
   unsigned long LinearAlloc;
   XF86ModReqInfo shadowReq; /* to test for later libshadow */
   Rotation rotation;
   int displayWidth;
   void (*PointerMoved)(int, int, int);
   CreateScreenResourcesProcPtr    CreateScreenResources;
   int *used3D;

   I830MemRange ContextMem;
#ifdef XF86DRI
   I830MemRange BackBuffer;
   I830MemRange DepthBuffer;
   I830MemRange TexMem;
   int TexGranularity;
   int drmMinor;
   Bool have3DWindows;
   int mmModeFlags;
   int mmSize;

   unsigned int front_tiled;
   unsigned int back_tiled;
   unsigned int depth_tiled;
#endif

   Bool NeedRingBufferLow;
   Bool allowPageFlip;
   Bool disableTiling;

   int backPitch;

   Bool CursorNeedsPhysical;
   Bool CursorIsARGB;
   CursorPtr pCurs;

   DGAModePtr DGAModes;
   int numDGAModes;
   Bool DGAactive;
   int DGAViewportStatus;

   int Chipset;
   unsigned long LinearAddr;
   unsigned long MMIOAddr;
   IOADDRESS ioBase;
   EntityInfoPtr pEnt;
   pciVideoPtr PciInfo;
   PCITAG PciTag;
   CARD8 variant;

   unsigned int BR[20];

   int GttBound;

   unsigned char **ScanlineColorExpandBuffers;
   int NumScanlineColorExpandBuffers;
   int nextColorExpandBuf;

   I830RegRec ModeReg;

   Bool useEXA;
   Bool noAccel;
   Bool SWCursor;
   Bool cursorOn;
#ifdef I830_USE_XAA
   XAAInfoRecPtr AccelInfoRec;

   /* additional XAA accelerated Composite support */
   CompositeProcPtr saved_composite;
   Bool (*xaa_check_composite)(int op, PicturePtr pSrc, PicturePtr pMask,
			       PicturePtr pDst);
   Bool (*xaa_prepare_composite)(int op, PicturePtr pSrc, PicturePtr pMask,
				 PicturePtr pDst, PixmapPtr pSrcPixmap,
				 PixmapPtr pMaskPixmap, PixmapPtr pDstPixmap);
   void (*xaa_composite)(PixmapPtr pDst, int xSrc, int ySrc,
			 int xMask, int yMask, int xDst, int yDst,
			 int w, int h);
   void (*xaa_done_composite)(PixmapPtr pDst);
#endif
   xf86CursorInfoPtr CursorInfoRec;
   CloseScreenProcPtr CloseScreen;

#ifdef I830_USE_EXA
   unsigned int copy_src_pitch;
   unsigned int copy_src_off;
   ExaDriverPtr	EXADriverPtr;
#endif

   I830WriteIndexedByteFunc writeControl;
   I830ReadIndexedByteFunc readControl;
   I830WriteByteFunc writeStandard;
   I830ReadByteFunc readStandard;

   Bool XvDisabled;			/* Xv disabled in PreInit. */
   Bool XvEnabled;			/* Xv enabled for this generation. */

#ifdef I830_XV
   int colorKey;
   XF86VideoAdaptorPtr adaptor;
   ScreenBlockHandlerProcPtr BlockHandler;
   Bool *overlayOn;
#endif

   /* EXA render state */
   float scale_units[2][2];
  /** Transform pointers for src/mask, or NULL if identity */
   PictTransform *transform[2];
   /* i915 EXA render state */
   CARD32 mapstate[6];
   CARD32 samplerstate[6];

   Bool directRenderingDisabled;	/* DRI disabled in PreInit. */
   Bool directRenderingEnabled;		/* DRI enabled this generation. */

#ifdef XF86DRI
   Bool directRenderingOpen;
   int LockHeld;
   DRIInfoPtr pDRIInfo;
   int drmSubFD;
   int numVisualConfigs;
   __GLXvisualConfig *pVisualConfigs;
   I830ConfigPrivPtr pVisualConfigsPriv;
   drm_handle_t buffer_map;
   drm_handle_t ring_map;
#endif

   /* Broken-out options. */
   OptionInfoPtr Options;

   /* Stolen memory support */
   Bool StolenOnly;

   Bool swfSaved;
   CARD32 saveSWF0;
   CARD32 saveSWF4;

   Bool checkDevices;

   /* Driver phase/state information */
   Bool preinit;
   Bool starting;
   Bool closing;
   Bool suspended;
   Bool leaving;

   /* fbOffset converted to (x, y). */
   int xoffset;
   int yoffset;

   unsigned int SaveGeneration;

   OsTimerPtr devicesTimer;

   int ddc2;

   /* The BIOS's fixed timings for the LVDS */
   DisplayModePtr panel_fixed_mode;

   int backlight_duty_cycle;  /* restore backlight to this value */
   
   Bool panel_wants_dither;

   CARD32 saveDSPACNTR;
   CARD32 saveDSPBCNTR;
   CARD32 savePIPEACONF;
   CARD32 savePIPEBCONF;
   CARD32 savePIPEASRC;
   CARD32 savePIPEBSRC;
   CARD32 saveFPA0;
   CARD32 saveFPA1;
   CARD32 saveDPLL_A;
   CARD32 saveDPLL_A_MD;
   CARD32 saveHTOTAL_A;
   CARD32 saveHBLANK_A;
   CARD32 saveHSYNC_A;
   CARD32 saveVTOTAL_A;
   CARD32 saveVBLANK_A;
   CARD32 saveVSYNC_A;
   CARD32 saveDSPASTRIDE;
   CARD32 saveDSPASIZE;
   CARD32 saveDSPAPOS;
   CARD32 saveDSPABASE;
   CARD32 saveDSPASURF;
   CARD32 saveFPB0;
   CARD32 saveFPB1;
   CARD32 saveDPLL_B;
   CARD32 saveDPLL_B_MD;
   CARD32 saveHTOTAL_B;
   CARD32 saveHBLANK_B;
   CARD32 saveHSYNC_B;
   CARD32 saveVTOTAL_B;
   CARD32 saveVBLANK_B;
   CARD32 saveVSYNC_B;
   CARD32 saveDSPBSTRIDE;
   CARD32 saveDSPBSIZE;
   CARD32 saveDSPBPOS;
   CARD32 saveDSPBBASE;
   CARD32 saveDSPBSURF;
   CARD32 saveVCLK_DIVISOR_VGA0;
   CARD32 saveVCLK_DIVISOR_VGA1;
   CARD32 saveVCLK_POST_DIV;
   CARD32 saveVGACNTRL;
   CARD32 saveADPA;
   CARD32 saveLVDS;
   CARD32 saveDVOA;
   CARD32 saveDVOB;
   CARD32 saveDVOC;
   CARD32 savePP_ON;
   CARD32 savePP_OFF;
   CARD32 savePP_CONTROL;
   CARD32 savePP_CYCLE;
   CARD32 savePFIT_CONTROL;
   CARD32 savePaletteA[256];
   CARD32 savePaletteB[256];
   CARD32 saveSWF[17];
   CARD32 saveBLC_PWM_CTL;

   enum last_3d last_3d;
} I830Rec;

#define I830PTR(p) ((I830Ptr)((p)->driverPrivate))
#define I830REGPTR(p) (&(I830PTR(p)->ModeReg))

#define I830_SELECT_FRONT	0
#define I830_SELECT_BACK	1
#define I830_SELECT_DEPTH	2

/* I830 specific functions */
extern int I830WaitLpRing(ScrnInfoPtr pScrn, int n, int timeout_millis);
extern void I830SetPIOAccess(I830Ptr pI830);
extern void I830SetMMIOAccess(I830Ptr pI830);
extern void I830Sync(ScrnInfoPtr pScrn);
extern void I830InitHWCursor(ScrnInfoPtr pScrn);
extern void I830SetPipeCursor (xf86CrtcPtr crtc, Bool force);
extern Bool I830CursorInit(ScreenPtr pScreen);
extern void IntelEmitInvarientState(ScrnInfoPtr pScrn);
extern void I830EmitInvarientState(ScrnInfoPtr pScrn);
extern void I915EmitInvarientState(ScrnInfoPtr pScrn);
extern void I830SelectBuffer(ScrnInfoPtr pScrn, int buffer);

extern void I830RefreshRing(ScrnInfoPtr pScrn);
extern void I830EmitFlush(ScrnInfoPtr pScrn);

extern Bool I830DGAReInit(ScreenPtr pScreen);
extern Bool I830DGAInit(ScreenPtr pScreen);

#ifdef I830_XV
extern void I830InitVideo(ScreenPtr pScreen);
extern void i830_crtc_dpms_video(xf86CrtcPtr crtc, Bool on);
#endif

#ifdef XF86DRI
extern Bool I830Allocate3DMemory(ScrnInfoPtr pScrn, const int flags);
extern Bool I830AllocateBackBuffer(ScrnInfoPtr pScrn, const int flags);
extern Bool I830AllocateDepthBuffer(ScrnInfoPtr pScrn, const int flags);
extern Bool I830AllocateTextureMemory(ScrnInfoPtr pScrn, const int flags);
extern void I830SetupMemoryTiling(ScrnInfoPtr pScrn);
extern Bool I830DRIScreenInit(ScreenPtr pScreen);
extern Bool I830CheckDRIAvailable(ScrnInfoPtr pScrn);
extern Bool I830DRIDoMappings(ScreenPtr pScreen);
extern Bool I830DRIResume(ScreenPtr pScreen);
extern void I830DRICloseScreen(ScreenPtr pScreen);
extern Bool I830DRIFinishScreenInit(ScreenPtr pScreen);
extern Bool I830UpdateDRIBuffers(ScrnInfoPtr pScrn, drmI830Sarea *sarea);
extern void I830DRIUnmapScreenRegions(ScrnInfoPtr pScrn, drmI830Sarea *sarea);
extern Bool I830DRIMapScreenRegions(ScrnInfoPtr pScrn, drmI830Sarea *sarea);
extern void I830DRIUnlock(ScrnInfoPtr pScrn);
extern Bool I830DRILock(ScrnInfoPtr pScrn);
extern Bool I830DRISetVBlankInterrupt (ScrnInfoPtr pScrn, Bool on);
#endif

unsigned long intel_get_pixmap_offset(PixmapPtr pPix);
unsigned long intel_get_pixmap_pitch(PixmapPtr pPix);
extern Bool I830AccelInit(ScreenPtr pScreen);
extern void I830SetupForScreenToScreenCopy(ScrnInfoPtr pScrn, int xdir,
					   int ydir, int rop,
					   unsigned int planemask,
					   int trans_color);
extern void I830SubsequentScreenToScreenCopy(ScrnInfoPtr pScrn, int srcX,
					     int srcY, int dstX, int dstY,
					     int w, int h);
extern void I830SetupForSolidFill(ScrnInfoPtr pScrn, int color, int rop,
				  unsigned int planemask);
extern void I830SubsequentSolidFillRect(ScrnInfoPtr pScrn, int x, int y,
					int w, int h);

extern void I830ResetAllocations(ScrnInfoPtr pScrn, const int flags);
extern long I830CheckAvailableMemory(ScrnInfoPtr pScrn);
extern long I830GetExcessMemoryAllocations(ScrnInfoPtr pScrn);
extern Bool I830Allocate2DMemory(ScrnInfoPtr pScrn, const int flags);
extern Bool I830DoPoolAllocation(ScrnInfoPtr pScrn, I830MemPool *pool);
extern Bool I830FixupOffsets(ScrnInfoPtr pScrn);
extern Bool I830BindAGPMemory(ScrnInfoPtr pScrn);
extern Bool I830UnbindAGPMemory(ScrnInfoPtr pScrn);
extern unsigned long I830AllocVidMem(ScrnInfoPtr pScrn, I830MemRange *result,
				     I830MemPool *pool, long size,
				     unsigned long alignment, int flags);
extern void I830FreeVidMem(ScrnInfoPtr pScrn, I830MemRange *range);

extern void I830PrintAllRegisters(I830RegPtr i830Reg);
extern void I830ReadAllRegisters(I830Ptr pI830, I830RegPtr i830Reg);

extern void I830ChangeFrontbuffer(ScrnInfoPtr pScrn,int buffer);
extern Bool I830IsPrimary(ScrnInfoPtr pScrn);

extern Bool I830FixOffset(ScrnInfoPtr pScrn, I830MemRange *mem);
extern Bool I830I2CInit(ScrnInfoPtr pScrn, I2CBusPtr *bus_ptr, int i2c_reg,
			char *name);

/* return a mask of output indices matching outputs against type_mask */
int i830_output_clones (ScrnInfoPtr pScrn, int type_mask);

/* i830_display.c */
Bool
i830PipeHasType (xf86CrtcPtr crtc, int type);

/* i830_crt.c */
void i830_crt_init(ScrnInfoPtr pScrn);

/* i830_dvo.c */
void i830_dvo_init(ScrnInfoPtr pScrn);

/* i830_lvds.c */
void i830_lvds_init(ScrnInfoPtr pScrn);

extern void i830MarkSync(ScrnInfoPtr pScrn);
extern void i830WaitSync(ScrnInfoPtr pScrn);

/* i830_memory.c */
Bool I830BindAGPMemory(ScrnInfoPtr pScrn);
Bool I830UnbindAGPMemory(ScrnInfoPtr pScrn);
#ifdef I830_USE_XAA
FBLinearPtr
i830_xf86AllocateOffscreenLinear(ScreenPtr pScreen, int length,
				 int granularity,
				 MoveLinearCallbackProcPtr moveCB,
				 RemoveLinearCallbackProcPtr removeCB,
				 pointer privData);
#endif /* I830_USE_EXA */

/* i830_modes.c */
DisplayModePtr i830_ddc_get_modes(xf86OutputPtr output);

/* i830_tv.c */
void i830_tv_init(ScrnInfoPtr pScrn);

/* i830_render.c */
Bool i830_check_composite(int op, PicturePtr pSrc, PicturePtr pMask,
			  PicturePtr pDst);
Bool i830_prepare_composite(int op, PicturePtr pSrc, PicturePtr pMask,
			    PicturePtr pDst, PixmapPtr pSrcPixmap,
			    PixmapPtr pMaskPixmap, PixmapPtr pDstPixmap);
void i830_composite(PixmapPtr pDst, int srcX, int srcY,
		    int maskX, int maskY, int dstX, int dstY, int w, int h);
void i830_done_composite(PixmapPtr pDst);
/* i915_render.c */
Bool i915_check_composite(int op, PicturePtr pSrc, PicturePtr pMask,
			  PicturePtr pDst);
Bool i915_prepare_composite(int op, PicturePtr pSrc, PicturePtr pMask,
			    PicturePtr pDst, PixmapPtr pSrcPixmap,
			    PixmapPtr pMaskPixmap, PixmapPtr pDstPixmap);
/* i965_render.c */
Bool i965_check_composite(int op, PicturePtr pSrc, PicturePtr pMask,
			  PicturePtr pDst);
Bool i965_prepare_composite(int op, PicturePtr pSrc, PicturePtr pMask,
			    PicturePtr pDst, PixmapPtr pSrcPixmap,
			    PixmapPtr pMaskPixmap, PixmapPtr pDstPixmap);
void i965_composite(PixmapPtr pDst, int srcX, int srcY,
		    int maskX, int maskY, int dstX, int dstY, int w, int h);

void
i830_get_transformed_coordinates(int x, int y, PictTransformPtr transform,
				 float *x_out, float *y_out);

extern const int I830PatternROP[16];
extern const int I830CopyROP[16];

/* Flags for memory allocation function */
#define FROM_ANYWHERE			0x00000000
#define FROM_POOL_ONLY			0x00000001
#define FROM_NEW_ONLY			0x00000002
#define FROM_MASK			0x0000000f

#define ALLOCATE_AT_TOP			0x00000010
#define ALLOCATE_AT_BOTTOM		0x00000020
#define FORCE_GAPS			0x00000040

#define NEED_PHYSICAL_ADDR		0x00000100
#define ALIGN_BOTH_ENDS			0x00000200
#define FORCE_LOW			0x00000400

#define ALLOC_NO_TILING			0x00001000
#define ALLOC_INITIAL			0x00002000

#define ALLOCATE_DRY_RUN		0x80000000

/* Chipset registers for VIDEO BIOS memory RW access */
#define _855_DRAM_RW_CONTROL 0x58
#define _845_DRAM_RW_CONTROL 0x90
#define DRAM_WRITE    0x33330000

/* 
 * Xserver MM compatibility. Remove code guarded by this when the
 * XServer contains the libdrm mm code
 */
#undef XSERVER_LIBDRM_MM

#endif /* _I830_H_ */
