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

Bool
NVInitDma(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nv04_fifo nv04_data = { .vram = NvDmaFB,
				       .gart = NvDmaTT };
	struct nvc0_fifo nvc0_data = { };
	struct nouveau_object *device = &pNv->dev->object;
	struct nouveau_fifo *fifo;
	int size, ret;
	void *data;

	if (pNv->dev->drm_version < 0x01000000 && pNv->dev->chipset >= 0xc0) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Fermi acceleration not supported on old kernel\n");
		return FALSE;
	}

	if (pNv->Architecture < NV_ARCH_C0) {
		data = &nv04_data;
		size = sizeof(nv04_data);
	} else {
		data = &nvc0_data;
		size = sizeof(nvc0_data);
	}

	ret = nouveau_object_new(device, 0, NOUVEAU_FIFO_CHANNEL_CLASS,
				 data, size, &pNv->channel);
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Error creating GPU channel: %d\n", ret);
		return FALSE;
	}

	fifo = pNv->channel->data;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Opened GPU channel %d\n", fifo->channel);

	ret = nouveau_pushbuf_new(pNv->client, pNv->channel, 4, 32 * 1024,
				  true, &pNv->pushbuf);
	if (ret) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Error allocating DMA push buffer: %d\n",ret);
		NVTakedownDma(pScrn);
		return FALSE;
	}

	ret = nouveau_bufctx_new(pNv->client, 1, &pNv->bufctx);
	if (ret) {
		NVTakedownDma(pScrn);
		return FALSE;
	}

	pNv->pushbuf->user_priv = pNv->bufctx;
	return TRUE;
}

void
NVTakedownDma(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	if (pNv->channel) {
		struct nouveau_fifo *fifo = pNv->channel->data;
		int chid = fifo->channel;

		nouveau_bufctx_del(&pNv->bufctx);
		nouveau_pushbuf_del(&pNv->pushbuf);
		nouveau_object_del(&pNv->channel);

		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Closed GPU channel %d\n", chid);
	}
}

