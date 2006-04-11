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
   Bool updateInvarient = FALSE;
#ifdef XF86DRI
   drmI830Sarea *sarea = NULL;
   drm_context_t myContext = 0;
#endif
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

   if (updateInvarient) {
      *pI830->used3D = pScrn->scrnIndex;
#ifdef XF86DRI
      if (sarea)
         sarea->ctxOwner = myContext;
#endif
      BEGIN_LP_RING(64);
      /* invarient state */
      OUT_RING(MI_NOOP);
      OUT_RING(0x66014140);
      OUT_RING(0x7d990000);
      OUT_RING(0x00000000);
      OUT_RING(0x7d9a0000);
      OUT_RING(0x00000000);
      OUT_RING(0x7d980000);
      OUT_RING(0x00000000);
      OUT_RING(0x76fac688);
      OUT_RING(0x6700a770);
      OUT_RING(0x7d040081);
      OUT_RING(0x00000000);
      /* flush map & render cache */
      OUT_RING(MI_FLUSH | MI_WRITE_DIRTY_STATE | MI_INVALIDATE_MAP_CACHE);
      OUT_RING(0x00000000);
      /* draw rect */
      OUT_RING(0x7d800003);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      OUT_RING((pScrn->virtualX - 1) | (pScrn->virtualY - 1) << 16);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      /* scissor */
      OUT_RING(0x7c800002);
      OUT_RING(0x7d810001);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      OUT_RING(0x7c000003);
      OUT_RING(0x7d070000);
      OUT_RING(0x00000000);
      OUT_RING(0x68000002);
      /* context setup */
      OUT_RING(0x6db3ffff);
      OUT_RING(0x7d040744);
      OUT_RING(0xfffffff0);
      OUT_RING(0x00902c80);
      OUT_RING(0x00000000);
      OUT_RING(0x00020216);
      OUT_RING(0x6ba008a1);
      OUT_RING(0x7d880000);
      OUT_RING(0x00000000);
      /* dv0 */
      OUT_RING(0x7d850000);
      /* dv1 */
      if (pI830->cpp == 1)
         OUT_RING(0x10880000);
      else if (pI830->cpp == 2)
            OUT_RING(0x10880200);
         else
            OUT_RING(0x10880308);
      /* stipple */
      OUT_RING(0x7d830000);
      OUT_RING(0x00000000);
      /* fragment program - texture blend replace*/
      OUT_RING(0x7d050008);
      OUT_RING(0x19180000);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      OUT_RING(0x19083c00);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      OUT_RING(0x15200000);
      OUT_RING(0x01000000);
      OUT_RING(0x00000000);
      /* texture sampler state */
      OUT_RING(0x7d010003);
      OUT_RING(0x00000001);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      /* front buffer, pitch, offset */
      OUT_RING(0x7d8e0001);
      OUT_RING(0x03800000 | (((pI830->displayWidth * pI830->cpp) / 4) << 2));
      if (I830IsPrimary(pScrn))
         OUT_RING(pI830->FrontBuffer.Start);
      else 
         OUT_RING(pI8301->FrontBuffer2.Start);

      /* Set the entire frontbuffer up as a texture */
      OUT_RING(0x7d000003);
      OUT_RING(0x00000001);

      if (I830IsPrimary(pScrn)) 
         OUT_RING(pI830->RotatedMem.Start);
      else 
	 OUT_RING(pI8301->RotatedMem2.Start);

      if (pI830->disableTiling)
         use_fence = 0;
      else
         use_fence = 4;
      
      if (pI830->cpp == 1)
         use_fence |= 0x80; /* MAPSURF_8BIT */
      else
      if (pI830->cpp == 2)
         use_fence |= 0x100; /* MAPSURF_16BIT */
      else
         use_fence |= 0x180; /* MAPSURF_32BIT */
      OUT_RING(use_fence | (pScreen->height - 1) << 21 | (pScreen->width - 1) << 10);
      OUT_RING(((((pScrn->displayWidth * pI830->cpp) / 4) - 1) << 21));
      ADVANCE_LP_RING();
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
   Bool updateInvarient = FALSE;
   int use_fence;
   int j;
#ifdef XF86DRI
   drmI830Sarea *sarea = NULL;
   drm_context_t myContext = 0;
#endif
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

   if (updateInvarient) {
      *pI830->used3D = pScrn->scrnIndex;
#ifdef XF86DRI
      if (sarea)
         sarea->ctxOwner = myContext;
#endif
      
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
      OUT_RING(0x7d800003);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);
      OUT_RING((pScrn->virtualX - 1) | (pScrn->virtualY - 1) << 16);
      OUT_RING(0x00000000);
      OUT_RING(0x00000000);

      /* front buffer */
      OUT_RING(0x7d8e0001);
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
         OUT_RING(0x00 | (pScreen->height - 1) << 21 | (pScreen->width - 1) << 10);
      else if (pI830->cpp == 2)
         OUT_RING(0x40 | (pScreen->height - 1) << 21 | (pScreen->width - 1) << 10);
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
   else
	   if (IS_I9XX(pI830))
		   func = I915UpdateRotate;
	   else
		   func = I830UpdateRotate;

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

   pI830->rotation = I830GetRotation(pScrn->pScreen);

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
      {
	 drmI830MemDestroyHeap destroy;
	 destroy.region = I830_MEM_REGION_AGP;
	 
	 if (drmCommandWrite(pI8301->drmSubFD, 
			     DRM_I830_DESTROY_HEAP, 
			     &destroy, sizeof(destroy))) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		       "[dri] I830 destroy heap failed\n");
	 }
      }
      
      if (pI8301->TexMem.Key != -1)
         xf86UnbindGARTMemory(pScrn1->scrnIndex, pI8301->TexMem.Key);
      I830FreeVidMem(pScrn1, &(pI8301->TexMem));
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
			      (pI8301->disableTiling ? ALLOC_NO_TILING : 0)))
            goto BAIL1;

         I830FixOffset(pScrn1, &(pI8301->RotatedMem));
         if (pI8301->RotatedMem.Key != -1)
            xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->RotatedMem.Key, pI8301->RotatedMem.Offset);
      }
   }
   
   shadowRemove (pScrn->pScreen, NULL);
   if (pI830->rotation != RR_Rotate_0)
      shadowAdd (pScrn->pScreen, 
                    (*pScrn->pScreen->GetScreenPixmap) (pScrn->pScreen), 
                    func, I830WindowLinear, pI830->rotation, 0);

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

      if (!I830AllocateTextureMemory(pScrn1,
			      pI8301->disableTiling ? ALLOC_NO_TILING : 0))
         goto BAIL4;

      I830DoPoolAllocation(pScrn1, &(pI8301->StolenPool));

      I830FixOffset(pScrn1, &(pI8301->BackBuffer));
      I830FixOffset(pScrn1, &(pI8301->DepthBuffer));

      if (pI8301->BackBuffer.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->BackBuffer.Key, pI8301->BackBuffer.Offset);
      if (pI8301->DepthBuffer.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->DepthBuffer.Key, pI8301->DepthBuffer.Offset);
      if (pI8301->StolenPool.Allocated.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->StolenPool.Allocated.Key, pI8301->StolenPool.Allocated.Offset);
      if (pI8301->TexMem.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->TexMem.Key, pI8301->TexMem.Offset);
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

   /* Don't allow pixmap cache or offscreen pixmaps when rotated */
   /* XAA needs some serious fixing for this to happen */
   if (pI830->rotation == RR_Rotate_0) {
      pI830->AccelInfoRec->Flags = LINEAR_FRAMEBUFFER | OFFSCREEN_PIXMAPS | PIXMAP_CACHE;
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
			      (pI8301->disableTiling ? ALLOC_NO_TILING : 0)))
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
                    func, I830WindowLinear, pI830->rotation, 0);

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

      if (!I830AllocateTextureMemory(pScrn1,
			      pI8301->disableTiling ? ALLOC_NO_TILING : 0))
         xf86DrvMsg(pScrn1->scrnIndex, X_INFO, 
		    "Oh dear, the texture cache failed - badness\n");

      I830DoPoolAllocation(pScrn1, &(pI8301->StolenPool));

      I830FixOffset(pScrn1, &(pI8301->BackBuffer));
      I830FixOffset(pScrn1, &(pI8301->DepthBuffer));

      if (pI8301->BackBuffer.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->BackBuffer.Key, pI8301->BackBuffer.Offset);
      if (pI8301->DepthBuffer.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->DepthBuffer.Key, pI8301->DepthBuffer.Offset);
      if (pI8301->StolenPool.Allocated.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->StolenPool.Allocated.Key, pI8301->StolenPool.Allocated.Offset);
      if (pI8301->TexMem.Key != -1)
         xf86BindGARTMemory(pScrn1->scrnIndex, pI8301->TexMem.Key, pI8301->TexMem.Offset);
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
