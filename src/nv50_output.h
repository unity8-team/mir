#ifndef __NV50_OUTPUT_H__
#define __NV50_OUTPUT_H__

#include "nv50_display.h"
#include "nv_type.h"

typedef struct NV50OutputPrivRec {
    ORNum or;
    NVOutputType type;
    DisplayModePtr nativeMode;
    enum NV50ScaleMode scale;

    xf86OutputPtr partner;
    I2CBusPtr i2c;

    xf86OutputStatus cached_status;

    void (*set_pclk)(xf86OutputPtr, int pclk);
} NV50OutputPrivRec, *NV50OutputPrivPtr;

void NV50OutputSetPClk(xf86OutputPtr, int pclk);
int NV50OutputModeValid(xf86OutputPtr, DisplayModePtr);
void NV50OutputPrepare(xf86OutputPtr);
void NV50OutputCommit(xf86OutputPtr);
void NV50OutputPartnersDetect(xf86OutputPtr dac, xf86OutputPtr sor, I2CBusPtr i2c);
void NV50OutputResetCachedStatus(ScrnInfoPtr);
DisplayModePtr NV50OutputGetDDCModes(xf86OutputPtr);
void NV50OutputDestroy(xf86OutputPtr);
Bool NV50CreateOutputs(ScrnInfoPtr);

/* nv50_dac.c */
xf86OutputPtr NV50CreateDac(ScrnInfoPtr, ORNum);
Bool NV50DacLoadDetect(xf86OutputPtr);

/* nv50_sor.c */
xf86OutputPtr NV50CreateSor(ScrnInfoPtr pScrn, ORNum or, NVOutputType type);

#endif
