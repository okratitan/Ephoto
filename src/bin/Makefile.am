AUTOMAKE_OPTIONS = subdir-objects
MAINTAINERCLEANFILES = *.gcda *.gcno *.o config.h.in config.h Makefile.in Makefile stamp-h1
DISTCLEANFILES = *.gcda *.gcno

bin_PROGRAMS = ephoto

ephoto_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/bin \
	-DPACKAGE_DATA_DIR=\"$(datadir)/$(PACKAGE)\" \
        -Wall \
        -Wextra \
        -Wshadow \
	@EFL_CFLAGS@
ephoto_LDADD =	@EFL_LIBS@
if HAVE_PO
ephoto_CPPFLAGS += -DLOCALEDIR=\"$(localedir)\"
ephoto_LDADD += @LTLIBINTL@
endif

ephoto_SOURCES = \
	ephoto.c \
	ephoto_main.c \
	ephoto_config.c \
        ephoto_thumb_browser.c \
        ephoto_single_browser.c \
        ephoto_slideshow.c \
        ephoto_cropper.c \
        ephoto_bcg.c \
        ephoto_hsv.c \
        ephoto_filters.c \
        ephoto_color.c

noinst_HEADERS = ephoto.h
EXTRA_DIST = ephoto.h
