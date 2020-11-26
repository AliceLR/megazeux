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
	[ -z "$DJGPP" ] && { ERRNO=20; return; }

	if [ -n "$MSYSTEM" ]; then
		export DJGPP="$(cygpath -u "$DJGPP")"
	fi

	export PATH="$PATH:$DJGPP/bin"

	export PLATFORM_CAVERNS_EXEC="mzxrun.exe"
	export PLATFORM_CAVERNS_BASE="."
	export PLATFORM_CRLF=1
}

platform_config_release()
{
	arch/djgpp/CONFIG.DJGPP "$@"
}

platform_check_build()
{
	[ -f "mzxrun.exe" ] || ERRNO=25;
	[ -f "megazeux.exe" ] || ERRNO=25;
}

platform_setup_environment()
{
	# FIXME pretty much involves building all deps manually.
	# https://www.digitalmzx.com/wiki/Compiling_MegaZeux#DJGPP
	ERRNO=26;
}
