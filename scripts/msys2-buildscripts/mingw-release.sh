# eg. "2.90 debytecode master" "windows-x64 windows-x86" Stable Debytecode Unstable
# $1: types   e.g. "2.90 debytecode master"
# $2: archs   e.g. "windows-x64 windows-x86"
# $3: update of first type  e.g. "Stable"
# $4: update of second type  e.g. "Debytecode"
# $5: update of third type  e.g. "Unstable"
# .. and so on.
types="$1"
archs="$2"
#uploadurl="$3"
shift
shift
MSYSTEM=MSYS
. /etc/profile

cd /mzx-build-workingdir
mkdir -p uploads
rm -rf releases-update
cp -r releases releases-update
pushd releases-update
for T in $types
do
  pushd $T
  for A in $archs
  do
    pushd $A
    /mzx-build-scripts/builddir-to-updatedir.sh &
    #/mingw-release-single.sh $T $A &
    popd
  done
  popd
  echo "Current-$1: $T" >> updates-uncompressed.txt
  shift
done
wait
/mzx-build-scripts/releasedir-to-tar.sh
popd
