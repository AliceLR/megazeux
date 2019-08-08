#!/bin/bash
PATH=$NDK_PATH:$PATH

[ -z "$NDK_PATH" ] && { echo "NDK_PATH must be set! Aborting."; exit 1; }

mkdir -p build/android
if [ -d build/android/out ]; then
	rm -r build/android/out
fi
mkdir -p build/android/out

for i in arm arm64 x86 x86_64; do
	arch/android/CONFIG.ANDROID --prefix "$(realpath ./build/android/toolchain-$i/sysroot/usr)"
	make -j1 clean
	ARCH="$i" make -j8
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

# ensure JNI libs will get refreshed
rm -r arch/android/project/app/build

ASSETS_DIR=arch/android/project/app/src/main/res/raw
mkdir -p "$ASSETS_DIR"

ASSETS_ZIP="$ASSETS_DIR/assets.zip"
if [ -f "$ASSETS_ZIP" ]; then
	rm "$ASSETS_ZIP"
fi
zip -9 -r "$ASSETS_ZIP" assets/ config.txt
