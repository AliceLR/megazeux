#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITARM

[ -z "$1" ] && { echo "argument 1 required."; exit 1; }
[ -z "$2" ] && { echo "argument 2 required."; exit 1; }

export PORTLIBS_REPO=https://github.com/sypherce/nds_portlibs.git

export DEVKITPRO=$1
export DEVKITARM=$2
export PATH="$PATH:$DEVKITARM/bin"


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
