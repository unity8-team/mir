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
 *    Dave Airlie <airlied@redhat.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef XF86DRM_MODE
#include <sys/ioctl.h>
#include "micmap.h"
#include "xf86cmap.h"
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_drm.h"
#include "sarea.h"

#include "drmmode_display.h"

/* DPMS */
#ifdef HAVE_XEXTPROTO_71
#include <X11/extensions/dpmsconst.h>
#else
#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#endif


static void
drmmode_ConvertFromKMode(ScrnInfoPtr	scrn,
		     drmModeModeInfo *kmode,
		     DisplayModePtr	mode)
{
	memset(mode, 0, sizeof(DisplayModeRec));
	mode->status = MODE_OK;

	mode->Clock = kmode->clock;

	mode->HDisplay = kmode->hdisplay;
	mode->HSyncStart = kmode->hsync_start;
	mode->HSyncEnd = kmode->hsync_end;
	mode->HTotal = kmode->htotal;
	mode->HSkew = kmode->hskew;

	mode->VDisplay = kmode->vdisplay;
	mode->VSyncStart = kmode->vsync_start;
	mode->VSyncEnd = kmode->vsync_end;
	mode->VTotal = kmode->vtotal;
	mode->VScan = kmode->vscan;

	mode->Flags = kmode->flags; //& FLAG_BITS;
	mode->name = strdup(kmode->name);

	if (kmode->type & DRM_MODE_TYPE_DRIVER)
		mode->type = M_T_DRIVER;
	if (kmode->type & DRM_MODE_TYPE_PREFERRED)
		mode->type |= M_T_PREFERRED;
	xf86SetModeCrtc (mode, scrn->adjustFlags);
}

static void
drmmode_ConvertToKMode(ScrnInfoPtr	scrn,
		     drmModeModeInfo *kmode,
		     DisplayModePtr	mode)
{
	memset(kmode, 0, sizeof(*kmode));

	kmode->clock = mode->Clock;
	kmode->hdisplay = mode->HDisplay;
	kmode->hsync_start = mode->HSyncStart;
	kmode->hsync_end = mode->HSyncEnd;
	kmode->htotal = mode->HTotal;
	kmode->hskew = mode->HSkew;

	kmode->vdisplay = mode->VDisplay;
	kmode->vsync_start = mode->VSyncStart;
	kmode->vsync_end = mode->VSyncEnd;
	kmode->vtotal = mode->VTotal;
	kmode->vscan = mode->VScan;

	kmode->flags = mode->Flags; //& FLAG_BITS;
	if (mode->name)
		strncpy(kmode->name, mode->name, DRM_DISPLAY_MODE_LEN);
	kmode->name[DRM_DISPLAY_MODE_LEN-1] = 0;

}

static void
drmmode_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
#if 0
	xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
//	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
//	drmmode_ptr drmmode = drmmode_crtc->drmmode;

	/* bonghits in the randr 1.2 - uses dpms to disable crtc - bad buzz */
	if (mode == DPMSModeOff) {
//		drmModeSetCrtc(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id,
//			       0, 0, 0, NULL, 0, NULL);
	}
#endif
}

static PixmapPtr
create_pixmap_for_fbcon(drmmode_ptr drmmode,
			ScrnInfoPtr pScrn, int crtc_id)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	drmmode_crtc_private_ptr drmmode_crtc;
	ScreenPtr pScreen = pScrn->pScreen;
	PixmapPtr pixmap;
	struct radeon_bo *bo;
	drmModeFBPtr fbcon;
	struct drm_gem_flink flink;

	drmmode_crtc = xf86_config->crtc[crtc_id]->driver_private;

	fbcon = drmModeGetFB(drmmode->fd, drmmode_crtc->mode_crtc->buffer_id);
	if (fbcon == NULL)
		return NULL;

	flink.handle = fbcon->handle;
	if (ioctl(drmmode->fd, DRM_IOCTL_GEM_FLINK, &flink) < 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Couldn't flink fbcon handle\n");
		return NULL;
	}

	bo = radeon_bo_open(drmmode->bufmgr, flink.name, 0, 0, 0, 0);
	if (bo == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Couldn't allocate bo for fbcon handle\n");
		return NULL;
	}

	pixmap = GetScratchPixmapHeader(pScreen,
					fbcon->width, fbcon->height,
					fbcon->depth, fbcon->bpp,
					fbcon->pitch, NULL);
	if (pixmap == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Couldn't allocate pixmap fbcon contents\n");
		return NULL;
	}

	radeon_set_pixmap_bo(pixmap, bo);

	radeon_bo_unref(bo);
	drmModeFreeFB(fbcon);
	return pixmap;
}

void drmmode_copy_fb(ScrnInfoPtr pScrn, drmmode_ptr drmmode)
{
	xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	RADEONInfoPtr info = RADEONPTR(pScrn);
	PixmapPtr src, dst;
	ScreenPtr pScreen = pScrn->pScreen;
	int crtc_id = 0;
	int i;
	int pitch = pScrn->displayWidth * info->CurrentLayout.pixel_bytes;

	if (info->ChipFamily >= CHIP_FAMILY_R600)
		return;

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];
		drmmode_crtc_private_ptr drmmode_crtc;

		drmmode_crtc = crtc->driver_private;
		if (drmmode_crtc->mode_crtc->buffer_id)
			crtc_id = i;
	}

	src = create_pixmap_for_fbcon(drmmode, pScrn, crtc_id);
	if (!src)
		return;

	dst = GetScratchPixmapHeader(pScreen,
				     pScrn->virtualX, pScrn->virtualY,
				     pScrn->depth, pScrn->bitsPerPixel,
				     pitch, NULL);
	radeon_set_pixmap_bo(dst, info->front_bo);
	info->accel_state->exa->PrepareCopy (src, dst,
					     -1, -1, GXcopy, FB_ALLONES);
	info->accel_state->exa->Copy (dst, 0, 0, 0, 0,
				      pScrn->virtualX, pScrn->virtualY);
	info->accel_state->exa->DoneCopy (dst);
	radeon_cs_flush_indirect(pScrn);

	(*pScreen->DestroyPixmap)(dst);
	(*pScreen->DestroyPixmap)(src);

}

static Bool
drmmode_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
		     Rotation rotation, int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	RADEONInfoPtr info = RADEONPTR(pScrn);
	xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	int saved_x, saved_y;
	Rotation saved_rotation;
	DisplayModeRec saved_mode;
	uint32_t *output_ids;
	int output_count = 0;
	Bool ret = TRUE;
	int i;
	int fb_id;
	drmModeModeInfo kmode;
	int pitch = pScrn->displayWidth * info->CurrentLayout.pixel_bytes;

	if (drmmode->fb_id == 0) {
		ret = drmModeAddFB(drmmode->fd,
				   pScrn->virtualX, pScrn->virtualY,
                                   pScrn->depth, pScrn->bitsPerPixel,
				   pitch,
				   info->front_bo->handle,
                                   &drmmode->fb_id);
                if (ret < 0) {
                        ErrorF("failed to add fb\n");
                        return FALSE;
                }
        }

	saved_mode = crtc->mode;
	saved_x = crtc->x;
	saved_y = crtc->y;
	saved_rotation = crtc->rotation;

	if (mode) {
		crtc->mode = *mode;
		crtc->x = x;
		crtc->y = y;
		crtc->rotation = rotation;
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,5,99,0,0)
		crtc->transformPresent = FALSE;
#endif
	}

	output_ids = xcalloc(sizeof(uint32_t), xf86_config->num_output);
	if (!output_ids) {
		ret = FALSE;
		goto done;
	}

	if (mode) {
		for (i = 0; i < xf86_config->num_output; i++) {
			xf86OutputPtr output = xf86_config->output[i];
			drmmode_output_private_ptr drmmode_output;

			if (output->crtc != crtc)
				continue;

			drmmode_output = output->driver_private;
			output_ids[output_count] = drmmode_output->mode_output->connector_id;
			output_count++;
		}

		if (!xf86CrtcRotate(crtc)) {
			goto done;
		}

		drmmode_ConvertToKMode(crtc->scrn, &kmode, mode);

		fb_id = drmmode->fb_id;
		if (drmmode_crtc->rotate_fb_id) {
			fb_id = drmmode_crtc->rotate_fb_id;
			x = y = 0;
		}
		ret = drmModeSetCrtc(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id,
				     fb_id, x, y, output_ids, output_count, &kmode);
		if (ret)
			xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
				   "failed to set mode: %s", strerror(-ret));
		else
			ret = TRUE;

		if (crtc->scrn->pScreen)
			xf86CrtcSetScreenSubpixelOrder(crtc->scrn->pScreen);
		/* go through all the outputs and force DPMS them back on? */
		for (i = 0; i < xf86_config->num_output; i++) {
			xf86OutputPtr output = xf86_config->output[i];

			if (output->crtc != crtc)
				continue;

			output->funcs->dpms(output, DPMSModeOn);
		}
	}



done:
	if (!ret) {
		crtc->x = saved_x;
		crtc->y = saved_y;
		crtc->rotation = saved_rotation;
		crtc->mode = saved_mode;
	}
#if defined(XF86_CRTC_VERSION) && XF86_CRTC_VERSION >= 3
	else
		crtc->active = TRUE;
#endif

	return ret;
}

static void
drmmode_set_cursor_colors (xf86CrtcPtr crtc, int bg, int fg)
{

}

static void
drmmode_set_cursor_position (xf86CrtcPtr crtc, int x, int y)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;

	drmModeMoveCursor(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id, x, y);
}

static void
drmmode_load_cursor_argb (xf86CrtcPtr crtc, CARD32 *image)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	void *ptr;

	/* cursor should be mapped already */
	ptr = drmmode_crtc->cursor_bo->ptr;

	memcpy (ptr, image, 64 * 64 * 4);

	return;
}


static void
drmmode_hide_cursor (xf86CrtcPtr crtc)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;

	drmModeSetCursor(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id, 0, 64, 64);

}

static void
drmmode_show_cursor (xf86CrtcPtr crtc)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	uint32_t handle = drmmode_crtc->cursor_bo->handle;

	drmModeSetCursor(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id, handle, 64, 64);
}

static void *
drmmode_crtc_shadow_allocate(xf86CrtcPtr crtc, int width, int height)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	int size;
	struct radeon_bo *rotate_bo;
	int ret;
	unsigned long rotate_pitch;

	width = RADEON_ALIGN(width, 63);
	rotate_pitch = width * drmmode->cpp;

	size = rotate_pitch * height;

	rotate_bo = radeon_bo_open(drmmode->bufmgr, 0, size, 0, RADEON_GEM_DOMAIN_VRAM, 0);
	if (rotate_bo == NULL)
		return NULL;

	radeon_bo_map(rotate_bo, 1);

	ret = drmModeAddFB(drmmode->fd, width, height, crtc->scrn->depth,
			   crtc->scrn->bitsPerPixel, rotate_pitch,
			   rotate_bo->handle,
			   &drmmode_crtc->rotate_fb_id);
	if (ret) {
		ErrorF("failed to add rotate fb\n");
	}

	drmmode_crtc->rotate_bo = rotate_bo;
	return drmmode_crtc->rotate_bo->ptr;
}

static PixmapPtr
drmmode_crtc_shadow_create(xf86CrtcPtr crtc, void *data, int width, int height)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	unsigned long rotate_pitch;
	PixmapPtr rotate_pixmap;

	if (!data)
		data = drmmode_crtc_shadow_allocate (crtc, width, height);

	rotate_pitch = RADEON_ALIGN(width, 63) * drmmode->cpp;

	rotate_pixmap = GetScratchPixmapHeader(pScrn->pScreen,
					       width, height,
					       pScrn->depth,
					       pScrn->bitsPerPixel,
					       rotate_pitch,
					       data);

	if (rotate_pixmap == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Couldn't allocate shadow pixmap for rotated CRTC\n");
	}

	if (drmmode_crtc->rotate_bo)
		radeon_set_pixmap_bo(rotate_pixmap, drmmode_crtc->rotate_bo);
	return rotate_pixmap;

}

static void
drmmode_crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr rotate_pixmap, void *data)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;

	if (rotate_pixmap)
	    FreeScratchPixmapHeader(rotate_pixmap);

	if (data) {
		drmModeRmFB(drmmode->fd, drmmode_crtc->rotate_fb_id);
		drmmode_crtc->rotate_fb_id = 0;
		radeon_bo_unmap(drmmode_crtc->rotate_bo);
		radeon_bo_unref(drmmode_crtc->rotate_bo);
		drmmode_crtc->rotate_bo = NULL;
	}

}

static void
drmmode_crtc_gamma_set(xf86CrtcPtr crtc, uint16_t *red, uint16_t *green,
                      uint16_t *blue, int size)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;

	drmModeCrtcSetGamma(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id,
			    size, red, green, blue);
}

static const xf86CrtcFuncsRec drmmode_crtc_funcs = {
    .dpms = drmmode_crtc_dpms,
    .set_mode_major = drmmode_set_mode_major,
    .set_cursor_colors = drmmode_set_cursor_colors,
    .set_cursor_position = drmmode_set_cursor_position,
    .show_cursor = drmmode_show_cursor,
    .hide_cursor = drmmode_hide_cursor,
    .load_cursor_argb = drmmode_load_cursor_argb,

    .gamma_set = drmmode_crtc_gamma_set,
    .shadow_create = drmmode_crtc_shadow_create,
    .shadow_allocate = drmmode_crtc_shadow_allocate,
    .shadow_destroy = drmmode_crtc_shadow_destroy,
    .destroy = NULL, /* XXX */
};


static void
drmmode_crtc_init(ScrnInfoPtr pScrn, drmmode_ptr drmmode, int num)
{
	xf86CrtcPtr crtc;
	drmmode_crtc_private_ptr drmmode_crtc;

	crtc = xf86CrtcCreate(pScrn, &drmmode_crtc_funcs);
	if (crtc == NULL)
		return;

	drmmode_crtc = xnfcalloc(sizeof(drmmode_crtc_private_rec), 1);
	drmmode_crtc->mode_crtc = drmModeGetCrtc(drmmode->fd, drmmode->mode_res->crtcs[num]);
	drmmode_crtc->drmmode = drmmode;
	crtc->driver_private = drmmode_crtc;

	return;
}

static xf86OutputStatus
drmmode_output_detect(xf86OutputPtr output)
{
	/* go to the hw and retrieve a new output struct */
	drmmode_output_private_ptr drmmode_output = output->driver_private;
	drmmode_ptr drmmode = drmmode_output->drmmode;
	xf86OutputStatus status;
	drmModeFreeConnector(drmmode_output->mode_output);

	drmmode_output->mode_output = drmModeGetConnector(drmmode->fd, drmmode_output->output_id);

	switch (drmmode_output->mode_output->connection) {
	case DRM_MODE_CONNECTED:
		status = XF86OutputStatusConnected;
		break;
	case DRM_MODE_DISCONNECTED:
		status = XF86OutputStatusDisconnected;
		break;
	default:
	case DRM_MODE_UNKNOWNCONNECTION:
		status = XF86OutputStatusUnknown;
		break;
	}
	return status;
}

static Bool
drmmode_output_mode_valid(xf86OutputPtr output, DisplayModePtr pModes)
{
	return MODE_OK;
}

static DisplayModePtr
drmmode_output_get_modes(xf86OutputPtr output)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;
	drmModeConnectorPtr koutput = drmmode_output->mode_output;
	drmmode_ptr drmmode = drmmode_output->drmmode;
	int i;
	DisplayModePtr Modes = NULL, Mode;
	drmModePropertyPtr props;

	/* look for an EDID property */
	for (i = 0; i < koutput->count_props; i++) {
		props = drmModeGetProperty(drmmode->fd, koutput->props[i]);
		if (props && (props->flags & DRM_MODE_PROP_BLOB)) {
			if (!strcmp(props->name, "EDID")) {
				if (drmmode_output->edid_blob)
					drmModeFreePropertyBlob(drmmode_output->edid_blob);
				drmmode_output->edid_blob = drmModeGetPropertyBlob(drmmode->fd, koutput->prop_values[i]);
			}
			drmModeFreeProperty(props);
		}
	}

	if (drmmode_output->edid_blob)
		xf86OutputSetEDID(output, xf86InterpretEDID(output->scrn->scrnIndex, drmmode_output->edid_blob->data));
	else
		xf86OutputSetEDID(output, xf86InterpretEDID(output->scrn->scrnIndex, NULL));

	/* modes should already be available */
	for (i = 0; i < koutput->count_modes; i++) {
		Mode = xnfalloc(sizeof(DisplayModeRec));

		drmmode_ConvertFromKMode(output->scrn, &koutput->modes[i], Mode);
		Modes = xf86ModesAdd(Modes, Mode);

	}
	return Modes;
}

static void
drmmode_output_destroy(xf86OutputPtr output)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;

	if (drmmode_output->edid_blob)
		drmModeFreePropertyBlob(drmmode_output->edid_blob);
	drmModeFreeConnector(drmmode_output->mode_output);
	xfree(drmmode_output);
	output->driver_private = NULL;
}

static void
drmmode_output_dpms(xf86OutputPtr output, int mode)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;
	drmModeConnectorPtr koutput = drmmode_output->mode_output;
	drmmode_ptr drmmode = drmmode_output->drmmode;

	drmModeConnectorSetProperty(drmmode->fd, koutput->connector_id,
				    drmmode_output->dpms_enum_id, mode);
	return;
}

static const xf86OutputFuncsRec drmmode_output_funcs = {
    .dpms = drmmode_output_dpms,
#if 0

    .save = drmmode_crt_save,
    .restore = drmmode_crt_restore,
    .mode_fixup = drmmode_crt_mode_fixup,
    .prepare = drmmode_output_prepare,
    .mode_set = drmmode_crt_mode_set,
    .commit = drmmode_output_commit,
#endif
    .detect = drmmode_output_detect,
    .mode_valid = drmmode_output_mode_valid,

    .get_modes = drmmode_output_get_modes,
    .destroy = drmmode_output_destroy
};

static int subpixel_conv_table[7] = { 0, SubPixelUnknown,
				      SubPixelHorizontalRGB,
				      SubPixelHorizontalBGR,
				      SubPixelVerticalRGB,
				      SubPixelVerticalBGR,
				      SubPixelNone };

const char *output_names[] = { "None",
			       "VGA",
			       "DVI",
			       "DVI",
			       "DVI",
			       "Composite",
			       "S-video",
			       "LVDS",
			       "CTV",
			       "DIN",
			       "DP",
			       "HDMI",
			       "HDMI",
};

static void
drmmode_output_init(ScrnInfoPtr pScrn, drmmode_ptr drmmode, int num)
{
	xf86OutputPtr output;
	drmModeConnectorPtr koutput;
	drmModeEncoderPtr kencoder;
	drmmode_output_private_ptr drmmode_output;
	drmModePropertyPtr props;
	char name[32];
	int i;

	koutput = drmModeGetConnector(drmmode->fd, drmmode->mode_res->connectors[num]);
	if (!koutput)
		return;

	kencoder = drmModeGetEncoder(drmmode->fd, koutput->encoders[0]);
	if (!kencoder) {
		drmModeFreeConnector(koutput);
		return;
	}

	/* need to do smart conversion here for compat with non-kms ATI driver */
	if (koutput->connector_type_id == 1) {
	    switch(koutput->connector_type) {
	    case DRM_MODE_CONNECTOR_VGA:
	    case DRM_MODE_CONNECTOR_DVII:
	    case DRM_MODE_CONNECTOR_DVID:
	    case DRM_MODE_CONNECTOR_DVIA:
	    case DRM_MODE_CONNECTOR_HDMIA:
	    case DRM_MODE_CONNECTOR_HDMIB:
		snprintf(name, 32, "%s-%d", output_names[koutput->connector_type], koutput->connector_type_id - 1);
		break;
	    default:
		snprintf(name, 32, "%s", output_names[koutput->connector_type]);
		break;
	    }
	} else {
	    snprintf(name, 32, "%s-%d", output_names[koutput->connector_type], koutput->connector_type_id - 1);
	}

	output = xf86OutputCreate (pScrn, &drmmode_output_funcs, name);
	if (!output) {
		drmModeFreeEncoder(kencoder);
		drmModeFreeConnector(koutput);
		return;
	}

	drmmode_output = xcalloc(sizeof(drmmode_output_private_rec), 1);
	if (!drmmode_output) {
		xf86OutputDestroy(output);
		drmModeFreeConnector(koutput);
		drmModeFreeEncoder(kencoder);
		return;
	}

	drmmode_output->output_id = drmmode->mode_res->connectors[num];
	drmmode_output->mode_output = koutput;
	drmmode_output->mode_encoder = kencoder;
	drmmode_output->drmmode = drmmode;
	output->mm_width = koutput->mmWidth;
	output->mm_height = koutput->mmHeight;

	output->subpixel_order = subpixel_conv_table[koutput->subpixel];
	output->driver_private = drmmode_output;

	output->possible_crtcs = kencoder->possible_crtcs;
	output->possible_clones = kencoder->possible_clones;

	for (i = 0; i < koutput->count_props; i++) {
		props = drmModeGetProperty(drmmode->fd, koutput->props[i]);
		if (props && (props->flags && DRM_MODE_PROP_ENUM)) {
			if (!strcmp(props->name, "DPMS")) {
				drmmode_output->dpms_enum_id = koutput->props[i];
				drmModeFreeProperty(props);
				break;
			}
			drmModeFreeProperty(props);
		}
	}

	return;
}

static Bool
drmmode_xf86crtc_resize (ScrnInfoPtr scrn, int width, int height)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	drmmode_crtc_private_ptr
		    drmmode_crtc = xf86_config->crtc[0]->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	RADEONInfoPtr info = RADEONPTR(scrn);
	struct radeon_bo *old_front = NULL;
	Bool	    ret;
	ScreenPtr   screen = screenInfo.screens[scrn->scrnIndex];
	uint32_t    old_fb_id;
	int	    i, pitch, old_width, old_height, old_pitch;
	int screen_size;
	int cpp = info->CurrentLayout.pixel_bytes;
	struct radeon_bo *front_bo;

	if (scrn->virtualX == width && scrn->virtualY == height)
		return TRUE;

	front_bo = radeon_get_pixmap_bo(screen->GetScreenPixmap(screen));
	radeon_cs_flush_indirect(scrn);

	if (front_bo)
		radeon_bo_wait(front_bo);

	pitch = RADEON_ALIGN(width, 63);
	height = RADEON_ALIGN(height, 16);

	screen_size = pitch * height * cpp;

	xf86DrvMsg(scrn->scrnIndex, X_INFO,
		   "Allocate new frame buffer %dx%d stride %d\n",
		   width, height, pitch);

	old_width = scrn->virtualX;
	old_height = scrn->virtualY;
	old_pitch = scrn->displayWidth;
	old_fb_id = drmmode->fb_id;
	old_front = info->front_bo;

	scrn->virtualX = width;
	scrn->virtualY = height;
	scrn->displayWidth = pitch;

	info->front_bo = radeon_bo_open(info->bufmgr, 0, screen_size, 0, RADEON_GEM_DOMAIN_VRAM, 0);
	if (!info->front_bo)
		goto fail;

	ret = drmModeAddFB(drmmode->fd, width, height, scrn->depth,
			   scrn->bitsPerPixel, pitch * cpp,
			   info->front_bo->handle,
			   &drmmode->fb_id);
	if (ret)
		goto fail;

	radeon_set_pixmap_bo(screen->GetScreenPixmap(screen), info->front_bo);
	screen->ModifyPixmapHeader(screen->GetScreenPixmap(screen),
				   width, height, -1, -1, pitch * cpp, NULL);

	//	xf86DrvMsg(scrn->scrnIndex, X_INFO, "New front buffer at 0x%lx\n",
	//		   info->front_bo-);

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		if (!crtc->enabled)
			continue;

		drmmode_set_mode_major(crtc, &crtc->mode,
				       crtc->rotation, crtc->x, crtc->y);
	}

	if (old_fb_id)
		drmModeRmFB(drmmode->fd, old_fb_id);
	if (old_front)
		radeon_bo_unref(old_front);

	return TRUE;

 fail:
	if (info->front_bo)
		radeon_bo_unref(info->front_bo);
	info->front_bo = old_front;
	scrn->virtualX = old_width;
	scrn->virtualY = old_height;
	scrn->displayWidth = old_pitch;
	drmmode->fb_id = old_fb_id;

	return FALSE;
}

static const xf86CrtcConfigFuncsRec drmmode_xf86crtc_config_funcs = {
	drmmode_xf86crtc_resize
};


Bool drmmode_pre_init(ScrnInfoPtr pScrn, drmmode_ptr drmmode, char *busId, char *driver_name, int cpp, int zaphod_mask)
{
	xf86CrtcConfigPtr   xf86_config;
	RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
	int i;
	Bool ret;

	/* Create a bus Id */
	/* Low level DRM open */
	if (!pRADEONEnt->fd) {
		ret = DRIOpenDRMMaster(pScrn, SAREA_MAX, busId, driver_name);
		if (!ret) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "[dri] DRIGetVersion failed to open the DRM\n"
				   "[dri] Disabling DRI.\n");
			return FALSE;
		}

		drmmode->fd = DRIMasterFD(pScrn);
		pRADEONEnt->fd = drmmode->fd;
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				" reusing fd for second head\n");
		drmmode->fd = pRADEONEnt->fd;
	}
	xf86CrtcConfigInit(pScrn, &drmmode_xf86crtc_config_funcs);
	xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);

	drmmode->cpp = cpp;
	drmmode->mode_res = drmModeGetResources(drmmode->fd);
	if (!drmmode->mode_res)
		return FALSE;

	xf86CrtcSetSizeRange(pScrn, 320, 200, drmmode->mode_res->max_width, drmmode->mode_res->max_height);
	for (i = 0; i < drmmode->mode_res->count_crtcs; i++)
		if (zaphod_mask & (1 << i))
			drmmode_crtc_init(pScrn, drmmode, i);

	for (i = 0; i < drmmode->mode_res->count_connectors; i++)
		if (zaphod_mask & (1 << i))
			drmmode_output_init(pScrn, drmmode, i);

	xf86InitialConfiguration(pScrn, TRUE);

	return TRUE;
}

Bool drmmode_set_bufmgr(ScrnInfoPtr pScrn, drmmode_ptr drmmode, struct radeon_bo_manager *bufmgr)
{
	drmmode->bufmgr = bufmgr;
	return TRUE;
}



void drmmode_set_cursor(ScrnInfoPtr scrn, drmmode_ptr drmmode, int id, struct radeon_bo *bo)
{
	xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	xf86CrtcPtr crtc = xf86_config->crtc[id];
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;

	drmmode_crtc->cursor_bo = bo;
}

void drmmode_adjust_frame(ScrnInfoPtr pScrn, drmmode_ptr drmmode, int x, int y, int flags)
{
	xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(pScrn);
	xf86OutputPtr  output = config->output[config->compat_output];
	xf86CrtcPtr	crtc = output->crtc;

	if (crtc && crtc->enabled) {
		drmmode_set_mode_major(crtc, &crtc->mode, crtc->rotation,
				       x, y);
	}
}

Bool drmmode_set_desired_modes(ScrnInfoPtr pScrn, drmmode_ptr drmmode)
{
	xf86CrtcConfigPtr   config = XF86_CRTC_CONFIG_PTR(pScrn);
	int c;

	drmmode_copy_fb(pScrn, drmmode);

	for (c = 0; c < config->num_crtc; c++) {
		xf86CrtcPtr	crtc = config->crtc[c];
		drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
		xf86OutputPtr	output = NULL;
		int		o;

		/* Skip disabled CRTCs */
		if (!crtc->enabled) {
			drmModeSetCrtc(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id,
				       0, 0, 0, NULL, 0, NULL);
			continue;
		}

		if (config->output[config->compat_output]->crtc == crtc)
			output = config->output[config->compat_output];
		else
		{
			for (o = 0; o < config->num_output; o++)
				if (config->output[o]->crtc == crtc)
				{
					output = config->output[o];
					break;
				}
		}
		/* paranoia */
		if (!output)
			continue;

		/* Mark that we'll need to re-set the mode for sure */
		memset(&crtc->mode, 0, sizeof(crtc->mode));
		if (!crtc->desiredMode.CrtcHDisplay)
		{
			DisplayModePtr  mode = xf86OutputFindClosestMode (output, pScrn->currentMode);

			if (!mode)
				return FALSE;
			crtc->desiredMode = *mode;
			crtc->desiredRotation = RR_Rotate_0;
			crtc->desiredX = 0;
			crtc->desiredY = 0;
		}

		if (!crtc->funcs->set_mode_major(crtc, &crtc->desiredMode, crtc->desiredRotation,
						 crtc->desiredX, crtc->desiredY))
			return FALSE;
	}
	return TRUE;
}

static void drmmode_load_palette(ScrnInfoPtr pScrn, int numColors,
                                 int *indices, LOCO *colors, VisualPtr pVisual)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    uint16_t       lut_r[256], lut_g[256], lut_b[256];
    int index, j, i;
    int c;

    for (c = 0; c < xf86_config->num_crtc; c++) {
        xf86CrtcPtr crtc = xf86_config->crtc[c];
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;

        for (i = 0 ; i < 256; i++) {
            lut_r[i] = drmmode_crtc->lut_r[i] << 6;
            lut_g[i] = drmmode_crtc->lut_g[i] << 6;
            lut_b[i] = drmmode_crtc->lut_b[i] << 6;
        }

        switch(pScrn->depth) {
        case 15:
            for (i = 0; i < numColors; i++) {
                index = indices[i];
                for (j = 0; j < 8; j++) {
                    lut_r[index * 8 + j] = colors[index].red << 6;
                    lut_g[index * 8 + j] = colors[index].green << 6;
                    lut_b[index * 8 + j] = colors[index].blue << 6;
                }
            }
         break;
         case 16:
             for (i = 0; i < numColors; i++) {
                 index = indices[i];

                  if (i <= 31) {
                      for (j = 0; j < 8; j++) {
                          lut_r[index * 8 + j] = colors[index].red << 6;
                          lut_b[index * 8 + j] = colors[index].blue << 6;
                      }
                  }

                  for (j = 0; j < 4; j++) {
                      lut_g[index * 4 + j] = colors[index].green << 6;
                  }
              }
	  break;
          default:
              for (i = 0; i < numColors; i++) {
                  index = indices[i];
                  lut_r[index] = colors[index].red << 6;
                  lut_g[index] = colors[index].green << 6;
                  lut_b[index] = colors[index].blue << 6;
              }
              break;
          }

    /* Make the change through RandR */
#ifdef RANDR_12_INTERFACE
        if (crtc->randr_crtc)
            RRCrtcGammaSet(crtc->randr_crtc, lut_r, lut_g, lut_b);
        else
#endif
            crtc->funcs->gamma_set(crtc, lut_r, lut_g, lut_b, 256);
     }
}

Bool drmmode_setup_colormap(ScreenPtr pScreen, ScrnInfoPtr pScrn)
{
    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
                  "Initializing kms color map\n");
    if (!miCreateDefColormap(pScreen))
        return FALSE;
    /* all radeons support 10 bit CLUTs */
    if (!xf86HandleColormaps(pScreen, 256, 10,
                             drmmode_load_palette, NULL,
                             CMAP_PALETTED_TRUECOLOR
#if 0 /* This option messes up text mode! (eich@suse.de) */
                             | CMAP_LOAD_EVEN_IF_OFFSCREEN
#endif
                             | CMAP_RELOAD_ON_MODE_SWITCH))
         return FALSE;
    return TRUE;
}
#endif
