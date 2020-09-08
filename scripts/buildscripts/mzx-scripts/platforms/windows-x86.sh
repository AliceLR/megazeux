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

MINGW32_PLATFORM="mingw32"
MINGW32_CONFIG="--prefix $MINGW32_PREFIX"
if [ -n "$MSYSTEM" ]; then
	MINGW32_PLATFORM="win32"
	MINGW32_CONFIG=""
	MSYSTEM="MINGW32"
	. /etc/profile

	export SDL_PREFIX="$MZX_WORKINGDIR/sdl2-mingw/i686-w64-mingw32/"
	[ -d "$SDL_PREFIX" ] || { mzx_warn "Failed to find MinGW SDL dir!" "MINGW32-1"; }
fi

platform_init()
{
	mingw32_check
	[ "$ERRNO" = "0" ] || { return; }

	if [ -n "$MSYSTEM" ]; then
		IS_HOST="true"
	else
		export SDL_PREFIX="$MINGW32_PREFIX"
	fi

	export PLATFORM_CAVERNS_EXEC="mzxrun.exe"
	export PLATFORM_CAVERNS_BASE="."
	export PLATFORM_CRLF=1
}

platform_config_debug()
{
	./config.sh --platform $MINGW32_PLATFORM $MINGW32_CONFIG --enable-fps --enable-trace "$@"
}

platform_config_release()
{
	./config.sh --platform $MINGW32_PLATFORM $MINGW32_CONFIG --enable-release "$@"
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
