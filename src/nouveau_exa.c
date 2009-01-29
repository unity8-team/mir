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
	unsigned cpp = pspix->drawable.bitsPerPixel / 8;
	unsigned line_len = w * cpp;
	unsigned src_pitch = 0, src_offset = 0, linear = 0;

	if (pNv->Architecture < NV_ARCH_50 ||
	    exaGetPixmapOffset(pspix) < pNv->EXADriverPtr->offScreenBase) {
		linear     = 1;
		src_pitch  = exaGetPixmapPitch(pspix);
		src_offset = (y * src_pitch) + (x * cpp);
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

		WAIT_RING (chan, 32);
		BEGIN_RING(chan, m2mf, 0x184, 2);
		OUT_PIXMAPo(chan, pspix,
			    NOUVEAU_BO_GART | NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
		OUT_RELOCo(chan, pNv->GART, NOUVEAU_BO_GART | NOUVEAU_BO_WR);

		if (pNv->Architecture >= NV_ARCH_50) {
			if (!linear) {
				BEGIN_RING(chan, m2mf, 0x0200, 7);
				OUT_RING  (chan, 0);
				OUT_RING  (chan, 0);
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
			OUT_PIXMAPh(chan, pspix, src_offset, NOUVEAU_BO_GART |
				    NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
			OUT_RELOCh(chan, pNv->GART, 0, NOUVEAU_BO_GART |
				   NOUVEAU_BO_WR);
		}

		BEGIN_RING(chan, m2mf,
			   NV04_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
		OUT_PIXMAPl(chan, pspix, src_offset, NOUVEAU_BO_GART |
			    NOUVEAU_BO_VRAM | NOUVEAU_BO_RD);
		OUT_RELOCl(chan, pNv->GART, 0, NOUVEAU_BO_GART | NOUVEAU_BO_WR);
		OUT_RING  (chan, src_pitch);
		OUT_RING  (chan, line_len);
		OUT_RING  (chan, line_len);
		OUT_RING  (chan, line_count);
		OUT_RING  (chan, (1<<8)|1);
		OUT_RING  (chan, 0);

		nouveau_notifier_reset(pNv->notify0, 0);
		BEGIN_RING(chan, m2mf, NV04_MEMORY_TO_MEMORY_FORMAT_NOTIFY, 1);
		OUT_RING  (chan, 0);
		BEGIN_RING(chan, m2mf, 0x100, 1);
		OUT_RING  (chan, 0);
		FIRE_RING (chan);
		if (nouveau_notifier_wait_status(pNv->notify0, 0, 0, 2000))
			return FALSE;

		nouveau_bo_map(pNv->GART, NOUVEAU_BO_RD);
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
	unsigned cpp = pdpix->drawable.bitsPerPixel / 8;
	unsigned line_len = w * cpp;
	unsigned dst_pitch = 0, dst_offset = 0, linear = 0;

	if (pNv->Architecture < NV_ARCH_50 ||
	    exaGetPixmapOffset(pdpix) < pNv->EXADriverPtr->offScreenBase) {
		linear     = 1;
		dst_pitch  = exaGetPixmapPitch(pdpix);
		dst_offset = (y * dst_pitch) + (x * cpp);
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
		nouveau_bo_map(pNv->GART, NOUVEAU_BO_WR);
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

		WAIT_RING (chan, 32);
		BEGIN_RING(chan, m2mf, 0x184, 2);
		OUT_RELOCo(chan, pNv->GART, NOUVEAU_BO_GART | NOUVEAU_BO_RD);
		OUT_PIXMAPo(chan, pdpix,
			    NOUVEAU_BO_VRAM | NOUVEAU_BO_GART | NOUVEAU_BO_WR);

		if (pNv->Architecture >= NV_ARCH_50) {
			BEGIN_RING(chan, m2mf, 0x0200, 1);
			OUT_RING  (chan, 1);

			if (!linear) {
				BEGIN_RING(chan, m2mf, 0x021c, 7);
				OUT_RING  (chan, 0);
				OUT_RING  (chan, 0);
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
			OUT_RELOCh(chan, pNv->GART, 0, NOUVEAU_BO_GART |
				   NOUVEAU_BO_RD);
			OUT_PIXMAPh(chan, pdpix, dst_offset, NOUVEAU_BO_VRAM | 
				    NOUVEAU_BO_GART | NOUVEAU_BO_WR);
		}

		/* DMA to VRAM */
		BEGIN_RING(chan, m2mf,
			   NV04_MEMORY_TO_MEMORY_FORMAT_OFFSET_IN, 8);
		OUT_RELOCl(chan, pNv->GART, 0, NOUVEAU_BO_GART | NOUVEAU_BO_RD);
		OUT_PIXMAPl(chan, pdpix, dst_offset, NOUVEAU_BO_VRAM |
			    NOUVEAU_BO_GART | NOUVEAU_BO_WR);
		OUT_RING  (chan, line_len);
		OUT_RING  (chan, dst_pitch);
		OUT_RING  (chan, line_len);
		OUT_RING  (chan, line_count);
		OUT_RING  (chan, (1<<8)|1);
		OUT_RING  (chan, 0);

		nouveau_notifier_reset(pNv->notify0, 0);
		BEGIN_RING(chan, m2mf, NV04_MEMORY_TO_MEMORY_FORMAT_NOTIFY, 1);
		OUT_RING  (chan, 0);
		BEGIN_RING(chan, m2mf, 0x100, 1);
		OUT_RING  (chan, 0);
		FIRE_RING (chan);
		if (nouveau_notifier_wait_status(pNv->notify0, 0, 0, 2000))
			return FALSE;

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
	NVSync(xf86Screens[pScreen->myNum]);
}

static void *
nouveau_exa_pixmap_map(PixmapPtr ppix)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	void *map;

	exaWaitSync(ppix->drawable.pScreen);
	nouveau_bo_map(pNv->FB, NOUVEAU_BO_RDWR);
	map = pNv->FB->map + exaGetPixmapOffset(ppix);
	return map;
}

static void
nouveau_exa_pixmap_unmap(PixmapPtr ppix)
{
	ScrnInfoPtr pScrn = xf86Screens[ppix->drawable.pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);

	nouveau_bo_unmap(pNv->FB);
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
		if (NVAccelUploadM2MF(pdpix, x, y, w, h, src, src_pitch))
			return TRUE;
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
	NVPtr pNv = NVPTR(pScrn);
	unsigned long offset = exaGetPixmapOffset(ppix);

	if (offset < pNv->EXADriverPtr->offScreenBase)
		return TRUE;

	return FALSE;
}

Bool
nouveau_exa_init(ScreenPtr pScreen) 
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	ExaDriverPtr exa;

	if(!(exa = (ExaDriverPtr)xnfcalloc(sizeof(ExaDriverRec), 1))) {
		pNv->NoAccel = TRUE;
		return FALSE;
	}

	exa->exa_major = EXA_VERSION_MAJOR;
	exa->exa_minor = EXA_VERSION_MINOR;
	exa->flags = EXA_OFFSCREEN_PIXMAPS;

	if (pNv->Architecture < NV_ARCH_50) {
		exa->pixmapOffsetAlign = 256; 
	} else {
		/* Workaround some corruption issues caused by exa's
		 * offscreen memory allocation no understanding G8x/G9x
		 * memory layout.  This is terrible, but it should
		 * prevent all but the most unlikely cases from occuring.
		 *
		 * See http://nouveau.freedesktop.org/wiki/NV50Support for
		 * a far better fix until the ng branch is ready to be used.
		 */
		exa->pixmapOffsetAlign = 65536;
		exa->flags |= EXA_OFFSCREEN_ALIGN_POT;
	}
	exa->pixmapPitchAlign = 64;

	nouveau_bo_map(pNv->FB, NOUVEAU_BO_RDWR);
	exa->memoryBase = pNv->FB->map;
	nouveau_bo_unmap(pNv->FB);
	exa->offScreenBase = NOUVEAU_ALIGN(pScrn->virtualX, 64) *
					   NOUVEAU_ALIGN(pScrn->virtualY, 64) *
					   (pScrn->bitsPerPixel / 8);
	exa->memorySize = pNv->FB->size; 

	if (pNv->Architecture >= NV_ARCH_50) {
		struct nouveau_device_priv *nvdev = nouveau_device(pNv->dev);
		struct nouveau_bo_priv *nvbo = nouveau_bo(pNv->FB);
		struct drm_nouveau_mem_tile t;

		t.offset = nvbo->drm.offset;
		t.flags  = nvbo->drm.flags | NOUVEAU_MEM_TILE;
		t.delta  = exa->offScreenBase;
		t.size   = exa->memorySize - 
			   exa->offScreenBase;
		drmCommandWrite(nvdev->fd, DRM_NOUVEAU_MEM_TILE, &t, sizeof(t));

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
