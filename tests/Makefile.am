AM_CPPFLAGS=-DPKGDATADIR=\"$(pkgdatadir)\" -DGETTEXT_PACKAGE=\"libgconf-bridge\"
AM_CFLAGS = -Wall -g $(LIBGCONF_BRIDGE_CFLAGS) \
	    -I$(top_srcdir) -I$(top_builddir) -Werror

noinst_PROGRAMS = test

test_SOURCES = test.c
test_LDADD   = $(top_builddir)/libgconf-bridge/libgconf-bridge.la

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
