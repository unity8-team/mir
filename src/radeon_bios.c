/*
 * Copyright 2004 ATI Technologies Inc., Markham, Ontario
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

#include "xf86.h"
#include "xf86_OSproc.h"

#include "xf86PciInfo.h"
#include "radeon.h"
#include "radeon_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_atombios.h"
#include "vbe.h"

typedef enum
{
    DDC_NONE_DETECTED,
    DDC_MONID,
    DDC_DVI,
    DDC_VGA,
    DDC_CRT2,
    DDC_LCD,
    DDC_GPIO,
} RADEONLegacyDDCType;

typedef enum
{
    CONNECTOR_NONE_LEGACY,
    CONNECTOR_PROPRIETARY_LEGACY,
    CONNECTOR_CRT_LEGACY,
    CONNECTOR_DVI_I_LEGACY,
    CONNECTOR_DVI_D_LEGACY,
    CONNECTOR_CTV_LEGACY,
    CONNECTOR_STV_LEGACY,
    CONNECTOR_UNSUPPORTED_LEGACY
} RADEONLegacyConnectorType;

/* Read the Video BIOS block and the FP registers (if applicable). */
Bool RADEONGetBIOSInfo(ScrnInfoPtr pScrn, xf86Int10InfoPtr  pInt10)
{
    RADEONInfoPtr info     = RADEONPTR(pScrn);
    int tmp;
    unsigned short dptr;

    if (!(info->VBIOS = xalloc(
#ifdef XSERVER_LIBPCIACCESS
			       info->PciInfo->rom_size
#else
			       RADEON_VBIOS_SIZE
#endif
			       ))) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "Cannot allocate space for hold Video BIOS!\n");
	return FALSE;
    } else {
	if (pInt10) {
	    info->BIOSAddr = pInt10->BIOSseg << 4;
	    (void)memcpy(info->VBIOS, xf86int10Addr(pInt10, info->BIOSAddr),
			 RADEON_VBIOS_SIZE);
	} else {
#ifdef XSERVER_LIBPCIACCESS
	    if (pci_device_read_rom(info->PciInfo, info->VBIOS)) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Failed to read PCI ROM!\n");
	    }
#else
	    xf86ReadPciBIOS(0, info->PciTag, 0, info->VBIOS, RADEON_VBIOS_SIZE);
	    if (info->VBIOS[0] != 0x55 || info->VBIOS[1] != 0xaa) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Video BIOS not detected in PCI space!\n");
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Attempting to read Video BIOS from "
			   "legacy ISA space!\n");
		info->BIOSAddr = 0x000c0000;
		xf86ReadDomainMemory(info->PciTag, info->BIOSAddr,
				     RADEON_VBIOS_SIZE, info->VBIOS);
	    }
#endif
	}
    }

    if (info->VBIOS[0] != 0x55 || info->VBIOS[1] != 0xaa) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Unrecognized BIOS signature, BIOS data will not be used\n");
	xfree (info->VBIOS);
	info->VBIOS = NULL;
	return FALSE;
    }

    /* Verify it's an x86 BIOS not OF firmware, copied from radeonfb */
    dptr = RADEON_BIOS16(0x18);
    /* If PCI data signature is wrong assume x86 video BIOS anyway */
    if (RADEON_BIOS32(dptr) != (('R' << 24) | ('I' << 16) | ('C' << 8) | 'P')) {
       xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "ROM PCI data signature incorrect, ignoring\n");
    }
    else if (info->VBIOS[dptr + 0x14] != 0x0) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Not an x86 BIOS ROM image, BIOS data will not be used\n");
	xfree (info->VBIOS);
	info->VBIOS = NULL;
	return FALSE;
    }

    if (info->VBIOS) info->ROMHeaderStart = RADEON_BIOS16(0x48);

    if(!info->ROMHeaderStart) {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		   "Invalid ROM pointer, BIOS data will not be used\n");
	xfree (info->VBIOS);
	info->VBIOS = NULL;
	return FALSE;
    }
 
    tmp = info->ROMHeaderStart + 4;
    if ((RADEON_BIOS8(tmp)   == 'A' &&
	 RADEON_BIOS8(tmp+1) == 'T' &&
	 RADEON_BIOS8(tmp+2) == 'O' &&
	 RADEON_BIOS8(tmp+3) == 'M') ||
	(RADEON_BIOS8(tmp)   == 'M' &&
	 RADEON_BIOS8(tmp+1) == 'O' &&
	 RADEON_BIOS8(tmp+2) == 'T' &&
	 RADEON_BIOS8(tmp+3) == 'A'))
	info->IsAtomBios = TRUE;
    else
	info->IsAtomBios = FALSE;

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "%s BIOS detected\n",
	       info->IsAtomBios ? "ATOM":"Legacy");

    if (info->IsAtomBios) {
#if 1
        AtomBiosArgRec atomBiosArg;

        if (RHDAtomBiosFunc(pScrn->scrnIndex, NULL, ATOMBIOS_INIT, &atomBiosArg)
            == ATOM_SUCCESS) {
            info->atomBIOS = atomBiosArg.atomhandle;
        }

        atomBiosArg.fb.start = info->FbFreeStart;
        atomBiosArg.fb.size = info->FbFreeSize;
        if (RHDAtomBiosFunc(pScrn->scrnIndex, info->atomBIOS, ATOMBIOS_ALLOCATE_FB_SCRATCH,
			    &atomBiosArg) == ATOM_SUCCESS) {

	    info->FbFreeStart = atomBiosArg.fb.start;
	    info->FbFreeSize = atomBiosArg.fb.size;
        }

        RHDAtomBiosFunc(pScrn->scrnIndex, info->atomBIOS, GET_DEFAULT_ENGINE_CLOCK,
                        &atomBiosArg);
        RHDAtomBiosFunc(pScrn->scrnIndex, info->atomBIOS, GET_DEFAULT_MEMORY_CLOCK,
                        &atomBiosArg);
        RHDAtomBiosFunc(pScrn->scrnIndex, info->atomBIOS,
                        GET_MAX_PIXEL_CLOCK_PLL_OUTPUT, &atomBiosArg);
        RHDAtomBiosFunc(pScrn->scrnIndex, info->atomBIOS,
                        GET_MIN_PIXEL_CLOCK_PLL_OUTPUT, &atomBiosArg);
        RHDAtomBiosFunc(pScrn->scrnIndex, info->atomBIOS,
                        GET_MAX_PIXEL_CLOCK_PLL_INPUT, &atomBiosArg);
        RHDAtomBiosFunc(pScrn->scrnIndex, info->atomBIOS,
			GET_MIN_PIXEL_CLOCK_PLL_INPUT, &atomBiosArg);
        RHDAtomBiosFunc(pScrn->scrnIndex, info->atomBIOS,
			GET_MAX_PIXEL_CLK, &atomBiosArg);
        RHDAtomBiosFunc(pScrn->scrnIndex, info->atomBIOS,
                        GET_REF_CLOCK, &atomBiosArg);

#endif
	info->MasterDataStart = RADEON_BIOS16 (info->ROMHeaderStart + 32);
    }

    return TRUE;
}

static Bool RADEONGetATOMConnectorInfoFromBIOS (ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    int offset, i, j, tmp, tmp0, id, portinfo, gpio;

    if (!info->VBIOS) return FALSE;

    offset = RADEON_BIOS16(info->MasterDataStart + 22);

    if (offset) {
	tmp = RADEON_BIOS16(offset + 4);
	for (i = 0; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	    if (tmp & (1 << i)) {

		if (i == ATOM_DEVICE_CV_INDEX) {
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Skipping Component Video\n");
		    info->BiosConnector[i].valid = FALSE;
		    continue;
		}

		info->BiosConnector[i].valid = TRUE;
		portinfo = RADEON_BIOS16(offset + 6 + i * 2);
		info->BiosConnector[i].DACType = (portinfo & 0xf) - 1;
		info->BiosConnector[i].ConnectorType = (portinfo >> 4) & 0xf;
		id = (portinfo >> 8) & 0xf;
		tmp0 = RADEON_BIOS16(info->MasterDataStart + 24);
		gpio = RADEON_BIOS16(tmp0 + 4 + 27 * id) * 4;
		/* don't assign a gpio for tv */
		if ((i == ATOM_DEVICE_TV1_INDEX) ||
		    (i == ATOM_DEVICE_TV2_INDEX) ||
		    (i == ATOM_DEVICE_CV_INDEX))
		    info->BiosConnector[i].ddc_line = 0;
		else
		    info->BiosConnector[i].ddc_line = gpio;

		info->BiosConnector[i].output_id = id;

		if (i == ATOM_DEVICE_DFP1_INDEX)
		    info->BiosConnector[i].TMDSType = TMDS_INT;
		else if (i == ATOM_DEVICE_DFP2_INDEX)
		    info->BiosConnector[i].TMDSType = TMDS_EXT;
		else if (i == ATOM_DEVICE_DFP3_INDEX)
		    info->BiosConnector[i].TMDSType = TMDS_EXT;
		else
		    info->BiosConnector[i].TMDSType = TMDS_UNKNOWN;

		/* Always set the connector type to VGA for CRT1/CRT2. if they are
		 * shared with a DVI port, we'll pick up the DVI connector below when we
		 * merge the outputs
		 */
		if ((i == ATOM_DEVICE_CRT1_INDEX || i == ATOM_DEVICE_CRT2_INDEX) &&
		    (info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_I ||
		     info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_D ||
		     info->BiosConnector[i].ConnectorType == CONNECTOR_DVI_A)) {
		    info->BiosConnector[i].ConnectorType = CONNECTOR_VGA;
		}

	    } else {
		info->BiosConnector[i].valid = FALSE;
	    }
	}   
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "No Device Info Table found!\n");
	return FALSE;
    }

    /* CRTs/DFPs may share a port */
    for (i = 0; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	if (info->BiosConnector[i].valid) {
	    for (j = 0; j < RADEON_MAX_BIOS_CONNECTOR; j++) {
		if (info->BiosConnector[j].valid && (i != j) ) {
		    if (info->BiosConnector[i].output_id == info->BiosConnector[j].output_id) {
			if (((i == ATOM_DEVICE_DFP1_INDEX) ||
			     (i == ATOM_DEVICE_DFP2_INDEX) ||
			     (i == ATOM_DEVICE_DFP3_INDEX)) &&
			    ((j == ATOM_DEVICE_CRT1_INDEX) || (j == ATOM_DEVICE_CRT2_INDEX))) {
			    info->BiosConnector[i].DACType = info->BiosConnector[j].DACType;
			    info->BiosConnector[j].valid = FALSE;
			} else if (((j == ATOM_DEVICE_DFP1_INDEX) ||
			     (j == ATOM_DEVICE_DFP2_INDEX) ||
			     (j == ATOM_DEVICE_DFP3_INDEX)) &&
			    ((i == ATOM_DEVICE_CRT1_INDEX) || (i == ATOM_DEVICE_CRT2_INDEX))) {
			    info->BiosConnector[j].DACType = info->BiosConnector[i].DACType;
			    info->BiosConnector[i].valid = FALSE;
			}
			/* other possible combos?  */
		    }
		}
	    }
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Bios Connector table: \n");
    for (i = 0; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	if (info->BiosConnector[i].valid) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Port%d: DDCType-0x%x, DACType-%d, TMDSType-%d, ConnectorType-%d\n",
		       i, info->BiosConnector[i].ddc_line, info->BiosConnector[i].DACType,
		       info->BiosConnector[i].TMDSType, info->BiosConnector[i].ConnectorType);
	}
    }

    return TRUE;
}

static Bool RADEONGetLegacyConnectorInfoFromBIOS (ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    int offset, i, entry, tmp, tmp0, tmp1;
    RADEONLegacyDDCType DDCType;
    RADEONLegacyConnectorType ConnectorType;

    if (!info->VBIOS) return FALSE;

    offset = RADEON_BIOS16(info->ROMHeaderStart + 0x50);
    if (offset) {
	for (i = 0; i < 4; i++) {
	    entry = offset + 2 + i*2;

	    if (!RADEON_BIOS16(entry)) {
		break;
	    }
	    info->BiosConnector[i].valid = TRUE;
	    tmp = RADEON_BIOS16(entry);
	    info->BiosConnector[i].ConnectorType = (tmp >> 12) & 0xf;
	    ConnectorType = (tmp >> 12) & 0xf;
	    switch (ConnectorType) {
	    case CONNECTOR_PROPRIETARY_LEGACY:
		info->BiosConnector[i].ConnectorType = CONNECTOR_LVDS;
		break;
	    case CONNECTOR_CRT_LEGACY:
		info->BiosConnector[i].ConnectorType = CONNECTOR_VGA;
		break;
	    case CONNECTOR_DVI_I_LEGACY:
		info->BiosConnector[i].ConnectorType = CONNECTOR_DVI_I;
		break;
	    case CONNECTOR_DVI_D_LEGACY:
		info->BiosConnector[i].ConnectorType = CONNECTOR_DVI_D;
		break;
	    case CONNECTOR_CTV_LEGACY:
		info->BiosConnector[i].ConnectorType = CONNECTOR_CTV;
		break;
	    case CONNECTOR_STV_LEGACY:
		info->BiosConnector[i].ConnectorType = CONNECTOR_STV;
		break;
	    default:
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Unknown Connector Type: %d\n", ConnectorType);
		info->BiosConnector[i].valid = FALSE;
		break;
	    }
	    DDCType = (tmp >> 8) & 0xf;
	    switch (DDCType) {
	    case DDC_MONID:
		info->BiosConnector[i].ddc_line = RADEON_GPIO_MONID;
		break;
	    case DDC_DVI:
		info->BiosConnector[i].ddc_line = RADEON_GPIO_DVI_DDC;
		break;
	    case DDC_VGA:
		info->BiosConnector[i].ddc_line = RADEON_GPIO_VGA_DDC;
		break;
	    case DDC_CRT2:
		info->BiosConnector[i].ddc_line = RADEON_GPIO_CRT2_DDC;
		break;
	    case DDC_LCD:
		info->BiosConnector[i].ddc_line = RADEON_LCD_GPIO_MASK;
		break;
	    case DDC_GPIO:
		info->BiosConnector[i].ddc_line = RADEON_MDGPIO_EN_REG;
		break;
	    default:
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Unknown DDC Type: %d\n", DDCType);
		break;
	    }
	    info->BiosConnector[i].DACType = tmp & 0x1;
	    info->BiosConnector[i].TMDSType = (tmp >> 4) & 0x1;

	    /* most XPRESS chips seem to specify DDC_CRT2 for their 
	     * VGA DDC port, however DDC never seems to work on that
	     * port.  Some have reported success on DDC_MONID, so 
	     * lets see what happens with that.
	     */
	    if (info->ChipFamily == CHIP_FAMILY_RS400 &&
		info->BiosConnector[i].ConnectorType == CONNECTOR_VGA &&
		info->BiosConnector[i].ddc_line == RADEON_GPIO_CRT2_DDC) {
		info->BiosConnector[i].ddc_line = RADEON_GPIO_MONID;
	    }

	    /* XPRESS desktop chips seem to have a proprietary connector listed for
	     * DVI-D, try and do the right thing here.
	    */
	    if ((!info->IsMobility) &&
		(info->BiosConnector[i].ConnectorType == CONNECTOR_LVDS)) {
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Proprietary connector found, assuming DVI-D\n");
		info->BiosConnector[i].DACType = DAC_NONE;
		info->BiosConnector[i].TMDSType = TMDS_EXT;
		info->BiosConnector[i].ConnectorType = CONNECTOR_DVI_D;
	    }
	}
    } else {
	xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "No Connector Info Table found!\n");
	return FALSE;
    }

    /* check LVDS table */
    if (info->IsMobility) {
	offset = RADEON_BIOS16(info->ROMHeaderStart + 0x40);
	if (offset) {
	    info->BiosConnector[4].valid = TRUE;
	    info->BiosConnector[4].ConnectorType = CONNECTOR_LVDS;
	    info->BiosConnector[4].DACType = DAC_NONE;
	    info->BiosConnector[4].TMDSType = TMDS_NONE;

	    tmp = RADEON_BIOS16(info->ROMHeaderStart + 0x42);
	    if (tmp) {
		tmp0 = RADEON_BIOS16(tmp + 0x15);
		if (tmp0) {
		    tmp1 = RADEON_BIOS8(tmp0+2) & 0x07;
		    if (tmp1) {
			DDCType	= tmp1;
			switch (DDCType) {
			case DDC_MONID:
			    info->BiosConnector[4].ddc_line = RADEON_GPIO_MONID;
			    break;
			case DDC_DVI:
			    info->BiosConnector[4].ddc_line = RADEON_GPIO_DVI_DDC;
			    break;
			case DDC_VGA:
			    info->BiosConnector[4].ddc_line = RADEON_GPIO_VGA_DDC;
			    break;
			case DDC_CRT2:
			    info->BiosConnector[4].ddc_line = RADEON_GPIO_CRT2_DDC;
			    break;
			case DDC_LCD:
			    info->BiosConnector[4].ddc_line = RADEON_LCD_GPIO_MASK;
			    break;
			case DDC_GPIO:
			    info->BiosConnector[4].ddc_line = RADEON_MDGPIO_EN_REG;
			    break;
			default:
			    xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "Unknown DDC Type: %d\n", DDCType);
			    break;
			}
			xf86DrvMsg(pScrn->scrnIndex, X_WARNING, "LCD DDC Info Table found!\n");
		    }
		}
	    } else {
		info->BiosConnector[4].ddc_line = 0;
	    }
	}
    }

    /* check TV table */
    if (info->InternalTVOut) {
	offset = RADEON_BIOS16(info->ROMHeaderStart + 0x32);
	if (offset) {
	    if (RADEON_BIOS8(offset + 6) == 'T') {
		info->BiosConnector[5].valid = TRUE;
		/* assume s-video for now */
		info->BiosConnector[5].ConnectorType = CONNECTOR_STV;
		info->BiosConnector[5].DACType = DAC_TVDAC;
		info->BiosConnector[5].TMDSType = TMDS_NONE;
		info->BiosConnector[5].ddc_line = 0;
	    }
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Bios Connector table: \n");
    for (i = 0; i < RADEON_MAX_BIOS_CONNECTOR; i++) {
	if (info->BiosConnector[i].valid) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Port%d: DDCType-0x%x, DACType-%d, TMDSType-%d, ConnectorType-%d\n",
		       i, info->BiosConnector[i].ddc_line, info->BiosConnector[i].DACType,
		       info->BiosConnector[i].TMDSType, info->BiosConnector[i].ConnectorType);
	}
    }

    return TRUE;
}

Bool RADEONGetConnectorInfoFromBIOS (ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);

    if(!info->VBIOS) return FALSE;

    if (info->IsAtomBios)
	return RADEONGetATOMConnectorInfoFromBIOS(pScrn);
    else
	return RADEONGetLegacyConnectorInfoFromBIOS(pScrn);
}

Bool RADEONGetTVInfoFromBIOS (xf86OutputPtr output) {
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int offset, refclk, stds;

    if (!info->VBIOS) return FALSE;

    if (info->IsAtomBios) {
	/* no idea where TV table is on ATOM bios */
	return FALSE;
    } else {
	offset = RADEON_BIOS16(info->ROMHeaderStart + 0x32);
	if (offset) {
	    if (RADEON_BIOS8(offset + 6) == 'T') {
		switch (RADEON_BIOS8(offset + 7) & 0xf) {
		case 1:
		    radeon_output->default_tvStd = TV_STD_NTSC;
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Default TV standard: NTSC\n");
		    break;
		case 2:
		    radeon_output->default_tvStd = TV_STD_PAL;
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Default TV standard: PAL\n");
		    break;
		case 3:
		    radeon_output->default_tvStd = TV_STD_PAL_M;
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Default TV standard: PAL-M\n");
		    break;
		case 4:
		    radeon_output->default_tvStd = TV_STD_PAL_60;
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Default TV standard: PAL-60\n");
		    break;
		case 5:
		    radeon_output->default_tvStd = TV_STD_NTSC_J;
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Default TV standard: NTSC-J\n");
		    break;
		case 6:
		    radeon_output->default_tvStd = TV_STD_SCART_PAL;
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Default TV standard: SCART-PAL\n");
		    break;
		default:
		    radeon_output->default_tvStd = TV_STD_NTSC;
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Unknown TV standard; defaulting to NTSC\n");
		    break;
		}
		radeon_output->tvStd = radeon_output->default_tvStd;

		refclk = (RADEON_BIOS8(offset + 9) >> 2) & 0x3;
		if (refclk == 0)
		    radeon_output->TVRefClk = 29.498928713; /* MHz */
		else if (refclk == 1)
		    radeon_output->TVRefClk = 28.636360000;
		else if (refclk == 2)
		    radeon_output->TVRefClk = 14.318180000;
		else if (refclk == 3)
		    radeon_output->TVRefClk = 27.000000000;

		radeon_output->SupportedTVStds = radeon_output->default_tvStd;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "TV standards supported by chip: ");
		stds = RADEON_BIOS8(offset + 10) & 0x1f;
		if (stds & TV_STD_NTSC) {
		    radeon_output->SupportedTVStds |= TV_STD_NTSC;
		    ErrorF("NTSC ");
		}
		if (stds & TV_STD_PAL) {
		    radeon_output->SupportedTVStds |= TV_STD_PAL;
		    ErrorF("PAL ");
		}
		if (stds & TV_STD_PAL_M) {
		    radeon_output->SupportedTVStds |= TV_STD_PAL_M;
		    ErrorF("PAL-M ");
		}
		if (stds & TV_STD_PAL_60) {
		    radeon_output->SupportedTVStds |= TV_STD_PAL_60;
		    ErrorF("PAL-60 ");
		}
		if (stds & TV_STD_NTSC_J) {
		    radeon_output->SupportedTVStds |= TV_STD_NTSC_J;
		    ErrorF("NTSC-J ");
		}
		if (stds & TV_STD_SCART_PAL) {
		    radeon_output->SupportedTVStds |= TV_STD_SCART_PAL;
		    ErrorF("SCART-PAL");
		}
		ErrorF("\n");

		return TRUE;
	    }
	}
    }
    return FALSE;
}

/* Read PLL parameters from BIOS block.  Default to typical values if there
   is no BIOS. */
Bool RADEONGetClockInfoFromBIOS (ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    RADEONPLLPtr pll = &info->pll;
    CARD16 pll_info_block;

    if (!info->VBIOS) {
	return FALSE;
    } else {
	if (info->IsAtomBios) {
	    pll_info_block = RADEON_BIOS16 (info->MasterDataStart + 12);

	    pll->reference_freq = RADEON_BIOS16 (pll_info_block + 82);
	    pll->reference_div = 0; /* Need to derive from existing setting
					or use a new algorithm to calculate
					from min_input and max_input
				     */
	    pll->min_pll_freq = RADEON_BIOS16 (pll_info_block + 78);
	    pll->max_pll_freq = RADEON_BIOS32 (pll_info_block + 32);
	    pll->xclk = RADEON_BIOS16 (pll_info_block + 72);

	    info->sclk = RADEON_BIOS32(pll_info_block + 8) / 100.0;
	    info->mclk = RADEON_BIOS32(pll_info_block + 12) / 100.0;
	    if (info->sclk == 0) info->sclk = 200;
	    if (info->mclk == 0) info->mclk = 200;
		
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ref_freq: %d, min_pll: %u, "
		       "max_pll: %u, xclk: %d, sclk: %f, mclk: %f\n",
		       pll->reference_freq, (unsigned)pll->min_pll_freq,
		       (unsigned)pll->max_pll_freq, pll->xclk, info->sclk,
		       info->mclk);

	} else {
	    pll_info_block = RADEON_BIOS16 (info->ROMHeaderStart + 0x30);

	    pll->reference_freq = RADEON_BIOS16 (pll_info_block + 0x0e);
	    pll->reference_div = RADEON_BIOS16 (pll_info_block + 0x10);
	    pll->min_pll_freq = RADEON_BIOS32 (pll_info_block + 0x12);
	    pll->max_pll_freq = RADEON_BIOS32 (pll_info_block + 0x16);
	    pll->xclk = RADEON_BIOS16 (pll_info_block + 0x08);

	    info->sclk = RADEON_BIOS16(pll_info_block + 8) / 100.0;
	    info->mclk = RADEON_BIOS16(pll_info_block + 10) / 100.0;
	}
    }

    return TRUE;
}

Bool RADEONGetLVDSInfoFromBIOS (xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    unsigned long tmp, i;

    if (!info->VBIOS) return FALSE;

    if (info->IsAtomBios) {
	if((tmp = RADEON_BIOS16 (info->MasterDataStart + 16))) {

	    radeon_output->PanelXRes = RADEON_BIOS16(tmp+6);
	    radeon_output->PanelYRes = RADEON_BIOS16(tmp+10);
	    radeon_output->DotClock   = RADEON_BIOS16(tmp+4)*10;
	    radeon_output->HBlank     = RADEON_BIOS16(tmp+8);
	    radeon_output->HOverPlus  = RADEON_BIOS16(tmp+14);
	    radeon_output->HSyncWidth = RADEON_BIOS16(tmp+16);
	    radeon_output->VBlank     = RADEON_BIOS16(tmp+12);
	    radeon_output->VOverPlus  = RADEON_BIOS16(tmp+18);
	    radeon_output->VSyncWidth = RADEON_BIOS16(tmp+20);
	    radeon_output->PanelPwrDly = RADEON_BIOS16(tmp+40);

	    if (radeon_output->PanelPwrDly > 2000 || radeon_output->PanelPwrDly < 0)
		radeon_output->PanelPwrDly = 2000;

	    radeon_output->Flags = 0;
	} else {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "No LVDS Info Table found in BIOS!\n");
	    return FALSE;
	}
    } else {

	tmp = RADEON_BIOS16(info->ROMHeaderStart + 0x40);

	if (!tmp) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "No Panel Info Table found in BIOS!\n");
	    return FALSE;
	} else {
	    char  stmp[30];
	    int   tmp0;

	    for (i = 0; i < 24; i++)
	    stmp[i] = RADEON_BIOS8(tmp+i+1);
	    stmp[24] = 0;

	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "Panel ID string: %s\n", stmp);

	    radeon_output->PanelXRes = RADEON_BIOS16(tmp+25);
	    radeon_output->PanelYRes = RADEON_BIOS16(tmp+27);
	    xf86DrvMsg(0, X_INFO, "Panel Size from BIOS: %dx%d\n",
		       radeon_output->PanelXRes, radeon_output->PanelYRes);
	
	    radeon_output->PanelPwrDly = RADEON_BIOS16(tmp+44);
	    if (radeon_output->PanelPwrDly > 2000 || radeon_output->PanelPwrDly < 0)
		radeon_output->PanelPwrDly = 2000;

	    /* some panels only work well with certain divider combinations.
	     */
	    info->RefDivider = RADEON_BIOS16(tmp+46);
	    info->PostDivider = RADEON_BIOS8(tmp+48);
	    info->FeedbackDivider = RADEON_BIOS16(tmp+49);
	    if ((info->RefDivider != 0) &&
		(info->FeedbackDivider > 3)) {
		info->UseBiosDividers = TRUE;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "BIOS provided dividers will be used.\n");
	    }

	    /* We don't use a while loop here just in case we have a corrupted BIOS image.
	       The max number of table entries is 23 at present, but may grow in future.
	       To ensure it works with future revisions we loop it to 32.
	    */
	    for (i = 0; i < 32; i++) {
		tmp0 = RADEON_BIOS16(tmp+64+i*2);
		if (tmp0 == 0) break;
		if ((RADEON_BIOS16(tmp0) == radeon_output->PanelXRes) &&
		    (RADEON_BIOS16(tmp0+2) == radeon_output->PanelYRes)) {
		    radeon_output->HBlank     = (RADEON_BIOS16(tmp0+17) -
					RADEON_BIOS16(tmp0+19)) * 8;
		    radeon_output->HOverPlus  = (RADEON_BIOS16(tmp0+21) -
					RADEON_BIOS16(tmp0+19) - 1) * 8;
		    radeon_output->HSyncWidth = RADEON_BIOS8(tmp0+23) * 8;
		    radeon_output->VBlank     = (RADEON_BIOS16(tmp0+24) -
					RADEON_BIOS16(tmp0+26));
		    radeon_output->VOverPlus  = ((RADEON_BIOS16(tmp0+28) & 0x7ff) -
					RADEON_BIOS16(tmp0+26));
		    radeon_output->VSyncWidth = ((RADEON_BIOS16(tmp0+28) & 0xf800) >> 11);
		    radeon_output->DotClock   = RADEON_BIOS16(tmp0+9) * 10;
		    radeon_output->Flags = 0;
		}
	    }
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	       "LVDS Info:\n"
	       "XRes: %d, YRes: %d, DotClock: %d\n"
	       "HBlank: %d, HOverPlus: %d, HSyncWidth: %d\n"
	       "VBlank: %d, VOverPlus: %d, VSyncWidth: %d\n",
	       radeon_output->PanelXRes, radeon_output->PanelYRes, radeon_output->DotClock,
	       radeon_output->HBlank, radeon_output->HOverPlus, radeon_output->HSyncWidth,
	       radeon_output->VBlank, radeon_output->VOverPlus, radeon_output->VSyncWidth);

    return TRUE;
}

Bool RADEONGetHardCodedEDIDFromBIOS (xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    unsigned long tmp;
    char EDID[256];

    if (!info->VBIOS) return FALSE;

    if (info->IsAtomBios) {
	/* Not yet */
	return FALSE;
    } else {
	if (!(tmp = RADEON_BIOS16(info->ROMHeaderStart + 0x4c))) {
	    return FALSE;
	}

	memcpy(EDID, (char*)(info->VBIOS + tmp), 256);

	radeon_output->DotClock = (*(CARD16*)(EDID+54)) * 10;
	radeon_output->PanelXRes = (*(CARD8*)(EDID+56)) + ((*(CARD8*)(EDID+58))>>4)*256;
	radeon_output->HBlank = (*(CARD8*)(EDID+57)) + ((*(CARD8*)(EDID+58)) & 0xf)*256;
	radeon_output->HOverPlus = (*(CARD8*)(EDID+62)) + ((*(CARD8*)(EDID+65)>>6)*256);
	radeon_output->HSyncWidth = (*(CARD8*)(EDID+63)) + (((*(CARD8*)(EDID+65)>>4) & 3)*256);
	radeon_output->PanelYRes = (*(CARD8*)(EDID+59)) + ((*(CARD8*)(EDID+61))>>4)*256;
	radeon_output->VBlank = ((*(CARD8*)(EDID+60)) + ((*(CARD8*)(EDID+61)) & 0xf)*256);
	radeon_output->VOverPlus = (((*(CARD8*)(EDID+64))>>4) + (((*(CARD8*)(EDID+65)>>2) & 3)*16));
	radeon_output->VSyncWidth = (((*(CARD8*)(EDID+64)) & 0xf) + ((*(CARD8*)(EDID+65)) & 3)*256);
	radeon_output->Flags      = V_NHSYNC | V_NVSYNC; /**(CARD8*)(EDID+71);*/
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Hardcoded EDID data will be used for TMDS panel\n");
    }
    return TRUE;
}

Bool RADEONGetTMDSInfoFromBIOS (xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    CARD32 tmp, maxfreq;
    int i, n;

    if (!info->VBIOS) return FALSE;

    if (info->IsAtomBios) {
	if((tmp = RADEON_BIOS16 (info->MasterDataStart + 18))) {

	    maxfreq = RADEON_BIOS16(tmp+4);
	    
	    for (i=0; i<4; i++) {
		radeon_output->tmds_pll[i].freq = RADEON_BIOS16(tmp+i*6+6);
		/* This assumes each field in TMDS_PLL has 6 bit as in R300/R420 */
		radeon_output->tmds_pll[i].value = ((RADEON_BIOS8(tmp+i*6+8) & 0x3f) |
					   ((RADEON_BIOS8(tmp+i*6+10) & 0x3f)<<6) |
					   ((RADEON_BIOS8(tmp+i*6+9) & 0xf)<<12) |
					   ((RADEON_BIOS8(tmp+i*6+11) & 0xf)<<16));
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, 
			   "TMDS PLL from BIOS: %u %x\n", 
			   (unsigned)radeon_output->tmds_pll[i].freq,
			   (unsigned)radeon_output->tmds_pll[i].value);
		       
		if (maxfreq == radeon_output->tmds_pll[i].freq) {
		    radeon_output->tmds_pll[i].freq = 0xffffffff;
		    break;
		}
	    }
	    return TRUE;
	}
    } else {

	tmp = RADEON_BIOS16(info->ROMHeaderStart + 0x34);
	if (tmp) {
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "DFP table revision: %d\n", RADEON_BIOS8(tmp));
	    if (RADEON_BIOS8(tmp) == 3) {
		n = RADEON_BIOS8(tmp + 5) + 1;
		if (n > 4) n = 4;
		for (i=0; i<n; i++) {
		    radeon_output->tmds_pll[i].value = RADEON_BIOS32(tmp+i*10+0x08);
		    radeon_output->tmds_pll[i].freq = RADEON_BIOS16(tmp+i*10+0x10);
		}
		return TRUE;
	    } else if (RADEON_BIOS8(tmp) == 4) {
	        int stride = 0;
		n = RADEON_BIOS8(tmp + 5) + 1;
		if (n > 4) n = 4;
		for (i=0; i<n; i++) {
		    radeon_output->tmds_pll[i].value = RADEON_BIOS32(tmp+stride+0x08);
		    radeon_output->tmds_pll[i].freq = RADEON_BIOS16(tmp+stride+0x10);
		    if (i == 0) stride += 10;
		    else stride += 6;
		}
		return TRUE;
	    }
	}
    }
    return FALSE;
}

Bool RADEONGetExtTMDSInfoFromBIOS (xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int offset, table_start, max_freq, gpio_reg, flags;

    if (!info->VBIOS) return FALSE;

    if (info->IsAtomBios) {
	return FALSE;
    } else {
	offset = RADEON_BIOS16(info->ROMHeaderStart + 0x58);
	if (offset) {
	     xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"External TMDS Table revision: %d\n",
			RADEON_BIOS8(offset));
	    table_start = offset+4;
	    max_freq = RADEON_BIOS16(table_start);
	    radeon_output->dvo_i2c_slave_addr = RADEON_BIOS8(table_start+2);
	    gpio_reg = RADEON_BIOS8(table_start+3);
	    if (gpio_reg == 1)
		radeon_output->dvo_i2c_reg = RADEON_GPIO_MONID;
	    else if (gpio_reg == 2)
		radeon_output->dvo_i2c_reg = RADEON_GPIO_DVI_DDC;
	    else if (gpio_reg == 3)
		radeon_output->dvo_i2c_reg = RADEON_GPIO_VGA_DDC;
	    else if (gpio_reg == 4)
		radeon_output->dvo_i2c_reg = RADEON_GPIO_CRT2_DDC;
	    else if (gpio_reg == 5)
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "unsupported MM gpio_reg\n");
		/*radeon_output->i2c_reg = RADEON_GPIO_MM;*/
	    else {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Unknown gpio reg: %d\n", gpio_reg);
		return FALSE;
	    }
	    flags = RADEON_BIOS8(table_start+5);
	    radeon_output->dvo_duallink = flags & 0x01;
	    if (radeon_output->dvo_duallink) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			   "Duallink TMDS detected\n");
	    }
	    return TRUE;
	}
    }

    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
	       "No External TMDS Table found\n");

    return FALSE;
}

Bool RADEONInitExtTMDSInfoFromBIOS (xf86OutputPtr output)
{
    ScrnInfoPtr pScrn = output->scrn;
    RADEONInfoPtr info = RADEONPTR(pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    RADEONOutputPrivatePtr radeon_output = output->driver_private;
    int offset, index, id;
    CARD32 val, reg, andmask, ormask;

    if (!info->VBIOS) return FALSE;

    if (info->IsAtomBios) {
	return FALSE;
    } else {
	offset = RADEON_BIOS16(info->ROMHeaderStart + 0x58);
	if (offset) {
	    index = offset+10;
	    id = RADEON_BIOS16(index);
	    while (id != 0xffff) {
		index += 2;
		switch(id >> 13) {
		case 0:
		    reg = id & 0x1fff;
		    val = RADEON_BIOS32(index);
		    index += 4;
		    ErrorF("WRITE INDEXED: 0x%x 0x%x\n",
			   (unsigned)reg, (unsigned)val);
		    /*OUTREG(reg, val);*/
		    break;
		case 2:
		    reg = id & 0x1fff;
		    andmask = RADEON_BIOS32(index);
		    index += 4;
		    ormask = RADEON_BIOS32(index);
		    index += 4;
		    val = INREG(reg);
		    val = (val & andmask) | ormask;
		    ErrorF("MASK DIRECT: 0x%x 0x%x 0x%x\n",
			   (unsigned)reg, (unsigned)andmask, (unsigned)ormask);
		    /*OUTREG(reg, val);*/
		    break;
		case 4:
		    val = RADEON_BIOS16(index);
		    index += 2;
		    ErrorF("delay: %u\n", (unsigned)val);
		    usleep(val);
		    break;
		case 5:
		    reg = id & 0x1fff;
		    andmask = RADEON_BIOS32(index);
		    index += 4;
		    ormask = RADEON_BIOS32(index);
		    index += 4;
		    ErrorF("MASK PLL: 0x%x 0x%x 0x%x\n",
			   (unsigned)reg, (unsigned)andmask, (unsigned)ormask);
		    /*val = INPLL(pScrn, reg);
		    val = (val & andmask) | ormask;
		    OUTPLL(pScrn, reg, val);*/
		    break;
		case 6:
		    reg = id & 0x1fff;
		    val = RADEON_BIOS8(index);
		    index += 1;
		    ErrorF("i2c write: 0x%x, 0x%x\n", (unsigned)reg,
			   (unsigned)val);
		    RADEONDVOWriteByte(radeon_output->DVOChip, reg, val);
		    break;
		default:
		    ErrorF("unknown id %d\n", id>>13);
		    return FALSE;
		};
		id = RADEON_BIOS16(index);
	    }
	    return TRUE;
	}
    }

    return FALSE;
}

/* support for init from bios tables
 *
 * Based heavily on the netbsd radeonfb driver
 * Written by Garrett D'Amore
 * Copyright (c) 2006 Itronix Inc.
 *
 */

/* bios table defines */

#define RADEON_TABLE_ENTRY_FLAG_MASK    0xe000
#define RADEON_TABLE_ENTRY_INDEX_MASK   0x1fff
#define RADEON_TABLE_ENTRY_COMMAND_MASK 0x00ff

#define RADEON_TABLE_FLAG_WRITE_INDEXED 0x0000
#define RADEON_TABLE_FLAG_WRITE_DIRECT  0x2000
#define RADEON_TABLE_FLAG_MASK_INDEXED  0x4000
#define RADEON_TABLE_FLAG_MASK_DIRECT   0x6000
#define RADEON_TABLE_FLAG_DELAY         0x8000
#define RADEON_TABLE_FLAG_SCOMMAND      0xa000

#define RADEON_TABLE_SCOMMAND_WAIT_MC_BUSY_MASK       0x03
#define RADEON_TABLE_SCOMMAND_WAIT_MEM_PWRUP_COMPLETE 0x08

#define RADEON_PLL_FLAG_MASK      0xc0
#define RADEON_PLL_INDEX_MASK     0x3f

#define RADEON_PLL_FLAG_WRITE     0x00
#define RADEON_PLL_FLAG_MASK_BYTE 0x40
#define RADEON_PLL_FLAG_WAIT      0x80

#define RADEON_PLL_WAIT_150MKS                    1
#define RADEON_PLL_WAIT_5MS                       2
#define RADEON_PLL_WAIT_MC_BUSY_MASK              3
#define RADEON_PLL_WAIT_DLL_READY_MASK            4
#define RADEON_PLL_WAIT_CHK_SET_CLK_PWRMGT_CNTL24 5

static CARD16
RADEONValidateBIOSOffset(ScrnInfoPtr pScrn, CARD16 offset)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    CARD8 revision = RADEON_BIOS8(offset - 1);

    if (revision > 0x10) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "Bad revision %d for BIOS table\n", revision);
        return 0;
    }

    if (offset < 0x60) {
        xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
                   "Bad offset 0x%x for BIOS Table\n", offset);
        return 0;
    }

    return offset;
}

Bool
RADEONGetBIOSInitTableOffsets(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    CARD8 val;

    if (!info->VBIOS) {
	return FALSE;
    } else {
	if (info->IsAtomBios) {
	    return FALSE;
	} else {
	    info->BiosTable.revision = RADEON_BIOS8(info->ROMHeaderStart + 4);
	    info->BiosTable.rr1_offset = RADEON_BIOS16(info->ROMHeaderStart + 0x0c);
	    if (info->BiosTable.rr1_offset) {
		info->BiosTable.rr1_offset =
		    RADEONValidateBIOSOffset(pScrn, info->BiosTable.rr1_offset);
	    }
	    if (info->BiosTable.revision > 0x09)
		return TRUE;
	    info->BiosTable.rr2_offset = RADEON_BIOS16(info->ROMHeaderStart + 0x4e);
	    if (info->BiosTable.rr2_offset) {
		info->BiosTable.rr2_offset =
		    RADEONValidateBIOSOffset(pScrn, info->BiosTable.rr2_offset);
	    }
	    info->BiosTable.dyn_clk_offset = RADEON_BIOS16(info->ROMHeaderStart + 0x52);
	    if (info->BiosTable.dyn_clk_offset) {
		info->BiosTable.dyn_clk_offset =
		    RADEONValidateBIOSOffset(pScrn, info->BiosTable.dyn_clk_offset);
	    }
	    info->BiosTable.pll_offset = RADEON_BIOS16(info->ROMHeaderStart + 0x46);
	    if (info->BiosTable.pll_offset) {
		info->BiosTable.pll_offset =
		    RADEONValidateBIOSOffset(pScrn, info->BiosTable.pll_offset);
	    }
	    info->BiosTable.mem_config_offset = RADEON_BIOS16(info->ROMHeaderStart + 0x48);
	    if (info->BiosTable.mem_config_offset) {
		info->BiosTable.mem_config_offset =
		    RADEONValidateBIOSOffset(pScrn, info->BiosTable.mem_config_offset);
	    }
	    if (info->BiosTable.mem_config_offset) {
		info->BiosTable.mem_reset_offset = info->BiosTable.mem_config_offset;
		if (info->BiosTable.mem_reset_offset) {
		    while (RADEON_BIOS8(info->BiosTable.mem_reset_offset))
			info->BiosTable.mem_reset_offset++;
		    info->BiosTable.mem_reset_offset++;
		    info->BiosTable.mem_reset_offset += 2;
		}
	    }
	    if (info->BiosTable.mem_config_offset) {
		info->BiosTable.short_mem_offset = info->BiosTable.mem_config_offset;
		if ((info->BiosTable.short_mem_offset != 0) &&
		    (RADEON_BIOS8(info->BiosTable.short_mem_offset - 2) <= 64))
		    info->BiosTable.short_mem_offset +=
			RADEON_BIOS8(info->BiosTable.short_mem_offset - 3);
	    }
	    if (info->BiosTable.rr2_offset) {
		info->BiosTable.rr3_offset = info->BiosTable.rr2_offset;
		if (info->BiosTable.rr3_offset) {
		    while ((val = RADEON_BIOS8(info->BiosTable.rr3_offset + 1)) != 0) {
			if (val & 0x40)
			    info->BiosTable.rr3_offset += 10;
			else if (val & 0x80)
			    info->BiosTable.rr3_offset += 4;
			else
			    info->BiosTable.rr3_offset += 6;
		    }
		    info->BiosTable.rr3_offset += 2;
		}
	    }

	    if (info->BiosTable.rr3_offset) {
		info->BiosTable.rr4_offset = info->BiosTable.rr3_offset;
		if (info->BiosTable.rr4_offset) {
		    while ((val = RADEON_BIOS8(info->BiosTable.rr4_offset + 1)) != 0) {
			if (val & 0x40)
			    info->BiosTable.rr4_offset += 10;
			else if (val & 0x80)
			    info->BiosTable.rr4_offset += 4;
			else
			    info->BiosTable.rr4_offset += 6;
		    }
		    info->BiosTable.rr4_offset += 2;
		}
	    }

	    if (info->BiosTable.rr3_offset + 1 == info->BiosTable.pll_offset) {
		info->BiosTable.rr3_offset = 0;
		info->BiosTable.rr4_offset = 0;
	    }

	    return TRUE;

	}
    }
}

static void
RADEONRestoreBIOSRegBlock(ScrnInfoPtr pScrn, CARD16 table_offset)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD16 offset = table_offset;
    CARD16 value, flag, index, count;
    CARD32 andmask, ormask, val, channel_complete_mask;
    CARD8  command;

    if (offset == 0)
	return;

    while ((value = RADEON_BIOS16(offset)) != 0) {
	flag = value & RADEON_TABLE_ENTRY_FLAG_MASK;
	index = value & RADEON_TABLE_ENTRY_INDEX_MASK;
	command = value & RADEON_TABLE_ENTRY_COMMAND_MASK;

	offset += 2;

	switch (flag) {
	case RADEON_TABLE_FLAG_WRITE_INDEXED:
	    val = RADEON_BIOS32(offset);
	    ErrorF("WRITE INDEXED: 0x%x 0x%x\n",
		   index, (unsigned)val);
	    OUTREG(RADEON_MM_INDEX, index);
	    OUTREG(RADEON_MM_DATA, val);
	    offset += 4;
	    break;

	case RADEON_TABLE_FLAG_WRITE_DIRECT:
	    val = RADEON_BIOS32(offset);
	    ErrorF("WRITE DIRECT: 0x%x 0x%x\n", index, (unsigned)val);
	    OUTREG(index, val);
	    offset += 4;
	    break;

	case RADEON_TABLE_FLAG_MASK_INDEXED:
	    andmask = RADEON_BIOS32(offset);
	    offset += 4;
	    ormask = RADEON_BIOS32(offset);
	    offset += 4;
	    ErrorF("MASK INDEXED: 0x%x 0x%x 0x%x\n",
		   index, (unsigned)andmask, (unsigned)ormask);
	    OUTREG(RADEON_MM_INDEX, index);
	    val = INREG(RADEON_MM_DATA);
	    val = (val & andmask) | ormask;
	    OUTREG(RADEON_MM_DATA, val);
	    break;

	case RADEON_TABLE_FLAG_MASK_DIRECT:
	    andmask = RADEON_BIOS32(offset);
	    offset += 4;
	    ormask = RADEON_BIOS32(offset);
	    offset += 4;
	    ErrorF("MASK DIRECT: 0x%x 0x%x 0x%x\n",
		   index, (unsigned)andmask, (unsigned)ormask);
	    val = INREG(index);
	    val = (val & andmask) | ormask;
	    OUTREG(index, val);
	    break;

	case RADEON_TABLE_FLAG_DELAY:
	    count = RADEON_BIOS16(offset);
	    ErrorF("delay: %d\n", count);
	    usleep(count);
	    offset += 2;
	    break;

	case RADEON_TABLE_FLAG_SCOMMAND:
	    ErrorF("SCOMMAND 0x%x\n", command); 
	    switch (command) {
	    case RADEON_TABLE_SCOMMAND_WAIT_MC_BUSY_MASK:
		count = RADEON_BIOS16(offset);
		ErrorF("SCOMMAND_WAIT_MC_BUSY_MASK %d\n", count);
		while (count--) {
		    if (!(INPLL(pScrn, RADEON_CLK_PWRMGT_CNTL) &
			  RADEON_MC_BUSY))
			break;
		}
		break;

	    case RADEON_TABLE_SCOMMAND_WAIT_MEM_PWRUP_COMPLETE:
		count = RADEON_BIOS16(offset);
		ErrorF("SCOMMAND_WAIT_MEM_PWRUP_COMPLETE %d\n", count);
		/* may need to take into account how many memory channels
		 * each card has
		 */
		if (IS_R300_VARIANT)
		    channel_complete_mask = R300_MEM_PWRUP_COMPLETE;
		else
		    channel_complete_mask = RADEON_MEM_PWRUP_COMPLETE;
		while (count--) {
		    /* XXX: may need indexed access */
		    if ((INREG(RADEON_MEM_STR_CNTL) &
			 channel_complete_mask) ==
		        channel_complete_mask)
			break;
		}
		break;

	    }
	    offset += 2;
	    break;
	}
    }
}

static void
RADEONRestoreBIOSMemBlock(ScrnInfoPtr pScrn, CARD16 table_offset)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    unsigned char *RADEONMMIO = info->MMIO;
    CARD16 offset = table_offset;
    CARD16 count;
    CARD32 ormask, val, channel_complete_mask;
    CARD8  index;

    if (offset == 0)
	return;

    while ((index = RADEON_BIOS8(offset)) != 0xff) {
	offset++;
	if (index == 0x0f) {
	    count = 20000;
	    ErrorF("MEM_WAIT_MEM_PWRUP_COMPLETE %d\n", count);
	    /* may need to take into account how many memory channels
	     * each card has
	     */
	    if (IS_R300_VARIANT)
		channel_complete_mask = R300_MEM_PWRUP_COMPLETE;
	    else
		channel_complete_mask = RADEON_MEM_PWRUP_COMPLETE;
	    while (count--) {
		/* XXX: may need indexed access */
		if ((INREG(RADEON_MEM_STR_CNTL) &
		     channel_complete_mask) ==
		    channel_complete_mask)
		    break;
	    }
	} else {
	    ormask = RADEON_BIOS16(offset);
	    offset += 2;

	    ErrorF("INDEX RADEON_MEM_SDRAM_MODE_REG %x %x\n",
		   RADEON_SDRAM_MODE_MASK, (unsigned)ormask);

	    /* can this use direct access? */
	    OUTREG(RADEON_MM_INDEX, RADEON_MEM_SDRAM_MODE_REG);
	    val = INREG(RADEON_MM_DATA);
	    val = (val & RADEON_SDRAM_MODE_MASK) | ormask;
	    OUTREG(RADEON_MM_DATA, val);

	    ormask = (CARD32)index << 24;

	    ErrorF("INDEX RADEON_MEM_SDRAM_MODE_REG %x %x\n",
		   RADEON_B3MEM_RESET_MASK, (unsigned)ormask);

            /* can this use direct access? */
            OUTREG(RADEON_MM_INDEX, RADEON_MEM_SDRAM_MODE_REG);
            val = INREG(RADEON_MM_DATA);
            val = (val & RADEON_B3MEM_RESET_MASK) | ormask;
            OUTREG(RADEON_MM_DATA, val);
	}
    }
}

static void
RADEONRestoreBIOSPllBlock(ScrnInfoPtr pScrn, CARD16 table_offset)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);
    CARD16 offset = table_offset;
    CARD8  index, shift;
    CARD32 andmask, ormask, val, clk_pwrmgt_cntl;
    CARD16 count;

    if (offset == 0)
	return;

    while ((index = RADEON_BIOS8(offset)) != 0) {
	offset++;

	switch (index & RADEON_PLL_FLAG_MASK) {
	case RADEON_PLL_FLAG_WAIT:
	    switch (index & RADEON_PLL_INDEX_MASK) {
	    case RADEON_PLL_WAIT_150MKS:
		ErrorF("delay: 150 us\n");
		usleep(150);
		break;
	    case RADEON_PLL_WAIT_5MS:
		ErrorF("delay: 5 ms\n");
		usleep(5000);
		break;

	    case RADEON_PLL_WAIT_MC_BUSY_MASK:
		count = 1000;
		ErrorF("PLL_WAIT_MC_BUSY_MASK %d\n", count);
		while (count--) {
		    if (!(INPLL(pScrn, RADEON_CLK_PWRMGT_CNTL) &
			  RADEON_MC_BUSY))
			break;
		}
		break;

	    case RADEON_PLL_WAIT_DLL_READY_MASK:
		count = 1000;
		ErrorF("PLL_WAIT_DLL_READY_MASK %d\n", count);
		while (count--) {
		    if (INPLL(pScrn, RADEON_CLK_PWRMGT_CNTL) &
			RADEON_DLL_READY)
			break;
		}
		break;

	    case RADEON_PLL_WAIT_CHK_SET_CLK_PWRMGT_CNTL24:
		ErrorF("PLL_WAIT_CHK_SET_CLK_PWRMGT_CNTL24\n");
		clk_pwrmgt_cntl = INPLL(pScrn, RADEON_CLK_PWRMGT_CNTL);
		if (clk_pwrmgt_cntl & RADEON_CG_NO1_DEBUG_0) {
		    val = INPLL(pScrn, RADEON_MCLK_CNTL);
		    /* is this right? */
		    val = (val & 0xFFFF0000) | 0x1111; /* seems like we should clear these... */
		    OUTPLL(pScrn, RADEON_MCLK_CNTL, val);
		    usleep(10000);
		    OUTPLL(pScrn, RADEON_CLK_PWRMGT_CNTL,
			   clk_pwrmgt_cntl & ~RADEON_CG_NO1_DEBUG_0);
		    usleep(10000);
		}
		break;
	    }
	    break;
	    
	case RADEON_PLL_FLAG_MASK_BYTE:
	    shift = RADEON_BIOS8(offset) * 8;
	    offset++;

	    andmask =
		(((CARD32)RADEON_BIOS8(offset)) << shift) |
		~((CARD32)0xff << shift);
	    offset++;

	    ormask = ((CARD32)RADEON_BIOS8(offset)) << shift;
	    offset++;

	    ErrorF("PLL_MASK_BYTE 0x%x 0x%x 0x%x 0x%x\n", 
		   index, shift, (unsigned)andmask, (unsigned)ormask);
	    val = INPLL(pScrn, index);
	    val = (val & andmask) | ormask;
	    OUTPLL(pScrn, index, val);
	    break;

	case RADEON_PLL_FLAG_WRITE:
	    val = RADEON_BIOS32(offset);
	    ErrorF("PLL_WRITE 0x%x 0x%x\n", index, (unsigned)val);
	    OUTPLL(pScrn, index, val);
	    offset += 4;
	    break;
	}
    }
}

Bool
RADEONPostCardFromBIOSTables(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);

    if (!info->VBIOS) {
	return FALSE;
    } else {
	if (info->IsAtomBios) {
	    return FALSE;
	} else {
	    if (info->BiosTable.rr1_offset) {
		ErrorF("rr1 restore, 0x%x\n", info->BiosTable.rr1_offset);
		RADEONRestoreBIOSRegBlock(pScrn, info->BiosTable.rr1_offset);
	    }
	    if (info->BiosTable.revision < 0x09) {
		if (info->BiosTable.pll_offset) {
		    ErrorF("pll restore, 0x%x\n", info->BiosTable.pll_offset);
		    RADEONRestoreBIOSPllBlock(pScrn, info->BiosTable.pll_offset);
		}
		if (info->BiosTable.rr2_offset) {
		    ErrorF("rr2 restore, 0x%x\n", info->BiosTable.rr2_offset);
		    RADEONRestoreBIOSRegBlock(pScrn, info->BiosTable.rr2_offset);
		}
		if (info->BiosTable.rr4_offset) {
		    ErrorF("rr4 restore, 0x%x\n", info->BiosTable.rr4_offset);
		    RADEONRestoreBIOSRegBlock(pScrn, info->BiosTable.rr4_offset);
		}
		if (info->BiosTable.mem_reset_offset) {
		    ErrorF("mem reset restore, 0x%x\n", info->BiosTable.mem_reset_offset);
		    RADEONRestoreBIOSMemBlock(pScrn, info->BiosTable.mem_reset_offset);
		}
		if (info->BiosTable.rr3_offset) {
		    ErrorF("rr3 restore, 0x%x\n", info->BiosTable.rr3_offset);
		    RADEONRestoreBIOSRegBlock(pScrn, info->BiosTable.rr3_offset);
		}
		if (info->BiosTable.dyn_clk_offset) {
		    ErrorF("dyn_clk restore, 0x%x\n", info->BiosTable.dyn_clk_offset);
		    RADEONRestoreBIOSPllBlock(pScrn, info->BiosTable.dyn_clk_offset);
		}
	    }
	}
    }
    return TRUE;
}
