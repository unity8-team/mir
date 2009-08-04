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

static PixmapPtr
nouveau_dri2_create_pixmap(ScreenPtr pScreen, DrawablePtr pDraw, bool zeta)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	PixmapPtr ppix;
	struct nouveau_bo *bo = NULL;
	uint32_t flags, aw = pDraw->width, ah = pDraw->height, pitch;
	int ret;

	flags = NOUVEAU_BO_VRAM;
	if (pNv->Architecture >= NV_ARCH_50) {
		aw = (aw + 7) & ~7;
		ah = (ah + 7) & ~7;

		flags |= NOUVEAU_BO_TILED;
		if (zeta)
			flags |= NOUVEAU_BO_ZTILE;
	}

	pitch = NOUVEAU_ALIGN(aw * (pDraw->bitsPerPixel >> 3), 64);

	ret = nouveau_bo_new(pNv->dev, flags | NOUVEAU_BO_MAP, 0,
			     pitch * ah, &bo);
	if (ret)
		return NULL;

	ppix = pScreen->CreatePixmap(pScreen, 0, 0, pDraw->depth, 0);
	if (!ppix) {
		nouveau_bo_ref(NULL, &bo);
		return NULL;
	}

	nouveau_bo_ref(bo, &nouveau_pixmap(ppix)->bo);
	nouveau_bo_ref(NULL, &bo);

	miModifyPixmapHeader(ppix, pDraw->width, pDraw->height, pDraw->depth,
			     pScrn->bitsPerPixel, pitch, NULL);
	return ppix;
}

DRI2BufferPtr
nouveau_dri2_create_buffer(DrawablePtr pDraw, unsigned int attachment,
			   unsigned int format)
{
	ScreenPtr pScreen = pDraw->pScreen;
	struct nouveau_dri2_buffer *nvbuf;
	PixmapPtr ppix;

	nvbuf = xcalloc(1, sizeof(*nvbuf));
	if (!nvbuf)
		return NULL;

	switch (attachment) {
	case DRI2BufferFrontLeft:
		if (pDraw->type == DRAWABLE_PIXMAP) {
			ppix = (PixmapPtr)pDraw;
		} else {
			WindowPtr pwin = (WindowPtr)pDraw;
			ppix = pScreen->GetWindowPixmap(pwin);
		}

		ppix->refcnt++;
		break;
	case DRI2BufferDepth:
	case DRI2BufferDepthStencil:
		ppix = nouveau_dri2_create_pixmap(pScreen, pDraw, true);
		break;
	default:
		ppix = nouveau_dri2_create_pixmap(pScreen, pDraw, false);
		break;
	}


	nvbuf->base.attachment = attachment;
	nvbuf->base.pitch = ppix->devKind;
	nvbuf->base.cpp = ppix->drawable.bitsPerPixel / 8;
	nvbuf->base.driverPrivate = nvbuf;
	nvbuf->base.format = format;
	nvbuf->base.flags = 0;
	nvbuf->ppix = ppix;

	nouveau_bo_handle_get(nouveau_pixmap(ppix)->bo, &nvbuf->base.name);
	return &nvbuf->base;
}

void
nouveau_dri2_destroy_buffer(DrawablePtr pDraw, DRI2BufferPtr buf)
{
	struct nouveau_dri2_buffer *nvbuf;

	nvbuf = nouveau_dri2_buffer(buf);
	if (!nvbuf)
		return;

	pDraw->pScreen->DestroyPixmap(nvbuf->ppix);
	xfree(nvbuf);
}

void
nouveau_dri2_copy_region(DrawablePtr pDraw, RegionPtr pRegion,
			 DRI2BufferPtr pDstBuffer, DRI2BufferPtr pSrcBuffer)
{
	struct nouveau_dri2_buffer *src = nouveau_dri2_buffer(pSrcBuffer);
	struct nouveau_dri2_buffer *dst = nouveau_dri2_buffer(pDstBuffer);
	PixmapPtr pspix = src->ppix, pdpix = dst->ppix;
	ScreenPtr pScreen = pDraw->pScreen;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	RegionPtr pCopyClip;
	GCPtr pGC;

	if (src->base.attachment == DRI2BufferFrontLeft)
		pspix = (PixmapPtr)pDraw;
	if (dst->base.attachment == DRI2BufferFrontLeft)
		pdpix = (PixmapPtr)pDraw;

	pGC = GetScratchGC(pDraw->depth, pScreen);
	pCopyClip = REGION_CREATE(pScreen, NULL, 0);
	REGION_COPY(pScreen, pCopyClip, pRegion);
	pGC->funcs->ChangeClip(pGC, CT_REGION, pCopyClip, 0);
	ValidateGC(&pdpix->drawable, pGC);

	pGC->ops->CopyArea(&pspix->drawable, &pdpix->drawable, pGC, 0, 0,
			   pDraw->width, pDraw->height, 0, 0);

	FreeScratchGC(pGC);

	FIRE_RING(pNv->chan);
}

Bool
nouveau_dri2_init(ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	DRI2InfoRec dri2;
	char *bus_id, *tmp_bus_id;
	int cmp, i, fd;

	/* The whole drmOpen thing is a fiasco and we need to find a way
	 * back to just using open(2).  For now, however, lets just make
	 * things worse with even more ad hoc directory walking code to
	 * discover the device file name. */
	bus_id = DRICreatePCIBusID(pNv->PciInfo);
	for (i = 0; i < DRM_MAX_MINOR; i++) {
		sprintf(pNv->drm_device_name, DRM_DEV_NAME, DRM_DIR_NAME, i);
		fd = open(pNv->drm_device_name, O_RDWR);
		if (fd < 0)
			continue;

		tmp_bus_id = drmGetBusid(fd);
		close(fd);
		if (tmp_bus_id == NULL)
			continue;

		cmp = strcmp(tmp_bus_id, bus_id);
		drmFree(tmp_bus_id);
		if (cmp == 0)
			break;
	}
	xfree(bus_id);

	if (i == DRM_MAX_MINOR) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "DRI2: failed to open drm device\n");
		return FALSE;
	}

	dri2.fd = nouveau_device(pNv->dev)->fd;
	dri2.driverName = "nouveau";
	dri2.deviceName = pNv->drm_device_name;

	dri2.version = DRI2INFOREC_VERSION;
	dri2.CreateBuffer = nouveau_dri2_create_buffer;
	dri2.DestroyBuffer = nouveau_dri2_destroy_buffer;
	dri2.CopyRegion = nouveau_dri2_copy_region;

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

