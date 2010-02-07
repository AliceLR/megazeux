#!/bin/sh

GEOMS="16x16 32x32 48x48"
SET="$1"

# need args
if [ "$SET" = "" ]; then
	echo "usage: $0 [setname]"
	exit 0
fi

# check for tarball
if [ ! -f "$SET.tar.gz" ]; then
	echo "Run genset.sh before this script!"
	exit 1
fi

# extract the set
mkdir -p tmp
tar -C tmp -zxf $SET.tar.gz

# build the ICO
for GEOM in $GEOMS; do PNGS="$PNGS tmp/$GEOM/apps/megazeux.png"; done
../png2ico/png2ico megazeux.ico $PNGS

# tidy up
rm -rf tmp
