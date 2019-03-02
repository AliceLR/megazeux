#!/bin/bash
# $1 = branch

[[ -z $PSPDEV ]] && { echo "\$PSPDEV is unset. Aborting"; exit 1; }
export PATH="$PSPDEV/bin:$PATH"

[ -z "$1" ] && { echo "argument 1 required."; exit 1; }

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
