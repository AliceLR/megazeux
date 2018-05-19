#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITA64

[ -z "$1" ] && { echo "argument 1 required."; exit 1; }
[ -z "$2" ] && { echo "argument 2 required."; exit 1; }

export DEVKITPRO=$1
export DEVKITA64=$2
export PATH="$PATH:$DEVKITA64/bin"


echo ""
echo "/***********************/"
echo "  Switch - dependencies  "
echo "/***********************/"

pacman --needed --noconfirm -S devkitA64 libnx switch-tools
pacman --needed --noconfirm -S switch-zlib switch-libpng switch-libogg switch-libvorbis
