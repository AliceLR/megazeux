#!/bin/sh

if [ "$1" == "" ]; then
	echo "usage: $0 zipfile"
	exit 0
fi

SRCZIP=$PWD/$1
shift

[ -d /tmp/split ] && rm -rf /tmp/split
mkdir -p /tmp/split/debug
mkdir -p /tmp/split/release

cd /tmp/split/debug
unzip $SRCZIP *.debug manifest.txt
7za -tzip a $SRCZIP.debug *

cd /tmp/split/release
rm -rf /tmp/split/debug

unzip $SRCZIP
egrep -v ".debug$" manifest.txt >manifest.txt.new
mv manifest.txt.new manifest.txt
find -name "*.debug" -exec rm '{}' ';'
7za -tzip a $SRCZIP.release *

rm -rf /tmp/split
