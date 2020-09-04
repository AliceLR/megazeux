#!/bin/sh

export MZX_SCRIPTS_BASE="`dirname $0`/mzx-scripts"
export MZX_BUILD_DIR="."
export MZX_TARGET="TARGET/"

source $MZX_SCRIPTS_BASE/version.sh
source $MZX_SCRIPTS_BASE/build.sh

build_init

# Type          Subplatform     Branch        Update Branch         Extra config flags
# -----------   -------------   -----------   -------------------   ------------------
build_release   "windows-x86"   $STABLE       $STABLE_UPDATES
build_debug     "windows-x86"   $UNSTABLE     $UNSTABLE_UPDATES
build_debug     "windows-x86"   $DEBYTECODE   $DEBYTECODE_UPDATES   "--enable-debytecode"
build_release   "windows-x64"   $STABLE       $STABLE_UPDATES
build_debug     "windows-x64"   $UNSTABLE     $UNSTABLE_UPDATES
build_debug     "windows-x64"   $DEBYTECODE   $DEBYTECODE_UPDATES   "--enable-debytecode"
build_release   "nds"           $STABLE
build_release   "3ds"           $STABLE
build_release   "wii"           $STABLE
build_release   "wiiu"          $STABLE
build_release   "switch"        $STABLE
build_release   "psp"           $STABLE
build_release   "psvita"        $STABLE
build_release   "emscripten"    $STABLE
build_release   "android"       $STABLE
build_release   "djgpp"         $STABLE
build_release   "source"        $STABLE
