/*
 * Copyright 2007 Ben Skeggs
 * Copyright 2007 Stephane Marchesin
 * Copyright 2007 Jeremy Kolb
 * Copyright 2007 Patrice Mandin
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
#include "nv30_shaders.h"

typedef struct nv_pict_surface_format {
	int	 pict_fmt;
	uint32_t card_fmt;
} nv_pict_surface_format_t;

typedef struct nv_pict_texture_format {
	int	 pict_fmt;
	uint32_t card_fmt;
	uint32_t card_swz;
} nv_pict_texture_format_t;

typedef struct nv_pict_op {
	Bool	 src_alpha;
	Bool	 dst_alpha;
	uint32_t src_card_op;
	uint32_t dst_card_op;
} nv_pict_op_t;

typedef struct nv30_exa_state {
	Bool have_mask;

	struct {
		PictTransformPtr transform;
		float width;
		float height;
	} unit[2];
} nv30_exa_state_t;
static nv30_exa_state_t exa_state;
#define NV30EXA_STATE nv30_exa_state_t *state = &exa_state

static nv_pict_surface_format_t
NV30SurfaceFormat[] = {
	{ PICT_a8r8g8b8	, 0x148 },
	{ PICT_a8b8g8r8	, 0x150 },
	{ PICT_x8r8g8b8	, 0x145 },
	{ PICT_x8b8g8r8	, 0x14f },
	{ PICT_r5g6b5	, 0x143 },
	{ PICT_a8       , 0x149 },
};

static nv_pict_surface_format_t *
NV30_GetPictSurfaceFormat(int format)
{
	int i;

	for(i=0;i<sizeof(NV30SurfaceFormat)/sizeof(NV30SurfaceFormat[0]);i++)
	{
		if (NV30SurfaceFormat[i].pict_fmt == format)
			return &NV30SurfaceFormat[i];
	}

	return NULL;
}

enum {
	NV30EXA_FPID_PASS_COL0 = 0,
	NV30EXA_FPID_PASS_TEX0 = 1,
	NV30EXA_FPID_COMPOSITE_MASK = 2,
	NV30EXA_FPID_COMPOSITE_MASK_SA_CA = 3,
	NV30EXA_FPID_COMPOSITE_MASK_CA = 4,
	NV30EXA_FPID_MAX = 5
} NV30EXA_FPID;

static nv_shader_t *nv40_fp_map[NV30EXA_FPID_MAX] = {
	&nv30_fp_pass_col0,
	&nv30_fp_pass_tex0,
	&nv30_fp_composite_mask,
	&nv30_fp_composite_mask_sa_ca,
	&nv30_fp_composite_mask_ca
};

static nv_shader_t *nv40_fp_map_a8[NV30EXA_FPID_MAX];

static void
NV30EXAHackupA8Shaders(ScrnInfoPtr pScrn)
{
	int s;

	for (s = 0; s < NV30EXA_FPID_MAX; s++) {
		nv_shader_t *def, *a8;

		def = nv40_fp_map[s];
		a8 = xcalloc(1, sizeof(nv_shader_t));
		a8->card_priv.NV30FP.num_regs = def->card_priv.NV30FP.num_regs;
		a8->size = def->size + 4;
		memcpy(a8->data, def->data, def->size * sizeof(uint32_t));
		nv40_fp_map_a8[s] = a8;

		a8->data[a8->size - 8 + 0] &= ~0x00000081;
		a8->data[a8->size - 4 + 0]  = 0x01401e81;
		a8->data[a8->size - 4 + 1]  = 0x1c9dfe00;
		a8->data[a8->size - 4 + 2]  = 0x0001c800;
		a8->data[a8->size - 4 + 3]  = 0x0001c800;
	}
}

/* should be in nouveau_reg.h at some point.. */
#define NV34TCL_TX_SWIZZLE_UNIT_S0_X_SHIFT	14
#define NV34TCL_TX_SWIZZLE_UNIT_S0_X_ZERO	 0
#define NV34TCL_TX_SWIZZLE_UNIT_S0_X_ONE	 1
#define NV34TCL_TX_SWIZZLE_UNIT_S0_X_S1		 2
#define NV34TCL_TX_SWIZZLE_UNIT_S0_Y_SHIFT	12
#define NV34TCL_TX_SWIZZLE_UNIT_S0_Z_SHIFT	10
#define NV34TCL_TX_SWIZZLE_UNIT_S0_W_SHIFT	 8
#define NV34TCL_TX_SWIZZLE_UNIT_S1_X_SHIFT	 6
#define NV34TCL_TX_SWIZZLE_UNIT_S1_X_X		 3
#define NV34TCL_TX_SWIZZLE_UNIT_S1_X_Y		 2
#define NV34TCL_TX_SWIZZLE_UNIT_S1_X_Z		 1
#define NV34TCL_TX_SWIZZLE_UNIT_S1_X_W		 0
#define NV34TCL_TX_SWIZZLE_UNIT_S1_Y_SHIFT	 4
#define NV34TCL_TX_SWIZZLE_UNIT_S1_Z_SHIFT	 2
#define NV34TCL_TX_SWIZZLE_UNIT_S1_W_SHIFT	 0

#define _(r,tf,ts0x,ts0y,ts0z,ts0w,ts1x,ts1y,ts1z,ts1w)                       \
  {                                                                           \
  PICT_##r,                                                                   \
  (tf),                                                                       \
  (NV34TCL_TX_SWIZZLE_UNIT_S0_X_##ts0x << NV34TCL_TX_SWIZZLE_UNIT_S0_X_SHIFT)|\
  (NV34TCL_TX_SWIZZLE_UNIT_S0_X_##ts0y << NV34TCL_TX_SWIZZLE_UNIT_S0_Y_SHIFT)|\
  (NV34TCL_TX_SWIZZLE_UNIT_S0_X_##ts0z << NV34TCL_TX_SWIZZLE_UNIT_S0_Z_SHIFT)|\
  (NV34TCL_TX_SWIZZLE_UNIT_S0_X_##ts0w << NV34TCL_TX_SWIZZLE_UNIT_S0_W_SHIFT)|\
  (NV34TCL_TX_SWIZZLE_UNIT_S1_X_##ts1x << NV34TCL_TX_SWIZZLE_UNIT_S1_X_SHIFT)|\
  (NV34TCL_TX_SWIZZLE_UNIT_S1_X_##ts1y << NV34TCL_TX_SWIZZLE_UNIT_S1_Y_SHIFT)|\
  (NV34TCL_TX_SWIZZLE_UNIT_S1_X_##ts1z << NV34TCL_TX_SWIZZLE_UNIT_S1_Z_SHIFT)|\
  (NV34TCL_TX_SWIZZLE_UNIT_S1_X_##ts1w << NV34TCL_TX_SWIZZLE_UNIT_S1_W_SHIFT)\
  }

static nv_pict_texture_format_t
NV30TextureFormat[] = {
	_(a8r8g8b8, 0x12,   S1,   S1,   S1,   S1, X, Y, Z, W),
	_(a8b8g8r8, 0x12,   S1,   S1,   S1,   S1, Z, Y, X, W),
	_(x8r8g8b8, 0x12,   S1,   S1,   S1,  ONE, X, Y, Z, W),
	_(x8b8g8r8, 0x12,   S1,   S1,   S1,  ONE, Z, Y, X, W),
	_(a1r5g5b5, 0x10,   S1,   S1,   S1,   S1, X, Y, Z, W),
	_(x1r5g5b5, 0x10,   S1,   S1,   S1,  ONE, X, Y, Z, W),
	_(x4r4g4b4, 0x1d,   S1,   S1,   S1,  ONE, X, Y, Z, W),
	_(a4r4g4b4, 0x1d,   S1,   S1,   S1,   S1, X, Y, Z, W),
	_(      a8, 0x1b, ZERO, ZERO, ZERO,   S1, X, X, X, X),
};


static nv_pict_texture_format_t *
NV30_GetPictTextureFormat(int format)
{
	int i;

	for(i=0;i<sizeof(NV30TextureFormat)/sizeof(NV30TextureFormat[0]);i++)
	{
		if (NV30TextureFormat[i].pict_fmt == format)
			return &NV30TextureFormat[i];
	}

	return NULL;
}

#define NV34TCL_BF_ZERO                                     0x0000
#define NV34TCL_BF_ONE                                      0x0001
#define NV34TCL_BF_SRC_COLOR                                0x0300
#define NV34TCL_BF_ONE_MINUS_SRC_COLOR                      0x0301
#define NV34TCL_BF_SRC_ALPHA                                0x0302
#define NV34TCL_BF_ONE_MINUS_SRC_ALPHA                      0x0303
#define NV34TCL_BF_DST_ALPHA                                0x0304
#define NV34TCL_BF_ONE_MINUS_DST_ALPHA                      0x0305
#define NV34TCL_BF_DST_COLOR                                0x0306
#define NV34TCL_BF_ONE_MINUS_DST_COLOR                      0x0307
#define NV34TCL_BF_ALPHA_SATURATE                           0x0308
#define BF(bf) NV34TCL_BF_##bf

static nv_pict_op_t 
NV30PictOp[] = {
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
/* Add         */ { 0, 0, BF(                ONE), BF(                ONE) }
};

static nv_pict_op_t *
NV30_GetPictOpRec(int op)
{
	if (op >= PictOpSaturate)
		return NULL;
#if 0
	switch(op)
	{
		case 0:ErrorF("Op Clear\n");break;
		case 1:ErrorF("Op Src\n");break;
		case 2:ErrorF("Op Dst\n");break;
		case 3:ErrorF("Op Over\n");break;
		case 4:ErrorF("Op OverReverse\n");break;
		case 5:ErrorF("Op In\n");break;
		case 6:ErrorF("Op InReverse\n");break;
		case 7:ErrorF("Op Out\n");break;
		case 8:ErrorF("Op OutReverse\n");break;
		case 9:ErrorF("Op Atop\n");break;
		case 10:ErrorF("Op AtopReverse\n");break;
		case 11:ErrorF("Op Xor\n");break;
		case 12:ErrorF("Op Add\n");break;
	}
#endif
	return &NV30PictOp[op];
}

static void
NV30_SetupBlend(ScrnInfoPtr pScrn, nv_pict_op_t *blend,
		PictFormatShort dest_format, Bool component_alpha)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *rankine = pNv->Nv3D;
	uint32_t sblend, dblend;

	sblend = blend->src_card_op;
	dblend = blend->dst_card_op;

	if (blend->dst_alpha) {
		if (!PICT_FORMAT_A(dest_format)) {
			if (sblend == BF(DST_ALPHA)) {
				sblend = BF(ONE);
			} else if (sblend == BF(ONE_MINUS_DST_ALPHA)) {
				sblend = BF(ZERO);
			}
		} else if (dest_format == PICT_a8) {
			if (sblend == BF(DST_ALPHA)) {
				sblend = BF(DST_COLOR);
			} else if (sblend == BF(ONE_MINUS_DST_ALPHA)) {
				sblend = BF(ONE_MINUS_DST_COLOR);
			}
		}
	}

	if (blend->src_alpha && (component_alpha || dest_format == PICT_a8)) {
		if (dblend == BF(SRC_ALPHA)) {
			dblend = BF(SRC_COLOR);
		} else if (dblend == BF(ONE_MINUS_SRC_ALPHA)) {
			dblend = BF(ONE_MINUS_SRC_COLOR);
		}
	}

	if (sblend == BF(ONE) && dblend == BF(ZERO)) {
		BEGIN_RING(chan, rankine, NV34TCL_BLEND_FUNC_ENABLE, 1);
		OUT_RING  (chan, 0);
	} else {
		BEGIN_RING(chan, rankine, NV34TCL_BLEND_FUNC_ENABLE, 3);
		OUT_RING  (chan, 1);
		OUT_RING  (chan, (sblend << 16) | sblend);
		OUT_RING  (chan, (dblend << 16) | dblend);
	}
}

static Bool
NV30EXATexture(ScrnInfoPtr pScrn, PixmapPtr pPix, PicturePtr pPict, int unit)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *rankine = pNv->Nv3D;
	struct nouveau_bo *bo = nouveau_pixmap_bo(pPix);
	unsigned delta = nouveau_pixmap_offset(pPix);
	nv_pict_texture_format_t *fmt;
	uint32_t card_filter, card_repeat;
	NV30EXA_STATE;

	fmt = NV30_GetPictTextureFormat(pPict->format);
	if (!fmt)
		return FALSE;

	card_repeat = 3; /* repeatNone */

	if (pPict->filter == PictFilterBilinear)
		card_filter = 2;
	else
		card_filter = 1;

	BEGIN_RING(chan, rankine, NV34TCL_TX_OFFSET(unit), 8);
	OUT_RELOCl(chan, bo, delta, NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);

	OUT_RING  (chan, NV34TCL_TX_FORMAT_DIMS_2D |
			(fmt->card_fmt << NV34TCL_TX_FORMAT_FORMAT_SHIFT) |
			(1 << 16) |
			(log2i(pPix->drawable.width)  << NV34TCL_TX_FORMAT_BASE_SIZE_U_SHIFT) |
			(log2i(pPix->drawable.height) << NV34TCL_TX_FORMAT_BASE_SIZE_V_SHIFT) |
			8 |
			NV34TCL_TX_FORMAT_DMA0);

	OUT_RING  (chan, (card_repeat << NV34TCL_TX_WRAP_S_SHIFT) |
			(card_repeat << NV34TCL_TX_WRAP_T_SHIFT) |
			(card_repeat << NV34TCL_TX_WRAP_R_SHIFT));
	OUT_RING  (chan, NV34TCL_TX_ENABLE_ENABLE);
	OUT_RING  (chan, (((uint32_t)exaGetPixmapPitch(pPix)) << NV34TCL_TX_SWIZZLE_RECT_PITCH_SHIFT ) | 
			fmt->card_swz);

	OUT_RING  (chan, (card_filter << NV34TCL_TX_FILTER_MINIFY_SHIFT) /* min */ |
			(card_filter << NV34TCL_TX_FILTER_MAGNIFY_SHIFT) /* mag */ |
			0x2000 /* engine lock */);
	OUT_RING  (chan, (pPix->drawable.width << NV34TCL_TX_NPOT_SIZE_W_SHIFT) | pPix->drawable.height);
	OUT_RING  (chan, 0); /* border ARGB */

	state->unit[unit].width		= (float)pPix->drawable.width;
	state->unit[unit].height	= (float)pPix->drawable.height;
	state->unit[unit].transform	= pPict->transform;

	return TRUE;
}

static Bool
NV30_SetupSurface(ScrnInfoPtr pScrn, PixmapPtr pPix, PicturePtr pPict)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *rankine = pNv->Nv3D;
	struct nouveau_bo *bo = nouveau_pixmap_bo(pPix);
	unsigned delta = nouveau_pixmap_offset(pPix);
	nv_pict_surface_format_t *fmt;

	fmt = NV30_GetPictSurfaceFormat(pPict->format);
	if (!fmt) {
		ErrorF("AIII no format\n");
		return FALSE;
	}

	uint32_t pitch = (uint32_t)exaGetPixmapPitch(pPix);

	BEGIN_RING(chan, rankine, NV34TCL_RT_FORMAT, 3);
	OUT_RING  (chan, fmt->card_fmt); /* format */
	OUT_RING  (chan, pitch << 16 | pitch);
	OUT_RELOCl(chan, bo, delta, NOUVEAU_BO_VRAM | NOUVEAU_BO_WR);

	return TRUE;
}

static Bool
NV30EXACheckCompositeTexture(PicturePtr pPict)
{
	nv_pict_texture_format_t *fmt;
	int w, h;

	if (!pPict->pDrawable)
		NOUVEAU_FALLBACK("Solid and gradient pictures unsupported\n");

	w = pPict->pDrawable->width;
	h = pPict->pDrawable->height;

	if ((w > 4096) || (h > 4096))
		NOUVEAU_FALLBACK("picture too large, %dx%d\n", w, h);

	fmt = NV30_GetPictTextureFormat(pPict->format);
	if (!fmt)
		NOUVEAU_FALLBACK("picture format 0x%08x not supported\n",
				pPict->format);

	if (pPict->filter != PictFilterNearest &&
			pPict->filter != PictFilterBilinear)
		NOUVEAU_FALLBACK("filter 0x%x not supported\n", pPict->filter);

	if (!(w==1 && h==1) && pPict->repeat && pPict->repeatType != RepeatNone)
		NOUVEAU_FALLBACK("repeat 0x%x not supported (surface %dx%d)\n",
				 pPict->repeatType,w,h);

	return TRUE;
}

Bool
NV30EXACheckComposite(int op, PicturePtr psPict,
		PicturePtr pmPict,
		PicturePtr pdPict)
{
	nv_pict_surface_format_t *fmt;
	nv_pict_op_t *opr;

	opr = NV30_GetPictOpRec(op);
	if (!opr)
		NOUVEAU_FALLBACK("unsupported blend op 0x%x\n", op);

	fmt = NV30_GetPictSurfaceFormat(pdPict->format);
	if (!fmt)
		NOUVEAU_FALLBACK("dst picture format 0x%08x not supported\n",
				pdPict->format);

	if (!NV30EXACheckCompositeTexture(psPict))
		NOUVEAU_FALLBACK("src picture\n");
	if (pmPict) {
		if (pmPict->componentAlpha &&
				PICT_FORMAT_RGB(pmPict->format) &&
				opr->src_alpha && opr->src_card_op != BF(ZERO))
			NOUVEAU_FALLBACK("mask CA + SA\n");
		if (!NV30EXACheckCompositeTexture(pmPict))
			NOUVEAU_FALLBACK("mask picture\n");
	}

	return TRUE;
}

static void
NV30EXAStateCompositeReemit(struct nouveau_channel *chan)
{
	ScrnInfoPtr pScrn = chan->user_private;
	NVPtr pNv = NVPTR(pScrn);

	NV30EXAPrepareComposite(pNv->alu, pNv->pspict, pNv->pmpict, pNv->pdpict,
				pNv->pspix, pNv->pmpix, pNv->pdpix);
}

Bool
NV30EXAPrepareComposite(int op, PicturePtr psPict,
		PicturePtr pmPict,
		PicturePtr pdPict,
		PixmapPtr  psPix,
		PixmapPtr  pmPix,
		PixmapPtr  pdPix)
{
	ScrnInfoPtr pScrn = xf86Screens[psPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *rankine = pNv->Nv3D;
	nv_pict_op_t *blend;
	int fpid = NV30EXA_FPID_PASS_COL0;
	NV30EXA_STATE;

	WAIT_RING(chan, 128);

	blend = NV30_GetPictOpRec(op);

	NV30_SetupBlend(pScrn, blend, pdPict->format,
			(pmPict && pmPict->componentAlpha &&
			 PICT_FORMAT_RGB(pmPict->format)));

	NV30_SetupSurface(pScrn, pdPix, pdPict);
	NV30EXATexture(pScrn, psPix, psPict, 0);

#if 0
#define printformat(f) ErrorF("(%xh %s %dbpp A%dR%dG%dB%d)",f,(f>>16)&0xf==2?"ARGB":"ABGR",(f>>24),(f&0xf000)>>12,(f&0xf00)>>8,(f&0xf0)>>4,f&0xf)
	ErrorF("Preparecomposite src(%dx%d)",psPict->pDrawable->width,psPict->pDrawable->height);
	printformat((psPict->format));
	ErrorF(" dst(%dx%d)",pdPict->pDrawable->width,pdPict->pDrawable->height);
	printformat((pdPict->format));
	if (pmPict)
	{
		ErrorF(" mask(%dx%d)",pmPict->pDrawable->width,pmPict->pDrawable->height);
		printformat((pmPict->format));
	}
	ErrorF("\n");
#endif

	if (pmPict) {
		NV30EXATexture(pScrn, pmPix, pmPict, 1);

		if (pmPict->componentAlpha && PICT_FORMAT_RGB(pmPict->format)) {
			if (blend->src_alpha)
				fpid = NV30EXA_FPID_COMPOSITE_MASK_SA_CA;
			else
				fpid = NV30EXA_FPID_COMPOSITE_MASK_CA;
		} else {
			fpid = NV30EXA_FPID_COMPOSITE_MASK;
		}

		state->have_mask = TRUE;
	} else {
		fpid = NV30EXA_FPID_PASS_TEX0;

		state->have_mask = FALSE;
	}

	if (pdPict->format == PICT_a8)
		NV30_LoadFragProg(pScrn, nv40_fp_map_a8[fpid]);
	else
		NV30_LoadFragProg(pScrn, nv40_fp_map[fpid]);

	BEGIN_RING(chan, rankine, 0x23c, 1);
	OUT_RING  (chan, pmPict?3:1);

	pNv->alu = op;
	pNv->pspict = psPict;
	pNv->pmpict = pmPict;
	pNv->pdpict = pdPict;
	pNv->pspix = psPix;
	pNv->pmpix = pmPix;
	pNv->pdpix = pdPix;
	chan->flush_notify = NV30EXAStateCompositeReemit;
	return TRUE;
}

#define xFixedToFloat(v) \
	((float)xFixedToInt((v)) + ((float)xFixedFrac(v) / 65536.0))

static void
NV30EXATransformCoord(PictTransformPtr t, int x, int y, float sx, float sy,
					  float *x_ret, float *y_ret)
{
	PictVector v;

	if (t) {
		v.vector[0] = IntToxFixed(x);
		v.vector[1] = IntToxFixed(y);
		v.vector[2] = xFixed1;
		PictureTransformPoint(t, &v);
		*x_ret = xFixedToFloat(v.vector[0]);
		*y_ret = xFixedToFloat(v.vector[1]);
	} else {
		*x_ret = (float)x;
		*y_ret = (float)y;
	}
}

#define CV_OUTm(sx,sy,mx,my,dx,dy) do {                                        \
	BEGIN_RING(chan, rankine, NV34TCL_VTX_ATTR_2F_X(8), 4);                   \
	OUT_RINGf (chan, (sx)); OUT_RINGf (chan, (sy));                        \
	OUT_RINGf (chan, (mx)); OUT_RINGf (chan, (my));                        \
	BEGIN_RING(chan, rankine, NV34TCL_VTX_ATTR_2I(0), 1);                     \
	OUT_RING  (chan, ((dy)<<16)|(dx));                                     \
} while(0)
#define CV_OUT(sx,sy,dx,dy) do {                                               \
	BEGIN_RING(chan, rankine, NV34TCL_VTX_ATTR_2F_X(8), 2);                   \
	OUT_RINGf (chan, (sx)); OUT_RINGf (chan, (sy));                        \
	BEGIN_RING(chan, rankine, NV34TCL_VTX_ATTR_2I(0), 1);                     \
	OUT_RING  (chan, ((dy)<<16)|(dx));                                     \
} while(0)

void
NV30EXAComposite(PixmapPtr pdPix, int srcX , int srcY,
				  int maskX, int maskY,
				  int dstX , int dstY,
				  int width, int height)
{
	ScrnInfoPtr pScrn = xf86Screens[pdPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *rankine = pNv->Nv3D;
	float sX0, sX1, sX2, sY0, sY1, sY2;
	float mX0, mX1, mX2, mY0, mY1, mY2;
	NV30EXA_STATE;

	WAIT_RING(chan, 64);

	/* We're drawing a triangle, we need to scissor it to a quad. */
	/* The scissors are here for a good reason, we don't get the full image, but just a part. */
	/* Handling the cliprects is done for us already. */
	BEGIN_RING(chan, rankine, NV34TCL_SCISSOR_HORIZ, 2);
	OUT_RING  (chan, (width << 16) | dstX);
	OUT_RING  (chan, (height << 16) | dstY);
	BEGIN_RING(chan, rankine, NV34TCL_VERTEX_BEGIN_END, 1);
	OUT_RING  (chan, NV34TCL_VERTEX_BEGIN_END_TRIANGLES);

#if 0
	ErrorF("Composite [%dx%d] (%d,%d)IN(%d,%d)OP(%d,%d)\n",width,height,srcX,srcY,maskX,maskY,dstX,dstY);
#endif
	NV30EXATransformCoord(state->unit[0].transform, 
				srcX, srcY - height,
				state->unit[0].width,
				state->unit[0].height, &sX0, &sY0);
	NV30EXATransformCoord(state->unit[0].transform,
				srcX, srcY + height,
				state->unit[0].width,
				state->unit[0].height, &sX1, &sY1);
	NV30EXATransformCoord(state->unit[0].transform,
				srcX + 2*width, srcY + height,
				state->unit[0].width,
				state->unit[0].height, &sX2, &sY2);

	if (state->have_mask) {
		NV30EXATransformCoord(state->unit[1].transform, 
					maskX, maskY - height,
					state->unit[1].width,
					state->unit[1].height, &mX0, &mY0);
		NV30EXATransformCoord(state->unit[1].transform,
					maskX, maskY + height,
					state->unit[1].width,
					state->unit[1].height, &mX1, &mY1);
		NV30EXATransformCoord(state->unit[1].transform,
					maskX + 2*width, maskY + height,
					state->unit[1].width,
					state->unit[1].height, &mX2, &mY2);

		CV_OUTm(sX0 , sY0 , mX0, mY0, dstX			,	dstY - height);
		CV_OUTm(sX1 , sY1 , mX1, mY1, dstX			,	dstY + height);
		CV_OUTm(sX2 , sY2 , mX2, mY2, dstX + 2*width	, 	dstY + height);
	} else {
		CV_OUT(sX0 , sY0 , dstX			,	dstY - height);
		CV_OUT(sX1 , sY1 , dstX			,	dstY + height);
		CV_OUT(sX2 , sY2 , dstX + 2*width	, 	dstY + height);
	}

	BEGIN_RING(chan, rankine, NV34TCL_VERTEX_BEGIN_END, 1);
	OUT_RING  (chan, 0);
}

void
NV30EXADoneComposite(PixmapPtr pdPix)
{
	ScrnInfoPtr pScrn = xf86Screens[pdPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;

	chan->flush_notify = NULL;
}

Bool
NVAccelInitNV30TCL(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *rankine;
	uint32_t class = 0, chipset;
	int next_hw_offset = 0, i;

	if (!nv40_fp_map_a8[0])
		NV30EXAHackupA8Shaders(pScrn);

#define NV30TCL_CHIPSET_3X_MASK 0x00000003
#define NV35TCL_CHIPSET_3X_MASK 0x000001e0
#define NV34TCL_CHIPSET_3X_MASK 0x00000010

	chipset = (nvReadMC(pNv, NV_PMC_BOOT_0) >> 20) & 0xff;
	if ((chipset & 0xf0) != NV_ARCH_30)
		return TRUE;
	chipset &= 0xf;

	if (NV30TCL_CHIPSET_3X_MASK & (1<<chipset))
		class = NV30TCL;
	else if (NV35TCL_CHIPSET_3X_MASK & (1<<chipset))
		class = NV35TCL;
	else if (NV34TCL_CHIPSET_3X_MASK & (1<<chipset))
		class = NV34TCL;
	else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "NV30EXA: Unknown chipset NV3%1x\n", chipset);
		return FALSE;
	}


	if (!pNv->Nv3D) {
		if (nouveau_grobj_alloc(chan, Nv3D, class, &pNv->Nv3D))
			return FALSE;
	}
	rankine = pNv->Nv3D;

	if (!pNv->shader_mem) {
		if (nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_MAP,
				   0, 0x1000, &pNv->shader_mem)) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "Couldn't alloc fragprog buffer!\n");
			nouveau_grobj_free(&pNv->Nv3D);
			return FALSE;
		}
	}

	BEGIN_RING(chan, rankine, NV34TCL_DMA_TEXTURE0, 3);
	OUT_RING  (chan, pNv->chan->vram->handle);
	OUT_RING  (chan, pNv->chan->gart->handle);
	OUT_RING  (chan, pNv->chan->vram->handle);
	BEGIN_RING(chan, rankine, NV34TCL_DMA_IN_MEMORY7, 1);
	OUT_RING  (chan, pNv->chan->vram->handle);
	BEGIN_RING(chan, rankine, NV34TCL_DMA_COLOR0, 2);
	OUT_RING  (chan, pNv->chan->vram->handle);
	OUT_RING  (chan, pNv->chan->vram->handle);
	BEGIN_RING(chan, rankine, NV34TCL_DMA_IN_MEMORY8, 1);
	OUT_RING  (chan, pNv->chan->vram->handle);

	for (i=1; i<8; i++) {
		BEGIN_RING(chan, rankine, NV34TCL_VIEWPORT_CLIP_HORIZ(i), 2);
		OUT_RING  (chan, 0);
		OUT_RING  (chan, 0);
	}

	BEGIN_RING(chan, rankine, 0x220, 1);
	OUT_RING  (chan, 1);

	BEGIN_RING(chan, rankine, 0x03b0, 1);
	OUT_RING  (chan, 0x00100000);
	BEGIN_RING(chan, rankine, 0x1454, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, rankine, 0x1d80, 1);
	OUT_RING  (chan, 3);
	BEGIN_RING(chan, rankine, 0x1450, 1);
	OUT_RING  (chan, 0x00030004);

	/* NEW */
	BEGIN_RING(chan, rankine, 0x1e98, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, rankine, 0x17e0, 3);
	OUT_RING  (chan, 0);
	OUT_RING  (chan, 0);
	OUT_RING  (chan, 0x3f800000);
	BEGIN_RING(chan, rankine, 0x1f80, 16);
	OUT_RING  (chan, 0); OUT_RING  (chan, 0); OUT_RING  (chan, 0); OUT_RING  (chan, 0); 
	OUT_RING  (chan, 0); OUT_RING  (chan, 0); OUT_RING  (chan, 0); OUT_RING  (chan, 0); 
	OUT_RING  (chan, 0x0000ffff);
	OUT_RING  (chan, 0); OUT_RING  (chan, 0); OUT_RING  (chan, 0); OUT_RING  (chan, 0); 
	OUT_RING  (chan, 0); OUT_RING  (chan, 0); OUT_RING  (chan, 0); 

	BEGIN_RING(chan, rankine, 0x120, 3);
	OUT_RING  (chan, 0);
	OUT_RING  (chan, 1);
	OUT_RING  (chan, 2);

	BEGIN_RING(chan, pNv->NvImageBlit, 0x120, 3);
	OUT_RING  (chan, 0);
	OUT_RING  (chan, 1);
	OUT_RING  (chan, 2);

	BEGIN_RING(chan, rankine, 0x1d88, 1);
	OUT_RING  (chan, 0x00001200);

	BEGIN_RING(chan, rankine, NV34TCL_RC_ENABLE, 1);
	OUT_RING  (chan, 0);

	/* Attempt to setup a known state.. Probably missing a heap of
	 * stuff here..
	 */
	BEGIN_RING(chan, rankine, NV34TCL_STENCIL_FRONT_ENABLE, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, rankine, NV34TCL_STENCIL_BACK_ENABLE, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, rankine, NV34TCL_ALPHA_FUNC_ENABLE, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, rankine, NV34TCL_DEPTH_WRITE_ENABLE, 2);
	OUT_RING  (chan, 0); /* wr disable */
	OUT_RING  (chan, 0); /* test disable */
	BEGIN_RING(chan, rankine, NV34TCL_COLOR_MASK, 1);
	OUT_RING  (chan, 0x01010101); /* TR,TR,TR,TR */
	BEGIN_RING(chan, rankine, NV34TCL_CULL_FACE_ENABLE, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, rankine, NV34TCL_BLEND_FUNC_ENABLE, 5);
	OUT_RING  (chan, 0);				/* Blend enable */
	OUT_RING  (chan, 0);				/* Blend src */
	OUT_RING  (chan, 0);				/* Blend dst */
	OUT_RING  (chan, 0x00000000);			/* Blend colour */
	OUT_RING  (chan, 0x8006);			/* FUNC_ADD */
	BEGIN_RING(chan, rankine, NV34TCL_COLOR_LOGIC_OP_ENABLE, 2);
	OUT_RING  (chan, 0);
	OUT_RING  (chan, 0x1503 /*GL_COPY*/);
	BEGIN_RING(chan, rankine, NV34TCL_DITHER_ENABLE, 1);
	OUT_RING  (chan, 1);
	BEGIN_RING(chan, rankine, NV34TCL_SHADE_MODEL, 1);
	OUT_RING  (chan, 0x1d01 /*GL_SMOOTH*/);
	BEGIN_RING(chan, rankine, NV34TCL_POLYGON_OFFSET_FACTOR,2);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	BEGIN_RING(chan, rankine, NV34TCL_POLYGON_MODE_FRONT, 2);
	OUT_RING  (chan, 0x1b02 /*GL_FILL*/);
	OUT_RING  (chan, 0x1b02 /*GL_FILL*/);
	/* - Disable texture units
	 * - Set fragprog to MOVR result.color, fragment.color */
	for (i=0;i<4;i++) {
		BEGIN_RING(chan, rankine, NV34TCL_TX_ENABLE(i), 1);
		OUT_RING  (chan, 0);
	}
	/* Polygon stipple */
	BEGIN_RING(chan, rankine, NV34TCL_POLYGON_STIPPLE_PATTERN(0), 0x20);
	for (i=0;i<0x20;i++)
		OUT_RING  (chan, 0xFFFFFFFF);

	BEGIN_RING(chan, rankine, NV34TCL_DEPTH_RANGE_NEAR, 2);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 1.0);

	/* Ok.  If you start X with the nvidia driver, kill it, and then
	 * start X with nouveau you will get black rendering instead of
	 * what you'd expect.  This fixes the problem, and it seems that
	 * it's not needed between nouveau restarts - which suggests that
	 * the 3D context (wherever it's stored?) survives somehow.
	 */
	//BEGIN_RING(chan, rankine, 0x1d60,1);
	//OUT_RING  (chan, 0x03008000);

	int w=4096;
	int h=4096;
	int pitch=4096*4;
	BEGIN_RING(chan, rankine, NV34TCL_RT_HORIZ, 5);
	OUT_RING  (chan, w<<16);
	OUT_RING  (chan, h<<16);
	OUT_RING  (chan, 0x148); /* format */
	OUT_RING  (chan, pitch << 16 | pitch);
	OUT_RING  (chan, 0x0);
	BEGIN_RING(chan, rankine, NV34TCL_VIEWPORT_TX_ORIGIN, 1);
	OUT_RING  (chan, 0);
        BEGIN_RING(chan, rankine, 0x0a00, 2);
        OUT_RING  (chan, (w<<16) | 0);
        OUT_RING  (chan, (h<<16) | 0);
	BEGIN_RING(chan, rankine, NV34TCL_VIEWPORT_CLIP_HORIZ(0), 2);
	OUT_RING  (chan, (w-1)<<16);
	OUT_RING  (chan, (h-1)<<16);
	BEGIN_RING(chan, rankine, NV34TCL_SCISSOR_HORIZ, 2);
	OUT_RING  (chan, w<<16);
	OUT_RING  (chan, h<<16);
	BEGIN_RING(chan, rankine, NV34TCL_VIEWPORT_HORIZ, 2);
	OUT_RING  (chan, w<<16);
	OUT_RING  (chan, h<<16);

	BEGIN_RING(chan, rankine, NV34TCL_VIEWPORT_TRANSLATE_X, 8);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 1.0);
	OUT_RINGf (chan, 1.0);
	OUT_RINGf (chan, 1.0);
	OUT_RINGf (chan, 0.0);

	BEGIN_RING(chan, rankine, NV34TCL_MODELVIEW_MATRIX(0), 16);
	OUT_RINGf (chan, 1.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 1.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 1.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 1.0);

	BEGIN_RING(chan, rankine, NV34TCL_PROJECTION_MATRIX(0), 16);
	OUT_RINGf (chan, 1.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 1.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 1.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 0.0);
	OUT_RINGf (chan, 1.0);

	BEGIN_RING(chan, rankine, NV34TCL_SCISSOR_HORIZ, 2);
	OUT_RING  (chan, 4096<<16);
	OUT_RING  (chan, 4096<<16);

	for (i = 0; i < NV30EXA_FPID_MAX; i++) {
		NV30_UploadFragProg(pNv, nv40_fp_map[i], &next_hw_offset);
		NV30_UploadFragProg(pNv, nv40_fp_map_a8[i], &next_hw_offset);
	}
	NV30_UploadFragProg(pNv, &nv30_fp_yv12_bicubic, &next_hw_offset);
	NV30_UploadFragProg(pNv, &nv30_fp_yv12_bilinear, &next_hw_offset);

	return TRUE;
}
