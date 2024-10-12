# MegaZeux Dependency Makefiles

This directory contains generalized Makefiles to build dependencies for
some platforms supported by MegaZeux which don't provide these dependencies
through other means (e.g. built-in package manager). These Makefiles are
not guaranteed to work with every setup; rather, they're whatever Makefiles
worked the last time I had to rebuild dependencies for MegaZeux. Note that
the Android port uses its own integrated dependency builder and the
MSVC port still requires manual generation of dependencies.

Official builds can be found here: https://github.com/AliceLR/megazeux-dependencies

## Usage of tarballs

* MinGW: extract to some directory `DIR`. Set the environment variables `MINGW32_PREFIX`
  and `MINGW64_PREFIX` to `DIR/x86` and `DIR/x64`, respectively, and configure with
  `arch/mingw/CONFIG.MINGW32` or `arch/mingw/CONFIG.MINGW64`.
* macOS: extract to some directory `DIR`. Configure with
  `./config.sh --platform darwin-dist`, then provide `PREFIX_[target]=DIR/[target_dir]`
  to Make for each platform to build.
* DJGPP: extract to some directory `DIR`. Set the environment variable `DJGPP`
  to `DIR/i386` and configure with `arch/djgpp/CONFIG.DJGPP`.
* linux (MSan): extract to some directory `DIR`. Configure with
  `./config.sh --platform unix-devel --prefix DIR --disable-sdl --enable-msan` and make.
  You may have to build your own depending on the available glibc version.

## Building

GNU Make is required for all targets.

To clean all source build directories (located in `./build`), use `make clean`.
To delete all build directories, use `make distclean`.

### MinGW
The MinGW-w64 toolchains should be on `$PATH`.

Builds the following libraries: zlib, libpng, libogg, libvorbis, libSDL, libSDL2.
The x86 binaries *should* target Windows XP on  `-march=i686` and
the x64 binaries *should* target Windows XP on `-march=x86-64`.

```sh
make PLATFORM=mingw
make PLATFORM=mingw package
```

### macOS (darwin-dist)
Install any of the following Xcode versions/SDKs.
Recommended setup: High Sierra running 9.4.1 with the 3.2.6 SDKs installed via XcodeLegacy,
plus any macOS version capable of running Xcode 12.2 or higher for ARM support.

| Target    | Xcode	| SDK		| macOS Target	| Supported SDL	| Notes |
|-----------|-----------|---------------|---------------|---------------|-------|
| `arm64`   | 12.2+	| 11.0+		| 11.0		| Latest	|
| `arm64e`  | 12.2+	| 11.0+		| 11.0		| Latest	|
| `i686`    | 9.4.1	| 10.13.4	| 10.6		| 2.0.22	| Last SDK to target 10.6, x86
| `x86_64`  | 9.4.1	| 10.13.4	| 10.6		| 2.0.22	| Last SDK to target 10.6
| `x86_64h` | 9.4.1?	| 10.13.4	| 10.9		| Latest	|
| `ppc`     | 3.2.6	| 10.4u		| 10.4		| 2.0.3		| Last SDK to target PPC
| `ppc64`   | 3.2.6	| 10.5		| 10.5		| 2.0.3		| Last SDK to target PPC

NOTE: XcodeLegacy will replace the current Xcode ld with a wrapper that reroutes all
PowerPC and <=10.6 usage to the Xcode 3.2.6 ld. This script will be located at
/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ld
and the check that redirects <=10.6 needs to be changed to <=10.5 instead. Otherwise,
linking i686 and x86_64 binaries will fail because ld doesn't understand the .tbd files
in newer SDKs.

Builds the following libraries: zlib, libpng, libogg, libvorbis, libSDL, libSDL2.

```sh
# Select only the required architectures, e.g.
make PLATFORM=macos ARCH=i686
make PLATFORM=macos ARCH=x86_64
make PLATFORM=macos ARCH=ppc
make PLATFORM=macos ARCH=ppc64
make PLATFORM=macos package
```

### Xcode
Install Xcode 12.2+ on any macOS version capable of running it. The dependency
builder targets x86_64 and arm64 only, but it should be possible to link x86_64h and arm64e
against these binaries.

Unlike the other platforms here, the tarball produced for this platform is designed
to be extracted directly into arch/xcode/, where the Xcode project expects the frameworks to be.

Builds the following libraries: libpng, libogg, libvorbis, libSDL2.
```sh
# Compile all frameworks.
make PLATFORM=xcode
# Package the frameworks. Do not use the 'package' target for this platform.
make PLATFORM=xcode frameworks
```

### DJGPP
The DJGPP toolchain `bin` directory should be on `$PATH`.

Builds the following libraries: zlib, libpng, libogg, libvorbis,
libvorbisidec (Tremor lowmem).
Note that stb_vorbis should be used due to limitations of running
audio code in an interrupt.
The binaries *should* target DOS/Windows 9x with CWSDPMI.EXE on `-march=i386`.

```sh
make PLATFORM=djgpp
make PLATFORM=djgpp package
```
