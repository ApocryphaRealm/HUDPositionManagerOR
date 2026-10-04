#pragma once

namespace page
{
	inline constexpr const char* kModName = "HUD Position Manager";
	void Register();

	// The move sliders' range on the element tab the page last drew, in percent of the screen - what Free placement
	// changes (DevBench hud.position op range, 2026-10-04)
	struct OpenRange
	{
		std::string element;
		double      minX = 0, maxX = 0, minY = 0, maxY = 0;
		bool        unlocked = false, valid = false;
	};
	OpenRange LastOpenRange();
}
