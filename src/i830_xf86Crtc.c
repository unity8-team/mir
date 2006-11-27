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
#include "i830.h"
#include "i830_xf86Modes.h"
#include "i830_xf86Crtc.h"

/*
 * Crtc functions
 */
I830_xf86CrtcPtr
i830xf86CrtcCreate (ScrnInfoPtr			scrn,
		    const I830_xf86CrtcFuncsRec	*funcs)
{
    I830_xf86CrtcPtr	xf86_crtc;

    xf86_crtc = xcalloc (sizeof (I830_xf86CrtcRec), 1);
    if (!xf86_crtc)
	return NULL;
    xf86_crtc->scrn = scrn;
    xf86_crtc->funcs = funcs;
#ifdef RANDR_12_INTERFACE
    xf86_crtc->randr_crtc = RRCrtcCreate (xf86_crtc);
    if (!xf86_crtc->randr_crtc)
    {
	xfree (xf86_crtc);
	return NULL;
    }
#endif
    return xf86_crtc;
}

void
i830xf86CrtcDestroy (I830_xf86CrtcPtr xf86_crtc)
{
#ifdef RANDR_12_INTERFACE
    RRCrtcDestroy (xf86_crtc->randr_crtc);
#endif
    xfree (xf86_crtc);
}

/*
 * Output functions
 */
I830_xf86OutputPtr
i830xf86OutputCreate (ScrnInfoPtr		    scrn,
		      const I830_xf86OutputFuncsRec *funcs,
		      const char		    *name)
{
    I830_xf86OutputPtr	output;
    I830Ptr		pI830 = I830PTR(scrn);
    int			len = strlen (name);

    output = xcalloc (sizeof (I830_xf86OutputRec) + len + 1, 1);
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
    pI830->xf86_output[pI830->num_outputs++] = output;
    return output;
}

void
i830xf86OutputDestroy (I830_xf86OutputPtr output)
{
    ScrnInfoPtr	scrn = output->scrn;
    I830Ptr	pI830 = I830PTR(scrn);
    int		o;
    
    (*output->funcs->destroy) (output);
#ifdef RANDR_12_INTERFACE
    RROutputDestroy (output->randr_output);
#endif
    while (output->probed_modes)
	xf86DeleteMode (&output->probed_modes, output->probed_modes);
    for (o = 0; o < pI830->num_outputs; o++)
	if (pI830->xf86_output[o] == output)
	{
	    memmove (&pI830->xf86_output[o],
		     &pI830->xf86_output[o+1],
		     pI830->num_outputs - (o + 1));
	    pI830->num_outputs--;
	    break;
	}
    xfree (output);
}

