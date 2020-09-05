#!/bin/sh

export MZX_SCRIPTS="`dirname $0`/mzx-scripts"
export MZX_WORKINGDIR="`dirname $0`/mzx-workingdir"
export MZX_TARGET="TARGET/"

source $MZX_SCRIPTS/setup.sh

setup_environment windows-x64 windows-x86 nds 3ds wii switch psp
