/***************************************************************************

 Copyright 2000 Intel Corporation.  All Rights Reserved.

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
 IN NO EVENT SHALL INTEL, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 **************************************************************************/

/*
 * i830_video.c: i830/i845 Xv driver.
 *
 * Copyright © 2002 by Alan Hourihane and David Dawes
 *
 * Authors:
 *	Alan Hourihane <alanh@tungstengraphics.com>
 *	David Dawes <dawes@xfree86.org>
 *
 * Derived from i810 Xv driver:
 *
 * Authors of i810 code:
 *	Jonathan Bian <jonathan.bian@intel.com>
 *      Offscreen Images:
 *        Matt Sottek <matthew.j.sottek@intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/mman.h>

#include "sna.h"
#include "sna_reg.h"
#include "sna_video.h"

#include <xf86xv.h>
#include <X11/extensions/Xv.h>

#ifdef SNA_XVMC
#define _SNA_XVMC_SERVER_
#include "sna_video_hwmc.h"
#else
static inline Bool sna_video_xvmc_setup(struct sna *sna,
					ScreenPtr ptr,
					XF86VideoAdaptorPtr target)
{
	return FALSE;
}
#endif

#if DEBUG_VIDEO_TEXTURED
#undef DBG
#define DBG(x) ErrorF x
#endif

void sna_video_free_buffers(struct sna *sna, struct sna_video *video)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(video->old_buf); i++) {
		if (video->old_buf[i]) {
			kgem_bo_destroy(&sna->kgem, video->old_buf[i]);
			video->old_buf[i] = NULL;
		}
	}

	if (video->buf) {
		kgem_bo_destroy(&sna->kgem, video->buf);
		video->buf = NULL;
	}
}

void sna_video_frame_fini(struct sna *sna,
			  struct sna_video *video,
			  struct sna_video_frame *frame)
{
	struct kgem_bo *bo;

	if (!frame->bo->reusable) {
		kgem_bo_destroy(&sna->kgem, frame->bo);
		return;
	}

	bo = video->old_buf[1];
	video->old_buf[1] = video->old_buf[0];
	video->old_buf[0] = video->buf;
	video->buf = bo;
}

Bool
sna_video_clip_helper(ScrnInfoPtr scrn,
		      struct sna_video *video,
		      xf86CrtcPtr * crtc_ret,
		      BoxPtr dst,
		      short src_x, short src_y,
		      short drw_x, short drw_y,
		      short src_w, short src_h,
		      short drw_w, short drw_h,
		      int id,
		      int *top, int* left, int* npixels, int *nlines,
		      RegionPtr reg, INT32 width, INT32 height)
{
	Bool ret;
	RegionRec crtc_region_local;
	RegionPtr crtc_region = reg;
	BoxRec crtc_box;
	INT32 x1, x2, y1, y2;
	xf86CrtcPtr crtc;

	x1 = src_x;
	x2 = src_x + src_w;
	y1 = src_y;
	y2 = src_y + src_h;

	dst->x1 = drw_x;
	dst->x2 = drw_x + drw_w;
	dst->y1 = drw_y;
	dst->y2 = drw_y + drw_h;

	/*
	 * For overlay video, compute the relevant CRTC and
	 * clip video to that
	 */
	crtc = sna_covering_crtc(scrn, dst, video->desired_crtc,
				   &crtc_box);

	/* For textured video, we don't actually want to clip at all. */
	if (crtc && !video->textured) {
		RegionInit(&crtc_region_local, &crtc_box, 1);
		crtc_region = &crtc_region_local;
		RegionIntersect(crtc_region, crtc_region, reg);
	}
	*crtc_ret = crtc;

	ret = xf86XVClipVideoHelper(dst, &x1, &x2, &y1, &y2,
				    crtc_region, width, height);
	if (crtc_region != reg)
		RegionUninit(&crtc_region_local);

	*top = y1 >> 16;
	*left = (x1 >> 16) & ~1;
	*npixels = ALIGN(((x2 + 0xffff) >> 16), 2) - *left;
	if (is_planar_fourcc(id)) {
		*top &= ~1;
		*nlines = ALIGN(((y2 + 0xffff) >> 16), 2) - *top;
	} else
		*nlines = ((y2 + 0xffff) >> 16) - *top;

	return ret;
}

void
sna_video_frame_init(struct sna *sna,
		     struct sna_video *video,
		     int id, short width, short height,
		     struct sna_video_frame *frame)
{
	int align;

	frame->id = id;
	frame->width = width;
	frame->height = height;

	/* Only needs to be DWORD-aligned for textured on i915, but overlay has
	 * stricter requirements.
	 */
	if (video->textured) {
		align = 4;
	} else {
		if (sna->kgem.gen >= 40)
			/* Actually the alignment is 64 bytes, too. But the
			 * stride must be at least 512 bytes. Take the easy fix
			 * and align on 512 bytes unconditionally. */
			align = 512;
		else if (IS_I830(sna) || IS_845G(sna))
			/* Harsh, errata on these chipsets limit the stride
			 * to be a multiple of 256 bytes.
			 */
			align = 256;
		else
			align = 64;
	}

#if SNA_XVMC
	/* for i915 xvmc, hw requires 1kb aligned surfaces */
	if (id == FOURCC_XVMC && sna->kgem.gen < 40)
		align = 1024;
#endif


	/* Determine the desired destination pitch (representing the chroma's pitch,
	 * in the planar case.
	 */
	if (is_planar_fourcc(id)) {
		if (video->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
			frame->pitch[0] = ALIGN((height / 2), align);
			frame->pitch[1] = ALIGN(height, align);
			frame->size = frame->pitch[0] * width * 3;
		} else {
			frame->pitch[0] = ALIGN((width / 2), align);
			frame->pitch[1] = ALIGN(width, align);
			frame->size = frame->pitch[0] * height * 3;
		}
	} else {
		if (video->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
			frame->pitch[0] = ALIGN((height << 1), align);
			frame->size = frame->pitch[0] * width;
		} else {
			frame->pitch[0] = ALIGN((width << 1), align);
			frame->size = frame->pitch[0] * height;
		}
		frame->pitch[1] = 0;
	}

	frame->YBufOffset = 0;

	if (video->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
		frame->UBufOffset =
			frame->YBufOffset + frame->pitch[1] * width;
		frame->VBufOffset =
			frame->UBufOffset + frame->pitch[0] * width / 2;
	} else {
		frame->UBufOffset =
			frame->YBufOffset + frame->pitch[1] * height;
		frame->VBufOffset =
			frame->UBufOffset + frame->pitch[0] * height / 2;
	}
}

static struct kgem_bo *
sna_video_buffer(struct sna *sna,
		 struct sna_video *video,
		 struct sna_video_frame *frame)
{
	/* Free the current buffer if we're going to have to reallocate */
	if (video->buf && video->buf->size < frame->size)
		sna_video_free_buffers(sna, video);

	if (video->buf == NULL)
		video->buf = kgem_create_linear(&sna->kgem, frame->size);

	return video->buf;
}

static void sna_memcpy_plane(unsigned char *dst, unsigned char *src,
			       int height, int width,
			       int dstPitch, int srcPitch, Rotation rotation)
{
	int i, j = 0;
	unsigned char *s;

	switch (rotation) {
	case RR_Rotate_0:
		/* optimise for the case of no clipping */
		if (srcPitch == dstPitch && srcPitch == width)
			memcpy(dst, src, srcPitch * height);
		else
			for (i = 0; i < height; i++) {
				memcpy(dst, src, width);
				src += srcPitch;
				dst += dstPitch;
			}
		break;
	case RR_Rotate_90:
		for (i = 0; i < height; i++) {
			s = src;
			for (j = 0; j < width; j++) {
				dst[(i) + ((width - j - 1) * dstPitch)] = *s++;
			}
			src += srcPitch;
		}
		break;
	case RR_Rotate_180:
		for (i = 0; i < height; i++) {
			s = src;
			for (j = 0; j < width; j++) {
				dst[(width - j - 1) +
				    ((height - i - 1) * dstPitch)] = *s++;
			}
			src += srcPitch;
		}
		break;
	case RR_Rotate_270:
		for (i = 0; i < height; i++) {
			s = src;
			for (j = 0; j < width; j++) {
				dst[(height - i - 1) + (j * dstPitch)] = *s++;
			}
			src += srcPitch;
		}
		break;
	}
}

static void
sna_copy_planar_data(struct sna *sna,
		     struct sna_video *video,
		     struct sna_video_frame *frame,
		     unsigned char *buf,
		     unsigned char *dst,
		     int srcPitch, int srcPitch2,
		     int srcH, int top, int left)
{
	unsigned char *src1, *src2, *src3, *dst1, *dst2, *dst3;

	/* Copy Y data */
	src1 = buf + (top * srcPitch) + left;

	dst1 = dst + frame->YBufOffset;

	sna_memcpy_plane(dst1, src1,
			 frame->height, frame->width,
			 frame->pitch[1], srcPitch,
			 video->rotation);

	/* Copy V data for YV12, or U data for I420 */
	src2 = buf +		/* start of YUV data */
	    (srcH * srcPitch) +	/* move over Luma plane */
	    ((top >> 1) * srcPitch2) +	/* move down from by top lines */
	    (left >> 1);	/* move left by left pixels */

	if (frame->id == FOURCC_I420)
		dst2 = dst + frame->UBufOffset;
	else
		dst2 = dst + frame->VBufOffset;

	sna_memcpy_plane(dst2, src2,
			 frame->height / 2, frame->width / 2,
			 frame->pitch[0], srcPitch2,
			 video->rotation);

	/* Copy U data for YV12, or V data for I420 */
	src3 = buf +		/* start of YUV data */
	    (srcH * srcPitch) +	/* move over Luma plane */
	    ((srcH >> 1) * srcPitch2) +	/* move over Chroma plane */
	    ((top >> 1) * srcPitch2) +	/* move down from by top lines */
	    (left >> 1);	/* move left by left pixels */
	if (frame->id == FOURCC_I420)
		dst3 = dst + frame->VBufOffset;
	else
		dst3 = dst + frame->UBufOffset;

	sna_memcpy_plane(dst3, src3,
			 frame->height / 2, frame->width / 2,
			 frame->pitch[0], srcPitch2,
			 video->rotation);
}

static void
sna_copy_packed_data(struct sna *sna,
		     struct sna_video *video,
		     struct sna_video_frame *frame,
		     unsigned char *buf,
		     unsigned char *dst,
		     int srcPitch,
		     int top, int left)
{
	unsigned char *src;
	unsigned char *s;
	int i, j;

	src = buf + (top * srcPitch) + (left << 1);

	dst += frame->YBufOffset;

	switch (video->rotation) {
	case RR_Rotate_0:
		frame->width <<= 1;
		for (i = 0; i < frame->height; i++) {
			memcpy(dst, src, frame->width);
			src += srcPitch;
			dst += frame->pitch[0];
		}
		break;
	case RR_Rotate_90:
		frame->height <<= 1;
		for (i = 0; i < frame->height; i += 2) {
			s = src;
			for (j = 0; j < frame->width; j++) {
				/* Copy Y */
				dst[(i + 0) + ((frame->width - j - 1) * frame->pitch[0])] = *s++;
				(void)*s++;
			}
			src += srcPitch;
		}
		frame->height >>= 1;
		src = buf + (top * srcPitch) + (left << 1);
		for (i = 0; i < frame->height; i += 2) {
			for (j = 0; j < frame->width; j += 2) {
				/* Copy U */
				dst[((i * 2) + 1) + ((frame->width - j - 1) * frame->pitch[0])] =
				    src[(j * 2) + 1 + (i * srcPitch)];
				dst[((i * 2) + 1) + ((frame->width - j - 2) * frame->pitch[0])] =
				    src[(j * 2) + 1 + ((i + 1) * srcPitch)];
				/* Copy V */
				dst[((i * 2) + 3) + ((frame->width - j - 1) * frame->pitch[0])] =
				    src[(j * 2) + 3 + (i * srcPitch)];
				dst[((i * 2) + 3) + ((frame->width - j - 2) * frame->pitch[0])] =
				    src[(j * 2) + 3 + ((i + 1) * srcPitch)];
			}
		}
		break;
	case RR_Rotate_180:
		frame->width <<= 1;
		for (i = 0; i < frame->height; i++) {
			s = src;
			for (j = 0; j < frame->width; j += 4) {
				dst[(frame->width - j - 4) + ((frame->height - i - 1) * frame->pitch[0])] =
				    *s++;
				dst[(frame->width - j - 3) + ((frame->height - i - 1) * frame->pitch[0])] =
				    *s++;
				dst[(frame->width - j - 2) + ((frame->height - i - 1) * frame->pitch[0])] =
				    *s++;
				dst[(frame->width - j - 1) + ((frame->height - i - 1) * frame->pitch[0])] =
				    *s++;
			}
			src += srcPitch;
		}
		break;
	case RR_Rotate_270:
		frame->height <<= 1;
		for (i = 0; i < frame->height; i += 2) {
			s = src;
			for (j = 0; j < frame->width; j++) {
				/* Copy Y */
				dst[(frame->height - i - 2) + (j * frame->pitch[0])] = *s++;
				(void)*s++;
			}
			src += srcPitch;
		}
		frame->height >>= 1;
		src = buf + (top * srcPitch) + (left << 1);
		for (i = 0; i < frame->height; i += 2) {
			for (j = 0; j < frame->width; j += 2) {
				/* Copy U */
				dst[(((frame->height - i) * 2) - 3) + (j * frame->pitch[0])] =
				    src[(j * 2) + 1 + (i * srcPitch)];
				dst[(((frame->height - i) * 2) - 3) +
				    ((j + 1) * frame->pitch[0])] =
				    src[(j * 2) + 1 + ((i + 1) * srcPitch)];
				/* Copy V */
				dst[(((frame->height - i) * 2) - 1) + (j * frame->pitch[0])] =
				    src[(j * 2) + 3 + (i * srcPitch)];
				dst[(((frame->height - i) * 2) - 1) +
				    ((j + 1) * frame->pitch[0])] =
				    src[(j * 2) + 3 + ((i + 1) * srcPitch)];
			}
		}
		break;
	}
}

Bool
sna_video_copy_data(struct sna *sna,
		    struct sna_video *video,
		    struct sna_video_frame *frame,
		    int top, int left,
		    int npixels, int nlines,
		    unsigned char *buf)
{
	unsigned char *dst;

	frame->bo = sna_video_buffer(sna, video, frame);
	if (frame->bo == NULL)
		return FALSE;

	/* copy data */
	dst = kgem_bo_map(&sna->kgem, frame->bo, PROT_READ | PROT_WRITE);
	if (dst == NULL)
		return FALSE;

	if (is_planar_fourcc(frame->id)) {
		int srcPitch = ALIGN(frame->width, 0x4);
		int srcPitch2 = ALIGN((frame->width >> 1), 0x4);

		sna_copy_planar_data(sna, video, frame,
				     buf, dst,
				     srcPitch, srcPitch2,
				     nlines, top, left);
	} else {
		int srcPitch = frame->width << 1;

		sna_copy_packed_data(sna, video, frame,
				     buf, dst,
				     srcPitch,
				     top, left);
	}

	munmap(dst, video->buf->size);
	return TRUE;
}

static void sna_crtc_box(xf86CrtcPtr crtc, BoxPtr crtc_box)
{
	if (crtc->enabled) {
		crtc_box->x1 = crtc->x;
		crtc_box->x2 =
		    crtc->x + xf86ModeWidth(&crtc->mode, crtc->rotation);
		crtc_box->y1 = crtc->y;
		crtc_box->y2 =
		    crtc->y + xf86ModeHeight(&crtc->mode, crtc->rotation);
	} else
		crtc_box->x1 = crtc_box->x2 = crtc_box->y1 = crtc_box->y2 = 0;
}

static void sna_box_intersect(BoxPtr dest, BoxPtr a, BoxPtr b)
{
	dest->x1 = a->x1 > b->x1 ? a->x1 : b->x1;
	dest->x2 = a->x2 < b->x2 ? a->x2 : b->x2;
	dest->y1 = a->y1 > b->y1 ? a->y1 : b->y1;
	dest->y2 = a->y2 < b->y2 ? a->y2 : b->y2;
	if (dest->x1 >= dest->x2 || dest->y1 >= dest->y2)
		dest->x1 = dest->x2 = dest->y1 = dest->y2 = 0;
}

static int sna_box_area(BoxPtr box)
{
	return (int)(box->x2 - box->x1) * (int)(box->y2 - box->y1);
}

/*
 * Return the crtc covering 'box'. If two crtcs cover a portion of
 * 'box', then prefer 'desired'. If 'desired' is NULL, then prefer the crtc
 * with greater coverage
 */

xf86CrtcPtr
sna_covering_crtc(ScrnInfoPtr scrn,
		  BoxPtr box, xf86CrtcPtr desired, BoxPtr crtc_box_ret)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	xf86CrtcPtr crtc, best_crtc;
	int coverage, best_coverage;
	int c;
	BoxRec crtc_box, cover_box;

	DBG(("%s for box=(%d, %d), (%d, %d)\n",
	     __FUNCTION__, box->x1, box->y1, box->x2, box->y2));

	best_crtc = NULL;
	best_coverage = 0;
	crtc_box_ret->x1 = 0;
	crtc_box_ret->x2 = 0;
	crtc_box_ret->y1 = 0;
	crtc_box_ret->y2 = 0;
	for (c = 0; c < xf86_config->num_crtc; c++) {
		crtc = xf86_config->crtc[c];

		/* If the CRTC is off, treat it as not covering */
		if (!sna_crtc_on(crtc)) {
			DBG(("%s: crtc %d off, skipping\n", __FUNCTION__, c));
			continue;
		}

		sna_crtc_box(crtc, &crtc_box);
		sna_box_intersect(&cover_box, &crtc_box, box);
		coverage = sna_box_area(&cover_box);
		if (coverage && crtc == desired) {
			DBG(("%s: box is on desired crtc [%p]\n",
			     __FUNCTION__, crtc));
			*crtc_box_ret = crtc_box;
			return crtc;
		}
		if (coverage > best_coverage) {
			*crtc_box_ret = crtc_box;
			best_crtc = crtc;
			best_coverage = coverage;
		}
	}
	DBG(("%s: best crtc = %p\n", __FUNCTION__, best_crtc));
	return best_crtc;
}

bool
sna_wait_for_scanline(struct sna *sna, PixmapPtr pixmap,
		      xf86CrtcPtr crtc, RegionPtr clip)
{
	pixman_box16_t box, crtc_box;
	int pipe, event;
	Bool full_height;
	int y1, y2;
	uint32_t *b;

	/* XXX no wait for scanline support on SNB? */
	if (sna->kgem.gen >= 60)
		return false;

	if (!pixmap_is_scanout(pixmap))
		return false;

	if (crtc == NULL) {
		if (clip) {
			crtc_box = *REGION_EXTENTS(NULL, clip);
		} else {
			crtc_box.x1 = 0; /* XXX drawable offsets? */
			crtc_box.y1 = 0;
			crtc_box.x2 = pixmap->drawable.width;
			crtc_box.y2 = pixmap->drawable.height;
		}
		crtc = sna_covering_crtc(sna->scrn, &crtc_box, NULL, &crtc_box);
	}

	if (crtc == NULL)
		return false;

	if (clip) {
		box = *REGION_EXTENTS(unused, clip);

		if (crtc->transform_in_use)
			pixman_f_transform_bounds(&crtc->f_framebuffer_to_crtc, &box);

		/* We could presume the clip was correctly computed... */
		sna_crtc_box(crtc, &crtc_box);
		sna_box_intersect(&box, &crtc_box, &box);

		/*
		 * Make sure we don't wait for a scanline that will
		 * never occur
		 */
		y1 = (crtc_box.y1 <= box.y1) ? box.y1 - crtc_box.y1 : 0;
		y2 = (box.y2 <= crtc_box.y2) ?
			box.y2 - crtc_box.y1 : crtc_box.y2 - crtc_box.y1;
		if (y2 <= y1)
			return false;

		full_height = FALSE;
		if (y1 == 0 && y2 == (crtc_box.y2 - crtc_box.y1))
			full_height = TRUE;
	} else {
		sna_crtc_box(crtc, &crtc_box);
		y1 = crtc_box.y1;
		y2 = crtc_box.y2;
		full_height = TRUE;
	}

	/*
	 * Pre-965 doesn't have SVBLANK, so we need a bit
	 * of extra time for the blitter to start up and
	 * do its job for a full height blit
	 */
	if (sna_crtc_to_pipe(crtc) == 0) {
		pipe = MI_LOAD_SCAN_LINES_DISPLAY_PIPEA;
		event = MI_WAIT_FOR_PIPEA_SCAN_LINE_WINDOW;
		if (full_height)
			event = MI_WAIT_FOR_PIPEA_SVBLANK;
	} else {
		pipe = MI_LOAD_SCAN_LINES_DISPLAY_PIPEB;
		event = MI_WAIT_FOR_PIPEB_SCAN_LINE_WINDOW;
		if (full_height)
			event = MI_WAIT_FOR_PIPEB_SVBLANK;
	}

	if (crtc->mode.Flags & V_INTERLACE) {
		/* DSL count field lines */
		y1 /= 2;
		y2 /= 2;
	}

	b = kgem_get_batch(&sna->kgem, 5);
	/* The documentation says that the LOAD_SCAN_LINES command
	 * always comes in pairs. Don't ask me why. */
	b[0] = MI_LOAD_SCAN_LINES_INCL | pipe;
	b[1] = (y1 << 16) | (y2-1);
	b[2] = MI_LOAD_SCAN_LINES_INCL | pipe;
	b[3] = (y1 << 16) | (y2-1);
	b[4] = MI_WAIT_FOR_EVENT | event;
	kgem_advance_batch(&sna->kgem, 5);
	return true;
}

void sna_video_init(struct sna *sna, ScreenPtr screen)
{
	XF86VideoAdaptorPtr *adaptors, *newAdaptors;
	XF86VideoAdaptorPtr textured, overlay;
	int num_adaptors;
	int prefer_overlay =
	    xf86ReturnOptValBool(sna->Options, OPTION_PREFER_OVERLAY, FALSE);

	num_adaptors = xf86XVListGenericAdaptors(sna->scrn, &adaptors);
	newAdaptors =
	    malloc((num_adaptors + 2) * sizeof(XF86VideoAdaptorPtr *));
	if (newAdaptors == NULL)
		return;

	memcpy(newAdaptors, adaptors,
	       num_adaptors * sizeof(XF86VideoAdaptorPtr));
	adaptors = newAdaptors;

	/* Set up textured video if we can do it at this depth and we are on
	 * supported hardware.
	 */
	textured = sna_video_textured_setup(sna, screen);
	overlay = sna_video_overlay_setup(sna, screen);

	if (overlay && prefer_overlay)
		adaptors[num_adaptors++] = overlay;

	if (textured)
		adaptors[num_adaptors++] = textured;

	if (overlay && !prefer_overlay)
		adaptors[num_adaptors++] = overlay;

	if (num_adaptors)
		xf86XVScreenInit(screen, adaptors, num_adaptors);
	else
		xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
			   "Disabling Xv because no adaptors could be initialized.\n");
	if (textured)
		sna_video_xvmc_setup(sna, screen, textured);

	free(adaptors);
}
