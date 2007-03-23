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
#include "atichip.h"
#include "atifillin.h"
#include "atimodule.h"
#include "atimach64io.h"
#include "atimach64probe.h"
#include "atioption.h"
#include "ativersion.h"

static SymTabRec
Mach64Chipsets[] = {
    {ATI_CHIP_88800GXC, "ATI 88800GX-C"},
    {ATI_CHIP_88800GXD, "ATI 88800GX-D"},
    {ATI_CHIP_88800GXE, "ATI 88800GX-E"},
    {ATI_CHIP_88800GXF, "ATI 88800GX-F"},
    {ATI_CHIP_88800GX,  "ATI 88800GX"},
    {ATI_CHIP_88800CX,  "ATI 88800CX"},
    {ATI_CHIP_264CT,    "ATI 264CT"},
    {ATI_CHIP_264ET,    "ATI 264ET"},
    {ATI_CHIP_264VT,    "ATI 264VT"},
    {ATI_CHIP_264VTB,   "ATI 264VT-B"},
    {ATI_CHIP_264GT,    "ATI 3D Rage"},
    {ATI_CHIP_264GTB,   "ATI 3D Rage II"},
    {ATI_CHIP_264VT3,   "ATI 264VT3"},
    {ATI_CHIP_264GTDVD, "ATI 3D Rage II+DVD"},
    {ATI_CHIP_264LT,    "ATI 3D Rage LT"},
    {ATI_CHIP_264VT4,   "ATI 264VT4"},
    {ATI_CHIP_264GT2C,  "ATI 3D Rage IIc"},
    {ATI_CHIP_264GTPRO, "ATI 3D Rage Pro"},
    {ATI_CHIP_264LTPRO, "ATI 3D Rage LT Pro"},
    {ATI_CHIP_264XL,    "ATI 3D Rage XL or XC"},
    {ATI_CHIP_MOBILITY, "ATI 3D Rage Mobility"},
    {-1,      NULL }
};

/*
 * This table maps a PCI device ID to a chipset family identifier.
 */
static PciChipsets
Mach64PciChipsets[] = {
    {ATI_CHIP_88800GX,   PCI_CHIP_MACH64GX,  RES_SHARED_VGA},
    {ATI_CHIP_88800CX,   PCI_CHIP_MACH64CX,  RES_SHARED_VGA},
    {ATI_CHIP_264CT,     PCI_CHIP_MACH64CT,  RES_SHARED_VGA},
    {ATI_CHIP_264ET,     PCI_CHIP_MACH64ET,  RES_SHARED_VGA},
    {ATI_CHIP_264VT,     PCI_CHIP_MACH64VT,  RES_SHARED_VGA},
    {ATI_CHIP_264GT,     PCI_CHIP_MACH64GT,  RES_SHARED_VGA},
    {ATI_CHIP_264VT3,    PCI_CHIP_MACH64VU,  RES_SHARED_VGA},
    {ATI_CHIP_264GTDVD,  PCI_CHIP_MACH64GU,  RES_SHARED_VGA},
    {ATI_CHIP_264LT,     PCI_CHIP_MACH64LG,  RES_SHARED_VGA},
    {ATI_CHIP_264VT4,    PCI_CHIP_MACH64VV,  RES_SHARED_VGA},
    {ATI_CHIP_264GT2C,   PCI_CHIP_MACH64GV,  RES_SHARED_VGA},
    {ATI_CHIP_264GT2C,   PCI_CHIP_MACH64GW,  RES_SHARED_VGA},
    {ATI_CHIP_264GT2C,   PCI_CHIP_MACH64GY,  RES_SHARED_VGA},
    {ATI_CHIP_264GT2C,   PCI_CHIP_MACH64GZ,  RES_SHARED_VGA},
    {ATI_CHIP_264GTPRO,  PCI_CHIP_MACH64GB,  RES_SHARED_VGA},
    {ATI_CHIP_264GTPRO,  PCI_CHIP_MACH64GD,  RES_SHARED_VGA},
    {ATI_CHIP_264GTPRO,  PCI_CHIP_MACH64GI,  RES_SHARED_VGA},
    {ATI_CHIP_264GTPRO,  PCI_CHIP_MACH64GP,  RES_SHARED_VGA},
    {ATI_CHIP_264GTPRO,  PCI_CHIP_MACH64GQ,  RES_SHARED_VGA},
    {ATI_CHIP_264LTPRO,  PCI_CHIP_MACH64LB,  RES_SHARED_VGA},
    {ATI_CHIP_264LTPRO,  PCI_CHIP_MACH64LD,  RES_SHARED_VGA},
    {ATI_CHIP_264LTPRO,  PCI_CHIP_MACH64LI,  RES_SHARED_VGA},
    {ATI_CHIP_264LTPRO,  PCI_CHIP_MACH64LP,  RES_SHARED_VGA},
    {ATI_CHIP_264LTPRO,  PCI_CHIP_MACH64LQ,  RES_SHARED_VGA},
    {ATI_CHIP_264XL,     PCI_CHIP_MACH64GL,  RES_SHARED_VGA},
    {ATI_CHIP_264XL,     PCI_CHIP_MACH64GM,  RES_SHARED_VGA},
    {ATI_CHIP_264XL,     PCI_CHIP_MACH64GN,  RES_SHARED_VGA},
    {ATI_CHIP_264XL,     PCI_CHIP_MACH64GO,  RES_SHARED_VGA},
    {ATI_CHIP_264XL,     PCI_CHIP_MACH64GR,  RES_SHARED_VGA},
    {ATI_CHIP_264XL,     PCI_CHIP_MACH64GS,  RES_SHARED_VGA},
    {ATI_CHIP_MOBILITY,  PCI_CHIP_MACH64LM,  RES_SHARED_VGA},
    {ATI_CHIP_MOBILITY,  PCI_CHIP_MACH64LN,  RES_SHARED_VGA},
    {ATI_CHIP_MOBILITY,  PCI_CHIP_MACH64LR,  RES_SHARED_VGA},
    {ATI_CHIP_MOBILITY,  PCI_CHIP_MACH64LS,  RES_SHARED_VGA},
    {-1, -1, RES_UNDEFINED}
};

_X_EXPORT const OptionInfoRec *
Mach64AvailableOptions(int chipid, int busid)
{
    /*
     * Return options defined in the atimisc submodule which will have been
     * loaded by this point.
     */
    return ATIOptionsWeak();
}

/*
 * Mach64Identify --
 *
 * Print the driver's list of chipset names.
 */
_X_EXPORT void
Mach64Identify
(
    int flags
)
{
    xf86Msg(X_INFO, "%s: %s\n", ATI_NAME,
            "Driver for ATI Mach64 chipsets");
}

/*
 * Mach64Probe --
 *
 * This function is called once, at the start of the first server generation to
 * do a minimal probe for supported hardware.
 */
_X_EXPORT Bool
Mach64Probe(DriverPtr pDriver, int flags)
{
    GDevPtr  *devSections;
    int  *usedChips;
    int  numDevSections;
    int  numUsed;
    Bool  ProbeSuccess = FALSE;

    if ((numDevSections = xf86MatchDevice(ATI_DRIVER_NAME, &devSections)) <= 0)
        return FALSE;

    if (xf86GetPciVideoInfo() == NULL)
        return FALSE;

    numUsed = xf86MatchPciInstances(ATI_DRIVER_NAME, PCI_VENDOR_ATI,
                                    Mach64Chipsets, Mach64PciChipsets,
                                    devSections, numDevSections,
                                    pDriver, &usedChips);
    xfree(devSections);

    if (numUsed <= 0)
        return FALSE;

    if (flags & PROBE_DETECT) {
        ProbeSuccess = TRUE;
    } else {
        int  i;

        for (i = 0; i < numUsed; i++) {
            ScrnInfoPtr pScrn;
            EntityInfoPtr pEnt;
            pciVideoPtr pVideo;

            pScrn = xf86ConfigPciEntity(NULL, 0, usedChips[i], Mach64PciChipsets,
                                        0, 0, 0, 0, NULL);

            if (!pScrn)
                continue;

            pEnt = xf86GetEntityInfo(usedChips[i]);
            pVideo = xf86GetPciInfoForEntity(usedChips[i]);

            ATIFillInScreenInfo(pScrn);

            pScrn->Probe = Mach64Probe;

            ProbeSuccess = TRUE;
        }
    }

    return ProbeSuccess;
}
