# Omiiba3DS v1.4.8

> **Omiiba3DS** is an unofficial fork of [Luma3DS](https://github.com/LumaTeam/Luma3DS), rebranded for educational purposes. Firmware functionality is upstream LumaTeam work; see [`NOTICE-OMIIBA.md`](https://github.com/macdirtycow/Omiiba3DS/blob/master/NOTICE-OMIIBA.md).

## Changes since v1.4.7

This alpha adds phase-2 OTA updates with a dedicated Omiiba Updater 3DSX app.

## Alpha warning

This is an **alpha / pre-release** build. OTA updating is handled by HOME Menu/Homebrew Launcher apps, not by the arm9 bootloader. Keep a known-good `boot.firm` backup before using OTA firmware updates.

- **Omiiba Updater app:** adds `SD:/3ds/OmiibaUpdater/OmiibaUpdater.3dsx`.
- **Direct boot.firm OTA:** the app downloads `https://github.com/macdirtycow/Omiiba3DS/releases/latest/download/boot.firm`.
- **Backup behavior:** the app moves the old `SD:/boot.firm` to `SD:/omiiba/backups/boot.firm.bak` before installing the new file.
- **Wireless Tools checks:** now checks for Omiiba Updater, Universal-Updater store, Universal-Updater CIA and ftpd paths.
- **Release assets:** this release publishes `boot.firm`, `OmiibaUpdater.3dsx`, and `omiiba.unistore` as standalone assets.
- **Version string:** firmware reports **v1.4.8** (see `arm9/Makefile` / top-level `Makefile` `REVISION`).

## Install

Extract `Omiiba3DS-v1.4.8.zip` to the SD card root. For OTA updates, launch `SD:/3ds/OmiibaUpdater/OmiibaUpdater.3dsx` from Homebrew Launcher, or use Universal-Updater with `SD:/3ds/Universal-Updater/stores/omiiba.unistore`. If you only want the firmware update, copy only `boot.firm` to the root. [boot9strap](https://github.com/SciresM/boot9strap) required.

## License

GPLv3 — see [`LICENSE`](https://github.com/macdirtycow/Omiiba3DS/blob/master/LICENSE). Original work © LumaTeam.
