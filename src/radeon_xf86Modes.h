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

double
RADEONxf86ModeHSync(DisplayModePtr mode);

double
RADEONxf86ModeVRefresh(DisplayModePtr mode);

DisplayModePtr
RADEONxf86DuplicateMode(DisplayModePtr pMode);

DisplayModePtr
RADEONxf86DuplicateModes(ScrnInfoPtr pScrn, DisplayModePtr modeList);

void
RADEONxf86SetModeDefaultName(DisplayModePtr mode);

void
RADEONxf86SetModeCrtc(DisplayModePtr p, int adjustFlags);

Bool
RADEONModesEqual(DisplayModePtr pMode1, DisplayModePtr pMode2);

void
RADEONxf86ValidateModesFlags(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			    int flags);

void
RADEONxf86ValidateModesClocks(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			    int *min, int *max, int n_ranges);

void
RADEONxf86ValidateModesSize(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			  int maxX, int maxY, int maxPitch);

void
RADEONxf86ValidateModesSync(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			  MonPtr mon);

void
RADEONxf86PruneInvalidModes(ScrnInfoPtr pScrn, DisplayModePtr *modeList,
			  Bool verbose);

void
RADEONxf86ValidateModesFlags(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			    int flags);

void
RADEONxf86ValidateModesUserConfig(ScrnInfoPtr pScrn, DisplayModePtr modeList);

void
PrintModeline(int scrnIndex,DisplayModePtr mode);

extern DisplayModeRec RADEONxf86DefaultModes[];
