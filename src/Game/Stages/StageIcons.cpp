#include "Game/Stages/StageIcons.h"

#include "Core/Config/Settings.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {

constexpr const char* kRoot = "Assets\\StageIcons\\";
constexpr const char* kSuffix = " Stage icons";
constexpr const char* kCard = "\\thumbnail.png";
constexpr const char* kResourceName = "\"stage-icons\"";
constexpr const char* kResourceType = "STAGEICONS";
constexpr char kMagic[4] = { 'S', 'I', 'C', 'N' };
constexpr uint32_t kVersion = 1;
constexpr size_t kHeader = 16;
constexpr const char* kSection = "StageIcons";
constexpr const char* kStampKey = "Bundle";

struct Card
{
	const uint8_t* data;
	size_t size;
};

struct Pack
{
	uint32_t stamp = 0;
	std::map<std::string, Card> cards;
};

const char* KeyOf(FbGameFolder::Game game)
{
	switch (game)
	{
	case FbGameFolder::Game_DFCI: return "DFCI";
	case FbGameFolder::Game_MBTL: return "MBTL";
	case FbGameFolder::Game_BBTAG: return "BBTAG";
	case FbGameFolder::Game_BBCF: return "BBCF";
	case FbGameFolder::Game_UNI: return "UNICLR";
	case FbGameFolder::Game_UNIEL: return "UNIEL";
	default: return nullptr;
	}
}

std::string Folded(const std::string& text)
{
	std::string out;

	for (const char c : text)
	{
		if (isalnum(static_cast<unsigned char>(c)) != 0)
			out += static_cast<char>(tolower(static_cast<unsigned char>(c)));
	}

	return out;
}

bool Written(const std::string& path, FILETIME& out)
{
	WIN32_FILE_ATTRIBUTE_DATA data = {};

	if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data))
		return false;

	out = data.ftLastWriteTime;
	return true;
}

uint32_t Dword(const uint8_t* at)
{
	uint32_t value = 0;
	memcpy(&value, at, sizeof(value));
	return value;
}

bool Resource(const uint8_t*& data, size_t& size)
{
	const HMODULE module = GetModModuleHandle();
	const HRSRC found = module == nullptr ? nullptr : FindResourceA(module, kResourceName, kResourceType);
	const HGLOBAL loaded = found == nullptr ? nullptr : LoadResource(module, found);

	if (loaded == nullptr)
		return false;

	data = static_cast<const uint8_t*>(LockResource(loaded));
	size = SizeofResource(module, found);

	return data != nullptr && size >= kHeader;
}

Pack Read()
{
	Pack pack;
	const uint8_t* data = nullptr;
	size_t size = 0;

	if (!Resource(data, size) || memcmp(data, kMagic, sizeof(kMagic)) != 0 || Dword(data + 4) != kVersion)
	{
		LOG("StageIcons: the mod carries no stage card pack");
		return pack;
	}

	const uint32_t count = Dword(data + 12);
	size_t at = kHeader;

	for (uint32_t i = 0; i < count; ++i)
	{
		if (at + 1 > size || at + 1 + data[at] + 8 > size)
			break;

		const size_t length = data[at];
		const std::string key(reinterpret_cast<const char*>(data + at + 1), length);
		const uint32_t offset = Dword(data + at + 1 + length);
		const uint32_t bytes = Dword(data + at + 1 + length + 4);

		at += 1 + length + 8;

		if (offset > size || bytes > size - offset)
			continue;

		pack.cards[key] = Card{ data + offset, bytes };
	}

	pack.stamp = Dword(data + 8);
	LOG("StageIcons: the mod carries %u stage card(s), stamp %08x", static_cast<unsigned>(pack.cards.size()),
		pack.stamp);

	return pack;
}

const Pack& Bundle()
{
	static const Pack pack = Read();
	return pack;
}

std::string Lowered(const std::string& text)
{
	std::string out = text;

	for (char& c : out)
		c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

	return out;
}

uint32_t StoredStamp()
{
	char stored[16] = {};

	GetPrivateProfileStringA(kSection, kStampKey, "", stored, sizeof(stored), Settings::GetIniPath().c_str());

	return static_cast<uint32_t>(strtoul(stored, nullptr, 16));
}

}

std::string StageIcons::FolderFor(FbGameFolder::Game game, const std::string& name,
	const std::string& source)
{
	const char* const key = KeyOf(game);

	if (key == nullptr || name.empty())
		return std::string();

	const std::string root = GetModRootPath(kRoot) + key + kSuffix + "\\";
	const std::string exact = Folded(name + source);
	const std::string plain = Folded(name);

	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA((root + "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return std::string();

	std::string match;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || found.cFileName[0] == '.')
			continue;

		const std::string folded = Folded(found.cFileName);

		if (folded == exact)
		{
			match = root + found.cFileName;
			break;
		}

		if (folded == plain && match.empty())
			match = root + found.cFileName;
	} while (FindNextFileA(search, &found));

	FindClose(search);

	FILETIME written = {};

	return match.empty() || !Written(match + kCard, written) ? std::string() : match;
}

bool StageIcons::Newer(const std::string& folder, const std::string& than)
{
	FILETIME icon = {};

	if (folder.empty() || !Written(folder + kCard, icon))
		return false;

	FILETIME card = {};

	return !Written(than, card) || CompareFileTime(&icon, &card) > 0;
}

bool StageIcons::Bundled(FbGameFolder::Game game, const std::string& source, const uint8_t*& data,
	size_t& size)
{
	const char* const key = KeyOf(game);

	if (key == nullptr || source.empty())
		return false;

	const std::map<std::string, Card>& cards = Bundle().cards;
	const std::map<std::string, Card>::const_iterator found =
		cards.find(Lowered(std::string(key) + "/" + source));

	if (found == cards.end())
		return false;

	data = found->second.data;
	size = found->second.size;
	return true;
}

bool StageIcons::BundleChanged()
{
	const uint32_t stamp = Bundle().stamp;

	return stamp != 0 && stamp != StoredStamp();
}

void StageIcons::RememberBundle()
{
	char value[16] = {};
	sprintf_s(value, "%08x", Bundle().stamp);

	Settings::SaveString(kSection, kStampKey, value);
}
