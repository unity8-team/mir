/*
 * Copyright 2007 Stephane Marchesin
 * Copyright 2007 Arthur Huillet
 * Copyright 2007 Peter Winters
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
		{PICT_a8r8g8b8,	0x900},
		{PICT_x8r8g8b8,	0x900},
		//{PICT_a1r5g5b5,	NV10_TCL_PRIMITIVE_3D_TX_FORMAT_FORMAT_R5G5B5A1},
		//{PICT_a4r4g4b4,	NV10_TCL_PRIMITIVE_3D_TX_FORMAT_FORMAT_R4G4B4A4},
		{PICT_a8,	0x03 << 7 | 1 << 11},
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
		{PICT_x8r8g8b8, 0x108}, //FIXME blending factors?
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
	if (Picture->componentAlpha)
		return FALSE;
	/* we cannot repeat on NV10 because NPOT textures do not support this. unfortunately. */
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
	if (Picture->componentAlpha) //this is used by rendercheck CA composite tests. not sure about real-life.
		return FALSE;
	if (!NV10DstFormat(Picture->format))
		return FALSE;
	return TRUE;
}

static Bool NV10CheckPictOp(int op)
{
	if ( op == PictOpAtopReverse ) /*this op doesn't work right now*/
		{
		return FALSE;
		}
	if ( op >= PictOpSaturate )
		{ //we do no saturate, disjoint, conjoint, though we could do e.g. DisjointClear which really is Clear
		return FALSE;
		}
	return TRUE;
}	

#if 0
#define NV10EXAFallbackInfo(X,Y,Z,S,T) NV10EXAFallbackInfo_real(X,Y,Z,S,T)
#else
#define NV10EXAFallbackInfo(X,Y,Z,S,T) do { ; } while (0);
#endif

static void NV10EXAFallbackInfo_real(char * reason, int op, PicturePtr pSrcPicture,
			     PicturePtr pMaskPicture,
			     PicturePtr pDstPicture)
{
	char out2[4096];
	char * out = out2;
	sprintf(out, "%s  ", reason);
	out = out + strlen(out);
	switch ( op )
		{
		case PictOpOver:
			sprintf(out, "PictOpOver ");
			break;
		case PictOpOutReverse:
			sprintf(out, "PictOpOutReverse ");
			break;
		case PictOpAdd:
			sprintf(out, "PictOpAdd ");
			break;
		default :
			sprintf(out, "PictOp%d ", op);
		}
	out = out + strlen(out);
	switch ( pSrcPicture->format )
		{
		case PICT_a8r8g8b8:
			sprintf(out, "A8R8G8B8 -> ");
			break;
		case PICT_x8r8g8b8:
			sprintf(out, "X8R8G8B8 -> ");
			break;
		case PICT_x8b8g8r8:
			sprintf(out, "X8B8G8R8 -> ");
			break;
		case PICT_a8:
			sprintf(out, "A8 -> ");
			break;
		case PICT_a1:
			sprintf(out, "A1 -> ");
			break;
		default:
			sprintf(out, "%x -> ", pSrcPicture->format);
		}
	out+=strlen(out);
	switch ( pDstPicture->format )
		{
		case PICT_a8r8g8b8:
			sprintf(out, "A8R8G8B8 ");
			break;
		case PICT_x8r8g8b8:
			sprintf(out, "X8R8G8B8  ");
			break;
		case PICT_x8b8g8r8:
			sprintf(out, "X8B8G8R8  ");
			break;
		case PICT_a8:
			sprintf(out, "A8  ");
			break;
		case PICT_a1:
			sprintf(out, "A1  ");
			break;
		default:
			sprintf(out, "%x  ", pDstPicture->format);
		}
	out+=strlen(out);
	if ( !pMaskPicture ) 
		sprintf(out, "& NONE");
	else
	switch ( pMaskPicture->format )
		{
		case PICT_a8r8g8b8:
			sprintf(out, "& A8R8G8B8 ");
			break;
		case PICT_x8r8g8b8:
			sprintf(out, "& X8R8G8B8  ");
			break;
		case PICT_x8b8g8r8:
			sprintf(out, "& X8B8G8R8  ");
			break;
		case PICT_a8:
			sprintf(out, "& A8  ");
			break;
		case PICT_a1:
			sprintf(out, "& A1  ");
			break;
		default:
			sprintf(out, "& %x  ", pMaskPicture->format);
		}
	strcat(out, "\n");
	xf86DrvMsg(0, X_INFO, out2);
}


Bool NV10CheckComposite(int	op,
			     PicturePtr pSrcPicture,
			     PicturePtr pMaskPicture,
			     PicturePtr pDstPicture)
{
	// XXX A8 + A8 special case "TO BE DONE LATER"
/*	if ((!pMaskPicture) &&
			(pSrcPicture->format == PICT_a8) &&
			(pDstPicture->format == PICT_a8) )
		return TRUE;*/

	if (!NV10CheckPictOp(op))
		{
		NV10EXAFallbackInfo("pictop", op, pSrcPicture, pMaskPicture, pDstPicture);
		return FALSE;
		}
	if (!NV10CheckBuffer(pDstPicture)) 
		{
		NV10EXAFallbackInfo("dst", op, pSrcPicture, pMaskPicture, pDstPicture);
		return FALSE;
		}
		
	if (!NV10CheckTexture(pSrcPicture))
		{
		NV10EXAFallbackInfo("src", op, pSrcPicture, pMaskPicture, pDstPicture);
		return FALSE;
		}
		
	if ((pMaskPicture) &&(!NV10CheckTexture(pMaskPicture)))
		{
		NV10EXAFallbackInfo("mask", op, pSrcPicture, pMaskPicture, pDstPicture);
		return FALSE;
		}
		
	NV10EXAFallbackInfo("Accelerating", op, pSrcPicture, pMaskPicture, pDstPicture);
	return TRUE;
}

static void NV10SetTexture(NVPtr pNv,int unit,PicturePtr Pict,PixmapPtr pixmap)
{
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_TX_OFFSET(unit), 1 );
	OUT_RING  (NVAccelGetPixmapOffset(pixmap));
	int log2w = log2i(Pict->pDrawable->width);
	int log2h = log2i(Pict->pDrawable->height);
	int w;
	unsigned int txfmt =
			(NV10_TCL_PRIMITIVE_3D_TX_FORMAT_WRAP_T_CLAMP_TO_EDGE) | (NV10_TCL_PRIMITIVE_3D_TX_FORMAT_WRAP_S_CLAMP_TO_EDGE) |
			(log2w<<20) |
			(log2h<<16) |
			(1<<12) | /* lod == 1 */
			(NV10TexFormat(Pict->format)) |
			0x51 /* UNK */;

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_TX_FORMAT(unit), 1 );
	OUT_RING  (txfmt);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_TX_ENABLE(unit), 1 );
	OUT_RING  (NV10_TCL_PRIMITIVE_3D_TX_ENABLE_ENABLE);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_TX_NPOT_PITCH(unit), 1);
	OUT_RING  (exaGetPixmapPitch(pixmap) << 16);

	/* NPOT_SIZE expects an even number for width, we can round up uneven
	 * numbers here because EXA always gives 64 byte aligned pixmaps
	 * and for all formats we support 64 bytes represents an even number
	 * of pixels
	 */
	w = Pict->pDrawable->width;
	if (w & 1)
		w++;

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_TX_NPOT_SIZE(unit), 1);
	OUT_RING  ((w<<16) | Pict->pDrawable->height);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_TX_FILTER(unit), 1);
	if (Pict->filter == PictFilterNearest)
		OUT_RING  ((NV10_TCL_PRIMITIVE_3D_TX_FILTER_MAGNIFY_NEAREST) |
				(NV10_TCL_PRIMITIVE_3D_TX_FILTER_MINIFY_NEAREST));
	else
		OUT_RING  ((NV10_TCL_PRIMITIVE_3D_TX_FILTER_MAGNIFY_LINEAR) |
				(NV10_TCL_PRIMITIVE_3D_TX_FILTER_MINIFY_LINEAR));

	state.unit[unit].width		= (float)pixmap->drawable.width;
	state.unit[unit].height		= (float)pixmap->drawable.height;
	state.unit[unit].transform	= Pict->transform;
}

static void NV10SetBuffer(NVPtr pNv,PicturePtr Pict,PixmapPtr pixmap)
{
	int i;
	int x = Pict->pDrawable->x;
	int y = Pict->pDrawable->y;
	int w = Pict->pDrawable->width;
	int h = Pict->pDrawable->height;

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_BUFFER_FORMAT, 4);
	OUT_RING  (NV10DstFormat(Pict->format));
	OUT_RING  (((uint32_t)exaGetPixmapPitch(pixmap) << 16) |(uint32_t)exaGetPixmapPitch(pixmap));
	OUT_RING  (NVAccelGetPixmapOffset(pixmap));
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_HORIZ, 2);
	OUT_RING  ((w<<16)|x);
	OUT_RING  ((h<<16)|y);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_MODE, 1); /* clip_mode */
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_HORIZ(0), 1);
	OUT_RING  (((w-1+x)<<16)|x|0x08000800);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_VERT(0), 1);
	OUT_RING  (((h-1+y)<<16)|y|0x08000800);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_PROJECTION_MATRIX(0), 16);
	for(i=0;i<16;i++)
		if (i/4==i%4)
			OUT_RINGf (1.0f);
		else
			OUT_RINGf (0.0f);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_DEPTH_RANGE_NEAR, 2);
	OUT_RING  (0);
#if SCREEN_BPP == 32
	OUT_RINGf (16777216.0);
#else
	OUT_RINGf (65536.0);
#endif
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_SCALE_X, 4);
	OUT_RINGf (-2048.0);
	OUT_RINGf (-2048.0);
	OUT_RINGf (0);
	OUT_RING  (0);
}

static void NV10SetRegCombs(NVPtr pNv, PicturePtr src, PicturePtr mask)
{
/*This can be a bit difficult to understand at first glance.
Reg combiners are described here:
http://icps.u-strasbg.fr/~marchesin/perso/extensions/NV/register_combiners.html
	
Single texturing setup, without honoring vertex colors (non default setup) is:
Alpha RC 0 : a_0  * 1 + 0 * 0
RGB RC 0 : rgb_0 * 1 + 0 * 0
RC 1s are unused
	
Default setup uses vertex rgb/alpha in place of 1s above, but we don't need that in 2D.
	
Multi texturing setup, where we do TEX0 in TEX1 (masking) is:
Alpha RC 0 : a_0 * a_1 + 0 * 0
RGB RC0 : rgb_0 * a_1 + 0 * 0
RC 1s are unused
	
*/

unsigned int rc0_in_alpha = 0, rc0_in_rgb = 0;
unsigned int rc1_in_alpha = 0, rc1_in_rgb = 0;

#define A_ALPHA_ZERO (NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_A_INPUT_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_A_COMPONENT_USAGE_ALPHA)
#define B_ALPHA_ZERO (NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_B_INPUT_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_B_COMPONENT_USAGE_ALPHA)
#define C_ALPHA_ZERO (NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_C_INPUT_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_C_COMPONENT_USAGE_ALPHA)
#define D_ALPHA_ZERO (NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_D_INPUT_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_D_COMPONENT_USAGE_ALPHA)
	
#define A_ALPHA_ONE (A_ALPHA_ZERO | (NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_A_MAPPING_UNSIGNED_INVERT_NV))
#define B_ALPHA_ONE (B_ALPHA_ZERO | (NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_B_MAPPING_UNSIGNED_INVERT_NV))
#define C_ALPHA_ONE (C_ALPHA_ZERO | (NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_C_MAPPING_UNSIGNED_INVERT_NV))
#define D_ALPHA_ONE (D_ALPHA_ZERO | (NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA_D_MAPPING_UNSIGNED_INVERT_NV))

#define A_RGB_ZERO (NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_A_INPUT_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_A_COMPONENT_USAGE_RGB)
#define B_RGB_ZERO (NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_B_INPUT_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_B_COMPONENT_USAGE_RGB)
#define C_RGB_ZERO (NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_C_INPUT_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_C_COMPONENT_USAGE_RGB)
#define D_RGB_ZERO (NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_D_INPUT_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_D_COMPONENT_USAGE_RGB)

#define A_RGB_ONE (A_RGB_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_A_MAPPING_UNSIGNED_INVERT_NV)
#define B_RGB_ONE (B_RGB_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_B_MAPPING_UNSIGNED_INVERT_NV)
#define C_RGB_ONE (C_RGB_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_C_MAPPING_UNSIGNED_INVERT_NV)
#define D_RGB_ONE (D_RGB_ZERO | NV10_TCL_PRIMITIVE_3D_RC_IN_RGB_D_MAPPING_UNSIGNED_INVERT_NV)

			
	rc0_in_alpha |= C_ALPHA_ZERO | D_ALPHA_ZERO;
	if (src->format == PICT_x8r8g8b8)
		rc0_in_alpha |= A_ALPHA_ONE; //A = alpha = 1 everywhere
	else
		rc0_in_alpha |= 0x18000000; //A = a_0, use texture 0 alpha value
	
	if ( ! mask ) 
		rc0_in_alpha |= B_ALPHA_ONE;
	else 
		if ( mask->format == PICT_x8r8g8b8 )  //no alpha? ignore it
			rc0_in_alpha |= B_ALPHA_ONE;
		else
			rc0_in_alpha |= 0x00190000; //B = a_1, use texture 1 alpha value
	
	rc0_in_rgb |=  C_RGB_ZERO | D_RGB_ZERO;
	if (src->format == PICT_a8 )
		rc0_in_rgb |= A_RGB_ZERO;
	else 
		rc0_in_rgb |= 0x08000000; //A = rgb_0, use texture 0 rgb
	
	if ( ! mask )
		rc0_in_rgb |= B_RGB_ONE;
	else 
		if (  mask->format == PICT_x8r8g8b8 )  //no alpha? ignore it
			rc0_in_rgb |= B_RGB_ONE;
		else
			rc0_in_rgb |= 0x00190000; //B = a_1, use texture 1 alpha value
	
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA(0), 12);
	OUT_RING(rc0_in_alpha);
	OUT_RING  (rc1_in_alpha);
	OUT_RING (rc0_in_rgb);
	OUT_RING  (rc1_in_rgb);
	OUT_RING  (0); /*COLOR 0*/
	OUT_RING  (0); /*COLOR 1*/
	OUT_RING  (0x00000c00);
	OUT_RING  (0);
	OUT_RING  (0x000010cd);
	OUT_RING  (0x18000000);
	OUT_RING  (0x300e0300);
	OUT_RING  (0x0c091c80);
}

static void NV10SetPictOp(NVPtr pNv,int op, int sf, int df)
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
	
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_BLEND_FUNC_SRC, 2);
	OUT_RING  (pictops[op].src);
	OUT_RING  (pictops[op].dst);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_BLEND_FUNC_ENABLE, 1);
	OUT_RING  (1);
}

Bool NV10PrepareComposite(int	  op,
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

	NV10SetRegCombs(pNv, pSrcPicture, pMaskPicture);

	/* Set PictOp */
	NV10SetPictOp(pNv, op, pSrcPicture->format, pDstPicture->format);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_BEGIN_END, 1);
	OUT_RING  (NV10_TCL_PRIMITIVE_3D_VERTEX_BEGIN_END_QUADS);

	state.have_mask=(pMaskPicture!=NULL);
	return TRUE;
}

static inline void NV10Vertex(NVPtr pNv,float vx,float vy,float tx,float ty)
{
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_TX0_2F_S, 2);
	OUT_RINGf (tx);
	OUT_RINGf (ty);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_POS_3F_X, 3);
	OUT_RINGf (vx);
	OUT_RINGf (vy);
	OUT_RINGf (0.f);
}

static inline void NV10MVertex(NVPtr pNv,float vx,float vy,float t0x,float t0y,float t1x,float t1y)
{
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_TX0_2F_S, 2);
	OUT_RINGf (t0x);
	OUT_RINGf (t0y);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_TX1_2F_S, 2);
	OUT_RINGf (t1x);
	OUT_RINGf (t1y);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_POS_3F_X, 3);
	OUT_RINGf (vx);
	OUT_RINGf (vy);
	OUT_RINGf (0.f);
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
		*x_ret = xFixedToFloat(v.vector[0]);
		*y_ret = xFixedToFloat(v.vector[1]);
	} else {
		*x_ret = (float)x;
		*y_ret = (float)y;
	}
}


void NV10Composite(PixmapPtr pDst,
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

	FIRE_RING();
}

void NV10DoneComposite (PixmapPtr pDst)
{
	ScrnInfoPtr pScrn = xf86Screens[pDst->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_BEGIN_END, 1);
	OUT_RING  (NV10_TCL_PRIMITIVE_3D_VERTEX_BEGIN_END_STOP);

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

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_DMA_NOTIFY, 1);
	OUT_RING  (NvNullObject);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_DMA_IN_MEMORY0, 2);
	OUT_RING  (NvDmaFB);
	OUT_RING  (NvDmaTT);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_DMA_IN_MEMORY2, 2);
	OUT_RING  (NvDmaFB);
	OUT_RING  (NvDmaFB);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_NOP, 1);
	OUT_RING  (0);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_HORIZ, 2);
	OUT_RING  (0);
	OUT_RING  (0);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_HORIZ(0), 1);
	OUT_RING  ((0x7ff<<16)|0x800);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_VERT(0), 1);
	OUT_RING  ((0x7ff<<16)|0x800);

	for (i=1;i<8;i++) {
		BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_HORIZ(i), 1);
		OUT_RING  (0);
		BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEWPORT_CLIP_VERT(i), 1);
		OUT_RING  (0);
	}

	BEGIN_RING(Nv3D, 0x290, 1);
	OUT_RING  ((0x10<<16)|1);
	BEGIN_RING(Nv3D, 0x3f4, 1);
	OUT_RING  (0);

//	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_NOTIFY, 1);
//	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_NOP, 1);
	OUT_RING  (0);

	if (class != NV10_TCL_PRIMITIVE_3D) {
		/* For nv11, nv17 */
		BEGIN_RING(Nv3D, 0x120, 3);
		OUT_RING  (0);
		OUT_RING  (1);
		OUT_RING  (2);

		BEGIN_RING(NvImageBlit, 0x120, 3);
		OUT_RING  (0);
		OUT_RING  (1);
		OUT_RING  (2);

		BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_NOP, 1);
		OUT_RING  (0);
	}

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_NOP, 1);
	OUT_RING  (0);

	/* Set state */
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_FOG_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_ALPHA_FUNC_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_ALPHA_FUNC_FUNC, 2);
	OUT_RING  (0x207);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_TX_ENABLE(0), 2);
	OUT_RING  (0);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_RC_IN_ALPHA(0), 12);
	OUT_RING  (0x30141010);
	OUT_RING  (0);
	OUT_RING  (0x20040000);
	OUT_RING  (0);
	OUT_RING  (0);
	OUT_RING  (0);
	OUT_RING  (0x00000c00);
	OUT_RING  (0);
	OUT_RING  (0x00000c00);
	OUT_RING  (0x18000000);
	OUT_RING  (0x300e0300);
	OUT_RING  (0x0c091c80);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_BLEND_FUNC_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_DITHER_ENABLE, 2);
	OUT_RING  (1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_LINE_SMOOTH_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_WEIGHT_ENABLE, 2);
	OUT_RING  (0);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_BLEND_FUNC_SRC, 4);
	OUT_RING  (1);
	OUT_RING  (0);
	OUT_RING  (0);
	OUT_RING  (0x8006);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_STENCIL_MASK, 8);
	OUT_RING  (0xff);
	OUT_RING  (0x207);
	OUT_RING  (0);
	OUT_RING  (0xff);
	OUT_RING  (0x1e00);
	OUT_RING  (0x1e00);
	OUT_RING  (0x1e00);
	OUT_RING  (0x1d01);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_NORMALIZE_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_FOG_ENABLE, 2);
	OUT_RING  (0);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_LIGHT_MODEL, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_COLOR_CONTROL, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_ENABLED_LIGHTS, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_POLYGON_OFFSET_POINT_ENABLE, 3);
	OUT_RING  (0);
	OUT_RING  (0);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_DEPTH_FUNC, 1);
	OUT_RING  (0x201);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_DEPTH_WRITE_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_DEPTH_TEST_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_POLYGON_OFFSET_FACTOR, 2);
	OUT_RING  (0);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_POINT_SIZE, 1);
	OUT_RING  (8);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_POINT_PARAMETERS_ENABLE, 2);
	OUT_RING  (0);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_LINE_WIDTH, 1);
	OUT_RING  (8);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_LINE_SMOOTH_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_POLYGON_MODE_FRONT, 2);
	OUT_RING  (0x1b02);
	OUT_RING  (0x1b02);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_CULL_FACE, 2);
	OUT_RING  (0x405);
	OUT_RING  (0x901);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_POLYGON_SMOOTH_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_CULL_FACE_ENABLE, 1);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_CLIP_PLANE_ENABLE(0), 8);
	for (i=0;i<8;i++) {
		OUT_RING  (0);
	}
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_FOG_EQUATION_CONSTANT, 3);
	OUT_RING  (0x3fc00000);	/* -1.50 */
	OUT_RING  (0xbdb8aa0a);	/* -0.09 */
	OUT_RING  (0);		/*  0.00 */

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_NOP, 1);
	OUT_RING  (0);

	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_FOG_MODE, 2);
	OUT_RING  (0x802);
	OUT_RING  (2);
	/* for some reason VIEW_MATRIX_ENABLE need to be 6 instead of 4 when
	 * using texturing, except when using the texture matrix
	 */
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VIEW_MATRIX_ENABLE, 1);
	OUT_RING  (6);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_COLOR_MASK, 1);
	OUT_RING  (0x01010101);

	/* Set vertex component */
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_COL_4F_R, 4);
	OUT_RINGf (1.0);
	OUT_RINGf (1.0);
	OUT_RINGf (1.0);
	OUT_RINGf (1.0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_COL2_3F_R, 3);
	OUT_RING  (0);
	OUT_RING  (0);
	OUT_RING  (0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_NOR_3F_X, 3);
	OUT_RING  (0);
	OUT_RING  (0);
	OUT_RINGf (1.0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_TX0_4F_S, 4);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (1.0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_TX1_4F_S, 4);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (0.0);
	OUT_RINGf (1.0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_VERTEX_FOG_1F, 1);
	OUT_RINGf (0.0);
	BEGIN_RING(Nv3D, NV10_TCL_PRIMITIVE_3D_EDGEFLAG_ENABLE, 1);
	OUT_RING  (1);

	return TRUE;
}




