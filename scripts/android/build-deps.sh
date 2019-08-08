#!/bin/bash
PATH=$NDK_PATH:$PATH
SDL_VERSION=2.0.9
LIBOGG_VERSION=1.3.3
LIBVORBIS_VERSION=1.3.6

[ -z "$NDK_PATH" ] && { echo "NDK_PATH must be set! Aborting."; exit 1; }

if [ -z "$MSYSTEM" ]; then
        NDK_BUILD=ndk-build
else
        NDK_BUILD="$NDK_PATH/ndk-build.cmd"
fi

cd build/android
if [ ! -f SDL2-"$SDL_VERSION".tar.gz ]; then
	wget https://www.libsdl.org/release/SDL2-"$SDL_VERSION".tar.gz
fi
if [ ! -f libogg-"$LIBOGG_VERSION".tar.gz ]; then
	wget http://downloads.xiph.org/releases/ogg/libogg-"$LIBOGG_VERSION".tar.gz
fi
if [ ! -f libvorbis-"$LIBVORBIS_VERSION".tar.gz ]; then
	wget http://downloads.xiph.org/releases/vorbis/libvorbis-"$LIBVORBIS_VERSION".tar.gz
fi
tar xzvf SDL2-"$SDL_VERSION".tar.gz
tar xzvf libogg-"$LIBOGG_VERSION".tar.gz
tar xzvf libvorbis-"$LIBVORBIS_VERSION".tar.gz

if [ -d deps ]; then rm -r deps; fi

mkdir deps
cd deps
mv ../SDL2-"$SDL_VERSION" SDL2
mv ../libogg-"$LIBOGG_VERSION" libogg
mv ../libvorbis-"$LIBVORBIS_VERSION" libvorbis
cp ../../../arch/android/project/app/jni/Android.mk .
cp ../../../scripts/android/libogg-Android.mk libogg/Android.mk
cp ../../../scripts/android/config_types.h libogg/include/ogg/config_types.h
cp ../../../scripts/android/libvorbis-Android.mk libvorbis/Android.mk
mkdir out

$NDK_BUILD \
  NDK_PROJECT_PATH=. NDK_OUT=out NDK_LIBS_OUT=out \
  APP_BUILD_SCRIPT=Android.mk APP_ABI="armeabi-v7a arm64-v8a x86 x86_64" \
  APP_PLATFORM=android-16

cp out/armeabi-v7a/*.so ../toolchain-arm/sysroot/usr/lib
cp out/arm64-v8a/*.so ../toolchain-arm64/sysroot/usr/lib
cp out/x86/*.so ../toolchain-x86/sysroot/usr/lib
cp out/x86_64/*.so ../toolchain-x86_64/sysroot/usr/lib

for i in arm arm64 x86 x86_64; do
	mkdir ../toolchain-"$i"/sysroot/usr/include/SDL2
	cp -Rv SDL2/include/* ../toolchain-"$i"/sysroot/usr/include/SDL2
	cp -Rv libogg/include/* ../toolchain-"$i"/sysroot/usr/include/
	cp -Rv libvorbis/include/* ../toolchain-"$i"/sysroot/usr/include/
done

for i in SDL2 libogg libvorbis; do
	if [ -d ../../../arch/android/project/app/jni/"$i" ]; then
		rm -r ../../../arch/android/project/app/jni/"$i"
	fi
	mv "$i" ../../../arch/android/project/app/jni/"$i"
done

cd ..
rm -r deps
