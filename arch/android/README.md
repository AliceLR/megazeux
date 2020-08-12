# Building MegaZeux for Android

NOTE: Only tested on Linux.

NOTE: Requires a few gigabytes of hard drive space and some patience.

This port is more complicated than the typical MegaZeux port and utilizes
a hybrid of MegaZeux's build system and a Gradle project. For best results,
follow this guide closely and ask for help on the MegaZeux Discord if you have
any problems.

While Android Studio can optionally be used to complete the final stages of the
build process, all other stages currently involve invoking Makefile targets
from the command line. This guide assumes you are comfortable with the command
line and GNU Make.

## Overview

The Android port of MegaZeux consists of the following components:

* MegaZeux built into native code as `libmain`.
* Native dependency libraries (`libSDL2`, `libogg`, `libvorbis`).
* The SDL Android project, which performs most required interactions with
  sensors/bluetooth/etc. and interfaces with the native code via JNI.
* MegaZeux-specific Java glue code to extract assets from the APK resources.

Each native library (MegaZeux + all dependency libraries) must be built for the
following ABIs:

| ABI           | Description               | Minimum API version |
|---------------|---------------------------|---------------------|
| `arm64-v8a`   | Modern 64-bit ARM devices | Lollipop 5.0 (API 21)
| `armeabi-v7a` | Older 32-bit ARM devices  | Jelly Bean 4.1 (API 16)
| `x86_64`      | Modern 64-bit x86 devices | Lollipop 5.0 (API 21)
| `x86`         | Older 32-bit x86 devices  | Jelly Bean 4.1 (API 16)

The build process for the MegaZeux port is as follows:

1) If the dependency libraries have not been built and installed to the Gradle
  project, they must be built and installed to `arch/android/project/app/jni/lib/${ABI}`.
  This is performed using the Makefile.
2) MegaZeux itself needs to be built as a library and installed to the Gradle project.
  This is also performed using the Makefile. The assets ZIP containing default
  charsets, shaders, etc. is also created and installed to
  `arch/android/project/app/src/main/res/raw/assets.zip` at this stage.
3) The Java portions of MZX/SDL are compiled and a debug or release APK is
  generated via the Gradle project. This can be invoked either by the Makefile
  or by Android Studio, but the prior two steps *must* have been performed
  for either option to work.

## Environment Setup

1. Install the latest version of both the Android SDK and NDK. The easiest way
  to do this is to install these through Android Studio. MegaZeux requires
  version 19 of the NDK or higher.
2. `export NDK_PATH=[the NDK path]`. As a hint, the directory should contain "ndk-build", among others.
  * For Android Studio users, this will be `/home/.../Android/Sdk/ndk-bundle/`
    Create the file `arch/android/project/local.properties` with the following lines:
```
ndk.dir=[NDK path here]
sdk.dir=[SDK path here]
```
3. If you haven't done so already, use `./config.sh --platform android` (or use
  the default configuration at `arch/android/CONFIG.ANDROID` instead).


## Building Dependencies

The Android port requires several libraries to be built before MegaZeux can be
built. Because each dependency needs to be built for four different ABIs,
MegaZeux provides several make targets for handling these dependencies:

| Target           | Description |
|------------------|-------------|
| `deps-build`     | Download and build dependency libraries as-needed.
| `deps-install`   | Invokes `make deps-build` and then installs the libraries to project JNI libraries folder.
| `deps-uninstall` | Removes the built dependency libraries from the project.
| `deps-clean`     | Delete the intermediate dependency build directory and all downloaded files (`arch/android/deps`).

The Android port currently relies on the following libraries:

* `libogg`
* `libvorbis`
* `libSDL2`

Currently, the Android port does not use libpng. The Android NDK contains
a pre-built static library of zlib, so it does not need to be built manually.

To update the downloaded version of any of the dependencies, see `arch/android/Makefile.deps`.


## Building MegaZeux

Instead of building with `make` or `make all`, MegaZeux uses meta targets to
manage the several builds of MegaZeux required for the Android port. The following
targets are provided:

| Target           | Description |
|------------------|-------------|
| `dist`           | Build of MegaZeux for all supported Android ABIs and install each build into the project JNI libraries folder. This will also create the assets.zip resource in the project's resources folder. To make this faster, it can be safely run with the -j# flag.
| `apk`            | Invoke 'gradlew build' to produce a release APK and copy it to `build/dist/android/`. This must be performed after `make dist`.
| `clean`          | Remove any leftover intermediate build files and perform various cleanup operations on the Gradle project.

Android Studio can be used in place of `make apk`. Release builds from Android
Studio are placed in `arch/android/project/app/release`.

## Known Issues

Several issues with this MegaZeux port have been reported. As this port is
effectively the same as every other SDL 2 port on the MZX side, most of these
issues seem to be compatibility issues between Android and SDL.

Issues **KNOWN** to be caused by MegaZeux bugs:

* The GLSL renderer relies on precise addressing of a 512x1024 texture in the
  tilemap fragment shaders, which is generally hard or impossible to do unless
  highp floats are available (or alternatively, mediump floats have more than
  10 bits of precision). This renderer is generally slower than softscale so
  it is not selected by default.

Issues **PROBABLY** caused by compatibility issues between Android and SDL:

* When text input is enabled, some keys may spontaneously stop working or stick.
  Because of this, text input has been disabled for Android, which means certain
  international keybord layouts probably won't work properly.
  (Moto G5 Plus, Android 8.1, arm64-v8a)
* Keys which would usually produce both a scancode and text will generate a key
  press and release on the same frame for some devices, meaning certain MZX
  features relying on the held status of a key (including shooting, the KEY#
  counters) will not work. (Nexus 7 (2013), Android 6.0, armeabi-v7a)
* The function keys (Fn) may not work as expected. (Xiaomi Mi 8 SE, Android 8.1, arm64-v8a)
* RGBA components may be reversed to ARBG, causing serious graphical issues.
  This can be worked around by turning on the "Disable HW Overlays" developer
  option. (Xiaomi Mi 8 SE, Android 8.1, arm64-v8a)
* Switching applications and/or connecting new Bluetooth devices may cause
  crashes. (Xiaomi Mi 8 SE, Android 8.1, arm64-v8a)
