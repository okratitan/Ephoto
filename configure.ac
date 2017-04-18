dnl Process this file with autoconf to produce a configure script.

# get rid of that stupid cache mechanism
rm -f config.cache

AC_INIT([ephoto], [1.0], [enlightenment-devel@lists.sourceforge.net])

AC_PREREQ([2.60])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(src/bin/config.h)

AC_CANONICAL_HOST

have_win32="no"
case "$host_os" in
   mingw*)
      have_win32="yes"
   ;;
esac

AM_CONDITIONAL([HAVE_WIN32], [test "x${have_win32}" = "xyes"])

AM_INIT_AUTOMAKE(1.6 dist-xz)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AM_PROG_CC_C_O
AC_C___ATTRIBUTE__

AC_CHECK_HEADERS([dirent.h limits.h math.h arpa/inet.h netinet/in.h])

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
AC_SUBST([LTLIBINTL])
if test "x$LIBINTL" = "x"; then
  LIBINTL="$INTLLIBS"
fi

if test "x${POSUB}" = "x" ; then
  have_po="no"
fi

AM_CONDITIONAL([HAVE_PO], [test "x${have_po}" = "xyes"])

requirements="eina >= 1.18.0 evas >= 1.18.0 ecore >= 1.18.0 ecore-file >=
1.18.0 ecore-ipc >= 1.18.0 edje >= 1.18.0 eo >= 1.18.0 eio >= 1.18.0
elementary >= 1.18.0 efreet-mime >= 1.18.0"

if test "x${have_win32}" = "xyes" ; then
 requirements="${requirements} evil"
fi

PKG_CHECK_MODULES([EFL], [${requirements}])

AC_CHECK_FUNCS([nice])

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
data/images/Makefile
data/themes/Makefile
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
echo "Host.....................: ${host_os}"
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
