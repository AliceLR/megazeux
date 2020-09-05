#!/bin/sh

#
# Common checks and defines for the buildscripts.
#

if git ls-files --error-unmatch "$0" "$MZX_SCRIPTS" >/dev/null 2>&1; then
	echo "ERROR: attempting to run this script from 'scripts/'."
	echo "The scripts directory is tracked by Git and may be modified when"
	echo "checking out different tags/branches for MZX builds."
	echo ""
	echo "You should duplicate 'scripts/buildscripts/' to another directory and"
	echo "run this script from that directory instead."
	exit 1
fi

mzx_log()
{
	echo "$1" 1>&2
}

mzx_warn()
{
	echo "WARN:" "$1" "(code $2)" 1>&2
}

mzx_error()
{
	echo "ERROR:" "$1" "(code $2)" 1>&2
}

# $@ commands to check for (e.g. "7za")
cmd_check()
{
	MSG=""
	while [ -n "$1" ]; do
		if ! command -v "$1" >/dev/null 2>&1; then
			MSG="$MSG$1;"
		fi
		shift
	done
	if [ -n "$MSG" ]; then
		ERRNO="CMD:$MSG"
	fi
}
