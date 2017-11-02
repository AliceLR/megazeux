#!/bin/bash

# Required for nds
pacman --needed --noconfirm -S p7zip

# Required for psp
pacman --needed --noconfirm -S autoconf automake libtool patch mingw-w64-x86_64-imagemagick

# Required for 3ds
pacman --needed --noconfirm -S autoconf automake libtool pkg-config zip unzip
