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
#include "i810_reg.h"
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
#include "GL/glxint.h"
#include "i830_dri.h"
#include "intel_bufmgr.h"
#include "i915_drm.h"

#include "uxa.h"
Bool i830_uxa_init(ScreenPtr pScreen);
void i830_uxa_create_screen_resources(ScreenPtr pScreen);
void i830_uxa_block_handler(ScreenPtr pScreen);
Bool i830_get_aperture_space(ScrnInfoPtr scrn, drm_intel_bo ** bo_table,
			     int num_bos);

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

#ifndef container_of
#define container_of(ptr, type, member) \
	(type *)((char *)(ptr) - (char *) &((type *)0)->member)
#endif

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

struct intel_pixmap {
	dri_bo *bo;
	uint32_t tiling;
	uint32_t flush_write_domain;
	uint32_t flush_read_domains;
	uint32_t batch_write_domain;
	uint32_t batch_read_domains;
	struct list flush, batch;
};

struct intel_pixmap *i830_get_pixmap_intel(PixmapPtr pixmap);

static inline Bool i830_uxa_pixmap_is_dirty(PixmapPtr pixmap)
{
	return i830_get_pixmap_intel(pixmap)->flush_write_domain != 0;
}

static inline Bool i830_pixmap_tiled(PixmapPtr pixmap)
{
	return i830_get_pixmap_intel(pixmap)->tiling != I915_TILING_NONE;
}

dri_bo *i830_get_pixmap_bo(PixmapPtr pixmap);
void i830_set_pixmap_bo(PixmapPtr pixmap, dri_bo * bo);

typedef struct _I830OutputRec I830OutputRec, *I830OutputPtr;

#include "common.h"

#ifdef XvMCExtension
#ifdef ENABLE_XVMC
#define INTEL_XVMC 1
#endif
#endif

#define ALWAYS_SYNC 0
#define ALWAYS_FLUSH 0

#define PITCH_NONE 0

/** Record of a linear allocation in the aperture. */
typedef struct _i830_memory i830_memory;
struct _i830_memory {
	/** Offset of the allocation in card VM */
	unsigned long offset;
	/** End of the allocation in card VM */
	unsigned long end;
	/**
	 * Requested size of the allocation: doesn't count padding.
	 *
	 * Any bound memory will cover offset to (offset + size).
	 */
	unsigned long size;

	uint32_t tiling_mode;
	/** Pitch value in bytes for tiled surfaces */
	unsigned int pitch;

	/** Description of the allocation, for logging */
	char *name;

	/** @{
	 * Memory allocator linked list pointers
	 */
	i830_memory *next;
	i830_memory *prev;
	/** @} */

	dri_bo *bo;
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
	unsigned char *MMIOBase;
	int cpp;

	unsigned int bufferOffset;	/* for I830SelectBuffer */

	/* These are set in PreInit and never changed. */
	long FbMapSize;
	long GTTMapSize;

	/**
	 * Linked list of video memory allocations.  The head and tail are
	 * dummy entries that bound the allocation area.
	 */
	i830_memory *memory_list;
	/** Linked list of buffer object memory allocations */
	i830_memory *bo_list;

	i830_memory *front_buffer;
	/* One big buffer for all cursors for kernels that support this */
	i830_memory *cursor_mem_argb[2];

	dri_bufmgr *bufmgr;

	uint8_t *batch_ptr;
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
	/** Ending batch_used that was verified by i830_start_batch_atomic() */
	int batch_atomic_limit;
	struct list batch_pixmaps;
	struct list flush_pixmaps;

	/* For Xvideo */
	Bool use_drmmode_overlay;
#ifdef INTEL_XVMC
	/* For XvMC */
	Bool XvMCEnabled;
#endif

	CreateScreenResourcesProcPtr CreateScreenResources;

	Bool need_mi_flush;

	Bool tiling;
	Bool swapbuffers_wait;

	int Chipset;
	unsigned long LinearAddr;
	EntityInfoPtr pEnt;
	struct pci_device *PciInfo;
	uint8_t variant;

	unsigned int BR[20];

	CloseScreenProcPtr CloseScreen;

	void (*batch_flush_notify) (ScrnInfoPtr scrn);

	uxa_driver_t *uxa_driver;
	Bool need_flush;
	int accel_pixmap_pitch_alignment;
	int accel_pixmap_offset_alignment;
	int accel_max_x;
	int accel_max_y;
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
	float dst_coord_adjust;
	float src_coord_adjust;
	float mask_coord_adjust;

	PixmapPtr render_source, render_mask, render_dest;
	PicturePtr render_source_picture, render_mask_picture, render_dest_picture;
	CARD32 render_source_solid;
	CARD32 render_mask_solid;
	Bool render_source_is_solid;
	Bool render_mask_is_solid;
	Bool needs_render_state_emit;

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

	/* 965 render acceleration state */
	struct gen4_render_state *gen4_render_state;

	enum dri_type directRenderingType;	/* DRI enabled this generation. */

	Bool directRenderingOpen;
	int drmSubFD;
	char *deviceName;

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


static inline intel_screen_private *
intel_get_screen_private(ScrnInfoPtr scrn)
{
	return (intel_screen_private *)(scrn->driverPrivate);
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define ALIGN(i,m)	(((i) + (m) - 1) & ~((m) - 1))
#define MIN(a,b)	((a) < (b) ? (a) : (b))

unsigned long intel_get_pixmap_pitch(PixmapPtr pixmap);

/* Batchbuffer support macros and functions */
#include "i830_batchbuffer.h"

/* I830 specific functions */
extern void IntelEmitInvarientState(ScrnInfoPtr scrn);
extern void I830EmitInvarientState(ScrnInfoPtr scrn);
extern void I915EmitInvarientState(ScrnInfoPtr scrn);

extern void I830EmitFlush(ScrnInfoPtr scrn);

extern void I830InitVideo(ScreenPtr pScreen);
extern xf86CrtcPtr i830_covering_crtc(ScrnInfoPtr scrn, BoxPtr box,
				      xf86CrtcPtr desired, BoxPtr crtc_box_ret);

extern xf86CrtcPtr i830_pipe_to_crtc(ScrnInfoPtr scrn, int pipe);

Bool I830DRI2ScreenInit(ScreenPtr pScreen);
void I830DRI2CloseScreen(ScreenPtr pScreen);

extern Bool drmmode_pre_init(ScrnInfoPtr scrn, int fd, int cpp);
extern void drmmode_closefb(ScrnInfoPtr scrn);
extern int drmmode_get_pipe_from_crtc_id(drm_intel_bufmgr * bufmgr,
					 xf86CrtcPtr crtc);
extern int drmmode_output_dpms_status(xf86OutputPtr output);
extern int drmmode_crtc_id(xf86CrtcPtr crtc);
void drmmode_crtc_set_cursor_bo(xf86CrtcPtr crtc, dri_bo * cursor);

extern Bool i830_crtc_on(xf86CrtcPtr crtc);
extern int i830_crtc_to_pipe(xf86CrtcPtr crtc);
extern Bool I830AccelInit(ScreenPtr pScreen);

Bool i830_allocator_init(ScrnInfoPtr scrn, unsigned long size);
void i830_allocator_fini(ScrnInfoPtr scrn);
i830_memory *i830_allocate_memory(ScrnInfoPtr scrn, const char *name,
				  unsigned long size, unsigned long pitch,
				  int flags, uint32_t tile_format);
void i830_describe_allocations(ScrnInfoPtr scrn, int verbosity,
			       const char *prefix);
void i830_reset_allocations(ScrnInfoPtr scrn);
void i830_free_3d_memory(ScrnInfoPtr scrn);
void i830_free_memory(ScrnInfoPtr scrn, i830_memory * mem);
Bool i830_allocate_2d_memory(ScrnInfoPtr scrn);
Bool i830_allocate_3d_memory(ScrnInfoPtr scrn);
void i830_init_bufmgr(ScrnInfoPtr scrn);
#ifdef INTEL_XVMC
Bool i830_allocate_xvmc_buffer(ScrnInfoPtr scrn, const char *name,
			       i830_memory ** buffer, unsigned long size,
			       int flags);
void i830_free_xvmc_buffer(ScrnInfoPtr scrn, i830_memory * buffer);
#endif

Bool i830_tiled_width(intel_screen_private *intel, int *width, int cpp);

int i830_pad_drawable_width(int width, int cpp);

/* i830_memory.c */
Bool i830_bind_all_memory(ScrnInfoPtr scrn);
unsigned long i830_get_fence_size(intel_screen_private *intel, unsigned long size);
unsigned long i830_get_fence_pitch(intel_screen_private *intel, unsigned long pitch,
				   uint32_t tiling_mode);
void i830_set_max_gtt_map_size(ScrnInfoPtr scrn);
void i830_set_max_tiling_size(ScrnInfoPtr scrn);

i830_memory *i830_allocate_framebuffer(ScrnInfoPtr scrn);

/* i830_render.c */
Bool i830_check_composite(int op, PicturePtr sourcec, PicturePtr mask,
			  PicturePtr dest);
Bool i830_prepare_composite(int op, PicturePtr sourcec, PicturePtr mask,
			    PicturePtr dest, PixmapPtr sourcecPixmap,
			    PixmapPtr maskPixmap, PixmapPtr destPixmap);
Bool i830_transform_is_affine(PictTransformPtr t);

void i830_composite(PixmapPtr dest, int srcX, int srcY,
		    int maskX, int maskY, int dstX, int dstY, int w, int h);
void i830_done_composite(PixmapPtr dest);
/* i915_render.c */
Bool i915_check_composite(int op, PicturePtr sourcec, PicturePtr mask,
			  PicturePtr dest);
Bool i915_prepare_composite(int op, PicturePtr sourcec, PicturePtr mask,
			    PicturePtr dest, PixmapPtr sourcecPixmap,
			    PixmapPtr maskPixmap, PixmapPtr destPixmap);
void i915_composite(PixmapPtr dest, int srcX, int srcY,
		    int maskX, int maskY, int dstX, int dstY, int w, int h);
void i915_batch_flush_notify(ScrnInfoPtr scrn);
void i830_batch_flush_notify(ScrnInfoPtr scrn);
/* i965_render.c */
unsigned int gen4_render_state_size(ScrnInfoPtr scrn);
void gen4_render_state_init(ScrnInfoPtr scrn);
void gen4_render_state_cleanup(ScrnInfoPtr scrn);
Bool i965_check_composite(int op, PicturePtr sourcec, PicturePtr mask,
			  PicturePtr dest);
Bool i965_prepare_composite(int op, PicturePtr sourcec, PicturePtr mask,
			    PicturePtr dest, PixmapPtr sourcecPixmap,
			    PixmapPtr maskPixmap, PixmapPtr destPixmap);
void i965_composite(PixmapPtr dest, int srcX, int srcY,
		    int maskX, int maskY, int dstX, int dstY, int w, int h);

void i965_batch_flush_notify(ScrnInfoPtr scrn);

Bool
i830_get_transformed_coordinates(int x, int y, PictTransformPtr transform,
				 float *x_out, float *y_out);

Bool
i830_get_transformed_coordinates_3d(int x, int y, PictTransformPtr transform,
				    float *x_out, float *y_out, float *z_out);

void i830_enter_render(ScrnInfoPtr);

extern void intel_sync(ScrnInfoPtr scrn);

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

Bool i830_pixmap_tiled(PixmapPtr p);

static inline Bool
intel_check_pitch_2d(PixmapPtr pixmap)
{
	uint32_t pitch = intel_get_pixmap_pitch(pixmap);
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
	uint32_t pitch = intel_get_pixmap_pitch(pixmap);
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

extern const int I830PatternROP[16];
extern const int I830CopyROP[16];

/* Flags for memory allocation function */
#define NEED_PHYSICAL_ADDR		0x00000001
#define ALLOW_SHARING			0x00000010
#define DISABLE_REUSE			0x00000020

/**
 * Hints to CreatePixmap to tell the driver how the pixmap is going to be
 * used.
 *
 * Compare to CREATE_PIXMAP_USAGE_* in the server.
 */
enum {
	INTEL_CREATE_PIXMAP_TILING_X = 0x10000000,
	INTEL_CREATE_PIXMAP_TILING_Y,
};

void i830_debug_flush(ScrnInfoPtr scrn);

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

#endif /* _I830_H_ */
