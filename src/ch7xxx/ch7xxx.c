/**************************************************************************

Copyright © 2006 Dave Airlie

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
#include <string.h>
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "compiler.h"
#include "miscstruct.h"
#include "xf86i2c.h"

#include "../i2c_vid.h"
#include "ch7xxx.h"
#include "ch7xxx_reg.h"

/** @file
 * driver for the Chrontel 7xxx DVI chip over DVO.
 */

struct ch7xxx_reg_state {
    CARD8 regs[CH7xxx_NUM_REGS];
};

struct ch7xxx_priv {
    I2CDevRec d;
    struct ch7xxx_reg_state SavedReg;
    struct ch7xxx_reg_state ModeReg;
};

static void ch7xxx_save(I2CDevPtr d);

static CARD8 ch7xxxFreqRegs[][7] =
  { { 0, 0x23, 0x08, 0x16, 0x30, 0x60, 0x00 },
    { 0, 0x23, 0x04, 0x26, 0x30, 0x60, 0x00 },
    { 0, 0x2D, 0x07, 0x26, 0x30, 0xE0, 0x00 } };

/** Reads an 8 bit register */
static Bool
ch7xxx_read(struct ch7xxx_priv *dev_priv, int addr, unsigned char *ch)
{
    if (!xf86I2CReadByte(&dev_priv->d, addr, ch)) {
	xf86DrvMsg(dev_priv->d.pI2CBus->scrnIndex,
		   X_ERROR, "Unable to read from %s Slave %d.\n",
		   dev_priv->d.pI2CBus->BusName, dev_priv->d.SlaveAddr);
	return FALSE;
    }

    return TRUE;
}

/** Writes an 8 bit register */
static Bool
ch7xxx_write(struct ch7xxx_priv *dev_priv, int addr, unsigned char ch)
{
    if (!xf86I2CWriteByte(&dev_priv->d, addr, ch)) {
	xf86DrvMsg(dev_priv->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to write to %s Slave %d.\n",
		   dev_priv->d.pI2CBus->BusName, dev_priv->d.SlaveAddr);
	return FALSE;
    }

    return TRUE;
}

static void *
ch7xxx_probe(I2CBusPtr b, I2CSlaveAddr addr)
{
    /* this will detect the CH7xxx chip on the specified i2c bus */
    struct ch7xxx_priv *dev_priv;
    unsigned char ch;

    xf86DrvMsg(b->scrnIndex, X_INFO, "detecting ch7xxx\n");

    dev_priv = xcalloc(1, sizeof(struct ch7xxx_priv));
    if (dev_priv == NULL)
	return NULL;

    dev_priv->d.DevName = "CH7xxx TMDS Controller";
    dev_priv->d.SlaveAddr = addr;
    dev_priv->d.pI2CBus = b;
    dev_priv->d.StartTimeout = b->StartTimeout;
    dev_priv->d.BitTimeout = b->BitTimeout;
    dev_priv->d.AcknTimeout = b->AcknTimeout;
    dev_priv->d.ByteTimeout = b->ByteTimeout;
    dev_priv->d.DriverPrivate.ptr = dev_priv;

    if (!ch7xxx_read(dev_priv, CH7xxx_REG_VID, &ch))
	goto out;

    ErrorF("VID is %02X", ch);
    if (ch!=(CH7xxx_VID & 0xFF)) {
	xf86DrvMsg(dev_priv->d.pI2CBus->scrnIndex, X_ERROR,
		   "ch7xxx not detected got %d: from %s Slave %d.\n",
		   ch, dev_priv->d.pI2CBus->BusName, dev_priv->d.SlaveAddr);
	goto out;
    }


    if (!ch7xxx_read(dev_priv, CH7xxx_REG_DID, &ch))
	goto out;

    ErrorF("DID is %02X", ch);
    if (ch!=(CH7xxx_DID & 0xFF)) {
	xf86DrvMsg(dev_priv->d.pI2CBus->scrnIndex, X_ERROR,
		   "ch7xxx not detected got %d: from %s Slave %d.\n",
		   ch, dev_priv->d.pI2CBus->BusName, dev_priv->d.SlaveAddr);
	goto out;
    }


    if (!xf86I2CDevInit(&dev_priv->d)) {
	goto out;
    }

    return dev_priv;

out:
    xfree(dev_priv);
    return NULL;
}


static Bool
ch7xxx_init(I2CDevPtr d)
{
    /* not much to do */
    return TRUE;
}

static ModeStatus
ch7xxx_mode_valid(I2CDevPtr d, DisplayModePtr mode)
{
    return MODE_OK;
}

static void
ch7xxx_mode_set(I2CDevPtr d, DisplayModePtr mode)
{
    struct ch7xxx_priv *dev_priv = d->DriverPrivate.ptr;
    int ret;
    unsigned char pm, idf;
    unsigned char tpcp, tpd, tpf, cm;
    CARD8 *freq_regs;
    int i;

    ErrorF("Clock is %d\n", mode->Clock);

    if (mode->Clock < 75000)
	freq_regs = ch7xxxFreqRegs[0];
    else if (mode->Clock < 125000)
	freq_regs = ch7xxxFreqRegs[1];
    else
	freq_regs = ch7xxxFreqRegs[2];

    for (i = 0x31; i < 0x37; i++) {
	dev_priv->ModeReg.regs[i] = freq_regs[i - 0x31];
	ch7xxx_write(dev_priv, i, dev_priv->ModeReg.regs[i]);
    }

#if 0
    xf86DrvMsg(dev_priv->d.pI2CBus->scrnIndex, X_ERROR,
	       "ch7xxx idf is 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
	       idf, tpcp, tpd, tpf);

    xf86DrvMsg(dev_priv->d.pI2CBus->scrnIndex, X_ERROR,
	       "ch7xxx pm is %02X\n", pm);

    if (mode->Clock < 65000) {
	tpcp = 0x08;
	tpd = 0x16;
	tpf = 0x60;
    } else {
	tpcp = 0x06;
	tpd = 0x26;
	tpf = 0xa0;
    }

    idf &= ~(CH7xxx_IDF_HSP | CH7xxx_IDF_VSP);
    if (mode->Flags & V_PHSYNC)
	idf |= CH7xxx_IDF_HSP;

    if (mode->Flags & V_PVSYNC)
	idf |= CH7xxx_IDF_HSP;

    /* setup PM Registers */
    pm &= ~CH7xxx_PM_FPD;
    pm |= CH7xxx_PM_DVIL | CH7xxx_PM_DVIP;

    /* cm |= 1; */

    ch7xxx_write(dev_priv, CH7xxx_CM, cm);
    ch7xxx_write(dev_priv, CH7xxx_TPCP, tpcp);
    ch7xxx_write(dev_priv, CH7xxx_TPD, tpd);
    ch7xxx_write(dev_priv, CH7xxx_TPF, tpf);
    ch7xxx_write(dev_priv, CH7xxx_TPF, idf);
    ch7xxx_write(dev_priv, CH7xxx_PM, pm);
#endif
}

/* set the CH7xxx power state */
static void
ch7xxx_power(I2CDevPtr d, Bool On)
{
    struct ch7xxx_priv *dev_priv = d->DriverPrivate.ptr;
    int ret;
    unsigned char ch;

    ret = ch7xxx_read(dev_priv, CH7xxx_PM, &ch);
    if (ret == FALSE)
	return;

    xf86DrvMsg(dev_priv->d.pI2CBus->scrnIndex, X_ERROR,
	       "ch7xxx pm is %02X\n", ch);

#if 0
    ret = ch7xxx_read(dev_priv, CH7xxx_REG8, &ch);
    if (ret)
	return;

    if (On)
	ch |= CH7xxx_8_PD;
    else
	ch &= ~CH7xxx_8_PD;

    ch7xxx_write(dev_priv, CH7xxx_REG8, ch);
#endif
}

static void
ch7xxx_dump_regs(I2CDevPtr d)
{
    struct ch7xxx_priv *dev_priv = d->DriverPrivate.ptr;
    int i;

    for (i = 0; i < CH7xxx_NUM_REGS; i++) {
	if (( i % 8 ) == 0 )
	    ErrorF("\n %02X: ", i);
	ErrorF("%02X ", dev_priv->ModeReg.regs[i]);
    }
}

static void
ch7xxx_save(I2CDevPtr d)
{
    struct ch7xxx_priv *dev_priv = d->DriverPrivate.ptr;
    int ret;
    int i;

    for (i = 0; i < CH7xxx_NUM_REGS; i++) {
	ret = ch7xxx_read(dev_priv, i, &dev_priv->SavedReg.regs[i]);
	if (ret == FALSE)
	    break;
    }

    memcpy(dev_priv->ModeReg.regs, dev_priv->SavedReg.regs, CH7xxx_NUM_REGS);

    return;
}

I830I2CVidOutputRec CH7xxxVidOutput = {
    ch7xxx_probe,
    ch7xxx_init,
    ch7xxx_mode_valid,
    ch7xxx_mode_set,
    ch7xxx_power,
    ch7xxx_dump_regs,
    ch7xxx_save,
    NULL,
};
