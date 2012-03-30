/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright © 2002 David Dawes

All Rights Reserved.

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
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *   David Dawes <dawes@xfree86.org>
 *
 */

#ifndef _SNA_H_
#define _SNA_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>

#include "compiler.h"

#include <xf86_OSproc.h>
#include <xf86Pci.h>
#include <xf86Cursor.h>
#include <xf86xv.h>
#include <xf86Crtc.h>
#include <xf86RandR12.h>
#include <gcstruct.h>

#include <xorg-server.h>
#include <pciaccess.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#define _XF86DRI_SERVER_
#include <dri2.h>
#include <i915_drm.h>

#if HAVE_UDEV
#include <libudev.h>
#endif

#include "compiler.h"

#define DBG(x)

#define DEBUG_ALL (HAS_DEBUG_FULL || 0)
#define DEBUG_ACCEL (DEBUG_ALL || 0)
#define DEBUG_BATCH (DEBUG_ALL || 0)
#define DEBUG_BLT (DEBUG_ALL || 0)
#define DEBUG_COMPOSITE (DEBUG_ALL || 0)
#define DEBUG_DAMAGE (DEBUG_ALL || 0)
#define DEBUG_DISPLAY (DEBUG_ALL || 0)
#define DEBUG_DRI (DEBUG_ALL || 0)
#define DEBUG_DRIVER (DEBUG_ALL || 0)
#define DEBUG_GRADIENT (DEBUG_ALL || 0)
#define DEBUG_GLYPHS (DEBUG_ALL || 0)
#define DEBUG_IO (DEBUG_ALL || 0)
#define DEBUG_KGEM (DEBUG_ALL || 0)
#define DEBUG_RENDER (DEBUG_ALL || 0)
#define DEBUG_STREAM (DEBUG_ALL || 0)
#define DEBUG_TRAPEZOIDS (DEBUG_ALL || 0)
#define DEBUG_VIDEO (DEBUG_ALL || 0)
#define DEBUG_VIDEO_TEXTURED (DEBUG_ALL || 0)
#define DEBUG_VIDEO_OVERLAY (DEBUG_ALL || 0)

#define DEBUG_NO_RENDER 0
#define DEBUG_NO_BLT 0

#define DEBUG_FLUSH_BATCH 0
#define DEBUG_FLUSH_SYNC 0

#define TEST_ALL 0
#define TEST_ACCEL (TEST_ALL || 0)
#define TEST_BATCH (TEST_ALL || 0)
#define TEST_BLT (TEST_ALL || 0)
#define TEST_COMPOSITE (TEST_ALL || 0)
#define TEST_DAMAGE (TEST_ALL || 0)
#define TEST_GRADIENT (TEST_ALL || 0)
#define TEST_GLYPHS (TEST_ALL || 0)
#define TEST_IO (TEST_ALL || 0)
#define TEST_KGEM (TEST_ALL || 0)
#define TEST_RENDER (TEST_ALL || 0)

#include "intel_driver.h"
#include "intel_list.h"
#include "kgem.h"
#include "sna_damage.h"
#include "sna_render.h"

#define SNA_CURSOR_X			64
#define SNA_CURSOR_Y			SNA_CURSOR_X

struct sna_pixmap {
	PixmapPtr pixmap;
	struct kgem_bo *gpu_bo, *cpu_bo;
	struct sna_damage *gpu_damage, *cpu_damage;
	void *ptr;

	struct list list;
	struct list inactive;

	uint32_t stride;
	uint32_t clear_color;
	unsigned flush;

#define SOURCE_BIAS 4
	uint16_t source_count;
	uint8_t pinned :1;
	uint8_t mapped :1;
	uint8_t clear :1;
	uint8_t undamaged :1;
	uint8_t create :3;
	uint8_t header :1;
};

struct sna_glyph {
	PicturePtr atlas;
	pixman_image_t *image;
	struct sna_coordinate coordinate;
	uint16_t size, pos;
};

extern DevPrivateKeyRec sna_private_index;
extern DevPrivateKeyRec sna_pixmap_index;
extern DevPrivateKeyRec sna_gc_index;
extern DevPrivateKeyRec sna_glyph_key;

static inline PixmapPtr get_window_pixmap(WindowPtr window)
{
#if 0
	return window->drawable.pScreen->GetWindowPixmap(window)
#else
	return *(void **)window->devPrivates;
#endif
}

static inline PixmapPtr get_drawable_pixmap(DrawablePtr drawable)
{
	if (drawable->type == DRAWABLE_PIXMAP)
		return (PixmapPtr)drawable;
	else
		return get_window_pixmap((WindowPtr)drawable);
}

constant static inline struct sna_pixmap *sna_pixmap(PixmapPtr pixmap)
{
	return ((void **)pixmap->devPrivates)[1];
}

static inline struct sna_pixmap *sna_pixmap_from_drawable(DrawablePtr drawable)
{
	return sna_pixmap(get_drawable_pixmap(drawable));
}

struct sna_gc {
	long changes;
	long serial;
	void *priv;
};

static inline struct sna_gc *sna_gc(GCPtr gc)
{
	return (struct sna_gc *)gc->devPrivates;
}

enum {
	OPTION_TILING_FB,
	OPTION_TILING_2D,
	OPTION_PREFER_OVERLAY,
	OPTION_COLOR_KEY,
	OPTION_VIDEO_KEY,
	OPTION_HOTPLUG,
	OPTION_THROTTLE,
	OPTION_RELAXED_FENCING,
	OPTION_VMAP,
	OPTION_ZAPHOD,
	OPTION_DELAYED_FLUSH,
	NUM_OPTIONS
};

enum {
	FLUSH_TIMER = 0,
	EXPIRE_TIMER,
	INACTIVE_TIMER,
	NUM_TIMERS
};
#define NUM_FINE_TIMERS 1

struct sna {
	ScrnInfoPtr scrn;

	unsigned flags;
#define SNA_NO_THROTTLE		0x1
#define SNA_NO_DELAYED_FLUSH	0x2

	unsigned watch_flush;
	unsigned flush;

	int timer[NUM_TIMERS];
	uint16_t timer_active;
	uint16_t timer_ready;

	int vblank_interval;

	struct list dirty_pixmaps;
	struct list active_pixmaps;
	struct list inactive_clock[2];

	PixmapPtr front, shadow;
	PixmapPtr freed_pixmap;

	struct sna_mode {
		uint32_t fb_id;
		uint32_t fb_pixmap;
		drmModeResPtr mode_res;
		int cpp;

		struct list outputs;
		struct list crtcs;
	} mode;

	struct sna_dri {
		void *flip_pending[2];
	} dri;

	unsigned int tiling;
#define SNA_TILING_FB		0x1
#define SNA_TILING_2D		0x2
#define SNA_TILING_3D		0x4
#define SNA_TILING_ALL (~0)

	EntityInfoPtr pEnt;
	struct pci_device *PciInfo;
	struct intel_chipset chipset;

	ScreenBlockHandlerProcPtr BlockHandler;
	void *BlockData;
	ScreenWakeupHandlerProcPtr WakeupHandler;
	void *WakeupData;
	CloseScreenProcPtr CloseScreen;

	PicturePtr clear;
	struct {
		uint32_t fill_bo;
		uint32_t fill_pixel;
		uint32_t fill_alu;
	} blt_state;
	union {
		struct gen2_render_state gen2;
		struct gen3_render_state gen3;
		struct gen4_render_state gen4;
		struct gen5_render_state gen5;
		struct gen6_render_state gen6;
		struct gen7_render_state gen7;
	} render_state;
	uint32_t have_render;
	uint32_t default_tiling;

	Bool directRenderingOpen;
	char *deviceName;

	/* Broken-out options. */
	OptionInfoPtr Options;

	/* Driver phase/state information */
	Bool suspended;

#if HAVE_UDEV
	struct udev_monitor *uevent_monitor;
	InputHandlerProc uevent_handler;
#endif

	struct kgem kgem;
	struct sna_render render;
};

Bool sna_mode_pre_init(ScrnInfoPtr scrn, struct sna *sna);
extern void sna_mode_remove_fb(struct sna *sna);
extern void sna_mode_fini(struct sna *sna);

extern int sna_crtc_id(xf86CrtcPtr crtc);
extern int sna_output_dpms_status(xf86OutputPtr output);

extern int sna_page_flip(struct sna *sna,
			 struct kgem_bo *bo,
			 void *data,
			 int ref_crtc_hw_id,
			 uint32_t *old_fb);

extern PixmapPtr sna_set_screen_pixmap(struct sna *sna, PixmapPtr pixmap);

void sna_mode_delete_fb(struct sna *sna, uint32_t fb);

constant static inline struct sna *
to_sna(ScrnInfoPtr scrn)
{
	return (struct sna *)(scrn->driverPrivate);
}

constant static inline struct sna *
to_sna_from_screen(ScreenPtr screen)
{
	return to_sna(xf86Screens[screen->myNum]);
}

constant static inline struct sna *
to_sna_from_pixmap(PixmapPtr pixmap)
{
	return *(void **)pixmap->devPrivates;
}

constant static inline struct sna *
to_sna_from_drawable(DrawablePtr drawable)
{
	return to_sna_from_screen(drawable->pScreen);
}

static inline struct sna *
to_sna_from_kgem(struct kgem *kgem)
{
	return container_of(kgem, struct sna, kgem);
}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif
#define ALIGN(i,m)	(((i) + (m) - 1) & ~((m) - 1))
#define MIN(a,b)	((a) <= (b) ? (a) : (b))
#define MAX(a,b)	((a) >= (b) ? (a) : (b))

extern xf86CrtcPtr sna_covering_crtc(ScrnInfoPtr scrn,
				     const BoxRec *box,
				     xf86CrtcPtr desired,
				     BoxPtr crtc_box_ret);

extern bool sna_wait_for_scanline(struct sna *sna, PixmapPtr pixmap,
				  xf86CrtcPtr crtc, const BoxRec *clip);
extern bool sna_crtc_is_bound(struct sna *sna, xf86CrtcPtr crtc);

Bool sna_dri_open(struct sna *sna, ScreenPtr pScreen);
void sna_dri_wakeup(struct sna *sna);
void sna_dri_close(struct sna *sna, ScreenPtr pScreen);

extern Bool sna_crtc_on(xf86CrtcPtr crtc);
int sna_crtc_to_pipe(xf86CrtcPtr crtc);
int sna_crtc_to_plane(xf86CrtcPtr crtc);

CARD32 sna_format_for_depth(int depth);
CARD32 sna_render_format_for_depth(int depth);

void sna_debug_flush(struct sna *sna);

static inline void
get_drawable_deltas(DrawablePtr drawable, PixmapPtr pixmap, int16_t *x, int16_t *y)
{
#ifdef COMPOSITE
	if (drawable->type == DRAWABLE_WINDOW) {
		*x = -pixmap->screen_x;
		*y = -pixmap->screen_y;
		return;
	}
#endif
	*x = *y = 0;
}

static inline int
get_drawable_dx(DrawablePtr drawable)
{
#ifdef COMPOSITE
	if (drawable->type == DRAWABLE_WINDOW)
		return -get_drawable_pixmap(drawable)->screen_x;
#endif
	return 0;
}

static inline int
get_drawable_dy(DrawablePtr drawable)
{
#ifdef COMPOSITE
	if (drawable->type == DRAWABLE_WINDOW)
		return -get_drawable_pixmap(drawable)->screen_y;
#endif
	return 0;
}

static inline Bool pixmap_is_scanout(PixmapPtr pixmap)
{
	ScreenPtr screen = pixmap->drawable.pScreen;
	return pixmap == screen->GetScreenPixmap(screen);
}

PixmapPtr sna_pixmap_create_upload(ScreenPtr screen,
				   int width, int height, int depth,
				   unsigned flags);
PixmapPtr sna_pixmap_create_unattached(ScreenPtr screen,
				       int width, int height, int depth);

struct sna_pixmap *sna_pixmap_move_to_gpu(PixmapPtr pixmap, unsigned flags);
struct sna_pixmap *sna_pixmap_force_to_gpu(PixmapPtr pixmap, unsigned flags);
struct kgem_bo *sna_pixmap_change_tiling(PixmapPtr pixmap, uint32_t tiling);

#define MOVE_WRITE 0x1
#define MOVE_READ 0x2
#define MOVE_INPLACE_HINT 0x4
#define MOVE_ASYNC_HINT 0x8
bool must_check _sna_pixmap_move_to_cpu(PixmapPtr pixmap, unsigned flags);
static inline bool must_check sna_pixmap_move_to_cpu(PixmapPtr pixmap, unsigned flags)
{
	if (flags == MOVE_READ) {
		struct sna_pixmap *priv = sna_pixmap(pixmap);
		if (priv == NULL)
			return true;
	}

	return _sna_pixmap_move_to_cpu(pixmap, flags);
}
bool must_check sna_drawable_move_region_to_cpu(DrawablePtr drawable,
						RegionPtr region,
						unsigned flags);

static inline bool must_check
sna_drawable_move_to_cpu(DrawablePtr drawable, unsigned flags)
{
	RegionRec region;

	pixman_region_init_rect(&region,
				drawable->x, drawable->y,
				drawable->width, drawable->height);
	return sna_drawable_move_region_to_cpu(drawable, &region, flags);
}

static inline bool must_check
sna_drawable_move_to_gpu(DrawablePtr drawable, unsigned flags)
{
	return sna_pixmap_move_to_gpu(get_drawable_pixmap(drawable), flags) != NULL;
}

static inline bool
sna_drawable_is_clear(DrawablePtr d)
{
	struct sna_pixmap *priv = sna_pixmap(get_drawable_pixmap(d));
	return priv && priv->clear && priv->clear_color == 0;
}

static inline struct kgem_bo *sna_pixmap_get_bo(PixmapPtr pixmap)
{
	return sna_pixmap(pixmap)->gpu_bo;
}

static inline struct kgem_bo *sna_pixmap_pin(PixmapPtr pixmap)
{
	struct sna_pixmap *priv;

	priv = sna_pixmap_force_to_gpu(pixmap, MOVE_READ | MOVE_WRITE);
	if (!priv)
		return NULL;

	priv->pinned = 1;
	return priv->gpu_bo;
}


static inline Bool
_sna_transform_point(const PictTransform *transform,
		     int64_t x, int64_t y, int64_t result[3])
{
	int j;

	for (j = 0; j < 3; j++)
		result[j] = (transform->matrix[j][0] * x +
			     transform->matrix[j][1] * y +
			     transform->matrix[j][2]);

	return result[2] != 0;
}

static inline void
_sna_get_transformed_coordinates(int x, int y,
				 const PictTransform *transform,
				 float *x_out, float *y_out)
{

	int64_t result[3];

	_sna_transform_point(transform, x, y, result);
	*x_out = result[0] / (double)result[2];
	*y_out = result[1] / (double)result[2];
}

void
sna_get_transformed_coordinates(int x, int y,
				const PictTransform *transform,
				float *x_out, float *y_out);

Bool
sna_get_transformed_coordinates_3d(int x, int y,
				   const PictTransform *transform,
				   float *x_out, float *y_out, float *z_out);

Bool sna_transform_is_affine(const PictTransform *t);
Bool sna_transform_is_integer_translation(const PictTransform *t,
					  int16_t *tx, int16_t *ty);
Bool sna_transform_is_translation(const PictTransform *t,
				  pixman_fixed_t *tx, pixman_fixed_t *ty);

static inline bool
sna_transform_equal(const PictTransform *a, const PictTransform *b)
{
	if (a == b)
		return true;

	if (a == NULL || b == NULL)
		return false;

	return memcmp(a, b, sizeof(*a)) == 0;
}

static inline bool
sna_picture_alphamap_equal(PicturePtr a, PicturePtr b)
{
	if (a->alphaMap != b->alphaMap)
		return false;

	if (a->alphaMap)
		return false;

	return (a->alphaOrigin.x == b->alphaOrigin.x &&
		a->alphaOrigin.y == b->alphaOrigin.y);
}

static inline bool wedged(struct sna *sna)
{
	return unlikely(sna->kgem.wedged);
}

static inline uint32_t pixmap_size(PixmapPtr pixmap)
{
	return (pixmap->drawable.height - 1) * pixmap->devKind +
		pixmap->drawable.width * pixmap->drawable.bitsPerPixel/8;
}

Bool sna_accel_pre_init(struct sna *sna);
Bool sna_accel_init(ScreenPtr sreen, struct sna *sna);
void sna_accel_block_handler(struct sna *sna);
void sna_accel_wakeup_handler(struct sna *sna, fd_set *ready);
void sna_accel_watch_flush(struct sna *sna, int enable);
void sna_accel_close(struct sna *sna);
void sna_accel_free(struct sna *sna);

Bool sna_accel_create(struct sna *sna);
void sna_copy_fbcon(struct sna *sna);

Bool sna_composite_create(struct sna *sna);
void sna_composite_close(struct sna *sna);

void sna_composite(CARD8 op,
		   PicturePtr src,
		   PicturePtr mask,
		   PicturePtr dst,
		   INT16 src_x,  INT16 src_y,
		   INT16 mask_x, INT16 mask_y,
		   INT16 dst_x,  INT16 dst_y,
		   CARD16 width, CARD16 height);
void sna_composite_rectangles(CARD8		 op,
			      PicturePtr		 dst,
			      xRenderColor	*color,
			      int			 num_rects,
			      xRectangle		*rects);
void sna_composite_trapezoids(CARD8 op,
			      PicturePtr src,
			      PicturePtr dst,
			      PictFormatPtr maskFormat,
			      INT16 xSrc, INT16 ySrc,
			      int ntrap, xTrapezoid *traps);
void sna_add_traps(PicturePtr picture, INT16 x, INT16 y, int n, xTrap *t);

void sna_composite_triangles(CARD8 op,
			     PicturePtr src,
			     PicturePtr dst,
			     PictFormatPtr maskFormat,
			     INT16 xSrc, INT16 ySrc,
			     int ntri, xTriangle *tri);

void sna_composite_tristrip(CARD8 op,
			    PicturePtr src,
			    PicturePtr dst,
			    PictFormatPtr maskFormat,
			    INT16 xSrc, INT16 ySrc,
			    int npoints, xPointFixed *points);

void sna_composite_trifan(CARD8 op,
			  PicturePtr src,
			  PicturePtr dst,
			  PictFormatPtr maskFormat,
			  INT16 xSrc, INT16 ySrc,
			  int npoints, xPointFixed *points);

Bool sna_gradients_create(struct sna *sna);
void sna_gradients_close(struct sna *sna);

Bool sna_glyphs_create(struct sna *sna);
void sna_glyphs(CARD8 op,
		PicturePtr src,
		PicturePtr dst,
		PictFormatPtr mask,
		INT16 xSrc, INT16 ySrc,
		int nlist,
		GlyphListPtr list,
		GlyphPtr *glyphs);
void sna_glyph_unrealize(ScreenPtr screen, GlyphPtr glyph);
void sna_glyphs_close(struct sna *sna);

void sna_read_boxes(struct sna *sna,
		    struct kgem_bo *src_bo, int16_t src_dx, int16_t src_dy,
		    PixmapPtr dst, int16_t dst_dx, int16_t dst_dy,
		    const BoxRec *box, int n);
bool sna_write_boxes(struct sna *sna, PixmapPtr dst,
		     struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
		     const void *src, int stride, int16_t src_dx, int16_t src_dy,
		     const BoxRec *box, int n);
void sna_write_boxes__xor(struct sna *sna, PixmapPtr dst,
			  struct kgem_bo *dst_bo, int16_t dst_dx, int16_t dst_dy,
			  const void *src, int stride, int16_t src_dx, int16_t src_dy,
			  const BoxRec *box, int nbox,
			  uint32_t and, uint32_t or);

bool sna_replace(struct sna *sna,
		 PixmapPtr pixmap,
		 struct kgem_bo **bo,
		 const void *src, int stride);
struct kgem_bo *sna_replace__xor(struct sna *sna,
				 PixmapPtr pixmap,
				 struct kgem_bo *bo,
				 const void *src, int stride,
				 uint32_t and, uint32_t or);

Bool
sna_compute_composite_extents(BoxPtr extents,
			      PicturePtr src, PicturePtr mask, PicturePtr dst,
			      INT16 src_x,  INT16 src_y,
			      INT16 mask_x, INT16 mask_y,
			      INT16 dst_x,  INT16 dst_y,
			      CARD16 width, CARD16 height);
Bool
sna_compute_composite_region(RegionPtr region,
			     PicturePtr src, PicturePtr mask, PicturePtr dst,
			     INT16 src_x,  INT16 src_y,
			     INT16 mask_x, INT16 mask_y,
			     INT16 dst_x,  INT16 dst_y,
			     CARD16 width, CARD16 height);

void
memcpy_blt(const void *src, void *dst, int bpp,
	   int32_t src_stride, int32_t dst_stride,
	   int16_t src_x, int16_t src_y,
	   int16_t dst_x, int16_t dst_y,
	   uint16_t width, uint16_t height);

void
memcpy_xor(const void *src, void *dst, int bpp,
	   int32_t src_stride, int32_t dst_stride,
	   int16_t src_x, int16_t src_y,
	   int16_t dst_x, int16_t dst_y,
	   uint16_t width, uint16_t height,
	   uint32_t and, uint32_t or);

#define SNA_CREATE_FB 0x10
#define SNA_CREATE_SCRATCH 0x11

inline static bool is_power_of_two(unsigned x)
{
	return (x & (x-1)) == 0;
}

#endif /* _SNA_H */
