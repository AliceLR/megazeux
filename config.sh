#!/bin/bash

### CONFIG.SH HELP TEXT #######################################################

function usage {
	echo "usage: ./config.sh --platform [platform] <--prefix prefix>"
	echo "                   <--sysconfdir sysconfdir> <option..>"
	echo
	echo " <prefix>        Where MegaZeux's dependencies should be found." 
	echo " <sysconfdir>    Where MegaZeux's config should be read from."
	echo
	echo "Supported [platform] values:"
	echo
	echo "  win32          Microsoft Windows"
	echo "  linux          Linux"
	echo "  macos          Macintosh OS X (not Classic)"
	echo "  linux-static   Linux (statically linked)"
	echo "  psp            Experimental PSP port"
	echo "  mingw32        Use MinGW32 on Linux, to build for win32"
	echo
	echo "Supported <option> values:"
	echo "  --disable-x11  Disables X11, removing binary dependency."
	echo "  --enable-x11   Enables X11 support (default)."
	echo
	echo "  --disable-gl   Disables all OpenGL renderers."
	echo "  --enable-gl    Enables OpenGL, runtime loaded (default)."
	echo
	echo "e.g.: ./config.sh --platform linux --prefix /usr"
	echo "                  --sysconfdir /etc --disable-x11"
	echo "e.g.: ./config.sh --platform win32"
	echo
}

### CHOMP CONFIG.SH FLAGS #####################################################

#
# Default settings for flags (used if unspecified)
#
PLATFORM=""
PREFIX="/usr"
SYSCONFDIR="/etc"
X11="true"
OPENGL="true"

#
# User may override above settings
#
while [ "$1" != "" ]; do
	# e.g. --platform linux-static
	if [ "$1" = "--platform" ]; then
		shift
		PLATFORM="$1"
	fi

	# e.g. --prefix /usr
	if [ "$1" = "--prefix" ]; then
		shift
		PREFIX="$1"
	fi

	# e.g. --sysconfdir /etc
	if [ "$1" = "--sysconfdir" ]; then
		shift
		SYSCONFDIR="$1"
	fi

	[ "$1" = "--disable-x11" ] && X11="false"
	[ "$1" = "--enable-x11" ]  && X11="true"

	[ "$1" = "--disable-gl" ] && OPENGL="false"
	[ "$1" = "--enable-gl" ]  && OPENGL="true"

	shift
done

#
# Platform cannot be sanely omitted
#
if [ "$PLATFORM" = "" ]; then
	usage
	exit 0
fi

### PLATFORM DEFINITION #######################################################

# hack for win32
if [ "$PLATFORM" = "win32" ]; then
	echo "MINGWBASE="          > Makefile.platform
	echo                      >> Makefile.platform
	cat arch/Makefile.mingw32 >> Makefile.platform
	PLATFORM="mingw32"
else
	cp -f arch/Makefile.$PLATFORM Makefile.platform

	if [ ! -f Makefile.platform ]; then
		echo "Invalid platform selection (see arch/)."
		exit 1
	fi
fi

### SYSTEM CONFIG DIRECTORY ###################################################

if [ "$PLATFORM" != "linux" ]; then
	SYSCONFDIR="."
fi

### SUMMARY OF OPTIONS ########################################################

echo "Building for platform:   $PLATFORM"
echo "Using prefix:            $PREFIX"
echo "Using sysconfdir:        $SYSCONFDIR"
echo

### GENERATE CONFIG.H HEADER ##################################################

echo                       >> Makefile.platform
echo "# config time stuff" >> Makefile.platform
echo "PREFIX=$PREFIX"      >> Makefile.platform

echo "#define CONFDIR  \"$SYSCONFDIR/\"" > src/config.h

#
# Only Linux wants to use a system prefix hierarchy currently
#
if [ "$PLATFORM" = "linux" ]; then
	echo "#define SHAREDIR \"$PREFIX/share/megazeux/\"" >> src/config.h
	echo "#define CONFFILE \"megazeux-config\""         >> src/config.h
else
	echo "#define SHAREDIR \"./\""         >> src/config.h
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
fi

#
# Some architectures define an "install" target, and need these.
#
echo "TARGET=`grep TARGET Makefile.in | sed "s/ //g" | cut -d "=" -f 2`" \
	>> Makefile.platform
echo "SYSCONFDIR=$SYSCONFDIR" >> Makefile.platform

#
# X11 support (linked against and needs headers installed)
#
if [ "$PLATFORM" != "mingw32" -a "$PLATFORM" != "psp" ]; then
	# attempt auto-detection
	if [ "$X11" = "true" ]; then
		# try to run X
		X -version >/dev/null 2>&1

		# X queried successfully
		if [ "$?" != "0" ]; then
			echo "X11 could not be queried, disabling."
			X11="false"
		fi
	else
		echo "X11 support disabled."
	fi

	# asked for X11?
	if [ "$X11" = "true" ]; then
		echo "X11 support enabled."

		# enable the C++ bits
		echo "#define CONFIG_X11" >> src/config.h

		# figure out where X11 is prefixed
		X11=`which X`
		X11DIR=`dirname $X11`
		X11LIB="$X11DIR/../lib"

		# add a flag for linking against X11
		echo "LIBS+=-L$X11LIB -lX11" >> Makefile.platform
	fi
fi

#
# OpenGL support (not linked against GL, but needs headers installed)
#
if [ "$PLATFORM" != "psp" ]; then
	# asked for opengl?
	if [ "$OPENGL" = "true" ]; then
		echo "OpenGL support enabled."

		# enable the C++ bits
		echo "#define CONFIG_OPENGL" >> src/config.h
	else
		echo "OpenGL support disabled."
	fi
fi

echo
echo "Now type \"make\"."
