#!/bin/bash

[ -z "$DEVKITPRO" ] && { echo "DEVKITPRO environment variable must be set!"; exit 1; }

export PORTLIBS_REPO=https://github.com/sypherce/nds_portlibs.git

export DEVKITPRO=`cygpath -u "$DEVKITPRO"`
export PATH="$PATH:$DEVKITPRO/devkitARM/bin"


echo ""
echo "/********************/"
echo "  NDS - dependencies  "
echo "/********************/"

pacman --needed --noconfirm -S devkitARM libnds libfat-nds maxmod-nds ndstool dstools

cd /mzx-build-workingdir
rm -rf nds-portlibs
git clone $PORTLIBS_REPO "nds-portlibs"
cd nds-portlibs


echo ""
echo "/************/"
echo "  NDS - zlib  "
echo "/************/"

cd /mzx-build-workingdir/nds-portlibs

make zlib -j8
make install-zlib


echo ""
echo "/******************/"
echo "  NDS - ndsScreens  "
echo "/******************/"

cd /mzx-build-workingdir/megazeux

7za x scripts/deps/nds.zip -oarch/nds/ -aoa
