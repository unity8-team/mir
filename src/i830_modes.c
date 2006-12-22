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

    ddc_modes = xf86DDCGetModes(pScrn->scrnIndex, ddc_mon);

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

    /* if no mm size is available from a detailed timing, check the max size field */
    if ((!output->mm_width || !output->mm_height) &&
	(ddc_mon->features.hsize && ddc_mon->features.vsize))
    {
	output->mm_width = ddc_mon->features.hsize * 10;
	output->mm_height = ddc_mon->features.vsize * 10;
    }

    return ddc_modes;
}
