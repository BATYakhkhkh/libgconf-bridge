SUBDIRS = libgconf-bridge tests po doc

pcdata_DATA= libgconf-bridge.pc
pcdatadir = $(libdir)/pkgconfig

if WITH_VALA
vapidir = $(VAPIDIR)
vapi_DATA = libgconf-bridge.vapi
endif

EXTRA_DIST = $(pcdata_DATA) libgconf-bridge.vapi

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub configure depcomp install-sh ltmain.sh Makefile.in missing config.h.in

