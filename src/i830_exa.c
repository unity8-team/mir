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
#include <string.h>

#ifdef I830DEBUG
#define DEBUG_I830FALLBACK 1
#endif

#define ALWAYS_SYNC		0

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
 * Returns whether a given pixmap is tiled or not.
 *
 * Currently, we only have one pixmap that might be tiled, which is the front
 * buffer.  At the point where we are tiling some pixmaps managed by the
 * general allocator, we should move this to using pixmap privates.
 */
Bool
i830_pixmap_tiled(PixmapPtr pPixmap)
{
    ScreenPtr pScreen = pPixmap->drawable.pScreen;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long offset;

    offset = intel_get_pixmap_offset(pPixmap);
    if (offset == pI830->front_buffer->offset &&
	pI830->front_buffer->tiling != TILE_NONE)
    {
	return TRUE;
    }

    return FALSE;
}

static unsigned long
i830_pixmap_pitch(PixmapPtr pixmap)
{
    return pixmap->devKind;
}

static int
i830_pixmap_pitch_is_aligned(PixmapPtr pixmap)
{
    ScrnInfoPtr pScrn = xf86Screens[pixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);

    return i830_pixmap_pitch(pixmap) % pI830->accel_pixmap_pitch_alignment == 0;
}

static Bool
i830_exa_pixmap_is_offscreen(PixmapPtr pPixmap)
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
    unsigned long pitch;

    I830FALLBACK("solid");
    if (!EXA_PM_IS_SOLID(&pPixmap->drawable, planemask))
	I830FALLBACK("planemask is not solid");

    if (pPixmap->drawable.bitsPerPixel == 24)
	I830FALLBACK("solid 24bpp unsupported!\n");

    i830_exa_check_pitch_2d(pPixmap);

    pitch = i830_pixmap_pitch(pPixmap);

    if (!i830_pixmap_pitch_is_aligned(pPixmap))
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
I830EXASolid(PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned long pitch;
    uint32_t cmd;

    pitch = i830_pixmap_pitch(pPixmap);

    {
	BEGIN_BATCH(6);

	cmd = XY_COLOR_BLT_CMD;

	if (pPixmap->drawable.bitsPerPixel == 32)
	    cmd |= XY_COLOR_BLT_WRITE_ALPHA | XY_COLOR_BLT_WRITE_RGB;

	if (IS_I965G(pI830) && i830_pixmap_tiled(pPixmap)) {
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

    I830FALLBACK("copy");
    if (!EXA_PM_IS_SOLID(&pSrcPixmap->drawable, planemask))
	I830FALLBACK("planemask is not solid");

    i830_exa_check_pitch_2d(pSrcPixmap);
    i830_exa_check_pitch_2d(pDstPixmap);

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
I830EXACopy(PixmapPtr pDstPixmap, int src_x1, int src_y1, int dst_x1,
	    int dst_y1, int w, int h)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    uint32_t cmd;
    int dst_x2, dst_y2;
    unsigned int dst_pitch, src_pitch;

    dst_x2 = dst_x1 + w;
    dst_y2 = dst_y1 + h;

    dst_pitch = i830_pixmap_pitch(pDstPixmap);
    src_pitch = i830_pixmap_pitch(pI830->pSrcPixmap);

    {
	BEGIN_BATCH(8);

	cmd = XY_SRC_COPY_BLT_CMD;

	if (pDstPixmap->drawable.bitsPerPixel == 32)
	    cmd |= XY_SRC_COPY_BLT_WRITE_ALPHA | XY_SRC_COPY_BLT_WRITE_RGB;

	if (IS_I965G(pI830)) {
	    if (i830_pixmap_tiled(pDstPixmap)) {
		assert((dst_pitch % 512) == 0);
		dst_pitch >>= 2;
		cmd |= XY_SRC_COPY_BLT_DST_TILED;
	    }

	    if (i830_pixmap_tiled(pI830->pSrcPixmap)) {
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
I830EXADoneCopy(PixmapPtr pDstPixmap)
{
#if ALWAYS_SYNC
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];

    I830Sync(pScrn);
#endif
}

#define xFixedToFloat(val) \
	((float)xFixedToInt(val) + ((float)xFixedFrac(val) / 65536.0))

static Bool
_i830_transform_point (PictTransformPtr transform,
		       float		x,
		       float		y,
		       float		result[3])
{
    int		    j;

    for (j = 0; j < 3; j++)
    {
	result[j] = (xFixedToFloat (transform->matrix[j][0]) * x +
		     xFixedToFloat (transform->matrix[j][1]) * y +
		     xFixedToFloat (transform->matrix[j][2]));
    }
    if (!result[2])
	return FALSE;
    return TRUE;
}

/**
 * Returns the floating-point coordinates transformed by the given transform.
 *
 * transform may be null.
 */
Bool
i830_get_transformed_coordinates(int x, int y, PictTransformPtr transform,
				 float *x_out, float *y_out)
{
    if (transform == NULL) {
	*x_out = x;
	*y_out = y;
    } else {
	float	result[3];

	if (!_i830_transform_point (transform, (float) x, (float) y, result))
	    return FALSE;
	*x_out = result[0] / result[2];
	*y_out = result[1] / result[2];
    }
    return TRUE;
}

/**
 * Returns the un-normalized floating-point coordinates transformed by the given transform.
 *
 * transform may be null.
 */
Bool
i830_get_transformed_coordinates_3d(int x, int y, PictTransformPtr transform,
				    float *x_out, float *y_out, float *w_out)
{
    if (transform == NULL) {
	*x_out = x;
	*y_out = y;
	*w_out = 1;
    } else {
	float    result[3];

	if (!_i830_transform_point (transform, (float) x, (float) y, result))
	    return FALSE;
	*x_out = result[0];
	*y_out = result[1];
	*w_out = result[2];
    }
    return TRUE;
}

/**
 * Returns whether the provided transform is affine.
 *
 * transform may be null.
 */
Bool
i830_transform_is_affine (PictTransformPtr t)
{
    if (t == NULL)
	return TRUE;
    return t->matrix[2][0] == 0 && t->matrix[2][1] == 0;
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
	pI830->accel = ACCEL_NONE;
	return FALSE;
    }
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
    pI830->EXADriverPtr->PrepareSolid = I830EXAPrepareSolid;
    pI830->EXADriverPtr->Solid = I830EXASolid;
    pI830->EXADriverPtr->DoneSolid = I830EXADoneSolid;

    /* Copy */
    pI830->EXADriverPtr->PrepareCopy = I830EXAPrepareCopy;
    pI830->EXADriverPtr->Copy = I830EXACopy;
    pI830->EXADriverPtr->DoneCopy = I830EXADoneCopy;

    /* Composite */
    if (!IS_I9XX(pI830)) {
    	pI830->EXADriverPtr->CheckComposite = i830_check_composite;
    	pI830->EXADriverPtr->PrepareComposite = i830_prepare_composite;
    	pI830->EXADriverPtr->Composite = i830_composite;
    	pI830->EXADriverPtr->DoneComposite = i830_done_composite;
    } else if (IS_I915G(pI830) || IS_I915GM(pI830) ||
	       IS_I945G(pI830) || IS_I945GM(pI830) || IS_G33CLASS(pI830))
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
#if EXA_VERSION_MINOR >= 2
    pI830->EXADriverPtr->PixmapIsOffscreen = i830_exa_pixmap_is_offscreen;
#endif

    if(!exaDriverInit(pScreen, pI830->EXADriverPtr)) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "EXA initialization failed; trying older version\n");
	pI830->EXADriverPtr->exa_minor = 0;
	if(!exaDriverInit(pScreen, pI830->EXADriverPtr)) {
	    xfree(pI830->EXADriverPtr);
	    pI830->accel = ACCEL_NONE;
	    return FALSE;
	}
    }

    I830SelectBuffer(pScrn, I830_SELECT_FRONT);

    return TRUE;
}

static DevPrivateKey	uxa_pixmap_key = &uxa_pixmap_key;

static void
i830_uxa_set_pixmap_bo (PixmapPtr pixmap, dri_bo *bo)
{
    dixSetPrivate(&pixmap->devPrivates, uxa_pixmap_key, bo);
}

dri_bo *
i830_uxa_get_pixmap_bo (PixmapPtr pixmap)
{
    return dixLookupPrivate(&pixmap->devPrivates, uxa_pixmap_key);
}

static Bool
i830_uxa_prepare_access (PixmapPtr pixmap, int index)
{
    dri_bo *bo = i830_uxa_get_pixmap_bo (pixmap);

    if (bo) {
	if (dri_bo_map (bo, index == UXA_PREPARE_DEST) != 0)
	    return FALSE;
	assert (pixmap->devPrivate.ptr == bo->virtual);
	pixmap->devPrivate.ptr = bo->virtual;
    }
    return TRUE;
}

void
i830_uxa_block_handler (ScreenPtr screen)
{
    ScrnInfoPtr scrn = xf86Screens[screen->myNum];
    I830Ptr i830 = I830PTR(scrn);

    if (i830->need_flush) {
	dri_bo_wait_rendering (i830->front_buffer->bo);
	i830->need_flush = FALSE;
    }
}

static void
i830_uxa_finish_access (PixmapPtr pixmap, int index)
{
    dri_bo *bo = i830_uxa_get_pixmap_bo (pixmap);

    if (bo) {
	ScreenPtr screen = pixmap->drawable.pScreen;
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	I830Ptr i830 = I830PTR(scrn);
	
	dri_bo_unmap (bo);
	if (bo == i830->front_buffer->bo)
	    i830->need_flush = TRUE;
    }
}

static Bool
i830_uxa_pixmap_is_offscreen(PixmapPtr pPixmap)
{
    return i830_uxa_get_pixmap_bo (pPixmap) != NULL;
}

static PixmapPtr
i830_uxa_create_pixmap (ScreenPtr screen, int w, int h, int depth, unsigned usage)
{
    ScrnInfoPtr scrn = xf86Screens[screen->myNum];
    I830Ptr i830 = I830PTR(scrn);
    dri_bo *bo;
    int stride;
    PixmapPtr pixmap;
    
    if (w > 32767 || h > 32767)
	return NullPixmap;

    pixmap = fbCreatePixmap (screen, 0, 0, depth, usage);
    
    if (w && h)
    {
	stride = ROUND_TO((w * pixmap->drawable.bitsPerPixel + 7) / 8,
			  i830->accel_pixmap_pitch_alignment);
    
	bo = dri_bo_alloc (i830->bufmgr, "pixmap", stride * h, 
			   i830->accel_pixmap_offset_alignment);
	if (!bo) {
	    fbDestroyPixmap (pixmap);
	    return NullPixmap;
	}
	
	if (dri_bo_map (bo, FALSE) != 0) {
	    fbDestroyPixmap (pixmap);
	    dri_bo_unreference (bo);
	    return NullPixmap;
	}
	    
	screen->ModifyPixmapHeader (pixmap, w, h, 0, 0, stride,
				    (pointer) bo->virtual);
    
	dri_bo_unmap (bo);
	i830_uxa_set_pixmap_bo (pixmap, bo);
    }

    return pixmap;
}

static Bool
i830_uxa_destroy_pixmap (PixmapPtr pixmap)
{
    if (pixmap->refcnt == 1) {
	dri_bo  *bo = i830_uxa_get_pixmap_bo (pixmap);
    
	if (bo) {
	    dri_bo_unmap (bo);
	    dri_bo_unreference (bo);
	}
    }
    fbDestroyPixmap (pixmap);
    return TRUE;
}

void i830_uxa_create_screen_resources(ScreenPtr pScreen)
{
    ScrnInfoPtr scrn = xf86Screens[pScreen->myNum];
    I830Ptr i830 = I830PTR(scrn);
    dri_bo *bo = i830->front_buffer->bo;

    if (bo != NULL) {
	PixmapPtr   pixmap = pScreen->GetScreenPixmap(pScreen);
	dri_bo_map (bo, i830->front_buffer->alignment);
	pixmap->devPrivate.ptr = bo->virtual;
	dri_bo_unmap (bo);
	i830_uxa_set_pixmap_bo (pixmap, bo);
    }
}

Bool
i830_uxa_init (ScreenPtr pScreen)
{
    ScrnInfoPtr scrn = xf86Screens[pScreen->myNum];
    I830Ptr i830 = I830PTR(scrn);

    if (!dixRequestPrivate(uxa_pixmap_key, 0))
	return FALSE;
    
    i830->uxa_driver = uxa_driver_alloc();
    if (i830->uxa_driver == NULL) {
	i830->accel = ACCEL_NONE;
	return FALSE;
    }
    memset(i830->uxa_driver, 0, sizeof(*i830->uxa_driver));

    i830->bufferOffset = 0;
    i830->uxa_driver->uxa_major = 1;
    i830->uxa_driver->uxa_minor = 0;

    i830->uxa_driver->maxX = i830->accel_max_x;
    i830->uxa_driver->maxY = i830->accel_max_y;

    /* Sync */
    i830->uxa_driver->WaitMarker = I830EXASync;

    /* Solid fill */
    i830->uxa_driver->PrepareSolid = I830EXAPrepareSolid;
    i830->uxa_driver->Solid = I830EXASolid;
    i830->uxa_driver->DoneSolid = I830EXADoneSolid;

    /* Copy */
    i830->uxa_driver->PrepareCopy = I830EXAPrepareCopy;
    i830->uxa_driver->Copy = I830EXACopy;
    i830->uxa_driver->DoneCopy = I830EXADoneCopy;

    /* Composite */
    if (!IS_I9XX(i830)) {
    	i830->uxa_driver->CheckComposite = i830_check_composite;
    	i830->uxa_driver->PrepareComposite = i830_prepare_composite;
    	i830->uxa_driver->Composite = i830_composite;
    	i830->uxa_driver->DoneComposite = i830_done_composite;
    } else if (IS_I915G(i830) || IS_I915GM(i830) ||
	       IS_I945G(i830) || IS_I945GM(i830) || IS_G33CLASS(i830))
    {
	i830->uxa_driver->CheckComposite = i915_check_composite;
   	i830->uxa_driver->PrepareComposite = i915_prepare_composite;
    	i830->uxa_driver->Composite = i830_composite;
    	i830->uxa_driver->DoneComposite = i830_done_composite;
    } else {
 	i830->uxa_driver->CheckComposite = i965_check_composite;
 	i830->uxa_driver->PrepareComposite = i965_prepare_composite;
 	i830->uxa_driver->Composite = i965_composite;
 	i830->uxa_driver->DoneComposite = i830_done_composite;
    }

    i830->uxa_driver->PrepareAccess = i830_uxa_prepare_access;
    i830->uxa_driver->FinishAccess = i830_uxa_finish_access;
    i830->uxa_driver->PixmapIsOffscreen = i830_uxa_pixmap_is_offscreen;

    if(!uxa_driver_init(pScreen, i830->uxa_driver)) {
	xf86DrvMsg(scrn->scrnIndex, X_INFO,
		   "UXA initialization failed\n");
	xfree(i830->uxa_driver);
	i830->accel = ACCEL_NONE;
	return FALSE;
    }

    pScreen->CreatePixmap = i830_uxa_create_pixmap;
    pScreen->DestroyPixmap = i830_uxa_destroy_pixmap;

    I830SelectBuffer(scrn, I830_SELECT_FRONT);

    return TRUE;
}

#ifdef XF86DRI

#ifndef ExaOffscreenMarkUsed
extern void ExaOffscreenMarkUsed(PixmapPtr);
#endif

unsigned long long
I830TexOffsetStart(PixmapPtr pPix)
{
    exaMoveInPixmap(pPix);
    ExaOffscreenMarkUsed(pPix);

    return exaGetPixmapOffset(pPix);
}
#endif
