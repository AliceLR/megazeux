#!/bin/sh

if [ "$1" = "" -o "$2" = "" ]; then
	echo "usage: $0 <datestr> <revision> <outdir> [branch]"
	exit 0
fi

DATE=$1
shift

REVISION=$1
shift

SVNBASE=$HOME/megazeux

if [ "$2" = "" ]; then
	MZXBASE=$HOME/megazeux/trunk
	OUTDIR=$1/trunk-$DATE-$REVISION
else
	# If there's no branch by the user's name, assume they want to
	# override the output directory but continue to build trunk/.
	#
	if [ -d $HOME/megazeux/branch/$2 ]; then
		MZXBASE=$HOME/megazeux/branch/$2
	else
		MZXBASE=$HOME/megazeux/trunk
	fi
	OUTDIR=$1/$2-$DATE-$REVISION
fi

if [ ! -d $MZXBASE ]; then
	echo "Could not find specified SVN basedir, aborting."
	exit 1
fi

[ -d "$OUTDIR" ] && rm -rf $OUTDIR
mkdir -p $OUTDIR/source

pushd $MZXBASE >/dev/null

git reset --hard $REVISION >/dev/null 2>&1
if [ "$?" != "0" ]; then
	echo "git reset --hard failed; aborting."
	exit 1
fi

rm -rf build
make source >/dev/null 2>&1

SRCPKG=$MZXBASE/build/dist/source/*.tar.bz2
if [ -f "$SRCPKG" ]; then
	echo "Failed to compile source package; aborting."
	exit 2
fi

[ -d /tmp/megazeux ] && rm -rf /tmp/megazeux
mkdir /tmp/megazeux

for PLATFORM in amiga gp2x nds psp wii windows-x86 windows-x64; do
	tar -C/tmp/megazeux -jxf $SRCPKG

	pushd /tmp/megazeux/mzx* >/dev/null
	$SVNBASE/scripts/build.sh $PLATFORM $PWD >/dev/null 2>&1

	if [ -d build/dist/$PLATFORM ]; then
		mv build/dist/$PLATFORM /tmp/megazeux
	else
		mkdir /tmp/megazeux/$PLATFORM
	fi

	mv build/logs/$PLATFORM/* /tmp/megazeux/$PLATFORM

	popd >/dev/null

	rm -rf /tmp/megazeux/mzx*
done

pushd /tmp/megazeux >/dev/null

export CONCURRENCY_LEVEL=3

mkdir ubuntu

for CHROOT in ubuntu-i386 ubuntu-amd64; do
	tar -jxf $SRCPKG
	MZX=`echo mzx* | sed 's,^mzx,megazeux-,' | sed 's,-2,-2.,'`
	mv mzx* $MZX

	pushd $MZX >/dev/null
	dchroot -d -c $CHROOT -- debuild -us -uc >/dev/null 2>&1
	popd >/dev/null

	rm -rf $MZX
	mv megazeux* ubuntu
done

popd >/dev/null

mv $SRCPKG $OUTDIR/source
rm -rf build

mv /tmp/megazeux/* $OUTDIR
rm -rf /tmp/megazeux

popd >/dev/null
