#include "Game/Files/ModProfiles.h"

#include "Core/Config/ConfigProfiles.h"
#include "Core/Config/IniStore.h"
#include "Core/Text/TextList.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Files/ModPacks.h"

#include <algorithm>

namespace {

constexpr const char* kFile = "Config\\ModProfiles.ini";
constexpr const char* kIndexSection = "Profiles";
constexpr const char* kNamesKey = "Names";
constexpr const char* kActiveKey = "Active";
constexpr const char* kOrderKey = "Order";
constexpr const char* kOffKey = "Off";
constexpr const char* kSectionPrefix = "Profile ";
constexpr char kSeparator = '|';
constexpr const char* kSeparators = "|";
constexpr int kListBytes = 32768;

std::string Path()
{
	return GetModRootPath(kFile);
}

std::string SectionOf(const std::string& name)
{
	return kSectionPrefix + name;
}

std::string Read(const std::string& section, const char* key)
{
	std::vector<char> value(kListBytes, 0);
	Ini::GetString(section.c_str(), key, "", value.data(), kListBytes, Path().c_str());

	return value.data();
}

void Write(const std::string& section, const char* key, const std::string& value)
{
	Ini::Write(section.c_str(), key, value.c_str(), Path().c_str());
}

std::vector<std::string> ListOf(const std::string& section, const char* key)
{
	return TextList::Split(Read(section, key), kSeparators);
}

bool Contains(const std::vector<std::string>& names, const std::string& name)
{
	return std::any_of(names.begin(), names.end(),
		[&name](const std::string& one) { return TextList::SameNoCase(one, name); });
}

void SetActive(const std::string& name)
{
	Write(kIndexSection, kActiveKey, name);
	Ini::Flush();
}

}

std::vector<std::string> ModProfiles::List()
{
	return ListOf(kIndexSection, kNamesKey);
}

std::string ModProfiles::Active()
{
	return Read(kIndexSection, kActiveKey);
}

bool ModProfiles::MatchesActive()
{
	const std::string active = Active();

	if (active.empty())
		return false;

	std::vector<std::string> order;
	std::vector<std::string> off;
	ModPacks::Snapshot(order, off);

	return TextList::Join(order, kSeparator) == Read(SectionOf(active), kOrderKey) &&
		TextList::Join(off, kSeparator) == Read(SectionOf(active), kOffKey);
}

bool ModProfiles::Save(const std::string& name)
{
	if (!ConfigProfiles::IsValidName(name))
		return false;

	std::vector<std::string> order;
	std::vector<std::string> off;
	ModPacks::Snapshot(order, off);

	Write(SectionOf(name), kOrderKey, TextList::Join(order, kSeparator));
	Write(SectionOf(name), kOffKey, TextList::Join(off, kSeparator));

	std::vector<std::string> names = List();

	if (!Contains(names, name))
	{
		names.push_back(name);
		Write(kIndexSection, kNamesKey, TextList::Join(names, kSeparator));
	}

	SetActive(name);
	LOG("ModProfiles: saved %s, %zu mod(s), %zu off", name.c_str(), order.size(), off.size());
	return true;
}

bool ModProfiles::Load(const std::string& name)
{
	if (!Contains(List(), name))
		return false;

	ModPacks::Apply(ListOf(SectionOf(name), kOrderKey), ListOf(SectionOf(name), kOffKey));
	SetActive(name);

	LOG("ModProfiles: loaded %s", name.c_str());
	return true;
}

bool ModProfiles::Remove(const std::string& name)
{
	std::vector<std::string> names = List();
	const auto found = std::find_if(names.begin(), names.end(),
		[&name](const std::string& one) { return TextList::SameNoCase(one, name); });

	if (found == names.end())
		return false;

	names.erase(found);
	Write(kIndexSection, kNamesKey, TextList::Join(names, kSeparator));
	Write(SectionOf(name), kOrderKey, std::string());
	Write(SectionOf(name), kOffKey, std::string());

	if (TextList::SameNoCase(Active(), name))
		SetActive(std::string());

	Ini::Flush();
	LOG("ModProfiles: removed %s", name.c_str());
	return true;
}
