
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
	unsigned char val;

	/* Get the result. */
	/* Doing this on head 0 seems fine. */
	val = NVReadVGA(pNv, 0, b->DriverPrivate.uval);

	*clock = (val & DDC_SCL_READ_MASK) != 0;
	*data  = (val & DDC_SDA_READ_MASK) != 0;
}

static void
NVI2CPutBits(I2CBusPtr b, int clock, int data)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	unsigned char val;

	/* Doing this on head 0 seems fine. */
	val = NVReadVGA(pNv, 0, b->DriverPrivate.uval + 1) & 0xf0;
	if (clock)
		val |= DDC_SCL_WRITE_MASK;
	else
		val &= ~DDC_SCL_WRITE_MASK;

	if (data)
		val |= DDC_SDA_WRITE_MASK;
	else
		val &= ~DDC_SDA_WRITE_MASK;

	/* Doing this on head 0 seems fine. */
	NVWriteVGA(pNv, 0, b->DriverPrivate.uval + 1, val | 0x1);
}

Bool
NV_I2CInit(ScrnInfoPtr pScrn, I2CBusPtr *bus_ptr, int i2c_reg, char *name)
{
	I2CBusPtr pI2CBus;

	pI2CBus = xf86CreateI2CBusRec();
	if(!pI2CBus)
		return FALSE;

	pI2CBus->BusName    = name;
	pI2CBus->scrnIndex  = pScrn->scrnIndex;
	pI2CBus->I2CPutBits = NVI2CPutBits;
	pI2CBus->I2CGetBits = NVI2CGetBits;
	pI2CBus->AcknTimeout = 5;

	pI2CBus->DriverPrivate.uval = i2c_reg;

	if (!xf86I2CBusInit(pI2CBus)) {
		return FALSE;
	}

	*bus_ptr = pI2CBus;
	return TRUE;
}

