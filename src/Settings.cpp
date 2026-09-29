#include "Settings.h"

#include <fstream>
#include <sstream>

namespace settings
{
	namespace
	{
		Values     g_values;
		std::mutex g_saveLock;
		std::mutex g_valuesLock;   // the page's thread writes, the game thread reads

		std::string_view Trim(std::string_view a_s)
		{
			while (!a_s.empty() && (a_s.front() == ' ' || a_s.front() == '\t')) a_s.remove_prefix(1);
			while (!a_s.empty() && (a_s.back() == ' ' || a_s.back() == '\t' || a_s.back() == '\r')) a_s.remove_suffix(1);
			return a_s;
		}

		std::string Lower(std::string a_s)
		{
			std::ranges::transform(a_s, a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_s;
		}

		std::filesystem::path ModuleFolder()
		{
			HMODULE self = nullptr;
			::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&ModuleFolder), &self);
			wchar_t buf[MAX_PATH]{};
			const DWORD n = self ? ::GetModuleFileNameW(self, buf, MAX_PATH) : 0;
			if (n == 0 || n >= MAX_PATH) {
				return std::filesystem::path(L"OBSE") / L"Plugins";   // relative to the game's working folder
			}
			return std::filesystem::path(buf).parent_path();
		}

		// every key this plugin owns, as "section.key" -> its current value as INI text
		std::vector<std::pair<std::string, std::string>> Rows(const Values& a_v)
		{
			const auto f = [](float a_f, int a_decimals) { return a_decimals == 2 ? std::format("{:.2f}", a_f) : std::format("{:.0f}", a_f); };
			std::vector<std::pair<std::string, std::string>> out{
				{ "General.bEnabled", a_v.enabled ? "1" : "0" },
				{ "General.bAlwaysVisible", a_v.alwaysVisible ? "1" : "0" },
				{ "General.bWidgetCollision", a_v.noOverlap ? "1" : "0" },
				{ "General.bSnapEdges", a_v.snapEdges ? "1" : "0" },
				{ "General.fSnapDistance", std::format("{:.0f}", a_v.snapDistance) },
				{ "General.bPreview", a_v.preview ? "1" : "0" },
			};
			const auto& all = elements::All();
			for (std::size_t i = 0; i < all.size() && i < a_v.elements.size(); ++i) {
				const std::string s = all[i].key;
				const auto&       e = a_v.elements[i];
				out.emplace_back(s + ".fX", std::format("{:.2f}", e.x));   // percent of the screen
				out.emplace_back(s + ".fY", std::format("{:.2f}", e.y));
				out.emplace_back(s + ".fScale", f(e.scale, 2));
				out.emplace_back(s + ".fLength", f(e.stretchX, 2));
				out.emplace_back(s + ".fHeight", f(e.stretchY, 2));
				out.emplace_back(s + ".bHide", e.hide ? "1" : "0");
				if (all[i].fades) {
					out.emplace_back(s + ".bAlwaysVisible", e.alwaysVisible ? "1" : "0");
				}
				out.emplace_back(s + ".sMoveWith", e.moveWith);
				if (all[i].bar) out.emplace_back(s + ".iFill", std::to_string(e.fill));
				if (all[i].stat) {
					out.emplace_back(s + ".bLinkLength", e.linkLength ? "1" : "0");
					out.emplace_back(s + ".fPointsPerLength", std::format("{:.0f}", e.pointsPerLength));
				}
			}
			std::string members;
			for (const auto& m : a_v.group.members) members += (members.empty() ? "" : ",") + m;
			out.emplace_back("Group.sMembers", members);
			out.emplace_back("Group.fX", std::format("{:.2f}", a_v.group.x));
			out.emplace_back("Group.fY", std::format("{:.2f}", a_v.group.y));
			out.emplace_back("Log.uLogLevel", std::to_string(a_v.logLevel));
			return out;
		}

		void Clamp(Values& a_v)
		{
			a_v.elements.resize(elements::Count());
			for (std::size_t i = 0; i < a_v.elements.size(); ++i) {
				auto& e = a_v.elements[i];
				e.x = std::clamp(e.x, -kMoveX, kMoveX);
				e.y = std::clamp(e.y, -kMoveY, kMoveY);
				e.scale = std::clamp(e.scale, kScaleMin, kScaleMax);
				e.stretchX = std::clamp(e.stretchX, kScaleMin, kScaleMax);
				e.stretchY = std::clamp(e.stretchY, kScaleMin, kScaleMax);
				if (!elements::All()[i].fades) {
					e.alwaysVisible = false;
				}
				if (!e.moveWith.empty() && (elements::IndexOf(e.moveWith) < 0 || e.moveWith == elements::All()[i].key)) {
					e.moveWith.clear();   // an unknown element, or itself
				}
				e.fill = elements::All()[i].bar ? std::clamp(e.fill, 0, 3) : 0;
				if (!elements::All()[i].stat) e.linkLength = false;
				e.pointsPerLength = std::clamp(e.pointsPerLength, kPointsMin, kPointsMax);
			}
			// the group: known elements, each once
			std::vector<std::string> members;
			for (const auto& m : a_v.group.members) {
				if (elements::IndexOf(m) >= 0 && std::ranges::find(members, m) == members.end()) members.push_back(m);
			}
			a_v.group.members = std::move(members);
			a_v.group.x = std::clamp(a_v.group.x, -kMoveX, kMoveX);
			a_v.group.y = std::clamp(a_v.group.y, -kMoveY, kMoveY);
			a_v.snapDistance = std::clamp(a_v.snapDistance, 0.0f, 40.0f);
			a_v.logLevel = std::clamp(a_v.logLevel, 0, 6);
		}
	}

	Values Snapshot()
	{
		std::scoped_lock l(g_valuesLock);
		return g_values;
	}

	void Update(const std::function<void(Values&)>& a_change)
	{
		{
			std::scoped_lock l(g_valuesLock);
			a_change(g_values);
			Clamp(g_values);
		}
		Save();
	}

	void ResetElement(std::size_t a_index)
	{
		{
			std::scoped_lock l(g_valuesLock);
			if (a_index < g_values.elements.size()) {
				g_values.elements[a_index] = Element{};
			}
		}
		Save();
	}

	void ResetAll()
	{
		{
			std::scoped_lock l(g_valuesLock);
			g_values.elements.assign(elements::Count(), Element{});
		}
		Save();
	}

	std::filesystem::path PluginFolder() { return ModuleFolder(); }
	std::filesystem::path PresetsFolder() { return ModuleFolder() / L"HUDPositionManager" / L"presets"; }
	std::filesystem::path IniPath() { return ModuleFolder() / L"HUDPositionManager.ini"; }

	namespace
	{
		using Entries = std::unordered_map<std::string, std::string>;   // "section.key" (lower case) -> value

		bool ReadIni(const std::filesystem::path& a_path, Entries& a_out)
		{
			std::ifstream file(a_path);
			if (!file.is_open()) return false;
			std::string line, section;
			while (std::getline(file, line)) {
				const auto t = Trim(line);
				if (t.empty() || t.front() == ';' || t.front() == '#') continue;
				if (t.front() == '[' && t.back() == ']') {
					section = Lower(std::string(Trim(t.substr(1, t.size() - 2))));
					continue;
				}
				const auto eq = t.find('=');
				if (eq == std::string_view::npos) continue;
				a_out[section + "." + Lower(std::string(Trim(t.substr(0, eq))))] = std::string(Trim(t.substr(eq + 1)));
			}
			return true;
		}

		const std::string* Get(const Entries& a_e, const std::string& a_key)
		{
			const auto it = a_e.find(Lower(a_key));
			return it != a_e.end() ? &it->second : nullptr;
		}

		bool Flag(const std::string& a_v) { return a_v != "0" && Lower(a_v) != "false"; }

		// the layout keys (the elements, the group, HUD always visible) of a settings or preset file into a_v;
		// how many elements differ from the game's own layout
		int ReadLayout(const Entries& a_e, Values& a_v)
		{
			if (const auto* s = Get(a_e, "General.bAlwaysVisible")) a_v.alwaysVisible = Flag(*s);
			if (const auto* s = Get(a_e, "General.bWidgetCollision")) a_v.noOverlap = Flag(*s);
			if (const auto* s = Get(a_e, "General.bSnapEdges")) a_v.snapEdges = Flag(*s);
			if (const auto* s = Get(a_e, "General.fSnapDistance")) a_v.snapDistance = static_cast<float>(std::atof(s->c_str()));
			if (const auto* s = Get(a_e, "General.bPreview")) a_v.preview = Flag(*s);
			const auto& all = elements::All();
			a_v.elements.resize(all.size());
			int moved = 0;
			for (std::size_t i = 0; i < all.size(); ++i) {
				const std::string k = all[i].key;
				auto&             e = a_v.elements[i];
				if (const auto* s = Get(a_e, k + ".fX")) e.x = static_cast<float>(std::atof(s->c_str()));
				if (const auto* s = Get(a_e, k + ".fY")) e.y = static_cast<float>(std::atof(s->c_str()));
				if (const auto* s = Get(a_e, k + ".fScale")) e.scale = static_cast<float>(std::atof(s->c_str()));
				if (const auto* s = Get(a_e, k + ".fLength")) e.stretchX = static_cast<float>(std::atof(s->c_str()));
				if (const auto* s = Get(a_e, k + ".fHeight")) e.stretchY = static_cast<float>(std::atof(s->c_str()));
				if (const auto* s = Get(a_e, k + ".bHide")) e.hide = Flag(*s);
				if (const auto* s = Get(a_e, k + ".bAlwaysVisible")) e.alwaysVisible = Flag(*s);
				if (const auto* s = Get(a_e, k + ".sMoveWith")) e.moveWith = *s;
				if (const auto* s = Get(a_e, k + ".iFill")) e.fill = std::atoi(s->c_str());
				if (const auto* s = Get(a_e, k + ".bLinkLength")) e.linkLength = Flag(*s);
				if (const auto* s = Get(a_e, k + ".fPointsPerLength")) e.pointsPerLength = static_cast<float>(std::atof(s->c_str()));
				moved += (e.x != 0.0f || e.y != 0.0f || e.scale != 1.0f || e.stretchX != 1.0f || e.stretchY != 1.0f || e.hide || e.fill != 0) ? 1 : 0;
			}
			if (const auto* s = Get(a_e, "Group.sMembers")) {
				a_v.group.members.clear();
				for (std::size_t p = 0; p <= s->size();) {
					const auto c = s->find(',', p);
					const auto part = std::string(Trim(std::string_view(*s).substr(p, (c == std::string::npos ? s->size() : c) - p)));
					if (!part.empty()) a_v.group.members.push_back(part);
					if (c == std::string::npos) break;
					p = c + 1;
				}
			}
			if (const auto* s = Get(a_e, "Group.fX")) a_v.group.x = static_cast<float>(std::atof(s->c_str()));
			if (const auto* s = Get(a_e, "Group.fY")) a_v.group.y = static_cast<float>(std::atof(s->c_str()));
			return moved;
		}
	}

	void Load()
	{
		Entries entries;
		if (!ReadIni(IniPath(), entries)) {
			logger::info("settings: {} not found - the compiled defaults are in effect (they match the shipped INI)", IniPath().string());
			return;
		}
		std::scoped_lock l(g_valuesLock);
		auto& v = g_values;
		if (const auto* s = Get(entries, "General.bEnabled")) v.enabled = Flag(*s);
		const int moved = ReadLayout(entries, v);
		if (const auto* s = Get(entries, "Log.uLogLevel")) v.logLevel = std::atoi(s->c_str());
		Clamp(v);
		logger::info("settings loaded from {}: layout {}, {} widget(s) moving as one, HUD {}, {} element(s) changed, log level {}", IniPath().string(),
			v.enabled ? "on" : "off", v.group.members.size(), v.alwaysVisible ? "always visible" : "as the game decides", moved, v.logLevel);
	}

	bool Save()
	{
		std::scoped_lock l(g_saveLock);
		const auto path = IniPath();
		std::vector<std::string> lines;
		bool crlf = true;
		{
			std::ifstream in(path, std::ios::binary);
			if (in.is_open()) {
				std::stringstream ss;
				ss << in.rdbuf();
				const std::string all = ss.str();
				crlf = all.find("\r\n") != std::string::npos || all.empty();
				std::string cur;
				for (char c : all) {
					if (c == '\n') {
						if (!cur.empty() && cur.back() == '\r') cur.pop_back();
						lines.push_back(cur);
						cur.clear();
					} else {
						cur.push_back(c);
					}
				}
				if (!cur.empty()) lines.push_back(cur);
			}
		}
		Values snapshot;
		{
			std::scoped_lock v(g_valuesLock);
			snapshot = g_values;
		}
		const auto rows = Rows(snapshot);
		std::vector<bool> written(rows.size(), false);
		std::string section;
		for (auto& ln : lines) {
			const auto t = Trim(ln);
			if (!t.empty() && t.front() == '[' && t.back() == ']') {
				section = Lower(std::string(Trim(t.substr(1, t.size() - 2))));
				continue;
			}
			if (t.empty() || t.front() == ';' || t.front() == '#') continue;
			const auto eq = t.find('=');
			if (eq == std::string_view::npos) continue;
			const std::string key = section + "." + Lower(std::string(Trim(t.substr(0, eq))));
			for (std::size_t i = 0; i < rows.size(); ++i) {
				if (Lower(rows[i].first) == key) {
					ln = rows[i].first.substr(rows[i].first.find('.') + 1) + "=" + rows[i].second;
					written[i] = true;
					break;
				}
			}
		}
		// keys the file did not have yet go at the end of their section (a new section at the end if needed)
		for (std::size_t i = 0; i < rows.size(); ++i) {
			if (written[i]) continue;
			const auto dot = rows[i].first.find('.');
			const std::string want = Lower(rows[i].first.substr(0, dot));
			const std::string text = rows[i].first.substr(dot + 1) + "=" + rows[i].second;
			std::size_t insertAt = lines.size();
			bool found = false;
			for (std::size_t j = 0; j < lines.size(); ++j) {
				const auto t = Trim(lines[j]);
				if (!t.empty() && t.front() == '[' && t.back() == ']') {
					if (found) {
						insertAt = j;
						break;
					}
					found = Lower(std::string(Trim(t.substr(1, t.size() - 2)))) == want;
				}
			}
			if (!found) {
				lines.push_back("");
				lines.push_back("[" + rows[i].first.substr(0, dot) + "]");
				insertAt = lines.size();
			} else {
				while (insertAt > 0 && Trim(lines[insertAt - 1]).empty()) --insertAt;
			}
			lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insertAt), text);
		}
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		if (!out.is_open()) {
			logger::error("settings: {} could not be written", path.string());
			return false;
		}
		for (const auto& ln : lines) {
			out << ln << (crlf ? "\r\n" : "\n");
		}
		logger::debug("settings: saved to {}", path.string());
		return true;
	}

	std::vector<PresetInfo> ListPresets()
	{
		std::vector<PresetInfo> out;
		std::error_code         ec;
		for (const auto& entry : std::filesystem::directory_iterator(PresetsFolder(), ec)) {
			if (!entry.is_regular_file(ec) || Lower(entry.path().extension().string()) != ".ini") continue;
			PresetInfo p;
			p.path = entry.path();
			p.name = entry.path().stem().string();
			Entries e;
			if (ReadIni(p.path, e)) {
				if (const auto* s = Get(e, "Preset.sName"); s && !s->empty()) p.name = *s;
				if (const auto* s = Get(e, "Preset.sAuthor")) p.author = *s;
				if (const auto* s = Get(e, "Preset.sNote")) p.note = *s;
			}
			out.push_back(std::move(p));
		}
		std::ranges::sort(out, [](const PresetInfo& a, const PresetInfo& b) { return Lower(a.name) < Lower(b.name); });
		return out;
	}

	bool LoadPreset(const std::filesystem::path& a_path)
	{
		Entries e;
		if (!ReadIni(a_path, e)) {
			logger::warn("preset: {} could not be read", a_path.string());
			return false;
		}
		int moved = 0;
		{
			std::scoped_lock l(g_valuesLock);
			g_values.elements.assign(elements::Count(), Element{});   // a preset is the whole layout: what it leaves out is the game's own
			g_values.group = Group{};
			moved = ReadLayout(e, g_values);
			Clamp(g_values);
		}
		Save();
		logger::info("preset: {} loaded - {} element(s) changed from the game's layout", a_path.string(), moved);
		return true;
	}

	namespace
	{
		bool WritePreset(const std::filesystem::path& a_path, const std::string& a_name, const std::string& a_author, const std::string& a_note)
		{
			Values snapshot = Snapshot();
			std::ofstream out(a_path, std::ios::binary | std::ios::trunc);
			if (!out.is_open()) {
				logger::error("preset: {} could not be written", a_path.string());
				return false;
			}
			out << "; HUD Position Manager preset - a whole layout. Load it from the Presets tab; every element the file\r\n"
			    << "; leaves out goes back to the game's own layout. fX / fY are a percentage of the screen.\r\n"
			    << "[Preset]\r\nsName=" << a_name << "\r\nsAuthor=" << a_author << "\r\nsNote=" << a_note << "\r\n\r\n"
			    << "[General]\r\nbAlwaysVisible=" << (snapshot.alwaysVisible ? 1 : 0) << "\r\nbWidgetCollision=" << (snapshot.noOverlap ? 1 : 0) << "\r\nbSnapEdges=" << (snapshot.snapEdges ? 1 : 0) << "\r\nfSnapDistance=" << std::format("{:.0f}", snapshot.snapDistance) << "\r\n";
			std::string section;
			for (const auto& [key, value] : Rows(snapshot)) {
				const auto        dot = key.find('.');
				const std::string sec = key.substr(0, dot);
				if (sec == "General" || sec == "Log") continue;
				if (sec != section) {
					out << "\r\n[" << sec << "]\r\n";
					section = sec;
				}
				out << key.substr(dot + 1) << "=" << value << "\r\n";
			}
			return true;
		}
	}

	bool UpdatePreset(const std::filesystem::path& a_path)
	{
		Entries e;
		if (!ReadIni(a_path, e)) {
			logger::warn("preset: {} could not be read - not updated", a_path.string());
			return false;
		}
		const auto* name = Get(e, "Preset.sName");
		const auto* author = Get(e, "Preset.sAuthor");
		const auto* note = Get(e, "Preset.sNote");
		const bool ok = WritePreset(a_path, name && !name->empty() ? *name : a_path.stem().string(), author ? *author : "", note ? *note : "");
		if (ok) logger::info("preset: {} updated with the current layout", a_path.string());
		return ok;
	}

	std::filesystem::path SavePreset(std::string a_name, const std::string& a_author, const std::string& a_note)
	{
		// a file name from the name: what Windows refuses is dropped; a taken name gets the next free number
		std::string stem;
		for (const char c : a_name) {
			if (std::strchr("\\/:*?\"<>|", c) == nullptr && static_cast<unsigned char>(c) >= 0x20) stem.push_back(c);
		}
		while (!stem.empty() && (stem.back() == ' ' || stem.back() == '.')) stem.pop_back();
		while (!stem.empty() && stem.front() == ' ') stem.erase(stem.begin());
		if (stem.empty()) stem = "My layout";
		std::error_code ec;
		std::filesystem::create_directories(PresetsFolder(), ec);
		std::filesystem::path path = PresetsFolder() / (stem + ".ini");
		for (int n = 2; std::filesystem::exists(path, ec) && n < 1000; ++n) {
			path = PresetsFolder() / std::format("{} {}.ini", stem, n);
		}
		if (!WritePreset(path, path.stem().string(), a_author, a_note)) {
			return {};
		}
		logger::info("preset: the current layout saved to {}", path.string());
		return path;
	}

	bool DeletePreset(const std::filesystem::path& a_path)
	{
		std::error_code ec;
		// only a file inside the presets folder
		const auto folder = std::filesystem::weakly_canonical(PresetsFolder(), ec);
		const auto file = std::filesystem::weakly_canonical(a_path, ec);
		if (file.parent_path() != folder) {
			logger::warn("preset: {} is not in the presets folder - not deleted", a_path.string());
			return false;
		}
		return std::filesystem::remove(file, ec) && !ec;
	}
}
