#pragma once

#include "Elements.h"

// HUDPositionManager.ini beside the plugin. The compiled defaults are the shipped INI's values (rule 16 - the INI is
// generated from these defaults by tools/gen-dist.py); Save() rewrites only this plugin's keys in place with ordinary
// file I/O, so comments and unknown lines survive and a change on the page persists. The page draws on AMF's render
// thread and the HUD is changed on the game thread, so values are read and written only through Snapshot() / Update().

namespace settings
{
	struct Element
	{
		float       x = 0.0f;              // [<key>] fX - right is positive, a PERCENTAGE of the screen's width (-100..100)
		float       y = 0.0f;              // [<key>] fY - down is positive, a percentage of the screen's height
		float       scale = 1.0f;          // [<key>] fScale - about the element's own centre
		bool        hide = false;          // [<key>] bHide
		bool        alwaysVisible = false; // [<key>] bAlwaysVisible - only for the elements the game fades on its own
		std::string moveWith;              // [<key>] sMoveWith - another element's key, or empty
	};

	struct Values
	{
		bool                 enabled = true;        // [General] bEnabled - "Apply my layout"
		bool                 linkBars = true;       // [General] bLinkBars - Magicka and Fatigue move with Health
		bool                 alwaysVisible = false; // [General] bAlwaysVisible - the whole HUD: "Always visible" vs "The game decides"
		bool                 noOverlap = true;      // [General] bWidgetCollision - an element stops where its edge meets another's (2026-09-29)
		std::vector<Element> elements = std::vector<Element>(elements::Count());
		int                  logLevel = 2;          // [Log] uLogLevel (rule 14: shipped at info)
	};

	inline constexpr float kMoveX = 100.0f, kMoveY = 100.0f;   // percent of the screen (the page narrows it to what fits)
	inline constexpr float kScaleMin = 0.25f, kScaleMax = 3.0f;

	Values Snapshot();
	void   Update(const std::function<void(Values&)>& a_change);   // clamps, then saves
	void   ResetElement(std::size_t a_index);                       // that element's defaults, saved
	void   ResetAll();                                               // every element's defaults (the switches stay), saved

	void                  Load();
	bool                  Save();

	// Presets: whole layouts as INI files in <plugin folder>\HUDPositionManager\presets\<name>.ini - the element
	// sections of the settings file plus [General] bLinkBars / bAlwaysVisible and a [Preset] header (sName, sAuthor,
	// sNote). Other authors ship theirs into that folder; the page lists, loads, saves and deletes them (2026-09-29).
	struct PresetInfo
	{
		std::filesystem::path path;
		std::string           name, author, note;
	};
	std::filesystem::path   PresetsFolder();
	std::vector<PresetInfo> ListPresets();                                  // by name, case-insensitively
	bool                    LoadPreset(const std::filesystem::path& a_path);  // applies its layout and saves the settings
	std::filesystem::path   SavePreset(std::string a_name, const std::string& a_author, const std::string& a_note);   // the current layout; empty on failure
	bool                    UpdatePreset(const std::filesystem::path& a_path);   // the current layout into an existing preset (its header kept)
	bool                    DeletePreset(const std::filesystem::path& a_path);
	std::filesystem::path PluginFolder();   // ...\OblivionRemastered\Binaries\Win64\OBSE\Plugins
	std::filesystem::path IniPath();
}
