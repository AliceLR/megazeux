#!/bin/bash

# Use make test
if [ -z $1 ] || [ $1 = "unix" -a -z $2 ]; then
       echo "USAGE: ./run.sh {PLATFORM} {LIBDIR} (or use make test)";
       exit 1
fi

# On unix platforms, installed libraries will override the ones we want used.
# The user has to manually remove or rename them to test if they aren't correct.
if [ $1 = "unix" ] && [ $2 != "." ] && [ -e "$2/libcore.so" ]; then
       diff -u libcore.so $2/libcore.so
       if [ $? -ne 0 ]; then
               echo "ERROR: Remove or update $2/libcore.so and try again";
               exit 1;
       fi
fi

# Unix release builds will try to find this if it isn't installed.
ln -s config.txt megazeux-config

# Give tests.mzx the MZX configuration so it can decide which tests to skip.
cp src/config.h testworlds

cd "$(dirname "$0")"
mkdir -p log

# Clear out any backup files so they aren't mistaken for tests.
rm -f backup*.mzx

# Clear out the temp folder.
find temp/* ! -name README.md -exec rm -f {} \;

# Clear out the default logs
rm -f log/detailed
rm -f log/summary
rm -f log/failures

# Force mzxrun to use the libraries in its own directory.
export LD_LIBRARY_PATH=".."

# Coupled with the software renderer, this will disable video in MZX, speeding things up
# and allowing for automated testing.
export SDL_VIDEODRIVER=dummy

# Standalone mode will allow tests.mzx to terminate MZX and no_titlescreen mode
# simplifies things. Disable auto update checking to save time.

../mzxrun \
  tests.mzx \
  video_output=software \
  update_auto_check=off \
  standalone_mode=1 \
  no_titlescreen=1 \
  audio_on=0 \
  pc_speaker_on=0 \
  &

# Attempt to detect a hang (e.g. an error occurred).

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

# Clean up some files that MegaZeux currently can't.

rm -f next
rm -f saved.sav
rm -f LOCKED.MZX
rm -f LOCKED.MZX.locked
rm -f ../megazeux-config
rm -f config.h

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
