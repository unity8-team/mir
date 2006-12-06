/* $XdotOrg: xc/programs/Xserver/hw/xfree86/common/xf86RandR.c,v 1.3 2004/07/30 21:53:09 eich Exp $ */
/*
 * $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86RandR.c,v 1.7tsi Exp $
 *
 * Copyright � 2002 Keith Packard, member of The XFree86 Project, Inc.
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
#include "i830_debug.h"
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
    xf86ProbeOutputModes (scrp);
    xf86SetScrnInfoModes (scrp);

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
    ScrnInfoPtr		scrp = XF86SCRNINFO(pScreen);
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
    DisplayModePtr	mode;
    int			px, py;
    Bool		useVirtual = FALSE;
    int			maxX = 0, maxY = 0;
    Rotation		oldRotation = randrp->rotation;

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
    xf86CrtcConfigPtr   config = XF86_CRTC_CONFIG_PTR(pScrn);
    RRModePtr		randr_mode = NULL;
    int			x;
    int			y;
    Rotation		rotation;
    int			numOutputs;
    RROutputPtr		randr_outputs[XF86_MAX_OUTPUT];
    RROutputPtr		randr_output;
    xf86CrtcPtr		crtc = randr_crtc->devPrivate;
    xf86OutputPtr	output;
    int			i, j;
    DisplayModePtr	curMode = &crtc->curMode;

    x = crtc->x;
    y = crtc->y;
    rotation = RR_Rotate_0;
    numOutputs = 0;
    randr_mode = NULL;
    for (i = 0; i < config->num_output; i++)
    {
	output = config->output[i];
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
    xf86CrtcConfigPtr   config = XF86_CRTC_CONFIG_PTR(pScrn);
    xf86CrtcPtr		crtc = randr_crtc->devPrivate;
    DisplayModePtr	mode = randr_mode ? randr_mode->devPrivate : NULL;
    Bool		changed = FALSE;
    Bool		pos_changed;
    int			o, ro;
    xf86CrtcPtr		save_crtcs[XF86_MAX_OUTPUT];
    Bool		save_enabled = crtc->enabled;

    if ((mode != NULL) != crtc->enabled)
	changed = TRUE;
    else if (mode && !xf86ModesEqual (&crtc->curMode, mode))
	changed = TRUE;
    
    pos_changed = changed;
    if (x != crtc->x || y != crtc->y)
	pos_changed = TRUE;
    for (o = 0; o < config->num_output; o++) 
    {
	xf86OutputPtr  output = config->output[o];
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
    /* XXX need device-independent mode setting code through an API */
    if (changed)
    {
	I830Ptr pI830 = I830PTR(pScrn);
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
		for (o = 0; o < config->num_output; o++)
		{
		    xf86OutputPtr	output = config->output[o];
		    output->crtc = save_crtcs[o];
		}
		return FALSE;
	    }
	    crtc->desiredMode = *mode;
	}
	i830DisableUnusedFunctions (pScrn);

	i830DumpRegs(pScrn);
    }
    if (pos_changed && mode)
	i830PipeSetBase(crtc, x, y);
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
xf86RROutputSetModes (RROutputPtr randr_output, DisplayModePtr modes)
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
		    if (rrmode) {
			rrmode->devPrivate = mode;
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
xf86RandR12SetInfo12 (ScreenPtr pScreen)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    xf86CrtcConfigPtr   config = XF86_CRTC_CONFIG_PTR(pScrn);
    RROutputPtr		clones[XF86_MAX_OUTPUT];
    RRCrtcPtr		crtcs[XF86_MAX_CRTC];
    int			ncrtc;
    int			o, c, l;
    RRCrtcPtr		randr_crtc;
    int			nclone;
    
    for (o = 0; o < config->num_output; o++)
    {
	xf86OutputPtr	output = config->output[o];
	
	ncrtc = 0;
	for (c = 0; c < config->num_crtc; c++)
	    if (output->possible_crtcs & (1 << c))
		crtcs[ncrtc++] = config->crtc[c]->randr_crtc;

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
	xf86RROutputSetModes (output->randr_output, output->probed_modes);

	switch (output->status = (*output->funcs->detect)(output)) {
	case XF86OutputStatusConnected:
	    RROutputSetConnection (output->randr_output, RR_Connected);
	    break;
	case XF86OutputStatusDisconnected:
	    RROutputSetConnection (output->randr_output, RR_Disconnected);
	    break;
	case XF86OutputStatusUnknown:
	    RROutputSetConnection (output->randr_output, RR_UnknownConnection);
	    break;
	}

	RROutputSetSubpixelOrder (output->randr_output, output->subpixel_order);

	/*
	 * Valid clones
	 */
	nclone = 0;
	for (l = 0; l < config->num_output; l++)
	{
	    xf86OutputPtr	    clone = config->output[l];
	    
	    if (l != o && (output->possible_clones & (1 << l)))
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

    xf86ProbeOutputModes (pScrn);
    xf86SetScrnInfoModes (pScrn);
    return xf86RandR12SetInfo12 (pScreen);
}

static Bool
xf86RandR12CreateObjects12 (ScreenPtr pScreen)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    xf86CrtcConfigPtr   config = XF86_CRTC_CONFIG_PTR(pScrn);
    int			c;
    int			o;
    
    if (!RRInit ())
	return FALSE;

    /*
     * Configure crtcs
     */
    for (c = 0; c < config->num_crtc; c++)
    {
	xf86CrtcPtr    crtc = config->crtc[c];
	
	crtc->randr_crtc = RRCrtcCreate (crtc);
	RRCrtcAttachScreen (crtc->randr_crtc, pScreen);
	RRCrtcGammaSetSize (crtc->randr_crtc, 256);
    }
    /*
     * Configure outputs
     */
    for (o = 0; o < config->num_output; o++)
    {
	xf86OutputPtr	output = config->output[o];

	output->randr_output = RROutputCreate (output->name, 
					       strlen (output->name),
					       output);
	RROutputAttachScreen (output->randr_output, pScreen);
    }
    return TRUE;
}

static Bool
xf86RandR12CreateScreenResources12 (ScreenPtr pScreen)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    xf86CrtcConfigPtr   config = XF86_CRTC_CONFIG_PTR(pScrn);
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
    int			c;
    int			width, height;

    /*
     * Compute width of screen
     */
    width = 0; height = 0;
    for (c = 0; c < config->num_crtc; c++)
    {
	xf86CrtcPtr crtc = config->crtc[c];
	int	    crtc_width = crtc->x + crtc->curMode.HDisplay;
	int	    crtc_height = crtc->y + crtc->curMode.VDisplay;
	
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

    for (c = 0; c < config->num_crtc; c++)
	xf86RandR12CrtcNotify (config->crtc[c]->randr_crtc);
    
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
    if (!xf86RandR12CreateObjects12 (pScreen))
	return FALSE;

    /*
     * Configure output modes
     */
    if (!xf86RandR12SetInfo12 (pScreen))
	return FALSE;
    return TRUE;
}

#endif

Bool
xf86RandR12PreInit (ScrnInfoPtr pScrn)
{
    return TRUE;
}
