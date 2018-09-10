#!/bin/bash
PATH=$NDK_PATH:$PATH

mkdir -p build/android
if [ -d build/android/out ]; then
	rm -r build/android/out
fi
mkdir -p build/android/out

for i in arm arm64 x86 x86_64; do
	./config.sh --platform android --enable-release --disable-utils --disable-libpng \
	  --prefix "$(realpath ./build/android/toolchain-$i/sysroot/usr)"
	make -j1 clean
	ARCH="$i" make
	cp "$1" build/android/out/"$1"-"$i".so
done

mkdir -p arch/android/project/app/jni/lib
mkdir -p arch/android/project/app/jni/lib/armeabi-v7a
mkdir -p arch/android/project/app/jni/lib/arm64-v8a
mkdir -p arch/android/project/app/jni/lib/x86
mkdir -p arch/android/project/app/jni/lib/x86_64

cp build/android/out/"$1"-arm.so arch/android/project/app/jni/lib/armeabi-v7a/libmain.so
cp build/android/out/"$1"-arm64.so arch/android/project/app/jni/lib/arm64-v8a/libmain.so
cp build/android/out/"$1"-x86.so arch/android/project/app/jni/lib/x86/libmain.so
cp build/android/out/"$1"-x86_64.so arch/android/project/app/jni/lib/x86_64/libmain.so
