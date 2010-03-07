/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/nv/nv_const.h,v 1.6 2001/12/07 00:09:55 mvojkovi Exp $ */

#ifndef __NV_CONST_H__
#define __NV_CONST_H__

#define NV_VERSION 4000
#define NV_NAME "NOUVEAU"
#define NV_DRIVER_NAME "nouveau"

typedef enum {
    OPTION_SW_CURSOR,
    OPTION_HW_CURSOR,
    OPTION_NOACCEL,
    OPTION_SHADOW_FB,
    OPTION_VIDEO_KEY,
} NVOpts;


static const OptionInfoRec NVOptions[] = {
    { OPTION_SW_CURSOR,         "SWcursor",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_HW_CURSOR,         "HWcursor",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_NOACCEL,           "NoAccel",      OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_SHADOW_FB,         "ShadowFB",     OPTV_BOOLEAN,   {0}, FALSE },
    { OPTION_VIDEO_KEY,		"VideoKey",	OPTV_INTEGER,	{0}, FALSE },
    { -1,                       NULL,           OPTV_NONE,      {0}, FALSE }
};

#endif /* __NV_CONST_H__ */
          
