#!/bin/sh

# Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

# $1 - File to POST.
# $2 - URL to POST the updates to.
upload_post_file()
{
	hash=$(sha256sum "$1" | cut -f 1 -d " ")
	curl --progress-bar \
	  -H "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_10_3) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.89 Safari/537.36" \
	  -F "hash=$hash" \
	  -F "upload=@$1" \
	  "$2"
}

# $1 - URL to POST the updates to.
upload_curl()
{
	upload_post_file "$MZX_TARGET/uploads/updates.tar" "$1"
}

# $1 - SSH login.
upload_ssh()
{
	scp "$MZX_TARGET/uploads/updates.tar" "$1:updates.tar"
	ssh "$1" "tar -xvf updates.tar && rm -f updates.tar"
}
