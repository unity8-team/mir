/* -*- c-basic-offset: 3 -*- */
/**************************************************************************

Copyright 2005 Tungsten Graphics, Inc., Cedar Park, Texas.

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

/*
 * Authors:
 *   Alan Hourihane <alanh@tungstengraphics.com>
 *   Brian Paul <brian.paul@tungstengraphics.com>
 *   Keith Whitwell <keith@tungstengraphics.com>
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "servermd.h"
#include "shadow.h"

#include "i830.h"
#include "i915_reg.h"
#include "i915_3d.h"
#include "brw_defines.h"
#include "brw_structs.h"

#ifdef XF86DRI
#include "dri.h"
#endif

static void *
I830WindowLinear (ScreenPtr pScreen,
		 CARD32    row,
		 CARD32    offset,
		 int	   mode,
		 CARD32    *size,
		 void	   *closure)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   CARD8 *ptr;

   *size = (pScrn->bitsPerPixel * pI830->displayWidth >> 3);
   if (I830IsPrimary(pScrn))
      ptr = (CARD8 *) (pI830->FbBase + pI830->FrontBuffer.Start) + row * (*size) + offset;
   else {
      I830Ptr pI8301 = I830PTR(pI830->entityPrivate->pScrn_1);
      ptr = (CARD8 *) (pI830->FbBase + pI8301->FrontBuffer2.Start) + row * (*size) + offset;
   }
   return (void *)ptr;
}

struct matrix23
{
	int m00, m01, m02;
	int m10, m11, m12;
};

static void
matrix23Set(struct matrix23 *m,
            int m00, int m01, int m02,
            int m10, int m11, int m12)
{
   m->m00 = m00;   m->m01 = m01;   m->m02 = m02;
   m->m10 = m10;   m->m11 = m11;   m->m12 = m12;
}


/*
 * Transform (x,y) coordinate by the given matrix.
 */
static void
matrix23TransformCoordf(const struct matrix23 *m, float *x, float *y)
{
   const float x0 = *x;
   const float y0 = *y;

   *x = m->m00 * x0 + m->m01 * y0 + m->m02;
   *y = m->m10 * x0 + m->m11 * y0 + m->m12;
}

/*
 * Make rotation matrix for width X height screen.
 */
static void
matrix23Rotate(struct matrix23 *m, int width, int height, int angle)
{
   switch (angle) {
   case 0:
      matrix23Set(m, 1, 0, 0, 0, 1, 0);
      break;
   case 90:
      matrix23Set(m, 0, 1, 0,  -1, 0, width);
      break;
   case 180:
      matrix23Set(m, -1, 0, width,  0, -1, height);
      break;
   case 270:
      matrix23Set(m, 0, -1, height,  1, 0, 0);
      break;
   default:
      break;
   }
}

/* Doesn't matter on the order for our purposes */
typedef struct {
   unsigned char red, green, blue, alpha;
} intel_color_t;

/* Vertex format */
typedef union {
   struct {
      float x, y, z, w;
      intel_color_t color;
      intel_color_t specular;
      float u0, v0;
      float u1, v1;
      float u2, v2;
      float u3, v3;
   } v;
   float f[24];
   unsigned int  ui[24];
   unsigned char ub4[24][4];
} intelVertex, *intelVertexPtr;

static void draw_poly(CARD32 *vb,
                      float verts[][2],
                      float texcoords[][2])
{
   int vertex_size = 8;
   intelVertex tmp;
   int i, k;

   /* initial constant vertex fields */
   tmp.v.z = 1.0;
   tmp.v.w = 1.0; 
   tmp.v.color.red = 255;
   tmp.v.color.green = 255;
   tmp.v.color.blue = 255;
   tmp.v.color.alpha = 255;
   tmp.v.specular.red = 0;
   tmp.v.specular.green = 0;
   tmp.v.specular.blue = 0;
   tmp.v.specular.alpha = 0;

   for (k = 0; k < 4; k++) {
      tmp.v.x = verts[k][0];
      tmp.v.y = verts[k][1];
      tmp.v.u0 = texcoords[k][0];
      tmp.v.v0 = texcoords[k][1];

      for (i = 0 ; i < vertex_size ; i++)
         vb[i] = tmp.ui[i];

      vb += vertex_size;
   }
}


/* Our PS kernel uses less than 32 GRF registers (about 20) */
#define PS_KERNEL_NUM_GRF   32
#define PS_MAX_THREADS	   32

#define BRW_GRF_BLOCKS(nreg)	((nreg + 15) / 16 - 1)

static const CARD32 ps_kernel_static0[][4] = {
#include "rotation_wm_prog0.h"
};

static const CARD32 ps_kernel_static90[][4] = {
#include "rotation_wm_prog90.h"
};

#define ALIGN(i,m)    (((i) + (m) - 1) & ~((m) - 1))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define BRW_LINEAR_EXTRA (32*1024)
#define WM_BINDING_TABLE_ENTRIES    2

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
  
#define SF_KERNEL_NUM_GRF  16
#define SF_MAX_THREADS	   1

static const CARD32 sf_kernel_static0[][4] = {
#include "rotation_sf_prog0.h"
};


static const CARD32 sf_kernel_static90[][4] = {
#include "rotation_sf_prog90.h"
};

static void
I965UpdateRotate (ScreenPtr      pScreen,
                 shadowBufPtr   pBuf)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   ScrnInfoPtr pScrn1 = pScrn;
   I830Ptr pI8301 = NULL;
   RegionPtr	damage = shadowDamage(pBuf);
   int		nbox = REGION_NUM_RECTS (damage);
   BoxPtr	pbox = REGION_RECTS (damage);
   int		box_x1, box_x2, box_y1, box_y2;
   float verts[4][2];
   struct matrix23 rotMatrix;
   Bool updateInvarient = FALSE;
#ifdef XF86DRI
   drmI830Sarea *sarea = NULL;
   drm_context_t myContext = 0;
#endif
   Bool didLock = FALSE;

/* Gen4 states */
   int urb_vs_start, urb_vs_size;
   int urb_gs_start, urb_gs_size;
   int urb_clip_start, urb_clip_size;
   int urb_sf_start, urb_sf_size;
   int urb_cs_start, urb_cs_size;
   struct brw_surface_state *dest_surf_state;
   struct brw_surface_state *src_surf_state;
   struct brw_sampler_state *src_sampler_state;
   struct brw_vs_unit_state *vs_state;
   struct brw_sf_unit_state *sf_state;
   struct brw_wm_unit_state *wm_state;
   struct brw_cc_unit_state *cc_state;
   struct brw_cc_viewport *cc_viewport;
   struct brw_instruction *sf_kernel;
   struct brw_instruction *ps_kernel;
   struct brw_instruction *sip_kernel;
   float *vb;  
   BOOL first_output = TRUE;
   CARD32 *binding_table;
   int dest_surf_offset, src_surf_offset, src_sampler_offset, vs_offset;
   int sf_offset, wm_offset, cc_offset, vb_offset, cc_viewport_offset;
   int wm_scratch_offset;
   int sf_kernel_offset, ps_kernel_offset, sip_kernel_offset;
   int binding_table_offset;
   int next_offset, total_state_size;
   int vb_size = (4 * 4) * 4; /* 4 DWORDS per vertex */
   char *state_base;
   int state_base_offset;

   DPRINTF(PFX, "I965UpdateRotate: from (%d x %d) -> (%d x %d)\n",	
		pScrn->virtualX, pScrn->virtualY, pScreen->width, pScreen->height);

   if (I830IsPrimary(pScrn)) {
      pI8301 = pI830;
   } else {
      pI8301 = I830PTR(pI830->entityPrivate->pScrn_1);
      pScrn1 = pI830->entityPrivate->pScrn_1;
   }

   switch (pI830->rotation) {
      case RR_Rotate_90:
         matrix23Rotate(&rotMatrix,
                     pScreen->width, pScreen->height,
                     90);
	 break;
      case RR_Rotate_180:
         matrix23Rotate(&rotMatrix,
                     pScreen->width, pScreen->height,
                     180);
	 break;
      case RR_Rotate_270:
         matrix23Rotate(&rotMatrix,
                     pScreen->width, pScreen->height,
                     270);
	 break;
      default:
	 break;
   }

#ifdef XF86DRI
   if (pI8301->directRenderingEnabled) {
      sarea = DRIGetSAREAPrivate(pScrn1->pScreen);
      myContext = DRIGetContext(pScrn1->pScreen);
      didLock = I830DRILock(pScrn1);
   }
#endif

   if (pScrn->scrnIndex != *pI830->used3D) 
      updateInvarient = TRUE;
 
#ifdef XF86DRI
   if (sarea && sarea->ctxOwner != myContext)
      updateInvarient = TRUE;
#endif

   /*XXX we'll always update state */
   *pI830->used3D = pScrn->scrnIndex;
#ifdef XF86DRI
   if (sarea)
      sarea->ctxOwner = myContext;
#endif

   /* this starts initialize 3D engine for rotation mapping*/
   next_offset = 0;

   /* Set up our layout of state in framebuffer.  First the general state: */
   vs_offset = ALIGN(next_offset, 64);
   next_offset = vs_offset + sizeof(*vs_state);
   sf_offset = ALIGN(next_offset, 32);
   next_offset = sf_offset + sizeof(*sf_state);
   wm_offset = ALIGN(next_offset, 32);
   next_offset = wm_offset + sizeof(*wm_state);
   wm_scratch_offset = ALIGN(next_offset, 1024);
   next_offset = wm_scratch_offset + 1024 * PS_MAX_THREADS;
   cc_offset = ALIGN(next_offset, 32);
   next_offset = cc_offset + sizeof(*cc_state);

   sf_kernel_offset = ALIGN(next_offset, 64);
      
   switch (pI830->rotation) {
       case RR_Rotate_90:
       case RR_Rotate_270:
      	    next_offset = sf_kernel_offset + sizeof (sf_kernel_static90);
      	    ps_kernel_offset = ALIGN(next_offset, 64);
      	    next_offset = ps_kernel_offset + sizeof (ps_kernel_static90);
	    break;
       case RR_Rotate_180:
       default:
      	    next_offset = sf_kernel_offset + sizeof (sf_kernel_static0);
      	    ps_kernel_offset = ALIGN(next_offset, 64);
      	    next_offset = ps_kernel_offset + sizeof (ps_kernel_static0);
	    break;
   }

   sip_kernel_offset = ALIGN(next_offset, 64);
   next_offset = sip_kernel_offset + sizeof (sip_kernel_static);
   cc_viewport_offset = ALIGN(next_offset, 32);
   next_offset = cc_viewport_offset + sizeof(*cc_viewport);

   src_sampler_offset = ALIGN(next_offset, 32);
   next_offset = src_sampler_offset + sizeof(*src_sampler_state);

   /* Align VB to native size of elements, for safety */
   vb_offset = ALIGN(next_offset, 8);
   next_offset = vb_offset + vb_size;

   dest_surf_offset = ALIGN(next_offset, 32);
   next_offset = dest_surf_offset + sizeof(*dest_surf_state);
   src_surf_offset = ALIGN(next_offset, 32);
   next_offset = src_surf_offset + sizeof(*src_surf_state);
   binding_table_offset = ALIGN(next_offset, 32);
   next_offset = binding_table_offset + (WM_BINDING_TABLE_ENTRIES * 4);

   total_state_size = next_offset;
   assert (total_state_size < BRW_LINEAR_EXTRA);

   state_base_offset = pI830->RotateStateMem.Start;
   state_base_offset = ALIGN(state_base_offset, 64);
   state_base = (char *)(pI830->FbBase + state_base_offset);
   DPRINTF(PFX, "rotate state buffer start 0x%x, addr 0x%x, base 0x%x\n",
			pI830->RotateStateMem.Start, state_base, pI830->FbBase);

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
   src_sampler_state = (void *)(state_base + src_sampler_offset);
   binding_table = (void *)(state_base + binding_table_offset);
   vb = (void *)(state_base + vb_offset);

   /* For 3D, the VS must have 8, 12, 16, 24, or 32 VUEs allocated to it.
    * A VUE consists of a 256-bit vertex header followed by the vertex data,
    * which in our case is 4 floats (128 bits), thus a single 512-bit URB
    * entry.
    */
#define URB_VS_ENTRIES	      8
#define URB_VS_ENTRY_SIZE     1
   
#define URB_GS_ENTRIES	      0
#define URB_GS_ENTRY_SIZE     0
   
#define URB_CLIP_ENTRIES      0
#define URB_CLIP_ENTRY_SIZE   0
   
   /* The SF kernel we use outputs only 4 256-bit registers, leading to an
    * entry size of 2 512-bit URBs.  We don't need to have many entries to
    * output as we're generally working on large rectangles and don't care
    * about having WM threads running on different rectangles simultaneously.
    */
#define URB_SF_ENTRIES	      1
#define URB_SF_ENTRY_SIZE     2

#define URB_CS_ENTRIES	      0
#define URB_CS_ENTRY_SIZE     0
   
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

   memset (cc_viewport, 0, sizeof (*cc_viewport));
   cc_viewport->min_depth = -1.e35;
   cc_viewport->max_depth = 1.e35;

   memset(cc_state, 0, sizeof(*cc_state));
   cc_state->cc0.stencil_enable = 0;   /* disable stencil */
   cc_state->cc2.depth_test = 0;       /* disable depth test */
   cc_state->cc2.logicop_enable = 1;   /* enable logic op */
   cc_state->cc3.ia_blend_enable = 1;  /* blend alpha just like colors */
   cc_state->cc3.blend_enable = 0;     /* disable color blend */
   cc_state->cc3.alpha_test = 0;       /* disable alpha test */
   cc_state->cc4.cc_viewport_state_offset = (state_base_offset + cc_viewport_offset) >> 5;
   cc_state->cc5.dither_enable = 0;    /* disable dither */
   cc_state->cc5.logicop_func = 0xc;   /* COPY S*/
   cc_state->cc5.statistics_enable = 1;
   cc_state->cc5.ia_blend_function = BRW_BLENDFUNCTION_ADD;
   cc_state->cc5.ia_src_blend_factor = BRW_BLENDFACTOR_ONE;
   cc_state->cc5.ia_dest_blend_factor = BRW_BLENDFACTOR_ZERO;

   /* Upload system kernel */
   memcpy (sip_kernel, sip_kernel_static, sizeof (sip_kernel_static));
   
   memset(dest_surf_state, 0, sizeof(*dest_surf_state));
   dest_surf_state->ss0.surface_type = BRW_SURFACE_2D;
   dest_surf_state->ss0.data_return_format = BRW_SURFACERETURNFORMAT_FLOAT32;
   if (pI8301->cpp == 2)
      dest_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_B5G6R5_UNORM;
   else
      dest_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_B8G8R8A8_UNORM;
   dest_surf_state->ss0.writedisable_alpha = 0;
   dest_surf_state->ss0.writedisable_red = 0;
   dest_surf_state->ss0.writedisable_green = 0;
   dest_surf_state->ss0.writedisable_blue = 0;
   dest_surf_state->ss0.color_blend = 0;
   dest_surf_state->ss0.vert_line_stride = 0;
   dest_surf_state->ss0.vert_line_stride_ofs = 0;
   dest_surf_state->ss0.mipmap_layout_mode = 0;
   dest_surf_state->ss0.render_cache_read_mode = 0;
   
   if (I830IsPrimary(pScrn))
      dest_surf_state->ss1.base_addr = pI830->FrontBuffer.Start;
   else 
      dest_surf_state->ss1.base_addr = pI8301->FrontBuffer2.Start;
   dest_surf_state->ss2.width = pScrn->virtualX - 1;
   dest_surf_state->ss2.height = pScrn->virtualY - 1; 
   dest_surf_state->ss2.mip_count = 0;
   dest_surf_state->ss2.render_target_rotation = 0; /*XXX how to use? */
   dest_surf_state->ss3.pitch = (pI830->displayWidth * pI830->cpp) - 1;
   if (pI830->front_tiled) {
      dest_surf_state->ss3.tiled_surface = 1;
      dest_surf_state->ss3.tile_walk = 0; /* X major */
   }

   memset(src_surf_state, 0, sizeof(*src_surf_state));
   src_surf_state->ss0.surface_type = BRW_SURFACE_2D;
/* src_surf_state->ss0.data_return_format = BRW_SURFACERETURNFORMAT_FLOAT32;*/
   if (pI8301->cpp == 2) 
      src_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_B5G6R5_UNORM;
   else 
      src_surf_state->ss0.surface_format = BRW_SURFACEFORMAT_B8G8R8A8_UNORM;
   src_surf_state->ss0.writedisable_alpha = 0;
   src_surf_state->ss0.writedisable_red = 0;
   src_surf_state->ss0.writedisable_green = 0;
   src_surf_state->ss0.writedisable_blue = 0;
   src_surf_state->ss0.color_blend = 0;
   src_surf_state->ss0.vert_line_stride = 0;
   src_surf_state->ss0.vert_line_stride_ofs = 0;
   src_surf_state->ss0.mipmap_layout_mode = 0;
   src_surf_state->ss0.render_cache_read_mode = 0;
  
   if (I830IsPrimary(pScrn)) 
      src_surf_state->ss1.base_addr = pI830->RotatedMem.Start;
   else 
      src_surf_state->ss1.base_addr = pI8301->RotatedMem2.Start;
   src_surf_state->ss2.width = pScreen->width - 1;
   src_surf_state->ss2.height = pScreen->height - 1;
   src_surf_state->ss2.mip_count = 0;
   src_surf_state->ss2.render_target_rotation = 0;
   src_surf_state->ss3.pitch = (pScrn->displayWidth * pI830->cpp) - 1;
   if (pI830->rotated_tiled) {
      src_surf_state->ss3.tiled_surface = 1;
      src_surf_state->ss3.tile_walk = 0; /* X major */
   }

   binding_table[0] = state_base_offset + dest_surf_offset;
   binding_table[1] = state_base_offset + src_surf_offset;

   memset(src_sampler_state, 0, sizeof(*src_sampler_state));
   src_sampler_state->ss0.min_filter = BRW_MAPFILTER_LINEAR;
   src_sampler_state->ss0.mag_filter = BRW_MAPFILTER_LINEAR;
   src_sampler_state->ss1.r_wrap_mode = BRW_TEXCOORDMODE_CLAMP;
   src_sampler_state->ss1.s_wrap_mode = BRW_TEXCOORDMODE_CLAMP;
   src_sampler_state->ss1.t_wrap_mode = BRW_TEXCOORDMODE_CLAMP;

   /* Set up the vertex shader to be disabled (passthrough) */
   memset(vs_state, 0, sizeof(*vs_state));
   vs_state->thread4.nr_urb_entries = URB_VS_ENTRIES;
   vs_state->thread4.urb_entry_allocation_size = URB_VS_ENTRY_SIZE - 1;
   vs_state->vs6.vs_enable = 0;
   vs_state->vs6.vert_cache_disable = 1;

   /* Set up the SF kernel to do coord interp: for each attribute,
    * calculate dA/dx and dA/dy.  Hand these interpolation coefficients
    * back to SF which then hands pixels off to WM.
    */

   switch (pI830->rotation) {
      case RR_Rotate_90:
      case RR_Rotate_270:
           memcpy (sf_kernel, sf_kernel_static90, sizeof (sf_kernel_static90));
           memcpy (ps_kernel, ps_kernel_static90, sizeof (ps_kernel_static90));
	   break;
      case RR_Rotate_180:
      default:
           memcpy (sf_kernel, sf_kernel_static0, sizeof (sf_kernel_static0));
           memcpy (ps_kernel, ps_kernel_static0, sizeof (ps_kernel_static0));
	   break;
   }

   memset(sf_state, 0, sizeof(*sf_state));
   sf_state->thread0.kernel_start_pointer = 
	          (state_base_offset + sf_kernel_offset) >> 6;
   sf_state->thread0.grf_reg_count = BRW_GRF_BLOCKS(SF_KERNEL_NUM_GRF);
   sf_state->sf1.single_program_flow = 1; /* XXX */
   sf_state->sf1.binding_table_entry_count = 0;
   sf_state->sf1.thread_priority = 0;
   sf_state->sf1.floating_point_mode = 0; 
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

   memset (wm_state, 0, sizeof (*wm_state));
   wm_state->thread0.kernel_start_pointer = 
	       (state_base_offset + ps_kernel_offset) >> 6;
   wm_state->thread0.grf_reg_count = BRW_GRF_BLOCKS(PS_KERNEL_NUM_GRF);
   wm_state->thread1.single_program_flow = 1; /* XXX */
   wm_state->thread1.binding_table_entry_count = 2;
   /* Though we never use the scratch space in our WM kernel, it has to be
    * set, and the minimum allocation is 1024 bytes.
    */
   wm_state->thread2.scratch_space_base_pointer = (state_base_offset +
						   wm_scratch_offset) >> 10;
   wm_state->thread2.per_thread_scratch_space = 0; /* 1024 bytes */
   wm_state->thread3.dispatch_grf_start_reg = 3;
   wm_state->thread3.const_urb_entry_read_length = 0;
   wm_state->thread3.const_urb_entry_read_offset = 0;
   wm_state->thread3.urb_entry_read_length = 1;
   wm_state->thread3.urb_entry_read_offset = 0;
   wm_state->wm4.stats_enable = 1;
   wm_state->wm4.sampler_state_pointer = (state_base_offset + src_sampler_offset) >> 5;
   wm_state->wm4.sampler_count = 1; /* 1-4 samplers used */
   wm_state->wm5.max_threads = PS_MAX_THREADS - 1;
   wm_state->wm5.thread_dispatch_enable = 1;
   wm_state->wm5.enable_16_pix = 1;
   wm_state->wm5.enable_8_pix = 0;
   wm_state->wm5.early_depth_test = 1;

   
   {
         BEGIN_LP_RING(2);
         OUT_RING(MI_FLUSH | 
	          MI_STATE_INSTRUCTION_CACHE_FLUSH |
	          BRW_MI_GLOBAL_SNAPSHOT_RESET);
         OUT_RING(MI_NOOP);
         ADVANCE_LP_RING();
    }

    {
         BEGIN_LP_RING(12);
         OUT_RING(BRW_PIPELINE_SELECT | PIPELINE_SELECT_3D);

   /* Mesa does this. Who knows... */
         OUT_RING(BRW_CS_URB_STATE | 0);
         OUT_RING((0 << 4) |	/* URB Entry Allocation Size */
	          (0 << 0));	/* Number of URB Entries */
   
   /* Zero out the two base address registers so all offsets are absolute */
         OUT_RING(BRW_STATE_BASE_ADDRESS | 4);
         OUT_RING(0 | BASE_ADDRESS_MODIFY);  /* Generate state base address */
         OUT_RING(0 | BASE_ADDRESS_MODIFY);  /* Surface state base address */
         OUT_RING(0 | BASE_ADDRESS_MODIFY);  /* media base addr, don't care */
         OUT_RING(0x10000000 | BASE_ADDRESS_MODIFY);  /* general state max addr, disabled */
         OUT_RING(0x10000000 | BASE_ADDRESS_MODIFY);  /* media object state max addr, disabled */

   /* Set system instruction pointer */
         OUT_RING(BRW_STATE_SIP | 0);
         OUT_RING(state_base_offset + sip_kernel_offset); /* system instruction pointer */
      
         OUT_RING(MI_NOOP);
         ADVANCE_LP_RING(); 
    }
   

    { 
         BEGIN_LP_RING(36);
   /* Enable VF statistics */
         OUT_RING(BRW_3DSTATE_VF_STATISTICS | 1);
   
   /* Pipe control */
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
   
   /* XXX: Blend constant color (magenta is fun) */
         //OUT_RING(BRW_3DSTATE_CONSTANT_COLOR | 3);
         //OUT_RING(float_to_uint (1.0));
         //OUT_RING(float_to_uint (0.0));
         //OUT_RING(float_to_uint (1.0));
         //OUT_RING(float_to_uint (1.0));
   
   /* The drawing rectangle clipping is always on.  Set it to values that
    * shouldn't do any clipping.
    */
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
         OUT_RING(BRW_CS_URB_STATE | 0);
         OUT_RING(((URB_CS_ENTRY_SIZE - 1) << 4) | /* URB Entry Allocation Size */
	          (URB_CS_ENTRIES << 0));	     /* Number of URB Entries */
   
   /* Set up the pointer to our vertex buffer */
         OUT_RING(BRW_3DSTATE_VERTEX_BUFFERS | 2);
         OUT_RING((0 << VB0_BUFFER_INDEX_SHIFT) |
	          VB0_VERTEXDATA |
	          ((4 * 4) << VB0_BUFFER_PITCH_SHIFT)); /* four 32-bit floats per vertex */
         OUT_RING(state_base_offset + vb_offset);
         OUT_RING(3); /* four corners to our rectangle */

   /* Set up our vertex elements, sourced from the single vertex buffer. */
         OUT_RING(BRW_3DSTATE_VERTEX_ELEMENTS | 3);
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

         //OUT_RING(MI_NOOP);			/* pad to quadword */
         ADVANCE_LP_RING(); 
   }

   {
      BEGIN_LP_RING(2);
      OUT_RING(MI_FLUSH | 
	       MI_STATE_INSTRUCTION_CACHE_FLUSH |
	       BRW_MI_GLOBAL_SNAPSHOT_RESET);
      OUT_RING(MI_NOOP);
      ADVANCE_LP_RING();
   }

   while (nbox--)
   {
      float src_scale_x, src_scale_y;
      int i;
      box_x1 = pbox->x1;
      box_y1 = pbox->y1;
      box_x2 = pbox->x2;
      box_y2 = pbox->y2;

      if (!first_output) {
	 /* Since we use the same little vertex buffer over and over, sync for
	  * subsequent rectangles.
	  */
	 if (pI830->AccelInfoRec && pI830->AccelInfoRec->NeedToSync) {
	    (*pI830->AccelInfoRec->Sync)(pScrn);
	    pI830->AccelInfoRec->NeedToSync = FALSE;
	 }
      }

      pbox++;

      verts[0][0] = box_x1; verts[0][1] = box_y1;
      verts[1][0] = box_x2; verts[1][1] = box_y1;
      verts[2][0] = box_x2; verts[2][1] = box_y2;
      verts[3][0] = box_x1; verts[3][1] = box_y2;

      /* transform coordinates to rotated versions, but leave texcoords unchanged */
      for (i = 0; i < 4; i++)
         matrix23TransformCoordf(&rotMatrix, &verts[i][0], &verts[i][1]);

      src_scale_x = (float)1.0 / (float)pScreen->width;
      src_scale_y = (float)1.0 / (float)pScreen->height;
      i = 0;

      DPRINTF(PFX, "box size (%d, %d) -> (%d, %d)\n", 
			box_x1, box_y1, box_x2, box_y2);

      switch (pI830->rotation) {
         case RR_Rotate_90:
      	    vb[i++] = (float)box_x1 * src_scale_x;
      	    vb[i++] = (float)box_y2 * src_scale_y;
      	    vb[i++] = verts[3][0];
      	    vb[i++] = verts[3][1];

      	    vb[i++] = (float)box_x1 * src_scale_x;
      	    vb[i++] = (float)box_y1 * src_scale_y;
      	    vb[i++] = verts[0][0];
      	    vb[i++] = verts[0][1];

      	    vb[i++] = (float)box_x2 * src_scale_x;
      	    vb[i++] = (float)box_y1 * src_scale_y;
      	    vb[i++] = verts[1][0];
      	    vb[i++] = verts[1][1];
	    break;
         case RR_Rotate_270:
      	    vb[i++] = (float)box_x2 * src_scale_x;
      	    vb[i++] = (float)box_y1 * src_scale_y;
      	    vb[i++] = verts[1][0];
      	    vb[i++] = verts[1][1];

      	    vb[i++] = (float)box_x2 * src_scale_x;
      	    vb[i++] = (float)box_y2 * src_scale_y;
      	    vb[i++] = verts[2][0];
      	    vb[i++] = verts[2][1];

      	    vb[i++] = (float)box_x1 * src_scale_x;
      	    vb[i++] = (float)box_y2 * src_scale_y;
      	    vb[i++] = verts[3][0];
      	    vb[i++] = verts[3][1];
	    break;
	 case RR_Rotate_180:
       	 default:
      	    vb[i++] = (float)box_x1 * src_scale_x;
      	    vb[i++] = (float)box_y1 * src_scale_y;
      	    vb[i++] = verts[0][0];
      	    vb[i++] = verts[0][1];

            vb[i++] = (float)box_x2 * src_scale_x;
      	    vb[i++] = (float)box_y1 * src_scale_y;
      	    vb[i++] = verts[1][0];
      	    vb[i++] = verts[1][1];

      	    vb[i++] = (float)box_x2 * src_scale_x;
      	    vb[i++] = (float)box_y2 * src_scale_y;
      	    vb[i++] = verts[2][0];
      	    vb[i++] = verts[2][1];
	    break;
      }

      BEGIN_LP_RING(6);
      OUT_RING(BRW_3DPRIMITIVE | 
	       BRW_3DPRIMITIVE_VERTEX_SEQUENTIAL |
	       (_3DPRIM_RECTLIST << BRW_3DPRIMITIVE_TOPOLOGY_SHIFT) | 
	       (0 << 9) |  /* CTG - indirect vertex count */
	       4);
      OUT_RING(3); /* vertex count per instance */
      OUT_RING(0); /* start vertex offset */
      OUT_RING(1); /* single instance */
      OUT_RING(0); /* start instance location */
      OUT_RING(0); /* index buffer offset, ignored */
      ADVANCE_LP_RING();

      first_output = FALSE;
      if (pI830->AccelInfoRec)
	 pI830->AccelInfoRec->NeedToSync = TRUE;
   }

   if (pI830->AccelInfoRec)
      (*pI830->AccelInfoRec->Sync)(pScrn);
#ifdef XF86DRI
   if (didLock)
      I830DRIUnlock(pScrn1);
#endif
}


static void
I915UpdateRotate (ScreenPtr      pScreen,
                 shadowBufPtr   pBuf)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   ScrnInfoPtr pScrn1 = pScrn;
   I830Ptr pI8301 = NULL;
   RegionPtr	damage = shadowDamage(pBuf);
   int		nbox = REGION_NUM_RECTS (damage);
   BoxPtr		pbox = REGION_RECTS (damage);
   int		box_x1, box_x2, box_y1, box_y2;
   CARD32	vb[32];	/* 32 dword vertex buffer */
   float verts[4][2], tex[4][2];
   struct matrix23 rotMatrix;
   int j;
   int use_fence;
   Bool didLock = FALSE;

   if (I830IsPrimary(pScrn)) {
      pI8301 = pI830;
   } else {
      pI8301 = I830PTR(pI830->entityPrivate->pScrn_1);
      pScrn1 = pI830->entityPrivate->pScrn_1;
   }

   switch (pI830->rotation) {
      case RR_Rotate_90:
         matrix23Rotate(&rotMatrix,
                     pScreen->width, pScreen->height,
                     90);
	 break;
      case RR_Rotate_180:
         matrix23Rotate(&rotMatrix,
                     pScreen->width, pScreen->height,
                     180);
	 break;
      case RR_Rotate_270:
         matrix23Rotate(&rotMatrix,
                     pScreen->width, pScreen->height,
                     270);
	 break;
      default:
	 break;
   }

#ifdef XF86DRI
   if (pI8301->directRenderingEnabled)
      didLock = I830DRILock(pScrn1);
#endif

   /* If another screen was active, we don't know the current state. */
   if (pScrn->scrnIndex != *pI830->used3D)
      pI830->last_3d = LAST_3D_OTHER;

   if (pI830->last_3d != LAST_3D_ROTATION) {
      FS_LOCALS(3);
      *pI830->used3D = pScrn->scrnIndex;

      BEGIN_LP_RING(34);
      /* invarient state */

      /* flush map & render cache */
      OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
      OUT_RING(0x00000000);

      /* draw rect */
      OUT_RING(_3DSTATE_DRAW_RECT_CMD);
      OUT_RING(DRAW_DITHER_OFS_X(0) | DRAW_DITHER_OFS_Y(0));
      OUT_RING(DRAW_XMIN(0) | DRAW_YMIN(0));
      OUT_RING(DRAW_XMAX(pScrn->virtualX - 1) |
	       DRAW_YMAX(pScrn->virtualY - 1));
      OUT_RING(DRAW_XORG(0) | DRAW_YORG(0));

      OUT_RING(MI_NOOP);

      OUT_RING(0x7c000003); /* XXX: magic numbers */
      OUT_RING(0x7d070000);
      OUT_RING(0x00000000);
      OUT_RING(0x68000002);

      OUT_RING(_3DSTATE_LOAD_STATE_IMMEDIATE_1 |
	       I1_LOAD_S(2) | I1_LOAD_S(4) | I1_LOAD_S(5) | I1_LOAD_S(6) | 3);

      OUT_RING(S2_TEXCOORD_FMT(0, TEXCOORDFMT_2D) |
	       S2_TEXCOORD_FMT(1, TEXCOORDFMT_NOT_PRESENT) |
	       S2_TEXCOORD_FMT(2, TEXCOORDFMT_NOT_PRESENT) |
	       S2_TEXCOORD_FMT(3, TEXCOORDFMT_NOT_PRESENT) |
	       S2_TEXCOORD_FMT(4, TEXCOORDFMT_NOT_PRESENT) |
	       S2_TEXCOORD_FMT(5, TEXCOORDFMT_NOT_PRESENT) |
	       S2_TEXCOORD_FMT(6, TEXCOORDFMT_NOT_PRESENT) |
	       S2_TEXCOORD_FMT(7, TEXCOORDFMT_NOT_PRESENT));
      OUT_RING((1 << S4_POINT_WIDTH_SHIFT) | S4_LINE_WIDTH_ONE |
	       S4_CULLMODE_NONE | S4_VFMT_SPEC_FOG | S4_VFMT_COLOR |
	       S4_VFMT_XYZW);
      OUT_RING(0x00000000); /* S5 -- enable bits */
      OUT_RING((2 << S6_DEPTH_TEST_FUNC_SHIFT) |
	       (2 << S6_CBUF_SRC_BLEND_FACT_SHIFT) |
	       (1 << S6_CBUF_DST_BLEND_FACT_SHIFT) | S6_COLOR_WRITE_ENABLE |
	       (2 << S6_TRISTRIP_PV_SHIFT));

      OUT_RING(_3DSTATE_CONST_BLEND_COLOR_CMD);
      OUT_RING(0x00000000);

      OUT_RING(_3DSTATE_DST_BUF_VARS_CMD);
      if (pI830->cpp == 1) {
	 OUT_RING(LOD_PRECLAMP_OGL | DSTORG_HORT_BIAS(0x8) |
		  DSTORG_VERT_BIAS(0x8) | COLR_BUF_8BIT);
      } else if (pI830->cpp == 2) {
	 OUT_RING(LOD_PRECLAMP_OGL | DSTORG_HORT_BIAS(0x8) |
		  DSTORG_VERT_BIAS(0x8) | COLR_BUF_RGB565);
      } else {
	 OUT_RING(LOD_PRECLAMP_OGL | DSTORG_HORT_BIAS(0x8) |
		  DSTORG_VERT_BIAS(0x8) | COLR_BUF_ARGB8888 |
		  DEPTH_FRMT_24_FIXED_8_OTHER);
      }

      /* texture sampler state */
      OUT_RING(_3DSTATE_SAMPLER_STATE | 3);
      OUT_RING(0x00000001);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);

      /* front buffer, pitch, offset */
      OUT_RING(_3DSTATE_BUF_INFO_CMD);
      OUT_RING(BUF_3D_ID_COLOR_BACK | BUF_3D_USE_FENCE |
	       BUF_3D_PITCH(pI830->displayWidth * pI830->cpp));
      if (I830IsPrimary(pScrn))
         OUT_RING(pI830->FrontBuffer.Start);
      else 
         OUT_RING(pI8301->FrontBuffer2.Start);

      /* Set the entire frontbuffer up as a texture */
      OUT_RING(_3DSTATE_MAP_STATE | 3);
      OUT_RING(0x00000001);

      if (I830IsPrimary(pScrn)) 
         OUT_RING(pI830->RotatedMem.Start);
      else 
	 OUT_RING(pI8301->RotatedMem2.Start);

      if (pI830->disableTiling)
         use_fence = 0;
      else
         use_fence = MS3_USE_FENCE_REGS;
      
      if (pI830->cpp == 1)
	 use_fence |= MAPSURF_8BIT;
      else
      if (pI830->cpp == 2)
	 use_fence |= MAPSURF_16BIT;
      else
	 use_fence |= MAPSURF_32BIT;
      OUT_RING(use_fence | (pScreen->height - 1) << 21 | (pScreen->width - 1) << 10);
      OUT_RING(((((pScrn->displayWidth * pI830->cpp) / 4) - 1) << 21));
      ADVANCE_LP_RING();

      /* fragment program - texture blend replace*/
      FS_BEGIN();
      i915_fs_dcl(FS_S0);
      i915_fs_dcl(FS_T0);
      i915_fs_texld(FS_OC, FS_S0, FS_T0);
      FS_END();
   }
   
   {
      BEGIN_LP_RING(2);
      OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
      OUT_RING(0x00000000);
      ADVANCE_LP_RING();
   }

   while (nbox--)
   {
      box_x1 = pbox->x1;
      box_y1 = pbox->y1;
      box_x2 = pbox->x2;
      box_y2 = pbox->y2;
      pbox++;

      BEGIN_LP_RING(40);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);

      /* vertex data */
      OUT_RING(PRIM3D_INLINE | PRIM3D_TRIFAN | (32 - 1));
      verts[0][0] = box_x1; verts[0][1] = box_y1;
      verts[1][0] = box_x2; verts[1][1] = box_y1;
      verts[2][0] = box_x2; verts[2][1] = box_y2;
      verts[3][0] = box_x1; verts[3][1] = box_y2;
      tex[0][0] = box_x1; tex[0][1] = box_y1;
      tex[1][0] = box_x2; tex[1][1] = box_y1;
      tex[2][0] = box_x2; tex[2][1] = box_y2;
      tex[3][0] = box_x1; tex[3][1] = box_y2;

      /* transform coordinates to rotated versions, but leave texcoords unchanged */
      for (j = 0; j < 4; j++)
         matrix23TransformCoordf(&rotMatrix, &verts[j][0], &verts[j][1]);

      /* emit vertex buffer */
      draw_poly(vb, verts, tex);
      for (j = 0; j < 32; j++)
         OUT_RING(vb[j]);

      ADVANCE_LP_RING();
   }

#ifdef XF86DRI
   if (didLock)
      I830DRIUnlock(pScrn1);
#endif
}

static void
I830UpdateRotate (ScreenPtr      pScreen,
                 shadowBufPtr   pBuf)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   I830Ptr pI8301 = NULL;
   ScrnInfoPtr pScrn1 = pScrn;
   RegionPtr	damage = shadowDamage(pBuf);
   int		nbox = REGION_NUM_RECTS (damage);
   BoxPtr		pbox = REGION_RECTS (damage);
   int		box_x1, box_x2, box_y1, box_y2;
   CARD32	vb[32];	/* 32 dword vertex buffer */
   float verts[4][2], tex[4][2];
   struct matrix23 rotMatrix;
   int use_fence;
   int j;
   Bool didLock = FALSE;

   if (I830IsPrimary(pScrn)) {
      pI8301 = pI830;
   } else {
      pI8301 = I830PTR(pI830->entityPrivate->pScrn_1);
      pScrn1 = pI830->entityPrivate->pScrn_1;
   }

   switch (pI830->rotation) {
      case RR_Rotate_90:
         matrix23Rotate(&rotMatrix,
                     pScreen->width, pScreen->height,
                     90);
	 break;
      case RR_Rotate_180:
         matrix23Rotate(&rotMatrix,
                     pScreen->width, pScreen->height,
                     180);
	 break;
      case RR_Rotate_270:
         matrix23Rotate(&rotMatrix,
                     pScreen->width, pScreen->height,
                     270);
	 break;
      default:
	 break;
   }

#ifdef XF86DRI
   if (pI8301->directRenderingEnabled)
      didLock = I830DRILock(pScrn1);
#endif

   if (pScrn->scrnIndex != *pI830->used3D)
      pI830->last_3d = LAST_3D_OTHER;

   if (pI830->last_3d != LAST_3D_ROTATION) {
      *pI830->used3D = pScrn->scrnIndex;

      pI830->last_3d = LAST_3D_ROTATION;

      BEGIN_LP_RING(48);
      OUT_RING(0x682008a1);
      OUT_RING(0x6f402100);
      OUT_RING(0x62120aa9);
      OUT_RING(0x76b3ffff);
      OUT_RING(0x6c818a01);
      OUT_RING(0x6ba008a1);
      OUT_RING(0x69802100);
      OUT_RING(0x63a00aaa);
      OUT_RING(0x6423070e);
      OUT_RING(0x66014142);
      OUT_RING(0x75000000);
      OUT_RING(0x7d880000);
      OUT_RING(0x00000000);
      OUT_RING(0x650001c4);
      OUT_RING(0x6a000000);
      OUT_RING(0x7d020000);
      OUT_RING(0x0000ba98);

      /* flush map & render cache */
      OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
      OUT_RING(0x00000000);
      /* draw rect */
      OUT_RING(_3DSTATE_DRAW_RECT_CMD);
      OUT_RING(0x00000000);	/* flags */
      OUT_RING(0x00000000);	/* ymin, xmin */
      OUT_RING((pScrn->virtualX - 1) | (pScrn->virtualY - 1) << 16); /* ymax, xmax */
      OUT_RING(0x00000000);	/* yorigin, xorigin */
      OUT_RING(MI_NOOP);

      /* front buffer */
      OUT_RING(_3DSTATE_BUF_INFO_CMD);
      OUT_RING(0x03800000 | (((pI830->displayWidth * pI830->cpp) / 4) << 2));
      if (I830IsPrimary(pScrn))
	 OUT_RING(pI830->FrontBuffer.Start);
      else 
	 OUT_RING(pI8301->FrontBuffer2.Start);
      OUT_RING(0x7d850000);
      if (pI830->cpp == 1)
	 OUT_RING(0x00880000);
      else
	 if (pI830->cpp == 2)
	    OUT_RING(0x00880200);
	 else
	    OUT_RING(0x00880308);
      /* scissor */
      OUT_RING(0x7c800002);
      OUT_RING(0x7d810001);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      /* stipple */
      OUT_RING(0x7d830000);
      OUT_RING(0x00000000);

      /* texture blend replace */
      OUT_RING(0x7c088088);
      OUT_RING(0x00000000);
      OUT_RING(0x6d021181);
      OUT_RING(0x6d060101);
      OUT_RING(0x6e008046);
      OUT_RING(0x6e048046);


      /* Set the entire frontbuffer up as a texture */
      OUT_RING(0x7d030804);

      if (pI830->disableTiling)
         use_fence = 0;
      else
         use_fence = 2;

      if (I830IsPrimary(pScrn)) 
         OUT_RING(pI830->RotatedMem.Start | use_fence);
      else 
	 OUT_RING(pI8301->RotatedMem2.Start | use_fence);

      if (pI830->cpp == 1)
         OUT_RING(0x40 | (pScreen->height - 1) << 21 | (pScreen->width - 1) << 10);
      else if (pI830->cpp == 2)
         OUT_RING(0x80 | (pScreen->height - 1) << 21 | (pScreen->width - 1) << 10);
      else
         OUT_RING(0xc0 | (pScreen->height - 1) << 21 | (pScreen->width - 1) << 10);

      OUT_RING((((pScrn->displayWidth * pI830->cpp / 4) - 1) << 21));
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);


      ADVANCE_LP_RING();
   }

   {
      BEGIN_LP_RING(2);
      /* flush map & render cache */
      OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
      OUT_RING(0x00000000);
      ADVANCE_LP_RING();
   }

   while (nbox--)
   {
      box_x1 = pbox->x1;
      box_y1 = pbox->y1;
      box_x2 = pbox->x2;
      box_y2 = pbox->y2;
      pbox++;

      BEGIN_LP_RING(40);

      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);
      OUT_RING(MI_NOOP);

      /* vertex data */
      OUT_RING(0x7f0c001f);
      verts[0][0] = box_x1; verts[0][1] = box_y1;
      verts[1][0] = box_x2; verts[1][1] = box_y1;
      verts[2][0] = box_x2; verts[2][1] = box_y2;
      verts[3][0] = box_x1; verts[3][1] = box_y2;
      tex[0][0] = box_x1; tex[0][1] = box_y1;
      tex[1][0] = box_x2; tex[1][1] = box_y1;
      tex[2][0] = box_x2; tex[2][1] = box_y2;
      tex[3][0] = box_x1; tex[3][1] = box_y2;

      /* transform coordinates to rotated versions, but leave texcoords unchanged */
      for (j = 0; j < 4; j++)
         matrix23TransformCoordf(&rotMatrix, &verts[j][0], &verts[j][1]);

      /* emit vertex buffer */
      draw_poly(vb, verts, tex);
      for (j = 0; j < 32; j++)
         OUT_RING(vb[j]);

      OUT_RING(0x05000000);
      OUT_RING(0x00000000);

      ADVANCE_LP_RING();
   }

   {
      BEGIN_LP_RING(2);
      /* flush map & render cache */
      OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
      OUT_RING(0x00000000);
      ADVANCE_LP_RING();
   }

#ifdef XF86DRI
   if (didLock)
      I830DRIUnlock(pScrn1);
#endif
}

Bool
I830Rotate(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
   I830Ptr pI830 = I830PTR(pScrn);
   I830Ptr pI8301 = NULL;
   I830Ptr pI8302 = NULL;
   ScrnInfoPtr pScrn1 = NULL;
   ScrnInfoPtr pScrn2 = NULL;
   int i;
   ShadowUpdateProc func = NULL;
   Rotation oldRotation = pI830->rotation; /* save old state */
   int displayWidth = pScrn->displayWidth; /* save displayWidth */
   Bool reAllocate = TRUE;
   Bool didLock = FALSE;

   /* Good pitches to allow tiling.  Don't care about pitches < 1024. */
   static const int pitches[] = {
/*
	 128 * 2,
	 128 * 4,
*/
	 128 * 8,
	 128 * 16,
	 128 * 32,
	 128 * 64,
	 0
   };

   if (pI830->noAccel)
      func = LoaderSymbol("shadowUpdateRotatePacked");
   else {
      if (IS_I9XX(pI830)) {
	 if (IS_I965G(pI830))
	     func = I965UpdateRotate;
	 else 
	     func = I915UpdateRotate;
      } else
	 func = I830UpdateRotate;
   }

   if (I830IsPrimary(pScrn)) {
      pI8301 = pI830;
      pScrn1 = pScrn;
      if (pI830->entityPrivate) {
         pI8302 = I830PTR(pI830->entityPrivate->pScrn_2);
         pScrn2 = pI830->entityPrivate->pScrn_2;
      }
   } else {
      pI8301 = I830PTR(pI830->entityPrivate->pScrn_1);
      pScrn1 = pI830->entityPrivate->pScrn_1;
      pI8302 = pI830;
      pScrn2 = pScrn;
   }

   pI830->rotation = xf86RandR12GetRotation(pScrn->pScreen);

   /* Check if we've still got the same orientation, or same mode */
   if (pI830->rotation == oldRotation && pI830->currentMode == mode)
#if 0
	reAllocate = FALSE;
#else
	return TRUE;
#endif

   /* 
    * We grab the DRI lock when reallocating buffers to avoid DRI clients
    * getting bogus information.
    */

#ifdef XF86DRI
   if (pI8301->directRenderingEnabled && reAllocate) {
      didLock = I830DRILock(pScrn1);
      
      /* Do heap teardown here
       */
      if (pI8301->mmModeFlags & I830_KERNEL_TEX) {
	 drmI830MemDestroyHeap destroy;
	 destroy.region = I830_MEM_REGION_AGP;
	 
	 if (drmCommandWrite(pI8301->drmSubFD, 
			     DRM_I830_DESTROY_HEAP, 
			     &destroy, sizeof(destroy))) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "[dri] I830 destroy heap failed\n");
	 }
      }
      
      if (pI8301->mmModeFlags & I830_KERNEL_TEX) {
	 if (pI8301->TexMem.Key != -1)
	    xf86UnbindGARTMemory(pScrn1->scrnIndex, pI8301->TexMem.Key);
	 I830FreeVidMem(pScrn1, &(pI8301->TexMem));
      }
      if (pI8301->StolenPool.Allocated.Key != -1) {
         xf86UnbindGARTMemory(pScrn1->scrnIndex, pI8301->StolenPool.Allocated.Key);
         xf86DeallocateGARTMemory(pScrn1->scrnIndex, pI8301->StolenPool.Allocated.Key);
      }
      if (pI8301->DepthBuffer.Key != -1)
         xf86UnbindGARTMemory(pScrn1->scrnIndex, pI8301->DepthBuffer.Key);
      I830FreeVidMem(pScrn1, &(pI8301->DepthBuffer));
      if (pI8301->BackBuffer.Key != -1)
         xf86UnbindGARTMemory(pScrn1->scrnIndex, pI8301->BackBuffer.Key);
      I830FreeVidMem(pScrn1, &(pI8301->BackBuffer));
   }
#endif

   if (reAllocate) {
      *pI830->used3D |= 1<<31; /* use high bit to denote new rotation occured */

      if (pI8301->RotatedMem.Key != -1)
         xf86UnbindGARTMemory(pScrn1->scrnIndex, pI8301->RotatedMem.Key);
 
      I830FreeVidMem(pScrn1, &(pI8301->RotatedMem));
      memset(&(pI8301->RotatedMem), 0, sizeof(pI8301->RotatedMem));
      pI8301->RotatedMem.Key = -1;

      if (IS_I965G(pI8301)) {
         if (pI8301->RotateStateMem.Key != -1)
            xf86UnbindGARTMemory(pScrn1->scrnIndex, pI8301->RotateStateMem.Key);
 
         I830FreeVidMem(pScrn1, &(pI8301->RotateStateMem));
         memset(&(pI8301->RotateStateMem), 0, sizeof(pI8301->RotateStateMem));
      	 pI8301->RotateStateMem.Key = -1;
      }

      if (pI830->entityPrivate) {
         if (pI8301->RotatedMem2.Key != -1)
            xf86UnbindGARTMemory(pScrn1->scrnIndex, pI8301->RotatedMem2.Key);
 
         I830FreeVidMem(pScrn1, &(pI8301->RotatedMem2));
         memset(&(pI8301->RotatedMem2), 0, sizeof(pI8301->RotatedMem2));
         pI8301->RotatedMem2.Key = -1;
      }
   }

   switch (pI830->rotation) {
      case RR_Rotate_0:
         if (reAllocate)
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Rotating Screen to 0 degrees\n");
         pScrn->displayWidth = pI830->displayWidth;
         break;
      case RR_Rotate_90:
         if (reAllocate)
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Rotating Screen to 90 degrees\n");
         pScrn->displayWidth = pScrn->pScreen->width;
         break;
      case RR_Rotate_180:
         if (reAllocate)
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Rotating Screen to 180 degrees\n");
         pScrn->displayWidth = pI830->displayWidth;
         break;
      case RR_Rotate_270:
         if (reAllocate)
            xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Rotating Screen to 270 degrees\n");
         pScrn->displayWidth = pScrn->pScreen->width;
         break;
   }

   /* As DRI doesn't run on the secondary head, we know that disableTiling
    * is always TRUE.
    */
   if (I830IsPrimary(pScrn) && !pI830->disableTiling) {
#if 0
      int dWidth = pScrn->displayWidth; /* save current displayWidth */
#endif

      for (i = 0; pitches[i] != 0; i++) {
         if (pitches[i] >= pScrn->displayWidth) {
            pScrn->displayWidth = pitches[i];
            break;
         }
      }

      /*
       * If the displayWidth is a tilable pitch, test if there's enough
       * memory available to enable tiling.
       */
      if (pScrn->displayWidth == pitches[i]) {
  	/* TODO */
      }
   }

   if (reAllocate) {
      if (pI830->entityPrivate) {
         if (pI8302->rotation != RR_Rotate_0) {
            if (!I830AllocateRotated2Buffer(pScrn1, 
			      pI8302->disableTiling ? ALLOC_NO_TILING : 0))
               goto BAIL0;

            I830FixOffset(pScrn1, &(pI8301->RotatedMem2));
            if (pI8301->RotatedMem2.Key != -1)
               xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->RotatedMem2.Key, pI8301->RotatedMem2.Offset);
         }
      }

      if (pI8301->rotation != RR_Rotate_0) {
         if (!I830AllocateRotatedBuffer(pScrn1, 
			      pI8301->disableTiling ? ALLOC_NO_TILING : 0))
            goto BAIL1;

         I830FixOffset(pScrn1, &(pI8301->RotatedMem));
         if (pI8301->RotatedMem.Key != -1)
            xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->RotatedMem.Key, pI8301->RotatedMem.Offset);
	 if (IS_I965G(pI8301)) {
            I830FixOffset(pScrn1, &(pI8301->RotateStateMem));
            if (pI8301->RotateStateMem.Key != -1)
            	xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->RotateStateMem.Key, 
				   pI8301->RotateStateMem.Offset);
	 }
      }
   }
   
   shadowRemove (pScrn->pScreen, NULL);
   if (pI830->rotation != RR_Rotate_0)
      shadowAdd (pScrn->pScreen, 
		 (*pScrn->pScreen->GetScreenPixmap) (pScrn->pScreen),
		 func, I830WindowLinear, pI830->rotation, NULL);

   if (I830IsPrimary(pScrn)) {
      if (pI830->rotation != RR_Rotate_0)
         pScrn->fbOffset = pI830->RotatedMem.Start;
      else
         pScrn->fbOffset = pI830->FrontBuffer.Start;
      if (pI830->entityPrivate) {
         if (pI8302->rotation != RR_Rotate_0) 
            pScrn2->fbOffset = pI8301->RotatedMem2.Start;
         else
            pScrn2->fbOffset = pI8301->FrontBuffer2.Start;
         I830SelectBuffer(pScrn2, I830_SELECT_FRONT);
      }
   } else {
      if (pI830->rotation != RR_Rotate_0)
         pScrn->fbOffset = pI8301->RotatedMem2.Start;
      else
         pScrn->fbOffset = pI8301->FrontBuffer2.Start;
      if (pI8301->rotation != RR_Rotate_0)
         pScrn1->fbOffset = pI8301->RotatedMem.Start;
      else
         pScrn1->fbOffset = pI8301->FrontBuffer.Start;
      I830SelectBuffer(pScrn1, I830_SELECT_FRONT);
   }
   I830SelectBuffer(pScrn, I830_SELECT_FRONT);

#ifdef XF86DRI
   if (pI8301->directRenderingEnabled && reAllocate) {
      if (!I830AllocateBackBuffer(pScrn1,
			      pI8301->disableTiling ? ALLOC_NO_TILING : 0))
         goto BAIL2;

      if (!I830AllocateDepthBuffer(pScrn1,
			      pI8301->disableTiling ? ALLOC_NO_TILING : 0))
         goto BAIL3;

      if (pI8301->mmModeFlags & I830_KERNEL_TEX) {
	 if (!I830AllocateTextureMemory(pScrn1,
					pI8301->disableTiling ? ALLOC_NO_TILING : 0))
	    goto BAIL4;
      }

      I830DoPoolAllocation(pScrn1, &(pI8301->StolenPool));

      I830FixOffset(pScrn1, &(pI8301->BackBuffer));
      I830FixOffset(pScrn1, &(pI8301->DepthBuffer));

      if (pI8301->BackBuffer.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->BackBuffer.Key, pI8301->BackBuffer.Offset);
      if (pI8301->DepthBuffer.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->DepthBuffer.Key, pI8301->DepthBuffer.Offset);
      if (pI8301->StolenPool.Allocated.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->StolenPool.Allocated.Key, pI8301->StolenPool.Allocated.Offset);
      if (pI8301->mmModeFlags & I830_KERNEL_TEX) {
	 if (pI8301->TexMem.Key != -1)
	    xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->TexMem.Key, pI8301->TexMem.Offset);
      }
      I830SetupMemoryTiling(pScrn1);
      /* update fence registers */
      if (IS_I965G(pI830)) {
         for (i = 0; i < FENCE_NEW_NR; i++) {
            OUTREG(FENCE_NEW + i * 8, pI830->ModeReg.Fence[i]);
            OUTREG(FENCE_NEW + 4 + i * 8, pI830->ModeReg.Fence[i+FENCE_NEW_NR]);
         }
      } else {
         for (i = 0; i < 8; i++) 
            OUTREG(FENCE + i * 4, pI8301->ModeReg.Fence[i]);
      }

      {
         drmI830Sarea *sarea = DRIGetSAREAPrivate(pScrn1->pScreen);
         I830UpdateDRIBuffers(pScrn1, sarea );
      }
      
      if (didLock)
	 I830DRIUnlock(pScrn1);
   }
#endif

#if 0
   if (I830IsPrimary(pScrn)) {
      pI830->xoffset = (pI830->FrontBuffer.Start / pI830->cpp) % pI830->displayWidth;
      pI830->yoffset = (pI830->FrontBuffer.Start / pI830->cpp) / pI830->displayWidth;
   } else {
      I830Ptr pI8301 = I830PTR(pI830->entityPrivate->pScrn_1);
      pI830->xoffset = (pI8301->FrontBuffer2.Start / pI830->cpp) % pI830->displayWidth;
      pI830->yoffset = (pI8301->FrontBuffer2.Start / pI830->cpp) / pI830->displayWidth;
   }
#endif

   pScrn->pScreen->ModifyPixmapHeader((*pScrn->pScreen->GetScreenPixmap)(pScrn->pScreen), pScrn->pScreen->width,
		    pScrn->pScreen->height, pScrn->pScreen->rootDepth, pScrn->bitsPerPixel,
		    PixmapBytePad(pScrn->displayWidth, pScrn->pScreen->rootDepth), 
		    (pointer)(pI8301->FbBase + pScrn->fbOffset));

   if (pI830->entityPrivate) {
      if (I830IsPrimary(pScrn)) {
         if (!pI830->starting) {
            pScrn2->pScreen->ModifyPixmapHeader((*pScrn2->pScreen->GetScreenPixmap)(pScrn2->pScreen), pScrn2->pScreen->width,
		    pScrn2->pScreen->height, pScrn2->pScreen->rootDepth, pScrn2->bitsPerPixel,
		    PixmapBytePad(pScrn2->displayWidth, pScrn2->pScreen->rootDepth), 
		    (pointer)(pI8301->FbBase + pScrn2->fbOffset));

            /* Repaint the second head */
            (*pScrn2->EnableDisableFBAccess) (pScrn2->pScreen->myNum, FALSE);
            (*pScrn2->EnableDisableFBAccess) (pScrn2->pScreen->myNum, TRUE);
         }
      } else {
         if (!pI830->starting) {
            pScrn1->pScreen->ModifyPixmapHeader((*pScrn1->pScreen->GetScreenPixmap)(pScrn1->pScreen), pScrn1->pScreen->width,
		    pScrn1->pScreen->height, pScrn1->pScreen->rootDepth, pScrn1->bitsPerPixel,
		    PixmapBytePad(pScrn1->displayWidth, pScrn1->pScreen->rootDepth), 
		    (pointer)(pI8301->FbBase + pScrn1->fbOffset));

            /* Repaint the first head */
            (*pScrn1->EnableDisableFBAccess) (pScrn1->pScreen->myNum, FALSE);
            (*pScrn1->EnableDisableFBAccess) (pScrn1->pScreen->myNum, TRUE);
         }
      }
   }

#ifdef I830_USE_XAA
   if (pI830->AccelInfoRec != NULL) {
      /* Don't allow pixmap cache or offscreen pixmaps when rotated */
      /* XAA needs some serious fixing for this to happen */
      if (pI830->rotation == RR_Rotate_0) {
	 pI830->AccelInfoRec->Flags = LINEAR_FRAMEBUFFER | OFFSCREEN_PIXMAPS |
				      PIXMAP_CACHE;
	 pI830->AccelInfoRec->UsingPixmapCache = TRUE;
	 /* funny as it seems this will enable XAA's createpixmap */
	 pI830->AccelInfoRec->maxOffPixWidth = 0;
	 pI830->AccelInfoRec->maxOffPixHeight = 0;
      } else {
	 pI830->AccelInfoRec->Flags = LINEAR_FRAMEBUFFER;
	 pI830->AccelInfoRec->UsingPixmapCache = FALSE;
	 /* funny as it seems this will disable XAA's createpixmap */
	 pI830->AccelInfoRec->maxOffPixWidth = 1;
	 pI830->AccelInfoRec->maxOffPixHeight = 1;
      }
   }
#endif

   return TRUE;

BAIL4:
#ifdef XF86DRI
   if (pI8301->directRenderingEnabled)
      I830FreeVidMem(pScrn1, &(pI8301->DepthBuffer));
#endif
BAIL3:
#ifdef XF86DRI
   if (pI8301->directRenderingEnabled)
      I830FreeVidMem(pScrn1, &(pI8301->BackBuffer));
#endif
BAIL2:
   if (pI8301->rotation != RR_Rotate_0) {
      if (pI8301->RotatedMem.Key != -1)
         xf86UnbindGARTMemory(pScrn1->scrnIndex, pI8301->RotatedMem.Key);
  
      I830FreeVidMem(pScrn1, &(pI8301->RotatedMem));
      memset(&(pI8301->RotatedMem), 0, sizeof(pI8301->RotatedMem));
      pI8301->RotatedMem.Key = -1;
   }
BAIL1:
   if (pI830->entityPrivate) {
      if (pI8302->rotation != RR_Rotate_0) {
         if (pI8301->RotatedMem.Key != -1)
            xf86UnbindGARTMemory(pScrn1->scrnIndex, pI8301->RotatedMem.Key);

         I830FreeVidMem(pScrn1, &(pI8301->RotatedMem));
         memset(&(pI8301->RotatedMem), 0, sizeof(pI8301->RotatedMem));
         pI8301->RotatedMem.Key = -1;
      }
   }
BAIL0:
   pScrn->displayWidth = displayWidth;

   /* must flip mmWidth & mmHeight */
   if ( ((oldRotation & (RR_Rotate_90 | RR_Rotate_270)) &&
	 (pI830->rotation & (RR_Rotate_0 | RR_Rotate_180))) ||
        ((oldRotation & (RR_Rotate_0 | RR_Rotate_180)) &&
	 (pI830->rotation & (RR_Rotate_90 | RR_Rotate_270))) ) {
      int tmp = pScrn->pScreen->mmWidth;
      pScrn->pScreen->mmWidth = pScrn->pScreen->mmHeight;
      pScrn->pScreen->mmHeight = tmp;
   }

   if (oldRotation & (RR_Rotate_0 | RR_Rotate_180)) {
      pScrn->pScreen->width = pScrn->virtualX;
      pScrn->pScreen->height = pScrn->virtualY;
   } else {
      pScrn->pScreen->width = pScrn->virtualY;
      pScrn->pScreen->height = pScrn->virtualX;
   }

   pI830->rotation = oldRotation;

   if (pI830->entityPrivate) {
      if (pI8302->rotation != RR_Rotate_0) {
         if (!I830AllocateRotated2Buffer(pScrn1, 
			      pI8302->disableTiling ? ALLOC_NO_TILING : 0))
            xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		    "Oh dear, the rotated2 buffer failed - badness\n");

         I830FixOffset(pScrn1, &(pI8301->RotatedMem2));
         if (pI8301->RotatedMem2.Key != -1)
            xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->RotatedMem2.Key, pI8301->RotatedMem2.Offset);
      }
   }

   if (pI8301->rotation != RR_Rotate_0) {
      if (!I830AllocateRotatedBuffer(pScrn1, 
			      pI8301->disableTiling ? ALLOC_NO_TILING : 0))
         xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
		    "Oh dear, the rotated buffer failed - badness\n");

      I830FixOffset(pScrn1, &(pI8301->RotatedMem));
      if (pI8301->RotatedMem.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->RotatedMem.Key, pI8301->RotatedMem.Offset);
   }

   shadowRemove (pScrn->pScreen, NULL);
   if (pI830->rotation != RR_Rotate_0)
      shadowAdd (pScrn->pScreen, 
		 (*pScrn->pScreen->GetScreenPixmap) (pScrn->pScreen),
		 func, I830WindowLinear, pI830->rotation, NULL);

   if (I830IsPrimary(pScrn)) {
      if (pI830->rotation != RR_Rotate_0)
         pScrn->fbOffset = pI830->RotatedMem.Start;
      else
         pScrn->fbOffset = pI830->FrontBuffer.Start;
      if (pI830->entityPrivate) {
         if (pI8302->rotation != RR_Rotate_0) 
            pScrn2->fbOffset = pI8301->RotatedMem2.Start;
         else
            pScrn2->fbOffset = pI8301->FrontBuffer2.Start;
         I830SelectBuffer(pScrn2, I830_SELECT_FRONT);
      }
   } else {
      if (pI830->rotation != RR_Rotate_0)
         pScrn->fbOffset = pI8301->RotatedMem2.Start;
      else
         pScrn->fbOffset = pI8301->FrontBuffer2.Start;
      if (pI8301->rotation != RR_Rotate_0)
         pScrn1->fbOffset = pI8301->RotatedMem.Start;
      else
         pScrn1->fbOffset = pI8301->FrontBuffer.Start;
      I830SelectBuffer(pScrn1, I830_SELECT_FRONT);
   }
   I830SelectBuffer(pScrn, I830_SELECT_FRONT);

   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Reverting to previous configured mode\n");

   switch (oldRotation) {
      case RR_Rotate_0:
         xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Rotating Screen back to 0 degrees\n");
         break;
      case RR_Rotate_90:
         xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Rotating Screen back to 90 degrees\n");
         break;
      case RR_Rotate_180:
         xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Rotating Screen back to 180 degrees\n");
         break;
      case RR_Rotate_270:
         xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		    "Rotating Screen back to 270 degrees\n");
         break;
   }

#ifdef XF86DRI
   if (pI8301->directRenderingEnabled) {
      if (!I830AllocateBackBuffer(pScrn1,
			      pI8301->disableTiling ? ALLOC_NO_TILING : 0))
         xf86DrvMsg(pScrn1->scrnIndex, X_INFO, 
		    "Oh dear, the back buffer failed - badness\n");

      if (!I830AllocateDepthBuffer(pScrn1,
			      pI8301->disableTiling ? ALLOC_NO_TILING : 0))
         xf86DrvMsg(pScrn1->scrnIndex, X_INFO, 
		    "Oh dear, the depth buffer failed - badness\n");
      if (pI8301->mmModeFlags & I830_KERNEL_TEX) {
	 if (!I830AllocateTextureMemory(pScrn1,
					pI8301->disableTiling ? ALLOC_NO_TILING : 0))
	    xf86DrvMsg(pScrn1->scrnIndex, X_INFO, 
		       "Oh dear, the texture cache failed - badness\n");
      }

      I830DoPoolAllocation(pScrn1, &(pI8301->StolenPool));

      I830FixOffset(pScrn1, &(pI8301->BackBuffer));
      I830FixOffset(pScrn1, &(pI8301->DepthBuffer));

      if (pI8301->BackBuffer.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->BackBuffer.Key, pI8301->BackBuffer.Offset);
      if (pI8301->DepthBuffer.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->DepthBuffer.Key, pI8301->DepthBuffer.Offset);
      if (pI8301->StolenPool.Allocated.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->StolenPool.Allocated.Key, pI8301->StolenPool.Allocated.Offset);
      if (pI8301->mmModeFlags & I830_KERNEL_TEX) {
	 if (pI8301->TexMem.Key != -1)
	    xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->TexMem.Key, pI8301->TexMem.Offset);
      }
      I830SetupMemoryTiling(pScrn1);
      /* update fence registers */
      for (i = 0; i < 8; i++) 
         OUTREG(FENCE + i * 4, pI8301->ModeReg.Fence[i]);
      { 
         drmI830Sarea *sarea = DRIGetSAREAPrivate(pScrn1->pScreen);
         I830UpdateDRIBuffers(pScrn1, sarea );
      }
      
      if (didLock)
	 I830DRIUnlock(pScrn1);
   }
#endif

   pScrn->pScreen->ModifyPixmapHeader((*pScrn->pScreen->GetScreenPixmap)(pScrn->pScreen), pScrn->pScreen->width,
		    pScrn->pScreen->height, pScrn->pScreen->rootDepth, pScrn->bitsPerPixel,
		    PixmapBytePad(pScrn->displayWidth, pScrn->pScreen->rootDepth), 
		    (pointer)(pI8301->FbBase + pScrn->fbOffset));

   if (pI830->entityPrivate) {
      if (I830IsPrimary(pScrn)) {
         pScrn2->pScreen->ModifyPixmapHeader((*pScrn2->pScreen->GetScreenPixmap)(pScrn2->pScreen), pScrn2->pScreen->width,
		    pScrn2->pScreen->height, pScrn2->pScreen->rootDepth, pScrn2->bitsPerPixel,
		    PixmapBytePad(pScrn2->displayWidth, pScrn2->pScreen->rootDepth), 
		    (pointer)(pI8301->FbBase + pScrn2->fbOffset));

         /* Repaint the second head */
         (*pScrn2->EnableDisableFBAccess) (pScrn2->pScreen->myNum, FALSE);
         (*pScrn2->EnableDisableFBAccess) (pScrn2->pScreen->myNum, TRUE);
      } else {
         pScrn1->pScreen->ModifyPixmapHeader((*pScrn1->pScreen->GetScreenPixmap)(pScrn1->pScreen), pScrn1->pScreen->width,
		    pScrn1->pScreen->height, pScrn1->pScreen->rootDepth, pScrn1->bitsPerPixel,
		    PixmapBytePad(pScrn1->displayWidth, pScrn1->pScreen->rootDepth), 
		    (pointer)(pI8301->FbBase + pScrn1->fbOffset));

         /* Repaint the first head */
         (*pScrn1->EnableDisableFBAccess) (pScrn1->pScreen->myNum, FALSE);
         (*pScrn1->EnableDisableFBAccess) (pScrn1->pScreen->myNum, TRUE);
      }
   }

   return FALSE;
}
