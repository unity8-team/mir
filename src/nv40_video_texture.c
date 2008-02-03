/*
 * Copyright 2007-2008 Maarten Maathuis
 * Copyright 2008 Stephane Marchesin
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

/*
 * Implements the filtering as described in
 * "Fast Third-Order Texture Filtering"
 * Sigg & Hardwiger in GPU Gems 2
 */
static nv_shader_t nv40_yv12 = {
	.card_priv.NV30FP.num_regs = 4,
	.size = (29*4),
	.data = {
		/* INST 0: MOVR R0.xy (TR0.xyzw), attrib.texcoord[0] */
		0x01008600, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
		/* INST 1: ADDR R0.z (TR0.xyzw), R0.yyyy, { 0.50, 0.00, 0.00, 0.00 }.xxxx */
		0x03000800, 0x1c9caa00, 0x00000002, 0x0001c800,
		0x3f000000, 0x00000000, 0x00000000, 0x00000000,
		/* INST 2: ADDR R1.x (TR0.xyzw), R0, { 0.50, 0.00, 0.00, 0.00 }.xxxx */
		0x03000202, 0x1c9dc800, 0x00000002, 0x0001c800,
		0x3f000000, 0x00000000, 0x00000000, 0x00000000,
		/* INST 3: TEXRC0 R1.xyz (TR0.xyzw), R0.zzzz, texture[0] */
		0x17000f82, 0x1c9d5400, 0x0001c800, 0x0001c800,
		/* INST 4: MULR R2.yw (TR0.xyzw), R1.xxyy, { -1.00, 1.00, 0.00, 0.00 }.xxyy */
		0x02001404, 0x1c9ca104, 0x0000a002, 0x0001c800,
		0xbf800000, 0x3f800000, 0x00000000, 0x00000000,
		/* INST 5: TEXR R3.xyz (TR0.xyzw), R1, texture[0] */
		0x17000e86, 0x1c9dc804, 0x0001c800, 0x0001c800,
		/* INST 6: MULR R2.xz (TR0.xyzw), R3.xxyy, { -1.00, 1.00, 0.00, 0.00 }.xxyy */
		0x02000a04, 0x1c9ca10c, 0x0000a002, 0x0001c800,
		0xbf800000, 0x3f800000, 0x00000000, 0x00000000,
		/* INST 7: ADDR R2 (TR0.xyzw), R0.xyxy, R2 */
		0x03001e04, 0x1c9c8800, 0x0001c808, 0x0001c800,
		/* INST 8: TEXR R1.y (TR0.xyzw), R2.zwzz, -texture[1] */
		0x17020402, 0x1c9d5c08, 0x0001c800, 0x0001c800,
		/* INST 9: MADH R1.x (TR0.xyzw), -R1.zzzz, R1.yyyy, R1.yyyy */
		0x04400282, 0x1c9f5504, 0x0000aa04, 0x0000aa04,
		/* INST 10: TEXR R0.y (TR0.xyzw), R2.xwxw, -texture[1] */
		0x17020400, 0x1c9d9808, 0x0001c800, 0x0001c800,
		/* INST 11: MADH R0.w (TR0.xyzw), -R1.zzzz, R0.yyyy, R0.yyyy */
		0x04401080, 0x1c9f5504, 0x0000aa00, 0x0000aa00,
		/* INST 12: TEXR R0.x (TR0.xyzw), R2.zyxy, texture[1] */
		0x17020200, 0x1c9c8c08, 0x0001c800, 0x0001c800,
		/* INST 13: MADH R1.x (TR0.xyzw), R1.zzzz, R0, R1 */
		0x04400282, 0x1c9d5504, 0x0001c800, 0x0001c904,
		/* INST 14: TEXR R0.x (NE0.zzzz), R2, texture[1] */
		0x17020200, 0x1555c808, 0x0001c800, 0x0001c800,
		/* INST 15: MADH R0.x (TR0.xyzw), R1.zzzz, R0, R0.wwww */
		0x04400280, 0x1c9d5504, 0x0001c800, 0x0001ff00,
		/* INST 16: MADH R0.w (TR0.xyzw), -R3.zzzz, R1.xxxx, R1.xxxx */
		0x04401080, 0x1c9f550c, 0x00000104, 0x00000104,
		/* INST 17: TEXR R0.yz (TR0.xyzw), attrib.texcoord[1], abs(texture[2]) */
		0x1704ac80, 0x1c9dc801, 0x0001c800, 0x3fe1c800,
		/* INST 18: MADH R0.x (TR0.xyzw), R3.zzzz, R0, R0.wwww */
		0x04400280, 0x1c9d550c, 0x0001c900, 0x0001ff00,
		/* INST 19: MADH R1.xyz (TR0.xyzw), R0.xxxx, { 1.16, -0.87, 0.53, -1.08 }.xxxx, { 1.16, -0.87, 0.53, -1.08 }.yzww */
		0x04400e82, 0x1c9c0100, 0x00000002, 0x0001f202,
		0x3f9507c8, 0xbf5ee393, 0x3f078fef, 0xbf8a6762,
		/* INST 20: MADH R1.xyz (TR0.xyzw), R0.yyyy, { 0.00, -0.39, 2.02, 0.00 }, R1 */
		0x04400e82, 0x1c9cab00, 0x0001c802, 0x0001c904,
		0x00000000, 0xbec890d6, 0x40011687, 0x00000000,
		/* INST 21: MADH R0.xyz (TR0.xyzw), R0.zzzz, { 1.60, -0.81, 0.00, 0.00 }, R1 + END */
		0x04400e81, 0x1c9d5500, 0x0001c802, 0x0001c904,
		0x3fcc432d, 0xbf501a37, 0x00000000, 0x00000000,
	}
};

/*
 * The filtering function used for video scaling. We use a cubic filter as defined in 
 * "Reconstruction Filters in Computer Graphics"
 * Mitchell & Netravali in SIGGRAPH '88 
 */
static float filter_func(float x)
{
	const double B=0.75;
	const double C=(1.0-B)/2.0;
	double x1=fabs(x);
	double x2=fabs(x)*x1;
	double x3=fabs(x)*x2;

	if (fabs(x)<1.0) 
		return ( (12.0-9.0*B-6.0*C)*x3+(-18.0+12.0*B+6.0*C)*x2+(6.0-2.0*B) )/6.0; 
	else 
		return ( (-B-6.0*C)*x3+(6.0*B+30.0*C)*x2+(-12.0*B-48.0*C)*x1+(8.0*B+24.0*C) )/6.0;
}

static uint16_t f32tof16(float v)
{
	if (v==0)
		return 0;
	uint16_t sign=(v>0)?0x0:0x8000;
	v=fabs(v);
	int e=(int)floor(log2(v));
	uint16_t exponent=(e+15)<<10;
	uint16_t mantissa=(v/pow(2.0,(float)e)-1.0)*1024;
	return sign|exponent|mantissa;
}

static int8_t f32tosb8(float v)
{
	return (int8_t)(v*127.0);
}

/*
 * 128 means 512 bytes of VRAM
 */
#define TABLE_SIZE 512
static void compute_filter_table(int8_t *t) {
	int i;
	float x;
	for(i=0;i<TABLE_SIZE;i++) {
		x=(i+0.5)/TABLE_SIZE;

		float w0=filter_func(x+1.0);
		float w1=filter_func(x);
		float w2=filter_func(x-1.0);
		float w3=filter_func(x-2.0);

		t[4*i+2]=f32tosb8(1.0+x-w1/(w0+w1));
		t[4*i+1]=f32tosb8(1.0-x+w3/(w2+w3));
		t[4*i+0]=f32tosb8(w0+w1);
		t[4*i+3]=f32tosb8(0.0);
	}
}

static uint64_t NV40_LoadFilterTable(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static struct nouveau_bo *table_mem = NULL;

	if (!table_mem) {
		if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_GART,
				0, TABLE_SIZE*sizeof(float)*4, &table_mem)) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				"Couldn't alloc filter table!\n");
			return 0;
		}

		if (nouveau_bo_map(table_mem, NOUVEAU_BO_RDWR)) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Couldn't map filter table!\n");
		}

		int8_t *t=table_mem->map;
		compute_filter_table(t);
	}
	return table_mem->offset;
}

#define SWIZZLE(ts0x,ts0y,ts0z,ts0w,ts1x,ts1y,ts1z,ts1w)				\
	(										\
	NV40TCL_TEX_SWIZZLE_S0_X_##ts0x | NV40TCL_TEX_SWIZZLE_S0_Y_##ts0y	|	\
	NV40TCL_TEX_SWIZZLE_S0_Z_##ts0z | NV40TCL_TEX_SWIZZLE_S0_W_##ts0w	|	\
	NV40TCL_TEX_SWIZZLE_S1_X_##ts1x | NV40TCL_TEX_SWIZZLE_S1_Y_##ts1y 	|	\
	NV40TCL_TEX_SWIZZLE_S1_Z_##ts1z | NV40TCL_TEX_SWIZZLE_S1_W_##ts1w		\
	)

/*
 * Texture 0 : filter table
 * Texture 1 : Y data
 * Texture 2 : UV data
 */
static Bool
NV40VideoTexture(ScrnInfoPtr pScrn, int offset, uint16_t width, uint16_t height, uint16_t src_pitch, int unit)
{
	NVPtr pNv = NVPTR(pScrn);

	uint32_t card_fmt = 0;
	uint32_t card_swz = 0;

	switch(unit) {
		case 0:
		card_fmt = NV40TCL_TEX_FORMAT_FORMAT_A8R8G8B8;
		card_swz = SWIZZLE(S1, S1, S1, S1, X, Y, Z, W);
		break;
		case 1:
		card_fmt = NV40TCL_TEX_FORMAT_FORMAT_L8;
		card_swz = SWIZZLE(S1, S1, S1, S1, X, X, X, X);
		break;
		case 2:
		card_fmt = NV40TCL_TEX_FORMAT_FORMAT_A8L8;
		card_swz = SWIZZLE(S1, S1, S1, S1, W, Z, Y, X); /* x = V, y = U */
		break;
	}

	BEGIN_RING(Nv3D, NV40TCL_TEX_OFFSET(unit), 8);
	/* We get an absolute offset, which needs to be corrected. */
	OUT_RELOCl(pNv->FB, (uint32_t)(offset - pNv->FB->offset), NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
	if (unit==0) {
		OUT_RELOCd(pNv->FB, card_fmt | 
				NV40TCL_TEX_FORMAT_DIMS_1D | NV40TCL_TEX_FORMAT_NO_BORDER |
				(0x8000) | (1 << NV40TCL_TEX_FORMAT_MIPMAP_COUNT_SHIFT),
				NOUVEAU_BO_VRAM | NOUVEAU_BO_RD,
				NV40TCL_TEX_FORMAT_DMA0, 0);
		OUT_RING(NV40TCL_TEX_WRAP_S_REPEAT |
				NV40TCL_TEX_WRAP_T_CLAMP_TO_EDGE |
				NV40TCL_TEX_WRAP_R_CLAMP_TO_EDGE);
	} else {
		OUT_RELOCd(pNv->FB, card_fmt | NV40TCL_TEX_FORMAT_LINEAR | NV40TCL_TEX_FORMAT_RECT |
				NV40TCL_TEX_FORMAT_DIMS_2D | NV40TCL_TEX_FORMAT_NO_BORDER |
				(0x8000) | (1 << NV40TCL_TEX_FORMAT_MIPMAP_COUNT_SHIFT),
				NOUVEAU_BO_VRAM | NOUVEAU_BO_RD,
				NV40TCL_TEX_FORMAT_DMA0, 0);
		OUT_RING(NV40TCL_TEX_WRAP_S_CLAMP_TO_EDGE |
				NV40TCL_TEX_WRAP_T_CLAMP_TO_EDGE |
				NV40TCL_TEX_WRAP_R_CLAMP_TO_EDGE);
	}

	OUT_RING(NV40TCL_TEX_ENABLE_ENABLE);
	OUT_RING(card_swz);
	if (unit==0)
		OUT_RING(0xF0000000|NV40TCL_TEX_FILTER_MIN_NEAREST |
				NV40TCL_TEX_FILTER_MAG_NEAREST |
				0x3fd6);
	else
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

/* To support EXA 2.0, 2.1 has this in the header */
#ifndef exaMoveInPixmap
extern void exaMoveInPixmap(PixmapPtr pPixmap);
#endif

#define SF(bf) (NV40TCL_BLEND_FUNC_SRC_RGB_##bf |                              \
		NV40TCL_BLEND_FUNC_SRC_ALPHA_##bf)
#define DF(bf) (NV40TCL_BLEND_FUNC_DST_RGB_##bf |                              \
		NV40TCL_BLEND_FUNC_DST_ALPHA_##bf)

#define VERTEX_OUT(sx,sy,dx,dy) do {                                           \
	BEGIN_RING(Nv3D, NV40TCL_VTX_ATTR_2F_X(8), 4);                         \
	OUT_RINGf ((sx)); OUT_RINGf ((sy));                                    \
	OUT_RINGf ((sx)/2.0); OUT_RINGf ((sy)/2.0);                            \
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
	NVPortPrivPtr  pPriv = GET_TEXTURED_PRIVATE(pNv);
	Bool redirected = FALSE;

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
	PixmapPtr pPix = exaGetDrawablePixmap(pDraw);
	BoxPtr pbox;
	int nbox;
	int dst_format = 0;
	uint64_t filter_table_offset=0;

	if (!NV40GetSurfaceFormat(pPix, &dst_format)) {
		ErrorF("No surface format, bad.\n");
	}

	/* This has to be called always, since it does more than just migration. */
	exaMoveInPixmap(pPix);
	ExaOffscreenMarkUsed(pPix);

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

	/* I suspect that pDraw itself is not offscreen, hence not suited for damage tracking. */
	DamageDamageRegion(&pPix->drawable, clipBoxes);

	/* This is test is unneeded for !COMPOSITE. */
	if (!NVExaPixmapIsOnscreen(pPix))
		redirected = TRUE;
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

	filter_table_offset=NV40_LoadFilterTable(pScrn);

	NV40VideoTexture(pScrn, filter_table_offset, TABLE_SIZE, 1, 0 , 0);
	NV40VideoTexture(pScrn, src_offset, src_w, src_h, src_pitch, 1);
	/* We've got NV12 format, which means half width and half height texture of chroma channels. */
	NV40VideoTexture(pScrn, src_offset2, src_w/2, src_h/2, src_pitch, 2);

	NV40_LoadVtxProg(pScrn, &nv40_video);
	NV40_LoadFragProg(pScrn, &nv40_yv12);

	/* Appears to be some kind of cache flush, needed here at least
	 * sometimes.. funky text rendering otherwise :)
	 */
	BEGIN_RING(Nv3D, NV40TCL_TEX_CACHE_CTL, 1);
	OUT_RING  (2);
	BEGIN_RING(Nv3D, NV40TCL_TEX_CACHE_CTL, 1);
	OUT_RING  (1);

	/* Just before rendering we wait for vblank in the non-composited case. */
	if (pPriv->SyncToVBlank && !redirected) {
		uint8_t crtcs = nv_window_belongs_to_crtc(pScrn, dstBox->x1, dstBox->y1,
			dstBox->x2 - dstBox->x1, dstBox->y2 - dstBox->y1);

		FIRE_RING();
		if (crtcs & 0x1)
			NVWaitVSync(pScrn, 0);
		else if (crtcs & 0x2)
			NVWaitVSync(pScrn, 1);
	}

	/* These are fixed point values in the 16.16 format. */
	X1 = (float)(x1>>16)+(float)(x1&0xFFFF)/(float)0xFFFF;
	Y1 = (float)(y1>>16)+(float)(y1&0xFFFF)/(float)0xFFFF;
	X2 = (float)(x2>>16)+(float)(x2&0xFFFF)/(float)0xFFFF;
	Y2 = (float)(y2>>16)+(float)(y2&0xFFFF)/(float)0xFFFF;

	BEGIN_RING(Nv3D, NV40TCL_BEGIN_END, 1);
	OUT_RING  (NV40TCL_BEGIN_END_TRIANGLES);

	while(nbox--) {
		float tx1=X1+(float)(pbox->x1 - dstBox->x1)/(float)drw_w*(X2-X1);
		float tx2=X1+(float)(pbox->x2 - dstBox->x1)/(float)drw_w*(X2-X1);
		float ty1=Y1+(float)(pbox->y1 - dstBox->y1)/(float)drw_h*(Y2-Y1);
		float ty2=Y1+(float)(pbox->y2 - dstBox->y1)/(float)drw_h*(Y2-Y1);
		int sx1=pbox->x1;
		int sx2=pbox->x2;
		int sy1=pbox->y1;
		int sy2=pbox->y2;

		BEGIN_RING(Nv3D, NV40TCL_SCISSOR_HORIZ, 2);
		OUT_RING  ((sx2 << 16) | 0);
		OUT_RING  ((sy2 << 16) | 0);

		VERTEX_OUT(tx1, ty1, sx1, sy1);
		VERTEX_OUT(tx2+(tx2-tx1), ty1, sx2+(sx2-sx1), sy1);
		VERTEX_OUT(tx1, ty2+(ty2-ty1), sx1, sy2+(sy2-sy1));

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

