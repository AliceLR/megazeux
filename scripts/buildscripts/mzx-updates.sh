#!/bin/sh

export MZX_SCRIPTS_BASE=$(cd $(dirname $0); pwd)
export MZX_SCRIPTS="$MZX_SCRIPTS_BASE/mzx-scripts"
export MZX_WORKINGDIR="$MZX_SCRIPTS_BASE/mzx-workingdir"
export MZX_TARGET="TARGET/"

source $MZX_SCRIPTS/version.sh
source $MZX_SCRIPTS/updates.sh

process_updates "Stable:$STABLE_UPDATES" "Unstable:$UNSTABLE_UPDATES" "Debytecode:$DEBYTECODE_UPDATES"
