m4_define([onboardosk_major_version], [0])
m4_define([onboardosk_minor_version], [0])
m4_define([onboardosk_micro_version], [1])

m4_define([onboardosk_version], [onboardosk_major_version.onboardosk_minor_version.onboardosk_micro_version])

AC_PREREQ([2.69])

AC_INIT([OnboardOSK],[onboardosk_version],[SendBugsHere@example.com])

# don't install static libraries by default
#AC_ENABLE_STATIC(no)

# For using custom m4 macros.
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/autotools])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_HEADER([project_config.h])

# Checks for programs
AC_PROG_CC
AC_PROG_CXX([gcc-7 gcc])
AC_PROG_INSTALL
AX_PTHREAD([have_pthread=yes], [heve_pthread=no])
# Checks for libraries
PKG_CHECK_MODULES([ATSPI2], [atspi-2])
PKG_CHECK_MODULES([CANBERRA], [libcanberra])
PKG_CHECK_MODULES([CLUTTER], [clutter-1.0 >= 1.24])
PKG_CHECK_MODULES([GIO], [gio-2.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([GOBJECT], [gobject-2.0 >= 2.44])
PKG_CHECK_MODULES([GTK3], [gtk+-3.0 >= 3.22])
PKG_CHECK_MODULES([HUNSPELL], [hunspell])
PKG_CHECK_MODULES([ICU], [icu-uc >= 57.1 icu-i18n])
PKG_CHECK_MODULES([PANGOCAIRO], [pangocairo])
PKG_CHECK_MODULES([RSVG], [librsvg-2.0])
PKG_CHECK_MODULES([UDEV], [libudev])
PKG_CHECK_MODULES([WAYLAND], [wayland-client])
PKG_CHECK_MODULES([X11], [x11])
PKG_CHECK_MODULES([XKBCOMMON], [xkbcommon])
PKG_CHECK_MODULES([XKBFILE], [xkbfile])
PKG_CHECK_MODULES([XTST], [xtst])
PKG_CHECK_MODULES([XML2], [libxml-2.0 >= 2.9])

# Checks for typedefs, structures, and compiler characteristics
AC_PROG_CC_STDC

# translation support using intltool
IT_PROG_INTLTOOL([0.35.0])

GETTEXT_PACKAGE=onboardosk
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"],
                   [onboardosk])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])

ONBOARDOSK_LOCALEDIR=[${datadir}/locale]
AC_SUBST(ONBOARDOSK_LOCALEDIR)

# @GSETTINGS_RULES@ for the gsettings schema
GLIB_GSETTINGS

# subdir-objects for googletest
# serial-tests to enable verbose make check output
AM_INIT_AUTOMAKE([-Wall -Werror foreign -Wno-portability subdir-objects serial-tests])
AM_PATH_GLIB_2_0
AM_PROG_AR
LT_INIT

# Silence compiler output.
# For more silence set AM_MAKEFLAGS += --silent in Makefile.am.
#AM_SILENT_RULES([yes])

# Versioning
ONBOARDOSK_MAJOR_VERSION=onboardosk_major_version
ONBOARDOSK_MINOR_VERSION=onboardosk_minor_version
ONBOARDOSK_MICRO_VERSION=onboardosk_micro_version
ONBOARDOSK_VERSION=onboardosk_version
AC_SUBST(ONBOARDOSK_MAJOR_VERSION)
AC_SUBST(ONBOARDOSK_MINOR_VERSION)
AC_SUBST(ONBOARDOSK_MICRO_VERSION)
AC_SUBST(ONBOARDOSK_VERSION)

# Subproject flags
ONBOARDOSK_GNOME_SHELL_CFLAGS="$GOBJECT_CFLAGS \
                            $CLUTTER_CFLAGS"
ONBOARDOSK_GNOME_SHELL_LIBS="$GOBJECT_LIBS \
                          $CLUTTER_LIBS"

ONBOARDOSK_GTK_CFLAGS="$GTK3_CFLAGS"
ONBOARDOSK_GTK_LIBS="$GTK3_LIBS"

LIBONBOARDOSK_CFLAGS="$ATSPI2_CFLAGS \
                      $CANBERRA_CFLAGS \
                      $GIO_CFLAGS \
                      $GTK3_CFLAGS \
                      $HUNSPELL_CFLAGS \
                      $ICU_CFLAGS \
                      $PANGOCAIRO_CFLAGS \
                      $RSVG_CFLAGS \
                      $UDEV_CFLAGS \
                      $WAYLAND_CFLAGS \
                      $X11_CFLAGS \
                      $XKBFILE_CFLAGS \
                      $XKBCOMMON_CFLAGS \
                      $XML2_CFLAGS \
                      $XTST_CFLAGS"
LIBONBOARDOSK_LIBS="$ATSPI2_LIBS \
                    $CANBERRA_LIBS \
                    $GIO_LIBS \
                    $GTK3_LIBS \
                    $HUNSPELL_LIBS \
                    $ICU_LIBS \
                    $PANGOCAIRO_LIBS \
                    $RSVG_LIBS \
                    $UDEV_LIBS \
                    $WAYLAND_LIBS \
                    $X11_LIBS \
                    $XKBFILE_LIBS \
                    $XKBCOMMON_LIBS \
                    $XML2_LIBS \
                    $XTST_LIBS"

LIBCOMMON_CFLAGS="$ICU_CFLAGS"
LIBCOMMON_LIBS="$ICU_LIBS"

LIBLM_CFLAGS=""
LIBLM_LIBS="$GLIB_LIBS"


AC_SUBST(ONBOARDOSK_GNOME_SHELL_CFLAGS)
AC_SUBST(ONBOARDOSK_GNOME_SHELL_LIBS)
AC_SUBST(ONBOARDOSK_GTK_CFLAGS)
AC_SUBST(ONBOARDOSK_GTK_LIBS)
AC_SUBST(LIBONBOARDOSK_CFLAGS)
AC_SUBST(LIBONBOARDOSK_LIBS)
AC_SUBST(LIBLM_CFLAGS)
AC_SUBST(LIBLM_LIBS)
AC_SUBST(LIBCOMMON_CFLAGS)
AC_SUBST(LIBCOMMON_LIBS)

# Introspection
GOBJECT_INTROSPECTION_CHECK([1.38.0])

# Unit testing with googletest
GTEST_VERSION="1.7.0"
if test "x$have_pthread" = "xyes"; then
  GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=1 -I\$(top_srcdir)/lib/gmock"
  GTEST_CXXFLAGS="$PTHREAD_CFLAGS"
  GTEST_LDFLAGS="-L\$(top_builddir)/lib/gmock"
  GTEST_LIBS="$PTHREAD_LIBS"
else
  GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=0 -I\$(top_srcdir)/lib/gmock"
  GTEST_CXXFLAGS=
  GTEST_LDFLAGS="-L\$(top_builddir)/lib/gmock"
  GTEST_LIBS=
fi
AC_SUBST([GTEST_VERSION])
AC_SUBST([GTEST_CPPFLAGS])
AC_SUBST([GTEST_CXXFLAGS])
AC_SUBST([GTEST_LDFLAGS])
AC_SUBST([GTEST_LIBS])

AC_CONFIG_FILES([Makefile po/Makefile.in
                 src/Makefile
                 src/libcommon/Makefile
                 src/liblm/Makefile
                 src/liblm/tests/Makefile
                 src/libonboardosk/Makefile
                 src/libonboardosk/tests/Makefile
                 src/onboard-gnome-shell/Makefile
                 src/onboard-gnome-shell/OnboardOSK@onboardosk.org/Makefile
                 src/onboard-gtk/Makefile
                 layouts/Makefile
                 themes/Makefile
                 models/Makefile
                 emojione/Makefile
                 data/Makefile
                 data/icons/Makefile
                 data/sounds/Makefile])

AC_OUTPUT

AC_MSG_RESULT([
Configure Information:
  C Compiler        : $CC
    DEFS            :   $DEFS
    CPPFLAGS        :   $CPPFLAGS
    CFLAGS          :   $CFLAGS

  C++ Compiler      : $CXX
    DEFS            :   $DEFS
    CPPFLAGS        :   $CPPFLAGS
    CXXFLAGS        :   $CXXFLAGS

  Linker            : $LD
    LDFLAGS         :   $LDFLAGS
    LIBS            :   $LIBS
])
