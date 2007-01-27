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

/*
 * NOTES:
 *
 * - The driver private structures (ATIRec's) are allocated here, rather than
 *   in ATIPreInit().  This allows ATIProbe() to pass information to later
 *   stages.
 * - A minor point, perhaps, is that XF86Config Chipset names denote functional
 *   levels, rather than specific graphics controller chips.
 * - ATIProbe() does not call xf86MatchPciInstances(), because ATIProbe()
 *   should be able to match a mix of PCI and non-PCI devices to XF86Config
 *   Device sections.  Also, PCI configuration space for Mach32's is to be
 *   largely ignored.
 */

/* Used as a temporary buffer */
#define Identifier ((char *)(pATI->MMIOCache))

/*
 * An internal structure definition to facilitate the matching of detected
 * adapters to XF86Config Device sections.
 */
typedef struct _ATIGDev
{
    GDevPtr pGDev;
    int     iATIPtr;
    CARD8   Chipset;
} ATIGDev, *ATIGDevPtr;

#ifndef AVOID_CPIO

/*
 * Definitions for I/O conflict avoidance.
 */
#define LongPort(_Port) GetBits(_Port, PCIGETIO(SPARSE_IO_BASE))
#define Allowed        (1 << 3)
#define DoProbe        (1 << 4)
typedef struct
{
    IOADDRESS Base;
    CARD8     Size;
    CARD8     Flag;
} PortRec, *PortPtr;

/*
 * ATIScanPCIBases --
 *
 * This function loops though a device's PCI registered bases and accumulates
 * a list of block I/O bases in use in the system.
 */
static void
ATIScanPCIBases
(
    PortPtr      *PCIPorts,
    int          *nPCIPort,
    const CARD32 *pBase,
    const int    *pSize,
    const CARD8  ProbeFlag
)
{
    IOADDRESS Base;
    int       i, j;

    for (i = 6;  --i >= 0;  pBase++, pSize++)
    {
        if (*pBase & PCI_MAP_IO)
        {
            Base = *pBase & ~IO_BYTE_SELECT;
            for (j = 0;  ;  j++)
            {
                if (j >= *nPCIPort)
                {
                    (*nPCIPort)++;
                    *PCIPorts = (PortPtr)xnfrealloc(*PCIPorts,
                        *nPCIPort * SizeOf(PortRec));
                    (*PCIPorts)[j].Base = Base;
                    (*PCIPorts)[j].Size = (CARD8)*pSize;
                    (*PCIPorts)[j].Flag = ProbeFlag;
                    break;
                }

                if (Base == (*PCIPorts)[j].Base)
                    break;
            }

            continue;
        }

        /* Allow for 64-bit addresses */
        if (!PCI_MAP_IS64BITMEM(*pBase))
            continue;

        i--;
        pBase++;
        pSize++;
    }
}

/*
 * ATICheckSparseIOBases --
 *
 * This function checks whether a sparse I/O base can safely be probed.
 */
static CARD8
ATICheckSparseIOBases
(
    pciVideoPtr     pVideo,
    CARD8           *ProbeFlags,
    const IOADDRESS IOBase,
    const int       Count,
    const Bool      Override
)
{
    CARD32 FirstPort, LastPort;

    if (!pVideo || !xf86IsPrimaryPci(pVideo))
    {
        FirstPort = LongPort(IOBase);
        LastPort  = LongPort(IOBase + Count - 1);

        for (;  FirstPort <= LastPort;  FirstPort++)
        {
            CARD8 ProbeFlag = ProbeFlags[FirstPort];

            if (ProbeFlag & DoProbe)
                continue;

            if (!(ProbeFlag & Allowed))
                return ProbeFlag;

            if (Override)
                continue;

            /* User might wish to override this decision */
            xf86Msg(X_WARNING,
                ATI_NAME ":  Sparse I/O base 0x%04lX not probed.\n", IOBase);
            return Allowed;
        }
    }

    return DoProbe;
}

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
    ATIPtr      pATI,
    CARD8       *ProbeFlags
)
{
    CARD8 IOValue1, IOValue2, IOValue3, IOValue4, IOValue5, IOValue6;

    switch (ATICheckSparseIOBases(pVideo, ProbeFlags,
        pATI->CPIO_VGAWonder, 2, TRUE))
    {
        case 0:
            xf86Msg(X_WARNING,
                ATI_NAME ":  Expected VGA Wonder capability could not be"
                " detected at I/O port 0x%04lX because it would conflict with"
                " a non-video PCI/AGP device.\n", pATI->CPIO_VGAWonder);
            pATI->CPIO_VGAWonder = 0;
            break;

        default:                /* Must be DoProbe */
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
            break;
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
    pATI->PCIInfo = NULL;
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
    pciVideoPtr       pVideo,
    const IOADDRESS   IOBase,
    const CARD8       IODecoding,
    const ATIChipType Chip
)
{
    ATIPtr pATI     = (ATIPtr)xnfcalloc(1, SizeOf(ATIRec));
    CARD16 ChipType = 0;

    pATI->CPIOBase = IOBase;
    pATI->CPIODecoding = IODecoding;

    if (pVideo)
    {
        pATI->PCIInfo = pVideo;
        ChipType = pVideo->chipType;

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

    xfree(pATI);
    return NULL;
}

#else /* AVOID_CPIO */

/*
 * ATIMach64Probe --
 *
 * This function looks for a Mach64 at a particular PIO address and returns an
 * ATIRec if one is found.
 */
static ATIPtr
ATIMach64Probe
(
    pciVideoPtr       pVideo,
    const IOADDRESS   IOBase,
    const CARD8       IODecoding,
    const ATIChipType Chip
)
{
    ATIPtr pATI;
    CARD32 IOValue;
    CARD16 ChipType = 0;

    if (!IOBase)
        return NULL;

    if (pVideo)
    {
        if ((IODecoding == BLOCK_IO) &&
            ((pVideo->size[1] < 8) ||
             (IOBase >= (CARD32)(-1 << pVideo->size[1]))))
            return NULL;

        ChipType = pVideo->chipType;
    }

    pATI = (ATIPtr)xnfcalloc(1, SizeOf(ATIRec));
    pATI->CPIOBase = IOBase;
    pATI->CPIODecoding = IODecoding;
    pATI->PCIInfo = pVideo;

    if (!ATIMach64Detect(pATI, ChipType, Chip))
    {
        xfree(pATI);
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
    ATIPtr      pATI,
    CARD8       *ProbeFlags
)
{
    if (pATI->CPIO_VGAWonder)
    {
        ATIVGAWonderProbe(pVideo, pATI, ProbeFlags);
        if (!pATI->CPIO_VGAWonder)
        {
            /*
             * Some adapters are reputed to append ATI extended VGA registers
             * to the VGA Graphics controller registers.  In particular, 0x01CE
             * cannot, in general, be used in a PCI environment due to routing
             * of I/O through the bus tree.
             */
            pATI->CPIO_VGAWonder = GRAX;
            ATIVGAWonderProbe(pVideo, pATI, ProbeFlags);
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
    ATIPtr      pATI,
    CARD8       *ProbeFlags
)
{
        /*
         * An ATI PCI adapter has been detected at this point, and its VGA, if
         * any, is shareable.  Ensure the VGA isn't in sleep mode.
         */
        outb(GENENA, 0x16U);
        outb(GENVS, 0x01U);
        outb(GENENA, 0x0EU);

    ATIAssignVGA(pVideo, pATI, ProbeFlags);
}

#endif /* AVOID_CPIO */

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
    ATIPtr                 pATI, *ATIPtrs = NULL;
    GDevPtr                *GDevs, pGDev;
    pciVideoPtr            pVideo, *xf86PciVideoInfo = xf86GetPciVideoInfo();
    ATIGDev                *ATIGDevs = NULL, *pATIGDev;
    ScrnInfoPtr            pScreenInfo;
    Bool                   ProbeSuccess = FALSE;
    Bool                   DoRage128 = FALSE, DoRadeon = FALSE;
    int                    i, j, k;
    int                    nGDev, nATIGDev = -1, nATIPtr = 0;
    int                    Chipset;
    ATIChipType            Chip;

#ifndef AVOID_CPIO
    pciConfigPtr           pPCI;
    CARD32                 PciReg;
#endif /* AVOID_CPIO */

#ifndef AVOID_CPIO

    pciConfigPtr           *xf86PciInfo = xf86GetPciConfigInfo();
    PortPtr                PCIPorts = NULL;
    int                    nPCIPort = 0;
    static const IOADDRESS Mach64SparseIOBases[] = {0x02ECU, 0x01CCU, 0x01C8U};
    CARD8                  ProbeFlags[LongPort(SPARSE_IO_BASE) + 1];

#endif /* AVOID_CPIO */

#   define                 AddAdapter(_p)                                  \
    do                                                                     \
    {                                                                      \
        nATIPtr++;                                                         \
        ATIPtrs = (ATIPtr *)xnfrealloc(ATIPtrs, SizeOf(ATIPtr) * nATIPtr); \
        ATIPtrs[nATIPtr - 1] = (_p);                                       \
        (_p)->iEntity = -2;                                                \
    } while (0)

    if (!(flags & PROBE_DETECT))
    {
        /*
         * Get a list of XF86Config device sections whose "Driver" is either
         * not specified, or specified as this driver.  From this list,
         * eliminate those device sections that specify a "Chipset" or a
         * "ChipID" not recognised by the driver.  Those device sections that
         * specify a "ChipRev" without a "ChipID" are also weeded out.
         */
        nATIGDev = 0;
        if ((nGDev = xf86MatchDevice(ATI_NAME, &GDevs)) > 0)
        {
            ATIGDevs = (ATIGDevPtr)xnfcalloc(nGDev, SizeOf(ATIGDev));

            for (i = 0, pATIGDev = ATIGDevs;  i < nGDev;  i++)
            {
                pGDev = GDevs[i];
                Chipset = ATIIdentProbe(pGDev->chipset);
                if (Chipset == -1)
                    continue;

                if ((pGDev->chipID > (int)((CARD16)(-1))) ||
                    (pGDev->chipRev > (int)((CARD8)(-1))))
                    continue;

                if (pGDev->chipID >= 0)
                {
                    if (ATIChipID(pGDev->chipID, 0) == ATI_CHIP_Mach64)
                        continue;
                }
                else
                {
                    if (pGDev->chipRev >= 0)
                        continue;
                }

                pATIGDev->pGDev = pGDev;
                pATIGDev->Chipset = Chipset;
                nATIGDev++;
                pATIGDev++;

                xf86MsgVerb(X_INFO, 3,
                    ATI_NAME ":  Candidate \"Device\" section \"%s\".\n",
                    pGDev->identifier);
            }

            xfree(GDevs);

            if (!nATIGDev)
            {
                xfree(ATIGDevs);
                ATIGDevs = NULL;
            }
        }

        if (xf86MatchDevice(R128_NAME, NULL) > 0)
            DoRage128 = TRUE;
        if (xf86MatchDevice(RADEON_NAME, NULL) > 0)
            DoRadeon = TRUE;
    }

#ifndef AVOID_CPIO

    /*
     * Collect hardware information.  This must be done with care to avoid
     * lockups due to overlapping I/O port assignments.
     *
     * First, scan PCI configuration space for registered I/O ports (which will
     * be block I/O bases).  Each such port is used to generate a list of
     * sparse I/O bases it precludes.  This list is then used to decide whether
     * or not certain sparse I/O probes are done.  Unfortunately, this assumes
     * that any registered I/O base actually reserves upto the full 256 ports
     * allowed by the PCI specification.  This assumption holds true for PCI
     * Mach64, but probably doesn't for other device types.  For some things,
     * such as video devices, the number of ports a base represents is
     * determined by the server's PCI probe, but, for other devices, this
     * cannot be done by a user-level process without jeopardizing system
     * integrity.  This information should ideally be retrieved from the OS's
     * own PCI probe (if any), but there's currently no portable way of doing
     * so.  The following allows sparse I/O probes to be forced in certain
     * circumstances when an appropriate chipset specification is used in any
     * XF86Config Device section.
     *
     * Note that this is not bullet-proof.  Lockups can still occur, but they
     * will usually be due to devices that are misconfigured to respond to the
     * same I/O ports as 8514/A's or ATI sparse I/O devices without registering
     * them in PCI configuration space.
     */
    if (nATIGDev)
    {
        if (xf86PciVideoInfo)
        {
            for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
            {
                if ((pVideo->vendor == PCI_VENDOR_ATI) ||
                    !(pPCI = pVideo->thisCard))
                    continue;

                ATIScanPCIBases(&PCIPorts, &nPCIPort,
                    &pPCI->pci_base0, pVideo->size,
                    (pciReadLong(pPCI->tag, PCI_CMD_STAT_REG) &
                     PCI_CMD_IO_ENABLE) ? 0 : Allowed);
            }
        }

        /* Check non-video PCI devices for I/O bases */
        if (xf86PciInfo)
        {
            for (i = 0;  (pPCI = xf86PciInfo[i++]);  )
            {
                if ((pPCI->pci_vendor == PCI_VENDOR_ATI) ||
                    (pPCI->pci_base_class == PCI_CLASS_BRIDGE) ||
                    (pPCI->pci_header_type &
                      ~GetByte(PCI_HEADER_MULTIFUNCTION, 2)))
                    continue;

                ATIScanPCIBases(&PCIPorts, &nPCIPort,
                    &pPCI->pci_base0, pPCI->basesize,
                    (pciReadLong(pPCI->tag, PCI_CMD_STAT_REG) &
                     PCI_CMD_IO_ENABLE) ? 0 : Allowed);
            }
        }

        /* Generate ProbeFlags array from list of registered PCI I/O bases */
        (void)memset(ProbeFlags, Allowed | DoProbe, SizeOf(ProbeFlags));
        for (i = 0;  i < nPCIPort;  i++)
        {
            CARD32 Base = PCIPorts[i].Base;
            CARD16 Count = (1 << PCIPorts[i].Size) - 1;
            CARD8  ProbeFlag = PCIPorts[i].Flag;

            /*
             * The following reduction of Count is based on the assumption that
             * PCI-registered I/O port ranges do not overlap.
             */
            for (j = 0;  j < nPCIPort;  j++)
            {
                CARD32 Base2 = PCIPorts[j].Base;

                if (Base < Base2)
                    while ((Base + Count) >= Base2)
                        Count >>= 1;
            }

            Base = LongPort(Base);
            Count = LongPort((Count | IO_BYTE_SELECT) + 1);
            while (Count--)
                ProbeFlags[Base++] &= ProbeFlag;
        }

        xfree(PCIPorts);
    }

#endif /* AVOID_CPIO */

    if (xf86PciVideoInfo)
    {
        if (nATIGDev)
        {

#ifndef AVOID_CPIO

            /* Next, look for sparse I/O Mach64's */
            for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
            {
            /* For some Radeons, pvideo->size[1] is not there but the card 
             * still works properly. Thus adding check that if the chip was 
             * correctly identified as being Rage128 or a Radeon, continue 
             * also in that case. See fd.o bug 6796. */
                Chip = ATIChipID(pVideo->chipType, pVideo->chipRev);
                if ((pVideo->vendor != PCI_VENDOR_ATI) ||
                    (pVideo->chipType == PCI_CHIP_MACH32) ||
                    (Chip > ATI_CHIP_Mach64) || 
                    pVideo->size[1])
                    continue;

                pPCI = pVideo->thisCard;
		if (pPCI == NULL)
		    continue;
		
                PciReg = pciReadLong(pPCI->tag, PCI_REG_USERCONFIG);
                j = PciReg & 0x03U;
                if (j == 0x03U)
                {
                    xf86Msg(X_WARNING,
                        ATI_NAME ":  PCI Mach64 in slot %d:%d:%d cannot be"
                        " enabled\n because it has neither a block, nor a"
                        " sparse, I/O base.\n",
                        pVideo->bus, pVideo->device, pVideo->func);
                }
                else switch(ATICheckSparseIOBases(pVideo, ProbeFlags,
                    Mach64SparseIOBases[j], 4, TRUE))
                {
                    case 0:
                        xf86Msg(X_WARNING,
                            ATI_NAME ":  PCI Mach64 in slot %d:%d:%d will not"
                            " be enabled\n because it conflicts with another"
                            " non-video PCI device.\n",
                            pVideo->bus, pVideo->device, pVideo->func);
                        break;

                    default:        /* Must be DoProbe */
                        if (!xf86CheckPciSlot(pVideo->bus,
                                              pVideo->device,
                                              pVideo->func))
                            continue;

                        /* Possibly fix block I/O indicator */
                        if (PciReg & 0x00000004U)
                            pciWriteLong(pPCI->tag, PCI_REG_USERCONFIG,
                                PciReg & ~0x00000004U);

                        xf86SetPciVideo(pVideo, MEM_IO);

                        Chip = ATIChipID(pVideo->chipType, pVideo->chipRev);
                        pATI = ATIMach64Probe(pVideo, Mach64SparseIOBases[j],
                            SPARSE_IO, Chip);
                        if (!pATI)
                        {
                            xf86Msg(X_WARNING,
                                ATI_NAME ":  PCI Mach64 in slot %d:%d:%d could"
                                " not be detected!\n",
                                pVideo->bus, pVideo->device, pVideo->func);
                        }
                        else
                        {
                            sprintf(Identifier,
                                "Shared PCI Mach64 in slot %d:%d:%d",
                                pVideo->bus, pVideo->device, pVideo->func);
                            xf86MsgVerb(X_INFO, 3,
                                ATI_NAME ":  %s with sparse PIO base 0x%04lX"
                                " detected.\n", Identifier,
                                Mach64SparseIOBases[j]);
                            AddAdapter(pATI);
                            pATI->PCIInfo = pVideo;

                            if (pATI->VGAAdapter)
                                ATIFindVGA(pVideo, pATI, ProbeFlags);
                        }

                        xf86SetPciVideo(NULL, NONE);
                        break;
                }
            }

#else /* AVOID_CPIO */

            for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
            {
                if ((pVideo->vendor != PCI_VENDOR_ATI) ||
                    (pVideo->chipType == PCI_CHIP_MACH32) ||
                    pVideo->size[1])
                    continue;

                /* Check if this one has already been detected */
                for (j = 0;  j < nATIPtr;  j++)
                {
                    pATI = ATIPtrs[j];
                    if (pATI->PCIInfo == pVideo)
                        goto SkipThisSlot;
                }

                if (!xf86CheckPciSlot(pVideo->bus,
                                      pVideo->device,
                                      pVideo->func))
                    continue;

                xf86SetPciVideo(pVideo, MEM_IO);

                Chip = ATIChipID(pVideo->chipType, pVideo->chipRev);

                /* The adapter's CPIO base is of little concern here */
                pATI = ATIMach64Probe(pVideo, 0, SPARSE_IO, Chip);
                if (pATI)
                {
                    sprintf(Identifier, "Shared PCI Mach64 in slot %d:%d:%d",
                        pVideo->bus, pVideo->device, pVideo->func);
                    xf86MsgVerb(X_INFO, 3,
                        ATI_NAME ":  %s with Block 0 base 0x%08lX detected.\n",
                        Identifier, pATI->Block0Base);
                    AddAdapter(pATI);
                    pATI->PCIInfo = pVideo;
                }
                else
                {
                    xf86Msg(X_WARNING,
                        ATI_NAME ":  PCI Mach64 in slot %d:%d:%d could not be"
                        " detected!\n",
                        pVideo->bus, pVideo->device, pVideo->func);
                }

                xf86SetPciVideo(NULL, NONE);

        SkipThisSlot:;
            }

#endif /* AVOID_CPIO */

        }

        /* Lastly, look for block I/O devices */
        for (i = 0;  (pVideo = xf86PciVideoInfo[i++]);  )
        {
        /* For some Radeons, !pvideo->size[1] applies but the card still 
         * works properly. Thus don't make !pVideo->size[1] check to continue 
         * automatically (instead check that the chip actually is below 
         * ATI_CHIP_Mach64 (includes ATI_CHIP_NONE)). See fd.o bug 6796. */
            if ((pVideo->vendor != PCI_VENDOR_ATI) ||
                (pVideo->chipType == PCI_CHIP_MACH32) ||
                (!pVideo->size[1] && Chip < ATI_CHIP_Mach64))
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

            if (!nATIGDev)
                continue;

            /* Check if this one has already been detected */
            for (j = 0;  j < nATIPtr;  j++)
            {
                pATI = ATIPtrs[j];
                if (pATI->CPIOBase == pVideo->ioBase[1])
                    goto SetPCIInfo;
            }

            if (!xf86CheckPciSlot(pVideo->bus, pVideo->device, pVideo->func))
                continue;

            /* Probe for it */
            xf86SetPciVideo(pVideo, MEM_IO);

            pATI = ATIMach64Probe(pVideo, pVideo->ioBase[1], BLOCK_IO, Chip);
            if (pATI)
            {
                sprintf(Identifier, "Shared PCI/AGP Mach64 in slot %d:%d:%d",
                    pVideo->bus, pVideo->device, pVideo->func);
                xf86MsgVerb(X_INFO, 3, ATI_NAME ":  %s detected.\n",
                    Identifier);
                AddAdapter(pATI);

#ifndef AVOID_CPIO

                if (pATI->VGAAdapter)
                    ATIFindVGA(pVideo, pATI, ProbeFlags);

#endif /* AVOID_CPIO */

            }

            xf86SetPciVideo(NULL, NONE);

            if (!pATI)
            {
                xf86Msg(X_WARNING,
                    ATI_NAME ":  PCI/AGP Mach64 in slot %d:%d:%d could not be"
                    " detected!\n", pVideo->bus, pVideo->device, pVideo->func);
                continue;
            }

        SetPCIInfo:
            pATI->PCIInfo = pVideo;
        }
    }

    /*
     * Re-order list of detected devices so that the primary device is before
     * any other PCI device.
     */
    for (i = 0;  i < nATIPtr;  i++)
    {
        if (!ATIPtrs[i]->PCIInfo)
            continue;

        for (j = i;  j < nATIPtr;  j++)
        {
            pATI = ATIPtrs[j];
            if (!xf86IsPrimaryPci(pATI->PCIInfo))
                continue;

            for (;  j > i;  j--)
                ATIPtrs[j] = ATIPtrs[j - 1];
            ATIPtrs[j] = pATI;
            break;
        }

        break;
    }

    if (flags & PROBE_DETECT)
    {
        /*
         * No XF86Config information available, so use the default Chipset of
         * "ati", and as many device sections as there are adapters.
         */
        for (i = 0;  i < nATIPtr;  i++)
        {
            pATI = ATIPtrs[i];

            {
                ProbeSuccess = TRUE;
                pGDev = xf86AddDeviceToConfigure(ATI_DRIVER_NAME,
                    pATI->PCIInfo, ATI_CHIPSET_ATI);
                if (pGDev)
                {
                    /* Fill in additional information */
                    pGDev->vendor = ATI_NAME;
                    pGDev->chipset = (char *)ATIChipsetNames[ATI_CHIPSET_ATI];
                    if (!pATI->PCIInfo)
                        pGDev->busID = NULL;
                }
            }

            xfree(pATI);
        }
    }
    else
    {
        /*
         * Assign detected devices to XF86Config Device sections.  This is done
         * by comparing certain Device section specifications against the
         * corresponding adapter information.  Begin with those specifications
         * that are independent of the adapter's bus location.
         */
        for (i = 0, pATIGDev = ATIGDevs;  i < nATIGDev;  i++, pATIGDev++)
        {
            pGDev = pATIGDev->pGDev;

            for (j = 0;  j < nATIPtr;  j++)
            {
                pATI = ATIPtrs[j];

                /*
                 * First check the Chipset specification.  The placement of
                 * "break" and "continue" statements here is carefully chosen
                 * to produce the intended behaviour for each Chipset value.
                 */
                switch (pATIGDev->Chipset)
                {
                    case ATI_CHIPSET_ATI:
                    case ATI_CHIPSET_MACH64:
                        break;

                    default:
                        continue;
                }

                /*
                 * The ChipID and ChipRev specifications are compared next.
                 * First, require these to be unspecified for anything other
                 * than Mach32 or Mach64 adapters.  ChipRev is also required to
                 * be unspecified for Mach32's.  ChipID is optional for
                 * Mach32's, and both specifications are optional for Mach64's.
                 * Lastly, allow both specifications to override their detected
                 * value in the case of Mach64 adapters whose ChipID is
                 * unrecognised.
                 */
                pVideo = pATI->PCIInfo;
                if (pGDev->chipID >= 0)
                {
                    if ((pATI->ChipType != pGDev->chipID) &&
                        (!pVideo || (pGDev->chipID != pVideo->chipType)))
                    {
                        if ((pATI->Chip != ATI_CHIP_Mach64))
                            continue;

                        Chip = ATIChipID(pGDev->chipID, 0);
                        if ((Chip <= ATI_CHIP_264GTB) ||
                            (Chip == ATI_CHIP_Mach64))
                            continue;
                    }
                    if ((pGDev->chipRev >= 0) &&
                        (pATI->ChipRev != pGDev->chipRev) &&
                        (!pVideo || (pGDev->chipRev != pVideo->chipRev) ||
                         (pGDev->chipID != pVideo->chipType)))
                    {
                        if (pATI->Chip < ATI_CHIP_264CT)
                            continue;

                        if (pATI->Chip != ATI_CHIP_Mach64)
                        {
                            /*
                             * There are two foundry codes for UMC.  Some
                             * adapters will advertise one in CONFIG_CHIP_ID
                             * and the other in PCI configuration space.  For
                             * matching purposes, make both codes compare
                             * equal.
                             */
#                           define UMC_IGNORE \
                                (ATI_FOUNDRY_UMC ^ ATI_FOUNDRY_UMCA)
#                           define UMC_NOCARE \
                                GetBits(SetBits(UMC_IGNORE, CFG_CHIP_FOUNDRY), \
                                    CFG_CHIP_REV)

                            if ((pATI->ChipRev ^ pGDev->chipRev) & ~UMC_NOCARE)
                                continue;

                            if ((pATI->ChipFoundry != ATI_FOUNDRY_UMC) &&
                                (pATI->ChipFoundry != ATI_FOUNDRY_UMCA))
                                continue;

                            k = GetBits(pGDev->chipRev,
                                GetBits(CFG_CHIP_FOUNDRY, CFG_CHIP_REV));
                            if ((k != ATI_FOUNDRY_UMC) &&
                                (k != ATI_FOUNDRY_UMCA))
                                continue;
                        }
                    }
                }

                /*
                 * IOBase is next.  This is the first specification that is
                 * potentially dependent on bus location.  It is only allowed
                 * for Mach64 adapters, and is optional.
                 */
                if (pGDev->IOBase && (pATI->CPIOBase != pGDev->IOBase))
                    continue;

                /*
                 * Compare BusID's.  This specification is only allowed for PCI
                 * Mach32's or Mach64's and is optional.
                 */
                if (pGDev->busID && pGDev->busID[0])
                {
                    pVideo = pATI->PCIInfo;

#ifndef AVOID_CPIO

                    if (!pVideo)
                        continue;

#endif /* AVOID_CPIO */

                    if (!xf86ComparePciBusString(pGDev->busID,
                            pVideo->bus, pVideo->device, pVideo->func))
                        continue;
                }

                /*
                 * Ensure no two adapters are assigned to the same XF86Config
                 * Device section.
                 */
                if (pATIGDev->iATIPtr)
                {
                    if (pATIGDev->iATIPtr < 0)
                        break;

                    xf86Msg(X_ERROR,
                        ATI_NAME ":  XF86Config Device section \"%s\" may not"
                        " be assigned to more than one adapter.\n",
                        pGDev->identifier);
                    pATIGDev->iATIPtr = -1;
                    break;
                }

                /* Assign adapter */
                pATIGDev->iATIPtr = j + 1;

                /*
                 * For compatibility with previous releases, assign the first
                 * applicable adapter if there is only one Device section.
                 */
                if (nATIGDev == 1)
                    break;
            }
        }

        /*
         * Ensure no two XF86Config Device sections are assigned to the same
         * adapter.  Then, generate screens for any that are left.
         */
        for (i = 0, pATIGDev = ATIGDevs;  i < nATIGDev;  i++, pATIGDev++)
        {
            pGDev = pATIGDev->pGDev;

            j = pATIGDev->iATIPtr;
            if (j <= 0)
                continue;

            for (k = i;  ++k < nATIGDev;  )
            {
                if (j == ATIGDevs[k].iATIPtr)
                {
                    xf86Msg(X_ERROR,
                        ATI_NAME ":  XF86Config Device sections \"%s\" and"
                        " \"%s\" may not be assigned to the same adapter.\n",
                        pGDev->identifier, ATIGDevs[k].pGDev->identifier);
                    pATIGDev->iATIPtr = ATIGDevs[k].iATIPtr = -1;
                }
            }

            j = ATIGDevs[i].iATIPtr;
            if (j <= 0)
                continue;

            pATI = ATIPtrs[j - 1];

            xf86MsgVerb(X_INFO, 3,
                ATI_NAME ":  %s assigned to %sactive \"Device\" section"
                " \"%s\".\n",
                Identifier, pGDev->active ? "" : "in", pGDev->identifier);

            /*
             * Attach adapter to XF86Config Device section and register its
             * resources.
             */
            if (ATIClaimBusSlot(pDriver, pATIGDev->Chipset,
                                pGDev, pGDev->active, pATI) < 0)
            {
                xf86Msg(X_ERROR,
                    ATI_NAME ":  Could not claim bus slot for %s.\n",
                    Identifier);
                continue;
            }

            if (!pGDev->active)
                continue;

            /* Allocate screen */
            pScreenInfo = xf86AllocateScreen(pDriver, 0);

#ifdef XFree86LOADER

            if (!xf86LoadSubModule(pScreenInfo, "atimisc"))
            {
                xf86Msg(X_ERROR,
                    ATI_NAME ":  Failed to load \"atimisc\" module.\n");
                xf86DeleteScreen(pScreenInfo->scrnIndex, 0);
                continue;
            }

            xf86LoaderReqSymLists(ATISymbols, NULL);

#endif

            /* Attach device to screen */
            xf86AddEntityToScreen(pScreenInfo, pATI->iEntity);

            ATIPtrs[j - 1] = NULL;

            /* Fill in probe data */
	    ATIFillInScreenInfo(pScreenInfo);

            pScreenInfo->driverPrivate = pATI;

            pATI->Chipset = pATIGDev->Chipset;

            ProbeSuccess = TRUE;
        }

        /* Deal with unassigned adapters */
        for (i = 0;  i < nATIPtr;  i++)
        {
            if (!(pATI = ATIPtrs[i]))
                continue;

            {
                if (pATI->iEntity < 0)
                    (void)ATIClaimBusSlot(pDriver, 0, NULL, FALSE, pATI);
            }

            xfree(pATI);
        }

        xfree(ATIGDevs);
    }

    xfree(ATIPtrs);

    /* Call Rage 128 driver probe */
    if (DoRage128 && R128Probe(pDriver, flags))
        ProbeSuccess = TRUE;

    /* Call Radeon driver probe */
    if (DoRadeon && RADEONProbe(pDriver, flags))
        ProbeSuccess = TRUE;

    return ProbeSuccess;
}
