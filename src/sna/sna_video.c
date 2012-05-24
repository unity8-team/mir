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

#include "intel_options.h"

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
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(video->old_buf); i++) {
		if (video->old_buf[i]) {
			if (video->old_buf[i]->unique_id)
				drmModeRmFB(sna->kgem.fd,
						video->old_buf[i]->unique_id);
			kgem_bo_destroy(&sna->kgem, video->old_buf[i]);
			video->old_buf[i] = NULL;
		}
	}

	if (video->buf) {
		if (video->buf->unique_id)
			drmModeRmFB(sna->kgem.fd, video->buf->unique_id);
		kgem_bo_destroy(&sna->kgem, video->buf);
		video->buf = NULL;
	}
}

struct kgem_bo *
sna_video_buffer(struct sna *sna,
		 struct sna_video *video,
		 struct sna_video_frame *frame)
{
	/* Free the current buffer if we're going to have to reallocate */
	if (video->buf && kgem_bo_size(video->buf) < frame->size)
		sna_video_free_buffers(sna, video);

	if (video->buf == NULL)
		video->buf = kgem_create_linear(&sna->kgem, frame->size,
						CREATE_GTT_MAP);

	return video->buf;
}

void sna_video_buffer_fini(struct sna *sna,
			   struct sna_video *video)
{
	struct kgem_bo *bo;

	bo = video->old_buf[1];
	video->old_buf[1] = video->old_buf[0];
	video->old_buf[0] = video->buf;
	video->buf = bo;
}

Bool
sna_video_clip_helper(ScrnInfoPtr scrn,
		      struct sna_video *video,
		      struct sna_video_frame *frame,
		      xf86CrtcPtr * crtc_ret,
		      BoxPtr dst,
		      short src_x, short src_y,
		      short drw_x, short drw_y,
		      short src_w, short src_h,
		      short drw_w, short drw_h,
		      RegionPtr reg)
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
	crtc = sna_covering_crtc(scrn, dst, video->desired_crtc, &crtc_box);

	/* For textured video, we don't actually want to clip at all. */
	if (crtc && !video->textured) {
		RegionInit(&crtc_region_local, &crtc_box, 0);
		crtc_region = &crtc_region_local;
		RegionIntersect(crtc_region, crtc_region, reg);
	}
	*crtc_ret = crtc;

	ret = xf86XVClipVideoHelper(dst, &x1, &x2, &y1, &y2,
				    crtc_region, frame->width, frame->height);
	if (crtc_region != reg)
		RegionUninit(&crtc_region_local);

	frame->top = y1 >> 16;
	frame->left = (x1 >> 16) & ~1;
	frame->npixels = ALIGN(((x2 + 0xffff) >> 16), 2) - frame->left;
	if (is_planar_fourcc(frame->id)) {
		frame->top &= ~1;
		frame->nlines = ALIGN(((y2 + 0xffff) >> 16), 2) - frame->top;
	} else
		frame->nlines = ((y2 + 0xffff) >> 16) - frame->top;

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
		else if (sna->kgem.gen < 21)
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

static void sna_memcpy_plane(uint8_t *dst, const uint8_t *src,
			     int height, int width,
			     int dstPitch, int srcPitch,
			     Rotation rotation)
{
	const uint8_t *s;
	int i, j = 0;

	switch (rotation) {
	case RR_Rotate_0:
		/* optimise for the case of no clipping */
		if (srcPitch == dstPitch && srcPitch == width)
			memcpy(dst, src, srcPitch * height);
		else while (height--) {
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
sna_copy_planar_data(struct sna_video *video,
		     const struct sna_video_frame *frame,
		     const uint8_t *src, uint8_t *dst)
{
	uint8_t *d;
	int w = frame->npixels;
	int h = frame->nlines;
	int pitch;

	pitch = ALIGN(frame->width, 4);
	sna_memcpy_plane(dst, src + frame->top * pitch + frame->left,
			 h, w, frame->pitch[1], pitch, video->rotation);

	src += frame->height * pitch; /* move over Luma plane */

	/* align to beginning of chroma planes */
	pitch = ALIGN((frame->width >> 1), 0x4);
	src += (frame->top >> 1) * pitch + (frame->left >> 1);
	w >>= 1;
	h >>= 1;

	if (frame->id == FOURCC_I420)
		d = dst + frame->UBufOffset;
	else
		d = dst + frame->VBufOffset;

	sna_memcpy_plane(d, src, h, w, frame->pitch[0], pitch, video->rotation);
	src += (frame->height >> 1) * pitch; /* move over Chroma plane */

	if (frame->id == FOURCC_I420)
		d = dst + frame->VBufOffset;
	else
		d = dst + frame->UBufOffset;

	sna_memcpy_plane(d, src, h, w, frame->pitch[0], pitch, video->rotation);
}

static void
sna_copy_packed_data(struct sna_video *video,
		     const struct sna_video_frame *frame,
		     const uint8_t *buf,
		     uint8_t *dst)
{
	int pitch = frame->width << 1;
	const uint8_t *src, *s;
	int w = frame->npixels;
	int h = frame->nlines;
	int i, j;

	src = buf + (frame->top * pitch) + (frame->left << 1);

	switch (video->rotation) {
	case RR_Rotate_0:
		w <<= 1;
		for (i = 0; i < h; i++) {
			memcpy(dst, src, w);
			src += pitch;
			dst += frame->pitch[0];
		}
		break;
	case RR_Rotate_90:
		h <<= 1;
		for (i = 0; i < h; i += 2) {
			s = src;
			for (j = 0; j < w; j++) {
				/* Copy Y */
				dst[(i + 0) + ((w - j - 1) * frame->pitch[0])] = *s;
				s += 2;
			}
			src += pitch;
		}
		h >>= 1;
		src = buf + (frame->top * pitch) + (frame->left << 1);
		for (i = 0; i < h; i += 2) {
			for (j = 0; j < w; j += 2) {
				/* Copy U */
				dst[((i * 2) + 1) + ((w - j - 1) * frame->pitch[0])] = src[(j * 2) + 1 + (i * pitch)];
				dst[((i * 2) + 1) + ((w - j - 2) * frame->pitch[0])] = src[(j * 2) + 1 + ((i + 1) * pitch)];
				/* Copy V */ dst[((i * 2) + 3) + ((w - j - 1) * frame->pitch[0])] = src[(j * 2) + 3 + (i * pitch)];
				dst[((i * 2) + 3) + ((w - j - 2) * frame->pitch[0])] = src[(j * 2) + 3 + ((i + 1) * pitch)];
			}
		}
		break;
	case RR_Rotate_180:
		w <<= 1;
		for (i = 0; i < h; i++) {
			s = src;
			for (j = 0; j < w; j += 4) {
				dst[(w - j - 4) + ((h - i - 1) * frame->pitch[0])] = *s++;
				dst[(w - j - 3) + ((h - i - 1) * frame->pitch[0])] = *s++;
				dst[(w - j - 2) + ((h - i - 1) * frame->pitch[0])] = *s++;
				dst[(w - j - 1) + ((h - i - 1) * frame->pitch[0])] = *s++;
			}
			src += pitch;
		}
		break;
	case RR_Rotate_270:
		h <<= 1;
		for (i = 0; i < h; i += 2) {
			s = src;
			for (j = 0; j < w; j++) {
				/* Copy Y */
				dst[(h - i - 2) + (j * frame->pitch[0])] = *s;
				s += 2;
			}
			src += pitch;
		}
		h >>= 1;
		src = buf + (frame->top * pitch) + (frame->left << 1);
		for (i = 0; i < h; i += 2) {
			for (j = 0; j < w; j += 2) {
				/* Copy U */
				dst[(((h - i) * 2) - 3) + (j * frame->pitch[0])] = src[(j * 2) + 1 + (i * pitch)];
				dst[(((h - i) * 2) - 3) + ((j + 1) * frame->pitch[0])] = src[(j * 2) + 1 + ((i + 1) * pitch)];
				/* Copy V */
				dst[(((h - i) * 2) - 1) + (j * frame->pitch[0])] = src[(j * 2) + 3 + (i * pitch)];
				dst[(((h - i) * 2) - 1) + ((j + 1) * frame->pitch[0])] = src[(j * 2) + 3 + ((i + 1) * pitch)];
			}
		}
		break;
	}
}

Bool
sna_video_copy_data(struct sna *sna,
		    struct sna_video *video,
		    struct sna_video_frame *frame,
		    const uint8_t *buf)
{
	uint8_t *dst;

	if (frame->bo == NULL)
		return FALSE;

	DBG(("%s: handle=%d, size=%dx%d, rotation=%d\n",
	     __FUNCTION__, frame->bo->handle, frame->width, frame->height,
	     video->rotation));
	DBG(("%s: top=%d, left=%d\n", __FUNCTION__, frame->top, frame->left));

	/* In the common case, we can simply the upload in a single pwrite */
	if (video->rotation == RR_Rotate_0) {
		if (is_planar_fourcc(frame->id)) {
			uint16_t pitch[2] = {
				ALIGN((frame->width >> 1), 0x4),
				ALIGN(frame->width, 0x4),
			};
			if (pitch[0] == frame->pitch[0] &&
			    pitch[1] == frame->pitch[1] &&
			    frame->top == 0 && frame->left == 0) {
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
					      buf + (frame->top * frame->width*2) + (frame->left << 1),
					      frame->nlines*frame->width*2);
				return TRUE;
			}
		}
	}

	/* copy data, must use GTT so that we keep the overlay uncached */
	dst = kgem_bo_map__gtt(&sna->kgem, frame->bo);
	if (dst == NULL)
		return FALSE;

	if (is_planar_fourcc(frame->id))
		sna_copy_planar_data(video, frame, buf, dst);
	else
		sna_copy_packed_data(video, frame, buf, dst);

	return TRUE;
}

void sna_video_init(struct sna *sna, ScreenPtr screen)
{
	XF86VideoAdaptorPtr *adaptors, *newAdaptors;
	XF86VideoAdaptorPtr textured, overlay;
	int num_adaptors;
	int prefer_overlay =
	    xf86ReturnOptValBool(sna->Options, OPTION_PREFER_OVERLAY, FALSE);

	if (!xf86LoaderCheckSymbol("xf86XVListGenericAdaptors"))
		return;

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
	overlay = sna_video_sprite_setup(sna, screen);
	if (overlay == NULL)
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
