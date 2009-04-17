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

static void
NVLockedUp(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	/* avoid re-entering FatalError on shutdown */
	if (pNv->LockedUp)
		return;
	pNv->LockedUp = TRUE;

	FatalError("Detected GPU lockup\n");
}

static void
NVChannelHangNotify(struct nouveau_channel *chan)
{
	ScrnInfoPtr pScrn = chan->user_private;

	NVLockedUp(pScrn);
}

void NVSync(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	struct nouveau_channel *chan = pNv->chan;
	struct nouveau_grobj *gr = pNv->Nv2D ? pNv->Nv2D : pNv->NvImageBlit;

	if (pNv->NoAccel)
		return;

	/* Wait for nvchannel to go completely idle */
	nouveau_notifier_reset(pNv->notify0, 0);
	BEGIN_RING(chan, gr, 0x104, 1);
	OUT_RING  (chan, 0);
	BEGIN_RING(chan, gr, 0x100, 1);
	OUT_RING  (chan, 0);
	FIRE_RING (chan);
	if (nouveau_notifier_wait_status(pNv->notify0, 0,
					 NV_NOTIFY_STATE_STATUS_COMPLETED, 2.0))
		NVLockedUp(pScrn);
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
	/* WARNING: on ppc at least, putting cmdbuf in MEM_FB results in
	 * DMA hangs
	 */

	cb_location = NOUVEAU_MEM_AGP | NOUVEAU_MEM_PCI_ACCEPTABLE;
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
	pNv->chan->user_private = pScrn;
	pNv->chan->hang_notify = NVChannelHangNotify;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Opened GPU channel %d\n", pNv->chan->id);

	return TRUE;
}

void
NVTakedownDma(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);

	if (!pNv->chan)
		return;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Closed GPU channel %d\n", pNv->chan->id);
	nouveau_channel_free(&pNv->chan);
}

