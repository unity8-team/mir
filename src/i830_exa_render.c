#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "i830.h"
#include "i830_reg.h"

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

struct blendinfo {
    Bool dst_alpha;
    Bool src_alpha;
    CARD32 blend_cntl;
};

struct formatinfo {
    int fmt;
    CARD32 card_fmt;
};

extern Bool
I830EXACheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
		      PicturePtr pDstPicture);

extern Bool
I830EXAPrepareComposite(int op, PicturePtr pSrcPicture,
			PicturePtr pMaskPicture, PicturePtr pDstPicture,
			PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst);

/* I830's Blend factor definition, LOAD_STATE_IMMEDIATE_1 */
/* move to header...*/
#define I830_SRC_BLENDFACTOR_ZERO 		(1 << 4)
#define I830_SRC_BLENDFACTOR_ONE 		(2 << 4)
#define I830_SRC_BLENDFACTOR_SRC_COLOR		(3 << 4)
#define I830_SRC_BLENDFACTOR_INV_SRC_COLOR	(4 << 4)
#define I830_SRC_BLENDFACTOR_SRC_ALPHA		(5 << 4)
#define I830_SRC_BLENDFACTOR_INV_SRC_ALPHA	(6 << 4)
#define I830_SRC_BLENDFACTOR_DST_ALPHA		(7 << 4)
#define I830_SRC_BLENDFACTOR_INV_DST_ALPHA	(8 << 4)
#define I830_SRC_BLENDFACTOR_DST_COLOR		(9 << 4)
#define I830_SRC_BLENDFACTOR_INV_DST_COLOR	(0xa << 4)
#define I830_SRC_BLENDFACTOR_SRC_ALPHA_SATURATE (0xb << 4)
#define I830_SRC_BLENDFACTOR_CONST_COLOR	(0xc << 4)
#define I830_SRC_BLENDFACTOR_INV_CONST_COLOR	(0xd << 4)
#define I830_SRC_BLENDFACTOR_CONST_ALPHA	(0xe << 4)
#define I830_SRC_BLENDFACTOR_INV_CONST_ALPHA	(0xf << 4)
#define I830_SRC_BLENDFACTOR_MASK		(0xf << 4)

#define I830_DST_BLENDFACTOR_ZERO 		(1)
#define I830_DST_BLENDFACTOR_ONE 		(2)
#define I830_DST_BLENDFACTOR_SRC_COLOR		(3)
#define I830_DST_BLENDFACTOR_INV_SRC_COLOR	(4)
#define I830_DST_BLENDFACTOR_SRC_ALPHA		(5)
#define I830_DST_BLENDFACTOR_INV_SRC_ALPHA	(6)
#define I830_DST_BLENDFACTOR_DST_ALPHA		(7)
#define I830_DST_BLENDFACTOR_INV_DST_ALPHA	(8)
#define I830_DST_BLENDFACTOR_DST_COLOR		(9)
#define I830_DST_BLENDFACTOR_INV_DST_COLOR	(0xa)
#define I830_DST_BLENDFACTOR_SRC_ALPHA_SATURATE (0xb)
#define I830_DST_BLENDFACTOR_CONST_COLOR	(0xc)
#define I830_DST_BLENDFACTOR_INV_CONST_COLOR	(0xd)
#define I830_DST_BLENDFACTOR_CONST_ALPHA	(0xe)
#define I830_DST_BLENDFACTOR_INV_CONST_ALPHA	(0xf)
#define I830_DST_BLENDFACTOR_MASK		(0xf)

#define TB0C_LAST_STAGE	(1 << 31)
#define TB0C_RESULT_SCALE_1X		(0 << 29)
#define TB0C_RESULT_SCALE_2X		(1 << 29)
#define TB0C_RESULT_SCALE_4X		(2 << 29)
#define TB0C_OP_MODULE			(3 << 25)
#define TB0C_OUTPUT_WRITE_CURRENT	(0 << 24)
#define TB0C_OUTPUT_WRITE_ACCUM		(1 << 24)
#define TB0C_ARG3_REPLICATE_ALPHA 	(1<<23)
#define TB0C_ARG3_INVERT		(1<<22)
#define TB0C_ARG3_SEL_XXX
#define TB0C_ARG2_REPLICATE_ALPHA 	(1<<17)
#define TB0C_ARG2_INVERT		(1<<16)
#define TB0C_ARG2_SEL_ONE		(0 << 12)
#define TB0C_ARG2_SEL_FACTOR		(1 << 12)
#define TB0C_ARG2_SEL_TEXEL0		(6 << 12)
#define TB0C_ARG2_SEL_TEXEL1		(7 << 12)
#define TB0C_ARG2_SEL_TEXEL2		(8 << 12)
#define TB0C_ARG2_SEL_TEXEL3		(9 << 12)
#define TB0C_ARG1_REPLICATE_ALPHA 	(1<<11)
#define TB0C_ARG1_INVERT		(1<<10)
#define TB0C_ARG1_SEL_TEXEL0		(6 << 6)
#define TB0C_ARG1_SEL_TEXEL1		(7 << 6)
#define TB0C_ARG1_SEL_TEXEL2		(8 << 6)
#define TB0C_ARG1_SEL_TEXEL3		(9 << 6)
#define TB0C_ARG0_REPLICATE_ALPHA 	(1<<5)
#define TB0C_ARG0_SEL_XXX

#define TB0A_CTR_STAGE_ENABLE 		(1<<31)
#define TB0A_RESULT_SCALE_1X		(0 << 29)
#define TB0A_RESULT_SCALE_2X		(1 << 29)
#define TB0A_RESULT_SCALE_4X		(2 << 29)
#define TB0A_OP_MODULE			(3 << 25)
#define TB0A_OUTPUT_WRITE_CURRENT	(0<<24)
#define TB0A_OUTPUT_WRITE_ACCUM		(1<<24)
#define TB0A_CTR_STAGE_SEL_BITS_XXX
#define TB0A_ARG3_SEL_XXX
#define TB0A_ARG3_INVERT		(1<<17)
#define TB0A_ARG2_INVERT		(1<<16)
#define TB0A_ARG2_SEL_ONE		(0 << 12)
#define TB0A_ARG2_SEL_TEXEL0		(6 << 12)
#define TB0A_ARG2_SEL_TEXEL1		(7 << 12)
#define TB0A_ARG2_SEL_TEXEL2		(8 << 12)
#define TB0A_ARG2_SEL_TEXEL3		(9 << 12)
#define TB0A_ARG1_INVERT		(1<<10)
#define TB0A_ARG1_SEL_TEXEL0		(6 << 6)
#define TB0A_ARG1_SEL_TEXEL1		(7 << 6)
#define TB0A_ARG1_SEL_TEXEL2		(8 << 6)
#define TB0A_ARG1_SEL_TEXEL3		(9 << 6)
#define TB0A_ARG0_SEL_XXX

/* end */


static struct blendinfo I830BlendOp[] = { 
    /* Clear */
    {0, 0, I830_SRC_BLENDFACTOR_ZERO           | I830_DST_BLENDFACTOR_ZERO},
    /* Src */
    {0, 0, I830_SRC_BLENDFACTOR_ONE            | I830_DST_BLENDFACTOR_ZERO},
    /* Dst */
    {0, 0, I830_SRC_BLENDFACTOR_ZERO           | I830_DST_BLENDFACTOR_ONE},
    /* Over */
    {0, 1, I830_SRC_BLENDFACTOR_ONE            | I830_DST_BLENDFACTOR_INV_SRC_ALPHA},
    /* OverReverse */
    {1, 0, I830_SRC_BLENDFACTOR_INV_DST_ALPHA | I830_DST_BLENDFACTOR_ONE},
    /* In */
    {1, 0, I830_SRC_BLENDFACTOR_DST_ALPHA     | I830_DST_BLENDFACTOR_ZERO},
    /* InReverse */
    {0, 1, I830_SRC_BLENDFACTOR_ZERO           | I830_DST_BLENDFACTOR_SRC_ALPHA},
    /* Out */
    {1, 0, I830_SRC_BLENDFACTOR_INV_DST_ALPHA | I830_DST_BLENDFACTOR_ZERO},
    /* OutReverse */
    {0, 1, I830_SRC_BLENDFACTOR_ZERO           | I830_DST_BLENDFACTOR_INV_SRC_ALPHA},
    /* Atop */
    {1, 1, I830_SRC_BLENDFACTOR_DST_ALPHA     | I830_DST_BLENDFACTOR_INV_SRC_ALPHA},
    /* AtopReverse */
    {1, 1, I830_SRC_BLENDFACTOR_INV_DST_ALPHA | I830_DST_BLENDFACTOR_SRC_ALPHA},
    /* Xor */
    {1, 1, I830_SRC_BLENDFACTOR_INV_DST_ALPHA | I830_DST_BLENDFACTOR_INV_SRC_ALPHA},
    /* Add */
    {0, 0, I830_SRC_BLENDFACTOR_ONE            | I830_DST_BLENDFACTOR_ONE},
};


static struct formatinfo I830TexFormats[] = {
        {PICT_a8r8g8b8, MT_32BIT_ARGB8888 },
        {PICT_x8r8g8b8, MT_32BIT_ARGB8888 },
        {PICT_a8b8g8r8, MT_32BIT_ABGR8888 },
        {PICT_x8b8g8r8, MT_32BIT_ABGR8888 },
        {PICT_r5g6b5,   MT_16BIT_RGB565	  },
        {PICT_a1r5g5b5, MT_16BIT_ARGB1555 },
        {PICT_x1r5g5b5, MT_16BIT_ARGB1555 },
        {PICT_a8,       MT_8BIT_I8       },
};

static Bool I830GetDestFormat(PicturePtr pDstPicture, CARD32 *dst_format)
{
	/* XXX: color buffer format for i830 */
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


static CARD32 I830GetBlendCntl(int op, PicturePtr pMask, CARD32 dst_format)
{
    CARD32 sblend, dblend;

    sblend = I830BlendOp[op].blend_cntl & I830_SRC_BLENDFACTOR_MASK;
    dblend = I830BlendOp[op].blend_cntl & I830_DST_BLENDFACTOR_MASK;

    /* If there's no dst alpha channel, adjust the blend op so that we'll treat
     * it as always 1.
     */
    if (PICT_FORMAT_A(dst_format) == 0 && I830BlendOp[op].dst_alpha) {
        if (sblend == I830_SRC_BLENDFACTOR_DST_ALPHA)
            sblend = I830_SRC_BLENDFACTOR_ONE;
        else if (sblend == I830_SRC_BLENDFACTOR_INV_DST_ALPHA)
            sblend = I830_SRC_BLENDFACTOR_ZERO;
    }

    /* If the source alpha is being used, then we should only be in a case where
     * the source blend factor is 0, and the source blend value is the mask
     * channels multiplied by the source picture's alpha.
     */
    if (pMask && pMask->componentAlpha && I830BlendOp[op].src_alpha) {
        if (dblend == I830_DST_BLENDFACTOR_SRC_ALPHA) {
            dblend = I830_DST_BLENDFACTOR_SRC_COLOR;
        } else if (dblend == I830_DST_BLENDFACTOR_INV_SRC_ALPHA) {
            dblend = I830_DST_BLENDFACTOR_INV_SRC_COLOR;
        }
    }

    return sblend | dblend;
}

static Bool I830CheckCompositeTexture(PicturePtr pPict, int unit)
{
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    int i;
                                                                                                                                                            
    if ((w > 0x7ff) || (h > 0x7ff))
        I830FALLBACK("Picture w/h too large (%dx%d)\n", w, h);

    for (i = 0; i < sizeof(I830TexFormats) / sizeof(I830TexFormats[0]); i++)
    {
        if (I830TexFormats[i].fmt == pPict->format)
            break;
    }
    if (i == sizeof(I830TexFormats) / sizeof(I830TexFormats[0]))
        I830FALLBACK("Unsupported picture format 0x%x\n",
                         (int)pPict->format);

    /* FIXME: fix repeat support */
    if (pPict->repeat)
	I830FALLBACK("repeat unsupport now\n");

    if (pPict->filter != PictFilterNearest &&
        pPict->filter != PictFilterBilinear)
        I830FALLBACK("Unsupported filter 0x%x\n", pPict->filter);

    return TRUE;
}

static Bool
I830TextureSetup(PicturePtr pPict, PixmapPtr pPix, int unit)
{

    ScrnInfoPtr pScrn = xf86Screens[pPict->pDrawable->pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 format, offset, pitch, filter;
    int w, h, i;
    CARD32 wrap_mode = TEXCOORDMODE_CLAMP; 

    offset = exaGetPixmapOffset(pPix);
    pitch = exaGetPixmapPitch(pPix);
    w = pPict->pDrawable->width;
    h = pPict->pDrawable->height;
    scale_units[unit][0] = pPix->drawable.width;
    scale_units[unit][1] = pPix->drawable.height;
    draw_coords[unit][0] = pPix->drawable.x;
    draw_coords[unit][1] = pPix->drawable.y;

    for (i = 0; i < sizeof(I830TexFormats) / sizeof(I830TexFormats[0]); i++) {
        if (I830TexFormats[i].fmt == pPict->format)
	    break;
    }
    if ( i == sizeof(I830TexFormats)/ sizeof(I830TexFormats[0]) )
	I830FALLBACK("unknown texture format\n");
    format = I830TexFormats[i].card_fmt;

    if (pPict->repeat) 
	wrap_mode = TEXCOORDMODE_WRAP; /* XXX: correct ? */
    
    switch (pPict->filter) {
    case PictFilterNearest:
        filter = ((FILTER_NEAREST<<TM0S3_MAG_FILTER_SHIFT) | 
			(FILTER_NEAREST<<TM0S3_MIN_FILTER_SHIFT));
        break;
    case PictFilterBilinear:
        filter = ((FILTER_LINEAR<<TM0S3_MAG_FILTER_SHIFT) | 
			(FILTER_LINEAR<<TM0S3_MIN_FILTER_SHIFT));
        break;
    default:
	filter = 0;
        I830FALLBACK("Bad filter 0x%x\n", pPict->filter);
    }

    {
	if (pI830->cpp == 1)
		format |= MAP_SURFACE_8BIT;
	else if (pI830->cpp == 2)
		format |= MAP_SURFACE_16BIT;
	else
		format |= MAP_SURFACE_32BIT;

	BEGIN_LP_RING(6);
	OUT_RING(_3DSTATE_MAP_INFO_CMD);
	OUT_RING(format | TEXMAP_INDEX(unit) | MAP_FORMAT_2D);
	OUT_RING((pPix->drawable.height<<16)|pPix->drawable.width); /* height, width */
	OUT_RING(offset<<2); /* map address */
	OUT_RING(pitch<<2); /* map pitch */
	OUT_RING(0);
	ADVANCE_LP_RING();
     }

     {
	BEGIN_LP_RING(2);
	/* coord sets */
	OUT_RING(_3DSTATE_MAP_COORD_SET_CMD | TEXCOORD_SET(unit) | 
		ENABLE_TEXCOORD_PARAMS | TEXCOORDS_ARE_NORMAL | /*XXX, check this, and fix vertex tex coord*/
		TEXCOORDTYPE_CARTESIAN | ENABLE_ADDR_V_CNTL | TEXCOORD_ADDR_V_MODE(wrap_mode) |
		ENABLE_ADDR_U_CNTL | TEXCOORD_ADDR_U_MODE(wrap_mode));
	OUT_RING(MI_NOOP);

	/* XXX: filter seems hang engine...*/
#if 0
	OUT_RING(I830_STATE3D_MAP_FILTER | FILTER_MAP_INDEX(unit) | ENABLE_KEYS| DISABLE_COLOR_KEY | DISABLE_CHROMA_KEY | DISABLE_KILL_PIXEL |ENABLE_MIP_MODE_FILTER | MIPFILTER_NONE | filter);
	OUT_RING(0);
#endif

	/* max & min mip level ? or base mip level? */

	ADVANCE_LP_RING();
    }

	/* XXX */
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

Bool
I830EXACheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
		      PicturePtr pDstPicture)
{
    CARD32 tmp1;
    
    /* Check for unsupported compositing operations. */
    if (op >= sizeof(I830BlendOp) / sizeof(I830BlendOp[0]))
        I830FALLBACK("Unsupported Composite op 0x%x\n", op);
                                                                                                                                                            
    if (pMaskPicture != NULL && pMaskPicture->componentAlpha) {
        /* Check if it's component alpha that relies on a source alpha and on
         * the source value.  We can only get one of those into the single
         * source value that we get to blend with.
         */
        if (I830BlendOp[op].src_alpha &&
            (I830BlendOp[op].blend_cntl & I830_SRC_BLENDFACTOR_MASK) !=
             I830_SRC_BLENDFACTOR_ZERO)
            	I830FALLBACK("Component alpha not supported with source "
                            "alpha and source value blending.\n");
    }

    if (!I830CheckCompositeTexture(pSrcPicture, 0))
        I830FALLBACK("Check Src picture texture\n");
    if (pMaskPicture != NULL && !I830CheckCompositeTexture(pMaskPicture, 1))
        I830FALLBACK("Check Mask picture texture\n");

    if (!I830GetDestFormat(pDstPicture, &tmp1)) 
	I830FALLBACK("Get Color buffer format\n");

    return TRUE;
}


static void
I830DefCtxSetup(ScrnInfoPtr pScrn)
{
	/* coord binding */
	CARD32 mcb;
    	I830Ptr pI830 = I830PTR(pScrn);

	BEGIN_LP_RING(2);
	OUT_RING(_3DSTATE_MAP_COORD_SETBIND_CMD);
	mcb = TEXBIND_SET3(TEXCOORDSRC_VTXSET_3);
	mcb |= TEXBIND_SET2(TEXCOORDSRC_VTXSET_2);
	mcb |= TEXBIND_SET1(TEXCOORDSRC_VTXSET_1);
	mcb |= TEXBIND_SET0(TEXCOORDSRC_VTXSET_0);
	OUT_RING(mcb);
	ADVANCE_LP_RING();
}


Bool
I830EXAPrepareComposite(int op, PicturePtr pSrcPicture,
			PicturePtr pMaskPicture, PicturePtr pDstPicture,
			PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst)
{
/* XXX: setup texture map from pixmap, vertex format, blend cntl */
    ScrnInfoPtr pScrn = xf86Screens[pSrcPicture->pDrawable->pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 dst_format, dst_offset, dst_pitch;

    I830GetDestFormat(pDstPicture, &dst_format);
    dst_offset = exaGetPixmapOffset(pDst);
    dst_pitch = exaGetPixmapPitch(pDst);
    draw_coords[2][0] = pDst->drawable.x;
    draw_coords[2][1] = pDst->drawable.y;
    
    I830DefCtxSetup(pScrn);

    if (!I830TextureSetup(pSrcPicture, pSrc, 0))
	I830FALLBACK("fail to setup src texture\n");
    if (pMask != NULL) {
	if (!I830TextureSetup(pMaskPicture, pMask, 1))
		I830FALLBACK("fail to setup mask texture\n");
    } else {
	is_transform[1] = FALSE;
	scale_units[1][0] = -1;
	scale_units[1][1] = -1;
    }

    {

	CARD32 cblend, ablend, blendctl, vf2;

	BEGIN_LP_RING(22);
	
	/*color buffer*/
	OUT_RING(_3DSTATE_BUF_INFO_CMD);
	OUT_RING(BUF_3D_ID_COLOR_BACK| BUF_3D_PITCH(dst_pitch));
	OUT_RING(BUF_3D_ADDR(dst_offset));
	OUT_RING(MI_NOOP);
	
	OUT_RING(_3DSTATE_DST_BUF_VARS_CMD);
	OUT_RING(dst_format);

	/* defaults */
	OUT_RING(_3DSTATE_DFLT_Z_CMD);
	OUT_RING(0);

	OUT_RING(_3DSTATE_DFLT_DIFFUSE_CMD);
	OUT_RING(0);

	OUT_RING(_3DSTATE_DFLT_SPEC_CMD);
	OUT_RING(0);
	
	OUT_RING(_3DSTATE_LOAD_STATE_IMMEDIATE_1|I1_LOAD_S(3)|0);
	OUT_RING((1<<S3_POINT_WIDTH_SHIFT) | (2<<S3_LINE_WIDTH_SHIFT) | S3_CULLMODE_NONE| S3_VERTEXHAS_XY);  
	OUT_RING(_3DSTATE_LOAD_STATE_IMMEDIATE_1|I1_LOAD_S(2)|0);
	if (pMask)
		vf2 = 2 << 12; /* 2 texture coord sets */
	else
		vf2 = 1 << 12;
	vf2 |= (TEXCOORDFMT_2D << 16);
	if (pMask)
		vf2 |= (TEXCOORDFMT_2D << 18);
	else
		vf2 |= (TEXCOORDFMT_1D << 18);
		
	vf2 |= (TEXCOORDFMT_1D << 20);
	vf2 |= (TEXCOORDFMT_1D << 22);
	vf2 |= (TEXCOORDFMT_1D << 24);
	vf2 |= (TEXCOORDFMT_1D << 26);
	vf2 |= (TEXCOORDFMT_1D << 28);
	vf2 |= (TEXCOORDFMT_1D << 30);
	OUT_RING(vf2);

	/* For (src In mask) operation */
	/* IN operator: Multiply src by mask components or mask alpha.*/
	/* TEXBLENDOP_MODULE: arg1*arg2 */
	cblend = TB0C_LAST_STAGE | TB0C_RESULT_SCALE_1X | TB0C_OP_MODULE |
		 TB0C_OUTPUT_WRITE_CURRENT;  
	ablend = TB0A_RESULT_SCALE_1X | TB0A_OP_MODULE | TB0A_OUTPUT_WRITE_CURRENT;
	
	cblend |= TB0C_ARG1_SEL_TEXEL0;
	ablend |= TB0A_ARG1_SEL_TEXEL0;
	if (pMask) {
		if (pMaskPicture->componentAlpha && pDstPicture->format != PICT_a8)
			cblend |= TB0C_ARG2_SEL_TEXEL1;
		else
			cblend |= (TB0C_ARG2_SEL_TEXEL1 | TB0C_ARG2_REPLICATE_ALPHA);
		ablend |= TB0A_ARG2_SEL_TEXEL1;
	} else {
		cblend |= TB0C_ARG2_SEL_ONE;
		ablend |= TB0A_ARG2_SEL_ONE;		
	}
		
	OUT_RING(_3DSTATE_LOAD_STATE_IMMEDIATE_2 | LOAD_TEXTURE_BLEND_STAGE(0)|1);
	OUT_RING(cblend);
	OUT_RING(ablend);
	OUT_RING(0);

	blendctl = I830GetBlendCntl(op, pMaskPicture, pDstPicture->format);
	OUT_RING(_3DSTATE_LOAD_STATE_IMMEDIATE_1 | I1_LOAD_S(8) | 0);
	OUT_RING(S8_ENABLE_COLOR_BLEND | S8_BLENDFUNC_ADD |(blendctl<<4) |
		S8_ENABLE_COLOR_BUFFER_WRITE);	
	ADVANCE_LP_RING();
    }

#ifdef I830DEBUG
   Error("try to sync to show any errors...");
   I830Sync(pScrn);
#endif

    return TRUE;
}

