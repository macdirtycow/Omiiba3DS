Drop your custom .firm chainload payloads here.
Files named like x_test.firm will boot when X is held at startup.
The Omiiba Boot Hub Payload Manager can create these hotkey copies
for you without deleting the original payload.

Supported examples:
- x_GodMode9.firm boots when X is held at startup.
- y_tools.firm boots when Y is held at startup.
- b_recovery.firm boots when B is held at startup.
- a_payload.firm boots when L + A is held at startup.
- start_payload.firm boots when L + START is held at startup.
- select_payload.firm boots when L + SELECT is held at startup.

Bundled:
- open_agb_firm is included as:
  SD:/omiiba/payloads/open_agb_firm.firm
- Its GBA database is included as:
  SD:/3ds/open_agb_firm/gba_db.bin

Recommended:
- Put GodMode9.firm here as:
  SD:/omiiba/payloads/GodMode9.firm

Then hold SELECT at boot, open:
Omiiba Boot Hub -> GodMode9 tools

For GBA SD-card ROM booting, open:
Omiiba Boot Hub -> GBA Labs -> Start open_agb_firm

For scaler/color/backlight presets, open:
Omiiba Boot Hub -> GBA Labs -> Display presets

Preset settings are written to:
SD:/3ds/open_agb_firm/config.ini

For DS games, the release zip includes TWiLight Menu++ / nds-bootstrap.
Install this CIA with FBI:
SD:/TWiLight Menu.cia

Recommended DS ROM folder:
SD:/roms/nds/

Then hold SELECT at boot, open:
Omiiba Boot Hub -> DS Labs

For Virtual Console title patches, open:
Omiiba Boot Hub -> VC Patch Helper

Patch root:
SD:/omiiba/titles/<16-digit TITLEID>/

Supported helper layout:
code.ips, code.bps, code.bin, exheader.bin, locale.txt, romfs/

For system save dumps, use the release zip's script:
SD:/gm9/scripts/Omiiba_System_Save_Dump.gm9

In GodMode9:
HOME -> Scripts -> Omiiba_System_Save_Dump
