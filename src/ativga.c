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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
