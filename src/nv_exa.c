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

#include "nv_dma.h"
#include "nv_local.h"

#include <sys/time.h>

const int NVCopyROP[16] =
{
   0x00,            /* GXclear */
   0x88,            /* GXand */
   0x44,            /* GXandReverse */
   0xCC,            /* GXcopy */
   0x22,            /* GXandInverted */
   0xAA,            /* GXnoop */
   0x66,            /* GXxor */
   0xEE,            /* GXor */
   0x11,            /* GXnor */
   0x99,            /* GXequiv */
   0x55,            /* GXinvert*/
   0xDD,            /* GXorReverse */
   0x33,            /* GXcopyInverted */
   0xBB,            /* GXorInverted */
   0x77,            /* GXnand */
   0xFF             /* GXset */
};

static void 
NVSetPattern(ScrnInfoPtr pScrn, CARD32 clr0, CARD32 clr1,
				CARD32 pat0, CARD32 pat1)
{
	NVPtr pNv = NVPTR(pScrn);

	BEGIN_RING(NvImagePattern, NV04_IMAGE_PATTERN_MONOCHROME_COLOR0, 4);
	OUT_RING  (clr0);
	OUT_RING  (clr1);
	OUT_RING  (pat0);
	OUT_RING  (pat1);
}

static void 
NVSetROP(ScrnInfoPtr pScrn, CARD32 alu, CARD32 planemask)
{
	NVPtr pNv = NVPTR(pScrn);
	int rop = NVCopyROP[alu] & 0xf0;

	if (planemask != ~0) {
		NVSetPattern(pScrn, 0, planemask, ~0, ~0);
		if (pNv->currentRop != (alu + 32)) {
			BEGIN_RING(NvRop, NV03_CONTEXT_ROP_ROP, 1);
			OUT_RING  (rop | 0x0a);
			pNv->currentRop = alu + 32;
		}
	} else
	if (pNv->currentRop != alu) {
		if(pNv->currentRop >= 16)
			NVSetPattern(pScrn, ~0, ~0, ~0, ~0);
		BEGIN_RING(NvRop, NV03_CONTEXT_ROP_ROP, 1);
		OUT_RING  (rop | (rop >> 4));
		pNv->currentRop = alu;
	}
}

static CARD32 rectFormat(DrawablePtr pDrawable)
{
	switch(pDrawable->bitsPerPixel) {
	case 32:
	case 24:
		return NV04_GDI_RECTANGLE_TEXT_COLOR_FORMAT_A8R8G8B8;
		break;
	case 16:
		return NV04_GDI_RECTANGLE_TEXT_COLOR_FORMAT_A16R5G6B5;
		break;
	default:
		return NV04_GDI_RECTANGLE_TEXT_COLOR_FORMAT_A8R8G8B8;
		break;
	}
}

/* EXA acceleration hooks */
static void NVExaWaitMarker(ScreenPtr pScreen, int marker)
{
	NVSync(xf86Screens[pScreen->myNum]);
}

static Bool NVExaPrepareSolid(PixmapPtr pPixmap,
			      int   alu,
			      Pixel planemask,
			      Pixel fg)
{
	ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	unsigned int fmt, pitch;

	planemask |= ~0 << pPixmap->drawable.bitsPerPixel;
	if (planemask != ~0 || alu != GXcopy) {
		if (pPixmap->drawable.bitsPerPixel == 32)
			return FALSE;
		BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_OPERATION, 1);
		OUT_RING  (1); /* ROP_AND */
		NVSetROP(pScrn, alu, planemask);
	} else {
		BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_OPERATION, 1);
		OUT_RING  (3); /* SRCCOPY */
	}

	if (!NVAccelGetCtxSurf2DFormatFromPixmap(pPixmap, (int*)&fmt))
		return FALSE;
	pitch = exaGetPixmapPitch(pPixmap);

	/* When SURFACE_FORMAT_A8R8G8B8 is used with GDI_RECTANGLE_TEXT, the 
	 * alpha channel gets forced to 0xFF for some reason.  We're using 
	 * SURFACE_FORMAT_Y32 as a workaround
	 */
	if (fmt == NV04_CONTEXT_SURFACES_2D_FORMAT_A8R8G8B8)
		fmt = NV04_CONTEXT_SURFACES_2D_FORMAT_Y32;

	BEGIN_RING(NvContextSurfaces, NV04_CONTEXT_SURFACES_2D_FORMAT, 4);
	OUT_RING  (fmt);
	OUT_RING  ((pitch << 16) | pitch);
	OUT_PIXMAPl(pPixmap, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_PIXMAPl(pPixmap, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);

	BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_COLOR_FORMAT, 1);
	OUT_RING  (rectFormat(&pPixmap->drawable));
	BEGIN_RING(NvRectangle, NV04_GDI_RECTANGLE_TEXT_COLOR1_A, 1);
	OUT_RING  (fg);

	return TRUE;
}

static void NVExaSolid (PixmapPtr pPixmap, int x1, int y1, int x2, int y2)
{
	ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	int width = x2-x1;
	int height = y2-y1;

	BEGIN_RING(NvRectangle,
		   NV04_GDI_RECTANGLE_TEXT_UNCLIPPED_RECTANGLE_POINT(0), 2);
	OUT_RING  ((x1 << 16) | y1);
	OUT_RING  ((width << 16) | height);

	if((width * height) >= 512)
		FIRE_RING();
}

static void NVExaDoneSolid (PixmapPtr pPixmap)
{
}

static Bool NVExaPrepareCopy(PixmapPtr pSrcPixmap,
			     PixmapPtr pDstPixmap,
			     int       dx,
			     int       dy,
			     int       alu,
			     Pixel     planemask)
{
	ScrnInfoPtr pScrn = xf86Screens[pSrcPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	int fmt;

	if (pSrcPixmap->drawable.bitsPerPixel !=
			pDstPixmap->drawable.bitsPerPixel)
		return FALSE;

	planemask |= ~0 << pDstPixmap->drawable.bitsPerPixel;
	if (planemask != ~0 || alu != GXcopy) {
		if (pDstPixmap->drawable.bitsPerPixel == 32)
			return FALSE;
		BEGIN_RING(NvImageBlit, NV_IMAGE_BLIT_OPERATION, 1);
		OUT_RING  (1); /* ROP_AND */
		NVSetROP(pScrn, alu, planemask);
	} else {
		BEGIN_RING(NvImageBlit, NV_IMAGE_BLIT_OPERATION, 1);
		OUT_RING  (3); /* SRCCOPY */
	}

	if (!NVAccelGetCtxSurf2DFormatFromPixmap(pDstPixmap, &fmt))
		return FALSE;

	BEGIN_RING(NvContextSurfaces, NV04_CONTEXT_SURFACES_2D_FORMAT, 4);
	OUT_RING  (fmt);
	OUT_RING  ((exaGetPixmapPitch(pDstPixmap) << 16) |
		   (exaGetPixmapPitch(pSrcPixmap)));
	OUT_PIXMAPl(pSrcPixmap, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_PIXMAPl(pDstPixmap, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);

	return TRUE;
}

static void NVExaCopy(PixmapPtr pDstPixmap,
		      int	srcX,
		      int	srcY,
		      int	dstX,
		      int	dstY,
		      int	width,
		      int	height)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	/* We want to catch people who have this bug, to find a decent fix */
#if 0
	/* Now check whether we have the same values for srcY and dstY and
	   whether the used chipset is buggy. Currently we flag all of G70
	   cards as buggy, which is probably much to broad. KoalaBR 
	   16 is an abritrary threshold. It should define the maximum number
	   of lines between dstY and srcY  If the number of lines is below
	   we guess, that the bug won't trigger...
	 */
	if ( ((abs(srcY - dstY)< 16)||(abs(srcX-dstX)<16)) &&
		((((pNv->Chipset & 0xfff0) == CHIPSET_G70) ||
		 ((pNv->Chipset & 0xfff0) == CHIPSET_G71) ||
		 ((pNv->Chipset & 0xfff0) == CHIPSET_G72) ||
		 ((pNv->Chipset & 0xfff0) == CHIPSET_G73) ||
		 ((pNv->Chipset & 0xfff0) == CHIPSET_C512))) )
	{
		int dx=abs(srcX - dstX),dy=abs(srcY - dstY);
		// Ok, let's do it manually unless someone comes up with a better idea
		// 1. If dstY and srcY are really the same, do a copy rowwise
		if (dy<dx) {
			int i,xpos,inc;
			NVDEBUG("ExaCopy: Lines identical:\n");
			if (srcX>=dstX) {
				xpos=0;
				inc=1;
			} else {
				xpos=width-1;
				inc=-1;
			}
			for (i = 0; i < width; i++) {
				BEGIN_RING(NvImageBlit,
					   NV_IMAGE_BLIT_POINT_IN, 3);
				OUT_RING  ((srcY << 16) | (srcX+xpos));
				OUT_RING  ((dstY << 16) | (dstX+xpos));
				OUT_RING  ((height  << 16) | 1);
				xpos+=inc;
			}
		} else {
			// 2. Otherwise we will try a line by line copy in the hope to avoid
			//    the card's bug.
			int i,ypos,inc;
			NVDEBUG("ExaCopy: Lines nearly the same srcY=%d, dstY=%d:\n", srcY, dstY);
			if (srcY>=dstY) {
				ypos=0;
				inc=1;
			} else {
				ypos=height-1;
				inc=-1;
			}
			for (i = 0; i < height; i++) {
				BEGIN_RING(NvImageBlit,
					   NV_IMAGE_BLIT_POINT_IN, 3);
				OUT_RING  (((srcY+ypos) << 16) | srcX);
				OUT_RING  (((dstY+ypos) << 16) | dstX);
				OUT_RING  ((1  << 16) | width);
				ypos+=inc;
			}
		} 
	} else {
		NVDEBUG("ExaCopy: Using default path\n");
		BEGIN_RING(NvImageBlit, NV_IMAGE_BLIT_POINT_IN, 3);
		OUT_RING  ((srcY << 16) | srcX);
		OUT_RING  ((dstY << 16) | dstX);
		OUT_RING  ((height  << 16) | width);
	}
#endif /* 0 */

	NVDEBUG("ExaCopy: Using default path\n");
	BEGIN_RING(NvImageBlit, NV_IMAGE_BLIT_POINT_IN, 3);
	OUT_RING  ((srcY << 16) | srcX);
	OUT_RING  ((dstY << 16) | dstX);
	OUT_RING  ((height  << 16) | width);

	if((width * height) >= 512)
		FIRE_RING(); 
}

static void NVExaDoneCopy (PixmapPtr pDstPixmap) {}

static inline Bool NVAccelMemcpyRect(char *dst, const char *src, int height,
		       int dst_pitch, int src_pitch, int line_len)
{
	if ((src_pitch == line_len) && (src_pitch == dst_pitch)) {
		memcpy(dst, src, line_len*height);
	} else {
		while (height--) {
			memcpy(dst, src, line_len);
			src += src_pitch;
			dst += dst_pitch;
		}
	}

	return TRUE;
}

static inline Bool
NVAccelDownloadM2MF(ScrnInfoPtr pScrn, char *dst, PixmapPtr pspix,
		    uint32_t src_offset, int dst_pitch, int src_pitch,
		    int line_len, int line_count)
{
	NVPtr pNv = NVPTR(pScrn);

	BEGIN_RING(NvMemFormat, 0x184, 2);
	OUT_PIXMAPo(pspix, NOUVEAU_BO_GART | NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_RELOCo(pNv->GART, NOUVEAU_BO_GART | NOUVEAU_BO_WR);

	while (line_count) {
		char *src = pNv->GART->map;
		int lc, i;

		if (line_count * line_len <= pNv->GART->size) {
			lc = line_count;
		} else {
			lc = pNv->GART->size / line_len;
			if (lc > line_count)
				lc = line_count;
		}

		/* HW limitations */
		if (lc > 2047)
			lc = 2047;

		if (pNv->Architecture >= NV_ARCH_50) {
			BEGIN_RING(NvMemFormat, 0x200, 1);
			OUT_RING  (1);
			BEGIN_RING(NvMemFormat, 0x21c, 1);
			OUT_RING  (1);
			/* probably high-order bits of address */
			BEGIN_RING(NvMemFormat, 0x238, 2);
			OUT_PIXMAPh(pspix, src_offset, NOUVEAU_BO_GART |
				    NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
			OUT_RELOCh(pNv->GART, 0, NOUVEAU_BO_GART |
				   NOUVEAU_BO_WR);
		}

		BEGIN_RING(NvMemFormat,
			   NV_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
		OUT_PIXMAPl(pspix, src_offset, NOUVEAU_BO_GART |
			    NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
		OUT_RELOCl(pNv->GART, 0, NOUVEAU_BO_GART | NOUVEAU_BO_WR);
		OUT_RING  (src_pitch);
		OUT_RING  (line_len);
		OUT_RING  (line_len);
		OUT_RING  (lc);
		OUT_RING  ((1<<8)|1);
		OUT_RING  (0);

		nouveau_notifier_reset(pNv->notify0, 0);
		BEGIN_RING(NvMemFormat, NV_MEMORY_TO_MEMORY_FORMAT_NOTIFY, 1);
		OUT_RING  (0);
		BEGIN_RING(NvMemFormat, 0x100, 1);
		OUT_RING  (0);
		FIRE_RING();
		if (nouveau_notifier_wait_status(pNv->notify0, 0, 0,
						 2000))
			return FALSE;

		if (dst_pitch == line_len) {
			memcpy(dst, src, dst_pitch * lc);
			dst += dst_pitch * lc;
		} else {
			for (i = 0; i < lc; i++) {
				memcpy(dst, src, line_len);
				src += line_len;
				dst += dst_pitch;
			}
		}

		line_count -= lc;
		src_offset += lc * src_pitch;
	}

	return TRUE;
}

static inline void *
NVExaPixmapMap(PixmapPtr pPix)
{
	ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	void *map;

	map = pNv->FB->map + exaGetPixmapOffset(pPix);
	return map;
}

static Bool NVDownloadFromScreen(PixmapPtr pSrc,
				 int x,  int y,
				 int w,  int h,
				 char *dst,  int dst_pitch)
{
	ScrnInfoPtr pScrn = xf86Screens[pSrc->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	int src_pitch, cpp, offset;
	const char *src;

	src_pitch  = exaGetPixmapPitch(pSrc);
	cpp = pSrc->drawable.bitsPerPixel >> 3;
	offset = (y * src_pitch) + (x * cpp);

	if (pNv->GART) {
		if (NVAccelDownloadM2MF(pScrn, dst, pSrc, offset,
					dst_pitch, src_pitch, w * cpp, h))
			return TRUE;
	}

	src = NVExaPixmapMap(pSrc) + offset;
	exaWaitSync(pSrc->drawable.pScreen);
	if (NVAccelMemcpyRect(dst, src, h, dst_pitch, src_pitch, w*cpp))
		return TRUE;

	return FALSE;
}

static inline Bool
NVAccelUploadIFC(ScrnInfoPtr pScrn, const char *src, int src_pitch, 
		 PixmapPtr pDst, int x, int y, int w, int h, int cpp)
{
	NVPtr pNv = NVPTR(pScrn);
	int line_len = w * cpp;
	int iw, id, surf_fmt, ifc_fmt;

	if (pNv->Architecture >= NV_ARCH_50)
		return FALSE;

	if (h > 1024)
		return FALSE;

	switch (cpp) {
	case 2: ifc_fmt = 1; break;
	case 4: ifc_fmt = 4; break;
	default:
		return FALSE;
	}

	if (NVAccelGetCtxSurf2DFormatFromPixmap(pDst, &surf_fmt))
		return FALSE;

	BEGIN_RING(NvContextSurfaces, NV04_CONTEXT_SURFACES_2D_FORMAT, 4);
	OUT_RING  (surf_fmt);
	OUT_RING  ((exaGetPixmapPitch(pDst) << 16) | exaGetPixmapPitch(pDst));
	OUT_PIXMAPl(pDst, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_PIXMAPl(pDst, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);

	/* Pad out input width to cover both COLORA() and COLORB() */
	iw  = (line_len + 7) & ~7;
	id  = iw / 4; /* line push size */
	iw /= cpp;

	/* Don't support lines longer than max push size yet.. */
	if (id > 1792)
		return FALSE;

	BEGIN_RING(NvClipRectangle, NV01_CONTEXT_CLIP_RECTANGLE_POINT, 2);
	OUT_RING  (0x0); 
	OUT_RING  (0x7FFF7FFF);

	BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_OPERATION, 2);
	OUT_RING  (NV01_IMAGE_FROM_CPU_OPERATION_SRCCOPY);
	OUT_RING  (ifc_fmt);
	BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_POINT, 3);
	OUT_RING  ((y << 16) | x); /* dst point */
	OUT_RING  ((h << 16) | w); /* width/height out */
	OUT_RING  ((h << 16) | iw); /* width/height in */

	while (h--) {
		/* send a line */
		BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_COLOR(0), id);
		OUT_RINGp (src, id);

		src += src_pitch;
	}

	return TRUE;
}

static inline Bool
NVAccelUploadM2MF(ScrnInfoPtr pScrn, PixmapPtr pdpix, uint32_t dst_offset,
		  const char *src, int dst_pitch, int src_pitch,
		  int line_len, int line_count)
{
	NVPtr pNv = NVPTR(pScrn);

	BEGIN_RING(NvMemFormat, 0x184, 2);
	OUT_RELOCo(pNv->GART, NOUVEAU_BO_GART | NOUVEAU_BO_RD);
	OUT_PIXMAPo(pdpix, NOUVEAU_BO_VRAM | NOUVEAU_BO_GART | NOUVEAU_BO_WR);

	while (line_count) {
		char *dst = pNv->GART->map;
		int lc, i;

		/* Determine max amount of data we can DMA at once */
		if (line_count * line_len <= pNv->GART->size) {
			lc = line_count;
		} else {
			lc = pNv->GART->size / line_len;
			if (lc > line_count)
				lc = line_count;
		}

		/* HW limitations */
		if (lc > 2047)
			lc = 2047;

		/* Upload to GART */
		if (src_pitch == line_len) {
			memcpy(dst, src, src_pitch * lc);
			src += src_pitch * lc;
		} else {
			for (i = 0; i < lc; i++) {
				memcpy(dst, src, line_len);
				src += src_pitch;
				dst += line_len;
			}
		}

		if (pNv->Architecture >= NV_ARCH_50) {
			BEGIN_RING(NvMemFormat, 0x200, 1);
			OUT_RING  (1);
			BEGIN_RING(NvMemFormat, 0x21c, 1);
			OUT_RING  (1);
			/* probably high-order bits of address */
			BEGIN_RING(NvMemFormat, 0x238, 2);
			OUT_RELOCh(pNv->GART, 0, NOUVEAU_BO_GART |
				   NOUVEAU_BO_RD);
			OUT_PIXMAPh(pdpix, 0, NOUVEAU_BO_VRAM | 
				    NOUVEAU_BO_GART | NOUVEAU_BO_WR);
		}

		/* DMA to VRAM */
		BEGIN_RING(NvMemFormat,
			   NV_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
		OUT_RELOCl(pNv->GART, 0, NOUVEAU_BO_GART | NOUVEAU_BO_RD);
		OUT_PIXMAPl(pdpix, dst_offset, NOUVEAU_BO_VRAM |
			    NOUVEAU_BO_GART | NOUVEAU_BO_WR);
		OUT_RING  (line_len);
		OUT_RING  (dst_pitch);
		OUT_RING  (line_len);
		OUT_RING  (lc);
		OUT_RING  ((1<<8)|1);
		OUT_RING  (0);

		nouveau_notifier_reset(pNv->notify0, 0);
		BEGIN_RING(NvMemFormat, NV_MEMORY_TO_MEMORY_FORMAT_NOTIFY, 1);
		OUT_RING  (0);
		BEGIN_RING(NvMemFormat, 0x100, 1);
		OUT_RING  (0);
		FIRE_RING();
		if (nouveau_notifier_wait_status(pNv->notify0, 0, 0, 2000))
			return FALSE;

		dst_offset += lc * dst_pitch;
		line_count -= lc;
	}

	return TRUE;
}

static Bool NVUploadToScreen(PixmapPtr pDst,
			     int x, int y, int w, int h,
			     char *src, int src_pitch)
{
	ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	int dst_pitch, cpp;
	char *dst;

	dst_pitch  = exaGetPixmapPitch(pDst);
	cpp = pDst->drawable.bitsPerPixel >> 3;

	/* try hostdata transfer */
	if (pNv->Architecture < NV_ARCH_50 && w*h*cpp<16*1024) /* heuristic */
	{
		if (NVAccelUploadIFC(pScrn, src, src_pitch, pDst,
				     x, y, w, h, cpp)) {
			exaMarkSync(pDst->drawable.pScreen);
			return TRUE;
		}
	}

	/* try gart-based transfer */
	if (pNv->GART) {
		if (NVAccelUploadM2MF(pScrn, pDst, (y * dst_pitch) + (x * cpp),
				      src, dst_pitch, src_pitch, w * cpp, h))
			return TRUE;
	}

	/* fallback to memcpy-based transfer */
	dst = NVExaPixmapMap(pDst) + (y * dst_pitch) + (x * cpp);
	exaWaitSync(pDst->drawable.pScreen);
	if (NVAccelMemcpyRect(dst, src, h, dst_pitch, src_pitch, w*cpp))
		return TRUE;

	return FALSE;
}

Bool
NVExaInit(ScreenPtr pScreen) 
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	if(!(pNv->EXADriverPtr = (ExaDriverPtr) xnfcalloc(sizeof(ExaDriverRec), 1))) {
		pNv->NoAccel = TRUE;
		return FALSE;
	}

	pNv->EXADriverPtr->exa_major = EXA_VERSION_MAJOR;
	pNv->EXADriverPtr->exa_minor = EXA_VERSION_MINOR;

	pNv->EXADriverPtr->memoryBase		= pNv->FB->map;
	pNv->EXADriverPtr->offScreenBase	=
		pScrn->virtualX * pScrn->virtualY*(pScrn->bitsPerPixel/8); 
	pNv->EXADriverPtr->memorySize		= pNv->FB->size; 
	pNv->EXADriverPtr->pixmapOffsetAlign	= 256; 
	pNv->EXADriverPtr->pixmapPitchAlign	= 64; 
	pNv->EXADriverPtr->flags		= EXA_OFFSCREEN_PIXMAPS;
	pNv->EXADriverPtr->maxX			= 32768;
	pNv->EXADriverPtr->maxY			= 32768;

	pNv->EXADriverPtr->WaitMarker = NVExaWaitMarker;

	/* Install default hooks */
	pNv->EXADriverPtr->DownloadFromScreen = NVDownloadFromScreen; 
	pNv->EXADriverPtr->UploadToScreen = NVUploadToScreen; 

	if (pNv->Architecture < NV_ARCH_50) {
		pNv->EXADriverPtr->PrepareCopy = NVExaPrepareCopy;
		pNv->EXADriverPtr->Copy = NVExaCopy;
		pNv->EXADriverPtr->DoneCopy = NVExaDoneCopy;

		pNv->EXADriverPtr->PrepareSolid = NVExaPrepareSolid;
		pNv->EXADriverPtr->Solid = NVExaSolid;
		pNv->EXADriverPtr->DoneSolid = NVExaDoneSolid;
	} else {
		pNv->EXADriverPtr->PrepareCopy = NV50EXAPrepareCopy;
		pNv->EXADriverPtr->Copy = NV50EXACopy;
		pNv->EXADriverPtr->DoneCopy = NV50EXADoneCopy;

		pNv->EXADriverPtr->PrepareSolid = NV50EXAPrepareSolid;
		pNv->EXADriverPtr->Solid = NV50EXASolid;
		pNv->EXADriverPtr->DoneSolid = NV50EXADoneSolid;
	}

	switch (pNv->Architecture) {
	
	case NV_ARCH_10:
	case NV_ARCH_20:
 		pNv->EXADriverPtr->CheckComposite   = NV10CheckComposite;
 		pNv->EXADriverPtr->PrepareComposite = NV10PrepareComposite;
 		pNv->EXADriverPtr->Composite        = NV10Composite;
 		pNv->EXADriverPtr->DoneComposite    = NV10DoneComposite;
		break;
	case NV_ARCH_30:
		pNv->EXADriverPtr->CheckComposite   = NV30EXACheckComposite;
		pNv->EXADriverPtr->PrepareComposite = NV30EXAPrepareComposite;
		pNv->EXADriverPtr->Composite        = NV30EXAComposite;
		pNv->EXADriverPtr->DoneComposite    = NV30EXADoneComposite;
		break;
	case NV_ARCH_40:
		pNv->EXADriverPtr->CheckComposite   = NV40EXACheckComposite;
		pNv->EXADriverPtr->PrepareComposite = NV40EXAPrepareComposite;
		pNv->EXADriverPtr->Composite        = NV40EXAComposite;
		pNv->EXADriverPtr->DoneComposite    = NV40EXADoneComposite;
		break;
	case NV_ARCH_50:
		break;
	default:
		break;
	}

	return exaDriverInit(pScreen, pNv->EXADriverPtr);
}

