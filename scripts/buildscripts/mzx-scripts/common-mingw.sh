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

MZX_SDL_MINGW_VERSION="2.0.12"
MZX_SDL_MINGW_FILENAME="SDL2-devel-$MZX_SDL_MINGW_VERSION-mingw.tar.gz"

mingw_setup_environment()
{
	if [ -n "$MSYSTEM" ]; then
		#
		# Can just install the toolchains and packages for both builds...
		#
		pacman --needed --noconfirm -S mingw-w64-{x86_64,i686}-{zlib,gcc,libpng,libogg,libvorbis}

		#
		# ...except SDL2. The MSYS2 SDL2 builds unfortunately have some issues that
		# make it unsuitable for release builds. Just grab the official MinGW SDL2
		# package and extract it; the environment variable SDL_PREFIX will be set
		# during the MinGW builds to make sure this SDL gets used instead.
		#
		if [ ! -f "$MZX_WORKINGDIR/$MZX_SDL_MINGW_FILENAME" ]; then
			cd "$MZX_WORKINGDIR"
			wget "https://www.libsdl.org/release/$MZX_SDL_MINGW_FILENAME"
			tar -xzf "$MZX_SDL_MINGW_FILENAME"

			rm -rf "sdl2-mingw"
			mv "SDL2-$MZX_SDL_MINGW_VERSION" "sdl2-mingw"
		fi
	fi
}
