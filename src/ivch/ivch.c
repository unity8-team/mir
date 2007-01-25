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
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "compiler.h"
#include "miscstruct.h"
#include "xf86i2c.h"
#include "../i830_xf86Crtc.h"
#define DPMS_SERVER
#include <X11/extensions/dpms.h>

#include "../i2c_vid.h"
#include "ivch_reg.h"

struct ivch_priv {
    I2CDevRec d;

    CARD16 save_VR01;
    CARD16 save_VR40;
};

static void
ivch_dump_regs(I2CDevPtr d);

/**
 * Reads a register on the ivch.
 *
 * Each of the 256 registers are 16 bits long.
 */
static Bool
ivch_read(struct ivch_priv *priv, int addr, CARD16 *data)
{
    if (!xf86I2CReadWord(&priv->d, addr, data)) {
	xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to read register 0x%02x from %s:%d.\n",
		   addr, priv->d.pI2CBus->BusName, priv->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}

/** Writes a 16-bit register on the ivch */
static Bool
ivch_write(struct ivch_priv *priv, int addr, CARD16 data)
{
    if (!xf86I2CWriteWord(&priv->d, addr, data)) {
	xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to write register 0x%02x to %s:%d.\n",
		   addr, priv->d.pI2CBus->BusName, priv->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}

/** Probes the given bus and slave address for an ivch */
static void *
ivch_init(I2CBusPtr b, I2CSlaveAddr addr)
{
    struct ivch_priv *priv;
    CARD16 temp;

    xf86DrvMsg(b->scrnIndex, X_INFO, "detecting ivch\n");

    priv = xcalloc(1, sizeof(struct ivch_priv));
    if (priv == NULL)
	return NULL;

    priv->d.DevName = "i82807aa \"ivch\" LVDS/CMOS panel controller";
    priv->d.SlaveAddr = addr;
    priv->d.pI2CBus = b;
    priv->d.StartTimeout = b->StartTimeout;
    priv->d.BitTimeout = b->BitTimeout;
    priv->d.AcknTimeout = b->AcknTimeout;
    priv->d.ByteTimeout = b->ByteTimeout;
    priv->d.DriverPrivate.ptr = priv;

    if (!xf86I2CReadWord(&priv->d, VR00, &temp))
	goto out;

    /* Since the identification bits are probably zeroes, which doesn't seem
     * very unique, check that the value in the base address field matches
     * the address it's responding on.
     */
    if ((temp & VR00_BASE_ADDRESS_MASK) != priv->d.SlaveAddr) {
	xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_ERROR,
		   "ivch detect failed due to address mismatch "
		   "(%d vs %d)\n",
		   (temp & VR00_BASE_ADDRESS_MASK), priv->d.SlaveAddr);
    }

    if (!xf86I2CDevInit(&priv->d)) {
	goto out;
    }

    return priv;

out:
    xfree(priv);
    return NULL;
}

static xf86OutputStatus
ivch_detect(I2CDevPtr d)
{
    return XF86OutputStatusUnknown;
}

static ModeStatus
ivch_mode_valid(I2CDevPtr d, DisplayModePtr mode)
{
    if (mode->Clock > 112000)
	return MODE_CLOCK_HIGH;

    return MODE_OK;
}

/** Sets the power state of the panel connected to the ivch */
static void
ivch_dpms(I2CDevPtr d, int mode)
{
    struct ivch_priv *priv = d->DriverPrivate.ptr;
    int i;
    CARD16 temp;

    /* Set the new power state of the panel. */
    if (!ivch_read(priv, VR01, &temp))
	return;

    if (mode == DPMSModeOn)
	temp |= VR01_LCD_ENABLE | VR01_DVO_ENABLE;
    else
	temp &= ~(VR01_LCD_ENABLE | VR01_DVO_ENABLE);

    ivch_write(priv, VR01, temp);

    /* Wait for the panel to make its state transition */
    for (i = 0; i < 1000; i++) {
	if (!ivch_read(priv, VR30, &temp))
	    break;

	if (((temp & VR30_PANEL_ON) != 0) == (mode == DPMSModeOn))
	    break;
    }
}

static void
ivch_mode_set(I2CDevPtr d, DisplayModePtr mode)
{
    struct ivch_priv *priv = d->DriverPrivate.ptr;

    /* Disable panel fitting for now, until we can test. */
    ivch_write(priv, VR40, 0);

    ivch_dpms(d, DPMSModeOn);

    ivch_dump_regs(d);
}

static void
ivch_dump_regs(I2CDevPtr d)
{
    struct ivch_priv *priv = d->DriverPrivate.ptr;
    CARD16 val;

    ivch_read(priv, VR00, &val);
    xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_INFO, "VR00: 0x%04x\n", val);
    ivch_read(priv, VR01, &val);
    xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_INFO, "VR01: 0x%04x\n", val);
    ivch_read(priv, VR30, &val);
    xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_INFO, "VR30: 0x%04x\n", val);
    ivch_read(priv, VR40, &val);
    xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_INFO, "VR40: 0x%04x\n", val);

}

static void
ivch_save(I2CDevPtr d)
{
    struct ivch_priv *priv = d->DriverPrivate.ptr;

    ivch_read(priv, VR01, &priv->save_VR01);
    ivch_read(priv, VR40, &priv->save_VR40);
}

static void
ivch_restore(I2CDevPtr d)
{
    struct ivch_priv *priv = d->DriverPrivate.ptr;

    ivch_write(priv, VR01, priv->save_VR01);
    ivch_write(priv, VR40, priv->save_VR40);
}


I830I2CVidOutputRec ivch_methods = {
    .init = ivch_init,
    .detect = ivch_detect,
    .mode_valid = ivch_mode_valid,
    .mode_set = ivch_mode_set,
    .dpms = ivch_dpms,
    .dump_regs = ivch_dump_regs,
    .save = ivch_save,
    .restore = ivch_restore,
};
