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

if [ -z $MZX_MAKE ]; then
	if command -v gmake &> /dev/null; then
		export MZX_MAKE="gmake"
	else
		export MZX_MAKE="make"
	fi
fi
if [ -z $MZX_MAKE_PARALLEL ]; then
	export MZX_MAKE_PARALLEL="$MZX_MAKE -j8"
fi

OLD_PATH="$PATH"

build_log()
{
	echo "INFO:" "$1" >&2
}

build_warn()
{
	echo "WARN:" "$1" "(code $2)" >&2
}

build_error()
{
	echo "ERROR:" "$1" "(code $2)" >&2
}

build_init()
{
	[ -f "config.sh" ] || { echo "Use this script from the MegaZeux repository root."; exit 1; }

	if echo "$MZX_SCRIPTS_BASE" | grep -q "^\(./\)\?scripts\(/\|\$\)"; then
		echo "ERROR: attempting to run this script from 'scripts/'."
		echo "The scripts directory is tracked by Git and may be modified when"
		echo "checking out different tags/branches for MZX builds."
		echo ""
		echo "You should duplicate 'scripts/' to another directory and run this script"
		echo "from that directory instead."
		exit 1
	fi

	mkdir -p "$MZX_TARGET"

	#
	# Reset the git repository and make sure all branches/tags are up-to-date.
	#
	git reset --hard
	git fetch
	git fetch --tags
}

# $1 subplatform
# $2 tag or branch
# $3 update branch or ""
# $4 extra configuration flags or ""
# $5 "release" or "debug"
build_common()
{
	export SUBPLATFORM=$1
	MZX_GIT_BRANCH=$2
	MZX_UPDATE_BRANCH=$3
	MZX_EXTRA_CONFIG_FLAGS=$4
	MZX_RELEASE_TYPE=$5
	[ -n "$SUBPLATFORM" ] || { build_error "no SUBPLATFORM defined" 1; return; }
	[ -n "$MZX_GIT_BRANCH" ] || { build_error "no git branch/tag provided" 2; return; }
	[ -f "$MZX_SCRIPTS_BASE/platforms/$SUBPLATFORM.sh" ] ||\
	 { build_error "couldn't find $MZX_SCRIPTS_BASE/platforms/$SUBPLATFORM.sh" 3; return; }

	export PATH="$OLD_PATH"
	source "$MZX_SCRIPTS_BASE/platforms/default.sh"
	source "$MZX_SCRIPTS_BASE/platforms/$SUBPLATFORM.sh"

	export ERRNO=0
	export IS_HOST="false"

	echo "" >&2
	build_log "Starting build:"
	build_log "  Platform:      $SUBPLATFORM"
	build_log "  Type:          $MZX_RELEASE_TYPE"
	build_log "  Branch/tag:    $MZX_GIT_BRANCH"
	build_log "  Update branch: $MZX_UPDATE_BRANCH"
	build_log "  Options:       $MZX_EXTRA_CONFIG_FLAGS"

	#
	# Initialize platform-dependent variables and perform platform-dependent
	# checks to make sure MegaZeux will be able to build.
	#
	platform_init
	[ "$ERRNO" -eq "0" ] || { build_warn "failed to initialize platform; skipping" $ERRNO; return; }

	#
	# Check out the requested branch.
	#
	git checkout $MZX_GIT_BRANCH
	git merge
	$MZX_MAKE distclean
	rm -rf build

	#
	# Run config.sh.
	#
	if [ "$MZX_RELEASE_TYPE" = "release" ]; then
		platform_config_release "$MZX_EXTRA_CONFIG_FLAGS"
	else
		platform_config_debug "$MZX_EXTRA_CONFIG_FLAGS"
	fi
	[ "$ERRNO" -eq "0" ] || { build_error "failed to configure for $MZX_RELEASE_TYPE" $ERRNO; return; }
	[ -e "platform.inc" ] || { build_error "config.sh failed to create platform.inc for $MZX_RELEASE_TYPE" 10; return; }

	#
	# Build MegaZeux.
	#
	platform_make
	[ "$ERRNO" -eq "0" ] || { build_error "make failed" $ERRNO; return; }

	#
	# Perform make test (if applicable).
	# If this fails it's not a fatal error but it definitely should be looked at.
	#
	platform_make_test
	[ "$ERRNO" -eq "0" ] || { build_warn "make test failed" $ERRNO; ERRNO=0; }

	#
	# Perform any other checks on the build that may be necessary.
	#
	platform_check_build
	[ "$ERRNO" -eq "0" ] || { build_error "build check failed" $ERRNO; return; }

	#
	# Package the build.
	# This will generally produce a build/SUBPLATFORM/ folder containing the
	# build and a build/dist/SUBPLATFORM/ folder containing the release archive.
	# Only check for the build/dist/SUBPLATFORM/ right now.
	#
	platform_package
	[ "$ERRNO" -eq "0" ] || { build_error "package failed" $ERRNO; return; }
	[ -d "build/dist/$SUBPLATFORM" ] || { build_error "couldn't find build/dist/$SUBPLATFORM/" 11; return; }

	#
	# All files in build/dist/SUBPLATFORM/ (there may be multiple depending on how the
	# platform handles things like .debug files) should be moved to the $MZX_TARGET/zips/ folder.
	#
	ZIP_DIR="$MZX_GIT_BRANCH"
	if [ "$MZX_GIT_BRANCH" = "master" -a "$MZX_UPDATE_BRANCH" != "" ]; then
		ZIP_DIR="$MZX_UPDATE_BRANCH"
	fi
	mkdir -p "$MZX_TARGET/zips/$ZIP_DIR/"
	mv build/dist/$SUBPLATFORM/* "$MZX_TARGET/zips/$ZIP_DIR/"

	#
	# The build/SUBPLATFORM/ folder should be moved to the $MZX_TARGET/releases/ folder
	# if the platform has requested that updates be generated from this build. If
	# updates were requested, there additionally should be a file called "manifest.txt"
	# in the build path.
	#
	if [ -n "$MZX_UPDATE_BRANCH" ]; then
		if [ ! -d "build/$SUBPLATFORM" ]; then
			build_error "couldn't find build/$SUBPLATFORM/" 12
			return
		fi
		if ! find "build/$SUBPLATFORM" -name "manifest.txt" | grep -q "manifest.txt"; then
			build_error "couldn't find manifest.txt for $MZX_UPDATE_BRANCH/$SUBPLATFORM" 13
			return
		fi
		mkdir -p "$MZX_TARGET/releases/$MZX_UPDATE_BRANCH/"
		rm -rf "$MZX_TARGET/releases/$MZX_UPDATE_BRANCH/$SUBPLATFORM/"
		mv "build/$SUBPLATFORM/" "$MZX_TARGET/releases/$MZX_UPDATE_BRANCH/"
	fi

	#
	# Clean the working tree.
	#
	$MZX_MAKE distclean
}

# $1 subplatform
# $2 tag or branch
# $3 update branch (optional)
# $4 extra configuration flags (optional)
build_release()
{
	build_common "$1" "$2" "$3" "$4" "release"
}

# $1 subplatform
# $2 tag or branch
# $3 update branch (optional)
# $4 extra configuration flags (optional)
build_debug()
{
	build_common "$1" "$2" "$3" "$4" "debug"
}
