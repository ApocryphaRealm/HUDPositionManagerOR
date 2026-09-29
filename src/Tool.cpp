// hud.position (rules 31 and 64): op state (default) - the switches, and per element: its settings, found, the widget,
// the game's base transform and the applied one, opacity, visibility, whether "Always visible" is holding it up;
// op set {element?, key, value} - change a setting as the page would (element keys: x, y in PERCENT of the screen, scale, hide, alwaysVisible,
// moveWith, fill (0 game's own, 1 left, 2 centre, 3 right), linkLength, pointsPerLength; without an element: enabled, alwaysVisible,
// groupMembers (comma-separated keys), groupX, groupY); op reset {element?} - one element or every element.
// Every accessor used here is thread-safe, so the handler answers on TestBench's own thread.
#include "Hud.h"
#include "Settings.h"
#include "TestBenchAPI.h"

namespace tool
{
	namespace
	{
		TestBenchAPI::ITestBenchInterface001* g_tb = nullptr;

		void Write(void* a_sink, TestBenchAPI::WriteFn a_write, const json& a_j) { a_write(a_sink, a_j.dump().c_str()); }

		json SettingsJson()
		{
			const auto v = settings::Snapshot();
			json       els = json::object();
			for (std::size_t i = 0; i < elements::Count(); ++i) {
				const auto& e = v.elements[i];
				els[elements::All()[i].key] = { { "x", e.x }, { "y", e.y }, { "scale", e.scale }, { "length", e.stretchX }, { "height", e.stretchY }, { "hide", e.hide }, { "alwaysVisible", e.alwaysVisible },
					{ "moveWith", e.moveWith }, { "fill", e.fill }, { "linkLength", e.linkLength }, { "pointsPerLength", e.pointsPerLength } };
			}
			std::string members;
			for (const auto& m : v.group.members) members += (members.empty() ? "" : ",") + m;
			return { { "enabled", v.enabled }, { "groupMembers", members }, { "groupX", v.group.x }, { "groupY", v.group.y }, { "alwaysVisible", v.alwaysVisible }, { "widgetCollision", v.noOverlap }, { "snapEdges", v.snapEdges }, { "snapDistance", v.snapDistance }, { "preview", v.preview }, { "elements", els } };
		}

		std::string Set(const json& a_args)
		{
			const std::string el = a_args.value("element", std::string());
			const std::string key = a_args.value("key", std::string());
			if (!a_args.contains("value")) {
				return "value is missing";
			}
			const json& val = a_args["value"];
			if (el.empty()) {
				if ((key == "enabled" || key == "alwaysVisible" || key == "widgetCollision" || key == "snapEdges" || key == "preview") && val.is_boolean()) {
					const bool b = val.get<bool>();
					settings::Update([&](settings::Values& s) { (key == "enabled" ? s.enabled : key == "widgetCollision" ? s.noOverlap : key == "snapEdges" ? s.snapEdges : key == "preview" ? s.preview : s.alwaysVisible) = b; });
					return {};
				}
				if (key == "groupMembers" && val.is_string()) {
					settings::Update([&](settings::Values& s) {
						s.group.members.clear();
						std::string part;
						for (const char c : val.get<std::string>() + ",") {
							if (c == ',') { if (!part.empty()) s.group.members.push_back(part); part.clear(); }
							else if (c != ' ') part.push_back(c);
						}
					});
					return {};
				}
				if ((key == "groupX" || key == "groupY") && val.is_number()) {
					settings::Update([&](settings::Values& s) { (key == "groupX" ? s.group.x : s.group.y) = val.get<float>(); });
					return {};
				}
				if (key == "snapDistance" && val.is_number()) {
					settings::Update([&](settings::Values& s) { s.snapDistance = val.get<float>(); });
					return {};
				}
				return "without an element: enabled, alwaysVisible, widgetCollision, snapEdges, preview (bool), snapDistance, groupX, groupY (number), groupMembers (comma-separated keys)";
			}
			const int i = elements::IndexOf(el);
			if (i < 0) {
				return "no element " + el;
			}
			bool ok = true;
			settings::Update([&](settings::Values& s) {
				auto& e = s.elements[static_cast<std::size_t>(i)];
				if ((key == "x" || key == "y" || key == "scale" || key == "length" || key == "height" || key == "pointsPerLength") && val.is_number()) {
					(key == "x" ? e.x : key == "y" ? e.y : key == "scale" ? e.scale : key == "length" ? e.stretchX : key == "height" ? e.stretchY : e.pointsPerLength) = val.get<float>();
				} else if (key == "fill" && val.is_number()) {
					e.fill = val.get<int>();
				} else if ((key == "hide" || key == "alwaysVisible" || key == "linkLength") && val.is_boolean()) {
					(key == "hide" ? e.hide : key == "linkLength" ? e.linkLength : e.alwaysVisible) = val.get<bool>();
				} else if (key == "moveWith" && val.is_string()) {
					e.moveWith = val.get<std::string>();
				} else {
					ok = false;
				}
			});
			return ok ? std::string() : "element keys: x, y, scale, length, height, pointsPerLength, fill (number), hide, alwaysVisible, linkLength (bool), moveWith (element key or \"\")";
		}

		void Tool(void*, const char* a_args, void* a_sink, TestBenchAPI::WriteFn a_write)
		{
			json args = json::parse(a_args ? a_args : "{}", nullptr, false);
			if (args.is_discarded() || !args.is_object()) args = json::object();
			const std::string op = args.value("op", "state");
			if (op == "set") {
				const auto why = Set(args);
				Write(a_sink, a_write, why.empty() ? json{ { "ok", true }, { "settings", SettingsJson() } } : json{ { "ok", false }, { "error", why } });
				return;
			}
			if (op == "reset") {
				const std::string el = args.value("element", std::string());
				if (el.empty()) {
					settings::ResetAll();
				} else if (const int i = elements::IndexOf(el); i >= 0) {
					settings::ResetElement(static_cast<std::size_t>(i));
				} else {
					Write(a_sink, a_write, { { "ok", false }, { "error", "no element " + el } });
					return;
				}
				Write(a_sink, a_write, { { "ok", true }, { "settings", SettingsJson() } });
				return;
			}
			if (op == "presets") {
				json list = json::array();
				for (const auto& p : settings::ListPresets()) list.push_back({ { "path", p.path.string() }, { "name", p.name }, { "author", p.author }, { "note", p.note } });
				Write(a_sink, a_write, { { "ok", true }, { "folder", settings::PresetsFolder().string() }, { "presets", list } });
				return;
			}
			if (op == "savePreset") {
				const auto path = settings::SavePreset(args.value("name", std::string("My layout")), args.value("author", std::string()), args.value("note", std::string()));
				Write(a_sink, a_write, path.empty() ? json{ { "ok", false }, { "error", "could not write the preset" } } : json{ { "ok", true }, { "path", path.string() } });
				return;
			}
			if (op == "loadPreset") {
				const std::string p = args.value("path", std::string());
				const bool        ok = !p.empty() && settings::LoadPreset(p);
				Write(a_sink, a_write, ok ? json{ { "ok", true }, { "settings", SettingsJson() } } : json{ { "ok", false }, { "error", "loadPreset {path}: could not read it" } });
				return;
			}
			if (op != "state") {
				Write(a_sink, a_write, { { "ok", false }, { "error", "op: state | set {element?, key, value} | reset {element?} | presets | savePreset {name, author?, note?} | loadPreset {path}" } });
				return;
			}
			Write(a_sink, a_write, { { "ok", true }, { "version", HPM_VERSION }, { "settings", SettingsJson() }, { "hud", hud::State() } });
		}
	}

	bool Register()
	{
		if (g_tb) return true;
		HMODULE tb = ::GetModuleHandleW(L"TestBench.dll");
		auto get = tb ? reinterpret_cast<void* (*)(unsigned)>(::GetProcAddress(tb, "TestBench_GetInterface")) : nullptr;
		g_tb = get ? static_cast<TestBenchAPI::ITestBenchInterface001*>(get(1)) : nullptr;
		if (!g_tb) return false;
		g_tb->RegisterTool("hud.position",
			R"({"description":"HUD Position Manager: op state (default) - switches, and per element settings / found / widget / base and applied transform / opacity / visibility / forced visible; op set {element?, key, value} - element keys x, y, scale, length, height, fill, linkLength, pointsPerLength, hide, alwaysVisible, moveWith; without element: enabled, alwaysVisible, groupMembers, groupX, groupY; op reset {element?}; op presets - the preset files; op savePreset {name, author?, note?} - the current layout as a preset; op loadPreset {path}","inputSchema":{"type":"object","properties":{"op":{"type":"string"},"element":{"type":"string"},"key":{"type":"string"},"value":{}}}})",
			&Tool, nullptr);
		logger::info("TestBench tool registered: hud.position");
		return true;
	}
}
