#!/bin/bash

# Perform a check to see if a nightly update build is required.
# WARNING: Will hard reset, fetch, and pull to the working repository.

cd /mzx-build-workingdir/megazeux
git reset --hard
git checkout master
git fetch

CURRENT=`git rev-parse @`
REMOTE=`git rev-parse @{u}`

if [ $CURRENT = $REMOTE ]; then
  echo "No updates required."
  exit 1
fi

git pull
exit 0
