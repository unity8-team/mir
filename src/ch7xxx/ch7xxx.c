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

static void ch7xxxSaveRegs(void *d);

static CARD8 ch7xxxFreqRegs[][7] =
  { { 0, 0x23, 0x08, 0x16, 0x30, 0x60, 0x00 },
    { 0, 0x23, 0x04, 0x26, 0x30, 0x60, 0x00 },
    { 0, 0x2D, 0x07, 0x26, 0x30, 0xE0, 0x00 } };


static Bool ch7xxxReadByte(CH7xxxPtr ch7xxx, int addr, unsigned char *ch)
{
  if (!xf86I2CReadByte(&(ch7xxx->d), addr, ch)) {
    xf86DrvMsg(ch7xxx->d.pI2CBus->scrnIndex, X_ERROR, "Unable to read from %s Slave %d.\n", ch7xxx->d.pI2CBus->BusName, ch7xxx->d.SlaveAddr);
    return FALSE;
  }
  return TRUE;
}

static Bool ch7xxxWriteByte(CH7xxxPtr ch7xxx, int addr, unsigned char ch)
{
  if (!xf86I2CWriteByte(&(ch7xxx->d), addr, ch)) {
    xf86DrvMsg(ch7xxx->d.pI2CBus->scrnIndex, X_ERROR, "Unable to write to %s Slave %d.\n", ch7xxx->d.pI2CBus->BusName, ch7xxx->d.SlaveAddr);
    return FALSE;
  }
  return TRUE;
}

/* Ch7xxxicon Image 164 driver for chip on i2c bus */
static void *ch7xxxDetect(I2CBusPtr b, I2CSlaveAddr addr)
{
  /* this will detect the CH7xxx chip on the specified i2c bus */
  CH7xxxPtr ch7xxx;
  unsigned char ch;

  xf86DrvMsg(b->scrnIndex, X_ERROR, "detecting ch7xxx\n");
  
  ch7xxx = xcalloc(1, sizeof(CH7xxxRec));
  if (ch7xxx == NULL)
    return NULL;

  ch7xxx->d.DevName = "CH7xxx TMDS Controller";
  ch7xxx->d.SlaveAddr = addr;
  ch7xxx->d.pI2CBus = b;
  ch7xxx->d.StartTimeout = b->StartTimeout;
  ch7xxx->d.BitTimeout = b->BitTimeout;
  ch7xxx->d.AcknTimeout = b->AcknTimeout;
  ch7xxx->d.ByteTimeout = b->ByteTimeout;
  ch7xxx->d.DriverPrivate.ptr = ch7xxx;

  if (!ch7xxxReadByte(ch7xxx, CH7xxx_REG_VID, &ch))
    goto out;

  ErrorF("VID is %02X", ch);
  if (ch!=(CH7xxx_VID & 0xFF))
  {
    xf86DrvMsg(ch7xxx->d.pI2CBus->scrnIndex, X_ERROR, "ch7xxx not detected got %d: from %s Slave %d.\n", ch, ch7xxx->d.pI2CBus->BusName, ch7xxx->d.SlaveAddr);
    goto out;
  }


  if (!ch7xxxReadByte(ch7xxx, CH7xxx_REG_DID, &ch))
    goto out;

  ErrorF("DID is %02X", ch);
  if (ch!=(CH7xxx_DID & 0xFF))
  {
    xf86DrvMsg(ch7xxx->d.pI2CBus->scrnIndex, X_ERROR, "ch7xxx not detected got %d: from %s Slave %d.\n", ch, ch7xxx->d.pI2CBus->BusName, ch7xxx->d.SlaveAddr);
    goto out;
  }


  if (!xf86I2CDevInit(&(ch7xxx->d)))
  {
    goto out;
  }

  return ch7xxx;
  
 out:
  xfree(ch7xxx);
  return NULL;
}


static Bool ch7xxxInit(I2CDevPtr d)
{
  CH7xxxPtr ch7xxx = CH7PTR(d);

  /* not much to do */
  return TRUE;
}

static ModeStatus ch7xxxModeValid(I2CDevPtr d, DisplayModePtr mode)
{
  CH7xxxPtr ch7xxx = CH7PTR(d);
  
  return MODE_OK;
}

static void ch7xxxMode(I2CDevPtr d, DisplayModePtr mode)
{
  CH7xxxPtr ch7xxx = CH7PTR(d);
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
    ch7xxx->ModeReg.regs[i] = freq_regs[i - 0x31];
    ch7xxxWriteByte(ch7xxx, i, ch7xxx->ModeReg.regs[i]);
  }
    
#if 0

  xf86DrvMsg(ch7xxx->d.pI2CBus->scrnIndex, X_ERROR, "ch7xxx idf is 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", idf, tpcp, tpd, tpf);

  xf86DrvMsg(ch7xxx->d.pI2CBus->scrnIndex, X_ERROR, "ch7xxx pm is %02X\n", pm);

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

  //  cm |= 1;

  ch7xxxWriteByte(ch7xxx, CH7xxx_CM, cm);
  ch7xxxWriteByte(ch7xxx, CH7xxx_TPCP, tpcp);
  ch7xxxWriteByte(ch7xxx, CH7xxx_TPD, tpd);
  ch7xxxWriteByte(ch7xxx, CH7xxx_TPF, tpf);
  ch7xxxWriteByte(ch7xxx, CH7xxx_TPF, idf);
  ch7xxxWriteByte(ch7xxx, CH7xxx_PM, pm);

#endif
  /* don't do much */
  return;
}

/* set the CH7xxx power state */
static void ch7xxxPower(I2CDevPtr d, Bool On)
{
  CH7xxxPtr ch7xxx = CH7PTR(d);
  int ret;
  unsigned char ch;


  ret = ch7xxxReadByte(ch7xxx, CH7xxx_PM, &ch);
  if (ret == FALSE)
    return;
  
  xf86DrvMsg(ch7xxx->d.pI2CBus->scrnIndex, X_ERROR, "ch7xxx pm is %02X\n", ch);
  
#if 0  
  ret = ch7xxxReadByte(ch7xxx, CH7xxx_REG8, &ch);
  if (ret)
    return;

  if (On)
    ch |= CH7xxx_8_PD;
  else
    ch &= ~CH7xxx_8_PD;

  ch7xxxWriteByte(ch7xxx, CH7xxx_REG8, ch);
#endif
  return;
}

static void ch7xxxPrintRegs(I2CDevPtr d)
{
  CH7xxxPtr ch7xxx = CH7PTR(d);
  int i;

  ch7xxxSaveRegs(d);

  for (i = 0; i < CH7xxx_NUM_REGS; i++) {
    if (( i % 8 ) == 0 )
      ErrorF("\n %02X: ", i);
    ErrorF("%02X ", ch7xxx->ModeReg.regs[i]);

  }
}

static void ch7xxxSaveRegs(void *d)
{
  CH7xxxPtr ch7xxx = CH7PTR(((I2CDevPtr)d));
  int ret;
  int i;

  for (i = 0; i < CH7xxx_NUM_REGS; i++) {
    ret = ch7xxxReadByte(ch7xxx, i, &ch7xxx->SavedReg.regs[i]);
    if (ret == FALSE)
      break;
  }

  memcpy(ch7xxx->ModeReg.regs, ch7xxx->SavedReg.regs, CH7xxx_NUM_REGS);

  return;
}

I830I2CVidOutputRec CH7xxxVidOutput = {
  ch7xxxDetect,
  ch7xxxInit,
  ch7xxxModeValid,
  ch7xxxMode,
  ch7xxxPower,
  ch7xxxPrintRegs,
  ch7xxxSaveRegs,
  NULL,
};
