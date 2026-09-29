#pragma once

// ============================================================================================================
// The HUD elements, from the game's packaged HUD Blueprints (/Game/UI/Modern/HUD/..., read from the .utoc directory
// index 2026-09-29 - 4. plans\hud-position-manager-oblivion\RESEARCH.md 1b). An element is found in the live HUD by
// its widget CLASS (sturdier than an instance name, and the same in every language); `names` are instance names tried
// when no widget of the class is found (a pak HUD mod may rebuild an element under another class). An element that
// is in neither form is reported "not found" and its settings are kept.
// ============================================================================================================

namespace elements
{
	// A reflected call that makes an event-only widget show itself for the preview (the page open): the function and
	// its parameters by name. A text parameter carries a TR key and its English, resolved when the call is made.
	struct PreviewArg
	{
		enum Kind { kBool, kDouble, kText };
		const char*    name;
		Kind           kind;
		bool           b = false;
		double         d = 0.0;
		const char*    trKey = nullptr;
		const char*    english = nullptr;
	};
	struct PreviewCall
	{
		const wchar_t*          fn;
		std::vector<PreviewArg> args;
	};
	inline PreviewArg ArgBool(const char* a_n, bool a_b) { return { a_n, PreviewArg::kBool, a_b }; }
	inline PreviewArg ArgDouble(const char* a_n, double a_d) { return { a_n, PreviewArg::kDouble, false, a_d }; }
	inline PreviewArg ArgText(const char* a_n, const char* a_key, const char* a_en) { return { a_n, PreviewArg::kText, false, 0.0, a_key, a_en }; }

	struct Element
	{
		const char*                      key;        // INI section, TR key suffix, tool name
		const char*                      english;    // the page's tab (TR("HPM_El_<key>"))
		std::vector<const wchar_t*>      classes;    // widget Blueprint class names (the generated class, "_C")
		std::vector<const char*>         names;      // instance names, tried when no class matched
		bool                             fades;      // the game fades it on its own (the bars: a FadeOut animation on their StatusBar, probed 2026-09-29): "Always visible" applies
		const char*                      barLink;    // moves with this element while "Move the three bars together" is on
		// the part of the widget's width that is actually drawn, centred (1 = all of it). The compass's box is 1272
		// units wide but its strip is soft-masked to the middle ~37% (measured off a capture, 2026-09-29: the owner
		// "tried moving the compass and it wouldn't go all the way left or all the way right"); the screen bound is
		// taken on the drawn part, so the strip itself reaches the edge.
		float                            visibleW = 1.0f;
		// moved through its panel slot's padding instead of the render transform: the quick wheel's open animation
		// writes the render transform every frame, so a translation there shows only after the animation (2026-09-29)
		bool                             moveViaSlot = false;
		// the drawn share of the HEIGHT, centred: the three bars' image is 32 units tall with the bar art in the middle
		// 16 (measured off the owner's capture, 2026-09-29: "still too large of a collision box" with the art touching
		// nothing) - the transparent margin inside a PNG is invisible to every geometry call
		float                            visibleH = 1.0f;
		// the preview (2026-09-29): what to call so the widget shows outside its event, and what puts it back
		std::vector<PreviewCall>         previewOn;
		std::vector<PreviewCall>         previewOff;
	};

	inline const std::vector<Element>& All()
	{
		static const std::vector<Element> kAll{
			{ "Health", "Health", { L"WBP_ModernHud_Health_C" }, { "Health", "HealthBar" }, true, nullptr, 1.0f, false, 0.5f },
			{ "Magicka", "Magicka", { L"WBP_ModernHud_Magicka_C" }, { "Magicka", "MagickaBar" }, true, "Health", 1.0f, false, 0.5f },
			{ "Fatigue", "Fatigue", { L"WBP_ModernHud_Fatigue_C" }, { "Fatigue", "FatigueBar" }, true, "Health", 1.0f, false, 0.5f },
			{ "Compass", "Compass", { L"WBP_ModernHud_Compass_C" }, { "Compass" }, false, nullptr, 0.37f },
			{ "Crosshair", "Crosshair", { L"WBP_ModernHud_Reticle_C" }, { "WBP_ModernHud_Reticle", "Reticle" }, false, nullptr },
			{ "WeaponIcon", "Weapon icon", { L"WBP_ModernHud_WeaponIcon_C" }, { "WeaponIcon" }, false, nullptr },
			{ "MagicIcon", "Spell icon", { L"WBP_ModernHud_MagicIcon_C" }, { "MagicIcon" }, false, nullptr },
			{ "EffectIcons", "Active effects", { L"WBP_ModernHud_EffectIcons_C" }, { "EffectIcons" }, false, nullptr },
			{ "EnemyHealth", "Enemy health", { L"WBP_ModernHud_StatusBarEnemy_C" }, { "StatusBarEnemy", "EnemyHealth" }, false, nullptr, 1.0f, false, 1.0f, { { L"SetProgress", { ArgDouble("InProgress", 0.75), ArgBool("IsPreview", true) } } }, { { L"SetProgress", { ArgDouble("InProgress", 0.0), ArgBool("IsPreview", true) } } } },
			{ "SneakEye", "Sneak eye", { L"WBP_ModernHud_SneakEye_C" }, { "SneakEye" }, false, nullptr, 1.0f, false, 1.0f, { { L"UpdateSneakingVisibility", { ArgBool("InSneaking", true) } }, { L"Update Sneak Level", { ArgDouble("InSneakLevel", 0.5) } } }, { { L"UpdateSneakingVisibility", { ArgBool("InSneaking", false) } } } },
			{ "LevelUp", "Level-up gauge", { L"WBP_ModernHud_LevelUpGauge_C" }, { "LevelUpGauge" }, false, nullptr, 1.0f, false, 1.0f, { { L"ToggleLevelUpIconVisibility", { ArgBool("Visible", true) } } }, { { L"ToggleLevelUpIconVisibility", { ArgBool("Visible", false) } } } },
			{ "Info", "Target name and value", { L"WBP_ModernHud_Info_C" }, { "WBP_ModernHud_Info", "Info" }, false, nullptr, 1.0f, false, 1.0f, { { L"ShowHide", { ArgBool("InShow", true) } }, { L"UpdateEmpty", { ArgBool("bIsEmpty", false) } }, { L"UpdateTargedItemName", { ArgText("InName", "HPM_PreviewItem", "Item name") } } }, { { L"UpdateEmpty", { ArgBool("bIsEmpty", true) } }, { L"ShowHide", { ArgBool("InShow", false) } } } },
			{ "Subtitles", "Subtitles and notifications", { L"WBP_ModernHud_Subtitle_C" }, { "WBP_ModernHud_Subtitle", "Subtitle" }, false, nullptr, 1.0f, false, 1.0f, { { L"UpdateSubtitle", { ArgText("InText", "HPM_PreviewSubtitle", "A subtitle appears here while someone speaks.") } }, { L"ShowHide", { ArgBool("InVisibility", true) } } }, { { L"ShowHide", { ArgBool("InVisibility", false) } } } },
			{ "Breath", "Breath meter", { L"WBP_ModernHud_Breath_C" }, { "WBP_ModernHud_Breath", "Breath" }, false, nullptr, 1.0f, false, 1.0f, { { L"UpdateBreathPercentage", { ArgDouble("Percentage", 0.6) } } }, { { L"UpdateBreathPercentage", { ArgDouble("Percentage", 1.0) } } } },
			{ "Location", "Location name", { L"WBP_ModernHud_Area_C" }, { "WBP_ModernHud_Area", "Area" }, false, nullptr, 1.0f, false, 1.0f, { { L"DisplayArea", { ArgText("AreaName", "HPM_PreviewArea", "Area name") } }, { L"Update Visibility", { ArgBool("Visible", true), ArgBool("Area Discovered", true) } } }, { { L"Update Visibility", { ArgBool("Visible", false), ArgBool("Area Discovered", false) } } } },
			{ "DamageIndicators", "Damage direction", { L"WBP_ModernHud_DamageIndicators_C" }, { "DamageIndicators" }, false, nullptr, 1.0f, false, 1.0f, { { L"UpdateOverencumberedVisibility", { ArgBool("bIsOverencumbered", true) } }, { L"Update Weapon Damage Visibility", { ArgBool("InVisible", true), ArgDouble("InHealth", 0.3) } } }, { { L"UpdateOverencumberedVisibility", { ArgBool("bIsOverencumbered", false) } }, { L"Update Weapon Damage Visibility", { ArgBool("InVisible", false), ArgDouble("InHealth", 1.0) } } } },
			{ "Notifications", "Pop-up notifications", { L"WBP_ModernPrefab_NotificationInHUD_C" }, { "NotificationInHUD" }, false, nullptr, 1.0f, false, 1.0f, { { L"Enable Notification", {} } }, {} },
			{ "Tutorial", "Tutorial messages", { L"WBP_ModernTutorialDisplay_C" }, { "TutorialDisplay" }, false, nullptr, 1.0f, false, 1.0f, { { L"LaunchOpenningAnimation", {} } }, { { L"LaunchClosingAnimation", {} } } },
			// the quick wheel shown while playing (the owner, 2026-09-29: "change the location or position of the wheel menu in game
			// only not in the menu") - the HUD layout's own instance; the menu's quick-keys page is another class and is not touched
			{ "QuickWheel", "Quick wheel", { L"WBP_ModernMenu_QuickKeys_C" }, { "WBP_ModernMenu_QuickKeys" }, false, nullptr, 1.0f, true },
		};
		return kAll;
	}

	inline std::size_t Count() { return All().size(); }

	// the element's index by key; -1 when there is none
	inline int IndexOf(std::string_view a_key)
	{
		const auto& all = All();
		for (std::size_t i = 0; i < all.size(); ++i) {
			if (a_key == all[i].key) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}
}
