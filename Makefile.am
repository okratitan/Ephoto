MAINTAINERCLEANFILES = \
ABOUT-NLS* \
Makefile.in \
$(PACKAGE_TARNAME)-$PACKAGE_VERSION).tar.gz \
$(PACKAGE_TARNAME)-$PACKAGE_VERSION).tar.xz \
acconfig.h \
aclocal.m4 \
compile \
config.guess \
config.h.in \
config.rpath \
config.sub \
configure \
depcomp \
install-sh \
ltconfig \
ltmain.sh \
missing \
mkinstalldirs \
stamp-h.in \
stamp-h

if HAVE_PO
MAINTAINERCLEANFILES += \
m4/codeset.m4 \
m4/gettext.m4 \
m4/glibc21.m4 \
m4/iconv.m4 \
m4/intdiv0.m4 \
m4/inttypes.m4 \
m4/inttypes_h.m4 \
m4/inttypes-pri.m4 \
m4/isc-posix.m4 \
m4/lcmessage.m4 \
m4/lib-ld.m4 \
m4/lib-link.m4 \
m4/lib-prefix.m4 \
m4/libtool.m4 \
m4/lt~obsolete.m4 \
m4/ltoptions.m4 \
m4/ltsugar.m4 \
m4/ltversion.m4 \
m4/nls.m4 \
m4/po.m4 \
m4/progtest.m4 \
m4/stdint_h.m4 \
m4/uintmax_t.m4 \
m4/ulonglong.m4 \
po/boldquot.sed \
po/en@boldquot.header \
po/en@quot.header \
po/insert-header.sin \
po/Makefile.in.in \
po/Makevars.template \
po/quot.sed \
po/remove-potcdate.sin \
po/Rules-quot

# no idea why this is required, it should not be:
DISTCLEANFILES = po/stamp-po

endif

SUBDIRS = src data

if HAVE_PO
SUBDIRS += po
endif

EXTRA_DIST = README AUTHORS COPYING autogen.sh ephoto.spec

ACLOCAL_AMFLAGS = -I m4
