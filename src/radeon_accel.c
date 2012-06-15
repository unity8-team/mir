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

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *   Rickard E. Faith <faith@valinux.com>
 *   Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
 * Credits:
 *
 *   Thanks to Ani Joshi <ajoshi@shell.unixbox.com> for providing source
 *   code to his Radeon driver.  Portions of this file are based on the
 *   initialization code for that driver.
 *
 * References:
 *
 * !!!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 * Notes on unimplemented XAA optimizations:
 *
 *   SetClipping:   This has been removed as XAA expects 16bit registers
 *                  for full clipping.
 *   TwoPointLine:  The Radeon supports this. Not Bresenham.
 *   DashedLine with non-power-of-two pattern length: Apparently, there is
 *                  no way to set the length of the pattern -- it is always
 *                  assumed to be 8 or 32 (or 1024?).
 *   ScreenToScreenColorExpandFill: See p. 4-17 of the Technical Reference
 *                  Manual where it states that monochrome expansion of frame
 *                  buffer data is not supported.
 *   CPUToScreenColorExpandFill, direct: The implementation here uses a hybrid
 *                  direct/indirect method.  If we had more data registers,
 *                  then we could do better.  If XAA supported a trigger write
 *                  address, the code would be simpler.
 *   Color8x8PatternFill: Apparently, an 8x8 color brush cannot take an 8x8
 *                  pattern from frame buffer memory.
 *   ImageWrites:   Same as CPUToScreenColorExpandFill
 *
 */

#include <errno.h>
#include <string.h>
#include <assert.h>
				/* Driver data structures */
#include "radeon.h"
#include "radeon_reg.h"
#include "r600_reg.h"
#include "radeon_macros.h"
#include "radeon_probe.h"
#include "radeon_version.h"

#include "ati_pciids_gen.h"

				/* Line support */
#include "miline.h"

				/* X and server generic header files */
#include "xf86.h"

static int RADEONDRMGetNumPipes(ScrnInfoPtr pScrn, int *num_pipes)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);
    struct drm_radeon_info np2;
    np2.value = (unsigned long)num_pipes;
    np2.request = RADEON_INFO_NUM_GB_PIPES;
    return drmCommandWriteRead(info->dri2.drm_fd, DRM_RADEON_INFO, &np2, sizeof(np2));
}

/* Initialize the acceleration hardware */
void RADEONEngineInit(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr  info       = RADEONPTR(pScrn);
    int datatype = 0;
    info->accel_state->num_gb_pipes = 0;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		   "EngineInit (%d/%d)\n",
		   info->CurrentLayout.pixel_code,
		   info->CurrentLayout.bitsPerPixel);

    if (info->directRenderingEnabled && (IS_R300_3D || IS_R500_3D)) {
	int num_pipes;

	if(RADEONDRMGetNumPipes(pScrn, &num_pipes) < 0) {
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		       "Failed to determine num pipes from DRM, falling back to "
		       "manual look-up!\n");
	    info->accel_state->num_gb_pipes = 0;
	} else {
	    info->accel_state->num_gb_pipes = num_pipes;
	}
    }

    switch (info->CurrentLayout.pixel_code) {
    case 8:  datatype = 2; break;
    case 15: datatype = 3; break;
    case 16: datatype = 4; break;
    case 24: datatype = 5; break;
    case 32: datatype = 6; break;
    default:
	xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, RADEON_LOGLEVEL_DEBUG,
		       "Unknown depth/bpp = %d/%d (code = %d)\n",
		       info->CurrentLayout.depth,
		       info->CurrentLayout.bitsPerPixel,
		       info->CurrentLayout.pixel_code);
    }

    info->accel_state->dp_gui_master_cntl =
	((datatype << RADEON_GMC_DST_DATATYPE_SHIFT)
	 | RADEON_GMC_CLR_CMP_CNTL_DIS
	 | RADEON_GMC_DST_PITCH_OFFSET_CNTL);

}

uint32_t radeonGetPixmapOffset(PixmapPtr pPix)
{
    return 0;
}

int radeon_cs_space_remaining(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR(pScrn);

    return (info->cs->ndw - info->cs->cdw);
}

#define BEGIN_ACCEL(n)          BEGIN_RING(2*(n))
#define OUT_ACCEL_REG(reg, val) OUT_RING_REG(reg, val)
#define FINISH_ACCEL()          ADVANCE_RING()


#include "radeon_commonfuncs.c"

#undef BEGIN_ACCEL
#undef OUT_ACCEL_REG
#undef FINISH_ACCEL

#define RADEON_IB_RESERVE (16 * sizeof(uint32_t))

void RADEONCopySwap(uint8_t *dst, uint8_t *src, unsigned int size, int swap)
{
    switch(swap) {
    case RADEON_HOST_DATA_SWAP_HDW:
        {
	    unsigned int *d = (unsigned int *)dst;
	    unsigned int *s = (unsigned int *)src;
	    unsigned int nwords = size >> 2;

	    for (; nwords > 0; --nwords, ++d, ++s)
		*d = ((*s & 0xffff) << 16) | ((*s >> 16) & 0xffff);
	    return;
        }
    case RADEON_HOST_DATA_SWAP_32BIT:
        {
	    unsigned int *d = (unsigned int *)dst;
	    unsigned int *s = (unsigned int *)src;
	    unsigned int nwords = size >> 2;

	    for (; nwords > 0; --nwords, ++d, ++s)
#ifdef __powerpc__
		asm volatile("stwbrx %0,0,%1" : : "r" (*s), "r" (d));
#else
		*d = ((*s >> 24) & 0xff) | ((*s >> 8) & 0xff00)
			| ((*s & 0xff00) << 8) | ((*s & 0xff) << 24);
#endif
	    return;
        }
    case RADEON_HOST_DATA_SWAP_16BIT:
        {
	    unsigned short *d = (unsigned short *)dst;
	    unsigned short *s = (unsigned short *)src;
	    unsigned int nwords = size >> 1;

	    for (; nwords > 0; --nwords, ++d, ++s)
#ifdef __powerpc__
		asm volatile("sthbrx %0,0,%1" : : "r" (*s), "r" (d));
#else
	        *d = (*s >> 8) | (*s << 8);
#endif
	    return;
	}
    }
    if (src != dst)
	memcpy(dst, src, size);
}



Bool RADEONAccelInit(ScreenPtr pScreen)
{
    ScrnInfoPtr    pScrn = xf86ScreenToScrn(pScreen);
    RADEONInfoPtr  info  = RADEONPTR(pScrn);

    if (info->directRenderingEnabled) {
	if (info->ChipFamily >= CHIP_FAMILY_CEDAR) {
	    if (!EVERGREENDrawInit(pScreen))
		return FALSE;
	} else
	    if (info->ChipFamily >= CHIP_FAMILY_R600) {
		if (!R600DrawInit(pScreen))
		    return FALSE;
	    } else {
		if (!RADEONDrawInitCP(pScreen))
		    return FALSE;
	    }
    }
    return TRUE;
}

void RADEONInit3DEngine(ScrnInfoPtr pScrn)
{
    RADEONInfoPtr info = RADEONPTR (pScrn);

    if (info->directRenderingEnabled) {
	RADEONInit3DEngineCP(pScrn);
    }
    info->accel_state->XInited3D = TRUE;
}

