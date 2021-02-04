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
# Common functions for platforms using devkitPro toolchains.
#

dkp_init_check()
{
	if [ -z "$DEVKITPRO" ]; then
		ERRNO="DK-1"
		return
	fi
}

dkp_pacman_check()
{
	dkp_init_check
	[ "$ERRNO" = "0" ] || { return; }

	if [ -z "$DKP_PACMAN" ]; then
		if command -v pacman >/dev/null 2>&1; then
			export DKP_PACMAN="pacman"
		fi

		if command -v dkp-pacman >/dev/null 2>&1; then
			export DKP_PACMAN="dkp-pacman"
		fi

		if [ -z "$DKP_PACMAN" ]; then
			ERRNO="DK-2"
			return
		fi
	fi
}

# $1 - package to check for
dkp_dependency_check()
{
	dkp_pacman_check
	[ "$ERRNO" = "0" ] || { return; }

	if ! $DKP_PACMAN -Qi "$1" >/dev/null 2>&1; then
		ERRNO="DK-3:$1"
		return
	fi
}

# $@ - packages to install
dkp_install()
{
	dkp_pacman_check
	[ "$ERRNO" = "0" ] || { return; }

	$DKP_PACMAN --needed --noconfirm -S "$@"
}
