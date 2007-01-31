/*
 * Copyright © 2006 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

/** @file
 * Integrated TV-out support for the 915GM and 945GM.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xf86.h"
#include "i830.h"
#include "i830_display.h"
#include <string.h>
enum tv_type {
    TV_TYPE_NONE,
    TV_TYPE_UNKNOWN,
    TV_TYPE_COMPOSITE,
    TV_TYPE_SVIDEO,
    TV_TYPE_COMPONENT
};

/** Private structure for the integrated TV support */
struct i830_tv_priv {
    int type;
    CARD32 save_TV_H_CTL_1;
    CARD32 save_TV_H_CTL_2;
    CARD32 save_TV_H_CTL_3;
    CARD32 save_TV_V_CTL_1;
    CARD32 save_TV_V_CTL_2;
    CARD32 save_TV_V_CTL_3;
    CARD32 save_TV_V_CTL_4;
    CARD32 save_TV_V_CTL_5;
    CARD32 save_TV_V_CTL_6;
    CARD32 save_TV_V_CTL_7;
    CARD32 save_TV_SC_CTL_1, save_TV_SC_CTL_2, save_TV_SC_CTL_3;

    CARD32 save_TV_CSC_Y;
    CARD32 save_TV_CSC_Y2;
    CARD32 save_TV_CSC_U;
    CARD32 save_TV_CSC_U2;
    CARD32 save_TV_CSC_V;
    CARD32 save_TV_CSC_V2;
    CARD32 save_TV_CLR_KNOBS;
    CARD32 save_TV_CLR_LEVEL;
    CARD32 save_TV_WIN_POS;
    CARD32 save_TV_WIN_SIZE;
    CARD32 save_TV_FILTER_CTL_1;
    CARD32 save_TV_FILTER_CTL_2;
    CARD32 save_TV_FILTER_CTL_3;

    CARD32 save_TV_H_LUMA[60];
    CARD32 save_TV_H_CHROMA[60];
    CARD32 save_TV_V_LUMA[43];
    CARD32 save_TV_V_CHROMA[43];

    CARD32 save_TV_DAC;
    CARD32 save_TV_CTL;
};

typedef struct {
    int	blank, black, burst;
} video_levels_t;

typedef struct {
    float   ry, gy, by, ay;
    float   ru, gu, bu, au;
    float   rv, gv, bv, av;
} color_conversion_t;

static const CARD32 filter_table_hres[] = {
    0xB1403000, 0x2E203500, 0x35002E20, 0x3000B140,
    0x35A0B160, 0x2DC02E80, 0xB1403480, 0xB1603000,
    0x2EA03640, 0x34002D80, 0x3000B120, 0x36E0B160,
    0x2D202EF0, 0xB1203380, 0xB1603000, 0x2F303780,
    0x33002CC0, 0x3000B100, 0x3820B160, 0x2C802F50,
    0xB10032A0, 0xB1603000, 0x2F9038C0, 0x32202C20,
    0x3000B0E0, 0x3980B160, 0x2BC02FC0, 0xB0E031C0,
    0xB1603000, 0x2FF03A20, 0x31602B60, 0xB020B0C0,
    0x3AE0B160, 0x2B001810, 0xB0C03120, 0xB140B020,
    0x18283BA0, 0x30C02A80, 0xB020B0A0, 0x3C60B140,
    0x2A201838, 0xB0A03080, 0xB120B020, 0x18383D20,
    0x304029C0, 0xB040B080, 0x3DE0B100, 0x29601848,
    0xB0803000, 0xB100B040, 0x18483EC0, 0xB0402900,
    0xB040B060, 0x3F80B0C0, 0x28801858, 0xB060B080,
    0xB0A0B060, 0x18602820, 0xB0A02820, 0x0000B060,
    0xB1403000, 0x2E203500, 0x35002E20, 0x3000B140,
    0x35A0B160, 0x2DC02E80, 0xB1403480, 0xB1603000,
    0x2EA03640, 0x34002D80, 0x3000B120, 0x36E0B160,
    0x2D202EF0, 0xB1203380, 0xB1603000, 0x2F303780,
    0x33002CC0, 0x3000B100, 0x3820B160, 0x2C802F50,
    0xB10032A0, 0xB1603000, 0x2F9038C0, 0x32202C20,
    0x3000B0E0, 0x3980B160, 0x2BC02FC0, 0xB0E031C0,
    0xB1603000, 0x2FF03A20, 0x31602B60, 0xB020B0C0,
    0x3AE0B160, 0x2B001810, 0xB0C03120, 0xB140B020,
    0x18283BA0, 0x30C02A80, 0xB020B0A0, 0x3C60B140,
    0x2A201838, 0xB0A03080, 0xB120B020, 0x18383D20,
    0x304029C0, 0xB040B080, 0x3DE0B100, 0x29601848,
    0xB0803000, 0xB100B040, 0x18483EC0, 0xB0402900,
    0xB040B060, 0x3F80B0C0, 0x28801858, 0xB060B080,
    0xB0A0B060, 0x18602820, 0xB0A02820, 0x0000B060,
    0x36403000, 0x2D002CC0, 0x30003640, 0x2D0036C0,
    0x35C02CC0, 0x37403000, 0x2C802D40, 0x30003540,
    0x2D8037C0, 0x34C02C40, 0x38403000, 0x2BC02E00,
    0x30003440, 0x2E2038C0, 0x34002B80, 0x39803000,
    0x2B402E40, 0x30003380, 0x2E603A00, 0x33402B00,
    0x3A803040, 0x2A802EA0, 0x30403300, 0x2EC03B40,
    0x32802A40, 0x3C003040, 0x2A002EC0, 0x30803240,
    0x2EC03C80, 0x320029C0, 0x3D403080, 0x29402F00,
    0x308031C0, 0x2F203DC0, 0x31802900, 0x3E8030C0,
    0x28802F40, 0x30C03140, 0x2F203F40, 0x31402840,
    0x28003100, 0x28002F00, 0x00003100, 0x36403000, 
    0x2D002CC0, 0x30003640, 0x2D0036C0,
    0x35C02CC0, 0x37403000, 0x2C802D40, 0x30003540,
    0x2D8037C0, 0x34C02C40, 0x38403000, 0x2BC02E00,
    0x30003440, 0x2E2038C0, 0x34002B80, 0x39803000,
    0x2B402E40, 0x30003380, 0x2E603A00, 0x33402B00,
    0x3A803040, 0x2A802EA0, 0x30403300, 0x2EC03B40,
    0x32802A40, 0x3C003040, 0x2A002EC0, 0x30803240,
    0x2EC03C80, 0x320029C0, 0x3D403080, 0x29402F00,
    0x308031C0, 0x2F203DC0, 0x31802900, 0x3E8030C0,
    0x28802F40, 0x30C03140, 0x2F203F40, 0x31402840,
    0x28003100, 0x28002F00, 0x00003100,
};

static const CARD32 filter_table_pal[] = {
	0xB0603000, 0x1920B3E0, 0xB3E01900, 0x3000B060, 0xB380B080,
	0x18A01980, 0xB040B440, 0xB0A03000, 0x19D0B300, 0xB4801840,
	0x3000B020, 0xB260B0E0, 0x2FE01A10, 0x3000B4C0, 0xB1003000,
	0x1A60B1C0, 0xB4E02F00, 0x30003020, 0xB120B140, 0x2E401AB0,
	0x3020B500, 0xB1803000, 0x1B00B060, 0xB5202D60, 0x30003040,
	0x3080B1A0, 0x2CA01B30, 0x3040B520, 0xB1E03020, 0x1B703160,
	0xB5202BC0, 0x30203040, 0x3260B220, 0x2B001B98, 0x3060B520,
	0xB2603020, 0x1BD03360, 0xB5002A20, 0x30203060, 0x3480B2A0,
	0x29601BF0, 0x3060B4E0, 0xB2E03020, 0x1C1835C0, 0xB4C02880,
	0x30403060, 0x3700B340, 0x3F801C30, 0x3060B4A0, 0xB3803040,
	0x1C403840, 0xB4603E00, 0x30403060, 0x39A0B3C0, 0x3C801C50,
	0x3040B420, 0xB4003040, 0x1C603B00, 0xB4003B00, 0x00003040,
	0x36E03640, 0x3CA03960, 0x39603CA0, 0x364036E0, 0x398036E0,
	0x3C803CC0, 0x36E03940, 0x37003640, 0x3CC039A0, 0x39203C80,
	0x364036C0, 0x39C03700, 0x3C603CC0, 0x36C03920, 0x37203640,
	0x3CC039E0, 0x39003C40, 0x364036C0, 0x3A003720, 0x3C403CE0,
	0x36A038E0, 0x37203640, 0x3D203A00, 0x38C03C20, 0x364036A0,
	0x3A203740, 0x3C003D20, 0x36A038A0, 0x37403640, 0x3D003A40,
	0x38A03C00, 0x364036A0, 0x3A603760, 0x3BE03D20, 0x36803880,
	0x37603660, 0x3D203A80, 0x38603BC0, 0x36603680, 0x3AA03780,
	0x3BA03D20, 0x36803840, 0x37803660, 0x3D003AC0, 0x38403BA0,
	0x36603680, 0x3AE037A0, 0x3B803D00, 0x36803820, 0x37C03660,
	0x3D203B00, 0x38003B60, 0x36603660, 0x3B0037C0, 0x3B403D40,
	0x36603800, 0x37E03660, 0x3D403B20, 0x37E03B20, 0x00003660,
	0x38C030C0, 0x2B802B60, 0x30C038C0, 0x2BA03940, 0x38402B40,
	0x39C030C0, 0x2B402BA0, 0x30C037C0, 0x2BC03A40, 0x37802B00,
	0x3AC03100, 0x2AC02BE0, 0x31003700, 0x2BE03B40, 0x36802AC0,
	0x3BC03100, 0x2A802C20, 0x31403600, 0x2C203C40, 0x35C02A40,
	0x3CC03140, 0x2A402C20, 0x31803540, 0x2C203D40, 0x35002A00,
	0x3DC03180, 0x29C02C60, 0x31C03480, 0x2C603E40, 0x34402980,
	0x3F003200, 0x29402C40, 0x32003400, 0x2C803F80, 0x33802900,
	0x3FC03240, 0x29002C60, 0x32803340, 0x2C402840, 0x330028C0,
	0x288032C0, 0x28802C40, 0x000032C0, 0x38C030C0, 0x2B802B60,
	0x30C038C0, 0x2BA03940, 0x38402B40, 0x39C030C0, 0x2B402BA0,
	0x30C037C0, 0x2BC03A40, 0x37802B00, 0x3AC03100, 0x2AC02BE0,
	0x31003700, 0x2BE03B40, 0x36802AC0, 0x3BC03100, 0x2A802C20,
	0x31403600, 0x2C203C40, 0x35C02A40, 0x3CC03140, 0x2A402C20,
	0x31803540, 0x2C203D40, 0x35002A00, 0x3DC03180, 0x29C02C60,
	0x31C03480, 0x2C603E40, 0x34402980, 0x3F003200, 0x29402C40,
	0x32003400, 0x2C803F80, 0x33802900, 0x3FC03240, 0x29002C60,
	0x32803340, 0x2C402840, 0x330028C0, 0x288032C0, 0x28802C40,
	0x000032C0,
};

static const CARD32 filter_table_ntsc[] = {
	0xB0603000, 0x1920B3E0, 0xB3E01900, 0x3000B060, 0xB380B080,
	0x18A01980, 0xB040B440, 0xB0A03000, 0x19D0B300, 0xB4801840,
	0x3000B020, 0xB260B0E0, 0x2FE01A10, 0x3000B4C0, 0xB1003000,
	0x1A60B1C0, 0xB4E02F00, 0x30003020, 0xB120B140, 0x2E401AB0,
	0x3020B500, 0xB1803000, 0x1B00B060, 0xB5202D60, 0x30003040,
	0x3080B1A0, 0x2CA01B30, 0x3040B520, 0xB1E03020, 0x1B703160,
	0xB5202BC0, 0x30203040, 0x3260B220, 0x2B001B98, 0x3060B520,
	0xB2603020, 0x1BD03360, 0xB5002A20, 0x30203060, 0x3480B2A0,
	0x29601BF0, 0x3060B4E0, 0xB2E03020, 0x1C1835C0, 0xB4C02880,
	0x30403060, 0x3700B340, 0x3F801C30, 0x3060B4A0, 0xB3803040,
	0x1C403840, 0xB4603E00, 0x30403060, 0x39A0B3C0, 0x3C801C50,
	0x3040B420, 0xB4003040, 0x1C603B00, 0xB4003B00, 0x00003040,
	0x38A03000, 0x3C803AE0, 0x3AE03C80, 0x300038A0, 0x3B0038E0,
	0x3C803C80, 0x38403AE0, 0x39203000, 0x3C803B00, 0x3AE03C80,
	0x30203800, 0x3B003960, 0x3C803C80, 0x37A03AE0, 0x39803040,
	0x3C603B20, 0x3AE03C80, 0x30403760, 0x3B2039C0, 0x3C803C80,
	0x37003AE0, 0x39E03080, 0x3C603B40, 0x3AE03C80, 0x30A036A0,
	0x3B603A20, 0x3C803C40, 0x36403AE0, 0x3A4030C0, 0x3C403B60,
	0x3AE03C80, 0x31003600, 0x3B803A60, 0x3C603C40, 0x35A03AE0,
	0x3A803140, 0x3C403B80, 0x3AE03C60, 0x31803540, 0x3BA03A80,
	0x3C603C40, 0x34E03AE0, 0x3AA031C0, 0x3C403BC0, 0x3AE03C40,
	0x32003480, 0x3BC03AC0, 0x3C403C40, 0x34203AE0, 0x3AC03260,
	0x3C403BE0, 0x3AE03C20, 0x32A033C0, 0x3C003AC0, 0x3C203C40,
	0x33603AE0, 0x3AE03300, 0x3C403C00, 0x3AE03C00, 0x00003300,
	0x38C030C0, 0x2B802B60, 0x30C038C0, 0x2BA03940, 0x38402B40,
	0x39C030C0, 0x2B402BA0, 0x30C037C0, 0x2BC03A40, 0x37802B00,
	0x3AC03100, 0x2AC02BE0, 0x31003700, 0x2BE03B40, 0x36802AC0,
	0x3BC03100, 0x2A802C20, 0x31403600, 0x2C203C40, 0x35C02A40,
	0x3CC03140, 0x2A402C20, 0x31803540, 0x2C203D40, 0x35002A00,
	0x3DC03180, 0x29C02C60, 0x31C03480, 0x2C603E40, 0x34402980,
	0x3F003200, 0x29402C40, 0x32003400, 0x2C803F80, 0x33802900,
	0x3FC03240, 0x29002C60, 0x32803340, 0x2C402840, 0x330028C0,
	0x288032C0, 0x28802C40, 0x000032C0, 0x38C030C0, 0x2B802B60,
	0x30C038C0, 0x2BA03940, 0x38402B40, 0x39C030C0, 0x2B402BA0,
	0x30C037C0, 0x2BC03A40, 0x37802B00, 0x3AC03100, 0x2AC02BE0,
	0x31003700, 0x2BE03B40, 0x36802AC0, 0x3BC03100, 0x2A802C20,
	0x31403600, 0x2C203C40, 0x35C02A40, 0x3CC03140, 0x2A402C20,
	0x31803540, 0x2C203D40, 0x35002A00, 0x3DC03180, 0x29C02C60,
	0x31C03480, 0x2C603E40, 0x34402980, 0x3F003200, 0x29402C40,
	0x32003400, 0x2C803F80, 0x33802900, 0x3FC03240, 0x29002C60,
	0x32803340, 0x2C402840, 0x330028C0, 0x288032C0, 0x28802C40,
	0x000032C0,
};

typedef struct {
    char *name;
    int	clock;
    CARD32 oversample;
    int hsync_end, hblank_start, hblank_end, htotal;
    Bool progressive, trilevel_sync, component_only;
    int vsync_start_f1, vsync_start_f2, vsync_len;
    Bool veq_ena;
    int veq_start_f1, veq_start_f2, veq_len;
    int vi_end_f1, vi_end_f2, nbr_end;
    Bool burst_ena;
    int hburst_start, hburst_len;
    int vburst_start_f1, vburst_end_f1;
    int vburst_start_f2, vburst_end_f2;
    int vburst_start_f3, vburst_end_f3;
    int vburst_start_f4, vburst_end_f4;
    /*
     * subcarrier programming
     */
    int dda2_size, dda3_size, dda1_inc, dda2_inc, dda3_inc;
    CARD32 sc_reset;
    Bool pal_burst;
    /*
     * blank/black levels
     */
    video_levels_t	composite_levels, svideo_levels;
    color_conversion_t	composite_color, svideo_color;
    const CARD32 *filter_table;
} tv_mode_t;


/*
 * Sub carrier DDA
 *
 *  I think this works as follows:
 *
 *  subcarrier freq = pixel_clock * (dda1_inc + dda2_inc / dda2_size) / 4096
 *
 * Presumably, when dda3 is added in, it gets to adjust the dda2_inc value
 *
 * So,
 *  dda1_ideal = subcarrier/pixel * 4096
 *  dda1_inc = floor (dda1_ideal)
 *  dda2 = dda1_ideal - dda1_inc
 *
 *  then pick a ratio for dda2 that gives the closest approximation. If
 *  you can't get close enough, you can play with dda3 as well. This
 *  seems likely to happen when dda2 is small as the jumps would be larger
 *
 * To invert this,
 *
 *  pixel_clock = subcarrier * 4096 / (dda1_inc + dda2_inc / dda2_size)
 *
 * The constants below were all computed using a 107.520MHz clock
 */
 
/**
 * Register programming values for TV modes.
 *
 * These values account for -1s required.
 */

const static tv_mode_t tv_modes[] = {
    {
	.name		= "NTSC-M",
	.clock		= 107520,	
	.oversample	= TV_OVERSAMPLE_8X,
	.component_only = 0,
	/* 525 Lines, 60 Fields, 15.734KHz line, Sub-Carrier 3.580MHz */

	.hsync_end	= 64,		    .hblank_end		= 124,
	.hblank_start	= 836,		    .htotal		= 857,
	
	.progressive	= FALSE,	    .trilevel_sync = FALSE,

	.vsync_start_f1	= 6,		    .vsync_start_f2	= 7,
	.vsync_len	= 6,
	
	.veq_ena	= TRUE,		    .veq_start_f1    	= 0,
	.veq_start_f2	= 1,		    .veq_len		= 18,
	
	.vi_end_f1	= 20,		    .vi_end_f2		= 21,
	.nbr_end	= 240,
	
	.burst_ena	= TRUE,
	.hburst_start	= 72,		    .hburst_len		= 34,
	.vburst_start_f1 = 9,		    .vburst_end_f1	= 240,
	.vburst_start_f2 = 10,		    .vburst_end_f2	= 240,
	.vburst_start_f3 = 9,		    .vburst_end_f3	= 240, 
	.vburst_start_f4 = 10,		    .vburst_end_f4	= 240,

	/* desired 3.5800000 actual 3.5800000 clock 107.52 */
	.dda1_inc	=    136,
	.dda2_inc	=   7624,	    .dda2_size		=  20013,
	.dda3_inc	=      0,	    .dda3_size		=      0,
	.sc_reset	= TV_SC_RESET_EVERY_4,
	.pal_burst	= FALSE,

	.composite_levels = { .blank = 225, .black = 267, .burst = 113 },
	.composite_color = {
	    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5082,
	    .ru =-0.0749, .gu =-0.1471, .bu = 0.2220, .au = 1.0000,
	    .rv = 0.3125, .gv =-0.2616, .bv =-0.0508, .av = 1.0000,
	},

	.svideo_levels    = { .blank = 266, .black = 316, .burst = 133 },
	.svideo_color = {
	    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.6006,
	    .ru =-0.0885, .gu =-0.1738, .bu = 0.2624, .au = 1.0000,
	    .rv = 0.3693, .gv =-0.3092, .bv =-0.0601, .av = 1.0000,
	},
	.filter_table = filter_table_ntsc,
    },
    {
         .name		= "NTSC-443",
	 .clock		= 107520,	
	 .oversample	= TV_OVERSAMPLE_8X,
	 .component_only = 0,
	 /* 525 Lines, 60 Fields, 15.734KHz line, Sub-Carrier 4.43MHz */
	 .hsync_end	= 64,		    .hblank_end		= 124,
	 .hblank_start	= 836,		    .htotal		= 857,

	 .progressive	= FALSE,	    .trilevel_sync = FALSE,

	 .vsync_start_f1 = 6,		    .vsync_start_f2	= 7,
	 .vsync_len	= 6,

	 .veq_ena	= TRUE,		    .veq_start_f1    	= 0,
	 .veq_start_f2	= 1,		    .veq_len		= 18,

	 .vi_end_f1	= 20,		    .vi_end_f2		= 21,
	 .nbr_end	= 240,

	 .burst_ena	= TRUE,
	 .hburst_start	= 72,		    .hburst_len		= 34,
	 .vburst_start_f1 = 9,		    .vburst_end_f1	= 240,
	 .vburst_start_f2 = 10,		    .vburst_end_f2	= 240,
	 .vburst_start_f3 = 9,		    .vburst_end_f3	= 240, 
	 .vburst_start_f4 = 10,		    .vburst_end_f4	= 240,

	 /* desired 4.4336180 actual 4.4336180 clock 107.52 */
	 .dda1_inc	=   168,
	 .dda2_inc	=   4093,	    .dda2_size		=  27456,
	 .dda3_inc	=   525,	    .dda3_size		=  310,
	 .sc_reset	= TV_SC_RESET_NEVER,
	 .pal_burst	= FALSE,

	 .composite_levels = { .blank = 225, .black = 267, .burst = 113 },
	 .composite_color = {
		 .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5082,
		 .ru =-0.0749, .gu =-0.1471, .bu = 0.2220, .au = 1.0000,
		 .rv = 0.3125, .gv =-0.2616, .bv =-0.0508, .av = 1.0000,
	 },

	 .svideo_levels    = { .blank = 266, .black = 316, .burst = 133 },
	 .svideo_color = {
		 .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.6006,
		 .ru =-0.0885, .gu =-0.1738, .bu = 0.2624, .au = 1.0000,
		 .rv = 0.3693, .gv =-0.3092, .bv =-0.0601, .av = 1.0000,
	 },
	.filter_table = filter_table_ntsc,
    },
    {
	    .name		= "NTSC-J",
	    .clock		= 107520,	
	    .oversample	= TV_OVERSAMPLE_8X,
	    .component_only = 0,

	    /* 525 Lines, 60 Fields, 15.734KHz line, Sub-Carrier 3.580MHz */
	    .hsync_end	= 64,		    .hblank_end		= 124,
	    .hblank_start = 836,	    .htotal		= 857,

	    .progressive	= FALSE,    .trilevel_sync = FALSE,

	    .vsync_start_f1	= 6,	    .vsync_start_f2	= 7,
	    .vsync_len	= 6,

	    .veq_ena	= TRUE,		    .veq_start_f1    	= 0,
	    .veq_start_f2 = 1,	    .veq_len		= 18,

	    .vi_end_f1	= 20,		    .vi_end_f2		= 21,
	    .nbr_end	= 240,

	    .burst_ena	= TRUE,
	    .hburst_start	= 72,		    .hburst_len		= 34,
	    .vburst_start_f1 = 9,		    .vburst_end_f1	= 240,
	    .vburst_start_f2 = 10,		    .vburst_end_f2	= 240,
	    .vburst_start_f3 = 9,		    .vburst_end_f3	= 240, 
	    .vburst_start_f4 = 10,		    .vburst_end_f4	= 240,

	    /* desired 3.5800000 actual 3.5800000 clock 107.52 */
	    .dda1_inc	=    136,
	    .dda2_inc	=   7624,	    .dda2_size		=  20013,
	    .dda3_inc	=      0,	    .dda3_size		=      0,
	    .sc_reset	= TV_SC_RESET_EVERY_4,
	    .pal_burst	= FALSE,

	    .composite_levels = { .blank = 225, .black = 225, .burst = 113 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5495,
		    .ru =-0.0810, .gu =-0.1590, .bu = 0.2400, .au = 1.0000,
		    .rv = 0.3378, .gv =-0.2829, .bv =-0.0549, .av = 1.0000,
	    },

	    .svideo_levels    = { .blank = 266, .black = 266, .burst = 133 },
	    .svideo_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.6494,
		    .ru =-0.0957, .gu =-0.1879, .bu = 0.2836, .au = 1.0000,
		    .rv = 0.3992, .gv =-0.3343, .bv =-0.0649, .av = 1.0000,
	    },
	    .filter_table = filter_table_ntsc,
    },
    {
	    .name		= "PAL-M",
	    .clock		= 107520,	
	    .oversample	= TV_OVERSAMPLE_8X,
	    .component_only = 0,

	    /* 525 Lines, 60 Fields, 15.734KHz line, Sub-Carrier 3.580MHz */
	    .hsync_end	= 64,		  .hblank_end		= 124,
	    .hblank_start = 836,	  .htotal		= 857,

	    .progressive	= FALSE,	    .trilevel_sync = FALSE,

	    .vsync_start_f1	= 6,		    .vsync_start_f2	= 7,
	    .vsync_len	= 6,

	    .veq_ena	= TRUE,		    .veq_start_f1    	= 0,
	    .veq_start_f2	= 1,		    .veq_len		= 18,

	    .vi_end_f1	= 20,		    .vi_end_f2		= 21,
	    .nbr_end	= 240,

	    .burst_ena	= TRUE,
	    .hburst_start	= 72,		    .hburst_len		= 34,
	    .vburst_start_f1 = 9,		    .vburst_end_f1	= 240,
	    .vburst_start_f2 = 10,		    .vburst_end_f2	= 240,
	    .vburst_start_f3 = 9,		    .vburst_end_f3	= 240, 
	    .vburst_start_f4 = 10,		    .vburst_end_f4	= 240,

	    /* desired 3.5800000 actual 3.5800000 clock 107.52 */
	    .dda1_inc	=    135,
	    .dda2_inc	=    16704,	    .dda2_size		=  27456,
	    .dda3_inc	=      0,	    .dda3_size		=      0,
	    .sc_reset	= TV_SC_RESET_EVERY_8,
	    .pal_burst  = TRUE,

	    .composite_levels = { .blank = 225, .black = 267, .burst = 113 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5082,
		    .ru =-0.0749, .gu =-0.1471, .bu = 0.2220, .au = 1.0000,
		    .rv = 0.3125, .gv =-0.2616, .bv =-0.0508, .av = 1.0000,
	    },

	    .svideo_levels    = { .blank = 266, .black = 316, .burst = 133 },
	    .svideo_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.6006,
		    .ru =-0.0885, .gu =-0.1738, .bu = 0.2624, .au = 1.0000,
		    .rv = 0.3693, .gv =-0.3092, .bv =-0.0601, .av = 1.0000,
	    },
	    .filter_table = filter_table_pal,
    },
    {
	    /* 625 Lines, 50 Fields, 15.625KHz line, Sub-Carrier 4.434MHz */
	    .name	    = "PAL-N",
	    .clock		= 107520,	
	    .oversample	= TV_OVERSAMPLE_8X,
	    .component_only = 0,

	    .hsync_end	= 64,		    .hblank_end		= 128,
	    .hblank_start = 844,	    .htotal		= 863,

	    .progressive  = FALSE,    .trilevel_sync = FALSE,


	    .vsync_start_f1	= 6,	   .vsync_start_f2	= 7,
	    .vsync_len	= 6,

	    .veq_ena	= TRUE,		    .veq_start_f1    	= 0,
	    .veq_start_f2	= 1,		    .veq_len		= 18,

	    .vi_end_f1	= 24,		    .vi_end_f2		= 25,
	    .nbr_end	= 286,

	    .burst_ena	= TRUE,
	    .hburst_start = 73,	    .hburst_len		= 34,
	    .vburst_start_f1 = 8,	    .vburst_end_f1	= 285,
	    .vburst_start_f2 = 8,	    .vburst_end_f2	= 286,
	    .vburst_start_f3 = 9,	    .vburst_end_f3	= 286, 
	    .vburst_start_f4 = 9,	    .vburst_end_f4	= 285,

	    /* desired 4.4336180 actual 4.4336180 clock 107.52 */
	    .dda1_inc	=    135,
	    .dda2_inc	=  23578,	.dda2_size	=  27648,
	    .dda3_inc	=     134,	.dda3_size	=  625,
	    .sc_reset   = TV_SC_RESET_EVERY_8,
	    .pal_burst  = TRUE,

	    .composite_levels = { .blank = 225, .black = 267, .burst = 118 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5082,
		    .ru =-0.0749, .gu =-0.1471, .bu = 0.2220, .au = 1.0000,
		    .rv = 0.3125, .gv =-0.2616, .bv =-0.0508, .av = 1.0000,
	    },

	    .svideo_levels    = { .blank = 266, .black = 316, .burst = 139 },
	    .svideo_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.6006,
		    .ru =-0.0885, .gu =-0.1738, .bu = 0.2624, .au = 1.0000,
		    .rv = 0.3693, .gv =-0.3092, .bv =-0.0601, .av = 1.0000,
	    },
	    .filter_table = filter_table_pal,
    },
    {
	    /* 625 Lines, 50 Fields, 15.625KHz line, Sub-Carrier 4.434MHz */
	    .name	    = "PAL",
	    .clock		= 107520,	
	    .oversample	= TV_OVERSAMPLE_8X,
	    .component_only = 0,

	    .hsync_end	= 64,		    .hblank_end		= 128,
	    .hblank_start	= 844,		    .htotal		= 863,

	    .progressive	= FALSE,	   .trilevel_sync = FALSE,


	    .vsync_start_f1	= 6,		    .vsync_start_f2	= 7,
	    .vsync_len	= 6,

	    .veq_ena	= TRUE,		    .veq_start_f1    	= 0,
	    .veq_start_f2	= 1,		    .veq_len		= 18,

	    .vi_end_f1	= 24,		    .vi_end_f2		= 25,
	    .nbr_end	= 286,

	    .burst_ena	= TRUE,
	    .hburst_start	= 73,		    .hburst_len		= 34,
	    .vburst_start_f1 = 8,		    .vburst_end_f1	= 285,
	    .vburst_start_f2 = 8,		    .vburst_end_f2	= 286,
	    .vburst_start_f3 = 9,		    .vburst_end_f3	= 286, 
	    .vburst_start_f4 = 9,		    .vburst_end_f4	= 285,

	    /* desired 4.4336180 actual 4.4336180 clock 107.52 */
	    .dda1_inc	=    168,
	    .dda2_inc	=   4122,	.dda2_size	=  27648,
	    .dda3_inc	=     67,	.dda3_size	=  625,
	    .sc_reset   = TV_SC_RESET_EVERY_8,
	    .pal_burst  = TRUE,

	    .composite_levels = { .blank = 237, .black = 237, .burst = 118 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5379,
		    .ru =-0.0793, .gu =-0.1557, .bu = 0.2350, .au = 1.0000,
		    .rv = 0.3307, .gv =-0.2769, .bv =-0.0538, .av = 1.0000,
	    },

	    .svideo_levels    = { .blank = 280, .black = 280, .burst = 139 },
	    .svideo_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.6357,
		    .ru =-0.0937, .gu =-0.1840, .bu = 0.2777, .au = 1.0000,
		    .rv = 0.3908, .gv =-0.3273, .bv =-0.0636, .av = 1.0000,
	    },
	    .filter_table = filter_table_pal,
    },
    {
	    .name       = "480p@59.94Hz",
	    .clock 	= 107520,	
	    .oversample     = TV_OVERSAMPLE_4X,
	    .component_only = 1,

	    .hsync_end      = 64,               .hblank_end         = 122,
	    .hblank_start   = 842,              .htotal             = 857,

	    .progressive    = TRUE,.trilevel_sync = FALSE,

	    .vsync_start_f1 = 12,               .vsync_start_f2     = 12,
	    .vsync_len      = 12,

	    .veq_ena        = FALSE,

	    .vi_end_f1      = 44,               .vi_end_f2          = 44,
	    .nbr_end        = 496,

	    .burst_ena      = FALSE,

	    .composite_levels = { .blank = 237, .black = 237, .burst = 118 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5379,
		    .ru =-0.0793, .gu =-0.1557, .bu = 0.2350, .au = 1.0000,
		    .rv = 0.3307, .gv =-0.2769, .bv =-0.0538, .av = 1.0000,
	    },

	    .svideo_levels    = { .blank = 280, .black = 280, .burst = 139 },
	    .svideo_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.6357,
		    .ru =-0.0937, .gu =-0.1840, .bu = 0.2777, .au = 1.0000,
		    .rv = 0.3908, .gv =-0.3273, .bv =-0.0636, .av = 1.0000,
	    },
	    .filter_table = filter_table_hres,
    },
    {
	    .name       = "480p@60Hz",
	    .clock 	= 107520,	
	    .oversample     = TV_OVERSAMPLE_4X,
	    .component_only = 1,

	    .hsync_end      = 64,               .hblank_end         = 122,
	    .hblank_start   = 842,              .htotal             = 856,

	    .progressive    = TRUE,.trilevel_sync = FALSE,

	    .vsync_start_f1 = 12,               .vsync_start_f2     = 12,
	    .vsync_len      = 12,

	    .veq_ena        = FALSE,

	    .vi_end_f1      = 44,               .vi_end_f2          = 44,
	    .nbr_end        = 496,

	    .burst_ena      = FALSE,

	    .composite_levels = { .blank = 237, .black = 237, .burst = 118 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5379,
		    .ru =-0.0793, .gu =-0.1557, .bu = 0.2350, .au = 1.0000,
		    .rv = 0.3307, .gv =-0.2769, .bv =-0.0538, .av = 1.0000,
	    },

	    .svideo_levels    = { .blank = 280, .black = 280, .burst = 139 },
	    .svideo_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.6357,
		    .ru =-0.0937, .gu =-0.1840, .bu = 0.2777, .au = 1.0000,
		    .rv = 0.3908, .gv =-0.3273, .bv =-0.0636, .av = 1.0000,
	    },
	    .filter_table = filter_table_hres,
    },
    {
	    .name       = "576p",
	    .clock 	= 107520,	
	    .oversample     = TV_OVERSAMPLE_4X,
	    .component_only = 1,

	    .hsync_end      = 64,               .hblank_end         = 139,
	    .hblank_start   = 859,              .htotal             = 863,

	    .progressive    = TRUE,		.trilevel_sync = FALSE,

	    .vsync_start_f1 = 10,               .vsync_start_f2     = 10,
	    .vsync_len      = 10,

	    .veq_ena        = FALSE,

	    .vi_end_f1      = 48,               .vi_end_f2          = 48,
	    .nbr_end        = 575,

	    .burst_ena      = FALSE,

	    .composite_levels = { .blank = 237, .black = 237, .burst = 118 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5379,
		    .ru =-0.0793, .gu =-0.1557, .bu = 0.2350, .au = 1.0000,
		    .rv = 0.3307, .gv =-0.2769, .bv =-0.0538, .av = 1.0000,
	    },

	    .svideo_levels    = { .blank = 280, .black = 280, .burst = 139 },
	    .svideo_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.6357,
		    .ru =-0.0937, .gu =-0.1840, .bu = 0.2777, .au = 1.0000,
		    .rv = 0.3908, .gv =-0.3273, .bv =-0.0636, .av = 1.0000,
	    },
	    .filter_table = filter_table_hres,
    },
    {
	    .name       = "720p@60Hz",
	    .clock		= 148800,	
	    .oversample     = TV_OVERSAMPLE_2X,
	    .component_only = 1,

	    .hsync_end      = 80,               .hblank_end         = 300,
	    .hblank_start   = 1580,             .htotal             = 1649,

	    .progressive    = TRUE, 	    .trilevel_sync = TRUE,

	    .vsync_start_f1 = 10,               .vsync_start_f2     = 10,
	    .vsync_len      = 10,

	    .veq_ena        = FALSE,

	    .vi_end_f1      = 29,               .vi_end_f2          = 29,
	    .nbr_end        = 719,

	    .burst_ena      = FALSE,

	    .composite_levels = { .blank = 237, .black = 237, .burst = 118 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5379,
		    .ru =-0.0793, .gu =-0.1557, .bu = 0.2350, .au = 1.0000,
		    .rv = 0.3307, .gv =-0.2769, .bv =-0.0538, .av = 1.0000,
	    },
	    .filter_table = filter_table_hres,
    },
    {
	    .name       = "720p@59.94Hz",
	    .clock		= 148800,	
	    .oversample     = TV_OVERSAMPLE_2X,
	    .component_only = 1,

	    .hsync_end      = 80,               .hblank_end         = 300,
	    .hblank_start   = 1580,             .htotal             = 1651,

	    .progressive    = TRUE, 	    .trilevel_sync = TRUE,

	    .vsync_start_f1 = 10,               .vsync_start_f2     = 10,
	    .vsync_len      = 10,

	    .veq_ena        = FALSE,

	    .vi_end_f1      = 29,               .vi_end_f2          = 29,
	    .nbr_end        = 719,

	    .burst_ena      = FALSE,

	    .composite_levels = { .blank = 237, .black = 237, .burst = 118 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5379,
		    .ru =-0.0793, .gu =-0.1557, .bu = 0.2350, .au = 1.0000,
		    .rv = 0.3307, .gv =-0.2769, .bv =-0.0538, .av = 1.0000,
	    },
	    .filter_table = filter_table_hres,
    },
    {
	    .name       = "1080i@50Hz",
	    .clock		= 148800,	
	    .oversample     = TV_OVERSAMPLE_2X,
	    .component_only = 1,

	    .hsync_end      = 88,               .hblank_end         = 235,
	    .hblank_start   = 2155,             .htotal             = 2639,

	    .progressive    = FALSE, 	    .trilevel_sync = TRUE,

	    .vsync_start_f1 = 4,              .vsync_start_f2     = 5,
	    .vsync_len      = 10,

	    .veq_ena	= TRUE,		    .veq_start_f1    	= 4,
	    .veq_start_f2   = 4,	    .veq_len		= 10,


	    .vi_end_f1      = 21,           .vi_end_f2          = 22,
	    .nbr_end        = 539,

	    .burst_ena      = FALSE,

	    .composite_levels = { .blank = 237, .black = 237, .burst = 118 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5379,
		    .ru =-0.0793, .gu =-0.1557, .bu = 0.2350, .au = 1.0000,
		    .rv = 0.3307, .gv =-0.2769, .bv =-0.0538, .av = 1.0000,
	    },
	    .filter_table = filter_table_hres,
    },
    {
	    .name       = "1080i@30Hz",
	    .clock		= 148800,	
	    .oversample     = TV_OVERSAMPLE_2X,
	    .component_only = 1,

	    .hsync_end      = 88,               .hblank_end         = 235,
	    .hblank_start   = 2155,             .htotal             = 2199,

	    .progressive    = FALSE, 	    .trilevel_sync = TRUE,

	    .vsync_start_f1 = 4,               .vsync_start_f2     = 5,
	    .vsync_len      = 10,

	    .veq_ena	= TRUE,		    .veq_start_f1    	= 4,
	    .veq_start_f2	= 4,		    .veq_len		= 10,


	    .vi_end_f1      = 21,               .vi_end_f2          = 22,
	    .nbr_end        = 539,

	    .burst_ena      = FALSE,

	    .composite_levels = { .blank = 237, .black = 237, .burst = 118 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5379,
		    .ru =-0.0793, .gu =-0.1557, .bu = 0.2350, .au = 1.0000,
		    .rv = 0.3307, .gv =-0.2769, .bv =-0.0538, .av = 1.0000,
	    },
	    .filter_table = filter_table_hres,
    },
    {
	    .name       = "1080i@29.97Hz",
	    .clock		= 148800,	
	    .oversample     = TV_OVERSAMPLE_2X,
	    .component_only = 1,

	    .hsync_end      = 88,               .hblank_end         = 235,
	    .hblank_start   = 2155,             .htotal             = 2200,

	    .progressive    = FALSE, 	    .trilevel_sync = TRUE,

	    .vsync_start_f1 = 4,            .vsync_start_f2    = 5,
	    .vsync_len      = 10,

	    .veq_ena	= TRUE,		    .veq_start_f1	= 4,
	    .veq_start_f2 = 4,	    	    .veq_len = 10,


	    .vi_end_f1      = 21,           .vi_end_f2         	= 22,
	    .nbr_end        = 539,

	    .burst_ena      = FALSE,

	    .composite_levels = { .blank = 237, .black = 237, .burst = 118 },
	    .composite_color = {
		    .ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.5379,
		    .ru =-0.0793, .gu =-0.1557, .bu = 0.2350, .au = 1.0000,
		    .rv = 0.3307, .gv =-0.2769, .bv =-0.0538, .av = 1.0000,
	    },
	    .filter_table = filter_table_hres,
    },
};

static const video_levels_t component_level = {
	.blank = 279, .black = 279 
};

static const color_conversion_t sdtv_component_color = {
	.ry = 0.2990, .gy = 0.5870, .by = 0.1140, .ay = 0.6364,
	.ru =-0.1687, .gu =-0.3313, .bu = 0.5000, .au = 1.0000,
	.rv = 0.5000, .gv =-0.4187, .bv =-0.0813, .av = 1.0000,
};

static const color_conversion_t hdtv_component_color = {
	.ry = 0.2126, .gy = 0.7152, .by = 0.0722, .ay = 0.6364,
	.ru =-0.1146, .gu =-0.3854, .bu = 0.5000, .au = 1.0000,
	.rv = 0.5000, .gv =-0.4542, .bv =-0.0458, .av = 1.0000,
};

static void
i830_tv_dpms(xf86OutputPtr output, int mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	I830Ptr pI830 = I830PTR(pScrn);

	switch(mode) {
		case DPMSModeOn:
			OUTREG(TV_CTL, INREG(TV_CTL) | TV_ENC_ENABLE);
			break;
		case DPMSModeStandby:
		case DPMSModeSuspend:
		case DPMSModeOff:
			OUTREG(TV_CTL, INREG(TV_CTL) & ~TV_ENC_ENABLE);
			break;
	}
}

static void
i830_tv_save(xf86OutputPtr output)
{
	ScrnInfoPtr		    pScrn = output->scrn;
	I830Ptr		    pI830 = I830PTR(pScrn);
	I830OutputPrivatePtr    intel_output = output->driver_private;
	struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;
	int			    i;

	dev_priv->save_TV_H_CTL_1 = INREG(TV_H_CTL_1);
	dev_priv->save_TV_H_CTL_2 = INREG(TV_H_CTL_2);
	dev_priv->save_TV_H_CTL_3 = INREG(TV_H_CTL_3);
	dev_priv->save_TV_V_CTL_1 = INREG(TV_V_CTL_1);
	dev_priv->save_TV_V_CTL_2 = INREG(TV_V_CTL_2);
	dev_priv->save_TV_V_CTL_3 = INREG(TV_V_CTL_3);
	dev_priv->save_TV_V_CTL_4 = INREG(TV_V_CTL_4);
	dev_priv->save_TV_V_CTL_5 = INREG(TV_V_CTL_5);
	dev_priv->save_TV_V_CTL_6 = INREG(TV_V_CTL_6);
	dev_priv->save_TV_V_CTL_7 = INREG(TV_V_CTL_7);
	dev_priv->save_TV_SC_CTL_1 = INREG(TV_SC_CTL_1);
	dev_priv->save_TV_SC_CTL_2 = INREG(TV_SC_CTL_2);
	dev_priv->save_TV_SC_CTL_3 = INREG(TV_SC_CTL_3);

	dev_priv->save_TV_CSC_Y = INREG(TV_CSC_Y);
	dev_priv->save_TV_CSC_Y2 = INREG(TV_CSC_Y2);
	dev_priv->save_TV_CSC_U = INREG(TV_CSC_U);
	dev_priv->save_TV_CSC_U2 = INREG(TV_CSC_U2);
	dev_priv->save_TV_CSC_V = INREG(TV_CSC_V);
	dev_priv->save_TV_CSC_V2 = INREG(TV_CSC_V2);
	dev_priv->save_TV_CLR_KNOBS = INREG(TV_CLR_KNOBS);
	dev_priv->save_TV_CLR_LEVEL = INREG(TV_CLR_LEVEL);
	dev_priv->save_TV_WIN_POS = INREG(TV_WIN_POS);
	dev_priv->save_TV_WIN_SIZE = INREG(TV_WIN_SIZE);
	dev_priv->save_TV_FILTER_CTL_1 = INREG(TV_FILTER_CTL_1);
	dev_priv->save_TV_FILTER_CTL_2 = INREG(TV_FILTER_CTL_2);
	dev_priv->save_TV_FILTER_CTL_3 = INREG(TV_FILTER_CTL_3);

	for (i = 0; i < 60; i++)
		dev_priv->save_TV_H_LUMA[i] = INREG(TV_H_LUMA_0 + (i <<2));
	for (i = 0; i < 60; i++)
		dev_priv->save_TV_H_CHROMA[i] = INREG(TV_H_CHROMA_0 + (i <<2));
	for (i = 0; i < 43; i++)
		dev_priv->save_TV_V_LUMA[i] = INREG(TV_V_LUMA_0 + (i <<2));
	for (i = 0; i < 43; i++)
		dev_priv->save_TV_V_CHROMA[i] = INREG(TV_V_CHROMA_0 + (i <<2));

	dev_priv->save_TV_DAC = INREG(TV_DAC);
	dev_priv->save_TV_CTL = INREG(TV_CTL);
}

static void
i830_tv_restore(xf86OutputPtr output)
{
	ScrnInfoPtr		    pScrn = output->scrn;
	I830Ptr		    pI830 = I830PTR(pScrn);
	I830OutputPrivatePtr    intel_output = output->driver_private;
	struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;
	int			    i;

	OUTREG(TV_H_CTL_1, dev_priv->save_TV_H_CTL_1);
	OUTREG(TV_H_CTL_2, dev_priv->save_TV_H_CTL_2);
	OUTREG(TV_H_CTL_3, dev_priv->save_TV_H_CTL_3);
	OUTREG(TV_V_CTL_1, dev_priv->save_TV_V_CTL_1);
	OUTREG(TV_V_CTL_2, dev_priv->save_TV_V_CTL_2);
	OUTREG(TV_V_CTL_3, dev_priv->save_TV_V_CTL_3);
	OUTREG(TV_V_CTL_4, dev_priv->save_TV_V_CTL_4);
	OUTREG(TV_V_CTL_5, dev_priv->save_TV_V_CTL_5);
	OUTREG(TV_V_CTL_6, dev_priv->save_TV_V_CTL_6);
	OUTREG(TV_V_CTL_7, dev_priv->save_TV_V_CTL_7);
	OUTREG(TV_SC_CTL_1, dev_priv->save_TV_SC_CTL_1);
	OUTREG(TV_SC_CTL_2, dev_priv->save_TV_SC_CTL_2);
	OUTREG(TV_SC_CTL_3, dev_priv->save_TV_SC_CTL_3);

	OUTREG(TV_CSC_Y, dev_priv->save_TV_CSC_Y);
	OUTREG(TV_CSC_Y2, dev_priv->save_TV_CSC_Y2);
	OUTREG(TV_CSC_U, dev_priv->save_TV_CSC_U);
	OUTREG(TV_CSC_U2, dev_priv->save_TV_CSC_U2);
	OUTREG(TV_CSC_V, dev_priv->save_TV_CSC_V);
	OUTREG(TV_CSC_V2, dev_priv->save_TV_CSC_V2);
	OUTREG(TV_CLR_KNOBS, dev_priv->save_TV_CLR_KNOBS);
	OUTREG(TV_CLR_LEVEL, dev_priv->save_TV_CLR_LEVEL);

	{
		xf86CrtcPtr	    crtc = output->crtc;
		I830CrtcPrivatePtr	    intel_crtc = crtc->driver_private;
		int pipeconf_reg = (intel_crtc->pipe == 0)?PIPEACONF:PIPEBCONF;
		int dspcntr_reg = (intel_crtc->pipe == 0)?DSPACNTR : DSPBCNTR;
		int pipeconf = INREG(pipeconf_reg);
		int dspcntr = INREG(dspcntr_reg);
		int dspbase_reg = (intel_crtc->pipe == 0) ? DSPABASE : DSPBBASE;
		/* Pipe must be off here */
		OUTREG(dspcntr_reg, dspcntr & ~DISPLAY_PLANE_ENABLE);
		/* Flush the plane changes */
		OUTREG(dspbase_reg, INREG(dspbase_reg));

		if (!IS_I9XX(pI830)) {
			/* Wait for vblank for the disable to take effect */
			i830WaitForVblank(pScrn);
		}

		OUTREG(pipeconf_reg, pipeconf & ~PIPEACONF_ENABLE);
		/* Wait for vblank for the disable to take effect. */
		i830WaitForVblank(pScrn);

		/* Filter ctl must be set before TV_WIN_SIZE and TV_WIN_POS */
		OUTREG(TV_FILTER_CTL_1, dev_priv->save_TV_FILTER_CTL_1);
		OUTREG(TV_FILTER_CTL_2, dev_priv->save_TV_FILTER_CTL_2);
		OUTREG(TV_FILTER_CTL_3, dev_priv->save_TV_FILTER_CTL_3);
		OUTREG(TV_WIN_POS, dev_priv->save_TV_WIN_POS);
		OUTREG(TV_WIN_SIZE, dev_priv->save_TV_WIN_SIZE);
		OUTREG(pipeconf_reg, pipeconf);
		OUTREG(dspcntr_reg, dspcntr);
		/* Flush the plane changes */
		OUTREG(dspbase_reg, INREG(dspbase_reg));
	}

	for (i = 0; i < 60; i++)
		OUTREG(TV_H_LUMA_0 + (i <<2), dev_priv->save_TV_H_LUMA[i]);
	for (i = 0; i < 60; i++)
		OUTREG(TV_H_CHROMA_0 + (i <<2), dev_priv->save_TV_H_CHROMA[i]);
	for (i = 0; i < 43; i++)
		OUTREG(TV_V_LUMA_0 + (i <<2), dev_priv->save_TV_V_LUMA[i]);
	for (i = 0; i < 43; i++)
		OUTREG(TV_V_CHROMA_0 + (i <<2), dev_priv->save_TV_V_CHROMA[i]);

	OUTREG(TV_DAC, dev_priv->save_TV_DAC);
	OUTREG(TV_CTL, dev_priv->save_TV_CTL);
}

static int
i830_tv_mode_valid(xf86OutputPtr output, DisplayModePtr pMode)
{
	return MODE_OK;
}


static Bool
i830_tv_mode_fixup(xf86OutputPtr output, DisplayModePtr mode,
		DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr pScrn = output->scrn;
	xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
	int i;

	for (i = 0; i < xf86_config->num_output; i++) {
		xf86OutputPtr other_output = xf86_config->output[i];

		if (other_output != output && other_output->crtc == output->crtc) {
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
					"Can't enable TV and another output on the same "
					"pipe\n");
			return FALSE;
		}
	}

	/* XXX: fill me in */

	return TRUE;
}

static CARD32
i830_float_to_csc (float fin)
{
	CARD32  exp;
	CARD32  mant;
	CARD32  ret;
	float   f = fin;

	/* somehow the color conversion knows the signs of all the values */
	if (f < 0) f = -f;

	if (f >= 1)
	{
		exp = 0x7;
		mant = 1 << 8;
	}
	else
	{
		for (exp = 0; exp < 3 && f < 0.5; exp++)
			f *= 2.0;
		mant = (f * (1 << 9) + 0.5);
		if (mant >= (1 << 9))
			mant = (1 << 9) - 1;
	}
	ret = (exp << 9) | mant;
	return ret;
}

static CARD16
i830_float_to_luma (float f)
{
	CARD16  ret;

	ret = (f * (1 << 9));
	return ret;
}

static void
i830_tv_mode_set(xf86OutputPtr output, DisplayModePtr mode,
		DisplayModePtr adjusted_mode)
{
	ScrnInfoPtr		    pScrn = output->scrn;
	I830Ptr		    pI830 = I830PTR(pScrn);
	xf86CrtcPtr	    crtc = output->crtc;
	I830OutputPrivatePtr    intel_output = output->driver_private;
	I830CrtcPrivatePtr	    intel_crtc = crtc->driver_private;
	struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;
	const tv_mode_t	    *tv_mode;
	CARD32		    tv_ctl;
	CARD32		    hctl1, hctl2, hctl3;
	CARD32		    vctl1, vctl2, vctl3, vctl4, vctl5, vctl6, vctl7;
	CARD32		    scctl1, scctl2, scctl3;
	int			    i, j;
	const video_levels_t	*video_levels;
	const color_conversion_t	*color_conversion;
	Bool		    burst_ena;

	for (i = 0; i < sizeof(tv_modes) / sizeof (tv_modes[0]); i++) {
		tv_mode = &tv_modes[i];
		if (strstr(mode->name, tv_mode->name)) 
			break;	
	}

	tv_ctl = 0;

	switch (dev_priv->type) {
		default:
		case TV_TYPE_UNKNOWN:
		case TV_TYPE_COMPOSITE:
			tv_ctl |= TV_ENC_OUTPUT_COMPOSITE;
			video_levels = &tv_mode->composite_levels;
			color_conversion = &tv_mode->composite_color;
			burst_ena = tv_mode->burst_ena;
			break;
		case TV_TYPE_COMPONENT:
			tv_ctl |= TV_ENC_OUTPUT_COMPONENT;
			video_levels = &component_level;
			if (tv_mode->burst_ena)
				color_conversion = &sdtv_component_color;
			else
				color_conversion = &hdtv_component_color;
			burst_ena = FALSE;
			break;
		case TV_TYPE_SVIDEO:
			tv_ctl |= TV_ENC_OUTPUT_SVIDEO;
			video_levels = &tv_mode->svideo_levels;
			color_conversion = &tv_mode->svideo_color;
			burst_ena = tv_mode->burst_ena;
			break;
	}
	hctl1 = (tv_mode->hsync_end << TV_HSYNC_END_SHIFT) |
		(tv_mode->htotal << TV_HTOTAL_SHIFT);

	hctl2 = (tv_mode->hburst_start << 16) |
		(tv_mode->hburst_len << TV_HBURST_LEN_SHIFT);
	if (burst_ena)
		hctl2 |= TV_BURST_ENA;

	hctl3 = (tv_mode->hblank_start << TV_HBLANK_START_SHIFT) |
		(tv_mode->hblank_end << TV_HBLANK_END_SHIFT);

	vctl1 = (tv_mode->nbr_end << TV_NBR_END_SHIFT) |
		(tv_mode->vi_end_f1 << TV_VI_END_F1_SHIFT) |
		(tv_mode->vi_end_f2 << TV_VI_END_F2_SHIFT);

	vctl2 = (tv_mode->vsync_len << TV_VSYNC_LEN_SHIFT) |
		(tv_mode->vsync_start_f1 << TV_VSYNC_START_F1_SHIFT) |
		(tv_mode->vsync_start_f2 << TV_VSYNC_START_F2_SHIFT);

	vctl3 = (tv_mode->veq_len << TV_VEQ_LEN_SHIFT) |
		(tv_mode->veq_start_f1 << TV_VEQ_START_F1_SHIFT) |
		(tv_mode->veq_start_f2 << TV_VEQ_START_F2_SHIFT);
	if (tv_mode->veq_ena)
		vctl3 |= TV_EQUAL_ENA;

	vctl4 = (tv_mode->vburst_start_f1 << TV_VBURST_START_F1_SHIFT) |
		(tv_mode->vburst_end_f1 << TV_VBURST_END_F1_SHIFT);

	vctl5 = (tv_mode->vburst_start_f2 << TV_VBURST_START_F2_SHIFT) |
		(tv_mode->vburst_end_f2 << TV_VBURST_END_F2_SHIFT);

	vctl6 = (tv_mode->vburst_start_f3 << TV_VBURST_START_F3_SHIFT) |
		(tv_mode->vburst_end_f3 << TV_VBURST_END_F3_SHIFT);

	vctl7 = (tv_mode->vburst_start_f4 << TV_VBURST_START_F4_SHIFT) |
		(tv_mode->vburst_end_f4 << TV_VBURST_END_F4_SHIFT);

	if (intel_crtc->pipe == 1)
		tv_ctl |= TV_ENC_PIPEB_SELECT;
	tv_ctl |= tv_mode->oversample;

	if (tv_mode->progressive)
		tv_ctl |= TV_PROGRESSIVE;
	if (tv_mode->trilevel_sync)
		tv_ctl |= TV_TRILEVEL_SYNC;
	if (tv_mode->pal_burst)
		tv_ctl |= TV_PAL_BURST;
	if (tv_mode->oversample == TV_OVERSAMPLE_8X)
		scctl1 = TV_SC_DDA1_EN;

	if (tv_mode->dda2_inc)
		scctl1 |= TV_SC_DDA2_EN;

	if (tv_mode->dda3_inc)
		scctl1 |= TV_SC_DDA3_EN;

	scctl1 |= tv_mode->sc_reset;
	scctl1 |= video_levels->burst << TV_BURST_LEVEL_SHIFT;
	scctl1 |= tv_mode->dda1_inc << TV_SCDDA1_INC_SHIFT;

	scctl2 = tv_mode->dda2_size << TV_SCDDA2_SIZE_SHIFT |
		tv_mode->dda2_inc << TV_SCDDA2_INC_SHIFT;

	scctl3 = tv_mode->dda3_size << TV_SCDDA3_SIZE_SHIFT |
		tv_mode->dda3_inc << TV_SCDDA3_INC_SHIFT;

	/* Enable two fixes for the chips that need them. */
	if (pI830->PciInfo->chipType < PCI_CHIP_I945_G)
		tv_ctl |= TV_ENC_C0_FIX | TV_ENC_SDP_FIX;


	OUTREG(TV_H_CTL_1, hctl1);
	OUTREG(TV_H_CTL_2, hctl2);
	OUTREG(TV_H_CTL_3, hctl3);
	OUTREG(TV_V_CTL_1, vctl1);
	OUTREG(TV_V_CTL_2, vctl2);
	OUTREG(TV_V_CTL_3, vctl3);
	OUTREG(TV_V_CTL_4, vctl4);
	OUTREG(TV_V_CTL_5, vctl5);
	OUTREG(TV_V_CTL_6, vctl6);
	OUTREG(TV_V_CTL_7, vctl7);
	OUTREG(TV_SC_CTL_1, scctl1);
	OUTREG(TV_SC_CTL_2, scctl2);
	OUTREG(TV_SC_CTL_3, scctl3);

	OUTREG(TV_CSC_Y,
			(i830_float_to_csc(color_conversion->ry) << 16) |
			(i830_float_to_csc(color_conversion->gy)));
	OUTREG(TV_CSC_Y2,
			(i830_float_to_csc(color_conversion->by) << 16) |
			(i830_float_to_luma(color_conversion->ay)));

	OUTREG(TV_CSC_U,
			(i830_float_to_csc(color_conversion->ru) << 16) |
			(i830_float_to_csc(color_conversion->gu)));

	OUTREG(TV_CSC_U2,
			(i830_float_to_csc(color_conversion->bu) << 16) |
			(i830_float_to_luma(color_conversion->au)));

	OUTREG(TV_CSC_V,
			(i830_float_to_csc(color_conversion->rv) << 16) |
			(i830_float_to_csc(color_conversion->gv)));

	OUTREG(TV_CSC_V2,
			(i830_float_to_csc(color_conversion->bv) << 16) |
			(i830_float_to_luma(color_conversion->av)));

	OUTREG(TV_CLR_KNOBS, 0x10606000);
	OUTREG(TV_CLR_LEVEL, ((video_levels->black << TV_BLACK_LEVEL_SHIFT) |
				(video_levels->blank << TV_BLANK_LEVEL_SHIFT)));
	{
		int pipeconf_reg = (intel_crtc->pipe == 0)?PIPEACONF:PIPEBCONF;
		int dspcntr_reg = (intel_crtc->pipe == 0)?DSPACNTR : DSPBCNTR;
		int pipeconf = INREG(pipeconf_reg);
		int dspcntr = INREG(dspcntr_reg);
		int dspbase_reg = (intel_crtc->pipe == 0) ? DSPABASE : DSPBBASE;
		int xpos = 0x0, ypos = 0x0;
		unsigned int xsize, ysize;
		/* Pipe must be off here */
		OUTREG(dspcntr_reg, dspcntr & ~DISPLAY_PLANE_ENABLE);
		/* Flush the plane changes */
		OUTREG(dspbase_reg, INREG(dspbase_reg));

		if (!IS_I9XX(pI830)) {
			/* Wait for vblank for the disable to take effect */
			i830WaitForVblank(pScrn);
		}

		OUTREG(pipeconf_reg, pipeconf & ~PIPEACONF_ENABLE);
		/* Wait for vblank for the disable to take effect. */
		i830WaitForVblank(pScrn);

		/* Filter ctl must be set before TV_WIN_SIZE and TV_WIN_POS */
		OUTREG(TV_FILTER_CTL_1, TV_AUTO_SCALE);

		xsize = tv_mode->hblank_start - tv_mode->hblank_end;
		if (tv_mode->progressive)
			ysize = tv_mode->nbr_end + 1;
		else
			ysize = 2*tv_mode->nbr_end + 1;

		OUTREG(TV_WIN_POS, (xpos<<16)|ypos);
		OUTREG(TV_WIN_SIZE, (xsize<<16)|ysize);

		OUTREG(pipeconf_reg, pipeconf);
		OUTREG(dspcntr_reg, dspcntr);
		/* Flush the plane changes */
		OUTREG(dspbase_reg, INREG(dspbase_reg));
	} 	

	j = 0;
	for (i = 0; i < 60; i++)
		OUTREG(TV_H_LUMA_0 + (i<<2), tv_mode->filter_table[j++]);
	for (i = 0; i < 60; i++)
		OUTREG(TV_H_CHROMA_0 + (i<<2), tv_mode->filter_table[j++]);
	for (i = 0; i < 43; i++)
		OUTREG(TV_V_LUMA_0 + (i<<2), tv_mode->filter_table[j++]);
	for (i = 0; i < 43; i++)
		OUTREG(TV_V_CHROMA_0 + (i<<2), tv_mode->filter_table[j++]);
	OUTREG(TV_DAC, 0);
	OUTREG(TV_CTL, tv_ctl);
}

static const DisplayModeRec reported_modes[] = {
	{
		.name = "NTSC 480i",
		.Clock = 107520,
		.HDisplay   = 1280,
		.HSyncStart = 1368,
		.HSyncEnd   = 1496,
		.HTotal     = 1712,

		.VDisplay   = 1024,
		.VSyncStart = 1027,
		.VSyncEnd   = 1034,
		.VTotal     = 1104,
		.type       = M_T_DRIVER
	},
};

/**
 * Detects TV presence by checking for load.
 *
 * Requires that the current pipe's DPLL is active.

 * \return TRUE if TV is connected.
 * \return FALSE if TV is disconnected.
 */
static void
i830_tv_detect_type (xf86CrtcPtr    crtc,
		xf86OutputPtr  output)
{
	ScrnInfoPtr		    pScrn = output->scrn;
	I830Ptr		    pI830 = I830PTR(pScrn);
	I830OutputPrivatePtr    intel_output = output->driver_private;
	struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;
	CARD32		    tv_ctl, save_tv_ctl;
	CARD32		    tv_dac, save_tv_dac;
	int			    type = TV_TYPE_UNKNOWN;

	tv_dac = INREG(TV_DAC);
	/*
	 * Detect TV by polling)
	 */
	if (intel_output->load_detect_temp)
	{
		/* TV not currently running, prod it with destructive detect */
		save_tv_dac = tv_dac;
		tv_ctl = INREG(TV_CTL);
		save_tv_ctl = tv_ctl;
		tv_ctl &= ~TV_ENC_ENABLE;
		tv_ctl &= ~TV_TEST_MODE_MASK;
		tv_ctl |= TV_TEST_MODE_MONITOR_DETECT;
		tv_dac &= ~TVDAC_SENSE_MASK;
		tv_dac |= (TVDAC_STATE_CHG_EN |
				TVDAC_A_SENSE_CTL |
				TVDAC_B_SENSE_CTL |
				TVDAC_C_SENSE_CTL |
				DAC_CTL_OVERRIDE |
				DAC_A_0_7_V |
				DAC_B_0_7_V |
				DAC_C_0_7_V);
		OUTREG(TV_CTL, tv_ctl);
		OUTREG(TV_DAC, tv_dac);
		i830WaitForVblank(pScrn);
		tv_dac = INREG(TV_DAC);
		OUTREG(TV_DAC, save_tv_dac);
		OUTREG(TV_CTL, save_tv_ctl);
	}
	/*
	 *  A B C
	 *  0 1 1 Composite
	 *  1 0 X svideo
	 *  0 0 0 Component
	 */
	if ((tv_dac & TVDAC_SENSE_MASK) == (TVDAC_B_SENSE | TVDAC_C_SENSE)) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"Detected Composite TV connection\n");
		type = TV_TYPE_COMPOSITE;
	} else if ((tv_dac & (TVDAC_A_SENSE|TVDAC_B_SENSE)) == TVDAC_A_SENSE) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"Detected S-Video TV connection\n");
		type = TV_TYPE_SVIDEO;
	} else if ((tv_dac & TVDAC_SENSE_MASK) == 0) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"Detected Component TV connection\n");
		type = TV_TYPE_COMPONENT;
	} else {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO,
				"No TV connection detected\n");
		type = TV_TYPE_NONE;
	}

	dev_priv->type = type;
}

/**
 * Detect the TV connection.
 *
 * Currently this always returns OUTPUT_STATUS_UNKNOWN, as we need to be sure
 * we have a pipe programmed in order to probe the TV.
 */
static xf86OutputStatus
i830_tv_detect(xf86OutputPtr output)
{
	xf86CrtcPtr		    crtc;
	DisplayModeRec	    mode;
	I830OutputPrivatePtr    intel_output = output->driver_private;
	struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;

	crtc = i830GetLoadDetectPipe (output);
	if (crtc)
	{
		if (intel_output->load_detect_temp)
		{
			/* we only need the pixel clock set correctly here */
			mode = reported_modes[0];
			xf86SetModeCrtc (&mode, INTERLACE_HALVE_V);
			i830PipeSetMode (crtc, &mode, FALSE);
		}
		i830_tv_detect_type (crtc, output);
		i830ReleaseLoadDetectPipe (output);
	}

	switch (dev_priv->type) {
		case TV_TYPE_NONE:
			return XF86OutputStatusDisconnected;
		case TV_TYPE_UNKNOWN:
			return XF86OutputStatusUnknown;
		default:
			return XF86OutputStatusConnected;
	}
}

struct input_res {
	char *name;
	int w, h;	
}input_res_table[] = 
{
	{"640x480", 640, 480},
	{"800x600", 800, 600},
	{"1024x768", 1024, 768},
	{"1280x1024", 1280, 1024},
	{"848x480", 848, 480},
	{"1280x720", 1280, 720}
};

/**
 * Stub get_modes function.
 *
 * This should probably return a set of fixed modes, unless we can figure out
 * how to probe modes off of TV connections.
 */

static DisplayModePtr
i830_tv_get_modes(xf86OutputPtr output)
{
	DisplayModePtr  ret = NULL, mode_ptr;
	int		    i, j;
	I830OutputPrivatePtr    intel_output = output->driver_private;
	struct i830_tv_priv	    *dev_priv = intel_output->dev_priv;

	for (i = 0; i < sizeof(tv_modes) / sizeof (tv_modes[0]); i++) 
	{
		const tv_mode_t *tv_mode = &tv_modes[i];
		unsigned int hactive = tv_mode->hblank_start - tv_mode->hblank_end;
		unsigned int vactive = tv_mode->progressive
			?tv_mode->nbr_end + 1: 2*(tv_mode->nbr_end + 1);
		unsigned int htotal = tv_mode->htotal + 1;
		unsigned int vtotal = tv_mode->progressive
			?tv_mode->nbr_end + 1 + tv_mode->vi_end_f2:
			2*(tv_mode->nbr_end+1) + 2*(tv_mode->vi_end_f2);

		if (dev_priv->type != TV_TYPE_COMPONENT && tv_mode->component_only)
			continue;

		for (j = 0; j < sizeof(input_res_table)/sizeof(input_res_table[0]); j++)	{
			struct input_res *input = &input_res_table[j];
			unsigned int hactive_s = input->w;
			unsigned int vactive_s = input->h;
			unsigned int htotal_s = htotal*hactive_s/hactive;
			unsigned int vtotal_s = vtotal*vactive_s/vactive;

			mode_ptr = xnfcalloc(1, sizeof(DisplayModeRec));
			mode_ptr->name = xnfalloc(strlen(tv_mode->name) + 
					strlen(input->name) + 4);
			sprintf(mode_ptr->name, "%s %s", tv_mode->name, input->name);

			mode_ptr->Clock = tv_mode->clock;

			mode_ptr->HDisplay = hactive_s;
			mode_ptr->HSyncStart = hactive_s + 1;
			mode_ptr->HSyncEnd = htotal_s - 20;  
			if ( mode_ptr->HSyncEnd <= mode_ptr->HSyncStart)
				mode_ptr->HSyncEnd = mode_ptr->HSyncStart  + 1;
			mode_ptr->HTotal = htotal_s;

			mode_ptr->VDisplay = vactive_s;
			mode_ptr->VSyncStart = vactive_s + 1;
			mode_ptr->VSyncEnd = vtotal_s - 20;
			if ( mode_ptr->VSyncEnd <= mode_ptr->VSyncStart)
				mode_ptr->VSyncEnd = mode_ptr->VSyncStart  + 1;
			mode_ptr->VTotal = vtotal_s;

			mode_ptr->type = M_T_DRIVER;
			mode_ptr->next = ret;
			ret = mode_ptr;
		} 
	}

	return ret;
}

static void
i830_tv_destroy (xf86OutputPtr output)
{
	if (output->driver_private)
		xfree (output->driver_private);
}

static const xf86OutputFuncsRec i830_tv_output_funcs = {
	.dpms = i830_tv_dpms,
	.save = i830_tv_save,
	.restore = i830_tv_restore,
	.mode_valid = i830_tv_mode_valid,
	.mode_fixup = i830_tv_mode_fixup,
	.mode_set = i830_tv_mode_set,
	.detect = i830_tv_detect,
	.get_modes = i830_tv_get_modes,
	.destroy = i830_tv_destroy
};

void
i830_tv_init(ScrnInfoPtr pScrn)
{
	I830Ptr		    pI830 = I830PTR(pScrn);
	xf86OutputPtr	    output;
	I830OutputPrivatePtr    intel_output;
	struct i830_tv_priv	    *dev_priv;
	CARD32		    tv_dac_on, tv_dac_off, save_tv_dac;

	if ((INREG(TV_CTL) & TV_FUSE_STATE_MASK) == TV_FUSE_STATE_DISABLED)
		return;

	/*
     * Sanity check the TV output by checking to see if the
     * DAC register holds a value
     */
    save_tv_dac = INREG(TV_DAC);
    
    OUTREG(TV_DAC, save_tv_dac | TVDAC_STATE_CHG_EN);
    tv_dac_on = INREG(TV_DAC);
    
    OUTREG(TV_DAC, save_tv_dac & ~TVDAC_STATE_CHG_EN);
    tv_dac_off = INREG(TV_DAC);
    
    OUTREG(TV_DAC, save_tv_dac);
    
    /*
     * If the register does not hold the state change enable
     * bit, (either as a 0 or a 1), assume it doesn't really
     * exist
     */
    if ((tv_dac_on & TVDAC_STATE_CHG_EN) == 0 || 
	(tv_dac_off & TVDAC_STATE_CHG_EN) != 0)
	return;
    
    output = xf86OutputCreate (pScrn, &i830_tv_output_funcs, "TV");
    
    if (!output)
	return;
    
    intel_output = xnfcalloc (sizeof (I830OutputPrivateRec) +
			      sizeof (struct i830_tv_priv), 1);
    if (!intel_output)
    {
	xf86OutputDestroy (output);
	return;
    }
    dev_priv = (struct i830_tv_priv *) (intel_output + 1);
    intel_output->type = I830_OUTPUT_TVOUT;
    intel_output->dev_priv = dev_priv;
    dev_priv->type = TV_TYPE_UNKNOWN;
    
    output->driver_private = intel_output;
    output->interlaceAllowed = FALSE;
    output->doubleScanAllowed = FALSE;
}
