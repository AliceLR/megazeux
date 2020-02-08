#!/bin/bash

[ -z "$DEVKITPRO" ] && { echo "DEVKITPRO environment variable must be set!"; exit 1; }

export DEVKITPRO=`cygpath -u "$DEVKITPRO"`
export PATH="$PATH:$DEVKITPRO/devkitARM/bin"


echo ""
echo "/********************/"
echo "  NDS - dependencies  "
echo "/********************/"

pacman --needed --noconfirm -S devkitARM libnds libfat-nds maxmod-nds nds-zlib ndstool dstools

cd /mzx-build-workingdir


echo ""
echo "/******************/"
echo "  NDS - ndsScreens  "
echo "/******************/"

cd /mzx-build-workingdir/megazeux

7za x scripts/deps/nds.zip -oarch/nds/ -aoa
