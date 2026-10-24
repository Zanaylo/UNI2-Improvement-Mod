#include "Core/Config/ConfigBoot.h"

#include "Core/Config/ConfigFiles.h"
#include "Core/Config/ConfigLayout.h"
#include "Core/Config/IniStore.h"
#include "Core/Config/Settings.h"
#include "Core/Config/default_ini.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>

#include <string>

namespace {

constexpr char kLineEnd[] = "\r\n";

ConfigFiles::Result g_result = { ConfigFiles::Outcome::Ready, 0, 0, std::string(), {} };
INIT_ONCE g_prepared = INIT_ONCE_STATIC_INIT;

std::string ShippedDefaults()
{
	std::string text;

	for (const char* line : kDefaultIniLines)
		text.append(line).append(kLineEnd);

	return text;
}

void LogNotes()
{
	for (const std::string& note : g_result.notes)
		LOG("Settings: %s", note.c_str());
}

BOOL CALLBACK PrepareOnce(PINIT_ONCE, PVOID, PVOID*)
{
	const std::string root = GetModRootPath();
	const std::string legacy = Settings::GetIniPath();

	g_result = ConfigFiles::PrepareIn(root, legacy, ShippedDefaults());

	if (g_result.kind != ConfigFiles::Outcome::Failed)
		Ini::Route(legacy, root, &ConfigLayout::FileFor);

	return TRUE;
}

}

void ConfigBoot::Prepare()
{
	InitOnceExecuteOnce(&g_prepared, &PrepareOnce, nullptr, nullptr);
}

void ConfigBoot::LogSummary()
{
	switch (g_result.kind)
	{
	case ConfigFiles::Outcome::Ready:
		return;

	case ConfigFiles::Outcome::Fresh:
		LOG("Settings: first start, the defaults were written to %d file(s) in Config and Data", g_result.files);
		return;

	case ConfigFiles::Outcome::Migrated:
		LOG("Settings: UNI2_IM.ini (revision %d) was split into %d file(s) in Config and Data and kept as %s",
			g_result.fromRevision, g_result.files, g_result.backup.c_str());
		LogNotes();
		return;

	case ConfigFiles::Outcome::Merged:
		LOG("Settings: a UNI2_IM.ini put back by hand was merged into the Config and Data files and kept as %s",
			g_result.backup.c_str());
		LogNotes();
		return;

	case ConfigFiles::Outcome::Failed:
		LOG("Settings: the Config and Data files could not be written, UNI2_IM.ini is still used as it is");
		return;
	}
}
