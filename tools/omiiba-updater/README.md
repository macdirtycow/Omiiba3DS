# Omiiba Updater

Omiiba Updater is a small 3DS homebrew app for phase-2 OTA updates.

It downloads the latest `boot.firm` release asset from:

```text
https://github.com/macdirtycow/Omiiba3DS/releases/latest/download/boot.firm
```

Install flow:

1. Download `boot.firm` to `SD:/omiiba/update/boot.firm.tmp`.
2. Reject the download if the size is outside the expected boot.firm range.
3. Move the current `SD:/boot.firm` to `SD:/omiiba/backups/boot.firm.bak`.
4. Move the downloaded file to `SD:/boot.firm`.
5. Restore the old `boot.firm` if the final install rename fails.

The app intentionally does not run inside the arm9 bootloader. It runs as
normal 3DS homebrew where network services are available.
