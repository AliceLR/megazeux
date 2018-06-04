#!/bin/bash

# Set up devkitPro pacman repositories.

if ! grep -q "dkp" /etc/pacman.conf ; then
  pacman-key --recv F7FD5492264BB9D0
  pacman-key --lsign F7FD5492264BB9D0

  cat /dk-patches/pacman.conf.append >> /etc/pacman.conf

  pacman --needed --no-confirm -S devkitpro-keyring
fi


# Set up symbolic links to map expected folders to devkitPro folders

mkdir -p /opt/devkitpro/portlibs
EXEC=`cygpath -w /dk-makelinks.bat`
REALPATH=`cygpath -w /opt/devkitpro/portlibs`

powershell -Command "Start-Process -filepath '$EXEC' -ArgumentList '$REALPATH' -Verb RunAs "
