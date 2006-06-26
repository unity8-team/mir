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

#include "xf86.h"
#include "i830.h"
#include "i830_xf86Modes.h"

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
    pNew->name = xnfstrdup(pMode->name);

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
	    fabs(I830ModeVRefresh(pMode) - est_timings[i].refresh) < 1.0)
	{
	    DisplayModePtr pNew = I830DuplicateMode(pMode);
	    pNew->VRefresh = I830ModeVRefresh(pMode);
	    return pNew;
	}
    }
    return NULL;
}

static DisplayModePtr
I830GetDDCModes(ScrnInfoPtr pScrn, xf86MonPtr ddc)
{
    DisplayModePtr  last  = NULL;
    DisplayModePtr  new   = NULL;
    DisplayModePtr  first = NULL;
    int             count = 0;
    int             j, tmp;
    char            stmp[32];

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

   new->type       = M_T_USERDEF;

   pScrn->virtualX = MAX(pScrn->virtualX, pI830->PanelXRes);
   pScrn->virtualY = MAX(pScrn->virtualY, pI830->PanelYRes);
   pScrn->display->virtualX = pScrn->virtualX;
   pScrn->display->virtualY = pScrn->virtualY;

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

   pScrn->virtualX = pScrn->display->virtualX;
   pScrn->virtualY = pScrn->display->virtualY;

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
      new->type |= M_T_USERDEF;

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
      first = last = i830FPNativeMode(pScrn);
      if (first)
	 count = 1;
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
	    if (ppModeName[i] == NULL)
		new->type |= M_T_USERDEF;
	    else
		new->type |= M_T_DEFAULT;

	    new->next       = NULL;
	    new->prev       = last;

	    if (last)
	       last->next = new;
	    last = new;
	    if (!first)
	       first = new;

	    count++;
	 }
      }
   }

   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	      "Total number of valid FP mode(s) found: %d\n", count);

   return first;
}

/**
 * Injects a list of probed modes into another mode list.
 *
 * Take the doubly-linked list of modes we've probed for the device, and injects
 * it into the doubly-linked modeList.  We don't need to filter, because the
 * eventual call to xf86ValidateModes will do this for us.  I think.
 */
static int
I830InjectProbedModes(ScrnInfoPtr pScrn, DisplayModePtr *modeList,
		      DisplayModePtr addModes)
{
    DisplayModePtr  last = *modeList;
    DisplayModePtr  first = *modeList;
    DisplayModePtr  addMode;
    int count = 0;

    for (addMode = addModes; addMode != NULL; addMode = addMode->next) {
	DisplayModePtr pNew;

	/* XXX: Do we need to check if modeList already contains the same mode?
	 */

	pNew = I830DuplicateMode(addMode);
#if 0
	/* If the user didn't specify any modes, mark all modes as M_T_USERDEF
	 * so that we can cycle through them, etc.  XXX: really need to?
	 */
	if (pScrn->display->modes[0] == NULL) {
	    pNew->type |= M_T_USERDEF;
	}
#endif

	/* Insert pNew into modeList */
	if (last) {
	    last->next = pNew;
	    pNew->prev = last;
	} else {
	    first = pNew;
	    pNew->prev = NULL;
	}
	pNew->next = NULL;
	last = pNew;

	count++;
    }
    *modeList = first;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Injected %d modes detected from the monitor\n", count);

    return count;
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
    Bool had_modes;

    had_modes = (pI830->pipeModes[pipe] != NULL);
    while (pI830->pipeModes[pipe] != NULL)
	xf86DeleteMode(&pI830->pipeModes[pipe], pI830->pipeModes[pipe]);

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
	case I830_OUTPUT_SDVO:
	    if (outputs & PIPE_DFP) {
		output_index = i;
	    }
	    break;
	}
    }
    /* XXX: If there's no output associated with the pipe, bail for now. */
    if (output_index == -1)
	return;

    if (outputs & PIPE_LFP) {
	pI830->pipeMon[pipe] = NULL; /* XXX */
	pI830->pipeModes[pipe] = i830GetLVDSModes(pScrn,
						  pScrn->display->modes);
    } else if (pI830->output[output_index].pDDCBus != NULL) {
	/* XXX: Free the mon */
	pI830->pipeMon[pipe] = xf86DoEDID_DDC2(pScrn->scrnIndex,
					       pI830->output[output_index].pDDCBus);
	pI830->pipeModes[pipe] = I830GetDDCModes(pScrn,
						 pI830->pipeMon[pipe]);

	for (pMode = pI830->pipeModes[pipe]; pMode != NULL; pMode = pMode->next)
	{
	    I830xf86SetModeCrtc(pMode, INTERLACE_HALVE_V);
	}
	if (had_modes && pI830->pipeModes[pipe] == NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Failed to DDC pipe %d, disabling output\n", pipe);
	    if (pipe == 0)
		pI830->operatingDevices &= ~0x00ff;
	    else
		pI830->operatingDevices &= ~0xff00;
	}
    } else {
	ErrorF("don't know how to get modes for this device.\n");
    }

    /* Set the vertical refresh, which is used by the choose-best-mode-per-pipe
     * code later on.
     */
#ifdef DEBUG_REPROBE
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Printing probed modes for pipe %d\n",
	       pipe);
#endif
    for (pMode = pI830->pipeModes[pipe]; pMode != NULL; pMode = pMode->next) {
	pMode->VRefresh = I830ModeVRefresh(pMode);
#ifdef DEBUG_REPROBE
	PrintModeline(pScrn->scrnIndex, pMode);
#endif
    }
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
    ClockRangePtr clockRanges;
    int n, pipe;
    DisplayModePtr saved_mode, availModes = NULL;
    int saved_virtualX = 0, saved_virtualY = 0, saved_displayWidth = 0;
    Bool pipes_reconfigured = FALSE;

    for (pipe = 0; pipe < MAX_DISPLAY_PIPES; pipe++) {
	I830ReprobePipeModeList(pScrn, pipe);
    }

    /* If we've got a spare pipe, try to detect if a new CRT has been plugged
     * in.
     */
    if ((pI830->operatingDevices & (PIPE_CRT | (PIPE_CRT << 8))) == 0) {
	if ((pI830->operatingDevices & 0xff) == PIPE_NONE) {
	    pI830->operatingDevices |= PIPE_CRT;
	    I830ReprobePipeModeList(pScrn, 0);
	    if (pI830->pipeModes[0] == NULL) {
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
	    if (pI830->pipeModes[1] == NULL) {
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

    /* Start by injecting the XFree86 default modes and user-configured
     * modelines.  XXX: This actually isn't of use if we've got any DDC, as DDC
     * currently has higher priority than the validated modelist.  We need to
     * deal with that.
     */
    I830InjectProbedModes(pScrn, &availModes, pScrn->monitor->Modes);
    if (pI830->pipeModes[0] != NULL) {
	I830InjectProbedModes(pScrn, &availModes, pI830->pipeModes[0]);
    }
    if (pI830->pipeModes[1] != NULL) {
	I830InjectProbedModes(pScrn, &availModes, pI830->pipeModes[1]);
    }

   /*
     * Set up the ClockRanges, which describe what clock ranges are available,
     * and what sort of modes they can be used for.
     */
    clockRanges = xnfcalloc(sizeof(ClockRange), 1);
    clockRanges->next = NULL;
    clockRanges->minClock = 25000;
    clockRanges->maxClock = pI830->MaxClock;
    clockRanges->clockIndex = -1;		/* programmable */
    clockRanges->interlaceAllowed = TRUE;	/* XXX check this */
    clockRanges->doubleScanAllowed = FALSE;	/* XXX check this */

    /* Remove the current mode from the modelist if we're re-validating, so we
     * can find a new mode to map ourselves to afterwards.
     */
    saved_mode = pI830->currentMode;
    if (saved_mode != NULL) {
	I830xf86DeleteModeFromList(&pScrn->modes, saved_mode);
    }

    if (!first_time) {
	saved_virtualX = pScrn->virtualX;
	saved_virtualY = pScrn->virtualY;
	saved_displayWidth = pScrn->displayWidth;
    }

    /* Take the pScrn->monitor->Modes we've accumulated and validate them into
     * pScrn->modes.
     * XXX: Should set up a scrp->monitor->DDC covering the union of the
     *      capabilities of our pipes.
     */
    n = xf86ValidateModes(pScrn,
			  availModes, /* availModes */
			  pScrn->display->modes, /* modeNames */
			  clockRanges, /* clockRanges */
			  !first_time ? &pScrn->displayWidth : NULL, /* linePitches */
			  320, /* minPitch */
			  MAX_DISPLAY_PITCH, /* maxPitch */
			  64 * pScrn->bitsPerPixel, /* pitchInc */
			  200, /* minHeight */
			  MAX_DISPLAY_HEIGHT, /* maxHeight */
			  pScrn->virtualX, /* virtualX */
			  pScrn->virtualY, /* virtualY */
			  pI830->FbMapSize, /* apertureSize */
			  LOOKUP_BEST_REFRESH /* strategy */);

    /* availModes is of no more use as xf86ValidateModes has duplicated and
     * saved everything it needs.
     */
    while (availModes != NULL)
	xf86DeleteMode(&availModes, availModes);

    if (!first_time) {
	/* Restore things that may have been damaged by xf86ValidateModes. */
	pScrn->virtualX = saved_virtualX;
	pScrn->virtualY = saved_virtualY;
	pScrn->displayWidth = saved_displayWidth;
    }

    /* Need to do xf86CrtcForModes so any user-configured modes are valid for
     * non-LVDS.
     */
    xf86SetCrtcForModes(pScrn, INTERLACE_HALVE_V);

    xf86PruneDriverModes(pScrn);

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

    /* Try to find the closest equivalent of the previous mode pointer to switch
     * to.
     */
    if (saved_mode != NULL) {
	DisplayModePtr pBestMode = NULL, pMode;

	/* XXX: Is finding a matching x/y res enough?  probably not. */
	for (pMode = pScrn->modes; ; pMode = pMode->next) {
	    if (pMode->HDisplay == saved_mode->HDisplay &&
		pMode->VDisplay == saved_mode->VDisplay)
	    {
		ErrorF("found matching mode %p\n", pMode);
		pBestMode = pMode;
	    }
	    if (pMode->next == pScrn->modes)
		break;
	}

	if (pBestMode != NULL)
		xf86SwitchMode(pScrn->pScreen, pBestMode);
	else
		FatalError("No suitable modes after re-probe\n");

	xfree(saved_mode->name);
	xfree(saved_mode);
    }

    /* If we've enabled/disabled some pipes, we need to reset cloning mode
     * support.
     */
    if (pipes_reconfigured) {
	if ((pI830->operatingDevices & 0x00ff) &&
	    (pI830->operatingDevices & 0xff00))
	{
	    pI830->Clone = TRUE;
	} else {
	    pI830->Clone = FALSE;
	}

	/* If HW cursor currently showing, reset cursor state */
	if (pI830->CursorInfoRec && !pI830->SWCursor && pI830->cursorOn)
	    pI830->CursorInfoRec->ShowCursor(pScrn);
    }

    return n;
}
