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

SUBDIRS = ch7017 ch7xxx ivch sil164 tfp410

AM_CFLAGS = @CWARNFLAGS@ @XORG_CFLAGS@ @DRM_CFLAGS@ @DRI_CFLAGS@ \
	@PCIACCESS_CFLAGS@ -DI830_XV -I$(top_srcdir)/src

noinst_LTLIBRARIES = liblegacy-ums.la

UMS_DRI_SRCS = \
         ums_dri.c \
         ums_dri.h

UMS_XVMC_SRCS = \
         ums_hwmc.c \
         ums_hwmc.h \
         ums_i915_hwmc.c \
         ums_i965_hwmc.c \
         ums_i965_hwmc.h

liblegacy_ums_la_SOURCES = \
         brw_defines.h \
         brw_structs.h \
         ums.h \
         ums_common.h \
	 ums_i2c_vid.h \
         ums_batchbuffer.h \
         ums_ring.h \
	 ums_crt.c \
         ums_cursor.c \
         ums_batchbuffer.c \
         ums_driver.c \
         ums_accel.c \
         ums_bios.c \
         ums_bios.h \
	 ums_debug.c \
	 ums_debug.h \
	 ums_display.c \
	 ums_display.h \
	 ums_exa.c \
	 ums_hdmi.c \
         ums_i2c.c \
         ums_io.c \
	 ums_lvds.c \
         ums_memory.c \
         ums_modes.c \
         ums_quirks.c \
	 ums_dvo.c \
	 ums_sdvo.c \
	 ums_sdvo.h \
	 ums_sdvo_regs.h \
	 ums_tv.c \
         ums_video.h \
         ums_video.c \
	 ums_xaa.c \
	 ums_i810_reg.h \
         ums_i830_common.h \
	 ums_i830_reg.h \
	 ums_i830_3d.c \
	 ums_i830_render.c \
	 ums_i915_3d.c \
	 ums_i915_3d.h \
	 ums_i915_reg.h \
	 ums_i915_render.c \
	 ums_i915_video.c \
	 ums_i965_video.c \
	 ums_i965_render.c \
	 ums_drmmode_display.c
liblegacy_ums_la_LIBADD =

INTEL_G4A =				\
	render_programs/packed_yuv_sf.g4a		\
	render_programs/packed_yuv_wm.g4a		\
	render_programs/exa_sf.g4a 			\
	render_programs/exa_sf_mask.g4a 		\
	render_programs/exa_wm_src_affine.g4a 		\
	render_programs/exa_wm_src_projective.g4a 	\
	render_programs/exa_wm_src_sample_argb.g4a 	\
	render_programs/exa_wm_src_sample_a.g4a 	\
	render_programs/exa_wm_src_sample_planar.g4a 	\
	render_programs/exa_wm_mask_affine.g4a 		\
	render_programs/exa_wm_mask_projective.g4a 	\
	render_programs/exa_wm_mask_sample_argb.g4a 	\
	render_programs/exa_wm_mask_sample_a.g4a 	\
	render_programs/exa_wm_noca.g4a			\
	render_programs/exa_wm_ca.g4a			\
	render_programs/exa_wm_ca_srcalpha.g4a		\
	render_programs/exa_wm_write.g4a 		\
	render_programs/exa_wm_yuv_rgb.g4a		\
	render_programs/exa_wm_xy.g4a

INTEL_G4I =				\
	render_programs/exa_wm.g4i			\
	render_programs/exa_wm_affine.g4i		\
	render_programs/exa_wm_projective.g4i

INTEL_G4B = 				\
	render_programs/packed_yuv_sf.g4b		\
	render_programs/packed_yuv_wm.g4b		\
	render_programs/exa_sf.g4b 			\
	render_programs/exa_sf_mask.g4b 		\
	render_programs/exa_wm_src_affine.g4b 		\
	render_programs/exa_wm_src_projective.g4b 	\
	render_programs/exa_wm_src_sample_argb.g4b 	\
	render_programs/exa_wm_src_sample_a.g4b 	\
	render_programs/exa_wm_src_sample_planar.g4b 	\
	render_programs/exa_wm_mask_affine.g4b 		\
	render_programs/exa_wm_mask_projective.g4b 	\
	render_programs/exa_wm_mask_sample_argb.g4b 	\
	render_programs/exa_wm_mask_sample_a.g4b 	\
	render_programs/exa_wm_noca.g4b			\
	render_programs/exa_wm_ca.g4b			\
	render_programs/exa_wm_ca_srcalpha.g4b		\
	render_programs/exa_wm_write.g4b 		\
	render_programs/exa_wm_yuv_rgb.g4b		\
	render_programs/exa_wm_xy.g4b

EXTRA_DIST = 		\
	$(XMODE_SRCS)	\
	$(INTEL_G4A)	\
	$(INTEL_G4I)	\
	$(INTEL_G4B)	\
	$(UMS_DRI_SRCS) \
	$(UMS_XVMC_SRCS)

if HAVE_GEN4ASM

SUFFIXES = .g4a .g4b
.g4a.g4b:
	m4 -I$(srcdir) -s $< > $*.g4m && intel-gen4asm -o $@ $*.g4m && rm $*.g4m

$(INTEL_G4B): $(INTEL_G4I)

BUILT_SOURCES= $(INTEL_G4B)

clean-local:
	-rm -f $(INTEL_G4B)
endif

if DRI
liblegacy_ums_la_SOURCES += $(UMS_DRI_SRCS)
liblegacy_ums_la_LIBADD += $(DRI_LIBS)
endif
