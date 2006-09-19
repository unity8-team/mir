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
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86RAC.h"
#include "xf86cmap.h"
#include "compiler.h"
#include "mibstore.h"
#include "vgaHW.h"
#include "mipointer.h"
#include "micmap.h"
#include "shadowfb.h"
#include <X11/extensions/randr.h>
#include "fb.h"
#include "miscstruct.h"
#include "xf86xv.h"
#include <X11/extensions/Xv.h>
#include "shadow.h"
#include "i830.h"

static void
i830I2CGetBits(I2CBusPtr b, int *clock, int *data)
{
    ScrnInfoPtr pScrn = xf86Screens[b->scrnIndex];
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 val;

    val = INREG(b->DriverPrivate.uval);
    *data = (val & GPIO_DATA_VAL_IN) != 0;
    *clock = (val & GPIO_CLOCK_VAL_IN) != 0;
}

static void
i830I2CPutBits(I2CBusPtr b, int clock, int data)
{
    ScrnInfoPtr pScrn = xf86Screens[b->scrnIndex];
    I830Ptr pI830 = I830PTR(pScrn);

    OUTREG(b->DriverPrivate.uval,
	(data ? GPIO_DATA_VAL_OUT : 0) |
	(clock ? GPIO_CLOCK_VAL_OUT : 0) |
	GPIO_CLOCK_DIR_OUT |
	GPIO_DATA_DIR_OUT |
	GPIO_CLOCK_DIR_MASK |
	GPIO_CLOCK_VAL_MASK |
	GPIO_DATA_DIR_MASK |
	GPIO_DATA_VAL_MASK);
}

/* the i830 has a number of I2C Buses */
Bool
I830I2CInit(ScrnInfoPtr pScrn, I2CBusPtr *bus_ptr, int i2c_reg, char *name)
{
    I2CBusPtr pI2CBus;

    pI2CBus = xf86CreateI2CBusRec();

    if (!pI2CBus)
	return FALSE;

    pI2CBus->BusName = name;
    pI2CBus->scrnIndex = pScrn->scrnIndex;
    pI2CBus->I2CGetBits = i830I2CGetBits;
    pI2CBus->I2CPutBits = i830I2CPutBits;
    pI2CBus->DriverPrivate.uval = i2c_reg;

    if (!xf86I2CBusInit(pI2CBus))
	return FALSE;

    *bus_ptr = pI2CBus;
    return TRUE;
}
