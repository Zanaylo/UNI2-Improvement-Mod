#pragma once

#include "Web/GitHubRelease.h"

#include <string>

namespace ReleasePackage
{
	const GitHubRelease::Asset* Pick(const GitHubRelease::Release& release);

	bool ExpectedSha256(const GitHubRelease::Release& release, const std::string& assetName,
		std::string& outSha, std::string& outError);

	bool MatchesChecksum(const std::string& archive, const std::string& expected,
		std::string& outError);

	bool Unpack(const std::string& archive, const std::string& stage, std::string& outError);
}
