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
		float       x = 0.0f;              // [<key>] fX - right is positive, in the HUD's own units (1920 x 1080)
		float       y = 0.0f;              // [<key>] fY - down is positive
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
		std::vector<Element> elements = std::vector<Element>(elements::Count());
		int                  logLevel = 2;          // [Log] uLogLevel (rule 14: shipped at info)
	};

	inline constexpr float kMoveX = 960.0f, kMoveY = 540.0f;
	inline constexpr float kScaleMin = 0.25f, kScaleMax = 3.0f;

	Values Snapshot();
	void   Update(const std::function<void(Values&)>& a_change);   // clamps, then saves
	void   ResetElement(std::size_t a_index);                       // that element's defaults, saved
	void   ResetAll();                                               // every element's defaults (the switches stay), saved

	void                  Load();
	bool                  Save();
	std::filesystem::path PluginFolder();   // ...\OblivionRemastered\Binaries\Win64\OBSE\Plugins
	std::filesystem::path IniPath();
}
