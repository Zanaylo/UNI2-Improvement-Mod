#include "Game/Audio/MusicRefresh.h"

#include "Game/Audio/BgmControl.h"
#include "Game/Audio/BgmLibrary.h"
#include "Game/Audio/BgmRules.h"
#include "Game/Audio/BgmThemes.h"
#include "Game/Files/ModFiles.h"
#include "Game/Audio/UserMusic.h"

void MusicRefresh::ReloadLibrary()
{
	const std::vector<std::string> before = BgmLibrary::Files();

	BgmLibrary::Load();

	const std::vector<std::string> after = BgmLibrary::Files();

	BgmRules::Remap(before, after);
	BgmControl::Remap(before, after);
}

void MusicRefresh::Reindex()
{
	ModFiles::Rescan();
	ReloadLibrary();
}

void MusicRefresh::Rescan()
{
	UserMusic::Scan();
	Reindex();
	BgmThemes::Reload();
}
