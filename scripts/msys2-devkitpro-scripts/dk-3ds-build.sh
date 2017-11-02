#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITARM
# $3 = branch

export DEVKITPRO=$1
export DEVKITARM=$2
export PATH="$PATH:$DEVKITARM/bin"

cd /mzx-build-workingdir
mkdir -p zips
cd megazeux

git checkout $3

arch/3ds/CONFIG.3DS
make clean
make shaders
make package -j8
make archive

mv build/dist/3ds/* /mzx-build-workingdir/zips/
