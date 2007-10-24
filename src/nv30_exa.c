#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_NV30EXA

#include "nv_include.h"
#include "nv_shaders.h"

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
	{ PICT_x8r8g8b8	, 0x145 },
	{ PICT_r5g6b5	, 0x143 },
//	{ PICT_a8       , 0x149 },
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
#define NV30TCL_TX_SWIZZLE_UNIT_S0_X_SHIFT	14
#define NV30TCL_TX_SWIZZLE_UNIT_S0_X_ZERO	 0
#define NV30TCL_TX_SWIZZLE_UNIT_S0_X_ONE	 1
#define NV30TCL_TX_SWIZZLE_UNIT_S0_X_S1		 2
#define NV30TCL_TX_SWIZZLE_UNIT_S0_Y_SHIFT	12
#define NV30TCL_TX_SWIZZLE_UNIT_S0_Z_SHIFT	10
#define NV30TCL_TX_SWIZZLE_UNIT_S0_W_SHIFT	 8
#define NV30TCL_TX_SWIZZLE_UNIT_S1_X_SHIFT	 6
#define NV30TCL_TX_SWIZZLE_UNIT_S1_X_X		 3
#define NV30TCL_TX_SWIZZLE_UNIT_S1_X_Y		 2
#define NV30TCL_TX_SWIZZLE_UNIT_S1_X_Z		 1
#define NV30TCL_TX_SWIZZLE_UNIT_S1_X_W		 0
#define NV30TCL_TX_SWIZZLE_UNIT_S1_Y_SHIFT	 4
#define NV30TCL_TX_SWIZZLE_UNIT_S1_Z_SHIFT	 2
#define NV30TCL_TX_SWIZZLE_UNIT_S1_W_SHIFT	 0

#define _(r,tf,ts0x,ts0y,ts0z,ts0w,ts1x,ts1y,ts1z,ts1w)                       \
  {                                                                           \
  PICT_##r,                                                                   \
  (tf),                                                                       \
  (NV30TCL_TX_SWIZZLE_UNIT_S0_X_##ts0x << NV30TCL_TX_SWIZZLE_UNIT_S0_X_SHIFT)|\
  (NV30TCL_TX_SWIZZLE_UNIT_S0_X_##ts0y << NV30TCL_TX_SWIZZLE_UNIT_S0_Y_SHIFT)|\
  (NV30TCL_TX_SWIZZLE_UNIT_S0_X_##ts0z << NV30TCL_TX_SWIZZLE_UNIT_S0_Z_SHIFT)|\
  (NV30TCL_TX_SWIZZLE_UNIT_S0_X_##ts0w << NV30TCL_TX_SWIZZLE_UNIT_S0_W_SHIFT)|\
  (NV30TCL_TX_SWIZZLE_UNIT_S1_X_##ts1x << NV30TCL_TX_SWIZZLE_UNIT_S1_X_SHIFT)|\
  (NV30TCL_TX_SWIZZLE_UNIT_S1_X_##ts1y << NV30TCL_TX_SWIZZLE_UNIT_S1_Y_SHIFT)|\
  (NV30TCL_TX_SWIZZLE_UNIT_S1_X_##ts1z << NV30TCL_TX_SWIZZLE_UNIT_S1_Z_SHIFT)|\
  (NV30TCL_TX_SWIZZLE_UNIT_S1_X_##ts1w << NV30TCL_TX_SWIZZLE_UNIT_S1_W_SHIFT)\
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
	_(      a8, 0x1b, ZERO, ZERO, ZERO,   S1, W, W, W, W),
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

#define NV34_TCL_PRIMITIVE_3D_BF_ZERO                                     0x0000
#define NV34_TCL_PRIMITIVE_3D_BF_ONE                                      0x0001
#define NV34_TCL_PRIMITIVE_3D_BF_SRC_COLOR                                0x0300
#define NV34_TCL_PRIMITIVE_3D_BF_ONE_MINUS_SRC_COLOR                      0x0301
#define NV34_TCL_PRIMITIVE_3D_BF_SRC_ALPHA                                0x0302
#define NV34_TCL_PRIMITIVE_3D_BF_ONE_MINUS_SRC_ALPHA                      0x0303
#define NV34_TCL_PRIMITIVE_3D_BF_DST_ALPHA                                0x0304
#define NV34_TCL_PRIMITIVE_3D_BF_ONE_MINUS_DST_ALPHA                      0x0305
#define NV34_TCL_PRIMITIVE_3D_BF_DST_COLOR                                0x0306
#define NV34_TCL_PRIMITIVE_3D_BF_ONE_MINUS_DST_COLOR                      0x0307
#define NV34_TCL_PRIMITIVE_3D_BF_ALPHA_SATURATE                           0x0308
#define BF(bf) NV34_TCL_PRIMITIVE_3D_BF_##bf

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

#define FALLBACK_DEBUG 0
#if FALLBACK_DEBUG == 1
#define FALLBACK(fmt,args...) do {					\
	ErrorF("FALLBACK %s:%d> " fmt, __func__, __LINE__, ##args);	\
	return FALSE;							\
} while(0)
#else
#define FALLBACK(fmt,args...) do { \
	return FALSE;              \
} while(0)
#endif

static void
NV30_LoadFragProg(ScrnInfoPtr pScrn, nv_shader_t *shader)
{
	NVPtr pNv = NVPTR(pScrn);
	static NVAllocRec *fp_mem = NULL;
	static int next_hw_id_offset = 0;

	if (!fp_mem) {
		fp_mem = NVAllocateMemory(pNv, NOUVEAU_MEM_FB, 0x1000);
		if (!fp_mem) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
					"Couldn't alloc fragprog buffer!\n");
			return;
		}
	}

	if (!shader->hw_id) {
		memcpy(fp_mem->map + next_hw_id_offset, shader->data,
				shader->size *
				sizeof(uint32_t));

		shader->hw_id  = fp_mem->offset;
		shader->hw_id += next_hw_id_offset;

		next_hw_id_offset += (shader->size * sizeof(uint32_t));
		next_hw_id_offset = (next_hw_id_offset + 63) & ~63;
	}

	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_FP_ACTIVE_PROGRAM, 1);
	OUT_RING  (shader->hw_id|1);

	BEGIN_RING(Nv3D, 0x23c, 1);
	OUT_RING  (3);
	BEGIN_RING(Nv3D, 0x1d60, 1);
	OUT_RING  (0); /* USES_KIL (1<<7) == 0 */
	BEGIN_RING(Nv3D, 0x1450, 1);
	OUT_RING  (shader->card_priv.NV30FP.num_regs << 16| 4);
	BEGIN_RING(Nv3D, 0x1d7c, 1);
	OUT_RING  (0xffff0000);

        //BEGIN_RING(Nv3D, 0x0b00, 1);
        //OUT_RING(0x00000004);

}

static void
NV30_SetupBlend(ScrnInfoPtr pScrn, nv_pict_op_t *blend,
		PictFormatShort dest_format, Bool component_alpha)
{
	NVPtr pNv = NVPTR(pScrn);
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
		BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_BLEND_FUNC_ENABLE, 1);
		OUT_RING  (0);
	} else {
		BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_BLEND_FUNC_ENABLE, 5);
		OUT_RING  (1);
		OUT_RING  ((sblend << 16) | sblend);
		OUT_RING  ((dblend << 16) | dblend);
		OUT_RING  (0x00000000);			/* Blend colour */
		OUT_RING  ((0x8006 << 16) | 0x8006);	/* FUNC_ADD, FUNC_ADD */
	}
}

static Bool
NV30EXATexture(ScrnInfoPtr pScrn, PixmapPtr pPix, PicturePtr pPict, int unit)
{
	NVPtr pNv = NVPTR(pScrn);
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

	BEGIN_RING(Nv3D,
			NV34_TCL_PRIMITIVE_3D_TX_OFFSET(unit), 8);
	OUT_RING  (NVAccelGetPixmapOffset(pPix));

	OUT_RING  ((2 << 4) /* 2D */ |
			(fmt->card_fmt << 8) |
			(1 << 16) /* 1 mipmap level */ |
/*			(log2i(pPix->drawable.width)  << 20) |
			(log2i(pPix->drawable.height) << 24) |*/
			9);

	OUT_RING  ((card_repeat <<  0) /* S */ |
			(card_repeat <<  8) /* T */ |
			(card_repeat << 16) /* R */);
	OUT_RING  (0x40000000); /* enable */
	OUT_RING  ((((uint32_t)exaGetPixmapPitch(pPix))<<16) | fmt->card_swz);

	OUT_RING  ((card_filter << 16) /* min */ |
			(card_filter << 24) /* mag */ |
			0x3fd6 /* engine lock */);
	OUT_RING  ((pPix->drawable.width << 16) | pPix->drawable.height);
	OUT_RING  (0); /* border ARGB */

	state->unit[unit].width		= (float)pPix->drawable.width;
	state->unit[unit].height	= (float)pPix->drawable.height;
	state->unit[unit].transform	= pPict->transform;

	return TRUE;
}

static Bool
NV30_SetupSurface(ScrnInfoPtr pScrn, PixmapPtr pPix, PicturePtr pPict)
{
	NVPtr pNv = NVPTR(pScrn);
	nv_pict_surface_format_t *fmt;

	fmt = NV30_GetPictSurfaceFormat(pPict->format);
	if (!fmt) {
		ErrorF("AIII no format\n");
		return FALSE;
	}

	uint32_t pitch = (uint32_t)exaGetPixmapPitch(pPix);

	int w = pPict->pDrawable->width;
	int h = pPict->pDrawable->height;
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VIEWPORT_HORIZ, 5);
	OUT_RING  (w<<16);
	OUT_RING  (h<<16);
	OUT_RING  (fmt->card_fmt); /* format */
	OUT_RING  (pitch << 16 | pitch);
	OUT_RING  (NVAccelGetPixmapOffset(pPix));
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_HORIZ(0), 2);
	OUT_RING  ((w-1)<<16);
	OUT_RING  ((h-1)<<16);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VIEWPORT_HORIZ, 2);
	OUT_RING  (w<<16);
	OUT_RING  (h<<16);

	return TRUE;
}

static Bool
NV30EXACheckCompositeTexture(PicturePtr pPict)
{
	nv_pict_texture_format_t *fmt;
	int w = pPict->pDrawable->width;
	int h = pPict->pDrawable->height;

	if ((w > 4096) || (h>4096))
		FALLBACK("picture too large, %dx%d\n", w, h);

	fmt = NV30_GetPictTextureFormat(pPict->format);
	if (!fmt)
		FALLBACK("picture format 0x%08x not supported\n",
				pPict->format);

	if (pPict->filter != PictFilterNearest &&
			pPict->filter != PictFilterBilinear)
		FALLBACK("filter 0x%x not supported\n", pPict->filter);

	if (pPict->repeat && pPict->repeatType != RepeatNone)
		FALLBACK("repeat 0x%x not supported\n", pPict->repeatType);

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
		FALLBACK("unsupported blend op 0x%x\n", op);

	fmt = NV30_GetPictSurfaceFormat(pdPict->format);
	if (!fmt)
		FALLBACK("dst picture format 0x%08x not supported\n",
				pdPict->format);

	if (!NV30EXACheckCompositeTexture(psPict))
		FALLBACK("src picture\n");
	if (pmPict) {
		if (pmPict->componentAlpha &&
				PICT_FORMAT_RGB(pmPict->format) &&
				opr->src_alpha && opr->src_card_op != BF(ZERO))
			FALLBACK("mask CA + SA\n");
		if (!NV30EXACheckCompositeTexture(pmPict))
			FALLBACK("mask picture\n");
	}

	return TRUE;
}

void NV30_SetVtx(ScrnInfoPtr pScrn, int multitex)
{
	NVPtr pNv = NVPTR(pScrn);

	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VERTEX_ARRAY_FORMAT(0), 16);
	OUT_RING  (0x22);
	OUT_RING  (0x2);
	OUT_RING  (0x2);
	OUT_RING  (0x2);
	OUT_RING  (0x2);
	OUT_RING  (0x2);
	OUT_RING  (0x2);
	OUT_RING  (0x2);

	OUT_RING  (0x22);
	if (multitex)
		OUT_RING  (0x22);
	else
		OUT_RING  (0x02);
	OUT_RING  (0x2);
	OUT_RING  (0x2);
	OUT_RING  (0x2);
	OUT_RING  (0x2);
	OUT_RING  (0x2);
	OUT_RING  (0x2);
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
	nv_pict_op_t *blend;
	int fpid = NV30EXA_FPID_PASS_COL0;
	NV30EXA_STATE;

	blend = NV30_GetPictOpRec(op);

	NV30_SetupBlend(pScrn, blend, pdPict->format,
			(pmPict && pmPict->componentAlpha &&
			 PICT_FORMAT_RGB(pmPict->format)));

	NV30_SetupSurface(pScrn, pdPix, pdPict);
	NV30EXATexture(pScrn, psPix, psPict, 0);

#if 0
#define printformat(f) ErrorF("(%xh %dbpp A%dR%dG%dB%d)",f,(f>>24),(f&0xf000)>>12,(f&0xf00)>>8,(f&0xf0)>>4,f&0xf)
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

	NV30_SetVtx(pScrn,pmPict?1:0);
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

	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VERTEX_BEGIN_END, 1);
	OUT_RING  (8); /* GL_QUADS */

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
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VERTEX_ATTR_2F_X(8), 4);        \
	OUT_RINGf ((sx)); OUT_RINGf ((sy));                          \
	OUT_RINGf ((mx)); OUT_RINGf ((my));                          \
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VERTEX_ATTR_2I(0), 1);          \
	OUT_RING  (((dy)<<16)|(dx));                                           \
} while(0)
#define CV_OUT(sx,sy,dx,dy) do {                                               \
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VERTEX_ATTR_2F_X(8), 2);        \
	OUT_RINGf ((sx)); OUT_RINGf ((sy));                          \
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VERTEX_ATTR_2I(0), 1);          \
	OUT_RING  (((dy)<<16)|(dx));                                           \
} while(0)

void
NV30EXAComposite(PixmapPtr pdPix, int srcX , int srcY,
				  int maskX, int maskY,
				  int dstX , int dstY,
				  int width, int height)
{
	ScrnInfoPtr pScrn = xf86Screens[pdPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	float sX0, sX1, sY0, sY1;
	float mX0, mX1, mY0, mY1;
	NV30EXA_STATE;

#if 0
	ErrorF("Composite [%dx%d] (%d,%d)IN(%d,%d)OP(%d,%d)\n",width,height,srcX,srcY,maskX,maskY,dstX,dstY);
#endif
	NV30EXATransformCoord(state->unit[0].transform, srcX, srcY,
			      state->unit[0].width,
			      state->unit[0].height, &sX0, &sY0);
	NV30EXATransformCoord(state->unit[0].transform,
			      srcX + width, srcY + height,
			      state->unit[0].width,
			      state->unit[0].height, &sX1, &sY1);

	if (state->have_mask) {
		NV30EXATransformCoord(state->unit[1].transform, maskX, maskY,
				      state->unit[1].width,
				      state->unit[1].height, &mX0, &mY0);
		NV30EXATransformCoord(state->unit[1].transform,
				      maskX + width, maskY + height,
				      state->unit[1].width,
				      state->unit[1].height, &mX1, &mY1);
		CV_OUTm(sX0 , sY0 , mX0, mY0, dstX        ,          dstY);
		CV_OUTm(sX1 , sY0 , mX1, mY0, dstX + width,          dstY);
		CV_OUTm(sX1 , sY1 , mX1, mY1, dstX + width, dstY + height);
		CV_OUTm(sX0 , sY1 , mX0, mY1, dstX        , dstY + height);
	} else {
		CV_OUT(sX0 , sY0 , dstX        ,          dstY);
		CV_OUT(sX1 , sY0 , dstX + width,          dstY);
		CV_OUT(sX1 , sY1 , dstX + width, dstY + height);
		CV_OUT(sX0 , sY1 , dstX        , dstY + height);
	}

	FIRE_RING();
}

void
NV30EXADoneComposite(PixmapPtr pdPix)
{
	ScrnInfoPtr pScrn = xf86Screens[pdPix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VERTEX_BEGIN_END, 1);
	OUT_RING  (0);
}

Bool
NVAccelInitNV30TCL(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t class = 0, chipset;
	int i;

	NV30EXAHackupA8Shaders(pScrn);

#undef  NV30_TCL_PRIMITIVE_3D
#define NV30_TCL_PRIMITIVE_3D                 0x0397
#define NV30_TCL_PRIMITIVE_3D_CHIPSET_3X_MASK 0x00000003
#define NV35_TCL_PRIMITIVE_3D                 0x0497
#define NV35_TCL_PRIMITIVE_3D_CHIPSET_3X_MASK 0x000001e0
#define NV34_TCL_PRIMITIVE_3D_CHIPSET_3X_MASK 0x00000010

	chipset = (nvReadMC(pNv, 0) >> 20) & 0xff;
	if ((chipset & 0xf0) != NV_ARCH_30)
		return TRUE;
	chipset &= 0xf;

	if (NV30_TCL_PRIMITIVE_3D_CHIPSET_3X_MASK & (1<<chipset))
		class = NV30_TCL_PRIMITIVE_3D;
	else if (NV35_TCL_PRIMITIVE_3D_CHIPSET_3X_MASK & (1<<chipset))
		class = NV35_TCL_PRIMITIVE_3D;
	else if (NV34_TCL_PRIMITIVE_3D_CHIPSET_3X_MASK & (1<<chipset))
		class = NV34_TCL_PRIMITIVE_3D;
	else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "NV30EXA: Unknown chipset NV3%1x\n", chipset);
		return FALSE;
	}

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, Nv3D, class))
			return FALSE;
		have_object = TRUE;
	}

	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_DMA_IN_MEMORY0, 3);
	OUT_RING  (NvDmaFB);
	OUT_RING  (NvDmaFB);
	OUT_RING  (NvDmaFB);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_DMA_IN_MEMORY7, 1);
	OUT_RING  (NvDmaFB);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_DMA_IN_MEMORY3, 2);
	OUT_RING  (NvDmaFB);
	OUT_RING  (NvDmaFB);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_DMA_IN_MEMORY8, 1);
	OUT_RING  (NvDmaFB);

	for(i = 0x2c8; i <= 0x2fc; i += 4)
	{
		BEGIN_RING(Nv3D, i, 1);
		OUT_RING  (0x0);
	}

	BEGIN_RING(Nv3D, 0x220, 1);
	OUT_RING  (1);

	BEGIN_RING(Nv3D, 0x03b0, 1);
	OUT_RING  (0x00100000);
	BEGIN_RING(Nv3D, 0x1454, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, 0x1d80, 1);
	OUT_RING  (3);
	
	/* NEW */
	BEGIN_RING(Nv3D, 0x1e98, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, 0x17e0, 3);
	OUT_RING  (0);
	OUT_RING  (0);
	OUT_RING  (0x3f800000);
	BEGIN_RING(Nv3D, 0x1f80, 16);
	OUT_RING  (0); OUT_RING  (0); OUT_RING  (0); OUT_RING  (0); 
	OUT_RING  (0); OUT_RING  (0); OUT_RING  (0); OUT_RING  (0); 
	OUT_RING  (0x0000ffff);
	OUT_RING  (0); OUT_RING  (0); OUT_RING  (0); OUT_RING  (0); 
	OUT_RING  (0); OUT_RING  (0); OUT_RING  (0); 

	BEGIN_RING(Nv3D, 0x120, 3);
	OUT_RING  (0);
	OUT_RING  (1);
	OUT_RING  (2);

	BEGIN_RING(Nv3D, 0x1d88, 1);
	OUT_RING  (0x00001200);

	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_RC_ENABLE, 1);
	OUT_RING  (0);

	/* Attempt to setup a known state.. Probably missing a heap of
	 * stuff here..
	 */
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_STENCIL_FRONT_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_STENCIL_BACK_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_ALPHA_FUNC_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_DEPTH_WRITE_ENABLE, 2);
	OUT_RING  (0); /* wr disable */
	OUT_RING  (0); /* test disable */
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_COLOR_MASK, 1);
	OUT_RING  (0x01010101); /* TR,TR,TR,TR */
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_CULL_FACE_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_BLEND_FUNC_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_COLOR_LOGIC_OP_ENABLE, 2);
	OUT_RING  (0);
	OUT_RING  (0x1503 /*GL_COPY*/);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_DITHER_ENABLE, 1);
	OUT_RING  (1);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_SHADE_MODEL, 1);
	OUT_RING  (0x1d01 /*GL_SMOOTH*/);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_POLYGON_OFFSET_FACTOR,2);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_POLYGON_MODE_FRONT, 2);
	OUT_RING  (0x1b02 /*GL_FILL*/);
	OUT_RING  (0x1b02 /*GL_FILL*/);
	/* - Disable texture units
	 * - Set fragprog to MOVR result.color, fragment.color */
	for (i=0;i<16;i++) {
		BEGIN_RING(Nv3D,
				NV34_TCL_PRIMITIVE_3D_TX_ENABLE(i), 1);
		OUT_RING  (0);
	}
	/* Polygon stipple */
	BEGIN_RING(Nv3D,
			NV34_TCL_PRIMITIVE_3D_POLYGON_STIPPLE_PATTERN(0), 0x20);
	for (i=0;i<0x20;i++)
		OUT_RING  (0xFFFFFFFF);

	/* Ok.  If you start X with the nvidia driver, kill it, and then
	 * start X with nouveau you will get black rendering instead of
	 * what you'd expect.  This fixes the problem, and it seems that
	 * it's not needed between nouveau restarts - which suggests that
	 * the 3D context (wherever it's stored?) survives somehow.
	 */
	//BEGIN_RING(Nv3D, 0x1d60,1);
	//OUT_RING  (0x03008000);

	int w=4096;
	int h=4096;
	int pitch=4096*4;
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VIEWPORT_HORIZ, 5);
	OUT_RING  (w<<16);
	OUT_RING  (h<<16);
	OUT_RING  (0x148); /* format */
	OUT_RING  (pitch << 16 | pitch);
	OUT_RING  (0x0);
        BEGIN_RING(Nv3D, 0x0a00, 2);
        OUT_RING  ((w<<16) | 0);
        OUT_RING  ((h<<16) | 0);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_HORIZ(0), 2);
	OUT_RING  ((w-1)<<16);
	OUT_RING  ((h-1)<<16);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_SCISSOR_HORIZ, 2);
	OUT_RING  (w<<16);
	OUT_RING  (h<<16);
	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VIEWPORT_HORIZ, 2);
	OUT_RING  (w<<16);
	OUT_RING  (h<<16);

	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_VIEWPORT_SCALE0_X, 8);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (1.0);
	OUT_RINGf (1.0);
	OUT_RINGf (1.0);
	OUT_RINGf (0.0);

	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_MODELVIEW_MATRIX(0), 16);
	OUT_RINGf (1.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (1.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (1.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (1.0);

	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_PROJECTION_MATRIX(0), 16);
	OUT_RINGf (1.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (1.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (1.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (1.0);

	BEGIN_RING(Nv3D, NV34_TCL_PRIMITIVE_3D_SCISSOR_HORIZ, 2);
	OUT_RING  (4096<<16);
	OUT_RING  (4096<<16);

	return TRUE;
}



#endif /* ENABLE_NV30EXA */
