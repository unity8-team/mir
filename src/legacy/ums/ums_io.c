/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
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
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i810_io.c,v 1.4 2002/01/25 21:56:04 tsi Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

/*
 * Authors:
 *   Daryll Strauss <daryll@precisioninsight.com>
 *
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "compiler.h"

#include "ums.h"

#define minb(p) *(volatile uint8_t *)(pI830->MMIOBase + (p))
#define moutb(p,v) *(volatile uint8_t *)(pI830->MMIOBase + (p)) = (v)

static void
I830WriteControlPIO(I830Ptr pI830, IOADDRESS addr, uint8_t index, uint8_t val)
{
   addr += pI830->ioBase;
   outb(addr, index);
   outb(addr + 1, val);
}

static uint8_t
I830ReadControlPIO(I830Ptr pI830, IOADDRESS addr, uint8_t index)
{
   addr += pI830->ioBase;
   outb(addr, index);
   return inb(addr + 1);
}

static void
I830WriteStandardPIO(I830Ptr pI830, IOADDRESS addr, uint8_t val)
{
   outb(pI830->ioBase + addr, val);
}

static uint8_t
I830ReadStandardPIO(I830Ptr pI830, IOADDRESS addr)
{
   return inb(pI830->ioBase + addr);
}

void
ums_I830SetPIOAccess(I830Ptr pI830)
{
   pI830->writeControl = I830WriteControlPIO;
   pI830->readControl = I830ReadControlPIO;
   pI830->writeStandard = I830WriteStandardPIO;
   pI830->readStandard = I830ReadStandardPIO;
}

static void
I830WriteControlMMIO(I830Ptr pI830, IOADDRESS addr, uint8_t index, uint8_t val)
{
   moutb(addr, index);
   moutb(addr + 1, val);
}

static uint8_t
I830ReadControlMMIO(I830Ptr pI830, IOADDRESS addr, uint8_t index)
{
   moutb(addr, index);
   return minb(addr + 1);
}

static void
I830WriteStandardMMIO(I830Ptr pI830, IOADDRESS addr, uint8_t val)
{
   moutb(addr, val);
}

static uint8_t
I830ReadStandardMMIO(I830Ptr pI830, IOADDRESS addr)
{
   return minb(addr);
}

void
ums_I830SetMMIOAccess(I830Ptr pI830)
{
   pI830->writeControl = I830WriteControlMMIO;
   pI830->readControl = I830ReadControlMMIO;
   pI830->writeStandard = I830WriteStandardMMIO;
   pI830->readStandard = I830ReadStandardMMIO;
}
