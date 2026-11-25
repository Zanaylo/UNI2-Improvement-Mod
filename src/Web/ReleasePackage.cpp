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
constexpr const char* kMinimalSuffix = "-minimal.zip";
constexpr size_t kSha256HexLength = 64;

bool EndsWithNoCase(const std::string& text, const char* suffix)
{
	const size_t length = strlen(suffix);

	return text.size() >= length &&
		_stricmp(text.c_str() + text.size() - length, suffix) == 0;
}

const GitHubRelease::Asset* PickStandard(const GitHubRelease::Release& release)
{
	const std::string wanted = std::string("UNI2-Improvement-Mod-") + release.version + ".zip";

	if (const GitHubRelease::Asset* const exact = release.FindAsset(wanted.c_str()))
		return exact;

	for (const GitHubRelease::Asset& asset : release.assets)
	{
		if (EndsWithNoCase(asset.name, ".zip") && !EndsWithNoCase(asset.name, kMinimalSuffix))
			return &asset;
	}

	return release.FindAssetEndingWith(".zip");
}

bool ExtractSha(const Json::Value& entry, const std::string& assetName, std::string& outSha,
	std::string& outError)
{
	const std::string sha = entry.MemberString("sha256");

	if (sha.size() != kSha256HexLength)
	{
		outError = "the release's manifest.json carries no SHA-256 for " + assetName;
		return false;
	}

	outSha = sha;
	return true;
}

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
		_stricmp(name.c_str(), UNI2_IM_UPDATER_EXE) == 0 ||
		_stricmp(name.c_str(), UNI2_IM_MODLOADER_EXE) == 0;
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

ReleasePackage::Variant ReleasePackage::VariantOfDescription(const std::string& description)
{
	if (_stricmp(description.c_str(), UNI2_IM_NAME UNI2_IM_MINIMAL_TAG) == 0)
		return Variant::Minimal;

	return Variant::Standard;
}

const GitHubRelease::Asset* ReleasePackage::Pick(const GitHubRelease::Release& release, Variant variant)
{
	if (variant == Variant::Minimal)
	{
		const std::string wanted = std::string("UNI2-Improvement-Mod-") + release.version + kMinimalSuffix;

		if (const GitHubRelease::Asset* const exact = release.FindAsset(wanted.c_str()))
			return exact;

		if (const GitHubRelease::Asset* const minimal = release.FindAssetEndingWith(kMinimalSuffix))
			return minimal;
	}

	return PickStandard(release);
}

bool ReleasePackage::ReadManifestSha256(const std::string& manifest, const std::string& assetName,
	std::string& outSha, std::string& outError)
{
	outSha.clear();

	Json::Value root;

	if (!Json::Parse(manifest, root) || !root.IsObject())
	{
		outError = "the release's manifest.json could not be read";
		return false;
	}

	if (_stricmp(root.MemberString("assetName").c_str(), assetName.c_str()) == 0)
		return ExtractSha(root, assetName, outSha, outError);

	const Json::Value* const variants = root.Find("variants");

	if (variants != nullptr && variants->IsArray())
	{
		for (size_t i = 0; i < variants->Count(); ++i)
		{
			const Json::Value* const entry = variants->At(i);

			if (entry == nullptr || !entry->IsObject())
				continue;

			if (_stricmp(entry->MemberString("assetName").c_str(), assetName.c_str()) == 0)
				return ExtractSha(*entry, assetName, outSha, outError);
		}
	}

	outError = "the release's manifest.json describes another zip than " + assetName;
	return false;
}

bool ReleasePackage::Verify(const GitHubRelease::Release& release, const std::string& assetName,
	const std::string& archive, std::string& outError)
{
	const GitHubRelease::Asset* const manifest = release.FindAsset(kManifestAsset);

	if (manifest == nullptr)
	{
		outError = "the release has no manifest.json, so the download cannot be checked";
		return false;
	}

	std::string text;

	if (!Http::GetText(manifest->url, text, outError))
	{
		outError = "the release's manifest.json could not be downloaded: " + outError;
		return false;
	}

	std::string expected;

	if (!ReadManifestSha256(text, assetName, expected, outError))
		return false;

	return MatchesChecksum(archive, expected, outError);
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
