#!/bin/bash

# Set up devkitPro pacman repositories.

if ! grep -q "dkp" /etc/pacman.conf ; then
  pacman-key --recv F7FD5492264BB9D0
  pacman-key --lsign F7FD5492264BB9D0

  cat /cross-patches/pacman.conf.append >> /etc/pacman.conf

  pacman --needed --no-confirm -S devkitpro-keyring
fi
