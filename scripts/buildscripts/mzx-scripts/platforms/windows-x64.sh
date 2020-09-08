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

. "$MZX_SCRIPTS/common.sh"
. "$MZX_SCRIPTS/common-mingw.sh"

MINGW64_PLATFORM="mingw64"
MINGW64_CONFIG="--prefix $MINGW64_PREFIX"
if [ -n "$MSYSTEM" ]; then
	MINGW64_PLATFORM="win64"
	MINGW64_CONFIG=""
	MSYSTEM="MINGW64"
	. /etc/profile

	export SDL_PREFIX="$MZX_WORKINGDIR/sdl2-mingw/x86_64-w64-mingw32/"
	[ -d "$SDL_PREFIX" ] || { mzx_warn "Failed to find MinGW SDL dir!" "MINGW64-1"; }
fi

platform_init()
{
	mingw64_check
	[ "$ERRNO" = "0" ] || { return; }

	if [ -n "$MSYSTEM" ]; then
		IS_HOST="true"
	else
		export SDL_PREFIX="$MINGW64_PREFIX"
	fi

	export PLATFORM_CAVERNS_EXEC="mzxrun.exe"
	export PLATFORM_CAVERNS_BASE="."
	export PLATFORM_CRLF=1
}

platform_config_debug()
{
	./config.sh --platform $MINGW64_PLATFORM $MINGW64_CONFIG --enable-fps --enable-trace "$@"
}

platform_config_release()
{
	./config.sh --platform $MINGW64_PLATFORM $MINGW64_CONFIG --enable-release "$@"
}

platform_check_build()
{
	[ -f "mzxrun.exe" ] || { ERRNO=25; }
	[ -f "megazeux.exe" ] || { ERRNO=25; }
}

platform_setup_environment()
{
	mingw_setup_environment
}
