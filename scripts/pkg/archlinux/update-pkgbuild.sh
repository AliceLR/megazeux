#!/usr/bin/env bash
# Produce a working PKGBUILD for the AUR from a template given a tarball.
# Requirements: mzx source tarball named in typical megazeux fashion like so:  mzx(version)src.tar.xz, bash, getopt.
# e.g., mzx291gsrc.tar.xz

MYFN=`basename "$0"`
_V=0

function usage () {
	cat <<EOUSAGE
Usage:
 $MYFN <path>
 
Creates a PKGBUILD by sha256summing specified MegaZeux tarball and editing a template.

Options:
 -h, --help    display this help and exit"

EOUSAGE
}

function log () {
	if [[ $_V -eq 1 ]]; then
		echo "$@"
	fi
}

while getopts :hv opt ; do
	case $opt in
		v)
			_V=1
			;;
		h)
			usage
			exit 1
			;;
		*)
			echo "Invalid option: -$OPTARG" >&2
			usage
			exit 2
			;;
	esac
done
shift $((OPTIND - 1))
if [[ "$1" == "" ]]
then
	usage
	exit 1
else
	log "Generating PKGBUILD..."
	sha256sum=($(sha256sum "$1"))
	log "Our checksum is $sha256sum"
	regex="mzx([0-9])([0-9a-z]+)src\.tar.+"
	if [[ $1 =~ $regex ]]
	then
		version="${BASH_REMATCH[1]}.${BASH_REMATCH[2]}"
		log "Our version is $version"
		cp PKGBUILD.template PKGBUILD
		sed -i "s/pkgver=UNSET/pkgver=$version/g" PKGBUILD
		sed -i "s/_sha256sum=UNSET/_sha256sum=$sha256sum/g" PKGBUILD
		echo "Done."
	else
		echo "Error: Tarball does not appear to be MegaZeux. Read script for requirements."
	fi
fi
