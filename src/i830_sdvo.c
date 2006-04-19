/**************************************************************************

 Copyright 2006 Dave Airlie <airlied@linux.ie>
 
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

#include "xf86.h"
#include "xf86_ansic.h"
#include "xf86_OSproc.h"
#include "compiler.h"
#include "i830.h"

/* SDVO support for i9xx chipsets */
static Bool sReadByte(I830SDVOPtr s, int addr, unsigned char *ch)
{
    if (!xf86I2CReadByte(&s->d, addr, ch)) {
	xf86DrvMsg(s->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to read from %s Slave %d.\n", s->d.pI2CBus->BusName,
		   s->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}

#if 0
static Bool sWriteByte(I830SDVOPtr s, int addr, unsigned char ch)
{
    if (!xf86I2CWriteByte(&s->d, addr, ch)) {
	xf86DrvMsg(s->d.pI2CBus->scrnIndex, X_ERROR,
		   "Unable to write to %s Slave %d.\n", s->d.pI2CBus->BusName,
		   s->d.SlaveAddr);
	return FALSE;
    }
    return TRUE;
}
#endif

I830SDVOPtr
I830SDVOInit(I2CBusPtr b)
{
    I830SDVOPtr sdvo;

    sdvo = xcalloc(1, sizeof(I830SDVORec));
    if (sdvo == NULL)
	return NULL;

    sdvo->d.DevName = "SDVO Controller";
    sdvo->d.SlaveAddr = 0x39 << 1;
    sdvo->d.pI2CBus = b;
    sdvo->d.StartTimeout = b->StartTimeout;
    sdvo->d.BitTimeout = b->BitTimeout;
    sdvo->d.AcknTimeout = b->AcknTimeout;
    sdvo->d.ByteTimeout = b->ByteTimeout;
    sdvo->d.DriverPrivate.ptr = sdvo;

    if (!xf86I2CDevInit(&sdvo->d))
	goto out;
    return sdvo;

out:
    xfree(sdvo);
    return NULL;
}

Bool
I830I2CDetectSDVOController(ScrnInfoPtr pScrn, int output_index)
{
    I830Ptr pI830 = I830PTR(pScrn);
    unsigned char ch[64];
    int i;
    I830SDVOPtr sdvo = pI830->output[output_index].sdvo_drv;

    if (sdvo == NULL)
	return FALSE;

    for (i=0; i<0x40; i++) {
	if (!sReadByte(sdvo, i, &ch[i]))
	    return FALSE;
    }

    pI830->output[output_index].sdvo_drv->found = 1;

    return TRUE;
}
