#!/bin/sh

. ../version.inc

# --arch=x86_64
# --arch=i386
# --arch=arm
# --arch=aarch64
flatpak-builder flatpak-builddir --force-clean com.digitalmzx.MegaZeux.yml

flatpak build-export flatpak-export flatpak-builddir
flatpak build-bundle flatpak-export "$TARGET".flatpak com.digitalmzx.MegaZeux

# To install instead:
#flatpak-builder --user --install --force-clean com.digitalmzx.MegaZeux.yml
