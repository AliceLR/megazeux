#!/bin/sh

export MZX_SCRIPTS_BASE=$(cd $(dirname $0); pwd)
export MZX_SCRIPTS="$MZX_SCRIPTS_BASE/mzx-scripts"
export MZX_WORKINGDIR="$MZX_SCRIPTS_BASE/mzx-workingdir"
export MZX_TARGET="TARGET/"

source $MZX_SCRIPTS/version.sh
source $MZX_SCRIPTS/build.sh

#
# Script to check if updated debug builds are required. If they are, build them
# and push updates files for them.
#
build_init
if ! build_check_branch_updates $UNSTABLE $DEBYTECODE; then exit 0; fi

#
# Clear out the old release dir so the updates script doesn't process anything
# that's already uploaded.
#
rm -rf "$MZX_TARGET/releases"

# Type          Subplatform     Branch        Update Branch         Extra config flags
# -----------   -------------   -----------   -------------------   ------------------
build_debug     "windows-x86"   $UNSTABLE     $UNSTABLE_UPDATES
build_debug     "windows-x86"   $DEBYTECODE   $DEBYTECODE_UPDATES   "--enable-debytecode"
build_debug     "windows-x64"   $UNSTABLE     $UNSTABLE_UPDATES
build_debug     "windows-x64"   $DEBYTECODE   $DEBYTECODE_UPDATES   "--enable-debytecode"

cd "$MZX_SCRIPTS_BASE"

./mzx-updates.sh
[ "$?" -eq "0" ] || { exit 1; }

./mzx-upload.sh
[ "$?" -eq "0" ] || { exit 1; }
