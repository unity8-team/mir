#include "nv_include.h"

#define NV50EXA_LOCALS(p)                                              \
	ScrnInfoPtr pScrn = xf86Screens[(p)->drawable.pScreen->myNum]; \
	NVPtr pNv = NVPTR(pScrn);                                      \
	(void)pNv

static Bool
NV50EXA2DSurfaceFormat(PixmapPtr pPix, uint32_t *fmt)
{
	NV50EXA_LOCALS(pPix);

	switch (pPix->drawable.depth) {
	case 8 : *fmt = NV50_2D_SRC_FORMAT_8BPP; break;
	case 15: *fmt = NV50_2D_SRC_FORMAT_15BPP; break;
	case 16: *fmt = NV50_2D_SRC_FORMAT_16BPP; break;
	case 24: *fmt = NV50_2D_SRC_FORMAT_24BPP; break;
	case 32: *fmt = NV50_2D_SRC_FORMAT_32BPP; break;
	default:
		 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			    "Unknown surface format for bpp=%d\n",
			    pPix->drawable.depth);
		 return FALSE;
	}

	return TRUE;
}

static Bool
NV50EXAAcquireSurface2D(PixmapPtr pPix, int is_src)
{
	NV50EXA_LOCALS(pPix);
	int mthd = is_src ? NV50_2D_SRC_FORMAT : NV50_2D_DST_FORMAT;
	uint32_t fmt, bo_flags;

	if (!NV50EXA2DSurfaceFormat(pPix, &fmt))
		return FALSE;

	bo_flags  = NOUVEAU_BO_VRAM;
	bo_flags |= is_src ? NOUVEAU_BO_RD : NOUVEAU_BO_WR;

	BEGIN_RING(Nv2D, mthd, 2);
	OUT_RING  (fmt);
	OUT_RING  (1);

	BEGIN_RING(Nv2D, mthd + 0x14, 5);
	OUT_RING  ((uint32_t)exaGetPixmapPitch(pPix));
	OUT_RING  (pPix->drawable.width);
	OUT_RING  (pPix->drawable.height);
	OUT_PIXMAPh(pPix, 0, bo_flags);
	OUT_PIXMAPl(pPix, 0, bo_flags);

	if (is_src == 0) {
		BEGIN_RING(Nv2D, NV50_2D_CLIP_X, 4);
		OUT_RING  (0);
		OUT_RING  (0);
		OUT_RING  (pPix->drawable.width);
		OUT_RING  (pPix->drawable.height);
	}

	return TRUE;
}

static Bool
NV50EXAAcquireSurfaces(PixmapPtr pdPix)
{
	NV50EXA_LOCALS(pdPix);

	return TRUE;
}

static void
NV50EXAReleaseSurfaces(PixmapPtr pdPix)
{
	NV50EXA_LOCALS(pdPix);

	BEGIN_RING(Nv2D, NV50_2D_NOP, 1);
	OUT_RING  (0);
	FIRE_RING();
}

static void
NV50EXASetPattern(PixmapPtr pdPix, int col0, int col1, int pat0, int pat1)
{
	NV50EXA_LOCALS(pdPix);

	BEGIN_RING(Nv2D, NV50_2D_PATTERN_COLOR(0), 4);
	OUT_RING  (col0);
	OUT_RING  (col1);
	OUT_RING  (pat0);
	OUT_RING  (pat1);
}

extern const int NVCopyROP[16];
static void
NV50EXASetROP(PixmapPtr pdPix, int alu, Pixel planemask)
{
	NV50EXA_LOCALS(pdPix);
	int rop = NVCopyROP[alu];

	BEGIN_RING(Nv2D, NV50_2D_OPERATION, 1);
	if(alu == GXcopy && planemask == ~0) {
		OUT_RING  (NV50_2D_OPERATION_SRCCOPY);
		return;
	} else {
		OUT_RING  (NV50_2D_OPERATION_ROP_AND);
	}

	BEGIN_RING(Nv2D, NV50_2D_PATTERN_FORMAT, 2);
	switch (pdPix->drawable.depth) {
		case  8: OUT_RING  (3); break;
		case 15: OUT_RING  (1); break;
		case 16: OUT_RING  (0); break;
		case 24:
		case 32:
		default:
			 OUT_RING  (2);
			 break;
	}
	OUT_RING(1);

	if(planemask != ~0) {
		NV50EXASetPattern(pdPix, 0, planemask, ~0, ~0);
		rop = (rop & 0xf0) | 0x0a;
	} else
	if((pNv->currentRop & 0x0f) == 0x0a) {
		NV50EXASetPattern(pdPix, ~0, ~0, ~0, ~0);
	}

	if (pNv->currentRop != rop) {
		BEGIN_RING(Nv2D, NV50_2D_ROP, 1);
		OUT_RING  (rop);
		pNv->currentRop = rop;
	}
}

Bool
NV50EXAPrepareSolid(PixmapPtr pdPix, int alu, Pixel planemask, Pixel fg)
{
	NV50EXA_LOCALS(pdPix);
	uint32_t fmt;

	if(pdPix->drawable.depth > 24)
		return FALSE;
	if (!NV50EXA2DSurfaceFormat(pdPix, &fmt))
		return FALSE;

	if (!NV50EXAAcquireSurface2D(pdPix, 0))
		return FALSE;
	if (!NV50EXAAcquireSurfaces(pdPix))
		return FALSE;
	NV50EXASetROP(pdPix, alu, planemask);

	BEGIN_RING(Nv2D, 0x580, 3);
	OUT_RING  (4);
	OUT_RING  (fmt);
	OUT_RING  (fg);

	return TRUE;
}

void
NV50EXASolid(PixmapPtr pdPix, int x1, int y1, int x2, int y2)
{
	NV50EXA_LOCALS(pdPix);

	BEGIN_RING(Nv2D, NV50_2D_RECT_X1, 4);
	OUT_RING  (x1);
	OUT_RING  (y1);
	OUT_RING  (x2);
	OUT_RING  (y2);

	if((x2 - x1) * (y2 - y1) >= 512)
		FIRE_RING();
}

void
NV50EXADoneSolid(PixmapPtr pdPix)
{
	NV50EXA_LOCALS(pdPix);

	NV50EXAReleaseSurfaces(pdPix);
}

Bool
NV50EXAPrepareCopy(PixmapPtr psPix, PixmapPtr pdPix, int dx, int dy,
		   int alu, Pixel planemask)
{
	NV50EXA_LOCALS(pdPix);

	if (!NV50EXAAcquireSurface2D(psPix, 1))
		return FALSE;
	if (!NV50EXAAcquireSurface2D(pdPix, 0))
		return FALSE;
	if (!NV50EXAAcquireSurfaces(pdPix))
		return FALSE;
	NV50EXASetROP(pdPix, alu, planemask);

	return TRUE;
}

void
NV50EXACopy(PixmapPtr pdPix, int srcX , int srcY,
			     int dstX , int dstY,
			     int width, int height)
{
	NV50EXA_LOCALS(pdPix);

	BEGIN_RING(Nv2D, 0x0110, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv2D, NV50_2D_BLIT_DST_X, 12);
	OUT_RING  (dstX);
	OUT_RING  (dstY);
	OUT_RING  (width);
	OUT_RING  (height);
	OUT_RING  (0);
	OUT_RING  (1);
	OUT_RING  (0);
	OUT_RING  (1);
	OUT_RING  (0);
	OUT_RING  (srcX);
	OUT_RING  (0);
	OUT_RING  (srcY);

	if(width * height >= 512)
		FIRE_RING();
}

void
NV50EXADoneCopy(PixmapPtr pdPix)
{
	NV50EXA_LOCALS(pdPix);

	NV50EXAReleaseSurfaces(pdPix);
}


