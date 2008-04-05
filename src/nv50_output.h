#ifndef __NV50_OUTPUT_H__
#define __NV50_OUTPUT_H__

#include "nv_type.h"

int NV50OrOffset(xf86OutputPtr output);
void NV50OutputSetPClk(xf86OutputPtr, int pclk);
int NV50OutputModeValid(xf86OutputPtr, DisplayModePtr);
void NV50OutputPrepare(xf86OutputPtr);
void NV50OutputCommit(xf86OutputPtr);
DisplayModePtr NV50OutputGetDDCModes(xf86OutputPtr);
void NV50OutputDestroy(xf86OutputPtr);
xf86MonPtr NV50OutputGetEDID(xf86OutputPtr output, I2CBusPtr pDDCBus);

/* nv50_dac.c */
xf86OutputPtr NV50CreateDac(ScrnInfoPtr, ORNum);
Bool NV50DacLoadDetect(xf86OutputPtr);
void NV50DacSetPClk(xf86OutputPtr output, int pclk);
const xf86OutputFuncsRec * nv50_get_analog_output_funcs();

/* nv50_sor.c */
xf86OutputPtr NV50CreateSor(ScrnInfoPtr pScrn, ORNum or, NVOutputType type);
void NV50SorSetPClk(xf86OutputPtr output, int pclk);
const xf86OutputFuncsRec * nv50_get_tmds_output_funcs();
const xf86OutputFuncsRec * nv50_get_lvds_output_funcs();
DisplayModePtr GetLVDSNativeMode(ScrnInfoPtr pScrn);

#endif
