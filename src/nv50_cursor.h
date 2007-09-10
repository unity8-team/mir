#ifndef __NV50_CURSOR_H__
#define __NV50_CURSOR_H__

#ifdef ENABLE_RANDR12

Bool NV50CursorInit(ScreenPtr);
Bool NV50CursorAcquire(ScrnInfoPtr);
void NV50CursorRelease(ScrnInfoPtr);

/* CRTC cursor functions */
void NV50SetCursorPosition(xf86CrtcPtr crtc, int x, int y);
void NV50LoadCursorARGB(xf86CrtcPtr crtc, CARD32 *src);

#endif

#endif
