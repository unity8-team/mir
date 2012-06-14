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

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

#include <xorgVersion.h>
#include <X11/Xatom.h>
#include <X11/extensions/dpmsconst.h>
#include <xf86drm.h>
#include <xf86DDC.h> /* for xf86InterpretEDID */

#include "sna.h"
#include "sna_reg.h"

#include "intel_options.h"

#if DEBUG_DISPLAY
#undef DBG
#define DBG(x) ErrorF x
#endif

#if 0
#define __DBG DBG
#else
#define __DBG(x)
#endif

struct sna_crtc {
	struct drm_mode_modeinfo kmode;
	PixmapPtr shadow;
	uint32_t shadow_fb_id;
	uint32_t cursor;
	uint8_t id;
	uint8_t pipe;
	uint8_t plane;
	uint8_t active;
	struct list link;
};

struct sna_property {
	drmModePropertyPtr mode_prop;
	uint64_t value;
	int num_atoms; /* if range prop, num_atoms == 1; if enum prop, num_atoms == num_enums + 1 */
	Atom *atoms;
};

struct sna_output {
	int id;
	drmModeConnectorPtr mode_output;
	int num_props;
	struct sna_property *props;

	Bool has_panel_limits;
	int panel_hdisplay;
	int panel_vdisplay;

	int dpms_mode;
	const char *backlight_iface;
	int backlight_active_level;
	int backlight_max;
	struct list link;
};

static inline struct sna_crtc *to_sna_crtc(xf86CrtcPtr crtc)
{
	return crtc->driver_private;
}

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
crtc_id(struct sna_crtc *crtc)
{
	return crtc->id;
}

int sna_crtc_id(xf86CrtcPtr crtc)
{
	return to_sna_crtc(crtc)->id;
}

int sna_crtc_on(xf86CrtcPtr crtc)
{
	return to_sna_crtc(crtc)->active;
}

int sna_crtc_to_pipe(xf86CrtcPtr crtc)
{
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
	return sna_crtc->pipe;
}

int sna_crtc_to_plane(xf86CrtcPtr crtc)
{
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
	return sna_crtc->plane;
}

static unsigned get_fb(struct sna *sna, struct kgem_bo *bo,
		       int width, int height)
{
	ScrnInfoPtr scrn = sna->scrn;
	struct drm_mode_fb_cmd arg;
	int ret;

	assert(bo->proxy == NULL);
	if (bo->delta) {
		DBG(("%s: reusing fb=%d for handle=%d\n",
		     __FUNCTION__, bo->delta, bo->handle));
		return bo->delta;
	}

	DBG(("%s: create fb %dx%d@%d/%d\n",
	     __FUNCTION__, width, height, scrn->depth, scrn->bitsPerPixel));

	assert(bo->tiling != I915_TILING_Y);

	VG_CLEAR(arg);
	arg.width = width;
	arg.height = height;
	arg.pitch = bo->pitch;
	arg.bpp = scrn->bitsPerPixel;
	arg.depth = scrn->depth;
	arg.handle = bo->handle;

	if ((ret = drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_ADDFB, &arg))) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "%s: failed to add fb: %dx%d depth=%d, bpp=%d, pitch=%d: %d\n",
			   __FUNCTION__, width, height,
			   scrn->depth, scrn->bitsPerPixel, bo->pitch, ret);
		return 0;
	}

	bo->scanout = true;
	return bo->delta = arg.fb_id;
}

static uint32_t gem_create(int fd, int size)
{
	struct drm_i915_gem_create create;

	VG_CLEAR(create);
	create.handle = 0;
	create.size = ALIGN(size, 4096);
	(void)drmIoctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create);

	return create.handle;
}

static void gem_close(int fd, uint32_t handle)
{
	struct drm_gem_close close;

	VG_CLEAR(close);
	close.handle = handle;
	(void)drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &close);
}

static void
sna_output_backlight_set(xf86OutputPtr output, int level)
{
	struct sna_output *sna_output = output->driver_private;
	char path[BACKLIGHT_PATH_LEN], val[BACKLIGHT_VALUE_LEN];
	int fd, len, ret;

	DBG(("%s: level=%d\n", __FUNCTION__, level));

	if (level > sna_output->backlight_max)
		level = sna_output->backlight_max;
	if (! sna_output->backlight_iface || level < 0)
		return;

	len = snprintf(val, BACKLIGHT_VALUE_LEN, "%d\n", level);
	sprintf(path, "%s/%s/brightness",
		BACKLIGHT_CLASS, sna_output->backlight_iface);
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
sna_output_backlight_get(xf86OutputPtr output)
{
	struct sna_output *sna_output = output->driver_private;
	char path[BACKLIGHT_PATH_LEN], val[BACKLIGHT_VALUE_LEN];
	int fd, level;

	sprintf(path, "%s/%s/actual_brightness",
		BACKLIGHT_CLASS, sna_output->backlight_iface);
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
	DBG(("%s: level=%d (max=%d)\n",
	     __FUNCTION__, level, sna_output->backlight_max));

	if (level > sna_output->backlight_max)
		level = sna_output->backlight_max;
	else if (level < 0)
		level = -1;
	return level;
}

static int
sna_output_backlight_get_max(xf86OutputPtr output)
{
	struct sna_output *sna_output = output->driver_private;
	char path[BACKLIGHT_PATH_LEN], val[BACKLIGHT_VALUE_LEN];
	int fd, max = 0;

	sprintf(path, "%s/%s/max_brightness",
		BACKLIGHT_CLASS, sna_output->backlight_iface);
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
sna_output_backlight_init(xf86OutputPtr output)
{
	struct sna_output *sna_output = output->driver_private;
	int i;

	for (i = 0; backlight_interfaces[i] != NULL; i++) {
		char path[BACKLIGHT_PATH_LEN];
		struct stat buf;

		sprintf(path, "%s/%s", BACKLIGHT_CLASS, backlight_interfaces[i]);
		if (!stat(path, &buf)) {
			sna_output->backlight_iface = backlight_interfaces[i];
			sna_output->backlight_max = sna_output_backlight_get_max(output);
			if (sna_output->backlight_max > 0) {
				sna_output->backlight_active_level = sna_output_backlight_get(output);
				xf86DrvMsg(output->scrn->scrnIndex, X_INFO,
					   "found backlight control interface %s\n", path);
				return;
			}
		}
	}
	sna_output->backlight_iface = NULL;
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
mode_to_kmode(struct drm_mode_modeinfo *kmode, DisplayModePtr mode)
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

bool sna_crtc_is_bound(struct sna *sna, xf86CrtcPtr crtc)
{
	struct drm_mode_crtc mode;

	VG_CLEAR(mode);
	mode.crtc_id = to_sna_crtc(crtc)->id;
	if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETCRTC, &mode))
		return false;

	DBG(("%s: crtc=%d, mode valid?=%d, fb attached?=%d\n", __FUNCTION__,
	     mode.crtc_id, mode.mode_valid, sna->mode.fb_id == mode.fb_id));
	return mode.mode_valid && sna->mode.fb_id == mode.fb_id;
}

static Bool
sna_crtc_apply(xf86CrtcPtr crtc)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	struct sna_mode *mode = &sna->mode;
	struct drm_mode_crtc arg;
	uint32_t output_ids[16];
	int output_count = 0;
	int fb_id, x, y;
	int i, ret = FALSE;

	DBG(("%s\n", __FUNCTION__));

	assert(xf86_config->num_output < ARRAY_SIZE(output_ids));

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr output = xf86_config->output[i];
		struct sna_output *sna_output;

		if (output->crtc != crtc)
			continue;

		sna_output = output->driver_private;
		output_ids[output_count] =
			sna_output->mode_output->connector_id;
		output_count++;
	}

	if (!xf86CrtcRotate(crtc)) {
		DBG(("%s: failed to rotate crtc\n", __FUNCTION__));
		return FALSE;
	}

	crtc->funcs->gamma_set(crtc, crtc->gamma_red, crtc->gamma_green,
			       crtc->gamma_blue, crtc->gamma_size);

	x = crtc->x;
	y = crtc->y;
	fb_id = mode->fb_id;
	if (sna_crtc->shadow_fb_id) {
		fb_id = sna_crtc->shadow_fb_id;
		x = 0;
		y = 0;
	}

	xf86DrvMsg(crtc->scrn->scrnIndex, X_INFO,
		   "switch to mode %dx%d on crtc %d (pipe %d)\n",
		   sna_crtc->kmode.hdisplay,
		   sna_crtc->kmode.vdisplay,
		   sna_crtc->id, sna_crtc->pipe);

	DBG(("%s: applying crtc [%d] mode=%dx%d@%d, fb=%d%s update to %d outputs\n",
	     __FUNCTION__, sna_crtc->id,
	     sna_crtc->kmode.hdisplay,
	     sna_crtc->kmode.vdisplay,
	     sna_crtc->kmode.clock,
	     fb_id, sna_crtc->shadow_fb_id ? " [shadow]" : "",
	     output_count));

	VG_CLEAR(arg);
	arg.x = x;
	arg.y = y;
	arg.crtc_id = sna_crtc->id;
	arg.fb_id = fb_id;
	arg.set_connectors_ptr = (uintptr_t)output_ids;
	arg.count_connectors = output_count;
	arg.mode = sna_crtc->kmode;
	arg.mode_valid = 1;
	ret = drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_SETCRTC, &arg);
	if (ret) {
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "failed to set mode: %s\n", strerror(-ret));
		ret = FALSE;
	} else
		ret = TRUE;

	if (crtc->scrn->pScreen)
		xf86_reload_cursors(crtc->scrn->pScreen);

	return ret;
}

static void
sna_crtc_disable(xf86CrtcPtr crtc)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
	struct drm_mode_crtc arg;

	DBG(("%s: disabling crtc [%d]\n", __FUNCTION__, sna_crtc->id));

	VG_CLEAR(arg);
	arg.crtc_id = sna_crtc->id;
	arg.fb_id = 0;
	arg.mode_valid = 0;
	(void)drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_SETCRTC, &arg);
	sna_crtc->active = false;
}

static void
sna_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
	DBG(("%s(pipe %d, dpms mode -> %d):= active=%d\n",
	     __FUNCTION__, to_sna_crtc(crtc)->pipe, mode, mode == DPMSModeOn));

	if (mode == DPMSModeOff)
		sna_crtc_disable(crtc);
}

static struct kgem_bo *sna_create_bo_for_fbcon(struct sna *sna,
					       const struct drm_mode_fb_cmd *fbcon)
{
	struct drm_gem_flink flink;
	struct kgem_bo *bo;
	int ret;

	/* Create a new reference for the fbcon so that we can track it
	 * using a normal bo and so that when we call gem_close on it we
	 * delete our reference and not fbcon's!
	 */
	VG_CLEAR(flink);
	flink.handle = fbcon->handle;
	ret = drmIoctl(sna->kgem.fd, DRM_IOCTL_GEM_FLINK, &flink);
	if (ret)
		return NULL;

	bo = kgem_create_for_name(&sna->kgem, flink.name);
	if (bo == NULL)
		return NULL;

	bo->pitch = fbcon->pitch;
	return bo;
}

/* Copy the current framebuffer contents into the front-buffer for a seamless
 * transition from e.g. plymouth.
 */
void sna_copy_fbcon(struct sna *sna)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(sna->scrn);
	struct drm_mode_fb_cmd fbcon;
	PixmapPtr scratch;
	struct sna_pixmap *priv;
	struct kgem_bo *bo;
	BoxRec box;
	bool ok;
	int sx, sy;
	int dx, dy;
	int i;

	if (wedged(sna))
		return;

	DBG(("%s\n", __FUNCTION__));

	/* Scan the connectors for a framebuffer and assume that is the fbcon */
	VG_CLEAR(fbcon);
	fbcon.fb_id = 0;
	for (i = 0; i < xf86_config->num_crtc; i++) {
		struct sna_crtc *crtc = to_sna_crtc(xf86_config->crtc[i]);
		struct drm_mode_crtc mode;

		VG_CLEAR(mode);
		mode.crtc_id = crtc->id;
		if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETCRTC, &mode))
			continue;
		if (!mode.fb_id)
			continue;

		fbcon.fb_id = mode.fb_id;
		if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETFB, &fbcon)) {
			fbcon.fb_id = 0;
			continue;
		}
		break;
	}
	if (fbcon.fb_id == 0) {
		DBG(("%s: no fbcon found\n", __FUNCTION__));
		return;
	}

	/* Wrap the fbcon in a pixmap so that we select the right formats
	 * in the render copy in case we need to preserve the fbcon
	 * across a depth change upon starting X.
	 */
	scratch = GetScratchPixmapHeader(sna->scrn->pScreen,
					fbcon.width, fbcon.height,
					fbcon.depth, fbcon.bpp,
					0, NULL);
	if (scratch == NullPixmap)
		return;

	box.x1 = box.y1 = 0;
	box.x2 = min(fbcon.width, sna->front->drawable.width);
	box.y2 = min(fbcon.height, sna->front->drawable.height);

	bo = sna_create_bo_for_fbcon(sna, &fbcon);
	if (bo == NULL)
		goto cleanup_scratch;

	DBG(("%s: fbcon handle=%d\n", __FUNCTION__, bo->handle));

	priv = sna_pixmap(sna->front);
	assert(priv && priv->gpu_bo);

	sx = dx = 0;
	if (box.x2 < (uint16_t)fbcon.width)
		sx = (fbcon.width - box.x2) / 2.;
	if (box.x2 < sna->front->drawable.width)
		dx = (sna->front->drawable.width - box.x2) / 2.;

	sy = dy = 0;
	if (box.y2 < (uint16_t)fbcon.height)
		sy = (fbcon.height - box.y2) / 2.;
	if (box.y2 < sna->front->drawable.height)
		dy = (sna->front->drawable.height - box.y2) / 2.;

	ok = sna->render.copy_boxes(sna, GXcopy,
				    scratch, bo, sx, sy,
				    sna->front, priv->gpu_bo, dx, dy,
				    &box, 1);
	if (!DAMAGE_IS_ALL(priv->gpu_damage))
		sna_damage_add_box(&priv->gpu_damage, &box);

	kgem_bo_destroy(&sna->kgem, bo);

	sna->scrn->pScreen->canDoBGNoneRoot = ok;

cleanup_scratch:
	FreeScratchPixmapHeader(scratch);
}

static void update_flush_interval(struct sna *sna)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(sna->scrn);
	int i, max_vrefresh = 0;

	for (i = 0; i < xf86_config->num_crtc; i++) {
		if (!xf86_config->crtc[i]->enabled)
			continue;

		max_vrefresh = max(max_vrefresh,
				   xf86ModeVRefresh(&xf86_config->crtc[i]->mode));
	}

	if (max_vrefresh == 0)
		max_vrefresh = 40;

	sna->vblank_interval = 1000 / max_vrefresh; /* Hz -> ms */
	DBG(("max_vrefresh=%d, vblank_interval=%d ms\n",
	       max_vrefresh, sna->vblank_interval));
}

static Bool
sna_crtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
			Rotation rotation, int x, int y)
{
	ScrnInfoPtr scrn = crtc->scrn;
	struct sna *sna = to_sna(scrn);
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
	struct sna_mode *sna_mode = &sna->mode;
	int saved_x, saved_y;
	Rotation saved_rotation;
	DisplayModeRec saved_mode;

	DBG(("%s(rotation=%d, x=%d, y=%d, mode=%dx%d@%d)\n",
	     __FUNCTION__, rotation, x, y,
	     mode->HDisplay, mode->VDisplay, mode->Clock));

	DBG(("%s: current fb pixmap = %d, front is %lu\n",
	     __FUNCTION__,
	     sna_mode->fb_pixmap,
	     sna->front->drawable.serialNumber));

	if (sna_mode->fb_pixmap != sna->front->drawable.serialNumber) {
		kgem_submit(&sna->kgem);
		sna_mode_remove_fb(sna);
	}

	if (sna_mode->fb_id == 0) {
		struct kgem_bo *bo = sna_pixmap_pin(sna->front);
		if (!bo)
			return FALSE;

		/* XXX recreate the fb in case the size has changed? */
		sna_mode->fb_id = get_fb(sna, bo,
					 scrn->virtualX, scrn->virtualY);
		if (sna_mode->fb_id == 0)
			return FALSE;

		DBG(("%s: handle %d attached to fb %d\n",
		     __FUNCTION__, bo->handle, sna_mode->fb_id));

		sna_mode->fb_pixmap = sna->front->drawable.serialNumber;
	}

	saved_mode = crtc->mode;
	saved_x = crtc->x;
	saved_y = crtc->y;
	saved_rotation = crtc->rotation;

	crtc->mode = *mode;
	crtc->x = x;
	crtc->y = y;
	crtc->rotation = rotation;

	mode_to_kmode(&sna_crtc->kmode, mode);
	if (!sna_crtc_apply(crtc)) {
		crtc->x = saved_x;
		crtc->y = saved_y;
		crtc->rotation = saved_rotation;
		crtc->mode = saved_mode;
		return FALSE;
	}
	sna_mode_update(sna);

	update_flush_interval(sna);
	return TRUE;
}

void sna_mode_adjust_frame(struct sna *sna, int x, int y)
{
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(sna->scrn);
	xf86OutputPtr output = config->output[config->compat_output];
	xf86CrtcPtr crtc = output->crtc;

	if (crtc && crtc->enabled)
		sna_crtc_set_mode_major(crtc, &crtc->mode, crtc->rotation, x, y);
}

static void
sna_crtc_hide_cursor(xf86CrtcPtr crtc)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
	struct drm_mode_cursor arg;

	__DBG(("%s: CRTC:%d\n", __FUNCTION__, sna_crtc->id));

	VG_CLEAR(arg);
	arg.flags = DRM_MODE_CURSOR_BO;
	arg.crtc_id = sna_crtc->id;
	arg.width = arg.height = 64;
	arg.handle = 0;

	(void)drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_CURSOR, &arg);
}

static void
sna_crtc_show_cursor(xf86CrtcPtr crtc)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
	struct drm_mode_cursor arg;

	__DBG(("%s: CRTC:%d\n", __FUNCTION__, sna_crtc->id));

	VG_CLEAR(arg);
	arg.flags = DRM_MODE_CURSOR_BO;
	arg.crtc_id = sna_crtc->id;
	arg.width = arg.height = 64;
	arg.handle = sna_crtc->cursor;

	(void)drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_CURSOR, &arg);
}

static void
sna_crtc_set_cursor_colors(xf86CrtcPtr crtc, int bg, int fg)
{
	__DBG(("%s: CRTC:%d (bg=%x, fg=%x)\n", __FUNCTION__,
	       to_sna_crtc(crtc)->id, bg, fg));
}

static void
sna_crtc_set_cursor_position(xf86CrtcPtr crtc, int x, int y)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
	struct drm_mode_cursor arg;

	__DBG(("%s: CRTC:%d (%d, %d)\n", __FUNCTION__, sna_crtc->id, x, y));

	VG_CLEAR(arg);
	arg.flags = DRM_MODE_CURSOR_MOVE;
	arg.crtc_id = sna_crtc->id;
	arg.x = x;
	arg.y = y;
	arg.handle = sna_crtc->cursor;

	(void)drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_CURSOR, &arg);
}

static void
sna_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *image)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
	struct drm_i915_gem_pwrite pwrite;

	__DBG(("%s: CRTC:%d\n", __FUNCTION__, sna_crtc->id));

	VG_CLEAR(pwrite);
	pwrite.handle = sna_crtc->cursor;
	pwrite.offset = 0;
	pwrite.size = 64*64*4;
	pwrite.data_ptr = (uintptr_t)image;
	(void)drmIoctl(sna->kgem.fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite);
}

static void *
sna_crtc_shadow_allocate(xf86CrtcPtr crtc, int width, int height)
{
	ScrnInfoPtr scrn = crtc->scrn;
	struct sna *sna = to_sna(scrn);
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
	PixmapPtr shadow;
	struct kgem_bo *bo;

	DBG(("%s(%d, %d)\n", __FUNCTION__, width, height));

	shadow = scrn->pScreen->CreatePixmap(scrn->pScreen,
					     width, height, scrn->depth,
					     SNA_CREATE_FB);
	if (!shadow)
		return NULL;

	bo = sna_pixmap_pin(shadow);
	if (!bo) {
		scrn->pScreen->DestroyPixmap(shadow);
		return NULL;
	}

	sna_crtc->shadow_fb_id = get_fb(sna, bo, width, height);
	if (sna_crtc->shadow_fb_id == 0) {
		scrn->pScreen->DestroyPixmap(shadow);
		return NULL;
	}

	DBG(("%s: attached handle %d to fb %d\n",
	     __FUNCTION__, bo->handle, sna_crtc->shadow_fb_id));
	return sna_crtc->shadow = shadow;
}

static PixmapPtr
sna_crtc_shadow_create(xf86CrtcPtr crtc, void *data, int width, int height)
{
	return data;
}

static void
sna_crtc_shadow_destroy(xf86CrtcPtr crtc, PixmapPtr pixmap, void *data)
{
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);

	/* We may have not called shadow_create() on the data yet and
	 * be cleaning up a NULL shadow_pixmap.
	 */
	pixmap = data;

	DBG(("%s(fb=%d, handle=%d)\n", __FUNCTION__,
	     sna_crtc->shadow_fb_id, sna_pixmap_get_bo(pixmap)->handle));

	sna_crtc->shadow_fb_id = 0;

	pixmap->drawable.pScreen->DestroyPixmap(pixmap);
	sna_crtc->shadow = NULL;
}

static void
sna_crtc_gamma_set(xf86CrtcPtr crtc,
		       CARD16 *red, CARD16 *green, CARD16 *blue, int size)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);

	drmModeCrtcSetGamma(sna->kgem.fd, sna_crtc->id,
			    size, red, green, blue);
}

static void
sna_crtc_destroy(xf86CrtcPtr crtc)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = to_sna_crtc(crtc);

	sna_crtc_hide_cursor(crtc);
	gem_close(sna->kgem.fd, sna_crtc->cursor);

	list_del(&sna_crtc->link);
	free(sna_crtc);

	crtc->driver_private = NULL;
}

static const xf86CrtcFuncsRec sna_crtc_funcs = {
	.dpms = sna_crtc_dpms,
	.set_mode_major = sna_crtc_set_mode_major,
	.set_cursor_colors = sna_crtc_set_cursor_colors,
	.set_cursor_position = sna_crtc_set_cursor_position,
	.show_cursor = sna_crtc_show_cursor,
	.hide_cursor = sna_crtc_hide_cursor,
	.load_cursor_argb = sna_crtc_load_cursor_argb,
	.shadow_create = sna_crtc_shadow_create,
	.shadow_allocate = sna_crtc_shadow_allocate,
	.shadow_destroy = sna_crtc_shadow_destroy,
	.gamma_set = sna_crtc_gamma_set,
	.destroy = sna_crtc_destroy,
};

static uint32_t
sna_crtc_find_plane(struct sna *sna, int pipe)
{
	struct drm_mode_get_plane_res r;
	uint32_t *planes, id = 0;
	int i;

	VG_CLEAR(r);
	r.count_planes = 0;
	if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETPLANERESOURCES, &r))
		return 0;

	if (!r.count_planes)
		return 0;

	planes = malloc(sizeof(uint32_t)*r.count_planes);
	if (planes == NULL)
		return 0;

	r.plane_id_ptr = (uintptr_t)planes;
	if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETPLANERESOURCES, &r))
		r.count_planes = 0;

	for (i = 0; i < r.count_planes; i++) {
		struct drm_mode_get_plane p;

		VG_CLEAR(p);
		p.plane_id = planes[i];
		p.count_format_types = 0;
		if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETPLANE, &p) == 0) {
			if (p.possible_crtcs & (1 << pipe)) {
				id = p.plane_id;
				break;
			}
		}
	}
	free(planes);

	return id;
}

static void
sna_crtc_init(ScrnInfoPtr scrn, struct sna_mode *mode, int num)
{
	struct sna *sna = to_sna(scrn);
	xf86CrtcPtr crtc;
	struct sna_crtc *sna_crtc;
	struct drm_i915_get_pipe_from_crtc_id get_pipe;

	DBG(("%s\n", __FUNCTION__));

	sna_crtc = calloc(sizeof(struct sna_crtc), 1);
	if (sna_crtc == NULL)
		return;

	sna_crtc->id = mode->mode_res->crtcs[num];

	VG_CLEAR(get_pipe);
	get_pipe.pipe = 0;
	get_pipe.crtc_id = sna_crtc->id;
	if (drmIoctl(sna->kgem.fd,
		     DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID,
		     &get_pipe)) {
		free(sna_crtc);
		return;
	}
	sna_crtc->pipe = get_pipe.pipe;
	sna_crtc->plane = sna_crtc_find_plane(sna, sna_crtc->pipe);

	if (xf86IsEntityShared(scrn->entityList[0]) &&
	    scrn->confScreen->device->screen != sna_crtc->pipe) {
		free(sna_crtc);
		return;
	}

	crtc = xf86CrtcCreate(scrn, &sna_crtc_funcs);
	if (crtc == NULL) {
		free(sna_crtc);
		return;
	}

	crtc->driver_private = sna_crtc;

	sna_crtc->cursor = gem_create(sna->kgem.fd, 64*64*4);
	DBG(("%s: created handle=%d for cursor on CRTC:%d\n",
	     __FUNCTION__, sna_crtc->cursor, sna_crtc->id));

	list_add(&sna_crtc->link, &mode->crtcs);

	DBG(("%s: attached crtc[%d] id=%d, pipe=%d\n",
	     __FUNCTION__, num, sna_crtc->id, sna_crtc->pipe));
}

static Bool
is_panel(int type)
{
	return (type == DRM_MODE_CONNECTOR_LVDS ||
		type == DRM_MODE_CONNECTOR_eDP);
}

static xf86OutputStatus
sna_output_detect(xf86OutputPtr output)
{
	/* go to the hw and retrieve a new output struct */
	struct sna *sna = to_sna(output->scrn);
	struct sna_output *sna_output = output->driver_private;
	xf86OutputStatus status;

	DBG(("%s\n", __FUNCTION__));

	drmModeFreeConnector(sna_output->mode_output);
	sna_output->mode_output =
		drmModeGetConnector(sna->kgem.fd, sna_output->id);

	switch (sna_output->mode_output->connection) {
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
sna_output_mode_valid(xf86OutputPtr output, DisplayModePtr pModes)
{
	struct sna_output *sna_output = output->driver_private;

	/*
	 * If the connector type is a panel, we will use the panel limit to
	 * verfiy whether the mode is valid.
	 */
	if (sna_output->has_panel_limits) {
		if (pModes->HDisplay > sna_output->panel_hdisplay ||
		    pModes->VDisplay > sna_output->panel_vdisplay)
			return MODE_PANEL;
	}

	return MODE_OK;
}

static void
sna_output_attach_edid(xf86OutputPtr output)
{
	struct sna *sna = to_sna(output->scrn);
	struct sna_output *sna_output = output->driver_private;
	drmModeConnectorPtr koutput = sna_output->mode_output;
	void *raw = NULL;
	int raw_length = 0;
	xf86MonPtr mon = NULL;
	int i;

	/* look for an EDID property */
	for (i = 0; i < koutput->count_props; i++) {
		struct drm_mode_get_property prop;
		struct drm_mode_get_blob blob;
		void *tmp;

		VG_CLEAR(prop);
		prop.prop_id = koutput->props[i];
		prop.count_values = 0;
		prop.count_enum_blobs = 0;
		if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETPROPERTY, &prop))
			continue;

		if (!(prop.flags & DRM_MODE_PROP_BLOB))
			continue;

		if (strcmp(prop.name, "EDID"))
			continue;

		VG_CLEAR(blob);
		blob.length = 0;
		blob.data =0;
		blob.blob_id = koutput->prop_values[i];

		if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETPROPBLOB, &blob))
			continue;

		DBG(("%s: retreiving blob (property %d, id=%d, value=%ld), length=%d\n",
		     __FUNCTION__, i, koutput->props[i], (long)koutput->prop_values[i],
		     blob.length));

		tmp = malloc(blob.length);
		if (tmp == NULL)
			continue;

		VG(memset(tmp, 0, blob.length));
		blob.data = (uintptr_t)tmp;
		if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETPROPBLOB, &blob)) {
			free(tmp);
			continue;
		}

		free(raw);
		raw = tmp;
		raw_length = blob.length;
	}

	if (raw) {
		mon = xf86InterpretEDID(output->scrn->scrnIndex, raw);
		if (mon && raw_length > 128)
			mon->flags |= MONITOR_EDID_COMPLETE_RAWDATA;
	}

	xf86OutputSetEDID(output, mon);
	free(raw);
}

static DisplayModePtr
sna_output_panel_edid(xf86OutputPtr output, DisplayModePtr modes)
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

		m = xf86GetDefaultModes();
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
sna_output_get_modes(xf86OutputPtr output)
{
	struct sna_output *sna_output = output->driver_private;
	drmModeConnectorPtr koutput = sna_output->mode_output;
	DisplayModePtr Modes = NULL;
	int i;

	DBG(("%s\n", __FUNCTION__));

	sna_output_attach_edid(output);

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
	sna_output->has_panel_limits = FALSE;
	if (is_panel(koutput->connector_type)) {
		for (i = 0; i < koutput->count_modes; i++) {
			drmModeModeInfo *mode_ptr;

			mode_ptr = &koutput->modes[i];
			if (mode_ptr->hdisplay > sna_output->panel_hdisplay)
				sna_output->panel_hdisplay = mode_ptr->hdisplay;
			if (mode_ptr->vdisplay > sna_output->panel_vdisplay)
				sna_output->panel_vdisplay = mode_ptr->vdisplay;
		}

		sna_output->has_panel_limits =
			sna_output->panel_hdisplay &&
			sna_output->panel_vdisplay;

		Modes = sna_output_panel_edid(output, Modes);
	}

	return Modes;
}

static void
sna_output_destroy(xf86OutputPtr output)
{
	struct sna_output *sna_output = output->driver_private;
	int i;

	for (i = 0; i < sna_output->num_props; i++) {
		drmModeFreeProperty(sna_output->props[i].mode_prop);
		free(sna_output->props[i].atoms);
	}
	free(sna_output->props);

	drmModeFreeConnector(sna_output->mode_output);
	sna_output->mode_output = NULL;

	list_del(&sna_output->link);
	free(sna_output);

	output->driver_private = NULL;
}

static void
sna_output_dpms_backlight(xf86OutputPtr output, int oldmode, int mode)
{
	struct sna_output *sna_output = output->driver_private;

	if (!sna_output->backlight_iface)
		return;

	if (mode == DPMSModeOn) {
		/* If we're going from off->on we may need to turn on the backlight. */
		if (oldmode != DPMSModeOn)
			sna_output_backlight_set(output,
						   sna_output->backlight_active_level);
	} else {
		/* Only save the current backlight value if we're going from on to off. */
		if (oldmode == DPMSModeOn)
			sna_output->backlight_active_level = sna_output_backlight_get(output);
		sna_output_backlight_set(output, 0);
	}
}

static void
sna_output_dpms(xf86OutputPtr output, int dpms)
{
	struct sna *sna = to_sna(output->scrn);
	struct sna_output *sna_output = output->driver_private;
	drmModeConnectorPtr koutput = sna_output->mode_output;
	int i;

	DBG(("%s: dpms=%d\n", __FUNCTION__, dpms));

	for (i = 0; i < koutput->count_props; i++) {
		struct drm_mode_get_property prop;

		VG_CLEAR(prop);
		prop.prop_id = koutput->props[i];
		prop.count_values = 0;
		prop.count_enum_blobs = 0;
		if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETPROPERTY, &prop))
			continue;

		if (strcmp(prop.name, "DPMS"))
			continue;

		/* Record thevalue of the backlight before turning
		 * off the display, and reset if after turnging it on.
		 * Order is important as the kernel may record and also
		 * reset the backlight across DPMS. Hence we need to
		 * record the value before the kernel modifies it
		 * and reapply it afterwards.
		 */
		if (dpms == DPMSModeOff)
			sna_output_dpms_backlight(output,
						  sna_output->dpms_mode,
						  dpms);

		drmModeConnectorSetProperty(sna->kgem.fd,
					    sna_output->id,
					    prop.prop_id,
					    dpms);

		if (dpms != DPMSModeOff)
			sna_output_dpms_backlight(output,
						  sna_output->dpms_mode,
						  dpms);

		sna_output->dpms_mode = dpms;
		break;
	}
}

int
sna_output_dpms_status(xf86OutputPtr output)
{
	struct sna_output *sna_output = output->driver_private;
	return sna_output->dpms_mode;
}

static Bool
sna_property_ignore(drmModePropertyPtr prop)
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

static void
sna_output_create_ranged_atom(xf86OutputPtr output, Atom *atom,
			      const char *name, INT32 min, INT32 max,
			      uint64_t value, Bool immutable)
{
	int err;
	INT32 atom_range[2];

	atom_range[0] = min;
	atom_range[1] = max;

	*atom = MakeAtom(name, strlen(name), TRUE);

	err = RRConfigureOutputProperty(output->randr_output, *atom, FALSE,
					TRUE, immutable, 2, atom_range);
	if (err != 0)
		xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
			   "RRConfigureOutputProperty error, %d\n", err);

	err = RRChangeOutputProperty(output->randr_output, *atom, XA_INTEGER,
				     32, PropModeReplace, 1, &value, FALSE,
				     TRUE);
	if (err != 0)
		xf86DrvMsg(output->scrn->scrnIndex, X_ERROR,
			   "RRChangeOutputProperty error, %d\n", err);
}

#define BACKLIGHT_NAME             "Backlight"
#define BACKLIGHT_DEPRECATED_NAME  "BACKLIGHT"
static Atom backlight_atom, backlight_deprecated_atom;

static void
sna_output_create_resources(xf86OutputPtr output)
{
	struct sna *sna = to_sna(output->scrn);
	struct sna_output *sna_output = output->driver_private;
	drmModeConnectorPtr mode_output = sna_output->mode_output;
	int i, j, err;

	sna_output->props = calloc(mode_output->count_props,
				     sizeof(struct sna_property));
	if (!sna_output->props)
		return;

	sna_output->num_props = 0;
	for (i = j = 0; i < mode_output->count_props; i++) {
		drmModePropertyPtr drmmode_prop;

		drmmode_prop = drmModeGetProperty(sna->kgem.fd,
						  mode_output->props[i]);
		if (sna_property_ignore(drmmode_prop)) {
			drmModeFreeProperty(drmmode_prop);
			continue;
		}

		sna_output->props[j].mode_prop = drmmode_prop;
		sna_output->props[j].value = mode_output->prop_values[i];
		j++;
	}
	sna_output->num_props = j;

	for (i = 0; i < sna_output->num_props; i++) {
		struct sna_property *p = &sna_output->props[i];
		drmModePropertyPtr drmmode_prop = p->mode_prop;

		if (drmmode_prop->flags & DRM_MODE_PROP_RANGE) {
			p->num_atoms = 1;
			p->atoms = calloc(p->num_atoms, sizeof(Atom));
			if (!p->atoms)
				continue;

			sna_output_create_ranged_atom(output, &p->atoms[0],
						      drmmode_prop->name,
						      drmmode_prop->values[0],
						      drmmode_prop->values[1],
						      p->value,
						      drmmode_prop->flags & DRM_MODE_PROP_IMMUTABLE ? TRUE : FALSE);

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

	if (sna_output->backlight_iface) {
		/* Set up the backlight property, which takes effect
		 * immediately and accepts values only within the
		 * backlight_range.
		 */
		sna_output_create_ranged_atom(output, &backlight_atom,
					BACKLIGHT_NAME, 0,
					sna_output->backlight_max,
					sna_output->backlight_active_level,
					FALSE);
		sna_output_create_ranged_atom(output,
					&backlight_deprecated_atom,
					BACKLIGHT_DEPRECATED_NAME, 0,
					sna_output->backlight_max,
					sna_output->backlight_active_level,
					FALSE);
	}
}

static Bool
sna_output_set_property(xf86OutputPtr output, Atom property,
			    RRPropertyValuePtr value)
{
	struct sna *sna = to_sna(output->scrn);
	struct sna_output *sna_output = output->driver_private;
	int i;

	if (property == backlight_atom || property == backlight_deprecated_atom) {
		INT32 val;

		if (value->type != XA_INTEGER || value->format != 32 ||
		    value->size != 1)
		{
			return FALSE;
		}

		val = *(INT32 *)value->data;
		if (val < 0 || val > sna_output->backlight_max)
			return FALSE;

		if (sna_output->dpms_mode == DPMSModeOn)
			sna_output_backlight_set(output, val);
		sna_output->backlight_active_level = val;
		return TRUE;
	}

	for (i = 0; i < sna_output->num_props; i++) {
		struct sna_property *p = &sna_output->props[i];

		if (p->atoms[0] != property)
			continue;

		if (p->mode_prop->flags & DRM_MODE_PROP_RANGE) {
			uint32_t val;

			if (value->type != XA_INTEGER || value->format != 32 ||
			    value->size != 1)
				return FALSE;
			val = *(uint32_t *)value->data;

			drmModeConnectorSetProperty(sna->kgem.fd, sna_output->id,
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
			if (name == NULL)
				return FALSE;

			/* search for matching name string, then set its value down */
			for (j = 0; j < p->mode_prop->count_enums; j++) {
				if (!strcmp(p->mode_prop->enums[j].name, name)) {
					drmModeConnectorSetProperty(sna->kgem.fd, sna_output->id,
								    p->mode_prop->prop_id, p->mode_prop->enums[j].value);
					return TRUE;
				}
			}
			return FALSE;
		}
	}

	/* We didn't recognise this property, just report success in order
	 * to allow the set to continue, otherwise we break setting of
	 * common properties like EDID.
	 */
	return TRUE;
}

static Bool
sna_output_get_property(xf86OutputPtr output, Atom property)
{
	struct sna_output *sna_output = output->driver_private;
	int err;

	if (property == backlight_atom || property == backlight_deprecated_atom) {
		INT32 val;

		if (! sna_output->backlight_iface)
			return FALSE;

		val = sna_output_backlight_get(output);
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

static const xf86OutputFuncsRec sna_output_funcs = {
	.create_resources = sna_output_create_resources,
#ifdef RANDR_12_INTERFACE
	.set_property = sna_output_set_property,
	.get_property = sna_output_get_property,
#endif
	.dpms = sna_output_dpms,
	.detect = sna_output_detect,
	.mode_valid = sna_output_mode_valid,

	.get_modes = sna_output_get_modes,
	.destroy = sna_output_destroy
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

static bool
sna_zaphod_match(const char *s, const char *output)
{
	char t[20];
	unsigned int i = 0;

	do {
		/* match any outputs in a comma list, stopping at whitespace */
		switch (*s) {
		case '\0':
			t[i] = '\0';
			return strcmp(t, output) == 0;

		case ',':
			t[i] ='\0';
			if (strcmp(t, output) == 0)
				return TRUE;
			i = 0;
			break;

		case ' ':
		case '\t':
		case '\n':
		case '\r':
			break;

		default:
			t[i++] = *s;
			break;
		}

		s++;
	} while (i < sizeof(t));

	return FALSE;
}

static void
sna_output_init(ScrnInfoPtr scrn, struct sna_mode *mode, int num)
{
	struct sna *sna = to_sna(scrn);
	xf86OutputPtr output;
	drmModeConnectorPtr koutput;
	struct drm_mode_get_encoder enc;
	struct sna_output *sna_output;
	const char *output_name;
	const char *s;
	char name[32];

	koutput = drmModeGetConnector(sna->kgem.fd,
				      mode->mode_res->connectors[num]);
	if (!koutput)
		return;

	VG_CLEAR(enc);
	enc.encoder_id = koutput->encoders[0];
	if (drmIoctl(sna->kgem.fd, DRM_IOCTL_MODE_GETENCODER, &enc))
		goto cleanup_connector;

	if (koutput->connector_type < ARRAY_SIZE(output_names))
		output_name = output_names[koutput->connector_type];
	else
		output_name = "UNKNOWN";
	snprintf(name, 32, "%s%d", output_name, koutput->connector_type_id);

	if (xf86IsEntityShared(scrn->entityList[0])) {
		s = xf86GetOptValString(sna->Options, OPTION_ZAPHOD);
		if (s && !sna_zaphod_match(s, name))
			goto cleanup_connector;
	}

	output = xf86OutputCreate(scrn, &sna_output_funcs, name);
	if (!output)
		goto cleanup_connector;

	sna_output = calloc(sizeof(struct sna_output), 1);
	if (!sna_output)
		goto cleanup_output;

	sna_output->id = mode->mode_res->connectors[num];
	sna_output->mode_output = koutput;

	output->mm_width = koutput->mmWidth;
	output->mm_height = koutput->mmHeight;

	output->subpixel_order = subpixel_conv_table[koutput->subpixel];
	output->driver_private = sna_output;

	if (is_panel(koutput->connector_type))
		sna_output_backlight_init(output);

	output->possible_crtcs = enc.possible_crtcs;
	output->possible_clones = enc.possible_clones;
	output->interlaceAllowed = TRUE;

	list_add(&sna_output->link, &mode->outputs);

	return;

cleanup_output:
	xf86OutputDestroy(output);
cleanup_connector:
	drmModeFreeConnector(koutput);
}

struct sna_visit_set_pixmap_window {
	PixmapPtr old, new;
};

static int
sna_visit_set_window_pixmap(WindowPtr window, pointer data)
{
    struct sna_visit_set_pixmap_window *visit = data;
    ScreenPtr screen = window->drawable.pScreen;

    if (screen->GetWindowPixmap(window) == visit->old) {
	    screen->SetWindowPixmap(window, visit->new);
	    return WT_WALKCHILDREN;
    }

    return WT_DONTWALKCHILDREN;
}

static void
sna_redirect_screen_pixmap(ScrnInfoPtr scrn, PixmapPtr old, PixmapPtr new)
{
	ScreenPtr screen = scrn->pScreen;
	struct sna_visit_set_pixmap_window visit;

	visit.old = old;
	visit.new = new;
	TraverseTree(screen->root, sna_visit_set_window_pixmap, &visit);

	screen->SetScreenPixmap(new);
}

static Bool
sna_crtc_resize(ScrnInfoPtr scrn, int width, int height)
{
	struct sna *sna = to_sna(scrn);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	struct sna_mode *mode = &sna->mode;
	PixmapPtr old_front;
	uint32_t old_fb_id;
	struct kgem_bo *bo;
	int i;

	DBG(("%s (%d, %d) -> (%d, %d)\n",
	     __FUNCTION__,
	     scrn->virtualX, scrn->virtualY,
	     width, height));

	if (scrn->virtualX == width && scrn->virtualY == height)
		return TRUE;

	assert(scrn->pScreen->GetScreenPixmap(scrn->pScreen) == sna->front);
	assert(scrn->pScreen->GetWindowPixmap(scrn->pScreen->root) == sna->front);

	kgem_submit(&sna->kgem);

	old_fb_id = mode->fb_id;
	old_front = sna->front;

	sna->front = scrn->pScreen->CreatePixmap(scrn->pScreen,
						 width, height,
						 scrn->depth,
						 SNA_CREATE_FB);
	if (!sna->front)
		goto fail;

	bo = sna_pixmap_pin(sna->front);
	if (!bo)
		goto fail;

	assert(bo->delta == 0);

	mode->fb_id = get_fb(sna, bo, width, height);
	if (mode->fb_id == 0)
		goto fail;

	DBG(("%s: handle %d, pixmap serial %lu attached to fb %d\n",
	     __FUNCTION__, bo->handle,
	     sna->front->drawable.serialNumber, mode->fb_id));

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		if (!crtc->enabled)
			continue;

		if (!sna_crtc_apply(crtc))
			goto fail;
	}
	sna_mode_update(sna);

	kgem_bo_retire(&sna->kgem, bo);

	scrn->virtualX = width;
	scrn->virtualY = height;
	scrn->displayWidth = bo->pitch / sna->mode.cpp;

	sna->mode.fb_pixmap = sna->front->drawable.serialNumber;
	sna_redirect_screen_pixmap(scrn, old_front, sna->front);
	assert(scrn->pScreen->GetScreenPixmap(scrn->pScreen) == sna->front);
	assert(scrn->pScreen->GetWindowPixmap(scrn->pScreen->root) == sna->front);

	scrn->pScreen->DestroyPixmap(old_front);

	return TRUE;

fail:
	DBG(("%s: restoring original front pixmap and fb\n", __FUNCTION__));
	mode->fb_id = old_fb_id;

	if (sna->front)
		scrn->pScreen->DestroyPixmap(sna->front);
	sna->front = old_front;
	return FALSE;
}

static int do_page_flip(struct sna *sna, void *data, int ref_crtc_hw_id)
{
	xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(sna->scrn);
	int count = 0;
	int i;

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
		struct sna_crtc *crtc = config->crtc[i]->driver_private;
		uintptr_t evdata;

		DBG(("%s: crtc %d active? %d\n",__FUNCTION__, i,crtc->active));
		if (!crtc->active)
			continue;

		/* Only the reference crtc will finally deliver its page flip
		 * completion event. All other crtc's events will be discarded.
		 */
		evdata = (uintptr_t)data;
		evdata |= crtc->pipe == ref_crtc_hw_id;

		DBG(("%s: crtc %d [ref? %d] --> fb %d\n",
		     __FUNCTION__, crtc->id,
		     crtc->pipe == ref_crtc_hw_id,
		     sna->mode.fb_id));
		if (drmModePageFlip(sna->kgem.fd,
				    crtc->id,
				    sna->mode.fb_id,
				    DRM_MODE_PAGE_FLIP_EVENT,
				    (void*)evdata)) {
			int err = errno;
			DBG(("%s: flip [fb=%d] on crtc %d [%d] failed - %d\n",
			     __FUNCTION__, sna->mode.fb_id,
			     i, crtc->id, err));
			xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
				   "flip queue failed: %s\n", strerror(err));
			continue;
		}

		count++;
	}

	return count;
}

int
sna_page_flip(struct sna *sna,
	      struct kgem_bo *bo,
	      void *data,
	      int ref_crtc_hw_id,
	      uint32_t *old_fb)
{
	ScrnInfoPtr scrn = sna->scrn;
	struct sna_mode *mode = &sna->mode;
	int count;

	*old_fb = mode->fb_id;

	/*
	 * Create a new handle for the back buffer
	 */
	mode->fb_id = get_fb(sna, bo, scrn->virtualX, scrn->virtualY);
	if (mode->fb_id == 0) {
		mode->fb_id = *old_fb;
		return 0;
	}

	DBG(("%s: handle %d attached to fb %d\n",
	     __FUNCTION__, bo->handle, mode->fb_id));

	kgem_submit(&sna->kgem);

	/*
	 * Queue flips on all enabled CRTCs
	 * Note that if/when we get per-CRTC buffers, we'll have to update this.
	 * Right now it assumes a single shared fb across all CRTCs, with the
	 * kernel fixing up the offset of each CRTC as necessary.
	 *
	 * Also, flips queued on disabled or incorrectly configured displays
	 * may never complete; this is a configuration error.
	 */
	count = do_page_flip(sna, data, ref_crtc_hw_id);
	DBG(("%s: page flipped %d crtcs\n", __FUNCTION__, count));
	if (count == 0)
		mode->fb_id = *old_fb;

	return count;
}

static const xf86CrtcConfigFuncsRec sna_crtc_config_funcs = {
	sna_crtc_resize
};

Bool sna_mode_pre_init(ScrnInfoPtr scrn, struct sna *sna)
{
	struct sna_mode *mode = &sna->mode;
	int i;

	list_init(&mode->crtcs);
	list_init(&mode->outputs);

	xf86CrtcConfigInit(scrn, &sna_crtc_config_funcs);

	mode->mode_res = drmModeGetResources(sna->kgem.fd);
	if (!mode->mode_res) {
		xf86DrvMsg(scrn->scrnIndex, X_ERROR,
			   "failed to get resources: %s\n", strerror(errno));
		return FALSE;
	}

	xf86CrtcSetSizeRange(scrn,
			     320, 200,
			     mode->mode_res->max_width,
			     mode->mode_res->max_height);
	for (i = 0; i < mode->mode_res->count_crtcs; i++)
		sna_crtc_init(scrn, mode, i);

	for (i = 0; i < mode->mode_res->count_connectors; i++)
		sna_output_init(scrn, mode, i);

	xf86InitialConfiguration(scrn, TRUE);

	return TRUE;
}

void
sna_mode_remove_fb(struct sna *sna)
{
	struct sna_mode *mode = &sna->mode;

	DBG(("%s: deleting fb id %d for pixmap serial %d\n",
	     __FUNCTION__, mode->fb_id,mode->fb_pixmap));

	mode->fb_id = 0;
	mode->fb_pixmap = 0;
}

void
sna_mode_fini(struct sna *sna)
{
#if 0
	while (!list_is_empty(&mode->crtcs)) {
		xf86CrtcDestroy(list_first_entry(&mode->crtcs,
						 struct sna_crtc,
						 link)->crtc);
	}

	while (!list_is_empty(&mode->outputs)) {
		xf86OutputDestroy(list_first_entry(&mode->outputs,
						   struct sna_output,
						   link)->output);
	}
#endif

	sna_mode_remove_fb(sna);

	/* mode->shadow_fb_id should have been destroyed already */
}

static void sna_crtc_box(xf86CrtcPtr crtc, BoxPtr crtc_box)
{
	if (crtc->enabled) {
		crtc_box->x1 = crtc->x;
		crtc_box->y1 = crtc->y;

		switch (crtc->rotation & 0xf) {
		default:
			assert(0);
		case RR_Rotate_0:
		case RR_Rotate_180:
			crtc_box->x2 = crtc->x + crtc->mode.HDisplay;
			crtc_box->y2 = crtc->y + crtc->mode.VDisplay;
			break;

		case RR_Rotate_90:
		case RR_Rotate_270:
			crtc_box->x2 = crtc->x + crtc->mode.VDisplay;
			crtc_box->y2 = crtc->y + crtc->mode.HDisplay;
			break;
		}
	} else
		crtc_box->x1 = crtc_box->x2 = crtc_box->y1 = crtc_box->y2 = 0;
}

static void sna_box_intersect(BoxPtr r, const BoxRec *a, const BoxRec *b)
{
	r->x1 = a->x1 > b->x1 ? a->x1 : b->x1;
	r->x2 = a->x2 < b->x2 ? a->x2 : b->x2;
	r->y1 = a->y1 > b->y1 ? a->y1 : b->y1;
	r->y2 = a->y2 < b->y2 ? a->y2 : b->y2;
	DBG(("%s: (%d, %d), (%d, %d) intersect (%d, %d), (%d, %d) = (%d, %d), (%d, %d)\n",
	     __FUNCTION__,
	     a->x1, a->y1, a->x2, a->y2,
	     b->x1, b->y1, b->x2, b->y2,
	     r->x1, r->y1, r->x2, r->y2));
	if (r->x1 >= r->x2 || r->y1 >= r->y2)
		r->x1 = r->x2 = r->y1 = r->y2 = 0;
}

static int sna_box_area(const BoxRec *box)
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
		  const BoxRec *box,
		  xf86CrtcPtr desired,
		  BoxPtr crtc_box_ret)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	xf86CrtcPtr best_crtc;
	int best_coverage, c;
	BoxRec best_crtc_box;

	/* If we do not own the VT, we do not own the CRTC either */
	if (!scrn->vtSema)
		return NULL;

	DBG(("%s for box=(%d, %d), (%d, %d)\n",
	     __FUNCTION__, box->x1, box->y1, box->x2, box->y2));

	best_crtc = NULL;
	best_coverage = 0;
	best_crtc_box.x1 = 0;
	best_crtc_box.x2 = 0;
	best_crtc_box.y1 = 0;
	best_crtc_box.y2 = 0;
	for (c = 0; c < xf86_config->num_crtc; c++) {
		xf86CrtcPtr crtc = xf86_config->crtc[c];
		BoxRec crtc_box, cover_box;
		int coverage;

		/* If the CRTC is off, treat it as not covering */
		if (!sna_crtc_on(crtc)) {
			DBG(("%s: crtc %d off, skipping\n", __FUNCTION__, c));
			continue;
		}

		sna_crtc_box(crtc, &crtc_box);
		DBG(("%s: crtc %d: (%d, %d), (%d, %d)\n",
		     __FUNCTION__, c,
		     crtc_box.x1, crtc_box.y1,
		     crtc_box.x2, crtc_box.y2));

		sna_box_intersect(&cover_box, &crtc_box, box);
		DBG(("%s: box instersects (%d, %d), (%d, %d) of crtc %d\n",
		     __FUNCTION__,
		     cover_box.x1, cover_box.y1,
		     cover_box.x2, cover_box.y2,
		     c));
		coverage = sna_box_area(&cover_box);
		DBG(("%s: box covers %d of crtc %d\n",
		     __FUNCTION__, coverage, c));
		if (coverage && crtc == desired) {
			DBG(("%s: box is on desired crtc [%p]\n",
			     __FUNCTION__, crtc));
			*crtc_box_ret = crtc_box;
			return crtc;
		}
		if (coverage > best_coverage) {
			best_crtc_box = crtc_box;
			best_crtc = crtc;
			best_coverage = coverage;
		}
	}
	DBG(("%s: best crtc = %p, coverage = %d\n",
	     __FUNCTION__, best_crtc, best_coverage));
	*crtc_box_ret = best_crtc_box;
	return best_crtc;
}

/* Gen6 wait for scan line support */
#define MI_LOAD_REGISTER_IMM			(0x22<<23)

/* gen6: Scan lines register */
#define GEN6_PIPEA_SLC			(0x70004)
#define GEN6_PIPEB_SLC			(0x71004)

static void sna_emit_wait_for_scanline_gen6(struct sna *sna,
					    int pipe, int y1, int y2,
					    bool full_height)
{
	uint32_t event;
	uint32_t *b;

	assert (y2 > 0);

	/* We just wait until the trace passes the roi */
	if (pipe == 0) {
		pipe = GEN6_PIPEA_SLC;
		event = MI_WAIT_FOR_PIPEA_SCAN_LINE_WINDOW;
	} else {
		pipe = GEN6_PIPEB_SLC;
		event = MI_WAIT_FOR_PIPEB_SCAN_LINE_WINDOW;
	}

	kgem_set_mode(&sna->kgem, KGEM_RENDER);
	b = kgem_get_batch(&sna->kgem, 4);
	b[0] = MI_LOAD_REGISTER_IMM | 1;
	b[1] = pipe;
	b[2] = y2 - 1;
	b[3] = MI_WAIT_FOR_EVENT | event;
	kgem_advance_batch(&sna->kgem, 4);
}

static void sna_emit_wait_for_scanline_gen4(struct sna *sna,
					    int pipe, int y1, int y2,
					    bool full_height)
{
	uint32_t event;
	uint32_t *b;

	if (pipe == 0) {
		if (full_height)
			event = MI_WAIT_FOR_PIPEA_SVBLANK;
		else
			event = MI_WAIT_FOR_PIPEA_SCAN_LINE_WINDOW;
	} else {
		if (full_height)
			event = MI_WAIT_FOR_PIPEB_SVBLANK;
		else
			event = MI_WAIT_FOR_PIPEB_SCAN_LINE_WINDOW;
	}

	kgem_set_mode(&sna->kgem, KGEM_BLT);
	b = kgem_get_batch(&sna->kgem, 5);
	/* The documentation says that the LOAD_SCAN_LINES command
	 * always comes in pairs. Don't ask me why. */
	b[2] = b[0] = MI_LOAD_SCAN_LINES_INCL | pipe << 20;
	b[3] = b[1] = (y1 << 16) | (y2-1);
	b[4] = MI_WAIT_FOR_EVENT | event;
	kgem_advance_batch(&sna->kgem, 5);
}

static void sna_emit_wait_for_scanline_gen2(struct sna *sna,
					    int pipe, int y1, int y2,
					    bool full_height)
{
	uint32_t *b;

	/*
	 * Pre-965 doesn't have SVBLANK, so we need a bit
	 * of extra time for the blitter to start up and
	 * do its job for a full height blit
	 */
	if (full_height)
		y2 -= 2;

	kgem_set_mode(&sna->kgem, KGEM_BLT);
	b = kgem_get_batch(&sna->kgem, 5);
	/* The documentation says that the LOAD_SCAN_LINES command
	 * always comes in pairs. Don't ask me why. */
	b[2] = b[0] = MI_LOAD_SCAN_LINES_INCL | pipe << 20;
	b[3] = b[1] = (y1 << 16) | (y2-1);
	if (pipe == 0)
		b[4] = MI_WAIT_FOR_EVENT | MI_WAIT_FOR_PIPEA_SCAN_LINE_WINDOW;
	else
		b[4] = MI_WAIT_FOR_EVENT | MI_WAIT_FOR_PIPEB_SCAN_LINE_WINDOW;
	kgem_advance_batch(&sna->kgem, 5);
}

bool
sna_wait_for_scanline(struct sna *sna,
		      PixmapPtr pixmap,
		      xf86CrtcPtr crtc,
		      const BoxRec *clip)
{
	pixman_box16_t box, crtc_box;
	Bool full_height;
	int y1, y2, pipe;

	assert(crtc);
	assert(sna_crtc_on(crtc));
	assert(pixmap_is_scanout(pixmap));

	/* XXX WAIT_EVENT is still causing hangs on SNB */
	if (sna->kgem.gen >= 60)
		return false;

	sna_crtc_box(crtc, &crtc_box);
	if (crtc->transform_in_use) {
		box = *clip;
		pixman_f_transform_bounds(&crtc->f_framebuffer_to_crtc, &box);
		clip = &box;
	}

	/*
	 * Make sure we don't wait for a scanline that will
	 * never occur
	 */
	y1 = clip->y1 - crtc_box.y1;
	if (y1 < 0)
		y1 = 0;
	y2 = clip->y2 - crtc_box.y1;
	if (y2 > crtc_box.y2 - crtc_box.y1)
		y2 = crtc_box.y2 - crtc_box.y1;
	DBG(("%s: clipped range = %d, %d\n", __FUNCTION__, y1, y2));
	if (y2 <= y1)
		return false;

	full_height = y1 == 0 && y2 == crtc_box.y2 - crtc_box.y1;

	if (crtc->mode.Flags & V_INTERLACE) {
		/* DSL count field lines */
		y1 /= 2;
		y2 /= 2;
	}

	pipe = sna_crtc_to_pipe(crtc);
	DBG(("%s: pipe=%d, y1=%d, y2=%d, full_height?=%d\n",
	     __FUNCTION__, pipe, y1, y2, full_height));

	if (sna->kgem.gen >= 60)
		sna_emit_wait_for_scanline_gen6(sna, pipe, y1, y2, full_height);
	else if (sna->kgem.gen >= 40)
		sna_emit_wait_for_scanline_gen4(sna, pipe, y1, y2, full_height);
	else
		sna_emit_wait_for_scanline_gen2(sna, pipe, y1, y2, full_height);

	return true;
}

void sna_mode_update(struct sna *sna)
{
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(sna->scrn);
	int i;

	/* Validate CRTC attachments */
	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];
		struct sna_crtc *sna_crtc = to_sna_crtc(crtc);
		if (crtc->enabled)
			sna_crtc->active = sna_crtc_is_bound(sna, crtc);
		else
			sna_crtc->active = false;
	}
}
