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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef ENABLE_RANDR12

#include "nv_include.h"

void NV50CrtcDPMSSet(xf86CrtcPtr crtc, int mode)
{
}

static Bool NV50CrtcLock(xf86CrtcPtr crtc)
{
	return FALSE;
}

static const xf86CrtcFuncsRec nv50_crtc_funcs = {
	.dpms = NV50CrtcDPMSSet,
	.save = NULL,
	.restore = NULL,
	.lock = NV50CrtcLock,
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

#endif /* ENABLE_RANDR12 */
