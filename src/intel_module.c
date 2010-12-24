/*
 * Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86cmap.h"

#include "common.h"
#include "intel.h"
#include "intel_driver.h"
#include "legacy/legacy.h"

#include <xf86drmMode.h>

static const SymTabRec _intel_chipsets[] = {
    {PCI_CHIP_I810,		"i810"},
    {PCI_CHIP_I810_DC100,	"i810-dc100"},
    {PCI_CHIP_I810_E,		"i810e"},
    {PCI_CHIP_I815,		"i815"},
    {PCI_CHIP_I830_M,		"i830M"},
    {PCI_CHIP_845_G,		"845G"},
    {PCI_CHIP_I854,		"854"},
    {PCI_CHIP_I855_GM,		"852GM/855GM"},
    {PCI_CHIP_I865_G,		"865G"},
    {PCI_CHIP_I915_G,		"915G"},
    {PCI_CHIP_E7221_G,		"E7221 (i915)"},
    {PCI_CHIP_I915_GM,		"915GM"},
    {PCI_CHIP_I945_G,		"945G"},
    {PCI_CHIP_I945_GM,		"945GM"},
    {PCI_CHIP_I945_GME,		"945GME"},
    {PCI_CHIP_IGD_GM,		"Pineview GM"},
    {PCI_CHIP_IGD_G,		"Pineview G"},
    {PCI_CHIP_I965_G,		"965G"},
    {PCI_CHIP_G35_G,		"G35"},
    {PCI_CHIP_I965_Q,		"965Q"},
    {PCI_CHIP_I946_GZ,		"946GZ"},
    {PCI_CHIP_I965_GM,		"965GM"},
    {PCI_CHIP_I965_GME,		"965GME/GLE"},
    {PCI_CHIP_G33_G,		"G33"},
    {PCI_CHIP_Q35_G,		"Q35"},
    {PCI_CHIP_Q33_G,		"Q33"},
    {PCI_CHIP_GM45_GM,		"GM45"},
    {PCI_CHIP_IGD_E_G,		"4 Series"},
    {PCI_CHIP_G45_G,		"G45/G43"},
    {PCI_CHIP_Q45_G,		"Q45/Q43"},
    {PCI_CHIP_G41_G,		"G41"},
    {PCI_CHIP_B43_G,		"B43"},
    {PCI_CHIP_B43_G1,		"B43"},
    {PCI_CHIP_IGDNG_D_G,		"Clarkdale"},
    {PCI_CHIP_IGDNG_M_G,		"Arrandale"},
    {PCI_CHIP_SANDYBRIDGE_GT1,	"Sandybridge" },
    {PCI_CHIP_SANDYBRIDGE_GT2,	"Sandybridge" },
    {PCI_CHIP_SANDYBRIDGE_GT2_PLUS,	"Sandybridge" },
    {PCI_CHIP_SANDYBRIDGE_M_GT1,	"Sandybridge" },
    {PCI_CHIP_SANDYBRIDGE_M_GT2,	"Sandybridge" },
    {PCI_CHIP_SANDYBRIDGE_M_GT2_PLUS,	"Sandybridge" },
    {PCI_CHIP_SANDYBRIDGE_S_GT,	"Sandybridge" },
    {-1,				NULL}
};
SymTabRec *intel_chipsets = (SymTabRec *) _intel_chipsets;

#define INTEL_DEVICE_MATCH(d,i) \
{ 0x8086, (d), PCI_MATCH_ANY, PCI_MATCH_ANY, 0, 0, (i) }

static const struct pci_id_match intel_device_match[] = {
    INTEL_DEVICE_MATCH (PCI_CHIP_I810, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I810_DC100, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I810_E, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I815, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I830_M, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_845_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I854, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I855_GM, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I865_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I915_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_E7221_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I915_GM, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I945_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I945_GM, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I945_GME, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_IGD_GM, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_IGD_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I965_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_G35_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I965_Q, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I946_GZ, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I965_GM, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_I965_GME, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_G33_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_Q35_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_Q33_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_GM45_GM, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_IGD_E_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_G45_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_Q45_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_G41_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_B43_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_IGDNG_D_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_IGDNG_M_G, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_SANDYBRIDGE_GT1, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_SANDYBRIDGE_GT2, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_SANDYBRIDGE_GT2_PLUS, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_SANDYBRIDGE_M_GT1, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_SANDYBRIDGE_M_GT2, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_SANDYBRIDGE_M_GT2_PLUS, 0 ),
    INTEL_DEVICE_MATCH (PCI_CHIP_SANDYBRIDGE_S_GT, 0 ),
    { 0, 0, 0 },
};

static PciChipsets intel_pci_chipsets[] = {
    {PCI_CHIP_I810,		PCI_CHIP_I810,		NULL},
    {PCI_CHIP_I810_DC100,	PCI_CHIP_I810_DC100,	NULL},
    {PCI_CHIP_I810_E,		PCI_CHIP_I810_E,	NULL},
    {PCI_CHIP_I815,		PCI_CHIP_I815,		NULL},
    {PCI_CHIP_I830_M,		PCI_CHIP_I830_M,	NULL},
    {PCI_CHIP_845_G,		PCI_CHIP_845_G,		NULL},
    {PCI_CHIP_I854,		PCI_CHIP_I854,		NULL},
    {PCI_CHIP_I855_GM,		PCI_CHIP_I855_GM,	NULL},
    {PCI_CHIP_I865_G,		PCI_CHIP_I865_G,	NULL},
    {PCI_CHIP_I915_G,		PCI_CHIP_I915_G,	NULL},
    {PCI_CHIP_E7221_G,		PCI_CHIP_E7221_G,	NULL},
    {PCI_CHIP_I915_GM,		PCI_CHIP_I915_GM,	NULL},
    {PCI_CHIP_I945_G,		PCI_CHIP_I945_G,	NULL},
    {PCI_CHIP_I945_GM,		PCI_CHIP_I945_GM,	NULL},
    {PCI_CHIP_I945_GME,		PCI_CHIP_I945_GME,	NULL},
    {PCI_CHIP_IGD_GM,		PCI_CHIP_IGD_GM,	NULL},
    {PCI_CHIP_IGD_G,		PCI_CHIP_IGD_G,		NULL},
    {PCI_CHIP_I965_G,		PCI_CHIP_I965_G,	NULL},
    {PCI_CHIP_G35_G,		PCI_CHIP_G35_G,		NULL},
    {PCI_CHIP_I965_Q,		PCI_CHIP_I965_Q,	NULL},
    {PCI_CHIP_I946_GZ,		PCI_CHIP_I946_GZ,	NULL},
    {PCI_CHIP_I965_GM,		PCI_CHIP_I965_GM,	NULL},
    {PCI_CHIP_I965_GME,		PCI_CHIP_I965_GME,	NULL},
    {PCI_CHIP_G33_G,		PCI_CHIP_G33_G,		NULL},
    {PCI_CHIP_Q35_G,		PCI_CHIP_Q35_G,		NULL},
    {PCI_CHIP_Q33_G,		PCI_CHIP_Q33_G,		NULL},
    {PCI_CHIP_GM45_GM,		PCI_CHIP_GM45_GM,	NULL},
    {PCI_CHIP_IGD_E_G,		PCI_CHIP_IGD_E_G,	NULL},
    {PCI_CHIP_G45_G,		PCI_CHIP_G45_G,		NULL},
    {PCI_CHIP_Q45_G,		PCI_CHIP_Q45_G,		NULL},
    {PCI_CHIP_G41_G,		PCI_CHIP_G41_G,		NULL},
    {PCI_CHIP_B43_G,		PCI_CHIP_B43_G,		NULL},
    {PCI_CHIP_IGDNG_D_G,		PCI_CHIP_IGDNG_D_G,	NULL},
    {PCI_CHIP_IGDNG_M_G,		PCI_CHIP_IGDNG_M_G,	NULL},
    {PCI_CHIP_SANDYBRIDGE_GT1,	PCI_CHIP_SANDYBRIDGE_GT1,	NULL},
    {PCI_CHIP_SANDYBRIDGE_GT2,	PCI_CHIP_SANDYBRIDGE_GT2,	NULL},
    {PCI_CHIP_SANDYBRIDGE_GT2_PLUS,	PCI_CHIP_SANDYBRIDGE_GT2_PLUS,	NULL},
    {PCI_CHIP_SANDYBRIDGE_M_GT1,	PCI_CHIP_SANDYBRIDGE_M_GT1,	NULL},
    {PCI_CHIP_SANDYBRIDGE_M_GT2,	PCI_CHIP_SANDYBRIDGE_M_GT2,	NULL},
    {PCI_CHIP_SANDYBRIDGE_M_GT2_PLUS,	PCI_CHIP_SANDYBRIDGE_M_GT2_PLUS, NULL},
    {PCI_CHIP_SANDYBRIDGE_S_GT,		PCI_CHIP_SANDYBRIDGE_S_GT,	NULL},
    {-1,				-1, NULL }
};

void intel_detect_chipset(ScrnInfoPtr scrn,
			  struct pci_device *pci,
			  struct intel_chipset *chipset)
{
    uint32_t capid;

    switch (DEVICE_ID(pci)) {
    case PCI_CHIP_I810:
	chipset->name = "i810";
	break;
    case PCI_CHIP_I810_DC100:
	chipset->name = "i810-dc100";
	break;
    case PCI_CHIP_I810_E:
	chipset->name = "i810e";
	break;
    case PCI_CHIP_I815:
	chipset->name = "i815";
	break;
    case PCI_CHIP_I830_M:
	chipset->name = "830M";
	break;
    case PCI_CHIP_845_G:
	chipset->name = "845G";
	break;
    case PCI_CHIP_I854:
	chipset->name = "854";
	break;
    case PCI_CHIP_I855_GM:
	/* Check capid register to find the chipset variant */
	pci_device_cfg_read_u32(pci, &capid, I85X_CAPID);
	chipset->variant =
	    (capid >> I85X_VARIANT_SHIFT) & I85X_VARIANT_MASK;
	switch (chipset->variant) {
	case I855_GM:
	    chipset->name = "855GM";
	    break;
	case I855_GME:
	    chipset->name = "855GME";
	    break;
	case I852_GM:
	    chipset->name = "852GM";
	    break;
	case I852_GME:
	    chipset->name = "852GME";
	    break;
	default:
	    xf86DrvMsg(scrn->scrnIndex, X_INFO,
		       "Unknown 852GM/855GM variant: 0x%x)\n",
		       chipset->variant);
	    chipset->name = "852GM/855GM (unknown variant)";
	    break;
	}
	break;
    case PCI_CHIP_I865_G:
	chipset->name = "865G";
	break;
    case PCI_CHIP_I915_G:
	chipset->name = "915G";
	break;
    case PCI_CHIP_E7221_G:
	chipset->name = "E7221 (i915)";
	break;
    case PCI_CHIP_I915_GM:
	chipset->name = "915GM";
	break;
    case PCI_CHIP_I945_G:
	chipset->name = "945G";
	break;
    case PCI_CHIP_I945_GM:
	chipset->name = "945GM";
	break;
    case PCI_CHIP_I945_GME:
	chipset->name = "945GME";
	break;
    case PCI_CHIP_IGD_GM:
	chipset->name = "Pineview GM";
	break;
    case PCI_CHIP_IGD_G:
	chipset->name = "Pineview G";
	break;
    case PCI_CHIP_I965_G:
	chipset->name = "965G";
	break;
    case PCI_CHIP_G35_G:
	chipset->name = "G35";
	break;
    case PCI_CHIP_I965_Q:
	chipset->name = "965Q";
	break;
    case PCI_CHIP_I946_GZ:
	chipset->name = "946GZ";
	break;
    case PCI_CHIP_I965_GM:
	chipset->name = "965GM";
	break;
    case PCI_CHIP_I965_GME:
	chipset->name = "965GME/GLE";
	break;
    case PCI_CHIP_G33_G:
	chipset->name = "G33";
	break;
    case PCI_CHIP_Q35_G:
	chipset->name = "Q35";
	break;
    case PCI_CHIP_Q33_G:
	chipset->name = "Q33";
	break;
    case PCI_CHIP_GM45_GM:
	chipset->name = "GM45";
	break;
    case PCI_CHIP_IGD_E_G:
	chipset->name = "4 Series";
	break;
    case PCI_CHIP_G45_G:
	chipset->name = "G45/G43";
	break;
    case PCI_CHIP_Q45_G:
	chipset->name = "Q45/Q43";
	break;
    case PCI_CHIP_G41_G:
	chipset->name = "G41";
	break;
    case PCI_CHIP_B43_G:
	chipset->name = "B43";
	break;
    case PCI_CHIP_IGDNG_D_G:
	chipset->name = "Clarkdale";
	break;
    case PCI_CHIP_IGDNG_M_G:
	chipset->name = "Arrandale";
	break;
    case PCI_CHIP_SANDYBRIDGE_GT1:
    case PCI_CHIP_SANDYBRIDGE_GT2:
    case PCI_CHIP_SANDYBRIDGE_GT2_PLUS:
    case PCI_CHIP_SANDYBRIDGE_M_GT1:
    case PCI_CHIP_SANDYBRIDGE_M_GT2:
    case PCI_CHIP_SANDYBRIDGE_M_GT2_PLUS:
    case PCI_CHIP_SANDYBRIDGE_S_GT:
	chipset->name = "Sandybridge";
	break;
    default:
	chipset->name = "unknown chipset";
	break;
    }

    xf86DrvMsg(scrn->scrnIndex, X_INFO,
	       "Integrated Graphics Chipset: Intel(R) %s\n", chipset->name);
}

/*
 * intel_identify --
 *
 * Returns the string name for the driver based on the chipset.
 *
 */
static void intel_identify(int flags)
{
    xf86PrintChipsets(INTEL_NAME,
		      "Driver for Intel Integrated Graphics Chipsets",
		      intel_chipsets);
}

static Bool intel_driver_func(ScrnInfoPtr pScrn,
			      xorgDriverFuncOp op,
			      pointer ptr)
{
    xorgHWFlags *flag;

    switch (op) {
    case GET_REQUIRED_HW_INTERFACES:
	flag = (CARD32*)ptr;
#ifdef KMS_ONLY
	(*flag) = 0;
#else
	(*flag) = HW_IO | HW_MMIO;
#endif
	return TRUE;
    default:
	/* Unknown or deprecated function */
	return FALSE;
    }
}

static Bool has_kernel_mode_setting(struct pci_device *dev)
{
	char id[20];
	int ret;

	snprintf(id, sizeof(id),
		 "pci:%04x:%02x:%02x.%d",
		 dev->domain, dev->bus, dev->dev, dev->func);

	ret = drmCheckModesettingSupported(id);
	if (ret) {
		if (xf86LoadKernelModule("i915"))
			ret = drmCheckModesettingSupported(id);
	}
	/* Be nice to the user and load fbcon too */
	if (!ret)
		(void)xf86LoadKernelModule("fbcon");

	return ret == 0;
}

/*
 * intel_pci_probe --
 *
 * Look through the PCI bus to find cards that are intel boards.
 * Setup the dispatch table for the rest of the driver functions.
 *
 */
static Bool intel_pci_probe (DriverPtr		driver,
			     int		entity_num,
			     struct pci_device	*device,
			     intptr_t		match_data)
{
    ScrnInfoPtr scrn;

    if (!has_kernel_mode_setting(device)) {
#if KMS_ONLY
	    return FALSE;
#else
	    switch (DEVICE_ID(device)) {
	    case PCI_CHIP_I810:
	    case PCI_CHIP_I810_DC100:
	    case PCI_CHIP_I810_E:
	    case PCI_CHIP_I815:
		    break;
	    default:
		    return FALSE;
	    }
#endif
    }

    scrn = xf86ConfigPciEntity(NULL, 0, entity_num, intel_pci_chipsets,
			       NULL, NULL, NULL, NULL, NULL);
    if (scrn != NULL) {
	scrn->driverVersion = INTEL_VERSION;
	scrn->driverName = INTEL_DRIVER_NAME;
	scrn->name = INTEL_NAME;
	scrn->Probe = NULL;

#if KMS_ONLY
	intel_init_scrn(scrn);
#else
	switch (DEVICE_ID(device)) {
	case PCI_CHIP_I810:
	case PCI_CHIP_I810_DC100:
	case PCI_CHIP_I810_E:
	case PCI_CHIP_I815:
	    lg_i810_init(scrn);
	    break;

	default:
	    intel_init_scrn(scrn);
	    break;
	}
#endif
    }
    return scrn != NULL;
}

#ifdef XFree86LOADER

static MODULESETUPPROTO(intel_setup);

static XF86ModuleVersionInfo intel_version = {
    "intel",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    INTEL_VERSION_MAJOR, INTEL_VERSION_MINOR, INTEL_VERSION_PATCH,
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0}
};

static const OptionInfoRec *
intel_available_options(int chipid, int busid)
{
#if KMS_ONLY
	return intel_uxa_available_options(chipid, busid);
#else
	switch (chipid) {
	case PCI_CHIP_I810:
	case PCI_CHIP_I810_DC100:
	case PCI_CHIP_I810_E:
	case PCI_CHIP_I815:
		return lg_i810_available_options(chipid, busid);

	default:
		return intel_uxa_available_options(chipid, busid);
	}
#endif
}

static DriverRec intel = {
    INTEL_VERSION,
    INTEL_DRIVER_NAME,
    intel_identify,
    NULL,
    intel_available_options,
    NULL,
    0,
    intel_driver_func,
    intel_device_match,
    intel_pci_probe
};

static pointer intel_setup(pointer module,
			   pointer opts,
			   int *errmaj,
			   int *errmin)
{
    static Bool setupDone = 0;

    /* This module should be loaded only once, but check to be sure.
    */
    if (!setupDone) {
	setupDone = 1;
	xf86AddDriver(&intel, module, HaveDriverFuncs);

	/*
	 * The return value must be non-NULL on success even though there
	 * is no TearDownProc.
	 */
	return (pointer) 1;
    } else {
	if (errmaj)
	    *errmaj = LDR_ONCEONLY;
	return NULL;
    }
}

_X_EXPORT XF86ModuleData intelModuleData = { &intel_version, intel_setup, NULL };
#endif
