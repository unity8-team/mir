#  Copyright 2005 Adam Jackson.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  on the rights to use, copy, modify, merge, publish, distribute, sub
#  license, and/or sell copies of the Software, and to permit persons to whom
#  the Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
#  ADAM JACKSON BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
#  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
#  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# this is obnoxious:
# -module lets us name the module exactly how we want
# -avoid-version prevents gratuitous .0.0.0 version numbers on the end
# _ladir passes a dummy rpath to libtool so the thing will actually link
# TODO: -nostdlib/-Bstatic/-lgcc platform magic, not installing the .a, etc.
AM_CFLAGS = @XORG_CFLAGS@
nouveau_drv_la_LTLIBRARIES = nouveau_drv.la
nouveau_drv_la_LDFLAGS = -module -avoid-version
nouveau_drv_ladir = @moduledir@/drivers

nouveau_drv_la_SOURCES = \
         nv_accel_common.c \
         nv_bios.c \
         nv_const.h \
         nv_cursor.c \
         nv_dac.c \
         nv_dma.c \
         nv_dma.h \
         nv_dri.c \
         nv_dri.h \
         nv_exa.c \
         nv_driver.c \
         nv_hw.c \
         nv_include.h \
         nouveau_reg.h \
         nv_dripriv.h \
         nv_local.h \
         nv_mem.c \
         nv_notifier.c \
         nv_proto.h \
         nvreg.h \
         nv_setup.c \
         nv_shadow.c \
         nv_type.h \
         nv_video.c \
         nv10_exa.c \
         nv30_exa.c \
         nv40_exa.c \
         nv_crtc.c \
         nv_output.c \
         nv_i2c.c \
	 nv50_cursor.c \
	 nv50_crtc.c \
	 nv50_dac.c \
	 nv50_display.c \
	 nv50_exa.c \
	 nv50_output.c \
	 nv50_sor.c \
	 nv50_type.h \
	 nv50_display.h \
	 nv50_cursor.h \
	 nv50_output.h \
	 nv_shaders.h \
	 nv_pcicompat.h

