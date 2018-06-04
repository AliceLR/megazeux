#!/bin/bash
# $1 = $DEVKITPRO
# $2 = $DEVKITA64
# $3 = branch

[ -z "$1" ] && { echo "argument 1 required."; exit 1; }
[ -z "$2" ] && { echo "argument 2 required."; exit 1; }
[ -z "$3" ] && { echo "argument 3 required."; exit 1; }

export DEVKITPRO=$1
export DEVKITA64=$2
export PATH="$PATH:$DEVKITA64/bin"
export PATH="$PATH:$DEVKITPRO/tools/bin"

cd /mzx-build-workingdir
mkdir -p zips
cd megazeux

git checkout $3

arch/switch/CONFIG.SWITCH
make debuglink -j8
make package
make archive

mv build/dist/switch/* /mzx-build-workingdir/zips/

make distclean
