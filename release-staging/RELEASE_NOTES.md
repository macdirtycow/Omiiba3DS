# Omiiba3DS v1.4.4

> **Omiiba3DS** is an unofficial fork of [Luma3DS](https://github.com/LumaTeam/Luma3DS), rebranded for educational purposes. Firmware functionality is upstream LumaTeam work; see [`NOTICE-OMIIBA.md`](https://github.com/macdirtycow/Omiiba3DS/blob/master/NOTICE-OMIIBA.md).

## Changes since v1.4.3

This release turns GBA Labs into a practical display preset manager for the bundled `open_agb_firm` payload, bundles the official TWiLight Menu++ 3DS package for DS Labs users, and keeps AGB_FIRM VC-inject patching clearly marked as research.

## Alpha warning

This is an **alpha / pre-release** build. The Omiiba3DS GBA Labs integration itself does not write to NAND and is not expected to brick a console by itself, but it has not been broadly hardware-tested. It writes only the SD-card file `SD:/3ds/open_agb_firm/config.ini` when applying display presets, and it launches a third-party bare-metal GBA payload (`open_agb_firm`). Use only if you understand normal custom-firmware recovery steps and keep a known-good `boot.firm` backup available.

- **GBA display presets:** `Omiiba Boot Hub -> GBA Labs -> Display presets` can write open_agb_firm scaler/color/backlight presets to `SD:/3ds/open_agb_firm/config.ini`.
- **Preset choices:** includes Original 1:1, Balanced matrix, Soft bilinear, GBA color, SP101 bright, Vivid emulator, Battery saver and Restore safe default config.
- **Safe restore behavior:** the restore option writes a complete conservative `config.ini`; it never deletes or empties the file.
- **Safety prompts:** brighter/heavier color presets warn about battery impact before writing.
- **Bundled open_agb_firm:** the official unmodified `profi200/open_agb_firm` payload remains included at `SD:/omiiba/payloads/open_agb_firm.firm`, with its GBA database at `SD:/3ds/open_agb_firm/gba_db.bin`.
- **Diagnostics:** the Boot Hub diagnostics screen now checks `open_agb_firm`, `gba_db.bin`, `open_agb_firm/config.ini`, and the recommended `SD:/gba/` ROM folder.
- **Bundled TWiLight Menu++:** includes the official TWiLight Menu++ v27.23.0 3DS release files (`_nds`, `BOOT.NDS`, `TWiLight Menu.cia`, `roms`) plus GPL credits.
- **DS Labs:** new Boot Hub submenu for TWiLight Menu++ / nds-bootstrap setup checks, bundled CIA guidance, DS ROM folder creation and TWL filter help.
- **DS diagnostics:** checks recommended `SD:/roms/nds/`, `TWiLight Menu.cia`, `BOOT.NDS`, `_nds`, TWiLightMenu, nds-bootstrap, optional `usrcheat.dat`, and `twl_upscaling_filter.bin` paths.
- **Honest AGB_FIRM research entries:** scaling/filter and color-preset ideas are visible in GBA Labs, but clearly labelled research until a safe AGB_FIRM display-driver patch is verified on hardware.
- **Third-party credits:** `THIRD_PARTY/open_agb_firm/`, `THIRD_PARTY/TWiLightMenu/`, and `THIRD_PARTY/nds-bootstrap/` include upstream READMEs, GPLv3 license files, source links, bundled release tags and Omiiba bundle notices.
- **Version string:** firmware reports **v1.4.4** (see `arm9/Makefile` / top-level `Makefile` `REVISION`).

## Install

Extract `Omiiba3DS-v1.4.4.zip` to the SD card root. For DS Labs, install `SD:/TWiLight Menu.cia` with FBI after extraction. If you only want the firmware update, copy only `boot.firm` to the root. [boot9strap](https://github.com/SciresM/boot9strap) required.

## License

GPLv3 — see [`LICENSE`](https://github.com/macdirtycow/Omiiba3DS/blob/master/LICENSE). Original work © LumaTeam.
