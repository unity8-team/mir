#ifndef __NV_PROTO_H__
#define __NV_PROTO_H__

/* in drmmode_display.c */
Bool drmmode_pre_init(ScrnInfoPtr pScrn, int fd, int cpp);
void drmmode_adjust_frame(ScrnInfoPtr pScrn, int x, int y, int flags);
void drmmode_remove_fb(ScrnInfoPtr pScrn);
Bool drmmode_cursor_init(ScreenPtr pScreen);
void drmmode_fbcon_copy(ScrnInfoPtr pScrn);

/* in nv_accel_common.c */
Bool NVAccelCommonInit(ScrnInfoPtr pScrn);
Bool NVAccelGetCtxSurf2DFormatFromPixmap(PixmapPtr pPix, int *fmt_ret);
Bool NVAccelGetCtxSurf2DFormatFromPicture(PicturePtr pPix, int *fmt_ret);
PixmapPtr NVGetDrawablePixmap(DrawablePtr pDraw);
void NVAccelFree(ScrnInfoPtr pScrn);

/* in nouveau_dri2.c */
Bool nouveau_dri2_init(ScreenPtr pScreen);
void nouveau_dri2_fini(ScreenPtr pScreen);

/* in nouveau_xv.c */
void NVInitVideo(ScreenPtr);
void NVTakedownVideo(ScrnInfoPtr);
void NVWaitVSync(ScrnInfoPtr pScrn, int crtc);
void NVSetPortDefaults (ScrnInfoPtr pScrn, NVPortPrivPtr pPriv);
unsigned int nv_window_belongs_to_crtc(ScrnInfoPtr, int, int, int, int);

/* in nv_dma.c */
void  NVSync(ScrnInfoPtr pScrn);
Bool  NVInitDma(ScrnInfoPtr pScrn);
void  NVTakedownDma(ScrnInfoPtr pScrn);

/* in nouveau_exa.c */
Bool nouveau_exa_init(ScreenPtr pScreen);
Bool nouveau_exa_pixmap_is_onscreen(PixmapPtr pPixmap);
bool nv50_style_tiled_pixmap(PixmapPtr ppix);

/* in nouveau_wfb.c */
void nouveau_wfb_setup_wrap(ReadMemoryProcPtr *, WriteMemoryProcPtr *,
			    DrawablePtr);
void nouveau_wfb_finish_wrap(DrawablePtr);
void nouveau_wfb_init();

/* in nv_shadow.c */
void NVRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);

/* in nv04_video_overlay.c */
void NV04PutOverlayImage(ScrnInfoPtr, struct nouveau_bo *, int, int, int,
			 BoxPtr, int, int, int, int, short, short, short,
			 short, short, short, RegionPtr clipBoxes);
int NV04SetOverlayPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
int NV04GetOverlayPortAttribute(ScrnInfoPtr, Atom, INT32 *, pointer);
void NV04StopOverlay(ScrnInfoPtr);

/* in nv04_video_blitter.c */
Bool NVPutBlitImage(ScrnInfoPtr, struct nouveau_bo *, int, int, int, BoxPtr,
		    int, int, int, int, short, short, short, short, short,
		    short, RegionPtr, PixmapPtr);
int NVSetBlitPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
int NVGetBlitPortAttribute(ScrnInfoPtr, Atom, INT32 *, pointer);
void NVStopBlitVideo(ScrnInfoPtr, pointer, Bool);

/* in nv04_exa.c */
Bool NV04EXAPrepareSolid(PixmapPtr, int, Pixel, Pixel);
void NV04EXASolid(PixmapPtr, int, int, int, int);
void NV04EXADoneSolid(PixmapPtr);
Bool NV04EXAPrepareCopy(PixmapPtr, PixmapPtr, int, int, int, Pixel);
void NV04EXACopy(PixmapPtr, int, int, int, int, int, int);
void NV04EXADoneCopy(PixmapPtr);
Bool NV04EXAUploadIFC(ScrnInfoPtr, const char *src, int src_pitch,
		      PixmapPtr pdPix, int x, int y, int w, int h, int cpp);

/* in nv10_exa.c */
Bool NVAccelInitNV10TCL(ScrnInfoPtr pScrn);
Bool NV10EXACheckComposite(int, PicturePtr, PicturePtr, PicturePtr);
Bool NV10EXAPrepareComposite(int, PicturePtr, PicturePtr, PicturePtr,
			     PixmapPtr, PixmapPtr, PixmapPtr);
void NV10EXAComposite(PixmapPtr, int, int, int, int, int, int, int, int);
void NV10EXADoneComposite(PixmapPtr);

/* in nv10_video_overlay.c */
void NV10PutOverlayImage(ScrnInfoPtr, struct nouveau_bo *, int, int, int, int,
			 BoxPtr, int, int, int, int, short, short, short,
			 short, short, short, RegionPtr clipBoxes);
int NV10SetOverlayPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
int NV10GetOverlayPortAttribute(ScrnInfoPtr, Atom, INT32 *, pointer);
void NV10StopOverlay(ScrnInfoPtr);
void NV10WriteOverlayParameters(ScrnInfoPtr);

/* in nv30_exa.c */
Bool NVAccelInitNV30TCL(ScrnInfoPtr pScrn);
Bool NV30EXACheckComposite(int, PicturePtr, PicturePtr, PicturePtr);
Bool NV30EXAPrepareComposite(int, PicturePtr, PicturePtr, PicturePtr,
				  PixmapPtr, PixmapPtr, PixmapPtr);
void NV30EXAComposite(PixmapPtr, int, int, int, int, int, int, int, int);
void NV30EXADoneComposite(PixmapPtr);

/* in nv30_video_texture.c */
int NV30PutTextureImage(ScrnInfoPtr, struct nouveau_bo *, int, int, int, int,
			BoxPtr, int, int, int, int, uint16_t, uint16_t,
			uint16_t, uint16_t, uint16_t, uint16_t,
			RegionPtr, PixmapPtr, NVPortPrivPtr);
void NV30StopTexturedVideo(ScrnInfoPtr, pointer, Bool);
int NV30GetTexturePortAttribute(ScrnInfoPtr, Atom, INT32 *, pointer);
int NV30SetTexturePortAttribute(ScrnInfoPtr, Atom, INT32, pointer);

/* in nv40_exa.c */
Bool NVAccelInitNV40TCL(ScrnInfoPtr pScrn);
Bool NV40EXACheckComposite(int, PicturePtr, PicturePtr, PicturePtr);
Bool NV40EXAPrepareComposite(int, PicturePtr, PicturePtr, PicturePtr,
				  PixmapPtr, PixmapPtr, PixmapPtr);
void NV40EXAComposite(PixmapPtr, int, int, int, int, int, int, int, int);
void NV40EXADoneComposite(PixmapPtr);

/* in nv40_video_texture.c */
int NV40PutTextureImage(ScrnInfoPtr, struct nouveau_bo *, int, int, int, int,
			BoxPtr, int, int, int, int, uint16_t, uint16_t,
			uint16_t, uint16_t, uint16_t, uint16_t,
			RegionPtr, PixmapPtr, NVPortPrivPtr);
void NV40StopTexturedVideo(ScrnInfoPtr, pointer, Bool);
int NV40GetTexturePortAttribute(ScrnInfoPtr, Atom, INT32 *, pointer);
int NV40SetTexturePortAttribute(ScrnInfoPtr, Atom, INT32, pointer);

/* in nv50_accel.c */
Bool NVAccelInitNV50TCL(ScrnInfoPtr pScrn);

/* in nv50_exa.c */
Bool NV50EXAPrepareSolid(PixmapPtr, int, Pixel, Pixel);
void NV50EXASolid(PixmapPtr, int, int, int, int);
void NV50EXADoneSolid(PixmapPtr);
Bool NV50EXAPrepareCopy(PixmapPtr, PixmapPtr, int, int, int, Pixel);
void NV50EXACopy(PixmapPtr, int, int, int, int, int, int);
void NV50EXADoneCopy(PixmapPtr);
Bool NV50EXACheckComposite(int, PicturePtr, PicturePtr, PicturePtr);
Bool NV50EXAPrepareComposite(int, PicturePtr, PicturePtr, PicturePtr,
				  PixmapPtr, PixmapPtr, PixmapPtr);
void NV50EXAComposite(PixmapPtr, int, int, int, int, int, int, int, int);
void NV50EXADoneComposite(PixmapPtr);
Bool NV50EXAUploadSIFC(const char *src, int src_pitch,
		       PixmapPtr pdPix, int x, int y, int w, int h, int cpp);

/* nv50_xv.c */
int nv50_xv_image_put(ScrnInfoPtr, struct nouveau_bo *, int, int, int, int,
		      BoxPtr, int, int, int, int, uint16_t, uint16_t,
		      uint16_t, uint16_t, uint16_t, uint16_t,
		      RegionPtr, PixmapPtr, NVPortPrivPtr);
void nv50_xv_video_stop(ScrnInfoPtr, pointer, Bool);
int nv50_xv_port_attribute_set(ScrnInfoPtr, Atom, INT32, pointer);
int nv50_xv_port_attribute_get(ScrnInfoPtr, Atom, INT32 *, pointer);

/* To support EXA 2.0, 2.1 has this in the header */
#ifndef exaMoveInPixmap
extern void exaMoveInPixmap(PixmapPtr pPixmap);
#endif

extern Bool wfbPictureInit(ScreenPtr, PictFormatPtr, int);

#endif /* __NV_PROTO_H__ */

