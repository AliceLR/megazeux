#!/bin/sh

usage() {
	echo "usage: $0 [-b win32 | win64 | psp]"
	echo
	echo "	-b	Builds a binary distribution for the specified arch."
	echo "	-h	Displays this help text."
	exit 0
}

#
# createpspzip
#
createpspzip() {
	#
	# Create the binary package.
	#
	$SEVENZIP a -tzip dist/$TARGET-psp.zip \
		$BINARY_DEPS EBOOT.PBP $DOCS $PSPPAD
}

#
# createzip_dynamic_sdl </path/to/SDL.dll> <POSTFIX>
#
createzip_dynamic_sdl() {
	DIRECTX_BAT="directx.bat"

	#
	# Copy SDL here temporarily.
	#
	cp -f $1 SDL.dll &&

	#
	# Copy utils here temporarily.
	#
	mkdir utils &&
	cp -f src/utils/checkres{.{exe,bat},-readme.txt} utils &&
	cp -f src/utils/txt2hlp.exe utils &&

	#
	# Generate a suitable directx.bat
	#
	echo "set SDL_VIDEODRIVER=directx" > $DIRECTX_BAT &&
	echo "start $TARGET.exe"          >> $DIRECTX_BAT &&

	#
	# pack the EXEs
	#
	for BINARY in $TARGET.exe utils/checkres.exe utils/txt2hlp.exe; do
		( $UPX --best $BINARY || echo "UPX unavailable." );
	done &&

	#
	# Create the binary package.
	#
	$SEVENZIP a -tzip dist/$TARGET-$2.zip \
		$BINARY_DEPS $DOCS $TARGET.exe SDL.dll $DIRECTX_BAT utils &&

	#
	# Remove SDL, and the bat file.
	#
	rm -rf utils &&
	rm -f SDL.dll $DIRECTX_BAT
}

#
# createtbz tar-name-ext [extra-binary]
#
createtbz() {
	#
	# create temporary directory
	#
	mkdir -p dist/$TARGET/docs &&

	#
	# copy binaries into it
	#
	cp -f --dereference $BINARY_DEPS $TARGET $2 dist/$TARGET &&

	#
	# copy docs over
	#
	cp -f --dereference $DOCS dist/$TARGET/docs &&

	#
	# tar it up
	#
	tar -C dist -jcvf dist/$TARGET-$1.tar.bz2 $TARGET &&

	#
	# now delete temporary dir
	#
	rm -rf dist/$TARGET
}

#
# in case of error; breakout CODE
#
breakout() {
	echo "Error $1 occured during packaging, aborted."
	exit $1
}

#
# The basename for the source and binary packages.
#
TARGET=`grep TARGET Makefile | head -n1 | sed "s/ //g" | cut -d "=" -f 2`

[ "$TARGET" == "" ] && breakout 1
[ "$1" == "-h" ] && usage

#
# MegaZeux source AND binary distributions depend on these
# files being here. Update this list carefully; things that
# are in source but NOT the binary package should be in
# build deps below.
#
BINARY_DEPS="smzx.pal mzx_ascii.chr mzx_blank.chr mzx_default.chr \
             mzx_help.fil mzx_smzx.chr mzx_edit.chr config.txt"

#
# Documents that the binary zip should contain (pathname will be stored too).
#
DOCS="docs/COPYING.DOC docs/changelog.txt docs/port.txt docs/macro.txt"

#
# Various joypad configuration files.
#
PSPPAD="pad.config"
GP2XPAD="pad.config.gp2x"

#
# MegaZeux's build system dependencies; these are packaged in
# addition to binary deps above to complete the source package.
#
BUILD_DEPS="config.sh Makefile package.sh $PSPPAD $GP2XPAD macosx.zip"

#
# These directories are purely for source distributions.
#
SUBDIRS="arch contrib debian docs"

#
# What we actually care about; the complete sources to MegaZeux. Try to
# extract crap Exo's left in the wrong place. Feel free to update this.
#
SRC="src/*.c src/*.cpp src/*.h src/Makefile.in src/old src/utils"

#
# Name of the 7zip extractor. On Windows, this is '7za.exe'. On Linux, this is
# _usually_ '7za', but if you're using a compatible replacement, change this
# here. Only affects Windows binary distributions and PSP binary distributions,
# otherwise GNU tar is used instead.
#
SEVENZIP="7za"

#
# Name of the Ultimate Packer for eXecutables program. On Windows, this is
# 'upx.exe'. On Linux, this is 'upx'. If yours is called something different,
# change it here. This program has to be in $PATH or it is not used.
#
UPX="upx"

#
# Do source package.
#
################################################################################

echo "Generating source package for $TARGET.."

#
# dist cannot safely exist prior to starting.
#
if [ -d dist ]; then
	echo "Destroying dist/.."
	rm -rf dist
fi

mkdir -p dist/$TARGET/src &&
cp -pv $BINARY_DEPS $BUILD_DEPS dist/$TARGET &&
cp -pvr $SUBDIRS dist/$TARGET &&
cp -pvr $SRC dist/$TARGET/src &&

# hack for gdm2s3m & libmodplug & misc
rm -f dist/$TARGET/contrib/gdm2s3m/src/{*.a,*.o} &&
rm -f dist/$TARGET/contrib/libmodplug/src/{*.a,*.o} &&
rm -f dist/$TARGET/contrib/unzip/src/{*.a,*.o} &&
rm -f dist/$TARGET/src/utils/*.o &&
rm -f dist/$TARGET/src/utils/txt2hlp{,.dbg}{,.exe} &&
rm -f dist/$TARGET/src/utils/checkres{,.dbg}{,.exe} &&
rm -f dist/$TARGET/contrib/icons/*.o &&

# hack for "dist" makefile
cp dist/$TARGET/arch/Makefile.dist dist/$TARGET/Makefile.platform

[ "$?" != "0" ] && breakout 2

rm -f dist/$TARGET/src/config.h

echo "Creating source (${TARGET}src.tar.bz2).."

cd dist
tar --exclude .svn -jcvf ${TARGET}src.tar.bz2 $TARGET
cd ..

[ "$?" != "0" ] && breakout 3

rm -rf dist/$TARGET

echo Built source distribution successfully!

#
# no binary package is required
#
if [ "$1" != "-b" ]; then
	echo "Skipping binary package build."
	exit 0
fi

#
# Do binary package.
#
################################################################################

echo "Generating binary package for $2.."

#
# PSP, using ZIP compression via 7ZIP compressor (add pad config)
#
if [ "$2" = "psp" ]; then
	createpspzip
	exit
fi

#
# Windows x86, using ZIP compression via 7ZIP compressor
#
if [ "$2" = "win32" ]; then
	LIBSDL="`sdl-config --prefix`/bin/SDL.dll"
	createzip_dynamic_sdl $LIBSDL x86
	exit
fi

#
# Windows x64, using ZIP compression via 7ZIP compressor
#
if [ "$2" = "win64" ]; then
	LIBSDL="`sdl-config --prefix`/bin/SDL.dll"
	createzip_dynamic_sdl $LIBSDL x64
	exit
fi

#
# Unknown binary arch
#
echo "Unknown binary architecture.."
echo
usage

