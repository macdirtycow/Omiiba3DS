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

Date of changes: **2026-05-10 / 2026-05-11**, updated **2026-05-13** (v1.3.1 colours, v1.3.2 splash extras, v1.3.3 no pre-firm LCD power-down, v1.3.4 splash readability, v1.4.0 Omiiba Boot Hub, v1.4.1 fast Boot Hub exit, **v1.4.2** Hub-first polish).

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
  a burnt-orange / amber-brown (`#CC6600`). (v1.3.0 briefly shipped a bright
  green; this was changed before public adoption.)
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
- Optional one-line user tagline read from `/omiiba/boot_message.txt`
  (max ~60 chars, first line only) and rendered on the top splash.
- Rotating "Cow tip" shown on the bottom splash; rotation state is kept in
  the 1-byte file `/omiiba/.cow_tip_state`. Tip pool is hardcoded in
  `arm9/source/draw.c::pickCowTip`.
- Modernised **cold-boot splash** (`arm9/source/draw.c`): dark background,
  raised panel cards, thin accent bars in the title colour, cleaner
  typography, a card layout on the bottom screen, **2× scaled** main title
  (`OMIIBA3DS`) for readability, lighter muted body text, shortened tips so
  they fit the card, automatic two-line wrap for edge cases, and a slightly
  wider bottom panel so text stays inside the frame.
- **Omiiba Boot Hub** (`arm9/source/config.c`): a new entry in the boot
  flow that replaces SELECT's first screen and groups Omiiba-specific shortcuts.
  The classic Luma/Omiiba configuration menu remains available as
  `Advanced boot settings`. The first version
  includes normal boot, chainloader, GodMode9 tools, diagnostics, profiles,
  payload manager and theme/settings entries. Low-level maintenance remains a
  GodMode9 handoff rather than native NAND/key/injection tooling in Omiiba.
  The Hub distinguishes fast `Continue normal boot` (skips config writes when
  no settings changed) from explicit `Save settings and boot`.
- **GodMode9 system save dump handoff**: the release skeleton includes
  `gm9/scripts/Omiiba_System_Save_Dump.gm9`, a read-only dump script that
  copies SysNAND system save data from `1:/data/$[SYSID0]/sysdata` to
  `SD:/gm9/out/...`. Omiiba only launches GodMode9 and documents the script;
  it does not perform NAND writes or key/title decryption.
- **Setup wizard** (`arm9/source/config.c`): a guided first-boot / on-demand
  flow for safe Omiiba defaults. It can enable splash-before-payloads, set a
  bright boot-menu brightness level, enable game patching, optionally enable
  external FIRMs/modules, and explain GodMode9 placement. Wizard choices update
  the same pending settings displayed by `Advanced boot settings`. Completion is tracked
  with `/omiiba/.setup_wizard_done`.
- **Read-only diagnostics** (`arm9/source/config.c`): Boot Hub diagnostics now
  check config presence, GodMode9 payload presence, the bundled GM9 system-save
  script, `.firm` payload count, SD/CTRNAND `boot.firm`, EmuNAND detection,
  selected splash/game-patching state, setup-wizard marker and expected
  `/omiiba` folders. These checks do not modify NAND or SD state.
- **Profiles** (`arm9/source/config.c`): Boot Hub profiles apply named presets
  to existing Omiiba/Luma configuration options only. Current profiles are
  Default, Safe, Performance, Plugin/Game Modding and Developer/GDB. Each
  profile previews the changes and asks for confirmation before modifying the
  pending settings.
- **Payload Manager** (`arm9/source/config.c`): lists `.firm` payloads under
  `/omiiba/payloads`, launches selected payloads through the existing
  chainloader path, shows hotkey naming help, and can create non-destructive
  hotkey copies such as `x_GodMode9.firm` without deleting or renaming the
  original payload.
- **Theme/settings** (`arm9/source/config.c`, `arm9/source/draw.c`): the
  cold-boot splash now supports built-in palettes stored in
  `/omiiba/.boot_theme` (`Omiiba amber`, `Midnight blue`, `Pasture green`,
  `Berry purple`). The Boot Hub also documents `/omiiba/boot_message.txt` and
  can reset the rotating Cow tip state by deleting `/omiiba/.cow_tip_state`.
- `arm9/source/main.c`: **No LCD power-down before `launchFirm()`.** Upstream
  Luma called `deinitScreens()` here, which blanks the panel until the OS
  redraws; Omiiba3DS omits that so the last splash frame stays visible
  (frozen) during FIRM load instead of a long black gap.

### Versioning
- `arm9/Makefile` no longer derives the version from upstream Luma git
  tags. Hardcoded default is `1.4.2`, override with
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
