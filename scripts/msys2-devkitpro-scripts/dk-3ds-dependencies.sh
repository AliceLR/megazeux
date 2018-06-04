#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITARM

[ -z "$1" ] && { echo "argument 1 required."; exit 1; }
[ -z "$2" ] && { echo "argument 2 required."; exit 1; }

export BANNERTOOL_REPO=https://github.com/Steveice10/bannertool
export BUILDTOOLS_REPO=https://github.com/Steveice10/buildtools
export    MAKEROM_REPO=https://github.com/profi200/Project_CTR

export DEVKITPRO=$1
export DEVKITARM=$2
export PATH="$DEVKITARM/bin:$PATH"
#export PATH="/mzx-build-workingdir/3ds_portlibs/utils:$PATH"


echo ""
echo "/********************/"
echo "  3DS - dependencies  "
echo "/********************/"

pacman --needed --noconfirm -S devkitARM libctru citro3d picasso 3dstools general-tools
pacman --needed --noconfirm -S 3ds-zlib 3ds-libpng 3ds-libogg 3ds-libvorbisidec


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
cp output/windows-x86_64/bannertool.exe $DEVKITPRO/tools/bin


echo ""
echo "/***************/"
echo "  3DS - makerom  "
echo "/***************/"

cd /mzx-build-workingdir

rm -rf "makerom"
git clone $MAKEROM_REPO "makerom"

cd "makerom/makerom"
make -j8
cp makerom.exe $DEVKITPRO/tools/bin


#echo ""
#echo "/************/"
#echo "  3DS - zlib  "
#echo "/************/"

#pkg-build zlib


#echo ""
#echo "/**************/"
#echo "  3DS - libpng  "
#echo "/**************/"

#pkg-build libpng


#echo ""
#echo "/*****************/"
#echo "  3DS - libtremor  "
#echo "/*****************/"

#pkg-build libtremor-lowmem
