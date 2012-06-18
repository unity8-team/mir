/*
 * Copyright 2009 Nouveau Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nv_include.h"
#include "exa.h"

#include "hwdefs/nv_m2mf.xml.h"

static inline Bool
NVAccelMemcpyRect(char *dst, const char *src, int height, int dst_pitch,
		  int src_pitch, int line_len)
{
	if ((src_pitch == line_len) && (src_pitch == dst_pitch)) {
		memcpy(dst, src, line_len*height);
	} else {
		while (height--) {
			memcpy(dst, src, line_len);
			src += src_pitch;
			dst += dst_pitch;
		}
	}

	return TRUE;
}

Bool
NVAccelM2MF(NVPtr pNv, int w, int h, int cpp, uint32_t srcoff, uint32_t dstoff,
	    struct nouveau_bo *src, int sd, int sp, int sh, int sx, int sy,
	    struct nouveau_bo *dst, int dd, int dp, int dh, int dx, int dy)
{
	if (pNv->Architecture >= NV_ARCH_E0)
		return NVE0EXARectCopy(pNv, w, h, cpp,
				       src, srcoff, sd, sp, sh, sx, sy,
				       dst, dstoff, dd, dp, dh, dx, dy);
	else
	if (pNv->Architecture >= NV_ARCH_C0 && pNv->NvCopy)
		return NVC0EXARectCopy(pNv, w, h, cpp,
				       src, srcoff, sd, sp, sh, sx, sy,
				       dst, dstoff, dd, dp, dh, dx, dy);
	if (pNv->Architecture >= NV_ARCH_C0)
		return NVC0EXARectM2MF(pNv, w, h, cpp,
				       src, srcoff, sd, sp, sh, sx, sy,
				       dst, dstoff, dd, dp, dh, dx, dy);
	else
	if (pNv->Architecture >= NV_ARCH_50 && pNv->NvCopy)
		return NVA3EXARectCopy(pNv, w, h, cpp,
				       src, srcoff, sd, sp, sh, sx, sy,
				       dst, dstoff, dd, dp, dh, dx, dy);
	else
	if (pNv->Architecture >= NV_ARCH_50)
		return NV50EXARectM2MF(pNv, w, h, cpp,
				       src, srcoff, sd, sp, sh, sx, sy,
				       dst, dstoff, dd, dp, dh, dx, dy);
	else
		return NV04EXARectM2MF(pNv, w, h, cpp,
				       src, srcoff, sd, sp, sh, sx, sy,
				       dst, dstoff, dd, dp, dh, dx, dy);
	return FALSE;
}

static int
nouveau_exa_mark_sync(ScreenPtr pScreen)
{
	return 0;
}

static void
nouveau_exa_wait_marker(ScreenPtr pScreen, int marker)
{
}

static Bool
nouveau_exa_prepare_access(PixmapPtr ppix, int index)
{
	struct nouveau_bo *bo = nouveau_pixmap_bo(ppix);
	NVPtr pNv = NVPTR(xf86ScreenToScrn(ppix->drawable.pScreen));

	if (nv50_style_tiled_pixmap(ppix) && !pNv->wfb_enabled)
		return FALSE;
	if (nouveau_bo_map(bo, NOUVEAU_BO_RDWR, pNv->client))
		return FALSE;
	ppix->devPrivate.ptr = bo->map;
	return TRUE;
}

static void
nouveau_exa_finish_access(PixmapPtr ppix, int index)
{
}

static Bool
nouveau_exa_pixmap_is_offscreen(PixmapPtr ppix)
{
	return nouveau_pixmap_bo(ppix) != NULL;
}

static void *
nouveau_exa_create_pixmap(ScreenPtr pScreen, int width, int height, int depth,
			  int usage_hint, int bitsPerPixel, int *new_pitch)
{
	ScrnInfoPtr scrn = xf86ScreenToScrn(pScreen);
	NVPtr pNv = NVPTR(scrn);
	struct nouveau_pixmap *nvpix;
	int ret;

	if (!width || !height)
		return calloc(1, sizeof(*nvpix));

	if (!pNv->exa_force_cp && pNv->dev->vram_size <= 32 * 1024 * 1024)
		return NULL;

	nvpix = calloc(1, sizeof(*nvpix));
	if (!nvpix)
		return NULL;

	ret = nouveau_allocate_surface(scrn, width, height, bitsPerPixel,
				       usage_hint, new_pitch, &nvpix->bo);
	if (!ret) {
		free(nvpix);
		return NULL;
	}

	return nvpix;
}

static void
nouveau_exa_destroy_pixmap(ScreenPtr pScreen, void *priv)
{
	struct nouveau_pixmap *nvpix = priv;

	if (!nvpix)
		return;

	nouveau_bo_ref(NULL, &nvpix->bo);
	free(nvpix);
}

bool
nv50_style_tiled_pixmap(PixmapPtr ppix)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(ppix->drawable.pScreen);
	NVPtr pNv = NVPTR(pScrn);

	return pNv->Architecture >= NV_ARCH_50 &&
	       nouveau_pixmap_bo(ppix)->config.nv50.memtype;
}

static int
nouveau_exa_scratch(NVPtr pNv, int size, struct nouveau_bo **pbo, int *off)
{
	struct nouveau_bo *bo;
	int ret;

	if (!pNv->transfer ||
	     pNv->transfer->size <= pNv->transfer_offset + size) {
		ret = nouveau_bo_new(pNv->dev, NOUVEAU_BO_GART | NOUVEAU_BO_MAP,
				     0, NOUVEAU_ALIGN(size, 1 * 1024 * 1024),
				     NULL, &bo);
		if (ret == 0)
			ret = nouveau_bo_map(bo, NOUVEAU_BO_RDWR, pNv->client);
		if (ret != 0)
			return ret;

		nouveau_bo_ref(bo, &pNv->transfer);
		pNv->transfer_offset = 0;
	}

	*off = pNv->transfer_offset;
	*pbo = pNv->transfer;

	pNv->transfer_offset += size + 0xff;
	pNv->transfer_offset &= ~0xff;
	return 0;
}

static Bool
nouveau_exa_download_from_screen(PixmapPtr pspix, int x, int y, int w, int h,
				 char *dst, int dst_pitch)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pspix->drawable.pScreen);
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_bo *bo;
	int src_pitch, tmp_pitch, cpp, i;
	const char *src;
	Bool ret;

	cpp = pspix->drawable.bitsPerPixel >> 3;
	src_pitch  = exaGetPixmapPitch(pspix);
	tmp_pitch = w * cpp;

	while (h) {
		const int lines = (h > 2047) ? 2047 : h;
		struct nouveau_bo *tmp;
		int tmp_offset;

		if (nouveau_exa_scratch(pNv, lines * tmp_pitch,
					&tmp, &tmp_offset))
			goto memcpy;

		if (!NVAccelM2MF(pNv, w, lines, cpp, 0, tmp_offset,
				 nouveau_pixmap_bo(pspix), NOUVEAU_BO_VRAM,
				 src_pitch, pspix->drawable.height, x, y,
				 tmp, NOUVEAU_BO_GART, tmp_pitch,
				 lines, 0, 0))
			goto memcpy;

		nouveau_bo_wait(tmp, NOUVEAU_BO_RD, pNv->client);
		if (src_pitch == tmp_pitch) {
			memcpy(dst, tmp->map + tmp_offset, dst_pitch * lines);
			dst += dst_pitch * lines;
		} else {
			src = tmp->map + tmp_offset;
			for (i = 0; i < lines; i++) {
				memcpy(dst, src, tmp_pitch);
				src += tmp_pitch;
				dst += dst_pitch;
			}
		}

		/* next! */
		h -= lines;
		y += lines;
	}

memcpy:
	bo = nouveau_pixmap_bo(pspix);
	if (nouveau_bo_map(bo, NOUVEAU_BO_RD, pNv->client))
		return FALSE;
	src = (char *)bo->map + (y * src_pitch) + (x * cpp);
	ret = NVAccelMemcpyRect(dst, src, h, dst_pitch, src_pitch, w*cpp);
	return ret;
}

static Bool
nouveau_exa_upload_to_screen(PixmapPtr pdpix, int x, int y, int w, int h,
			     char *src, int src_pitch)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pdpix->drawable.pScreen);
	NVPtr pNv = NVPTR(pScrn);
	int dst_pitch, tmp_pitch, cpp, i;
	struct nouveau_bo *bo;
	char *dst;
	Bool ret;

	cpp = pdpix->drawable.bitsPerPixel >> 3;
	dst_pitch  = exaGetPixmapPitch(pdpix);
	tmp_pitch = w * cpp;

	/* try hostdata transfer */
	if (w * h * cpp < 16*1024) /* heuristic */
	{
		if (pNv->Architecture < NV_ARCH_50) {
			if (NV04EXAUploadIFC(pScrn, src, src_pitch, pdpix,
					     x, y, w, h, cpp)) {
				exaMarkSync(pdpix->drawable.pScreen);
				return TRUE;
			}
		} else
		if (pNv->Architecture < NV_ARCH_C0) {
			if (NV50EXAUploadSIFC(src, src_pitch, pdpix,
					      x, y, w, h, cpp)) {
				exaMarkSync(pdpix->drawable.pScreen);
				return TRUE;
			}
		} else {
			if (NVC0EXAUploadSIFC(src, src_pitch, pdpix,
					      x, y, w, h, cpp)) {
				exaMarkSync(pdpix->drawable.pScreen);
				return TRUE;
			}
		}
	}

	while (h) {
		const int lines = (h > 2047) ? 2047 : h;
		struct nouveau_bo *tmp;
		int tmp_offset;

		if (nouveau_exa_scratch(pNv, lines * tmp_pitch,
					&tmp, &tmp_offset))
			goto memcpy;

		if (src_pitch == tmp_pitch) {
			memcpy(tmp->map + tmp_offset, src, src_pitch * lines);
			src += src_pitch * lines;
		} else {
			dst = tmp->map + tmp_offset;
			for (i = 0; i < lines; i++) {
				memcpy(dst, src, tmp_pitch);
				src += src_pitch;
				dst += tmp_pitch;
			}
		}

		if (!NVAccelM2MF(pNv, w, lines, cpp, tmp_offset, 0, tmp,
				 NOUVEAU_BO_GART, tmp_pitch, lines, 0, 0,
				 nouveau_pixmap_bo(pdpix), NOUVEAU_BO_VRAM,
				 dst_pitch, pdpix->drawable.height, x, y))
			goto memcpy;

		/* next! */
		h -= lines;
		y += lines;
	}

	exaMarkSync(pdpix->drawable.pScreen);
	return TRUE;

	/* fallback to memcpy-based transfer */
memcpy:
	bo = nouveau_pixmap_bo(pdpix);
	if (nouveau_bo_map(bo, NOUVEAU_BO_WR, pNv->client))
		return FALSE;
	dst = (char *)bo->map + (y * dst_pitch) + (x * cpp);
	ret = NVAccelMemcpyRect(dst, src, h, dst_pitch, src_pitch, w*cpp);
	return ret;
}

Bool
nouveau_exa_pixmap_is_onscreen(PixmapPtr ppix)
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(ppix->drawable.pScreen);

	if (pScrn->pScreen->GetScreenPixmap(pScrn->pScreen) == ppix)
		return TRUE;

	return FALSE;
}

Bool
nouveau_exa_init(ScreenPtr pScreen) 
{
	ScrnInfoPtr pScrn = xf86ScreenToScrn(pScreen);
	NVPtr pNv = NVPTR(pScrn);
	ExaDriverPtr exa;

	exa = exaDriverAlloc();
	if (!exa) {
		pNv->NoAccel = TRUE;
		return FALSE;
	}

	exa->exa_major = EXA_VERSION_MAJOR;
	exa->exa_minor = EXA_VERSION_MINOR;
	exa->flags = EXA_OFFSCREEN_PIXMAPS;

#ifdef EXA_SUPPORTS_PREPARE_AUX
	exa->flags |= EXA_SUPPORTS_PREPARE_AUX;
#endif

	exa->PixmapIsOffscreen = nouveau_exa_pixmap_is_offscreen;
	exa->PrepareAccess = nouveau_exa_prepare_access;
	exa->FinishAccess = nouveau_exa_finish_access;

	exa->flags |= (EXA_HANDLES_PIXMAPS | EXA_MIXED_PIXMAPS);
	exa->pixmapOffsetAlign = 256;
	exa->pixmapPitchAlign = 64;

	exa->CreatePixmap2 = nouveau_exa_create_pixmap;
	exa->DestroyPixmap = nouveau_exa_destroy_pixmap;

	if (pNv->Architecture >= NV_ARCH_50) {
		exa->maxX = 8192;
		exa->maxY = 8192;
	} else
	if (pNv->Architecture >= NV_ARCH_10) {
		exa->maxX = 4096;
		exa->maxY = 4096;
	} else {
		exa->maxX = 2048;
		exa->maxY = 2048;
	}

	exa->MarkSync = nouveau_exa_mark_sync;
	exa->WaitMarker = nouveau_exa_wait_marker;

	exa->DownloadFromScreen = nouveau_exa_download_from_screen;
	exa->UploadToScreen = nouveau_exa_upload_to_screen;

	if (pNv->Architecture < NV_ARCH_50) {
		exa->PrepareCopy = NV04EXAPrepareCopy;
		exa->Copy = NV04EXACopy;
		exa->DoneCopy = NV04EXADoneCopy;

		exa->PrepareSolid = NV04EXAPrepareSolid;
		exa->Solid = NV04EXASolid;
		exa->DoneSolid = NV04EXADoneSolid;
	} else
	if (pNv->Architecture < NV_ARCH_C0) {
		exa->PrepareCopy = NV50EXAPrepareCopy;
		exa->Copy = NV50EXACopy;
		exa->DoneCopy = NV50EXADoneCopy;

		exa->PrepareSolid = NV50EXAPrepareSolid;
		exa->Solid = NV50EXASolid;
		exa->DoneSolid = NV50EXADoneSolid;
	} else {
		exa->PrepareCopy = NVC0EXAPrepareCopy;
		exa->Copy        = NVC0EXACopy;
		exa->DoneCopy    = NVC0EXADoneCopy;

		exa->PrepareSolid = NVC0EXAPrepareSolid;
		exa->Solid        = NVC0EXASolid;
		exa->DoneSolid    = NVC0EXADoneSolid;
	}

	switch (pNv->Architecture) {	
	case NV_ARCH_10:
	case NV_ARCH_20:
 		exa->CheckComposite   = NV10EXACheckComposite;
 		exa->PrepareComposite = NV10EXAPrepareComposite;
 		exa->Composite        = NV10EXAComposite;
 		exa->DoneComposite    = NV10EXADoneComposite;
		break;
	case NV_ARCH_30:
		exa->CheckComposite   = NV30EXACheckComposite;
		exa->PrepareComposite = NV30EXAPrepareComposite;
		exa->Composite        = NV30EXAComposite;
		exa->DoneComposite    = NV30EXADoneComposite;
		break;
	case NV_ARCH_40:
		exa->CheckComposite   = NV40EXACheckComposite;
		exa->PrepareComposite = NV40EXAPrepareComposite;
		exa->Composite        = NV40EXAComposite;
		exa->DoneComposite    = NV40EXADoneComposite;
		break;
	case NV_ARCH_50:
		exa->CheckComposite   = NV50EXACheckComposite;
		exa->PrepareComposite = NV50EXAPrepareComposite;
		exa->Composite        = NV50EXAComposite;
		exa->DoneComposite    = NV50EXADoneComposite;
		break;
	case NV_ARCH_C0:
	case NV_ARCH_E0:
		exa->CheckComposite   = NVC0EXACheckComposite;
		exa->PrepareComposite = NVC0EXAPrepareComposite;
		exa->Composite        = NVC0EXAComposite;
		exa->DoneComposite    = NVC0EXADoneComposite;
		break;
	default:
		break;
	}

	if (!exaDriverInit(pScreen, exa))
		return FALSE;

	pNv->EXADriverPtr = exa;
	return TRUE;
}
