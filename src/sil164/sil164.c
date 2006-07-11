/* -*- c-basic-offset: 4 -*- */
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
#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "compiler.h"
#include "miscstruct.h"
#include "xf86i2c.h"

#include "../i2c_vid.h"
#include "sil164.h"
#include "sil164_reg.h"

static void
sil164PrintRegs(I2CDevPtr d);
static void
sil164Power(I2CDevPtr d, Bool On);

static Bool
sil164ReadByte(SIL164Ptr sil, int addr, CARD8 *ch)
{
    if (!xf86I2CReadByte(&(sil->d), addr, ch)) {
	xf86DrvMsg(sil->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to read from %s Slave %d.\n",
		   sil->d.pI2CBus->BusName, sil->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}

static Bool
sil164WriteByte(SIL164Ptr sil, int addr, CARD8 ch)
{
    if (!xf86I2CWriteByte(&(sil->d), addr, ch)) {
	xf86DrvMsg(sil->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to write to %s Slave %d.\n",
		   sil->d.pI2CBus->BusName, sil->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}

/* Silicon Image 164 driver for chip on i2c bus */
static void *
sil164Detect(I2CBusPtr b, I2CSlaveAddr addr)
{
    /* this will detect the SIL164 chip on the specified i2c bus */
    SIL164Ptr sil;
    unsigned char ch;

    xf86DrvMsg(b->scrnIndex, X_ERROR, "detecting sil164\n");

    sil = xcalloc(1, sizeof(SIL164Rec));
    if (sil == NULL)
	return NULL;

    sil->d.DevName = "SIL164 TMDS Controller";
    sil->d.SlaveAddr = addr;
    sil->d.pI2CBus = b;
    sil->d.StartTimeout = b->StartTimeout;
    sil->d.BitTimeout = b->BitTimeout;
    sil->d.AcknTimeout = b->AcknTimeout;
    sil->d.ByteTimeout = b->ByteTimeout;
    sil->d.DriverPrivate.ptr = sil;

    if (!sil164ReadByte(sil, SIL164_VID_LO, &ch))
	goto out;

    if (ch!=(SIL164_VID & 0xFF)) {
	xf86DrvMsg(sil->d.pI2CBus->scrnIndex, X_ERROR,
		   "sil164 not detected got %d: from %s Slave %d.\n",
		   ch, sil->d.pI2CBus->BusName, sil->d.SlaveAddr);
	goto out;
    }

    if (!sil164ReadByte(sil, SIL164_DID_LO, &ch))
	goto out;

    if (ch!=(SIL164_DID & 0xFF)) {
	xf86DrvMsg(sil->d.pI2CBus->scrnIndex, X_ERROR,
		   "sil164 not detected got %d: from %s Slave %d.\n",
		   ch, sil->d.pI2CBus->BusName, sil->d.SlaveAddr);
	goto out;
    }

    if (!xf86I2CDevInit(&(sil->d))) {
	goto out;
    }

    return sil;

out:
    xfree(sil);
    return NULL;
}


static Bool
sil164Init(I2CDevPtr d)
{
    /* not much to do */
    return TRUE;
}

static ModeStatus
sil164ModeValid(I2CDevPtr d, DisplayModePtr mode)
{
    return MODE_OK;
}

static void
sil164Mode(I2CDevPtr d, DisplayModePtr mode)
{
    sil164Power(d, TRUE);
    sil164PrintRegs(d);

    /* recommended programming sequence from doc */
    /*sil164WriteByte(sil, 0x08, 0x30);
      sil164WriteByte(sil, 0x09, 0x00);
      sil164WriteByte(sil, 0x0a, 0x90);
      sil164WriteByte(sil, 0x0c, 0x89);
      sil164WriteByte(sil, 0x08, 0x31);*/
    /* don't do much */
    return;
}

/* set the SIL164 power state */
static void
sil164Power(I2CDevPtr d, Bool On)
{
    SIL164Ptr sil = SILPTR(d);
    int ret;
    unsigned char ch;

    ret = sil164ReadByte(sil, SIL164_REG8, &ch);
    if (ret == FALSE)
	return;

    if (On)
	ch |= SIL164_8_PD;
    else
	ch &= ~SIL164_8_PD;

    sil164WriteByte(sil, SIL164_REG8, ch);

    return;
}

static void
sil164PrintRegs(I2CDevPtr d)
{
    SIL164Ptr sil = SILPTR(d);
    CARD8 val;

    sil164ReadByte(sil, SIL164_FREQ_LO, &val);
    xf86DrvMsg(sil->d.pI2CBus->scrnIndex, X_INFO, "SIL164_FREQ_LO: 0x%02x\n",
	       val);
    sil164ReadByte(sil, SIL164_FREQ_HI, &val);
    xf86DrvMsg(sil->d.pI2CBus->scrnIndex, X_INFO, "SIL164_FREQ_HI: 0x%02x\n",
	       val);
    sil164ReadByte(sil, SIL164_REG8, &val);
    xf86DrvMsg(sil->d.pI2CBus->scrnIndex, X_INFO, "SIL164_REG8: 0x%02x\n", val);
    sil164ReadByte(sil, SIL164_REG9, &val);
    xf86DrvMsg(sil->d.pI2CBus->scrnIndex, X_INFO, "SIL164_REG9: 0x%02x\n", val);
    sil164ReadByte(sil, SIL164_REGC, &val);
    xf86DrvMsg(sil->d.pI2CBus->scrnIndex, X_INFO, "SIL164_REGC: 0x%02x\n", val);
}

static void
sil164SaveRegs(I2CDevPtr d)
{
    SIL164Ptr sil = SILPTR(d);

    if (!sil164ReadByte(sil, SIL164_FREQ_LO, &sil->SavedReg.freq_lo))
	return;

    if (!sil164ReadByte(sil, SIL164_FREQ_HI, &sil->SavedReg.freq_hi))
	return;

    if (!sil164ReadByte(sil, SIL164_REG8, &sil->SavedReg.reg8))
	return;

    if (!sil164ReadByte(sil, SIL164_REG9, &sil->SavedReg.reg9))
	return;

    if (!sil164ReadByte(sil, SIL164_REGC, &sil->SavedReg.regc))
	return;

    return;
}

I830I2CVidOutputRec SIL164VidOutput = {
    sil164Detect,
    sil164Init,
    sil164ModeValid,
    sil164Mode,
    sil164Power,
    sil164PrintRegs,
    sil164SaveRegs,
    NULL,
};
