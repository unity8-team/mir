#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "xorg-server.h"
#ifdef DRI2
#include "nv_include.h"
#include "dri2.h"

struct nouveau_dri2_buffer {
	PixmapPtr pPixmap;
};

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
nouveau_dri2_create_buffers(DrawablePtr pDraw, unsigned int *attachments,
			    int count)
{
	ScreenPtr pScreen = pDraw->pScreen;
	DRI2BufferPtr dri2_bufs;
	struct nouveau_dri2_buffer *nv_bufs;
	PixmapPtr ppix, pzpix;
	int i;

	dri2_bufs = xcalloc(count, sizeof(*dri2_bufs));
	if (!dri2_bufs)
		return NULL;

	nv_bufs = xcalloc(count, sizeof(*nv_bufs));
	if (!nv_bufs) {
		xfree(dri2_bufs);
		return NULL;
	}

	pzpix = NULL;
	for (i = 0; i < count; i++) {
		if (attachments[i] == DRI2BufferFrontLeft) {
			if (pDraw->type == DRAWABLE_PIXMAP) {
				ppix = (PixmapPtr)pDraw;
			} else {
				WindowPtr pwin = (WindowPtr)pDraw;
				ppix = pScreen->GetWindowPixmap(pwin);
			}

			ppix->refcnt++;
		} else
		if (attachments[i] == DRI2BufferStencil && pzpix) {
			ppix = pzpix;
			ppix->refcnt++;
		} else {
			bool zeta;

			if (attachments[i] == DRI2BufferDepth ||
			    attachments[i] == DRI2BufferStencil)
				zeta = true;
			else
				zeta = false;

			ppix = nouveau_dri2_create_pixmap(pScreen, pDraw, zeta);
		}

		if (attachments[i] == DRI2BufferDepth)
			pzpix = ppix;

		dri2_bufs[i].attachment = attachments[i];
		dri2_bufs[i].pitch = ppix->devKind;
		dri2_bufs[i].cpp = ppix->drawable.bitsPerPixel / 8;
		dri2_bufs[i].driverPrivate = &nv_bufs[i];
		dri2_bufs[i].flags = 0;
		nv_bufs[i].pPixmap = ppix;

		nouveau_bo_handle_get(nouveau_pixmap(ppix)->bo,
				      &dri2_bufs[i].name);
	}

	return dri2_bufs;
}

void
nouveau_dri2_destroy_buffers(DrawablePtr pDraw, DRI2BufferPtr buffers,
			     int count)
{
	struct nouveau_dri2_buffer *nvbuf;

	while (count--) {
		nvbuf = buffers[count].driverPrivate;
		pDraw->pScreen->DestroyPixmap(nvbuf->pPixmap);
	}

	if (buffers) {
		xfree(buffers[0].driverPrivate);
		xfree(buffers);
	}
}

void
nouveau_dri2_copy_region(DrawablePtr pDraw, RegionPtr pRegion,
			 DRI2BufferPtr pDstBuffer, DRI2BufferPtr pSrcBuffer)
{
	struct nouveau_dri2_buffer *nvbuf = pSrcBuffer->driverPrivate;
	ScreenPtr pScreen = pDraw->pScreen;
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	NVPtr pNv = NVPTR(pScrn);
	RegionPtr pCopyClip;
	GCPtr pGC;

	pGC = GetScratchGC(pDraw->depth, pScreen);
	pCopyClip = REGION_CREATE(pScreen, NULL, 0);
	REGION_COPY(pScreen, pCopyClip, pRegion);
	pGC->funcs->ChangeClip(pGC, CT_REGION, pCopyClip, 0);
	ValidateGC(pDraw, pGC);
	pGC->ops->CopyArea(&nvbuf->pPixmap->drawable, pDraw, pGC, 0, 0,
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

	dri2.version = 1;
	dri2.fd = nouveau_device(pNv->dev)->fd;
	dri2.driverName = "nouveau";
	dri2.deviceName = pNv->drm_device_name;
	dri2.CreateBuffers = nouveau_dri2_create_buffers;
	dri2.DestroyBuffers = nouveau_dri2_destroy_buffers;
	dri2.CopyRegion = nouveau_dri2_copy_region;

	return DRI2ScreenInit(pScreen, &dri2);
}

void
nouveau_dri2_fini(ScreenPtr pScreen)
{
	DRI2CloseScreen(pScreen);
}
#endif
