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

platform_init()
{
	[ -z "$PSPDEV" ] && { ERRNO=20; return; }

	if [ -n "$MSYSTEM" ]; then
		export PSPDEV="$(cygpath -u "$PSPDEV")"
	fi

	export PATH="$PATH:$PSPDEV/bin"

	export PLATFORM_CAVERNS_EXEC="EBOOT.PBP"
	export PLATFORM_CAVERNS_BASE="."
	export PLATFORM_CRLF=1
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

	#
	# Can just install these for MSYS2.
	# This probably shouldn't be extended to support other systems because this sucks.
	#
	if [ -n "$MSYSTEM" ]; then
		pacman --needed --noconfirm -S patch mingw-w64-x86_64-imagemagick
	fi

	cmd_check convert

	#
	# The copy of the SDK formerly distributed with the Windows version of
	# devkitPro was missing an include. If the include is missing, patch
	# it back in.
	#
	if ! grep -q "psptypes" "$PSPDEV/psp/sdk/include/pspge.h" ; then
		patch "$PSPDEV/psp/sdk/include/pspge.h" "$MZX_SCRIPTS/patches/pspge.patch"
	fi

	PSP_MISSING=""

	if [ ! -f "$PSPDEV/psp/lib/libz.a" ]; then
		PSP_MISSING="$PSP_MISSING zlib"
	fi

	if [ ! -f "$PSPDEV/psp/lib/libpng.a" ]; then
		PSP_MISSING="$PSP_MISSING libpng"
	fi

	if [ ! -f "$PSPDEV/psp/lib/libvorbisidec.a" ]; then
		PSP_MISSING="$PSP_MISSING libtremor"
	fi

	if [ ! -f "$PSPDEV/psp/lib/libSDL.a" ]; then
		PSP_MISSING="$PSP_MISSING SDL"
	fi

	if [ ! -f "$PSPDEV/psp/lib/libSDL2.a" ]; then
		PSP_MISSING="$PSP_MISSING SDL2"
	fi

	if [ ! -f "$PSPDEV/psp/sdk/lib/libpspirkeyb.a" ]; then
		PSP_MISSING="$PSP_MISSING pspirkeyb"
	fi

	if [ ! -f "$PSPDEV/psp/lib/libGL.a" ]; then
		PSP_MISSING="$PSP_MISSING pspgl"
	fi

	if [ -n "$PSP_MISSING" ]; then
		echo "Missing PSP dependencies from the current pspdev install. Use:"
		echo ""
		echo "  ./libraries.sh" "$PSP_MISSING"
		echo ""
		echo "to install the missing libraries."
		ERRNO=26;
	fi
}
