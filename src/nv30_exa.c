#include "nv_include.h"
//#include "nv30_vpinst.h"
#include "nv40_vpinst.h"
#include "nv30_fpinst.h"

#define VTX_POSITION	(1<<0)
#define VTX_COL0	(1<<3)
#define VTX_TEX0	(1<<8)
#define VTX_TEX1	(1<<9)
#define VTX_TEX_ALL     (0xFF<<8)

/* Some simple state caching.. probably doesn't make much difference.. */
#define CACHE_STATE
static struct {
	struct {
		CARD32 offset;
		int pitch;
		int bpp;
	} surface;
	struct {
		CARD32 offset;
		int pitch;
		int bpp;
		int repeat;
		int filter;
	} tex[16];

	int         vtx_enables;
	NVAllocRec *fragprog;
	Pixel       col0_pixel;
	int         rop;
} state;

/* Fragment programs */
static NVAllocRec *NV30_FP_COL0_WR;
static NVAllocRec *NV30_FP_TEX0_WR;
static NVAllocRec *NV30_FP_TEX0_IN_TEX1_WR;

#define FALLBACK(fmt, arg...) do { \
	NVDEBUG("FALLBACK %s: " fmt, __func__, ##arg); \
	return FALSE; \
} while(0)

#define VRAM_OFFSET(pNv, ar) (CARD32)((ar)->offset - (pNv)->VRAMPhysical)
#define TX_XFRM_BASE(unit) ((unit+1)*4)

static CARD32
getOffset(NVPtr pNv, DrawablePtr pDrawable)
{
	PixmapPtr pPixmap;
	CARD32 offset;

	if (pDrawable->type == DRAWABLE_WINDOW) {
		offset = pNv->FB->offset - pNv->VRAMPhysical;
	} else {
		pPixmap = (PixmapPtr)pDrawable;
		offset  = (CARD32)((unsigned long)pPixmap->devPrivate.ptr -
				(unsigned long)pNv->FB->map);
		offset += VRAM_OFFSET(pNv, pNv->FB);
	}

	return offset;
}

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static Bool
NV30_SetROP(NVPtr pNv, int rop, int planemask)
{
	static const CARD32 XROPtoGL[16] = {
		/* GL_CLEAR         */ 0x1500,
		/* GL_AND           */ 0x1501,
		/* GL_AND_REVERSE   */ 0x1502,
		/* GL_COPY          */ 0x1503,
		/* GL_AND_INVERTED  */ 0x1504,
		/* GL_NOOP          */ 0x1505,
		/* GL_XOR           */ 0x1506,
		/* GL_OR            */ 0x1507,
		/* GL_NOR           */ 0x1508,
		/* GL_EQUIV         */ 0x1509,
		/* GL_INVERT        */ 0x150A,
		/* GL_OR_REVERSE    */ 0x150B,
		/* GL_COPY_INVERTED */ 0x150C,
		/* GL_OR_INVERTED   */ 0x150D,
		/* GL_NAND          */ 0x150E,
		/* GL_SET           */ 0x150F,
	};

#ifdef CACHE_STATE
	if (state.rop == rop)
		return TRUE;
	state.rop = rop;
#endif

	if (planemask != 0xFFFFFFFF)
		FALLBACK("planemask\n");

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_LOGIC_OP_ENABLE, 2);
	NVDmaNext (pNv, rop == GXcopy ? 0 : 1);
	NVDmaNext (pNv, XROPtoGL[rop]);

	return TRUE;
}

static void
NV30_SetFragmentProg(NVPtr pNv, NVAllocRec *fp)
{
#ifdef CACHE_STATE
	if (state.fragprog == fp)
		return;
	state.fragprog = fp;
#endif

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_FP_ACTIVE_PROGRAM, 1);
	NVDmaNext (pNv, VRAM_OFFSET(pNv, fp)|1);
}

static void
NV30_Ortho2DTransform(NVPtr pNv, int id, float l, float r, float t, float b)
{
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_ID, 17);
	NVDmaNext (pNv, id);

	NVDmaFloat(pNv, (2.0/(r-l)));
	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, -((r+l)/(r-l)));

	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, (2.0/(t-b)));
	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, -((t+b)/(t-b)));

	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, 1.0);
	NVDmaFloat(pNv, 0.0);

	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, 1.0);
}

/* TX_FORMAT */
#define NV30_TCL_PRIMITIVE_3D_TX_MIPMAP_COUNT_SHIFT                           20
#define NV30_TCL_PRIMITIVE_3D_TX_RECTANGLE                               (1<<14)
#define NV30_TCL_PRIMITIVE_3D_TX_NPOT                                    (1<<13)
#define NV30_TCL_PRIMITIVE_3D_TX_FORMAT_SHIFT                                  8
#	define NV30_TCL_PRIMITIVE_3D_TX_FORMAT_L8                           0x01
#	define NV30_TCL_PRIMITIVE_3D_TX_FORMAT_R5G6B5                       0x04
#	define NV30_TCL_PRIMITIVE_3D_TX_FORMAT_A8R8G8B8                     0x05
#define NV30_TCL_PRIMITIVE_3D_TX_NCOMP_SHIFT                                   4
#define NV30_TCL_PRIMITIVE_3D_TX_CUBIC                                    (1<<2)
/* TX_WRAP */
#define NV30_TCL_PRIMITIVE_3D_TX_WRAP_S_SHIFT                                  0
#define NV30_TCL_PRIMITIVE_3D_TX_WRAP_T_SHIFT                                  8
#define NV30_TCL_PRIMITIVE_3D_TX_WRAP_R_SHIFT                                 16
#	define NV30_TCL_PRIMITIVE_3D_TX_REPEAT                              0x01
#	define NV30_TCL_PRIMITIVE_3D_TX_MIRRORED_REPEAT                     0x02
#	define NV30_TCL_PRIMITIVE_3D_TX_CLAMP_TO_EDGE                       0x03
#	define NV30_TCL_PRIMITIVE_3D_TX_CLAMP_TO_BORDER                     0x04
#	define NV30_TCL_PRIMITIVE_3D_TX_CLAMP                               0x05
/* TX_SWIZZLE */
#define NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S0_ZERO                            0x00
#define NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S0_ONE                             0x01
#define NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S0_S1                              0x02
#define NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S1_X                               0x03
#define NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S1_Y                               0x02
#define NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S1_Z                               0x01
#define NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S1_W                               0x00
#define NV30_TX_SWIZZLE(x1,y1,z1,w1,x2,y2,z2,w2) ( \
	(NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S0_##x1 << 14) | \
	(NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S0_##y1 << 12) | \
	(NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S0_##z1 << 10) | \
	(NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S0_##w1 <<  8) | \
	(NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S1_##x2 <<  6) | \
	(NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S1_##y2 <<  4) | \
	(NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S1_##z2 <<  2) | \
	(NV30_TCL_PRIMITIVE_3D_TX_SWIZZLE_S1_##w2 <<  0))
/* TX_FILTER */
#define NV30_TCL_PRIMITIVE_3D_TX_MIN_FILTER_SHIFT                             16
#define NV30_TCL_PRIMITIVE_3D_TX_MAG_FILTER_SHIFT                             24
#	define NV30_TCL_PRIMITIVE_3D_TX_FILTER_NEAREST                      0x01
#	define NV30_TCL_PRIMITIVE_3D_TX_FILTER_LINEAR                       0x02
/* TX_DEPTH */
#define NV30_TCL_PRIMITIVE_3D_TX_DEPTH_NPOT_PITCH_SHIFT                        0
#define NV30_TCL_PRIMITIVE_3D_TX_DEPTH_SHIFT                                  20


static Bool
NV30_TextureFromDrawable(NVPtr pNv, DrawablePtr pDrawable, int unit,
			 int repeat, int filter)
{
	unsigned int offset, pitch;
	unsigned int fmt, swz, unk;

	offset = getOffset(pNv, pDrawable);
	pitch  = exaGetPixmapPitch((PixmapPtr)pDrawable);

#ifdef CACHE_STATE
	if (state.tex[unit].offset == offset &&
			state.tex[unit].pitch == pitch &&
			state.tex[unit].bpp == pDrawable->bitsPerPixel &&
			state.tex[unit].filter == filter &&
			state.tex[unit].repeat == repeat) {
		NVDmaStart(pNv, NvSub3D,
				NV30_TCL_PRIMITIVE_3D_TX_ENABLE_UNIT(unit), 1);
		NVDmaNext(pNv, 0x80000000);
		return TRUE;
	}
	state.tex[unit].offset = offset;
	state.tex[unit].pitch  = pitch;
	state.tex[unit].bpp    = pDrawable->bitsPerPixel;
	state.tex[unit].repeat = repeat;
	state.tex[unit].filter = filter;
#endif

	/* (some of?) these are probably wrong */
	switch (pDrawable->bitsPerPixel) {
	case 32:
		fmt = NV30_TCL_PRIMITIVE_3D_TX_FORMAT_A8R8G8B8;
		swz = NV30_TX_SWIZZLE(S1, S1, S1, S1, X, Y, Z, W);
		unk = 0x00000000;
		break;
	case 24:
		fmt = NV30_TCL_PRIMITIVE_3D_TX_FORMAT_A8R8G8B8;
		swz = NV30_TX_SWIZZLE(S1, S1, S1, ONE, X, Y, Z, W);
		unk = 0xFF000000;
		break;
	case 16:
		fmt = NV30_TCL_PRIMITIVE_3D_TX_FORMAT_R5G6B5;
		swz = NV30_TX_SWIZZLE(S1, S1, S1, ONE, X, Y, Z, W);
		unk = 0xFF000000;
		break;
	default:
		fmt = NV30_TCL_PRIMITIVE_3D_TX_FORMAT_L8;
		swz = NV30_TX_SWIZZLE(ZERO, ZERO, ZERO, S1, X, X, X, X);
		unk = 0x00000000;
		break;
	}

	switch (repeat) {
	case RepeatNormal:
		repeat = NV30_TCL_PRIMITIVE_3D_TX_REPEAT;
		break;
	case RepeatNone:
	default:
		repeat = NV30_TCL_PRIMITIVE_3D_TX_CLAMP;
		break;
	}

	switch (filter) {
	case PictFilterBilinear:
		filter = NV30_TCL_PRIMITIVE_3D_TX_FILTER_LINEAR;
		break;
	case PictFilterNearest:
	default:
		filter = NV30_TCL_PRIMITIVE_3D_TX_FILTER_NEAREST;
		break;
	}

	/* Setup texture unit */
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_TX_ADDRESS_UNIT(unit),8);
	NVDmaNext(pNv, offset);
	NVDmaNext(pNv, (fmt << NV30_TCL_PRIMITIVE_3D_TX_FORMAT_SHIFT) |
			(0   << NV30_TCL_PRIMITIVE_3D_TX_MIPMAP_COUNT_SHIFT) |
			(2   << NV30_TCL_PRIMITIVE_3D_TX_NCOMP_SHIFT) |
			NV30_TCL_PRIMITIVE_3D_TX_NPOT |
			(1<<0)|(3<<15) /* engine locks without this */
//			| (1<<3)
		 );
	NVDmaNext(pNv, (repeat << NV30_TCL_PRIMITIVE_3D_TX_WRAP_S_SHIFT) |
			(repeat << NV30_TCL_PRIMITIVE_3D_TX_WRAP_T_SHIFT) |
			(repeat << NV30_TCL_PRIMITIVE_3D_TX_WRAP_R_SHIFT)
		 );
	NVDmaNext(pNv, 0x80000000); /* unk - enable? */
	NVDmaNext(pNv, swz);
	NVDmaNext(pNv, (filter << NV30_TCL_PRIMITIVE_3D_TX_MIN_FILTER_SHIFT) |
			(filter << NV30_TCL_PRIMITIVE_3D_TX_MAG_FILTER_SHIFT) |
			0x3fd6 /* engine locks without this */
		 );
	NVDmaNext(pNv, (pDrawable->width<<16) | pDrawable->height);
	NVDmaNext(pNv, unk);
	
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_TX_DEPTH_UNIT(unit), 1);
	NVDmaNext(pNv,	(1 << NV30_TCL_PRIMITIVE_3D_TX_DEPTH_SHIFT) | pitch);

	/* Update texcoord scaling */
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_ID, 5);
	NVDmaNext (pNv, TX_XFRM_BASE(unit));
	NVDmaFloat(pNv, 1.0/(float)(pDrawable->width-1));
	NVDmaFloat(pNv, 1.0/(float)(pDrawable->height-1));
	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, 0.0);

	return TRUE;
}

static Bool
NV30_InitSurface(NVPtr pNv, PixmapPtr pPixmap)
{
	CARD32 offset;
	int width, height, pitch, fmt;

	width  = pPixmap->drawable.width;
	height = pPixmap->drawable.height;
	pitch  = exaGetPixmapPitch(pPixmap);
	offset = getOffset(pNv, (DrawablePtr)pPixmap);

#ifdef CACHE_STATE
	if (state.surface.offset == offset &&
			state.surface.pitch == pitch &&
			state.surface.bpp == pPixmap->drawable.bitsPerPixel) {
		return TRUE;
	}
	state.surface.offset = offset;
	state.surface.pitch  = pitch;
	state.surface.bpp    = pPixmap->drawable.bitsPerPixel;
#endif

	/* FIXME FIXME FIXME FIXME FIXME FIXME FIXME*/
	/* Get the picture? :P */
	switch (pPixmap->drawable.bitsPerPixel) {
	case 32:
		fmt = 0x148;
		break;
	case 24:
		fmt = 0x145;
		break;
	case 16:
		fmt = 0x143;
		break;
	case 8:
	default:
		FALLBACK("Unsupported bpp: %d\n",
				pPixmap->drawable.bitsPerPixel);
	}

	NVDmaStart(pNv, NvSub3D,
			NV30_TCL_PRIMITIVE_3D_VIEWPORT_COLOR_BUFFER_DIM0, 5);
	NVDmaNext (pNv, width <<16);
	NVDmaNext (pNv, height<<16);
	NVDmaNext (pNv, fmt);
	NVDmaNext (pNv, pitch);
	NVDmaNext (pNv, offset);

	NVDmaStart(pNv, NvSub3D,
			NV30_TCL_PRIMITIVE_3D_VIEWPORT_COLOR_BUFFER_OFS0, 2);
	NVDmaNext (pNv, (width-1)<<16);
	NVDmaNext (pNv, (height-1)<<16);

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_SCISSOR_WIDTH_XPOS, 2);
	NVDmaNext (pNv, width <<16);
	NVDmaNext (pNv, height<<16);

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_VIEWPORT_DIMS_0, 2);
	NVDmaNext (pNv, width <<16);
	NVDmaNext (pNv, height<<16);

	/* viewport transform */
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_VIEWPORT_XFRM_OX, 8);
	NVDmaFloat(pNv, width /2.0);   /* center x */
	NVDmaFloat(pNv, height/2.0);   /* center y */
	NVDmaFloat(pNv, ((-1.0+1.0)/2.0));    /* (n+f)/2 */
	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, width /2.0);   /* width/2 */
	NVDmaFloat(pNv, -height/2.0);   /* height/2 */
	NVDmaFloat(pNv, ((1.0-(-1.0))/2.0));; /* (f-n)/2 */
	NVDmaFloat(pNv, 0.0);

	/* MVP transform */
	NV30_Ortho2DTransform(pNv, 0,
			0, pPixmap->drawable.width - 1,
			0, pPixmap->drawable.height - 1);

	return TRUE;
}

static void
NV30_InitVertexProg(NVPtr pNv,
		    PixmapPtr pPixmap,
		    CARD32    vtx_enables)
{
	NV40VP_LOCALS(NvSub3D);
	int attr, unit;

#ifdef CACHE_STATE
	if (state.vtx_enables == vtx_enables)
		return;
#endif
	state.vtx_enables = vtx_enables;

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_VP_UPLOAD_FROM_ID, 1);
	NVDmaNext(pNv, 0);

	/* transform vertex.position */
	NV40VP_ARITH_INST_SET_DEFAULTS;
	NV40VP_INST_S0(DP4, RESULT, NV40_VP_INST_DEST_POS, 1, 0, 0, 0);
	NV40VP_SET_SOURCE_CONST(0, 0x00, NV40VP_SWIZZLE(X,Y,Z,W), 0, 0, 0);
	NV40VP_SET_SOURCE_INPUT(1, NV40_VP_INST_IN_POS,
				NV40VP_SWIZZLE(X,Y,Z,W), 0, 0, 0);
	NV40VP_INST_EMIT;
	NV40VP_INST_S0(DP4, RESULT, NV40_VP_INST_DEST_POS, 0, 1, 0, 0);
	NV40VP_SET_SOURCE_CONST(0, 0x01, NV40VP_SWIZZLE(X,Y,Z,W), 0, 0, 0);
	NV40VP_INST_EMIT;
	NV40VP_INST_S0(DP4, RESULT, NV40_VP_INST_DEST_POS, 0, 0, 1, 0);
	NV40VP_SET_SOURCE_CONST(0, 0x02, NV40VP_SWIZZLE(X,Y,Z,W), 0, 0, 0);
	NV40VP_INST_EMIT;
	NV40VP_INST_S0(DP4, RESULT, NV40_VP_INST_DEST_POS, 0, 0, 0, 1);
	NV40VP_SET_SOURCE_CONST(0, 0x03, NV40VP_SWIZZLE(X,Y,Z,W), 0, 0, 0);
	NV40VP_INST_EMIT;
	/* scale texcoords */
	if (vtx_enables & VTX_TEX_ALL) {
		NV40VP_ARITH_INST_SET_DEFAULTS;
		for (unit=0;unit<8;unit++) {
			if (!(vtx_enables & (1<<(8+unit)))) continue;
			NV40VP_INST_S0(MUL, RESULT, NV40_VP_INST_DEST_TC(unit),
					1, 1, 0, 0);
			NV40VP_SET_SOURCE_CONST(0, TX_XFRM_BASE(unit)+0,
					NV40VP_SWIZZLE(X,Y,X,X), 0, 0, 0);
			NV40VP_SET_SOURCE_INPUT(1, NV40_VP_INST_IN_TC(unit),
					NV40VP_SWIZZLE(X,Y,X,X), 0, 0, 0);
			NV40VP_INST_EMIT;
		}
	}
	/* pass through primary color */
	NV40VP_ARITH_INST_SET_DEFAULTS;
	NV40VP_SET_SOURCE_INPUT(0, NV40_VP_INST_IN_COL0,
				NV40VP_SWIZZLE(X,Y,Z,W), 0, 0, 0);
	NV40VP_INST_S0(MOV, RESULT, NV40_VP_INST_DEST_COL0, 1, 1, 1, 1);
	NV40VP_SET_LAST_INST;
	NV40VP_INST_EMIT;

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_VP_PROGRAM_START_ID, 1);
	NVDmaNext(pNv, 0);

	/*FIXME hackish.. */
	if (vtx_enables & VTX_TEX1) {
		NVDmaStart(pNv, NvSub3D, 0x1ff0, 2);
		NVDmaNext (pNv, 0x309);
		NVDmaNext (pNv, 0xC001);
	} else if (vtx_enables & VTX_TEX0) {
		NVDmaStart(pNv, NvSub3D, 0x1ff0, 2);
		NVDmaNext (pNv, 0x109);
		NVDmaNext (pNv, 0x4001);
	} else {
		NVDmaStart(pNv, NvSub3D, 0x1ff0, 2);
		NVDmaNext (pNv, 0x009);
		NVDmaNext (pNv, 0x0001);
	}

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_DO_VERTICES, 1);
	NVDmaNext (pNv, 0);

	NVDmaStart(pNv, NvSub3D, 0x1740, 16);
	for (attr=0;attr<16;attr++) {
		if (vtx_enables & (1<<attr)) {
			NVDmaNext(pNv, 0x22); /* 2 component float */
		} else {
			NVDmaNext(pNv, 0x02); /* 0 component float */
		}
	}
}

/*******************************************************************************
 * EXA hooks
 ******************************************************************************/
static Bool
NV30EXAPrepareSolid(PixmapPtr pDstPixmap,
		    int       alu,
		    Pixel     planemask,
		    Pixel     fg)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	float r,g,b,a;

	if (!NV30_InitSurface(pNv, pDstPixmap))
		FALLBACK("pDstPixmap\n");
	NV30_SetFragmentProg(pNv, NV30_FP_COL0_WR);
	NV30_InitVertexProg (pNv, pDstPixmap, VTX_POSITION);
	if (!NV30_SetROP    (pNv, alu, planemask))
		FALLBACK("ROP\n");

#ifdef CACHE_STATE
	if (state.col0_pixel != fg) {
#endif
		r = (float)((fg & 0x00FF0000)>>16)/255.0;
		g = (float)((fg & 0x0000FF00)>> 8)/255.0;
		b = (float)((fg & 0x000000FF)>> 0)/255.0;
		a = (float)((fg & 0xFF000000)>>24)/255.0;

		NVDmaStart(pNv, NvSub3D,
				NV30_TCL_PRIMITIVE_3D_VTX_ATTR_4X(3), 4);
		NVDmaFloat(pNv, r);
		NVDmaFloat(pNv, g);
		NVDmaFloat(pNv, b);
		NVDmaFloat(pNv, a);
#ifdef CACHE_STATE
		state.col0_pixel = fg;
	}
#endif

	pNv->DMAKickoffCallback = NVDmaKickoffCallback;
	return TRUE;
}

static void
NV30EXASolid(PixmapPtr pDstPixmap, int x1, int y1, int x2, int y2)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_BEGIN_END, 1);
	NVDmaNext(pNv, 7+1); /* GL_QUADS */
	NVDmaStart_NonInc(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_VERTEX_DATA, 8);
		NVDmaFloat(pNv, x1); NVDmaFloat(pNv, y1);
		NVDmaFloat(pNv, x1); NVDmaFloat(pNv, y2);
		NVDmaFloat(pNv, x2); NVDmaFloat(pNv, y2);
		NVDmaFloat(pNv, x2); NVDmaFloat(pNv, y1);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_BEGIN_END, 1);
	NVDmaNext(pNv, 0);
}

static void
NV30EXADoneSolid(PixmapPtr pDstPixmap)
{
}

static Bool
NV30EXAPrepareCopy(PixmapPtr pSrcPixmap,
		   PixmapPtr pDstPixmap,
		   int dirx, int diry,
		   int alu,
		   Pixel planemask)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	/* Slight corruption using same surface as texture and color buffer */
	if (pSrcPixmap == pDstPixmap)
		FALLBACK("pSrc==pDst\n");

	if (!NV30_InitSurface   (pNv, pDstPixmap))
		FALLBACK("pDstPixmap\n");
	NV30_SetFragmentProg    (pNv, NV30_FP_TEX0_WR);
	NV30_TextureFromDrawable(pNv, &pSrcPixmap->drawable, 0,
				 RepeatNone, PictFilterNearest);
	NV30_InitVertexProg     (pNv, pDstPixmap, VTX_TEX0|VTX_POSITION);
	if (!NV30_SetROP        (pNv, alu, planemask))
		FALLBACK("ROP\n");

	pNv->DMAKickoffCallback = NVDmaKickoffCallback;
	return TRUE;
}

static void
NV30EXACopy(PixmapPtr pDstPixmap,
	    int srcX, int srcY,
	    int dstX, int dstY,
	    int width,
	    int height)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_BEGIN_END, 1);
	NVDmaNext (pNv, 7+1); /* GL_QUADS */
	NVDmaStart_NonInc(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_VERTEX_DATA, 16);
		NVDmaFloat(pNv, dstX);		NVDmaFloat(pNv, dstY);
		NVDmaFloat(pNv, srcX);		NVDmaFloat(pNv, srcY);
		NVDmaFloat(pNv, dstX+width-1);	NVDmaFloat(pNv, dstY);
		NVDmaFloat(pNv, srcX+width-1);	NVDmaFloat(pNv, srcY);
		NVDmaFloat(pNv, dstX+width-1);	NVDmaFloat(pNv, dstY+height-1);
		NVDmaFloat(pNv, srcX+width-1);	NVDmaFloat(pNv, srcY+height-1);
		NVDmaFloat(pNv, dstX);		NVDmaFloat(pNv, dstY+height-1);
		NVDmaFloat(pNv, srcX);		NVDmaFloat(pNv, srcY+height-1);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_BEGIN_END, 1);
	NVDmaNext(pNv, 0);
}

static void
NV30EXADoneCopy(PixmapPtr pDstPixmap)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_TX_ENABLE_UNIT(0), 1);
	NVDmaNext (pNv, 0);
}


#define NV30_TCL_PRIMITIVE_3D_BF_ZERO                                     0x0000
#define NV30_TCL_PRIMITIVE_3D_BF_ONE                                      0x0001
#define NV30_TCL_PRIMITIVE_3D_BF_SRC_COLOR                                0x0300
#define NV30_TCL_PRIMITIVE_3D_BF_ONE_MINUS_SRC_COLOR                      0x0301
#define NV30_TCL_PRIMITIVE_3D_BF_SRC_ALPHA                                0x0302
#define NV30_TCL_PRIMITIVE_3D_BF_ONE_MINUS_SRC_ALPHA                      0x0303
#define NV30_TCL_PRIMITIVE_3D_BF_DST_ALPHA                                0x0304
#define NV30_TCL_PRIMITIVE_3D_BF_ONE_MINUS_DST_ALPHA                      0x0305
#define NV30_TCL_PRIMITIVE_3D_BF_DST_COLOR                                0x0306
#define NV30_TCL_PRIMITIVE_3D_BF_ONE_MINUS_DST_COLOR                      0x0307
#define NV30_TCL_PRIMITIVE_3D_BF_ALPHA_SATURATE                           0x0308
#define _(cb,ab){NV30_TCL_PRIMITIVE_3D_BF_##cb,NV30_TCL_PRIMITIVE_3D_BF_##ab },
static struct {
	CARD32 sblend;
	CARD32 dblend;
} NV30BlendOp[] = {
	/* Clear       */ _(ZERO               , ZERO)
	/* Src         */ _(ONE                , ZERO)
	/* Dst         */ _(ZERO               , ONE)
	/* Over        */ _(ONE                , ONE_MINUS_SRC_ALPHA)
	/* OverReverse */ _(ONE_MINUS_DST_ALPHA, ONE)
	/* In          */ _(DST_ALPHA          , ZERO)
	/* InReverse   */ _(ZERO               , SRC_ALPHA)
	/* Out         */ _(ONE_MINUS_DST_ALPHA, ZERO)
	/* OutReverse  */ _(ZERO               , ONE_MINUS_SRC_ALPHA)
	/* Atop        */ _(DST_ALPHA          , ONE_MINUS_SRC_ALPHA)
	/* AtopReverse */ _(ONE_MINUS_DST_ALPHA, SRC_ALPHA)
	/* Xor         */ _(ONE_MINUS_DST_ALPHA, ONE_MINUS_SRC_ALPHA)
	/* Add         */ _(ONE                , ONE)
};
#define NV30_MAX_BLEND_OP (sizeof(NV30BlendOp) / sizeof(NV30BlendOp[0]))
#undef _

static Bool
NV30EXACheckCompositeTexture(PicturePtr pPicture)
{
	if (pPicture->pDrawable->width > 4096 ||
		pPicture->pDrawable->height > 4096)
		FALLBACK("picture too large (%dx%d)\n",
				pPicture->pDrawable->width,
				pPicture->pDrawable->height);

	if (pPicture->repeat && pPicture->repeatType != RepeatNormal)
		FALLBACK("repeat mode %d unsupported\n", pPicture->repeatType);

	if (pPicture->filter != PictFilterNearest &&
		pPicture->filter != PictFilterBilinear)
		FALLBACK("filtering mode %d unsupported\n", pPicture->filter);

	if (pPicture->transform)
		FALLBACK("transforms not supported yet\n");

	return TRUE;
}

static Bool
NV30EXACheckComposite(int op, PicturePtr pSrcPicture,
			      PicturePtr pMaskPicture,
			      PicturePtr pDstPicture)
{
	if (op >= NV30_MAX_BLEND_OP)
		FALLBACK("Unsupported composite op 0x%x\n", op);

	//FIXME: not true in *all* cases
	if (pMaskPicture && pMaskPicture->componentAlpha)
			FALLBACK("Component alpha not supported\n");

	if (!NV30EXACheckCompositeTexture(pSrcPicture))
		FALLBACK("src picture\n");
	if (pMaskPicture && !NV30EXACheckCompositeTexture(pMaskPicture))
		FALLBACK("mask picture\n");
	if (!NV30EXACheckCompositeTexture(pDstPicture))
		FALLBACK("dest picture\n");

	return TRUE;
}

static Bool
NV30EXAPrepareComposite(int op,	PicturePtr pSrcPicture,
				PicturePtr pMaskPicture,
				PicturePtr pDstPicture,
				PixmapPtr  pSrcPixmap,
				PixmapPtr  pMaskPixmap,
				PixmapPtr  pDstPixmap)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	if (!NV30_InitSurface(pNv, pDstPixmap))
		FALLBACK("pDstPixmap\n");
	NV30_SetROP(pNv, GXcopy, 0xFFFFFFFF);

	if (!pMaskPixmap || !pMaskPicture->componentAlpha) {
		NVDmaStart(pNv, NvSub3D,
				NV30_TCL_PRIMITIVE_3D_BLEND_FUNC_ENABLE, 5);
		NVDmaNext (pNv, 1);
		NVDmaNext (pNv, (NV30BlendOp[op].sblend<<16) |
				NV30BlendOp[op].sblend);
		NVDmaNext (pNv, (NV30BlendOp[op].dblend<<16) |
				NV30BlendOp[op].dblend);
		NVDmaNext (pNv, 0 /* blend colour */);
		NVDmaNext (pNv, (0x8006<<16)|0x8006); /* eqn: a add, c add */
	}

	if (!NV30_TextureFromDrawable(pNv, &pSrcPixmap->drawable, 0,
				pSrcPicture->repeat, pSrcPicture->filter))
		FALLBACK("source texture\n");

	if (pMaskPixmap) {
		if (!NV30_TextureFromDrawable(pNv, &pMaskPixmap->drawable, 1,
					pMaskPicture->repeat,
					pMaskPicture->filter))
			FALLBACK("mask texture\n");
	}

	if (pMaskPixmap) {
		NV30_SetFragmentProg(pNv, NV30_FP_TEX0_IN_TEX1_WR);
		NV30_InitVertexProg (pNv, pDstPixmap,
				VTX_POSITION|VTX_TEX0|VTX_TEX1);
	} else {
		NV30_SetFragmentProg(pNv, NV30_FP_TEX0_WR);
		NV30_InitVertexProg (pNv, pDstPixmap, VTX_POSITION|VTX_TEX0);
	}

	return TRUE;
}

static void
NV30EXAComposite(PixmapPtr pDstPixmap,
		 int srcX, int srcY,
		 int maskX, int maskY,
		 int dstX, int dstY,
		 int width, int height)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	int srcXend, srcYend, maskXend, maskYend, dstXend, dstYend;

	srcXend = srcX + width - 1;
	srcYend = srcY + height - 1;
	maskXend = maskX + width - 1;
	maskYend = maskY + height - 1;
	dstXend = dstX + width - 1;
	dstYend = dstY + height - 1;

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_BEGIN_END, 1);
	NVDmaNext (pNv, 7+1); /* GL_QUADS */
	NVDmaStart_NonInc(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_VERTEX_DATA,
			state.vtx_enables & VTX_TEX1 ? 24 : 16);
		NVDmaFloat(pNv, dstX); NVDmaFloat(pNv, dstY);
		NVDmaFloat(pNv, srcX); NVDmaFloat(pNv, srcY);
		if (state.vtx_enables & VTX_TEX1) {
			NVDmaFloat(pNv, maskX);	NVDmaFloat(pNv, maskY);
		}
		NVDmaFloat(pNv, dstX); NVDmaFloat(pNv, dstYend);
		NVDmaFloat(pNv, srcX); NVDmaFloat(pNv, srcYend);
		if (state.vtx_enables & VTX_TEX1) {
			NVDmaFloat(pNv, maskX); NVDmaFloat(pNv, maskYend);
		}
		NVDmaFloat(pNv, dstXend); NVDmaFloat(pNv, dstYend);
		NVDmaFloat(pNv, srcXend); NVDmaFloat(pNv, srcYend);
		if (state.vtx_enables & VTX_TEX1) {
			NVDmaFloat(pNv, maskXend); NVDmaFloat(pNv, maskYend);
		}
		NVDmaFloat(pNv, dstXend); NVDmaFloat(pNv, dstY);
		NVDmaFloat(pNv, srcXend); NVDmaFloat(pNv, srcY);
		if (state.vtx_enables & VTX_TEX1) {
			NVDmaFloat(pNv, maskXend); NVDmaFloat(pNv, maskY);
		}
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_BEGIN_END, 1);
	NVDmaNext(pNv, 0);
}

static void
NV30EXADoneComposite(PixmapPtr pDstPixmap)
{
	ScrnInfoPtr pScrn = xf86Screens[pDstPixmap->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_BLEND_FUNC_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_TX_ENABLE_UNIT(0), 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_TX_ENABLE_UNIT(1), 1);
	NVDmaNext (pNv, 0);
}

Bool
NV30EXAPreInit(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	NV30FP_LOCALS;

	/*
	 * MOV result.color, fragment.color
	 */
	NV30_FP_COL0_WR = NVAllocateMemory(pNv, NOUVEAU_MEM_FB, 2*4);
	if (!NV30_FP_COL0_WR)
		goto fail;
	NV30FP_SETBUF(NV30_FP_COL0_WR->map);
	NV30FP_ARITH_INST_SET_DEFAULTS;
	NV30FP_ARITH(MOV, RESULT, 0, 1, 1, 1, 1);
	NV30FP_SOURCE_INPUT(0, NV30_FP_OP_INPUT_SRC_COL0, X, Y, Z, W, 0, 0, 0);
	NV30FP_LAST_INST;

	/*
	 * TXP result.color, fragment.texcoord[0].xy, texture[0], 2D
	 */
	NV30_FP_TEX0_WR = NVAllocateMemory(pNv, NOUVEAU_MEM_FB, 2*4);
	if (!NV30_FP_TEX0_WR)
		goto fail;
	NV30FP_SETBUF(NV30_FP_TEX0_WR->map);
	NV30FP_ARITH_INST_SET_DEFAULTS;
	NV30FP_TEX(TEX, RESULT, 0, 1, 1, 1, 1, 0);
	NV30FP_SOURCE_INPUT(0, NV30_FP_OP_INPUT_SRC_TC(0), X, Y, X, X, 0, 0, 0);
	NV30FP_LAST_INST;

	/* (src IN mask)
	 * TXP t0  , fragment.texcoord[0].xy, texture[0], 2D
	 * TXP t1.w, fragment.texcoord[1].xy, texture[1], 2D
	 * MUL result.color, t0, t1.w
	 */
	NV30_FP_TEX0_IN_TEX1_WR = NVAllocateMemory(pNv, NOUVEAU_MEM_FB, 4*4);
	if (!NV30_FP_TEX0_IN_TEX1_WR)
		goto fail;
	NV30FP_SETBUF(NV30_FP_TEX0_IN_TEX1_WR->map);
	NV30FP_ARITH_INST_SET_DEFAULTS;
	NV30FP_TEX(TEX, TEMP, 0, 1, 1, 1, 1, 0);
	NV30FP_SOURCE_INPUT(0, NV30_FP_OP_INPUT_SRC_TC(0), X, Y, X, X, 0, 0, 0);
	NV30FP_NEXT;
	NV30FP_TEX(TEX, TEMP, 1, 0, 0, 0, 1, 1);
	NV30FP_SOURCE_INPUT(0, NV30_FP_OP_INPUT_SRC_TC(1), X, Y, X, X, 0, 0, 0);
	NV30FP_NEXT;
	NV30FP_ARITH(MUL, RESULT, 0, 1, 1, 1, 1);
	NV30FP_SOURCE_TEMP(0, 0, X, Y, Z, W, 0, 0);
	NV30FP_SOURCE_TEMP(1, 1, W, W, W, W, 0, 0);
	NV30FP_LAST_INST;

	return TRUE;
fail:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "NV30_EXA pre-init failed\n");

	if (NV30_FP_COL0_WR) NVFreeMemory(pNv, NV30_FP_COL0_WR);
	if (NV30_FP_TEX0_WR) NVFreeMemory(pNv, NV30_FP_COL0_WR);
	if (NV30_FP_TEX0_IN_TEX1_WR) NVFreeMemory(pNv, NV30_FP_COL0_WR);

	return FALSE;
}

void
NV30EXAInstallHooks(NVPtr pNv) 
{
	pNv->EXADriverPtr->PrepareCopy        = NV30EXAPrepareCopy;
	pNv->EXADriverPtr->Copy               = NV30EXACopy;
	pNv->EXADriverPtr->DoneCopy           = NV30EXADoneCopy;

	pNv->EXADriverPtr->PrepareSolid       = NV30EXAPrepareSolid;
	pNv->EXADriverPtr->Solid              = NV30EXASolid;
	pNv->EXADriverPtr->DoneSolid          = NV30EXADoneSolid;

	pNv->EXADriverPtr->CheckComposite     = NV30EXACheckComposite;
	pNv->EXADriverPtr->PrepareComposite   = NV30EXAPrepareComposite;
	pNv->EXADriverPtr->Composite          = NV30EXAComposite;
	pNv->EXADriverPtr->DoneComposite      = NV30EXADoneComposite;
}

void
NV30EXAResetGraphics(NVPtr pNv)
{
	int i;

	memset(&state, 0xFF, sizeof(state)); /* invalidate state cache */

	/*FIXME: We will need more here most likely, all that's left is what's
	 *       needed for this to work after the binary driver..
	 */

	/* All black rendering without */
	NVDmaStart(pNv, NvSub3D, 0x184, 2);
	NVDmaNext (pNv, NvDmaFB);
	NVDmaNext (pNv, NvDmaFB /*NvDmaAGP*/);
	/* Can't recall.. */
	NVDmaStart(pNv, NvSub3D, 0x1AC, 1);
	NVDmaNext (pNv, NvDmaFB);
	/* No rendering without, DMA objs for colour buffer? */
	NVDmaStart(pNv, NvSub3D, 0x194, 2);
	NVDmaNext (pNv, NvDmaFB);
	NVDmaNext (pNv, NvDmaFB);
	/* Enabled buffers? bits 0-3 = colour 0-3, 4 = depth? */
	NVDmaStart(pNv, NvSub3D, 0x220, 1);
	NVDmaNext (pNv, 1);
	/* Bad rendering after nvidia without this.. */
	NVDmaStart(pNv, NvSub3D, 0x1fc8, 2);
	NVDmaNext (pNv, 0xedcba987);
	NVDmaNext (pNv, 0x00000021);

	/* Attempt to setup a known state.. Probably missing a heap of
	 * stuff here..
	 */
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_STENCIL_FRONT_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_STENCIL_BACK_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_ALPHA_FUNC_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_DEPTH_WRITE_ENABLE, 2);
	NVDmaNext (pNv, 0); /* wr disable */
	NVDmaNext (pNv, 0); /* test disable */
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_COLOR_MASK, 1);
	NVDmaNext (pNv, 0x01010101); /* TR,TR,TR,TR */
	NVDmaStart(pNv, NvSub3D, NV40_TCL_PRIMITIVE_3D_COLOR_MASK_BUFFER123, 1);
	NVDmaNext (pNv, 0x0000fff0);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_CULL_FACE_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_BLEND_FUNC_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_LOGIC_OP_ENABLE, 1);
	NVDmaNext (pNv, 0);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_DITHER_ENABLE, 1);
	NVDmaNext (pNv, 1);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_SHADE_MODEL, 1);
	NVDmaNext (pNv, 0x1d01); /* GL_SMOOTH */
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_POLYGON_OFFSET_FACTOR,2);
	NVDmaFloat(pNv, 0.0);
	NVDmaFloat(pNv, 0.0);
	NVDmaStart(pNv, NvSub3D, NV30_TCL_PRIMITIVE_3D_POLYGON_MODE_FRONT, 2);
	NVDmaNext (pNv, 0x1b02); /* FRONT = GL_FILL */
	NVDmaNext (pNv, 0x1b02); /* BACK  = GL_FILL */
	/* - Disable texture units
	 * - Set fragprog to MOVR result.color, fragment.color */
	for (i=0;i<16;i++) {
		NVDmaStart(pNv, NvSub3D,
				NV30_TCL_PRIMITIVE_3D_TX_ENABLE_UNIT(i), 1);
		NVDmaNext(pNv, 0);
	}
	NV30_SetFragmentProg(pNv, NV30_FP_COL0_WR);
	/* Polygon stipple */
	NVDmaStart(pNv, NvSub3D,
			NV30_TCL_PRIMITIVE_3D_POLYGON_STIPPLE_PATTERN(0), 0x20);
	for (i=0;i<0x20;i++)
		NVDmaNext(pNv, 0xFFFFFFFF);

	/* Ok.  If you start X with the nvidia driver, kill it, and then
	 * start X with nouveau you will get black rendering instead of
	 * what you'd expect.  This fixes the problem, and it seems that
	 * it's not needed between nouveau restarts - which suggests that
	 * the 3D context (wherever it's stored?) survives somehow.
	 */
	NVDmaStart(pNv, NvSub3D, 0x1d60,1);
	NVDmaNext (pNv, 0x03008000);
}

