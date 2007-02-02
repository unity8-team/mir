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

#include "xf86.h"
#include "xaarop.h"
#include "i830.h"
#include "i810_reg.h"
#include "i830_reg.h"
#include <string.h>

#ifdef I830DEBUG
#define DEBUG_I830FALLBACK 1
#endif

#define ALWAYS_SYNC		1

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

/**
 * I830EXASync - wait for a command to finish
 * @pScreen: current screen
 * @marker: marker command to wait for
 *
 * Wait for the command specified by @marker to finish, then return.  We don't
 * actually do marker waits, though we might in the future.  For now, just
 * wait for a full idle.
 */
static void
I830EXASync(ScreenPtr pScreen, int marker)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];

    I830Sync(pScrn);
}

/**
 * I830EXAPrepareSolid - prepare for a Solid operation, if possible
 */
static Bool
I830EXAPrepareSolid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long offset, pitch;

    if (!EXA_PM_IS_SOLID(&pPixmap->drawable, planemask))
	I830FALLBACK("planemask is not solid");

    if (pPixmap->drawable.bitsPerPixel == 24)
	I830FALLBACK("solid 24bpp unsupported!\n");

    offset = exaGetPixmapOffset(pPixmap);
    pitch = exaGetPixmapPitch(pPixmap);

    if ( offset % pI830->EXADriverPtr->pixmapOffsetAlign != 0)
	I830FALLBACK("pixmap offset not aligned");
    if ( pitch % pI830->EXADriverPtr->pixmapPitchAlign != 0)
	I830FALLBACK("pixmap pitch not aligned");

    pI830->BR[13] = (pitch & 0xffff);
    switch (pPixmap->drawable.bitsPerPixel) {
	case 8:
	    break;
	case 16:
	    /* RGB565 */
	    pI830->BR[13] |= (1 << 24);
	    break;
	case 32:
	    /* RGB8888 */
	    pI830->BR[13] |= ((1 << 24) | (1 << 25));
	    break;
    }
    pI830->BR[13] |= (I830PatternROP[alu] & 0xff) << 16 ;
    pI830->BR[16] = fg;
    return TRUE;
}

static void
I830EXASolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long offset;

    offset = exaGetPixmapOffset(pPixmap);  
    
    {
	BEGIN_LP_RING(6);
	if (pPixmap->drawable.bitsPerPixel == 32)
	    OUT_RING(XY_COLOR_BLT_CMD | XY_COLOR_BLT_WRITE_ALPHA
			| XY_COLOR_BLT_WRITE_RGB);
	else
	    OUT_RING(XY_COLOR_BLT_CMD);

	OUT_RING(pI830->BR[13]);
	OUT_RING((y1 << 16) | (x1 & 0xffff));
	OUT_RING((y2 << 16) | (x2 & 0xffff));
	OUT_RING(offset);
	OUT_RING(pI830->BR[16]);
	ADVANCE_LP_RING();
    }
}

static void
I830EXADoneSolid(PixmapPtr pPixmap)
{
#if ALWAYS_SYNC
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];

    I830Sync(pScrn);
#endif
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

    switch (pSrcPixmap->drawable.bitsPerPixel) {
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
    unsigned int dst_off;

    dst_x2 = dst_x1 + w;
    dst_y2 = dst_y1 + h;

    dst_off = exaGetPixmapOffset(pDstPixmap);

    {
	BEGIN_LP_RING(8);

	if (pDstPixmap->drawable.bitsPerPixel == 32)
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
	OUT_RING(pI830->copy_src_off);

	ADVANCE_LP_RING();
    }
}

static void
I830EXADoneCopy(PixmapPtr pDstPixmap)
{
#if ALWAYS_SYNC
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];

    I830Sync(pScrn);
#endif
}

#define xFixedToFloat(val) \
	((float)xFixedToInt(val) + ((float)xFixedFrac(val) / 65536.0))

/**
 * Returns the floating-point coordinates transformed by the given transform.
 *
 * transform may be null.
 */
void
i830_get_transformed_coordinates(int x, int y, PictTransformPtr transform,
				 float *x_out, float *y_out)
{
    if (transform == NULL) {
	*x_out = x;
	*y_out = y;
    } else {
	PictVector v;

        v.vector[0] = IntToxFixed(x);
        v.vector[1] = IntToxFixed(y);
        v.vector[2] = xFixed1;
        PictureTransformPoint(transform, &v);
	*x_out = xFixedToFloat(v.vector[0]);
	*y_out = xFixedToFloat(v.vector[1]);
    }
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
    pI830->EXADriverPtr->exa_minor = 1;
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

    /* Limits are described in the BLT engine chapter under Graphics Data Size
     * Limitations, and the descriptions of SURFACE_STATE, 3DSTATE_BUFFER_INFO,
     * 3DSTATE_DRAWING_RECTANGLE, 3DSTATE_MAP_INFO, and 3DSTATE_MAP_INFO.
     *
     * i845 through i965 limits 2D rendering to 65536 lines and pitch of 32768.
     *
     * i965 limits 3D surface to (2*element size)-aligned offset if un-tiled.
     * i965 limits 3D surface to 4kB-aligned offset if tiled.
     * i965 limits 3D surfaces to w,h of ?,8192.
     * i965 limits 3D surface to pitch of 1B - 128kB.
     * i965 limits 3D surface pitch alignment to 512B, only if tiled.
     * i965 limits 3D destination drawing rect to w,h of 8192,8192.
     *
     * i915 limits 3D textures to 4B-aligned offset if un-tiled.
     * i915 limits 3D textures to ~4kB-aligned offset if tiled.
     * i915 limits 3D textures to width,height of 2048,2048.
     * i915 limits 3D textures to pitch of 16B - 8kB, in dwords.
     * i915 limits 3D destination to ~4kB-aligned offset if tiled.
     * i915 limits 3D destination to pitch of 16B - 8kB, in dwords, if un-tiled.
     * i915 limits 3D destination to pitch of 512B - 8kB, in tiles, if tiled.
     * i915 limits 3D destination to POT aligned pitch if tiled.
     * i915 limits 3D destination drawing rect to w,h of 2048,2048.
     *
     * i845 limits 3D textures to 4B-aligned offset if un-tiled.
     * i845 limits 3D textures to ~4kB-aligned offset if tiled.
     * i845 limits 3D textures to width,height of 2048,2048.
     * i845 limits 3D textures to pitch of 4B - 8kB, in dwords.
     * i845 limits 3D destination to 4B-aligned offset if un-tiled.
     * i845 limits 3D destination to ~4kB-aligned offset if tiled.
     * i845 limits 3D destination to pitch of 8B - 8kB, in dwords.
     * i845 limits 3D destination drawing rect to w,h of 2048,2048.
     *
     * For the tiled issues, the only tiled buffer we draw to should be
     * the front, which will have an appropriate pitch/offset already set up,
     * so EXA doesn't need to worry.
     */
    if (IS_I965G(pI830)) {
	pI830->EXADriverPtr->pixmapOffsetAlign = 4 * 2;
	pI830->EXADriverPtr->pixmapPitchAlign = 1;
	pI830->EXADriverPtr->maxX = 8192;
	pI830->EXADriverPtr->maxY = 8192;
    } else {
	pI830->EXADriverPtr->pixmapOffsetAlign = 4;
	pI830->EXADriverPtr->pixmapPitchAlign = 16;
	pI830->EXADriverPtr->maxX = 2048;
	pI830->EXADriverPtr->maxY = 2048;
    }

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
    if (IS_I865G(pI830) || IS_I855(pI830) ||
	       IS_845G(pI830) || IS_I830(pI830))
    {
    	pI830->EXADriverPtr->CheckComposite = i830_check_composite;
    	pI830->EXADriverPtr->PrepareComposite = i830_prepare_composite;
    	pI830->EXADriverPtr->Composite = i830_composite;
    	pI830->EXADriverPtr->DoneComposite = i830_done_composite;
    } else if (IS_I915G(pI830) || IS_I915GM(pI830) ||
	       IS_I945G(pI830) || IS_I945GM(pI830))
    {
	pI830->EXADriverPtr->CheckComposite = i915_check_composite;
   	pI830->EXADriverPtr->PrepareComposite = i915_prepare_composite;
    	pI830->EXADriverPtr->Composite = i830_composite;
    	pI830->EXADriverPtr->DoneComposite = i830_done_composite;
    } else {
 	pI830->EXADriverPtr->CheckComposite = i965_check_composite;
 	pI830->EXADriverPtr->PrepareComposite = i965_prepare_composite;
 	pI830->EXADriverPtr->Composite = i965_composite;
 	pI830->EXADriverPtr->DoneComposite = i830_done_composite;
    }

    if(!exaDriverInit(pScreen, pI830->EXADriverPtr)) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "EXA initialization failed; trying older version\n");
	pI830->EXADriverPtr->exa_minor = 0;
	if(!exaDriverInit(pScreen, pI830->EXADriverPtr)) {
	    xfree(pI830->EXADriverPtr);
	    pI830->noAccel = TRUE;
	    return FALSE;
	}
    }

    I830SelectBuffer(pScrn, I830_SELECT_FRONT);

    return TRUE;
}
