#!/bin/sh

#
# Git branches for each update pin.
#
export STABLE="v2.92f"
export UNSTABLE="master"
export DEBYTECODE="master"

#
# Update branches for each update pin.
#
# Typically stable updates for each new MZX release and unstable/debytecode
# stay the same. In the case of MZX releases, this string must correspond
# exactly to the MZX version in version.inc.
#
export STABLE_UPDATES="2.92f"
export UNSTABLE_UPDATES="master"
export DEBYTECODE_UPDATES="debytecode"
