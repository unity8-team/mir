/*
 * Copyright (c) 2007 NVIDIA, Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "nv_include.h"

/* Don't call the directly, only load state should do this on the long run*/
void NV50CheckWriteVClk(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	while (NVRead(pNv, 0x00610300) & 0x80000000) {
		/* What does is the meaning of this? */
		const int super = ffs((NVRead(pNv, 0x00610024) >> 4) & 0x7);

		if (super > 0) {
			if (super == 2) {
				xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
				const CARD32 clockvar = NVRead(pNv, 0x00610030);
				int i;

				for(i = 0; i < xf86_config->num_crtc; i++) {
					xf86CrtcPtr crtc = xf86_config->crtc[i];
					NV50CrtcPrivPtr nv_crtc = crtc->driver_private;

					if (clockvar & (1 << (9 + nv_crtc->head))) {
						NV50CrtcSetPClk(crtc);
					}
				}
			}

			NVWrite(pNv, 0x00610024, 1 << (3 + super));
			NVWrite(pNv, 0x00610030, 0x80000000);
		}
	}
}

void NV50DisplayCommand(ScrnInfoPtr pScrn, CARD32 addr, CARD32 value)
{
	NVPtr pNv = NVPTR(pScrn);
	NVWrite(pNv, 0x00610304, value);
	NVWrite(pNv, 0x00610300, addr | 0x80010001);
	NV50CheckWriteVClk(pScrn);
}

void NV50CrtcCommand(xf86CrtcPtr crtc, CARD32 addr, CARD32 value)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NV50CrtcPrivPtr nv_crtc = crtc->driver_private;

	/* This head dependent offset may not be true everywere */
	NV50DisplayCommand(pScrn, addr + 0x400 * nv_crtc->head, value);
}

void nv50_crtc_dpms_set(xf86CrtcPtr crtc, int mode)
{
}

static Bool nv50_crtc_lock(xf86CrtcPtr crtc)
{
	return FALSE;
}

static const xf86CrtcFuncsRec nv50_crtc_funcs = {
	.dpms = nv50_crtc_dpms_set,
	.save = NULL,
	.restore = NULL,
	.lock = nv50_crtc_lock,
	.unlock = NULL,
	.mode_fixup = NV50CrtcModeFixup,
	.prepare = NV50CrtcPrepare,
	.mode_set = NV50CrtcModeSet,
	// .gamma_set = NV50DispGammaSet,
	.commit = NV50CrtcCommit,
	.shadow_create = NULL,
	.shadow_destroy = NULL,
	.set_cursor_position = NV50SetCursorPosition,
	.show_cursor = NV50CrtcShowCursor,
	.hide_cursor = NV50CrtcHideCursor,
	.load_cursor_argb = NV50LoadCursorARGB,
	.destroy = NULL,
};

void NV50DispCreateCrtcs(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	Head head;
	xf86CrtcPtr crtc;
	NV50CrtcPrivPtr nv50_crtc;

	/* Create a "crtc" object for each head */
	for(head = HEAD0; head <= HEAD1; head++) {
		crtc = xf86CrtcCreate(pScrn, &nv50_crtc_funcs);
		if(!crtc) return;

		nv50_crtc = xnfcalloc(sizeof(*nv50_crtc), 1);
		nv50_crtc->head = head;
		nv50_crtc->dither = pNv->FPDither;
		crtc->driver_private = nv50_crtc;
	}
}

