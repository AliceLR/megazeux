#!/bin/sh

### CONFIG.SH HELP TEXT #######################################################

usage() {
	echo "usage: ./config.sh --platform [platform] <--prefix [dir]> <--sysconfdir [dir]>"
	echo "                                         <--gamesdir [dir]> <--bindir [dir]>"
	echo "                                         <--sharedir [dir]> <options..>"
	echo
	echo "  --prefix       Where dependencies should be found."
	echo "  --sysconfdir   Where the config should be read from."
	echo "  --gamesdir     Where binaries should be installed."
	echo "  --bindir       Where utilities should be installed."
	echo "  --sharedir     Where resources should be installed." 
	echo
	echo "Supported [platform] values:"
	echo
	echo "  win32          Microsoft Windows (x86)"
	echo "  win64          Microsoft Windows (x64)"
	echo "  mingw32        Use MinGW32 on Linux, to build for win32"
	echo "  mingw64        Use MinGW64 on Linux, to build for win64"
	echo "  unix           Unix-like / Linux / Solaris / BSD / Embedded"
	echo "  unix-devel     As above, but for running from current dir"
	echo "  darwin         Macintosh OS X (not Classic)"
	echo "  psp            Experimental PSP port"
	echo "  gp2x           Experimental GP2X port"
	echo "  nds            Experimental NDS port"
	echo "  wii            Experimental Wii port"
	echo "  amiga          Experimental AmigaOS 4 port"
	echo "  android        Experimental Android port"
	echo "  pandora        Experimental Pandora port"
	echo
	echo "Supported <option> values (negatives can be used):"
	echo
	echo "  --as-needed-hack      Pass --as-needed through to GNU ld."
	echo "  --enable-release      Optimize and remove debugging code."
	echo "  --enable-verbose      Build system is always verbose (V=1)."
	echo "  --optimize-size       Perform size optimizations (-Os)."
	echo "  --disable-datestamp   Disable adding date to version."
	echo "  --disable-editor      Disable the built-in editor."
	echo "  --disable-mzxrun      Disable generation of separate MZXRun."
	echo "  --disable-helpsys     Disable the built-in help system."
	echo "  --disable-utils       Disable compilation of utils."
	echo "  --disable-x11         Disable X11, removing binary dependency."
	echo "  --disable-software    Disable software renderer."
	echo "  --disable-gl          Disable all GL renderers."
	echo "  --disable-gl-fixed    Disable GL renderers for fixed-function h/w."
	echo "  --disable-gl-prog     Disable GL renderers for programmable h/w."
	echo "  --disable-overlay     Disable all overlay renderers."
	echo "  --enable-gp2x         Enables half-res software renderer."
	echo "  --disable-modplug     Disable ModPlug music engine."
	echo "  --enable-mikmod       Enables MikMod music engine."
	echo "  --disable-libpng      Disable PNG screendump support."
	echo "  --disable-audio       Disable all audio (sound + music)."
	echo "  --enable-tremor       Switches out libvorbis for libtremor."
	echo "  --disable-pthread     Use SDL's locking instead of pthread."
	echo "  --disable-icon        Do not try to brand executable."
	echo "  --disable-modular     Disable dynamically shared objects."
	echo "  --disable-updater     Disable built-in updater."
	echo "  --disable-network     Disable networking abilities."
	echo "  --enable-meter        Enable load/save meter display."
	echo "  --disable-sdl         Disables SDL dependencies and features."
	echo "  --enable-egl          Enables EGL backend (if SDL disabled)."
	echo "  --disable-check-alloc Disables memory allocator error handling."
	echo "  --enable-debytecode   Enable experimental 'debytecode' transform."
	echo
	echo "e.g.: ./config.sh --platform unix --prefix /usr"
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
GAMESDIR="${PREFIX}/games"
BINDIR="${PREFIX}/bin"
SHAREDIR="${PREFIX}/share"
SYSCONFDIR_SET="false"
DATE_STAMP="true"
AS_NEEDED="false"
RELEASE="false"
OPT_SIZE="false"
EDITOR="true"
MZXRUN="true"
HELPSYS="true"
UTILS="true"
X11="true"
SOFTWARE="true"
GL_FIXED="true"
GL_PROGRAM="true"
OVERLAY="true"
GP2X="false"
MODPLUG="true"
MIKMOD="false"
LIBPNG="true"
AUDIO="true"
TREMOR="false"
PTHREAD="true"
ICON="true"
MODULAR="true"
UPDATER="true"
NETWORK="true"
VERBOSE="false"
METER="false"
SDL="true"
EGL="false"
CHECK_ALLOC="true"
DEBYTECODE="false"

#
# User may override above settings
#
while [ "$1" != "" ]; do
	# e.g. --platform unix-devel
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

	# e.g. --gamesdir /usr/games
	if [ "$1" = "--gamesdir" ]; then
		shift
		GAMESDIR="$1"
	fi

	# e.g. --bindir /usr/bin
	if [ "$1" = "--bindir" ]; then
		shift
		BINDIR="$1"
	fi

	# e.g. --sharedir /usr/share
	if [ "$1" = "--sharedir" ]; then
		shift
		SHAREDIR="$1"
	fi

	[ "$1" = "--as-needed-hack" ] && AS_NEEDED="true"

	[ "$1" = "--enable-release" ]  && RELEASE="true"
	[ "$1" = "--disable-release" ] && RELEASE="false"

	[ "$1" = "--optimize-size" ]  && OPT_SIZE="true"
	[ "$1" = "--optimize-speed" ] && OPT_SIZE="false"

	[ "$1" = "--disable-datestamp" ] && DATE_STAMP="false"
	[ "$1" = "--enable-datestamp" ]  && DATE_STAMP="true"

	[ "$1" = "--disable-editor" ] && EDITOR="false"
	[ "$1" = "--enable-editor" ]  && EDITOR="true"

	[ "$1" = "--disable-mzxrun" ] && MZXRUN="false"
	[ "$1" = "--enable-mzxrun" ]  && MZXRUN="true"

	[ "$1" = "--disable-helpsys" ] && HELPSYS="false"
	[ "$1" = "--enable-helpsys" ]  && HELPSYS="true"

	[ "$1" = "--disable-utils" ] && UTILS="false"
	[ "$1" = "--enable-utils" ] && UTILS="true"

	[ "$1" = "--disable-x11" ] && X11="false"
	[ "$1" = "--enable-x11" ]  && X11="true"

	[ "$1" = "--disable-software" ] && SOFTWARE="false"
	[ "$1" = "--enable-software" ]  && SOFTWARE="true"

	[ "$1" = "--disable-gl" ] && GL="false"
	[ "$1" = "--enable-gl" ]  && GL="true"

	[ "$1" = "--disable-gl-fixed" ] && GL_FIXED="false"
	[ "$1" = "--enable-gl-fixed" ]  && GL_FIXED="true"

	[ "$1" = "--disable-gl-prog" ] && GL_PROGRAM="false"
	[ "$1" = "--enable-gl-prog" ]  && GL_PROGRAM="true"

	[ "$1" = "--disable-overlay" ] && OVERLAY="false"
	[ "$1" = "--enable-overlay" ]  && OVERLAY="true"

	[ "$1" = "--disable-gp2x" ] && GP2X="false"
	[ "$1" = "--enable-gp2x" ]  && GP2X="true"

	[ "$1" = "--disable-modplug" ] && MODPLUG="false"
	[ "$1" = "--enable-modplug" ]  && MODPLUG="true"

	[ "$1" = "--disable-mikmod" ] && MIKMOD="false"
	[ "$1" = "--enable-mikmod" ]  && MIKMOD="true"

	[ "$1" = "--disable-libpng" ] && LIBPNG="false"
	[ "$1" = "--enable-libpng" ]  && LIBPNG="true"

	[ "$1" = "--disable-audio" ] && AUDIO="false"
	[ "$1" = "--enable-audio" ]  && AUDIO="true"

	[ "$1" = "--disable-tremor" ] && TREMOR="false"
	[ "$1" = "--enable-tremor" ]  && TREMOR="true"

	[ "$1" = "--disable-pthread" ] && PTHREAD="false"
	[ "$1" = "--enable-pthread" ]  && PTHREAD="true"

	[ "$1" = "--disable-icon" ] && ICON="false"
	[ "$1" = "--enable-icon" ]  && ICON="true"

	[ "$1" = "--disable-modular" ] && MODULAR="false"
	[ "$1" = "--enable-modular" ]  && MODULAR="true"

	[ "$1" = "--disable-updater" ] && UPDATER="false"
	[ "$1" = "--enable-updater" ]  && UPDATER="true"

	[ "$1" = "--disable-network" ] && NETWORK="false"
	[ "$1" = "--enable-network" ]  && NETWORK="true"

	[ "$1" = "--disable-verbose" ] && VERBOSE="false"
	[ "$1" = "--enable-verbose" ]  && VERBOSE="true"

	[ "$1" = "--enable-meter" ]  && METER="true"
	[ "$1" = "--disable-meter" ] && METER="false"

	[ "$1" = "--disable-sdl" ] && SDL="false"
	[ "$1" = "--enable-sdl" ]  && SDL="true"

	[ "$1" = "--enable-egl" ]  && EGL="true"
	[ "$1" = "--disable-egl" ] && EGL="false"

	[ "$1" = "--disable-check-alloc" ] && CHECK_ALLOC="false"
	[ "$1" = "--enable-check-alloc" ]  && CHECK_ALLOC="true"

	[ "$1" = "--enable-debytecode" ]  && DEBYTECODE="true"
	[ "$1" = "--disable-debytecode" ] && DEBYTECODE="false"

	if [ "$1" = "--help" ]; then
		usage
		exit 0
	fi

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

echo "PREFIX?=$PREFIX" > platform.inc

if [ "$PLATFORM" = "win32"   -o "$PLATFORM" = "win64" \
  -o "$PLATFORM" = "mingw32" -o "$PLATFORM" = "mingw64" ]; then
	[ "$PLATFORM" = "win32" -o "$PLATFORM" = "mingw32" ] && ARCHNAME=x86
	[ "$PLATFORM" = "win64" -o "$PLATFORM" = "mingw64" ] && ARCHNAME=x64
	[ "$PLATFORM" = "mingw32" ] && MINGWBASE=i586-mingw32msvc-
	[ "$PLATFORM" = "mingw64" ] && MINGWBASE=amd64-mingw32msvc-
	PLATFORM="mingw"
	echo "#define PLATFORM \"windows-$ARCHNAME\"" > src/config.h
	echo "SUBPLATFORM=windows-$ARCHNAME"         >> platform.inc
	echo "PLATFORM=$PLATFORM"                    >> platform.inc
	echo "MINGWBASE=$MINGWBASE"                  >> platform.inc
elif [ "$PLATFORM" = "unix" -o "$PLATFORM" = "unix-devel" ]; then
	OS="`uname -s`"
	MACH="`uname -m`"

	case "$OS" in
		"Linux")
			UNIX="linux"
			;;
		"FreeBSD")
			UNIX="freebsd"
			;;
		*)
			echo "WARNING: Should define proper UNIX name here!"
			UNIX="unix"
			;;
	esac

	if [ "$MACH" = "x86_64" -o "$MACH" = "amd64" ]; then
		ARCHNAME=amd64
		LIBDIR=lib64
		# FIXME: FreeBSD amd64 hack
		[ "$UNIX" = "freebsd" ] && LIBDIR=lib
		if [ "$MODULAR" = "true" ]; then
			echo "ARCH_CFLAGS+=-fPIC" >> platform.inc
			echo "ARCH_CXXFLAGS+=-fPIC" >> platform.inc
		fi
	elif [ "`echo $MACH | sed 's,i.86,x86,'`" = "x86" ]; then
		ARCHNAME=x86
		LIBDIR=lib
	elif [ "`echo $MACH | sed 's,^arm.*,arm,'`" = "arm" ]; then
		ARCHNAME=arm
		LIBDIR=lib
	else
		echo "Add a friendly MACH to config.sh."
		exit 1
	fi

	echo "#define PLATFORM \"$UNIX-$ARCHNAME\"" > src/config.h
	echo "SUBPLATFORM=$UNIX-$ARCHNAME"         >> platform.inc
	echo "PLATFORM=unix"                       >> platform.inc
else
	if [ ! -d arch/$PLATFORM ]; then
		echo "Invalid platform selection (see arch/)."
		exit 1
	fi

	echo "#define PLATFORM \"$PLATFORM\"" > src/config.h
	echo "SUBPLATFORM=$PLATFORM"         >> platform.inc
	echo "PLATFORM=$PLATFORM"            >> platform.inc
fi

if [ "$PLATFORM" = "unix" -o "$PLATFORM" = "unix-devel" ]; then
	echo "RAWLIBDIR=${LIBDIR}" >> platform.inc
fi

if [ "$PLATFORM" = "unix" ]; then
	echo "LIBDIR=\${PREFIX}/${LIBDIR}/megazeux" >> platform.inc
elif [ "$PLATFORM" = "android" ]; then
	echo "LIBDIR=/data/megazeux" >> platform.inc
else
	echo "LIBDIR=." >> platform.inc
fi

### SYSTEM CONFIG DIRECTORY ###################################################

if [ "$PLATFORM" = "darwin" ]; then
	SYSCONFDIR="../Resources"
elif [ "$PLATFORM" = "android" ]; then
	SYSCONFDIR="/data/megazeux"
elif [ "$PLATFORM" != "unix" ]; then
	if [ "$SYSCONFDIR_SET" != "true" ]; then
		SYSCONFDIR="."
	fi
fi

### SUMMARY OF OPTIONS ########################################################

echo "Building for platform:   $PLATFORM"
echo "Using prefix:            $PREFIX"
echo "Using sysconfdir:        $SYSCONFDIR"
echo

### GENERATE CONFIG.H HEADER ##################################################

. ./version.inc

#
# Set the version to build with
#

echo "#define VERSION \"$VERSION\"" >> src/config.h

if [ "$DATE_STAMP" = "true" ]; then
	echo "Stamping version with today's date."
	echo "#define VERSION_DATE \" (`date -u +%Y%m%d`)\"" >> src/config.h
else
	echo "Not stamping version with today's date."
fi

echo "#define CONFDIR \"$SYSCONFDIR/\"" >> src/config.h

#
# Some platforms may have filesystem hierarchies they need to fit into
# FIXME: SHAREDIR should be hardcoded in fewer cases
#
if [ "$PLATFORM" = "unix" ]; then
	echo "#define CONFFILE \"megazeux-config\""    >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR/megazeux/\"" >> src/config.h
elif [ "$PLATFORM" = "nds" ]; then
	SHAREDIR=/games/megazeux
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "wii" ]; then
	SHAREDIR=/apps/megazeux
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "darwin" ]; then
	SHAREDIR=../Resources
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "android" ]; then
	SHAREDIR=/data/megazeux
	GAMESDIR=/data/megazeux
	BINDIR=/data/megazeux
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
else
	SHAREDIR=.
	GAMESDIR=.
	BINDIR=.
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
fi

#
# Some architectures define an "install" target, and need these.
#
echo "SYSCONFDIR=$SYSCONFDIR" >> platform.inc
echo "GAMESDIR=$GAMESDIR"     >> platform.inc
echo "BINDIR=$BINDIR"         >> platform.inc
echo "SHAREDIR=$SHAREDIR"     >> platform.inc

#
# Platform-specific libraries, or SDL?
#
if [ "$PLATFORM" = "wii" ]; then
	echo "#define CONFIG_WII" >> src/config.h
	echo "BUILD_WII=1" >> platform.inc
fi

if [ "$PLATFORM" = "wii" ]; then
	echo "Disabling SDL (Wii)."
	SDL="false"
fi

if [ "$PLATFORM" = "nds" ]; then
	echo "Disabling SDL (NDS)."
	SDL="false"
fi

if [ "$PLATFORM" = "android" ]; then
	echo "Disabling SDL (Android), force-enabling EGL."
	SDL="false"
	EGL="true"
fi

if [ "$PLATFORM" = "pandora" ]; then
	echo "#define CONFIG_PANDORA" >> src/config.h
	echo "BUILD_PANDORA=1" >> platform.inc
fi

#
# SDL was disabled above; must also disable SDL-dependent modules
#
if [ "$SDL" = "false" ]; then
	echo "Force-disabling SDL dependent components:"
	echo " -> SOFTWARE, OVERLAY, MIKMOD"
	SOFTWARE="false"
	OVERLAY="false"
	MIKMOD="false"
else
	echo "#define CONFIG_SDL" >> src/config.h
	echo "BUILD_SDL=1" >> platform.inc
	EGL="false"
fi

#
# We have EGL support
#
if [ "$EGL" = "true" ]; then
	echo "#define CONFIG_EGL" >> src/config.h
	echo "BUILD_EGL=1" >> platform.inc
fi

#
# We need either SDL or EGL for OpenGL
#
if [ "$SDL" = "false" -a "$EGL" = "false" ]; then
	echo "Force-disabling OpenGL (no SDL or EGL support)."
	GL="false"
fi

#
# If the NDS arch is enabled, some code has to be compile time
# enabled too.
#
# Additionally, AUDIO must be disabled on NDS as it is currently
# broken (please fix this!).
#
if [ "$PLATFORM" = "nds" ]; then
	echo "Enabling NDS-specific hacks."
	echo "#define CONFIG_NDS" >> src/config.h
	echo "BUILD_NDS=1" >> platform.inc

	echo "Force-disabling audio on NDS (fixme)."
	AUDIO="false"

	echo "Force-disabling software renderer on NDS."
	echo "Building custom NDS renderer."
	SOFTWARE="false"
fi

#
# If the PSP arch is enabled, some code has to be compile time
# enabled too.
#
if [ "$PLATFORM" = "psp" ]; then
	echo "Enabling PSP-specific hacks."
	echo "#define CONFIG_PSP" >> src/config.h
	echo "BUILD_PSP=1" >> platform.inc
fi

#
# If the GP2X arch is enabled, some code has to be compile time
# enabled too.
#
# Additionally, the software renderer is replaced with the gp2x
# renderer on this platform.
#
if [ "$PLATFORM" = "gp2x" ]; then
	echo "Enabling GP2X-specific hacks."
	echo "#define CONFIG_GP2X" >> src/config.h
	echo "BUILD_GP2X=1" >> platform.inc

	echo "Force disabling software renderer on GP2X."
	SOFTWARE="false"

	echo "Force-enabling GP2X 320x240 renderer."
	GP2X="true"

	echo "Force-disabling Modplug audio."
	MODPLUG="false"
fi

#
# Force-disable OpenGL and overlay renderers on PSP, GP2X and NDS
#
if [ "$PLATFORM" = "psp" -o "$PLATFORM" = "gp2x" \
  -o "$PLATFORM" = "nds" -o "$PLATFORM" = "wii" ]; then
  	echo "Force-disabling OpenGL and overlay renderers."
	GL="false"
	OVERLAY="false"
fi

#
# Must have at least one OpenGL renderer enabled
#
[ "$GL_FIXED" = "false" -a "$GL_PROGRAM" = "false" ] && GL="false"

#
# If OpenGL is globally disabled, disable all renderers
#
if [ "$GL" = "false" ]; then
	echo "Force-disabling OpenGL."
	GL_FIXED="false"
	GL_PROGRAM="false"
fi

#
# Force-enable tremor on PSP/GP2X
#
if [ "$PLATFORM" = "psp" -o "$PLATFORM" = "gp2x" \
  -o "$PLATFORM" = "android" ]; then
	echo "Force-switching ogg/vorbis to tremor."
	TREMOR="true"
fi

#
# Force-disable modplug/mikmod if audio is disabled
#
if [ "$AUDIO" = "false" ]; then
	MODPLUG="false"
	MIKMOD="false"
fi

#
# Force-disable pthread on non-POSIX platforms
#
if [ "$PLATFORM" != "unix" -a "$PLATFORM" != "unix-devel" \
  -a "$PLATFORM" != "gp2x" -a "$PLATFORM" != "android" ]; then
	echo "Force-disabling pthread on non-POSIX platforms."
	PTHREAD="false"
fi

#
# Force disable modular DSOs.
#
if [ "$PLATFORM" = "gp2x" -o "$PLATFORM" = "nds" \
  -o "$PLATFORM" = "psp"  -o "$PLATFORM" = "wii" ]; then
	echo "Force-disabling modular build (nonsensical or unsupported)."
	MODULAR="false"
fi

#
# Force disable networking.
#
if [ "$EDITOR" = "false" -o "$PLATFORM" = "unix" -o "$PLATFORM" = "psp" \
  -o "$PLATFORM" = "nds" -o "$PLATFORM" = "wii" ]; then
	echo "Force-disabling networking (nonsensical or unsupported)."
	NETWORK="false"
fi

#
# Force disable updater.
#
if [ "$UPDATER" = "true" -a "$NETWORK" = "false" ]; then
	echo "Force-disabling updater (networking disabled)."
	UPDATER="false"
fi

#
# Force disable utils.
#
if [ "$DEBYTECODE" = "true" ]; then
	echo "Force-disabling utils (debytecode)."
	UTILS="false"
fi

#
# As GNU ld supports recursive dylib dependency tracking, we don't need to
# explicitly link to as many libraries as the authors would have us provide.
#
# Instead, pass the linker --as-needed to discard libraries explicitly
# passed through but not directly used. Fixes warnings with the Debian
# packaging infrastructure.
#
if [ "$AS_NEEDED" = "true" ]; then
	echo "Assuming GNU ld and passing --as-needed through."
	echo "LDFLAGS+=-Wl,--as-needed" >> platform.inc
fi

#
# Users may enable release mode
#
if [ "$RELEASE" = "true" ]; then
	#
	# Users may want size optimizations
	#
	if [ "$OPT_SIZE" = "true" ]; then
		echo "Optimizing for size."
		echo "OPTIMIZE_CFLAGS=-Os" >> platform.inc
	else
		echo "Optimizing for speed."
	fi
else
	echo "Disabling optimization, debug enabled."
	echo "DEBUG=1" >> platform.inc
fi

#
# User may disable the built-in editor
#
if [ "$EDITOR" = "true" ]; then
	echo "Built-in editor enabled."
	echo "BUILD_EDITOR=1" >> platform.inc
	echo "#define CONFIG_EDITOR" >> src/config.h
else
	echo "Built-in editor disabled."
fi

#
# User may disable `MZXRun' component
#
if [ "$MZXRUN" = "true" ]; then
	echo "Building MZXRun executable."
	echo "BUILD_MZXRUN=1" >> platform.inc
else
	echo "Not building MZXRun executable."
fi

#
# User may disable the built-in help system.
#
if [ "$HELPSYS" = "true" ]; then
	echo "Built-in help system enabled."
	echo "BUILD_HELPSYS=1" >> platform.inc
	echo "#define CONFIG_HELPSYS" >> src/config.h
else
	echo "Built-in help system disabled."
fi

#
# User may want to compile utils (checkres, downver, txt2hlp)
#
if [ "$UTILS" = "true" ]; then
	echo "Building utils (checkres, downver, hlp2txt, txt2hlp)."
	echo "BUILD_UTILS=1" >> platform.inc
	echo "#define CONFIG_UTILS" >> src/config.h
else
	echo "Disabled utils (checkres, downver, hlp2txt, txt2hlp)."
fi

#
# X11 support (linked against and needs headers installed)
#
if [ "$PLATFORM" = "unix" -o "$PLATFORM" = "unix-devel" \
  -o "$PLATFORM" = "pandora" ]; then
	#
	# Confirm the user's selection of X11, if they enabled it
	#
	if [ "$X11" = "true" ]; then
		for XBIN in X Xorg; do
			# try to run X
			$XBIN -version >/dev/null 2>&1

			# X/Xorg queried successfully
			[ "$?" = "0" ] && break
		done

		if [ "$?" != "0" ]; then
			echo "Force-disabling X11 (could not be queried)."
			X11="false"
		fi
	fi

	if [ "$ICON" = "true" -a "$X11" = "false" ]; then
		echo "Force-disabling icon branding (X11 disabled)."
		ICON="false"
	fi
else
	echo "Force-disabling X11 (unsupported platform)."
	X11="false"
fi

if [ "$X11" = "true" ]; then
	echo "X11 support enabled."

	# enable the C++ bits
	echo "#define CONFIG_X11" >> src/config.h

	# figure out where X11 is prefixed
	X11PATH=`which $XBIN`
	X11DIR=`dirname $X11PATH`

	# pass this along to the build system
	echo "X11DIR=${X11DIR}" >> platform.inc
fi

#
# Force disable icon branding.
#
if [ "$ICON" = "true" ]; then
	if [ "$PLATFORM" = "darwin" -o "$PLATFORM" = "gp2x" \
	  -o "$PLATFORM" = "psp" -o "$PLATFORM" = "nds" \
	  -o "$PLATFORM" = "wii" ]; then
		echo "Force-disabling icon branding (redundant)."
		ICON="false"
	fi
fi

#
# Software renderer
#
if [ "$SOFTWARE" = "true" ]; then
	echo "Software renderer enabled."
	echo "#define CONFIG_RENDER_SOFT" >> src/config.h
	echo "BUILD_RENDER_SOFT=1" >> platform.inc
else
	echo "Software renderer disabled."
fi

#
# Fixed-function H/W OpenGL renderers
#
if [ "$GL_FIXED" = "true" ]; then
	echo "Fixed-function H/W OpenGL renderers enabled."
	echo "#define CONFIG_RENDER_GL_FIXED" >> src/config.h
	echo "BUILD_RENDER_GL_FIXED=1" >> platform.inc
else
	echo "Fixed-function OpenGL renderers disabled."
fi

#
# Programmable H/W OpenGL renderers
#
if [ "$GL_PROGRAM" = "true" ]; then
	echo "Programmable H/W OpenGL renderer enabled."
	echo "#define CONFIG_RENDER_GL_PROGRAM" >> src/config.h
	echo "BUILD_RENDER_GL_PROGRAM=1" >> platform.inc
else
	echo "Programmable H/W OpenGL renderer disabled."
fi

#
# Overlay renderers
#
if [ "$OVERLAY" = "true" ]; then
	echo "Overlay renderers enabled."
	echo "#define CONFIG_RENDER_YUV" >> src/config.h
	echo "BUILD_RENDER_YUV=1" >> platform.inc
else
	echo "Overlay renderers disabled."
fi

#
# GX renderer (Wii and GameCube)
#
if [ "$PLATFORM" = "wii" ]; then
	echo "Building custom GX renderer."
	echo "#define CONFIG_RENDER_GX" >> src/config.h
	echo "BUILD_RENDER_GX=1" >> platform.inc
fi

#
# GP2X renderer
#
if [ "$GP2X" = "true" ]; then
	echo "GP2X half-res renderer enabled."
	echo "#define CONFIG_RENDER_GP2X" >> src/config.h
	echo "BUILD_RENDER_GP2X=1" >> platform.inc
else
	echo "GP2X half-res renderer disabled."
fi

#
# GP2X needs Mikmod, other platforms can pick
#
if [ "$MODPLUG" = "true" ]; then
	echo "Selected Modplug music engine."
	echo "#define CONFIG_MODPLUG" >> src/config.h
	echo "BUILD_MODPLUG=1" >> platform.inc
elif [ "$MIKMOD" = "true" ]; then
	echo "Selected Mikmod music engine."
	echo "#define CONFIG_MIKMOD" >> src/config.h
	echo "BUILD_MIKMOD=1" >> platform.inc
else
	echo "Music engine disabled."
fi

#
# Handle audio subsystem, if enabled
#
if [ "$AUDIO" = "true" ]; then
	echo "Audio subsystem enabled."
	echo "#define CONFIG_AUDIO" >> src/config.h
	echo "BUILD_AUDIO=1" >> platform.inc
else
	echo "Audio subsystem disabled."
fi

#
# Handle PNG screendump support, if enabled
#
if [ "$LIBPNG" = "true" ]; then
	echo "PNG screendump support enabled."
	echo "#define CONFIG_PNG" >> src/config.h
	echo "LIBPNG=1" >> platform.inc
else
	echo "PNG screendump support disabled."
fi

#
# Handle libtremor support, if enabled
#
if [ "$TREMOR" = "true" ]; then
	echo "Using tremor in place of ogg/vorbis."
	echo "#define CONFIG_TREMOR" >> src/config.h
	echo "TREMOR=1" >> platform.inc
else
	echo "Not using tremor in place of ogg/vorbis."
fi

#
# Handle pthread mutexes, if enabled
#
if [ "$PTHREAD" = "true" ]; then
	echo "Using pthread for locking primitives."
	echo "#define CONFIG_PTHREAD_MUTEXES" >> src/config.h
	echo "PTHREAD=1" >> platform.inc
else
	echo "Not using pthread for locking primitives."
fi

#
# Handle icon branding, if enabled
#
if [ "$ICON" = "true" ]; then
	echo "Icon branding enabled."
	echo "#define CONFIG_ICON" >> src/config.h

	#
	# On Windows we want the icons to be compiled in
	#
	if [ "$PLATFORM" = "mingw" ]; then
		echo "EMBED_ICONS=1" >> platform.inc
	fi
else
	echo "Icon branding disabled."
fi

#
# Inform build system of modular build, if appropriate
#
if [ "$MODULAR" = "true" ]; then
	echo "Modular build enabled."
	echo "BUILD_MODULAR=1" >> platform.inc
	echo "#define CONFIG_MODULAR" >> src/config.h

else
	echo "Modular build disabled."
fi

#
# Handle networking, if enabled
#
if [ "$NETWORK" = "true" ]; then
        echo "Networking enabled."
	echo "#define CONFIG_NETWORK" >> src/config.h
	echo "BUILD_NETWORK=1" >> platform.inc
else
	echo "Networking disabled."
fi

#
# Handle built-in updater, if enabled
#
if [ "$UPDATER" = "true" ]; then
	echo "Built-in updater enabled."
	echo "#define CONFIG_UPDATER" >> src/config.h
	echo "BUILD_UPDATER=1" >> platform.inc
else
	echo "Built-in updater disabled."
fi

#
# Some users may prefer the build system to ALWAYS be verbose
#
if [ "$VERBOSE" = "true" ]; then
	echo "Verbose build system (always)."
	echo "export V=1" >> platform.inc
fi

#
# Users may enable the load/save meter display.
#
if [ "$METER" = "true" ]; then
	echo "Load/save meter display enabled."
	echo "#define CONFIG_LOADSAVE_METER" >> src/config.h
else
	echo "Load/save meter display disabled."
fi

#
# Memory allocation error handling, if enabled
#
if [ "$CHECK_ALLOC" = "true" ]; then
	echo "Memory allocation error checking enabled."
	echo "#define CONFIG_CHECK_ALLOC" >> src/config.h
else
	echo "Memory allocation error checking disabled."
fi

#
# Experimental 'debytecode' transformation, if enabled
#
if [ "$DEBYTECODE" = "true" ]; then
	echo "Experimental 'debytecode' transform enabled."
	echo "#define CONFIG_DEBYTECODE" >> src/config.h
	echo "BUILD_DEBYTECODE=1" >> platform.inc
else
	echo "Experimental 'debytecode' transform disabled."
fi

echo
echo "Now type \"make\" (or \"gmake\" on BSD)."
