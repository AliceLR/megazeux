# BUILDING MEGAZEUX VIA THIS MAKEFILE

This Makefile can be used to build a DMG targeting Mac OS X 10.4 (ppc, ppc64),
Mac OS X 10.6 (i686 and x86_64), Mac OS X 10.9 (x86_64h), and macOS 11 (arm64).
The process here is slightly involved and in most cases will require *multiple*
versions of Xcode to target everything. Notarization is NOT currently supported;
please use the Xcode project for that.

The current "official" build environment for this port is a dual-booted Mac Mini
with macOS 10.13 High Sierra (Xcode 9.4.1 with 3.2.6 compilers and SDKs via
XcodeLegacy) and macOS 12 Monterey (Xcode 14.2). However, this buildsystem should
technically work in some capacity with most Xcode versions.

Tested versions of Xcode:

- Xcode 3.2.6 has been used from MegaZeux 2.90 onward to produce PowerPC
  binaries. To get the correct compiler names from Makefile.arch, define
  XCODE_3=1. This is the **LAST** version of Xcode that targets PowerPC.
  The 10.4 SDK should be used for PPC and the 10.5 SDK should be used for PPC64.
  The current setup installs these SDKs alongside Xcode 9.4.1 via XcodeLegacy.
- Xcode 4.1 has been tested in the past and should work.
- Xcode 9.4.1 can target i686, x86_64, and x86_64h. It is the **LAST** version
  of Xcode that targets i686. This is used to produce the Intel binaries from
  from 2.93b onward. It is also capable of hosting the Xcode 3.2.6 SDKs and
  compilers.
- Xcode 14.2 is used to provide arm64/arm64e binaries from 2.93c onward. It
  can also provide x86_64 and x86_64h with a minimum OS version of 10.9.
  Versions as low as Xcode 12.2 should work for this, but are untested.

Untested and unsupported features:

- Other versions of Xcode might require changing some variables (Makefile.arch).
- arm64 and arm64e builds haven't been tested.
- Notarization will probably never be supported due to the nature of how
  this platform builds application bundles. Use the Xcode port for this instead.
- osxcross should work, but you'll need to override the compilers defined in
  Makefile.arch. Only the official SDKs have been tested so far.

Other required software:

- [dylibbundler](https://github.com/auriamg/macdylibbundler/), which can be
  installed via MacPorts: `sudo port install dylibbundler`.

## PREREQUISITES

1) Install the base version of Xcode that will be used (e.g. 9.4.1).
   Most of these are available from the Apple Developers site or from the
   OS install media (for older OS versions).
2) Add extra compilers/SDKs as necessary via XcodeLegacy.
3) Install dylibbundler via MacPorts or otherwise.
4) Grab a prebuilt MegaZeux dependencies bundle from
   [here](https://github.com/AliceLR/megazeux-dependencies) and extract the
   tarball into scripts/deps/. Alternatively (if you are feeling brave), you
   can use the Makefiles in scripts/deps/ to build these yourself. Manually
   building the dependencies currently requires wget.

## CONFIGURATION

Use the following config.sh setup to prepare `darwin-dist`.
```
./config.sh --platform darwin-dist --enable-release --enable-lto
```

For i686 builds and x86_64 builds targeting versions prior to 10.9, SDL2
should be explicitly supplied to future-proof configuring for them:
```
./config.sh --platform darwin-dist --enable-release --enable-lto --enable-sdl2
```

For the PowerPC and PowerPC64 builds, using SDL 1.2 is recommended:
```
./config.sh --platform darwin-dist --enable-release --enable-sdl1
```

Note that you can reconfigure between individual architecture builds,
but invoking `make clean` manually at any point will clean up the temporary
pre-lipo binaries from previous builds.

## BUILDING

Run make with variables defining the dependencies folders of each architecture
to be targeted (and optionally -j#, etc). Each architecture provided will be
compiled, e.g.

```
make \
  PREFIX_I686=scripts/deps/macos/i686 \
  PREFIX_PPC=scripts/deps/macos/ppc
```

will compile the x86 and PowerPC 32-bit builds.

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
The lipo and archive steps should work on any macOS version regardless of
which macOS/Xcode produced the component builds.

NOTE: OS X Mavericks (and presumably older versions) have linker issues when
a binary links against both x86_64 and x86_64h dylibs. This was fixed in FIXME!!!
and any binary containing both should target this OS version minimum *for both*.

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
