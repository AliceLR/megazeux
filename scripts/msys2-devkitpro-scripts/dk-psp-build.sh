#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITPSP
# $3 = branch

export DEVKITPRO=$1
export DEVKITPSP=$2
export PATH="$PATH:$DEVKITPSP/bin"

cd /mzx-build-workingdir
mkdir -p zips
cd megazeux

git checkout $3

arch/psp/CONFIG.PSP
make clean
make debuglink -j8
make package
make archive

mv build/dist/psp/* /mzx-build-workingdir/zips/
