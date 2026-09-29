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
			ImGui::TextUnformatted(a_label);
			ImGui::PopID();
			return pressed;
		}

		void Hint(const char* a_text) { ImGui::TextDisabled("%s", a_text); }

		// every element's tab name, written out in full so translation-coverage.py reads each key from the source
		const char* ElementName(std::size_t a_i)
		{
			switch (a_i) {
			case 0: return TR("HPM_El_Health", "Health");
			case 1: return TR("HPM_El_Magicka", "Magicka");
			case 2: return TR("HPM_El_Fatigue", "Fatigue");
			case 3: return TR("HPM_El_Compass", "Compass");
			case 4: return TR("HPM_El_Crosshair", "Crosshair");
			case 5: return TR("HPM_El_WeaponIcon", "Weapon icon");
			case 6: return TR("HPM_El_MagicIcon", "Spell icon");
			case 7: return TR("HPM_El_EffectIcons", "Active effects");
			case 8: return TR("HPM_El_EnemyHealth", "Enemy health");
			case 9: return TR("HPM_El_SneakEye", "Sneak eye");
			case 10: return TR("HPM_El_LevelUp", "Level-up gauge");
			case 11: return TR("HPM_El_Info", "Target name and value");
			case 12: return TR("HPM_El_Subtitles", "Subtitles and notifications");
			case 13: return TR("HPM_El_Breath", "Breath meter");
			case 14: return TR("HPM_El_Location", "Location name");
			case 15: return TR("HPM_El_DamageIndicators", "Damage direction");
			case 16: return TR("HPM_El_Notifications", "Pop-up notifications");
			case 17: return TR("HPM_El_Tutorial", "Tutorial messages");
			default: return elements::All()[a_i].english;
			}
		}

		void ElementTab(std::size_t a_i, const settings::Values& a_v, const hud::ElementStatus& a_st, bool a_hud)
		{
			const auto& el = elements::All()[a_i];
			auto        e = a_v.elements[a_i];
			const std::string id = std::string("##") + el.key;
			if (!a_hud) {
				Hint(TR("HPM_NoHud", "The HUD has not been shown yet - load a game to see this element."));
			} else if (!a_st.found) {
				ImGui::TextWrapped("%s", TR("HPM_NotFound", "Not in your HUD right now: it may appear later, or the HUD you use may not have it. Its settings are kept."));
			} else {
				Hint(TR("HPM_Found", "In your HUD - changes show at once."));
			}
			bool changed = false;
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
			changed |= ImGui::SliderFloat((std::string(TR("HPM_MoveX", "Move left / right")) + id + "x").c_str(), &e.x, -settings::kMoveX, settings::kMoveX, "%.0f");
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
			changed |= ImGui::SliderFloat((std::string(TR("HPM_MoveY", "Move up / down")) + id + "y").c_str(), &e.y, -settings::kMoveY, settings::kMoveY, "%.0f");
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
			changed |= ImGui::SliderFloat((std::string(TR("HPM_Size", "Size")) + id + "s").c_str(), &e.scale, settings::kScaleMin, settings::kScaleMax, "%.2fx");
			changed |= Switch((std::string(TR("HPM_Hide", "Hide")) + id + "h").c_str(), &e.hide);
			if (el.fades) {
				changed |= Switch((std::string(TR("HPM_AlwaysOne", "Always visible")) + id + "a").c_str(), &e.alwaysVisible);
				Hint(TR("HPM_AlwaysOneHint", "the game fades or hides this on its own; on keeps it shown while you play"));
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
			if (el.barLink && a_v.linkBars && e.moveWith.empty()) {
				Hint(TR("HPM_LinkedHint", "moves with Health while \"Move the three bars together\" is on"));
			}
			if (changed) {
				settings::Update([&](settings::Values& s) { s.elements[a_i] = e; });
			}
			if (ImGui::Button((std::string(TR("HPM_ResetOne", "Reset this element")) + id + "r").c_str())) {
				settings::ResetElement(a_i);
				logger::info("page: {} reset", el.key);
			}
		}

		void Draw()
		{
			if (!AMF::UseFrameworkImGui()) {
				return;
			}
			strings::Refresh();
			auto v = settings::Snapshot();
			ImGui::TextWrapped("%s", TR("HPM_Intro", "Move, resize or hide each part of the HUD. Changes show in the HUD at once and are saved automatically."));
			if (Switch(TR("HPM_Enabled", "Apply my layout"), &v.enabled)) {
				settings::Update([&](settings::Values& s) { s.enabled = v.enabled; });
				logger::info("page: layout {}", v.enabled ? "on" : "off");
			}
			Hint(TR("HPM_EnabledHint", "off: every element back where the game puts it"));
			if (Switch(TR("HPM_LinkBars", "Move the three bars together"), &v.linkBars)) {
				settings::Update([&](settings::Values& s) { s.linkBars = v.linkBars; });
			}
			Hint(TR("HPM_LinkBarsHint", "Magicka and Fatigue move with Health"));

			ImGui::SeparatorText(TR("HPM_GroupVisibility", "HUD visibility"));
			if (Switch(TR("HPM_AlwaysAll", "Always visible"), &v.alwaysVisible)) {
				settings::Update([&](settings::Values& s) { s.alwaysVisible = v.alwaysVisible; });
				logger::info("page: HUD {}", v.alwaysVisible ? "always visible" : "as the game decides");
			}
			Hint(v.alwaysVisible ? TR("HPM_AlwaysAllOnHint", "the bars, compass, crosshair and icons stay shown while you play (menus, dialogue and loading still hide the HUD)")
			                     : TR("HPM_AlwaysAllOffHint", "off: the game decides - bars fade when full, parts hide as the game chooses"));

			ImGui::Spacing();
			const bool hudFound = hud::HudFound();
			const auto st = hud::Statuses();
			if (ImGui::BeginTabBar("HudElements", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton)) {
				for (std::size_t i = 0; i < elements::Count(); ++i) {
					if (ImGui::BeginTabItem((std::string(ElementName(i)) + "##tab" + elements::All()[i].key).c_str())) {
						ElementTab(i, v, st[i], hudFound);
						ImGui::EndTabItem();
					}
				}
				ImGui::EndTabBar();
			}
			ImGui::Spacing();
			ImGui::Separator();
			if (ImGui::Button(TR("HPM_ResetAll", "Reset every element"))) {
				settings::ResetAll();
				logger::info("page: every element reset");
			}
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
