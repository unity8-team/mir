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

#include "radeon_xf86Modes.h"
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
}


/* This is used only when no mode is specified for FP and no ddc is
 * available.  We force it to native mode, if possible.
 */
static DisplayModePtr RADEONFPNativeMode(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr   info  = RADEONPTR(pScrn);
    DisplayModePtr  new   = NULL;
    char            stmp[32];

    if (info->PanelXRes != 0 &&
	info->PanelYRes != 0 &&
	info->DotClock != 0) {

	/* Add native panel size */
	new             = xnfcalloc(1, sizeof (DisplayModeRec));
	sprintf(stmp, "%dx%d", info->PanelXRes, info->PanelYRes);
	new->name       = xnfalloc(strlen(stmp) + 1);
	strcpy(new->name, stmp);
	new->HDisplay   = info->PanelXRes;
	new->VDisplay   = info->PanelYRes;

	new->HTotal     = new->HDisplay + info->HBlank;
	new->HSyncStart = new->HDisplay + info->HOverPlus;
	new->HSyncEnd   = new->HSyncStart + info->HSyncWidth;
	new->VTotal     = new->VDisplay + info->VBlank;
	new->VSyncStart = new->VDisplay + info->VOverPlus;
	new->VSyncEnd   = new->VSyncStart + info->VSyncWidth;

	new->Clock      = info->DotClock;
	new->Flags      = 0;
	new->type       = M_T_USERDEF;

	new->next       = NULL;
	new->prev       = NULL;

	pScrn->display->virtualX =
	    pScrn->virtualX = MAX(pScrn->virtualX, info->PanelXRes);
	pScrn->display->virtualY =
	    pScrn->virtualY = MAX(pScrn->virtualY, info->PanelYRes);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "No valid mode specified, force to native mode\n");
    }

    return new;
}

/* FP mode initialization routine for using on-chip RMX to scale
 */
int RADEONValidateFPModes(ScrnInfoPtr pScrn, char **ppModeName, DisplayModePtr *modeList)
{
    RADEONInfoPtr   info       = RADEONPTR(pScrn);
    DisplayModePtr  last       = NULL;
    DisplayModePtr  new        = NULL;
    DisplayModePtr  first      = NULL;
    DisplayModePtr  p, tmp;
    int             count      = 0;
    int             i, width, height;

    pScrn->virtualX = pScrn->display->virtualX;
    pScrn->virtualY = pScrn->display->virtualY;

    /* We have a flat panel connected to the primary display, and we
     * don't have any DDC info.
     */
    for (i = 0; ppModeName[i] != NULL; i++) {

	if (sscanf(ppModeName[i], "%dx%d", &width, &height) != 2) continue;

	/* Note: We allow all non-standard modes as long as they do not
	 * exceed the native resolution of the panel.  Since these modes
	 * need the internal RMX unit in the video chips (and there is
	 * only one per card), this will only apply to the primary head.
	 */
	if (width < 320 || width > info->PanelXRes ||
	    height < 200 || height > info->PanelYRes) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Mode %s is out of range.\n", ppModeName[i]);
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Valid modes must be between 320x200-%dx%d\n",
		       info->PanelXRes, info->PanelYRes);
	    continue;
	}

	new             = xnfcalloc(1, sizeof(DisplayModeRec));
	new->name       = xnfalloc(strlen(ppModeName[i]) + 1);
	strcpy(new->name, ppModeName[i]);
	new->HDisplay   = width;
	new->VDisplay   = height;

	/* These values are effective values after expansion They are
	 * not really used to set CRTC registers.
	 */
	new->HTotal     = info->PanelXRes + info->HBlank;
	new->HSyncStart = info->PanelXRes + info->HOverPlus;
	new->HSyncEnd   = new->HSyncStart + info->HSyncWidth;
	new->VTotal     = info->PanelYRes + info->VBlank;
	new->VSyncStart = info->PanelYRes + info->VOverPlus;
	new->VSyncEnd   = new->VSyncStart + info->VSyncWidth;
	new->Clock      = info->DotClock;
	new->Flags     |= RADEON_USE_RMX;

#ifdef M_T_PREFERRED
	if (width == info->PanelXRes && height == info->PanelYRes)
	  new->type |= M_T_PREFERRED;
#endif

	new->type      |= M_T_USERDEF;

	new->next       = NULL;
	new->prev       = last;

	if (last) last->next = new;
	last = new;
	if (!first) first = new;

	pScrn->display->virtualX =
	    pScrn->virtualX = MAX(pScrn->virtualX, width);
	pScrn->display->virtualY =
	    pScrn->virtualY = MAX(pScrn->virtualY, height);
	count++;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Valid mode using on-chip RMX: %s\n", new->name);
    }

    /* If all else fails, add the native mode */
    if (!count) {
	first = last = RADEONFPNativeMode(pScrn);
	if (first) count = 1;
    }

    /* add in all default vesa modes smaller than panel size, used for randr*/
    for (p = *modeList; p && p->next; p = p->next->next) {
	if ((p->HDisplay <= info->PanelXRes) && (p->VDisplay <= info->PanelYRes)) {
	    tmp = first;
	    while (tmp) {
		if ((p->HDisplay == tmp->HDisplay) && (p->VDisplay == tmp->VDisplay)) break;
		tmp = tmp->next;
	    }
	    if (!tmp) {
		new             = xnfcalloc(1, sizeof(DisplayModeRec));
		new->name       = xnfalloc(strlen(p->name) + 1);
		strcpy(new->name, p->name);
		new->HDisplay   = p->HDisplay;
		new->VDisplay   = p->VDisplay;

		/* These values are effective values after expansion They are
		 * not really used to set CRTC registers.
		 */
		new->HTotal     = info->PanelXRes + info->HBlank;
		new->HSyncStart = info->PanelXRes + info->HOverPlus;
		new->HSyncEnd   = new->HSyncStart + info->HSyncWidth;
		new->VTotal     = info->PanelYRes + info->VBlank;
		new->VSyncStart = info->PanelYRes + info->VOverPlus;
		new->VSyncEnd   = new->VSyncStart + info->VSyncWidth;
		new->Clock      = info->DotClock;
		new->Flags     |= RADEON_USE_RMX;

		new->type      |= M_T_DEFAULT;

		new->next       = NULL;
		new->prev       = last;

		if (last) last->next = new;
		last = new;
		if (!first) first = new;
	    }
	}
    }

    /* Close the doubly-linked mode list, if we found any usable modes */
    if (last) {
	last->next   = NULL; //first;
	first->prev  = NULL; //last;
	*modeList = first;
	//RADEONSetPitch(pScrn);
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Total number of valid FP mode(s) found: %d\n", count);

    return count;
}

void
RADEONProbeOutputModes(xf86OutputPtr output)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR (pScrn);
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    DisplayModePtr ddc_modes, mode;
    DisplayModePtr test;

    /* force reprobe */
    radeon_output->MonType = MT_UNKNOWN;
	
    RADEONConnectorFindMonitor(pScrn, output);
    
    /* okay we got DDC info */
    if (output->MonInfo) {
      /* Debug info for now, at least */
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID for output %d\n", radeon_output->num);
      xf86PrintEDID(output->MonInfo);
      
      ddc_modes = xf86DDCGetModes(pScrn->scrnIndex, output->MonInfo);
      
      for (mode = ddc_modes; mode != NULL; mode = mode->next) {
	if (mode->Flags & V_DBLSCAN) {
	  if ((mode->CrtcHDisplay >= 1024) || (mode->CrtcVDisplay >= 768))
	    mode->status = MODE_CLOCK_RANGE;
	}
      }
      xf86PruneInvalidModes(pScrn, &ddc_modes, TRUE);
      
      /* do some physcial size stuff */
    }
    
    
    if (output->probed_modes == NULL) {
      MonRec fixed_mon;
      DisplayModePtr modes;
      
      switch(radeon_output->MonType) {
      case MT_CRT:
      case MT_DFP:
	
	/* We've got a potentially-connected monitor that we can't DDC.  Return a
	 * fixed set of VESA plus user modes for a presumed multisync monitor with
	 * some reasonable limits.
	 */
	fixed_mon.nHsync = 1;
	fixed_mon.hsync[0].lo = 31.0;
	fixed_mon.hsync[0].hi = 100.0;
	fixed_mon.nVrefresh = 1;
	fixed_mon.vrefresh[0].lo = 50.0;
	fixed_mon.vrefresh[0].hi = 70.0;
	
	modes = xf86DuplicateModes(pScrn, pScrn->monitor->Modes);
	xf86ValidateModesSync(pScrn, modes, &fixed_mon);
	xf86PruneInvalidModes(pScrn, &modes, TRUE);
	/* fill out CRT of FP mode table */
	output->probed_modes = modes;
	break;
	
      case MT_LCD:
	RADEONValidateFPModes(pScrn, pScrn->display->modes, &output->probed_modes);
	break;
      default:
	break;
      }
    }
    
    if (output->probed_modes) {
      xf86ValidateModesUserConfig(pScrn,
					output->probed_modes);
      xf86PruneInvalidModes(pScrn, &output->probed_modes,
				  FALSE);
    }
}

/**
 * Takes the output mode lists and decides the default root window size
 * and framebuffer pitch.
 */
void
RADEON_set_default_screen_size(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    int maxX = -1, maxY = -1;
    int i;

    /* Set up a virtual size that will cover any clone mode we'd want to
     * set for the currently-connected outputs.
     */
    for (i = 0; i < RADEON_MAX_CONNECTOR; i++) {
	DisplayModePtr mode;

	for (mode = pRADEONEnt->pOutput[i]->probed_modes; mode != NULL;
	     mode = mode->next)
	{
	    if (mode->HDisplay > maxX)
		maxX = mode->HDisplay;
	    if (mode->VDisplay > maxY)
		maxY = mode->VDisplay;
	}
    }
    /* let the user specify a bigger virtual size if they like */
    if (pScrn->display->virtualX > maxX)
	maxX = pScrn->display->virtualX;
    if (pScrn->display->virtualY > maxY)
	maxY = pScrn->display->virtualY;
    pScrn->virtualX = maxX;
    pScrn->virtualY = maxY;
    pScrn->displayWidth = (maxX + 63) & ~63;
}



