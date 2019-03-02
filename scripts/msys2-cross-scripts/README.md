# MSYS2 Cross Build Scripts

This set of scripts is meant to complement the Windows MSYS2 build scripts.
These scripts should be run only after "1_PrepareReleaseEnvironment.bat"
from the Windows scripts has been run. Copy the contents of this folder to the
root of your MSYS2 directory tree.

## devkitPro

### Environment setup:

The first two scripts are required only for creating a new MSYS2 build
environment, and can be skipped if your environment is already set up:

* DK1_PrepareReleaseEnvironment.bat -- installs extra required tools.

* DK2_BuildDependencies.bat -- collects dependencies required for MegaZeux from
various repositories and builds them. All work is done in the same working
directory used by the Windows scripts.

### Creating builds:

These scripts can be used to create a fresh build for each devkitPro platform.

* DK3_NDS.bat -- generates an NDS build and moves it to the "zips" folder.
* DK4_3DS.bat -- generates a 3DS build and moves it to the "zips" folder.
* DK5_Wii.bat -- generates a Wii build and moves it to the "zips" folder.
* DK6_Switch.bat -- generates a Switch build and moves it to the "zips" folder.

These scripts no longer support devkitPSP as support for it has been officially
dropped. Old devkitPSP binaries can not be distributed as they contain devkitPro
trademarks. Use pspdev directly instead.

## pspdev

### Environment setup:

* FIXME_PrepareReleaseEnvironment.bat -- generate a build environment with
  pspdev/psptoolchain. This should be performed only once per environment. This
  will be installed to `/opt/pspdev` and should add `$PSPDEV` to your .profile.

* PSP1_BuildDependencies.bat -- install pspdev/psp-port dependencies required to
  build MegaZeux to an existing pspdev/psptoolchain installation. This should
  be performed only once per environment.

### Creating builds:

* PSP2_Build.bat -- generates a PSP build and moves it to the "zips" folder.

