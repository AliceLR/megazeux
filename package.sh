#!/bin/sh

#
# This trivial script helps automate the correct packaging of binaries
# and their dependencies for a multitude of platforms. It makes (good)
# assumptions about the compile options in use and the dependencies
# required.
#

usage() {
	echo "usage: $0 [-b darwin ]"
	echo
	echo "	-b	Builds a binary distribution for the specified arch."
	echo "	-h	Displays this help text."
	exit 0
}

#
# createUnifiedDMG
#
createUnifiedDMG() {
	#
	# Lipo all the binaries together, if not already done
	#
	for FILE in megazeux mzxrun libcore.dylib \
	            libeditor.dylib libnetwork.dylib; do
		[ -f ${FILE} ] && continue

		if [ ! -f ${FILE}.i686 -a ! -f ${FILE}.ppc ]; then
			echo "Neither ${FILE}.i686 nor ${FILE}.ppc are present!"
			breakout 4
		fi

		if [ -f ${FILE}.i686 -a ! -f ${FILE}.ppc ]; then
			mv ${FILE}.i686 ${FILE}
		elif [ -f ${FILE}.ppc -a ! -f ${FILE}.i686 ]; then
			mv ${FILE}.ppc ${FILE}
		else
			lipo -create ${FILE}.i686 ${FILE}.ppc -output ${FILE}
			rm -f ${FILE}.i686 ${FILE}.ppc
		fi

		if [ ! -f ${FILE} ]; then
			echo "Failed to compile '${FILE}'.."
			breakout 5
		fi
	done

	#
	# Prep the DMG's directory structure in a temp directory
	#
	CONTENTS=dist/dmgroot/MegaZeux.app/Contents

	mkdir -p ${CONTENTS}/MacOS &&
	mkdir -p ${CONTENTS}/Resources &&
	mkdir -p ${CONTENTS}/Resources/shaders &&
	cp -RP $HOME/workspace/Frameworks ${CONTENTS} &&
	cp megazeux ${CONTENTS}/MacOS/MegaZeux &&
	cp mzxrun ${CONTENTS}/MacOS/MZXRun &&
	cp libcore.dylib libeditor.dylib libnetwork.dylib ${CONTENTS}/MacOS &&
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
# Do binary package.
#
################################################################################

echo "Generating binary package for $2.."

#
# Mac OS X, specialized DMG
#
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
