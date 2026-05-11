# Omiiba3DS — Notice of Modifications

This file satisfies GPLv3 §5(a) ("you must cause the modified files to carry
prominent notices stating that you changed the files and the date of any
change") and the Additional Terms 7.b / 7.c that Luma3DS attaches to its
sources (preserve attributions; do not misrepresent origin).

## Origin

Omiiba3DS is an unofficial fork of [Luma3DS](https://github.com/LumaTeam/Luma3DS).
All original copyrights remain with the **LumaTeam** developers
(notably AuroraWright, TuxSH and PabloMK7). The original `LICENSE` (GPLv3) is
unchanged in this repository.

This fork is **not** affiliated with, endorsed by, or supported by LumaTeam.

## Maintainer of this fork

- @Macdirtycow (Leopold) — fork created May 2026 for educational purposes.

## Summary of changes vs. upstream Luma3DS

Date of changes: **2026-05-10 / 2026-05-11**.

The following modifications were made to the upstream Luma3DS source tree.
Almost every source file received the rebrand replacements; only the most
significant functional changes are listed individually.

### Rebrand
- All in-source mentions of `Luma3DS` / `LumaTeam` were renamed to `Omiiba3DS`
  / `OmiibaTeam` in user-visible strings, file headers (text only), README,
  `config_template.ini`, and the `.github/ISSUE_TEMPLATE`.
- Internal symbols renamed:
  `LumaSharedConfig` → `OmiibaSharedConfig`,
  `Luma_SharedConfig` → `Omiiba_SharedConfig`,
  `LumaConfig_*` → `OmiibaConfig_*`,
  `readLumaConfig` → `readOmiibaConfig`,
  `MAKE_LUMA_VERSION_MCU` → `MAKE_OMIIBA_VERSION_MCU`, etc.
- Files renamed:
  `luma_config.{c,h}` → `omiiba_config.{c,h}`,
  `luma_shared_config.h` → `omiiba_shared_config.h`,
  `pm/source/luma.{c,h}` → `pm/source/omiiba.{c,h}`.

### File-system layout
- The on-card data folder moved from `/luma` to `/omiiba` (and from
  `/rw/luma` to `/rw/omiiba` on CTRNAND).
- A one-shot legacy migration in `arm9/source/fs.c::switchToMainDir` will
  rename `/luma` to `/omiiba` on first boot when the new folder is missing,
  so existing setups keep working without manual intervention.

### Branding tweaks
- Bootloader and Rosalina title color (`COLOR_TITLE`) changed from cyan to
  green.
- Rosalina overlay menu retitled `"Cow menu"` (titles like
  `"Rosalina -- System info"` became `"Cow menu -- System info"`). The
  internal sysmodule and C symbols (`rosalina.cxi`, `RosalinaMenu_*`) are
  unchanged for cross-process compatibility.
- The `CfwInfo.magic` tag in the kernel extension was changed from
  `"LUMA"` to `"OMIB"`. No code in this tree compares it; external tools
  that look for `"LUMA"` will not recognise an Omiiba3DS dump.

### New features
- Omiiba3DS coldboot splash drawn in `arm9/source/draw.c`
  (`omiibaBootSplash`) and invoked from `arm9/source/main.c` only on real
  cold boots (skips firmlaunch / NTR / button-mash). Existing
  user-supplied `splash.bin` / `splashbottom.bin` continue to work as in
  Luma3DS.

### Versioning
- `arm9/Makefile` no longer derives the version from upstream Luma git
  tags. Hardcoded default is `1.3.0`, override with
  `make VERSION_MAJOR=… VERSION_MINOR=… VERSION_BUILD=…`.

### Buffer fixes related to the longer `/omiiba` prefix
- `sysmodules/rosalina/source/menus.c`: `filename` buffers grown to 128 B.
- `arm9/source/firm.c`: `absPath` grown to `32 + 255` B.

### Build helpers
- New scripts under `scripts/`:
  `install-devkitpro-macos.sh`, `env-build.sh`, `push-omiiba-to-github.sh`.

### Documentation
- This `NOTICE-OMIIBA.md`.
- Credits screen in Rosalina/Cow menu still shows the original Luma3DS
  authors and a clear "fork for educational purposes" notice.
- The original Luma3DS `README.md` content is retained; only branding
  strings and paths were updated.

## Source

The complete corresponding source code for the binary released with this
fork is the contents of this repository at the tagged commit. No part of
the firmware is built from undisclosed sources.

## Trademarks

"Luma3DS" and the LumaTeam name are used here purely to identify the
upstream project; no endorsement is implied.
