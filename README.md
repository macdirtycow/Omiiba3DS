# Omiiba3DS

![License](https://img.shields.io/badge/License-GPLv3-blue.svg)

*Nintendo 3DS "Custom Firmware"*

> **Omiiba3DS is an unofficial fork of [Luma3DS](https://github.com/LumaTeam/Luma3DS),
> rebranded for educational purposes.**
> All firmware functionality is the work of the original **LumaTeam**
> (AuroraWright, TuxSH, PabloMK7 and contributors). This fork is not
> affiliated with, endorsed by, or supported by LumaTeam — please direct
> bug reports about anything other than the Omiiba3DS-specific changes to
> the upstream project.
>
> A summary of every change made in this fork (as required by GPLv3 §5(a))
> is in [`NOTICE-OMIIBA.md`](NOTICE-OMIIBA.md).
> The full original GPLv3 text is in [`LICENSE`](LICENSE).

![Boot menu screenshot](img/boot_menu_v1321.png)
![Rosalina menu screenshot](img/rosalina_menu_v1321.png)

## Description
**Luma3DS** (the project this fork is built on) patches and reimplements significant parts of the system software running on all models of the Nintendo 3DS family of consoles. It aims to greatly improve the user experience and support the 3DS far beyond its end-of-life. Features inherited by Omiiba3DS include:

* **First-class support for homebrew applications**
* **Rosalina**, an overlay menu (triggered by <kbd>L+Down+Select</kbd> by default), allowing things like:
    * Taking screenshots while in game
    * Blue light filters and other screen filters
    * Input redirection to play with external devices, such as controllers
    * Using cheat codes
    * Setting time and date accurately from the network (NTP)
    * ... and much more!
* **Many game modding features**, such as, but not limited to:
    * Game plugins (in 3GX format)
    * Per-game language overrides ("locale emulation")
    * Asset content path redirection ("LayeredFS")
* **Support for user-provided patches and/or full "system modules" replacements**, an essential feature for Nintendo Network replacements (amongst other projects)
* A **fully-fledged GDB stub**, allowing homebrew developers and reverse-engineers alike to work much more efficiently
* Ability to chainload other firmware files, including other versions of itself
* ... and much more!

## Installation and upgrade
Omiiba3DS requires [boot9strap](https://github.com/SciresM/boot9strap) to run.

Once boot9strap has been installed, simply download the [latest release archive](https://github.com/macdirtycow/Omiiba3DS/releases/latest) and extract the archive onto the root of your SD card to "install" or to upgrade Omiiba3DS alongside the [homebrew menu and certs bundle](https://github.com/devkitPro/3ds-hbmenu) shipped with it. Replace existing files and merge existing folders if necessary.

Configuration, layered FS data, plugins, screenshots, and payloads now live under the **`omiiba`** folder on the SD card root (`/omiiba`, or `/rw/omiiba` when booting from CTRNAND without an SD card). On first boot, if that folder is missing but the legacy **`luma`** folder is present, Omiiba3DS **renames `luma` to `omiiba`** so existing setups keep working without manual copying.

## Basic usage
**The Omiiba Boot Hub** is accessed by pressing <kbd>Select</kbd> at boot. The classic Luma/Omiiba configuration screen is still available from the Hub as **Advanced boot settings**. The configuration file is stored in `/omiiba/config.ini` on the SD card (or `/rw/omiiba/config.ini` on the CTRNAND partition if Omiiba3DS has been launched from the CTRNAND partition, which happens when SD card is missing).

The Hub groups Omiiba-specific shortcuts such as GodMode9 tools, GBA Labs, DS Labs, diagnostics, profiles, payload management, and theme settings. `Continue normal boot` skips rewriting `config.ini` when nothing changed, while `Save settings and boot` explicitly saves pending settings first. Profiles provide named presets (`Default`, `Safe`, `Performance`, `Plugin/Game Modding`, `Developer/GDB`) that preview and apply existing safe boot options. Payload Manager lists `.firm` payloads, launches selected payloads, shows hotkey naming help, and can create safe hotkey copies such as `x_GodMode9.firm` without deleting the original file. Theme/settings lets you choose a built-in cold-boot splash palette (`Omiiba amber`, `Midnight blue`, `Pasture green`, `Berry purple`), shows `boot_message.txt` help, and resets rotating Cow tips. The diagnostics screen is read-only and checks items such as `config.ini`, GodMode9, `open_agb_firm`, DS/TWiLight folders, the bundled GM9 script, payload count, SD/CTRNAND `boot.firm`, EmuNAND detection, setup-wizard status, and the expected `/omiiba` folders. Risky maintenance actions stay delegated to GodMode9; place `GodMode9.firm` at `/omiiba/payloads/GodMode9.firm` to use that shortcut.

**The setup wizard** appears on first configuration creation and is also available from the Omiiba Boot Hub. It guides safe defaults for splash position, brightness, game patching, optional external FIRMs/modules, and GodMode9 placement. Wizard choices update the same underlying settings shown later in **Advanced boot settings**. It writes a small `.setup_wizard_done` marker under `/omiiba` after running.

**System save dump workflow:** install the release zip so `gm9/scripts/Omiiba_System_Save_Dump.gm9` is present, put `GodMode9.firm` in `/omiiba/payloads/`, then open <kbd>Select</kbd> at boot → **Omiiba Boot Hub** → **GodMode9 tools** → **System save dump script**. In GodMode9 run <kbd>HOME</kbd> → **Scripts** → `Omiiba_System_Save_Dump`.

**GBA Labs / open_agb_firm workflow:** the release zip bundles the official `open_agb_firm` payload at `/omiiba/payloads/open_agb_firm.firm` and its GBA database at `/3ds/open_agb_firm/gba_db.bin`. Open <kbd>Select</kbd> at boot → **Omiiba Boot Hub** → **GBA Labs** → **Display presets** to write `/3ds/open_agb_firm/config.ini` with scaler, color, brightness and battery presets, then choose **Start open_agb_firm**. This is the stable GBA improvement path for now: it launches open_agb_firm directly for SD-card GBA ROM use. The AGB_FIRM scaling and color entries in GBA Labs are intentionally labelled research because Virtual Console GBA injects run in legacy AGB_FIRM mode, where Rosalina-style live filters are not available without deeper FIRM/display-driver patching.

**DS Labs / TWiLight workflow:** the release zip bundles the official TWiLight Menu++ 3DS package (`TWiLight Menu.cia`, `BOOT.NDS`, `_nds`, and `roms`). Install `SD:/TWiLight Menu.cia` with FBI, place DS ROMs in the recommended `SD:/roms/nds/` folder, then use **Omiiba Boot Hub** → **DS Labs** to check common setup paths or create the DS ROM folders. DS Labs also documents the existing TWL external filter hook using `/omiiba/twl_upscaling_filter.bin`.

**The chainloader menu** is accessed by pressing <kbd>Start</kbd> at boot, or from the configuration menu. Payloads are expected to be located in `/omiiba/payloads` with the `.firm` extension; if there is only one such payload, the aforementionned selection menu will be skipped. Hotkeys can be assigned to payload, for example `x_test.firm` will be chainloaded when <kbd>X</kbd> is pressed at boot.

**The overlay menu, Rosalina**, has a default button combination: <kbd>L+Down+Select</kbd>. For greater flexbility, most Rosalina menu settings aren't saved automatically, hence the "Save settings" option.

**GDB ports**, when enabled, are `4000-4002` for the normal ports. Use of `attach` in "extended-remote" mode, alongside `info os processes` is supported and encouraged (for reverse-engineering, also check out `monitor getmemregions`). The port for the break-on-start feature is `4003` without "extended-remote". Both devkitARM-patched GDB and IDA Pro (without "stepping support" enabled) are actively supported.

We have a wiki, however it is currently very outdated.

## What this fork actually changes

Everything in the *Description* section above was already present in upstream Luma3DS. The Omiiba3DS-specific differences are:

* Project rebranded from "Luma3DS" / "LumaTeam" to "Omiiba3DS" in user-visible strings, file names and config paths
* The on-card data folder moved from `/luma` to `/omiiba`, with an automatic one-shot migration on first boot
* A custom textual coldboot splash screen (existing `splash.bin` / `splashbottom.bin` files keep working)
* Rosalina overlay menu retitled "Cow menu" with a burnt-orange / amber-brown title color (`#CC6600`)
* Hardcoded version `v1.4.4` (no longer derived from upstream Luma git tags)
* Optional one-line tagline from `/omiiba/boot_message.txt` shown on the boot splash
* Rotating "Cow tip" on the bottom splash (state in `/omiiba/.cow_tip_state`)
* **Cold-boot splash:** dark panels and accent bars, 2× scaled `OMIIBA3DS` logo, readable muted text, rotating tips kept inside the bottom card (wrapped when needed)
* **Omiiba Boot Hub:** a boot-menu hub for GodMode9 tools, diagnostics, profiles, payload management and theme settings, while keeping low-level dump/inject work in GodMode9
* **Setup wizard:** first-boot / on-demand guided setup for safe defaults and GodMode9 placement
* **Diagnostics:** read-only boot health checks for config, payloads, GodMode9, GM9 scripts, boot.firm locations, EmuNAND and expected folders
* **Profiles:** named presets for existing Omiiba options (`Default`, `Safe`, `Performance`, `Plugin/Game Modding`, `Developer/GDB`) with confirmation before applying
* **Payload Manager:** lists and launches `.firm` payloads, explains hotkey naming, and can copy payloads to hotkey-prefixed filenames without destructive renames
* **Theme/settings:** built-in boot splash palettes stored in `/omiiba/.boot_theme`, custom `boot_message.txt` help, and Cow tip rotation reset
* **GBA Labs:** experimental Boot Hub area with a stable `open_agb_firm` launcher, SD-only display preset writer for `/3ds/open_agb_firm/config.ini`, and honest research notes for future AGB_FIRM scaling/filter and color-preset work
* **DS Labs:** bundled TWiLight Menu++ / nds-bootstrap setup checks, `SD:/TWiLight Menu.cia` install guidance, recommended `SD:/roms/nds/` folder creation and TWL external filter help
* A few helper build scripts under `scripts/`

For the full per-file change list, see [`NOTICE-OMIIBA.md`](NOTICE-OMIIBA.md).

## Components (inherited from Luma3DS)

The firmware consists of multiple components, all originally written for **Luma3DS** by LumaTeam over many years. Omiiba3DS reuses them as-is apart from the rebrand:

* **arm9**, **arm11**: baremetal main settings menu, chainloader and firmware loader. Aside from showing settings and chainloading to other homebrew firmware files on demand, it is responsible for patching the official firmware to modify `Process9` code and to inject all other custom components. This was the first component ever written for this project, in 2015
* **k11_extension**: code extending the Arm11 `NATIVE_FIRM` kernel (`Kernel11`). It is injected by the above mentioned baremetal loader into the kernel by hooking its startup code, then hooks itself into the rest of the kernel. Its features include hooking system calls (SVCs), introducing new SVCs and hooking into interprocess communications, to bypass limitations in Nintendo's system design. This is the component that allows Rosalina to pause other processes on overlay menu entry, for example. This was written at a time when we didn't fully reverse-engineer the kernel, and originally released in 2017 alongside Rosalina. Further hooks for "game plugin" support have been merged in 2023
* **sysmodules**: reimplementation of "system modules" (processes) of the 3DS's OS (except for Rosalina being custom), currently only initial processes loaded directly in-memory by the kernel ("kernel initial process", or KIP in short)
    * **loader**: process that loads non-KIP processes from storage. Because this is the perfect place to patch/replace executable code, this is where all process patches are done, enabling in particular "game modding" features. This is also the sysmodule handling 3DSX homebrew loading. Introduced in 2016
    * _**rosalina**_: the most important custom KIP: overlay menu, GDB server, `err:f` (fatal error screen) reimplementation, and much more. Introduced in mid-2017, and has continuously undergone changes and received many external contributions ever since
    * **pxi**: Arm11<>Arm9 communication KIP, reimplemented just for the sake of it. Introduced late 2017
    * **sm**: service manager KIP, reimplemented to remove service access control restrictions. Introduced late 2017
    * **pm**: process manager KIP reponsible of starting/terminating processes and instructing `loader` to load them. The reimplemention allows for break-on-start GDB feature in Rosalina, as well as lifting FS access control restrictions the proper way. Introduced in 2019

## Upstream maintainers (Luma3DS)

The firmware itself is the work of LumaTeam — please credit them, not this fork:

* **[@TuxSH](https://github.com/TuxSH)**: lead developer of Luma3DS, created and maintains most features. Joined in 2016
* **[@AuroraWright](https://github.com/AuroraWright)**: author of Luma3DS, implemented the core features (most of the baremetal boot settings menu and firmware loading code). Created the project in 2015, currently inactive
* **[@PabloMK7](https://github.com/PabloMK7)**: maintainer of the plugin loader feature merged for the Luma3DS v13.0 release. Joined in 2023

## Maintainer of this fork

* **[@Macdirtycow](https://github.com/Macdirtycow)** — created Omiiba3DS in May 2026 for educational purposes. Only the items listed in [`NOTICE-OMIIBA.md`](NOTICE-OMIIBA.md) are this fork's work.

## Roadmap (upstream Luma3DS)

These items are upstream Luma3DS plans, **not commitments by this fork**. Any of them landing in Omiiba3DS would be by merging future Luma3DS releases:

* Full reimplementation of `TwlBg` and `AgbBg`. This will allow much better, and more configurable, upscaling for top screen in DS and GBA games (except on Old 2DS). This is currently being developed privately in C++23 by upstream (no ETA). While this is quite a difficult endeavor as this requires rewriting the entire driver stack in semi-bare-metal (limited kernel with no IPC), this is the most critical feature Luma3DS hopes to ship and will make driver sysmodule reimplementation trivial
* Reimplementation of `Process9` for `TWL_FIRM` and `AGB_FIRM` to allow for more features in DS and GBA compatibility mode (ones that require file access)
* Eventually, a full `Kernel11` reimplementation

## Known issues

* **Cheat engine crashes with some applications, in particular Pokémon games**: there is a race condition in Nintendo's `Kernel11` pertaining to attaching a new `KDebugThread` to a `KThread` on thread creation, and another thread null-dereferencing `thread->debugThread`. This causes the cheat engine to crashes games that create and destroy many threads all the time (like Pokémon).
    * For these games, having a **dedicated "game plugin"** is the only alternative until `Kernel11` is reimplemented.
* **Applications reacting to Rosalina menu button combo**: Rosalina merely polls button input at an interval to know when to show the menu. This means that the Rosalina menu combo can sometimes be processed by the game/process that is going to be paused.
    * You can **change the menu combo** in the "Miscellaneous options" submenu (then save it with "Save settings" in the main menu) to work around this.

## Building from source

To build Omiiba3DS, the following is needed:
* git
* [makerom](https://github.com/jakcron/Project_CTR) in `$PATH`
* [firmtool](https://github.com/TuxSH/firmtool) installed
* up-to-date devkitARM and libctru:
    * install `dkp-pacman` (or, for distributions that already provide pacman, add repositories): https://devkitpro.org/wiki/devkitPro_pacman
    * install packages from `3ds-dev` metapackage: `sudo dkp-pacman -S 3ds-dev --needed`
    * while libctru and Omiiba3DS releases are kept in sync, you may have to build libctru from source for non-release Omiiba3DS commits

While Omiiba3DS releases are bundled with `3ds-hbmenu`, Omiiba3DS actually compiles into one single file: `boot.firm`. Just copy it over to the root of your SD card ([ftpd](https://github.com/mtheall/ftpd) is the easiest way to do so), and you're done.

## Licensing
This software is licensed under the terms of the GPLv3. You can find a copy of the license in the [`LICENSE`](LICENSE) file. Modifications made by this fork are documented in [`NOTICE-OMIIBA.md`](NOTICE-OMIIBA.md), as required by GPLv3 §5(a).

Files in the GDB stub are instead triple-licensed as MIT or "GPLv2 or any later version", in which case it's specified in the file header. PM, SM, PXI reimplementations are also licensed under MIT. All upstream copyright notices and per-file license headers have been preserved unchanged.

The release archive also bundles an unmodified official `open_agb_firm` payload for convenience. `open_agb_firm` is maintained by [profi200 and contributors](https://github.com/profi200/open_agb_firm) and is licensed under GPLv3. Its license files, upstream README, source link, bundled release tag and Omiiba bundle notice are included under `THIRD_PARTY/open_agb_firm/` in the release zip.

## Credits

The list below is the original Luma3DS credits — preserved verbatim, because every line of it is also true of Omiiba3DS:

* **[@AuroraWright](https://github.com/AuroraWright)**, **[@TuxSH](https://github.com/TuxSH)** and **[@PabloMK7](https://github.com/PabloMK7)** — authors and maintainers of [Luma3DS](https://github.com/LumaTeam/Luma3DS), on which Omiiba3DS is entirely based
* **[@devkitPro](https://github.com/devkitPro)** (especially **[@fincs](https://github.com/fincs)**, **[@WinterMute](https://github.com/WinterMute)** and **[@mtheall](https://github.com/mtheall)**) for providing quality and easy-to-use toolchains with bleeding-edge GCC, and for their continued technical advice
* **[@Nanquitas](https://github.com/Nanquitas)** for the initial version of the game plugin loader code as well as very useful contributions to the GDB stub
* **[@piepie62](https://github.com/piepie62)** for the current implementation of the Rosalina cheat engine, **Duckbill** for its original implementation
* **[@panicbit](https://github.com/panicbit)** for the original implementation of screen filters in Rosalina
* **[@jasondellaluce](https://github.com/jasondellaluce)** for LayeredFS
* **[@LiquidFenrir](https://github.com/LiquidFenrir)** for the memory viewer inside Rosalina's "Process List"
* **ChaN** for [FatFs](http://elm-chan.org/fsw/ff/00index_e.html)
* Everyone who has contributed to the Luma3DS repository over the years
* Everyone who has assisted Luma3DS end-users
* Everyone who has provided constructive feedback to Luma3DS
