#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "nv_include.h"

typedef struct nv10_exa_state {
	Bool have_mask;

	struct {
		PictTransformPtr transform;
		float width;
		float height;
	} unit[2];
} nv10_exa_state_t;
static nv10_exa_state_t state;

static int NV10TexFormat(int ExaFormat)
{
	struct {int exa;int hw;} tex_format[] =
	{
		{PICT_a8r8g8b8,	NV10_TCL_PRIMITIVE_3D_TX_FORMAT_FORMAT_R8G8B8A8},
		{PICT_a1r5g5b5,	NV10_TCL_PRIMITIVE_3D_TX_FORMAT_FORMAT_R5G5B5A1},
		{PICT_a4r4g4b4,	NV10_TCL_PRIMITIVE_3D_TX_FORMAT_FORMAT_R4G4B4A4},
		{PICT_a8,	NV10_TCL_PRIMITIVE_3D_TX_FORMAT_FORMAT_A8_RECT}
		// FIXME other formats
	};

	int i;
	for(i=0;i<sizeof(tex_format)/sizeof(tex_format[0]);i++)
	{
		if(tex_format[i].exa==ExaFormat)
			return tex_format[i].hw;
	}

	return 0;
}

static int NV10DstFormat(int ExaFormat)
{
	struct {int exa;int hw;} dst_format[] =
	{
		{PICT_a8r8g8b8,	0x108},
		{PICT_x8r8g8b8, 0x108},	// FIXME that one might need more
		{PICT_r5g6b5,	0x103}
		// FIXME other formats
	};

	int i;
	for(i=0;i<sizeof(dst_format)/sizeof(dst_format[0]);i++)
	{
		if(dst_format[i].exa==ExaFormat)
			return dst_format[i].hw;
	}

	return 0;
}

static Bool NV10CheckTexture(PicturePtr Picture)
{
	int w = Picture->pDrawable->width;
	int h = Picture->pDrawable->height;

	if ((w > 2046) || (h>2046))
		return FALSE;
	if (!NV10TexFormat(Picture->format))
		return FALSE;
	if (Picture->filter != PictFilterNearest && Picture->filter != PictFilterBilinear)
		return FALSE;
	if (Picture->repeat != RepeatNone)
		return FALSE;
	return TRUE;
}

static Bool NV10CheckBuffer(PicturePtr Picture)
{
	int w = Picture->pDrawable->width;
	int h = Picture->pDrawable->height;

	if ((w > 4096) || (h>4096))
		return FALSE;
	if (!NV10DstFormat(Picture->format))
		return FALSE;
	return TRUE;
}

static Bool NV10CheckComposite(int	op,
			     PicturePtr pSrcPicture,
			     PicturePtr pMaskPicture,
			     PicturePtr pDstPicture)
{
	// XXX A8 + A8 special case "TO BE DONE LATER"
/*	if ((!pMaskPicture) &&
			(pSrcPicture->format == PICT_a8) &&
			(pDstPicture->format == PICT_a8) )
		return TRUE;*/

	if (!NV10CheckBuffer(pDstPicture))
		return FALSE;
	if (!NV10CheckTexture(pSrcPicture))
		return FALSE;
	if ((pMaskPicture)&&(!NV10CheckTexture(pMaskPicture)))
		return FALSE;

	return TRUE;
}

static void NV10SetTexture(NVPtr pNv,int unit,PicturePtr Pict,PixmapPtr pixmap)
{
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_TX_OFFSET(unit), 1 );
	NVDmaNext (pNv, NVAccelGetPixmapOffset(pixmap));

	int log2w = log2i(Pict->pDrawable->width);
	int log2h = log2i(Pict->pDrawable->height);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_TX_FORMAT(unit), 1 );
	NVDmaNext (pNv, (NV10_TCL_PRIMITIVE_3D_TX_FORMAT_WRAP_T_CLAMP_TO_EDGE<<28) |
			(NV10_TCL_PRIMITIVE_3D_TX_FORMAT_WRAP_S_CLAMP_TO_EDGE<<24) |
			(log2w<<20) |
			(log2h<<16) |
			(1<<12) | /* lod == 1 */
			(1<<11) | /* enable NPOT */
			(NV10TexFormat(Pict->format)<<7) |
			0x51 /* UNK */
			);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_TX_ENABLE(unit), 1 );
	NVDmaNext (pNv, 1);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_TX_NPOT_PITCH(unit), 1);
	NVDmaNext (pNv, exaGetPixmapPitch(pixmap) << 16);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_TX_NPOT_SIZE(unit), 1);
	NVDmaNext (pNv, (Pict->pDrawable->width<<16) | Pict->pDrawable->height); /* FIXME alignment restrictions, should be even at least */

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_TX_FILTER(unit), 1);
	if (Pict->filter == PictFilterNearest)
		NVDmaNext (pNv, (NV10_TCL_PRIMITIVE_3D_TX_FILTER_MAG_FILTER_NEAREST<<28) |
				(NV10_TCL_PRIMITIVE_3D_TX_FILTER_MIN_FILTER_NEAREST<<24));
	else
		NVDmaNext (pNv, (NV10_TCL_PRIMITIVE_3D_TX_FILTER_MAG_FILTER_LINEAR<<28) |
				(NV10_TCL_PRIMITIVE_3D_TX_FILTER_MIN_FILTER_LINEAR<<24));

	state.unit[unit].width		= (float)pixmap->drawable.width;
	state.unit[unit].height		= (float)pixmap->drawable.height;
	state.unit[unit].transform	= Pict->transform;
}

static void NV10SetBuffer(NVPtr pNv,PicturePtr Pict,PixmapPtr pixmap)
{
	int x = 0,y = 0,i;
	int w = Pict->pDrawable->width;
	int h = Pict->pDrawable->height;

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_BUFFER_FORMAT, 4);
	NVDmaNext (pNv, NV10DstFormat(Pict->format));
	NVDmaNext (pNv, ((uint32_t)exaGetPixmapPitch(pixmap) << 16) |(uint32_t)exaGetPixmapPitch(pixmap));
	NVDmaNext (pNv, NVAccelGetPixmapOffset(pixmap));
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_HORIZ, 2);
	NVDmaNext (pNv, (w<<16)|x);
	NVDmaNext (pNv, (h<<16)|y);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_MODE, 1); /* clip_mode */
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_HORIZ(0), 1);
	NVDmaNext (pNv, ((w-1+x)<<16)|x|0x08000800);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_VERT(0), 1);
	NVDmaNext (pNv, ((h-1+y)<<16)|y|0x08000800);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_PROJECTION_MATRIX(0), 16);
	for(i=0;i<16;i++)
		if (i/4==i%4)
			NVDmaFloat(pNv, 1.0f);
		else
			NVDmaFloat(pNv, 0.0f);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_DEPTH_RANGE_NEAR, 2);
	NVDmaNext (pNv, 0);
#if SCREEN_BPP == 32
	NVDmaFloat (pNv, 16777216.0);
#else
	NVDmaFloat (pNv, 65536.0);
#endif
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_SCALE_X, 4);
	NVDmaFloat (pNv, (w * 0.5) - 2048.0);
	NVDmaFloat (pNv, (h * 0.5) - 2048.0);
#if SCREEN_BPP == 32
	NVDmaFloat (pNv, 16777215.0 * 0.5);
#else
	NVDmaFloat (pNv, 65535.0 * 0.5);
#endif
	NVDmaNext (pNv, 0);
}

static void NV10SetMultitexture(NVPtr pNv,int multitex)
{
	// FIXME
	if (multitex)
	{
/*
18141010    NV10_TCL_PRIMITIVE_3D.RC_IN_ALPHA[0] = D_INPUT=ZERO | D_COMPONENT_USAGE=ALPHA | D_MAPPING=UNSIGNED_IDENTITY_NV | C_INPUT=ZERO | C_COMPONENT_USAGE=ALPHA | C_MAPPING=UNSIGNED_IDENTITY_NV | B_INPUT=PRIMARY_COLOR_NV | B_COMPONENT_USAGE=ALPHA | B_MAPPING=UNSIGNED_IDENTITY_NV | A_INPUT=TEXTURE1_ARB | A_COMPONENT_USAGE=ALPHA | A_MAPPING=UNSIGNED_IDENTITY_NV
091c1010    NV10_TCL_PRIMITIVE_3D.RC_IN_ALPHA[1] = D_INPUT=ZERO | D_COMPONENT_USAGE=ALPHA | D_MAPPING=UNSIGNED_IDENTITY_NV | C_INPUT=ZERO | C_COMPONENT_USAGE=ALPHA | C_MAPPING=UNSIGNED_IDENTITY_NV | B_INPUT=SPARE0_NV | B_COMPONENT_USAGE=ALPHA | B_MAPPING=UNSIGNED_IDENTITY_NV | A_INPUT=TEXTURE0_ARB | A_COMPONENT_USAGE=BLUE | A_MAPPING=UNSIGNED_IDENTITY_NV
08040820    NV10_TCL_PRIMITIVE_3D.RC_IN_RGB[0] = D_INPUT=ZERO | D_COMPONENT_USAGE=RGB | D_MAPPING=UNSIGNED_INVERT_NV | C_INPUT=TEXTURE1_ARB | C_COMPONENT_USAGE=RGB | C_MAPPING=UNSIGNED_IDENTITY_NV | B_INPUT=PRIMARY_COLOR_NV | B_COMPONENT_USAGE=RGB | B_MAPPING=UNSIGNED_IDENTITY_NV | A_INPUT=TEXTURE1_ARB | A_COMPONENT_USAGE=RGB | A_MAPPING=UNSIGNED_IDENTITY_NV
090c0920    NV10_TCL_PRIMITIVE_3D.RC_IN_RGB[1] = D_INPUT=ZERO | D_COMPONENT_USAGE=RGB | D_MAPPING=UNSIGNED_INVERT_NV | C_INPUT=TEXTURE0_ARB | C_COMPONENT_USAGE=RGB | C_MAPPING=UNSIGNED_IDENTITY_NV | B_INPUT=SPARE0_NV | B_COMPONENT_USAGE=RGB | B_MAPPING=UNSIGNED_IDENTITY_NV | A_INPUT=TEXTURE0_ARB | A_COMPONENT_USAGE=RGB | A_MAPPING=UNSIGNED_IDENTITY_NV
00000000    NV10_TCL_PRIMITIVE_3D.RC_COLOR[0] = B=0 | G=0 | R=0 | A=0
00000000    NV10_TCL_PRIMITIVE_3D.RC_COLOR[1] = B=0 | G=0 | R=0 | A=0
00000c00    NV10_TCL_PRIMITIVE_3D.RC_OUT_ALPHA[0] = CD_OUTPUT=ZERO | AB_OUTPUT=ZERO | SUM_OUTPUT=SPARE0_NV | CD_DOT_PRODUCT=FALSE | AB_DOT_PRODUCT=FALSE | MUX_SUM=FALSE | BIAS=NONE | SCALE=NONE | leftover=0x00000000/0x0000ffff
00000c00    NV10_TCL_PRIMITIVE_3D.RC_OUT_ALPHA[1] = CD_OUTPUT=ZERO | AB_OUTPUT=ZERO | SUM_OUTPUT=SPARE0_NV | CD_DOT_PRODUCT=FALSE | AB_DOT_PRODUCT=FALSE | MUX_SUM=FALSE | BIAS=NONE | SCALE=NONE | leftover=0x00000000/0x0000ffff
000010cd    NV10_TCL_PRIMITIVE_3D.RC_OUT_RGB[0] = CD_OUTPUT=SPARE1_NV | AB_OUTPUT=SPARE0_NV | SUM_OUTPUT=ZERO | CD_DOT_PRODUCT=TRUE | AB_DOT_PRODUCT=FALSE | MUX_SUM=FALSE | BIAS=NONE | SCALE=NONE | OPERATION=0 | leftover=0x00000000/0x3800ffff
280010cd    NV10_TCL_PRIMITIVE_3D.RC_OUT_RGB[1] = CD_OUTPUT=SPARE1_NV | AB_OUTPUT=SPARE0_NV | SUM_OUTPUT=ZERO | CD_DOT_PRODUCT=TRUE | AB_DOT_PRODUCT=FALSE | MUX_SUM=FALSE | BIAS=NONE | SCALE=NONE | OPERATION=5 | leftover=0x00000000/0x3800ffff
300e0300    NV10_TCL_PRIMITIVE_3D.RC_FINAL0 = D_INPUT=ZERO | D_COMPONENT_USAGE=RGB | D_MAPPING=UNSIGNED_IDENTITY_NV | C_INPUT=FOG | C_COMPONENT_USAGE=RGB | C_MAPPING=UNSIGNED_IDENTITY_NV | B_INPUT=SPARE0_PLUS_SECONDARY_COLOR_NV | B_COMPONENT_USAGE=RGB | B_MAPPING=UNSIGNED_IDENTITY_NV | A_INPUT=ZERO | A_COMPONENT_USAGE=ALPHA | A_MAPPING=UNSIGNED_INVERT_NV
0c091c80    NV10_TCL_PRIMITIVE_3D.RC_FINAL1 = COLOR_SUM_CLAMP=TRUE | G_INPUT=SPARE0_NV | G_COMPONENT_USAGE=ALPHA | G_MAPPING=UNSIGNED_IDENTITY_NV | F_INPUT=TEXTURE0_ARB | F_COMPONENT_USAGE=RGB | F_MAPPING=UNSIGNED_IDENTITY_NV | E_INPUT=SPARE0_NV | E_COMPONENT_USAGE=RGB | E_MAPPING=UNSIGNED_IDENTITY_NV | leftover=0x00000000/0xffffff80
00042294  size 1, subchannel 1 (0xbeef5601),offset 0x0294,increment
00000000    NV10_TCL_PRIMITIVE_3D.LIGHT_MODEL = COLOR_CONTROL=0 | LOCAL_VIEWER=FALSE | leftover=0x00000000/0x00010002
000423b8  size 1, subchannel 1 (0xbeef5601),offset 0x03b8,increment
00000000    NV10_TCL_PRIMITIVE_3D.COLOR_CONTROL
000423bc  size 1, subchannel 1 (0xbeef5601),offset 0x03bc,increment
00000000    NV10_TCL_PRIMITIVE_3D.ENABLED_LIGHTS = LIGHT0=FALSE | LIGHT1=FALSE | LIGHT2=FALSE | LIGHT3=FALSE | LIGHT4=FALSE | LIGHT5=FALSE | LIGHT6=FALSE | LIGHT7=FALSE | leftover=0x00000000/0x00005555
00402500  size 16, subchannel 1 (0xbeef5601),offset 0x0500,increment
*/		
	}
	else
	{
	}
}

static void NV10SetPictOp(NVPtr pNv,int op)
{
	struct {int src;int dst;} pictops[] =
	{
		{0x0000,0x0000}, // PictOpClear
		{0x0001,0x0000}, // PictOpSrc 
		{0x0000,0x0001}, // PictOpDst
		{0x0001,0x0303}, // PictOpOver
		{0x0305,0x0001}, // PictOpOverReverse
		{0x0304,0x0000}, // PictOpIn
		{0x0000,0x0302}, // PictOpInReverse
		{0x0305,0x0000}, // PictOpOut
		{0x0000,0x0303}, // PictOpOutReverse
		{0x0304,0x0303}, // PictOpAtop
		{0x0305,0x0302}, // PictOpAtopReverse
		{0x0305,0x0303}, // PictOpXor
		{0x0001,0x0001}, // PictOpAdd
	};

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_BLEND_FUNC_SRC, 2);
	NVDmaNext (pNv, pictops[op].src);
	NVDmaNext (pNv, pictops[op].dst);
}

static Bool NV10PrepareComposite(int	  op,
			       PicturePtr pSrcPicture,
			       PicturePtr pMaskPicture,
			       PicturePtr pDstPicture,
			       PixmapPtr  pSrc,
			       PixmapPtr  pMask,
			       PixmapPtr  pDst)
{
	ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	/* Set dst format */
	NV10SetBuffer(pNv,pDstPicture,pDst);

	/* Set src format */
	NV10SetTexture(pNv,0,pSrcPicture,pSrc);

	/* Set mask format */
	if (pMaskPicture)
		NV10SetTexture(pNv,1,pMaskPicture,pMask);

	/* Set Multitexturing */
	NV10SetMultitexture(pNv, (pMaskPicture!=NULL));

	/* Set PictOp */
	NV10SetPictOp(pNv, op);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_BEGIN_END, 1);
	NVDmaNext (pNv, 8); /* GL_QUADS + 1 */

	state.have_mask=(pMaskPicture!=NULL);
	return TRUE;
}

static inline void NV10Vertex(NVPtr pNv,float vx,float vy,float tx,float ty)
{
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_TX0_2F_S, 2);
	NVDmaFloat(pNv, tx);
	NVDmaFloat(pNv, ty);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_POS_3F_X, 3);
	NVDmaFloat(pNv, vx);
	NVDmaFloat(pNv, vy);
	NVDmaFloat(pNv, 0.f);
}

static inline void NV10MVertex(NVPtr pNv,float vx,float vy,float t0x,float t0y,float t1x,float t1y)
{
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_TX0_2F_S, 2);
	NVDmaFloat(pNv, t0x);
	NVDmaFloat(pNv, t0y);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_TX1_2F_S, 2);
	NVDmaFloat(pNv, t1x);
	NVDmaFloat(pNv, t1y);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_POS_3F_X, 3);
	NVDmaFloat(pNv, vx);
	NVDmaFloat(pNv, vy);
	NVDmaFloat(pNv, 0.f);
}

#define xFixedToFloat(v) \
	((float)xFixedToInt((v)) + ((float)xFixedFrac(v) / 65536.0))

static void
NV10EXATransformCoord(PictTransformPtr t, int x, int y, float sx, float sy,
					  float *x_ret, float *y_ret)
{
	PictVector v;

	if (t) {
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


static void NV10Composite(PixmapPtr pDst,
			int	  srcX,
			int	  srcY,
			int	  maskX,
			int	  maskY,
			int	  dstX,
			int	  dstY,
			int	  width,
			int	  height)
{
	ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	float sX0, sX1, sY0, sY1;
	float mX0, mX1, mY0, mY1;

	NV10EXATransformCoord(state.unit[0].transform, srcX, srcY,
			      state.unit[0].width,
			      state.unit[0].height, &sX0, &sY0);
	NV10EXATransformCoord(state.unit[0].transform,
			      srcX + width, srcY + height,
			      state.unit[0].width,
			      state.unit[0].height, &sX1, &sY1);

	if (state.have_mask) {
		NV10EXATransformCoord(state.unit[1].transform, maskX, maskY,
				      state.unit[1].width,
				      state.unit[1].height, &mX0, &mY0);
		NV10EXATransformCoord(state.unit[1].transform,
				      maskX + width, maskY + height,
				      state.unit[1].width,
				      state.unit[1].height, &mX1, &mY1);
		NV10MVertex(pNv , dstX         ,          dstY,sX0 , sY0 , mX0 , mY0);
		NV10MVertex(pNv , dstX + width ,          dstY,sX1 , sY0 , mX1 , mY0);
		NV10MVertex(pNv , dstX + width , dstY + height,sX1 , sY1 , mX1 , mY1);
		NV10MVertex(pNv , dstX         , dstY + height,sX0 , sY1 , mX0 , mY1);
	} else {
		NV10Vertex(pNv , dstX         ,          dstY , sX0 , sY0);
		NV10Vertex(pNv , dstX + width ,          dstY , sX1 , sY0);
		NV10Vertex(pNv , dstX + width , dstY + height , sX1 , sY1);
		NV10Vertex(pNv , dstX         , dstY + height , sX0 , sY1);
	}

	NVDmaKickoff(pNv);
}

static void NV10DoneComposite (PixmapPtr pDst)
{
	ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_BEGIN_END, 1);
	NVDmaNext (pNv, 0); /* STOP */

	exaMarkSync(pDst->drawable.pScreen);
}


Bool
NVAccelInitNV10TCL(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	static int have_object = FALSE;
	uint32_t class = 0, chipset;
	int i;

	chipset = (nvReadMC(pNv, 0) >> 20) & 0xff;
	if (	((chipset & 0xf0) != NV_ARCH_10) &&
		((chipset & 0xf0) != NV_ARCH_20) )
		return FALSE;

	if (chipset>=0x20)
		class = NV11_TCL_PRIMITIVE_3D;
	else if (chipset>=0x17)
		class = NV17_TCL_PRIMITIVE_3D;
	else if (chipset>=0x11)
		class = NV11_TCL_PRIMITIVE_3D;
	else
		class = NV10_TCL_PRIMITIVE_3D;

	if (!have_object) {
		if (!NVDmaCreateContextObject(pNv, Nv3D, class))
			return FALSE;
		have_object = TRUE;
	}

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_SET_DMA_NOTIFY, 1);
	NVDmaNext (pNv, NvNullObject);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_SET_DMA_IN_MEMORY0, 2);
	NVDmaNext (pNv, NvDmaFB);
	NVDmaNext (pNv, NvDmaTT);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_SET_DMA_IN_MEMORY2, 2);
	NVDmaNext (pNv, NvDmaFB);
	NVDmaNext (pNv, NvDmaFB);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_NOP, 1);
	NVDmaNext (pNv, 0);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_HORIZ, 2);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_HORIZ(0), 1);
	NVDmaNext (pNv, (0x7ff<<16)|0x800);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_VERT(0), 1);
	NVDmaNext (pNv, (0x7ff<<16)|0x800);

	for (i=1;i<8;i++) {
		NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_HORIZ(i), 1);
		NVDmaNext (pNv, 0);
		NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_VERT(i), 1);
		NVDmaNext (pNv, 0);
	}

	NVDmaStart(pNv, Nv3D, 0x290, 1);
	NVDmaNext (pNv, (0x10<<16)|1);
	NVDmaStart(pNv, Nv3D, 0x3f4, 1);
	NVDmaNext (pNv, 0);

//	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_NOTIFY, 1);
//	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_NOP, 1);
	NVDmaNext (pNv, 0);

#if IS_NV11
	NVDmaStart(pNv, Nv3D, 0x120, 3);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 1);
	NVDmaNext (pNv, 2);

	NVDmaStart(pNv, NvImageBlit, 0x120, 3);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 1);
	NVDmaNext (pNv, 2);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_NOP, 1);
	NVDmaNext (pNv, 0);
#endif

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_NOP, 1);
	NVDmaNext (pNv, 0);

	/* Set state */
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_FOG_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_ALPHA_FUNC_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_ALPHA_FUNC_FUNC, 2);
	NVDmaNext (pNv, 0x207);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_TX_ENABLE(0), 2);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA(0), 12);
	NVDmaNext (pNv, 0x30141010);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0x20040000);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0x00000c00);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0x00000c00);
	NVDmaNext (pNv, 0x18000000);
	NVDmaNext (pNv, 0x300e0300);
	NVDmaNext (pNv, 0x0c091c80);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_BLEND_FUNC_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_DITHER_ENABLE, 2);
	NVDmaNext (pNv, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_LINE_SMOOTH_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_WEIGHT_ENABLE, 2);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_BLEND_FUNC_SRC, 4);
	NVDmaNext (pNv, 1);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0x8006);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_STENCIL_MASK, 8);
	NVDmaNext (pNv, 0xff);
	NVDmaNext (pNv, 0x207);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0xff);
	NVDmaNext (pNv, 0x1e00);
	NVDmaNext (pNv, 0x1e00);
	NVDmaNext (pNv, 0x1e00);
	NVDmaNext (pNv, 0x1d01);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_NORMALIZE_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_FOG_ENABLE, 2);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_LIGHT_MODEL, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_COLOR_CONTROL, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_ENABLED_LIGHTS, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_POLYGON_OFFSET_POINT_ENABLE, 3);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_DEPTH_FUNC, 1);
	NVDmaNext (pNv, 0x201);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_DEPTH_WRITE_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_DEPTH_TEST_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_POLYGON_OFFSET_FACTOR, 2);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_POINT_SIZE, 1);
	NVDmaNext (pNv, 8);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_POINT_PARAMETERS_ENABLE, 2);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_LINE_WIDTH, 1);
	NVDmaNext (pNv, 8);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_LINE_SMOOTH_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_POLYGON_MODE_FRONT, 2);
	NVDmaNext (pNv, 0x1b02);
	NVDmaNext (pNv, 0x1b02);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_CULL_FACE, 2);
	NVDmaNext (pNv, 0x405);
	NVDmaNext (pNv, 0x901);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_POLYGON_SMOOTH_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_CULL_FACE_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_CLIP_PLANE_ENABLE(0), 8);
	for (i=0;i<8;i++) {
		NVDmaNext (pNv, 0);
	}
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_FOG_EQUATION_CONSTANT, 3);
	NVDmaNext (pNv, 0x3fc00000);	/* -1.50 */
	NVDmaNext (pNv, 0xbdb8aa0a);	/* -0.09 */
	NVDmaNext (pNv, 0);		/*  0.00 */

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_NOP, 1);
	NVDmaNext (pNv, 0);

	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_FOG_MODE, 2);
	NVDmaNext (pNv, 0x802);
	NVDmaNext (pNv, 2);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VIEW_MATRIX_ENABLE, 1);
	NVDmaNext (pNv, 4);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_COLOR_MASK, 1);
	NVDmaNext (pNv, 0x01010101);

	/* Set vertex component */
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_COL_4F_R, 4);
	NVDmaFloat (pNv, 0.6);
	NVDmaFloat (pNv, 0.4);
	NVDmaFloat (pNv, 0.2);
	NVDmaFloat (pNv, 1.0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_COL2_3F_R, 3);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_NOR_3F_X, 3);
	NVDmaNext (pNv, 0);
	NVDmaNext (pNv, 0);
	NVDmaFloat (pNv, 1.0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_TX0_4F_S, 4);
	NVDmaFloat (pNv, 0.0);
	NVDmaFloat (pNv, 0.0);
	NVDmaFloat (pNv, 0.0);
	NVDmaFloat (pNv, 1.0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_TX1_4F_S, 4);
	NVDmaFloat (pNv, 0.0);
	NVDmaFloat (pNv, 0.0);
	NVDmaFloat (pNv, 0.0);
	NVDmaFloat (pNv, 1.0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_FOG_1F, 1);
	NVDmaFloat (pNv, 0.0);
	NVDmaStart(pNv, Nv3D, NV10_TCL_PRIMITIVE_3D_EDGEFLAG_ENABLE, 1);
	NVDmaNext (pNv, 1);

	return TRUE;
}




