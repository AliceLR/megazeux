#!/bin/bash
# Set up the 3DS portlibs repo that the 3DS and Wii ports need.

REPO=https://github.com/asiekierka/3ds_portlibs.git
PORTLIBSDIR=/mzx-build-workingdir/3ds_portlibs

rm -rf $PORTLIBSDIR
git clone $REPO $PORTLIBSDIR
