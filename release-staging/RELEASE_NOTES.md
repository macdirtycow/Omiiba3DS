# Omiiba3DS v1.4.5

> **Omiiba3DS** is an unofficial fork of [Luma3DS](https://github.com/LumaTeam/Luma3DS), rebranded for educational purposes. Firmware functionality is upstream LumaTeam work; see [`NOTICE-OMIIBA.md`](https://github.com/macdirtycow/Omiiba3DS/blob/master/NOTICE-OMIIBA.md).

## Changes since v1.4.4

This alpha adds a real VC Patch Helper for Omiiba's existing title patch system and expands GBA Labs with a safer GBA VC research/prototype area.

## Alpha warning

This is an **alpha / pre-release** build. The new VC Patch Helper only creates SD-card folders/help files and uses Omiiba's existing game patching support; it does not write to NAND. The GBA VC research menu deliberately does **not** apply unverified AGB_FIRM display byte patches yet.

- **VC Patch Helper:** new Boot Hub entry for title-based Virtual Console patches using `SD:/omiiba/titles/<TITLEID>/`.
- **Patch templates:** can create `SD:/omiiba/titles/0004000000000000/romfs/` and `SD:/omiiba/VC_PATCH_HELP.txt`.
- **Supported patch layout:** documents `code.ips`, `code.bps`, `code.bin`, `exheader.bin`, `locale.txt`, and LayeredFS `romfs/` replacements.
- **GBA VC research submenu:** GBA Labs now has a prototype area that tracks AGB_FIRM patch status and can create `SD:/omiiba/agb_vc_research/README.txt`.
- **Diagnostics:** Boot Hub diagnostics now checks game patching, the VC patch root, and GBA VC research notes.
- **Safety boundary:** NES/SNES/GB/GBC VC title patching uses the normal loader patch path; GBA VC display scaling/color remains research until a verified AgbBg/display-driver patch point exists.
- **Version string:** firmware reports **v1.4.5** (see `arm9/Makefile` / top-level `Makefile` `REVISION`).

## Install

Extract `Omiiba3DS-v1.4.5.zip` to the SD card root. For DS Labs, install `SD:/TWiLight Menu.cia` with FBI after extraction. If you only want the firmware update, copy only `boot.firm` to the root. [boot9strap](https://github.com/SciresM/boot9strap) required.

## License

GPLv3 — see [`LICENSE`](https://github.com/macdirtycow/Omiiba3DS/blob/master/LICENSE). Original work © LumaTeam.
