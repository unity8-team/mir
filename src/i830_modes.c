#define DEBUG_VERB 2
/*
 * Copyright © 2002 David Dawes
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the author(s) shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from
 * the author(s).
 *
 * Authors: David Dawes <dawes@xfree86.org>
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/vbe/vbeModes.c,v 1.6 2002/11/02 01:38:25 dawes Exp $
 */
/*
 * Modified by Alan Hourihane <alanh@tungstengraphics.com>
 * to support extended BIOS modes for the Intel chipsets
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "xf86.h"
#include "i830.h"

#include <math.h>

#define rint(x) floor(x)

#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define MARGIN_PERCENT    1.8   /* % of active vertical image                */
#define CELL_GRAN         8.0   /* assumed character cell granularity        */
#define MIN_PORCH         1     /* minimum front porch                       */
#define V_SYNC_RQD        3     /* width of vsync in lines                   */
#define H_SYNC_PERCENT    8.0   /* width of hsync as % of total line         */
#define MIN_VSYNC_PLUS_BP 550.0 /* min time of vsync + back porch (microsec) */
#define M                 600.0 /* blanking formula gradient                 */
#define C                 40.0  /* blanking formula offset                   */
#define K                 128.0 /* blanking formula scaling factor           */
#define J                 20.0  /* blanking formula scaling factor           */

/* C' and M' are part of the Blanking Duty Cycle computation */

#define C_PRIME           (((C - J) * K/256.0) + J)
#define M_PRIME           (K/256.0 * M)
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

extern const int i830refreshes[];

void
I830PrintModes(ScrnInfoPtr scrp)
{
    DisplayModePtr p;
    float hsync, refresh = 0;
    char *desc, *desc2, *prefix, *uprefix;

    if (scrp == NULL)
	return;

    xf86DrvMsg(scrp->scrnIndex, scrp->virtualFrom, "Virtual size is %dx%d "
	       "(pitch %d)\n", scrp->virtualX, scrp->virtualY,
	       scrp->displayWidth);
    
    p = scrp->modes;
    if (p == NULL)
	return;

    do {
	desc = desc2 = "";
	if (p->HSync > 0.0)
	    hsync = p->HSync;
	else if (p->HTotal > 0)
	    hsync = (float)p->Clock / (float)p->HTotal;
	else
	    hsync = 0.0;
	if (p->VTotal > 0)
	    refresh = hsync * 1000.0 / p->VTotal;
	if (p->Flags & V_INTERLACE) {
	    refresh *= 2.0;
	    desc = " (I)";
	}
	if (p->Flags & V_DBLSCAN) {
	    refresh /= 2.0;
	    desc = " (D)";
	}
	if (p->VScan > 1) {
	    refresh /= p->VScan;
	    desc2 = " (VScan)";
	}
	if (p->VRefresh > 0.0)
	    refresh = p->VRefresh;
	if (p->type & M_T_BUILTIN)
	    prefix = "Built-in mode";
	else if (p->type & M_T_DEFAULT)
	    prefix = "Default mode";
	else
	    prefix = "Mode";
	if (p->type & M_T_USERDEF)
	    uprefix = "*";
	else
	    uprefix = " ";
	if (p->name)
	    xf86DrvMsg(scrp->scrnIndex, X_CONFIG,
			   "%s%s \"%s\"\n", uprefix, prefix, p->name);
	else
	    xf86DrvMsg(scrp->scrnIndex, X_PROBED,
			   "%s%s %dx%d (unnamed)\n",
			   uprefix, prefix, p->HDisplay, p->VDisplay);
	p = p->next;
    } while (p != NULL && p != scrp->modes);
}

/* This function will sort all modes according to their resolution.
 * Highest resolution first.
 */
void
I830xf86SortModes(DisplayModePtr *new, DisplayModePtr *first,
	      DisplayModePtr *last)
{
    DisplayModePtr  p;

    p = *last;
    while (p) {
	if ((((*new)->HDisplay < p->HDisplay) &&
	     ((*new)->VDisplay < p->VDisplay)) ||
	    (((*new)->HDisplay == p->HDisplay) &&
	     ((*new)->VDisplay == p->VDisplay) &&
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

DisplayModePtr I830xf86DDCModes(ScrnInfoPtr pScrn)
{
    DisplayModePtr  p;
    DisplayModePtr  last  = NULL;
    DisplayModePtr  new   = NULL;
    DisplayModePtr  first = NULL;
    int             count = 0;
    int             j, tmp;
    char            stmp[32];
    xf86MonPtr      ddc   = pScrn->monitor->DDC;

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
	    new->type       = M_T_DEFAULT;

	    if (d_timings->sync == 3) {
		switch (d_timings->misc) {
		case 0: new->Flags |= V_NHSYNC | V_NVSYNC; break;
		case 1: new->Flags |= V_PHSYNC | V_NVSYNC; break;
		case 2: new->Flags |= V_NHSYNC | V_PVSYNC; break;
		case 3: new->Flags |= V_PHSYNC | V_PVSYNC; break;
		}
	    }
	    count++;

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Valid Mode from Detailed timing table: %s (ht %d hss %d hse %d vt %d vss %d vse %d)\n",
		       new->name,
		       new->HTotal, new->HSyncStart, new->HSyncEnd,
		       new->VTotal, new->VSyncStart, new->VSyncEnd);

	    I830xf86SortModes(&new, &first, &last);
	}
    }

    /* Search thru standard VESA modes from EDID */
    for (j = 0; j < 8; j++) {
        if (ddc->timings2[j].hsize == 0 || ddc->timings2[j].vsize == 0)
               continue;
	for (p = pScrn->monitor->Modes; p && p->next; p = p->next->next) {
	    /* Ignore all double scan modes */
	    if ((ddc->timings2[j].hsize == p->HDisplay) &&
		(ddc->timings2[j].vsize == p->VDisplay)) {
		float  refresh =
		    (float)p->Clock * 1000.0 / p->HTotal / p->VTotal;
		float err = (float)ddc->timings2[j].refresh - refresh;

		if (err < 0) err = -err;
		if (err < 1.0) {
		    /* Is this good enough? */
		    new = xnfcalloc(1, sizeof (DisplayModeRec));
		    memcpy(new, p, sizeof(DisplayModeRec));
		    new->name = xnfalloc(strlen(p->name) + 1);
		    strcpy(new->name, p->name);
		    new->status = MODE_OK;
		    new->type   = M_T_DEFAULT;

		    count++;

		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			       "Valid Mode from standard timing table: %s\n",
			       new->name);

		    I830xf86SortModes(&new, &first, &last);
		    break;
		}
	    }
	}
    }

    /* Search thru established modes from EDID */
    tmp = (ddc->timings1.t1 << 8) | ddc->timings1.t2;
    for (j = 0; j < 16; j++) {
	if (tmp & (1 << j)) {
	    for (p = pScrn->monitor->Modes; p && p->next; p = p->next->next) {
		if ((est_timings[j].hsize == p->HDisplay) &&
		    (est_timings[j].vsize == p->VDisplay)) {
		    float  refresh =
			(float)p->Clock * 1000.0 / p->HTotal / p->VTotal;
		    float err = (float)est_timings[j].refresh - refresh;

		    if (err < 1.0) {
			/* Is this good enough? */
			new = xnfcalloc(1, sizeof (DisplayModeRec));
			memcpy(new, p, sizeof(DisplayModeRec));
			new->name = xnfalloc(strlen(p->name) + 1);
			strcpy(new->name, p->name);
			new->status = MODE_OK;
			new->type   = M_T_DEFAULT;

			count++;

			xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				   "Valid Mode from established timing "
				   "table: %s\n", new->name);

			I830xf86SortModes(&new, &first, &last);
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
int I830xf86ValidateDDCModes(ScrnInfoPtr pScrn1, char **ppModeName)
{
    DisplayModePtr  p;
    DisplayModePtr  last       = NULL;
    DisplayModePtr  first      = NULL;
    DisplayModePtr  ddcModes   = NULL;
    int             count      = 0;
    int             i, width, height;
    ScrnInfoPtr pScrn = pScrn1;

    pScrn->virtualX = pScrn1->display->virtualX;
    pScrn->virtualY = pScrn1->display->virtualY;

    if (pScrn->monitor->DDC) {
	int  maxVirtX = pScrn->virtualX;
	int  maxVirtY = pScrn->virtualY;

	/* Collect all of the DDC modes */
	first = last = ddcModes = I830xf86DDCModes(pScrn);

	for (p = ddcModes; p; p = p->next) {

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

	pScrn->virtualX = pScrn->display->virtualX = maxVirtX;
	pScrn->virtualY = pScrn->display->virtualY = maxVirtY;
    }

    /* Close the doubly-linked mode list, if we found any usable modes */
    if (last) {
      DisplayModePtr  temp      = NULL;
        /* we should add these to pScrn monitor modes */
      last->next   = pScrn->monitor->Modes;
      temp = pScrn->monitor->Modes->prev;
      pScrn->monitor->Modes->prev = first;
      pScrn->monitor->Modes->prev = last;

      first->prev = temp;
      if (temp)
	temp->next = first;

      pScrn->monitor->Modes = first;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Total number of valid DDC mode(s) found: %d\n", count);

    return count;
}
