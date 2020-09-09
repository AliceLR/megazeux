#!/bin/sh

#
# Script to run shellcheck on these scripts to check for portability issues
# and errors.
#
# Ignored warnings:
#
# SC1090 - Can't follow non-constant source--no way to fix this without
#          polluting the scripts with directive comments.
#          v0.7.2 is supposed to be able to work around this with the -P flags,
#          but Fedora has v0.7.0 so I can't really verify right now.
# SC1091 - Can't follow external file references. Don't want to anyway, since
#          these might not even be for this platform.
# SC2155 - Avoid masking return values for "export" doesn't matter because this
#          code never checks the return value of "export".
#
shellcheck \
 -e "SC2155" -e "SC1090" -e "SC1091" \
 -P "mzx-scripts/" \
 -P "mzx-scripts/platforms/" \
 ./*.sh \
 mzx-scripts/*.sh \
 mzx-scripts/platforms/*.sh
