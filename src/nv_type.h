/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_type.h,v 1.51 2005/04/16 23:57:26 mvojkovi Exp $ */

#ifndef __NV_STRUCT_H__
#define __NV_STRUCT_H__

#include "colormapst.h"
#include "vgaHW.h"
#include "xaa.h"
#include "xf86Cursor.h"
#include "xf86int10.h"
#include "exa.h"
#ifdef XF86DRI
#define _XF86DRI_SERVER_
#include "xf86drm.h"
#include "dri.h"
#include "nouveau_drm.h"
#else
#error "This driver requires a DRI-enabled X server"
#endif

#define NV_ARCH_04  0x04
#define NV_ARCH_10  0x10
#define NV_ARCH_20  0x20
#define NV_ARCH_30  0x30
#define NV_ARCH_40  0x40

#define CHIPSET_NV04     0x0020
#define CHIPSET_NV10     0x0100
#define CHIPSET_NV11     0x0110
#define CHIPSET_NV15     0x0150
#define CHIPSET_NV17     0x0170
#define CHIPSET_NV18     0x0180
#define CHIPSET_NFORCE   0x01A0
#define CHIPSET_NFORCE2  0x01F0
#define CHIPSET_NV20     0x0200
#define CHIPSET_NV25     0x0250
#define CHIPSET_NV28     0x0280
#define CHIPSET_NV30     0x0300
#define CHIPSET_NV31     0x0310
#define CHIPSET_NV34     0x0320
#define CHIPSET_NV35     0x0330
#define CHIPSET_NV36     0x0340
#define CHIPSET_NV40     0x0040
#define CHIPSET_NV41     0x00C0
#define CHIPSET_NV43     0x0140
#define CHIPSET_NV44     0x0160
#define CHIPSET_NV44A    0x0220
#define CHIPSET_NV45     0x0210
#define CHIPSET_MISC_BRIDGED  0x00F0
#define CHIPSET_G70      0x0090
#define CHIPSET_G71      0x0290
#define CHIPSET_G72      0x01D0
#define CHIPSET_G73      0x0390
// integrated GeForces (6100, 6150)
#define CHIPSET_C51      0x0240
// variant of C51, seems based on a G70 design
#define CHIPSET_C512     0x03D0
#define CHIPSET_G73_BRIDGED 0x02E0


#define BITMASK(t,b) (((unsigned)(1U << (((t)-(b)+1)))-1)  << (b))
#define MASKEXPAND(mask) BITMASK(1?mask,0?mask)
#define SetBF(mask,value) ((value) << (0?mask))
#define GetBF(var,mask) (((unsigned)((var) & MASKEXPAND(mask))) >> (0?mask) )
#define SetBitField(value,from,to) SetBF(to, GetBF(value,from))
#define SetBit(n) (1<<(n))
#define Set8Bits(value) ((value)&0xff)

typedef struct {
    int bitsPerPixel;
    int depth;
    int displayWidth;
    rgb weight;
    DisplayModePtr mode;
} NVFBLayout;

typedef struct _riva_hw_state
{
    U032 bpp;
    U032 width;
    U032 height;
    U032 interlace;
    U032 repaint0;
    U032 repaint1;
    U032 screen;
    U032 scale;
    U032 dither;
    U032 extra;
    U032 fifo;
    U032 pixel;
    U032 horiz;
    U032 arbitration0;
    U032 arbitration1;
    U032 pll;
    U032 pllB;
    U032 vpll;
    U032 vpll2;
    U032 vpllB;
    U032 vpll2B;
    U032 pllsel;
    U032 general;
    U032 crtcOwner;
    U032 head;
    U032 head2;
    U032 config;
    U032 cursorConfig;
    U032 cursor0;
    U032 cursor1;
    U032 cursor2;
    U032 timingH;
    U032 timingV;
    U032 displayV;
    U032 crtcSync;
} RIVA_HW_STATE, *NVRegPtr;


typedef struct _NVRec *NVPtr;
typedef struct _NVRec {
    RIVA_HW_STATE       SavedReg;
    RIVA_HW_STATE       ModeReg;
    RIVA_HW_STATE       *CurrentState;
    CARD32              Architecture;
    CARD32              CursorStart;
    EntityInfoPtr       pEnt;
    pciVideoPtr         PciInfo;
    PCITAG              PciTag;
    int                 Chipset;
    int                 ChipRev;
    Bool                Primary;
    CARD32              IOAddress;
    unsigned long       FbAddress;
    unsigned char *     FbBase;
    unsigned char *     FbStart;
    CARD32              FbMapSize;
    CARD32              FbUsableSize;
    CARD32              ScratchBufferSize;
    CARD32              ScratchBufferStart;
    Bool                NoAccel;
    Bool                HWCursor;
    Bool                FpScale;
    Bool                ShadowFB;
    unsigned char *     ShadowPtr;
    int                 ShadowPitch;
    CARD32              MinVClockFreqKHz;
    CARD32              MaxVClockFreqKHz;
    CARD32              CrystalFreqKHz;
    CARD32              RamAmountKBytes;
    int drm_fd;
    unsigned long drm_agp_handle;
    unsigned long drm_agp_map_handle;
    unsigned char *agpScratch;
    unsigned long agpScratchPhysical;
    unsigned long agpScratchSize;

    volatile U032 *REGS;
    volatile U032 *PCRTC0;
    volatile U032 *PCRTC;
    volatile U032 *PRAMDAC0;
    volatile U032 *PFB;
    volatile U032 *PFIFO;
    volatile U032 *PGRAPH;
    volatile U032 *PEXTDEV;
    volatile U032 *PTIMER;
    volatile U032 *PMC;
    volatile U032 *PRAMIN;
    volatile U032 *FIFO;
    volatile U032 *CURSOR;
    volatile U008 *PCIO0;
    volatile U008 *PCIO;
    volatile U008 *PVIO;
    volatile U008 *PDIO0;
    volatile U008 *PDIO;
    volatile U032 *PRAMDAC;
    volatile U008 *PROM;

    volatile U032 *RAMHT;
    CARD32 pramin_free;

    XAAInfoRecPtr       AccelInfoRec;
    ExaDriverPtr	EXADriverPtr;
    Bool                useEXA;
    xf86CursorInfoPtr   CursorInfoRec;
    DGAModePtr          DGAModes;
    int                 numDGAModes;
    Bool                DGAactive;
    int                 DGAViewportStatus;
    void		(*PointerMoved)(int index, int x, int y);
    ScreenBlockHandlerProcPtr BlockHandler;
    CloseScreenProcPtr  CloseScreen;
    Bool                FBDev;
    int			Rotate;
    NVFBLayout		CurrentLayout;
    /* Cursor */
    CARD32              curFg, curBg;
    CARD32              curImage[256];
    /* I2C / DDC */
    I2CBusPtr           I2C;
    xf86Int10InfoPtr    pInt;
    void		(*VideoTimerCallback)(ScrnInfoPtr, Time);
    void		(*DMAKickoffCallback)(NVPtr pNv);
    XF86VideoAdaptorPtr	overlayAdaptor;
    XF86VideoAdaptorPtr	blitAdaptor;
    int			videoKey;
    int			FlatPanel;
    Bool                FPDither;
    Bool                Television;
    int			CRTCnumber;
	int         vtOWNER;
    OptionInfoPtr	Options;
    Bool                alphaCursor;
    unsigned char       DDCBase;
    Bool                twoHeads;
    Bool                twoStagePLL;
    Bool                fpScaler;
    int                 fpWidth;
    int                 fpHeight;
    CARD32              fpSyncs;
    Bool                usePanelTweak;
    int                 PanelTweak;
    Bool                LVDS;

    int                 IRQ;
    Bool                LockedUp;

    void *              Notifier0;
    drm_nouveau_fifo_init_t fifo;
    CARD32              dmaPut;
    CARD32              dmaCurrent;
    CARD32              dmaFree;
    CARD32              dmaMax;
    CARD32              *dmaBase;

    CARD32              currentRop;
    Bool                WaitVSyncPossible;
    Bool                BlendingPossible;
    Bool                RandRRotation;
#ifdef XF86DRI
    DRIInfoPtr          pDRIInfo;
#endif /* XF86DRI */
} NVRec;

#define NVPTR(p) ((NVPtr)((p)->driverPrivate))

#endif /* __NV_STRUCT_H__ */
