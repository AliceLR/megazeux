#!/bin/sh

set -e
if [ "$1" = "" ]; then
	echo "usage: $0 [build dir]"
	exit 0
fi

if command -v sha256sum >/dev/null 2>/dev/null; then
	export sha256cmd="sha256sum -b"
elif command -v gsha256sum >/dev/null 2>/dev/null; then
	export sha256cmd="gsha256sum -b"
elif command -v sha256 >/dev/null 2>/dev/null; then
	export sha256cmd="sha256 -q"
elif command -v shasum >/dev/null 2>/dev/null; then
	export sha256cmd="shasum -a256 -b"
else
	echo "error: install sha256sum (coreutils, BusyBox) or shasum (perl)."
	exit 1
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
	'size=$(($(wc -c <"$1")));			\
	 sha=$($sha256cmd "$1" | cut -f1 -d" ");	\
	 name=$(echo "$1" | sed "s,\\.\\/,,");		\
	 echo "$sha" "$size" "$name"' "$0"		\
| sort -k3						\
>> manifest.txt
