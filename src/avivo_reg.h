/* AVIVO registers are specific to the AVIVO display engine, first
 * introduced on R5xx.
 *
 * CRTCs are the master unit.  A CRTC controls scanout, and both the
 * DACs (be it TV or VGA) and TMDS transmitters take their input from
 * the CRTC.
 */

/* Core engine. */
#define AVIVO_ENGINE_STATUS					0x0014

/* Memory mapping. */
#define AVIVO_MC_INDEX						0x0070
/* AVIVO_MC_MEMORY_MAP control memory mapping of the video card ram: 
 * base is higher 16bits of the starting address at which card
 * sees it own memory. end is higher 16bits address at which card
 * memory should end (might be usefull if you want to use half memory
 * but don't know what does the card if you address out of bound
 * memory, likely trigger pci stuff which often end in bad things).
 */
#	define MC00									0x00
#define R520_MC_STATUS 0x00
#define R520_MC_STATUS_IDLE (1<<1)
#	define MC01									0x01
#	define MC02									0x02
#	define MC03									0x03
#	define AVIVO_MC_MEMORY_MAP					0x04
#		define AVIVO_MC_MEMORY_MAP_BASE_MASK		(0xFFFF << 0)
#		define AVIVO_MC_MEMORY_MAP_BASE_SHIFT		0
#		define AVIVO_MC_MEMORY_MAP_END_MASK			(0xFFFF << 16)
#		define AVIVO_MC_MEMORY_MAP_END_SHIFT		16
#	define MC05									0x05
#	define MC06									0x06
#	define MC07									0x07
#	define MC08									0x08
#define RV515_MC_STATUS 0x08
#define RV515_MC_STATUS_IDLE (1<<4)
#	define MC09									0x09
#	define MC0a									0x0a
#	define MC0b									0x0b
#	define MC0c									0x0c
#	define MC0d									0x0d
#	define MC0e									0x0e
#	define MC0f									0x0f
#	define MC10									0x10
#	define MC11									0x11
#	define MC12									0x12
#	define MC13									0x13
#	define MC14									0x14
#	define MC15									0x15
#	define MC16									0x16
#	define MC17									0x17
#	define MC18									0x18
#	define MC19									0x19
#	define MC1a									0x1a
#	define MC1b									0x1b
#	define MC1c									0x1c
#	define MC1d									0x1d
#	define MC1e									0x1e
#	define MC1f									0x1f
#define AVIVO_MC_DATA						0x0074

/*
 * You set memory base at which card see its memory (should be the
 * same as AVIVO_MC_MEMORY_MAP lower 16bits
 */
#define AVIVO_VGA_MEMORY_BASE				0x0134
#define AVIVO_VGA_FB_START					0x0310
#define AVIVO_VGA1_CONTROL					0x0330
 #define AVIVO_VGA1_CONTROL_MODE_ENABLE (1<<0)
 #define AVIVO_VGA1_CONTROL_TIMING_SELECT (1<<8)
 #define AVIVO_VGA1_CONTROL_SYNC_POLARITY_SELECT (1<<9)
 #define AVIVO_VGA1_CONTROL_OVERSCAN_TIMING_SELECT (1<<10)
 #define AVIVO_VGA1_CONTROL_OVERSCAN_COLOR_EN (1<<16)
 #define AVIVO_VGA1_CONTROL_ROTATE (1<<24)
#define AVIVO_VGA2_CONTROL					0x0338
 #define AVIVO_VGA2_CONTROL_MODE_ENABLE (1<<0)
 #define AVIVO_VGA2_CONTROL_TIMING_SELECT (1<<8)
 #define AVIVO_VGA2_CONTROL_SYNC_POLARITY_SELECT (1<<9)
 #define AVIVO_VGA2_CONTROL_OVERSCAN_TIMING_SELECT (1<<10)
 #define AVIVO_VGA2_CONTROL_OVERSCAN_COLOR_EN (1<<16)
 #define AVIVO_VGA2_CONTROL_ROTATE (1<<24)

/*
 * We believe reference clock is 108Mhz, we likely can change that using
 * mystery PLL reg spoted below more dump are needed in order to find out.
 *
 * The formula we derived so far seems to work for card we have:
 * (vclk is video mode clock)
 * vclk = (1080 * AVIVO_PLL_POST_MUL) /
 *        (AVIVO_PLL_DIVIDER * AVIVO_PLL_POST_DIV * 40)
 * It seems that AVIVO_PLL_DIVIDER * AVIVO_PLL_POST_DIV needs to be
 * above 40 and that AVIVO_DIVIDER should be greater than AVIVO_PLL_POST_DIV
 * Try to keep this constraint while computing PLL values.
 */
#define AVIVO_PLL1_POST_DIV_CNTL		0x0400 // REF_DIV_SRC
#	define AVIVO_PLL_POST_DIV_EN			(1 << 0)
#define AVIVO_PLL1_POST_DIV			0x0404 // REF_DIV
#define AVIVO_PLL1_POST_DIV_MYSTERY		0x040C
#	define AVIVO_PLL_POST_DIV_MYSTERY_VALUE		0x10000
#define AVIVO_PLL2_POST_DIV_CNTL		0x0410
#define AVIVO_PLL2_POST_DIV			0x0414
#define AVIVO_PLL2_POST_DIV_MYSTERY		0x041C
#define AVIVO_PLL1_POST_MUL			0x0430 // FB_DIV
#	define AVIVO_PLL_POST_MUL_SHIFT			16
#define AVIVO_PLL2_POST_MUL			0x0434 // FB_DIV
#define AVIVO_PLL1_DIVIDER_CNTL			0x0438 // POST DIV_SRC
#	define AVIVO_PLL_DIVIDER_EN			(1 << 0)
#define AVIVO_PLL1_DIVIDER			0x043C // POST DIV
#define AVIVO_PLL2_DIVIDER_CNTL			0x0440
#define AVIVO_PLL2_DIVIDER			0x0444 // POST_DIV
#define AVIVO_PLL1_MYSTERY0			0x0448 // POST_DIV_SRC
#	define AVIVO_PLL_MYSTERY0_VALUE			0x20704
#define AVIVO_PLL2_MYSTERY0			0x044C
#define AVIVO_PLL1_MYSTERY1			0x0450
#	define AVIVO_PLL_MYSTERY1_VALUE			0x4310000
#define AVIVO_PLL2_MYSTERY1			0x0454
#define AVIVO_CRTC_PLL_SOURCE			0x0484
#	define AVIVO_CRTC1_PLL_SOURCE_SHIFT		0
#	define AVIVO_CRTC2_PLL_SOURCE_SHIFT		16

/* CRTC controls; these appear to influence the DAC's scanout. */
#define AVIVO_CRTC1_H_TOTAL					0x6000
#define AVIVO_CRTC1_H_BLANK					0x6004
#define AVIVO_CRTC1_H_SYNC_WID				0x6008
#define AVIVO_CRTC1_H_SYNC_POL				0x600c
#define AVIVO_CRTC1_V_TOTAL					0x6020
#define AVIVO_CRTC1_V_BLANK					0x6024
#define AVIVO_CRTC1_V_SYNC_WID				0x6028
#define AVIVO_CRTC1_V_SYNC_POL				0x602c
#define AVIVO_CRTC1_CNTL					0x6080
#	define AVIVO_CRTC_EN						(1 << 0)
#define AVIVO_CRTC1_BLANK_STATUS			0x6084
#define AVIVO_CRTC1_STEREO_STATUS			0x60c0

/* These all appear to control the scanout from the framebuffer.
 * Flicking SCAN_ENABLE low results in a black screen -- aside from
 * the cursor.  Messing with PITCH gives you the obvious symptoms,
 * and messing with X_LENGTH and Y_LENGTH will give you a black
 * screen beyond those bounds if you make it shorter.
 *
 * Messing with the format gives you ... odd results.  Setting it
 * to 3 exactly quadruples my display size, with the next three
 * panes displaying the next parts of FB memory.  BPP?
 *
 * FB_LOCATION gives me the obvious result; FB_END is exactly
 * FB_LOCATION + (xres * yres * 2).  FB_END doesn't appear to actually
 * function as an upper bound.
 */
#define AVIVO_CRTC1_SCAN_ENABLE				0x6100
#	define AVIVO_CRTC_SCAN_EN					(1 << 0)
#define AVIVO_CRTC1_FB_FORMAT				0x6104
#	define AVIVO_CRTC_FORMAT_ARGB15				(1 << 0)
#	define AVIVO_CRTC_FORMAT_ARGB16				((1 << 0) | (1 << 8))
#	define AVIVO_CRTC_FORMAT_ARGB32				(1 << 1)
#	define AVIVO_CRTC_TILED                                 (1 << 20)
#	define AVIVO_CRTC_MACRO_ADDRESS_MODE                   (1 << 21)
#define AVIVO_CRTC1_FB_LOCATION				0x6110
#define AVIVO_CRTC1_FB_END					0x6118
/* This is in pixels, not bytes.  Obviously. */
#define AVIVO_CRTC1_PITCH					0x6120
#define AVIVO_CRTC1_X_LENGTH				0x6134
#define AVIVO_CRTC1_Y_LENGTH				0x6138

#define AVIVO_CRTC1_OFFSET_END				0x6454

#define AVIVO_CRTC1_FB_HEIGHT			   	0x652c
#define AVIVO_CRTC1_OFFSET_START            0x6580
#define AVIVO_CRTC1_EXPANSION_SOURCE		0x6584
#define AVIVO_CRTC1_EXPANSION_CNTL			0x6590
#	define AVIVO_CRTC_EXPANSION_EN				(1 << 0)
#define AVIVO_CRTC1_6594					0x6594
#	define AVIVO_CRTC1_6594_VALUE				((1 << 8) | (1 << 0))
#define AVIVO_CRTC1_659C					0x659C
#	define AVIVO_CRTC1_659C_VALUE				((1 << 1))
#define AVIVO_CRTC1_65A4					0x65a4
#	define AVIVO_CRTC1_65A4_VALUE				((1 << 16) | (1 << 0))
#define AVIVO_CRTC1_65A8					0x65a8
#	define AVIVO_CRTC1_65A8_VALUE				((1 << 16) | (1 << 14))
#define AVIVO_CRTC1_65AC					0x65ac
#	define AVIVO_CRTC1_65AC_VALUE				((1 << 15) | (1 << 14) | (1 << 13))
#define AVIVO_CRTC1_65B0					0x65b0
#	define AVIVO_CRTC1_65B0_VALUE				((1 << 17) | (1 << 16) | (1 << 8))
#define AVIVO_CRTC1_65B8					0x65b8
#	define AVIVO_CRTC1_65B8_VALUE				((1 << 16))
#define AVIVO_CRTC1_65BC					0x65bc
#	define AVIVO_CRTC1_65BC_VALUE				((1 << 16))
#define AVIVO_CRTC1_65C0					0x65c0
#	define AVIVO_CRTC1_65C0_VALUE				((1 << 17) | (1 << 16) | (1 << 8))
#define AVIVO_CRTC1_65C8					0x65c8
#	define AVIVO_CRTC1_65C8_VALUE				((1 << 16))
 
#define AVIVO_CRTC2_H_TOTAL					0x6800
#define AVIVO_CRTC2_H_BLANK					0x6804
#define AVIVO_CRTC2_H_SYNC_WID				0x6808
#define AVIVO_CRTC2_H_SYNC_POL				0x680c
#define AVIVO_CRTC2_V_TOTAL					0x6820
#define AVIVO_CRTC2_V_BLANK					0x6824
#define AVIVO_CRTC2_V_SYNC_WID				0x6828
#define AVIVO_CRTC2_V_SYNC_POL				0x682c
#define AVIVO_CRTC2_CNTL					0x6880
#define AVIVO_CRTC2_BLANK_STATUS			0x6884

#define AVIVO_CRTC2_SCAN_ENABLE				0x6900
#define AVIVO_CRTC2_FB_FORMAT				0x6904
#define AVIVO_CRTC2_FB_LOCATION				0x6910
#define AVIVO_CRTC2_FB_END					0x6918
#define AVIVO_CRTC2_PITCH					0x6920
#define AVIVO_CRTC2_X_LENGTH				0x6934
#define AVIVO_CRTC2_Y_LENGTH				0x6938

#define AVIVO_CRTC2_OFFSET_END				0x6c54

#define AVIVO_CRTC2_FB_HEIGHT			   	0x6d2c
#define AVIVO_CRTC2_OFFSET_START			0x6d80
#define AVIVO_CRTC2_EXPANSION_SOURCE		0x6d84
#define AVIVO_CRTC2_EXPANSION_CNTL			0x6d90
#define AVIVO_CRTC2_6594					0x6d94
#define AVIVO_CRTC2_659C					0x6d9C
#define AVIVO_CRTC2_65A4					0x6da4
#define AVIVO_CRTC2_65A8					0x6da8
#define AVIVO_CRTC2_65AC					0x6dac
#define AVIVO_CRTC2_65B0					0x6db0
#define AVIVO_CRTC2_65B8					0x6db8
#define AVIVO_CRTC2_65BC					0x6dbc
#define AVIVO_CRTC2_65C0					0x6dc0
#define AVIVO_CRTC2_65C8					0x6dc8

#define AVIVO_DACA_CNTL						0x7800
#define AVIVO_DACA_CRTC_SOURCE				0x7804
#	define AVIVO_DAC_EN							(1 << 0)
#define AVIVO_DACA_FORCE_OUTPUT_CNTL				0x783c
# define AVIVO_DACA_FORCE_OUTPUT_CNTL_FORCE_DATA_EN             (1 << 0)
# define AVIVO_DACA_FORCE_OUTPUT_CNTL_DATA_SEL_SHIFT            (8)
# define AVIVO_DACA_FORCE_OUTPUT_CNTL_DATA_SEL_BLUE             (1 << 0)
# define AVIVO_DACA_FORCE_OUTPUT_CNTL_DATA_SEL_GREEN            (1 << 1)
# define AVIVO_DACA_FORCE_OUTPUT_CNTL_DATA_SEL_RED              (1 << 2)
# define AVIVO_DACA_FORCE_OUTPUT_CNTL_DATA_ON_BLANKB_ONLY       (1 << 24)
#define AVIVO_DACA_POWERDOWN					0x7850
# define AVIVO_DACA_POWERDOWN_POWERDOWN                         (1 << 0)
# define AVIVO_DACA_POWERDOWN_BLUE                              (1 << 8)
# define AVIVO_DACA_POWERDOWN_GREEN                             (1 << 16)
# define AVIVO_DACA_POWERDOWN_RED                               (1 << 24)

#define AVIVO_DACB_CNTL						0x7a00
#define AVIVO_DACB_CRTC_SOURCE				0x7a04
#define AVIVO_DACB_FORCE_OUTPUT_CNTL				0x7a3c
# define AVIVO_DACB_FORCE_OUTPUT_CNTL_FORCE_DATA_EN             (1 << 0)
# define AVIVO_DACB_FORCE_OUTPUT_CNTL_DATA_SEL_SHIFT            (8)
# define AVIVO_DACB_FORCE_OUTPUT_CNTL_DATA_SEL_BLUE             (1 << 0)
# define AVIVO_DACB_FORCE_OUTPUT_CNTL_DATA_SEL_GREEN            (1 << 1)
# define AVIVO_DACB_FORCE_OUTPUT_CNTL_DATA_SEL_RED              (1 << 2)
# define AVIVO_DACB_FORCE_OUTPUT_CNTL_DATA_ON_BLANKB_ONLY       (1 << 24)
#define AVIVO_DACB_POWERDOWN					0x7a50
# define AVIVO_DACB_POWERDOWN_POWERDOWN                         (1 << 0)
# define AVIVO_DACB_POWERDOWN_BLUE                              (1 << 8)
# define AVIVO_DACB_POWERDOWN_GREEN                             (1 << 16)
# define AVIVO_DACB_POWERDOWN_RED                               (1 << 24)

/* Frustratingly, at least on my R580, the DAC and TMDS orders
 * appear inversed: 7800 and 7a80 enable/disable the same physical
 * connector; ditto 7a00 and 7880.  O brave new world!
 */
/* TMDS_CNTL only lower bit of each half bytes matters.
 *     UNK0 seems to have no effect on LVDS but kill the feed of DVI connector
 *     UNK1 really unknow: so far no visible change from setting it or not
 *     UNK2 really unknow: so far no visible change from setting it or not
 *     UNK3 really unknow: so far no visible change from setting it or not
 *     UNK4 seems to switch red & blue encoding
 *     UNK5 is the fun bits on some card people will see their desktop
 *          tiled 4 times but for most cards this will give wrong pictures
 *     UNK6 seems to kill the feed LVDS & DVI
 */
#define AVIVO_TMDSA_CNTL                    0x7880
#   define AVIVO_TMDSA_CNTL_ENABLE               (1 << 0)
#   define AVIVO_TMDSA_CNTL_HPD_MASK             (1 << 4)
#   define AVIVO_TMDSA_CNTL_HPD_SELECT           (1 << 8)
#   define AVIVO_TMDSA_CNTL_SYNC_PHASE           (1 << 12)
#   define AVIVO_TMDSA_CNTL_PIXEL_ENCODING       (1 << 16)
#   define AVIVO_TMDSA_CNTL_DUAL_LINK_ENABLE     (1 << 24)
#   define AVIVO_TMDSA_CNTL_SWAP                 (1 << 28)
#define AVIVO_TMDSA_CRTC_SOURCE				0x7884
/* 78a8 appears to be some kind of (reasonably tolerant) clock?
 * 78d0 definitely hits the transmitter, definitely clock. */
/* MYSTERY1 This appears to control dithering? */
#define AVIVO_TMDSA_BIT_DEPTH_CONTROL		0x7894
#   define AVIVO_TMDS_BIT_DEPTH_CONTROL_TRUNCATE_EN           (1 << 0)
#   define AVIVO_TMDS_BIT_DEPTH_CONTROL_TRUNCATE_DEPTH        (1 << 4)
#   define AVIVO_TMDS_BIT_DEPTH_CONTROL_SPATIAL_DITHER_EN     (1 << 8)
#   define AVIVO_TMDS_BIT_DEPTH_CONTROL_SPATIAL_DITHER_DEPTH  (1 << 12)
#   define AVIVO_TMDS_BIT_DEPTH_CONTROL_TEMPORAL_DITHER_EN    (1 << 16)
#   define AVIVO_TMDS_BIT_DEPTH_CONTROL_TEMPORAL_DITHER_DEPTH (1 << 20)
#   define AVIVO_TMDS_BIT_DEPTH_CONTROL_TEMPORAL_LEVEL        (1 << 24)
#   define AVIVO_TMDS_BIT_DEPTH_CONTROL_TEMPORAL_DITHER_RESET (1 << 26)
#define AVIVO_TMDSA_DCBALANCER_CONTROL                  0x78d0
#   define AVIVO_TMDSA_DCBALANCER_CONTROL_EN                  (1 << 0)
#   define AVIVO_TMDSA_DCBALANCER_CONTROL_TEST_EN             (1 << 8)
#   define AVIVO_TMDSA_DCBALANCER_CONTROL_TEST_IN_SHIFT       (16)
#   define AVIVO_TMDSA_DCBALANCER_CONTROL_FORCE               (1 << 24)
#define AVIVO_TMDSA_DATA_SYNCHRONIZATION                0x78d8
#   define AVIVO_TMDSA_DATA_SYNCHRONIZATION_DSYNSEL           (1 << 0)
#   define AVIVO_TMDSA_DATA_SYNCHRONIZATION_PFREQCHG          (1 << 8)
#define AVIVO_TMDSA_CLOCK_ENABLE            0x7900
#define AVIVO_TMDSA_TRANSMITTER_ENABLE              0x7904
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_TX0_ENABLE          (1 << 0)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_LNKC0EN             (1 << 1)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_LNKD00EN            (1 << 2)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_LNKD01EN            (1 << 3)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_LNKD02EN            (1 << 4)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_TX1_ENABLE          (1 << 8)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_LNKD10EN            (1 << 10)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_LNKD11EN            (1 << 11)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_LNKD12EN            (1 << 12)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_TX_ENABLE_HPD_MASK  (1 << 16)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_LNKCEN_HPD_MASK     (1 << 17)
#   define AVIVO_TMDSA_TRANSMITTER_ENABLE_LNKDEN_HPD_MASK     (1 << 18)

/* I don't know any of the bits here, only that enabling (1 << 5)
 * without (1 << 4) makes things go utterly mental ... seems to be
 * the transmitter clock again. */
/* 790c is a clock?
 * 7910 appears to be some kind of control field, again.  (1 << 25)
 * must be enabled to get a signal on my monitor. */
#define AVIVO_TMDSA_TRANSMITTER_CONTROL				0x7910
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_PLL_ENABLE	(1 << 0)
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_PLL_RESET  	(1 << 1)
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_PLL_HPD_MASK_SHIFT	(2)
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_IDSCKSEL	        (1 << 4)
#       define AVIVO_TMDSA_TRANSMITTER_CONTROL_BGSLEEP          (1 << 5)
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_PLL_PWRUP_SEQ_EN	(1 << 6)
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_TMCLK	        (1 << 8)
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_TMCLK_FROM_PADS	(1 << 13)
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_TDCLK	        (1 << 14)
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_TDCLK_FROM_PADS	(1 << 15)
#       define AVIVO_TMDSA_TRANSMITTER_CONTROL_CLK_PATTERN_SHIFT (16)
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_BYPASS_PLL	(1 << 28)
#       define AVIVO_TMDSA_TRANSMITTER_CONTROL_USE_CLK_DATA     (1 << 29)
#	define AVIVO_TMDSA_TRANSMITTER_CONTROL_INPUT_TEST_CLK_SEL	(1 << 31)

#define AVIVO_LVTMA_CNTL					0x7a80
#   define AVIVO_LVTMA_CNTL_ENABLE               (1 << 0)
#   define AVIVO_LVTMA_CNTL_HPD_MASK             (1 << 4)
#   define AVIVO_LVTMA_CNTL_HPD_SELECT           (1 << 8)
#   define AVIVO_LVTMA_CNTL_SYNC_PHASE           (1 << 12)
#   define AVIVO_LVTMA_CNTL_PIXEL_ENCODING       (1 << 16)
#   define AVIVO_LVTMA_CNTL_DUAL_LINK_ENABLE     (1 << 24)
#   define AVIVO_LVTMA_CNTL_SWAP                 (1 << 28)
#define AVIVO_LVTMA_CRTC_SOURCE				0x7a84
#define AVIVO_LVTMA_BIT_DEPTH_CONTROL                   0x7a94
#   define AVIVO_LVTMA_BIT_DEPTH_CONTROL_TRUNCATE_EN           (1 << 0)
#   define AVIVO_LVTMA_BIT_DEPTH_CONTROL_TRUNCATE_DEPTH        (1 << 4)
#   define AVIVO_LVTMA_BIT_DEPTH_CONTROL_SPATIAL_DITHER_EN     (1 << 8)
#   define AVIVO_LVTMA_BIT_DEPTH_CONTROL_SPATIAL_DITHER_DEPTH  (1 << 12)
#   define AVIVO_LVTMA_BIT_DEPTH_CONTROL_TEMPORAL_DITHER_EN    (1 << 16)
#   define AVIVO_LVTMA_BIT_DEPTH_CONTROL_TEMPORAL_DITHER_DEPTH (1 << 20)
#   define AVIVO_LVTMA_BIT_DEPTH_CONTROL_TEMPORAL_LEVEL        (1 << 24)
#   define AVIVO_LVTMA_BIT_DEPTH_CONTROL_TEMPORAL_DITHER_RESET (1 << 26)

#define AVIVO_LVTMA_DCBALANCER_CONTROL                  0x7ad0
#   define AVIVO_LVTMA_DCBALANCER_CONTROL_EN                  (1 << 0)
#   define AVIVO_LVTMA_DCBALANCER_CONTROL_TEST_EN             (1 << 8)
#   define AVIVO_LVTMA_DCBALANCER_CONTROL_TEST_IN_SHIFT       (16)
#   define AVIVO_LVTMA_DCBALANCER_CONTROL_FORCE               (1 << 24)

#define AVIVO_LVTMA_DATA_SYNCHRONIZATION                0x78d8
#   define AVIVO_LVTMA_DATA_SYNCHRONIZATION_DSYNSEL           (1 << 0)
#   define AVIVO_LVTMA_DATA_SYNCHRONIZATION_PFREQCHG          (1 << 8)
#define AVIVO_LVTMA_CLOCK_ENABLE			0x7b00

#define AVIVO_LVTMA_TRANSMITTER_ENABLE              0x7b04
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKC0EN             (1 << 1)
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKD00EN            (1 << 2)
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKD01EN            (1 << 3)
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKD02EN            (1 << 4)
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKD03EN            (1 << 5)
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKC1EN             (1 << 9)
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKD10EN            (1 << 10)
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKD11EN            (1 << 11)
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKD12EN            (1 << 12)
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKCEN_HPD_MASK     (1 << 17)
#   define AVIVO_LVTMA_TRANSMITTER_ENABLE_LNKDEN_HPD_MASK     (1 << 18)

#define AVIVO_LVTMA_TRANSMITTER_CONTROL			        0x7b10
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_PLL_ENABLE	  (1 << 0)
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_PLL_RESET  	  (1 << 1)
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_PLL_HPD_MASK_SHIFT (2)
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_IDSCKSEL	          (1 << 4)
#       define AVIVO_LVTMA_TRANSMITTER_CONTROL_BGSLEEP            (1 << 5)
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_PLL_PWRUP_SEQ_EN	  (1 << 6)
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_TMCLK	          (1 << 8)
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_TMCLK_FROM_PADS	  (1 << 13)
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_TDCLK	          (1 << 14)
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_TDCLK_FROM_PADS	  (1 << 15)
#       define AVIVO_LVTMA_TRANSMITTER_CONTROL_CLK_PATTERN_SHIFT  (16)
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_BYPASS_PLL	  (1 << 28)
#       define AVIVO_LVTMA_TRANSMITTER_CONTROL_USE_CLK_DATA       (1 << 29)
#	define AVIVO_LVTMA_TRANSMITTER_CONTROL_INPUT_TEST_CLK_SEL (1 << 31)

#define AVIVO_LVTMA_PWRSEQ_CNTL						0x7af0
#	define AVIVO_LVTMA_PWRSEQ_EN					    (1 << 0)
#	define AVIVO_LVTMA_PWRSEQ_PLL_ENABLE_MASK			    (1 << 2)
#	define AVIVO_LVTMA_PWRSEQ_PLL_RESET_MASK			    (1 << 3)
#	define AVIVO_LVTMA_PWRSEQ_TARGET_STATE				    (1 << 4)
#	define AVIVO_LVTMA_SYNCEN					    (1 << 8)
#	define AVIVO_LVTMA_SYNCEN_OVRD					    (1 << 9)
#	define AVIVO_LVTMA_SYNCEN_POL					    (1 << 10)
#	define AVIVO_LVTMA_DIGON					    (1 << 16)
#	define AVIVO_LVTMA_DIGON_OVRD					    (1 << 17)
#	define AVIVO_LVTMA_DIGON_POL					    (1 << 18)
#	define AVIVO_LVTMA_BLON						    (1 << 24)
#	define AVIVO_LVTMA_BLON_OVRD					    (1 << 25)
#	define AVIVO_LVTMA_BLON_POL					    (1 << 26)

#define AVIVO_LVTMA_PWRSEQ_STATE                        0x7af4
#       define AVIVO_LVTMA_PWRSEQ_STATE_TARGET_STATE_R          (1 << 0)
#       define AVIVO_LVTMA_PWRSEQ_STATE_DIGON                   (1 << 1)
#       define AVIVO_LVTMA_PWRSEQ_STATE_SYNCEN                  (1 << 2)
#       define AVIVO_LVTMA_PWRSEQ_STATE_BLON                    (1 << 3)
#       define AVIVO_LVTMA_PWRSEQ_STATE_DONE                    (1 << 4)
#       define AVIVO_LVTMA_PWRSEQ_STATE_STATUS_SHIFT            (8)

#define AVIVO_LVDS_BACKLIGHT_CNTL			0x7af8
#	define AVIVO_LVDS_BACKLIGHT_CNTL_EN			(1 << 0)
#	define AVIVO_LVDS_BACKLIGHT_LEVEL_MASK		0x0000ff00
#	define AVIVO_LVDS_BACKLIGHT_LEVEL_SHIFT		8

/* The BIOS says so, anyway ... */
#define AVIVO_GPIO_0                        0x7e30
#define AVIVO_GPIO_1                        0x7e40
#define AVIVO_GPIO_2                        0x7e50
#define AVIVO_GPIO_3                        0x7e60

#define AVIVO_TMDS_STATUS					0x7e9c
#	define AVIVO_TMDSA_CONNECTED				(1 << 0)
#	define AVIVO_LVTMA_CONNECTED				(1 << 8)

/* Cursor registers. */
#define AVIVO_CURSOR1_CNTL					0x6400
#	define AVIVO_CURSOR_EN						(1 << 0)
#	define AVIVO_CURSOR_FORMAT_MASK				(3 << 8)
#	define AVIVO_CURSOR_FORMAT_ABGR				0x1
#	define AVIVO_CURSOR_FORMAT_ARGB				0x2
#	define AVIVO_CURSOR_FORMAT_SHIFT			8
#define AVIVO_CURSOR1_LOCATION				0x6408
/* x is in the top 16 bits; y in the lower 16.  Note that _SIZE does not
 * impact the in-memory format: it is always 64x64. */
#define AVIVO_CURSOR1_SIZE					0x6410
#define AVIVO_CURSOR1_POSITION				0x6414

#define AVIVO_I2C_STATUS					0x7d30
#	define AVIVO_I2C_STATUS_DONE				(1 << 0)
#	define AVIVO_I2C_STATUS_NACK				(1 << 1)
#	define AVIVO_I2C_STATUS_HALT				(1 << 2)
#	define AVIVO_I2C_STATUS_GO				(1 << 3)
#	define AVIVO_I2C_STATUS_MASK				0x7
/* If radeon_mm_i2c is to be believed, this is HALT, NACK, and maybe
 * DONE? */
#	define AVIVO_I2C_STATUS_CMD_RESET			0x7
#	define AVIVO_I2C_STATUS_CMD_WAIT			(1 << 3)
#define AVIVO_I2C_STOP						0x7d34
#define AVIVO_I2C_START_CNTL				0x7d38
#	define AVIVO_I2C_START						(1 << 8)
#	define AVIVO_I2C_CONNECTOR0					(0 << 16)
#	define AVIVO_I2C_CONNECTOR1					(1 << 16)
#define R520_I2C_START (1<<0)
#define R520_I2C_STOP (1<<1)
#define R520_I2C_RX (1<<2)
#define R520_I2C_EN (1<<8)
#define R520_I2C_DDC1 (0<<16)
#define R520_I2C_DDC2 (1<<16)
#define R520_I2C_DDC3 (2<<16)
#define R520_I2C_DDC_MASK (3<<16)
#define AVIVO_I2C_CONTROL2					0x7d3c
#	define AVIVO_I2C_7D3C_SIZE_SHIFT			8
#	define AVIVO_I2C_7D3C_SIZE_MASK				(0xf << 8)
#define AVIVO_I2C_CONTROL3						0x7d40
/* Reading is done 4 bytes at a time: read the bottom 8 bits from
 * 7d44, four times in a row.
 * Writing is a little more complex.  First write DATA with
 * 0xnnnnnnzz, then 0xnnnnnnyy, where nnnnnn is some non-deterministic
 * magic number, zz is, I think, the slave address, and yy is the byte
 * you want to write. */
#define AVIVO_I2C_DATA						0x7d44
#define R520_I2C_ADDR_COUNT_MASK (0x7)
#define R520_I2C_DATA_COUNT_SHIFT (8)
#define R520_I2C_DATA_COUNT_MASK (0xF00)
#define AVIVO_I2C_CNTL						0x7d50
#	define AVIVO_I2C_EN							(1 << 0)
#	define AVIVO_I2C_RESET						(1 << 8)


#define R520_PCLK_HDCP_CNTL  0x494

#define AVIVO_HDP_FB_LOCATION 0x134
#define AVIVO_VGA_MEM_BASE 0x310
#define AVIVO_VGA_SURF_ADDR 0x318
#define AVIVO_D1VGA_CTRL 0x330
#define AVIVO_D2VGA_CTRL 0x338

#define AVIVO_DVGA_MODE_ENABLE (1<<0)


