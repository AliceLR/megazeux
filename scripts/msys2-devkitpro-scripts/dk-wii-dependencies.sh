#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITPPC

[ -z "$1" ] && { echo "argument 1 required."; exit 1; }
[ -z "$2" ] && { echo "argument 2 required."; exit 1; }

export SDL_WII_REPO=https://github.com/dborth/sdl-wii.git

# This needs to be run after dk-3ds-dependencies.sh
# Wii portlibs are provided by the same repository as the 3DS.

export DEVKITPRO=$1
export DEVKITPPC=$2
export PATH="$PATH:$DEVKITPPC/bin"
#export PATH="/mzx-build-workingdir/3ds_portlibs/utils:$PATH"


echo ""
echo "/********************/"
echo "  Wii - dependencies  "
echo "/********************/"

pacman --needed --noconfirm -S devkitPPC libogc libfat-ogc gamecube-tools
pacman --needed --noconfirm -S ppc-zlib ppc-libpng ppc-libogg ppc-libvorbisidec


#echo ""
#echo "/************/"
#echo "  Wii - zlib  "
#echo "/************/"

#pkg-build-wii zlib


#echo ""
#echo "/**************/"
#echo "  Wii - libpng  "
#echo "/**************/"

#pkg-build-wii libpng


#echo ""
#echo "/*****************/"
#echo "  Wii - libtremor  "
#echo "/*****************/"

#pkg-build-wii libtremor-lowmem


echo ""
echo "/***************/"
echo "  Wii - SDL Wii  "
echo "/***************/"

rm -rf /mzx-build-workingdir/sdl-wii
git clone $SDL_WII_REPO /mzx-build-workingdir/sdl-wii
cd /mzx-build-workingdir/sdl-wii/SDL

make -j8
make install
