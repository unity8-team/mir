/*
 * Copyright © 2006 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "xf86.h"
#include "xf86DDC.h"
/*#include "i830.h" */
#include "i830_xf86Crtc.h"
#include "i830_xf86Modes.h"
#include "i830_randr.h"
#include "X11/extensions/render.h"
#define DPMS_SERVER
#include "X11/extensions/dpms.h"
#include "X11/Xatom.h"

/*
 * Initialize xf86CrtcConfig structure
 */

int xf86CrtcConfigPrivateIndex = -1;

void
xf86CrtcConfigInit (ScrnInfoPtr scrn)
{
    xf86CrtcConfigPtr	config;
    
    if (xf86CrtcConfigPrivateIndex == -1)
	xf86CrtcConfigPrivateIndex = xf86AllocateScrnInfoPrivateIndex();
    config = xnfcalloc (1, sizeof (xf86CrtcConfigRec));
    scrn->privates[xf86CrtcConfigPrivateIndex].ptr = config;
}
 
void
xf86CrtcSetSizeRange (ScrnInfoPtr scrn,
		      int minWidth, int minHeight,
		      int maxWidth, int maxHeight)
{
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(scrn);

    config->minWidth = minWidth;
    config->minHeight = minHeight;
    config->maxWidth = maxWidth;
    config->maxHeight = maxHeight;
}

/*
 * Crtc functions
 */
xf86CrtcPtr
xf86CrtcCreate (ScrnInfoPtr		scrn,
		const xf86CrtcFuncsRec	*funcs)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
    xf86CrtcPtr		crtc, *crtcs;

    crtc = xcalloc (sizeof (xf86CrtcRec), 1);
    if (!crtc)
	return NULL;
    crtc->scrn = scrn;
    crtc->funcs = funcs;
#ifdef RANDR_12_INTERFACE
    crtc->randr_crtc = NULL;
#endif
    if (xf86_config->crtc)
	crtcs = xrealloc (xf86_config->crtc,
			  (xf86_config->num_crtc + 1) * sizeof (xf86CrtcPtr));
    else
	crtcs = xalloc ((xf86_config->num_crtc + 1) * sizeof (xf86CrtcPtr));
    if (!crtcs)
    {
	xfree (crtc);
	return NULL;
    }
    xf86_config->crtc = crtcs;
    xf86_config->crtc[xf86_config->num_crtc++] = crtc;
    return crtc;
}

void
xf86CrtcDestroy (xf86CrtcPtr crtc)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
    int			c;
    
    (*crtc->funcs->destroy) (crtc);
    for (c = 0; c < xf86_config->num_crtc; c++)
	if (xf86_config->crtc[c] == crtc)
	{
	    memmove (&xf86_config->crtc[c],
		     &xf86_config->crtc[c+1],
		     xf86_config->num_crtc - (c + 1));
	    xf86_config->num_crtc--;
	    break;
	}
    xfree (crtc);
}

/*
 * Output functions
 */

extern XF86ConfigPtr xf86configptr;

static void
xf86OutputSetMonitor (xf86OutputPtr output)
{
    char    *option_name;
    static const char monitor_prefix[] = "monitor-";
    char    *monitor;

    option_name = xnfalloc (strlen (monitor_prefix) +
			    strlen (output->name) + 1);
    strcpy (option_name, monitor_prefix);
    strcat (option_name, output->name);
    monitor = xf86findOptionValue (output->scrn->options, option_name);
    if (!monitor)
	monitor = output->name;
    else
	xf86MarkOptionUsedByName (output->scrn->options, option_name);
    free (option_name);
    output->conf_monitor = xf86findMonitor (monitor, xf86configptr->conf_monitor_lst);
}

xf86OutputPtr
xf86OutputCreate (ScrnInfoPtr		    scrn,
		  const xf86OutputFuncsRec *funcs,
		  const char		    *name)
{
    xf86OutputPtr	output, *outputs;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
    int			len = strlen (name);

    output = xcalloc (sizeof (xf86OutputRec) + len + 1, 1);
    if (!output)
	return NULL;
    output->scrn = scrn;
    output->funcs = funcs;
    output->name = (char *) (output + 1);
    output->subpixel_order = SubPixelUnknown;
    strcpy (output->name, name);
#ifdef RANDR_12_INTERFACE
    output->randr_output = NULL;
#endif
    if (xf86_config->output)
	outputs = xrealloc (xf86_config->output,
			  (xf86_config->num_output + 1) * sizeof (xf86OutputPtr));
    else
	outputs = xalloc ((xf86_config->num_output + 1) * sizeof (xf86OutputPtr));
    if (!outputs)
    {
	xfree (output);
	return NULL;
    }
    
    xf86_config->output = outputs;
    xf86_config->output[xf86_config->num_output++] = output;
    
    xf86OutputSetMonitor (output);
    return output;
}

void
xf86OutputRename (xf86OutputPtr output, const char *name)
{
    int	    len = strlen(name);
    char    *newname = xalloc (len + 1);
    
    if (!newname)
	return;	/* so sorry... */
    
    strcpy (newname, name);
    if (output->name != (char *) (output + 1))
	xfree (output->name);
    output->name = newname;
    xf86OutputSetMonitor (output);
}

void
xf86OutputDestroy (xf86OutputPtr output)
{
    ScrnInfoPtr		scrn = output->scrn;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
    int			o;
    
    (*output->funcs->destroy) (output);
    while (output->probed_modes)
	xf86DeleteMode (&output->probed_modes, output->probed_modes);
    for (o = 0; o < xf86_config->num_output; o++)
	if (xf86_config->output[o] == output)
	{
	    memmove (&xf86_config->output[o],
		     &xf86_config->output[o+1],
		     xf86_config->num_output - (o + 1));
	    xf86_config->num_output--;
	    break;
	}
    if (output->name != (char *) (output + 1))
	xfree (output->name);
    xfree (output);
}

static DisplayModePtr
xf86DefaultMode (xf86OutputPtr output, int width, int height)
{
    DisplayModePtr  target_mode = NULL;
    DisplayModePtr  mode;
    int		    target_diff = 0;
    int		    target_preferred = 0;
    int		    mm_height;
    
    mm_height = output->mm_height;
    if (!mm_height)
	mm_height = 203;	/* 768 pixels at 96dpi */
    /*
     * Pick a mode closest to 96dpi 
     */
    for (mode = output->probed_modes; mode; mode = mode->next)
    {
	int	    dpi;
	int	    preferred = (mode->type & M_T_PREFERRED) != 0;
	int	    diff;

	if (mode->HDisplay > width || mode->VDisplay > height) continue;
	dpi = (mode->HDisplay * 254) / (mm_height * 10);
	diff = dpi - 96;
	diff = diff < 0 ? -diff : diff;
	if (target_mode == NULL || (preferred > target_preferred) ||
	    (preferred == target_preferred && diff < target_diff))
	{
	    target_mode = mode;
	    target_diff = diff;
	    target_preferred = preferred;
	}
    }
    return target_mode;
}

static DisplayModePtr
xf86ClosestMode (xf86OutputPtr output, DisplayModePtr match,
		 int width, int height)
{
    DisplayModePtr  target_mode = NULL;
    DisplayModePtr  mode;
    int		    target_diff = 0;
    
    /*
     * Pick a mode closest to the specified mode
     */
    for (mode = output->probed_modes; mode; mode = mode->next)
    {
	int	    dx, dy;
	int	    diff;

	if (mode->HDisplay > width || mode->VDisplay > height) continue;
	
	/* exact matches are preferred */
	if (xf86ModesEqual (mode, match))
	    return mode;
	
	dx = match->HDisplay - mode->HDisplay;
	dy = match->VDisplay - mode->VDisplay;
	diff = dx * dx + dy * dy;
	if (target_mode == NULL || diff < target_diff)
	{
	    target_mode = mode;
	    target_diff = diff;
	}
    }
    return target_mode;
}

static Bool
xf86OutputHasPreferredMode (xf86OutputPtr output, int width, int height)
{
    DisplayModePtr  mode;

    for (mode = output->probed_modes; mode; mode = mode->next)
    {
	if (mode->HDisplay > width || mode->VDisplay > height) continue;
	if (mode->type & M_T_PREFERRED)
	    return TRUE;
    }
    return FALSE;
}

static int
xf86PickCrtcs (ScrnInfoPtr	pScrn,
	       xf86CrtcPtr	*best_crtcs,
	       DisplayModePtr	*modes,
	       int		n,
	       int		width,
	       int		height)
{
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(pScrn);
    int		    c, o, l;
    xf86OutputPtr   output;
    xf86CrtcPtr	    crtc;
    xf86CrtcPtr	    *crtcs;
    xf86CrtcPtr	    best_crtc;
    int		    best_score;
    int		    score;
    int		    my_score;
    
    if (n == config->num_output)
	return 0;
    output = config->output[n];
    
    /*
     * Compute score with this output disabled
     */
    best_crtcs[n] = NULL;
    best_crtc = NULL;
    best_score = xf86PickCrtcs (pScrn, best_crtcs, modes, n+1, width, height);
    if (modes[n] == NULL)
	return best_score;
    
    crtcs = xalloc (config->num_output * sizeof (xf86CrtcPtr));
    if (!crtcs)
	return best_score;

    my_score = 1;
    /* Score outputs that are known to be connected higher */
    if (output->status == XF86OutputStatusConnected)
	my_score++;
    /* Score outputs with preferred modes higher */
    if (xf86OutputHasPreferredMode (output, width, height))
	my_score++;
    /*
     * Select a crtc for this output and
     * then attempt to configure the remaining
     * outputs
     */
    for (c = 0; c < config->num_crtc; c++)
    {
	if ((output->possible_crtcs & (1 << c)) == 0)
	    continue;
	
	crtc = config->crtc[c];
	/*
	 * Check to see if some other output is
	 * using this crtc
	 */
	for (o = 0; o < n; o++)
	    if (best_crtcs[o] == crtc)
		break;
	if (o < n)
	{
	    /*
	     * If the two outputs desire the same mode,
	     * see if they can be cloned
	     */
	    if (xf86ModesEqual (modes[o], modes[n]))
	    {
		for (l = 0; l < config->num_output; l++)
		    if (output->possible_clones & (1 << l))
			break;
		if (l == config->num_output)
		    continue;		/* nope, try next CRTC */
	    }
	    else
		continue;		/* different modes, can't clone */
	}
	crtcs[n] = crtc;
	memcpy (crtcs, best_crtcs, n * sizeof (xf86CrtcPtr));
	score = my_score + xf86PickCrtcs (pScrn, crtcs, modes, n+1, width, height);
	if (score > best_score)
	{
	    best_crtc = crtc;
	    best_score = score;
	    memcpy (best_crtcs, crtcs, config->num_output * sizeof (xf86CrtcPtr));
	}
    }
    xfree (crtcs);
    return best_score;
}


/*
 * Compute the virtual size necessary to place all of the available
 * crtcs in a panorama configuration
 */

static void
xf86DefaultScreenLimits (ScrnInfoPtr pScrn, int *widthp, int *heightp)
{
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(pScrn);
    int	    width = 0, height = 0;
    int	    o;
    int	    c;
    int	    s;

    for (c = 0; c < config->num_crtc; c++)
    {
	int	    crtc_width = 0, crtc_height = 0;

	for (o = 0; o < config->num_output; o++) 
	{
	    xf86OutputPtr   output = config->output[o];

	    for (s = 0; s < config->num_crtc; s++)
		if (output->possible_crtcs & (1 << s))
		{
		    DisplayModePtr  mode;
		    for (mode = output->probed_modes; mode; mode = mode->next)
		    {
			if (mode->HDisplay > crtc_width)
			    crtc_width = mode->HDisplay;
			if (mode->VDisplay > crtc_height)
			    crtc_height = mode->VDisplay;
		    }
		}
	}
	width += crtc_width;
	if (crtc_height > height)
	    height = crtc_height;
    }
    if (config->maxWidth && width > config->maxWidth) width = config->maxWidth;
    if (config->maxHeight && height > config->maxHeight) height = config->maxHeight;
    if (config->minWidth && width < config->minWidth) width = config->minWidth;
    if (config->minHeight && height < config->minHeight) height = config->minHeight;
    *widthp = width;
    *heightp = height;
}

/*
 * XXX walk the monitor mode list and prune out duplicates that
 * are inserted by xf86DDCMonitorSet. In an ideal world, that
 * function would do this work by itself.
 */

static void
xf86PruneDuplicateMonitorModes (MonPtr Monitor)
{
    DisplayModePtr  master, clone, next;

    for (master = Monitor->Modes; 
	 master && master != Monitor->Last; 
	 master = master->next)
    {
	for (clone = master->next; clone && clone != Monitor->Modes; clone = next)
	{
	    next = clone->next;
	    if (xf86ModesEqual (master, clone))
	    {
		if (Monitor->Last == clone)
		    Monitor->Last = clone->prev;
		xf86DeleteMode (&Monitor->Modes, clone);
	    }
	}
    }
}

/** Return - 0 + if a should be earlier, same or later than b in list
 */
static int
i830xf86ModeCompare (DisplayModePtr a, DisplayModePtr b)
{
    int	diff;

    diff = ((b->type & M_T_PREFERRED) != 0) - ((a->type & M_T_PREFERRED) != 0);
    if (diff)
	return diff;
    diff = b->HDisplay * b->VDisplay - a->HDisplay * a->VDisplay;
    if (diff)
	return diff;
    diff = b->Clock - a->Clock;
    return diff;
}

/**
 * Insertion sort input in-place and return the resulting head
 */
static DisplayModePtr
i830xf86SortModes (DisplayModePtr input)
{
    DisplayModePtr  output = NULL, i, o, n, *op, prev;

    /* sort by preferred status and pixel area */
    while (input)
    {
	i = input;
	input = input->next;
	for (op = &output; (o = *op); op = &o->next)
	    if (i830xf86ModeCompare (o, i) > 0)
		break;
	i->next = *op;
	*op = i;
    }
    /* prune identical modes */
    for (o = output; o && (n = o->next); o = n)
    {
	if (!strcmp (o->name, n->name) && xf86ModesEqual (o, n))
	{
	    o->next = n->next;
	    xfree (n->name);
	    xfree (n);
	    n = o;
	}
    }
    /* hook up backward links */
    prev = NULL;
    for (o = output; o; o = o->next)
    {
	o->prev = prev;
	prev = o;
    }
    return output;
}

#define DEBUG_REPROBE 1

void
xf86ProbeOutputModes (ScrnInfoPtr pScrn)
{
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(pScrn);
    int			o;
    int			virtualX, virtualY;

    xf86RandR12GetOriginalVirtualSize (pScrn, &virtualX, &virtualY);

    /* Elide duplicate modes before defaulting code uses them */
    xf86PruneDuplicateMonitorModes (pScrn->monitor);
    
    /* Probe the list of modes for each output. */
    for (o = 0; o < config->num_output; o++) 
    {
	xf86OutputPtr	    output = config->output[o];
	DisplayModePtr	    mode;
	DisplayModePtr	    config_modes = NULL, output_modes, default_modes;
	XF86ConfMonitorPtr  conf_monitor;
	xf86MonPtr	    edid_monitor;
	MonRec		    mon_rec;
	int		    min_clock = 0;
	int		    max_clock = 0;
	enum { sync_config, sync_edid, sync_default } sync_source = sync_default;
	
	while (output->probed_modes != NULL)
	    xf86DeleteMode(&output->probed_modes, output->probed_modes);

	/*
	 * Check connection status
	 */
	output->status = (*output->funcs->detect)(output);

	if (output->status == XF86OutputStatusDisconnected)
	    continue;

	memset (&mon_rec, '\0', sizeof (mon_rec));
	
	conf_monitor = output->conf_monitor;
	
	if (conf_monitor)
	{
	    int	i;
	    
	    for (i = 0; i < conf_monitor->mon_n_hsync; i++)
	    {
		mon_rec.hsync[mon_rec.nHsync].lo = conf_monitor->mon_hsync[i].lo;
		mon_rec.hsync[mon_rec.nHsync].hi = conf_monitor->mon_hsync[i].hi;
		mon_rec.nHsync++;
		sync_source = sync_config;
	    }
	    for (i = 0; i < conf_monitor->mon_n_vrefresh; i++)
	    {
		mon_rec.vrefresh[mon_rec.nVrefresh].lo = conf_monitor->mon_vrefresh[i].lo;
		mon_rec.vrefresh[mon_rec.nVrefresh].hi = conf_monitor->mon_vrefresh[i].hi;
		mon_rec.nVrefresh++;
		sync_source = sync_config;
	    }
	    config_modes = i830xf86GetMonitorModes (pScrn, conf_monitor);
	}
	
	output_modes = (*output->funcs->get_modes) (output);
	
	edid_monitor = output->MonInfo;
	
	if (edid_monitor)
	{
	    int			    i;
	    Bool		    set_hsync = mon_rec.nHsync == 0;
	    Bool		    set_vrefresh = mon_rec.nVrefresh == 0;

	    for (i = 0; i < sizeof (edid_monitor->det_mon) / sizeof (edid_monitor->det_mon[0]); i++)
	    {
		if (edid_monitor->det_mon[i].type == DS_RANGES)
		{
		    struct monitor_ranges   *ranges = &edid_monitor->det_mon[i].section.ranges;
		    if (set_hsync && ranges->max_h)
		    {
			mon_rec.hsync[mon_rec.nHsync].lo = ranges->min_h;
			mon_rec.hsync[mon_rec.nHsync].hi = ranges->max_h;
			mon_rec.nHsync++;
			if (sync_source == sync_default)
			    sync_source = sync_edid;
		    }
		    if (set_vrefresh && ranges->max_v)
		    {
			mon_rec.vrefresh[mon_rec.nVrefresh].lo = ranges->min_v;
			mon_rec.vrefresh[mon_rec.nVrefresh].hi = ranges->max_v;
			mon_rec.nVrefresh++;
			if (sync_source == sync_default)
			    sync_source = sync_edid;
		    }
		    if (ranges->max_clock > max_clock)
			max_clock = ranges->max_clock;
		}
	    }
	}
	/*
	 * These limits will end up setting a 1024x768@60Hz mode by default,
	 * which seems like a fairly good mode to use when nothing else is
	 * specified
	 */
	if (mon_rec.nHsync == 0)
	{
	    mon_rec.hsync[0].lo = 31.0;
	    mon_rec.hsync[0].hi = 55.0;
	    mon_rec.nHsync = 1;
	}
	if (mon_rec.nVrefresh == 0)
	{
	    mon_rec.vrefresh[0].lo = 58.0;
	    mon_rec.vrefresh[0].hi = 62.0;
	    mon_rec.nVrefresh = 1;
	}
	default_modes = i830xf86GetDefaultModes (output->interlaceAllowed,
						 output->doubleScanAllowed);
	
	if (sync_source == sync_config)
	{
	    /* 
	     * Check output and config modes against sync range from config file
	     */
	    i830xf86ValidateModesSync (pScrn, output_modes, &mon_rec);
	    i830xf86ValidateModesSync (pScrn, config_modes, &mon_rec);
	}
	/*
	 * Check default modes against sync range
	 */
        i830xf86ValidateModesSync (pScrn, default_modes, &mon_rec);
	/*
	 * Check default modes against monitor max clock
	 */
	if (max_clock)
	    i830xf86ValidateModesClocks(pScrn, default_modes,
					&min_clock, &max_clock, 1);
	
	output->probed_modes = NULL;
	output->probed_modes = xf86ModesAdd (output->probed_modes, config_modes);
	output->probed_modes = xf86ModesAdd (output->probed_modes, output_modes);
	output->probed_modes = xf86ModesAdd (output->probed_modes, default_modes);
	
	/*
	 * Check all modes against virtual size
	 */
	i830xf86ValidateModesSize (pScrn, output->probed_modes, virtualX, virtualY, 0);
	 
	/*
	 * Check all modes against output
	 */
	for (mode = output->probed_modes; mode != NULL; mode = mode->next) 
	    if (mode->status == MODE_OK)
		mode->status = (*output->funcs->mode_valid)(output, mode);
	
	i830xf86PruneInvalidModes(pScrn, &output->probed_modes, TRUE);
	
	output->probed_modes = i830xf86SortModes (output->probed_modes);
	
	/* Check for a configured preference for a particular mode */
	if (conf_monitor)
	{
	    char  *preferred_mode = xf86findOptionValue (conf_monitor->mon_option_lst,
							 "Preferred Mode");

	    if (preferred_mode)
	    {
		for (mode = output->probed_modes; mode; mode = mode->next)
		    if (!strcmp (preferred_mode, mode->name))
			break;
		if (mode && mode != output->probed_modes)
		{
		    if (mode->prev)
			mode->prev->next = mode->next;
		    if (mode->next)
			mode->next->prev = mode->prev;
		    mode->next = output->probed_modes;
		    output->probed_modes->prev = mode;
		    mode->prev = NULL;
		    output->probed_modes = mode;
		    mode->type |= M_T_PREFERRED;
		}
	    }
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
	    mode->VRefresh = xf86ModeVRefresh(mode);
	    xf86SetModeCrtc(mode, INTERLACE_HALVE_V);

#ifdef DEBUG_REPROBE
	    xf86PrintModeline(pScrn->scrnIndex, mode);
#endif
	}
    }
}


/**
 * Copy one of the output mode lists to the ScrnInfo record
 */

/* XXX where does this function belong? Here? */
void
xf86RandR12GetOriginalVirtualSize(ScrnInfoPtr pScrn, int *x, int *y);

void
xf86SetScrnInfoModes (ScrnInfoPtr pScrn)
{
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(pScrn);
    xf86OutputPtr	output;
    xf86CrtcPtr		crtc;
    DisplayModePtr	last, mode;
    int			originalVirtualX, originalVirtualY;

    output = config->output[config->compat_output];
    if (!output->crtc)
    {
	int o;

	output = NULL;
	for (o = 0; o < config->num_output; o++)
	    if (config->output[o]->crtc)
	    {
		config->compat_output = o;
		output = config->output[o];
		break;
	    }
	/* no outputs are active, punt and leave things as they are */
	if (!output)
	    return;
    }
    crtc = output->crtc;

    /* Clear any existing modes from pScrn->modes */
    while (pScrn->modes != NULL)
	xf86DeleteMode(&pScrn->modes, pScrn->modes);

    /* Set pScrn->modes to the mode list for the 'compat' output */
    pScrn->modes = xf86DuplicateModes(pScrn, output->probed_modes);

    xf86RandR12GetOriginalVirtualSize(pScrn, &originalVirtualX, &originalVirtualY);

    /* Disable modes in the XFree86 DDX list that are larger than the current
     * virtual size.
     */
    i830xf86ValidateModesSize(pScrn, pScrn->modes,
			      originalVirtualX, originalVirtualY,
			      pScrn->displayWidth);

    /* Strip out anything that we threw out for virtualX/Y. */
    i830xf86PruneInvalidModes(pScrn, &pScrn->modes, TRUE);

    for (mode = pScrn->modes; mode; mode = mode->next)
	if (xf86ModesEqual (mode, &crtc->desiredMode))
	    break;

    if (pScrn->modes != NULL) {
	/* For some reason, pScrn->modes is circular, unlike the other mode
	 * lists.  How great is that?
	 */
	for (last = pScrn->modes; last && last->next; last = last->next)
	    ;
	last->next = pScrn->modes;
	pScrn->modes->prev = last;
	if (mode) {
	    while (pScrn->modes != mode)
		pScrn->modes = pScrn->modes->next;
	}
    }
    pScrn->currentMode = pScrn->modes;
}

/**
 * Construct default screen configuration
 *
 * Given auto-detected (and, eventually, configured) values,
 * construct a usable configuration for the system
 */

Bool
xf86InitialConfiguration (ScrnInfoPtr	    pScrn)
{
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(pScrn);
    int			o, c;
    DisplayModePtr	target_mode = NULL;
    xf86CrtcPtr		*crtcs;
    DisplayModePtr	*modes;
    int			width, height;

    xf86ProbeOutputModes (pScrn);

    if (pScrn->display->virtualX == 0)
    {
	/*
	 * Expand virtual size to cover potential mode switches
	 */
	xf86DefaultScreenLimits (pScrn, &width, &height);
    
	pScrn->display->virtualX = width;
	pScrn->display->virtualY = height;
    }
    else
    {
	width = pScrn->display->virtualX;
	height = pScrn->display->virtualY;
    }
    if (width > pScrn->virtualX)
	pScrn->virtualX = width;
    if (height > pScrn->virtualY)
	pScrn->virtualY = height;
    
    crtcs = xnfcalloc (config->num_output, sizeof (xf86CrtcPtr));
    modes = xnfcalloc (config->num_output, sizeof (DisplayModePtr));
    
    for (o = 0; o < config->num_output; o++)
	modes[o] = NULL;
    
    /*
     * Let outputs with preferred modes drive screen size
     */
    for (o = 0; o < config->num_output; o++)
    {
	xf86OutputPtr output = config->output[o];

	if (output->status != XF86OutputStatusDisconnected &&
	    xf86OutputHasPreferredMode (output, width, height))
	{
	    target_mode = xf86DefaultMode (output, width, height);
	    if (target_mode)
	    {
		modes[o] = target_mode;
		config->compat_output = o;
		break;
	    }
	}
    }
    if (!target_mode)
    {
	for (o = 0; o < config->num_output; o++)
	{
	    xf86OutputPtr output = config->output[o];
	    if (output->status != XF86OutputStatusDisconnected)
	    {
		target_mode = xf86DefaultMode (output, width, height);
		if (target_mode)
		{
		    modes[o] = target_mode;
		    config->compat_output = o;
		    break;
		}
	    }
	}
    }
    for (o = 0; o < config->num_output; o++)
    {
	xf86OutputPtr output = config->output[o];
	
	if (output->status != XF86OutputStatusDisconnected && !modes[o])
	    modes[o] = xf86ClosestMode (output, target_mode, width, height);
    }

    if (!xf86PickCrtcs (pScrn, crtcs, modes, 0, width, height))
    {
	xfree (crtcs);
	xfree (modes);
	return FALSE;
    }
    
    /* XXX override xf86 common frame computation code */
    
    pScrn->display->frameX0 = 0;
    pScrn->display->frameY0 = 0;
    
    for (c = 0; c < config->num_crtc; c++)
    {
	xf86CrtcPtr	crtc = config->crtc[c];

	crtc->enabled = FALSE;
	memset (&crtc->desiredMode, '\0', sizeof (crtc->desiredMode));
    }
	
    /*
     * Set initial configuration
     */
    for (o = 0; o < config->num_output; o++)
    {
	xf86OutputPtr	output = config->output[o];
	DisplayModePtr	mode = modes[o];
        xf86CrtcPtr	crtc = crtcs[o];

	if (mode && crtc)
	{
	    crtc->desiredMode = *mode;
	    crtc->enabled = TRUE;
	    crtc->x = 0;
	    crtc->y = 0;
	    output->crtc = crtc;
	    /* XXX set position; for now, we clone */
	}
    }
    
    /* Mirror output modes to pScrn mode list */
    xf86SetScrnInfoModes (pScrn);
    
    xfree (crtcs);
    xfree (modes);
    return TRUE;
}

/**
 * Set the DPMS power mode of all outputs and CRTCs.
 *
 * If the new mode is off, it will turn off outputs and then CRTCs.
 * Otherwise, it will affect CRTCs before outputs.
 */
void
xf86DPMSSet(ScrnInfoPtr pScrn, int mode, int flags)
{
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(pScrn);
    int			i;

    if (mode == DPMSModeOff) {
	for (i = 0; i < config->num_output; i++) {
	    xf86OutputPtr output = config->output[i];
	    if (output->crtc != NULL)
		(*output->funcs->dpms) (output, mode);
	}
    }

    for (i = 0; i < config->num_crtc; i++) {
	xf86CrtcPtr crtc = config->crtc[i];
	if (crtc->enabled)
	    (*crtc->funcs->dpms) (crtc, mode);
    }

    if (mode != DPMSModeOff) {
	for (i = 0; i < config->num_output; i++) {
	    xf86OutputPtr output = config->output[i];
	    if (output->crtc != NULL)
		(*output->funcs->dpms) (output, mode);
	}
    }
}

#ifdef RANDR_12_INTERFACE

#define EDID_ATOM_NAME		"EDID_DATA"

/**
 * Set the RandR EDID property
 */
static void
xf86OutputSetEDIDProperty (xf86OutputPtr output, void *data, int data_len)
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
 * Set the EDID information for the specified output
 */
void
i830_xf86OutputSetEDID (xf86OutputPtr output, xf86MonPtr edid_mon)
{
    ScrnInfoPtr		pScrn = output->scrn;
    xf86CrtcConfigPtr	config = XF86_CRTC_CONFIG_PTR(pScrn);
    int			i, size;
    
    if (output->MonInfo != NULL)
	xfree(output->MonInfo);
    
    output->MonInfo = edid_mon;

    /* Debug info for now, at least */
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "EDID for output %s\n", output->name);
    xf86PrintEDID(edid_mon);
    
    /* Set the DDC properties for the 'compat' output */
    if (output == config->output[config->compat_output])
        xf86SetDDCproperties(pScrn, edid_mon);

#ifdef RANDR_12_INTERFACE
    /* Set the RandR output properties */
    size = 0;
    if (edid_mon)
    {
	if (edid_mon->ver.version == 1)
	    size = 128;
	else if (edid_mon->ver.version == 2)
	    size = 256;
    }
    xf86OutputSetEDIDProperty (output, edid_mon ? edid_mon->rawData : NULL, size);
#endif

    if (edid_mon)
    {
	/* Pull out a phyiscal size from a detailed timing if available. */
	for (i = 0; i < 4; i++) {
	    if (edid_mon->det_mon[i].type == DT &&
		edid_mon->det_mon[i].section.d_timings.h_size != 0 &&
		edid_mon->det_mon[i].section.d_timings.v_size != 0)
	    {
		output->mm_width = edid_mon->det_mon[i].section.d_timings.h_size;
		output->mm_height = edid_mon->det_mon[i].section.d_timings.v_size;
		break;
	    }
	}
    
	/* if no mm size is available from a detailed timing, check the max size field */
	if ((!output->mm_width || !output->mm_height) &&
	    (edid_mon->features.hsize && edid_mon->features.vsize))
	{
	    output->mm_width = edid_mon->features.hsize * 10;
	    output->mm_height = edid_mon->features.vsize * 10;
	}
    }
}

/**
 * Return the list of modes supported by the EDID information
 * stored in 'output'
 */
DisplayModePtr
i830_xf86OutputGetEDIDModes (xf86OutputPtr output)
{
    ScrnInfoPtr	pScrn = output->scrn;
    xf86MonPtr	edid_mon = output->MonInfo;

    if (!edid_mon)
	return NULL;
    return xf86DDCGetModes(pScrn->scrnIndex, edid_mon);
}

xf86MonPtr
i830_xf86OutputGetEDID (xf86OutputPtr output, I2CBusPtr pDDCBus)
{
    ScrnInfoPtr	pScrn = output->scrn;

    return xf86DoEDID_DDC2 (pScrn->scrnIndex, pDDCBus);
}
