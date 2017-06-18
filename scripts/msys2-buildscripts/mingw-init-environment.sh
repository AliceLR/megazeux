MSYSTEM=MSYS
. /etc/profile

pacman --needed --noconfirm -S git make tar curl zip mingw-w64-{x86_64,i686}-{zlib,gcc,libpng,libogg,libvorbis,SDL2}
rm -rf /mingw{32,64}/lib/lib{ogg,png16,vorbis,vorbisfile,z}.dll.a /mingw64/x86_64-w64-mingw32/lib/lib{pthread,winpthread}.dll.a /mingw32/i686-w64-mingw32/lib/lib{pthread,winpthread}.dll.a
mkdir -p /mzx-build-workingdir
cd /mzx-build-workingdir
git clone $1
