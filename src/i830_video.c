#define VIDEO_DEBUG 1
/***************************************************************************
 
Copyright 2000 Intel Corporation.  All Rights Reserved. 

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
IN NO EVENT SHALL INTEL, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, 
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR 
THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i830_video.c,v 1.11tsi Exp $ */

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
 * i830_video.c: i830/i845 Xv driver. 
 *
 * Copyright © 2002 by Alan Hourihane and David Dawes
 *
 * Authors: 
 *	Alan Hourihane <alanh@tungstengraphics.com>
 *	David Dawes <dawes@xfree86.org>
 *
 * Derived from i810 Xv driver:
 *
 * Authors of i810 code:
 * 	Jonathan Bian <jonathan.bian@intel.com>
 *      Offscreen Images:
 *        Matt Sottek <matthew.j.sottek@intel.com>
 */

/*
 * XXX Could support more formats.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <string.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"
#include "randrstr.h"
#include "i830.h"
#include "xf86xv.h"
#include <X11/extensions/Xv.h>
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

#ifndef USE_USLEEP_FOR_VIDEO
#define USE_USLEEP_FOR_VIDEO 0
#endif

#define OFF_DELAY 	250		/* milliseconds */
#define FREE_DELAY 	15000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

static void I830InitOffscreenImages(ScreenPtr);

static XF86VideoAdaptorPtr I830SetupImageVideoOverlay(ScreenPtr);
static XF86VideoAdaptorPtr I830SetupImageVideoTextured(ScreenPtr);
static void I830StopVideo(ScrnInfoPtr, pointer, Bool);
static int I830SetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int I830GetPortAttribute(ScrnInfoPtr, Atom, INT32 *, pointer);
static void I830QueryBestSize(ScrnInfoPtr, Bool,
			      short, short, short, short, unsigned int *,
			      unsigned int *, pointer);
static int I830PutImage(ScrnInfoPtr, short, short, short, short, short, short,
			short, short, int, unsigned char *, short, short,
			Bool, RegionPtr, pointer, DrawablePtr);
static int I830QueryImageAttributesOverlay(ScrnInfoPtr, int, unsigned short *,
					   unsigned short *, int *, int *);
static int I830QueryImageAttributesTextured(ScrnInfoPtr, int, unsigned short *,
					    unsigned short *, int *, int *);

static void I830BlockHandler(int, pointer, pointer, pointer);

#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvBrightness, xvContrast, xvColorKey, xvPipe, xvDoubleBuffer;
static Atom xvGamma0, xvGamma1, xvGamma2, xvGamma3, xvGamma4, xvGamma5;

#define IMAGE_MAX_WIDTH		1920
#define IMAGE_MAX_HEIGHT	1088
#define IMAGE_MAX_WIDTH_LEGACY	1024
#define IMAGE_MAX_HEIGHT_LEGACY	1088

#if !VIDEO_DEBUG
#define ErrorF Edummy
static void
Edummy(const char *dummy, ...)
{
}
#endif

/*
 * This is more or less the correct way to initalise, update, and shut down
 * the overlay.  Note OVERLAY_OFF should be used only after disabling the
 * overlay in OCMD and calling OVERLAY_UPDATE.
 *
 * XXX Need to make sure that the overlay engine is cleanly shutdown in
 * all modes of server exit.
 */

#define OVERLAY_UPDATE						\
   do { 								\
      BEGIN_LP_RING(6);							\
      OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE);			\
      OUT_RING(MI_NOOP);						\
      if (!*pI830->overlayOn) {						\
         OUT_RING(MI_NOOP);						\
	 OUT_RING(MI_NOOP);						\
	 OUT_RING(MI_OVERLAY_FLIP | MI_OVERLAY_FLIP_ON);		\
	 ErrorF("Overlay goes from off to on\n");			\
	 *pI830->overlayOn = TRUE;					\
      } else {								\
	 OUT_RING(MI_WAIT_FOR_EVENT | MI_WAIT_FOR_OVERLAY_FLIP);	\
	 OUT_RING(MI_NOOP);						\
	 OUT_RING(MI_OVERLAY_FLIP | MI_OVERLAY_FLIP_CONTINUE);		\
      }									\
      OUT_RING(pI830->OverlayMem->Physical | OFC_UPDATE); 		\
      ADVANCE_LP_RING();						\
      ErrorF("OVERLAY_UPDATE\n");					\
   } while(0)

#define OVERLAY_OFF							\
   do { 								\
      if (*pI830->overlayOn) {						\
	 int spin = 1000000;						\
	 BEGIN_LP_RING(12);						\
	 OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE);			\
	 OUT_RING(MI_NOOP);						\
	 OUT_RING(MI_WAIT_FOR_EVENT | MI_WAIT_FOR_OVERLAY_FLIP);	\
	 OUT_RING(MI_NOOP);						\
	 OUT_RING(MI_OVERLAY_FLIP | MI_OVERLAY_FLIP_CONTINUE);		\
         OUT_RING(pI830->OverlayMem->Physical | OFC_UPDATE); 		\
	 OUT_RING(MI_WAIT_FOR_EVENT | MI_WAIT_FOR_OVERLAY_FLIP);	\
	 OUT_RING(MI_NOOP);						\
	 OUT_RING(MI_OVERLAY_FLIP | MI_OVERLAY_FLIP_OFF);		\
         OUT_RING(pI830->OverlayMem->Physical | OFC_UPDATE); 		\
	 OUT_RING(MI_WAIT_FOR_EVENT | MI_WAIT_FOR_OVERLAY_FLIP);	\
	 OUT_RING(MI_NOOP);						\
	 ADVANCE_LP_RING();						\
	 *pI830->overlayOn = FALSE;					\
	 ErrorF("Overlay goes from on to off\n");			\
         while (spin != 0 && (INREG(OCMD_REGISTER) & OVERLAY_ENABLE)){	\
		ErrorF("SPIN %d\n",spin);				\
		spin--;							\
 	 }								\
	 if (spin == 0) ErrorF("OVERLAY FAILED TO GO OFF\n");		\
	 ErrorF("OVERLAY_OFF\n");					\
      }									\
   } while(0)

/*
 * OCMD - Overlay Command Register
 */
#define OCMD_REGISTER		0x30168
#define MIRROR_MODE		(0x3<<17)
#define MIRROR_HORIZONTAL	(0x1<<17)
#define MIRROR_VERTICAL		(0x2<<17)
#define MIRROR_BOTH		(0x3<<17)
#define OV_BYTE_ORDER		(0x3<<14)
#define UV_SWAP			(0x1<<14)
#define Y_SWAP			(0x2<<14)
#define Y_AND_UV_SWAP		(0x3<<14)
#define SOURCE_FORMAT		(0xf<<10)
#define RGB_888			(0x1<<10)
#define	RGB_555			(0x2<<10)
#define	RGB_565			(0x3<<10)
#define	YUV_422			(0x8<<10)
#define	YUV_411			(0x9<<10)
#define	YUV_420			(0xc<<10)
#define	YUV_422_PLANAR		(0xd<<10)
#define	YUV_410			(0xe<<10)
#define TVSYNC_FLIP_PARITY	(0x1<<9)
#define TVSYNC_FLIP_ENABLE	(0x1<<7)
#define BUF_TYPE		(0x1<<5)
#define BUF_TYPE_FRAME		(0x0<<5)
#define BUF_TYPE_FIELD		(0x1<<5)
#define TEST_MODE		(0x1<<4)
#define BUFFER_SELECT		(0x3<<2)
#define BUFFER0			(0x0<<2)
#define BUFFER1			(0x1<<2)
#define FIELD_SELECT		(0x1<<1)
#define FIELD0			(0x0<<1)
#define FIELD1			(0x1<<1)
#define OVERLAY_ENABLE		0x1

#define OFC_UPDATE		0x1

/* OCONFIG register */
#define CC_OUT_8BIT		(0x1<<3)
#define OVERLAY_PIPE_MASK	(0x1<<18)		
#define OVERLAY_PIPE_A		(0x0<<18)		
#define OVERLAY_PIPE_B		(0x1<<18)		
#define THREE_LINE_BUFFERS	(0x1<<0)
#define TWO_LINE_BUFFERS	(0x0<<0)

/* DCLRKM register */
#define DEST_KEY_ENABLE		(0x1<<31)

/* Polyphase filter coefficients */
#define N_HORIZ_Y_TAPS		5
#define N_VERT_Y_TAPS		3
#define N_HORIZ_UV_TAPS		3
#define N_VERT_UV_TAPS		3
#define N_PHASES		17
#define MAX_TAPS		5

/* Filter cutoff frequency limits. */
#define MIN_CUTOFF_FREQ		1.0
#define MAX_CUTOFF_FREQ		3.0

#define RGB16ToColorKey(c) \
	(((c & 0xF800) << 8) | ((c & 0x07E0) << 5) | ((c & 0x001F) << 3))

#define RGB15ToColorKey(c) \
        (((c & 0x7c00) << 9) | ((c & 0x03E0) << 6) | ((c & 0x001F) << 3))

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[1] = {
   {
      0,
      "XV_IMAGE",
      IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
      {1, 1}
   }
};

#define NUM_FORMATS 3

static XF86VideoFormatRec Formats[NUM_FORMATS] = {
   {15, TrueColor}, {16, TrueColor}, {24, TrueColor}
};

#define CLONE_ATTRIBUTES 1
static XF86AttributeRec CloneAttributes[CLONE_ATTRIBUTES] = {
   {XvSettable | XvGettable, 0, 1, "XV_PIPE"}
};

#define NUM_ATTRIBUTES 4
static XF86AttributeRec Attributes[NUM_ATTRIBUTES] = {
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"},
   {XvSettable | XvGettable, -128, 127, "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 255, "XV_CONTRAST"},
   {XvSettable | XvGettable, 0, 1, "XV_DOUBLE_BUFFER"}
};

#define NUM_TEXTURED_ATTRIBUTES 2
static XF86AttributeRec TexturedAttributes[NUM_ATTRIBUTES] = {
   {XvSettable | XvGettable, -128, 127, "XV_BRIGHTNESS"},
   {XvSettable | XvGettable, 0, 255, "XV_CONTRAST"},
};

#define GAMMA_ATTRIBUTES 6
static XF86AttributeRec GammaAttributes[GAMMA_ATTRIBUTES] = {
   {XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA0"},
   {XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA1"},
   {XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA2"},
   {XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA3"},
   {XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA4"},
   {XvSettable | XvGettable, 0, 0xffffff, "XV_GAMMA5"}
};

#define NUM_IMAGES 4

static XF86ImageRec Images[NUM_IMAGES] = {
   XVIMAGE_YUY2,
   XVIMAGE_YV12,
   XVIMAGE_I420,
   XVIMAGE_UYVY
};

typedef struct {
   CARD32 OBUF_0Y;
   CARD32 OBUF_1Y;
   CARD32 OBUF_0U;
   CARD32 OBUF_0V;
   CARD32 OBUF_1U;
   CARD32 OBUF_1V;
   CARD32 OSTRIDE;
   CARD32 YRGB_VPH;
   CARD32 UV_VPH;
   CARD32 HORZ_PH;
   CARD32 INIT_PHS;
   CARD32 DWINPOS;
   CARD32 DWINSZ;
   CARD32 SWIDTH;
   CARD32 SWIDTHSW;
   CARD32 SHEIGHT;
   CARD32 YRGBSCALE;
   CARD32 UVSCALE;
   CARD32 OCLRC0;
   CARD32 OCLRC1;
   CARD32 DCLRKV;
   CARD32 DCLRKM;
   CARD32 SCLRKVH;
   CARD32 SCLRKVL;
   CARD32 SCLRKEN;
   CARD32 OCONFIG;
   CARD32 OCMD;
   CARD32 RESERVED1;			/* 0x6C */
   CARD32 AWINPOS;
   CARD32 AWINSZ;
   CARD32 RESERVED2;			/* 0x78 */
   CARD32 RESERVED3;			/* 0x7C */
   CARD32 RESERVED4;			/* 0x80 */
   CARD32 RESERVED5;			/* 0x84 */
   CARD32 RESERVED6;			/* 0x88 */
   CARD32 RESERVED7;			/* 0x8C */
   CARD32 RESERVED8;			/* 0x90 */
   CARD32 RESERVED9;			/* 0x94 */
   CARD32 RESERVEDA;			/* 0x98 */
   CARD32 RESERVEDB;			/* 0x9C */
   CARD32 FASTHSCALE;			/* 0xA0 */
   CARD32 UVSCALEV;			/* 0xA4 */

   CARD32 RESERVEDC[(0x200 - 0xA8) / 4];		   /* 0xA8 - 0x1FC */
   CARD16 Y_VCOEFS[N_VERT_Y_TAPS * N_PHASES];		   /* 0x200 */
   CARD16 RESERVEDD[0x100 / 2 - N_VERT_Y_TAPS * N_PHASES];
   CARD16 Y_HCOEFS[N_HORIZ_Y_TAPS * N_PHASES];		   /* 0x300 */
   CARD16 RESERVEDE[0x200 / 2 - N_HORIZ_Y_TAPS * N_PHASES];
   CARD16 UV_VCOEFS[N_VERT_UV_TAPS * N_PHASES];		   /* 0x500 */
   CARD16 RESERVEDF[0x100 / 2 - N_VERT_UV_TAPS * N_PHASES];
   CARD16 UV_HCOEFS[N_HORIZ_UV_TAPS * N_PHASES];	   /* 0x600 */
   CARD16 RESERVEDG[0x100 / 2 - N_HORIZ_UV_TAPS * N_PHASES];
} I830OverlayRegRec, *I830OverlayRegPtr;

typedef struct {
   CARD32 YBuf0offset;
   CARD32 UBuf0offset;
   CARD32 VBuf0offset;

   CARD32 YBuf1offset;
   CARD32 UBuf1offset;
   CARD32 VBuf1offset;

   unsigned char currentBuf;

   int brightness;
   int contrast;
   int pipe;
   int doubleBuffer;

   RegionRec clip;
   CARD32 colorKey;

   CARD32 gamma0;
   CARD32 gamma1;
   CARD32 gamma2;
   CARD32 gamma3;
   CARD32 gamma4;
   CARD32 gamma5;

   CARD32 videoStatus;
   Time offTime;
   Time freeTime;
   FBLinearPtr linear;

   Bool overlayOK;
   int oneLineMode;
   int scaleRatio;
   Bool textured;
} I830PortPrivRec, *I830PortPrivPtr;

#define GET_PORT_PRIVATE(pScrn) \
   (I830PortPrivPtr)((I830PTR(pScrn))->adaptor->pPortPrivates[0].ptr)

#if VIDEO_DEBUG
static void
CompareOverlay(I830Ptr pI830, CARD32 * overlay, int size)
{
   int i;
   CARD32 val;
   int bad = 0;

   for (i = 0; i < size; i += 4) {
      val = INREG(0x30100 + i);
      if (val != overlay[i / 4]) {
	 ErrorF("0x%05x value doesn't match (0x%lx != 0x%lx)\n",
		0x30100 + i, val, overlay[i / 4]);
	 bad++;
      }
   }
   if (!bad)
      ErrorF("CompareOverlay: no differences\n");
}
#endif

void
I830InitVideo(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
   XF86VideoAdaptorPtr overlayAdaptor = NULL, texturedAdaptor = NULL;
   int num_adaptors;

   DPRINTF(PFX, "I830InitVideo\n");

#if 0
   {
      I830OverlayRegRec tmp;

      ErrorF("sizeof I830OverlayRegRec is 0x%x\n", sizeof(I830OverlayRegRec));
      ErrorF("Reserved C, D, E, F, G are %x, %x, %x, %x, %x\n",
	     (unsigned long)&(tmp.RESERVEDC[0]) - (unsigned long)&tmp,
	     (unsigned long)&(tmp.RESERVEDD[0]) - (unsigned long)&tmp,
	     (unsigned long)&(tmp.RESERVEDE[0]) - (unsigned long)&tmp,
	     (unsigned long)&(tmp.RESERVEDF[0]) - (unsigned long)&tmp,
	     (unsigned long)&(tmp.RESERVEDG[0]) - (unsigned long)&tmp);
   }
#endif

   num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);
   /* Give our adaptor list enough space for the overlay and/or texture video
    * adaptors.
    */
   newAdaptors = xalloc((num_adaptors + 2) * sizeof(XF86VideoAdaptorPtr *));
   if (newAdaptors == NULL)
      return;

   memcpy(newAdaptors, adaptors, num_adaptors * sizeof(XF86VideoAdaptorPtr));
   adaptors = newAdaptors;

   /* Add the adaptors supported by our hardware.  First, set up the atoms
    * that will be used by both output adaptors.
    */
   xvBrightness = MAKE_ATOM("XV_BRIGHTNESS");
   xvContrast = MAKE_ATOM("XV_CONTRAST");

   /* Set up overlay video if we can do it at this depth. */
   if (pScrn->bitsPerPixel != 8) {
      overlayAdaptor = I830SetupImageVideoOverlay(pScreen);
      if (overlayAdaptor != NULL) {
	 adaptors[num_adaptors++] = overlayAdaptor;
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Set up overlay video\n");
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Failed to set up overlay video\n");
      }
      I830InitOffscreenImages(pScreen);
   }

   /* Set up textured video if we can do it at this depth and we are on
    * supported hardware.
    */
   if (pScrn->bitsPerPixel >= 16 && IS_I9XX(pI830)) {
      texturedAdaptor = I830SetupImageVideoTextured(pScreen);
      if (texturedAdaptor != NULL) {
	 adaptors[num_adaptors++] = texturedAdaptor;
	 xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Set up textured video\n");
      } else {
	 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		    "Failed to set up textured video\n");
      }
   }

   if (num_adaptors)
      xf86XVScreenInit(pScreen, adaptors, num_adaptors);

   xfree(adaptors);
}

static void
I830ResetVideo(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830PortPrivPtr pPriv = pI830->adaptor->pPortPrivates[0].ptr;
   I830OverlayRegPtr overlay =
	 (I830OverlayRegPtr) (pI830->FbBase + pI830->OverlayMem->Start);

   DPRINTF(PFX, "I830ResetVideo: base: %p, offset: 0x%lx, obase: %p\n",
	   pI830->FbBase, pI830->OverlayMem->Start, overlay);
   /*
    * Default to maximum image size in YV12
    */

   memset(overlay, 0, sizeof(*overlay));
   overlay->YRGB_VPH = 0;
   overlay->UV_VPH = 0;
   overlay->HORZ_PH = 0;
   overlay->INIT_PHS = 0;
   overlay->DWINPOS = 0;
   overlay->DWINSZ = 0;
   overlay->SWIDTH = 0;
   overlay->SWIDTHSW = 0;
   overlay->SHEIGHT = 0;
   overlay->OCLRC0 = (pPriv->contrast << 18) | (pPriv->brightness & 0xff);
   overlay->OCLRC1 = 0x00000080;	/* saturation: bypass */
   overlay->AWINPOS = 0;
   overlay->AWINSZ = 0;
   overlay->FASTHSCALE = 0;

   /*
    * Enable destination color keying
    */
   switch (pScrn->depth) {
   case 8:
      overlay->DCLRKV = 0;
      overlay->DCLRKM = 0xffffff | DEST_KEY_ENABLE;
      break;
   case 15:
      overlay->DCLRKV = RGB15ToColorKey(pPriv->colorKey);
      overlay->DCLRKM = 0x070707 | DEST_KEY_ENABLE;
      break;
   case 16:
      overlay->DCLRKV = RGB16ToColorKey(pPriv->colorKey);
      overlay->DCLRKM = 0x070307 | DEST_KEY_ENABLE;
      break;
   default:
      overlay->DCLRKV = pPriv->colorKey;
      overlay->DCLRKM = DEST_KEY_ENABLE;
      break;
   }

   overlay->SCLRKVH = 0;
   overlay->SCLRKVL = 0;
   overlay->SCLRKEN = 0;		/* source color key disable */
   overlay->OCONFIG = CC_OUT_8BIT;

   /*
    * Select which pipe the overlay is enabled on.
    */
   overlay->OCONFIG &= ~OVERLAY_PIPE_MASK;
   if (pPriv->pipe == 0)
      overlay->OCONFIG |= OVERLAY_PIPE_A;
   else 
      overlay->OCONFIG |= OVERLAY_PIPE_B;

   overlay->OCMD = YUV_420;

#if 0
   /* 
    * XXX DUMP REGISTER CODE !!!
    * This allows us to dump the complete i845 registers and compare
    * with warm boot situations before we upload our first copy.
    */
   {
      int i;
      for (i = 0x30000; i < 0x31000; i += 4)
	 ErrorF("0x%x 0x%lx\n", i, INREG(i));
   }
#endif
}

#define PFIT_CONTROLS 0x61230
#define PFIT_AUTOVSCALE_MASK 0x200
#define PFIT_ON_MASK 0x80000000
#define PFIT_AUTOSCALE_RATIO 0x61238
#define PFIT_PROGRAMMED_SCALE_RATIO 0x61234

static void
I830SetOneLineModeRatio(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830PortPrivPtr pPriv = pI830->adaptor->pPortPrivates[0].ptr;
   CARD32 panelFitControl = INREG(PFIT_CONTROLS);
   int vertScale;

   pPriv->scaleRatio = 0x10000;

   if (panelFitControl & PFIT_ON_MASK) {
      if (panelFitControl & PFIT_AUTOVSCALE_MASK) {
         vertScale = INREG(PFIT_AUTOSCALE_RATIO) >> 16;
      } else {
         vertScale = INREG(PFIT_PROGRAMMED_SCALE_RATIO) >> 16;
      }

      if (vertScale != 0)
         pPriv->scaleRatio = ((double) 0x10000 / (double)vertScale) * 0x10000;
 
      pPriv->oneLineMode = TRUE;

      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Enabling Xvideo one-line mode\n");
   }

   if (pPriv->scaleRatio == 0x10000)
      pPriv->oneLineMode = FALSE;
}

static CARD32 I830BoundGammaElt (CARD32 elt, CARD32 eltPrev)
{
   elt &= 0xff;
   eltPrev &= 0xff;
   if (elt < eltPrev)
      elt = eltPrev;
   else if ((elt - eltPrev) > 0x7e)
      elt = eltPrev + 0x7e;
   return elt;
}

static CARD32 I830BoundGamma (CARD32 gamma, CARD32 gammaPrev)
{
   return (I830BoundGammaElt (gamma >> 24, gammaPrev >> 24) << 24 |
	   I830BoundGammaElt (gamma >> 16, gammaPrev >> 16) << 16 |
	   I830BoundGammaElt (gamma >>  8, gammaPrev >>  8) <<  8 |
	   I830BoundGammaElt (gamma      , gammaPrev      ));
}

static void
I830UpdateGamma(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830PortPrivPtr pPriv = pI830->adaptor->pPortPrivates[0].ptr;
   CARD32   gamma0 = pPriv->gamma0;
   CARD32   gamma1 = pPriv->gamma1;
   CARD32   gamma2 = pPriv->gamma2;
   CARD32   gamma3 = pPriv->gamma3;
   CARD32   gamma4 = pPriv->gamma4;
   CARD32   gamma5 = pPriv->gamma5;

   ErrorF ("Original gamma: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx\n",
	   gamma0, gamma1, gamma2, gamma3, gamma4, gamma5);
   gamma1 = I830BoundGamma (gamma1, gamma0);
   gamma2 = I830BoundGamma (gamma2, gamma1);
   gamma3 = I830BoundGamma (gamma3, gamma2);
   gamma4 = I830BoundGamma (gamma4, gamma3);
   gamma5 = I830BoundGamma (gamma5, gamma4);
   ErrorF ("Bounded  gamma: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx\n",
	   gamma0, gamma1, gamma2, gamma3, gamma4, gamma5);

   OUTREG(OGAMC5, gamma5);
   OUTREG(OGAMC4, gamma4);
   OUTREG(OGAMC3, gamma3);
   OUTREG(OGAMC2, gamma2);
   OUTREG(OGAMC1, gamma1);
   OUTREG(OGAMC0, gamma0);
}

static XF86VideoAdaptorPtr
I830SetupImageVideoOverlay(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   XF86VideoAdaptorPtr adapt;
   I830PortPrivPtr pPriv;
   XF86AttributePtr att;

   DPRINTF(PFX, "I830SetupImageVideoOverlay\n");

   if (!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			 sizeof(I830PortPrivRec) + sizeof(DevUnion))))
      return NULL;

   adapt->type = XvWindowMask | XvInputMask | XvImageMask;
   adapt->flags = VIDEO_OVERLAID_IMAGES /*| VIDEO_CLIP_TO_VIEWPORT*/;
   adapt->name = "Intel(R) Video Overlay";
   adapt->nEncodings = 1;
   adapt->pEncodings = DummyEncoding;
   /* update the DummyEncoding for these two chipsets */
   if (IS_845G(pI830) || IS_I830(pI830)) {
      adapt->pEncodings->width = IMAGE_MAX_WIDTH_LEGACY;
      adapt->pEncodings->height = IMAGE_MAX_HEIGHT_LEGACY;
   }
   adapt->nFormats = NUM_FORMATS;
   adapt->pFormats = Formats;
   adapt->nPorts = 1;
   adapt->pPortPrivates = (DevUnion *) (&adapt[1]);

   pPriv = (I830PortPrivPtr) (&adapt->pPortPrivates[1]);

   adapt->pPortPrivates[0].ptr = (pointer) (pPriv);
   adapt->nAttributes = NUM_ATTRIBUTES;
   if (pI830->Clone)
      adapt->nAttributes += CLONE_ATTRIBUTES;
   if (IS_I9XX(pI830))
      adapt->nAttributes += GAMMA_ATTRIBUTES; /* has gamma */
   adapt->pAttributes = xnfalloc(sizeof(XF86AttributeRec) * adapt->nAttributes);
   /* Now copy the attributes */
   att = adapt->pAttributes;
   memcpy((char *)att, (char*)Attributes, sizeof(XF86AttributeRec)* NUM_ATTRIBUTES);
   att+=NUM_ATTRIBUTES;
   if (pI830->Clone) {
      memcpy((char*)att, (char*)CloneAttributes, sizeof(XF86AttributeRec) * CLONE_ATTRIBUTES);
      att+=CLONE_ATTRIBUTES;
   }
   if (IS_I9XX(pI830)) {
      memcpy((char*)att, (char*)GammaAttributes, sizeof(XF86AttributeRec) * GAMMA_ATTRIBUTES);
      att+=GAMMA_ATTRIBUTES;
   }
   adapt->nImages = NUM_IMAGES;
   adapt->pImages = Images;
   adapt->PutVideo = NULL;
   adapt->PutStill = NULL;
   adapt->GetVideo = NULL;
   adapt->GetStill = NULL;
   adapt->StopVideo = I830StopVideo;
   adapt->SetPortAttribute = I830SetPortAttribute;
   adapt->GetPortAttribute = I830GetPortAttribute;
   adapt->QueryBestSize = I830QueryBestSize;
   adapt->PutImage = I830PutImage;
   adapt->QueryImageAttributes = I830QueryImageAttributesOverlay;

   pPriv->textured = FALSE;
   pPriv->colorKey = pI830->colorKey & ((1 << pScrn->depth) - 1);
   pPriv->videoStatus = 0;
   pPriv->brightness = 0;
   pPriv->contrast = 64;
   pPriv->pipe = pI830->pipe; /* default to current pipe */
   pPriv->linear = NULL;
   pPriv->currentBuf = 0;
   pPriv->gamma5 = 0xc0c0c0;
   pPriv->gamma4 = 0x808080;
   pPriv->gamma3 = 0x404040;
   pPriv->gamma2 = 0x202020;
   pPriv->gamma1 = 0x101010;
   pPriv->gamma0 = 0x080808;
   pPriv->doubleBuffer = 1;

   /* gotta uninit this someplace */
   REGION_NULL(pScreen, &pPriv->clip);

   pI830->adaptor = adapt;

   /* With LFP's we need to detect whether we're in One Line Mode, which
    * essentially means a resolution greater than 1024x768, and fix up
    * the scaler accordingly. */
   pPriv->scaleRatio = 0x10000;
   pPriv->oneLineMode = FALSE;

   /*
    * Initialise pPriv->overlayOK.  Set it to TRUE here so that a warning will
    * be generated if I830VideoSwitchModeAfter() sets it to FALSE.
    */
   pPriv->overlayOK = TRUE;
   I830VideoSwitchModeAfter(pScrn, pScrn->currentMode);

   pI830->BlockHandler = pScreen->BlockHandler;
   pScreen->BlockHandler = I830BlockHandler;

   xvColorKey = MAKE_ATOM("XV_COLORKEY");
   xvDoubleBuffer = MAKE_ATOM("XV_DOUBLE_BUFFER");

   /* Allow the pipe to be switched from pipe A to B when in clone mode */
   if (pI830->Clone)
     xvPipe = MAKE_ATOM("XV_PIPE");
   
   if (IS_I9XX(pI830)) {
     xvGamma0 = MAKE_ATOM("XV_GAMMA0");
     xvGamma1 = MAKE_ATOM("XV_GAMMA1");
     xvGamma2 = MAKE_ATOM("XV_GAMMA2");
     xvGamma3 = MAKE_ATOM("XV_GAMMA3");
     xvGamma4 = MAKE_ATOM("XV_GAMMA4");
     xvGamma5 = MAKE_ATOM("XV_GAMMA5");
   }

   I830ResetVideo(pScrn);

   I830UpdateGamma(pScrn);

   return adapt;
}

static XF86VideoAdaptorPtr
I830SetupImageVideoTextured(ScreenPtr pScreen)
{
   XF86VideoAdaptorPtr adapt;
   XF86VideoEncodingPtr encoding;
   XF86AttributePtr attrs;
   I830PortPrivPtr portPrivs;
   DevUnion *devUnions;
   int nports = 16, i;
   int nAttributes;

   DPRINTF(PFX, "I830SetupImageVideoOverlay\n");

   nAttributes = NUM_TEXTURED_ATTRIBUTES;

   adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec));
   portPrivs = xcalloc(nports, sizeof(I830PortPrivRec));
   devUnions = xcalloc(nports, sizeof(DevUnion));
   encoding = xcalloc(1, sizeof(XF86VideoEncodingRec));
   attrs = xcalloc(nAttributes, sizeof(XF86AttributeRec));
   if (adapt == NULL || portPrivs == NULL || devUnions == NULL ||
       encoding == NULL || attrs == NULL)
   {
      xfree(adapt);
      xfree(portPrivs);
      xfree(devUnions);
      xfree(encoding);
      xfree(attrs);
      return NULL;
   }

   adapt->type = XvWindowMask | XvInputMask | XvImageMask;
   adapt->flags = 0;
   adapt->name = "Intel(R) Textured Video";
   adapt->nEncodings = 1;
   adapt->pEncodings = encoding;
   adapt->pEncodings[0].id = 0;
   adapt->pEncodings[0].name = "XV_IMAGE";
   adapt->pEncodings[0].width = 2048;
   adapt->pEncodings[0].height = 2048;
   adapt->pEncodings[0].rate.numerator = 1;
   adapt->pEncodings[0].rate.denominator = 1;
   adapt->nFormats = NUM_FORMATS;
   adapt->pFormats = Formats;
   adapt->nPorts = nports;
   adapt->pPortPrivates = devUnions;
   adapt->nAttributes = nAttributes;
   adapt->pAttributes = attrs;
   memcpy(attrs, TexturedAttributes, nAttributes * sizeof(XF86AttributeRec));
   adapt->nImages = NUM_IMAGES;
   adapt->pImages = Images;
   adapt->PutVideo = NULL;
   adapt->PutStill = NULL;
   adapt->GetVideo = NULL;
   adapt->GetStill = NULL;
   adapt->StopVideo = I830StopVideo;
   adapt->SetPortAttribute = I830SetPortAttribute;
   adapt->GetPortAttribute = I830GetPortAttribute;
   adapt->QueryBestSize = I830QueryBestSize;
   adapt->PutImage = I830PutImage;
   adapt->QueryImageAttributes = I830QueryImageAttributesTextured;

   for (i = 0; i < nports; i++) {
      I830PortPrivPtr pPriv = &portPrivs[i];

      pPriv->textured = TRUE;
      pPriv->videoStatus = 0;
      pPriv->linear = NULL;
      pPriv->currentBuf = 0;
      pPriv->doubleBuffer = 0;

      /* gotta uninit this someplace, XXX: shouldn't be necessary for textured */
      REGION_NULL(pScreen, &pPriv->clip);

      adapt->pPortPrivates[i].ptr = (pointer) (pPriv);
   }

   return adapt;
}

static Bool
RegionsEqual(RegionPtr A, RegionPtr B)
{
   int *dataA, *dataB;
   int num;

   num = REGION_NUM_RECTS(A);
   if (num != REGION_NUM_RECTS(B))
      return FALSE;

   if ((A->extents.x1 != B->extents.x1) ||
       (A->extents.x2 != B->extents.x2) ||
       (A->extents.y1 != B->extents.y1) || (A->extents.y2 != B->extents.y2))
      return FALSE;

   dataA = (int *)REGION_RECTS(A);
   dataB = (int *)REGION_RECTS(B);

   while (num--) {
      if ((dataA[0] != dataB[0]) || (dataA[1] != dataB[1]))
	 return FALSE;
      dataA += 2;
      dataB += 2;
   }

   return TRUE;
}

static void
I830StopVideo(ScrnInfoPtr pScrn, pointer data, Bool shutdown)
{
   I830PortPrivPtr pPriv = (I830PortPrivPtr) data;
   I830Ptr pI830 = I830PTR(pScrn);

   I830OverlayRegPtr overlay =
	 (I830OverlayRegPtr) (pI830->FbBase + pI830->OverlayMem->Start);

   if (pPriv->textured)
      return;

   DPRINTF(PFX, "I830StopVideo\n");

   REGION_EMPTY(pScrn->pScreen, &pPriv->clip);

   if (shutdown) {
      if (pPriv->videoStatus & CLIENT_VIDEO_ON) {

	 overlay->OCMD &= ~OVERLAY_ENABLE;
	 OVERLAY_UPDATE;
#if 1
	 OVERLAY_OFF;
#endif

         if (pI830->entityPrivate)
            pI830->entityPrivate->XvInUse = -1;
      }
      if (pPriv->linear) {
	 xf86FreeOffscreenLinear(pPriv->linear);
	 pPriv->linear = NULL;
      }
      pPriv->videoStatus = 0;
   } else {
      if (pPriv->videoStatus & CLIENT_VIDEO_ON) {
	 pPriv->videoStatus |= OFF_TIMER;
	 pPriv->offTime = currentTime.milliseconds + OFF_DELAY;
      }
   }

}

static int
I830SetPortAttribute(ScrnInfoPtr pScrn,
		     Atom attribute, INT32 value, pointer data)
{
   I830PortPrivPtr pPriv = (I830PortPrivPtr) data;
   I830Ptr pI830 = I830PTR(pScrn);
   I830OverlayRegPtr overlay =
	 (I830OverlayRegPtr) (pI830->FbBase + pI830->OverlayMem->Start);

   if (pPriv->textured) {
      /* XXX: Currently the brightness/saturation attributes aren't hooked up.
       * However, apps expect them to be there, and the spec seems to let us
       * sneak out of actually implementing them for now.
       */
      return Success;
   }

   if (attribute == xvBrightness) {
      if ((value < -128) || (value > 127))
	 return BadValue;
      pPriv->brightness = value;
      overlay->OCLRC0 = (pPriv->contrast << 18) | (pPriv->brightness & 0xff);
      ErrorF("BRIGHTNESS\n");
      overlay->OCMD &= ~OVERLAY_ENABLE;
      OVERLAY_UPDATE;
#if 1
      OVERLAY_OFF;
#endif
   } else if (attribute == xvContrast) {
      if ((value < 0) || (value > 255))
	 return BadValue;
      pPriv->contrast = value;
      overlay->OCLRC0 = (pPriv->contrast << 18) | (pPriv->brightness & 0xff);
      ErrorF("CONTRAST\n");
      overlay->OCMD &= ~OVERLAY_ENABLE;
      OVERLAY_UPDATE;
#if 1
      OVERLAY_OFF;
#endif
   } else if (pI830->Clone && attribute == xvPipe) {
      if ((value < 0) || (value > 1))
         return BadValue;
      pPriv->pipe = value;
      /*
       * Select which pipe the overlay is enabled on.
       */
      overlay->OCONFIG &= ~OVERLAY_PIPE_MASK;
      if (pPriv->pipe == 0)
         overlay->OCONFIG |= OVERLAY_PIPE_A;
      else 
         overlay->OCONFIG |= OVERLAY_PIPE_B;
      ErrorF("PIPE CHANGE\n");
      overlay->OCMD &= ~OVERLAY_ENABLE;
      OVERLAY_UPDATE;
#if 1
      OVERLAY_OFF;
#endif
   } else if (attribute == xvGamma0 && (IS_I9XX(pI830))) {
      pPriv->gamma0 = value; 
   } else if (attribute == xvGamma1 && (IS_I9XX(pI830))) {
      pPriv->gamma1 = value;
   } else if (attribute == xvGamma2 && (IS_I9XX(pI830))) {
      pPriv->gamma2 = value;
   } else if (attribute == xvGamma3 && (IS_I9XX(pI830))) {
      pPriv->gamma3 = value;
   } else if (attribute == xvGamma4 && (IS_I9XX(pI830))) {
      pPriv->gamma4 = value;
   } else if (attribute == xvGamma5 && (IS_I9XX(pI830))) {
      pPriv->gamma5 = value;
   } else if (attribute == xvColorKey) {
      pPriv->colorKey = value;
      switch (pScrn->depth) {
      case 16:
	 overlay->DCLRKV = RGB16ToColorKey(pPriv->colorKey);
	 break;
      case 15:
	 overlay->DCLRKV = RGB15ToColorKey(pPriv->colorKey);
	 break;
      default:
	 overlay->DCLRKV = pPriv->colorKey;
	 break;
      }
      ErrorF("COLORKEY\n");
      overlay->OCMD &= ~OVERLAY_ENABLE;
      OVERLAY_UPDATE;
#if 1
      OVERLAY_OFF;
#endif
      REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
   } else if(attribute == xvDoubleBuffer) {
      if ((value < 0) || (value > 1))
         return BadValue;
      /* Do not allow buffer change while playing video */
      if(!*pI830->overlayOn)
     	 pPriv->doubleBuffer = value;
   } else
      return BadMatch;

   /* We've already confirmed that the overlay is off, ready for updating */
   if ((attribute == xvGamma0 ||
        attribute == xvGamma1 ||
        attribute == xvGamma2 ||
        attribute == xvGamma3 ||
        attribute == xvGamma4 ||
        attribute == xvGamma5) && (IS_I9XX(pI830))) {
        ErrorF("GAMMA\n");
        overlay->OCMD &= ~OVERLAY_ENABLE;
        OVERLAY_UPDATE;
#if 1
        OVERLAY_OFF;
#endif
	I830UpdateGamma(pScrn);
   }

   return Success;
}

static int
I830GetPortAttribute(ScrnInfoPtr pScrn,
		     Atom attribute, INT32 * value, pointer data)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830PortPrivPtr pPriv = (I830PortPrivPtr) data;

   if (attribute == xvBrightness) {
      *value = pPriv->brightness;
   } else if (attribute == xvContrast) {
      *value = pPriv->contrast;
   } else if (pI830->Clone && attribute == xvPipe) {
      *value = pPriv->pipe;
   } else if (attribute == xvGamma0 && (IS_I9XX(pI830))) {
      *value = pPriv->gamma0;
   } else if (attribute == xvGamma1 && (IS_I9XX(pI830))) {
      *value = pPriv->gamma1;
   } else if (attribute == xvGamma2 && (IS_I9XX(pI830))) {
      *value = pPriv->gamma2;
   } else if (attribute == xvGamma3 && (IS_I9XX(pI830))) {
      *value = pPriv->gamma3;
   } else if (attribute == xvGamma4 && (IS_I9XX(pI830))) {
      *value = pPriv->gamma4;
   } else if (attribute == xvGamma5 && (IS_I9XX(pI830))) {
      *value = pPriv->gamma5;
   } else if (attribute == xvColorKey) {
      *value = pPriv->colorKey;
   } else if (attribute == xvDoubleBuffer) {
      *value = pPriv->doubleBuffer;
   } else 
      return BadMatch;

   return Success;
}

static void
I830QueryBestSize(ScrnInfoPtr pScrn,
		  Bool motion,
		  short vid_w, short vid_h,
		  short drw_w, short drw_h,
		  unsigned int *p_w, unsigned int *p_h, pointer data)
{
   if (vid_w > (drw_w << 1))
      drw_w = vid_w >> 1;
   if (vid_h > (drw_h << 1))
      drw_h = vid_h >> 1;

   *p_w = drw_w;
   *p_h = drw_h;
}

static void
I830CopyPackedData(ScrnInfoPtr pScrn, I830PortPrivPtr pPriv,
		   unsigned char *buf,
		   int srcPitch,
		   int dstPitch, int top, int left, int h, int w)
{
   I830Ptr pI830 = I830PTR(pScrn);
   unsigned char *src, *dst;
   int i,j;
   unsigned char *s;

   ErrorF("I830CopyPackedData: (%d,%d) (%d,%d)\n"
	   "srcPitch: %d, dstPitch: %d\n", top, left, h, w, srcPitch, dstPitch);

   src = buf + (top * srcPitch) + (left << 1);

   if (pPriv->currentBuf == 0)
      dst = pI830->FbBase + pPriv->YBuf0offset;
   else
      dst = pI830->FbBase + pPriv->YBuf1offset;

   switch (pI830->rotation) {
      case RR_Rotate_0:
         w <<= 1;
         for (i = 0; i < h; i++) {
            memcpy(dst, src, w);
            src += srcPitch;
            dst += dstPitch;
         }
	 break;
      case RR_Rotate_90:
         h <<= 1;
         for (i = 0; i < h; i+=2) {
            s = src;
            for (j = 0; j < w; j++) {
		/* Copy Y */
               dst[(i + 0) + ((w - j - 1) * dstPitch)] = *s++; 
		(void)*s++;
            }
            src += srcPitch;
         }
         h >>= 1;
   	 src = buf + (top * srcPitch) + (left << 1);
         for (i = 0; i < h; i+=2) {
            for (j = 0; j < w; j+=2) {
		/* Copy U */
		dst[((i*2) + 1) + ((w - j - 1) * dstPitch)] = src[(j*2) + 1 + (i * srcPitch)];
		dst[((i*2) + 1) + ((w - j - 2) * dstPitch)] = src[(j*2) + 1 + ((i+1) * srcPitch)];
		/* Copy V */
		dst[((i*2) + 3) + ((w - j - 1) * dstPitch)] = src[(j*2) + 3 + (i * srcPitch)];
		dst[((i*2) + 3) + ((w - j - 2) * dstPitch)] = src[(j*2) + 3 + ((i+1) * srcPitch)];
            }
         }
         break;
      case RR_Rotate_180:
         w <<= 1;
         for (i = 0; i < h; i++) {
            s = src;
            for (j = 0; j < w; j+=4) {
               dst[(w - j - 4) + ((h - i - 1) * dstPitch)] = *s++;
               dst[(w - j - 3) + ((h - i - 1) * dstPitch)] = *s++;
               dst[(w - j - 2) + ((h - i - 1) * dstPitch)] = *s++;
               dst[(w - j - 1) + ((h - i - 1) * dstPitch)] = *s++;
            }
            src += srcPitch;
         }
         break;
      case RR_Rotate_270:
         h <<= 1;
         for (i = 0; i < h; i+=2) {
            s = src;
            for (j = 0; j < w; j++) {
		/* Copy Y */
               dst[(h - i - 2) + (j * dstPitch)] = *s++;
		(void)*s++;
            }
            src += srcPitch;
         }
         h >>= 1;
   	 src = buf + (top * srcPitch) + (left << 1);
         for (i = 0; i < h; i+=2) {
            for (j = 0; j < w; j+=2) {
		/* Copy U */
		dst[(((h - i)*2) - 3) + (j * dstPitch)] = src[(j*2) + 1 + (i * srcPitch)];
		dst[(((h - i)*2) - 3) + ((j - 1) * dstPitch)] = src[(j*2) + 1 + ((i+1) * srcPitch)];
		/* Copy V */
		dst[(((h - i)*2) - 1) + (j * dstPitch)] = src[(j*2) + 3 + (i * srcPitch)];
		dst[(((h - i)*2) - 1) + ((j - 1) * dstPitch)] = src[(j*2) + 3 + ((i+1) * srcPitch)];
            }
         }
         break;
   }
}

/* Copies planar data in *buf to UYVY-packed data in the screen atYBufXOffset.
 */
static void
I830CopyPlanarToPackedData(ScrnInfoPtr pScrn, I830PortPrivPtr pPriv,
			   unsigned char *buf, int srcPitch,
			   int srcPitch2, int dstPitch, int srcH,
			   int top, int left, int h, int w, int id)
{
   I830Ptr pI830 = I830PTR(pScrn);
   CARD8 *dst1, *srcy, *srcu, *srcv;
   int y;

   if (pPriv->currentBuf == 0)
      dst1 = pI830->FbBase + pPriv->YBuf0offset;
   else
      dst1 = pI830->FbBase + pPriv->YBuf1offset;

   srcy = buf + (top * srcPitch) + left;
   if (id == FOURCC_YV12) {
      srcu = buf + (srcH * srcPitch) + ((top / 2) * srcPitch2) + (left / 2);
      srcv = buf + (srcH * srcPitch) + ((srcH / 2) * srcPitch2) +
	    ((top / 2) * srcPitch2) + (left / 2);
   } else {
      srcv = buf + (srcH * srcPitch) + ((top / 2) * srcPitch2) + (left / 2);
      srcu = buf + (srcH * srcPitch) + ((srcH / 2) * srcPitch2) +
	    ((top / 2) * srcPitch2) + (left / 2);
   }

   for (y = 0; y < h; y++) {
      CARD32 *dst = (CARD32 *)dst1;
      CARD8 *sy = srcy;
      CARD8 *su = srcu;
      CARD8 *sv = srcv;
      int i;

      i = w / 2;
      while(i > 4) {
	 dst[0] = sy[0] | (sy[1] << 16) | (sv[0] << 8) | (su[0] << 24);
	 dst[1] = sy[2] | (sy[3] << 16) | (sv[1] << 8) | (su[1] << 24);
	 dst[2] = sy[4] | (sy[5] << 16) | (sv[2] << 8) | (su[2] << 24);
	 dst[3] = sy[6] | (sy[7] << 16) | (sv[3] << 8) | (su[3] << 24);
	 dst += 4; su += 4; sv += 4; sy += 8;
	 i -= 4;
      }
      while(i--) {
	 dst[0] = sy[0] | (sy[1] << 16) | (sv[0] << 8) | (su[0] << 24);
	 dst++; su++; sv++;
	 sy += 2;
      }

      dst1 += dstPitch;
      srcy += srcPitch;
      if (y & 1) {
	 srcu += srcPitch2;
	 srcv += srcPitch2;
      }	
   }
}

static void
I830CopyPlanarData(ScrnInfoPtr pScrn, I830PortPrivPtr pPriv,
		   unsigned char *buf, int srcPitch,
		   int srcPitch2, int dstPitch, int srcH, int top, int left,
		   int h, int w, int id)
{
   I830Ptr pI830 = I830PTR(pScrn);
   int i, j = 0;
   unsigned char *src1, *src2, *src3, *dst1, *dst2, *dst3;
   unsigned char *s;
   int dstPitch2 = dstPitch << 1;

   ErrorF("I830CopyPlanarData: srcPitch %d, srcPitch %d, dstPitch %d\n"
	   "nlines %d, npixels %d, top %d, left %d\n", srcPitch, srcPitch2, dstPitch,
	   h, w, top, left);

   /* Copy Y data */
   src1 = buf + (top * srcPitch) + left;
   ErrorF("src1 is %p, offset is %ld\n", src1,
	  (unsigned long)src1 - (unsigned long)buf);
   if (pPriv->currentBuf == 0)
      dst1 = pI830->FbBase + pPriv->YBuf0offset;
   else
      dst1 = pI830->FbBase + pPriv->YBuf1offset;

   switch (pI830->rotation) {
      case RR_Rotate_0:
         for (i = 0; i < h; i++) {
            memcpy(dst1, src1, w);
            src1 += srcPitch;
            dst1 += dstPitch2;
         }
	 break;
      case RR_Rotate_90:
         for (i = 0; i < h; i++) {
            s = src1;
            for (j = 0; j < w; j++) {
               dst1[(i) + ((w - j - 1) * dstPitch2)] = *s++;
            }
            src1 += srcPitch;
         }
         break;
      case RR_Rotate_180:
         for (i = 0; i < h; i++) {
            s = src1;
            for (j = 0; j < w; j++) {
               dst1[(w - j - 1) + ((h - i - 1) * dstPitch2)] = *s++;
            }
            src1 += srcPitch;
         }
         break;
      case RR_Rotate_270:
         for (i = 0; i < h; i++) {
            s = src1;
            for (j = 0; j < w; j++) {
               dst1[(h - i - 1) + (j * dstPitch2)] = *s++;
            }
            src1 += srcPitch;
         }
         break;
   }

   /* Copy V data for YV12, or U data for I420 */
   src2 = buf + (srcH * srcPitch) + ((top * srcPitch) >> 2) + (left >> 1);
   ErrorF("src2 is %p, offset is %ld\n", src2,
	  (unsigned long)src2 - (unsigned long)buf);
   if (pPriv->currentBuf == 0) {
      if (id == FOURCC_I420)
	 dst2 = pI830->FbBase + pPriv->UBuf0offset;
      else
	 dst2 = pI830->FbBase + pPriv->VBuf0offset;
   } else {
      if (id == FOURCC_I420)
	 dst2 = pI830->FbBase + pPriv->UBuf1offset;
      else
	 dst2 = pI830->FbBase + pPriv->VBuf1offset;
   }

   switch (pI830->rotation) {
      case RR_Rotate_0:
         for (i = 0; i < h / 2; i++) {
            memcpy(dst2, src2, w / 2);
            src2 += srcPitch2;
            dst2 += dstPitch;
         }
         break;
      case RR_Rotate_90:
         for (i = 0; i < (h/2); i++) {
            s = src2;
            for (j = 0; j < (w/2); j++) {
               dst2[(i) + (((w/2) - j - 1) * (dstPitch))] = *s++;
            }
            src2 += srcPitch2;
         }
         break;
      case RR_Rotate_180:
         for (i = 0; i < (h/2); i++) {
            s = src2;
            for (j = 0; j < (w/2); j++) {
               dst2[((w/2) - j - 1) + (((h/2) - i - 1) * dstPitch)] = *s++;
            }
            src2 += srcPitch2;
         }
         break;
      case RR_Rotate_270:
         for (i = 0; i < (h/2); i++) {
            s = src2;
            for (j = 0; j < (w/2); j++) {
               dst2[((h/2) - i - 1) + (j * dstPitch)] = *s++;
            }
            src2 += srcPitch2;
         }
         break;
   }

   /* Copy U data for YV12, or V data for I420 */
   src3 = buf + (srcH * srcPitch) + ((srcH >> 1) * srcPitch2) +
	 ((top * srcPitch) >> 2) + (left >> 1);
   ErrorF("src3 is %p, offset is %ld\n", src3,
	  (unsigned long)src3 - (unsigned long)buf);
   if (pPriv->currentBuf == 0) {
      if (id == FOURCC_I420)
	 dst3 = pI830->FbBase + pPriv->VBuf0offset;
      else
	 dst3 = pI830->FbBase + pPriv->UBuf0offset;
   } else {
      if (id == FOURCC_I420)
	 dst3 = pI830->FbBase + pPriv->VBuf1offset;
      else
	 dst3 = pI830->FbBase + pPriv->UBuf1offset;
   }

   switch (pI830->rotation) {
      case RR_Rotate_0:
         for (i = 0; i < h / 2; i++) {
            memcpy(dst3, src3, w / 2);
            src3 += srcPitch2;
            dst3 += dstPitch;
         }
         break;
      case RR_Rotate_90:
         for (i = 0; i < (h/2); i++) {
            s = src3;
            for (j = 0; j < (w/2); j++) {
               dst3[(i) + (((w/2) - j - 1) * (dstPitch))] = *s++;
            }
            src3 += srcPitch2;
         }
         break;
      case RR_Rotate_180:
         for (i = 0; i < (h/2); i++) {
            s = src3;
            for (j = 0; j < (w/2); j++) {
               dst3[((w/2) - j - 1) + (((h/2) - i - 1) * dstPitch)] = *s++;
            }
            src3 += srcPitch2;
         }
         break;
      case RR_Rotate_270:
         for (i = 0; i < (h/2); i++) {
            s = src3;
            for (j = 0; j < (w/2); j++) {
               dst3[((h/2) - i - 1) + (j * dstPitch)] = *s++;
            }
            src3 += srcPitch2;
         }
         break;
   }
}

typedef struct {
   CARD8 sign;
   CARD16 mantissa;
   CARD8 exponent;
} coeffRec, *coeffPtr;

static Bool
SetCoeffRegs(double *coeff, int mantSize, coeffPtr pCoeff, int pos)
{
   int maxVal, icoeff, res;
   int sign;
   double c;

   sign = 0;
   maxVal = 1 << mantSize;
   c = *coeff;
   if (c < 0.0) {
      sign = 1;
      c = -c;
   }

   res = 12 - mantSize;
   if ((icoeff = (int)(c * 4 * maxVal + 0.5)) < maxVal) {
      pCoeff[pos].exponent = 3;
      pCoeff[pos].mantissa = icoeff << res;
      *coeff = (double)icoeff / (double)(4 * maxVal);
   } else if ((icoeff = (int)(c * 2 * maxVal + 0.5)) < maxVal) {
      pCoeff[pos].exponent = 2;
      pCoeff[pos].mantissa = icoeff << res;
      *coeff = (double)icoeff / (double)(2 * maxVal);
   } else if ((icoeff = (int)(c * maxVal + 0.5)) < maxVal) {
      pCoeff[pos].exponent = 1;
      pCoeff[pos].mantissa = icoeff << res;
      *coeff = (double)icoeff / (double)(maxVal);
   } else if ((icoeff = (int)(c * maxVal * 0.5 + 0.5)) < maxVal) {
      pCoeff[pos].exponent = 0;
      pCoeff[pos].mantissa = icoeff << res;
      *coeff = (double)icoeff / (double)(maxVal / 2);
   } else {
      /* Coeff out of range */
      return FALSE;
   }

   pCoeff[pos].sign = sign;
   if (sign)
      *coeff = -(*coeff);
   return TRUE;
}

static void
UpdateCoeff(int taps, double fCutoff, Bool isHoriz, Bool isY, coeffPtr pCoeff)
{
   int i, j, j1, num, pos, mantSize;
   double pi = 3.1415926535, val, sinc, window, sum;
   double rawCoeff[MAX_TAPS * 32], coeffs[N_PHASES][MAX_TAPS];
   double diff;
   int tapAdjust[MAX_TAPS], tap2Fix;
   Bool isVertAndUV;

   if (isHoriz)
      mantSize = 7;
   else
      mantSize = 6;

   isVertAndUV = !isHoriz && !isY;
   num = taps * 16;
   for (i = 0; i < num  * 2; i++) {
      val = (1.0 / fCutoff) * taps * pi * (i - num) / (2 * num);
      if (val == 0.0)
	 sinc = 1.0;
      else
	 sinc = sin(val) / val;

      /* Hamming window */
      window = (0.5 - 0.5 * cos(i * pi / num));
      rawCoeff[i] = sinc * window;
   }

   for (i = 0; i < N_PHASES; i++) {
      /* Normalise the coefficients. */
      sum = 0.0;
      for (j = 0; j < taps; j++) {
	 pos = i + j * 32;
	 sum += rawCoeff[pos];
      }
      for (j = 0; j < taps; j++) {
	 pos = i + j * 32;
	 coeffs[i][j] = rawCoeff[pos] / sum;
      }

      /* Set the register values. */
      for (j = 0; j < taps; j++) {
	 pos = j + i * taps;
	 if ((j == (taps - 1) / 2) && !isVertAndUV)
	    SetCoeffRegs(&coeffs[i][j], mantSize + 2, pCoeff, pos);
	 else
	    SetCoeffRegs(&coeffs[i][j], mantSize, pCoeff, pos);
      }

      tapAdjust[0] = (taps - 1) / 2;
      for (j = 1, j1 = 1; j <= tapAdjust[0]; j++, j1++) {
	 tapAdjust[j1] = tapAdjust[0] - j;
	 tapAdjust[++j1] = tapAdjust[0] + j;
      }

      /* Adjust the coefficients. */
      sum = 0.0;
      for (j = 0; j < taps; j++)
	 sum += coeffs[i][j];
      if (sum != 1.0) {
	 for (j1 = 0; j1 < taps; j1++) {
	    tap2Fix = tapAdjust[j1];
	    diff = 1.0 - sum;
	    coeffs[i][tap2Fix] += diff;
	    pos = tap2Fix + i * taps;
	    if ((tap2Fix == (taps - 1) / 2) && !isVertAndUV)
	       SetCoeffRegs(&coeffs[i][tap2Fix], mantSize + 2, pCoeff, pos);
	    else
	       SetCoeffRegs(&coeffs[i][tap2Fix], mantSize, pCoeff, pos);

	    sum = 0.0;
	    for (j = 0; j < taps; j++)
	       sum += coeffs[i][j];
	    if (sum == 1.0)
	       break;
	 }
      }
   }
}

static void
I830DisplayVideo(ScrnInfoPtr pScrn, int id, short width, short height,
		 int dstPitch, int x1, int y1, int x2, int y2, BoxPtr dstBox,
		 short src_w, short src_h, short drw_w, short drw_h)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830PortPrivPtr pPriv = pI830->adaptor->pPortPrivates[0].ptr;
   I830OverlayRegPtr overlay =
	 (I830OverlayRegPtr) (pI830->FbBase + pI830->OverlayMem->Start);
   unsigned int swidth;
   unsigned int mask, shift, offsety, offsetu;
   int tmp;

   ErrorF("I830DisplayVideo: %dx%d (pitch %d)\n", width, height,
	   dstPitch);

   if (!pPriv->overlayOK)
      return;

#if VIDEO_DEBUG
   CompareOverlay(pI830, (CARD32 *) overlay, 0x100);
#endif

   /* When in dual head with different bpp setups we need to refresh the
    * color key, so let's reset the video parameters and refresh here */
   if (pI830->entityPrivate)
      I830ResetVideo(pScrn);

   /* Ensure overlay is turned on with OVERLAY_ENABLE at 0 */
   if (!*pI830->overlayOn)
      OVERLAY_UPDATE;

   switch (pI830->rotation) {
	case RR_Rotate_0:
		dstBox->x1 -= pScrn->frameX0;
		dstBox->x2 -= pScrn->frameX0;
		dstBox->y1 -= pScrn->frameY0;
		dstBox->y2 -= pScrn->frameY0;
		break;
	case RR_Rotate_90:
		tmp = dstBox->x1;
		dstBox->x1 = dstBox->y1 - pScrn->frameX0;
		dstBox->y1 = pScrn->virtualY - tmp - pScrn->frameY0;
		tmp = dstBox->x2;
		dstBox->x2 = dstBox->y2 - pScrn->frameX0;
		dstBox->y2 = pScrn->virtualY - tmp - pScrn->frameY0;
		tmp = dstBox->y1;
		dstBox->y1 = dstBox->y2;
		dstBox->y2 = tmp;
		break;
	case RR_Rotate_180:
		tmp = dstBox->x1;
		dstBox->x1 = pScrn->virtualX - dstBox->x2 - pScrn->frameX0;
		dstBox->x2 = pScrn->virtualX - tmp - pScrn->frameX0;
		tmp = dstBox->y1;
		dstBox->y1 = pScrn->virtualY - dstBox->y2 - pScrn->frameY0;
		dstBox->y2 = pScrn->virtualY - tmp - pScrn->frameY0;
		break;
	case RR_Rotate_270:
		tmp = dstBox->x1;
		dstBox->x1 = pScrn->virtualX - dstBox->y1 - pScrn->frameX0;
		dstBox->y1 = tmp - pScrn->frameY0;
		tmp = dstBox->x2;
		dstBox->x2 = pScrn->virtualX - dstBox->y2 - pScrn->frameX0;
		dstBox->y2 = tmp - pScrn->frameY0;
		tmp = dstBox->x1;
		dstBox->x1 = dstBox->x2;
		dstBox->x2 = tmp;
		break;
   }

   /* Fix up the dstBox if outside the visible screen */
   {
      int offset_x = (dstBox->x1 < 0) ? -dstBox->x1 : 0;
      int offset_y = (dstBox->y1 < 0) ? -dstBox->y1 : 0;
      int offset, offset2;

      /* align */
      offset_x = (offset_x + 3) & ~3;
      offset_y = (offset_y + 3) & ~3;

      if (pI830->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
         height -= offset_x;
         width -= offset_y;
      } else {
         height -= offset_y;
         width -= offset_x;
      }

      if (id == FOURCC_I420 || id == FOURCC_YV12) {
         offset = ((offset_x/2) + (dstPitch * offset_y)) * 2;
         offset2 = ((offset_x/2) + ((dstPitch/2) * offset_y));
      } else {
         offset = ((offset_x*2) + (dstPitch * offset_y));
         offset2 = ((offset_x*2) + ((dstPitch/2) * offset_y));
      }

      /* buffer locations */
      pPriv->YBuf0offset += offset;
      pPriv->UBuf0offset += offset2;
      pPriv->VBuf0offset += offset2;

      if(pPriv->doubleBuffer) {
         pPriv->YBuf1offset += offset;
         pPriv->UBuf1offset += offset2;
         pPriv->VBuf1offset += offset2;
      }
   }

   if (pI830->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
      tmp = width;
      width = height;
      height = tmp;
      tmp = drw_w;
      drw_w = drw_h;
      drw_h = tmp;
      tmp = src_w;
      src_w = src_h;
      src_h = tmp;
   }

   if (pPriv->oneLineMode) {
      /* change the coordinates with panel fitting active */
      dstBox->y1 = (((dstBox->y1 - 1) * pPriv->scaleRatio) >> 16) + 1;
      dstBox->y2 = ((dstBox->y2 * pPriv->scaleRatio) >> 16) + 1;
 
      /* Now, alter the height, so we scale to the correct size */
      drw_h = ((drw_h * pPriv->scaleRatio) >> 16) + 1;
   }

   {
      /* Keep the engine happy and clip to the real vertical size just
       * in case an LFP is in use and it's not at it's native resolution.
       */
      int vactive = pI830->pipe ? (INREG(VTOTAL_B) & 0x7FF) : (INREG(VTOTAL_A) & 0x7FF);

      vactive += 1;

      if (dstBox->y1 < 0) dstBox->y1 = 0;
      if (dstBox->y2 < 0) dstBox->y2 = 0;
      if (dstBox->x1 < 0) dstBox->x1 = 0;
      if (dstBox->x2 < 0) dstBox->x2 = 0;
      if (dstBox->y1 > vactive) dstBox->y1 = vactive;
      if (dstBox->y2 > vactive) dstBox->y2 = vactive;
      if (dstBox->x1 > pScrn->currentMode->HDisplay) dstBox->x1 = pScrn->currentMode->HDisplay - 1;
      if (dstBox->x2 > pScrn->currentMode->HDisplay) dstBox->x2 = pScrn->currentMode->HDisplay - 1;

      /* nothing do to */
      if ((!dstBox->x1 && !dstBox->x2) || (!dstBox->y1 && !dstBox->y2)) {
         ErrorF("NOTHING TO DO\n");
         return;
      }
      if ((dstBox->x1 == (pScrn->currentMode->HDisplay - 1) && 
           dstBox->x2 == (pScrn->currentMode->HDisplay - 1)) || 
          (dstBox->y1 == vactive && 
           dstBox->y2 == vactive)) {
         ErrorF("NOTHING TO DO\n");
         return;
      }
      if ((dstBox->y2 - dstBox->y1) <= N_VERT_Y_TAPS) {
         ErrorF("NOTHING TO DO\n");
         return;
      }
      if ((dstBox->x2 - dstBox->x1) <= 2) {
         ErrorF("NOTHING TO DO\n");
         return;
      }
   }

   if (IS_I9XX(pI830)) {
      shift = 6;
      mask = 0x3f;
   } else {
      shift = 5;
      mask = 0x1f;
   }

   if (pPriv->currentBuf == 0) {
      offsety = pPriv->YBuf0offset;
      offsetu = pPriv->UBuf0offset;
   } else {
      offsety = pPriv->YBuf1offset;
      offsetu = pPriv->UBuf1offset;
   }

   switch (id) {
   case FOURCC_YV12:
   case FOURCC_I420:
      swidth = width;

      overlay->SWIDTH = swidth;
      swidth /= 2;
      overlay->SWIDTH |= (swidth & 0x7ff) << 16;

      swidth = ((offsety + width + mask) >> shift) -
	    (offsety >> shift);

      if (IS_I9XX(pI830))
         swidth <<= 1;

      swidth -= 1;

      ErrorF("Y width is %d, swidth is %d\n", width, swidth);

      overlay->SWIDTHSW = swidth << 2;

      swidth = ((offsetu + (width / 2) + mask) >> shift) -
	    (offsetu >> shift);

      if (IS_I9XX(pI830))
         swidth <<= 1;

      swidth -= 1;

      ErrorF("UV width is %d, swidthsw is %d\n", width / 2, swidth);

      overlay->SWIDTHSW |= swidth << 18;

      ErrorF("HEIGHT is %d\n",height);

      overlay->SHEIGHT = height | ((height / 2) << 16);
      break;
   case FOURCC_UYVY:
   case FOURCC_YUY2:
   default:
      swidth = width;
      overlay->SWIDTH = swidth;

      ErrorF("Y width is %d\n", swidth);

      swidth = ((offsety + (width << 1) + mask) >> shift) -
	    (offsety >> shift);

      if (IS_I9XX(pI830))
         swidth <<= 1;

      swidth -= 1;

      ErrorF("swidthsw is %d\n", swidth);

      overlay->SWIDTHSW = swidth << 2;

      ErrorF("HEIGHT is %d\n",height);

      overlay->SHEIGHT = height;
      break;
   }

   overlay->OCMD = OVERLAY_ENABLE;

   overlay->DWINPOS = (dstBox->y1 << 16) | dstBox->x1;

   overlay->DWINSZ = ((dstBox->y2 - dstBox->y1) << 16) |
	 (dstBox->x2 - dstBox->x1);

   ErrorF("dstBox: x1: %d, y1: %d, x2: %d, y2: %d\n", dstBox->x1, dstBox->y1,
			dstBox->x2, dstBox->y2);

   /* buffer locations */
   overlay->OBUF_0Y = pPriv->YBuf0offset;
   overlay->OBUF_0U = pPriv->UBuf0offset;
   overlay->OBUF_0V = pPriv->VBuf0offset;

   if(pPriv->doubleBuffer) {
      overlay->OBUF_1Y = pPriv->YBuf1offset;
      overlay->OBUF_1U = pPriv->UBuf1offset;
      overlay->OBUF_1V = pPriv->VBuf1offset;
   }

   ErrorF("Buffers: Y0: 0x%lx, U0: 0x%lx, V0: 0x%lx\n", overlay->OBUF_0Y,
	  overlay->OBUF_0U, overlay->OBUF_0V);
   ErrorF("Buffers: Y1: 0x%lx, U1: 0x%lx, V1: 0x%lx\n", overlay->OBUF_1Y,
	  overlay->OBUF_1U, overlay->OBUF_1V);

#if 0
   {
      int i;

      ErrorF("First 32 bytes of Y data:\n");
      for (i = 0; i < 32; i++)
	 ErrorF(" %02x",
		((unsigned char *)pI830->FbBase + pPriv->YBuf0offset)[i]);
      ErrorF("\n");
      ErrorF("First 16 bytes of U data:\n");
      for (i = 0; i < 16; i++)
	 ErrorF(" %02x",
		((unsigned char *)pI830->FbBase + pPriv->UBuf0offset)[i]);
      ErrorF("\n");
      ErrorF("First 16 bytes of V data:\n");
      for (i = 0; i < 16; i++)
	 ErrorF(" %02x",
		((unsigned char *)pI830->FbBase + pPriv->VBuf0offset)[i]);
      ErrorF("\n");
   }
#endif

   ErrorF("pos: 0x%lx, size: 0x%lx\n", overlay->DWINPOS, overlay->DWINSZ);
   ErrorF("dst: %d x %d, src: %d x %d\n", drw_w, drw_h, src_w, src_h);

   /* 
    * Calculate horizontal and vertical scaling factors and polyphase
    * coefficients.
    */

   {
      Bool scaleChanged = FALSE;
      int xscaleInt, xscaleFract, yscaleInt, yscaleFract;
      int xscaleIntUV, xscaleFractUV;
      int yscaleIntUV, yscaleFractUV;
      /* UV is half the size of Y -- YUV420 */
      int uvratio = 2;
      CARD32 newval;
      coeffRec xcoeffY[N_HORIZ_Y_TAPS * N_PHASES];
      coeffRec xcoeffUV[N_HORIZ_UV_TAPS * N_PHASES];
      int i, j, pos;

      /*
       * Y down-scale factor as a multiple of 4096.
       */
      xscaleFract = ((src_w - 1) << 12) / drw_w;
      yscaleFract = ((src_h - 1) << 12) / drw_h;

      /* Calculate the UV scaling factor. */
      xscaleFractUV = xscaleFract / uvratio;
      yscaleFractUV = yscaleFract / uvratio;

      /*
       * To keep the relative Y and UV ratios exact, round the Y scales
       * to a multiple of the Y/UV ratio.
       */
      xscaleFract = xscaleFractUV * uvratio;
      yscaleFract = yscaleFractUV * uvratio;

      /* Integer (un-multiplied) values. */
      xscaleInt = xscaleFract >> 12;
      yscaleInt = yscaleFract >> 12;

      xscaleIntUV = xscaleFractUV >> 12;
      yscaleIntUV = yscaleFractUV >> 12;

      ErrorF("xscale: %x.%03x, yscale: %x.%03x\n", xscaleInt,
	     xscaleFract & 0xFFF, yscaleInt, yscaleFract & 0xFFF);
      ErrorF("UV xscale: %x.%03x, UV yscale: %x.%03x\n", xscaleIntUV,
	     xscaleFractUV & 0xFFF, yscaleIntUV, yscaleFractUV & 0xFFF);

      /* shouldn't get here */
      if (xscaleInt > 7) { 
         ErrorF("xscale: bad scale\n");
	 return;
      }

      /* shouldn't get here */
      if (xscaleIntUV > 7) { 
         ErrorF("xscaleUV: bad scale\n");
         return;
      }

      newval = (xscaleInt << 16) |
	    ((xscaleFract & 0xFFF) << 3) | ((yscaleFract & 0xFFF) << 20);
      if (newval != overlay->YRGBSCALE) {
	 scaleChanged = TRUE;
	 overlay->YRGBSCALE = newval;
      }

      newval = (xscaleIntUV << 16) | ((xscaleFractUV & 0xFFF) << 3) |
	    ((yscaleFractUV & 0xFFF) << 20);
      if (newval != overlay->UVSCALE) {
	 scaleChanged = TRUE;
	 overlay->UVSCALE = newval;
      }

      newval = yscaleInt << 16 | yscaleIntUV;
      if (newval != overlay->UVSCALEV) {
	 scaleChanged = TRUE;
	 overlay->UVSCALEV = newval;
      }

      /* Recalculate coefficients if the scaling changed. */

      /*
       * Only Horizontal coefficients so far.
       */
      if (scaleChanged) {
	 double fCutoffY;
	 double fCutoffUV;

	 fCutoffY = xscaleFract / 4096.0;
	 fCutoffUV = xscaleFractUV / 4096.0;

	 /* Limit to between 1.0 and 3.0. */
	 if (fCutoffY < MIN_CUTOFF_FREQ)
	    fCutoffY = MIN_CUTOFF_FREQ;
	 if (fCutoffY > MAX_CUTOFF_FREQ)
	    fCutoffY = MAX_CUTOFF_FREQ;
	 if (fCutoffUV < MIN_CUTOFF_FREQ)
	    fCutoffUV = MIN_CUTOFF_FREQ;
	 if (fCutoffUV > MAX_CUTOFF_FREQ)
	    fCutoffUV = MAX_CUTOFF_FREQ;

	 UpdateCoeff(N_HORIZ_Y_TAPS, fCutoffY, TRUE, TRUE, xcoeffY);
	 UpdateCoeff(N_HORIZ_UV_TAPS, fCutoffUV, TRUE, FALSE, xcoeffUV);

	 for (i = 0; i < N_PHASES; i++) {
	    for (j = 0; j < N_HORIZ_Y_TAPS; j++) {
	       pos = i * N_HORIZ_Y_TAPS + j;
	       overlay->Y_HCOEFS[pos] = xcoeffY[pos].sign << 15 |
					xcoeffY[pos].exponent << 12 |
					xcoeffY[pos].mantissa;
	    }
	 }
	 for (i = 0; i < N_PHASES; i++) {
	    for (j = 0; j < N_HORIZ_UV_TAPS; j++) {
	       pos = i * N_HORIZ_UV_TAPS + j;
	       overlay->UV_HCOEFS[pos] = xcoeffUV[pos].sign << 15 |
					 xcoeffUV[pos].exponent << 12 |
					 xcoeffUV[pos].mantissa;
	    }
	 }
      }
   }

   switch (id) {
   case FOURCC_YV12:
   case FOURCC_I420:
      ErrorF("YUV420\n");
#if 0
      /* set UV vertical phase to -0.25 */
      overlay->UV_VPH = 0x30003000;
#endif
      ErrorF("UV stride is %d, Y stride is %d\n", dstPitch, dstPitch * 2);
      overlay->OSTRIDE = (dstPitch * 2) | (dstPitch << 16);
      overlay->OCMD &= ~SOURCE_FORMAT;
      overlay->OCMD &= ~OV_BYTE_ORDER;
      overlay->OCMD |= YUV_420;
      break;
   case FOURCC_UYVY:
   case FOURCC_YUY2:
   default:
      ErrorF("YUV422\n");
      overlay->OSTRIDE = dstPitch;
      overlay->OCMD &= ~SOURCE_FORMAT;
      overlay->OCMD |= YUV_422;
      overlay->OCMD &= ~OV_BYTE_ORDER;
      if (id == FOURCC_UYVY)
	 overlay->OCMD |= Y_SWAP;
      break;
   }

   overlay->OCMD &= ~(BUFFER_SELECT | FIELD_SELECT);
   if (pPriv->currentBuf == 0)
      overlay->OCMD |= BUFFER0;
   else
      overlay->OCMD |= BUFFER1;

   ErrorF("OCMD is 0x%lx\n", overlay->OCMD);

   OVERLAY_UPDATE;
}

/* Doesn't matter on the order for our purposes */
typedef struct {
   unsigned char red, green, blue, alpha;
} intel_color_t;

/* Vertex format */
typedef union {
   struct {
      float x, y, z, w;
      intel_color_t color;
      intel_color_t specular;
      float u0, v0;
      float u1, v1;
      float u2, v2;
      float u3, v3;
   } v;
   float f[24];
   unsigned int  ui[24];
   unsigned char ub4[24][4];
} intelVertex, *intelVertexPtr;

static void draw_poly(CARD32 *vb,
                      float verts[][2],
                      float texcoords[][2],
		      float texcoords2[][2])
{
   int vertex_size;
   intelVertex tmp;
   int i, k;

   if (texcoords2 != NULL)
      vertex_size = 10;
   else
      vertex_size = 8;
   
   /* initial constant vertex fields */
   tmp.v.z = 1.0;
   tmp.v.w = 1.0; 
   tmp.v.color.red = 255;
   tmp.v.color.green = 255;
   tmp.v.color.blue = 255;
   tmp.v.color.alpha = 255;
   tmp.v.specular.red = 0;
   tmp.v.specular.green = 0;
   tmp.v.specular.blue = 0;
   tmp.v.specular.alpha = 0;

   for (k = 0; k < 4; k++) {
      tmp.v.x = verts[k][0];
      tmp.v.y = verts[k][1];
      tmp.v.u0 = texcoords[k][0];
      tmp.v.v0 = texcoords[k][1];
      if (texcoords2 != NULL) {
	 tmp.v.u1 = texcoords2[k][0];
	 tmp.v.v1 = texcoords2[k][1];
      }

      for (i = 0 ; i < vertex_size ; i++)
         vb[i] = tmp.ui[i];

      vb += vertex_size;
   }
}

union intfloat {
   CARD32 ui;
   float f;
};

#define OUT_RING_F(x) do {						\
   union intfloat _tmp;							\
   _tmp.f = x;								\
   OUT_RING(_tmp.ui);							\
} while (0)

#define OUT_DCL(type, nr) do {						\
   CARD32 chans = 0;							\
   if (REG_TYPE_##type == REG_TYPE_T)					\
      chans = D0_CHANNEL_ALL;						\
   else if (REG_TYPE_##type != REG_TYPE_S)				\
      FatalError("wrong reg type %d to declare\n", REG_TYPE_##type);	\
   OUT_RING(D0_DCL |							\
	    (REG_TYPE_##type << D0_TYPE_SHIFT) | (nr << D0_NR_SHIFT) |	\
	    chans);							\
   OUT_RING(0x00000000);						\
   OUT_RING(0x00000000);						\
} while (0)

#define OUT_TEXLD(dest_type, dest_nr, sampler_nr, addr_type, addr_nr)	\
do {									\
      OUT_RING(T0_TEXLD |						\
	       (REG_TYPE_##dest_type << T0_DEST_TYPE_SHIFT) |		\
	       (dest_nr << T0_DEST_NR_SHIFT) |				\
	       (sampler_nr << T0_SAMPLER_NR_SHIFT));			\
      OUT_RING((REG_TYPE_##addr_type << T1_ADDRESS_REG_TYPE_SHIFT) |	\
	       (addr_nr << T1_ADDRESS_REG_NR_SHIFT));			\
      OUT_RING(0x00000000);						\
} while (0)

/* Move the dest_chan from src0 to dest, leaving the other channels alone */
#define OUT_MOV_TO_CHANNEL(dest_type, dest_nr, src0_type, src0_nr,	\
			   dest_chan)					\
do {									\
   OUT_RING(A0_MOV | A0_DEST_CHANNEL_##dest_chan |			\
	    (REG_TYPE_##dest_type << A0_DEST_TYPE_SHIFT) |		\
	    (dest_nr << A0_DEST_NR_SHIFT) |				\
	    (REG_TYPE_##src0_type << A0_SRC0_TYPE_SHIFT) |		\
	    (src0_nr << A0_SRC0_NR_SHIFT));				\
   OUT_RING((SRC_X << A1_SRC0_CHANNEL_X_SHIFT) |			\
	    (SRC_Y << A1_SRC0_CHANNEL_Y_SHIFT) |			\
	    (SRC_Z << A1_SRC0_CHANNEL_Z_SHIFT) |			\
	    (SRC_W << A1_SRC0_CHANNEL_W_SHIFT));			\
   OUT_RING(0);								\
} while (0)

/* Dot3-product src0 and src1, storing the result in dest_chan of the dest.
 * Saturates, in case we have out-of-range YUV values.
 */
#define OUT_DP3_TO_CHANNEL(dest_type, dest_nr, src0_type, src0_nr,	\
			   src1_type, src1_nr, dest_chan)		\
do {									\
   OUT_RING(A0_DP3 | A0_DEST_CHANNEL_##dest_chan | A0_DEST_SATURATE |	\
	    (REG_TYPE_##dest_type << A0_DEST_TYPE_SHIFT) |		\
	    (dest_nr << A0_DEST_NR_SHIFT) |				\
	    (REG_TYPE_##src0_type << A0_SRC0_TYPE_SHIFT) |		\
	    (src0_nr << A0_SRC0_NR_SHIFT));				\
   OUT_RING((SRC_X << A1_SRC0_CHANNEL_X_SHIFT) |			\
	    (SRC_Y << A1_SRC0_CHANNEL_Y_SHIFT) |			\
	    (SRC_Z << A1_SRC0_CHANNEL_Z_SHIFT) |			\
	    (SRC_W << A1_SRC0_CHANNEL_W_SHIFT) |			\
	    (REG_TYPE_##src1_type << A1_SRC1_TYPE_SHIFT) |		\
	    (src1_nr << A1_SRC1_TYPE_SHIFT) |				\
	    (SRC_X << A1_SRC1_CHANNEL_X_SHIFT) |			\
	    (SRC_Y << A1_SRC1_CHANNEL_Y_SHIFT));			\
   OUT_RING((SRC_Z << A2_SRC1_CHANNEL_Z_SHIFT) |			\
	    (SRC_W << A2_SRC1_CHANNEL_W_SHIFT));			\
} while (0)

static void
I915DisplayVideoTextured(ScrnInfoPtr pScrn, I830PortPrivPtr pPriv, int id,
			 RegionPtr dstRegion,
			 short width, short height, int video_pitch,
			 int x1, int y1, int x2, int y2,
			 short src_w, short src_h, short drw_w, short drw_h,
			 DrawablePtr pDraw)
{
   I830Ptr pI830 = I830PTR(pScrn);
   CARD32 format, ms3, s2;
   BoxPtr pbox;
   int nbox, dxo, dyo;
   Bool planar;

   ErrorF("I915DisplayVideo: %dx%d (pitch %d)\n", width, height,
	  video_pitch);

   switch (id) {
   case FOURCC_UYVY:
   case FOURCC_YUY2:
      planar = FALSE;
      break;
   case FOURCC_YV12:
   case FOURCC_I420:
      planar = TRUE;
      break;
   default:
      ErrorF("Unknown format 0x%x\n", id);
      planar = FALSE;
      break;
   }

   /* XXX: Dirty dri/rotate state  */

   BEGIN_LP_RING(44);

   /* invarient state */
   OUT_RING(MI_NOOP);
   OUT_RING(STATE3D_ANTI_ALIASING |
	    LINE_CAP_WIDTH_MODIFY | LINE_CAP_WIDTH_1_0 |
	    LINE_WIDTH_MODIFY | LINE_WIDTH_1_0);

   OUT_RING(STATE3D_DFLT_DIFFUSE_CMD);
   OUT_RING(0x00000000);

   OUT_RING(STATE3D_DFLT_SPEC_CMD);
   OUT_RING(0x00000000);

   OUT_RING(STATE3D_DFLT_Z_CMD);
   OUT_RING(0x00000000);

   OUT_RING(STATE3D_COORD_SET_BINDINGS | CSB_TCB(0, 0) | CSB_TCB(1, 1) |
	    CSB_TCB(2,2) | CSB_TCB(3,3) | CSB_TCB(4,4) | CSB_TCB(5,5) |
	    CSB_TCB(6,6) | CSB_TCB(7,7));

   OUT_RING(STATE3D_RASTERIZATION_RULES |
	    ENABLE_TRI_FAN_PROVOKE_VRTX | TRI_FAN_PROVOKE_VRTX(2) |
	    ENABLE_LINE_STRIP_PROVOKE_VRTX | LINE_STRIP_PROVOKE_VRTX(1) |
	    ENABLE_TEXKILL_3D_4D | TEXKILL_4D |
	    ENABLE_POINT_RASTER_RULE | OGL_POINT_RASTER_RULE);

   OUT_RING(STATE3D_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(3) | 1);
   OUT_RING(0x00000000); /* texture coordinate wrap */

   /* flush map & render cache */
   OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
   OUT_RING(0x00000000);

   /* draw rect -- just clipping */
   OUT_RING(STATE3D_DRAWING_RECTANGLE);
   OUT_RING(0x00000000);	/* flags */
   OUT_RING(0x00000000);	/* ymin, xmin */
   OUT_RING((pScrn->virtualX - 1) | (pScrn->virtualY - 1) << 16); /* ymax, xmax */
   OUT_RING(0x00000000);	/* yorigin, xorigin */
   OUT_RING(MI_NOOP);

   /* scissor */
   OUT_RING(STATE3D_SCISSOR_ENABLE | DISABLE_SCISSOR_RECT);
   OUT_RING(STATE3D_SCISSOR_RECTANGLE);
   OUT_RING(0x00000000);	/* ymin, xmin */
   OUT_RING(0x00000000);	/* ymax, xmax */

   OUT_RING(0x7c000003);	/* unknown command */
   OUT_RING(0x7d070000);
   OUT_RING(0x00000000);
   OUT_RING(0x68000002);

   /* context setup */
   OUT_RING(STATE3D_MODES_4 |
	    ENABLE_LOGIC_OP_FUNC | LOGIC_OP_FUNC(LOGICOP_COPY) |
	    ENABLE_STENCIL_WRITE_MASK | STENCIL_WRITE_MASK(0xff) |
	    ENABLE_STENCIL_TEST_MASK | STENCIL_TEST_MASK(0xff));

   OUT_RING(STATE3D_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(2) |
	    I1_LOAD_S(4) | I1_LOAD_S(5) | I1_LOAD_S(6) | 4);
   s2 = S2_TEXCOORD_FMT(0, TEXCOORDFMT_2D);
   if (planar)
      s2 |= S2_TEXCOORD_FMT(1, TEXCOORDFMT_2D);
   else
      s2 |= S2_TEXCOORD_FMT(1, TEXCOORDFMT_NOT_PRESENT);
   s2 |= S2_TEXCOORD_FMT(2, TEXCOORDFMT_NOT_PRESENT) |
      S2_TEXCOORD_FMT(3, TEXCOORDFMT_NOT_PRESENT) |
      S2_TEXCOORD_FMT(4, TEXCOORDFMT_NOT_PRESENT) |
      S2_TEXCOORD_FMT(5, TEXCOORDFMT_NOT_PRESENT) |
      S2_TEXCOORD_FMT(6, TEXCOORDFMT_NOT_PRESENT) |
      S2_TEXCOORD_FMT(7, TEXCOORDFMT_NOT_PRESENT);
   OUT_RING(s2);
   OUT_RING((1 << S4_POINT_WIDTH_SHIFT) | S4_LINE_WIDTH_ONE |
	    S4_CULLMODE_NONE | S4_VFMT_SPEC_FOG | S4_VFMT_COLOR | S4_VFMT_XYZW);
   OUT_RING(0x00000000); /* S5 - enable bits */
   OUT_RING((2 << S6_DEPTH_TEST_FUNC_SHIFT) |
	    (2 << S6_CBUF_SRC_BLEND_FACT_SHIFT) |
	    (1 << S6_CBUF_DST_BLEND_FACT_SHIFT) | S6_COLOR_WRITE_ENABLE |
	    (2 << S6_TRISTRIP_PV_SHIFT));

   OUT_RING(STATE3D_INDEPENDENT_ALPHA_BLEND |
	    IAB_MODIFY_ENABLE |
	    IAB_MODIFY_FUNC | (BLENDFUNC_ADD << IAB_FUNC_SHIFT) |
	    IAB_MODIFY_SRC_FACTOR | (BLENDFACT_ONE << IAB_SRC_FACTOR_SHIFT) |
	    IAB_MODIFY_DST_FACTOR | (BLENDFACT_ZERO << IAB_DST_FACTOR_SHIFT));

   OUT_RING(STATE3D_CONST_BLEND_COLOR);
   OUT_RING(0x00000000);

   OUT_RING(STATE3D_DEST_BUFFER_VARIABLES);
   if (pI830->cpp == 2)
      format = COLR_BUF_RGB565;
   else
      format = COLR_BUF_ARGB8888 | DEPTH_FRMT_24_FIXED_8_OTHER;

   OUT_RING(LOD_PRECLAMP_OGL |
     DSTORG_HORIZ_BIAS(0x80) | DSTORG_VERT_BIAS(0x80) | format);

   OUT_RING(STATE3D_STIPPLE);
   OUT_RING(0x00000000);

   /* front buffer, pitch, offset */
   OUT_RING(STATE3D_BUFFER_INFO);
   OUT_RING(BUFFERID_COLOR_BACK | BUFFER_USE_FENCES |
	    (((pI830->displayWidth * pI830->cpp) / 4) << 2));
   OUT_RING(pI830->bufferOffset);
   ADVANCE_LP_RING();

   if (!planar) {
      BEGIN_LP_RING(20);
      /* fragment program - texture blend replace. */
      OUT_RING(STATE3D_PIXEL_SHADER_PROGRAM | 8);
      OUT_DCL(S, 0);
      OUT_DCL(T, 0);
      OUT_TEXLD(OC, 0, 0, T, 0);
      /* End fragment program */

      OUT_RING(STATE3D_SAMPLER_STATE | 3);
      OUT_RING(0x00000001);
      OUT_RING(SS2_COLORSPACE_CONVERSION);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);

      OUT_RING(STATE3D_MAP_STATE | 3);
      OUT_RING(0x00000001);	/* texture map #1 */
      OUT_RING(pPriv->YBuf0offset);
      ms3 = MAPSURF_422;
      switch (id) {
      case FOURCC_YUY2:
	 ms3 |= MT_422_YCRCB_NORMAL;
	 break;
      case FOURCC_UYVY:
	 ms3 |= MT_422_YCRCB_SWAPY;
	 break;
      }
      ms3 |= (height - 1) << MS3_HEIGHT_SHIFT;
      ms3 |= (width - 1) << MS3_WIDTH_SHIFT;
      if (!pI830->disableTiling)
	 ms3 |= MS3_USE_FENCE_REGS;
      OUT_RING(ms3);
      OUT_RING(((video_pitch / 4) - 1) << 21);
      ADVANCE_LP_RING();
   } else {
      BEGIN_LP_RING(1 + 18 + (1 + 3*16) + 11 + 11);
      OUT_RING(MI_NOOP);
      /* For the planar formats, we set up three samplers -- one for each plane,
       * in a Y8 format.  Because I couldn't get the special PLANAR_TO_PACKED
       * shader setup to work, I did the manual pixel shader:
       *
       * y' = y - .0625
       * u' = u - .5
       * v' = v - .5;
       *
       * r = 1.1643 * y' + 0.0     * u' + 1.5958  * v'
       * g = 1.1643 * y' - 0.39173 * u' - 0.81290 * v'
       * b = 1.1643 * y' + 2.017   * u' + 0.0     * v'
       *
       * register assignment:
       * r0 = (y',u',v',0)
       * r1 = (y,y,y,y)
       * r2 = (u,u,u,u)
       * r3 = (v,v,v,v)
       * OC = (r,g,b,1)
       */
      OUT_RING(STATE3D_PIXEL_SHADER_CONSTANTS | 16);
      OUT_RING(0x000000f);	/* constants 0-3 */
      /* constant 0: normalization offsets */
      OUT_RING_F(-0.0625);
      OUT_RING_F(-0.5);
      OUT_RING_F(-0.5);
      OUT_RING_F(0.0);
      /* constant 1: r coefficients*/
      OUT_RING_F(1.1643);
      OUT_RING_F(0.0);
      OUT_RING_F(1.5958);
      OUT_RING_F(0.0);
      /* constant 2: g coefficients */
      OUT_RING_F(1.1643);
      OUT_RING_F(-0.39173);
      OUT_RING_F(-0.81290);
      OUT_RING_F(0.0);
      /* constant 3: b coefficients */
      OUT_RING_F(1.1643);
      OUT_RING_F(2.017);
      OUT_RING_F(0.0);
      OUT_RING_F(0.0);

      OUT_RING(STATE3D_PIXEL_SHADER_PROGRAM | (3 * 16 - 1));
      /* Declare samplers */
      OUT_DCL(S, 0);
      OUT_DCL(S, 1);
      OUT_DCL(S, 2);
      OUT_DCL(T, 0);
      OUT_DCL(T, 1);

      /* Load samplers to temporaries.  Y (sampler 0) gets the un-halved coords
       * from t1.
       */
      OUT_TEXLD(R, 1, 0, T, 1);
      OUT_TEXLD(R, 2, 1, T, 0);
      OUT_TEXLD(R, 3, 2, T, 0);

      /* Move the sampled YUV data in R[123] to the first 3 channels of R0. */
      OUT_MOV_TO_CHANNEL(R, 0, R, 1, X);
      OUT_MOV_TO_CHANNEL(R, 0, R, 2, Y);
      OUT_MOV_TO_CHANNEL(R, 0, R, 3, Z);

      /* Normalize the YUV data */
      OUT_RING(A0_ADD | A0_DEST_CHANNEL_ALL |
	       (REG_TYPE_R << A0_DEST_TYPE_SHIFT) | (0 << A0_DEST_NR_SHIFT) |				\
	       (REG_TYPE_R << A0_SRC0_TYPE_SHIFT) | (0 << A0_SRC0_NR_SHIFT));
      OUT_RING((SRC_X << A1_SRC0_CHANNEL_X_SHIFT) |
	       (SRC_Y << A1_SRC0_CHANNEL_Y_SHIFT) |
	       (SRC_Z << A1_SRC0_CHANNEL_Z_SHIFT) |
	       (SRC_W << A1_SRC0_CHANNEL_W_SHIFT) |
	       (REG_TYPE_CONST << A1_SRC1_TYPE_SHIFT) | (0 << A1_SRC1_NR_SHIFT) |
	       (SRC_X << A1_SRC1_CHANNEL_X_SHIFT) |
	       (SRC_Y << A1_SRC1_CHANNEL_Y_SHIFT));
      OUT_RING((SRC_Z << A2_SRC1_CHANNEL_Z_SHIFT) |
	       (SRC_W << A2_SRC1_CHANNEL_W_SHIFT));

      /* dot-product the YUV data in R0 by the vectors of coefficients for
       * calculating R, G, and B, storing the results in the R, G, or B channels
       * of the output color.
       */
      OUT_DP3_TO_CHANNEL(OC, 0, R, 0, CONST, 1, X);
      OUT_DP3_TO_CHANNEL(OC, 0, R, 0, CONST, 2, Y);
      OUT_DP3_TO_CHANNEL(OC, 0, R, 0, CONST, 3, Z);

      /* Set alpha of the output to 1.0, by wiring W to 1 and not actually using
       * the source.
       */
      OUT_RING(A0_MOV | A0_DEST_CHANNEL_W |
	       (REG_TYPE_OC << A0_DEST_TYPE_SHIFT) | (0 << A0_DEST_NR_SHIFT) |
	       (REG_TYPE_OC << A0_SRC0_TYPE_SHIFT) | (0 << A0_SRC0_NR_SHIFT));
      OUT_RING((SRC_X << A1_SRC0_CHANNEL_X_SHIFT) |
	       (SRC_Y << A1_SRC0_CHANNEL_Y_SHIFT) |
	       (SRC_Z << A1_SRC0_CHANNEL_Z_SHIFT) |
	       (SRC_ONE << A1_SRC0_CHANNEL_W_SHIFT));
      OUT_RING(0);
      /* End fragment program */

      OUT_RING(STATE3D_SAMPLER_STATE | 9);
      OUT_RING(0x00000007);
      /* sampler 0 */
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      /* sampler 1 */
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      /* sampler 2 */
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);

      OUT_RING(STATE3D_MAP_STATE | 9);
      OUT_RING(0x00000007);

      OUT_RING(pPriv->YBuf0offset);
      ms3 = MAPSURF_8BIT | MT_8BIT_I8;
      ms3 |= (height - 1) << MS3_HEIGHT_SHIFT;
      ms3 |= (width - 1) << MS3_WIDTH_SHIFT;
      OUT_RING(ms3);
      OUT_RING(((video_pitch * 2 / 4) - 1) << 21);

      OUT_RING(pPriv->UBuf0offset);
      ms3 = MAPSURF_8BIT | MT_8BIT_I8;
      ms3 |= (height / 2 - 1) << MS3_HEIGHT_SHIFT;
      ms3 |= (width / 2 - 1) << MS3_WIDTH_SHIFT;
      OUT_RING(ms3);
      OUT_RING(((video_pitch / 4) - 1) << 21);

      OUT_RING(pPriv->VBuf0offset);
      ms3 = MAPSURF_8BIT | MT_8BIT_I8;
      ms3 |= (height / 2 - 1) << MS3_HEIGHT_SHIFT;
      ms3 |= (width / 2 - 1) << MS3_WIDTH_SHIFT;
      OUT_RING(ms3);
      OUT_RING(((video_pitch / 4) - 1) << 21);
      ADVANCE_LP_RING();
   }
   
   {
      BEGIN_LP_RING(2);
      OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
      OUT_RING(0x00000000);
      ADVANCE_LP_RING();
   }

   dxo = dstRegion->extents.x1;
   dyo = dstRegion->extents.y1;

   pbox = REGION_RECTS(dstRegion);
   nbox = REGION_NUM_RECTS(dstRegion);
   while (nbox--)
   {
      int box_x1 = pbox->x1;
      int box_y1 = pbox->y1;
      int box_x2 = pbox->x2;
      int box_y2 = pbox->y2;
      int j;
      float src_scale_x, src_scale_y;
      CARD32 vb[40];
      float verts[4][2], tex[4][2], tex2[4][2];
      int vert_data_count;

      pbox++;

      src_scale_x = (float)src_w / (float)drw_w;
      src_scale_y  = (float)src_h / (float)drw_h;

      if (!planar)
	 vert_data_count = 32;
      else
	 vert_data_count = 40;

      BEGIN_LP_RING(vert_data_count + 8);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);

      /* vertex data */
      OUT_RING(PRIMITIVE3D | PRIM3D_INLINE | PRIM3D_TRIFAN |
	       (vert_data_count - 1));
      verts[0][0] = box_x1; verts[0][1] = box_y1;
      verts[1][0] = box_x2; verts[1][1] = box_y1;
      verts[2][0] = box_x2; verts[2][1] = box_y2;
      verts[3][0] = box_x1; verts[3][1] = box_y2;

      if (!planar) {
	 tex[0][0] = (box_x1 - dxo) * src_scale_x;
	 tex[0][1] = (box_y1 - dyo) * src_scale_y;
	 tex[1][0] = (box_x2 - dxo) * src_scale_x;
	 tex[1][1] = (box_y1 - dyo) * src_scale_y;
	 tex[2][0] = (box_x2 - dxo) * src_scale_x;
	 tex[2][1] = (box_y2 - dyo) * src_scale_y;
	 tex[3][0] = (box_x1 - dxo) * src_scale_x;
	 tex[3][1] = (box_y2 - dyo) * src_scale_y;
	 /* emit vertex buffer */
	 draw_poly(vb, verts, tex, NULL);
	 for (j = 0; j < vert_data_count; j++)
	    OUT_RING(vb[j]);
      } else {
	 tex[0][0] = (box_x1 - dxo) * src_scale_x / 2.0;
	 tex[0][1] = (box_y1 - dyo) * src_scale_y / 2.0;
	 tex[1][0] = (box_x2 - dxo) * src_scale_x / 2.0;
	 tex[1][1] = (box_y1 - dyo) * src_scale_y / 2.0;
	 tex[2][0] = (box_x2 - dxo) * src_scale_x / 2.0;
	 tex[2][1] = (box_y2 - dyo) * src_scale_y / 2.0;
	 tex[3][0] = (box_x1 - dxo) * src_scale_x / 2.0;
	 tex[3][1] = (box_y2 - dyo) * src_scale_y / 2.0;
	 tex2[0][0] = (box_x1 - dxo) * src_scale_x;
	 tex2[0][1] = (box_y1 - dyo) * src_scale_y;
	 tex2[1][0] = (box_x2 - dxo) * src_scale_x;
	 tex2[1][1] = (box_y1 - dyo) * src_scale_y;
	 tex2[2][0] = (box_x2 - dxo) * src_scale_x;
	 tex2[2][1] = (box_y2 - dyo) * src_scale_y;
	 tex2[3][0] = (box_x1 - dxo) * src_scale_x;
	 tex2[3][1] = (box_y2 - dyo) * src_scale_y;
	 /* emit vertex buffer */
	 draw_poly(vb, verts, tex, tex2);
	 for (j = 0; j < vert_data_count; j++)
	    OUT_RING(vb[j]);
      }

      ADVANCE_LP_RING();
   }

   if (pI830->AccelInfoRec)
      pI830->AccelInfoRec->NeedToSync = TRUE;
}

static FBLinearPtr
I830AllocateMemory(ScrnInfoPtr pScrn, FBLinearPtr linear, int size)
{
   ScreenPtr pScreen;
   FBLinearPtr new_linear = NULL;

   DPRINTF(PFX, "I830AllocateMemory\n");

   if (linear) {
      if (linear->size >= size)
	 return linear;

      if (xf86ResizeOffscreenLinear(linear, size))
	 return linear;

      xf86FreeOffscreenLinear(linear);
   }

   pScreen = screenInfo.screens[pScrn->scrnIndex];

   new_linear = xf86AllocateOffscreenLinear(pScreen, size, 4,
					    NULL, NULL, NULL);

   if (!new_linear) {
      int max_size;

      xf86QueryLargestOffscreenLinear(pScreen, &max_size, 4,
				      PRIORITY_EXTREME);

      if (max_size < size) {
         ErrorF("No memory available\n");
	 return NULL;
      }

      xf86PurgeUnlockedOffscreenAreas(pScreen);
      new_linear = xf86AllocateOffscreenLinear(pScreen, size, 4,
					       NULL, NULL, NULL);
   }

   return new_linear;
}

/*
 * The source rectangle of the video is defined by (src_x, src_y, src_w, src_h).
 * The dest rectangle of the video is defined by (drw_x, drw_y, drw_w, drw_h).
 * id is a fourcc code for the format of the video.
 * buf is the pointer to the source data in system memory.
 * width and height are the w/h of the source data.
 * If "sync" is TRUE, then we must be finished with *buf at the point of return
 * (which we always are).
 * clipBoxes is the clipping region in screen space.
 * data is a pointer to our port private.
 * pDraw is a Drawable, which might not be the screen in the case of
 * compositing.  It's a new argument to the function in the 1.1 server.
 */
static int
I830PutImage(ScrnInfoPtr pScrn,
	     short src_x, short src_y,
	     short drw_x, short drw_y,
	     short src_w, short src_h,
	     short drw_w, short drw_h,
	     int id, unsigned char *buf,
	     short width, short height,
	     Bool sync, RegionPtr clipBoxes, pointer data,
	     DrawablePtr pDraw)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830PortPrivPtr pPriv = (I830PortPrivPtr) data;
   ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];
   I830OverlayRegPtr overlay =
	 (I830OverlayRegPtr) (pI830->FbBase + pI830->OverlayMem->Start);
   INT32 x1, x2, y1, y2;
   int srcPitch, srcPitch2 = 0, dstPitch, destId;
   int top, left, npixels, nlines, size, loops;
   BoxRec dstBox;
   int pitchAlignMask;

   DPRINTF(PFX, "I830PutImage: src: (%d,%d)(%d,%d), dst: (%d,%d)(%d,%d)\n"
	   "width %d, height %d\n", src_x, src_y, src_w, src_h, drw_x, drw_y,
	   drw_w, drw_h, width, height);

   if (pI830->entityPrivate) {
	 if (pI830->entityPrivate->XvInUse != -1 &&
	     pI830->entityPrivate->XvInUse != pPriv->pipe) {
#ifdef PANORAMIX
		if (!noPanoramiXExtension) {
			return Success; /* faked for trying to share it */
		} else
#endif
		{
			return BadAlloc;
		}
	 }

      pI830->entityPrivate->XvInUse = pPriv->pipe;
   }

   /* overlay limits */
   if(src_w > (drw_w * 7))
      drw_w = src_w * 7;

   if(src_h > (drw_h * 7))
      drw_h = src_h * 7;

   /* Clip */
   x1 = src_x;
   x2 = src_x + src_w;
   y1 = src_y;
   y2 = src_y + src_h;

   dstBox.x1 = drw_x;
   dstBox.x2 = drw_x + drw_w;
   dstBox.y1 = drw_y;
   dstBox.y2 = drw_y + drw_h;

   if (!xf86XVClipVideoHelper(&dstBox, &x1, &x2, &y1, &y2, clipBoxes,
			      width, height))
      return Success;

   destId = id;
   switch (id) {
   case FOURCC_YV12:
   case FOURCC_I420:
      srcPitch = (width + 3) & ~3;
      srcPitch2 = ((width >> 1) + 3) & ~3;
      if (pPriv->textured) {
	 destId = FOURCC_YUY2;
      }
      break;
   case FOURCC_UYVY:
   case FOURCC_YUY2:
   default:
      srcPitch = width << 1;
      break;
   }

   /* Only needs to be DWORD-aligned for textured on i915, but overlay has
    * stricter requirements.
    */
   if (pPriv->textured) {
      pitchAlignMask = 3;
   } else {
      pitchAlignMask = 63;
   }

   /* Determine the desired destination pitch (representing the chroma's pitch,
    * in the planar case.
    */
   switch (destId) {
   case FOURCC_YV12:
   case FOURCC_I420:
      if (pI830->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
         dstPitch = ((height / 2) + pitchAlignMask) & ~pitchAlignMask;
         size = dstPitch * width * 3;
      } else {
         dstPitch = ((width / 2) + pitchAlignMask) & ~pitchAlignMask;
         size = dstPitch * height * 3;
      }
      break;
   case FOURCC_UYVY:
   case FOURCC_YUY2:
   default:
      if (pI830->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
         dstPitch = ((height << 1) + pitchAlignMask) & ~pitchAlignMask;
         size = dstPitch * width;
      } else {
         dstPitch = ((width << 1) + pitchAlignMask) & ~pitchAlignMask;
         size = dstPitch * height;
      }
      break;
   }
   ErrorF("srcPitch: %d, dstPitch: %d, size: %d\n", srcPitch, dstPitch, size);

   /* size is multiplied by 2 because we have two buffers that are flipping */
   pPriv->linear = I830AllocateMemory(pScrn, pPriv->linear,
		   (pPriv->doubleBuffer ? size * 2 : size) / pI830->cpp);

   if(!pPriv->linear || pPriv->linear->offset < (pScrn->virtualX * pScrn->virtualY))
      return BadAlloc;

   /* fixup pointers */
   pPriv->YBuf0offset = pI830->FrontBuffer.Start + pPriv->linear->offset * pI830->cpp;
   if (pI830->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
      pPriv->UBuf0offset = pPriv->YBuf0offset + (dstPitch * 2 * width);
      pPriv->VBuf0offset = pPriv->UBuf0offset + (dstPitch * width / 2);
      if(pPriv->doubleBuffer) {
         pPriv->YBuf1offset = pPriv->YBuf0offset + size;
         pPriv->UBuf1offset = pPriv->YBuf1offset + (dstPitch * 2 * width);
         pPriv->VBuf1offset = pPriv->UBuf1offset + (dstPitch * width / 2);
      }
   } else {
      pPriv->UBuf0offset = pPriv->YBuf0offset + (dstPitch * 2 * height);
      pPriv->VBuf0offset = pPriv->UBuf0offset + (dstPitch * height / 2);
      if(pPriv->doubleBuffer) {
         pPriv->YBuf1offset = pPriv->YBuf0offset + size;
         pPriv->UBuf1offset = pPriv->YBuf1offset + (dstPitch * 2 * height);
         pPriv->VBuf1offset = pPriv->UBuf1offset + (dstPitch * height / 2);
      }
   }

   /* Make sure this buffer isn't in use */
   loops = 0;
   if (!pPriv->textured && *pI830->overlayOn && pPriv->doubleBuffer &&
       (overlay->OCMD & OVERLAY_ENABLE))
   {
      while (loops < 1000000) {
#if USE_USLEEP_FOR_VIDEO
         usleep(10);
#endif
         if (((INREG(DOVSTA) & OC_BUF) >> 20) == pPriv->currentBuf) {
	    break;
         }
         loops++;
      }
      if (loops >= 1000000) {
         ErrorF("loops (1) maxed out for buffer %d\n", pPriv->currentBuf);
#if 0
         pPriv->currentBuf = !pPriv->currentBuf;
#endif
      }

      /* buffer swap */
      if (pPriv->currentBuf == 0)
         pPriv->currentBuf = 1;
      else
         pPriv->currentBuf = 0;
   }

   /* copy data */
   top = y1 >> 16;
   left = (x1 >> 16) & ~1;
   npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;

   if (pPriv->textured) {
      /* For textured video, we don't double buffer, and instead just wait for
       * acceleration to finish before writing the new video data into
       * framebuffer.
       */
      if (pI830->AccelInfoRec && pI830->AccelInfoRec->NeedToSync) {
	 (*pI830->AccelInfoRec->Sync)(pScrn);
	 pI830->AccelInfoRec->NeedToSync = FALSE;
      }
   }

   switch (id) {
   case FOURCC_YV12:
   case FOURCC_I420:
      top &= ~1;
      nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
      if (pPriv->textured) {
	 I830CopyPlanarToPackedData(pScrn, pPriv, buf, srcPitch, srcPitch2,
				    dstPitch, height, top, left, nlines,
				    npixels, id);
      } else {
	 I830CopyPlanarData(pScrn, pPriv, buf, srcPitch, srcPitch2, dstPitch,
			    height, top, left, nlines, npixels, id);
      }
      break;
   case FOURCC_UYVY:
   case FOURCC_YUY2:
   default:
      nlines = ((y2 + 0xffff) >> 16) - top;
      I830CopyPackedData(pScrn, pPriv, buf, srcPitch, dstPitch, top, left,
			 nlines, npixels);
      break;
   }

   if (!pPriv->textured) {
      /* update cliplist */
      if (!RegionsEqual(&pPriv->clip, clipBoxes)) {
 	REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);
	 xf86XVFillKeyHelper(pScreen, pPriv->colorKey, clipBoxes);
      }

      I830DisplayVideo(pScrn, destId, width, height, dstPitch,
		       x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h);
   } else {
      I915DisplayVideoTextured(pScrn, pPriv, destId, clipBoxes, width, height,
			       dstPitch, x1, y1, x2, y2,
			       src_w, src_h, drw_w, drw_h, pDraw);
   }
   pPriv->videoStatus = CLIENT_VIDEO_ON;

   return Success;
}

static int
I830QueryImageAttributes(ScrnInfoPtr pScrn,
			 int id,
			 unsigned short *w, unsigned short *h,
			 int *pitches, int *offsets, Bool textured)
{
   I830Ptr pI830 = I830PTR(pScrn);
   int size, tmp;

   ErrorF("I830QueryImageAttributes: w is %d, h is %d\n", *w, *h);

   if (!textured) {
      if (IS_845G(pI830) || IS_I830(pI830)) {
	 if (*w > IMAGE_MAX_WIDTH_LEGACY)
	    *w = IMAGE_MAX_WIDTH_LEGACY;
	 if (*h > IMAGE_MAX_HEIGHT_LEGACY)
	    *h = IMAGE_MAX_HEIGHT_LEGACY;
      } else {
	 if (*w > IMAGE_MAX_WIDTH)
	    *w = IMAGE_MAX_WIDTH;
	 if (*h > IMAGE_MAX_HEIGHT)
	    *h = IMAGE_MAX_HEIGHT;
      }
   }

   *w = (*w + 1) & ~1;
   if (offsets)
      offsets[0] = 0;

   switch (id) {
      /* IA44 is for XvMC only */
   case FOURCC_IA44:
   case FOURCC_AI44:
      if (pitches)
	 pitches[0] = *w;
      size = *w * *h;
      break;
   case FOURCC_YV12:
   case FOURCC_I420:
      *h = (*h + 1) & ~1;
      size = (*w + 3) & ~3;
      if (pitches)
	 pitches[0] = size;
      size *= *h;
      if (offsets)
	 offsets[1] = size;
      tmp = ((*w >> 1) + 3) & ~3;
      if (pitches)
	 pitches[1] = pitches[2] = tmp;
      tmp *= (*h >> 1);
      size += tmp;
      if (offsets)
	 offsets[2] = size;
      size += tmp;
      if (pitches)
	 ErrorF("pitch 0 is %d, pitch 1 is %d, pitch 2 is %d\n", pitches[0],
		pitches[1], pitches[2]);
      if (offsets)
	 ErrorF("offset 1 is %d, offset 2 is %d\n", offsets[1], offsets[2]);
      if (offsets)
	 ErrorF("size is %d\n", size);
      break;
   case FOURCC_UYVY:
   case FOURCC_YUY2:
   default:
      size = *w << 1;
      if (pitches)
	 pitches[0] = size;
      size *= *h;
      break;
   }

   return size;
}

static int
I830QueryImageAttributesOverlay(ScrnInfoPtr pScrn,
				int id,
				unsigned short *w, unsigned short *h,
				int *pitches, int *offsets)
{
   return I830QueryImageAttributes(pScrn, id, w, h, pitches, offsets, FALSE);
}

static int
I830QueryImageAttributesTextured(ScrnInfoPtr pScrn,
				 int id,
				 unsigned short *w, unsigned short *h,
				 int *pitches, int *offsets)
{
   return I830QueryImageAttributes(pScrn, id, w, h, pitches, offsets, TRUE);
}

static void
I830BlockHandler(int i,
		 pointer blockData, pointer pTimeout, pointer pReadmask)
{
   ScreenPtr pScreen = screenInfo.screens[i];
   ScrnInfoPtr pScrn = xf86Screens[i];
   I830Ptr pI830 = I830PTR(pScrn);
   I830PortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);
   I830OverlayRegPtr overlay =
	 (I830OverlayRegPtr) (pI830->FbBase + pI830->OverlayMem->Start);

   pScreen->BlockHandler = pI830->BlockHandler;

   (*pScreen->BlockHandler) (i, blockData, pTimeout, pReadmask);

   pScreen->BlockHandler = I830BlockHandler;

   if (pPriv->videoStatus & TIMER_MASK) {
      Time now = currentTime.milliseconds;
      if (pPriv->videoStatus & OFF_TIMER) {
	 if (pPriv->offTime < now) {
	    /* Turn off the overlay */
	    ErrorF("BLOCKHANDLER\n");
	    overlay->OCMD &= ~OVERLAY_ENABLE;
            OVERLAY_UPDATE;
#if 1
            OVERLAY_OFF;
#endif

	    pPriv->videoStatus = FREE_TIMER;
	    pPriv->freeTime = now + FREE_DELAY;
       
            if (pI830->entityPrivate)
               pI830->entityPrivate->XvInUse = -1;
	 }
      } else {				/* FREE_TIMER */
	 if (pPriv->freeTime < now) {
	    if (pPriv->linear) {
	       xf86FreeOffscreenLinear(pPriv->linear);
	       pPriv->linear = NULL;
	    }
	    pPriv->videoStatus = 0;
	 }
      }
   }
}

/***************************************************************************
 * Offscreen Images
 ***************************************************************************/

typedef struct {
   FBLinearPtr linear;
   Bool isOn;
} OffscreenPrivRec, *OffscreenPrivPtr;

static int
I830AllocateSurface(ScrnInfoPtr pScrn,
		    int id,
		    unsigned short w,
		    unsigned short h, XF86SurfacePtr surface)
{
   FBLinearPtr linear;
   int pitch, fbpitch, size, bpp;
   OffscreenPrivPtr pPriv;
   I830Ptr pI830 = I830PTR(pScrn);

   ErrorF("I830AllocateSurface\n");

   if (IS_845G(pI830) || IS_I830(pI830)) {
      if ((w > IMAGE_MAX_WIDTH_LEGACY) || (h > IMAGE_MAX_HEIGHT_LEGACY))
         return BadAlloc;
   } else {
      if ((w > IMAGE_MAX_WIDTH) || (h > IMAGE_MAX_HEIGHT))
         return BadAlloc;
   }

   /* What to do when rotated ?? */
   if (pI830->rotation != RR_Rotate_0)
      return BadAlloc;

   w = (w + 1) & ~1;
   pitch = ((w << 1) + 15) & ~15;
   bpp = pScrn->bitsPerPixel >> 3;
   fbpitch = bpp * pScrn->displayWidth;
   size = ((pitch * h) + bpp - 1) / bpp;

   if (!(linear = I830AllocateMemory(pScrn, NULL, size)))
      return BadAlloc;

   surface->width = w;
   surface->height = h;

   if (!(surface->pitches = xalloc(sizeof(int)))) {
      xf86FreeOffscreenLinear(linear);
      return BadAlloc;
   }
   if (!(surface->offsets = xalloc(sizeof(int)))) {
      xfree(surface->pitches);
      xf86FreeOffscreenLinear(linear);
      return BadAlloc;
   }
   if (!(pPriv = xalloc(sizeof(OffscreenPrivRec)))) {
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
   surface->devPrivate.ptr = (pointer) pPriv;

   memset(pI830->FbBase + pI830->FrontBuffer.Start + surface->offsets[0], 0, size);

   return Success;
}

static int
I830StopSurface(XF86SurfacePtr surface)
{
   OffscreenPrivPtr pPriv = (OffscreenPrivPtr) surface->devPrivate.ptr;
   ScrnInfoPtr pScrn = surface->pScrn;

   if (pPriv->isOn) {
      I830Ptr pI830 = I830PTR(pScrn);

      I830OverlayRegPtr overlay =
	    (I830OverlayRegPtr) (pI830->FbBase + pI830->OverlayMem->Start);

      ErrorF("StopSurface\n");
      overlay->OCMD &= ~OVERLAY_ENABLE;
      OVERLAY_UPDATE;
#if 1
      OVERLAY_OFF;
#endif

      if (pI830->entityPrivate)
         pI830->entityPrivate->XvInUse = -1;

      pPriv->isOn = FALSE;
   }

   return Success;
}

static int
I830FreeSurface(XF86SurfacePtr surface)
{
   OffscreenPrivPtr pPriv = (OffscreenPrivPtr) surface->devPrivate.ptr;

   if (pPriv->isOn) {
      I830StopSurface(surface);
   }
   xf86FreeOffscreenLinear(pPriv->linear);
   xfree(surface->pitches);
   xfree(surface->offsets);
   xfree(surface->devPrivate.ptr);

   return Success;
}

static int
I830GetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attribute, INT32 * value)
{
   return I830GetPortAttribute(pScrn, attribute, value, 0);
}

static int
I830SetSurfaceAttribute(ScrnInfoPtr pScrn, Atom attribute, INT32 value)
{
   return I830SetPortAttribute(pScrn, attribute, value, 0);
}

static int
I830DisplaySurface(XF86SurfacePtr surface,
		   short src_x, short src_y,
		   short drw_x, short drw_y,
		   short src_w, short src_h,
		   short drw_w, short drw_h, RegionPtr clipBoxes)
{
   OffscreenPrivPtr pPriv = (OffscreenPrivPtr) surface->devPrivate.ptr;
   ScrnInfoPtr pScrn = surface->pScrn;
   ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];
   I830Ptr pI830 = I830PTR(pScrn);
   I830PortPrivPtr pI830Priv = GET_PORT_PRIVATE(pScrn);
   I830OverlayRegPtr overlay =
	 (I830OverlayRegPtr) (pI830->FbBase + pI830->OverlayMem->Start);
   INT32 x1, y1, x2, y2;
   INT32 loops = 0;
   BoxRec dstBox;

   ErrorF("I830DisplaySurface\n");

   if (pI830->entityPrivate) {
	 if (pI830->entityPrivate->XvInUse != -1 &&
	     pI830->entityPrivate->XvInUse != pI830Priv->pipe) {
#ifdef PANORAMIX
		if (!noPanoramiXExtension) {
			return Success; /* faked for trying to share it */
		} else
#endif
		{
			return BadAlloc;
		}
	 }

      pI830->entityPrivate->XvInUse = pI830Priv->pipe;
   }

   x1 = src_x;
   x2 = src_x + src_w;
   y1 = src_y;
   y2 = src_y + src_h;

   dstBox.x1 = drw_x;
   dstBox.x2 = drw_x + drw_w;
   dstBox.y1 = drw_y;
   dstBox.y2 = drw_y + drw_h;

   if (!xf86XVClipVideoHelper(&dstBox, &x1, &x2, &y1, &y2, clipBoxes,
			      surface->width, surface->height))
      return Success;

   /* fixup pointers */
   pI830Priv->YBuf0offset = surface->offsets[0];
   pI830Priv->YBuf1offset = pI830Priv->YBuf0offset;

   /* Make sure this buffer isn't in use */
   loops = 0;
   if (*pI830->overlayOn && pI830Priv->doubleBuffer && (overlay->OCMD & OVERLAY_ENABLE)) {
      while (loops < 1000000) {
#if USE_USLEEP_FOR_VIDEO
         usleep(10);
#endif
         if (((INREG(DOVSTA) & OC_BUF) >> 20) == pI830Priv->currentBuf) {
	    break;
         }
         loops++;
      }
      if (loops >= 1000000) {
         ErrorF("loops (1) maxed out for buffer %d\n", pI830Priv->currentBuf);
#if 0
         pI830Priv->currentBuf = !pI830Priv->currentBuf;
#endif
      }

      /* buffer swap */
      if (pI830Priv->currentBuf == 0)
         pI830Priv->currentBuf = 1;
      else
         pI830Priv->currentBuf = 0;
   }

   I830DisplayVideo(pScrn, surface->id, surface->width, surface->height,
		    surface->pitches[0], x1, y1, x2, y2, &dstBox,
		    src_w, src_h, drw_w, drw_h);

   xf86XVFillKeyHelper(pScreen, pI830Priv->colorKey, clipBoxes);

   pPriv->isOn = TRUE;
   /* we've prempted the XvImage stream so set its free timer */
   if (pI830Priv->videoStatus & CLIENT_VIDEO_ON) {
      REGION_EMPTY(pScrn->pScreen, &pI830Priv->clip);
      UpdateCurrentTime();
      pI830Priv->videoStatus = FREE_TIMER;
      pI830Priv->freeTime = currentTime.milliseconds + FREE_DELAY;
      pScrn->pScreen->BlockHandler = I830BlockHandler;
   }

   return Success;
}

static void
I830InitOffscreenImages(ScreenPtr pScreen)
{
   XF86OffscreenImagePtr offscreenImages;
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);

   /* need to free this someplace */
   if (!(offscreenImages = xalloc(sizeof(XF86OffscreenImageRec)))) {
      return;
   }

   offscreenImages[0].image = &Images[0];
   offscreenImages[0].flags = VIDEO_OVERLAID_IMAGES /*| VIDEO_CLIP_TO_VIEWPORT*/;
   offscreenImages[0].alloc_surface = I830AllocateSurface;
   offscreenImages[0].free_surface = I830FreeSurface;
   offscreenImages[0].display = I830DisplaySurface;
   offscreenImages[0].stop = I830StopSurface;
   offscreenImages[0].setAttribute = I830SetSurfaceAttribute;
   offscreenImages[0].getAttribute = I830GetSurfaceAttribute;
   if (IS_845G(pI830) || IS_I830(pI830)) {
      offscreenImages[0].max_width = IMAGE_MAX_WIDTH_LEGACY;
      offscreenImages[0].max_height = IMAGE_MAX_HEIGHT_LEGACY;
   } else {
      offscreenImages[0].max_width = IMAGE_MAX_WIDTH;
      offscreenImages[0].max_height = IMAGE_MAX_HEIGHT; 
   }
   offscreenImages[0].num_attributes = 1;
   offscreenImages[0].attributes = Attributes;

   xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
}

void
I830VideoSwitchModeBefore(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830PortPrivPtr pPriv;

   if (!pI830->adaptor) {
      return;
   }

   pPriv = GET_PORT_PRIVATE(pScrn);

   if (!pPriv) {
      xf86ErrorF("pPriv isn't set\n");
      return;
   }

   /* We stop the video when mode switching, just so we don't lockup
    * the engine. The overlayOK will determine whether we can re-enable
    * with the current video on completion of the mode switch.
    */
   I830StopVideo(pScrn, pPriv, TRUE);

   pPriv->overlayOK = FALSE;

   pPriv->oneLineMode = FALSE;
}

void
I830VideoSwitchModeAfter(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830PortPrivPtr pPriv;
   int size, hsize, vsize, active;

   if (!pI830->adaptor) {
      return;
   }
   pPriv = GET_PORT_PRIVATE(pScrn);
   if (!pPriv)
      return;

   pPriv->overlayOK = TRUE;

   /* ensure pipe is updated on mode switch */
   if (!pI830->Clone) {
      if (pPriv->pipe != pI830->pipe) {
         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	   "Changing XVideo pipe (%d to %d).\n", pPriv->pipe, pI830->pipe);
         pPriv->pipe = pI830->pipe;
      }
   }

   if (pPriv->pipe == 0) {
      if (INREG(PIPEACONF) & PIPEACONF_DOUBLE_WIDE) {
         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	   "Disabling XVideo output because Pipe A is in double-wide mode.\n");
         pPriv->overlayOK = FALSE;
      } else if (!pPriv->overlayOK) {
         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	   "Re-enabling XVideo output because Pipe A is now in single-wide mode.\n");
         pPriv->overlayOK = TRUE;
      }
   }

   if (pPriv->pipe == 1) {
      if (INREG(PIPEBCONF) & PIPEBCONF_DOUBLE_WIDE) {
         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	   "Disabling XVideo output because Pipe B is in double-wide mode.\n");
         pPriv->overlayOK = FALSE;
      } else if (!pPriv->overlayOK) {
         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	   "Re-enabling XVideo output because Pipe B is now in single-wide mode.\n");
         pPriv->overlayOK = TRUE;
      }
   }

   /* Check we have an LFP connected */
   if ((pPriv->pipe == 1 && pI830->operatingDevices & (PIPE_LFP << 8)) ||
       (pPriv->pipe == 0 && pI830->operatingDevices & PIPE_LFP) ) {
      size = pI830->pipe ? INREG(PIPEBSRC) : INREG(PIPEASRC);
      hsize = (size >> 16) & 0x7FF;
      vsize = size & 0x7FF;
      active = pI830->pipe ? (INREG(VTOTAL_B) & 0x7FF) : (INREG(VTOTAL_A) & 0x7FF);

      if (vsize < active && hsize > 1024)
         I830SetOneLineModeRatio(pScrn);
   
      if (pPriv->scaleRatio & 0xFFFE0000) {
         /* Possible bogus ratio, using in-accurate fallback */
         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	    "Bogus panel fit register, Xvideo positioning may not be accurate.\n");
         xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	    "Using fallback ratio - was 0x%x, now 0x%x\n", pPriv->scaleRatio, (int)(((float)active * 65536)/(float)vsize));
   
   
         pPriv->scaleRatio = (int)(((float)active * 65536) / (float)vsize);
      }
   }
}
