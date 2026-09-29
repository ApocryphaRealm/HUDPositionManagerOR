# HUDPositionManagerOR - changelog

Written as changes happen, not reconstructed afterwards (rule 61). A version number is issued by the version gate only
once a build is seen working in game (rule 48); until then the work sits under Unreleased.

## Unreleased - 2026-09-29 - untested, never run

### Added
- the owner, 2026-09-29: "port the hud position manager mod over to oblivion remaster and i want it to also have
  toggles for setting the hud to active and visible at all times or defer to the games settings". Plan and research:
  4. plans\hud-position-manager-oblivion\ (PLAN.md, RESEARCH.md).
- the HUD found from WBP_PrimaryGameLayout_C by walking the UMG tree (user widget WidgetTree -> RootWidget, panel
  Slots -> Content), 18 elements matched by widget class (instance names as a fallback), kept by object-array slot and
  found again every 2 s while one is missing.
- per element: move (SetRenderTranslation), size about its centre (SetRenderTransformPivot 0.5 / SetRenderScale),
  hide (Hidden, the game's visibility put back), move with, bars together; the game's own transform read back each frame
  and followed as the base.
- "Always visible" (global, and per element for the ones the game fades): in gameplay (menuMode == 1) the element
  root's RenderOpacity is held at 1 and a Hidden / Collapsed root made visible - UNPROVEN: the fade may live on a
  child or in an animation (the probe in PLAN.md decides).
- the AMF page (tabs per element), the INI with ordinary file I/O, TestBench tool hud.position, the previous log kept as
  HUDPositionManager.prev.log.

### Not done yet
- tools/gen-dist.py (the shipped INI from the compiled defaults, the eleven translation files), README, NOTICE,
  the dist INI. The build has never run in game.
