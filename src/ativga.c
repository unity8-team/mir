/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/ativga.c,v 1.20 2003/04/23 21:51:31 tsi Exp $ */
/*
 * Copyright 1997 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "ati.h"
#include "atiadapter.h"
#include "atichip.h"
#include "atimono.h"
#include "atistruct.h"
#include "ativga.h"
#include "ativgaio.h"

#ifndef DPMS_SERVER
# define DPMS_SERVER
#endif
#include <X11/extensions/dpms.h>

#ifndef AVOID_CPIO

/*
 * ATIVGAPreInit --
 *
 * This function is called to set up VGA-related data that is common to all
 * video modes generated by the driver.
 */
void
ATIVGAPreInit
(
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{
    int Index;

    /* Initialise sequencer register values */
    pATIHW->seq[0] = 0x03U;
    if (pATI->depth == 1)
        pATIHW->seq[2] = 0x01U << BIT_PLANE;
    else
        pATIHW->seq[2] = 0x0FU;
    if (pATI->depth <= 4)
        pATIHW->seq[4] = 0x06U;
    else if (pATI->Adapter == ATI_ADAPTER_VGA)
        pATIHW->seq[4] = 0x0EU;
    else
        pATIHW->seq[4] = 0x0AU;

    /* Initialise CRTC register values */
    if ((pATI->depth >= 8) &&
        ((pATI->Chip >= ATI_CHIP_264CT) ||
         (pATI->CPIO_VGAWonder &&
          (pATI->Chip <= ATI_CHIP_18800_1) &&
          (pATI->VideoRAM == 256))))
        pATIHW->crt[19] = pATI->displayWidth >> 3;
    else
        pATIHW->crt[19] = pATI->displayWidth >> 4;
    if ((pATI->depth >= 8) && (pATI->Adapter == ATI_ADAPTER_VGA))
        pATIHW->crt[23] = 0xC3U;
    else
        pATIHW->crt[23] = 0xE3U;
    pATIHW->crt[24] = 0xFFU;

    /* Initialise attribute controller register values */
    if (pATI->depth == 1)
    {
        Bool FlipPixels = xf86GetFlipPixels();

        for (Index = 0;  Index < 16;  Index++)
            if (((Index & (0x01U << BIT_PLANE)) != 0) != FlipPixels)
                pATIHW->attr[Index] = MONO_WHITE;
            else
                pATIHW->attr[Index] = MONO_BLACK;
        pATIHW->attr[16] = 0x01U;
        pATIHW->attr[17] = MONO_OVERSCAN;
    }
    else
    {
        for (Index = 0;  Index < 16;  Index++)
            pATIHW->attr[Index] = Index;
        if (pATI->depth <= 4)
            pATIHW->attr[16] = 0x81U;
        else if (pATI->Adapter == ATI_ADAPTER_VGA)
            pATIHW->attr[16] = 0x41U;
        else
            pATIHW->attr[16] = 0x01U;
        pATIHW->attr[17] = 0xFFU;
    }
    pATIHW->attr[18] = 0x0FU;

    /* Initialise graphics controller register values */
    if (pATI->depth == 1)
        pATIHW->gra[4] = BIT_PLANE;
    else if (pATI->depth <= 4)
        pATIHW->gra[5] = 0x02U;
    else if (pATI->Chip >= ATI_CHIP_264CT)
        pATIHW->gra[5] = 0x40U;
    if (pATI->UseSmallApertures && (pATI->Chip >= ATI_CHIP_264CT) &&
        ((pATI->Chip >= ATI_CHIP_264VT) || !pATI->LinearBase))
        pATIHW->gra[6] = 0x01U;         /* 128kB aperture */
    else
        pATIHW->gra[6] = 0x05U;         /* 64kB aperture */
    pATIHW->gra[7] = 0x0FU;
    pATIHW->gra[8] = 0xFFU;
}

/*
 * ATIVGASave --
 *
 * This function is called to save the VGA portion of the current video state.
 */
void
ATIVGASave
(
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{
    int Index;

    /* Save miscellaneous output register */
    pATIHW->genmo = inb(R_GENMO);
    ATISetVGAIOBase(pATI, pATIHW->genmo);

    /* Save sequencer registers */
    for (Index = 0;  Index < NumberOf(pATIHW->seq);  Index++)
        pATIHW->seq[Index] = GetReg(SEQX, Index);

    /* Save CRTC registers */
    for (Index = 0;  Index < NumberOf(pATIHW->crt);  Index++)
        pATIHW->crt[Index] = GetReg(CRTX(pATI->CPIO_VGABase), Index);

    /* Save attribute controller registers */
    for (Index = 0;  Index < NumberOf(pATIHW->attr);  Index++)
    {
        (void)inb(GENS1(pATI->CPIO_VGABase));   /* Reset flip-flop */
        pATIHW->attr[Index] = GetReg(ATTRX, Index);
    }

    /* Save graphics controller registers */
    for (Index = 0;  Index < NumberOf(pATIHW->gra);  Index++)
        pATIHW->gra[Index] = GetReg(GRAX, Index);
}

/*
 * ATIVGACalculate --
 *
 * This function fills in the VGA portion of an ATIHWRec.
 */
void
ATIVGACalculate
(
    ATIPtr         pATI,
    ATIHWPtr       pATIHW,
    DisplayModePtr pMode
)
{
    int Index, VDisplay;

    /* If not already done, adjust horizontal timings */
    if (!pMode->CrtcHAdjusted)
    {
        pMode->CrtcHAdjusted = TRUE;
        pMode->CrtcHDisplay = (pMode->HDisplay >> 3) - 1;
        pMode->CrtcHBlankStart = (pMode->HDisplay >> 3);
        if ((pATI->Chip == ATI_CHIP_18800_1) ||
            (pATI->Chip >= ATI_CHIP_264CT))
            pMode->CrtcHBlankStart--;
        pMode->CrtcHSyncStart = pMode->HSyncStart >> 3;
        pMode->CrtcHSyncEnd = pMode->HSyncEnd >> 3;
        pMode->CrtcHBlankEnd = (pMode->HTotal >> 3) - 1;
        pMode->CrtcHTotal = (pMode->HTotal >> 3) - 5;
        pMode->CrtcHSkew = pMode->HSkew;

        /* Check sync pulse width */
        Index = pMode->CrtcHSyncEnd - pMode->CrtcHSyncStart - 0x1F;
        if (Index > 0)
        {
            pMode->CrtcHSyncStart += Index / 2;
            pMode->CrtcHSyncEnd = pMode->CrtcHSyncStart + 0x1F;
        }

        /* Check blank pulse width */
        Index = pMode->CrtcHBlankEnd - pMode->CrtcHBlankStart - 0x3F;
        if (Index > 0)
        {
            if ((pMode->CrtcHBlankEnd - Index) > pMode->CrtcHSyncEnd)
            {
                pMode->CrtcHBlankStart += Index / 2;
                if (pMode->CrtcHBlankStart >= pMode->CrtcHSyncStart)
                    pMode->CrtcHBlankStart = pMode->CrtcHSyncStart - 1;
                pMode->CrtcHBlankEnd = pMode->CrtcHBlankStart + 0x3F;
            }
            else
            {
                Index -= 0x40;
                if (Index > 0)
                {
                    pMode->CrtcHBlankStart += Index / 2;
                    if (pMode->CrtcHBlankStart >= pMode->CrtcHSyncStart)
                        pMode->CrtcHBlankStart = pMode->CrtcHSyncStart - 1;
                    pMode->CrtcHBlankEnd = pMode->CrtcHBlankStart + 0x7F;
                }
            }
        }
    }

    /*
     * Because of the use of CRTC[23] bit 0x04's for vertical doubling, it is
     * necessary to always re-adjust vertical timings here.
     */
    pMode->CrtcVDisplay = pMode->VDisplay;
    pMode->CrtcVBlankStart = pMode->VDisplay;
    pMode->CrtcVSyncStart = pMode->VSyncStart;
    pMode->CrtcVSyncEnd = pMode->VSyncEnd;
    pMode->CrtcVBlankEnd = pMode->VTotal;
    pMode->CrtcVTotal = pMode->VTotal;

    /* Adjust for doublescanned modes */
    if (pMode->Flags & V_DBLSCAN)
    {
        pMode->CrtcVDisplay <<= 1;
        pMode->CrtcVBlankStart <<= 1;
        pMode->CrtcVSyncStart <<= 1;
        pMode->CrtcVSyncEnd <<= 1;
        pMode->CrtcVBlankEnd <<= 1;
        pMode->CrtcVTotal <<= 1;
    }

    /* Adjust for multiscanned modes */
    if (pMode->VScan > 1)
    {
        pMode->CrtcVDisplay *= pMode->VScan;
        pMode->CrtcVBlankStart *= pMode->VScan;
        pMode->CrtcVSyncStart *= pMode->VScan;
        pMode->CrtcVSyncEnd *= pMode->VScan;
        pMode->CrtcVBlankEnd *= pMode->VScan;
        pMode->CrtcVTotal *= pMode->VScan;
    }

    /* Set up miscellaneous output register value */
    pATIHW->genmo = 0x23U;
    if ((pMode->Flags & (V_PHSYNC | V_NHSYNC)) &&
        (pMode->Flags & (V_PVSYNC | V_NVSYNC)))
    {
        if (pMode->Flags & V_NHSYNC)
            pATIHW->genmo |= 0x40U;
        if (pMode->Flags & V_NVSYNC)
            pATIHW->genmo |= 0x80U;
    }
    else
    {
        pMode->Flags &= ~(V_PHSYNC | V_NHSYNC | V_PVSYNC | V_NVSYNC);

        if (pATI->OptionPanelDisplay && (pATI->LCDPanelID >= 0))
            VDisplay = pATI->LCDVertical;
        else
            VDisplay = pMode->CrtcVDisplay;

        if (VDisplay < 400)
        {
            pMode->Flags |= V_PHSYNC | V_NVSYNC;
            pATIHW->genmo |= 0x80U;
        }
        else if (VDisplay < 480)
        {
            pMode->Flags |= V_NHSYNC | V_PVSYNC;
            pATIHW->genmo |= 0x40U;
        }
        else if (VDisplay < 768)
        {
            pMode->Flags |= V_NHSYNC | V_NVSYNC;
            pATIHW->genmo |= 0xC0U;
        }
        else
        {
            pMode->Flags |= V_PHSYNC | V_PVSYNC;
        }
    }

    /* Adjust for interlaced modes */
    if ((pMode->Flags & V_INTERLACE) && (pATI->Chip < ATI_CHIP_264CT))
    {
        pMode->CrtcVDisplay >>= 1;
        pMode->CrtcVBlankStart >>= 1;
        pMode->CrtcVSyncStart >>= 1;
        pMode->CrtcVSyncEnd >>= 1;
        pMode->CrtcVBlankEnd >>= 1;
        pMode->CrtcVTotal >>= 1;
    }

    if (pMode->CrtcVTotal > 1024)
    {
        pATIHW->crt[23] |= 0x04U;
        pMode->CrtcVDisplay >>= 1;
        pMode->CrtcVBlankStart >>= 1;
        pMode->CrtcVSyncStart >>= 1;
        pMode->CrtcVSyncEnd >>= 1;
        pMode->CrtcVBlankEnd >>= 1;
        pMode->CrtcVTotal >>= 1;
    }
    else
    {
        pATIHW->crt[23] &= ~0x04U;
    }

    pMode->CrtcVDisplay--;
    if (pATI->Chip == ATI_CHIP_18800)
        pMode->CrtcVBlankStart++;
    else
        pMode->CrtcVBlankStart--;
    pMode->CrtcVBlankEnd--;
    if (pATI->Chip < ATI_CHIP_264CT)
        pMode->CrtcVBlankEnd--;
    pMode->CrtcVTotal -= 2;
    pMode->CrtcVAdjusted = TRUE;        /* Redundant */

    /* Check sync pulse width */
    Index = pMode->CrtcVSyncEnd - pMode->CrtcVSyncStart - 0x0F;
    if (Index > 0)
    {
        pMode->CrtcVSyncStart += Index / 2;
        pMode->CrtcVSyncEnd = pMode->CrtcVSyncStart + 0x0F;
    }

    /* Check blank pulse width */
    Index = pMode->CrtcVBlankEnd - pMode->CrtcVBlankStart - 0x00FF;
    if (Index > 0)
    {
        if ((pMode->CrtcVBlankEnd - Index) > pMode->CrtcVSyncEnd)
        {
            pMode->CrtcVBlankStart += Index / 2;
            if (pMode->CrtcVBlankStart >= pMode->CrtcVSyncStart)
                pMode->CrtcVBlankStart = pMode->CrtcVSyncStart - 1;
            pMode->CrtcVBlankEnd = pMode->CrtcVBlankStart + 0x00FF;
        }
        else
        {
            Index -= 0x0100;
            if (Index > 0)
            {
                pMode->CrtcVBlankStart += Index / 2;
                if (pMode->CrtcVBlankStart >= pMode->CrtcVSyncStart)
                    pMode->CrtcVBlankStart = pMode->CrtcVSyncStart - 1;
                pMode->CrtcVBlankEnd = pMode->CrtcVBlankStart + 0x01FF;
            }
        }
    }

    /* Set up sequencer register values */
    if (pMode->Flags & V_CLKDIV2)
        pATIHW->seq[1] = 0x09U;
    else
        pATIHW->seq[1] = 0x01U;

    /* Set up CRTC register values */
    pATIHW->crt[0] = pMode->CrtcHTotal;
    pATIHW->crt[1] = pMode->CrtcHDisplay;
    pATIHW->crt[2] = pMode->CrtcHBlankStart;
    pATIHW->crt[3] = (pMode->CrtcHBlankEnd & 0x1FU) | 0x80U;
    Index = ((pMode->CrtcHSkew << 2) + 0x10U) & ~0x1FU;
    if (Index < 0x0080)
        pATIHW->crt[3] |= Index;
    pATIHW->crt[4] = pMode->CrtcHSyncStart;
    pATIHW->crt[5] = ((pMode->CrtcHBlankEnd & 0x20U) << 2) |
                     ((pMode->CrtcHSyncEnd & 0x1FU)      );
    pATIHW->crt[6] = pMode->CrtcVTotal & 0xFFU;
    pATIHW->crt[7] = ((pMode->CrtcVTotal & 0x0100U) >> 8) |
                     ((pMode->CrtcVDisplay & 0x0100U) >> 7) |
                     ((pMode->CrtcVSyncStart & 0x0100U) >> 6) |
                     ((pMode->CrtcVBlankStart & 0x0100U) >> 5) |
                     0x10U |
                     ((pMode->CrtcVTotal & 0x0200U) >> 4) |
                     ((pMode->CrtcVDisplay & 0x0200U) >> 3) |
                     ((pMode->CrtcVSyncStart & 0x0200U) >> 2);
    pATIHW->crt[9] = ((pMode->CrtcVBlankStart & 0x0200U) >> 4) | 0x40U;
    /*
     * Doublescanned modes are missing the top scanline.  Convert
     * doublescanning to multiscanning, using the doublescan bit only as a last
     * resort.
     */
    if ((Index = pMode->VScan) <= 0)
        Index = 1;
    if (pMode->Flags & V_DBLSCAN)
        Index <<= 1;
    Index--;
    pATIHW->crt[9] |= (Index & 0x1FU) | ((Index & 0x20U) << 2);
    pATIHW->crt[16] = pMode->CrtcVSyncStart & 0xFFU;
    pATIHW->crt[17] = (pMode->CrtcVSyncEnd & 0x0FU) | 0x20U;
    pATIHW->crt[18] = pMode->CrtcVDisplay & 0xFFU;
    pATIHW->crt[21] = pMode->CrtcVBlankStart & 0xFFU;
    pATIHW->crt[22] = pMode->CrtcVBlankEnd & 0xFFU;
}

/*
 * ATIVGASet --
 *
 * This function is called to load the VGA portion of a video state.
 */
void
ATIVGASet
(
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{
    int Index;

    /* Set VGA I/O base */
    ATISetVGAIOBase(pATI, pATIHW->genmo);

    /* Load miscellaneous output register */
    outb(GENMO, pATIHW->genmo);

    /* Load sequencer in reverse index order;  this also ends its reset */
    for (Index = NumberOf(pATIHW->seq);  --Index >= 0;  )
        PutReg(SEQX, Index, pATIHW->seq[Index]);

    /* Load CRTC registers */
    for (Index = 0;  Index < NumberOf(pATIHW->crt);  Index++)
        PutReg(CRTX(pATI->CPIO_VGABase), Index, pATIHW->crt[Index]);

    /* Load attribute controller registers */
    for (Index = 0;  Index < NumberOf(pATIHW->attr);  Index++)
    {
        (void)inb(GENS1(pATI->CPIO_VGABase));   /* Reset flip-flop & delay */
        outb(ATTRX, Index);
        outb(ATTRX, pATIHW->attr[Index]);
    }

    /* Load graphics controller registers */
    for (Index = 0;  Index < NumberOf(pATIHW->gra);  Index++)
        PutReg(GRAX, Index, pATIHW->gra[Index]);
}

/*
 * ATIVGASaveScreen --
 *
 * This function blanks or unblanks a VGA screen.
 */
void
ATIVGASaveScreen
(
    ATIPtr pATI,
    int    Mode
)
{
    (void)inb(GENS1(pATI->CPIO_VGABase));       /* Reset flip-flop */

    switch (Mode)
    {
        case SCREEN_SAVER_OFF:
        case SCREEN_SAVER_FORCER:
            outb(ATTRX, 0x20U);                /* Turn PAS on */
            break;

        case SCREEN_SAVER_ON:
        case SCREEN_SAVER_CYCLE:
            outb(ATTRX, 0x00U);                /* Turn PAS off */
            break;

        default:
            break;
    }
}

/*
 * ATIVGASetDPMSMode --
 *
 * This function sets a VGA's VESA Display Power Management Signaling mode.
 */
void
ATIVGASetDPMSMode
(
    ATIPtr pATI,
    int    DPMSMode
)
{
    CARD8 seq1, crt17;

    switch (DPMSMode)
    {
        case DPMSModeOn:        /* HSync on, VSync on */
            seq1 = 0x00U;
            crt17 = 0x80U;
            break;

        case DPMSModeStandby:   /* HSync off, VSync on -- unsupported */
            seq1 = 0x20U;
            crt17 = 0x80U;
            break;

        case DPMSModeSuspend:   /* HSync on, VSync off -- unsupported */
            seq1 = 0x20U;
            crt17 = 0x80U;
            break;

        case DPMSModeOff:       /* HSync off, VSync off */
            seq1 = 0x20U;
            crt17 = 0x00U;
            break;

        default:                /* Muffle compiler */
            return;
    }

    PutReg(SEQX, 0x00U, 0x01U); /* Start synchonous reset */
    seq1 |= GetReg(SEQX, 0x01U) & ~0x20U;
    PutReg(SEQX, 0x01U, seq1);
    crt17 |= GetReg(CRTX(pATI->CPIO_VGABase), 0x17U) & ~0x80U;
    usleep(10000);
    PutReg(CRTX(pATI->CPIO_VGABase), 0x17U, crt17);
    PutReg(SEQX, 0x01U, 0x03U); /* End synchonous reset */
}

#endif /* AVOID_CPIO */
