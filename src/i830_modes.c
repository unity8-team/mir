/* -*- c-basic-offset: 4 -*- */

#define DEBUG_VERB 2
/*
 * Copyright © 2002 David Dawes
 * Copyright © 2006 Intel Corporation
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
 *	    Eric Anholt <eric.anholt@intel.com>
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
#include <assert.h>
#include <math.h>

#include "xf86.h"
#include "i830.h"
#include "i830_display.h"
#include "i830_xf86Modes.h"
#include <randrstr.h>

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

#define DEBUG_REPROBE 1

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

/**
 * Allocates and returns a copy of pMode, including pointers within pMode.
 */
static DisplayModePtr
I830DuplicateMode(DisplayModePtr pMode)
{
    DisplayModePtr pNew;

    pNew = xnfalloc(sizeof(DisplayModeRec));
    *pNew = *pMode;
    pNew->next = NULL;
    pNew->prev = NULL;
    if (pNew->name == NULL) {
	i830xf86SetModeDefaultName(pMode);
    } else {
	pNew->name = xnfstrdup(pMode->name);
    }

    return pNew;
}

/* This function will sort all modes according to their resolution.
 * Highest resolution first.
 */
static void
I830xf86SortModes(DisplayModePtr new, DisplayModePtr *first,
	      DisplayModePtr *last)
{
    DisplayModePtr  p;

    p = *last;
    while (p) {
	if (((new->HDisplay < p->HDisplay) &&
	     (new->VDisplay < p->VDisplay)) ||
	    ((new->HDisplay * new->VDisplay) < (p->HDisplay * p->VDisplay)) ||
	    ((new->HDisplay == p->HDisplay) &&
	     (new->VDisplay == p->VDisplay) &&
	     (new->Clock < p->Clock))) {

	    if (p->next) 
		p->next->prev = new;
	    new->prev = p;
	    new->next = p->next;
	    p->next = new;
	    if (!(new->next))
		*last = new;
	    break;
	}
	if (!p->prev) {
	    new->prev = NULL;
	    new->next = p;
	    p->prev = new;
	    *first = new;
	    break;
	}
	p = p->prev;
    }

    if (!*first) {
	*first = new;
	new->prev = NULL;
	new->next = NULL;
	*last = new;
    }
}

/**
 * Gets a new pointer to a VESA established mode.
 *
 * \param i index into the VESA established modes table.
 */
static DisplayModePtr
I830GetVESAEstablishedMode(ScrnInfoPtr pScrn, int i)
{
    DisplayModePtr pMode;

    for (pMode = I830xf86DefaultModes; pMode->name != NULL; pMode++)
    {
	if (pMode->HDisplay == est_timings[i].hsize &&
	    pMode->VDisplay == est_timings[i].vsize &&
	    fabs(i830xf86ModeVRefresh(pMode) - est_timings[i].refresh) < 1.0)
	{
	    DisplayModePtr pNew = I830DuplicateMode(pMode);
	    i830xf86SetModeDefaultName(pNew);
	    pNew->VRefresh = i830xf86ModeVRefresh(pMode);
	    return pNew;
	}
    }
    return NULL;
}

static DisplayModePtr
i830GetDDCModes(ScrnInfoPtr pScrn, xf86MonPtr ddc)
{
    DisplayModePtr  last  = NULL;
    DisplayModePtr  new   = NULL;
    DisplayModePtr  first = NULL;
    int             count = 0;
    int             j, tmp;

    if (ddc == NULL)
	return NULL;

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

	    new->HTotal     = new->HDisplay + d_timings->h_blanking;
	    new->HSyncStart = new->HDisplay + d_timings->h_sync_off;
	    new->HSyncEnd   = new->HSyncStart + d_timings->h_sync_width;
	    new->VTotal     = new->VDisplay + d_timings->v_blanking;
	    new->VSyncStart = new->VDisplay + d_timings->v_sync_off;
	    new->VSyncEnd   = new->VSyncStart + d_timings->v_sync_width;
	    new->Clock      = d_timings->clock / 1000;
	    new->Flags      = (d_timings->interlaced ? V_INTERLACE : 0);
	    new->status     = MODE_OK;
	    if (PREFERRED_TIMING_MODE(ddc->features.msc))
		new->type   = M_T_PREFERRED;
	    else
		new->type   = M_T_DRIVER;

	    i830xf86SetModeDefaultName(new);

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

	    I830xf86SortModes(new, &first, &last);
	}
    }

    /* Search thru standard VESA modes from EDID */
    for (j = 0; j < 8; j++) {
        if (ddc->timings2[j].hsize == 0 || ddc->timings2[j].vsize == 0)
               continue;
#if 1
	new = i830GetGTF(ddc->timings2[j].hsize, ddc->timings2[j].vsize,
			 ddc->timings2[j].refresh, FALSE, FALSE);
	new->status = MODE_OK;
	new->type |= M_T_DEFAULT;

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Valid Mode from standard timing table: %s\n",
		   new->name);

	I830xf86SortModes(new, &first, &last);
#else
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

		    I830xf86SortModes(new, &first, &last);
		    break;
		}
	    }
	}
#endif
    }

    /* Search thru established modes from EDID */
    tmp = (ddc->timings1.t1 << 8) | ddc->timings1.t2;
    for (j = 0; j < 16; j++) {
	if (tmp & (1 << j)) {
	    new = I830GetVESAEstablishedMode(pScrn, j);
	    if (new == NULL) {
		ErrorF("Couldn't get established mode %d\n", j);
		continue;
	    }
	    new->status = MODE_OK;
	    new->type = M_T_DEFAULT;

	    count++;

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Valid Mode from established "
		       "timing table: %s\n", new->name);

	    I830xf86SortModes(new, &first, &last);
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Total of %d DDC mode(s) found.\n", count);

    return first;
}

/**
 * This function returns a default mode for flat panels using the timing
 * information provided by the BIOS.
 */
static DisplayModePtr
i830FPNativeMode(ScrnInfoPtr pScrn)
{
   I830Ptr pI830 = I830PTR(pScrn);
   DisplayModePtr  new;
   char            stmp[32];

   if (pI830->PanelXRes == 0 || pI830->PanelYRes == 0)
      return NULL;

   /* Add native panel size */
   new             = xnfcalloc(1, sizeof (DisplayModeRec));
   sprintf(stmp, "%dx%d", pI830->PanelXRes, pI830->PanelYRes);
   new->name       = xnfalloc(strlen(stmp) + 1);
   strcpy(new->name, stmp);
   new->HDisplay   = pI830->PanelXRes;
   new->VDisplay   = pI830->PanelYRes;
   new->HSyncStart = pI830->panel_fixed_hactive + pI830->panel_fixed_hsyncoff;
   new->HSyncEnd   = new->HSyncStart + pI830->panel_fixed_hsyncwidth;
   new->HTotal     = new->HSyncEnd + 1;
   new->VSyncStart = pI830->panel_fixed_vactive + pI830->panel_fixed_vsyncoff;
   new->VSyncEnd   = new->VSyncStart + pI830->panel_fixed_vsyncwidth;
   new->VTotal     = new->VSyncEnd + 1;
   new->Clock      = pI830->panel_fixed_clock;

   new->type       = M_T_PREFERRED;

   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "No valid mode specified, force to native mode\n");

   return new;
}

/**
 * FP automatic modelist creation routine for using panel fitting.
 *
 * Constructs modes for any resolution less than the panel size specified by the
 * user, with the user flag set, plus standard VESA mode sizes without the user
 * flag set (for randr).
 *
 * Modes will be faked to use GTF parameters, even though at the time of being
 * programmed into the LVDS they'll end up being forced to the panel's fixed
 * mode.
 *
 * \return doubly-linked list of modes.
 */
static DisplayModePtr
i830GetLVDSModes(ScrnInfoPtr pScrn, char **ppModeName)
{
   I830Ptr pI830 = I830PTR(pScrn);
   DisplayModePtr  last       = NULL;
   DisplayModePtr  new        = NULL;
   DisplayModePtr  first      = NULL;
   DisplayModePtr  p, tmp;
   int             count      = 0;
   int             i, width, height;

   /* We have a flat panel connected to the primary display, and we
    * don't have any DDC info.
    */
   for (i = 0; ppModeName[i] != NULL; i++) {

      if (sscanf(ppModeName[i], "%dx%d", &width, &height) != 2)
	 continue;

      /* Note: We allow all non-standard modes as long as they do not
       * exceed the native resolution of the panel.  Since these modes
       * need the internal RMX unit in the video chips (and there is
       * only one per card), this will only apply to the primary head.
       */
      if (width < 320 || width > pI830->PanelXRes ||
	 height < 200 || height > pI830->PanelYRes) {
	 xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Mode %s is out of range.\n",
 		    ppModeName[i]);
	 xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		    "Valid modes must be between 320x200-%dx%d\n",
		    pI830->PanelXRes, pI830->PanelYRes);
	 continue;
      }

      new = i830GetGTF(width, height, 60.0, FALSE, FALSE);
      new->type |= M_T_DEFAULT;

      new->next       = NULL;
      new->prev       = last;

      if (last)
	 last->next = new;
      last = new;
      if (!first)
	 first = new;

      count++;
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		 "Valid mode using panel fitting: %s\n", new->name);
   }

   /* If the user hasn't specified modes, add the native mode */
   if (!count) {
      new = i830FPNativeMode(pScrn);
      if (new) {
	 I830xf86SortModes(new, &first, &last);
	 count = 1;
      }
   }

   /* add in all default vesa modes smaller than panel size, used for randr */
   for (p = pScrn->monitor->Modes; p && p->next; p = p->next->next) {
      if ((p->HDisplay <= pI830->PanelXRes) && (p->VDisplay <= pI830->PanelYRes)) {
	 tmp = first;
	 while (tmp) {
	    if ((p->HDisplay == tmp->HDisplay) && (p->VDisplay == tmp->VDisplay)) break;
	       tmp = tmp->next;
	 }
	 if (!tmp) {
	    new = i830GetGTF(p->HDisplay, p->VDisplay, 60.0, FALSE, FALSE);
	    new->type |= M_T_DEFAULT;

	    I830xf86SortModes(new, &first, &last);

	    count++;
	 }
      }
   }

   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "Total number of valid FP mode(s) found: %d\n", count);

   return first;
}

static DisplayModePtr
i830GetModeListTail(DisplayModePtr pModeList)
{
    DisplayModePtr last;

    if (pModeList == NULL)
	return NULL;

    for (last = pModeList; last->next != NULL; last = last->next)
	;

    return last;
}

/**
 * Appends a list of modes to another mode list, without duplication.
 */
static void
i830AppendModes(ScrnInfoPtr pScrn, DisplayModePtr *modeList,
		DisplayModePtr addModes)
{
    DisplayModePtr first = *modeList;
    DisplayModePtr last = i830GetModeListTail(first);

    if (addModes == NULL)
      return;

    if (first == NULL) {
	*modeList = addModes;
    } else {
	last->next = addModes;
	addModes->prev = last;
    }
}

/**
 * Duplicates every mode in the given list and returns a pointer to the first
 * mode.
 *
 * \param modeList doubly-linked mode list
 */
static DisplayModePtr
i830DuplicateModes(ScrnInfoPtr pScrn, DisplayModePtr modeList)
{
    DisplayModePtr first = NULL, last = NULL;
    DisplayModePtr mode;

    for (mode = modeList; mode != NULL; mode = mode->next) {
	DisplayModePtr new;

	new = I830DuplicateMode(mode);

	/* Insert pNew into modeList */
	if (last) {
	    last->next = new;
	    new->prev = last;
	} else {
	    first = new;
	    new->prev = NULL;
	}
	new->next = NULL;
	last = new;
    }

    return first;
}

static MonPtr
i830GetDDCMonitor(ScrnInfoPtr pScrn, I2CBusPtr pDDCBus)
{
    xf86MonPtr ddc;
    MonPtr mon;
    DisplayModePtr userModes;
    int i;

    ddc = xf86DoEDID_DDC2(pScrn->scrnIndex, pDDCBus);

    if (ddc == NULL)
	return NULL;

    mon = xnfcalloc(1, sizeof(*mon));
    mon->Modes = i830GetDDCModes(pScrn, ddc);
    mon->DDC = ddc;

    for (i = 0; i < DET_TIMINGS; i++) {
	struct detailed_monitor_section *det_mon = &ddc->det_mon[i];

	switch (ddc->det_mon[i].type) {
	case DS_RANGES:
	    mon->hsync[mon->nHsync].lo = det_mon->section.ranges.min_h;
	    mon->hsync[mon->nHsync].hi = det_mon->section.ranges.max_h;
	    mon->nHsync++;
	    mon->vrefresh[mon->nVrefresh].lo = det_mon->section.ranges.min_v;
	    mon->vrefresh[mon->nVrefresh].hi = det_mon->section.ranges.max_v;
	    mon->nVrefresh++;
	    break;
	default:
	    /* We probably don't care about trying to contruct ranges around
	     * modes specified by DDC.
	     */
	    break;
	}
    }

    /* Add in VESA standard and user modelines, and do additional validation
     * on them beyond what pipe config will do (x/y/pitch, clocks, flags)
     */
    userModes = i830DuplicateModes(pScrn, pScrn->monitor->Modes);

    i830xf86ValidateModesSync(pScrn, userModes, mon);
    i830xf86PruneInvalidModes(pScrn, &userModes, TRUE);

    i830AppendModes(pScrn, &mon->Modes, userModes);

    mon->Last = i830GetModeListTail(mon->Modes);

    return mon;
}

static MonPtr
i830GetLVDSMonitor(ScrnInfoPtr pScrn)
{
    MonPtr mon;

    mon = xnfcalloc(1, sizeof(*mon));
    mon->Modes = i830GetLVDSModes(pScrn, pScrn->display->modes);
    mon->Last = i830GetModeListTail(mon->Modes);

    return mon;
}

static MonPtr
i830GetConfiguredMonitor(ScrnInfoPtr pScrn)
{
    MonPtr mon;

    mon = xnfcalloc(1, sizeof(*mon));
    memcpy(mon, pScrn->monitor, sizeof(*mon));

    if (pScrn->monitor->id != NULL)
	mon->id = xnfstrdup(pScrn->monitor->id);
    if (pScrn->monitor->vendor != NULL)
	mon->vendor = xnfstrdup(pScrn->monitor->vendor);
    if (pScrn->monitor->model != NULL)
	mon->model = xnfstrdup(pScrn->monitor->model);

    /* Use VESA standard and user modelines, and do additional validation
     * on them beyond what pipe config will do (x/y/pitch, clocks, flags)
     */
    mon->Modes = i830DuplicateModes(pScrn, pScrn->monitor->Modes);
    i830xf86ValidateModesSync(pScrn, mon->Modes, mon);
    i830xf86PruneInvalidModes(pScrn, &mon->Modes, TRUE);
    mon->Last = i830GetModeListTail(mon->Modes);

    return mon;
}

static MonPtr
i830GetDefaultMonitor(ScrnInfoPtr pScrn)
{
    MonPtr mon;

    mon = xnfcalloc(1, sizeof(*mon));

    mon->id = xnfstrdup("Unknown Id");
    mon->vendor = xnfstrdup("Unknown Vendor");
    mon->model = xnfstrdup("Unknown Model");

    mon->nHsync = 1;
    mon->hsync[0].lo = 31.0;
    mon->hsync[0].hi = 100.0;
    mon->nVrefresh = 1;
    mon->vrefresh[0].lo = 50.0;
    mon->vrefresh[0].hi = 70.0;
    mon->widthmm = 400;
    mon->heightmm = 300;
    /* Use VESA standard and user modelines, and do additional validation
     * on them beyond what pipe config will do (x/y/pitch, clocks, flags)
     */
    mon->Modes = i830DuplicateModes(pScrn, pScrn->monitor->Modes);
    i830xf86ValidateModesSync(pScrn, mon->Modes, mon);
    i830xf86PruneInvalidModes(pScrn, &mon->Modes, TRUE);
    mon->Last = i830GetModeListTail(mon->Modes);

    return mon;
}

static void
i830FreeMonitor(ScrnInfoPtr pScrn, MonPtr mon)
{
    while (mon->Modes != NULL)
	xf86DeleteMode(&mon->Modes, mon->Modes);
    xfree(mon->id);
    xfree(mon->vendor);
    xfree(mon->model);
    xfree(mon->DDC);
    xfree(mon);
}

/**
 * Performs probing of modes available on the output connected to the given
 * pipe.
 *
 * We do not support multiple outputs per pipe (since the cases for that are
 * sufficiently rare we can't imagine the complexity being worth it), so
 * the pipe is a sufficient specifier.
 */
static void
I830ReprobePipeModeList(ScrnInfoPtr pScrn, int pipe)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int output_index = -1;
    int i;
    int outputs;
    DisplayModePtr pMode;
    MonPtr old_mon = pI830->pipeMon[pipe];

    if (pipe == 0)
	outputs = pI830->operatingDevices & 0xff;
    else
	outputs = (pI830->operatingDevices >> 8) & 0xff;

    for (i = 0; i < MAX_OUTPUTS; i++) {
	switch (pI830->output[i].type) {
	case I830_OUTPUT_ANALOG:
	    if (outputs & PIPE_CRT) {
		output_index = i;
	    }
	    break;
	case I830_OUTPUT_LVDS:
	    if (outputs & PIPE_LFP) {
		output_index = i;
	    }
	    break;
	case I830_OUTPUT_DVO:
	    if (outputs & PIPE_DFP && pI830->output[i].i2c_drv != NULL) {
		output_index = i;
	    }
	    break;
	case I830_OUTPUT_SDVO:
	    if (outputs & PIPE_DFP &&
		pI830->output[i].sdvo_drv != NULL)
	    {
		output_index = i;
	    }
	    break;
	}
    }
    /* XXX: If there's no output associated with the pipe, bail for now. */
    if (output_index == -1)
	return;

    if (outputs & PIPE_LFP) {
	pI830->pipeMon[pipe] = i830GetLVDSMonitor(pScrn);
    } else if (pI830->output[output_index].pDDCBus != NULL) {
	pI830->pipeMon[pipe] =
	    i830GetDDCMonitor(pScrn, pI830->output[output_index].pDDCBus);
    }
    /* If DDC didn't work (or the flat panel equivalent), then see if we can
     * detect if a monitor is at least plugged in.  If we can't tell that one
     * is plugged in, then assume that it is.
     */
    if (pI830->pipeMon[pipe] == NULL) {
	switch (pI830->output[output_index].type) {
	case I830_OUTPUT_SDVO:
	    if (I830DetectSDVODisplays(pScrn, output_index))
		pI830->pipeMon[pipe] = i830GetConfiguredMonitor(pScrn);
	    break;
	case I830_OUTPUT_ANALOG:
	    /* Do a disruptive detect if necessary, since we want to be sure we
	     * know if a monitor is attached, and this detect process should be
	     * infrequent.
	     */
	    if (i830DetectCRT(pScrn, TRUE)) {
/*		if (pipe == pI830->pipe)
		    pI830->pipeMon[pipe] = i830GetConfiguredMonitor(pScrn);
		else */
		    pI830->pipeMon[pipe] = i830GetDefaultMonitor(pScrn);
	    }
	    break;
	default:
	    pI830->pipeMon[pipe] = i830GetConfiguredMonitor(pScrn);
	    break;
	}
    }

#ifdef DEBUG_REPROBE
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Printing probed modes for pipe %d\n",
	       pipe);
#endif
    if (pI830->pipeMon[pipe] != NULL) {
	int minclock, maxclock;

	switch (pI830->output[output_index].type) {
	case I830_OUTPUT_SDVO:
	    minclock = 25000;
	    maxclock = 165000;
	case I830_OUTPUT_LVDS:
	case I830_OUTPUT_ANALOG:
	default:
	    minclock = 25000;
	    maxclock = 400000;
	}

	i830xf86ValidateModesFlags(pScrn, pI830->pipeMon[pipe]->Modes,
				   V_INTERLACE);
	i830xf86ValidateModesClocks(pScrn, pI830->pipeMon[pipe]->Modes,
				    &minclock, &maxclock, 1);

	i830xf86PruneInvalidModes(pScrn, &pI830->pipeMon[pipe]->Modes, TRUE);

	/* silently prune modes down to ones matching the user's configuration.
	 */
	i830xf86ValidateModesUserConfig(pScrn, pI830->pipeMon[pipe]->Modes);
	i830xf86PruneInvalidModes(pScrn, &pI830->pipeMon[pipe]->Modes, FALSE);

	for (pMode = pI830->pipeMon[pipe]->Modes; pMode != NULL;
	     pMode = pMode->next)
	{
	    /* The code to choose the best mode per pipe later on will require
	     * VRefresh to be set.
	     */
	    pMode->VRefresh = i830xf86ModeVRefresh(pMode);
	    I830xf86SetModeCrtc(pMode, INTERLACE_HALVE_V);
#ifdef DEBUG_REPROBE
	    PrintModeline(pScrn->scrnIndex, pMode);
#endif
	}
    }

    if (old_mon != NULL && pI830->pipeMon[pipe] == NULL) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Failed to probe output on pipe %d, disabling output at next "
		   "mode switch\n", pipe);
	if (pipe == 0)
	    pI830->operatingDevices &= ~0x00ff;
	else
	    pI830->operatingDevices &= ~0xff00;
    }

    if (old_mon != NULL)
	i830FreeMonitor(pScrn, old_mon);
}

/**
 * This function removes a mode from a list of modes.  It should probably be
 * moved to xf86Mode.c.
 *
 * There are different types of mode lists:
 *
 *  - singly linked linear lists, ending in NULL
 *  - doubly linked linear lists, starting and ending in NULL
 *  - doubly linked circular lists
 *
 */

static void
I830xf86DeleteModeFromList(DisplayModePtr *modeList, DisplayModePtr mode)
{
    /* Catch the easy/insane cases */
    if (modeList == NULL || *modeList == NULL || mode == NULL)
	return;

    /* If the mode is at the start of the list, move the start of the list */
    if (*modeList == mode)
	*modeList = mode->next;

    /* If mode is the only one on the list, set the list to NULL */
    if ((mode == mode->prev) && (mode == mode->next)) {
	*modeList = NULL;
    } else {
	if ((mode->prev != NULL) && (mode->prev->next == mode))
	    mode->prev->next = mode->next;
	if ((mode->next != NULL) && (mode->next->prev == mode))
	    mode->next->prev = mode->prev;
    }
}
    
/**
 * Probes for video modes on attached otuputs, and assembles a list to insert
 * into pScrn.
 *
 * \param first_time indicates that the memory layout has already been set up,
 * 	  so displayWidth, virtualX, and virtualY shouldn't be touched.
 *
 * A SetMode must follow this call in order for operatingDevices to match the
 * hardware's state, in case we detect a new output device.  
 */
int
I830ValidateXF86ModeList(ScrnInfoPtr pScrn, Bool first_time)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int pipe;
    DisplayModePtr saved_mode, last;
    Bool pipes_reconfigured = FALSE;
    int originalVirtualX, originalVirtualY;

    for (pipe = 0; pipe < pI830->availablePipes; pipe++) {
	I830ReprobePipeModeList(pScrn, pipe);
    }

    /* If we've got a spare pipe, try to detect if a new CRT has been plugged
     * in.
     */
    if ((pI830->operatingDevices & (PIPE_CRT | (PIPE_CRT << 8))) == 0) {
	if ((pI830->operatingDevices & 0xff) == PIPE_NONE) {
	    pI830->operatingDevices |= PIPE_CRT;
	    I830ReprobePipeModeList(pScrn, 0);
	    if (pI830->pipeMon[0] == NULL) {
		/* No new output found. */
		pI830->operatingDevices &= ~PIPE_CRT;
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Enabled new CRT on pipe A\n");
		pipes_reconfigured = TRUE;
		/* Clear the current mode, so we reprogram the pipe for sure. */
		memset(&pI830->pipeCurMode[0], 0, sizeof(pI830->pipeCurMode[0]));
	    }
	} else if (((pI830->operatingDevices >> 8) & 0xff) == PIPE_NONE) {
	    pI830->operatingDevices |= PIPE_CRT << 8;
	    I830ReprobePipeModeList(pScrn, 1);
	    if (pI830->pipeMon[1] == NULL) {
		/* No new output found. */
		pI830->operatingDevices &= ~(PIPE_CRT << 8);
	    } else {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Enabled new CRT on pipe B\n");
		pipes_reconfigured = TRUE;
		/* Clear the current mode, so we reprogram the pipe for sure. */
		memset(&pI830->pipeCurMode[1], 0, sizeof(pI830->pipeCurMode[1]));
	    }
	}
    }

    if ((pI830->pipeMon[0] == NULL || pI830->pipeMon[0]->Modes == NULL) &&
	(pI830->pipeMon[1] == NULL || pI830->pipeMon[1]->Modes == NULL))
    {
	FatalError("No modes found on either pipe\n");
    }

    if (first_time) {
	int maxX = -1, maxY = -1;

	/* Set up a virtual size that will cover any clone mode we'd want to set
	 * for either of the two pipes.
	 */
	for (pipe = 0; pipe < pI830->availablePipes; pipe++) {
	    MonPtr mon = pI830->pipeMon[pipe];
	    DisplayModePtr mode;

	    if (mon == NULL)
		continue;

	    for (mode = mon->Modes; mode != NULL; mode = mode->next) {
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

    I830GetOriginalVirtualSize(pScrn, &originalVirtualX, &originalVirtualY);

    /* Disable modes that are larger than the virtual size we decided on
     * initially.
     */
    if (!first_time) {
	for (pipe = 0; pipe < pI830->availablePipes; pipe++) {
	    MonPtr mon = pI830->pipeMon[pipe];
	    DisplayModePtr mode;

	    if (mon == NULL)
		continue;

	    for (mode = mon->Modes; mode != NULL; mode = mode->next)
	    {
		if (mode->HDisplay > originalVirtualX)
		    mode->status = MODE_VIRTUAL_X;
		if (mode->VDisplay > originalVirtualY)
		    mode->status = MODE_VIRTUAL_Y;
	    }
	}
    }

    /* Remove the current mode from the modelist if we're re-validating, so we
     * can find a new mode to map ourselves to afterwards.
     */
    saved_mode = pI830->currentMode;
    if (saved_mode != NULL) {
	I830xf86DeleteModeFromList(&pScrn->modes, saved_mode);
    }

    /* Clear any existing modes from pScrn->modes */
    while (pScrn->modes != NULL)
	xf86DeleteMode(&pScrn->modes, pScrn->modes);

    /* Set pScrn->modes to the mode list for the an arbitrary head.
     * pScrn->modes should only be used for XF86VidMode now, which we don't
     * care about enough to make some sort of unioned list.
     */
    if (pI830->pipeMon[1] != NULL) {
	pScrn->modes = i830DuplicateModes(pScrn, pI830->pipeMon[1]->Modes);
    } else {
	pScrn->modes = i830DuplicateModes(pScrn, pI830->pipeMon[0]->Modes);
    }
    if (pScrn->modes == NULL) {
	FatalError("No modes found\n");
    }

    /* Don't let pScrn->modes have modes larger than the max root window size.
     * We don't really care about the monitors having it, particularly since we
     * eventually want randr to be able to move to those sizes.
     */
    i830xf86ValidateModesSize(pScrn, pScrn->modes,
			      originalVirtualX, originalVirtualY,
			      pScrn->displayWidth);

    /* Strip out anything bad that we threw out for virtualX. */
    i830xf86PruneInvalidModes(pScrn, &pScrn->modes, TRUE);

    /* For some reason, pScrn->modes is circular, unlike the other mode lists.
     * How great is that?
     */
    last = i830GetModeListTail(pScrn->modes);
    last->next = pScrn->modes;
    pScrn->modes->prev = last;

#if DEBUG_REPROBE
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Modes post revalidate\n");
    do {
	DisplayModePtr pMode;

	for (pMode = pScrn->modes; ; pMode = pMode->next) {
	    PrintModeline(pScrn->scrnIndex, pMode);
	    if (pMode->next == pScrn->modes)
		break;
	}
    } while (0);
#endif

    /* Save a pointer to the previous current mode.  We can't reset
     * pScrn->currentmode, because we rely on xf86SwitchMode's shortcut not
     * happening so we can hot-enable devices at SwitchMode.  We'll notice this
     * case at SwitchMode and free the saved mode.
     */
    pI830->savedCurrentMode = saved_mode;

    return 1; /* XXX */
}
