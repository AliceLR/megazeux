# MegaZeux Build Scripts

This set of scripts is meant to compile a (mostly) complete set of MegaZeux
builds and updater files. They are only tested and known to work with MSYS2 but
the scripts here should be fairly POSIX-friendly.

The following platforms can currently be built by this script:

- Windows x64 and x86 (MinGW)
- Nintendo DS
- Nintendo 3DS
- Nintendo Wii
- Nintendo Wii U
- Nintendo Switch
- PlayStation Portable
- PlayStation Vita
- Sega Dreamcast
- HTML5/Emscripten
- Android
- DOS (DJGPP)
- Source package

Any platforms requiring chroots (Debian, Ubuntu, Raspbian), rpmbuild (Fedora),
obscure toolchains (Mac OS X x86/PPC, Amiga, Pandora, GP2X), or a separate build
system (Xcode, MSVC) are not currently supported by these scripts. Support for
some of these may be added at a later date.

## Usage
For MSYS2, the easiest way to do this is to copy all of these files to the MSYS
root folder. The following batch scripts can then be run, mostly automating the
build and updates process.

- `1_PrepareReleaseEnvironment.bat` (runs `mzx-init.sh`): create the `mzx-workingdir/`
  folder and clone an MegaZeux repository to build MZX with. For MSYS2 this can
  also install all required dependencies for the Windows builds, but other builds
  require additional work (see below).
- `2_CreateReleases.bat` (runs `mzx-build.sh`): run a complete set of MZX builds
  (see `mzx-build.sh` for a list of builds). This will produce a set of archives
  in `TARGET/zips/` and a release tree in `TARGET/releases/` for processing by
  the next script. Note that this script will typically take several minutes to run.
- `3_PackageReleases.bat` (runs `mzx-updates.sh`): convert `TARGET/releases/`
  into the format expected by the MZX updater. Files are gzipped and renamed to
  their SHA-256 sum with no extension; manifest.txt is gzipped with the same filename,
  and config.txt is removed. This will also create `updates.txt` (in gzipped form).
  This process occurs in the intermediate directory `TARGET/releases-update/`.
  Finally, the contents of this directory are archived with `tar` for distribution.
- `4_UploadReleases.bat` (runs `mzx-upload.sh`): uploads the updates tarball to
  user-specified update hosts and extracts it. The file `mzx-upload.sh` must be
  edited for this step to work.

NOTE: the batch scripts only work if the entire buildscript system is copied to
the root MSYS2 directory. To run the scripts from a different directory, use the
shell scripts directly.

These scripts can optionally use an existing MZX repository if run from the base
dir, but they should be copied to a separate place first (to prevent them from
being deleted when the current git branch/tag changes). The build script WILL
reset any changes in the MZX repository used.

## Environment setup

### MSYS2

Before running the init script above, you should run the following command:
`pacman -Syu`

This will make sure the base MSYS2 system is up to date. This command will likely
ask you to close the window; after doing so, simply open MSYS2 again and run
`pacman -Su` to finish the update.

### devkitPro

Follow the instructions [here](https://devkitpro.org/wiki/devkitPro_pacman) to
set up your existing pacman install (MSYS2, Arch) or a dkp-pacman install for
use with the devkitPro repositories. Add `export DEVKITPRO=/opt/devkitpro` to
your `.profile` or `.bashrc`. Finally, run the init script to install the
required packages.

The init script intentionally does not set up devkitPro pacman because devkitPro
doesn't want their package system to be used for automated scripts. It's also
easy enough to set up that there's no real reason to automate it.

NOTE:
These scripts no longer support devkitPSP as support for it has been officially
dropped. Old devkitPSP binaries can not be distributed as they contain devkitPro
trademarks. Use pspdev directly instead.

### pspdev

TODO: Versions of psptoolchain since April 2020 now successfully build in MSYS2.
Support for the newer toolchain has been patched into the PSP Makefile, but
builds produced with this fail to start in ppsspp with a black screen and no
console output. This needs to be investigated further.

If you have an old copy of devkitPSP, you can also simply define
`PSPDEV=/opt/devkitpro/devkitPSP` until then.

To set up pspdev, first add the following to your .profile or .bashrc:
```
export PSPDEV="/opt/pspdev"
export PATH="$PATH:$PSPDEV/bin"
```

Make sure the following packages are installed (MSYS2; use a MINGW64 terminal):
```
pacman --needed --noconfirm -S git svn wget patch tar unzip bzip2 xz diffutils python
pacman --needed --noconfirm -S autoconf automake cmake make m4 bison flex tcl texinfo doxygen
pacman --needed --noconfirm -S mingw-w64-x86_64-{gcc,SDL}
pacman --needed --noconfirm -S mingw-w64-x86_64-{libelf,libusb,ncurses,curl,gpgme,dlfcn}
pacman --needed --noconfirm -S mingw-w64-x86_64-{libarchive,openssl,readline,libtool}
```

Then clone the pspdev repository somewhere, `cd` to it, and run `./pspdev.sh`
to generate a pspdev build. Note that for MSYS2, this currently requires manual
intervention to finish linking GDB and certain utilities currently don't build.
```
git clone "https://github.com/pspdev/pspdev" /opt/pspdev-repo
cd /opt/pspdev-repo
./pspdev.sh
```
