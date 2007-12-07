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
    DDC_NONE_DETECTED,
    DDC_MONID,
    DDC_DVI,
    DDC_VGA,
    DDC_CRT2,
    DDC_LCD,
    DDC_GPIO,
} RADEONDDCType;

typedef enum
{
    MT_UNKNOWN = -1,
    MT_NONE    = 0,
    MT_CRT     = 1,
    MT_LCD     = 2,
    MT_DFP     = 3,
    MT_CTV     = 4,
    MT_STV     = 5
} RADEONMonitorType;

typedef enum
{
    CONNECTOR_NONE,
    CONNECTOR_PROPRIETARY,
    CONNECTOR_CRT,
    CONNECTOR_DVI_I,
    CONNECTOR_DVI_D,
    CONNECTOR_CTV,
    CONNECTOR_STV,
    CONNECTOR_UNSUPPORTED
} RADEONConnectorType;

typedef enum
{
    CONNECTOR_NONE_ATOM,
    CONNECTOR_VGA_ATOM,
    CONNECTOR_DVI_I_ATOM,
    CONNECTOR_DVI_D_ATOM,
    CONNECTOR_DVI_A_ATOM,
    CONNECTOR_STV_ATOM,
    CONNECTOR_CTV_ATOM,
    CONNECTOR_LVDS_ATOM,
    CONNECTOR_DIGITAL_ATOM,
    CONNECTOR_UNSUPPORTED_ATOM
} RADEONConnectorTypeATOM;

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
    OUTPUT_DVI,
    OUTPUT_LVDS,
    OUTPUT_STV,
    OUTPUT_CTV,
} RADEONOutputType;

/* standards */
typedef enum
{
    TV_STD_NTSC      = 1,
    TV_STD_PAL       = 2,
    TV_STD_PAL_M     = 4,
    TV_STD_PAL_60    = 8,
    TV_STD_NTSC_J    = 16,
    TV_STD_SCART_PAL = 32,
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
} RADEONCrtcPrivateRec, *RADEONCrtcPrivatePtr;

typedef struct {
    RADEONDDCType DDCType;
    RADEONDacType DACType;
    RADEONTmdsType TMDSType;
    RADEONConnectorType ConnectorType;
    Bool valid;
} RADEONBIOSConnector;

typedef struct _RADEONOutputPrivateRec {
    int num;
    RADEONOutputType type;
    void *dev_priv;
    RADEONDDCType DDCType;
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
} RADEONOutputPrivateRec, *RADEONOutputPrivatePtr;


/*
 * Maximum length of horizontal/vertical code timing tables for state storage
 */
#define MAX_H_CODE_TIMING_LEN 32
#define MAX_V_CODE_TIMING_LEN 32

typedef struct {
				/* Common registers */
    CARD32            ovr_clr;
    CARD32            ovr_wid_left_right;
    CARD32            ovr_wid_top_bottom;
    CARD32            ov0_scale_cntl;
    CARD32            mpp_tb_config;
    CARD32            mpp_gp_config;
    CARD32            subpic_cntl;
    CARD32            viph_control;
    CARD32            i2c_cntl_1;
    CARD32            gen_int_cntl;
    CARD32            cap0_trig_cntl;
    CARD32            cap1_trig_cntl;
    CARD32            bus_cntl;
    CARD32            bios_4_scratch;
    CARD32            bios_5_scratch;
    CARD32            bios_6_scratch;
    CARD32            surface_cntl;
    CARD32            surfaces[8][3];
    CARD32            mc_agp_location;
    CARD32            mc_fb_location;
    CARD32            display_base_addr;
    CARD32            display2_base_addr;
    CARD32            ov0_base_addr;

				/* Other registers to save for VT switches */
    CARD32            dp_datatype;
    CARD32            rbbm_soft_reset;
    CARD32            clock_cntl_index;
    CARD32            amcgpio_en_reg;
    CARD32            amcgpio_mask;

				/* CRTC registers */
    CARD32            crtc_gen_cntl;
    CARD32            crtc_ext_cntl;
    CARD32            dac_cntl;
    CARD32            crtc_h_total_disp;
    CARD32            crtc_h_sync_strt_wid;
    CARD32            crtc_v_total_disp;
    CARD32            crtc_v_sync_strt_wid;
    CARD32            crtc_offset;
    CARD32            crtc_offset_cntl;
    CARD32            crtc_pitch;
    CARD32            disp_merge_cntl;
    CARD32            grph_buffer_cntl;
    CARD32            crtc_more_cntl;
    CARD32            crtc_tile_x0_y0;

				/* CRTC2 registers */
    CARD32            crtc2_gen_cntl;
    CARD32            dac_macro_cntl;
    CARD32            dac2_cntl;
    CARD32            disp_output_cntl;
    CARD32            disp_tv_out_cntl;
    CARD32            disp_hw_debug;
    CARD32            disp2_merge_cntl;
    CARD32            grph2_buffer_cntl;
    CARD32            crtc2_h_total_disp;
    CARD32            crtc2_h_sync_strt_wid;
    CARD32            crtc2_v_total_disp;
    CARD32            crtc2_v_sync_strt_wid;
    CARD32            crtc2_offset;
    CARD32            crtc2_offset_cntl;
    CARD32            crtc2_pitch;
    CARD32            crtc2_tile_x0_y0;

				/* Flat panel registers */
    CARD32            fp_crtc_h_total_disp;
    CARD32            fp_crtc_v_total_disp;
    CARD32            fp_gen_cntl;
    CARD32            fp2_gen_cntl;
    CARD32            fp_h_sync_strt_wid;
    CARD32            fp_h2_sync_strt_wid;
    CARD32            fp_horz_stretch;
    CARD32            fp_panel_cntl;
    CARD32            fp_v_sync_strt_wid;
    CARD32            fp_v2_sync_strt_wid;
    CARD32            fp_vert_stretch;
    CARD32            lvds_gen_cntl;
    CARD32            lvds_pll_cntl;
    CARD32            tmds_pll_cntl;
    CARD32            tmds_transmitter_cntl;

				/* Computed values for PLL */
    CARD32            dot_clock_freq;
    CARD32            pll_output_freq;
    int               feedback_div;
    int               post_div;

				/* PLL registers */
    unsigned          ppll_ref_div;
    unsigned          ppll_div_3;
    CARD32            htotal_cntl;
    CARD32            vclk_ecp_cntl;

				/* Computed values for PLL2 */
    CARD32            dot_clock_freq_2;
    CARD32            pll_output_freq_2;
    int               feedback_div_2;
    int               post_div_2;

				/* PLL2 registers */
    CARD32            p2pll_ref_div;
    CARD32            p2pll_div_0;
    CARD32            htotal_cntl2;
    CARD32            pixclks_cntl;

				/* Pallet */
    Bool              palette_valid;
    CARD32            palette[256];
    CARD32            palette2[256];

    CARD32            rs480_unk_e30;
    CARD32            rs480_unk_e34;
    CARD32            rs480_unk_e38;
    CARD32            rs480_unk_e3c;

    /* TV out registers */
    CARD32 	      tv_master_cntl;
    CARD32 	      tv_htotal;
    CARD32 	      tv_hsize;
    CARD32 	      tv_hdisp;
    CARD32 	      tv_hstart;
    CARD32 	      tv_vtotal;
    CARD32 	      tv_vdisp;
    CARD32 	      tv_timing_cntl;
    CARD32 	      tv_vscaler_cntl1;
    CARD32 	      tv_vscaler_cntl2;
    CARD32 	      tv_sync_size;
    CARD32 	      tv_vrestart;
    CARD32 	      tv_hrestart;
    CARD32 	      tv_frestart;
    CARD32 	      tv_ftotal;
    CARD32 	      tv_clock_sel_cntl;
    CARD32 	      tv_clkout_cntl;
    CARD32 	      tv_data_delay_a;
    CARD32 	      tv_data_delay_b;
    CARD32 	      tv_dac_cntl;
    CARD32 	      tv_pll_cntl;
    CARD32 	      tv_pll_cntl1;
    CARD32	      tv_pll_fine_cntl;
    CARD32 	      tv_modulator_cntl1;
    CARD32 	      tv_modulator_cntl2;
    CARD32 	      tv_frame_lock_cntl;
    CARD32 	      tv_pre_dac_mux_cntl;
    CARD32 	      tv_rgb_cntl;
    CARD32 	      tv_y_saw_tooth_cntl;
    CARD32 	      tv_y_rise_cntl;
    CARD32 	      tv_y_fall_cntl;
    CARD32 	      tv_uv_adr;
    CARD32	      tv_upsamp_and_gain_cntl;
    CARD32	      tv_gain_limit_settings;
    CARD32	      tv_linear_gain_settings;
    CARD32	      tv_crc_cntl;
    CARD32            tv_sync_cntl;
    CARD32	      gpiopad_a;
    CARD32            pll_test_cntl;

    CARD16	      h_code_timing[MAX_H_CODE_TIMING_LEN];
    CARD16	      v_code_timing[MAX_V_CODE_TIMING_LEN];

} RADEONSaveRec, *RADEONSavePtr;

#define RADEON_MAX_CRTC 2
#define RADEON_MAX_BIOS_CONNECTOR 8

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

    ScrnInfoPtr pSecondaryScrn;    
    ScrnInfoPtr pPrimaryScrn;

    RADEONSaveRec     ModeReg;          /* Current mode                      */
    RADEONSaveRec     SavedReg;         /* Original (text) mode              */

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
