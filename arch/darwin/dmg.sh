#!/bin/sh

TARGET=$1

BASEDIR=build/dist/darwin
DMGNAME=${BASEDIR}/${TARGET}.dmg
VOLNAME=MegaZeux

mkdir -p ${BASEDIR}
hdiutil create $DMGNAME -size 50m -fs HFS+ \
                        -volname "$VOLNAME" -layout SPUD &&
DEV_HANDLE=`hdid $DMGNAME | grep Apple_HFS | \
            perl -e '\$_=<>; /^\\/dev\\/(disk.)/; print \$1'`
ditto -rsrcFork build/darwin-dist /Volumes/$VOLNAME &&
hdiutil detach $DEV_HANDLE &&

hdiutil convert $DMGNAME -format UDZO -o $DMGNAME.compressed &&
mv -f $DMGNAME.compressed.dmg $DMGNAME
