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
		enum Kind { kBool, kDouble, kText, kObjects, kDoubles };
		const char*    name;
		Kind           kind;
		bool           b = false;
		double         d = 0.0;
		const char*    trKey = nullptr;
		const char*    english = nullptr;
		std::vector<const wchar_t*> objects;   // kObjects: object paths (StaticFindObject) into a TArray<UObject*>
		std::vector<double>         doubles;   // kDoubles: into a TArray<double> (a Blueprint float array)
	};
	struct PreviewCall
	{
		const wchar_t*          fn;
		std::vector<PreviewArg> args;
		// the call goes to the nearest OUTER user widget of this class instead of the element itself (the enemy bar is
		// shown and faded by its parent, WBP_ModernTopStats, 2026-09-29)
		const wchar_t*          outerClass = nullptr;
		// or to the widget this OBJECT PROPERTY of the element names (the top-stats block's NPC_Health bar)
		const char*             innerProp = nullptr;
	};
	inline PreviewArg ArgBool(const char* a_n, bool a_b) { return { a_n, PreviewArg::kBool, a_b }; }
	inline PreviewArg ArgDouble(const char* a_n, double a_d) { return { a_n, PreviewArg::kDouble, false, a_d }; }
	inline PreviewArg ArgText(const char* a_n, const char* a_key, const char* a_en) { return { a_n, PreviewArg::kText, false, 0.0, a_key, a_en }; }
	inline PreviewArg ArgObjects(const char* a_n, std::vector<const wchar_t*> a_paths) { PreviewArg a{ a_n, PreviewArg::kObjects }; a.objects = std::move(a_paths); return a; }
	inline PreviewArg ArgDoubles(const char* a_n, std::vector<double> a_v) { PreviewArg a{ a_n, PreviewArg::kDoubles }; a.doubles = std::move(a_v); return a; }

	struct Element
	{
		const char*                      key;        // INI section, TR key suffix, tool name
		const char*                      english;    // the page's tab (TR("HPM_El_<key>"))
		std::vector<const wchar_t*>      classes;    // widget Blueprint class names (the generated class, "_C")
		std::vector<const char*>         names;      // instance names, tried when no class matched
		bool                             fades = false;   // the game fades it on its own (the bars: a FadeOut animation on their StatusBar, probed 2026-09-29): "Always visible" applies
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
		// what to call once when "Always visible" starts holding the widget up without the preview (the breath bar came
		// up empty until the game first updated it, 2026-09-29)
		std::vector<PreviewCall>         holdOn;
		// what to call when that hold ends (the top-stats block faded out again)
		std::vector<PreviewCall>         holdOff;
		// an element this mod CREATES on the HUD from one of the game's own widget classes (found by class name), because
		// the game has no such widget: the Level text (2026-09-29)
		const char*                      createClass = nullptr;
		// a resource bar: Length and Height beside Size on its tab, and "Fill from" on the Combined widgets tab (2026-09-29)
		bool                             bar = false;
		// a bar whose length may follow the player's maximum of that resource ("Length follows the resource")
		bool                             stat = false;
	};

	inline const std::vector<Element>& All()
	{
		// the three effect icons the preview shows in the active-effects list: the game's own restoration / alteration icons
		static const std::vector<const wchar_t*> kPreviewEffectIcons{
			L"/Game/Art/UI/Icons/Dynamic_Icons/menus/icons/magic/restoration_icons/T_restore_restoration.T_restore_restoration",
			L"/Game/Art/UI/Icons/Dynamic_Icons/menus/icons/magic/alteration_icons/T_shield_alteration.T_shield_alteration",
			L"/Game/Art/UI/Icons/Dynamic_Icons/menus/icons/magic/restoration_icons/T_fortify_restoration.T_fortify_restoration",
		};
		static const std::vector<Element> kAll{
			{ .key = "Health", .english = "Health", .classes = { L"WBP_ModernHud_Health_C" }, .names = { "Health", "HealthBar" }, .fades = true, .visibleH = 0.5f, .bar = true, .stat = true },
			{ .key = "Magicka", .english = "Magicka", .classes = { L"WBP_ModernHud_Magicka_C" }, .names = { "Magicka", "MagickaBar" }, .fades = true, .visibleH = 0.5f, .bar = true, .stat = true },
			{ .key = "Fatigue", .english = "Fatigue", .classes = { L"WBP_ModernHud_Fatigue_C" }, .names = { "Fatigue", "FatigueBar" }, .fades = true, .visibleH = 0.5f, .bar = true, .stat = true },
			{ .key = "Breath", .english = "Breath meter", .classes = { L"WBP_ModernHud_Breath_C" }, .names = { "WBP_ModernHud_Breath", "Breath" }, .fades = true,
				.previewOn = { { L"UpdateBreathPercentage", { ArgDouble("Percentage", 0.6) } } }, .previewOff = { { L"UpdateBreathPercentage", { ArgDouble("Percentage", 1.0) } } },
				.holdOn = { { L"UpdateBreathPercentage", { ArgDouble("Percentage", 1.0) } } }, .bar = true },
			{ .key = "Compass", .english = "Compass", .classes = { L"WBP_ModernHud_Compass_C" }, .names = { "Compass" }, .fades = true, .visibleW = 0.37f },
			// the quick wheel shown while playing (the owner, 2026-09-29: "change the location or position of the wheel menu in game
			// only not in the menu") - the HUD layout's own instance; the menu's quick-keys page is another class and is not touched
			{ .key = "QuickWheel", .english = "Quick wheel", .classes = { L"WBP_ModernMenu_QuickKeys_C" }, .names = { "WBP_ModernMenu_QuickKeys" }, .moveViaSlot = true },
			{ .key = "Crosshair", .english = "Crosshair", .classes = { L"WBP_ModernHud_Reticle_C" }, .names = { "WBP_ModernHud_Reticle", "Reticle" } },
			{ .key = "WeaponIcon", .english = "Weapon icon", .classes = { L"WBP_ModernHud_WeaponIcon_C" }, .names = { "WeaponIcon" } },
			{ .key = "MagicIcon", .english = "Spell icon", .classes = { L"WBP_ModernHud_MagicIcon_C" }, .names = { "MagicIcon" } },
			// the preview fills the list through the widget's own update functions (they spawn the list entries from the icons)
			{ .key = "EffectIcons", .english = "Active effects", .classes = { L"WBP_ModernHud_EffectIcons_C" }, .names = { "EffectIcons" },
				.previewOn = { { L"Update Active Effect Icons", { ArgObjects("InIcons", kPreviewEffectIcons) } }, { L"Update Active Effects Time Left", { ArgDoubles("InProgresses", { 0.85, 0.55, 0.3 }) } } },
				.previewOff = { { L"Update Active Effect Icons", { ArgObjects("InIcons", {}) } }, { L"Update Active Effects Time Left", { ArgDoubles("InProgresses", {}) } } } },
			// The enemy's health bar (NPC_Health) sits inside the top-stats block's AnimatableRetainerBox, which renders only
			// its own 370x63 area: the bar moved above it was clipped away (the owner, 2026-09-29: "it goes invisible if you
			// try to position it above where it's currently at"). So the element is the whole block (WBP_ModernTopStats: the
			// bar, the enemy's name, the boss frame), moved as one. The block's own HandleNPCVisibility raises the retainer's
			// material veil and keeps it up until FadeNPCOut; the inner bar's SetProgress fills it for the preview.
			{ .key = "EnemyHealth", .english = "Enemy health", .classes = { L"WBP_ModernTopStats_C" }, .names = { "WBP_ModernTopStats", "TopStats" }, .fades = true,
				.previewOn = { { L"HandleNPCVisibility", { ArgBool("InNewVisibility", true) } }, { L"SetProgress", { ArgDouble("InProgress", 0.75), ArgBool("IsPreview", true) }, nullptr, "NPC_Health" } },
				.previewOff = { { L"SetProgress", { ArgDouble("InProgress", 0.0), ArgBool("IsPreview", true) }, nullptr, "NPC_Health" }, { L"FadeNPCOut", {} } },
				.holdOn = { { L"HandleNPCVisibility", { ArgBool("InNewVisibility", true) } } }, .holdOff = { { L"FadeNPCOut", {} } }, .bar = true },
			{ .key = "SneakEye", .english = "Sneak eye", .classes = { L"WBP_ModernHud_SneakEye_C" }, .names = { "SneakEye" },
				.previewOn = { { L"UpdateSneakingVisibility", { ArgBool("InSneaking", true) } }, { L"Update Sneak Level", { ArgDouble("InSneakLevel", 0.5) } } }, .previewOff = { { L"UpdateSneakingVisibility", { ArgBool("InSneaking", false) } } } },
			{ .key = "LevelUp", .english = "Level-up gauge", .classes = { L"WBP_ModernHud_LevelUpGauge_C" }, .names = { "LevelUpGauge" },
				.previewOn = { { L"ToggleLevelUpIconVisibility", { ArgBool("Visible", true) } } }, .previewOff = { { L"ToggleLevelUpIconVisibility", { ArgBool("Visible", false) } } } },
			// the player's level as an instance of the game's own level-up gauge (its "Lvl [bar] 5" row, the skill text and the
			// level-up icon hidden), created by this mod (createClass); "Level" is its tab. A plain text block came out black with
			// no bar (the owner, 2026-09-29).
			{ .key = "Level", .english = "Level", .classes = { L"HPM_LevelGauge_C" }, .names = { "HPM_LevelGauge" }, .createClass = "WBP_ModernHud_LevelUpGauge_C" },
			{ .key = "Info", .english = "Target name and value", .classes = { L"WBP_ModernHud_Info_C" }, .names = { "WBP_ModernHud_Info", "Info" },
				.previewOn = { { L"ShowHide", { ArgBool("InShow", true) } }, { L"UpdateEmpty", { ArgBool("bIsEmpty", false) } }, { L"UpdateTargedItemName", { ArgText("InName", "HPM_PreviewItem", "Item name") } } },
				.previewOff = { { L"UpdateEmpty", { ArgBool("bIsEmpty", true) } }, { L"ShowHide", { ArgBool("InShow", false) } } } },
			{ .key = "Subtitles", .english = "Subtitles and notifications", .classes = { L"WBP_ModernHud_Subtitle_C" }, .names = { "WBP_ModernHud_Subtitle", "Subtitle" },
				.previewOn = { { L"UpdateSubtitle", { ArgText("InText", "HPM_PreviewSubtitle", "A subtitle appears here while someone speaks.") } }, { L"ShowHide", { ArgBool("InVisibility", true) } } },
				.previewOff = { { L"ShowHide", { ArgBool("InVisibility", false) } } } },
			{ .key = "Location", .english = "Location name", .classes = { L"WBP_ModernHud_Area_C" }, .names = { "WBP_ModernHud_Area", "Area" },
				.previewOn = { { L"DisplayArea", { ArgText("AreaName", "HPM_PreviewArea", "Area name") } }, { L"Update Visibility", { ArgBool("Visible", true), ArgBool("Area Discovered", true) } } },
				.previewOff = { { L"Update Visibility", { ArgBool("Visible", false), ArgBool("Area Discovered", false) } } } },
			{ .key = "DamageIndicators", .english = "Damage direction", .classes = { L"WBP_ModernHud_DamageIndicators_C" }, .names = { "DamageIndicators" },
				.previewOn = { { L"UpdateOverencumberedVisibility", { ArgBool("bIsOverencumbered", true) } }, { L"Update Weapon Damage Visibility", { ArgBool("InVisible", true), ArgDouble("InHealth", 0.3) } } },
				.previewOff = { { L"UpdateOverencumberedVisibility", { ArgBool("bIsOverencumbered", false) } }, { L"Update Weapon Damage Visibility", { ArgBool("InVisible", false), ArgDouble("InHealth", 1.0) } } } },
			{ .key = "Notifications", .english = "Pop-up notifications", .classes = { L"WBP_ModernPrefab_NotificationInHUD_C" }, .names = { "NotificationInHUD" }, .previewOn = { { L"Enable Notification", {} } } },
			// ClearDisplay after the closing animation: the animation alone left the message on screen (the owner, 2026-09-29)
			{ .key = "Tutorial", .english = "Tutorial messages", .classes = { L"WBP_ModernTutorialDisplay_C" }, .names = { "TutorialDisplay" },
				.previewOn = { { L"LaunchOpenningAnimation", {} } }, .previewOff = { { L"LaunchClosingAnimation", {} }, { L"ClearDisplay", {} } } },
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
