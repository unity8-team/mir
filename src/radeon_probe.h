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

extern DriverRec RADEON;

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
    MT_HDMI    = 7, // this should really just be MT_DFP
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
    CONNECTOR_0XC,
    CONNECTOR_0XD,
    CONNECTOR_DIN,
    CONNECTOR_DISPLAY_PORT,
    CONNECTOR_UNSUPPORTED
} RADEONConnectorType;

typedef enum
{
    DAC_NONE    = 0,
    DAC_PRIMARY = 1,
    DAC_TVDAC   = 2,
    DAC_EXT     = 3
} RADEONDacType;

typedef enum
{
    TMDS_NONE    = 0,
    TMDS_INT     = 1,
    TMDS_EXT     = 2,
    TMDS_LVTMA   = 3,
    TMDS_DDIA    = 4,
    TMDS_UNIPHY  = 5
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
    RMX_CENTER
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

typedef struct
{
    Bool   valid;
    CARD32 mask_clk_reg;
    CARD32 mask_data_reg;
    CARD32 put_clk_reg;
    CARD32 put_data_reg;
    CARD32 get_clk_reg;
    CARD32 get_data_reg;
    CARD32 mask_clk_mask;
    CARD32 mask_data_mask;
    CARD32 put_clk_mask;
    CARD32 put_data_mask;
    CARD32 get_clk_mask;
    CARD32 get_data_mask;
} RADEONI2CBusRec, *RADEONI2CBusPtr;

typedef struct _RADEONCrtcPrivateRec {
#ifdef USE_XAA
    FBLinearPtr rotate_mem_xaa;
#endif
#ifdef USE_EXA
    ExaOffscreenArea *rotate_mem_exa;
#endif
    int crtc_id;
    int binding;
    CARD32 cursor_offset;
    /* Lookup table values to be set when the CRTC is enabled */
    CARD8 lut_r[256], lut_g[256], lut_b[256];

    uint32_t crtc_offset;
    int can_tile;
} RADEONCrtcPrivateRec, *RADEONCrtcPrivatePtr;

typedef struct {
    RADEONDacType DACType;
    RADEONTmdsType TMDSType;
    RADEONConnectorType ConnectorType;
    Bool valid;
    int output_id;
    int devices;
    int hpd_mask;
    RADEONI2CBusRec ddc_i2c;
    int igp_lane_info;
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
    RADEONI2CBusRec   ddc_i2c;
    CARD32            ps2_tvdac_adj;
    CARD32            pal_tvdac_adj;
    CARD32            ntsc_tvdac_adj;
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
    RADEONI2CBusRec   dvo_i2c;
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
    /* dig block */
    int transmitter_config;
    Bool coherent_mode;
    int igp_lane_info;

    char              *name;
    int               output_id;
    int               devices;
} RADEONOutputPrivateRec, *RADEONOutputPrivatePtr;

struct avivo_pll_state {
    CARD32 ref_div_src;
    CARD32 ref_div;
    CARD32 fb_div;
    CARD32 post_div_src;
    CARD32 post_div;
    CARD32 ext_ppll_cntl;
    CARD32 pll_cntl;
    CARD32 int_ss_cntl;
};


struct avivo_crtc_state {
    CARD32 pll_source;
    CARD32 h_total;
    CARD32 h_blank_start_end;
    CARD32 h_sync_a;
    CARD32 h_sync_a_cntl;
    CARD32 h_sync_b;
    CARD32 h_sync_b_cntl;
    CARD32 v_total;
    CARD32 v_blank_start_end;
    CARD32 v_sync_a;
    CARD32 v_sync_a_cntl;
    CARD32 v_sync_b;
    CARD32 v_sync_b_cntl;
    CARD32 control;
    CARD32 blank_control;
    CARD32 interlace_control;
    CARD32 stereo_control;
    CARD32 cursor_control;
};

struct avivo_grph_state {
    CARD32 enable;
    CARD32 control;
    CARD32 prim_surf_addr;
    CARD32 sec_surf_addr;
    CARD32 pitch;
    CARD32 x_offset;
    CARD32 y_offset;
    CARD32 x_start;
    CARD32 y_start;
    CARD32 x_end;
    CARD32 y_end;

    CARD32 viewport_start;
    CARD32 viewport_size;
};

struct avivo_state
{
    CARD32 hdp_fb_location;
    CARD32 mc_memory_map;
    CARD32 vga_memory_base;
    CARD32 vga_fb_start;

    CARD32 vga1_cntl;
    CARD32 vga2_cntl;

    CARD32 crtc_master_en;
    CARD32 crtc_tv_control;

    struct avivo_pll_state pll1;
    struct avivo_pll_state pll2;

    struct avivo_crtc_state crtc1;
    struct avivo_crtc_state crtc2;

    struct avivo_grph_state grph1;
    struct avivo_grph_state grph2;

    /* DDIA block on RS6xx chips */
    CARD32 ddia[37];

    /* scalers */
    CARD32 d1scl[40];
    CARD32 d2scl[40];
    CARD32 dxscl[6+2];

    /* dac regs */
    CARD32 daca[26];
    CARD32 dacb[26];

    /* tmdsa */
    CARD32 tmdsa[31];

    /* lvtma */
    CARD32 lvtma[39];

    /* dvoa */
    CARD32 dvoa[16];

    /* DCE3 chips */
    CARD32 fmt1[18];
    CARD32 fmt2[18];
    CARD32 dig1[19];
    CARD32 dig2[19];
    CARD32 hdmi1[57];
    CARD32 hdmi2[57];
    CARD32 aux_cntl1[14];
    CARD32 aux_cntl2[14];
    CARD32 aux_cntl3[14];
    CARD32 aux_cntl4[14];
    CARD32 phy[10];
    CARD32 uniphy1[8];
    CARD32 uniphy2[8];

};

/*
 * Maximum length of horizontal/vertical code timing tables for state storage
 */
#define MAX_H_CODE_TIMING_LEN 32
#define MAX_V_CODE_TIMING_LEN 32

typedef struct {
    struct avivo_state avivo;

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

    CARD32            bios_0_scratch;
    CARD32            bios_1_scratch;
    CARD32            bios_2_scratch;
    CARD32            bios_3_scratch;
    CARD32            bios_4_scratch;
    CARD32            bios_5_scratch;
    CARD32            bios_6_scratch;
    CARD32            bios_7_scratch;

    CARD32            surface_cntl;
    CARD32            surfaces[8][3];
    CARD32            mc_agp_location;
    CARD32            mc_agp_location_hi;
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
    CARD32            fp_horz_vert_active;
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
    int               reference_div;
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
    int               reference_div_2;
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

    ScrnInfoPtr pSecondaryScrn;    
    ScrnInfoPtr pPrimaryScrn;

    RADEONSaveRec     ModeReg;          /* Current mode                      */
    RADEONSaveRec     SavedReg;         /* Original (text) mode              */

} RADEONEntRec, *RADEONEntPtr;

/* radeon_probe.c */
extern PciChipsets          RADEONPciChipsets[];

/* radeon_driver.c */
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
