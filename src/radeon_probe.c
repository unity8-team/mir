/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *
 * Modified by Marc Aurele La France <tsi@xfree86.org> for ATI driver merge.
 */

#include "ativersion.h"

#include "radeon_probe.h"
#include "radeon_version.h"
#include "atipciids.h"

#include "xf86.h"
#define _XF86MISC_SERVER_
#include <X11/extensions/xf86misc.h>
#include "xf86Resources.h"

#include "radeon_chipset_gen.h"

#include "radeon_pci_chipset_gen.h"

int gRADEONEntityIndex = -1;

/* Return the options for supported chipset 'n'; NULL otherwise */
_X_EXPORT const OptionInfoRec *
RADEONAvailableOptions(int chipid, int busid)
{
    int  i;

    /*
     * Return options defined in the radeon submodule which will have been
     * loaded by this point.
     */
    if ((chipid >> 16) == PCI_VENDOR_ATI)
	chipid -= PCI_VENDOR_ATI << 16;
    for (i = 0; RADEONPciChipsets[i].PCIid > 0; i++) {
	if (chipid == RADEONPciChipsets[i].PCIid)
	    return RADEONOptionsWeak();
    }
    return NULL;
}

/* Return the string name for supported chipset 'n'; NULL otherwise. */
_X_EXPORT void
RADEONIdentify(int flags)
{
    xf86PrintChipsets(RADEON_NAME,
		      "Driver for ATI Radeon chipsets",
		      RADEONChipsets);
}

/* Return TRUE if chipset is present; FALSE otherwise. */
_X_EXPORT Bool
RADEONProbe(DriverPtr drv, int flags)
{
    int      numUsed;
    int      numDevSections, nATIGDev, nRadeonGDev;
    int     *usedChips;
    GDevPtr *devSections, *ATIGDevs, *RadeonGDevs;
    Bool     foundScreen = FALSE;
    int      i;

#ifndef XSERVER_LIBPCIACCESS
    if (!xf86GetPciVideoInfo()) return FALSE;
#endif

    /* Collect unclaimed device sections for both driver names */
    nATIGDev    = xf86MatchDevice(ATI_NAME, &ATIGDevs);
    nRadeonGDev = xf86MatchDevice(RADEON_NAME, &RadeonGDevs);

    if (!(numDevSections = nATIGDev + nRadeonGDev)) return FALSE;

    if (!ATIGDevs) {
	if (!(devSections = RadeonGDevs)) numDevSections = 1;
	else                              numDevSections = nRadeonGDev;
    } if (!RadeonGDevs) {
	devSections    = ATIGDevs;
	numDevSections = nATIGDev;
    } else {
	/* Combine into one list */
	devSections = xnfalloc((numDevSections + 1) * sizeof(GDevPtr));
	(void)memcpy(devSections,
		     ATIGDevs, nATIGDev * sizeof(GDevPtr));
	(void)memcpy(devSections + nATIGDev,
		     RadeonGDevs, nRadeonGDev * sizeof(GDevPtr));
	devSections[numDevSections] = NULL;
	xfree(ATIGDevs);
	xfree(RadeonGDevs);
    }

    numUsed = xf86MatchPciInstances(RADEON_NAME,
				    PCI_VENDOR_ATI,
				    RADEONChipsets,
				    RADEONPciChipsets,
				    devSections,
				    numDevSections,
				    drv,
				    &usedChips);

    if (numUsed <= 0) return FALSE;

    if (flags & PROBE_DETECT) {
	foundScreen = TRUE;
    } else {
	for (i = 0; i < numUsed; i++) {
	    ScrnInfoPtr    pScrn = NULL;
	    EntityInfoPtr  pEnt;
	    pEnt = xf86GetEntityInfo(usedChips[i]);
	    if ((pScrn = xf86ConfigPciEntity(pScrn, 0, usedChips[i],
					     RADEONPciChipsets, 0, 0, 0,
					     0, 0))) {
		pScrn->driverVersion = RADEON_VERSION_CURRENT;
		pScrn->driverName    = RADEON_DRIVER_NAME;
		pScrn->name          = RADEON_NAME;
		pScrn->Probe         = RADEONProbe;
		pScrn->PreInit       = RADEONPreInit;
		pScrn->ScreenInit    = RADEONScreenInit;
		pScrn->SwitchMode    = RADEONSwitchMode;
#ifdef X_XF86MiscPassMessage
		pScrn->HandleMessage = RADEONHandleMessage;
#endif
		pScrn->AdjustFrame   = RADEONAdjustFrame;
		pScrn->EnterVT       = RADEONEnterVT;
		pScrn->LeaveVT       = RADEONLeaveVT;
		pScrn->FreeScreen    = RADEONFreeScreen;
		pScrn->ValidMode     = RADEONValidMode;

		foundScreen          = TRUE;
	    }

            xfree(pEnt);
	    pEnt = xf86GetEntityInfo(usedChips[i]);

            /* create a RADEONEntity for all chips, even with
               old single head Radeon, need to use pRADEONEnt
               for new monitor detection routines
            */
            {
		DevUnion   *pPriv;
		RADEONEntPtr pRADEONEnt;

		xf86SetEntitySharable(usedChips[i]);

		if (gRADEONEntityIndex == -1)
		    gRADEONEntityIndex = xf86AllocateEntityPrivateIndex();

		pPriv = xf86GetEntityPrivate(pEnt->index,
					     gRADEONEntityIndex);

		if (!pPriv->ptr) {
		    int j;
		    int instance = xf86GetNumEntityInstances(pEnt->index);

		    for (j = 0; j < instance; j++)
			xf86SetEntityInstanceForScreen(pScrn, pEnt->index, j);

		    pPriv->ptr = xnfcalloc(sizeof(RADEONEntRec), 1);
		    pRADEONEnt = pPriv->ptr;
		    pRADEONEnt->HasSecondary = FALSE;
		} else {
		    pRADEONEnt = pPriv->ptr;
		    pRADEONEnt->HasSecondary = TRUE;
		}
	    }
	    xfree(pEnt);
	}
    }

    xfree(usedChips);
    xfree(devSections);

    return foundScreen;
}
