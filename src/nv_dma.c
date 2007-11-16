/*
 * Copyright 2007 Ben Skeggs
 * Copyright 2007 Stephane Marchesin
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

#include <errno.h>
#include "nv_include.h"
#include "nvreg.h"

void NVDmaKickoffNNN(NVPtr pNv)
{
	if(pNv->dmaCurrent != pNv->dmaPut) {
		pNv->dmaPut = pNv->dmaCurrent;
		WRITE_PUT(pNv,  pNv->dmaPut);
	}
}

void NVDmaKickoffCallback(NVPtr pNv)
{
	FIRE_RING();
	pNv->DMAKickoffCallback = NULL;
}

static uint32_t subchannels[8];

void NVDmaStartNNN(NVPtr pNv, uint32_t object, uint32_t tag, int size)
{
	int subchannel=-1;
	int i;
	
	/* XXX FIXME */
	ScrnInfoPtr pScrn = xf86Screens[0];

	/* look for a subchannel already bound to that object */
	for(i=0;i<8;i++)
	{
		if (subchannels[i]==object)
		{
			subchannel=i;
			break;
		}
	}

	/* add 2 for the potential subchannel binding */
	if((pNv)->dmaFree <= (size + 2))
		WAIT_RING(size + 2);

	if (subchannel==-1)
	{
		/* bind the object */
		subchannel=rand()%8;
		subchannels[subchannel]=object;
		NVDEBUG("Bind object %x on subchannel %d\n", (object), (subchannel));
		OUT_RING  ((1<<18) | (subchannel<<13));
		OUT_RING  (object);
		pNv->dmaFree -= (2);
	}
	NVDEBUG("BEGIN_RING: subc=%d, cmd=%x, num=%d\n", (subchannel), (tag), (size));
	OUT_RING  (((size) << 18) | ((subchannel) << 13) | (tag));
	pNv->dmaFree -= ((size) + 1); 
}


/* There is a HW race condition with videoram command buffers.
 * You can't jump to the location of your put offset.  We write put
 * at the jump offset + SKIPS dwords with noop padding in between
 * to solve this problem
 */
#define SKIPS  8

void NVDmaWaitNNN(ScrnInfoPtr pScrn, int size)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel_priv *chan = nouveau_channel(pNv->chan);
	int t_start;
	int dmaGet;

	size++;

	t_start = GetTimeInMillis();
	while(pNv->dmaFree < size) {
		dmaGet = READ_GET(pNv);

		if(pNv->dmaPut >= dmaGet) {
			pNv->dmaFree = pNv->dmaMax - pNv->dmaCurrent;
			if(pNv->dmaFree < size) {
				OUT_RING  ((0x20000000|chan->drm.put_base));
				if(dmaGet <= SKIPS) {
					if(pNv->dmaPut <= SKIPS) /* corner case - will be idle */
						WRITE_PUT(pNv, SKIPS + 1);
					do {
						if (GetTimeInMillis() - t_start > 2000)
							NVSync(pScrn);
						dmaGet = READ_GET(pNv);
					} while(dmaGet <= SKIPS);
				}
				WRITE_PUT(pNv, SKIPS);
				pNv->dmaCurrent = pNv->dmaPut = SKIPS;
				pNv->dmaFree = dmaGet - (SKIPS + 1);
			}
		} else
			pNv->dmaFree = dmaGet - pNv->dmaCurrent - 1;

		if (GetTimeInMillis() - t_start > 2000)
			NVSync(pScrn);
	}
}

static void NVDumpLockupInfo(NVPtr pNv)
{
	int i,start;
	start=READ_GET(pNv)-20;
	if (start<0) start=0;
	xf86DrvMsg(0, X_INFO, "Fifo dump (lockup 0x%04x,0x%04x):\n",READ_GET(pNv),pNv->dmaPut);
	for(i=start;i<pNv->dmaPut+10;i++)
		xf86DrvMsg(0, X_INFO, "[0x%04x] 0x%08x\n", i, pNv->dmaBase[i]);
	xf86DrvMsg(0, X_INFO, "End of fifo dump\n");
}

static void
NVLockedUp(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	/* avoid re-entering FatalError on shutdown */
	if (pNv->LockedUp)
		return;
	pNv->LockedUp = TRUE;

	NVDumpLockupInfo(pNv);

	FatalError("DMA queue hang: dmaPut=%x, current=%x, status=%x\n",
		   pNv->dmaPut, READ_GET(pNv), pNv->PGRAPH[NV_PGRAPH_STATUS/4]);
}

void NVSync(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int t_start, timeout = 2000;
	int grobj = pNv->Architecture < NV_ARCH_50 ? NvImageBlit : Nv2D;

	if(pNv->NoAccel)
		return;

	if(pNv->DMAKickoffCallback)
		(*pNv->DMAKickoffCallback)(pNv);

	/* Wait for entire FIFO to be processed */
	t_start = GetTimeInMillis();
	while((GetTimeInMillis() - t_start) < timeout &&
			(READ_GET(pNv) != pNv->dmaPut));
	if ((GetTimeInMillis() - t_start) >= timeout) {
		NVLockedUp(pScrn);
		return;
	}

	/* Wait for channel to go completely idle */
	NVNotifierReset(pScrn, pNv->Notifier0);
	BEGIN_RING(grobj, 0x104, 1);
	OUT_RING  (0);
	BEGIN_RING(grobj, 0x100, 1);
	OUT_RING  (0);
	FIRE_RING();
	if (!NVNotifierWaitStatus(pScrn, pNv->Notifier0, 0, timeout))
		NVLockedUp(pScrn);
}

void NVResetGraphics(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int i;

	pNv->dmaPut = pNv->dmaCurrent = READ_GET(pNv);
	pNv->dmaMax = (nouveau_channel(pNv->chan)->drm.cmdbuf_size >> 2) - 2;
	pNv->dmaFree = pNv->dmaMax - pNv->dmaCurrent;

	/* assert there's enough room for the skips */
	if(pNv->dmaFree <= SKIPS)
		WAIT_RING(SKIPS); 
	for (i=0; i<SKIPS; i++) {
		OUT_RING(0);
		pNv->dmaBase[i]=0;
	}
	pNv->dmaFree -= SKIPS;

	for(i=0;i<8;i++)
		subchannels[i]=0;

	NVAccelCommonInit(pScrn);
}

static void NVInitDmaCB(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	unsigned int cb_location;
	int cb_size;
	char *s;

	/* I'm not bothering to check for failures here, the DRM will fall back
	 * on defaults if anything's wrong (ie. out of AGP, invalid sizes)
	 */
#ifndef __powerpc__
	if (pNv->GARTScratch)
		cb_location = NOUVEAU_MEM_AGP | NOUVEAU_MEM_PCI_ACCEPTABLE;
	else
#endif
	cb_location = NOUVEAU_MEM_FB;
	if((s = (char *)xf86GetOptValString(pNv->Options, OPTION_CMDBUF_LOCATION))) {
		if(!xf86NameCmp(s, "AGP"))
			cb_location = NOUVEAU_MEM_AGP;
		else if (!xf86NameCmp(s, "VRAM"))
			cb_location = NOUVEAU_MEM_FB;
		else if (!xf86NameCmp(s, "PCI"))
			cb_location = NOUVEAU_MEM_PCI;
		else
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Invalid value \"%s\" for CBLocation\n", s);
	}
	nouveau_device_set_param(pNv->dev, NOUVEAU_SETPARAM_CMDBUF_LOCATION,
				 cb_location);

	/* CBSize == size of space reserved for *all* FIFOs in MiB */
	if (xf86GetOptValInteger(pNv->Options, OPTION_CMDBUF_SIZE, &cb_size))
		nouveau_device_set_param(pNv->dev, NOUVEAU_SETPARAM_CMDBUF_SIZE,
					 (cb_size << 20));
}

Bool
NVInitDma(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int ret;

	NVInitDmaCB(pScrn);

	ret = nouveau_channel_alloc(pNv->dev, NvDmaFB, NvDmaTT, &pNv->chan);
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Error creating GPU channel: %d\n", ret);
		return FALSE;
	}
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Opened GPU channel %d\n", pNv->chan->id);

	{
		struct nouveau_channel_priv *chan = nouveau_channel(pNv->chan);
		int i;

		pNv->dmaBase = chan->pushbuf;
		pNv->FIFO = chan->user;
		pNv->NotifierBlock = chan->notifier_block;
		pNv->dmaPut = pNv->dmaCurrent = READ_GET(pNv);
		pNv->dmaMax = (chan->drm.cmdbuf_size >> 2) - 2;
		pNv->dmaFree = pNv->dmaMax - pNv->dmaCurrent;

		for (i=0; i<SKIPS; i++)
			OUT_RING(0);
		pNv->dmaFree -= SKIPS;
	}

	return TRUE;
}

