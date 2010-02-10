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

DRI2BufferPtr
nouveau_dri2_create_buffer(DrawablePtr pDraw, unsigned int attachment,
			   unsigned int format)
{
	ScreenPtr pScreen = pDraw->pScreen;
	NVPtr pNv = NVPTR(xf86Screens[pScreen->myNum]);
	struct nouveau_dri2_buffer *nvbuf;
	PixmapPtr ppix;

	nvbuf = xcalloc(1, sizeof(*nvbuf));
	if (!nvbuf)
		return NULL;

	if (attachment == DRI2BufferFrontLeft) {
		if (pDraw->type == DRAWABLE_PIXMAP) {
			ppix = (PixmapPtr)pDraw;
		} else {
			WindowPtr pwin = (WindowPtr)pDraw;
			ppix = pScreen->GetWindowPixmap(pwin);
		}

		ppix->refcnt++;
	} else {
		unsigned int usage_hint = NOUVEAU_CREATE_PIXMAP_TILED;

		if (attachment == DRI2BufferDepth ||
		    attachment == DRI2BufferDepthStencil)
			usage_hint |= NOUVEAU_CREATE_PIXMAP_ZETA;

		ppix = pScreen->CreatePixmap(pScreen, pDraw->width,
					     pDraw->height, pDraw->depth,
					     usage_hint);
	}

	pNv->exa_force_cp = TRUE;
	exaMoveInPixmap(ppix);
	pNv->exa_force_cp = FALSE;

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

	if (pNv->Architecture >= NV_ARCH_30)
		dri2.driverName = "nouveau";
	else
		dri2.driverName = "nouveau_vieux";

	dri2.fd = nouveau_device(pNv->dev)->fd;
	dri2.deviceName = pNv->drm_device_name;

	dri2.version = DRI2INFOREC_VERSION;
	dri2.CreateBuffer = nouveau_dri2_create_buffer;
	dri2.DestroyBuffer = nouveau_dri2_destroy_buffer;
	dri2.CopyRegion = nouveau_dri2_copy_region;
#if DRI2INFOREC_VERSION >= 4
	dri2.ScheduleSwap = NULL;
	dri2.ScheduleWaitMSC = NULL;
	dri2.GetMSC = NULL;
	dri2.numDrivers = 0;
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

