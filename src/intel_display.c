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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include "xorgVersion.h"

#include "intel.h"
#include "intel_bufmgr.h"
#include "xf86drmMode.h"
#include "X11/Xatom.h"

struct intel_mode {
	int fd;
	uint32_t fb_id;
	drmModeResPtr mode_res;
	int cpp;

	drmEventContext event_context;
	void *event_data;
	int old_fb_id;
	int flip_count;

	struct list outputs;
	struct list crtcs;
};

struct intel_crtc {
	struct intel_mode *mode;
	drmModeModeInfo kmode;
	drmModeCrtcPtr mode_crtc;
	dri_bo *cursor;
	dri_bo *rotate_bo;
	uint32_t rotate_pitch;
	uint32_t rotate_fb_id;
	xf86CrtcPtr crtc;
	struct list link;
};

struct intel_property {
	drmModePropertyPtr mode_prop;
	uint64_t value;
	int num_atoms; /* if range prop, num_atoms == 1; if enum prop, num_atoms == num_enums + 1 */
	Atom *atoms;
};

struct intel_output {
	struct intel_mode *mode;
	int output_id;
	drmModeConnectorPtr mode_output;
	drmModeEncoderPtr mode_encoder;
	int num_props;
	struct intel_property *props;
	void *private_data;

	Bool has_panel_limits;
	int panel_hdisplay;
	int panel_vdisplay;

	int dpms_mode;
	const char *backlight_iface;
	int backlight_active_level;
	int backlight_max;
	xf86OutputPtr output;
	struct list link;
};

static void
intel_output_dpms(xf86OutputPtr output, int mode);

#define BACKLIGHT_CLASS "/sys/class/backlight"

/*
 * List of available kernel interfaces in priority order
 */
static const char *backlight_interfaces[] = {
	"intel", /* prefer our own native backlight driver */
	"asus-laptop",
	"eeepc",
	"thinkpad_screen",
	"mbp_backlight",
	"fujitsu-laptop",
	"sony",
	"samsung",
	"acpi_video1", /* finally fallback to the generic acpi drivers */
	"acpi_video0",
	NULL,
};
/*
 * Must be long enough for BACKLIGHT_CLASS + '/' + longest in above table +
 * '/' + "max_backlight"
 */
#define BACKLIGHT_PATH_LEN 80
/* Enough for 10 digits of backlight + '\n' + '\0' */
#define BACKLIGHT_VALUE_LEN 12

static inline int
crtc_id(struct intel_crtc *crtc)
{
	return crtc->mode_crtc->crtc_id;
}

static void
intel_output_backlight_set(xf86OutputPtr output, int level)
{
	struct intel_output *intel_output = output->driver_private;
	char path[BACKLIGHT_PATH_LEN], val[BACKLIGHT_VALUE_LEN];
	int fd, len, ret;

	if (level > intel_output->backlight_max)
		level = intel_output->backlight_max;
	if (! intel_output->backlight_iface || level < 0)
		return;

	len = snprintf(val, BACKLIGHT_VALUE_LEN, "%d\n", level);
	sprintf(path, "%s/%s/brightness",
		BACKLIGHT_CLASS, intel_output->backlight_iface);
	fd = open(path, O_RDWR);
	if (fd == -1) {
		xf86DrvMsg(output->scrn->scrnIndex, X_ERROR, "failed to open %s for backlight "
			   "control: %s\n", path, strerror(errno));
		return;
	}

	ret = write(fd, val, len);
	if (ret == -1) {
		xf86DrvMsg(output->scrn->scrnIndex, X_ERROR, "write to %s for backlight "
			   "control failed: %s\n", path, strerror(errno));
	}

	close(fd);
}

static int
intel_output_backlight_get(xf86OutputPtr output)
{
	struct intel_output *intel_output = output->driver_private;
	char path[BACKLIGHT_PATH_LEN], val[BACKLIGHT_VALUE_LEN];
	int fd, level;

	sprintf(path, "%s/%s/actual_brightness",
		BACKLIGHT_CLASS, intel_output->backlight_iface);
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		xf86DrvMsg(output->scrn->scrnIndex, X_ERROR, "failed to open %s "
			   "for backlight control: %s\n", path, strerror(errno));
		return -1;
	}

	memset(val, 0, sizeof(val));
	if (read(fd, val, BACKLIGHT_VALUE_LEN) == -1) {
		close(fd);
		return -1;
	}

	close(fd);

	level = atoi(val);
	if (level > intel_output->backlight_max)
		level = intel_output->backlight_max;
	if (level < 0)
		level = -1;
	return level;
}

static int
intel_output_backlight_get_max(xf86OutputPtr output)
{
	struct intel_output *intel_output = output->driver_private;
	char path[BACKLIGHT_PATH_LEN], val[BACKLIGHT_VALUE_LEN];
	int fd, max = 0;

	sprintf(path, "%s/%s/max_brightness",
		BACKLIGHT_CLASS, intel_output->backlight_iface);
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		xf86DrvMsg(output->scrn->scrnIndex, X_ERROR, "failed to open %s "
			   "for backlight control: %s\n", path, strerror(errno));
		return -1;
	}

	memset(val, 0, sizeof(val));
	if (read(fd, val, BACKLIGHT_VALUE_LEN) == -1) {
		close(fd);
		return -1;
	}

	close(fd);

	max = atoi(val);
	if (max <= 0)
		max = -1;
	return max;
}

static void
intel_output_backlight_init(xf86OutputPtr output)
{
	struct intel_output *intel_output = output->driver_private;
	int i;

	for (i = 0; backlight_interfaces[i] != NULL; i++) {
		char path[BACKLIGHT_PATH_LEN];
		struct stat buf;

		sprintf(path, "%s/%s", BACKLIGHT_CLASS, backlight_interfaces[i]);
		if (!stat(path, &buf)) {
			intel_output->backlight_iface = backlight_interfaces[i];
			intel_output->backlight_max = intel_output_backlight_get_max(output);
			if (intel_output->backlight_max > 0) {
				intel_output->backlight_active_level = intel_output_backlight_get(output);
				xf86DrvMsg(output->scrn->scrnIndex, X_INFO,
					   "found backlight control interface %s\n", path);
				return;
			}
		}
	}
	intel_output->backlight_iface = NULL;
}


static void
mode_from_kmode(ScrnInfoPtr scrn,
		drmModeModeInfoPtr kmode,
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
mode_to_kmode(ScrnInfoPtr scrn,
	      drmModeModeInfoPtr kmode,
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
intel_crtc_dpms(xf86CrtcPtr intel_crtc, int mode)
{

}

static Bool
intel_crtc_apply(xf86CrtcPtr crtc)
{
	ScrnInfoPtr scrn = crtc->scrn;
	struct intel_crtc *intel_crtc = crtc->driver_private;
	struct intel_mode *mode = intel_crtc->mode;
	xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	uint32_t *output_ids;
	int output_count = 0;
	int fb_id, x, y;
	int i, ret = FALSE;

	output_ids = calloc(sizeof(uint32_t), xf86_config->num_output);
	if (!output_ids)
		return FALSE;

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		struct intel_output *intel_output;

		if (output->crtc != crtc)
			continue;

		intel_output = output->driver_private;
		output_ids[output_count] =
			intel_output->mode_output->connector_id;
		output_count++;
	}

#if XORG_VERSION_CURRENT < XORG_VERSION_NUMERIC(1,5,99,0,0)
	if (!xf86CrtcRotate(crtc, mode, rotation))
		goto done;
#else
	if (!xf86CrtcRotate(crtc))
		goto done;
#endif

#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,7,0,0,0)
	crtc->funcs->gamma_set(crtc, crtc->gamma_red, crtc->gamma_green,
			       crtc->gamma_blue, crtc->gamma_size);
#endif


	x = crtc->x;
	y = crtc->y;
	fb_id = mode->fb_id;
	if (intel_crtc->rotate_fb_id) {
		fb_id = intel_crtc->rotate_fb_id;
		x = 0;
		y = 0;
	}
	ret = drmModeSetCrtc(mode->fd, crtc_id(intel_crtc),
			     fb_id, x, y, output_ids, output_count,
			     &intel_crtc->kmode);
	if (ret) {
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "failed to set mode: %s\n", strerror(-ret));
		ret = FALSE;
	} else
		ret = TRUE;

	intel_set_gem_max_sizes(scrn);

	if (scrn->pScreen)
		xf86_reload_cursors(scrn->pScreen);

done:
	free(output_ids);
	return ret;
}

static Bool
intel_crtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
			  Rotation rotation, int x, int y)
{
	ScrnInfoPtr scrn = crtc->scrn;
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct intel_crtc *intel_crtc = crtc->driver_private;
	struct intel_mode *intel_mode = intel_crtc->mode;
	int saved_x, saved_y;
	Rotation saved_rotation;
	DisplayModeRec saved_mode;
	int ret = TRUE;
	unsigned int pitch = scrn->displayWidth * intel->cpp;

	if (intel_mode->fb_id == 0) {
		ret = drmModeAddFB(intel_mode->fd,
				   scrn->virtualX, scrn->virtualY,
				   scrn->depth, scrn->bitsPerPixel,
				   pitch, intel->front_buffer->handle,
				   &intel_mode->fb_id);
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

	mode_to_kmode(crtc->scrn, &intel_crtc->kmode, mode);
	ret = intel_crtc_apply(crtc);
	if (!ret) {
		crtc->x = saved_x;
		crtc->y = saved_y;
		crtc->rotation = saved_rotation;
		crtc->mode = saved_mode;
	}
	return ret;
}

static void
intel_crtc_set_cursor_colors(xf86CrtcPtr crtc, int bg, int fg)
{

}

static void
intel_crtc_set_cursor_position (xf86CrtcPtr crtc, int x, int y)
{
	struct intel_crtc *intel_crtc = crtc->driver_private;
	struct intel_mode *mode = intel_crtc->mode;

	drmModeMoveCursor(mode->fd, crtc_id(intel_crtc), x, y);
}

static void
intel_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *image)
{
	struct intel_crtc *intel_crtc = crtc->driver_private;
	int ret;

	ret = dri_bo_subdata(intel_crtc->cursor, 0, 64*64*4, image);
	if (ret)
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "failed to set cursor: %s\n", strerror(-ret));
}

static void
intel_crtc_hide_cursor(xf86CrtcPtr crtc)
{
	struct intel_crtc *intel_crtc = crtc->driver_private;
	struct intel_mode *mode = intel_crtc->mode;

	drmModeSetCursor(mode->fd, crtc_id(intel_crtc), 0, 64, 64);
}

static void
intel_crtc_show_cursor(xf86CrtcPtr crtc)
{
	struct intel_crtc *intel_crtc = crtc->driver_private;
	struct intel_mode *mode = intel_crtc->mode;

	drmModeSetCursor(mode->fd, crtc_id(intel_crtc),
			 intel_crtc->cursor->handle, 64, 64);
}

static void *
intel_crtc_shadow_allocate(xf86CrtcPtr crtc, int width, int height)
{
	ScrnInfoPtr scrn = crtc->scrn;
	struct intel_crtc *intel_crtc = crtc->driver_private;
	struct intel_mode *mode = intel_crtc->mode;
	unsigned long rotate_pitch;
	int ret;

	intel_crtc->rotate_bo = intel_allocate_framebuffer(scrn,
							     width, height,
							     mode->cpp,
							     &rotate_pitch);

	if (!intel_crtc->rotate_bo) {
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "Couldn't allocate shadow memory for rotated CRTC\n");
		return NULL;
	}

	ret = drmModeAddFB(mode->fd, width, height, crtc->scrn->depth,
			   crtc->scrn->bitsPerPixel, rotate_pitch,
			   intel_crtc->rotate_bo->handle,
			   &intel_crtc->rotate_fb_id);
	if (ret) {
		ErrorF("failed to add rotate fb\n");
		drm_intel_bo_unreference(intel_crtc->rotate_bo);
		return NULL;
	}

	intel_crtc->rotate_pitch = rotate_pitch;
	return intel_crtc->rotate_bo;
}

static PixmapPtr
intel_crtc_shadow_create(xf86CrtcPtr crtc, void *data, int width, int height)
{
	ScrnInfoPtr scrn = crtc->scrn;
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct intel_crtc *intel_crtc = crtc->driver_private;
	PixmapPtr rotate_pixmap;

	if (!data) {
		data = intel_crtc_shadow_allocate (crtc, width, height);
		if (!data) {
			xf86DrvMsg(scrn->scrnIndex, X_ERROR,
				   "Couldn't allocate shadow pixmap for rotated CRTC\n");
			return NULL;
		}
	}
	if (intel_crtc->rotate_bo == NULL) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Couldn't allocate shadow pixmap for rotated CRTC\n");
		return NULL;
	}

	rotate_pixmap = GetScratchPixmapHeader(scrn->pScreen,
					       width, height,
					       scrn->depth,
					       scrn->bitsPerPixel,
					       intel_crtc->rotate_pitch,
					       NULL);

	if (rotate_pixmap == NULL) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Couldn't allocate shadow pixmap for rotated CRTC\n");
		return NULL;
	}

	intel_set_pixmap_bo(rotate_pixmap, intel_crtc->rotate_bo);

	intel->shadow_present = TRUE;

	return rotate_pixmap;
}

static void
intel_crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr rotate_pixmap, void *data)
{
	ScrnInfoPtr scrn = crtc->scrn;
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct intel_crtc *intel_crtc = crtc->driver_private;
	struct intel_mode *mode = intel_crtc->mode;

	if (rotate_pixmap) {
		intel_set_pixmap_bo(rotate_pixmap, NULL);
		FreeScratchPixmapHeader(rotate_pixmap);
	}

	if (data) {
		/* Be sure to sync acceleration before the memory gets
		 * unbound. */
		drmModeRmFB(mode->fd, intel_crtc->rotate_fb_id);
		intel_crtc->rotate_fb_id = 0;

		dri_bo_unreference(intel_crtc->rotate_bo);
		intel_crtc->rotate_bo = NULL;
	}

	intel->shadow_present = FALSE;
}

static void
intel_crtc_gamma_set(xf86CrtcPtr crtc,
		       CARD16 *red, CARD16 *green, CARD16 *blue, int size)
{
	struct intel_crtc *intel_crtc = crtc->driver_private;
	struct intel_mode *mode = intel_crtc->mode;

	drmModeCrtcSetGamma(mode->fd, crtc_id(intel_crtc),
			    size, red, green, blue);
}

static void
intel_crtc_destroy(xf86CrtcPtr crtc)
{
	struct intel_crtc *intel_crtc = crtc->driver_private;

	if (intel_crtc->cursor) {
		drmModeSetCursor(intel_crtc->mode->fd, crtc_id(intel_crtc), 0, 64, 64);
		drm_intel_bo_unreference(intel_crtc->cursor);
		intel_crtc->cursor = NULL;
	}

	list_del(&intel_crtc->link);
	free(intel_crtc);

	crtc->driver_private = NULL;
}

static const xf86CrtcFuncsRec intel_crtc_funcs = {
	.dpms = intel_crtc_dpms,
	.set_mode_major = intel_crtc_set_mode_major,
	.set_cursor_colors = intel_crtc_set_cursor_colors,
	.set_cursor_position = intel_crtc_set_cursor_position,
	.show_cursor = intel_crtc_show_cursor,
	.hide_cursor = intel_crtc_hide_cursor,
	.load_cursor_argb = intel_crtc_load_cursor_argb,
	.shadow_create = intel_crtc_shadow_create,
	.shadow_allocate = intel_crtc_shadow_allocate,
	.shadow_destroy = intel_crtc_shadow_destroy,
	.gamma_set = intel_crtc_gamma_set,
	.destroy = intel_crtc_destroy,
};

static void
intel_crtc_init(ScrnInfoPtr scrn, struct intel_mode *mode, int num)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	xf86CrtcPtr crtc;
	struct intel_crtc *intel_crtc;

	intel_crtc = calloc(sizeof(struct intel_crtc), 1);
	if (intel_crtc == NULL)
		return;

	crtc = xf86CrtcCreate(scrn, &intel_crtc_funcs);
	if (crtc == NULL) {
		free(intel_crtc);
		return;
	}

	intel_crtc->mode_crtc = drmModeGetCrtc(mode->fd,
					       mode->mode_res->crtcs[num]);
	intel_crtc->mode = mode;
	crtc->driver_private = intel_crtc;

	intel_crtc->cursor = drm_intel_bo_alloc(intel->bufmgr, "ARGB cursor",
						HWCURSOR_SIZE_ARGB,
						GTT_PAGE_SIZE);

	intel_crtc->crtc = crtc;
	list_add(&intel_crtc->link, &mode->crtcs);
}

static xf86OutputStatus
intel_output_detect(xf86OutputPtr output)
{
	/* go to the hw and retrieve a new output struct */
	struct intel_output *intel_output = output->driver_private;
	struct intel_mode *mode = intel_output->mode;
	xf86OutputStatus status;

	drmModeFreeConnector(intel_output->mode_output);
	intel_output->mode_output =
		drmModeGetConnector(mode->fd, intel_output->output_id);

	switch (intel_output->mode_output->connection) {
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
intel_output_mode_valid(xf86OutputPtr output, DisplayModePtr pModes)
{
	struct intel_output *intel_output = output->driver_private;

	/*
	 * If the connector type is a panel, we will use the panel limit to
	 * verfiy whether the mode is valid.
	 */
	if (intel_output->has_panel_limits) {
		if (pModes->HDisplay > intel_output->panel_hdisplay ||
		    pModes->VDisplay > intel_output->panel_vdisplay)
			return MODE_PANEL;
	}

	return MODE_OK;
}

static void
intel_output_attach_edid(xf86OutputPtr output)
{
	struct intel_output *intel_output = output->driver_private;
	drmModeConnectorPtr koutput = intel_output->mode_output;
	struct intel_mode *mode = intel_output->mode;
	drmModePropertyBlobPtr edid_blob = NULL;
	xf86MonPtr mon = NULL;
	int i;

	/* look for an EDID property */
	for (i = 0; i < koutput->count_props; i++) {
		drmModePropertyPtr props;

		props = drmModeGetProperty(mode->fd, koutput->props[i]);
		if (!props)
			continue;

		if (!(props->flags & DRM_MODE_PROP_BLOB)) {
			drmModeFreeProperty(props);
			continue;
		}

		if (!strcmp(props->name, "EDID")) {
			drmModeFreePropertyBlob(edid_blob);
			edid_blob =
				drmModeGetPropertyBlob(mode->fd,
						       koutput->prop_values[i]);
		}
		drmModeFreeProperty(props);
	}

	if (edid_blob) {
		mon = xf86InterpretEDID(output->scrn->scrnIndex,
					edid_blob->data);

		if (mon && edid_blob->length > 128)
			mon->flags |= MONITOR_EDID_COMPLETE_RAWDATA;

		drmModeFreePropertyBlob(edid_blob);
	}

	xf86OutputSetEDID(output, mon);
}

static DisplayModePtr
intel_output_panel_edid(xf86OutputPtr output, DisplayModePtr modes)
{
	xf86MonPtr mon = output->MonInfo;

	if (!mon || !GTF_SUPPORTED(mon->features.msc)) {
		DisplayModePtr i, m, p = NULL;
		int max_x = 0, max_y = 0;
		float max_vrefresh = 0.0;

		for (m = modes; m; m = m->next) {
			if (m->type & M_T_PREFERRED)
				p = m;
			max_x = max(max_x, m->HDisplay);
			max_y = max(max_y, m->VDisplay);
			max_vrefresh = max(max_vrefresh, xf86ModeVRefresh(m));
		}

		max_vrefresh = max(max_vrefresh, 60.0);
		max_vrefresh *= (1 + SYNC_TOLERANCE);

#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,6,99,0,0)
		m = xf86GetDefaultModes();
#else
		m = xf86GetDefaultModes(0,0);
#endif

		xf86ValidateModesSize(output->scrn, m, max_x, max_y, 0);

		for (i = m; i; i = i->next) {
			if (xf86ModeVRefresh(i) > max_vrefresh)
				i->status = MODE_VSYNC;
			if (p && i->HDisplay >= p->HDisplay &&
			    i->VDisplay >= p->VDisplay &&
			    xf86ModeVRefresh(i) >= xf86ModeVRefresh(p))
				i->status = MODE_VSYNC;
		}

		xf86PruneInvalidModes(output->scrn, &m, FALSE);

		modes = xf86ModesAdd(modes, m);
	}

	return modes;
}

static DisplayModePtr
intel_output_get_modes(xf86OutputPtr output)
{
	struct intel_output *intel_output = output->driver_private;
	drmModeConnectorPtr koutput = intel_output->mode_output;
	DisplayModePtr Modes = NULL;
	int i;

	intel_output_attach_edid(output);

	/* modes should already be available */
	for (i = 0; i < koutput->count_modes; i++) {
		DisplayModePtr Mode;

		Mode = calloc(1, sizeof(DisplayModeRec));
		if (Mode) {
			mode_from_kmode(output->scrn, &koutput->modes[i], Mode);
			Modes = xf86ModesAdd(Modes, Mode);
		}
	}

	/*
	 * If the connector type is a panel, we will traverse the kernel mode to
	 * get the panel limit. And then add all the standard modes to fake
	 * the fullscreen experience.
	 * If it is incorrect, please fix me.
	 */
	intel_output->has_panel_limits = FALSE;
	if (koutput->connector_type == DRM_MODE_CONNECTOR_LVDS ||
	    koutput->connector_type == DRM_MODE_CONNECTOR_eDP) {
		for (i = 0; i < koutput->count_modes; i++) {
			drmModeModeInfo *mode_ptr;

			mode_ptr = &koutput->modes[i];
			if (mode_ptr->hdisplay > intel_output->panel_hdisplay)
				intel_output->panel_hdisplay = mode_ptr->hdisplay;
			if (mode_ptr->vdisplay > intel_output->panel_vdisplay)
				intel_output->panel_vdisplay = mode_ptr->vdisplay;
		}

		intel_output->has_panel_limits =
			intel_output->panel_hdisplay &&
			intel_output->panel_vdisplay;

		Modes = intel_output_panel_edid(output, Modes);
	}

	return Modes;
}

static void
intel_output_destroy(xf86OutputPtr output)
{
	struct intel_output *intel_output = output->driver_private;
	int i;

	for (i = 0; i < intel_output->num_props; i++) {
		drmModeFreeProperty(intel_output->props[i].mode_prop);
		free(intel_output->props[i].atoms);
	}
	free(intel_output->props);

	drmModeFreeConnector(intel_output->mode_output);
	intel_output->mode_output = NULL;

	list_del(&intel_output->link);
	free(intel_output);

	output->driver_private = NULL;
}

static void
intel_output_dpms_backlight(xf86OutputPtr output, int oldmode, int mode)
{
	struct intel_output *intel_output = output->driver_private;

	if (!intel_output->backlight_iface)
		return;

	if (mode == DPMSModeOn) {
		/* If we're going from off->on we may need to turn on the backlight. */
		if (oldmode != DPMSModeOn)
			intel_output_backlight_set(output,
						   intel_output->backlight_active_level);
	} else {
		/* Only save the current backlight value if we're going from on to off. */
		if (oldmode == DPMSModeOn)
			intel_output->backlight_active_level = intel_output_backlight_get(output);
		intel_output_backlight_set(output, 0);
	}
}

static void
intel_output_dpms(xf86OutputPtr output, int dpms)
{
	struct intel_output *intel_output = output->driver_private;
	drmModeConnectorPtr koutput = intel_output->mode_output;
	struct intel_mode *mode = intel_output->mode;
	int i;

	for (i = 0; i < koutput->count_props; i++) {
		drmModePropertyPtr props;

		props = drmModeGetProperty(mode->fd, koutput->props[i]);
		if (!props)
			continue;

		if (!strcmp(props->name, "DPMS")) {
			drmModeConnectorSetProperty(mode->fd,
						    intel_output->output_id,
						    props->prop_id,
						    dpms);
			intel_output_dpms_backlight(output,
						      intel_output->dpms_mode,
						      dpms);
			intel_output->dpms_mode = dpms;
			drmModeFreeProperty(props);
			return;
		}

		drmModeFreeProperty(props);
	}
}

int
intel_output_dpms_status(xf86OutputPtr output)
{
	struct intel_output *intel_output = output->driver_private;
	return intel_output->dpms_mode;
}

static Bool
intel_property_ignore(drmModePropertyPtr prop)
{
	if (!prop)
		return TRUE;

	/* ignore blob prop */
	if (prop->flags & DRM_MODE_PROP_BLOB)
		return TRUE;

	/* ignore standard property */
	if (!strcmp(prop->name, "EDID") ||
	    !strcmp(prop->name, "DPMS"))
		return TRUE;

	return FALSE;
}

#define BACKLIGHT_NAME             "Backlight"
#define BACKLIGHT_DEPRECATED_NAME  "BACKLIGHT"
static Atom backlight_atom, backlight_deprecated_atom;

static void
intel_output_create_resources(xf86OutputPtr output)
{
	struct intel_output *intel_output = output->driver_private;
	drmModeConnectorPtr mode_output = intel_output->mode_output;
	struct intel_mode *mode = intel_output->mode;
	int i, j, err;

	intel_output->props = calloc(mode_output->count_props,
				     sizeof(struct intel_property));
	if (!intel_output->props)
		return;

	intel_output->num_props = 0;
	for (i = j = 0; i < mode_output->count_props; i++) {
		drmModePropertyPtr drmmode_prop;

		drmmode_prop = drmModeGetProperty(mode->fd,
						  mode_output->props[i]);
		if (intel_property_ignore(drmmode_prop)) {
			drmModeFreeProperty(drmmode_prop);
			continue;
		}

		intel_output->props[j].mode_prop = drmmode_prop;
		intel_output->props[j].value = mode_output->prop_values[i];
		j++;
	}
	intel_output->num_props = j;

	for (i = 0; i < intel_output->num_props; i++) {
		struct intel_property *p = &intel_output->props[i];
		drmModePropertyPtr drmmode_prop = p->mode_prop;

		if (drmmode_prop->flags & DRM_MODE_PROP_RANGE) {
			INT32 range[2];

			p->num_atoms = 1;
			p->atoms = calloc(p->num_atoms, sizeof(Atom));
			if (!p->atoms)
				continue;

			p->atoms[0] = MakeAtom(drmmode_prop->name, strlen(drmmode_prop->name), TRUE);
			range[0] = drmmode_prop->values[0];
			range[1] = drmmode_prop->values[1];
			err = RRConfigureOutputProperty(output->randr_output, p->atoms[0],
							FALSE, TRUE,
							drmmode_prop->flags & DRM_MODE_PROP_IMMUTABLE ? TRUE : FALSE,
							2, range);
			if (err != 0) {
				xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
					   "RRConfigureOutputProperty error, %d\n", err);
			}
			err = RRChangeOutputProperty(output->randr_output, p->atoms[0],
						     XA_INTEGER, 32, PropModeReplace, 1, &p->value, FALSE, TRUE);
			if (err != 0) {
				xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
					   "RRChangeOutputProperty error, %d\n", err);
			}
		} else if (drmmode_prop->flags & DRM_MODE_PROP_ENUM) {
			p->num_atoms = drmmode_prop->count_enums + 1;
			p->atoms = calloc(p->num_atoms, sizeof(Atom));
			if (!p->atoms)
				continue;

			p->atoms[0] = MakeAtom(drmmode_prop->name, strlen(drmmode_prop->name), TRUE);
			for (j = 1; j <= drmmode_prop->count_enums; j++) {
				struct drm_mode_property_enum *e = &drmmode_prop->enums[j-1];
				p->atoms[j] = MakeAtom(e->name, strlen(e->name), TRUE);
			}

			err = RRConfigureOutputProperty(output->randr_output, p->atoms[0],
							FALSE, FALSE,
							drmmode_prop->flags & DRM_MODE_PROP_IMMUTABLE ? TRUE : FALSE,
							p->num_atoms - 1, (INT32 *)&p->atoms[1]);
			if (err != 0) {
				xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
					   "RRConfigureOutputProperty error, %d\n", err);
			}

			for (j = 0; j < drmmode_prop->count_enums; j++)
				if (drmmode_prop->enums[j].value == p->value)
					break;
			/* there's always a matching value */
			err = RRChangeOutputProperty(output->randr_output, p->atoms[0],
						     XA_ATOM, 32, PropModeReplace, 1, &p->atoms[j+1], FALSE, TRUE);
			if (err != 0) {
				xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
					   "RRChangeOutputProperty error, %d\n", err);
			}
		}
	}

	if (intel_output->backlight_iface) {
		INT32 data, backlight_range[2];

		/* Set up the backlight property, which takes effect
		 * immediately and accepts values only within the
		 * backlight_range.
		 */
		backlight_atom = MakeAtom(BACKLIGHT_NAME, sizeof(BACKLIGHT_NAME) - 1, TRUE);
		backlight_deprecated_atom = MakeAtom(BACKLIGHT_DEPRECATED_NAME,
						     sizeof(BACKLIGHT_DEPRECATED_NAME) - 1, TRUE);

		backlight_range[0] = 0;
		backlight_range[1] = intel_output->backlight_max;
		err = RRConfigureOutputProperty(output->randr_output,
					       	backlight_atom,
						FALSE, TRUE, FALSE,
					       	2, backlight_range);
		if (err != 0) {
			xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
				   "RRConfigureOutputProperty error, %d\n", err);
		}
		err = RRConfigureOutputProperty(output->randr_output,
					       	backlight_deprecated_atom,
						FALSE, TRUE, FALSE,
					       	2, backlight_range);
		if (err != 0) {
			xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
				   "RRConfigureOutputProperty error, %d\n", err);
		}
		/* Set the current value of the backlight property */
		data = intel_output->backlight_active_level;
		err = RRChangeOutputProperty(output->randr_output, backlight_atom,
					     XA_INTEGER, 32, PropModeReplace, 1, &data,
					     FALSE, TRUE);
		if (err != 0) {
			xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
				   "RRChangeOutputProperty error, %d\n", err);
		}
		err = RRChangeOutputProperty(output->randr_output, backlight_deprecated_atom,
					     XA_INTEGER, 32, PropModeReplace, 1, &data,
					     FALSE, TRUE);
		if (err != 0) {
			xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
				   "RRChangeOutputProperty error, %d\n", err);
		}
	}
}

static Bool
intel_output_set_property(xf86OutputPtr output, Atom property,
			    RRPropertyValuePtr value)
{
	struct intel_output *intel_output = output->driver_private;
	struct intel_mode *mode = intel_output->mode;
	int i;

	if (property == backlight_atom || property == backlight_deprecated_atom) {
		INT32 val;

		if (value->type != XA_INTEGER || value->format != 32 ||
		    value->size != 1)
		{
			return FALSE;
		}

		val = *(INT32 *)value->data;
		if (val < 0 || val > intel_output->backlight_max)
			return FALSE;

		if (intel_output->dpms_mode == DPMSModeOn)
			intel_output_backlight_set(output, val);
		intel_output->backlight_active_level = val;
		return TRUE;
	}

	for (i = 0; i < intel_output->num_props; i++) {
		struct intel_property *p = &intel_output->props[i];

		if (p->atoms[0] != property)
			continue;

		if (p->mode_prop->flags & DRM_MODE_PROP_RANGE) {
			uint32_t val;

			if (value->type != XA_INTEGER || value->format != 32 ||
			    value->size != 1)
				return FALSE;
			val = *(uint32_t *)value->data;

			drmModeConnectorSetProperty(mode->fd, intel_output->output_id,
						    p->mode_prop->prop_id, (uint64_t)val);
			return TRUE;
		} else if (p->mode_prop->flags & DRM_MODE_PROP_ENUM) {
			Atom	atom;
			const char	*name;
			int		j;

			if (value->type != XA_ATOM || value->format != 32 || value->size != 1)
				return FALSE;
			memcpy(&atom, value->data, 4);
			name = NameForAtom(atom);

			/* search for matching name string, then set its value down */
			for (j = 0; j < p->mode_prop->count_enums; j++) {
				if (!strcmp(p->mode_prop->enums[j].name, name)) {
					drmModeConnectorSetProperty(mode->fd, intel_output->output_id,
								    p->mode_prop->prop_id, p->mode_prop->enums[j].value);
					return TRUE;
				}
			}
			return FALSE;
		}
	}

	return FALSE;
}

static Bool
intel_output_get_property(xf86OutputPtr output, Atom property)
{
	struct intel_output *intel_output = output->driver_private;
	int err;

	if (property == backlight_atom || property == backlight_deprecated_atom) {
		INT32 val;

		if (! intel_output->backlight_iface)
			return FALSE;

		val = intel_output_backlight_get(output);
		if (val < 0)
			return FALSE;

		err = RRChangeOutputProperty(output->randr_output, property,
					     XA_INTEGER, 32, PropModeReplace, 1, &val,
					     FALSE, TRUE);
		if (err != 0) {
			xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
				   "RRChangeOutputProperty error, %d\n", err);
			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

static const xf86OutputFuncsRec intel_output_funcs = {
	.create_resources = intel_output_create_resources,
#ifdef RANDR_12_INTERFACE
	.set_property = intel_output_set_property,
	.get_property = intel_output_get_property,
#endif
	.dpms = intel_output_dpms,
#if 0

	.save = drmmode_crt_save,
	.restore = drmmode_crt_restore,
	.mode_fixup = drmmode_crt_mode_fixup,
	.prepare = intel_output_prepare,
	.mode_set = drmmode_crt_mode_set,
	.commit = intel_output_commit,
#endif
	.detect = intel_output_detect,
	.mode_valid = intel_output_mode_valid,

	.get_modes = intel_output_get_modes,
	.destroy = intel_output_destroy
};

static const int subpixel_conv_table[7] = {
       	0,
       	SubPixelUnknown,
	SubPixelHorizontalRGB,
	SubPixelHorizontalBGR,
	SubPixelVerticalRGB,
	SubPixelVerticalBGR,
	SubPixelNone
};

static const char *output_names[] = {
       	"None",
	"VGA",
	"DVI",
	"DVI",
	"DVI",
	"Composite",
	"TV",
	"LVDS",
	"CTV",
	"DIN",
	"DP",
	"HDMI",
	"HDMI",
	"TV",
	"eDP",
};

static void
intel_output_init(ScrnInfoPtr scrn, struct intel_mode *mode, int num)
{
	xf86OutputPtr output;
	drmModeConnectorPtr koutput;
	drmModeEncoderPtr kencoder;
	struct intel_output *intel_output;
	const char *output_name;
	char name[32];

	koutput = drmModeGetConnector(mode->fd,
				      mode->mode_res->connectors[num]);
	if (!koutput)
		return;

	kencoder = drmModeGetEncoder(mode->fd, koutput->encoders[0]);
	if (!kencoder) {
		drmModeFreeConnector(koutput);
		return;
	}

	if (koutput->connector_type < ARRAY_SIZE(output_names))
		output_name = output_names[koutput->connector_type];
	else
		output_name = "UNKNOWN";
	snprintf(name, 32, "%s%d", output_name, koutput->connector_type_id);

	output = xf86OutputCreate (scrn, &intel_output_funcs, name);
	if (!output) {
		drmModeFreeEncoder(kencoder);
		drmModeFreeConnector(koutput);
		return;
	}

	intel_output = calloc(sizeof(struct intel_output), 1);
	if (!intel_output) {
		xf86OutputDestroy(output);
		drmModeFreeConnector(koutput);
		drmModeFreeEncoder(kencoder);
		return;
	}

	intel_output->output_id = mode->mode_res->connectors[num];
	intel_output->mode_output = koutput;
	intel_output->mode_encoder = kencoder;
	intel_output->mode = mode;

	output->mm_width = koutput->mmWidth;
	output->mm_height = koutput->mmHeight;

	output->subpixel_order = subpixel_conv_table[koutput->subpixel];
	output->driver_private = intel_output;

	if (koutput->connector_type == DRM_MODE_CONNECTOR_LVDS)
		intel_output_backlight_init(output);

	output->possible_crtcs = kencoder->possible_crtcs;
	output->possible_clones = kencoder->possible_clones;

	intel_output->output = output;
	list_add(&intel_output->link, &mode->outputs);
}

static Bool
intel_xf86crtc_resize(ScrnInfoPtr scrn, int width, int height)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	struct intel_crtc *intel_crtc = xf86_config->crtc[0]->driver_private;
	struct intel_mode *mode = intel_crtc->mode;
	intel_screen_private *intel = intel_get_screen_private(scrn);
	drm_intel_bo *old_front = NULL;
	Bool	    ret;
	ScreenPtr   screen = screenInfo.screens[scrn->scrnIndex];
	PixmapPtr   pixmap;
	uint32_t    old_fb_id;
	int	    i, old_width, old_height, old_pitch;
	unsigned long pitch;

	if (scrn->virtualX == width && scrn->virtualY == height)
		return TRUE;

	old_width = scrn->virtualX;
	old_height = scrn->virtualY;
	old_pitch = scrn->displayWidth;
	old_fb_id = mode->fb_id;
	old_front = intel->front_buffer;

	intel->front_buffer = intel_allocate_framebuffer(scrn,
							 width, height,
							 intel->cpp,
							 &pitch);
	if (!intel->front_buffer)
		goto fail;

	ret = drmModeAddFB(mode->fd, width, height, scrn->depth,
			   scrn->bitsPerPixel, pitch,
			   intel->front_buffer->handle,
			   &mode->fb_id);
	if (ret)
		goto fail;

	scrn->virtualX = width;
	scrn->virtualY = height;
	scrn->displayWidth = pitch / intel->cpp;

	pixmap = screen->GetScreenPixmap(screen);
	screen->ModifyPixmapHeader(pixmap, width, height, -1, -1, pitch, NULL);
	intel_set_pixmap_bo(pixmap, intel->front_buffer);
	intel_get_pixmap_private(pixmap)->busy = 1;

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		if (!crtc->enabled)
			continue;

		if (!intel_crtc_apply(crtc))
			goto fail;
	}

	if (old_fb_id)
		drmModeRmFB(mode->fd, old_fb_id);
	if (old_front)
		drm_intel_bo_unreference(old_front);

	return TRUE;

fail:
	if (intel->front_buffer)
		drm_intel_bo_unreference(intel->front_buffer);
	intel->front_buffer = old_front;
	scrn->virtualX = old_width;
	scrn->virtualY = old_height;
	scrn->displayWidth = old_pitch;
	if (old_fb_id != mode->fb_id)
		drmModeRmFB(mode->fd, mode->fb_id);
	mode->fb_id = old_fb_id;

	return FALSE;
}

Bool
intel_do_pageflip(ScreenPtr screen, dri_bo *new_front, void *data)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(scrn);
	struct intel_crtc *crtc = config->crtc[0]->driver_private;
	struct intel_mode *mode = crtc->mode;
	unsigned int pitch = scrn->displayWidth * intel->cpp;
	int i, old_fb_id;

	/*
	 * Create a new handle for the back buffer
	 */
	old_fb_id = mode->fb_id;
	if (drmModeAddFB(mode->fd, scrn->virtualX, scrn->virtualY,
			 scrn->depth, scrn->bitsPerPixel, pitch,
			 new_front->handle, &mode->fb_id))
		goto error_out;

	/*
	 * Queue flips on all enabled CRTCs
	 * Note that if/when we get per-CRTC buffers, we'll have to update this.
	 * Right now it assumes a single shared fb across all CRTCs, with the
	 * kernel fixing up the offset of each CRTC as necessary.
	 *
	 * Also, flips queued on disabled or incorrectly configured displays
	 * may never complete; this is a configuration error.
	 */
	for (i = 0; i < config->num_crtc; i++) {
		if (!config->crtc[i]->enabled)
			continue;

		mode->event_data = data;
		mode->flip_count++;
		if (drmModePageFlip(mode->fd,
				    crtc_id(config->crtc[i]->driver_private),
				    mode->fb_id,
				    DRM_MODE_PAGE_FLIP_EVENT, mode)) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "flip queue failed: %s\n", strerror(errno));
			goto error_undo;
		}
	}

	mode->old_fb_id = old_fb_id;
	return TRUE;

error_undo:
	drmModeRmFB(mode->fd, mode->fb_id);
	mode->fb_id = old_fb_id;

error_out:
	xf86DrvMsg(scrn->scrnIndex, X_WARNING, "Page flip failed: %s\n",
		   strerror(errno));
	return FALSE;
}

static const xf86CrtcConfigFuncsRec intel_xf86crtc_config_funcs = {
	intel_xf86crtc_resize
};

static void
intel_vblank_handler(int fd, unsigned int frame, unsigned int tv_sec,
		       unsigned int tv_usec, void *event_data)
{
	I830DRI2FrameEventHandler(frame, tv_sec, tv_usec, event_data);
}

static void
intel_page_flip_handler(int fd, unsigned int frame, unsigned int tv_sec,
			  unsigned int tv_usec, void *event_data)
{
	struct intel_mode *mode = event_data;

	mode->flip_count--;
	if (mode->flip_count > 0)
		return;

	drmModeRmFB(mode->fd, mode->old_fb_id);

	I830DRI2FlipEventHandler(frame, tv_sec, tv_usec, mode->event_data);
}

static void
drm_wakeup_handler(pointer data, int err, pointer p)
{
	struct intel_mode *mode = data;
	fd_set *read_mask = p;

	if (err >= 0 && FD_ISSET(mode->fd, read_mask))
		drmHandleEvent(mode->fd, &mode->event_context);
}

Bool intel_mode_pre_init(ScrnInfoPtr scrn, int fd, int cpp)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct drm_i915_getparam gp;
	struct intel_mode *mode;
	unsigned int i;
	int has_flipping;

	mode = calloc(1, sizeof *mode);
	if (!mode)
		return FALSE;

	mode->fd = fd;

	list_init(&mode->crtcs);
	list_init(&mode->outputs);

	xf86CrtcConfigInit(scrn, &intel_xf86crtc_config_funcs);

	mode->cpp = cpp;
	mode->mode_res = drmModeGetResources(mode->fd);
	if (!mode->mode_res) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "failed to get resources: %s\n", strerror(errno));
		free(mode);
		return FALSE;
	}

	xf86CrtcSetSizeRange(scrn, 320, 200, mode->mode_res->max_width,
			     mode->mode_res->max_height);
	for (i = 0; i < mode->mode_res->count_crtcs; i++)
		intel_crtc_init(scrn, mode, i);

	for (i = 0; i < mode->mode_res->count_connectors; i++)
		intel_output_init(scrn, mode, i);

	xf86InitialConfiguration(scrn, TRUE);

	has_flipping = 0;
	gp.param = I915_PARAM_HAS_PAGEFLIPPING;
	gp.value = &has_flipping;
	(void)drmCommandWriteRead(intel->drmSubFD, DRM_I915_GETPARAM, &gp,
				  sizeof(gp));
	if (has_flipping) {
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "Kernel page flipping support detected, enabling\n");
		intel->use_pageflipping = TRUE;

		mode->event_context.version = DRM_EVENT_CONTEXT_VERSION;
		mode->event_context.vblank_handler = intel_vblank_handler;
		mode->event_context.page_flip_handler = intel_page_flip_handler;
	}

	intel->modes = mode;
	return TRUE;
}

void
intel_mode_init(struct intel_screen_private *intel)
{
	if (intel->use_pageflipping) {
		struct intel_mode *mode = intel->modes;

		/* We need to re-register the mode->fd for the synchronisation
		 * feedback on every server generation, so perform the
		 * registration within ScreenInit and not PreInit.
		 */
		mode->flip_count = 0;
		AddGeneralSocket(mode->fd);
		RegisterBlockAndWakeupHandlers((BlockHandlerProcPtr)NoopDDA,
					       drm_wakeup_handler, mode);
	}
}

void
intel_mode_remove_fb(intel_screen_private *intel)
{
	struct intel_mode *mode = intel->modes;

	if (mode->fb_id) {
		drmModeRmFB(mode->fd, mode->fb_id);
		mode->fb_id = 0;
	}
}

void
intel_mode_fini(intel_screen_private *intel)
{
	struct intel_mode *mode = intel->modes;

	while(!list_is_empty(&mode->crtcs)) {
		xf86CrtcDestroy(list_first_entry(&mode->crtcs,
						 struct intel_crtc,
						 link)->crtc);
	}

	while(!list_is_empty(&mode->outputs)) {
		xf86OutputDestroy(list_first_entry(&mode->outputs,
						   struct intel_output,
						   link)->output);
	}

	if (mode->fb_id)
		drmModeRmFB(mode->fd, mode->fb_id);

	/* mode->rotate_fb_id should have been destroyed already */

	free(mode);
	intel->modes = NULL;
}

int
intel_get_pipe_from_crtc_id(drm_intel_bufmgr *bufmgr, xf86CrtcPtr crtc)
{
	return drm_intel_get_pipe_from_crtc_id(bufmgr,
					      	crtc_id(crtc->driver_private));
}

/* for the mode overlay */
int
intel_crtc_id(xf86CrtcPtr crtc)
{
	return crtc_id(crtc->driver_private);
}
