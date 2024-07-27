#!/bin/sh -e
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.
#autoreconf --force --install --verbose --warnings=all "$srcdir"
autoreconf --force --install --verbose "$srcdir"

# intltoolize must be run from the project root, i.e. 
# where the po directory can be found.
# is that even necessary? intltoolize --copy --force is already run by
# configure.ac's IT_PROG_INTLTOOL.
# prevents error: config.status: error: po/Makefile.in.in was not created by intltoolize.

CWD=`pwd`
echo "Entering source directory ${srcdir}"
cd ${srcdir}
echo "Running intltoolize --copy --force --automake"
intltoolize --copy --force --automake
echo "Entering directory ${CWD}"
cd ${CWD}

test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
