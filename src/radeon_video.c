/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/radeon_video.c,v 1.30 2003/11/10 18:22:18 tsi Exp $ */

#include "radeon.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_reg.h"
#include "radeon_mergedfb.h"

#include "xf86.h"
#include "dixstruct.h"

#include "Xv.h"
#include "fourcc.h"

#define OFF_DELAY       250  /* milliseconds */
#define FREE_DELAY      15000

#define OFF_TIMER       0x01
#define FREE_TIMER      0x02
#define CLIENT_VIDEO_ON 0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

static void RADEONInitOffscreenImages(ScreenPtr);

static XF86VideoAdaptorPtr RADEONSetupImageVideo(ScreenPtr);
static int  RADEONSetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int  RADEONGetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void RADEONStopVideo(ScrnInfoPtr, pointer, Bool);
static void RADEONQueryBestSize(ScrnInfoPtr, Bool, short, short, short, short,
			unsigned int *, unsigned int *, pointer);
static int  RADEONPutImage(ScrnInfoPtr, short, short, short, short, short,
			short, short, short, int, unsigned char*, short,
			short, Bool, RegionPtr, pointer);
static int  RADEONQueryImageAttributes(ScrnInfoPtr, int, unsigned short *,
			unsigned short *,  int *, int *);

static void RADEONVideoTimerCallback(ScrnInfoPtr pScrn, Time now);


#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)
#define ClipValue(v,min,max) ((v) < (min) ? (min) : (v) > (max) ? (max) : (v))

static Atom xvBrightness, xvColorKey, xvSaturation, xvDoubleBuffer;
static Atom xvRedIntensity, xvGreenIntensity, xvBlueIntensity;
static Atom xvContrast, xvHue, xvColor, xvAutopaintColorkey, xvSetDefaults;
static Atom xvGamma, xvColorspace;
static Atom xvSwitchCRT;

typedef struct {
   CARD32	 transform_index;
   CARD32	 gamma; /* gamma value x 1000 */
   int           brightness;
   int           saturation;
   int           hue;
   int           contrast;
   int           red_intensity;
   int           green_intensity;
   int           blue_intensity;
   int		 ecp_div;

   Bool          doubleBuffer;
   unsigned char currentBuffer;
   RegionRec     clip;
   CARD32        colorKey;
   CARD32        videoStatus;
   Time          offTime;
   Time          freeTime;
   Bool          autopaint_colorkey;
   Bool		 crt2; /* 0=CRT1, 1=CRT2 */
} RADEONPortPrivRec, *RADEONPortPrivPtr;


#define GET_PORT_PRIVATE(pScrn) \
   (RADEONPortPrivPtr)((RADEONPTR(pScrn))->adaptor->pPortPrivates[0].ptr)


void RADEONInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    int num_adaptors;

    newAdaptor = RADEONSetupImageVideo(pScreen);
    RADEONInitOffscreenImages(pScreen);
    num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);

    if(newAdaptor) {
	if(!num_adaptors) {
	    num_adaptors = 1;
	    adaptors = &newAdaptor;
	} else {
	    newAdaptors =  /* need to free this someplace */
		xalloc((num_adaptors + 1) * sizeof(XF86VideoAdaptorPtr*));
	    if(newAdaptors) {
		memcpy(newAdaptors, adaptors, num_adaptors *
					sizeof(XF86VideoAdaptorPtr));
		newAdaptors[num_adaptors] = newAdaptor;
		adaptors = newAdaptors;
		num_adaptors++;
	    }
	}
    }

    if(num_adaptors)
	xf86XVScreenInit(pScreen, adaptors, num_adaptors);

    if(newAdaptors)
	xfree(newAdaptors);
}

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding =
{
   0,
   "XV_IMAGE",
   2048, 2048,
   {1, 1}
};

#define NUM_FORMATS 12

static XF86VideoFormatRec Formats[NUM_FORMATS] =
{
   {8, TrueColor}, {8, DirectColor}, {8, PseudoColor},
   {8, GrayScale}, {8, StaticGray}, {8, StaticColor},
   {15, TrueColor}, {16, TrueColor}, {24, TrueColor},
   {15, DirectColor}, {16, DirectColor}, {24, DirectColor}
};


#define NUM_ATTRIBUTES 9+6

static XF86AttributeRec Attributes[NUM_ATTRIBUTES] =
{
   {XvSettable             ,     0,    1, "XV_SET_DEFAULTS"},
   {XvSettable | XvGettable,     0,    1, "XV_AUTOPAINT_COLORKEY"},
   {XvSettable | XvGettable,     0,   ~0, "XV_COLORKEY"},
   {XvSettable | XvGettable,     0,    1, "XV_DOUBLE_BUFFER"},
   {XvSettable | XvGettable, -1000, 1000, "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, -1000, 1000, "XV_CONTRAST"},
   {XvSettable | XvGettable, -1000, 1000, "XV_SATURATION"},
   {XvSettable | XvGettable, -1000, 1000, "XV_COLOR"},
   {XvSettable | XvGettable, -1000, 1000, "XV_HUE"},
   {XvSettable | XvGettable, -1000, 1000, "XV_RED_INTENSITY"},
   {XvSettable | XvGettable, -1000, 1000, "XV_GREEN_INTENSITY"},
   {XvSettable | XvGettable, -1000, 1000, "XV_BLUE_INTENSITY"},
   {XvSettable | XvGettable,     0,    1, "XV_SWITCHCRT"},
   {XvSettable | XvGettable,   100, 10000, "XV_GAMMA"},
   {XvSettable | XvGettable,     0,    1, "XV_COLORSPACE"},
};

#define NUM_IMAGES 4

static XF86ImageRec Images[NUM_IMAGES] =
{
    XVIMAGE_YUY2,
    XVIMAGE_UYVY,
    XVIMAGE_YV12,
    XVIMAGE_I420
};

/* Reference color space transform data */
typedef struct tagREF_TRANSFORM
{
    float   RefLuma;
    float   RefRCb;
    float   RefRCr;
    float   RefGCb;
    float   RefGCr;
    float   RefBCb;
    float   RefBCr;
} REF_TRANSFORM;

/* Parameters for ITU-R BT.601 and ITU-R BT.709 colour spaces */
REF_TRANSFORM trans[2] =
{
    {1.1678, 0.0, 1.6007, -0.3929, -0.8154, 2.0232, 0.0}, /* BT.601 */
    {1.1678, 0.0, 1.7980, -0.2139, -0.5345, 2.1186, 0.0}  /* BT.709 */
};


/* Gamma curve definition for preset gammas */
typedef struct tagGAMMA_CURVE_R100
{
    CARD32 GAMMA_0_F_SLOPE;
    CARD32 GAMMA_0_F_OFFSET;
    CARD32 GAMMA_10_1F_SLOPE;
    CARD32 GAMMA_10_1F_OFFSET;
    CARD32 GAMMA_20_3F_SLOPE;
    CARD32 GAMMA_20_3F_OFFSET;
    CARD32 GAMMA_40_7F_SLOPE;
    CARD32 GAMMA_40_7F_OFFSET;
    CARD32 GAMMA_380_3BF_SLOPE;
    CARD32 GAMMA_380_3BF_OFFSET;
    CARD32 GAMMA_3C0_3FF_SLOPE;
    CARD32 GAMMA_3C0_3FF_OFFSET;
    float OvGammaCont;
} GAMMA_CURVE_R100;

typedef struct tagGAMMA_CURVE_R200
{
    CARD32 GAMMA_0_F_SLOPE;
    CARD32 GAMMA_0_F_OFFSET;
    CARD32 GAMMA_10_1F_SLOPE;
    CARD32 GAMMA_10_1F_OFFSET;
    CARD32 GAMMA_20_3F_SLOPE;
    CARD32 GAMMA_20_3F_OFFSET;
    CARD32 GAMMA_40_7F_SLOPE;
    CARD32 GAMMA_40_7F_OFFSET;
    CARD32 GAMMA_80_BF_SLOPE;
    CARD32 GAMMA_80_BF_OFFSET;
    CARD32 GAMMA_C0_FF_SLOPE;
    CARD32 GAMMA_C0_FF_OFFSET;
    CARD32 GAMMA_100_13F_SLOPE;
    CARD32 GAMMA_100_13F_OFFSET;
    CARD32 GAMMA_140_17F_SLOPE;
    CARD32 GAMMA_140_17F_OFFSET;
    CARD32 GAMMA_180_1BF_SLOPE;
    CARD32 GAMMA_180_1BF_OFFSET;
    CARD32 GAMMA_1C0_1FF_SLOPE;
    CARD32 GAMMA_1C0_1FF_OFFSET;
    CARD32 GAMMA_200_23F_SLOPE;
    CARD32 GAMMA_200_23F_OFFSET;
    CARD32 GAMMA_240_27F_SLOPE;
    CARD32 GAMMA_240_27F_OFFSET;
    CARD32 GAMMA_280_2BF_SLOPE;
    CARD32 GAMMA_280_2BF_OFFSET;
    CARD32 GAMMA_2C0_2FF_SLOPE;
    CARD32 GAMMA_2C0_2FF_OFFSET;
    CARD32 GAMMA_300_33F_SLOPE;
    CARD32 GAMMA_300_33F_OFFSET;
    CARD32 GAMMA_340_37F_SLOPE;
    CARD32 GAMMA_340_37F_OFFSET;
    CARD32 GAMMA_380_3BF_SLOPE;
    CARD32 GAMMA_380_3BF_OFFSET;
    CARD32 GAMMA_3C0_3FF_SLOPE;
    CARD32 GAMMA_3C0_3FF_OFFSET;
    float OvGammaCont;
} GAMMA_CURVE_R200;


/* Preset gammas */
GAMMA_CURVE_R100 gamma_curve_r100[8] = 
{
	/* Gamma 1.0 */
	{0x100, 0x0, 
	 0x100, 0x20, 
	 0x100, 0x40, 
	 0x100, 0x80, 
	 0x100, 0x100, 
	 0x100, 0x100, 
	 1.0},
	/* Gamma 0.85 */
	{0x75,  0x0, 
	 0xA2,  0xF,  
	 0xAC,  0x23, 
	 0xC6,  0x4E, 
	 0x129, 0xD6, 
	 0x12B, 0xD5, 
	 1.0},
	/* Gamma 1.1 */
	{0x180, 0x0, 
	 0x13C, 0x30, 
	 0x13C, 0x57, 
	 0x123, 0xA5, 
	 0xEA,  0x116, 
	 0xEA, 0x116, 
	 0.9913},
	/* Gamma 1.2 */
	{0x21B, 0x0, 
	 0x16D, 0x43, 
	 0x172, 0x71, 
	 0x13D, 0xCD, 
	 0xD9,  0x128, 
	 0xD6, 0x12A, 
	 0.9827},
	/* Gamma 1.45 */
	{0x404, 0x0, 
	 0x1B9, 0x81, 
	 0x1EE, 0xB8, 
	 0x16A, 0x133, 
	 0xB7, 0x14B, 
	 0xB2, 0x14E, 
	 0.9567},
	/* Gamma 1.7 */
	{0x658, 0x0, 
	 0x1B5, 0xCB, 
	 0x25F, 0x102, 
	 0x181, 0x199, 
	 0x9C,  0x165, 
	 0x98, 0x167, 
	 0.9394},
	/* Gamma 2.2 */
	{0x7FF, 0x0, 
	 0x625, 0x100, 
	 0x1E4, 0x1C4, 
	 0x1BD, 0x23D, 
	 0x79,  0x187, 
	 0x76,  0x188, 
	 0.9135},
	/* Gamma 2.5 */
	{0x7FF, 0x0, 
	 0x7FF, 0x100, 
	 0x2AD, 0x200, 
	 0x1A2, 0x2AB, 
	 0x6E,  0x194, 
	 0x67,  0x197, 
	 0.9135}
};

GAMMA_CURVE_R200 gamma_curve_r200[8] =
 {
	/* Gamma 1.0 */
      {0x00000040, 0x00000000,
       0x00000040, 0x00000020,
       0x00000080, 0x00000040,
       0x00000100, 0x00000080,
       0x00000100, 0x00000100,
       0x00000100, 0x00000100,
       0x00000100, 0x00000200,
       0x00000100, 0x00000200,
       0x00000100, 0x00000300,
       0x00000100, 0x00000300,
       0x00000100, 0x00000400,
       0x00000100, 0x00000400,
       0x00000100, 0x00000500,
       0x00000100, 0x00000500,
       0x00000100, 0x00000600,
       0x00000100, 0x00000600,
       0x00000100, 0x00000700,
       0x00000100, 0x00000700,
       1.0},
	/* Gamma 0.85 */
      {0x0000001D, 0x00000000,
       0x00000028, 0x0000000F,
       0x00000056, 0x00000023,
       0x000000C5, 0x0000004E,
       0x000000DA, 0x000000B0,
       0x000000E6, 0x000000AA,
       0x000000F1, 0x00000190,
       0x000000F9, 0x0000018C,
       0x00000101, 0x00000286,
       0x00000108, 0x00000282,
       0x0000010D, 0x0000038A,
       0x00000113, 0x00000387,
       0x00000118, 0x0000049A,
       0x0000011C, 0x00000498,
       0x00000120, 0x000005B4,
       0x00000124, 0x000005B2,
       0x00000128, 0x000006D6,
       0x0000012C, 0x000006D5,
       1.0},
	/* Gamma 1.1 */
      {0x00000060, 0x00000000,
       0x0000004F, 0x00000030,
       0x0000009C, 0x00000057,
       0x00000121, 0x000000A5,
       0x00000113, 0x00000136,
       0x0000010B, 0x0000013A,
       0x00000105, 0x00000245,
       0x00000100, 0x00000247,
       0x000000FD, 0x00000348,
       0x000000F9, 0x00000349,
       0x000000F6, 0x00000443,
       0x000000F4, 0x00000444,
       0x000000F2, 0x00000538,
       0x000000F0, 0x00000539,
       0x000000EE, 0x00000629,
       0x000000EC, 0x00000629,
       0x000000EB, 0x00000716,
       0x000000E9, 0x00000717,
       0.9913},
	/* Gamma 1.2 */
      {0x00000087, 0x00000000,
       0x0000005B, 0x00000043,
       0x000000B7, 0x00000071,
       0x0000013D, 0x000000CD,
       0x00000121, 0x0000016B,
       0x00000113, 0x00000172,
       0x00000107, 0x00000286,
       0x000000FF, 0x0000028A,
       0x000000F8, 0x00000389,
       0x000000F2, 0x0000038B,
       0x000000ED, 0x0000047D,
       0x000000E9, 0x00000480,
       0x000000E5, 0x00000568,
       0x000000E1, 0x0000056A,
       0x000000DE, 0x0000064B,
       0x000000DB, 0x0000064D,
       0x000000D9, 0x00000728,
       0x000000D6, 0x00000729,
       0.9827},
	/* Gamma 1.45 */
      {0x00000101, 0x00000000,
       0x0000006E, 0x00000081,
       0x000000F7, 0x000000B8,
       0x0000016E, 0x00000133,
       0x00000139, 0x000001EA,
       0x0000011B, 0x000001F9,
       0x00000105, 0x00000314,
       0x000000F6, 0x0000031C,
       0x000000E9, 0x00000411,
       0x000000DF, 0x00000417,
       0x000000D7, 0x000004F6,
       0x000000CF, 0x000004F9,
       0x000000C9, 0x000005C9,
       0x000000C4, 0x000005CC,
       0x000000BF, 0x0000068F,
       0x000000BA, 0x00000691,
       0x000000B6, 0x0000074B,
       0x000000B2, 0x0000074D,
       0.9567},
	/* Gamma 1.7 */
      {0x00000196, 0x00000000,
       0x0000006D, 0x000000CB,
       0x0000012F, 0x00000102,
       0x00000187, 0x00000199,
       0x00000144, 0x0000025b,
       0x00000118, 0x00000273,
       0x000000FE, 0x0000038B,
       0x000000E9, 0x00000395,
       0x000000DA, 0x0000047E,
       0x000000CE, 0x00000485,
       0x000000C3, 0x00000552,
       0x000000BB, 0x00000556,
       0x000000B3, 0x00000611,
       0x000000AC, 0x00000614,
       0x000000A7, 0x000006C1,
       0x000000A1, 0x000006C3,
       0x0000009D, 0x00000765,
       0x00000098, 0x00000767,
       0.9394},
	/* Gamma 2.2 */
      {0x000001FF, 0x00000000,
       0x0000018A, 0x00000100,
       0x000000F1, 0x000001C5,
       0x000001D6, 0x0000023D,
       0x00000124, 0x00000328,
       0x00000116, 0x0000032F,
       0x000000E2, 0x00000446,
       0x000000D3, 0x0000044D,
       0x000000BC, 0x00000520,
       0x000000B0, 0x00000526,
       0x000000A4, 0x000005D6,
       0x0000009B, 0x000005DB,
       0x00000092, 0x00000676,
       0x0000008B, 0x00000679,
       0x00000085, 0x00000704,
       0x00000080, 0x00000707,
       0x0000007B, 0x00000787,
       0x00000076, 0x00000789,
       0.9135},
	/* Gamma 2.5 */
      {0x000001FF, 0x00000000,
       0x000001FF, 0x00000100,
       0x00000159, 0x000001FF,
       0x000001AC, 0x000002AB,
       0x0000012F, 0x00000381,
       0x00000101, 0x00000399,
       0x000000D9, 0x0000049A,
       0x000000C3, 0x000004A5,
       0x000000AF, 0x00000567,
       0x000000A1, 0x0000056E,
       0x00000095, 0x00000610,
       0x0000008C, 0x00000614,
       0x00000084, 0x000006A0,
       0x0000007D, 0x000006A4,
       0x00000077, 0x00000721,
       0x00000071, 0x00000723,
       0x0000006D, 0x00000795,
       0x00000068, 0x00000797,
       0.9135}
};

static void
RADEONSetOverlayGamma(ScrnInfoPtr pScrn, CARD32 gamma)
{
    RADEONInfoPtr    info = RADEONPTR(pScrn);
    unsigned char   *RADEONMMIO = info->MMIO;
    CARD32	    ov0_scale_cntl;

    /* Set gamma */
    ov0_scale_cntl = INREG(RADEON_OV0_SCALE_CNTL) & ~RADEON_SCALER_GAMMA_SEL_MASK;
    OUTREG(RADEON_OV0_SCALE_CNTL, ov0_scale_cntl | (gamma << 0x00000005));

    /* Load gamma curve adjustments */
    if (info->ChipFamily >= CHIP_FAMILY_R200) {
    	OUTREG(RADEON_OV0_GAMMA_000_00F,
	    (gamma_curve_r200[gamma].GAMMA_0_F_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_0_F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_010_01F,
	    (gamma_curve_r200[gamma].GAMMA_10_1F_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_10_1F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_020_03F,
	    (gamma_curve_r200[gamma].GAMMA_20_3F_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_20_3F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_040_07F,
	    (gamma_curve_r200[gamma].GAMMA_40_7F_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_40_7F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_080_0BF,
	    (gamma_curve_r200[gamma].GAMMA_80_BF_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_80_BF_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_0C0_0FF,
	    (gamma_curve_r200[gamma].GAMMA_C0_FF_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_C0_FF_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_100_13F,
	    (gamma_curve_r200[gamma].GAMMA_100_13F_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_100_13F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_140_17F,
	    (gamma_curve_r200[gamma].GAMMA_140_17F_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_140_17F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_180_1BF,
	    (gamma_curve_r200[gamma].GAMMA_180_1BF_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_180_1BF_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_1C0_1FF,
	    (gamma_curve_r200[gamma].GAMMA_1C0_1FF_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_1C0_1FF_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_200_23F,
	    (gamma_curve_r200[gamma].GAMMA_200_23F_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_200_23F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_240_27F,
	    (gamma_curve_r200[gamma].GAMMA_240_27F_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_240_27F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_280_2BF,
	    (gamma_curve_r200[gamma].GAMMA_280_2BF_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_280_2BF_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_2C0_2FF,
	    (gamma_curve_r200[gamma].GAMMA_2C0_2FF_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_2C0_2FF_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_300_33F,
	    (gamma_curve_r200[gamma].GAMMA_300_33F_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_300_33F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_340_37F,
	    (gamma_curve_r200[gamma].GAMMA_340_37F_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_340_37F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_380_3BF,
	    (gamma_curve_r200[gamma].GAMMA_380_3BF_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_380_3BF_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_3C0_3FF,
	    (gamma_curve_r200[gamma].GAMMA_3C0_3FF_OFFSET << 0x00000000) |
	    (gamma_curve_r200[gamma].GAMMA_3C0_3FF_SLOPE << 0x00000010));
    } else {
    	OUTREG(RADEON_OV0_GAMMA_000_00F,
	    (gamma_curve_r100[gamma].GAMMA_0_F_OFFSET << 0x00000000) |
	    (gamma_curve_r100[gamma].GAMMA_0_F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_010_01F,
	    (gamma_curve_r100[gamma].GAMMA_10_1F_OFFSET << 0x00000000) |
	    (gamma_curve_r100[gamma].GAMMA_10_1F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_020_03F,
	    (gamma_curve_r100[gamma].GAMMA_20_3F_OFFSET << 0x00000000) |
	    (gamma_curve_r100[gamma].GAMMA_20_3F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_040_07F,
	    (gamma_curve_r100[gamma].GAMMA_40_7F_OFFSET << 0x00000000) |
	    (gamma_curve_r100[gamma].GAMMA_40_7F_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_380_3BF,
	    (gamma_curve_r100[gamma].GAMMA_380_3BF_OFFSET << 0x00000000) |
	    (gamma_curve_r100[gamma].GAMMA_380_3BF_SLOPE << 0x00000010));
    	OUTREG(RADEON_OV0_GAMMA_3C0_3FF,
	    (gamma_curve_r100[gamma].GAMMA_3C0_3FF_OFFSET << 0x00000000) |
	    (gamma_curve_r100[gamma].GAMMA_3C0_3FF_SLOPE << 0x00000010));
    }

}


/****************************************************************************
 * SetTransform                                                             *
 *  Function: Calculates and sets color space transform from supplied       *
 *            reference transform, gamma, brightness, contrast, hue and     *
 *            saturation.                                                   *
 *    Inputs: bright - brightness                                           *
 *            cont - contrast                                               *
 *            sat - saturation                                              *
 *            hue - hue                                                     *
 *            red_intensity - intensity of red component                    *
 *            green_intensity - intensity of green component                *
 *            blue_intensity - intensity of blue component                  *
 *            ref - index to the table of refernce transforms               *
 *            user_gamma - gamma value x 1000 (e.g., 1200 = gamma of 1.2)   *
 *   Outputs: NONE                                                          *
 ****************************************************************************/

static void RADEONSetTransform (ScrnInfoPtr pScrn,
				float	    bright,
				float	    cont,
				float	    sat,
				float	    hue,
				float	    red_intensity,
				float	    green_intensity,
				float	    blue_intensity,
				CARD32	    ref,
				CARD32      user_gamma)
{
    RADEONInfoPtr    info = RADEONPTR(pScrn);
    unsigned char   *RADEONMMIO = info->MMIO;
    float	    OvHueSin, OvHueCos;
    float	    CAdjLuma, CAdjOff;
    float	    CAdjRCb, CAdjRCr;
    float	    CAdjGCb, CAdjGCr;
    float	    CAdjBCb, CAdjBCr;
    float	    RedAdj,GreenAdj,BlueAdj;
    float	    OvLuma, OvROff, OvGOff, OvBOff;
    float	    OvRCb, OvRCr;
    float	    OvGCb, OvGCr;
    float	    OvBCb, OvBCr;
    float	    Loff = 64.0;
    float	    Coff = 512.0f;

    CARD32	    dwOvLuma, dwOvROff, dwOvGOff, dwOvBOff;
    CARD32	    dwOvRCb, dwOvRCr;
    CARD32	    dwOvGCb, dwOvGCr;
    CARD32	    dwOvBCb, dwOvBCr;
    CARD32	    gamma = 0;

    if (ref >= 2)
	return;

    /* translate from user_gamma (gamma x 1000) to radeon gamma table index value */
    if (user_gamma <= 925)       /* 0.85 */
	gamma = 1;
    else if (user_gamma <= 1050) /* 1.0  */
	gamma = 0;
    else if (user_gamma <= 1150) /* 1.1  */
	gamma = 2;
    else if (user_gamma <= 1325) /* 1.2  */
	gamma = 3;
    else if (user_gamma <= 1575) /* 1.45 */
	gamma = 4;
    else if (user_gamma <= 1950) /* 1.7  */
	gamma = 5;
    else if (user_gamma <= 2350) /* 2.2  */
	gamma = 6;
    else if (user_gamma > 2350)  /* 2.5  */
	gamma = 7;

    if (gamma >= 8) 
	return;

    OvHueSin = sin(hue);
    OvHueCos = cos(hue);

    CAdjLuma = cont * trans[ref].RefLuma;
    CAdjOff = cont * trans[ref].RefLuma * bright * 1023.0;
    RedAdj = cont * trans[ref].RefLuma * red_intensity * 1023.0;
    GreenAdj = cont * trans[ref].RefLuma * green_intensity * 1023.0;
    BlueAdj = cont * trans[ref].RefLuma * blue_intensity * 1023.0;

    CAdjRCb = sat * -OvHueSin * trans[ref].RefRCr;
    CAdjRCr = sat * OvHueCos * trans[ref].RefRCr;
    CAdjGCb = sat * (OvHueCos * trans[ref].RefGCb - OvHueSin * trans[ref].RefGCr);
    CAdjGCr = sat * (OvHueSin * trans[ref].RefGCb + OvHueCos * trans[ref].RefGCr);
    CAdjBCb = sat * OvHueCos * trans[ref].RefBCb;
    CAdjBCr = sat * OvHueSin * trans[ref].RefBCb;

#if 0 /* default constants */
    CAdjLuma = 1.16455078125;

    CAdjRCb = 0.0;
    CAdjRCr = 1.59619140625;
    CAdjGCb = -0.39111328125;
    CAdjGCr = -0.8125;
    CAdjBCb = 2.01708984375;
    CAdjBCr = 0;
#endif

    OvLuma = CAdjLuma * gamma_curve_r100[gamma].OvGammaCont;
    OvRCb = CAdjRCb * gamma_curve_r100[gamma].OvGammaCont;
    OvRCr = CAdjRCr * gamma_curve_r100[gamma].OvGammaCont;
    OvGCb = CAdjGCb * gamma_curve_r100[gamma].OvGammaCont;
    OvGCr = CAdjGCr * gamma_curve_r100[gamma].OvGammaCont;
    OvBCb = CAdjBCb * gamma_curve_r100[gamma].OvGammaCont;
    OvBCr = CAdjBCr * gamma_curve_r100[gamma].OvGammaCont;
    OvROff = CAdjOff * gamma_curve_r100[gamma].OvGammaCont - 
	OvLuma * Loff - (OvRCb + OvRCr) * Coff;
    OvGOff = CAdjOff * gamma_curve_r100[gamma].OvGammaCont - 
	OvLuma * Loff - (OvGCb + OvGCr) * Coff;
    OvBOff = CAdjOff * gamma_curve_r100[gamma].OvGammaCont - 
	OvLuma * Loff - (OvBCb + OvBCr) * Coff;
#if 0 /* default constants */
    OvROff = -888.5;
    OvGOff = 545;
    OvBOff = -1104;
#endif

    OvROff = ClipValue(OvROff, -2048.0, 2047.5);
    OvGOff = ClipValue(OvGOff, -2048.0, 2047.5);
    OvBOff = ClipValue(OvBOff, -2048.0, 2047.5);
    dwOvROff = ((INT32)(OvROff * 2.0)) & 0x1fff;
    dwOvGOff = ((INT32)(OvGOff * 2.0)) & 0x1fff;
    dwOvBOff = ((INT32)(OvBOff * 2.0)) & 0x1fff;

    if(info->ChipFamily < CHIP_FAMILY_RADEON)
    {
	dwOvLuma =(((INT32)(OvLuma * 2048.0))&0x7fff)<<17;
	dwOvRCb = (((INT32)(OvRCb * 2048.0))&0x7fff)<<1;
	dwOvRCr = (((INT32)(OvRCr * 2048.0))&0x7fff)<<17;
	dwOvGCb = (((INT32)(OvGCb * 2048.0))&0x7fff)<<1;
	dwOvGCr = (((INT32)(OvGCr * 2048.0))&0x7fff)<<17;
	dwOvBCb = (((INT32)(OvBCb * 2048.0))&0x7fff)<<1;
	dwOvBCr = (((INT32)(OvBCr * 2048.0))&0x7fff)<<17;
    }
    else
    {
	dwOvLuma = (((INT32)(OvLuma * 256.0))&0xfff)<<20;
	dwOvRCb = (((INT32)(OvRCb * 256.0))&0xfff)<<4;
	dwOvRCr = (((INT32)(OvRCr * 256.0))&0xfff)<<20;
	dwOvGCb = (((INT32)(OvGCb * 256.0))&0xfff)<<4;
	dwOvGCr = (((INT32)(OvGCr * 256.0))&0xfff)<<20;
	dwOvBCb = (((INT32)(OvBCb * 256.0))&0xfff)<<4;
	dwOvBCr = (((INT32)(OvBCr * 256.0))&0xfff)<<20;
    }

    /* set gamma */
    RADEONSetOverlayGamma(pScrn, gamma);

    /* color transforms */
    OUTREG(RADEON_OV0_LIN_TRANS_A, dwOvRCb | dwOvLuma);
    OUTREG(RADEON_OV0_LIN_TRANS_B, dwOvROff | dwOvRCr);
    OUTREG(RADEON_OV0_LIN_TRANS_C, dwOvGCb | dwOvLuma);
    OUTREG(RADEON_OV0_LIN_TRANS_D, dwOvGOff | dwOvGCr);
    OUTREG(RADEON_OV0_LIN_TRANS_E, dwOvBCb | dwOvLuma);
    OUTREG(RADEON_OV0_LIN_TRANS_F, dwOvBOff | dwOvBCr);
}

static void RADEONSetColorKey(ScrnInfoPtr pScrn, CARD32 colorKey)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 min, max;
    CARD8 r, g, b;

    if (info->CurrentLayout.depth > 8)
    {
	CARD32	rbits, gbits, bbits;

	rbits = (colorKey & pScrn->mask.red) >> pScrn->offset.red;
	gbits = (colorKey & pScrn->mask.green) >> pScrn->offset.green;
	bbits = (colorKey & pScrn->mask.blue) >> pScrn->offset.blue;

	r = rbits << (8 - pScrn->weight.red);
	g = gbits << (8 - pScrn->weight.green);
	b = bbits << (8 - pScrn->weight.blue);
    }
    else
    {
	CARD32	bits;

	bits = colorKey & ((1 << info->CurrentLayout.depth) - 1);
	r = bits;
	g = bits;
	b = bits;
    }
    min = (r << 16) | (g << 8) | (b);
    max = (0xff << 24) | (r << 16) | (g << 8) | (b);

    RADEONWaitForFifo(pScrn, 2);
    OUTREG(RADEON_OV0_GRAPHICS_KEY_CLR_HIGH, max);
    OUTREG(RADEON_OV0_GRAPHICS_KEY_CLR_LOW, min);
}

void
RADEONResetVideo(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr   info      = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONPortPrivPtr pPriv = info->adaptor->pPortPrivates[0].ptr;

    /* this function is called from ScreenInit. pScreen is used 
       by XAA internally, but not valid until ScreenInit finishs.
    */
    if (info->accelOn && pScrn->pScreen) info->accel->Sync(pScrn);

    OUTREG(RADEON_OV0_SCALE_CNTL, 0x80000000);
    OUTREG(RADEON_OV0_AUTO_FLIP_CNTL, 0);   /* maybe */
    OUTREG(RADEON_OV0_EXCLUSIVE_HORZ, 0);
    OUTREG(RADEON_OV0_FILTER_CNTL, 0x0000000f);
    OUTREG(RADEON_OV0_KEY_CNTL, RADEON_GRAPHIC_KEY_FN_EQ |
				RADEON_VIDEO_KEY_FN_FALSE |
				RADEON_CMP_MIX_OR);
    OUTREG(RADEON_OV0_TEST, 0);
    OUTREG(RADEON_FCP_CNTL, RADEON_FCP0_SRC_GND);
    OUTREG(RADEON_CAP0_TRIG_CNTL, 0);
    RADEONSetColorKey(pScrn, pPriv->colorKey);

    if (info->ChipFamily == CHIP_FAMILY_RADEON) {

	OUTREG(RADEON_OV0_LIN_TRANS_A, 0x12a00000);
	OUTREG(RADEON_OV0_LIN_TRANS_B, 0x1990190e);
	OUTREG(RADEON_OV0_LIN_TRANS_C, 0x12a0f9c0);
	OUTREG(RADEON_OV0_LIN_TRANS_D, 0xf3000442);
	OUTREG(RADEON_OV0_LIN_TRANS_E, 0x12a02040);
	OUTREG(RADEON_OV0_LIN_TRANS_F, 0x175f);

    } else {

	OUTREG(RADEON_OV0_LIN_TRANS_A, 0x12a20000);
	OUTREG(RADEON_OV0_LIN_TRANS_B, 0x198a190e);
	OUTREG(RADEON_OV0_LIN_TRANS_C, 0x12a2f9da);
	OUTREG(RADEON_OV0_LIN_TRANS_D, 0xf2fe0442);
	OUTREG(RADEON_OV0_LIN_TRANS_E, 0x12a22046);
	OUTREG(RADEON_OV0_LIN_TRANS_F, 0x175f);
    }
	/*
	 * Set default Gamma ramp:
	 *
	 * Of 18 segments for gamma curve, all segments in R200 (and
	 * newer) are programmable, while only lower 4 and upper 2
	 * segments are programmable in the older Radeons.
	 */

    RADEONSetOverlayGamma(pScrn, 0); /* gamma = 1.0 */

}


static XF86VideoAdaptorPtr
RADEONAllocAdaptor(ScrnInfoPtr pScrn)
{
    XF86VideoAdaptorPtr adapt;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONPortPrivPtr pPriv;
    unsigned char *RADEONMMIO = info->MMIO;
    CARD32 dot_clock;

    if(!(adapt = xf86XVAllocateVideoAdaptorRec(pScrn)))
	return NULL;

    if(!(pPriv = xcalloc(1, sizeof(RADEONPortPrivRec) + sizeof(DevUnion))))
    {
	xfree(adapt);
	return NULL;
    }

    adapt->pPortPrivates = (DevUnion*)(&pPriv[1]);
    adapt->pPortPrivates[0].ptr = (pointer)pPriv;

    pPriv->colorKey = info->videoKey;
    pPriv->doubleBuffer = TRUE;
    pPriv->videoStatus = 0;
    pPriv->brightness = 0;
    pPriv->transform_index = 0;
    pPriv->saturation = 0;
    pPriv->contrast = 0;
    pPriv->red_intensity = 0;
    pPriv->green_intensity = 0;
    pPriv->blue_intensity = 0;
    pPriv->hue = 0;
    pPriv->currentBuffer = 0;
    pPriv->autopaint_colorkey = TRUE;
    pPriv->gamma = 1000;
    if (info->OverlayOnCRTC2)
	pPriv->crt2 = TRUE;
    else
	pPriv->crt2 = FALSE;

    /*
     * Unlike older Mach64 chips, RADEON has only two ECP settings:
     * 0 for PIXCLK < 175Mhz, and 1 (divide by 2)
     * for higher clocks, sure makes life nicer
     */

    /* Figure out which head we are on */
    if ((info->MergedFB && info->OverlayOnCRTC2) || info->IsSecondary)
	dot_clock = info->ModeReg.dot_clock_freq_2;
    else
	dot_clock = info->ModeReg.dot_clock_freq;

    if(dot_clock < 17500)
        pPriv->ecp_div = 0;
    else
        pPriv->ecp_div = 1;


#if 0
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Dotclock is %g Mhz, setting ecp_div to %d\n", info->ModeReg.dot_clock_freq/100.0, pPriv->ecp_div);
#endif

    OUTPLL(RADEON_VCLK_ECP_CNTL, (INPLL(pScrn, RADEON_VCLK_ECP_CNTL) &
				  0xfffffCff) | (pPriv->ecp_div << 8));

    /* I suspect we may need a usleep after writing to the PLL.  if you play a video too soon
       after switching crtcs in mergedfb clone mode you get a temporary one pixel line of colorkey 
       on the right edge video output.  */


    if ((info->ChipFamily == CHIP_FAMILY_RS100) || 
	(info->ChipFamily == CHIP_FAMILY_RS200) ||
	(info->ChipFamily == CHIP_FAMILY_RS300)) {
        /* Force the overlay clock on for integrated chips
	 */
        OUTPLL(RADEON_VCLK_ECP_CNTL, (INPLL(pScrn, RADEON_VCLK_ECP_CNTL) | (1<<18)));
    }

    info->adaptor = adapt;

    return adapt;
}

static XF86VideoAdaptorPtr
RADEONSetupImageVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    RADEONPortPrivPtr pPriv;
    XF86VideoAdaptorPtr adapt;

    if(!(adapt = RADEONAllocAdaptor(pScrn)))
	return NULL;

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name = "ATI Radeon Video Overlay";
    adapt->nEncodings = 1;
    adapt->pEncodings = &DummyEncoding;
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = Formats;
    adapt->nPorts = 1;
    adapt->nAttributes = NUM_ATTRIBUTES;
    adapt->pAttributes = Attributes;
    adapt->nImages = NUM_IMAGES;
    adapt->pImages = Images;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = RADEONStopVideo;
    adapt->SetPortAttribute = RADEONSetPortAttribute;
    adapt->GetPortAttribute = RADEONGetPortAttribute;
    adapt->QueryBestSize = RADEONQueryBestSize;
    adapt->PutImage = RADEONPutImage;
    adapt->QueryImageAttributes = RADEONQueryImageAttributes;

    pPriv = (RADEONPortPrivPtr)(adapt->pPortPrivates[0].ptr);
    REGION_NULL(pScreen, &(pPriv->clip));

    xvBrightness   = MAKE_ATOM("XV_BRIGHTNESS");
    xvSaturation   = MAKE_ATOM("XV_SATURATION");
    xvColor        = MAKE_ATOM("XV_COLOR");
    xvContrast     = MAKE_ATOM("XV_CONTRAST");
    xvColorKey     = MAKE_ATOM("XV_COLORKEY");
    xvDoubleBuffer = MAKE_ATOM("XV_DOUBLE_BUFFER");
    xvHue          = MAKE_ATOM("XV_HUE");
    xvRedIntensity   = MAKE_ATOM("XV_RED_INTENSITY");
    xvGreenIntensity = MAKE_ATOM("XV_GREEN_INTENSITY");
    xvBlueIntensity  = MAKE_ATOM("XV_BLUE_INTENSITY");
    xvGamma          = MAKE_ATOM("XV_GAMMA");
    xvColorspace     = MAKE_ATOM("XV_COLORSPACE");

    xvAutopaintColorkey = MAKE_ATOM("XV_AUTOPAINT_COLORKEY");
    xvSetDefaults = MAKE_ATOM("XV_SET_DEFAULTS");
    xvSwitchCRT = MAKE_ATOM("XV_SWITCHCRT");
    
    RADEONResetVideo(pScrn);

    return adapt;
}

static void
RADEONStopVideo(ScrnInfoPtr pScrn, pointer data, Bool cleanup)
{
  RADEONInfoPtr info = RADEONPTR(pScrn);
  unsigned char *RADEONMMIO = info->MMIO;
  RADEONPortPrivPtr pPriv = (RADEONPortPrivPtr)data;

  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);

  if(cleanup) {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
	RADEONWaitForFifo(pScrn, 2);
	OUTREG(RADEON_OV0_SCALE_CNTL, 0);
     }
     if(info->videoLinear) {
	xf86FreeOffscreenLinear(info->videoLinear);
	info->videoLinear = NULL;
     }
     pPriv->videoStatus = 0;
  } else {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
	pPriv->videoStatus |= OFF_TIMER;
	pPriv->offTime = currentTime.milliseconds + OFF_DELAY;
     }
  }
}

static int
RADEONSetPortAttribute(ScrnInfoPtr  pScrn,
		       Atom	    attribute,
		       INT32	    value,
		       pointer	    data)
{
    RADEONInfoPtr	info = RADEONPTR(pScrn);
    RADEONPortPrivPtr	pPriv = (RADEONPortPrivPtr)data;
    Bool		setTransform = FALSE;

    info->accel->Sync(pScrn);

#define RTFSaturation(a)   (1.0 + ((a)*1.0)/1000.0)
#define RTFBrightness(a)   (((a)*1.0)/2000.0)
#define RTFIntensity(a)   (((a)*1.0)/2000.0)
#define RTFContrast(a)   (1.0 + ((a)*1.0)/1000.0)
#define RTFHue(a)   (((a)*3.1416)/1000.0)

    if(attribute == xvAutopaintColorkey)
    {
	pPriv->autopaint_colorkey = ClipValue (value, 0, 1);
    }
    else if(attribute == xvSetDefaults)
    {
	pPriv->autopaint_colorkey = TRUE;
	pPriv->brightness = 0;
	pPriv->saturation = 0;
	pPriv->contrast = 0;
	pPriv->hue = 0;
	pPriv->red_intensity = 0;
	pPriv->green_intensity = 0;
	pPriv->blue_intensity = 0;
	pPriv->gamma = 1000;
	pPriv->transform_index = 0;
	pPriv->doubleBuffer = FALSE;
	setTransform = TRUE;
    }
    else if(attribute == xvBrightness)
    {
	pPriv->brightness = ClipValue (value, -1000, 1000);
	setTransform = TRUE;
    }
    else if((attribute == xvSaturation) || (attribute == xvColor))
    {
	pPriv->saturation = ClipValue (value, -1000, 1000);
	setTransform = TRUE;
    }
    else if(attribute == xvContrast)
    {
	pPriv->contrast = ClipValue (value, -1000, 1000);
	setTransform = TRUE;
    }
    else if(attribute == xvHue)
    {
	pPriv->hue = ClipValue (value, -1000, 1000);
	setTransform = TRUE;
    }
    else if(attribute == xvRedIntensity)
    {
	pPriv->red_intensity = ClipValue (value, -1000, 1000);
	setTransform = TRUE;
    }
    else if(attribute == xvGreenIntensity)
    {
	pPriv->green_intensity = ClipValue (value, -1000, 1000);
	setTransform = TRUE;
    }
    else if(attribute == xvBlueIntensity)
    {
	pPriv->blue_intensity = ClipValue (value, -1000, 1000);
	setTransform = TRUE;
    }
    else if(attribute == xvGamma) 
    {
	pPriv->gamma = ClipValue (value, 100, 10000);
	setTransform = TRUE;
    } 
    else if(attribute == xvColorspace) 
    {
	pPriv->transform_index = ClipValue (value, 0, 1);
	setTransform = TRUE;
    } 
    else if(attribute == xvDoubleBuffer)
    {
	pPriv->doubleBuffer = ClipValue (value, 0, 1);
	pPriv->doubleBuffer = value;
    }
    else if(attribute == xvColorKey)
    {
	pPriv->colorKey = value;
	RADEONSetColorKey (pScrn, pPriv->colorKey);
	REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
    } 
    else if(attribute == xvSwitchCRT) 
    {
	pPriv->crt2 = ClipValue (value, 0, 1);
	pPriv->crt2 = value;
	if (pPriv->crt2)
	    info->OverlayOnCRTC2 = TRUE;
	else
	    info->OverlayOnCRTC2 = FALSE; 
    } 
    else 
	return BadMatch;

    if (setTransform)
    {
	RADEONSetTransform(pScrn,
			   RTFBrightness(pPriv->brightness),
			   RTFContrast(pPriv->contrast),
			   RTFSaturation(pPriv->saturation),
			   RTFHue(pPriv->hue),
			   RTFIntensity(pPriv->red_intensity),
			   RTFIntensity(pPriv->green_intensity),
			   RTFIntensity(pPriv->blue_intensity),
			   pPriv->transform_index,
			   pPriv->gamma);
    }

    return Success;
}

static int
RADEONGetPortAttribute(ScrnInfoPtr  pScrn,
		       Atom	    attribute,
		       INT32	    *value,
		       pointer	    data)
{
    RADEONInfoPtr	info = RADEONPTR(pScrn);
    RADEONPortPrivPtr	pPriv = (RADEONPortPrivPtr)data;

    if (info->accelOn) info->accel->Sync(pScrn);

    if(attribute == xvAutopaintColorkey)
	*value = pPriv->autopaint_colorkey;
    else if(attribute == xvBrightness)
	*value = pPriv->brightness;
    else if((attribute == xvSaturation) || (attribute == xvColor))
	*value = pPriv->saturation;
    else if(attribute == xvContrast)
	*value = pPriv->contrast;
    else if(attribute == xvHue)
	*value = pPriv->hue;
    else if(attribute == xvRedIntensity)
	*value = pPriv->red_intensity;
    else if(attribute == xvGreenIntensity)
	*value = pPriv->green_intensity;
    else if(attribute == xvBlueIntensity)
	*value = pPriv->blue_intensity;
    else if(attribute == xvGamma)
	*value = pPriv->gamma;
    else if(attribute == xvColorspace)
	*value = pPriv->transform_index;
    else if(attribute == xvDoubleBuffer)
	*value = pPriv->doubleBuffer ? 1 : 0;
    else if(attribute == xvColorKey)
	*value = pPriv->colorKey;
    else if(attribute == xvSwitchCRT)
	*value = pPriv->crt2 ? 1 : 0;
    else 
	return BadMatch;

    return Success;
}

static void
RADEONQueryBestSize(
  ScrnInfoPtr pScrn,
  Bool motion,
  short vid_w, short vid_h,
  short drw_w, short drw_h,
  unsigned int *p_w, unsigned int *p_h,
  pointer data
){
   if(vid_w > (drw_w << 4))
	drw_w = vid_w >> 4;
   if(vid_h > (drw_h << 4))
	drw_h = vid_h >> 4;

  *p_w = drw_w;
  *p_h = drw_h;
}

static void
RADEONCopyData(
  unsigned char *src,
  unsigned char *dst,
  int srcPitch,
  int dstPitch,
  int h,
  int w
){
    w <<= 1;
    while(h--) {
	memcpy(dst, src, w);
	src += srcPitch;
	dst += dstPitch;
    }
}

static void
RADEONCopyMungedData(
   unsigned char *src1,
   unsigned char *src2,
   unsigned char *src3,
   unsigned char *dst1,
   int srcPitch,
   int srcPitch2,
   int dstPitch,
   int h,
   int w
){
   CARD32 *dst;
   CARD8 *s1, *s2, *s3;
   int i, j;

   w >>= 1;

   for(j = 0; j < h; j++) {
	dst = (pointer)dst1;
	s1 = src1;  s2 = src2;  s3 = src3;
	i = w;
	while(i > 4) {
	   dst[0] = s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24);
	   dst[1] = s1[2] | (s1[3] << 16) | (s3[1] << 8) | (s2[1] << 24);
	   dst[2] = s1[4] | (s1[5] << 16) | (s3[2] << 8) | (s2[2] << 24);
	   dst[3] = s1[6] | (s1[7] << 16) | (s3[3] << 8) | (s2[3] << 24);
	   dst += 4; s2 += 4; s3 += 4; s1 += 8;
	   i -= 4;
	}
	while(i--) {
	   dst[0] = s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24);
	   dst++; s2++; s3++;
	   s1 += 2;
	}

	dst1 += dstPitch;
	src1 += srcPitch;
	if(j & 1) {
	    src2 += srcPitch2;
	    src3 += srcPitch2;
	}
   }
}


static FBLinearPtr
RADEONAllocateMemory(
   ScrnInfoPtr pScrn,
   FBLinearPtr linear,
   int size
){
   ScreenPtr pScreen;
   FBLinearPtr new_linear;

   if(linear) {
	if(linear->size >= size)
	   return linear;

	if(xf86ResizeOffscreenLinear(linear, size))
	   return linear;

	xf86FreeOffscreenLinear(linear);
   }

   pScreen = screenInfo.screens[pScrn->scrnIndex];

   new_linear = xf86AllocateOffscreenLinear(pScreen, size, 16,
						NULL, NULL, NULL);

   if(!new_linear) {
	int max_size;

	xf86QueryLargestOffscreenLinear(pScreen, &max_size, 16,
						PRIORITY_EXTREME);

	if(max_size < size)
	   return NULL;

	xf86PurgeUnlockedOffscreenAreas(pScreen);
	new_linear = xf86AllocateOffscreenLinear(pScreen, size, 16,
						NULL, NULL, NULL);
   }

   return new_linear;
}

static void
RADEONDisplayVideo(
    ScrnInfoPtr pScrn,
    int id,
    int offset1, int offset2,
    short width, short height,
    int pitch,
    int left, int right, int top,
    BoxPtr dstBox,
    short src_w, short src_h,
    short drw_w, short drw_h
){
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    int v_inc, h_inc, step_by, tmp;
    int p1_h_accum_init, p23_h_accum_init;
    int p1_v_accum_init;
    int ecp_div;
    int v_inc_shift;
    int y_mult;
    int x_off;
    int y_off;
    CARD32 scaler_src;
    CARD32 dot_clock;
    DisplayModePtr overlay_mode;

    /* Unlike older Mach64 chips, RADEON has only two ECP settings: 0 for PIXCLK < 175Mhz, and 1 (divide by 2)
       for higher clocks, sure makes life nicer

       Here we need to find ecp_div again, as the user may have switched resolutions */

    /* Figure out which head we are on for dot clock */
    if ((info->MergedFB && info->OverlayOnCRTC2) || info->IsSecondary)
        dot_clock = info->ModeReg.dot_clock_freq_2;
    else
        dot_clock = info->ModeReg.dot_clock_freq;

    if (dot_clock < 17500)
        ecp_div = 0;
    else
	ecp_div = 1;

    OUTPLL(RADEON_VCLK_ECP_CNTL, (INPLL(pScrn, RADEON_VCLK_ECP_CNTL) & 0xfffffCff) | (ecp_div << 8));

    /* I suspect we may need a usleep after writing to the PLL.  if you play a video too soon
       after switching crtcs in mergedfb clone mode you get a temporary one pixel line of colorkey 
       on the right edge video output.  */

    v_inc_shift = 20;
    y_mult = 1;

    if (info->MergedFB) {
        if (info->OverlayOnCRTC2)
	    overlay_mode = ((RADEONMergedDisplayModePtr)info->CurrentLayout.mode->Private)->CRT2;
	else
	    overlay_mode = ((RADEONMergedDisplayModePtr)info->CurrentLayout.mode->Private)->CRT1;
	if (overlay_mode->Flags & V_INTERLACE)
	    v_inc_shift++;
    	if (overlay_mode->Flags & V_DBLSCAN) {
	    v_inc_shift--;
	    y_mult = 2;
	}
    	if (overlay_mode->Flags & RADEON_USE_RMX) {
	    v_inc = ((src_h * overlay_mode->CrtcVDisplay / info->PanelYRes) << v_inc_shift) / drw_h;
    	} else {
	    v_inc = (src_h << v_inc_shift) / drw_h;
    	}
    } else {
	if (pScrn->currentMode->Flags & V_INTERLACE)
	    v_inc_shift++;
    	if (pScrn->currentMode->Flags & V_DBLSCAN) {
	    v_inc_shift--;
	    y_mult = 2;
	}
    	if (pScrn->currentMode->Flags & RADEON_USE_RMX) {
	    v_inc = ((src_h * pScrn->currentMode->CrtcVDisplay / info->PanelYRes) << v_inc_shift) / drw_h;
    	} else {
	    v_inc = (src_h << v_inc_shift) / drw_h;
    	}
    }
    h_inc = ((src_w << (12 + ecp_div)) / drw_w);
    step_by = 1;

    while(h_inc >= (2 << 12)) {
	step_by++;
	h_inc >>= 1;
    }

    /* keep everything in 16.16 */

    offset1 += ((left >> 16) & ~7) << 1;
    offset2 += ((left >> 16) & ~7) << 1;

    if (info->IsSecondary) {
	offset1 += info->FbMapSize;
	offset2 += info->FbMapSize;
    }

    tmp = (left & 0x0003ffff) + 0x00028000 + (h_inc << 3);
    p1_h_accum_init = ((tmp <<  4) & 0x000f8000) |
		      ((tmp << 12) & 0xf0000000);

    tmp = ((left >> 1) & 0x0001ffff) + 0x00028000 + (h_inc << 2);
    p23_h_accum_init = ((tmp <<  4) & 0x000f8000) |
		       ((tmp << 12) & 0x70000000);

    tmp = (top & 0x0000ffff) + 0x00018000;
    p1_v_accum_init = ((tmp << 4) & 0x03ff8000) | 0x00000001;

    left = (left >> 16) & 7;

    RADEONWaitForFifo(pScrn, 2);
    OUTREG(RADEON_OV0_REG_LOAD_CNTL, 1);
    if (info->accelOn) info->accel->Sync(pScrn);
    while(!(INREG(RADEON_OV0_REG_LOAD_CNTL) & (1 << 3)));

    RADEONWaitForFifo(pScrn, 14);
    OUTREG(RADEON_OV0_H_INC, h_inc | ((h_inc >> 1) << 16));
    OUTREG(RADEON_OV0_STEP_BY, step_by | (step_by << 8));

    x_off = 8;
    y_off = 0;

    if (IS_R300_VARIANT ||
        (info->ChipFamily == CHIP_FAMILY_R200))
	x_off = 0;

    /* needed to make the overlay work on crtc1 in leftof and above modes */
    if (info->MergedFB) {
	RADEONScrn2Rel srel =
	    ((RADEONMergedDisplayModePtr)info->CurrentLayout.mode->Private)->CRT2Position;
	overlay_mode = ((RADEONMergedDisplayModePtr)info->CurrentLayout.mode->Private)->CRT2;
	if (srel == radeonLeftOf) {
    	    x_off -= overlay_mode->CrtcHDisplay;
	    /* y_off -= pScrn->frameY0; */
	}
	if (srel == radeonAbove) {
    	    y_off -= overlay_mode->CrtcVDisplay;
	    /* x_off -= pScrn->frameX0; */
	}
    }

    /* Put the hardware overlay on CRTC2:
     *
     * Since one hardware overlay can not be displayed on two heads
     * at the same time, we might need to consider using software
     * rendering for the second head.
     */

    if ((info->MergedFB && info->OverlayOnCRTC2) || info->IsSecondary) {
        x_off = 0;
        OUTREG(RADEON_OV1_Y_X_START, ((dstBox->x1 + x_off) |
                                      ((dstBox->y1*y_mult) << 16)));
        OUTREG(RADEON_OV1_Y_X_END,   ((dstBox->x2 + x_off) |
                                      ((dstBox->y2*y_mult) << 16)));
        scaler_src = (1 << 14);
    } else {
	OUTREG(RADEON_OV0_Y_X_START, ((dstBox->x1 + x_off) |
				      (((dstBox->y1*y_mult) + y_off) << 16)));
	OUTREG(RADEON_OV0_Y_X_END,   ((dstBox->x2 + x_off) |
				      (((dstBox->y2*y_mult) + y_off) << 16)));
	scaler_src = 0;
    }

    OUTREG(RADEON_OV0_V_INC, v_inc);
    OUTREG(RADEON_OV0_P1_BLANK_LINES_AT_TOP, 0x00000fff | ((src_h - 1) << 16));
    OUTREG(RADEON_OV0_VID_BUF_PITCH0_VALUE, pitch);
    OUTREG(RADEON_OV0_VID_BUF_PITCH1_VALUE, pitch);
    OUTREG(RADEON_OV0_P1_X_START_END, (src_w + left - 1) | (left << 16));
    left >>= 1; src_w >>= 1;
    OUTREG(RADEON_OV0_P2_X_START_END, (src_w + left - 1) | (left << 16));
    OUTREG(RADEON_OV0_P3_X_START_END, (src_w + left - 1) | (left << 16));
    OUTREG(RADEON_OV0_VID_BUF0_BASE_ADRS, offset1 & 0xfffffff0);
    OUTREG(RADEON_OV0_VID_BUF1_BASE_ADRS, offset2 & 0xfffffff0);
    OUTREG(RADEON_OV0_VID_BUF2_BASE_ADRS, offset1 & 0xfffffff0);

    RADEONWaitForFifo(pScrn, 9);
    OUTREG(RADEON_OV0_VID_BUF3_BASE_ADRS, offset2 & 0xfffffff0);
    OUTREG(RADEON_OV0_VID_BUF4_BASE_ADRS, offset1 & 0xfffffff0);
    OUTREG(RADEON_OV0_VID_BUF5_BASE_ADRS, offset2 & 0xfffffff0);
    OUTREG(RADEON_OV0_P1_V_ACCUM_INIT, p1_v_accum_init);
    OUTREG(RADEON_OV0_P1_H_ACCUM_INIT, p1_h_accum_init);
    OUTREG(RADEON_OV0_P23_H_ACCUM_INIT, p23_h_accum_init);

#if 0
    if(id == FOURCC_UYVY)
       OUTREG(RADEON_OV0_SCALE_CNTL, 0x41008C03);
    else
       OUTREG(RADEON_OV0_SCALE_CNTL, 0x41008B03);
#endif

    if (id == FOURCC_UYVY)
	OUTREG(RADEON_OV0_SCALE_CNTL, (RADEON_SCALER_SOURCE_YVYU422
				       | RADEON_SCALER_ADAPTIVE_DEINT
				       | RADEON_SCALER_SMART_SWITCH
				       | RADEON_SCALER_DOUBLE_BUFFER
				       | RADEON_SCALER_ENABLE
				       | scaler_src));
    else
	OUTREG(RADEON_OV0_SCALE_CNTL, (RADEON_SCALER_SOURCE_VYUY422
				       | RADEON_SCALER_ADAPTIVE_DEINT
				       | RADEON_SCALER_SMART_SWITCH
				       | RADEON_SCALER_DOUBLE_BUFFER
				       | RADEON_SCALER_ENABLE
				       | scaler_src));

    OUTREG(RADEON_OV0_REG_LOAD_CNTL, 0);
}


static int
RADEONPutImage(
  ScrnInfoPtr pScrn,
  short src_x, short src_y,
  short drw_x, short drw_y,
  short src_w, short src_h,
  short drw_w, short drw_h,
  int id, unsigned char* buf,
  short width, short height,
  Bool Sync,
  RegionPtr clipBoxes, pointer data
){
   RADEONInfoPtr info = RADEONPTR(pScrn);
   RADEONPortPrivPtr pPriv = (RADEONPortPrivPtr)data;
   INT32 xa, xb, ya, yb;
   unsigned char *dst_start;
   int new_size, offset, s2offset, s3offset;
   int srcPitch, srcPitch2, dstPitch;
   int top, left, npixels, nlines, bpp;
   BoxRec dstBox;
   CARD32 tmp;
#if X_BYTE_ORDER == X_BIG_ENDIAN
   unsigned char *RADEONMMIO = info->MMIO;
#endif

   /*
    * s2offset, s3offset - byte offsets into U and V plane of the
    *                      source where copying starts.  Y plane is
    *                      done by editing "buf".
    *
    * offset - byte offset to the first line of the destination.
    *
    * dst_start - byte address to the first displayed pel.
    *
    */

   /* make the compiler happy */
   s2offset = s3offset = srcPitch2 = 0;

   if(src_w > (drw_w << 4))
	drw_w = src_w >> 4;
   if(src_h > (drw_h << 4))
	drw_h = src_h >> 4;

   /* Clip */
   xa = src_x;
   xb = src_x + src_w;
   ya = src_y;
   yb = src_y + src_h;

   dstBox.x1 = drw_x;
   dstBox.x2 = drw_x + drw_w;
   dstBox.y1 = drw_y;
   dstBox.y2 = drw_y + drw_h;

   if (info->MergedFB)
	RADEONChooseOverlayCRTC(pScrn, &dstBox);

   if(!xf86XVClipVideoHelper(&dstBox, &xa, &xb, &ya, &yb,
			     clipBoxes, width, height))
	return Success;

   if (info->MergedFB && info->OverlayOnCRTC2) {
	dstBox.x1 -= info->CRT2pScrn->frameX0;
	dstBox.x2 -= info->CRT2pScrn->frameX0;
	dstBox.y1 -= info->CRT2pScrn->frameY0;
	dstBox.y2 -= info->CRT2pScrn->frameY0;
   } else {
	dstBox.x1 -= pScrn->frameX0;
	dstBox.x2 -= pScrn->frameX0;
	dstBox.y1 -= pScrn->frameY0;
	dstBox.y2 -= pScrn->frameY0;
   }

   bpp = pScrn->bitsPerPixel >> 3;

   switch(id) {
   case FOURCC_YV12:
   case FOURCC_I420:
	dstPitch = ((width << 1) + 15) & ~15;
	new_size = ((dstPitch * height) + bpp - 1) / bpp;
	srcPitch = (width + 3) & ~3;
	s2offset = srcPitch * height;
	srcPitch2 = ((width >> 1) + 3) & ~3;
	s3offset = (srcPitch2 * (height >> 1)) + s2offset;
	break;
   case FOURCC_UYVY:
   case FOURCC_YUY2:
   default:
	dstPitch = ((width << 1) + 15) & ~15;
	new_size = ((dstPitch * height) + bpp - 1) / bpp;
	srcPitch = (width << 1);
	break;
   }

   if(!(info->videoLinear = RADEONAllocateMemory(pScrn, info->videoLinear,
		pPriv->doubleBuffer ? (new_size << 1) : new_size)))
   {
	return BadAlloc;
   }

   pPriv->currentBuffer ^= 1;

    /* copy data */
   top = ya >> 16;
   left = (xa >> 16) & ~1;
   npixels = ((((xb + 0xffff) >> 16) + 1) & ~1) - left;

   offset = (info->videoLinear->offset * bpp) + (top * dstPitch);
   if(pPriv->doubleBuffer)
	offset += pPriv->currentBuffer * new_size * bpp;

   dst_start = info->FB + offset;

   switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	top &= ~1;
	dst_start += left << 1;
	tmp = ((top >> 1) * srcPitch2) + (left >> 1);
	s2offset += tmp;
	s3offset += tmp;
	if(id == FOURCC_I420) {
	   tmp = s2offset;
	   s2offset = s3offset;
	   s3offset = tmp;
	}
	nlines = ((((yb + 0xffff) >> 16) + 1) & ~1) - top;
#if X_BYTE_ORDER == X_BIG_ENDIAN
	OUTREG(RADEON_SURFACE_CNTL, (info->ModeReg.surface_cntl |
				     RADEON_NONSURF_AP0_SWP_32BPP)
				    & ~RADEON_NONSURF_AP0_SWP_16BPP);
#endif
	RADEONCopyMungedData(buf + (top * srcPitch) + left, buf + s2offset,
			     buf + s3offset, dst_start, srcPitch, srcPitch2,
			     dstPitch, nlines, npixels);
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	left <<= 1;
	buf += (top * srcPitch) + left;
	nlines = ((yb + 0xffff) >> 16) - top;
	dst_start += left;
#if X_BYTE_ORDER == X_BIG_ENDIAN
	OUTREG(RADEON_SURFACE_CNTL, info->ModeReg.surface_cntl &
				    ~(RADEON_NONSURF_AP0_SWP_32BPP
				      | RADEON_NONSURF_AP0_SWP_16BPP));
#endif
	RADEONCopyData(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
	break;
    }

#if X_BYTE_ORDER == X_BIG_ENDIAN
    /* restore byte swapping */
    OUTREG(RADEON_SURFACE_CNTL, info->ModeReg.surface_cntl);
#endif

    /* update cliplist */
    if(!REGION_EQUAL(pScrn->pScreen, &pPriv->clip, clipBoxes))
    {
	REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);
	/* draw these */
	if(pPriv->autopaint_colorkey)
	    xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
    }

    RADEONDisplayVideo(pScrn, id, offset, offset, width, height, dstPitch,
		     xa, xb, ya, &dstBox, src_w, src_h, drw_w, drw_h);

    pPriv->videoStatus = CLIENT_VIDEO_ON;

    info->VideoTimerCallback = RADEONVideoTimerCallback;

    return Success;
}


static int
RADEONQueryImageAttributes(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short *w, unsigned short *h,
    int *pitches, int *offsets
){
    int size, tmp;

    if(*w > 2048) *w = 2048;
    if(*h > 2048) *h = 2048;

    *w = (*w + 1) & ~1;
    if(offsets) offsets[0] = 0;

    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	*h = (*h + 1) & ~1;
	size = (*w + 3) & ~3;
	if(pitches) pitches[0] = size;
	size *= *h;
	if(offsets) offsets[1] = size;
	tmp = ((*w >> 1) + 3) & ~3;
	if(pitches) pitches[1] = pitches[2] = tmp;
	tmp *= (*h >> 1);
	size += tmp;
	if(offsets) offsets[2] = size;
	size += tmp;
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	size = *w << 1;
	if(pitches) pitches[0] = size;
	size *= *h;
	break;
    }

    return size;
}

static void
RADEONVideoTimerCallback(ScrnInfoPtr pScrn, Time now)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONPortPrivPtr pPriv = info->adaptor->pPortPrivates[0].ptr;

    if(pPriv->videoStatus & TIMER_MASK) {
	if(pPriv->videoStatus & OFF_TIMER) {
	    if(pPriv->offTime < now) {
		unsigned char *RADEONMMIO = info->MMIO;
		OUTREG(RADEON_OV0_SCALE_CNTL, 0);
		pPriv->videoStatus = FREE_TIMER;
		pPriv->freeTime = now + FREE_DELAY;
	    }
	} else {  /* FREE_TIMER */
	    if(pPriv->freeTime < now) {
		if(info->videoLinear) {
		   xf86FreeOffscreenLinear(info->videoLinear);
		   info->videoLinear = NULL;
		}
		pPriv->videoStatus = 0;
		info->VideoTimerCallback = NULL;
	    }
	}
    } else  /* shouldn't get here */
	info->VideoTimerCallback = NULL;
}

/****************** Offscreen stuff ***************/
typedef struct {
  FBLinearPtr linear;
  Bool isOn;
} OffscreenPrivRec, * OffscreenPrivPtr;

static int
RADEONAllocateSurface(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short w,
    unsigned short h,
    XF86SurfacePtr surface
){
    FBLinearPtr linear;
    int pitch, size, bpp;
    OffscreenPrivPtr pPriv;
    if((w > 1024) || (h > 1024))
	return BadAlloc;

    w = (w + 1) & ~1;
    pitch = ((w << 1) + 15) & ~15;
    bpp = pScrn->bitsPerPixel >> 3;
    size = ((pitch * h) + bpp - 1) / bpp;

    if(!(linear = RADEONAllocateMemory(pScrn, NULL, size)))
	return BadAlloc;

    surface->width = w;
    surface->height = h;

    if(!(surface->pitches = xalloc(sizeof(int)))) {
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }
    if(!(surface->offsets = xalloc(sizeof(int)))) {
	xfree(surface->pitches);
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }
    if(!(pPriv = xalloc(sizeof(OffscreenPrivRec)))) {
	xfree(surface->pitches);
	xfree(surface->offsets);
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }

    pPriv->linear = linear;
    pPriv->isOn = FALSE;

    surface->pScrn = pScrn;
    surface->id = id;
    surface->pitches[0] = pitch;
    surface->offsets[0] = linear->offset * bpp;
    surface->devPrivate.ptr = (pointer)pPriv;

    return Success;
}

static int
RADEONStopSurface(
    XF86SurfacePtr surface
){
  OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;
  RADEONInfoPtr info = RADEONPTR(surface->pScrn);
  unsigned char *RADEONMMIO = info->MMIO;

  if(pPriv->isOn) {
	OUTREG(RADEON_OV0_SCALE_CNTL, 0);
	pPriv->isOn = FALSE;
  }
  return Success;
}


static int
RADEONFreeSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn)
	RADEONStopSurface(surface);
    xf86FreeOffscreenLinear(pPriv->linear);
    xfree(surface->pitches);
    xfree(surface->offsets);
    xfree(surface->devPrivate.ptr);

    return Success;
}

static int
RADEONGetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value
){
   return RADEONGetPortAttribute(pScrn, attribute, value,
		(pointer)(GET_PORT_PRIVATE(pScrn)));
}

static int
RADEONSetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value
){
   return RADEONSetPortAttribute(pScrn, attribute, value,
		(pointer)(GET_PORT_PRIVATE(pScrn)));
}


static int
RADEONDisplaySurface(
    XF86SurfacePtr surface,
    short src_x, short src_y,
    short drw_x, short drw_y,
    short src_w, short src_h,
    short drw_w, short drw_h,
    RegionPtr clipBoxes
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;
    ScrnInfoPtr pScrn = surface->pScrn;

    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONPortPrivPtr portPriv = info->adaptor->pPortPrivates[0].ptr;

    INT32 xa, ya, xb, yb;
    BoxRec dstBox;

    if (src_w > (drw_w << 4))
	drw_w = src_w >> 4;
    if (src_h > (drw_h << 4))
	drw_h = src_h >> 4;

    xa = src_x;
    xb = src_x + src_w;
    ya = src_y;
    yb = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    if (info->MergedFB)
        RADEONChooseOverlayCRTC(pScrn, &dstBox);

    if (!xf86XVClipVideoHelper(&dstBox, &xa, &xb, &ya, &yb, clipBoxes, 
			       surface->width, surface->height))
	return Success;

    if (info->MergedFB && info->OverlayOnCRTC2) {
	dstBox.x1 -= info->CRT2pScrn->frameX0;
	dstBox.x2 -= info->CRT2pScrn->frameX0;
	dstBox.y1 -= info->CRT2pScrn->frameY0;
	dstBox.y2 -= info->CRT2pScrn->frameY0;
    } else {
	dstBox.x1 -= pScrn->frameX0;
	dstBox.x2 -= pScrn->frameX0;
	dstBox.y1 -= pScrn->frameY0;
	dstBox.y2 -= pScrn->frameY0;
    }

#if 0
    /* this isn't needed */
    RADEONResetVideo(pScrn);
#endif
    RADEONDisplayVideo(pScrn, surface->id,
		       surface->offsets[0], surface->offsets[0],
		       surface->width, surface->height, surface->pitches[0],
		       xa, xb, ya, &dstBox, src_w, src_h, drw_w, drw_h);

    if (portPriv->autopaint_colorkey)
	xf86XVFillKeyHelper(pScrn->pScreen, portPriv->colorKey, clipBoxes);

    pPriv->isOn = TRUE;
    /* we've prempted the XvImage stream so set its free timer */
    if (portPriv->videoStatus & CLIENT_VIDEO_ON) {
	REGION_EMPTY(pScrn->pScreen, &portPriv->clip);
	UpdateCurrentTime();
	portPriv->videoStatus = FREE_TIMER;
	portPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	info->VideoTimerCallback = RADEONVideoTimerCallback;
    }

    return Success;
}


static void
RADEONInitOffscreenImages(ScreenPtr pScreen)
{
/*  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    RADEONInfoPtr info = RADEONPTR(pScrn); */
    XF86OffscreenImagePtr offscreenImages;
    /* need to free this someplace */

    if (!(offscreenImages = xalloc(sizeof(XF86OffscreenImageRec))))
	return;

    offscreenImages[0].image = &Images[0];
    offscreenImages[0].flags = VIDEO_OVERLAID_IMAGES |
			       VIDEO_CLIP_TO_VIEWPORT;
    offscreenImages[0].alloc_surface = RADEONAllocateSurface;
    offscreenImages[0].free_surface = RADEONFreeSurface;
    offscreenImages[0].display = RADEONDisplaySurface;
    offscreenImages[0].stop = RADEONStopSurface;
    offscreenImages[0].setAttribute = RADEONSetSurfaceAttribute;
    offscreenImages[0].getAttribute = RADEONGetSurfaceAttribute;
    offscreenImages[0].max_width = 2048;
    offscreenImages[0].max_height = 2048;
    offscreenImages[0].num_attributes = NUM_ATTRIBUTES;
    offscreenImages[0].attributes = Attributes;

    xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
}
