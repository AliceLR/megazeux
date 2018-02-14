# $1 - x64 or x86
# $2 - win32 or win64
# $3 - extra config flags e.g. "--enable-debytecode" or ""
# $4 - 2.90 or master or debytecode
# $5 - branch name (e.g. build-testing)
# $6 - MINGW32 or MINGW64

MSYSTEM=$6
. /etc/profile
cd /mzx-build-workingdir/megazeux
git fetch
git checkout $5
git pull

./config.sh --platform $2 $3 --enable-release
make -j8 debuglink

rm -rf build

CUSTOM_SDL_DLL_DIR=contrib/precompiled-sdl/$1 make archive

/mingw-build-debug.sh $1

mkdir -p /mzx-build-workingdir/zips/$4
mv build/dist/windows-$1/* /mzx-build-workingdir/zips/$4

mkdir -p /mzx-build-workingdir/releases/$4
rm -rf /mzx-build-workingdir/releases/$4/windows-$1
mv build/windows-$1/ /mzx-build-workingdir/releases/$4/

make distclean
