# $1 - x86 or x64
# $2 - MINGW32 or MINGW64 

MSYSTEM=$2
. /etc/profile

# Build XMP manually until it's available from pacman

if [ "$1" = "x86" ]
then
  target=i586-mingw32
else
  target=x86_64-mingw32
fi

cd /mzx-build-workingdir/megazeux/contrib/libxmp

./configure --build=$target --enable-static
make -j8
mv lib/libxmp.a lib/libxmp$1.a
rm lib/libxmp*dll*
