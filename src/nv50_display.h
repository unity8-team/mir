#ifndef __NV50_DISPLAY_H__
#define __NV50_DISPLAY_H__

#include "nv50_type.h"

Bool NV50DispPreInit(ScrnInfoPtr);
Bool NV50DispInit(ScrnInfoPtr);
void NV50DispShutdown(ScrnInfoPtr);

Head NV50CrtcGetHead(xf86CrtcPtr);

void NV50CrtcBlankScreen(xf86CrtcPtr, Bool blank);
void NV50CrtcEnableCursor(xf86CrtcPtr, Bool update);
void NV50CrtcDisableCursor(xf86CrtcPtr, Bool update);
void NV50CrtcSetCursorPosition(xf86CrtcPtr, int x, int y);
void NV50CrtcSetDither(xf86CrtcPtr, Bool update);
void NV50CrtcSetScale(xf86CrtcPtr, DisplayModePtr, DisplayModePtr, enum scaling_modes);

void NV50DispCreateCrtcs(ScrnInfoPtr pScrn);
const xf86CrtcFuncsRec * nv50_get_crtc_funcs();

#endif
