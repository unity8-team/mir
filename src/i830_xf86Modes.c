/* -*- c-basic-offset: 4 -*- */
/* $XdotOrg: xserver/xorg/hw/xfree86/common/xf86Mode.c,v 1.10 2006/03/07 16:00:57 libv Exp $ */
/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86Mode.c,v 1.69 2003/10/08 14:58:28 dawes Exp $ */
/*
 * Copyright (c) 1997-2003 by The XFree86 Project, Inc.
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
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the copyright holder(s)
 * and author(s) shall not be used in advertising or otherwise to promote
 * the sale, use or other dealings in this Software without prior written
 * authorization from the copyright holder(s) and author(s).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "xf86.h"
#include "i830.h"
#include "i830_xf86Modes.h"

/**
 * @file this file contains symbols from xf86Mode.c and friends that are static
 * there but we still want to use.  We need to come up with better API here.
 */


/**
 * Calculates the horizontal sync rate of a mode.
 *
 * Exact copy of xf86Mode.c's.
 */
double
i830xf86ModeHSync(DisplayModePtr mode)
{
    double hsync = 0.0;
    
    if (mode->HSync > 0.0)
	    hsync = mode->HSync;
    else if (mode->HTotal > 0)
	    hsync = (float)mode->Clock / (float)mode->HTotal;

    return hsync;
}

/**
 * Calculates the vertical refresh rate of a mode.
 *
 * Exact copy of xf86Mode.c's.
 */
double
i830xf86ModeVRefresh(DisplayModePtr mode)
{
    double refresh = 0.0;

    if (mode->VRefresh > 0.0)
	refresh = mode->VRefresh;
    else if (mode->HTotal > 0 && mode->VTotal > 0) {
	refresh = mode->Clock * 1000.0 / mode->HTotal / mode->VTotal;
	if (mode->Flags & V_INTERLACE)
	    refresh *= 2.0;
	if (mode->Flags & V_DBLSCAN)
	    refresh /= 2.0;
	if (mode->VScan > 1)
	    refresh /= (float)(mode->VScan);
    }
    return refresh;
}

/**
 * Sets a default mode name of <width>x<height>x<refresh> on a mode.
 *
 * The refresh rate doesn't contain decimals, as that's expected to be
 * unimportant from the user's perspective for non-custom modelines.
 */
void
i830xf86SetModeDefaultName(DisplayModePtr mode)
{
    if (mode->name != NULL)
	xfree(mode->name);

    mode->name = XNFprintf("%dx%dx%.0f", mode->HDisplay, mode->VDisplay,
			   i830xf86ModeVRefresh(mode));
}

/*
 * I830xf86SetModeCrtc
 *
 * Initialises the Crtc parameters for a mode.  The initialisation includes
 * adjustments for interlaced and double scan modes.
 *
 * Exact copy of xf86Mode.c's.
 */
void
I830xf86SetModeCrtc(DisplayModePtr p, int adjustFlags)
{
    if ((p == NULL) || ((p->type & M_T_CRTC_C) == M_T_BUILTIN))
	return;

    p->CrtcHDisplay             = p->HDisplay;
    p->CrtcHSyncStart           = p->HSyncStart;
    p->CrtcHSyncEnd             = p->HSyncEnd;
    p->CrtcHTotal               = p->HTotal;
    p->CrtcHSkew                = p->HSkew;
    p->CrtcVDisplay             = p->VDisplay;
    p->CrtcVSyncStart           = p->VSyncStart;
    p->CrtcVSyncEnd             = p->VSyncEnd;
    p->CrtcVTotal               = p->VTotal;
    if (p->Flags & V_INTERLACE) {
	if (adjustFlags & INTERLACE_HALVE_V) {
	    p->CrtcVDisplay         /= 2;
	    p->CrtcVSyncStart       /= 2;
	    p->CrtcVSyncEnd         /= 2;
	    p->CrtcVTotal           /= 2;
	}
	/* Force interlaced modes to have an odd VTotal */
	/* maybe we should only do this when INTERLACE_HALVE_V is set? */
	p->CrtcVTotal |= 1;
    }

    if (p->Flags & V_DBLSCAN) {
        p->CrtcVDisplay         *= 2;
        p->CrtcVSyncStart       *= 2;
        p->CrtcVSyncEnd         *= 2;
        p->CrtcVTotal           *= 2;
    }
    if (p->VScan > 1) {
        p->CrtcVDisplay         *= p->VScan;
        p->CrtcVSyncStart       *= p->VScan;
        p->CrtcVSyncEnd         *= p->VScan;
        p->CrtcVTotal           *= p->VScan;
    }
    p->CrtcHAdjusted = FALSE;
    p->CrtcVAdjusted = FALSE;

    /*
     * XXX
     *
     * The following is taken from VGA, but applies to other cores as well.
     */
    p->CrtcVBlankStart = min(p->CrtcVSyncStart, p->CrtcVDisplay);
    p->CrtcVBlankEnd = max(p->CrtcVSyncEnd, p->CrtcVTotal);
    if ((p->CrtcVBlankEnd - p->CrtcVBlankStart) >= 127) {
        /* 
         * V Blanking size must be < 127.
         * Moving blank start forward is safer than moving blank end
         * back, since monitors clamp just AFTER the sync pulse (or in
         * the sync pulse), but never before.
         */
        p->CrtcVBlankStart = p->CrtcVBlankEnd - 127;
	/*
	 * If VBlankStart is now > VSyncStart move VBlankStart
	 * to VSyncStart using the maximum width that fits into
	 * VTotal.
	 */
	if (p->CrtcVBlankStart > p->CrtcVSyncStart) {
	    p->CrtcVBlankStart = p->CrtcVSyncStart;
	    p->CrtcVBlankEnd = min(p->CrtcHBlankStart + 127, p->CrtcVTotal);
	}
    }
    p->CrtcHBlankStart = min(p->CrtcHSyncStart, p->CrtcHDisplay);
    p->CrtcHBlankEnd = max(p->CrtcHSyncEnd, p->CrtcHTotal);

    if ((p->CrtcHBlankEnd - p->CrtcHBlankStart) >= 63 * 8) {
        /*
         * H Blanking size must be < 63*8. Same remark as above.
         */
        p->CrtcHBlankStart = p->CrtcHBlankEnd - 63 * 8;
	if (p->CrtcHBlankStart > p->CrtcHSyncStart) {
	    p->CrtcHBlankStart = p->CrtcHSyncStart;
	    p->CrtcHBlankEnd = min(p->CrtcHBlankStart + 63 * 8, p->CrtcHTotal);
	}
    }
}

/**
 * Returns true if the given modes should program to the same timings.
 *
 * This doesn't use Crtc values, as it might be used on ModeRecs without the
 * Crtc values set.  So, it's assumed that the other numbers are enough.
 *
 * This isn't in xf86Modes.c, but it might deserve to be there.
 */
Bool
I830ModesEqual(DisplayModePtr pMode1, DisplayModePtr pMode2)
{
     if (pMode1->Clock == pMode2->Clock &&
	 pMode1->HDisplay == pMode2->HDisplay &&
	 pMode1->HSyncStart == pMode2->HSyncStart &&
	 pMode1->HSyncEnd == pMode2->HSyncEnd &&
	 pMode1->HTotal == pMode2->HTotal &&
	 pMode1->HSkew == pMode2->HSkew &&
	 pMode1->VDisplay == pMode2->VDisplay &&
	 pMode1->VSyncStart == pMode2->VSyncStart &&
	 pMode1->VSyncEnd == pMode2->VSyncEnd &&
	 pMode1->VTotal == pMode2->VTotal &&
	 pMode1->VScan == pMode2->VScan &&
	 pMode1->Flags == pMode2->Flags)
     {
	return TRUE;
     } else {
	return FALSE;
     }
}

/* exact copy of xf86Mode.c */
static void
add(char **p, char *new)
{
    *p = xnfrealloc(*p, strlen(*p) + strlen(new) + 2);
    strcat(*p, " ");
    strcat(*p, new);
}

/**
 * Print out a modeline.
 *
 * Convenient VRefresh printing was added, though, compared to xf86Mode.c
 */
void
PrintModeline(int scrnIndex,DisplayModePtr mode)
{
    char tmp[256];
    char *flags = xnfcalloc(1, 1);

    if (mode->HSkew) { 
	snprintf(tmp, 256, "hskew %i", mode->HSkew); 
	add(&flags, tmp);
    }
    if (mode->VScan) { 
	snprintf(tmp, 256, "vscan %i", mode->VScan); 
	add(&flags, tmp);
    }
    if (mode->Flags & V_INTERLACE) add(&flags, "interlace");
    if (mode->Flags & V_CSYNC) add(&flags, "composite");
    if (mode->Flags & V_DBLSCAN) add(&flags, "doublescan");
    if (mode->Flags & V_BCAST) add(&flags, "bcast");
    if (mode->Flags & V_PHSYNC) add(&flags, "+hsync");
    if (mode->Flags & V_NHSYNC) add(&flags, "-hsync");
    if (mode->Flags & V_PVSYNC) add(&flags, "+vsync");
    if (mode->Flags & V_NVSYNC) add(&flags, "-vsync");
    if (mode->Flags & V_PCSYNC) add(&flags, "+csync");
    if (mode->Flags & V_NCSYNC) add(&flags, "-csync");
#if 0
    if (mode->Flags & V_CLKDIV2) add(&flags, "vclk/2");
#endif
    xf86DrvMsg(scrnIndex, X_ERROR,
		   "Modeline \"%s\"x%.01f  %6.2f  %i %i %i %i  %i %i %i %i%s "
		   "(%.01f kHz)\n",
		   mode->name, mode->VRefresh, mode->Clock/1000., mode->HDisplay,
		   mode->HSyncStart, mode->HSyncEnd, mode->HTotal,
		   mode->VDisplay, mode->VSyncStart, mode->VSyncEnd,
		   mode->VTotal, flags, i830xf86ModeHSync(mode));
    xfree(flags);
}

/**
 * Marks as bad any modes with unsupported flags.
 *
 * \param modeList doubly-linked or circular list of modes.
 * \param flags flags supported by the driver.
 *
 * \bug only V_INTERLACE and V_DBLSCAN are supported.  Is that enough?
 *
 * This is not in xf86Modes.c, but would be part of the proposed new API.
 */
void
i830xf86ValidateModesFlags(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			    int flags)
{
    DisplayModePtr mode;

    for (mode = modeList; mode != NULL; mode = mode->next) {
	if (mode->Flags & V_INTERLACE && !(flags & V_INTERLACE))
	    mode->status = MODE_NO_INTERLACE;
	if (mode->Flags & V_DBLSCAN && !(flags & V_DBLSCAN))
	    mode->status = MODE_NO_DBLESCAN;
    }
}

/**
 * Marks as bad any modes extending beyond the given max X, Y, or pitch.
 *
 * \param modeList doubly-linked or circular list of modes.
 *
 * This is not in xf86Modes.c, but would be part of the proposed new API.
 */
void
i830xf86ValidateModesSize(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			  int maxX, int maxY, int maxPitch)
{
    DisplayModePtr mode;

    for (mode = modeList; mode != NULL; mode = mode->next) {
	if (maxPitch > 0 && mode->HDisplay > maxPitch)
	    mode->status = MODE_BAD_WIDTH;

	if (maxX > 0 && mode->HDisplay > maxX)
	    mode->status = MODE_VIRTUAL_X;

	if (maxY > 0 && mode->VDisplay > maxY)
	    mode->status = MODE_VIRTUAL_Y;

	if (mode->next == modeList)
	    break;
    }
}

/**
 * Marks as bad any modes that aren't supported by the given monitor's
 * hsync and vrefresh ranges.
 *
 * \param modeList doubly-linked or circular list of modes.
 *
 * This is not in xf86Modes.c, but would be part of the proposed new API.
 */
void
i830xf86ValidateModesSync(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			  MonPtr mon)
{
    DisplayModePtr mode;

    for (mode = modeList; mode != NULL; mode = mode->next) {
	Bool bad;
	int i;

	bad = TRUE;
	for (i = 0; i < mon->nHsync; i++) {
	    if (i830xf86ModeHSync(mode) >= mon->hsync[i].lo &&
		i830xf86ModeHSync(mode) <= mon->hsync[i].hi)
	    {
		bad = FALSE;
	    }
	}
	if (bad)
	    mode->status = MODE_HSYNC;

	bad = TRUE;
	for (i = 0; i < mon->nVrefresh; i++) {
	    if (i830xf86ModeVRefresh(mode) >= mon->vrefresh[i].lo &&
		i830xf86ModeVRefresh(mode) <= mon->vrefresh[i].hi)
	    {
		bad = FALSE;
	    }
	}
	if (bad)
	    mode->status = MODE_VSYNC;

	if (mode->next == modeList)
	    break;
    }
}

/**
 * Marks as bad any modes extending beyond outside of the given clock ranges.
 *
 * \param modeList doubly-linked or circular list of modes.
 * \param min pointer to minimums of clock ranges
 * \param max pointer to maximums of clock ranges
 * \param n_ranges number of ranges.
 *
 * This is not in xf86Modes.c, but would be part of the proposed new API.
 */
void
i830xf86ValidateModesClocks(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			    int *min, int *max, int n_ranges)
{
    DisplayModePtr mode;
    int i;

    for (mode = modeList; mode != NULL; mode = mode->next) {
	Bool good = FALSE;
	for (i = 0; i < n_ranges; i++) {
	    if (mode->Clock >= min[i] && mode->Clock <= max[i]) {
		good = TRUE;
		break;
	    }
	}
	if (!good)
	    mode->status = MODE_CLOCK_RANGE;
    }
}

/**
 * If the user has specified a set of mode names to use, mark as bad any modes
 * not listed.
 *
 * The user mode names specified are prefixes to names of modes, so "1024x768"
 * will match modes named "1024x768", "1024x768x75", "1024x768-good", but
 * "1024x768x75" would only match "1024x768x75" from that list.
 *
 * MODE_BAD is used as the rejection flag, for lack of a better flag.
 *
 * \param modeList doubly-linked or circular list of modes.
 *
 * This is not in xf86Modes.c, but would be part of the proposed new API.
 */
void
i830xf86ValidateModesUserConfig(ScrnInfoPtr pScrn, DisplayModePtr modeList)
{
    DisplayModePtr mode;

    if (pScrn->display->modes[0] == NULL)
	return;

    for (mode = modeList; mode != NULL; mode = mode->next) {
	int i;
	Bool good = FALSE;

	for (i = 0; pScrn->display->modes[i] != NULL; i++) {
	    if (strncmp(pScrn->display->modes[i], mode->name,
			strlen(pScrn->display->modes[i])) == 0) {
		good = TRUE;
		break;
	    }
	}
	if (!good)
	    mode->status = MODE_BAD;
    }
}


/**
 * Frees any modes from the list with a status other than MODE_OK.
 *
 * \param modeList pointer to a doubly-linked or circular list of modes.
 * \param verbose determines whether the reason for mode invalidation is
 *	  printed.
 *
 * This is not in xf86Modes.c, but would be part of the proposed new API.
 */
void
i830xf86PruneInvalidModes(ScrnInfoPtr pScrn, DisplayModePtr *modeList,
			  Bool verbose)
{
    DisplayModePtr mode;

    for (mode = *modeList; mode != NULL;) {
	DisplayModePtr next = mode->next, first = *modeList;

	if (mode->status != MODE_OK) {
	    if (verbose) {
		char *type = "";
		if (mode->type & M_T_BUILTIN)
		    type = "built-in ";
		else if (mode->type & M_T_DEFAULT)
		    type = "default ";
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Not using %smode \"%s\" (%s)\n", type, mode->name,
			   xf86ModeStatusToString(mode->status));
	    }
	    xf86DeleteMode(modeList, mode);
	}

	if (next == first)
	    break;
	mode = next;
    }
}

#define MODEPREFIX(name) NULL, NULL, name, MODE_OK, M_T_DEFAULT
#define MODESUFFIX       0,0, 0,0,0,0,0,0,0, 0,0,0,0,0,0,FALSE,FALSE,0,NULL,0,0.0,0.0

/**
 * List of VESA established modes, taken from xf86DefaultModes but trimmed down.
 * (not trimming should be harmless).
 */
DisplayModeRec I830xf86DefaultModes[] = {
/* 640x350 @ 85Hz (VESA) hsync: 37.9kHz */
	{MODEPREFIX("640x350"),31500, 640,672,736,832,0, 350,382,385,445,0, V_PHSYNC | V_NVSYNC, MODESUFFIX},
	{MODEPREFIX("320x175"),15750, 320,336,368,416,0, 175,191,192,222,0, V_PHSYNC | V_NVSYNC | V_DBLSCAN, MODESUFFIX},
/* 640x400 @ 85Hz (VESA) hsync: 37.9kHz */
	{MODEPREFIX("640x400"),31500, 640,672,736,832,0, 400,401,404,445,0, V_NHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("320x200"),15750, 320,336,368,416,0, 200,200,202,222,0, V_NHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 720x400 @ 85Hz (VESA) hsync: 37.9kHz */
	{MODEPREFIX("720x400"),35500, 720,756,828,936,0, 400,401,404,446,0, V_NHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("360x200"),17750, 360,378,414,468,0, 200,200,202,223,0, V_NHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 640x480 @ 72Hz (VESA) hsync: 37.9kHz */
	{MODEPREFIX("640x480"),31500, 640,664,704,832,0, 480,489,491,520,0, V_NHSYNC | V_NVSYNC, MODESUFFIX},
	{MODEPREFIX("320x240"),15750, 320,332,352,416,0, 240,244,245,260,0, V_NHSYNC | V_NVSYNC | V_DBLSCAN, MODESUFFIX},
/* 640x480 @ 75Hz (VESA) hsync: 37.5kHz */
	{MODEPREFIX("640x480"),31500, 640,656,720,840,0, 480,481,484,500,0, V_NHSYNC | V_NVSYNC, MODESUFFIX},
	{MODEPREFIX("320x240"),15750, 320,328,360,420,0, 240,240,242,250,0, V_NHSYNC | V_NVSYNC | V_DBLSCAN, MODESUFFIX},
/* 640x480 @ 85Hz (VESA) hsync: 43.3kHz */
	{MODEPREFIX("640x480"),36000, 640,696,752,832,0, 480,481,484,509,0, V_NHSYNC | V_NVSYNC, MODESUFFIX},
	{MODEPREFIX("320x240"),18000, 320,348,376,416,0, 240,240,242,254,0, V_NHSYNC | V_NVSYNC | V_DBLSCAN, MODESUFFIX},
/* 800x600 @ 56Hz (VESA) hsync: 35.2kHz */
	{MODEPREFIX("800x600"),36000, 800,824,896,1024,0, 600,601,603,625,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("400x300"),18000, 400,412,448,512,0, 300,300,301,312,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 800x600 @ 60Hz (VESA) hsync: 37.9kHz */
	{MODEPREFIX("800x600"),40000, 800,840,968,1056,0, 600,601,605,628,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("400x300"),20000, 400,420,484,528,0, 300,300,302,314,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 800x600 @ 72Hz (VESA) hsync: 48.1kHz */
	{MODEPREFIX("800x600"),50000, 800,856,976,1040,0, 600,637,643,666,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("400x300"),25000, 400,428,488,520,0, 300,318,321,333,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 800x600 @ 75Hz (VESA) hsync: 46.9kHz */
	{MODEPREFIX("800x600"),49500, 800,816,896,1056,0, 600,601,604,625,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("400x300"),24750, 400,408,448,528,0, 300,300,302,312,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 800x600 @ 85Hz (VESA) hsync: 53.7kHz */
	{MODEPREFIX("800x600"),56300, 800,832,896,1048,0, 600,601,604,631,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("400x300"),28150, 400,416,448,524,0, 300,300,302,315,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1024x768 @ 60Hz (VESA) hsync: 48.4kHz */
	{MODEPREFIX("1024x768"),65000, 1024,1048,1184,1344,0, 768,771,777,806,0, V_NHSYNC | V_NVSYNC, MODESUFFIX},
	{MODEPREFIX("512x384"),32500, 512,524,592,672,0, 384,385,388,403,0, V_NHSYNC | V_NVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1024x768 @ 70Hz (VESA) hsync: 56.5kHz */
	{MODEPREFIX("1024x768"),75000, 1024,1048,1184,1328,0, 768,771,777,806,0, V_NHSYNC | V_NVSYNC, MODESUFFIX},
	{MODEPREFIX("512x384"),37500, 512,524,592,664,0, 384,385,388,403,0, V_NHSYNC | V_NVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1024x768 @ 75Hz (VESA) hsync: 60.0kHz */
	{MODEPREFIX("1024x768"),78800, 1024,1040,1136,1312,0, 768,769,772,800,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("512x384"),39400, 512,520,568,656,0, 384,384,386,400,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1024x768 @ 85Hz (VESA) hsync: 68.7kHz */
	{MODEPREFIX("1024x768"),94500, 1024,1072,1168,1376,0, 768,769,772,808,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("512x384"),47250, 512,536,584,688,0, 384,384,386,404,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1152x864 @ 75Hz (VESA) hsync: 67.5kHz */
	{MODEPREFIX("1152x864"),108000, 1152,1216,1344,1600,0, 864,865,868,900,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("576x432"),54000, 576,608,672,800,0, 432,432,434,450,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1280x960 @ 60Hz (VESA) hsync: 60.0kHz */
	{MODEPREFIX("1280x960"),108000, 1280,1376,1488,1800,0, 960,961,964,1000,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("640x480"),54000, 640,688,744,900,0, 480,480,482,500,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1280x960 @ 85Hz (VESA) hsync: 85.9kHz */
	{MODEPREFIX("1280x960"),148500, 1280,1344,1504,1728,0, 960,961,964,1011,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("640x480"),74250, 640,672,752,864,0, 480,480,482,505,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1280x1024 @ 60Hz (VESA) hsync: 64.0kHz */
	{MODEPREFIX("1280x1024"),108000, 1280,1328,1440,1688,0, 1024,1025,1028,1066,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("640x512"),54000, 640,664,720,844,0, 512,512,514,533,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1280x1024 @ 75Hz (VESA) hsync: 80.0kHz */
	{MODEPREFIX("1280x1024"),135000, 1280,1296,1440,1688,0, 1024,1025,1028,1066,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("640x512"),67500, 640,648,720,844,0, 512,512,514,533,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1280x1024 @ 85Hz (VESA) hsync: 91.1kHz */
	{MODEPREFIX("1280x1024"),157500, 1280,1344,1504,1728,0, 1024,1025,1028,1072,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("640x512"),78750, 640,672,752,864,0, 512,512,514,536,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1600x1200 @ 60Hz (VESA) hsync: 75.0kHz */
	{MODEPREFIX("1600x1200"),162000, 1600,1664,1856,2160,0, 1200,1201,1204,1250,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("800x600"),81000, 800,832,928,1080,0, 600,600,602,625,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1600x1200 @ 65Hz (VESA) hsync: 81.3kHz */
	{MODEPREFIX("1600x1200"),175500, 1600,1664,1856,2160,0, 1200,1201,1204,1250,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("800x600"),87750, 800,832,928,1080,0, 600,600,602,625,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1600x1200 @ 70Hz (VESA) hsync: 87.5kHz */
	{MODEPREFIX("1600x1200"),189000, 1600,1664,1856,2160,0, 1200,1201,1204,1250,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("800x600"),94500, 800,832,928,1080,0, 600,600,602,625,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1600x1200 @ 75Hz (VESA) hsync: 93.8kHz */
	{MODEPREFIX("1600x1200"),202500, 1600,1664,1856,2160,0, 1200,1201,1204,1250,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("800x600"),101250, 800,832,928,1080,0, 600,600,602,625,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1600x1200 @ 85Hz (VESA) hsync: 106.3kHz */
	{MODEPREFIX("1600x1200"),229500, 1600,1664,1856,2160,0, 1200,1201,1204,1250,0, V_PHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("800x600"),114750, 800,832,928,1080,0, 600,600,602,625,0, V_PHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1792x1344 @ 60Hz (VESA) hsync: 83.6kHz */
	{MODEPREFIX("1792x1344"),204800, 1792,1920,2120,2448,0, 1344,1345,1348,1394,0, V_NHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("896x672"),102400, 896,960,1060,1224,0, 672,672,674,697,0, V_NHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1792x1344 @ 75Hz (VESA) hsync: 106.3kHz */
	{MODEPREFIX("1792x1344"),261000, 1792,1888,2104,2456,0, 1344,1345,1348,1417,0, V_NHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("896x672"),130500, 896,944,1052,1228,0, 672,672,674,708,0, V_NHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1856x1392 @ 60Hz (VESA) hsync: 86.3kHz */
	{MODEPREFIX("1856x1392"),218300, 1856,1952,2176,2528,0, 1392,1393,1396,1439,0, V_NHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("928x696"),109150, 928,976,1088,1264,0, 696,696,698,719,0, V_NHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1856x1392 @ 75Hz (VESA) hsync: 112.5kHz */
	{MODEPREFIX("1856x1392"),288000, 1856,1984,2208,2560,0, 1392,1393,1396,1500,0, V_NHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("928x696"),144000, 928,992,1104,1280,0, 696,696,698,750,0, V_NHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1920x1440 @ 60Hz (VESA) hsync: 90.0kHz */
	{MODEPREFIX("1920x1440"),234000, 1920,2048,2256,2600,0, 1440,1441,1444,1500,0, V_NHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("960x720"),117000, 960,1024,1128,1300,0, 720,720,722,750,0, V_NHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},
/* 1920x1440 @ 75Hz (VESA) hsync: 112.5kHz */
	{MODEPREFIX("1920x1440"),297000, 1920,2064,2288,2640,0, 1440,1441,1444,1500,0, V_NHSYNC | V_PVSYNC, MODESUFFIX},
	{MODEPREFIX("960x720"),148500, 960,1032,1144,1320,0, 720,720,722,750,0, V_NHSYNC | V_PVSYNC | V_DBLSCAN, MODESUFFIX},

	/* Terminator */
	{MODEPREFIX(NULL), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MODESUFFIX}
};
