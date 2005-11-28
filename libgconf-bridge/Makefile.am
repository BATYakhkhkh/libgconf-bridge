AM_CPPFLAGS=-DPKGDATADIR=\"$(pkgdatadir)\" -DGETTEXT_PACKAGE=\"libgconf-bridge\"
AM_CFLAGS = -Wall -g $(LIBGCONF_BRIDGE_CFLAGS) \
	    -I$(top_srcdir) -I$(top_builddir) -Werror

libgconf_bridgeincdir = $(includedir)/libgconf-bridge

lib_LTLIBRARIES = libgconf-bridge.la

libgconf_bridgeinc_HEADERS = gconf-bridge.h

libgconf_bridge_la_SOURCES = gconf-bridge.c

libgconf_bridge_la_LIBADD = $(LIBGCONF_BRIDGE_LIBS)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
