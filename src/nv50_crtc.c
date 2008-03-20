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
	while (NVRead(pNv, NV50_DISPLAY_CTRL_STATE) & NV50_DISPLAY_CTRL_STATE_PENDING) {
		/* An educated guess. */
		const uint32_t supervisor = NVRead(pNv, NV50_DISPLAY_SUPERVISOR);

		if (supervisor & NV50_DISPLAY_SUPERVISOR_CLK_MASK) {
			if (supervisor & NV50_DISPLAY_SUPERVISOR_CLK_UPDATE) {
				xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
				const uint32_t clockvar = NVRead(pNv, NV50_DISPLAY_UNK30_CTRL);
				int i;

				for(i = 0; i < xf86_config->num_crtc; i++) {
					xf86CrtcPtr crtc = xf86_config->crtc[i];
					NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
					uint32_t mask = 0;

					if (nv_crtc->head == 1)
						mask = NV50_DISPLAY_UNK30_CTRL_UPDATE_VCLK1;
					else
						mask = NV50_DISPLAY_UNK30_CTRL_UPDATE_VCLK0;

					if (clockvar & mask) {
						NV50CrtcSetPClk(crtc);
					}
				}
			}

			NVWrite(pNv, NV50_DISPLAY_SUPERVISOR, 1 << (ffs(supervisor & NV50_DISPLAY_SUPERVISOR_CLK_MASK) - 1));
			NVWrite(pNv, NV50_DISPLAY_UNK30_CTRL, NV50_DISPLAY_UNK30_CTRL_PENDING);
		}
	}
}

void NV50DisplayCommand(ScrnInfoPtr pScrn, CARD32 addr, CARD32 value)
{
	NVPtr pNv = NVPTR(pScrn);
	NVWrite(pNv, NV50_DISPLAY_CTRL_VAL, value);
	NVWrite(pNv, NV50_DISPLAY_CTRL_STATE, addr | 0x10000 | NV50_DISPLAY_CTRL_STATE_ENABLE | NV50_DISPLAY_CTRL_STATE_PENDING);
	NV50CheckWriteVClk(pScrn);
}

void NV50CrtcCommand(xf86CrtcPtr crtc, CARD32 addr, CARD32 value)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

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

const xf86CrtcFuncsRec * nv50_get_crtc_funcs()
{
	return &nv50_crtc_funcs;
}

