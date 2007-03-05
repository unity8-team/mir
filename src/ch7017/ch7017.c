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

#include <unistd.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "compiler.h"
#include "miscstruct.h"
#include "xf86i2c.h"
#include "xf86Crtc.h"
#define DPMS_SERVER
#include <X11/extensions/dpms.h>

#include "../i2c_vid.h"
#include "ch7017_reg.h"

struct ch7017_priv {
    I2CDevRec d;

    CARD8 save_hapi;
    CARD8 save_vali;
    CARD8 save_valo;
    CARD8 save_lvds_pll_vco;
    CARD8 save_feedback_div;
    CARD8 save_lvds_control_2;
    CARD8 save_outputs_enable;
    CARD8 save_lvds_power_down;
};

static void
ch7017_dump_regs(I2CDevPtr d);

static Bool
ch7017_read(struct ch7017_priv *priv, int addr, CARD8 *val)
{
    if (!xf86I2CReadByte(&priv->d, addr, val)) {
	xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to read from %s Slave %d.\n",
		   priv->d.pI2CBus->BusName, priv->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}

static Bool
ch7017_write(struct ch7017_priv *priv, int addr, CARD8 val)
{
    if (!xf86I2CWriteByte(&priv->d, addr, val)) {
	xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to write to %s Slave %d.\n",
		   priv->d.pI2CBus->BusName, priv->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}

/** Probes for a CH7017 on the given bus and slave address. */
static void *
ch7017_init(I2CBusPtr b, I2CSlaveAddr addr)
{
    struct ch7017_priv *priv;
    CARD8 val;

    xf86DrvMsg(b->scrnIndex, X_INFO, "detecting ch7017\n");

    priv = xcalloc(1, sizeof(struct ch7017_priv));
    if (priv == NULL)
	return NULL;

    priv->d.DevName = "CH7017/7018 TMDS Controller";
    priv->d.SlaveAddr = addr;
    priv->d.pI2CBus = b;
    priv->d.StartTimeout = b->StartTimeout;
    priv->d.BitTimeout = b->BitTimeout;
    priv->d.AcknTimeout = b->AcknTimeout;
    priv->d.ByteTimeout = b->ByteTimeout;
    priv->d.DriverPrivate.ptr = priv;

    if (!xf86I2CReadByte(&priv->d, CH7017_DEVICE_ID, &val))
	goto fail;

    if (val != CH7017_DEVICE_ID_VALUE && val != CH7018_DEVICE_ID_VALUE) {
	xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_ERROR,
		   "ch7017 not detected, got %d: from %s Slave %d.\n",
		   val, priv->d.pI2CBus->BusName, priv->d.SlaveAddr);
	goto fail;
    }

    if (!xf86I2CDevInit(&(priv->d)))
	goto fail;

    return priv;

fail:
    xfree(priv);
    return NULL;
}

static xf86OutputStatus
ch7017_detect(I2CDevPtr d)
{
    return XF86OutputStatusUnknown;
}

static ModeStatus
ch7017_mode_valid(I2CDevPtr d, DisplayModePtr mode)
{
    if (mode->Clock > 160000)
	return MODE_CLOCK_HIGH;

    return MODE_OK;
}

static void
ch7017_mode_set(I2CDevPtr d, DisplayModePtr mode)
{
    struct ch7017_priv *priv = d->DriverPrivate.ptr;
    CARD8 lvds_pll_feedback_div, lvds_pll_vco_control;
    CARD8 outputs_enable, lvds_control_2, lvds_power_down;
    CARD8 horizontal_active_pixel_input;
    CARD8 horizontal_active_pixel_output, vertical_active_line_output;
    CARD8 active_input_line_output;

    xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_INFO,
	       "Registers before mode setting\n");
    ch7017_dump_regs(d);

    /* LVDS PLL settings from page 75 of 7017-7017ds.pdf*/
    if (mode->Clock < 50000) {
	lvds_pll_feedback_div = 45;
	lvds_pll_vco_control = (2 << 4) | (3 << 0);
	outputs_enable = (0 << 0); /* XXX: enables */
	lvds_control_2 = (1 << CH7017_LOOP_FILTER_SHIFT) |
	    (0 << CH7017_PHASE_DETECTOR_SHIFT);
    } else if (mode->Clock < 100000) {
	lvds_pll_feedback_div = 45;
	lvds_pll_vco_control = (2 << 4) | (3 << 0);
	outputs_enable = (0 << 0); /* XXX: enables */
	lvds_control_2 = (1 << CH7017_LOOP_FILTER_SHIFT) |
	    (0 << CH7017_PHASE_DETECTOR_SHIFT);
    } else if (mode->Clock < 160000) {
	lvds_pll_feedback_div = 35;
	outputs_enable = (3 << 0); /* XXX: enables */
	lvds_control_2 = (3 << CH7017_LOOP_FILTER_SHIFT) |
	    (0 << CH7017_PHASE_DETECTOR_SHIFT);
	if (1) { /* XXX: dual panel */
	    lvds_pll_vco_control = (2 << 4) | (13 << 0);
	} else {
	    lvds_pll_vco_control = (1 << 4) | (13 << 0);
	}
    } else {
	FatalError("Invalid mode clock (%.1fMHz)\n",
		   (float)mode->Clock / 1000.0);
    }

    horizontal_active_pixel_input = mode->HDisplay & 0x00ff;

    vertical_active_line_output = mode->VDisplay & 0x00ff;
    horizontal_active_pixel_output = mode->HDisplay & 0x00ff;

    active_input_line_output = ((mode->HDisplay & 0x0700) >> 9) |
	((mode->VDisplay & 0x0700) >> 8);

    lvds_power_down = (mode->HDisplay & 0x0f00) >> 8;

    ch7017Power(d, FALSE);
    ch7017_write(priv, CH7017_HORIZONTAL_ACTIVE_PIXEL_INPUT,
		    horizontal_active_pixel_input);
    ch7017_write(priv, CH7017_HORIZONTAL_ACTIVE_PIXEL_OUTPUT,
		    horizontal_active_pixel_output);
    ch7017_write(priv, CH7017_VERTICAL_ACTIVE_LINE_OUTPUT,
		    vertical_active_line_output);
    ch7017_write(priv, CH7017_LVDS_PLL_VCO_CONTROL, lvds_pll_vco_control);
    ch7017_write(priv, CH7017_LVDS_PLL_FEEDBACK_DIV, lvds_pll_feedback_div);
    ch7017_write(priv, CH7017_LVDS_CONTROL_2, lvds_control_2);
    ch7017_write(priv, CH7017_OUTPUTS_ENABLE, outputs_enable);

    /* Turn the LVDS back on with new settings. */
    ch7017_write(priv, CH7017_LVDS_POWER_DOWN, lvds_power_down);

    xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_INFO,
	       "Registers after mode setting\n");
    ch7017PrintRegs(d);
}

/* set the CH7017 power state */
static void
ch7017_dpms(I2CDevPtr d, int mode)
{
    struct ch7017_priv *priv = d->DriverPrivate.ptr;
    CARD8 val;

    ch7017_read(priv, CH7017_LVDS_POWER_DOWN, &val);

    if (mode == DPMSModeOn) {
	/* Turn on the LVDS */
	ch7017_write(priv, CH7017_LVDS_POWER_DOWN,
			val & ~CH7017_LVDS_POWER_DOWN_EN);
    } else {
	/* Turn on the LVDS */
	ch7017_write(priv, CH7017_LVDS_POWER_DOWN,
			val | CH7017_LVDS_POWER_DOWN_EN);
    }

    /* XXX: Should actually wait for update power status somehow */
    usleep(50000);
}

static void
ch7017_dump_regs(I2CDevPtr d)
{
    struct ch7017_priv *priv = d->DriverPrivate.ptr;
    CARD8 val;

#define DUMP(reg)					\
do {							\
	ch7017_read(priv, reg, &val);			\
	xf86DrvMsg(priv->d.pI2CBus->scrnIndex, X_INFO,	\
		   #reg ": %02x\n", val);		\
} while (0)

    DUMP(CH7017_HORIZONTAL_ACTIVE_PIXEL_INPUT);
    DUMP(CH7017_HORIZONTAL_ACTIVE_PIXEL_OUTPUT);
    DUMP(CH7017_VERTICAL_ACTIVE_LINE_OUTPUT);
    DUMP(CH7017_LVDS_PLL_VCO_CONTROL);
    DUMP(CH7017_LVDS_PLL_FEEDBACK_DIV);
    DUMP(CH7017_LVDS_CONTROL_2);
    DUMP(CH7017_OUTPUTS_ENABLE);
    DUMP(CH7017_LVDS_POWER_DOWN);
}

static void
ch7017_save(I2CDevPtr d)
{
    struct ch7017_priv *priv = d->DriverPrivate.ptr;

    ch7017_read(priv, CH7017_HORIZONTAL_ACTIVE_PIXEL_INPUT, &priv->save_hapi);
    ch7017_read(priv, CH7017_VERTICAL_ACTIVE_LINE_OUTPUT, &priv->save_valo);
    ch7017_read(priv, CH7017_LVDS_PLL_VCO_CONTROL, &priv->save_lvds_pll_vco);
    ch7017_read(priv, CH7017_LVDS_PLL_FEEDBACK_DIV, &priv->save_feedback_div);
    ch7017_read(priv, CH7017_LVDS_CONTROL_2, &priv->save_lvds_control_2);
    ch7017_read(priv, CH7017_OUTPUTS_ENABLE, &priv->save_outputs_enable);
    ch7017_read(priv, CH7017_LVDS_POWER_DOWN, &priv->save_lvds_power_down);
}

static void
ch7017_restore(I2CDevPtr d)
{
    struct ch7017_priv *priv = d->DriverPrivate.ptr;

    /* Power down before changing mode */
    ch7017Power(d, FALSE);

    ch7017_write(priv, CH7017_HORIZONTAL_ACTIVE_PIXEL_INPUT, priv->save_hapi);
    ch7017_write(priv, CH7017_VERTICAL_ACTIVE_LINE_OUTPUT, priv->save_valo);
    ch7017_write(priv, CH7017_LVDS_PLL_VCO_CONTROL, priv->save_lvds_pll_vco);
    ch7017_write(priv, CH7017_LVDS_PLL_FEEDBACK_DIV, priv->save_feedback_div);
    ch7017_write(priv, CH7017_LVDS_CONTROL_2, priv->save_lvds_control_2);
    ch7017_write(priv, CH7017_OUTPUTS_ENABLE, priv->save_outputs_enable);
    ch7017_write(priv, CH7017_LVDS_POWER_DOWN, priv->save_lvds_power_down);
}

I830I2CVidOutputRec ch7017_methods = {
    .init = ch7017_init,
    .detect = ch7017_detect,
    .mode_valid = ch7017_mode_valid,
    .mode_set = ch7017_mode_set,
    .dpms = ch7017_dpms,
    .dump_regs = ch7017_dump_regs,
    .save = ch7017_save,
    .restore = ch7017_restore,
};
