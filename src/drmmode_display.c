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

#include "i830.h"
#include "intel_bufmgr.h"
#include "xf86drmMode.h"
#include "X11/Xatom.h"

typedef struct {
    int fd;
    uint32_t fb_id;
    drmModeResPtr mode_res;
    int cpp;
    
    drmEventContext event_context;
    void *event_data;
    int old_fb_id;
    int flip_count;
} drmmode_rec, *drmmode_ptr;

typedef struct {
    drmmode_ptr drmmode;
    drmModeCrtcPtr mode_crtc;
    dri_bo *cursor;
    dri_bo *rotate_bo;
    uint32_t rotate_fb_id;
} drmmode_crtc_private_rec, *drmmode_crtc_private_ptr;

typedef struct {
    drmModePropertyPtr mode_prop;
    uint64_t value;
    int num_atoms; /* if range prop, num_atoms == 1; if enum prop, num_atoms == num_enums + 1 */
    Atom *atoms;
} drmmode_prop_rec, *drmmode_prop_ptr;

struct fixed_panel_lvds {
	int hdisplay;
	int vdisplay;
};
typedef struct {
    drmmode_ptr drmmode;
    int output_id;
    drmModeConnectorPtr mode_output;
    drmModeEncoderPtr mode_encoder;
    drmModePropertyBlobPtr edid_blob;
    int num_props;
    drmmode_prop_ptr props;
    void *private_data;
    int dpms_mode;
    char *backlight_iface;
    int backlight_active_level;
    int backlight_max;
} drmmode_output_private_rec, *drmmode_output_private_ptr;

static void
drmmode_output_dpms(xf86OutputPtr output, int mode);

#define BACKLIGHT_CLASS "/sys/class/backlight"

/*
 * List of available kernel interfaces in priority order
 */
static char *backlight_interfaces[] = {
    "asus-laptop",
    "eeepc",
    "thinkpad_screen",
    "acpi_video1",
    "acpi_video0",
    "fujitsu-laptop",
    "sony",
    "samsung",
    NULL,
};
/*
 * Must be long enough for BACKLIGHT_CLASS + '/' + longest in above table +
 * '/' + "max_backlight"
 */
#define BACKLIGHT_PATH_LEN 80
/* Enough for 10 digits of backlight + '\n' + '\0' */
#define BACKLIGHT_VALUE_LEN 12

static void
drmmode_backlight_set(xf86OutputPtr output, int level)
{
    drmmode_output_private_ptr drmmode_output = output->driver_private;
    char path[BACKLIGHT_PATH_LEN], val[BACKLIGHT_VALUE_LEN];
    int fd, len, ret;

    if (level > drmmode_output->backlight_max)
	level = drmmode_output->backlight_max;
    if (! drmmode_output->backlight_iface || level < 0)
	return;

    len = snprintf(val, BACKLIGHT_VALUE_LEN, "%d\n", level);
    sprintf(path, "%s/%s/brightness",
	    BACKLIGHT_CLASS, drmmode_output->backlight_iface);
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
drmmode_backlight_get(xf86OutputPtr output)
{
    drmmode_output_private_ptr drmmode_output = output->driver_private;
    char path[BACKLIGHT_PATH_LEN], val[BACKLIGHT_VALUE_LEN];
    int fd, level;

    if (! drmmode_output->backlight_iface)
	return -1;

    sprintf(path, "%s/%s/actual_brightness",
	    BACKLIGHT_CLASS, drmmode_output->backlight_iface);
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
    if (level > drmmode_output->backlight_max)
	level = drmmode_output->backlight_max;
    if (level < 0)
	level = -1;
    return level;
}

static int
drmmode_backlight_get_max(xf86OutputPtr output)
{
    drmmode_output_private_ptr drmmode_output = output->driver_private;
    char path[BACKLIGHT_PATH_LEN], val[BACKLIGHT_VALUE_LEN];
    int fd, max = 0;

    sprintf(path, "%s/%s/max_brightness",
	    BACKLIGHT_CLASS, drmmode_output->backlight_iface);
    fd = open(path, O_RDONLY);
    if (fd == -1) {
	xf86DrvMsg(output->scrn->scrnIndex, X_ERROR, "failed to open %s "
		   "for backlight control: %s\n", path, strerror(errno));
	return 0;
    }

    memset(val, 0, sizeof(val));
    if (read(fd, val, BACKLIGHT_VALUE_LEN) == -1) {
	close(fd);
	return -1;
    }

    close(fd);

    max = atoi(val);
    if (max <= 0)
	max  = -1;
    return max;
}

static void
drmmode_backlight_init(xf86OutputPtr output)
{
    drmmode_output_private_ptr drmmode_output = output->driver_private;
    char path[BACKLIGHT_PATH_LEN];
    struct stat buf;
    int i;

    for (i = 0; backlight_interfaces[i] != NULL; i++) {
	sprintf(path, "%s/%s", BACKLIGHT_CLASS, backlight_interfaces[i]);
	if (!stat(path, &buf)) {
	    drmmode_output->backlight_iface = backlight_interfaces[i];
	    xf86DrvMsg(output->scrn->scrnIndex, X_INFO,
		       "found backlight control interface %s\n", path);
	    drmmode_output->backlight_max = drmmode_backlight_get_max(output);
	    drmmode_output->backlight_active_level = drmmode_backlight_get(output);
	    return;
	}
    }
    drmmode_output->backlight_iface = NULL;
}


static void
drmmode_ConvertFromKMode(ScrnInfoPtr scrn,
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
drmmode_ConvertToKMode(ScrnInfoPtr scrn,
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
drmmode_crtc_dpms(xf86CrtcPtr drmmode_crtc, int mode)
{

}

static Bool
drmmode_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
		       Rotation rotation, int x, int y)
{
	ScrnInfoPtr scrn = crtc->scrn;
	intel_screen_private *intel = intel_get_screen_private(scrn);
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
	unsigned int pitch = scrn->displayWidth * intel->cpp;

	if (drmmode->fb_id == 0) {
		ret = drmModeAddFB(drmmode->fd,
				   scrn->virtualX, scrn->virtualY,
				   scrn->depth, scrn->bitsPerPixel,
				   pitch, intel->front_buffer->handle,
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

#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(1,7,0,0,0)
	crtc->funcs->gamma_set(crtc, crtc->gamma_red, crtc->gamma_green,
			       crtc->gamma_blue, crtc->gamma_size);
#endif

	drmmode_ConvertToKMode(crtc->scrn, &kmode, mode);


	fb_id = drmmode->fb_id;
	if (drmmode_crtc->rotate_fb_id) {
		fb_id = drmmode_crtc->rotate_fb_id;
		x = 0;
		y = 0;
	}
	ret = drmModeSetCrtc(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id,
			     fb_id, x, y, output_ids, output_count, &kmode);
	if (ret)
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "failed to set mode: %s", strerror(-ret));
	else
		ret = TRUE;

	/* Turn on any outputs on this crtc that may have been disabled */
	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];

		if (output->crtc != crtc)
			continue;

		drmmode_output_dpms(output, DPMSModeOn);
	}

	i830_set_gem_max_sizes(scrn);

	if (scrn->pScreen)
		xf86_reload_cursors(scrn->pScreen);
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
	int ret;

	/* cursor should be mapped already */
	ret = dri_bo_subdata(drmmode_crtc->cursor, 0, 64*64*4, image);
	if (ret)
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "failed to set cursor: %s", strerror(-ret));

	return;
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
	ScrnInfoPtr scrn = crtc->scrn;
	intel_screen_private *intel = intel_get_screen_private(scrn);
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	int size, ret;
	unsigned long rotate_pitch;

	width = i830_pad_drawable_width(width, drmmode->cpp);
	rotate_pitch = width * drmmode->cpp;
	size = rotate_pitch * height;

	drmmode_crtc->rotate_bo =
		drm_intel_bo_alloc(intel->bufmgr, "rotate", size, 4096);

	if (!drmmode_crtc->rotate_bo) {
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "Couldn't allocate shadow memory for rotated CRTC\n");
		return NULL;
	}

	drm_intel_bo_disable_reuse(drmmode_crtc->rotate_bo);

	ret = drmModeAddFB(drmmode->fd, width, height, crtc->scrn->depth,
			   crtc->scrn->bitsPerPixel, rotate_pitch,
			   drmmode_crtc->rotate_bo->handle,
			   &drmmode_crtc->rotate_fb_id);
	if (ret) {
		ErrorF("failed to add rotate fb\n");
		drm_intel_bo_unreference(drmmode_crtc->rotate_bo);
		return NULL;
	}

	return drmmode_crtc->rotate_bo;
}

static PixmapPtr
drmmode_crtc_shadow_create(xf86CrtcPtr crtc, void *data, int width, int height)
{
	ScrnInfoPtr scrn = crtc->scrn;
	intel_screen_private *intel = intel_get_screen_private(scrn);
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	unsigned long rotate_pitch;
	PixmapPtr rotate_pixmap;

	if (!data) {
		data = drmmode_crtc_shadow_allocate (crtc, width, height);
		if (!data) {
			xf86DrvMsg(scrn->scrnIndex, X_ERROR,
				   "Couldn't allocate shadow pixmap for rotated CRTC\n");
			return NULL;
		}
	}

	rotate_pitch =
		i830_pad_drawable_width(width, drmmode->cpp) * drmmode->cpp;
	rotate_pixmap = GetScratchPixmapHeader(scrn->pScreen,
					       width, height,
					       scrn->depth,
					       scrn->bitsPerPixel,
					       rotate_pitch,
					       NULL);

	if (rotate_pixmap == NULL) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "Couldn't allocate shadow pixmap for rotated CRTC\n");
		return NULL;
	}

	if (drmmode_crtc->rotate_bo)
		i830_set_pixmap_bo(rotate_pixmap, drmmode_crtc->rotate_bo);

	intel->shadow_present = TRUE;

	return rotate_pixmap;
}

static void
drmmode_crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr rotate_pixmap, void *data)
{
	ScrnInfoPtr scrn = crtc->scrn;
	intel_screen_private *intel = intel_get_screen_private(scrn);
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;

	if (rotate_pixmap) {
		i830_set_pixmap_bo(rotate_pixmap, NULL);
		FreeScratchPixmapHeader(rotate_pixmap);
	}


	if (data) {
		/* Be sure to sync acceleration before the memory gets
		 * unbound. */
		drmModeRmFB(drmmode->fd, drmmode_crtc->rotate_fb_id);
		drmmode_crtc->rotate_fb_id = 0;
		dri_bo_unreference(drmmode_crtc->rotate_bo);
		drmmode_crtc->rotate_bo = NULL;
	}
	intel->shadow_present = FALSE;
}

static void
drmmode_crtc_gamma_set(xf86CrtcPtr crtc,
		       CARD16 *red, CARD16 *green, CARD16 *blue, int size)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;

	drmModeCrtcSetGamma(drmmode->fd, drmmode_crtc->mode_crtc->crtc_id,
			    size, red, green, blue);
}

static void
drmmode_crtc_destroy(xf86CrtcPtr crtc)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;

	drm_intel_bo_unreference(drmmode_crtc->cursor);
	drmmode_crtc->cursor = NULL;
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
	.gamma_set = drmmode_crtc_gamma_set,
	.destroy = drmmode_crtc_destroy,
};


static void
drmmode_crtc_init(ScrnInfoPtr scrn, drmmode_ptr drmmode, int num)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	xf86CrtcPtr crtc;
	drmmode_crtc_private_ptr drmmode_crtc;

	crtc = xf86CrtcCreate(scrn, &drmmode_crtc_funcs);
	if (crtc == NULL)
		return;

	drmmode_crtc = xnfcalloc(sizeof(drmmode_crtc_private_rec), 1);
	drmmode_crtc->mode_crtc = drmModeGetCrtc(drmmode->fd,
						 drmmode->mode_res->crtcs[num]);
	drmmode_crtc->drmmode = drmmode;
	crtc->driver_private = drmmode_crtc;

	drmmode_crtc->cursor = drm_intel_bo_alloc(intel->bufmgr, "ARGB cursor",
						  HWCURSOR_SIZE_ARGB,
						  GTT_PAGE_SIZE);
	drm_intel_bo_disable_reuse(drmmode_crtc->cursor);

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
	drmmode_output_private_ptr drmmode_output = output->driver_private;
	drmModeConnectorPtr koutput = drmmode_output->mode_output;
	struct fixed_panel_lvds *p_lvds = drmmode_output->private_data;

	/*
	 * If the connector type is LVDS, we will use the panel limit to
	 * verfiy whether the mode is valid.
	 */
	if ((koutput->connector_type ==  DRM_MODE_CONNECTOR_LVDS) && p_lvds) {
		if (pModes->HDisplay > p_lvds->hdisplay ||
			pModes->VDisplay > p_lvds->vdisplay)
			return MODE_PANEL;
		else
			return MODE_OK;
	}
	return MODE_OK;
}

static DisplayModePtr
drmmode_output_lvds_edid(xf86OutputPtr output, DisplayModePtr modes)
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
drmmode_output_get_modes(xf86OutputPtr output)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;
	drmModeConnectorPtr koutput = drmmode_output->mode_output;
	drmmode_ptr drmmode = drmmode_output->drmmode;
	int i;
	DisplayModePtr Modes = NULL, Mode;
	drmModePropertyPtr props;
	struct fixed_panel_lvds *p_lvds;
	drmModeModeInfo *mode_ptr;

	/* look for an EDID property */
	for (i = 0; i < koutput->count_props; i++) {
		props = drmModeGetProperty(drmmode->fd, koutput->props[i]);
		if (!props)
			continue;
		if (!(props->flags & DRM_MODE_PROP_BLOB)) {
			drmModeFreeProperty(props);
			continue;
		}

		if (!strcmp(props->name, "EDID")) {
			drmModeFreePropertyBlob(drmmode_output->edid_blob);
			drmmode_output->edid_blob =
				drmModeGetPropertyBlob(drmmode->fd,
						       koutput->prop_values[i]);
		}
		drmModeFreeProperty(props);
	}

	if (drmmode_output->edid_blob)
		xf86OutputSetEDID(output,
				  xf86InterpretEDID(output->scrn->scrnIndex,
						    drmmode_output->edid_blob->data));
	else
		xf86OutputSetEDID(output,
				  xf86InterpretEDID(output->scrn->scrnIndex,
						    NULL));

	/* modes should already be available */
	for (i = 0; i < koutput->count_modes; i++) {
		Mode = xnfalloc(sizeof(DisplayModeRec));

		drmmode_ConvertFromKMode(output->scrn, &koutput->modes[i],
					 Mode);
		Modes = xf86ModesAdd(Modes, Mode);

	}
	p_lvds = drmmode_output->private_data;
	/*
	 * If the connector type is LVDS, we will traverse the kernel mode to
	 * get the panel limit.
	 * If it is incorrect, please fix me.
	 */
	if ((koutput->connector_type ==  DRM_MODE_CONNECTOR_LVDS) && p_lvds) {
		p_lvds->hdisplay = 0;
		p_lvds->vdisplay = 0;
		for (i = 0; i < koutput->count_modes; i++) {
			mode_ptr = &koutput->modes[i];
			if ((mode_ptr->hdisplay >= p_lvds->hdisplay) &&
				(mode_ptr->vdisplay >= p_lvds->vdisplay)) {
				p_lvds->hdisplay = mode_ptr->hdisplay;
				p_lvds->vdisplay = mode_ptr->vdisplay;
			}
		}
		if (!p_lvds->hdisplay || !p_lvds->vdisplay)
			xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
				"Incorrect KMS mode.\n");
	}

	if (koutput->connector_type ==  DRM_MODE_CONNECTOR_LVDS)
		Modes = drmmode_output_lvds_edid(output, Modes);

	return Modes;
}

static void
drmmode_output_destroy(xf86OutputPtr output)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;
	int i;

	if (drmmode_output->edid_blob)
		drmModeFreePropertyBlob(drmmode_output->edid_blob);
	for (i = 0; i < drmmode_output->num_props; i++) {
	    drmModeFreeProperty(drmmode_output->props[i].mode_prop);
	    xfree(drmmode_output->props[i].atoms);
	}
	xfree(drmmode_output->props);
	drmModeFreeConnector(drmmode_output->mode_output);
	drmmode_output->mode_output = NULL;
	if (drmmode_output->private_data) {
		xfree(drmmode_output->private_data);
		drmmode_output->private_data = NULL;
	}
	if (drmmode_output->backlight_iface)
		drmmode_backlight_set(output, drmmode_output->backlight_active_level);
	xfree(drmmode_output);
	output->driver_private = NULL;
}

static void
drmmode_output_dpms_backlight(xf86OutputPtr output, int oldmode, int mode)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;

	if (!drmmode_output->backlight_iface)
		return;

	if (mode == DPMSModeOn) {
		/* If we're going from off->on we may need to turn on the backlight. */
		if (oldmode != DPMSModeOn)
			drmmode_backlight_set(output, drmmode_output->backlight_active_level);
	} else {
		/* Only save the current backlight value if we're going from on to off. */
		if (oldmode == DPMSModeOn)
			drmmode_output->backlight_active_level = drmmode_backlight_get(output);
		drmmode_backlight_set(output, 0);
	}
}

static void
drmmode_output_dpms(xf86OutputPtr output, int mode)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;
	drmModeConnectorPtr koutput = drmmode_output->mode_output;
	drmmode_ptr drmmode = drmmode_output->drmmode;
	int i;
	drmModePropertyPtr props;

	for (i = 0; i < koutput->count_props; i++) {
		props = drmModeGetProperty(drmmode->fd, koutput->props[i]);
		if (!props)
			continue;

		if (!strcmp(props->name, "DPMS")) {
                        drmModeConnectorSetProperty(drmmode->fd,
                                drmmode_output->output_id,
                                props->prop_id,
                                mode);
			drmmode_output_dpms_backlight(output,
				drmmode_output->dpms_mode,
				mode);
			drmmode_output->dpms_mode = mode;
                        drmModeFreeProperty(props);
                        return;
		}
		drmModeFreeProperty(props);
	}
}

int
drmmode_output_dpms_status(xf86OutputPtr output)
{
	drmmode_output_private_ptr drmmode_output = output->driver_private;

	return drmmode_output->dpms_mode;
}

static Bool
drmmode_property_ignore(drmModePropertyPtr prop)
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
drmmode_output_create_resources(xf86OutputPtr output)
{
    drmmode_output_private_ptr drmmode_output = output->driver_private;
    drmModeConnectorPtr mode_output = drmmode_output->mode_output;
    drmmode_ptr drmmode = drmmode_output->drmmode;
    drmModePropertyPtr drmmode_prop;
    int i, j, err;

    drmmode_output->props = xcalloc(mode_output->count_props, sizeof(drmmode_prop_rec));
    if (!drmmode_output->props)
	return;

    drmmode_output->num_props = 0;
    for (i = 0, j = 0; i < mode_output->count_props; i++) {
	drmmode_prop = drmModeGetProperty(drmmode->fd, mode_output->props[i]);
	if (drmmode_property_ignore(drmmode_prop)) {
	    drmModeFreeProperty(drmmode_prop);
	    continue;
	}
	drmmode_output->props[j].mode_prop = drmmode_prop;
	drmmode_output->props[j].value = mode_output->prop_values[i];
	drmmode_output->num_props++;
	j++;
    }

    for (i = 0; i < drmmode_output->num_props; i++) {
	drmmode_prop_ptr p = &drmmode_output->props[i];
	drmmode_prop = p->mode_prop;

	if (drmmode_prop->flags & DRM_MODE_PROP_RANGE) {
	    INT32 range[2];

	    p->num_atoms = 1;
	    p->atoms = xcalloc(p->num_atoms, sizeof(Atom));
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
	    p->atoms = xcalloc(p->num_atoms, sizeof(Atom));
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

    if (drmmode_output->backlight_iface) {
	INT32 data, backlight_range[2];
	/* Set up the backlight property, which takes effect immediately
	 * and accepts values only within the backlight_range. */
	backlight_atom = MakeAtom(BACKLIGHT_NAME, sizeof(BACKLIGHT_NAME) - 1, TRUE);
	backlight_deprecated_atom = MakeAtom(BACKLIGHT_DEPRECATED_NAME,
		sizeof(BACKLIGHT_DEPRECATED_NAME) - 1, TRUE);

	backlight_range[0] = 0;
	backlight_range[1] = drmmode_output->backlight_max;
	err = RRConfigureOutputProperty(output->randr_output, backlight_atom,
	                                FALSE, TRUE, FALSE, 2, backlight_range);
	if (err != 0) {
	    xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
	               "RRConfigureOutputProperty error, %d\n", err);
	}
	err = RRConfigureOutputProperty(output->randr_output, backlight_deprecated_atom,
	                                FALSE, TRUE, FALSE, 2, backlight_range);
	if (err != 0) {
	    xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
	               "RRConfigureOutputProperty error, %d\n", err);
	}
	/* Set the current value of the backlight property */
	data = drmmode_output->backlight_active_level;
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
drmmode_output_set_property(xf86OutputPtr output, Atom property,
		RRPropertyValuePtr value)
{
    drmmode_output_private_ptr drmmode_output = output->driver_private;
    drmmode_ptr drmmode = drmmode_output->drmmode;
    int i;

    if (property == backlight_atom || property == backlight_deprecated_atom) {
	INT32 val;

	if (value->type != XA_INTEGER || value->format != 32 ||
	    value->size != 1)
	{
	    return FALSE;
	}

	val = *(INT32 *)value->data;
	if (val < 0 || val > drmmode_output->backlight_max)
	    return FALSE;

	if (drmmode_output->dpms_mode == DPMSModeOn)
	    drmmode_backlight_set(output, val);
	drmmode_output->backlight_active_level = val;
	return TRUE;
    }

    for (i = 0; i < drmmode_output->num_props; i++) {
	drmmode_prop_ptr p = &drmmode_output->props[i];

	if (p->atoms[0] != property)
	    continue;

	if (p->mode_prop->flags & DRM_MODE_PROP_RANGE) {
	    uint32_t val;

	    if (value->type != XA_INTEGER || value->format != 32 ||
		    value->size != 1)
		return FALSE;
	    val = *(uint32_t *)value->data;

	    drmModeConnectorSetProperty(drmmode->fd, drmmode_output->output_id,
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
		    drmModeConnectorSetProperty(drmmode->fd, drmmode_output->output_id,
			    p->mode_prop->prop_id, p->mode_prop->enums[j].value);
		    return TRUE;
		}
	    }
	    return FALSE;
	}
    }

    return TRUE;
}

static Bool
drmmode_output_get_property(xf86OutputPtr output, Atom property)
{
    drmmode_output_private_ptr drmmode_output = output->driver_private;
    int err;

    if (property == backlight_atom || property == backlight_deprecated_atom) {
	INT32 val;

	if (! drmmode_output->backlight_iface)
	    return FALSE;

	val = drmmode_backlight_get(output);
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

    return TRUE;
}

static const xf86OutputFuncsRec drmmode_output_funcs = {
	.create_resources = drmmode_output_create_resources,
#ifdef RANDR_12_INTERFACE
	.set_property = drmmode_output_set_property,
	.get_property = drmmode_output_get_property,
#endif
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

static const char *output_names[] = { "None",
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
};


static void
drmmode_output_init(ScrnInfoPtr scrn, drmmode_ptr drmmode, int num)
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

	snprintf(name, 32, "%s%d", output_names[koutput->connector_type],
		 koutput->connector_type_id);

	output = xf86OutputCreate (scrn, &drmmode_output_funcs, name);
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
	/*
	 * If the connector type of the output device is LVDS, we will
	 * allocate the private_data to store the panel limit.
	 * For example: hdisplay, vdisplay
	 */
	drmmode_output->private_data = NULL;
	if (koutput->connector_type ==  DRM_MODE_CONNECTOR_LVDS) {
		drmmode_output->private_data = xcalloc(
				sizeof(struct fixed_panel_lvds), 1);
		if (!drmmode_output->private_data)
			xf86DrvMsg(scrn->scrnIndex, X_ERROR,
				"Can't allocate private memory for LVDS.\n");
	}
	drmmode_output->output_id = drmmode->mode_res->connectors[num];
	drmmode_output->mode_output = koutput;
	drmmode_output->mode_encoder = kencoder;
	drmmode_output->drmmode = drmmode;
	output->mm_width = koutput->mmWidth;
	output->mm_height = koutput->mmHeight;

	output->subpixel_order = subpixel_conv_table[koutput->subpixel];
	output->driver_private = drmmode_output;

	if (koutput->connector_type ==  DRM_MODE_CONNECTOR_LVDS)
		drmmode_backlight_init(output);

	output->possible_crtcs = kencoder->possible_crtcs;
	output->possible_clones = kencoder->possible_clones;
	return;
}

static Bool
drmmode_xf86crtc_resize (ScrnInfoPtr scrn, int width, int height)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	drmmode_crtc_private_ptr
		    drmmode_crtc = xf86_config->crtc[0]->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	intel_screen_private *intel = intel_get_screen_private(scrn);
	drm_intel_bo *old_front = NULL;
	Bool	    ret;
	ScreenPtr   screen = screenInfo.screens[scrn->scrnIndex];
	uint32_t    old_fb_id;
	int	    i, pitch, old_width, old_height, old_pitch;

	if (scrn->virtualX == width && scrn->virtualY == height)
		return TRUE;

	pitch = i830_pad_drawable_width(width, intel->cpp);
	i830_tiled_width(intel, &pitch, intel->cpp);
	xf86DrvMsg(scrn->scrnIndex, X_INFO,
		   "Allocate new frame buffer %dx%d stride %d\n",
		   width, height, pitch);

	old_width = scrn->virtualX;
	old_height = scrn->virtualY;
	old_pitch = scrn->displayWidth;
	old_fb_id = drmmode->fb_id;
	old_front = intel->front_buffer;

	scrn->virtualX = width;
	scrn->virtualY = height;
	scrn->displayWidth = pitch;
	intel->front_buffer = i830_allocate_framebuffer(scrn);
	if (!intel->front_buffer)
		goto fail;

	ret = drmModeAddFB(drmmode->fd, width, height, scrn->depth,
			   scrn->bitsPerPixel, pitch * intel->cpp,
			   intel->front_buffer->handle,
			   &drmmode->fb_id);
	if (ret)
		goto fail;

	i830_set_pixmap_bo(screen->GetScreenPixmap(screen), intel->front_buffer);

	screen->ModifyPixmapHeader(screen->GetScreenPixmap(screen),
				   width, height, -1, -1, pitch * intel->cpp, NULL);

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
		drm_intel_bo_unreference(old_front);

	return TRUE;

 fail:
	if (intel->front_buffer)
		drm_intel_bo_unreference(intel->front_buffer);
	intel->front_buffer = old_front;
	scrn->virtualX = old_width;
	scrn->virtualY = old_height;
	scrn->displayWidth = old_pitch;
	drmmode->fb_id = old_fb_id;

	return FALSE;
}

Bool
drmmode_do_pageflip(ScreenPtr screen, dri_bo *new_front, dri_bo *old_front,
		    void *data)
{
	ScrnInfoPtr scrn = xf86Screens[screen->myNum];
	intel_screen_private *intel = intel_get_screen_private(scrn);
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(scrn);
	drmmode_crtc_private_ptr drmmode_crtc = config->crtc[0]->driver_private;
	drmmode_ptr drmmode = drmmode_crtc->drmmode;
	unsigned int pitch = scrn->displayWidth * intel->cpp;
	int i, old_fb_id;
	unsigned int crtc_id;

	/*
	 * Create a new handle for the back buffer
	 */
	old_fb_id = drmmode->fb_id;
	if (drmModeAddFB(drmmode->fd, scrn->virtualX, scrn->virtualY,
			 scrn->depth, scrn->bitsPerPixel, pitch,
			 new_front->handle, &drmmode->fb_id))
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
		xf86CrtcPtr crtc = config->crtc[i];

		if (!crtc->enabled)
			continue;

		drmmode_crtc = crtc->driver_private;
		crtc_id = drmmode_crtc->mode_crtc->crtc_id;
		drmmode->event_data = data;
		drmmode->flip_count++;
		if (drmModePageFlip(drmmode->fd, crtc_id, drmmode->fb_id,
				    DRM_MODE_PAGE_FLIP_EVENT, drmmode)) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "flip queue failed: %s\n", strerror(errno));
			goto error_undo;
		}
	}

	dri_bo_pin(new_front, 0);
	dri_bo_unpin(new_front);

	intel->front_buffer = new_front;
	drmmode->old_fb_id = old_fb_id;

	return TRUE;

error_undo:
	drmModeRmFB(drmmode->fd, drmmode->fb_id);
	drmmode->fb_id = old_fb_id;

error_out:
	xf86DrvMsg(scrn->scrnIndex, X_WARNING, "Page flip failed: %s\n",
		   strerror(errno));
	return FALSE;
}

static const xf86CrtcConfigFuncsRec drmmode_xf86crtc_config_funcs = {
	drmmode_xf86crtc_resize
};

static void
drmmode_vblank_handler(int fd, unsigned int frame, unsigned int tv_sec,
		       unsigned int tv_usec, void *event_data)
{
	I830DRI2FrameEventHandler(frame, tv_sec, tv_usec, event_data);
}

static void
drmmode_page_flip_handler(int fd, unsigned int frame, unsigned int tv_sec,
			  unsigned int tv_usec, void *event_data)
{
	drmmode_ptr drmmode = event_data;

	drmmode->flip_count--;
	if (drmmode->flip_count > 0)
		return;

	drmModeRmFB(drmmode->fd, drmmode->old_fb_id);

	I830DRI2FlipEventHandler(frame, tv_sec, tv_usec, drmmode->event_data);
}

static void
drm_wakeup_handler(pointer data, int err, pointer p)
{
    drmmode_ptr drmmode = data;
    fd_set *read_mask = p;

    if (err >= 0 && FD_ISSET(drmmode->fd, read_mask))
	drmHandleEvent(drmmode->fd, &drmmode->event_context);
}

Bool drmmode_pre_init(ScrnInfoPtr scrn, int fd, int cpp)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	struct drm_i915_getparam gp;
	drmmode_ptr drmmode;
	unsigned int i;
	int has_flipping = 0;

	drmmode = xnfalloc(sizeof *drmmode);
	drmmode->fd = fd;
	drmmode->fb_id = 0;

	xf86CrtcConfigInit(scrn, &drmmode_xf86crtc_config_funcs);

	drmmode->cpp = cpp;
	drmmode->mode_res = drmModeGetResources(drmmode->fd);
	if (!drmmode->mode_res) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "failed to get resources: %s\n", strerror(errno));
		return FALSE;
	}

	xf86CrtcSetSizeRange(scrn, 320, 200, drmmode->mode_res->max_width,
			     drmmode->mode_res->max_height);
	for (i = 0; i < drmmode->mode_res->count_crtcs; i++)
		drmmode_crtc_init(scrn, drmmode, i);

	for (i = 0; i < drmmode->mode_res->count_connectors; i++)
		drmmode_output_init(scrn, drmmode, i);

	xf86InitialConfiguration(scrn, TRUE);

	gp.param = I915_PARAM_HAS_PAGEFLIPPING;
	gp.value = &has_flipping;
	(void)drmCommandWriteRead(intel->drmSubFD, DRM_I915_GETPARAM, &gp,
				  sizeof(gp));
	if (has_flipping) {
		xf86DrvMsg(scrn->scrnIndex, X_INFO,
			   "Kernel page flipping support detected, enabling\n");
		intel->use_pageflipping = TRUE;
		drmmode->flip_count = 0;
		drmmode->event_context.version = DRM_EVENT_CONTEXT_VERSION;
		drmmode->event_context.vblank_handler = drmmode_vblank_handler;
		drmmode->event_context.page_flip_handler =
		    drmmode_page_flip_handler;
		AddGeneralSocket(fd);
		RegisterBlockAndWakeupHandlers((BlockHandlerProcPtr)NoopDDA,
					       drm_wakeup_handler, drmmode);
	}

	return TRUE;
}

int
drmmode_get_pipe_from_crtc_id(drm_intel_bufmgr *bufmgr, xf86CrtcPtr crtc)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;

	return drm_intel_get_pipe_from_crtc_id (bufmgr, drmmode_crtc->mode_crtc->crtc_id);
}

/* for the drmmode overlay */
int
drmmode_crtc_id(xf86CrtcPtr crtc)
{
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;

	return drmmode_crtc->mode_crtc->crtc_id;
}

void drmmode_closefb(ScrnInfoPtr scrn)
{
	xf86CrtcConfigPtr xf86_config;
	drmmode_crtc_private_ptr drmmode_crtc;
	drmmode_ptr drmmode;

	xf86_config = XF86_CRTC_CONFIG_PTR(scrn);

	drmmode_crtc = xf86_config->crtc[0]->driver_private;
	drmmode = drmmode_crtc->drmmode;

	drmModeRmFB(drmmode->fd, drmmode->fb_id);
	drmmode->fb_id = 0;
}
