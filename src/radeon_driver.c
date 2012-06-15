/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *   Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
 * Credits:
 *
 *   Thanks to Ani Joshi <ajoshi@shell.unixbox.com> for providing source
 *   code to his Radeon driver.  Portions of this file are based on the
 *   initialization code for that driver.
 *
 * References:
 *
 * !!!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 * This server does not yet support these XFree86 4.0 features:
 * !!!! FIXME !!!!
 *   DDC1 & DDC2
 *   shadowfb
 *   overlay planes
 *
 * Modified by Marc Aurele La France (tsi@xfree86.org) for ATI driver merge.
 *
 * Mergedfb and pseudo xinerama support added by Alex Deucher (agd5f@yahoo.com)
 * based on the sis driver by Thomas Winischhofer.
 *
 */

#include <string.h>
#include <stdio.h>

				/* Driver data structures */
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_version.h"

#include "fb.h"

				/* colormap initialization */
#include "micmap.h"
#include "dixstruct.h"

				/* X and server generic header files */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86RandR12.h"
#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6
#include "xf86RAC.h"
#include "xf86Resources.h"
#endif
#include "xf86cmap.h"
#include "vbe.h"

#include "shadow.h"
				/* vgaHW definitions */
#ifdef HAVE_XEXTPROTO_71
#include <X11/extensions/dpmsconst.h>
#else
#define DPMS_SERVER
#include <X11/extensions/dpms.h>
#endif


#include "atipciids.h"

extern _X_EXPORT int gRADEONEntityIndex;

static int getRADEONEntityIndex(void)
{
    return gRADEONEntityIndex;
}

RADEONEntPtr RADEONEntPriv(ScrnInfoPtr pScrn)
{
    DevUnion     *pPriv;
    RADEONInfoPtr  info   = RADEONPTR(pScrn);
    pPriv = xf86GetEntityPrivate(info->pEnt->index,
                                 getRADEONEntityIndex());
    return pPriv->ptr;
}

/* Allocate our private RADEONInfoRec */
Bool RADEONGetRec(ScrnInfoPtr pScrn)
{
    if (pScrn->driverPrivate) return TRUE;

    pScrn->driverPrivate = xnfcalloc(sizeof(RADEONInfoRec), 1);
    return TRUE;
}

/* Free our private RADEONInfoRec */
void RADEONFreeRec(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info;

    if (!pScrn || !pScrn->driverPrivate) return;

    info = RADEONPTR(pScrn);

    if (info->accel_state) {
	free(info->accel_state);
	info->accel_state = NULL;
    }

    free(pScrn->driverPrivate);
    pScrn->driverPrivate = NULL;
}

/* This is called by RADEONPreInit to set up the default visual */
Bool RADEONPreInitVisual(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

    if (!xf86SetDepthBpp(pScrn, 0, 0, 0, Support32bppFb))
	return FALSE;

    switch (pScrn->depth) {
    case 8:
    case 15:
    case 16:
    case 24:
	break;

    default:
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Given depth (%d) is not supported by %s driver\n",
		   pScrn->depth, RADEON_DRIVER_NAME);
	return FALSE;
    }

    xf86PrintDepthBpp(pScrn);

    info->pix24bpp                   = xf86GetBppFromDepth(pScrn,
							   pScrn->depth);
    info->CurrentLayout.bitsPerPixel = pScrn->bitsPerPixel;
    info->CurrentLayout.depth        = pScrn->depth;
    info->CurrentLayout.pixel_bytes  = pScrn->bitsPerPixel / 8;
    info->CurrentLayout.pixel_code   = (pScrn->bitsPerPixel != 16
				       ? pScrn->bitsPerPixel
				       : pScrn->depth);

    if (info->pix24bpp == 24) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Radeon does NOT support 24bpp\n");
	return FALSE;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Pixel depth = %d bits stored in %d byte%s (%d bpp pixmaps)\n",
	       pScrn->depth,
	       info->CurrentLayout.pixel_bytes,
	       info->CurrentLayout.pixel_bytes > 1 ? "s" : "",
	       info->pix24bpp);

    if (!xf86SetDefaultVisual(pScrn, -1)) return FALSE;

    if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Default visual (%s) is not supported at depth %d\n",
		   xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
	return FALSE;
    }
    return TRUE;
}

/* This is called by RADEONPreInit to handle all color weight issues */
Bool RADEONPreInitWeight(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info = RADEONPTR(pScrn);

				/* Save flag for 6 bit DAC to use for
				   setting CRTC registers.  Otherwise use
				   an 8 bit DAC, even if xf86SetWeight sets
				   pScrn->rgbBits to some value other than
				   8. */
    info->dac6bits = FALSE;

    if (pScrn->depth > 8) {
	rgb  defaultWeight = { 0, 0, 0 };

	if (!xf86SetWeight(pScrn, defaultWeight, defaultWeight)) return FALSE;
    } else {
	pScrn->rgbBits = 8;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
	       "Using %d bits per RGB (%d bit DAC)\n",
	       pScrn->rgbBits, info->dac6bits ? 6 : 8);

    return TRUE;
}

Bool
RADEONZaphodStringMatches(ScrnInfoPtr pScrn, const char *s, char *output_name)
{
    int i = 0;
    char s1[20];

    do {
	switch(*s) {
	case ',':
  	    s1[i] = '\0';
	    i = 0;
	    if (strcmp(s1, output_name) == 0)
		return TRUE;
	    break;
	case ' ':
	case '\t':
	case '\n':
	case '\r':
	    break;
	default:
	    s1[i] = *s;
	    i++;
	    break;
	}
    } while(*s++);

    s1[i] = '\0';
    if (strcmp(s1, output_name) == 0)
	return TRUE;

    return FALSE;
}

#ifdef X_XF86MiscPassMessage
Bool RADEONHandleMessage(int scrnIndex, const char* msgtype,
                                   const char* msgval, char** retmsg)
{
    ErrorF("RADEONHandleMessage(%d, \"%s\", \"%s\", retmsg)\n", scrnIndex,
		    msgtype, msgval);
    *retmsg = "";
    return 0;
}
#endif

#ifndef HAVE_XF86MODEBANDWIDTH
/** Calculates the memory bandwidth (in MiB/sec) of a mode. */
_X_HIDDEN unsigned int
xf86ModeBandwidth(DisplayModePtr mode, int depth)
{
    float a_active, a_total, active_percent, pixels_per_second;
    int bytes_per_pixel = (depth + 7) / 8;

    if (!mode->HTotal || !mode->VTotal || !mode->Clock)
	return 0;

    a_active = mode->HDisplay * mode->VDisplay;
    a_total = mode->HTotal * mode->VTotal;
    active_percent = a_active / a_total;
    pixels_per_second = active_percent * mode->Clock * 1000.0;

    return (unsigned int)(pixels_per_second * bytes_per_pixel / (1024 * 1024));
}
#endif

