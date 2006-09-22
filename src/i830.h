
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
#include "randrstr.h"

#ifdef XF86DRI
#include "xf86drm.h"
#include "sarea.h"
#define _XF86DRI_SERVER_
#include "dri.h"
#include "GL/glxint.h"
#include "i830_dri.h"
#endif

#include "common.h"
#include "i830_sdvo.h"
#include "i2c_vid.h"

/* I830 Video support */

/*
 * The mode handling is based upon the VESA driver written by
 * Paulo César Pereira de Andrade <pcpa@conectiva.com.br>.
 */

#define PIPE_NONE	0<<0
#define PIPE_CRT	1<<0
#define PIPE_TV		1<<1
#define PIPE_DFP	1<<2
#define PIPE_LFP	1<<3
#define PIPE_CRT2	1<<4
#define PIPE_TV2	1<<5
#define PIPE_DFP2	1<<6
#define PIPE_LFP2	1<<7

typedef struct _VESARec {
   /* SVGA state */
   pointer state, pstate;
   int statePage, stateSize, stateMode, stateRefresh;
   CARD32 *savedPal;
   int savedScanlinePitch;
   xf86MonPtr monitor;
   /* display start */
   int x, y;
} VESARec, *VESAPtr;


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
   unsigned int Fence[8];
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

typedef struct _I830SDVODriver {
   I2CDevRec d;
   unsigned char sdvo_regs[20];
   CARD32 output_device;		/* SDVOB or SDVOC */

   i830_sdvo_caps caps;
   int save_sdvo_mult;
   Bool save_sdvo_active_1, save_sdvo_active_2;
   i830_sdvo_dtd save_input_dtd_1, save_input_dtd_2;
   i830_sdvo_dtd save_output_dtd_1, save_output_dtd_2;
   CARD32 save_SDVOX;
} I830SDVORec, *I830SDVOPtr;

struct _I830OutputRec {
   int type;
   int pipe;
   int flags;
   xf86MonPtr MonInfo;
   I2CBusPtr pI2CBus;
   I2CBusPtr pDDCBus;
   struct _I830DVODriver *i2c_drv;
   I830SDVOPtr sdvo_drv;
};

typedef struct _I830Rec {
   unsigned char *MMIOBase;
   unsigned char *FbBase;
   int cpp;

   unsigned int bios_version;

   Bool newPipeSwitch;

   Bool fakeSwitch;
   
   int fixedPipe;

   DisplayModePtr currentMode;
   /* Mode saved during randr reprobe, which will need to be freed at the point
    * of the next SwitchMode, when we lose this last reference to it.
    */
   DisplayModePtr savedCurrentMode;

   Bool gammaEnabled[MAX_DISPLAY_PIPES];

   Bool Clone;
   int CloneRefresh;
   int CloneHDisplay;
   int CloneVDisplay;

   I830EntPtr entityPrivate;	
   int pipe, origPipe;
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

   /* Regions allocated either from the above pools, or from agpgart. */
   I830MemRange	*CursorMem;
   I830MemRange	*CursorMemARGB;
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
   I830MemRange RotatedMem;
   I830MemRange RotatedMem2;
   Rotation rotation;
   int InitialRotation;
   int displayWidth;
   void (*PointerMoved)(int, int, int);
   CreateScreenResourcesProcPtr    CreateScreenResources;
   int *used3D;

#ifdef XF86DRI
   I830MemRange BackBuffer;
   I830MemRange DepthBuffer;
   I830MemRange TexMem;
   int TexGranularity;
   I830MemRange ContextMem;
   int drmMinor;
   Bool have3DWindows;
#endif

   Bool NeedRingBufferLow;
   Bool allowPageFlip;
   Bool disableTiling;

   int backPitch;

   Bool CursorNeedsPhysical;
   Bool CursorIsARGB;
   CursorPtr pCurs;

   int MonType1;
   int MonType2;
   Bool specifiedMonitor;

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

   Bool noAccel;
   Bool SWCursor;
   Bool cursorOn;
   XAAInfoRecPtr AccelInfoRec;
   xf86CursorInfoPtr CursorInfoRec;
   CloseScreenProcPtr CloseScreen;

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

   /* Video BIOS support. */
   vbeInfoPtr pVbe;
   VbeInfoBlock *vbeInfo;
   VESAPtr vesa;

   Bool swfSaved;
   CARD32 saveSWF0;
   CARD32 saveSWF4;

   Bool checkDevices;
   int monitorSwitch;
   int operatingDevices;
   int toggleDevices;
   int lastDevice0, lastDevice1, lastDevice2;

   /* [0] is Pipe A, [1] is Pipe B. */
   int availablePipes;
   /* [0] is display plane A, [1] is display plane B. */
   int planeEnabled[MAX_DISPLAY_PIPES];
   MonPtr pipeMon[MAX_DISPLAY_PIPES];
   DisplayModeRec pipeCurMode[MAX_DISPLAY_PIPES];

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
   Bool vbeRestoreWorkaround;
   Bool devicePresence;

   OsTimerPtr devicesTimer;
   int MaxClock;

   int ddc2;
   int num_outputs;
   struct _I830OutputRec output[MAX_OUTPUTS];
   I830SDVOPtr sdvo;

   /* Panel size pulled from the BIOS */
   int PanelXRes, PanelYRes;

   /* The BIOS's fixed timings for the LVDS */
   int panel_fixed_clock;
   int panel_fixed_hactive;
   int panel_fixed_hblank;
   int panel_fixed_hsyncoff;
   int panel_fixed_hsyncwidth;
   int panel_fixed_vactive;
   int panel_fixed_vblank;
   int panel_fixed_vsyncoff;
   int panel_fixed_vsyncwidth;

   int backlight_duty_cycle;  /* restore backlight to this value */
   
   Bool panel_wants_dither;

   unsigned char *VBIOS;

   CARD32 saveDSPACNTR;
   CARD32 saveDSPBCNTR;
   CARD32 savePIPEACONF;
   CARD32 savePIPEBCONF;
   CARD32 savePIPEASRC;
   CARD32 savePIPEBSRC;
   CARD32 saveFPA0;
   CARD32 saveFPA1;
   CARD32 saveDPLL_A;
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
   CARD32 saveFPB0;
   CARD32 saveFPB1;
   CARD32 saveDPLL_B;
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
extern void I830PrintErrorState(ScrnInfoPtr pScrn);
extern void I830Sync(ScrnInfoPtr pScrn);
extern void I830InitHWCursor(ScrnInfoPtr pScrn);
extern Bool I830CursorInit(ScreenPtr pScreen);
extern void I830EmitInvarientState(ScrnInfoPtr pScrn);
extern void I830SelectBuffer(ScrnInfoPtr pScrn, int buffer);

extern void I830RefreshRing(ScrnInfoPtr pScrn);
extern void I830EmitFlush(ScrnInfoPtr pScrn);

extern Bool I830DGAInit(ScreenPtr pScreen);

#ifdef I830_XV
extern void I830InitVideo(ScreenPtr pScreen);
extern void I830VideoSwitchModeBefore(ScrnInfoPtr pScrn, DisplayModePtr mode);
extern void I830VideoSwitchModeAfter(ScrnInfoPtr pScrn, DisplayModePtr mode);
#endif

extern Bool I830AllocateRotatedBuffer(ScrnInfoPtr pScrn, const int flags);
extern Bool I830AllocateRotated2Buffer(ScrnInfoPtr pScrn, const int flags);
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
extern Bool I830BindGARTMemory(ScrnInfoPtr pScrn);
extern Bool I830UnbindGARTMemory(ScrnInfoPtr pScrn);
extern unsigned long I830AllocVidMem(ScrnInfoPtr pScrn, I830MemRange *result,
				     I830MemPool *pool, long size,
				     unsigned long alignment, int flags);
extern void I830FreeVidMem(ScrnInfoPtr pScrn, I830MemRange *range);

extern void I830PrintAllRegisters(I830RegPtr i830Reg);
extern void I830ReadAllRegisters(I830Ptr pI830, I830RegPtr i830Reg);

extern void I830ChangeFrontbuffer(ScrnInfoPtr pScrn,int buffer);
extern Bool I830IsPrimary(ScrnInfoPtr pScrn);

extern void I830PrintModes(ScrnInfoPtr pScrn);
extern Bool I830CheckModeSupport(ScrnInfoPtr pScrn, int x, int y, int mode);
extern Bool I830Rotate(ScrnInfoPtr pScrn, DisplayModePtr mode);
extern Bool I830FixOffset(ScrnInfoPtr pScrn, I830MemRange *mem);
extern Bool I830I2CInit(ScrnInfoPtr pScrn, I2CBusPtr *bus_ptr, int i2c_reg,
			char *name);

/* i830_dvo.c */
Bool I830I2CDetectDVOControllers(ScrnInfoPtr pScrn, I2CBusPtr pI2CBus,
				 struct _I830DVODriver **retdrv);

/* i830_memory.c */
Bool I830BindAGPMemory(ScrnInfoPtr pScrn);
Bool I830UnbindAGPMemory(ScrnInfoPtr pScrn);

/* i830_gtf.c */
DisplayModePtr i830GetGTF(int h_pixels, int v_lines, float freq,
			  int interlaced, int margins);

/* i830_modes.c */
int I830ValidateXF86ModeList(ScrnInfoPtr pScrn, Bool first_time);

/* i830_randr.c */
Bool I830RandRInit(ScreenPtr pScreen, int rotation);
Bool I830RandRSetConfig(ScreenPtr pScreen, Rotation rotation, int rate,
			RRScreenSizePtr pSize);
Rotation I830GetRotation(ScreenPtr pScreen);
void I830GetOriginalVirtualSize(ScrnInfoPtr pScrn, int *x, int *y);

/*
 * 12288 is set as the maximum, chosen because it is enough for
 * 1920x1440@32bpp with a 2048 pixel line pitch with some to spare.
 */
#define I830_MAXIMUM_VBIOS_MEM		12288
#define I830_DEFAULT_VIDEOMEM_2D	(MB(32) / 1024)
#define I830_DEFAULT_VIDEOMEM_3D	(MB(64) / 1024)

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

#endif /* _I830_H_ */
