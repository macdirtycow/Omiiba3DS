# Omiiba3DS v1.4.7

> **Omiiba3DS** is an unofficial fork of [Luma3DS](https://github.com/LumaTeam/Luma3DS), rebranded for educational purposes. Firmware functionality is upstream LumaTeam work; see [`NOTICE-OMIIBA.md`](https://github.com/macdirtycow/Omiiba3DS/blob/master/NOTICE-OMIIBA.md).

## Changes since v1.4.6

This alpha adds phase-1 OTA updates through a bundled Universal-Updater store.

## Alpha warning

This is an **alpha / pre-release** build. OTA updating is handled by Universal-Updater from HOME Menu, not by the arm9 bootloader. Keep a known-good `boot.firm` backup before using OTA firmware updates.

- **Bundled Omiiba UniStore:** adds `SD:/3ds/Universal-Updater/stores/omiiba.unistore` for Universal-Updater.
- **OTA boot.firm install:** the UniStore downloads the latest GitHub release zip and extracts `boot.firm` to `SD:/boot.firm`.
- **Wireless Tools checks:** now checks for the Omiiba UniStore in addition to Universal-Updater CIA and ftpd paths.
- **OTA notes:** adds `SD:/omiiba/OTA_UPDATE_NOTES.txt` with a short safe-update workflow.
- **Release assets:** this release also publishes `omiiba.unistore` as a GitHub asset so Universal-Updater can refresh the store.
- **Version string:** firmware reports **v1.4.7** (see `arm9/Makefile` / top-level `Makefile` `REVISION`).

## Install

Extract `Omiiba3DS-v1.4.7.zip` to the SD card root. For OTA updates, install Universal-Updater and use the bundled Omiiba store at `SD:/3ds/Universal-Updater/stores/omiiba.unistore`. If you only want the firmware update, copy only `boot.firm` to the root. [boot9strap](https://github.com/SciresM/boot9strap) required.

## License

GPLv3 — see [`LICENSE`](https://github.com/macdirtycow/Omiiba3DS/blob/master/LICENSE). Original work © LumaTeam.
