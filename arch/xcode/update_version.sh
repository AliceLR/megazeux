#!/bin/sh
VERSION=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' MegaZeux/Info.plist)
VER_START=${VERSION:0:1}
NUM_MATCH='^[0-9]*$'

OUTPUT='#define VERSION'

if ! [[ $VER_START =~ $NUM_MATCH ]] ; then
	# If the version doesn't start with a number (i.e. 'git'), leave off the 'v').
	OUTPUT="$OUTPUT \"$VERSION\""
else
	OUTPUT="$OUTPUT \"v$VERSION\""
fi

echo "$OUTPUT" > version.h

