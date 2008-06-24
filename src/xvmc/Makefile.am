if XVMC
lib_LTLIBRARIES=libI810XvMC.la libIntelXvMC.la

libI810XvMC_la_SOURCES = I810XvMC.c \
			 I810XvMC.h

libI810XvMC_la_CFLAGS = @WARN_CFLAGS@ @XORG_CFLAGS@ @DRI_CFLAGS@ \
	-I$(top_srcdir)/src -DTRUE=1 -DFALSE=0
libI810XvMC_la_LDFLAGS = -version-number 1:0:0
libI810XvMC_la_LIBADD = @DRI_LIBS@ 

libIntelXvMC_la_SOURCES = intel_xvmc.c \
			 intel_xvmc.h \
			 intel_xvmc_dump.c \
			 i915_structs.h \
			 i915_program.h \
			 i915_xvmc.c \
                         i915_xvmc.h \
                         i965_xvmc.c \
                         i965_xvmc.h \
                         intel_batchbuffer.c \
                         intel_batchbuffer.h \
                         xf86dri.c \
                         xf86dri.h \
                         xf86dristr.h

libIntelXvMC_la_CFLAGS = @XORG_CFLAGS@ @DRI_CFLAGS@ @XVMCLIB_CFLAGS@ -I$(top_srcdir)/src -DTRUE=1 -DFALSE=0
libIntelXvMC_la_LDFLAGS = -version-number 1:0:0
libIntelXvMC_la_LIBADD = @DRI_LIBS@

INTEL_G4A =                             \
        forward.g4a               	\
        backward.g4a               	\
        f_b.g4a                      	\
        ipicture.g4a                    \
        null.g4a              

INTEL_G4I =                             \
	motion.g4i			\
	avg.g4i				\
	correct.g4i			\
	media_read.g4i			\
	media_read_avg_x0y1.g4i		\
	media_read_avg_x1y0.g4i		\
	media_read_avg_x1y1.g4i	
	
INTEL_G4B =                             \
        forward.g4b               	\
        backward.g4b 			\
        ipicture.g4b 			\
        null.g4b 			\
        f_b.g4b     	               	

EXTRA_DIST =            \
        $(INTEL_G4A)    \
        $(INTEL_G4I)    \
        $(INTEL_G4B)    

if HAVE_GEN4ASM

SUFFIXES = .g4a .g4b
.g4a.g4b:
	m4 $*.g4a > $*.g4m && intel-gen4asm -o $@ $*.g4m && rm $*.g4m

$(INTEL_G4B): $(INTEL_G4I)

BUILT_SOURCES= $(INTEL_G4B)

clean-local:
	-rm -f $(INTEL_G4B)
endif
endif
