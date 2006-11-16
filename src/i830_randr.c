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

#include "i830.h"
#include "i830_xf86Modes.h"
#include "i830_display.h"

typedef struct _i830RandRInfo {
    int				    virtualX;
    int				    virtualY;
    int				    mmWidth;
    int				    mmHeight;
    int				    maxX;
    int				    maxY;
    Rotation			    rotation; /* current mode */
    Rotation                        supported_rotations; /* driver supported */
#ifdef RANDR_12_INTERFACE
    DisplayModePtr  		    modes[MAX_DISPLAY_PIPES];
#endif
} XF86RandRInfoRec, *XF86RandRInfoPtr;

#ifdef RANDR_12_INTERFACE
static Bool I830RandRInit12 (ScreenPtr pScreen);
static Bool I830RandRCreateScreenResources12 (ScreenPtr pScreen);
#endif

static int	    i830RandRIndex;
static int	    i830RandRGeneration;

#define XF86RANDRINFO(p) \
	((XF86RandRInfoPtr)(p)->devPrivates[i830RandRIndex].ptr)

static int
I830RandRModeRefresh (DisplayModePtr mode)
{
    if (mode->VRefresh)
	return (int) (mode->VRefresh + 0.5);
    else
	return (int) (mode->Clock * 1000.0 / mode->HTotal / mode->VTotal + 0.5);
}

static Bool
I830RandRGetInfo (ScreenPtr pScreen, Rotation *rotations)
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
	int refresh = I830RandRModeRefresh (mode);
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

	if (I830ModesEqual(mode, scrp->currentMode) &&
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
I830RandRSetMode (ScreenPtr	    pScreen,
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
I830RandRSetConfig (ScreenPtr		pScreen,
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
	    (rate == 0 || I830RandRModeRefresh (mode) == rate))
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

    if (!I830RandRSetMode (pScreen, mode, useVirtual, pSize->mmWidth,
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
I830GetRotation(ScreenPtr pScreen)
{
    XF86RandRInfoPtr	    randrp = XF86RANDRINFO(pScreen);

    return randrp->rotation;
}

Bool
I830RandRCreateScreenResources (ScreenPtr pScreen)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr pI830 = I830PTR(pScrn);
#ifdef PANORAMIX
    /* XXX disable RandR when using Xinerama */
    if (!noPanoramiXExtension)
	return TRUE;
#endif
#if RANDR_12_INTERFACE
    if (I830RandRCreateScreenResources12 (pScreen))
	return TRUE;
#endif
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
	I830RandRSetConfig (pScreen, requestedRotation, 0, &p);
	pI830->starting = FALSE;
    }
    return TRUE;
}


Bool
I830RandRInit (ScreenPtr    pScreen, int rotation)
{
    rrScrPrivPtr	rp;
    XF86RandRInfoPtr	randrp;

#ifdef PANORAMIX
    /* XXX disable RandR when using Xinerama */
    if (!noPanoramiXExtension)
	return TRUE;
#endif
    if (i830RandRGeneration != serverGeneration)
    {
	i830RandRIndex = AllocateScreenPrivateIndex();
	i830RandRGeneration = serverGeneration;
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
    rp->rrGetInfo = I830RandRGetInfo;
    rp->rrSetConfig = I830RandRSetConfig;

    randrp->virtualX = -1;
    randrp->virtualY = -1;
    randrp->mmWidth = pScreen->mmWidth;
    randrp->mmHeight = pScreen->mmHeight;

    randrp->rotation = RR_Rotate_0; /* initial rotated mode */

    randrp->supported_rotations = rotation;

    randrp->maxX = randrp->maxY = 0;

    pScreen->devPrivates[i830RandRIndex].ptr = randrp;

#if RANDR_12_INTERFACE
    if (!I830RandRInit12 (pScreen))
	return FALSE;
#endif
    return TRUE;
}

void
I830GetOriginalVirtualSize(ScrnInfoPtr pScrn, int *x, int *y)
{
    ScreenPtr pScreen = screenInfo.screens[pScrn->scrnIndex];

    if (i830RandRGeneration != serverGeneration ||
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
I830RandRScreenSetSize (ScreenPtr	pScreen,
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
I830RandRCrtcNotify (RRCrtcPtr	crtc)
{
    ScreenPtr		pScreen = crtc->pScreen;
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr		pI830 = I830PTR(pScrn);
    RRModePtr		mode = NULL;
    int			x;
    int			y;
    Rotation		rotation;
    int			numOutputs;
    RROutputPtr		outputs[MAX_OUTPUTS];
    struct _I830OutputRec   *output;
    RROutputPtr		rrout;
    int			pipe = (int) crtc->devPrivate;
    I830PipePtr		pI830Pipe = &pI830->pipes[pipe];
    int			i, j;
    DisplayModePtr	pipeMode = &pI830Pipe->curMode;

    x = pI830Pipe->x;
    y = pI830Pipe->y;
    rotation = RR_Rotate_0;
    numOutputs = 0;
    mode = NULL;
    for (i = 0; i < pI830->num_outputs; i++)
    {
	output = &pI830->output[i];
	if (output->enabled && output->pipe == pipe)
	{
	    rrout = output->randr_output;
	    outputs[numOutputs++] = rrout;
	    /*
	     * We make copies of modes, so pointer equality 
	     * isn't sufficient
	     */
	    for (j = 0; j < rrout->numModes; j++)
	    {
		DisplayModePtr	outMode = rrout->modes[j]->devPrivate;
		if (I830ModesEqual(pipeMode, outMode))
		{
		    mode = rrout->modes[j];
		    break;
		}
	    }
	}
    }
    return RRCrtcNotify (crtc, mode, x, y, rotation, numOutputs, outputs);
}

static Bool
I830RandRCrtcSet (ScreenPtr	pScreen,
		  RRCrtcPtr	crtc,
		  RRModePtr	mode,
		  int		x,
		  int		y,
		  Rotation	rotation,
		  int		num_randr_outputs,
		  RROutputPtr	*randr_outputs)
{
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr		pI830 = I830PTR(pScrn);
    int			pipe = (int) (crtc->devPrivate);
    I830PipePtr		pI830Pipe = &pI830->pipes[pipe];
    DisplayModePtr	display_mode = mode ? mode->devPrivate : NULL;
    Bool		changed = FALSE;
    Bool		disable = FALSE;
    int			o, ro;
    struct {
	int pipe;
	int enabled;
    }			save_output[MAX_OUTPUTS];
    Bool		save_enabled = pI830Pipe->enabled;

    if (display_mode != randrp->modes[pipe])
    {
	changed = TRUE;
	if (!display_mode)
	    disable = TRUE;
    }
    
    for (o = 0; o < pI830->num_outputs; o++) 
    {
	I830OutputPtr	output = &pI830->output[o];
	RROutputPtr	randr_output = NULL;
	
	save_output[o].enabled = output->enabled;
	save_output[o].pipe = output->pipe;
	for (ro = 0; ro < num_randr_outputs; ro++) 
	{
	    if (output->randr_output == randr_outputs[ro])
	    {
		randr_output = randr_outputs[ro];
		break;
	    }
	}
	if (randr_output)
	{
	    if (output->pipe != pipe || !output->enabled)
	    {
		output->pipe = pipe;
		output->enabled = TRUE;
		changed = TRUE;
	    }
	}
	else
	{
	    /* Disable outputs which were on this pipe */
	    if (output->enabled && output->pipe == pipe)
	    {
		output->enabled = FALSE;
		changed = TRUE;
		disable = TRUE;
	    }
	}
    }
    if (changed)
    {
	pI830Pipe->enabled = mode != NULL;
	/* Sync the engine before adjust mode */
	if (pI830->AccelInfoRec && pI830->AccelInfoRec->NeedToSync) {
	    (*pI830->AccelInfoRec->Sync)(pScrn);
	    pI830->AccelInfoRec->NeedToSync = FALSE;
	}

	if (display_mode)
	{
	    if (!i830PipeSetMode (pScrn, display_mode, pipe, TRUE))
	    {
		pI830Pipe->enabled = save_enabled;
		for (o = 0; o < pI830->num_outputs; o++)
		{
		    I830OutputPtr	output = &pI830->output[o];
		    output->enabled = save_output[o].enabled;
		    output->pipe = save_output[o].pipe;
		}
		return FALSE;
	    }
	    i830PipeSetBase(pScrn, pipe, x, y);
	}
	randrp->modes[pipe] = display_mode;
	if (disable)
	    i830DisableUnusedFunctions (pScrn);
    }
    return I830RandRCrtcNotify (crtc);
}

static Bool
I830RandRCrtcSetGamma (ScreenPtr    pScreen,
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
I830RandRSetInfo12 (ScrnInfoPtr pScrn)
{
    I830Ptr		    pI830 = I830PTR(pScrn);
    RROutputPtr		    clones[MAX_OUTPUTS];
    RRCrtcPtr		    crtcs[MAX_DISPLAY_PIPES];
    int			    ncrtc;
    I830OutputPtr	    output;
    int			    o, c, p;
    int			    clone_types;
    int			    crtc_types;
    int			    subpixel;
    RRCrtcPtr		    randr_crtc;
    RROutputPtr		    randr_output;
    int			    nclone;
    
    for (o = 0; o < pI830->num_outputs; o++)
    {
	output = &pI830->output[o];
	randr_output = output->randr_output;
	/*
	 * Valid crtcs
	 */
	switch (output->type) {
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
	for (p = 0; p < pI830->num_pipes; p++)
	    if (crtc_types & (1 << p))
		crtcs[ncrtc++] = pI830->pipes[p].randr_crtc;

	if (output->enabled)
	    randr_crtc = pI830->pipes[output->pipe].randr_crtc;
	else
	    randr_crtc = NULL;

	if (!RROutputSetCrtcs (output->randr_output, crtcs, ncrtc))
	    return FALSE;

	RROutputSetCrtc (output->randr_output, randr_crtc);
	RROutputSetPhysicalSize(output->randr_output, 
				output->mm_width,
				output->mm_height);
	I830xf86RROutputSetModes (output->randr_output, output->probed_modes);

	switch (output->detect(pScrn, output)) {
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
	for (c = 0; c < pI830->num_outputs; c++)
	{
	    if (o != c && ((1 << pI830->output[c].type) & clone_types))
		clones[nclone++] = pI830->output[c].randr_output;
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
I830RandRGetInfo12 (ScreenPtr pScreen, Rotation *rotations)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];

    i830_reprobe_output_modes(pScrn);
    return I830RandRSetInfo12 (pScrn);
}

static Bool
I830RandRCreateObjects12 (ScrnInfoPtr pScrn)
{
    I830Ptr		pI830 = I830PTR(pScrn);
    int			p;
    int			o;
    
    if (!RRInit ())
	return FALSE;

    /*
     * Create RandR resources, then probe them
     */
    for (p = 0; p < pI830->num_pipes; p++)
    {
	I830PipePtr pipe = &pI830->pipes[p];
	RRCrtcPtr   randr_crtc = RRCrtcCreate ((void *) p);
	
	if (!randr_crtc)
	    return FALSE;
	RRCrtcGammaSetSize (randr_crtc, 256);
	pipe->randr_crtc = randr_crtc;
    }

    for (o = 0; o < pI830->num_outputs; o++)
    {
	I830OutputPtr	output = &pI830->output[o];
	const char	*name = i830_output_type_names[output->type];
	RROutputPtr	randr_output = RROutputCreate (name, strlen (name),
						       (void *) o);
	if (!randr_output)
	    return FALSE;
	output->randr_output = randr_output;
    }
    /*
     * Configure output modes
     */
    if (!I830RandRSetInfo12 (pScrn))
	return FALSE;
    return TRUE;
}

static Bool
I830RandRCreateScreenResources12 (ScreenPtr pScreen)
{
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr		pI830 = I830PTR(pScrn);
    int			p, o;
    DisplayModePtr	mode;

    /*
     * Attach RandR objects to screen
     */
    for (p = 0; p < pI830->num_pipes; p++)
	if (!RRCrtcAttachScreen (pI830->pipes[p].randr_crtc, pScreen))
	    return FALSE;

    for (o = 0; o < pI830->num_outputs; o++)
	if (!RROutputAttachScreen (pI830->output[o].randr_output, pScreen))
	    return FALSE;

    mode = pScrn->currentMode;
    if (mode)
    {
	int mmWidth, mmHeight;

	mmWidth = pScreen->mmWidth;
	mmHeight = pScreen->mmHeight;
	if (mode->HDisplay != pScreen->width)
	    mmWidth = mmWidth * mode->HDisplay / pScreen->width;
	if (mode->VDisplay != pScreen->height)
	    mmHeight = mmHeight * mode->VDisplay / pScreen->height;
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Setting screen physical size to %d x %d\n",
		   mmWidth, mmHeight);
	I830RandRScreenSetSize (pScreen,
				mode->HDisplay,
				mode->VDisplay,
				mmWidth,
				mmHeight);
    }

    for (p = 0; p < pI830->num_pipes; p++)
    {
	i830PipeSetBase(pScrn, p, 0, 0);
	I830RandRCrtcNotify (pI830->pipes[p].randr_crtc);
    }

    
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
I830RandRPointerMoved (int scrnIndex, int x, int y)
{
}

static Bool
I830RandRInit12 (ScreenPtr pScreen)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    rrScrPrivPtr	rp = rrGetScrPriv(pScreen);

    rp->rrGetInfo = I830RandRGetInfo12;
    rp->rrScreenSetSize = I830RandRScreenSetSize;
    rp->rrCrtcSet = I830RandRCrtcSet;
    rp->rrCrtcSetGamma = I830RandRCrtcSetGamma;
    rp->rrSetConfig = NULL;
    pScrn->PointerMoved = I830RandRPointerMoved;
    return TRUE;
}
#endif

Bool
I830RandRPreInit (ScrnInfoPtr pScrn)
{
    int n;
    
    n = I830ValidateXF86ModeList(pScrn, TRUE);
    if (n <= 0)
	return FALSE;
#if RANDR_12_INTERFACE
    if (!I830RandRCreateObjects12 (pScrn))
	return FALSE;
#endif
    return TRUE;
}
