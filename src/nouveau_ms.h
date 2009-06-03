/*
 * Copyright 2006 Dave Airlie
 * Copyright 2007 Maarten Maathuis
 * Copyright 2007-2009 Stuart Bennett
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
 */

#ifndef __NOUVEAU_MS_H__
#define __NOUVEAU_MS_H__

//#define NOUVEAU_DEBUG
#ifdef NOUVEAU_DEBUG
#define NV_DEBUG(d, fmt, arg...) xf86DrvMsg(d->scrnIndex, X_INFO, fmt, ##arg)
#else
#define NV_DEBUG(d, fmt, arg...)
#endif
#define NV_ERROR(d, fmt, arg...) xf86DrvMsg(d->scrnIndex, X_ERROR, fmt, ##arg)
#define NV_INFO(d, fmt, arg...) xf86DrvMsg(d->scrnIndex, X_PROBED, fmt, ##arg)
#define NV_TRACEWARN(d, fmt, arg...) xf86DrvMsg(d->scrnIndex, X_NOTICE, fmt, ##arg)
#define NV_TRACE(d, fmt, arg...) xf86DrvMsg(d->scrnIndex, X_INFO, fmt, ##arg)
#define NV_WARN(d, fmt, arg...) xf86DrvMsg(d->scrnIndex, X_WARNING, fmt, ##arg)

#define NV_DPMS_CLEARED 0x80

enum scaling_modes {
	SCALE_PANEL,
	SCALE_FULLSCREEN,
	SCALE_ASPECT,
	SCALE_NOSCALE,
	SCALE_INVALID
};

struct nouveau_pll_vals {
	union {
		struct {
#if X_BYTE_ORDER == X_BIG_ENDIAN
			uint8_t N1, M1, N2, M2;
#else
			uint8_t M1, N1, M2, N2;
#endif
		};
		struct {
			uint16_t NM1, NM2;
		} __attribute__((packed));
	};
	int log2P;

	int refclk;
};

struct nouveau_crtc_state {
	uint8_t MiscOutReg;
	uint8_t CRTC[0x9f];
	uint8_t CR58[0x10];
	uint8_t Sequencer[5];
	uint8_t Graphics[9];
	uint8_t Attribute[21];
	uint8_t DAC[768];

	/* PCRTC regs */
	uint32_t fb_start;
	uint32_t crtc_cfg;
	uint32_t cursor_cfg;
	uint32_t gpio_ext;
	uint32_t crtc_830;
	uint32_t crtc_834;
	uint32_t crtc_850;
	uint32_t crtc_eng_ctrl;

	/* PRAMDAC regs */
	uint32_t nv10_cursync;
	struct nouveau_pll_vals pllvals;
	uint32_t ramdac_gen_ctrl;
	uint32_t ramdac_630;
	uint32_t ramdac_634;
	uint32_t fp_horiz_regs[7];
	uint32_t fp_vert_regs[7];
	uint32_t dither;
	uint32_t fp_control;
	uint32_t dither_regs[6];
	uint32_t fp_debug_0;
	uint32_t fp_debug_1;
	uint32_t fp_debug_2;
	uint32_t ramdac_a20;
	uint32_t ramdac_a24;
	uint32_t ramdac_a34;
};

struct nouveau_encoder_state {
	uint32_t output;
	int head;
};

struct nouveau_mode_state
{
	uint32_t pllsel;
	uint32_t sel_clk;

	struct nouveau_crtc_state head[2];
};

struct nouveau_crtc {
	int head;
	uint8_t last_dpms;
	int fp_users;
	uint32_t dpms_saved_fp_control;
	int saturation, sharpness;

	/* convenient pointer to pNv->set_state.head[head_nr] */
	struct nouveau_crtc_state *state;

	uint32_t cursor_fg, cursor_bg;
	ExaOffscreenArea *shadow;
};

struct nouveau_encoder {
	uint8_t last_dpms;
	struct dcb_entry *dcb;
	DisplayModePtr native_mode;
	uint8_t scaling_mode;
	bool dithering;
	bool dual_link;
	struct nouveau_encoder_state restore;
};

struct nouveau_connector {
	xf86MonPtr edid;
	I2CBusPtr pDDCBus;
	uint16_t possible_encoders;
	struct nouveau_encoder *detected_encoder;
	struct nouveau_encoder *nv_encoder;
};

#define to_nouveau_connector(x) ((struct nouveau_connector *)(x)->driver_private)
#define to_nouveau_crtc(x) ((struct nouveau_crtc *)(x)->driver_private)
#define to_nouveau_encoder(x) ((struct nouveau_connector *)(x)->driver_private)->nv_encoder

#endif /* __NOUVEAU_MS_H__ */
