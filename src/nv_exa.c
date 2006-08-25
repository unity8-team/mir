 /***************************************************************************\
|*                                                                           *|
|*       Copyright 2003 NVIDIA, Corporation.  All rights reserved.           *|
|*                                                                           *|
|*     NOTICE TO USER:   The source code  is copyrighted under  U.S. and     *|
|*     international laws.  Users and possessors of this source code are     *|
|*     hereby granted a nonexclusive,  royalty-free copyright license to     *|
|*     use this code in individual and commercial software.                  *|
|*                                                                           *|
|*     Any use of this source code must include,  in the user documenta-     *|
|*     tion and  internal comments to the code,  notices to the end user     *|
|*     as follows:                                                           *|
|*                                                                           *|
|*       Copyright 2003 NVIDIA, Corporation.  All rights reserved.           *|
|*                                                                           *|
|*     NVIDIA, CORPORATION MAKES NO REPRESENTATION ABOUT THE SUITABILITY     *|
|*     OF  THIS SOURCE  CODE  FOR ANY PURPOSE.  IT IS  PROVIDED  "AS IS"     *|
|*     WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.  NVIDIA, CORPOR-     *|
|*     ATION DISCLAIMS ALL WARRANTIES  WITH REGARD  TO THIS SOURCE CODE,     *|
|*     INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGE-     *|
|*     MENT,  AND FITNESS  FOR A PARTICULAR PURPOSE.   IN NO EVENT SHALL     *|
|*     NVIDIA, CORPORATION  BE LIABLE FOR ANY SPECIAL,  INDIRECT,  INCI-     *|
|*     DENTAL, OR CONSEQUENTIAL DAMAGES,  OR ANY DAMAGES  WHATSOEVER RE-     *|
|*     SULTING FROM LOSS OF USE,  DATA OR PROFITS,  WHETHER IN AN ACTION     *|
|*     OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,  ARISING OUT OF     *|
|*     OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOURCE CODE.     *|
|*                                                                           *|
|*     U.S. Government  End  Users.   This source code  is a "commercial     *|
|*     item,"  as that  term is  defined at  48 C.F.R. 2.101 (OCT 1995),     *|
|*     consisting  of "commercial  computer  software"  and  "commercial     *|
|*     computer  software  documentation,"  as such  terms  are  used in     *|
|*     48 C.F.R. 12.212 (SEPT 1995)  and is provided to the U.S. Govern-     *|
|*     ment only as  a commercial end item.   Consistent with  48 C.F.R.     *|
|*     12.212 and  48 C.F.R. 227.7202-1 through  227.7202-4 (JUNE 1995),     *|
|*     all U.S. Government End Users  acquire the source code  with only     *|
|*     those rights set forth herein.                                        *|
|*                                                                           *|
 \***************************************************************************/

/*
  Exa Modifications (c) Lars Knoll (lars@trolltech.com)
 */

#include "nv_include.h"
#include "exa.h"

#if (EXA_VERSION_MAJOR < 2)
#error You need EXA >=2.0.0
#endif

#include "nv_dma.h"
#include "nv_local.h"

#include <sys/time.h>

static CARD32 getPitch(DrawablePtr pDrawable)
{
    return (pDrawable->width*(pDrawable->bitsPerPixel >> 3) + 63) & ~63;
}

static CARD32 getOffset(NVPtr pNv, DrawablePtr pDrawable)
{
    PixmapPtr pPixmap;
    if (pDrawable->type == DRAWABLE_WINDOW)
        return 0;

    pPixmap = (PixmapPtr)pDrawable;
    return (CARD32)((unsigned long)pPixmap->devPrivate.ptr - (unsigned long)pNv->FbBase);
}

static CARD32 surfaceFormat(DrawablePtr pDrawable)
{
    switch(pDrawable->bitsPerPixel) {
    case 32:
    case 24:
        return SURFACE_FORMAT_X8R8G8B8;
        break;
    case 16:
        return SURFACE_FORMAT_R5G6B5;
        break;
    default:
        return SURFACE_FORMAT_Y8;
        break;
    }
}

static CARD32 rectFormat(DrawablePtr pDrawable)
{
    switch(pDrawable->bitsPerPixel) {
    case 32:
    case 24:
        return RECT_FORMAT_DEPTH24;
        break;
    case 16:
        return RECT_FORMAT_DEPTH16;
        break;
    default:
        return RECT_FORMAT_DEPTH8;
        break;
    }
}

/* EXA acceleration hooks */

static void NVExaWaitMarker(ScreenPtr pScreen, int marker)
{
    NVSync(xf86Screens[pScreen->myNum]);
}

static Bool NVExaPrepareSolid (PixmapPtr      pPixmap,
                               int            alu,
                               Pixel          planemask,
                               Pixel          fg)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);
    CARD32 pitch;

    planemask |= ~0 << pNv->CurrentLayout.depth;

    NVSetRopSolid(pScrn, alu, planemask);

    NVDmaStart(pNv, NvSubContextSurfaces, SURFACE_FORMAT, 4);

    NVDmaNext (pNv, surfaceFormat(&pPixmap->drawable));

    pitch = getPitch(&pPixmap->drawable);
    NVDmaNext (pNv, (pitch<<16)|pitch);
  
    NVDmaNext (pNv, getOffset(pNv, &pPixmap->drawable));
    NVDmaNext (pNv, getOffset(pNv, &pPixmap->drawable));
    
    NVDmaStart(pNv, NvSubRectangle, RECT_FORMAT, 1);
    NVDmaNext(pNv, rectFormat(&pPixmap->drawable));
    NVDmaStart(pNv, NvSubRectangle, RECT_SOLID_COLOR, 1);
    NVDmaNext (pNv, fg);

    pNv->DMAKickoffCallback = NVDMAKickoffCallback;

    return TRUE;
}

static void NVExaSolid (PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
    ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);
    int width = x2-x1;
    int height = y2-y1;

    NVDmaStart(pNv, NvSubRectangle, RECT_SOLID_RECTS(0), 2);
    NVDmaNext (pNv, (x1 << 16) | y1);
    NVDmaNext (pNv, (width << 16) | height);

    if((width * height) >= 512)
        NVDmaKickoff(pNv);
}

static void NVExaDoneSolid (PixmapPtr      pPixmap)
{
}

static Bool NVExaPrepareCopy (PixmapPtr       pSrcPixmap,
                              PixmapPtr       pDstPixmap,
                              int             dx,
                              int             dy,
                              int             alu,
                              Pixel           planemask)
{
    ScrnInfoPtr pScrn = xf86Screens[pSrcPixmap->drawable.pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);
    CARD32 srcPitch, dstPitch;

    planemask |= ~0 << pNv->CurrentLayout.depth;
    
    NVSetRopSolid(pScrn, alu, planemask);
    
    dstPitch = getPitch(&pDstPixmap->drawable);
    srcPitch = getPitch(&pSrcPixmap->drawable);
    NVDmaStart(pNv, NvSubContextSurfaces, SURFACE_PITCH, 3);
    NVDmaNext (pNv, (dstPitch<<16)|srcPitch);
    NVDmaNext (pNv, getOffset(pNv, &pSrcPixmap->drawable));
    NVDmaNext (pNv, getOffset(pNv, &pDstPixmap->drawable));

    pNv->DMAKickoffCallback = NVDMAKickoffCallback;
    return TRUE;
}

static void NVExaCopy (PixmapPtr pDstPixmap,
                       int    srcX,
                       int    srcY,
                       int    dstX,
                       int    dstY,
                       int    width,
                       int    height)
{
    ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);

    NVDmaStart(pNv, NvSubImageBlit, BLIT_POINT_SRC, 3);
    NVDmaNext (pNv, (srcY << 16) | srcX);
    NVDmaNext (pNv, (dstY << 16) | dstX);
    NVDmaNext (pNv, (height  << 16) | width);

    if((width * height) >= 512)
       NVDmaKickoff(pNv); 
}

static void NVExaDoneCopy (PixmapPtr pDstPixmap) {}

static Bool NVDownloadFromScreen(PixmapPtr pSrc,
                                 int x,  int y,
                                 int w,  int h,
                                 char *dst,  int dst_pitch)
{
    ScrnInfoPtr pScrn = xf86Screens[pSrc->drawable.pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);
    CARD32 offset_in, pitch_in, max_lines, line_length;
    Bool ret = TRUE;
    
    pitch_in = getPitch(&pSrc->drawable);
    offset_in = getOffset(pNv, &pSrc->drawable);
    offset_in += y*pitch_in;
    offset_in += x * (pSrc->drawable.bitsPerPixel >> 3);
    max_lines = 65536/dst_pitch + 1;
    line_length = w * (pSrc->drawable.bitsPerPixel >> 3);

    NVDmaSetObjectOnSubchannel(pNv, NvSubGraphicsToAGP, NvGraphicsToAGP);
    
    NVDEBUG("NVDownloadFromScreen: x=%d, y=%d, w=%d, h=%d\n", x, y, w, h);
    NVDEBUG("    pitch_in=%x dst_pitch=%x offset_in=%x", pitch_in, dst_pitch, offset_in);
    while (h > 0) {
        NVDEBUG("     max_lines=%d, h=%d\n", max_lines, h);
        int nlines = h > max_lines ? max_lines : h;
        /* reset the notification object */
        memset(pNv->agpMemory, 0xff, 0x100);
        NVDmaStart(pNv, NvSubGraphicsToAGP, MEMFORMAT_NOTIFY, 1);
        NVDmaNext (pNv, 0);
        NVDmaStart(pNv, NvSubGraphicsToAGP, MEMFORMAT_OFFSET_IN, 8);
        NVDmaNext (pNv, offset_in);
        NVDmaNext (pNv, 0);
        NVDmaNext (pNv, pitch_in);
        NVDmaNext (pNv, dst_pitch);
        NVDmaNext (pNv, line_length);
        NVDmaNext (pNv, nlines);
        NVDmaNext (pNv, 0x101);
        NVDmaNext (pNv, 0);
        NVDmaKickoff(pNv);
        if (!NVDmaWaitForNotifier(pNv, NV_DMA_TARGET_AGP, 0)) {
            ret = FALSE;
            goto error;
        }
#if 0
        if (memcmp(pNv->FbBase + offset_in, pNv->agpMemory + 0x10000, nlines*dst_pitch) != 0)
            ErrorF("DMA transfer wrong!\n");
#endif
        memcpy(dst, pNv->agpMemory + 0x10000, nlines*dst_pitch);
        h -= nlines;
        offset_in += nlines*pitch_in;
        dst += nlines*dst_pitch;
    }

error:
    NVDmaSetObjectOnSubchannel(pNv, NvSubGraphicsToAGP, NvScaledImage);
    exaMarkSync(pSrc->drawable.pScreen);
    return ret;
}

static Bool NVUploadToScreen(PixmapPtr pDst,
                             int x, int y, int w, int h,
                             char *src, int src_pitch)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);
    CARD32 offset_out, pitch_out, max_lines, line_length;
    Bool ret = TRUE;
#if 0
    x = 0;
    y = 0;
    w = pDst->drawable.width;
    h = pDst->drawable.height;
#endif

    pitch_out = getPitch(&pDst->drawable);
    offset_out = getOffset(pNv, &pDst->drawable);
    offset_out += y*pitch_out;
    offset_out += x * (pDst->drawable.bitsPerPixel >> 3);

    max_lines = 65536/src_pitch + 1;
    line_length = w * (pDst->drawable.bitsPerPixel >> 3);

    NVDmaSetObjectOnSubchannel(pNv, NvSubGraphicsToAGP, NvAGPToGraphics);
    
    NVDEBUG("NVUploadToScreen: x=%d, y=%d, w=%d, h=%d\n", x, y, w, h);
    while (h > 0) {
        NVDEBUG("     max_lines=%d, h=%d\n", max_lines, h);
        int nlines = h > max_lines ? max_lines : h;
        /* reset the notification object */
        memset(pNv->agpMemory, 0xff, 0x100);
        memcpy(pNv->agpMemory + 0x10000, src, nlines*src_pitch);
        NVDmaStart(pNv, NvSubGraphicsToAGP, MEMFORMAT_NOTIFY, 1);
        NVDmaNext (pNv, 0);
        NVDmaStart(pNv, NvSubGraphicsToAGP, MEMFORMAT_OFFSET_IN, 8);
        NVDmaNext (pNv, 0);
        NVDmaNext (pNv, offset_out);
        NVDmaNext (pNv, src_pitch);
        NVDmaNext (pNv, pitch_out);
        NVDmaNext (pNv, line_length);
        NVDmaNext (pNv, nlines);
        NVDmaNext (pNv, 0x101);
        NVDmaNext (pNv, 0);
        NVDmaKickoff(pNv);
        if (!NVDmaWaitForNotifier(pNv, NV_DMA_TARGET_AGP, 0)) {
            ret = FALSE;
            goto error;
        }
        h -= nlines;
        offset_out += nlines*pitch_out;
        src += nlines*src_pitch;
    }

error:
    NVDmaSetObjectOnSubchannel(pNv, NvSubGraphicsToAGP, NvScaledImage);
    exaMarkSync(pDst->drawable.pScreen);
    return ret;
}


static Bool NVCheckComposite (int          op,
                            PicturePtr   pSrcPicture,
                            PicturePtr   pMaskPicture,
                            PicturePtr   pDstPicture)
{
    CARD32 ret = 0;
    if (pMaskPicture)
        ret = 0x1;
    /* PictOpOver doesn't work correctly. The HW command assumes non premuliplied alpha */
    else if (op != PictOpOver && op != PictOpSrc)
        ret = 0x2;
    else if (!pSrcPicture->pDrawable)
        ret = 0x4;
    else if (pSrcPicture->transform || pSrcPicture->repeat)
        ret = 0x8;
    else if (pSrcPicture->alphaMap || pDstPicture->alphaMap)
        ret = 0x10;
    else if (pSrcPicture->format != PICT_a8r8g8b8 &&
        pSrcPicture->format != PICT_x8r8g8b8 &&
        pSrcPicture->format != PICT_r5g6b5)
        ret = 0x20;
    else if (pDstPicture->format != PICT_a8r8g8b8 &&
        pDstPicture->format != PICT_x8r8g8b8 &&
        pDstPicture->format != PICT_r5g6b5)
        ret = 0x40;
    return ret == 0;
}

static CARD32 src_size, src_pitch, src_offset;

static Bool NVPrepareComposite (int                op,
                                PicturePtr         pSrcPicture,
                                PicturePtr         pMaskPicture,
                                PicturePtr         pDstPicture,
                                PixmapPtr          pSrc,
                                PixmapPtr          pMask,
                                PixmapPtr          pDst)
{
    ScrnInfoPtr pScrn = xf86Screens[pSrcPicture->pDrawable->pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);

    int srcFormat, dstFormat;
    if (pSrcPicture->format == PICT_a8r8g8b8)
        srcFormat = STRETCH_BLIT_FORMAT_A8R8G8B8;
    else if (pSrcPicture->format == PICT_x8r8g8b8)
        srcFormat = STRETCH_BLIT_FORMAT_X8R8G8B8;
    else
        srcFormat = STRETCH_BLIT_FORMAT_DEPTH16;

    if (pDstPicture->format == PICT_a8r8g8b8)
        dstFormat = SURFACE_FORMAT_A8R8G8B8;
    else if (pDstPicture->format == PICT_x8r8g8b8)
        dstFormat = SURFACE_FORMAT_X8R8G8B8;
    else
        dstFormat = SURFACE_FORMAT_R5G6B5;

#if 0
    NVDmaStart(pNv, NvSubContextSurfaces, SURFACE_FORMAT, 1);
    NVDmaNext (pNv, dstFormat);
    NVDmaNext (pNv, getPitch(pSrcPicture->pDrawable)|(getPitch(pDstPicture->pDrawable) << 16));
    NVDmaNext (pNv, getOffset(pNv, pSrcPicture->pDrawable));
    NVDmaNext (pNv, getOffset(pNv, pDstPicture->pDrawable));
#endif
    NVDmaStart(pNv, NvSubScaledImage, STRETCH_BLIT_FORMAT, 2);
    NVDmaNext (pNv, srcFormat);
    NVDmaNext (pNv, (op == PictOpSrc) ? STRETCH_BLIT_OPERATION_COPY : STRETCH_BLIT_OPERATION_BLEND);

    src_size = pSrcPicture->pDrawable->width | (pSrcPicture->pDrawable->height << 16);
    src_pitch  = getPitch(pSrcPicture->pDrawable)
                 | (STRETCH_BLIT_SRC_FORMAT_ORIGIN_CORNER << 16)
                 | (STRETCH_BLIT_SRC_FORMAT_FILTER_POINT_SAMPLE << 24);
    src_offset = getOffset(pNv, pSrcPicture->pDrawable);
    return TRUE;
}

static void NVComposite (PixmapPtr         pDst,
                         int       srcX,
                         int        srcY,
                         int        maskX,
                         int        maskY,
                         int        dstX,
                         int        dstY,
                         int        width,
                         int        height)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);

    NVDmaStart(pNv, NvSubScaledImage, STRETCH_BLIT_CLIP_POINT, 6);
    NVDmaNext (pNv, dstX | (dstY << 16));
    NVDmaNext (pNv, width | (height << 16));
    NVDmaNext (pNv, dstX | (dstY << 16));
    NVDmaNext (pNv, width | (height << 16));
    NVDmaNext (pNv, 1<<20);
    NVDmaNext (pNv, 1<<20);

    NVDmaStart(pNv, NvSubScaledImage, STRETCH_BLIT_SRC_SIZE, 4);
    NVDmaNext (pNv, src_size);
    NVDmaNext (pNv, src_pitch);
    NVDmaNext (pNv, src_offset);
    NVDmaNext (pNv, srcX | (srcY<<16));

    NVDmaKickoff(pNv);
}

static void NVDoneComposite (PixmapPtr         pDst)
{
    ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);
    CARD32 format;
    if (pNv->CurrentLayout.depth == 8)
        format = SURFACE_FORMAT_Y8;
    else if (pNv->CurrentLayout.depth == 16)
        format = SURFACE_FORMAT_R5G6B5;
    else
        format = SURFACE_FORMAT_X8R8G8B8;
    NVDmaStart(pNv, NvSubContextSurfaces, SURFACE_FORMAT, 1);
    NVDmaNext (pNv, format);
    exaMarkSync(pDst->drawable.pScreen);
}

Bool NVExaInit(ScreenPtr pScreen) 
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    NVPtr pNv = NVPTR(pScrn);

    if(!(pNv->EXADriverPtr = (ExaDriverPtr) xnfcalloc(sizeof(ExaDriverRec), 1))) {
        pNv->NoAccel = TRUE;
        return FALSE;
    }
    		pNv->EXADriverPtr->exa_major = EXA_VERSION_MAJOR;
		pNv->EXADriverPtr->exa_minor = EXA_VERSION_MINOR;

		pNv->EXADriverPtr->memoryBase         = pNv->FbStart;
		pNv->EXADriverPtr->offScreenBase      = pScrn->virtualX*pScrn->virtualY*pScrn->depth; 
		pNv->EXADriverPtr->memorySize         = pNv->ScratchBufferStart; 
		pNv->EXADriverPtr->pixmapOffsetAlign  = 256; 
		pNv->EXADriverPtr->pixmapPitchAlign   = 64; 
		pNv->EXADriverPtr->flags              = EXA_OFFSCREEN_PIXMAPS;
		pNv->EXADriverPtr->maxX               = 32768;
		pNv->EXADriverPtr->maxY               = 32768;

		pNv->EXADriverPtr->WaitMarker = NVExaWaitMarker;

		pNv->EXADriverPtr->PrepareCopy = NVExaPrepareCopy;
		pNv->EXADriverPtr->Copy = NVExaCopy;
		pNv->EXADriverPtr->DoneCopy = NVExaDoneCopy;

		pNv->EXADriverPtr->PrepareSolid = NVExaPrepareSolid;
		pNv->EXADriverPtr->Solid = NVExaSolid;
    pNv->EXADriverPtr->DoneSolid = NVExaDoneSolid;

    if (pNv->agpMemory) {
        pNv->EXADriverPtr->DownloadFromScreen = NVDownloadFromScreen; 
        pNv->EXADriverPtr->UploadToScreen = NVUploadToScreen; 
    }
    if (pNv->BlendingPossible) {
        /* install composite hooks */
        pNv->EXADriverPtr->CheckComposite = NVCheckComposite;
        pNv->EXADriverPtr->PrepareComposite = NVPrepareComposite;
        pNv->EXADriverPtr->Composite = NVComposite;
        pNv->EXADriverPtr->DoneComposite = NVDoneComposite;
    }

#if 0
    {
        int i;
        struct timeval tv, tv2;
        ErrorF("\nTiming upload:\n");
        gettimeofday(&tv, 0);
        NVDmaSetObjectOnSubchannel(pNv, NvSubGraphicsToAGP, NvAGPToGraphics);

        for (i = 0; i < 0x10000; ++i)
            ((CARD32 *)(pNv->agpMemory+0x10000))[i] = (i<<16) | (0xffff-i);
        for (i = 0; i < 0x10000; ++i)
            ((CARD32 *)(pNv->FbBase))[i] = 0;
            
        /* reset the notification object */
        memset(pNv->agpMemory, 0xff, 0x100);
        NVDmaStart(pNv, NvSubGraphicsToAGP, MEMFORMAT_NOTIFY, 1);
        NVDmaNext (pNv, 0);
        NVDmaStart(pNv, NvSubGraphicsToAGP, MEMFORMAT_OFFSET_IN, 8);
        NVDmaNext (pNv, 0);
        NVDmaNext (pNv, 0);
        NVDmaNext (pNv, 1024);
        NVDmaNext (pNv, 1024);
        NVDmaNext (pNv, 1024);
        NVDmaNext (pNv, 1024);
        NVDmaNext (pNv, 0x101);
        NVDmaNext (pNv, 0);
        NVDmaKickoff(pNv);
        if (!NVDmaWaitForNotifier(pNv, NV_DMA_TARGET_AGP, 0))
            ErrorF("DMA transfer error!\n");

        NVDmaSetObjectOnSubchannel(pNv, NvSubGraphicsToAGP, NvScaledImage);
        gettimeofday(&tv2, 0);
        for (i = 0; i < 0x10000; i += 256)
            ErrorF("%x %x %x %x\n", ((CARD32 *)(pNv->FbBase))[i], ((CARD32 *)(pNv->FbBase))[i+1],
                   ((CARD32 *)(pNv->FbBase))[i+2], ((CARD32 *)(pNv->FbBase))[i+3]);
        ErrorF("Download from Screen %f MB/s\n", 
                   (1.)*1000000
                   /((int)tv2.tv_usec- (int)tv.tv_usec + 1000000*(tv2.tv_sec-tv.tv_sec)+1));
    }
#endif
    
    return exaDriverInit(pScreen, pNv->EXADriverPtr);
}
