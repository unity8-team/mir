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
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_version.h"

				/* DDC support */
#include "xf86DDC.h"

/* Established timings from EDID standard */
static struct
{
    int hsize;
    int vsize;
    int refresh;
} est_timings[] = {
    {1280, 1024, 75},
    {1024, 768, 75},
    {1024, 768, 70},
    {1024, 768, 60},
    {1024, 768, 87},
    {832, 624, 75},
    {800, 600, 75},
    {800, 600, 72},
    {800, 600, 60},
    {800, 600, 56},
    {640, 480, 75},
    {640, 480, 72},
    {640, 480, 67},
    {640, 480, 60},
    {720, 400, 88},
    {720, 400, 70},
};

/* This function will sort all modes according to their resolution.
 * Highest resolution first.
 */
static void RADEONSortModes(DisplayModePtr *new, DisplayModePtr *first,
			    DisplayModePtr *last)
{
    DisplayModePtr  p;

    p = *last;
    while (p) {
	if (((*new)->HDisplay < p->HDisplay) ||
	    (((*new)->HDisplay == p->HDisplay) &&
	     ((*new)->VDisplay < p->VDisplay)) ||
	    (((*new)->HDisplay == p->HDisplay) &&
	     ((*new)->VDisplay == p->VDisplay) &&
	     ((*new)->type < p->type) && 
	     !(((*new)->type == M_T_USERDEF) || (!(*new)->type))) ||
	    (((*new)->HDisplay == p->HDisplay) &&
	     ((*new)->VDisplay == p->VDisplay) &&
	     ((*new)->type == p->type) && 
	     ((*new)->Clock < p->Clock))) {

	    if (p->next) p->next->prev = *new;
	    (*new)->prev = p;
	    (*new)->next = p->next;
	    p->next = *new;
	    if (!((*new)->next)) *last = *new;
	    break;
	}
	if (!p->prev) {
	    (*new)->prev = NULL;
	    (*new)->next = p;
	    p->prev = *new;
	    *first = *new;
	    break;
	}
	p = p->prev;
    }

    if (!*first) {
	*first = *new;
	(*new)->prev = NULL;
	(*new)->next = NULL;
	*last = *new;
    }
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

/* When no mode provided in config file, this will add all modes supported in
 * DDC date the pScrn->modes list
 */
static DisplayModePtr RADEONDDCModes(ScrnInfoPtr pScrn, xf86MonPtr ddc)
{
    DisplayModePtr  p;
    DisplayModePtr  last  = NULL;
    DisplayModePtr  new   = NULL;
    DisplayModePtr  first = NULL;
    int             count = 0;
    int             j, tmp;
    char            stmp[32];

    /* Go thru detailed timing table first */
    for (j = 0; j < 4; j++) {
	if (ddc->det_mon[j].type == 0) {
	    struct detailed_timings *d_timings =
		&ddc->det_mon[j].section.d_timings;

	    if (d_timings->h_active == 0 || d_timings->v_active == 0) break;

	    new = xnfcalloc(1, sizeof (DisplayModeRec));
	    memset(new, 0, sizeof (DisplayModeRec));

	    new->HDisplay   = d_timings->h_active;
	    new->VDisplay   = d_timings->v_active;

	    sprintf(stmp, "%dx%d", new->HDisplay, new->VDisplay);
	    new->name       = xnfalloc(strlen(stmp) + 1);
	    strcpy(new->name, stmp);

	    new->HTotal     = new->HDisplay + d_timings->h_blanking;
	    new->HSyncStart = new->HDisplay + d_timings->h_sync_off;
	    new->HSyncEnd   = new->HSyncStart + d_timings->h_sync_width;
	    new->VTotal     = new->VDisplay + d_timings->v_blanking;
	    new->VSyncStart = new->VDisplay + d_timings->v_sync_off;
	    new->VSyncEnd   = new->VSyncStart + d_timings->v_sync_width;
	    new->Clock      = d_timings->clock / 1000;
	    new->Flags      = (d_timings->interlaced ? V_INTERLACE : 0);
	    new->status     = MODE_OK;
#ifdef M_T_PREFERRED
	    if (PREFERRED_TIMING_MODE(ddc->features.msc))
	      new->type     |= M_T_PREFERRED;
#endif
#ifdef M_T_DRIVER
	      new->type     |= M_T_DRIVER;
#endif

	    switch (d_timings->misc) {
	    case 0: new->Flags |= V_NHSYNC | V_NVSYNC; break;
	    case 1: new->Flags |= V_PHSYNC | V_NVSYNC; break;
	    case 2: new->Flags |= V_NHSYNC | V_PVSYNC; break;
	    case 3: new->Flags |= V_PHSYNC | V_PVSYNC; break;
	    }
	    count++;

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Valid Mode from Detailed timing table: %s\n",
		       new->name);

	    RADEONSortModes(&new, &first, &last);
	}
    }

    /* Search thru standard VESA modes from EDID */
    for (j = 0; j < 8; j++) {
        if (ddc->timings2[j].hsize == 0 || ddc->timings2[j].vsize == 0)
               continue;
	for (p = pScrn->monitor->Modes; p; p = p->next) {
	    /* Ignore all double scan modes */
	    if (p->Flags & V_DBLSCAN)
		continue;
	    if ((ddc->timings2[j].hsize == p->HDisplay) &&
		(ddc->timings2[j].vsize == p->VDisplay)) {
		float  refresh =
		    (float)p->Clock * 1000.0 / p->HTotal / p->VTotal;

		if (abs((float)ddc->timings2[j].refresh - refresh) < 1.0) {
		    /* Is this good enough? */
		    new = xnfcalloc(1, sizeof (DisplayModeRec));
		    memcpy(new, p, sizeof(DisplayModeRec));
		    new->name = xnfalloc(strlen(p->name) + 1);
		    strcpy(new->name, p->name);
		    new->status = MODE_OK;
		    if ((new->type != M_T_USERDEF) && (new->type))
		    	new->type   = M_T_DEFAULT;

		    count++;

		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			       "Valid Mode from standard timing table: %s\n",
			       new->name);

		    RADEONSortModes(&new, &first, &last);
		    break;
		}
	    }
	}
    }

    /* Search thru established modes from EDID */
    tmp = (ddc->timings1.t1 << 8) | ddc->timings1.t2;
    for (j = 0; j < 16; j++) {
	if (tmp & (1 << j)) {
	    for (p = pScrn->monitor->Modes; p; p = p->next) {
		/* Ignore all double scan modes */
		if (p->Flags & V_DBLSCAN)
		    continue;
		if ((est_timings[j].hsize == p->HDisplay) &&
		    (est_timings[j].vsize == p->VDisplay)) {
		    float  refresh =
			(float)p->Clock * 1000.0 / p->HTotal / p->VTotal;

		    if (abs((float)est_timings[j].refresh - refresh) < 1.0) {
			/* Is this good enough? */
			new = xnfcalloc(1, sizeof (DisplayModeRec));
			memcpy(new, p, sizeof(DisplayModeRec));
			new->name = xnfalloc(strlen(p->name) + 1);
			strcpy(new->name, p->name);
			new->status = MODE_OK;
		    	if ((new->type != M_T_USERDEF) && (new->type))
		    	    new->type   = M_T_DEFAULT;

			count++;

			xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				   "Valid Mode from established timing "
				   "table: %s\n", new->name);

			RADEONSortModes(&new, &first, &last);
			break;
		    }
		}
	    }
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Total of %d mode(s) found.\n", count);

    return first;
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
    int             maxXRes    = 0;
    int             maxYRes    = 0;
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
	first = last = ddcModes = RADEONDDCModes(pScrn, pScrn->monitor->DDC);

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

	    maxXRes = maxVirtX = MAX(maxVirtX, p->HDisplay);
	    maxYRes = maxVirtY = MAX(maxVirtY, p->VDisplay);
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
			    RADEONSortModes(&p, &first, &last);

			    break;
			}
		    }
		    if (!p) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   " %dx%d is not supported by the device\n",
				   width, height);
		    }
		}
	    }
	    /* just for sanity check, if maxVirtX and maxVirtY are not
	     * specified, set max resolution that panel support for the max
	     * virtual dimensions */
	    if ((!maxVirtX) || (!maxVirtY)) {
		maxVirtX = maxXRes;
		maxVirtY = maxYRes;
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
		    RADEONSortModes(&p, &first, &last);
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
int RADEONValidateFPModes(ScrnInfoPtr pScrn, char **ppModeName)
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

	RADEONSortModes(&new, &first, &last);

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
    for (p = pScrn->monitor->Modes; p; p = p->next) {
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

		RADEONSortModes(&new, &first, &last);
	    }
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
	xf86SetDDCproperties(pScrn, pScrn->monitor->DDC); 
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
