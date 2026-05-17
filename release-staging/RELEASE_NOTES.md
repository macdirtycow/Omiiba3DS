# Omiiba3DS v1.4.3

> **Omiiba3DS** is an unofficial fork of [Luma3DS](https://github.com/LumaTeam/Luma3DS), rebranded for educational purposes. Firmware functionality is upstream LumaTeam work; see [`NOTICE-OMIIBA.md`](https://github.com/macdirtycow/Omiiba3DS/blob/master/NOTICE-OMIIBA.md).

## Changes since v1.4.2

This release starts the GBA-focused Omiiba roadmap with a stable `open_agb_firm` shortcut and an experimental GBA Labs area.

## Alpha warning

This is an **alpha / pre-release** build. The Omiiba3DS GBA Labs integration itself does not write to NAND and is not expected to brick a console by itself, but it has not been broadly hardware-tested. It also bundles and launches a third-party bare-metal GBA payload (`open_agb_firm`). Use only if you understand normal custom-firmware recovery steps and keep a known-good `boot.firm` backup available.

- **GBA Labs:** new Boot Hub submenu for GBA-focused tools and research notes.
- **Bundled open_agb_firm:** the official unmodified `profi200/open_agb_firm` payload is included at `SD:/omiiba/payloads/open_agb_firm.firm`, with its GBA database at `SD:/3ds/open_agb_firm/gba_db.bin`.
- **open_agb_firm launcher:** open `Omiiba Boot Hub -> GBA Labs -> Start open_agb_firm` to launch the bundled payload.
- **Diagnostics:** the Boot Hub diagnostics screen now checks whether `open_agb_firm` is present.
- **Honest AGB_FIRM research entries:** scaling/filter and color-preset ideas are visible in GBA Labs, but clearly labelled research until a safe AGB_FIRM display-driver patch is verified on hardware.
- **Third-party credits:** `THIRD_PARTY/open_agb_firm/` includes the upstream README, GPLv3 license files, source link, bundled release tag and Omiiba bundle notice.
- **Release docs:** payload instructions and `omiiba/GBA_LABS_NOTES.txt` document the stable open_agb_firm workflow and the current AGB_FIRM limitations.
- **Version string:** firmware reports **v1.4.3** (see `arm9/Makefile` / top-level `Makefile` `REVISION`).

## Install

Extract `Omiiba3DS-v1.4.3.zip` to the SD card root, or copy only `boot.firm` to the root. [boot9strap](https://github.com/SciresM/boot9strap) required.

## License

GPLv3 — see [`LICENSE`](https://github.com/macdirtycow/Omiiba3DS/blob/master/LICENSE). Original work © LumaTeam.
