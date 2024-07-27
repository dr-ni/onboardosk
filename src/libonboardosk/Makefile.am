
include $(top_srcdir)/Makefile-common.mk

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

local_includes = \
	-I$(top_srcdir)/src/liblm \
	-I$(top_srcdir)/src/libcommon \
	$(NULL)
local_libs= \
	$(top_builddir)/src/liblm/liblm.la \
	$(top_builddir)/src/libcommon/libcommon.la \
	$(NULL)

AM_CPPFLAGS += \
    $(local_includes) \
	$(LIBONBOARDOSK_CFLAGS) \
	-DONBOARDOSK_LOCALEDIR=\"${ONBOARDOSK_LOCALEDIR}\" \
	$(NULL)

AM_CFLAGS += \
	$(AM_CFLAGS_DEFAULT) \
	$(NULL)
	
AM_CXXFLAGS += \
	$(AM_CXXFLAGS_DEFAULT) \
	-Winvalid-pch \
	$(NULL)

# Attempt at using precompiled headers: quite unsuccessful, no difference
# in compile time at all.
#	-include stdheaders.hh 
#	-fpch-preprocess 
#	-H 

# BUILT_SOURCES = stdheaders.hh.gch
# CLEANFILES = stdheaders.hh.gch
# %.hh.gch: %.hh
# 	$(CXXCOMPILE) -fPIC -DPIC -c $< 
        
#wayland/virtkey_wayland.$(OBJEXT) : CXXFLAGS += -Wno-unused-parameter

source_h = \
	atspistatetracker.h \
	autohide.h \
	autoshow.h \
	audioplayer.h \
	buttoncontroller.h \
	callonce.h \
	characterdata.h \
	canonicalequivalents.h \
	clickgenerator.h \
	colorscheme.h \
	commandlineoptions.h \
	configdecls.h \
	configobject.h \
	configuration.h \
	drawingcontext.h \
	dwellprogresscontroller.h \
	emojidata.h \
	exception.h \
	fadetimer.h \
	globalkeylistener.h \
	handle.h \
	hardwaresensortracker.h \
	inputeventreceiver.h \
	inputeventsource.h \
	inputsequence.h \
	inputsequencedecls.h \
	inputsequencetarget.h \
	keyboard.h \
	keyboardanimator.h \
	keyboarddecls.h \
	keyboardkeylogic.h \
	keyboardpopup.h \
	keyboardpopup_fwd.h \
	keyboardscanner.h \
	keyboardview.h \
	keydefinitions.h \
	keygeometry.h \
	keypath.h \
	keysynth.h \
	languagedb.h \
	layoutcharacterpalette.h \
	layoutcontext.h \
	layoutdecls.h \
	layoutdrawingitem.h \
	layoutitem.h \
	layoutkey.h \
	layoutkeyviewcairo.h \
	layoutloadersvg.h \
	layoutroot.h \
	layoutscrolledpanel.h \
	layoutview.h \
	layoutviewkeyboard.h \
	layoutwordlist.h \
	learnstrategy.h \
	manipulatable.h \
	monitor.h \
	mousetweaks.h \
	onboardosk.h \
	onboardoskevent.h \
	onboardoskimpl.h \
	onboardoskglobals.h \
	onboardoskview.h \
	partialurlparser.h \
	punctuator.h \
	signalling.h \
	spellchecker.h \
	stickybehavior.h \
	textchanger.h \
	textchanges.h \
	textchangesdecls.h \
	textcontext.h \
	textdomain.h \
	textrendererpangocairo.h \
	theme.h \
	timer.h \
	tools/border.h \
	tools/border_decl.h \
	tools/border_fwd.h \
	tools/cairocontext.h \
	tools/color.h \
	tools/drawing_helpers.h \
	tools/enum_helpers.h \
	tools/image.h \
	tools/imagesvg.h \
	tools/iostream_helpers.h \
	tools/keydecls.h \
	tools/logger.h \
	tools/process_helpers.h \
	tools/point.h \
	tools/point_decl.h \
	tools/point_fwd.h \
	tools/rect.h \
	tools/rect_decl.h \
	tools/rect_fwd.h \
	tools/stacktrace.h \
	tools/textdecls.h \
	tools/thread_helpers.h \
	tools/time_helpers.h \
	tools/version.h \
	tools/xdgdirs.h \
	toplevelview.h \
	touchhandles.h \
	translation.h \
	treeitem.h \
	udevtracker.h \
	uielement.h \
	uinput.h \
	viewmanipulator.h \
	virtkey.h \
	wordsuggestions.h \
	wpengine.h \
	xmlparser.h \
	x11/inputeventsourcexinput.h \
	$(NULL)

source_c = \
	atspistatetracker.cpp \
	audiobackendcanberra.cpp \
	audioplayer.cpp \
	autohide.cpp \
	autoshow.cpp \
	buttoncontroller.cpp \
	callonce.cpp \
	characterdata.cpp \
	canonicalequivalents.cpp \
	clickgenerator.cpp \
	colorscheme.cpp \
	commandlineoptions.cpp \
	configobject.cpp \
	configuration.cpp \
	drawingcontext.cpp \
	dwellprogresscontroller.cpp \
	exception.cpp \
	fadetimer.cpp \
	globalkeylistener.cpp \
	handle.cpp \
	hardwaresensortracker.cpp \
	inputsequence.cpp \
	inputeventreceiver.cpp \
	inputeventsource.cpp \
	keyboard.cpp \
	keyboardanimator.cpp \
	keyboarddecls.cpp \
	keyboardkeylogic.cpp \
	keyboardscanner.cpp \
	keyboardpopup.cpp \
	keyboardview.cpp \
	keydefinitions.cpp \
	keygeometry.cpp \
	keypath.cpp \
	keysynth.cpp \
	languagedb.cpp \
	layoutcharacterpalette.cpp \
	layoutcontext.cpp \
	layoutdecls.cpp \
	layoutdrawingitem.cpp \
	layoutitem.cpp \
	layoutkey.cpp \
	layoutkeyviewcairo.cpp \
	layoutloadersvg.cpp \
	layoutroot.cpp \
	layoutscrolledpanel.cpp \
	layoutview.cpp \
	layoutviewkeyboard.cpp \
	layoutwordlist.cpp \
	learnstrategy.cpp \
	monitor.cpp \
	mousetweaks.cpp \
	onboardosk.cpp \
	onboardoskimpl.cpp \
	onboardoskglobals.cpp \
	partialurlparser.cpp \
	punctuator.cpp \
	signalling.cpp \
	spellchecker.cpp \
	stickybehavior.cpp \
	textchanger.cpp \
	textchanges.cpp \
	textcontext.cpp \
	textdomain.cpp \
	textrendererpangocairo.cpp \
	theme.cpp \
	timer.cpp \
	tools/cairocontext.cpp \
	tools/drawing_helpers.cpp \
	tools/image.cpp \
	tools/imagesvg.cpp \
	tools/logger.cpp \
	tools/point.cpp \
	tools/process_helpers.cpp \
	tools/rect.cpp \
	tools/stacktrace.cpp \
	tools/thread_helpers.cpp \
	tools/time_helpers.cpp \
	tools/version.cpp \
	tools/xdgdirs.cpp \
	toplevelview.cpp \
	touchhandles.cpp \
	translation.cpp \
	treeitem.cpp \
	udevtracker.cpp \
	uinput.cpp \
	viewmanipulator.cpp \
	virtkey.cpp \
	wayland/virtkey_wayland.cpp \
	wordsuggestions.cpp \
	wpengine.cpp \
	wperrorrecovery.cpp \
	x11/inputeventsourcexinput.cpp \
	x11/virtkey_x11.cpp \
	xmlparserxml2.cpp \
	$(NULL)
	
lib_LTLIBRARIES = libonboardosk.la
libonboardosk_la_DEPENDENCIES = $(local_libs)
libonboardosk_la_SOURCES = $(source_c) $(source_h)
libonboardosk_la_LIBADD =  $(LIBONBOARDOSK_LIBS) $(local_libs) -lstdc++fs

SUBDIRS = tests

