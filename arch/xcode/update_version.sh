#!/bin/sh

VERSION=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' MegaZeux/Info.plist)
echo "#define VERSION \"${VERSION}\"" > version.h
echo "#define VERSION_DATE \" (`date -u +%Y%m%d`)\"" >> version.h

