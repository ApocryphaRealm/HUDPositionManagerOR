#pragma once

// ============================================================================================================
// The live HUD (game thread): found from WBP_PrimaryGameLayout_C, the root of all the game's UI, by walking the UMG
// widget tree (a user widget's WidgetTree -> RootWidget, a panel's Slots -> Content) and matching each element by its
// widget class (elements.h). Kept by object-array slot (ue::Handle) and found again when the HUD is rebuilt (a load, a
// level change).
//
// Every frame each element found gets the player's layout as a RENDER TRANSFORM (SetRenderTranslation /
// SetRenderScale about its centre) - drawn on top of where the game's layout puts it, so the game's own moves still
// happen: the element's current transform is read back, and a value this mod did not write becomes the new base the
// offset rides on (the Skyrim mod's "follow the HUD instead of fighting it"). Hide sets it Hidden and puts back what
// the game had. "Always visible" (the elements the game fades on its own, in gameplay only) re-asserts full opacity
// and a visible state every frame. Everything goes through reflected UFunctions (ProcessEvent) - no hook.
// ============================================================================================================

namespace hud
{
	void Tick();

	struct ElementStatus
	{
		bool        found = false;
		std::string widget;          // the instance's name and class
		double      baseX = 0, baseY = 0, baseScale = 1;   // where the game puts it
		double      x = 0, y = 0, scale = 1;               // as it is now
		float       opacity = 1.0f;
		int         visibility = -1;                       // ESlateVisibility: 0 Visible, 1 Collapsed, 2 Hidden, 3/4 hit-test-invisible
		bool        forcedVisible = false;                 // "Always visible" is holding it up now
	};

	bool                       HudFound();   // any thread
	std::vector<ElementStatus> Statuses();   // any thread, in elements::All() order
	json                       State();      // any thread: the tool's answer
}
