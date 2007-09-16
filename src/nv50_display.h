#ifndef __NV50_DISPLAY_H__
#define __NV50_DISPLAY_H__

#ifdef ENABLE_RANDR12

#include "nv50_type.h"

enum NV50ScaleMode {
	NV50_SCALE_OFF,
	NV50_SCALE_ASPECT,
	NV50_SCALE_FILL,
	NV50_SCALE_CENTER
};

Bool NV50DispPreInit(ScrnInfoPtr);
Bool NV50DispInit(ScrnInfoPtr);
void NV50DispShutdown(ScrnInfoPtr);

void NV50DispCommand(ScrnInfoPtr, CARD32 addr, CARD32 data);
#define C(mthd, data) NV50DispCommand(pScrn, (mthd), (data))

Head NV50CrtcGetHead(xf86CrtcPtr);

void NV50CrtcDoModeFixup(DisplayModePtr dst, DisplayModePtr src);
void NV50CrtcBlankScreen(xf86CrtcPtr, Bool blank);
void NV50CrtcEnableCursor(xf86CrtcPtr, Bool update);
void NV50CrtcDisableCursor(xf86CrtcPtr, Bool update);
void NV50CrtcSetCursorPosition(xf86CrtcPtr, int x, int y);
void NV50CrtcSkipModeFixup(xf86CrtcPtr);
void NV50CrtcSetDither(xf86CrtcPtr, Bool dither, Bool update);
void NV50CrtcSetScale(xf86CrtcPtr, DisplayModePtr, enum NV50ScaleMode);

void NV50DispCreateCrtcs(ScrnInfoPtr pScrn);

#endif

#endif
