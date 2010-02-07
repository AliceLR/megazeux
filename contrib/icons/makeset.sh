#!/bin/sh

#
# creates icon packs using the correct names from the fdo icon standard:
# http://standards.freedesktop.org/icon-naming-spec/icon-naming-spec-0.8.html
#

GEOMS="128x128 64x64 48x48 32x32 22x22 16x16"
SET="$1"

# need arguments
if [ "$SET" = "" ]; then
	echo "usage: $0 [setname]"
	exit 0
fi

# couldn't find set directory
if [ ! -d "$SET" ]; then
	echo "Set $SET not found (or in wrong directory)."
	exit 1
fi

echo "Generating fdo archive for \"$SET\" set."

pushd $SET >/dev/null

for CLASS in apps mimetypes; do
	for GEOM in $GEOMS; do
		# class might not exist in this set
		[ "`ls *-$CLASS.* 2>/dev/null`" = "" ] && continue

		# create class subdir and generate for all geoms
		mkdir -p $GEOM/$CLASS
		for ICON in *-$CLASS.*; do
			BASE=`echo $ICON | sed 's,-'$CLASS'.*,,g'`
			convert $ICON -scale $GEOM $GEOM/$CLASS/$BASE.png
		done
	done
done

# generate the tarball and remove the generated set
tar -zcf ../$SET.tar.gz $GEOMS
rm -rf $GEOMS

popd >/dev/null
