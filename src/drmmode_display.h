/*
 * Copyright © 2007 Red Hat, Inc.
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
 *     Dave Airlie <airlied@redhat.com>
 *
 */
#ifndef DRMMODE_DISPLAY_H
#define DRMMODE_DISPLAY_H

#ifdef XF86DRM_MODE

#include "intel_bufmgr.h"
#include "xf86drmMode.h"

typedef struct {
    int fd;
    uint32_t fb_id;
    drmModeResPtr mode_res;
    drmModeFBPtr mode_fb;
    int cpp;
    uint32_t (*create_new_fb)(ScrnInfoPtr pScrn, int width, int height,
			      int *pitch);
} drmmode_rec, *drmmode_ptr;

typedef struct {

    drmmode_ptr drmmode;
    drmModeCrtcPtr mode_crtc;
    dri_bo *cursor;
    dri_bo *rotate_bo;
    int rotate_fb_id;
} drmmode_crtc_private_rec, *drmmode_crtc_private_ptr;

typedef struct {
    drmmode_ptr drmmode;
    int output_id;
    drmModeConnectorPtr mode_output;
    drmModeEncoderPtr mode_encoder;
    drmModePropertyBlobPtr edid_blob;
} drmmode_output_private_rec, *drmmode_output_private_ptr;


extern Bool drmmode_pre_init(ScrnInfoPtr pScrn, drmmode_ptr drmmode,
			     char *busId, char *driver_name, int cpp);
extern void drmmode_set_fb(ScrnInfoPtr pScrn, drmmode_ptr drmmode, int width,
			   int height, int pitch, dri_bo *bo);
extern Bool drmmode_is_rotate_pixmap(ScrnInfoPtr pScrn, pointer pPixData,
				     dri_bo **bo);
extern void drmmode_set_cursor(ScrnInfoPtr scrn, drmmode_ptr drmmode, int id,
			       void *ptr, uint32_t handle);
#endif /* XF86DRM_MODE */

#endif /* DRMMODE_DISPLAY_H */
