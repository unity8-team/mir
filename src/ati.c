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

#ifdef XSERVER_LIBPCIACCESS
#include <pciaccess.h>
#endif
#include "atipcirename.h"

#include "ati.h"
#include "ativersion.h"

/* names duplicated from version headers */
#define MACH64_NAME         "MACH64"
#define MACH64_DRIVER_NAME  "mach64"
#define R128_NAME           "R128"
#define R128_DRIVER_NAME    "r128"
#define RADEON_NAME         "RADEON"
#define RADEON_DRIVER_NAME  "radeon"

enum
{
    ATI_CHIP_FAMILY_NONE = 0,
    ATI_CHIP_FAMILY_Mach64,
    ATI_CHIP_FAMILY_Rage128,
    ATI_CHIP_FAMILY_Radeon
};

/*
 * Record which sub-drivers have already been loaded, and thus have called
 * xf86AddDriver(). For those sub-drivers, cause the ati wrapper later to fail
 * when probing.
 *
 * The check is only called once when the ati wrapper is loaded and depends on
 * the X server loading all drivers before doing any probes.
 */
static Bool mach64_drv_added = FALSE;
static Bool r128_drv_added = FALSE;
static Bool radeon_drv_added = FALSE;

void
ati_check_subdriver_added()
{
    if (LoaderSymbol(MACH64_NAME))
        mach64_drv_added = TRUE;
    if (LoaderSymbol(R128_NAME))
        r128_drv_added = TRUE;
    if (LoaderSymbol(RADEON_NAME))
        radeon_drv_added = TRUE;
}

static int ATIChipID(const CARD16);

#ifdef XSERVER_LIBPCIACCESS
static const struct pci_id_match ati_device_match = {
    PCI_VENDOR_ATI, PCI_MATCH_ANY, PCI_MATCH_ANY, PCI_MATCH_ANY, 0, 0, 0
};

/* Stolen from xf86pciBus.c */
/* PCI classes that get included in xf86PciVideoInfo */
#define PCIINFOCLASSES(c) \
    (  (((c) & 0x00ff0000) == (PCI_CLASS_PREHISTORIC << 16)) ||           \
       (((c) & 0x00ff0000) == (PCI_CLASS_DISPLAY << 16)) ||               \
      ((((c) & 0x00ffff00) == ((PCI_CLASS_MULTIMEDIA << 16) |             \
                               (PCI_SUBCLASS_MULTIMEDIA_VIDEO << 8)))) || \
      ((((c) & 0x00ffff00) == ((PCI_CLASS_PROCESSOR << 16) |              \
                               (PCI_SUBCLASS_PROCESSOR_COPROC << 8)))) )
#endif

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
    pciVideoPtr pVideo;
#ifndef XSERVER_LIBPCIACCESS
    pciVideoPtr *xf86PciVideoInfo;
#else
    struct pci_device_iterator *pVideoIter;
#endif
    Bool        DoMach64 = FALSE;
    Bool        DoRage128 = FALSE, DoRadeon = FALSE;
    int         Chip;

    /* Let the sub-drivers probe & configure for themselves */
    if (xf86ServerIsOnlyDetecting())
        return FALSE;

#ifndef XSERVER_LIBPCIACCESS

    xf86PciVideoInfo = xf86GetPciVideoInfo();

    if (xf86PciVideoInfo == NULL)
        return FALSE;

    while ((pVideo = *xf86PciVideoInfo++) != NULL)
    {
        if ((PCI_DEV_VENDOR_ID(pVideo) != PCI_VENDOR_ATI) ||
            (PCI_DEV_DEVICE_ID(pVideo) == PCI_CHIP_MACH32))
            continue;

        /* Check for Rage128's, Radeon's and later adapters */
        Chip = ATIChipID(PCI_DEV_DEVICE_ID(pVideo));
        if (Chip == ATI_CHIP_FAMILY_Mach64)
            DoMach64 = TRUE;
        else if (Chip == ATI_CHIP_FAMILY_Rage128)
            DoRage128 = TRUE;
        else if (Chip == ATI_CHIP_FAMILY_Radeon)
            DoRadeon = TRUE;
    }

#else /* XSERVER_LIBPCIACCESS */

    pVideoIter = pci_id_match_iterator_create(&ati_device_match);

    while ((pVideo = pci_device_next(pVideoIter)) != NULL)
    {
        /* Check for non-video devices */
        if (!PCIINFOCLASSES(pVideo->device_class))
            continue;

        /* Check for prehistoric PCI Mach32 */
        if ((PCI_DEV_VENDOR_ID(pVideo) != PCI_VENDOR_ATI) ||
            (PCI_DEV_DEVICE_ID(pVideo) == PCI_CHIP_MACH32))
            continue;

        /* Check for Rage128's, Radeon's and later adapters */
        Chip = ATIChipID(PCI_DEV_DEVICE_ID(pVideo));
        if (Chip == ATI_CHIP_FAMILY_Mach64)
            DoMach64 = TRUE;
        else if (Chip == ATI_CHIP_FAMILY_Rage128)
            DoRage128 = TRUE;
        else if (Chip == ATI_CHIP_FAMILY_Radeon)
            DoRadeon = TRUE;
    }

    pci_iterator_destroy(pVideoIter);

#endif /* XSERVER_LIBPCIACCESS */

    /* Call Radeon driver probe */
    if (DoRadeon)
    {
        DriverRec *radeon;

        /* If the sub-driver was added, let it probe for itself */
        if (radeon_drv_added)
            return FALSE;

        if (!LoaderSymbol(RADEON_NAME))
            xf86LoadDrvSubModule(pDriver, RADEON_DRIVER_NAME);

        radeon = (DriverRec*)LoaderSymbol(RADEON_NAME);

        if (!radeon)
        {
            xf86Msg(X_ERROR,
                ATI_NAME ":  Failed to find \"radeon\" driver symbol.\n");
            return FALSE;
        }

        radeon->Identify(flags);

        if (radeon->Probe(pDriver, flags))
            return TRUE;
    }

    /* Call Rage 128 driver probe */
    if (DoRage128)
    {
        DriverRec *r128;

        /* If the sub-driver was added, let it probe for itself */
        if (r128_drv_added)
            return FALSE;

        if (!LoaderSymbol(R128_NAME))
            xf86LoadDrvSubModule(pDriver, R128_DRIVER_NAME);

        r128 = (DriverRec*)LoaderSymbol(R128_NAME);

        if (!r128)
        {
            xf86Msg(X_ERROR,
                ATI_NAME ":  Failed to find \"r128\" driver symbol.\n");
            return FALSE;
        }

        r128->Identify(flags);

        if (r128->Probe(pDriver, flags))
            return TRUE;
    }

    /* Call Mach64 driver probe */
    if (DoMach64)
    {
        DriverRec *mach64;

        /* If the sub-driver was added, let it probe for itself */
        if (mach64_drv_added)
            return FALSE;

        if (!LoaderSymbol(MACH64_NAME))
            xf86LoadDrvSubModule(pDriver, MACH64_DRIVER_NAME);

        mach64 = (DriverRec*)LoaderSymbol(MACH64_NAME);

        if (!mach64)
        {
            xf86Msg(X_ERROR,
                ATI_NAME ":  Failed to find \"mach64\" driver symbol.\n");
            return FALSE;
        }

        mach64->Identify(flags);

        if (mach64->Probe(pDriver, flags))
            return TRUE;
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
    int         Chip;

    /* Probe should have loaded the appropriate subdriver by this point */

    Chip = ATIChipID(ChipType);
    if (Chip == ATI_CHIP_FAMILY_Mach64)
    {
        DriverRec *mach64 = (DriverRec*)LoaderSymbol(MACH64_NAME);

        if (!mach64)
        {
            xf86Msg(X_ERROR,
                ATI_NAME ":  Failed to find \"mach64\" driver symbol.\n");
            return NULL;
        }

        return mach64->AvailableOptions(ChipId, BusId);
    }

    if (Chip == ATI_CHIP_FAMILY_Rage128)
    {
        DriverRec *r128 = (DriverRec*)LoaderSymbol(R128_NAME);

        if (!r128)
        {
            xf86Msg(X_ERROR,
                ATI_NAME ":  Failed to find \"r128\" driver symbol.\n");
            return NULL;
        }

        return r128->AvailableOptions(ChipId, BusId);
    }

    if (Chip == ATI_CHIP_FAMILY_Radeon)
    {
        DriverRec *radeon = (DriverRec*)LoaderSymbol(RADEON_NAME);

        if (!radeon)
        {
            xf86Msg(X_ERROR,
                ATI_NAME ":  Failed to find \"radeon\" driver symbol.\n");
            return NULL;
        }

        return radeon->AvailableOptions(ChipId, BusId);
    }

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

/*
 * ATIChipID --
 *
 * This returns the ATI_CHIP_FAMILY_* value associated with a particular ChipID.
 */
static int
ATIChipID(const CARD16 ChipID)
{
    switch (ChipID)
    {
        case PCI_CHIP_MACH64GX:
        case PCI_CHIP_MACH64CX:
        case PCI_CHIP_MACH64CT:
        case PCI_CHIP_MACH64ET:
        case PCI_CHIP_MACH64VT:
        case PCI_CHIP_MACH64GT:
        case PCI_CHIP_MACH64VU:
        case PCI_CHIP_MACH64GU:
        case PCI_CHIP_MACH64LG:
        case PCI_CHIP_MACH64VV:
        case PCI_CHIP_MACH64GV:
        case PCI_CHIP_MACH64GW:
        case PCI_CHIP_MACH64GY:
        case PCI_CHIP_MACH64GZ:
        case PCI_CHIP_MACH64GB:
        case PCI_CHIP_MACH64GD:
        case PCI_CHIP_MACH64GI:
        case PCI_CHIP_MACH64GP:
        case PCI_CHIP_MACH64GQ:
        case PCI_CHIP_MACH64LB:
        case PCI_CHIP_MACH64LD:
        case PCI_CHIP_MACH64LI:
        case PCI_CHIP_MACH64LP:
        case PCI_CHIP_MACH64LQ:
        case PCI_CHIP_MACH64GL:
        case PCI_CHIP_MACH64GM:
        case PCI_CHIP_MACH64GN:
        case PCI_CHIP_MACH64GO:
        case PCI_CHIP_MACH64GR:
        case PCI_CHIP_MACH64GS:
        case PCI_CHIP_MACH64LM:
        case PCI_CHIP_MACH64LN:
        case PCI_CHIP_MACH64LR:
        case PCI_CHIP_MACH64LS:
            return ATI_CHIP_FAMILY_Mach64;

        case PCI_CHIP_RAGE128RE:
        case PCI_CHIP_RAGE128RF:
        case PCI_CHIP_RAGE128RG:
        case PCI_CHIP_RAGE128SK:
        case PCI_CHIP_RAGE128SL:
        case PCI_CHIP_RAGE128SM:
        case PCI_CHIP_RAGE128SN:
        case PCI_CHIP_RAGE128RK:
        case PCI_CHIP_RAGE128RL:
        case PCI_CHIP_RAGE128SE:
        case PCI_CHIP_RAGE128SF:
        case PCI_CHIP_RAGE128SG:
        case PCI_CHIP_RAGE128SH:
        case PCI_CHIP_RAGE128PA:
        case PCI_CHIP_RAGE128PB:
        case PCI_CHIP_RAGE128PC:
        case PCI_CHIP_RAGE128PD:
        case PCI_CHIP_RAGE128PE:
        case PCI_CHIP_RAGE128PF:
        case PCI_CHIP_RAGE128PG:
        case PCI_CHIP_RAGE128PH:
        case PCI_CHIP_RAGE128PI:
        case PCI_CHIP_RAGE128PJ:
        case PCI_CHIP_RAGE128PK:
        case PCI_CHIP_RAGE128PL:
        case PCI_CHIP_RAGE128PM:
        case PCI_CHIP_RAGE128PN:
        case PCI_CHIP_RAGE128PO:
        case PCI_CHIP_RAGE128PP:
        case PCI_CHIP_RAGE128PQ:
        case PCI_CHIP_RAGE128PR:
        case PCI_CHIP_RAGE128PS:
        case PCI_CHIP_RAGE128PT:
        case PCI_CHIP_RAGE128PU:
        case PCI_CHIP_RAGE128PV:
        case PCI_CHIP_RAGE128PW:
        case PCI_CHIP_RAGE128PX:
        case PCI_CHIP_RAGE128TF:
        case PCI_CHIP_RAGE128TL:
        case PCI_CHIP_RAGE128TR:
        case PCI_CHIP_RAGE128TS:
        case PCI_CHIP_RAGE128TT:
        case PCI_CHIP_RAGE128TU:
        case PCI_CHIP_RAGE128LE:
        case PCI_CHIP_RAGE128LF:
#if 0
        case PCI_CHIP_RAGE128LK:
        case PCI_CHIP_RAGE128LL:
#endif
        case PCI_CHIP_RAGE128MF:
        case PCI_CHIP_RAGE128ML:
            return ATI_CHIP_FAMILY_Rage128;

        default:
            return ATI_CHIP_FAMILY_Radeon;
    }
}
