#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITPPC
# $3 = branch

export DEVKITPRO=$1
export DEVKITPPC=$2
export PATH="$PATH:$DEVKITPPC/bin"

cd /mzx-build-workingdir
mkdir -p zips
cd megazeux

git checkout $3

arch/wii/CONFIG.WII
make clean
make debuglink -j8
make package
make archive

mv build/dist/wii/* /mzx-build-workingdir/zips/
