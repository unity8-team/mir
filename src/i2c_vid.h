/* this needs to go in the server */
#ifndef I2C_VID_H
#define I2C_VID_H

typedef struct _I830I2CVidOutputRec {
    void *(*init)(I2CBusPtr b, I2CSlaveAddr addr);
    xf86OutputStatus (*detect)(I2CDevPtr d);
    ModeStatus (*mode_valid)(I2CDevPtr d, DisplayModePtr mode);
    void (*mode_set)(I2CDevPtr d, DisplayModePtr mode);
    void (*dpms)(I2CDevPtr d, int mode);
    void (*dump_regs)(I2CDevPtr d);
    void (*save)(I2CDevPtr d);
    void (*restore)(I2CDevPtr d);
} I830I2CVidOutputRec, *I830I2CVidOutputPtr;

#endif
