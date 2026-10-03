#pragma once

#include "Game/Files/FbGameFolder.h"
#include "Game/Stages/StageInstall.h"

#include <string>

namespace StageExport
{
	bool Start(int id);

	bool Install(int id, FbGameFolder::Game game, const StageInstall::Target& target);

	bool IsBusy();

	std::string StatusText();

	std::string Folder();
}
