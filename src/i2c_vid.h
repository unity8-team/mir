/* this needs to go in the server */
#ifndef I2C_VID_H
#define I2C_VID_H

typedef struct _I830I2CVidOutputRec {
  void *(*Detect)(I2CBusPtr b, I2CSlaveAddr addr);
  Bool (*Init)(I2CDevPtr d);
  ModeStatus (*ModeValid)(I2CDevPtr d, DisplayModePtr mode);
  void (*Mode)(I2CDevPtr d, DisplayModePtr mode);
  void (*Power)(I2CDevPtr d, Bool On);
  void (*PrintRegs)(I2CDevPtr d);
  void (*SaveRegs)(I2CDevPtr d);
  void (*RestoreRegs)(I2CDevPtr d);
} I830I2CVidOutputRec, *I830I2CVidOutputPtr;

#endif
