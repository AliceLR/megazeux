#!/bin/sh

### CONFIG.SH HELP TEXT #######################################################

usage() {
	echo "usage: ./config.sh --platform [platform] <--prefix [dir]> <--sysconfdir [dir]>"
	echo "                                         <--gamesdir [dir]> <--bindir [dir]>"
	echo "                                         <--sharedir [dir]> <--licensedir [dir]>"
	echo "                                         <options..>"
	echo
	echo "  --prefix       Where dependencies should be found."
	echo "  --sysconfdir   Where the config should be read from."
	echo "  --gamesdir     Where binaries should be installed."
	echo "  --libdir       Where libraries should be installed."
	echo "  --bindir       Where utilities should be installed."
	echo "  --sharedir     Where resources should be installed."
	echo "  --licensedir   Where licenses should be installed."
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
	echo "  wiiu           Experimental Wii U port"
	echo "  dreamcast      Experimental Dreamcast port"
	echo "  amiga          Experimental AmigaOS 4 port"
	echo "  android        Experimental Android port"
	echo "  pandora        Experimental Pandora port"
	echo "  emscripten     Experimental HTML5 (Emscripten) port"
	echo "  djgpp          Experimental DOS port"
	echo
	echo "Supported <option> values (negatives can be used):"
	echo
	echo "Optimization and debug options:"
	echo "  --as-needed-hack          Pass --as-needed through to GNU ld."
	echo "  --enable-release          Optimize and remove debugging code."
	echo "  --enable-verbose          Build system is always verbose (V=1)."
	echo "  --optimize-size           Perform size optimizations (-Os)."
	echo "  --enable-asan             Enable AddressSanitizer for debug builds."
	echo "  --enable-msan             Enable MemorySanitizer for debug builds."
	echo "  --enable-tsan             Enable ThreadSanitizer for debug builds."
	echo "  --enable-ubsan            Enable UndefinedBehaviorSanitizer for debug builds."
	echo "  --enable-analyzer         Enable -fanalyzer (requires GCC 10+)."
	echo "  --enable-trace            Enable trace logging for debug builds."
	echo "  --enable-stdio-redirect   Redirect console logging to stdout.txt/stderr.txt."
	echo "  --disable-stack-protector Disable stack protector safety checks."
	echo
	echo "Platform-dependent options:"
	echo "  --disable-x11             Disable X11, removing binary dependency."
	echo "  --disable-pthread         Use SDL's threads/locking instead of pthread."
	echo "  --disable-icon            Do not try to brand executable."
	echo "  --disable-modular         Disable dynamically shared objects."
	echo "  --disable-libsdl2         Disable SDL 2.0 support (falls back on 1.2)."
	echo "  --disable-sdl             Disables SDL dependencies and features."
	echo "  --enable-egl              Enables EGL backend (if SDL disabled)."
	echo "  --enable-pledge           Enable experimental OpenBSD pledge(2) support"
	echo
	echo "General options:"
	echo "  --disable-datestamp       Disable adding date to version."
	echo "  --disable-editor          Disable the built-in editor."
	echo "  --disable-mzxrun          Disable generation of separate MZXRun."
	echo "  --disable-helpsys         Disable the built-in help system."
	echo "  --disable-utils           Disable compilation of utils."
	echo "  --disable-check-alloc     Disables memory allocator error handling."
	echo "  --disable-counter-hash    Disables hash tables for counter/string lookups."
	echo "  --disable-vfs             Disables MZX's built-in virtual filesystem."
	echo "  --enable-extram           Enable board memory compression and storage hacks."
	echo "  --enable-meter            Enable load/save meter display."
	echo "  --enable-debytecode       Enable experimental 'debytecode' transform."
	echo
	echo "Graphics options:"
	echo "  --enable-gles             Enable hacks for OpenGL ES platforms."
	echo "  --disable-software        Disable software renderer."
	echo "  --disable-softscale       Disable SDL 2 accelerated software renderer."
	echo "  --disable-gl              Disable all GL renderers."
	echo "  --disable-gl-fixed        Disable GL renderers for fixed-function h/w."
	echo "  --disable-gl-prog         Disable GL renderers for programmable h/w."
	echo "  --disable-overlay         Disable SDL 1.2 overlay renderers."
	echo "  --enable-gp2x             Enables half-res software renderer."
	echo "  --disable-dos-svga        On the DOS platform, disable SVGA software renderer."
	echo "  --disable-libpng          Disable PNG screendump support."
	echo "  --disable-screenshots     Disable the screenshot hotkey."
	echo "  --enable-fps              Enable frames-per-second counter."
	echo
	echo "Audio options:"
	echo "  --disable-audio           Disable all audio (sound + music)."
	echo "  --disable-xmp             Disable XMP music engine."
	echo "  --enable-modplug          Enables ModPlug music engine."
	echo "  --enable-mikmod           Enables MikMod music engine."
	echo "  --enable-openmpt          Enables OpenMPT music engine."
	echo "  --disable-rad             Disables Reality Adlib Tracker (RAD) support."
	echo "  --disable-vorbis          Disable ogg/vorbis support."
	echo "  --enable-tremor           Use libvorbisidec instead of libvorbis."
	echo "  --enable-tremor-lowmem    Use libvorbisidec (lowmem) instead of libvorbis."
	echo
	echo "Network options:"
	echo "  --disable-network         Disable networking abilities."
	echo "  --disable-updater         Disable built-in updater."
	echo "  --disable-getaddrinfo     Disable getaddrinfo for name resolution."
	echo "  --disable-poll            Disable poll for socket monitoring."
	echo "  --disable-ipv6            Disable IPv6 support."
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
LICENSEDIR_IS_SET="false"
LICENSEDIR_IN_PREFIX="/share/doc"
LICENSEDIR="${PREFIX}${LICENSEDIR_IN_PREFIX}"
DATE_STAMP="true"
AS_NEEDED="false"
RELEASE="false"
OPT_SIZE="false"
SANITIZER="false"
ANALYZER="false"
STACK_PROTECTOR="true"
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
GETADDRINFO="true"
POLL="true"
IPV6="true"
VERBOSE="false"
EXTRAM="false"
METER="false"
SDL="true"
EGL="false"
GLES="false"
CHECK_ALLOC="true"
COUNTER_HASH="true"
DEBYTECODE="false"
LIBSDL2="true"
TRACE_LOGGING="false"
STDIO_REDIRECT="false"
GAMECONTROLLERDB="true"
FPSCOUNTER="false"
LAYER_RENDERING="true"
DOS_SVGA="true"
VFS="true"

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

		if [ "$LICENSEDIR_IS_SET" = "false" ]; then
			LICENSEDIR="${PREFIX}${LICENSEDIR_IN_PREFIX}"
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

	# e.g. --licensedir /usr/share/license
	if [ "$1" = "--licensedir" ]; then
		shift
		LICENSEDIR="$1"
		LICENSEDIR_IS_SET="true"
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

	[ "$1" = "--enable-ubsan" ] &&  SANITIZER="undefined"
	[ "$1" = "--disable-ubsan" ] && SANITIZER="false"

	[ "$1" = "--enable-analyzer" ] &&  ANALYZER="true"
	[ "$1" = "--disable-analyzer" ] && ANALYZER="false"

	[ "$1" = "--enable-stack-protector" ]  && STACK_PROTECTOR="true"
	[ "$1" = "--disable-stack-protector" ] && STACK_PROTECTOR="false"

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

	[ "$1" = "--disable-getaddrinfo" ] && GETADDRINFO="false"
	[ "$1" = "--enable-getaddrinfo" ]  && GETADDRINFO="true"

	[ "$1" = "--disable-poll" ] && POLL="false"
	[ "$1" = "--enable-poll" ]  && POLL="true"

	[ "$1" = "--disable-ipv6" ] && IPV6="false"
	[ "$1" = "--enable-ipv6" ]  && IPV6="true"

	[ "$1" = "--disable-verbose" ] && VERBOSE="false"
	[ "$1" = "--enable-verbose" ]  && VERBOSE="true"

	[ "$1" = "--enable-extram" ]  && EXTRAM="true"
	[ "$1" = "--disable-extram" ] && EXTRAM="false"

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

	[ "$1" = "--enable-counter-hash" ]  && COUNTER_HASH="true"
	[ "$1" = "--disable-counter-hash" ] && COUNTER_HASH="false"

	[ "$1" = "--enable-vfs" ]  && VFS="true"
	[ "$1" = "--disable-vfs" ] && VFS="false"

	[ "$1" = "--enable-debytecode" ]  && DEBYTECODE="true"
	[ "$1" = "--disable-debytecode" ] && DEBYTECODE="false"

	[ "$1" = "--enable-libsdl2" ]  && LIBSDL2="true"
	[ "$1" = "--disable-libsdl2" ] && LIBSDL2="false"

	[ "$1" = "--enable-trace" ]  && TRACE_LOGGING="true"
	[ "$1" = "--disable-trace" ] && TRACE_LOGGING="false"

	[ "$1" = "--enable-stdio-redirect" ]  && STDIO_REDIRECT="true"
	[ "$1" = "--disable-stdio-redirect" ] && STDIO_REDIRECT="false"

	[ "$1" = "--enable-fps" ]  && FPSCOUNTER="true"
	[ "$1" = "--disable-fps" ] && FPSCOUNTER="false"

	[ "$1" = "--enable-dos-svga" ]  && DOS_SVGA="true"
	[ "$1" = "--disable-dos-svga" ] && DOS_SVGA="false"

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

if [ "$PLATFORM" = "win32"   ] || [ "$PLATFORM" = "win64" ] ||
   [ "$PLATFORM" = "mingw32" ] || [ "$PLATFORM" = "mingw64" ]; then
	# Auto-prefix for the MSYS2 MINGW32/MINGW64 environments if a prefix wasn't
	# provided. This helps avoid errors that occur when gcc or libs exist in
	# /usr, which is used by MSYS2 for the MSYS environment.
	if [ "$PREFIX_IS_SET" = "false" ] && [ -n "$MSYSTEM" ] &&
	   [ "$(uname -o)" = "Msys" ] && [ "$MSYSTEM" != "MSYS" ]; then
		case "$MSYSTEM" in
		  "MINGW32"|"MINGW64")
			[ "$PLATFORM" = "win32" ] && PREFIX="/mingw32"
			[ "$PLATFORM" = "win64" ] && PREFIX="/mingw64"
			;;
		  "CLANG32"|"CLANG64")
			[ "$PLATFORM" = "win32" ] && PREFIX="/clang32"
			[ "$PLATFORM" = "win64" ] && PREFIX="/clang64"
			;;
		  "CLANGARM64")
			[ "$PLATFORM" = "win64" ] && PREFIX="/clangarm64"
			;;
		  "UCRT64")
			[ "$PLATFORM" = "win64" ] && PREFIX="/ucrt64"
			;;
		  *)
			echo "WARNING: Unknown MSYSTEM '$MSYSTEM'!"
			;;
		esac
	fi

	[ "$PLATFORM" = "win32" ] || [ "$PLATFORM" = "mingw32" ] && ARCHNAME=x86
	[ "$PLATFORM" = "win64" ] || [ "$PLATFORM" = "mingw64" ] && ARCHNAME=x64
	[ "$PLATFORM" = "mingw32" ] && MINGWBASE=i686-w64-mingw32-
	[ "$PLATFORM" = "mingw64" ] && MINGWBASE=x86_64-w64-mingw32-
	PLATFORM="mingw"
	echo "#define PLATFORM \"windows-$ARCHNAME\"" > src/config.h
	echo "SUBPLATFORM=windows-$ARCHNAME"         >> platform.inc
	echo "PLATFORM=$PLATFORM"                    >> platform.inc
	echo "MINGWBASE=$MINGWBASE"                  >> platform.inc
elif [ "$PLATFORM" = "unix" ] || [ "$PLATFORM" = "unix-devel" ]; then
	OS="$(uname -s)"
	MACH="$(uname -m)"

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
		"NetBSD")
			UNIX="netbsd"
			;;
		*)
			echo "WARNING: Should define proper UNIX name here!"
			UNIX="unix"
			;;
	esac

	if [ "$MACH" = "x86_64" ] || [ "$MACH" = "amd64" ]; then
		ARCHNAME=amd64
		#RAWLIBDIR=lib64
		# FreeBSD amd64 hack
		#[ "$UNIX" = "freebsd" ] && RAWLIBDIR=lib
	elif [ "$(echo "$MACH" | sed 's,i.86,x86,')" = "x86" ]; then
		ARCHNAME=x86
		#RAWLIBDIR=lib
	elif [ "$MACH" = "aarch64" ] || [ "$MACH" = "arm64" ]; then
		ARCHNAME=aarch64
	elif [ "$(echo "$MACH" | sed 's,^arm.*,arm,')" = "arm" ]; then
		ARCHNAME=arm
		#RAWLIBDIR=lib
	elif [ "$MACH" = "ppc" ]; then
		ARCHNAME=ppc
		#RAWLIBDIR=lib
	elif [ "$MACH" = "ppc64" ]; then
		ARCHNAME=ppc64
	elif [ "$MACH" = "mips" ]; then
		ARCHNAME=mips
	elif [ "$MACH" = "mips64" ]; then
		ARCHNAME=mips64
	elif [ "$MACH" = "m68k" ]; then
		ARCHNAME=m68k
	elif [ "$MACH" = "alpha" ]; then
		ARCHNAME=alpha
	elif [ "$MACH" = "hppa" ] || [ "$MACH" = "parisc" ]; then
		ARCHNAME=hppa
	elif [ "$MACH" = "sh4" ]; then
		ARCHNAME=sh4
	elif [ "$MACH" = "sparc" ]; then
		ARCHNAME=sparc
	elif [ "$MACH" = "sparc64" ]; then
		ARCHNAME=sparc64
	elif [ "$MACH" = "riscv64" ]; then
		ARCHNAME=riscv64
	else
		ARCHNAME=$MACH
		#RAWLIBDIR=lib
		echo "WARNING: Compiling on an unsupported architecture. Add a friendly MACH to config.sh."
	fi

	echo "#define PLATFORM \"$UNIX-$ARCHNAME\"" > src/config.h
	echo "SUBPLATFORM=$UNIX-$ARCHNAME"         >> platform.inc
	echo "PLATFORM=unix"                       >> platform.inc
elif [ "$PLATFORM" = "darwin" ] || [ "$PLATFORM" = "darwin-devel" ] ||
     [ "$PLATFORM" = "darwin-dist" ]; then

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

if [ "$PLATFORM" = "unix" ] || [ "$PLATFORM" = "darwin" ]; then
	LIBDIR="${LIBDIR}/megazeux"
elif [ "$PLATFORM" = "emscripten" ]; then
	LIBDIR="/data"
else
	LIBDIR="."
fi

### SYSTEM CONFIG DIRECTORY ###################################################

if [ "$PLATFORM" = "unix" ] || [ "$PLATFORM" = "darwin" ]; then
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
	echo "#define VERSION_DATE \" ($(date -u +%Y%m%d))\"" >> src/config.h
else
	echo "Not stamping version with today's date."
fi

echo "#define CONFDIR \"$SYSCONFDIR/\"" >> src/config.h

#
# Some platforms may have filesystem hierarchies they need to fit into
# FIXME: SHAREDIR should be hardcoded in fewer cases
#
if [ "$PLATFORM" = "unix" ] || [ "$PLATFORM" = "darwin" ]; then
	echo "#define CONFFILE \"megazeux-config\""      >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR/megazeux/\""  >> src/config.h
	echo "#define USERCONFFILE \".megazeux-config\"" >> src/config.h
elif [ "$PLATFORM" = "nds" ]; then
	SHAREDIR=/games/megazeux
	LICENSEDIR=$SHAREDIR
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "3ds" ]; then
	SHAREDIR=/3ds/megazeux
	LICENSEDIR=$SHAREDIR
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "dreamcast" ]; then
	SHAREDIR=/cd
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "wii" ]; then
	SHAREDIR=/apps/megazeux
	LICENSEDIR=$SHAREDIR
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "wiiu" ]; then
	SHAREDIR=fs:/vol/external01/wiiu/apps/megazeux
	LICENSEDIR=$SHAREDIR
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "switch" ]; then
	SHAREDIR=/switch/megazeux
	LICENSEDIR=$SHAREDIR
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
elif [ "$PLATFORM" = "darwin-dist" ]; then
	SHAREDIR=../Resources
	LICENSEDIR=$SHAREDIR
	GAMESDIR=$SHAREDIR
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\""           >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""            >> src/config.h
	echo "#define USERCONFFILE \".megazeux-config\"" >> src/config.h
elif [ "$PLATFORM" = "emscripten" ]; then
	SHAREDIR=/data
	LICENSEDIR=$SHAREDIR
	GAMESDIR=$SHAREDIR/game
	BINDIR=$SHAREDIR
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
	echo "#define SHAREDIR \"$SHAREDIR\""  >> src/config.h
else
	SHAREDIR=.
	LICENSEDIR=.
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
echo "LICENSEDIR=$LICENSEDIR" >> platform.inc

#
# Platform-specific libraries, or SDL?
#
if [ "$PLATFORM" = "3ds" ] ||
   [ "$PLATFORM" = "nds" ] ||
   [ "$PLATFORM" = "djgpp" ] ||
   [ "$PLATFORM" = "dreamcast" ] ||
   [ "$PLATFORM" = "egl" ]; then
	echo "Disabling SDL ($PLATFORM)."
	SDL="false"

	# The SDL software renderer works as a dummy renderer without
	# SDL, which is not desired for these platforms. Wii also
	# needs to do this conditionally if SDL is disabled.
	echo "Force-disabling software renderer (SDL or dummy only)."
	SOFTWARE="false"
fi

#
# SDL was disabled above; must also disable SDL-dependent modules
#
if [ "$SDL" = "false" ]; then
	echo "Force-disabling SDL dependent components:"
	echo " -> SOFTSCALE, OVERLAY"
	SOFTSCALE="false"
	OVERLAY="false"
	LIBSDL2="false"
else
	echo "#define CONFIG_SDL" >> src/config.h
	echo "BUILD_SDL=1" >> platform.inc
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
if [ "$SDL" = "false" ] && [ "$EGL" = "false" ]; then
	echo "Force-disabling OpenGL (no SDL or EGL backend)."
	GL="false"
fi

#
# The stack protector may cause issues with various C++ features (platform
# matrix claims it breaks exceptions) in some versions of MinGW. This hasn't
# been verified (and MZX doesn't use exceptions), but for now just disable it.
#
if [ "$PLATFORM" = "mingw" ]; then
	echo "Force-disabling stack protector on Windows."
	STACK_PROTECTOR="false"
fi

#
# Force-disable features unnecessary on Emscripten.
#
if [ "$PLATFORM" = "emscripten" ]; then
	echo "Enabling Emscripten-specific hacks."
	echo "BUILD_EMSCRIPTEN=1" >> platform.inc

	echo "Force-disabling stack protector (Emscripten)."
	STACK_PROTECTOR="false"

	echo "Force-disabling virtual filesystem (Emscripten)."
	VFS="false"

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
if [ "$PLATFORM" = "nds" ]; then
	echo "Enabling NDS-specific hacks."
	echo "#define CONFIG_NDS" >> src/config.h
	echo "BUILD_NDS=1" >> platform.inc

	echo "Force-disabling stack protector on NDS."
	STACK_PROTECTOR="false"

	echo "Building custom NDS renderer."

	echo "Force-disabling hash tables on NDS."
	COUNTER_HASH="false"

	echo "Force-disabling layer rendering on NDS."
	LAYER_RENDERING="false"

	echo "Force-disabling virtual filesystem on NDS."
	VFS="false"

	echo "Force-disabling existing music playback libraries on NDS."
	MODPLUG="false"
	MIKMOD="false"
	XMP="false"
	OPENMPT="false"
	REALITY="false"
	VORBIS="false"
fi

#
# If the 3DS arch is enabled, some code has to be compile time
# enabled too.
#
if [ "$PLATFORM" = "3ds" ]; then
	echo "Enabling 3DS-specific hacks."
	echo "#define CONFIG_3DS" >> src/config.h
	echo "BUILD_3DS=1" >> platform.inc

	echo "Force-disabling stack protector on 3DS."
	STACK_PROTECTOR="false"

	echo "Building custom 3DS renderer."

	echo "Disabling utils on 3DS."
	UTILS="false"

	echo "Force-disabling IPv6 on 3DS (not implemented)."
	IPV6="false"
fi

#
# If the Wii arch is enabled, some code has to be compile time
# enabled too.
#
if [ "$PLATFORM" = "wii" ]; then
	echo "Enabling Wii-specific hacks."
	echo "#define CONFIG_WII" >> src/config.h
	echo "BUILD_WII=1" >> platform.inc

	if [ "$SDL" = "false" ]; then
		echo "Force-disabling software renderer on Wii."
		echo "Building custom Wii renderers."
		SOFTWARE="false"
	fi

	# No SDL 2 support currently.
	LIBSDL2="false"

	echo "Force-disabling utils on Wii."
	UTILS="false"

	echo "Force-disabling stack protector on Wii."
	STACK_PROTECTOR="false"
fi

#
# If the Wii U arch is enabled, some code has to be compile time
# enabled too.
#
if [ "$PLATFORM" = "wiiu" ]; then
	echo "Enabling Wii U-specific hacks."
	echo "#define CONFIG_WIIU" >> src/config.h
	echo "BUILD_WIIU=1" >> platform.inc

	echo "Disabling utils on Wii U."
	UTILS="false"

	# Doesn't seem to be fully populated on the Wii U.
	GAMECONTROLLERDB="false"
fi

#
# If the Switch arch is enabled, some code has to be compile time
# enabled too.
#
if [ "$PLATFORM" = "switch" ]; then
	echo "Enabling Switch-specific hacks."
	echo "#define CONFIG_SWITCH" >> src/config.h
	echo "BUILD_SWITCH=1" >> platform.inc

	echo "Disabling utils on Switch."
	UTILS="false"

	echo "Force-enabling OpenGL ES support (Switch)."
	GLES="true"

	echo "Force-disabling IPv6 on Switch (FIXME getaddrinfo seems to not support it)."
	IPV6="false"

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

	echo "Force-disabling stack protector on PSP."
	STACK_PROTECTOR="false"
fi

#
# If the DJGPP arch is enabled, some code has to be compile time
# enabled too.
#
if [ "$PLATFORM" = "djgpp" ]; then
	echo "#define CONFIG_DJGPP" >> src/config.h
	echo "BUILD_DJGPP=1" >> platform.inc

	echo "Force-disabling stack protector (DOS)."
	STACK_PROTECTOR="false"

	if [ "$DOS_SVGA" = "true" ]; then
		echo "#define CONFIG_DOS_SVGA" >> src/config.h
		echo "BUILD_DOS_SVGA=1" >> platform.inc
		echo "SVGA software renderer enabled."
	else
		echo "SVGA software renderer disabled."
	fi
fi

#
# If the Dreamcast arch is enabled, some code has to be compile time
# enabled too.
#
if [ "$PLATFORM" = "dreamcast" ]; then
	echo "Enabling Dreamcast-specific hacks."
	echo "#define CONFIG_DREAMCAST" >> src/config.h
	echo "BUILD_DREAMCAST=1" >> platform.inc
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

	echo "Force-disabling stack protector on GP2X."
	STACK_PROTECTOR="false"

	echo "Force disabling software renderer on GP2X."
	SOFTWARE="false"

	echo "Force-enabling GP2X 320x240 renderer."
	GP2X="true"

	echo "Force-disabling Modplug audio."
	MODPLUG="false"
fi

#
# If the Pandora arch is enabled, some code has to be compile time
# enabled too.
#
if [ "$PLATFORM" = "pandora" ]; then
	echo "#define CONFIG_PANDORA" >> src/config.h
	echo "BUILD_PANDORA=1" >> platform.inc
fi

#
# Force-disable OpenGL and overlay renderers on PSP, GP2X, 3DS, NDS and Wii
#
if [ "$PLATFORM" = "psp" ] ||
   [ "$PLATFORM" = "gp2x" ] ||
   [ "$PLATFORM" = "nds" ] ||
   [ "$PLATFORM" = "3ds" ] ||
   [ "$PLATFORM" = "wii" ] ||
   [ "$PLATFORM" = "wiiu" ] ||
   [ "$PLATFORM" = "djgpp" ] ||
   [ "$PLATFORM" = "dreamcast" ]; then
  	echo "Force-disabling OpenGL and overlay renderers."
	GL="false"
	OVERLAY="false"
fi

#
# Force-disable the softscale renderer for SDL 1.2 (requires SDL_Renderer).
#
if [ "$SDL" = "true" ] && [ "$LIBSDL2" = "false" ] && [ "$SOFTSCALE" = "true" ]; then
	echo "Force-disabling softscale renderer (requires SDL 2)."
	SOFTSCALE="false"
fi

#
# Force-disable overlay renderers for SDL 2. The SDL 2 answer to SDL_Overlay
# involves planar YUV modes which don't mesh well with MZX's internal rendering.
#
if [ "$SDL" = "true" ] && [ "$LIBSDL2" = "true" ] && [ "$OVERLAY" = "true" ]; then
	echo "Force-disabling overlay renderers (requires SDL 1.2)."
	OVERLAY="false"
fi

#
# Must have at least one OpenGL renderer enabled
#
[ "$GL_FIXED" = "false" ] && [ "$GL_PROGRAM" = "false" ] && GL="false"

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
if [ "$SCREENSHOTS" = "false" ] &&
   [ "$UTILS" = "false" ] &&
   [ "$LIBPNG" = "true" ] &&
   [ "$PLATFORM" != "3ds" ]; then
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
if [ "$PLATFORM" != "unix" ] &&
   [ "$PLATFORM" != "unix-devel" ] &&
   [ "$PLATFORM" != "gp2x" ] &&
   [ "$PLATFORM" != "android" ]; then
	echo "Force-disabling pthread on non-POSIX platforms."
	PTHREAD="false"
fi

#
# Force disable modular DSOs.
#
if [ "$PLATFORM" = "gp2x" ] ||
   [ "$PLATFORM" = "nds" ] ||
   [ "$PLATFORM" = "3ds" ] ||
   [ "$PLATFORM" = "wii" ] ||
   [ "$PLATFORM" = "wiiu" ] ||
   [ "$PLATFORM" = "switch" ] ||
   [ "$PLATFORM" = "android" ] ||
   [ "$PLATFORM" = "emscripten" ] ||
   [ "$PLATFORM" = "psp" ] ||
   [ "$PLATFORM" = "djgpp" ] ||
   [ "$PLATFORM" = "dreamcast" ]; then
	echo "Force-disabling modular build (nonsensical or unsupported)."
	MODULAR="false"
fi

#
# Force disable networking (unsupported platform or no editor build)
#
if [ "$EDITOR" = "false" ] ||
   [ "$PLATFORM" = "djgpp" ] ||
   [ "$PLATFORM" = "nds" ]; then
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
if [ "$NETWORK" = "true" ] && [ "$UPDATER" = "false" ]; then
	echo "Force-disabling networking (no network-dependent features enabled)."
	NETWORK="false"
fi

if [ "$NETWORK" = "true" ]; then
	#
	# Force disable getaddrinfo and IPv6 (unsupported platform)
	#
	if [ "$PLATFORM" = "amiga" ] ||
	   [ "$PLATFORM" = "wii" ] ||
	   [ "$PLATFORM" = "psp" ]; then
		echo "Force-disabling getaddrinfo name resolution and IPv6 support (unsupported platform)."
		GETADDRINFO="false"
		IPV6="false"
	fi

	#
	# Force disable poll (unsupported platform)
	# PSPSDK doesn't have this function and old versions of Mac OS X have a bugged
	# implementation. Amiga hasn't been verified, but considering the other things
	# missing, it's probably a safe inclusion.
	#
	if [ "$PLATFORM" = "amiga" ] ||
	   [ "$PLATFORM" = "darwin" ] ||
	   [ "$PLATFORM" = "darwin-dist" ] ||
	   [ "$PLATFORM" = "darwin-devel" ] ||
	   [ "$PLATFORM" = "psp" ]; then
		echo "Force-disabling poll (unsupported platform)."
		POLL="false"
	fi
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
# Enable -fanalyzer support.
#
if [ "$ANALYZER" = "true" ]; then
	echo "Enabling -fanalyzer."
	echo ""
	echo "  *** WARNING ***: this is an experimental GCC feature! Its output"
	echo "  currently isn't very reliable, but it did help find some places"
	echo "  check_alloc wasn't being used and one legitimate memory leak."
	echo ""
	echo "BUILD_F_ANALYZER=1" >> platform.inc
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
		echo "Enabling $SANITIZER sanitizer (may enable some optimizations)."
		echo "SANITIZER=$SANITIZER" >> platform.inc
	fi

	#
	# Trace logging for debug builds, if enabled.
	#
	if [ "$TRACE_LOGGING" = "true" ]; then
		echo "Enabling trace logging."
		echo "#define DEBUG_TRACE" >> src/config.h
	fi
fi

#
# Enable stack protector.
#
if [ "$STACK_PROTECTOR" = "true" ]; then
	echo "Stack protector enabled."
	echo "BUILD_STACK_PROTECTOR=1" >> platform.inc
else
	echo "Stack protector disabled."
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
if [ "$PLATFORM" = "unix" ] ||
   [ "$PLATFORM" = "unix-devel" ] ||
   [ "$PLATFORM" = "pandora" ]; then
	#
	# Confirm the user's selection of X11, if they enabled it
	#
	if [ "$X11" = "true" ]; then
		#
		# Locate X11 binary path.
		#
		X11="false"
		for XBIN in X Xorg; do
			# check if X exists
			X11PATH=$(command -v "$XBIN" 2>/dev/null)
			if [ -n "$X11PATH" ]; then
				X11="true"
				break
			fi
		done

		#
		# Locate X11 lib dir. "*/lib" should be the last element in the
		# list so it will be used as the default in case of failure.
		#
		if [ "$X11" = "true" ]; then
			X11DIR=$(dirname "$(dirname "$X11PATH")")

			for X11LIBDIR in "$X11DIR/lib64" "$X11DIR/lib"; do
				for EXT in "so" "dylib"; do
					if [ -f "$X11LIBDIR/libX11.$EXT" ] || \
					   [ -L "$X11LIBDIR/libX11.$EXT" ]; then
						break 2
					fi
				done
			done
		fi

		if [ "$X11" = "false" ]; then
			echo "Force-disabling X11 (could not be queried)."
		fi
	fi

	if [ "$ICON" = "true" ] && [ "$X11" = "false" ]; then
		echo "Force-disabling icon branding (X11 disabled)."
		ICON="false"
	fi
else
	echo "Force-disabling X11 (unsupported platform)."
	X11="false"
fi

if [ "$X11" = "true" ]; then
	echo "X11 support enabled."
	echo "#define CONFIG_X11" >> src/config.h
	echo "X11DIR=${X11DIR}" >> platform.inc
	echo "X11LIBDIR=${X11LIBDIR}" >> platform.inc
fi

#
# Force disable icon branding.
#
if [ "$ICON" = "true" ]; then
	if [ "$PLATFORM" = "darwin" ] ||
	   [ "$PLATFORM" = "darwin-devel" ] ||
	   [ "$PLATFORM" = "darwin-dist" ] ||
	   [ "$PLATFORM" = "gp2x" ] ||
	   [ "$PLATFORM" = "psp" ] ||
	   [ "$PLATFORM" = "nds" ] ||
	   [ "$PLATFORM" = "wii" ] ||
	   [ "$PLATFORM" = "djgpp" ] ||
	   [ "$PLATFORM" = "dreamcast" ]; then
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
if [ "$PLATFORM" = "wii" ] && [ "$SDL" = "false" ]; then
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
# Frames-per-second counter
#
if [ "$FPSCOUNTER" = "true" ]; then
	echo "fps counter enabled."
	echo "#define CONFIG_FPS" >> src/config.h
else
	echo "fps counter disabled."
fi

#
# Layer rendering, if enabled
#
if [ "$LAYER_RENDERING" = "true" ]; then
	echo "Layer rendering enabled."
else
	echo "Layer rendering disabled."
	echo "#define CONFIG_NO_LAYER_RENDERING" >> src/config.h
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
			ICONFILE="$SHAREDIR/icons/hicolor/128x128/apps/megazeux.png"
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

	#
	# Handle networking options.
	#
	if [ "$GETADDRINFO" = "true" ]; then
		echo "getaddrinfo name resolution enabled."
		echo "#define CONFIG_GETADDRINFO" >> src/config.h
	else
		echo "getaddrinfo name resolution disabled."
	fi

	if [ "$POLL" = "true" ]; then
		echo "poll enabled."
		echo "#define CONFIG_POLL" >> src/config.h
	else
		echo "poll disabled; falling back to select."
	fi

	if [ "$IPV6" = "true" ]; then
		echo "IPv6 support enabled."
		echo "#define CONFIG_IPV6" >> src/config.h
	else
		echo "IPv6 support disabled."
	fi
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
# Enable extra memory hacks and memory compression, if enabled.
#
if [ "$EXTRAM" = "true" ]; then
	echo "Board memory compression and extra memory hacks enabled."
	echo "#define CONFIG_EXTRAM" >> src/config.h
	echo "BUILD_EXTRAM=1" >> platform.inc
else
	echo "Board memory compression and extra memory hacks disabled."
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
if [ "$COUNTER_HASH" = "true" ]; then
	echo "Hash table counter/string lookups enabled."
	echo "#define CONFIG_COUNTER_HASH_TABLES" >> src/config.h
	echo "BUILD_COUNTER_HASH_TABLES=1" >> platform.inc
else
	echo "Hash table counter/string lookups disabled (using binary search)."
fi

#
# Virtual filesystem, if enabled
#
if [ "$VFS" = "true" ]; then
	echo "Virtual filesystem enabled."
	echo "#define CONFIG_VFS" >> src/config.h
else
	echo "Virtual filesystem disabled."
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
if [ "$SDL" = "true" ] && [ "$LIBSDL2" = "false" ]; then
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
if [ "$LIBSDL2" = "true" ] && [ "$GAMECONTROLLERDB" = "true" ]; then
	echo "SDL_GameControllerDB enabled."
	echo "#define CONFIG_GAMECONTROLLERDB" >> src/config.h
	echo "BUILD_GAMECONTROLLERDB=1" >> platform.inc
else
	echo "SDL_GameControllerDB disabled."
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
