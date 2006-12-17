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
#include "radeon.h"
#include "radeon_xf86Modes.h"


#include <math.h>

#define rint(x) floor(x)

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

DisplayModePtr
RADEONGetGTF(int h_pixels, int v_lines, float freq, int interlaced, int margins)
{
    float h_pixels_rnd;
    float v_lines_rnd;
    float v_field_rate_rqd;
    float top_margin;
    float bottom_margin;
    float interlace;
    float h_period_est;
    float vsync_plus_bp;
    float v_back_porch;
    float total_v_lines;
    float v_field_rate_est;
    float h_period;
    float v_field_rate;
    float v_frame_rate;
    float left_margin;
    float right_margin;
    float total_active_pixels;
    float ideal_duty_cycle;
    float h_blank;
    float total_pixels;
    float pixel_freq;
    float h_freq;

    float h_sync;
    float h_front_porch;
    float v_odd_front_porch_lines;
    DisplayModePtr m;

    m = xnfcalloc(sizeof(DisplayModeRec), 1);
    
    
    /*  1. In order to give correct results, the number of horizontal
     *  pixels requested is first processed to ensure that it is divisible
     *  by the character size, by rounding it to the nearest character
     *  cell boundary:
     *
     *  [H PIXELS RND] = ((ROUND([H PIXELS]/[CELL GRAN RND],0))*[CELLGRAN RND])
     */
    
    h_pixels_rnd = rint((float) h_pixels / CELL_GRAN) * CELL_GRAN;
    
    
    /*  2. If interlace is requested, the number of vertical lines assumed
     *  by the calculation must be halved, as the computation calculates
     *  the number of vertical lines per field. In either case, the
     *  number of lines is rounded to the nearest integer.
     *   
     *  [V LINES RND] = IF([INT RQD?]="y", ROUND([V LINES]/2,0),
     *                                     ROUND([V LINES],0))
     */

    v_lines_rnd = interlaced ?
            rint((float) v_lines) / 2.0 :
            rint((float) v_lines);
    
    /*  3. Find the frame rate required:
     *
     *  [V FIELD RATE RQD] = IF([INT RQD?]="y", [I/P FREQ RQD]*2,
     *                                          [I/P FREQ RQD])
     */

    v_field_rate_rqd = interlaced ? (freq * 2.0) : (freq);

    /*  4. Find number of lines in Top margin:
     *
     *  [TOP MARGIN (LINES)] = IF([MARGINS RQD?]="Y",
     *          ROUND(([MARGIN%]/100*[V LINES RND]),0),
     *          0)
     */

    top_margin = margins ? rint(MARGIN_PERCENT / 100.0 * v_lines_rnd) : (0.0);

    /*  5. Find number of lines in Bottom margin:
     *
     *  [BOT MARGIN (LINES)] = IF([MARGINS RQD?]="Y",
     *          ROUND(([MARGIN%]/100*[V LINES RND]),0),
     *          0)
     */

    bottom_margin = margins ? rint(MARGIN_PERCENT/100.0 * v_lines_rnd) : (0.0);

    /*  6. If interlace is required, then set variable [INTERLACE]=0.5:
     *   
     *  [INTERLACE]=(IF([INT RQD?]="y",0.5,0))
     */

    interlace = interlaced ? 0.5 : 0.0;

    /*  7. Estimate the Horizontal period
     *
     *  [H PERIOD EST] = ((1/[V FIELD RATE RQD]) - [MIN VSYNC+BP]/1000000) /
     *                    ([V LINES RND] + (2*[TOP MARGIN (LINES)]) +
     *                     [MIN PORCH RND]+[INTERLACE]) * 1000000
     */

    h_period_est = (((1.0/v_field_rate_rqd) - (MIN_VSYNC_PLUS_BP/1000000.0))
                    / (v_lines_rnd + (2*top_margin) + MIN_PORCH + interlace)
                    * 1000000.0);

    /*  8. Find the number of lines in V sync + back porch:
     *
     *  [V SYNC+BP] = ROUND(([MIN VSYNC+BP]/[H PERIOD EST]),0)
     */

    vsync_plus_bp = rint(MIN_VSYNC_PLUS_BP/h_period_est);

    /*  9. Find the number of lines in V back porch alone:
     *
     *  [V BACK PORCH] = [V SYNC+BP] - [V SYNC RND]
     *
     *  XXX is "[V SYNC RND]" a typo? should be [V SYNC RQD]?
     */
    
    v_back_porch = vsync_plus_bp - V_SYNC_RQD;
    
    /*  10. Find the total number of lines in Vertical field period:
     *
     *  [TOTAL V LINES] = [V LINES RND] + [TOP MARGIN (LINES)] +
     *                    [BOT MARGIN (LINES)] + [V SYNC+BP] + [INTERLACE] +
     *                    [MIN PORCH RND]
     */

    total_v_lines = v_lines_rnd + top_margin + bottom_margin + vsync_plus_bp +
        interlace + MIN_PORCH;
    
    /*  11. Estimate the Vertical field frequency:
     *
     *  [V FIELD RATE EST] = 1 / [H PERIOD EST] / [TOTAL V LINES] * 1000000
     */

    v_field_rate_est = 1.0 / h_period_est / total_v_lines * 1000000.0;
    
    /*  12. Find the actual horizontal period:
     *
     *  [H PERIOD] = [H PERIOD EST] / ([V FIELD RATE RQD] / [V FIELD RATE EST])
     */

    h_period = h_period_est / (v_field_rate_rqd / v_field_rate_est);
    
    /*  13. Find the actual Vertical field frequency:
     *
     *  [V FIELD RATE] = 1 / [H PERIOD] / [TOTAL V LINES] * 1000000
     */

    v_field_rate = 1.0 / h_period / total_v_lines * 1000000.0;

    /*  14. Find the Vertical frame frequency:
     *
     *  [V FRAME RATE] = (IF([INT RQD?]="y", [V FIELD RATE]/2, [V FIELD RATE]))
     */

    v_frame_rate = interlaced ? v_field_rate / 2.0 : v_field_rate;

    /*  15. Find number of pixels in left margin:
     *
     *  [LEFT MARGIN (PIXELS)] = (IF( [MARGINS RQD?]="Y",
     *          (ROUND( ([H PIXELS RND] * [MARGIN%] / 100 /
     *                   [CELL GRAN RND]),0)) * [CELL GRAN RND],
     *          0))
     */

    left_margin = margins ?
        rint(h_pixels_rnd * MARGIN_PERCENT / 100.0 / CELL_GRAN) * CELL_GRAN :
        0.0;
    
    /*  16. Find number of pixels in right margin:
     *
     *  [RIGHT MARGIN (PIXELS)] = (IF( [MARGINS RQD?]="Y",
     *          (ROUND( ([H PIXELS RND] * [MARGIN%] / 100 /
     *                   [CELL GRAN RND]),0)) * [CELL GRAN RND],
     *          0))
     */
    
    right_margin = margins ?
        rint(h_pixels_rnd * MARGIN_PERCENT / 100.0 / CELL_GRAN) * CELL_GRAN :
        0.0;
    
    /*  17. Find total number of active pixels in image and left and right
     *  margins:
     *
     *  [TOTAL ACTIVE PIXELS] = [H PIXELS RND] + [LEFT MARGIN (PIXELS)] +
     *                          [RIGHT MARGIN (PIXELS)]
     */

    total_active_pixels = h_pixels_rnd + left_margin + right_margin;
    
    /*  18. Find the ideal blanking duty cycle from the blanking duty cycle
     *  equation:
     *
     *  [IDEAL DUTY CYCLE] = [C'] - ([M']*[H PERIOD]/1000)
     */

    ideal_duty_cycle = C_PRIME - (M_PRIME * h_period / 1000.0);
    
    /*  19. Find the number of pixels in the blanking time to the nearest
     *  double character cell:
     *
     *  [H BLANK (PIXELS)] = (ROUND(([TOTAL ACTIVE PIXELS] *
     *                               [IDEAL DUTY CYCLE] /
     *                               (100-[IDEAL DUTY CYCLE]) /
     *                               (2*[CELL GRAN RND])), 0))
     *                       * (2*[CELL GRAN RND])
     */

    h_blank = rint(total_active_pixels *
                   ideal_duty_cycle /
                   (100.0 - ideal_duty_cycle) /
                   (2.0 * CELL_GRAN)) * (2.0 * CELL_GRAN);
    
    /*  20. Find total number of pixels:
     *
     *  [TOTAL PIXELS] = [TOTAL ACTIVE PIXELS] + [H BLANK (PIXELS)]
     */

    total_pixels = total_active_pixels + h_blank;
    
    /*  21. Find pixel clock frequency:
     *
     *  [PIXEL FREQ] = [TOTAL PIXELS] / [H PERIOD]
     */
    
    pixel_freq = total_pixels / h_period;
    
    /*  22. Find horizontal frequency:
     *
     *  [H FREQ] = 1000 / [H PERIOD]
     */

    h_freq = 1000.0 / h_period;
    

    /* Stage 1 computations are now complete; I should really pass
       the results to another function and do the Stage 2
       computations, but I only need a few more values so I'll just
       append the computations here for now */

    

    /*  17. Find the number of pixels in the horizontal sync period:
     *
     *  [H SYNC (PIXELS)] =(ROUND(([H SYNC%] / 100 * [TOTAL PIXELS] /
     *                             [CELL GRAN RND]),0))*[CELL GRAN RND]
     */

    h_sync = rint(H_SYNC_PERCENT/100.0 * total_pixels / CELL_GRAN) * CELL_GRAN;

    /*  18. Find the number of pixels in the horizontal front porch period:
     *
     *  [H FRONT PORCH (PIXELS)] = ([H BLANK (PIXELS)]/2)-[H SYNC (PIXELS)]
     */

    h_front_porch = (h_blank / 2.0) - h_sync;

    /*  36. Find the number of lines in the odd front porch period:
     *
     *  [V ODD FRONT PORCH(LINES)]=([MIN PORCH RND]+[INTERLACE])
     */
    
    v_odd_front_porch_lines = MIN_PORCH + interlace;
    
    /* finally, pack the results in the DisplayMode struct */
    
    m->HDisplay  = (int) (h_pixels_rnd);
    m->HSyncStart = (int) (h_pixels_rnd + h_front_porch);
    m->HSyncEnd = (int) (h_pixels_rnd + h_front_porch + h_sync);
    m->HTotal = (int) (total_pixels);

    m->VDisplay  = (int) (v_lines_rnd);
    m->VSyncStart = (int) (v_lines_rnd + v_odd_front_porch_lines);
    m->VSyncEnd = (int) (int) (v_lines_rnd + v_odd_front_porch_lines + V_SYNC_RQD);
    m->VTotal = (int) (total_v_lines);

    m->Clock   = (int)(pixel_freq * 1000);
    m->SynthClock   = m->Clock;
    m->HSync = h_freq;
    m->VRefresh = v_frame_rate /* freq */;

    RADEONxf86SetModeDefaultName(m);

    return (m);
}

void
RADEONPrintModes(ScrnInfoPtr scrp)
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
RADEONxf86SortModes(DisplayModePtr new, DisplayModePtr *first,
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

DisplayModePtr
RADEONGetModeListTail(DisplayModePtr pModeList)
{
    DisplayModePtr last;

    if (pModeList == NULL)
	return NULL;

    for (last = pModeList; last->next != NULL; last = last->next)
	;

    return last;
}

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
RADEONxf86ModeHSync(DisplayModePtr mode)
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
RADEONxf86ModeVRefresh(DisplayModePtr mode)
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
RADEONxf86SetModeDefaultName(DisplayModePtr mode)
{
    if (mode->name != NULL)
	xfree(mode->name);

    mode->name = XNFprintf("%dx%d", mode->HDisplay, mode->VDisplay);
}

/*
 * RADEONxf86SetModeCrtc
 *
 * Initialises the Crtc parameters for a mode.  The initialisation includes
 * adjustments for interlaced and double scan modes.
 *
 * Exact copy of xf86Mode.c's.
 */
void
RADEONxf86SetModeCrtc(DisplayModePtr p, int adjustFlags)
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
 * Allocates and returns a copy of pMode, including pointers within pMode.
 */
DisplayModePtr
RADEONxf86DuplicateMode(DisplayModePtr pMode)
{
    DisplayModePtr pNew;

    pNew = xnfalloc(sizeof(DisplayModeRec));
    *pNew = *pMode;
    pNew->next = NULL;
    pNew->prev = NULL;
    if (pNew->name == NULL) {
	RADEONxf86SetModeDefaultName(pMode);
    } else {
	pNew->name = xnfstrdup(pMode->name);
    }

    return pNew;
}

/**
 * Duplicates every mode in the given list and returns a pointer to the first
 * mode.
 *
 * \param modeList doubly-linked mode list
 */
DisplayModePtr
RADEONxf86DuplicateModes(ScrnInfoPtr pScrn, DisplayModePtr modeList)
{
    DisplayModePtr first = NULL, last = NULL;
    DisplayModePtr mode;

    for (mode = modeList; mode != NULL; mode = mode->next) {
	DisplayModePtr new;

	new = RADEONxf86DuplicateMode(mode);

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

/**
 * Returns true if the given modes should program to the same timings.
 *
 * This doesn't use Crtc values, as it might be used on ModeRecs without the
 * Crtc values set.  So, it's assumed that the other numbers are enough.
 *
 * This isn't in xf86Modes.c, but it might deserve to be there.
 */
Bool
RADEONModesEqual(DisplayModePtr pMode1, DisplayModePtr pMode2)
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
    xf86DrvMsg(scrnIndex, X_INFO,
		   "Modeline \"%s\"x%.01f  %6.2f  %i %i %i %i  %i %i %i %i%s "
		   "(%.01f kHz)\n",
		   mode->name, mode->VRefresh, mode->Clock/1000., mode->HDisplay,
		   mode->HSyncStart, mode->HSyncEnd, mode->HTotal,
		   mode->VDisplay, mode->VSyncStart, mode->VSyncEnd,
		   mode->VTotal, flags, RADEONxf86ModeHSync(mode));
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
RADEONxf86ValidateModesFlags(ScrnInfoPtr pScrn, DisplayModePtr modeList,
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
RADEONxf86ValidateModesSize(ScrnInfoPtr pScrn, DisplayModePtr modeList,
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
RADEONxf86ValidateModesSync(ScrnInfoPtr pScrn, DisplayModePtr modeList,
			  MonPtr mon)
{
    DisplayModePtr mode;

    for (mode = modeList; mode != NULL; mode = mode->next) {
	Bool bad;
	int i;

	bad = TRUE;
	for (i = 0; i < mon->nHsync; i++) {
	    if (RADEONxf86ModeHSync(mode) >= mon->hsync[i].lo &&
		RADEONxf86ModeHSync(mode) <= mon->hsync[i].hi)
	    {
		bad = FALSE;
	    }
	}
	if (bad)
	    mode->status = MODE_HSYNC;

	bad = TRUE;
	for (i = 0; i < mon->nVrefresh; i++) {
	    if (RADEONxf86ModeVRefresh(mode) >= mon->vrefresh[i].lo &&
		RADEONxf86ModeVRefresh(mode) <= mon->vrefresh[i].hi)
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
RADEONxf86ValidateModesClocks(ScrnInfoPtr pScrn, DisplayModePtr modeList,
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
RADEONxf86ValidateModesUserConfig(ScrnInfoPtr pScrn, DisplayModePtr modeList)
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
RADEONxf86PruneInvalidModes(ScrnInfoPtr pScrn, DisplayModePtr *modeList,
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

