#!/bin/bash

cd "$(dirname "$0")"
mkdir -p log

# Force mzxrun to use the libraries in its directory instead of any installed libraries.
export LD_LIBRARY_PATH=".."

# Coupled with the software renderer, this will disable video in MZX, speeding things up
# and allowing for automated testing.
export SDL_VIDEODRIVER=dummy

# Standalone mode will allow tests.mzx to terminate MZX and no_titlescreen mode
# simplifies things. Disable auto update checking to save time.

mzxrun="../mzxrun \
  tests.mzx \
  video_output=software \
  update_auto_check=off \
  standalone_mode=1 \
  no_titlescreen=1"

# Run mzxrun and attempt to detect a hang (e.g. an error occurred).

${mzxrun} &
mzxrun_pid=$!
disown

i="0"

while ps -sp $mzxrun_pid | grep -q $mzxrun_pid
do
	sleep 1
	i=$[$i+1]
	printf "."
	if [ $i -ge 10 ];
	then
		kill -9 $mzxrun_pid
		echo "killing frozen process."
		break
	fi
done
echo ""

# Color code PASS/FAIL tags and important numbers.

COL_END="`tput sgr0`"
COL_RED="`tput bold``tput setaf 1`"
COL_GREEN="`tput bold``tput setaf 2`"
COL_YELLOW="`tput bold``tput setaf 3`"

if [ -z $1 ] || [ $1 != "-q" ];
then
	echo "$(\
	  cat log/failures \
	  | sed --expression="s/\[PASS\]/\[${COL_GREEN}PASS${COL_END}\]/g" \
	  | sed --expression="s/\[FAIL\]/\[${COL_RED}FAIL${COL_END}\]/g" \
	  | sed --expression="s/\[[?][?][?][?]]/\[${COL_YELLOW}????${COL_END}\]/g" \
	  | sed --expression="s/passes: \([1-9][0-9]*\)/passes: ${COL_GREEN}\1${COL_END}/g" \
	  | sed --expression="s/failures: \([1-9][0-9]*\)/failures: ${COL_RED}\1${COL_END}/g" \
	  )"

	tput bel
fi

# Exit 1 if there are any failures.

if [ ! -e log/failures ] || grep -q "FAIL" log/failures;
then
	exit 1
else
	exit 0
fi
