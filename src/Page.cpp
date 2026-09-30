// The "HUD Position Manager" page on the Apocrypha Menu Framework - the Skyrim mod's page (one tab per element: move,
// size, hide, move with, reset) plus the HUD visibility switches the owner asked for. Drawn on AMF's render thread: it
// never touches a widget, it reads hud::Statuses() and changes settings through settings::Update().
#include "Page.h"

#include <imgui.h>

#include "AMF.h"
#include "Hud.h"
#include "Settings.h"
#include "Strings.h"

namespace page
{
	namespace
	{
		// An on/off switch (rule 32 - never a checkbox): the framework's own design, a red/green track and a white knob
		// (ApocryphaMenuFrameworkOR include/utils/ToggleSwitch.h), in fixed colours (AMF's theme leaves FrameBg clear).
		bool Switch(const char* a_label, bool* a_v)
		{
			ImGui::PushID(a_label);
			const float  h = ImGui::GetFrameHeight();
			const float  w = h * 2.0f;
			const float  r = h * 0.5f;
			const ImVec2 p = ImGui::GetCursorScreenPos();
			const bool   pressed = ImGui::InvisibleButton("##switch", ImVec2(w, h));
			if (pressed) *a_v = !*a_v;
			const bool  hot = ImGui::IsItemHovered() || ImGui::IsItemFocused();
			const ImU32 track = *a_v ? (hot ? IM_COL32(92, 191, 96, 255) : IM_COL32(76, 175, 80, 255))
			                         : (hot ? IM_COL32(207, 84, 84, 255) : IM_COL32(191, 68, 68, 255));
			auto* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), track, r);
			dl->AddCircleFilled(ImVec2(p.x + r + (*a_v ? w - h : 0.0f), p.y + r), r - 2.0f, IM_COL32(240, 240, 240, 255), 32);
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::AlignTextToFramePadding();
			// ImGui's convention: what follows "##" is the id, never shown (the tabs' switches printed it, 2026-09-29)
			const char* hash = std::strstr(a_label, "##");
			ImGui::TextUnformatted(a_label, hash ? hash : nullptr);
			ImGui::PopID();
			return pressed;
		}

		void Hint(const char* a_text) { ImGui::TextDisabled("%s", a_text); }

		// A percent slider that moves 0.1 % per D-pad tick (the owner, 2026-09-29: "make sure that the percentage ticks per
		// slider move is 0.1%, even if it's a slow movement to get across the whole way"): the value is held in tenths
		// of a percent on an integer slider, which ImGui's navigation steps one unit at a time, and the text is drawn
		// over it as a percentage with one decimal. The mouse drags it as any slider.
		bool PercentSlider(const char* a_label, float* a_value, double a_min, double a_max)
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
		bool ScaleSlider(const char* a_label, float* a_value, float a_min, float a_max)
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

		// a slider's range, frozen while it is held (per element; the page draws on one thread)
		struct HeldRange
		{
			bool   x = false, y = false;
			double minX = 0, maxX = 0, minY = 0, maxY = 0;
		};
		std::vector<HeldRange> g_held(elements::Count());
		HeldRange              g_heldGroup;

		// ---------------------------------------------------------------- the Presets tab
		std::vector<settings::PresetInfo> g_presets;
		ULONGLONG                         g_presetsAt = 0;   // listed at most once a second while the tab is open
		int                               g_presetIndex = 0;
		char                              g_presetName[64] = "My layout";
		std::string                       g_presetNotice;    // the last result, shown under the buttons
		std::string                       g_deleteArmed;     // the preset path a first press of Delete named
		int                               g_saveIndex = 0;   // 0 = a new preset (the name field), else g_presets[i - 1] updated in place

		void PresetsTab()
		{
			const ULONGLONG now = GetTickCount64();
			if (now - g_presetsAt >= 1000) {
				g_presetsAt = now;
				g_presets = settings::ListPresets();
			}
			if (g_presetIndex >= static_cast<int>(g_presets.size())) g_presetIndex = g_presets.empty() ? 0 : static_cast<int>(g_presets.size()) - 1;
			ImGui::TextWrapped("%s", TR("HPM_PresetsIntro", "A preset is a whole layout: every element's position, size and visibility. Load one made by someone else, or save your own to switch between."));
			Hint(settings::PresetsFolder().string().c_str());
			ImGui::Spacing();
			if (g_presets.empty()) {
				Hint(TR("HPM_PresetsNone", "No presets yet. Save your layout below, or put a preset file from another author in the folder above."));
			} else {
				std::vector<std::string> labels;
				for (const auto& p : g_presets) {
					labels.push_back(p.author.empty() ? p.name : std::format("{} ({})", p.name, p.author));
				}
				std::vector<const char*> ptrs;
				for (const auto& l : labels) ptrs.push_back(l.c_str());
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				ImGui::Combo((std::string(TR("HPM_Preset", "Preset")) + "##preset").c_str(), &g_presetIndex, ptrs.data(), static_cast<int>(ptrs.size()));
				const auto& cur = g_presets[static_cast<std::size_t>(g_presetIndex)];
				if (!cur.note.empty()) Hint(cur.note.c_str());
				if (ImGui::Button((std::string(TR("HPM_PresetLoad", "Load this preset")) + "##load").c_str())) {
					const bool ok = settings::LoadPreset(cur.path);
					g_presetNotice = ok ? std::format("{}: {}", TR("HPM_PresetLoaded", "Loaded"), cur.name) : std::format("{}: {}", TR("HPM_PresetFailed", "Could not read"), cur.name);
					logger::info("page: preset {} {}", cur.path.string(), ok ? "loaded" : "could not be read");
				}
				ImGui::SameLine();
				const bool armed = g_deleteArmed == cur.path.string();
				if (ImGui::Button((std::string(armed ? TR("HPM_PresetDeleteSure", "Press again to delete") : TR("HPM_PresetDelete", "Delete")) + "##delete").c_str())) {
					if (armed) {
						const bool ok = settings::DeletePreset(cur.path);
						g_presetNotice = ok ? std::format("{}: {}", TR("HPM_PresetDeleted", "Deleted"), cur.name) : std::format("{}: {}", TR("HPM_PresetDeleteFailed", "Could not delete"), cur.name);
						logger::info("page: preset {} {}", cur.path.string(), ok ? "deleted" : "could not be deleted");
						g_deleteArmed.clear();
						g_presetsAt = 0;
					} else {
						g_deleteArmed = cur.path.string();
					}
				}
			}
			ImGui::Spacing();
			ImGui::SeparatorText(TR("HPM_PresetSaveGroup", "Save my layout"));
			// where to: an existing preset, updated in place (no retyping its name - the owner, 2026-09-29), or a new one
			{
				std::vector<std::string> labels{ TR("HPM_PresetNew", "New preset") };
				for (const auto& p : g_presets) labels.push_back(p.name);
				std::vector<const char*> ptrs;
				for (const auto& l : labels) ptrs.push_back(l.c_str());
				if (g_saveIndex >= static_cast<int>(labels.size())) g_saveIndex = 0;
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				ImGui::Combo((std::string(TR("HPM_PresetSaveTo", "Save to")) + "##saveto").c_str(), &g_saveIndex, ptrs.data(), static_cast<int>(ptrs.size()));
			}
			if (g_saveIndex == 0) {
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				ImGui::InputText((std::string(TR("HPM_PresetName", "Name")) + "##name").c_str(), g_presetName, sizeof(g_presetName));
				Hint(TR("HPM_PresetNameHint", "With a controller, leave the name as it is: each save gets its own number."));
				if (ImGui::Button((std::string(TR("HPM_PresetSave", "Save as a preset")) + "##save").c_str())) {
					const auto saved = settings::SavePreset(g_presetName, "", "");
					g_presetNotice = saved.empty() ? std::string(TR("HPM_PresetSaveFailed", "The preset could not be written.")) : std::format("{}: {}", TR("HPM_PresetSaved", "Saved"), saved.stem().string());
					logger::info("page: preset {} {}", g_presetName, saved.empty() ? "could not be written" : "saved to " + saved.string());
					g_presetsAt = 0;
				}
			} else {
				const auto& target = g_presets[static_cast<std::size_t>(g_saveIndex - 1)];
				if (ImGui::Button((std::string(TR("HPM_PresetUpdate", "Update this preset with my layout")) + "##update").c_str())) {
					const bool ok = settings::UpdatePreset(target.path);
					g_presetNotice = ok ? std::format("{}: {}", TR("HPM_PresetSaved", "Saved"), target.name) : std::string(TR("HPM_PresetSaveFailed", "The preset could not be written."));
					logger::info("page: preset {} {}", target.path.string(), ok ? "updated" : "could not be written");
					g_presetsAt = 0;
				}
			}
			if (!g_presetNotice.empty()) {
				ImGui::TextWrapped("%s", g_presetNotice.c_str());
			}
		}

		// every element's tab name, written out in full so translation-coverage.py reads each key from the source
		const char* ElementName(std::size_t a_i)
		{
			const std::string k = a_i < elements::Count() ? elements::All()[a_i].key : "";
			if (k == "Health") return TR("HPM_El_Health", "Health");
			if (k == "Magicka") return TR("HPM_El_Magicka", "Magicka");
			if (k == "Fatigue") return TR("HPM_El_Fatigue", "Fatigue");
			if (k == "Compass") return TR("HPM_El_Compass", "Compass");
			if (k == "Crosshair") return TR("HPM_El_Crosshair", "Crosshair");
			if (k == "WeaponIcon") return TR("HPM_El_WeaponIcon", "Weapon icon");
			if (k == "MagicIcon") return TR("HPM_El_MagicIcon", "Spell icon");
			if (k == "EffectIcons") return TR("HPM_El_EffectIcons", "Active effects");
			if (k == "EnemyHealth") return TR("HPM_El_EnemyHealth", "Enemy health");
			if (k == "SneakEye") return TR("HPM_El_SneakEye", "Sneak eye");
			if (k == "LevelUp") return TR("HPM_El_LevelUp", "Level-up gauge");
			if (k == "Level") return TR("HPM_El_Level", "Level");
			if (k == "Info") return TR("HPM_El_Info", "Target name and value");
			if (k == "Subtitles") return TR("HPM_El_Subtitles", "Subtitles and notifications");
			if (k == "Breath") return TR("HPM_El_Breath", "Breath meter");
			if (k == "Location") return TR("HPM_El_Location", "Location name");
			if (k == "DamageIndicators") return TR("HPM_El_DamageIndicators", "Damage direction and warning icons");
			if (k == "Notifications") return TR("HPM_El_Notifications", "Pop-up notifications");
			if (k == "Tutorial") return TR("HPM_El_Tutorial", "Tutorial messages");
			if (k == "QuickWheel") return TR("HPM_El_QuickWheel", "Quick wheel");
			return a_i < elements::Count() ? elements::All()[a_i].english : "";
		}

		void ElementTab(std::size_t a_i, const settings::Values& a_v, const std::vector<hud::ElementStatus>& a_all, bool a_hud)
		{
			const hud::ElementStatus& a_st = a_all[a_i];
			const auto& el = elements::All()[a_i];
			auto        e = a_v.elements[a_i];
			const std::string id = std::string("##") + el.key;
			if (!a_hud) {
				Hint(TR("HPM_NoHud", "The HUD has not been shown yet - load a game to see this element."));
			} else if (!a_st.found) {
				ImGui::TextWrapped("%s", TR("HPM_NotFound", "Not in your HUD right now: it may appear later, or the HUD you use may not have it. Its settings are kept."));
			} else {
				Hint(TR("HPM_Found", "In your HUD. Changes show at once."));
			}
			bool changed = false;
			// the sliders' range is the screen: as far as the element can go before its edge leaves the viewport, from its
			// measured rectangle (hud::OffsetRange); the fixed range until it has been measured
			// the sliders are PERCENT of the screen; the measured range comes in layout units and is converted
			double minX = -settings::kMoveX, maxX = settings::kMoveX, minY = -settings::kMoveY, maxY = settings::kMoveY;
			float  maxScale = settings::kScaleMax;
			const double unitW = a_st.viewW > 0.0 ? a_st.viewW : 1920.0, unitH = a_st.viewH > 0.0 ? a_st.viewH : 1080.0;
			const auto [withX, withY] = hud::MoveWithOffset(a_v, a_i);
			if (hud::OffsetRange(a_all, a_i, a_v, withX / 100.0 * unitW, withY / 100.0 * unitH, e.x / 100.0 * unitW, e.y / 100.0 * unitH, minX, maxX, minY, maxY)) {
				minX = minX / unitW * 100.0;
				maxX = maxX / unitW * 100.0;
				minY = minY / unitH * 100.0;
				maxY = maxY / unitH * 100.0;
				if (a_st.vw > 0.0 && a_st.vh > 0.0 && e.scale > 0.0f) {
					const double unscaledW = a_st.vw / e.scale, unscaledH = a_st.vh / e.scale;
					maxScale = static_cast<float>(std::clamp(std::min(a_st.viewW / unscaledW, a_st.viewH / unscaledH), static_cast<double>(settings::kScaleMin), static_cast<double>(settings::kScaleMax)));
				}
			}
			// hard lock: a percentage of the screen can never leave [-100, 100], whatever a measurement says (2026-09-29)
			minX = std::clamp(minX, -100.0, 100.0);
			maxX = std::clamp(maxX, minX, 100.0);
			minY = std::clamp(minY, -100.0, 100.0);
			maxY = std::clamp(maxY, minY, 100.0);
			// while a slider is held (a mouse drag) its range is frozen: a range that moved with the twice-a-second
			// measurement made the mouse's position mean a different value each frame ("teleporting", 2026-09-29)
			auto& held = g_held[a_i];
			if (held.x) { minX = held.minX; maxX = held.maxX; } else { held.minX = minX; held.maxX = maxX; }
			if (held.y) { minY = held.minY; maxY = held.maxY; } else { held.minY = minY; held.maxY = maxY; }
			e.x = std::clamp(e.x, static_cast<float>(minX), static_cast<float>(maxX));
			e.y = std::clamp(e.y, static_cast<float>(minY), static_cast<float>(maxY));
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
			changed |= PercentSlider((std::string(TR("HPM_MoveX", "Move left / right")) + id + "x").c_str(), &e.x, minX, maxX);
			held.x = ImGui::IsItemActive();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
			changed |= PercentSlider((std::string(TR("HPM_MoveY", "Move up / down")) + id + "y").c_str(), &e.y, minY, maxY);
			held.y = ImGui::IsItemActive();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
			changed |= ScaleSlider((std::string(TR("HPM_Size", "Size")) + id + "s").c_str(), &e.scale, settings::kScaleMin, maxScale);
			if (el.bar) {   // a resource bar: its length and height on their own, on top of the size
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				changed |= ScaleSlider((std::string(TR("HPM_Length", "Length")) + id + "l").c_str(), &e.stretchX, settings::kScaleMin, settings::kScaleMax);
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				changed |= ScaleSlider((std::string(TR("HPM_Height", "Height")) + id + "t").c_str(), &e.stretchY, settings::kScaleMin, settings::kScaleMax);
				Hint(TR("HPM_StretchHint", "Length and Height stretch the bar on one side each, on top of Size."));
			}
			changed |= Switch((std::string(TR("HPM_Hide", "Hide")) + id + "h").c_str(), &e.hide);
			if (el.fades) {
				changed |= Switch((std::string(TR("HPM_AlwaysOne", "Always visible")) + id + "a").c_str(), &e.alwaysVisible);
				Hint(TR("HPM_AlwaysOneHint", "The game fades this out on its own. On: it stays shown while you play."));
			}
			// move with: nothing, or any other element
			std::vector<std::string> labels{ TR("HPM_MoveWithNone", "Nothing - on its own") };
			std::vector<std::string> keys{ "" };
			int                      current = 0;
			for (std::size_t j = 0; j < elements::Count(); ++j) {
				if (j == a_i) continue;
				if (e.moveWith == elements::All()[j].key) current = static_cast<int>(labels.size());
				labels.emplace_back(ElementName(j));
				keys.emplace_back(elements::All()[j].key);
			}
			std::vector<const char*> ptrs;
			for (const auto& l : labels) ptrs.push_back(l.c_str());
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
			if (ImGui::Combo((std::string(TR("HPM_MoveWith", "Move with")) + id + "f").c_str(), &current, ptrs.data(), static_cast<int>(ptrs.size()))) {
				e.moveWith = keys[static_cast<std::size_t>(current)];
				changed = true;
			}
			if (a_v.group.Has(el.key)) {
				Hint(TR("HPM_InGroupHint", "Also moves with the widgets on the Combined widgets tab."));
			}
			if (changed) {
				settings::Update([&](settings::Values& s) { s.elements[a_i] = e; });
			}
			if (ImGui::Button((std::string(TR("HPM_ResetOne", "Reset this element")) + id + "r").c_str())) {
				settings::ResetElement(a_i);
				logger::info("page: {} reset", el.key);
			}
		}

		// ---------------------------------------------------------------- the Combined widgets tab (2026-09-29)
		// Any set of widgets moves as one on the shared sliders, on top of each one's own position; and the resource
		// bars' "Fill from" and "Length follows the resource" (the owner: "integrate the fill from alignment settings
		// into the combined widget tab").
		void CombinedTab(const settings::Values& a_v, const std::vector<hud::ElementStatus>& a_all, bool a_hud)
		{
			if (a_v.preview) hud::PageDrawn();
			ImGui::TextWrapped("%s", TR("HPM_CombinedIntro", "Pick the widgets that should move as one. The sliders below move every picked widget together, on top of the position each has on its own tab."));
			ImGui::SeparatorText(TR("HPM_GroupMembers", "Widgets that move as one"));
			auto group = a_v.group;
			bool changed = false;
			for (std::size_t i = 0; i < elements::Count(); ++i) {
				const char* key = elements::All()[i].key;
				bool on = group.Has(key);
				if (Switch((std::string(ElementName(i)) + "##grp" + key).c_str(), &on)) {
					if (on) group.members.emplace_back(key);
					else std::erase(group.members, std::string(key));
					changed = true;
				}
			}
			ImGui::Spacing();
			if (group.members.size() < 2) {
				Hint(TR("HPM_GroupNone", "Pick two or more widgets above to move them together."));
			} else {
				// the sliders' range: what every member can still travel (its own slider's range, less its own value), in percent
				double minX = -settings::kMoveX, maxX = settings::kMoveX, minY = -settings::kMoveY, maxY = settings::kMoveY;
				if (a_hud) {
					for (std::size_t i = 0; i < elements::Count(); ++i) {
						if (!group.Has(elements::All()[i].key) || i >= a_all.size() || !a_all[i].found) continue;
						const auto& st = a_all[i];
						const auto& e = a_v.elements[i];
						const double unitW = st.viewW > 0.0 ? st.viewW : 1920.0, unitH = st.viewH > 0.0 ? st.viewH : 1080.0;
						const auto [withX, withY] = hud::MoveWithOffset(a_v, i);   // the group's offset is in it
						double lo, hi, tlo, thi;
						if (!hud::OffsetRange(a_all, i, a_v, withX / 100.0 * unitW, withY / 100.0 * unitH, e.x / 100.0 * unitW, e.y / 100.0 * unitH, lo, hi, tlo, thi)) continue;
						// the member's own slider may go [lo, hi]; the group may move by what is left on each side of its own value
						minX = std::max(minX, group.x + lo / unitW * 100.0 - e.x);
						maxX = std::min(maxX, group.x + hi / unitW * 100.0 - e.x);
						minY = std::max(minY, group.y + tlo / unitH * 100.0 - e.y);
						maxY = std::min(maxY, group.y + thi / unitH * 100.0 - e.y);
					}
				}
				minX = std::clamp(minX, -100.0, 100.0);
				maxX = std::clamp(maxX, minX, 100.0);
				minY = std::clamp(minY, -100.0, 100.0);
				maxY = std::clamp(maxY, minY, 100.0);
				auto& held = g_heldGroup;
				if (held.x) { minX = held.minX; maxX = held.maxX; } else { held.minX = minX; held.maxX = maxX; }
				if (held.y) { minY = held.minY; maxY = held.maxY; } else { held.minY = minY; held.maxY = maxY; }
				group.x = std::clamp(group.x, static_cast<float>(minX), static_cast<float>(maxX));
				group.y = std::clamp(group.y, static_cast<float>(minY), static_cast<float>(maxY));
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				changed |= PercentSlider((std::string(TR("HPM_MoveX", "Move left / right")) + "##groupx").c_str(), &group.x, minX, maxX);
				held.x = ImGui::IsItemActive();
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				changed |= PercentSlider((std::string(TR("HPM_MoveY", "Move up / down")) + "##groupy").c_str(), &group.y, minY, maxY);
				held.y = ImGui::IsItemActive();
				if (ImGui::Button((std::string(TR("HPM_GroupReset", "Reset the shared position")) + "##groupreset").c_str())) {
					group.x = 0.0f;
					group.y = 0.0f;
					changed = true;
				}
			}
			if (changed) {
				settings::Update([&](settings::Values& s) { s.group = group; });
			}

			ImGui::Spacing();
			ImGui::SeparatorText(TR("HPM_BarsSection", "Resource bars"));
			Hint(TR("HPM_FillHint", "Fill from: the side the filled part of a bar is anchored to - it drains away from that side. Centre drains toward both ends."));
			const char* fills[4]{ TR("HPM_FillGame", "The game's own"), TR("HPM_FillLeft", "Left"), TR("HPM_FillCentre", "Centre"), TR("HPM_FillRight", "Right") };
			for (std::size_t i = 0; i < elements::Count(); ++i) {
				const auto& el = elements::All()[i];
				if (!el.bar) continue;
				auto e = a_v.elements[i];
				bool ch = false;
				const std::string id = std::string("##bar") + el.key;
				ImGui::TextUnformatted(ElementName(i));
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				if (ImGui::Combo((std::string(TR("HPM_FillFrom", "Fill from")) + id + "f").c_str(), &e.fill, fills, 4)) ch = true;
				if (el.stat) {
					const char* grows[3]{ TR("HPM_GrowBoth", "Both sides"), TR("HPM_GrowRight", "The right"), TR("HPM_GrowLeft", "The left") };
					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
					if (ImGui::Combo((std::string(TR("HPM_GrowSide", "Grows toward")) + id + "g").c_str(), &e.grow, grows, 3)) ch = true;
					Hint(TR("HPM_GrowHint", "Which way the bar gets longer or shorter: from its centre, or anchored on one end so it grows toward the other."));
					ch |= Switch((std::string(TR("HPM_LinkLength", "Length follows the resource")) + id + "l").c_str(), &e.linkLength);
					if (e.linkLength) {
						Hint(TR("HPM_LinkLengthHint", "The bar grows with your maximum: it is 1.00x long at the points below, longer above them."));
						int points = static_cast<int>(std::lround(e.pointsPerLength));
						ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
						if (ImGui::SliderInt((std::string(TR("HPM_PointsPerLength", "Points per full length")) + id + "p").c_str(), &points, static_cast<int>(settings::kPointsMin), static_cast<int>(settings::kPointsMax), "%d", ImGuiSliderFlags_NoInput)) {
							e.pointsPerLength = static_cast<float>(points);
							ch = true;
						}
					}
				}
				if (ch) settings::Update([&](settings::Values& s) { s.elements[i] = e; });
				ImGui::Spacing();
			}
		}

		void Draw()
		{
			if (!AMF::UseFrameworkImGui()) {
				return;
			}
			strings::Refresh();
			if (!ImGui::BeginTabBar("HpmTop", ImGuiTabBarFlags_None)) {
				return;
			}
			if (ImGui::BeginTabItem((std::string(TR("HPM_TabPresets", "Presets")) + "##tabpresets").c_str())) {
				PresetsTab();
				ImGui::EndTabItem();
			}
			auto       v = settings::Snapshot();
			const bool hudFound = hud::HudFound();
			const auto st = hud::Statuses();
			if (!ImGui::BeginTabItem((std::string(TR("HPM_TabLayout", "Layout")) + "##tablayout").c_str())) {
				if (ImGui::BeginTabItem((std::string(TR("HPM_TabCombined", "Combined widgets")) + "##tabcombined").c_str())) {
					CombinedTab(v, st, hudFound);
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
				return;
			}
			if (v.preview) hud::PageDrawn();
			ImGui::TextWrapped("%s", TR("HPM_Intro", "Move, resize or hide each part of the HUD. Changes show in the HUD at once and are saved automatically."));
			if (Switch(TR("HPM_Enabled", "Apply my layout"), &v.enabled)) {
				settings::Update([&](settings::Values& s) { s.enabled = v.enabled; });
				logger::info("page: layout {}", v.enabled ? "on" : "off");
			}
			Hint(TR("HPM_EnabledHint", "Off: every element goes back to where the game puts it."));
			if (Switch(TR("HPM_Preview", "Show every element"), &v.preview)) {
				settings::Update([&](settings::Values& s) { s.preview = v.preview; });
				logger::info("page: preview {}", v.preview ? "on" : "off");
			}
			Hint(TR("HPM_PreviewHint", "On: elements that only appear during an event (an enemy's health, the sneak eye, the level-up icon, subtitles) are made to show, in the menu and out of it, until you turn this off - so you can place them and look at the result. Turn it off when your layout is done. Off: the HUD shows only what the game shows."));
			if (Switch(TR("HPM_Snap", "Snap art edges together"), &v.snapEdges)) {
				settings::Update([&](settings::Values& s) { s.snapEdges = v.snapEdges; });
				logger::info("page: snap art edges {}", v.snapEdges ? "on" : "off");
			}
			Hint(TR("HPM_SnapHint", "On: when an edge of a widget you move comes close to an edge of another widget you have placed, it pulls onto that line, so two widgets meet or align exactly even when a slider tick overshoots. Off: widgets sit exactly where the sliders put them."));
			if (v.snapEdges) {
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
				if (ImGui::SliderFloat((std::string(TR("HPM_SnapDistance", "Snap distance")) + "##snapdist").c_str(), &v.snapDistance, 1.0f, 30.0f, "%.0f")) {
					settings::Update([&](settings::Values& s) { s.snapDistance = v.snapDistance; });
				}
				Hint(TR("HPM_SnapDistanceHint", "How close an edge has to come before it snaps, in screen units (the screen is 1080 tall)."));
			}
			if (Switch(TR("HPM_NoOverlap", "HUD widget collision"), &v.noOverlap)) {
				settings::Update([&](settings::Values& s) { s.noOverlap = v.noOverlap; });
				logger::info("page: elements stop at each other's edges {}", v.noOverlap ? "on" : "off");
			}
			Hint(TR("HPM_NoOverlapHint", "On: a widget you move stops where its edge meets another widget you have placed, so the bars line up without decimal-point work. Widgets the game still places are never in the way. Off: widgets may overlap."));

			ImGui::SeparatorText(TR("HPM_GroupVisibility", "HUD visibility"));
			if (Switch(TR("HPM_AlwaysAll", "Always visible"), &v.alwaysVisible)) {
				settings::Update([&](settings::Values& s) { s.alwaysVisible = v.alwaysVisible; });
				logger::info("page: HUD {}", v.alwaysVisible ? "always visible" : "as the game decides");
			}
			Hint(v.alwaysVisible ? TR("HPM_AlwaysAllOnHint", "The bars stay shown while you play. Menus, dialogue and loading screens still hide the HUD.")
			                     : TR("HPM_AlwaysAllOffHint", "Off: the game decides. The bars fade out when they are full."));

			ImGui::Spacing();
			std::size_t current = elements::Count();
			if (ImGui::BeginTabBar("HudElements", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton)) {
				for (std::size_t i = 0; i < elements::Count(); ++i) {
					if (ImGui::BeginTabItem((std::string(ElementName(i)) + "##tab" + elements::All()[i].key).c_str())) {
						current = i;
						ElementTab(i, v, st, hudFound);
						ImGui::EndTabItem();
					}
				}
				ImGui::EndTabBar();
			}
			(void)current;
			ImGui::Spacing();
			ImGui::Separator();
			if (ImGui::Button(TR("HPM_ResetAll", "Reset every element"))) {
				settings::ResetAll();
				logger::info("page: every element reset");
			}
			ImGui::EndTabItem();
			if (ImGui::BeginTabItem((std::string(TR("HPM_TabCombined", "Combined widgets")) + "##tabcombined").c_str())) {
				CombinedTab(v, st, hudFound);
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}

	void Register()
	{
		if (!AMF::IsInstalled()) {
			logger::info("the Apocrypha Menu Framework is not installed - no settings page (the INI still applies)");
			return;
		}
		if (AMF::RegisterPage(kModName, "Settings", &Draw)) {
			logger::info("AMF {}: the {} settings page is registered", AMF::Version(), kModName);
		} else {
			logger::warn("AMF refused the {} page", kModName);
		}
	}
}
