/* $XFree86$ */
/*
 * Copyright 2003 Alex Deucher.
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
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ALEX DEUCHER, OR ANY OTHER 
 * CONTRIBUTORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Alex Deucher <agd5f@yahoo.com>
 */

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86Resources.h"
#include "xf86_OSproc.h"
#include "extnsionst.h"  	/* required */
#include "panoramiXproto.h"  	/* required */
#include "dixstruct.h"
#include "vbe.h"


#include "radeon.h"
#include "radeon_macros.h"
#include "radeon_reg.h"
#include "radeon_mergedfb.h"

/* psuedo xinerama support */
static unsigned char 	RADEONXineramaReqCode = 0;
int 			RADEONXineramaPixWidth = 0;
int 			RADEONXineramaPixHeight = 0;
int 			RADEONXineramaNumScreens = 0;
RADEONXineramaData	*RADEONXineramadataPtr = NULL;
static int 		RADEONXineramaGeneration;
Bool 		RADEONnoPanoramiXExtension = TRUE;

int RADEONProcXineramaQueryVersion(ClientPtr client);
int RADEONProcXineramaGetState(ClientPtr client);
int RADEONProcXineramaGetScreenCount(ClientPtr client);
int RADEONProcXineramaGetScreenSize(ClientPtr client);
int RADEONProcXineramaIsActive(ClientPtr client);
int RADEONProcXineramaQueryScreens(ClientPtr client);
int RADEONSProcXineramaDispatch(ClientPtr client);

static void
RADEONChooseCursorCRTC(ScrnInfoPtr pScrn1, int x, int y);

/* mergedfb functions */
/* Helper function for CRT2 monitor vrefresh/hsync options
 * (Taken from mga, sis drivers)
 */
int
RADEONStrToRanges(range *r, char *s, int max)
{
   float num = 0.0;
   int rangenum = 0;
   Bool gotdash = FALSE;
   Bool nextdash = FALSE;
   char* strnum = NULL;
   do {
     switch(*s) {
     case '0':
     case '1':
     case '2':
     case '3':
     case '4':
     case '5':
     case '6':
     case '7':
     case '8':
     case '9':
     case '.':
        if(strnum == NULL) {
           strnum = s;
           gotdash = nextdash;
           nextdash = FALSE;
        }
        break;
     case '-':
     case ' ':
     case 0:
        if(strnum == NULL) break;
	sscanf(strnum, "%f", &num);
	strnum = NULL;
        if(gotdash)
           r[rangenum - 1].hi = num;
        else {
           r[rangenum].lo = num;
           r[rangenum].hi = num;
           rangenum++;
        }
        if(*s == '-') nextdash = (rangenum != 0);
        else if(rangenum >= max) return rangenum;
        break;
     default :
        return 0;
     }
   } while(*(s++) != 0);

   return rangenum;
}

/* Copy and link two modes form merged-fb mode
 * (Taken from mga, sis drivers)
 * Copys mode i, links the result to dest, and returns it.
 * Links i and j in Private record.
 * If dest is NULL, return value is copy of i linked to itself.
 * For mergedfb auto-config, we only check the dimension
 * against virtualX/Y, if they were user-provided.
 */
static DisplayModePtr
RADEONCopyModeNLink(ScrnInfoPtr pScrn, DisplayModePtr dest,
                 DisplayModePtr i, DisplayModePtr j,
		 RADEONScrn2Rel srel)
{
    DisplayModePtr mode;
    int dx = 0,dy = 0;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

    if(!((mode = xalloc(sizeof(DisplayModeRec))))) return dest;
    memcpy(mode, i, sizeof(DisplayModeRec));
    if(!((mode->Private = xalloc(sizeof(RADEONMergedDisplayModeRec))))) {
       xfree(mode);
       return dest;
    }
    ((RADEONMergedDisplayModePtr)mode->Private)->CRT1 = i;
    ((RADEONMergedDisplayModePtr)mode->Private)->CRT2 = j;
    ((RADEONMergedDisplayModePtr)mode->Private)->CRT2Position = srel;
    mode->PrivSize = 0;

    switch(srel) {
    case radeonLeftOf:
    case radeonRightOf:
       if(!(pScrn->display->virtualX)) {
          dx = i->HDisplay + j->HDisplay;
       } else {
          dx = min(pScrn->virtualX, i->HDisplay + j->HDisplay);
       }
       dx -= mode->HDisplay;
       if(!(pScrn->display->virtualY)) {
          dy = max(i->VDisplay, j->VDisplay);
       } else {
          dy = min(pScrn->virtualY, max(i->VDisplay, j->VDisplay));
       }
       dy -= mode->VDisplay;
       break;
    case radeonAbove:
    case radeonBelow:
       if(!(pScrn->display->virtualY)) {
          dy = i->VDisplay + j->VDisplay;
       } else {
          dy = min(pScrn->virtualY, i->VDisplay + j->VDisplay);
       }
       dy -= mode->VDisplay;
       if(!(pScrn->display->virtualX)) {
          dx = max(i->HDisplay, j->HDisplay);
       } else {
          dx = min(pScrn->virtualX, max(i->HDisplay, j->HDisplay));
       }
       dx -= mode->HDisplay;
       break;
    case radeonClone:
       if(!(pScrn->display->virtualX)) {
          dx = max(i->HDisplay, j->HDisplay);
       } else {
          dx = min(pScrn->virtualX, max(i->HDisplay, j->HDisplay));
       }
       dx -= mode->HDisplay;
       if(!(pScrn->display->virtualY)) {
          dy = max(i->VDisplay, j->VDisplay);
       } else {
	  dy = min(pScrn->virtualY, max(i->VDisplay, j->VDisplay));
       }
       dy -= mode->VDisplay;
       break;
    }
    mode->HDisplay += dx;
    mode->HSyncStart += dx;
    mode->HSyncEnd += dx;
    mode->HTotal += dx;
    mode->VDisplay += dy;
    mode->VSyncStart += dy;
    mode->VSyncEnd += dy;
    mode->VTotal += dy;
    mode->Clock = 0;

    if( ((mode->HDisplay * ((pScrn->bitsPerPixel + 7) / 8) * mode->VDisplay) > 
	(pScrn->videoRam * 1024)) ||
        (mode->HDisplay > 8192) ||
	(mode->VDisplay > 8192) ) {

       xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
       		"Skipped %dx%d, not enough video RAM or beyond hardware specs\n",
		mode->HDisplay, mode->VDisplay);
       xfree(mode->Private);
       xfree(mode);

       return dest;
    }

    if(srel != radeonClone) {
       info->AtLeastOneNonClone = TRUE;
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
    	"Merged %dx%d and %dx%d to %dx%d%s\n",
	i->HDisplay, i->VDisplay, j->HDisplay, j->VDisplay,
	mode->HDisplay, mode->VDisplay, (srel == radeonClone) ? " (Clone)" : "");

    mode->next = mode;
    mode->prev = mode;

    if(dest) {
        mode->next = dest->next; 	/* Insert node after "dest" */
        dest->next->prev = mode;
        mode->prev = dest;
        dest->next = mode;
    }

    return mode;
}

/* Helper function to find a mode from a given name
 * (Taken from mga driver)
 */
static DisplayModePtr
RADEONGetModeFromName(char* str, DisplayModePtr i)
{
    DisplayModePtr c = i;
    if(!i) return NULL;
    do {
       if(strcmp(str, c->name) == 0) return c;
       c = c->next;
    } while(c != i);
    return NULL;
}

static DisplayModePtr
RADEONFindWidestTallestMode(DisplayModePtr i, Bool tallest)
{
    DisplayModePtr c = i, d = NULL;
    int max = 0;
    if(!i) return NULL;
    do {
       if(tallest) {
          if(c->VDisplay > max) {
             max = c->VDisplay;
	     d = c;
          }
       } else {
          if(c->HDisplay > max) {
             max = c->HDisplay;
	     d = c;
          }
       }
       c = c->next;
    } while(c != i);
    return d;
}

static DisplayModePtr
RADEONGenerateModeListFromLargestModes(ScrnInfoPtr pScrn,
		    DisplayModePtr i, DisplayModePtr j,
		    RADEONScrn2Rel srel)
{

    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    DisplayModePtr mode1 = NULL;
    DisplayModePtr mode2 = NULL;
    DisplayModePtr result = NULL;
    int p = 0;
    int count = 0;

    info->AtLeastOneNonClone = FALSE;


    switch(srel) {
    case radeonLeftOf:
    case radeonRightOf:
       mode1 = RADEONFindWidestTallestMode(i, FALSE);
       mode2 = RADEONFindWidestTallestMode(j, FALSE);
       break;
    case radeonAbove:
    case radeonBelow:
       mode1 = RADEONFindWidestTallestMode(i, TRUE);
       mode2 = RADEONFindWidestTallestMode(j, TRUE);
       break;
    case radeonClone:
       mode1 = i;
       mode2 = j;
	while (pScrn->display->modes[count]) count++;
	for (p = 0; p < count; p++) {
	    result = RADEONCopyModeNLink(pScrn, result, mode1, mode2, srel);
	    mode1 = mode1->next;
	    mode2 = mode2->next;
	}  
    }

    if(mode1 && mode2) {
	if (srel == radeonClone)
	    return result;
	else 
            return(RADEONCopyModeNLink(pScrn, result, mode1, mode2, srel));
    } else {
       return NULL;
    }
}

/* Generate the merged-fb mode modelist
 * (Taken from mga driver)
 */
static DisplayModePtr
RADEONGenerateModeListFromMetaModes(ScrnInfoPtr pScrn, char* str,
		    DisplayModePtr i, DisplayModePtr j,
		    RADEONScrn2Rel srel)
{
    char* strmode = str;
    char modename[256];
    Bool gotdash = FALSE;
    RADEONScrn2Rel sr;
    DisplayModePtr mode1 = NULL;
    DisplayModePtr mode2 = NULL;
    DisplayModePtr result = NULL;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);

    info->AtLeastOneNonClone = FALSE;

    do {
        switch(*str) {
        case 0:
        case '-':
        case ' ':
           if((strmode != str)) {

              strncpy(modename, strmode, str - strmode);
              modename[str - strmode] = 0;

              if(gotdash) {
                 if(mode1 == NULL) return NULL;
                 mode2 = RADEONGetModeFromName(modename, j);
                 if(!mode2) {
                    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                        "Mode \"%s\" is not a supported mode for CRT2\n", modename);
                    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                        "Skipping metamode \"%s-%s\".\n", mode1->name, modename);
                    mode1 = NULL;
                 }
              } else {
                 mode1 = RADEONGetModeFromName(modename, i);
                 if(!mode1) {
                    char* tmps = str;
                    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                        "Mode \"%s\" is not a supported mode for CRT1\n", modename);
                    gotdash = FALSE;
                    while(*tmps == ' ') tmps++;
                    if(*tmps == '-') { 							/* skip the next mode */
                       tmps++;
                       while((*tmps == ' ') && (*tmps != 0)) tmps++; 			/* skip spaces */
                       while((*tmps != ' ') && (*tmps != '-') && (*tmps != 0)) tmps++; 	/* skip modename */
                       strncpy(modename,strmode,tmps - strmode);
                       modename[tmps - strmode] = 0;
                       str = tmps-1;
                    }
                    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                        "Skipping metamode \"%s\".\n", modename);
                    mode1 = NULL;
                 }
              }
              gotdash = FALSE;
           }
           strmode = str + 1;
           gotdash |= (*str == '-');

           if(*str != 0) break;
	   /* Fall through otherwise */

        default:
           if(!gotdash && mode1) {
              sr = srel;
              if(!mode2) {
                 mode2 = RADEONGetModeFromName(mode1->name, j);
                 sr = radeonClone;
              }
              if(!mode2) {
                 xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                     "Mode: \"%s\" is not a supported mode for CRT2\n", mode1->name);
                 xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                     "Skipping metamode \"%s\".\n", mode1->name);
                 mode1 = NULL;
              } else {
                 result = RADEONCopyModeNLink(pScrn, result, mode1, mode2, sr);
                 mode1 = NULL;
                 mode2 = NULL;
              }
           }
           break;

        }

    } while(*(str++) != 0);
     
    return result;
}

DisplayModePtr
RADEONGenerateModeList(ScrnInfoPtr pScrn, char* str,
		    DisplayModePtr i, DisplayModePtr j,
		    RADEONScrn2Rel srel)
{
   if(str != NULL) {
      return(RADEONGenerateModeListFromMetaModes(pScrn, str, i, j, srel));
   } else {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO,
      	"No MetaModes given, linking %s modes by default\n",
	(srel == radeonClone) ? "first" : "largest");
      return(RADEONGenerateModeListFromLargestModes(pScrn, i, j, srel));
   }
}

void
RADEONRecalcDefaultVirtualSize(ScrnInfoPtr pScrn)
{
    DisplayModePtr mode, bmode;
    int max;
    static const char *str = "MergedFB: Virtual %s %d\n";

    if(!(pScrn->display->virtualX)) {
       mode = bmode = pScrn->modes;
       max = 0;
       do {
          if(mode->HDisplay > max) max = mode->HDisplay;
          mode = mode->next;
       } while(mode != bmode);
       pScrn->virtualX = max;
       pScrn->displayWidth = max;
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED, str, "width", max);
    }
    if(!(pScrn->display->virtualY)) {
       mode = bmode = pScrn->modes;
       max = 0;
       do {
          if(mode->VDisplay > max) max = mode->VDisplay;
          mode = mode->next;
       } while(mode != bmode);
       pScrn->virtualY = max;
       xf86DrvMsg(pScrn->scrnIndex, X_PROBED, str, "height", max);
    }
}

/* Pseudo-Xinerama extension for MergedFB mode */
void
RADEONUpdateXineramaScreenInfo(ScrnInfoPtr pScrn1)
{
    RADEONInfoPtr  info   = RADEONPTR(pScrn1);
    ScrnInfoPtr pScrn2 = NULL;
    int crt1scrnnum = 0, crt2scrnnum = 1;
    int x1=0, x2=0, y1=0, y2=0, h1=0, h2=0, w1=0, w2=0;
    DisplayModePtr currentMode, firstMode;
    Bool infochanged = FALSE;

    if(!info->MergedFB) return;

    if(RADEONnoPanoramiXExtension) return;

    if(!RADEONXineramadataPtr) return;

    if(info->CRT2IsScrn0) {
       crt1scrnnum = 1;
       crt2scrnnum = 0;
    }

    pScrn2 = info->CRT2pScrn;

    /* Attention: Usage of RandR may lead into virtual X and Y values
     * actually smaller than our MetaModes! To avoid this, we calculate
     * the maxCRT fields here (and not somewhere else, like in CopyNLink)
     */

    if((info->RADEONXineramaVX != pScrn1->virtualX) || (info->RADEONXineramaVY != pScrn1->virtualY)) {

      if(!(pScrn1->modes)) {
          xf86DrvMsg(pScrn1->scrnIndex, X_ERROR,
       	     "Internal error: RADEONUpdateXineramaScreenInfo(): pScrn->modes is NULL\n");
	  return;
       }

       info->maxCRT1_X1 = info->maxCRT1_X2 = 0;
       info->maxCRT1_Y1 = info->maxCRT1_Y2 = 0;
       info->maxCRT2_X1 = info->maxCRT2_X2 = 0;
       info->maxCRT2_Y1 = info->maxCRT2_Y2 = 0;
       info->maxClone_X1 = info->maxClone_X2 = 0;
       info->maxClone_Y1 = info->maxClone_Y2 = 0;

       currentMode = firstMode = pScrn1->modes;

       do {

          DisplayModePtr p = currentMode->next;
          DisplayModePtr i = ((RADEONMergedDisplayModePtr)currentMode->Private)->CRT1;
          DisplayModePtr j = ((RADEONMergedDisplayModePtr)currentMode->Private)->CRT2;
          RADEONScrn2Rel srel = ((RADEONMergedDisplayModePtr)currentMode->Private)->CRT2Position;

          if((i->HDisplay <= pScrn1->virtualX) && (j->HDisplay <= pScrn1->virtualX) &&
             (i->VDisplay <= pScrn1->virtualY) && (j->VDisplay <= pScrn1->virtualY)) {

             if(srel != radeonClone) {
                if(info->maxCRT1_X1 <= i->HDisplay) {
                   info->maxCRT1_X1 = i->HDisplay;      /* Largest CRT1 mode */
                   if(info->maxCRT1_X2 < j->HDisplay) {
                      info->maxCRT1_X2 = j->HDisplay;   /* Largest X of CRT2 mode displayed with largest CRT1 mode */
                   }
                }
                if(info->maxCRT2_X2 <= j->HDisplay) {
                   info->maxCRT2_X2 = j->HDisplay;      /* Largest CRT2 mode */
                   if(info->maxCRT2_X1 < i->HDisplay) {
                      info->maxCRT2_X1 = i->HDisplay;   /* Largest X of CRT1 mode displayed with largest CRT2 mode */
                   }
                }
                if(info->maxCRT1_Y1 <= i->VDisplay) {
                   info->maxCRT1_Y1 = i->VDisplay;
                   if(info->maxCRT1_Y2 < j->VDisplay) {
                      info->maxCRT1_Y2 = j->VDisplay;
                   }
                }
                if(info->maxCRT2_Y2 <= j->VDisplay) {
                   info->maxCRT2_Y2 = j->VDisplay;
                   if(info->maxCRT2_Y1 < i->VDisplay) {
                      info->maxCRT2_Y1 = i->VDisplay;
                   }
                }
             } else {
                if(info->maxClone_X1 < i->HDisplay) {
                   info->maxClone_X1 = i->HDisplay;
                }
                if(info->maxClone_X2 < j->HDisplay) {
                   info->maxClone_X2 = j->HDisplay;
                }
                if(info->maxClone_Y1 < i->VDisplay) {
                   info->maxClone_Y1 = i->VDisplay;
                }
                if(info->maxClone_Y2 < j->VDisplay) {
                   info->maxClone_Y2 = j->VDisplay;
                }
             }
          }
          currentMode = p;

       } while((currentMode) && (currentMode != firstMode));

       info->RADEONXineramaVX = pScrn1->virtualX;
       info->RADEONXineramaVY = pScrn1->virtualY;
       infochanged = TRUE;

    }

    /* leftof

        V 1:
        CRT2: 	x = 0
		y = 0
		w = (maxCRT2 X)
		h = (virtual Y)
	CRT1:   x = (virtual X) - (maxCRT1 X)
		y = 0
		w = (maxCRT1 X)
		h = (virtual Y)

	V 2:
       	CRT2: 	x = 0
		y = 0
		w = max CRT2 mode X
		h = virtual Y size
	CRT1:   x = (max) CRT2 mode X von dem Metamode, wo CRT1 mode maximal breit ist
		y = 0
		w = max CRT1 mode X
		h = virtual Y size

	V 3:  (current)
	CRT1:   x = (maxCRT2 X von dem MMode, wo maxCRT1 X)
		y = 0
		w = (virtual X) - x
		h = (virtual Y)
        CRT2: 	x = 0
		y = 0
		w = (virtual X) - (maxCRT1 X von dem MMode, wo maxCRT2 X)
		h = (virtual Y)

    */
    switch(info->CRT2Position) {
    case radeonLeftOf:								/* V 1 */
       x1 = min(info->maxCRT1_X2, pScrn1->virtualX - info->maxCRT1_X1);		/* pScrn1->virtualX - pSiS->maxCRT1_X1; 
*/
       if(x1 < 0) x1 = 0;
       y1 = 0;									/* 0; */
       w1 = pScrn1->virtualX - x1;						/* pSiS->maxCRT1_X1; */
       h1 = pScrn1->virtualY;							/* pScrn1->virtualY; */
       x2 = 0;									/* 0; */
       y2 = 0;									/* 0; */
       w2 = max(info->maxCRT2_X2, pScrn1->virtualX - info->maxCRT2_X1); 	/* pSiS->maxCRT2_X2; */
       if(w2 > pScrn1->virtualX) w2 = pScrn1->virtualX;
       h2 = pScrn1->virtualY; 							/* pScrn1->virtualY; */
       break;
    case radeonRightOf:
       x1 = 0;									/* 0; */
       y1 = 0;									/* 0; */
       w1 = max(info->maxCRT1_X1, pScrn1->virtualX - info->maxCRT1_X2);		/* pSiS->maxCRT1_X1; */
       if(w1 > pScrn1->virtualX) w1 = pScrn1->virtualX;
       h1 = pScrn1->virtualY;							/* pScrn1->virtualY; */
       x2 = min(info->maxCRT2_X1, pScrn1->virtualX - info->maxCRT2_X2);		/* pScrn1->virtualX - pSiS->maxCRT2_X2; 
*/
       if(x2 < 0) x2 = 0;
       y2 = 0;									/* 0; */
       w2 = pScrn1->virtualX - x2;						/* pSiS->maxCRT2_X2; */
       h2 = pScrn1->virtualY;							/* pScrn1->virtualY; */
       break;
    case radeonAbove:
       x1 = 0;									/* 0; */
       y1 = min(info->maxCRT1_Y2, pScrn1->virtualY - info->maxCRT1_Y1);		/* pScrn1->virtualY - pSiS->maxCRT1_Y1; 
*/
       if(y1 < 0) y1 = 0;
       w1 = pScrn1->virtualX;							/* pScrn1->virtualX; */
       h1 = pScrn1->virtualY - y1;						/* pSiS->maxCRT1_Y1; */
       x2 = 0;									/* 0; */
       y2 = 0;									/* 0; */
       w2 = pScrn1->virtualX;							/* pScrn1->virtualX; */
       h2 = max(info->maxCRT2_Y2, pScrn1->virtualY - info->maxCRT2_Y1);		/* pSiS->maxCRT2_Y2; */
       if(h2 > pScrn1->virtualY) h2 = pScrn1->virtualY;
       break;
    case radeonBelow:
       x1 = 0;									/* 0; */
       y1 = 0;									/* 0; */
       w1 = pScrn1->virtualX;							/* pScrn1->virtualX; */
       h1 = max(info->maxCRT1_Y1, pScrn1->virtualY - info->maxCRT1_Y2);		/* pSiS->maxCRT1_Y1; */
       if(h1 > pScrn1->virtualY) h1 = pScrn1->virtualY;
       x2 = 0;									/* 0; */
       y2 = min(info->maxCRT2_Y1, pScrn1->virtualY - info->maxCRT2_Y2);		/* pScrn1->virtualY - pSiS->maxCRT2_Y2; 
*/
       if(y2 < 0) y2 = 0;
       w2 = pScrn1->virtualX;							/* pScrn1->virtualX; */
       h2 = pScrn1->virtualY - y2;						/* pSiS->maxCRT2_Y2; */
       break;
    default:
       xf86DrvMsg(pScrn1->scrnIndex, X_ERROR,
       	  "Internal error: UpdateXineramaInfo(): unsupported CRT2Position (%d)\n",
	  info->CRT2Position);
    }
    RADEONXineramadataPtr[crt1scrnnum].x = x1;
    RADEONXineramadataPtr[crt1scrnnum].y = y1;
    RADEONXineramadataPtr[crt1scrnnum].width = w1;
    RADEONXineramadataPtr[crt1scrnnum].height = h1;
    RADEONXineramadataPtr[crt2scrnnum].x = x2;
    RADEONXineramadataPtr[crt2scrnnum].y = y2;
    RADEONXineramadataPtr[crt2scrnnum].width = w2;
    RADEONXineramadataPtr[crt2scrnnum].height = h2;

    if(infochanged) {
       xf86DrvMsg(pScrn1->scrnIndex, X_INFO,
          "Pseudo-Xinerama: CRT1 (Screen %d) (%d,%d)-(%d,%d)\n",
          crt1scrnnum, x1, y1, w1+x1-1, h1+y1-1);
       xf86DrvMsg(pScrn1->scrnIndex, X_INFO,
          "Pseudo-Xinerama: CRT2 (Screen %d) (%d,%d)-(%d,%d)\n",
          crt2scrnnum, x2, y2, w2+x2-1, h2+y2-1);
    }

}

/* Proc */

int
RADEONProcXineramaQueryVersion(ClientPtr client)
{
    xPanoramiXQueryVersionReply	  rep;
    register int		  n;

    REQUEST_SIZE_MATCH(xPanoramiXQueryVersionReq);
    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.majorVersion = RADEON_XINERAMA_MAJOR_VERSION;
    rep.minorVersion = RADEON_XINERAMA_MINOR_VERSION;
    if(client->swapped) {
        swaps(&rep.sequenceNumber, n);
        swapl(&rep.length, n);
        swaps(&rep.majorVersion, n);
        swaps(&rep.minorVersion, n);
    }
    WriteToClient(client, sizeof(xPanoramiXQueryVersionReply), (char *)&rep);
    return (client->noClientException);
}

int
RADEONProcXineramaGetState(ClientPtr client)
{
    REQUEST(xPanoramiXGetStateReq);
    WindowPtr			pWin;
    xPanoramiXGetStateReply	rep;
    register int		n;

    REQUEST_SIZE_MATCH(xPanoramiXGetStateReq);
    pWin = LookupWindow(stuff->window, client);
    if(!pWin) return BadWindow;

    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.state = !RADEONnoPanoramiXExtension;
    if(client->swapped) {
       swaps (&rep.sequenceNumber, n);
       swapl (&rep.length, n);
       swaps (&rep.state, n);
    }
    WriteToClient(client, sizeof(xPanoramiXGetStateReply), (char *)&rep);
    return client->noClientException;
}

int
RADEONProcXineramaGetScreenCount(ClientPtr client)
{
    REQUEST(xPanoramiXGetScreenCountReq);
    WindowPtr				pWin;
    xPanoramiXGetScreenCountReply	rep;
    register int			n;

    REQUEST_SIZE_MATCH(xPanoramiXGetScreenCountReq);
    pWin = LookupWindow(stuff->window, client);
    if(!pWin) return BadWindow;

    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.ScreenCount = RADEONXineramaNumScreens;
    if(client->swapped) {
       swaps(&rep.sequenceNumber, n);
       swapl(&rep.length, n);
       swaps(&rep.ScreenCount, n);
    }
    WriteToClient(client, sizeof(xPanoramiXGetScreenCountReply), (char *)&rep);
    return client->noClientException;
}

int
RADEONProcXineramaGetScreenSize(ClientPtr client)
{
    REQUEST(xPanoramiXGetScreenSizeReq);
    WindowPtr				pWin;
    xPanoramiXGetScreenSizeReply	rep;
    register int			n;

    REQUEST_SIZE_MATCH(xPanoramiXGetScreenSizeReq);
    pWin = LookupWindow (stuff->window, client);
    if(!pWin)  return BadWindow;

    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.width  = RADEONXineramadataPtr[stuff->screen].width;
    rep.height = RADEONXineramadataPtr[stuff->screen].height;
    if(client->swapped) {
       swaps(&rep.sequenceNumber, n);
       swapl(&rep.length, n);
       swaps(&rep.width, n);
       swaps(&rep.height, n);
    }
    WriteToClient(client, sizeof(xPanoramiXGetScreenSizeReply), (char *)&rep);
    return client->noClientException;
}

int
RADEONProcXineramaIsActive(ClientPtr client)
{
    xXineramaIsActiveReply	rep;

    REQUEST_SIZE_MATCH(xXineramaIsActiveReq);

    rep.type = X_Reply;
    rep.length = 0;
    rep.sequenceNumber = client->sequence;
    rep.state = !RADEONnoPanoramiXExtension;
    if(client->swapped) {
	register int n;
	swaps(&rep.sequenceNumber, n);
	swapl(&rep.length, n);
	swapl(&rep.state, n);
    }
    WriteToClient(client, sizeof(xXineramaIsActiveReply), (char *) &rep);
    return client->noClientException;
}

int
RADEONProcXineramaQueryScreens(ClientPtr client)
{
    xXineramaQueryScreensReply	rep;

    REQUEST_SIZE_MATCH(xXineramaQueryScreensReq);

    rep.type = X_Reply;
    rep.sequenceNumber = client->sequence;
    rep.number = (RADEONnoPanoramiXExtension) ? 0 : RADEONXineramaNumScreens;
    rep.length = rep.number * sz_XineramaScreenInfo >> 2;
    if(client->swapped) {
       register int n;
       swaps(&rep.sequenceNumber, n);
       swapl(&rep.length, n);
       swapl(&rep.number, n);
    }
    WriteToClient(client, sizeof(xXineramaQueryScreensReply), (char *)&rep);

    if(!RADEONnoPanoramiXExtension) {
       xXineramaScreenInfo scratch;
       int i;

       for(i = 0; i < RADEONXineramaNumScreens; i++) {
	  scratch.x_org  = RADEONXineramadataPtr[i].x;
	  scratch.y_org  = RADEONXineramadataPtr[i].y;
	  scratch.width  = RADEONXineramadataPtr[i].width;
	  scratch.height = RADEONXineramadataPtr[i].height;
	  if(client->swapped) {
	     register int n;
	     swaps(&scratch.x_org, n);
	     swaps(&scratch.y_org, n);
	     swaps(&scratch.width, n);
    	     swaps(&scratch.height, n);
	  }
	  WriteToClient(client, sz_XineramaScreenInfo, (char *)&scratch);
       }
    }

    return client->noClientException;
}

static int
RADEONProcXineramaDispatch(ClientPtr client)
{
    REQUEST(xReq);
    switch (stuff->data)
    {
	case X_PanoramiXQueryVersion:
	     return RADEONProcXineramaQueryVersion(client);
	case X_PanoramiXGetState:
	     return RADEONProcXineramaGetState(client);
	case X_PanoramiXGetScreenCount:
	     return RADEONProcXineramaGetScreenCount(client);
	case X_PanoramiXGetScreenSize:
	     return RADEONProcXineramaGetScreenSize(client);
	case X_XineramaIsActive:
	     return RADEONProcXineramaIsActive(client);
	case X_XineramaQueryScreens:
	     return RADEONProcXineramaQueryScreens(client);
    }
    return BadRequest;
}

/* SProc */

static int
RADEONSProcXineramaQueryVersion (ClientPtr client)
{
	REQUEST(xPanoramiXQueryVersionReq);
	register int n;
	swaps(&stuff->length,n);
	REQUEST_SIZE_MATCH (xPanoramiXQueryVersionReq);
	return RADEONProcXineramaQueryVersion(client);
}

static int
RADEONSProcXineramaGetState(ClientPtr client)
{
	REQUEST(xPanoramiXGetStateReq);
	register int n;
 	swaps (&stuff->length, n);
	REQUEST_SIZE_MATCH(xPanoramiXGetStateReq);
	return RADEONProcXineramaGetState(client);
}

static int
RADEONSProcXineramaGetScreenCount(ClientPtr client)
{
	REQUEST(xPanoramiXGetScreenCountReq);
	register int n;
	swaps (&stuff->length, n);
	REQUEST_SIZE_MATCH(xPanoramiXGetScreenCountReq);
	return RADEONProcXineramaGetScreenCount(client);
}

static int
RADEONSProcXineramaGetScreenSize(ClientPtr client)
{
	REQUEST(xPanoramiXGetScreenSizeReq);
	register int n;
	swaps (&stuff->length, n);
	REQUEST_SIZE_MATCH(xPanoramiXGetScreenSizeReq);
	return RADEONProcXineramaGetScreenSize(client);
}

static int
RADEONSProcXineramaIsActive(ClientPtr client)
{
	REQUEST(xXineramaIsActiveReq);
	register int n;
	swaps (&stuff->length, n);
	REQUEST_SIZE_MATCH(xXineramaIsActiveReq);
	return RADEONProcXineramaIsActive(client);
}

static int
RADEONSProcXineramaQueryScreens(ClientPtr client)
{
	REQUEST(xXineramaQueryScreensReq);
	register int n;
	swaps (&stuff->length, n);
	REQUEST_SIZE_MATCH(xXineramaQueryScreensReq);
	return RADEONProcXineramaQueryScreens(client);
}

int
RADEONSProcXineramaDispatch(ClientPtr client)
{
    REQUEST(xReq);
    switch (stuff->data) {
	case X_PanoramiXQueryVersion:
	     return RADEONSProcXineramaQueryVersion(client);
	case X_PanoramiXGetState:
	     return RADEONSProcXineramaGetState(client);
	case X_PanoramiXGetScreenCount:
	     return RADEONSProcXineramaGetScreenCount(client);
	case X_PanoramiXGetScreenSize:
	     return RADEONSProcXineramaGetScreenSize(client);
	case X_XineramaIsActive:
	     return RADEONSProcXineramaIsActive(client);
	case X_XineramaQueryScreens:
	     return RADEONSProcXineramaQueryScreens(client);
    }
    return BadRequest;
}

static void
RADEONXineramaResetProc(ExtensionEntry* extEntry)
{
    if(RADEONXineramadataPtr) {
       Xfree(RADEONXineramadataPtr);
       RADEONXineramadataPtr = NULL;
    }
}

void
RADEONXineramaExtensionInit(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    Bool	success = FALSE;

    if(!(RADEONXineramadataPtr)) {

       if(!info->MergedFB) {
          RADEONnoPanoramiXExtension = TRUE;
          return;
       }

#ifdef PANORAMIX
       if(!noPanoramiXExtension) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       	     "Xinerama active, not initializing Radeon Pseudo-Xinerama\n");
          RADEONnoPanoramiXExtension = TRUE;
          return;
       }
#endif

       if(RADEONnoPanoramiXExtension) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       	      "Radeon Pseudo-Xinerama disabled\n");
          return;
       }

       if(info->CRT2Position == radeonClone) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       	     "Running MergedFB in Clone mode, Radeon Pseudo-Xinerama disabled\n");
          RADEONnoPanoramiXExtension = TRUE;
          return;
       }

       if(!(info->AtLeastOneNonClone)) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO,
       	     "Only Clone modes defined, Radeon Pseudo-Xinerama disabled\n");
          RADEONnoPanoramiXExtension = TRUE;
          return;
       }

       RADEONXineramaNumScreens = 2;

       while(RADEONXineramaGeneration != serverGeneration) {

	  info->XineramaExtEntry = AddExtension(PANORAMIX_PROTOCOL_NAME, 0,0,
					RADEONProcXineramaDispatch,
					RADEONSProcXineramaDispatch,
					RADEONXineramaResetProc,
					StandardMinorOpcode);

	  if(!info->XineramaExtEntry) break;

	  RADEONXineramaReqCode = (unsigned char)info->XineramaExtEntry->base;

	  if(!(RADEONXineramadataPtr = (RADEONXineramaData *)
	        xcalloc(RADEONXineramaNumScreens, sizeof(RADEONXineramaData)))) break;

	  RADEONXineramaGeneration = serverGeneration;
	  success = TRUE;
       }

       if(!success) {
          xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	    	"Failed to initialize Radeon Pseudo-Xinerama extension\n");
          RADEONnoPanoramiXExtension = TRUE;
          return;
       }

       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
    	  "Initialized Radeon Pseudo-Xinerama extension\n");

       info->RADEONXineramaVX = 0;
       info->RADEONXineramaVY = 0;

    }

    RADEONUpdateXineramaScreenInfo(pScrn);

}
/* End of PseudoXinerama */

static Bool
InRegion(int x, int y, region r)
{
    return (r.x0 <= x) && (x <= r.x1) && (r.y0 <= y) && (y <= r.y1);
}

void
RADEONMergePointerMoved(int scrnIndex, int x, int y)
{
  ScrnInfoPtr   pScrn1 = xf86Screens[scrnIndex];
  RADEONInfoPtr info = RADEONPTR(pScrn1);
  ScrnInfoPtr   pScrn2 = info->CRT2pScrn;
  region 	out, in1, in2, f2, f1;
  int 		deltax, deltay;

  f1.x0 = info->CRT1frameX0;
  f1.x1 = info->CRT1frameX1;
  f1.y0 = info->CRT1frameY0;
  f1.y1 = info->CRT1frameY1;
  f2.x0 = pScrn2->frameX0;
  f2.x1 = pScrn2->frameX1;
  f2.y0 = pScrn2->frameY0;
  f2.y1 = pScrn2->frameY1;

  /* Define the outer region. Crossing this causes all frames to move */
  out.x0 = pScrn1->frameX0;
  out.x1 = pScrn1->frameX1;
  out.y0 = pScrn1->frameY0;
  out.y1 = pScrn1->frameY1;

  /*
   * Define the inner sliding window. Being outsize both frames but
   * inside the outer clipping window will slide corresponding frame
   */
  in1 = out;
  in2 = out;
  switch(((RADEONMergedDisplayModePtr)info->CurrentLayout.mode->Private)->CRT2Position) {
     case radeonLeftOf:
        in1.x0 = f1.x0;
        in2.x1 = f2.x1;
        break;
     case radeonRightOf:
        in1.x1 = f1.x1;
        in2.x0 = f2.x0;
        break;
     case radeonBelow:
        in1.y1 = f1.y1;
        in2.y0 = f2.y0;
        break;
     case radeonAbove:
        in1.y0 = f1.y0;
        in2.y1 = f2.y1;
        break;
     case radeonClone:
        break;
  }

  deltay = 0;
  deltax = 0;

  if(InRegion(x, y, out)) {	/* inside outer region */

     /* xf86DrvMsg(0, X_INFO, "1: %d %d | %d %d %d %d | %d %d %d %d\n",
     	x, y, in1.x0, in1.x1, in1.y0, in1.y1, f1.x0, f1.x1, f1.y0, f1.y1); */

     if(InRegion(x, y, in1) && !InRegion(x, y, f1)) {
        REBOUND(f1.x0, f1.x1, x);
        REBOUND(f1.y0, f1.y1, y);
        deltax = 1;
	/* xf86DrvMsg(0, X_INFO, "2: %d %d | %d %d %d %d | %d %d %d %d\n",
     		x, y, in1.x0, in1.x1, in1.y0, in1.y1, f1.x0, f1.x1, f1.y0, f1.y1); */
     }
     if(InRegion(x, y, in2) && !InRegion(x, y, f2)) {
        REBOUND(f2.x0, f2.x1, x);
        REBOUND(f2.y0, f2.y1, y);
        deltax = 1;
     }

  } else {			/* outside outer region */

     /* xf86DrvMsg(0, X_INFO, "3: %d %d | %d %d %d %d | %d %d %d %d\n",
     	x, y, in1.x0, in1.x1, in1.y0, in1.y1, f1.x0, f1.x1, f1.y0, f1.y1);
     xf86DrvMsg(0, X_INFO, "3-out: %d %d %d %d\n",
     	out.x0, out.x1, out.y0, out.y1); */

     if(out.x0 > x) {
        deltax = x - out.x0;
     }
     if(out.x1 < x) {
        deltax = x - out.x1;
     }
     if(deltax) {
        pScrn1->frameX0 += deltax;
        pScrn1->frameX1 += deltax;
	f1.x0 += deltax;
        f1.x1 += deltax;
        f2.x0 += deltax;
        f2.x1 += deltax;
     }

     if(out.y0 > y) {
        deltay = y - out.y0;
     }
     if(out.y1 < y) {
        deltay = y - out.y1;
     }
     if(deltay) {
        pScrn1->frameY0 += deltay;
        pScrn1->frameY1 += deltay;
	f1.y0 += deltay;
        f1.y1 += deltay;
        f2.y0 += deltay;
        f2.y1 += deltay;
     }

     switch(((RADEONMergedDisplayModePtr)info->CurrentLayout.mode->Private)->CRT2Position) {
        case radeonLeftOf:
	   if(x >= f1.x0) { REBOUND(f1.y0, f1.y1, y); }
	   if(x <= f2.x1) { REBOUND(f2.y0, f2.y1, y); }
           break;
        case radeonRightOf:
	   if(x <= f1.x1) { REBOUND(f1.y0, f1.y1, y); }
	   if(x >= f2.x0) { REBOUND(f2.y0, f2.y1, y); }
           break;
        case radeonBelow:
	   if(y <= f1.y1) { REBOUND(f1.x0, f1.x1, x); }
	   if(y >= f2.y0) { REBOUND(f2.x0, f2.x1, x); }
           break;
        case radeonAbove:
	   if(y >= f1.y0) { REBOUND(f1.x0, f1.x1, x); }
	   if(y <= f2.y1) { REBOUND(f2.x0, f2.x1, x); }
           break;
        case radeonClone:
           break;
     }

  }

  if(deltax || deltay) {
     info->CRT1frameX0 = f1.x0;
     info->CRT1frameY0 = f1.y0;
     pScrn2->frameX0 = f2.x0;
     pScrn2->frameY0 = f2.y0;

     info->CRT1frameX1 = info->CRT1frameX0 + CDMPTR->CRT1->HDisplay - 1;
     info->CRT1frameY1 = info->CRT1frameY0 + CDMPTR->CRT1->VDisplay - 1;
     pScrn2->frameX1   = pScrn2->frameX0   + CDMPTR->CRT2->HDisplay - 1;
     pScrn2->frameY1   = pScrn2->frameY0   + CDMPTR->CRT2->VDisplay - 1;
     pScrn1->frameX1   = pScrn1->frameX0   + info->CurrentLayout.mode->HDisplay  - 1;
     pScrn1->frameY1   = pScrn1->frameY0   + info->CurrentLayout.mode->VDisplay  - 1;

     RADEONDoAdjustFrame(pScrn1, info->CRT1frameX0, info->CRT1frameY0, FALSE);
     RADEONDoAdjustFrame(pScrn1, pScrn2->frameX0, pScrn2->frameY0, TRUE);
  }
}

static void
RADEONAdjustFrameMergedHelper(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn1 = xf86Screens[scrnIndex];
    RADEONInfoPtr info = RADEONPTR(pScrn1);
    ScrnInfoPtr pScrn2 = info->CRT2pScrn;
    int VTotal = info->CurrentLayout.mode->VDisplay;
    int HTotal = info->CurrentLayout.mode->HDisplay;
    int VMax = VTotal;
    int HMax = HTotal;

    BOUND(x, 0, pScrn1->virtualX - HTotal);
    BOUND(y, 0, pScrn1->virtualY - VTotal);

    switch(SDMPTR(pScrn1)->CRT2Position) {
        case radeonLeftOf:
            pScrn2->frameX0 = x;
            BOUND(pScrn2->frameY0,   y, y + VMax - CDMPTR->CRT2->VDisplay);
            info->CRT1frameX0 = x + CDMPTR->CRT2->HDisplay;
            BOUND(info->CRT1frameY0, y, y + VMax - CDMPTR->CRT1->VDisplay);
            break;
        case radeonRightOf:
            info->CRT1frameX0 = x;
            BOUND(info->CRT1frameY0, y, y + VMax - CDMPTR->CRT1->VDisplay);
            pScrn2->frameX0 = x + CDMPTR->CRT1->HDisplay;
            BOUND(pScrn2->frameY0,   y, y + VMax - CDMPTR->CRT2->VDisplay);
            break;
        case radeonAbove:
            BOUND(pScrn2->frameX0,   x, x + HMax - CDMPTR->CRT2->HDisplay);
            pScrn2->frameY0 = y;
            BOUND(info->CRT1frameX0, x, x + HMax - CDMPTR->CRT1->HDisplay);
            info->CRT1frameY0 = y + CDMPTR->CRT2->VDisplay;
            break;
        case radeonBelow:
            BOUND(info->CRT1frameX0, x, x + HMax - CDMPTR->CRT1->HDisplay);
            info->CRT1frameY0 = y;
            BOUND(pScrn2->frameX0,   x, x + HMax - CDMPTR->CRT2->HDisplay);
            pScrn2->frameY0 = y + CDMPTR->CRT1->VDisplay;
            break;
        case radeonClone:
            BOUND(info->CRT1frameX0, x, x + HMax - CDMPTR->CRT1->HDisplay);
            BOUND(info->CRT1frameY0, y, y + VMax - CDMPTR->CRT1->VDisplay);
            BOUND(pScrn2->frameX0,   x, x + HMax - CDMPTR->CRT2->HDisplay);
            BOUND(pScrn2->frameY0,   y, y + VMax - CDMPTR->CRT2->VDisplay);
            break;
    }

    BOUND(info->CRT1frameX0, 0, pScrn1->virtualX - CDMPTR->CRT1->HDisplay);
    BOUND(info->CRT1frameY0, 0, pScrn1->virtualY - CDMPTR->CRT1->VDisplay);
    BOUND(pScrn2->frameX0,   0, pScrn1->virtualX - CDMPTR->CRT2->HDisplay);
    BOUND(pScrn2->frameY0,   0, pScrn1->virtualY - CDMPTR->CRT2->VDisplay);
    
    pScrn1->frameX0 = x;
    pScrn1->frameY0 = y;

    info->CRT1frameX1 = info->CRT1frameX0 + CDMPTR->CRT1->HDisplay - 1;
    info->CRT1frameY1 = info->CRT1frameY0 + CDMPTR->CRT1->VDisplay - 1;
    pScrn2->frameX1   = pScrn2->frameX0   + CDMPTR->CRT2->HDisplay - 1;
    pScrn2->frameY1   = pScrn2->frameY0   + CDMPTR->CRT2->VDisplay - 1;
    pScrn1->frameX1   = pScrn1->frameX0   + info->CurrentLayout.mode->HDisplay  - 1;
    pScrn1->frameY1   = pScrn1->frameY0   + info->CurrentLayout.mode->VDisplay  - 1;
/*
    RADEONDoAdjustFrame(pScrn1, info->CRT1frameX0, info->CRT1frameY0, FALSE);
    RADEONDoAdjustFrame(pScrn1, pScrn2->frameX0, pScrn2->frameY0, TRUE);
*/
}

void
RADEONAdjustFrameMerged(int scrnIndex, int x, int y, int flags)
{
    ScrnInfoPtr pScrn1 = xf86Screens[scrnIndex];
    RADEONInfoPtr info = RADEONPTR(pScrn1);
    ScrnInfoPtr pScrn2 = info->CRT2pScrn;

    RADEONAdjustFrameMergedHelper(scrnIndex, x, y, flags);
    RADEONDoAdjustFrame(pScrn1, info->CRT1frameX0, info->CRT1frameY0, FALSE);
    RADEONDoAdjustFrame(pScrn1, pScrn2->frameX0, pScrn2->frameY0, TRUE);
}

void
RADEONMergedFBSetDpi(ScrnInfoPtr pScrn1, ScrnInfoPtr pScrn2, RADEONScrn2Rel srel)
{
   RADEONInfoPtr      info       = RADEONPTR(pScrn1);
   MessageType from = X_DEFAULT;
   xf86MonPtr DDC1 = (xf86MonPtr)(pScrn1->monitor->DDC);
   xf86MonPtr DDC2 = (xf86MonPtr)(pScrn2->monitor->DDC);
   int ddcWidthmm = 0, ddcHeightmm = 0;
   const char *dsstr = "MergedFB: Display dimensions: (%d, %d) mm\n";

   /* This sets the DPI for MergedFB mode. The problem is that
    * this can never be exact, because the output devices may
    * have different dimensions. This function tries to compromise
    * through a few assumptions, and it just calculates an average DPI
    * value for both monitors.
    */

   /* Given DisplaySize should regard BOTH monitors */
   pScrn1->widthmm = pScrn1->monitor->widthmm;
   pScrn1->heightmm = pScrn1->monitor->heightmm;

   /* Get DDC display size; if only either CRT1 or CRT2 provided these,
    * assume equal dimensions for both, otherwise add dimensions
    */
   if( (DDC1 && (DDC1->features.hsize > 0 && DDC1->features.vsize > 0)) &&
       (DDC2 && (DDC2->features.hsize > 0 && DDC2->features.vsize > 0)) ) {
      ddcWidthmm = max(DDC1->features.hsize, DDC2->features.hsize) * 10;
      ddcHeightmm = max(DDC1->features.vsize, DDC2->features.vsize) * 10;
      switch(srel) {
      case radeonLeftOf:
      case radeonRightOf:
         ddcWidthmm = (DDC1->features.hsize + DDC2->features.hsize) * 10;
	 break;
      case radeonAbove:
      case radeonBelow:
         ddcHeightmm = (DDC1->features.vsize + DDC2->features.vsize) * 10;
      default:
	 break;
      }

   } else if(DDC1 && (DDC1->features.hsize > 0 && DDC1->features.vsize > 0)) {
      ddcWidthmm = DDC1->features.hsize * 10;
      ddcHeightmm = DDC1->features.vsize * 10;
      switch(srel) {
      case radeonLeftOf:
      case radeonRightOf:
         ddcWidthmm *= 2;
	 break;
      case radeonAbove:
      case radeonBelow:
         ddcHeightmm *= 2;
      default:
	 break;
      }
   } else if(DDC2 && (DDC2->features.hsize > 0 && DDC2->features.vsize > 0) ) {
      ddcWidthmm = DDC2->features.hsize * 10;
      ddcHeightmm = DDC2->features.vsize * 10;
      switch(srel) {
      case radeonLeftOf:
      case radeonRightOf:
         ddcWidthmm *= 2;
	 break;
      case radeonAbove:
      case radeonBelow:
         ddcHeightmm *= 2;
      default:
	 break;
      }
   }

   if(monitorResolution > 0) {

      /* Set command line given values (overrules given options) */
      pScrn1->xDpi = monitorResolution;
      pScrn1->yDpi = monitorResolution;
      from = X_CMDLINE;

   } else if(info->MergedFBXDPI) {

      /* Set option-wise given values (overrule DisplaySize) */
      pScrn1->xDpi = info->MergedFBXDPI;
      pScrn1->yDpi = info->MergedFBYDPI;
      from = X_CONFIG;

   } else if(pScrn1->widthmm > 0 || pScrn1->heightmm > 0) {

      /* Set values calculated from given DisplaySize */
      from = X_CONFIG;
      if(pScrn1->widthmm > 0) {
	 pScrn1->xDpi = (int)((double)pScrn1->virtualX * 25.4 / pScrn1->widthmm);
      }
      if(pScrn1->heightmm > 0) {
	 pScrn1->yDpi = (int)((double)pScrn1->virtualY * 25.4 / pScrn1->heightmm);
      }
      xf86DrvMsg(pScrn1->scrnIndex, from, dsstr, pScrn1->widthmm, pScrn1->heightmm);

    } else if(ddcWidthmm && ddcHeightmm) {

      /* Set values from DDC-provided display size */
      from = X_PROBED;
      xf86DrvMsg(pScrn1->scrnIndex, from, dsstr, ddcWidthmm, ddcHeightmm );
      pScrn1->widthmm = ddcWidthmm;
      pScrn1->heightmm = ddcHeightmm;
      if(pScrn1->widthmm > 0) {
	 pScrn1->xDpi = (int)((double)pScrn1->virtualX * 25.4 / pScrn1->widthmm);
      }
      if(pScrn1->heightmm > 0) {
	 pScrn1->yDpi = (int)((double)pScrn1->virtualY * 25.4 / pScrn1->heightmm);
      }

    } else {

      pScrn1->xDpi = pScrn1->yDpi = DEFAULT_DPI;

    }

    /* Sanity check */
    if(pScrn1->xDpi > 0 && pScrn1->yDpi <= 0)
       pScrn1->yDpi = pScrn1->xDpi;
    if(pScrn1->yDpi > 0 && pScrn1->xDpi <= 0)
       pScrn1->xDpi = pScrn1->yDpi;

    pScrn2->xDpi = pScrn1->xDpi;
    pScrn2->yDpi = pScrn1->yDpi;

    xf86DrvMsg(pScrn1->scrnIndex, from, "MergedFB: DPI set to (%d, %d)\n",
	       pScrn1->xDpi, pScrn1->yDpi);
}

/* radeon cursor helpers */
static void
RADEONChooseCursorCRTC(ScrnInfoPtr pScrn1, int x, int y)
{
    RADEONInfoPtr      info       = RADEONPTR(pScrn1);
    unsigned char     *RADEONMMIO = info->MMIO;
    RADEONScrn2Rel srel = 
        ((RADEONMergedDisplayModePtr)info->CurrentLayout.mode->Private)->CRT2Position;
    ScrnInfoPtr pScrn2 = info->CRT2pScrn;

    if (srel == radeonClone) {
	/* show cursor 2 */
	OUTREGP(RADEON_CRTC2_GEN_CNTL, RADEON_CRTC2_CUR_EN,
		~RADEON_CRTC2_CUR_EN);
	/* show cursor 1 */
	OUTREGP(RADEON_CRTC_GEN_CNTL, RADEON_CRTC_CUR_EN,
		~RADEON_CRTC_CUR_EN);
    }
    else {
	if (((x >= pScrn1->frameX0) && (x <= pScrn1->frameX1)) &&
       	    ((y >= pScrn1->frameY0) && (y <= pScrn1->frameY1))) {
		/* hide cursor 2 */
		OUTREGP(RADEON_CRTC2_GEN_CNTL, 0, ~RADEON_CRTC2_CUR_EN);
		/* show cursor 1 */
		OUTREGP(RADEON_CRTC_GEN_CNTL, RADEON_CRTC_CUR_EN,
			~RADEON_CRTC_CUR_EN);
	}
	if (((x >= pScrn2->frameX0) && (x <= pScrn2->frameX1)) &&
       	    ((y >= pScrn2->frameY0) && (y <= pScrn2->frameY1))) {
		/* hide cursor 1 */
		OUTREGP(RADEON_CRTC_GEN_CNTL, 0, ~RADEON_CRTC_CUR_EN);
		/* show cursor 2 */
		OUTREGP(RADEON_CRTC2_GEN_CNTL, RADEON_CRTC2_CUR_EN,
			~RADEON_CRTC2_CUR_EN);
	}
    }
}

void
RADEONSetCursorPositionMerged(ScrnInfoPtr pScrn, int x, int y)
{
    RADEONInfoPtr      info       = RADEONPTR(pScrn);
    unsigned char     *RADEONMMIO = info->MMIO;
    xf86CursorInfoPtr  cursor     = info->cursor;
    int                xorigin    = 0;
    int                yorigin    = 0;
    int		       stride     = 256;
    ScrnInfoPtr    pScrn2 = info->CRT2pScrn;
    DisplayModePtr mode1 = CDMPTR->CRT1;
    DisplayModePtr mode2 = CDMPTR->CRT2;
    int            x1, y1, x2, y2;
    int                total_y1    = pScrn->frameY1 - pScrn->frameY0;
    int                total_y2    = pScrn2->frameY1 - pScrn2->frameY0;

    if (x < 0)                        xorigin = -x+1;
    if (y < 0)                        yorigin = -y+1;
    /* if (y > total_y)                  y       = total_y; */
    if (xorigin >= cursor->MaxWidth)  xorigin = cursor->MaxWidth - 1;
    if (yorigin >= cursor->MaxHeight) yorigin = cursor->MaxHeight - 1;

    x += pScrn->frameX0;
    y += pScrn->frameY0;

    x1 = x - info->CRT1frameX0;
    y1 = y - info->CRT1frameY0;

    x2 = x - pScrn2->frameX0;
    y2 = y - pScrn2->frameY0;

    if (y1 > total_y1)
	y1       = total_y1;
    if (y2 > total_y2)                  
	y2       = total_y2;

    if(mode1->Flags & V_INTERLACE)
	y1 /= 2;
    else if(mode1->Flags & V_DBLSCAN)
	y1 *= 2;

    if(mode2->Flags & V_INTERLACE)
	y2 /= 2;
    else if(mode2->Flags & V_DBLSCAN)
	y2 *= 2;

    if (x < 0)
	x = 0;
    if (y < 0)
	y = 0;

    RADEONChooseCursorCRTC(pScrn, x, y);

		/* cursor1 */
    OUTREG(RADEON_CUR_HORZ_VERT_OFF,  (RADEON_CUR_LOCK
				   | (xorigin << 16)
				   | yorigin));
    OUTREG(RADEON_CUR_HORZ_VERT_POSN, (RADEON_CUR_LOCK
				   | ((xorigin ? 0 : x1) << 16)
				   | (yorigin ? 0 : y1)));
    OUTREG(RADEON_CUR_OFFSET, info->cursor_start + yorigin * stride);
		/* cursor2 */
    OUTREG(RADEON_CUR2_HORZ_VERT_OFF,  (RADEON_CUR2_LOCK
				    | (xorigin << 16)
				    | yorigin));
    OUTREG(RADEON_CUR2_HORZ_VERT_POSN, (RADEON_CUR2_LOCK
				    | ((xorigin ? 0 : x2) << 16)
				    | (yorigin ? 0 : y2)));
    OUTREG(RADEON_CUR2_OFFSET,
	       info->cursor_start + yorigin * stride);

}

/* radeon Xv helpers */

/* choose the crtc for the overlay for mergedfb based on the location
   of the output window and the orientation of the crtcs */

void
RADEONChooseOverlayCRTC(
    ScrnInfoPtr pScrn,
    BoxPtr dstBox
) {
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONScrn2Rel srel = 
        ((RADEONMergedDisplayModePtr)info->CurrentLayout.mode->Private)->CRT2Position;

    if (srel == radeonLeftOf) {
        if (dstBox->x1 >= info->CRT2pScrn->frameX1)
            info->OverlayOnCRTC2 = FALSE;
        else
            info->OverlayOnCRTC2 = TRUE;
    }
    if (srel == radeonRightOf) {
        if (dstBox->x2 <= info->CRT2pScrn->frameX0)
            info->OverlayOnCRTC2 = FALSE;
        else
            info->OverlayOnCRTC2 = TRUE;
    }
    if (srel == radeonAbove) {
        if (dstBox->y1 >= info->CRT2pScrn->frameY1)
            info->OverlayOnCRTC2 = FALSE;
        else
            info->OverlayOnCRTC2 = TRUE;
    }
    if (srel == radeonBelow) {
	if (dstBox->y2 <= info->CRT2pScrn->frameY0)
            info->OverlayOnCRTC2 = FALSE;
        else
            info->OverlayOnCRTC2 = TRUE;
    }
}
