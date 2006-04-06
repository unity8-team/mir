/*
 * Copyright © 2006 Intel Corporation
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
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "xf86_ansic.h"
#include "i830.h"
#include "i830_bios.h"

#define INTEL_BIOS_8(_addr)	(pI830->VBIOS[_addr])
#define INTEL_BIOS_16(_addr)	(pI830->VBIOS[_addr] | \
				 (pI830->VBIOS[_addr + 1] << 8))
#define INTEL_BIOS_32(_addr)	(pI830->VBIOS[_addr] | \
				 (pI830->VBIOS[_addr + 1] << 8) \
				 (pI830->VBIOS[_addr + 2] << 16) \
				 (pI830->VBIOS[_addr + 3] << 24))

/* XXX */
#define INTEL_VBIOS_SIZE (64 * 1024)

/**
 * Loads the Video BIOS and checks that the VBT exists.
 *
 * VBT existence is a sanity check that is relied on by other i830_bios.c code.
 * Note that it would be better to use a BIOS call to get the VBT, as BIOSes may
 * feed an updated VBT back through that, compared to what we'll fetch using
 * this method of groping around in the BIOS data.
 */
static Bool
i830GetBIOS(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    struct vbt_header *vbt;
    int vbt_off;

    if (pI830->VBIOS != NULL)
	return TRUE;

    pI830->VBIOS = xalloc(INTEL_VBIOS_SIZE);
    if (pI830->VBIOS == NULL)
	return FALSE;

    if (pI830->pVbe != NULL) {
	memcpy(pI830->VBIOS, (void *)(pI830->pVbe->pInt10->BIOSseg << 4),
	       INTEL_VBIOS_SIZE);
    } else {
	xf86ReadPciBIOS(0, pI830->PciTag, 0, pI830->VBIOS, INTEL_VBIOS_SIZE);
    }

    vbt_off = INTEL_BIOS_16(0x1a);
    if (vbt_off >= INTEL_VBIOS_SIZE) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Bad VBT offset: 0x%x\n",
		   vbt_off);
	xfree(pI830->VBIOS);
	return FALSE;
    }

    vbt = (struct vbt_header *)(pI830->VBIOS + vbt_off);

    if (memcmp(vbt->signature, "$VBT", 4) != 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Bad VBT signature\n");
	xfree(pI830->VBIOS);
	return FALSE;
    }

    return TRUE;
}

Bool
i830GetLVDSInfoFromBIOS(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    struct vbt_header *vbt;
    struct bdb_header *bdb;
    int vbt_off, bdb_block_off, block_size;

    if (!i830GetBIOS(pScrn))
	return FALSE;

    vbt_off = INTEL_BIOS_16(0x1a);
    vbt = (struct vbt_header *)(pI830->VBIOS + vbt_off);
    bdb = (struct bdb_header *)(pI830->VBIOS + vbt_off + vbt->bdb_offset);

    if (memcmp(bdb->signature, "BIOS_DATA_BLOCK ", 16) != 0) {
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "Bad BDB signature\n");
	return FALSE;
    }

    for (bdb_block_off = bdb->header_size; bdb_block_off < bdb->bdb_size;
	 bdb_block_off += block_size)
    {
	int start = vbt_off + vbt->bdb_offset + bdb_block_off;
	int id;

	id = INTEL_BIOS_8(start);
	block_size = INTEL_BIOS_16(start + 1) + 3;
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Found BDB block type %d\n", id);
    }
    return TRUE;
}
