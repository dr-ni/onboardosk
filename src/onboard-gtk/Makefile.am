include $(top_srcdir)/Makefile-common.mk

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

local_includes = \
	-I$(top_srcdir)/src/libcommon \
	-I$(top_srcdir)/src/libonboardosk \
	$(NULL)
local_libs= \
	$(top_builddir)/src/libcommon/libcommon.la \
	$(top_builddir)/src/libonboardosk/libonboardosk.la \
	$(NULL)

AM_CPPFLAGS += \
    $(local_includes) \
	$(ONBOARDOSK_GTK_CFLAGS) \
	-DONBOARDOSK_LOCALEDIR=\"${ONBOARDOSK_LOCALEDIR}\" \
	$(NULL)

AM_CXXFLAGS += \
	$(AM_CXXFLAGS_DEFAULT) \
	$(NULL)
	

source_h = \
	$(NULL)

source_c = \
	main.cpp \
	$(NULL)


bin_PROGRAMS = onboard_gtk
onboard_gtk_SOURCES = $(source_c) $(source_h)
onboard_gtk_DEPENDENCIES = $(local_libs)
onboard_gtk_LDADD = $(local_libs) $(ONBOARDOSK_GTK_LIBS) -lstdc++


