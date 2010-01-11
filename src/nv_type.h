#ifndef __NV_STRUCT_H__
#define __NV_STRUCT_H__

#include "colormapst.h"
#include "xf86Cursor.h"
#include "xf86int10.h"
#include "exa.h"
#ifdef XF86DRI
#define _XF86DRI_SERVER_
#include "xf86drm.h"
#include "dri.h"
#include <stdbool.h>
#include <stdint.h>
#include "nouveau_drm.h"
#include "xf86Crtc.h"
#else
#error "This driver requires a DRI-enabled X server"
#endif

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

/* NV50 */
typedef struct _NVRec *NVPtr;
typedef struct _NVRec {
    uint32_t              Architecture;
    EntityInfoPtr       pEnt;
	struct pci_device *PciInfo;
    int                 Chipset;
    int                 NVArch;
    Bool                Primary;

    /* Various pinned memory regions */
    struct nouveau_bo * scanout;
    struct nouveau_bo * offscreen;
    void *              offscreen_map;
    struct nouveau_bo * GART;

    Bool                NoAccel;
    Bool                HWCursor;
    Bool                ShadowFB;
    unsigned char *     ShadowPtr;
    int                 ShadowPitch;

    ExaDriverPtr	EXADriverPtr;
    Bool		exa_driver_pixmaps;
    Bool                exa_force_cp;
    Bool		wfb_enabled;
    Bool		tiled_scanout;
    ScreenBlockHandlerProcPtr BlockHandler;
    CreateScreenResourcesProcPtr CreateScreenResources;
    CloseScreenProcPtr  CloseScreen;
    /* Cursor */
    uint32_t		curImage[256];
    void		(*VideoTimerCallback)(ScrnInfoPtr, Time);
    XF86VideoAdaptorPtr	overlayAdaptor;
    XF86VideoAdaptorPtr	blitAdaptor;
    XF86VideoAdaptorPtr	textureAdaptor[2];
    int			videoKey;
    OptionInfoPtr	Options;

    Bool                LockedUp;

    CARD32              currentRop;

    DRIInfoPtr          pDRIInfo;
    drmVersionPtr       pLibDRMVersion;
    drmVersionPtr       pKernelDRMVersion;

	void *drmmode; /* for KMS */
	Bool allow_dpms;

	/* DRM interface */
	struct nouveau_device *dev;
	char drm_device_name[128];

	/* GPU context */
	struct nouveau_channel *chan;
	struct nouveau_notifier *notify0;
	struct nouveau_notifier *vblank_sem;
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
	struct nouveau_grobj *NvSW;
	struct nouveau_bo *tesla_scratch;
	struct nouveau_bo *shader_mem;
	struct nouveau_bo *xv_filtertable_mem;

	/* Acceleration context */
	PixmapPtr pspix, pmpix, pdpix;
	PicturePtr pspict, pmpict, pdpict;
	Pixel fg_colour;
	Pixel planemask;
	int alu;
	unsigned point_x, point_y;
	unsigned width_in, width_out;
	unsigned height_in, height_out;
} NVRec;

#define NVPTR(p) ((NVPtr)((p)->driverPrivate))

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
	Bool		bicubic; /* only for texture adapter */
	Bool		SyncToVBlank;
	struct nouveau_bo *video_mem;
	int		pitch;
	int		offset;
	struct nouveau_bo *TT_mem_chunk[2];
	int		currentHostBuffer;
} NVPortPrivRec, *NVPortPrivPtr;

#define GET_OVERLAY_PRIVATE(pNv) \
            (NVPortPrivPtr)((pNv)->overlayAdaptor->pPortPrivates[0].ptr)

#define GET_BLIT_PRIVATE(pNv) \
            (NVPortPrivPtr)((pNv)->blitAdaptor->pPortPrivates[0].ptr)

#define OFF_TIMER       0x01
#define FREE_TIMER      0x02
#define CLIENT_VIDEO_ON 0x04
#define OFF_DELAY       500  /* milliseconds */
#define FREE_DELAY      5000

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

/* EXA driver-controlled pixmaps */
#define NOUVEAU_CREATE_PIXMAP_ZETA 0x10000000
#define NOUVEAU_CREATE_PIXMAP_TILED 0x20000000

struct nouveau_pixmap {
	struct nouveau_bo *bo;
	void *linear;
	unsigned size;
	int map_refcount;
};

static inline struct nouveau_pixmap *
nouveau_pixmap(PixmapPtr ppix)
{
	return (struct nouveau_pixmap *)exaGetPixmapDriverPrivate(ppix);
}

Bool drmmode_is_rotate_pixmap(PixmapPtr, struct nouveau_bo **);
static inline struct nouveau_bo *
nouveau_pixmap_bo(PixmapPtr ppix)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_bo *bo;

	if (pNv->exa_driver_pixmaps) {
		struct nouveau_pixmap *nvpix = nouveau_pixmap(ppix);
		return nvpix ? nvpix->bo : NULL;
	} else
	if (ppix == pScrn->pScreen->GetScreenPixmap(pScrn->pScreen))
		return pNv->scanout;
	else
	if (drmmode_is_rotate_pixmap(ppix, &bo))
		return bo;

	return pNv->offscreen;
}

static inline unsigned
nouveau_pixmap_offset(PixmapPtr ppix)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	if (pNv->exa_driver_pixmaps ||
	    ppix == pScrn->pScreen->GetScreenPixmap(pScrn->pScreen) ||
	    drmmode_is_rotate_pixmap(ppix, NULL))
		return 0;

	return exaGetPixmapOffset(ppix);
}

static inline uint32_t
 nv_pitch_align(NVPtr pNv, uint32_t width, int bpp)
{
	int mask;

	if (bpp == 15)
	        bpp = 16;
	if (bpp == 24 || bpp == 30)
	        bpp = 8;

	/* Alignment requirements taken from the Haiku driver */
	if (pNv->Architecture == NV_ARCH_04)
	        mask = 128 / bpp - 1;
	else
	        mask = 512 / bpp - 1;

	return (width + mask) & ~mask;
}

/* nv04 cursor max dimensions of 32x32 (A1R5G5B5) */
#define NV04_CURSOR_SIZE 32
/* limit nv10 cursors to 64x64 (ARGB8) (we could go to 64x255) */
#define NV10_CURSOR_SIZE 64

static inline int nv_cursor_width(NVPtr pNv)
{
	return pNv->NVArch >= 0x10 ? NV10_CURSOR_SIZE : NV04_CURSOR_SIZE;
}

static inline int nv_cursor_pixels(NVPtr pNv)
{
	int width = nv_cursor_width(pNv);

	return width * width;
}

#endif /* __NV_STRUCT_H__ */
