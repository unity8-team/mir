/*
 * Copyright 2007 Maarten Maathuis
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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

#include "nv_shaders.h"

extern Atom xvSyncToVBlank, xvSetDefaults;

static nv_shader_t nv40_video = {
	.card_priv.NV30VP.vp_in_reg  = 0x00000309,
	.card_priv.NV30VP.vp_out_reg = 0x0000c001,
	.size = (3*4),
	.data = {
		/* MOV result.position, vertex.position */
		0x40041c6c, 0x0040000d, 0x8106c083, 0x6041ff80,
		/* MOV result.texcoord[0], vertex.texcoord[0] */
		0x401f9c6c, 0x0040080d, 0x8106c083, 0x6041ff9c,
		/* MOV result.texcoord[1], vertex.texcoord[1] */
		0x401f9c6c, 0x0040090d, 0x8106c083, 0x6041ffa1,
	}
};

static nv_shader_t nv40_yv12 = {
	.card_priv.NV30FP.num_regs = 2,
	.size = (8*4),
	.data = {
		/* INST 0: TEXR R0.x (TR0.xyzw), attrib.texcoord[0], abs(texture[0]) */
		0x17008200, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
		/* INST 1: MADR R1.xyz (TR0.xyzw), R0.xxxx, { 1.16, -0.87, 0.53, -1.08 }.xxxx, { 1.16, -0.87, 0.53, -1.08 }.yzww */
		0x04000e02, 0x1c9c0000, 0x00000002, 0x0001f202,
		0x3f9507c8, 0xbf5ee393, 0x3f078fef, 0xbf8a6762,
		/* INST 2: TEXR R0.yz (TR0.xyzw), attrib.texcoord[1], abs(texture[1]) */
		0x1702ac80, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
		/* INST 3: MADR R1.xyz (TR0.xyzw), R0.yyyy, { 0.00, -0.39, 2.02, 0.00 }, R1 */
		0x04000e02, 0x1c9cab00, 0x0001c802, 0x0001c804,
		0x00000000, 0xbec890d6, 0x40011687, 0x00000000,
		/* INST 4: MADR R0.xyz (TR0.xyzw), R0.zzzz, { 1.60, -0.81, 0.00, 0.00 }, R1 + END */
		0x04000e81, 0x1c9d5500, 0x0001c802, 0x0001c804,
		0x3fcc432d, 0xbf501a37, 0x00000000, 0x00000000,
	}
};

#define SWIZZLE(ts0x,ts0y,ts0z,ts0w,ts1x,ts1y,ts1z,ts1w)							\
	(																	\
	NV40TCL_TEX_SWIZZLE_S0_X_##ts0x | NV40TCL_TEX_SWIZZLE_S0_Y_##ts0y		|	\
	NV40TCL_TEX_SWIZZLE_S0_Z_##ts0z | NV40TCL_TEX_SWIZZLE_S0_W_##ts0w	|	\
	NV40TCL_TEX_SWIZZLE_S1_X_##ts1x | NV40TCL_TEX_SWIZZLE_S1_Y_##ts1y 	|	\
	NV40TCL_TEX_SWIZZLE_S1_Z_##ts1z | NV40TCL_TEX_SWIZZLE_S1_W_##ts1w		\
	)

static Bool
NV40VideoTexture(ScrnInfoPtr pScrn, int offset, uint16_t width, uint16_t height, uint16_t src_pitch, int unit)
{
	NVPtr pNv = NVPTR(pScrn);

	uint32_t card_fmt = 0;
	uint32_t card_swz = 0;

	if (unit == 0) {
		/* Pretend we've got a normal 8 bits format. */
		card_fmt = NV40TCL_TEX_FORMAT_FORMAT_L8;
		card_swz = SWIZZLE(S1, S1, S1, S1, X, X, X, X);
	} else {
		/* Pretend we've got a normal 2x8 bits format. */
		card_fmt = NV40TCL_TEX_FORMAT_FORMAT_A8L8;
		card_swz = SWIZZLE(S1, S1, S1, S1, W, Z, Y, X); /* x = V, y = U */
	}

	BEGIN_RING(Nv3D, NV40TCL_TEX_OFFSET(unit), 8);
	/* We get an obsolute offset, which needs to be corrected. */
	OUT_RELOCl(pNv->FB, (uint32_t)(offset - pNv->FB->offset), NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	OUT_RELOCd(pNv->FB, card_fmt | NV40TCL_TEX_FORMAT_LINEAR |
			NV40TCL_TEX_FORMAT_DIMS_2D | NV40TCL_TEX_FORMAT_NO_BORDER |
			(0x8000) | (1 << NV40TCL_TEX_FORMAT_MIPMAP_COUNT_SHIFT),
			NOUVEAU_BO_VRAM | NOUVEAU_BO_RD,
			NV40TCL_TEX_FORMAT_DMA0, 0);

	OUT_RING(NV40TCL_TEX_WRAP_S_CLAMP_TO_EDGE |
			NV40TCL_TEX_WRAP_T_CLAMP_TO_EDGE |
			NV40TCL_TEX_WRAP_R_CLAMP_TO_EDGE);
	OUT_RING(NV40TCL_TEX_ENABLE_ENABLE);
	OUT_RING(card_swz);
	OUT_RING(NV40TCL_TEX_FILTER_MIN_LINEAR |
			NV40TCL_TEX_FILTER_MAG_LINEAR |
			0x3fd6);
	OUT_RING((width << 16) | height);
	OUT_RING(0); /* border ARGB */
	BEGIN_RING(Nv3D, NV40TCL_TEX_SIZE1(unit), 1);
	OUT_RING((1 << NV40TCL_TEX_SIZE1_DEPTH_SHIFT) |
			(uint16_t) src_pitch);

	return TRUE;
}

Bool
NV40GetSurfaceFormat(PixmapPtr pPix, int *fmt_ret)
{
	switch (pPix->drawable.bitsPerPixel) {
		case 32:
			*fmt_ret = NV40TCL_RT_FORMAT_COLOR_A8R8G8B8;
			break;
		case 24:
			*fmt_ret = NV40TCL_RT_FORMAT_COLOR_X8R8G8B8;
			break;
		case 16:
			*fmt_ret = NV40TCL_RT_FORMAT_COLOR_R5G6B5;
			break;
		case 8:
			*fmt_ret = NV40TCL_RT_FORMAT_COLOR_B8;
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

void
NV40StopTexturedVideo(ScrnInfoPtr pScrn, pointer data, Bool Exit)
{
}

#ifndef ExaOffscreenMarkUsed
extern void ExaOffscreenMarkUsed(PixmapPtr);
#endif
#ifndef exaGetDrawablePixmap
extern PixmapPtr exaGetDrawablePixmap(DrawablePtr);
#endif
#ifndef exaPixmapIsOffscreen
extern Bool exaPixmapIsOffscreen(PixmapPtr p);
#endif
/* To support EXA 2.0, 2.1 has this in the header */
#ifndef exaMoveInPixmap
extern void exaMoveInPixmap(PixmapPtr pPixmap);
#endif

#define SF(bf) (NV40TCL_BLEND_FUNC_SRC_RGB_##bf |                              \
		NV40TCL_BLEND_FUNC_SRC_ALPHA_##bf)
#define DF(bf) (NV40TCL_BLEND_FUNC_DST_RGB_##bf |                              \
		NV40TCL_BLEND_FUNC_DST_ALPHA_##bf)

#define VERTEX_OUT(sx,sy,dx,dy) do {                                        \
	BEGIN_RING(Nv3D, NV40TCL_VTX_ATTR_2F_X(8), 4);                         \
	OUT_RINGf ((sx)); OUT_RINGf ((sy));                                    \
	OUT_RINGf ((sx)); OUT_RINGf ((sy));                                    \
	BEGIN_RING(Nv3D, NV40TCL_VTX_ATTR_2I(0), 1);                           \
	OUT_RING  (((dy)<<16)|(dx));                                           \
} while(0)

#define GET_TEXTURED_PRIVATE(pNv) \
	(NVPortPrivPtr)((pNv)->blitAdaptor->pPortPrivates[0].ptr)

int NV40PutTextureImage(ScrnInfoPtr pScrn, int src_offset,
		int src_offset2, int id,
		int src_pitch, BoxPtr dstBox,
		int x1, int y1, int x2, int y2,
		uint16_t width, uint16_t height,
		uint16_t src_w, uint16_t src_h,
		uint16_t drw_w, uint16_t drw_h,
		RegionPtr clipBoxes,
		DrawablePtr pDraw)
{
	NVPtr          pNv   = NVPTR(pScrn);
	//NVPortPrivPtr  pPriv = GET_TEXTURED_PRIVATE(pNv);

	/* Remove some warnings. */
	/* This has to be done better at some point. */
	(void)nv40_vp_exa_render;
	(void)nv30_fp_pass_col0;
	(void)nv30_fp_pass_tex0;
	(void)nv30_fp_composite_mask;
	(void)nv30_fp_composite_mask_sa_ca;
	(void)nv30_fp_composite_mask_ca;

	if (drw_w > 4096 || drw_h > 4096) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"XV: Draw size too large.\n");
		return BadAlloc;
	}

	float X1, X2, Y1, Y2;
	float scaleX1, scaleX2, scaleY1, scaleY2;
	float scaleX, scaleY;
	PixmapPtr pPix = exaGetDrawablePixmap(pDraw);
	BoxPtr pbox;
	int nbox;
	int dst_format = 0;
	if (!NV40GetSurfaceFormat(pPix, &dst_format)) {
		ErrorF("No surface format, bad.\n");
	}

	/* Try to get the dest drawable into vram */
	if (!exaPixmapIsOffscreen(pPix)) {
		exaMoveInPixmap(pPix);
		ExaOffscreenMarkUsed(pPix);
	}

	/* Fail if we can't move the pixmap into memory. */
	if (!exaPixmapIsOffscreen(pPix)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"XV: couldn't move dst surface into vram.\n");
		return BadAlloc;
	}

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

	/* Disable blending */
	BEGIN_RING(Nv3D, NV40TCL_BLEND_ENABLE, 1);
	OUT_RING(0);

	/* Setup surface */
	BEGIN_RING(Nv3D, NV40TCL_RT_FORMAT, 3);
	OUT_RING  (NV40TCL_RT_FORMAT_TYPE_LINEAR |
			NV40TCL_RT_FORMAT_ZETA_Z24S8 |
			dst_format);
	OUT_RING  (exaGetPixmapPitch(pPix));
	OUT_PIXMAPl(pPix, 0, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);

	NV40VideoTexture(pScrn, src_offset, src_w, src_h, src_pitch, 0);
	/* We've got NV12 format, which means half width and half height texture of chroma channels. */
	NV40VideoTexture(pScrn, src_offset2, src_w/2, src_h/2, src_pitch, 1);

	NV40_LoadVtxProg(pScrn, &nv40_video);
	NV40_LoadFragProg(pScrn, &nv40_yv12);

	/* Appears to be some kind of cache flush, needed here at least
	 * sometimes.. funky text rendering otherwise :)
	 */
	BEGIN_RING(Nv3D, NV40TCL_TEX_CACHE_CTL, 1);
	OUT_RING  (2);
	BEGIN_RING(Nv3D, NV40TCL_TEX_CACHE_CTL, 1);
	OUT_RING  (1);

	/* These are fixed point values in the 16.16 format. */
	x1 >>= 16;
	x2 >>= 16;
	y1 >>= 16;
	y2 >>= 16;

	X1 = (float)x1/(float)src_w;
	Y1 = (float)y1/(float)src_h;
	X2 = (float)x2/(float)src_w;
	Y2 = (float)y2/(float)src_h;

	/* The corrections here are emperical, i tried to explain them as best as possible. */

	/* This correction is need for when the image clips the screen at the right or bottom. */
	/* In this case x2 and/or y2 is adjusted for the clipping, otherwise not. */
	/* Otherwise the lower right coordinate stretches in the clipping direction. */
	scaleX = (float)src_w/(float)(x2 - x1);
	scaleY = (float)src_h/(float)(y2 - y1);

	BEGIN_RING(Nv3D, NV40TCL_BEGIN_END, 1);
	OUT_RING  (NV40TCL_BEGIN_END_QUADS);

	while(nbox--) {

		/* The src coordinates needs to be scaled to the draw size. */
		scaleX1 = (float)(pbox->x1 - dstBox->x1)/(float)drw_w;
		scaleX2 = (float)(pbox->x2 - dstBox->x1)/(float)drw_w;
		scaleY1 = (float)(pbox->y1 - dstBox->y1)/(float)drw_h;
		scaleY2 = (float)(pbox->y2 - dstBox->y1)/(float)drw_h;

		/* Submit the appropriate vertices. */
		/* This submits the same vertices for the Y and the UV texture. */
		VERTEX_OUT(X1 + (X2 - X1) * scaleX1 * scaleX, Y1 + (Y2 - Y1) * scaleY1 * scaleY, pbox->x1, pbox->y1);
		VERTEX_OUT(X1 + (X2 - X1) * scaleX2 * scaleX, Y1 + (Y2 - Y1) * scaleY1 * scaleY, pbox->x2, pbox->y1);
		VERTEX_OUT(X1 + (X2 - X1) * scaleX2 * scaleX, Y1 + (Y2 - Y1) * scaleY2 * scaleY, pbox->x2, pbox->y2);
		VERTEX_OUT(X1 + (X2 - X1) * scaleX1 * scaleX, Y1 + (Y2 - Y1) * scaleY2 * scaleY, pbox->x1, pbox->y2);

		pbox++;
	}

	BEGIN_RING(Nv3D, NV40TCL_BEGIN_END, 1);
	OUT_RING  (NV40TCL_BEGIN_END_STOP);

	FIRE_RING();

	return Success;
}

/**
 * NVSetTexturePortAttribute
 * sets the attribute "attribute" of port "data" to value "value"
 * supported attributes:
 * Sync to vblank.
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
NVSetTexturePortAttribute(ScrnInfoPtr pScrn, Atom attribute,
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
 * NVGetTexturePortAttribute
 * reads the value of attribute "attribute" from port "data" into INT32 "*value"
 * Sync to vblank.
 * 
 * @param pScrn unused
 * @param attribute attribute to be read
 * @param value value of attribute will be stored here
 * @param data port from which attribute will be read
 * @return Success, if queried attribute exists
 */
int
NVGetTexturePortAttribute(ScrnInfoPtr pScrn, Atom attribute,
                       INT32 *value, pointer data)
{
        NVPortPrivPtr pPriv = (NVPortPrivPtr)data;

        if(attribute == xvSyncToVBlank)
                *value = (pPriv->SyncToVBlank) ? 1 : 0;
        else
                return BadMatch;

        return Success;
}

