/*
 * Copyright 2006 Dave Airlie
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
 * NON-INFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>

/* X and server generic header files */
#include "xf86.h"
#include "xf86_OSproc.h"
#include "fbdevhw.h"
#include "vgaHW.h"

#include "randrstr.h"

/* Driver data structures */
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_version.h"
#include "radeon_mergedfb.h"

typedef struct _radeonRandRInfo {
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

 
static int	    RADEONRandRIndex;
static int	    RADEONRandRGeneration;

#define XF86RANDRINFO(p)    ((XF86RandRInfoPtr) (p)->devPrivates[RADEONRandRIndex].ptr)

#if RANDR_12_INTERFACE
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


static void
RADEONRandRPointerMoved (int scrnIndex, int x, int y)
{
}

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
    RROutputPtr		*randr_outputs;
    RROutputPtr		randr_output;
    xf86CrtcPtr		crtc = randr_crtc->devPrivate;
    xf86OutputPtr	output;
    int			i, j;
    DisplayModePtr	curMode = &crtc->curMode;
    Bool		ret;

    randr_outputs = ALLOCATE_LOCAL(config->num_output * sizeof (RROutputPtr));
    if (!randr_outputs)
	return FALSE;
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
    ret = RRCrtcNotify (randr_crtc, randr_mode, x, y,
			rotation, numOutputs, randr_outputs);
    DEALLOCATE_LOCAL(randr_outputs);
    return ret;
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
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  xf86CrtcConfigPtr   config = XF86_CRTC_CONFIG_PTR(pScrn);
  xf86CrtcPtr crtc = randr_crtc->devPrivate;
  DisplayModePtr mode = randr_mode ? randr_mode->devPrivate : NULL;
  Bool changed = FALSE;
  Bool pos_changed;
  int o, ro;
  xf86CrtcPtr		*save_crtcs;
  Bool save_enabled = crtc->enabled;
  int ret;

  save_crtcs = ALLOCATE_LOCAL(config->num_crtc * sizeof (xf86CrtcPtr));
  if ((mode != NULL) != crtc->enabled)
    changed = TRUE;
  else if (mode && !xf86ModesEqual (&crtc->curMode, mode))
    changed = TRUE;

  pos_changed = changed;
  if (x != crtc->x || y != crtc->y)
    pos_changed = TRUE;

  for (o = 0; o < config->num_output; o++) {
    xf86OutputPtr  output = config->output[o];
    xf86CrtcPtr new_crtc;

    save_crtcs[o] = output->crtc;

    if (output->crtc == crtc)
      new_crtc = NULL;
    else
      new_crtc = output->crtc;

    for (ro = 0; ro < num_randr_outputs; ro++)
      if (output->randr_output == randr_outputs[ro]) {
	new_crtc = crtc;
	break;
      }
    if (new_crtc != output->crtc) {
      changed = TRUE;
      output->crtc = new_crtc;
    }
  }

  /* got to set the modes in here */
  if (changed) {
    RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
    RADEONInfoPtr  info = RADEONPTR(pScrn);
    RADEONCrtcPrivatePtr pRcrtc;
    crtc->enabled = mode != NULL;

    if (info->accelOn)
      RADEON_SYNC(info, pScrn);

    if (mode) {
      if (pRcrtc->crtc_id == 0)
	ret = RADEONInit2(pScrn, mode, NULL, 1, &info->ModeReg);
      else if (pRcrtc->crtc_id == 1)
	ret = RADEONInit2(pScrn, NULL, mode, 2, &info->ModeReg);
      
      if (!ret) {
	crtc->enabled = save_enabled;
	for (o = 0; o < config->num_output; o++) {
	  xf86OutputPtr output = config->output[o];
	  output->crtc = save_crtcs[o];
	}
	DEALLOCATE_LOCAL(save_crtcs);
	return FALSE;
      }
      crtc->desiredMode = *mode;
      
      pScrn->vtSema = TRUE;
      RADEONBlank(pScrn);
      RADEONRestoreMode(pScrn, &info->ModeReg);
      RADEONUnblank(pScrn);
      
      if (info->DispPriority)
	RADEONInitDispBandwidth(pScrn);
    }
  }
  DEALLOCATE_LOCAL(save_crtcs);
  return RADEONRandRCrtcNotify(randr_crtc);
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
xf86RandR12SetInfo12 (ScreenPtr pScreen)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
    xf86CrtcConfigPtr   config = XF86_CRTC_CONFIG_PTR(pScrn);
    RROutputPtr		*clones;
    RRCrtcPtr		*crtcs;
    int ncrtc;
    int o, c, l;
    RRCrtcPtr		randr_crtc;
    int nclone;

    clones = ALLOCATE_LOCAL(config->num_output * sizeof (RROutputPtr));
    crtcs = ALLOCATE_LOCAL (config->num_crtc * sizeof (RRCrtcPtr));
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
	{
	    DEALLOCATE_LOCAL (crtcs);
	    DEALLOCATE_LOCAL (clones);
	    return FALSE;
	}

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
	{
	    DEALLOCATE_LOCAL (crtcs);
	    DEALLOCATE_LOCAL (clones);
	    return FALSE;
	}
    }
    DEALLOCATE_LOCAL (crtcs);
    DEALLOCATE_LOCAL (clones);
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
    return RADEONRandRSetInfo12 (pScrn);
}

extern const char *ConnectorTypeName[], *ConnectorTypeNameATOM[];

Bool
RADEONRandRCreateScreenResources (ScreenPtr pScreen)
{
#if RANDR_12_INTERFACE
    if (RADEONRandRCreateScreenResources12 (pScreen))
      return TRUE;
#endif
    return FALSE;
}

static Bool
xf86RandR12CreateObjects12(ScreenPtr pScreen)
{
  ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
  xf86CrtcConfigPtr   config = XF86_CRTC_CONFIG_PTR(pScrn);
  int c, o;

  if (!RRInit())
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
xf86RandRCreateScreenResources12 (ScreenPtr pScreen)
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

Bool
RADEONRandRInit (ScreenPtr    pScreen, int rotation)
{
    rrScrPrivPtr	rp;
    XF86RandRInfoPtr	randrp;
    
#ifdef PANORAMIX
    /* XXX disable RandR when using Xinerama */
    if (!noPanoramiXExtension)
	return TRUE;
#endif
    if (RADEONRandRGeneration != serverGeneration)
    {
	RADEONRandRIndex = AllocateScreenPrivateIndex();
	RADEONRandRGeneration = serverGeneration;
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
    //    rp->rrGetInfo = RADEONRandRGetInfo;
    //    rp->rrSetConfig = RADEONRandRSetConfig;

    randrp->virtualX = -1;
    randrp->virtualY = -1;
    randrp->mmWidth = pScreen->mmWidth;
    randrp->mmHeight = pScreen->mmHeight;
    
    randrp->rotation = RR_Rotate_0; /* initial rotated mode */

    randrp->supported_rotations = rotation;

    randrp->maxX = randrp->maxY = 0;

    pScreen->devPrivates[RADEONRandRIndex].ptr = randrp;

#if RANDR_12_INTERFACE
    if (!RADEONRandRInit12 (pScreen))
	return FALSE;
#endif
    return TRUE;
}

Bool
RADEONRandRInit12(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  rrScrPrivPtr rp = rrGetScrPriv(pScreen);

    rp->rrGetInfo = xf86RandR12GetInfo12;
    rp->rrScreenSetSize = xf86RandR12ScreenSetSize;
    rp->rrCrtcSet = xf86RandR12CrtcSet;
    rp->rrCrtcSetGamma = xf86RandR12CrtcSetGamma;
  rp->rrSetConfig = NULL;
  //  memset (rp->modes, '\0', sizeof (rp->modes));
  pScrn->PointerMoved = RADEONRandRPointerMoved;

  if (!xf86RandR12CreateObjects12 (pScreen))
    return FALSE;
  /*
   * Configure output modes
   */
  if (!xf86RandR12SetInfo12 (pScreen))
    return FALSE;
  return TRUE;
}

static RRModePtr
RADEONRRDefaultMode (RROutputPtr output)
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
RADEONClosestMode (RROutputPtr output, RRModePtr match)
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
RADEONRRPickCrtcs (RROutputPtr	*outputs,
		 RRCrtcPtr	*best_crtcs,
		 RRModePtr	*modes,
		 int		num_outputs,
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
    
    if (n == num_outputs)
	return 0;
    output = outputs[n];
    
    /*
     * Compute score with this output disabled
     */
    best_crtcs[n] = NULL;
    best_crtc = NULL;
    best_score = RADEONRRPickCrtcs (outputs, best_crtcs, modes, num_outputs, n+1);
    if (modes[n] == NULL)
	return best_score;
    
    crtcs = xalloc (num_outputs * sizeof (RRCrtcPtr));
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
	score = my_score + RADEONRRPickCrtcs (outputs, crtcs, modes,
					    num_outputs, n+1);
	if (score >= best_score)
	{
	    best_crtc = crtc;
	    best_score = score;
	    memcpy (best_crtcs, crtcs, num_outputs * sizeof (RRCrtcPtr));
	}
    }
    xfree (crtcs);
    return best_score;
}

static Bool
RADEONRRInitialConfiguration (RROutputPtr *outputs,
			    RRCrtcPtr	*crtcs,
			    RRModePtr	*modes,
			    int		num_outputs)
{
    int		o;
    RRModePtr	target_mode = NULL;

    for (o = 0; o < num_outputs; o++)
	modes[o] = NULL;
    
    /*
     * Let outputs with preferred modes drive screen size
     */
    for (o = 0; o < num_outputs; o++)
    {
	RROutputPtr output = outputs[o];

	if (output->connection != RR_Disconnected && output->numPreferred)
	{
	    target_mode = RADEONRRDefaultMode (output);
	    if (target_mode)
	    {
		modes[o] = target_mode;
		break;
	    }
	}
    }
    if (!target_mode)
    {
	for (o = 0; o < num_outputs; o++)
	{
	    RROutputPtr output = outputs[o];
	    if (output->connection != RR_Disconnected)
	    {
		target_mode = RADEONRRDefaultMode (output);
		if (target_mode)
		{
		    modes[o] = target_mode;
		    break;
		}
	    }
	}
    }
    for (o = 0; o < num_outputs; o++)
    {
	RROutputPtr output = outputs[o];
	
	if (output->connection != RR_Disconnected && !modes[o])
	    modes[o] = RADEONClosestMode (output, target_mode);
    }

    if (!RADEONRRPickCrtcs (outputs, crtcs, modes, num_outputs, 0))
	return FALSE;
    
    return TRUE;
}

/*
 * Compute the virtual size necessary to place all of the available
 * crtcs in a panorama configuration
 */

static void
RADEONRRDefaultScreenLimits (RROutputPtr *outputs, int num_outputs,
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

	for (o = 0; o < num_outputs; o++) 
	{
	    RROutputPtr	output = outputs[o];

	    for (s = 0; s < output->numCrtcs; s++)
		if (output->crtcs[s] == crtc)
		    break;
	    if (s == output->numCrtcs)
		continue;
	    for (m = 0; m < output->numModes; m++)
	    {
		RRModePtr   mode = output->modes[m];
		if (mode->mode.width > crtc_width)
		    crtc_width = mode->mode.width;
		if (mode->mode.height > crtc_width)
		    crtc_height = mode->mode.height;
	    }
	}
	width += crtc_width;
	if (crtc_height > height)
	    height = crtc_height;
    }
    *widthp = width;
    *heightp = height;
}

#if 0
Bool
RADEONRandRPreInit(ScrnInfoPtr pScrn)
{
  RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
#if RANDR_12_INTERFACE
  RROutputPtr outputs[RADEON_MAX_CONNECTOR];
  RRCrtcPtr output_crtcs[RADEON_MAX_CONNECTOR];
  RRModePtr output_modes[RADEON_MAX_CONNECTOR];
  RRCrtcPtr crtcs[RADEON_MAX_CRTC];
#endif
  int conn, crtc;
  int o,c;
  int width, height;

  RADEONProbeOutputModes(pScrn);

#if RANDR_12_INTERFACE
  if (!xf86RandRCreateObjects12(pScrn))
    return FALSE;

  if (!RADEONRandRSetInfo12(pScrn))
    return FALSE;
  
#endif
    
  /*
   * With RandR info set up, let RandR choose
   * the initial configuration
   */
  for (o = 0; o < RADEON_MAX_CONNECTOR; o++)
    outputs[o] = pRADEONEnt->pOutput[o]->randr_output;
  for (c = 0; c < RADEON_MAX_CRTC; c++)
    crtcs[c] = pRADEONEnt->pCrtc[c]->randr_crtc;
  
  if (!RADEONRRInitialConfiguration (outputs, output_crtcs, output_modes,
				     RADEON_MAX_CONNECTOR))
	return FALSE;
    
  RADEONRRDefaultScreenLimits (outputs, RADEON_MAX_CONNECTOR,
			       crtcs, RADEON_MAX_CRTC,
			       &width, &height);
    
    if (width > pScrn->virtualX)
      pScrn->virtualX = width;
    if (height > pScrn->virtualY)
      pScrn->virtualY = height;
    
    for (o = 0; o < RADEON_MAX_CONNECTOR; o++)
    {
	RRModePtr	randr_mode = output_modes[o];
	DisplayModePtr	mode;
	RRCrtcPtr	randr_crtc = output_crtcs[o];
	int		pipe;
	Bool		enabled;

	if (randr_mode)
	    mode = (DisplayModePtr) randr_mode->devPrivate;
	else
	    mode = NULL;
	if (randr_crtc)
	{
	    pipe = (int) randr_crtc->devPrivate;
	    enabled = TRUE;
	}
	else
	{
	    pipe = 0;
	    enabled = FALSE;
	}
	//	if (mode)
	//	    pRADEON->pipes[pipe].desiredMode = *mode;
	//	pRADEON->output[o].pipe = pipe;
	    //	pRADEON->output[o].enabled = enabled;
    }

  RADEON_set_xf86_modes_from_outputs(pScrn);
  RADEON_set_default_screen_size(pScrn);
    
  return TRUE;
}
#endif
#endif

Bool
xf86RandR12PreInit (ScrnInfoPtr pScrn)
{
    return TRUE;
}
