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

# FIXME using SDL Wii for now. Want to switch back to hardware eventually.
arch/wii/CONFIG.SDLWII
make clean
make debuglink -j8
make package
make archive

mv build/dist/wii/* /mzx-build-workingdir/zips/
