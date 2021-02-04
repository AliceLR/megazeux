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

. "$MZX_SCRIPTS/common-dkp.sh"

platform_init()
{
	dkp_init_check
	[ "$ERRNO" = "0" ] || { return; }

	dkp_dependency_check devkitARM mmutil
	[ "$ERRNO" = "0" ] || { return; }

	if [ -n "$MSYSTEM" ]; then
		export DEVKITPRO="$(cygpath -u "$DEVKITPRO")"
	fi

	export PATH="$PATH:$DEVKITPRO/devkitARM/bin"
	export PATH="$PATH:$DEVKITPRO/tools/bin"

	export PLATFORM_CAVERNS_EXEC="mzxrun.nds"
	export PLATFORM_CAVERNS_BASE="games/megazeux"
	export PLATFORM_CAVERNS_WHICH="caverns_mmutil"
	export PLATFORM_CRLF=1
}

platform_config_release()
{
	arch/nds/CONFIG.NDS "$@"
}

platform_check_build()
{
	[ -f "mzxrun" ] || ERRNO=25;
}

platform_setup_environment()
{
	dkp_install devkitARM ndstool dstools mmutil
	dkp_install libnds libfat-nds maxmod-nds nds-zlib
}
