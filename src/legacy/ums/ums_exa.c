/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.
Copyright (c) 2005 Jesse Barnes <jbarnes@virtuousgeek.org>
  Based on code from ums_xaa.c.

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

#include <string.h>

#include "xf86.h"
#include "xaarop.h"
#include "i915_drm.h"

#include "ums.h"
#include "ums_i810_reg.h"

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
 * Returns whether a given pixmap is tiled or not.
 *
 * Currently, we only have one pixmap that might be tiled, which is the front
 * buffer.  At the point where we are tiling some pixmaps managed by the
 * general allocator, we should move this to using pixmap privates.
 */
Bool
ums_pixmap_tiled(PixmapPtr pPixmap)
{
    ScreenPtr pScreen = pPixmap->drawable.pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long offset;

    offset = ums_get_pixmap_offset(pPixmap);
    if (offset == pI830->front_buffer->offset &&
	pI830->front_buffer->tiling != TILE_NONE)
    {
	return TRUE;
    }

    return FALSE;
}

static int
ums_pixmap_pitch_is_aligned(PixmapPtr pixmap)
{
    ScrnInfoPtr pScrn = xf86Screens[pixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    return ums_get_pixmap_pitch(pixmap) % pI830->accel_pixmap_pitch_alignment == 0;
}

static Bool
ums_exa_pixmap_is_offscreen(PixmapPtr pPixmap)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    if ((void *)pPixmap->devPrivate.ptr >= (void *)pI830->FbBase &&
	(void *)pPixmap->devPrivate.ptr <
	(void *)(pI830->FbBase + pI830->FbMapSize))
    {
	return TRUE;
    } else {
	return FALSE;
    }
}

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

    ums_I830Sync(pScrn);
}

/**
 * Sets up hardware state for a series of solid fills.
 */
static Bool
ums_exa_prepare_solid(PixmapPtr pPixmap, int alu, Pixel planemask, Pixel fg)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long pitch;

    if (!EXA_PM_IS_SOLID(&pPixmap->drawable, planemask))
	I830FALLBACK("planemask is not solid");

    if (pPixmap->drawable.bitsPerPixel == 24)
	I830FALLBACK("solid 24bpp unsupported!\n");

    if (pPixmap->drawable.bitsPerPixel < 8)
	I830FALLBACK("under 8bpp pixmaps unsupported\n");

    ums_exa_check_pitch_2d(pPixmap);

    pitch = ums_get_pixmap_pitch(pPixmap);

    if (!ums_pixmap_pitch_is_aligned(pPixmap))
	I830FALLBACK("pixmap pitch not aligned");

    pI830->BR[13] = (I830PatternROP[alu] & 0xff) << 16 ;
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
    pI830->BR[16] = fg;
    return TRUE;
}

static void
ums_exa_solid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long pitch;
    uint32_t cmd;

    if (x1 < 0)
	    x1 = 0;
    if (y1 < 0)
	    y1 = 0;
    if (x2 > pPixmap->drawable.width)
	    x2 = pPixmap->drawable.width;
    if (y2 > pPixmap->drawable.height)
	    y2 = pPixmap->drawable.height;

    if (x2 <= x1 || y2 <= y1)
	    return;

    pitch = ums_get_pixmap_pitch(pPixmap);

    {
	BEGIN_BATCH(6);

	cmd = XY_COLOR_BLT_CMD;

	if (pPixmap->drawable.bitsPerPixel == 32)
	    cmd |= XY_COLOR_BLT_WRITE_ALPHA | XY_COLOR_BLT_WRITE_RGB;

	if (IS_I965G(pI830) && ums_pixmap_tiled(pPixmap)) {
	    assert((pitch % 512) == 0);
	    pitch >>= 2;
	    cmd |= XY_COLOR_BLT_TILED;
	}

	OUT_BATCH(cmd);

	OUT_BATCH(pI830->BR[13] | pitch);
	OUT_BATCH((y1 << 16) | (x1 & 0xffff));
	OUT_BATCH((y2 << 16) | (x2 & 0xffff));
	OUT_RELOC_PIXMAP(pPixmap, I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER, 0);
	OUT_BATCH(pI830->BR[16]);
	ADVANCE_BATCH();
    }
}

static void
ums_exa_done_solid(PixmapPtr pPixmap)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];

    ums_debug_sync(pScrn);
}

/**
 * TODO:
 *   - support planemask using FULL_BLT_CMD?
 */
static Bool
ums_exa_prepare_copy(PixmapPtr pSrcPixmap, PixmapPtr pDstPixmap, int xdir,
		      int ydir, int alu, Pixel planemask)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    if (!EXA_PM_IS_SOLID(&pSrcPixmap->drawable, planemask))
	I830FALLBACK("planemask is not solid");

    if (pDstPixmap->drawable.bitsPerPixel < 8)
	I830FALLBACK("under 8bpp pixmaps unsupported\n");

    ums_exa_check_pitch_2d(pSrcPixmap);
    ums_exa_check_pitch_2d(pDstPixmap);

    pI830->pSrcPixmap = pSrcPixmap;

    pI830->BR[13] = I830CopyROP[alu] << 16;

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
ums_exa_copy(PixmapPtr pDstPixmap, int src_x1, int src_y1, int dst_x1,
	      int dst_y1, int w, int h)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    uint32_t cmd;
    int dst_x2, dst_y2;
    unsigned int dst_pitch, src_pitch;

    dst_x2 = dst_x1 + w;
    dst_y2 = dst_y1 + h;

    dst_pitch = ums_get_pixmap_pitch(pDstPixmap);
    src_pitch = ums_get_pixmap_pitch(pI830->pSrcPixmap);

    {
	BEGIN_BATCH(8);

	cmd = XY_SRC_COPY_BLT_CMD;

	if (pDstPixmap->drawable.bitsPerPixel == 32)
	    cmd |= XY_SRC_COPY_BLT_WRITE_ALPHA | XY_SRC_COPY_BLT_WRITE_RGB;

	if (IS_I965G(pI830)) {
	    if (ums_pixmap_tiled(pDstPixmap)) {
		assert((dst_pitch % 512) == 0);
		dst_pitch >>= 2;
		cmd |= XY_SRC_COPY_BLT_DST_TILED;
	    }

	    if (ums_pixmap_tiled(pI830->pSrcPixmap)) {
		assert((src_pitch % 512) == 0);
		src_pitch >>= 2;
		cmd |= XY_SRC_COPY_BLT_SRC_TILED;
	    }
	}

	OUT_BATCH(cmd);

	OUT_BATCH(pI830->BR[13] | dst_pitch);
	OUT_BATCH((dst_y1 << 16) | (dst_x1 & 0xffff));
	OUT_BATCH((dst_y2 << 16) | (dst_x2 & 0xffff));
	OUT_RELOC_PIXMAP(pDstPixmap, I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER, 0);
	OUT_BATCH((src_y1 << 16) | (src_x1 & 0xffff));
	OUT_BATCH(src_pitch);
	OUT_RELOC_PIXMAP(pI830->pSrcPixmap, I915_GEM_DOMAIN_RENDER, 0, 0);

	ADVANCE_BATCH();
    }
}

static void
ums_exa_done_copy(PixmapPtr pDstPixmap)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];

    ums_debug_sync(pScrn);
}



void
ums_done_composite(PixmapPtr pDst)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];

    ums_debug_sync(pScrn);
}

Bool
ums_I830EXAInit(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->EXADriverPtr = exaDriverAlloc();
    if (pI830->EXADriverPtr == NULL)
	return FALSE;

    memset(pI830->EXADriverPtr, 0, sizeof(*pI830->EXADriverPtr));

    pI830->bufferOffset = 0;
    pI830->EXADriverPtr->exa_major = 2;
    /* If compiled against EXA 2.2, require 2.2 so we can use the
     * PixmapIsOffscreen hook.
     */
#if EXA_VERSION_MINOR >= 2
    pI830->EXADriverPtr->exa_minor = 2;
#else
    pI830->EXADriverPtr->exa_minor = 1;
    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	       "EXA compatibility mode.  Output rotation rendering "
	       "performance may suffer\n");
#endif
    pI830->EXADriverPtr->memoryBase = pI830->FbBase;
    if (pI830->exa_offscreen) {
	pI830->EXADriverPtr->offScreenBase = pI830->exa_offscreen->offset;
	pI830->EXADriverPtr->memorySize = pI830->exa_offscreen->offset +
	    pI830->exa_offscreen->size;
    } else {
	pI830->EXADriverPtr->offScreenBase = pI830->FbMapSize;
	pI830->EXADriverPtr->memorySize = pI830->FbMapSize;
    }
    pI830->EXADriverPtr->flags = EXA_OFFSCREEN_PIXMAPS;

    DPRINTF(PFX, "EXA Mem: memoryBase 0x%x, end 0x%x, offscreen base 0x%x, "
	    "memorySize 0x%x\n",
	    pI830->EXADriverPtr->memoryBase,
	    pI830->EXADriverPtr->memoryBase + pI830->EXADriverPtr->memorySize,
	    pI830->EXADriverPtr->offScreenBase,
	    pI830->EXADriverPtr->memorySize);

    pI830->EXADriverPtr->pixmapOffsetAlign = pI830->accel_pixmap_offset_alignment;
    pI830->EXADriverPtr->pixmapPitchAlign = pI830->accel_pixmap_pitch_alignment;
    pI830->EXADriverPtr->maxX = pI830->accel_max_x;
    pI830->EXADriverPtr->maxY = pI830->accel_max_y;

    /* Sync */
    pI830->EXADriverPtr->WaitMarker = I830EXASync;

    /* Solid fill */
    pI830->EXADriverPtr->PrepareSolid = ums_exa_prepare_solid;
    pI830->EXADriverPtr->Solid = ums_exa_solid;
    pI830->EXADriverPtr->DoneSolid = ums_exa_done_solid;

    /* Copy */
    pI830->EXADriverPtr->PrepareCopy = ums_exa_prepare_copy;
    pI830->EXADriverPtr->Copy = ums_exa_copy;
    pI830->EXADriverPtr->DoneCopy = ums_exa_done_copy;

    /* Composite */
    if (!IS_I9XX(pI830)) {
    	pI830->EXADriverPtr->CheckComposite = ums_i830_check_composite;
    	pI830->EXADriverPtr->PrepareComposite = ums_i830_prepare_composite;
    	pI830->EXADriverPtr->Composite = ums_i830_composite;
    	pI830->EXADriverPtr->DoneComposite = ums_done_composite;
    } else if (IS_I915G(pI830) || IS_I915GM(pI830) ||
	       IS_I945G(pI830) || IS_I945GM(pI830) || IS_G33CLASS(pI830))
    {
	pI830->EXADriverPtr->CheckComposite = ums_i915_check_composite;
   	pI830->EXADriverPtr->PrepareComposite = ums_i915_prepare_composite;
	pI830->EXADriverPtr->Composite = ums_i915_composite;
    	pI830->EXADriverPtr->DoneComposite = ums_done_composite;
    } else {
 	pI830->EXADriverPtr->CheckComposite = ums_i965_check_composite;
 	pI830->EXADriverPtr->PrepareComposite = ums_i965_prepare_composite;
 	pI830->EXADriverPtr->Composite = ums_i965_composite;
 	pI830->EXADriverPtr->DoneComposite = ums_done_composite;
    }
#if EXA_VERSION_MINOR >= 2
    pI830->EXADriverPtr->PixmapIsOffscreen = ums_exa_pixmap_is_offscreen;
#endif

    if(!exaDriverInit(pScreen, pI830->EXADriverPtr)) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "EXA initialization failed; trying older version\n");
	pI830->EXADriverPtr->exa_minor = 0;
	if(!exaDriverInit(pScreen, pI830->EXADriverPtr)) {
	    free(pI830->EXADriverPtr);
	    return FALSE;
	}
    }

    ums_I830SelectBuffer(pScrn, I830_SELECT_FRONT);

    return TRUE;
}

#ifdef BUILD_DRI

#ifndef ExaOffscreenMarkUsed
extern void ExaOffscreenMarkUsed(PixmapPtr);
#endif

unsigned long long
ums_I830TexOffsetStart(PixmapPtr pPix)
{
    exaMoveInPixmap(pPix);
    ExaOffscreenMarkUsed(pPix);

    return exaGetPixmapOffset(pPix);
}
#endif
