/*
 * Copyright 2006 Stephane Marchesin
 * Copyright 2007 Maarten Maathuis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "nv_include.h"

/*
 * DDC1 support only requires DDC_SDA_MASK,
 * DDC2 support requires DDC_SDA_MASK and DDC_SCL_MASK
 */
#define DDC_SDA_READ_MASK  (1 << 3)
#define DDC_SCL_READ_MASK  (1 << 2)
#define DDC_SDA_WRITE_MASK (1 << 4)
#define DDC_SCL_WRITE_MASK (1 << 5)

static void
NVI2CGetBits(I2CBusPtr b, int *clock, int *data)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	struct dcb_i2c_entry *dcb_i2c = b->DriverPrivate.ptr;
	unsigned char val;

	/* Get the result. */
	/* Doing this on head 0 seems fine. */
	if (dcb_i2c->port_type == 4)	/* C51 */
		val = NVReadCRTC(pNv, 0, 0x600800 + dcb_i2c->read) >> 16;
	else
		val = NVReadVgaCrtc(pNv, 0, dcb_i2c->read);

	*clock = (val & DDC_SCL_READ_MASK) != 0;
	*data  = (val & DDC_SDA_READ_MASK) != 0;
}

static void
NVI2CPutBits(I2CBusPtr b, int clock, int data)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	struct dcb_i2c_entry *dcb_i2c = b->DriverPrivate.ptr;
	uint32_t val;

	/* Doing this on head 0 seems fine. */
	if (dcb_i2c->port_type == 4)	/* C51 */
		val = NVReadCRTC(pNv, 0, 0x600800 + dcb_i2c->write);
	else
		val = NVReadVgaCrtc(pNv, 0, dcb_i2c->write);

	val = (val & ~0xf) | 1;

	if (clock)
		val |= DDC_SCL_WRITE_MASK;
	else
		val &= ~DDC_SCL_WRITE_MASK;

	if (data)
		val |= DDC_SDA_WRITE_MASK;
	else
		val &= ~DDC_SDA_WRITE_MASK;

	/* Doing this on head 0 seems fine. */
	if (dcb_i2c->port_type == 4)	/* C51 */
		NVWriteCRTC(pNv, 0, 0x600800 + dcb_i2c->write, val);
	else
		NVWriteVgaCrtc(pNv, 0, dcb_i2c->write, val);
}

static uint32_t NV50_GetI2CPort(ScrnInfoPtr pScrn, int index)
{
	NVPtr pNv = NVPTR(pScrn);

	if (index <= 3)
		return 0xe138 + (index * 24);

	/* I have my doubts that this is 100% correct everywhere,
	 * but this is the best guess based on the data we have.
	 */
	if (pNv->NVArch >= 0x90) /* 0x90, 0xA0 */
		return 0xe1d4 + (index * 32);
	return 0xe1e0 + (index * 24);
}

static void NV50_I2CPutBits(I2CBusPtr b, int clock, int data)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	struct dcb_i2c_entry *dcb_i2c = b->DriverPrivate.ptr;

	NVWrite(pNv, NV50_GetI2CPort(xf86Screens[b->scrnIndex], dcb_i2c->write), (4 | clock | data << 1));
}

static void NV50_I2CGetBits(I2CBusPtr b, int *clock, int *data)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	struct dcb_i2c_entry *dcb_i2c = b->DriverPrivate.ptr;
	unsigned char val;

	val = NVRead(pNv, NV50_GetI2CPort(xf86Screens[b->scrnIndex], dcb_i2c->read));
	*clock = !!(val & 1);
	*data = !!(val & 2);
}

int
NV_I2CInit(ScrnInfoPtr pScrn, I2CBusPtr *bus_ptr, struct dcb_i2c_entry *dcb_i2c, char *name)
{
	I2CBusPtr pI2CBus;

	if (dcb_i2c->chan)
		goto initialized;

	if (!(pI2CBus = xf86CreateI2CBusRec()))
		return -ENOMEM;

	pI2CBus->BusName    = name;
	pI2CBus->scrnIndex  = pScrn->scrnIndex;
	if (dcb_i2c->port_type == 5) {	/* NV50 */
		pI2CBus->I2CPutBits = NV50_I2CPutBits;
		pI2CBus->I2CGetBits = NV50_I2CGetBits;
		/* Could this be used for the rest as well? */
		pI2CBus->ByteTimeout = 2200; /* VESA DDC spec 3 p. 43 (+10 %) */
		pI2CBus->StartTimeout = 550;
		pI2CBus->BitTimeout = 40;
		pI2CBus->ByteTimeout = 40;
		pI2CBus->AcknTimeout = 40;
	} else {
		pI2CBus->I2CPutBits = NVI2CPutBits;
		pI2CBus->I2CGetBits = NVI2CGetBits;
		pI2CBus->AcknTimeout = 5;
	}
	pI2CBus->DriverPrivate.ptr = dcb_i2c;

	if (!xf86I2CBusInit(pI2CBus))
		return -EINVAL;

	dcb_i2c->chan = pI2CBus;

initialized:
	*bus_ptr = dcb_i2c->chan;

	return 0;
}

