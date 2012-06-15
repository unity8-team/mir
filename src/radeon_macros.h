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
 * References:
 *
 * !!!! FIXME !!!!
 *   RAGE 128 VR/ RAGE 128 GL Register Reference Manual (Technical
 *   Reference Manual P/N RRG-G04100-C Rev. 0.04), ATI Technologies: April
 *   1999.
 *
 * !!!! FIXME !!!!
 *   RAGE 128 Software Development Manual (Technical Reference Manual P/N
 *   SDK-G04000 Rev. 0.01), ATI Technologies: June 1999.
 *
 */


#ifndef _RADEON_MACROS_H_
#define _RADEON_MACROS_H_

#include "compiler.h"

				/* Memory mapped register access macros */

#define BEGIN_ACCEL_RELOC(n, r) do {		\
	int _nqw = (n) + (r);	\
	BEGIN_RING(2*_nqw);			\
    } while (0)

#define EMIT_OFFSET(reg, value, pPix, rd, wd) do {		\
    driver_priv = exaGetPixmapDriverPrivate(pPix);		\
    OUT_RING_REG((reg), (value));				\
    OUT_RING_RELOC(driver_priv->bo, (rd), (wd));			\
    } while(0)

#define EMIT_READ_OFFSET(reg, value, pPix) EMIT_OFFSET(reg, value, pPix, (RADEON_GEM_DOMAIN_VRAM | RADEON_GEM_DOMAIN_GTT), 0)
#define EMIT_WRITE_OFFSET(reg, value, pPix) EMIT_OFFSET(reg, value, pPix, 0, RADEON_GEM_DOMAIN_VRAM)

#define OUT_TEXTURE_REG(reg, offset, bo) do {   \
    OUT_RING_REG((reg), (offset));                                   \
    OUT_RING_RELOC((bo), RADEON_GEM_DOMAIN_VRAM | RADEON_GEM_DOMAIN_GTT, 0); \
  } while(0)

#define EMIT_COLORPITCH(reg, value, pPix) do {			\
    driver_priv = exaGetPixmapDriverPrivate(pPix);			\
    OUT_RING_REG((reg), value);					\
    OUT_RING_RELOC(driver_priv->bo, 0, RADEON_GEM_DOMAIN_VRAM);		\
}while(0)

#endif
