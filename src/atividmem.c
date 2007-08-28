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
#include "atistruct.h"
#include "atividmem.h"

/* Memory types for 68800's and 88800GX's */
const char *ATIMemoryTypeNames_Mach[] =
{
    "DRAM (256Kx4)",
    "VRAM (256Kx4, x8, x16)",
    "VRAM (256Kx16 with short shift register)",
    "DRAM (256Kx16)",
    "Graphics DRAM (256Kx16)",
    "Enhanced VRAM (256Kx4, x8, x16)",
    "Enhanced VRAM (256Kx16 with short shift register)",
    "Unknown video memory type"
};

/* Memory types for 88800CX's */
const char *ATIMemoryTypeNames_88800CX[] =
{
    "DRAM (256Kx4, x8, x16)",
    "EDO DRAM (256Kx4, x8, x16)",
    "Unknown video memory type",
    "DRAM (256Kx16 with assymetric RAS/CAS)",
    "Unknown video memory type",
    "Unknown video memory type",
    "Unknown video memory type",
    "Unknown video memory type"
};

/* Memory types for 264xT's */
const char *ATIMemoryTypeNames_264xT[] =
{
    "Disabled video memory",
    "DRAM",
    "EDO DRAM",
    "Pseudo-EDO DRAM",
    "SDRAM (1:1)",
    "SGRAM (1:1)",
    "SGRAM (2:1) 32-bit",
    "Unknown video memory type"
};

/*
 * FIXME: This is an internal Xserver function that should be exported and
 * called explicitely with pci-rework, pci-rework does not setup mtrr's.
 *
 * It is called implicitely by xf86MapPciMem(VIDMEM_FRAMEBUFFER).
 */
#ifndef XSERVER_LIBPCIACCESS
#define nop_setWC(_screenNum, _base, _size, _enable) \
do {                                                 \
} while (0)
#else
#define nop_setWC(_screenNum, _base, _size, _enable) \
do {                                                 \
    /* XXX */                                        \
} while (0)
#endif

#ifndef AVOID_CPIO

/*
 * ATIUnmapVGA --
 *
 * Unmap VGA aperture.
 */
static void
ATIUnmapVGA
(
    int    iScreen,
    ATIPtr pATI
)
{
    if (!pATI->pBank)
        return;

    xf86UnMapVidMem(iScreen, pATI->pBank, 0x00010000U);

    pATI->pBank = NULL;
}

#endif /* AVOID_CPIO */

/*
 * ATIUnmapLinear --
 *
 * Unmap linear aperture.
 */
static void
ATIUnmapLinear
(
    int    iScreen,
    ATIPtr pATI
)
{
    pciVideoPtr pVideo = pATI->PCIInfo;

    if (pATI->pMemoryLE)
    {
        if (pATI->LinearBase)
            nop_setWC(iScreen, pATI->LinearBase, pATI->LinearSize, FALSE);

#ifndef XSERVER_LIBPCIACCESS
        xf86UnMapVidMem(iScreen, pATI->pMemoryLE, (1U << pVideo->size[0]));
#else
        pci_device_unmap_region(pVideo, 0);
#endif
    }

    pATI->pMemory = pATI->pMemoryLE = NULL;

    pATI->pCursorImage = NULL;
}

/*
 * ATIUnmapMMIO --
 *
 * Unmap MMIO registers.
 */
static void
ATIUnmapMMIO
(
    int    iScreen,
    ATIPtr pATI
)
{
    pciVideoPtr pVideo = pATI->PCIInfo;

    if (pATI->pMMIO)
    {
#ifndef XSERVER_LIBPCIACCESS
        xf86UnMapVidMem(iScreen, pATI->pMMIO, getpagesize());
#else
        pci_device_unmap_region(pVideo, 2);
#endif
    }

    pATI->pMMIO = pATI->pBlock[0] = pATI->pBlock[1] = NULL;
}

/*
 * ATIMapApertures --
 *
 * This function maps all apertures used by the driver.
 *
 * It is called three times:
 * - to setup MMIO for an MMIO-only driver during Probe
 * - to setup MMIO for an MMIO-only driver during PreInit
 * - to setup MMIO (with Block0Base set) and FB (with LinearBase set)
 */
Bool
ATIMapApertures
(
    int    iScreen,
    ATIPtr pATI
)
{
    pciVideoPtr pVideo = pATI->PCIInfo;

    if (pATI->Mapped)
        return TRUE;

#ifndef AVOID_CPIO

    /* Map VGA aperture */
    if (pATI->VGAAdapter)
    {
        /*
         * No relocation, resizing, caching or write-combining of this
         * aperture is supported.  Hence, the hard-coded values here...
         */
            pATI->pBank = xf86MapDomainMemory(iScreen, VIDMEM_MMIO_32BIT,
                PCI_CFG_TAG(pVideo), 0x000A0000U, 0x00010000U);

        if (!pATI->pBank)
            goto bail;

        pATI->Mapped = TRUE;
    }

#endif /* AVOID_CPIO */

    /* Map linear aperture */
    if (pATI->LinearBase || (pATI->Block0Base && pATI->MMIOInLinear))
    {

#ifndef XSERVER_LIBPCIACCESS

        int mode = VIDMEM_FRAMEBUFFER;
    
        /* Reset write-combining for the whole FB when MMIO registers fall in
         * the linear aperture.
         */
        if (pATI->MMIOInLinear)
            mode = VIDMEM_MMIO;
    
        pATI->pMemoryLE = xf86MapPciMem(iScreen, mode, PCI_CFG_TAG(pVideo),
                                        pVideo->memBase[0],
                                        (1U << pVideo->size[0]));

#else /* XSERVER_LIBPCIACCESS */

        int err = pci_device_map_region(pVideo, 0, TRUE);
    
        if (err)
            pATI->pMemoryLE = NULL;
        else
            pATI->pMemoryLE = pVideo->regions[0].memory;

#endif /* XSERVER_LIBPCIACCESS */

        if (!pATI->pMemoryLE)
            goto bail;

        pATI->Mapped = TRUE;

#if X_BYTE_ORDER == X_LITTLE_ENDIAN

        pATI->pMemory = pATI->pMemoryLE;

#else /* if X_BYTE_ORDER != X_LITTLE_ENDIAN */

        pATI->pMemory = (char *)pATI->pMemoryLE + 0x00800000U;

#endif /* X_BYTE_ORDER */

        /* Set write-combining for the FB (and the HW cursor on LE) */
        if (pATI->LinearBase)
            nop_setWC(iScreen, pATI->LinearBase, pATI->LinearSize, TRUE);

        if (pATI->CursorBase)
            pATI->pCursorImage = (char *)pATI->pMemoryLE + pATI->CursorOffset;
    }

    /* Map MMIO aperture */
    if (pATI->Block0Base && !pATI->MMIOInLinear)
    {

#ifndef XSERVER_LIBPCIACCESS

        int mode = VIDMEM_MMIO;
    
        pATI->pMMIO = xf86MapPciMem(iScreen, mode, PCI_CFG_TAG(pVideo),
                                    pVideo->memBase[2],
                                    getpagesize());

#else /* XSERVER_LIBPCIACCESS */

        int err = pci_device_map_region(pVideo, 2, TRUE);
    
        if (err)
            pATI->pMMIO = NULL;
        else
            pATI->pMMIO = pVideo->regions[2].memory;

#endif /* XSERVER_LIBPCIACCESS */

        if (!pATI->pMMIO)
            goto bail;

        pATI->Mapped = TRUE;

        pATI->pBlock[0] = (char *)pATI->pMMIO + 0x00000400U;

        if (pATI->Block1Base)
            pATI->pBlock[1] = (char *)pATI->pBlock[0] - 0x00000400U;
    }
    else if (pATI->Block0Base)
    {
        unsigned long mmio_offset, linear_size;

        mmio_offset = pATI->Block0Base - PCI_REGION_BASE(pVideo, 0, REGION_MEM);

        linear_size = PCI_REGION_SIZE(pVideo, 0);

        pATI->pMMIO = NULL;

        /* Check that requested MMIO offset falls in the linear aperture. This
         * ensures that we do not poke outside a mapped region and bails early
         * for old mach64 cards with a 4MB linear aperture (unless they have an
         * extended BE aperture which would give a size of 8MB).
         */
        if (mmio_offset + 0x00000400U > linear_size)
            goto bail;

        pATI->Mapped = TRUE;

        pATI->pBlock[0] = (char *)pATI->pMemoryLE + mmio_offset;

        if (pATI->Block1Base)
            pATI->pBlock[1] = (char *)pATI->pBlock[0] - 0x00000400U;
    }

    return TRUE;

bail:

    ATIUnmapLinear(iScreen, pATI);

#ifndef AVOID_CPIO

    ATIUnmapVGA(iScreen, pATI);

#endif /* AVOID_CPIO */

    pATI->Mapped = FALSE;

    return FALSE;
}

/*
 * ATIUnmapApertures --
 *
 * This function unmaps all apertures used by the driver.
 */
void
ATIUnmapApertures
(
    int    iScreen,
    ATIPtr pATI
)
{
    if (!pATI->Mapped)
        return;
    pATI->Mapped = FALSE;

    /* Unmap MMIO area */
    ATIUnmapMMIO(iScreen, pATI);

    /* Unmap linear aperture */
    ATIUnmapLinear(iScreen, pATI);

#ifndef AVOID_CPIO

    /* Unmap VGA aperture */
    ATIUnmapVGA(iScreen, pATI);

#endif /* AVOID_CPIO */

}
