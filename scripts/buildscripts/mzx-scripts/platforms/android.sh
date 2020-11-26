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

JNI_LIB_DIR=arch/android/project/app/jni/lib
JNI_LIB_SO=libmain.so

. "$MZX_SCRIPTS/common.sh"

platform_init()
{
	[ -z "$NDK_PATH" ] && { ERRNO=20; return; }

	if [ -n "$MSYSTEM" ]; then
		export NDK_PATH="$(cygpath -u "$NDK_PATH")"
	fi
}

platform_config_release()
{
	arch/android/CONFIG.ANDROID "$@"
}

platform_make()
{
	$MZX_MAKE_PARALLEL dist || ERRNO=23;
}

platform_check_build()
{
	[ -f "$JNI_LIB_DIR/armeabi-v7a/$JNI_LIB_SO" ] || ERRNO=24;
	[ -f "$JNI_LIB_DIR/arm64-v8a/$JNI_LIB_SO" ] || ERRNO=24;
	[ -f "$JNI_LIB_DIR/x86/$JNI_LIB_SO" ] || ERRNO=24;
	[ -f "$JNI_LIB_DIR/x86_64/$JNI_LIB_SO" ] || ERRNO=24;
}

platform_package()
{
	$MZX_MAKE apk || ERRNO=25;
}

platform_setup_environment()
{
	platform_init
	[ "$ERRNO" -eq "0" ] || { return; }

	arch/android/CONFIG.ANDROID
	$MZX_MAKE_PARALLEL deps-install || ERRNO=26;

	$MZX_MAKE distclean
}
