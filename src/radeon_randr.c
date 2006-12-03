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
#ifdef RANDR_12_INTERFACE
    DisplayModePtr  		    modes[2];
#endif
} XF86RandRInfoRec, *XF86RandRInfoPtr;

#ifdef RANDR_12_INTERFACE
static Bool RADEONRandRInit12 (ScreenPtr pScreen);
static Bool RADEONRandRCreateScreenResources12 (ScreenPtr pScreen);
#endif

 
static int	    RADEONRandRIndex;
static int	    RADEONRandRGeneration;

#define XF86RANDRINFO(p)    ((XF86RandRInfoPtr) (p)->devPrivates[RADEONRandRIndex].ptr)

#if RANDR_12_INTERFACE
static void
RADEONRandRPointerMoved (int scrnIndex, int x, int y)
{
}

static Bool
RADEONRandRScreenSetSize (ScreenPtr	pScreen,
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
RADEONRandRCrtcNotify (RRCrtcPtr crtc)
{
  ScreenPtr		pScreen = crtc->pScreen;
  XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
  ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
  RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
  RRModePtr		mode = NULL;
  int i, j;
  int numOutputs = 0;
  int x, y;
  int rotation = RR_Rotate_0;
  RROutputPtr outputs[RADEON_MAX_CRTC];
  RROutputPtr rrout;

  for (i = 0; i<RADEON_MAX_CONNECTOR; i++) {
    
    rrout = pRADEONEnt->PortInfo[i]->randr_output;

    outputs[numOutputs++] = rrout;
    for (j = 0; j<rrout->numModes; j++) {
      DisplayModePtr outMode = rrout->modes[j]->devPrivate;
      mode = rrout->modes[j];
    }
  }
  
  return RRCrtcNotify (crtc, mode, x, y, rotation, numOutputs, outputs);
}

static Bool
RADEONRandRCrtcSet (ScreenPtr	pScreen,
		  RRCrtcPtr	crtc,
		  RRModePtr	mode,
		  int		x,
		  int		y,
		  Rotation	rotation,
		  int		num_randr_outputs,
		  RROutputPtr	*randr_outputs)
{
  XF86RandRInfoPtr randrp = XF86RANDRINFO(pScreen);
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
 
  return RADEONRandRCrtcNotify(crtc);
}


static Bool
RADEONRandRCrtcSetGamma (ScreenPtr    pScreen,
		       RRCrtcPtr    crtc)
{
    return FALSE;
}

/**
 * Given a list of xf86 modes and a RandR Output object, construct
 * RandR modes and assign them to the output
 */
static Bool
RADEONxf86RROutputSetModes (RROutputPtr randr_output, DisplayModePtr modes)
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
RADEONRandRSetInfo12 (ScrnInfoPtr pScrn)
{
    RADEONEntPtr pRADEONEnt = RADEONEntPriv(pScrn);
    RROutputPtr		clones[RADEON_MAX_CONNECTOR];
    RRCrtcPtr		crtc;
    int			nclone;
    RRCrtcPtr		crtcs[RADEON_MAX_CRTC];
    int			ncrtc;
    DisplayModePtr	modes, mode;
    xRRModeInfo		modeInfo;
    RRModePtr		rrmode, *rrmodes;
    int                 nmode, npreferred;
    int i, j, p;
    CARD32		possibleOptions = 0;
    CARD32		currentOptions = 0;
    int			connection;
    int                 subpixel = SubPixelNone;
    RRCrtcPtr		    randr_crtc;
    RADEONConnector *connector;

    for (i = 0; i < RADEON_MAX_CONNECTOR; i++) {
      ncrtc = 0;
      crtc = NULL;
      
      connector = pRADEONEnt->PortInfo[i];

      if (connector->MonType) {
	crtc = pRADEONEnt->Controller[i]->randr_crtc;
	crtcs[ncrtc++] = crtc;
	randr_crtc = crtc;
      } else
	randr_crtc = NULL;
      

      if (!RROutputSetCrtcs(connector->randr_output, crtcs, ncrtc))
	return FALSE;

      RROutputSetCrtc(connector->randr_output, crtc);

      nmode = 0;
      npreferred = 0;
      rrmodes = NULL;
      
      if (connector->probed_modes) {
	RADEONxf86RROutputSetModes (connector->randr_output, connector->probed_modes);
      }

      connection = RR_Disconnected;
      if (connector->MonType > MT_NONE)
	connection = RR_Connected;

      RROutputSetConnection(connector->randr_output, connection);

      RROutputSetSubpixelOrder(connector->randr_output, subpixel);
    }
    return TRUE;
}

/*
 * Query the hardware for the current state, then mirror
 * that to RandR
 */
static Bool
RADEONRandRGetInfo12 (ScreenPtr pScreen, Rotation *rotations)
{
    ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];

    RADEONProbeOutputModes(pScrn);
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
RADEONRandRCreateObjects12(ScrnInfoPtr pScrn)
{
  int i;
  RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
  RADEONInfoPtr  info = RADEONPTR(pScrn);

  if (!RRInit())
    return FALSE;

  /*
   * Create RandR resources, then probe them
   */
  for (i = 0; i < 2; i++)
  {
    RRCrtcPtr randr_crtc = RRCrtcCreate((void *)i);

    if (!randr_crtc)
      return FALSE;

    RRCrtcGammaSetSize(randr_crtc, 256);
    pRADEONEnt->Controller[i]->randr_crtc = randr_crtc;
  }

  for (i = 0; i < 2; i++)
  {
    int output = pRADEONEnt->PortInfo[i]->ConnectorType;
    const char *name = name = info->IsAtomBios ? ConnectorTypeNameATOM[output] : ConnectorTypeName[output];
    RROutputPtr randr_output = RROutputCreate(name, strlen(name),
					      (void *) i);
    
    if (!randr_output)
      return FALSE;

    pRADEONEnt->PortInfo[i]->randr_output = randr_output;
  }
  return TRUE;
}

static Bool
RADEONRandRCreateScreenResources12 (ScreenPtr pScreen)
{
  XF86RandRInfoPtr	randrp = XF86RANDRINFO(pScreen);
  ScrnInfoPtr		pScrn = xf86Screens[pScreen->myNum];
  RADEONInfoPtr  info = RADEONPTR(pScrn);
  RADEONEntPtr pRADEONEnt   = RADEONEntPriv(pScrn);
  DisplayModePtr mode;
  const char *name;
  int i;

  for (i = 0; i < 2; i++)
  {
    if (!RRCrtcAttachScreen(pRADEONEnt->Controller[i]->randr_crtc, pScreen))
      return FALSE;
  }
  
  for (i = 0; i < 2; i++) {
    if (!RROutputAttachScreen(pRADEONEnt->PortInfo[i]->randr_output, pScreen))
      return FALSE;
  }

  mode = pScrn->currentMode;
  if (mode) {
    int mmWidth, mmHeight;
    
    if (mode->HDisplay == pScreen->width &&
	mode->VDisplay == pScreen->height)
      {
	mmWidth = pScrn->widthmm;
	mmHeight = pScrn->heightmm;
      } else {
#define MMPERINCH 25.4
	mmWidth = (double) mode->HDisplay / pScrn->xDpi * MMPERINCH;
	mmHeight = (double) mode->VDisplay / pScrn->yDpi * MMPERINCH;
    }
    RADEONRandRScreenSetSize (pScreen,
			    mode->HDisplay,
			    mode->VDisplay,
			    mmWidth,
			    mmHeight);
  }

  for (i = 0; i < RADEON_MAX_CRTC; i++)
    RADEONRandRCrtcNotify (pRADEONEnt->Controller[i]->randr_crtc);
    
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

static Bool
RADEONRandRInit12(ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  rrScrPrivPtr rp = rrGetScrPriv(pScreen);

  rp->rrGetInfo = RADEONRandRGetInfo12;
  rp->rrScreenSetSize = RADEONRandRScreenSetSize;
  rp->rrCrtcSet = RADEONRandRCrtcSet;
  rp->rrCrtcSetGamma = RADEONRandRCrtcSetGamma;
  rp->rrSetConfig = NULL;
  //  memset (rp->modes, '\0', sizeof (rp->modes));
  pScrn->PointerMoved = RADEONRandRPointerMoved;
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
  if (!RADEONRandRCreateObjects12(pScrn))
    return FALSE;

  if (!RADEONRandRSetInfo12(pScrn))
    return FALSE;
  
#endif
    
  /*
   * With RandR info set up, let RandR choose
   * the initial configuration
   */
  for (o = 0; o < RADEON_MAX_CONNECTOR; o++)
    outputs[o] = pRADEONEnt->PortInfo[o]->randr_output;
  for (c = 0; c < RADEON_MAX_CRTC; c++)
    crtcs[c] = pRADEONEnt->Controller[c]->randr_crtc;
  
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
