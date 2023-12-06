# MegaZeux for the PlayStation Vita

MegaZeux is now fully supported on the PS Vita handheld. The editor is fully
supported - the world can be edited using a Bluetooth keyboard.

As of this writing, MegaZeux for Vita has been successfully built with Gentoo
Linux, Kubuntu 19.10, and macOS 10.15 Catalina and has been tested on a
softmodded PSVita PCH-1000 running the 3.71 firmware.

## Building

### Prerequisites

[Vita SDK](https://vitasdk.org/) is required to build MegaZeux. Follow the
instructions on their web site to install the SDK. No additional packages need
to be installed in order to build MegaZeux.

You will need a modified PS Vita or PSTV console in order to use MegaZeux.
Modifying your console to run homebrew is outside the scope of this guide.

### Build Instructions

Before building MegaZeux, please ensure that the `VITASDK` environment variable
is set and that the SDK has been added to your system path.

After cloning the MegaZeux repository, change into its directory and type the
following in your terminal emulator of choice:

`sh arch/psvita/CONFIG.PSVITA`

This will invoke the `config.sh` script with the default settings for this
platform. Now, type `make` and press ENTER. If you wish to run a parallel make
operation, use `make -jX`, replacing X with the number of threads that you wish
to use. The number of hardware threads available for your CPU is a good starting
point.

If the build completes successfully, type `make package` to build an installable
VPK package.

### Installation

Move the generated `mzxrun.vpk` or `megazeux.vpk` package to your console and
use VitaShell or a similar application to install it to your LiveArea. If you
only plan to play games, install `mzxrun.vpk`. If you plan to use the editor,
install `megazeux.vpk`. Please note that a Bluetooth keyboard will be required
in order to use the Robotic editor.

MegaZeux expects all games and assets to be stored in `ux0:/data/MegaZeux`.
Create this directory on your device, then transfer the included `config.txt`
file, `assets` directory, and any games that you wish to run into that
directory.

After this is done, MegaZeux should be fully playable. Have fun!
