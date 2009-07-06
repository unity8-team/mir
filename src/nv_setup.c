/*
 * Copyright 2003 NVIDIA, Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nv_include.h"

static void nv4GetConfig (NVPtr pNv)
{
	uint32_t reg_FB0 = nvReadFB(pNv, NV_PFB_BOOT_0);

	if (reg_FB0 & 0x00000100)
		pNv->RamAmountKBytes = ((reg_FB0 >> 12) & 0x0F) * 1024 * 2 + 1024 * 2;
	else
		switch (reg_FB0 & 0x00000003) {
		case 0:
			pNv->RamAmountKBytes = 1024 * 32;
			break;
		case 1:
			pNv->RamAmountKBytes = 1024 * 4;
			break;
		case 2:
			pNv->RamAmountKBytes = 1024 * 8;
			break;
		case 3:
		default:
			pNv->RamAmountKBytes = 1024 * 16;
			break;
		}
}

static void nv10GetConfig(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	uint32_t implementation = pNv->Chipset & 0x0ff0;
	struct pci_device *dev = NULL;
	uint32_t data;

	if (implementation == CHIPSET_NFORCE ||
	    implementation == CHIPSET_NFORCE2) {
		dev = pci_device_find_by_slot(0, 0, 0, 1);
		if (!dev) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
				   "couldn't find bridge device\n");
			return;
		}
	}

#if X_BYTE_ORDER == X_BIG_ENDIAN
	if (!(nvReadMC(pNv, 0x0004) & 0x01000001))
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Card is in big endian mode, something is very wrong !\n");
#endif

	if (implementation == CHIPSET_NFORCE) {
		pci_device_cfg_read_u32(dev, &data, 0x7c);
		pNv->RamAmountKBytes = (((data >> 6) & 31) + 1) * 1024;
	} else if (implementation == CHIPSET_NFORCE2) {
		pci_device_cfg_read_u32(dev, &data, 0x84);
		pNv->RamAmountKBytes = (((data >> 4) & 127) + 1) * 1024;
	} else {
		pNv->RamAmountKBytes =
			(nvReadFB(pNv, NV_PFB_CSTATUS) & 0xFFF00000) >> 10;
	}

	if (pNv->RamAmountKBytes > 256*1024)
		pNv->RamAmountKBytes = 256*1024;
}

void
NVCommonSetup(ScrnInfoPtr pScrn)
{
	NVPtr pNv = NVPTR(pScrn);
	uint16_t implementation = pNv->Chipset & 0x0ff0;
 
	/* 0x01000000 is the size */
	pci_device_map_range(pNv->PciInfo, pNv->IOAddress, 0x01000000, PCI_DEV_MAP_FLAG_WRITABLE, (void *)&pNv->REGS);
	pci_device_map_range(pNv->PciInfo, pNv->VRAMPhysical, 0x10000, PCI_DEV_MAP_FLAG_WRITABLE, (void *)&pNv->FB_BAR);

	pNv->alphaCursor = (pNv->NVArch >= 0x11);

	pNv->twoHeads = (pNv->Architecture >= NV_ARCH_10) &&
			(implementation != CHIPSET_NV10) &&
			(implementation != CHIPSET_NV15) &&
			(implementation != CHIPSET_NFORCE) &&
			(implementation != CHIPSET_NV20);

	pNv->gf4_disp_arch = pNv->twoHeads && implementation != CHIPSET_NV11;

	/* nv30 and nv35 have two stage PLLs, but use only one register; they are dealt with separately */
	pNv->two_reg_pll = (implementation == CHIPSET_NV31) ||
			   (implementation == CHIPSET_NV36) ||
			   (pNv->Architecture >= NV_ARCH_40);

	pNv->WaitVSyncPossible = (pNv->Architecture >= NV_ARCH_10) &&
				 (implementation != CHIPSET_NV10);

	pNv->BlendingPossible = ((pNv->Chipset & 0xffff) > CHIPSET_NV04);

    /* look for known laptop chips */
    /* FIXME still probably missing some ids (for randr12, pre-nv40 mobile should be auto-detected) */
    switch(pNv->Chipset & 0xffff) {
    case 0x0098:
    case 0x0099:
    case 0x00C8:
    case 0x00C9:
    case 0x00CC:
    case 0x0112:
    case 0x0144:
    case 0x0146:
    case 0x0148:
    case 0x0149:
    case 0x0160:
    case 0x0164:
    case 0x0166:
    case 0x0167:
    case 0x0168:
    case 0x0169:
    case 0x016B:
    case 0x016C:
    case 0x016D:
    case 0x0174:
    case 0x0175:
    case 0x0176:
    case 0x0177:
    case 0x0179:
    case 0x017C:
    case 0x017D:
    case 0x0186:
    case 0x0187:
    case 0x018D:
    case 0x01D6:
    case 0x01D7:
    case 0x01D8:
    case 0x0228:
    case 0x0244:
    case 0x0286:
    case 0x028C:
    case 0x0297:
    case 0x0298:
    case 0x0299:
    case 0x0316:
    case 0x0317:
    case 0x031A:
    case 0x031B:
    case 0x031C:
    case 0x031D:
    case 0x031E:
    case 0x031F:
    case 0x0324:
    case 0x0325:
    case 0x0328:
    case 0x0329:
    case 0x032C:
    case 0x032D:
    case 0x0347:
    case 0x0348:
    case 0x0349:
    case 0x034B:
    case 0x034C:
    case 0x0397:
    case 0x0398:
    case 0x039B:
        pNv->Mobile = TRUE;
        break;
    default:
        break;
    }

	if (pNv->twoHeads) {
		pNv->vtOWNER = nouveau_hw_get_current_head(pScrn);

		NV_TRACE(pScrn, "Initial CRTC_OWNER is %d\n", pNv->vtOWNER);

		/* we need to ensure the heads are not tied henceforth, or
		 * reading any 8 bit reg on head B will fail.
		 * setting a single arbitrary head solves that */
		NVSetOwner(pNv, 0);
	}

	/* Parse the bios to initialize the card */
	if (!pNv->kms_enable)
		NVParseBios(pScrn);

	if (pNv->Architecture == NV_ARCH_04)
		nv4GetConfig(pNv);
	else
		nv10GetConfig(pScrn);
}

