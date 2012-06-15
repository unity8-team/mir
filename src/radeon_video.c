
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_video.h"

#include "xf86.h"
#include "dixstruct.h"
#include "atipciids.h"
#include "xf86fbman.h"

/* DPMS */
#ifdef HAVE_XEXTPROTO_71
#include <X11/extensions/dpmsconst.h>
#else
#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#endif

#include <X11/extensions/Xv.h>
#include "fourcc.h"

#define OFF_DELAY       250  /* milliseconds */
#define FREE_DELAY      15000

#define OFF_TIMER       0x01
#define FREE_TIMER      0x02
#define CLIENT_VIDEO_ON 0x04

#define GET_PORT_PRIVATE(pScrn) \
   (RADEONPortPrivPtr)((RADEONPTR(pScrn))->adaptor->pPortPrivates[0].ptr)

static void
radeon_box_intersect(BoxPtr dest, BoxPtr a, BoxPtr b)
{
    dest->x1 = a->x1 > b->x1 ? a->x1 : b->x1;
    dest->x2 = a->x2 < b->x2 ? a->x2 : b->x2;
    dest->y1 = a->y1 > b->y1 ? a->y1 : b->y1;
    dest->y2 = a->y2 < b->y2 ? a->y2 : b->y2;

    if (dest->x1 >= dest->x2 || dest->y1 >= dest->y2)
	dest->x1 = dest->x2 = dest->y1 = dest->y2 = 0;
}

static void
radeon_crtc_box(xf86CrtcPtr crtc, BoxPtr crtc_box)
{
    if (crtc->enabled) {
	crtc_box->x1 = crtc->x;
	crtc_box->x2 = crtc->x + xf86ModeWidth(&crtc->mode, crtc->rotation);
	crtc_box->y1 = crtc->y;
	crtc_box->y2 = crtc->y + xf86ModeHeight(&crtc->mode, crtc->rotation);
    } else
	crtc_box->x1 = crtc_box->x2 = crtc_box->y1 = crtc_box->y2 = 0;
}

static int
radeon_box_area(BoxPtr box)
{
    return (int) (box->x2 - box->x1) * (int) (box->y2 - box->y1);
}

static Bool
radeon_crtc_is_enabled(xf86CrtcPtr crtc)
{
    drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
    return drmmode_crtc->dpms_mode == DPMSModeOn;
}

xf86CrtcPtr
radeon_pick_best_crtc(ScrnInfoPtr pScrn,
		      int x1, int x2, int y1, int y2)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int			coverage, best_coverage, c;
    BoxRec		box, crtc_box, cover_box;
    RROutputPtr         primary_output = NULL;
    xf86CrtcPtr         best_crtc = NULL, primary_crtc = NULL;

    if (!pScrn->vtSema)
	return NULL;

    box.x1 = x1;
    box.x2 = x2;
    box.y1 = y1;
    box.y2 = y2;
    best_coverage = 0;

    /* Prefer the CRTC of the primary output */
    if (dixPrivateKeyRegistered(rrPrivKey)) {
	primary_output = RRFirstOutput(pScrn->pScreen);
    }
    if (primary_output && primary_output->crtc)
	primary_crtc = primary_output->crtc->devPrivate;

    for (c = 0; c < xf86_config->num_crtc; c++) {
	xf86CrtcPtr crtc = xf86_config->crtc[c];

	if (!radeon_crtc_is_enabled(crtc))
	    continue;

	radeon_crtc_box(crtc, &crtc_box);
	radeon_box_intersect(&cover_box, &crtc_box, &box);
	coverage = radeon_box_area(&cover_box);
	if (coverage > best_coverage ||
	    (coverage == best_coverage && crtc == primary_crtc)) {
	    best_crtc = crtc;
	    best_coverage = coverage;
	}
    }
    return best_crtc;
}

#ifndef HAVE_XF86CRTCCLIPVIDEOHELPER
static xf86CrtcPtr
radeon_covering_crtc(ScrnInfoPtr pScrn,
		     BoxPtr	box,
		     xf86CrtcPtr desired,
		     BoxPtr	crtc_box_ret)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    xf86CrtcPtr		crtc, best_crtc;
    int			coverage, best_coverage;
    int			c;
    BoxRec		crtc_box, cover_box;

    best_crtc = NULL;
    best_coverage = 0;
    crtc_box_ret->x1 = 0;
    crtc_box_ret->x2 = 0;
    crtc_box_ret->y1 = 0;
    crtc_box_ret->y2 = 0;
    for (c = 0; c < xf86_config->num_crtc; c++) {
	crtc = xf86_config->crtc[c];
	radeon_crtc_box(crtc, &crtc_box);
	radeon_box_intersect(&cover_box, &crtc_box, box);
	coverage = radeon_box_area(&cover_box);
	if (coverage && crtc == desired) {
	    *crtc_box_ret = crtc_box;
	    return crtc;
	} else if (coverage > best_coverage) {
	    *crtc_box_ret = crtc_box;
	    best_crtc = crtc;
	    best_coverage = coverage;
	}
    }
    return best_crtc;
}

static Bool
radeon_crtc_clip_video_helper(ScrnInfoPtr pScrn,
			      xf86CrtcPtr *crtc_ret,
			      xf86CrtcPtr desired_crtc,
			      BoxPtr      dst,
			      INT32	  *xa,
			      INT32	  *xb,
			      INT32	  *ya,
			      INT32	  *yb,
			      RegionPtr   reg,
			      INT32	  width,
			      INT32	  height)
{
    Bool	ret;
    RegionRec	crtc_region_local;
    RegionPtr	crtc_region = reg;
    
    /*
     * For overlay video, compute the relevant CRTC and
     * clip video to that
     */
    if (crtc_ret) {
	BoxRec		crtc_box;
	xf86CrtcPtr	crtc = radeon_covering_crtc(pScrn, dst,
						    desired_crtc,
						    &crtc_box);

	if (crtc) {
	    REGION_INIT (pScreen, &crtc_region_local, &crtc_box, 1);
	    crtc_region = &crtc_region_local;
	    REGION_INTERSECT (pScreen, crtc_region, crtc_region, reg);
	}
	*crtc_ret = crtc;
    }

    ret = xf86XVClipVideoHelper(dst, xa, xb, ya, yb, 
				crtc_region, width, height);

    if (crtc_region != reg)
	REGION_UNINIT (pScreen, &crtc_region_local);

    return ret;
}
#endif

void RADEONInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
    RADEONInfoPtr    info = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr texturedAdaptor = NULL;
    int num_adaptors;

    /* no overlay or 3D on RN50 */
    if (info->ChipFamily == CHIP_FAMILY_RV100 && !pRADEONEnt->HasCRTC2)
	    return;

    num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);
    newAdaptors = malloc((num_adaptors + 2) * sizeof(XF86VideoAdaptorPtr *));
    if (newAdaptors == NULL)
	return;

    memcpy(newAdaptors, adaptors, num_adaptors * sizeof(XF86VideoAdaptorPtr));
    adaptors = newAdaptors;

    if ((info->ChipFamily < CHIP_FAMILY_RS400)
	|| (info->directRenderingEnabled)
	) {
	texturedAdaptor = RADEONSetupImageTexturedVideo(pScreen);
	if (texturedAdaptor != NULL) {
	    adaptors[num_adaptors++] = texturedAdaptor;
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Set up textured video\n");
	} else
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Failed to set up textured video\n");
    } else
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Textured video requires CP on R5xx/R6xx/R7xx/IGP\n");

    if(num_adaptors)
	xf86XVScreenInit(pScreen, adaptors, num_adaptors);

    if(texturedAdaptor) {
	XF86MCAdaptorPtr xvmcAdaptor = RADEONCreateAdaptorXvMC(pScreen, texturedAdaptor->name);
	if(xvmcAdaptor) {
	    if(!xf86XvMCScreenInit(pScreen, 1, &xvmcAdaptor))
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "[XvMC] Failed to initialize extension.\n");
	    else
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "[XvMC] Extension initialized.\n");
	}
    }

    if(newAdaptors)
	free(newAdaptors);

}

#define INCLUDE_RGB_FORMATS 1

#if INCLUDE_RGB_FORMATS

#define NUM_IMAGES 8

/* Note: GUIDs are bogus... - but nothing uses them anyway */

#define FOURCC_RGBA32   0x41424752

#define XVIMAGE_RGBA32(byte_order)   \
        { \
                FOURCC_RGBA32, \
                XvRGB, \
                byte_order, \
                { 'R', 'G', 'B', 'A', \
                  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
                32, \
                XvPacked, \
                1, \
                32, 0x00FF0000, 0x0000FF00, 0x000000FF, \
                0, 0, 0, 0, 0, 0, 0, 0, 0, \
                {'A', 'R', 'G', 'B', \
                  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
                XvTopToBottom \
        }               

#define FOURCC_RGB24    0x00000000

#define XVIMAGE_RGB24   \
        { \
                FOURCC_RGB24, \
                XvRGB, \
                LSBFirst, \
                { 'R', 'G', 'B', 0, \
                  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
                24, \
                XvPacked, \
                1, \
                24, 0x00FF0000, 0x0000FF00, 0x000000FF, \
                0, 0, 0, 0, 0, 0, 0, 0, 0, \
                { 'R', 'G', 'B', \
                  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
                XvTopToBottom \
        }

#define FOURCC_RGBT16   0x54424752

#define XVIMAGE_RGBT16(byte_order)   \
        { \
                FOURCC_RGBT16, \
                XvRGB, \
                byte_order, \
                { 'R', 'G', 'B', 'T', \
                  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
                16, \
                XvPacked, \
                1, \
                16, 0x00007C00, 0x000003E0, 0x0000001F, \
                0, 0, 0, 0, 0, 0, 0, 0, 0, \
                {'A', 'R', 'G', 'B', \
                  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
                XvTopToBottom \
        }               

#define FOURCC_RGB16    0x32424752

#define XVIMAGE_RGB16(byte_order)   \
        { \
                FOURCC_RGB16, \
                XvRGB, \
                byte_order, \
                { 'R', 'G', 'B', 0x00, \
                  0x00,0x00,0x00,0x10,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71}, \
                16, \
                XvPacked, \
                1, \
                16, 0x0000F800, 0x000007E0, 0x0000001F, \
                0, 0, 0, 0, 0, 0, 0, 0, 0, \
                {'R', 'G', 'B', \
                  0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, \
                XvTopToBottom \
        }               
#endif

void
RADEONFreeVideoMemory(ScrnInfoPtr pScrn, RADEONPortPrivPtr pPriv)
{
    if (pPriv->video_memory != NULL) {
	radeon_legacy_free_memory(pScrn, pPriv->video_memory);
	pPriv->video_memory = NULL;

	if (pPriv->textured) {
	    pPriv->src_bo[0] = NULL;
	    radeon_legacy_free_memory(pScrn, pPriv->src_bo[1]);
	    pPriv->src_bo[1] = NULL;
	}
    }
}

void
RADEONStopVideo(ScrnInfoPtr pScrn, pointer data, Bool cleanup)
{
  RADEONPortPrivPtr pPriv = (RADEONPortPrivPtr)data;

  if (pPriv->textured) {
      if (cleanup) {
	  RADEONFreeVideoMemory(pScrn, pPriv);
      }
      return;
  }
}

void
RADEONQueryBestSize(
  ScrnInfoPtr pScrn,
  Bool motion,
  short vid_w, short vid_h,
  short drw_w, short drw_h,
  unsigned int *p_w, unsigned int *p_h,
  pointer data
){
    RADEONPortPrivPtr pPriv = (RADEONPortPrivPtr)data;

    if (!pPriv->textured) {
	if (vid_w > (drw_w << 4))
	    drw_w = vid_w >> 4;
	if (vid_h > (drw_h << 4))
	    drw_h = vid_h >> 4;
    }

  *p_w = drw_w;
  *p_h = drw_h;
}

void
RADEONCopyData(
  ScrnInfoPtr pScrn,
  unsigned char *src,
  unsigned char *dst,
  unsigned int srcPitch,
  unsigned int dstPitch,
  unsigned int h,
  unsigned int w,
  unsigned int bpp
){
    /* Get the byte-swapping right for big endian systems */
    if ( bpp == 2 ) {
	w *= 2;
	bpp = 1;
    }

    {
	int swap = RADEON_HOST_DATA_SWAP_NONE;

#if X_BYTE_ORDER == X_BIG_ENDIAN
	switch(bpp) {
	case 2:
	    swap = RADEON_HOST_DATA_SWAP_16BIT;
	    break;
	case 4:
	    swap = RADEON_HOST_DATA_SWAP_32BIT;
	    break;
	}
#endif

	w *= bpp;

	if (dstPitch == w && dstPitch == srcPitch)
	    RADEONCopySwap(dst, src, h * dstPitch, swap);
	else {
	    while (h--) {
		RADEONCopySwap(dst, src, w, swap);
		src += srcPitch;
		dst += dstPitch;
	    }
	}
    }
}


void
RADEONCopyMungedData(
   ScrnInfoPtr pScrn,
   unsigned char *src1,
   unsigned char *src2,
   unsigned char *src3,
   unsigned char *dst1,
   unsigned int srcPitch,
   unsigned int srcPitch2,
   unsigned int dstPitch,
   unsigned int h,
   unsigned int w
){
    uint32_t *dst;
    uint8_t *s1, *s2, *s3;
    int i, j;

    w /= 2;

    for( j = 0; j < h; j++ ) {
	dst = (pointer)dst1;
	s1 = src1;  s2 = src2;  s3 = src3;
	i = w;
	while( i > 4 ) {
	    dst[0] = cpu_to_le32(s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24));
	    dst[1] = cpu_to_le32(s1[2] | (s1[3] << 16) | (s3[1] << 8) | (s2[1] << 24));
	    dst[2] = cpu_to_le32(s1[4] | (s1[5] << 16) | (s3[2] << 8) | (s2[2] << 24));
	    dst[3] = cpu_to_le32(s1[6] | (s1[7] << 16) | (s3[3] << 8) | (s2[3] << 24));
	    dst += 4; s2 += 4; s3 += 4; s1 += 8;
	    i -= 4;
	}
	while( i-- ) {
	    dst[0] = cpu_to_le32(s1[0] | (s1[1] << 16) | (s3[0] << 8) | (s2[0] << 24));
	    dst++; s2++; s3++;
	    s1 += 2;
	}
	
	dst1 += dstPitch;
	src1 += srcPitch;
	if( j & 1 ) {
	    src2 += srcPitch2;
	    src3 += srcPitch2;
	}	
    }
}

int
RADEONQueryImageAttributes(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short *w, unsigned short *h,
    int *pitches, int *offsets
){
    const RADEONInfoRec * const info = RADEONPTR(pScrn);
    int size, tmp;

    if(*w > info->xv_max_width) *w = info->xv_max_width;
    if(*h > info->xv_max_height) *h = info->xv_max_height;

    *w = RADEON_ALIGN(*w, 2);
    if(offsets) offsets[0] = 0;

    switch(id) {
    case FOURCC_YV12:
    case FOURCC_I420:
	*h = RADEON_ALIGN(*h, 2);
	size = RADEON_ALIGN(*w, 4);
	if(pitches) pitches[0] = size;
	size *= *h;
	if(offsets) offsets[1] = size;
	tmp = RADEON_ALIGN(*w >> 1, 4);
	if(pitches) pitches[1] = pitches[2] = tmp;
	tmp *= (*h >> 1);
	size += tmp;
	if(offsets) offsets[2] = size;
	size += tmp;
	break;
    case FOURCC_RGBA32:
	size = *w << 2;
	if(pitches) pitches[0] = size;
	size *= *h;
	break;
    case FOURCC_RGB24:
	size = *w * 3;
	if(pitches) pitches[0] = size;
	size *= *h;
	break;
    case FOURCC_RGBT16:
    case FOURCC_RGB16:
    case FOURCC_UYVY:
    case FOURCC_YUY2:
    default:
	size = *w << 1;
	if(pitches) pitches[0] = size;
	size *= *h;
	break;
    }

    return size;
}

