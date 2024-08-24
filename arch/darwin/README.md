# BUILDING MEGAZEUX VIA THIS MAKEFILE

This Makefile can be used to build a DMG targeting Mac OS X 10.4 (ppc, ppc64),
Mac OS X 10.6 (i686 and x86_64), Mac OS X 10.9 (x86_64h), and macOS 11 (arm64).
The process here is slightly involved and in most cases will require *multiple*
versions of Xcode to target everything. Notarization is NOT currently supported;
please use the Xcode project for that.

Tested versions of Xcode:

- Xcode 3.2.6 has been used from MegaZeux 2.90 onward to produce PowerPC
  binaries. To get the correct compiler names from Makefile.arch, define
  XCODE_3=1. This is the **LAST** version of Xcode that targets PowerPC.
  The 10.4 SDK should be used for PPC and the 10.5 SDK should be used for PPC64.
  The current setup installs these SDKs alongside Xcode 9.4.1 via XcodeLegacy.
- Xcode 4.1 has been tested in the past and should work.
- Xcode 9.4.1 can target i686, x86_64, and x86_64h. It is the **LAST** version
  of Xcode that targets i686. This is used to produce the Intel binaries from
  from 2.93b onward.

Untested and unsupported features:

- Other versions of Xcode might require changing some variables (Makefile.arch).
- arm64 and arm64e builds haven't been tested.
- Notarization isn't supported yet.
- osxcross should work, but you'll need to override the compilers defined in
  Makefile.arch. Only the official SDKs have been tested so far.

Other required software:

- [dylibbundler](https://github.com/auriamg/macdylibbundler/), which can be
  installed via MacPorts: `sudo port install dylibbundler`.

## PREREQUISITES (Xcode 3.2.6)

Note: this section was written against Xcode 3.2.6. The process of building
dependencies is different for later versions.

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

## BUILDING

You need Xcode's development tools package to have been installed. This does
NOT mean you need all of the Xcode IDE, but you do need the DMG file it is
installed from. This is normally included on your OS X install DVD #1. It is
enough to install the UNIX tools.

Run make with variables defining the dependencies folders of each architecture
to be targeted (and optionally -j#, etc). Each architecture provided will be
compiled:

```
make \
  PREFIX_I686=[YOUR PREFIX HERE]/i686 \
  PREFIX_PPC=[YOUR PREFIX HERE]/ppc
```

The prefixes you supply will determine which binaries are built. Supported
prefixes:

| Arch    | Prefix          |
|---------|-----------------|
| arm64   | `PREFIX_ARM64`  |
| arm64e  | `PREFIX_ARM64E` |
| i686    | `PREFIX_I686`   |
| x86_64  | `PREFIX_AMD64`  |
| x86_64h | `PREFIX_AMD64H` |
| ppc     | `PREFIX_PPC`    |
| ppc64   | `PREFIX_PPC64`  |

Each architecture's binaries will be output as "mzxrun.${ARCH}" et al.
After all desired platforms are built, `make lipo` will be run automatically
to merge them. Any pre-existing binaries from other architectures will also
be combined into the fat binary. This can be used to e.g. import PowerPC
builds made with Xcode 3.2.6 into a binary otherwise built by Xcode 9.4.1.

## PACKAGING

**FIRST**: Make sure there is no currently mounted volume named "MegaZeux",
and that /Volumes contains no folder named "MegaZeux". This will break the
DMG creation step. You may have to manually clean up from prior failed builds.

After `make` or `make lipo`, simply do the following:
```
make archive
```

This will output a file to the `build/dist/darwin` directory at the top level.
One of these is a redistributable DMG that contains both MegaZeux and MZXRun
applications supporting the architectures you built.
