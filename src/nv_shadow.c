/*
 * Copyright 1999 Mark Vojkovich
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

#include "nv_local.h"
#include "nv_include.h"
#include "nv_type.h"
#include "shadowfb.h"
#include "servermd.h"


void
NVRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    NVPtr pNv = NVPTR(pScrn);
    int width, height, Bpp, FBPitch;
    unsigned char *src, *dst;
   
    Bpp = pScrn->bitsPerPixel >> 3;
    FBPitch = BitmapBytePad(pScrn->displayWidth * pScrn->bitsPerPixel);

    while(num--) {
	width = (pbox->x2 - pbox->x1) * Bpp;
	height = pbox->y2 - pbox->y1;
	src = pNv->ShadowPtr + (pbox->y1 * pNv->ShadowPitch) + 
						(pbox->x1 * Bpp);
	dst = pNv->FB->map + (pbox->y1 * FBPitch) + (pbox->x1 * Bpp);

	while(height--) {
	    memcpy(dst, src, width);
	    dst += FBPitch;
	    src += pNv->ShadowPitch;
	}
	
	pbox++;
    }
} 

void
NVPointerMoved(int index, int x, int y)
{
    ScrnInfoPtr pScrn = xf86Screens[index];
    NVPtr pNv = NVPTR(pScrn);
    int newX, newY;

    if(pNv->Rotate == 1) {
	newX = pScrn->pScreen->height - y - 1;
	newY = x;
    } else {
	newX = y;
	newY = pScrn->pScreen->width - x - 1;
    }

    (*pNv->PointerMoved)(index, newX, newY);
}

void
NVRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    NVPtr pNv = NVPTR(pScrn);
    int count, width, height, y1, y2, dstPitch, srcPitch;
    CARD16 *dstPtr, *srcPtr, *src;
    CARD32 *dst;

    if(!pNv->Rotate) {
       NVRefreshArea(pScrn, num, pbox);
       return;
    }

    dstPitch = pScrn->displayWidth;
    srcPitch = -pNv->Rotate * pNv->ShadowPitch >> 1;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	y1 = pbox->y1 & ~1;
	y2 = (pbox->y2 + 1) & ~1;
	height = (y2 - y1) >> 1;  /* in dwords */

	if(pNv->Rotate == 1) {
	    dstPtr = (CARD16*)pNv->FB->map + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - y2;
	    srcPtr = (CARD16*)pNv->ShadowPtr + 
			((1 - y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD16*)pNv->FB->map + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + y1;
	    srcPtr = (CARD16*)pNv->ShadowPtr + 
			(y1 * srcPitch) + pbox->x2 - 1;
	}

	while(width--) {
	    src = srcPtr;
	    dst = (CARD32*)dstPtr;
	    count = height;
	    while(count--) {
		*(dst++) = src[0] | (src[srcPitch] << 16);
		src += srcPitch * 2;
	    }
	    srcPtr += pNv->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}


void
NVRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox)
{
    NVPtr pNv = NVPTR(pScrn);
    int count, width, height, dstPitch, srcPitch;
    CARD32 *dstPtr, *srcPtr, *src, *dst;

    if(!pNv->Rotate) {
       NVRefreshArea(pScrn, num, pbox);
       return;
    }

    dstPitch = pScrn->displayWidth;
    srcPitch = -pNv->Rotate * pNv->ShadowPitch >> 2;

    while(num--) {
	width = pbox->x2 - pbox->x1;
	height = pbox->y2 - pbox->y1;

	if(pNv->Rotate == 1) {
	    dstPtr = (CARD32*)pNv->FB->map + 
			(pbox->x1 * dstPitch) + pScrn->virtualX - pbox->y2;
	    srcPtr = (CARD32*)pNv->ShadowPtr + 
			((1 - pbox->y2) * srcPitch) + pbox->x1;
	} else {
	    dstPtr = (CARD32*)pNv->FB->map + 
			((pScrn->virtualY - pbox->x2) * dstPitch) + pbox->y1;
	    srcPtr = (CARD32*)pNv->ShadowPtr + 
			(pbox->y1 * srcPitch) + pbox->x2 - 1;
	}

	while(width--) {
	    src = srcPtr;
	    dst = dstPtr;
	    count = height;
	    while(count--) {
		*(dst++) = *src;
		src += srcPitch;
	    }
	    srcPtr += pNv->Rotate;
	    dstPtr += dstPitch;
	}

	pbox++;
    }
}



