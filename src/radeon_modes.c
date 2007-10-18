/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *   Alan Hourihane <alanh@fairlite.demon.co.uk>
 */

#include <string.h>
#include <stdio.h>

#include "xf86.h"
				/* Driver data structures */
#include "randrstr.h"
#include "radeon_probe.h"
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"

#include "radeon_version.h"

#include "xf86Modes.h"
				/* DDC support */
#include "xf86DDC.h"
#include <randrstr.h>

void RADEONSetPitch (ScrnInfoPtr pScrn)
{
    int  dummy = pScrn->virtualX;
    RADEONInfoPtr info = RADEONPTR(pScrn);

    /* FIXME: May need to validate line pitch here */
    switch (pScrn->depth / 8) {
    case 1: if (info->allowColorTiling) dummy = (pScrn->virtualX + 255) & ~255;
	else dummy = (pScrn->virtualX + 127) & ~127;
	break;
    case 2: if (info->allowColorTiling) dummy = (pScrn->virtualX + 127) & ~127;
	else dummy = (pScrn->virtualX + 31) & ~31;
	break;
    case 3:
    case 4: if (info->allowColorTiling) dummy = (pScrn->virtualX + 63) & ~63;
	else dummy = (pScrn->virtualX + 15) & ~15;
	break;
    }
    pScrn->displayWidth = dummy;
    info->CurrentLayout.displayWidth = pScrn->displayWidth;

}

static DisplayModePtr RADEONTVModes(xf86OutputPtr output)
{
    DisplayModePtr new  = NULL;

    /* just a place holder */
    new = xf86CVTMode(800, 600, 60.00, FALSE, FALSE);
    new->type = M_T_DRIVER | M_T_PREFERRED;

    return new;
}

/* This is used only when no mode is specified for FP and no ddc is
 * available.  We force it to native mode, if possible.
 */
static DisplayModePtr RADEONFPNativeMode(xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    DisplayModePtr  new   = NULL;
    char            stmp[32];

    if (radeon_output->PanelXRes != 0 &&
	radeon_output->PanelYRes != 0 &&
	radeon_output->DotClock != 0) {

	new             = xnfcalloc(1, sizeof (DisplayModeRec));
	sprintf(stmp, "%dx%d", radeon_output->PanelXRes, radeon_output->PanelYRes);
	new->name       = xnfalloc(strlen(stmp) + 1);
	strcpy(new->name, stmp);
	new->HDisplay   = radeon_output->PanelXRes;
	new->VDisplay   = radeon_output->PanelYRes;

	new->HTotal     = new->HDisplay + radeon_output->HBlank;
	new->HSyncStart = new->HDisplay + radeon_output->HOverPlus;
	new->HSyncEnd   = new->HSyncStart + radeon_output->HSyncWidth;
	new->VTotal     = new->VDisplay + radeon_output->VBlank;
	new->VSyncStart = new->VDisplay + radeon_output->VOverPlus;
	new->VSyncEnd   = new->VSyncStart + radeon_output->VSyncWidth;

	new->Clock      = radeon_output->DotClock;
	new->Flags      = 0;

	if (new) {
	    new->type       = M_T_DRIVER | M_T_PREFERRED;

	    new->next       = NULL;
	    new->prev       = NULL;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Added native panel mode: %dx%d\n",
		   radeon_output->PanelXRes, radeon_output->PanelYRes);
    }

    return new;
}

/* this function is basically a hack to add the screen modes */
static void RADEONAddScreenModes(xf86OutputPtr output, DisplayModePtr *modeList)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    DisplayModePtr  last       = NULL;
    DisplayModePtr  new        = NULL;
    DisplayModePtr  first      = NULL;
    int             count      = 0;
    int             i, width, height;
    char **ppModeName = pScrn->display->modes;

    first = last = *modeList;

    /* We have a flat panel connected to the primary display, and we
     * don't have any DDC info.
     */
    for (i = 0; ppModeName[i] != NULL; i++) {

	if (sscanf(ppModeName[i], "%dx%d", &width, &height) != 2) continue;

	if (radeon_output->type == OUTPUT_LVDS) {
	    /* already added the native mode */
	    if (width == radeon_output->PanelXRes && height == radeon_output->PanelYRes)
		continue;

	    /* Note: We allow all non-standard modes as long as they do not
	     * exceed the native resolution of the panel.  Since these modes
	     * need the internal RMX unit in the video chips (and there is
	     * only one per card), this will only apply to the primary head.
	     */
	    if (width < 320 || width > radeon_output->PanelXRes ||
		height < 200 || height > radeon_output->PanelYRes) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Mode %s is out of range.\n", ppModeName[i]);
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Valid FP modes must be between 320x200-%dx%d\n",
			   radeon_output->PanelXRes, radeon_output->PanelYRes);
		continue;
	    }
	}

	new = xf86CVTMode(width, height, 60.0, FALSE, FALSE);

	new->type      |= M_T_USERDEF;

	new->next       = NULL;
	new->prev       = last;

	if (last) last->next = new;
	last = new;
	if (!first) first = new;

	count++;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Adding Screen mode: %s\n", new->name);
    }


    /* Close the doubly-linked mode list, if we found any usable modes */
    if (last) {
	last->next   = NULL; //first;
	first->prev  = NULL; //last;
	*modeList = first;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Total number of valid Screen mode(s) added: %d\n", count);

}

DisplayModePtr
RADEONProbeOutputModes(xf86OutputPtr output)
{
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    xf86MonPtr		    edid_mon;
    DisplayModePtr	    modes = NULL;

    ErrorF("in RADEONProbeOutputModes\n");

    if (output->status == XF86OutputStatusConnected) {
	if (radeon_output->type == OUTPUT_DVI || radeon_output->type == OUTPUT_VGA) {
	    if (output->MonInfo) {
		edid_mon = xf86OutputGetEDID (output, radeon_output->pI2CBus);
		xf86OutputSetEDID (output, edid_mon);

		modes = xf86OutputGetEDIDModes (output);
		return modes;
	    }
	}
	if (radeon_output->type == OUTPUT_STV || radeon_output->type == OUTPUT_CTV) {
	    modes = RADEONTVModes(output);
	    return modes;
	}
	if (radeon_output->type == OUTPUT_LVDS) {
	    /* okay we got DDC info */
	    if (output->MonInfo) {
		edid_mon = xf86OutputGetEDID (output, radeon_output->pI2CBus);
		xf86OutputSetEDID (output, edid_mon);

		modes = xf86OutputGetEDIDModes (output);
	    }
	    if (modes == NULL) {
		modes = RADEONFPNativeMode(output);
		/* add the screen modes */
		RADEONAddScreenModes(output, &modes);
	    }
	    return modes;
	}
    }

    return modes;
}

