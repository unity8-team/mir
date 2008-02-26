/*
 * Copyright 2008 Alex Deucher
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 * Based on radeon_exa_render.c and kdrive ati_video.c by Eric Anholt, et al.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_video.h"

#include <X11/extensions/Xv.h>
#include "fourcc.h"

#define IMAGE_MAX_WIDTH		2048
#define IMAGE_MAX_HEIGHT	2048

static Bool
RADEONTilingEnabled(ScrnInfoPtr pScrn, PixmapPtr pPix)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);

#ifdef USE_EXA
    if (info->useEXA) {
	if (info->tilingEnabled && exaGetPixmapOffset(pPix) == 0)
	    return TRUE;
	else
	    return FALSE;
    } else
#endif
	{
	    if (info->tilingEnabled)
		return TRUE;
	    else
		return FALSE;
	}
}

static __inline__ CARD32 F_TO_DW(float val)
{
    union {
	float f;
	CARD32 l;
    } tmp;
    tmp.f = val;
    return tmp.l;
}

#define ACCEL_MMIO
#define VIDEO_PREAMBLE()	unsigned char *RADEONMMIO = info->MMIO
#define BEGIN_VIDEO(n)		RADEONWaitForFifo(pScrn, (n))
#define OUT_VIDEO_REG(reg, val)	OUTREG(reg, val)
#define OUT_VIDEO_REG_F(reg, val) OUTREG(reg, F_TO_DW(val))
#define FINISH_VIDEO()

#include "radeon_textured_videofuncs.c"

#undef ACCEL_MMIO
#undef VIDEO_PREAMBLE
#undef BEGIN_VIDEO
#undef OUT_VIDEO_REG
#undef FINISH_VIDEO

#ifdef XF86DRI

#define ACCEL_CP
#define VIDEO_PREAMBLE()						\
    RING_LOCALS;							\
    RADEONCP_REFRESH(pScrn, info)
#define BEGIN_VIDEO(n)		BEGIN_RING(2*(n))
#define OUT_VIDEO_REG(reg, val)	OUT_RING_REG(reg, val)
#define FINISH_VIDEO()		ADVANCE_RING()
#define OUT_VIDEO_RING_F(x) OUT_RING(F_TO_DW(x))

#include "radeon_textured_videofuncs.c"

#endif /* XF86DRI */

static void
RADEONXVCopyPlanarData(CARD8 *src, CARD8 *dst, int randr,
		       int srcPitch, int srcPitch2, int dstPitch,
		       int srcW, int srcH, int height,
		       int top, int left, int h, int w, int id)
{
    int i, j;
    CARD8 *src1, *src2, *src3, *dst1;
    int srcDown = srcPitch, srcDown2 = srcPitch2;
    int srcRight = 2, srcRight2 = 1, srcNext = 1;

    /* compute source data pointers */
    src1 = src;
    src2 = src1 + height * srcPitch;
    src3 = src2 + (height >> 1) * srcPitch2;
    switch (randr) {
    case RR_Rotate_0:
	srcDown = srcPitch;
	srcDown2 = srcPitch2;
	srcRight = 2;
	srcRight2 = 1;
	srcNext = 1;
	break;
    case RR_Rotate_90:
	src1 = src1 + srcH - 1;
	src2 = src2 + (srcH >> 1) - 1;
	src3 = src3 + (srcH >> 1) - 1;
	srcDown = -1;
	srcDown2 = -1;
	srcRight = srcPitch * 2;
	srcRight2 = srcPitch2;
	srcNext = srcPitch;
	break;
    case RR_Rotate_180:
	src1 = src1 + srcPitch * (srcH - 1) + (srcW - 1);
	src2 = src2 + srcPitch2 * ((srcH >> 1) - 1) + ((srcW >> 1) - 1);
	src3 = src3 + srcPitch2 * ((srcH >> 1) - 1) + ((srcW >> 1) - 1);
	srcDown = -srcPitch;
	srcDown2 = -srcPitch2;
	srcRight = -2;
	srcRight2 = -1;
	srcNext = -1;
	break;
    case RR_Rotate_270:
	src1 = src1 + srcPitch * (srcW - 1);
	src2 = src2 + srcPitch2 * ((srcW >> 1) - 1);
	src3 = src3 + srcPitch2 * ((srcW >> 1) - 1);
	srcDown = 1;
	srcDown2 = 1;
	srcRight = -srcPitch * 2;
	srcRight2 = -srcPitch2;
	srcNext = -srcPitch;
	break;
    }

    /* adjust for origin */
    src1 += top * srcDown + left * srcNext;
    src2 += (top >> 1) * srcDown2 + (left >> 1) * srcRight2;
    src3 += (top >> 1) * srcDown2 + (left >> 1) * srcRight2;

    if (id == FOURCC_I420) {
	CARD8 *srct = src2;
	src2 = src3;
	src3 = srct;
    }

    dst1 = dst;

    w >>= 1;
    for (j = 0; j < h; j++) {
	CARD32 *dst = (CARD32 *)dst1;
	CARD8 *s1l = src1;
	CARD8 *s1r = src1 + srcNext;
	CARD8 *s2 = src2;
	CARD8 *s3 = src3;

	for (i = 0; i < w; i++) {
	    *dst++ = *s1l | (*s1r << 16) | (*s3 << 8) | (*s2 << 24);
	    s1l += srcRight;
	    s1r += srcRight;
	    s2 += srcRight2;
	    s3 += srcRight2;
	}
	src1 += srcDown;
	dst1 += dstPitch;
	if (j & 1) {
	    src2 += srcDown2;
	    src3 += srcDown2;
	}
    }
}

static void
RADEONXVCopyPackedData(CARD8 *src, CARD8 *dst, int randr,
		       int srcPitch, int dstPitch,
		       int srcW, int srcH, int top, int left,
		       int h, int w)
{
    int srcDown = srcPitch, srcRight = 2, srcNext;
    int p;

    switch (randr) {
    case RR_Rotate_0:
	srcDown = srcPitch;
	srcRight = 2;
	break;
    case RR_Rotate_90:
	src += (srcH - 1) * 2;
	srcDown = -2;
	srcRight = srcPitch;
	break;
    case RR_Rotate_180:
	src += srcPitch * (srcH - 1) + (srcW - 1) * 2;
	srcDown = -srcPitch;
	srcRight = -2;
	break;
    case RR_Rotate_270:
	src += srcPitch * (srcW - 1);
	srcDown = 2;
	srcRight = -srcPitch;
	break;
    }

    src = src + top * srcDown + left * srcRight;

    w >>= 1;
    /* srcRight >>= 1; */
    srcNext = srcRight >> 1;
    while (h--) {
	CARD16 *s = (CARD16 *)src;
	CARD32 *d = (CARD32 *)dst;
	p = w;
	while (p--) {
	    *d++ = s[0] | (s[srcNext] << 16);
	    s += srcRight;
	}
	src += srcPitch;
	dst += dstPitch;
    }
}

static int
RADEONPutImageTextured(ScrnInfoPtr pScrn,
		       short src_x, short src_y,
		       short drw_x, short drw_y,
		       short src_w, short src_h,
		       short drw_w, short drw_h,
		       int id,
		       unsigned char *buf,
		       short width,
		       short height,
		       Bool sync,
		       RegionPtr clipBoxes,
		       pointer data,
		       DrawablePtr pDraw)
{
    ScreenPtr pScreen = pScrn->pScreen;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONPortPrivPtr pPriv = (RADEONPortPrivPtr)data;
    INT32 x1, x2, y1, y2;
    int randr = RR_Rotate_0 /* XXX */;
    int srcPitch, srcPitch2, dstPitch;
    int top, left, npixels, nlines, size;
    BoxRec dstBox;
    int dst_width = width, dst_height = height;
    int rot_x1, rot_y1, rot_x2, rot_y2;
    int dst_x1, dst_y1, dst_x2, dst_y2;
    int rot_src_w, rot_src_h, rot_drw_w, rot_drw_h;

    /* Clip */
    x1 = src_x;
    x2 = src_x + src_w;
    y1 = src_y;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    if (!xf86XVClipVideoHelper(&dstBox, &x1, &x2, &y1, &y2, clipBoxes, width, height))
	return Success;

    src_w = (x2 - x1) >> 16;
    src_h = (y2 - y1) >> 16;
    drw_w = dstBox.x2 - dstBox.x1;
    drw_h = dstBox.y2 - dstBox.y1;

    if ((x1 >= x2) || (y1 >= y2))
	return Success;

    if (randr & (RR_Rotate_0|RR_Rotate_180)) {
	dst_width = width;
	dst_height = height;
	rot_src_w = src_w;
	rot_src_h = src_h;
	rot_drw_w = drw_w;
	rot_drw_h = drw_h;
    } else {
	dst_width = height;
	dst_height = width;
	rot_src_w = src_h;
	rot_src_h = src_w;
	rot_drw_w = drw_h;
	rot_drw_h = drw_w;
    }

    switch (randr) {
    case RR_Rotate_0:
    default:
	dst_x1 = dstBox.x1;
	dst_y1 = dstBox.y1;
	dst_x2 = dstBox.x2;
	dst_y2 = dstBox.y2;
	rot_x1 = x1;
	rot_y1 = y1;
	rot_x2 = x2;
	rot_y2 = y2;
	break;
    case RR_Rotate_90:
	dst_x1 = dstBox.y1;
	dst_y1 = pScrn->virtualY - dstBox.x2;
	dst_x2 = dstBox.y2;
	dst_y2 = pScrn->virtualY - dstBox.x1;
	rot_x1 = y1;
	rot_y1 = (src_w << 16) - x2;
	rot_x2 = y2;
	rot_y2 = (src_w << 16) - x1;
	break;
    case RR_Rotate_180:
	dst_x1 = pScrn->virtualX - dstBox.x2;
	dst_y1 = pScrn->virtualY - dstBox.y2;
	dst_x2 = pScrn->virtualX - dstBox.x1;
	dst_y2 = pScrn->virtualY - dstBox.y1;
	rot_x1 = (src_w << 16) - x2;
	rot_y1 = (src_h << 16) - y2;
	rot_x2 = (src_w << 16) - x1;
	rot_y2 = (src_h << 16) - y1;
	break;
    case RR_Rotate_270:
	dst_x1 = pScrn->virtualX - dstBox.y2;
	dst_y1 = dstBox.x1;
	dst_x2 = pScrn->virtualX - dstBox.y1;
	dst_y2 = dstBox.x2;
	rot_x1 = (src_h << 16) - y2;
	rot_y1 = x1;
	rot_x2 = (src_h << 16) - y1;
	rot_y2 = x2;
	break;
    }

    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	dstPitch = ((dst_width << 1) + 15) & ~15;
	srcPitch = (width + 3) & ~3;
	srcPitch2 = ((width >> 1) + 3) & ~3;
	size = dstPitch * dst_height;
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	dstPitch = ((dst_width << 1) + 15) & ~15;
	srcPitch = (width << 1);
	srcPitch2 = 0;
	size = dstPitch * dst_height;
	break;
    }

    if (pPriv->video_memory != NULL && size != pPriv->size) {
	RADEONFreeMemory(pScrn, pPriv->video_memory);
	pPriv->video_memory = NULL;
    }

    if (pPriv->video_memory == NULL) {
	pPriv->video_offset = RADEONAllocateMemory(pScrn,
						       &pPriv->video_memory,
						       size * 2);
	if (pPriv->video_offset == 0)
	    return BadAlloc;
    }

    if (pDraw->type == DRAWABLE_WINDOW)
	pPriv->pPixmap = (*pScreen->GetWindowPixmap)((WindowPtr)pDraw);
    else
	pPriv->pPixmap = (PixmapPtr)pDraw;

#ifdef USE_EXA
    if (info->useEXA) {
	/* Force the pixmap into framebuffer so we can draw to it. */
	exaMoveInPixmap(pPriv->pPixmap);
    }
#endif

    if (!info->useEXA &&
	(((char *)pPriv->pPixmap->devPrivate.ptr < (char *)info->FB) ||
	 ((char *)pPriv->pPixmap->devPrivate.ptr >= (char *)info->FB +
	  info->FbMapSize))) {
	/* If the pixmap wasn't in framebuffer, then we have no way in XAA to
	 * force it there. So, we simply refuse to draw and fail.
	 */
	return BadAlloc;
    }

    pPriv->src_offset = pPriv->video_offset + info->fbLocation;
    pPriv->src_addr = (CARD8 *)(info->FB + pPriv->video_offset);
    pPriv->src_pitch = dstPitch;
    pPriv->size = size;
    pPriv->pDraw = pDraw;

#if 0
    ErrorF("src_offset: 0x%x\n", pPriv->src_offset);
    ErrorF("src_addr: 0x%x\n", pPriv->src_addr);
    ErrorF("src_pitch: 0x%x\n", pPriv->src_pitch);
#endif

    /* copy data */
    top = rot_y1 >> 16;
    left = (rot_x1 >> 16) & ~1;
    npixels = ((((rot_x2 + 0xffff) >> 16) + 1) & ~1) - left;

    /* Since we're probably overwriting the area that might still be used
     * for the last PutImage request, wait for idle.
     */
#ifdef XF86DRI
    if (info->directRenderingEnabled)
	RADEONWaitForIdleCP(pScrn);
    else
#endif
	RADEONWaitForIdleMMIO(pScrn);


    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	top &= ~1;
	nlines = ((((rot_y2 + 0xffff) >> 16) + 1) & ~1) - top;
	RADEONXVCopyPlanarData(buf, pPriv->src_addr, randr,
			       srcPitch, srcPitch2, dstPitch, rot_src_w, rot_src_h,
			       height, top, left, nlines, npixels, id);
	break;
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	nlines = ((rot_y2 + 0xffff) >> 16) - top;
	RADEONXVCopyPackedData(buf, pPriv->src_addr, randr,
			       srcPitch, dstPitch, rot_src_w, rot_src_h, top, left,
			       nlines, npixels);
	break;
    }

    /* update cliplist */
    if (!REGION_EQUAL(pScrn->pScreen, &pPriv->clip, clipBoxes)) {
	REGION_COPY(pScrn->pScreen, &pPriv->clip, clipBoxes);
    }

    pPriv->id = id;
    pPriv->src_x1 = rot_x1;
    pPriv->src_y1 = rot_y1;
    pPriv->src_x2 = rot_x2;
    pPriv->src_y2 = rot_y2;
    pPriv->src_w = rot_src_w;
    pPriv->src_h = rot_src_h;
    pPriv->dst_x1 = dst_x1;
    pPriv->dst_y1 = dst_y1;
    pPriv->dst_x2 = dst_x2;
    pPriv->dst_y2 = dst_y2;
    pPriv->dst_w = rot_drw_w;
    pPriv->dst_h = rot_drw_h;

#ifdef XF86DRI
    if (info->directRenderingEnabled)
	RADEONDisplayTexturedVideoCP(pScrn, pPriv);
    else
#endif
	RADEONDisplayTexturedVideoMMIO(pScrn, pPriv);

    return Success;
}

/* client libraries expect an encoding */
static XF86VideoEncodingRec DummyEncoding[1] =
{
    {
	0,
	"XV_IMAGE",
	IMAGE_MAX_WIDTH, IMAGE_MAX_HEIGHT,
	{1, 1}
    }
};

#define NUM_FORMATS 3

static XF86VideoFormatRec Formats[NUM_FORMATS] =
{
    {15, TrueColor}, {16, TrueColor}, {24, TrueColor}
};

#define NUM_ATTRIBUTES 0

static XF86AttributeRec Attributes[NUM_ATTRIBUTES] =
{
};

#define NUM_IMAGES 4

static XF86ImageRec Images[NUM_IMAGES] =
{
    XVIMAGE_YUY2,
    XVIMAGE_YV12,
    XVIMAGE_I420,
    XVIMAGE_UYVY
};

XF86VideoAdaptorPtr
RADEONSetupImageTexturedVideo(ScreenPtr pScreen)
{
    RADEONPortPrivPtr pPortPriv;
    XF86VideoAdaptorPtr adapt;
    int i;
    int num_texture_ports = 16;

    adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) + num_texture_ports *
		    (sizeof(RADEONPortPrivRec) + sizeof(DevUnion)));
    if (adapt == NULL)
	return NULL;

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = 0;
    adapt->name = "Radeon Textured Video";
    adapt->nEncodings = 1;
    adapt->pEncodings = DummyEncoding;
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = Formats;
    adapt->nPorts = num_texture_ports;
    adapt->pPortPrivates = (DevUnion*)(&adapt[1]);

    pPortPriv =
	(RADEONPortPrivPtr)(&adapt->pPortPrivates[num_texture_ports]);

    adapt->nAttributes = NUM_ATTRIBUTES;
    adapt->pAttributes = Attributes;
    adapt->pImages = Images;
    adapt->nImages = NUM_IMAGES;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = RADEONStopVideo;
    adapt->SetPortAttribute = RADEONSetPortAttribute;
    adapt->GetPortAttribute = RADEONGetPortAttribute;
    adapt->QueryBestSize = RADEONQueryBestSize;
    adapt->PutImage = RADEONPutImageTextured;
    adapt->ReputImage = NULL;
    adapt->QueryImageAttributes = RADEONQueryImageAttributes;

    for (i = 0; i < num_texture_ports; i++) {
	RADEONPortPrivPtr pPriv = &pPortPriv[i];

	pPriv->textured = TRUE;
	pPriv->videoStatus = 0;
	pPriv->currentBuffer = 0;
	pPriv->doubleBuffer = 0;

	/* gotta uninit this someplace, XXX: shouldn't be necessary for textured */
	REGION_NULL(pScreen, &pPriv->clip);
	adapt->pPortPrivates[i].ptr = (pointer) (pPriv);
    }

    return adapt;
}

