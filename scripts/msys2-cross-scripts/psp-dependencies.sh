#!/bin/bash

[[ -z $PSPDEV ]] && { echo "\$PSPDEV is unset. Aborting"; exit 1; }
export PATH="$PSPDEV/bin:$PATH"

export PORTS_REPO=https://github.com/pspdev/psp-ports.git


echo ""
echo "/********************/"
echo "  PSP - dependencies  "
echo "/********************/"

pacman --needed --noconfirm -S autoconf automake libtool patch mingw-w64-x86_64-imagemagick

cd /mzx-build-workingdir
rm -rf psp-ports
git clone $PORTS_REPO "psp-ports"

# The copy of the SDK formerly distributed with the Windows version of
# devkitPro was missing an include. If the include is missing, patch
# it back in.

if ! grep -q "psptypes" $PSPDEV/psp/sdk/include/pspge.h ; then
  patch $PSPDEV/psp/sdk/include/pspge.h /cross-patches/pspge.patch
fi


echo ""
echo "/************/"
echo "  PSP - zlib  "
echo "/************/"

cd /mzx-build-workingdir/psp-ports/zlib

make -j8
make install


echo ""
echo "/**************/"
echo "  PSP - libpng  "
echo "/**************/"

cd /mzx-build-workingdir/psp-ports/libpng

make -j8
make install


echo ""
echo "/**************/"
echo "  PSP - tremor  "
echo "/**************/"

cd /mzx-build-workingdir/psp-ports/libTremor

LDFLAGS="-L$(psp-config --pspsdk-path)/lib" LIBS="-lc -lpspuser" ./autogen.sh \
     --host psp --prefix=$(psp-config --psp-prefix)

make -j8
make install


echo ""
echo "/***********/"
echo "  PSP - SDL  "
echo "/***********/"

cd /mzx-build-workingdir/psp-ports/SDL

./autogen.sh
LDFLAGS="-L$(psp-config --pspsdk-path)/lib" LIBS="-lc -lpspuser" \
 ./configure --host psp --prefix=$(psp-config --psp-prefix)

# If you thought that pspge.h patch was pretty cool, then you should
# know that this generated a worthless SDL_config.h. Replace it with
# our own based on SDL2's PSP config.

cp /cross-patches/SDL_config_psp.h include/SDL_config.h

make -j8
make install


echo ""
echo "/*****************/"
echo "  PSP - pspirkeyb  "
echo "/*****************/"

cd /mzx-build-workingdir/psp-ports/pspirkeyb

make -j8
make install


echo ""
echo "/*************/"
echo "  PSP - pspgl  "
echo "/*************/"

cd /mzx-build-workingdir/psp-ports/pspgl

export PSP_MOUNTDIR=/Volumes/PSP
export PSP_REVISION=1.50
make -j8
make install
