dnl Process this file with autoconf to produce a configure script.

# get rid of that stupid cache mechanism
rm -f config.cache

EFL_VERSION([0], [0], [1], [dev])
AC_INIT([ephoto], [efl_version], [enlightenment-devel@lists.sourceforge.net])

AC_PREREQ([2.60])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS(src/bin/config.h)

AM_INIT_AUTOMAKE(1.6 dist-xz)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AM_PROG_CC_C_O
AC_C___ATTRIBUTE__

m4_ifdef([AM_GNU_GETTEXT_VERSION],[
AM_GNU_GETTEXT_VERSION([0.18])
])

m4_ifdef([AM_GNU_GETTEXT], [
AM_GNU_GETTEXT([external], [need-ngettext])
po_makefile_in=po/Makefile.in
have_po="yes"
],[
have_po="no"
])
AC_SUBST(LTLLIBINTL)
if test "x$LIBINTL" = "x"; then
  LIBINTL="$INTLLIBS"
fi

if test "x${POSUB}" = "x" ; then
  have_po="no"
fi

AM_CONDITIONAL([HAVE_PO], [test "x${have_po}" = "xyes"])

PKG_CHECK_MODULES([EFL], [
 eina >= 1.7.0
 evas >= 1.7.0
 ecore >= 1.7.0
 edje >= 1.7.0
 eo >= 1.7.0
 eio >= 1.7.0
 elementary >= 1.7.0
 efreet-mime >= 1.7.0
])

AC_ARG_WITH([tests],
   [AC_HELP_STRING([--with-tests=none|regular|coverage],
                   [choose testing method: regular, coverage, or none.
                    @<:@default=none:>@])],
   [build_tests=${withval}],
   [build_tests=auto])

EFL_TESTS([${build_tests}])

EFL_WITH_BIN([edje], [edje-cc], [edje_cc])

AC_CONFIG_FILES([
ephoto.spec
])

AC_OUTPUT([
Makefile
data/Makefile
data/desktop/Makefile
src/Makefile
src/bin/Makefile
$po_makefile_in
])


###########################################################################
## Info

echo
echo
echo
echo "------------------------------------------------------------------------"
echo "$PACKAGE_NAME $PACKAGE_VERSION"
echo "------------------------------------------------------------------------"
echo 
echo "Compilation..............: make (or gmake)"
echo "  CPPFLAGS...............: $CPPFLAGS"
echo "  CFLAGS.................: $CFLAGS"
echo "  LDFLAGS................: $LDFLAGS"
echo
echo "Building tests...........: ${have_tests}"
echo
echo "Installation:............: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix.................: $prefix"
echo
