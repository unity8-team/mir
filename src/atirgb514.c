/*
 * Copyright 2001 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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
#include "aticrtc.h"
#include "atimach64io.h"
#include "atirgb514.h"

/*
 * ATIRGB514PreInit --
 *
 * This function fills in the IBM RGB 514 portion of an ATIHWRec that is common
 * to all video modes generated by the server.
 */
void
ATIRGB514PreInit
(
    ATIPtr   pATI,
    ATIHWPtr pATIHW
)
{
    /* Get a work copy of IBM RGB 514 registers */
    ATIRGB514Save(pATI, pATIHW);

    /* Miscellaneous Clock Control */
    pATIHW->ibmrgb514[0x0002U] = 0x01U;

    /* Sync Control */
    pATIHW->ibmrgb514[0x0003U] &= ~0x80U;

    /* Horizontal Sync Control */
    pATIHW->ibmrgb514[0x0004U] = 0x00U;

    /* Power Management */
    pATIHW->ibmrgb514[0x0005U] = 0x00U;

    /* DAC Operation */
    pATIHW->ibmrgb514[0x0006U] &= ~0x04U;

    /* Palette Control */
    pATIHW->ibmrgb514[0x0007U] = 0x00U;

    /* PLL Control */
    pATIHW->ibmrgb514[0x0010U] = 0x01U;

    /* Cursor control */
    pATIHW->ibmrgb514[0x0030U] &= ~0x03U;       /* For now */

    /* Border (i.e. overscan) */
    pATIHW->ibmrgb514[0x0060U] = 0x00U;
    pATIHW->ibmrgb514[0x0061U] = 0x00U;
    pATIHW->ibmrgb514[0x0062U] = 0x00U;

    /* Miscellaneous Control */
    pATIHW->ibmrgb514[0x0070U] &= ~0x20U;
    pATIHW->ibmrgb514[0x0071U] = 0x41U; /* See workaround in ATIRGB514Set() */

#ifndef AVOID_CPIO

    if (pATIHW->crtc == ATI_CRTC_VGA)
    {
        /* Pixel Format */
        pATIHW->ibmrgb514[0x000AU] = 0x03U;

        /* Miscellaneous Control */
        pATIHW->ibmrgb514[0x0070U] |= 0x40U;

        /* VRAM Mask */
        pATIHW->ibmrgb514[0x0090U] = 0x03U;
    }
    else

#endif /* AVOID_CPIO */

    {
        /* Miscellaneous Control */
        pATIHW->ibmrgb514[0x0070U] &= ~0x40U;

        /* VRAM Mask */
        pATIHW->ibmrgb514[0x0090U] = 0x00U;
        pATIHW->ibmrgb514[0x0091U] = 0x00U;

        /* Pixel Format */
        switch (pATI->depth)
        {
            case 8:
                pATIHW->ibmrgb514[0x000AU] = 0x03U;
                pATIHW->ibmrgb514[0x000BU] = 0x00U;
                break;

            case 15:
                pATIHW->ibmrgb514[0x000AU] = 0x04U;
                pATIHW->ibmrgb514[0x000CU] = 0xC4U;
                break;

            case 16:
                pATIHW->ibmrgb514[0x000AU] = 0x04U;
                pATIHW->ibmrgb514[0x000CU] = 0xC6U;
                break;

            case 24:
                if (pATI->bitsPerPixel == 24)
                {
                    pATIHW->ibmrgb514[0x000AU] = 0x05U;
                    pATIHW->ibmrgb514[0x000DU] = 0x01U;
                }
                else
                {
                    pATIHW->ibmrgb514[0x000AU] = 0x06U;
                    pATIHW->ibmrgb514[0x000EU] = 0x03U;
                }
                break;

            default:
                break;
        }
    }

    if (pATI->rgbBits == 8)
        pATIHW->ibmrgb514[0x0071U] |= 0x04U;
}

/*
 * ATIRGB514Save --
 *
 * This function saves IBM RGB514 related data into an ATIHWRec.
 */
void
ATIRGB514Save
(
    ATIPtr   pATI,
    ATIHWPtr pATIHW
)
{
    CARD32 crtc_gen_cntl, dac_cntl;
    CARD8  index_lo, index_hi, index_ctl;
    int    Index;

    /* Temporarily switch to Mach64 CRTC */
    crtc_gen_cntl = inr(CRTC_GEN_CNTL);
    if (!(crtc_gen_cntl & CRTC_EXT_DISP_EN))
        outr(CRTC_GEN_CNTL, crtc_gen_cntl | CRTC_EXT_DISP_EN);

    /* Temporarily switch to IBM RGB 514 registers */
    dac_cntl = inr(DAC_CNTL) & ~(DAC_EXT_SEL_RS2 | DAC_EXT_SEL_RS3);
    outr(DAC_CNTL, dac_cntl | DAC_EXT_SEL_RS2);

    index_lo = in8(M64_DAC_WRITE);
    index_hi = in8(M64_DAC_DATA);
    index_ctl = in8(M64_DAC_READ);

    out8(M64_DAC_WRITE, 0x00U);
    out8(M64_DAC_DATA, 0x00U);
    out8(M64_DAC_READ, 0x01U);  /* Auto-increment */

    /* Save IBM RGB 514 registers */
    for (Index = 0;  Index < NumberOf(pATIHW->ibmrgb514);  Index++)
    {
        /* Need to rewrite the index every so often... */
        if ((Index == 0x0100) || (Index == 0x0500))
        {
            out8(M64_DAC_WRITE, 0);
            out8(M64_DAC_DATA, Index >> 8);
        }
        pATIHW->ibmrgb514[Index] = in8(M64_DAC_MASK);
    }

    /* Restore registers */
    out8(M64_DAC_WRITE, index_lo);
    out8(M64_DAC_DATA, index_hi);
    out8(M64_DAC_READ, index_ctl);
    outr(DAC_CNTL, dac_cntl);
    if (!(crtc_gen_cntl & CRTC_EXT_DISP_EN))
        outr(CRTC_GEN_CNTL, crtc_gen_cntl);
}

/*
 * ATIRGB514Calculate --
 *
 * This function fills in the IBM RGB 514 portion of an ATIHWRec that is
 * specific to a display mode.  pATIHW->ibmrgb514 has already been
 * initialised by a previous call to ATIRGB514PreInit().
 */
void
ATIRGB514Calculate
(
    ATIPtr         pATI,
    ATIHWPtr       pATIHW,
    DisplayModePtr pMode
)
{
    if (pATI->OptionCSync || (pMode->Flags & (V_CSYNC | V_PCSYNC)))
        pATIHW->ibmrgb514[0x0006U] |= 0x08U;
    else
        pATIHW->ibmrgb514[0x0006U] &= ~0x08U;

    if (pMode->Flags & V_INTERLACE)
        pATIHW->ibmrgb514[0x0071U] |= 0x20U;
    else
        pATIHW->ibmrgb514[0x0071U] &= ~0x20U;
}

/*
 * ATIRGB514Set --
 *
 * This function is called to set an IBM RGB514's registers.
 */
void
ATIRGB514Set
(
    ATIPtr   pATI,
    ATIHWPtr pATIHW
)
{
    CARD32 crtc_gen_cntl, dac_cntl;
    CARD8  index_lo, index_hi, index_ctl;
    int    Index;

    /* Temporarily switch to Mach64 CRTC */
    crtc_gen_cntl = inr(CRTC_GEN_CNTL);
    if (!(crtc_gen_cntl & CRTC_EXT_DISP_EN))
        outr(CRTC_GEN_CNTL, crtc_gen_cntl | CRTC_EXT_DISP_EN);

    /* Temporarily switch to IBM RGB 514 registers */
    dac_cntl = inr(DAC_CNTL) & ~(DAC_EXT_SEL_RS2 | DAC_EXT_SEL_RS3);
    outr(DAC_CNTL, dac_cntl | DAC_EXT_SEL_RS2);

    index_lo = in8(M64_DAC_WRITE);
    index_hi = in8(M64_DAC_DATA);
    index_ctl = in8(M64_DAC_READ);

    out8(M64_DAC_WRITE, 0x00U);
    out8(M64_DAC_DATA, 0x00U);
    out8(M64_DAC_READ, 0x01U);  /* Auto-increment */

    /* Load IBM RGB 514 registers */
    for (Index = 0;  Index < NumberOf(pATIHW->ibmrgb514);  Index++)
         out8(M64_DAC_MASK, pATIHW->ibmrgb514[Index]);

#ifndef AVOID_CPIO

    /* Deal with documented anomaly */
    if (pATIHW->crtc == ATI_CRTC_VGA)
    {
        /* Reset Miscellaneous Control 2 */
        out8(M64_DAC_WRITE, 0x71U);
        out8(M64_DAC_DATA, 0x00U);
        out8(M64_DAC_MASK, pATIHW->ibmrgb514[0x0071U] & ~0x41U);
    }

#endif /* AVOID_CPIO */

    /* Restore registers */
    out8(M64_DAC_WRITE, index_lo);
    out8(M64_DAC_DATA, index_hi);
    out8(M64_DAC_READ, index_ctl);
    outr(DAC_CNTL, dac_cntl);
    if (!(crtc_gen_cntl & CRTC_EXT_DISP_EN))
        outr(CRTC_GEN_CNTL, crtc_gen_cntl);
}
