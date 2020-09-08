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

#
# Convert the current working directory from release dir to an updater dir.
#
process_release_dir()
{
	#
	# Run some checks to make sure this isn't something other than a
	# build directory, because this will totally nuke the directory
	# it is run in.
	#
	# Make sure there's no Makefile or config.sh or anything else
	# that should only appear in a development directory.
	#
	if [ -f config.sh ] || [ -f Makefile ] || [ -d src ]
	then
		mzx_error "this appears to be a source dir. Aborting." 3
		exit 1
	fi

	#
	# There needs to be a manifest.txt file for this to be a valid update dir.
	#
	if [ ! -f manifest.txt ]
	then
		mzx_error "no manifest.txt found in '$(pwd)'. Aborting." 4
		exit 1
	fi

	#
	# Compress everything to its sha256sum.
	#
	FILES=$(find . -mindepth 1 -type f -not -name manifest.txt -not -name config.txt)
	for file in $FILES
	do
		#
		# Calculate the sha256sum and make sure it's something that's actually in
		# the manifest. If not, it doesn't matter what it is--just abort.
		#
		sha256sum=$(sha256sum -b "$file" | cut -f1 -d' ')
		if ! grep -q "$sha256sum" manifest.txt
		then
			mzx_error "file '$file' sha256sum '$sha256sum' not found in manifest.txt! Aborting." 5
			exit 1
		fi

		gzip < "$file" > "$sha256sum"
		rm -f "$file"
	done

	#
	# Compress manifest.txt but keep the same filename.
	#
	mv manifest.txt __tmp_manifest.txt
	gzip < __tmp_manifest.txt > manifest.txt
	rm -f __tmp_manifest.txt

	#
	# Prune directories and config.txt.
	#
	find . -mindepth 1 -type d -exec rm -rf '{}' ';' 2>/dev/null
	rm -f config.txt
	chmod 0644 ./*
}

#
# $@ - list of tags-to-branches to process in the format "tag:branch".
#      example: process_updates Stable:2.90 Unstable:master Debytecode:debytecode
#
process_updates()
{
	mkdir -p "$MZX_TARGET"
	cd "$MZX_TARGET" || { mzx_error "failed to cd to '$MZX_TARGET'" 1; exit 1; }

	[ -d "releases" ] || \
	 { mzx_error "Couldn't find 'releases' dir. Must run mzx-build.sh first!" 2; exit 1; }
	rm -rf "uploads"
	mkdir -p "uploads"
	rm -rf "releases-update"
	cp -r "releases" "releases-update"

	#
	# Process each tag:branch pair.
	#
	while [ -n "$1" ]
	do
		UPDATE_TAG=$(echo "$1" | cut -d: -f1)
		UPDATE_BRANCH=$(echo "$1" | cut -d: -f2)

		#
		# Handle each architecture found in the branch directory if it exists.
		#
		if [ -d "$MZX_TARGET/releases-update/$UPDATE_BRANCH" ]
		then
			for F in "$MZX_TARGET/releases-update/$UPDATE_BRANCH"/*
			do
				if [ -d "$F" ]
				then
					mzx_log "Processing '$F'..."
					(
						if ! cd "$F"
						then
							mzx_error "failed to cd to '$F'" 3
							exit 1
						fi
						process_release_dir
					) &
				fi
			done
		fi

		#
		# Add each type to the updates file regardless of whether or not it was
		# processed. This way, individual release tags can be updated without
		# breaking other existing release tags.
		#
		echo "Current-$UPDATE_TAG: $UPDATE_BRANCH" >> "$MZX_TARGET/releases-update/updates-uncompressed.txt"
		shift
	done
	wait

	cd "$MZX_TARGET/releases-update" || { mzx_error "failed to cd to '$MZX_TARGET/releases-update'" 4; exit 1; }
	if [ -f "updates-uncompressed.txt" ]
	then
		gzip < updates-uncompressed.txt > updates.txt
		rm -f updates-uncompressed.txt
		tar -cvf "$MZX_TARGET/uploads/updates.tar" ./*
	else
		mzx_error "No updates to process! Aborting." 5
		exit 1
	fi
}
