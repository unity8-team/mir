/* $XdotOrg: xc/programs/Xserver/hw/xfree86/common/xf86RandR.c,v 1.3 2004/07/30 21:53:09 eich Exp $ */
/*
 * $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86RandR.c,v 1.7tsi Exp $
 *
 * Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
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

#include "xf86.h"
#include "os.h"
#include "mibank.h"
#include "globals.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86DDC.h"
#include "mipointer.h"
#include "windowstr.h"
#include <randrstr.h>
#include <X11/extensions/render.h>

#include "i830_xf86Crtc.h"
#include "i830_randr.h"
#include "i830_display.h"
#include "i830.h"

typedef struct _xf86RandR12Info {
    int				    virtualX;
    int				    virtualY;
    int				    mmWidth;
    int				    mmHeight;
    int				    maxX;
    int				    maxY;
    Rotation			    rotation; /* current mode */
    Rotation                        supported_rotations; /* driver supported */
} XF86RandRInfoRec, *XF86RandRInfoPtr;

#ifdef RANDR_12_INTERFACE
static Bool xf86RandR12Init12 (ScreenPtr pScreen);
static Bool xf86RandR12CreateScreenResources12 (ScreenPtr pScreen);
#endif

static int	    xf86RandR12Index;
static int	    xf86RandR12Generation;

#define XF86RANDRINFO(p) \
	((XF86RandRInfoPtr)(p)->devPrivates[xf86RandR12Index].ptr)

static int
xf86RandR12ModeRefresh (DisplayModePtr mode)
{
    if (mode->VRefresh)
	return (int) (mode->VRefresh + 0.5);
    else
	return (int) (mode->Clock * 1000.0 / mode->HTotal / mode->VTotal + 0.5);
}

static Bool
xf86RandR12GetInfo (ScreenPtr pScreen, Rotation *rotations)
{
    RRScreenSizePtr	    pSize;
    ScrnInfoPtr		    scrp = XF86SCRNINFO(pScreen);
    XF86RandRInfoPtr	    randrp = XF86RANDRINFO(pScreen);
    DisplayModePtr	    mode;
    int			    refresh0 = 60;
    int			    maxX = 0, maxY = 0;

    *rotations = randrp->supported_rotations;

    if (randrp->virtualX == -1 || randrp->virtualY == -1)
    {
	randrp->virtualX = scrp->virtualX;
	randrp->virtualY = scrp->virtualY;
    }

    /* Re-probe the outputs for new monitors or modes */
    I830ValidateXF86ModeList(scrp, FALSE);

    for (mode = scrp->modes; ; mode = mode->next)
    {
	int refresh = xf86RandR12ModeRefresh (mode);
	if (randrp->maxX == 0 || randrp->maxY == 0)
	{
		if (maxX < mode->HDisplay)
			maxX = mode->HDisplay;
		if (maxY < mode->VDisplay)
			maxY = mode->VDisplay;
	}
	if (mode == scrp->modes)
	    refresh0 = refresh;
	pSize = RRRegisterSize (pScreen,
				mode->HDisplay, mode->VDisplay,
				randrp->mmWidth, randrp->mmHeight);
	if (!pSize)
	    return FALSE;
	RRRegisterRate (pScreen, pSize, refresh);

	if (xf86ModesEqual(mode, scrp->currentMode) &&
	    mode->HDisplay == scrp->virtualX &&
	    mode->VDisplay == scrp->virtualY)
	{
	    RRSetCurrentConfig (pScreen, randrp->rotation, refresh, pSize);
	}
	if (mode->next == scrp->modes)
	    break;
    }

    if (randrp->maxX == 0 || randrp->maxY == 0)
    {
	randrp->maxX = maxX;
	randrp->maxY = maxY;
    }

    if (scrp->currentMode->HDisplay != randrp->virtualX ||
	scrp->currentMode->VDisplay != randrp->virtualY)
    {
	pSize = RRRegisterSize (pScreen,
				randrp->virtualX, randrp->virtualY,
				randrp->mmWidth,
				randrp->mmHeight);
	if (!pSize)
	    return FALSE;
	RRRegisterRate (pScreen, pSize, refresh0);
	if (scrp->virtualX == randrp->virtualX &&
	    scrp->virtualY == randrp->virtualY)
	{
	    RRSetCurrentConfig (pScreen, randrp->rotation, refresh0, pSize);
	}
    }

    return TRUE;
}

static Bool
xf86RandR12SetMode (ScreenPtr	    pScreen,
		  DisplayModePtr    mode,
		  Bool		    useVirtual,
		  int		    mmWidth,
		  int		    mmHeight)
{
    ScrnInfoPtr		scrp = XF86SCRNINFO(pScreen);
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
    int			oldWidth = pScreen->width;
    int			oldHeight = pScreen->height;
    int			oldmmWidth = pScreen->mmWidth;
    int			oldmmHeight = pScreen->mmHeight;
    WindowPtr		pRoot = WindowTable[pScreen->myNum];
    DisplayModePtr      currentMode = NULL;
    Bool 		ret = TRUE;
    PixmapPtr 		pspix = NULL;

    if (pRoot)
	(*scrp->EnableDisableFBAccess) (pScreen->myNum, FALSE);
    if (useVirtual)
    {
	scrp->virtualX = randrp->virtualX;
	scrp->virtualY = randrp->virtualY;
    }
    else
    {
	scrp->virtualX = mode->HDisplay;
	scrp->virtualY = mode->VDisplay;
    }

    if(randrp->rotation & (RR_Rotate_90 | RR_Rotate_270))
    {
	/* If the screen is rotated 90 or 270 degrees, swap the sizes. */
	pScreen->width = scrp->virtualY;
	pScreen->height = scrp->virtualX;
	pScreen->mmWidth = mmHeight;
	pScreen->mmHeight = mmWidth;
    }
    else
    {
	pScreen->width = scrp->virtualX;
	pScreen->height = scrp->virtualY;
	pScreen->mmWidth = mmWidth;
	pScreen->mmHeight = mmHeight;
    }
    if (scrp->currentMode == mode) {
        /* Save current mode */
        currentMode = scrp->currentMode;
        /* Reset, just so we ensure the drivers SwitchMode is called */
        scrp->currentMode = NULL;
    }
    /*
     * We know that if the driver failed to SwitchMode to the rotated
     * version, then it should revert back to it's prior mode.
     */
    if (!xf86SwitchMode (pScreen, mode))
    {
        ret = FALSE;
	scrp->virtualX = pScreen->width = oldWidth;
	scrp->virtualY = pScreen->height = oldHeight;
	pScreen->mmWidth = oldmmWidth;
	pScreen->mmHeight = oldmmHeight;
        scrp->currentMode = currentMode;
    }
    /*
     * Get the new Screen pixmap ptr as SwitchMode might have called
     * ModifyPixmapHeader and xf86EnableDisableFBAccess will put it back...
     * Unfortunately.
     */
    pspix = (*pScreen->GetScreenPixmap) (pScreen);
    if (pspix->devPrivate.ptr)
       scrp->pixmapPrivate = pspix->devPrivate;

    /*
     * Make sure the layout is correct
     */
    xf86ReconfigureLayout();

    /*
     * Make sure the whole screen is visible
     */
    xf86SetViewport (pScreen, pScreen->width, pScreen->height);
    xf86SetViewport (pScreen, 0, 0);
    if (pRoot)
	(*scrp->EnableDisableFBAccess) (pScreen->myNum, TRUE);
    return ret;
}

Bool
xf86RandR12SetConfig (ScreenPtr		pScreen,
		    Rotation		rotation,
		    int			rate,
		    RRScreenSizePtr	pSize)
{
    ScrnInfoPtr		    scrp = XF86SCRNINFO(pScreen);
    XF86RandRInfoPtr	    randrp = XF86RANDRINFO(pScreen);
    DisplayModePtr	    mode;
    int			    px, py;
    Bool		    useVirtual = FALSE;
    int			    maxX = 0, maxY = 0;
    Rotation		    oldRotation = randrp->rotation;

    randrp->rotation = rotation;

    if (randrp->virtualX == -1 || randrp->virtualY == -1)
    {
	randrp->virtualX = scrp->virtualX;
	randrp->virtualY = scrp->virtualY;
    }

    miPointerPosition (&px, &py);
    for (mode = scrp->modes; ; mode = mode->next)
    {
	if (randrp->maxX == 0 || randrp->maxY == 0)
	{
		if (maxX < mode->HDisplay)
			maxX = mode->HDisplay;
		if (maxY < mode->VDisplay)
			maxY = mode->VDisplay;
	}
	if (mode->HDisplay == pSize->width &&
	    mode->VDisplay == pSize->height &&
	    (rate == 0 || xf86RandR12ModeRefresh (mode) == rate))
	    break;
	if (mode->next == scrp->modes)
	{
	    if (pSize->width == randrp->virtualX &&
		pSize->height == randrp->virtualY)
	    {
		mode = scrp->modes;
		useVirtual = TRUE;
		break;
	    }
    	    if (randrp->maxX == 0 || randrp->maxY == 0)
    	    {
		randrp->maxX = maxX;
		randrp->maxY = maxY;
    	    }
	    return FALSE;
	}
    }

    if (randrp->maxX == 0 || randrp->maxY == 0)
    {
	randrp->maxX = maxX;
	randrp->maxY = maxY;
    }

    if (!xf86RandR12SetMode (pScreen, mode, useVirtual, pSize->mmWidth,
			   pSize->mmHeight)) {
        randrp->rotation = oldRotation;
	return FALSE;
    }

    /*
     * Move the cursor back where it belongs; SwitchMode repositions it
     */
    if (pScreen == miPointerCurrentScreen ())
    {
        px = (px >= pScreen->width ? (pScreen->width - 1) : px);
        py = (py >= pScreen->height ? (pScreen->height - 1) : py);

	xf86SetViewport(pScreen, px, py);

	(*pScreen->SetCursorPosition) (pScreen, px, py, FALSE);
    }

    return TRUE;
}

Rotation
xf86RandR12GetRotation(ScreenPtr pScreen)
{
    XF86RandRInfoPtr	    randrp = XF86RANDRINFO(pScreen);

    return randrp->rotation;
}

Bool
xf86RandR12CreateScreenResources (ScreenPtr pScreen)
{
#if 0
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr		pI830 = I830PTR(pScrn);
#endif
#ifdef PANORAMIX
    /* XXX disable RandR when using Xinerama */
    if (!noPanoramiXExtension)
	return TRUE;
#endif
#if RANDR_12_INTERFACE
    if (xf86RandR12CreateScreenResources12 (pScreen))
	return TRUE;
#endif
#if 0
    /* XXX deal with initial rotation */
    if (pI830->rotation != RR_Rotate_0) {
	RRScreenSize p;
	Rotation requestedRotation = pI830->rotation;

	pI830->rotation = RR_Rotate_0;

	/* Just setup enough for an initial rotate */
	p.width = pScreen->width;
	p.height = pScreen->height;
	p.mmWidth = pScreen->mmWidth;
	p.mmHeight = pScreen->mmHeight;

	pI830->starting = TRUE; /* abuse this for dual head & rotation */
	xf86RandR12SetConfig (pScreen, requestedRotation, 0, &p);
	pI830->starting = FALSE;
    }
#endif
    return TRUE;
}


Bool
xf86RandR12Init (ScreenPtr pScreen)
{
    rrScrPrivPtr	rp;
    XF86RandRInfoPtr	randrp;

#ifdef PANORAMIX
    /* XXX disable RandR when using Xinerama */
    if (!noPanoramiXExtension)
	return TRUE;
#endif
    if (xf86RandR12Generation != serverGeneration)
    {
	xf86RandR12Index = AllocateScreenPrivateIndex();
	xf86RandR12Generation = serverGeneration;
    }

    randrp = xalloc (sizeof (XF86RandRInfoRec));
    if (!randrp)
	return FALSE;

    if (!RRScreenInit(pScreen))
    {
	xfree (randrp);
	return FALSE;
    }
    rp = rrGetScrPriv(pScreen);
    rp->rrGetInfo = xf86RandR12GetInfo;
    rp->rrSetConfig = xf86RandR12SetConfig;

    randrp->virtualX = -1;
    randrp->virtualY = -1;
    randrp->mmWidth = pScreen->mmWidth;
    randrp->mmHeight = pScreen->mmHeight;

    randrp->rotation = RR_Rotate_0; /* initial rotated mode */

    randrp->supported_rotations = RR_Rotate_0;

    randrp->maxX = randrp->maxY = 0;

    pScreen->devPrivates[xf86RandR12Index].ptr = randrp;

#if RANDR_12_INTERFACE
    if (!xf86RandR12Init12 (pScreen))
	return FALSE;
#endif
    return TRUE;
}

void
xf86RandR12SetRotations (ScreenPtr pScreen, Rotation rotations)
{
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);

    randrp->supported_rotations = rotations;
}

void
xf86RandR12GetOriginalVirtualSize(ScrnInfoPtr pScrn, int *x, int *y)
{
    ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];

    if (xf86RandR12Generation != serverGeneration ||
	XF86RANDRINFO(pScreen)->virtualX == -1)
    {
	*x = pScrn->virtualX;
	*y = pScrn->virtualY;
    } else {
	XF86RandRInfoPtr randrp = XF86RANDRINFO(pScreen);

	*x = randrp->virtualX;
	*y = randrp->virtualY;
    }
}

#if RANDR_12_INTERFACE
static Bool
xf86RandR12ScreenSetSize (ScreenPtr	pScreen,
			CARD16		width,
			CARD16		height,
			CARD32		mmWidth,
			CARD32		mmHeight)
{
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
    ScrnInfoPtr		pScrn = XF86SCRNINFO(pScreen);
    WindowPtr		pRoot = WindowTable[pScreen->myNum];
    Bool 		ret = TRUE;

    if (randrp->virtualX == -1 || randrp->virtualY == -1)
    {
	randrp->virtualX = pScrn->virtualX;
	randrp->virtualY = pScrn->virtualY;
    }
    if (pRoot)
	(*pScrn->EnableDisableFBAccess) (pScreen->myNum, FALSE);
    pScrn->virtualX = width;
    pScrn->virtualY = height;

    pScreen->width = pScrn->virtualX;
    pScreen->height = pScrn->virtualY;
    pScreen->mmWidth = mmWidth;
    pScreen->mmHeight = mmHeight;

    xf86SetViewport (pScreen, pScreen->width, pScreen->height);
    xf86SetViewport (pScreen, 0, 0);
    if (pRoot)
	(*pScrn->EnableDisableFBAccess) (pScreen->myNum, TRUE);
    if (WindowTable[pScreen->myNum])
	RRScreenSizeNotify (pScreen);
    return ret;
}

static Bool
xf86RandR12CrtcNotify (RRCrtcPtr	randr_crtc)
{
    ScreenPtr		pScreen = randr_crtc->pScreen;
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr		pI830 = I830PTR(pScrn);
    RRModePtr		randr_mode = NULL;
    int			x;
    int			y;
    Rotation		rotation;
    int			numOutputs;
    RROutputPtr		randr_outputs[MAX_OUTPUTS];
    RROutputPtr		randr_output;
    xf86CrtcPtr	crtc = randr_crtc->devPrivate;
    xf86OutputPtr	output;
    int			i, j;
    DisplayModePtr	curMode = &crtc->curMode;

    x = crtc->x;
    y = crtc->y;
    rotation = RR_Rotate_0;
    numOutputs = 0;
    randr_mode = NULL;
    for (i = 0; i < pI830->xf86_config.num_output; i++)
    {
	output = pI830->xf86_config.output[i];
	if (output->crtc == crtc)
	{
	    randr_output = output->randr_output;
	    randr_outputs[numOutputs++] = randr_output;
	    /*
	     * We make copies of modes, so pointer equality 
	     * isn't sufficient
	     */
	    for (j = 0; j < randr_output->numModes; j++)
	    {
		DisplayModePtr	outMode = randr_output->modes[j]->devPrivate;
		if (xf86ModesEqual(curMode, outMode))
		{
		    randr_mode = randr_output->modes[j];
		    break;
		}
	    }
	}
    }
    return RRCrtcNotify (randr_crtc, randr_mode, x, y,
			 rotation, numOutputs, randr_outputs);
}

static Bool
xf86RandR12CrtcSet (ScreenPtr	pScreen,
		  RRCrtcPtr	randr_crtc,
		  RRModePtr	randr_mode,
		  int		x,
		  int		y,
		  Rotation	rotation,
		  int		num_randr_outputs,
		  RROutputPtr	*randr_outputs)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr		pI830 = I830PTR(pScrn);
    xf86CrtcPtr	crtc = randr_crtc->devPrivate;
    DisplayModePtr	mode = randr_mode ? randr_mode->devPrivate : NULL;
    Bool		changed = FALSE;
    int			o, ro;
    xf86CrtcPtr	save_crtcs[MAX_OUTPUTS];
    Bool		save_enabled = crtc->enabled;

    if ((mode != NULL) != crtc->enabled)
	changed = TRUE;
    else if (mode && !xf86ModesEqual (&crtc->curMode, mode))
	changed = TRUE;
    
    for (o = 0; o < pI830->xf86_config.num_output; o++) 
    {
	xf86OutputPtr  output = pI830->xf86_config.output[o];
	xf86CrtcPtr    new_crtc;

	save_crtcs[o] = output->crtc;
	
	if (output->crtc == crtc)
	    new_crtc = NULL;
	else
	    new_crtc = output->crtc;
	for (ro = 0; ro < num_randr_outputs; ro++) 
	    if (output->randr_output == randr_outputs[ro])
	    {
		new_crtc = crtc;
		break;
	    }
	if (new_crtc != output->crtc)
	{
	    changed = TRUE;
	    output->crtc = new_crtc;
	}
    }
    if (changed)
    {
	crtc->enabled = mode != NULL;
	
	/* Sync the engine before adjust mode */
	if (pI830->AccelInfoRec && pI830->AccelInfoRec->NeedToSync) {
	    (*pI830->AccelInfoRec->Sync)(pScrn);
	    pI830->AccelInfoRec->NeedToSync = FALSE;
	}

	if (mode)
	{
	    if (!i830PipeSetMode (crtc, mode, TRUE))
	    {
		crtc->enabled = save_enabled;
		for (o = 0; o < pI830->xf86_config.num_output; o++)
		{
		    xf86OutputPtr	output = pI830->xf86_config.output[o];
		    output->crtc = save_crtcs[o];
		}
		return FALSE;
	    }
	    crtc->desiredMode = *mode;
	    i830PipeSetBase(crtc, x, y);
	}
	i830DisableUnusedFunctions (pScrn);
    }
    return xf86RandR12CrtcNotify (randr_crtc);
}

static Bool
xf86RandR12CrtcSetGamma (ScreenPtr    pScreen,
		       RRCrtcPtr    crtc)
{
    return FALSE;
}

/**
 * Given a list of xf86 modes and a RandR Output object, construct
 * RandR modes and assign them to the output
 */
static Bool
I830xf86RROutputSetModes (RROutputPtr randr_output, DisplayModePtr modes)
{
    DisplayModePtr  mode;
    RRModePtr	    *rrmodes = NULL;
    int		    nmode = 0;
    int		    npreferred = 0;
    Bool	    ret = TRUE;
    int		    pref;

    for (mode = modes; mode; mode = mode->next)
	nmode++;

    if (nmode) {
	rrmodes = xalloc (nmode * sizeof (RRModePtr));
	
	if (!rrmodes)
	    return FALSE;
	nmode = 0;

	for (pref = 1; pref >= 0; pref--) {
	    for (mode = modes; mode; mode = mode->next) {
		if ((pref != 0) == ((mode->type & M_T_PREFERRED) != 0)) {
		    xRRModeInfo		modeInfo;
		    RRModePtr		rrmode;
		    
		    modeInfo.nameLength = strlen (mode->name);
		    modeInfo.width = mode->HDisplay;
		    modeInfo.dotClock = mode->Clock * 1000;
		    modeInfo.hSyncStart = mode->HSyncStart;
		    modeInfo.hSyncEnd = mode->HSyncEnd;
		    modeInfo.hTotal = mode->HTotal;
		    modeInfo.hSkew = mode->HSkew;

		    modeInfo.height = mode->VDisplay;
		    modeInfo.vSyncStart = mode->VSyncStart;
		    modeInfo.vSyncEnd = mode->VSyncEnd;
		    modeInfo.vTotal = mode->VTotal;
		    modeInfo.modeFlags = mode->Flags;

		    rrmode = RRModeGet (&modeInfo, mode->name);
		    rrmode->devPrivate = mode;
		    if (rrmode) {
			rrmodes[nmode++] = rrmode;
			npreferred += pref;
		    }
		}
	    }
	}
    }
    
    ret = RROutputSetModes (randr_output, rrmodes, nmode, npreferred);
    xfree (rrmodes);
    return ret;
}

/*
 * Mirror the current mode configuration to RandR
 */
static Bool
xf86RandR12SetInfo12 (ScrnInfoPtr pScrn)
{
    I830Ptr		    pI830 = I830PTR(pScrn);
    RROutputPtr		    clones[MAX_OUTPUTS];
    RRCrtcPtr		    crtcs[MAX_DISPLAY_PIPES];
    int			    ncrtc;
    int			    o, c, p;
    int			    clone_types;
    int			    crtc_types;
    int			    subpixel;
    RRCrtcPtr		    randr_crtc;
    int			    nclone;
    
    for (o = 0; o < pI830->xf86_config.num_output; o++)
    {
	xf86OutputPtr	output = pI830->xf86_config.output[o];
	I830OutputPrivatePtr	intel_output = output->driver_private;
	/*
	 * Valid crtcs
	 */
	switch (intel_output->type) {
	case I830_OUTPUT_DVO:
	case I830_OUTPUT_SDVO:
	    crtc_types = ((1 << 0)|
			  (1 << 1));
	    clone_types = ((1 << I830_OUTPUT_ANALOG) |
			   (1 << I830_OUTPUT_DVO) |
			   (1 << I830_OUTPUT_SDVO));
	    subpixel = SubPixelHorizontalRGB;
	    break;
	case I830_OUTPUT_ANALOG:
	    crtc_types = ((1 << 0) | (1 << 1));
	    clone_types = ((1 << I830_OUTPUT_ANALOG) |
			   (1 << I830_OUTPUT_DVO) |
			   (1 << I830_OUTPUT_SDVO));
	    subpixel = SubPixelNone;
	    break;
	case I830_OUTPUT_LVDS:
	    crtc_types = (1 << 1);
	    clone_types = (1 << I830_OUTPUT_LVDS);
	    subpixel = SubPixelHorizontalRGB;
	    break;
	case I830_OUTPUT_TVOUT:
	    crtc_types = ((1 << 0) |
			  (1 << 1));
	    clone_types = (1 << I830_OUTPUT_TVOUT);
	    subpixel = SubPixelNone;
	    break;
	default:
	    crtc_types = 0;
	    clone_types = 0;
	    subpixel = SubPixelUnknown;
	    break;
	}
	ncrtc = 0;
	for (p = 0; p < pI830->xf86_config.num_crtc; p++)
	    if (crtc_types & (1 << p))
		crtcs[ncrtc++] = pI830->xf86_config.crtc[p]->randr_crtc;

	if (output->crtc)
	    randr_crtc = output->crtc->randr_crtc;
	else
	    randr_crtc = NULL;

	if (!RROutputSetCrtcs (output->randr_output, crtcs, ncrtc))
	    return FALSE;

	RROutputSetCrtc (output->randr_output, randr_crtc);
	RROutputSetPhysicalSize(output->randr_output, 
				output->mm_width,
				output->mm_height);
	I830xf86RROutputSetModes (output->randr_output, output->probed_modes);

	switch ((*output->funcs->detect)(output)) {
	case OUTPUT_STATUS_CONNECTED:
	    RROutputSetConnection (output->randr_output, RR_Connected);
	    break;
	case OUTPUT_STATUS_DISCONNECTED:
	    RROutputSetConnection (output->randr_output, RR_Disconnected);
	    break;
	case OUTPUT_STATUS_UNKNOWN:
	    RROutputSetConnection (output->randr_output, RR_UnknownConnection);
	    break;
	}

	RROutputSetSubpixelOrder (output->randr_output, subpixel);

	/*
	 * Valid clones
	 */
	nclone = 0;
	for (c = 0; c < pI830->xf86_config.num_output; c++)
	{
	    xf86OutputPtr	    clone = pI830->xf86_config.output[c];
	    I830OutputPrivatePtr    intel_clone = clone->driver_private;
	    
	    if (o != c && ((1 << intel_clone->type) & clone_types))
		clones[nclone++] = clone->randr_output;
	}
	if (!RROutputSetClones (output->randr_output, clones, nclone))
	    return FALSE;
    }
    return TRUE;
}

/*
 * Query the hardware for the current state, then mirror
 * that to RandR
 */
static Bool
xf86RandR12GetInfo12 (ScreenPtr pScreen, Rotation *rotations)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];

    i830_reprobe_output_modes(pScrn);
    return xf86RandR12SetInfo12 (pScrn);
}

static Bool
xf86RandR12CreateObjects12 (ScrnInfoPtr pScrn)
{
    I830Ptr		pI830 = I830PTR(pScrn);
    int			p;
    
    if (!RRInit ())
	return FALSE;

    /*
     * Configure crtcs
     */
    for (p = 0; p < pI830->xf86_config.num_crtc; p++)
    {
	xf86CrtcPtr    crtc = pI830->xf86_config.crtc[p];
	
	RRCrtcGammaSetSize (crtc->randr_crtc, 256);
    }

    return TRUE;
}

static Bool
xf86RandR12CreateScreenResources12 (ScreenPtr pScreen)
{
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr		pI830 = I830PTR(pScrn);
    int			p, o;
    int			width, height;

    /*
     * Attach RandR objects to screen
     */
    for (p = 0; p < pI830->xf86_config.num_crtc; p++)
	if (!RRCrtcAttachScreen (pI830->xf86_config.crtc[p]->randr_crtc, pScreen))
	    return FALSE;

    for (o = 0; o < pI830->xf86_config.num_output; o++)
	if (!RROutputAttachScreen (pI830->xf86_config.output[o]->randr_output, pScreen))
	    return FALSE;

    /*
     * Compute width of screen
     */
    width = 0; height = 0;
    for (p = 0; p < pI830->xf86_config.num_crtc; p++)
    {
	xf86CrtcPtr    crtc = pI830->xf86_config.crtc[p];
	int		    crtc_width = crtc->x + crtc->curMode.HDisplay;
	int		    crtc_height = crtc->y + crtc->curMode.VDisplay;
	
	if (crtc->enabled && crtc_width > width)
	    width = crtc_width;
	if (crtc->enabled && crtc_height > height)
	    height = crtc_height;
    }
    
    if (width && height)
    {
	int mmWidth, mmHeight;

	mmWidth = pScreen->mmWidth;
	mmHeight = pScreen->mmHeight;
	if (width != pScreen->width)
	    mmWidth = mmWidth * width / pScreen->width;
	if (height != pScreen->height)
	    mmHeight = mmHeight * height / pScreen->height;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Setting screen physical size to %d x %d\n",
		   mmWidth, mmHeight);
	xf86RandR12ScreenSetSize (pScreen,
				width,
				height,
				mmWidth,
				mmHeight);
    }

    for (p = 0; p < pI830->xf86_config.num_crtc; p++)
	xf86RandR12CrtcNotify (pI830->xf86_config.crtc[p]->randr_crtc);
    
    if (randrp->virtualX == -1 || randrp->virtualY == -1)
    {
	randrp->virtualX = pScrn->virtualX;
	randrp->virtualY = pScrn->virtualY;
    }
    
    RRScreenSetSizeRange (pScreen, 320, 240,
			  randrp->virtualX, randrp->virtualY);
    return TRUE;
}

static void
xf86RandR12PointerMoved (int scrnIndex, int x, int y)
{
}

static Bool
xf86RandR12Init12 (ScreenPtr pScreen)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    rrScrPrivPtr	rp = rrGetScrPriv(pScreen);

    rp->rrGetInfo = xf86RandR12GetInfo12;
    rp->rrScreenSetSize = xf86RandR12ScreenSetSize;
    rp->rrCrtcSet = xf86RandR12CrtcSet;
    rp->rrCrtcSetGamma = xf86RandR12CrtcSetGamma;
    rp->rrSetConfig = NULL;
    pScrn->PointerMoved = xf86RandR12PointerMoved;
    return TRUE;
}

static RRModePtr
I830RRDefaultMode (RROutputPtr output)
{
    RRModePtr   target_mode = NULL;
    int		target_diff = 0;
    int		mmHeight;
    int		num_modes;
    int		m;
    
    num_modes = output->numPreferred ? output->numPreferred : output->numModes;
    mmHeight = output->mmHeight;
    if (!mmHeight)
	mmHeight = 203;	/* 768 pixels at 96dpi */
    /*
     * Pick a mode closest to 96dpi 
     */
    for (m = 0; m < num_modes; m++)
    {
	RRModePtr   mode = output->modes[m];
	int	    dpi;
	int	    diff;

	dpi = (mode->mode.height * 254) / (mmHeight * 10);
	diff = dpi - 96;
	diff = diff < 0 ? -diff : diff;
	if (target_mode == NULL || diff < target_diff)
	{
	    target_mode = mode;
	    target_diff = diff;
	}
    }
    return target_mode;
}

static RRModePtr
I830ClosestMode (RROutputPtr output, RRModePtr match)
{
    RRModePtr   target_mode = NULL;
    int		target_diff = 0;
    int		m;
    
    /*
     * Pick a mode closest to the specified mode
     */
    for (m = 0; m < output->numModes; m++)
    {
	RRModePtr   mode = output->modes[m];
	int	    dx, dy;
	int	    diff;

	/* exact matches are preferred */
	if (mode == match)
	    return mode;
	
	dx = match->mode.width - mode->mode.width;
	dy = match->mode.height - mode->mode.height;
	diff = dx * dx + dy * dy;
	if (target_mode == NULL || diff < target_diff)
	{
	    target_mode = mode;
	    target_diff = diff;
	}
    }
    return target_mode;
}

static int
I830RRPickCrtcs (RROutputPtr	*outputs,
		 RRCrtcPtr	*best_crtcs,
		 RRModePtr	*modes,
		 int		num_output,
		 int		n)
{
    int		c, o, l;
    RROutputPtr	output;
    RRCrtcPtr	crtc;
    RRCrtcPtr	*crtcs;
    RRCrtcPtr	best_crtc;
    int		best_score;
    int		score;
    int		my_score;
    
    if (n == num_output)
	return 0;
    output = outputs[n];
    
    /*
     * Compute score with this output disabled
     */
    best_crtcs[n] = NULL;
    best_crtc = NULL;
    best_score = I830RRPickCrtcs (outputs, best_crtcs, modes, num_output, n+1);
    if (modes[n] == NULL)
	return best_score;
    
    crtcs = xalloc (num_output * sizeof (RRCrtcPtr));
    if (!crtcs)
	return best_score;

    my_score = 1;
    /* Score outputs that are known to be connected higher */
    if (output->connection == RR_Connected)
	my_score++;
    /* Score outputs with preferred modes higher */
    if (output->numPreferred)
	my_score++;
    /*
     * Select a crtc for this output and
     * then attempt to configure the remaining
     * outputs
     */
    for (c = 0; c < output->numCrtcs; c++)
    {
	crtc = output->crtcs[c];
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
	    if (modes[o] == modes[n])
	    {
		for (l = 0; l < output->numClones; l++)
		    if (output->clones[l] == outputs[o])
			break;
		if (l == output->numClones)
		    continue;		/* nope, try next CRTC */
	    }
	    else
		continue;		/* different modes, can't clone */
	}
	crtcs[n] = crtc;
	memcpy (crtcs, best_crtcs, n * sizeof (RRCrtcPtr));
	score = my_score + I830RRPickCrtcs (outputs, crtcs, modes,
					    num_output, n+1);
	if (score >= best_score)
	{
	    best_crtc = crtc;
	    best_score = score;
	    memcpy (best_crtcs, crtcs, num_output * sizeof (RRCrtcPtr));
	}
    }
    xfree (crtcs);
    return best_score;
}

static Bool
I830RRInitialConfiguration (RROutputPtr *outputs,
			    RRCrtcPtr	*crtcs,
			    RRModePtr	*modes,
			    int		num_output)
{
    int		o;
    RRModePtr	target_mode = NULL;

    for (o = 0; o < num_output; o++)
	modes[o] = NULL;
    
    /*
     * Let outputs with preferred modes drive screen size
     */
    for (o = 0; o < num_output; o++)
    {
	RROutputPtr output = outputs[o];

	if (output->connection != RR_Disconnected && output->numPreferred)
	{
	    target_mode = I830RRDefaultMode (output);
	    if (target_mode)
	    {
		modes[o] = target_mode;
		break;
	    }
	}
    }
    if (!target_mode)
    {
	for (o = 0; o < num_output; o++)
	{
	    RROutputPtr output = outputs[o];
	    if (output->connection != RR_Disconnected)
	    {
		target_mode = I830RRDefaultMode (output);
		if (target_mode)
		{
		    modes[o] = target_mode;
		    break;
		}
	    }
	}
    }
    for (o = 0; o < num_output; o++)
    {
	RROutputPtr output = outputs[o];
	
	if (output->connection != RR_Disconnected && !modes[o])
	    modes[o] = I830ClosestMode (output, target_mode);
    }

    if (!I830RRPickCrtcs (outputs, crtcs, modes, num_output, 0))
	return FALSE;
    
    return TRUE;
}

/*
 * Compute the virtual size necessary to place all of the available
 * crtcs in a panorama configuration
 */

static void
I830RRDefaultScreenLimits (RROutputPtr *outputs, int num_output,
			   RRCrtcPtr *crtcs, int num_crtc,
			   int *widthp, int *heightp)
{
    int	    width = 0, height = 0;
    int	    o;
    int	    c;
    int	    m;
    int	    s;

    for (c = 0; c < num_crtc; c++)
    {
	RRCrtcPtr   crtc = crtcs[c];
	int	    crtc_width = 1600, crtc_height = 1200;

	for (o = 0; o < num_output; o++) 
	{
	    RROutputPtr	output = outputs[o];

	    for (s = 0; s < output->numCrtcs; s++)
		if (output->crtcs[s] == crtc)
		{
		    for (m = 0; m < output->numModes; m++)
		    {
			RRModePtr   mode = output->modes[m];
			if (mode->mode.width > crtc_width)
			    crtc_width = mode->mode.width;
			if (mode->mode.height > crtc_width)
			    crtc_height = mode->mode.height;
		    }
		}
	}
	if (crtc_width > width)
	    width = crtc_width;
	if (crtc_height > height)
	    height = crtc_height;
    }
    *widthp = width;
    *heightp = height;
}

#endif

Bool
xf86RandR12PreInit (ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
#if RANDR_12_INTERFACE
    RROutputPtr	outputs[MAX_OUTPUTS];
    RRCrtcPtr	output_crtcs[MAX_OUTPUTS];
    RRModePtr	output_modes[MAX_OUTPUTS];
    RRCrtcPtr	crtcs[MAX_DISPLAY_PIPES];
    int		width, height;
    int		o;
    int		c;
#endif
    
    if (pI830->xf86_config.num_output <= 0)
	return FALSE;
    
    i830_reprobe_output_modes(pScrn);

#if RANDR_12_INTERFACE
    if (!xf86RandR12CreateObjects12 (pScrn))
	return FALSE;

    /*
     * Configure output modes
     */
    if (!xf86RandR12SetInfo12 (pScrn))
	return FALSE;
    /*
     * With RandR info set up, let RandR choose
     * the initial configuration
     */
    for (o = 0; o < pI830->xf86_config.num_output; o++)
	outputs[o] = pI830->xf86_config.output[o]->randr_output;
    for (c = 0; c < pI830->xf86_config.num_crtc; c++)
	crtcs[c] = pI830->xf86_config.crtc[c]->randr_crtc;
    
    if (!I830RRInitialConfiguration (outputs, output_crtcs, output_modes,
				     pI830->xf86_config.num_output))
	return FALSE;
    
    I830RRDefaultScreenLimits (outputs, pI830->xf86_config.num_output, 
			       crtcs, pI830->xf86_config.num_crtc,
			       &width, &height);
    
    if (width > pScrn->virtualX)
	pScrn->virtualX = width;
    if (width > pScrn->display->virtualX)
	pScrn->display->virtualX = width;
    if (height > pScrn->virtualY)
	pScrn->virtualY = height;
    if (height > pScrn->display->virtualY)
	pScrn->display->virtualY = height;
    
    /* XXX override xf86 common frame computation code */
    pScrn->display->frameX0 = 0;
    pScrn->display->frameY0 = 0;
    for (o = 0; o < pI830->xf86_config.num_output; o++)
    {
	xf86OutputPtr  output = pI830->xf86_config.output[o];
	RRModePtr	    randr_mode = output_modes[o];
        RRCrtcPtr	    randr_crtc = output_crtcs[o];
	DisplayModePtr	    mode;

	if (randr_mode && randr_crtc)
	{
	    xf86CrtcPtr    crtc = randr_crtc->devPrivate;
	    
	    mode = (DisplayModePtr) randr_mode->devPrivate;
	    crtc->desiredMode = *mode;
	    output->crtc = crtc;
	}
    }
#endif
    i830_set_xf86_modes_from_outputs (pScrn);
    
    i830_set_default_screen_size(pScrn);

    return TRUE;
}
