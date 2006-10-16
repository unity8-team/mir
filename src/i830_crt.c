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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "i830.h"

static void
i830_crt_dpms(ScrnInfoPtr pScrn, I830OutputPtr output, int mode)
{
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 temp;

    temp = INREG(ADPA);
    temp &= ~(ADPA_HSYNC_CNTL_DISABLE | ADPA_VSYNC_CNTL_DISABLE);
    temp &= ~ADPA_DAC_ENABLE;

    switch(mode) {
    case DPMSModeOn:
	temp |= ADPA_DAC_ENABLE;
	break;
    case DPMSModeStandby:
	temp |= ADPA_DAC_ENABLE | ADPA_HSYNC_CNTL_DISABLE;
	break;
    case DPMSModeSuspend:
	temp |= ADPA_DAC_ENABLE | ADPA_VSYNC_CNTL_DISABLE;
	break;
    case DPMSModeOff:
	temp |= ADPA_HSYNC_CNTL_DISABLE | ADPA_VSYNC_CNTL_DISABLE;
	break;
    }

    OUTREG(ADPA, temp);
}

static void
i830_crt_save(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->saveADPA = INREG(ADPA);
}

static void
i830_crt_restore(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);

    OUTREG(ADPA, pI830->saveADPA);
}

static int
i830_crt_mode_valid(ScrnInfoPtr pScrn, I830OutputPtr output,
		    DisplayModePtr pMode)
{
    return MODE_OK;
}

static void
i830_crt_pre_set_mode(ScrnInfoPtr pScrn, I830OutputPtr output,
		      DisplayModePtr pMode)
{
}

static void
i830_crt_post_set_mode(ScrnInfoPtr pScrn, I830OutputPtr output,
		       DisplayModePtr pMode)
{
    I830Ptr pI830 = I830PTR(pScrn);

    CARD32 adpa;

    adpa = ADPA_DAC_ENABLE;

    if (pMode->Flags & V_PHSYNC)
	adpa |= ADPA_HSYNC_ACTIVE_HIGH;
    if (pMode->Flags & V_PVSYNC)
	adpa |= ADPA_VSYNC_ACTIVE_HIGH;

    if (output->pipe == 0)
	adpa |= ADPA_PIPE_A_SELECT;
    else
	adpa |= ADPA_PIPE_B_SELECT;

    OUTREG(ADPA, adpa);
}

void
i830_crt_init(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);

    pI830->output[pI830->num_outputs].type = I830_OUTPUT_ANALOG;
    pI830->output[pI830->num_outputs].dpms = i830_crt_dpms;
    pI830->output[pI830->num_outputs].save = i830_crt_save;
    pI830->output[pI830->num_outputs].restore = i830_crt_restore;
    pI830->output[pI830->num_outputs].mode_valid = i830_crt_mode_valid;
    pI830->output[pI830->num_outputs].pre_set_mode = i830_crt_pre_set_mode;
    pI830->output[pI830->num_outputs].post_set_mode = i830_crt_post_set_mode;

    /* Set up the DDC bus. */
    I830I2CInit(pScrn, &pI830->output[pI830->num_outputs].pDDCBus,
		GPIOA, "CRTDDC_A");

    pI830->num_outputs++;
}
