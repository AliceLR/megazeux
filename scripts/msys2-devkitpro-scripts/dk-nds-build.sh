#!/bin/bash
# $1 = branch

[ -z "$DEVKITPRO" ] && { echo "DEVKITPRO environment variable must be set!"; exit 1; }
[ -z "$1" ] && { echo "argument 1 required."; exit 1; }

export DEVKITPRO=`cygpath -u "$DEVKITPRO"`

export PATH="$PATH:$DEVKITPRO/devkitARM/bin"
export PATH="$PATH:$DEVKITPRO/tools/bin"

cd /mzx-build-workingdir
mkdir -p zips
cd megazeux

git checkout $1

arch/nds/CONFIG.NDS
make debuglink -j8
make package
make archive

mv build/dist/nds/* /mzx-build-workingdir/zips/

make distclean
