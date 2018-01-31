#!/bin/sh

mkdir -p log

../mzxrun \
  video_output=software \
  standalone_mode=1 \
  no_titlescreen=1 \
  tests.mzx

echo -e "$(\
  cat log/failures \
  | sed --expression='s/\[PASS\]/\[\\e\[92m\\e\[1mFAIL\\e\[0m\]/g' \
  | sed --expression='s/\[FAIL\]/\[\\e\[91m\\e\[1mFAIL\\e\[0m\]/g' \
  | sed --expression='s/failures: ([0-9]*[1-9]|[0-9]+0)/failures: \\e\[90m\\e\[1m$1\\e\[0m/g' \
  )"
