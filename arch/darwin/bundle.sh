#!/bin/sh

ARCH=$1
shift

# PPC64 hotfix--replace otool with otool-classic.
# LLVM-based otool uses llvm-objdump, does not support PowerPC64 LC_UNIXTHREAD.
# dylibbundler currently does not support replacing otool by other means.
if [ "$ARCH" = "ppc64" ]; then
	export PATH="$(pwd)/arch/darwin/bin:$PATH"
fi

# Note: do not codesign here since there's no guarantee the MZX binaries
# are being processed by dylibbundler in the "correct" order for codesign.
# Signing should instead be performed during the .app bundling stage,
# after the executables have been combined with lipo.
#
for exe in $@; do
	if [ -f "$exe" ]; then
		mkdir -p ./bundles
		dylibbundler -ns -cd -of -b -x $exe \
		  -d "./bundles/libs-$ARCH/" -p "@executable_path/../Frameworks/libs-$ARCH/"
	fi
done
