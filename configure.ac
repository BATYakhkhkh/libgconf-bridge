AC_PREREQ(2.53)
AC_INIT(libgconf-bridge, 0.1, http://www.openedhand.com/)
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
