/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_type.h,v 1.51 2005/04/16 23:57:26 mvojkovi Exp $ */

#ifndef __NV_STRUCT_H__
#define __NV_STRUCT_H__

#include "colormapst.h"
#include "vgaHW.h"
#include "xf86Cursor.h"
#include "xf86int10.h"
#include "exa.h"
#ifdef XF86DRI
#define _XF86DRI_SERVER_
#include "xf86drm.h"
#include "dri.h"
#include <stdint.h>
#include "nouveau_drm.h"
#include "xf86Crtc.h"
#else
#error "This driver requires a DRI-enabled X server"
#endif

#include "nv50_type.h"
#include "nv_pcicompat.h"

#include "nouveau_local.h" /* needed for NOUVEAU_EXA_PIXMAPS */

#define NV_ARCH_03  0x03
#define NV_ARCH_04  0x04
#define NV_ARCH_10  0x10
#define NV_ARCH_20  0x20
#define NV_ARCH_30  0x30
#define NV_ARCH_40  0x40
#define NV_ARCH_50  0x50

#define CHIPSET_NV03     0x0010
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
#define CHIPSET_NV50     0x0190
#define CHIPSET_NV84     0x0400
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

#define MAX_NUM_DCB_ENTRIES 16

typedef enum /* matches DCB types */
{
    OUTPUT_NONE = 4,
    OUTPUT_ANALOG = 0,
    OUTPUT_TMDS = 2,
    OUTPUT_LVDS = 3,
    OUTPUT_TV = 1,
} NVOutputType;

typedef struct {
    int bitsPerPixel;
    int depth;
    int displayWidth;
    rgb weight;
    DisplayModePtr mode;
} NVFBLayout;

typedef struct _nv_crtc_reg 
{
	unsigned char MiscOutReg;     /* */
	uint8_t CRTC[0xff];
	uint8_t CR58[0x10];
	uint8_t Sequencer[5];
	uint8_t Graphics[9];
	uint8_t Attribute[21];
	unsigned char DAC[768];       /* Internal Colorlookuptable */
	uint32_t cursorConfig;
	uint32_t crtcOwner;
	uint32_t gpio;
	uint32_t unk830;
	uint32_t unk834;
	uint32_t unk850;
	uint32_t unk81c;
	uint32_t head;
	uint32_t config;

	/* These are former output regs, but are believed to be crtc related */
	uint32_t general;
	uint32_t debug_0[2];
	uint32_t debug_1;
	uint32_t debug_2;
	uint32_t unk_a20;
	uint32_t unk_a24;
	uint32_t unk_a34;
	uint32_t fp_horiz_regs[7];
	uint32_t fp_vert_regs[7];
	uint32_t fp_hvalid_start;
	uint32_t fp_hvalid_end;
	uint32_t fp_vvalid_start;
	uint32_t fp_vvalid_end;
	uint32_t bpp;
	uint32_t nv10_cursync;
	uint32_t fp_control[2];
	uint32_t crtcSync;
	uint32_t dither;
} NVCrtcRegRec, *NVCrtcRegPtr;

typedef struct _nv_output_reg
{
	uint32_t test_control;
	uint32_t unk_670;

	uint32_t output;
	uint8_t TMDS[0xFF];
	uint8_t TMDS2[0xFF];
} NVOutputRegRec, *NVOutputRegPtr;

typedef struct _riva_hw_state
{
    CARD32 bpp;
    CARD32 width;
    CARD32 height;
    CARD32 interlace;
    CARD32 repaint0;
    CARD32 repaint1;
    CARD32 screen;
    CARD32 scale;
    CARD32 dither;
    CARD32 extra;
    CARD32 fifo;
    CARD32 pixel;
    CARD32 horiz;
    CARD32 arbitration0;
    CARD32 arbitration1;
    CARD32 pll;
    CARD32 pllB;
    CARD32 vpll;
    CARD32 vpll2;
    CARD32 vpllB;
    CARD32 vpll2B;
    CARD32 pllsel;
	uint32_t sel_clk;
	Bool crosswired;
	Bool vpll_changed[2];
	Bool db1_ratio[2];
	/* These vpll values are only for nv4x hardware */
	uint32_t vpll1_a;
	uint32_t vpll1_b;
	uint32_t vpll2_a;
	uint32_t vpll2_b;
	uint32_t reg580;
	uint32_t reg594;
    CARD32 general;
    CARD32 crtcOwner;
    CARD32 head;
    CARD32 head2;
    CARD32 config;
    CARD32 cursorConfig;
    CARD32 cursor0;
    CARD32 cursor1;
    CARD32 cursor2;
    CARD32 timingH;
    CARD32 timingV;
    CARD32 displayV;
    CARD32 crtcSync;

    NVCrtcRegRec crtc_reg[2];
    NVOutputRegRec dac_reg[2];
} RIVA_HW_STATE, *NVRegPtr;

typedef struct _nv50_crtc_reg
{
	
} NV50CrtcRegRec, *NV50CrtcRegPtr;

typedef struct _nv50_hw_state
{
	NV50CrtcRegRec crtc_reg[2];
} NV50_HW_STATE, *NV50RegPtr;

typedef enum {
	OUTPUT_0 = (1 << 0),
	OUTPUT_1 = (1 << 1)
} ValidOutputResource;

typedef struct _NVOutputPrivateRec {
	uint8_t preferred_output;
	uint8_t output_resource;
	uint8_t last_dpms;
	I2CBusPtr pDDCBus;
	NVOutputType type;
	int dcb_entry;
	uint32_t fpWidth;
	uint32_t fpHeight;
	DisplayModePtr native_mode;
	Bool fpdither;
	uint8_t scaling_mode;
} NVOutputPrivateRec, *NVOutputPrivatePtr;

typedef struct _MiscStartupInfo {
	uint8_t crtc_0_reg_52;
	uint32_t ramdac_0_reg_580;
	uint32_t ramdac_0_pllsel;
	uint32_t reg_c040;
	uint32_t sel_clk;
	uint32_t output[2];
} MiscStartupInfo;

typedef enum {
	OUTPUT_0_SLAVED = (1 << 0),
	OUTPUT_1_SLAVED = (1 << 1),
	OUTPUT_0_LVDS = (1 << 2),
	OUTPUT_1_LVDS = (1 << 3),
	OUTPUT_0_CROSSWIRED_TMDS = (1 << 4),
	OUTPUT_1_CROSSWIRED_TMDS = (1 << 5)
} OutputInfo;

struct dcb_entry {
	uint8_t type;
	uint8_t i2c_index;
	uint8_t heads;
	uint8_t bus;
	uint8_t location;
	uint8_t or;
	Bool duallink_possible;
	union {
		struct {
			Bool use_straps_for_mode;
			Bool use_power_scripts;
		} lvdsconf;
	};
};

enum pll_types {
	VPLL1,
	VPLL2
};

struct pll_lims {
	struct {
		/* nv3x needs 32 bit values */
		uint32_t minfreq;
		uint32_t maxfreq;
		uint32_t min_inputfreq;
		uint16_t max_inputfreq;

		uint8_t min_m;
		uint8_t max_m;
		uint8_t min_n;
		uint8_t max_n;
	} vco1, vco2;

	uint8_t unk1c;
	uint8_t unk1d;
	uint8_t unk1e;
};

typedef struct {
	uint8_t *data;
	unsigned int length;
	Bool execute;

	uint8_t major_version, chip_version;

	uint32_t fmaxvco, fminvco;

	uint16_t init_script_tbls_ptr;
	uint16_t extra_init_script_tbl_ptr;
	uint16_t macro_index_tbl_ptr;
	uint16_t macro_tbl_ptr;
	uint16_t condition_tbl_ptr;
	uint16_t io_condition_tbl_ptr;
	uint16_t io_flag_condition_tbl_ptr;
	uint16_t init_function_tbl_ptr;

	uint16_t pll_limit_tbl_ptr;
	uint16_t ram_restrict_tbl_ptr;

	struct {
		DisplayModePtr native_mode;
		uint8_t *edid;
		uint16_t lvdsmanufacturerpointer;
		uint16_t xlated_entry;
		Bool reset_after_pclk_change;
		Bool dual_link;
		Bool if_is_18bit;
		Bool BITbit1;
	} fp;

	struct {
		uint16_t output0_script_ptr;
		uint16_t output1_script_ptr;
	} tmds;

	struct {
		uint8_t crt, tv, panel;
	} legacy_i2c_indices;
} bios_t;

enum LVDS_script {
	/* Order *does* matter here */
	LVDS_INIT = 1,
	LVDS_RESET,
	LVDS_BACKLIGHT_ON,
	LVDS_BACKLIGHT_OFF,
	LVDS_PANEL_ON,
	LVDS_PANEL_OFF
};

#define NVOutputPrivate(o) ((NVOutputPrivatePtr (o)->driver_private)

typedef struct _NVRec *NVPtr;
typedef struct _NVRec {
    RIVA_HW_STATE       SavedReg;
    RIVA_HW_STATE       ModeReg;
    RIVA_HW_STATE       *CurrentState;
	NV50_HW_STATE	NV50SavedReg;
	NV50_HW_STATE	NV50ModeReg;
    uint32_t              Architecture;
    EntityInfoPtr       pEnt;
#ifndef XSERVER_LIBPCIACCESS
	pciVideoPtr	PciInfo;
	PCITAG		PciTag;
#else
	struct pci_device *PciInfo;
#endif /* XSERVER_LIBPCIACCESS */
    int                 Chipset;
    int                 NVArch;
    Bool                Primary;
    CARD32              IOAddress;
    Bool cursorOn;

    /* VRAM physical address */
    unsigned long	VRAMPhysical;
    /* Size of VRAM BAR */
    unsigned long	VRAMPhysicalSize;
    /* Accesible VRAM size (by the GPU) */
    unsigned long	VRAMSize;
    /* Accessible AGP size */
    unsigned long	AGPSize;

    /* Various pinned memory regions */
    struct nouveau_bo * FB;
    struct nouveau_bo * Cursor;
    struct nouveau_bo * Cursor2;
    struct nouveau_bo * CLUT;	/* NV50 only */
    struct nouveau_bo * GART;

    bios_t		VBIOS;
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

    volatile CARD32 *REGS;
    volatile CARD32 *PCRTC0;
    volatile CARD32 *PCRTC1;

	volatile CARD32 *NV50_PCRTC;

    volatile CARD32 *PRAMDAC0;
    volatile CARD32 *PRAMDAC1;
    volatile CARD32 *PFB;
    volatile CARD32 *PFIFO;
    volatile CARD32 *PGRAPH;
    volatile CARD32 *PEXTDEV;
    volatile CARD32 *PTIMER;
    volatile CARD32 *PVIDEO;
    volatile CARD32 *PMC;
    volatile CARD32 *PRAMIN;
    volatile CARD32 *CURSOR;
    volatile CARD8 *PCIO0;
    volatile CARD8 *PCIO1;
    volatile CARD8 *PVIO0;
    volatile CARD8 *PVIO1;
    volatile CARD8 *PDIO0;
    volatile CARD8 *PDIO1;
    volatile CARD8 *PROM;


    volatile CARD32 *RAMHT;
    CARD32 pramin_free;

    unsigned int SaveGeneration;
    uint8_t cur_head;
    ExaDriverPtr	EXADriverPtr;
    xf86CursorInfoPtr   CursorInfoRec;
    void		(*PointerMoved)(int index, int x, int y);
    ScreenBlockHandlerProcPtr BlockHandler;
    CloseScreenProcPtr  CloseScreen;
    int			Rotate;
    NVFBLayout		CurrentLayout;
    /* Cursor */
    CARD32              curFg, curBg;
    CARD32              curImage[256];
    /* I2C / DDC */
    int ddc2;
    xf86Int10InfoPtr    pInt10;
    I2CBusPtr           I2C;
  void		(*VideoTimerCallback)(ScrnInfoPtr, Time);
    XF86VideoAdaptorPtr	overlayAdaptor;
    XF86VideoAdaptorPtr	blitAdaptor;
    XF86VideoAdaptorPtr	textureAdaptor;
    int			videoKey;
    int			FlatPanel;
    Bool                FPDither;
    int                 Mobile;
    Bool                Television;
	int         vtOWNER;
	Bool		crtc_active[2];
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

    Bool                LockedUp;

    CARD32              currentRop;

    Bool                WaitVSyncPossible;
    Bool                BlendingPossible;
    Bool                RandRRotation;
    DRIInfoPtr          pDRIInfo;
    drmVersionPtr       pLibDRMVersion;
    drmVersionPtr       pKernelDRMVersion;

    Bool randr12_enable;
    CreateScreenResourcesProcPtr    CreateScreenResources;

    I2CBusPtr           pI2CBus[MAX_NUM_DCB_ENTRIES];

	/* Is our secondary (analog) output not flexible (ffs(or) != 3)? */
	Bool restricted_mode;
	Bool switchable_crtc;

	uint8_t fp_regs_owner[2];

	struct {
		int entries;
		struct dcb_entry entry[MAX_NUM_DCB_ENTRIES];
		unsigned char i2c_read[MAX_NUM_DCB_ENTRIES];
		unsigned char i2c_write[MAX_NUM_DCB_ENTRIES];
	} dcb_table;

	uint32_t output_info;
	MiscStartupInfo misc_info;

	struct {
		ORNum dac;
		ORNum sor;
	} i2cMap[4];
	struct {
		Bool  present;
		ORNum or;
	} lvds;

	/* DRM interface */
	struct nouveau_device *dev;

	/* GPU context */
	struct nouveau_channel *chan;
	struct nouveau_notifier *notify0;
	struct nouveau_grobj *NvNull;
	struct nouveau_grobj *NvContextSurfaces;
	struct nouveau_grobj *NvContextBeta1;
	struct nouveau_grobj *NvContextBeta4;
	struct nouveau_grobj *NvImagePattern;
	struct nouveau_grobj *NvRop;
	struct nouveau_grobj *NvRectangle;
	struct nouveau_grobj *NvImageBlit;
	struct nouveau_grobj *NvScaledImage;
	struct nouveau_grobj *NvClipRectangle;
	struct nouveau_grobj *NvMemFormat;
	struct nouveau_grobj *NvImageFromCpu;
	struct nouveau_grobj *Nv2D;
	struct nouveau_grobj *Nv3D;

} NVRec;

typedef struct _NVCrtcPrivateRec {
	int head;
	uint8_t last_dpms;
	Bool paletteEnabled;
#if NOUVEAU_EXA_PIXMAPS
	struct nouveau_bo *shadow;
#else
	ExaOffscreenArea *shadow;
#endif /* NOUVEAU_EXA_PIXMAPS */
} NVCrtcPrivateRec, *NVCrtcPrivatePtr;

typedef struct _NV50CrtcPrivRec {
	int head;
	int pclk; /* Target pixel clock in kHz */
	Bool cursorVisible;
	Bool skipModeFixup;
	Bool dither;
} NV50CrtcPrivRec, *NV50CrtcPrivPtr;

enum scaling_modes {
	SCALE_PANEL,
	SCALE_FULLSCREEN,
	SCALE_ASPECT,
	SCALE_NOSCALE,
	SCALE_INVALID
};

#define NVCrtcPrivate(c) ((NVCrtcPrivatePtr)(c)->driver_private)

#define NVPTR(p) ((NVPtr)((p)->driverPrivate))

#define nvReadRAMDAC0(pNv, reg) nvReadRAMDAC(pNv, 0, reg)
#define nvWriteRAMDAC0(pNv, reg, val) nvWriteRAMDAC(pNv, 0, reg, val)

#define nvReadCurRAMDAC(pNv, reg) nvReadRAMDAC(pNv, pNv->cur_head, reg)
#define nvWriteCurRAMDAC(pNv, reg, val) nvWriteRAMDAC(pNv, pNv->cur_head, reg, val)

#define nvReadCRTC0(pNv, reg) nvReadCRTC(pNv, 0, reg)
#define nvWriteCRTC0(pNv, reg, val) nvWriteCRTC(pNv, 0, reg, val)

#define nvReadCurCRTC(pNv, reg) nvReadCRTC(pNv, pNv->cur_head, reg)
#define nvWriteCurCRTC(pNv, reg, val) nvWriteCRTC(pNv, pNv->cur_head, reg, val)

#define nvReadFB(pNv, fb_reg) MMIO_IN32(pNv->PFB, fb_reg)
#define nvWriteFB(pNv, fb_reg, val) MMIO_OUT32(pNv->PFB, fb_reg, val)

#define nvReadGRAPH(pNv, reg) MMIO_IN32(pNv->PGRAPH, reg)
#define nvWriteGRAPH(pNv, reg, val) MMIO_OUT32(pNv->PGRAPH, reg, val)

#define nvReadMC(pNv, reg) MMIO_IN32(pNv->PMC, reg)
#define nvWriteMC(pNv, reg, val) MMIO_OUT32(pNv->PMC, reg, val)

#define nvReadEXTDEV(pNv, reg) MMIO_IN32(pNv->PEXTDEV, reg)
#define nvWriteEXTDEV(pNv, reg, val) MMIO_OUT32(pNv->PEXTDEV, reg, val)

#define nvReadTIMER(pNv, reg) MMIO_IN32(pNv->PTIMER, reg)
#define nvWriteTIMER(pNv, reg, val) MMIO_OUT32(pNv->PTIMER, reg, val)

#define nvReadVIDEO(pNv, reg) MMIO_IN32(pNv->PVIDEO, reg)
#define nvWriteVIDEO(pNv, reg, val) MMIO_OUT32(pNv->PVIDEO, reg, val)

typedef struct _NVPortPrivRec {
	short		brightness;
	short		contrast;
	short		saturation;
	short		hue;
	RegionRec	clip;
	CARD32		colorKey;
	Bool		autopaintColorKey;
	Bool		doubleBuffer;
	CARD32		videoStatus;
	int		currentBuffer;
	Time		videoTime;
	int		overlayCRTC;
	Bool		grabbedByV4L;
	Bool		iturbt_709;
	Bool		blitter;
	Bool		texture;
	Bool		SyncToVBlank;
	struct nouveau_bo *video_mem;
	int		pitch;
	int		offset;
	struct nouveau_bo *TT_mem_chunk[2];
	int		currentHostBuffer;
	struct nouveau_notifier *DMANotifier[2];
} NVPortPrivRec, *NVPortPrivPtr;

#define GET_OVERLAY_PRIVATE(pNv) \
            (NVPortPrivPtr)((pNv)->overlayAdaptor->pPortPrivates[0].ptr)

#define GET_BLIT_PRIVATE(pNv) \
            (NVPortPrivPtr)((pNv)->blitAdaptor->pPortPrivates[0].ptr)

#define GET_TEXTURE_PRIVATE(pNv) \
            (NVPortPrivPtr)((pNv)->textureAdaptor->pPortPrivates[0].ptr)

#define OFF_TIMER       0x01
#define FREE_TIMER      0x02
#define CLIENT_VIDEO_ON 0x04
#define OFF_DELAY       500  /* milliseconds */
#define FREE_DELAY      5000

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

#endif /* __NV_STRUCT_H__ */
