# Script to add caverns to all applicable release zips.
#!/bin/bash

URL="https://www.digitalmzx.com/download/182/26f59999fc55953e81a66cf01d0207fe3b124387a4a4c9288a155f6a44134641/"
PREFIX="/mzx-build-workingdir/caverns"

CAVERNS_PATH="caverns"
CAVERNS_MMUTIL_PATH="caverns_mmutil"

_3DS_PATH="3ds"
NDS_PATH="nds"
WII_PATH="wii"
SWITCH_PATH="switch"

_3DS_DIR="3ds/megazeux"
NDS_DIR="games/megazeux"
WII_DIR="apps/megazeux"
SWITCH_DIR="switch/megazeux"


mkdir -p $PREFIX

if [ ! -f "$PREFIX/caverns.zip" ]; then
	wget $URL -O $PREFIX/caverns.zip
fi

if [ ! -d "$PREFIX/$CAVERNS_PATH" ]; then
	7z e $PREFIX/caverns.zip -o$PREFIX/$CAVERNS_PATH
fi

if [ ! -z "$DEVKITPRO" -a ! -d "$PREFIX/$CAVERNS_MMUTIL_PATH" ]; then
	7z e $PREFIX/caverns.zip -o$PREFIX/$CAVERNS_MMUTIL_PATH
	pushd $PREFIX/$CAVERNS_MMUTIL_PATH
	for f in *.{mod,MOD}; do
		$DEVKITPRO/tools/bin/mmutil -d -m $f && rm -f $f
	done
	popd
fi


function setup_arch
{
	# A pretty convoluted directory structure is required to get 7za to
	# put anything where we want it to go.

	# $1 Arch folder
	# $2 Directory to put caverns in
	# $3 Caverns path to copy from (optional, defaults to $CAVERNS_PATH)

	CAVERNS_SETUP=$CAVERNS_PATH
	if [ ! -z $3 ]; then
		CAVERNS_SETUP=$3
	fi

	if [ ! -d "$PREFIX/$1" ]; then

		mkdir -p $PREFIX/$1/$2
		cp -r $PREFIX/$CAVERNS_SETUP/* $PREFIX/$1/$2

	fi
}

setup_arch $_3DS_PATH $_3DS_DIR
setup_arch $NDS_PATH $NDS_DIR $CAVERNS_MMUTIL_PATH
setup_arch $WII_PATH $WII_DIR
setup_arch $SWITCH_PATH $SWITCH_DIR


function locate_mzx
{
	# $1 - archive to search
	# $2 - filename to check for

	return $(7za l $1 $2 -r | grep -q "$2")
}


function add_caverns
{
	# 7za actually requires changing to the directory for the copy to not
	# include the containing directory. No, really.

	# $1 - Archive
	# $2 - Arch folder

	pushd $PREFIX/$2 > /dev/null

	7za a $1 * -y -bsp0 -bso0

	popd > /dev/null
}

function check_file
{
	# $1 the file to check for different MegaZeux releases.

	if $(locate_mzx $1 "mzxrun.exe"); then
		echo "Found mzxrun.exe in $1; adding caverns to Windows at /"
		add_caverns $1 $CAVERNS_PATH

	elif $(locate_mzx $1 "mzxrun.cia"); then
		echo "Found mzxrun.cia in $1; adding caverns to 3DS at $_3DS_DIR"
		add_caverns $1 $_3DS_PATH

	elif $(locate_mzx $1 "mzxrun.nds"); then
		echo "Found mzxrun.nds in $1; adding caverns to NDS at $NDS_DIR"
		add_caverns $1 $NDS_PATH

	elif $(locate_mzx $1 "boot.dol"); then
		echo "Found boot.dol in $1; adding caverns to Wii at $WII_DIR"
		add_caverns $1 $WII_PATH

	elif $(locate_mzx $1 "megazeux.nro"); then
		echo "Found megazeux.nro in $1; adding caverns to Switch at $SWITCH_DIR"
		add_caverns $1 $SWITCH_PATH

	elif $(locate_mzx $1 "EBOOT.PBP"); then
		echo "Found EBOOT.PBP in $1; adding caverns to PSP at /"
		add_caverns $1 $CAVERNS_PATH

	fi
}


for f in $(find /mzx-build-workingdir/zips -name "*.zip"); do

	check_file $f &

done

wait
