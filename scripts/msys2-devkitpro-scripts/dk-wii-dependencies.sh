#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITPPC

[ -z "$1" ] && { echo "argument 1 required."; exit 1; }
[ -z "$2" ] && { echo "argument 2 required."; exit 1; }

export SDL_WII_REPO=https://github.com/dborth/sdl-wii.git

export DEVKITPRO=$1
export DEVKITPPC=$2
export PATH="$PATH:$DEVKITPPC/bin"


echo ""
echo "/********************/"
echo "  Wii - dependencies  "
echo "/********************/"

pacman --needed --noconfirm -S devkitPPC libogc libfat-ogc gamecube-tools
pacman --needed --noconfirm -S ppc-zlib ppc-libpng ppc-libogg ppc-libvorbisidec


echo ""
echo "/***************/"
echo "  Wii - SDL Wii  "
echo "/***************/"

rm -rf /mzx-build-workingdir/sdl-wii
git clone $SDL_WII_REPO /mzx-build-workingdir/sdl-wii
cd /mzx-build-workingdir/sdl-wii/SDL

make -j8
make install
