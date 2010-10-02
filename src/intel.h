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

#if 0
#define I830DEBUG
#endif

#include <stdint.h>

#ifndef REMAP_RESERVED
#define REMAP_RESERVED 0
#endif

#ifndef _I830_H_
#define _I830_H_

#include "xf86_OSproc.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86Cursor.h"
#include "xf86xv.h"
#include "vgaHW.h"
#include "xf86Crtc.h"
#include "xf86RandR12.h"

#include "xorg-server.h"
#include <pciaccess.h>

#include "xf86drm.h"
#include "sarea.h"
#define _XF86DRI_SERVER_
#include "dri.h"
#include "dri2.h"
#include "intel_bufmgr.h"
#include "i915_drm.h"

#include "intel_driver.h"

#include "uxa.h"
/* XXX
 * The X server gained an *almost* identical implementation in 1.9.
 *
 * Remove this duplicate code either in 2.16 (when we can depend upon 1.9)
 * or the drivers are merged back into the xserver tree, whichever happens
 * earlier.
 */

#ifndef _LIST_H_
/* classic doubly-link circular list */
struct list {
	struct list *next, *prev;
};

static void
list_init(struct list *list)
{
	list->next = list->prev = list;
}

static inline void
__list_add(struct list *entry,
	    struct list *prev,
	    struct list *next)
{
	next->prev = entry;
	entry->next = next;
	entry->prev = prev;
	prev->next = entry;
}

static inline void
list_add(struct list *entry, struct list *head)
{
	__list_add(entry, head, head->next);
}

static inline void
__list_del(struct list *prev, struct list *next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void
list_del(struct list *entry)
{
	__list_del(entry->prev, entry->next);
	list_init(entry);
}

static inline Bool
list_is_empty(struct list *head)
{
	return head->next == head;
}
#endif

/* XXX work around a broken define in list.h currently [ickle 20100713] */
#undef container_of

#ifndef container_of
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - (char *) &((type *)0)->member))
#endif

#ifndef list_entry
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)
#endif

#ifndef list_first_entry
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)
#endif

#ifndef list_foreach
#define list_foreach(pos, head)			\
	for (pos = (head)->next; pos != (head);	pos = pos->next)
#endif

/* XXX list.h from xserver-1.9 uses a GCC-ism to avoid having to pass type */
#ifndef list_foreach_entry
#define list_foreach_entry(pos, type, head, member)		\
	for (pos = list_entry((head)->next, type, member);\
	     &pos->member != (head);					\
	     pos = list_entry(pos->member.next, type, member))
#endif

struct intel_pixmap {
	dri_bo *bo;

	struct list flush, batch, in_flight;

	uint16_t stride;
	uint8_t tiling;
	int8_t busy :2;
	int8_t batch_write :1;
};

#if HAS_DEVPRIVATEKEYREC
extern DevPrivateKeyRec uxa_pixmap_index;
#else
extern int uxa_pixmap_index;
#endif

static inline struct intel_pixmap *intel_get_pixmap_private(PixmapPtr pixmap)
{
#if HAS_DEVPRIVATEKEYREC
	return dixGetPrivate(&pixmap->devPrivates, &uxa_pixmap_index);
#else
	return dixLookupPrivate(&pixmap->devPrivates, &uxa_pixmap_index);
#endif
}

static inline Bool intel_pixmap_is_busy(struct intel_pixmap *priv)
{
	if (priv->busy == -1)
		priv->busy = drm_intel_bo_busy(priv->bo);
	return priv->busy;
}

static inline void intel_set_pixmap_private(PixmapPtr pixmap, struct intel_pixmap *intel)
{
	dixSetPrivate(&pixmap->devPrivates, &uxa_pixmap_index, intel);
}

static inline Bool intel_pixmap_is_dirty(PixmapPtr pixmap)
{
	return !list_is_empty(&intel_get_pixmap_private(pixmap)->flush);
}

static inline Bool intel_pixmap_tiled(PixmapPtr pixmap)
{
	return intel_get_pixmap_private(pixmap)->tiling != I915_TILING_NONE;
}

dri_bo *intel_get_pixmap_bo(PixmapPtr pixmap);
void intel_set_pixmap_bo(PixmapPtr pixmap, dri_bo * bo);

typedef struct _I830OutputRec I830OutputRec, *I830OutputPtr;

#include "common.h"

#ifdef XvMCExtension
#ifdef ENABLE_XVMC
#define INTEL_XVMC 1
#endif
#endif

#define PITCH_NONE 0

/** Record of a linear allocation in the aperture. */
typedef struct _intel_memory intel_memory;
struct _intel_memory {
	/** Description of the allocation, for logging */
	char *name;

	/** @{
	 * Memory allocator linked list pointers
	 */
	intel_memory *next;
	intel_memory *prev;
	/** @} */

	drm_intel_bo *bo;
	uint32_t gem_name;
};

typedef struct _I830CrtcPrivateRec {
	int pipe;
	int plane;

	Bool enabled;

	int dpms_mode;

	int x, y;

	/* Lookup table values to be set when the CRTC is enabled */
	uint8_t lut_r[256], lut_g[256], lut_b[256];
} I830CrtcPrivateRec, *I830CrtcPrivatePtr;

#define I830CrtcPrivate(c) ((I830CrtcPrivatePtr) (c)->driver_private)

/** enumeration of 3d consumers so some can maintain invariant state. */
enum last_3d {
	LAST_3D_OTHER,
	LAST_3D_VIDEO,
	LAST_3D_RENDER,
	LAST_3D_ROTATION
};

enum dri_type {
	DRI_DISABLED,
	DRI_NONE,
	DRI_DRI2
};

typedef struct intel_screen_private {
	ScrnInfoPtr scrn;
	unsigned char *MMIOBase;
	int cpp;

	unsigned int bufferOffset;	/* for I830SelectBuffer */

	/* These are set in PreInit and never changed. */
	long FbMapSize;
	long GTTMapSize;

	void *modes;
	drm_intel_bo *front_buffer;
	long front_pitch, front_tiling;
	void *shadow_buffer;
	int shadow_stride;
	DamagePtr shadow_damage;

	dri_bufmgr *bufmgr;

	uint32_t batch_ptr[4096];
	/** Byte offset in batch_ptr for the next dword to be emitted. */
	unsigned int batch_used;
	/** Position in batch_ptr at the start of the current BEGIN_BATCH */
	unsigned int batch_emit_start;
	/** Number of bytes to be emitted in the current BEGIN_BATCH. */
	uint32_t batch_emitting;
	dri_bo *batch_bo;
	dri_bo *last_batch_bo;
	/** Whether we're in a section of code that can't tolerate flushing */
	Bool in_batch_atomic;
	/** Ending batch_used that was verified by intel_start_batch_atomic() */
	int batch_atomic_limit;
	struct list batch_pixmaps;
	struct list flush_pixmaps;
	struct list in_flight;

	/* For Xvideo */
	Bool use_overlay;
#ifdef INTEL_XVMC
	/* For XvMC */
	Bool XvMCEnabled;
#endif

	CreateScreenResourcesProcPtr CreateScreenResources;

	Bool shadow_present;

	Bool need_mi_flush;

	Bool tiling;
	Bool swapbuffers_wait;

	int Chipset;
	unsigned long LinearAddr;
	EntityInfoPtr pEnt;
	struct pci_device *PciInfo;
	struct intel_chipset chipset;

	unsigned int BR[20];

	CloseScreenProcPtr CloseScreen;

	void (*vertex_flush) (struct intel_screen_private *intel);
	void (*batch_flush_notify) (ScrnInfoPtr scrn);

	uxa_driver_t *uxa_driver;
	Bool need_sync;
	int accel_pixmap_offset_alignment;
	int accel_max_x;
	int accel_max_y;
	int max_bo_size;
	int max_gtt_map_size;
	int max_tiling_size;

	Bool XvDisabled;	/* Xv disabled in PreInit. */
	Bool XvEnabled;		/* Xv enabled for this generation. */
	Bool XvPreferOverlay;

	int colorKey;
	XF86VideoAdaptorPtr adaptor;
	ScreenBlockHandlerProcPtr BlockHandler;
	Bool overlayOn;

	struct {
		drm_intel_bo *gen4_vs_bo;
		drm_intel_bo *gen4_sf_bo;
		drm_intel_bo *gen4_wm_packed_bo;
		drm_intel_bo *gen4_wm_planar_bo;
		drm_intel_bo *gen4_cc_bo;
		drm_intel_bo *gen4_cc_vp_bo;
		drm_intel_bo *gen4_sampler_bo;
		drm_intel_bo *gen4_sip_kernel_bo;
	} video;

	/* Render accel state */
	float scale_units[2][2];
	/** Transform pointers for src/mask, or NULL if identity */
	PictTransform *transform[2];

	PixmapPtr render_source, render_mask, render_dest;
	PicturePtr render_source_picture, render_mask_picture, render_dest_picture;
	CARD32 render_source_solid;
	CARD32 render_mask_solid;
	PixmapPtr render_current_dest;
	Bool render_source_is_solid;
	Bool render_mask_is_solid;
	Bool needs_render_state_emit;
	Bool needs_render_vertex_emit;
	Bool needs_render_ca_pass;

	/* i830 render accel state */
	uint32_t render_dest_format;
	uint32_t cblend, ablend, s8_blendctl;

	/* i915 render accel state */
	PixmapPtr texture[2];
	uint32_t mapstate[6];
	uint32_t samplerstate[6];

	struct {
		int op;
		uint32_t dst_format;
	} i915_render_state;

	uint32_t prim_offset;
	void (*prim_emit)(PixmapPtr dest,
			  int srcX, int srcY,
			  int maskX, int maskY,
			  int dstX, int dstY,
			  int w, int h);
	int floats_per_vertex;
	int last_floats_per_vertex;
	uint32_t vertex_count;
	uint32_t vertex_index;
	uint32_t vertex_used;
	float vertex_ptr[4*1024];
	dri_bo *vertex_bo;

	/* 965 render acceleration state */
	struct gen4_render_state *gen4_render_state;

	enum dri_type directRenderingType;	/* DRI enabled this generation. */

	Bool directRenderingOpen;
	int drmSubFD;
	char *deviceName;

	Bool use_pageflipping;
	Bool force_fallback;
	Bool use_shadow;

	/* Broken-out options. */
	OptionInfoPtr Options;

	/* Driver phase/state information */
	Bool suspended;

	enum last_3d last_3d;

	/**
	 * User option to print acceleration fallback info to the server log.
	 */
	Bool fallback_debug;
	unsigned debug_flush;
} intel_screen_private;

enum {
	DEBUG_FLUSH_BATCHES = 0x1,
	DEBUG_FLUSH_CACHES = 0x2,
	DEBUG_FLUSH_WAIT = 0x4,
};

extern Bool intel_mode_pre_init(ScrnInfoPtr pScrn, int fd, int cpp);
extern void intel_mode_init(struct intel_screen_private *intel);
extern void intel_mode_remove_fb(intel_screen_private *intel);
extern void intel_mode_fini(intel_screen_private *intel);

extern int intel_get_pipe_from_crtc_id(drm_intel_bufmgr *bufmgr, xf86CrtcPtr crtc);
extern int intel_crtc_id(xf86CrtcPtr crtc);
extern int intel_output_dpms_status(xf86OutputPtr output);

extern Bool intel_do_pageflip(intel_screen_private *intel,
			      dri_bo *new_front,
			      void *data);

static inline intel_screen_private *
intel_get_screen_private(ScrnInfoPtr scrn)
{
	return (intel_screen_private *)(scrn->driverPrivate);
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define ALIGN(i,m)	(((i) + (m) - 1) & ~((m) - 1))
#define MIN(a,b)	((a) < (b) ? (a) : (b))

static inline unsigned long intel_pixmap_pitch(PixmapPtr pixmap)
{
	return (unsigned long)pixmap->devKind;
}

/* Batchbuffer support macros and functions */
#include "intel_batchbuffer.h"

/* I830 specific functions */
extern void IntelEmitInvarientState(ScrnInfoPtr scrn);
extern void I830EmitInvarientState(ScrnInfoPtr scrn);
extern void I915EmitInvarientState(ScrnInfoPtr scrn);

extern void I830EmitFlush(ScrnInfoPtr scrn);

extern void I830InitVideo(ScreenPtr pScreen);
extern xf86CrtcPtr intel_covering_crtc(ScrnInfoPtr scrn, BoxPtr box,
				      xf86CrtcPtr desired, BoxPtr crtc_box_ret);

extern xf86CrtcPtr intel_pipe_to_crtc(ScrnInfoPtr scrn, int pipe);

Bool I830DRI2ScreenInit(ScreenPtr pScreen);
void I830DRI2CloseScreen(ScreenPtr pScreen);
void I830DRI2FrameEventHandler(unsigned int frame, unsigned int tv_sec,
			       unsigned int tv_usec, void *user_data);
void I830DRI2FlipEventHandler(unsigned int frame, unsigned int tv_sec,
			      unsigned int tv_usec, void *user_data);

extern Bool intel_crtc_on(xf86CrtcPtr crtc);
static inline int intel_crtc_to_pipe(xf86CrtcPtr crtc)
{
	intel_screen_private *intel = intel_get_screen_private(crtc->scrn);
	return intel_get_pipe_from_crtc_id(intel->bufmgr, crtc);
}

/* intel_memory.c */
unsigned long intel_get_fence_size(intel_screen_private *intel, unsigned long size);
unsigned long intel_get_fence_pitch(intel_screen_private *intel, unsigned long pitch,
				   uint32_t tiling_mode);
void intel_set_gem_max_sizes(ScrnInfoPtr scrn);

drm_intel_bo *intel_allocate_framebuffer(ScrnInfoPtr scrn,
					int w, int h, int cpp,
					unsigned long *pitch,
					uint32_t *tiling);

/* i830_render.c */
Bool i830_check_composite(int op,
			  PicturePtr sourcec, PicturePtr mask, PicturePtr dest,
			  int width, int height);
Bool i830_check_composite_target(PixmapPtr pixmap);
Bool i830_check_composite_texture(ScreenPtr screen, PicturePtr picture);
Bool i830_prepare_composite(int op, PicturePtr sourcec, PicturePtr mask,
			    PicturePtr dest, PixmapPtr sourcecPixmap,
			    PixmapPtr maskPixmap, PixmapPtr destPixmap);

void i830_composite(PixmapPtr dest, int srcX, int srcY,
		    int maskX, int maskY, int dstX, int dstY, int w, int h);
/* i915_render.c */
Bool i915_check_composite(int op,
			  PicturePtr sourcec, PicturePtr mask, PicturePtr dest,
			  int width, int height);
Bool i915_check_composite_target(PixmapPtr pixmap);
Bool i915_check_composite_texture(ScreenPtr screen, PicturePtr picture);
Bool i915_prepare_composite(int op, PicturePtr sourcec, PicturePtr mask,
			    PicturePtr dest, PixmapPtr sourcecPixmap,
			    PixmapPtr maskPixmap, PixmapPtr destPixmap);
void i915_composite(PixmapPtr dest, int srcX, int srcY,
		    int maskX, int maskY, int dstX, int dstY, int w, int h);
void i915_vertex_flush(intel_screen_private *intel);
void i915_batch_flush_notify(ScrnInfoPtr scrn);
void i830_batch_flush_notify(ScrnInfoPtr scrn);
/* i965_render.c */
unsigned int gen4_render_state_size(ScrnInfoPtr scrn);
void gen4_render_state_init(ScrnInfoPtr scrn);
void gen4_render_state_cleanup(ScrnInfoPtr scrn);
Bool i965_check_composite(int op,
			  PicturePtr sourcec, PicturePtr mask, PicturePtr dest,
			  int width, int height);
Bool i965_check_composite_texture(ScreenPtr screen, PicturePtr picture);
Bool i965_prepare_composite(int op, PicturePtr sourcec, PicturePtr mask,
			    PicturePtr dest, PixmapPtr sourcecPixmap,
			    PixmapPtr maskPixmap, PixmapPtr destPixmap);
void i965_composite(PixmapPtr dest, int srcX, int srcY,
		    int maskX, int maskY, int dstX, int dstY, int w, int h);

void i965_batch_flush_notify(ScrnInfoPtr scrn);

Bool intel_transform_is_affine(PictTransformPtr t);
Bool
intel_get_transformed_coordinates(int x, int y, PictTransformPtr transform,
				 float *x_out, float *y_out);

Bool
intel_get_transformed_coordinates_3d(int x, int y, PictTransformPtr transform,
				    float *x_out, float *y_out, float *z_out);

static inline void
intel_debug_fallback(ScrnInfoPtr scrn, char *format, ...)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	va_list ap;

	va_start(ap, format);
	if (intel->fallback_debug) {
		xf86DrvMsg(scrn->scrnIndex, X_INFO, "fallback: ");
		LogVMessageVerb(X_INFO, 1, format, ap);
	}
	va_end(ap);
}

static inline Bool
intel_check_pitch_2d(PixmapPtr pixmap)
{
	uint32_t pitch = intel_pixmap_pitch(pixmap);
	if (pitch > KB(32)) {
		ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
		intel_debug_fallback(scrn, "pitch exceeds 2d limit 32K\n");
		return FALSE;
	}
	return TRUE;
}

/* For pre-965 chip only, as they have 8KB limit for 3D */
static inline Bool
intel_check_pitch_3d(PixmapPtr pixmap)
{
	uint32_t pitch = intel_pixmap_pitch(pixmap);
	if (pitch > KB(8)) {
		ScrnInfoPtr scrn = xf86Screens[pixmap->drawable.pScreen->myNum];
		intel_debug_fallback(scrn, "pitch exceeds 3d limit 8K\n");
		return FALSE;
	}
	return TRUE;
}

/**
 * Little wrapper around drm_intel_bo_reloc to return the initial value you
 * should stuff into the relocation entry.
 *
 * If only we'd done this before settling on the library API.
 */
static inline uint32_t
intel_emit_reloc(drm_intel_bo * bo, uint32_t offset,
		 drm_intel_bo * target_bo, uint32_t target_offset,
		 uint32_t read_domains, uint32_t write_domain)
{
	drm_intel_bo_emit_reloc(bo, offset, target_bo, target_offset,
				read_domains, write_domain);

	return target_bo->offset + target_offset;
}

static inline drm_intel_bo *intel_bo_alloc_for_data(ScrnInfoPtr scrn,
						    void *data,
						    unsigned int size,
						    char *name)
{
	intel_screen_private *intel = intel_get_screen_private(scrn);
	drm_intel_bo *bo;

	bo = drm_intel_bo_alloc(intel->bufmgr, name, size, 4096);
	if (!bo)
		return NULL;
	drm_intel_bo_subdata(bo, 0, size, data);

	return bo;
}

/* Flags for memory allocation function */
#define NEED_PHYSICAL_ADDR		0x00000001
#define ALLOW_SHARING			0x00000010
#define DISABLE_REUSE			0x00000020

void intel_debug_flush(ScrnInfoPtr scrn);

static inline PixmapPtr get_drawable_pixmap(DrawablePtr drawable)
{
	ScreenPtr screen = drawable->pScreen;

	if (drawable->type == DRAWABLE_PIXMAP)
		return (PixmapPtr) drawable;
	else
		return screen->GetWindowPixmap((WindowPtr) drawable);
}

static inline Bool pixmap_is_scanout(PixmapPtr pixmap)
{
	ScreenPtr screen = pixmap->drawable.pScreen;

	return pixmap == screen->GetScreenPixmap(screen);
}

const OptionInfoRec *intel_uxa_available_options(int chipid, int busid);

Bool intel_uxa_init(ScreenPtr pScreen);
void intel_uxa_create_screen_resources(ScreenPtr pScreen);
void intel_uxa_block_handler(intel_screen_private *intel);
Bool intel_get_aperture_space(ScrnInfoPtr scrn, drm_intel_bo ** bo_table,
			      int num_bos);

#endif /* _I830_H_ */
