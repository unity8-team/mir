/*
 * Copyright 2000 ATI Technologies Inc., Markham, Ontario, and
 *                VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL ATI, VA LINUX SYSTEMS AND/OR
 * THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Kevin E. Martin <martin@xfree86.org>
 *
 * Modified by Marc Aurele La France <tsi@xfree86.org> for ATI driver merge.
 */

#ifndef _RADEON_PROBE_H_
#define _RADEON_PROBE_H_ 1

#include <stdint.h>
#include "xf86str.h"
#include "xf86DDC.h"
#include "randrstr.h"

#define _XF86MISC_SERVER_
#include <X11/extensions/xf86misc.h>

#include "xf86Crtc.h"

#ifdef USE_EXA
#include "exa.h"
#endif
#ifdef USE_XAA
#include "xaa.h"
#endif

typedef enum
{
    MT_UNKNOWN = -1,
    MT_NONE    = 0,
    MT_CRT     = 1,
    MT_LCD     = 2,
    MT_DFP     = 3,
    MT_CTV     = 4,
    MT_STV     = 5,
    MT_CV      = 6,
    MT_HDMI    = 7,
    MT_DP      = 8
} RADEONMonitorType;

typedef enum
{
    CONNECTOR_NONE,
    CONNECTOR_VGA,
    CONNECTOR_DVI_I,
    CONNECTOR_DVI_D,
    CONNECTOR_DVI_A,
    CONNECTOR_STV,
    CONNECTOR_CTV,
    CONNECTOR_LVDS,
    CONNECTOR_DIGITAL,
    CONNECTOR_SCART,
    CONNECTOR_HDMI_TYPE_A,
    CONNECTOR_HDMI_TYPE_B,
    CONNECTOR_DIN,
    CONNECTOR_DISPLAY_PORT,
    CONNECTOR_UNSUPPORTED
} RADEONConnectorType;

typedef enum {
    OUTPUT_NONE_ATOM,
    OUTPUT_DAC_EXTERNAL_ATOM,
    OUTPUT_DACA_ATOM,
    OUTPUT_DACB_ATOM,
    OUTPUT_TMDSA_ATOM,
    OUTPUT_LVTMA_ATOM,
    OUTPUT_TMDSB_ATOM,
    OUTPUT_LVDS_ATOM,
    OUTPUT_LVTMB_ATOM
} RADEONOutputTypeATOM;

typedef enum
{
    DAC_UNKNOWN = -1,
    DAC_PRIMARY = 0,
    DAC_TVDAC   = 1,
    DAC_NONE    = 2
} RADEONDacType;

typedef enum
{
    TMDS_UNKNOWN = -1,
    TMDS_INT     = 0,
    TMDS_EXT     = 1,
    TMDS_NONE    = 2
} RADEONTmdsType;

typedef enum
{
    DVI_AUTO,
    DVI_DIGITAL,
    DVI_ANALOG
} RADEONDviType;

typedef enum
{
    RMX_OFF,
    RMX_FULL,
    RMX_CENTER,
    RMX_ASPECT
} RADEONRMXType;

typedef struct {
    CARD32 freq;
    CARD32 value;
}RADEONTMDSPll;

typedef enum
{
    OUTPUT_NONE,
    OUTPUT_VGA,
    OUTPUT_DVI_I,
    OUTPUT_DVI_D,
    OUTPUT_DVI_A,
    OUTPUT_LVDS,
    OUTPUT_STV,
    OUTPUT_CTV,
    OUTPUT_CV,
    OUTPUT_HDMI,
    OUTPUT_DP
} RADEONOutputType;

#define OUTPUT_IS_DVI ((radeon_output->type == OUTPUT_DVI_D || \
                        radeon_output->type == OUTPUT_DVI_I || \
                        radeon_output->type == OUTPUT_DVI_A))
#define OUTPUT_IS_TV ((radeon_output->type == OUTPUT_STV || \
                       radeon_output->type == OUTPUT_CTV))

/* standards */
typedef enum
{
    TV_STD_NTSC      = 1,
    TV_STD_PAL       = 2,
    TV_STD_PAL_M     = 4,
    TV_STD_PAL_60    = 8,
    TV_STD_NTSC_J    = 16,
    TV_STD_SCART_PAL = 32,
    TV_STD_SECAM     = 64,
    TV_STD_PAL_CN    = 128,
} TVStd;

typedef struct _RADEONCrtcPrivateRec {
#ifdef USE_XAA
    FBLinearPtr rotate_mem_xaa;
#endif
#ifdef USE_EXA
    ExaOffscreenArea *rotate_mem_exa;
#endif
    int crtc_id;
    int binding;
    /* Lookup table values to be set when the CRTC is enabled */
    CARD8 lut_r[256], lut_g[256], lut_b[256];

    uint32_t crtc_offset;
    int               h_total, h_blank, h_sync_wid, h_sync_pol;
    int               v_total, v_blank, v_sync_wid, v_sync_pol;
    int               fb_format, fb_length;
    int               fb_pitch, fb_width, fb_height;
    INT16             cursor_x;
    INT16             cursor_y;
    unsigned long     cursor_offset;
} RADEONCrtcPrivateRec, *RADEONCrtcPrivatePtr;

typedef struct {
    CARD32 ddc_line;
    RADEONDacType DACType;
    RADEONTmdsType TMDSType;
    RADEONConnectorType ConnectorType;
    Bool valid;
    int output_id;
    int devices;
} RADEONBIOSConnector;

typedef struct _RADEONOutputPrivateRec {
    int num;
    RADEONOutputType type;
    void *dev_priv;
    CARD32 ddc_line;
    RADEONDacType DACType;
    RADEONDviType DVIType;
    RADEONTmdsType TMDSType;
    RADEONConnectorType ConnectorType;
    RADEONMonitorType MonType;
    int crtc_num;
    int DDCReg;
    I2CBusPtr         pI2CBus;
    CARD32            tv_dac_adj;
    /* panel stuff */
    int               PanelXRes;
    int               PanelYRes;
    int               HOverPlus;
    int               HSyncWidth;
    int               HBlank;
    int               VOverPlus;
    int               VSyncWidth;
    int               VBlank;
    int               Flags;            /* Saved copy of mode flags          */
    int               PanelPwrDly;
    int               DotClock;
    RADEONTMDSPll     tmds_pll[4];
    RADEONRMXType     rmx_type;
    /* dvo */
    I2CDevPtr         DVOChip;
    int               dvo_i2c_reg;
    int               dvo_i2c_slave_addr;
    Bool              dvo_duallink;
    /* TV out */
    TVStd             default_tvStd;
    TVStd             tvStd;
    int               hPos;
    int               vPos;
    int               hSize;
    float             TVRefClk;
    int               SupportedTVStds;
    Bool              tv_on;
    int               load_detection;

    char              *name;
    int               output_id;
    int               devices;
} RADEONOutputPrivateRec, *RADEONOutputPrivatePtr;

#define RADEON_MAX_CRTC 2
#define RADEON_MAX_BIOS_CONNECTOR 16

typedef struct
{
    Bool HasSecondary;
    Bool              HasCRTC2;         /* All cards except original Radeon  */
    /*
     * The next two are used to make sure CRTC2 is restored before CRTC_EXT,
     * otherwise it could lead to blank screens.
     */
    Bool IsSecondaryRestored;
    Bool RestorePrimary;

    Bool ReversedDAC;	  /* TVDAC used as primary dac */
    Bool ReversedTMDS;    /* DDC_DVI is used for external TMDS */
    xf86CrtcPtr pCrtc[RADEON_MAX_CRTC];
    RADEONCrtcPrivatePtr Controller[RADEON_MAX_CRTC];

} RADEONEntRec, *RADEONEntPtr;

/* radeon_probe.c */
extern const OptionInfoRec *RADEONAvailableOptions(int, int);
extern void                 RADEONIdentify(int);
extern Bool                 RADEONProbe(DriverPtr, int);

extern PciChipsets          RADEONPciChipsets[];

/* radeon_driver.c */
extern void                 RADEONLoaderRefSymLists(void);
extern Bool                 RADEONPreInit(ScrnInfoPtr, int);
extern Bool                 RADEONScreenInit(int, ScreenPtr, int, char **);
extern Bool                 RADEONSwitchMode(int, DisplayModePtr, int);
#ifdef X_XF86MiscPassMessage
extern Bool                 RADEONHandleMessage(int, const char*, const char*,
					        char**);
#endif
extern void                 RADEONAdjustFrame(int, int, int, int);
extern Bool                 RADEONEnterVT(int, int);
extern void                 RADEONLeaveVT(int, int);
extern void                 RADEONFreeScreen(int, int);
extern ModeStatus           RADEONValidMode(int, DisplayModePtr, Bool, int);

extern const OptionInfoRec *RADEONOptionsWeak(void);

#endif /* _RADEON_PROBE_H_ */
