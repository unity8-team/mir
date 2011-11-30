/*
 * Copyright 2007 Ben Skeggs
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
#include "nv04_pushbuf.h"

#include "hwdefs/nv_object.xml.h"
#include "hwdefs/nv_m2mf.xml.h"
#include "hwdefs/nv01_2d.xml.h"
#include "hwdefs/nv50_2d.xml.h"

Bool
nouveau_allocate_surface(ScrnInfoPtr scrn, int width, int height, int bpp,
			 int usage_hint, int *pitch, struct nouveau_bo **bo)
{
	NVPtr pNv = NVPTR(scrn);
	Bool scanout = (usage_hint & NOUVEAU_CREATE_PIXMAP_SCANOUT);
	Bool tiled = (usage_hint & NOUVEAU_CREATE_PIXMAP_TILED);
	int tile_mode = 0, tile_flags = 0;
	int flags = NOUVEAU_BO_MAP | (bpp >= 8 ? NOUVEAU_BO_VRAM : 0);
	int cpp = bpp / 8, ret;

	if (pNv->Architecture >= NV_ARCH_50) {
		if (scanout) {
			if (pNv->tiled_scanout) {
				tiled = TRUE;
				*pitch = NOUVEAU_ALIGN(width * cpp, 64);
			} else {
				*pitch = NOUVEAU_ALIGN(width * cpp, 256);
			}
		} else {
			if (bpp >= 8)
				tiled = TRUE;
			*pitch = NOUVEAU_ALIGN(width * cpp, 64);
		}
	} else {
		if (scanout && pNv->tiled_scanout)
			tiled = TRUE;
		*pitch = NOUVEAU_ALIGN(width * cpp, 64);
	}

	if (tiled) {
		if (pNv->Architecture >= NV_ARCH_C0) {
			if (height > 64)
				tile_mode = 0x40;
			else if (height > 32)
				tile_mode = 0x30;
			else if (height > 16)
				tile_mode = 0x20;
			else if (height > 8)
				tile_mode = 0x10;
			else
				tile_mode = 0x00;

			if (usage_hint & NOUVEAU_CREATE_PIXMAP_ZETA)
				tile_flags = 0x1100; /* S8Z24 */
			else
				tile_flags = 0xfe00;

			height = NOUVEAU_ALIGN(
				height, NVC0_TILE_HEIGHT(tile_mode));
		} else if (pNv->Architecture >= NV_ARCH_50) {
			if (height > 32)
				tile_mode = 4;
			else if (height > 16)
				tile_mode = 3;
			else if (height > 8)
				tile_mode = 2;
			else if (height > 4)
				tile_mode = 1;
			else
				tile_mode = 0;

			if (usage_hint & NOUVEAU_CREATE_PIXMAP_ZETA)
				tile_flags = 0x22800;
			else if (usage_hint & NOUVEAU_CREATE_PIXMAP_SCANOUT)
				tile_flags = (bpp == 16 ? 0x7000 : 0x7a00);
			else
				tile_flags = 0x7000;

			height = NOUVEAU_ALIGN(height, 1 << (tile_mode + 2));
		} else {
			int pitch_align = max(
				pNv->dev->chipset >= 0x40 ? 1024 : 256,
				round_down_pow2(*pitch / 4));

			tile_mode = *pitch =
				NOUVEAU_ALIGN(*pitch, pitch_align);
		}
	}

	if (bpp == 32)
		tile_flags |= NOUVEAU_BO_TILE_32BPP;
	else if (bpp == 16)
		tile_flags |= NOUVEAU_BO_TILE_16BPP;

	if (usage_hint & NOUVEAU_CREATE_PIXMAP_ZETA)
		tile_flags |= NOUVEAU_BO_TILE_ZETA;

	if (usage_hint & NOUVEAU_CREATE_PIXMAP_SCANOUT)
		tile_flags |= NOUVEAU_BO_TILE_SCANOUT;

	ret = nouveau_bo_new_tile(pNv->dev, flags, 0, *pitch * height,
				  tile_mode, tile_flags, bo);
	if (ret)
		return FALSE;

	return TRUE;
}

void
NV11SyncToVBlank(PixmapPtr ppix, BoxPtr box)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *blit = pNv->NvImageBlit;
	int crtcs;

	if (!nouveau_exa_pixmap_is_onscreen(ppix))
		return;

	crtcs = nv_window_belongs_to_crtc(pScrn, box->x1, box->y1,
					  box->x2 - box->x1,
					  box->y2 - box->y1);
	if (!crtcs)
		return;

	BEGIN_RING(chan, blit, 0x0000012C, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, blit, 0x00000134, 1);
	OUT_RING  (chan, ffs(crtcs) - 1);
	BEGIN_RING(chan, blit, 0x00000100, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, blit, 0x00000130, 1);
	OUT_RING  (chan, 0);
}

static Bool
NVAccelInitDmaNotifier0(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	if (!pNv->notify0) {
		if (nouveau_notifier_alloc(pNv->chan, NvDmaNotifier0, 1,
					   &pNv->notify0))
			return FALSE;
	}

	return TRUE;
}

/* FLAGS_ROP_AND, DmaFB, DmaFB, 0 */
static Bool
NVAccelInitContextSurfaces(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *surf2d;
	uint32_t class;

	class = (pNv->Architecture >= NV_ARCH_10) ? NV10_SURFACE_2D :
						    NV04_SURFACE_2D;

	if (!pNv->NvContextSurfaces) {
		if (nouveau_grobj_alloc(chan, NvContextSurfaces, class,
					&pNv->NvContextSurfaces))
			return FALSE;
	}
	surf2d = pNv->NvContextSurfaces;

	BEGIN_RING(chan, surf2d, NV04_SURFACE_2D_DMA_NOTIFY, 1);
	OUT_RING  (chan, chan->nullobj->handle);
	BEGIN_RING(chan, surf2d,
		   NV04_SURFACE_2D_DMA_IMAGE_SOURCE, 2);
	OUT_RING  (chan, pNv->chan->vram->handle);
	OUT_RING  (chan, pNv->chan->vram->handle);

	return TRUE;
}

/* FLAGS_ROP_AND, DmaFB, DmaFB, 0 */
static Bool
NVAccelInitContextBeta1(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *beta1;

	if (!pNv->NvContextBeta1) {
		if (nouveau_grobj_alloc(chan, NvContextBeta1, 0x12,
					&pNv->NvContextBeta1))
			return FALSE;
	}
	beta1 = pNv->NvContextBeta1;

	BEGIN_RING(chan, beta1, 0x300, 1); /*alpha factor*/
	OUT_RING  (chan, 0xff << 23);

	return TRUE;
}


static Bool
NVAccelInitContextBeta4(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *beta4;
	
	if (!pNv->NvContextBeta4) {
		if (nouveau_grobj_alloc(chan, NvContextBeta4, 0x72,
					&pNv->NvContextBeta4))
			return FALSE;
	}
	beta4 = pNv->NvContextBeta4;

	BEGIN_RING(chan, beta4, 0x300, 1); /*RGBA factor*/
	OUT_RING  (chan, 0xffff0000);
	return TRUE;
}

Bool
NVAccelGetCtxSurf2DFormatFromPixmap(PixmapPtr pPix, int *fmt_ret)
{
	switch (pPix->drawable.bitsPerPixel) {
	case 32:
		*fmt_ret = NV04_SURFACE_2D_FORMAT_A8R8G8B8;
		break;
	case 24:
		*fmt_ret = NV04_SURFACE_2D_FORMAT_X8R8G8B8_Z8R8G8B8;
		break;
	case 16:
		if (pPix->drawable.depth == 16)
			*fmt_ret = NV04_SURFACE_2D_FORMAT_R5G6B5;
		else
			*fmt_ret = NV04_SURFACE_2D_FORMAT_X1R5G5B5_Z1R5G5B5;
		break;
	case 8:
		*fmt_ret = NV04_SURFACE_2D_FORMAT_Y8;
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

Bool
NVAccelGetCtxSurf2DFormatFromPicture(PicturePtr pPict, int *fmt_ret)
{
	switch (pPict->format) {
	case PICT_a8r8g8b8:
		*fmt_ret = NV04_SURFACE_2D_FORMAT_A8R8G8B8;
		break;
	case PICT_x8r8g8b8:
		*fmt_ret = NV04_SURFACE_2D_FORMAT_X8R8G8B8_Z8R8G8B8;
		break;
	case PICT_r5g6b5:
		*fmt_ret = NV04_SURFACE_2D_FORMAT_R5G6B5;
		break;
	case PICT_a8:
		*fmt_ret = NV04_SURFACE_2D_FORMAT_Y8;
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

/* A copy of exaGetOffscreenPixmap(), since it's private. */
PixmapPtr
NVGetDrawablePixmap(DrawablePtr pDraw)
{
	if (pDraw->type == DRAWABLE_WINDOW)
		return pDraw->pScreen->GetWindowPixmap ((WindowPtr) pDraw);
	else
		return (PixmapPtr) pDraw;
}

static Bool
NVAccelInitImagePattern(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *patt;

	if (!pNv->NvImagePattern) {
		if (nouveau_grobj_alloc(chan, NvImagePattern, NV04_PATTERN,
					&pNv->NvImagePattern))
			return FALSE;
	}
	patt = pNv->NvImagePattern;

	BEGIN_RING(chan, patt, NV01_PATTERN_DMA_NOTIFY, 1);
	OUT_RING  (chan, chan->nullobj->handle);
	BEGIN_RING(chan, patt, NV01_PATTERN_MONOCHROME_FORMAT, 3);
#if X_BYTE_ORDER == X_BIG_ENDIAN
	OUT_RING  (chan, NV01_PATTERN_MONOCHROME_FORMAT_LE);
#else
	OUT_RING  (chan, NV01_PATTERN_MONOCHROME_FORMAT_CGA6);
#endif
	OUT_RING  (chan, NV01_PATTERN_MONOCHROME_SHAPE_8X8);
	OUT_RING  (chan, NV04_PATTERN_PATTERN_SELECT_MONO);

	return TRUE;
}

static Bool
NVAccelInitRasterOp(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *rop;

	if (!pNv->NvRop) {
		if (nouveau_grobj_alloc(chan, NvRop, NV03_ROP, &pNv->NvRop))
			return FALSE;
	}
	rop = pNv->NvRop;

	BEGIN_RING(chan, rop, NV01_ROP_DMA_NOTIFY, 1);
	OUT_RING  (chan, chan->nullobj->handle);

	pNv->currentRop = ~0;
	return TRUE;
}

static Bool
NVAccelInitRectangle(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *rect;

	if (!pNv->NvRectangle) {
		if (nouveau_grobj_alloc(chan, NvRectangle, NV04_GDI,
					&pNv->NvRectangle))
			return FALSE;
	}
	rect = pNv->NvRectangle;

	BEGIN_RING(chan, rect, NV04_GDI_DMA_NOTIFY, 1);
	OUT_RING  (chan, pNv->notify0->handle);
	BEGIN_RING(chan, rect, NV04_GDI_DMA_FONTS, 1);
	OUT_RING  (chan, chan->nullobj->handle);
	BEGIN_RING(chan, rect, NV04_GDI_SURFACE, 1);
	OUT_RING  (chan, pNv->NvContextSurfaces->handle);
	BEGIN_RING(chan, rect, NV04_GDI_ROP, 1);
	OUT_RING  (chan, pNv->NvRop->handle);
	BEGIN_RING(chan, rect, NV04_GDI_PATTERN, 1);
	OUT_RING  (chan, pNv->NvImagePattern->handle);
	BEGIN_RING(chan, rect, NV04_GDI_OPERATION, 1);
	OUT_RING  (chan, NV04_GDI_OPERATION_ROP_AND);
	BEGIN_RING(chan, rect, NV04_GDI_MONOCHROME_FORMAT, 1);
	/* XXX why putting 1 like renouveau dump, swap the text */
#if 1 || X_BYTE_ORDER == X_BIG_ENDIAN
	OUT_RING  (chan, NV04_GDI_MONOCHROME_FORMAT_LE);
#else
	OUT_RING  (chan, NV04_GDI_MONOCHROME_FORMAT_CGA6);
#endif

	return TRUE;
}

static Bool
NVAccelInitImageBlit(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *blit;
	uint32_t class;

	class = (pNv->dev->chipset >= 0x11) ? NV15_BLIT : NV04_BLIT;

	if (!pNv->NvImageBlit) {
		if (nouveau_grobj_alloc(chan, NvImageBlit, class,
					&pNv->NvImageBlit))
			return FALSE;
	}
	blit = pNv->NvImageBlit;

	BEGIN_RING(chan, blit, NV01_BLIT_DMA_NOTIFY, 1);
	OUT_RING  (chan, pNv->notify0->handle);
	BEGIN_RING(chan, blit, NV01_BLIT_COLOR_KEY, 1);
	OUT_RING  (chan, chan->nullobj->handle);
	BEGIN_RING(chan, blit, NV04_BLIT_SURFACES, 1);
	OUT_RING  (chan, pNv->NvContextSurfaces->handle);
	BEGIN_RING(chan, blit, NV01_BLIT_CLIP, 3);
	OUT_RING  (chan, chan->nullobj->handle);
	OUT_RING  (chan, pNv->NvImagePattern->handle);
	OUT_RING  (chan, pNv->NvRop->handle);
	BEGIN_RING(chan, blit, NV01_BLIT_OPERATION, 1);
	OUT_RING  (chan, NV01_BLIT_OPERATION_ROP_AND);
	if (blit->grclass == NV15_BLIT) {
		BEGIN_RING(chan, blit, NV15_BLIT_FLIP_SET_READ, 3);
		OUT_RING  (chan, 0);
		OUT_RING  (chan, 1);
		OUT_RING  (chan, 2);
	}

	return TRUE;
}

static Bool
NVAccelInitScaledImage(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *sifm;
	uint32_t class;

	switch (pNv->Architecture) {
	case NV_ARCH_04:
		class = NV04_SIFM;
		break;
	case NV_ARCH_10:
	case NV_ARCH_20:
	case NV_ARCH_30:
		class = NV10_SIFM;
		break;
	case NV_ARCH_40:
	default:
		class = NV40_SIFM;
		break;
	}

	if (!pNv->NvScaledImage) {
		if (nouveau_grobj_alloc(chan, NvScaledImage, class,
					&pNv->NvScaledImage))
			return FALSE;
	}
	sifm = pNv->NvScaledImage;

	BEGIN_RING(chan, sifm, NV03_SIFM_DMA_NOTIFY, 7);
	OUT_RING  (chan, pNv->notify0->handle);
	OUT_RING  (chan, pNv->chan->vram->handle);
	OUT_RING  (chan, chan->nullobj->handle);
	OUT_RING  (chan, chan->nullobj->handle);
	OUT_RING  (chan, pNv->NvContextBeta1->handle);
	OUT_RING  (chan, pNv->NvContextBeta4->handle);
	OUT_RING  (chan, pNv->NvContextSurfaces->handle);
	if (pNv->Architecture>=NV_ARCH_10) {
		BEGIN_RING(chan, sifm, NV05_SIFM_COLOR_CONVERSION, 1);
		OUT_RING  (chan, NV05_SIFM_COLOR_CONVERSION_DITHER);
	}
	BEGIN_RING(chan, sifm, NV03_SIFM_OPERATION, 1);
	OUT_RING  (chan, NV03_SIFM_OPERATION_SRCCOPY);

	return TRUE;
}

static Bool
NVAccelInitClipRectangle(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *clip;

	if (!pNv->NvClipRectangle) {
		if (nouveau_grobj_alloc(pNv->chan, NvClipRectangle, NV01_CLIP,
					&pNv->NvClipRectangle))
			return FALSE;
	}
	clip = pNv->NvClipRectangle;

	BEGIN_RING(chan, clip, NV01_CLIP_DMA_NOTIFY, 1);
	OUT_RING  (chan, chan->nullobj->handle);

	return TRUE;
}

/* FLAGS_NONE, NvDmaFB, NvDmaAGP, NvDmaNotifier0 */
static Bool
NVAccelInitMemFormat(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *m2mf;
	uint32_t class;

	if (pNv->Architecture < NV_ARCH_50)
		class = NV03_M2MF;
	else
		class = NV50_M2MF;

	if (!pNv->NvMemFormat) {
		if (nouveau_grobj_alloc(chan, NvMemFormat, class,
					&pNv->NvMemFormat))
			return FALSE;
	}
	m2mf = pNv->NvMemFormat;

	BEGIN_RING(chan, m2mf, NV03_M2MF_DMA_NOTIFY, 1);
	OUT_RING  (chan, pNv->notify0->handle);
	BEGIN_RING(chan, m2mf, NV03_M2MF_DMA_BUFFER_IN, 2);
	OUT_RING  (chan, chan->vram->handle);
	OUT_RING  (chan, chan->vram->handle);

	return TRUE;
}

static Bool
NVAccelInitImageFromCpu(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *ifc;
	uint32_t class;

	switch (pNv->Architecture) {
	case NV_ARCH_04:
		class = NV04_IFC;
		break;
	case NV_ARCH_10:
	case NV_ARCH_20:
	case NV_ARCH_30:
	case NV_ARCH_40:
	default:
		class = NV10_IFC;
		break;
	}

	if (!pNv->NvImageFromCpu) {
		if (nouveau_grobj_alloc(chan, NvImageFromCpu, class,
					&pNv->NvImageFromCpu))
			return FALSE;
	}
	ifc = pNv->NvImageFromCpu;

	BEGIN_RING(chan, ifc, NV01_IFC_DMA_NOTIFY, 1);
	OUT_RING  (chan, pNv->notify0->handle);
	BEGIN_RING(chan, ifc, NV01_IFC_CLIP, 1);
	OUT_RING  (chan, chan->nullobj->handle);
	BEGIN_RING(chan, ifc, NV01_IFC_PATTERN, 1);
	OUT_RING  (chan, chan->nullobj->handle);
	BEGIN_RING(chan, ifc, NV01_IFC_ROP, 1);
	OUT_RING  (chan, chan->nullobj->handle);
	if (pNv->Architecture >= NV_ARCH_10) {
		BEGIN_RING(chan, ifc, NV01_IFC_BETA, 1);
		OUT_RING  (chan, chan->nullobj->handle);
		BEGIN_RING(chan, ifc, NV04_IFC_BETA4, 1);
		OUT_RING  (chan, chan->nullobj->handle);
	}
	BEGIN_RING(chan, ifc, NV04_IFC_SURFACE, 1);
	OUT_RING  (chan, pNv->NvContextSurfaces->handle);
	BEGIN_RING(chan, ifc, NV01_IFC_OPERATION, 1);
	OUT_RING  (chan, NV01_IFC_OPERATION_SRCCOPY);

	return TRUE;
}

static Bool
NVAccelInit2D_NV50(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *eng2d;

	if (!pNv->Nv2D) {
		if (nouveau_grobj_alloc(chan, Nv2D, NV50_2D, &pNv->Nv2D))
			return FALSE;
	}
	eng2d = pNv->Nv2D;

	BEGIN_RING(chan, eng2d, NV50_2D_DMA_NOTIFY, 3);
	OUT_RING  (chan, pNv->notify0->handle);
	OUT_RING  (chan, pNv->chan->vram->handle);
	OUT_RING  (chan, pNv->chan->vram->handle);

	/* Magics from nv, no clue what they do, but at least some
	 * of them are needed to avoid crashes.
	 */
	BEGIN_RING(chan, eng2d, 0x260, 1);
	OUT_RING  (chan, 1);
	BEGIN_RING(chan, eng2d, NV50_2D_CLIP_ENABLE, 1);
	OUT_RING  (chan, 1);
	BEGIN_RING(chan, eng2d, NV50_2D_COLOR_KEY_ENABLE, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, eng2d, 0x58c, 1);
	OUT_RING  (chan, 0x111);

	pNv->currentRop = 0xfffffffa;
	return TRUE;
}

#define INIT_CONTEXT_OBJECT(name) do {                                        \
	ret = NVAccelInit##name(pScrn);                                       \
	if (!ret) {                                                           \
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,                         \
			   "Failed to initialise context object: "#name       \
			   " (%d)\n", ret);                                   \
		return FALSE;                                                 \
	}                                                                     \
} while(0)

Bool
NVAccelCommonInit(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	Bool ret;

	if (pNv->NoAccel)
		return TRUE;

	/* General engine objects */
	if (pNv->Architecture < NV_ARCH_C0)
		INIT_CONTEXT_OBJECT(DmaNotifier0);

	/* 2D engine */
	if (pNv->Architecture < NV_ARCH_50) {
		INIT_CONTEXT_OBJECT(ContextSurfaces);
		INIT_CONTEXT_OBJECT(ContextBeta1);
		INIT_CONTEXT_OBJECT(ContextBeta4);
		INIT_CONTEXT_OBJECT(ImagePattern);
		INIT_CONTEXT_OBJECT(RasterOp);
		INIT_CONTEXT_OBJECT(Rectangle);
		INIT_CONTEXT_OBJECT(ImageBlit);
		INIT_CONTEXT_OBJECT(ScaledImage);
		INIT_CONTEXT_OBJECT(ClipRectangle);
		INIT_CONTEXT_OBJECT(ImageFromCpu);
	} else
	if (pNv->Architecture < NV_ARCH_C0) {
		INIT_CONTEXT_OBJECT(2D_NV50);
	} else {
		INIT_CONTEXT_OBJECT(2D_NVC0);
	}

	if (pNv->Architecture < NV_ARCH_C0)
		INIT_CONTEXT_OBJECT(MemFormat);
	else
		INIT_CONTEXT_OBJECT(M2MF_NVC0);

	/* 3D init */
	switch (pNv->Architecture) {
	case NV_ARCH_C0:
		INIT_CONTEXT_OBJECT(3D_NVC0);
		break;
	case NV_ARCH_50:
		INIT_CONTEXT_OBJECT(NV50TCL);
		break;
	case NV_ARCH_40:
		INIT_CONTEXT_OBJECT(NV40TCL);
		break;
	case NV_ARCH_30:
		INIT_CONTEXT_OBJECT(NV30TCL);
		break;
	case NV_ARCH_20:
	case NV_ARCH_10:
		INIT_CONTEXT_OBJECT(NV10TCL);
		break;
	default:
		break;
	}

	return TRUE;
}

void NVAccelFree(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	if (pNv->NoAccel)
		return;

	nouveau_notifier_free(&pNv->notify0);
	nouveau_notifier_free(&pNv->vblank_sem);

	nouveau_grobj_free(&pNv->NvContextSurfaces);
	nouveau_grobj_free(&pNv->NvContextBeta1);
	nouveau_grobj_free(&pNv->NvContextBeta4);
	nouveau_grobj_free(&pNv->NvImagePattern);
	nouveau_grobj_free(&pNv->NvRop);
	nouveau_grobj_free(&pNv->NvRectangle);
	nouveau_grobj_free(&pNv->NvImageBlit);
	nouveau_grobj_free(&pNv->NvScaledImage);
	nouveau_grobj_free(&pNv->NvClipRectangle);
	nouveau_grobj_free(&pNv->NvImageFromCpu);
	nouveau_grobj_free(&pNv->Nv2D);
	nouveau_grobj_free(&pNv->NvMemFormat);
	nouveau_grobj_free(&pNv->NvSW);
	nouveau_grobj_free(&pNv->Nv3D);

	nouveau_bo_ref(NULL, &pNv->tesla_scratch);
	nouveau_bo_ref(NULL, &pNv->shader_mem);
}
