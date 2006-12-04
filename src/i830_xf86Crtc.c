/*
 * $Id: $
 *
 * Copyright © 2006 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "xf86.h"
#include "i830_xf86Crtc.h"

/*
 * Crtc functions
 */
xf86CrtcPtr
xf86CrtcCreate (ScrnInfoPtr		scrn,
		const xf86CrtcFuncsRec	*funcs)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
    xf86CrtcPtr		crtc;

    crtc = xcalloc (sizeof (xf86CrtcRec), 1);
    if (!crtc)
	return NULL;
    crtc->scrn = scrn;
    crtc->funcs = funcs;
#ifdef RANDR_12_INTERFACE
    crtc->randr_crtc = RRCrtcCreate (crtc);
    if (!crtc->randr_crtc)
    {
	xfree (crtc);
	return NULL;
    }
#endif
    xf86_config->crtc[xf86_config->num_crtc++] = crtc;
    return crtc;
}

void
xf86CrtcDestroy (xf86CrtcPtr crtc)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(crtc->scrn);
    int			c;
    
    (*crtc->funcs->destroy) (crtc);
#ifdef RANDR_12_INTERFACE
    if (crtc->randr_crtc)
	RRCrtcDestroy (crtc->randr_crtc);
#endif
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
xf86OutputPtr
xf86OutputCreate (ScrnInfoPtr		    scrn,
		  const xf86OutputFuncsRec *funcs,
		  const char		    *name)
{
    xf86OutputPtr	output;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
    int			len = strlen (name);

    output = xcalloc (sizeof (xf86OutputRec) + len + 1, 1);
    if (!output)
	return NULL;
    output->scrn = scrn;
    output->funcs = funcs;
    output->name = (char *) (output + 1);
    strcpy (output->name, name);
#ifdef RANDR_12_INTERFACE
    output->randr_output = RROutputCreate (name, strlen (name), output);
    if (!output->randr_output)
    {
	xfree (output);
	return NULL;
    }
#endif
    xf86_config->output[xf86_config->num_output++] = output;
    return output;
}

void
xf86OutputDestroy (xf86OutputPtr output)
{
    ScrnInfoPtr		scrn = output->scrn;
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(scrn);
    int			o;
    
    (*output->funcs->destroy) (output);
#ifdef RANDR_12_INTERFACE
    if (output->randr_output)
	RROutputDestroy (output->randr_output);
#endif
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
    xfree (output);
}

Bool
xf86CrtcScreenInit (ScreenPtr pScreen)
{
#ifdef RANDR_12_INTERFACE
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int			i;

    for (i = 0; i < xf86_config->num_crtc; i++)
    {
	xf86CrtcPtr crtc = xf86_config->crtc[i];

	if (!crtc->randr_crtc)
	    crtc->randr_crtc = RRCrtcCreate (crtc);
	if (!crtc->randr_crtc)
	    return FALSE;
    }
    for (i = 0; i < xf86_config->num_output; i++)
    {
	xf86OutputPtr output = xf86_config->output[i];
	
	if (!output->randr_output)
	    output->randr_output = RROutputCreate (output->name,
						   strlen (output->name),
						   output);
	if (!output->randr_output)
	    return FALSE;
    }
#endif
    return TRUE;
}

void
xf86CrtcCloseScreen (ScreenPtr pScreen)
{
#ifdef RANDR_12_INTERFACE
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int			i;

    for (i = 0; i < xf86_config->num_crtc; i++)
    {
	xf86CrtcPtr crtc = xf86_config->crtc[i];
	crtc->randr_crtc = NULL;
    }
    for (i = 0; i < xf86_config->num_output; i++)
    {
	xf86OutputPtr output = xf86_config->output[i];
	output->randr_output = NULL;
    }
#endif
}
