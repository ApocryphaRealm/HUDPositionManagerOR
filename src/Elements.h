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
	struct Element
	{
		const char*                      key;        // INI section, TR key suffix, tool name
		const char*                      english;    // the page's tab (TR("HPM_El_<key>"))
		std::vector<const wchar_t*>      classes;    // widget Blueprint class names (the generated class, "_C")
		std::vector<const char*>         names;      // instance names, tried when no class matched
		bool                             fades;      // the game fades it on its own (the bars: a FadeOut animation on their StatusBar, probed 2026-09-29): "Always visible" applies
		const char*                      barLink;    // moves with this element while "Move the three bars together" is on
	};

	inline const std::vector<Element>& All()
	{
		static const std::vector<Element> kAll{
			{ "Health", "Health", { L"WBP_ModernHud_Health_C" }, { "Health", "HealthBar" }, true, nullptr },
			{ "Magicka", "Magicka", { L"WBP_ModernHud_Magicka_C" }, { "Magicka", "MagickaBar" }, true, "Health" },
			{ "Fatigue", "Fatigue", { L"WBP_ModernHud_Fatigue_C" }, { "Fatigue", "FatigueBar" }, true, "Health" },
			{ "Compass", "Compass", { L"WBP_ModernHud_Compass_C" }, { "Compass" }, false, nullptr },
			{ "Crosshair", "Crosshair", { L"WBP_ModernHud_Reticle_C" }, { "WBP_ModernHud_Reticle", "Reticle" }, false, nullptr },
			{ "WeaponIcon", "Weapon icon", { L"WBP_ModernHud_WeaponIcon_C" }, { "WeaponIcon" }, false, nullptr },
			{ "MagicIcon", "Spell icon", { L"WBP_ModernHud_MagicIcon_C" }, { "MagicIcon" }, false, nullptr },
			{ "EffectIcons", "Active effects", { L"WBP_ModernHud_EffectIcons_C" }, { "EffectIcons" }, false, nullptr },
			{ "EnemyHealth", "Enemy health", { L"WBP_ModernHud_StatusBarEnemy_C" }, { "StatusBarEnemy", "EnemyHealth" }, false, nullptr },
			{ "SneakEye", "Sneak eye", { L"WBP_ModernHud_SneakEye_C" }, { "SneakEye" }, false, nullptr },
			{ "LevelUp", "Level-up gauge", { L"WBP_ModernHud_LevelUpGauge_C" }, { "LevelUpGauge" }, false, nullptr },
			{ "Info", "Target name and value", { L"WBP_ModernHud_Info_C" }, { "WBP_ModernHud_Info", "Info" }, false, nullptr },
			{ "Subtitles", "Subtitles and notifications", { L"WBP_ModernHud_Subtitle_C" }, { "WBP_ModernHud_Subtitle", "Subtitle" }, false, nullptr },
			{ "Breath", "Breath meter", { L"WBP_ModernHud_Breath_C" }, { "WBP_ModernHud_Breath", "Breath" }, false, nullptr },
			{ "Location", "Location name", { L"WBP_ModernHud_Area_C" }, { "WBP_ModernHud_Area", "Area" }, false, nullptr },
			{ "DamageIndicators", "Damage direction", { L"WBP_ModernHud_DamageIndicators_C" }, { "DamageIndicators" }, false, nullptr },
			{ "Notifications", "Pop-up notifications", { L"WBP_ModernPrefab_NotificationInHUD_C" }, { "NotificationInHUD" }, false, nullptr },
			{ "Tutorial", "Tutorial messages", { L"WBP_ModernTutorialDisplay_C" }, { "TutorialDisplay" }, false, nullptr },
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
