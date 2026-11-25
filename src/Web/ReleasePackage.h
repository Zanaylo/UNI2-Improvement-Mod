#pragma once

#include "Web/GitHubRelease.h"

#include <string>

namespace ReleasePackage
{
	enum class Variant
	{
		Standard,
		Minimal,
	};

	Variant VariantOfDescription(const std::string& description);

	const GitHubRelease::Asset* Pick(const GitHubRelease::Release& release,
		Variant variant = Variant::Standard);

	bool ReadManifestSha256(const std::string& manifest, const std::string& assetName,
		std::string& outSha, std::string& outError);

	bool Verify(const GitHubRelease::Release& release, const std::string& assetName,
		const std::string& archive, std::string& outError);

	bool MatchesChecksum(const std::string& archive, const std::string& expected,
		std::string& outError);

	bool Unpack(const std::string& archive, const std::string& stage, std::string& outError);
}
