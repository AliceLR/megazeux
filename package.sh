#!/bin/sh

#
# This trivial script helps automate the correct packaging of binaries
# and their dependencies for a multitude of platforms. It makes (good)
# assumptions about the compile options in use and the dependencies
# required.
#

usage() {
	echo "usage: $0 [-b win32 | win64 | psp | nds | gp2x | wii | darwin]"
	echo
	echo "	-b	Builds a binary distribution for the specified arch."
	echo "	-h	Displays this help text."
	exit 0
}

#
# createzip_psp
#
createzip_psp() {
	cp -f $PSPPAD pad.config &&
	$SEVENZIP a -tzip dist/$TARGET-psp.zip \
		$BINARY_DEPS EBOOT.PBP $DOCS pad.config &&
	rm -f pad.config
}

#
# createzip_nds
#
createzip_nds() {
	mv ${TARGET}run.elf.nds ${TARGET}run.nds &&
	cp -f $NDSPAD pad.config &&
	$SEVENZIP a -tzip dist/$TARGET-nds.zip \
		$BINARY_DEPS ${TARGET}run.nds $DOCS pad.config &&
	rm -f pad.config
}

#
# createzip_gp2x
#
createzip_gp2x() {
	cp -f $GP2XPAD pad.config &&
	convert -scale 32x32 contrib/icons/quantump.png $TARGET.png &&
	$SEVENZIP a -tzip dist/$TARGET-gp2x.zip \
		$BINARY_DEPS ${TARGET}run.gpe $DOCS pad.config $TARGET.png &&
	rm -f pad.config $TARGET.png
}

#
# createzip_wii
#
createzip_wii() {
	mkdir apps &&
	mkdir apps/megazeux &&
	mkdir apps/megazeux/docs &&
	cp -f $WIIPAD apps/megazeux/pad.config &&
	cp -f arch/wii/icon.png arch/wii/meta.xml apps/megazeux/ &&
	sed "s/%VERSION%/$VERSION/g;s/%DATE%/`date -u +%Y%m%d%H%M`/g" \
		arch/wii/meta.xml > apps/megazeux/meta.xml &&
	cp -f $BINARY_DEPS boot.dol apps/megazeux/ &&
	cp -f $DOCS apps/megazeux/docs/ &&
	$SEVENZIP a -tzip dist/$TARGET-wii.zip apps &&
	rm -rf apps
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
	cp -f src/utils/checkres{.bat,-readme.txt} utils &&
	cp -f src/utils/*.exe utils &&

	#
	# Generate a suitable directx.bat
	#
	echo "set SDL_VIDEODRIVER=directx" > $DIRECTX_BAT &&
	echo "start $TARGET.exe"          >> $DIRECTX_BAT &&

	#
	# Create the binary package.
	#
	$SEVENZIP a -tzip dist/$TARGET-$2.zip \
		$BINARY_DEPS $HELP_FILE $GLSL_PROGRAMS $DOCS \
		$TARGET.exe ${TARGET}run.exe core.dll editor.dll \
		SDL.dll $DIRECTX_BAT utils &&

	#
	# Remove SDL, and the bat file.
	#
	rm -rf utils &&
	rm -f SDL.dll $DIRECTX_BAT
}

#
# createUnifiedDMG
#
createUnifiedDMG() {
	if [ ! -f $TARGET -a ! -f $TARGET.i686 -a ! -f $TARGET.ppc ]; then
		echo "Neither $TARGET.i686 nor $TARGET.ppc are present!"
		breakout 4
	fi

	if [ -f $TARGET.i686 -a ! -f $TARGET.ppc ]; then
		mv $TARGET.i686 $TARGET
	elif [ -f $TARGET.ppc -a ! -f $TARGET.i686 ]; then
		mv $TARGET.ppc $TARGET
	elif [ ! -f $TARGET ]; then
		lipo -create $TARGET.i686 $TARGET.ppc -output $TARGET &&
		rm -f $TARGET.i686 $TARGET.ppc
	fi

	if [ ! -f $TARGET ]; then
		echo "Failed to compile $TARGET.."
		breakout 5
	fi

	#
	# Prep the DMG's directory structure in a temp directory
	#
	CONTENTS=dist/dmgroot/MegaZeux.app/Contents

	mkdir -p ${CONTENTS}/MacOS &&
	mkdir -p ${CONTENTS}/Resources &&
	mkdir -p ${CONTENTS}/Resources/shaders &&
	cp -RP $HOME/workspace/Frameworks ${CONTENTS} &&
	cp $TARGET ${CONTENTS}/MacOS/MegaZeux &&
	cp $BINARY_DEPS $HELP_FILE ${CONTENTS}/Resources &&
        cp $GLSL_PROGRAMS ${CONTENTS}/Resources/shaders &&
	cp contrib/icons/quantump.icns ${CONTENTS}/Resources/MegaZeux.icns &&
	cp arch/darwin/Info.plist ${CONTENTS} &&
	ln -s MegaZeux.app/Contents/Resources/config.txt dist/dmgroot &&
	cp $DOCS dist/dmgroot &&

	#
	# Package it
	#
	DMGNAME=dist/$TARGET.dmg
	VOLNAME=MegaZeux

	hdiutil create $DMGNAME -size 10m -fs HFS+ \
	                        -volname "$VOLNAME" -layout SPUD &&
	DEV_HANDLE=`hdid $DMGNAME | grep Apple_HFS | \
	            perl -e '\$_=<>; /^\\/dev\\/(disk.)/; print \$1'`
	ditto -rsrcFork dist/dmgroot /Volumes/$VOLNAME &&
	hdiutil detach $DEV_HANDLE &&

	hdiutil convert $DMGNAME -format UDZO -o $DMGNAME.compressed &&
	mv -f $DMGNAME.compressed.dmg $DMGNAME &&

	rm -rf dist/dmgroot
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
. ./version.inc

[ "$TARGET" = "" ] && breakout 1
[ "$1" = "-h" ] && usage

#
# MegaZeux source AND binary distributions depend on these
# files being here. Update this list carefully; things that
# are in source but NOT the binary package should be in
# build deps below.
#
BINARY_DEPS="smzx.pal mzx_ascii.chr mzx_blank.chr mzx_default.chr \
             mzx_smzx.chr mzx_edit.chr config.txt"

#
# The help file is also technically a "binary dep", but some
# embedded platforms can omit it if they disable the help system.
#
HELP_FILE="mzx_help.fil"

#
# The GLSL shader programs are technically "binary deps" too, but
# they're useless on embedded platforms, which can choose to omit them.
#
GLSL_PROGRAMS="shaders/*.frag shaders/*.vert"

#
# Documents that the binary zip should contain (pathname will be stored too).
#
DOCS="docs/COPYING.DOC docs/changelog.txt docs/port.txt docs/macro.txt"

#
# Various joypad configuration files.
#
PSPPAD="arch/psp/pad.config"
GP2XPAD="arch/gp2x/pad.config"
NDSPAD="arch/nds/pad.config"
WIIPAD="arch/wii/pad.config"

#
# MegaZeux's build system dependencies; these are packaged in
# addition to binary deps above to complete the source package.
#
BUILD_DEPS="config.sh Makefile package.sh msvc.zip version.inc"

#
# These directories are purely for source distributions.
#
SUBDIRS="arch contrib debian docs"

#
# Name of the 7zip extractor. On Windows, this is '7za.exe'. On Linux, this is
# _usually_ '7za', but if you're using a compatible replacement, change this
# here. Only affects Windows binary distributions and PSP binary distributions,
# otherwise GNU tar is used instead.
#
SEVENZIP="7za"

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

# clean the source tree for DEBUG and non-DEBUG
make package_clean

mkdir -p dist/$TARGET/src &&
mkdir -p dist/$TARGET/shaders &&
cp -p $BINARY_DEPS $HELP_FILE $BUILD_DEPS dist/$TARGET &&
cp -p $GLSL_PROGRAMS dist/$TARGET/shaders &&
cp -pr $SUBDIRS dist/$TARGET &&
cp -pr src/* dist/$TARGET/src &&

# nasty hack for binary packaging (not removed by package_clean)
rm -f dist/$TARGET/src/utils/{checkres,downver,hlp2txt,txt2hlp}{,.exe}{,.debug} &&

echo "PLATFORM=none" > dist/$TARGET/platform.inc

[ "$?" != "0" ] && breakout 2

echo "Creating source (${TARGET}src.tar.bz2).."

cd dist
tar --exclude .svn -jcf ${TARGET}src.tar.bz2 $TARGET
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
# PSP, using ZIP compression via 7ZIP compressor (pad config, no help file)
#
if [ "$2" = "psp" ]; then
	createzip_psp
	exit
fi

#
# NDS, using ZIP compression via 7ZIP compressor (pad config, no help file)
#
if [ "$2" = "nds" ]; then
	createzip_nds
	exit
fi

#
# GP2X, using ZIP compression via 7ZIP compressor (pad config, no help file)
#
if [ "$2" = "gp2x" ]; then
	createzip_gp2x
	exit
fi

#
# Wii, using ZIP compression via 7ZIP compressor (pad config)
#
if [ "$2" = "wii" ]; then
	createzip_wii
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

if [ "$2" = "darwin" ]; then
	createUnifiedDMG
	exit
fi

#
# Unknown binary arch
#
echo "Unknown binary architecture.."
echo
usage

