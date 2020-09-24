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

CARRIAGE_RETURN=$(printf '\r')

#
# Replace LF with CRLF in a POSIX-compatible way. Since the POSIX versions of
# sed and numerous other utilities don't support \r, it needs to be inserted via
# the printf variable above. Using ERE allows this method to insert \r only
# when a \n is found without a \r, meaning theoretically this should be safe to
# use on files that already use CRLF. This is still a hack though and unix2dos
# (below) should be preferred over this.
#
# $1 - file to convert.
#
crlf_convert_posix()
{
	mv "$1" "$1~" || { return 1; }
	sed -E "s/(^|[^$CARRIAGE_RETURN])$/\1$CARRIAGE_RETURN/" "$1~" > "$1" || { return 1; }
	rm "$1~"
}

#
# unix2dos isn't a standard POSIX utility but it's fairly common. This likely
# does a better job than the hack above, so generally it should be used instead.
#
# $1 - file to convert.
#
crlf_convert_unix2dos()
{
	unix2dos "$1" >/dev/null 2>&1 || { return 1; }
}

#
# Convert certain LF files to CRLF within the repository.
# This generally includes anything users are expected to read directly or to
# modify in Windows and in any platforms that will generally be configured from
# Windows by most users (e.g. consoles, HTML5).
#
# This needs to be done before "make build"/"make archive"/etc. because on
# platforms with updates, those Makefile rules will generate manifest.txt.
#
# Note that this isn't necessary the other way around. The source tarball has
# any CRLFs added by a Windows host removed in "make source" (via git's autocrlf
# setting). Other packages that should not use CRLF (Debian, Fedora, etc.)
# are generally created using their respective platforms as the host.
#
crlf_convert_repository()
{
	#
	# This is pretty pointless in MSYS2; you should enable git autocrlf instead.
	#
	[ -z "$MSYSTEM" ] || { return; }

	FILES=$(find config.txt LICENSE assets docs arch/mingw arch/*/pad.config arch/emscripten/web/res/index.html \
		-name "*.txt" -o \
		-name "*.frag" -o \
		-name "*.vert" -o \
		-name "*.bat" -o \
		-name "index.html" -o \
		-name "*LICENSE*" -o \
		-name "pad.config")

	if command -v unix2dos >/dev/null
	then
		CONVERT_FUNCTION="crlf_convert_unix2dos"
	else
		CONVERT_FUNCTION="crlf_convert_posix"
	fi

	for F in $FILES
	do
		if "$CONVERT_FUNCTION" "$F"
		then
			mzx_log "LF->CRLF '$F'"
		else
			mzx_warn "LF->CRLF '$F' failed" 1310
		fi
	done
}
