# MegaZeux Dependency Makefiles

This directory contains generalized Makefiles to build dependencies for
some platforms supported by MegaZeux which don't provide these dependencies
through other means (e.g. built-in package manager). These Makefiles are
not guaranteed to work with every setup; rather, they're whatever Makefiles
worked the last time I had to rebuild dependencies for MegaZeux. Note that
the Android port uses its own integrated dependency builder.

Official builds can be found here: https://github.com/AliceLR/megazeux-dependencies

## Usage of tarballs

* MinGW: extract to some directory `DIR`. Set the environment variables `MINGW32_PREFIX`
  and `MINGW64_PREFIX` to `DIR/x86` and `DIR/x64`, respectively, and configure with
  `arch/mingw/CONFIG.MINGW32` or `arch/mingw/CONFIG.MINGW64`.
* DJGPP: extract to some directory `DIR`. Set the environment variable `DJGPP`
  to `DIR/i386` and configure with `arch/djgpp/CONFIG.DJGPP`.

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

### DJGPP
The DJGPP toolchain `bin` directory should be on `$PATH`.

Builds the following libraries: zlib, libpng, libogg, libvorbis,
libvorbisidec (Tremor lowmem).
The binaries *should* target DOS/Windows 9x with CWSDPMI.EXE on `-march=i386`.

```sh
make PLATFORM=djgpp
make PLATFORM=djgpp package
```
