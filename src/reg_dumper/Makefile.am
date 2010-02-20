noinst_PROGRAMS = \
	intel_hotplug

intel_hotplug_SOURCES = \
	hotplug.c \
	reg_dumper.h

intel_hotplug_LDADD = $(PCIACCESS_LIBS)

AM_CFLAGS = $(PCIACCESS_CFLAGS) $(CWARNFLAGS) \
	-I$(srcdir)/.. -DREG_DUMPER

