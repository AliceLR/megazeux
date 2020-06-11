#!/bin/bash

[ -z "$DEVKITPRO" ] && { echo "DEVKITPRO environment variable must be set!"; exit 1; }

export DEVKITPRO=`cygpath -u "$DEVKITPRO"`
export PATH="$PATH:$DEVKITPRO/devkitPPC/bin"

echo ""
echo "/********************/"
echo " Wii U - dependencies "
echo "/********************/"

pacman --needed --noconfirm -S devkitPPC wut wut-tools
pacman --needed --noconfirm -S ppc-libogg ppc-libvorbisidec wiiu-sdl2
