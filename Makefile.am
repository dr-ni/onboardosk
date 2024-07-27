NULL=

AM_MAKEFLAGS = --silent
ACLOCAL_AMFLAGS = -I build/autotools
DISTCHECK_CONFIGURE_FLAGS = --enable-introspection

SUBDIRS=src layouts themes models emojione data po


#-------------------------------------------------------------------------
# translation support (intltool)
#-------------------------------------------------------------------------

INTLTOOL_FILES = intltool-extract.in \
                 intltool-merge.in \
                 intltool-update.in
DISTCLEANFILES = ... intltool-extract \
                 intltool-merge \
                 intltool-update \
                 po/.intltool-merge-cache     
# enable this once there are desktop files for the C++ Onboard.
# datadir is usually /usr/share
#desktopdir = $(datadir)/applications
#desktop_in_files = onboard.desktop.in
#desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
#@INTLTOOL_DESKTOP_RULE@                       

EXTRA_DIST = build/config.rpath build/autotools/ChangeLog INTLTOOL_FILES

#-------------------------------------------------------------------------
# optional unitTest target, TODO: check if still needed
#-------------------------------------------------------------------------

.PHONY: unitTest
unitTest:
	cd src/libonboard/tests && $(MAKE) $(AM_MAKEFLAGS) onlyunittests



