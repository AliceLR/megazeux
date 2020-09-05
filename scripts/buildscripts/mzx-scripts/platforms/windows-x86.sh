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

source "$MZX_SCRIPTS/common.sh"
source "$MZX_SCRIPTS/common-mingw.sh"

MINGW32_PLATFORM="mingw32"
if [ -n "$MSYSTEM" ]; then
	MINGW32_PLATFORM="win32"
	MSYSTEM="MINGW32"
	source /etc/profile

	export SDL_PREFIX="$MZX_WORKINGDIR/sdl2-mingw/i686-w64-mingw32/"
	[ -d "$SDL_PREFIX" ] || { mzx_warn "Failed to find MinGW SDL dir!" "MINGW32-1"; }
fi

platform_init()
{
	# FIXME should verify packages somehow...
	[ -n "$MSYSTEM" ] && IS_HOST="true"

	export PLATFORM_CAVERNS_EXEC="mzxrun.exe"
	export PLATFORM_CAVERNS_BASE="."
}

platform_config_debug()
{
	./config.sh --platform $MINGW32_PLATFORM --enable-fps --enable-trace "$@"
}

platform_config_release()
{
	./config.sh --platform $MINGW32_PLATFORM --enable-release "$@"
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
