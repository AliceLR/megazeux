#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITPSP

export PORTS_REPO=https://github.com/pspdev/psp-ports.git

export DEVKITPRO=$1
export DEVKITPSP=$2
export PATH="$PATH:$DEVKITPSP/bin"


echo ""
echo "/********************/"
echo "  PSP - dependencies  "
echo "/********************/"

cd /mzx-build-workingdir
rm -rf psp-ports
git clone $PORTS_REPO "psp-ports"

# The copy of the SDK distributed with the Windows version of
# devkitPro is missing an include. If the include is missing,
# patch it back in.

cd /
if ! grep -q "psptypes" $DEVKITPSP/psp/sdk/include/pspge.h ; then
  patch $DEVKITPSP/psp/sdk/include/pspge.h pspge.patch
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
