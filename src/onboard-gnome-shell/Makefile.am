NULL =
AM_CPPFLAGS =
AM_CFLAGS =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =
DISTCLEANFILES =
lib_LTLIBRARIES =
bin_PROGRAMS =

SUBDIRS=OnboardOSK@onboardosk.org

AM_CPPFLAGS += \
		$(ONBOARDOSK_GNOME_SHELL_CFLAGS) \
		-I $(top_srcdir)/src/libonboardosk
	$(NULL)

AM_CFLAGS += \
        $(NULL)


introspection_h = \
    onboardosk-instance.h \
    onboardosk-keyboardview.h

introspection_c = \
    onboardosk-instance.c \
    onboardosk-keyboardview.c

source_h = \
	$(introspection_h) \
	$(NULL)

source_c = \
	$(introspection_c) \
	$(NULL)

LOCAL_LIBS = $(top_builddir)/src/libonboardosk/libonboardosk.la

lib_LTLIBRARIES += libonboardoskgnomeshell-1.0.la
libonboardoskgnomeshell_1_0_la_DEPENDENCIES = $(LOCAL_LIBS)
libonboardoskgnomeshell_1_0_la_LIBADD = $(LOCAL_LIBS) $(ONBOARDOSK_GNOME_SHELL_LIBS)
libonboardoskgnomeshell_1_0_la_SOURCES = $(source_c) $(source_h)

onboardincludedir = $(includedir)/onboard
onboardinclude_DATA = \
	$(source_h) \
	$(NULL)


# GObject introspection
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = OnboardOsk_1_0_gir
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS =

if HAVE_INTROSPECTION
introspection_sources = $(introspection_c) $(introspection_h)

OnboardOsk-1.0.gir: libonboardoskgnomeshell-1.0.la
#OnboardOsk_1_0_gir_INCLUDES = GObject-2.0 Clutter-1.0
OnboardOsk_1_0_gir_INCLUDES = GObject-2.0 cairo-1.0
OnboardOsk_1_0_gir_CFLAGS = $(AM_CPPFLAGS)
OnboardOsk_1_0_gir_PACKAGES =
OnboardOsk_1_0_gir_LIBS = libonboardoskgnomeshell-1.0.la $(LOCAL_LIBS)
OnboardOsk_1_0_gir_FILES = $(introspection_sources)
OnboardOsk_1_0_gir_NAMESPACE = OnboardOsk
INTROSPECTION_GIRS += OnboardOsk-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif


