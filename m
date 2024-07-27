#!/bin/sh

set -e

[ -d ./build ] || mkdir ./build
cd build

if [ ! -f ../configure ]; then
    CFLAGS='-g -O0' CXXFLAGS='-g -O0' ../autogen.sh --prefix=/home/user/staging
    #./autogen.sh && CFLAGS='-g -O0' CXXFLAGS='-g -O0' --enable-silent-rules ./configure --prefix=/home/user/staging
    #autoreconf -fiv && make
fi

make 
make install  # install in staging area

#cd ..

