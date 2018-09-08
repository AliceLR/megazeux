#!/bin/bash
# $1 = branch

[ -z "$DEVKITPRO" ] && { echo "DEVKITPRO environment variable must be set!"; exit 1; }
[ -z "$DEVKITPSP" ] && { echo "DEVKITPSP environment variable must be set!"; exit 1; }
[ -z "$1" ] && { echo "argument 1 required."; exit 1; }

export DEVKITPSP=`cygpath -u "$DEVKITPSP"`

export PATH="$PATH:$DEVKITPSP/bin"

cd /mzx-build-workingdir
mkdir -p zips
cd megazeux

git checkout $1

arch/psp/CONFIG.PSP
make debuglink -j8
make package
make archive

mv build/dist/psp/* /mzx-build-workingdir/zips/

make distclean
