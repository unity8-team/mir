/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.
Copyright (c) 2005 Jesse Barnes <jbarnes@virtuousgeek.org>
  Based on code from i830_xaa.c.

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86_ansic.h"
#include "xf86.h"
#include "xaarop.h"
#include "i830.h"
#include "i810_reg.h"
#include "i830_reg.h"

#ifdef I830DEBUG
#define DEBUG_I830FALLBACK 1
#endif

#ifdef DEBUG_I830FALLBACK
#define I830FALLBACK(s, arg...)				\
do {							\
	DPRINTF(PFX, "EXA fallback: " s "\n", ##arg); 	\
	return FALSE;					\
} while(0)
#else
#define I830FALLBACK(s, arg...) 			\
do { 							\
	return FALSE;					\
} while(0) 
#endif

float scale_units[2][2];
int draw_coords[3][2];

const int I830CopyROP[16] =
{
   ROP_0,               /* GXclear */
   ROP_DSa,             /* GXand */
   ROP_SDna,            /* GXandReverse */
   ROP_S,               /* GXcopy */
   ROP_DSna,            /* GXandInverted */
   ROP_D,               /* GXnoop */
   ROP_DSx,             /* GXxor */
   ROP_DSo,             /* GXor */
   ROP_DSon,            /* GXnor */
   ROP_DSxn,            /* GXequiv */
   ROP_Dn,              /* GXinvert*/
   ROP_SDno,            /* GXorReverse */
   ROP_Sn,              /* GXcopyInverted */
   ROP_DSno,            /* GXorInverted */
   ROP_DSan,            /* GXnand */
   ROP_1                /* GXset */
};

const int I830PatternROP[16] =
{
    ROP_0,
    ROP_DPa,
    ROP_PDna,
    ROP_P,
    ROP_DPna,
    ROP_D,
    ROP_DPx,
    ROP_DPo,
    ROP_DPon,
    ROP_PDxn,
    ROP_Dn,
    ROP_PDno,
    ROP_Pn,
    ROP_DPno,
    ROP_DPan,
    ROP_1
};

/* move to common.h */
union intfloat {
	float f;
	unsigned int ui;
};

#define OUT_RING_F(x) do {			\
	union intfloat tmp;			\
	tmp.f = (float)(x);			\
	OUT_RING(tmp.ui);			\
} while(0)				

Bool is_transform[2];
PictTransform *transform[2];

Bool i830UploadToScreen(PixmapPtr pDst, int x, int y, int w, int h, 
			    char *src, int src_pitch);
Bool i830DownloadFromScreen(PixmapPtr pSrc, int x, int y, int w, int h,
			    char *dst, int dst_pitch);

extern Bool I830EXACheckComposite(int, PicturePtr, PicturePtr, PicturePtr);
extern Bool I830EXAPrepareComposite(int, PicturePtr, PicturePtr, PicturePtr, 
				PixmapPtr, PixmapPtr, PixmapPtr);
extern Bool I915EXACheckComposite(int, PicturePtr, PicturePtr, PicturePtr);
extern Bool I915EXAPrepareComposite(int, PicturePtr, PicturePtr, PicturePtr, 
				PixmapPtr, PixmapPtr, PixmapPtr);

/**
 * I830EXASync - wait for a command to finish
 * @pScreen: current screen
 * @marker: marker command to wait for
 *
 * Wait for the command specified by @marker to finish, then return.
 */
static void
I830EXASync(ScreenPtr pScreen, int marker)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

#ifdef XF86DRI
    /* VT switching tries to do this. */
    if (!pI830->LockHeld && pI830->directRenderingEnabled)
	return;
#endif

    if (pI830->entityPrivate && !pI830->entityPrivate->RingRunning)
	return;

    /* Send a flush instruction and then wait till the ring is empty.
     * This is stronger than waiting for the blitter to finish as it also
     * flushes the internal graphics caches.
     */
    {
	BEGIN_LP_RING(2);
	OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
	OUT_RING(MI_NOOP);		/* pad to quadword */
	ADVANCE_LP_RING();
    }

    I830WaitLpRing(pScrn, pI830->LpRing->mem.Size - 8, 0);

    pI830->LpRing->space = pI830->LpRing->mem.Size - 8;
    pI830->nextColorExpandBuf = 0;
}

/**
 * I830EXAPrepareSolid - prepare for a Solid operation, if possible
 *
 * TODO:
 *   - support planemask using FILL_MONO_SRC_BLT_CMD?
 */
static Bool
I830EXAPrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long offset, pitch;

    if (!EXA_PM_IS_SOLID(&pPixmap->drawable, planemask))
	I830FALLBACK("planemask is not solid");

    offset = exaGetPixmapOffset(pPixmap);
    pitch = exaGetPixmapPitch(pPixmap);

    if ( offset % pI830->EXADriverPtr->pixmapOffsetAlign != 0)
	I830FALLBACK("pixmap offset not aligned");
    if ( pitch % pI830->EXADriverPtr->pixmapPitchAlign != 0)
	I830FALLBACK("pixmap pitch not aligned");

    pI830->BR[13] = pitch;
    pI830->BR[13] |= I830PatternROP[alu] << 16;

    pI830->BR[16] = fg;

    /*
     * Depth: 00 - 8 bit, 01 - 16 bit, 10 - 24 bit, 11 - 32 bit
     */
    switch (pScrn->bitsPerPixel) {
    case 8:
	pI830->BR[13] |= ((0 << 25) | (0 << 24));
	break;
    case 16:
	pI830->BR[13] |= ((0 << 25) | (1 << 24));
	break;
    case 32:
	pI830->BR[13] |= ((1 << 25) | (1 << 24));
	break;
    }
    return TRUE;
}

static void
I830EXASolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    int h, w;
    unsigned long offset;

    /* pixmap's offset and pitch is aligned, 
       otherwise it falls back in PrepareSolid */
    offset = exaGetPixmapOffset(pPixmap) + y1 * exaGetPixmapPitch(pPixmap) +
	x1 * (pPixmap->drawable.bitsPerPixel / 8);
    
    h = y2 - y1;
    w = x2 - x1;

    {
	BEGIN_LP_RING(6);

	if (pScrn->bitsPerPixel == 32)
	    OUT_RING(COLOR_BLT_CMD | COLOR_BLT_WRITE_ALPHA |
		     COLOR_BLT_WRITE_RGB);
	else
	    OUT_RING(COLOR_BLT_CMD);

	OUT_RING(pI830->BR[13]);
	OUT_RING((h << 16) | (w * (pPixmap->drawable.bitsPerPixel/8)));
	OUT_RING(offset);
	OUT_RING(pI830->BR[16]);
	OUT_RING(0);

	ADVANCE_LP_RING();
    }
}

static void
I830EXADoneSolid(PixmapPtr pPixmap)
{
    return;
}

/**
 * TODO:
 *   - support planemask using FULL_BLT_CMD?
 */
static Bool
I830EXAPrepareCopy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int xdir,
		   int ydir, int alu, Pixel planemask)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    if (!EXA_PM_IS_SOLID(&pSrcPixmap->drawable, planemask))
	I830FALLBACK("planemask is not solid");

    pI830->copy_src_pitch = exaGetPixmapPitch(pSrcPixmap);
    pI830->copy_src_off = exaGetPixmapOffset(pSrcPixmap);

    pI830->BR[13] = exaGetPixmapPitch(pDstPixmap);
    pI830->BR[13] |= I830CopyROP[alu] << 16;

    switch (pScrn->bitsPerPixel) {
    case 8:
	break;
    case 16:
	pI830->BR[13] |= (1 << 24);
	break;
    case 32:
	pI830->BR[13] |= ((1 << 25) | (1 << 24));
	break;
    }
    return TRUE;
}

static void
I830EXACopy(PixmapPtr pDstPixmap, int src_x1, int src_y1, int dst_x1,
	    int dst_y1, int w, int h)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    int dst_x2, dst_y2;
    unsigned int src_off, dst_off;

    dst_x2 = dst_x1 + w;
    dst_y2 = dst_y1 + h;

    src_off = pI830->copy_src_off;
    dst_off = exaGetPixmapOffset(pDstPixmap);

    {
	BEGIN_LP_RING(8);

	if (pScrn->bitsPerPixel == 32)
	    OUT_RING(XY_SRC_COPY_BLT_CMD | XY_SRC_COPY_BLT_WRITE_ALPHA |
		     XY_SRC_COPY_BLT_WRITE_RGB);
	else
	    OUT_RING(XY_SRC_COPY_BLT_CMD);

	OUT_RING(pI830->BR[13]);
	OUT_RING((dst_y1 << 16) | (dst_x1 & 0xffff));
	OUT_RING((dst_y2 << 16) | (dst_x2 & 0xffff));
	OUT_RING(dst_off);
	OUT_RING((src_y1 << 16) | (src_x1 & 0xffff));
	OUT_RING(pI830->copy_src_pitch);
	OUT_RING(src_off);

	ADVANCE_LP_RING();
    }
}

static void
I830EXADoneCopy(PixmapPtr pDstPixmap)
{
    	return;
}

//#define UPLOAD_USE_BLIT 1

static Bool
I830EXAUploadToScreen(PixmapPtr pDst, int x, int y, int w, int h, 
		char *src, int src_pitch)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    int dst_pitch = exaGetPixmapPitch(pDst);
    int dst_offset = exaGetPixmapOffset(pDst);
    unsigned char *dst;

    I830Sync(pScrn);

#ifdef I830DEBUG
    ErrorF("Up->Screen: dst offset 0x%x, dst pitch %d, x %d, y %d, src %p, src pitch %d\n",
		dst_offset, dst_pitch, x, y, src, src_pitch);
#endif
#ifndef UPLOAD_USE_BLIT
    dst = pI830->FbBase + dst_offset + y*dst_pitch + 
		x* (pDst->drawable.bitsPerPixel/8);
    w *= pDst->drawable.bitsPerPixel/8;
    while(h--) {
	memcpy(dst, src, w);
	src += src_pitch;
	dst += dst_pitch;
    }
#else
    /* setup blit engine to copy one pixel data by one */
    {
	int x1, x2, y1, y2, i, j;
	CARD32 d, len, *srcp;
	x1 = x;
	y1 = y;
	x2 = x + w;
	y2 = y + h;

	len = (w * (pDst->drawable.bitsPerPixel/8)) >> 2;

	pI830->BR[13] = (1 << 24) | (1 << 25);
	pI830->BR[13] |= I830CopyROP[GXcopy]<<16;
	pI830->BR[13] |= dst_pitch & 0xffff;
	for (i = 0; i < h; i++) {
		srcp = (CARD32*)src;
		for ( j = len; j > 0; j--) {
			d = *srcp;
			BEGIN_LP_RING(6);
			OUT_RING(XY_COLOR_BLT_CMD | XY_COLOR_BLT_WRITE_ALPHA | 
				XY_COLOR_BLT_WRITE_RGB);
			OUT_RING(pI830->BR[13]);
			OUT_RING((y1 << 16) | x1);
			OUT_RING((y2 << 16) | x2);
			OUT_RING(dst_offset);
			OUT_RING(d);
			ADVANCE_LP_RING();
			srcp++;
		}
		src += src_pitch;
	}

    }
#endif

    return TRUE;
}

static Bool
I830EXADownloadFromScreen(PixmapPtr pSrc, int x, int y, int w, int h,
			  char *dst, int dst_pitch)
{
    ScrnInfoPtr pScrn = xf86Screens[pSrc->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    int src_pitch = exaGetPixmapPitch(pSrc);
    int src_offset = exaGetPixmapOffset(pSrc);
    unsigned char *src = pI830->FbBase + src_offset + y*src_pitch +
		x*(pSrc->drawable.bitsPerPixel/8);

    I830Sync(pScrn);

#ifdef I830DEBUG
    ErrorF("Screen->Mem: src offset 0x%x, src %p, src pitch %d, x %d, y %d, dst %p, dst_pitch %d\n",
	src_offset, src, src_pitch, x, y, dst, dst_pitch);
#endif
    w *= pSrc->drawable.bitsPerPixel/8;
    while(h--) {
	memcpy(dst, src, w);
	src += src_pitch;
	dst += dst_pitch;
    }

    return TRUE;
}

static void
IntelEXAComposite(PixmapPtr pDst, int srcX, int srcY, int maskX, int maskY,
		 int dstX, int dstY, int w, int h)
{
	/* should be same like I830Composite */
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    int srcXend, srcYend, maskXend, maskYend;
    PictVector v;
    int pMask = 1;

    DPRINTF(PFX, "Composite: srcX %d, srcY %d\n\t maskX %d, maskY %d\n\t"
	     "dstX %d, dstY %d\n\twidth %d, height %d\n\t"
	     "src_scale_x %f, src_scale_y %f, mask_scale_x %f, mask_scale_y %f\n""\tdx0 %d, dy0 %x, dx1 %d dy1 %x\n", 
		srcX, srcY, maskX, maskY, dstX, dstY, w, h,
		scale_units[0][0], scale_units[0][1], 
		scale_units[1][0], scale_units[1][1],
		draw_coords[0][0], draw_coords[0][1], 
		draw_coords[1][0], draw_coords[1][1]);

    if (scale_units[1][0] == -1 || scale_units[1][1] == -1) {
	ErrorF("mask is null\n");
	pMask = 0;
    }

    srcXend = srcX + w;
    srcYend = srcY + h;
    maskXend = maskX + w;
    maskYend = maskY + h;
    if (is_transform[0]) {
        v.vector[0] = IntToxFixed(srcX);
        v.vector[1] = IntToxFixed(srcY);
        v.vector[2] = xFixed1;
        PictureTransformPoint(transform[0], &v);
        srcX = xFixedToInt(v.vector[0]);
        srcY = xFixedToInt(v.vector[1]);
        v.vector[0] = IntToxFixed(srcXend);
        v.vector[1] = IntToxFixed(srcYend);
        v.vector[2] = xFixed1;
        PictureTransformPoint(transform[0], &v);
        srcXend = xFixedToInt(v.vector[0]);
        srcYend = xFixedToInt(v.vector[1]);
    }
    if (is_transform[1]) {
        v.vector[0] = IntToxFixed(maskX);
        v.vector[1] = IntToxFixed(maskY);
        v.vector[2] = xFixed1;
        PictureTransformPoint(transform[1], &v);
        maskX = xFixedToInt(v.vector[0]);
        maskY = xFixedToInt(v.vector[1]);
        v.vector[0] = IntToxFixed(maskXend);
        v.vector[1] = IntToxFixed(maskYend);
        v.vector[2] = xFixed1;
        PictureTransformPoint(transform[1], &v);
        maskXend = xFixedToInt(v.vector[0]);
        maskYend = xFixedToInt(v.vector[1]);
    }
    DPRINTF(PFX, "After transform: srcX %d, srcY %d,srcXend %d, srcYend %d\n\t"
		"maskX %d, maskY %d, maskXend %d, maskYend %d\n\t"
		"dstX %d, dstY %d\n", srcX, srcY, srcXend, srcYend,
		maskX, maskY, maskXend, maskYend, dstX, dstY);

    draw_coords[0][0] -= draw_coords[2][0];
    draw_coords[0][1] -= draw_coords[2][1];
    if (pMask) {
	draw_coords[1][0] -= draw_coords[2][0];
	draw_coords[1][1] -= draw_coords[2][1];
    }

    {
	int vertex_count; 

	if (pMask)
		vertex_count = 4*6;
	else
		vertex_count = 4*4;

	BEGIN_LP_RING(6+vertex_count);

	OUT_RING(MI_NOOP);
	OUT_RING(MI_NOOP);
	OUT_RING(MI_NOOP);
	OUT_RING(MI_NOOP);
	OUT_RING(MI_NOOP);

	OUT_RING(PRIM3D_INLINE | PRIM3D_TRIFAN | (vertex_count-1));

	OUT_RING_F(dstX);
	OUT_RING_F(dstY);
	OUT_RING_F(((srcX - draw_coords[0][0]) / scale_units[0][0]));
	OUT_RING_F(((srcY - draw_coords[0][1]) / scale_units[0][1]));
	if (pMask) {
		OUT_RING_F(((maskX - draw_coords[1][0]) / scale_units[1][0]));
		OUT_RING_F(((maskY - draw_coords[1][1]) / scale_units[1][1]));
	}

	OUT_RING_F(dstX);
	OUT_RING_F((dstY+h));
	OUT_RING_F(((srcX - draw_coords[0][0]) / scale_units[0][0]));
	OUT_RING_F(((srcYend - draw_coords[0][1]) / scale_units[0][1]));
	if (pMask) {
		OUT_RING_F(((maskX - draw_coords[1][0]) / scale_units[1][0]));
		OUT_RING_F(((maskYend - draw_coords[1][1]) / scale_units[1][1]));
	}

	OUT_RING_F((dstX+w));
	OUT_RING_F((dstY+h));
	OUT_RING_F(((srcXend - draw_coords[0][0]) / scale_units[0][0]));
	OUT_RING_F(((srcYend - draw_coords[0][1]) / scale_units[0][1]));
	if (pMask) {
		OUT_RING_F(((maskXend - draw_coords[1][0]) / scale_units[1][0]));
		OUT_RING_F(((maskYend - draw_coords[1][1]) / scale_units[1][1]));
	}

	OUT_RING_F((dstX+w));
	OUT_RING_F((dstY));
	OUT_RING_F(((srcXend - draw_coords[0][0]) / scale_units[0][0]));
	OUT_RING_F(((srcY - draw_coords[0][1]) / scale_units[0][1]));
	if (pMask) {
		OUT_RING_F(((maskXend - draw_coords[1][0]) / scale_units[1][0]));
		OUT_RING_F(((maskY - draw_coords[1][1]) / scale_units[1][1]));
	}
	ADVANCE_LP_RING();
    }
#ifdef I830DEBUG
    ErrorF("sync after 3dprimitive");
    I830Sync(pScrn);
#endif

}

static void
IntelEXADoneComposite(PixmapPtr pDst)
{
    return; 
}
/*
 * TODO:
 *   - Dual head?
 */
Bool
I830EXAInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->EXADriverPtr = exaDriverAlloc();
    if (pI830->EXADriverPtr == NULL) {
	pI830->noAccel = TRUE;
	return FALSE;
    }
    memset(pI830->EXADriverPtr, 0, sizeof(*pI830->EXADriverPtr));
    
    pI830->bufferOffset = 0;
    pI830->EXADriverPtr->exa_major = 2;
    pI830->EXADriverPtr->exa_minor = 0;
    pI830->EXADriverPtr->memoryBase = pI830->FbBase;
    pI830->EXADriverPtr->offScreenBase = pI830->Offscreen.Start;
    pI830->EXADriverPtr->memorySize = pI830->Offscreen.End;
	   
    DPRINTF(PFX, "EXA Mem: memoryBase 0x%x, end 0x%x, offscreen base 0x%x, memorySize 0x%x\n",
		pI830->EXADriverPtr->memoryBase,
		pI830->EXADriverPtr->memoryBase + pI830->EXADriverPtr->memorySize,
		pI830->EXADriverPtr->offScreenBase,
		pI830->EXADriverPtr->memorySize);

    if(pI830->EXADriverPtr->memorySize >
       pI830->EXADriverPtr->offScreenBase)
	pI830->EXADriverPtr->flags = EXA_OFFSCREEN_PIXMAPS;
    else {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Not enough video RAM for "
		   "offscreen memory manager. Xv disabled\n");
	/* disable Xv here... */
    }

    pI830->EXADriverPtr->pixmapOffsetAlign = 256;
    pI830->EXADriverPtr->pixmapPitchAlign = 64;
    pI830->EXADriverPtr->maxX = 4095;
    pI830->EXADriverPtr->maxY = 4095;

    /* Sync */
    pI830->EXADriverPtr->WaitMarker = I830EXASync;

    /* Solid fill */
    pI830->EXADriverPtr->PrepareSolid = I830EXAPrepareSolid;
    pI830->EXADriverPtr->Solid = I830EXASolid;
    pI830->EXADriverPtr->DoneSolid = I830EXADoneSolid;

    /* Copy */
    pI830->EXADriverPtr->PrepareCopy = I830EXAPrepareCopy;
    pI830->EXADriverPtr->Copy = I830EXACopy;
    pI830->EXADriverPtr->DoneCopy = I830EXADoneCopy;

    /* Composite */
    if (IS_I9XX(pI830)) {   		
	pI830->EXADriverPtr->CheckComposite = I915EXACheckComposite;
   	pI830->EXADriverPtr->PrepareComposite = I915EXAPrepareComposite;
    	pI830->EXADriverPtr->Composite = IntelEXAComposite;
    	pI830->EXADriverPtr->DoneComposite = IntelEXADoneComposite;
    } else if (IS_I865G(pI830) || IS_I855(pI830) || IS_845G(pI830) || IS_I830(pI830)) { 
    	pI830->EXADriverPtr->CheckComposite = I830EXACheckComposite;
    	pI830->EXADriverPtr->PrepareComposite = I830EXAPrepareComposite;
    	pI830->EXADriverPtr->Composite = IntelEXAComposite;
    	pI830->EXADriverPtr->DoneComposite = IntelEXADoneComposite;
    }

    /* Upload, download to/from Screen, experimental!! */
    pI830->EXADriverPtr->UploadToScreen = I830EXAUploadToScreen;
    pI830->EXADriverPtr->DownloadFromScreen = I830EXADownloadFromScreen;

    if(!exaDriverInit(pScreen, pI830->EXADriverPtr)) {
	xfree(pI830->EXADriverPtr);
	pI830->noAccel = TRUE;
	return FALSE;
    }

    I830SelectBuffer(pScrn, I830_SELECT_FRONT);

    return TRUE;
}
