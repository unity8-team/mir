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

#include <string.h>

#include <cursorstr.h>

#include "nv_include.h"
#include "nv50_type.h"
#include "nv50_cursor.h"
#include "nv50_display.h"

void NV50SetCursorPosition(xf86CrtcPtr crtc, int x, int y)
{
	NVPtr pNv = NVPTR(crtc->scrn);
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	if (nv_crtc->head == 1)
		NVWrite(pNv, NV50_CRTC1_CURSOR_POS, (y & 0xFFFF) << 16 | (x & 0xFFFF));
	else
		NVWrite(pNv, NV50_CRTC0_CURSOR_POS, (y & 0xFFFF) << 16 | (x & 0xFFFF));

	/* This is needed to allow the cursor to move. */
	NVWrite(pNv, 0x00647080 + nv_crtc->head * 0x1000, 0);
}

void NV50LoadCursorARGB(xf86CrtcPtr crtc, CARD32 *src)
{
	NVPtr pNv = NVPTR(crtc->scrn);
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;
	uint32_t *dst = NULL;

	if (nv_crtc->head == 1)
		dst = (uint32_t *) pNv->Cursor2->map;
	else
		dst = (uint32_t *) pNv->Cursor->map;

	/* Assume cursor is 64x64 */
	memcpy(dst, (uint32_t *)src, 64 * 64 * 4);
}

Bool NV50CursorAcquire(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	if (!pNv->HWCursor) return TRUE;

	/* Initialize the cursor on each head */
	for(i = 0; i < xf86_config->num_crtc; i++) {
		NVCrtcPrivatePtr nv_crtc = xf86_config->crtc[i]->driver_private;
		const int headOff = 0x10 * nv_crtc->head;

		NVWrite(pNv, NV50_CRTC0_CURSOR_CTRL+headOff, 0x2000);
		while (NVRead(pNv, NV50_CRTC0_CURSOR_CTRL+headOff) & 0x30000);

		NVWrite(pNv, NV50_CRTC0_CURSOR_CTRL+headOff, NV50_CRTC_CURSOR_CTRL_ON);
		while ((NVRead(pNv, NV50_CRTC0_CURSOR_CTRL+headOff) & 0x30000) != 0x10000);
	}

	return TRUE;
}

void NV50CursorRelease(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	if (!pNv->HWCursor) return;

	/* Release the cursor on each head */
	for(i = 0; i < xf86_config->num_crtc; i++) {
		NVCrtcPrivatePtr nv_crtc = xf86_config->crtc[i]->driver_private;
		const int headOff = 0x10 * nv_crtc->head;

		NVWrite(pNv, NV50_CRTC0_CURSOR_CTRL+headOff, NV50_CRTC_CURSOR_CTRL_OFF);
		while (NVRead(pNv, NV50_CRTC0_CURSOR_CTRL+headOff) & 0x30000);
	}
}

Bool NV50CursorInit(ScreenPtr pScreen)
{
	return xf86_cursors_init(pScreen, 64, 64,
		HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
		HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_32 |
		HARDWARE_CURSOR_ARGB);
}

