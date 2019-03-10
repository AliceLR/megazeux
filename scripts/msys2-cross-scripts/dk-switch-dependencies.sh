#!/bin/bash

[ -z "$DEVKITPRO" ] && { echo "DEVKITPRO environment variable must be set!"; exit 1; }

export DEVKITPRO=`cygpath -u "$DEVKITPRO"`
export PATH="$PATH:$DEVKITPRO/devkitA64/bin"


echo ""
echo "/***********************/"
echo "  Switch - dependencies  "
echo "/***********************/"

pacman --needed --noconfirm -S devkitA64 libnx switch-tools
pacman --needed --noconfirm -S switch-zlib switch-libpng switch-libogg switch-libvorbis
