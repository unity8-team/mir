/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiwonder.c,v 1.14 2003/01/01 19:16:35 tsi Exp $ */
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

/*
 * The ATI x8800 chips use special registers for their extended VGA features.
 * These registers are accessible through an index I/O port and a data I/O
 * port.  BIOS initialisation stores the index port number in the Graphics
 * register bank (0x03CE), indices 0x50 and 0x51.  Unfortunately, for all but
 * the 18800-x series of adapters, these registers are write-only (a.k.a. black
 * holes).  On all but 88800's, the index port number can be found in the short
 * integer at offset 0x10 in the BIOS.  For 88800's, this driver will use
 * 0x01CE or 0x03CE as the index port number, depending on the I/O port
 * decoding used.  The data port number is one more than the index port number
 * (i.e. 0x01CF).  These ports differ slightly in their I/O behaviour from the
 * normal VGA ones:
 *
 *    write:  outw(0x01CE, (data << 8) | index);        (16-bit, not used)
 *            outb(0x01CE, index);  outb(0x01CF, data); (8-bit)
 *    read:   outb(0x01CE, index);  data = inb(0x01CF);
 *
 * Two consecutive byte-writes to the data port will not work.  Furthermore an
 * index written to 0x01CE is usable only once.  Note also that the setting of
 * ATI extended registers (especially those with clock selection bits) should
 * be bracketed by a sequencer reset.
 *
 * The number of these extended VGA registers varies by chipset.  The 18800
 * series have 16, the 28800 series have 32, while 68800's and 88800's have 64.
 * The last 16 on each have almost identical definitions.  Thus, the BIOS sets
 * up an indexing scheme whereby the last 16 extended VGA registers are
 * accessed at indices 0xB0 through 0xBF on all chipsets.
 */

#include "ati.h"
#include "atichip.h"
#include "atiwonder.h"
#include "atiwonderio.h"

#ifndef AVOID_CPIO

/*
 * ATIVGAWonderPreInit --
 *
 * This function is called to initialise the VGA Wonder part of an ATIHWRec
 * that is common to all modes generated by the driver.
 */
void
ATIVGAWonderPreInit
(
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{
    pATIHW->b3 = ATIGetExtReg(0xB3U) & 0x20U;
    if (pATI->depth <= 4)
        pATIHW->b6 = 0x40U;
    else
        pATIHW->b6 = 0x04U;
    if (pATI->Chip <= ATI_CHIP_18800)
        pATIHW->ba = 0x08U;
    else if (pATI->Chip >= ATI_CHIP_28800_2)
    {
        if (pATI->VideoRAM > 256)
            pATIHW->b6 |= 0x01U;
        pATIHW->bf = ATIGetExtReg(0xBFU) & 0x5FU;
        pATIHW->a3 = ATIGetExtReg(0xA3U) & 0x67U;
        pATIHW->ab = ATIGetExtReg(0xABU) & 0xE7U;
        pATIHW->ae = ATIGetExtReg(0xAEU) & 0xE0U;
    }
}

/*
 * ATIVGAWonderSave --
 *
 * This function is called to save the VGA Wonder portion of the current video
 * state.
 */
void
ATIVGAWonderSave
(
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{
    pATIHW->b0 = ATIGetExtReg(0xB0U);
    pATIHW->b1 = ATIGetExtReg(0xB1U);
    pATIHW->b2 = ATIGetExtReg(0xB2U);
    pATIHW->b3 = ATIGetExtReg(0xB3U);
    pATIHW->b5 = ATIGetExtReg(0xB5U);
    pATIHW->b6 = ATIGetExtReg(0xB6U);
    pATIHW->b8 = ATIGetExtReg(0xB8U);
    pATIHW->b9 = ATIGetExtReg(0xB9U);
    pATIHW->ba = ATIGetExtReg(0xBAU);
    pATIHW->bd = ATIGetExtReg(0xBDU);
    if (pATI->Chip > ATI_CHIP_18800)
    {
        pATIHW->be = ATIGetExtReg(0xBEU);
        if (pATI->Chip >= ATI_CHIP_28800_2)
        {
            pATIHW->bf = ATIGetExtReg(0xBFU);
            pATIHW->a3 = ATIGetExtReg(0xA3U);
            pATIHW->a6 = ATIGetExtReg(0xA6U);
            pATIHW->a7 = ATIGetExtReg(0xA7U);
            pATIHW->ab = ATIGetExtReg(0xABU);
            pATIHW->ac = ATIGetExtReg(0xACU);
            pATIHW->ad = ATIGetExtReg(0xADU);
            pATIHW->ae = ATIGetExtReg(0xAEU);
        }
    }
}

/*
 * ATIVGAWonderCalculate --
 *
 * This function fills in the VGA Wonder portion of an ATIHWRec structure
 * occurrence.
 */
void
ATIVGAWonderCalculate
(
    ATIPtr         pATI,
    ATIHWPtr       pATIHW,
    DisplayModePtr pMode
)
{
    /* Set up the default horizontal display enable skew */
    if ((pATI->Chip >= ATI_CHIP_28800_2) && (pATI->Chip <= ATI_CHIP_28800_6) &&
        !(pMode->Flags & V_HSKEW))
    {
        /*
         * Modes using the higher clock frequencies need a non-zero Display
         * Enable Skew.  The following number has been empirically determined
         * to be somewhere between 4.2 and 4.7 MHz.
         */
#       define DisplayEnableSkewThreshold 4500

        /* Set a reasonable default Display Enable Skew */
        pMode->HSkew = pMode->CrtcHSkew =
            ATIDivide(pMode->SynthClock, DisplayEnableSkewThreshold, 0, 0);
    }
    pMode->Flags |= V_HSKEW;

    /*
     * Fill in mode-specific VGA Wonder data.
     */
    pATIHW->b0 = 0x00U;
    if (pATI->depth >= 8)
        pATIHW->b0 = 0x20U;
    if (pATI->Chip >= ATI_CHIP_28800_2)
    {
        if (pATI->VideoRAM > 512)
            pATIHW->b0 |= 0x08U;
        else if (pATI->VideoRAM > 256)
            pATIHW->b0 |= 0x10U;
    }
    else if (pATI->depth <= 4)
    {
        if (pATI->VideoRAM > 256)
            pATIHW->b0 |= 0x08U;
    }
    else
    {
        if (pATI->VideoRAM > 256)
            pATIHW->b0 |= 0x18U;
        else
            pATIHW->b0 |= 0x06U;
    }
    pATIHW->b1 = ATIGetExtReg(0xB1U) & 0x04U;
    /*
     * Setting the following bit causes hangs on return to text mode from
     * packed modes on 18800-1's.  The hang occurs because the adapter's I/O
     * response is completely disabled when the register is rewritten.  The
     * adapter can then only be re-enabled with a powerdown.  The bit, when on,
     * blanks out the overscan.
     */
    if ((pATI->Chip == ATI_CHIP_18800_1) && (pATI->depth >= 8))
        pATIHW->b5 = 0x00U;
    else
        pATIHW->b5 = 0x01U;
    pATIHW->b8 = ATIGetExtReg(0xB8U) & 0xC0U;
    pATIHW->b9 = ATIGetExtReg(0xB9U) & 0x7FU;
    pATIHW->bd = ATIGetExtReg(0xBDU) & 0x02U;
    if (pATI->Chip <= ATI_CHIP_18800)
        pATIHW->b2 = ATIGetExtReg(0xB2U) & 0xC0U;
    else
    {
        pATIHW->b2 = 0x00U;
        pATIHW->be = (ATIGetExtReg(0xBEU) & 0x30U) | 0x09U;
        if (pATI->Chip >= ATI_CHIP_28800_2)
        {
            pATIHW->a6 = (ATIGetExtReg(0xA6U) & 0x38U) | 0x04U;
            pATIHW->a7 = (ATIGetExtReg(0xA7U) & 0xBEU)        ;
            pATIHW->ac = (ATIGetExtReg(0xACU) & 0x8EU)        ;
        }
    }
    if (pMode->Flags & V_INTERLACE)
    {                                   /* Enable interlace */
        if (pATI->Chip <= ATI_CHIP_18800)
            pATIHW->b2 |= 0x01U;
        else
            pATIHW->be |= 0x02U;
    }
#if 0   /* This is no longer needed but is left in for reference */
    if (pMode->Flags & V_DBLSCAN)       /* Enable doublescan */
        pATIHW->b1 |= 0x08U;
#endif
    if (pATI->OptionCSync || (pMode->Flags & (V_CSYNC | V_PCSYNC)))
        pATIHW->bd |= 0x08U;            /* Enable composite sync */
    if (pMode->Flags & V_NCSYNC)
        pATIHW->bd |= 0x09U;            /* Invert composite sync */
    if (pMode->HSkew > 0)
    {
        if (pMode->HSkew <= 3)
            pATIHW->b5 |= 0x04U;
        else if (pATI->Chip >= ATI_CHIP_28800_2)
            switch ((pMode->HSkew + 4) >> 3)
            {
                case 1:         /* Use ATI override */
                    pATIHW->crt[3] &= ~0x60U;
                    pATIHW->b0 |= 0x01U;
                    break;
                case 2:         /* Use ATI override */
                    pATIHW->crt[3] &= ~0x60U;
                    pATIHW->a6 |= 0x01U;
                    break;
                case 3:
                    pATIHW->crt[3] |= 0x60U;
                    break;
                case 4:
                    pATIHW->a7 |= 0x40U;
                    break;
                case 5:
                    pATIHW->ac |= 0x10U;
                    break;
                case 6:
                    pATIHW->ac |= 0x20U;
                    break;
                default:
                    break;
            }
    }
}

/*
 * ATIVGAWonderSet --
 *
 * This function loads the VGA Wonder portion of a video state.
 */
void
ATIVGAWonderSet
(
    ATIPtr      pATI,
    ATIHWPtr    pATIHW
)
{
    if (pATI->Chip <= ATI_CHIP_18800)
        ATIModifyExtReg(pATI, 0xB2U, -1, 0x00U, pATIHW->b2);
    else
    {
        ATIModifyExtReg(pATI, 0xBEU, -1, 0x00U, pATIHW->be);
        if (pATI->Chip >= ATI_CHIP_28800_2)
        {
            ATIModifyExtReg(pATI, 0xBFU, -1, 0x00U, pATIHW->bf);
            ATIModifyExtReg(pATI, 0xA3U, -1, 0x00U, pATIHW->a3);
            ATIModifyExtReg(pATI, 0xA6U, -1, 0x00U, pATIHW->a6);
            ATIModifyExtReg(pATI, 0xA7U, -1, 0x00U, pATIHW->a7);
            ATIModifyExtReg(pATI, 0xABU, -1, 0x00U, pATIHW->ab);
            ATIModifyExtReg(pATI, 0xACU, -1, 0x00U, pATIHW->ac);
            ATIModifyExtReg(pATI, 0xADU, -1, 0x00U, pATIHW->ad);
            ATIModifyExtReg(pATI, 0xAEU, -1, 0x00U, pATIHW->ae);
        }
    }
    ATIModifyExtReg(pATI, 0xB0U, -1, 0x00U, pATIHW->b0);
    ATIModifyExtReg(pATI, 0xB1U, -1, 0x00U, pATIHW->b1);
    ATIModifyExtReg(pATI, 0xB3U, -1, 0x00U, pATIHW->b3);
    ATIModifyExtReg(pATI, 0xB5U, -1, 0x00U, pATIHW->b5);
    ATIModifyExtReg(pATI, 0xB6U, -1, 0x00U, pATIHW->b6);
    ATIModifyExtReg(pATI, 0xB8U, -1, 0x00U, pATIHW->b8);
    ATIModifyExtReg(pATI, 0xB9U, -1, 0x00U, pATIHW->b9);
    ATIModifyExtReg(pATI, 0xBAU, -1, 0x00U, pATIHW->ba);
    ATIModifyExtReg(pATI, 0xBDU, -1, 0x00U, pATIHW->bd);
}

#endif /* AVOID_CPIO */
