#!/bin/sh

cd "$(dirname "$0")"
mkdir -p log

# Standalone mode will allow tests.mzx to terminate MZX and no_titlescreen mode
# simplifies things. Use the software renderer because we don't care about video
# output performance.

../mzxrun \
  video_output=software \
  standalone_mode=1 \
  no_titlescreen=1 \
  tests.mzx

# Color code PASS/FAIL tags and important numbers.

if [ -z $1 ] || [ $1 != "-q" ];
then
	echo -e "$(\
	  cat log/failures \
	  | sed --expression='s/\[PASS\]/\[\\e\[92m\\e\[1mPASS\\e\[0m\]/g' \
	  | sed --expression='s/\[FAIL\]/\[\\e\[91m\\e\[1mFAIL\\e\[0m\]/g' \
	  | sed --expression='s/\[[?][?][?][?]]/\[\\e\[93m\\e\[1m????\\e\[0m\]/g' \
	  | sed --expression='s/passes: \([1-9][0-9]*\)/passes: \\e\[92m\\e\[1m\1\\e\[0m/g' \
	  | sed --expression='s/failures: \([1-9][0-9]*\)/failures: \\e\[91m\\e\[1m\1\\e\[0m/g' \
	  )"

	echo -ne "\007"
fi

# Exit 1 if there are any failures.

if grep -q "FAIL" log/failures;
then
	exit 1
else
	exit 0
fi
