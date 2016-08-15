AUTOMAKE_OPTIONS = subdir-objects
MAINTAINERCLEANFILES = *.gcda *.gcno *.o config.h.in config.h Makefile.in Makefile stamp-h1
DISTCLEANFILES = *.gcda *.gcno

bin_PROGRAMS = ephoto

ephoto_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/bin \
	-DPACKAGE_DATA_DIR=\"$(datadir)/$(PACKAGE)\" \
        -DPACKAGE_LIB_DIR=\"$(libdir)/$(PACKAGE)\" \
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
        ephoto_bcg.c \
	ephoto_color.c \
	ephoto_config.c \
        ephoto_cropper.c \
        ephoto_directory_browser.c \
        ephoto_editor.c \
        ephoto_file.c \
        ephoto_filters.c \
        ephoto_hsv.c \
        ephoto_ipc.c \
        ephoto_main.c \
        ephoto_red_eye.c \
        ephoto_scale.c \
        ephoto_single_browser.c \
        ephoto_slideshow.c \
        ephoto_thumb.c \
        ephoto_thumb_browser.c

internal_bindir = $(libdir)/$(PACKAGE)
internal_bin_PROGRAMS = ephoto_thumbnail

ephoto_thumbnail_CPPFLAGS = \
	-I$(top_srcdir) \
        -I$(top_srcdir)/src/bin \
        -DPACKAGE_DATA_DIR=\"$(datadir)/$(PACKAGE)\" \
        -DPACKAGE_LIB_DIR=\"$(libdir)/$(PACKAGE)\" \
        -Wall \
        -Wextra \
        -Wshadow \
        @EFL_CFLAGS@

ephoto_thumbnail_LDADD = @EFL_LIBS@

if HAVE_WIN32
ephoto_thumbnail_LDADD += -lws2_32
endif

if HAVE_PO
ephoto_thumbnail_LDADD += @LTLIBINTL@
endif

ephoto_thumbnail_SOURCES = \
	ephoto_thumbnailer.c \
        ephoto_ipc.c \
        ephoto_thumb.c

noinst_HEADERS = ephoto.h
EXTRA_DIST = ephoto.h
