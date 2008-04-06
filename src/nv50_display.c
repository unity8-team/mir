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

#include <float.h>
#include <math.h>
#include <strings.h>
#include <unistd.h>

#include "nv_include.h"

Bool
NV50DispPreInit(ScrnInfoPtr pScrn)
{
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50DispPreInit is called.\n");

	NVPtr pNv = NVPTR(pScrn);
	/* These labels are guesswork based on symmetry (2 SOR's and 3 DAC's exist)*/
	NVWrite(pNv, 0x00610184, NVRead(pNv, 0x00614004));
	NVWrite(pNv, 0x00610190 + 0 * 0x10, NVRead(pNv, 0x00616100 + 0 * 0x800));
	NVWrite(pNv, 0x00610190 + 1 * 0x10, NVRead(pNv, 0x00616100 + 1 * 0x800));
	NVWrite(pNv, 0x00610194 + 0 * 0x10, NVRead(pNv, 0x00616104 + 0 * 0x800));
	NVWrite(pNv, 0x00610194 + 1 * 0x10, NVRead(pNv, 0x00616104 + 1 * 0x800));
	NVWrite(pNv, 0x00610198 + 0 * 0x10, NVRead(pNv, 0x00616108 + 0 * 0x800));
	NVWrite(pNv, 0x00610198 + 1 * 0x10, NVRead(pNv, 0x00616108 + 1 * 0x800));
	NVWrite(pNv, 0x0061019c + 0 * 0x10, NVRead(pNv, 0x0061610c + 0 * 0x800));
	NVWrite(pNv, 0x0061019c + 1 * 0x10, NVRead(pNv, 0x0061610c + 1 * 0x800));
	NVWrite(pNv, 0x006101d0 + DAC0 * 0x4, NVRead(pNv, 0x0061a000 + DAC0 * 0x800));
	NVWrite(pNv, 0x006101d0 + DAC1 * 0x4, NVRead(pNv, 0x0061a000 + DAC1 * 0x800));
	NVWrite(pNv, 0x006101d0 + DAC2 * 0x4, NVRead(pNv, 0x0061a000 + DAC2 * 0x800));
	NVWrite(pNv, 0x006101e0 + SOR0 * 0x4, NVRead(pNv, 0x0061c000 + SOR0 * 0x800));
	NVWrite(pNv, 0x006101e0 + SOR1 * 0x4, NVRead(pNv, 0x0061c000 + SOR1 * 0x800));
	NVWrite(pNv, NV50_DAC0_DPMS_CTRL, 0x00550000 | NV50_DAC_DPMS_CTRL_PENDING);
	NVWrite(pNv, NV50_DAC0_CLK_CTRL2, 0x00000001);
	NVWrite(pNv, NV50_DAC1_DPMS_CTRL, 0x00550000 | NV50_DAC_DPMS_CTRL_PENDING);
	NVWrite(pNv, NV50_DAC1_CLK_CTRL2, 0x00000001);
	NVWrite(pNv, NV50_DAC2_DPMS_CTRL, 0x00550000 | NV50_DAC_DPMS_CTRL_PENDING);
	NVWrite(pNv, NV50_DAC2_CLK_CTRL2, 0x00000001);

	return TRUE;
}

Bool
NV50DispInit(ScrnInfoPtr pScrn)
{
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50DispInit is called.\n");

	NVPtr pNv = NVPTR(pScrn);
	uint32_t val;
	if (NVRead(pNv, NV50_DISPLAY_SUPERVISOR) & 0x100) {
		NVWrite(pNv, NV50_DISPLAY_SUPERVISOR, 0x100);
		NVWrite(pNv, 0x006194e8, NVRead(pNv, 0x006194e8) & ~1);
		while (NVRead(pNv, 0x006194e8) & 2);
	}

	NVWrite(pNv, NV50_DISPLAY_UNK200_CTRL, 0x2b00);
	/* A bugfix (#12637) from the nv driver, to unlock the driver if it's left in a poor state */
	do {
		val = NVRead(pNv, NV50_DISPLAY_UNK200_CTRL);
		if ((val & 0x9f0000) == 0x20000)
			NVWrite(pNv, NV50_DISPLAY_UNK200_CTRL, val | 0x800000);

		if ((val & 0x3f0000) == 0x30000)
			NVWrite(pNv, NV50_DISPLAY_UNK200_CTRL, val | 0x200000);
	} while ((val & 0x1e0000) != 0);
	NVWrite(pNv, NV50_DISPLAY_CTRL_STATE, NV50_DISPLAY_CTRL_STATE_ENABLE);
	NVWrite(pNv, NV50_DISPLAY_UNK200_CTRL, 0x1000b03);
	while (!(NVRead(pNv, NV50_DISPLAY_UNK200_CTRL) & 0x40000000));

	NV50DisplayCommand(pScrn, 0x84, 0);
	NV50DisplayCommand(pScrn, 0x88, 0);
	/* The GetLVDSNativeMode() function is proof that more than crtc0 is used by the bios. */
	NV50DisplayCommand(pScrn, NV50_CRTC0_BLANK_CTRL, NV50_CRTC0_BLANK_CTRL_BLANK);
	NV50DisplayCommand(pScrn, 0x800, 0);
	NV50DisplayCommand(pScrn, NV50_CRTC0_DISPLAY_START, 0);
	NV50DisplayCommand(pScrn, 0x82c, 0);

	return TRUE;
}

void
NV50DispShutdown(ScrnInfoPtr pScrn)
{
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "NV50DispShutdown is called.\n");
	NVPtr pNv = NVPTR(pScrn);
	xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	for(i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];

		NV50CrtcBlankScreen(crtc, TRUE);
	}

	NV50DisplayCommand(pScrn, NV50_UPDATE_DISPLAY, 0);

	for(i = 0; i < xf86_config->num_crtc; i++) {
		xf86CrtcPtr crtc = xf86_config->crtc[i];
		NVCrtcPrivatePtr nv_crtc = crtc->driver_private;

		/* I think this is some kind of trigger to reinitialise the supervisor. */
		/* Without anything active (this is shutdown) it's likely to disable itself. */
		/* The blob doesn't do it quite this way., it seems to do 0x30C as init and end. */
		/* It doesn't wait for a non-zero value either. */
		if (crtc->enabled) {
			uint32_t mask = 0;
			if (nv_crtc->head == 1)
				mask = NV50_DISPLAY_SUPERVISOR_CRTC1;
			else 
				mask = NV50_DISPLAY_SUPERVISOR_CRTC0;

			NVWrite(pNv, NV50_DISPLAY_SUPERVISOR, mask);
			while(!(NVRead(pNv, NV50_DISPLAY_SUPERVISOR) & mask));
		}
	}

	NVWrite(pNv, NV50_DISPLAY_UNK200_CTRL, 0x0);
	NVWrite(pNv, NV50_DISPLAY_CTRL_STATE, NV50_DISPLAY_CTRL_STATE_DISABLE);
	while ((NVRead(pNv, NV50_DISPLAY_UNK200_CTRL) & 0x1e0000) != 0);
	while ((NVRead(pNv, NV50_SOR0_DPMS_STATE) & NV50_SOR_DPMS_STATE_PENDING));
	while ((NVRead(pNv, NV50_SOR1_DPMS_STATE) & NV50_SOR_DPMS_STATE_PENDING));
}
