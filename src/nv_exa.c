/*
 * Copyright 2003 NVIDIA, Corporation
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
		BEGIN_RING(NvImageBlit, NV04_IMAGE_BLIT_OPERATION, 1);
		OUT_RING  (1); /* ROP_AND */
		NVSetROP(pScrn, alu, planemask);
	} else {
		BEGIN_RING(NvImageBlit, NV04_IMAGE_BLIT_OPERATION, 1);
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
	BEGIN_RING(NvImageBlit, NV01_IMAGE_BLIT_POINT_IN, 3);
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
NVAccelDownloadM2MF(PixmapPtr pspix, int x, int y, int w, int h,
		    char *dst, unsigned dst_pitch)
{
	ScrnInfoPtr pScrn = xf86Screens[pspix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	unsigned cpp = pspix->drawable.bitsPerPixel / 8;
	unsigned line_len = w * cpp;
	unsigned line_count = h;
	unsigned src_pitch = 0, src_offset = 0, linear = 0;

	BEGIN_RING(NvMemFormat, 0x184, 2);
	OUT_PIXMAPo(pspix, NOUVEAU_BO_GART | NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_RELOCo(pNv->GART, NOUVEAU_BO_GART | NOUVEAU_BO_WR);

	if (pNv->Architecture < NV_ARCH_50 ||
	    exaGetPixmapOffset(pspix) < pNv->EXADriverPtr->offScreenBase) {
		linear     = 1;
		src_pitch  = exaGetPixmapPitch(pspix);
		src_offset = (y * src_pitch) + (x * cpp);
	}

	if (pNv->Architecture >= NV_ARCH_50) {
		if (linear) {
			BEGIN_RING(NvMemFormat, 0x0200, 1);
			OUT_RING  (1);
		} else {
			BEGIN_RING(NvMemFormat, 0x0200, 6);
			OUT_RING  (0);
			OUT_RING  (0);
			OUT_RING  (exaGetPixmapPitch(pspix));
			OUT_RING  (pspix->drawable.height);
			OUT_RING  (1);
			OUT_RING  (0);
		}

		BEGIN_RING(NvMemFormat, 0x021c, 1);
		OUT_RING  (1);
	}

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
			if (!linear) {
				BEGIN_RING(NvMemFormat, 0x0218, 1);
				OUT_RING  ((y << 16) | (x * cpp));
			}

			BEGIN_RING(NvMemFormat, 0x238, 2);
			OUT_PIXMAPh(pspix, src_offset, NOUVEAU_BO_GART |
				    NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
			OUT_RELOCh(pNv->GART, 0, NOUVEAU_BO_GART |
				   NOUVEAU_BO_WR);
		}

		BEGIN_RING(NvMemFormat,
			   NV04_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
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
		BEGIN_RING(NvMemFormat, NV04_MEMORY_TO_MEMORY_FORMAT_NOTIFY, 1);
		OUT_RING  (0);
		BEGIN_RING(NvMemFormat, 0x100, 1);
		OUT_RING  (0);
		FIRE_RING();
		if (nouveau_notifier_wait_status(pNv->notify0, 0, 0, 2000))
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

		if (linear)
			src_offset += lc * src_pitch;
		line_count -= lc;
		y += lc;
	}

	return TRUE;
}

static inline void *
NVExaPixmapMap(PixmapPtr pPix)
{
	void *map;
#if NOUVEAU_EXA_PIXMAPS
	struct nouveau_pixmap *nvpix;

	nvpix = exaGetPixmapDriverPrivate(pPix);
	if (!nvpix || !nvpix->bo)
		return NULL;

	map = nvpix->bo->map;
#else
	ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	map = pNv->FB->map + exaGetPixmapOffset(pPix);
#endif /* NOUVEAU_EXA_PIXMAPS */
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
		if (NVAccelDownloadM2MF(pSrc, x, y, w, h, dst, dst_pitch))
			return TRUE;
	}

	src = NVExaPixmapMap(pSrc);
	if (!src)
		return FALSE;
	src += offset;
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
	int padbytes;

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

	if (!NVAccelGetCtxSurf2DFormatFromPixmap(pDst, &surf_fmt))
		return FALSE;

	BEGIN_RING(NvContextSurfaces, NV04_CONTEXT_SURFACES_2D_FORMAT, 4);
	OUT_RING  (surf_fmt);
	OUT_RING  ((exaGetPixmapPitch(pDst) << 16) | exaGetPixmapPitch(pDst));
	OUT_PIXMAPl(pDst, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_PIXMAPl(pDst, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);

	/* Pad out input width to cover both COLORA() and COLORB() */
	iw  = (line_len + 7) & ~7;
	padbytes = iw - line_len;
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

	if (padbytes)
		h--;
	while (h--) {
		/* send a line */
		BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_COLOR(0), id);
		OUT_RINGp (src, id);

		src += src_pitch;
	}
	if (padbytes) {
		char padding[8];
		int aux = (padbytes + 7) >> 2;
		BEGIN_RING(NvImageFromCpu, NV01_IMAGE_FROM_CPU_COLOR(0), id);
		OUT_RINGp (src, id - aux);
		memcpy(padding, src + (id - aux) * 4, padbytes);
		OUT_RINGp (padding, aux);
	}

	return TRUE;
}

static inline Bool
NVAccelUploadM2MF(PixmapPtr pdpix, int x, int y, int w, int h,
		  const char *src, int src_pitch)
{
	ScrnInfoPtr pScrn = xf86Screens[pdpix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	unsigned cpp = pdpix->drawable.bitsPerPixel / 8;
	unsigned line_len = w * cpp;
	unsigned line_count = h;
	unsigned dst_pitch = 0, dst_offset = 0, linear = 0;

	BEGIN_RING(NvMemFormat, 0x184, 2);
	OUT_RELOCo(pNv->GART, NOUVEAU_BO_GART | NOUVEAU_BO_RD);
	OUT_PIXMAPo(pdpix, NOUVEAU_BO_VRAM | NOUVEAU_BO_GART | NOUVEAU_BO_WR);

	if (pNv->Architecture < NV_ARCH_50 ||
	    exaGetPixmapOffset(pdpix) < pNv->EXADriverPtr->offScreenBase) {
		linear     = 1;
		dst_pitch  = exaGetPixmapPitch(pdpix);
		dst_offset = (y * dst_pitch) + (x * cpp);
	}

	if (pNv->Architecture >= NV_ARCH_50) {
		BEGIN_RING(NvMemFormat, 0x0200, 1);
		OUT_RING  (1);

		if (linear) {
			BEGIN_RING(NvMemFormat, 0x021c, 1);
			OUT_RING  (1);
		} else {
			BEGIN_RING(NvMemFormat, 0x021c, 6);
			OUT_RING  (0);
			OUT_RING  (0);
			OUT_RING  (exaGetPixmapPitch(pdpix));
			OUT_RING  (pdpix->drawable.height);
			OUT_RING  (1);
			OUT_RING  (0);
		}
	}

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
			if (!linear) {
				BEGIN_RING(NvMemFormat, 0x0234, 1);
				OUT_RING  ((y << 16) | (x * cpp));
			}

			BEGIN_RING(NvMemFormat, 0x0238, 2);
			OUT_RELOCh(pNv->GART, 0, NOUVEAU_BO_GART |
				   NOUVEAU_BO_RD);
			OUT_PIXMAPh(pdpix, 0, NOUVEAU_BO_VRAM | 
				    NOUVEAU_BO_GART | NOUVEAU_BO_WR);
		}

		/* DMA to VRAM */
		BEGIN_RING(NvMemFormat,
			   NV04_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
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
		BEGIN_RING(NvMemFormat, NV04_MEMORY_TO_MEMORY_FORMAT_NOTIFY, 1);
		OUT_RING  (0);
		BEGIN_RING(NvMemFormat, 0x100, 1);
		OUT_RING  (0);
		FIRE_RING();
		if (nouveau_notifier_wait_status(pNv->notify0, 0, 0, 2000))
			return FALSE;

		if (linear)
			dst_offset += lc * dst_pitch;
		line_count -= lc;
		y += lc;
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
	if (w * h * cpp < 16*1024) /* heuristic */
	{
		if (pNv->Architecture < NV_ARCH_50) {
			if (NVAccelUploadIFC(pScrn, src, src_pitch, pDst,
					     x, y, w, h, cpp)) {
				exaMarkSync(pDst->drawable.pScreen);
				return TRUE;
			}
		} else {
			if (NV50EXAUploadSIFC(pScrn, src, src_pitch, pDst,
					      x, y, w, h, cpp)) {
				exaMarkSync(pDst->drawable.pScreen);
				return TRUE;
			}
		}
	}

	/* try gart-based transfer */
	if (pNv->GART) {
		if (NVAccelUploadM2MF(pDst, x, y, w, h, src, src_pitch))
			return TRUE;
	}

	/* fallback to memcpy-based transfer */
	dst = NVExaPixmapMap(pDst);
	if (!dst)
		return FALSE;
	dst += (y * dst_pitch) + (x * cpp);
	exaWaitSync(pDst->drawable.pScreen);
	if (NVAccelMemcpyRect(dst, src, h, dst_pitch, src_pitch, w*cpp))
		return TRUE;

	return FALSE;
}

#if NOUVEAU_EXA_PIXMAPS
static Bool
NVExaPrepareAccess(PixmapPtr pPix, int index)
{
	ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pixmap *nvpix;
	(void)pNv;

	nvpix = exaGetPixmapDriverPrivate(pPix);
	if (!nvpix || !nvpix->bo)
		return FALSE;

	/*XXX: ho hum.. sync if needed */

	if (nvpix->mapped)
		return TRUE;

	if (nouveau_bo_map(nvpix->bo, NOUVEAU_BO_RDWR))
		return FALSE;
	pPix->devPrivate.ptr = nvpix->bo->map;
	nvpix->mapped = TRUE;
	return TRUE;
}

static void
NVExaFinishAccess(PixmapPtr pPix, int index)
{
	ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pixmap *nvpix;
	(void)pNv;

	nvpix = exaGetPixmapDriverPrivate(pPix);
	if (!nvpix || !nvpix->bo || !nvpix->mapped)
		return;

	nouveau_bo_unmap(nvpix->bo);
	pPix->devPrivate.ptr = NULL;
	nvpix->mapped = FALSE;
}

static Bool
NVExaPixmapIsOffscreen(PixmapPtr pPix)
{
	ScrnInfoPtr pScrn = xf86Screens[pPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pixmap *nvpix;
	(void)pNv;

	nvpix = exaGetPixmapDriverPrivate(pPix);
	if (!nvpix || !nvpix->bo)
		return FALSE;

	return TRUE;
}

static void *
NVExaCreatePixmap(ScreenPtr pScreen, int size, int align)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pixmap *nvpix;

	nvpix = xcalloc(1, sizeof(struct nouveau_pixmap));
	if (!nvpix)
		return NULL;

	if (size) {
		if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM, 0, size,
				   &nvpix->bo)) {
			xfree(nvpix);
			return NULL;
		}
	}

	return nvpix;
}

static void
NVExaDestroyPixmap(ScreenPtr pScreen, void *driverPriv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pixmap *nvpix = driverPriv;

	if (!driverPriv)
		return;

	/*XXX: only if pending relocs reference this buffer..*/
	FIRE_RING();

	nouveau_bo_del(&nvpix->bo);
	xfree(nvpix);
}

static Bool
NVExaModifyPixmapHeader(PixmapPtr pPixmap, int width, int height, int depth,
			int bitsPerPixel, int devKind, pointer pPixData)
{
	ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_pixmap *nvpix;

	if (pPixData == pNv->FB->map) {
		nvpix = exaGetPixmapDriverPrivate(pPixmap);
		if (!nvpix)
			return FALSE;

		if (nouveau_bo_ref(pNv->dev, pNv->FB->handle, &nvpix->bo))
			return FALSE;

		miModifyPixmapHeader(pPixmap, width, height, depth,
				     bitsPerPixel, devKind, NULL);
		return TRUE;
	}

	return FALSE;
}
#endif

#if !NOUVEAU_EXA_PIXMAPS
static Bool
nouveau_exa_pixmap_is_offscreen(PixmapPtr pPixmap)
{
	ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	void *addr = (void *)pPixmap->devPrivate.ptr;

	if (addr >= pNv->FB->map && addr < (pNv->FB->map + pNv->FB->size))
		return TRUE;

	if (pNv->shadow[0] && (addr >= pNv->shadow[0]->map && addr < (pNv->shadow[0]->map + pNv->shadow[0]->size)))
		return TRUE;

	if (pNv->shadow[1] && (addr >= pNv->shadow[1]->map && addr < (pNv->shadow[1]->map + pNv->shadow[1]->size)))
		return TRUE;

	return FALSE;
}
#endif /* !NOUVEAU_EXA_PIXMAPS */

Bool
NVExaPixmapIsOnscreen(PixmapPtr pPixmap)
{
	ScrnInfoPtr pScrn = xf86Screens[pPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

#if NOUVEAU_EXA_PIXMAPS
	struct nouveau_pixmap *nvpix;
	nvpix = exaGetPixmapDriverPrivate(pPixmap);

	if (nvpix && nvpix->bo == pNv->FB)
		return TRUE;

#else
	unsigned long offset = exaGetPixmapOffset(pPixmap);

	if (offset < pNv->EXADriverPtr->offScreenBase)
		return TRUE;
#endif /* NOUVEAU_EXA_PIXMAPS */

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

#if NOUVEAU_EXA_PIXMAPS
	if (NOUVEAU_EXA_PIXMAPS) {
		pNv->EXADriverPtr->flags = EXA_OFFSCREEN_PIXMAPS |
					   EXA_HANDLES_PIXMAPS;
		pNv->EXADriverPtr->PrepareAccess = NVExaPrepareAccess;
		pNv->EXADriverPtr->FinishAccess = NVExaFinishAccess;
		pNv->EXADriverPtr->PixmapIsOffscreen = NVExaPixmapIsOffscreen;
		pNv->EXADriverPtr->CreatePixmap = NVExaCreatePixmap;
		pNv->EXADriverPtr->DestroyPixmap = NVExaDestroyPixmap;
		pNv->EXADriverPtr->ModifyPixmapHeader = NVExaModifyPixmapHeader;
	} else
#endif
	{
		pNv->EXADriverPtr->flags = EXA_OFFSCREEN_PIXMAPS;
		pNv->EXADriverPtr->memoryBase = pNv->FB->map;
		pNv->EXADriverPtr->offScreenBase =
			NOUVEAU_ALIGN(pScrn->virtualX, 64) * NOUVEAU_ALIGN(pScrn->virtualY,64) * 
			(pScrn->bitsPerPixel / 8); 
		pNv->EXADriverPtr->memorySize		= pNv->FB->size; 
#if EXA_VERSION_MINOR >= 2
		pNv->EXADriverPtr->PixmapIsOffscreen = nouveau_exa_pixmap_is_offscreen;
#endif
	}

	if (pNv->Architecture < NV_ARCH_50)
		pNv->EXADriverPtr->pixmapOffsetAlign = 256; 
	else
		pNv->EXADriverPtr->pixmapOffsetAlign = 65536; /* fuck me! */
	pNv->EXADriverPtr->pixmapPitchAlign = 64; 

	if (pNv->Architecture >= NV_ARCH_50) {
		struct nouveau_device_priv *nvdev = nouveau_device(pNv->dev);
		struct nouveau_bo_priv *nvbo = nouveau_bo(pNv->FB);
		struct drm_nouveau_mem_tile t;

		t.offset = nvbo->drm.offset;
		t.flags  = nvbo->drm.flags | NOUVEAU_MEM_TILE;
		t.delta  = pNv->EXADriverPtr->offScreenBase;
		t.size   = pNv->EXADriverPtr->memorySize - 
			   pNv->EXADriverPtr->offScreenBase;
		drmCommandWrite(nvdev->fd, DRM_NOUVEAU_MEM_TILE, &t, sizeof(t));

		pNv->EXADriverPtr->maxX = 8192;
		pNv->EXADriverPtr->maxY = 8192;
	} else
	if (pNv->Architecture >= NV_ARCH_20) {
		pNv->EXADriverPtr->maxX = 4096;
		pNv->EXADriverPtr->maxY = 4096;
	} else {
		pNv->EXADriverPtr->maxX = 2048;
		pNv->EXADriverPtr->maxY = 2048;
	}

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
		pNv->EXADriverPtr->CheckComposite   = NV50EXACheckComposite;
		pNv->EXADriverPtr->PrepareComposite = NV50EXAPrepareComposite;
		pNv->EXADriverPtr->Composite        = NV50EXAComposite;
		pNv->EXADriverPtr->DoneComposite    = NV50EXADoneComposite;
		break;
	default:
		break;
	}

	if (!exaDriverInit(pScreen, pNv->EXADriverPtr))
		return FALSE;
	else
		/* EXA init catches this, but only for xserver >= 1.4 */
		if (pNv->VRAMPhysicalSize / 2 < NOUVEAU_ALIGN(pScrn->virtualX, 64) * NOUVEAU_ALIGN(pScrn->virtualY, 64) * (pScrn->bitsPerPixel >> 3)) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "The virtual screen size's resolution is too big for the video RAM framebuffer at this colour depth.\n");
			return FALSE;
		}

	return TRUE;
}
