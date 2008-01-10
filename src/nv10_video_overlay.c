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

extern Atom xvBrightness, xvContrast, xvColorKey, xvSaturation,       
          xvHue, xvAutopaintColorKey, xvSetDefaults, xvDoubleBuffer,
	       xvITURBT709, xvSyncToVBlank, xvOnCRTCNb;

/**
 * NV10PutOverlayImage
 * program hardware to overlay image into front buffer
 * 
 * @param pScrn screen
 * @param offset card offset to the pixel data
 * @param id format of image
 * @param dstPitch pitch of the pixel data in VRAM
 * @param dstBox destination box
 * @param x1 first source point - x
 * @param y1 first source point - y
 * @param x2 second source point - x
 * @param y2 second source point - y
 * @param width width of the source image = x2 - x1
 * @param height height
 * @param src_w width of the image data in VRAM
 * @param src_h height
 * @param drw_w width of the image to draw to screen
 * @param drw_h height
 * @param clipBoxes ???
 */
void
NV10PutOverlayImage(ScrnInfoPtr pScrn, int offset, int uvoffset, int id,
                  int dstPitch, BoxPtr dstBox,
                  int x1, int y1, int x2, int y2,
                  short width, short height,
                  short src_w, short src_h,
                  short drw_w, short drw_h,
                  RegionPtr clipBoxes)
{
        NVPtr         pNv    = NVPTR(pScrn);
        NVPortPrivPtr pPriv  = GET_OVERLAY_PRIVATE(pNv);
        int           buffer = pPriv->currentBuffer;

        /* paint the color key */
        if(pPriv->autopaintColorKey && (pPriv->grabbedByV4L ||
                !REGION_EQUAL(pScrn->pScreen, &pPriv->clip, clipBoxes))) {
                /* we always paint V4L's color key */
                if (!pPriv->grabbedByV4L)
                        REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);
                {
                xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
                }
        }

        if(pNv->CurrentLayout.mode->Flags & V_DBLSCAN) {
                dstBox->y1 <<= 1;
                dstBox->y2 <<= 1;
                drw_h <<= 1;
        }

        //xf86DrvMsg(0, X_INFO, "SIZE_IN h %d w %d, POINT_IN x %d y %d, DS_DX %d DT_DY %d, POINT_OUT x %d y %d SIZE_OUT h %d w %d\n", height, width, x1 >>
	//16,y1>>16, (src_w << 20) / drw_w, (src_h << 20) / drw_h,  (dstBox->x1),(dstBox->y1), (dstBox->y2 - dstBox->y1), (dstBox->x2 - dstBox->x1));

        nvWriteVIDEO(pNv, NV_PVIDEO_BASE(buffer)     , 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_OFFSET_BUFF(buffer)     , offset);
        nvWriteVIDEO(pNv, NV_PVIDEO_SIZE_IN(buffer)  , (height << 16) | width);
        nvWriteVIDEO(pNv, NV_PVIDEO_POINT_IN(buffer) ,
                          ((y1 << 4) & 0xffff0000) | (x1 >> 12));
        nvWriteVIDEO(pNv, NV_PVIDEO_DS_DX(buffer)    , (src_w << 20) / drw_w);
        nvWriteVIDEO(pNv, NV_PVIDEO_DT_DY(buffer)    , (src_h << 20) / drw_h);
        nvWriteVIDEO(pNv, NV_PVIDEO_POINT_OUT(buffer),
                          (dstBox->y1 << 16) | dstBox->x1);
        nvWriteVIDEO(pNv, NV_PVIDEO_SIZE_OUT(buffer) ,
                          ((dstBox->y2 - dstBox->y1) << 16) |
                           (dstBox->x2 - dstBox->x1));

        dstPitch |= NV_PVIDEO_FORMAT_DISPLAY_COLOR_KEY;   /* use color key */
        if(id != FOURCC_UYVY)
                dstPitch |= NV_PVIDEO_FORMAT_COLOR_LE_CR8YB8CB8YA8;
        if(pPriv->iturbt_709)
                dstPitch |= NV_PVIDEO_FORMAT_MATRIX_ITURBT709;

        if( id == FOURCC_YV12 || id == FOURCC_I420 )
                dstPitch |= NV_PVIDEO_FORMAT_PLANAR;

        /* Those are important only for planar formats (NV12) */
        if ( uvoffset )
                {
                nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_BASE(buffer), 0);
                nvWriteVIDEO(pNv, NV_PVIDEO_UVPLANE_OFFSET_BUFF(buffer), uvoffset);
                }

        nvWriteVIDEO(pNv, NV_PVIDEO_FORMAT(buffer), dstPitch);
        nvWriteVIDEO(pNv, NV_PVIDEO_STOP, 0);
        nvWriteVIDEO(pNv, NV_PVIDEO_BUFFER, buffer ? 0x10 :  0x1);

        pPriv->videoStatus = CLIENT_VIDEO_ON;
}

/**
 * NV10SetOverlayPortAttribute
 * sets the attribute "attribute" of port "data" to value "value"
 * calls NVResetVideo(pScrn) to apply changes to hardware
 * 
 * @param pScrenInfo
 * @param attribute attribute to set
 * @param value value to which attribute is to be set
 * @param data port from which the attribute is to be set
 * 
 * @return Success, if setting is successful
 * BadValue/BadMatch, if value/attribute are invalid
 * @see NVResetVideo(ScrnInfoPtr pScrn)
 */
int
NV10SetOverlayPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
                          INT32 value, pointer data)
{
        NVPortPrivPtr pPriv = (NVPortPrivPtr)data;
        NVPtr         pNv   = NVPTR(pScrn);

        if (attribute == xvBrightness) {
                if ((value < -512) || (value > 512))
                        return BadValue;
                pPriv->brightness = value;
        } else
        if (attribute == xvDoubleBuffer) {
                if ((value < 0) || (value > 1))
                        return BadValue;
                pPriv->doubleBuffer = value;
        } else
        if (attribute == xvContrast) {
                if ((value < 0) || (value > 8191))
                        return BadValue;
                pPriv->contrast = value;
        } else
        if (attribute == xvHue) {
                value %= 360;
                if (value < 0)
                        value += 360;
                pPriv->hue = value;
        } else
        if (attribute == xvSaturation) {
                if ((value < 0) || (value > 8191))
                        return BadValue;
                pPriv->saturation = value;
        } else
        if (attribute == xvColorKey) {
                pPriv->colorKey = value;
                REGION_EMPTY(pScrn->pScreen, &pPriv->clip);
        } else
        if (attribute == xvAutopaintColorKey) {
                if ((value < 0) || (value > 1))
                        return BadValue;
                pPriv->autopaintColorKey = value;
        } else
        if (attribute == xvITURBT709) {
                if ((value < 0) || (value > 1))
                        return BadValue;
                pPriv->iturbt_709 = value;
        } else
        if (attribute == xvSetDefaults) {
                NVSetPortDefaults(pScrn, pPriv);
        } else
        if ( attribute == xvOnCRTCNb) {
                if ((value < 0) || (value > 1))
                        return BadValue;
                pPriv->overlayCRTC = value;
                nvWriteCRTC(pNv, value, NV_CRTC_FSEL, nvReadCRTC(pNv, value, NV_CRTC_FSEL) | NV_CRTC_FSEL_OVERLAY);
                nvWriteCRTC(pNv, !value, NV_CRTC_FSEL, nvReadCRTC(pNv, !value, NV_CRTC_FSEL) & ~NV_CRTC_FSEL_OVERLAY);
        } else
                return BadMatch;

        NV10WriteOverlayParameters(pScrn);

        return Success;
}

/**
 * NV10GetOverlayPortAttribute
 * 
 * @param pScrn unused
 * @param attribute attribute to be read
 * @param value value of attribute will be stored in this pointer
 * @param data port from which attribute will be read
 * @return Success, if queried attribute exists
 */
int
NV10GetOverlayPortAttribute(ScrnInfoPtr pScrn, Atom attribute,
                          INT32 *value, pointer data)
{
        NVPortPrivPtr pPriv = (NVPortPrivPtr)data;

        if (attribute == xvBrightness)
                *value = pPriv->brightness;
        else if (attribute == xvDoubleBuffer)
                *value = (pPriv->doubleBuffer) ? 1 : 0;
        else if (attribute == xvContrast)
                *value = pPriv->contrast;
        else if (attribute == xvSaturation)
                *value = pPriv->saturation;
        else if (attribute == xvHue)
                *value = pPriv->hue;
        else if (attribute == xvColorKey)
                *value = pPriv->colorKey;
        else if (attribute == xvAutopaintColorKey)
                *value = (pPriv->autopaintColorKey) ? 1 : 0;
        else if (attribute == xvITURBT709)
                *value = (pPriv->iturbt_709) ? 1 : 0;
        else if (attribute == xvOnCRTCNb)
                *value = (pPriv->overlayCRTC) ? 1 : 0;
        else
                return BadMatch;

        return Success;
}

/**
 * NV10StopOverlay
 * Tell the hardware to stop the overlay
 */
void
NV10StopOverlay (ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    nvWriteVIDEO(pNv, NV_PVIDEO_STOP, 1);
}

/** 
 * NV10WriteOverlayParameters
 * Tell the hardware about parameters that are too expensive to be set
 * on every frame
 */
void
NV10WriteOverlayParameters (ScrnInfoPtr pScrn)
{
    NVPtr          pNv     = NVPTR(pScrn);
    NVPortPrivPtr  pPriv   = GET_OVERLAY_PRIVATE(pNv);
    int            satSine, satCosine;
    double         angle;

    angle = (double)pPriv->hue * 3.1415927 / 180.0;

    satSine = pPriv->saturation * sin(angle);
    if (satSine < -1024)
	satSine = -1024;
    satCosine = pPriv->saturation * cos(angle);
    if (satCosine < -1024)
	satCosine = -1024;

    nvWriteVIDEO(pNv, NV_PVIDEO_LUMINANCE(0), (pPriv->brightness << 16) |
	    pPriv->contrast);
    nvWriteVIDEO(pNv, NV_PVIDEO_LUMINANCE(1), (pPriv->brightness << 16) |
	    pPriv->contrast);
    nvWriteVIDEO(pNv, NV_PVIDEO_CHROMINANCE(0), (satSine << 16) |
	    (satCosine & 0xffff));
    nvWriteVIDEO(pNv, NV_PVIDEO_CHROMINANCE(1), (satSine << 16) |
	    (satCosine & 0xffff));
    nvWriteVIDEO(pNv, NV_PVIDEO_COLOR_KEY, pPriv->colorKey);

}

