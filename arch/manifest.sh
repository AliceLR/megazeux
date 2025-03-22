#!/bin/sh

set -e
if [ "$1" = "" ]; then
	echo "usage: $0 [build dir]"
	exit 0
fi

echo "Generating manifest..."

cd "$1"

rm -f manifest.txt

# -P24 is a GNU-ism but also the entire point of using xargs here.
# Most BSD xargs seem to support it; BusyBox gracefully ignores it.
find . -mindepth 1 -type f				\
	! -name config.txt				\
	! -name manifest.txt				\
	! -name pad.config				\
	! -name '*.debug' 				\
	-print0						\
| xargs -0 -n1 -P24 /bin/sh -c				\
	'size=$(du -b "$1" | cut -f1);			\
	 sha=$(sha256sum -b "$1" | cut -f1 -d" ");	\
	 name=$(echo "$1" | sed "s,\\.\\/,,");		\
	 echo "$sha" "$size" "$name"' "$0"		\
| sort -k3						\
>> manifest.txt
