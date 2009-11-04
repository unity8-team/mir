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

static void *nouveau_exa_pixmap_map(PixmapPtr);
static void nouveau_exa_pixmap_unmap(PixmapPtr);

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

static inline Bool
NVAccelDownloadM2MF(PixmapPtr pspix, int x, int y, int w, int h,
		    char *dst, unsigned dst_pitch)
{
	ScrnInfoPtr pScrn = xf86Screens[pspix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *m2mf = pNv->NvMemFormat;
	struct nouveau_bo *bo = nouveau_pixmap_bo(pspix);
	unsigned src_offset = nouveau_pixmap_offset(pspix);
	unsigned cpp = pspix->drawable.bitsPerPixel / 8;
	unsigned line_len = w * cpp;
	unsigned src_pitch = 0, linear = 0;

	if (!nouveau_exa_pixmap_is_tiled(pspix)) {
		linear     = 1;
		src_pitch  = exaGetPixmapPitch(pspix);
		src_offset += (y * src_pitch) + (x * cpp);
	}

	while (h) {
		int line_count, i;
		char *src;

		if (h * line_len <= pNv->GART->size) {
			line_count = h;
		} else {
			line_count = pNv->GART->size / line_len;
			if (line_count > h)
				line_count = h;
		}

		/* HW limitations */
		if (line_count > 2047)
			line_count = 2047;

		if (MARK_RING(chan, 32, 6))
			return FALSE;

		BEGIN_RING(chan, m2mf, 0x184, 2);
		if (OUT_RELOCo(chan, bo, NOUVEAU_BO_GART | NOUVEAU_BO_VRAM |
			       NOUVEAU_BO_RD) ||
		    OUT_RELOCo(chan, pNv->GART, NOUVEAU_BO_GART |
			       NOUVEAU_BO_WR)) {
			MARK_UNDO(chan);
			return FALSE;
		}

		if (pNv->Architecture >= NV_ARCH_50) {
			if (!linear) {
				BEGIN_RING(chan, m2mf, 0x0200, 7);
				OUT_RING  (chan, 0);
				OUT_RING  (chan, bo->tile_mode << 4);
				OUT_RING  (chan, pspix->drawable.width * cpp);
				OUT_RING  (chan, pspix->drawable.height);
				OUT_RING  (chan, 1);
				OUT_RING  (chan, 0);
				OUT_RING  (chan, (y << 16) | (x * cpp));
			} else {
				BEGIN_RING(chan, m2mf, 0x0200, 1);
				OUT_RING  (chan, 1);
			}

			BEGIN_RING(chan, m2mf, 0x021c, 1);
			OUT_RING  (chan, 1);

			BEGIN_RING(chan, m2mf, 0x238, 2);
			if (OUT_RELOCh(chan, bo, src_offset, NOUVEAU_BO_GART |
				       NOUVEAU_BO_VRAM | NOUVEAU_BO_RD) ||
			    OUT_RELOCh(chan, pNv->GART, 0, NOUVEAU_BO_GART |
				       NOUVEAU_BO_WR)) {
				MARK_UNDO(chan);
				return FALSE;
			}
		}

		BEGIN_RING(chan, m2mf,
			   NV04_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
		if (OUT_RELOCl(chan, bo, src_offset, NOUVEAU_BO_GART |
			       NOUVEAU_BO_VRAM | NOUVEAU_BO_RD) ||
		    OUT_RELOCl(chan, pNv->GART, 0, NOUVEAU_BO_GART |
			       NOUVEAU_BO_WR)) {
			MARK_UNDO(chan);
			return FALSE;
		}
		OUT_RING  (chan, src_pitch);
		OUT_RING  (chan, line_len);
		OUT_RING  (chan, line_len);
		OUT_RING  (chan, line_count);
		OUT_RING  (chan, (1<<8)|1);
		OUT_RING  (chan, 0);

		if (nouveau_bo_map(pNv->GART, NOUVEAU_BO_RD)) {
			MARK_UNDO(chan);
			return FALSE;
		}
		src = pNv->GART->map;
		if (dst_pitch == line_len) {
			memcpy(dst, src, dst_pitch * line_count);
			dst += dst_pitch * line_count;
		} else {
			for (i = 0; i < line_count; i++) {
				memcpy(dst, src, line_len);
				src += line_len;
				dst += dst_pitch;
			}
		}
		nouveau_bo_unmap(pNv->GART);

		if (linear)
			src_offset += line_count * src_pitch;
		h -= line_count;
		y += line_count;
	}

	return TRUE;
}

static inline Bool
NVAccelUploadM2MF(PixmapPtr pdpix, int x, int y, int w, int h,
		  const char *src, int src_pitch)
{
	ScrnInfoPtr pScrn = xf86Screens[pdpix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *m2mf = pNv->NvMemFormat;
	struct nouveau_bo *bo = nouveau_pixmap_bo(pdpix);
	unsigned dst_offset = nouveau_pixmap_offset(pdpix);
	unsigned cpp = pdpix->drawable.bitsPerPixel / 8;
	unsigned line_len = w * cpp;
	unsigned dst_pitch = 0, linear = 0;

	if (!nouveau_exa_pixmap_is_tiled(pdpix)) {
		linear     = 1;
		dst_pitch  = exaGetPixmapPitch(pdpix);
		dst_offset += (y * dst_pitch) + (x * cpp);
	}

	while (h) {
		int line_count, i;
		char *dst;

		/* Determine max amount of data we can DMA at once */
		if (h * line_len <= pNv->GART->size) {
			line_count = h;
		} else {
			line_count = pNv->GART->size / line_len;
			if (line_count > h)
				line_count = h;
		}

		/* HW limitations */
		if (line_count > 2047)
			line_count = 2047;

		/* Upload to GART */
		if (nouveau_bo_map(pNv->GART, NOUVEAU_BO_WR))
			return FALSE;
		dst = pNv->GART->map;
		if (src_pitch == line_len) {
			memcpy(dst, src, src_pitch * line_count);
			src += src_pitch * line_count;
		} else {
			for (i = 0; i < line_count; i++) {
				memcpy(dst, src, line_len);
				src += src_pitch;
				dst += line_len;
			}
		}
		nouveau_bo_unmap(pNv->GART);

		if (MARK_RING(chan, 32, 6))
			return FALSE;

		BEGIN_RING(chan, m2mf, 0x184, 2);
		if (OUT_RELOCo(chan, pNv->GART, NOUVEAU_BO_GART |
			       NOUVEAU_BO_RD) ||
		    OUT_RELOCo(chan, bo, NOUVEAU_BO_VRAM | NOUVEAU_BO_GART |
			       NOUVEAU_BO_WR)) {
			MARK_UNDO(chan);
			return FALSE;
		}

		if (pNv->Architecture >= NV_ARCH_50) {
			BEGIN_RING(chan, m2mf, 0x0200, 1);
			OUT_RING  (chan, 1);

			if (!linear) {
				BEGIN_RING(chan, m2mf, 0x021c, 7);
				OUT_RING  (chan, 0);
				OUT_RING  (chan, bo->tile_mode << 4);
				OUT_RING  (chan, pdpix->drawable.width * cpp);
				OUT_RING  (chan, pdpix->drawable.height);
				OUT_RING  (chan, 1);
				OUT_RING  (chan, 0);
				OUT_RING  (chan, (y << 16) | (x * cpp));
			} else {
				BEGIN_RING(chan, m2mf, 0x021c, 1);
				OUT_RING  (chan, 1);
			}

			BEGIN_RING(chan, m2mf, 0x0238, 2);
			if (OUT_RELOCh(chan, pNv->GART, 0, NOUVEAU_BO_GART |
				       NOUVEAU_BO_RD) ||
			    OUT_RELOCh(chan, bo, dst_offset, NOUVEAU_BO_VRAM |
				       NOUVEAU_BO_GART | NOUVEAU_BO_WR)) {
				MARK_UNDO(chan);
				return FALSE;
			}
		}

		/* DMA to VRAM */
		BEGIN_RING(chan, m2mf,
			   NV04_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
		if (OUT_RELOCl(chan, pNv->GART, 0, NOUVEAU_BO_GART |
			       NOUVEAU_BO_RD) ||
		    OUT_RELOCl(chan, bo, dst_offset, NOUVEAU_BO_VRAM |
			       NOUVEAU_BO_GART | NOUVEAU_BO_WR)) {
			MARK_UNDO(chan);
			return FALSE;
		}
		OUT_RING  (chan, line_len);
		OUT_RING  (chan, dst_pitch);
		OUT_RING  (chan, line_len);
		OUT_RING  (chan, line_count);
		OUT_RING  (chan, (1<<8)|1);
		OUT_RING  (chan, 0);
		FIRE_RING (chan);

		if (linear)
			dst_offset += line_count * dst_pitch;
		h -= line_count;
		y += line_count;
	}

	return TRUE;
}

static int
nouveau_exa_mark_sync(ScreenPtr pScreen)
{
	return 0;
}

static void
nouveau_exa_wait_marker(ScreenPtr pScreen, int marker)
{
	NVPtr pNv = NVPTR(xf86Screens[pScreen->myNum]);
	
	if (!pNv->exa_driver_pixmaps)
		NVSync(xf86Screens[pScreen->myNum]);
}

static Bool
nouveau_exa_prepare_access(PixmapPtr ppix, int index)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_bo *bo;

	if (pNv->exa_driver_pixmaps) {
		void *map = nouveau_exa_pixmap_map(ppix);

		if (!map)
			return FALSE;

		ppix->devPrivate.ptr = map;
		return TRUE;
	} else
	if (ppix == pScrn->pScreen->GetScreenPixmap(pScrn->pScreen)) {
		nouveau_bo_map(pNv->scanout, NOUVEAU_BO_RDWR);
		ppix->devPrivate.ptr = pNv->scanout->map;
		nouveau_bo_unmap(pNv->scanout);
		return TRUE;
	} else
	if (drmmode_is_rotate_pixmap(ppix, &bo)) {
		nouveau_bo_map(bo, NOUVEAU_BO_RDWR);
		ppix->devPrivate.ptr = bo->map;
		nouveau_bo_unmap(bo);
		return TRUE;
	}

	return FALSE;
}

static void
nouveau_exa_finish_access(PixmapPtr ppix, int index)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	if (pNv->exa_driver_pixmaps) {
		nouveau_exa_pixmap_unmap(ppix);
	} else
	if (ppix == pScrn->pScreen->GetScreenPixmap(pScrn->pScreen) ||
	    drmmode_is_rotate_pixmap(ppix, NULL)) {
		ppix->devPrivate.ptr = NULL;
	}
}

static Bool
nouveau_exa_pixmap_is_offscreen(PixmapPtr ppix)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	if (pNv->exa_driver_pixmaps) {
		struct nouveau_pixmap *nvpix = nouveau_pixmap(ppix);

		if (nvpix && nvpix->bo)
			return TRUE;
	} else
	if (ppix->devPrivate.ptr >= pNv->offscreen_map &&
	    ppix->devPrivate.ptr < pNv->offscreen_map + pNv->offscreen->size)
		return TRUE;
	else
	if (ppix == pScrn->pScreen->GetScreenPixmap(pScrn->pScreen))
		return TRUE;
	else
	if (drmmode_is_rotate_pixmap(ppix, NULL))
		return TRUE;

	return FALSE;
}

static void *
nouveau_exa_create_pixmap(ScreenPtr pScreen, int width, int height, int depth,
			  int usage_hint, int bitsPerPixel, int *new_pitch)
{
	NVPtr pNv = NVPTR(xf86Screens[pScreen->myNum]);
	struct nouveau_pixmap *nvpix;
	uint32_t flags = NOUVEAU_BO_MAP, tile_mode = 0, tile_flags = 0;
	int ret, size, cpp = bitsPerPixel >> 3;

	if (!width || !height)
		return xcalloc(1, sizeof(*nvpix));

	if (!pNv->exa_force_cp &&
	     pNv->dev->vm_vram_size <= 32*1024*1024)
		return NULL;

	nvpix = xcalloc(1, sizeof(*nvpix));
	if (!nvpix)
		return NULL;

	if (cpp) {
		flags |= NOUVEAU_BO_VRAM;

		if (pNv->Architecture >= NV_ARCH_50) {
			if      (height > 32) tile_mode = 4;
			else if (height > 16) tile_mode = 3;
			else if (height >  8) tile_mode = 2;
			else if (height >  4) tile_mode = 1;
			else                  tile_mode = 0;

			if (usage_hint == NOUVEAU_CREATE_PIXMAP_ZETA)
				tile_flags = 0x2800;
			else
				tile_flags = 0x7000;

			height = NOUVEAU_ALIGN(height, 1 << (tile_mode + 2));
		}

		*new_pitch = width * cpp;
	} else {
		*new_pitch = (width * bitsPerPixel + 7) / 8;
	}

	*new_pitch = NOUVEAU_ALIGN(*new_pitch, 64);
	size  = *new_pitch * height;

	ret = nouveau_bo_new_tile(pNv->dev, flags, 0, size, tile_mode,
				  tile_flags, &nvpix->bo);
	if (ret) {
		xfree(nvpix);
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
	xfree(nvpix);
}

bool
nouveau_exa_pixmap_is_tiled(PixmapPtr ppix)
{
	if (!nouveau_pixmap_bo(ppix)->tile_flags)
		return false;

	return true;
}

static void *
nouveau_exa_pixmap_map(PixmapPtr ppix)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_bo *bo = nouveau_pixmap_bo(ppix);
	unsigned delta = nouveau_pixmap_offset(ppix);

	if (bo->tile_flags && !pNv->wfb_enabled) {
		struct nouveau_pixmap *nvpix = nouveau_pixmap(ppix);

		nvpix->map_refcount++;
		if (nvpix->linear)
			return nvpix->linear;

		nvpix->linear = xcalloc(1, ppix->devKind * ppix->drawable.height);

		NVAccelDownloadM2MF(ppix, 0, 0, ppix->drawable.width,
				    ppix->drawable.height, nvpix->linear,
				    ppix->devKind);

		nouveau_bo_map(bo, NOUVEAU_BO_RDWR);
		return nvpix->linear;
	}

	nouveau_bo_map(bo, NOUVEAU_BO_RDWR);
	return bo->map + delta;
}

static void
nouveau_exa_pixmap_unmap(PixmapPtr ppix)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_bo *bo = nouveau_pixmap_bo(ppix);

	if (bo->tile_flags && !pNv->wfb_enabled) {
		struct nouveau_pixmap *nvpix = nouveau_pixmap(ppix);

		if (--nvpix->map_refcount)
			return;

		NVAccelUploadM2MF(ppix, 0, 0, ppix->drawable.width,
				  ppix->drawable.height, nvpix->linear,
				  ppix->devKind);

		xfree(nvpix->linear);
		nvpix->linear = NULL;
	}

	nouveau_bo_unmap(bo);
}

static Bool
nouveau_exa_download_from_screen(PixmapPtr pspix, int x, int y, int w, int h,
				 char *dst, int dst_pitch)
{
	ScrnInfoPtr pScrn = xf86Screens[pspix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	int src_pitch, cpp, offset;
	const char *src;
	Bool ret;

	src_pitch  = exaGetPixmapPitch(pspix);
	cpp = pspix->drawable.bitsPerPixel >> 3;
	offset = (y * src_pitch) + (x * cpp);

	if (pNv->GART) {
		if (NVAccelDownloadM2MF(pspix, x, y, w, h, dst, dst_pitch))
			return TRUE;
	}

	src = nouveau_exa_pixmap_map(pspix);
	if (!src)
		return FALSE;
	src += offset;
	ret = NVAccelMemcpyRect(dst, src, h, dst_pitch, src_pitch, w*cpp);
	nouveau_exa_pixmap_unmap(pspix);
	return ret;
}

static Bool
nouveau_exa_upload_to_screen(PixmapPtr pdpix, int x, int y, int w, int h,
			     char *src, int src_pitch)
{
	ScrnInfoPtr pScrn = xf86Screens[pdpix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	int dst_pitch, cpp;
	char *dst;
	Bool ret;

	dst_pitch  = exaGetPixmapPitch(pdpix);
	cpp = pdpix->drawable.bitsPerPixel >> 3;

	/* try hostdata transfer */
	if (w * h * cpp < 16*1024) /* heuristic */
	{
		if (pNv->Architecture < NV_ARCH_50) {
			if (NV04EXAUploadIFC(pScrn, src, src_pitch, pdpix,
					     x, y, w, h, cpp)) {
				exaMarkSync(pdpix->drawable.pScreen);
				return TRUE;
			}
		} else {
			if (NV50EXAUploadSIFC(src, src_pitch, pdpix,
					      x, y, w, h, cpp)) {
				exaMarkSync(pdpix->drawable.pScreen);
				return TRUE;
			}
		}
	}

	/* try gart-based transfer */
	if (pNv->GART) {
		if (NVAccelUploadM2MF(pdpix, x, y, w, h, src, src_pitch)) {
			exaMarkSync(pdpix->drawable.pScreen);
			return TRUE;
		}
	}

	/* fallback to memcpy-based transfer */
	dst = nouveau_exa_pixmap_map(pdpix);
	if (!dst)
		return FALSE;
	dst += (y * dst_pitch) + (x * cpp);
	ret = NVAccelMemcpyRect(dst, src, h, dst_pitch, src_pitch, w*cpp);
	nouveau_exa_pixmap_unmap(pdpix);
	return ret;
}

Bool
nouveau_exa_pixmap_is_onscreen(PixmapPtr ppix)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];

	if (pScrn->pScreen->GetScreenPixmap(pScrn->pScreen) == ppix)
		return TRUE;

	return FALSE;
}

Bool
nouveau_exa_init(ScreenPtr pScreen) 
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
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

#if (EXA_VERSION_MAJOR == 2 && EXA_VERSION_MINOR >= 5) || EXA_VERSION_MAJOR > 2
	if (pNv->exa_driver_pixmaps) {
		exa->flags |= (EXA_HANDLES_PIXMAPS | EXA_MIXED_PIXMAPS);
		exa->pixmapOffsetAlign = 256;
		exa->pixmapPitchAlign = 64;

		exa->CreatePixmap2 = nouveau_exa_create_pixmap;
		exa->DestroyPixmap = nouveau_exa_destroy_pixmap;
	} else
#endif
	{
		exa->memoryBase = pNv->offscreen_map;
		exa->memorySize = pNv->offscreen->size;
		exa->offScreenBase = 0;

		if (pNv->Architecture < NV_ARCH_50) {
			exa->pixmapOffsetAlign = 256;
		} else {
			/* Workaround some corruption issues caused by exa's
			 * offscreen memory allocation no understanding G8x/G9x
			 * memory layout.  This is terrible, but it should
			 * prevent all but the most unlikely cases from
			 * occuring.
			 *
			 * See http://nouveau.freedesktop.org/wiki/NV50Support
			 * for a far better fix until driver pixmaps are ready
			 * to be used.
			 */
			exa->pixmapOffsetAlign = 65536;
			exa->flags |= EXA_OFFSCREEN_ALIGN_POT;
			exa->offScreenBase =
				NOUVEAU_ALIGN(exa->offScreenBase, 0x10000);
		}
		exa->pixmapPitchAlign = 64;
	}

	if (pNv->Architecture >= NV_ARCH_50) {
		exa->maxX = 8192;
		exa->maxY = 8192;
	} else
	if (pNv->Architecture >= NV_ARCH_20) {
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
	} else {
		exa->PrepareCopy = NV50EXAPrepareCopy;
		exa->Copy = NV50EXACopy;
		exa->DoneCopy = NV50EXADoneCopy;

		exa->PrepareSolid = NV50EXAPrepareSolid;
		exa->Solid = NV50EXASolid;
		exa->DoneSolid = NV50EXADoneSolid;
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
	default:
		break;
	}

	if (!exaDriverInit(pScreen, exa))
		return FALSE;
	else
		/* EXA init catches this, but only for xserver >= 1.4 */
		if (pNv->VRAMPhysicalSize / 2 < NOUVEAU_ALIGN(pScrn->virtualX, 64) * NOUVEAU_ALIGN(pScrn->virtualY, 64) * (pScrn->bitsPerPixel >> 3)) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "The virtual screen size's resolution is too big for the video RAM framebuffer at this colour depth.\n");
			return FALSE;
		}

	pNv->EXADriverPtr = exa;
	return TRUE;
}
