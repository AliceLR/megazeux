#!/bin/sh

ARCH=$1
shift

# PPC64 hotfix--replace otool with otool-classic.
# LLVM-based otool uses llvm-objdump, does not support PowerPC64 LC_UNIXTHREAD.
# dylibbundler currently does not support replacing otool by other means.
if [ "$ARCH" = "ppc64" ]; then
	export PATH="$(pwd)/arch/darwin/bin:$PATH"
fi

for exe in $@; do
	if [ -f "$exe" ]; then
		mkdir -p ./bundles
		dylibbundler -cd -of -b -x $exe \
		  -d "./bundles/libs-$ARCH/" -p "@executable_path/../libs-$ARCH/"
	fi
done
