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

/*************************************************************************/

/*
 * Author:  Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * This is the ATI driver for XFree86.
 *
 * John Donne once said "No man is an island", and I am most certainly not an
 * exception.  Contributions, intentional or not, to this and previous versions
 * of this driver by the following are hereby acknowledged:
 *
 * Thomas Roell, Per Lindqvist, Doug Evans, Rik Faith, Arthur Tateishi,
 * Alain Hebert, Ton van Rosmalen, David Chambers, William Shubert,
 * ATI Technologies Incorporated, Robert Wolff, David Dawes, Mark Weaver,
 * Hans Nasten, Kevin Martin, Frederic Rienthaler, Marc Bolduc, Reuben Sumner,
 * Benjamin T. Yang, James Fast Kane, Randall Hopper, W. Marcus Miller,
 * Henrik Harmsen, Christian Lupien, Precision Insight Incorporated,
 * Mark Vojkovich, Huw D M Davies, Andrew C Aitchison, Ani Joshi,
 * Kostas Gewrgiou, Jakub Jelinek, David S. Miller, A E Lawrence,
 * Linus Torvalds, William Blew, Ignacio Garcia Etxebarria, Patrick Chase,
 * Vladimir Dergachev, Egbert Eich, Mike A. Harris
 *
 * ... and, many, many others from around the world.
 *
 * In addition, this work would not have been possible without the active
 * support, both moral and otherwise, of the staff and management of Computing
 * and Network Services at the University of Alberta, in Edmonton, Alberta,
 * Canada.
 *
 * The driver is intended to support all ATI adapters since their VGA Wonder
 * V3, including OEM counterparts.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ati.h"
#include "atichip.h"
#include "atimodule.h"
#include "ativersion.h"
#include "atimach64probe.h"

#include "radeon_probe.h"
#include "radeon_version.h"
#include "r128_probe.h"
#include "r128_version.h"

/*
 * ATIIdentify --
 *
 * Print the driver's list of chipset names.
 */
static void
ATIIdentify
(
    int flags
)
{
    /*
     * Only print chip families here, chip lists are printed when a subdriver
     * is loaded.
     */
    xf86Msg(X_INFO, "%s: %s\n", ATI_NAME,
            "ATI driver wrapper (version " ATI_VERSION_NAME ") for chipsets: "
            "mach64, rage128, radeon");
}

/*
 * ATIProbe --
 *
 * This function is called once, at the start of the first server generation to
 * do a minimal probe for supported hardware.
 */
static Bool
ATIProbe
(
    DriverPtr pDriver,
    int       flags
)
{
    pciVideoPtr pVideo, *xf86PciVideoInfo = xf86GetPciVideoInfo();
    Bool        DoMach64 = FALSE;
    Bool        DoRage128 = FALSE, DoRadeon = FALSE;
    int         i;
    ATIChipType Chip;

    if (!(flags & PROBE_DETECT))
    {
        if (xf86MatchDevice(ATI_NAME, NULL) > 0)
            DoMach64 = TRUE;
        if (xf86MatchDevice(R128_NAME, NULL) > 0)
            DoRage128 = TRUE;
        if (xf86MatchDevice(RADEON_NAME, NULL) > 0)
            DoRadeon = TRUE;
    }

    if (xf86PciVideoInfo)
    {
        for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
        {
            if ((pVideo->vendor != PCI_VENDOR_ATI) ||
                (pVideo->chipType == PCI_CHIP_MACH32))
                continue;

            /* Check for Rage128's, Radeon's and later adapters */
            Chip = ATIChipID(pVideo->chipType, pVideo->chipRev);
            if (Chip > ATI_CHIP_Mach64)
            {
                if (Chip <= ATI_CHIP_Rage128)
                    DoRage128 = TRUE;
                else if (Chip <= ATI_CHIP_Radeon)
                    DoRadeon = TRUE;

                continue;
            }

            DoMach64 = TRUE;
        }
    }

    /* Call Radeon driver probe */
    if (DoRadeon)
    {
        pointer radeon = xf86LoadDrvSubModule(pDriver, "radeon");

        if (!radeon)
        {
            xf86Msg(X_ERROR,
                ATI_NAME ":  Failed to load \"radeon\" module.\n");
            return FALSE;
        }

        xf86LoaderReqSymLists(RADEONSymbols, NULL);

        RADEONIdentify(flags);

        if (RADEONProbe(pDriver, flags))
            return TRUE;

        xf86UnloadSubModule(radeon);
    }

    /* Call Rage 128 driver probe */
    if (DoRage128)
    {
        pointer r128 = xf86LoadDrvSubModule(pDriver, "r128");

        if (!r128)
        {
            xf86Msg(X_ERROR,
                ATI_NAME ":  Failed to load \"r128\" module.\n");
            return FALSE;
        }

        xf86LoaderReqSymLists(R128Symbols, NULL);

        R128Identify(flags);

        if (R128Probe(pDriver, flags))
            return TRUE;

        xf86UnloadSubModule(r128);
    }

    /* Call Mach64 driver probe */
    if (DoMach64)
    {
        pointer atimisc = xf86LoadDrvSubModule(pDriver, "atimisc");

        if (!atimisc)
        {
            xf86Msg(X_ERROR,
                ATI_NAME ":  Failed to load \"atimisc\" module.\n");
            return FALSE;
        }

        xf86LoaderReqSymLists(ATISymbols, NULL);

        Mach64Identify(flags);

        if (Mach64Probe(pDriver, flags))
            return TRUE;

        xf86UnloadSubModule(atimisc);
    }

    return FALSE;
}

/*
 * ATIAvailableOptions --
 *
 * Return recognised options that are intended for public consumption.
 */
static const OptionInfoRec *
ATIAvailableOptions
(
    int ChipId,
    int BusId
)
{
    CARD16      ChipType = ChipId & 0xffff;
    ATIChipType Chip;

    /* Probe should have loaded the appropriate subdriver by this point */

    Chip = ATIChipID(ChipType, 0x0); /* chip revision is don't care */
    if (Chip <= ATI_CHIP_Mach64)
        return Mach64AvailableOptions(ChipId, BusId);
    else if (Chip <= ATI_CHIP_Rage128)
        return R128AvailableOptions(ChipId, BusId);
    else if (Chip <= ATI_CHIP_Radeon)
        return RADEONAvailableOptions(ChipId, BusId);

    return NULL;
}

/* The root of all evil... */
_X_EXPORT DriverRec ATI =
{
    ATI_VERSION_CURRENT,
    "ati",
    ATIIdentify,
    ATIProbe,
    ATIAvailableOptions,
    NULL,
    0
};
