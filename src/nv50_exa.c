/*
 * Copyright 2007 NVIDIA, Corporation
 * Copyright 2008 Ben Skeggs
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

#include "nv50_accel.h"
#include "nv50_texture.h"

struct nv50_exa_state {
	Bool have_mask;

	struct {
		PictTransformPtr transform;
		float width;
		float height;
	} unit[2];
};
static struct nv50_exa_state exa_state;

#define NV50EXA_LOCALS(p)                                              \
	ScrnInfoPtr pScrn = xf86Screens[(p)->drawable.pScreen->myNum]; \
	NVPtr pNv = NVPTR(pScrn);                                      \
	struct nv50_exa_state *state = &exa_state;                     \
	(void)pNv; (void)state

#define BF(f) (NV50TCL_BLEND_FUNC_SRC_RGB_##f | 0x4000)

struct nv50_blend_op {
	unsigned src_alpha;
	unsigned dst_alpha;
	unsigned src_blend;
	unsigned dst_blend;
};

static struct nv50_blend_op
NV50EXABlendOp[] = {
/* Clear       */ { 0, 0, BF(               ZERO), BF(               ZERO) },
/* Src         */ { 0, 0, BF(                ONE), BF(               ZERO) },
/* Dst         */ { 0, 0, BF(               ZERO), BF(                ONE) },
/* Over        */ { 1, 0, BF(                ONE), BF(ONE_MINUS_SRC_ALPHA) },
/* OverReverse */ { 0, 1, BF(ONE_MINUS_DST_ALPHA), BF(                ONE) },
/* In          */ { 0, 1, BF(          DST_ALPHA), BF(               ZERO) },
/* InReverse   */ { 1, 0, BF(               ZERO), BF(          SRC_ALPHA) },
/* Out         */ { 0, 1, BF(ONE_MINUS_DST_ALPHA), BF(               ZERO) },
/* OutReverse  */ { 1, 0, BF(               ZERO), BF(ONE_MINUS_SRC_ALPHA) },
/* Atop        */ { 1, 1, BF(          DST_ALPHA), BF(ONE_MINUS_SRC_ALPHA) },
/* AtopReverse */ { 1, 1, BF(ONE_MINUS_DST_ALPHA), BF(          SRC_ALPHA) },
/* Xor         */ { 1, 1, BF(ONE_MINUS_DST_ALPHA), BF(ONE_MINUS_SRC_ALPHA) },
/* Add         */ { 0, 0, BF(                ONE), BF(                ONE) },
};

static Bool
NV50EXA2DSurfaceFormat(PixmapPtr ppix, uint32_t *fmt)
{
	NV50EXA_LOCALS(ppix);

	switch (ppix->drawable.depth) {
	case 8 : *fmt = NV50_2D_SRC_FORMAT_8BPP; break;
	case 15: *fmt = NV50_2D_SRC_FORMAT_15BPP; break;
	case 16: *fmt = NV50_2D_SRC_FORMAT_16BPP; break;
	case 24: *fmt = NV50_2D_SRC_FORMAT_24BPP; break;
	case 32: *fmt = NV50_2D_SRC_FORMAT_32BPP; break;
	default:
		 xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			    "Unknown surface format for bpp=%d\n",
			    ppix->drawable.depth);
		 return FALSE;
	}

	return TRUE;
}

static Bool
NV50EXAAcquireSurface2D(PixmapPtr ppix, int is_src)
{
	NV50EXA_LOCALS(ppix);
	int mthd = is_src ? NV50_2D_SRC_FORMAT : NV50_2D_DST_FORMAT;
	uint32_t fmt, bo_flags;

	if (!NV50EXA2DSurfaceFormat(ppix, &fmt))
		return FALSE;

	bo_flags  = NOUVEAU_BO_VRAM;
	bo_flags |= is_src ? NOUVEAU_BO_RD : NOUVEAU_BO_WR;

	if (exaGetPixmapOffset(ppix) < pNv->EXADriverPtr->offScreenBase) {
		BEGIN_RING(Nv2D, mthd, 2);
		OUT_RING  (fmt);
		OUT_RING  (1);
		BEGIN_RING(Nv2D, mthd + 0x14, 1);
		OUT_RING  ((uint32_t)exaGetPixmapPitch(ppix));
	} else {
		BEGIN_RING(Nv2D, mthd, 5);
		OUT_RING  (fmt);
		OUT_RING  (0);
		OUT_RING  (0);
		OUT_RING  (1);
		OUT_RING  (0);
	}

	BEGIN_RING(Nv2D, mthd + 0x18, 4);
	OUT_RING  (ppix->drawable.width);
	OUT_RING  (ppix->drawable.height);
	OUT_PIXMAPh(ppix, 0, bo_flags);
	OUT_PIXMAPl(ppix, 0, bo_flags);

	if (is_src == 0) {
		BEGIN_RING(Nv2D, NV50_2D_CLIP_X, 4);
		OUT_RING  (0);
		OUT_RING  (0);
		OUT_RING  (ppix->drawable.width);
		OUT_RING  (ppix->drawable.height);
	}

	return TRUE;
}

static void
NV50EXASetPattern(PixmapPtr pdpix, int col0, int col1, int pat0, int pat1)
{
	NV50EXA_LOCALS(pdpix);

	BEGIN_RING(Nv2D, NV50_2D_PATTERN_COLOR(0), 4);
	OUT_RING  (col0);
	OUT_RING  (col1);
	OUT_RING  (pat0);
	OUT_RING  (pat1);
}

extern const int NVCopyROP[16];
static void
NV50EXASetROP(PixmapPtr pdpix, int alu, Pixel planemask)
{
	NV50EXA_LOCALS(pdpix);
	int rop = NVCopyROP[alu];

	BEGIN_RING(Nv2D, NV50_2D_OPERATION, 1);
	if (alu == GXcopy && planemask == ~0) {
		OUT_RING  (NV50_2D_OPERATION_SRCCOPY);
		return;
	} else {
		OUT_RING  (NV50_2D_OPERATION_ROP_AND);
	}

	BEGIN_RING(Nv2D, NV50_2D_PATTERN_FORMAT, 2);
	switch (pdpix->drawable.depth) {
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
		NV50EXASetPattern(pdpix, 0, planemask, ~0, ~0);
		rop = (rop & 0xf0) | 0x0a;
	} else
	if((pNv->currentRop & 0x0f) == 0x0a) {
		NV50EXASetPattern(pdpix, ~0, ~0, ~0, ~0);
	}

	if (pNv->currentRop != rop) {
		BEGIN_RING(Nv2D, NV50_2D_ROP, 1);
		OUT_RING  (rop);
		pNv->currentRop = rop;
	}
}

Bool
NV50EXAPrepareSolid(PixmapPtr pdpix, int alu, Pixel planemask, Pixel fg)
{
	NV50EXA_LOCALS(pdpix);
	uint32_t fmt;

	if (pdpix->drawable.depth > 24)
		NOUVEAU_FALLBACK("32bpp\n");
	if (!NV50EXA2DSurfaceFormat(pdpix, &fmt))
		NOUVEAU_FALLBACK("rect format\n");

	if (!NV50EXAAcquireSurface2D(pdpix, 0))
		NOUVEAU_FALLBACK("dest pixmap\n");
	NV50EXASetROP(pdpix, alu, planemask);

	BEGIN_RING(Nv2D, 0x580, 3);
	OUT_RING  (4);
	OUT_RING  (fmt);
	OUT_RING  (fg);

	return TRUE;
}

void
NV50EXASolid(PixmapPtr pdpix, int x1, int y1, int x2, int y2)
{
	NV50EXA_LOCALS(pdpix);

	BEGIN_RING(Nv2D, NV50_2D_RECT_X1, 4);
	OUT_RING  (x1);
	OUT_RING  (y1);
	OUT_RING  (x2);
	OUT_RING  (y2);

	if((x2 - x1) * (y2 - y1) >= 512)
		FIRE_RING();
}

void
NV50EXADoneSolid(PixmapPtr pdpix)
{
}

Bool
NV50EXAPrepareCopy(PixmapPtr pspix, PixmapPtr pdpix, int dx, int dy,
		   int alu, Pixel planemask)
{
	NV50EXA_LOCALS(pdpix);

	if (!NV50EXAAcquireSurface2D(pspix, 1))
		NOUVEAU_FALLBACK("src pixmap\n");
	if (!NV50EXAAcquireSurface2D(pdpix, 0))
		NOUVEAU_FALLBACK("dest pixmap\n");
	NV50EXASetROP(pdpix, alu, planemask);

	return TRUE;
}

void
NV50EXACopy(PixmapPtr pdpix, int srcX , int srcY,
			     int dstX , int dstY,
			     int width, int height)
{
	NV50EXA_LOCALS(pdpix);

	BEGIN_RING(Nv2D, 0x0110, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv2D, 0x088c, 1);
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
NV50EXADoneCopy(PixmapPtr pdpix)
{
}

Bool
NV50EXAUploadSIFC(ScrnInfoPtr pScrn, const char *src, int src_pitch,
		  PixmapPtr pdpix, int x, int y, int w, int h, int cpp)
{
	NVPtr pNv = NVPTR(pScrn);
	int line_dwords = (w * cpp + 3) / 4;
	uint32_t sifc_fmt;

	if (!NV50EXA2DSurfaceFormat(pdpix, &sifc_fmt))
		NOUVEAU_FALLBACK("hostdata format\n");
	if (!NV50EXAAcquireSurface2D(pdpix, 0))
		NOUVEAU_FALLBACK("dest pixmap\n");

	BEGIN_RING(Nv2D, NV50_2D_OPERATION, 1);
	OUT_RING (NV50_2D_OPERATION_SRCCOPY);
	BEGIN_RING(Nv2D, NV50_2D_SIFC_UNK0800, 2);
	OUT_RING (0);
	OUT_RING (sifc_fmt);
	BEGIN_RING(Nv2D, NV50_2D_SIFC_WIDTH, 10);
	OUT_RING ((line_dwords * 4) / cpp);
	OUT_RING (h);
	OUT_RING (0);
	OUT_RING (1);
	OUT_RING (0);
	OUT_RING (1);
	OUT_RING (0);
	OUT_RING (x);
	OUT_RING (0);
	OUT_RING (y);

	while (h--) {
		int count = line_dwords;
		const char *p = src;

		while(count) {
			int size = count > 1792 ? 1792 : count;

			BEGIN_RING(Nv2D, NV50_2D_SIFC_DATA | 0x40000000, size);
			OUT_RINGp (p, size);

			p += size * cpp;
			count -= size;
		}

		src += src_pitch;
	}

	return TRUE;
}

static Bool
NV50EXACheckRenderTarget(PicturePtr ppict)
{
	if (ppict->pDrawable->width > 8192 ||
	    ppict->pDrawable->height > 8192)
		NOUVEAU_FALLBACK("render target dimensions exceeded %dx%d\n",
				 ppict->pDrawable->width,
				 ppict->pDrawable->height);

	switch (ppict->format) {
	case PICT_a8r8g8b8:
	case PICT_x8r8g8b8:
	case PICT_r5g6b5:
	case PICT_a8:
		break;
	default:
		NOUVEAU_FALLBACK("picture format 0x%08x\n", ppict->format);
	}

	return TRUE;
}

static Bool
NV50EXARenderTarget(PixmapPtr ppix, PicturePtr ppict)
{
	NV50EXA_LOCALS(ppix);
	unsigned format;

	/*XXX: Scanout buffer not tiled, someone needs to figure it out */
	if (exaGetPixmapOffset(ppix) < pNv->EXADriverPtr->offScreenBase)
		NOUVEAU_FALLBACK("pixmap is scanout buffer\n");

	switch (ppict->format) {
	case PICT_a8r8g8b8: format = NV50TCL_RT_FORMAT_32BPP; break;
	case PICT_x8r8g8b8: format = NV50TCL_RT_FORMAT_24BPP; break;
	case PICT_r5g6b5  : format = NV50TCL_RT_FORMAT_16BPP; break;
	case PICT_a8      : format = NV50TCL_RT_FORMAT_8BPP; break;
	default:
		NOUVEAU_FALLBACK("invalid picture format\n");
	}

	BEGIN_RING(Nv3D, NV50TCL_RT_ADDRESS_HIGH(0), 5);
	OUT_PIXMAPh(ppix, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_PIXMAPl(ppix, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);
	OUT_RING  (format);
	OUT_RING  (0);
	OUT_RING  (0x00000000);
	BEGIN_RING(Nv3D, NV50TCL_RT_HORIZ(0), 2);
	OUT_RING  (ppix->drawable.width);
	OUT_RING  (ppix->drawable.height);
	BEGIN_RING(Nv3D, 0x1224, 1);
	OUT_RING  (0x00000001);

	return TRUE;
}

static Bool
NV50EXACheckTexture(PicturePtr ppict)
{
	if (ppict->pDrawable->width > 8192 ||
	    ppict->pDrawable->height > 8192)
		NOUVEAU_FALLBACK("texture dimensions exceeded %dx%d\n",
				 ppict->pDrawable->width,
				 ppict->pDrawable->height);

	switch (ppict->format) {
	case PICT_a8r8g8b8:
	case PICT_a8b8g8r8:
	case PICT_x8r8g8b8:
	case PICT_x8b8g8r8:
	case PICT_r5g6b5:
	case PICT_a8:
		break;
	default:
		NOUVEAU_FALLBACK("picture format 0x%08x\n", ppict->format);
	}

	switch (ppict->filter) {
	case PictFilterNearest:
	case PictFilterBilinear:
		break;
	default:
		NOUVEAU_FALLBACK("picture filter %d\n", ppict->filter);
	}

	return TRUE;
}

static Bool
NV50EXATexture(PixmapPtr ppix, PicturePtr ppict, unsigned unit)
{
	NV50EXA_LOCALS(ppix);

	/*XXX: Scanout buffer not tiled, someone needs to figure it out */
	if (exaGetPixmapOffset(ppix) < pNv->EXADriverPtr->offScreenBase)
		NOUVEAU_FALLBACK("pixmap is scanout buffer\n");

	BEGIN_RING(Nv3D, NV50TCL_CB_ADDR, 1);
	OUT_RING  (CB_TIC | ((unit * 8) << NV50TCL_CB_ADDR_ID_SHIFT));
	BEGIN_RING(Nv3D, NV50TCL_CB_DATA(0) | 0x40000000, 8);
	switch (ppict->format) {
	case PICT_a8r8g8b8:
		OUT_RING(NV50TIC_0_0_MAPA_C3 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPR_C0 | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_MAPG_C1 | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPB_C2 | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_FMT_8_8_8_8);
		break;
	case PICT_a8b8g8r8:
		OUT_RING(NV50TIC_0_0_MAPA_C3 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPR_C2 | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_MAPG_C1 | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPB_C0 | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_FMT_8_8_8_8);
		break;
	case PICT_x8r8g8b8:
		OUT_RING(NV50TIC_0_0_MAPA_ONE | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPR_C0 | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_MAPG_C1 | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPB_C2 | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_FMT_8_8_8_8);
		break;
	case PICT_x8b8g8r8:
		OUT_RING(NV50TIC_0_0_MAPA_ONE | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPR_C2 | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_MAPG_C1 | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPB_C0 | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_FMT_8_8_8_8);
		break;
	case PICT_r5g6b5:
		OUT_RING(NV50TIC_0_0_MAPA_ONE | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPR_C0 | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_MAPG_C1 | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPB_C2 | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_FMT_5_6_5);
		break;
	case PICT_a8:
		OUT_RING(NV50TIC_0_0_MAPA_C0 | NV50TIC_0_0_TYPEA_UNORM |
			 NV50TIC_0_0_MAPR_ZERO | NV50TIC_0_0_TYPER_UNORM |
			 NV50TIC_0_0_MAPG_ZERO | NV50TIC_0_0_TYPEB_UNORM |
			 NV50TIC_0_0_MAPB_ZERO | NV50TIC_0_0_TYPEG_UNORM |
			 NV50TIC_0_0_FMT_8);
		break;
	default:
		NOUVEAU_FALLBACK("invalid picture format\n");
	}
	OUT_PIXMAPl(ppix, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_RING  (0xd0005000);
	OUT_RING  (0x00300000);
	OUT_RING  (ppix->drawable.width);
	OUT_RING  ((1 << NV50TIC_0_5_DEPTH_SHIFT) | ppix->drawable.height);
	OUT_RING  (0x03000000);
	OUT_PIXMAPh(ppix, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);

	BEGIN_RING(Nv3D, NV50TCL_CB_ADDR, 1);
	OUT_RING  (CB_TSC | ((unit * 8) << NV50TCL_CB_ADDR_ID_SHIFT));
	BEGIN_RING(Nv3D, NV50TCL_CB_DATA(0) | 0x40000000, 8);
	if (ppict->repeat) {
		switch (ppict->repeatType) {
		case RepeatPad:
			OUT_RING(NV50TSC_1_0_WRAPS_CLAMP |
				 NV50TSC_1_0_WRAPT_CLAMP |
				 NV50TSC_1_0_WRAPR_CLAMP | 0x00024000);
			break;
		case RepeatReflect:
			OUT_RING(NV50TSC_1_0_WRAPS_MIRROR_REPEAT |
				 NV50TSC_1_0_WRAPT_MIRROR_REPEAT |
				 NV50TSC_1_0_WRAPR_MIRROR_REPEAT | 0x00024000);
			break;
		case RepeatNormal:
		default:
			OUT_RING(NV50TSC_1_0_WRAPS_REPEAT |
				 NV50TSC_1_0_WRAPT_REPEAT |
				 NV50TSC_1_0_WRAPR_REPEAT | 0x00024000);
			break;
		}
	} else {
		OUT_RING(NV50TSC_1_0_WRAPS_CLAMP_TO_BORDER |
			 NV50TSC_1_0_WRAPT_CLAMP_TO_BORDER |
			 NV50TSC_1_0_WRAPR_CLAMP_TO_BORDER | 0x00024000);
	}
	if (ppict->filter == PictFilterBilinear) {
		OUT_RING(NV50TSC_1_1_MAGF_LINEAR |
			 NV50TSC_1_1_MINF_LINEAR |
			 NV50TSC_1_1_MIPF_NONE);
	} else {
		OUT_RING(NV50TSC_1_1_MAGF_NEAREST |
			 NV50TSC_1_1_MINF_NEAREST |
			 NV50TSC_1_1_MIPF_NONE);
	}
	OUT_RING  (0x00000000);
	OUT_RING  (0x00000000);
	OUT_RING  (0x00000000);
	OUT_RING  (0x00000000);
	OUT_RING  (0x00000000);
	OUT_RING  (0x00000000);

	state->unit[unit].width = ppix->drawable.width;
	state->unit[unit].height = ppix->drawable.height;
	state->unit[unit].transform = ppict->transform;
	return TRUE;
}

static Bool
NV50EXACheckBlend(int op)
{
	if (op > PictOpAdd)
		NOUVEAU_FALLBACK("unsupported blend op %d\n", op);
	return TRUE;
}

static void
NV50EXABlend(PixmapPtr ppix, PicturePtr ppict, int op, int component_alpha)
{
	NV50EXA_LOCALS(ppix);
	struct nv50_blend_op *b = &NV50EXABlendOp[op];
	unsigned sblend = b->src_blend;
	unsigned dblend = b->dst_blend;

	if (b->dst_alpha) {
		if (!PICT_FORMAT_A(ppict->format)) {
			if (sblend == BF(DST_ALPHA))
				sblend = BF(ONE);
			else
			if (sblend == BF(ONE_MINUS_DST_ALPHA))
				sblend = BF(ZERO);
		} else
		if (ppict->format == PICT_a8) {
			if (sblend == BF(DST_ALPHA))
				sblend = BF(DST_COLOR);
			else
			if (sblend == BF(ONE_MINUS_DST_ALPHA))
				sblend = BF(ONE_MINUS_DST_COLOR);
		}
	}

	if (b->src_alpha && component_alpha) {
		if (dblend == BF(SRC_ALPHA))
			dblend = BF(SRC_COLOR);
		else
		if (dblend == BF(ONE_MINUS_SRC_ALPHA))
			dblend = BF(ONE_MINUS_SRC_COLOR);
	}

	if (b->src_blend == BF(ONE) && b->dst_blend == BF(ZERO)) {
		BEGIN_RING(Nv3D, NV50TCL_BLEND_ENABLE(0), 1);
		OUT_RING  (0);
	} else {
		BEGIN_RING(Nv3D, NV50TCL_BLEND_ENABLE(0), 1);
		OUT_RING  (1);
		BEGIN_RING(Nv3D, NV50TCL_BLEND_EQUATION_RGB, 5);
		OUT_RING  (NV50TCL_BLEND_EQUATION_RGB_FUNC_ADD);
		OUT_RING  (sblend);
		OUT_RING  (dblend);
		OUT_RING  (NV50TCL_BLEND_EQUATION_ALPHA_FUNC_ADD);
		OUT_RING  (sblend);
		BEGIN_RING(Nv3D, NV50TCL_BLEND_FUNC_DST_ALPHA, 1);
		OUT_RING  (dblend);
	}
}

Bool
NV50EXACheckComposite(int op,
		      PicturePtr pspict, PicturePtr pmpict, PicturePtr pdpict)
{
	if (!NV50EXACheckBlend(op))
		NOUVEAU_FALLBACK("blend not supported\n");

	if (!NV50EXACheckRenderTarget(pdpict))
		NOUVEAU_FALLBACK("render target invalid\n");

	if (!NV50EXACheckTexture(pspict))
		NOUVEAU_FALLBACK("src picture invalid\n");

	if (pmpict) {
		if (pmpict->componentAlpha &&
		    PICT_FORMAT_RGB(pmpict->format) &&
		    NV50EXABlendOp[op].src_alpha &&
		    NV50EXABlendOp[op].src_blend != 0x4000)
			NOUVEAU_FALLBACK("component-alpha not supported\n");

		if (!NV50EXACheckTexture(pmpict))
			NOUVEAU_FALLBACK("mask picture invalid\n");
	}

	return TRUE;
}

Bool
NV50EXAPrepareComposite(int op,
			PicturePtr pspict, PicturePtr pmpict, PicturePtr pdpict,
			PixmapPtr pspix, PixmapPtr pmpix, PixmapPtr pdpix)
{
	NV50EXA_LOCALS(pspix);

	if (!NV50EXARenderTarget(pdpix, pdpict))
		NOUVEAU_FALLBACK("render target invalid\n");

	NV50EXABlend(pdpix, pdpict, op, pmpict && pmpict->componentAlpha &&
		     PICT_FORMAT_RGB(pmpict->format));

	if (pmpict) {
		if (!NV50EXATexture(pspix, pspict, 0))
			NOUVEAU_FALLBACK("src picture invalid\n");
		if (!NV50EXATexture(pmpix, pmpict, 1))
			NOUVEAU_FALLBACK("mask picture invalid\n");
		state->have_mask = TRUE;

		BEGIN_RING(Nv3D, NV50TCL_FP_START_ID, 1);
		if (pdpict->format == PICT_a8) {
			OUT_RING(PFP_C_A8);
		} else {
			if (pmpict->componentAlpha &&
			    PICT_FORMAT_RGB(pmpict->format)) {
				if (NV50EXABlendOp[op].src_alpha)
					OUT_RING(PFP_CCASA);
				else
					OUT_RING(PFP_CCA);
			} else {
				OUT_RING(PFP_C);
			}
		}
	} else {
		if (!NV50EXATexture(pspix, pspict, 0))
			NOUVEAU_FALLBACK("src picture invalid\n");
		state->have_mask = FALSE;

		BEGIN_RING(Nv3D, NV50TCL_FP_START_ID, 1);
		if (pdpict->format == PICT_a8)
			OUT_RING(PFP_S_A8);
		else
			OUT_RING(PFP_S);
	}

	BEGIN_RING(Nv3D, 0x1334, 1);
	OUT_RING(0);

	BEGIN_RING(Nv3D, 0x1458, 1);
	OUT_RING(1);
	BEGIN_RING(Nv3D, 0x1458, 1);
	OUT_RING(0x203);

	return TRUE;
}

#define xFixedToFloat(v) \
	((float)xFixedToInt((v)) + ((float)xFixedFrac(v) / 65536.0))
static inline void
NV50EXATransform(PictTransformPtr t, int x, int y, float sx, float sy,
		 float *x_ret, float *y_ret)
{
	if (t) {
		PictVector v;

		v.vector[0] = IntToxFixed(x);
		v.vector[1] = IntToxFixed(y);
		v.vector[2] = xFixed1;
		PictureTransformPoint(t, &v);
		*x_ret = xFixedToFloat(v.vector[0]) / sx;
		*y_ret = xFixedToFloat(v.vector[1]) / sy;
	} else {
		*x_ret = (float)x / sx;
		*y_ret = (float)y / sy;
	}
}

void
NV50EXAComposite(PixmapPtr pdpix, int sx, int sy, int mx, int my,
		 int dx, int dy, int w, int h)
{
	NV50EXA_LOCALS(pdpix);
	float sX0, sX1, sX2, sX3, sY0, sY1, sY2, sY3;
	unsigned dX0 = dx, dX1 = dx + w, dY0 = dy, dY1 = dy + h;

	NV50EXATransform(state->unit[0].transform, sx, sy,
			 state->unit[0].width, state->unit[0].height,
			 &sX0, &sY0);
	NV50EXATransform(state->unit[0].transform, sx + w, sy,
			 state->unit[0].width, state->unit[0].height,
			 &sX1, &sY1);
	NV50EXATransform(state->unit[0].transform, sx + w, sy + h,
			 state->unit[0].width, state->unit[0].height,
			 &sX2, &sY2);
	NV50EXATransform(state->unit[0].transform, sx, sy + h,
			 state->unit[0].width, state->unit[0].height,
			 &sX3, &sY3);

	BEGIN_RING(Nv3D, NV50TCL_VERTEX_BEGIN, 1);
	OUT_RING  (NV50TCL_VERTEX_BEGIN_QUADS);
	if (state->have_mask) {
		float mX0, mX1, mX2, mX3, mY0, mY1, mY2, mY3;

		NV50EXATransform(state->unit[1].transform, mx, my,
				 state->unit[1].width, state->unit[1].height,
				 &mX0, &mY0);
		NV50EXATransform(state->unit[1].transform, mx + w, my,
				 state->unit[1].width, state->unit[1].height,
				 &mX1, &mY1);
		NV50EXATransform(state->unit[1].transform, mx + w, my + h,
				 state->unit[1].width, state->unit[1].height,
				 &mX2, &mY2);
		NV50EXATransform(state->unit[1].transform, mx, my + h,
				 state->unit[1].width, state->unit[1].height,
				 &mX3, &mY3);

		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2F_X(8), 4);
		OUT_RINGf (sX0); OUT_RINGf (sY0);
		OUT_RINGf (mX0); OUT_RINGf (mY0);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2I(0), 1);
		OUT_RING  ((dY0 << 16) | dX0);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2F_X(8), 4);
		OUT_RINGf (sX1); OUT_RINGf (sY1);
		OUT_RINGf (mX1); OUT_RINGf (mY1);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2I(0), 1);
		OUT_RING  ((dY0 << 16) | dX1);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2F_X(8), 4);
		OUT_RINGf (sX2); OUT_RINGf (sY2);
		OUT_RINGf (mX2); OUT_RINGf (mY2);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2I(0), 1);
		OUT_RING  ((dY1 << 16) | dX1);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2F_X(8), 4);
		OUT_RINGf (sX3); OUT_RINGf (sY3);
		OUT_RINGf (mX3); OUT_RINGf (mY3);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2I(0), 1);
		OUT_RING  ((dY1 << 16) | dX0);
	} else {
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2F_X(8), 2);
		OUT_RINGf (sX0); OUT_RINGf (sY0);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2I(0), 1);
		OUT_RING  ((dY0 << 16) | dX0);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2F_X(8), 2);
		OUT_RINGf (sX1); OUT_RINGf (sY1);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2I(0), 1);
		OUT_RING  ((dY0 << 16) | dX1);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2F_X(8), 2);
		OUT_RINGf (sX2); OUT_RINGf (sY2);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2I(0), 1);
		OUT_RING  ((dY1 << 16) | dX1);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2F_X(8), 2);
		OUT_RINGf (sX3); OUT_RINGf (sY3);
		BEGIN_RING(Nv3D, NV50TCL_VTX_ATTR_2I(0), 1);
		OUT_RING  ((dY1 << 16) | dX0);
	}
	BEGIN_RING(Nv3D, NV50TCL_VERTEX_END, 1);
	OUT_RING  (0);
}

void
NV50EXADoneComposite(PixmapPtr pdpix)
{
	NV50EXA_LOCALS(pdpix);

	FIRE_RING();
}

