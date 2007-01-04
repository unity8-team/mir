/*
 * Copyright © 2006 Intel Corporation
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
 *    Wang Zhenyu <zhenyu.z.wang@intel.com>
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "i830.h"
#include "i915_reg.h"

/* bring in brw structs */
#include "brw_defines.h"
#include "brw_structs.h"

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

extern Bool
I965EXACheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
		      PicturePtr pDstPicture);

extern Bool
I965EXAPrepareComposite(int op, PicturePtr pSrcPicture,
			PicturePtr pMaskPicture, PicturePtr pDstPicture,
			PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst);

extern void
I965EXAComposite(PixmapPtr pDst, int srcX, int srcY, int maskX, int maskY,
		int dstX, int dstY, int width, int height);

extern float scale_units[2][2];
extern Bool is_transform[2];
extern PictTransform *transform[2];

struct blendinfo {
    Bool dst_alpha;
    Bool src_alpha;
    CARD32 src_blend;
    CARD32 dst_blend;
};

struct formatinfo {
    int fmt;
    CARD32 card_fmt;
};

// refer vol2, 3d rasterization 3.8.1

/* XXX: bad!bad! broadwater has different blend factor definition */
/* defined in brw_defines.h */
static struct blendinfo I965BlendOp[] = { 
    /* Clear */
    {0, 0, BRW_BLENDFACT_ZERO,          BRW_BLENDFACT_ZERO},
    /* Src */
    {0, 0, BRW_BLENDFACT_ONE,           BRW_BLENDFACT_ZERO},
    /* Dst */
    {0, 0, BRW_BLENDFACT_ZERO,          BRW_BLENDFACT_ONE},
    /* Over */
    {0, 1, BRW_BLENDFACT_ONE,           BRW_BLENDFACT_INV_SRC_ALPHA},
    /* OverReverse */
    {1, 0, BRW_BLENDFACT_INV_DST_ALPHA, BRW_BLENDFACT_ONE},
    /* In */
    {1, 0, BRW_BLENDFACT_DST_ALPHA,     BRW_BLENDFACT_ZERO},
    /* InReverse */
    {0, 1, BRW_BLENDFACT_ZERO,          BRW_BLENDFACT_SRC_ALPHA},
    /* Out */
    {1, 0, BRW_BLENDFACT_INV_DST_ALPHA, BRW_BLENDFACT_ZERO},
    /* OutReverse */
    {0, 1, BRW_BLENDFACT_ZERO,          BRW_BLENDFACT_INV_SRC_ALPHA},
    /* Atop */
    {1, 1, BRW_BLENDFACT_DST_ALPHA,     BRW_BLENDFACT_INV_SRC_ALPHA},
    /* AtopReverse */
    {1, 1, BRW_BLENDFACT_INV_DST_ALPHA, BRW_BLENDFACT_SRC_ALPHA},
    /* Xor */
    {1, 1, BRW_BLENDFACT_INV_DST_ALPHA, BRW_BLENDFACT_INV_SRC_ALPHA},
    /* Add */
    {0, 0, BRW_BLENDFACT_ONE,           BRW_BLENDFACT_ONE},
};

/* FIXME: surface format defined in brw_defines.h, shared Sampling engine 1.7.2*/
static struct formatinfo I965TexFormats[] = {
        {PICT_a8r8g8b8, BRW_SURFACEFORMAT_R8G8B8A8_UNORM },
        {PICT_x8r8g8b8, BRW_SURFACEFORMAT_R8G8B8X8_UNORM },
        {PICT_a8b8g8r8, BRW_SURFACEFORMAT_B8G8R8A8_UNORM },
        {PICT_x8b8g8r8, BRW_SURFACEFORMAT_B8G8R8X8_UNORM },
        {PICT_r5g6b5,   BRW_SURFACEFORMAT_B5G6R5_UNORM   },
        {PICT_a1r5g5b5, BRW_SURFACEFORMAT_B5G6R5A1_UNORM },
        {PICT_x1r5g5b5, BRW_SURFACEFORMAT_B5G6R5X1_UNORM },
        {PICT_a8,       BRW_SURFACEFORMAT_A8_UNORM	 },
};

static void I965GetBlendCntl(int op, PicturePtr pMask, CARD32 dst_format, 
			     CARD32 *sblend, CARD32 *dblend)
{

    *sblend = I965BlendOp[op].src_blend;
    *dblend = I965BlendOp[op].dst_blend;

    /* If there's no dst alpha channel, adjust the blend op so that we'll treat
     * it as always 1.
     */
    if (PICT_FORMAT_A(dst_format) == 0 && I965BlendOp[op].dst_alpha) {
        if (*sblend == BRW_BLENDFACT_DST_ALPHA)
            *sblend = BRW_BLENDFACT_ONE;
        else if (*sblend == BRW_BLENDFACT_INV_DST_ALPHA)
            *sblend = BRW_BLENDFACT_ZERO;
    }

    /* If the source alpha is being used, then we should only be in a case where
     * the source blend factor is 0, and the source blend value is the mask
     * channels multiplied by the source picture's alpha.
     */
    if (pMask && pMask->componentAlpha && I965BlendOp[op].src_alpha) {
        if (*dblend == BRW_BLENDFACT_SRC_ALPHA) {
	    *dblend = BRW_BLENDFACT_SRC_COLR;
        } else if (*dblend == BRW_BLENDFACT_INV_SRC_ALPHA) {
	    *dblend = BRW_BLENDFACT_INV_SRC_COLR;
        }
    }

}


/* FIXME */
static Bool I965GetDestFormat(PicturePtr pDstPicture, CARD32 *dst_format)
{
    switch (pDstPicture->format) {
    case PICT_a8r8g8b8:
    case PICT_x8r8g8b8:
        *dst_format = BRW_SURFACEFORMAT_B8G8R8A8_UNORM;
        break;
    case PICT_r5g6b5:
        *dst_format = BRW_SURFACEFORMAT_B5G6R5_UNORM;
        break;
    case PICT_a1r5g5b5:
    	*dst_format = BRW_SURFACEFORMAT_B5G6R5A1_UNORM;
	break;
    case PICT_x1r5g5b5:
        *dst_format = BRW_SURFACEFORMAT_B5G6R5X1_UNORM;
        break;
    /* COLR_BUF_8BIT is special for YUV surfaces.  While we may end up being
     * able to use it depending on how the hardware implements it, disable it
     * for now while we don't know what exactly it does (what channel does it
     * read from?
     */
    /*
    case PICT_a8:
        *dst_format = COLR_BUF_8BIT;
        break;
    */
    case PICT_a4r4g4b4:
    case PICT_x4r4g4b4:
	*dst_format = BRW_SURFACEFORMAT_B4G4R4A4_UNORM; 
	break;
    default:
        I830FALLBACK("Unsupported dest format 0x%x\n",
                        (int)pDstPicture->format);
    }

    return TRUE;
}

static Bool I965CheckCompositeTexture(PicturePtr pPict, int unit)
{
    int w = pPict->pDrawable->width;
    int h = pPict->pDrawable->height;
    int i;
                                                                                                                                                            
    if ((w > 0x7ff) || (h > 0x7ff))
        I830FALLBACK("Picture w/h too large (%dx%d)\n", w, h);

    for (i = 0; i < sizeof(I965TexFormats) / sizeof(I965TexFormats[0]); i++)
    {
        if (I965TexFormats[i].fmt == pPict->format)
            break;
    }
    if (i == sizeof(I965TexFormats) / sizeof(I965TexFormats[0]))
        I830FALLBACK("Unsupported picture format 0x%x\n",
                         (int)pPict->format);

    /* XXX: fallback when repeat? */
    if (pPict->repeat && pPict->repeatType != RepeatNormal)
	I830FALLBACK("extended repeat (%d) not supported\n",
		     pPict->repeatType);

    if (pPict->filter != PictFilterNearest &&
        pPict->filter != PictFilterBilinear)
        I830FALLBACK("Unsupported filter 0x%x\n", pPict->filter);

    return TRUE;
}

Bool
I965EXACheckComposite(int op, PicturePtr pSrcPicture, PicturePtr pMaskPicture,
		      PicturePtr pDstPicture)
{
	/* check op*/
	/* check op with mask's componentAlpha*/
	/* check textures */
	/* check dst buffer format */
    CARD32 tmp1;
    
    /* Check for unsupported compositing operations. */
    if (op >= sizeof(I965BlendOp) / sizeof(I965BlendOp[0]))
        I830FALLBACK("Unsupported Composite op 0x%x\n", op);
                                                                                                                                                            
    if (pMaskPicture != NULL && pMaskPicture->componentAlpha) {
        /* Check if it's component alpha that relies on a source alpha and on
         * the source value.  We can only get one of those into the single
         * source value that we get to blend with.
         */
        if (I965BlendOp[op].src_alpha &&
            (I965BlendOp[op].src_blend != BRW_BLENDFACT_ZERO))
            	I830FALLBACK("Component alpha not supported with source "
                            "alpha and source value blending.\n");
	/* XXX: fallback now for mask with componentAlpha */
	I830FALLBACK("mask componentAlpha not ready.\n");
    }

    if (!I965CheckCompositeTexture(pSrcPicture, 0))
        I830FALLBACK("Check Src picture texture\n");
    if (pMaskPicture != NULL && !I965CheckCompositeTexture(pMaskPicture, 1))
        I830FALLBACK("Check Mask picture texture\n");

    if (!I965GetDestFormat(pDstPicture, &tmp1)) 
	I830FALLBACK("Get Color buffer format\n");

    return TRUE;

}

#define ALIGN(i,m)    (((i) + (m) - 1) & ~((m) - 1))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

int urb_vs_start, urb_vs_size;
int urb_gs_start, urb_gs_size;
int urb_clip_start, urb_clip_size;
int urb_sf_start, urb_sf_size;
int urb_cs_start, urb_cs_size;

struct brw_surface_state *dest_surf_state;
struct brw_surface_state *src_surf_state;
struct brw_surface_state *mask_surf_state;
struct brw_sampler_state *src_sampler_state;
struct brw_sampler_state *mask_sampler_state;  // could just use one sampler?

struct brw_vs_unit_state *vs_state;
struct brw_sf_unit_state *sf_state;
struct brw_wm_unit_state *wm_state;
struct brw_cc_unit_state *cc_state;
struct brw_cc_viewport *cc_viewport;

struct brw_instruction *sf_kernel;
struct brw_instruction *ps_kernel;
struct brw_instruction *sip_kernel;

CARD32 *binding_table;
int binding_table_entries; 

int dest_surf_offset, src_surf_offset, src_sampler_offset, vs_offset;
int sf_offset, wm_offset, cc_offset, vb_offset, cc_viewport_offset;
int sf_kernel_offset, ps_kernel_offset, sip_kernel_offset;
int binding_table_offset;
int next_offset, total_state_size;
char *state_base;
int state_base_offset;
float *vb;
int vb_size = 4 * 4 ; /* 4 DWORDS per vertex, 4 vertices for TRIFAN*/ 

int src_blend, dst_blend;

static const CARD32 sip_kernel_static[][4] = {
/*    wait (1) a0<1>UW a145<0,1,0>UW { align1 +  } */
    { 0x00000030, 0x20000108, 0x00001220, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
    { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
    { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
    { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
    { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
    { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
    { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
    { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
    { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
    { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
};

/*
 * this program computes dA/dx and dA/dy for the texture coordinates along
 * with the base texture coordinate. It was extracted from the Mesa driver
 */

#define SF_KERNEL_NUM_GRF  10
#define SF_KERNEL_NUM_URB  8
#define SF_MAX_THREADS	   4

static const CARD32 sf_kernel_static[][4] = {
/*    send   0 (1) g6<1>F g1.12<0,1,0>F math mlen 1 rlen 1 { align1 +  } */
   { 0x00000031, 0x20c01fbd, 0x0000002c, 0x01110081 },
/*    send   0 (1) g6.4<1>F g1.20<0,1,0>F math mlen 1 rlen 1 { align1 +  } */
   { 0x00000031, 0x20c41fbd, 0x00000034, 0x01110081 },
/*    add (8) g7<1>F g4<8,8,1>F g3<8,8,1>F { align1 +  } */
   { 0x00600040, 0x20e077bd, 0x008d0080, 0x008d4060 },
/*    mul (1) g7<1>F g7<0,1,0>F g6<0,1,0>F { align1 +  } */
   { 0x00000041, 0x20e077bd, 0x000000e0, 0x000000c0 },
/*    mul (1) g7.4<1>F g7.4<0,1,0>F g6.4<0,1,0>F { align1 +  } */
   { 0x00000041, 0x20e477bd, 0x000000e4, 0x000000c4 },
/*    mov (8) m1<1>F g7<0,1,0>F { align1 +  } */
   { 0x00600001, 0x202003be, 0x000000e0, 0x00000000 },
/*    mov (8) m2<1>F g7.4<0,1,0>F { align1 +  } */
   { 0x00600001, 0x204003be, 0x000000e4, 0x00000000 },
/*    mov (8) m3<1>F g3<8,8,1>F { align1 +  } */
   { 0x00600001, 0x206003be, 0x008d0060, 0x00000000 },
/*    send   0 (8) a0<1>F g0<8,8,1>F urb mlen 4 rlen 0 write +0 transpose used complete EOT{ align1 +  } */
   { 0x00600031, 0x20001fbc, 0x008d0000, 0x8640c800 },
/*    nop (4) g0<1>UD { align1 +  } */
   { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
   { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
   { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
   { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
   { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
   { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
   { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
/*    nop (4) g0<1>UD { align1 +  } */
   { 0x0040007e, 0x20000c21, 0x00690000, 0x00000000 },
};

/* ps kernels */
/* 1: no mask */
static const CARD32 ps_kernel_static_nomask [][4] = {
	#include "i965_composite_ps_nomask.h"
};

/* 2: mask with componentAlpha, src * mask color, XXX: later */
static const CARD32 ps_kernel_static_maskca [][4] = {
	#include "i965_composite_ps_maskca.h"
};

/* 3: mask without componentAlpha, src * mask alpha */
static const CARD32 ps_kernel_static_masknoca [][4] = {
	#include "i965_composite_ps_masknoca.h"
};

Bool
I965EXAPrepareComposite(int op, PicturePtr pSrcPicture,
			PicturePtr pMaskPicture, PicturePtr pDstPicture,
			PixmapPtr pSrc, PixmapPtr pMask, PixmapPtr pDst)
{
    ScrnInfoPtr pScrn = xf86Screens[pSrcPicture->pDrawable->pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 src_offset, src_pitch;
    CARD32 mask_offset, mask_pitch;
    CARD32 dst_format, dst_offset, dst_pitch;
    CARD32 blendctl;
 
ErrorF("i965 prepareComposite\n");

//    i965_3d_pipeline_setup(pScrn);
//    i965_surf_setup(pScrn, pSrcPicture, pMaskPicture, pDstPicture,
//   			pSrc, pMask, pDst);
    // then setup blend, and shader program 

    I965GetDestFormat(pDstPicture, &dst_format);
    src_offset = exaGetPixmapOffset(pSrc);
    src_pitch = exaGetPixmapPitch(pSrc);
    dst_offset = exaGetPixmapOffset(pDst);
    dst_pitch = exaGetPixmapPitch(pDst);
    if (pMask) {
	mask_offset = exaGetPixmapOffset(pMask);
	mask_pitch = exaGetPixmapPitch(pMask);
    }
    scale_units[0][0] = pSrc->drawable.width;
    scale_units[0][1] = pSrc->drawable.height;
    scale_units[2][0] = pDst->drawable.width;
    scale_units[2][1] = pDst->drawable.height;

    if (!pMask) {
	is_transform[1] = FALSE;
	scale_units[1][0] = -1;
	scale_units[1][1] = -1;
    } else {
	scale_units[1][0] = pMask->drawable.width;
	scale_units[1][1] = pMask->drawable.height;
    }

/* FIXME */
	/* setup 3d pipeline state */

   binding_table_entries = 2; /* default no mask */

   /* Set up our layout of state in framebuffer.  First the general state: */
   next_offset = 0;
   vs_offset = ALIGN(next_offset, 64);
   next_offset = vs_offset + sizeof(*vs_state);
    
   sf_offset = ALIGN(next_offset, 32);
   next_offset = sf_offset + sizeof(*sf_state);
    
   wm_offset = ALIGN(next_offset, 32);
   next_offset = wm_offset + sizeof(*wm_state);
    
   cc_offset = ALIGN(next_offset, 32);
   next_offset = cc_offset + sizeof(*cc_state);

// fixup sf_kernel_static, is sf_kernel needed? or not? why? 
//	-> just keep current sf_kernel, which will send one setup urb entry to
//	PS kernel
   sf_kernel_offset = ALIGN(next_offset, 64);
   next_offset = sf_kernel_offset + sizeof (sf_kernel_static);

   //XXX: ps_kernel may be seperated, fix with offset
   ps_kernel_offset = ALIGN(next_offset, 64);
   if (pMask) {
	if (pMaskPicture->componentAlpha)
	    next_offset = ps_kernel_offset + sizeof(ps_kernel_static_maskca);
	else 
	    next_offset = ps_kernel_offset + sizeof(ps_kernel_static_masknoca);
   } else 
   	next_offset = ps_kernel_offset + sizeof (ps_kernel_static_nomask);
    
   sip_kernel_offset = ALIGN(next_offset, 64);
   next_offset = sip_kernel_offset + sizeof (sip_kernel_static);
   
   // needed?
   cc_viewport_offset = ALIGN(next_offset, 32);
   next_offset = cc_viewport_offset + sizeof(*cc_viewport);

   // : fix for texture sampler
   // XXX: -> use only one sampler
   src_sampler_offset = ALIGN(next_offset, 32);
   next_offset = src_sampler_offset + sizeof(*src_sampler_state);

   /* Align VB to native size of elements, for safety */
   vb_offset = ALIGN(next_offset, 8);
   next_offset = vb_offset + vb_size;

   /* And then the general state: */
   //XXX: fix for texture map and target surface
   dest_surf_offset = ALIGN(next_offset, 32);
   next_offset = dest_surf_offset + sizeof(*dest_surf_state);

   src_surf_offset = ALIGN(next_offset, 32);
   next_offset = src_surf_offset + sizeof(*src_surf_state);

   if (pMask) {
   	mask_surf_offset = ALIGN(next_offset, 32);
   	next_offset = mask_surf_offset + sizeof(*mask_surf_state);
	binding_table_entries = 3;
   }

   binding_table_offset = ALIGN(next_offset, 32);
   next_offset = binding_table_offset + (binding_table_entries * 4);

   total_state_size = next_offset;

   /*
    * XXX: Use the extra space allocated at the end of the exa offscreen buffer?
    */
#define BRW_LINEAR_EXTRA	(32*1024)

   state_base_offset = (pI830->Offscreen.End -
			BRW_LINEAR_EXTRA);
   
   state_base_offset = ALIGN(state_base_offset, 64);
   state_base = (char *)(pI830->FbBase + state_base_offset);
   /* Set up our pointers to state structures in framebuffer.  It would probably
    * be a good idea to fill these structures out in system memory and then dump
    * them there, instead.
    */
   vs_state = (void *)(state_base + vs_offset);
   sf_state = (void *)(state_base + sf_offset);
   wm_state = (void *)(state_base + wm_offset);
   cc_state = (void *)(state_base + cc_offset);
   sf_kernel = (void *)(state_base + sf_kernel_offset);
   ps_kernel = (void *)(state_base + ps_kernel_offset);
   sip_kernel = (void *)(state_base + sip_kernel_offset);
   
   cc_viewport = (void *)(state_base + cc_viewport_offset);
   
   dest_surf_state = (void *)(state_base + dest_surf_offset);
   src_surf_state = (void *)(state_base + src_surf_offset);
   if (pMask)
	mask_surf_state = (void *)(state_base + mask_surf_offset);

   src_sampler_state = (void *)(state_base + src_sampler_offset);
   binding_table = (void *)(state_base + binding_table_offset);

   vb = (void *)(state_base + vb_offset);

   /* Set up a default static partitioning of the URB, which is supposed to
    * allow anything we would want to do, at potentially lower performance.
    */
#define URB_CS_ENTRY_SIZE     0
#define URB_CS_ENTRIES	      0
   
#define URB_VS_ENTRY_SIZE     1	  // each 512-bit row
#define URB_VS_ENTRIES	      8	  // we needs at least 8 entries
   
#define URB_GS_ENTRY_SIZE     0
#define URB_GS_ENTRIES	      0
   
#define URB_CLIP_ENTRY_SIZE   0
#define URB_CLIP_ENTRIES      0
   
#define URB_SF_ENTRY_SIZE     4
#define URB_SF_ENTRIES	      8

   urb_vs_start = 0;
   urb_vs_size = URB_VS_ENTRIES * URB_VS_ENTRY_SIZE;
   urb_gs_start = urb_vs_start + urb_vs_size;
   urb_gs_size = URB_GS_ENTRIES * URB_GS_ENTRY_SIZE;
   urb_clip_start = urb_gs_start + urb_gs_size;
   urb_clip_size = URB_CLIP_ENTRIES * URB_CLIP_ENTRY_SIZE;
   urb_sf_start = urb_clip_start + urb_clip_size;
   urb_sf_size = URB_SF_ENTRIES * URB_SF_ENTRY_SIZE;
   urb_cs_start = urb_sf_start + urb_sf_size;
   urb_cs_size = URB_CS_ENTRIES * URB_CS_ENTRY_SIZE;

   /* We'll be poking the state buffers that could be in use by the 3d hardware
    * here, but we should have synced the 3D engine already in I830PutImage.
    */

// needed?
   memset (cc_viewport, 0, sizeof (*cc_viewport));
   cc_viewport->min_depth = -1.e35;
   cc_viewport->max_depth = 1.e35;

   /* Color calculator state */
   memset(cc_state, 0, sizeof(*cc_state));
   cc_state->cc0.stencil_enable = 0;   /* disable stencil */
   cc_state->cc2.depth_test = 0;       /* disable depth test */
   cc_state->cc2.logicop_enable = 0;   /* disable logic op */
   cc_state->cc3.ia_blend_enable = 0;  /* blend alpha just like colors */
   cc_state->cc3.blend_enable = 1;     /* enable color blend */
   cc_state->cc3.alpha_test = 0;       /* disable alpha test */
   // XXX:cc_viewport needed? 
   cc_state->cc4.cc_viewport_state_offset = (state_base_offset + cc_viewport_offset) >> 5;
   cc_state->cc5.dither_enable = 0;    /* disable dither */
//   cc_state->cc5.logicop_func = 0xc;   /* COPY */
//   cc_state->cc5.statistics_enable = 1;
//   cc_state->cc5.ia_blend_function = BRW_BLENDFUNCTION_ADD;
//   cc_state->cc5.ia_src_blend_factor = BRW_BLENDFACTOR_ONE;
//   cc_state->cc5.ia_dest_blend_factor = BRW_BLENDFACTOR_ONE;
   cc_state->cc6.blend_function = BRW_BLENDFUNCTION_ADD;
   I965GetBlendCntl(op, pMask, pDstPicture->format, 
		    &src_blend, &dst_blend);
   cc_state->cc6.src_blend_factor = src_blend;
   cc_state->cc6.dest_blend_factor = dst_blend;

   /* Upload system kernel */
   memcpy (sip_kernel, sip_kernel_static, sizeof (sip_kernel_static));
   
   /* Set up the state buffer for the destination surface */
   memset(dest_surf_state, 0, sizeof(*dest_surf_state));
   dest_surf_state->ss0.surface_type = BRW_SURFACE_2D;
   dest_surf_state->ss0.data_return_format = BRW_SURFACERETURNFORMAT_FLOAT32;
   // XXX: should compare with picture's cpp?...8 bit surf?
   if (pDst->drawable.bitsPerPixel == 16) {
      dest_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_B5G6R5_UNORM;
   } else {
      dest_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_B8G8R8A8_UNORM;
   }
   dest_surf_state->ss0.writedisable_alpha = 0;
   dest_surf_state->ss0.writedisable_red = 0;
   dest_surf_state->ss0.writedisable_green = 0;
   dest_surf_state->ss0.writedisable_blue = 0;
   dest_surf_state->ss0.color_blend = 1;
   dest_surf_state->ss0.vert_line_stride = 0;
   dest_surf_state->ss0.vert_line_stride_ofs = 0;
   dest_surf_state->ss0.mipmap_layout_mode = 0;
   dest_surf_state->ss0.render_cache_read_mode = 0;
   
   // XXX: fix to picture address & size
   dest_surf_state->ss1.base_addr = dst_offset;
   dest_surf_state->ss2.height = pDst->drawable.height - 1;
   dest_surf_state->ss2.width = pDst->drawable.width - 1;
   dest_surf_state->ss2.mip_count = 0;
   dest_surf_state->ss2.render_target_rotation = 0;
   dest_surf_state->ss3.pitch = dst_pitch - 1; 
   // tiled surface?

   /* Set up the source surface state buffer */
   memset(src_surf_state, 0, sizeof(*src_surf_state));
   src_surf_state->ss0.surface_type = BRW_SURFACE_2D;
   if (pSrc->drawable.bitsPerPixel == 8)
      src_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_A8_UNORM; //XXX? 
   else if (pSrc->drawable.bitsPerPixel == 16)
      src_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_B5G6R5_UNORM;
   else 
      src_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_B8G8R8A8_UNORM;

   src_surf_state->ss0.writedisable_alpha = 0;
   src_surf_state->ss0.writedisable_red = 0;
   src_surf_state->ss0.writedisable_green = 0;
   src_surf_state->ss0.writedisable_blue = 0;
   src_surf_state->ss0.color_blend = 1;
   src_surf_state->ss0.vert_line_stride = 0;
   src_surf_state->ss0.vert_line_stride_ofs = 0;
   src_surf_state->ss0.mipmap_layout_mode = 0;
   src_surf_state->ss0.render_cache_read_mode = 0;
   
   src_surf_state->ss1.base_addr = src_offset;
   src_surf_state->ss2.width = pSrc->drawable.width - 1;
   src_surf_state->ss2.height = pSrc->drawable.height - 1;
   src_surf_state->ss2.mip_count = 0;
   src_surf_state->ss2.render_target_rotation = 0;
   src_surf_state->ss3.pitch = src_pitch - 1; 

   /* setup mask surface */
   if (pMask) {
   	memset(mask_surf_state, 0, sizeof(*mask_surf_state));
	mask_surf_state->ss0.surface_type = BRW_SURFACE_2D;
   	if (pMask->drawable.bitsPerPixel == 8)
      	    mask_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_A8_UNORM; //XXX? 
   	else if (pMask->drawable.bitsPerPixel == 16)
      	    mask_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_B5G6R5_UNORM;
   	else 
      	    mask_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_B8G8R8A8_UNORM;

   	mask_surf_state->ss0.writedisable_alpha = 0;
   	mask_surf_state->ss0.writedisable_red = 0;
   	mask_surf_state->ss0.writedisable_green = 0;
   	mask_surf_state->ss0.writedisable_blue = 0;
   	mask_surf_state->ss0.color_blend = 1;
   	mask_surf_state->ss0.vert_line_stride = 0;
   	mask_surf_state->ss0.vert_line_stride_ofs = 0;
   	mask_surf_state->ss0.mipmap_layout_mode = 0;
   	mask_surf_state->ss0.render_cache_read_mode = 0;
   
   	mask_surf_state->ss1.base_addr = mask_offset;
   	mask_surf_state->ss2.width = pMask->drawable.width - 1;
   	mask_surf_state->ss2.height = pMask->drawable.height - 1;
   	mask_surf_state->ss2.mip_count = 0;
   	mask_surf_state->ss2.render_target_rotation = 0;
   	mask_surf_state->ss3.pitch = mask_pitch - 1; 
   }

   /* Set up a binding table for our surfaces.  Only the PS will use it */
   binding_table[0] = state_base_offset + dest_surf_offset;
   binding_table[1] = state_base_offset + src_surf_offset;
   if (pMask)
   	binding_table[2] = state_base_offset + mask_surf_offset;

   /* PS kernel use this sampler */
   memset(src_sampler_state, 0, sizeof(*src_sampler_state));
   src_sampler_state->ss0.lod_peclamp = 1; /* GL mode */
   switch(pSrcPicture->filter) {
   case PictFilterNearest:
   	src_sampler_state->ss0.min_filter = BRW_MAPFILTER_NEAREST; 
   	src_sampler_state->ss0.mag_filter = BRW_MAPFILTER_NEAREST;
	break;
   case PictFilterBilinear:
   	src_sampler_state->ss0.min_filter = BRW_MAPFILTER_LINEAR; 
   	src_sampler_state->ss0.mag_filter = BRW_MAPFILTER_LINEAR;
	break;
   default:
	I830FALLBACK("Bad filter 0x%x\n", pSrcPicture->filter);
   }

   if (!pSrcPicture->repeat) {
	/* XXX: clamp_border and set border to 0 */
   	src_sampler_state->ss1.r_wrap_mode = BRW_TEXCOORDMODE_CLAMP; 
   	src_sampler_state->ss1.s_wrap_mode = BRW_TEXCOORDMODE_CLAMP;
   	src_sampler_state->ss1.t_wrap_mode = BRW_TEXCOORDMODE_CLAMP;
   } else {
   	src_sampler_state->ss1.r_wrap_mode = BRW_TEXCOORDMODE_WRAP; 
   	src_sampler_state->ss1.s_wrap_mode = BRW_TEXCOORDMODE_WRAP;
   	src_sampler_state->ss1.t_wrap_mode = BRW_TEXCOORDMODE_WRAP;
   }
   /* XXX: ss2 has border color pointer, which should be in general state address,
    	   and just a single texel tex map, with R32G32B32A32_FLOAT */
   src_sampler_state->ss3.chroma_key_enable = 0; /* disable chromakey */

   /* Set up the vertex shader to be disabled (passthrough) */
   memset(vs_state, 0, sizeof(*vs_state));
   // XXX: vs URB should be defined for VF vertex URB store. done already?
   vs_state->vs6.vs_enable = 0;

   // XXX: sf_kernel? keep it as now
   /* Set up the SF kernel to do coord interp: for each attribute,
    * calculate dA/dx and dA/dy.  Hand these interpolation coefficients
    * back to SF which then hands pixels off to WM.
    */
   memcpy (sf_kernel, sf_kernel_static, sizeof (sf_kernel_static));

   memset(sf_state, 0, sizeof(*sf_state));
   sf_state->thread0.kernel_start_pointer = 
	       (state_base_offset + sf_kernel_offset) >> 6;
   sf_state->thread0.grf_reg_count = ((SF_KERNEL_NUM_GRF & ~15) / 16);
   sf_state->sf1.single_program_flow = 1;
   sf_state->sf1.binding_table_entry_count = 0;
   sf_state->sf1.thread_priority = 0;
   sf_state->sf1.floating_point_mode = 0; /* Mesa does this */
   sf_state->sf1.illegal_op_exception_enable = 1;
   sf_state->sf1.mask_stack_exception_enable = 1;
   sf_state->sf1.sw_exception_enable = 1;
   sf_state->thread2.per_thread_scratch_space = 0;
   sf_state->thread2.scratch_space_base_pointer = 0; /* not used in our kernel */
   sf_state->thread3.const_urb_entry_read_length = 0; /* no const URBs */
   sf_state->thread3.const_urb_entry_read_offset = 0; /* no const URBs */
   sf_state->thread3.urb_entry_read_length = 1; /* 1 URB per vertex */
   sf_state->thread3.urb_entry_read_offset = 0;
   sf_state->thread3.dispatch_grf_start_reg = 3;
   sf_state->thread4.max_threads = SF_MAX_THREADS - 1;
   sf_state->thread4.urb_entry_allocation_size = URB_SF_ENTRY_SIZE - 1;
   sf_state->thread4.nr_urb_entries = URB_SF_ENTRIES;
   sf_state->thread4.stats_enable = 1;
   sf_state->sf5.viewport_transform = FALSE; /* skip viewport */
   sf_state->sf6.cull_mode = BRW_CULLMODE_NONE;
   sf_state->sf6.scissor = 0;
   sf_state->sf7.trifan_pv = 2;
   sf_state->sf6.dest_org_vbias = 0x8;
   sf_state->sf6.dest_org_hbias = 0x8;

   /* Set up the PS kernel (dispatched by WM) 
    */
    
    // XXX: replace to texture blend shader, and different cases 
   if (pMask) {
	if (pMaskPicture->componentAlpha)
   	    memcpy (ps_kernel, ps_kernel_static_maskca, sizeof (ps_kernel_static_maskca));
	else
   	    memcpy (ps_kernel, ps_kernel_static_masknoca, sizeof (ps_kernel_static_masknoca));
   } else 
   	memcpy (ps_kernel, ps_kernel_static_nomask, sizeof (ps_kernel_static_nomask));

   memset (wm_state, 0, sizeof (*wm_state));
   wm_state->thread0.kernel_start_pointer = 
	    (state_base_offset + ps_kernel_offset) >> 6;
   wm_state->thread0.grf_reg_count = ((PS_KERNEL_NUM_GRF & ~15) / 16);
   wm_state->thread1.single_program_flow = 1;
   if (!pMask)
       wm_state->thread1.binding_table_entry_count = 2; /* tex and fb */
   else
       wm_state->thread1.binding_table_entry_count = 3; /* tex and fb */

   wm_state->thread2.scratch_space_base_pointer = 0;
   wm_state->thread2.per_thread_scratch_space = 0;
   // XXX: urb allocation
   wm_state->thread3.dispatch_grf_start_reg = 3; /* must match kernel */
   // wm kernel use urb from 3, see wm_program in compiler module
   wm_state->thread3.urb_entry_read_length = 1;  /* one per pair of attrib */
   wm_state->thread3.const_urb_entry_read_length = 0;
   wm_state->thread3.const_urb_entry_read_offset = 0;
   wm_state->thread3.urb_entry_read_offset = 0;

   wm_state->wm4.stats_enable = 1;
   wm_state->wm4.sampler_state_pointer = (state_base_offset + src_sampler_offset) >> 5;
   wm_state->wm4.sampler_count = 1; /* 1-4 samplers used */
   wm_state->wm5.max_threads = PS_MAX_THREADS - 1;
   wm_state->wm5.thread_dispatch_enable = 1;
   //just use 16-pixel dispatch, don't need to change kernel start point
   wm_state->wm5.enable_16_pix = 1;
   wm_state->wm5.enable_8_pix = 0;
   wm_state->wm5.early_depth_test = 1;

   /* Begin the long sequence of commands needed to set up the 3D 
    * rendering pipe
    */
   {
   
   BEGIN_LP_RING((pMask?48:46));
   // MI_FLUSH prior to PIPELINE_SELECT
   OUT_RING(MI_FLUSH | 
	    MI_STATE_INSTRUCTION_CACHE_FLUSH |
	    BRW_MI_GLOBAL_SNAPSHOT_RESET);
   
   /* Match Mesa driver setup */
   OUT_RING(BRW_PIPELINE_SELECT | PIPELINE_SELECT_3D);
   
   /* Zero out the two base address registers so all offsets are absolute */
   // XXX: zero out...
   OUT_RING(BRW_STATE_BASE_ADDRESS | 4);
   // why this's not state_base_offset? -> because later we'll always add on
   // state_base_offset to offset params. see SIP
   OUT_RING(0 | BASE_ADDRESS_MODIFY);  /* Generate state base address */
   OUT_RING(0 | BASE_ADDRESS_MODIFY);  /* Surface state base address */
   OUT_RING(0 | BASE_ADDRESS_MODIFY);  /* media base addr, don't care */
   OUT_RING(0x10000000 | BASE_ADDRESS_MODIFY);  /* general state max addr, disabled */
   OUT_RING(0x10000000 | BASE_ADDRESS_MODIFY);  /* media object state max addr, disabled */

   /* Set system instruction pointer */
   OUT_RING(BRW_STATE_SIP | 0);
   OUT_RING(state_base_offset + sip_kernel_offset); /* system instruction pointer */
      
   /* Pipe control */
   // XXX: pipe control write cache before enabling color blending
   // vol2, geometry pipeline 1.8.4
   OUT_RING(BRW_PIPE_CONTROL |
	    BRW_PIPE_CONTROL_NOWRITE |
	    BRW_PIPE_CONTROL_IS_FLUSH |
	    2);
   OUT_RING(0);			       /* Destination address */
   OUT_RING(0);			       /* Immediate data low DW */
   OUT_RING(0);			       /* Immediate data high DW */

   /* Binding table pointers */
   OUT_RING(BRW_3DSTATE_BINDING_TABLE_POINTERS | 4);
   OUT_RING(0); /* vs */
   OUT_RING(0); /* gs */
   OUT_RING(0); /* clip */
   OUT_RING(0); /* sf */
   /* Only the PS uses the binding table */
   OUT_RING(state_base_offset + binding_table_offset); /* ps */

   //ring 20

   /* The drawing rectangle clipping is always on.  Set it to values that
    * shouldn't do any clipping.
    */
    //XXX: fix for picture size
   OUT_RING(BRW_3DSTATE_DRAWING_RECTANGLE | 2);	/* XXX 3 for BLC or CTG */
   OUT_RING(0x00000000);	/* ymin, xmin */
   OUT_RING((pScrn->virtualX - 1) |
	    (pScrn->virtualY - 1) << 16); /* ymax, xmax */
   OUT_RING(0x00000000);	/* yorigin, xorigin */

   /* skip the depth buffer */
   /* skip the polygon stipple */
   /* skip the polygon stipple offset */
   /* skip the line stipple */
   
   /* Set the pointers to the 3d pipeline state */
   OUT_RING(BRW_3DSTATE_PIPELINED_POINTERS | 5);
   OUT_RING(state_base_offset + vs_offset);  /* 32 byte aligned */
   OUT_RING(BRW_GS_DISABLE);		     /* disable GS, resulting in passthrough */
   OUT_RING(BRW_CLIP_DISABLE);		     /* disable CLIP, resulting in passthrough */
   OUT_RING(state_base_offset + sf_offset);  /* 32 byte aligned */
   OUT_RING(state_base_offset + wm_offset);  /* 32 byte aligned */
   OUT_RING(state_base_offset + cc_offset);  /* 64 byte aligned */

   /* URB fence */
   // XXX: CS for const URB needed? if not, cs_fence should be equal to sf_fence
   OUT_RING(BRW_URB_FENCE |
	    UF0_CS_REALLOC |
	    UF0_SF_REALLOC |
	    UF0_CLIP_REALLOC |
	    UF0_GS_REALLOC |
	    UF0_VS_REALLOC |
	    1);
   OUT_RING(((urb_clip_start + urb_clip_size) << UF1_CLIP_FENCE_SHIFT) |
	    ((urb_gs_start + urb_gs_size) << UF1_GS_FENCE_SHIFT) |
	    ((urb_vs_start + urb_vs_size) << UF1_VS_FENCE_SHIFT));
   OUT_RING(((urb_cs_start + urb_cs_size) << UF2_CS_FENCE_SHIFT) |
	    ((urb_sf_start + urb_sf_size) << UF2_SF_FENCE_SHIFT));

   /* Constant buffer state */
   // XXX: needed? seems no usage, as we don't have CONSTANT_BUFFER definition
   OUT_RING(BRW_CS_URB_STATE | 0);
   OUT_RING(((URB_CS_ENTRY_SIZE - 1) << 4) | /* URB Entry Allocation Size */
	    (URB_CS_ENTRIES << 0));	     /* Number of URB Entries */
   
   /* Set up the pointer to our vertex buffer */
   // XXX: double check
  // int vb_pitch = 4 * 4;  // XXX: pitch should include mask's coords? possible
  // all three coords on one row?
   int nelem = pMask ? 3: 2;
   OUT_RING(BRW_3DSTATE_VERTEX_BUFFERS | 3); //should be 4n-1 -> 3
   OUT_RING((0 << VB0_BUFFER_INDEX_SHIFT) |
	    VB0_VERTEXDATA |
	    ((4 * 2 * nelem) << VB0_BUFFER_PITCH_SHIFT)); 
   		// pitch includes all vertex data, 4bytes for 1 dword, each
		// element has 2 coords (x,y)(s0,t0), nelem to reflect possible
		// mask
   OUT_RING(state_base_offset + vb_offset);
   OUT_RING(4 * nelem); // max index, prim has 4 coords
   OUT_RING(0); // ignore for VERTEXDATA, but still there

   /* Set up our vertex elements, sourced from the single vertex buffer. */
   OUT_RING(BRW_3DSTATE_VERTEX_ELEMENTS | ((2 * nelem) - 1));  // XXX: 2n-1, (x,y) + (s0,t0) +
						//   possible (s1, t1)
   /* offset 0: X,Y -> {X, Y, 1.0, 1.0} */
   OUT_RING((0 << VE0_VERTEX_BUFFER_INDEX_SHIFT) |
	    VE0_VALID |
	    (BRW_SURFACEFORMAT_R32G32_FLOAT << VE0_FORMAT_SHIFT) |
	    (0 << VE0_OFFSET_SHIFT));
   OUT_RING((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
	    (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
	    (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_2_SHIFT) |
	    (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT) |
	    (0 << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT));
   /* offset 8: S0, T0 -> {S0, T0, 1.0, 1.0} */
   OUT_RING((0 << VE0_VERTEX_BUFFER_INDEX_SHIFT) |
	    VE0_VALID |
	    (BRW_SURFACEFORMAT_R32G32_FLOAT << VE0_FORMAT_SHIFT) |
	    (8 << VE0_OFFSET_SHIFT));
   OUT_RING((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
	    (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
	    (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_2_SHIFT) |
	    (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT) |
	    (4 << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT));

   if (pMask) {
   	OUT_RING((0 << VE0_VERTEX_BUFFER_INDEX_SHIFT) |
	    VE0_VALID |
	    (BRW_SURFACEFORMAT_R32G32_FLOAT << VE0_FORMAT_SHIFT) |
	    (16 << VE0_OFFSET_SHIFT));
	OUT_RING((BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_0_SHIFT) |
	    (BRW_VFCOMPONENT_STORE_SRC << VE1_VFCOMPONENT_1_SHIFT) |
	    (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_2_SHIFT) |
	    (BRW_VFCOMPONENT_STORE_1_FLT << VE1_VFCOMPONENT_3_SHIFT) |
	    (8 << VE1_DESTINATION_ELEMENT_OFFSET_SHIFT)); 
		//XXX: is this has alignment issue? and thread access problem?
	    
   }
   
   ADVANCE_LP_RING();
    
   }

    {
	/* cc states */
	/* dest buffer */
	/* urbs */
	/* binding tables */
	/* clipping */
	/* color blend (color calculator, dataport shared function)
		COLOR_CALC_STATE/SURFACE_STATE(rendertarget's color blend enable
		bit)
		Errata!!!: brw-a/b, rendertarget 'local' color blending always
		enabled! only control by global enable bit.
	   surface format for blend, "Surface format table in Sampling Engine"
	   XXX: if surface format not support, we should fallback.
	*/
	/* 
	    render target should be defined in SURFACE_STATE
	    	o render target SURFTYPE_BUFFER? 2D? Keith has 2D set.
		o depth buffer SURFTYPE_NULL?
	    color blend:
	        o Errata!!: mush issue PIPE_CONTROL with Write Cache Flush
		enable set, before transite to read-write color buffer. 
	    	o disable pre/post-blending clamping
		o enable color buffer blending enable in COLOR_CALC_STATE,(vol2, 3d rasterization 3.8) 
		  enable color blending enable in SURFACE_STATE.(shared,
		  sampling engine 1.7) 
		  disable depth test
		o (we don't use BLENDFACT_SRC_ALPHA_SATURATE, so don't care
		the Errata for independent alpha blending, just use color
		blending factor for all) disable independent alpha blending
		in COLOR_CALC_STATE
		o set src/dst blend factor in COLOR_CALC_STATE

	*/
    }

	/* shader program 
		o use sampler shared function for texture data
		o submit result to dataport for later color blending */
    {
	 /* PS program:
	 	o declare sampler and variables??
		o 'send' cmd to Sampling Engine to load 'src' picture
		o if (!pMask) then 'send' 'src' texture value to DataPort
		target render cache
		o else 
		    - 'send' cmd to SE to load 'mask' picture
		    - if no alpha, force to 1 (move 1 to W element of mask)
		    - if (mask->componentAlpha) then mul 'src' & 'mask', 'send'
		    	output to DataPort render cache
		    - else mul 'src' & 'mask''s W element(alpha), 'send' output
		    	to Dataport render cache
	 */

    }

#ifdef I830DEBUG
    ErrorF("try to sync to show any errors...");
    I830Sync(pScrn);
#endif
    return TRUE;
}	

void
I965EXAComposite(PixmapPtr pDst, int srcX, int srcY, int maskX, int maskY,
		int dstX, int dstY, int w, int h)
{
    int srcXend, srcYend, maskXend, maskYend;
    PictVector v;
    int pMask = 1, i = 0;

    DPRINTF(PFX, "Composite: srcX %d, srcY %d\n\t maskX %d, maskY %d\n\t"
	    "dstX %d, dstY %d\n\twidth %d, height %d\n\t"
	    "src_scale_x %f, src_scale_y %f, "
	    "mask_scale_x %f, mask_scale_y %f\n",
	    srcX, srcY, maskX, maskY, dstX, dstY, w, h,
	    scale_units[0][0], scale_units[0][1],
	    scale_units[1][0], scale_units[1][1]);

    if (scale_units[1][0] == -1 || scale_units[1][1] == -1) {
	pMask = 0;
    }

    srcXend = srcX + w;
    srcYend = srcY + h;
    maskXend = maskX + w;
    maskYend = maskY + h;
    if (is_transform[0]) {
        v.vector[0] = IntToxFixed(srcX);
        v.vector[1] = IntToxFixed(srcY);
        v.vector[2] = xFixed1;
        PictureTransformPoint(transform[0], &v);
        srcX = xFixedToInt(v.vector[0]);
        srcY = xFixedToInt(v.vector[1]);
        v.vector[0] = IntToxFixed(srcXend);
        v.vector[1] = IntToxFixed(srcYend);
        v.vector[2] = xFixed1;
        PictureTransformPoint(transform[0], &v);
        srcXend = xFixedToInt(v.vector[0]);
        srcYend = xFixedToInt(v.vector[1]);
    }
    if (is_transform[1]) {
        v.vector[0] = IntToxFixed(maskX);
        v.vector[1] = IntToxFixed(maskY);
        v.vector[2] = xFixed1;
        PictureTransformPoint(transform[1], &v);
        maskX = xFixedToInt(v.vector[0]);
        maskY = xFixedToInt(v.vector[1]);
        v.vector[0] = IntToxFixed(maskXend);
        v.vector[1] = IntToxFixed(maskYend);
        v.vector[2] = xFixed1;
        PictureTransformPoint(transform[1], &v);
        maskXend = xFixedToInt(v.vector[0]);
        maskYend = xFixedToInt(v.vector[1]);
    }

    DPRINTF(PFX, "After transform: srcX %d, srcY %d,srcXend %d, srcYend %d\n\t"
		"maskX %d, maskY %d, maskXend %d, maskYend %d\n\t"
		"dstX %d, dstY %d\n", srcX, srcY, srcXend, srcYend,
		maskX, maskY, maskXend, maskYend, dstX, dstY);

 
    vb[i++] = (float)dstX;
    vb[i++] = (float)dstY;
    vb[i++] = (float)srcX / scale_units[0][0];
    vb[i++] = (float)srcY / scale_units[0][1];
    if (pMask) {
        vb[i++] = (float)maskX / scale_units[1][0];
        vb[i++] = (float)maskY / scale_units[1][1];
    }

    vb[i++] = (float)dstX;
    vb[i++] = (float)(dstY + h);
    vb[i++] = (float)srcX / scale_units[0][0];
    vb[i++] = (float)srcYend / scale_units[0][1];
    if (pMask) {
        vb[i++] = (float)maskX / scale_units[1][0];
        vb[i++] = (float)maskYend / scale_units[1][1];
    }

    vb[i++] = (float)(dstX + w);
    vb[i++] = (float)(dstY + h);
    vb[i++] = (float)srcXend / scale_units[0][0];
    vb[i++] = (float)srcYend / scale_units[0][1];
    if (pMask) {
        vb[i++] = (float)maskXend / scale_units[1][0];
        vb[i++] = (float)maskYend / scale_units[1][1];
    }

    vb[i++] = (float)(dstX + w);
    vb[i++] = (float)dstY;
    vb[i++] = (float)srcXend / scale_units[0][0];
    vb[i++] = (float)srcY / scale_units[0][1];
    if (pMask) {
        vb[i++] = (float)maskXend / scale_units[1][0];
        vb[i++] = (float)maskY / scale_units[1][1];
    }

    {
      BEGIN_LP_RING(6);
      OUT_RING(BRW_3DPRIMITIVE | 
	       BRW_3DPRIMITIVE_VERTEX_SEQUENTIAL |
	       (_3DPRIM_TRIFAN << BRW_3DPRIMITIVE_TOPOLOGY_SHIFT) | 
	       (0 << 9) |  /* CTG - indirect vertex count */
	       4);
      OUT_RING(4);  /* vertex count per instance */
      OUT_RING(0); /* start vertex offset */
      OUT_RING(1); /* single instance */
      OUT_RING(0); /* start instance location */
      OUT_RING(0); /* index buffer offset, ignored */
      ADVANCE_LP_RING();
    }
#ifdef I830DEBUG
    ErrorF("sync after 3dprimitive");
    I830Sync(pScrn);
#endif
}
