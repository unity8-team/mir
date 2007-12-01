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
 *   Rickard E. Faith <faith@valinux.com>
 *   Alan Hourihane <alanh@fairlite.demon.co.uk>
 *
 */

#ifndef _RADEON_H_
#define _RADEON_H_

#include <stdlib.h>		/* For abs() */
#include <unistd.h>		/* For usleep() */
#include <sys/time.h>		/* For gettimeofday() */

#include "xf86str.h"
#include "compiler.h"
#include "xf86fbman.h"

				/* PCI support */
#include "xf86Pci.h"

#ifdef USE_EXA
#include "exa.h"
#endif
#ifdef USE_XAA
#include "xaa.h"
#endif

				/* Exa and Cursor Support */
#include "vbe.h"
#include "xf86Cursor.h"

				/* DDC support */
#include "xf86DDC.h"

				/* Xv support */
#include "xf86xv.h"

#include "radeon_probe.h"
#include "radeon_tv.h"

				/* DRI support */
#ifdef XF86DRI
#define _XF86DRI_SERVER_
#include "radeon_dripriv.h"
#include "dri.h"
#include "GL/glxint.h"
#ifdef DAMAGE
#include "damage.h"
#include "globals.h"
#endif
#endif

#include "xf86Crtc.h"
#include "X11/Xatom.h"

				/* Render support */
#ifdef RENDER
#include "picturestr.h"
#endif

#include "atipcirename.h"

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b) ((a)>(b)?(b):(a))
#endif

typedef enum {
    OPTION_NOACCEL,
    OPTION_SW_CURSOR,
    OPTION_DAC_6BIT,
    OPTION_DAC_8BIT,
#ifdef XF86DRI
    OPTION_BUS_TYPE,
    OPTION_CP_PIO,
    OPTION_USEC_TIMEOUT,
    OPTION_AGP_MODE,
    OPTION_AGP_FW,
    OPTION_GART_SIZE,
    OPTION_GART_SIZE_OLD,
    OPTION_RING_SIZE,
    OPTION_BUFFER_SIZE,
    OPTION_DEPTH_MOVE,
    OPTION_PAGE_FLIP,
    OPTION_NO_BACKBUFFER,
    OPTION_XV_DMA,
    OPTION_FBTEX_PERCENT,
    OPTION_DEPTH_BITS,
    OPTION_PCIAPER_SIZE,
#ifdef USE_EXA
    OPTION_ACCEL_DFS,
#endif
#endif
    OPTION_DDC_MODE,
    OPTION_IGNORE_EDID,
    OPTION_DISP_PRIORITY,
    OPTION_PANEL_SIZE,
    OPTION_MIN_DOTCLOCK,
    OPTION_COLOR_TILING,
#ifdef XvExtension
    OPTION_VIDEO_KEY,
    OPTION_RAGE_THEATRE_CRYSTAL,
    OPTION_RAGE_THEATRE_TUNER_PORT,
    OPTION_RAGE_THEATRE_COMPOSITE_PORT,
    OPTION_RAGE_THEATRE_SVIDEO_PORT,
    OPTION_TUNER_TYPE,
    OPTION_RAGE_THEATRE_MICROC_PATH,
    OPTION_RAGE_THEATRE_MICROC_TYPE,
    OPTION_SCALER_WIDTH,
#endif
#ifdef RENDER
    OPTION_RENDER_ACCEL,
    OPTION_SUBPIXEL_ORDER,
#endif
    OPTION_SHOWCACHE,
    OPTION_DYNAMIC_CLOCKS,
    OPTION_BIOS_HOTKEYS,
    OPTION_VGA_ACCESS,
    OPTION_REVERSE_DDC,
    OPTION_LVDS_PROBE_PLL,
    OPTION_ACCELMETHOD,
    OPTION_CONNECTORTABLE,
    OPTION_DRI,
    OPTION_DEFAULT_CONNECTOR_TABLE,
#if defined(__powerpc__)
    OPTION_MAC_MODEL,
#endif
    OPTION_DEFAULT_TMDS_PLL,
    OPTION_TVDAC_LOAD_DETECT
} RADEONOpts;


#define RADEON_IDLE_RETRY      16 /* Fall out of idle loops after this count */
#define RADEON_TIMEOUT    2000000 /* Fall out of wait loops after this count */

#define RADEON_VSYNC_TIMEOUT	20000 /* Maximum wait for VSYNC (in usecs) */

/* Buffer are aligned on 4096 byte boundaries */
#define RADEON_BUFFER_ALIGN 0x00000fff
#define RADEON_VBIOS_SIZE 0x00010000
#define RADEON_USE_RMX 0x80000000 /* mode flag for using RMX
				   * Need to comfirm this is not used
				   * for something else.
				   */

#define RADEON_LOGLEVEL_DEBUG 4

/* for Xv, outputs */
#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

/* Other macros */
#define RADEON_ARRAY_SIZE(x)  (sizeof(x)/sizeof(x[0]))
#define RADEON_ALIGN(x,bytes) (((x) + ((bytes) - 1)) & ~((bytes) - 1))
#define RADEONPTR(pScrn)      ((RADEONInfoPtr)(pScrn)->driverPrivate)

typedef struct {
    int    revision;
    CARD16 rr1_offset;
    CARD16 rr2_offset;
    CARD16 dyn_clk_offset;
    CARD16 pll_offset;
    CARD16 mem_config_offset;
    CARD16 mem_reset_offset;
    CARD16 short_mem_offset;
    CARD16 rr3_offset;
    CARD16 rr4_offset;
} RADEONBIOSInitTable;

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
    CARD32 scl_enable;
};

struct avivo_dac_state {
    CARD32 enable;
    CARD32 source_select;
    CARD32 force_output_cntl;
    CARD32 powerdown;
};

struct avivo_dig_state {
    CARD32 cntl;
    CARD32 bit_depth_cntl;
    CARD32 data_sync;
    CARD32 transmitter_enable;
    CARD32 transmitter_cntl;
    CARD32 source_select;
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

    CARD32 lvtma_pwrseq_cntl;
    CARD32 lvtma_pwrseq_state;

    struct avivo_pll_state pll1;
    struct avivo_pll_state pll2;

    struct avivo_crtc_state crtc1;
    struct avivo_crtc_state crtc2;

    struct avivo_grph_state grph1;
    struct avivo_grph_state grph2;

    struct avivo_dac_state daca;
    struct avivo_dac_state dacb;

    struct avivo_dig_state tmds1;
    struct avivo_dig_state tmds2;

};

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
    CARD32            bios_4_scratch;
    CARD32            bios_5_scratch;
    CARD32            bios_6_scratch;
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

typedef struct {
    CARD16            reference_freq;
    CARD16            reference_div;
    CARD32            min_pll_freq;
    CARD32            max_pll_freq;
    CARD16            xclk;
} RADEONPLLRec, *RADEONPLLPtr;

typedef struct {
    int               bitsPerPixel;
    int               depth;
    int               displayWidth;
    int               displayHeight;
    int               pixel_code;
    int               pixel_bytes;
    DisplayModePtr    mode;
} RADEONFBLayout;

typedef enum {
    CHIP_FAMILY_UNKNOW,
    CHIP_FAMILY_LEGACY,
    CHIP_FAMILY_RADEON,
    CHIP_FAMILY_RV100,
    CHIP_FAMILY_RS100,    /* U1 (IGP320M) or A3 (IGP320)*/
    CHIP_FAMILY_RV200,
    CHIP_FAMILY_RS200,    /* U2 (IGP330M/340M/350M) or A4 (IGP330/340/345/350), RS250 (IGP 7000) */
    CHIP_FAMILY_R200,
    CHIP_FAMILY_RV250,
    CHIP_FAMILY_RS300,    /* RS300/RS350 */
    CHIP_FAMILY_RV280,
    CHIP_FAMILY_R300,
    CHIP_FAMILY_R350,
    CHIP_FAMILY_RV350,
    CHIP_FAMILY_RV380,    /* RV370/RV380/M22/M24 */
    CHIP_FAMILY_R420,     /* R420/R423/M18 */
    CHIP_FAMILY_RV410,    /* RV410, M26 */
    CHIP_FAMILY_RS400,    /* xpress 200, 200m (RS400/410/480) */
    CHIP_FAMILY_RV515,    /* rv515 */
    CHIP_FAMILY_R520,    /* r520 */
    CHIP_FAMILY_RV530,    /* rv530 */
    CHIP_FAMILY_R580,    /* r580 */
    CHIP_FAMILY_RV560,   /* rv560 */
    CHIP_FAMILY_RV570,   /* rv570 */
    CHIP_FAMILY_RS690,
    CHIP_FAMILY_R600,    /* r60 */
    CHIP_FAMILY_R630,
    CHIP_FAMILY_RV610,
    CHIP_FAMILY_RV630,
    CHIP_FAMILY_RS740,
    CHIP_FAMILY_LAST
} RADEONChipFamily;

#define IS_RV100_VARIANT ((info->ChipFamily == CHIP_FAMILY_RV100)  ||  \
        (info->ChipFamily == CHIP_FAMILY_RV200)  ||  \
        (info->ChipFamily == CHIP_FAMILY_RS100)  ||  \
        (info->ChipFamily == CHIP_FAMILY_RS200)  ||  \
        (info->ChipFamily == CHIP_FAMILY_RV250)  ||  \
        (info->ChipFamily == CHIP_FAMILY_RV280)  ||  \
        (info->ChipFamily == CHIP_FAMILY_RS300))


#define IS_R300_VARIANT ((info->ChipFamily == CHIP_FAMILY_R300)  ||  \
        (info->ChipFamily == CHIP_FAMILY_RV350) ||  \
        (info->ChipFamily == CHIP_FAMILY_R350)  ||  \
        (info->ChipFamily == CHIP_FAMILY_RV380) ||  \
        (info->ChipFamily == CHIP_FAMILY_R420)  ||  \
        (info->ChipFamily == CHIP_FAMILY_RV410) ||  \
        (info->ChipFamily == CHIP_FAMILY_RS400))

#define IS_AVIVO_VARIANT ((info->ChipFamily >= CHIP_FAMILY_RV515))

/*
 * Errata workarounds
 */
typedef enum {
       CHIP_ERRATA_R300_CG             = 0x00000001,
       CHIP_ERRATA_PLL_DUMMYREADS      = 0x00000002,
       CHIP_ERRATA_PLL_DELAY           = 0x00000004
} RADEONErrata;

typedef enum {
    RADEON_SIL_164  = 0x00000001,
    RADEON_SIL_1178 = 0x00000002
} RADEONExtTMDSChip;

#if defined(__powerpc__)
typedef enum {
       RADEON_MAC_IBOOK              = 0x00000001,
       RADEON_MAC_POWERBOOK_EXTERNAL = 0x00000002,
       RADEON_MAC_POWERBOOK_INTERNAL = 0x00000004,
       RADEON_MAC_POWERBOOK_VGA      = 0x00000008,
       RADEON_MAC_MINI_EXTERNAL      = 0x00000016,
       RADEON_MAC_MINI_INTERNAL      = 0x00000032
} RADEONMacModel;
#endif

typedef enum {
	CARD_PCI,
	CARD_AGP,
	CARD_PCIE
} RADEONCardType;

typedef struct _atomBiosHandle *atomBiosHandlePtr;

typedef struct {
    CARD32 pci_device_id;
    RADEONChipFamily chip_family;
    int mobility;
    int igp;
    int nocrtc2;
    int nointtvout;
    int singledac;
} RADEONCardInfo;

typedef struct {
    EntityInfoPtr     pEnt;
    pciVideoPtr       PciInfo;
    PCITAG            PciTag;
    int               Chipset;
    RADEONChipFamily  ChipFamily;
    RADEONErrata      ChipErrata;

    unsigned long     LinearAddr;       /* Frame buffer physical address     */
    unsigned long     MMIOAddr;         /* MMIO region physical address      */
    unsigned long     BIOSAddr;         /* BIOS physical address             */
    CARD32            fbLocation;
    CARD32            gartLocation;
    CARD32            mc_fb_location;
    CARD32            mc_agp_location;
    CARD32            mc_agp_location_hi;

    void              *MMIO;            /* Map of MMIO region                */
    void              *FB;              /* Map of frame buffer               */
    CARD8             *VBIOS;           /* Video BIOS pointer                */

    Bool              IsAtomBios;       /* New BIOS used in R420 etc.        */
    int               ROMHeaderStart;   /* Start of the ROM Info Table       */
    int               MasterDataStart;  /* Offset for Master Data Table for ATOM BIOS */

    CARD32            MemCntl;
    CARD32            BusCntl;
    unsigned long     MMIOSize;         /* MMIO region physical address      */
    unsigned long     FbMapSize;        /* Size of frame buffer, in bytes    */
    unsigned long     FbSecureSize;     /* Size of secured fb area at end of
                                           framebuffer */

    Bool              IsMobility;       /* Mobile chips for laptops */
    Bool              IsIGP;            /* IGP chips */
    Bool              HasSingleDAC;     /* only TVDAC on chip */
    Bool              ddc_mode;         /* Validate mode by matching exactly
					 * the modes supported in DDC data
					 */
    Bool              R300CGWorkaround;

				/* EDID or BIOS values for FPs */
    int               RefDivider;
    int               FeedbackDivider;
    int               PostDivider;
    Bool              UseBiosDividers;
				/* EDID data using DDC interface */
    Bool              ddc_bios;
    Bool              ddc1;
    Bool              ddc2;

    RADEONPLLRec      pll;

    int               RamWidth;
    float	      sclk;		/* in MHz */
    float	      mclk;		/* in MHz */
    Bool	      IsDDR;
    int               DispPriority;

    RADEONSaveRec     SavedReg;         /* Original (text) mode              */
    RADEONSaveRec     ModeReg;          /* Current mode                      */
    Bool              (*CloseScreen)(int, ScreenPtr);

    void              (*BlockHandler)(int, pointer, pointer, pointer);

    Bool              PaletteSavedOnVT; /* Palette saved on last VT switch   */

#ifdef USE_EXA
    ExaDriverPtr      exa;
    int               exaSyncMarker;
    int               exaMarkerSynced;
    int               engineMode;
#define EXA_ENGINEMODE_UNKNOWN 0
#define EXA_ENGINEMODE_2D      1
#define EXA_ENGINEMODE_3D      2
#ifdef XF86DRI
    Bool              accelDFS;
#endif
#endif
#ifdef USE_XAA
    XAAInfoRecPtr     accel;
#endif
    Bool              accelOn;
    xf86CursorInfoPtr cursor;
    CARD32            cursor_offset;
#ifdef USE_XAA
    unsigned long     cursor_end;
#endif
    Bool              allowColorTiling;
    Bool              tilingEnabled; /* mirror of sarea->tiling_enabled */
#ifdef ARGB_CURSOR
    Bool	      cursor_argb;
#endif
    int               cursor_fg;
    int               cursor_bg;

#ifdef USE_XAA
    /*
     * XAAForceTransBlit is used to change the behavior of the XAA
     * SetupForScreenToScreenCopy function, to make it DGA-friendly.
     */
    Bool              XAAForceTransBlit;
#endif

    int               fifo_slots;       /* Free slots in the FIFO (64 max)   */
    int               pix24bpp;         /* Depth of pixmap for 24bpp fb      */
    Bool              dac6bits;         /* Use 6 bit DAC?                    */

				/* Computed values for Radeon */
    int               pitch;
    int               datatype;
    CARD32            dp_gui_master_cntl;
    CARD32            dp_gui_master_cntl_clip;
    CARD32            trans_color;

				/* Saved values for ScreenToScreenCopy */
    int               xdir;
    int               ydir;

#ifdef USE_XAA
				/* ScanlineScreenToScreenColorExpand support */
    unsigned char     *scratch_buffer[1];
    unsigned char     *scratch_save;
    int               scanline_x;
    int               scanline_y;
    int               scanline_w;
    int               scanline_h;
    int               scanline_h_w;
    int               scanline_words;
    int               scanline_direct;
    int               scanline_bpp;     /* Only used for ImageWrite */
    int               scanline_fg;
    int               scanline_bg;
    int               scanline_hpass;
    int               scanline_x1clip;
    int               scanline_x2clip;
#endif
				/* Saved values for DashedTwoPointLine */
    int               dashLen;
    CARD32            dashPattern;
    int               dash_fg;
    int               dash_bg;

    DGAModePtr        DGAModes;
    int               numDGAModes;
    Bool              DGAactive;
    int               DGAViewportStatus;
    DGAFunctionRec    DGAFuncs;

    RADEONFBLayout    CurrentLayout;
    CARD32            dst_pitch_offset;
#ifdef XF86DRI
    Bool              noBackBuffer;	
    Bool              directRenderingEnabled;
    Bool              directRenderingInited;
    Bool              newMemoryMap;
    drmVersionPtr     pLibDRMVersion;
    drmVersionPtr     pKernelDRMVersion;
    DRIInfoPtr        pDRIInfo;
    int               drmFD;
    int               numVisualConfigs;
    __GLXvisualConfig *pVisualConfigs;
    RADEONConfigPrivPtr pVisualConfigsPriv;
    Bool             (*DRICloseScreen)(int, ScreenPtr);

    drm_handle_t         fbHandle;

    drmSize           registerSize;
    drm_handle_t         registerHandle;

    RADEONCardType    cardType;            /* Current card is a PCI card */
    drmSize           pciSize;
    drm_handle_t         pciMemHandle;
    unsigned char     *PCI;             /* Map */

    Bool              depthMoves;       /* Enable depth moves -- slow! */
    Bool              allowPageFlip;    /* Enable 3d page flipping */
#ifdef DAMAGE
    DamagePtr         pDamage;
    RegionRec         driRegion;
#endif
    Bool              have3DWindows;    /* Are there any 3d clients? */

    int               pciAperSize;
    drmSize           gartSize;
    drm_handle_t         agpMemHandle;     /* Handle from drmAgpAlloc */
    unsigned long     gartOffset;
    unsigned char     *AGP;             /* Map */
    int               agpMode;

    CARD32            pciCommand;

    Bool              CPRuns;           /* CP is running */
    Bool              CPInUse;          /* CP has been used by X server */
    Bool              CPStarted;        /* CP has started */
    int               CPMode;           /* CP mode that server/clients use */
    int               CPFifoSize;       /* Size of the CP command FIFO */
    int               CPusecTimeout;    /* CP timeout in usecs */
    Bool              needCacheFlush;

				/* CP ring buffer data */
    unsigned long     ringStart;        /* Offset into GART space */
    drm_handle_t         ringHandle;       /* Handle from drmAddMap */
    drmSize           ringMapSize;      /* Size of map */
    int               ringSize;         /* Size of ring (in MB) */
    drmAddress        ring;             /* Map */
    int               ringSizeLog2QW;

    unsigned long     ringReadOffset;   /* Offset into GART space */
    drm_handle_t         ringReadPtrHandle; /* Handle from drmAddMap */
    drmSize           ringReadMapSize;  /* Size of map */
    drmAddress        ringReadPtr;      /* Map */

				/* CP vertex/indirect buffer data */
    unsigned long     bufStart;         /* Offset into GART space */
    drm_handle_t         bufHandle;        /* Handle from drmAddMap */
    drmSize           bufMapSize;       /* Size of map */
    int               bufSize;          /* Size of buffers (in MB) */
    drmAddress        buf;              /* Map */
    int               bufNumBufs;       /* Number of buffers */
    drmBufMapPtr      buffers;          /* Buffer map */

				/* CP GART Texture data */
    unsigned long     gartTexStart;      /* Offset into GART space */
    drm_handle_t         gartTexHandle;     /* Handle from drmAddMap */
    drmSize           gartTexMapSize;    /* Size of map */
    int               gartTexSize;       /* Size of GART tex space (in MB) */
    drmAddress        gartTex;           /* Map */
    int               log2GARTTexGran;

				/* CP accleration */
    drmBufPtr         indirectBuffer;
    int               indirectStart;

				/* DRI screen private data */
    int               fbX;
    int               fbY;
    int               backX;
    int               backY;
    int               depthX;
    int               depthY;

    int               frontOffset;
    int               frontPitch;
    int               backOffset;
    int               backPitch;
    int               depthOffset;
    int               depthPitch;
    int               depthBits;
    int               textureOffset;
    int               textureSize;
    int               log2TexGran;

    int               pciGartSize;
    CARD32            pciGartOffset;
    void              *pciGartBackup;
#ifdef USE_XAA
    CARD32            frontPitchOffset;
    CARD32            backPitchOffset;
    CARD32            depthPitchOffset;

				/* offscreen memory management */
    int               backLines;
    FBAreaPtr         backArea;
    int               depthTexLines;
    FBAreaPtr         depthTexArea;
#endif

				/* Saved scissor values */
    CARD32            sc_left;
    CARD32            sc_right;
    CARD32            sc_top;
    CARD32            sc_bottom;

    CARD32            re_top_left;
    CARD32            re_width_height;

    CARD32            aux_sc_cntl;

    int               irq;

    Bool              DMAForXv;

#ifdef PER_CONTEXT_SAREA
    int               perctx_sarea_size;
#endif

    /* Debugging info for BEGIN_RING/ADVANCE_RING pairs. */
    int               dma_begin_count;
    char              *dma_debug_func;
    int               dma_debug_lineno;
#endif /* XF86DRI */

				/* XVideo */
    XF86VideoAdaptorPtr adaptor;
    void              (*VideoTimerCallback)(ScrnInfoPtr, Time);
    int               videoKey;
    int		      RageTheatreCrystal;
    int               RageTheatreTunerPort;
    int               RageTheatreCompositePort;
    int               RageTheatreSVideoPort;
    int               tunerType;
	char*			RageTheatreMicrocPath;
	char*			RageTheatreMicrocType;
    Bool               MM_TABLE_valid;
    struct {
    	CARD8 table_revision;
	CARD8 table_size;
        CARD8 tuner_type;
        CARD8 audio_chip;
        CARD8 product_id;
        CARD8 tuner_voltage_teletext_fm;
        CARD8 i2s_config; /* configuration of the sound chip */
        CARD8 video_decoder_type;
        CARD8 video_decoder_host_config;
        CARD8 input[5];
    	} MM_TABLE;
    CARD16 video_decoder_type;
    int overlay_scaler_buffer_width;
    int ecp_div;

    /* Render */
    Bool              RenderAccel;
    unsigned short    texW[2];
    unsigned short    texH[2];
#ifdef USE_XAA
    FBLinearPtr       RenderTex;
    void              (*RenderCallback)(ScrnInfoPtr);
    Time              RenderTimeout;
#endif

    /* general */
    Bool              showCache;
    OptionInfoPtr     Options;

    Bool              useEXA;
#ifdef XFree86LOADER
#ifdef USE_EXA
    XF86ModReqInfo    exaReq;
#endif
#ifdef USE_XAA
    XF86ModReqInfo    xaaReq;
#endif
#endif

    /* X itself has the 3D context */
    Bool              XInited3D;

    DisplayModePtr currentMode, savedCurrentMode;

    /* special handlings for DELL triple-head server */
    Bool		IsDellServer; 

    Bool               VGAAccess;

    int                MaxSurfaceWidth;
    int                MaxLines;

    CARD32            tv_dac_adj;
    CARD32            tv_dac_enable_mask;

    Bool want_vblank_interrupts;
    RADEONBIOSConnector BiosConnector[RADEON_MAX_BIOS_CONNECTOR];
    RADEONBIOSInitTable BiosTable;

    /* save crtc state for console restore */
    Bool              crtc_on;
    Bool              crtc2_on;

    Bool              InternalTVOut;
    int               tvdac_use_count;

#if defined(__powerpc__)
    RADEONMacModel    MacModel;
#endif
    RADEONExtTMDSChip ext_tmds_chip;

    atomBiosHandlePtr atomBIOS;
    unsigned long FbFreeStart, FbFreeSize;
    unsigned char*      BIOSCopy;

    Rotation rotation;
    void (*PointerMoved)(int, int, int);
    CreateScreenResourcesProcPtr CreateScreenResources;
} RADEONInfoRec, *RADEONInfoPtr;

#define RADEONWaitForFifo(pScrn, entries)				\
do {									\
    if (info->fifo_slots < entries)					\
	RADEONWaitForFifoFunction(pScrn, entries);			\
    info->fifo_slots -= entries;					\
} while (0)

extern RADEONEntPtr RADEONEntPriv(ScrnInfoPtr pScrn);
extern void        RADEONWaitForFifoFunction(ScrnInfoPtr pScrn, int entries);
extern void        RADEONWaitForIdleMMIO(ScrnInfoPtr pScrn);
#ifdef XF86DRI
extern int RADEONDRISetParam(ScrnInfoPtr pScrn, unsigned int param, int64_t value);
extern void        RADEONWaitForIdleCP(ScrnInfoPtr pScrn);
#endif

extern void        RADEONDoAdjustFrame(ScrnInfoPtr pScrn, int x, int y,
				       Bool clone);

extern void        RADEONEngineReset(ScrnInfoPtr pScrn);
extern void        RADEONEngineFlush(ScrnInfoPtr pScrn);
extern void        RADEONEngineRestore(ScrnInfoPtr pScrn);

extern unsigned    RADEONINPLL(ScrnInfoPtr pScrn, int addr);
extern void        RADEONOUTPLL(ScrnInfoPtr pScrn, int addr, CARD32 data);

extern unsigned    RADEONINMC(ScrnInfoPtr pScrn, int addr);
extern void        RADEONOUTMC(ScrnInfoPtr pScrn, int addr, CARD32 data);

extern void        RADEONWaitForVerticalSync(ScrnInfoPtr pScrn);
extern void        RADEONWaitForVerticalSync2(ScrnInfoPtr pScrn);

extern void        RADEONChangeSurfaces(ScrnInfoPtr pScrn);

extern Bool        RADEONAccelInit(ScreenPtr pScreen);
#ifdef USE_EXA
extern Bool        RADEONSetupMemEXA (ScreenPtr pScreen);
extern Bool        RADEONDrawInitMMIO(ScreenPtr pScreen);
#ifdef XF86DRI
extern unsigned long long RADEONTexOffsetStart(PixmapPtr pPix);
extern Bool        RADEONGetDatatypeBpp(int bpp, CARD32 *type);
extern Bool        RADEONGetPixmapOffsetPitch(PixmapPtr pPix,
					      CARD32 *pitch_offset);
extern Bool        RADEONDrawInitCP(ScreenPtr pScreen);
extern void        RADEONDoPrepareCopyCP(ScrnInfoPtr pScrn,
					 CARD32 src_pitch_offset,
					 CARD32 dst_pitch_offset,
					 CARD32 datatype, int rop,
					 Pixel planemask);
extern void        RADEONCopyCP(PixmapPtr pDst, int srcX, int srcY, int dstX,
				int dstY, int w, int h);
#endif
#endif
#ifdef USE_XAA
extern void        RADEONAccelInitMMIO(ScreenPtr pScreen, XAAInfoRecPtr a);
#endif
extern void        RADEONEngineInit(ScrnInfoPtr pScrn);
extern Bool        RADEONCursorInit(ScreenPtr pScreen);
extern Bool        RADEONDGAInit(ScreenPtr pScreen);

extern void        RADEONInit3DEngine(ScrnInfoPtr pScrn);

extern int         RADEONMinBits(int val);

extern void        RADEONInitVideo(ScreenPtr pScreen);
extern void        RADEONResetVideo(ScrnInfoPtr pScrn);
extern void        R300CGWorkaround(ScrnInfoPtr pScrn);

extern void        RADEONPllErrataAfterIndex(RADEONInfoPtr info);
extern void        RADEONPllErrataAfterData(RADEONInfoPtr info);

extern Bool        RADEONGetBIOSInfo(ScrnInfoPtr pScrn, xf86Int10InfoPtr pInt10);
extern Bool        RADEONGetConnectorInfoFromBIOS (ScrnInfoPtr pScrn);
extern Bool        RADEONGetClockInfoFromBIOS (ScrnInfoPtr pScrn);
extern Bool        RADEONGetLVDSInfoFromBIOS (xf86OutputPtr output);
extern Bool        RADEONGetTMDSInfoFromBIOS (xf86OutputPtr output);
extern Bool        RADEONGetTVInfoFromBIOS (xf86OutputPtr output);
extern Bool        RADEONGetHardCodedEDIDFromBIOS (xf86OutputPtr output);

extern void        RADEONRestoreMemMapRegisters(ScrnInfoPtr pScrn,
						RADEONSavePtr restore);
extern void        RADEONRestoreCommonRegisters(ScrnInfoPtr pScrn,
						RADEONSavePtr restore);
extern void        RADEONRestoreCrtcRegisters(ScrnInfoPtr pScrn,
					      RADEONSavePtr restore);
extern void        RADEONRestoreDACRegisters(ScrnInfoPtr pScrn,
					     RADEONSavePtr restore);
extern void        RADEONRestoreFPRegisters(ScrnInfoPtr pScrn,
					    RADEONSavePtr restore);
extern void        RADEONRestoreFP2Registers(ScrnInfoPtr pScrn,
					     RADEONSavePtr restore);
extern void        RADEONRestoreLVDSRegisters(ScrnInfoPtr pScrn,
					      RADEONSavePtr restore);
extern void        RADEONRestoreBIOSRegisters(ScrnInfoPtr pScrn,
					      RADEONSavePtr restore);
extern void        RADEONRestoreRMXRegisters(ScrnInfoPtr pScrn,
					     RADEONSavePtr restore);
extern void        RADEONRestorePLLRegisters(ScrnInfoPtr pScrn,
					     RADEONSavePtr restore);
extern void        RADEONRestoreCrtc2Registers(ScrnInfoPtr pScrn,
					       RADEONSavePtr restore);
extern void        RADEONRestorePLL2Registers(ScrnInfoPtr pScrn,
					      RADEONSavePtr restore);

extern void        RADEONInitMemMapRegisters(ScrnInfoPtr pScrn,
					     RADEONSavePtr save,
					     RADEONInfoPtr info);
extern void        RADEONInitDispBandwidth(ScrnInfoPtr pScrn);
extern Bool        RADEONI2cInit(ScrnInfoPtr pScrn);
extern void        RADEONSetSyncRangeFromEdid(ScrnInfoPtr pScrn, int flag);
extern Bool        RADEONSetupConnectors(ScrnInfoPtr pScrn);
extern void        RADEONPrintPortMap(ScrnInfoPtr pScrn);
extern void        RADEONEnableDisplay(xf86OutputPtr pPort, BOOL bEnable);
extern void        RADEONDisableDisplays(ScrnInfoPtr pScrn);
extern void        RADEONGetPanelInfo(ScrnInfoPtr pScrn);
extern void        RADEONGetTVDacAdjInfo(xf86OutputPtr output);
extern void        RADEONUnblank(ScrnInfoPtr pScrn);
extern void        RADEONUnblank(ScrnInfoPtr pScrn);
extern void        RADEONBlank(ScrnInfoPtr pScrn);
extern void        RADEONDisplayPowerManagementSet(ScrnInfoPtr pScrn,
						   int PowerManagementMode,
						   int flags);
extern Bool RADEONAllocateControllers(ScrnInfoPtr pScrn);
extern Bool RADEONAllocateConnectors(ScrnInfoPtr pScrn);
extern int RADEONValidateMergeModes(ScrnInfoPtr pScrn);
extern int RADEONValidateDDCModes(ScrnInfoPtr pScrn1, char **ppModeName,
				  RADEONMonitorType DisplayType, int crtc2);
extern void RADEONSetPitch (ScrnInfoPtr pScrn);
extern void RADEONUpdateHVPosition(xf86OutputPtr output, DisplayModePtr mode);

DisplayModePtr
RADEONProbeOutputModes(xf86OutputPtr output);
extern Bool RADEONInit2(ScrnInfoPtr pScrn, DisplayModePtr crtc1,
			DisplayModePtr crtc2, int crtc_mask,
			RADEONSavePtr save, RADEONMonitorType montype);

extern Bool
RADEONDVOReadByte(I2CDevPtr dvo, int addr, CARD8 *ch);
extern Bool
RADEONDVOWriteByte(I2CDevPtr dvo, int addr, CARD8 ch);
extern Bool
RADEONGetExtTMDSInfoFromBIOS (xf86OutputPtr output);
extern Bool
RADEONInitExtTMDSInfoFromBIOS (xf86OutputPtr output);

void
radeon_crtc_set_cursor_position (xf86CrtcPtr crtc, int x, int y);
void
radeon_crtc_show_cursor (xf86CrtcPtr crtc);
void
radeon_crtc_hide_cursor (xf86CrtcPtr crtc);
void
radeon_crtc_set_cursor_position (xf86CrtcPtr crtc, int x, int y);
void
radeon_crtc_set_cursor_colors (xf86CrtcPtr crtc, int bg, int fg);
void
radeon_crtc_load_cursor_argb (xf86CrtcPtr crtc, CARD32 *image);
void
RADEONEnableOutputs(ScrnInfoPtr pScrn, int crtc_num);

extern void RADEONAdjustCrtcRegistersForTV(ScrnInfoPtr pScrn, RADEONSavePtr save,
					   DisplayModePtr mode, xf86OutputPtr output);
extern void RADEONAdjustPLLRegistersForTV(ScrnInfoPtr pScrn, RADEONSavePtr save,
					  DisplayModePtr mode, xf86OutputPtr output);
extern void RADEONAdjustCrtc2RegistersForTV(ScrnInfoPtr pScrn, RADEONSavePtr save,
					   DisplayModePtr mode, xf86OutputPtr output);
extern void RADEONAdjustPLL2RegistersForTV(ScrnInfoPtr pScrn, RADEONSavePtr save,
					  DisplayModePtr mode, xf86OutputPtr output);
extern void RADEONInitTVRegisters(xf86OutputPtr output, RADEONSavePtr save,
                                  DisplayModePtr mode, BOOL IsPrimary);

extern void RADEONRestoreTVRegisters(ScrnInfoPtr pScrn, RADEONSavePtr restore);
extern void RADEONRestoreTVRestarts(ScrnInfoPtr pScrn, RADEONSavePtr restore);
extern void RADEONRestoreTVTimingTables(ScrnInfoPtr pScrn, RADEONSavePtr restore);

#ifdef XF86DRI
#ifdef USE_XAA
extern void        RADEONAccelInitCP(ScreenPtr pScreen, XAAInfoRecPtr a);
#endif
extern Bool        RADEONDRIGetVersion(ScrnInfoPtr pScrn);
extern Bool        RADEONDRIScreenInit(ScreenPtr pScreen);
extern void        RADEONDRICloseScreen(ScreenPtr pScreen);
extern void        RADEONDRIResume(ScreenPtr pScreen);
extern Bool        RADEONDRIFinishScreenInit(ScreenPtr pScreen);
extern void        RADEONDRIAllocatePCIGARTTable(ScreenPtr pScreen);
extern int         RADEONDRIGetPciAperTableSize(ScrnInfoPtr pScrn);
extern void        RADEONDRIStop(ScreenPtr pScreen);

extern drmBufPtr   RADEONCPGetBuffer(ScrnInfoPtr pScrn);
extern void        RADEONCPFlushIndirect(ScrnInfoPtr pScrn, int discard);
extern void        RADEONCPReleaseIndirect(ScrnInfoPtr pScrn);
extern int         RADEONCPStop(ScrnInfoPtr pScrn,  RADEONInfoPtr info);
extern Bool RADEONDRISetVBlankInterrupt(ScrnInfoPtr pScrn, Bool on);

extern void        RADEONHostDataParams(ScrnInfoPtr pScrn, CARD8 *dst,
					CARD32 pitch, int cpp,
					CARD32 *dstPitchOffset, int *x, int *y);
extern CARD8*      RADEONHostDataBlit(ScrnInfoPtr pScrn, unsigned int cpp,
				      unsigned int w, CARD32 dstPitchOff,
				      CARD32 *bufPitch, int x, int *y,
				      unsigned int *h, unsigned int *hpass);
extern void        RADEONHostDataBlitCopyPass(ScrnInfoPtr pScrn,
					      unsigned int bpp,
					      CARD8 *dst, CARD8 *src,
					      unsigned int hpass,
					      unsigned int dstPitch,
					      unsigned int srcPitch);
extern void        RADEONCopySwap(CARD8 *dst, CARD8 *src, unsigned int size,
				  int swap);

#define RADEONCP_START(pScrn, info)					\
do {									\
    int _ret = drmCommandNone(info->drmFD, DRM_RADEON_CP_START);	\
    if (_ret) {								\
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,				\
		   "%s: CP start %d\n", __FUNCTION__, _ret);		\
    }									\
    info->CPStarted = TRUE;                                             \
} while (0)

#define RADEONCP_RELEASE(pScrn, info)					\
do {									\
    if (info->CPInUse) {						\
	RADEON_PURGE_CACHE();						\
	RADEON_WAIT_UNTIL_IDLE();					\
	RADEONCPReleaseIndirect(pScrn);					\
	info->CPInUse = FALSE;						\
    }									\
} while (0)

#define RADEONCP_STOP(pScrn, info)					\
do {									\
    int _ret;								\
     if (info->CPStarted) {						\
        _ret = RADEONCPStop(pScrn, info);				\
        if (_ret) {							\
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,			\
		   "%s: CP stop %d\n", __FUNCTION__, _ret);		\
        }								\
        info->CPStarted = FALSE;                                        \
   }									\
    RADEONEngineRestore(pScrn);						\
    info->CPRuns = FALSE;						\
} while (0)

#define RADEONCP_RESET(pScrn, info)					\
do {									\
    if (RADEONCP_USE_RING_BUFFER(info->CPMode)) {			\
	int _ret = drmCommandNone(info->drmFD, DRM_RADEON_CP_RESET);	\
	if (_ret) {							\
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,			\
		       "%s: CP reset %d\n", __FUNCTION__, _ret);	\
	}								\
    }									\
} while (0)

#define RADEONCP_REFRESH(pScrn, info)					\
do {									\
    if (!info->CPInUse) {						\
	if (info->needCacheFlush) {					\
	    RADEON_PURGE_CACHE();					\
	    RADEON_PURGE_ZCACHE();					\
	    info->needCacheFlush = FALSE;				\
	}								\
	RADEON_WAIT_UNTIL_IDLE();					\
	BEGIN_RING(6);							\
	OUT_RING_REG(RADEON_RE_TOP_LEFT,     info->re_top_left);	\
	OUT_RING_REG(RADEON_RE_WIDTH_HEIGHT, info->re_width_height);	\
	OUT_RING_REG(RADEON_AUX_SC_CNTL,     info->aux_sc_cntl);	\
	ADVANCE_RING();							\
	info->CPInUse = TRUE;						\
    }									\
} while (0)


#define CP_PACKET0(reg, n)						\
	(RADEON_CP_PACKET0 | ((n) << 16) | ((reg) >> 2))
#define CP_PACKET1(reg0, reg1)						\
	(RADEON_CP_PACKET1 | (((reg1) >> 2) << 11) | ((reg0) >> 2))
#define CP_PACKET2()							\
	(RADEON_CP_PACKET2)
#define CP_PACKET3(pkt, n)						\
	(RADEON_CP_PACKET3 | (pkt) | ((n) << 16))


#define RADEON_VERBOSE	0

#define RING_LOCALS	CARD32 *__head = NULL; int __expected; int __count = 0

#define BEGIN_RING(n) do {						\
    if (RADEON_VERBOSE) {						\
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,				\
		   "BEGIN_RING(%d) in %s\n", (unsigned int)n, __FUNCTION__);\
    }									\
    if (++info->dma_begin_count != 1) {					\
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,				\
		   "BEGIN_RING without end at %s:%d\n",			\
		   info->dma_debug_func, info->dma_debug_lineno);	\
	info->dma_begin_count = 1;					\
    }									\
    info->dma_debug_func = __FILE__;					\
    info->dma_debug_lineno = __LINE__;					\
    if (!info->indirectBuffer) {					\
	info->indirectBuffer = RADEONCPGetBuffer(pScrn);		\
	info->indirectStart = 0;					\
    } else if (info->indirectBuffer->used + (n) * (int)sizeof(CARD32) >	\
	       info->indirectBuffer->total) {				\
	RADEONCPFlushIndirect(pScrn, 1);				\
    }									\
    __expected = n;							\
    __head = (pointer)((char *)info->indirectBuffer->address +		\
		       info->indirectBuffer->used);			\
    __count = 0;							\
} while (0)

#define ADVANCE_RING() do {						\
    if (info->dma_begin_count-- != 1) {					\
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,				\
		   "ADVANCE_RING without begin at %s:%d\n",		\
		   __FILE__, __LINE__);					\
	info->dma_begin_count = 0;					\
    }									\
    if (__count != __expected) {					\
	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,				\
		   "ADVANCE_RING count != expected (%d vs %d) at %s:%d\n", \
		   __count, __expected, __FILE__, __LINE__);		\
    }									\
    if (RADEON_VERBOSE) {						\
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,				\
		   "ADVANCE_RING() start: %d used: %d count: %d\n",	\
		   info->indirectStart,					\
		   info->indirectBuffer->used,				\
		   __count * (int)sizeof(CARD32));			\
    }									\
    info->indirectBuffer->used += __count * (int)sizeof(CARD32);	\
} while (0)

#define OUT_RING(x) do {						\
    if (RADEON_VERBOSE) {						\
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,				\
		   "   OUT_RING(0x%08x)\n", (unsigned int)(x));		\
    }									\
    __head[__count++] = (x);						\
} while (0)

#define OUT_RING_REG(reg, val)						\
do {									\
    OUT_RING(CP_PACKET0(reg, 0));					\
    OUT_RING(val);							\
} while (0)

#define FLUSH_RING()							\
do {									\
    if (RADEON_VERBOSE)							\
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,				\
		   "FLUSH_RING in %s\n", __FUNCTION__);			\
    if (info->indirectBuffer) {						\
	RADEONCPFlushIndirect(pScrn, 0);				\
    }									\
} while (0)


#define RADEON_WAIT_UNTIL_2D_IDLE()					\
do {									\
    BEGIN_RING(2);							\
    OUT_RING(CP_PACKET0(RADEON_WAIT_UNTIL, 0));				\
    OUT_RING((RADEON_WAIT_2D_IDLECLEAN |				\
	      RADEON_WAIT_HOST_IDLECLEAN));				\
    ADVANCE_RING();							\
} while (0)

#define RADEON_WAIT_UNTIL_3D_IDLE()					\
do {									\
    BEGIN_RING(2);							\
    OUT_RING(CP_PACKET0(RADEON_WAIT_UNTIL, 0));				\
    OUT_RING((RADEON_WAIT_3D_IDLECLEAN |				\
	      RADEON_WAIT_HOST_IDLECLEAN));				\
    ADVANCE_RING();							\
} while (0)

#define RADEON_WAIT_UNTIL_IDLE()					\
do {									\
    if (RADEON_VERBOSE) {						\
	xf86DrvMsg(pScrn->scrnIndex, X_INFO,				\
		   "WAIT_UNTIL_IDLE() in %s\n", __FUNCTION__);		\
    }									\
    BEGIN_RING(2);							\
    OUT_RING(CP_PACKET0(RADEON_WAIT_UNTIL, 0));				\
    OUT_RING((RADEON_WAIT_2D_IDLECLEAN |				\
	      RADEON_WAIT_3D_IDLECLEAN |				\
	      RADEON_WAIT_HOST_IDLECLEAN));				\
    ADVANCE_RING();							\
} while (0)

#define RADEON_PURGE_CACHE()						\
do {									\
    BEGIN_RING(2);							\
    OUT_RING(CP_PACKET0(RADEON_RB3D_DSTCACHE_CTLSTAT, 0));		\
    OUT_RING(RADEON_RB3D_DC_FLUSH_ALL);					\
    ADVANCE_RING();							\
} while (0)

#define RADEON_PURGE_ZCACHE()						\
do {									\
    OUT_RING(CP_PACKET0(RADEON_RB3D_ZCACHE_CTLSTAT, 0));		\
    OUT_RING(RADEON_RB3D_ZC_FLUSH_ALL);					\
} while (0)

#endif /* XF86DRI */

static __inline__ void RADEON_MARK_SYNC(RADEONInfoPtr info, ScrnInfoPtr pScrn)
{
#ifdef USE_EXA
    if (info->useEXA)
	exaMarkSync(pScrn->pScreen);
#endif
#ifdef USE_XAA
    if (!info->useEXA)
	SET_SYNC_FLAG(info->accel);
#endif
}

static __inline__ void RADEON_SYNC(RADEONInfoPtr info, ScrnInfoPtr pScrn)
{
#ifdef USE_EXA
    if (info->useEXA)
	exaWaitSync(pScrn->pScreen);
#endif
#ifdef USE_XAA
    if (!info->useEXA && info->accel)
	info->accel->Sync(pScrn);
#endif
}

static __inline__ void radeon_init_timeout(struct timeval *endtime,
    unsigned int timeout)
{
    gettimeofday(endtime, NULL);
    endtime->tv_usec += timeout;
    endtime->tv_sec += endtime->tv_usec / 1000000;
    endtime->tv_usec %= 1000000;
}

static __inline__ int radeon_timedout(const struct timeval *endtime)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec == endtime->tv_sec ?
        now.tv_usec > endtime->tv_usec : now.tv_sec > endtime->tv_sec;
}

#endif /* _RADEON_H_ */
