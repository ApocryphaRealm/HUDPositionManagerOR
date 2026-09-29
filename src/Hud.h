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

#include "Settings.h"

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
		// the element's rectangle on screen in viewport pixels (as drawn, with this mod's offset and scale in it) and
		// the viewport's size - the page's slider bounds come from these (2026-09-29: nothing may leave the screen)
		bool        measured = false;
		double      vx = 0, vy = 0, vw = 0, vh = 0, viewW = 0, viewH = 0;
		double      baseVX = 0, baseVY = 0;   // the rectangle's top-left with NO offset applied (fixed at measurement)
		double      insetX = 0;               // the undrawn margin on each side (elements::Element::visibleW), in units
		// what the element actually DRAWS (its visible images, text and bars), for the collision (2026-09-29)
		bool        drawn = false;
		double      dvx = 0, dvy = 0, dvw = 0, dvh = 0, baseDX = 0, baseDY = 0;
	};

	// The offset range an element's OWN slider may take without the element leaving the screen, in layout units:
	// from the rectangle's anchor fixed at measurement plus what the element moves with (a_withX / a_withY, units -
	// hud::MoveWithOffset in percent times the viewport). Nothing in it changes while a slider moves, so a mouse drag
	// maps to the same value from frame to frame (2026-09-29). False when the element has not been measured.
	bool OffsetRange(const ElementStatus& a_st, double a_withX, double a_withY, double& a_minX, double& a_maxX, double& a_minY, double& a_maxY);

	// The same range narrowed by the neighbours' edges while [General] bNoOverlap is on (the owner, 2026-09-29: the
	// bars line up by stopping where their borders meet): a_all in elements::All() order, a_i the element, a_ownX /
	// a_ownY its slider's part of the offset in units (so the current position always stays inside the range).
	bool OffsetRange(const std::vector<ElementStatus>& a_all, std::size_t a_i, const settings::Values& a_s, double a_withX, double a_withY,
		double a_ownX, double a_ownY, double& a_minX, double& a_maxX, double& a_minY, double& a_maxY);

	// what the element inherits from the element(s) it moves with, in percent of the screen (its own offset excluded)
	std::pair<double, double> MoveWithOffset(const settings::Values& a_s, std::size_t a_i);

	bool                       HudFound();   // any thread
	std::vector<ElementStatus> Statuses();   // any thread, in elements::All() order
	json                       State();      // any thread: the tool's answer
}
