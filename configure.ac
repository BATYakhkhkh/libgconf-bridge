AC_PREREQ(2.53)
AC_INIT(libgconf-bridge, 0.2, http://www.openedhand.com/)
AM_INIT_AUTOMAKE()
AC_CONFIG_SRCDIR(libgconf-bridge/gconf-bridge.c)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_LIBTOOL

PKG_CHECK_MODULES(LIBGCONF_BRIDGE,
                  glib-2.0
                  gtk+-2.0
                  gconf-2.0)

old_cflags=$CFLAGS
CFLAGS=$LIBGCONF_BRIDGE_CFLAGS
AC_CHECK_DECLS(gconf_value_compare, [], [], [#include <gconf/gconf.h>])
CFLAGS=$old_cflags

AC_MSG_CHECKING(whether we are using CORBA GConf)
if `pkg-config --libs gconf-2.0 | grep -q ORBit`; then
        AC_MSG_RESULT(yes)

        AC_DEFINE(HAVE_CORBA_GCONF, 1, [Defined if we are using CORBA GConf])
else
        AC_MSG_RESULT(no)
fi

PKG_CHECK_MODULES(VALA, [vala-1.0 >= 0.3.1], have_vala=yes, have_vala=no)

AM_CONDITIONAL(WITH_VALA, [test "x$have_vala" = "xyes"])

if test "x$have_vala" = "xyes"; then
    VAPIDIR=`pkg-config --variable=vapidir vala-1.0`
    AC_SUBST(VAPIDIR)
fi

GETTEXT_PACKAGE=libgconf-bridge
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], 
  [Define the gettext package to be used])
  
ALL_LINGUAS=""
AM_GLIB_GNU_GETTEXT

GTK_DOC_CHECK([1.0])

AC_OUTPUT([
Makefile
libgconf-bridge.pc
libgconf-bridge/Makefile
tests/Makefile
po/Makefile.in
po/Makefile
doc/Makefile
])
