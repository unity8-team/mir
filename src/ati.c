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
#include "atimach64probe.h"

#include "radeon_probe.h"
#include "r128_probe.h"

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
    ATIChipType Chip;

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
        Chip = ATIChipID(PCI_DEV_DEVICE_ID(pVideo), PCI_DEV_REVISION(pVideo));
        if (Chip <= ATI_CHIP_Mach64)
            DoMach64 = TRUE;
        else if (Chip <= ATI_CHIP_Rage128)
            DoRage128 = TRUE;
        else if (Chip <= ATI_CHIP_Radeon)
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
        Chip = ATIChipID(PCI_DEV_DEVICE_ID(pVideo), PCI_DEV_REVISION(pVideo));
        if (Chip <= ATI_CHIP_Mach64)
            DoMach64 = TRUE;
        else if (Chip <= ATI_CHIP_Rage128)
            DoRage128 = TRUE;
        else if (Chip <= ATI_CHIP_Radeon)
            DoRadeon = TRUE;
    }

    pci_iterator_destroy(pVideoIter);

#endif /* XSERVER_LIBPCIACCESS */

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

/*
 * Chip-related definitions.
 */
const char *ATIChipNames[] =
{
    "Unknown",
    "ATI 88800GX-C",
    "ATI 88800GX-D",
    "ATI 88800GX-E",
    "ATI 88800GX-F",
    "ATI 88800GX",
    "ATI 88800CX",
    "ATI 264CT",
    "ATI 264ET",
    "ATI 264VT",
    "ATI 3D Rage",
    "ATI 264VT-B",
    "ATI 3D Rage II",
    "ATI 264VT3",
    "ATI 3D Rage II+DVD",
    "ATI 3D Rage LT",
    "ATI 264VT4",
    "ATI 3D Rage IIc",
    "ATI 3D Rage Pro",
    "ATI 3D Rage LT Pro",
    "ATI 3D Rage XL or XC",
    "ATI 3D Rage Mobility",
    "ATI unknown Mach64",
    "ATI Rage 128 GL",
    "ATI Rage 128 VR",
    "ATI Rage 128 Pro GL",
    "ATI Rage 128 Pro VR",
    "ATI Rage 128 Pro ULTRA",
    "ATI Rage 128 Mobility M3",
    "ATI Rage 128 Mobility M4",
    "ATI unknown Rage 128"
    "ATI Radeon 7200",
    "ATI Radeon 7000 (VE)",
    "ATI Radeon Mobility M6",
    "ATI Radeon IGP320",
    "ATI Radeon IGP330/340/350",
    "ATI Radeon 7000 IGP",
    "ATI Radeon 7500",
    "ATI Radeon Mobility M7",
    "ATI Radeon 8500/9100",
    "ATI Radeon 9000",
    "ATI Radeon Mobility M9",
    "ATI Radeon 9100 IGP",
    "ATI Radeon 9200 IGP",
    "ATI Radeon 9200",
    "ATI Radeon Mobility M9+",
    "ATI Radeon 9700/9500",
    "ATI Radeon 9600/9550",
    "ATI Radeon 9800",
    "ATI Radeon 9800XT",
    "ATI Radeon X300/X550/M22",
    "ATI Radeon X600/X550/M24",
    "ATI Radeon X800/M18 AGP",
    "ATI Radeon X800/M28 PCIE",
    "ATI Radeon X800XL PCIE",
    "ATI Radeon X850 PCIE",
    "ATI Radeon X850 AGP",
    "ATI Radeon X700",
    "ATI Xpress 200"
    "ATI unknown Radeon",
    "ATI Rage HDTV"
};

#include "atichip.h"

/*
 * ATIChipID --
 *
 * This returns the ATI_CHIP_* value (generally) associated with a particular
 * ChipID/ChipRev combination.
 */
ATIChipType
ATIChipID
(
    const CARD16 ChipID,
    const CARD8  ChipRev
)
{
    switch (ChipID)
    {
        case OldChipID('G', 'X'):  case NewChipID('G', 'X'):
            switch (ChipRev)
            {
                case 0x00U:
                    return ATI_CHIP_88800GXC;

                case 0x01U:
                    return ATI_CHIP_88800GXD;

                case 0x02U:
                    return ATI_CHIP_88800GXE;

                case 0x03U:
                    return ATI_CHIP_88800GXF;

                default:
                    return ATI_CHIP_88800GX;
            }

        case OldChipID('C', 'X'):  case NewChipID('C', 'X'):
            return ATI_CHIP_88800CX;

        case OldChipID('C', 'T'):  case NewChipID('C', 'T'):
            return ATI_CHIP_264CT;

        case OldChipID('E', 'T'):  case NewChipID('E', 'T'):
            return ATI_CHIP_264ET;

        case OldChipID('V', 'T'):  case NewChipID('V', 'T'):
            /* For simplicity, ignore ChipID discrepancy that can occur here */
            if (!(ChipRev & GetBits(CFG_CHIP_VERSION, CFG_CHIP_REV)))
                return ATI_CHIP_264VT;
            return ATI_CHIP_264VTB;

        case OldChipID('G', 'T'):  case NewChipID('G', 'T'):
            if (!(ChipRev & GetBits(CFG_CHIP_VERSION, CFG_CHIP_REV)))
                return ATI_CHIP_264GT;
            return ATI_CHIP_264GTB;

        case OldChipID('V', 'U'):  case NewChipID('V', 'U'):
            return ATI_CHIP_264VT3;

        case OldChipID('G', 'U'):  case NewChipID('G', 'U'):
            return ATI_CHIP_264GTDVD;

        case OldChipID('L', 'G'):  case NewChipID('L', 'G'):
            return ATI_CHIP_264LT;

        case OldChipID('V', 'V'):  case NewChipID('V', 'V'):
            return ATI_CHIP_264VT4;

        case OldChipID('G', 'V'):  case NewChipID('G', 'V'):
        case OldChipID('G', 'W'):  case NewChipID('G', 'W'):
        case OldChipID('G', 'Y'):  case NewChipID('G', 'Y'):
        case OldChipID('G', 'Z'):  case NewChipID('G', 'Z'):
            return ATI_CHIP_264GT2C;

        case OldChipID('G', 'B'):  case NewChipID('G', 'B'):
        case OldChipID('G', 'D'):  case NewChipID('G', 'D'):
        case OldChipID('G', 'I'):  case NewChipID('G', 'I'):
        case OldChipID('G', 'P'):  case NewChipID('G', 'P'):
        case OldChipID('G', 'Q'):  case NewChipID('G', 'Q'):
            return ATI_CHIP_264GTPRO;

        case OldChipID('L', 'B'):  case NewChipID('L', 'B'):
        case OldChipID('L', 'D'):  case NewChipID('L', 'D'):
        case OldChipID('L', 'I'):  case NewChipID('L', 'I'):
        case OldChipID('L', 'P'):  case NewChipID('L', 'P'):
        case OldChipID('L', 'Q'):  case NewChipID('L', 'Q'):
            return ATI_CHIP_264LTPRO;

        case OldChipID('G', 'L'):  case NewChipID('G', 'L'):
        case OldChipID('G', 'M'):  case NewChipID('G', 'M'):
        case OldChipID('G', 'N'):  case NewChipID('G', 'N'):
        case OldChipID('G', 'O'):  case NewChipID('G', 'O'):
        case OldChipID('G', 'R'):  case NewChipID('G', 'R'):
        case OldChipID('G', 'S'):  case NewChipID('G', 'S'):
            return ATI_CHIP_264XL;

        case OldChipID('L', 'M'):  case NewChipID('L', 'M'):
        case OldChipID('L', 'N'):  case NewChipID('L', 'N'):
        case OldChipID('L', 'R'):  case NewChipID('L', 'R'):
        case OldChipID('L', 'S'):  case NewChipID('L', 'S'):
            return ATI_CHIP_MOBILITY;

        case NewChipID('R', 'E'):
        case NewChipID('R', 'F'):
        case NewChipID('R', 'G'):
        case NewChipID('S', 'K'):
        case NewChipID('S', 'L'):
        case NewChipID('S', 'M'):
        /* "SN" is listed as ATI_CHIP_RAGE128_4X in ATI docs */
        case NewChipID('S', 'N'):
            return ATI_CHIP_RAGE128GL;

        case NewChipID('R', 'K'):
        case NewChipID('R', 'L'):
        /*
         * ATI documentation lists SE/SF/SG under both ATI_CHIP_RAGE128VR
         * and ATI_CHIP_RAGE128_4X, and lists SH/SK/SL under Rage 128 4X only.
         * I'm stuffing them here for now until this can be clarified as ATI
         * documentation doesn't mention their details. <mharris@redhat.com>
         */
        case NewChipID('S', 'E'):
        case NewChipID('S', 'F'):
        case NewChipID('S', 'G'):
        case NewChipID('S', 'H'):
            return ATI_CHIP_RAGE128VR;

     /* case NewChipID('S', 'H'): */
     /* case NewChipID('S', 'K'): */
     /* case NewChipID('S', 'L'): */
     /* case NewChipID('S', 'N'): */
     /*     return ATI_CHIP_RAGE128_4X; */

        case NewChipID('P', 'A'):
        case NewChipID('P', 'B'):
        case NewChipID('P', 'C'):
        case NewChipID('P', 'D'):
        case NewChipID('P', 'E'):
        case NewChipID('P', 'F'):
            return ATI_CHIP_RAGE128PROGL;

        case NewChipID('P', 'G'):
        case NewChipID('P', 'H'):
        case NewChipID('P', 'I'):
        case NewChipID('P', 'J'):
        case NewChipID('P', 'K'):
        case NewChipID('P', 'L'):
        case NewChipID('P', 'M'):
        case NewChipID('P', 'N'):
        case NewChipID('P', 'O'):
        case NewChipID('P', 'P'):
        case NewChipID('P', 'Q'):
        case NewChipID('P', 'R'):
        case NewChipID('P', 'S'):
        case NewChipID('P', 'T'):
        case NewChipID('P', 'U'):
        case NewChipID('P', 'V'):
        case NewChipID('P', 'W'):
        case NewChipID('P', 'X'):
            return ATI_CHIP_RAGE128PROVR;

        case NewChipID('T', 'F'):
        case NewChipID('T', 'L'):
        case NewChipID('T', 'R'):
        case NewChipID('T', 'S'):
        case NewChipID('T', 'T'):
        case NewChipID('T', 'U'):
            return ATI_CHIP_RAGE128PROULTRA;

        case NewChipID('L', 'E'):
        case NewChipID('L', 'F'):
        /*
         * "LK" and "LL" are not in any ATI documentation I can find
         * - mharris
         */
        case NewChipID('L', 'K'):
        case NewChipID('L', 'L'):
            return ATI_CHIP_RAGE128MOBILITY3;

        case NewChipID('M', 'F'):
        case NewChipID('M', 'L'):
            return ATI_CHIP_RAGE128MOBILITY4;

        case NewChipID('Q', 'D'):
        case NewChipID('Q', 'E'):
        case NewChipID('Q', 'F'):
        case NewChipID('Q', 'G'):
            return ATI_CHIP_RADEON;

        case NewChipID('Q', 'Y'):
        case NewChipID('Q', 'Z'):
        case NewChipID('Q', '^'):
            return ATI_CHIP_RADEONVE;

        case NewChipID('L', 'Y'):
        case NewChipID('L', 'Z'):
            return ATI_CHIP_RADEONMOBILITY6;

        case NewChipID('A', '6'):
        case NewChipID('C', '6'):
             return ATI_CHIP_RS100;

        case NewChipID('A', '7'):
        case NewChipID('C', '7'):
             return ATI_CHIP_RS200;

        case NewChipID('D', '7'):
        case NewChipID('B', '7'):
             return ATI_CHIP_RS250;

        case NewChipID('L', 'W'):
        case NewChipID('L', 'X'):
            return ATI_CHIP_RADEONMOBILITY7;

        case NewChipID('Q', 'H'):
        case NewChipID('Q', 'I'):
        case NewChipID('Q', 'J'):
        case NewChipID('Q', 'K'):
        case NewChipID('Q', 'L'):
        case NewChipID('Q', 'M'):
        case NewChipID('Q', 'N'):
        case NewChipID('Q', 'O'):
        case NewChipID('Q', 'h'):
        case NewChipID('Q', 'i'):
        case NewChipID('Q', 'j'):
        case NewChipID('Q', 'k'):
        case NewChipID('Q', 'l'):
        case NewChipID('B', 'B'):
            return ATI_CHIP_R200;

        case NewChipID('Q', 'W'):
        case NewChipID('Q', 'X'):
            return ATI_CHIP_RV200;

        case NewChipID('I', 'f'):
        case NewChipID('I', 'g'):
            return ATI_CHIP_RV250;

        case NewChipID('L', 'd'):
        case NewChipID('L', 'f'):
        case NewChipID('L', 'g'):
            return ATI_CHIP_RADEONMOBILITY9;

        case NewChipID('X', '4'):
        case NewChipID('X', '5'):
             return ATI_CHIP_RS300;

        case NewChipID('x', '4'):
        case NewChipID('x', '5'):
             return ATI_CHIP_RS350;

        case NewChipID('Y', '\''):
        case NewChipID('Y', 'a'):
        case NewChipID('Y', 'b'):
        case NewChipID('Y', 'd'):
        case NewChipID('Y', 'e'):
            return ATI_CHIP_RV280;

        case NewChipID('\\', 'a'):
        case NewChipID('\\', 'c'):
            return ATI_CHIP_RADEONMOBILITY9PLUS;

        case NewChipID('A', 'D'):
        case NewChipID('A', 'E'):
        case NewChipID('A', 'F'):
        case NewChipID('A', 'G'):
        case NewChipID('N', 'D'):
        case NewChipID('N', 'E'):
        case NewChipID('N', 'F'):
        case NewChipID('N', 'G'):
            return ATI_CHIP_R300;

        case NewChipID('A', 'H'):
        case NewChipID('A', 'I'):
        case NewChipID('A', 'J'):
        case NewChipID('A', 'K'):
        case NewChipID('N', 'H'):
        case NewChipID('N', 'I'):
        case NewChipID('N', 'K'):
            return ATI_CHIP_R350;

        case NewChipID('A', 'P'):
        case NewChipID('A', 'Q'):
        case NewChipID('A', 'R'):
        case NewChipID('A', 'S'):
        case NewChipID('A', 'T'):
        case NewChipID('A', 'U'):
        case NewChipID('A', 'V'):
        case NewChipID('N', 'P'):
        case NewChipID('N', 'Q'):
        case NewChipID('N', 'R'):
        case NewChipID('N', 'S'):
        case NewChipID('N', 'T'):
        case NewChipID('N', 'V'):
            return ATI_CHIP_RV350;

        case NewChipID('N', 'J'):
            return ATI_CHIP_R360;

        case NewChipID('[', '\''):
        case NewChipID('[', 'b'):
        case NewChipID('[', 'c'):
        case NewChipID('[', 'd'):
        case NewChipID('[', 'e'):
        case NewChipID('T', '\''):
        case NewChipID('T', 'b'):
        case NewChipID('T', 'd'):
	    return ATI_CHIP_RV370;

        case NewChipID('>', 'P'):
        case NewChipID('>', 'T'):
        case NewChipID('1', 'P'):
        case NewChipID('1', 'R'):
        case NewChipID('1', 'T'):
	    return ATI_CHIP_RV380;

        case NewChipID('J', 'H'):
        case NewChipID('J', 'I'):
        case NewChipID('J', 'J'):
        case NewChipID('J', 'K'):
        case NewChipID('J', 'L'):
        case NewChipID('J', 'M'):
        case NewChipID('J', 'N'):
        case NewChipID('J', 'O'):
        case NewChipID('J', 'P'):
        case NewChipID('J', 'T'):
	    return ATI_CHIP_R420;

        case NewChipID('U', 'H'):
        case NewChipID('U', 'I'):
        case NewChipID('U', 'J'):
        case NewChipID('U', 'K'):
        case NewChipID('U', 'P'):
        case NewChipID('U', 'Q'):
        case NewChipID('U', 'R'):
        case NewChipID('U', 'T'):
        case NewChipID(']', 'W'):
        /* those are m28, not 100% certain they are r423 could
	   be r480 but not r430 as their pci id names indicate... */
        case NewChipID(']', 'H'):
        case NewChipID(']', 'I'):
        case NewChipID(']', 'J'):
	    return ATI_CHIP_R423;

        case NewChipID('U', 'L'):
        case NewChipID('U', 'M'):
        case NewChipID('U', 'N'):
        case NewChipID('U', 'O'):
	    return ATI_CHIP_R430;

        case NewChipID(']', 'L'):
        case NewChipID(']', 'M'):
        case NewChipID(']', 'N'):
        case NewChipID(']', 'O'):
        case NewChipID(']', 'P'):
        case NewChipID(']', 'R'):
	    return ATI_CHIP_R480;

        case NewChipID('K', 'I'):
        case NewChipID('K', 'J'):
        case NewChipID('K', 'K'):
        case NewChipID('K', 'L'):
	    return ATI_CHIP_R481;

        case NewChipID('^', 'H'):
        case NewChipID('^', 'J'):
        case NewChipID('^', 'K'):
        case NewChipID('^', 'L'):
        case NewChipID('^', 'M'):
        case NewChipID('^', 'O'):
        case NewChipID('V', 'J'):
        case NewChipID('V', 'K'):
        case NewChipID('V', 'O'):
        case NewChipID('V', 'R'):
        case NewChipID('V', 'S'):
	    return ATI_CHIP_RV410;

        case NewChipID('Z', 'A'):
        case NewChipID('Z', 'B'):
        case NewChipID('Z', 'a'):
        case NewChipID('Z', 'b'):
        case NewChipID('Y', 'T'):
        case NewChipID('Y', 'U'):
        case NewChipID('Y', 't'):
        case NewChipID('Y', 'u'):
	    return ATI_CHIP_RS400;

        case NewChipID('H', 'D'):
            return ATI_CHIP_HDTV;

        default:
            /*
             * Treat anything else as an unknown Radeon.  Please keep the above
             * up-to-date however, as it serves as a central chip list.
             */
            return ATI_CHIP_Radeon;
    }
}
