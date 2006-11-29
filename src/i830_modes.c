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
#include "xf86DDC.h"
#include "X11/Xatom.h"
#include "i830.h"
#include "i830_display.h"
#include "i830_xf86Modes.h"
#include <randrstr.h>

#define DEBUG_REPROBE 1

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
    for (i = 0; i < pI830->num_outputs; i++) {
	DisplayModePtr mode;

	while (pI830->output[i].probed_modes != NULL) {
	    xf86DeleteMode(&pI830->output[i].probed_modes,
			   pI830->output[i].probed_modes);
	}

	pI830->output[i].probed_modes =
	    pI830->output[i].get_modes(pScrn, &pI830->output[i]);

	/* Set the DDC properties to whatever first output has DDC information.
	 */
	if (pI830->output[i].MonInfo != NULL && !properties_set) {
	    xf86SetDDCproperties(pScrn, pI830->output[i].MonInfo);
	    properties_set = TRUE;
	}

	if (pI830->output[i].probed_modes != NULL) {
	    /* silently prune modes down to ones matching the user's
	     * configuration.
	     */
	    i830xf86ValidateModesUserConfig(pScrn,
					    pI830->output[i].probed_modes);
	    i830xf86PruneInvalidModes(pScrn, &pI830->output[i].probed_modes,
				      FALSE);
	}

#ifdef DEBUG_REPROBE
	if (pI830->output[i].probed_modes != NULL) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Printing probed modes for output %s\n",
		       i830_output_type_names[pI830->output[i].type]);
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "No remaining probed modes for output %s\n",
		       i830_output_type_names[pI830->output[i].type]);
	}
#endif
	for (mode = pI830->output[i].probed_modes; mode != NULL;
	     mode = mode->next)
	{
	    /* The code to choose the best mode per pipe later on will require
	     * VRefresh to be set.
	     */
	    mode->VRefresh = xf86ModeVRefresh(mode);
	    xf86SetModeCrtc(mode, INTERLACE_HALVE_V);

#ifdef DEBUG_REPROBE
	    xf86PrintModeline(pScrn->scrnIndex, mode);
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
    for (i = 0; i < pI830->num_outputs; i++) {
	if (pI830->output[i].probed_modes != NULL) {
	    pScrn->modes =
		xf86DuplicateModes(pScrn, pI830->output[i].probed_modes);
	    break;
	}
    }

    I830GetOriginalVirtualSize(pScrn, &originalVirtualX, &originalVirtualY);

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
    for (i = 0; i < pI830->num_outputs; i++) {
	DisplayModePtr mode;

	for (mode = pI830->output[i].probed_modes; mode != NULL;
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
i830_ddc_set_edid_property(ScrnInfoPtr pScrn, I830OutputPtr output,
			   void *data, int data_len)
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
i830_ddc_get_modes(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    xf86MonPtr ddc_mon;
    DisplayModePtr ddc_modes, mode;
    int i;

    ddc_mon = xf86DoEDID_DDC2(pScrn->scrnIndex, output->pDDCBus);
    if (ddc_mon == NULL) {
#ifdef RANDR_12_INTERFACE
	i830_ddc_set_edid_property(pScrn, output, NULL, 0);
#endif
	return NULL;
    }

    if (output->MonInfo != NULL)
	xfree(output->MonInfo);
    output->MonInfo = ddc_mon;

#ifdef RANDR_12_INTERFACE
    if (output->MonInfo->ver.version == 1) {
	i830_ddc_set_edid_property(pScrn, output, ddc_mon->rawData, 128);
    } else if (output->MonInfo->ver.version == 2) {
	i830_ddc_set_edid_property(pScrn, output, ddc_mon->rawData, 256);
    } else {
	i830_ddc_set_edid_property(pScrn, output, NULL, 0);
    }
#endif

    /* Debug info for now, at least */
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID for output %s\n",
	       i830_output_type_names[output->type]);
    xf86PrintEDID(output->MonInfo);

    ddc_modes = xf86DDCGetModes(pScrn->scrnIndex, ddc_mon);

    /* Strip out any modes that can't be supported on this output. */
    for (mode = ddc_modes; mode != NULL; mode = mode->next) {
	int status = output->mode_valid(pScrn, output, mode);

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
