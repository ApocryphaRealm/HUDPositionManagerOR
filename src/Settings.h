#pragma once

#include "Elements.h"

// HUDPositionManager.ini beside the plugin. The compiled defaults are the shipped INI's values (rule 16 - the INI is
// generated from these defaults by tools/gen-dist.py); Save() rewrites only this plugin's keys in place with ordinary
// file I/O, so comments and unknown lines survive and a change on the page persists. The page draws on AMF's render
// thread and the HUD is changed on the game thread, so values are read and written only through Snapshot() / Update().

namespace settings
{
	// the indicators' ring (fRadius), pixels
	inline constexpr float kRadiusMin = 40.0f, kRadiusMax = 500.0f;
	inline float DefaultRadius(std::string_view a_native) { return a_native == "damage" ? 150.0f : 110.0f; }

	struct Element
	{
		float       x = 0.0f;              // [<key>] fX - right is positive, a PERCENTAGE of the screen's width (-100..100)
		float       y = 0.0f;              // [<key>] fY - down is positive, a percentage of the screen's height
		float       scale = 1.0f;          // [<key>] fScale - about the element's own centre
		float       stretchX = 1.0f;       // [<key>] fLength - the scale along the element's width, on top of fScale (the bars)
		float       stretchY = 1.0f;       // [<key>] fHeight - the same for its height
		bool        hide = false;          // [<key>] bHide
		// when the element shows in play (2026-10-03, UpsidedownMonkey: "combat hiding ... and idle (exploration) hiding"):
		// 0 always (as the game decides), 1 only in combat, 2 only out of combat
		int         show = 0;              // [<key>] iShow
		bool        alwaysVisible = false; // [<key>] bAlwaysVisible - only for the elements the game fades on its own (the Level gauge: on when the file has no key)
		std::string moveWith;              // [<key>] sMoveWith - another element's key, or empty
		// the resource bars (2026-09-29): where the filled part is anchored - 0 the game's own, 1 left, 2 centre, 3 right
		int         fill = 0;              // [<key>] iFill
		bool        linkLength = false;    // [<key>] bLinkLength - the bar's length follows the player's maximum of that resource
		float       pointsPerLength = 100.0f;   // [<key>] fPointsPerLength - the maximum that makes the bar 1.00x long
		int         grow = 0;              // [<key>] iGrow - 0 both sides, 1 toward the right (anchored left), 2 toward the left
		// the two indicators this mod builds (2026-09-30): the ring's radius in pixels (0 = the indicator's own default)
		float       radius = 0.0f;         // [<key>] fRadius
	};

	// the Combined widgets tab (2026-09-29): any set of elements moves as one on shared sliders, on top of each one's own
	struct Group
	{
		std::vector<std::string> members;   // [Group] sMembers - element keys, comma-separated
		float                    x = 0.0f;  // [Group] fX / fY - percent of the screen, like an element's own
		float                    y = 0.0f;
		bool                     Has(std::string_view a_key) const
		{
			for (const auto& m : members) if (m == a_key) return true;
			return false;
		}
	};

	struct Values
	{
		bool                 enabled = true;        // [General] bEnabled - "Apply my layout"
		bool                 alwaysVisible = false; // [General] bAlwaysVisible - the whole HUD: "Always visible" vs "The game decides"
		bool                 noOverlap = false;     // [General] bWidgetCollision - an element stops where its edge meets another's (2026-09-29; off since the snap)
		bool                 snapEdges = false;     // [General] bSnapEdges (off since the 0.1 % slider ticks, the owner 2026-09-29) - a placed widget's art edges pull onto another placed widget's (2026-09-29)
		float                snapDistance = 6.0f;   // [General] fSnapDistance - how close an edge has to come, in layout units
		bool                 preview = false;       // [General] bPreview - every element shown, event ones through their own show calls, until turned off (2026-09-29)
		bool                 unlocked = false;      // [General] bUnlocked - "Free placement": the move sliders span the whole range, past the screen's edges (2026-10-03)
		std::vector<Element> elements = std::vector<Element>(elements::Count());
		Group                group;
		int                  logLevel = 2;          // [Log] uLogLevel (rule 14: shipped at info)
	};

	inline constexpr float kMoveX = 100.0f, kMoveY = 100.0f;   // percent of the screen (the page narrows it to what fits)
	inline constexpr float kScaleMin = 0.25f, kScaleMax = 3.0f;
	inline constexpr float kPointsMin = 10.0f, kPointsMax = 1000.0f;

	Values Snapshot();
	void   Update(const std::function<void(Values&)>& a_change);   // clamps, then saves
	void   ResetElement(std::size_t a_index);                       // that element's defaults, saved
	void   ResetAll();                                               // every element's defaults (the switches stay), saved

	void                  Load();
	bool                  Save();

	// Presets: whole layouts as INI files in <plugin folder>\HUDPositionManager\presets\<name>.ini - the element
	// sections of the settings file plus [General] bAlwaysVisible, the [Group] section and a [Preset] header (sName, sAuthor,
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
