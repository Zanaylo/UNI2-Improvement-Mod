#include "Game/Files/SteamLibrary.h"

#include "Core/utils.h"

#include <Windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

constexpr const char* kSteamKey = "Software\\Valve\\Steam";
constexpr const char* kSteamPathValue = "SteamPath";
constexpr const char* kLibraryFile = "\\steamapps\\libraryfolders.vdf";
constexpr const char* kCommonFolder = "\\steamapps\\common\\";
constexpr const char* kPathKey = "\"path\"";

std::string SteamPath()
{
	char buffer[MAX_PATH] = {};
	DWORD size = sizeof(buffer);

	if (RegGetValueA(HKEY_CURRENT_USER, kSteamKey, kSteamPathValue, RRF_RT_REG_SZ, nullptr, buffer, &size)
		!= ERROR_SUCCESS)
		return std::string();

	std::string out = buffer;
	std::replace(out.begin(), out.end(), '/', '\\');

	return out;
}

std::string Unescaped(const std::string& text)
{
	std::string out;

	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == '\\' && i + 1 < text.size())
			++i;

		out.push_back(text[i]);
	}

	return out;
}

std::string QuotedAfter(const std::string& text, size_t from)
{
	const size_t open = text.find('"', from);

	if (open == std::string::npos)
		return std::string();

	size_t close = open + 1;

	while (close < text.size() && text[close] != '"')
		close += text[close] == '\\' ? 2 : 1;

	return close < text.size() ? Unescaped(text.substr(open + 1, close - open - 1)) : std::string();
}

bool IsFolder(const std::string& path)
{
	const DWORD attributes = GetFileAttributesA(path.c_str());

	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void AddUnique(std::vector<std::string>& folders, const std::string& folder)
{
	if (folder.empty())
		return;

	const bool known = std::any_of(folders.begin(), folders.end(),
		[&folder](const std::string& one) { return _stricmp(one.c_str(), folder.c_str()) == 0; });

	if (!known)
		folders.push_back(folder);
}

}

std::vector<std::string> SteamLibrary::Folders()
{
	std::vector<std::string> out;
	const std::string steam = SteamPath();

	if (steam.empty())
		return out;

	AddUnique(out, steam);

	std::vector<uint8_t> raw;

	if (!ReadWholeFile(steam + kLibraryFile, raw))
		return out;

	const std::string text(raw.begin(), raw.end());

	for (size_t at = text.find(kPathKey); at != std::string::npos; at = text.find(kPathKey, at + 1))
		AddUnique(out, QuotedAfter(text, at + strlen(kPathKey)));

	return out;
}

std::string SteamLibrary::GameFolder(const char* installFolder)
{
	for (const std::string& library : Folders())
	{
		const std::string candidate = library + kCommonFolder + installFolder;

		if (IsFolder(candidate))
			return candidate;
	}

	return std::string();
}
