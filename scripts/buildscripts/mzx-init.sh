#!/bin/sh

export MZX_SCRIPTS_BASE="$(cd "$(dirname "$0")" || true; pwd)"
export MZX_SCRIPTS="${MZX_SCRIPTS_BASE%/}/mzx-scripts"
export MZX_WORKINGDIR="${MZX_SCRIPTS_BASE%/}/mzx-workingdir"
export MZX_TARGET_BASE="$(pwd)"
export MZX_TARGET="${MZX_TARGET_BASE%/}/TARGET"

. "$MZX_SCRIPTS/setup.sh"

setup_environment windows-x64 windows-x86 nds 3ds wii switch psp android emscripten
