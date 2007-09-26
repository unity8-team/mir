if DRI
lib_LTLIBRARIES=libI810XvMC.la libI915XvMC.la

libI810XvMC_la_SOURCES = I810XvMC.c \
			 I810XvMC.h

libI810XvMC_la_CFLAGS = @WARN_CFLAGS@ @XORG_CFLAGS@ @DRI_CFLAGS@ \
	-I$(top_srcdir)/src -DTRUE=1 -DFALSE=0
libI810XvMC_la_LDFLAGS = -version-number 1:0:0
libI810XvMC_la_LIBADD = @DRI_LIBS@ 

libI915XvMC_la_SOURCES = I915XvMC.c \
                         I915XvMC.h \
			 intel_xvmc.h \
                         intel_batchbuffer.c \
                         intel_batchbuffer.h \
                         xf86dri.c \
                         xf86dri.h \
                         xf86dristr.h \
                         driDrawable.c \
                         driDrawable.h
libI915XvMC_la_CFLAGS = @XORG_CFLAGS@ @DRI_CFLAGS@ -I$(top_srcdir)/src -DTRUE=1 -DFALSE=0
libI915XvMC_la_LDFLAGS = -version-number 1:0:0
libI915XvMC_la_LIBADD = @DRI_LIBS@
endif
