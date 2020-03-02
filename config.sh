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
	echo "  --libdir       Where libraries should be installed."
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
	echo "  darwin         Mac OS X Unix-like install"
	echo "  darwin-devel   Mac OS X running from current dir"
	echo "  darwin-dist    Mac OS X (PPC .app builds -- use Xcode for Intel)"
	echo "  psp            Experimental PSP port"
	echo "  gp2x           Experimental GP2X port"
	echo "  nds            Experimental NDS port"
	echo "  3ds            Experimental 3DS port"
	echo "  switch         Experimental Switch port"
	echo "  wii            Experimental Wii port"
	echo "  amiga          Experimental AmigaOS 4 port"
	echo "  android        Experimental Android port"
	echo "  pandora        Experimental Pandora port"
	echo "  emscripten     Experimental HTML5 (Emscripten) port"
	echo
	echo "Supported <option> values (negatives can be used):"
	echo
	echo "  --as-needed-hack        Pass --as-needed through to GNU ld."
	echo "  --enable-release        Optimize and remove debugging code."
	echo "  --enable-verbose        Build system is always verbose (V=1)."
	echo "  --optimize-size         Perform size optimizations (-Os)."
	echo "  --enable-asan           Enable AddressSanitizer for debug builds"
	echo "  --enable-msan           Enable MemorySanitizer for debug builds"
	echo "  --enable-tsan           Enable ThreadSanitizer for debug builds"
	echo "  --enable-pledge         Enable experimental OpenBSD pledge(2) support"
	echo "  --disable-datestamp     Disable adding date to version."
	echo "  --disable-editor        Disable the built-in editor."
	echo "  --disable-mzxrun        Disable generation of separate MZXRun."
	echo "  --disable-helpsys       Disable the built-in help system."
	echo "  --disable-utils         Disable compilation of utils."
	echo "  --disable-x11           Disable X11, removing binary dependency."
	echo "  --disable-software      Disable software renderer."
	echo "  --disable-softscale     Disable SDL 2 accelerated software renderer."
	echo "  --disable-gl            Disable all GL renderers."
	echo "  --disable-gl-fixed      Disable GL renderers for fixed-function h/w."
	echo "  --disable-gl-prog       Disable GL renderers for programmable h/w."
	echo "  --disable-overlay       Disable SDL 1.2 overlay renderers."
	echo "  --enable-gp2x           Enables half-res software renderer."
	echo "  --disable-screenshots   Disable the screenshot hotkey."
	echo "  --disable-xmp           Disable XMP music engine."
	echo "  --enable-modplug        Enables ModPlug music engine."
	echo "  --enable-mikmod         Enables MikMod music engine."
	echo "  --enable-openmpt        Enables OpenMPT music engine."
	echo "  --disable-rad           Disables Reality Adlib Tracker (RAD) support."
	echo "  --disable-libpng        Disable PNG screendump support."
	echo "  --disable-audio         Disable all audio (sound + music)."
	echo "  --disable-vorbis        Disable ogg/vorbis support."
	echo "  --enable-tremor         Switches out libvorbis for libvorbisidec."
	echo "  --enable-tremor-lowmem  Switches out libvorbis for libvorbisidec (lowmem branch)."
	echo "  --disable-pthread       Use SDL's threads/locking instead of pthread."
	echo "  --disable-icon          Do not try to brand executable."
	echo "  --disable-modular       Disable dynamically shared objects."
	echo "  --disable-updater       Disable built-in updater."
	echo "  --disable-network       Disable networking abilities."
	echo "  --enable-meter          Enable load/save meter display."
	echo "  --disable-sdl           Disables SDL dependencies and features."
	echo "  --enable-egl            Enables EGL backend (if SDL disabled)."
	echo "  --enable-gles           Enable hacks for OpenGL ES platforms."
	echo "  --disable-check-alloc   Disables memory allocator error handling."
	echo "  --disable-khash         Disables using khash for counter/string lookups."
	echo "  --enable-debytecode     Enable experimental 'debytecode' transform."
	echo "  --disable-libsdl2       Disable SDL 2.0 support (falls back on 1.2)."
	echo "  --enable-stdio-redirect Redirect console output to stdout.txt/stderr.txt."
	echo "  --enable-fps            Enable frames-per-second counter."
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
PREFIX_IS_SET="false"
SYSCONFDIR="/etc"
SYSCONFDIR_IS_SET="false"
GAMESDIR_IS_SET="false"
GAMESDIR_IN_PREFIX="/games"
GAMESDIR="${PREFIX}${GAMESDIR_IN_PREFIX}"
LIBDIR_IS_SET="false"
LIBDIR_IN_PREFIX="/lib"
LIBDIR="${PREFIX}${LIBDIR_IN_PREFIX}"
BINDIR_IS_SET="false"
BINDIR_IN_PREFIX="/bin"
BINDIR="${PREFIX}${BINDIR_IN_PREFIX}"
SHAREDIR_IS_SET="false"
SHAREDIR_IN_PREFIX="/share"
SHAREDIR="${PREFIX}${SHAREDIR_IN_PREFIX}"
DATE_STAMP="true"
AS_NEEDED="false"
RELEASE="false"
OPT_SIZE="false"
SANITIZER="false"
PLEDGE="false"
PLEDGE_UTILS="true"
EDITOR="true"
MZXRUN="true"
HELPSYS="true"
UTILS="true"
X11="true"
SOFTWARE="true"
SOFTSCALE="true"
GL_FIXED="true"
GL_PROGRAM="true"
OVERLAY="true"
GP2X="false"
SCREENSHOTS="true"
XMP="true"
MODPLUG="false"
MIKMOD="false"
OPENMPT="false"
REALITY="true"
LIBPNG="true"
AUDIO="true"
VORBIS="true"
PTHREAD="true"
ICON="true"
MODULAR="true"
UPDATER="true"
NETWORK="true"
VERBOSE="false"
METER="false"
SDL="true"
EGL="false"
GLES="false"
CHECK_ALLOC="true"
KHASH="true"
DEBYTECODE="false"
LIBSDL2="true"
STDIO_REDIRECT="false"
GAMECONTROLLERDB="true"
FPSCOUNTER="false"

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
		PREFIX_IS_SET="true"
		# Update other install folders to match
		if [ "$GAMESDIR_IS_SET" = "false" ]; then
			GAMESDIR="${PREFIX}${GAMESDIR_IN_PREFIX}"
		fi

		if [ "$LIBDIR_IS_SET" = "false" ]; then
			LIBDIR="${PREFIX}${LIBDIR_IN_PREFIX}"
		fi

		if [ "$BINDIR_IS_SET" = "false" ]; then
			BINDIR="${PREFIX}${BINDIR_IN_PREFIX}"
		fi

		if [ "$SHAREDIR_IS_SET" = "false" ]; then
			SHAREDIR="${PREFIX}${SHAREDIR_IN_PREFIX}"
		fi
	fi

	# e.g. --sysconfdir /etc
	if [ "$1" = "--sysconfdir" ]; then
		shift
		SYSCONFDIR="$1"
		SYSCONFDIR_IS_SET="true"
	fi

	# e.g. --gamesdir /usr/games
	if [ "$1" = "--gamesdir" ]; then
		shift
		GAMESDIR="$1"
		GAMESDIR_IS_SET="true"
	fi

	# e.g. --libdir /usr/lib
	if [ "$1" = "--libdir" ]; then
		shift
		LIBDIR="$1"
		LIBDIR_IS_SET="true"
	fi

	# e.g. --bindir /usr/bin
	if [ "$1" = "--bindir" ]; then
		shift
		BINDIR="$1"
		BINDIR_IS_SET="true"
	fi

	# e.g. --sharedir /usr/share
	if [ "$1" = "--sharedir" ]; then
		shift
		SHAREDIR="$1"
		SHAREDIR_IS_SET="true"
	fi

	[ "$1" = "--as-needed-hack" ] && AS_NEEDED="true"

	[ "$1" = "--enable-release" ]  && RELEASE="true"
	[ "$1" = "--disable-release" ] && RELEASE="false"

	[ "$1" = "--optimize-size" ]  && OPT_SIZE="true"
	[ "$1" = "--optimize-speed" ] && OPT_SIZE="false"

	[ "$1" = "--enable-asan" ]  && SANITIZER="address"
	[ "$1" = "--disable-asan" ] && SANITIZER="false"

	[ "$1" = "--enable-msan" ]  && SANITIZER="memory"
	[ "$1" = "--disable-msan" ] && SANITIZER="false"

	[ "$1" = "--enable-tsan" ] &&  SANITIZER="thread"
	[ "$1" = "--disable-tsan" ] && SANITIZER="false"

	[ "$1" = "--enable-pledge" ] &&  PLEDGE="true"  && PLEDGE_UTILS="true"
	[ "$1" = "--disable-pledge" ] && PLEDGE="false" && PLEDGE_UTILS="false"

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

	[ "$1" = "--disable-softscale" ] && SOFTSCALE="false"
	[ "$1" = "--enable-softscale" ]  && SOFTSCALE="true"

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

	[ "$1" = "--disable-screenshots" ] && SCREENSHOTS="false"
	[ "$1" = "--enable-screenshots" ]  && SCREENSHOTS="true"

	[ "$1" = "--disable-modplug" ] && MODPLUG="false"
	[ "$1" = "--enable-modplug" ]  && MODPLUG="true"

	[ "$1" = "--disable-mikmod" ] && MIKMOD="false"
	[ "$1" = "--enable-mikmod" ]  && MIKMOD="true"

	[ "$1" = "--disable-xmp" ] && XMP="false"
	[ "$1" = "--enable-xmp" ]  && XMP="true"

	[ "$1" = "--disable-openmpt" ] && OPENMPT="false"
	[ "$1" = "--enable-openmpt" ]  && OPENMPT="true"

	[ "$1" = "--disable-rad" ] && REALITY="false"
	[ "$1" = "--enable-rad" ]  && REALITY="true"

	[ "$1" = "--disable-libpng" ] && LIBPNG="false"
	[ "$1" = "--enable-libpng" ]  && LIBPNG="true"

	[ "$1" = "--disable-audio" ] && AUDIO="false"
	[ "$1" = "--enable-audio" ]  && AUDIO="true"

	[ "$1" = "--disable-vorbis" ] && VORBIS="false"
	[ "$1" = "--enable-vorbis" ]  && VORBIS="true"

	[ "$1" = "--disable-tremor" ] && VORBIS="true"
	[ "$1" = "--enable-tremor" ]  && VORBIS="tremor"

	[ "$1" = "--disable-tremor-lowmem" ] && VORBIS="true"
	[ "$1" = "--enable-tremor-lowmem" ]  && VORBIS="tremor-lowmem"

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

	[ "$1" = "--enable-gles" ]  && GLES="true"
	[ "$1" = "--disable-gles" ] && GLES="false"

	[ "$1" = "--disable-check-alloc" ] && CHECK_ALLOC="false"
	[ "$1" = "--enable-check-alloc" ]  && CHECK_ALLOC="true"

	[ "$1" = "--enable-khash" ]  && KHASH="true"
	[ "$1" = "--disable-khash" ] && KHASH="false"

	[ "$1" = "--enable-debytecode" ]  && DEBYTECODE="true"
	[ "$1" = "--disable-debytecode" ] && DEBYTECODE="false"

	[ "$1" = "--enable-libsdl2" ]  && LIBSDL2="true"
	[ "$1" = "--disable-libsdl2" ] && LIBSDL2="false"

	[ "$1" = "--enable-stdio-redirect" ]  && STDIO_REDIRECT="true"
	[ "$1" = "--disable-stdio-redirect" ] && STDIO_REDIRECT="false"

	[ "$1" = "--enable-fps" ]  && FPSCOUNTER="true"
	[ "$1" = "--disable-fps" ] && FPSCOUNTER="false"

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

rm -f platform.inc

if [ "$PLATFORM" = "win32"   -o "$PLATFORM" = "win64" \
  -o "$PLATFORM" = "mingw32" -o "$PLATFORM" = "mingw64" ]; then
	# Auto-prefix for the MSYS2 MINGW32/MINGW64 environments if a prefix wasn't
	# provided. This helps avoid errors that occur when gcc or libs exist in
	# /usr, which is used by MSYS2 for the MSYS environment.
	if [ "$PREFIX_IS_SET" = "false" -a -n "$MSYSTEM" \
	 -a "$(uname -o)" == "Msys" ]; then
		if [ "$MSYSTEM" = "MINGW32" -o "$MSYSTEM" = "MINGW64" ]; then
			[ "$PLATFORM" = "win32" ] && PREFIX="/mingw32"
			[ "$PLATFORM" = "win64" ] && PREFIX="/mingw64"
		fi
	fi

	[ "$PLATFORM" = "win32" -o "$PLATFORM" = "mingw32" ] && ARCHNAME=x86
	[ "$PLATFORM" = "win64" -o "$PLATFORM" = "mingw64" ] && ARCHNAME=x64
	[ "$PLATFORM" = "mingw32" ] && MINGWBASE=i686-w64-mingw32-
	[ "$PLATFORM" = "mingw64" ] && MINGWBASE=x86_64-w64-mingw32-
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
		"OpenBSD")
			UNIX="openbsd"
			;;
		*)
			echo "WARNING: Should define proper UNIX name here!"
			UNIX="unix"
			;;
	esac

	if [ "$MACH" = "x86_64" -o "$MACH" = "amd64" ]; then
		ARCHNAME=amd64
		#RAWLIBDIR=lib64
		# FreeBSD amd64 hack
		#[ "$UNIX" = "freebsd" ] && RAWLIBDIR=lib
	elif [ "`echo $MACH | sed 's,i.86,x86,'`" = "x86" ]; then
		ARCHNAME=x86
		#RAWLIBDIR=lib
	elif [ "`echo $MACH | sed 's,^arm.*,arm,'`" = "arm" ]; then
		ARCHNAME=arm
		#RAWLIBDIR=lib
	elif [ "$MACH" = "ppc" ]; then
		ARCHNAME=ppc
		#RAWLIBDIR=lib
	else
		ARCHNAME=$MACH
		#RAWLIBDIR=lib
		echo "WARNING: Compiling on an unsupported architecture. Add a friendly MACH to config.sh."
	fi

	echo "#define PLATFORM \"$UNIX-$ARCHNAME\"" > src/config.h
	echo "SUBPLATFORM=$UNIX-$ARCHNAME"         >> platform.inc
	echo "PLATFORM=unix"                       >> platform.inc
elif [ "$PLATFORM" = "darwin" -o "$PLATFORM" = "darwin-devel" \
 -o "$PLATFORM" = "darwin-dist" ]; then

	echo "#define PLATFORM \"darwin\""    > src/config.h
	echo "SUBPLATFORM=$PLATFORM"         >> platform.inc
	echo "PLATFORM=darwin"               >> platform.inc
else
	if [ ! -d arch/$PLATFORM ]; then
		echo "Invalid platform selection (see arch/)."
		exit 1
	fi

	echo "#define PLATFORM \"$PLATFORM\"" > src/config.h
	echo "SUBPLATFORM=$PLATFORM"         >> platform.inc
	echo "PLATFORM=$PLATFORM"            >> platform.inc
fi

echo "PREFIX:=$PREFIX" >> platform.inc

if [ "$PLATFORM" = "unix" -o "$PLATFORM" = "darwin" ]; then
	LIBDIR="${LIBDIR}/megazeux"
elif [ "$PLATFORM" = "emscripten" ]; then
	LIBDIR="/data"
else
	LIBDIR="."
fi

### SYSTEM CONFIG DIRECTORY ###################################################

if [ "$PLATFORM" = "unix" -o "$PLATFORM" = "darwin" ]; then
	: # Use default or user-defined SYSCONFDIR
elif [ "$PLATFORM" = "darwin-dist" ]; then
	SYSCONFDIR="../Resources"
elif [ "$PLATFORM" = "emscripten" ]; then
	SYSCONFDIR="/data/etc"
elif [ "$SYSCONFDIR_IS_SET" != "true" ]; then
	SYSCONFDIR="."
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
if [ "$PLATFORM" = "unix" -o "$PLATFORM" = "darwin" ]; then
	echo "#define CONFFILE \"megazeux-config\""      >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR/megazeux/\""  >> src/config.h
	echo "#define USERCONFFILE \".megazeux-config\"" >> src/config.h
elif [ "$PLATFORM" = "nds" ]; then
	SHAREDIR=/games/megazeux
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "3ds" ]; then
	SHAREDIR=/3ds/megazeux
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
elif [ "$PLATFORM" = "switch" ]; then
	SHAREDIR=/switch/megazeux
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "darwin-dist" ]; then
	SHAREDIR=../Resources
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\""           >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""            >> src/config.h
	echo "#define USERCONFFILE \".megazeux-config\"" >> src/config.h
elif [ "$PLATFORM" = "emscripten" ]; then
	SHAREDIR=/data
	GAMESDIR=/data/game
	BINDIR=/data
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
# LIBDIR is required by several platforms that support modular builds.
# Some architectures define an "install" target, and need the rest.
#
echo "SYSCONFDIR=$SYSCONFDIR" >> platform.inc
echo "GAMESDIR=$GAMESDIR"     >> platform.inc
echo "LIBDIR=$LIBDIR"         >> platform.inc
echo "BINDIR=$BINDIR"         >> platform.inc
echo "SHAREDIR=$SHAREDIR"     >> platform.inc

#
# Platform-specific libraries, or SDL?
#
if [ "$PLATFORM" = "wii" ]; then
	echo "#define CONFIG_WII" >> src/config.h
	echo "BUILD_WII=1" >> platform.inc
	LIBSDL2="false"
fi

if [ "$PLATFORM" = "3ds" -o "$PLATFORM" = "nds" ]; then
	echo "Disabling SDL ($PLATFORM)."
	SDL="false"
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
	echo " -> SOFTWARE, SOFTSCALE, OVERLAY, MIKMOD"
	SOFTWARE="false"
	SOFTSCALE="false"
	OVERLAY="false"
	MIKMOD="false"
	LIBSDL2="false"
else
	echo "#define CONFIG_SDL" >> src/config.h
	echo "BUILD_SDL=1" >> platform.inc
	EGL="false"
fi

#
# Use an EGL backend in place of SDL.
# For now, also force-enable OpenGL ES hacks, since that's most
# likely what is being used with EGL.
#
if [ "$EGL" = "true" ]; then
	echo "#define CONFIG_EGL" >> src/config.h
	echo "BUILD_EGL=1" >> platform.inc

	echo "Force-enabling OpenGL ES support (EGL)."
	GLES="true"
fi

#
# Use GLES on Android.
#
if [ "$PLATFORM" = "android" ]; then
	echo "Force-enabling OpenGL ES support (Android)."
	GLES="true"
fi

#
# We need either SDL or EGL for OpenGL
#
if [ "$SDL" = "false" -a "$EGL" = "false" ]; then
	echo "Force-disabling OpenGL (no SDL or EGL backend)."
	GL="false"
fi

#
# Force-disable features unnecessary on Emscripten.
#
if [ "$PLATFORM" = "emscripten" ]; then
	echo "Enabling Emscripten-specific hacks."
	EDITOR="false"
	SCREENSHOTS="false"
	UPDATER="false"
	UTILS="false"

	GLES="true"
	GL_FIXED="false"
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

	echo "Force-disabling hash tables on NDS."
	KHASH="false"
fi

#
# If the 3DS arch is enabled, some code has to be compile time
# enabled too.
#
if [ "$PLATFORM" = "3ds" ]; then
	echo "Enabling 3DS-specific hacks."
	echo "#define CONFIG_3DS" >> src/config.h
	echo "BUILD_3DS=1" >> platform.inc

	echo "Force-disabling software renderer on 3DS."
	echo "Building custom 3DS renderer."
	SOFTWARE="false"

	echo "Disabling utils on 3DS (silly)."
	UTILS="false"
fi

#
# If the Switch arch is enabled, some code has to be compile time
# enabled too.
#
if [ "$PLATFORM" = "switch" ]; then
	echo "Enabling Switch-specific hacks."
	echo "#define CONFIG_SWITCH" >> src/config.h
	echo "BUILD_SWITCH=1" >> platform.inc

	echo "Disabling utils on Switch (silly)."
	UTILS="false"

	echo "Force-enabling OpenGL ES support (Switch)."
	GLES="true"

	# This may or may not be totally useless for the Switch, disable it for now.
	GAMECONTROLLERDB="false"
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
# Force-disable OpenGL and overlay renderers on PSP, GP2X, 3DS, NDS and Wii
#
if [ "$PLATFORM" = "psp" -o "$PLATFORM" = "gp2x" \
  -o "$PLATFORM" = "3ds" \
  -o "$PLATFORM" = "nds" -o "$PLATFORM" = "wii" ]; then
  	echo "Force-disabling OpenGL and overlay renderers."
	GL="false"
	OVERLAY="false"
fi

#
# Force-disable the softscale renderer for SDL 1.2 (requires SDL_Renderer).
#
if [ "$SDL" = "true" -a "$LIBSDL2" = "false" -a "$SOFTSCALE" = "true" ]; then
	echo "Force-disabling softscale renderer (requires SDL 2)."
	SOFTSCALE="false"
fi

#
# Force-disable overlay renderers for SDL 2. The SDL 2 answer to SDL_Overlay
# involves planar YUV modes which don't mesh well with MZX's internal rendering.
#
if [ "$SDL" = "true" -a "$LIBSDL2" = "true" -a "$OVERLAY" = "true" ]; then
	echo "Force-disabling overlay renderers (requires SDL 1.2)."
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
	GLES="false"
fi

#
# Force-disable PNG support on platforms without screenshots or utils enabled.
# The 3DS port requires PNG for other purposes.
#
if [ "$SCREENSHOTS" = "false" -a "$UTILS" = "false" \
  -a "$LIBPNG" = "true" -a "$PLATFORM" != "3ds" ]; then
	echo "Force-disabling PNG support (screenshots and utils disabled)"
	LIBPNG="false"
fi

#
# Force-enable tremor-lowmem on GP2X
#
if [ "$PLATFORM" = "gp2x" ]; then
	echo "Force-switching ogg/vorbis to tremor-lowmem."
	VORBIS="tremor-lowmem"
fi

#
# Force-disable modplug/mikmod/openmpt if audio is disabled
#
if [ "$AUDIO" = "false" ]; then
	MODPLUG="false"
	MIKMOD="false"
	XMP="false"
	OPENMPT="false"
	REALITY="false"
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
  -o "$PLATFORM" = "3ds"  -o "$PLATFORM" = "switch" \
  -o "$PLATFORM" = "android" -o "$PLATFORM" = "emscripten" \
  -o "$PLATFORM" = "psp"  -o "$PLATFORM" = "wii" ]; then
	echo "Force-disabling modular build (nonsensical or unsupported)."
	MODULAR="false"
fi

#
# Force disable networking (unsupported platform or no editor build)
#
if [ "$EDITOR" = "false" -o "$PLATFORM" = "nds" ]; then
	echo "Force-disabling networking (unsupported platform or editor disabled)."
	NETWORK="false"
fi

#
# Force disable updater (unsupported platform)
#
if [ "$PLATFORM" != "mingw" ]; then
	echo "Force-disabling updater (unsupported platform)."
	UPDATER="false"
fi

#
# Force disable network applications (network disabled)
#
if [ "$NETWORK" = "false" ]; then
	echo "Force-disabling network-dependent features (networking disabled)"
	UPDATER="false"
fi

#
# Force disable networking (no applications enabled)
#
if [ "$NETWORK" = "true" -a "$UPDATER" = "false" ]; then
	echo "Force-disabling networking (no network-dependent features enabled)."
	NETWORK="false"
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
# Enable pledge(2) support (OpenBSD only)
#
if [ "$UNIX" = "openbsd" ]; then
	if [ "$PLEDGE" = "true" ]; then
		echo "Enabling OpenBSD pledge(2) support for main executable(s)."
		echo "#define CONFIG_PLEDGE" >> src/config.h
	else
		echo "OpenBSD pledge(2) support disabled for main executable(s)."
	fi
	if [ "$UTILS" = "true" ]; then
		if [ "$PLEDGE_UTILS" = "true" ]; then
			echo "Enabling OpenBSD pledge(2) support for utils"
			echo "#define CONFIG_PLEDGE_UTILS" >> src/config.h
		else
			echo "OpenBSD pledge(2) support disabled for utils"
		fi
	fi
else
	PLEDGE="false"
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

	if [ "$SANITIZER" != "false" ]; then
		echo "Enabling $SANITIZER sanitizer (may enable some optimizations)"
		echo "SANITIZER=$SANITIZER" >> platform.inc
	fi
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
	echo "Building utils (checkres, downver, png2smzx, hlp2txt, txt2hlp)."
	echo "BUILD_UTILS=1" >> platform.inc
	echo "#define CONFIG_UTILS" >> src/config.h
else
	echo "Disabled utils (checkres, downver, png2smzx, hlp2txt, txt2hlp)."
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
			# check if X exists
			command -v $XBIN >/dev/null 2>&1

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
	if [ "$PLATFORM" = "darwin" -o "$PLATFORM" = "darwin-devel" \
	  -o "$PLATFORM" = "darwin-dist" -o "$PLATFORM" = "gp2x" \
	  -o "$PLATFORM" = "psp" -o "$PLATFORM" = "nds" \
	  -o "$PLATFORM" = "wii" ]; then
		echo "Force-disabling icon branding (redundant)."
		ICON="false"
	fi
fi

#
# Enable OpenGL ES hacks if required.
#
if [ "$GLES" = "true" ]; then
	echo "OpenGL ES support enabled."
	echo "#define CONFIG_GLES" >> src/config.h
else
	echo "OpenGL ES support disabled."
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
# Softscale renderer (SDL 2)
#
if [ "$SOFTSCALE" = "true" ]; then
	echo "Softscale renderer enabled."
	echo "#define CONFIG_RENDER_SOFTSCALE" >> src/config.h
	echo "BUILD_RENDER_SOFTSCALE=1" >> platform.inc
else
	echo "Softscale renderer disabled."
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
if [ "$PLATFORM" = "wii" -a "$SDL" = "false" ]; then
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
# Screenshot hotkey
#
if [ "$SCREENSHOTS" = "true" ]; then
	echo "Screenshot hotkey enabled."
	echo "#define CONFIG_ENABLE_SCREENSHOTS" >> src/config.h
	echo "BUILD_ENABLE_SCREENSHOTS=1" >> platform.inc
else
	echo "Screenshot hotkey disabled."
fi

#
# GP2X needs Mikmod, other platforms can pick
# Keep the default at the bottom so it doesn't override others.
#

if [ "$MODPLUG" = "true" ]; then
	echo "Selected Modplug music engine."
	echo "#define CONFIG_AUDIO_MOD_SYSTEM" >> src/config.h
	echo "#define CONFIG_MODPLUG" >> src/config.h
	echo "BUILD_MODPLUG=1" >> platform.inc
elif [ "$MIKMOD" = "true" ]; then
	echo "Selected Mikmod music engine."
	echo "#define CONFIG_AUDIO_MOD_SYSTEM" >> src/config.h
	echo "#define CONFIG_MIKMOD" >> src/config.h
	echo "BUILD_MIKMOD=1" >> platform.inc
elif [ "$OPENMPT" = "true" ]; then
	echo "Selected OpenMPT music engine."
	echo "#define CONFIG_AUDIO_MOD_SYSTEM" >> src/config.h
	echo "#define CONFIG_OPENMPT" >> src/config.h
	echo "BUILD_OPENMPT=1" >> platform.inc
elif [ "$XMP" = "true" ]; then
	echo "Selected XMP music engine."
	echo "#define CONFIG_AUDIO_MOD_SYSTEM" >> src/config.h
	echo "#define CONFIG_XMP" >> src/config.h
	echo "BUILD_XMP=1" >> platform.inc
else
	echo "Music engine disabled."
fi

#
# Handle RAD support, if enabled
#

if [ "$REALITY" = "true" ]; then
	echo "Reality Adlib Tracker (RAD) support enabled."
	echo "#define CONFIG_REALITY" >> src/config.h
	echo "BUILD_REALITY=1" >> platform.inc
else
	echo "Reality Adlib Tracker (RAD) support disabled."
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
# Handle PNG support, if enabled
#
if [ "$LIBPNG" = "true" ]; then
	echo "PNG support enabled."
	echo "#define CONFIG_PNG" >> src/config.h
	echo "LIBPNG=1" >> platform.inc
else
	echo "PNG support disabled."
fi

#
# Handle vorbis support, if enabled
#
if [ "$VORBIS" = "true" ]; then
	echo "Using ogg/vorbis."
	echo "#define CONFIG_VORBIS" >> src/config.h
	echo "VORBIS=vorbis" >> platform.inc

elif [ "$VORBIS" = "tremor" ]; then
	echo "Using tremor in place of ogg/vorbis."
	echo "#define CONFIG_VORBIS" >> src/config.h
	echo "#define CONFIG_TREMOR" >> src/config.h
	echo "VORBIS=tremor" >> platform.inc

elif [ "$VORBIS" = "tremor-lowmem" ]; then
	echo "Using tremor (lowmem) in place of ogg/vorbis."
	echo "#define CONFIG_VORBIS" >> src/config.h
	echo "#define CONFIG_TREMOR" >> src/config.h
	echo "VORBIS=tremor-lowmem" >> platform.inc
else
	echo "Ogg/vorbis disabled."
fi

#
# Handle pthread mutexes, if enabled
#
if [ "$PTHREAD" = "true" ]; then
	echo "Using pthread for threads/locking primitives."
	echo "#define CONFIG_PTHREAD" >> src/config.h
	echo "PTHREAD=1" >> platform.inc
else
	echo "Not using pthread for threads/locking primitives."
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
	else
		#
		# Also get the (probable) icon path...
		#
		if [ "$SHAREDIR" = "." ]; then
			ICONFILE="contrib/icons/quantump.png"
		else
			ICONFILE="$SHAREDIR/icons/megazeux.png"
		fi
		echo "#define ICONFILE \"$ICONFILE\"" >> src/config.h
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
# Allow use of hash table counter/string lookups, if enabled
#
if [ "$KHASH" = "true" ]; then
	echo "khash counter/string lookup enabled."
	echo "#define CONFIG_KHASH" >> src/config.h
	echo "BUILD_KHASH=1" >> platform.inc
else
	echo "khash counter/string lookup disabled (using binary search)."
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

#
# SDL 2.0 support, if enabled
#
if [ "$LIBSDL2" = "true" ]; then
	echo "SDL 2.0 support enabled."
	echo "BUILD_LIBSDL2=1" >> platform.inc
else
	echo "SDL 2.0 support disabled."
fi

#
# stdio redirect, if enabled
#
if [ "$SDL" = "true" -a "$LIBSDL2" = "false" ]; then
	echo "Using SDL 1.x default stdio redirect behavior."
elif [ "$STDIO_REDIRECT" = "true" ]; then
	echo "Redirecting stdio to stdout.txt and stderr.txt."
	echo "#define CONFIG_STDIO_REDIRECT" >> src/config.h
else
	echo "stdio redirect disabled."
fi

#
# SDL_GameControllerDB, if enabled. This depends on SDL 2.
#
if [ "$LIBSDL2" = "true" -a "$GAMECONTROLLERDB" = "true" ]; then
	echo "SDL_GameControllerDB enabled."
	echo "#define CONFIG_GAMECONTROLLERDB" >> src/config.h
	echo "BUILD_GAMECONTROLLERDB=1" >> platform.inc
else
	echo "SDL_GameControllerDB disabled."
fi

#
# Frames-per-second counter
#
if [ "$FPSCOUNTER" = "true" ]; then
	echo "fps counter enabled."
	echo "#define CONFIG_FPS" >> src/config.h
else
	echo "fps counter disabled."
fi

#
# Pledge(2) on main executable warning
#
if [ "$PLEDGE" = "true" ]; then
	echo
	echo "  WARNING: pledge will probably: break renderer switching; crash when"
	echo "  switching to fullscreen (use fullscreen=1) or exiting when using"
	echo "  the software renderer; crash when switching to fullscreen with the"
	echo "  scaling renderers (use fullscreen=1 and/or fullscreen_windowed=1);"
	echo "  crash when using any scaling renderer with some Mesa versions."
	echo "  You've been warned!"
fi

echo
echo "Now type \"make\" (or \"gmake\" on BSD)."
