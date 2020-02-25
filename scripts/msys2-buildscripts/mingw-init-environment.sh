MSYSTEM=MSYS
. /etc/profile

echo "Y" | pacman -Syu
pacman --needed --noconfirm -S git make tar curl zip mingw-w64-{x86_64,i686}-{zlib,gcc,libpng,libogg,libvorbis,SDL2}
mkdir -p /mzx-build-workingdir
cd /mzx-build-workingdir
git clone $1
/mingw-sdl-init.sh
