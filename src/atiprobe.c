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

#include <string.h>
#include <stdio.h>

#include "ati.h"
#include "atiadjust.h"
#include "atibus.h"
#include "atichip.h"
#include "aticonsole.h"
#include "atifillin.h"
#include "atiident.h"
#include "atimach64io.h"
#include "atimodule.h"
#include "atipreinit.h"
#include "atiprobe.h"
#include "atiscreen.h"
#include "ativalid.h"
#include "ativersion.h"
#include "atividmem.h"
#include "atiwonderio.h"

#include "radeon_probe.h"
#include "radeon_version.h"
#include "r128_probe.h"
#include "r128_version.h"

#ifndef AVOID_CPIO

/*
 * ATIVGAWonderProbe --
 *
 * This function determines if ATI extended VGA registers can be accessed
 * through the I/O port specified by pATI->CPIO_VGAWonder.  If not, the
 * function resets pATI->CPIO_VGAWonder to zero.
 */
static void
ATIVGAWonderProbe
(
    pciVideoPtr pVideo,
    ATIPtr      pATI
)
{
    CARD8 IOValue1, IOValue2, IOValue3, IOValue4, IOValue5, IOValue6;

            if (!pATI->OptionProbeSparse)
            {
                xf86Msg(X_WARNING,
                    ATI_NAME ":  Expected VGA Wonder capability at I/O port"
                    " 0x%04lX will not be probed\n"
                    "set option \"probe_sparse\" to force probing.\n",
                    pATI->CPIO_VGAWonder);

                pATI->CPIO_VGAWonder = 0;
                return;
            }

            if (pVideo && !xf86IsPrimaryPci(pVideo) &&
                (pATI->Chip <= ATI_CHIP_88800GXD))
            {
                /* Set up extended VGA register addressing */
                PutReg(GRAX, 0x50U, GetByte(pATI->CPIO_VGAWonder, 0));
                PutReg(GRAX, 0x51U, GetByte(pATI->CPIO_VGAWonder, 1) | 0x80U);
            }
            /*
             * Register 0xBB is used by the BIOS to keep track of various
             * things (monitor type, etc.).  Except for 18800-x's, register
             * 0xBC must be zero and causes the adapter to enter a test mode
             * when written to with a non-zero value.
             */
            IOValue1 = inb(pATI->CPIO_VGAWonder);
            IOValue2 = ATIGetExtReg(IOValue1);
            IOValue3 = ATIGetExtReg(0xBBU);
            ATIPutExtReg(0xBBU, IOValue3 ^ 0xAAU);
            IOValue4 = ATIGetExtReg(0xBBU);
            ATIPutExtReg(0xBBU, IOValue3 ^ 0x55U);
            IOValue5 = ATIGetExtReg(0xBBU);
            ATIPutExtReg(0xBBU, IOValue3);
            IOValue6 = ATIGetExtReg(0xBCU);
            ATIPutExtReg(IOValue1, IOValue2);

            if ((IOValue4 == (IOValue3 ^ 0xAAU)) &&
                (IOValue5 == (IOValue3 ^ 0x55U)) &&
                (IOValue6 == 0))
            {
                xf86MsgVerb(X_INFO, 3,
                    ATI_NAME ":  VGA Wonder at I/O port 0x%04lX detected.\n",
                    pATI->CPIO_VGAWonder);
            }
            else
            {
                xf86Msg(X_WARNING,
                    ATI_NAME ":  Expected VGA Wonder capability at I/O port"
                    " 0x%04lX was not detected.\n", pATI->CPIO_VGAWonder);
                pATI->CPIO_VGAWonder = 0;
            }
}

#endif /* AVOID_CPIO */

/*
 * ATIMach64Detect --
 *
 * This function determines if a Mach64 is detectable at a particular base
 * address.
 */
static Bool
ATIMach64Detect
(
    ATIPtr            pATI,
    const CARD16      ChipType,
    const ATIChipType Chip
)
{
    CARD32 IOValue, bus_cntl, gen_test_cntl;
    Bool DetectSuccess = FALSE;

    (void)ATIMapApertures(-1, pATI);    /* Ignore errors */

#ifdef AVOID_CPIO

    if (!pATI->pBlock[0])
    {
        ATIUnmapApertures(-1, pATI);
        return FALSE;
    }

#endif /* AVOID_CPIO */

    /* Make sure any Mach64 is not in some weird state */
    bus_cntl = inr(BUS_CNTL);
    if (Chip < ATI_CHIP_264VTB)
        outr(BUS_CNTL,
             (bus_cntl & ~(BUS_HOST_ERR_INT_EN | BUS_FIFO_ERR_INT_EN)) |
             (BUS_HOST_ERR_INT | BUS_FIFO_ERR_INT));
    else if (Chip < ATI_CHIP_264VT4)
        outr(BUS_CNTL, (bus_cntl & ~BUS_HOST_ERR_INT_EN) | BUS_HOST_ERR_INT);

    gen_test_cntl = inr(GEN_TEST_CNTL);
    IOValue = gen_test_cntl &
        (GEN_OVR_OUTPUT_EN | GEN_OVR_POLARITY | GEN_CUR_EN | GEN_BLOCK_WR_EN);
    outr(GEN_TEST_CNTL, IOValue | GEN_GUI_EN);
    outr(GEN_TEST_CNTL, IOValue);
    outr(GEN_TEST_CNTL, IOValue | GEN_GUI_EN);

    /* See if a Mach64 answers */
    IOValue = inr(SCRATCH_REG0);

    /* Test odd bits */
    outr(SCRATCH_REG0, 0x55555555U);
    if (inr(SCRATCH_REG0) == 0x55555555U)
    {
        /* Test even bits */
        outr(SCRATCH_REG0, 0xAAAAAAAAU);
        if (inr(SCRATCH_REG0) == 0xAAAAAAAAU)
        {
            /*
             * *Something* has a R/W 32-bit register at this address.  Try to
             * make sure it's a Mach64.  The following assumes that ATI will
             * not be producing any more adapters that do not register
             * themselves in PCI configuration space.
             */
            ATIMach64ChipID(pATI, ChipType);
            if ((pATI->Chip != ATI_CHIP_Mach64) ||
                (pATI->CPIODecoding == BLOCK_IO))
                DetectSuccess = TRUE;
        }
    }

    /* Restore clobbered register value */
    outr(SCRATCH_REG0, IOValue);

    /* If no Mach64 was detected, return now */
    if (!DetectSuccess)
    {
        outr(GEN_TEST_CNTL, gen_test_cntl);
        outr(BUS_CNTL, bus_cntl);
        ATIUnmapApertures(-1, pATI);
        return FALSE;
    }

    /* Determine legacy BIOS address */
    pATI->BIOSBase = 0x000C0000U +
        (GetBits(inr(SCRATCH_REG1), BIOS_BASE_SEGMENT) << 11);

    ATIUnmapApertures(-1, pATI);
    return TRUE;
}

#ifdef AVOID_CPIO

/*
 * ATIMach64Probe --
 *
 * This function looks for a Mach64 at a particular MMIO address and returns an
 * ATIRec if one is found.
 */
static ATIPtr
ATIMach64Probe
(
    ATIPtr            pATI,
    pciVideoPtr       pVideo,
    const ATIChipType Chip
)
{
    CARD16 ChipType = pVideo->chipType;

        /*
         * Probe through auxiliary MMIO aperture if one exists.  Because such
         * apertures can be enabled/disabled only through PCI, this probes no
         * further.
         */
        if ((pVideo->size[2] >= 12) &&
            (pATI->Block0Base = pVideo->memBase[2]) &&
            (pATI->Block0Base < (CARD32)(-1 << pVideo->size[2])))
        {
            pATI->Block0Base += 0x00000400U;
            goto LastProbe;
        }

        /*
         * Probe through the primary MMIO aperture that exists at the tail end
         * of the linear aperture.  Test for both 8MB and 4MB linear apertures.
         */
        if ((pVideo->size[0] >= 22) && (pATI->Block0Base = pVideo->memBase[0]))
        {
            pATI->Block0Base += 0x007FFC00U;
            if ((pVideo->size[0] >= 23) &&
                ATIMach64Detect(pATI, ChipType, Chip))
                return pATI;

            pATI->Block0Base -= 0x00400000U;
            if (ATIMach64Detect(pATI, ChipType, Chip))
                return pATI;
        }

    /*
     * A last, perhaps desparate, probe attempt.  Note that if this succeeds,
     * there's a VGA in the system and it's likely the PIO version of the
     * driver should be used instead (barring OS issues).
     */
    pATI->Block0Base = 0x000BFC00U;

LastProbe:
    if (ATIMach64Detect(pATI, ChipType, Chip))
        return pATI;

    return NULL;
}

#else /* AVOID_CPIO */

/*
 * ATIMach64Probe --
 *
 * This function looks for a Mach64 at a particular PIO address and returns an
 * ATIRec if one is found.
 */
ATIPtr
ATIMach64Probe
(
    ATIPtr            pATI,
    pciVideoPtr       pVideo,
    const ATIChipType Chip
)
{
    CARD32 IOValue;
    CARD16 ChipType = pVideo->chipType;

        if ((pATI->CPIODecoding == BLOCK_IO) &&
            ((pVideo->size[1] < 8) ||
             (pATI->CPIOBase >= (CARD32)(-1 << pVideo->size[1]))))
            return NULL;

    if (!ATIMach64Detect(pATI, ChipType, Chip))
    {
        return NULL;
    }

    /*
     * Determine VGA capability.  VGA can always be enabled on integrated
     * controllers.  For the GX/CX, it's a board strap.
     */
    if (pATI->Chip >= ATI_CHIP_264CT)
    {
        pATI->VGAAdapter = TRUE;
    }
    else
    {
        IOValue = inr(CONFIG_STATUS64_0);
        pATI->BusType = GetBits(IOValue, CFG_BUS_TYPE);
        IOValue &= (CFG_VGA_EN | CFG_CHIP_EN);
        if (pATI->Chip == ATI_CHIP_88800CX)
            IOValue |= CFG_VGA_EN;
        if (IOValue == (CFG_VGA_EN | CFG_CHIP_EN))
        {
            pATI->VGAAdapter = TRUE;
            pATI->CPIO_VGAWonder = 0x01CEU;
        }
    }

    return pATI;
}

static void
ATIAssignVGA
(
    pciVideoPtr pVideo,
    ATIPtr      pATI
)
{
    if (pATI->CPIO_VGAWonder)
    {
        ATIVGAWonderProbe(pVideo, pATI);
        if (!pATI->CPIO_VGAWonder)
        {
            /*
             * Some adapters are reputed to append ATI extended VGA registers
             * to the VGA Graphics controller registers.  In particular, 0x01CE
             * cannot, in general, be used in a PCI environment due to routing
             * of I/O through the bus tree.
             */
            pATI->CPIO_VGAWonder = GRAX;
            ATIVGAWonderProbe(pVideo, pATI);
        }
    }
}

/*
 * ATIFindVGA --
 *
 * This function determines if a VGA associated with an ATI PCI adapter is
 * shareable.
 */
static void
ATIFindVGA
(
    pciVideoPtr pVideo,
    ATIPtr      pATI
)
{
        /*
         * An ATI PCI adapter has been detected at this point, and its VGA, if
         * any, is shareable.  Ensure the VGA isn't in sleep mode.
         */
        outb(GENENA, 0x16U);
        outb(GENVS, 0x01U);
        outb(GENENA, 0x0EU);

    ATIAssignVGA(pVideo, pATI);
}

#endif /* AVOID_CPIO */

/*
 * ATIMach64ProbeIO --
 *
 * This function determines the IO method and IO base of the ATI PCI adapter.
 */
Bool
ATIMach64ProbeIO
(
    pciVideoPtr pVideo,
    ATIPtr      pATI
)
{
    Bool ProbeSuccess = FALSE;

#ifndef AVOID_CPIO

    /* Next, look for sparse I/O Mach64's */
    if (!pVideo->size[1])
    {
        static const IOADDRESS Mach64SparseIOBases[] = {
            0x02ECU,
            0x01CCU,
            0x01C8U
        };
        pciConfigPtr pPCI = pVideo->thisCard;
        CARD32       PciReg;
        CARD32       j;

        if (pPCI == NULL)
            goto SkipSparse;

        PciReg = pciReadLong(pPCI->tag, PCI_REG_USERCONFIG);
        j = PciReg & 0x03U;

        if (j == 0x03U)
        {
            xf86Msg(X_WARNING, ATI_NAME ": "
                "PCI Mach64 in slot %d:%d:%d cannot be enabled\n"
                "because it has neither a block, nor a sparse, I/O base.\n",
                pVideo->bus, pVideo->device, pVideo->func);

            goto SkipSparse;
        }

        /* FIXME:
         * Should not probe at sparse I/O bases which have been registered to
         * other PCI devices. The old ATIProbe() would scan the PCI space and
         * build a list of registered I/O ports. If there was a conflict
         * between a mach64 sparse I/O base and a registered I/0 port, probing
         * that port was not allowed...
         *
         * We just add an option and let the user decide, this will not work
         * with "X -configure" though...
         */
        if (!pATI->OptionProbeSparse)
        {
            xf86Msg(X_WARNING, ATI_NAME ": "
                "PCI Mach64 in slot %d:%d:%d will not be probed\n"
                "set option \"probe_sparse\" to force sparse I/O probing.\n",
                pVideo->bus, pVideo->device, pVideo->func);

            goto SkipSparse;
        }

        /* Possibly fix block I/O indicator */
        if (PciReg & 0x00000004U)
            pciWriteLong(pPCI->tag, PCI_REG_USERCONFIG, PciReg & ~0x00000004U);

        pATI->CPIOBase = Mach64SparseIOBases[j];
        pATI->CPIODecoding = SPARSE_IO;
        pATI->PCIInfo = pVideo;

        if (!ATIMach64Probe(pATI, pVideo, pATI->Chip))
        {
            xf86Msg(X_WARNING, ATI_NAME ": "
                "PCI Mach64 in slot %d:%d:%d could not be detected!\n",
                pVideo->bus, pVideo->device, pVideo->func);
        }
        else
        {
            ProbeSuccess = TRUE;
            xf86Msg(X_INFO, ATI_NAME ": "
                "Shared PCI Mach64 in slot %d:%d:%d with sparse PIO base"
                " 0x%04lX detected.\n",
                pVideo->bus, pVideo->device, pVideo->func,
                Mach64SparseIOBases[j]);

            if (pATI->VGAAdapter)
                ATIFindVGA(pVideo, pATI);
        }
    }

SkipSparse:

#else /* AVOID_CPIO */

    if (!pVideo->size[1])
    {
        /* The adapter's CPIO base is of little concern here */
        pATI->CPIOBase = 0;
        pATI->CPIODecoding = SPARSE_IO;
        pATI->PCIInfo = pVideo;

        if (ATIMach64Probe(pATI, pVideo, pATI->Chip))
        {
            ProbeSuccess = TRUE;
            xf86Msg(X_INFO, ATI_NAME ": "
                "Shared PCI Mach64 in slot %d:%d:%d with Block 0 base"
                " 0x%08lX detected.\n",
                pVideo->bus, pVideo->device, pVideo->func,
                pATI->Block0Base);
        }
        else
        {
            xf86Msg(X_WARNING, ATI_NAME ": "
                "PCI Mach64 in slot %d:%d:%d could not be detected!\n",
                pVideo->bus, pVideo->device, pVideo->func);
        }
    }

#endif /* AVOID_CPIO */

    /* Lastly, look for block I/O devices */
    if (pVideo->size[1])
    {
        pATI->CPIOBase = pVideo->ioBase[1];
        pATI->CPIODecoding = BLOCK_IO;
        pATI->PCIInfo = pVideo;

        if (ATIMach64Probe(pATI, pVideo, pATI->Chip))
        {
            ProbeSuccess = TRUE;
            xf86Msg(X_INFO, ATI_NAME ": "
                "Shared PCI/AGP Mach64 in slot %d:%d:%d detected.\n",
                pVideo->bus, pVideo->device, pVideo->func);

#ifndef AVOID_CPIO

            if (pATI->VGAAdapter)
                ATIFindVGA(pVideo, pATI);

#endif /* AVOID_CPIO */

        }
        else
        {
            xf86Msg(X_WARNING, ATI_NAME ": "
                "PCI/AGP Mach64 in slot %d:%d:%d could not be detected!\n",
                pVideo->bus, pVideo->device, pVideo->func);
        }
    }

    return ProbeSuccess;
}

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

/*
 * Mach64Probe --
 *
 * This function is called once, at the start of the first server generation to
 * do a minimal probe for supported hardware.
 */
static Bool
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

#ifdef XFree86LOADER

            if (!xf86LoadSubModule(pScrn, "atimisc"))
            {
                xf86Msg(X_ERROR,
                    ATI_NAME ":  Failed to load \"atimisc\" module.\n");
                xf86DeleteScreen(pScrn->scrnIndex, 0);
                continue;
            }

            xf86LoaderReqSymLists(ATISymbols, NULL);

#endif

            ATIFillInScreenInfo(pScrn);

            pScrn->Probe = Mach64Probe;

            ProbeSuccess = TRUE;
        }
    }

    return ProbeSuccess;
}

/*
 * ATIProbe --
 *
 * This function is called once, at the start of the first server generation to
 * do a minimal probe for supported hardware.
 */
Bool
ATIProbe
(
    DriverPtr pDriver,
    int       flags
)
{
    pciVideoPtr pVideo, *xf86PciVideoInfo = xf86GetPciVideoInfo();
    Bool        ProbeSuccess = FALSE;
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

    /* Call Mach64 driver probe */
    if (DoMach64 && Mach64Probe(pDriver, flags))
        ProbeSuccess = TRUE;

    /* Call Rage 128 driver probe */
    if (DoRage128 && R128Probe(pDriver, flags))
        ProbeSuccess = TRUE;

    /* Call Radeon driver probe */
    if (DoRadeon && RADEONProbe(pDriver, flags))
        ProbeSuccess = TRUE;

    return ProbeSuccess;
}
