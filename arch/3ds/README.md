## Preparation

You need to install the latest versions of the devkitARM
toolchain, as well as the necessary 3DS tooling.

You also need to install zlib, libpng and libtremor. This
can be done with the following command:

    pacman -S 3ds-libogg 3ds-libpng 3ds-libvorbisidec 3ds-zlib

## Configuring

See CONFIG.3DS for an optimal `config.sh` configure line. You need
to ensure DEVKITPRO and DEVKITARM are both defined and valid.

## Building

For the moment, you need to build with:

    make package

This will emit the "mzxrun.3dsx" file.

## Packaging

You can then use the usual `make archive` to build a
build/dist/3ds/mzxgit-3ds.zip file for distribution.
