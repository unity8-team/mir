
#include "dixstruct.h"

#include "xaa.h"
#include "xaalocal.h"

#ifndef RENDER_GENERIC_HELPER
#define RENDER_GENERIC_HELPER

static void RadeonInit3DEngineMMIO(ScrnInfoPtr pScrn);
#ifdef XF86DRI
static void RadeonInit3DEngineCP(ScrnInfoPtr pScrn);
#endif

struct blendinfo {
	Bool dst_alpha;
	Bool src_alpha;
	CARD32 blend_cntl;
};

/* The first part of blend_cntl corresponds to Fa from the render "protocol"
 * document, and the second part to Fb.
 */
static const struct blendinfo RadeonBlendOp[] = {
    /* Clear */
    {0, 0, RADEON_SRC_BLEND_GL_ZERO |
	   RADEON_DST_BLEND_GL_ZERO},
    /* Src */
    {0, 0, RADEON_SRC_BLEND_GL_ONE |
	   RADEON_DST_BLEND_GL_ZERO},
    /* Dst */
    {0, 0, RADEON_SRC_BLEND_GL_ZERO |
	   RADEON_DST_BLEND_GL_ONE},
    /* Over */
    {0, 1, RADEON_SRC_BLEND_GL_ONE |
	   RADEON_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA},
    /* OverReverse */
    {1, 0, RADEON_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA |
	   RADEON_DST_BLEND_GL_ONE},
    /* In */
    {1, 0, RADEON_SRC_BLEND_GL_DST_ALPHA |
	   RADEON_DST_BLEND_GL_ZERO},
    /* InReverse */
    {0, 1, RADEON_SRC_BLEND_GL_ZERO |
	   RADEON_DST_BLEND_GL_SRC_ALPHA},
    /* Out */
    {1, 0, RADEON_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA |
	   RADEON_DST_BLEND_GL_ZERO},
    /* OutReverse */
    {0, 1, RADEON_SRC_BLEND_GL_ZERO |
	   RADEON_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA},
    /* Atop */
    {1, 1, RADEON_SRC_BLEND_GL_DST_ALPHA |
	   RADEON_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA},
    /* AtopReverse */
    {1, 1, RADEON_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA |
	   RADEON_DST_BLEND_GL_SRC_ALPHA},
    /* Xor */
    {1, 1, RADEON_SRC_BLEND_GL_ONE_MINUS_DST_ALPHA |
	   RADEON_DST_BLEND_GL_ONE_MINUS_SRC_ALPHA},
    /* Add */
    {0, 0, RADEON_SRC_BLEND_GL_ONE |
	   RADEON_DST_BLEND_GL_ONE},
    /* Saturate */
    {0, 1, RADEON_SRC_BLEND_GL_SRC_ALPHA_SATURATE |
	   RADEON_DST_BLEND_GL_ONE},
    {0, 0, 0},
    {0, 0, 0},
    /* DisjointClear */
    {0, 0, RADEON_SRC_BLEND_GL_ZERO |
	   RADEON_DST_BLEND_GL_ZERO},
    /* DisjointSrc */
    {0, 0, RADEON_SRC_BLEND_GL_ONE |
	   RADEON_DST_BLEND_GL_ZERO},
    /* DisjointDst */
    {0, 0, RADEON_SRC_BLEND_GL_ZERO |
	   RADEON_DST_BLEND_GL_ONE},
    /* DisjointOver unsupported */
    {0, 0, 0},
    /* DisjointOverReverse */
    {1, 1, RADEON_SRC_BLEND_GL_SRC_ALPHA_SATURATE |
	   RADEON_DST_BLEND_GL_ONE},
    /* DisjointIn unsupported */
    {0, 0, 0},
    /* DisjointInReverse unsupported */
    {0, 0, 0},
    /* DisjointOut unsupported */
    {1, 1, RADEON_SRC_BLEND_GL_SRC_ALPHA_SATURATE |
	   RADEON_DST_BLEND_GL_ZERO},
    /* DisjointOutReverse unsupported */
    {0, 0, 0},
    /* DisjointAtop unsupported */
    {0, 0, 0},
    /* DisjointAtopReverse unsupported */
    {0, 0, 0},
    /* DisjointXor unsupported */
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    /* ConjointClear */
    {0, 0, RADEON_SRC_BLEND_GL_ZERO |
	   RADEON_DST_BLEND_GL_ZERO},
    /* ConjointSrc */
    {0, 0, RADEON_SRC_BLEND_GL_ONE |
	   RADEON_DST_BLEND_GL_ZERO},
    /* ConjointDst */
    {0, 0, RADEON_SRC_BLEND_GL_ZERO |
	   RADEON_DST_BLEND_GL_ONE},
};
#define RadeonOpMax (sizeof(RadeonBlendOp) / sizeof(RadeonBlendOp[0]))

/* Note on texture formats:
 * TXFORMAT_Y8 expands to (Y,Y,Y,1).  TXFORMAT_I8 expands to (I,I,I,I)
 * The RADEON and R200 TXFORMATS we use are the same on r100/r200.
 */

static CARD32 RADEONTextureFormats[] = {
    PICT_a8r8g8b8,
    PICT_a8,
    PICT_x8r8g8b8,
    PICT_r5g6b5,
    PICT_x1r5g5b5,
};

static void RadeonGetTextureFormat(CARD32 format, CARD32 *txformat, int *bytepp)
{
    switch (format) {
    case PICT_a8r8g8b8:
	*txformat = RADEON_TXFORMAT_ARGB8888 | RADEON_TXFORMAT_ALPHA_IN_MAP;
	*bytepp = 4;
	break;
    case PICT_a8:
	*txformat = RADEON_TXFORMAT_I8 | RADEON_TXFORMAT_ALPHA_IN_MAP;
	*bytepp = 1;
	break;
    case PICT_x8r8g8b8:
	*txformat = RADEON_TXFORMAT_ARGB8888;
	*bytepp = 4;
	break;
    case PICT_r5g6b5:
	*txformat = RADEON_TXFORMAT_RGB565;
	*bytepp = 2;
	break;
    case PICT_a1r5g5b5:
	*txformat = RADEON_TXFORMAT_ARGB1555 | RADEON_TXFORMAT_ALPHA_IN_MAP;
	*bytepp = 2;
	break;
    case PICT_x1r5g5b5:
	*txformat = RADEON_TXFORMAT_ARGB1555;
	*bytepp = 2;
	break;
    }
}

static __inline__ CARD32 F_TO_DW(float val)
{
    union {
	float f;
	CARD32 l;
    } tmp;
    tmp.f = val;
    return tmp.l;
}

/* Compute log base 2 of val. */
static __inline__ int
ATILog2(int val)
{
	int bits;

	for (bits = 0; val != 0; val >>= 1, ++bits)
		;
	return bits - 1;
}

void RADEONInit3DEngineForRender(ScrnInfoPtr pScrn)
{
#ifdef XF86DRI
    RADEONInfoPtr info = RADEONPTR (pScrn);

    if (info->directRenderingEnabled)
	RadeonInit3DEngineCP(pScrn);
    else
#endif
	RadeonInit3DEngineMMIO(pScrn);
}

static void
RemoveLinear (FBLinearPtr linear)
{
   RADEONInfoPtr info = (RADEONInfoPtr)(linear->devPrivate.ptr);

   info->RenderTex = NULL; 
}

static void
RenderCallback (ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

    if ((currentTime.milliseconds > info->RenderTimeout) && info->RenderTex) {
	xf86FreeOffscreenLinear(info->RenderTex);
	info->RenderTex = NULL;
    }

    if (!info->RenderTex)
	info->RenderCallback = NULL;
}

static Bool
AllocateLinear (
   ScrnInfoPtr pScrn,
   int sizeNeeded
){
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

   info->RenderTimeout = currentTime.milliseconds + 30000;
   info->RenderCallback = RenderCallback;

   if (info->RenderTex) {
	if (info->RenderTex->size >= sizeNeeded)
	   return TRUE;
	else {
	   if (xf86ResizeOffscreenLinear(info->RenderTex, sizeNeeded))
		return TRUE;

	   xf86FreeOffscreenLinear(info->RenderTex);
	   info->RenderTex = NULL;
	}
   }

   info->RenderTex = xf86AllocateOffscreenLinear(pScrn->pScreen, sizeNeeded, 32,
						 NULL, RemoveLinear, info);

   return (info->RenderTex != NULL);
}

#endif

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


static void FUNC_NAME(RadeonInit3DEngine)(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    ACCEL_PREAMBLE();

    if (info->ChipFamily >= CHIP_FAMILY_R300) {
	/* Unimplemented */
    } else if ((info->ChipFamily == CHIP_FAMILY_RV250) || 
	       (info->ChipFamily == CHIP_FAMILY_RV280) || 
	       (info->ChipFamily == CHIP_FAMILY_RS300) || 
	       (info->ChipFamily == CHIP_FAMILY_R200)) {
	BEGIN_ACCEL(7);
	OUT_ACCEL_REG(R200_SE_VAP_CNTL_STATUS, 0);
	OUT_ACCEL_REG(R200_PP_CNTL_X, 0);
	OUT_ACCEL_REG(R200_PP_TXMULTI_CTL_0, 0);
	OUT_ACCEL_REG(R200_SE_VTX_STATE_CNTL, 0);
	OUT_ACCEL_REG(R200_RE_CNTL, 0x0);
	/* XXX: correct?  Want it to be like RADEON_VTX_ST?_NONPARAMETRIC */
	OUT_ACCEL_REG(R200_SE_VTE_CNTL, R200_VTX_ST_DENORMALIZED);
	OUT_ACCEL_REG(R200_SE_VAP_CNTL, R200_VAP_FORCE_W_TO_ONE |
	    R200_VAP_VF_MAX_VTX_NUM);
	FINISH_ACCEL();
    } else {
	BEGIN_ACCEL(2);
	OUT_ACCEL_REG(RADEON_SE_CNTL_STATUS, RADEON_TCL_BYPASS);
	OUT_ACCEL_REG(RADEON_SE_COORD_FMT,
	    RADEON_VTX_XY_PRE_MULT_1_OVER_W0 |
	    RADEON_VTX_ST0_NONPARAMETRIC |
	    RADEON_VTX_ST1_NONPARAMETRIC |
	    RADEON_TEX1_W_ROUTING_USE_W0);
	FINISH_ACCEL();
    }

    BEGIN_ACCEL(3);
    OUT_ACCEL_REG(RADEON_RE_TOP_LEFT, 0);
    OUT_ACCEL_REG(RADEON_RE_WIDTH_HEIGHT, 0x07ff07ff);
    OUT_ACCEL_REG(RADEON_SE_CNTL, RADEON_DIFFUSE_SHADE_GOURAUD |
				  RADEON_BFACE_SOLID | 
				  RADEON_FFACE_SOLID |
				  RADEON_VTX_PIX_CENTER_OGL |
				  RADEON_ROUND_MODE_ROUND |
				  RADEON_ROUND_PREC_4TH_PIX);
    FINISH_ACCEL();
}

static Bool FUNC_NAME(R100SetupTexture)(ScrnInfoPtr pScrn,
	int format,
	CARD8 *src,
	int src_pitch,
	int width,
	int height,
	int flags)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    CARD8 *dst;
    CARD32 tex_size = 0, txformat;
    int dst_pitch, offset, size, i, tex_bytepp;
    ACCEL_PREAMBLE();

    if ((width > 2048) || (height > 2048))
	return FALSE;

    RadeonGetTextureFormat(format, &txformat, &tex_bytepp);

    dst_pitch = (width * tex_bytepp + 31) & ~31;
    size = dst_pitch * height;

    if (!AllocateLinear(pScrn, size))
	return FALSE;

    if (flags & XAA_RENDER_REPEAT) {
	txformat |= ATILog2(width) << RADEON_TXFORMAT_WIDTH_SHIFT;
	txformat |= ATILog2(height) << RADEON_TXFORMAT_HEIGHT_SHIFT;
    } else {
	tex_size = ((height - 1) << 16) | (width - 1);
	txformat |= RADEON_TXFORMAT_NON_POWER2;
    }

    offset = info->RenderTex->offset * pScrn->bitsPerPixel / 8;

    /* Upload texture to card.  Should use ImageWrite to avoid syncing. */
    i = height;
    dst = (CARD8*)(info->FB + offset);
    if (info->accel->NeedToSync)
	info->accel->Sync(pScrn);
    while(i--) {
	memcpy(dst, src, width * tex_bytepp);
	src += src_pitch;
	dst += dst_pitch;
    }

    BEGIN_ACCEL(5);
    OUT_ACCEL_REG(RADEON_PP_TXFORMAT_0, txformat);
    OUT_ACCEL_REG(RADEON_PP_TEX_SIZE_0, tex_size);
    OUT_ACCEL_REG(RADEON_PP_TEX_PITCH_0, dst_pitch - 32);
    OUT_ACCEL_REG(RADEON_PP_TXOFFSET_0, offset + info->fbLocation +
					pScrn->fbOffset);
    OUT_ACCEL_REG(RADEON_PP_TXFILTER_0, RADEON_MAG_FILTER_LINEAR |
					RADEON_MIN_FILTER_LINEAR |
					RADEON_CLAMP_S_WRAP |
					RADEON_CLAMP_T_WRAP);
    FINISH_ACCEL();

    return TRUE;
}

static Bool
FUNC_NAME(R100SetupForCPUToScreenAlphaTexture) (
	ScrnInfoPtr	pScrn,
	int		op,
	CARD16		red,
	CARD16		green,
	CARD16		blue,
	CARD16		alpha,
	int		alphaFormat,
	CARD8		*alphaPtr,
	int		alphaPitch,
	int		width,
	int		height,
	int		flags
) 
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    CARD32 format, srccolor;
    ACCEL_PREAMBLE();

    if (op >= RadeonOpMax || RadeonBlendOp[op].blend_cntl == 0)
	return FALSE;
    
    if (!FUNC_NAME(R100SetupTexture)(pScrn, alphaFormat, alphaPtr, alphaPitch,
				     width, height, flags))
	return FALSE;

    if (pScrn->bitsPerPixel == 32)
	format = RADEON_COLOR_FORMAT_ARGB8888;
    else
	format = RADEON_COLOR_FORMAT_RGB565;

    srccolor = ((alpha & 0xff00) << 16) | ((red & 0xff00) << 8) | (blue >> 8) |
	(green & 0xff00);

    BEGIN_ACCEL(8);
    OUT_ACCEL_REG(RADEON_RB3D_CNTL, format | RADEON_ALPHA_BLEND_ENABLE);
    OUT_ACCEL_REG(RADEON_RB3D_COLORPITCH, pScrn->displayWidth);
    OUT_ACCEL_REG(RADEON_PP_CNTL, RADEON_TEX_0_ENABLE |
				  RADEON_TEX_BLEND_0_ENABLE);
    OUT_ACCEL_REG(RADEON_PP_TFACTOR_0, srccolor);
    OUT_ACCEL_REG(RADEON_PP_TXCBLEND_0, RADEON_COLOR_ARG_A_TFACTOR_COLOR |
					RADEON_COLOR_ARG_B_T0_ALPHA);
    OUT_ACCEL_REG(RADEON_PP_TXABLEND_0, RADEON_ALPHA_ARG_A_TFACTOR_ALPHA |
					RADEON_ALPHA_ARG_B_T0_ALPHA);
    OUT_ACCEL_REG(RADEON_SE_VTX_FMT, RADEON_SE_VTX_FMT_XY |
				     RADEON_SE_VTX_FMT_ST0);
    OUT_ACCEL_REG(RADEON_RB3D_BLENDCNTL,
	RadeonBlendOp[op].blend_cntl);
    FINISH_ACCEL();

    return TRUE;
}


static Bool
FUNC_NAME(R100SetupForCPUToScreenTexture) (
	ScrnInfoPtr	pScrn,
	int		op,
	int		texFormat,
	CARD8		*texPtr,
	int		texPitch,
	int		width,
	int		height,
	int		flags
)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    CARD32 format;
    ACCEL_PREAMBLE();

    if (op >= RadeonOpMax || RadeonBlendOp[op].blend_cntl == 0)
	return FALSE;
    
    if (!FUNC_NAME(R100SetupTexture)(pScrn, texFormat, texPtr, texPitch, width,
				     height, flags))
	return FALSE;

    if (pScrn->bitsPerPixel == 32)
	format = RADEON_COLOR_FORMAT_ARGB8888;
    else
	format = RADEON_COLOR_FORMAT_RGB565;
    
    BEGIN_ACCEL(7);
    OUT_ACCEL_REG(RADEON_RB3D_CNTL, format | RADEON_ALPHA_BLEND_ENABLE);
    OUT_ACCEL_REG(RADEON_RB3D_COLORPITCH, pScrn->displayWidth);
    OUT_ACCEL_REG(RADEON_PP_CNTL, RADEON_TEX_0_ENABLE |
				  RADEON_TEX_BLEND_0_ENABLE);
    if (texFormat != PICT_a8)
	OUT_ACCEL_REG(RADEON_PP_TXCBLEND_0, RADEON_COLOR_ARG_C_T0_COLOR);
    else
	OUT_ACCEL_REG(RADEON_PP_TXCBLEND_0, RADEON_COLOR_ARG_C_ZERO);
    OUT_ACCEL_REG(RADEON_PP_TXABLEND_0, RADEON_ALPHA_ARG_C_T0_ALPHA);
    OUT_ACCEL_REG(RADEON_SE_VTX_FMT, RADEON_SE_VTX_FMT_XY |
				     RADEON_SE_VTX_FMT_ST0);
    OUT_ACCEL_REG(RADEON_RB3D_BLENDCNTL,
	RadeonBlendOp[op].blend_cntl);
    FINISH_ACCEL();

    return TRUE;
}


static void
FUNC_NAME(R100SubsequentCPUToScreenTexture) (
	ScrnInfoPtr	pScrn,
	int		dstx,
	int		dsty,
	int		srcx,
	int		srcy,
	int		width,
	int		height
)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    int byteshift;
    CARD32 fboffset;
    float l, t, r, b, fl, fr, ft, fb;

    ACCEL_PREAMBLE();

    /* Note: we can't simply set up the 3D surface at the same location as the
     * front buffer, because the 2048x2048 limit on coordinates may be smaller
     * than the (MergedFB) screen.
     */ 
    byteshift = (pScrn->bitsPerPixel >> 4);
    fboffset = (info->fbLocation + pScrn->fbOffset +
		((pScrn->displayWidth * dsty + dstx) << byteshift)) & ~15;
    l = ((dstx << byteshift) % 16) >> byteshift;
    t = 0.0;
    r = width + l;
    b = height;
    fl = srcx;
    fr = srcx + width;
    ft = srcy;
    fb = srcy + height;

#ifdef ACCEL_CP
    BEGIN_RING(23);

    OUT_ACCEL_REG(RADEON_RB3D_COLOROFFSET, fboffset);

    OUT_RING(CP_PACKET3(RADEON_CP_PACKET3_3D_DRAW_IMMD, 17));
    /* RADEON_SE_VTX_FMT */
    OUT_RING(RADEON_CP_VC_FRMT_XY |
	     RADEON_CP_VC_FRMT_ST0);
    /* SE_VF_CNTL */
    OUT_RING(RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_FAN |
	     RADEON_CP_VC_CNTL_PRIM_WALK_RING |
	     RADEON_CP_VC_CNTL_MAOS_ENABLE |
	     RADEON_CP_VC_CNTL_VTX_FMT_RADEON_MODE |
	     (4 << RADEON_CP_VC_CNTL_NUM_SHIFT));

    OUT_RING(F_TO_DW(l));
    OUT_RING(F_TO_DW(t));
    OUT_RING(F_TO_DW(fl));
    OUT_RING(F_TO_DW(ft));

    OUT_RING(F_TO_DW(r));
    OUT_RING(F_TO_DW(t));
    OUT_RING(F_TO_DW(fr));
    OUT_RING(F_TO_DW(ft));

    OUT_RING(F_TO_DW(r));
    OUT_RING(F_TO_DW(b));
    OUT_RING(F_TO_DW(fr));
    OUT_RING(F_TO_DW(fb));

    OUT_RING(F_TO_DW(l));
    OUT_RING(F_TO_DW(b));
    OUT_RING(F_TO_DW(fl));
    OUT_RING(F_TO_DW(fb));

    OUT_ACCEL_REG(RADEON_WAIT_UNTIL, RADEON_WAIT_3D_IDLECLEAN);

    ADVANCE_RING();
#else
    BEGIN_ACCEL(19);
    
    OUT_ACCEL_REG(RADEON_RB3D_COLOROFFSET, fboffset);

    OUT_ACCEL_REG(RADEON_SE_VF_CNTL, RADEON_VF_PRIM_TYPE_TRIANGLE_FAN |
				     RADEON_VF_PRIM_WALK_DATA |
				     RADEON_VF_RADEON_MODE |
				     (4 << RADEON_VF_NUM_VERTICES_SHIFT));
	
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(l));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(t));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fl));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(ft));

    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(r));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(t));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fr));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(ft));

    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(r));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(b));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fr));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fb));

    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(l));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(b));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fl));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fb));

    OUT_ACCEL_REG(RADEON_WAIT_UNTIL, RADEON_WAIT_3D_IDLECLEAN);
    FINISH_ACCEL();
#endif

}

static Bool FUNC_NAME(R200SetupTexture)(ScrnInfoPtr pScrn,
	int format,
	CARD8 *src,
	int src_pitch,
	int width,
	int height,
	int flags)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    CARD8 *dst;
    CARD32 tex_size = 0, txformat;
    int dst_pitch, offset, size, i, tex_bytepp;
    ACCEL_PREAMBLE();

    if ((width > 2048) || (height > 2048))
	return FALSE;

    RadeonGetTextureFormat(format, &txformat, &tex_bytepp);

    dst_pitch = (width * tex_bytepp + 31) & ~31;
    size = dst_pitch * height;

    if (!AllocateLinear(pScrn, size))
	return FALSE;

    if (flags & XAA_RENDER_REPEAT) {
	txformat |= ATILog2(width) << R200_TXFORMAT_WIDTH_SHIFT;
	txformat |= ATILog2(height) << R200_TXFORMAT_HEIGHT_SHIFT;
    } else {
	tex_size = ((height - 1) << 16) | (width - 1);
	txformat |= RADEON_TXFORMAT_NON_POWER2;
    }

    offset = info->RenderTex->offset * pScrn->bitsPerPixel / 8;

    /* Upload texture to card.  Should use ImageWrite to avoid syncing. */
    i = height;
    dst = (CARD8*)(info->FB + offset);
    if (info->accel->NeedToSync)
	info->accel->Sync(pScrn);
    while(i--) {
	memcpy(dst, src, width * tex_bytepp);
	src += src_pitch;
	dst += dst_pitch;
    }

    BEGIN_ACCEL(6);
    OUT_ACCEL_REG(R200_PP_TXFORMAT_0, txformat);
    OUT_ACCEL_REG(R200_PP_TXFORMAT_X_0, 0);
    OUT_ACCEL_REG(R200_PP_TXSIZE_0, tex_size);
    OUT_ACCEL_REG(R200_PP_TXPITCH_0, dst_pitch - 32);
    OUT_ACCEL_REG(R200_PP_TXOFFSET_0, offset + info->fbLocation +
				      pScrn->fbOffset);
    OUT_ACCEL_REG(R200_PP_TXFILTER_0, R200_MAG_FILTER_NEAREST |
				      R200_MIN_FILTER_NEAREST |
				      R200_CLAMP_S_WRAP |
				      R200_CLAMP_T_WRAP);
    FINISH_ACCEL();

    return TRUE;
}

static Bool
FUNC_NAME(R200SetupForCPUToScreenAlphaTexture) (
	ScrnInfoPtr	pScrn,
	int		op,
	CARD16		red,
	CARD16		green,
	CARD16		blue,
	CARD16		alpha,
	int		alphaFormat,
	CARD8		*alphaPtr,
	int		alphaPitch,
	int		width,
	int		height,
	int		flags
) 
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    CARD32 format, srccolor;
    ACCEL_PREAMBLE();

    if (op >= RadeonOpMax || RadeonBlendOp[op].blend_cntl == 0)
	return FALSE;

    if (!FUNC_NAME(R200SetupTexture)(pScrn, alphaFormat, alphaPtr, alphaPitch,
				     width, height, flags))
	return FALSE;

    if (pScrn->bitsPerPixel == 32)
	format = RADEON_COLOR_FORMAT_ARGB8888;
    else
	format = RADEON_COLOR_FORMAT_RGB565;

    srccolor = ((alpha & 0xff00) << 16) | ((red & 0xff00) << 8) | (blue >> 8) |
	(green & 0xff00);

    BEGIN_ACCEL(11);
    OUT_ACCEL_REG(RADEON_RB3D_CNTL, format | RADEON_ALPHA_BLEND_ENABLE);
    OUT_ACCEL_REG(RADEON_RB3D_COLORPITCH, pScrn->displayWidth);
    OUT_ACCEL_REG(RADEON_PP_CNTL, RADEON_TEX_0_ENABLE |
				  RADEON_TEX_BLEND_0_ENABLE);
    OUT_ACCEL_REG(R200_PP_TFACTOR_0, srccolor);
    OUT_ACCEL_REG(R200_PP_TXCBLEND_0, R200_TXC_ARG_A_TFACTOR_COLOR |
				      R200_TXC_ARG_B_R0_ALPHA);
    OUT_ACCEL_REG(R200_PP_TXCBLEND2_0, R200_TXC_OUTPUT_REG_R0);
    OUT_ACCEL_REG(R200_PP_TXABLEND_0, R200_TXA_ARG_A_TFACTOR_ALPHA |
				      R200_TXA_ARG_B_R0_ALPHA);
    OUT_ACCEL_REG(R200_PP_TXABLEND2_0, R200_TXA_OUTPUT_REG_R0);
    OUT_ACCEL_REG(R200_SE_VTX_FMT_0, 0);
    OUT_ACCEL_REG(R200_SE_VTX_FMT_1, (2 << R200_VTX_TEX0_COMP_CNT_SHIFT));
    OUT_ACCEL_REG(RADEON_RB3D_BLENDCNTL,
	RadeonBlendOp[op].blend_cntl);
    FINISH_ACCEL();

    return TRUE;
}

static Bool
FUNC_NAME(R200SetupForCPUToScreenTexture) (
	ScrnInfoPtr	pScrn,
	int		op,
	int		texFormat,
	CARD8		*texPtr,
	int		texPitch,
	int		width,
	int		height,
	int		flags
)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    CARD32 format;
    ACCEL_PREAMBLE();

    if (op >= RadeonOpMax || RadeonBlendOp[op].blend_cntl == 0)
	return FALSE;

    if (!FUNC_NAME(R200SetupTexture)(pScrn, texFormat, texPtr, texPitch, width,
				     height, flags))
	return FALSE;

    if (pScrn->bitsPerPixel == 32)
	format = RADEON_COLOR_FORMAT_ARGB8888;
    else
	format = RADEON_COLOR_FORMAT_RGB565;

    BEGIN_ACCEL(10);
    OUT_ACCEL_REG(RADEON_RB3D_CNTL, format | RADEON_ALPHA_BLEND_ENABLE);
    OUT_ACCEL_REG(RADEON_RB3D_COLORPITCH, pScrn->displayWidth);
    OUT_ACCEL_REG(RADEON_PP_CNTL, RADEON_TEX_0_ENABLE |
				  RADEON_TEX_BLEND_0_ENABLE);
    if (texFormat != PICT_a8)
	OUT_ACCEL_REG(R200_PP_TXCBLEND_0, R200_TXC_ARG_C_R0_COLOR);
    else
	OUT_ACCEL_REG(R200_PP_TXCBLEND_0, R200_TXC_ARG_C_ZERO);
    OUT_ACCEL_REG(R200_PP_TXCBLEND2_0, R200_TXC_OUTPUT_REG_R0);
    OUT_ACCEL_REG(R200_PP_TXABLEND_0, R200_TXA_ARG_C_R0_ALPHA);
    OUT_ACCEL_REG(R200_PP_TXABLEND2_0, R200_TXA_OUTPUT_REG_R0);
    OUT_ACCEL_REG(R200_SE_VTX_FMT_0, 0);
    OUT_ACCEL_REG(R200_SE_VTX_FMT_1, (2 << R200_VTX_TEX0_COMP_CNT_SHIFT));
    OUT_ACCEL_REG(RADEON_RB3D_BLENDCNTL,
	RadeonBlendOp[op].blend_cntl);
    FINISH_ACCEL();

    return TRUE;
}

static void
FUNC_NAME(R200SubsequentCPUToScreenTexture) (
	ScrnInfoPtr	pScrn,
	int		dstx,
	int		dsty,
	int		srcx,
	int		srcy,
	int		width,
	int		height
)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    int byteshift;
    CARD32 fboffset;
    float l, t, r, b, fl, fr, ft, fb;
    ACCEL_PREAMBLE();

    /* Note: we can't simply set up the 3D surface at the same location as the
     * front buffer, because the 2048x2048 limit on coordinates may be smaller
     * than the (MergedFB) screen.
     */ 
    byteshift = (pScrn->bitsPerPixel >> 4);
    fboffset = (info->fbLocation + pScrn->fbOffset + ((pScrn->displayWidth *
	dsty + dstx) << byteshift)) & ~15;
    l = ((dstx << byteshift) % 16) >> byteshift;
    t = 0.0;
    r = width + l;
    b = height;
    fl = srcx;
    fr = srcx + width;
    ft = srcy;
    fb = srcy + height;

#ifdef ACCEL_CP
    BEGIN_RING(24);

    OUT_ACCEL_REG(RADEON_RB3D_COLOROFFSET, fboffset);

    OUT_RING(CP_PACKET3(R200_CP_PACKET3_3D_DRAW_IMMD_2, 16));
    /* RADEON_SE_VF_CNTL */
    OUT_RING(RADEON_CP_VC_CNTL_PRIM_TYPE_TRI_FAN |
	     RADEON_CP_VC_CNTL_PRIM_WALK_RING |
	     (4 << RADEON_CP_VC_CNTL_NUM_SHIFT));

    OUT_RING(F_TO_DW(l));
    OUT_RING(F_TO_DW(t));
    OUT_RING(F_TO_DW(fl));
    OUT_RING(F_TO_DW(ft));

    OUT_RING(F_TO_DW(r));
    OUT_RING(F_TO_DW(t));
    OUT_RING(F_TO_DW(fr));
    OUT_RING(F_TO_DW(ft));

    OUT_RING(F_TO_DW(r));
    OUT_RING(F_TO_DW(b));
    OUT_RING(F_TO_DW(fr));
    OUT_RING(F_TO_DW(fb));

    OUT_RING(F_TO_DW(l));
    OUT_RING(F_TO_DW(b));
    OUT_RING(F_TO_DW(fl));
    OUT_RING(F_TO_DW(fb));

    OUT_ACCEL_REG(RADEON_WAIT_UNTIL, RADEON_WAIT_3D_IDLECLEAN);

    ADVANCE_RING();
#else
    BEGIN_ACCEL(19);
    
    /* Note: we can't simply setup 3D surface at the same location as the front buffer,
       some apps may draw offscreen pictures out of the limitation of radeon 3D surface.
    */ 
    OUT_ACCEL_REG(RADEON_RB3D_COLOROFFSET, fboffset);

    OUT_ACCEL_REG(RADEON_SE_VF_CNTL, (RADEON_VF_PRIM_TYPE_QUAD_LIST |
				      RADEON_VF_PRIM_WALK_DATA |
				      4 << RADEON_VF_NUM_VERTICES_SHIFT));
	
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(l));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(t));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fl));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(ft));

    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(r));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(t));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fr));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(ft));

    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(r));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(b));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fr));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fb));

    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(l));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(b));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fl));
    OUT_ACCEL_REG(RADEON_SE_PORT_DATA0, F_TO_DW(fb));

    OUT_ACCEL_REG(RADEON_WAIT_UNTIL, RADEON_WAIT_3D_IDLECLEAN);

    FINISH_ACCEL();
#endif
}

#undef FUNC_NAME

