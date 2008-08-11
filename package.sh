#!/bin/sh
#
# Because nobody's perfect.. ;-)
#
################################################################################

#
# The basename for the source and binary packages.
#
TARGET=`cat Makefile.in | grep TARGET | cut -d " " -f6`

if [ "$TARGET" == "" ]; then
	echo Could not determine version!
	exit 1
fi

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
DOCS="docs/COPYING.doc docs/changelog.txt docs/port.txt docs/macro.txt"

#
# Hack for windows
#
SDL="/usr/lib/SDL.dll"

#
# MegaZeux's build system dependencies; these are packaged in
# addition to binary deps above to complete the source package.
#
BUILD_DEPS="config.sh Makefile Makefile.in package.sh"

#
# These directories are purely for source distributions.
#
SUBDIRS="arch contrib docs"

#
# What we actually care about; the complete sources to MegaZeux. Try to
# extract crap Exo's left in the wrong place. Feel free to update this.
#
SRC="src/*.cpp src/*.h src/Makefile"

echo Generating sources in $TARGET and binary package with $TARGET.exe..

#
# Do source package.
#
################################################################################

echo Collating sources..

if [ -e dist/$TARGET ]; then
	rm -rf dist/$TARGET
fi

mkdir -p dist/$TARGET &&
mkdir -p dist/$TARGET/src &&
cp -pv $BINARY_DEPS $BUILD_DEPS dist/$TARGET &&
cp -pvr $SUBDIRS dist/$TARGET &&
cp -pv $SRC dist/$TARGET/src &&

# hack for gdm2s3m & libmodplug
rm -f dist/$TARGET/contrib/gdm2s3m/src/{*.a,*.o} &&
rm -f dist/$TARGET/contrib/libmodplug/src/{*.a,*.o} &&

# hack for "dist" makefile
cp dist/$TARGET/arch/Makefile.dist dist/$TARGET/Makefile.platform

if [ "$?" != "0" ]; then
	echo Some error occured during source build, aborted.
	exit 2
fi

rm -f dist/$TARGET/src/config.h

echo Creating source tar ${TARGET}src.tar.gz..

cd dist
tar --exclude CVS -jcvf ${TARGET}src.tar.bz2 $TARGET
cd ..

if [ "$?" != "0" ]; then
	echo Some error occured during packaging, aborted.
	exit 3
fi

rm -rf dist/$TARGET

echo Built source distribution successfully!


# no binary package is required
if [ "$1" != "-b" ]; then
	exit 0
fi

#
# Do binary package.
#
################################################################################

#
# Remove destination, a 7zip bug means that preexisting files get updated
# implicitly.
#
if [ -e dist/$TARGET.zip ]; then
	rm -f dist/$TARGET.zip
fi

#
# Create the binary package.
#
7za.exe a -tzip dist/$TARGET.zip $BINARY_DEPS $DOCS $TARGET.exe

#
# Hack for SDL inclusion (we don't want to store pathname, and 7zip is too
# lame to have a flag for it).
#
cp -f $SDL .
7za.exe u -tzip dist/$TARGET.zip SDL.dll
rm -f SDL.dll
