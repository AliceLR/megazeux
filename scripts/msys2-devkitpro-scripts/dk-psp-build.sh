#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITPSP
# $3 = branch

[ -z "$1" ] && { echo "argument 1 required."; exit 1; }
[ -z "$2" ] && { echo "argument 2 required."; exit 1; }
[ -z "$3" ] && { echo "argument 3 required."; exit 1; }

export DEVKITPRO=$1
export DEVKITPSP=$2
export PATH="$PATH:$DEVKITPSP/bin"

cd /mzx-build-workingdir
mkdir -p zips
cd megazeux

git checkout $3

arch/psp/CONFIG.PSP
make debuglink -j8
make package
make archive

mv build/dist/psp/* /mzx-build-workingdir/zips/

make distclean
