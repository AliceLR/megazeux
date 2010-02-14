#!/bin/sh

if [ "$1" = "" ]; then
	echo "usage: $0 [build dir]"
	exit 0
fi

cd $1

rm -f manifest.txt

for file in `find -mindepth 1 -type f -not -name config.txt \
                                      -not -name pad.config | sort`; do
	size=`du -b $file | cut -f1`
	sha256sum=`sha256sum -b $file | cut -f1 -d\ `
	echo $sha256sum $size `echo $file | sed 's,\.\/,,'` \
		>> manifest.txt
done
