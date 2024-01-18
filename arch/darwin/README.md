# BUILDING MEGAZEUX VIA THIS MAKEFILE

If you are building on the Darwin 10 platform (OS 10.6 or higher) you can build
MegaZeux without using Xcode by following the instructions in this file. This
process has been superceded by the Xcode project folder (arch/xcode) for all
Intel Mac builds, but is still required to create PowerPC builds.

Xcode 3.2.6 is assumed to be installed, but older versions may work. Xcode 4.x
and later remove the PowerPC compilers, however, so they are not suited to
creating PowerPC builds.

## PREREQUISITES

Make sure `i686-apple-darwin10-gcc` and/or `powerpc-apple-darwin10-gcc` exist
in your `/usr/bin` folder or create them with the following commands:

```
cd /usr/bin
sudo ln -s i686-apple-darwin10-gcc-4.2.1 i686-apple-darwin10-gcc
sudo ln -s powerpc-apple-darwin10-gcc-4.2.1 powerpc-apple-darwin10-gcc
```

You must build static library versions of zlib, libpng, libogg, libvorbis,
and SDL 1.2 (SDL 2 is not supported for these platforms). These libraries
must be installed to a common prefix. The following commands to `configure'
are recommended:

### zlib

#### i686
```
CHOST="i686-apple-darwin10" \
CFLAGS="-O2 -mmacosx-version-min=10.6" \
./configure \
  --prefix=[YOUR PREFIX HERE]/i686 \
  -static
```

#### powerpc
```
CHOST="powerpc-apple-darwin10" \
CFLAGS="-O2 -mmacosx-version-min=10.4" \
./configure \
  --prefix=[YOUR PREFIX HERE]/ppc \
  -static
```

### libpng, libogg, libvorbis, and SDL

#### i686
```
CFLAGS="-O2 -mmacosx-version-min=10.6" \
./configure \
  --prefix=[YOUR PREFIX HERE]/i686 \
  --disable-shared \
  --enable-static \
  --build=i686-apple-darwin10 \
  --host=i686-apple-darwin10
```

#### powerpc
```
CFLAGS="-O2 -mmacosx-version-min=10.4" \
./configure \
  --prefix=[YOUR PREFIX HERE]/ppc \
  --disable-shared \
  --enable-static \
  --build=powerpc-apple-darwin10 \
  --host=powerpc-apple-darwin10
```

Similarly, AMD64 and PPC64 sets of dependencies can be produced, but are
probably not necessary.

## CONFIGURATION

Use the following config.sh setup to prepare `darwin-dist`. While the utils
can be readily built, they currently are not included:

```
./config.sh \
  --platform darwin-dist \
  --disable-utils \
  --enable-release
```

## BUILDING INDIVIDUALLY (optional)

You need Xcode's development tools package to have been installed. This does
NOT mean you need all of the Xcode IDE, but you do need the DMG file it is
installed from. This is normally included on your OS X install DVD #1. It is
enough to install the UNIX tools.

**NOTE**: Read this section and then _the following section_ before building.

A build for a single arch can be produced using one of the following sequences
of commands:
```
make package ARCH=i686 PREFIX_I686=[YOUR PREFIX HERE]/i686
make distclean
```
```
make package ARCH=ppc PREFIX_PPC=[YOUR PREFIX HERE]/ppc
make distclean
```

These will build the i686/ppc binary and put it in the root as "mzxrun.i686" or
"megazeux.ppc" or etc. It will also clean the source tree, which is **VERY IMPORTANT**.

The instructions above can also be repeated with `ARCH=amd64` and `ARCH=ppc64` for
64-bit binaries (this may improve performance, but it is not necessary).

The individual builds can then be merged using the following comand:
```
make lipo
```

## BUILDING SEVERAL ARCHITECTURES AT ONCE

All of the instructions from the previous section can be performed with a single
command:
```
make dist \
  PREFIX_I686=[YOUR PREFIX HERE]/i686 \
  PREFIX_PPC=[YOUR PREFIX HERE]/ppc
```

The prefixes you supply (`PREFIX_I686`, `PREFIX_AMD64`, `PREFIX_PPC`, and/or
`PREFIX_PPC64`) will determine which binaries are built. After all desired
platforms are built, `make lipo` will be run automatically to merge them.


## PACKAGING

After `make dist` (or `make lipo` if using the first method), simply do the
following:
```
make archive
```

This will output a file to the `build/dist/darwin` directory at the top level.
One of these is a redistributable DMG that contains both MegaZeux and MZXRun
applications supporting the architectures you built.

