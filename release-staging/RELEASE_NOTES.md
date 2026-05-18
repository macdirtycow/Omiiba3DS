# Omiiba3DS v1.4.9

> **Omiiba3DS** is an unofficial fork of [Luma3DS](https://github.com/LumaTeam/Luma3DS), rebranded for educational purposes. Firmware functionality is upstream LumaTeam work; see [`NOTICE-OMIIBA.md`](https://github.com/macdirtycow/Omiiba3DS/blob/master/NOTICE-OMIIBA.md).

## Alpha warning

This is an **alpha / pre-release** build. Keep a known-good `boot.firm` backup on your PC or SD card before testing OTA firmware updates or experimental retro features. OTA updating is handled by HOME Menu/Homebrew Launcher apps, not by the arm9 bootloader.

## What is included

- **Omiiba Boot Hub:** SELECT opens the Hub first, with fast `Continue normal boot`, explicit `Save settings and boot`, setup wizard, advanced settings, diagnostics, profiles, payload manager, theme/settings, GodMode9 tools, GBA Labs, DS Labs, DS Widescreen Labs, VC Patch Helper, and Wireless Tools.
- **Setup wizard and profiles:** guided safe defaults for splash, brightness, game patching, external FIRMs/modules, GodMode9 placement, plus named presets for Default, Safe, Performance, Plugin/Game Modding, and Developer/GDB.
- **Diagnostics:** read-only checks for config, GodMode9, payloads, GM9 scripts, SD/CTRNAND `boot.firm`, EmuNAND, setup wizard marker, `open_agb_firm`, DS/TWiLight paths, VC patch folders, widescreen files, and OTA helper paths.
- **Payload manager:** lists and launches `.firm` payloads, explains hotkey naming, and can create non-destructive hotkey copies.
- **GBA Labs:** bundles and launches `open_agb_firm`, writes SD-only display presets for `/3ds/open_agb_firm/config.ini`, checks GBA setup files, and keeps deeper AGB_FIRM scaling/filter and GBA VC display patching clearly marked as research.
- **DS Labs:** bundles TWiLight Menu++ / nds-bootstrap files, checks common DS setup paths, guides CIA installation, and can create the recommended `SD:/roms/nds/` folder.
- **DS Widescreen Labs:** manages the existing TWL external filter setting, checks `twl_upscaling_filter.bin`, checks the bundled widescreen compatibility list, creates setup notes/folders, and documents the limits around per-game widescreen codes.
- **VC Patch Helper:** checks game patching status, creates the `SD:/omiiba/titles/0004000000000000/romfs/` template, and documents LayeredFS, IPS, BPS, code, exheader, locale, and RomFS patch layouts.
- **Wireless Tools:** prepares and checks paths for Omiiba Updater, Universal-Updater, the bundled Omiiba UniStore, CIA placement, and ftpd without trying to run Wi-Fi networking in the bootloader.
- **OTA updates:** includes `SD:/3ds/OmiibaUpdater/OmiibaUpdater.3dsx` for direct latest `boot.firm` downloads from GitHub releases, plus `SD:/3ds/Universal-Updater/stores/omiiba.unistore` for Universal-Updater.
- **Updater safety fixes in v1.4.9:** the Omiiba Updater now rejects obviously wrong `boot.firm` download sizes and attempts to restore the previous `SD:/boot.firm` if the final install rename fails. The UniStore now stores temporary zip downloads at the SD root so it does not depend on nested folders existing first.
- **Release assets:** this release publishes `Omiiba3DS-v1.4.9.zip`, standalone `boot.firm`, `OmiibaUpdater.3dsx`, and `omiiba.unistore`.
- **Version string:** firmware reports **v1.4.9**.

## Install

Extract `Omiiba3DS-v1.4.9.zip` to the SD card root. For OTA updates, launch `SD:/3ds/OmiibaUpdater/OmiibaUpdater.3dsx` from Homebrew Launcher, or use Universal-Updater with `SD:/3ds/Universal-Updater/stores/omiiba.unistore`. If you only want the firmware update, copy only `boot.firm` to the root. [boot9strap](https://github.com/SciresM/boot9strap) required.

## License

GPLv3 — see [`LICENSE`](https://github.com/macdirtycow/Omiiba3DS/blob/master/LICENSE). Original work © LumaTeam.
