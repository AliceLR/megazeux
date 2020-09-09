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
	# STUB - IMPLEMENT FOR THIS PLATFORM
	#
	# Should perform any platform-dependent checks
	# required to make sure this platform will build.
	# Any missing dependency should error here (this
	# should not attempt to install the dependencies).
	ERRNO=20
}

platform_config_debug()
{
	# STUB - IMPLEMENT FOR THIS PLATFORM
	ERRNO=21
}

platform_config_release()
{
	# STUB - IMPLEMENT FOR THIS PLATFORM
	ERRNO=22
}

platform_make()
{
	if [ "$MZX_RELEASE_TYPE" = "release" ]; then
		$MZX_MAKE_PARALLEL debuglink || ERRNO=23;
	else
		$MZX_MAKE_PARALLEL || ERRNO=23;
	fi
}

platform_make_test()
{
	if [ "$IS_HOST" = "true" ]; then
		$MZX_MAKE_PARALLEL test || ERRNO=24;
	fi
}

platform_check_build()
{
	# STUB - IMPLEMENT FOR THIS PLATFORM
	ERRNO=25;
}

platform_package()
{
	$MZX_MAKE archive || ERRNO=26;
}

platform_setup_environment()
{
	# STUB - IMPLEMENT FOR THIS PLATFORM
	#
	# Initialize any dependencies and the environment for this
	# platform. This may use any of the above hooks as-needed.
	# If missing dependencies are trivial to fetch this may
	# do so, but note that e.g. devkitPro hates having their
	# package servers used in automated CI build systems
	# and would prefer that you use Docker containers or a
	# preconfigured environment for that sort of thing. In other
	# words, use this to set up your environment once, but don't
	# repeatedly use it in automated systems.
	ERRNO=26
}

