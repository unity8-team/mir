
#include "nv_include.h"
#include "nvreg.h"

CARD32 NVDmaCreateDMAObject(NVPtr pNv, int target, CARD32 base_address, CARD32 size, int access)
{
	drm_nouveau_dma_object_init_t dma;
	CARD32 object;

	dma.instance = &object;
	dma.access   = access;
	dma.target   = target;
	dma.size     = size;
	dma.offset   = base_address;
	if (target == NV_DMA_TARGET_AGP)
		dma.offset += pNv->agpPhysical;
	drmCommandWriteRead(pNv->drm_fd, DRM_NOUVEAU_DMA_OBJECT_INIT, &dma, sizeof(dma));

    return object;
}

/*
   A DMA notifier is a DMA object that references a small (32 byte it
   seems, we use 256 for saftey) memory area that will be used by the HW to give feedback
   about a DMA operation.
*/
CARD32 NVDmaCreateNotifier(NVPtr pNv, int target, CARD32 base_address)
{
    return NVDmaCreateDMAObject(pNv, target, base_address, 0x100, NV_DMA_ACCES_RW);
}

/*
  How do we wait for DMA completion (by notifiers) ?

   Either repeatedly read the notifier address and wait until it changes,
   or enable a 'wakeup' interrupt by writing NOTIFY_WRITE_LE_AWAKEN into
   the 'notify' field of the object in the channel.  My guess is that 
   this causes an interrupt in PGRAPH/NOTIFY as soon as the transfer is
   completed.  Clients probably can use poll on the nv* devices to get this 
   event.  All this is a guess.	 I don't know any details, and I have not
   tested is.  Also, I have no idea how the 'nvdriver' reacts if it gets 
   notify events that are not registered.

   Writing NV_NOTIFY_WRITE_LE_AWAKEN into the 'Notify' field of an object
   in a channel really causes an interrupt in the PGRAPH engine.  Thus
   we can determine whether a DMA transfer has finished in the interrupt
   handler.

   We can't use interrupts in user land, so we do the simple polling approach.
   The method returns FALSE in case of an error.
*/
Bool NVDmaWaitForNotifier(NVPtr pNv, int target, CARD32 base_address)
{
    volatile U032 *n;
    unsigned char *notifier = (target == NV_DMA_TARGET_AGP)
                              ? pNv->agpMemory
                              : pNv->FbBase;
    notifier += base_address;
    n = (volatile U032 *)notifier;
    NVDEBUG("NVDmaWaitForNotifier @%p", n);
    while (1) {
        U032 a = n[0];
        U032 b = n[1];
        U032 c = n[2];
        U032 status = n[3];
        NVDEBUG("status: n[0]=%x, n[1]=%x, n[2]=%x, n[3]=%x\n", a, b, c, status);
        if (status == 0xffffffff)
            continue;
        if (!status)
            break;
        if (status & 0xffff)
            return FALSE;
    }
    return TRUE;
}

void NVDmaCreateContextObject(NVPtr pNv, int handle, int class, CARD32 flags,
                              CARD32 dma_in, CARD32 dma_out, CARD32 dma_notifier)
{
	drm_nouveau_object_init_t cto;
    CARD32 nv_flags0 = 0, nv_flags1 = 0, nv_flags2 = 0;
    
    if (pNv->Architecture >= NV_ARCH_40) {
        if (flags & NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND)
            nv_flags0 |= 0x02080000;
        else if (flags & NV_DMA_CONTEXT_FLAGS_PATCH_SRCCOPY)
            nv_flags0 |= 0x02080000;
        if (flags & NV_DMA_CONTEXT_FLAGS_CLIP_ENABLE)
            nv_flags0 |= 0x00020000;
#if X_BYTE_ORDER == X_BIG_ENDIAN
        if (flags & NV_DMA_CONTEXT_FLAGS_MONO)
            nv_flags1 |= 0x01000000;
        nv_flags2 |= 0x01000000;
#else
        if (flags & NV_DMA_CONTEXT_FLAGS_MONO)
            nv_flags1 |= 0x02000000;
#endif
    } else {
        if (flags & NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND)
            nv_flags0 |= 0x01008000;
        else if (flags & NV_DMA_CONTEXT_FLAGS_PATCH_SRCCOPY)
            nv_flags0 |= 0x01018000;
        if (flags & NV_DMA_CONTEXT_FLAGS_CLIP_ENABLE)
            nv_flags0 |= 0x00002000;
#if X_BYTE_ORDER == X_BIG_ENDIAN
        nv_flags0 |= 0x00080000;
        if (flags & NV_DMA_CONTEXT_FLAGS_MONO)
            nv_flags1 |= 0x00000001;
#else
        if (flags & NV_DMA_CONTEXT_FLAGS_MONO)
            nv_flags1 |= 0x00000002;
#endif
    }

	cto.handle = handle;
	cto.class  = class;
	cto.flags0 = nv_flags0;
	cto.flags1 = nv_flags1;
	cto.flags2 = nv_flags2;
	cto.dma_in       = dma_in;
	cto.dma_out      = dma_out;
	cto.dma_notifier = dma_notifier;
	drmCommandWrite(pNv->drm_fd, DRM_NOUVEAU_OBJECT_INIT, &cto, sizeof(cto));
}
                           
Bool NVInitDma(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    CARD32 dma_fb;
#ifdef XF86DRI
    CARD32 dma_agp;
    CARD32 dma_notifier;
#endif
	drm_nouveau_fifo_init_t fifo_init;
	unsigned int *fifo_regs, *fifo_cmdbuf;
	int fifo;
	int i;

	if (!NVDRIScreenInit(pScrn))
		return FALSE;

	fifo_init.fifo_num = &fifo;
	if (drmCommandWriteRead(pNv->drm_fd, DRM_NOUVEAU_FIFO_INIT, &fifo_init, sizeof(fifo_init)) != 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Could not initialise kernel module\n");
		return FALSE;
	}

    dma_fb = NVDmaCreateDMAObject(pNv, NV_DMA_TARGET_VIDMEM, 0, pNv->FbMapSize, NV_DMA_ACCES_RW);
    
    NVDmaCreateContextObject (pNv, NvContextSurfaces,
                              (pNv->Architecture >= NV_ARCH_10) ? NV10_CONTEXT_SURFACES_2D : NV4_SURFACE,
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND,
                              dma_fb, dma_fb, 0);
    NVDmaCreateContextObject (pNv, NvRop,
                              NV_ROP5_SOLID, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND,
                              0, 0, 0);
    NVDmaCreateContextObject (pNv, NvImagePattern,
                              NV4_IMAGE_PATTERN, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND|NV_DMA_CONTEXT_FLAGS_MONO,
                              0, 0, 0);
    NVDmaCreateContextObject (pNv, NvClipRectangle,
                              NV_IMAGE_BLACK_RECTANGLE, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND,
                              0, 0, 0);
    NVDmaCreateContextObject (pNv, NvSolidLine,
                              NV4_RENDER_SOLID_LIN, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND|NV_DMA_CONTEXT_FLAGS_CLIP_ENABLE,
                              0, 0, 0);
    NVDmaCreateContextObject (pNv, NvImageBlit,
                              pNv->WaitVSyncPossible ? NV12_IMAGE_BLIT : NV_IMAGE_BLIT,
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND, 
                              dma_fb, dma_fb, 0);
    NVDmaCreateContextObject (pNv, NvRectangle,
                              NV4_GDI_RECTANGLE_TEXT, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_ROP_AND|NV_DMA_CONTEXT_FLAGS_MONO, 
                              0, 0, 0);
    NVDmaCreateContextObject (pNv, NvScaledImage,
                              NV_SCALED_IMAGE_FROM_MEMORY, 
                              NV_DMA_CONTEXT_FLAGS_PATCH_SRCCOPY, 
                              dma_fb, dma_fb, 0);

#ifdef XF86DRI
    if (NVInitAGP(pScrn) && pNv->agpMemory) {

        dma_agp = NVDmaCreateDMAObject(pNv, NV_DMA_TARGET_AGP, 0x10000, pNv->agpSize - 0x10000,
                                       NV_DMA_ACCES_RW);
        dma_notifier = NVDmaCreateNotifier(pNv, NV_DMA_TARGET_AGP, 0);
        
        NVDmaCreateContextObject (pNv, NvGraphicsToAGP,
                                  NV_MEMORY_TO_MEMORY_FORMAT,
                                  0,
                                  dma_fb, dma_agp, dma_notifier);
        
        NVDmaCreateContextObject (pNv, NvAGPToGraphics,
                                  NV_MEMORY_TO_MEMORY_FORMAT,
                                  0,
                                  dma_agp, dma_fb, dma_notifier);
    }
#endif

	fifo_regs   = pNv->FIFO;
	fifo_cmdbuf = pNv->dmaBase;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Using FIFO %d\n", fifo);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "FIFO control registers: %p\n", fifo_regs);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "DMA command buffer    : %p\n", fifo_cmdbuf);
	return TRUE;
}
