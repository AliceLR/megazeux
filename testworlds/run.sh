#!/bin/sh

cd "$(dirname "$0")"
mkdir -p log

# Standalone mode will allow tests.mzx to terminate MZX and no_titlescreen mode
# simplifies things. Use the software renderer because we don't care about video
# output performance.

SDL_VIDEODRIVER=dummy ../mzxrun \
  video_output=software \
  standalone_mode=1 \
  no_titlescreen=1 \
  tests.mzx

# Color code PASS/FAIL tags and important numbers.

echo -e "$(\
  cat log/failures \
  | sed --expression='s/\[PASS\]/\[\\e\[92m\\e\[1mPASS\\e\[0m\]/g' \
  | sed --expression='s/\[FAIL\]/\[\\e\[91m\\e\[1mFAIL\\e\[0m\]/g' \
  | sed --expression='s/\[[?][?][?][?]]/\[\\e\[93m\\e\[1m????\\e\[0m\]/g' \
  | sed --expression='s/passes: \([1-9][0-9]*\)/passes: \\e\[92m\\e\[1m\1\\e\[0m/g' \
  | sed --expression='s/failures: \([1-9][0-9]*\)/failures: \\e\[91m\\e\[1m\1\\e\[0m/g' \
  )"

# The pizza's done!

echo -ne "\007"
