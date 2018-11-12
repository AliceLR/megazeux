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
  # Only handle types/architectures if their given directories actually exist.
  if [ -d "$T" ]; then
    pushd $T
    for A in $archs
    do
      if [ -d "$A" ]; then
        pushd $A
        /mzx-build-scripts/builddir-to-updatedir.sh &
        #/mingw-release-single.sh $T $A &
        popd
      fi
    done
    popd
  fi
  # Add each type to the updates file regardless of whether or not it was
  # processed. This way, individual release tags can be updated without
  # breaking other existing release tags.
  echo "Current-$1: $T" >> updates-uncompressed.txt
  shift
done
wait
/mzx-build-scripts/releasedir-to-tar.sh
popd
