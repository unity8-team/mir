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

#ifndef _RADEON_XF86MODES_H_
#define _RADEON_XF86MODES_H_
#include "xorgVersion.h"
#include "xf86Parser.h"

#if XORG_VERSION_CURRENT <= XORG_VERSION_NUMERIC(7,2,99,2,0)
double RADEON_xf86ModeHSync(DisplayModePtr mode);
double RADEON_xf86ModeVRefresh(DisplayModePtr mode);
DisplayModePtr RADEON_xf86DuplicateMode(DisplayModePtr pMode);
DisplayModePtr RADEON_xf86DuplicateModes(ScrnInfoPtr pScrn,
				       DisplayModePtr modeList);
void RADEON_xf86SetModeDefaultName(DisplayModePtr mode);
void RADEON_xf86SetModeCrtc(DisplayModePtr p, int adjustFlags);
Bool RADEON_xf86ModesEqual(DisplayModePtr pMode1, DisplayModePtr pMode2);
void RADEON_xf86PrintModeline(int scrnIndex,DisplayModePtr mode);
DisplayModePtr RADEON_xf86ModesAdd(DisplayModePtr modes, DisplayModePtr new);

DisplayModePtr RADEON_xf86DDCGetModes(int scrnIndex, xf86MonPtr DDC);
DisplayModePtr RADEON_xf86CVTMode(int HDisplay, int VDisplay, float VRefresh,
				Bool Reduced, Bool Interlaced);

#define xf86ModeHSync RADEON_xf86ModeHSync
#define xf86ModeVRefresh RADEON_xf86ModeVRefresh
#define xf86DuplicateMode RADEON_xf86DuplicateMode
#define xf86DuplicateModes RADEON_xf86DuplicateModes
#define xf86SetModeDefaultName RADEON_xf86SetModeDefaultName
#define xf86SetModeCrtc RADEON_xf86SetModeCrtc
#define xf86ModesEqual RADEON_xf86ModesEqual
#define xf86PrintModeline RADEON_xf86PrintModeline
#define xf86ModesAdd RADEON_xf86ModesAdd
#define xf86DDCGetModes RADEON_xf86DDCGetModes
#define xf86CVTMode RADEON_xf86CVTMode
#endif /* XORG_VERSION_CURRENT <= 7.2.99.2 */

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

DisplayModePtr
RADEONxf86GetMonitorModes (ScrnInfoPtr pScrn, XF86ConfMonitorPtr conf_monitor);

DisplayModePtr
RADEONxf86GetDefaultModes (Bool interlaceAllowed, Bool doubleScanAllowed);

#endif
