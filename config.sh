#!/bin/sh

### CONFIG.SH HELP TEXT #######################################################

usage() {
	echo "usage: ./config.sh --platform [platform] <--prefix prefix>"
	echo "                   <--sysconfdir sysconfdir> <option..>"
	echo
	echo " <prefix>        Where MegaZeux's dependencies should be found." 
	echo " <sysconfdir>    Where MegaZeux's config should be read from."
	echo
	echo "Supported [platform] values:"
	echo
	echo "  win32          Microsoft Windows"
	echo "  linux          Linux / BSD / Embedded"
	echo "  macos          Macintosh OS X (not Classic)"
	echo "  linux-static   Linux (statically linked)"
	echo "  psp            Experimental PSP port"
	echo "  gp2x           Experimental GP2X port"
	echo "  nds            Experimental NDS port"
	echo "  mingw32        Use MinGW32 on Linux, to build for win32"
	echo "  mingw64        Use MinGW64 on Linux, to build for win64"
	echo
	echo "Supported <option> values:"
	echo
	echo "  --enable-host-tools  Use 'cc' to build utils (txt2hlp)."
	echo "  --disable-host-tools Use the default compiler (default)."
	echo
	echo "  --disable-utils      Disables compilation of utils (txt2hlp)."
	echo "  --enable-utils       Enables compilation of utils (default)."
	echo
	echo "  --disable-x11        Disables X11, removing binary dependency."
	echo "  --enable-x11         Enables X11 support (default)."
	echo
	echo "  --disable-software   Disable software renderer."
	echo "  --enable-software    Enable software renderer (default)."
	echo
	echo "  --disable-gl         Disables all OpenGL renderers."
	echo "  --enable-gl          Enables OpenGL, runtime loaded (default)."
	echo
	echo "  --disable-overlay    Disables all overlay renderers."
	echo "  --enable-overlay     Enables all overlay renderers. (default)"
	echo
	echo "  --enable-gp2x        Enables half-width software renderer."
	echo "  --disable-gp2x       Disables half-width software renderer (default)."
	echo
	echo "  --disable-modplug    Disables ModPlug music engine."
	echo "  --enable-modplug     Enables ModPlug music engine (default)."
	echo
	echo "  --enable-mikmod      Enables MikMod music engine."
	echo "  --disable-mikmod     Disables MikMod music engine (default)."
	echo
	echo "  --disable-libpng     Disables PNG screendump support."
	echo "  --enable-libpng      Enables PNG screendump support (default)."
	echo
	echo "  --disable-audio      Disables all audio (sound + music)."
	echo "  --enable-audio       Audio (sound + music) is enabled (default)."
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
SYSCONFDIR_SET="false"
HOST_TOOLS="false"
UTILS="true"
X11="true"
SOFTWARE="true"
OPENGL="true"
OVERLAY="true"
GP2X="false"
MODPLUG="true"
MIKMOD="false"
LIBPNG="true"
AUDIO="true"

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
		SYSCONFDIR_SET="true"
	fi

	[ "$1" = "--disable-host-tools" ] && HOST_TOOLS="false"
	[ "$1" = "--enable-host-tools" ] && HOST_TOOLS="true"

	[ "$1" = "--disable-utils" ] && UTILS="false"
	[ "$1" = "--enable-utils" ] && UTILS="true"

	[ "$1" = "--disable-x11" ] && X11="false"
	[ "$1" = "--enable-x11" ]  && X11="true"

	[ "$1" = "--disable-software" ] && SOFTWARE="false"
	[ "$1" = "--enable-software" ]  && SOFTWARE="true"

	[ "$1" = "--disable-gl" ] && OPENGL="false"
	[ "$1" = "--enable-gl" ]  && OPENGL="true"

	[ "$1" = "--disable-overlay" ] && OVERLAY="false"
	[ "$1" = "--enable-overlay" ]  && OVERLAY="true"

	[ "$1" = "--disable-gp2x" ] && GP2X="false"
	[ "$1" = "--enable-gp2x" ]  && GP2X="true"

	[ "$1" = "--disable-modplug" ] && MODPLUG="false"
	[ "$1" = "--enable-modplug" ]  && MODPLUG="true"

	[ "$1" = "--disable-mikmod" ] && MIKMOD="false"
	[ "$1" = "--enable-mikmod" ]  && MIKMOD="true"

	[ "$1" = "--disable-libpng" ] && LIBPNG="false"
	[ "$1" = "--enable-libpng" ] && LIBPNG="true"

	[ "$1" = "--disable-audio" ] && AUDIO="false"
	[ "$1" = "--enable-audio" ] && AUDIO="true"

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

if [ "$PLATFORM" != "linux" -a "$SYSCONFDIR_SET" != "true" ]; then
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
elif [ "$PLATFORM" = "nds" ]; then
	echo "#define SHAREDIR \"/games/megazeux/\"" >>src/config.h
	echo "#define CONFFILE \"config.txt\""       >>src/config.h
else
	echo "#define SHAREDIR \"./\""         >> src/config.h
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
fi

#
# Some architectures define an "install" target, and need these.
#
echo "TARGET=`grep TARGET Makefile | head -n1 | \
              sed "s/ //g" | cut -d "=" -f 2`" \
	>> Makefile.platform
echo "SYSCONFDIR=$SYSCONFDIR" >> Makefile.platform

#
# User may not want to use her cross compiler for tools
#
if [ "$HOST_TOOLS" = "true" ]; then
	echo "Using host's compiler for tools."
	echo "NATIVE_TOOLS=1" >> Makefile.platform
	echo "HOST_CC = cc" >> Makefile.platform
else
	echo "Using default compiler for tools."
	echo "NATIVE_TOOLS=0" >> Makefile.platform
	echo "HOST_CC := \${CC}" >> Makefile.platform
fi

#
# User may want to compile utils (txt2hlp)
#
if [ "$UTILS" = "true" ]; then
	echo "Building utils (txt2hlp)."
	echo "BUILD_UTILS=1" >> Makefile.platform
else
	echo "Disabled utils (txt2hlp)."
	echo "BUILD_UTILS=0" >> Makefile.platform
fi

#
# X11 support (linked against and needs headers installed)
#
if [ "$PLATFORM" = "linux" -o "$PLATFORM" = "linux-static" \
  -o "$PLATFORM" = "macos" ]; then
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
		echo "mzx_ldflags += -L$X11LIB -lX11" >> Makefile.platform
	fi
else
	echo "X11 support disabled (unsupported platform)."
fi

#
# Force-enable software on non-NDS/GP2X platforms
#
if [ "$PLATFORM" != "nds" -a "$PLATFORM" != "gp2x" ]; then
	echo "Force-enabling software renderer."
	SOFTWARE="true"
fi

#
# Force-enable GP2X renderer on GP2X
#
if [ "$PLATFORM" = "gp2x" ]; then
	echo "Force-enabling GP2X half-width renderer."
	GP2X="true"
fi

#
# Force-disable OpenGL and overlay renderers on PSP, GP2X and NDS
#
if [ "$PLATFORM" = "psp" -o "$PLATFORM" = "gp2x" \
  -o "$PLATFORM" = "nds" ]; then
  	echo "Force-disabling OpenGL and overlay renderers."
	OPENGL="false"
	OVERLAY="false"
fi

#
# Force-disable modplug/mikmod if audio is disabled
#
if [ "$AUDIO" = "false" ]; then
	MODPLUG="false"
	MIKMOD="false"
fi

#
# Software renderer
#
if [ "$SOFTWARE" = "true" ]; then
	echo "Software renderer enabled."
	echo "#define CONFIG_SOFTWARE" >> src/config.h
	echo "BUILD_SOFTWARE=1" >> Makefile.platform
else
	echo "Software renderer disabled."
fi

#
# OpenGL renderers (not linked against GL, but needs headers installed)
#
if [ "$OPENGL" = "true" ]; then
	echo "OpenGL renderers enabled."
	echo "#define CONFIG_OPENGL" >> src/config.h
	echo "BUILD_OPENGL=1" >> Makefile.platform
else
	echo "OpenGL renderers disabled."
fi

#
# Overlay renderers
#
if [ "$OVERLAY" = "true" ]; then
	echo "Overlay renderers enabled."
	echo "#define CONFIG_OVERLAY" >> src/config.h
	echo "BUILD_OVERLAY=1" >> Makefile.platform
else
	echo "Overlay renderers disabled."
fi

#
# GP2X renderer
#
if [ "$GP2X" = "true" ]; then
	echo "GP2X half-width renderer enabled."
	echo "#define CONFIG_GP2X" >> src/config.h
	echo "BUILD_GP2X=1" >> Makefile.platform
else
	echo "GP2X half-width renderer disabled."
fi

#
# GP2X needs Mikmod, other platforms can pick
#
if [ "$MODPLUG" = "true" -a "$PLATFORM" != "gp2x" ]; then
	echo "Selected Modplug music engine."
	echo "#define CONFIG_MODPLUG" >> src/config.h
	echo "BUILD_MODPLUG=1" >> Makefile.platform
else
	# disable building local copy of ModPlug in either case
	echo "BUILD_MODPLUG=0" >> Makefile.platform

	if [ "$MIKMOD" = "true" ]; then
		echo "Selected Mikmod music engine."
		echo "#define CONFIG_MIKMOD" >> src/config.h
		echo "BUILD_MIKMOD=1" >> Makefile.platform
	else
		echo "Music engine disabled."
	fi
fi

#
# Handle audio subsystem, if enabled
#
if [ "$AUDIO" = "true" ]; then
	echo "Audio subsystem enabled."
	echo "#define CONFIG_AUDIO" >> src/config.h
	echo "BUILD_AUDIO=1" >> Makefile.platform
else
	echo "Audio subsystem disabled."
	echo "BUILD_AUDIO=0" >> Makefile.platform
fi

#
# Handle PNG screendump support, if enabled
#
if [ "$LIBPNG" = "true" ]; then
	echo "PNG screendump support enabled."
	echo "#define CONFIG_PNG" >> src/config.h
	echo "LIBPNG=1" >> Makefile.platform
else
	echo "PNG screendump support disabled."
	echo "LIBPNG=0" >> Makefile.platform
fi

echo
echo "Now type \"make\"."
