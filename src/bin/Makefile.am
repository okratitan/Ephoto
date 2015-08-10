MAINTAINERCLEANFILES = *.o config.h.in config.h Makefile.in Makefile stamp-h1

bin_PROGRAMS = ephoto

ephoto_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/bin \
	-DPACKAGE_DATA_DIR=\"$(datadir)/$(PACKAGE)\" \
	@EFL_CFLAGS@
ephoto_LDADD =	@EFL_LIBS@
if HAVE_PO
ephoto_CPPFLAGS += -DLOCALEDIR=\"$(localedir)\"
ephoto_LDADD += @LIBINTL@
endif

ephoto_SOURCES = \
	ephoto.c \
	ephoto_main.c \
	ephoto_config.c \
        ephoto_thumb_browser.c \
        ephoto_single_browser.c \
        ephoto_slideshow.c \
        ephoto_cropper.c

noinst_HEADERS = ephoto.h gettext.h
EXTRA_DIST = ephoto.h gettext.h
