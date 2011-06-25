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

#include "sna.h"
#include "sna_reg.h"

#if DEBUG_DISPLAY
#undef DBG
#define DBG(x) ErrorF x
#endif

struct sna_crtc {
	struct sna *sna;
	drmModeModeInfo kmode;
	PixmapPtr shadow;
	uint32_t shadow_fb_id;
	uint32_t cursor;
	xf86CrtcPtr crtc;
	int num;
	int id;
	int pipe;
	int active;
	struct list link;
};

struct sna_property {
	drmModePropertyPtr mode_prop;
	uint64_t value;
	int num_atoms; /* if range prop, num_atoms == 1; if enum prop, num_atoms == num_enums + 1 */
	Atom *atoms;
};

struct sna_output {
	struct sna_mode *mode;
	int output_id;
	drmModeConnectorPtr mode_output;
	drmModeEncoderPtr mode_encoder;
	int num_props;
	struct sna_property *props;
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
sna_output_dpms(xf86OutputPtr output, int mode);

#define BACKLIGHT_CLASS "/sys/class/backlight"

/*
 * List of available kernel interfaces in priority order
 */
static const char *backlight_interfaces[] = {
	"sna", /* prefer our own native backlight driver */
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
	return crtc_id(crtc->driver_private);
}

int sna_crtc_on(xf86CrtcPtr crtc)
{
	struct sna_crtc *sna_crtc = crtc->driver_private;
	return sna_crtc->active;
}

int sna_crtc_to_pipe(xf86CrtcPtr crtc)
{
	struct sna_crtc *sna_crtc = crtc->driver_private;
	return sna_crtc->pipe;
}

static uint32_t gem_create(int fd, int size)
{
	struct drm_i915_gem_create create;

	create.handle = 0;
	create.size = ALIGN(size, 4096);
	(void)drmIoctl(fd, DRM_IOCTL_I915_GEM_CREATE, &create);

	return create.handle;
}

static void gem_close(int fd, uint32_t handle)
{
	struct drm_gem_close close;

	close.handle = handle;
	(void)drmIoctl(fd, DRM_IOCTL_GEM_CLOSE, &close);
}

static void
sna_output_backlight_set(xf86OutputPtr output, int level)
{
	struct sna_output *sna_output = output->driver_private;
	char path[BACKLIGHT_PATH_LEN], val[BACKLIGHT_VALUE_LEN];
	int fd, len, ret;

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
	if (level > sna_output->backlight_max)
		level = sna_output->backlight_max;
	if (level < 0)
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

static Bool
sna_crtc_apply(xf86CrtcPtr crtc)
{
	ScrnInfoPtr scrn = crtc->scrn;
	struct sna *sna = to_sna(scrn);
	struct sna_crtc *sna_crtc = crtc->driver_private;
	struct sna_mode *mode = &sna->mode;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
	uint32_t output_ids[16];
	int output_count = 0;
	int fb_id, x, y;
	int i, ret = FALSE;

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

	DBG(("%s: applying crtc [%d] mode=%dx%d@%d, fb=%d%s update to %d outputs\n",
	     __FUNCTION__, crtc_id(sna_crtc),
	     sna_crtc->kmode.hdisplay,
	     sna_crtc->kmode.vdisplay,
	     sna_crtc->kmode.clock,
	     fb_id, sna_crtc->shadow_fb_id ? " [shadow]" : "",
	     output_count));

	ret = drmModeSetCrtc(sna->kgem.fd, crtc_id(sna_crtc),
			     fb_id, x, y, output_ids, output_count,
			     &sna_crtc->kmode);
	if (ret) {
		xf86DrvMsg(crtc->scrn->scrnIndex, X_ERROR,
			   "failed to set mode: %s\n", strerror(-ret));
		ret = FALSE;
	} else {
		sna_crtc->active = 1;
		ret = TRUE;
	}

	if (scrn->pScreen)
		xf86_reload_cursors(scrn->pScreen);

	return ret;
}

static void
sna_crtc_restore(struct sna *sna)
{
	ScrnInfoPtr scrn = sna->scrn;
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	struct kgem_bo *bo;
	int i;

	if (sna->mode.fb_pixmap == sna->front->drawable.serialNumber)
		return;

	bo = sna_pixmap_pin(sna->front);
	if (!bo)
		return;

	assert(bo->tiling != I915_TILING_Y);

	DBG(("%s: create fb %dx%d@%d/%d\n",
	     __FUNCTION__,
	     sna->front->drawable.width,
	     sna->front->drawable.height,
	     sna->front->drawable.depth,
	     sna->front->drawable.bitsPerPixel));

	sna_mode_remove_fb(sna);
	if (drmModeAddFB(sna->kgem.fd,
			 sna->front->drawable.width,
			 sna->front->drawable.height,
			 sna->front->drawable.depth,
			 sna->front->drawable.bitsPerPixel,
			 bo->pitch, bo->handle,
			 &sna->mode.fb_id))
		return;

	DBG(("%s: handle %d attached to fb %d\n",
	     __FUNCTION__, bo->handle, sna->mode.fb_id));

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		if (!crtc->enabled)
			continue;

		if (!sna_crtc_apply(crtc))
			return;
	}

	scrn->displayWidth = bo->pitch / sna->mode.cpp;
	sna->mode.fb_pixmap = sna->front->drawable.serialNumber;
}

static void
sna_crtc_dpms(xf86CrtcPtr crtc, int mode)
{
	struct sna_crtc *sna_crtc = crtc->driver_private;

	DBG(("%s(pipe %d, dpms mode -> %d):= active=%d\n",
	     __FUNCTION__, sna_crtc->pipe, mode, mode == DPMSModeOn));

	sna_crtc->active = mode == DPMSModeOn;
	if (mode == DPMSModeOn)
		sna_crtc_restore(sna_crtc->sna);
}

static struct kgem_bo *sna_create_bo_for_fbcon(struct sna *sna,
					       drmModeFBPtr fbcon)
{
	struct drm_gem_flink flink;
	struct kgem_bo *bo;
	int ret;

	/* Create a new reference for the fbcon so that we can track it
	 * using a normal bo and so that when we call gem_close on it we
	 * delete our reference and not fbcon's!
	 */
	memset(&flink, 0, sizeof(flink));
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
	drmModeFBPtr fbcon;
	PixmapPtr scratch;
	struct sna_pixmap *priv;
	struct kgem_bo *bo;
	BoxRec box;
	bool ok;
	int sx, sy;
	int dx, dy;
	int i;

	if (sna->kgem.wedged)
		return;

	/* Scan the connectors for a framebuffer and assume that is the fbcon */
	fbcon = NULL;
	for (i = 0; fbcon == NULL && i < xf86_config->num_crtc; i++) {
		struct sna_crtc *crtc = xf86_config->crtc[i]->driver_private;
		drmModeCrtcPtr mode_crtc;

		mode_crtc = drmModeGetCrtc(sna->kgem.fd,
					   sna->mode.mode_res->crtcs[crtc->num]);
		if (mode_crtc == NULL)
			continue;

		if (mode_crtc->buffer_id)
			fbcon = drmModeGetFB(sna->kgem.fd,
					     mode_crtc->buffer_id);
		drmModeFreeCrtc(mode_crtc);
	}
	if (fbcon == NULL)
		return;

	/* Wrap the fbcon in a pixmap so that we select the right formats
	 * in the render copy in case we need to preserve the fbcon
	 * across a depth change upon starting X.
	 */
	scratch = GetScratchPixmapHeader(sna->scrn->pScreen,
					fbcon->width, fbcon->height,
					fbcon->depth, fbcon->bpp,
					0, NULL);
	if (scratch == NullPixmap)
		goto cleanup_fbcon;

	box.x1 = box.y1 = 0;
	box.x2 = min(fbcon->width, sna->front->drawable.width);
	box.y2 = min(fbcon->height, sna->front->drawable.height);

	bo = sna_create_bo_for_fbcon(sna, fbcon);
	if (bo == NULL)
		goto cleanup_scratch;

	priv = sna_pixmap(sna->front);
	assert(priv && priv->gpu_bo);

	sx = dx = 0;
	if (box.x2 < fbcon->width)
		sx = (fbcon->width - box.x2) / 2.;
	if (box.x2 < sna->front->drawable.width)
		dx = (sna->front->drawable.width - box.x2) / 2.;

	sy = dy = 0;
	if (box.y2 < fbcon->height)
		sy = (fbcon->height - box.y2) / 2.;
	if (box.y2 < sna->front->drawable.height)
		dy = (sna->front->drawable.height - box.y2) / 2.;

	ok = sna->render.copy_boxes(sna, GXcopy,
				    scratch, bo, sx, sy,
				    sna->front, priv->gpu_bo, dx, dy,
				    &box, 1);
	sna_damage_add_box(&priv->gpu_damage, &box);

	kgem_bo_destroy(&sna->kgem, bo);

	sna->scrn->pScreen->canDoBGNoneRoot = ok;

cleanup_scratch:
	FreeScratchPixmapHeader(scratch);
cleanup_fbcon:
	drmModeFreeFB(fbcon);
}

static Bool
sna_crtc_set_mode_major(xf86CrtcPtr crtc, DisplayModePtr mode,
			Rotation rotation, int x, int y)
{
	ScrnInfoPtr scrn = crtc->scrn;
	struct sna *sna = to_sna(scrn);
	struct sna_crtc *sna_crtc = crtc->driver_private;
	struct sna_mode *sna_mode = &sna->mode;
	int saved_x, saved_y;
	Rotation saved_rotation;
	DisplayModeRec saved_mode;
	int ret = TRUE;

	DBG(("%s(rotation=%d, x=%d, y=%d, mode=%dx%d@%d)\n",
	     __FUNCTION__, rotation, x, y,
	     mode->HDisplay, mode->VDisplay, mode->Clock));

	if (sna_mode->fb_pixmap != sna->front->drawable.serialNumber)
		sna_mode_remove_fb(sna);

	if (sna_mode->fb_id == 0) {
		struct kgem_bo *bo = sna_pixmap_pin(sna->front);
		if (!bo)
			return FALSE;

		DBG(("%s: create fb %dx%d@%d/%d\n",
		     __FUNCTION__,
		     scrn->virtualX, scrn->virtualY,
		     scrn->depth, scrn->bitsPerPixel));

		assert(bo->tiling != I915_TILING_Y);
		ret = drmModeAddFB(sna->kgem.fd,
				   scrn->virtualX, scrn->virtualY,
				   scrn->depth, scrn->bitsPerPixel,
				   bo->pitch, bo->handle,
				   &sna_mode->fb_id);
		if (ret < 0) {
			ErrorF("%s: failed to add fb: %dx%d depth=%d, bpp=%d, pitch=%d\n",
			       __FUNCTION__,
			       scrn->virtualX, scrn->virtualY,
			       scrn->depth, scrn->bitsPerPixel, bo->pitch);
			return FALSE;
		}

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

	kgem_submit(&sna->kgem);

	mode_to_kmode(scrn, &sna_crtc->kmode, mode);
	ret = sna_crtc_apply(crtc);
	if (!ret) {
		crtc->x = saved_x;
		crtc->y = saved_y;
		crtc->rotation = saved_rotation;
		crtc->mode = saved_mode;
	}

	return ret;
}

static void
sna_crtc_set_cursor_colors(xf86CrtcPtr crtc, int bg, int fg)
{

}

static void
sna_crtc_set_cursor_position (xf86CrtcPtr crtc, int x, int y)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = crtc->driver_private;

	drmModeMoveCursor(sna->kgem.fd, crtc_id(sna_crtc), x, y);
}

static void
sna_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *image)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = crtc->driver_private;
	struct drm_i915_gem_pwrite pwrite;

	pwrite.handle = sna_crtc->cursor;
	pwrite.offset = 0;
	pwrite.size = 64*64*4;
	pwrite.data_ptr = (uintptr_t)image;
	(void)drmIoctl(sna->kgem.fd, DRM_IOCTL_I915_GEM_PWRITE, &pwrite);
}

static void
sna_crtc_hide_cursor(xf86CrtcPtr crtc)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = crtc->driver_private;

	drmModeSetCursor(sna->kgem.fd, crtc_id(sna_crtc), 0, 64, 64);
}

static void
sna_crtc_show_cursor(xf86CrtcPtr crtc)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = crtc->driver_private;

	drmModeSetCursor(sna->kgem.fd, crtc_id(sna_crtc),
			 sna_crtc->cursor, 64, 64);
}

static void *
sna_crtc_shadow_allocate(xf86CrtcPtr crtc, int width, int height)
{
	ScrnInfoPtr scrn = crtc->scrn;
	struct sna *sna = to_sna(scrn);
	struct sna_crtc *sna_crtc = crtc->driver_private;
	PixmapPtr shadow;
	struct kgem_bo *bo;

	DBG(("%s(%d, %d)\n", __FUNCTION__, width, height));

	shadow = scrn->pScreen->CreatePixmap(scrn->pScreen, width, height, scrn->depth, 0);
	if (!shadow)
		return NULL;

	bo = sna_pixmap_pin(shadow);
	if (!bo) {
		scrn->pScreen->DestroyPixmap(shadow);
		return NULL;
	}

	assert(bo->tiling != I915_TILING_Y);
	if (drmModeAddFB(sna->kgem.fd,
			 width, height, scrn->depth, scrn->bitsPerPixel,
			 bo->pitch, bo->handle,
			 &sna_crtc->shadow_fb_id)) {
		ErrorF("%s: failed to add rotate  fb: %dx%d depth=%d, bpp=%d, pitch=%d\n",
		       __FUNCTION__,
		       width, height,
		       scrn->depth, scrn->bitsPerPixel, bo->pitch);
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
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = crtc->driver_private;

	DBG(("%s(fb=%d, handle=%d)\n", __FUNCTION__,
	     sna_crtc->shadow_fb_id, sna_pixmap_get_bo(pixmap)->handle));

	drmModeRmFB(sna->kgem.fd, sna_crtc->shadow_fb_id);
	sna_crtc->shadow_fb_id = 0;

	pixmap->drawable.pScreen->DestroyPixmap(pixmap);
	sna_crtc->shadow = NULL;
}

static void
sna_crtc_gamma_set(xf86CrtcPtr crtc,
		       CARD16 *red, CARD16 *green, CARD16 *blue, int size)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = crtc->driver_private;

	drmModeCrtcSetGamma(sna->kgem.fd, crtc_id(sna_crtc),
			    size, red, green, blue);
}

static void
sna_crtc_destroy(xf86CrtcPtr crtc)
{
	struct sna *sna = to_sna(crtc->scrn);
	struct sna_crtc *sna_crtc = crtc->driver_private;

	drmModeSetCursor(sna->kgem.fd, crtc_id(sna_crtc), 0, 64, 64);
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

static void
sna_crtc_init(ScrnInfoPtr scrn, struct sna_mode *mode, int num)
{
	struct sna *sna = to_sna(scrn);
	drmModeCrtcPtr mode_crtc;
	xf86CrtcPtr crtc;
	struct sna_crtc *sna_crtc;
	struct drm_i915_get_pipe_from_crtc_id get_pipe;

	sna_crtc = calloc(sizeof(struct sna_crtc), 1);
	if (sna_crtc == NULL)
		return;

	sna_crtc->num = num;

	mode_crtc = drmModeGetCrtc(sna->kgem.fd, mode->mode_res->crtcs[num]);
	sna_crtc->id = mode_crtc->crtc_id;
	drmModeFreeCrtc(mode_crtc);

	get_pipe.pipe = 0;
	get_pipe.crtc_id = sna_crtc->id;
	drmIoctl(sna->kgem.fd,
		 DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID,
		 &get_pipe);
	sna_crtc->pipe = get_pipe.pipe;

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

	sna_crtc->sna = sna;
	sna_crtc->crtc = crtc;
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

	drmModeFreeConnector(sna_output->mode_output);
	sna_output->mode_output =
		drmModeGetConnector(sna->kgem.fd, sna_output->output_id);

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
	drmModePropertyBlobPtr edid_blob = NULL;
	xf86MonPtr mon = NULL;
	int i;

	/* look for an EDID property */
	for (i = 0; i < koutput->count_props; i++) {
		drmModePropertyPtr props;

		props = drmModeGetProperty(sna->kgem.fd, koutput->props[i]);
		if (!props)
			continue;

		if (!(props->flags & DRM_MODE_PROP_BLOB)) {
			drmModeFreeProperty(props);
			continue;
		}

		if (!strcmp(props->name, "EDID")) {
			drmModeFreePropertyBlob(edid_blob);
			edid_blob =
				drmModeGetPropertyBlob(sna->kgem.fd,
						       koutput->prop_values[i]);
		}
		drmModeFreeProperty(props);
	}

	if (edid_blob) {
		mon = xf86InterpretEDID(output->scrn->scrnIndex,
					edid_blob->data);

		if (mon && edid_blob->length > 128)
			mon->flags |= MONITOR_EDID_COMPLETE_RAWDATA;
	}

	xf86OutputSetEDID(output, mon);

	if (0&&edid_blob)
		drmModeFreePropertyBlob(edid_blob);
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

	for (i = 0; i < koutput->count_props; i++) {
		drmModePropertyPtr props;

		props = drmModeGetProperty(sna->kgem.fd, koutput->props[i]);
		if (!props)
			continue;

		if (!strcmp(props->name, "DPMS")) {
			drmModeConnectorSetProperty(sna->kgem.fd,
						    sna_output->output_id,
						    props->prop_id,
						    dpms);
			sna_output_dpms_backlight(output,
						      sna_output->dpms_mode,
						      dpms);
			sna_output->dpms_mode = dpms;
			drmModeFreeProperty(props);
			return;
		}

		drmModeFreeProperty(props);
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

	if (sna_output->backlight_iface) {
		INT32 data, backlight_range[2];

		/* Set up the backlight property, which takes effect
		 * immediately and accepts values only within the
		 * backlight_range.
		 */
		backlight_atom = MakeAtom(BACKLIGHT_NAME, sizeof(BACKLIGHT_NAME) - 1, TRUE);
		backlight_deprecated_atom = MakeAtom(BACKLIGHT_DEPRECATED_NAME,
						     sizeof(BACKLIGHT_DEPRECATED_NAME) - 1, TRUE);

		backlight_range[0] = 0;
		backlight_range[1] = sna_output->backlight_max;
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
		data = sna_output->backlight_active_level;
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

			drmModeConnectorSetProperty(sna->kgem.fd, sna_output->output_id,
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
					drmModeConnectorSetProperty(sna->kgem.fd, sna_output->output_id,
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
sna_zaphod_match(ScrnInfoPtr scrn, const char *s, const char *output)
{
	char t[20];
	int i = 0;

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
	drmModeEncoderPtr kencoder;
	struct sna_output *sna_output;
	const char *output_name;
	const char *s;
	char name[32];

	koutput = drmModeGetConnector(sna->kgem.fd,
				      mode->mode_res->connectors[num]);
	if (!koutput)
		return;

	kencoder = drmModeGetEncoder(sna->kgem.fd, koutput->encoders[0]);
	if (!kencoder)
		goto cleanup_connector;

	if (koutput->connector_type < ARRAY_SIZE(output_names))
		output_name = output_names[koutput->connector_type];
	else
		output_name = "UNKNOWN";
	snprintf(name, 32, "%s%d", output_name, koutput->connector_type_id);

	if (xf86IsEntityShared(scrn->entityList[0])) {
		s = xf86GetOptValString(sna->Options, OPTION_ZAPHOD);
		if (s && !sna_zaphod_match(scrn, s, name))
			goto cleanup_encoder;
	}

	output = xf86OutputCreate(scrn, &sna_output_funcs, name);
	if (!output)
		goto cleanup_encoder;

	sna_output = calloc(sizeof(struct sna_output), 1);
	if (!sna_output)
		goto cleanup_output;

	sna_output->output_id = mode->mode_res->connectors[num];
	sna_output->mode_output = koutput;
	sna_output->mode_encoder = kencoder;
	sna_output->mode = mode;

	output->mm_width = koutput->mmWidth;
	output->mm_height = koutput->mmHeight;

	output->subpixel_order = subpixel_conv_table[koutput->subpixel];
	output->driver_private = sna_output;

	if (is_panel(koutput->connector_type))
		sna_output_backlight_init(output);

	output->possible_crtcs = kencoder->possible_crtcs;
	output->possible_clones = kencoder->possible_clones;
	output->interlaceAllowed = TRUE;

	sna_output->output = output;
	list_add(&sna_output->link, &mode->outputs);

	return;

cleanup_output:
	xf86OutputDestroy(output);
cleanup_connector:
	drmModeFreeConnector(koutput);
cleanup_encoder:
	drmModeFreeEncoder(kencoder);
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

	assert(bo->tiling != I915_TILING_Y);
	if (drmModeAddFB(sna->kgem.fd, width, height,
			 scrn->depth, scrn->bitsPerPixel,
			 bo->pitch, bo->handle,
			 &mode->fb_id)) {
		ErrorF("%s: failed to add fb: %dx%d depth=%d, bpp=%d, pitch=%d\n",
		       __FUNCTION__,
		       width, height,
		       scrn->depth, scrn->bitsPerPixel, bo->pitch);
		goto fail;
	}

	DBG(("%s: handle %d attached to fb %d\n",
	     __FUNCTION__, bo->handle, mode->fb_id));

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		if (!crtc->enabled)
			continue;

		if (!sna_crtc_apply(crtc))
			goto fail;
	}

	scrn->virtualX = width;
	scrn->virtualY = height;
	scrn->displayWidth = bo->pitch / sna->mode.cpp;

	sna->mode.fb_pixmap = sna->front->drawable.serialNumber;
	sna_redirect_screen_pixmap(scrn, old_front, sna->front);
	assert(scrn->pScreen->GetScreenPixmap(scrn->pScreen) == sna->front);
	assert(scrn->pScreen->GetWindowPixmap(scrn->pScreen->root) == sna->front);

	if (old_fb_id)
		drmModeRmFB(sna->kgem.fd, old_fb_id);
	scrn->pScreen->DestroyPixmap(old_front);

	return TRUE;

fail:
	DBG(("%s: restoring original front pixmap and fb\n", __FUNCTION__));
	if (old_fb_id != mode->fb_id)
		drmModeRmFB(sna->kgem.fd, mode->fb_id);
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
		evdata |= sna_crtc_to_pipe(crtc->crtc) == ref_crtc_hw_id;

		DBG(("%s: crtc %d [ref? %d] --> fb %d\n",
		     __FUNCTION__, crtc_id(crtc),
		     sna_crtc_to_pipe(crtc->crtc) == ref_crtc_hw_id,
		     sna->mode.fb_id));
		if (drmModePageFlip(sna->kgem.fd,
				    crtc_id(crtc),
				    sna->mode.fb_id,
				    DRM_MODE_PAGE_FLIP_EVENT,
				    (void*)evdata)) {
			int err = errno;
			DBG(("%s: flip [fb=%d] on crtc %d [%d] failed - %d\n",
			     __FUNCTION__, sna->mode.fb_id,
			     i, crtc_id(crtc), err));
			xf86DrvMsg(sna->scrn->scrnIndex, X_WARNING,
				   "flip queue failed: %s\n", strerror(err));
			continue;
		}

		count++;
	}

	return count;
}

PixmapPtr sna_set_screen_pixmap(struct sna *sna, PixmapPtr pixmap)
{
	PixmapPtr old = sna->front;
	ScrnInfoPtr scrn = sna->scrn;

	assert(sna->front != pixmap);

	sna->front = pixmap;
	pixmap->refcnt++;

	sna_redirect_screen_pixmap(scrn, old, pixmap);
	scrn->displayWidth = sna_pixmap_get_bo(pixmap)->pitch / sna->mode.cpp;

	return old;
}

int
sna_do_pageflip(struct sna *sna,
		PixmapPtr pixmap,
		void *data,
		int ref_crtc_hw_id,
		uint32_t *old_fb)
{
	ScrnInfoPtr scrn = sna->scrn;
	struct sna_mode *mode = &sna->mode;
	struct kgem_bo *bo;
	int count;

	bo = sna_pixmap_pin(pixmap);
	if (!bo)
		return 0;

	*old_fb = mode->fb_id;

	/*
	 * Create a new handle for the back buffer
	 */
	assert(bo->tiling != I915_TILING_Y);
	if (drmModeAddFB(sna->kgem.fd, scrn->virtualX, scrn->virtualY,
			 scrn->depth, scrn->bitsPerPixel,
			 bo->pitch, bo->handle,
			 &mode->fb_id)) {
		ErrorF("%s: failed to add fb: %dx%d depth=%d, bpp=%d, pitch=%d\n",
		       __FUNCTION__,
		       scrn->virtualX, scrn->virtualY,
		       scrn->depth, scrn->bitsPerPixel, bo->pitch);
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
	if (count) {
		sna->mode.fb_pixmap = pixmap->drawable.serialNumber;
		bo->cpu_read = bo->cpu_write = false;
		bo->gpu = true;
		bo->needs_flush = true;
	} else {
		drmModeRmFB(sna->kgem.fd, mode->fb_id);
		mode->fb_id = *old_fb;
	}

	return count;
}

void sna_mode_delete_fb(struct sna *sna, PixmapPtr pixmap, uint32_t fb)
{
	if (fb)
		drmModeRmFB(sna->kgem.fd, fb);

	if (pixmap)
		pixmap->drawable.pScreen->DestroyPixmap(pixmap);
}

static const xf86CrtcConfigFuncsRec sna_crtc_config_funcs = {
	sna_crtc_resize
};

Bool sna_mode_pre_init(ScrnInfoPtr scrn, struct sna *sna)
{
	struct sna_mode *mode = &sna->mode;
	unsigned int i;

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

	if (mode->fb_id) {
		drmModeRmFB(sna->kgem.fd, mode->fb_id);
		mode->fb_id = 0;
		mode->fb_pixmap = 0;
	}
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
#define GEN6_PIPEA_SLC			(0x7004)
#define GEN6_PIPEB_SLC			(0x7104)

static void sna_emit_wait_for_scanline_gen6(struct sna *sna,
					    int pipe, int y1, int y2,
					    bool full_height)
{
	uint32_t event;
	uint32_t *b;

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

static void sna_emit_wait_for_scanline_gen2(struct sna *sna,
					    int pipe, int y1, int y2,
					    bool full_height)
{
	uint32_t event;
	uint32_t *b;

	/*
	 * Pre-965 doesn't have SVBLANK, so we need a bit
	 * of extra time for the blitter to start up and
	 * do its job for a full height blit
	 */
	if (pipe == 0) {
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

	if (sna->kgem.mode == KGEM_NONE)
		kgem_set_mode(&sna->kgem, KGEM_BLT);

	b = kgem_get_batch(&sna->kgem, 5);
	/* The documentation says that the LOAD_SCAN_LINES command
	 * always comes in pairs. Don't ask me why. */
	b[0] = MI_LOAD_SCAN_LINES_INCL | pipe;
	b[1] = (y1 << 16) | (y2-1);
	b[2] = MI_LOAD_SCAN_LINES_INCL | pipe;
	b[3] = (y1 << 16) | (y2-1);
	b[4] = MI_WAIT_FOR_EVENT | event;
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

	if (sna->kgem.gen >= 60)
		return false;

	if (!pixmap_is_scanout(pixmap))
		return false;

	if (crtc == NULL) {
		crtc = sna_covering_crtc(sna->scrn, clip, NULL, &crtc_box);
		if (crtc == NULL)
			return false;
	} else
		sna_crtc_box(crtc, &crtc_box);
	assert(sna_crtc_on(crtc));

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
	else
		sna_emit_wait_for_scanline_gen2(sna, pipe, y1, y2, full_height);

	return true;
}
