Omiiba3DS GBA VC Patch Research
===============================

Current safe AGB_FIRM patch point:

  Show GBA boot screen in patched AGB_FIRM

Not enabled yet:

  Forced GBA VC scaling filters
  Forced GBA VC color/brightness presets

Reason:

  GBA Virtual Console injects run under AGB_FIRM legacy mode. Omiiba currently
  has patchAgbFirm() and patchAgbBootSplash(), but no verified AgbBg/display
  driver patch point for color or scaling. Enabling blind byte patches here
  could black-screen.

Safe current recommendation:

  Use open_agb_firm for SD-card GBA games and Omiiba display presets. Keep VC
  inject display patching in alpha research until verified on hardware.
