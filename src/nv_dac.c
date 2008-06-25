/*
 * Copyright 2003 NVIDIA, Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nv_include.h"

static int
NVDACPanelTweaks(NVPtr pNv, NVRegPtr state)
{
   int tweak = 0;

   if(pNv->usePanelTweak) {
       tweak = pNv->PanelTweak;
   } else {
       /* begin flat panel hacks */
       /* This is unfortunate, but some chips need this register
          tweaked or else you get artifacts where adjacent pixels are
          swapped.  There are no hard rules for what to set here so all
          we can do is experiment and apply hacks. */

       if(((pNv->Chipset & 0xffff) == 0x0328) && (state->bpp == 32)) {
          /* At least one NV34 laptop needs this workaround. */
          tweak = -1;
       }

       if((pNv->Chipset & 0xfff0) == CHIPSET_NV31) {
          tweak = 1;
       }
       /* end flat panel hacks */
   }

   return tweak;
}

Bool
NVDACInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    int i;
    int horizDisplay    = (mode->CrtcHDisplay/8)   - 1;
    int horizStart      = (mode->CrtcHSyncStart/8) - 1;
    int horizEnd        = (mode->CrtcHSyncEnd/8)   - 1;
    int horizTotal      = (mode->CrtcHTotal/8)     - 5;
    int horizBlankStart = (mode->CrtcHDisplay/8)   - 1;
    int horizBlankEnd   = (mode->CrtcHTotal/8)     - 1;
    int vertDisplay     =  mode->CrtcVDisplay      - 1;
    int vertStart       =  mode->CrtcVSyncStart    - 1;
    int vertEnd         =  mode->CrtcVSyncEnd      - 1;
    int vertTotal       =  mode->CrtcVTotal        - 2;
    int vertBlankStart  =  mode->CrtcVDisplay      - 1;
    int vertBlankEnd    =  mode->CrtcVTotal        - 1;
   
    NVPtr pNv = NVPTR(pScrn);
    NVRegPtr nvReg = &pNv->ModeReg;
    vgaRegPtr   pVga;

    /*
     * Initialize all of the generic VGA registers.  Don't bother with
     * VGA_FIX_SYNC_PULSES, given the relevant CRTC settings are overridden
     * below.  Ditto for the KGA workaround.
     */
    if (!vgaHWInit(pScrn, mode))
        return(FALSE);

    pVga = &VGAHWPTR(pScrn)->ModeReg;

    /*
     * Set all CRTC values.
     */

    if(mode->Flags & V_INTERLACE) 
        vertTotal |= 1;

    if(pNv->FlatPanel == 1) {
	vertStart = vertTotal - 3;  
	vertEnd = vertTotal - 2;
	vertBlankStart = vertStart;
	horizStart = horizTotal - 5;
	horizEnd = horizTotal - 2;   
	horizBlankEnd = horizTotal + 4;   
	if ( ( pNv->Architecture == NV_ARCH_40 && ((pNv->Chipset & 0xfff0) == CHIPSET_NV40) ) || pNv->Architecture == NV_ARCH_30 || pNv->Architecture == NV_ARCH_20 || pNv->Architecture == NV_ARCH_10 )	{ 
		/* This reportedly works around Xv some overlay bandwidth problems*/
		horizTotal += 2;
		}
    }

    pVga->CRTC[0x0]  = Set8Bits(horizTotal);
    pVga->CRTC[0x1]  = Set8Bits(horizDisplay);
    pVga->CRTC[0x2]  = Set8Bits(horizBlankStart);
    pVga->CRTC[0x3]  = SetBitField(horizBlankEnd,4:0,4:0) 
                       | SetBit(7);
    pVga->CRTC[0x4]  = Set8Bits(horizStart);
    pVga->CRTC[0x5]  = SetBitField(horizBlankEnd,5:5,7:7)
                       | SetBitField(horizEnd,4:0,4:0);
    pVga->CRTC[0x6]  = SetBitField(vertTotal,7:0,7:0);
    pVga->CRTC[0x7]  = SetBitField(vertTotal,8:8,0:0)
                       | SetBitField(vertDisplay,8:8,1:1)
                       | SetBitField(vertStart,8:8,2:2)
                       | SetBitField(vertBlankStart,8:8,3:3)
                       | SetBit(4)
                       | SetBitField(vertTotal,9:9,5:5)
                       | SetBitField(vertDisplay,9:9,6:6)
                       | SetBitField(vertStart,9:9,7:7);
    pVga->CRTC[0x9]  = SetBitField(vertBlankStart,9:9,5:5)
                       | SetBit(6)
                       | ((mode->Flags & V_DBLSCAN) ? 0x80 : 0x00);
    pVga->CRTC[0x10] = Set8Bits(vertStart);
    pVga->CRTC[0x11] = SetBitField(vertEnd,3:0,3:0) | SetBit(5);
    pVga->CRTC[0x12] = Set8Bits(vertDisplay);
    pVga->CRTC[0x13] = ((pScrn->displayWidth/8)*(pScrn->bitsPerPixel/8));
    pVga->CRTC[0x15] = Set8Bits(vertBlankStart);
    pVga->CRTC[0x16] = Set8Bits(vertBlankEnd);

    pVga->Attribute[0x10] = 0x01;

    if(pNv->Television)
       pVga->Attribute[0x11] = 0x00;

    nvReg->screen = SetBitField(horizBlankEnd,6:6,4:4)
                  | SetBitField(vertBlankStart,10:10,3:3)
                  | SetBitField(vertStart,10:10,2:2)
                  | SetBitField(vertDisplay,10:10,1:1)
                  | SetBitField(vertTotal,10:10,0:0);

    nvReg->horiz  = SetBitField(horizTotal,8:8,0:0) 
                  | SetBitField(horizDisplay,8:8,1:1)
                  | SetBitField(horizBlankStart,8:8,2:2)
                  | SetBitField(horizStart,8:8,3:3);

    nvReg->extra  = SetBitField(vertTotal,11:11,0:0)
                    | SetBitField(vertDisplay,11:11,2:2)
                    | SetBitField(vertStart,11:11,4:4)
                    | SetBitField(vertBlankStart,11:11,6:6);

    if(mode->Flags & V_INTERLACE) {
       horizTotal = (horizTotal >> 1) & ~1;
       nvReg->interlace = Set8Bits(horizTotal);
       nvReg->horiz |= SetBitField(horizTotal,8:8,4:4);
    } else {
       nvReg->interlace = 0xff;  /* interlace off */
    }


    /*
     * Initialize DAC palette.
     */
    if(pScrn->bitsPerPixel != 8 )
    {
        for (i = 0; i < 256; i++)
        {
            pVga->DAC[i*3]     = i;
            pVga->DAC[(i*3)+1] = i;
            pVga->DAC[(i*3)+2] = i;
        }
    }
    
    /*
     * Calculate the extended registers.
     */

    if (pScrn->depth < 24) 
	i = pScrn->depth;
    else i = 32;

    if(pNv->Architecture >= NV_ARCH_10)
	pNv->CURSOR = (CARD32 *)pNv->Cursor->map;

    NVCalcStateExt(pNv, 
                    nvReg,
                    i,
                    pScrn->displayWidth,
                    mode->CrtcHDisplay,
                    pScrn->virtualY,
                    mode->Clock,
                    mode->Flags);

    nvReg->scale = nvReadCurRAMDAC(pNv, NV_RAMDAC_FP_CONTROL) & 0xfff000ff;
    if(pNv->FlatPanel == 1) {
       nvReg->pixel |= (1 << 7);
       if(!pNv->fpScaler || (pNv->fpWidth <= mode->HDisplay)
                         || (pNv->fpHeight <= mode->VDisplay))
       {
           nvReg->scale |= (1 << 8) ;
       }
       nvReg->crtcSync = nvReadCurRAMDAC(pNv, NV_RAMDAC_FP_HCRTC);
       nvReg->crtcSync += NVDACPanelTweaks(pNv, nvReg);
    }

    nvReg->vpll = nvReg->pll;
    nvReg->vpll2 = nvReg->pll;
    nvReg->vpllB = nvReg->pllB;
    nvReg->vpll2B = nvReg->pllB;

    nvReg->fifo = nvReadCurVGA(pNv, 0x1c) & ~(1<<5);

    if(pNv->crtc_active[1]) {
       nvReg->head  = NVReadCRTC(pNv, 0, NV_CRTC_FSEL) & ~0x00001000;
       nvReg->head2 = NVReadCRTC(pNv, 1, NV_CRTC_FSEL) | 0x00001000;
       nvReg->crtcOwner = 3;
       nvReg->pllsel |= 0x20000800;
       nvReg->vpll = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL);
       if(pNv->twoStagePLL) 
          nvReg->vpllB = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL_B);
    } else if(pNv->twoHeads) {
       nvReg->head  =  NVReadCRTC(pNv, 0, NV_CRTC_FSEL) | 0x00001000;
       nvReg->head2 =  NVReadCRTC(pNv, 1, NV_CRTC_FSEL) & ~0x00001000;
       nvReg->crtcOwner = 0;
       nvReg->vpll2 = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2);
       if(pNv->twoStagePLL) 
          nvReg->vpll2B = NVReadRAMDAC(pNv, 0, NV_RAMDAC_VPLL2_B);
    }

    nvReg->cursorConfig = 0x00000100;
    if(mode->Flags & V_DBLSCAN)
       nvReg->cursorConfig |= (1 << 4);
    if(pNv->alphaCursor) {
        if((pNv->Chipset & 0x0ff0) != CHIPSET_NV11) 
           nvReg->cursorConfig |= 0x04011000;
        else
           nvReg->cursorConfig |= 0x14011000;
        nvReg->general |= (1 << 29);
    } else
       nvReg->cursorConfig |= 0x02000000;

    if(pNv->twoHeads) {
        if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
           nvReg->dither = nvReadCurRAMDAC(pNv, NV_RAMDAC_DITHER_NV11) & ~0x00010000;
           if(pNv->FPDither)
              nvReg->dither |= 0x00010000;
        } else {
           nvReg->dither = nvReadCurRAMDAC(pNv, NV_RAMDAC_FP_DITHER) & ~1;
           if(pNv->FPDither)
              nvReg->dither |= 1;
        } 
    }

    nvReg->timingH = 0;
    nvReg->timingV = 0;
    nvReg->displayV = mode->CrtcVDisplay;

    return (TRUE);
}

void 
NVDACRestore(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, NVRegPtr nvReg,
             Bool primary)
{
    int restore = VGA_SR_MODE;

    if(primary) restore |= VGA_SR_CMAP | VGA_SR_FONTS;
    NVLoadStateExt(pScrn, nvReg);
#if defined(__powerpc__)
    restore &= ~VGA_SR_FONTS;
#endif
    vgaHWRestore(pScrn, vgaReg, restore);
}

/*
 * NVDACSave
 *
 * This function saves the video state.
 */
void
NVDACSave(ScrnInfoPtr pScrn, vgaRegPtr vgaReg, NVRegPtr nvReg,
          Bool saveFonts)
{
    NVPtr pNv = NVPTR(pScrn);

#if defined(__powerpc__)
    saveFonts = FALSE;
#endif

    vgaHWSave(pScrn, vgaReg, VGA_SR_CMAP | VGA_SR_MODE | 
                             (saveFonts? VGA_SR_FONTS : 0));
    NVUnloadStateExt(pNv, nvReg);

	/* can't read this reliably on NV11 */
	if((pNv->Chipset & 0x0ff0) == CHIPSET_NV11) {
		/* 0 if inactive -> crtc0 is active, otherwise 1 */
		nvReg->crtcOwner = pNv->crtc_active[1];
	}
}

#define DEPTH_SHIFT(val, w) ((val << (8 - w)) | (val >> ((w << 1) - 8)))
#define MAKE_INDEX(in, w) (DEPTH_SHIFT(in, w) * 3)

void
NVDACLoadPalette(ScrnInfoPtr pScrn, int numColors, int *indices, LOCO *colors,
                 VisualPtr pVisual )
{
    int i, index;
    vgaRegPtr pVga = &VGAHWPTR(pScrn)->ModeReg;

    switch(pScrn->depth) {
    case 15:
        for(i = 0; i < numColors; i++) {
            index = indices[i];
            pVga->DAC[MAKE_INDEX(index, 5) + 0] = colors[index].red;
            pVga->DAC[MAKE_INDEX(index, 5) + 1] = colors[index].green;
            pVga->DAC[MAKE_INDEX(index, 5) + 2] = colors[index].blue;
        }
        break;
    case 16:
        for(i = 0; i < numColors; i++) {
            index = indices[i];
            pVga->DAC[MAKE_INDEX(index, 6) + 1] = colors[index].green;
	    if(index < 32) {
            	pVga->DAC[MAKE_INDEX(index, 5) + 0] = colors[index].red;
            	pVga->DAC[MAKE_INDEX(index, 5) + 2] = colors[index].blue;
	    }
        }
        break;
    default:
	for(i = 0; i < numColors; i++) {
            index = indices[i];
            pVga->DAC[index*3]     = colors[index].red;
            pVga->DAC[(index*3)+1] = colors[index].green;
            pVga->DAC[(index*3)+2] = colors[index].blue;
	}
	break;
    }
    vgaHWRestore(pScrn, pVga, VGA_SR_CMAP);
}

/*
 * DDC1 support only requires DDC_SDA_MASK,
 * DDC2 support requires DDC_SDA_MASK and DDC_SCL_MASK
 */
#define DDC_SDA_READ_MASK  (1 << 3)
#define DDC_SCL_READ_MASK  (1 << 2)
#define DDC_SDA_WRITE_MASK (1 << 4)
#define DDC_SCL_WRITE_MASK (1 << 5)

static void
NV_I2CGetBits(I2CBusPtr b, int *clock, int *data)
{
    NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
    unsigned char val;

    /* Get the result. */
    val = nvReadCurVGA(pNv, pNv->DDCBase);

    *clock = (val & DDC_SCL_READ_MASK) != 0;
    *data  = (val & DDC_SDA_READ_MASK) != 0;
}

static void
NV_I2CPutBits(I2CBusPtr b, int clock, int data)
{
    NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
    unsigned char val;

    val = nvReadCurVGA(pNv, pNv->DDCBase + 1) & 0xf0;
    if (clock)
        val |= DDC_SCL_WRITE_MASK;
    else
        val &= ~DDC_SCL_WRITE_MASK;

    if (data)
        val |= DDC_SDA_WRITE_MASK;
    else
        val &= ~DDC_SDA_WRITE_MASK;

    nvWriteCurVGA(pNv, pNv->DDCBase + 1, val | 0x1);
}

Bool
NVDACi2cInit(ScrnInfoPtr pScrn)
{
    NVPtr pNv = NVPTR(pScrn);
    I2CBusPtr I2CPtr;

    I2CPtr = xf86CreateI2CBusRec();
    if(!I2CPtr) return FALSE;

    pNv->I2C = I2CPtr;

    I2CPtr->BusName    = "DDC";
    I2CPtr->scrnIndex  = pScrn->scrnIndex;
    I2CPtr->I2CPutBits = NV_I2CPutBits;
    I2CPtr->I2CGetBits = NV_I2CGetBits;
    I2CPtr->AcknTimeout = 5;

    if (!xf86I2CBusInit(I2CPtr)) {
        return FALSE;
    }
    return TRUE;
}

