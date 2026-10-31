#include "Core/Config/ConfigMigrations.h"

namespace {

using ConfigMigrations::Kind;
using ConfigMigrations::Step;

constexpr Step kSteps[] = {
	{ 2, Kind::Set, "Netplay", "Diagnostics", "0", nullptr, nullptr,
		"turned off, it reaches into the netcode and is a diagnostic, not a feature" },
	{ 3, Kind::Set, "Netplay", "RoomRosterFix", "0", nullptr, nullptr,
		"turned off, it writes into the game's room state" },
	{ 3, Kind::Set, "Netplay", "RepublishPingLocation", "0", nullptr, nullptr,
		"turned off, it writes into the game's room state" },
	{ 3, Kind::Remove, "Netplay", "SafeOnline", nullptr, nullptr, nullptr, "retired" },
	{ 3, Kind::Remove, "Netplay", "Diagnostics", nullptr, nullptr, nullptr, "retired" },
	{ 4, Kind::Set, "Netplay", "NetLog", "0", nullptr, nullptr, "off unless turned on for a report" },
	{ 4, Kind::Set, "Netplay", "CaptureGgpoLog", "0", nullptr, nullptr, "off unless turned on for a report" },
	{ 5, Kind::SetIfEmpty, "Keybinds", "RestartGame", "Ctrl+F5", nullptr, nullptr,
		"Ctrl+F5 restarts the game, as in MBTL IM" },
	{ 6, Kind::Remove, "Graphics", "UltrawideFov", nullptr, nullptr, nullptr,
		"retired, the ultrawide widen left the mod" },
	{ 6, Kind::Remove, "Graphics", "UltrawideWidth", nullptr, nullptr, nullptr,
		"retired, the ultrawide widen left the mod" },
	{ 6, Kind::Remove, "Graphics", "UltrawideHeight", nullptr, nullptr, nullptr,
		"retired, the ultrawide widen left the mod" },
	{ 6, Kind::Remove, "Subtitles", "DrawWithTheGame", nullptr, nullptr, nullptr,
		"retired, subtitles are always drawn the game's way" },
};

constexpr int Newest()
{
	int newest = 0;

	for (const Step& step : kSteps)
		newest = step.revision > newest ? step.revision : newest;

	return newest;
}

static_assert(Newest() == ConfigMigrations::kCurrent, "kCurrent is the newest revision in the table");

}

const Step* ConfigMigrations::Steps(size_t& outCount)
{
	outCount = sizeof(kSteps) / sizeof(kSteps[0]);
	return kSteps;
}
