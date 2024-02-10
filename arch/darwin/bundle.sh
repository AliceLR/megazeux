#!/bin/sh

ARCH=$1
shift

for exe in $@; do
	if [ -f "$exe" ]; then
		mkdir -p ./bundles
		dylibbundler -cd -of -b -x $exe \
		  -d "./bundles/libs-$ARCH/" -p "@executable_path/../libs-$ARCH/"
	fi
done
