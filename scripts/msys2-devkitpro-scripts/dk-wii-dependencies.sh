#!/bin/bash

[ -z "$DEVKITPRO" ] && { echo "DEVKITPRO environment variable must be set!"; exit 1; }

export SDL_WII_REPO=https://github.com/dborth/sdl-wii.git

export DEVKITPRO=`cygpath -u "$DEVKITPRO"`
export PATH="$PATH:$DEVKITPRO/devkitPPC/bin"


echo ""
echo "/********************/"
echo "  Wii - dependencies  "
echo "/********************/"

pacman --needed --noconfirm -S devkitPPC libogc libfat-ogc gamecube-tools
pacman --needed --noconfirm -S ppc-zlib ppc-libpng ppc-libogg ppc-libvorbisidec
