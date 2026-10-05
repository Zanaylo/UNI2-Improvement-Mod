#include "Game/Customize/PortraitChoices.h"

#include "Core/Config/Settings.h"
#include "Game/Customize/PortraitCatalog.h"
#include "Game/Customize/PortraitPicks.h"

#include <Windows.h>

#include <map>
#include <mutex>

namespace {

constexpr const char* kSection = "Portraits";
constexpr size_t kIdBytes = 160;

std::mutex g_lock;
std::map<int, std::string> g_chosen;

std::string ReadLocked(int chara)
{
	const auto known = g_chosen.find(chara);

	if (known != g_chosen.end())
		return known->second;

	char value[kIdBytes] = {};
	GetPrivateProfileStringA(kSection, PortraitPicks::KeyOf(chara).c_str(), "", value, sizeof(value),
		Settings::GetIniPath().c_str());

	const PortraitCatalog::Art* const art = PortraitCatalog::Find(value);
	const std::string id = art != nullptr ? art->id : std::string();
	g_chosen[chara] = id;
	return id;
}

}

std::string PortraitChoices::Of(int chara)
{
	std::lock_guard<std::mutex> hold(g_lock);
	return ReadLocked(chara);
}

void PortraitChoices::Choose(int chara, const std::string& id)
{
	std::lock_guard<std::mutex> hold(g_lock);
	g_chosen[chara] = id;
	Settings::SaveString(kSection, PortraitPicks::KeyOf(chara).c_str(), id.c_str());
}
