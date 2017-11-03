#!/bin/bash
# $1 - x64 or x86
# $2 - 2.90 or master or debytecode

# Clear the debug files out of the build folder and put them in their own archive.

cd /mzx-build-workingdir/megazeux/build

source ../version.inc

rm -rf windows-$1-debug
mkdir windows-$1-debug
mkdir windows-$1-debug/utils

mv windows-$1/*.debug       windows-$1-debug/
mv windows-$1/utils/*.debug windows-$1-debug/utils/

cd windows-$1-debug
rm -f ../dist/windows-$1/$TARGET-$1-debug.zip
7za a -tzip ../dist/windows-$1/$TARGET-$1-debug.zip *

cd ..
rm -rf windows-$1-debug

mkdir -p /mzx-build-workingdir/zips/$2
mv dist/windows-$1/$TARGET-$1-debug.zip /mzx-build-workingdir/zips/$2
