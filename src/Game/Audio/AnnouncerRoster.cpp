#include "Game/Audio/AnnouncerRoster.h"

#include "Core/Config/Settings.h"
#include "Game/Customize/AnnouncerList.h"
#include "Game/Files/DataArchive.h"

#include <Windows.h>

#include <algorithm>
#include <mutex>
#include <sstream>

namespace {

constexpr const char* kSection = "Sounds";
constexpr const char* kKey = "MbtlAnnouncers";
constexpr const char* kCustomize = "grpdat\\Customize";
constexpr const char* kAnnouncerList = "AnnounceCharaList.csv";
constexpr int kHighestNumber = 127;
constexpr int kUnknownCapacity = -1;
constexpr const char* kNone = "-";

constexpr AnnouncerRoster::Speaker kSpeakers[] = {
	{ "system", "Announcer" },
	{ "chr000", "Arcueid" },
	{ "chr001", "Hisui" },
	{ "chr002", "Akiha" },
	{ "chr003", "Shiki" },
	{ "chr004", "Kohaku" },
	{ "chr005", "Roa" },
	{ "chr006", "Kouma" },
	{ "chr008", "Noel" },
	{ "chr009", "Vlov" },
	{ "chr010", "Red Arcueid" },
	{ "chr011", "Ciel" },
	{ "chr012", "Saber" },
	{ "chr013", "Miyako" },
	{ "chr014", "Dead Apostle Noel" },
	{ "chr015", "Aoko" },
	{ "chr016", "Powered Ciel" },
	{ "chr017", "Mario" },
	{ "chr019", "Neco-Arc" },
	{ "chr020", "Mash" },
	{ "chr021", "Ushiwakamaru" },
	{ "chr022", "Edmond Dantes" },
};

constexpr int kSpeakerCount = static_cast<int>(sizeof(kSpeakers) / sizeof(kSpeakers[0]));

std::mutex g_lock;
std::vector<std::string> g_chosen;
bool g_loaded = false;
int g_capacity = kUnknownCapacity;

std::vector<std::string> Split(const std::string& text)
{
	std::vector<std::string> out;
	std::istringstream stream(text);
	std::string item;

	while (std::getline(stream, item, ','))
	{
		if (!item.empty() && item != kNone)
			out.push_back(item);
	}

	return out;
}

std::string Joined(const std::vector<std::string>& items)
{
	std::string out;

	for (const std::string& item : items)
		out += (out.empty() ? "" : ",") + item;

	return out;
}

int ComputeCapacity()
{
	std::vector<uint8_t> csv;

	if (!DataArchive::Read(kCustomize, kAnnouncerList, csv))
		return 0;

	return AnnouncerList::Capacity(std::string(csv.begin(), csv.end()));
}

std::vector<std::string> Defaults(int capacity)
{
	std::vector<std::string> out;

	for (int i = 0; i < kSpeakerCount && static_cast<int>(out.size()) < capacity; ++i)
		out.push_back(kSpeakers[i].folder);

	return out;
}

void Load()
{
	if (g_loaded)
		return;

	g_loaded = true;
	g_capacity = ComputeCapacity();

	char buffer[1024] = {};
	GetPrivateProfileStringA(kSection, kKey, "", buffer, sizeof(buffer), Settings::GetIniPath().c_str());

	g_chosen = buffer[0] != '\0' ? Split(buffer) : Defaults(g_capacity);
}

void Save()
{
	Settings::SaveString(kSection, kKey, g_chosen.empty() ? kNone : Joined(g_chosen).c_str());
}

std::vector<std::string> InRosterOrder(const std::vector<std::string>& chosen)
{
	std::vector<std::string> out;

	for (const AnnouncerRoster::Speaker& speaker : kSpeakers)
	{
		if (std::find(chosen.begin(), chosen.end(), speaker.folder) != chosen.end())
			out.push_back(speaker.folder);
	}

	return out;
}

}

int AnnouncerRoster::Count()
{
	return kSpeakerCount;
}

const AnnouncerRoster::Speaker& AnnouncerRoster::At(int index)
{
	return kSpeakers[std::clamp(index, 0, kSpeakerCount - 1)];
}

int AnnouncerRoster::NumberOf(const std::string& folder)
{
	for (int i = 0; i < kSpeakerCount; ++i)
	{
		if (folder == kSpeakers[i].folder)
			return kHighestNumber - i;
	}

	return -1;
}

int AnnouncerRoster::Capacity()
{
	std::lock_guard<std::mutex> guard(g_lock);
	Load();
	return g_capacity;
}

bool AnnouncerRoster::IsChosen(const std::string& folder)
{
	std::lock_guard<std::mutex> guard(g_lock);
	Load();
	return std::find(g_chosen.begin(), g_chosen.end(), folder) != g_chosen.end();
}

void AnnouncerRoster::SetChosen(const std::string& folder, bool chosen)
{
	std::lock_guard<std::mutex> guard(g_lock);
	Load();

	g_chosen.erase(std::remove(g_chosen.begin(), g_chosen.end(), folder), g_chosen.end());

	if (chosen)
		g_chosen.push_back(folder);

	g_chosen = InRosterOrder(g_chosen);
	Save();
}

int AnnouncerRoster::ChosenCount()
{
	std::lock_guard<std::mutex> guard(g_lock);
	Load();
	return static_cast<int>(g_chosen.size());
}

std::vector<std::string> AnnouncerRoster::Chosen()
{
	std::lock_guard<std::mutex> guard(g_lock);
	Load();

	std::vector<std::string> out = InRosterOrder(g_chosen);

	if (static_cast<int>(out.size()) > g_capacity)
		out.resize(static_cast<size_t>((std::max)(0, g_capacity)));

	return out;
}
