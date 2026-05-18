Omiiba Updater
==============

Launch from Homebrew Launcher:

  SD:/3ds/OmiibaUpdater/OmiibaUpdater.3dsx

What it does:

1. Downloads latest boot.firm from GitHub releases.
2. Backs up current SD:/boot.firm to SD:/omiiba/backups/boot.firm.bak.
3. Installs the downloaded file as SD:/boot.firm.

Release requirement:

The GitHub release must include a standalone asset named boot.firm.

Keep a known-good boot.firm backup before using alpha OTA updates.
