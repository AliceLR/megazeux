#!/bin/bash

if [ "$1" = "" ]; then
	echo "usage: $0 <platform> [<sourcedir>]"
	exit 0
else
	PLATFORM="$1"
	shift
fi

BASEDIR=`dirname $0`

if [ ! -f "$BASEDIR/$PLATFORM.sh" ]; then
	echo "Unknown port: $PLATFORM"
	exit 0
fi

. "$BASEDIR/global.sh"
. "$BASEDIR/$PLATFORM.sh"

if [ "$1" = "" ]; then
	pushd $HOME/megazeux >/dev/null
else
	pushd $1 >/dev/null
	shift
fi

LOGDIR=build/logs/$PLATFORM
rm -rf build/$PLATFORM build/dist/$PLATFORM $LOGDIR
mkdir -p $LOGDIR

export ERRNO=0

platform_enter_hook

./config.sh $CONFIG_FLAGS 2>&1 | tee $LOGDIR/config.log

make -j3 $BUILD_FLAGS 2>&1 | tee $LOGDIR/build.log
platform_build_test_hook

if [ "$ERRNO" = "0" ]; then
	make archive 2>&1 | tee $LOGDIR/archive.log
	platform_archive_test_hook

	if [ "$ERRNO" != "0" ]; then
		echo "Archive test failed, aborting.."
	fi
else
	echo "Build test failed, aborting.."
fi

make distclean >/dev/null 2>&1
platform_exit_hook

popd >/dev/null

exit $ERRNO
