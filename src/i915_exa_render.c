#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "i830.h"
#include "i915_reg.h"

#ifdef I830DEBUG
#define DEBUG_I830FALLBACK 1
#endif

#ifdef DEBUG_I830FALLBACK
#define I830FALLBACK(s, arg...)				\
do {							\
	DPRINTF(PFX, "EXA fallback: " s "\n", ##arg); 	\
	return FALSE;					\
} while(0)
#else
#define I830FALLBACK(s, arg...) 			\
do { 							\
	return FALSE;					\
} while(0) 
#endif

extern float scale_units[2][2];
extern int draw_coords[3][2];
extern Bool is_transform[2];
extern PictTransform *transform[2];

struct formatinfo {
    int fmt;
    CARD32 card_fmt;
};

struct blendinfo {
    Bool dst_alpha;
    Bool src_alpha;
    CARD32 blend_cntl;
};

extern Bool
I915EXACheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
		      PicturePtr pDstPicture);

extern Bool
I915EXAPrepareComposite(int op, PicturePtr pSrcPicture,
			PicturePtr pMaskPicture, PicturePtr pDstPicture,
			PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst);

/* copy from Eric's texture-video branch, move to header.. */
#define OUT_DCL(type, nr) do {                                          \
   CARD32 chans = 0;                                                    \
   if (REG_TYPE_##type == REG_TYPE_T)                                   \
      chans = D0_CHANNEL_ALL;                                           \
   else if (REG_TYPE_##type != REG_TYPE_S)                              \
      FatalError("wrong reg type %d to declare\n", REG_TYPE_##type);    \
   OUT_RING(D0_DCL |                                                    \
            (REG_TYPE_##type << D0_TYPE_SHIFT) | (nr << D0_NR_SHIFT) |  \
            chans);                                                     \
   OUT_RING(0x00000000);                                                \
   OUT_RING(0x00000000);                                                \
} while (0)

#define OUT_TEXLD(dest_type, dest_nr, sampler_nr, addr_type, addr_nr)   \
do {                                                                    \
      OUT_RING(T0_TEXLD |                                               \
               (REG_TYPE_##dest_type << T0_DEST_TYPE_SHIFT) |           \
               (dest_nr << T0_DEST_NR_SHIFT) |                          \
               (sampler_nr << T0_SAMPLER_NR_SHIFT));                    \
      OUT_RING((REG_TYPE_##addr_type << T1_ADDRESS_REG_TYPE_SHIFT) |    \
               (addr_nr << T1_ADDRESS_REG_NR_SHIFT));                   \
      OUT_RING(0x00000000);                                             \
} while (0)

/* XXX: It seems that offset of 915's blendfactor in Load_immediate_1
   is _different_ with i830, so I should just define plain value
   and use it with shift bits*/

#define I915_SRC_BLENDFACTOR_ZERO 		(1 << 8)
#define I915_SRC_BLENDFACTOR_ONE 		(2 << 8)
#define I915_SRC_BLENDFACTOR_SRC_COLOR		(3 << 8)
#define I915_SRC_BLENDFACTOR_INV_SRC_COLOR	(4 << 8)
#define I915_SRC_BLENDFACTOR_SRC_ALPHA		(5 << 8)
#define I915_SRC_BLENDFACTOR_INV_SRC_ALPHA	(6 << 8)
#define I915_SRC_BLENDFACTOR_DST_ALPHA		(7 << 8)
#define I915_SRC_BLENDFACTOR_INV_DST_ALPHA	(8 << 8)
#define I915_SRC_BLENDFACTOR_DST_COLOR		(9 << 8)
#define I915_SRC_BLENDFACTOR_INV_DST_COLOR	(0xa << 8)
#define I915_SRC_BLENDFACTOR_SRC_ALPHA_SATURATE (0xb << 8)
#define I915_SRC_BLENDFACTOR_CONST_COLOR	(0xc << 8)
#define I915_SRC_BLENDFACTOR_INV_CONST_COLOR	(0xd << 8)
#define I915_SRC_BLENDFACTOR_CONST_ALPHA	(0xe << 8)
#define I915_SRC_BLENDFACTOR_INV_CONST_ALPHA	(0xf << 8)
#define I915_SRC_BLENDFACTOR_MASK		(0xf << 8)

#define I915_DST_BLENDFACTOR_ZERO 		(1 << 4)
#define I915_DST_BLENDFACTOR_ONE 		(2 << 4)
#define I915_DST_BLENDFACTOR_SRC_COLOR		(3 << 4)
#define I915_DST_BLENDFACTOR_INV_SRC_COLOR	(4 << 4)
#define I915_DST_BLENDFACTOR_SRC_ALPHA		(5 << 4)
#define I915_DST_BLENDFACTOR_INV_SRC_ALPHA	(6 << 4)
#define I915_DST_BLENDFACTOR_DST_ALPHA		(7 << 4)
#define I915_DST_BLENDFACTOR_INV_DST_ALPHA	(8 << 4)
#define I915_DST_BLENDFACTOR_DST_COLOR		(9 << 4)
#define I915_DST_BLENDFACTOR_INV_DST_COLOR	(0xa << 4)
#define I915_DST_BLENDFACTOR_SRC_ALPHA_SATURATE (0xb << 4)
#define I915_DST_BLENDFACTOR_CONST_COLOR	(0xc << 4)
#define I915_DST_BLENDFACTOR_INV_CONST_COLOR	(0xd << 4)
#define I915_DST_BLENDFACTOR_CONST_ALPHA	(0xe << 4)
#define I915_DST_BLENDFACTOR_INV_CONST_ALPHA	(0xf << 4)
#define I915_DST_BLENDFACTOR_MASK		(0xf << 4)

static struct blendinfo I915BlendOp[] = { 
    /* Clear */
    {0, 0, I915_SRC_BLENDFACTOR_ZERO           | I915_DST_BLENDFACTOR_ZERO},
    /* Src */
    {0, 0, I915_SRC_BLENDFACTOR_ONE            | I915_DST_BLENDFACTOR_ZERO},
    /* Dst */
    {0, 0, I915_SRC_BLENDFACTOR_ZERO           | I915_DST_BLENDFACTOR_ONE},
    /* Over */
    {0, 1, I915_SRC_BLENDFACTOR_ONE            | I915_DST_BLENDFACTOR_INV_SRC_ALPHA},
    /* OverReverse */
    {1, 0, I915_SRC_BLENDFACTOR_INV_DST_ALPHA | I915_DST_BLENDFACTOR_ONE},
    /* In */
    {1, 0, I915_SRC_BLENDFACTOR_DST_ALPHA     | I915_DST_BLENDFACTOR_ZERO},
    /* InReverse */
    {0, 1, I915_SRC_BLENDFACTOR_ZERO           | I915_DST_BLENDFACTOR_SRC_ALPHA},
    /* Out */
    {1, 0, I915_SRC_BLENDFACTOR_INV_DST_ALPHA | I915_DST_BLENDFACTOR_ZERO},
    /* OutReverse */
    {0, 1, I915_SRC_BLENDFACTOR_ZERO           | I915_DST_BLENDFACTOR_INV_SRC_ALPHA},
    /* Atop */
    {1, 1, I915_SRC_BLENDFACTOR_DST_ALPHA     | I915_DST_BLENDFACTOR_INV_SRC_ALPHA},
    /* AtopReverse */
    {1, 1, I915_SRC_BLENDFACTOR_INV_DST_ALPHA | I915_DST_BLENDFACTOR_SRC_ALPHA},
    /* Xor */
    {1, 1, I915_SRC_BLENDFACTOR_INV_DST_ALPHA | I915_DST_BLENDFACTOR_INV_SRC_ALPHA},
    /* Add */
    {0, 0, I915_SRC_BLENDFACTOR_ONE            | I915_DST_BLENDFACTOR_ONE},
};

static struct formatinfo I915TexFormats[] = {
        {PICT_a8r8g8b8, MT_32BIT_ARGB8888 },
        {PICT_x8r8g8b8, MT_32BIT_XRGB8888 },
        {PICT_a8b8g8r8, MT_32BIT_ABGR8888 },
        {PICT_x8b8g8r8, MT_32BIT_XBGR8888 },
        {PICT_r5g6b5,   MT_16BIT_RGB565   },
        {PICT_a1r5g5b5, MT_16BIT_ARGB1555 },
        {PICT_x1r5g5b5, MT_16BIT_ARGB1555 },
        {PICT_a8,       MT_8BIT_I8 	  },
};

static CARD32 I915GetBlendCntl(int op, PicturePtr pMask, CARD32 dst_format)
{
    CARD32 sblend, dblend;

    sblend = I915BlendOp[op].blend_cntl & I915_SRC_BLENDFACTOR_MASK;
    dblend = I915BlendOp[op].blend_cntl & I915_DST_BLENDFACTOR_MASK;

    /* If there's no dst alpha channel, adjust the blend op so that we'll treat
     * it as always 1.
     */
    if (PICT_FORMAT_A(dst_format) == 0 && I915BlendOp[op].dst_alpha) {
        if (sblend == I915_SRC_BLENDFACTOR_DST_ALPHA)
            sblend = I915_SRC_BLENDFACTOR_ONE;
        else if (sblend == I915_SRC_BLENDFACTOR_INV_DST_ALPHA)
            sblend = I915_SRC_BLENDFACTOR_ZERO;
    }

    /* If the source alpha is being used, then we should only be in a case where
     * the source blend factor is 0, and the source blend value is the mask
     * channels multiplied by the source picture's alpha.
     */
    if (pMask && pMask->componentAlpha && I915BlendOp[op].src_alpha) {
        if (dblend == I915_DST_BLENDFACTOR_SRC_ALPHA) {
            dblend = I915_DST_BLENDFACTOR_SRC_COLOR;
        } else if (dblend == I915_DST_BLENDFACTOR_INV_SRC_ALPHA) {
            dblend = I915_DST_BLENDFACTOR_INV_SRC_COLOR;
        }
    }

    return sblend | dblend;
}

static Bool I915GetDestFormat(PicturePtr pDstPicture, CARD32 *dst_format)
{
    switch (pDstPicture->format) {
    case PICT_a8r8g8b8:
    case PICT_x8r8g8b8:
        *dst_format = COLR_BUF_ARGB8888;
        break;
    case PICT_r5g6b5:
        *dst_format = COLR_BUF_RGB565;
        break;
    case PICT_a1r5g5b5:
    case PICT_x1r5g5b5:
        *dst_format = COLR_BUF_ARGB1555;
        break;
    case PICT_a8:
        *dst_format = COLR_BUF_8BIT;
        break;
    case PICT_a4r4g4b4:
    case PICT_x4r4g4b4:
	*dst_format = COLR_BUF_ARGB4444;
	break;
    default:
        I830FALLBACK("Unsupported dest format 0x%x\n",
                        (int)pDstPicture->format);
    }

    return TRUE;
}

static Bool I915CheckCompositeTexture(PicturePtr pPict, int unit)
{
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    int i;
                                                                                                                                                            
    if ((w > 0x7ff) || (h > 0x7ff))
        I830FALLBACK("Picture w/h too large (%dx%d)\n", w, h);

    for (i = 0; i < sizeof(I915TexFormats) / sizeof(I915TexFormats[0]); i++)
    {
        if (I915TexFormats[i].fmt == pPict->format)
            break;
    }
    if (i == sizeof(I915TexFormats) / sizeof(I915TexFormats[0]))
        I830FALLBACK("Unsupported picture format 0x%x\n",
                         (int)pPict->format);

    /* FIXME: fix repeat support */
    if (pPict->repeat) 
	I830FALLBACK("repeat not support now!\n");

    if (pPict->filter != PictFilterNearest &&
        pPict->filter != PictFilterBilinear)
        I830FALLBACK("Unsupported filter 0x%x\n", pPict->filter);

    return TRUE;
}

Bool
I915EXACheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
		      PicturePtr pDstPicture)
{
    CARD32 tmp1;
    
    /* Check for unsupported compositing operations. */
    if (op >= sizeof(I915BlendOp) / sizeof(I915BlendOp[0]))
        I830FALLBACK("Unsupported Composite op 0x%x\n", op);
                                                                                                                                                            
    if (pMaskPicture != NULL && pMaskPicture->componentAlpha) {
        /* Check if it's component alpha that relies on a source alpha and on
         * the source value.  We can only get one of those into the single
         * source value that we get to blend with.
         */
        if (I915BlendOp[op].src_alpha &&
            (I915BlendOp[op].blend_cntl & I915_SRC_BLENDFACTOR_MASK) !=
             I915_SRC_BLENDFACTOR_ZERO)
            	I830FALLBACK("Component alpha not supported with source "
                            "alpha and source value blending.\n");
    }

    if (!I915CheckCompositeTexture(pSrcPicture, 0))
        I830FALLBACK("Check Src picture texture\n");
    if (pMaskPicture != NULL && !I915CheckCompositeTexture(pMaskPicture, 1))
        I830FALLBACK("Check Mask picture texture\n");

    if (!I915GetDestFormat(pDstPicture, &tmp1)) 
	I830FALLBACK("Get Color buffer format\n");

    return TRUE;
}

static Bool
I915TextureSetup(PicturePtr pPict, PixmapPtr pPix, int unit)
{
    ScrnInfoPtr pScrn = xf86Screens[pPict->pDrawable->pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 format, offset, pitch, filter;
    int w, h, i;
    CARD32 wrap_mode = TEXCOORDMODE_CLAMP_EDGE; 

    offset = exaGetPixmapOffset(pPix);
    pitch = exaGetPixmapPitch(pPix);
    w = pPict->pDrawable->width;
    h = pPict->pDrawable->height;
    scale_units[unit][0] = pPix->drawable.width;
    scale_units[unit][1] = pPix->drawable.height;
    draw_coords[unit][0] = pPix->drawable.x;
    draw_coords[unit][1] = pPix->drawable.y;

    for (i = 0; i < sizeof(I915TexFormats) / sizeof(I915TexFormats[0]); i++) {
        if (I915TexFormats[i].fmt == pPict->format)
	    break;
    }
    if ( i == sizeof(I915TexFormats)/ sizeof(I915TexFormats[0]) )
	I830FALLBACK("unknown texture format\n");
    format = I915TexFormats[i].card_fmt;

    if (pPict->repeat) 
	wrap_mode = TEXCOORDMODE_WRAP; /* XXX:correct ? */
    
    switch (pPict->filter) {
    case PictFilterNearest:
        filter = (FILTER_NEAREST << SS2_MAG_FILTER_SHIFT) | 
			(FILTER_NEAREST << SS2_MIN_FILTER_SHIFT);
        break;
    case PictFilterBilinear:
        filter = (FILTER_LINEAR << SS2_MAG_FILTER_SHIFT) | 
			(FILTER_LINEAR << SS2_MIN_FILTER_SHIFT);
        break;
    default:
	filter = 0;
        I830FALLBACK("Bad filter 0x%x\n", pPict->filter);
    }

    {
	CARD32 ms3;
	if (pI830->cpp == 1)
		format |= MAPSURF_8BIT;
	else if (pI830->cpp == 2)
		format |= MAPSURF_16BIT;
	else
		format |= MAPSURF_32BIT;

	BEGIN_LP_RING(6);
	OUT_RING(_3DSTATE_MAP_STATE | (3 * (1 << unit)));
	OUT_RING(1<<unit);
	OUT_RING(offset&MS2_ADDRESS_MASK);
	ms3 = (pPix->drawable.height << MS3_HEIGHT_SHIFT) | 
		(pPix->drawable.width << MS3_WIDTH_SHIFT) | format;
	if (!pI830->disableTiling)
		ms3 |= MS3_USE_FENCE_REGS;
	OUT_RING(ms3); 
	OUT_RING(pitch<<MS4_PITCH_SHIFT);
	OUT_RING(0);
	ADVANCE_LP_RING();
     }

     {
	CARD32 ss2, ss3;
	BEGIN_LP_RING(6);
	/* max & min mip level ? or base mip level? */

	OUT_RING(_3DSTATE_SAMPLER_STATE | (3*(1<<unit)));
	OUT_RING(1<<unit);
	ss2 = (MIPFILTER_NONE << SS2_MIP_FILTER_SHIFT);
	ss2 |= filter;
	OUT_RING(ss2);
	/* repeat? */
	ss3 = TEXCOORDMODE_WRAP << SS3_TCX_ADDR_MODE_SHIFT;
	ss3 |= (TEXCOORDMODE_WRAP << SS3_TCY_ADDR_MODE_SHIFT);
	ss3 |= SS3_NORMALIZED_COORDS;
	ss3 |= (unit << SS3_TEXTUREMAP_INDEX_SHIFT);
	OUT_RING(ss3);
	OUT_RING(0x00000000); /* default color */
	OUT_RING(0);

	ADVANCE_LP_RING();
    }

    if (pPict->transform != 0) {
        is_transform[unit] = TRUE;
        transform[unit] = pPict->transform;
    } else {
        is_transform[unit] = FALSE;
    }

#ifdef I830DEBUG
    ErrorF("try to sync to show any errors...");
    I830Sync(pScrn);
#endif
	
    return TRUE;
}

static void
I915DefCtxSetup(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    BEGIN_LP_RING(2);
    /* set default texture binding, may be in prepare better */
    OUT_RING(_3DSTATE_COORD_SET_BINDINGS | CSB_TCB(0,0) | CSB_TCB(1,1) |
	CSB_TCB(2,2) | CSB_TCB(3,3) | CSB_TCB(4,4) | CSB_TCB(5,5) |
	CSB_TCB(6,6) | CSB_TCB(7,7));
    OUT_RING(0);
    ADVANCE_LP_RING();
}

Bool
I915EXAPrepareComposite(int op, PicturePtr pSrcPicture,
			PicturePtr pMaskPicture, PicturePtr pDstPicture,
			PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst)
{
    ScrnInfoPtr pScrn = xf86Screens[pSrcPicture->pDrawable->pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 dst_format, dst_offset, dst_pitch;
    CARD32 blendctl;

ErrorF("i915 prepareComposite\n");

    I915GetDestFormat(pDstPicture, &dst_format);
    dst_offset = exaGetPixmapOffset(pDst);
    dst_pitch = exaGetPixmapPitch(pDst);
    draw_coords[2][0] = pDst->drawable.x;
    draw_coords[2][1] = pDst->drawable.y;
    
    I915DefCtxSetup(pScrn);

    if (!I915TextureSetup(pSrcPicture, pSrc, 0))
	I830FALLBACK("fail to setup src texture\n");
    if (pMask != NULL) {
	if (!I915TextureSetup(pMaskPicture, pMask, 1))
		I830FALLBACK("fail to setup mask texture\n");
    } else {
	is_transform[1] = FALSE;
	scale_units[1][0] = -1;
	scale_units[1][1] = -1;
    }

    {
	CARD32 ss2;
	BEGIN_LP_RING(24);
	/*color buffer*/
	OUT_RING(_3DSTATE_BUF_INFO_CMD);
	OUT_RING(BUF_3D_ID_COLOR_BACK| BUF_3D_PITCH(dst_pitch)); /* fence, tile? */
	OUT_RING(BUF_3D_ADDR(dst_offset));
	OUT_RING(MI_NOOP);
	
	OUT_RING(_3DSTATE_DST_BUF_VARS_CMD);
	OUT_RING(dst_format);

	/* XXX: defaults */
	OUT_RING(_3DSTATE_DFLT_Z_CMD);
	OUT_RING(0);

	OUT_RING(_3DSTATE_DFLT_DIFFUSE_CMD);
	OUT_RING(0);

	OUT_RING(_3DSTATE_DFLT_SPEC_CMD);
	OUT_RING(0);
	
	/* XXX:S3? define vertex format with tex coord sets number*/
	OUT_RING(_3DSTATE_LOAD_STATE_IMMEDIATE_1|I1_LOAD_S(2)|I1_LOAD_S(3)|
		I1_LOAD_S(4)|1);
	ss2 = S2_TEXCOORD_FMT(0, TEXCOORDFMT_2D);
	if (pMask)
		ss2 |= S2_TEXCOORD_FMT(1, TEXCOORDFMT_2D);
	else
		ss2 |= S2_TEXCOORD_FMT(1, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(2, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(3, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(4, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(5, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(6, TEXCOORDFMT_NOT_PRESENT);
	ss2 |= S2_TEXCOORD_FMT(7, TEXCOORDFMT_NOT_PRESENT);
	OUT_RING(ss2);
	OUT_RING(0x00000000); /*XXX: does ss3 needed? */
	OUT_RING((1<<S4_POINT_WIDTH_SHIFT)|S4_LINE_WIDTH_ONE| 
		S4_CULLMODE_NONE| S4_VFMT_XY);  

	/* issue a flush */
	OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
	OUT_RING(0);

	/* draw rect is unconditional */
	OUT_RING(_3DSTATE_DRAW_RECT_CMD);
	OUT_RING(0x00000000);
	OUT_RING(0x00000000);  /* ymin, xmin*/
	OUT_RING(DRAW_YMAX(pScrn->virtualY-1) | DRAW_XMAX(pScrn->virtualX-1));
	OUT_RING(0x00000000);  /* yorig, xorig (relate to color buffer?)*/
	OUT_RING(0);
	ADVANCE_LP_RING();
    }

	/* For (src In mask) operation */
	/* IN operator: Multiply src by mask components or mask alpha.*/
	/* TEXBLENDOP_MODULE: arg1*arg2 */

	/* LOAD_IMMEDIATE_1 ss6 ??*/

	/****
	   shader program prototype:
		dcl t0.xy
		dcl t1.xy
		dcl_2d s0   
		dcl_2d s1
		texld t0, s0
		texld t1, s1
		mul oC, t0, t1 ()
	***/
    if (!pMask) {
	BEGIN_LP_RING(1+3+3+3);
	OUT_RING(_3DSTATE_PIXEL_SHADER_PROGRAM |(3*3-1));
	OUT_DCL(S, 0);
	OUT_DCL(T, 0);
	OUT_TEXLD(OC, 0, 0, T, 0);
	ADVANCE_LP_RING();
    } else {
	BEGIN_LP_RING(1+3*6+3);
	OUT_RING(_3DSTATE_PIXEL_SHADER_PROGRAM |(3*7-1));
	OUT_DCL(S, 0);
	OUT_DCL(S, 1);
	OUT_DCL(T, 0);
	OUT_DCL(T, 1);
	OUT_TEXLD(R, 0, 0, T, 0);
	OUT_TEXLD(R, 1, 1, T, 1);
	if (pMaskPicture->componentAlpha && pDstPicture->format != PICT_a8) {
		/* then just mul */
		OUT_RING(A0_MUL | (REG_TYPE_OC << A0_DEST_TYPE_SHIFT) | 
			(0 << A0_DEST_NR_SHIFT) | A0_DEST_CHANNEL_ALL | 
			(REG_TYPE_R << A0_SRC0_TYPE_SHIFT) | (0 << A0_SRC0_NR_SHIFT));
		OUT_RING((SRC_X << A1_SRC0_CHANNEL_X_SHIFT)|(SRC_Y << A1_SRC0_CHANNEL_Y_SHIFT)|
			(SRC_Z << A1_SRC0_CHANNEL_Z_SHIFT)|(SRC_W << A1_SRC0_CHANNEL_W_SHIFT)|
			(REG_TYPE_R << A1_SRC1_TYPE_SHIFT) | (1 << A1_SRC1_NR_SHIFT) |
			(SRC_X << A1_SRC1_CHANNEL_X_SHIFT) | (SRC_Y << A1_SRC1_CHANNEL_Y_SHIFT));
		OUT_RING((SRC_Z << A2_SRC1_CHANNEL_Z_SHIFT) | (SRC_W << A2_SRC1_CHANNEL_W_SHIFT));
	} else {
		/* we should duplicate R1's w for all channel, Arithemic can choose channel to use! */
		OUT_RING(A0_MUL | (REG_TYPE_OC << A0_DEST_TYPE_SHIFT) |
			(0 << A0_DEST_NR_SHIFT) | A0_DEST_CHANNEL_ALL |
			(REG_TYPE_R << A0_SRC0_TYPE_SHIFT) | (0 << A0_SRC0_NR_SHIFT));
		OUT_RING((SRC_X << A1_SRC0_CHANNEL_X_SHIFT) | (SRC_Y << A1_SRC0_CHANNEL_Y_SHIFT) |
			(SRC_Z << A1_SRC0_CHANNEL_Z_SHIFT) | (SRC_W << A1_SRC0_CHANNEL_W_SHIFT) |
			(REG_TYPE_R << A1_SRC1_TYPE_SHIFT) | (1 << A1_SRC1_NR_SHIFT) |
			(SRC_W << A1_SRC1_CHANNEL_X_SHIFT) | (SRC_W << A1_SRC1_CHANNEL_Y_SHIFT));
		OUT_RING((SRC_W << A2_SRC1_CHANNEL_Z_SHIFT) | (SRC_W << A2_SRC1_CHANNEL_W_SHIFT));
	}
	ADVANCE_LP_RING();
    }
		
    {
	CARD32 ss6;
	blendctl = I915GetBlendCntl(op, pMaskPicture, pDstPicture->format);

	BEGIN_LP_RING(2);
	OUT_RING(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(6) | 0);
	ss6 = S6_CBUF_BLEND_ENABLE | S6_COLOR_WRITE_ENABLE;
	OUT_RING(ss6 | (0 << S6_CBUF_BLEND_FUNC_SHIFT) | blendctl);
	ADVANCE_LP_RING();
    }

#ifdef I830DEBUG
    ErrorF("try to sync to show any errors...");
    I830Sync(pScrn);
#endif

    return TRUE;
}
