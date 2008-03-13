#ifndef __NV50_OUTPUT_H__
#define __NV50_OUTPUT_H__

#include "nv50_display.h"
#include "nv_type.h"

void NV50OutputSetPClk(xf86OutputPtr, int pclk);
int NV50OutputModeValid(xf86OutputPtr, DisplayModePtr);
void NV50OutputPrepare(xf86OutputPtr);
void NV50OutputCommit(xf86OutputPtr);
DisplayModePtr NV50OutputGetDDCModes(xf86OutputPtr);
void NV50OutputDestroy(xf86OutputPtr);
Bool NV50CreateOutputs(ScrnInfoPtr);

/* nv50_dac.c */
xf86OutputPtr NV50CreateDac(ScrnInfoPtr, ORNum);
Bool NV50DacLoadDetect(xf86OutputPtr);
void NV50DacSetPClk(xf86OutputPtr output, int pclk);

/* nv50_sor.c */
xf86OutputPtr NV50CreateSor(ScrnInfoPtr pScrn, ORNum or, NVOutputType type);
void NV50SorSetPClk(xf86OutputPtr output, int pclk);

#endif
