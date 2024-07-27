
include $(top_srcdir)/Makefile-common.mk

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

local_includes = \
	-I$(top_srcdir)/src/libcommon \
	$(NULL)
local_libs= \
	#$(top_builddir)/src/libcommon/libcommon.la \
	$(NULL)

AM_CPPFLAGS += \
    $(local_includes) \
	$(LIBLM_CFLAGS) \
	$(NULL)

AM_CXXFLAGS += \
	$(AM_CXXFLAGS_DEFAULT) \
	$(NULL)
	

source_h = \
    accent_transform.h \
    lm.h \
    lm_dynamic_cached.h \
    lm_dynamic.h \
    lm_dynamic_impl.h \
    lm_dynamic_kn.h \
    lm_heapalloc.h \
    lm_merged.h \
    lm_tokenize.h \
    lm_unigram.h \
    lm_wrapper.h \
    pool_allocator.h \
	$(NULL)

source_c = \
    lm.cpp \
    lm_dynamic.cpp \
    lm_heapalloc.cpp \
    lm_merged.cpp \
    lm_unigram.cpp \
    lm_wrapper.cpp \
    lm_tokenize.cpp \
    pool_allocator.cpp \
	$(NULL)
	
noinst_LTLIBRARIES = liblm.la
liblm_la_DEPENDENCIES = $(local_libs)
liblm_la_SOURCES = $(source_c) $(source_h)
liblm_la_LIBADD =  $(LIBLM_LIBS) $(local_libs)

SUBDIRS = tests

