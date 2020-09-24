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
	# Not applicable.
	ERRNO=0;
}

platform_config_release()
{
	# Not applicable.
	ERRNO=0;
}

platform_make()
{
	$MZX_MAKE source || ERRNO=23;
}

platform_make_test()
{
	# Not applicable.
	ERRNO=0;
}

platform_check_build()
{
	# Not applicable.
	ERRNO=0;
}

platform_package()
{
	# Not applicable.
	ERRNO=0;
}

platform_setup_environment()
{
	# Not applicable.
	ERRNO=0;
}

