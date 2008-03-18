/*
 * Copyright 2005 Eric Anholt
 * Copyright 2005 Benjamin Herrenschmidt
 * All Rights Reserved.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <anholt@FreeBSD.org>
 *    Zack Rusin <zrusin@trolltech.com>
 *    Benjamin Herrenschmidt <benh@kernel.crashing.org>
 *
 */

#if defined(ACCEL_MMIO) && defined(ACCEL_CP)
#error Cannot define both MMIO and CP acceleration!
#endif

#if !defined(UNIXCPP) || defined(ANSICPP)
#define FUNC_NAME_CAT(prefix,suffix) prefix##suffix
#else
#define FUNC_NAME_CAT(prefix,suffix) prefix/**/suffix
#endif

#ifdef ACCEL_MMIO
#define FUNC_NAME(prefix) FUNC_NAME_CAT(prefix,MMIO)
#else
#ifdef ACCEL_CP
#define FUNC_NAME(prefix) FUNC_NAME_CAT(prefix,CP)
#else
#error No accel type defined!
#endif
#endif

#ifndef ACCEL_CP
#define ONLY_ONCE
#endif

/* Only include the following (generic) bits once. */
#ifdef ONLY_ONCE
static Bool is_transform[2];
static PictTransform *transform[2];

struct blendinfo {
    Bool dst_alpha;
    Bool src_alpha;
    CARD32 blend_cntl;
};

static struct blendinfo RadeonBlendOp[] = {
    /* Clear */
    {0, 0, RADEON_SRC_BLEND_GL_ZERO	      | RADEON_DST_BLEND_GL_ZERO},
    /* Src */
    {0, 0, RADEON_SRC_BLEND_GL_ONE	      | RADEON_DST_BLEND_GL_ZERO},
    /* Dst */
    {0, 0, RADEON_SRC_BLEND_GL_ZERO	      | RADEON_DST_BLEND_GL_ONE},
    /* Over */
    {0, 1, RADEON_SRC_BLEND_GL_ONE	      | RADEON_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA},
    /* OverReverse */
    {1, 0, RADEON_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA | RADEON_DST_BLEND_GL_ONE},
    /* In */
    {1, 0, RADEON_SRC_BLEND_GL_DST_ALPHA     | RADEON_DST_BLEND_GL_ZERO},
    /* InReverse */
    {0, 1, RADEON_SRC_BLEND_GL_ZERO	      | RADEON_DST_BLEND_GL_SRC_ALPHA},
    /* Out */
    {1, 0, RADEON_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA | RADEON_DST_BLEND_GL_ZERO},
    /* OutReverse */
    {0, 1, RADEON_SRC_BLEND_GL_ZERO	      | RADEON_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA},
    /* Atop */
    {1, 1, RADEON_SRC_BLEND_GL_DST_ALPHA     | RADEON_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA},
    /* AtopReverse */
    {1, 1, RADEON_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA | RADEON_DST_BLEND_GL_SRC_ALPHA},
    /* Xor */
    {1, 1, RADEON_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA | RADEON_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA},
    /* Add */
    {0, 0, RADEON_SRC_BLEND_GL_ONE	      | RADEON_DST_BLEND_GL_ONE},
};

struct formatinfo {
    int fmt;
    CARD32 card_fmt;
};

/* Note on texture formats:
 * TXFORMAT_Y8 expands to (Y,Y,Y,1).  TXFORMAT_I8 expands to (I,I,I,I)
 */
static struct formatinfo R100TexFormats[] = {
	{PICT_a8r8g8b8,	RADEON_TXFORMAT_ARGB8888 | RADEON_TXFORMAT_ALPHA_IN_MAP},
	{PICT_x8r8g8b8,	RADEON_TXFORMAT_ARGB8888},
	{PICT_r5g6b5,	RADEON_TXFORMAT_RGB565},
	{PICT_a1r5g5b5,	RADEON_TXFORMAT_ARGB1555 | RADEON_TXFORMAT_ALPHA_IN_MAP},
	{PICT_x1r5g5b5,	RADEON_TXFORMAT_ARGB1555},
	{PICT_a8,	RADEON_TXFORMAT_I8 | RADEON_TXFORMAT_ALPHA_IN_MAP},
};

static struct formatinfo R200TexFormats[] = {
    {PICT_a8r8g8b8,	R200_TXFORMAT_ARGB8888 | R200_TXFORMAT_ALPHA_IN_MAP},
    {PICT_x8r8g8b8,	R200_TXFORMAT_ARGB8888},
    {PICT_a8b8g8r8,	R200_TXFORMAT_ABGR8888 | R200_TXFORMAT_ALPHA_IN_MAP},
    {PICT_x8b8g8r8,	R200_TXFORMAT_ABGR8888},
    {PICT_r5g6b5,	R200_TXFORMAT_RGB565},
    {PICT_a1r5g5b5,	R200_TXFORMAT_ARGB1555 | R200_TXFORMAT_ALPHA_IN_MAP},
    {PICT_x1r5g5b5,	R200_TXFORMAT_ARGB1555},
    {PICT_a8,		R200_TXFORMAT_I8 | R200_TXFORMAT_ALPHA_IN_MAP},
};

static struct formatinfo R300TexFormats[] = {
    {PICT_a8r8g8b8,	R300_EASY_TX_FORMAT(X, Y, Z, W, W8Z8Y8X8)},
    {PICT_x8r8g8b8,	R300_EASY_TX_FORMAT(X, Y, Z, ONE, W8Z8Y8X8)},
    {PICT_a8b8g8r8,	R300_EASY_TX_FORMAT(Z, Y, X, W, W8Z8Y8X8)},
    {PICT_x8b8g8r8,	R300_EASY_TX_FORMAT(Z, Y, X, ONE, W8Z8Y8X8)},
    {PICT_r5g6b5,	R300_EASY_TX_FORMAT(X, Y, Z, ONE, Z5Y6X5)},
    {PICT_a1r5g5b5,	R300_EASY_TX_FORMAT(X, Y, Z, W, W1Z5Y5X5)},
    {PICT_x1r5g5b5,	R300_EASY_TX_FORMAT(X, Y, Z, ONE, W1Z5Y5X5)},
    {PICT_a8,		R300_EASY_TX_FORMAT(ZERO, ZERO, ZERO, X, X8)},
};

/* Common Radeon setup code */

static Bool RADEONGetDestFormat(PicturePtr pDstPicture, CARD32 *dst_format)
{
    switch (pDstPicture->format) {
    case PICT_a8r8g8b8:
    case PICT_x8r8g8b8:
	*dst_format = RADEON_COLOR_FORMAT_ARGB8888;
	break;
    case PICT_r5g6b5:
	*dst_format = RADEON_COLOR_FORMAT_RGB565;
	break;
    case PICT_a1r5g5b5:
    case PICT_x1r5g5b5:
	*dst_format = RADEON_COLOR_FORMAT_ARGB1555;
	break;
    case PICT_a8:
	*dst_format = RADEON_COLOR_FORMAT_RGB8;
	break;
    default:
	RADEON_FALLBACK(("Unsupported dest format 0x%x\n",
			(int)pDstPicture->format));
    }

    return TRUE;
}

static Bool R300GetDestFormat(PicturePtr pDstPicture, CARD32 *dst_format)
{
    switch (pDstPicture->format) {
    case PICT_a8r8g8b8:
    case PICT_x8r8g8b8:
	*dst_format = R300_COLORFORMAT_ARGB8888;
	break;
    case PICT_r5g6b5:
	*dst_format = R300_COLORFORMAT_RGB565;
	break;
    case PICT_a1r5g5b5:
    case PICT_x1r5g5b5:
	*dst_format = R300_COLORFORMAT_ARGB1555;
	break;
    case PICT_a8:
	*dst_format = R300_COLORFORMAT_I8;
	break;
    default:
	ErrorF("Unsupported dest format 0x%x\n",
	       (int)pDstPicture->format);
	return FALSE;
    }
    return TRUE;
}

static CARD32 RADEONGetBlendCntl(int op, PicturePtr pMask, CARD32 dst_format)
{
    CARD32 sblend, dblend;

    sblend = RadeonBlendOp[op].blend_cntl & RADEON_SRC_BLEND_MASK;
    dblend = RadeonBlendOp[op].blend_cntl & RADEON_DST_BLEND_MASK;

    /* If there's no dst alpha channel, adjust the blend op so that we'll treat
     * it as always 1.
     */
    if (PICT_FORMAT_A(dst_format) == 0 && RadeonBlendOp[op].dst_alpha) {
	if (sblend == RADEON_SRC_BLEND_GL_DST_ALPHA)
	    sblend = RADEON_SRC_BLEND_GL_ONE;
	else if (sblend == RADEON_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA)
	    sblend = RADEON_SRC_BLEND_GL_ZERO;
    }

    /* If the source alpha is being used, then we should only be in a case where
     * the source blend factor is 0, and the source blend value is the mask
     * channels multiplied by the source picture's alpha.
     */
    if (pMask && pMask->componentAlpha && RadeonBlendOp[op].src_alpha) {
	if (dblend == RADEON_DST_BLEND_GL_SRC_ALPHA) {
	    dblend = RADEON_DST_BLEND_GL_SRC_COLOR;
	} else if (dblend == RADEON_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA) {
	    dblend = RADEON_DST_BLEND_GL_ONE_MINUS_SRC_COLOR;
	}
    }

    return sblend | dblend;
}

union intfloat {
    float f;
    CARD32 i;
};

/* R100-specific code */

static Bool R100CheckCompositeTexture(PicturePtr pPict, int unit)
{
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    int i;

    if ((w > 0x7ff) || (h > 0x7ff))
	RADEON_FALLBACK(("Picture w/h too large (%dx%d)\n", w, h));

    for (i = 0; i < sizeof(R100TexFormats) / sizeof(R100TexFormats[0]); i++) {
	if (R100TexFormats[i].fmt == pPict->format)
	    break;
    }
    if (i == sizeof(R100TexFormats) / sizeof(R100TexFormats[0]))
	RADEON_FALLBACK(("Unsupported picture format 0x%x\n",
			(int)pPict->format));

    if (pPict->repeat && ((w & (w - 1)) != 0 || (h & (h - 1)) != 0))
	RADEON_FALLBACK(("NPOT repeat unsupported (%dx%d)\n", w, h));

    if (pPict->filter != PictFilterNearest &&
	pPict->filter != PictFilterBilinear)
    {
	RADEON_FALLBACK(("Unsupported filter 0x%x\n", pPict->filter));
    }

    return TRUE;
}

#endif /* ONLY_ONCE */

static Bool FUNC_NAME(R100TextureSetup)(PicturePtr pPict, PixmapPtr pPix,
					int unit)
{
    RINFO_FROM_SCREEN(pPix->drawable.pScreen);
    CARD32 txfilter, txformat, txoffset, txpitch;
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    int i;
    ACCEL_PREAMBLE();

    txpitch = exaGetPixmapPitch(pPix);
    txoffset = exaGetPixmapOffset(pPix) + info->fbLocation;

    if ((txoffset & 0x1f) != 0)
	RADEON_FALLBACK(("Bad texture offset 0x%x\n", (int)txoffset));
    if ((txpitch & 0x1f) != 0)
	RADEON_FALLBACK(("Bad texture pitch 0x%x\n", (int)txpitch));
    
    for (i = 0; i < sizeof(R100TexFormats) / sizeof(R100TexFormats[0]); i++)
    {
	if (R100TexFormats[i].fmt == pPict->format)
	    break;
    }
    txformat = R100TexFormats[i].card_fmt;
    if (RADEONPixmapIsColortiled(pPix))
	txoffset |= RADEON_TXO_MACRO_TILE;

    if (pPict->repeat) {
	if ((h != 1) &&
	    (((w * pPix->drawable.bitsPerPixel / 8 + 31) & ~31) != txpitch))
	    RADEON_FALLBACK(("Width %d and pitch %u not compatible for repeat\n",
			     w, (unsigned)txpitch));

	txformat |= RADEONLog2(w) << RADEON_TXFORMAT_WIDTH_SHIFT;
	txformat |= RADEONLog2(h) << RADEON_TXFORMAT_HEIGHT_SHIFT;
    } else
	txformat |= RADEON_TXFORMAT_NON_POWER2;
    txformat |= unit << 24; /* RADEON_TXFORMAT_ST_ROUTE_STQX */

    info->texW[unit] = 1;
    info->texH[unit] = 1;

    switch (pPict->filter) {
    case PictFilterNearest:
	txfilter = (RADEON_MAG_FILTER_NEAREST | RADEON_MIN_FILTER_NEAREST);
	break;
    case PictFilterBilinear:
	txfilter = (RADEON_MAG_FILTER_LINEAR | RADEON_MIN_FILTER_LINEAR);
	break;
    default:
	RADEON_FALLBACK(("Bad filter 0x%x\n", pPict->filter));
    }

    BEGIN_ACCEL(5);
    if (unit == 0) {
	OUT_ACCEL_REG(RADEON_PP_TXFILTER_0, txfilter);
	OUT_ACCEL_REG(RADEON_PP_TXFORMAT_0, txformat);
	OUT_ACCEL_REG(RADEON_PP_TXOFFSET_0, txoffset);
	OUT_ACCEL_REG(RADEON_PP_TEX_SIZE_0,
	    (pPix->drawable.width - 1) |
	    ((pPix->drawable.height - 1) << RADEON_TEX_VSIZE_SHIFT));
	OUT_ACCEL_REG(RADEON_PP_TEX_PITCH_0, txpitch - 32);
    } else {
	OUT_ACCEL_REG(RADEON_PP_TXFILTER_1, txfilter);
	OUT_ACCEL_REG(RADEON_PP_TXFORMAT_1, txformat);
	OUT_ACCEL_REG(RADEON_PP_TXOFFSET_1, txoffset);
	OUT_ACCEL_REG(RADEON_PP_TEX_SIZE_1,
	    (pPix->drawable.width - 1) |
	    ((pPix->drawable.height - 1) << RADEON_TEX_VSIZE_SHIFT));
	OUT_ACCEL_REG(RADEON_PP_TEX_PITCH_1, txpitch - 32);
    }
    FINISH_ACCEL();

    if (pPict->transform != 0) {
	is_transform[unit] = TRUE;
	transform[unit] = pPict->transform;
    } else {
	is_transform[unit] = FALSE;
    }

    return TRUE;
}

#ifdef ONLY_ONCE

static PixmapPtr
RADEONGetDrawablePixmap(DrawablePtr pDrawable)
{
    if (pDrawable->type == DRAWABLE_WINDOW)
	return pDrawable->pScreen->GetWindowPixmap((WindowPtr)pDrawable);
    else
	return (PixmapPtr)pDrawable;
}	

static Bool R100CheckComposite(int op, PicturePtr pSrcPicture,
			       PicturePtr pMaskPicture, PicturePtr pDstPicture)
{
    PixmapPtr pSrcPixmap, pDstPixmap;
    CARD32 tmp1;

    /* Check for unsupported compositing operations. */
    if (op >= sizeof(RadeonBlendOp) / sizeof(RadeonBlendOp[0]))
	RADEON_FALLBACK(("Unsupported Composite op 0x%x\n", op));

    if (!pSrcPicture->pDrawable)
	return FALSE;

    pSrcPixmap = RADEONGetDrawablePixmap(pSrcPicture->pDrawable);

    if (pSrcPixmap->drawable.width >= 2048 ||
	pSrcPixmap->drawable.height >= 2048) {
	RADEON_FALLBACK(("Source w/h too large (%d,%d).\n",
			 pSrcPixmap->drawable.width,
			 pSrcPixmap->drawable.height));
    }

    pDstPixmap = RADEONGetDrawablePixmap(pDstPicture->pDrawable);

    if (pDstPixmap->drawable.width >= 2048 ||
	pDstPixmap->drawable.height >= 2048) {
	RADEON_FALLBACK(("Dest w/h too large (%d,%d).\n",
			 pDstPixmap->drawable.width,
			 pDstPixmap->drawable.height));
    }

    if (pMaskPicture) {
	PixmapPtr pMaskPixmap = RADEONGetDrawablePixmap(pMaskPicture->pDrawable);

	if (pMaskPixmap->drawable.width >= 2048 ||
	    pMaskPixmap->drawable.height >= 2048) {
	    RADEON_FALLBACK(("Mask w/h too large (%d,%d).\n",
			     pMaskPixmap->drawable.width,
			     pMaskPixmap->drawable.height));
	}

	if (pMaskPicture->componentAlpha) {
	    /* Check if it's component alpha that relies on a source alpha and
	     * on the source value.  We can only get one of those into the
	     * single source value that we get to blend with.
	     */
	    if (RadeonBlendOp[op].src_alpha &&
		(RadeonBlendOp[op].blend_cntl & RADEON_SRC_BLEND_MASK) !=
		RADEON_SRC_BLEND_GL_ZERO) {
		RADEON_FALLBACK(("Component alpha not supported with source "
				 "alpha and source value blending.\n"));
	    }
	}

	if (!R100CheckCompositeTexture(pMaskPicture, 1))
	    return FALSE;
    }

    if (!R100CheckCompositeTexture(pSrcPicture, 0))
	return FALSE;

    if (!RADEONGetDestFormat(pDstPicture, &tmp1))
	return FALSE;

    return TRUE;
}
#endif /* ONLY_ONCE */

static Bool FUNC_NAME(R100PrepareComposite)(int op,
					    PicturePtr pSrcPicture,
					    PicturePtr pMaskPicture,
					    PicturePtr pDstPicture,
					    PixmapPtr pSrc,
					    PixmapPtr pMask,
					    PixmapPtr pDst)
{
    RINFO_FROM_SCREEN(pDst->drawable.pScreen);
    CARD32 dst_format, dst_offset, dst_pitch, colorpitch;
    CARD32 pp_cntl, blendcntl, cblend, ablend;
    int pixel_shift;
    ACCEL_PREAMBLE();

    TRACE;

    if (!info->XInited3D)
	RADEONInit3DEngine(pScrn);

    if (!RADEONGetDestFormat(pDstPicture, &dst_format))
    	return FALSE;

    pixel_shift = pDst->drawable.bitsPerPixel >> 4;

    dst_offset = exaGetPixmapOffset(pDst) + info->fbLocation;
    dst_pitch = exaGetPixmapPitch(pDst);
    colorpitch = dst_pitch >> pixel_shift;
    if (RADEONPixmapIsColortiled(pDst))
	colorpitch |= RADEON_COLOR_TILE_ENABLE;

    dst_offset = exaGetPixmapOffset(pDst) + info->fbLocation;
    dst_pitch = exaGetPixmapPitch(pDst);
    if ((dst_offset & 0x0f) != 0)
	RADEON_FALLBACK(("Bad destination offset 0x%x\n", (int)dst_offset));
    if (((dst_pitch >> pixel_shift) & 0x7) != 0)
	RADEON_FALLBACK(("Bad destination pitch 0x%x\n", (int)dst_pitch));

    if (!FUNC_NAME(R100TextureSetup)(pSrcPicture, pSrc, 0))
	return FALSE;
    pp_cntl = RADEON_TEX_0_ENABLE | RADEON_TEX_BLEND_0_ENABLE;

    if (pMask != NULL) {
	if (!FUNC_NAME(R100TextureSetup)(pMaskPicture, pMask, 1))
	    return FALSE;
	pp_cntl |= RADEON_TEX_1_ENABLE;
    } else {
	is_transform[1] = FALSE;
    }

    RADEON_SWITCH_TO_3D();

    BEGIN_ACCEL(8);
    OUT_ACCEL_REG(RADEON_PP_CNTL, pp_cntl);
    OUT_ACCEL_REG(RADEON_RB3D_CNTL, dst_format | RADEON_ALPHA_BLEND_ENABLE);
    OUT_ACCEL_REG(RADEON_RB3D_COLOROFFSET, dst_offset);
    OUT_ACCEL_REG(RADEON_RB3D_COLORPITCH, colorpitch);

    /* IN operator: Multiply src by mask components or mask alpha.
     * BLEND_CTL_ADD is A * B + C.
     * If a source is a8, we have to explicitly zero its color values.
     * If the destination is a8, we have to route the alpha to red, I think.
     * If we're doing component alpha where the source for blending is going to
     * be the source alpha (and there's no source value used), we have to zero
     * the source's color values.
     */
    cblend = RADEON_BLEND_CTL_ADD | RADEON_CLAMP_TX | RADEON_COLOR_ARG_C_ZERO;
    ablend = RADEON_BLEND_CTL_ADD | RADEON_CLAMP_TX | RADEON_ALPHA_ARG_C_ZERO;

    if (pDstPicture->format == PICT_a8 ||
	(pMask && pMaskPicture->componentAlpha && RadeonBlendOp[op].src_alpha))
    {
	cblend |= RADEON_COLOR_ARG_A_T0_ALPHA;
    } else if (pSrcPicture->format == PICT_a8)
	cblend |= RADEON_COLOR_ARG_A_ZERO;
    else
	cblend |= RADEON_COLOR_ARG_A_T0_COLOR;
    ablend |= RADEON_ALPHA_ARG_A_T0_ALPHA;

    if (pMask) {
	if (pMaskPicture->componentAlpha &&
	    pDstPicture->format != PICT_a8)
	    cblend |= RADEON_COLOR_ARG_B_T1_COLOR;
	else
	    cblend |= RADEON_COLOR_ARG_B_T1_ALPHA;
	ablend |= RADEON_ALPHA_ARG_B_T1_ALPHA;
    } else {
	cblend |= RADEON_COLOR_ARG_B_ZERO | RADEON_COMP_ARG_B;
	ablend |= RADEON_ALPHA_ARG_B_ZERO | RADEON_COMP_ARG_B;
    }

    OUT_ACCEL_REG(RADEON_PP_TXCBLEND_0, cblend);
    OUT_ACCEL_REG(RADEON_PP_TXABLEND_0, ablend);
    OUT_ACCEL_REG(RADEON_SE_VTX_FMT, RADEON_SE_VTX_FMT_XY |
				     RADEON_SE_VTX_FMT_ST0 |
				     RADEON_SE_VTX_FMT_ST1);
    /* Op operator. */
    blendcntl = RADEONGetBlendCntl(op, pMaskPicture, pDstPicture->format);

    OUT_ACCEL_REG(RADEON_RB3D_BLENDCNTL, blendcntl);
    FINISH_ACCEL();

    return TRUE;
}

#ifdef ONLY_ONCE

static Bool R200CheckCompositeTexture(PicturePtr pPict, int unit)
{
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    int i;

    if ((w > 0x7ff) || (h > 0x7ff))
	RADEON_FALLBACK(("Picture w/h too large (%dx%d)\n", w, h));

    for (i = 0; i < sizeof(R200TexFormats) / sizeof(R200TexFormats[0]); i++)
    {
	if (R200TexFormats[i].fmt == pPict->format)
	    break;
    }
    if (i == sizeof(R200TexFormats) / sizeof(R200TexFormats[0]))
	RADEON_FALLBACK(("Unsupported picture format 0x%x\n",
			 (int)pPict->format));

    if (pPict->repeat && ((w & (w - 1)) != 0 || (h & (h - 1)) != 0))
	RADEON_FALLBACK(("NPOT repeat unsupported (%dx%d)\n", w, h));

    if (pPict->filter != PictFilterNearest &&
	pPict->filter != PictFilterBilinear)
	RADEON_FALLBACK(("Unsupported filter 0x%x\n", pPict->filter));

    return TRUE;
}

#endif /* ONLY_ONCE */

static Bool FUNC_NAME(R200TextureSetup)(PicturePtr pPict, PixmapPtr pPix,
					int unit)
{
    RINFO_FROM_SCREEN(pPix->drawable.pScreen);
    CARD32 txfilter, txformat, txoffset, txpitch;
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    int i;
    ACCEL_PREAMBLE();

    txpitch = exaGetPixmapPitch(pPix);
    txoffset = exaGetPixmapOffset(pPix) + info->fbLocation;

    if ((txoffset & 0x1f) != 0)
	RADEON_FALLBACK(("Bad texture offset 0x%x\n", (int)txoffset));
    if ((txpitch & 0x1f) != 0)
	RADEON_FALLBACK(("Bad texture pitch 0x%x\n", (int)txpitch));

    for (i = 0; i < sizeof(R200TexFormats) / sizeof(R200TexFormats[0]); i++)
    {
	if (R200TexFormats[i].fmt == pPict->format)
	    break;
    }
    txformat = R200TexFormats[i].card_fmt;
    if (RADEONPixmapIsColortiled(pPix))
	txoffset |= R200_TXO_MACRO_TILE;

    if (pPict->repeat) {
	if ((h != 1) &&
	    (((w * pPix->drawable.bitsPerPixel / 8 + 31) & ~31) != txpitch))
	    RADEON_FALLBACK(("Width %d and pitch %u not compatible for repeat\n",
			     w, (unsigned)txpitch));

	txformat |= RADEONLog2(w) << R200_TXFORMAT_WIDTH_SHIFT;
	txformat |= RADEONLog2(h) << R200_TXFORMAT_HEIGHT_SHIFT;
    } else
	txformat |= R200_TXFORMAT_NON_POWER2;
    txformat |= unit << R200_TXFORMAT_ST_ROUTE_SHIFT;

    info->texW[unit] = w;
    info->texH[unit] = h;

    switch (pPict->filter) {
    case PictFilterNearest:
	txfilter = (R200_MAG_FILTER_NEAREST |
		    R200_MIN_FILTER_NEAREST);
	break;
    case PictFilterBilinear:
	txfilter = (R200_MAG_FILTER_LINEAR |
		    R200_MIN_FILTER_LINEAR);
	break;
    default:
	RADEON_FALLBACK(("Bad filter 0x%x\n", pPict->filter));
    }

    BEGIN_ACCEL(6);
    if (unit == 0) {
	OUT_ACCEL_REG(R200_PP_TXFILTER_0, txfilter);
	OUT_ACCEL_REG(R200_PP_TXFORMAT_0, txformat);
	OUT_ACCEL_REG(R200_PP_TXFORMAT_X_0, 0);
	OUT_ACCEL_REG(R200_PP_TXSIZE_0, (pPix->drawable.width - 1) |
		      ((pPix->drawable.height - 1) << RADEON_TEX_VSIZE_SHIFT));
	OUT_ACCEL_REG(R200_PP_TXPITCH_0, txpitch - 32);
	OUT_ACCEL_REG(R200_PP_TXOFFSET_0, txoffset);
    } else {
	OUT_ACCEL_REG(R200_PP_TXFILTER_1, txfilter);
	OUT_ACCEL_REG(R200_PP_TXFORMAT_1, txformat);
	OUT_ACCEL_REG(R200_PP_TXFORMAT_X_1, 0);
	OUT_ACCEL_REG(R200_PP_TXSIZE_1, (pPix->drawable.width - 1) |
		      ((pPix->drawable.height - 1) << RADEON_TEX_VSIZE_SHIFT));
	OUT_ACCEL_REG(R200_PP_TXPITCH_1, txpitch - 32);
	OUT_ACCEL_REG(R200_PP_TXOFFSET_1, txoffset);
    }
    FINISH_ACCEL();

    if (pPict->transform != 0) {
	is_transform[unit] = TRUE;
	transform[unit] = pPict->transform;
    } else {
	is_transform[unit] = FALSE;
    }

    return TRUE;
}

#ifdef ONLY_ONCE
static Bool R200CheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
			       PicturePtr pDstPicture)
{
    PixmapPtr pSrcPixmap, pDstPixmap;
    CARD32 tmp1;

    TRACE;

    if (!pSrcPicture->pDrawable)
	return FALSE;

    pSrcPixmap = RADEONGetDrawablePixmap(pSrcPicture->pDrawable);

    if (pSrcPixmap->drawable.width >= 2048 ||
	pSrcPixmap->drawable.height >= 2048) {
	RADEON_FALLBACK(("Source w/h too large (%d,%d).\n",
			 pSrcPixmap->drawable.width,
			 pSrcPixmap->drawable.height));
    }

    pDstPixmap = RADEONGetDrawablePixmap(pDstPicture->pDrawable);

    if (pDstPixmap->drawable.width >= 2048 ||
	pDstPixmap->drawable.height >= 2048) {
	RADEON_FALLBACK(("Dest w/h too large (%d,%d).\n",
			 pDstPixmap->drawable.width,
			 pDstPixmap->drawable.height));
    }

    if (pMaskPicture) {
	PixmapPtr pMaskPixmap = RADEONGetDrawablePixmap(pMaskPicture->pDrawable);

	if (pMaskPixmap->drawable.width >= 2048 ||
	    pMaskPixmap->drawable.height >= 2048) {
	    RADEON_FALLBACK(("Mask w/h too large (%d,%d).\n",
			     pMaskPixmap->drawable.width,
			     pMaskPixmap->drawable.height));
	}

	if (pMaskPicture->componentAlpha) {
	    /* Check if it's component alpha that relies on a source alpha and
	     * on the source value.  We can only get one of those into the
	     * single source value that we get to blend with.
	     */
	    if (RadeonBlendOp[op].src_alpha &&
		(RadeonBlendOp[op].blend_cntl & RADEON_SRC_BLEND_MASK) !=
		RADEON_SRC_BLEND_GL_ZERO) {
		RADEON_FALLBACK(("Component alpha not supported with source "
				 "alpha and source value blending.\n"));
	    }
	}

	if (!R200CheckCompositeTexture(pMaskPicture, 1))
	    return FALSE;
    }

    if (!R200CheckCompositeTexture(pSrcPicture, 0))
	return FALSE;

    if (!RADEONGetDestFormat(pDstPicture, &tmp1))
	return FALSE;

    return TRUE;
}
#endif /* ONLY_ONCE */

static Bool FUNC_NAME(R200PrepareComposite)(int op, PicturePtr pSrcPicture,
				PicturePtr pMaskPicture, PicturePtr pDstPicture,
				PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst)
{
    RINFO_FROM_SCREEN(pDst->drawable.pScreen);
    CARD32 dst_format, dst_offset, dst_pitch;
    CARD32 pp_cntl, blendcntl, cblend, ablend, colorpitch;
    int pixel_shift;
    ACCEL_PREAMBLE();

    TRACE;

    if (!info->XInited3D)
	RADEONInit3DEngine(pScrn);

    if (!RADEONGetDestFormat(pDstPicture, &dst_format))
	return FALSE;

    pixel_shift = pDst->drawable.bitsPerPixel >> 4;

    dst_offset = exaGetPixmapOffset(pDst) + info->fbLocation;
    dst_pitch = exaGetPixmapPitch(pDst);
    colorpitch = dst_pitch >> pixel_shift;
    if (RADEONPixmapIsColortiled(pDst))
	colorpitch |= RADEON_COLOR_TILE_ENABLE;

    if ((dst_offset & 0x0f) != 0)
	RADEON_FALLBACK(("Bad destination offset 0x%x\n", (int)dst_offset));
    if (((dst_pitch >> pixel_shift) & 0x7) != 0)
	RADEON_FALLBACK(("Bad destination pitch 0x%x\n", (int)dst_pitch));

    if (!FUNC_NAME(R200TextureSetup)(pSrcPicture, pSrc, 0))
	return FALSE;
    pp_cntl = RADEON_TEX_0_ENABLE | RADEON_TEX_BLEND_0_ENABLE;

    if (pMask != NULL) {
	if (!FUNC_NAME(R200TextureSetup)(pMaskPicture, pMask, 1))
	    return FALSE;
	pp_cntl |= RADEON_TEX_1_ENABLE;
    } else {
	is_transform[1] = FALSE;
    }

    RADEON_SWITCH_TO_3D();

    BEGIN_ACCEL(11);

    OUT_ACCEL_REG(RADEON_PP_CNTL, pp_cntl);
    OUT_ACCEL_REG(RADEON_RB3D_CNTL, dst_format | RADEON_ALPHA_BLEND_ENABLE);
    OUT_ACCEL_REG(RADEON_RB3D_COLOROFFSET, dst_offset);

    OUT_ACCEL_REG(R200_SE_VTX_FMT_0, R200_VTX_XY);
    OUT_ACCEL_REG(R200_SE_VTX_FMT_1,
		 (2 << R200_VTX_TEX0_COMP_CNT_SHIFT) |
		 (2 << R200_VTX_TEX1_COMP_CNT_SHIFT));

    OUT_ACCEL_REG(RADEON_RB3D_COLORPITCH, colorpitch);

    /* IN operator: Multiply src by mask components or mask alpha.
     * BLEND_CTL_ADD is A * B + C.
     * If a picture is a8, we have to explicitly zero its color values.
     * If the destination is a8, we have to route the alpha to red, I think.
     * If we're doing component alpha where the source for blending is going to
     * be the source alpha (and there's no source value used), we have to zero
     * the source's color values.
     */
    cblend = R200_TXC_OP_MADD | R200_TXC_ARG_C_ZERO;
    ablend = R200_TXA_OP_MADD | R200_TXA_ARG_C_ZERO;

    if (pDstPicture->format == PICT_a8 ||
	(pMask && pMaskPicture->componentAlpha && RadeonBlendOp[op].src_alpha))
    {
	cblend |= R200_TXC_ARG_A_R0_ALPHA;
    } else if (pSrcPicture->format == PICT_a8)
	cblend |= R200_TXC_ARG_A_ZERO;
    else
	cblend |= R200_TXC_ARG_A_R0_COLOR;
    ablend |= R200_TXA_ARG_A_R0_ALPHA;

    if (pMask) {
	if (pMaskPicture->componentAlpha &&
	    pDstPicture->format != PICT_a8)
	    cblend |= R200_TXC_ARG_B_R1_COLOR;
	else
	    cblend |= R200_TXC_ARG_B_R1_ALPHA;
	ablend |= R200_TXA_ARG_B_R1_ALPHA;
    } else {
	cblend |= R200_TXC_ARG_B_ZERO | R200_TXC_COMP_ARG_B;
	ablend |= R200_TXA_ARG_B_ZERO | R200_TXA_COMP_ARG_B;
    }

    OUT_ACCEL_REG(R200_PP_TXCBLEND_0, cblend);
    OUT_ACCEL_REG(R200_PP_TXCBLEND2_0,
	R200_TXC_CLAMP_0_1 | R200_TXC_OUTPUT_REG_R0);
    OUT_ACCEL_REG(R200_PP_TXABLEND_0, ablend);
    OUT_ACCEL_REG(R200_PP_TXABLEND2_0,
	R200_TXA_CLAMP_0_1 | R200_TXA_OUTPUT_REG_R0);

    /* Op operator. */
    blendcntl = RADEONGetBlendCntl(op, pMaskPicture, pDstPicture->format);
    OUT_ACCEL_REG(RADEON_RB3D_BLENDCNTL, blendcntl);
    FINISH_ACCEL();

    return TRUE;
}

#ifdef ONLY_ONCE

static Bool R300CheckCompositeTexture(PicturePtr pPict, int unit)
{
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    int i;

    if ((w > 0x7ff) || (h > 0x7ff))
	RADEON_FALLBACK(("Picture w/h too large (%dx%d)\n", w, h));

    for (i = 0; i < sizeof(R300TexFormats) / sizeof(R300TexFormats[0]); i++)
    {
	if (R300TexFormats[i].fmt == pPict->format)
	    break;
    }
    if (i == sizeof(R300TexFormats) / sizeof(R300TexFormats[0]))
	RADEON_FALLBACK(("Unsupported picture format 0x%x\n",
			 (int)pPict->format));

    if (pPict->repeat && ((w & (w - 1)) != 0 || (h & (h - 1)) != 0))
	RADEON_FALLBACK(("NPOT repeat unsupported (%dx%d)\n", w, h));

    if (pPict->filter != PictFilterNearest &&
	pPict->filter != PictFilterBilinear)
	RADEON_FALLBACK(("Unsupported filter 0x%x\n", pPict->filter));

    return TRUE;
}

#endif /* ONLY_ONCE */

static Bool FUNC_NAME(R300TextureSetup)(PicturePtr pPict, PixmapPtr pPix,
					int unit)
{
    RINFO_FROM_SCREEN(pPix->drawable.pScreen);
    CARD32 txfilter, txformat0, txformat1, txoffset, txpitch;
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    int i, pixel_shift;
    ACCEL_PREAMBLE();

    TRACE;

    txpitch = exaGetPixmapPitch(pPix);
    txoffset = exaGetPixmapOffset(pPix) + info->fbLocation;

    if ((txoffset & 0x1f) != 0)
	RADEON_FALLBACK(("Bad texture offset 0x%x\n", (int)txoffset));
    if ((txpitch & 0x1f) != 0)
	RADEON_FALLBACK(("Bad texture pitch 0x%x\n", (int)txpitch));

    pixel_shift = pPix->drawable.bitsPerPixel >> 4;
    txpitch >>= pixel_shift;
    txpitch -= 1;

    if (RADEONPixmapIsColortiled(pPix))
	txoffset |= R300_MACRO_TILE;

    for (i = 0; i < sizeof(R300TexFormats) / sizeof(R300TexFormats[0]); i++)
    {
	if (R300TexFormats[i].fmt == pPict->format)
	    break;
    }

    txformat1 = R300TexFormats[i].card_fmt;

    txformat0 = (((w - 1) << R300_TXWIDTH_SHIFT) |
		 ((h - 1) << R300_TXHEIGHT_SHIFT));

    if (pPict->repeat) {
	ErrorF("repeat\n");
	if ((h != 1) &&
	    (((w * pPix->drawable.bitsPerPixel / 8 + 31) & ~31) != txpitch))
	    RADEON_FALLBACK(("Width %d and pitch %u not compatible for repeat\n",
			     w, (unsigned)txpitch));
    } else
	txformat0 |= R300_TXPITCH_EN;


    info->texW[unit] = w;
    info->texH[unit] = h;

    txfilter = (R300_TX_CLAMP_S(R300_TX_CLAMP_CLAMP_LAST) |
		R300_TX_CLAMP_T(R300_TX_CLAMP_CLAMP_LAST));

    txfilter |= (unit << R300_TX_ID_SHIFT);

    switch (pPict->filter) {
    case PictFilterNearest:
	txfilter |= (R300_TX_MAG_FILTER_NEAREST | R300_TX_MIN_FILTER_NEAREST);
	break;
    case PictFilterBilinear:
	txfilter |= (R300_TX_MAG_FILTER_LINEAR | R300_TX_MIN_FILTER_LINEAR);
	break;
    default:
	RADEON_FALLBACK(("Bad filter 0x%x\n", pPict->filter));
    }

    BEGIN_ACCEL(6);
    OUT_ACCEL_REG(R300_TX_FILTER0_0 + (unit * 4), txfilter);
    OUT_ACCEL_REG(R300_TX_FILTER1_0 + (unit * 4), 0);
    OUT_ACCEL_REG(R300_TX_FORMAT0_0 + (unit * 4), txformat0);
    OUT_ACCEL_REG(R300_TX_FORMAT1_0 + (unit * 4), txformat1);
    OUT_ACCEL_REG(R300_TX_FORMAT2_0 + (unit * 4), txpitch);
    OUT_ACCEL_REG(R300_TX_OFFSET_0 + (unit * 4), txoffset);
    FINISH_ACCEL();

    if (pPict->transform != 0) {
	is_transform[unit] = TRUE;
	transform[unit] = pPict->transform;
    } else {
	is_transform[unit] = FALSE;
    }

    return TRUE;
}

#ifdef ONLY_ONCE

static Bool R300CheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
			       PicturePtr pDstPicture)
{
    CARD32 tmp1;
    ScreenPtr pScreen = pDstPicture->pDrawable->pScreen;
    PixmapPtr pSrcPixmap, pDstPixmap;
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int i;

    TRACE;

    /* Check for unsupported compositing operations. */
    if (op >= sizeof(RadeonBlendOp) / sizeof(RadeonBlendOp[0]))
	RADEON_FALLBACK(("Unsupported Composite op 0x%x\n", op));

#if 0
    /* Throw out cases that aren't going to be our rotation first */
    if (pMaskPicture != NULL || op != PictOpSrc || pSrcPicture->pDrawable == NULL)
	RADEON_FALLBACK(("Junk driver\n"));

    if (pSrcPicture->pDrawable->type != DRAWABLE_WINDOW ||
	pDstPicture->pDrawable->type != DRAWABLE_PIXMAP) {
	RADEON_FALLBACK(("bad drawable\n"));
    }

    pSrcPixmap = (*pScreen->GetWindowPixmap) ((WindowPtr) pSrcPicture->pDrawable);
    pDstPixmap = (PixmapPtr)pDstPicture->pDrawable;

    /* Check if the dest is one of our shadow pixmaps */
    for (i = 0; i < xf86_config->num_crtc; i++) {
	xf86CrtcPtr crtc = xf86_config->crtc[i];

	if (crtc->rotatedPixmap == pDstPixmap)
	    break;
    }
    if (i == xf86_config->num_crtc)
	RADEON_FALLBACK(("no rotated pixmap\n"));

    if (pSrcPixmap != pScreen->GetScreenPixmap(pScreen))
	RADEON_FALLBACK(("src not screen\n"));
#endif

    pSrcPixmap = RADEONGetDrawablePixmap(pSrcPicture->pDrawable);

    /* XXX: R(V)5xx may have higher limits
     */
    if (pSrcPixmap->drawable.width >= 2048 ||
	pSrcPixmap->drawable.height >= 2048) {
	RADEON_FALLBACK(("Source w/h too large (%d,%d).\n",
			 pSrcPixmap->drawable.width,
			 pSrcPixmap->drawable.height));
    }

    pDstPixmap = RADEONGetDrawablePixmap(pDstPicture->pDrawable);

    if (pDstPixmap->drawable.width >= 2560 ||
	pDstPixmap->drawable.height >= 2560) {
	RADEON_FALLBACK(("Dest w/h too large (%d,%d).\n",
			 pDstPixmap->drawable.width,
			 pDstPixmap->drawable.height));
    }

    if (pMaskPicture) {
	PixmapPtr pMaskPixmap = RADEONGetDrawablePixmap(pMaskPicture->pDrawable);

	if (pMaskPixmap->drawable.width >= 2048 ||
	    pMaskPixmap->drawable.height >= 2048) {
	    RADEON_FALLBACK(("Mask w/h too large (%d,%d).\n",
			     pMaskPixmap->drawable.width,
			     pMaskPixmap->drawable.height));
	}

	if (pMaskPicture->componentAlpha) {
	    /* Check if it's component alpha that relies on a source alpha and
	     * on the source value.  We can only get one of those into the
	     * single source value that we get to blend with.
	     */
	    if (RadeonBlendOp[op].src_alpha &&
		(RadeonBlendOp[op].blend_cntl & RADEON_SRC_BLEND_MASK) !=
		RADEON_SRC_BLEND_GL_ZERO) {
		RADEON_FALLBACK(("Component alpha not supported with source "
				 "alpha and source value blending.\n"));
	    }
	}

	if (!R300CheckCompositeTexture(pMaskPicture, 1))
	    return FALSE;
    }

    if (!R300CheckCompositeTexture(pSrcPicture, 0))
	return FALSE;

    if (!R300GetDestFormat(pDstPicture, &tmp1))
	return FALSE;

    return TRUE;

}
#endif /* ONLY_ONCE */

static Bool FUNC_NAME(R300PrepareComposite)(int op, PicturePtr pSrcPicture,
				PicturePtr pMaskPicture, PicturePtr pDstPicture,
				PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst)
{
    RINFO_FROM_SCREEN(pDst->drawable.pScreen);
    CARD32 dst_format, dst_offset, dst_pitch;
    CARD32 txenable, colorpitch;
    CARD32 blendcntl;
    int pixel_shift;
    int has_tcl = ((info->ChipFamily != CHIP_FAMILY_RS690) &&
		   (info->ChipFamily != CHIP_FAMILY_RS740) &&
		   (info->ChipFamily != CHIP_FAMILY_RS400));
    ACCEL_PREAMBLE();

    TRACE;

    if (!info->XInited3D)
	RADEONInit3DEngine(pScrn);

    if (!R300GetDestFormat(pDstPicture, &dst_format))
	return FALSE;

    pixel_shift = pDst->drawable.bitsPerPixel >> 4;

    dst_offset = exaGetPixmapOffset(pDst) + info->fbLocation;
    dst_pitch = exaGetPixmapPitch(pDst);
    colorpitch = dst_pitch >> pixel_shift;

    if (RADEONPixmapIsColortiled(pDst))
	colorpitch |= R300_COLORTILE;

    colorpitch |= dst_format;

    if ((dst_offset & 0x0f) != 0)
	RADEON_FALLBACK(("Bad destination offset 0x%x\n", (int)dst_offset));
    if (((dst_pitch >> pixel_shift) & 0x7) != 0)
	RADEON_FALLBACK(("Bad destination pitch 0x%x\n", (int)dst_pitch));

    if (!FUNC_NAME(R300TextureSetup)(pSrcPicture, pSrc, 0))
	return FALSE;
    txenable = R300_TEX_0_ENABLE;

    if (pMask != NULL) {
	if (!FUNC_NAME(R300TextureSetup)(pMaskPicture, pMask, 1))
	    return FALSE;
	txenable |= R300_TEX_1_ENABLE;
    } else {
	is_transform[1] = FALSE;
    }

    RADEON_SWITCH_TO_3D();

    /* setup the VAP */
    if (has_tcl) {
	BEGIN_ACCEL(9);
	OUT_ACCEL_REG(R300_VAP_CNTL_STATUS, 0);
	OUT_ACCEL_REG(R300_VAP_PVS_STATE_FLUSH_REG, 0);
	OUT_ACCEL_REG(R300_VAP_CNTL, ((6 << R300_PVS_NUM_SLOTS_SHIFT) |
				      (5 << R300_PVS_NUM_CNTLRS_SHIFT) |
				      (4 << R300_PVS_NUM_FPUS_SHIFT) |
				      (12 << R300_VF_MAX_VTX_NUM_SHIFT)));
    } else {
	BEGIN_ACCEL(8);
	OUT_ACCEL_REG(R300_VAP_CNTL_STATUS, R300_PVS_BYPASS);
	OUT_ACCEL_REG(R300_VAP_CNTL, ((10 << R300_PVS_NUM_SLOTS_SHIFT) |
				      (5 << R300_PVS_NUM_CNTLRS_SHIFT) |
				      (4 << R300_PVS_NUM_FPUS_SHIFT) |
				      (5 << R300_VF_MAX_VTX_NUM_SHIFT)));
    }

    OUT_ACCEL_REG(R300_VAP_VTE_CNTL, R300_VTX_XY_FMT | R300_VTX_Z_FMT);
    OUT_ACCEL_REG(R300_VAP_PSC_SGN_NORM_CNTL, 0);

    if (has_tcl) {
	OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_0,
		      ((R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_0_SHIFT) |
		       (0 << R300_SKIP_DWORDS_0_SHIFT) |
		       (0 << R300_DST_VEC_LOC_0_SHIFT) |
		       R300_SIGNED_0 |
		       (R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_1_SHIFT) |
		       (0 << R300_SKIP_DWORDS_1_SHIFT) |
		       (1 << R300_DST_VEC_LOC_1_SHIFT) |
		       R300_SIGNED_1));
	OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_1,
		      ((R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_2_SHIFT) |
		       (0 << R300_SKIP_DWORDS_2_SHIFT) |
		       (2 << R300_DST_VEC_LOC_2_SHIFT) |
		       R300_LAST_VEC_2 |
		       R300_SIGNED_2));
	OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_EXT_0,
		      ((R300_SWIZZLE_SELECT_X << R300_SWIZZLE_SELECT_X_0_SHIFT) |
		       (R300_SWIZZLE_SELECT_Y << R300_SWIZZLE_SELECT_Y_0_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ZERO << R300_SWIZZLE_SELECT_Z_0_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ONE << R300_SWIZZLE_SELECT_W_0_SHIFT) |
		       ((R300_WRITE_ENA_X | R300_WRITE_ENA_Y)
			<< R300_WRITE_ENA_0_SHIFT) |
		       (R300_SWIZZLE_SELECT_X << R300_SWIZZLE_SELECT_X_1_SHIFT) |
		       (R300_SWIZZLE_SELECT_Y << R300_SWIZZLE_SELECT_Y_1_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ZERO << R300_SWIZZLE_SELECT_Z_1_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ONE << R300_SWIZZLE_SELECT_W_1_SHIFT) |
		       ((R300_WRITE_ENA_X | R300_WRITE_ENA_Y)
			<< R300_WRITE_ENA_1_SHIFT)));
	OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_EXT_1,
		      ((R300_SWIZZLE_SELECT_X << R300_SWIZZLE_SELECT_X_2_SHIFT) |
		       (R300_SWIZZLE_SELECT_Y << R300_SWIZZLE_SELECT_Y_2_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ZERO << R300_SWIZZLE_SELECT_Z_2_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ONE << R300_SWIZZLE_SELECT_W_2_SHIFT) |
		       ((R300_WRITE_ENA_X | R300_WRITE_ENA_Y)
			<< R300_WRITE_ENA_2_SHIFT)));
    } else {
	OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_0,
		      ((R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_0_SHIFT) |
		       (0 << R300_SKIP_DWORDS_0_SHIFT) |
		       (0 << R300_DST_VEC_LOC_0_SHIFT) |
		       R300_SIGNED_0 |
		       (R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_1_SHIFT) |
		       (0 << R300_SKIP_DWORDS_1_SHIFT) |
		       (6 << R300_DST_VEC_LOC_1_SHIFT) |
		       R300_SIGNED_1));
	OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_1,
		      ((R300_DATA_TYPE_FLOAT_2 << R300_DATA_TYPE_2_SHIFT) |
		       (0 << R300_SKIP_DWORDS_2_SHIFT) |
		       (7 << R300_DST_VEC_LOC_2_SHIFT) |
		       R300_LAST_VEC_2 |
		       R300_SIGNED_2));
	OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_EXT_0,
		      ((R300_SWIZZLE_SELECT_X << R300_SWIZZLE_SELECT_X_0_SHIFT) |
		       (R300_SWIZZLE_SELECT_Y << R300_SWIZZLE_SELECT_Y_0_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ZERO << R300_SWIZZLE_SELECT_Z_0_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ONE << R300_SWIZZLE_SELECT_W_0_SHIFT) |
		       ((R300_WRITE_ENA_X | R300_WRITE_ENA_Y)
			<< R300_WRITE_ENA_0_SHIFT) |
		       (R300_SWIZZLE_SELECT_X << R300_SWIZZLE_SELECT_X_1_SHIFT) |
		       (R300_SWIZZLE_SELECT_Y << R300_SWIZZLE_SELECT_Y_1_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ZERO << R300_SWIZZLE_SELECT_Z_1_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ONE << R300_SWIZZLE_SELECT_W_1_SHIFT) |
		       ((R300_WRITE_ENA_X | R300_WRITE_ENA_Y)
			<< R300_WRITE_ENA_1_SHIFT)));
	OUT_ACCEL_REG(R300_VAP_PROG_STREAM_CNTL_EXT_1,
		      ((R300_SWIZZLE_SELECT_X << R300_SWIZZLE_SELECT_X_2_SHIFT) |
		       (R300_SWIZZLE_SELECT_Y << R300_SWIZZLE_SELECT_Y_2_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ZERO << R300_SWIZZLE_SELECT_Z_2_SHIFT) |
		       (R300_SWIZZLE_SELECT_FP_ONE << R300_SWIZZLE_SELECT_W_2_SHIFT) |
		       ((R300_WRITE_ENA_X | R300_WRITE_ENA_Y)
			<< R300_WRITE_ENA_2_SHIFT)));
    }
    FINISH_ACCEL();

    /* setup the vertex shader */
    if (has_tcl) {
	if (pMask) {
	    BEGIN_ACCEL(22);
	    /* flush the PVS before updating??? */
	    OUT_ACCEL_REG(R300_VAP_PVS_STATE_FLUSH_REG, 0);

	    OUT_ACCEL_REG(R300_VAP_PVS_CODE_CNTL_0,
			  ((0 << R300_PVS_FIRST_INST_SHIFT) |
			   (2 << R300_PVS_XYZW_VALID_INST_SHIFT) |
			   (2 << R300_PVS_LAST_INST_SHIFT)));
	    OUT_ACCEL_REG(R300_VAP_PVS_CODE_CNTL_1,
			  (2 << R300_PVS_LAST_VTX_SRC_INST_SHIFT));
	} else {
	    BEGIN_ACCEL(18);
	    /* flush the PVS before updating??? */
	    OUT_ACCEL_REG(R300_VAP_PVS_STATE_FLUSH_REG, 0);

	    OUT_ACCEL_REG(R300_VAP_PVS_CODE_CNTL_0,
			  ((0 << R300_PVS_FIRST_INST_SHIFT) |
			   (1 << R300_PVS_XYZW_VALID_INST_SHIFT) |
			   (1 << R300_PVS_LAST_INST_SHIFT)));
	    OUT_ACCEL_REG(R300_VAP_PVS_CODE_CNTL_1,
			  (1 << R300_PVS_LAST_VTX_SRC_INST_SHIFT));
	}
	OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_INDX_REG, 0);
	/* PVS inst 0 */
	OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
		      (R300_PVS_DST_OPCODE(R300_VE_ADD) |
		       R300_PVS_DST_REG_TYPE(R300_PVS_DST_REG_OUT) |
		       R300_PVS_DST_OFFSET(0) |
		       R300_PVS_DST_WE_X | R300_PVS_DST_WE_Y |
		       R300_PVS_DST_WE_Z | R300_PVS_DST_WE_W));
	OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
		      (R300_PVS_SRC_REG_TYPE(R300_PVS_SRC_REG_INPUT) |
		       R300_PVS_SRC_OFFSET(0) |
		       R300_PVS_SRC_SWIZZLE_X(R300_PVS_SRC_SELECT_X) |
		       R300_PVS_SRC_SWIZZLE_Y(R300_PVS_SRC_SELECT_Y) |
		       R300_PVS_SRC_SWIZZLE_Z(R300_PVS_SRC_SELECT_Z) |
		       R300_PVS_SRC_SWIZZLE_W(R300_PVS_SRC_SELECT_W)));
	OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
		      (R300_PVS_SRC_REG_TYPE(R300_PVS_SRC_REG_INPUT) |
		       R300_PVS_SRC_OFFSET(0) |
		       R300_PVS_SRC_SWIZZLE_X(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_Y(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_Z(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_W(R300_PVS_SRC_SELECT_FORCE_0)));
	OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
		      (R300_PVS_SRC_REG_TYPE(R300_PVS_SRC_REG_INPUT) |
		       R300_PVS_SRC_OFFSET(0) |
		       R300_PVS_SRC_SWIZZLE_X(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_Y(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_Z(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_W(R300_PVS_SRC_SELECT_FORCE_0)));

	/* PVS inst 1 */
	OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
		      (R300_PVS_DST_OPCODE(R300_VE_ADD) |
		       R300_PVS_DST_REG_TYPE(R300_PVS_DST_REG_OUT) |
		       R300_PVS_DST_OFFSET(1) |
		       R300_PVS_DST_WE_X | R300_PVS_DST_WE_Y |
		       R300_PVS_DST_WE_Z | R300_PVS_DST_WE_W));
	OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
		      (R300_PVS_SRC_REG_TYPE(R300_PVS_SRC_REG_INPUT) |
		       R300_PVS_SRC_OFFSET(1) |
		       R300_PVS_SRC_SWIZZLE_X(R300_PVS_SRC_SELECT_X) |
		       R300_PVS_SRC_SWIZZLE_Y(R300_PVS_SRC_SELECT_Y) |
		       R300_PVS_SRC_SWIZZLE_Z(R300_PVS_SRC_SELECT_Z) |
		       R300_PVS_SRC_SWIZZLE_W(R300_PVS_SRC_SELECT_W)));
	OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
		      (R300_PVS_SRC_REG_TYPE(R300_PVS_SRC_REG_INPUT) |
		       R300_PVS_SRC_OFFSET(1) |
		       R300_PVS_SRC_SWIZZLE_X(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_Y(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_Z(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_W(R300_PVS_SRC_SELECT_FORCE_0)));
	OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
		      (R300_PVS_SRC_REG_TYPE(R300_PVS_SRC_REG_INPUT) |
		       R300_PVS_SRC_OFFSET(1) |
		       R300_PVS_SRC_SWIZZLE_X(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_Y(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_Z(R300_PVS_SRC_SELECT_FORCE_0) |
		       R300_PVS_SRC_SWIZZLE_W(R300_PVS_SRC_SELECT_FORCE_0)));

	if (pMask) {
	    /* PVS inst 2 */
	    OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
			  (R300_PVS_DST_OPCODE(R300_VE_ADD) |
			   R300_PVS_DST_REG_TYPE(R300_PVS_DST_REG_OUT) |
			   R300_PVS_DST_OFFSET(2) |
			   R300_PVS_DST_WE_X | R300_PVS_DST_WE_Y |
			   R300_PVS_DST_WE_Z | R300_PVS_DST_WE_W));
	    OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
			  (R300_PVS_SRC_REG_TYPE(R300_PVS_SRC_REG_INPUT) |
			   R300_PVS_SRC_OFFSET(2) |
			   R300_PVS_SRC_SWIZZLE_X(R300_PVS_SRC_SELECT_X) |
			   R300_PVS_SRC_SWIZZLE_Y(R300_PVS_SRC_SELECT_Y) |
			   R300_PVS_SRC_SWIZZLE_Z(R300_PVS_SRC_SELECT_Z) |
			   R300_PVS_SRC_SWIZZLE_W(R300_PVS_SRC_SELECT_W)));
	    OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
			  (R300_PVS_SRC_REG_TYPE(R300_PVS_SRC_REG_INPUT) |
			   R300_PVS_SRC_OFFSET(2) |
			   R300_PVS_SRC_SWIZZLE_X(R300_PVS_SRC_SELECT_FORCE_0) |
			   R300_PVS_SRC_SWIZZLE_Y(R300_PVS_SRC_SELECT_FORCE_0) |
			   R300_PVS_SRC_SWIZZLE_Z(R300_PVS_SRC_SELECT_FORCE_0) |
			   R300_PVS_SRC_SWIZZLE_W(R300_PVS_SRC_SELECT_FORCE_0)));
	    OUT_ACCEL_REG(R300_VAP_PVS_VECTOR_DATA_REG,
			  (R300_PVS_SRC_REG_TYPE(R300_PVS_SRC_REG_INPUT) |
			   R300_PVS_SRC_OFFSET(2) |
			   R300_PVS_SRC_SWIZZLE_X(R300_PVS_SRC_SELECT_FORCE_0) |
			   R300_PVS_SRC_SWIZZLE_Y(R300_PVS_SRC_SELECT_FORCE_0) |
			   R300_PVS_SRC_SWIZZLE_Z(R300_PVS_SRC_SELECT_FORCE_0) |
			   R300_PVS_SRC_SWIZZLE_W(R300_PVS_SRC_SELECT_FORCE_0)));
	}

	OUT_ACCEL_REG(R300_VAP_PVS_FLOW_CNTL_OPC, 0);

	OUT_ACCEL_REG(R300_VAP_GB_VERT_CLIP_ADJ, 0x3f800000);
	OUT_ACCEL_REG(R300_VAP_GB_VERT_DISC_ADJ, 0x3f800000);
	OUT_ACCEL_REG(R300_VAP_GB_HORZ_CLIP_ADJ, 0x3f800000);
	OUT_ACCEL_REG(R300_VAP_GB_HORZ_DISC_ADJ, 0x3f800000);
	OUT_ACCEL_REG(R300_VAP_CLIP_CNTL, R300_CLIP_DISABLE);
	FINISH_ACCEL();
    }

    BEGIN_ACCEL(4);
    OUT_ACCEL_REG(R300_VAP_OUT_VTX_FMT_0, R300_VTX_POS_PRESENT);
    OUT_ACCEL_REG(R300_VAP_OUT_VTX_FMT_1,
		  ((2 << R300_TEX_0_COMP_CNT_SHIFT) |
		   (2 << R300_TEX_1_COMP_CNT_SHIFT)));

    OUT_ACCEL_REG(R300_TX_INVALTAGS, 0x0);
    OUT_ACCEL_REG(R300_TX_ENABLE, txenable);
    FINISH_ACCEL();

    /* setup pixel shader */
    if (IS_R300_VARIANT ||
	(info->ChipFamily == CHIP_FAMILY_RS690) ||
	(info->ChipFamily == CHIP_FAMILY_RS740)) {
	CARD32 output_fmt;
	int src_color, src_alpha;
	int mask_color, mask_alpha;

	if (PICT_FORMAT_RGB(pSrcPicture->format) == 0)
	    src_color = R300_ALU_RGB_1_0;
	else
	    src_color = R300_ALU_RGB_SRC0_RGB;

	if (PICT_FORMAT_A(pSrcPicture->format) == 0)
	    src_alpha = R300_ALU_ALPHA_1_0;
	else
	    src_alpha = R300_ALU_ALPHA_SRC0_A;

	if (pMask && pMaskPicture->componentAlpha) {
	    if (RadeonBlendOp[op].src_alpha) {
		if (PICT_FORMAT_A(pSrcPicture->format) == 0) {
		    src_color = R300_ALU_RGB_1_0;
		    src_alpha = R300_ALU_ALPHA_1_0;
		} else {
		    src_color = R300_ALU_RGB_SRC0_AAA;
		    src_alpha = R300_ALU_ALPHA_SRC0_A;
		}

		mask_color = R300_ALU_RGB_SRC1_RGB;

		if (PICT_FORMAT_A(pMaskPicture->format) == 0)
		    mask_alpha = R300_ALU_ALPHA_1_0;
		else
		    mask_alpha = R300_ALU_ALPHA_SRC1_A;

	    } else {
		src_color = R300_ALU_RGB_SRC0_RGB;

		if (PICT_FORMAT_A(pSrcPicture->format) == 0)
		    src_alpha = R300_ALU_ALPHA_1_0;
		else
		    src_alpha = R300_ALU_ALPHA_SRC0_A;

		mask_color = R300_ALU_RGB_SRC1_RGB;

		if (PICT_FORMAT_A(pMaskPicture->format) == 0)
		    mask_alpha = R300_ALU_ALPHA_1_0;
		else
		    mask_alpha = R300_ALU_ALPHA_SRC1_A;

	    }
	} else if (pMask) {
	    if (PICT_FORMAT_A(pMaskPicture->format) == 0)
		mask_color = R300_ALU_RGB_1_0;
	    else
		mask_color = R300_ALU_RGB_SRC1_AAA;

	    if (PICT_FORMAT_A(pMaskPicture->format) == 0)
		mask_alpha = R300_ALU_ALPHA_1_0;
	    else
		mask_alpha = R300_ALU_ALPHA_SRC1_A;
	} else {
	    mask_color = R300_ALU_RGB_1_0;
	    mask_alpha = R300_ALU_ALPHA_1_0;
	}

	/* shader output swizzling */
	switch (pDstPicture->format) {
	case PICT_a8r8g8b8:
	case PICT_x8r8g8b8:
	default:
	    output_fmt = (R300_OUT_FMT_C4_8 |
			  R300_OUT_FMT_C0_SEL_BLUE |
			  R300_OUT_FMT_C1_SEL_GREEN |
			  R300_OUT_FMT_C2_SEL_RED |
			  R300_OUT_FMT_C3_SEL_ALPHA);
	    break;
	case PICT_a8b8g8r8:
	case PICT_x8b8g8r8:
	    output_fmt = (R300_OUT_FMT_C4_8 |
			  R300_OUT_FMT_C0_SEL_RED |
			  R300_OUT_FMT_C1_SEL_GREEN |
			  R300_OUT_FMT_C2_SEL_BLUE |
			  R300_OUT_FMT_C3_SEL_ALPHA);
	    break;
	case PICT_r5g6b5:
	case PICT_a1r5g5b5:
	case PICT_x1r5g5b5:
	    output_fmt = (R300_OUT_FMT_C_5_6_5 |
			  R300_OUT_FMT_C0_SEL_BLUE |
			  R300_OUT_FMT_C1_SEL_GREEN |
			  R300_OUT_FMT_C2_SEL_RED |
			  R300_OUT_FMT_C3_SEL_ALPHA);
	    break;
	case PICT_a8:
	    output_fmt = (R300_OUT_FMT_C4_8 |
			  R300_OUT_FMT_C0_SEL_ALPHA);
	    break;
	}


	/* setup the rasterizer */
      if (pMask) {
	  BEGIN_ACCEL(20);
	  /* 4 components: 2 for tex0, 2 for tex1 */
	  OUT_ACCEL_REG(R300_RS_COUNT,
			((4 << R300_RS_COUNT_IT_COUNT_SHIFT) |
			 R300_RS_COUNT_HIRES_EN));
	  /* rasterizer source table */
	  OUT_ACCEL_REG(R300_RS_IP_0,
			(R300_RS_TEX_PTR(0) |
			 R300_RS_SEL_S(R300_RS_SEL_C0) |
			 R300_RS_SEL_T(R300_RS_SEL_C1) |
			 R300_RS_SEL_R(R300_RS_SEL_K0) |
			 R300_RS_SEL_Q(R300_RS_SEL_K1)));
	  OUT_ACCEL_REG(R300_RS_IP_1,
			(R300_RS_TEX_PTR(2) |
			 R300_RS_SEL_S(R300_RS_SEL_C0) |
			 R300_RS_SEL_T(R300_RS_SEL_C1) |
			 R300_RS_SEL_R(R300_RS_SEL_K0) |
			 R300_RS_SEL_Q(R300_RS_SEL_K1)));

	  OUT_ACCEL_REG(R300_RS_INST_COUNT, R300_INST_COUNT_RS(1) | R300_TX_OFFSET_RS(6));
	  /* src tex */
	  OUT_ACCEL_REG(R300_RS_INST_0, (R300_INST_TEX_ID(0) |
					 R300_RS_INST_TEX_CN_WRITE |
					 R300_INST_TEX_ADDR(0)));
	  /* mask tex */
	  OUT_ACCEL_REG(R300_RS_INST_1, (R300_INST_TEX_ID(1) |
					 R300_RS_INST_TEX_CN_WRITE |
					 R300_INST_TEX_ADDR(1)));

	  OUT_ACCEL_REG(R300_US_CONFIG, (0 << R300_NLEVEL_SHIFT) | R300_FIRST_TEX);
	  OUT_ACCEL_REG(R300_US_PIXSIZE, 2); /* max num of temps used */
	  OUT_ACCEL_REG(R300_US_CODE_OFFSET, (R300_ALU_CODE_OFFSET(0) |
					      R300_ALU_CODE_SIZE(0) |
					      R300_TEX_CODE_OFFSET(0) |
					      R300_TEX_CODE_SIZE(1)));

      } else {
	    BEGIN_ACCEL(17);
	    /* 2 components: 2 for tex0 */
	    OUT_ACCEL_REG(R300_RS_COUNT,
			  ((2 << R300_RS_COUNT_IT_COUNT_SHIFT) |
			   R300_RS_COUNT_HIRES_EN));
	    OUT_ACCEL_REG(R300_RS_IP_0,
			  (R300_RS_TEX_PTR(0) |
			   R300_RS_SEL_S(R300_RS_SEL_C0) |
			   R300_RS_SEL_T(R300_RS_SEL_C1) |
			   R300_RS_SEL_R(R300_RS_SEL_K0) |
			   R300_RS_SEL_Q(R300_RS_SEL_K1)));
	    OUT_ACCEL_REG(R300_RS_INST_COUNT, R300_INST_COUNT_RS(0) | R300_TX_OFFSET_RS(6));
	    /* src tex */
	    OUT_ACCEL_REG(R300_RS_INST_0, (R300_INST_TEX_ID(0) |
					   R300_RS_INST_TEX_CN_WRITE |
					   R300_INST_TEX_ADDR(0)));

	    OUT_ACCEL_REG(R300_US_CONFIG, (0 << R300_NLEVEL_SHIFT) | R300_FIRST_TEX);
	    OUT_ACCEL_REG(R300_US_PIXSIZE, 1); /* max num of temps used */
	    OUT_ACCEL_REG(R300_US_CODE_OFFSET, (R300_ALU_CODE_OFFSET(0) |
						R300_ALU_CODE_SIZE(0) |
						R300_TEX_CODE_OFFSET(0) |
						R300_TEX_CODE_SIZE(0)));

      }

      OUT_ACCEL_REG(R300_US_CODE_ADDR_0,
		    (R300_ALU_START(0) |
		     R300_ALU_SIZE(0) |
		     R300_TEX_START(0) |
		     R300_TEX_SIZE(0)));
      OUT_ACCEL_REG(R300_US_CODE_ADDR_1,
		    (R300_ALU_START(0) |
		     R300_ALU_SIZE(0) |
		     R300_TEX_START(0) |
		     R300_TEX_SIZE(0)));
      OUT_ACCEL_REG(R300_US_CODE_ADDR_2,
		    (R300_ALU_START(0) |
		     R300_ALU_SIZE(0) |
		     R300_TEX_START(0) |
		     R300_TEX_SIZE(0)));

      if (pMask) {
	  OUT_ACCEL_REG(R300_US_CODE_ADDR_3,
			(R300_ALU_START(0) |
			 R300_ALU_SIZE(0) |
			 R300_TEX_START(0) |
			 R300_TEX_SIZE(1) |
			 R300_RGBA_OUT));
      } else {
	  OUT_ACCEL_REG(R300_US_CODE_ADDR_3,
			(R300_ALU_START(0) |
			 R300_ALU_SIZE(0) |
			 R300_TEX_START(0) |
			 R300_TEX_SIZE(0) |
			 R300_RGBA_OUT));
      }

      OUT_ACCEL_REG(R300_US_OUT_FMT_0, output_fmt);

      OUT_ACCEL_REG(R300_US_TEX_INST_0,
		    (R300_TEX_SRC_ADDR(0) |
		     R300_TEX_DST_ADDR(0) |
		     R300_TEX_ID(0) |
		     R300_TEX_INST(R300_TEX_INST_LD)));

      if (pMask) {
	  OUT_ACCEL_REG(R300_US_TEX_INST_1,
			(R300_TEX_SRC_ADDR(1) |
			 R300_TEX_DST_ADDR(1) |
			 R300_TEX_ID(1) |
			 R300_TEX_INST(R300_TEX_INST_LD)));
      }

      OUT_ACCEL_REG(R300_US_ALU_RGB_ADDR_0,
		    (R300_ALU_RGB_ADDR0(0) |
		     R300_ALU_RGB_ADDR1(1) |
		     R300_ALU_RGB_ADDR2(0) |
		     R300_ALU_RGB_ADDRD(0) |
		     R300_ALU_RGB_OMASK((R300_ALU_RGB_MASK_R |
					 R300_ALU_RGB_MASK_G |
					 R300_ALU_RGB_MASK_B)) |
		     R300_ALU_RGB_TARGET_A));
      OUT_ACCEL_REG(R300_US_ALU_RGB_INST_0,
		    (R300_ALU_RGB_SEL_A(src_color) |
		     R300_ALU_RGB_MOD_A(R300_ALU_RGB_MOD_NOP) |
		     R300_ALU_RGB_SEL_B(mask_color) |
		     R300_ALU_RGB_MOD_B(R300_ALU_RGB_MOD_NOP) |
		     R300_ALU_RGB_SEL_C(R300_ALU_RGB_0_0) |
		     R300_ALU_RGB_MOD_C(R300_ALU_RGB_MOD_NOP) |
		     R300_ALU_RGB_OP(R300_ALU_RGB_OP_MAD) |
		     R300_ALU_RGB_OMOD(R300_ALU_RGB_OMOD_NONE)));
      OUT_ACCEL_REG(R300_US_ALU_ALPHA_ADDR_0,
		    (R300_ALU_ALPHA_ADDR0(0) |
		     R300_ALU_ALPHA_ADDR1(1) |
		     R300_ALU_ALPHA_ADDR2(0) |
		     R300_ALU_ALPHA_ADDRD(0) |
		     R300_ALU_ALPHA_OMASK(R300_ALU_ALPHA_MASK_A) |
		     R300_ALU_ALPHA_TARGET_A |
		     R300_ALU_ALPHA_OMASK_W(R300_ALU_ALPHA_MASK_NONE)));
      OUT_ACCEL_REG(R300_US_ALU_ALPHA_INST_0,
		    (R300_ALU_ALPHA_SEL_A(src_alpha) |
		     R300_ALU_ALPHA_MOD_A(R300_ALU_ALPHA_MOD_NOP) |
		     R300_ALU_ALPHA_SEL_B(mask_alpha) |
		     R300_ALU_ALPHA_MOD_B(R300_ALU_ALPHA_MOD_NOP) |
		     R300_ALU_ALPHA_SEL_C(R300_ALU_ALPHA_0_0) |
		     R300_ALU_ALPHA_MOD_C(R300_ALU_ALPHA_MOD_NOP) |
		     R300_ALU_ALPHA_OP(R300_ALU_ALPHA_OP_MAD) |
		     R300_ALU_ALPHA_OMOD(R300_ALU_ALPHA_OMOD_NONE)));
      FINISH_ACCEL();
    } else {
      BEGIN_ACCEL(23);
      OUT_ACCEL_REG(R300_RS_COUNT,
		    ((2 << R300_RS_COUNT_IT_COUNT_SHIFT) |
		     R300_RS_COUNT_HIRES_EN));
      OUT_ACCEL_REG(R500_RS_IP_0, ((0 << R500_RS_IP_TEX_PTR_S_SHIFT) |
				   (1 << R500_RS_IP_TEX_PTR_T_SHIFT) |
				   (R500_RS_IP_PTR_K0 << R500_RS_IP_TEX_PTR_R_SHIFT) |
				   (R500_RS_IP_PTR_K1 << R500_RS_IP_TEX_PTR_Q_SHIFT)));

      OUT_ACCEL_REG(R300_RS_INST_COUNT, 0);
      OUT_ACCEL_REG(R500_RS_INST_0, R500_RS_INST_TEX_CN_WRITE);
      OUT_ACCEL_REG(R300_US_CONFIG, R500_ZERO_TIMES_ANYTHING_EQUALS_ZERO);
      OUT_ACCEL_REG(R300_US_PIXSIZE, 0);
      OUT_ACCEL_REG(R500_US_FC_CTRL, 0);
      OUT_ACCEL_REG(R500_US_CODE_ADDR, (R500_US_CODE_START_ADDR(0) |
					R500_US_CODE_END_ADDR(1)));
      OUT_ACCEL_REG(R500_US_CODE_RANGE, (R500_US_CODE_RANGE_ADDR(0) |
					 R500_US_CODE_RANGE_SIZE(1)));
      OUT_ACCEL_REG(R500_US_CODE_OFFSET, 0);
      OUT_ACCEL_REG(R500_GA_US_VECTOR_INDEX, 0);
      // 7807
      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_TEX |
					     R500_INST_TEX_SEM_WAIT |
					     R500_INST_RGB_WMASK_R |
					     R500_INST_RGB_WMASK_G |
					     R500_INST_RGB_WMASK_B |
					     R500_INST_ALPHA_WMASK));

      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_ID(0) |
					     R500_TEX_INST_LD |
					     R500_TEX_SEM_ACQUIRE |
					     R500_TEX_IGNORE_UNCOVERED));

      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_TEX_SRC_ADDR(0) |
					     R500_TEX_SRC_S_SWIZ_R |
					     R500_TEX_SRC_T_SWIZ_G |
					     R500_TEX_DST_ADDR(0) |
					     R500_TEX_DST_R_SWIZ_R |
					     R500_TEX_DST_G_SWIZ_G |
					     R500_TEX_DST_B_SWIZ_B |
					     R500_TEX_DST_A_SWIZ_A));
      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_DX_ADDR(0) |
					     R500_DX_S_SWIZ_R |
					     R500_DX_T_SWIZ_R |
					     R500_DX_R_SWIZ_R |
					     R500_DX_Q_SWIZ_R |
					     R500_DY_ADDR(0) |
					     R500_DY_S_SWIZ_R |
					     R500_DY_T_SWIZ_R |
					     R500_DY_R_SWIZ_R |
					     R500_DY_Q_SWIZ_R)); // TEX_ADDR_DXDY
      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000); // mbz
      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, 0x00000000); // mbz

      // 0x78105
      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_INST_TYPE_OUT |
					     R500_INST_TEX_SEM_WAIT |
					     R500_INST_LAST |
					     R500_INST_RGB_OMASK_R |
					     R500_INST_RGB_OMASK_G |
					     R500_INST_RGB_OMASK_B |
					     R500_INST_ALPHA_OMASK));

      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_RGB_ADDR0(0) |
					     R500_RGB_ADDR1(0) |
					     R500_RGB_ADDR1_CONST |
					     R500_RGB_ADDR2(0) |
					     R500_RGB_ADDR2_CONST |
					     R500_RGB_SRCP_OP_1_MINUS_2RGB0)); //0x10040000
      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_ADDR0(0) |
					     R500_ALPHA_ADDR1(0) |
					     R500_ALPHA_ADDR1_CONST |
					     R500_ALPHA_ADDR2(0) |
					     R500_ALPHA_ADDR2_CONST |
					     R500_ALPHA_SRCP_OP_1_MINUS_2A0)); //0x10040000

      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGB_SEL_A_SRC0 |
					     R500_ALU_RGB_R_SWIZ_A_R |
					     R500_ALU_RGB_G_SWIZ_A_G |
					     R500_ALU_RGB_B_SWIZ_A_B |
					     R500_ALU_RGB_SEL_B_SRC0 |
					     R500_ALU_RGB_R_SWIZ_B_1 |
					     R500_ALU_RGB_B_SWIZ_B_1 |
					     R500_ALU_RGB_G_SWIZ_B_1));//0x00db0220

      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALPHA_OP_MAD |
					     R500_ALPHA_SWIZ_A_A |
					     R500_ALPHA_SWIZ_B_1));//0x00c0c000)

      OUT_ACCEL_REG(R500_GA_US_VECTOR_DATA, (R500_ALU_RGBA_OP_MAD |
					     R500_ALU_RGBA_R_SWIZ_0 |
					     R500_ALU_RGBA_G_SWIZ_0 |
					     R500_ALU_RGBA_B_SWIZ_0 |
					     R500_ALU_RGBA_A_SWIZ_0));//0x20490000
      FINISH_ACCEL();
    }

    BEGIN_ACCEL(4);

    OUT_ACCEL_REG(R300_RB3D_COLOROFFSET0, dst_offset);
    OUT_ACCEL_REG(R300_RB3D_COLORPITCH0, colorpitch);

    blendcntl = RADEONGetBlendCntl(op, pMaskPicture, pDstPicture->format);
    OUT_ACCEL_REG(R300_RB3D_BLENDCNTL, blendcntl | R300_ALPHA_BLEND_ENABLE | R300_READ_ENABLE);
    OUT_ACCEL_REG(R300_RB3D_ABLENDCNTL, 0);

#if 0
    /* IN operator: Multiply src by mask components or mask alpha.
     * BLEND_CTL_ADD is A * B + C.
     * If a picture is a8, we have to explicitly zero its color values.
     * If the destination is a8, we have to route the alpha to red, I think.
     * If we're doing component alpha where the source for blending is going to
     * be the source alpha (and there's no source value used), we have to zero
     * the source's color values.
     */
    cblend = R200_TXC_OP_MADD | R200_TXC_ARG_C_ZERO;
    ablend = R200_TXA_OP_MADD | R200_TXA_ARG_C_ZERO;

    if (pDstPicture->format == PICT_a8 ||
	(pMask && pMaskPicture->componentAlpha && RadeonBlendOp[op].src_alpha))
    {
	cblend |= R200_TXC_ARG_A_R0_ALPHA;
    } else if (pSrcPicture->format == PICT_a8)
	cblend |= R200_TXC_ARG_A_ZERO;
    else
	cblend |= R200_TXC_ARG_A_R0_COLOR;
    ablend |= R200_TXA_ARG_A_R0_ALPHA;

    if (pMask) {
	if (pMaskPicture->componentAlpha &&
	    pDstPicture->format != PICT_a8)
	    cblend |= R200_TXC_ARG_B_R1_COLOR;
	else
	    cblend |= R200_TXC_ARG_B_R1_ALPHA;
	ablend |= R200_TXA_ARG_B_R1_ALPHA;
    } else {
	cblend |= R200_TXC_ARG_B_ZERO | R200_TXC_COMP_ARG_B;
	ablend |= R200_TXA_ARG_B_ZERO | R200_TXA_COMP_ARG_B;
    }

    OUT_ACCEL_REG(R200_PP_TXCBLEND_0, cblend);
    OUT_ACCEL_REG(R200_PP_TXCBLEND2_0,
	R200_TXC_CLAMP_0_1 | R200_TXC_OUTPUT_REG_R0);
    OUT_ACCEL_REG(R200_PP_TXABLEND_0, ablend);
    OUT_ACCEL_REG(R200_PP_TXABLEND2_0,
	R200_TXA_CLAMP_0_1 | R200_TXA_OUTPUT_REG_R0);

    /* Op operator. */
    blendcntl = RADEONGetBlendCntl(op, pMaskPicture, pDstPicture->format);
    OUT_ACCEL_REG(RADEON_RB3D_BLENDCNTL, blendcntl);
#endif

    FINISH_ACCEL();

    return TRUE;
}

#define VTX_COUNT 6

#ifdef ACCEL_CP

#define VTX_OUT(_dstX, _dstY, _srcX, _srcY, _maskX, _maskY)	\
do {								\
    OUT_RING_F(_dstX);						\
    OUT_RING_F(_dstY);						\
    OUT_RING_F(_srcX);						\
    OUT_RING_F(_srcY);						\
    OUT_RING_F(_maskX);						\
    OUT_RING_F(_maskY);						\
} while (0)

#else /* ACCEL_CP */

#define VTX_OUT(_dstX, _dstY, _srcX, _srcY, _maskX, _maskY)	\
do {								\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _dstX);		\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _dstY);		\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _srcX);		\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _srcY);		\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _maskX);		\
    OUT_ACCEL_REG_F(RADEON_SE_PORT_DATA0, _maskY);		\
} while (0)

#endif /* !ACCEL_CP */

#ifdef ONLY_ONCE
static inline void transformPoint(PictTransform *transform, xPointFixed *point)
{
    PictVector v;
    v.vector[0] = point->x;
    v.vector[1] = point->y;
    v.vector[2] = xFixed1;
    PictureTransformPoint(transform, &v);
    point->x = v.vector[0];
    point->y = v.vector[1];
}
#endif

static void FUNC_NAME(RadeonComposite)(PixmapPtr pDst,
				     int srcX, int srcY,
				     int maskX, int maskY,
				     int dstX, int dstY,
				     int w, int h)
{
    RINFO_FROM_SCREEN(pDst->drawable.pScreen);
    int vtx_count;
    xPointFixed srcTopLeft, srcTopRight, srcBottomLeft, srcBottomRight;
    xPointFixed maskTopLeft, maskTopRight, maskBottomLeft, maskBottomRight;
    ACCEL_PREAMBLE();

    ENTER_DRAW(0);

    /* ErrorF("RadeonComposite (%d,%d) (%d,%d) (%d,%d) (%d,%d)\n",
       srcX, srcY, maskX, maskY,dstX, dstY, w, h); */

    srcTopLeft.x     = IntToxFixed(srcX);
    srcTopLeft.y     = IntToxFixed(srcY);
    srcTopRight.x    = IntToxFixed(srcX + w);
    srcTopRight.y    = IntToxFixed(srcY);
    srcBottomLeft.x  = IntToxFixed(srcX);
    srcBottomLeft.y  = IntToxFixed(srcY + h);
    srcBottomRight.x = IntToxFixed(srcX + w);
    srcBottomRight.y = IntToxFixed(srcY + h);

    maskTopLeft.x     = IntToxFixed(maskX);
    maskTopLeft.y     = IntToxFixed(maskY);
    maskTopRight.x    = IntToxFixed(maskX + w);
    maskTopRight.y    = IntToxFixed(maskY);
    maskBottomLeft.x  = IntToxFixed(maskX);
    maskBottomLeft.y  = IntToxFixed(maskY + h);
    maskBottomRight.x = IntToxFixed(maskX + w);
    maskBottomRight.y = IntToxFixed(maskY + h);

    if (is_transform[0]) {
	transformPoint(transform[0], &srcTopLeft);
	transformPoint(transform[0], &srcTopRight);
	transformPoint(transform[0], &srcBottomLeft);
	transformPoint(transform[0], &srcBottomRight);
    }
    if (is_transform[1]) {
	transformPoint(transform[1], &maskTopLeft);
	transformPoint(transform[1], &maskTopRight);
	transformPoint(transform[1], &maskBottomLeft);
	transformPoint(transform[1], &maskBottomRight);
    }

    vtx_count = VTX_COUNT;

    if (IS_R300_VARIANT || IS_AVIVO_VARIANT) {
	BEGIN_ACCEL(1);
	OUT_ACCEL_REG(R300_VAP_VTX_SIZE, vtx_count);
	FINISH_ACCEL();
    }

#ifdef ACCEL_CP
    if (info->ChipFamily < CHIP_FAMILY_R200) {
	BEGIN_RING(4 * vtx_count + 3);
	OUT_RING(CP_PACKET3(RADEON_CP_PACKET3_3D_DRAW_IMMD,
			    4 * vtx_count + 1));
	OUT_RING(RADEON_CP_VC_FRMT_XY |
		 RADEON_CP_VC_FRMT_ST0 |
		 RADEON_CP_VC_FRMT_ST1);
	OUT_RING(RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_FAN |
		 RADEON_CP_VC_CNTL_PRIM_WALK_RING |
		 RADEON_CP_VC_CNTL_MAOS_ENABLE |
		 RADEON_CP_VC_CNTL_VTX_FMT_RADEON_MODE |
		 (4 << RADEON_CP_VC_CNTL_NUM_SHIFT));
    } else {
	if (IS_R300_VARIANT || IS_AVIVO_VARIANT)
	    BEGIN_RING(4 * vtx_count + 6);
	else
	    BEGIN_RING(4 * vtx_count + 2);

	OUT_RING(CP_PACKET3(R200_CP_PACKET3_3D_DRAW_IMMD_2,
			    4 * vtx_count));
	OUT_RING(RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_FAN |
		 RADEON_CP_VC_CNTL_PRIM_WALK_RING |
		 (4 << RADEON_CP_VC_CNTL_NUM_SHIFT));
    }

#else /* ACCEL_CP */
    if (IS_R300_VARIANT || IS_AVIVO_VARIANT)
	BEGIN_ACCEL(3 + vtx_count * 4);
    else
	BEGIN_ACCEL(1 + vtx_count * 4);

    if (info->ChipFamily < CHIP_FAMILY_R200) {
	OUT_ACCEL_REG(RADEON_SE_VF_CNTL, (RADEON_VF_PRIM_TYPE_TRIANGLE_FAN |
					  RADEON_VF_PRIM_WALK_DATA |
					  RADEON_VF_RADEON_MODE |
					  4 << RADEON_VF_NUM_VERTICES_SHIFT));
    } else {
	OUT_ACCEL_REG(RADEON_SE_VF_CNTL, (RADEON_VF_PRIM_TYPE_QUAD_LIST |
					  RADEON_VF_PRIM_WALK_DATA |
					  4 << RADEON_VF_NUM_VERTICES_SHIFT));
    }
#endif

    VTX_OUT((float)dstX,                                      (float)dstY,
	    xFixedToFloat(srcTopLeft.x) / info->texW[0],      xFixedToFloat(srcTopLeft.y) / info->texH[0],
	    xFixedToFloat(maskTopLeft.x) / info->texW[1],     xFixedToFloat(maskTopLeft.y) / info->texH[1]);
    VTX_OUT((float)dstX,                                      (float)(dstY + h),
	    xFixedToFloat(srcBottomLeft.x) / info->texW[0],   xFixedToFloat(srcBottomLeft.y) / info->texH[0],
	    xFixedToFloat(maskBottomLeft.x) / info->texW[1],  xFixedToFloat(maskBottomLeft.y) / info->texH[1]);
    VTX_OUT((float)(dstX + w),                                (float)(dstY + h),
	    xFixedToFloat(srcBottomRight.x) / info->texW[0],  xFixedToFloat(srcBottomRight.y) / info->texH[0],
	    xFixedToFloat(maskBottomRight.x) / info->texW[1], xFixedToFloat(maskBottomRight.y) / info->texH[1]);
    VTX_OUT((float)(dstX + w),                                (float)dstY,
	    xFixedToFloat(srcTopRight.x) / info->texW[0],     xFixedToFloat(srcTopRight.y) / info->texH[0],
	    xFixedToFloat(maskTopRight.x) / info->texW[1],    xFixedToFloat(maskTopRight.y) / info->texH[1]);

    if (IS_R300_VARIANT || IS_AVIVO_VARIANT) {
	OUT_ACCEL_REG(R300_RB3D_DSTCACHE_CTLSTAT, R300_DC_FLUSH_3D | R300_DC_FREE_3D);
	OUT_ACCEL_REG(RADEON_WAIT_UNTIL, RADEON_WAIT_3D_IDLECLEAN);
    }

#ifdef ACCEL_CP
    ADVANCE_RING();
#else
    FINISH_ACCEL();
#endif /* !ACCEL_CP */

    LEAVE_DRAW(0);
}
#undef VTX_OUT

#ifdef ONLY_ONCE
static void RadeonDoneComposite(PixmapPtr pDst)
{
    ENTER_DRAW(0);
    LEAVE_DRAW(0);
}
#endif /* ONLY_ONCE */

#undef ONLY_ONCE
#undef FUNC_NAME
