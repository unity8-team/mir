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

void
RADEONGetOriginalVirtualSize(ScrnInfoPtr pScrn, int *x, int *y)
{
    ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];

    *x = pScrn->virtualX;
    *y = pScrn->virtualY;
}

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

/* XFree86's xf86ValidateModes routine doesn't work well with DDC modes,
 * so here is our own validation routine.
 */
int RADEONValidateDDCModes(ScrnInfoPtr pScrn1, char **ppModeName,
			   RADEONMonitorType DisplayType, int crtc2)
{
    RADEONInfoPtr   info       = RADEONPTR(pScrn1);
    DisplayModePtr  p;
    DisplayModePtr  last       = NULL;
    DisplayModePtr  first      = NULL;
    DisplayModePtr  ddcModes   = NULL;
    int             count      = 0;
    int             i, width, height;
    ScrnInfoPtr pScrn = pScrn1;

    if (crtc2)
	pScrn = info->CRT2pScrn;

    pScrn->virtualX = pScrn1->display->virtualX;
    pScrn->virtualY = pScrn1->display->virtualY;

    if (pScrn->monitor->DDC) {
	int  maxVirtX = pScrn->virtualX;
	int  maxVirtY = pScrn->virtualY;

	/* Collect all of the DDC modes */
	first = last = ddcModes = xf86DDCGetModes(pScrn->scrnIndex, pScrn->monitor->DDC);

	for (p = ddcModes; p; p = p->next) {

	    /* If primary head is a flat panel, use RMX by default */
	    if ((!info->IsSecondary && DisplayType != MT_CRT) &&
		(!info->ddc_mode) && (!crtc2)) {
		/* These values are effective values after expansion.
		 * They are not really used to set CRTC registers.
		 */
		p->HTotal     = info->PanelXRes + info->HBlank;
		p->HSyncStart = info->PanelXRes + info->HOverPlus;
		p->HSyncEnd   = p->HSyncStart + info->HSyncWidth;
		p->VTotal     = info->PanelYRes + info->VBlank;
		p->VSyncStart = info->PanelYRes + info->VOverPlus;
		p->VSyncEnd   = p->VSyncStart + info->VSyncWidth;
		p->Clock      = info->DotClock;

		p->Flags     |= RADEON_USE_RMX;
	    }

	    maxVirtX = MAX(maxVirtX, p->HDisplay);
	    maxVirtY = MAX(maxVirtY, p->VDisplay);
	    count++;

	    last = p;
	}

	/* Match up modes that are specified in the XF86Config file */
	if (ppModeName[0]) {
	    DisplayModePtr  next;

	    /* Reset the max virtual dimensions */
	    maxVirtX = pScrn->virtualX;
	    maxVirtY = pScrn->virtualY;

	    /* Reset list */
	    first = last = NULL;

	    for (i = 0; ppModeName[i]; i++) {
		/* FIXME: Use HDisplay and VDisplay instead of mode string */
		if (sscanf(ppModeName[i], "%dx%d", &width, &height) == 2) {
		    for (p = ddcModes; p; p = next) {
			next = p->next;

			if (p->HDisplay == width && p->VDisplay == height) {
			    /* We found a DDC mode that matches the one
                               requested in the XF86Config file */
			    p->type |= M_T_USERDEF;

			    /* Update  the max virtual setttings */
			    maxVirtX = MAX(maxVirtX, width);
			    maxVirtY = MAX(maxVirtY, height);

			    /* Unhook from DDC modes */
			    if (p->prev) p->prev->next = p->next;
			    if (p->next) p->next->prev = p->prev;
			    if (p == ddcModes) ddcModes = p->next;

			    /* Add to used modes */
			    if (last) {
				last->next = p;
				p->prev = last;
			    } else {
				first = p;
				p->prev = NULL;
			    }
			    p->next = NULL;
			    last = p;

			    break;
			}
		    }
		}
	    }

	    /*
	     * Add remaining DDC modes if they're smaller than the user
	     * specified modes
	     */
	    for (p = ddcModes; p; p = next) {
		next = p->next;
		if (p->HDisplay <= maxVirtX && p->VDisplay <= maxVirtY) {
		    /* Unhook from DDC modes */
		    if (p->prev) p->prev->next = p->next;
		    if (p->next) p->next->prev = p->prev;
		    if (p == ddcModes) ddcModes = p->next;

		    /* Add to used modes */
		    if (last) {
			last->next = p;
			p->prev = last;
		    } else {
			first = p;
			p->prev = NULL;
		    }
		    p->next = NULL;
		    last = p;
		}
	    }

	    /* Delete unused modes */
	    while (ddcModes)
		xf86DeleteMode(&ddcModes, ddcModes);
	} else {
	    /*
	     * No modes were configured, so we make the DDC modes
	     * available for the user to cycle through.
	     */
	    for (p = ddcModes; p; p = p->next)
		p->type |= M_T_USERDEF;
	}

        if (crtc2) {
            pScrn->virtualX = maxVirtX;
            pScrn->virtualY = maxVirtY;
	} else {
	    pScrn->virtualX = pScrn->display->virtualX = maxVirtX;
	    pScrn->virtualY = pScrn->display->virtualY = maxVirtY;
	}
    }

    /* Close the doubly-linked mode list, if we found any usable modes */
    if (last) {
	last->next   = first;
	first->prev  = last;
	pScrn->modes = first;
	RADEONSetPitch(pScrn);
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Total number of valid DDC mode(s) found: %d\n", count);

    return count;
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
	RADEONSetPitch(pScrn);
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Total number of valid FP mode(s) found: %d\n", count);

    return count;
}


int RADEONValidateMergeModes(ScrnInfoPtr pScrn1)
{
    RADEONInfoPtr   info             = RADEONPTR(pScrn1);
    ClockRangePtr   clockRanges;
    int             modesFound;
    ScrnInfoPtr pScrn = info->CRT2pScrn;

    /* fill in pScrn2 */
    pScrn->videoRam = pScrn1->videoRam;
    pScrn->depth = pScrn1->depth;
    pScrn->numClocks = pScrn1->numClocks;
    pScrn->progClock = pScrn1->progClock;
    pScrn->fbFormat = pScrn1->fbFormat;
    pScrn->videoRam = pScrn1->videoRam;
    pScrn->maxHValue = pScrn1->maxHValue;
    pScrn->maxVValue = pScrn1->maxVValue;
    pScrn->xInc = pScrn1->xInc;

    if (info->NoVirtual) {
	pScrn1->display->virtualX = 0;
        pScrn1->display->virtualY = 0;
    }

    if (pScrn->monitor->DDC) {
        /* If we still don't know sync range yet, let's try EDID.
         *
         * Note that, since we can have dual heads, Xconfigurator
         * may not be able to probe both monitors correctly through
         * vbe probe function (RADEONProbeDDC). Here we provide an
         * additional way to auto-detect sync ranges if they haven't
         * been added to XF86Config manually.
         */
        if (pScrn->monitor->nHsync <= 0)
            RADEONSetSyncRangeFromEdid(pScrn, 1);
        if (pScrn->monitor->nVrefresh <= 0)
            RADEONSetSyncRangeFromEdid(pScrn, 0);
    }

    /* Get mode information */
    pScrn->progClock               = TRUE;
    clockRanges                    = xnfcalloc(sizeof(*clockRanges), 1);
    clockRanges->next              = NULL;
    clockRanges->minClock          = info->pll.min_pll_freq;
    clockRanges->maxClock          = info->pll.max_pll_freq * 10;
    clockRanges->clockIndex        = -1;
    clockRanges->interlaceAllowed  = (info->MergeType == MT_CRT);
    clockRanges->doubleScanAllowed = (info->MergeType == MT_CRT);

    /* We'll use our own mode validation routine for DFP/LCD, since
     * xf86ValidateModes does not work correctly with the DFP/LCD modes
     * 'stretched' from their native mode.
     */
    if (info->MergeType == MT_CRT && !info->ddc_mode) {
 
	modesFound =
	    xf86ValidateModes(pScrn,
			      pScrn->monitor->Modes,
			      pScrn1->display->modes,
			      clockRanges,
			      NULL,                  /* linePitches */
			      8 * 64,                /* minPitch */
			      8 * 1024,              /* maxPitch */
			      info->allowColorTiling ? 2048 :
			          64 * pScrn1->bitsPerPixel, /* pitchInc */
			      128,                   /* minHeight */
			      info->MaxLines,        /* maxHeight */
			      pScrn1->display->virtualX ? pScrn1->virtualX : 0,
			      pScrn1->display->virtualY ? pScrn1->virtualY : 0,
			      info->FbMapSize,
			      LOOKUP_BEST_REFRESH);

	if (modesFound == -1) return 0;

	xf86PruneDriverModes(pScrn);
	if (!modesFound || !pScrn->modes) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
	    return 0;
	}

    } else {
	/* First, free any allocated modes during configuration, since
	 * we don't need them
	 */
	while (pScrn->modes)
	    xf86DeleteMode(&pScrn->modes, pScrn->modes);
	while (pScrn->modePool)
	    xf86DeleteMode(&pScrn->modePool, pScrn->modePool);

	/* Next try to add DDC modes */
	modesFound = RADEONValidateDDCModes(pScrn, pScrn1->display->modes,
					    info->MergeType, 1);

	/* If that fails and we're connect to a flat panel, then try to
         * add the flat panel modes
	 */
	if (info->MergeType != MT_CRT) {
	    
	    /* some panels have DDC, but don't have internal scaler.
	     * in this case, we need to validate additional modes
	     * by using on-chip RMX.
	     */
	    int user_modes_asked = 0, user_modes_found = 0, i;
	    DisplayModePtr  tmp_mode = pScrn->modes;
	    while (pScrn1->display->modes[user_modes_asked]) user_modes_asked++;	    
	    if (tmp_mode) {
		for (i = 0; i < modesFound; i++) {
		    if (tmp_mode->type & M_T_USERDEF) user_modes_found++;
		    tmp_mode = tmp_mode->next;
		}
	    }

 	    if ((modesFound <= 1) || (user_modes_found < user_modes_asked)) {
		/* when panel size is not valid, try to validate 
		 * mode using xf86ValidateModes routine
		 * This can happen when DDC is disabled.
		 */
		/* if (info->PanelXRes < 320 || info->PanelYRes < 200) */
		    modesFound =
			xf86ValidateModes(pScrn,
					  pScrn->monitor->Modes,
					  pScrn1->display->modes,
					  clockRanges,
					  NULL,                  /* linePitches */
					  8 * 64,                /* minPitch */
					  8 * 1024,              /* maxPitch */
					  info->allowColorTiling ? 2048 :
					      64 * pScrn1->bitsPerPixel, /* pitchInc */
					  128,                   /* minHeight */
					  info->MaxLines,        /* maxHeight */
					  pScrn1->display->virtualX,
					  pScrn1->display->virtualY,
					  info->FbMapSize,
					  LOOKUP_BEST_REFRESH);

	    } 
        }

	/* Setup the screen's clockRanges for the VidMode extension */
	if (!pScrn->clockRanges) {
	    pScrn->clockRanges = xnfcalloc(sizeof(*(pScrn->clockRanges)), 1);
	    memcpy(pScrn->clockRanges, clockRanges, sizeof(*clockRanges));
	    pScrn->clockRanges->strategy = LOOKUP_BEST_REFRESH;
	}

	/* Fail if we still don't have any valid modes */
	if (modesFound < 1) {
	    if (info->MergeType == MT_CRT) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No valid DDC modes found for this CRT\n");
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Try turning off the \"DDCMode\" option\n");
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "No valid mode found for this DFP/LCD\n");
	    }
	    return 0;
	}
    }
    return modesFound;
}

void
RADEONProbeOutputModes(xf86OutputPtr output)
{
    ScrnInfoPtr	    pScrn = output->scrn;
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR (pScrn);
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    RADEONOutputPrivatePtr pRPort = output->driver_private;
    int i;
    DisplayModePtr ddc_modes, mode;
    DisplayModePtr test;

    /* force reprobe */
    pRPort->MonType = MT_UNKNOWN;
	
    RADEONConnectorFindMonitor(pScrn, output);
    
    /* okay we got DDC info */
    if (output->MonInfo) {
      /* Debug info for now, at least */
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID for output %d\n", i);
      xf86PrintEDID(output->MonInfo);
      
      ddc_modes = xf86DDCGetModes(pScrn->scrnIndex, output->MonInfo);
      
      for (mode = ddc_modes; mode != NULL; mode = mode->next) {
	if (mode->Flags & V_DBLSCAN) {
	  if ((mode->CrtcHDisplay >= 1024) || (mode->CrtcVDisplay >= 768))
	    mode->status = MODE_CLOCK_RANGE;
	}
      }
      RADEONxf86PruneInvalidModes(pScrn, &ddc_modes, TRUE);
      
      /* do some physcial size stuff */
    }
    
    
    if (output->probed_modes == NULL) {
      MonRec fixed_mon;
      DisplayModePtr modes;
      
      switch(pRPort->MonType) {
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
	
	modes = RADEONxf86DuplicateModes(pScrn, pScrn->monitor->Modes);
	RADEONxf86ValidateModesSync(pScrn, modes, &fixed_mon);
	RADEONxf86PruneInvalidModes(pScrn, &modes, TRUE);
	/* fill out CRT of FP mode table */
	pRADEONEnt->pOutput[i]->probed_modes = modes;
	break;
	
      case MT_LCD:
	RADEONValidateFPModes(pScrn, pScrn->display->modes, &output->probed_modes);
	break;
      default:
	break;
      }
    }
    
    if (output->probed_modes) {
      RADEONxf86ValidateModesUserConfig(pScrn,
					output->probed_modes);
      RADEONxf86PruneInvalidModes(pScrn, &output->probed_modes,
				  FALSE);
    }
}
  

/**
 * Constructs pScrn->modes from the output mode lists.
 *
 * Currently it only takes one output's mode list and stuffs it into the
 * XFree86 DDX mode list while trimming it for root window size.
 *
 * This should be obsoleted by RandR 1.2 hopefully.
 */
void
RADEON_set_xf86_modes_from_outputs(ScrnInfoPtr pScrn)
{
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR (pScrn);
    RADEONInfoPtr info       = RADEONPTR(pScrn);
    RADEONEntPtr pRADEONEnt  = RADEONEntPriv(pScrn);
    DisplayModePtr saved_mode, last;
    int originalVirtualX, originalVirtualY;
    int i;

    /* Remove the current mode from the modelist if we're re-validating, so we
     * can find a new mode to map ourselves to afterwards.
     */
    saved_mode = info->currentMode;
    if (saved_mode != NULL) {
	RADEONxf86DeleteModeFromList(&pScrn->modes, saved_mode);
    }

    /* Clear any existing modes from pScrn->modes */
    while (pScrn->modes != NULL)
	xf86DeleteMode(&pScrn->modes, pScrn->modes);

    /* Set pScrn->modes to the mode list for an arbitrary output.
     * pScrn->modes should only be used for XF86VidMode now, which we don't
     * care about enough to make some sort of unioned list.
     */
    for (i = 0; i < config->num_output; i++) {
        xf86OutputPtr output = config->output[i];
	if (output->probed_modes != NULL) {
	    pScrn->modes =
		RADEONxf86DuplicateModes(pScrn, output->probed_modes);
	    break;
	}
    }

    RADEONGetOriginalVirtualSize(pScrn, &originalVirtualX, &originalVirtualY);

    /* Disable modes in the XFree86 DDX list that are larger than the current
     * virtual size.
     */
    RADEONxf86ValidateModesSize(pScrn, pScrn->modes,
			      originalVirtualX, originalVirtualY,
			      pScrn->displayWidth);

    /* Strip out anything that we threw out for virtualX/Y. */
    RADEONxf86PruneInvalidModes(pScrn, &pScrn->modes, TRUE);

    if (pScrn->modes == NULL) {
	FatalError("No modes left for XFree86 DDX\n");
    }

    /* For some reason, pScrn->modes is circular, unlike the other mode lists.
     * How great is that?
     */
    last = RADEONGetModeListTail(pScrn->modes);
    last->next = pScrn->modes;
    pScrn->modes->prev = last;

    /* Save a pointer to the previous current mode.  We can't reset
     * pScrn->currentmode, because we rely on xf86SwitchMode's shortcut not
     * happening so we can hot-enable devices at SwitchMode.  We'll notice this
     * case at SwitchMode and free the saved mode.
     */
    info->savedCurrentMode = saved_mode;
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



int RADEONValidateXF86ModeList(ScrnInfoPtr pScrn, Bool first_time)
{
  RADEONProbeOutputModes(pScrn);

  if (first_time)
    {
      RADEON_set_default_screen_size(pScrn);
    }

  RADEON_set_xf86_modes_from_outputs(pScrn);
  return 1;
}
