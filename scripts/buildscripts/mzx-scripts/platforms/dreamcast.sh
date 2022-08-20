#!/bin/sh

# Copyright (C) 2022 Alice Rowan <petrifiedrowan@gmail.com>
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
	[ -z "$KOS_BASE" ] && { ERRNO=20; return; }

	if [ -n "$MSYSTEM" ]; then
		export KOS_BASE="$(cygpath -u "$KOS_BASE")"
	fi

	. "$KOS_BASE"/environ.sh

	export PLATFORM_CAVERNS_EXEC="1ST_READ.BIN"
	export PLATFORM_CAVERNS_BASE="."
	export PLATFORM_CRLF=1
}

platform_config_release()
{
	arch/dreamcast/CONFIG.DC "$@"
}

platform_check_build()
{
	[ -f "mzxrun" ] || ERRNO=25;
}

platform_setup_environment()
{
	# KallistiOS pretty much needs to be set up manually, including deps.
	# https://www.digitalmzx.com/wiki/Compiling_MegaZeux#Dreamcast
	ERRNO=26;
}
