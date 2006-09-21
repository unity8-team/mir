/*
 * Copyright © 2006 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

/* i830_display.c */
Bool i830PipeSetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode, int pipe);
void i830DisableUnusedFunctions(ScrnInfoPtr pScrn);
Bool i830SetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode);
Bool i830DetectCRT(ScrnInfoPtr pScrn, Bool allow_disturb);
void i830SetLVDSPanelPower(ScrnInfoPtr pScrn, Bool on);
void i830PipeSetBase(ScrnInfoPtr pScrn, int pipe, int x, int y);

/* i830_sdvo.c */
I830SDVOPtr I830SDVOInit(ScrnInfoPtr pScrn, int output_index,
			 CARD32 output_device);
Bool I830SDVOPreSetMode(I830SDVOPtr s, DisplayModePtr mode);
Bool I830SDVOPostSetMode(I830SDVOPtr s, DisplayModePtr mode);
