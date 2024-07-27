
include $(top_srcdir)/Makefile-common.mk

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS += \
	$(LIBONBOARDOSK_CFLAGS) \
	$(NULL)

AM_CXXFLAGS += \
	$(AM_CXXFLAGS_DEFAULT) \
	$(NULL)
	
source_h = \
	tools/container_helpers.h \
	tools/file_helpers.h \
	tools/glib_helpers.h \
    tools/iostream_helpers.h \
	tools/path_helpers.h \
    tools/string_helpers.h \
    tools/textdecls.h \
    tools/ustringmain.h \
    tools/ustringregex.h \
	$(NULL)

source_c = \
	tools/file_helpers.cpp \
	tools/path_helpers.cpp \
    tools/string_helpers.cpp \
    tools/ustringmain.cpp \
    tools/ustringregex.cpp \
	$(NULL)
	
# noinst = static library
noinst_LTLIBRARIES = libcommon.la
libcommon_la_SOURCES = $(source_c) $(source_h)

#SUBDIRS = tests

