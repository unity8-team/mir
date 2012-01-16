stress_TESTS = \
	basic-fillrect \
	basic-copyarea \
	basic-copyarea-size \
	basic-putimage \
	basic-lines \
	basic-stress \
	render-fill \
	render-trapezoid \
	render-trapezoid-image \
	render-fill-copy \
	render-composite-solid \
	render-copyarea \
	render-copyarea-size \
	render-copy-alphaless \
	mixed-stress \
	$(NULL)

check_PROGRAMS = $(stress_TESTS)

AM_CFLAGS = @CWARNFLAGS@ @X11_CFLAGS@
LDADD = libtest.la @X11_LIBS@

noinst_LTLIBRARIES = libtest.la
libtest_la_SOURCES = \
	test.h \
	test_display.c \
	test_image.c \
	test_log.c \
	test_render.c \
	$(NULL)

EXTRA_DIST = README
