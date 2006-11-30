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
#include "X11/Xatom.h"
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
	    DisplayModePtr pNew = i830xf86DuplicateMode(pMode);
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

void
i830_reprobe_output_modes(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    Bool properties_set = FALSE;
    int i;

    /* Re-probe the list of modes for each output. */
    for (i = 0; i < pI830->xf86_config.num_output; i++) 
    {
	xf86OutputPtr  output = pI830->xf86_config.output[i];
	DisplayModePtr mode;

	while (output->probed_modes != NULL)
	    xf86DeleteMode(&output->probed_modes, output->probed_modes);

	output->probed_modes = (*output->funcs->get_modes) (output);

	/* Set the DDC properties to whatever first output has DDC information.
	 */
	if (output->MonInfo != NULL && !properties_set) {
	    xf86SetDDCproperties(pScrn, output->MonInfo);
	    properties_set = TRUE;
	}

	if (output->probed_modes != NULL) 
	{
	    /* silently prune modes down to ones matching the user's
	     * configuration.
	     */
	    i830xf86ValidateModesUserConfig(pScrn, output->probed_modes);
	    i830xf86PruneInvalidModes(pScrn, &output->probed_modes, FALSE);
	}

#ifdef DEBUG_REPROBE
	if (output->probed_modes != NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Printing probed modes for output %s\n",
		       output->name);
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "No remaining probed modes for output %s\n",
		       output->name);
	}
#endif
	for (mode = output->probed_modes; mode != NULL; mode = mode->next)
	{
	    /* The code to choose the best mode per pipe later on will require
	     * VRefresh to be set.
	     */
	    mode->VRefresh = i830xf86ModeVRefresh(mode);
	    I830xf86SetModeCrtc(mode, INTERLACE_HALVE_V);

#ifdef DEBUG_REPROBE
	    PrintModeline(pScrn->scrnIndex, mode);
#endif
	}
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
i830_set_xf86_modes_from_outputs(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    DisplayModePtr saved_mode, last;
    int originalVirtualX, originalVirtualY;
    int i;

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

    /* Set pScrn->modes to the mode list for an arbitrary output.
     * pScrn->modes should only be used for XF86VidMode now, which we don't
     * care about enough to make some sort of unioned list.
     */
    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr output = pI830->xf86_config.output[i];
	if (output->probed_modes != NULL) {
	    pScrn->modes = i830xf86DuplicateModes(pScrn, output->probed_modes);
	    break;
	}
    }

    xf86GetOriginalVirtualSize(pScrn, &originalVirtualX, &originalVirtualY);

    /* Disable modes in the XFree86 DDX list that are larger than the current
     * virtual size.
     */
    i830xf86ValidateModesSize(pScrn, pScrn->modes,
			      originalVirtualX, originalVirtualY,
			      pScrn->displayWidth);

    /* Strip out anything that we threw out for virtualX/Y. */
    i830xf86PruneInvalidModes(pScrn, &pScrn->modes, TRUE);

    if (pScrn->modes == NULL) {
	FatalError("No modes left for XFree86 DDX\n");
    }

    /* For some reason, pScrn->modes is circular, unlike the other mode lists.
     * How great is that?
     */
    last = i830GetModeListTail(pScrn->modes);
    last->next = pScrn->modes;
    pScrn->modes->prev = last;

    /* Save a pointer to the previous current mode.  We can't reset
     * pScrn->currentmode, because we rely on xf86SwitchMode's shortcut not
     * happening so we can hot-enable devices at SwitchMode.  We'll notice this
     * case at SwitchMode and free the saved mode.
     */
    pI830->savedCurrentMode = saved_mode;
}

/**
 * Takes the output mode lists and decides the default root window size
 * and framebuffer pitch.
 */
void
i830_set_default_screen_size(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    int maxX = -1, maxY = -1;
    int i;

    /* Set up a virtual size that will cover any clone mode we'd want to
     * set for the currently-connected outputs.
     */
    for (i = 0; i < pI830->xf86_config.num_output; i++) {
	xf86OutputPtr  output = pI830->xf86_config.output[i];
	DisplayModePtr mode;

	for (mode = output->probed_modes; mode != NULL; mode = mode->next)
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
    i830_reprobe_output_modes(pScrn);

    if (first_time) {
	i830_set_default_screen_size(pScrn);
    }

    i830_set_xf86_modes_from_outputs(pScrn);

    return 1; /* XXX */
}

#ifdef RANDR_12_INTERFACE

#define EDID_ATOM_NAME		"EDID_DATA"

static void
i830_ddc_set_edid_property(xf86OutputPtr output, void *data, int data_len)
{
    Atom edid_atom = MakeAtom(EDID_ATOM_NAME, sizeof(EDID_ATOM_NAME), TRUE);

    /* This may get called before the RandR resources have been created */
    if (output->randr_output == NULL)
	return;

    if (data_len != 0) {
	RRChangeOutputProperty(output->randr_output, edid_atom, XA_INTEGER, 8,
			       PropModeReplace, data_len, data, FALSE);
    } else {
	RRDeleteOutputProperty(output->randr_output, edid_atom);
    }
}
#endif

/**
 * Generic get_modes function using DDC, used by many outputs.
 */
DisplayModePtr
i830_ddc_get_modes(xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    I830OutputPrivatePtr intel_output = output->driver_private;
    xf86MonPtr ddc_mon;
    DisplayModePtr ddc_modes, mode;
    int i;

    ddc_mon = xf86DoEDID_DDC2(pScrn->scrnIndex, intel_output->pDDCBus);
    if (ddc_mon == NULL) {
#ifdef RANDR_12_INTERFACE
	i830_ddc_set_edid_property(output, NULL, 0);
#endif
	return NULL;
    }

    if (output->MonInfo != NULL)
	xfree(output->MonInfo);
    output->MonInfo = ddc_mon;

#ifdef RANDR_12_INTERFACE
    if (output->MonInfo->ver.version == 1) {
	i830_ddc_set_edid_property(output, ddc_mon->rawData, 128);
    } else if (output->MonInfo->ver.version == 2) {
	i830_ddc_set_edid_property(output, ddc_mon->rawData, 256);
    } else {
	i830_ddc_set_edid_property(output, NULL, 0);
    }
#endif

    /* Debug info for now, at least */
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID for output %s\n", output->name);
    xf86PrintEDID(output->MonInfo);

    ddc_modes = i830GetDDCModes(pScrn, ddc_mon);

    /* Strip out any modes that can't be supported on this output. */
    for (mode = ddc_modes; mode != NULL; mode = mode->next) {
	int status = (*output->funcs->mode_valid)(output, mode);

	if (status != MODE_OK)
	    mode->status = status;
    }
    i830xf86PruneInvalidModes(pScrn, &ddc_modes, TRUE);

    /* Pull out a phyiscal size from a detailed timing if available. */
    for (i = 0; i < 4; i++) {
	if (ddc_mon->det_mon[i].type == DT &&
	    ddc_mon->det_mon[i].section.d_timings.h_size != 0 &&
	    ddc_mon->det_mon[i].section.d_timings.v_size != 0)
	{
	    output->mm_width = ddc_mon->det_mon[i].section.d_timings.h_size;
	    output->mm_height = ddc_mon->det_mon[i].section.d_timings.v_size;
	    break;
	}
    }

    return ddc_modes;
}
