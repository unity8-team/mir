/*
 * Copyright � 2006 Intel Corporation
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
Bool i830PipeSetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode, int pipe,
		     Bool plane_enable);
void i830DisableUnusedFunctions(ScrnInfoPtr pScrn);
Bool i830SetMode(ScrnInfoPtr pScrn, DisplayModePtr pMode);
void i830PipeSetBase(ScrnInfoPtr pScrn, int pipe, int x, int y);
void i830WaitForVblank(ScrnInfoPtr pScrn);
void i830DescribeOutputConfiguration(ScrnInfoPtr pScrn);
int i830GetLoadDetectPipe(ScrnInfoPtr pScrn, I830OutputPtr output);
void i830ReleaseLoadDetectPipe(ScrnInfoPtr pScrn, I830OutputPtr output);
