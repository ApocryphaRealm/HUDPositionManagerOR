// HUD Position Manager for Oblivion Remastered - move, resize and hide each part of the game's own HUD from a page on the
// Apocrypha Menu Framework, and keep the HUD visible at all times or leave its showing and hiding to the game. The port
// of our Skyrim HUD Position Manager (same end result, on the Remastered's UMG HUD). Plan:
// 4. plans\hud-position-manager-oblivion\PLAN.md.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Hud.h"
#include "Page.h"
#include "Settings.h"
#include "Tick.h"

namespace tool { bool Register(); }

namespace
{
	// every frame (from the message pump, on the game thread)
	void OnFrame()
	{
		static bool          toolRegistered = false;
		static std::uint64_t n = 0;
		if (!toolRegistered && ++n % 60 == 0) {
			toolRegistered = tool::Register();
		}
		hud::Tick();
	}

	void OnMessage(OBSE::MessagingInterface::Message* a_msg)
	{
		if (a_msg && a_msg->type == OBSE::MessagingInterface::kPostLoad) {
			tick::Install(&OnFrame);
			page::Register();
		}
	}

	// the previous launch's log, kept as HUDPositionManager.prev.log before OBSE::Init truncates it (a quick relaunch
	// overwrote the log of the round that mattered twice on 2026-09-29, in Better Third-Person Selection)
	void KeepPreviousLog()
	{
		PWSTR docs = nullptr;
		if (FAILED(::SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs)) || !docs) {
			return;
		}
		const std::filesystem::path dir = std::filesystem::path(docs) / L"My Games" / L"Oblivion Remastered" / L"OBSE" / L"Logs";
		::CoTaskMemFree(docs);
		std::error_code ec;
		if (std::filesystem::exists(dir / L"HUDPositionManager.log", ec)) {
			std::filesystem::copy_file(dir / L"HUDPositionManager.log", dir / L"HUDPositionManager.prev.log",
				std::filesystem::copy_options::overwrite_existing, ec);
		}
	}
}

OBSE_PLUGIN_LOAD(const OBSE::LoadInterface* a_obse)
{
	KeepPreviousLog();
	OBSE::Init(a_obse);
	settings::Load();
	const auto v = settings::Snapshot();
	const auto level = static_cast<spdlog::level::level_enum>(std::clamp(v.logLevel, 0, 6));
	logger::set_level(level, level);
	// rule 14: the log names its level and how to get everything
	logger::info("HUD Position Manager {} loaded (Oblivion Remastered) - log level {}; set uLogLevel=0 in HUDPositionManager.ini to capture everything",
		HPM_VERSION, v.logLevel);
	if (auto* messaging = OBSE::GetMessagingInterface(); !messaging || !messaging->RegisterListener(&OnMessage)) {
		logger::error("OBSE messaging unavailable - no frame tick, the HUD is not changed");
	}
	return true;
}
