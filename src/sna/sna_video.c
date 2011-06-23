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

	if (video->rotation & (RR_Rotate_90 | RR_Rotate_270)) {
		frame->UBufOffset = frame->pitch[1] * width;
		frame->VBufOffset =
			frame->UBufOffset + frame->pitch[0] * width / 2;
	} else {
		frame->UBufOffset = frame->pitch[1] * height;
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
		     const struct sna_video_frame *frame,
		     unsigned char *src,
		     unsigned char *dst,
		     int srcPitch, int srcPitch2,
		     int srcH, int top, int left)
{
	unsigned char *src1, *dst1;

	/* Copy Y data */
	src1 = src + (top * srcPitch) + left;

	sna_memcpy_plane(dst, src1,
			 frame->height, frame->width,
			 frame->pitch[1], srcPitch,
			 video->rotation);

	/* Copy V data for YV12, or U data for I420 */
	src1 = src +		/* start of YUV data */
	    (srcH * srcPitch) +	/* move over Luma plane */
	    ((top >> 1) * srcPitch2) +	/* move down from by top lines */
	    (left >> 1);	/* move left by left pixels */

	if (frame->id == FOURCC_I420)
		dst1 = dst + frame->UBufOffset;
	else
		dst1 = dst + frame->VBufOffset;

	sna_memcpy_plane(dst1, src1,
			 frame->height / 2, frame->width / 2,
			 frame->pitch[0], srcPitch2,
			 video->rotation);

	/* Copy U data for YV12, or V data for I420 */
	src1 = src +		/* start of YUV data */
	    (srcH * srcPitch) +	/* move over Luma plane */
	    ((srcH >> 1) * srcPitch2) +	/* move over Chroma plane */
	    ((top >> 1) * srcPitch2) +	/* move down from by top lines */
	    (left >> 1);	/* move left by left pixels */
	if (frame->id == FOURCC_I420)
		dst1 = dst + frame->VBufOffset;
	else
		dst1 = dst + frame->UBufOffset;

	sna_memcpy_plane(dst1, src1,
			 frame->height / 2, frame->width / 2,
			 frame->pitch[0], srcPitch2,
			 video->rotation);
}

static void
sna_copy_packed_data(struct sna *sna,
		     struct sna_video *video,
		     const struct sna_video_frame *frame,
		     unsigned char *buf,
		     unsigned char *dst,
		     int srcPitch,
		     int top, int left)
{
	int w = frame->width;
	int h = frame->height;
	unsigned char *src;
	unsigned char *s;
	int i, j;

	src = buf + (top * srcPitch) + (left << 1);

	switch (video->rotation) {
	case RR_Rotate_0:
		w <<= 1;
		for (i = 0; i < h; i++) {
			memcpy(dst, src, w);
			src += srcPitch;
			dst += frame->pitch[0];
		}
		break;
	case RR_Rotate_90:
		h <<= 1;
		for (i = 0; i < h; i += 2) {
			s = src;
			for (j = 0; j < w; j++) {
				/* Copy Y */
				dst[(i + 0) + ((w - j - 1) * frame->pitch[0])] = *s++;
				(void)*s++;
			}
			src += srcPitch;
		}
		h >>= 1;
		src = buf + (top * srcPitch) + (left << 1);
		for (i = 0; i < h; i += 2) {
			for (j = 0; j < w; j += 2) {
				/* Copy U */
				dst[((i * 2) + 1) + ((w - j - 1) * frame->pitch[0])] =
				    src[(j * 2) + 1 + (i * srcPitch)];
				dst[((i * 2) + 1) + ((w - j - 2) * frame->pitch[0])] =
				    src[(j * 2) + 1 + ((i + 1) * srcPitch)];
				/* Copy V */
				dst[((i * 2) + 3) + ((w - j - 1) * frame->pitch[0])] =
				    src[(j * 2) + 3 + (i * srcPitch)];
				dst[((i * 2) + 3) + ((w - j - 2) * frame->pitch[0])] =
				    src[(j * 2) + 3 + ((i + 1) * srcPitch)];
			}
		}
		break;
	case RR_Rotate_180:
		w <<= 1;
		for (i = 0; i < h; i++) {
			s = src;
			for (j = 0; j < w; j += 4) {
				dst[(w - j - 4) + ((h - i - 1) * frame->pitch[0])] =
				    *s++;
				dst[(w - j - 3) + ((h - i - 1) * frame->pitch[0])] =
				    *s++;
				dst[(w - j - 2) + ((h - i - 1) * frame->pitch[0])] =
				    *s++;
				dst[(w - j - 1) + ((h - i - 1) * frame->pitch[0])] =
				    *s++;
			}
			src += srcPitch;
		}
		break;
	case RR_Rotate_270:
		h <<= 1;
		for (i = 0; i < h; i += 2) {
			s = src;
			for (j = 0; j < w; j++) {
				/* Copy Y */
				dst[(h - i - 2) + (j * frame->pitch[0])] = *s++;
				(void)*s++;
			}
			src += srcPitch;
		}
		h >>= 1;
		src = buf + (top * srcPitch) + (left << 1);
		for (i = 0; i < h; i += 2) {
			for (j = 0; j < w; j += 2) {
				/* Copy U */
				dst[(((h - i) * 2) - 3) + (j * frame->pitch[0])] =
				    src[(j * 2) + 1 + (i * srcPitch)];
				dst[(((h - i) * 2) - 3) +
				    ((j + 1) * frame->pitch[0])] =
				    src[(j * 2) + 1 + ((i + 1) * srcPitch)];
				/* Copy V */
				dst[(((h - i) * 2) - 1) + (j * frame->pitch[0])] =
				    src[(j * 2) + 3 + (i * srcPitch)];
				dst[(((h - i) * 2) - 1) +
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

	/* In the common case, we can simply the upload in a single pwrite */
	if (video->rotation == RR_Rotate_0) {
		if (is_planar_fourcc(frame->id)) {
			uint16_t pitch[2] = {
				ALIGN((frame->width >> 1), 0x4),
				ALIGN(frame->width, 0x4),
			};
			if (pitch[0] == frame->pitch[0] &&
			    pitch[1] == frame->pitch[1] &&
			    top == 0 && left == 0) {
				kgem_bo_write(&sna->kgem, frame->bo,
					      buf,
					      pitch[1]*frame->height +
					      pitch[0]*frame->height);
				if (frame->id != FOURCC_I420) {
					uint32_t tmp;
					tmp = frame->VBufOffset;
					frame->VBufOffset = frame->UBufOffset;
					frame->UBufOffset = tmp;
				}
				return TRUE;
			}
		} else {
			if (frame->width*2 == frame->pitch[0]) {
				kgem_bo_write(&sna->kgem, frame->bo,
					      buf + (top * frame->width*2) + (left << 1),
					      frame->height*frame->width*2);
				return TRUE;
			}
		}
	}

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
