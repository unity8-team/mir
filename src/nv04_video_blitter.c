/*
 * Copyright 2007 Arthur Huillet
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86xv.h"
#include <X11/extensions/Xv.h>
#include "exa.h"
#include "damage.h"
#include "dixstruct.h"
#include "fourcc.h"

#include "nv_include.h"
#include "nv_dma.h"

/* To support EXA 2.0, 2.1 has this in the header */
#ifndef exaMoveInPixmap
extern void exaMoveInPixmap(PixmapPtr pPixmap);
#endif

#define FOURCC_RGB 0x0000003

extern Atom xvSetDefaults, xvSyncToVBlank;

/**
 * NVPutBlitImage
 * 
 * @param pScrn screen
 * @param src_offset offset of image data in VRAM
 * @param id pixel format
 * @param src_pitch source pitch
 * @param dstBox 
 * @param x1
 * @param y1
 * @param x2
 * @param y2
 * @param width
 * @param height
 * @param src_w
 * @param src_h
 * @param drw_w
 * @param drw_h
 * @param clipBoxes
 * @param pDraw
 */
void
NVPutBlitImage(ScrnInfoPtr pScrn, int src_offset, int id,
               int src_pitch, BoxPtr dstBox,
               int x1, int y1, int x2, int y2,
               short width, short height,
               short src_w, short src_h,
               short drw_w, short drw_h,
               RegionPtr clipBoxes,
               DrawablePtr pDraw)
{
        NVPtr          pNv   = NVPTR(pScrn);
        NVPortPrivPtr  pPriv = GET_BLIT_PRIVATE(pNv);
        BoxPtr         pbox;
        int            nbox;
        CARD32         dsdx, dtdy;
        CARD32         dst_size, dst_point;
        CARD32         src_point, src_format;

        unsigned int crtcs;
        PixmapPtr pPix = NVGetDrawablePixmap(pDraw);
        int dst_format;

	/* This has to be called always, since it does more than just migration. */
	exaMoveInPixmap(pPix);
	ExaOffscreenMarkUsed(pPix);

        NVAccelGetCtxSurf2DFormatFromPixmap(pPix, &dst_format);
        BEGIN_RING(NvContextSurfaces, NV04_CONTEXT_SURFACES_2D_FORMAT, 4);
        OUT_RING  (dst_format);
        OUT_RING  ((exaGetPixmapPitch(pPix) << 16) | exaGetPixmapPitch(pPix));
        OUT_PIXMAPl(pPix, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
        OUT_PIXMAPl(pPix, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);

#ifdef COMPOSITE
        /* Adjust coordinates if drawing to an offscreen pixmap */
        if (pPix->screen_x || pPix->screen_y) {
                REGION_TRANSLATE(pScrn->pScreen, clipBoxes,
                                                     -pPix->screen_x,
                                                     -pPix->screen_y);
                dstBox->x1 -= pPix->screen_x;
                dstBox->x2 -= pPix->screen_x;
                dstBox->y1 -= pPix->screen_y;
                dstBox->y2 -= pPix->screen_y;
        }

        DamageDamageRegion((DrawablePtr)pPix, clipBoxes);
#endif

        pbox = REGION_RECTS(clipBoxes);
        nbox = REGION_NUM_RECTS(clipBoxes);

        dsdx = (src_w << 20) / drw_w;
        dtdy = (src_h << 20) / drw_h;

        dst_size  = ((dstBox->y2 - dstBox->y1) << 16) |
                     (dstBox->x2 - dstBox->x1);
        dst_point = (dstBox->y1 << 16) | dstBox->x1;

        src_pitch |= (NV04_SCALED_IMAGE_FROM_MEMORY_FORMAT_ORIGIN_CENTER |
                      NV04_SCALED_IMAGE_FROM_MEMORY_FORMAT_FILTER_BILINEAR);
        src_point = ((y1 << 4) & 0xffff0000) | (x1 >> 12);

        switch(id) {
        case FOURCC_RGB:
                src_format =
                        NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_FORMAT_X8R8G8B8;
                break;
        case FOURCC_UYVY:
                src_format =
                        NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_FORMAT_YB8V8YA8U8;
                break;
        default:
                src_format =
                        NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_FORMAT_V8YB8U8YA8;
                break;
        }

        if(pPriv->SyncToVBlank) {
                crtcs = nv_window_belongs_to_crtc(pScrn, dstBox->x1, dstBox->y1,
                        dstBox->x2, dstBox->y2);

                FIRE_RING();
                if (crtcs & 0x1)
                        NVWaitVSync(pScrn, 0);
                else if (crtcs & 0x2)
                        NVWaitVSync(pScrn, 1);
        }

        if(pNv->BlendingPossible) {
                BEGIN_RING(NvScaledImage,
                                NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_FORMAT, 2);
                OUT_RING  (src_format);
                OUT_RING  (NV04_SCALED_IMAGE_FROM_MEMORY_OPERATION_SRCCOPY);
        } else {
                BEGIN_RING(NvScaledImage,
                                NV04_SCALED_IMAGE_FROM_MEMORY_COLOR_FORMAT, 2);
                OUT_RING  (src_format);
        }

        while(nbox--) {
                BEGIN_RING(NvRectangle,
                                NV04_GDI_RECTANGLE_TEXT_COLOR1_A, 1);
                OUT_RING  (0);

                BEGIN_RING(NvScaledImage,
                                NV04_SCALED_IMAGE_FROM_MEMORY_CLIP_POINT, 6);
                OUT_RING  ((pbox->y1 << 16) | pbox->x1);
                OUT_RING  (((pbox->y2 - pbox->y1) << 16) |
                                 (pbox->x2 - pbox->x1));
                OUT_RING  (dst_point);
                OUT_RING  (dst_size);
                OUT_RING  (dsdx);
                OUT_RING  (dtdy);

                BEGIN_RING(NvScaledImage,
                                NV04_SCALED_IMAGE_FROM_MEMORY_SIZE, 4);
                OUT_RING  ((height << 16) | width);
                OUT_RING  (src_pitch);
                OUT_RING  (src_offset);
                OUT_RING  (src_point);
                pbox++;
        }

        FIRE_RING();

        exaMarkSync(pScrn->pScreen);

        pPriv->videoStatus = FREE_TIMER;
        pPriv->videoTime = currentTime.milliseconds + FREE_DELAY;
        extern void NVVideoTimerCallback(ScrnInfoPtr, Time);
	pNv->VideoTimerCallback = NVVideoTimerCallback;
}


/**
 * NVSetBlitPortAttribute
 * sets the attribute "attribute" of port "data" to value "value"
 * supported attributes:
 * - xvSyncToVBlank (values: 0,1)
 * - xvSetDefaults (values: NA; SyncToVBlank will be set, if hardware supports it)
 * 
 * @param pScrenInfo
 * @param attribute attribute to set
 * @param value value to which attribute is to be set
 * @param data port from which the attribute is to be set
 * 
 * @return Success, if setting is successful
 * BadValue/BadMatch, if value/attribute are invalid
 */
int
NVSetBlitPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
                       INT32 value, pointer data)
{
        NVPortPrivPtr pPriv = (NVPortPrivPtr)data;
        NVPtr           pNv = NVPTR(pScrn);

        if ((attribute == xvSyncToVBlank) && pNv->WaitVSyncPossible) {
                if ((value < 0) || (value > 1))
                        return BadValue;
                pPriv->SyncToVBlank = value;
        } else
        if (attribute == xvSetDefaults) {
                pPriv->SyncToVBlank = pNv->WaitVSyncPossible;
        } else
                return BadMatch;

        return Success;
}

/**
 * NVGetBlitPortAttribute
 * reads the value of attribute "attribute" from port "data" into INT32 "*value"
 * currently only one attribute supported: xvSyncToVBlank
 * 
 * @param pScrn unused
 * @param attribute attribute to be read
 * @param value value of attribute will be stored here
 * @param data port from which attribute will be read
 * @return Success, if queried attribute exists
 */
int
NVGetBlitPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
                       INT32 *value, pointer data)
{
        NVPortPrivPtr pPriv = (NVPortPrivPtr)data;

        if(attribute == xvSyncToVBlank)
                *value = (pPriv->SyncToVBlank) ? 1 : 0;
        else
                return BadMatch;

        return Success;
}

/**
 * NVStopBlitVideo
 */
void
NVStopBlitVideo(ScrnInfoPtr pScrn, pointer data, Bool Exit)
{
}

