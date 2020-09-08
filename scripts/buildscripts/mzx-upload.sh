#!/bin/sh

export MZX_SCRIPTS_BASE="$(cd "$(dirname "$0")" || true; pwd)"
export MZX_SCRIPTS="${MZX_SCRIPTS_BASE%/}/mzx-scripts"
export MZX_WORKINGDIR="${MZX_SCRIPTS_BASE%/}/mzx-workingdir"
export MZX_TARGET_BASE="$(pwd)"
export MZX_TARGET="${MZX_TARGET_BASE%/}/TARGET"

. "$MZX_SCRIPTS/uploads.sh"

#
# Add lines to configure upload targets here.
#
# "upload_curl" will POST the uploads via curl to a remote HTTP script.
#
# "upload-ssh" will transfer the uploads via scp and then extract them via ssh.
# Note that upload-ssh will upload and extract the .tar file directly into the
# user directory.
#

# upload_curl "http://Put the URL of your upload PHP script here/.php"
# upload_ssh  "username@example.com"
