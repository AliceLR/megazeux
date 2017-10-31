#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITARM

export PORTLIBS_REPO=https://github.com/sypherce/nds_portlibs.git

export DEVKITPRO=$1
export DEVKITARM=$2
export PATH="$PATH:$DEVKITARM/bin"


echo ""
echo "/********************/"
echo "  NDS - dependencies  "
echo "/********************/"

cd /mzx-build-workingdir
rm -rf nds-portlibs
git clone $PORTLIBS_REPO "nds-portlibs"
cd nds-portlibs


echo ""
echo "/************/"
echo "  NDS - zlib  "
echo "/************/"

cd /mzx-build-workingdir/nds-portlibs

make zlib           PORTLIBS_PATH=../.build-zlib -j8
make install-zlib   PORTLIBS_PATH=../.build-zlib

cp -r .build-zlib/armv5te /mzx-build-workingdir/megazeux/arch/nds/zlib


echo ""
echo "/******************/"
echo "  NDS - ndsScreens  "
echo "/******************/"

cd /mzx-build-workingdir/megazeux

7za x scripts/deps/nds.zip -oarch/nds/ -aoa
