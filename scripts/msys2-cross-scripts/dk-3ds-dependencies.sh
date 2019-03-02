#!/bin/bash

[ -z "$DEVKITPRO" ] && { echo "DEVKITPRO environment variable must be set!"; exit 1; }

export BANNERTOOL_REPO=https://github.com/Steveice10/bannertool
export BUILDTOOLS_REPO=https://github.com/Steveice10/buildtools
export    MAKEROM_REPO=https://github.com/profi200/Project_CTR

export DEVKITPRO=`cygpath -u "$DEVKITPRO"`
export PATH="$DEVKITPRO/devkitARM/bin:$PATH"


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
