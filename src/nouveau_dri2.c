#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "xorg-server.h"
#include "nv_include.h"
#ifdef DRI2
#include "dri2.h"
#endif

#if defined(DRI2) && DRI2INFOREC_VERSION >= 3
struct nouveau_dri2_buffer {
	DRI2BufferRec base;
	PixmapPtr ppix;
};

static inline struct nouveau_dri2_buffer *
nouveau_dri2_buffer(DRI2BufferPtr buf)
{
	return (struct nouveau_dri2_buffer *)buf;
}

static PixmapPtr get_drawable_pixmap(DrawablePtr drawable)
{
	if (drawable->type == DRAWABLE_PIXMAP)
		return (PixmapPtr)drawable;
	else
		return (*drawable->pScreen->GetWindowPixmap)((WindowPtr)drawable);
}

DRI2BufferPtr
nouveau_dri2_create_buffer2(ScreenPtr pScreen, DrawablePtr pDraw, unsigned int attachment,
			   unsigned int format)
{
	NVPtr pNv = NVPTR(xf86ScreenToScrn(pScreen));
	struct nouveau_dri2_buffer *nvbuf;
	struct nouveau_pixmap *nvpix;
	PixmapPtr ppix = NULL;

	nvbuf = calloc(1, sizeof(*nvbuf));
	if (!nvbuf)
		return NULL;

	if (attachment == DRI2BufferFrontLeft) {
		ppix = get_drawable_pixmap(pDraw);
		if (pScreen != ppix->drawable.pScreen)
			ppix = NULL;

		if (pDraw->type == DRAWABLE_WINDOW) {
#if DRI2INFOREC_VERSION >= 6
			/* Set initial swap limit on drawable. */
			DRI2SwapLimit(pDraw, pNv->swap_limit);
#endif
		}
		if (ppix)
			ppix->refcnt++;
	} else {
		int bpp;
		unsigned int usage_hint = NOUVEAU_CREATE_PIXMAP_TILED;

		/* 'format' is just depth (or 0, or maybe it depends on the caller) */
		bpp = round_up_pow2(format ? format : pDraw->depth);

		if (attachment == DRI2BufferDepth ||
		    attachment == DRI2BufferDepthStencil)
			usage_hint |= NOUVEAU_CREATE_PIXMAP_ZETA;
		else
			usage_hint |= NOUVEAU_CREATE_PIXMAP_SCANOUT;

		ppix = pScreen->CreatePixmap(pScreen, pDraw->width,
					     pDraw->height, bpp,
					     usage_hint);
	}

	if (ppix) {
		pNv->exa_force_cp = TRUE;
		exaMoveInPixmap(ppix);
		pNv->exa_force_cp = FALSE;

		nvbuf->base.pitch = ppix->devKind;
		nvbuf->base.cpp = ppix->drawable.bitsPerPixel / 8;
	}

	nvbuf->base.attachment = attachment;
	nvbuf->base.driverPrivate = nvbuf;
	nvbuf->base.format = format;
	nvbuf->base.flags = 0;
	nvbuf->ppix = ppix;

	if (ppix) {
		nvpix = nouveau_pixmap(ppix);
		if (!nvpix || !nvpix->bo ||
		    nouveau_bo_name_get(nvpix->bo, &nvbuf->base.name)) {
			pScreen->DestroyPixmap(nvbuf->ppix);
			free(nvbuf);
			return NULL;
		}
	}
	return &nvbuf->base;
}

DRI2BufferPtr
nouveau_dri2_create_buffer(DrawablePtr pDraw, unsigned int attachment,
			   unsigned int format)
{
	return nouveau_dri2_create_buffer2(pDraw->pScreen, pDraw,
					   attachment, format);
}

void
nouveau_dri2_destroy_buffer2(ScreenPtr pScreen, DrawablePtr pDraw, DRI2BufferPtr buf)
{
	struct nouveau_dri2_buffer *nvbuf;

	nvbuf = nouveau_dri2_buffer(buf);
	if (!nvbuf)
		return;

	if (nvbuf->ppix)
	    pScreen->DestroyPixmap(nvbuf->ppix);
	free(nvbuf);
}

void
nouveau_dri2_destroy_buffer(DrawablePtr pDraw, DRI2BufferPtr buf)
{
	nouveau_dri2_destroy_buffer2(pDraw->pScreen, pDraw, buf);
}

void
nouveau_dri2_copy_region2(ScreenPtr pScreen, DrawablePtr pDraw, RegionPtr pRegion,
			 DRI2BufferPtr pDstBuffer, DRI2BufferPtr pSrcBuffer)
{
	struct nouveau_dri2_buffer *src = nouveau_dri2_buffer(pSrcBuffer);
	struct nouveau_dri2_buffer *dst = nouveau_dri2_buffer(pDstBuffer);
	NVPtr pNv = NVPTR(xf86ScreenToScrn(pScreen));
	RegionPtr pCopyClip;
	GCPtr pGC;
	DrawablePtr src_draw, dst_draw;
	Bool translate = FALSE;
	int off_x = 0, off_y = 0;

	src_draw = &src->ppix->drawable;
	dst_draw = &dst->ppix->drawable;
#if 0
	ErrorF("attachments src %d, dst %d, drawable %p %p pDraw %p\n",
	       src->base.attachment, dst->base.attachment,
	       src_draw, dst_draw, pDraw);
#endif
	if (src->base.attachment == DRI2BufferFrontLeft)
		src_draw = pDraw;
	if (dst->base.attachment == DRI2BufferFrontLeft) {
#ifdef NOUVEAU_PIXMAP_SHARING
		if (pDraw->pScreen != pScreen) {
			dst_draw = DRI2UpdatePrime(pDraw, pDstBuffer);
			if (!dst_draw)
				return;
		} 
		else
#endif
			dst_draw = pDraw;
		if (dst_draw != pDraw)
			translate = TRUE;
	}

	if (translate && pDraw->type == DRAWABLE_WINDOW) {
		PixmapPtr pPix = get_drawable_pixmap(pDraw);
		off_x = pDraw->x - pPix->screen_x;
		off_y = pDraw->y - pPix->screen_y;
	}

	pGC = GetScratchGC(pDraw->depth, pScreen);
	pCopyClip = REGION_CREATE(pScreen, NULL, 0);
	REGION_COPY(pScreen, pCopyClip, pRegion);

	if (translate) {
		REGION_TRANSLATE(pScreen, pCopyClip, off_x, off_y);
	}
	pGC->funcs->ChangeClip(pGC, CT_REGION, pCopyClip, 0);
	ValidateGC(dst_draw, pGC);

	/* If this is a full buffer swap or frontbuffer flush, throttle on
	 * the previous one.
	 */
	if (dst->base.attachment == DRI2BufferFrontLeft &&
	    REGION_NUM_RECTS(pRegion) == 1) {
		BoxPtr extents = REGION_EXTENTS(pScreen, pRegion);
		if (extents->x1 == 0 && extents->y1 == 0 &&
		    extents->x2 == pDraw->width &&
		    extents->y2 == pDraw->height) {
			PixmapPtr fpix = get_drawable_pixmap(dst_draw);
			struct nouveau_bo *bo = nouveau_pixmap_bo(fpix);
			if (bo)
				nouveau_bo_wait(bo, NOUVEAU_BO_RD, pNv->client);
		}
	}

	pGC->ops->CopyArea(src_draw, dst_draw, pGC, 0, 0,
			   pDraw->width, pDraw->height, off_x, off_y);

	FreeScratchGC(pGC);
}

void
nouveau_dri2_copy_region(DrawablePtr pDraw, RegionPtr pRegion,
			 DRI2BufferPtr pDstBuffer, DRI2BufferPtr pSrcBuffer)
{
    return nouveau_dri2_copy_region2(pDraw->pScreen, pDraw, pRegion,
				     pDstBuffer, pSrcBuffer);
}

struct nouveau_dri2_vblank_state {
	enum {
		SWAP,
		BLIT,
		WAIT
	} action;

	ClientPtr client;
	XID draw;

	DRI2BufferPtr dst;
	DRI2BufferPtr src;
	DRI2SwapEventPtr func;
	void *data;
	unsigned int frame;
};

static Bool
update_front(DrawablePtr draw, DRI2BufferPtr front)
{
	int r;
	PixmapPtr pixmap;
	struct nouveau_dri2_buffer *nvbuf = nouveau_dri2_buffer(front);

	if (draw->type == DRAWABLE_PIXMAP)
		pixmap = (PixmapPtr)draw;
	else
		pixmap = (*draw->pScreen->GetWindowPixmap)((WindowPtr)draw);

	pixmap->refcnt++;

	exaMoveInPixmap(pixmap);
	r = nouveau_bo_name_get(nouveau_pixmap_bo(pixmap), &front->name);
	if (r) {
		(*draw->pScreen->DestroyPixmap)(pixmap);
		return FALSE;
	}

	if (nvbuf->ppix)
		(*draw->pScreen->DestroyPixmap)(nvbuf->ppix);

	front->pitch = pixmap->devKind;
	front->cpp = pixmap->drawable.bitsPerPixel / 8;
	nvbuf->ppix = pixmap;

	return TRUE;
}

static Bool
can_exchange(DrawablePtr draw, PixmapPtr dst_pix, PixmapPtr src_pix)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(draw->pScreen);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
	NVPtr pNv = NVPTR(scrn);
	int i;

	for (i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];
		if (crtc->enabled && crtc->rotatedData)
			return FALSE;

	}

	return ((DRI2CanFlip(draw) && pNv->has_pageflip)) &&
		dst_pix->drawable.width == src_pix->drawable.width &&
		dst_pix->drawable.height == src_pix->drawable.height &&
		dst_pix->drawable.bitsPerPixel == src_pix->drawable.bitsPerPixel &&
		dst_pix->devKind == src_pix->devKind;
}

static Bool
can_sync_to_vblank(DrawablePtr draw)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(draw->pScreen);
	NVPtr pNv = NVPTR(scrn);

	return pNv->glx_vblank &&
		nv_window_belongs_to_crtc(scrn, draw->x, draw->y,
					  draw->width, draw->height);
}

static int
nouveau_wait_vblank(DrawablePtr draw, int type, CARD64 msc,
		    CARD64 *pmsc, CARD64 *pust, void *data)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(draw->pScreen);
	NVPtr pNv = NVPTR(scrn);
	int crtcs = nv_window_belongs_to_crtc(scrn, draw->x, draw->y,
					      draw->width, draw->height);
	drmVBlank vbl;
	int ret;

	vbl.request.type = type | (crtcs == 2 ? DRM_VBLANK_SECONDARY : 0);
	vbl.request.sequence = msc;
	vbl.request.signal = (unsigned long)data;

	ret = drmWaitVBlank(pNv->dev->fd, &vbl);
	if (ret) {
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "Wait for VBlank failed: %s\n", strerror(errno));
		return ret;
	}

	if (pmsc)
		*pmsc = vbl.reply.sequence;
	if (pust)
		*pust = (CARD64)vbl.reply.tval_sec * 1000000 +
			vbl.reply.tval_usec;
	return 0;
}

#if DRI2INFOREC_VERSION >= 6
static Bool
nouveau_dri2_swap_limit_validate(DrawablePtr draw, int swap_limit)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(draw->pScreen);
	NVPtr pNv = NVPTR(scrn);

	if ((swap_limit < 1 ) || (swap_limit > pNv->max_swap_limit))
		return FALSE;

	return TRUE;
}
#endif

/* Shall we intentionally violate the OML_sync_control spec to
 * get some sort of triple-buffering behaviour on a pre 1.12.0
 * x-server?
 */
static Bool violate_oml(DrawablePtr draw)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(draw->pScreen);
	NVPtr pNv = NVPTR(scrn);

	return (DRI2INFOREC_VERSION < 6) && (pNv->swap_limit > 1);
}

static void
nouveau_dri2_finish_swap(DrawablePtr draw, unsigned int frame,
			 unsigned int tv_sec, unsigned int tv_usec,
			 struct nouveau_dri2_vblank_state *s)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(draw->pScreen);
	NVPtr pNv = NVPTR(scrn);
	PixmapPtr dst_pix;
	PixmapPtr src_pix = nouveau_dri2_buffer(s->src)->ppix;
	struct nouveau_bo *dst_bo;
	struct nouveau_bo *src_bo = nouveau_pixmap_bo(src_pix);
	struct nouveau_pushbuf *push = pNv->pushbuf;
	RegionRec reg;
	int type, ret;
	Bool front_updated;

	REGION_INIT(0, &reg, (&(BoxRec){ 0, 0, draw->width, draw->height }), 0);
	REGION_TRANSLATE(0, &reg, draw->x, draw->y);

	/* Main crtc for this drawable shall finally deliver pageflip event. */
	unsigned int ref_crtc_hw_id = nv_window_belongs_to_crtc(scrn, draw->x,
								draw->y,
								draw->width,
								draw->height);

	/* Whenever first crtc is involved, choose it as reference, as
	 * its vblank event triggered this swap.
	 */
	if (ref_crtc_hw_id & 1)
		ref_crtc_hw_id = 1;

	/* Update frontbuffer pixmap and name: Could have changed due to
	 * window (un)redirection as part of compositing.
	 */
	front_updated = update_front(draw, s->dst);

	/* Assign frontbuffer pixmap, after update in update_front() */
	dst_pix = nouveau_dri2_buffer(s->dst)->ppix;
	dst_bo = nouveau_pixmap_bo(dst_pix);

	/* Throttle on the previous frame before swapping */
	nouveau_bo_wait(dst_bo, NOUVEAU_BO_RD, push->client);

	if (can_sync_to_vblank(draw)) {
		/* Reference the back buffer to sync it to vblank */
		nouveau_pushbuf_refn(push, &(struct nouveau_pushbuf_refn) {
					   src_bo,
					   NOUVEAU_BO_VRAM | NOUVEAU_BO_RD
				     }, 1);

		if (pNv->Architecture >= NV_ARCH_C0)
			NVC0SyncToVBlank(dst_pix, REGION_EXTENTS(0, &reg));
		else
		if (pNv->Architecture >= NV_ARCH_50)
			NV50SyncToVBlank(dst_pix, REGION_EXTENTS(0, &reg));
		else
			NV11SyncToVBlank(dst_pix, REGION_EXTENTS(0, &reg));

		nouveau_pushbuf_kick(push, push->channel);
	}

	if (front_updated && can_exchange(draw, dst_pix, src_pix)) {
		type = DRI2_EXCHANGE_COMPLETE;
		DamageRegionAppend(draw, &reg);

		if (nouveau_exa_pixmap_is_onscreen(dst_pix)) {
			type = DRI2_FLIP_COMPLETE;
			ret = drmmode_page_flip(draw, src_pix,
						violate_oml(draw) ? NULL : s,
						ref_crtc_hw_id);
			if (!ret)
				goto out;
		}

		SWAP(s->dst->name, s->src->name);
		SWAP(nouveau_pixmap(dst_pix)->bo, nouveau_pixmap(src_pix)->bo);

		DamageRegionProcessPending(draw);

		/* If it is a page flip, finish it in the flip event handler. */
		if ((type == DRI2_FLIP_COMPLETE) && !violate_oml(draw))
			return;
	} else {
		type = DRI2_BLIT_COMPLETE;

		/* Reference the front buffer to let throttling work
		 * on occluded drawables. */
		nouveau_pushbuf_refn(push, &(struct nouveau_pushbuf_refn) {
					   dst_bo,
					   NOUVEAU_BO_VRAM | NOUVEAU_BO_RD
				     }, 1);

		REGION_TRANSLATE(0, &reg, -draw->x, -draw->y);
		nouveau_dri2_copy_region(draw, &reg, s->dst, s->src);

		if (can_sync_to_vblank(draw) && !violate_oml(draw)) {
			/* Request a vblank event one vblank from now, the most
			 * likely (optimistic?) time a direct framebuffer blit
			 * will complete or a desktop compositor will update its
			 * screen. This defers DRI2SwapComplete() to the earliest
			 * likely time of real swap completion.
			 */
			s->action = BLIT;
			ret = nouveau_wait_vblank(draw, DRM_VBLANK_EVENT |
						  DRM_VBLANK_RELATIVE, 1,
						  NULL, NULL, s);
			/* Done, if success. Otherwise use fallback below. */
			if (!ret)
				return;
		}
	}

	/* Special triple-buffering hack for old pre 1.12.0 x-servers used? */
	if (violate_oml(draw)) {
		/* Signal to client that swap completion timestamps and counts
		 * are invalid - they violate the specification.
		 */
		frame = tv_sec = tv_usec = 0;
	}

	/*
	 * Tell the X server buffers are already swapped even if they're
	 * not, to prevent it from blocking the client on the next
	 * GetBuffers request (and let the client do triple-buffering).
	 *
	 * XXX - The DRI2SwapLimit() API allowed us to move this to
	 *	 the flip handler with no FPS hit for page flipped swaps.
	 *       It is still needed as a fallback for some copy swaps as
	 *       we lack a method to detect true swap completion for
	 *       DRI2_BLIT_COMPLETE.
	 *
	 *       It is also used if triple-buffering is requested on
	 *       old x-servers which don't support the DRI2SwapLimit()
	 *       function.
	 */
	DRI2SwapComplete(s->client, draw, frame, tv_sec, tv_usec,
			 type, s->func, s->data);
out:
	free(s);
}

static Bool
nouveau_dri2_schedule_swap(ClientPtr client, DrawablePtr draw,
			   DRI2BufferPtr dst, DRI2BufferPtr src,
			   CARD64 *target_msc, CARD64 divisor, CARD64 remainder,
			   DRI2SwapEventPtr func, void *data)
{
	struct nouveau_dri2_vblank_state *s;
	CARD64 current_msc, expect_msc;
	CARD64 current_ust;
	int ret;

	/* Initialize a swap structure */
	s = malloc(sizeof(*s));
	if (!s)
		return FALSE;

	*s = (struct nouveau_dri2_vblank_state)
		{ SWAP, client, draw->id, dst, src, func, data, 0 };

	if (can_sync_to_vblank(draw)) {
		/* Get current sequence and vblank time*/
		ret = nouveau_wait_vblank(draw, DRM_VBLANK_RELATIVE, 0,
					  &current_msc, &current_ust, NULL);
		if (ret)
			goto fail;

		/* Truncate to match kernel interfaces; means occasional overflow
		 * misses, but that's generally not a big deal.
		 */
		*target_msc &= 0xffffffff;
		divisor &= 0xffffffff;
		remainder &= 0xffffffff;

		/* Calculate a swap target if we don't have one */
		if (current_msc >= *target_msc && divisor)
			*target_msc = current_msc + divisor
				- (current_msc - remainder) % divisor;

		/* Avoid underflow of unsigned value below */
		if (*target_msc == 0)
			*target_msc = 1;

		/* Swap at next possible vblank requested? */
		if (current_msc >= *target_msc - 1) {
			/* Special case: Need to swap at next vblank.
			 * Schedule swap immediately, bypassing the kernel
			 * vblank event mechanism to avoid a dangerous race
			 * between the client and the x-server vblank event
			 * dispatch in the main x-server dispatch loop when
			 * the swap_limit is set to 2 for triple-buffering.
			 *
			 * This also optimizes for the common case of swap
			 * at next vblank, avoiding vblank dispatch delay.
			 */
			s->frame = 1 + ((unsigned int) current_msc & 0xffffffff);
			*target_msc = 1 + current_msc;
			nouveau_dri2_finish_swap(draw, current_msc,
						 (unsigned int) (current_ust / 1000000),
						 (unsigned int) (current_ust % 1000000),
						 s);
			return TRUE;
		}

		/* This is a swap in the future, ie. the vblank event will
		 * only get dispatched at least 2 vblanks into the future.
		 */

#if DRI2INFOREC_VERSION >= 6
		/* On XOrg 1.12+ we need to temporarily lower the swaplimit to 1,
		 * so that DRI2GetBuffersWithFormat() requests from the client get
		 * deferred in the x-server until the vblank event has been
		 * dispatched to us and nouveau_dri2_finish_swap() is done. If
		 * we wouldn't do this, DRI2GetBuffersWithFormat() would operate
		 * on wrong (pre-swap) buffers, and cause a segfault later on in
		 * nouveau_dri2_finish_swap(). Our vblank event handler will restore
		 * the old swaplimit immediately after nouveau_dri2_finish_swap()
		 * is done, so we still get 1 video refresh cycle worth of triple-
		 * buffering, because the client can start rendering again 1 cycle
		 * before the pending swap is completed.
		 *
		 * The same race would happen for the "swap at next vblank" case,
		 * but the special case "swap immediately" code above prevents this.
		 */
		DRI2SwapLimit(draw, 1);
#endif

		/* Request a vblank event one frame before the target */
		ret = nouveau_wait_vblank(draw, DRM_VBLANK_ABSOLUTE |
					  DRM_VBLANK_EVENT,
					  max(current_msc, *target_msc - 1),
					  &expect_msc, NULL, s);
		if (ret)
			goto fail;
		s->frame = 1 + ((unsigned int) expect_msc & 0xffffffff);
		*target_msc = 1 + expect_msc;
	} else {
		/* We can't/don't want to sync to vblank, just swap. */
		nouveau_dri2_finish_swap(draw, 0, 0, 0, s);
	}

	return TRUE;

fail:
	free(s);
	return FALSE;
}

static Bool
nouveau_dri2_schedule_wait(ClientPtr client, DrawablePtr draw,
			   CARD64 target_msc, CARD64 divisor, CARD64 remainder)
{
	struct nouveau_dri2_vblank_state *s;
	CARD64 current_msc;
	int ret;

	/* Truncate to match kernel interfaces; means occasional overflow
	 * misses, but that's generally not a big deal.
	 */
	target_msc &= 0xffffffff;
	divisor &= 0xffffffff;
	remainder &= 0xffffffff;

	if (!can_sync_to_vblank(draw)) {
		DRI2WaitMSCComplete(client, draw, target_msc, 0, 0);
		return TRUE;
	}

	/* Initialize a vblank structure */
	s = malloc(sizeof(*s));
	if (!s)
		return FALSE;

	*s = (struct nouveau_dri2_vblank_state) { WAIT, client, draw->id };

	/* Get current sequence */
	ret = nouveau_wait_vblank(draw, DRM_VBLANK_RELATIVE, 0,
				  &current_msc, NULL, NULL);
	if (ret)
		goto fail;

	/* Calculate a wait target if we don't have one */
	if (current_msc >= target_msc && divisor)
		target_msc = current_msc + divisor
			- (current_msc - remainder) % divisor;

	/* Request a vblank event */
	ret = nouveau_wait_vblank(draw, DRM_VBLANK_ABSOLUTE |
				  DRM_VBLANK_EVENT,
				  max(current_msc, target_msc),
				  NULL, NULL, s);
	if (ret)
		goto fail;

	DRI2BlockClient(client, draw);
	return TRUE;
fail:
	free(s);
	return FALSE;
}

static Bool
nouveau_dri2_get_msc(DrawablePtr draw, CARD64 *ust, CARD64 *msc)
{
	int ret;

	if (!can_sync_to_vblank(draw)) {
		*ust = 0;
		*msc = 0;
		return TRUE;
	}

	/* Get current sequence */
	ret = nouveau_wait_vblank(draw, DRM_VBLANK_RELATIVE, 0, msc, ust, NULL);
	if (ret)
		return FALSE;

	return TRUE;
}

void
nouveau_dri2_vblank_handler(int fd, unsigned int frame,
			    unsigned int tv_sec, unsigned int tv_usec,
			    void *event_data)
{
	struct nouveau_dri2_vblank_state *s = event_data;
	DrawablePtr draw;
	int ret;

	ret = dixLookupDrawable(&draw, s->draw, serverClient,
				M_ANY, DixWriteAccess);
	if (ret) {
		free(s);
		return;
	}

	switch (s->action) {
	case SWAP:
		nouveau_dri2_finish_swap(draw, frame, tv_sec, tv_usec, s);
#if DRI2INFOREC_VERSION >= 6
		/* Restore real swap limit on drawable, now that it is safe. */
		ScrnInfoPtr scrn = xf86ScreenToScrn(draw->pScreen);
		DRI2SwapLimit(draw, NVPTR(scrn)->swap_limit);
#endif

		break;

	case WAIT:
		DRI2WaitMSCComplete(s->client, draw, frame, tv_sec, tv_usec);
		free(s);
		break;

	case BLIT:
		DRI2SwapComplete(s->client, draw, frame, tv_sec, tv_usec,
				 DRI2_BLIT_COMPLETE, s->func, s->data);
		free(s);
		break;
	}
}

void
nouveau_dri2_flip_event_handler(unsigned int frame, unsigned int tv_sec,
				unsigned int tv_usec, void *event_data)
{
	struct nouveau_dri2_vblank_state *flip = event_data;
	DrawablePtr draw;
	ScreenPtr screen;
	ScrnInfoPtr scrn;
	int status;

	status = dixLookupDrawable(&draw, flip->draw, serverClient,
				   M_ANY, DixWriteAccess);
	if (status != Success) {
		free(flip);
		return;
	}

	screen = draw->pScreen;
	scrn = xf86ScreenToScrn(screen);

	/* We assume our flips arrive in order, so we don't check the frame */
	switch (flip->action) {
	case SWAP:
		/* Check for too small vblank count of pageflip completion,
		 * taking wraparound into account. This usually means some
		 * defective kms pageflip completion, causing wrong (msc, ust)
		 * return values and possible visual corruption.
		 * Skip test for frame == 0, as this is a valid constant value
		 * reported by all Linux kernels at least up to Linux 3.0.
		 */
		if ((frame != 0) &&
		    (frame < flip->frame) && (flip->frame - frame < 5)) {
			xf86DrvMsg(scrn->scrnIndex, X_WARNING,
				   "%s: Pageflip has impossible msc %d < target_msc %d\n",
				   __func__, frame, flip->frame);
			/* All-Zero values signal failure of (msc, ust)
			 * timestamping to client.
			 */
			frame = tv_sec = tv_usec = 0;
		}

		DRI2SwapComplete(flip->client, draw, frame, tv_sec, tv_usec,
				 DRI2_FLIP_COMPLETE, flip->func,
				 flip->data);
		break;
	default:
		xf86DrvMsg(scrn->scrnIndex, X_WARNING,
			   "%s: unknown vblank event received\n", __func__);
		/* Unknown type */
		break;
	}

	free(flip);
}

Bool
nouveau_dri2_init(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
	NVPtr pNv = NVPTR(pScrn);
	DRI2InfoRec dri2 = { 0 };
	const char *drivernames[2][2] = {
		{ "nouveau", "nouveau" },
		{ "nouveau_vieux", "nouveau_vieux" }
	};

	if (pNv->Architecture >= NV_ARCH_30)
		dri2.driverNames = drivernames[0];
	else
		dri2.driverNames = drivernames[1];
	dri2.numDrivers = 2;
	dri2.driverName = dri2.driverNames[0];

	dri2.fd = pNv->dev->fd;
	dri2.deviceName = pNv->drm_device_name;

	dri2.version = DRI2INFOREC_VERSION;
	dri2.CreateBuffer = nouveau_dri2_create_buffer;
	dri2.DestroyBuffer = nouveau_dri2_destroy_buffer;
	dri2.CopyRegion = nouveau_dri2_copy_region;
	dri2.ScheduleSwap = nouveau_dri2_schedule_swap;
	dri2.ScheduleWaitMSC = nouveau_dri2_schedule_wait;
	dri2.GetMSC = nouveau_dri2_get_msc;

#if DRI2INFOREC_VERSION >= 6
	dri2.SwapLimitValidate = nouveau_dri2_swap_limit_validate;
#endif

#if DRI2INFOREC_VERSION >= 7
	dri2.version = 7;
	dri2.GetParam = NULL;
#endif

#if DRI2INFOREC_VERSION >= 9
	dri2.version = 9;
	dri2.CreateBuffer2 = nouveau_dri2_create_buffer2;
	dri2.DestroyBuffer2 = nouveau_dri2_destroy_buffer2;
	dri2.CopyRegion2 = nouveau_dri2_copy_region2;
#endif
	return DRI2ScreenInit(pScreen, &dri2);
}

void
nouveau_dri2_fini(ScreenPtr pScreen)
{
	DRI2CloseScreen(pScreen);
}
#else
Bool
nouveau_dri2_init(ScreenPtr pScreen)
{
	return TRUE;
}

void
nouveau_dri2_fini(ScreenPtr pScreen)
{
}
#endif

