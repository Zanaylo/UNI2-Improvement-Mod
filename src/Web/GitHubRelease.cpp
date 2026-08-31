#include "Web/GitHubRelease.h"

#include "Core/Json.h"
#include "Core/info.h"
#include "Web/Http.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr int kParts = 4;

std::string StripTagPrefix(const std::string& tag)
{
	if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V'))
		return tag.substr(1);

	return tag;
}

void ReadParts(const std::string& version, int* out)
{
	for (int i = 0; i < kParts; ++i)
		out[i] = 0;

	sscanf_s(version.c_str(), "%d.%d.%d.%d", &out[0], &out[1], &out[2], &out[3]);
}

bool EndsWith(const std::string& text, const char* suffix)
{
	const size_t length = strlen(suffix);

	if (text.size() < length)
		return false;

	return _stricmp(text.c_str() + text.size() - length, suffix) == 0;
}

}

const GitHubRelease::Asset* GitHubRelease::Release::FindAsset(const char* name) const
{
	if (name == nullptr)
		return nullptr;

	for (const Asset& asset : assets)
	{
		if (_stricmp(asset.name.c_str(), name) == 0)
			return &asset;
	}

	return nullptr;
}

const GitHubRelease::Asset* GitHubRelease::Release::FindAssetEndingWith(const char* suffix) const
{
	if (suffix == nullptr)
		return nullptr;

	for (const Asset& asset : assets)
	{
		if (EndsWith(asset.name, suffix))
			return &asset;
	}

	return nullptr;
}

int GitHubRelease::Compare(const std::string& left, const std::string& right)
{
	int theirs[kParts] = {};
	int ours[kParts] = {};

	ReadParts(left, theirs);
	ReadParts(right, ours);

	for (int i = 0; i < kParts; ++i)
	{
		if (theirs[i] != ours[i])
			return theirs[i] > ours[i] ? 1 : -1;
	}

	return 0;
}

bool GitHubRelease::IsNewerThanRunning(const std::string& version)
{
	return Compare(version, UNI2_IM_VERSION) > 0;
}

bool GitHubRelease::ParseLatest(const std::string& json, Release& out, std::string& outError)
{
	out = Release();
	outError.clear();

	Json::Value root;

	if (!Json::Parse(json, root) || !root.IsObject())
	{
		outError = "the answer was not the release JSON";
		return false;
	}

	out.tag = root.MemberString("tag_name");

	if (out.tag.empty())
	{
		outError = "the answer carries no tag";
		return false;
	}

	out.version = StripTagPrefix(out.tag);
	out.title = root.MemberString("name");
	out.notes = root.MemberString("body");
	out.page = root.MemberString("html_url", UNI2_IM_RELEASE_PAGE);
	out.published = root.MemberString("published_at");
	out.draft = root.MemberBool("draft");
	out.prerelease = root.MemberBool("prerelease");

	const Json::Value* const assets = root.Find("assets");

	if (assets == nullptr || !assets->IsArray())
		return true;

	for (size_t i = 0; i < assets->Count(); ++i)
	{
		const Json::Value* const entry = assets->At(i);

		if (entry == nullptr || !entry->IsObject())
			continue;

		Asset asset;
		asset.name = entry->MemberString("name");
		asset.url = entry->MemberString("browser_download_url");
		asset.size = entry->MemberUnsigned("size");

		if (asset.name.empty() || asset.url.empty())
			continue;

		out.assets.push_back(asset);
	}

	return true;
}

bool GitHubRelease::FetchLatest(Release& out, std::string& outError)
{
	std::string json;

	if (!Http::GetText(UNI2_IM_RELEASE_API, json, outError))
		return false;

	return ParseLatest(json, out, outError);
}
