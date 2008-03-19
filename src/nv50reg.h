/*
 * Copyright 2007 Maarten Maathuis
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

#ifndef __NV50REG_H_
#define __NV50REG_H_

/* These are probably redrirected from 0x4000 range (very similar regs to nv40, maybe different order) */
#define NV50_CRTC_VPLL1_A		0x00614104
#define NV50_CRTC_VPLL1_B		0x00614108
#define NV50_CRTC_VPLL2_A		0x00614904
#define NV50_CRTC_VPLL2_B		0x00614908

/* Clamped to 256 MiB */
#define NV50_CRTC0_RAM_AMOUNT		0x00610384
#define NV50_CRTC1_RAM_AMOUNT		0x00610784

/* These things below are so called "commands" */
#define NV50_UPDATE_DISPLAY		0x80

#define NV50_CRTC0_CLOCK			0x804
#define NV50_CRTC0_INTERLACE		0x808

/* Anyone know what part of the chip is triggered here precisely? */
#define NV84_CRTC0_BLANK_UNK1		0x85C
	#define NV84_CRTC0_BLANK_UNK1_BLANK	0x0
	#define NV84_CRTC0_BLANK_UNK1_UNBLANK	0x1

#define NV50_CRTC0_FB_SIZE			0x868
#define NV50_CRTC0_PITCH			0x86C

/* I'm openminded to better interpretations. */
/* This is an educated guess. */
/* NV50 has RAMDAC and TMDS offchip, so it's unlikely to be that. */
#define NV50_CRTC0_BLANK_CTRL		0x874
	#define NV50_CRTC0_BLANK_CTRL_BLANK	0x0
	#define NV50_CRTC0_BLANK_CTRL_UNBLANK	0x1

/* Anyone know what part of the chip is triggered here precisely? */
#define NV84_CRTC0_BLANK_UNK2		0x89C
	#define NV84_CRTC0_BLANK_UNK2_BLANK	0x0
	#define NV84_CRTC0_BLANK_UNK2_UNBLANK	0x1

#define NV50_CRTC0_FB_POS			0x8C0
#define NV50_CRTC0_SCRN_SIZE		0x8C8

#define NV50_CRTC0_HBLANK_START	0x814
#define NV50_CRTC0_HSYNC_END		0x818
#define NV50_CRTC0_HBLANK_END		0x81C
#define NV50_CRTC0_HTOTAL			0x820

#define NV50_CRTC1_CLOCK			0xC04
#define NV50_CRTC1_INTERLACE		0xC08

/* Anyone know what part of the chip is triggered here precisely? */
#define NV84_CRTC1_BLANK_UNK1		0xC5C
	#define NV84_CRTC1_BLANK_UNK1_BLANK	0x0
	#define NV84_CRTC1_BLANK_UNK1_UNBLANK	0x1

/* I'm openminded to better interpretations. */
#define NV50_CRTC1_BLANK_CTRL		0xC74
	#define NV50_CRTC1_BLANK_CTRL_BLANK	0x0
	#define NV50_CRTC1_BLANK_CTRL_UNBLANK	0x1

/* Anyone know what part of the chip is triggered here precisely? */
#define NV84_CRTC1_BLANK_UNK2		0xC9C
	#define NV84_CRTC1_BLANK_UNK2_BLANK	0x0
	#define NV84_CRTC1_BLANK_UNK2_UNBLANK	0x1

#define NV50_CRTC1_HBLANK_START	0xC14
#define NV50_CRTC1_HSYNC_END		0xC18
#define NV50_CRTC1_HBLANK_END		0xC1C
#define NV50_CRTC1_HTOTAL			0xC20

#define NV50_CRTC1_FB_SIZE			0xC68
#define NV50_CRTC1_PITCH			0xC6C

#define NV50_CRTC1_FB_POS			0xCC0
#define NV50_CRTC1_SCRN_SIZE		0xCC8

#define NV50_CRTC0_DEPTH			0x870
#define NV50_CRTC0_DEPTH_8BPP		0x1E00
#define NV50_CRTC0_DEPTH_15BPP	0xE900
#define NV50_CRTC0_DEPTH_16BPP	0xE800
#define NV50_CRTC0_DEPTH_24BPP	0xCF00

#define NV50_CRTC1_DEPTH			0xC70
	#define NV50_CRTC1_DEPTH_8BPP		0x1E00
	#define NV50_CRTC1_DEPTH_15BPP	0xE900
	#define NV50_CRTC1_DEPTH_16BPP	0xE800
	#define NV50_CRTC1_DEPTH_24BPP	0xCF00

#define NV50_CRTC0_FB_OFFSET		0x860
#define NV50_CRTC1_FB_OFFSET		0xC60

#define NV50_CRTC0_CURSOR_OFFSET	0x884
#define NV50_CRTC1_CURSOR_OFFSET	0xC84

/* You can't have a palette in 8 bit mode (=OFF) */
#define NV50_CRTC0_CLUT_MODE		0x840
	#define NV50_CRTC0_CLUT_MODE_BLANK		0x00000000
	#define NV50_CRTC0_CLUT_MODE_OFF		0x80000000
	#define NV50_CRTC0_CLUT_MODE_ON		0xC0000000
#define NV50_CRTC0_CLUT_OFFSET		0x844

#define NV50_CRTC1_CLUT_MODE		0xC40
	#define NV50_CRTC1_CLUT_MODE_BLANK		0x00000000
	#define NV50_CRTC1_CLUT_MODE_OFF		0x80000000
	#define NV50_CRTC1_CLUT_MODE_ON		0xC0000000
#define NV50_CRTC1_CLUT_OFFSET		0xC44

#define NV50_CRTC0_CURSOR0		0x880
	#define NV50_CRTC0_CURSOR0_SHOW		0x85000000
	#define NV50_CRTC0_CURSOR0_HIDE		0x05000000

#define NV50_CRTC1_CURSOR0		0xC80
	#define NV50_CRTC1_CURSOR0_SHOW		0x85000000
	#define NV50_CRTC1_CURSOR0_HIDE		0x05000000

#endif /* __NV50REG_H_ */
