# Building MegaZeux for Android

NOTE: Only tested on Linux.

NOTE: Requires a few gigabytes of hard drive space and some patience.

This port is more complicated than the typical MegaZeux port and utilizes
a hybrid of MegaZeux's build system and a Gradle project. For best results,
follow this guide closely and ask for help on the MegaZeux Discord if you have
and problems.

While Android Studio can optionally be used to complete the final stages of the
build process, all other stages currently involve invoking Makefile targets
from the command line. This guide assumes you are comfortable with the command
line and GNU Make.

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

* libogg
* libvorbis
* libSDL2

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
