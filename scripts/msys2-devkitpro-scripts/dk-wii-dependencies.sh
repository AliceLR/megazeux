#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITPPC

export SDL_WII_REPO=https://github.com/dborth/sdl-wii.git

# This needs to be run after dk-3ds-dependencies.sh
# Wii portlibs are provided by the same repository as the 3DS.

export DEVKITPRO=$1
export DEVKITPPC=$2
export PATH="$PATH:$DEVKITPPC/bin"

# The following scripts need this on the path.
export PATH="$PATH:/mzx-build-workingdir/3ds_portlibs/utils"

cd /mzx-build-workingdir/3ds_portlibs


echo ""
echo "/************/"
echo "  Wii - zlib  "
echo "/************/"

pkg-build-wii zlib


echo ""
echo "/**************/"
echo "  Wii - libogg  "
echo "/**************/"

pkg-build-wii libogg


echo ""
echo "/*****************/"
echo "  Wii - libvorbis  "
echo "/*****************/"

pkg-build-wii libvorbis


echo ""
echo "/***************/"
echo "  Wii - SDL Wii  "
echo "/***************/"

rm -rf /mzx-build-workingdir/sdl-wii
git clone $SDL_WII_REPO /mzx-build-workingdir/sdl-wii
cd /mzx-build-workingdir/sdl-wii/SDL

make -j8
make install
