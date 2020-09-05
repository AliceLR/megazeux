#!/bin/sh

# Copyright (C) 2020 Alice Rowan <petrifiedrowan@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

PSP_PORTS_REPO="https://github.com/pspdev/psp-ports.git"

platform_init()
{
	[ -z "$PSPDEV" ] && { ERRNO=20; return; }

	if [ -n "$MSYSTEM" ]; then
		export PSPDEV=`cygpath -u "$PSPDEV"`
	fi

	export PATH="$PATH:$PSPDEV/bin"

	export PLATFORM_CAVERNS_EXEC="EBOOT.PBP"
	export PLATFORM_CAVERNS_BASE="."
}

platform_config_release()
{
	arch/psp/CONFIG.PSP "$@"
}

platform_check_build()
{
	[ -f "mzxrun" ] || ERRNO=25;
}

platform_setup_environment()
{
	platform_init
	[ "$ERRNO" = "0" ] || { return; }

	# Get this as an absolute path...
	cd "$MZX_BUILD_WORKINGDIR"
	PSP_PORTS_DIR="$(pwd)/psp-ports"

	echo ""
	echo "/********************/"
	echo "  PSP - dependencies  "
	echo "/********************/"

	#
	# Can just install these for MSYS2.
	# This probably shouldn't be extended to support other systems because this sucks.
	#
	if [ -n "$MSYSTEM" ]; then
		pacman --needed --noconfirm -S autoconf automake libtool patch mingw-w64-x86_64-imagemagick
	fi

	cmd_check autoconf automake patch convert

	if [ ! -d "$PSP_PORTS_DIR" ]; then
		git clone "$PSP_PORTS_REPO" "$PSP_PORTS_DIR"
	fi
	cd "$PSP_PORTS_DIR"
	git pull

	#
	# The copy of the SDK formerly distributed with the Windows version of
	# devkitPro was missing an include. If the include is missing, patch
	# it back in.
	#
	if ! grep -q "psptypes" "$PSPDEV/psp/sdk/include/pspge.h" ; then
		patch "$PSPDEV/psp/sdk/include/pspge.h" "$MZX_SCRIPTS/patches/pspge.patch"
	fi


	if [ ! -f "$PSPDEV/psp/lib/libz.a" ]; then
		echo ""
		echo "/************/"
		echo "  PSP - zlib  "
		echo "/************/"

		cd "$PSP_PORTS_DIR/zlib"
		make -j8
		make install
	fi


	if [ ! -f "$PSPDEV/psp/lib/libpng.a" ]; then
		echo ""
		echo "/**************/"
		echo "  PSP - libpng  "
		echo "/**************/"

		cd "$PSP_PORTS_DIR/libpng"
		make -j8
		make install
	fi


	if [ ! -f "$PSPDEV/psp/lib/libvorbisidec.a" ]; then
		echo ""
		echo "/**************/"
		echo "  PSP - tremor  "
		echo "/**************/"

		cd "$PSP_PORTS_DIR/libTremor"

		LDFLAGS="-L$(psp-config --pspsdk-path)/lib" LIBS="-lc -lpspuser" ./autogen.sh \
		  --host psp --prefix=$(psp-config --psp-prefix)

		make -j8
		make install
	fi


	if [ ! -f "$PSPDEV/psp/lib/libSDL.a" ]; then
		echo ""
		echo "/***********/"
		echo "  PSP - SDL  "
		echo "/***********/"

		cd "$PSP_PORTS_DIR/SDL"

		./autogen.sh
		LDFLAGS="-L$(psp-config --pspsdk-path)/lib" LIBS="-lc -lpspuser" \
		  ./configure --host psp --prefix=$(psp-config --psp-prefix)

		#
		# If you thought that pspge.h patch was pretty cool, then you should
		# know that this generated a worthless SDL_config.h. Replace it with
		# a custom one based on SDL2's PSP config.
		#
		cp "$MZX_SCRIPTS/patches/SDL_config_psp.h" "include/SDL_config.h"

		make -j8
		make install
	fi


	if [ ! -f "$PSPDEV/psp/sdk/lib/libpspirkeyb.a" ]; then
		echo ""
		echo "/*****************/"
		echo "  PSP - pspirkeyb  "
		echo "/*****************/"

		cd "$PSP_PORTS_DIR/pspirkeyb"
		make -j8
		make install
	fi


	if [ ! -f "$PSPDEV/psp/lib/libGL.a" ]; then
		echo ""
		echo "/*************/"
		echo "  PSP - pspgl  "
		echo "/*************/"

		cd "$PSP_PORTS_DIR/pspgl"
		export PSP_MOUNTDIR=/Volumes/PSP
		export PSP_REVISION=1.50
		make -j8
		make install
	fi
}
