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

### Added during the owner's rounds, 2026-09-29 (each seen in game unless marked)
- a Presets tab first: preset files listed, loaded, deleted; "Save to" a new preset or an existing one updated in
  place; the owner's layout ships as presets/ApocryphaRealm.ini.
- sliders in 0.1 % ticks (integer sliders under ImGui's fine-tweak modifier for the one call), ranges frozen while a
  slider is held (the mouse drag "teleported"), bounds taken on the drawn art (the compass strip reaches the edge).
- "HUD widget collision" and "Snap art edges together" (both off by default), only among widgets the owner placed.
- the quick wheel as an element (in game only, moved through its slot's padding - its open animation owns the render
  transform); its tab right after the compass. The breath meter's tab after Fatigue; Always-visible switches on the
  breath meter and the compass.
- "Show every element": every event-only element shown through its own show calls, once, silently (its Wwise event
  slots cleared while shown), held when the menu closes; the enemy bar's retainer-box veil raised.
- the Level element: the player's level as a WBP_AltarTextBlock this mod creates on the layout's Overlay.
- Length and Height on the bars' tabs (fLength / fHeight: the render scale per axis on top of Size).
- round 7 (UNTESTED, installed at the next exit): the Combined widgets tab ([Group] sMembers / fX / fY: any set of
  widgets moves as one; "Move the three bars together" removed); "Fill from" per bar (iFill: the bar's material
  re-parented to the game's material instance carrying the wanted static permutation - MIC_UI_ProgressBar_Fatigue
  left, _HealthHUD centre, _MagickaHUD right - with its own parameters carried over); "Length follows the resource"
  (bLinkLength / fPointsPerLength, the maximum from the HUD view model and the player's actor values, indices proven
  against the view model first); Size / Length / Height in hundredths; ue::Handle checks class and name (a dead
  created text passed as alive through a reused slot and address); the enemy bar's preview through
  WBP_ModernTopStats.HandleNPCVisibility / FadeNPCOut; the active effects through the widget's own
  "Update Active Effect Icons" / "Update Active Effects Time Left"; the tutorial's preview ends with ClearDisplay;
  the breath bar filled to 1.0 when Always visible starts holding it.

### Added during the owner's rounds, 2026-09-30
- Combined widgets members pinned to the front of the Layout tab's element tabs, marked "+" (seen in game: "pinned
  tabs are good"); the bumpers walk the tabs through AMF::DeclareInnerTabs (seen in game: "bumpers work").
- Damage direction and Sneak detection indicators, built by the mod on the HUD layer (8 / 12 marks on a ring).
  - Sneak detection reads who can detect the player from the engine: the player's HighProcess detection list (+0x2B8)
    names the actors around them, and each of their HighProcess lists holds its entry for the player - level (+0x08:
    unseen / noticed / seen) and value (+0x10). Fault-guarded, four times a second, within ~60 m. The first build used
    the compass's hostile list, which holds only actors in combat, so sneaking past guards drew nothing.
  - Both draw curved arcs made for this mod (tools/gen-indicator-art.py, Art/DamageArc.png and SneakArc.png beside
    the plugin), imported at run time through KismetRenderingLibrary::ImportFileAsTexture2D and sized from the ring so
    each arc lies on it; the game's T_Line_glow if the import fails. Damage arcs red; sneak arcs white (undetected),
    yellow (partly), red (detected). The sneak-eye flipbook tried in between was dropped at the owner's word.
  - A Radius slider (fRadius, pixels, one per nudge) first on both tabs.

### Not done yet
- tools/gen-dist.py (the shipped INI from the compiled defaults, the eleven translation files), README, NOTICE,
  the dist INI. The build has never run in game.
