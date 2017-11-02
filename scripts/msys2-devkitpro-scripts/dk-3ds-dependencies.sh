#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITARM

export   PORTLIBS_REPO=https://github.com/asiekierka/3ds_portlibs.git
export BANNERTOOL_REPO=https://github.com/Steveice10/bannertool
export BUILDTOOLS_REPO=https://github.com/Steveice10/buildtools
export    MAKEROM_REPO=https://github.com/profi200/Project_CTR

export DEVKITPRO=$1
export DEVKITARM=$2
export PATH="$PATH:$DEVKITARM/bin"


echo ""
echo "/******************/"
echo "  3DS - bannertool  "
echo "/******************/"

# Some of the tools required by the 3DS Makefile aren't currently in devkitPro,
# so they have to be built manually.

cd /mzx-build-workingdir

rm -rf "bannertool"
git clone $BANNERTOOL_REPO "bannertool"

rm -rf "bannertool/buildtools"
git clone $BUILDTOOLS_REPO "bannertool/buildtools"

cd "bannertool"
make -j8
cp output/windows-x86_64/bannertool.exe $DEVKITARM/bin


echo ""
echo "/***************/"
echo "  3DS - makerom  "
echo "/***************/"

cd /mzx-build-workingdir

rm -rf "makerom"
git clone $MAKEROM_REPO "makerom"

cd "makerom/makerom"
make -j8
cp makerom.exe $DEVKITARM/bin


echo ""
echo "/****************/"
echo "  3DS - portlibs  "
echo "/****************/"

cd /mzx-build-workingdir
rm -rf 3ds_portlibs
git clone $PORTLIBS_REPO "3ds_portlibs"
cd 3ds_portlibs

# portlibs will fail without this folder, but doesn't currently produce it.
mkdir -p "files"

# The following scripts need this on the path.
export PATH="$PATH:/mzx-build-workingdir/3ds_portlibs/utils"


echo ""
echo "/************/"
echo "  3DS - zlib  "
echo "/************/"

pkg-build zlib


echo ""
echo "/**************/"
echo "  3DS - libpng  "
echo "/**************/"

pkg-build libpng


echo ""
echo "/*****************/"
echo "  3DS - libtremor  "
echo "/*****************/"

# FIXME the libtremor portlibs builds requires libogg, which we don't want,
# so use this version that points to the lowmem branch.

cp /dk-patches/libtremor-lowmem packages/

pkg-build libtremor-lowmem
