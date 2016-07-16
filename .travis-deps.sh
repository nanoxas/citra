#!/bin/sh

set -e
set -x

#if OS is linux or is not set
if [ "$TRAVIS_OS_NAME" = "linux" -o -z "$TRAVIS_OS_NAME" ]; then

    mkdir -p $HOME/.local
    curl -L http://www.cmake.org/files/v3.1/cmake-3.1.0-Linux-i386.tar.gz \
        | tar -xz -C $HOME/.local --strip-components=1

    if [ "$CROSS_COMPILE" = "false" ]; then
        export CC=gcc-6
        export CXX=g++-6

        (
            wget http://libsdl.org/release/SDL2-2.0.4.tar.gz -O - | tar xz
            cd SDL2-2.0.4
            ./configure --prefix=$HOME/.local
            make -j4 && make install
        )
    else
        # add the MXE repo and install needed packages. See: http://pkg.mxe.cc/
        echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" > \
            /etc/apt/sources.list.d/mxeapt.list
        # if this fails, we can find the gpg key at http://pkg.mxe.cc/repos/apt/conf/mxeapt.gpg
        # and just hardcode the key in this file
        sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB
        sudo apt-get update
        sudo apt-get install mxe-x86-64-w64-mingw32.static-qt5base \
            mxe-x86-64-w64-mingw32.static-gcc mxe-x86-64-w64-mingw32.static-sdl2
    fi
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
    brew update > /dev/null # silence the very verbose output
    brew unlink cmake
    brew install cmake31 qt5 sdl2 dylibbundler
    gem install xcpretty
fi
