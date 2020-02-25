#!/bin/bash

# Required for nds
pacman --needed --noconfirm -S p7zip

# Required for 3ds and wii
pacman --needed --noconfirm -S autoconf automake libtool pkg-config zip unzip
