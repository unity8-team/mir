#ifndef __NV_CRTC_H__
#define __NV_CRTC_H__

void nv_crtc_init(ScrnInfoPtr pScrn, int crtc_num);
void NVCrtcLockUnlock(xf86CrtcPtr crtc, Bool Lock);

#endif /* __NV_CRTC_H__ */
