#ifndef __THEATRE_H__
#define __THEATRE_H__

#define MODE_UNINITIALIZED		1
#define MODE_INITIALIZATION_IN_PROGRESS 2
#define MODE_INITIALIZED_FOR_TV_IN	3

typedef struct {
         GENERIC_BUS_Ptr VIP;
	 
	 int theatre_num;
	 CARD32 theatre_id;
	 int  mode;
	 char* microc_path;
	 char* microc_type;
	 
	 CARD16 video_decoder_type;
	 CARD32 wStandard;
	 CARD32 wConnector;
	 int    iHue;
	 int    iSaturation;
	 CARD32 wSaturation_U;
	 CARD32 wSaturation_V;
	 int    iBrightness;
	 int    dbBrightnessRatio;
	 CARD32 wSharpness;
	 int    iContrast;
	 int    dbContrast;
	 CARD32 wInterlaced;
	 CARD32 wTunerConnector;
	 CARD32 wComp0Connector;
	 CARD32 wSVideo0Connector;
	 CARD32 dwHorzScalingRatio;
	 CARD32 dwVertScalingRatio;
	 
	 } TheatreRec, * TheatrePtr;


/* DO NOT FORGET to setup constants before calling InitTheatre */
#define xf86_InitTheatre		InitTheatre
_X_EXPORT void InitTheatre(TheatrePtr t);
#define xf86_RT_SetTint			RT_SetTint
_X_EXPORT void RT_SetTint (TheatrePtr t, int hue);
#define xf86_RT_SetSaturation		RT_SetSaturation
_X_EXPORT void RT_SetSaturation (TheatrePtr t, int Saturation);
#define xf86_RT_SetBrightness		RT_SetBrightness
_X_EXPORT void RT_SetBrightness (TheatrePtr t, int Brightness);
#define xf86_RT_SetSharpness		RT_SetSharpness
_X_EXPORT void RT_SetSharpness (TheatrePtr t, CARD16 wSharpness);
#define xf86_RT_SetContrast		RT_SetContrast
_X_EXPORT void RT_SetContrast (TheatrePtr t, int Contrast);
#define xf86_RT_SetInterlace		RT_SetInterlace
_X_EXPORT void RT_SetInterlace (TheatrePtr t, CARD8 bInterlace);
#define xf86_RT_SetStandard		RT_SetStandard
_X_EXPORT void RT_SetStandard (TheatrePtr t, CARD16 wStandard);
#define xf86_RT_SetOutputVideoSize	RT_SetOutputVideoSize
_X_EXPORT void RT_SetOutputVideoSize (TheatrePtr t, CARD16 wHorzSize, CARD16 wVertSize, CARD8 fCC_On, CARD8 fVBICap_On);
#define xf86_RT_SetConnector		RT_SetConnector
_X_EXPORT void RT_SetConnector (TheatrePtr t, CARD16 wConnector, int tunerFlag);
#define xf86_ResetTheatreRegsForNoTVout	ResetTheatreRegsForNoTVout
_X_EXPORT void ResetTheatreRegsForNoTVout(TheatrePtr t);
#define xf86_ResetTheatreRegsForTVout	ResetTheatreRegsForTVout
_X_EXPORT void ResetTheatreRegsForTVout(TheatrePtr t);
#define xf86_DumpRageTheatreRegs	DumpRageTheatreRegs
_X_EXPORT void DumpRageTheatreRegs(TheatrePtr t);
#define xf86_DumpRageTheatreRegsByName	DumpRageTheatreRegsByName
_X_EXPORT void DumpRageTheatreRegsByName(TheatrePtr t);
#define xf86_ShutdownTheatre		ShutdownTheatre
_X_EXPORT void ShutdownTheatre(TheatrePtr t);

#endif
