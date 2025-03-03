#!/bin/sh

# Copyright (C) 2025 Alice Rowan <petrifiedrowan@gmail.com>
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
	[ -z "$BLOCKSDS" ] && { ERRNO=20; return; }
	[ -z "$WONDERFUL_TOOLCHAIN" ] && { ERRNO=20; return; }

	if [ -n "$MSYSTEM" ]; then
		export BLOCKDS="$(cygpath -u "$BLOCKSDS")"
		export WONDERFUL_TOOLCHAIN="$(cygpath -u "$WONDERFUL_TOOLCHAIN")"
	fi

	export PATH="$PATH:$WONDERFUL_TOOLCHAIN/bin"

	export PLATFORM_CAVERNS_EXEC="mzxrun.nds"
	export PLATFORM_CAVERNS_BASE="games/megazeux"
	export PLATFORM_CAVERNS_WHICH="caverns_mmutil"
	export PLATFORM_CRLF=1
}

platform_config_release()
{
	arch/nds-blocksds/CONFIG.NDS "$@"
}

platform_check_build()
{
	[ -f "mzxrun" ] || ERRNO=25;
}

platform_setup_environment()
{
	return
}
