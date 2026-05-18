Omiiba Updater
==============

Launch from Homebrew Launcher:

  SD:/3ds/OmiibaUpdater/OmiibaUpdater.3dsx

What it does:

1. Downloads latest boot.firm from GitHub releases.
2. Rejects obviously wrong download sizes.
3. Backs up current SD:/boot.firm to SD:/omiiba/backups/boot.firm.bak.
4. Installs the downloaded file as SD:/boot.firm.
5. Restores the old boot.firm if the final install rename fails.

Release requirement:

The GitHub release must include a standalone asset named boot.firm.

Keep a known-good boot.firm backup before using alpha OTA updates.
