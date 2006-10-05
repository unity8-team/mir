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
    RRCrtcPtr			    crtcs[MAX_DISPLAY_PIPES];
    RROutputPtr			    outputs[MAX_OUTPUTS];
    DisplayModePtr  		    modes[MAX_DISPLAY_PIPES];
#endif
} XF86RandRInfoRec, *XF86RandRInfoPtr;
    
#ifdef RANDR_12_INTERFACE
static Bool I830RandRInit12 (ScreenPtr pScreen);
static Bool I830RandRCreateScreenResources12 (ScreenPtr pScreen);
#endif

static int	    i830RandRIndex;
static int	    i830RandRGeneration;

#define XF86RANDRINFO(p)    ((XF86RandRInfoPtr) (p)->devPrivates[i830RandRIndex].ptr)

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

    if (!I830RandRSetMode (pScreen, mode, useVirtual, pSize->mmWidth, pSize->mmHeight)) {
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
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
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
    int			i, j;
    DisplayModePtr	pipeMode = &pI830->pipeCurMode[pipe];
    int			pipe_type;
    
    x = pI830->pipeX[pipe];
    y = pI830->pipeY[pipe];
    rotation = RR_Rotate_0;
    numOutputs = 0;
    for (i = 0; i < pI830->num_outputs; i++)
    {
	output = &pI830->output[i];
	/*
	 * Valid crtcs
	 */
	switch (output->type) {
	case I830_OUTPUT_DVO:
	case I830_OUTPUT_SDVO:
	    pipe_type = PIPE_DFP;
	    break;
	case I830_OUTPUT_ANALOG:
	    pipe_type = PIPE_CRT;
	    break;
	case I830_OUTPUT_LVDS:
	    pipe_type = PIPE_LFP;
	    break;
	case I830_OUTPUT_TVOUT:
	    pipe_type = PIPE_TV;
	    break;
	default:
	    pipe_type = PIPE_NONE;
	    break;
	}
	if (pI830->operatingDevices & (pipe_type << (pipe << 3)))
	{
	    rrout = randrp->outputs[i];
	    outputs[numOutputs++] = rrout;
	    for (j = 0; j < rrout->numModes; j++)
	    {
		DisplayModePtr	outMode = rrout->modes[j]->devPrivate;
		if (I830ModesEqual(pipeMode, outMode))
		    mode = rrout->modes[j];
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
		  int		numOutputs,
		  RROutputConfigPtr	outputs)
{
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr		pI830 = I830PTR(pScrn);
    int			pipe = (int) (crtc->devPrivate);
    DisplayModePtr	display_mode = mode ? mode->devPrivate : NULL;
    
    /* Sync the engine before adjust mode */
    if (pI830->AccelInfoRec && pI830->AccelInfoRec->NeedToSync) {
	(*pI830->AccelInfoRec->Sync)(pScrn);
	pI830->AccelInfoRec->NeedToSync = FALSE;
    }

    if (display_mode != randrp->modes[pipe])
    {
	pI830->planeEnabled[pipe] = mode != NULL;
	if (display_mode)
	{
	    if (!i830PipeSetMode (pScrn, display_mode, pipe))
		return FALSE;
	    /* XXX need I830SDVOPostSetMode here */
	}
	else
	{
	    CARD32  operatingDevices = pI830->operatingDevices;

	    if (pipe == 0)
		pI830->operatingDevices &= ~0xff;
	    else
		pI830->operatingDevices &= ~0xff00;
	    i830DisableUnusedFunctions (pScrn);
	    pI830->operatingDevices = operatingDevices;
	}
	randrp->modes[pipe] = display_mode;
    }
    i830PipeSetBase(pScrn, pipe, x, y);
    return I830RandRCrtcNotify (crtc);
}

static Bool
I830RandRCrtcSetGamma (ScreenPtr    pScreen,
		       RRCrtcPtr    crtc)
{
    return FALSE;
}

/*
 * Mirror the current mode configuration to RandR
 */
static Bool
I830RandRSetInfo12 (ScreenPtr pScreen)
{
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr		pI830 = I830PTR(pScrn);
    RROutputPtr		clones[MAX_OUTPUTS];
    RRCrtcPtr		crtc;
    int			nclone;
    RRCrtcPtr		crtcs[MAX_DISPLAY_PIPES];
    int			ncrtc;
    int			nmode, npreferred;
    struct _I830OutputRec   *output;
    int			i;
    int			j;
    int			p;
    int			clone_types;
    int			crtc_types;
    int			connection;
    int			pipe_type;
    int			pipe;
    int			subpixel;
    DisplayModePtr	modes, mode;
    xRRModeInfo		modeInfo;
    RRModePtr		rrmode, *rrmodes;
    CARD32		possibleOptions = 0;
    CARD32		currentOptions = 0;
    
    if (randrp->virtualX == -1 || randrp->virtualY == -1) 
    {
	randrp->virtualX = pScrn->virtualX;
	randrp->virtualY = pScrn->virtualY;
    }
    RRScreenSetSizeRange (pScreen, 320, 240, 
			  randrp->virtualX, randrp->virtualY);
    for (i = 0; i < pI830->num_outputs; i++)
    {
	output = &pI830->output[i];
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
	    pipe_type = PIPE_DFP;
	    subpixel = SubPixelHorizontalRGB;
	    break;
	case I830_OUTPUT_ANALOG:
	    crtc_types = (1 << 0);
	    clone_types = ((1 << I830_OUTPUT_ANALOG) |
			   (1 << I830_OUTPUT_DVO) |
			   (1 << I830_OUTPUT_SDVO));
	    pipe_type = PIPE_CRT;
	    subpixel = SubPixelNone;
	    break;
	case I830_OUTPUT_LVDS:
	    crtc_types = (1 << 1);
	    clone_types = (1 << I830_OUTPUT_LVDS);
	    pipe_type = PIPE_LFP;
	    subpixel = SubPixelHorizontalRGB;
	    possibleOptions = (RROutputOptionScaleNone|
			       RROutputOptionScaleMaxAspect |
			       RROutputOptionScaleMax);
	    currentOptions = RROutputOptionScaleMax;
	    break;
	case I830_OUTPUT_TVOUT:
	    crtc_types = ((1 << 0) |
			  (1 << 1));
	    clone_types = (1 << I830_OUTPUT_TVOUT);
	    pipe_type = PIPE_TV;
	    subpixel = SubPixelNone;
	    break;
	default:
	    crtc_types = 0;
	    clone_types = 0;
	    pipe_type = PIPE_NONE;
	    subpixel = SubPixelUnknown;
	    break;
	}
	ncrtc = 0;
	pipe = -1;
	crtc = NULL;
	for (j = 0; j < MAX_DISPLAY_PIPES; j++)
	{
#if 0
	     /* Can't flip outputs among crtcs yet */
	    if (crtc_types & (1 << j))
		crtcs[ncrtc++] = randrp->crtcs[j];
#endif
	    if (pI830->operatingDevices & (pipe_type << (j << 3)))
	    {
		pipe = j;
		crtc = randrp->crtcs[j];
		crtcs[ncrtc++] = crtc;
	    }
	}
	if (!RROutputSetCrtcs (randrp->outputs[i], crtcs, ncrtc))
	    return FALSE;

	RROutputSetCrtc (randrp->outputs[i], crtc);
    
	RROutputSetPossibleOptions (randrp->outputs[i], possibleOptions);
	RROutputSetCurrentOptions (randrp->outputs[i], currentOptions);
        nmode = 0;
	npreferred = 0;
	rrmodes = NULL;
	if (pipe >= 0) 
	{
	    MonPtr  mon = pI830->pipeMon[pipe];
	    modes = mon->Modes;
	
	    for (mode = modes; mode; mode = mode->next)
		nmode++;

	    if (nmode)
	    {
		rrmodes = xalloc (nmode * sizeof (RRModePtr));
		if (!rrmodes)
		    return FALSE;
		nmode = 0;
		for (p = 1; p >= 0; p--)
		{
		    for (mode = modes; mode; mode = mode->next)
		    {
			if ((p != 0) == ((mode->type & M_T_PREFERRED) != 0))
			{
			    modeInfo.nameLength = strlen (mode->name);
			    modeInfo.mmWidth = mon->widthmm;
			    modeInfo.mmHeight = mon->heightmm;
	
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
	
			    rrmode = RRModeGet (pScreen, &modeInfo, mode->name);
			    rrmode->devPrivate = mode;
			    if (rrmode)
			    {
				rrmodes[nmode++] = rrmode;
				npreferred += p;
			    }
			}
		    }
		}
	    }
	}
	
    	if (!RROutputSetModes (randrp->outputs[i], rrmodes, nmode, npreferred))
	{
    	    xfree (rrmodes);
	    return FALSE;
	}
	
	xfree (rrmodes);
	
	connection = RR_Disconnected;
	if (pipe >= 0)
	    connection = RR_Connected;

	RROutputSetConnection (randrp->outputs[i], connection);
	    
	RROutputSetSubpixelOrder (randrp->outputs[i], subpixel);

	/*
	 * Valid clones
	 */
	nclone = 0;
	for (j = 0; j < pI830->num_outputs; j++)
	{
	    if (i != j && ((1 << pI830->output[j].type) & clone_types))
		clones[nclone++] = randrp->outputs[j];
	}
	if (!RROutputSetClones (randrp->outputs[i], clones, nclone))
	    return FALSE;
    }
    for (i = 0; i < MAX_DISPLAY_PIPES; i++)
	I830RandRCrtcNotify (randrp->crtcs[i]);
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

    I830ValidateXF86ModeList(pScrn, FALSE);
    return I830RandRSetInfo12 (pScreen);
}

static Bool
I830RandRCreateScreenResources12 (ScreenPtr pScreen)
{
    XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    I830Ptr		pI830 = I830PTR(pScrn);
    struct _I830OutputRec   *output;
    const char		*name;
    int			i;
    DisplayModePtr	mode;

    /*
     * Create RandR resources, then probe them
     */
    for (i = 0; i < MAX_DISPLAY_PIPES; i++)
    {
	randrp->crtcs[i] = RRCrtcCreate (pScreen, (void *) i);
	RRCrtcGammaSetSize (randrp->crtcs[i], 256);
    }

    for (i = 0; i < pI830->num_outputs; i++)
    {
	output = &pI830->output[i];
	name = i830_output_type_names[output->type];
	randrp->outputs[i] = RROutputCreate (pScreen,
					     name, strlen (name),
					     (void *) i);
    }
    
    mode = pScrn->currentMode;
    if (mode)
    {
	int mmWidth, mmHeight;

	if (mode->HDisplay == pScreen->width &&
	    mode->VDisplay == pScreen->height)
	{
	    mmWidth = pScrn->widthmm;
	    mmHeight = pScrn->heightmm;
	}
	else
	{
#define MMPERINCH 25.4
	    mmWidth = (double) mode->HDisplay / pScrn->xDpi * MMPERINCH;
	    mmHeight = (double) mode->VDisplay / pScrn->yDpi * MMPERINCH;
	}
	I830RandRScreenSetSize (pScreen,
				mode->HDisplay,
				mode->VDisplay,
				mmWidth,
				mmHeight);
    }
			    
    for (i = 0; i < MAX_DISPLAY_PIPES; i++)
	i830PipeSetBase(pScrn, i, 0, 0);
	
    return I830RandRSetInfo12 (pScreen);
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
    memset (rp->modes, '\0', sizeof (rp->modes));
    pScrn->PointerMoved = I830RandRPointerMoved;
    return TRUE;
}
#endif
