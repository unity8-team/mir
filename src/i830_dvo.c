/**************************************************************************

Copyright 2006 Dave Airlie <airlied@linux.ie>

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
THE COPYRIGHT HOLDERS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "i830.h"

#include "sil164/sil164.h"
#include "ch7xxx/ch7xxx.h"

static const char *SIL164Symbols[] = {
    "Sil164VidOutput",
    NULL
};
static const char *CH7xxxSymbols[] = {
    "CH7xxxVidOutput",
    NULL
};

/* driver list */
struct _I830DVODriver i830_dvo_drivers[] =
{
	{ I830_DVO_CHIP_TMDS, "sil164", "SIL164VidOutput",
		(SIL164_ADDR_1<<1), SIL164Symbols, NULL , NULL, NULL},
	{ I830_DVO_CHIP_TMDS | I830_DVO_CHIP_TVOUT, "ch7xxx", "CH7xxxVidOutput",
		(CH7xxx_ADDR_1<<1), CH7xxxSymbols, NULL , NULL, NULL}
};

#define I830_NUM_DVO_DRIVERS (sizeof(i830_dvo_drivers)/sizeof(struct _I830DVODriver))

static void
i830_dvo_dpms(ScrnInfoPtr pScrn, I830OutputPtr output, int mode)
{
    if (output->i2c_drv == NULL)
	return;

    if (mode == DPMSModeOn)
	output->i2c_drv->vid_rec->Power(output->i2c_drv->dev_priv, TRUE);
    else
	output->i2c_drv->vid_rec->Power(output->i2c_drv->dev_priv, FALSE);
}

static void
i830_dvo_save(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (output->i2c_drv == NULL)
	return;

    /* Each output should probably just save the registers it touches, but for
     * now, use more overkill.
     */
    pI830->saveDVOA = INREG(DVOA);
    pI830->saveDVOB = INREG(DVOB);
    pI830->saveDVOC = INREG(DVOC);

    output->i2c_drv->vid_rec->SaveRegs(output->i2c_drv->dev_priv);
}

static void
i830_dvo_restore(ScrnInfoPtr pScrn, I830OutputPtr output)
{
    I830Ptr pI830 = I830PTR(pScrn);

    if (output->i2c_drv == NULL)
	return;

    OUTREG(DVOA, pI830->saveDVOA);
    OUTREG(DVOB, pI830->saveDVOB);
    OUTREG(DVOC, pI830->saveDVOC);

    output->i2c_drv->vid_rec->RestoreRegs(output->i2c_drv->dev_priv);
}

Bool
I830I2CDetectDVOControllers(ScrnInfoPtr pScrn, I2CBusPtr pI2CBus,
			    struct _I830DVODriver **retdrv)
{
    int i;
    void *ret_ptr;
    struct _I830DVODriver *drv;

    for (i = 0; i < I830_NUM_DVO_DRIVERS; i++) {
	drv = &i830_dvo_drivers[i];
	drv->modhandle = xf86LoadSubModule(pScrn, drv->modulename);
	if (drv->modhandle == NULL)
	    continue;

	xf86LoaderReqSymLists(drv->symbols, NULL);

	ret_ptr = NULL;
	drv->vid_rec = LoaderSymbol(drv->fntablename);
	if (drv->vid_rec != NULL)
	    ret_ptr = drv->vid_rec->Detect(pI2CBus, drv->address);

	if (ret_ptr != NULL) {
	    drv->dev_priv = ret_ptr;
	    *retdrv = drv;
	    return TRUE;
	}
	xf86UnloadSubModule(drv->modhandle);
    }
    return FALSE;
}

void
i830_dvo_init(ScrnInfoPtr pScrn)
{
    I830Ptr pI830 = I830PTR(pScrn);
    Bool ret;
    int i = pI830->num_outputs;

    pI830->output[i].type = I830_OUTPUT_DVO;
    pI830->output[i].dpms = i830_dvo_dpms;
    pI830->output[i].save = i830_dvo_save;
    pI830->output[i].restore = i830_dvo_restore;

    /* Set up the I2C and DDC buses */
    ret = I830I2CInit(pScrn, &pI830->output[i].pI2CBus, GPIOE, "DVOI2C_E");
    if (!ret)
	return;

    ret = I830I2CInit(pScrn, &pI830->output[i].pDDCBus, GPIOD, "DVODDC_D");
    if (!ret) {
	xf86DestroyI2CBusRec(pI830->output[i].pI2CBus, TRUE, TRUE);
	return;
    }

    /* Now, try to find a controller */
    ret = I830I2CDetectDVOControllers(pScrn, pI830->output[i].pI2CBus,
				      &pI830->output[i].i2c_drv);
    if (ret) {
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Found i2c %s on %08lX\n",
		   pI830->output[i].i2c_drv->modulename,
		   pI830->output[i].pI2CBus->DriverPrivate.uval);
    } else {
	xf86DestroyI2CBusRec(pI830->output[i].pI2CBus, TRUE, TRUE);
	xf86DestroyI2CBusRec(pI830->output[i].pDDCBus, TRUE, TRUE);
	return;
    }

    pI830->num_outputs++;
}
