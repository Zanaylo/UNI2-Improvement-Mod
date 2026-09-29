#include "Web/ReleasePackage.h"

#include "Core/Formats/Json.h"
#include "Core/Formats/ZipArchive.h"
#include "Core/info.h"
#include "Web/Http.h"

#include <Windows.h>

#include <cstring>
#include <vector>

namespace {

constexpr const char* kManifestAsset = "manifest.json";

std::string Combine(const std::string& folder, const char* name)
{
	if (folder.empty() || folder.back() == '\\')
		return folder + name;

	return folder + "\\" + name;
}

void EmptyFolder(const std::string& folder)
{
	WIN32_FIND_DATAA found = {};
	const HANDLE search = FindFirstFileA(Combine(folder, "*").c_str(), &found);

	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (found.cFileName[0] == '.' && (found.cFileName[1] == '\0' ||
			(found.cFileName[1] == '.' && found.cFileName[2] == '\0')))
		{
			continue;
		}

		const std::string path = Combine(folder, found.cFileName);

		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
		{
			EmptyFolder(path);
			RemoveDirectoryA(path.c_str());
			continue;
		}

		DeleteFileA(path.c_str());
	}
	while (FindNextFileA(search, &found) != 0);

	FindClose(search);
}

bool IsAllowedEntry(const std::string& name)
{
	return _stricmp(name.c_str(), UNI2_IM_ENTRY_DLL) == 0 ||
		_stricmp(name.c_str(), UNI2_IM_UPDATER_EXE) == 0;
}

bool Validate(const std::string& path, std::string& outError)
{
	std::vector<std::string> names;

	if (!ZipArchive::List(path, names))
	{
		outError = "the downloaded file is not a zip";
		return false;
	}

	bool hasDll = false;

	for (const std::string& name : names)
	{
		if (name.empty() || name.back() == '/' || name.back() == '\\')
			continue;

		if (!IsAllowedEntry(name))
		{
			outError = "the release zip contains '" + name + "', which the updater will not install";
			return false;
		}

		hasDll = hasDll || _stricmp(name.c_str(), UNI2_IM_ENTRY_DLL) == 0;
	}

	if (!hasDll)
	{
		outError = "the release zip has no " UNI2_IM_ENTRY_DLL;
		return false;
	}

	return true;
}

}

const GitHubRelease::Asset* ReleasePackage::Pick(const GitHubRelease::Release& release)
{
	const std::string wanted = std::string("UNI2-Improvement-Mod-") + release.version + ".zip";

	const GitHubRelease::Asset* const asset = release.FindAsset(wanted.c_str());

	return asset != nullptr ? asset : release.FindAssetEndingWith(".zip");
}

bool ReleasePackage::ExpectedSha256(const GitHubRelease::Release& release,
	const std::string& assetName, std::string& outSha, std::string& outError)
{
	outSha.clear();
	outError.clear();

	const GitHubRelease::Asset* const manifest = release.FindAsset(kManifestAsset);

	if (manifest == nullptr)
		return false;

	std::string text;

	if (!Http::GetText(manifest->url, text, outError))
		return false;

	Json::Value root;

	if (!Json::Parse(text, root) || !root.IsObject())
		return false;

	if (_stricmp(root.MemberString("assetName").c_str(), assetName.c_str()) != 0)
		return false;

	outSha = root.MemberString("sha256");
	return outSha.size() == 64;
}

bool ReleasePackage::MatchesChecksum(const std::string& archive, const std::string& expected,
	std::string& outError)
{
	std::string actual;

	if (!Http::Sha256OfFile(archive, actual, outError))
		return false;

	if (_stricmp(actual.c_str(), expected.c_str()) == 0)
		return true;

	DeleteFileA(archive.c_str());
	outError = "the download does not match the release checksum";
	return false;
}

bool ReleasePackage::Unpack(const std::string& archive, const std::string& stage,
	std::string& outError)
{
	if (!Validate(archive, outError))
		return false;

	EmptyFolder(stage);

	int files = 0;
	char status[192] = {};

	if (!ZipArchive::Extract(archive, stage, files, status, sizeof(status)))
	{
		outError = status;
		return false;
	}

	if (GetFileAttributesA(Combine(stage, UNI2_IM_ENTRY_DLL).c_str()) != INVALID_FILE_ATTRIBUTES)
		return true;

	outError = "the package did not unpack a " UNI2_IM_ENTRY_DLL;
	return false;
}
