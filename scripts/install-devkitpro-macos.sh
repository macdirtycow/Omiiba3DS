#!/bin/sh
# One-shot installer for devkitPro pacman + 3DS toolchain on macOS.
# Requires administrator password (sudo). Run from repo root:
#   chmod +x scripts/install-devkitpro-macos.sh && ./scripts/install-devkitpro-macos.sh

set -eu

PKG_URL="https://github.com/devkitPro/pacman/releases/download/v6.0.2/devkitpro-pacman-installer.pkg"
PKG_TMP="${TMPDIR:-/tmp}/devkitpro-pacman-installer.pkg"

echo "Downloading devkitPro pacman installer..."
curl -sSfL -o "$PKG_TMP" "$PKG_URL"

echo "Installing pacman package (requires sudo)..."
sudo installer -pkg "$PKG_TMP" -target /

echo "Syncing package databases and installing 3ds-dev (requires sudo, may take a while)..."
sudo dkp-pacman -Syu --noconfirm
sudo dkp-pacman -S 3ds-dev --needed --noconfirm

echo ""
echo "Done. Open a new terminal (or reboot if advised by the installer), then run:"
echo "  source /etc/profile.d/devkit-env.sh   # if this file exists"
echo "  source scripts/env-build.sh           # from this repo"
echo "  make"
