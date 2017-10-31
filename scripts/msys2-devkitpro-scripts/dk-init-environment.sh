#!/bin/bash

# Required for nds
pacman --needed --noconfirm -S p7zip

# Required for psp
pacman --needed --noconfirm -S autoconf automake imagemagick libtool patch
