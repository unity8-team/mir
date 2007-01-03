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

******
********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "i830.h"
#include "i810_reg.h"

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

#if 0
static const char *ivch_symbols[] = {
    "ivch_methods",
    NULL
};
#endif
#if 0
static const char *ch7017_symbols[] = {
    "ch7017_methods",
    NULL
};
#endif

/* driver list */
struct _I830DVODriver i830_dvo_drivers[] =
{
    {I830_DVO_CHIP_TMDS, "sil164", "SIL164VidOutput",
     (SIL164_ADDR_1<<1), SIL164Symbols, NULL , NULL, NULL},
    {I830_DVO_CHIP_TMDS | I830_DVO_CHIP_TVOUT, "ch7xxx", "CH7xxxVidOutput",
     (CH7xxx_ADDR_1<<1), CH7xxxSymbols, NULL , NULL, NULL},
    /*
    {I830_DVO_CHIP_LVDS, "ivch", "ivch_methods",
     (0x2 << 1), ivch_symbols, NULL, NULL, NULL},
    */
    /*
    { I830_DVO_CHIP_LVDS, "ch7017", "ch7017_methods",
      (CH7017_ADDR_1 << 1), ch7017_symbols, NULL, NULL, NULL }
    */
};

#define I830_NUM_DVO_DRIVERS (sizeof(i830_dvo_drivers)/sizeof(struct _I830DVODriver))

static void
i830_dvo_dpms(xf86OutputPtr output, int mode)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    I830OutputPrivatePtr    intel_output = output->driver_private;
    void *		    dev_priv = intel_output->i2c_drv->dev_priv;

    if (mode == DPMSModeOn) {
	OUTREG(DVOC, INREG(DVOC) | DVO_ENABLE);
	(*intel_output->i2c_drv->vid_rec->dpms)(dev_priv, mode);
    } else {
	(*intel_output->i2c_drv->vid_rec->dpms)(dev_priv, mode);
	OUTREG(DVOC, INREG(DVOC) & ~DVO_ENABLE);
    }
}

static void
i830_dvo_save(xf86OutputPtr output)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    I830OutputPrivatePtr    intel_output = output->driver_private;
    void *		    dev_priv = intel_output->i2c_drv->dev_priv;

    /* Each output should probably just save the registers it touches, but for
     * now, use more overkill.
     */
    pI830->saveDVOA = INREG(DVOA);
    pI830->saveDVOB = INREG(DVOB);
    pI830->saveDVOC = INREG(DVOC);

    (*intel_output->i2c_drv->vid_rec->save)(dev_priv);
}

static void
i830_dvo_restore(xf86OutputPtr output)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    I830OutputPrivatePtr    intel_output = output->driver_private;
    void *		    dev_priv = intel_output->i2c_drv->dev_priv;

    OUTREG(DVOA, pI830->saveDVOA);
    OUTREG(DVOB, pI830->saveDVOB);
    OUTREG(DVOC, pI830->saveDVOC);

    (*intel_output->i2c_drv->vid_rec->restore)(dev_priv);
}

static int
i830_dvo_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
    I830OutputPrivatePtr    intel_output = output->driver_private;
    void *dev_priv = intel_output->i2c_drv->dev_priv;

    if (pMode->Flags & V_DBLSCAN)
	return MODE_NO_DBLESCAN;

    /* XXX: Validate clock range */

    return intel_output->i2c_drv->vid_rec->mode_valid(dev_priv, pMode);
}

static Bool
i830_dvo_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		    DisplayModePtr adjusted_mode)
{
    /* XXX: Hook this up to a DVO driver function */

    return TRUE;
}

static void
i830_dvo_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		  DisplayModePtr adjusted_mode)
{
    ScrnInfoPtr		    pScrn = output->scrn;
    I830Ptr		    pI830 = I830PTR(pScrn);
    xf86CrtcPtr	    crtc = output->crtc;
    I830CrtcPrivatePtr	    intel_crtc = crtc->driver_private;
    I830OutputPrivatePtr    intel_output = output->driver_private;
    int			    pipe = intel_crtc->pipe;
    CARD32		    dvo;
    int			    dpll_reg = (pipe == 0) ? DPLL_A : DPLL_B;

    intel_output->i2c_drv->vid_rec->mode_set(intel_output->i2c_drv->dev_priv,
					     mode);

    /* Save the data order, since I don't know what it should be set to. */
    dvo = INREG(DVOC) & (DVO_PRESERVE_MASK | DVO_DATA_ORDER_GBRG);
    dvo |= DVO_DATA_ORDER_FP | DVO_BORDER_ENABLE | DVO_BLANK_ACTIVE_HIGH;

    if (pipe == 1)
	dvo |= DVO_PIPE_B_SELECT;

    if (adjusted_mode->Flags & V_PHSYNC)
	dvo |= DVO_HSYNC_ACTIVE_HIGH;
    if (adjusted_mode->Flags & V_PVSYNC)
	dvo |= DVO_VSYNC_ACTIVE_HIGH;

    OUTREG(dpll_reg, INREG(dpll_reg) | DPLL_DVO_HIGH_SPEED);

    /*OUTREG(DVOB_SRCDIM,
      (adjusted_mode->HDisplay << DVO_SRCDIM_HORIZONTAL_SHIFT) |
      (adjusted_mode->VDisplay << DVO_SRCDIM_VERTICAL_SHIFT));*/
    OUTREG(DVOC_SRCDIM,
	   (adjusted_mode->HDisplay << DVO_SRCDIM_HORIZONTAL_SHIFT) |
	   (adjusted_mode->VDisplay << DVO_SRCDIM_VERTICAL_SHIFT));
    /*OUTREG(DVOB, dvo);*/
    OUTREG(DVOC, dvo);
}

/**
 * Detect the output connection on our DVO device.
 *
 * Unimplemented.
 */
static xf86OutputStatus
i830_dvo_detect(xf86OutputPtr output)
{
    I830OutputPrivatePtr    intel_output = output->driver_private;
    void *dev_priv = intel_output->i2c_drv->dev_priv;

    return intel_output->i2c_drv->vid_rec->detect(dev_priv);
}

static void
i830_dvo_destroy (xf86OutputPtr output)
{
    I830OutputPrivatePtr    intel_output = output->driver_private;

    if (intel_output)
    {
	if (intel_output->pI2CBus)
	    xf86DestroyI2CBusRec (intel_output->pI2CBus, TRUE, TRUE);
	if (intel_output->pDDCBus)
	    xf86DestroyI2CBusRec (intel_output->pDDCBus, TRUE, TRUE);
	/* XXX sub module cleanup? */
	xfree (intel_output);
    }
}

static const xf86OutputFuncsRec i830_dvo_output_funcs = {
    .dpms = i830_dvo_dpms,
    .save = i830_dvo_save,
    .restore = i830_dvo_restore,
    .mode_valid = i830_dvo_mode_valid,
    .mode_fixup = i830_dvo_mode_fixup,
    .mode_set = i830_dvo_mode_set,
    .detect = i830_dvo_detect,
    .get_modes = i830_ddc_get_modes,
    .destroy = i830_dvo_destroy
};

void
i830_dvo_init(ScrnInfoPtr pScrn)
{
    xf86OutputPtr output;
    I830OutputPrivatePtr intel_output;
    int ret;
    int i;
    void *ret_ptr;
    struct _I830DVODriver *drv;
    int gpio_inited = 0;
    I2CBusPtr pI2CBus = NULL;

    output = xf86OutputCreate (pScrn, &i830_dvo_output_funcs,
				   "TMDS");
    if (!output)
	return;
    intel_output = xnfcalloc (sizeof (I830OutputPrivateRec), 1);
    if (!intel_output)
    {
	xf86OutputDestroy (output);
	return;
    }
    intel_output->type = I830_OUTPUT_DVO;
    output->driver_private = intel_output;
    output->subpixel_order = SubPixelHorizontalRGB;
    output->interlaceAllowed = FALSE;
    output->doubleScanAllowed = FALSE;
    
    /* Set up the DDC bus */
    ret = I830I2CInit(pScrn, &intel_output->pDDCBus, GPIOD, "DVODDC_D");
    if (!ret)
    {
	xf86OutputDestroy (output);
	return;
    }

    /* Now, try to find a controller */
    for (i = 0; i < I830_NUM_DVO_DRIVERS; i++) {
	int gpio;

	drv = &i830_dvo_drivers[i];
	drv->modhandle = xf86LoadSubModule(pScrn, drv->modulename);
	if (drv->modhandle == NULL)
	    continue;

	xf86LoaderReqSymLists(drv->symbols, NULL);

	ret_ptr = NULL;
	drv->vid_rec = LoaderSymbol(drv->fntablename);

	if (drv->type & I830_DVO_CHIP_LVDS)
	    gpio = GPIOB;
	else
	    gpio = GPIOE;

	/* Set up the I2C bus necessary for the chip we're probing.  It appears
	 * that everything is on GPIOE except for panels on i830 laptops, which
	 * are on GPIOB (DVOA).
	 */
	if (gpio_inited != gpio) {
	    if (pI2CBus != NULL)
		xf86DestroyI2CBusRec(pI2CBus, TRUE, TRUE);
	    if (!I830I2CInit(pScrn, &pI2CBus, gpio,
			     gpio == GPIOB ? "DVOI2C_B" : "DVOI2C_E")) {
		continue;
	    }
	}

	if (drv->vid_rec != NULL)
	    ret_ptr = drv->vid_rec->init(pI2CBus, drv->address);

	if (ret_ptr != NULL) {
	    drv->dev_priv = ret_ptr;
	    intel_output->i2c_drv = drv;
	    intel_output->pI2CBus = pI2CBus;
	    return;
	}
	xf86UnloadSubModule(drv->modhandle);
    }

    /* Didn't find a chip, so tear down. */
    if (pI2CBus != NULL)
	xf86DestroyI2CBusRec(pI2CBus, TRUE, TRUE);
    xf86OutputDestroy (output);
}
