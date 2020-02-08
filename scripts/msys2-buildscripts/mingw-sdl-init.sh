#!/bin/sh

# Due to several issues with the MSYS2 SDL builds, they are not suitable
# for redistribution. Download and extract the MinGW development files for SDL.

cd /mzx-build-workingdir

VERSION=2.0.10
FILENAME=SDL2-devel-$VERSION-mingw.tar.gz

rm -f $FILENAME
wget https://www.libsdl.org/release/$FILENAME
tar -xzf $FILENAME

rm -rf sdl2-mingw
mv SDL2-$VERSION sdl2-mingw
