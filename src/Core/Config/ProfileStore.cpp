#include "Core/Config/ProfileStore.h"

#include "Core/Config/ConfigProfiles.h"
#include "Core/Config/IniStore.h"
#include "Core/logger.h"
#include "Core/utils.h"

namespace {

bool g_loaded = false;

}

std::vector<std::string> ProfileStore::List()
{
	return ConfigProfiles::ListIn(GetModRootPath());
}

bool ProfileStore::Save(const std::string& name)
{
	Ini::Flush();

	const bool saved = ConfigProfiles::SaveIn(GetModRootPath(), name);
	LOG("Profiles: '%s' %s", name.c_str(), saved ? "saved" : "could not be saved");
	return saved;
}

bool ProfileStore::Load(const std::string& name)
{
	Ini::Flush();

	const bool loaded = ConfigProfiles::LoadIn(GetModRootPath(), name);
	Ini::Forget();

	g_loaded = g_loaded || loaded;
	LOG("Profiles: '%s' %s", name.c_str(), loaded ? "loaded, it applies at the next start" : "could not be loaded");
	return loaded;
}

bool ProfileStore::Delete(const std::string& name)
{
	const bool deleted = ConfigProfiles::DeleteIn(GetModRootPath(), name);
	LOG("Profiles: '%s' %s", name.c_str(), deleted ? "deleted" : "could not be deleted");
	return deleted;
}

bool ProfileStore::NeedsRestart()
{
	return g_loaded;
}
