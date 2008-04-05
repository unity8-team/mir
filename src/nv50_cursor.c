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

void NV50CrtcShowHideCursor(xf86CrtcPtr crtc, Bool show, Bool update)
{
	ScrnInfoPtr pScrn = crtc->scrn;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50CrtcShowHideCursor is called (%s, %s).\n", show ? "show" : "hide", update ? "update" : "no update");

	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	NV50CrtcCommand(crtc, NV50_CRTC0_CURSOR, 
		show ? NV50_CRTC0_CURSOR_SHOW : NV50_CRTC0_CURSOR_HIDE);
	if (update) {
		nv_crtc->cursorVisible = show;
		NV50DisplayCommand(pScrn, NV50_UPDATE_DISPLAY, 0);
	}
}

void nv50_crtc_show_cursor(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	/* Calling NV50_UPDATE_DISPLAY during modeset will lock up everything. */
	if (nv_crtc->modeset_lock)
		return;

	NV50CrtcShowHideCursor(crtc, TRUE, TRUE);
}

void nv50_crtc_hide_cursor(xf86CrtcPtr crtc)
{
	NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

	/* Calling NV50_UPDATE_DISPLAY during modeset will lock up everything. */
	if (nv_crtc->modeset_lock)
		return;

	NV50CrtcShowHideCursor(crtc, FALSE, TRUE);
}

void nv50_crtc_set_cursor_position(xf86CrtcPtr crtc, int x, int y)
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

void nv50_crtc_load_cursor_argb(xf86CrtcPtr crtc, CARD32 *src)
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

