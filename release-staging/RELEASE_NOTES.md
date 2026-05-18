# Omiiba3DS v1.4.6

> **Omiiba3DS** is an unofficial fork of [Luma3DS](https://github.com/LumaTeam/Luma3DS), rebranded for educational purposes. Firmware functionality is upstream LumaTeam work; see [`NOTICE-OMIIBA.md`](https://github.com/macdirtycow/Omiiba3DS/blob/master/NOTICE-OMIIBA.md).

## Changes since v1.4.5

This alpha adds DS Widescreen Labs as a real TWL filter manager and adds Wireless Tools as a safe updater/FTP setup helper.

## Alpha warning

This is an **alpha / pre-release** build. DS Widescreen Labs toggles the existing Omiiba/Luma TWL external filter setting and creates SD-card helper files only; it does not bundle a new proprietary widescreen filter binary. Wireless Tools does not run networking in the bootloader and does not install CIA files directly.

- **DS Widescreen Labs:** new Boot Hub entry for TWL external filter status, pending enable/disable, setup notes and compatibility checks.
- **Widescreen setup files:** can create `SD:/omiiba/DS_WIDESCREEN_LABS.txt`, `SD:/omiiba/twl_filters/`, and the recommended `SD:/roms/nds/` folder.
- **TWiLight compatibility checks:** checks bundled `Games supported with widescreen.txt`, TWiLight folders, DS ROM folder, and `SD:/omiiba/twl_upscaling_filter.bin`.
- **Wireless Tools:** new Boot Hub entry to prepare `SD:/cias/`, `SD:/3ds/ftpd/`, and `SD:/omiiba/WIRELESS_TOOLS.txt`.
- **Safe wireless boundary:** Universal-Updater and ftpd are still launched from HOME Menu/Homebrew Launcher; Omiiba only prepares and checks the expected paths.
- **Version string:** firmware reports **v1.4.6** (see `arm9/Makefile` / top-level `Makefile` `REVISION`).

## Install

Extract `Omiiba3DS-v1.4.6.zip` to the SD card root. For DS Labs, install `SD:/TWiLight Menu.cia` with FBI after extraction. If you only want the firmware update, copy only `boot.firm` to the root. [boot9strap](https://github.com/SciresM/boot9strap) required.

## License

GPLv3 — see [`LICENSE`](https://github.com/macdirtycow/Omiiba3DS/blob/master/LICENSE). Original work © LumaTeam.
