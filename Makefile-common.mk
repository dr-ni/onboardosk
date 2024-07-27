# Common include for all subprojects' Makefile.am.

NULL =
AM_CPPFLAGS =
AM_CFLAGS =
AM_CXXFLAGS =

# Global compiler flags
AM_CFLAGS_DEFAULT = \
	-Wall \
	-Wextra \
	-Werror=return-type \
	-Werror=uninitialized \
	-Werror=maybe-uninitialized \
	-Werror=shadow \
	-Wlogical-op \
	-Werror=declaration-after-statement \
	-pedantic-errors \
	$(NULL)

AM_CXXFLAGS_NOPEDANTIC = \
	-std=c++1z \
	-Wall \
	-Wextra \
	-Werror=return-type \
	-Werror=uninitialized \
	-Werror=maybe-uninitialized \
	-Werror=shadow \
	-Wlogical-op \
	$(NULL)
	
AM_CXXFLAGS_DEFAULT = \
	$(AM_CXXFLAGS_NOPEDANTIC)
	-pedantic-errors \
	$(NULL)
#all:
#	echo $(top_srcdir)
#	echo $(gtest_source_dir)
#	echo $(gtest_includes)


