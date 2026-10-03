#pragma once

#include "Game/Files/FbGameFolder.h"
#include "Game/Stages/Bbtag/BbtagExport.h"

#include <string>
#include <vector>

namespace StageInstall
{
	struct Target
	{
		std::string folder;
		std::string stem;
	};

	bool Supports(FbGameFolder::Game game);

	std::string GameFolder(FbGameFolder::Game game);

	bool ChooseGameFolder(FbGameFolder::Game game, const std::string& folder);

	std::vector<Target> Targets(FbGameFolder::Game game);

	std::string ModelOf(FbGameFolder::Game game, const Target& target);

	bool HasBackup(FbGameFolder::Game game, const Target& target);

	std::string Installed(FbGameFolder::Game game, const Target& target);

	bool Write(const std::string& folder, const std::string& stem, const BbtagExport::Archives& archives);

	bool Install(FbGameFolder::Game game, const Target& target, const BbtagExport::Archives& archives,
		const std::string& stageName, std::string& report);

	bool Restore(FbGameFolder::Game game, const Target& target, std::string& report);
}
