#!/bin/bash
mkdir -p build/android
cd build/android

for i in arm arm64 x86 x86_64; do
	PLATFORM=android-16
	if [[ "$i" == "x86_64" || "$i" == "arm64" ]]; then
		PLATFORM=android-21
	fi

	"$NDK_PATH"/build/tools/make-standalone-toolchain.sh --install-dir=toolchain-"$i"/ --platform="$PLATFORM" --arch="$i"
done
