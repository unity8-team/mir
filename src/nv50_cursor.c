/*
 * Copyright (c) 2007 NVIDIA, Corporation
 * Copyright (c) 2008 Maarten Maathuis
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

#include <string.h>

#include <cursorstr.h>

#include "nv_include.h"

Bool NV50CursorAcquire(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int i;

	if (!pNv->HWCursor) return TRUE;

	/* Initialize the cursor on each head */
	for (i = 0; i < 2; i++) {
		nouveauCrtcPtr crtc = pNv->crtc[i];
		const int headOff = 0x10 * crtc->index;

		NVWrite(pNv, NV50_CRTC0_CURSOR_CTRL + headOff, 0x2000);
		while (NVRead(pNv, NV50_CRTC0_CURSOR_CTRL+headOff) & NV50_CRTC_CURSOR_CTRL_STATUS_MASK);

		NVWrite(pNv, NV50_CRTC0_CURSOR_CTRL + headOff, NV50_CRTC_CURSOR_CTRL_ON);
		while ((NVRead(pNv, NV50_CRTC0_CURSOR_CTRL+headOff) & NV50_CRTC_CURSOR_CTRL_STATUS_MASK) != NV50_CRTC_CURSOR_CTRL_STATUS_ACTIVE);
	}

	return TRUE;
}

void NV50CursorRelease(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	int i;

	if (!pNv->HWCursor) return;

	/* Release the cursor on each head */
	for (i = 0; i < 2; i++) {
		nouveauCrtcPtr crtc = pNv->crtc[i];
		const int headOff = 0x10 * crtc->index;

		NVWrite(pNv, NV50_CRTC0_CURSOR_CTRL+headOff, NV50_CRTC_CURSOR_CTRL_OFF);
		while (NVRead(pNv, NV50_CRTC0_CURSOR_CTRL+headOff) & NV50_CRTC_CURSOR_CTRL_STATUS_MASK);
	}
}

Bool NV50CursorInit(ScreenPtr pScreen)
{
	return xf86_cursors_init(pScreen, 64, 64,
		HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
		HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32 |
		HARDWARE_CURSOR_ARGB);
}

