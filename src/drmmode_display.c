/*
 * Copyright © 2007 Red Hat, Inc.
 * Copyright © 2008 Maarten Maathuis
 *
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

#include "xorgVersion.h"

#ifdef XF86DRM_MODE
#include "nv_include.h"
#include "xf86drmMode.h"
#include "X11/Xatom.h"

#include <sys/ioctl.h>

typedef struct {
    int fd;
    uint32_t fb_id;
    drmModeResPtr mode_res;
    int cpp;
} drmmode_rec, *drmmode_ptr;

typedef struct {
    drmmode_ptr drmmode;
    drmModeCrtcPtr mode_crtc;
    struct nouveau_bo *cursor;
    struct nouveau_bo *rotate_bo;
    uint32_t rotate_pitch;
    PixmapPtr rotate_pixmap;
    uint32_t rotate_fb_id;
} drmmode_crtc_private_rec, *drmmode_crtc_private_ptr;

typedef struct {
    drmmode_ptr drmmode;
    int output_id;
    drmModeConnectorPtr mode_output;
    drmModeEncoderPtr mode_encoder;
    drmModePropertyBlobPtr edid_blob;
} drmmode_output_private_rec, *drmmode_output_private_ptr;

static void drmmode_output_dpms(xf86OutputPtr output, int mode);

static void
drmmode_ConvertFromKMode(ScrnInfoPtr scrn, drmModeModeInfo *kmode,
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
drmmode_ConvertToKMode(ScrnInfoPtr scrn, drmModeModeInfo *kmode,
		       DisplayModePtr mode)
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
drmmode_crtc_dpms(xf86CrtcPtr drmmode_crtc, int mode)
{

}

static PixmapPtr
drmmode_fb_pixmap(ScrnInfoPtr pScrn, int id, unsigned *w, unsigned *h)
{
	ScreenPtr pScreen = pScrn->pScreen;
	struct nouveau_pixmap *nvpix;
	NVPtr pNv = NVPTR(pScrn);
	drmModeFBPtr fb;
	PixmapPtr ppix;
	int ret;

	fb = drmModeGetFB(nouveau_device(pNv->dev)->fd, id);
	if (!fb)
		return NULL;

	ppix = pScreen->CreatePixmap(pScreen, 0, 0, fb->depth, 0);
	nvpix = nouveau_pixmap(ppix);
	if (!nvpix) {
		pScreen->DestroyPixmap(ppix);
		drmFree(fb);
		return NULL;
	}

	miModifyPixmapHeader(ppix, fb->width, fb->height, fb->depth,
			     pScrn->bitsPerPixel, fb->pitch, NULL);
	if (w && h) {
		if (fb->width < *w)
			*w = fb->width;
		if (fb->height < *h)
			*h = fb->height;
	}

	ret = nouveau_bo_wrap(pNv->dev, fb->handle, &nvpix->bo);
	drmFree(fb);
	if (ret) {
		pScreen->DestroyPixmap(ppix);
		return NULL;
	}

	return ppix;
}

static void
drmmode_fb_copy(ScrnInfoPtr pScrn, drmmode_ptr drmmode, int dst_id, int src_id,
		int x, int y)
{
	ScreenPtr pScreen = pScrn->pScreen;
	NVPtr pNv = NVPTR(pScrn);
	ExaDriverPtr exa = pNv->EXADriverPtr;
	PixmapPtr pspix, pdpix;
	unsigned w = -1, h = -1;

	if (!src_id || !dst_id)
		return;

	pspix = drmmode_fb_pixmap(pScrn, src_id, &w, &h);
	if (!pspix)
		return;

	pdpix = drmmode_fb_pixmap(pScrn, dst_id, &w, &h);
	if (!pdpix) {
		pScreen->DestroyPixmap(pspix);
		return;
	}

	exa->PrepareCopy(pspix, pdpix, 0, 0, GXcopy, ~0);
	exa->Copy(pdpix, 0, 0, x, y, w, h);
	exa->DoneCopy(pdpix);
	FIRE_RING (pNv->chan);

	pScreen->DestroyPixmap(pdpix);
	pScreen->DestroyPixmap(pspix);
}

static Bool
drmmode_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
		       Rotation rotation, int x, int y)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	int saved_x, saved_y;
	Rotation saved_rotation;
	DisplayModeRec saved_mode;
	uint32_t *output_ids;
	int output_count = 0;
	int ret = TRUE;
	int i;
	int fb_id;
	drmModeModeInfo kmode;

	if (drmmode->fb_id == 0) {
		unsigned int pitch =
			pScrn->displayWidth * (pScrn->bitsPerPixel / 8);

		ret = drmModeAddFB(drmmode->fd,
				   pScrn->virtualX, pScrn->virtualY,
				   pScrn->depth, pScrn->bitsPerPixel,
				   pitch, pNv->scanout->handle,
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

	crtc->mode = *mode;
	crtc->x = x;
	crtc->y = y;
	crtc->rotation = rotation;
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,5,99,0,0)
	crtc->transformPresent = FALSE;
#endif

	output_ids = xcalloc(sizeof(uint32_t), xf86_config->num_output);
	if (!output_ids) {
		ret = FALSE;
		goto done;
	}

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		drmmode_output_private_ptr drmmode_output;

		if (output->crtc != crtc)
			continue;

		drmmode_output = output->driver_private;
		output_ids[output_count] =
			drmmode_output->mode_output->connector_id;
		output_count++;
	}

#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1,5,99,0,0)
	if (!xf86CrtcRotate(crtc, mode, rotation))
		goto done;
#else
	if (!xf86CrtcRotate(crtc))
		goto done;
#endif

	drmmode_ConvertToKMode(crtc->scrn, &kmode, mode);

	fb_id = drmmode->fb_id;
	if (drmmode_crtc->rotate_fb_id) {
		fb_id = drmmode_crtc->rotate_fb_id;
		x = 0;
		y = 0;
	} else
	if (fb_id != drmmode_crtc->mode_crtc->buffer_id &&
	    pNv->exa_driver_pixmaps) {
		drmmode_fb_copy(pScrn, drmmode, fb_id,
				drmmode_crtc->mode_crtc->buffer_id, x, y);
	}

	ret = drmModeSetCrtc(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id,
			     fb_id, x, y, output_ids, output_count, &kmode);
	if (ret)
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "failed to set mode: %s", strerror(-ret));
	else
		ret = TRUE;

	/* Work around some xserver stupidity */
	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if (output->crtc != crtc)
			continue;

		drmmode_output_dpms(output, DPMSModeOn);
	}

done:
	if (!ret) {
		crtc->x = saved_x;
		crtc->y = saved_y;
		crtc->rotation = saved_rotation;
		crtc->mode = saved_mode;
	}
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
	struct nouveau_bo *cursor = drmmode_crtc->cursor;

	nouveau_bo_map(cursor, NOUVEAU_BO_WR);
	memcpy(cursor->map, image, 64*64*4);
	nouveau_bo_unmap(cursor);
}


static void
drmmode_hide_cursor (xf86CrtcPtr crtc)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;

	drmModeSetCursor(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id,
			 0, 64, 64);
}

static void
drmmode_show_cursor (xf86CrtcPtr crtc)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;

	drmModeSetCursor(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id,
			 drmmode_crtc->cursor->handle, 64, 64);
}

static void *
drmmode_crtc_shadow_allocate(xf86CrtcPtr crtc, int width, int height)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	NVPtr pNv = NVPTR(crtc->scrn);
	uint32_t tile_mode = 0, tile_flags = 0;
	int ah = height, ret, pitch;
	void *virtual;

	if (pNv->Architecture >= NV_ARCH_50) {
		tile_mode = 4;
		tile_flags = (drmmode->cpp == 2) ? 0x7000 : 0x7a00;
		ah = NOUVEAU_ALIGN(height, 1 << (tile_mode + 2));
		pitch = NOUVEAU_ALIGN(width * drmmode->cpp, 64);
	} else {
		pitch  = nv_pitch_align(pNv, width, crtc->scrn->depth);
		pitch *= drmmode->cpp;
	}
	drmmode_crtc->rotate_pitch = pitch;

	ret = nouveau_bo_new_tile(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_MAP, 0,
				  drmmode_crtc->rotate_pitch * ah, tile_mode,
				  tile_flags, &drmmode_crtc->rotate_bo);
	if (ret) {
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "Couldn't allocate shadow memory for rotated CRTC\n");
		return NULL;
	}

	ret = nouveau_bo_map(drmmode_crtc->rotate_bo, NOUVEAU_BO_RDWR);
	if (ret) {
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "Couldn't get virtual address of shadow scanout\n");
		nouveau_bo_ref(NULL, &drmmode_crtc->rotate_bo);
		return NULL;
	}
	virtual = drmmode_crtc->rotate_bo->map;
	nouveau_bo_unmap(drmmode_crtc->rotate_bo);

	ret = drmModeAddFB(drmmode->fd, width, height, crtc->scrn->depth,
			   crtc->scrn->bitsPerPixel, drmmode_crtc->rotate_pitch,
			   drmmode_crtc->rotate_bo->handle,
			   &drmmode_crtc->rotate_fb_id);
	if (ret) {
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "Error adding FB for shadow scanout: %s\n",
			   strerror(-ret));
		nouveau_bo_ref(NULL, &drmmode_crtc->rotate_bo);
		return NULL;
	}

	return virtual;
}

static PixmapPtr
drmmode_crtc_shadow_create(xf86CrtcPtr crtc, void *data, int width, int height)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	PixmapPtr rotate_pixmap;

	if (!data)
		data = drmmode_crtc_shadow_allocate (crtc, width, height);

	rotate_pixmap = GetScratchPixmapHeader(pScrn->pScreen,
					       width, height,
					       pScrn->depth,
					       pScrn->bitsPerPixel,
					       drmmode_crtc->rotate_pitch,
					       data);

	if (rotate_pixmap == NULL) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Couldn't allocate shadow pixmap for rotated CRTC\n");
	}

	if (drmmode_crtc->rotate_bo && NVPTR(pScrn)->exa_driver_pixmaps) {
		struct nouveau_pixmap *nvpix = nouveau_pixmap(rotate_pixmap);

		if (nvpix)
			nouveau_bo_ref(drmmode_crtc->rotate_bo, &nvpix->bo);
	}

	drmmode_crtc->rotate_pixmap = rotate_pixmap;
	return drmmode_crtc->rotate_pixmap;
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
		nouveau_bo_ref(NULL, &drmmode_crtc->rotate_bo);
		drmmode_crtc->rotate_pixmap = NULL;
	}
}

static void
drmmode_gamma_set(xf86CrtcPtr crtc, CARD16 *red, CARD16 *green, CARD16 *blue,
		  int size)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	int ret;

	ret = drmModeCrtcSetGamma(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id,
				  size, red, green, blue);
	if (ret != 0) {
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "failed to set gamma: %s", strerror(-ret));
	}
}

static const xf86CrtcFuncsRec drmmode_crtc_funcs = {
	.dpms = drmmode_crtc_dpms,
	.set_mode_major = drmmode_set_mode_major,
	.set_cursor_colors = drmmode_set_cursor_colors,
	.set_cursor_position = drmmode_set_cursor_position,
	.show_cursor = drmmode_show_cursor,
	.hide_cursor = drmmode_hide_cursor,
	.load_cursor_argb = drmmode_load_cursor_argb,
	.shadow_create = drmmode_crtc_shadow_create,
	.shadow_allocate = drmmode_crtc_shadow_allocate,
	.shadow_destroy = drmmode_crtc_shadow_destroy,
	.gamma_set = drmmode_gamma_set,
#if 0
	.set_cursor_colors = drmmode_crtc_set_cursor_colors,
#endif
	.destroy = NULL, /* XXX */
};


static void
drmmode_crtc_init(ScrnInfoPtr pScrn, drmmode_ptr drmmode, int num)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcPtr crtc;
	drmmode_crtc_private_ptr drmmode_crtc;
	int ret;

	crtc = xf86CrtcCreate(pScrn, &drmmode_crtc_funcs);
	if (crtc == NULL)
		return;

	drmmode_crtc = xnfcalloc(sizeof(drmmode_crtc_private_rec), 1);
	drmmode_crtc->mode_crtc = drmModeGetCrtc(drmmode->fd,
						 drmmode->mode_res->crtcs[num]);
	drmmode_crtc->drmmode = drmmode;

	ret = nouveau_bo_new(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_MAP, 0,
			     64*64*4, &drmmode_crtc->cursor);
	assert(ret == 0);

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

	drmmode_output->mode_output =
		drmModeGetConnector(drmmode->fd, drmmode_output->output_id);

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
	xf86MonPtr ddc_mon = NULL;

	/* look for an EDID property */
	for (i = 0; i < koutput->count_props; i++) {
		props = drmModeGetProperty(drmmode->fd, koutput->props[i]);
		if (!props || !(props->flags & DRM_MODE_PROP_BLOB))
			continue;

		if (!strcmp(props->name, "EDID")) {
			if (drmmode_output->edid_blob)
				drmModeFreePropertyBlob(drmmode_output->edid_blob);
			drmmode_output->edid_blob =
				drmModeGetPropertyBlob(drmmode->fd,
						       koutput->prop_values[i]);
		}
		drmModeFreeProperty(props);
	}

	if (drmmode_output->edid_blob)
		ddc_mon = xf86InterpretEDID(output->scrn->scrnIndex,
					    drmmode_output->edid_blob->data);
	xf86OutputSetEDID(output, ddc_mon);

	/* modes should already be available */
	for (i = 0; i < koutput->count_modes; i++) {
		Mode = xnfalloc(sizeof(DisplayModeRec));

		drmmode_ConvertFromKMode(output->scrn, &koutput->modes[i],
					 Mode);
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
	drmModePropertyPtr props;
	drmmode_ptr drmmode = drmmode_output->drmmode;
	int mode_id = -1, i;

	if (!NVPTR(output->scrn)->allow_dpms)
		return;

	for (i = 0; i < koutput->count_props; i++) {
		props = drmModeGetProperty(drmmode->fd, koutput->props[i]);
		if (props && (props->flags && DRM_MODE_PROP_ENUM)) {
			if (!strcmp(props->name, "DPMS")) {
				mode_id = koutput->props[i];
				drmModeFreeProperty(props);
				break;
			}
			drmModeFreeProperty(props);
		}
	}

	if (mode_id < 0)
		return;

	drmModeConnectorSetProperty(drmmode->fd, koutput->connector_id,
				    mode_id, mode);
}

/*
 * Several scaling modes exist, let the user choose.
 */
struct drmmode_enum {
	char *name;
	int value;
};

static struct drmmode_enum scaling_mode[] = {
	{ "non-gpu", DRM_MODE_SCALE_NON_GPU },
	{ "fullscreen", DRM_MODE_SCALE_FULLSCREEN },
	{ "aspect", DRM_MODE_SCALE_ASPECT },
	{ "noscale", DRM_MODE_SCALE_NO_SCALE },
	{ NULL, -1 /* invalid */ }
};
static Atom scaling_mode_atom;

static struct drmmode_enum dithering_mode[] = {
	{ "off", DRM_MODE_DITHERING_OFF },
	{ "on", DRM_MODE_DITHERING_ON },
	{ NULL, -1 /* invalid */ }
};
static Atom dithering_atom;

static int
drmmode_enum_lookup_value(struct drmmode_enum *ptr, char *name, int size) {
	if (size == 0)
		size = strlen(name);

	while (ptr->name) {
		if (!strcmp(name, ptr->name))
			return ptr->value;
		ptr++;
	}

	return -1;
}

static char *
drmmode_enum_lookup_name(struct drmmode_enum *ptr, int value)
{
	while (ptr->name) {
		if (ptr->value == value)
			return ptr->name;
		ptr++;
	}

	return NULL;
}

drmModePropertyPtr
drmmode_output_property_find(xf86OutputPtr output, uint32_t type,
			     const char *name)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;
	drmModeConnectorPtr koutput = drmmode_output->mode_output;
	drmmode_ptr drmmode = drmmode_output->drmmode;
	drmModePropertyPtr props;
	int i;

	for (i = 0; i < koutput->count_props; i++) {
		props = drmModeGetProperty(drmmode->fd, koutput->props[i]);

		if (!props || !(props->flags & type))
			continue;
		
		if (!strcmp(props->name, name))
			return props;
	}

	return NULL;
}

static const char *
drmmode_output_property_string(xf86OutputPtr output, struct drmmode_enum *ptr,
			       const char *prop)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;
	drmModeConnectorPtr koutput = drmmode_output->mode_output;
	drmModePropertyPtr props;
	const char *name;
	int i;

	props = drmmode_output_property_find(output, DRM_MODE_PROP_ENUM, prop);
	if (!props)
		return "unknown_prop";

	for (i = 0; i < koutput->count_props; i++) {
		if (koutput->props[i] == props->prop_id)
			break;
	}

	if (koutput->props[i] != props->prop_id)
		return "unknown_output_prop";

	name = drmmode_enum_lookup_name(ptr, koutput->prop_values[i]);
	return name ? name : "unknown_enum";
}

static void
drmmode_output_create_resources(xf86OutputPtr output)
{
	ScrnInfoPtr pScrn = output->scrn;
	const char *name;
	int ret;

	scaling_mode_atom = MakeAtom("SCALING_MODE", 12, TRUE);
	dithering_atom = MakeAtom("DITHERING", 9, TRUE);

	ret = RRConfigureOutputProperty(output->randr_output, scaling_mode_atom,
					FALSE, FALSE, FALSE, 0, NULL);
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Error creating SCALING_MODE property: %d\n", ret);
	}

	name = drmmode_output_property_string(output, scaling_mode,
					      "scaling mode");
	RRChangeOutputProperty(output->randr_output, scaling_mode_atom,
			       XA_STRING, 8, PropModeReplace, strlen(name),
			       (char *)name, FALSE, FALSE);

	ret = RRConfigureOutputProperty(output->randr_output, dithering_atom,
					FALSE, FALSE, FALSE, 0, NULL);
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Error creating DITHERING property: %d\n", ret);
	}

	name = drmmode_output_property_string(output, dithering_mode,
					      "dithering");
	RRChangeOutputProperty(output->randr_output, dithering_atom,
			       XA_STRING, 8, PropModeReplace, strlen(name),
			       (char *)name, FALSE, FALSE);
}

static Bool
drmmode_output_set_property(xf86OutputPtr output, Atom property,
			    RRPropertyValuePtr value)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;
	drmmode_ptr drmmode = drmmode_output->drmmode;
	drmModePropertyPtr props;
	int mode, ret = 0;

	if (property == scaling_mode_atom) {
		if (value->type != XA_STRING || value->format != 8)
			return FALSE;

		mode = drmmode_enum_lookup_value(scaling_mode, value->data,
						 value->size);
		if (mode == -1)
			return FALSE;

		props = drmmode_output_property_find(output, DRM_MODE_PROP_ENUM,
						     "scaling mode");
		if (!props)
			return FALSE;

		ret = drmModeConnectorSetProperty(drmmode->fd,
						  drmmode_output->output_id,
						  props->prop_id, mode);
	} else
	if (property == dithering_atom) {
		if (value->type != XA_STRING || value->format != 8)
			return FALSE;

		mode = drmmode_enum_lookup_value(dithering_mode, value->data,
						 value->size);
		if (mode == -1)
			return FALSE;

		props = drmmode_output_property_find(output, DRM_MODE_PROP_ENUM,
						     "dithering");
		if (!props)
			return FALSE;

		ret = drmModeConnectorSetProperty(drmmode->fd,
						  drmmode_output->output_id,
						  props->prop_id, mode);
	}

	return ret == 0;
}

static const xf86OutputFuncsRec drmmode_output_funcs = {
	.create_resources = drmmode_output_create_resources,
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
	.set_property = drmmode_output_set_property,
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
			       "DVI-I",
			       "DVI-D",
			       "DVI-A",
			       "Composite",
			       "TV",
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
	char name[32];

	koutput = drmModeGetConnector(drmmode->fd,
				      drmmode->mode_res->connectors[num]);
	if (!koutput)
		return;

	kencoder = drmModeGetEncoder(drmmode->fd, koutput->encoders[0]);
	if (!kencoder) {
		drmModeFreeConnector(koutput);
		return;
	}

	snprintf(name, 32, "%s-%d", output_names[koutput->connector_type],
		 koutput->connector_type_id - 1);

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
	return;
}

static Bool
drmmode_xf86crtc_resize(ScrnInfoPtr scrn, int width, int height)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	ScreenPtr screen = screenInfo.screens[scrn->scrnIndex];
	NVPtr pNv = NVPTR(scrn);
	drmmode_crtc_private_ptr
		    drmmode_crtc = xf86_config->crtc[0]->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	uint32_t pitch, old_width, old_height, old_pitch, old_fb_id;
	struct nouveau_bo *old_bo = NULL;
	uint32_t tile_mode = 0, tile_flags = 0, ah = height;
	PixmapPtr ppix;
	int ret, i;

	ErrorF("resize called %d %d\n", width, height);

	if (scrn->virtualX == width && scrn->virtualY == height)
		return TRUE;

	if (pNv->Architecture >= NV_ARCH_50 && pNv->wfb_enabled) {
		tile_mode = 4;
		tile_flags = (scrn->bitsPerPixel == 16) ? 0x7000 : 0x7a00;
		ah = NOUVEAU_ALIGN(height, 1 << (tile_mode + 2));
		pitch = NOUVEAU_ALIGN(width * (scrn->bitsPerPixel >> 3), 64);
	} else {
		pitch  = nv_pitch_align(pNv, width, scrn->depth);
		pitch *= (scrn->bitsPerPixel >> 3);
	}

	old_width = scrn->virtualX;
	old_height = scrn->virtualY;
	old_pitch = scrn->displayWidth;
	old_fb_id = drmmode->fb_id;
	nouveau_bo_ref(pNv->scanout, &old_bo);
	nouveau_bo_ref(NULL, &pNv->scanout);

	scrn->virtualX = width;
	scrn->virtualY = height;
	scrn->displayWidth = pitch / (scrn->bitsPerPixel >> 3);

	ret = nouveau_bo_new_tile(pNv->dev, NOUVEAU_BO_VRAM | NOUVEAU_BO_MAP,
				  0, pitch * ah, tile_mode, tile_flags,
				  &pNv->scanout);
	if (ret)
		goto fail;

	nouveau_bo_map(pNv->scanout, NOUVEAU_BO_RDWR);

	ret = drmModeAddFB(drmmode->fd, width, height, scrn->depth,
			   scrn->bitsPerPixel, pitch, pNv->scanout->handle,
			   &drmmode->fb_id);
	if (ret) {
		nouveau_bo_unmap(pNv->scanout);
		goto fail;
	}

	if (pNv->ShadowPtr) {
		xfree(pNv->ShadowPtr);
		pNv->ShadowPitch = pitch;
		pNv->ShadowPtr = xalloc(pNv->ShadowPitch * height);
	}

	ppix = screen->GetScreenPixmap(screen);
	if (pNv->exa_driver_pixmaps)
		nouveau_bo_ref(pNv->scanout, &nouveau_pixmap(ppix)->bo);
	screen->ModifyPixmapHeader(ppix, width, height, -1, -1, pitch,
				   (!pNv->NoAccel || pNv->ShadowPtr) ?
				   pNv->ShadowPtr : pNv->scanout->map);

	nouveau_bo_unmap(pNv->scanout);

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		if (!crtc->enabled)
			continue;

		drmmode_set_mode_major(crtc, &crtc->mode,
				       crtc->rotation, crtc->x, crtc->y);
	}

	if (old_fb_id)
		drmModeRmFB(drmmode->fd, old_fb_id);
	nouveau_bo_ref(NULL, &old_bo);

	NVDRIFinishScreenInit(scrn, true);
	return TRUE;

 fail:
	nouveau_bo_ref(old_bo, &pNv->scanout);
	scrn->virtualX = old_width;
	scrn->virtualY = old_height;
	scrn->displayWidth = old_pitch;
	drmmode->fb_id = old_fb_id;

	return FALSE;
}

static const xf86CrtcConfigFuncsRec drmmode_xf86crtc_config_funcs = {
	drmmode_xf86crtc_resize
};

Bool drmmode_pre_init(ScrnInfoPtr pScrn, int fd, int cpp)
{
	xf86CrtcConfigPtr   xf86_config;
	drmmode_ptr drmmode;
	int i;

	drmmode = xnfalloc(sizeof *drmmode);
	drmmode->fd = fd;
	drmmode->fb_id = 0;

	xf86CrtcConfigInit(pScrn, &drmmode_xf86crtc_config_funcs);
	xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);

	drmmode->cpp = cpp;
	drmmode->mode_res = drmModeGetResources(drmmode->fd);
	if (!drmmode->mode_res)
		return FALSE;

	xf86CrtcSetSizeRange(pScrn, 320, 200, drmmode->mode_res->max_width,
			     drmmode->mode_res->max_height);
	for (i = 0; i < drmmode->mode_res->count_crtcs; i++)
		drmmode_crtc_init(pScrn, drmmode, i);

	for (i = 0; i < drmmode->mode_res->count_connectors; i++)
		drmmode_output_init(pScrn, drmmode, i);

	xf86InitialConfiguration(pScrn, TRUE);

	return TRUE;
}

Bool
drmmode_is_rotate_pixmap(PixmapPtr ppix, struct nouveau_bo **bo)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR (pScrn);
	int i;

	if (!NVPTR(pScrn)->kms_enable)
		return FALSE;

	for (i = 0; i < config->num_crtc; i++) {
		xf86CrtcPtr crtc = config->crtc[i];
		drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;

		if (!drmmode_crtc->rotate_bo)
			continue;

		if (drmmode_crtc->rotate_pixmap == ppix) {
			if (bo)
				*bo = drmmode_crtc->rotate_bo;
			return TRUE;
		}
	}

	return FALSE;
}

void
drmmode_adjust_frame(ScrnInfoPtr scrn, int x, int y, int flags)
{
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(scrn);
	xf86OutputPtr output = config->output[config->compat_output];
	xf86CrtcPtr crtc = output->crtc;

	if (!crtc || !crtc->enabled)
		return;

	drmmode_set_mode_major(crtc, &crtc->mode, crtc->rotation, x, y);
}

void
drmmode_remove_fb(ScrnInfoPtr pScrn)
{
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(pScrn);
	xf86CrtcPtr crtc = NULL;
	drmmode_crtc_private_ptr drmmode_crtc;
	drmmode_ptr drmmode;

	if (!NVPTR(pScrn)->kms_enable)
		return;

	if (config)
		crtc = config->crtc[0];
	if (!crtc)
		return;

	drmmode_crtc = crtc->driver_private;
	drmmode = drmmode_crtc->drmmode;

	if (drmmode->fb_id)
		drmModeRmFB(drmmode->fd, drmmode->fb_id);
	drmmode->fb_id = 0;
}

#endif
