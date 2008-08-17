#!/bin/sh

if [ "$1" = "" ]; then
	if [ "`uname -o`" = "GNU/Linux" ]; then
		echo "Assuming Linux operating system.."
		cp -f arch/Makefile.linux Makefile.platform
		ARCH=linux
	else
		echo "Assuming Windows operating system.."
		cp -f arch/Makefile.win32 Makefile.platform
		ARCH=win32
	fi
else
	cp -f arch/Makefile.$1 Makefile.platform
	ARCH=$1
fi

if [ ! -f Makefile.platform ]; then
	echo "Invalid platform selection (see arch/)"
	exit 1
fi

if [ "$2" = "" ]; then
	echo "Assuming /usr prefix.."
	PREFIX=/usr
else
	PREFIX=$2
fi

if [ "$3" = "" ]; then
	if [ "$ARCH" = "linux" ]; then
		echo "Assuming /etc sysconfdir.."
		SYSCONFDIR=/etc
	else
		echo "Assuming config.txt is in working directory.."
		SYSCONFDIR=.
	fi
else
	SYSCONFDIR=$3
fi

echo                       >> Makefile.platform
echo "# config time stuff" >> Makefile.platform
echo "PREFIX=$PREFIX"      >> Makefile.platform

echo "#define CONFDIR  \"$SYSCONFDIR/\"" > src/config.h

#
# Windows, MacOS X and Linux-Static all want to read files
# relative to their install directory. Every other platform
# wants to read from a /usr tree, and rename config.txt.
#
if [ "$ARCH" = "win32" -o "$ARCH" = "macos" -o "$ARCH" = "linux-static" \
     -o "$ARCH" = "psp" ]; then
	echo "#define SHAREDIR \"./\""         >> src/config.h
	echo "#define CONFFILE \"config.txt\"" >> src/config.h
else
	echo "#define SHAREDIR \"$PREFIX/share/megazeux/\"" >> src/config.h
	echo "#define CONFFILE \"megazeux-config\""         >> src/config.h
fi

#
# linux target has "make install", which requires these features
#
if [ "$ARCH" = "linux" -o "$ARCH" = "psp" ]; then
	echo "TARGET=`grep TARGET Makefile.in | sed "s/ //g" | cut -d "=" -f 2`" \
		>> Makefile.platform
	echo "SYSCONFDIR=$SYSCONFDIR" >> Makefile.platform
fi

if [ "$ARCH" != "win32" -a "$ARCH" != "psp" ]; then
	# default, enable X support
	X11="true"

	# attempt auto-detection
	if [ "$4" = "" ]; then
		# try to run X
		X -version >/dev/null 2>&1

		# X queried successfully
		if [ "$?" != "0" ]; then
			X11="false"
		fi
	fi

	# don't autodetect, force off
	if [ "$4" = "-nox11" ]; then
		X11="false"
	fi

	# asked for X11?
	if [ "$X11" = "true" ]; then
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

echo "All done!"

