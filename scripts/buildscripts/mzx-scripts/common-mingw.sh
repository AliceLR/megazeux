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

#
# Common functions for MinGW.
#

MZX_SDL_MINGW_VERSION="1.2.15"
MZX_SDL_MINGW_FILENAME="SDL-devel-$MZX_SDL_MINGW_VERSION-mingw32.tar.gz"
MZX_SDL2_MINGW_VERSION="2.0.12"
MZX_SDL2_MINGW_FILENAME="SDL2-devel-$MZX_SDL2_MINGW_VERSION-mingw.tar.gz"

# $1 - toolchain prefix
# $2 - dependencies path
mingw_check()
{
	#
	# Check for the MinGW toolchain.
	#
	cmd_check "$1gcc" "$1g++" "$1ar" "$1ld" "$1strip" "$1objcopy" "$1windres"
	[ "$ERRNO" = "0" ] || { return; }

	#
	# Check for MinGW prebuilt dependencies.
	#
	MISSING=""
	for DEP in "bin/SDL2.dll" "bin/sdl2-config" "bin/libpng-config" "lib/libSDL2main.a" \
	 "lib/libz.a" "lib/libpng.a" "lib/libogg.a" "lib/libvorbis.a" "lib/libvorbisfile.a"
	do
		if [ ! -e "$2/$DEP" ]; then
			MISSING="$MISSING:$DEP"
		fi
	done
	if [ -n "$MISSING" ]; then
		ERRNO="MINGW-DEPS-MISSING$MISSING";
	fi
}

mingw32_check()
{
	if [ -n "$MSYSTEM" ]; then
		mingw_check "" "/mingw32"
	else
		mingw_check "i686-w64-mingw32-" "$MINGW32_PREFIX"
	fi
}

mingw64_check()
{
	if [ -n "$MSYSTEM" ]; then
		mingw_check "" "/mingw64"
	else
		mingw_check "x86_64-w64-mingw32-" "$MINGW64_PREFIX"
	fi
}

mingw_setup_environment()
{
	if [ -n "$MSYSTEM" ]; then
		#
		# Can just install the toolchains and packages for both builds...
		#
		M32="mingw-w64-i686"
		M64="mingw-w64-x86_64"
		pacman --needed --noconfirm -S \
		 $M32-gcc $M32-zlib $M32-libpng $M32-libogg $M32-libvorbis $M32-SDL2 \
		 $M64-gcc $M64-zlib $M64-libpng $M64-libogg $M64-libvorbis $M64-SDL2

		#
		# ...except SDL. The MSYS2 SDL builds unfortunately have some issues that
		# make them unsuitable for release builds. Just grab the official MinGW SDL
		# packages and extract them; the environment variable SDL_PREFIX will be set
		# during the MinGW builds to make sure the correct SDL gets used instead.
		#
		if [ ! -f "$MZX_WORKINGDIR/$MZX_SDL_MINGW_FILENAME" ]; then
			cd "$MZX_WORKINGDIR" || { ERRNO="MINGW-CD"; return; }
			wget "https://www.libsdl.org/release/$MZX_SDL_MINGW_FILENAME"
			tar -xzf "$MZX_SDL_MINGW_FILENAME"

			rm -rf "sdl1-mingw"
			mv "SDL-$MZX_SDL_MINGW_VERSION" "sdl1-mingw"

		fi

		if [ ! -f "$MZX_WORKINGDIR/$MZX_SDL2_MINGW_FILENAME" ]; then
			cd "$MZX_WORKINGDIR" || { ERRNO="MINGW-CD"; return; }
			wget "https://www.libsdl.org/release/$MZX_SDL2_MINGW_FILENAME"
			tar -xzf "$MZX_SDL2_MINGW_FILENAME"

			rm -rf "sdl2-mingw"
			mv "SDL2-$MZX_SDL2_MINGW_VERSION" "sdl2-mingw"
		fi
	fi
}
