/* -*- c-basic-offset: 4 -*- */
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
 * This file contains the register definitions for the i82807aa.
 *
 * Documentation on this chipset can be found in datasheet #29069001 at
 * intel.com.
 */
#ifndef I82807AA_REG_H
#define I82807AA_REG_H

/** @defgroup VR00
 * @{
 */
#define VR00		0x00
# define VR00_BASE_ADDRESS_MASK		0x007f
/** @} */

/** @defgroup VR01
 * @{
 */
#define VR01		0x01
# define VR01_PANEL_FIT_ENABLE		(1 << 3)
/**
 * Enables the LCD display.
 *
 * This must not be set while VR01_DVO_BYPASS_ENABLE is set.
 */
# define VR01_LCD_ENABLE		(1 << 2)
/** Enables the DVO repeater. */
# define VR01_DVO_BYPASS_ENABLE		(1 << 1)
/** Enables the DVO clock */
# define VR01_DVO_ENABLE		(1 << 0)
/** @} */

/** @defgroup VR10
 * @{
 */
#define VR10		0x10
/** Enables LVDS output instead of CMOS */
# define VR10_LVDS_ENABLE		(1 << 4)
/** Enables 18-bit LVDS output. */
# define VR10_INTERFACE_1X18		(0 << 2)
/** Enables 24-bit LVDS or CMOS output */
# define VR10_INTERFACE_1X24		(1 << 2)
/** Enables 2x18-bit LVDS or CMOS output. */
# define VR10_INTERFACE_2X18		(2 << 2)
/** Enables 2x24-bit LVDS output */
# define VR10_INTERFACE_2X24		(3 << 2)
/** @} */

/** @defgroup VR30
 * @{
 */
#define VR30		0x30
/** Read only bit indicating that the panel is not in a safe poweroff state. */
# define VR30_PANEL_ON			(1 << 15)
/** @} */

/** @defgroup VR40
 * @{
 */
#define VR40		0x40
# define VR40_STALL_ENABLE		(1 << 13)
# define VR40_VERTICAL_INTERP_ENABLE	(1 << 11)
# define VR40_HORIZONTAL_INTERP_ENABLE	(1 << 10)
# define VR40_RATIO_ENABLE		(1 << 9)
# define VR40_PANEL_FIT_ENABLE		(1 << 8)
/** @} */

#endif /* I82807AA_REG_H */
