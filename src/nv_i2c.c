
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
	val = NVReadVgaCrtc(pNv, 0, b->DriverPrivate.uval);

	*clock = (val & DDC_SCL_READ_MASK) != 0;
	*data  = (val & DDC_SDA_READ_MASK) != 0;
}

static void
NVI2CPutBits(I2CBusPtr b, int clock, int data)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	unsigned char val;

	/* Doing this on head 0 seems fine. */
	val = NVReadVgaCrtc(pNv, 0, b->DriverPrivate.uval + 1) & 0xf0;
	if (clock)
		val |= DDC_SCL_WRITE_MASK;
	else
		val &= ~DDC_SCL_WRITE_MASK;

	if (data)
		val |= DDC_SDA_WRITE_MASK;
	else
		val &= ~DDC_SDA_WRITE_MASK;

	/* Doing this on head 0 seems fine. */
	NVWriteVgaCrtc(pNv, 0, b->DriverPrivate.uval + 1, val | 0x1);
}

/* This is a duplicate of the xorg function, plus an extra register write. */
static Bool
NV50_I2CStart(I2CBusPtr b, int timeout)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	const int off = b->DriverPrivate.val * 0x18;

	NVWrite(pNv, (0x0000E138+off)/4, NV50_I2C_START);

	if (pNv->I2CStart)
		return pNv->I2CStart(b, timeout);
	else
		xf86DrvMsg(b->scrnIndex, X_ERROR, "We're lacking a pNv->I2CStart pointer.\n");

	return FALSE;
}

/* This is a duplicate of the xorg function, plus an extra register write. */
static void
NV50_I2CStop(I2CDevPtr d)
{
	I2CBusPtr b = d->pI2CBus;
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);

	const int off = b->DriverPrivate.val * 0x18;

	if (pNv->I2CStop)
		pNv->I2CStop(d);
	else
		xf86DrvMsg(b->scrnIndex, X_ERROR, "We're lacking a pNv->I2CStop pointer.\n");

	NVWrite(pNv, (0x0000E138+off)/4, NV50_I2C_STOP);
}

static void NV50_I2CPutBits(I2CBusPtr b, int clock, int data)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	const int off = b->DriverPrivate.val * 0x18;

	NVWrite(pNv, (0x0000E138+off)/4, (4 | clock | data << 1));
}

static void NV50_I2CGetBits(I2CBusPtr b, int *clock, int *data)
{
	NVPtr pNv = NVPTR(xf86Screens[b->scrnIndex]);
	const int off = b->DriverPrivate.val * 0x18;
	unsigned char val;

	val = NVRead(pNv, (0x0000E138+off)/4);
	*clock = !!(val & 1);
	*data = !!(val & 2);
}

Bool
NV_I2CInit(ScrnInfoPtr pScrn, I2CBusPtr *bus_ptr, int i2c_reg, char *name)
{
	I2CBusPtr pI2CBus;
	NVPtr pNv = NVPTR(pScrn);

	pI2CBus = xf86CreateI2CBusRec();
	if(!pI2CBus)
		return FALSE;

	pI2CBus->BusName    = name;
	pI2CBus->scrnIndex  = pScrn->scrnIndex;
	if (pNv->Architecture == NV_ARCH_50) {
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

	pI2CBus->DriverPrivate.uval = i2c_reg;

	if (!xf86I2CBusInit(pI2CBus)) {
		return FALSE;
	}

	/* This is to avoid code duplication, so we can wrap the start and stop function. */
	if (pNv->Architecture == NV_ARCH_50) {
		if (!pNv->I2CStart || !pNv->I2CStop) {
			pNv->I2CStart = pI2CBus->I2CStart;
			pNv->I2CStop = pI2CBus->I2CStop;
		}
		pI2CBus->I2CStart = NV50_I2CStart;
		pI2CBus->I2CStop = NV50_I2CStop;
	}

	*bus_ptr = pI2CBus;
	return TRUE;
}

