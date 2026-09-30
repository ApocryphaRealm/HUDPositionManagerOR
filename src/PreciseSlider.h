#pragma once

// ============================================================================================================
// HUD Position Manager's precise sliders (rule 68, the owner 2026-09-29: "all of our sliders are precise sliders and
// they don't jump more than one numerical unit per D-pad nudge"). Each is an integer slider in the unit it shows
// (tenths of a percent, hundredths of a scale, whole units) with ImGui's fine-step modifier held for that one call,
// so a D-pad nudge is exactly one unit; the mouse drags as any slider.
// ============================================================================================================

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace precise
{
	// A percent slider that moves 0.1 % per D-pad tick (the owner, 2026-09-29: "make sure that the percentage ticks per
	// slider move is 0.1%, even if it's a slow movement to get across the whole way"): the value is held in tenths
	// of a percent on an integer slider, which ImGui's navigation steps one unit at a time, and the text is drawn
	// over it as a percentage with one decimal. The mouse drags it as any slider.
	inline bool PercentSlider(const char* a_label, float* a_value, double a_min, double a_max)
	{
		int tenths = static_cast<int>(std::lround(*a_value * 10.0f));
		const int lo = static_cast<int>(std::floor(a_min * 10.0)), hi = static_cast<int>(std::ceil(a_max * 10.0));
		tenths = std::clamp(tenths, lo, hi);
		// 0.1 % per tick: ImGui moves an integer slider one unit per D-pad press only under its fine-tweak modifier once
		// the range passes 100 units (1 % of the range otherwise). The modifier's key state is set for the duration of
		// this one call and put back at once - nothing is queued into the framework's input, so nothing can stay held
		// (a queued modifier stuck when the menu closed mid-adjustment and took the game's bumpers with it, 2026-09-29).
		ImGuiIO& io = ImGui::GetIO();
		auto& l1 = io.KeysData[ImGuiKey_GamepadL1 - ImGuiKey_KeysData_OFFSET];
		auto& ctrl = io.KeysData[ImGuiKey_ReservedForModCtrl - ImGuiKey_KeysData_OFFSET];
		const bool l1Was = l1.Down, ctrlWas = ctrl.Down;
		l1.Down = true;
		ctrl.Down = true;
		const bool changed = ImGui::SliderInt(a_label, &tenths, lo, std::max(lo, hi), "", ImGuiSliderFlags_NoInput);
		l1.Down = l1Was;
		ctrl.Down = ctrlWas;
		const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
		char text[32];
		std::snprintf(text, sizeof(text), "%.1f %%", tenths / 10.0);
		const ImVec2 sz = ImGui::CalcTextSize(text);
		ImGui::GetWindowDrawList()->AddText(ImVec2((mn.x + mx.x - sz.x) * 0.5f, (mn.y + mx.y - sz.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), text);
		if (changed) *a_value = tenths / 10.0f;
		return changed;
	}

	// The same for a scale in HUNDREDTHS ("1.00x"): with a float slider the three scales could not be brought back to
	// exactly 1.00 after fine adjustment (the owner, 2026-09-29).
	inline bool ScaleSlider(const char* a_label, float* a_value, float a_min, float a_max)
	{
		int hundredths = static_cast<int>(std::lround(*a_value * 100.0f));
		const int lo = static_cast<int>(std::lround(a_min * 100.0f)), hi = std::max(lo, static_cast<int>(std::lround(a_max * 100.0f)));
		hundredths = std::clamp(hundredths, lo, hi);
		ImGuiIO& io = ImGui::GetIO();
		auto& l1 = io.KeysData[ImGuiKey_GamepadL1 - ImGuiKey_KeysData_OFFSET];
		auto& ctrl = io.KeysData[ImGuiKey_ReservedForModCtrl - ImGuiKey_KeysData_OFFSET];
		const bool l1Was = l1.Down, ctrlWas = ctrl.Down;
		l1.Down = true;
		ctrl.Down = true;
		const bool changed = ImGui::SliderInt(a_label, &hundredths, lo, hi, "", ImGuiSliderFlags_NoInput);
		l1.Down = l1Was;
		ctrl.Down = ctrlWas;
		const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
		char text[32];
		std::snprintf(text, sizeof(text), "%.2fx", hundredths / 100.0);
		const ImVec2 sz = ImGui::CalcTextSize(text);
		ImGui::GetWindowDrawList()->AddText(ImVec2((mn.x + mx.x - sz.x) * 0.5f, (mn.y + mx.y - sz.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), text);
		if (changed) *a_value = hundredths / 100.0f;
		return changed;
	}

	// An integer slider that moves exactly one unit per D-pad nudge whatever its range (the owner's standard for every
	// slider, 2026-09-29): ImGui steps an integer slider by 1 % of its range under navigation once the range passes 100,
	// and by one unit under its fine-tweak modifier - set around this one call, as for the percent sliders.
	inline bool StepSlider(const char* a_label, int* a_value, int a_min, int a_max, const char* a_format = "%d")
	{
		ImGuiIO& io = ImGui::GetIO();
		auto& l1 = io.KeysData[ImGuiKey_GamepadL1 - ImGuiKey_KeysData_OFFSET];
		auto& ctrl = io.KeysData[ImGuiKey_ReservedForModCtrl - ImGuiKey_KeysData_OFFSET];
		const bool l1Was = l1.Down, ctrlWas = ctrl.Down;
		l1.Down = true;
		ctrl.Down = true;
		const bool changed = ImGui::SliderInt(a_label, a_value, a_min, a_max, a_format, ImGuiSliderFlags_NoInput);
		l1.Down = l1Was;
		ctrl.Down = ctrlWas;
		return changed;
	}

}
