HUD Position Manager
====================
Version 1.0.1

Move, resize and hide every part of Oblivion Remastered's HUD live, from a settings page in Apocrypha
Menu Framework - and keep the health, magicka and fatigue bars on screen instead of letting them fade.
The Skyrim mod "HUD Position Manager" carried to the remaster as an OBSE64 plugin.

WHAT YOU GET
------------
  * A page "HUD Position Manager" in the Apocrypha Menu Framework: one tab per HUD element (the three
    bars, compass, crosshair, weapon and spell icons, active effects, enemy health, sneak eye, level-up
    gauge, target name and value, subtitles and notifications, breath meter, location name, the warning
    icons (weapon and armour condition, over-encumbered), pop-up notifications, tutorial messages).
  * Per element: move left / right, move up / down (0.1 % of the screen per tick, never off the screen),
    size (about its own centre), hide, "move with" another element, reset. The bars also get Length and
    Height on their own. Each tab says whether your HUD has that element. A "Level" element (your level as
    a text of the game's own style) is added to the HUD, and the quick wheel can be placed in game.
  * Presets: whole layouts as INI files - load one made by someone else, save your own, update it in place.
  * Combined widgets: pick any set of widgets and move them as one on shared sliders. The same tab holds the
    resource bars' "Fill from" (left, centre, right or the game's own - the side the filled part is anchored
    to) and "Length follows the resource" (the bar grows with your maximum health, magicka or fatigue).
  * "Show every element": the elements that only appear during an event (enemy health, sneak eye, active
    effects, subtitles, the level-up gauge, tutorial messages...) are made to show - in the menu and out of it -
    until you turn it off, so you can place them and look at the result.
  * "HUD widget collision" (a widget stops where its edge meets another you placed) and "Snap art edges
    together", both off by default.
  * "Apply my layout" (off = the HUD exactly as the game lays it out), on by default.
  * HUD visibility: "the game decides" (the bars fade out when full, as the game does) or "Always visible"
    (the bars stay shown while you play; menus, dialogue and loading screens still hide the HUD). Each bar
    also has its own "Always visible" switch.
  * Applied live, every frame, on the game's own HUD widgets - offsets on top of where the game puts each
    element, so when the game moves an element itself the offset follows.
  * Eleven languages.

INSTALLATION
------------
  * Everything goes under OblivionRemastered\Binaries\Win64\OBSE\Plugins\: HUDPositionManager.dll, .pdb,
    .ini, and the translation files under ApocryphaMenuFramework\Translations\.
  * Install with your mod manager or drop the OblivionRemastered folder over the game's own. Mod Organizer 2
    users need Root Builder, as for every OBSE64 plugin.
  * Start the game through OBSE64. Requires OBSE64, Address Library for OBSE Plugins and Apocrypha Menu
    Framework for Oblivion Remastered (1.0.1 or newer) for the settings page; without the framework the
    layout in the INI still applies.

SETTINGS
--------
  HUDPositionManager.ini beside the DLL, written by the page. Edit it by hand only with the game closed.

DEBUGGING
---------
  Send the log with any bug report:
  Documents\My Games\Oblivion Remastered\OBSE\Logs\HUDPositionManager.log
  Set uLogLevel=1 in the INI to log every move and every hold. The TestBench tool hud.position reports each
  element's state. The debug symbols (.pdb) ship in this download, so a crash log names this mod's functions.

REQUIREMENTS
------------
  * OBSE64 (Nexus 282), 0.2.2 or newer
  * Address Library for OBSE Plugins (Nexus 4475)
  * Apocrypha Menu Framework for Oblivion Remastered, for the settings page

LICENCE
-------
  GPL-3.0-or-later (LICENSE, NOTICE.md). Built on CommonLibOB64 (GPL-3.0); Dear ImGui (MIT) through the
  framework's own context. Source: https://github.com/ApocryphaRealm/HUDPositionManagerOR
