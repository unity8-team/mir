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
#include "atiident.h"
#include "atiutil.h"
#include "ativersion.h"

#include "r128_probe.h"
#include "radeon_probe.h"

const char *ATIChipsetNames[] =
{
    "ati",
    "mach64",
    "rage128",
    "radeon"
};

static SymTabRec ATIPublicChipsetNames[] =
{
    {ATI_CHIPSET_ATI, "ati"},
    {-1, NULL}
};

/*
 * ATIIdentify --
 *
 * Print the driver's list of chipset names.
 */
void
ATIIdentify
(
    int flags
)
{
    xf86PrintChipsets(ATI_NAME,
        (NumberOf(ATIPublicChipsetNames) <= 2) ?
            "ATI driver (version " ATI_VERSION_NAME ") for chipset" :
            "ATI driver (version " ATI_VERSION_NAME ") for chipsets",
        ATIPublicChipsetNames);
    R128Identify(flags);
    RADEONIdentify(flags);
}

/*
 * ATIIdentProbe --
 *
 * This function determines if the user specified a chipset name acceptable to
 * the driver.  It returns an ATIChipsetType or -1.
 */
int
ATIIdentProbe
(
    const char *ChipsetName
)
{
    int              Chipset;

    static SymTabRec SpecificNames[] =
    {
        {ATI_CHIPSET_MACH64, "mach64"},
        {ATI_CHIPSET_RAGE128, "rage128"},
        {ATI_CHIPSET_RADEON, "radeon"},
        {-1, NULL}
    };

    /* If no Chipset specification, default to "ati" */
    if (!ChipsetName || !*ChipsetName)
        return ATI_CHIPSET_ATI;

    Chipset = xf86StringToToken(ATIPublicChipsetNames, ChipsetName);
    if (Chipset != -1)
        return Chipset;

    /* Check for some other chipset names */
    return xf86StringToToken(SpecificNames, ChipsetName);
}
