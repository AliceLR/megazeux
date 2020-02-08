#!/bin/bash

if [ "$1" = "" ]; then
	echo "usage: $0 [build dir]"
	exit 0
fi

function getManifestEntry
{
	size=`du -b $1 | cut -f1`
	sha256sum=`sha256sum -b $1 | cut -f1 -d\ `
	echo $sha256sum $size `echo $1 | sed 's,\.\/,,'`
}

# can't execute the above function via xargs unless it is exported
export -f getManifestEntry

echo "Generating manifest..."

cd $1

rm -f manifest.txt

find -mindepth 1 -type f				\
	-not -name config.txt				\
	-not -name manifest.txt				\
	-not -name pad.config				\
	-not -name '*.debug' 				\
	-print0						\
| xargs -0 -n1 -I{} -P24 bash -c 'getManifestEntry {}'	\
| sort --key=3						\
>> manifest.txt
