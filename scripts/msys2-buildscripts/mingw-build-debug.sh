#!/bin/bash
# $1 - x64 or x86

# Clear the debug files out of the release folder (used for the updater).

cd /mzx-build-workingdir/megazeux/build/windows-$1/
find . -type f -name '*.debug' -delete

# Clear the debug files out of the archive and put them in their own archive.

cd /mzx-build-workingdir/megazeux/build/dist/windows-$1/
source ../../../version.inc

7za e $TARGET-$1.zip              *.debug -r
7za d $TARGET-$1.zip              *.debug -r
7za a -tzip $TARGET-$1-debug.zip  *.debug
rm *.debug
