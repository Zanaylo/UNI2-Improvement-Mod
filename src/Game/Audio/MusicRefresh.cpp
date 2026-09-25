#include "Game/Audio/MusicRefresh.h"

#include "Game/Audio/BgmLibrary.h"
#include "Game/Audio/BgmThemes.h"
#include "Game/Files/ModFiles.h"
#include "Game/Audio/UserMusic.h"

void MusicRefresh::Reindex()
{
	ModFiles::Rescan();
	BgmLibrary::Load();
}

void MusicRefresh::Rescan()
{
	UserMusic::Scan();
	Reindex();
	BgmThemes::Reload();
}
