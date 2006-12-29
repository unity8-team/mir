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

#ifndef _I830_XF86MODES_H_
#define _I830_XF86MODES_H_
#include "xorgVersion.h"
#include "xf86Parser.h"

#if XORG_VERSION_CURRENT <= XORG_VERSION_NUMERIC(7,2,99,2,0)
double i830_xf86ModeHSync(DisplayModePtr mode);
double i830_xf86ModeVRefresh(DisplayModePtr mode);
DisplayModePtr i830_xf86DuplicateMode(DisplayModePtr pMode);
DisplayModePtr i830_xf86DuplicateModes(ScrnInfoPtr pScrn,
				       DisplayModePtr modeList);
void i830_xf86SetModeDefaultName(DisplayModePtr mode);
void i830_xf86SetModeCrtc(DisplayModePtr p, int adjustFlags);
Bool i830_xf86ModesEqual(DisplayModePtr pMode1, DisplayModePtr pMode2);
void i830_xf86PrintModeline(int scrnIndex,DisplayModePtr mode);
DisplayModePtr i830_xf86ModesAdd(DisplayModePtr modes, DisplayModePtr new);

DisplayModePtr i830_xf86DDCGetModes(int scrnIndex, xf86MonPtr DDC);
DisplayModePtr i830_xf86CVTMode(int HDisplay, int VDisplay, float VRefresh,
				Bool Reduced, Bool Interlaced);

#define xf86ModeHSync i830_xf86ModeHSync
#define xf86ModeVRefresh i830_xf86ModeVRefresh
#define xf86DuplicateMode i830_xf86DuplicateMode
#define xf86DuplicateModes i830_xf86DuplicateModes
#define xf86SetModeDefaultName i830_xf86SetModeDefaultName
#define xf86SetModeCrtc i830_xf86SetModeCrtc
#define xf86ModesEqual i830_xf86ModesEqual
#define xf86PrintModeline i830_xf86PrintModeline
#define xf86ModesAdd i830_xf86ModesAdd
#define xf86DDCGetModes i830_xf86DDCGetModes
#define xf86CVTMode i830_xf86CVTMode
#endif /* XORG_VERSION_CURRENT <= 7.2.99.2 */

void
i830xf86ValidateModesFlags(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			    int flags);

void
i830xf86ValidateModesClocks(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			    int *min, int *max, int n_ranges);

void
i830xf86ValidateModesSize(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			  int maxX, int maxY, int maxPitch);

void
i830xf86ValidateModesSync(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			  MonPtr mon);

void
i830xf86PruneInvalidModes(ScrnInfoPtr pScrn, DisplayModePtr *modeList,
			  Bool verbose);

void
i830xf86ValidateModesFlags(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			    int flags);

void
i830xf86ValidateModesUserConfig(ScrnInfoPtr pScrn, DisplayModePtr modeList);

DisplayModePtr
i830xf86GetMonitorModes (ScrnInfoPtr pScrn, XF86ConfMonitorPtr conf_monitor);

DisplayModePtr
i830xf86GetDefaultModes (void);

#endif /* _I830_XF86MODES_H_ */
